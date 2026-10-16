// BotGearGenerator - Pure function: given (level, class, spec), returns a list
// of equipment-slot/item-entry pairs that gear the bot to the bracket's
// expected ilvl. Phase B3 of WORLD_POPULATION_PLAN.md.
//
// Algorithm:
//   1. Compute target ilvl from level (linear ramp, capped at max-level ilvl).
//   2. For each equipment slot:
//      - Filter ItemTemplate by RequiredLevel <= bot.level, ItemLevel ~= target,
//        AllowableClass match, ArmorType match (cloth/leather/mail/plate per class),
//        InventoryType match (slot fits), Bonding != BoP-raid-only.
//      - Pick deterministic from filtered list via bot-id hash.
//   3. Return slot/entry tuples.
//
// Cached at module init: per (class, level_bracket, slot) candidate pools so
// per-bot lookups are O(1).

#pragma once
#include "../../Fleet/BotPool.h"
#include "OwnedGearPolicy.h"

#include "Bot/BotTypes.h"
#include <string>
#include <vector>

class Player;
struct ItemTemplate;
namespace Playerbot { struct BotSnapshot; }

namespace Playerbot::V2::Gear {

struct GearItem
{
    uint8  slot;        // EquipmentSlots::EQUIPMENT_SLOT_*
    uint32 item_entry;  // ItemTemplate.entry
};

struct GearGenerationContext
{
    uint8  level;
    uint8  cls;
    uint16 spec;
    uint64 bot_id;      // for deterministic selection
    // Race matters because Player::CanUseItem hard-rejects an item whose
    // AllowableRace excludes the wearer, or whose faction flag belongs to the
    // other side, with EQUIP_ERR_CANT_EQUIP_EVER. The candidate pool is built
    // per CLASS and cannot express either, so the check has to happen at pick
    // time. Leave 0 to skip the race/faction gate (callers that genuinely have
    // no race, e.g. pre-creation planning).
    uint8  race = 0;
    // The bot itself, when there is one: Classic weapon skills and plate/mail are trained, so a class-legal pick can
    // still be refused with EQUIP_ERR_PROFICIENCY_NEEDED (a level 60 warrior handed a two-handed mace it never learned).
    // Picks are limited to what Player::CanUseItem accepts and whose skill it has. Null = no gate (planning).
    Player const* wearer = nullptr;
    // A world / ambient bot (no owner): capped at Playerbot.World.Gear.MaxTier and mostly blue (Playerbot.World.Gear.EpicPercent),
    // like the players of the era. Owned and raid bots keep Playerbot.Gear.MaxTier.
    ::Playerbot::Pool::BotRole role = ::Playerbot::Pool::BotRole::Owned;
    // A level-cap companion's own effective item level ceiling (0 = the configured Playerbot.Gear.MaxTier / RaidIlvlCap). Set by the
    // caller from the owner's equipment, see Owned::OwnerBasedCap. Ignored for world bots.
    int32  ilvl_cap = 0;
    // What the owner asked for (Owned::GearMode, already reduced by Owned::EffectiveMode): at the cap T1 / T2 / T3 put the class tier set on
    // and raise the ceiling to that tier for the other slots; Best ignores the owner's own gear level. Ignored for world bots.
    Owned::GearMode gear_mode = Owned::GearMode::Match;
    bool   world_bot() const { return ::Playerbot::Pool::UsesWorldGear(role); }
};

// Owned-bot policy (OwnedGearPolicy.h): below the cap a companion's pick is the role-weighted best green or blue within
// Playerbot.Gear.Owned.IlvlBand item levels of its level; at the cap its ceiling follows the owner (Playerbot.Gear.Owned.MatchOwner).
// Off (Playerbot.Gear.Owned.Enable = 0): the level-scaled generic pick.
bool OwnedLevelGearActive(uint8 level, ::Playerbot::Pool::BotRole role);
bool OwnedMatchOwnerActive();
// Playerbot.OwnedGear.AllowedModes: the gear modes the server lets owners choose (Owned::ParseAllowedModes; default every mode).
uint32 OwnedAllowedModes();
int32 OwnedOwnerSlack();
// The configured ceiling a level-cap companion would have without the owner's gear (Playerbot.Gear.MaxTier / RaidIlvlCap).
int32 OwnedConfiguredCap();
// Role score of an item for a below-cap companion, quality adjusted (Owned::QualityAdjusted); -100000 when unwearable for the role.
int32 OwnedSlotScore(ItemTemplate const* tpl, uint8 cls, uint16 spec, uint8 level, uint8 slot);

// The item entries of a class's tier set (ItemSet.db2, found by name, see Owned::TierSetName), tier 1..3; empty when the data has none.
std::vector<uint32> const& TierSetPieces(uint8 cls, int32 tier);
// Whether `entry` is a piece of the tier set the mode names for the class.
bool IsTierSetPick(uint8 cls, Owned::GearMode mode, uint32 entry);

// Variety: a pick may be any candidate of the best's quality whose effective item level is within this many points of
// the best's (and that scores about as well), chosen by bot_id and slot. Two items of one quality this close are
// peers, so swapping one for the other is never a downgrade (BotPopulationManager::RerollCompanionGear relies on it).
constexpr int32 kVarietyIlvlBand = 4;

// Raid-ready gear (Playerbot.Behavior.RaidGear): a bot at the level cap gets pre-raid best-in-slot style pieces picked by
// role stat weights (Playerbot::Gear::ScoreItemRaid) instead of the level-scaled generic pick. Candidates are rare and
// epic items of required level 50+ and effective item level up to kRaidIlvlCap (Dire Maul, BRS, Scholomance, Stratholme,
// UBRS and the first Molten Core tier; never AQ / Naxxramas), never PvP items; a slot with none falls back to anything
// the wearer may use. Classic only (the stat model is vanilla's).
constexpr uint32 kRaidMinRequiredLevel = 50;
constexpr int32  kRaidIlvlCap          = 71;   // legacy default; the live cap is Playerbot.Gear.MaxTier / RaidIlvlCap
// A regear replaces a worn piece only when the pick scores at least this much higher (never a downgrade).
constexpr float  kRaidUpgradeRatio     = 1.10f;
// Per-bot variety: any of the best kRaidVarietyTop candidates within kRaidVarietyMargin of the best score may be picked.
constexpr uint32 kRaidVarietyTop       = 4;
constexpr float  kRaidVarietyMargin    = 0.07f;
bool RaidGearActive(uint8 level);
// The effective item level ceiling raid gear may reach (Playerbot.Gear.MaxTier / RaidIlvlCap, read once at first use).
int32 LiveRaidIlvlCap();
// The same ceiling for world bots (Playerbot.World.Gear.MaxTier, default 1 = item level 76).
int32 LiveWorldIlvlCap();
// A world bot's piece that is above its limits: effective item level over the world cap, or an epic in a slot this bot's share
// of epics (Playerbot.World.Gear.EpicPercent, by bot id and slot) does not cover. Such a piece is replaced by the regear pass.
bool WorldGearOver(ItemTemplate const* tpl, uint8 level, uint64 bot_id, uint8 slot);
// True for a world bot that wears a PvP rank set on its six armour slots (decided per bot).
bool WorldPvpSet(uint64 bot_id);

// The owner's "equip" orders, planned from the bot's published snapshot (thread safe, no Player): `arg` is the order text
// after "equip" (already lower case): empty / "upgrades" / "best" / "best gear" = every bag item that is an upgrade (the
// raid role score and its 10% rule at the level cap, the older stat-weight rule below it); an item link
// (|Hitem:ID:...|h[name]|h) or a name = that bag item, worn if usable whatever its score (PvP items refused).
// Fills `picks` with the equip swaps to emit and returns the whisper text ("Name: equipped [x] (+ilvl 8), ..." /
// "Name: nothing better in my bags").
struct EquipOrderPick { uint8 bag, slot, to_slot; uint32 entry; };
std::string PlanEquipOrder(::Playerbot::BotSnapshot const& snap, std::string const& bot_name, std::string const& arg, std::vector<EquipOrderPick>& picks);

// Bag-item upgrade test of the snapshot-side auto-equip rules at the level cap (RaidGearActive), the same decision
// RaidRegearPass makes: the bagged item must be wearable PvE gear (no PvP item, effective item level <= kRaidIlvlCap) and
// score kRaidUpgradeRatio above the worn piece in `slot` (ScoreItemRaid), a two-hander against main hand + off hand
// together, and an off hand never next to a worn two-hander. cur_* are the worn entries (0 = empty). Touches no Player:
// item templates and DB2 only, so it can run on the snapshot build / bot worker threads.
bool RaidEquipUpgrade(uint8 cls, uint16 spec, uint8 level, uint8 slot, uint32 bag_entry,
                      uint32 cur_entry, uint32 cur_mainhand_entry, uint32 cur_offhand_entry,
                      bool world_bot = false, uint64 bot_id = 0);

// Build per-class candidate pools at module init. Idempotent. Walks
// sObjectMgr->GetItemTemplateStore() exactly once.
void Initialize();

// Pure-function gear generator. Returns one entry per equipment slot the
// bot can fill given its class. Empty vector if generator not initialized
// or no candidates exist.
std::vector<GearItem> GenerateGearFor(GearGenerationContext const& ctx);

// Warrior and rogue (vanilla's only melee classes with a ranged attack; a paladin has no ranged item) wear a thrown
// weapon in the ranged slot (Playerbot.Behavior.TankRangedWeapon): thrown is its own ammo, a bow or gun needs ammo
// nothing restocks. GenerateGearFor then offers only thrown weapons for that slot.
bool UsesThrownRanged(uint8 cls);

// The best thrown weapon (highest required level, then quality, then effective item level) the wearer may use at its
// level, 0 when the pool has none. Classic-era items only (the pools exclude everything else).
uint32 PickThrownWeapon(Player const* wearer);

// Whether a bot may wear this item at all. Classic 1.60 (Forever) also ships Season of Discovery items (ids from
// ~208000, rune set bonuses); bots there wear vanilla's own items, which all sit below 25000, and no honor-rank rewards.
bool IsBotWearableItemId(uint32 entry);

// Approximate ilvl target for a character level (linear L1=1 -> L80=600,
// then L80=600 -> L90=700; clamped at the realm MaxPlayerLevel).
// Exposed so the hygiene re-gear pass can detect under-geared bots (avg
// equipped effective ilvl well below this target).
uint16 TargetIlvlForLevel(uint8 level);

} // namespace Playerbot::V2::Gear
