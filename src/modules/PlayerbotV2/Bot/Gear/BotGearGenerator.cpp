#include "BotGearGenerator.h"
#include "BotItemScorer.h"
#include "OwnedGearPolicy.h"
#include "BisPools.h"
#include "WorldGearStart.h"
#include "Bot/ClassTables.h"   // MaxPlayerLevel()
#include "ObjectMgr.h"
#include "DB2Stores.h"           // sItemSetStore: tier set pieces
#include "ItemTemplate.h"
#include "Player.h"
#include "PlayerbotAPI.h"  // IsItemRenderableInSlot — the renderability gate
#include "Log.h"
#include "World.h"
#include "Config.h"
#include "DatabaseEnv.h"         // the holiday loot conditions (Bots.HolidayClothes)
#include "GameEventMgr.h"        // IsHolidayActive: holiday items only in their season
#include "../BotSnapshot.h"          // PlanEquipOrder
#include "../../Services.h"          // Services::Config(): Playerbot.Behavior.TankRangedWeapon
#include "../../Fleet/OwnerRegistry.h"
#include "../../Fleet/BotIdentityRegistry.h"
#include "../../Util/ConfigReader.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <limits>
#include <mutex>
#include <cstring>
#include <string_view>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
// The highest effective item level raid gear may reach: Playerbot.Gear.RaidIlvlCap when set (> 0), else the cap of
// Playerbot.Gear.MaxTier (default 2 = Blackwing Lair). Approximate Classic item level ceilings per raid tier
// (not derived from the item store, which was not reachable): pre-raid dungeon gear ~58-63, MC / Onyxia up to ~76,
// Blackwing Lair non-set drops up to ~83, AQ40 81-88, Naxxramas 86-92. Monotone in the tier. Generation and
// RaidEquipUpgrade both compare it with EffectiveItemLevelForLevel(item, wearer level).
int32 CapForTier(float tier)
{
    if (tier < 1.0f) return 63;     // 0: pre-raid / dungeon gear only
    if (tier < 2.0f) return 76;     // 1: Molten Core, Onyxia
    if (tier < 2.5f) return 83;     // 2: Blackwing Lair
    if (tier < 3.0f) return 88;     // 2.5: Ahn'Qiraj 40
    return 92;                      // 3: Naxxramas
}

int32 RaidIlvlCap()
{
    static int32 const cap = []
    {
        int32 const explicit_cap = sConfigMgr->GetIntDefault("Playerbot.Gear.RaidIlvlCap", 0, true);
        if (explicit_cap > 0)
            return explicit_cap;
        return CapForTier(sConfigMgr->GetFloatDefault("Playerbot.Gear.MaxTier", 2.0f, true));
    }();
    return cap;
}

// World bots: Playerbot.World.Gear.MaxTier (default 1, item level 76).
int32 WorldIlvlCap()
{
    static int32 const cap = CapForTier(sConfigMgr->GetFloatDefault("Playerbot.World.Gear.MaxTier", 1.0f, true));
    return cap;
}

int32 OwnedIlvlBandCfg()
{
    static int32 const band = std::clamp(sConfigMgr->GetIntDefault("Playerbot.Gear.Owned.IlvlBand",
        Playerbot::V2::Gear::Owned::kDefaultIlvlBand, true), 0, 40);
    return band;
}

// The share of a world bot's slots that may hold an epic (Playerbot.World.Gear.EpicPercent, default 15), decided per bot and slot.
bool WorldEpicAllowed(uint64 bot_id, uint8 slot)
{
    static uint32 const pct = uint32(std::clamp(sConfigMgr->GetIntDefault("Playerbot.World.Gear.EpicPercent", 15, true), 0, 100));
    uint64 h = bot_id * 0x9E3779B97F4A7C15ull ^ (uint64(slot) + 7) * 0xD6E8FEB86659FD93ull;
    h ^= h >> 31;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 29;
    return (h % 100) < pct;
}

// About one world bot in seven wears a PvP rank set on its six armour slots (Playerbot.World.Gear.PvpPercent, default 14).
bool WorldPvpSetBot(uint64 bot_id)
{
    static uint32 const pct = uint32(std::clamp(sConfigMgr->GetIntDefault("Playerbot.World.Gear.PvpPercent", 14, true), 0, 100));
    uint64 h = bot_id * 0xD6E8FEB86659FD93ull ^ 0x51ED270B9F8A3C25ull;
    h ^= h >> 31;
    h *= 0x94D049BB133111EBull;
    h ^= h >> 29;
    return (h % 100) < pct;
}

bool PvpSetSlot(uint8 slot)
{
    return slot == EQUIPMENT_SLOT_HEAD || slot == EQUIPMENT_SLOT_SHOULDERS || slot == EQUIPMENT_SLOT_CHEST ||
           slot == EQUIPMENT_SLOT_LEGS || slot == EQUIPMENT_SLOT_HANDS || slot == EQUIPMENT_SLOT_FEET;
}
}

namespace Playerbot::V2::Gear {

namespace {

// Equipment slot indices we generate for. Bag slots (19-22) come from the
// starter kit, not from gear generation. Tabard/Body/Shirt are cosmetic
// and skipped.
constexpr std::array<uint8, 17> kSlots = {
    EQUIPMENT_SLOT_HEAD,
    EQUIPMENT_SLOT_NECK,
    EQUIPMENT_SLOT_SHOULDERS,
    EQUIPMENT_SLOT_CHEST,
    EQUIPMENT_SLOT_WAIST,
    EQUIPMENT_SLOT_LEGS,
    EQUIPMENT_SLOT_FEET,
    EQUIPMENT_SLOT_WRISTS,
    EQUIPMENT_SLOT_HANDS,
    EQUIPMENT_SLOT_FINGER1,
    EQUIPMENT_SLOT_FINGER2,
    EQUIPMENT_SLOT_TRINKET1,
    EQUIPMENT_SLOT_TRINKET2,
    EQUIPMENT_SLOT_BACK,
    EQUIPMENT_SLOT_MAINHAND,
    EQUIPMENT_SLOT_OFFHAND,
    EQUIPMENT_SLOT_RANGED,      // Classic 1.60: bows, guns, crossbows, thrown and wands have their own slot (Player::FindEquipSlot)
};

// The slots resistance gear goes in (GearGenerationContext::resist_school): armour and jewellery. Weapons, trinkets and the ranged slot
// keep their role picks, as a raider dressing for Huhuran kept its weapons.
constexpr bool ResistGearSlot(uint8 slot)
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD: case EQUIPMENT_SLOT_NECK: case EQUIPMENT_SLOT_SHOULDERS: case EQUIPMENT_SLOT_CHEST: case EQUIPMENT_SLOT_WAIST:
        case EQUIPMENT_SLOT_LEGS: case EQUIPMENT_SLOT_FEET: case EQUIPMENT_SLOT_WRISTS: case EQUIPMENT_SLOT_HANDS: case EQUIPMENT_SLOT_FINGER1:
        case EQUIPMENT_SLOT_FINGER2: case EQUIPMENT_SLOT_BACK:
            return true;
        default:
            return false;
    }
}

// Preferred armor tier comes from BotItemScorer - ONE definition shared with
// the scorer that ranks these pools. Keeping a second copy here is what let the
// two drift apart.
using ::Playerbot::Gear::PreferredArmorForClass;

// Quick "is armor inventory type"
bool IsArmorSlot(uint8 slot)
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD: case EQUIPMENT_SLOT_SHOULDERS:
        case EQUIPMENT_SLOT_CHEST: case EQUIPMENT_SLOT_WAIST:
        case EQUIPMENT_SLOT_LEGS: case EQUIPMENT_SLOT_FEET:
        case EQUIPMENT_SLOT_WRISTS: case EQUIPMENT_SLOT_HANDS:
            return true;
        default:
            return false;
    }
}

// Inventory types acceptable per equipment slot. A slot can match multiple
// inventory types (e.g. CHEST accepts INVTYPE_CHEST and INVTYPE_ROBE).
bool InventoryTypeFitsSlot(uint8 slot, uint8 inv)
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD:      return inv == INVTYPE_HEAD;
        case EQUIPMENT_SLOT_NECK:      return inv == INVTYPE_NECK;
        case EQUIPMENT_SLOT_SHOULDERS: return inv == INVTYPE_SHOULDERS;
        case EQUIPMENT_SLOT_CHEST:     return inv == INVTYPE_CHEST || inv == INVTYPE_ROBE;
        case EQUIPMENT_SLOT_WAIST:     return inv == INVTYPE_WAIST;
        case EQUIPMENT_SLOT_LEGS:      return inv == INVTYPE_LEGS;
        case EQUIPMENT_SLOT_FEET:      return inv == INVTYPE_FEET;
        case EQUIPMENT_SLOT_WRISTS:    return inv == INVTYPE_WRISTS;
        case EQUIPMENT_SLOT_HANDS:     return inv == INVTYPE_HANDS;
        case EQUIPMENT_SLOT_FINGER1:
        case EQUIPMENT_SLOT_FINGER2:   return inv == INVTYPE_FINGER;
        case EQUIPMENT_SLOT_TRINKET1:
        case EQUIPMENT_SLOT_TRINKET2:  return inv == INVTYPE_TRINKET;
        case EQUIPMENT_SLOT_BACK:      return inv == INVTYPE_CLOAK;
        case EQUIPMENT_SLOT_MAINHAND:  return inv == INVTYPE_WEAPON || inv == INVTYPE_2HWEAPON
                                           || inv == INVTYPE_WEAPONMAINHAND;
        case EQUIPMENT_SLOT_OFFHAND:   return inv == INVTYPE_WEAPON || inv == INVTYPE_WEAPONOFFHAND
                                           || inv == INVTYPE_SHIELD || inv == INVTYPE_HOLDABLE;
        case EQUIPMENT_SLOT_RANGED:    return inv == INVTYPE_RANGED || inv == INVTYPE_RANGEDRIGHT
                                           || inv == INVTYPE_THROWN;
        default:                       return false;
    }
}

// Per-class candidate pool: for each slot, sorted-by-ilvl items the class
// can wear. Built once at Initialize().
struct ClassPool
{
    // [slot] -> sorted vec by ItemLevel ASC. Entry = (ItemTemplate*).
    std::array<std::vector<ItemTemplate const*>, 19> slots;
};

// Indexed by class id (1..13). Class 0 unused.
std::array<ClassPool, MAX_CLASSES> g_pools;
bool g_initialized = false;

uint64 fnv1a64(uint64 v)
{
    uint64 h = 0xcbf29ce484222325ULL;
    while (v) { h ^= (v & 0xff); h *= 0x100000001b3ULL; v >>= 8; }
    return h;
}

} // anonymous

// Approximate ilvl target for a level. Linear from L1=1 to L80=600 (TWW
// cap), then L80=600 -> L90=700 for the Midnight band (12.x max-level
// gear starts around ilvl 700). Clamped at the realm cap so a lower
// configured MaxPlayerLevel never targets gear above what it can equip.
// Exposed (declared in the header) for the hygiene under-gear detector.
uint16 TargetIlvlForLevel(uint8 level)
{
    // Classic 1.60 (Forever): the item data is vanilla's, where a level-appropriate piece sits a few item levels above
    // the wearer (quest greens ~level+5) and pre-raid blues at 60 are ilvl ~58-63. The retail ramp below asks for
    // ilvl 450 at 60, which in vanilla data means "the highest item level that exists": a fresh 60 would be handed
    // Naxxramas epics. Aim at dungeon gear instead; raid gear is earned in the raid.
    constexpr uint32 kClassicCap = 60, kClassicCapIlvl = 63;
    if (MaxPlayerLevel() <= kClassicCap)
    {
        if (level <= 1) return 1;
        level = std::min<uint8>(level, MaxPlayerLevel());
        return uint16(level >= kClassicCap ? kClassicCapIlvl : uint32(level) + 5);
    }

    constexpr uint32 kTwwCap = 80, kTwwIlvl = 600;
    constexpr uint32 kMidnightCap = 90, kMidnightIlvl = 700;
    if (level <= 1) return 1;
    level = std::min<uint8>(level, MaxPlayerLevel());
    if (level >= kMidnightCap) return uint16(kMidnightIlvl);
    if (level >= kTwwCap)
        return uint16(kTwwIlvl + ((uint32(level) - kTwwCap) * (kMidnightIlvl - kTwwIlvl)) / (kMidnightCap - kTwwCap));
    return uint16((uint32(level) * kTwwIlvl) / kTwwCap);
}

// Bots.HolidayClothes (the player's file): 1 (default) = bots wear holiday items while that holiday runs, 0 = never.
// Read once at start; a change needs a restart.
static bool HolidayClothesAllowed()
{
    static bool const allowed = sConfigMgr->GetBoolDefault("Bots.HolidayClothes", true, true);
    return allowed;
}

// Holiday items, such as the Red and Green Winter Hats that some bosses drop only during Winter Veil: the holidays each one
// belongs to. A holiday item is one the item data ties to a holiday, one whose loot rows drop it only while a holiday event
// runs (conditions type 12 on a game_event with a holiday), or one a vendor sells only during a holiday. Read once, at the
// first gear look-up after start.
static std::unordered_map<uint32, std::vector<HolidayIds>> const& HolidayItems()
{
    static std::unordered_map<uint32, std::vector<HolidayIds>> const s_items = []
    {
        std::unordered_map<uint32, std::vector<HolidayIds>> items;
        auto add = [&items](uint32 entry, uint32 holiday)
        {
            if (holiday == HOLIDAY_NONE) return;
            std::vector<HolidayIds>& v = items[entry];
            if (std::find(v.begin(), v.end(), HolidayIds(holiday)) == v.end()) v.push_back(HolidayIds(holiday));
        };
        for (auto const& [entry, tpl] : sObjectMgr->GetItemTemplateStore())
            add(entry, tpl.GetHolidayID());
        // condition source types 1-12 are the loot tables; their SourceEntry is the item
        if (QueryResult r = WorldDatabase.Query("SELECT DISTINCT c.SourceEntry, e.holiday FROM conditions c JOIN game_event e ON e.eventEntry = c.ConditionValue1 "
                                                "WHERE c.SourceTypeOrReferenceId BETWEEN 1 AND 12 AND c.ConditionTypeOrReference = 12 "
                                                "AND c.NegativeCondition = 0 AND e.holiday <> 0"))
            do add((*r)[0].GetUInt32(), (*r)[1].GetUInt32()); while (r->NextRow());
        if (QueryResult r = WorldDatabase.Query("SELECT DISTINCT v.item, e.holiday FROM game_event_npc_vendor v JOIN game_event e ON e.eventEntry = v.eventEntry "
                                                "WHERE e.holiday <> 0 AND v.type = 1"))
            do add((*r)[0].GetUInt32(), (*r)[1].GetUInt32()); while (r->NextRow());
        TC_LOG_INFO("playerbot.v2", "[BotGearGenerator] {} holiday items: bots wear them {}", items.size(),
                    HolidayClothesAllowed() ? "only while their holiday runs (Bots.HolidayClothes = 1)" : "never (Bots.HolidayClothes = 0)");
        return items;
    }();
    return s_items;
}

// Whether a bot may wear the item today: anything that is not a holiday item; a holiday item only while one of its holidays
// runs, and never with Bots.HolidayClothes = 0. A hat put on during Winter Veil comes off at the next gear check after it.
// World thread (game events change in World::Update, outside the map updates).
static bool InSeason(uint32 entry)
{
    auto const& items = HolidayItems();
    auto it = items.find(entry);
    if (it == items.end())
        return true;
    if (!HolidayClothesAllowed())
        return false;
    for (HolidayIds h : it->second)
        if (IsHolidayActive(h))
            return true;
    return false;
}

// The item rules that do not change while the server runs; the gear pools are built from these at start.
static bool IsBotWearableAnyDay(uint32 entry)
{
    if (!HolidayClothesAllowed() && HolidayItems().count(entry))
        return false;

    constexpr uint32 kClassicCap = 60, kFirstNonVanillaItemId = 25000;
    if (MaxPlayerLevel() > kClassicCap)
        return true;
    if (entry >= kFirstNonVanillaItemId)
        return false;
    // Honor-rank rewards (Warlord's / Field Marshal's sets...): the server doesn't enforce the rank, so an Alliance
    // human warrior wore the Horde rank 14 set and drew like an orc (Testwar, 2026-10-05)
    ItemTemplate const* tpl = sObjectMgr->GetItemTemplate(entry);
    return !tpl || tpl->ExtendedData->RequiredPVPRank <= 0;
}

bool IsBotWearableItemId(uint32 entry)
{
    return IsBotWearableAnyDay(entry) && InSeason(entry);
}

// A whole word "Test" or "Deprecated" in the name, any case: developer items that ship in the item data. The same holds for the placeholders
// the client data carries for unfinished items: "[PH]" / "PH]" / "[DNT]" markers, "Nax PH ..." and the "90 Epic Warrior Cloak" stand-ins
// (item level 90, above every raid cap but not real gear; found decoding ItemSparse.db2, 224 names).
static bool IsTestItemName(std::string const& name)
{
    std::string lower;
    lower.reserve(name.size());
    for (char c : name) lower.push_back(char(std::tolower(static_cast<unsigned char>(c))));
    if (lower.find("ph]") != std::string::npos || lower.find("dnt]") != std::string::npos || lower.find(" ph ") != std::string::npos)
        return true;
    {
        size_t digits = 0;
        while (digits < lower.size() && std::isdigit(static_cast<unsigned char>(lower[digits]))) ++digits;
        if (digits > 0 && lower.compare(digits, 1, " ") == 0)
            for (char const* tier : { "epic ", "green ", "rare ", "blue ", "purple " })
                if (lower.compare(digits + 1, std::strlen(tier), tier) == 0)
                    return true;
    }
    for (char const* word : { "test", "deprecated" })
    {
        for (size_t pos = lower.find(word); pos != std::string::npos; pos = lower.find(word, pos + 1))
        {
            size_t const end = pos + std::strlen(word);
            bool const start_ok = pos == 0 || !std::isalpha(static_cast<unsigned char>(lower[pos - 1]));
            bool const end_ok = end >= lower.size() || !std::isalpha(static_cast<unsigned char>(lower[end]));
            if (start_ok && end_ok)
                return true;
        }
    }
    return false;
}

void Initialize()
{
    if (g_initialized) return;
    g_initialized = true;

    auto const& store = sObjectMgr->GetItemTemplateStore();
    uint32 examined = 0, indexed = 0, skipped_unrenderable = 0;
    std::vector<uint32> unrenderable_sample;

    for (auto const& [entry, tpl] : store)
    {
        ++examined;
        // Drop bind-on-pickup POOR and COMMON: grey vendor trash and white
        // filler a real player would not keep. Everything from UNCOMMON up is
        // kept whatever its binding.
        //
        // This used to cut at RARE, which threw away every bind-on-pickup green
        // — and at levelling brackets that is precisely where the appropriate
        // gear lives, because quest rewards are BoP uncommons. Measured on the
        // 12.1 client data for an L16 warrior: the old cut dropped 255 usable
        // one-hand swords, and removed ~50% of all candidates uniformly across
        // head, shoulder, back, finger and trinket. That matches the slot-fill
        // deficits seen on the live fleet (chest/legs ~87% filled, head and
        // shoulders ~45%, back 28%).
        //
        // Low risk at endgame rather than provably inert: ranking is
        // effective-item-level driven (+ilvl, +5 per quality tier), so a green
        // only beats a rare when it actually WEARS better, which is the
        // behaviour we want. Endgame greens sit far below endgame rares on
        // item level, so max-level picks should be unchanged - worth watching
        // on the first fleet-wide pass rather than assuming.
        if (tpl.GetBonding() == BIND_ON_ACQUIRE && tpl.GetQuality() < ITEM_QUALITY_UNCOMMON)
            continue;
        // Skip items with quality > Epic (Legendary / Artifact require special unlock)
        if (tpl.GetQuality() > ITEM_QUALITY_EPIC) continue;
        // Internal items are never equippable by anyone - Player::CanUseItem
        // rejects them outright with EQUIP_ERR_CANT_EQUIP_EVER.
        if (tpl.HasFlag(ITEM_FLAG2_INTERNAL_ITEM)) continue;
        if (!IsBotWearableAnyDay(entry)) continue;   // holiday items stay in the pools; GenerateGearFor checks the season
        // A developer test item ("Fast Test Thrown", item 5559) is not gear: bots wore it as their thrown weapon (PC 20 Oct).
        if (IsTestItemName(tpl.GetName(DEFAULT_LOCALE))) continue;
        // Skip items with too-high required level for our brackets
        if (tpl.GetBaseRequiredLevel() > MaxPlayerLevel()) continue;
        // Skip items with no required level set BUT high ilvl (raid drops typically)
        if (tpl.GetBaseRequiredLevel() == 0 && tpl.GetBaseItemLevel() > 50) continue;

        uint8 inv = tpl.GetInventoryType();
        // Find matching slot(s) — most items map to 1 slot, but rings/trinkets
        // map to 2 (FINGER1/2, TRINKET1/2). We only index FINGER1/TRINKET1
        // and AutoEquip later picks 2nd slot.
        uint8 target_slot = 255;
        switch (inv)
        {
            case INVTYPE_HEAD:        target_slot = EQUIPMENT_SLOT_HEAD; break;
            case INVTYPE_NECK:        target_slot = EQUIPMENT_SLOT_NECK; break;
            case INVTYPE_SHOULDERS:   target_slot = EQUIPMENT_SLOT_SHOULDERS; break;
            case INVTYPE_CHEST:
            case INVTYPE_ROBE:        target_slot = EQUIPMENT_SLOT_CHEST; break;
            case INVTYPE_WAIST:       target_slot = EQUIPMENT_SLOT_WAIST; break;
            case INVTYPE_LEGS:        target_slot = EQUIPMENT_SLOT_LEGS; break;
            case INVTYPE_FEET:        target_slot = EQUIPMENT_SLOT_FEET; break;
            case INVTYPE_WRISTS:      target_slot = EQUIPMENT_SLOT_WRISTS; break;
            case INVTYPE_HANDS:       target_slot = EQUIPMENT_SLOT_HANDS; break;
            case INVTYPE_FINGER:      target_slot = EQUIPMENT_SLOT_FINGER1; break;
            case INVTYPE_TRINKET:     target_slot = EQUIPMENT_SLOT_TRINKET1; break;
            case INVTYPE_CLOAK:       target_slot = EQUIPMENT_SLOT_BACK; break;
            case INVTYPE_WEAPON:
            case INVTYPE_2HWEAPON:
            case INVTYPE_WEAPONMAINHAND: target_slot = EQUIPMENT_SLOT_MAINHAND; break;
            case INVTYPE_RANGED:
            case INVTYPE_RANGEDRIGHT:
            case INVTYPE_THROWN:      target_slot = EQUIPMENT_SLOT_RANGED; break;
            case INVTYPE_WEAPONOFFHAND:
            case INVTYPE_SHIELD:
            case INVTYPE_HOLDABLE:    target_slot = EQUIPMENT_SLOT_OFFHAND; break;
            default:                  continue;  // unsupported slot
        }

        // The item must be RENDERABLE by the client, not merely equippable by
        // the server — see ::Playerbot::IsItemRenderableInSlot for why.
        // Evaluated here rather than at the top of the loop because it needs
        // target_slot. Rejects ~3551 of 175898 examined items (measured
        // 2026-09-16), so it prunes a real but small tail without starving any
        // pool.
        //
        // HISTORY, so nobody re-derives a wrong conclusion from the commit log:
        // this gate was added believing that items 251573-251580 (the Ascension
        // Chaser plate set, worn by bot Sellarino) lacked appearances and were
        // crashing the client on inspect. That was WRONG on both counts. Those
        // items do have ItemModifiedAppearance rows - verified against
        // ItemModifiedAppearance.db2 and by the runtime gate declining to reject
        // them - and the inspect crash was a corrupted packet serializer on the
        // integration branch (ERROR #8, fixed in ba79afcad9), nothing to do with
        // appearances. The gate is kept because an item with no appearance
        // genuinely cannot be drawn and is cheap to exclude, but it has never
        // been shown to fix an observed crash.
        if (!::Playerbot::IsItemRenderableInSlot(entry, target_slot))
        {
            ++skipped_unrenderable;
            // Name the first few. The count alone cannot answer "which items?",
            // which is exactly what was asked when an offline db2 scan flagged
            // nine BACK-slot cloaks as appearance-less while the runtime kept
            // them: whoever checks next needs entries to look up, not a total.
            if (unrenderable_sample.size() < 24)
                unrenderable_sample.push_back(entry);
            continue;
        }

        // Class weapon-proficiency mask, bit = ItemSubclassWeapon value.
        // GetAllowableClass on weapons is usually -1 (all classes), so
        // without THIS filter the generator handed DKs bows and rogues
        // staves: the equip then failed Player::CanUseItem silently, the
        // pipeline latched its AutoEquip bit, and 82% of DKs (plus 27% of
        // DHs, 14% of Evokers) fought BARE-HANDED — a bigger DPS loss than
        // every rotation bug combined; weapon-requiring abilities hard-
        // failed with SPELL_FAILED_EQUIPPED_ITEM_CLASS (audit B15).
        auto weapon_mask_for_class = [](uint8 c) -> uint32 {
            constexpr uint32 AXE=1u<<0, AXE2=1u<<1, BOW=1u<<2, GUN=1u<<3,
                MACE=1u<<4, MACE2=1u<<5, POLEARM=1u<<6, SWORD=1u<<7,
                SWORD2=1u<<8, GLAIVE=1u<<9, STAFF=1u<<10, FIST=1u<<13,
                DAGGER=1u<<15, THROWN=1u<<16, XBOW=1u<<18, WAND=1u<<19;
            switch (c)
            {
                case 1:  return AXE|AXE2|MACE|MACE2|POLEARM|SWORD|SWORD2|STAFF|DAGGER|FIST|BOW|GUN|XBOW|THROWN; // Warrior
                case 2:  return AXE|AXE2|MACE|MACE2|POLEARM|SWORD|SWORD2;                   // Paladin
                case 3:  return BOW|GUN|XBOW|AXE|AXE2|SWORD|SWORD2|POLEARM|STAFF|DAGGER|FIST; // Hunter
                case 4:  return DAGGER|SWORD|AXE|MACE|FIST|BOW|GUN|XBOW|THROWN;             // Rogue
                case 5:  return MACE|DAGGER|STAFF|WAND;                                     // Priest
                case 6:  return AXE|AXE2|MACE|MACE2|SWORD|SWORD2|POLEARM;                   // Death Knight
                case 7:  return AXE|AXE2|MACE|MACE2|STAFF|DAGGER|FIST;                      // Shaman
                case 8:  return DAGGER|SWORD|STAFF|WAND;                                    // Mage
                case 9:  return DAGGER|SWORD|STAFF|WAND;                                    // Warlock
                case 10: return FIST|AXE|MACE|SWORD|STAFF|POLEARM;                          // Monk
                case 11: return MACE|MACE2|STAFF|DAGGER|FIST|POLEARM;                       // Druid
                case 12: return GLAIVE|SWORD|AXE|FIST;                                      // Demon Hunter
                case 13: return DAGGER|FIST|MACE|SWORD|AXE|STAFF;                           // Evoker
                default: return 0;
            }
        };
        // For each class, check if it can wear this item.
        for (uint8 cls = 1; cls < MAX_CLASSES; ++cls)
        {
            uint32 cls_mask = (1u << (cls - 1));
            uint32 allowable = tpl.GetAllowableClass();
            // 0 / -1 means usable by all classes.
            if (allowable && allowable != uint32(-1) && !(allowable & cls_mask))
                continue;

            // Weapon proficiency (mask table above).
            if (tpl.GetClass() == ITEM_CLASS_WEAPON)
            {
                const uint32 sub = tpl.GetSubClass();
                if (sub > 31 || !(weapon_mask_for_class(cls) & (1u << sub)))
                    continue;
            }
            // Shields: Warrior / Paladin / Shaman only.
            if (inv == INVTYPE_SHIELD && cls != 1 && cls != 2 && cls != 7)
                continue;

            // For armor slots, filter by class's preferred armor type.
            if (IsArmorSlot(target_slot) && tpl.GetClass() == ITEM_CLASS_ARMOR)
            {
                ItemSubclassArmor preferred = PreferredArmorForClass(cls);
                // Allow items at or below preferred (cloth-wearing under-leather is bad,
                // but accept for low-level brackets where higher armor unavailable).
                ItemSubclassArmor item_armor = ItemSubclassArmor(tpl.GetSubClass());
                if (item_armor != preferred &&
                    item_armor != ITEM_SUBCLASS_ARMOR_MISCELLANEOUS &&
                    item_armor != ITEM_SUBCLASS_ARMOR_COSMETIC)
                {
                    // Allow lighter armor (cloth ≤ leather ≤ mail ≤ plate) for filler
                    // but prefer same. We'll let GenerateGearFor pick highest matching.
                    if (item_armor > preferred) continue;
                }
            }

            g_pools[cls].slots[target_slot].push_back(&tpl);
        }
        ++indexed;
    }

    // Sort each pool by static base ItemLevel ascending. This is ENUMERATION
    // ORDER ONLY — GenerateGearFor scores every candidate (it does not binary-
    // search), so the sort key does not affect the pick. Do NOT switch this to
    // effective/level-scaled ilvl: the pool is built once at init with no
    // wearer level, while effective ilvl is level-dependent (that scaling is
    // applied per-pick in GenerateGearFor).
    for (auto& pool : g_pools)
        for (auto& slot_vec : pool.slots)
            std::sort(slot_vec.begin(), slot_vec.end(),
                [](ItemTemplate const* a, ItemTemplate const* b) {
                    return a->GetBaseItemLevel() < b->GetBaseItemLevel();
                });

    // skipped_unrenderable is reported separately because it is the count that
    // matters after a data import: a sudden jump means a batch of items landed
    // in Item/ItemSparse without appearances, which is precisely what made the
    // client crash on inspect before this gate existed.
    TC_LOG_INFO("playerbot.v2",
                "[BotGearGenerator] indexed {} items (of {} examined) into per-class pools; "
                "rejected {} with no ItemModifiedAppearance (not client-renderable)",
                indexed, examined, skipped_unrenderable);

    if (!unrenderable_sample.empty())
    {
        std::string sample;
        for (uint32 e : unrenderable_sample)
        {
            if (!sample.empty()) sample += ", ";
            sample += std::to_string(e);
        }
        TC_LOG_INFO("playerbot.v2",
            "[BotGearGenerator] first {} rejected entries: {}{}",
            unrenderable_sample.size(), sample,
            skipped_unrenderable > unrenderable_sample.size() ? " ..." : "");
    }
}

// How far below the best candidate a pick may score, and how many effective item levels it may differ by, and still be
// chosen for variety (see GenerateGearFor). 40 points is 20 item levels of distance or a minor secondary-stat gap; a
// wrong armour type (-300 relative) or primary stat (-500) is far outside it.
// kVarietyIlvlBand is in the header: a companion gear re-roll uses the same band (BotPopulationManager::RerollCompanionGear).
constexpr int32 kVarietyScoreMargin = 40;

bool UsesThrownRanged(uint8 cls)
{
    return (cls == CLASS_WARRIOR || cls == CLASS_ROGUE) && Services::Config().behavior_tank_ranged_weapon();
}

int32 LiveRaidIlvlCap() { return RaidIlvlCap(); }
int32 LiveWorldIlvlCap() { return WorldIlvlCap(); }

bool OwnedLevelGearActive(uint8 level, ::Playerbot::Pool::BotRole role)
{
    static bool const enabled = sConfigMgr->GetBoolDefault("Playerbot.Gear.Owned.Enable", true, true);
    constexpr uint32 kClassicCap = 60;
    return enabled && role == ::Playerbot::Pool::BotRole::Owned && MaxPlayerLevel() <= kClassicCap && level < MaxPlayerLevel();
}

uint32 OwnedAllowedModes()
{
    static uint32 const mask = Owned::ParseAllowedModes(sConfigMgr->GetStringDefault("Playerbot.OwnedGear.AllowedModes", "", true));
    return mask;
}

bool OwnedMatchOwnerActive()
{
    static bool const on = sConfigMgr->GetBoolDefault("Playerbot.Gear.Owned.MatchOwner", true, true);
    return on;
}

int32 OwnedOwnerSlack()
{
    static int32 const slack = std::clamp(sConfigMgr->GetIntDefault("Playerbot.Gear.Owned.OwnerSlack", Owned::kDefaultOwnerSlack, true), 0, 30);
    return slack;
}

int32 OwnedConfiguredCap() { return RaidIlvlCap(); }

int32 OwnedSlotScore(ItemTemplate const* tpl, uint8 cls, uint16 spec, uint8 level, uint8 slot)
{
    if (!tpl)
        return -100000;
    int32 const fit = ::Playerbot::Gear::ScoreItemRaid(tpl, cls, spec, level, slot);
    return fit <= -100000 ? fit : Owned::QualityAdjusted(fit, int32(tpl->GetQuality()));
}

bool WorldGearOver(ItemTemplate const* tpl, uint8 level, uint64 bot_id, uint8 slot)
{
    if (!tpl)
        return false;
    if (::Playerbot::Gear::EffectiveItemLevelForLevel(tpl, level) > WorldIlvlCap())
        return true;
    if (tpl->GetQuality() < ITEM_QUALITY_EPIC || WorldEpicAllowed(bot_id, slot))
        return false;
    // A rank piece of the bot's PvP set is exempt from the epic share (the item level cap still applies).
    return !(WorldPvpSetBot(bot_id) && PvpSetSlot(slot) && ::Playerbot::Gear::IsPvpItem(tpl));
}

bool WorldPvpSet(uint64 bot_id)
{
    return WorldPvpSetBot(bot_id);
}

bool RaidGearActive(uint8 level)
{
    constexpr uint32 kClassicCap = 60;
    return Services::Config().behavior_raid_gear() && MaxPlayerLevel() <= kClassicCap && level >= MaxPlayerLevel();
}

bool RaidEquipUpgrade(uint8 cls, uint16 spec, uint8 level, uint8 slot, uint32 bag_entry,
                      uint32 cur_entry, uint32 cur_mainhand_entry, uint32 cur_offhand_entry, bool world_bot, uint64 bot_id)
{
    ItemTemplate const* bag = sObjectMgr->GetItemTemplate(bag_entry);
    if (!bag || cls == 0 || cls >= MAX_CLASSES || ::Playerbot::Gear::IsPvpItem(bag) ||
        ::Playerbot::Gear::EffectiveItemLevelForLevel(bag, level) > (world_bot ? WorldIlvlCap() : RaidIlvlCap()) ||
        (world_bot && WorldGearOver(bag, level, bot_id, slot)))
        return false;
    if (slot == EQUIPMENT_SLOT_OFFHAND)
    {
        ItemTemplate const* mh = cur_mainhand_entry ? sObjectMgr->GetItemTemplate(cur_mainhand_entry) : nullptr;
        if (mh && mh->GetInventoryType() == INVTYPE_2HWEAPON)
            return false;
        if (::Playerbot::Gear::OffhandMustBeWeapon(cls, spec) && bag->GetClass() != ITEM_CLASS_WEAPON)   // dual-wielders and two-hand users never take a shield or a held item
            return false;
    }
    int32 const bag_score = ::Playerbot::Gear::ScoreItemRaid(bag, cls, spec, level, slot);
    if (bag_score <= -100000)
        return false;
    if (!cur_entry)
        return true;
    ItemTemplate const* cur = sObjectMgr->GetItemTemplate(cur_entry);
    if (!cur)
        return true;
    if (::Playerbot::Gear::IsPvpItem(cur))
        return true;
    int32 cur_score = ::Playerbot::Gear::ScoreItemRaid(cur, cls, spec, level, slot);
    if (slot == EQUIPMENT_SLOT_MAINHAND && bag->GetInventoryType() == INVTYPE_2HWEAPON && cur_offhand_entry)
        if (ItemTemplate const* oh = sObjectMgr->GetItemTemplate(cur_offhand_entry))
            cur_score += ::Playerbot::Gear::ScoreItemRaid(oh, cls, spec, level, EQUIPMENT_SLOT_OFFHAND);
    return cur_score <= 0 ? bag_score > cur_score : float(bag_score) >= float(cur_score) * kRaidUpgradeRatio;
}

std::string PlanEquipOrder(::Playerbot::BotSnapshot const& snap, std::string const& bot_name, std::string const& arg, std::vector<EquipOrderPick>& picks)
{
    using ::Playerbot::EquippedItem;
    using ::Playerbot::InventoryItem;
    using ::Playerbot::EquipFitScore;
    picks.clear();
    auto const& equipped = snap.inventory.equipped;
    auto cur_level_of = [&](uint8 slot) -> int32 { return slot < equipped.size() && equipped[slot].entry ? int32(equipped[slot].item_level) : 0; };
    auto describe = [&](InventoryItem const& it, uint8 slot)
    {
        ItemTemplate const* t = sObjectMgr->GetItemTemplate(it.entry);
        int32 const d = int32(it.item_level) - cur_level_of(slot);
        return fmt::format("[{}] ({}ilvl {})", t ? t->GetDefaultLocaleName() : "item", d >= 0 ? "+" : "", d);
    };
    auto join = [&](std::vector<std::string> const& parts)
    {
        std::string s;
        for (std::string const& p : parts) { if (!s.empty()) s += ", "; s += p; }
        return s;
    };
    std::string a = arg;
    while (!a.empty() && a.front() == ' ') a.erase(a.begin());
    while (!a.empty() && a.back() == ' ') a.pop_back();
    bool const upgrades = a.empty() || a == "upgrades" || a == "upgrade" || a == "best" || a == "best gear" || a == "your best gear" ||
                          a == "all" || a == "everything";

    uint8 const cls = snap.identity.cls, level = snap.identity.level;
    uint16 const spec = uint16(snap.identity.spec);
    bool const raid = RaidGearActive(level);

    if (!upgrades)
    {
        // One named bag item: by the id of its link, else by (part of) its name.
        uint32 id = 0;
        if (size_t p = a.find("item:"); p != std::string::npos)
            id = uint32(std::strtoul(a.c_str() + p + 5, nullptr, 10));
        else if (a.find_first_not_of("0123456789") == std::string::npos)
            id = uint32(std::strtoul(a.c_str(), nullptr, 10));   // the older "equip <entry>"
        std::string name = a;
        if (size_t l = a.find('['); l != std::string::npos)
            if (size_t r = a.find(']', l); r != std::string::npos)
                name = a.substr(l + 1, r - l - 1);
        InventoryItem const* found = nullptr;
        for (InventoryItem const& it : snap.inventory.bag_items)
        {
            ItemTemplate const* t = sObjectMgr->GetItemTemplate(it.entry);
            if (!t || t->GetInventoryType() == INVTYPE_NON_EQUIP) continue;
            bool match = id ? it.entry == id : false;
            if (!id)
            {
                std::string n = t->GetDefaultLocaleName();
                std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                match = !name.empty() && n.find(name) != std::string::npos;
            }
            if (match) { found = &it; break; }
        }
        if (!found)
            return fmt::format("{}: I don't have that in my bags", bot_name);
        ItemTemplate const* t = sObjectMgr->GetItemTemplate(found->entry);
        // Too low for it (Gremloc 11-08: a traded piece above the bot's level stayed in the bags with no word why).
        if (t && t->GetBaseRequiredLevel() > int32(level))
            return fmt::format("{}: I need level {} for [{}], I'm {}", bot_name, t->GetBaseRequiredLevel(), t->GetDefaultLocaleName(), uint32(level));
        if (::Playerbot::Gear::IsPvpItem(t))
            return fmt::format("{}: [{}] is a PvP item, I don't wear those", bot_name, t->GetDefaultLocaleName());
        if (found->equip_slot == 0xFF || found->equip_slot >= equipped.size())
            return fmt::format("{}: I can't wear [{}]", bot_name, t->GetDefaultLocaleName());
        picks.push_back({ found->bag, found->slot, found->equip_slot, found->entry });
        return fmt::format("{}: equipped {}", bot_name, describe(*found, found->equip_slot));
    }

    // Upgrades: best candidate per slot first; a ring / trinket may take the second slot.
    struct Cand { InventoryItem const* it; uint8 slot; };
    std::vector<Cand> cands;
    for (InventoryItem const& it : snap.inventory.bag_items)
    {
        if (it.equip_slot == 0xFF || it.equip_slot >= equipped.size() || it.item_level == 0 || it.quality == 0) continue;
        // Above the bot's level: the server refuses it, but the reply said "equipped" (Gremloc 11-08). Named below if nothing else goes on.
        if (ItemTemplate const* t = sObjectMgr->GetItemTemplate(it.entry); t && t->GetBaseRequiredLevel() > int32(level)) continue;
        cands.push_back({ &it, it.equip_slot });
    }
    auto score_of = [&](InventoryItem const& it, uint8 slot) -> int32
    {
        ItemTemplate const* t = sObjectMgr->GetItemTemplate(it.entry);
        return t ? ::Playerbot::Gear::ScoreItemRaid(t, cls, spec, level, slot) : 0;
    };
    std::ranges::sort(cands, [&](Cand const& x, Cand const& y) { return raid ? score_of(*x.it, x.slot) > score_of(*y.it, y.slot)
                                                                              : x.it->item_level > y.it->item_level; });
    std::array<bool, 19> taken{};
    std::vector<std::string> done;
    for (Cand const& c : cands)
    {
        InventoryItem const& it = *c.it;
        uint8 slot = c.slot;
        if (taken[slot])
        {
            uint8 const alt = slot == EQUIPMENT_SLOT_FINGER1 ? EQUIPMENT_SLOT_FINGER2 : slot == EQUIPMENT_SLOT_TRINKET1 ? EQUIPMENT_SLOT_TRINKET2 : 255;
            if (alt == 255 || taken[alt]) continue;
            slot = alt;
        }
        // An off hand next to a two-hander that is being, or stays, worn.
        EquippedItem const& cur = equipped[slot];
        bool better;
        if (raid)
            better = RaidEquipUpgrade(cls, spec, level, slot, it.entry, cur.entry, equipped[EQUIPMENT_SLOT_MAINHAND].entry,
                                      equipped[EQUIPMENT_SLOT_OFFHAND].entry);
        else
            better = cur.entry == 0 || EquipFitScore(it.stats, it.item_level, snap.stat_weights) > EquipFitScore(cur.stats, cur.item_level, snap.stat_weights);
        if (!better) continue;
        if (ItemTemplate const* t = sObjectMgr->GetItemTemplate(it.entry); !t || ::Playerbot::Gear::IsPvpItem(t)) continue;
        taken[slot] = true;
        picks.push_back({ it.bag, it.slot, slot, it.entry });
        done.push_back(describe(it, slot));
    }
    if (picks.empty())
    {
        // Say why a piece the player may have meant stays in the bags: the lowest required level above the bot's.
        ItemTemplate const* too_high = nullptr;
        for (InventoryItem const& it : snap.inventory.bag_items)
            if (ItemTemplate const* t = sObjectMgr->GetItemTemplate(it.entry);
                t && (t->GetClass() == ITEM_CLASS_ARMOR || t->GetClass() == ITEM_CLASS_WEAPON) && t->GetInventoryType() != INVTYPE_NON_EQUIP &&
                t->GetBaseRequiredLevel() > int32(level) && (!too_high || t->GetBaseRequiredLevel() < too_high->GetBaseRequiredLevel()))
                too_high = t;
        if (too_high)
            return fmt::format("{}: nothing better in my bags I can wear yet ([{}] needs level {}, I'm {})", bot_name,
                               too_high->GetDefaultLocaleName(), too_high->GetBaseRequiredLevel(), uint32(level));
        return fmt::format("{}: nothing better in my bags", bot_name);
    }
    return fmt::format("{}: equipped {}", bot_name, join(done));
}

uint32 PickThrownWeapon(Player const* wearer)
{
    if (!g_initialized || !wearer || wearer->GetClass() >= MAX_CLASSES) return 0;
    uint8 const level = wearer->GetLevel();
    ItemTemplate const* best = nullptr;
    // A thrown weapon whose display is the generic 1.60 one is drawn by the client as a white box with blue dots (UBRS, 20 Oct):
    // a weapon with its own display goes first, whatever its level.
    auto key = [&](ItemTemplate const* t)
    {
        return std::tuple<bool, uint32, uint32, int32, uint32>(Player::HasDrawableAmmoDisplay(t), t->GetBaseRequiredLevel(), uint32(t->GetQuality()),
            ::Playerbot::Gear::EffectiveItemLevelForLevel(t, level), ~t->GetId());
    };
    static std::once_flag thrown_log_once;
    std::call_once(thrown_log_once, [&]
    {
        for (ItemTemplate const* tpl : g_pools[wearer->GetClass()].slots[EQUIPMENT_SLOT_RANGED])
            if (tpl->GetClass() == ITEM_CLASS_WEAPON && tpl->GetSubClass() == ITEM_SUBCLASS_WEAPON_THROWN)
                TC_LOG_INFO("playerbot.v2", "[thrown_display] item {} required level {} display {} drawable {}", tpl->GetId(), tpl->GetBaseRequiredLevel(),
                    Player::GetAmmoDisplayId(tpl), int(Player::HasDrawableAmmoDisplay(tpl)));
    });
    for (ItemTemplate const* tpl : g_pools[wearer->GetClass()].slots[EQUIPMENT_SLOT_RANGED])
    {
        if (tpl->GetClass() != ITEM_CLASS_WEAPON || tpl->GetSubClass() != ITEM_SUBCLASS_WEAPON_THROWN) continue;
        if (tpl->GetBaseRequiredLevel() > level) continue;
        if (wearer->CanUseItem(tpl) != EQUIP_ERR_OK || (tpl->GetSkill() && !wearer->GetSkillValue(tpl->GetSkill()))) continue;
        if (!tpl->GetAllowableRace().HasRace(wearer->GetRace())) continue;
        uint32 const team = ::Player::TeamForRace(wearer->GetRace());
        if (tpl->HasFlag(ITEM_FLAG2_FACTION_HORDE) && team != HORDE) continue;
        if (tpl->HasFlag(ITEM_FLAG2_FACTION_ALLIANCE) && team != ALLIANCE) continue;
        if (!best || key(tpl) > key(best)) best = tpl;
    }
    return best ? best->GetId() : 0;
}

std::vector<GearItem> GenerateGearFor(GearGenerationContext const& ctx)
{
    std::vector<GearItem> out;
    if (!g_initialized || ctx.cls < 1 || ctx.cls >= MAX_CLASSES) return out;
    // A world bot below level 10 keeps its starting outfit (WorldGearStart.h).
    if (!::Playerbot::Gear::GeneratesGear(ctx.world_bot(), ctx.level, ctx.replace_unrenderable_starter)) return out;

    uint16 const target_ilvl = TargetIlvlForLevel(ctx.level);
    auto const& pool = g_pools[ctx.cls];

    // Shield-tank specs (Protection Warrior 73, Protection Paladin 66) MUST wield
    // a ONE-HAND weapon so they can also carry a shield: a 2H locks out the
    // offhand, and with no shield the tank loses block, ~half its armor, and its
    // entire Shield Slam / Shield Block kit. The raw-stat scorer ranks a 2H above
    // a 1H, so without this guard the generator handed prot tanks a 2H mainhand
    // plus a shield it could NEVER equip (EQUIP_ERR_2HANDED_EQUIPPED) — observed
    // live 2026-06-28: the Deadmines prot-warrior tank ran a 2H with an empty
    // offhand and died 38x in the harbor, casting Shield Slam/Block with no
    // shield. For these specs we skip INVTYPE_2HWEAPON mainhand candidates so the
    // pick falls to the best 1H, leaving the offhand free for the shield.
    const bool shield_tank = (ctx.spec == 73 || ctx.spec == 66);
    bool const thrown_only = UsesThrownRanged(ctx.cls);
    // Level-cap bots: role-weighted pre-raid picks (see kRaidMinRequiredLevel in the header).
    bool const raid = RaidGearActive(ctx.level);
    // Companion below the cap: role-weighted best green / blue near its level (OwnedGearPolicy.h).
    bool const owned_lv = !raid && OwnedLevelGearActive(ctx.level, ctx.role);
    int32 const owned_band = OwnedIlvlBandCfg();
    bool const classic_levelling = !raid && MaxPlayerLevel() <= 60 && ctx.level < MaxPlayerLevel();
    // Tier modes (an owner's "gear t3"): the tier set below, and the other slots up to that tier's item level.
    bool const tier_mode = raid && !ctx.world_bot() && Owned::IsTierMode(ctx.gear_mode);
    bool const bis_mode = raid && !ctx.world_bot() && ctx.gear_mode == Owned::GearMode::Bis;
    // Resistance gear for a fight (ctx.resist_school): a piece with the school's resistance outranks any stat fit in the armour and
    // jewellery slots, more resistance first; the slots it fills are kept from the tier set below.
    bool const resist_mode = raid && !ctx.world_bot() && ctx.resist_school != 0;
    constexpr int32 kResistPick = 10000000;
    std::array<bool, EQUIPMENT_SLOT_END> resist_held{};
    int32 const raid_cap = ctx.world_bot() ? WorldIlvlCap()
                         : tier_mode      ? CapForTier(float(Owned::TierNumber(ctx.gear_mode)))
                         : bis_mode       ? CapForTier(3.0f)
                         : (ctx.ilvl_cap > 0 ? ctx.ilvl_cap : RaidIlvlCap());
    // A second one-hand weapon in the offhand needs Dual Wield: rogues, and warriors / hunters / shamans that learned it.
    bool const can_dual_wield = ctx.wearer ? ctx.wearer->CanDualWield()
                                           : (ctx.cls == CLASS_ROGUE || ctx.cls == CLASS_WARRIOR || ctx.cls == CLASS_HUNTER);

    out.reserve(kSlots.size());
    uint32 slot_idx = 0;
    // FINGER2/TRINKET2 draw from FINGER1/TRINKET1's pool and score it with the
    // same weights, so left to itself the generator picks the IDENTICAL entry
    // for both. That is fine for a plain ring and fatal for a unique-equipped
    // one: Player::CanEquipItem refuses the second with
    // EQUIP_ERR_ITEM_UNIQUE_EQUIPPABLE and the slot is left EMPTY. Most
    // trinkets and many rings carry that flag. Measured across 2461 warriors on
    // the live fleet before this fix: finger1 1067 filled -> finger2 790,
    // trinket1 698 -> trinket2 551.
    ItemTemplate const* picked_finger1 = nullptr;
    ItemTemplate const* picked_trinket1 = nullptr;
    ItemTemplate const* picked_mainhand = nullptr;
    for (uint8 slot : kSlots)
    {
        // Pick from pool slot.
        // For FINGER2/TRINKET2, reuse FINGER1/TRINKET1 pool (same items wearable in both).
        uint8 lookup_slot = (slot == EQUIPMENT_SLOT_FINGER2)  ? EQUIPMENT_SLOT_FINGER1
                          : (slot == EQUIPMENT_SLOT_TRINKET2) ? EQUIPMENT_SLOT_TRINKET1
                          : slot;
        auto const& candidates = pool.slots[lookup_slot];
        if (candidates.empty()) { ++slot_idx; continue; }
        // A two-handed mainhand leaves no offhand to fill (no Titan's Grip in Classic): an offhand pick next to it
        // can never be worn together with it, and equipped first it blocks the two-hander.
        if (slot == EQUIPMENT_SLOT_OFFHAND && picked_mainhand && picked_mainhand->GetInventoryType() == INVTYPE_2HWEAPON)
        { ++slot_idx; continue; }

        // Pick the candidate that best fits the bot's class+spec and lands
        // near the target ilvl. Combined score = fit - 2 * |ilvl - target|;
        // the distance penalty keeps us in the bracket's ilvl band, while
        // the shared Gear scorer (primary stat, armor type, secondaries)
        // breaks ties so a wrong-primary item never beats a right-primary
        // alternative within the same band. ~250 ilvl-distance points is
        // worth one primary-stat match (+500 in the scorer).
        ItemTemplate const* best = nullptr;
        int32 best_score = std::numeric_limits<int32>::min();
        int32 const target = int32(target_ilvl);
        struct Scored { ItemTemplate const* tpl; int32 combined; int32 ilvl; };
        std::vector<Scored> scored;
        // The entry already chosen for the paired slot, if any.
        ItemTemplate const* twin = (slot == EQUIPMENT_SLOT_FINGER2)  ? picked_finger1
                                 : (slot == EQUIPMENT_SLOT_TRINKET2) ? picked_trinket1
                                 : nullptr;

        // Raid mode runs two passes: the rare / epic raid-prep candidates first (cheap to reject the rest), everything
        // else only for a slot where none of those was wearable.
        // A world bot (also every BG / pool bot) runs the plain pass too: some of its slots take a green or an everyday blue
        // below the pre-raid rares, so a roster does not wear one set.
        bool const world_mix = raid && ctx.world_bot() && ctx.bot_id;
        bool const pvp_slot = world_mix && WorldPvpSetBot(ctx.bot_id) && PvpSetSlot(slot);
        bool const resist_slot = resist_mode && ResistGearSlot(slot);
        // A resistance slot runs the plain pass too when the raid-prep pass found no piece with the school's resistance: a green of it
        // beats a blue without, as the raiders of the day wore them.
        for (int pass = raid ? 0 : 1; pass < 2 && (!best || world_mix || (resist_slot && best_score < kResistPick)); ++pass)
        for (auto const* tpl : candidates)
        {
            if (tpl->GetBaseRequiredLevel() > ctx.level) continue;
            if (!InSeason(tpl->GetId())) continue;
            if (raid)
            {
                bool const prep_item = tpl->GetQuality() >= ITEM_QUALITY_RARE && tpl->GetBaseRequiredLevel() >= int32(kRaidMinRequiredLevel);
                if (prep_item != (pass == 0)) continue;
            }
            // Warrior / rogue ranged slot: thrown only (a bow or gun needs ammo nobody restocks; thrown is its own ammo).
            if (thrown_only && slot == EQUIPMENT_SLOT_RANGED && tpl->GetSubClass() != ITEM_SUBCLASS_WEAPON_THROWN) continue;
            if (ctx.wearer && (ctx.wearer->CanUseItem(tpl) != EQUIP_ERR_OK ||
                               (tpl->GetSkill() && !ctx.wearer->GetSkillValue(tpl->GetSkill()))))
                continue;
            // Race and faction. The pool is per-class and cannot carry these,
            // so without the check here the generator happily hands a bot an
            // item Player::CanUseItem rejects with EQUIP_ERR_CANT_EQUIP_EVER -
            // the pick is staged into the bags, the equip is refused, and the
            // slot stays as it was. Live case 2026-09-16: Sellarino, a Dwarf
            // (Alliance) Protection warrior, was handed rare sword 218333 for a
            // mainhand still holding an ilvl-1 white starter, and the swap
            // failed with err=10 every pass.
            if (ctx.race)
            {
                if (!tpl->GetAllowableRace().HasRace(ctx.race)) continue;
                uint32 const team = ::Player::TeamForRace(ctx.race);
                if (tpl->HasFlag(ITEM_FLAG2_FACTION_HORDE)    && team != HORDE)    continue;
                if (tpl->HasFlag(ITEM_FLAG2_FACTION_ALLIANCE) && team != ALLIANCE) continue;
            }
            // Never hand the paired slot the same entry: prefer the next-best
            // distinct item. A fallback below restores the duplicate when it is
            // legal and nothing else qualifies.
            if (twin && tpl->GetId() == twin->GetId()) continue;
            // Shield-tank mainhand: never a 2H (see shield_tank note above).
            if (shield_tank && slot == EQUIPMENT_SLOT_MAINHAND &&
                tpl->GetInventoryType() == INVTYPE_2HWEAPON)
                continue;
            // Shield-tank offhand: must be a SHIELD — not an offhand weapon /
            // holdable a prot warrior/paladin can't dual-wield (it would fail
            // EQUIP_ERR_2HSKILLNOTFOUND and leave the tank shield-less).
            if (shield_tank && slot == EQUIPMENT_SLOT_OFFHAND &&
                tpl->GetInventoryType() != INVTYPE_SHIELD)
                continue;
            // Use the LEVEL-SCALED effective ilvl, not the static base. TC 12.0
            // scales many items at wear-time (PlayerLevelToItemLevelCurveId);
            // ranking by static base made the generator pick a base-120 piece
            // that collapses to effective ilvl 5 when worn at L16 over a static
            // green that wears at ~20 — the under-gearing root (Varethon L16
            // ItemLevel 5, Loroyn L45 ItemLevel 15). dist is now in effective
            // units so the pick lands near what the bot actually wears.
            // A rogue wields two weapons: a shield or a held-in-off-hand caster item (Royal Scepter of Vek'lor, the tier 3 damage_parity
            // rogue 2026-10-11) leaves the off hand dead, dual_wield false and the rogue at a third of its damage. Whatever the gear mode.
            if (slot == EQUIPMENT_SLOT_OFFHAND && ::Playerbot::Gear::OffhandMustBeWeapon(ctx.cls, ctx.spec) && tpl->GetClass() != ITEM_CLASS_WEAPON) continue;
            int32 ilvl = ::Playerbot::Gear::EffectiveItemLevelForLevel(tpl, ctx.level);
            int32 combined;
            if (raid)
            {
                // Pre-raid, never PvP, never raid-tier above the cap, a usable weapon style.
                bool const pvp_piece = pvp_slot && ::Playerbot::Gear::IsPvpItem(tpl) && tpl->GetQuality() >= ITEM_QUALITY_RARE;
                if ((!pvp_piece && ::Playerbot::Gear::IsPvpItem(tpl)) || ilvl > raid_cap) continue;
                if (!pvp_piece && ctx.world_bot() && tpl->GetQuality() >= ITEM_QUALITY_EPIC && !WorldEpicAllowed(ctx.bot_id, slot)) continue;
                if (slot == EQUIPMENT_SLOT_OFFHAND && !can_dual_wield && tpl->GetClass() == ITEM_CLASS_WEAPON) continue;
                int32 const fit = ::Playerbot::Gear::ScoreItemRaid(tpl, ctx.cls, ctx.spec, ctx.level, slot);
                if (fit <= -100000) continue;
                // Rare / epic of the raid-prep levels outrank everything else, whatever the stats; the rest only fill a slot
                // nothing better can.
                bool const prep = tpl->GetQuality() >= ITEM_QUALITY_RARE && tpl->GetBaseRequiredLevel() >= int32(kRaidMinRequiredLevel);
                combined = fit + (prep ? 1000000 : 0) + (pvp_piece ? 100000 : 0);
                if (resist_slot)
                    if (int32 const points = ::Playerbot::Gear::ResistPoints(tpl, ctx.resist_school, ctx.level); points > 0)
                        combined += kResistPick + points * 20000;   // the stat fit only decides between pieces of equal resistance
            }
            else if (owned_lv)
            {
                if (!Owned::PieceAllowed(int32(tpl->GetQuality()), ilvl, ctx.level, owned_band) || ::Playerbot::Gear::IsPvpItem(tpl)) continue;
                if (slot == EQUIPMENT_SLOT_OFFHAND && !can_dual_wield && tpl->GetClass() == ITEM_CLASS_WEAPON) continue;
                int32 const fit = OwnedSlotScore(tpl, ctx.cls, ctx.spec, ctx.level, slot);
                if (fit <= -100000) continue;
                combined = fit;
            }
            else
            {
                if (ctx.world_bot() && WorldGearOver(tpl, ctx.level, ctx.bot_id, slot)) continue;
                // Classic below the cap: the same ceiling as a companion's (Owned::PieceAllowed), at most the band above the
                // wearer's level. The distance penalty alone let a piece with no required level and a strong stat fit through,
                // so low-level world bots wore gear far above their level (player report, Discord 11-06).
                if (classic_levelling && ilvl > int32(ctx.level) + owned_band) continue;
                int32 dist = ilvl > target ? ilvl - target : target - ilvl;
                int32 fit = ::Playerbot::Gear::ScoreItemForClass(
                    tpl, ctx.cls, ctx.spec, ctx.level);
                combined = fit - 2 * dist;
            }
            scored.push_back({ tpl, combined, ilvl });
            if (combined > best_score) { best_score = combined; best = tpl; }
        }

        // Raid-ready variety: any of the best few (same tier, within a few percent of the best score) by the bot's guid and
        // the slot. Stable for a given bot, so regear never churns.
        bool const resist_pick = resist_slot && best && best_score >= kResistPick;
        if (world_mix && best && scored.size() > 1)
        {
            uint64 h = ctx.bot_id * 0x9E3779B97F4A7C15ull ^ (uint64(slot) + 11) * 0xA24BAED4963EE407ull;
            h ^= h >> 29;
            h *= 0xBF58476D1CE4E5B9ull;
            h ^= h >> 32;
            std::ranges::sort(scored, [](Scored const& a, Scored const& b) {
                return a.combined != b.combined ? a.combined > b.combined : a.tpl->GetId() < b.tpl->GetId(); });
            // Slot by slot, each bot rolls on its own: about 45% wear a green or an ordinary blue (nothing below uncommon), the rest
            // one of the best sixteen pre-raid pieces within 40% of the top. Stable per bot and slot.
            bool const lower = best_score < 1100000 && (h % 100) < 45;
            std::vector<ItemTemplate const*> options;
            int32 ref = std::numeric_limits<int32>::min();
            for (Scored const& c : scored)
            {
                bool const prep = c.combined >= 500000;
                if (prep == lower) continue;
                if (lower && c.tpl->GetQuality() != ITEM_QUALITY_UNCOMMON && c.tpl->GetQuality() != ITEM_QUALITY_RARE) continue;
                if (ref == std::numeric_limits<int32>::min()) ref = c.combined;
                int32 const base = prep ? c.combined - 1000000 : c.combined;
                int32 const base_ref = prep ? ref - 1000000 : ref;
                if (options.size() >= (lower ? 14u : 16u) || base < base_ref - int32(std::abs(float(base_ref)) * (lower ? 0.60f : 0.40f))) break;
                options.push_back(c.tpl);
            }
            if (options.size() > 1)
                best = options[(h >> 12) % options.size()];
            else if (options.size() == 1)
                best = options[0];
        }
        else if (resist_pick)
            resist_held[slot] = true;   // the most resistance, no variety roll among the pieces
        else if ((raid || owned_lv) && best && ctx.bot_id && !bis_mode)
        {
            bool const best_prep = best_score >= 500000;
            int32 const floor_score = best_score - int32(std::abs(float(best_score - (best_prep ? 1000000 : 0))) * kRaidVarietyMargin);
            std::ranges::sort(scored, [](Scored const& a, Scored const& b) {
                return a.combined != b.combined ? a.combined > b.combined : a.tpl->GetId() < b.tpl->GetId(); });
            std::vector<ItemTemplate const*> top;
            for (Scored const& c : scored)
            {
                if (top.size() >= kRaidVarietyTop || c.combined < floor_score) break;
                top.push_back(c.tpl);
            }
            if (top.size() > 1)
            {
                uint64 h = ctx.bot_id * 0x9E3779B97F4A7C15ull ^ (uint64(slot) + 1) * 0xC2B2AE3D27D4EB4Full;
                h ^= h >> 29;
                h *= 0xBF58476D1CE4E5B9ull;
                h ^= h >> 32;
                best = top[h % top.size()];
            }
        }

        // Variety: bots of one class and level would otherwise all wear the single top-scoring item per slot and look
        // like clones. Any candidate about as good as the best (same quality, effective item level within a few
        // points, score within a small margin, so the same armour type and primary stat) may be picked instead, chosen
        // by the bot's guid and the slot. The same bot always gets the same pick, so backfill and regear never churn.
        if (!raid && !owned_lv && best && ctx.bot_id)
        {
            int32 const best_ilvl = ::Playerbot::Gear::EffectiveItemLevelForLevel(best, ctx.level);
            std::vector<ItemTemplate const*> peers;
            for (Scored const& c : scored)
                if (c.combined >= best_score - kVarietyScoreMargin && c.tpl->GetQuality() == best->GetQuality() &&
                    std::abs(c.ilvl - best_ilvl) <= kVarietyIlvlBand)
                    peers.push_back(c.tpl);
            if (peers.size() > 1)
            {
                std::ranges::sort(peers, {}, &ItemTemplate::GetId);
                uint64 h = ctx.bot_id * 0x9E3779B97F4A7C15ull ^ (uint64(slot) + 1) * 0xC2B2AE3D27D4EB4Full;
                h ^= h >> 29;
                h *= 0xBF58476D1CE4E5B9ull;
                h ^= h >> 32;
                best = peers[h % peers.size()];
            }
        }

        // Nothing distinct qualified. A matching pair is still better than an
        // empty slot, but only when the item may actually be worn twice - for a
        // unique-equipped piece the second equip would be refused and we would
        // have produced a pick that can never land.
        if (!best && twin && !twin->HasFlag(ITEM_FLAG_UNIQUE_EQUIPPABLE) && twin->GetMaxCount() != 1)
            best = twin;

        // A tier leg whose main hand stays empty fights with fists (damage_parity 147 shaman, t1: no weapon, 107 dps): say which filter emptied the pool.
        if (!best && tier_mode && slot == EQUIPMENT_SLOT_MAINHAND)
        {
            uint32 n_level = 0, n_skill = 0, n_wear = 0, n_ilvl = 0, n_fit = 0;
            for (auto const* tpl : candidates)
            {
                if (tpl->GetBaseRequiredLevel() > ctx.level) { ++n_level; continue; }
                if (ctx.wearer && ctx.wearer->CanUseItem(tpl) != EQUIP_ERR_OK) { ++n_wear; continue; }
                if (ctx.wearer && tpl->GetSkill() && !ctx.wearer->GetSkillValue(tpl->GetSkill())) { ++n_skill; continue; }
                if (::Playerbot::Gear::IsPvpItem(tpl) || ::Playerbot::Gear::EffectiveItemLevelForLevel(tpl, ctx.level) > raid_cap) { ++n_ilvl; continue; }
                if (::Playerbot::Gear::ScoreItemRaid(tpl, ctx.cls, ctx.spec, ctx.level, slot) <= -100000) ++n_fit;
            }
            TC_LOG_INFO("playerbot.v2", "[gear_gen] class {} spec {} level {}: no main hand under item level {}: {} candidates, {} above the level, {} refused by CanUseItem, {} without the skill, {} over the cap or PvP, {} refused by the stat weights",
                        uint32(ctx.cls), uint32(ctx.spec), uint32(ctx.level), raid_cap, candidates.size(), n_level, n_wear, n_skill, n_ilvl, n_fit);
        }

        // A tier leg's off hand that is not a raid-prep weapon (fury t1 wore Left-Handed Blades, item level 35, while one-handers of item level 76 exist):
        // count the prep weapons of the pool by the filter that removed each, so the next run names the cause.
        if (tier_mode && slot == EQUIPMENT_SLOT_OFFHAND && ctx.cls == CLASS_WARRIOR && (!best || best->GetQuality() < ITEM_QUALITY_RARE ||
            best->GetBaseRequiredLevel() < int32(kRaidMinRequiredLevel)))
        {
            uint32 n_prep = 0, n_level = 0, n_wear = 0, n_skill = 0, n_cap = 0, n_fit = 0, n_dual = 0, n_ok = 0;
            for (auto const* tpl : candidates)
            {
                if (tpl->GetClass() != ITEM_CLASS_WEAPON || tpl->GetQuality() < ITEM_QUALITY_RARE || tpl->GetBaseRequiredLevel() < int32(kRaidMinRequiredLevel)) continue;
                ++n_prep;
                if (tpl->GetBaseRequiredLevel() > ctx.level) { ++n_level; continue; }
                if (ctx.wearer && ctx.wearer->CanUseItem(tpl) != EQUIP_ERR_OK) { ++n_wear; continue; }
                if (ctx.wearer && tpl->GetSkill() && !ctx.wearer->GetSkillValue(tpl->GetSkill())) { ++n_skill; continue; }
                if (::Playerbot::Gear::IsPvpItem(tpl) || ::Playerbot::Gear::EffectiveItemLevelForLevel(tpl, ctx.level) > raid_cap) { ++n_cap; continue; }
                if (!can_dual_wield) { ++n_dual; continue; }
                if (::Playerbot::Gear::ScoreItemRaid(tpl, ctx.cls, ctx.spec, ctx.level, slot) <= -100000) { ++n_fit; continue; }
                ++n_ok;
            }
            TC_LOG_INFO("playerbot.v2", "[gear_gen] class {} spec {} level {}: off hand pick {} is not a prep weapon; pool has {} prep weapons ({} above the level, {} refused by CanUseItem, {} without the skill, {} over the cap or PvP, {} with no dual wield, {} refused by the stat weights, {} usable)",
                        uint32(ctx.cls), uint32(ctx.spec), uint32(ctx.level), best ? best->GetId() : 0u, n_prep, n_level, n_wear, n_skill, n_cap, n_dual, n_fit, n_ok);
        }

        if (best)
        {
            out.push_back({slot, best->GetId()});
            if (slot == EQUIPMENT_SLOT_FINGER1)  picked_finger1 = best;
            if (slot == EQUIPMENT_SLOT_TRINKET1) picked_trinket1 = best;
            if (slot == EQUIPMENT_SLOT_MAINHAND) picked_mainhand = best;
        }
        ++slot_idx;
    }

    if (tier_mode)
    {
        // The class tier set goes on over the raid picks: the slots it covers are its own, the rest keep the picks made under the tier's
        // item level ceiling above.
        // A class and spec with a sourced list (warrior dps, Tier 3) wears that list, slot by slot, instead of the tier set.
        auto const& sourced = SourcedTierGear(ctx.cls, ctx.spec, ctx.gear_mode);
        uint32 sourced_slots = 0;
        for (SourcedGearPick const& pick : sourced)
        {
            ItemTemplate const* tpl = sObjectMgr->GetItemTemplate(pick.entry);
            if (!tpl || resist_held[pick.slot] || (ctx.wearer && ctx.wearer->CanUseItem(tpl) != EQUIP_ERR_OK))
            {
                TC_LOG_INFO("playerbot.v2", "[gear] bot {} sourced pick {} for slot {} skipped: {}", ctx.bot_id, pick.entry, uint32(pick.slot),
                            !tpl ? "no such item in the item data" : resist_held[pick.slot] ? "slot held by resistance gear"
                                 : fmt::format("CanUseItem error {}", uint32(ctx.wearer->CanUseItem(tpl))));
                continue;
            }
            if (ctx.race)
            {
                if (!tpl->GetAllowableRace().HasRace(ctx.race)) continue;
                uint32 const team = ::Player::TeamForRace(ctx.race);
                if (tpl->HasFlag(ITEM_FLAG2_FACTION_HORDE) && team != HORDE) continue;
                if (tpl->HasFlag(ITEM_FLAG2_FACTION_ALLIANCE) && team != ALLIANCE) continue;
            }
            sourced_slots |= 1u << pick.slot;
            auto it = std::ranges::find(out, pick.slot, &GearItem::slot);
            if (it != out.end())
                it->item_entry = pick.entry;
            else
                out.push_back({ pick.slot, pick.entry });
        }
        // A two-hander in the main hand leaves no off hand.
        if (!sourced.empty())
            if (auto mh = std::ranges::find(out, uint8(EQUIPMENT_SLOT_MAINHAND), &GearItem::slot); mh != out.end())
                if (ItemTemplate const* mtpl = sObjectMgr->GetItemTemplate(mh->item_entry); mtpl && mtpl->GetInventoryType() == INVTYPE_2HWEAPON)
                    std::erase_if(out, [](GearItem const& g) { return g.slot == EQUIPMENT_SLOT_OFFHAND; });
        // The tier set fills the slots the sourced list did not set (a sourced entry the item data lacks leaves its slot to the set).
        for (uint32 const entry : !TierSetIsHealerSet(ctx.cls, ctx.spec) ? TierSetPieces(ctx.cls, Owned::TierNumber(ctx.gear_mode)) : std::vector<uint32>{})
        {
            ItemTemplate const* tpl = sObjectMgr->GetItemTemplate(entry);
            if (!tpl)
                continue;
            if (uint32 const allowable = tpl->GetAllowableClass(); allowable && allowable != uint32(-1) && !(allowable & (1u << (ctx.cls - 1))))
                continue;
            uint8 slot = 255;
            for (uint8 s : { uint8(EQUIPMENT_SLOT_HEAD), uint8(EQUIPMENT_SLOT_SHOULDERS), uint8(EQUIPMENT_SLOT_CHEST), uint8(EQUIPMENT_SLOT_WAIST),
                             uint8(EQUIPMENT_SLOT_LEGS), uint8(EQUIPMENT_SLOT_FEET), uint8(EQUIPMENT_SLOT_WRISTS), uint8(EQUIPMENT_SLOT_HANDS) })
                if (InventoryTypeFitsSlot(s, tpl->GetInventoryType())) { slot = s; break; }
            if (slot == 255 || resist_held[slot] || (sourced_slots & (1u << slot)))
                continue;
            if (ctx.wearer && ctx.wearer->CanUseItem(tpl) != EQUIP_ERR_OK)
                continue;
            if (ctx.race)
            {
                if (!tpl->GetAllowableRace().HasRace(ctx.race)) continue;
                uint32 const team = ::Player::TeamForRace(ctx.race);
                if (tpl->HasFlag(ITEM_FLAG2_FACTION_HORDE) && team != HORDE) continue;
                if (tpl->HasFlag(ITEM_FLAG2_FACTION_ALLIANCE) && team != ALLIANCE) continue;
            }
            auto it = std::ranges::find(out, slot, &GearItem::slot);
            if (it != out.end())
                it->item_entry = entry;
            else
                out.push_back({ slot, entry });
        }
    }
    // Best-in-slot candidates of the tier for the slots no tier set covers (BisPools.h): neck, back, rings, trinkets. A candidate goes on only when
    // the role-weighted raid score beats what the pick above holds there, under the tier's item level ceiling; unknown names are skipped.
    if (tier_mode)
    {
        int const tier = int(Owned::TierNumber(ctx.gear_mode));
        auto const role = ::Playerbot::Gear::Loadout::Role(uint8(::Playerbot::Gear::RaidRoleForBot(ctx.cls, ctx.spec)));
        auto const resolve = [](std::string_view name) -> uint32
        {
            static std::mutex lock;
            static std::unordered_map<std::string, uint32> cache;
            std::lock_guard g(lock);
            std::string const key(name);
            if (auto it = cache.find(key); it != cache.end())
                return it->second;
            uint32 found = 0;
            LocaleConstant const loc = sWorld->GetDefaultDbcLocale();
            for (auto const& [id, tpl] : sObjectMgr->GetItemTemplateStore())
            {
                char const* n = tpl.GetName(loc);
                if (n && name == n && (!found || tpl.GetBaseItemLevel() > sObjectMgr->GetItemTemplate(found)->GetBaseItemLevel()))
                    found = id;
            }
            if (!found)
                TC_LOG_INFO("playerbot.v2", "[bis] unresolved: no item named \"{}\" in the item data", name);
            cache.emplace(key, found);
            return found;
        };
        auto const score_of = [&](uint8 slot) -> int32
        {
            auto it = std::ranges::find(out, slot, &GearItem::slot);
            ItemTemplate const* t = it != out.end() ? sObjectMgr->GetItemTemplate(it->item_entry) : nullptr;
            return t ? ::Playerbot::Gear::ScoreItemRaid(t, ctx.cls, ctx.spec, ctx.level, slot) : 0;
        };
        struct KindSlots { ::Playerbot::Gear::Bis::Kind kind; std::array<uint8, 2> slots; };
        for (KindSlots const& ks : { KindSlots{ ::Playerbot::Gear::Bis::Kind::Neck, { EQUIPMENT_SLOT_NECK, 255 } }, KindSlots{ ::Playerbot::Gear::Bis::Kind::Back, { EQUIPMENT_SLOT_BACK, 255 } },
                                     KindSlots{ ::Playerbot::Gear::Bis::Kind::Ring, { EQUIPMENT_SLOT_FINGER1, EQUIPMENT_SLOT_FINGER2 } },
                                     KindSlots{ ::Playerbot::Gear::Bis::Kind::Trinket, { EQUIPMENT_SLOT_TRINKET1, EQUIPMENT_SLOT_TRINKET2 } },
                                     KindSlots{ ::Playerbot::Gear::Bis::Kind::Offhand, { EQUIPMENT_SLOT_OFFHAND, 255 } },
                                     KindSlots{ ::Playerbot::Gear::Bis::Kind::Ranged, { EQUIPMENT_SLOT_RANGED, 255 } } })
        {
            // No off hand next to a two-hand weapon.
            if (ks.kind == ::Playerbot::Gear::Bis::Kind::Offhand)
            {
                auto mh = std::ranges::find(out, uint8(EQUIPMENT_SLOT_MAINHAND), &GearItem::slot);
                ItemTemplate const* mt = mh != out.end() ? sObjectMgr->GetItemTemplate(mh->item_entry) : nullptr;
                if (mt && mt->GetInventoryType() == INVTYPE_2HWEAPON)
                    continue;
            }
            std::vector<uint32> taken;
            for (uint8 const slot : ks.slots)
            {
                if (slot == 255 || resist_held[slot])
                    continue;
                ItemTemplate const* best = nullptr;
                // A ring or trinket is unique: what the paired slot holds is not a candidate here (damage_parity 164: the same Band of Unnatural
                // Forces and Slayer's Crest were picked for both slots, and the second copy was refused with error 17).
                std::vector<uint32> in_pair = taken;
                for (uint8 const other : ks.slots)
                    if (other != 255 && other != slot)
                        if (auto held = std::ranges::find(out, other, &GearItem::slot); held != out.end())
                            in_pair.push_back(held->item_entry);
                auto cur_pick = std::ranges::find(out, slot, &GearItem::slot);
                bool const cur_dup = cur_pick != out.end() && std::ranges::find(in_pair, cur_pick->item_entry) != in_pair.end();
                int32 best_score = cur_dup ? std::numeric_limits<int32>::min() : score_of(slot);
                // The weapon slots log what the slot holds and why each pool candidate was refused or what it scored (174 kept item 4696 in the off
                // hand with three pool pieces on offer, and nothing said why).
                bool const log_slot = ks.kind == ::Playerbot::Gear::Bis::Kind::Offhand || ks.kind == ::Playerbot::Gear::Bis::Kind::Ranged;
                int32 const held_score = best_score;
                uint32 const held_item = cur_pick != out.end() ? cur_pick->item_entry : 0;
                auto const note = [&](std::string_view name, uint32 entry, std::string const& what)
                {
                    if (log_slot)
                        TC_LOG_INFO("playerbot.v2", "[bis] slot {} holds item {} (score {}): candidate \"{}\" ({}) {}", uint32(slot), held_item, held_score, name, entry, what);
                };
                for (::Playerbot::Gear::Bis::PoolEntry const& e : ::Playerbot::Gear::Bis::PoolFor(role))
                {
                    if (e.kind != ks.kind || !::Playerbot::Gear::Bis::InTier(e, tier))
                        continue;
                    uint32 const entry = resolve(e.name);
                    ItemTemplate const* tpl = entry ? sObjectMgr->GetItemTemplate(entry) : nullptr;
                    if (!tpl || std::ranges::find(in_pair, entry) != in_pair.end() || !InventoryTypeFitsSlot(slot, tpl->GetInventoryType()))
                    {
                        note(e.name, entry, !tpl ? "refused: no item template" : "refused: taken or does not fit the slot");
                        continue;
                    }
                    if (::Playerbot::Gear::EffectiveItemLevelForLevel(tpl, ctx.level) > raid_cap || ::Playerbot::Gear::IsPvpItem(tpl))
                    {
                        note(e.name, entry, "refused: over the item level cap or a PvP item");
                        continue;
                    }
                    if (ctx.wearer && ctx.wearer->CanUseItem(tpl) != EQUIP_ERR_OK)
                    {
                        note(e.name, entry, "refused: the wearer cannot use it");
                        continue;
                    }
                    if (ctx.race)
                    {
                        if (!tpl->GetAllowableRace().HasRace(ctx.race)) continue;
                        uint32 const team = ::Player::TeamForRace(ctx.race);
                        if (tpl->HasFlag(ITEM_FLAG2_FACTION_HORDE) && team != HORDE) continue;
                        if (tpl->HasFlag(ITEM_FLAG2_FACTION_ALLIANCE) && team != ALLIANCE) continue;
                    }
                    int32 const sc = ::Playerbot::Gear::ScoreItemRaid(tpl, ctx.cls, ctx.spec, ctx.level, slot);
                    note(e.name, entry, fmt::format("scores {}", sc));
                    if (sc > best_score)
                    {
                        best = tpl;
                        best_score = sc;
                    }
                }
                if (!best)
                {
                    if (cur_dup)
                        out.erase(cur_pick);   // a second copy of a unique piece cannot be worn: the slot goes without a pick
                    continue;
                }
                taken.push_back(best->GetId());
                if (auto it = std::ranges::find(out, slot, &GearItem::slot); it != out.end())
                    it->item_entry = best->GetId();
                else
                    out.push_back({ slot, best->GetId() });
            }
        }
    }
    // A piece the raid wears for the fight whatever it scores (ctx.wear_entry: the Onyxia Scale Cloak against Shadow Flame): over the pick,
    // the tier piece or the resistance piece in its slot.
    if (raid && !ctx.world_bot() && ctx.wear_entry)
    {
        ItemTemplate const* tpl = sObjectMgr->GetItemTemplate(ctx.wear_entry);
        uint8 slot = 255;
        if (tpl)
            for (uint8 s : kSlots)
                if (InventoryTypeFitsSlot(s, tpl->GetInventoryType())) { slot = s; break; }
        if (tpl && slot != 255 && (!ctx.wearer || ctx.wearer->CanUseItem(tpl) == EQUIP_ERR_OK))
        {
            auto it = std::ranges::find(out, slot, &GearItem::slot);
            if (it != out.end())
                it->item_entry = ctx.wear_entry;
            else
                out.push_back({ slot, ctx.wear_entry });
        }
        else
            TC_LOG_INFO("playerbot.v2", "[gear] bot {} cannot wear the fight's piece {}: {}", ctx.bot_id, ctx.wear_entry,
                        !tpl ? "no such item" : slot == 255 ? "no equipment slot takes it" : "CanUseItem refuses it");
    }
    return out;
}

namespace {
std::string NormalizeSetName(std::string_view s)
{
    std::string r;
    for (char ch : s)
        if (std::isalnum(static_cast<unsigned char>(ch)))
            r += char(std::tolower(static_cast<unsigned char>(ch)));
    return r;
}
}

std::vector<uint32> const& TierSetPieces(uint8 cls, int32 tier)
{
    static std::once_flag once;
    static std::array<std::array<std::vector<uint32>, 4>, 12> cache;   // [class][tier]
    static std::vector<uint32> const empty;
    std::call_once(once, []
    {
        std::unordered_map<std::string, ItemSetEntry const*> by_name;
        for (ItemSetEntry const* set : sItemSetStore)
            if (set)
                by_name.try_emplace(NormalizeSetName(set->Name[DEFAULT_LOCALE]), set);
        uint32 found = 0;
        for (uint8 c = 1; c < 12; ++c)
            for (int32 t = 1; t <= 3; ++t)
            {
                std::string_view const name = Owned::TierSetName(c, t);
                if (name.empty())
                    continue;
                auto it = by_name.find(NormalizeSetName(name));
                if (it == by_name.end())
                {
                    TC_LOG_WARN("playerbot.v2", "[OwnedGear] tier {} set '{}' (class {}) not found in ItemSet.db2: that tier mode picks raid gear only", t, name, uint32(c));
                    continue;
                }
                for (uint32 e : it->second->ItemID)
                    if (e && sObjectMgr->GetItemTemplate(e))
                        cache[c][t].push_back(e);
                found += !cache[c][t].empty();
                TC_LOG_INFO("playerbot.v2", "[OwnedGear] tier {} set '{}' (class {}): {} piece(s)", t, name, uint32(c), cache[c][t].size());
            }
        TC_LOG_INFO("playerbot.v2", "[OwnedGear] {} tier set(s) resolved", found);
    });
    return cls >= 1 && cls < 12 && tier >= 1 && tier <= 3 ? cache[cls][tier] : empty;
}

// Phase 6 (Naxxramas) best in slot for a dps warrior, Icy Veins "Warrior DPS Season of Mastery Phase 6 Gear":
// https://www.icy-veins.com/wow-classic/warrior-dps-season-of-mastery-phase-6-gear (one list for fury and arms). The page names Lionheart Helm,
// Stormrage's Talisman of Seething, Conqueror's Spaulders, Shroud of Dominion, Plated Abomination Ribcage, Wristguards of Vengeance,
// Gauntlets of Annihilation, Girdle of the Mentor, Legplates of Carnage, Chromatic Boots, Band of Unnatural Forces, Don Julio's Band,
// Drake Fang Talisman, Slayer's Crest (Diamond Flask and Kiss of the Spider are the other trinkets it lists), Soulstring; weapons: The Hungering
// Cold and Gressil, Dawn of Ruin for dual wield (fury), Might of Menethil for two-hand (arms).
// The client item data (ItemSparse.db2, decoded) holds only the entries below. Plated Abomination Ribcage (23000), Wristguards of Vengeance
// (22936), Girdle of the Mentor (23219), Legplates of Carnage (23068), Chromatic Boots (19387), Shroud of Dominion (23045), Soulstring (22811),
// The Hungering Cold (23577), Gressil, Dawn of Ruin (23054) and Might of Menethil (22798) do not exist in it. Those slots are left out here:
// the Dreadnaught set (items 22416 to 22423, all present) fills chest, wrists, waist, legs and feet, and back, ranged and weapons keep
// the role-scored pick (BisPools, ScoreItemRaid) under the item level ceiling.
std::vector<SourcedGearPick> const& SourcedTierGear(uint8 cls, uint16 spec, Owned::GearMode mode)
{
    static std::vector<SourcedGearPick> const none;
    static std::vector<SourcedGearPick> const common = {
        { EQUIPMENT_SLOT_HEAD, 12640 }, { EQUIPMENT_SLOT_NECK, 23053 }, { EQUIPMENT_SLOT_SHOULDERS, 21330 }, { EQUIPMENT_SLOT_HANDS, 21581 },
        { EQUIPMENT_SLOT_FINGER1, 23038 }, { EQUIPMENT_SLOT_FINGER2, 19325 }, { EQUIPMENT_SLOT_TRINKET1, 19406 }, { EQUIPMENT_SLOT_TRINKET2, 23041 } };
    // Fury dual wields. The weapons of the list are not in the client data; the one-handers that are and carry the most attack power are Ancient
    // Qiraji Ripper 21650 (sword, 2.8 s, 91 attack power, 64 crit rating) and Death's Sting 21126 (dagger, 1.8 s, 158 attack power). The off hand
    // they replace was Left-Handed Blades 15909, item level 35 (itemsparse-decoded.tsv).
    static std::vector<SourcedGearPick> const fury = [] { auto v = common; v.push_back({ EQUIPMENT_SLOT_MAINHAND, 21650 }); v.push_back({ EQUIPMENT_SLOT_OFFHAND, 21126 }); return v; }();
    if (cls != CLASS_WARRIOR || mode != Owned::GearMode::T3 || spec == 73)
        return none;
    return spec == 71 ? common : fury;
}

// The hybrids' tier sets (Cenarion / Stormrage / Dreamwalker, Lawbringer / Judgement / Redemption, Prophecy / Transcendence / Faith, Earthfury /
// Ten Storms / Earthshatterer) are built for the healer. A damage spec of those classes (balance and cat druid, retribution paladin, shadow priest,
// elemental and enhancement shaman) wears the picks of its own role under the tier's item level ceiling instead: damage_parity 147 gave a balance
// druid 168 spell power at Tier 3 against 268 at Tier 1.
bool TierSetIsHealerSet(uint8 cls, uint16 spec)
{
    switch (cls)
    {
        case CLASS_DRUID:   return spec == 102 || spec == 103;
        case CLASS_PALADIN: return spec == 70;
        case CLASS_PRIEST:  return spec == 258;
        case CLASS_SHAMAN:  return spec == 262 || spec == 263;
        default:            return false;
    }
}

bool IsTierSetPick(uint8 cls, Owned::GearMode mode, uint32 entry, uint16 spec)
{
    if (!Owned::IsTierMode(mode))
        return false;
    if (TierSetIsHealerSet(cls, spec))
        return false;
    for (SourcedGearPick const& pick : SourcedTierGear(cls, spec, mode))
        if (pick.entry == entry)
            return true;
    auto const& pieces = TierSetPieces(cls, Owned::TierNumber(mode));
    return std::ranges::find(pieces, entry) != pieces.end();
}

} // namespace Playerbot::V2::Gear
