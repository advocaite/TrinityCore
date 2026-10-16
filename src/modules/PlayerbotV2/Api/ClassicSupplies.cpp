#include "ClassicSupplies.h"
#include "DB2Stores.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <array>

namespace Playerbot::Classic {

namespace {

// ---- Shaman: the four totem tools (given by the level 4/10/20/30 class quests) ----
constexpr SupplyRank kEarthTotem[] = { { 5175, 4 } };
constexpr SupplyRank kFireTotem[]  = { { 5176, 10 } };
constexpr SupplyRank kWaterTotem[] = { { 5177, 20 } };
constexpr SupplyRank kAirTotem[]   = { { 5178, 30 } };
constexpr SupplyRank kAnkh[]       = { { 17030, 30 } };   // Reincarnation

// ---- Rogue ----
constexpr SupplyRank kInstantPoison[] = {
    { 6947, 20 }, { 6949, 28 }, { 6950, 36 }, { 8926, 44 }, { 8927, 52 }, { 8928, 60 },
};
constexpr SupplyRank kDeadlyPoison[] = {
    { 2892, 30 }, { 2893, 38 }, { 8984, 46 }, { 8985, 54 }, { 20844, 60 },
};
constexpr SupplyRank kFlashPowder[] = { { 5140, 22 } };   // Vanish

// ---- Paladin ----
constexpr SupplyRank kSymbolOfKings[] = { { 21177, 52 } };   // Greater Blessings

// ---- Priest ----
constexpr SupplyRank kPrayerCandle[] = { { 17028, 48 }, { 17029, 56 } };   // Holy / Sacred Candle

// ---- Mage ----
constexpr SupplyRank kArcanePowder[] = { { 17020, 56 } };   // Arcane Brilliance

// ---- Druid ----
constexpr SupplyRank kGiftReagent[]   = { { 17021, 50 }, { 17026, 60 } };   // Wild Berries / Wild Thornroot
constexpr SupplyRank kRebirthReagent[] = {
    { 17034, 20 }, { 17035, 30 }, { 17036, 40 }, { 17037, 50 }, { 17038, 60 },
};

// ---- Warlock ----
constexpr SupplyRank kSoulShard[] = { { 6265, 10 } };

// ---- Every class: vendor food and water, potions, bandages (1.12 ids) ----
// Rank = the item's required level (bandages: the bot's First Aid at that
// level, see FirstAidSkillForLevel).
constexpr SupplyRank kFood[] = {
    { 4540, 1 }, { 4541, 5 }, { 4542, 15 }, { 4544, 25 }, { 4601, 35 }, { 8950, 45 },
};   // Tough Hunk of Bread .. Homemade Cherry Pie
constexpr SupplyRank kWater[] = {
    { 159, 1 }, { 1179, 5 }, { 1205, 15 }, { 1708, 25 }, { 1645, 35 }, { 8766, 45 },
};   // Refreshing Spring Water .. Morning Glory Dew
constexpr SupplyRank kHealingPotion[] = {
    { 118, 1 }, { 858, 3 }, { 929, 12 }, { 1710, 21 }, { 3928, 35 }, { 13446, 45 },
};   // Minor .. Major Healing Potion
constexpr SupplyRank kManaPotion[] = {
    { 2455, 5 }, { 3385, 14 }, { 3827, 22 }, { 6149, 31 }, { 13443, 41 }, { 13444, 49 },
};   // Minor .. Major Mana Potion
constexpr SupplyRank kDemonicRune[] = { { 12662, 55 } };   // raid-level mana users, next to the potion
constexpr SupplyRank kBandage[] = {
    { 1251, 1 }, { 2581, 8 }, { 3530, 14 }, { 3531, 20 }, { 6450, 26 },
    { 6451, 32 }, { 8544, 38 }, { 8545, 44 }, { 14529, 50 }, { 14530, 56 },
};   // Linen .. Heavy Runecloth Bandage (First Aid 1 .. 225)

constexpr uint8 kConsumable = SUPPLY_ANY_CLASS | SUPPLY_ALL_RANKS;

constexpr SupplyFamily kFamilies[] = {
    { "Earth Totem",     CLASS_SHAMAN,  1,  kEarthTotem },
    { "Fire Totem",      CLASS_SHAMAN,  1,  kFireTotem },
    { "Water Totem",     CLASS_SHAMAN,  1,  kWaterTotem },
    { "Air Totem",       CLASS_SHAMAN,  1,  kAirTotem },
    { "Ankh",            CLASS_SHAMAN,  5,  kAnkh },
    { "Instant Poison",  CLASS_ROGUE,   10, kInstantPoison },
    { "Deadly Poison",   CLASS_ROGUE,   10, kDeadlyPoison },
    { "Flash Powder",    CLASS_ROGUE,   10, kFlashPowder },
    { "Symbol of Kings", CLASS_PALADIN, 40, kSymbolOfKings },
    { "Prayer Candle",   CLASS_PRIEST,  20, kPrayerCandle },
    { "Arcane Powder",   CLASS_MAGE,    20, kArcanePowder },
    { "Gift Reagent",    CLASS_DRUID,   20, kGiftReagent },
    { "Rebirth Reagent", CLASS_DRUID,   5,  kRebirthReagent },
    { "Soul Shard",      CLASS_WARLOCK, 5,  kSoulShard },
    // Open-world bots keep a small stock; raid-level (55+) bots carry what a
    // raid night needs (the owner: full potion stacks, 20 food, 40 drinks).
    { "Food",            0, 5,  kFood,          kConsumable,                     20 },
    { "Water",           0, 10, kWater,         kConsumable | SUPPLY_MANA_USERS, 40 },
    { "Healing Potion",  0, 2,  kHealingPotion, kConsumable,                     5 },
    { "Mana Potion",     0, 2,  kManaPotion,    kConsumable | SUPPLY_MANA_USERS, 5 },
    { "Demonic Rune",    0, 5,  kDemonicRune,   kConsumable | SUPPLY_MANA_USERS },
    { "Bandage",         0, 5,  kBandage,       kConsumable | SUPPLY_FIRST_AID,  10 },
};

// ---- Raid consumables (1.12 ids, the 1.60 client data keeps them; an id the server has no template for is skipped) ----
// Flask of the Titans 13510, Flask of Supreme Power 13512, Flask of Distilled Wisdom 13511, Elixir of Fortitude 3825,
// Elixir of Superior Defense 13445, Elixir of the Mongoose 13452, Elixir of the Giants 9206, Greater Arcane Elixir 13454,
// Mageblood Potion 20007, Smoked Desert Dumplings 20452, Grilled Squid 13928, Runn Tum Tuber Surprise 18254,
// Nightfin Soup 13931, Tender Wolf Steak 18045.
constexpr RaidConsumable kTankKit[] = {
    { 13510, false, 2, true }, { 13445, false, 2 }, { 3825, false, 2 }, { 18045, true, 5 },
    { 13457, false, 2, false, true },   // Greater Fire Protection Potion: drunk in the fight where the boss data orders it (Ragnaros)
};
constexpr RaidConsumable kMeleeKit[] = {
    { 13452, false, 2 }, { 9206, false, 2 }, { 20452, true, 5 },
    { 13457, false, 1, false, true },   // Greater Fire Protection Potion: drunk in the fight where the boss data orders it (Ragnaros' Might hits 20 y round a random mana user)
};
constexpr RaidConsumable kCasterKit[] = {
    { 13512, false, 2, true }, { 13454, false, 2 }, { 18254, true, 5 },
    { 13457, false, 1, false, true },   // Greater Fire Protection Potion: drunk in the fight where the boss data orders it (Ragnaros' Might hits 20 y round a random mana user)
};
constexpr RaidConsumable kHealerKit[] = {
    { 13511, false, 2, true }, { 20007, false, 2 }, { 13931, true, 5 },
    { 13457, false, 1, false, true },   // Greater Fire Protection Potion: drunk in the fight where the boss data orders it (Ragnaros' Might hits 20 y round a random mana user)
};
constexpr RaidConsumable kHunterKit[] = {
    { 13452, false, 2 }, { 9206, false, 2 }, { 13928, true, 5 },
    { 13457, false, 1, false, true },   // Greater Fire Protection Potion: drunk in the fight where the boss data orders it (Ragnaros' Might hits 20 y round a random mana user)
};

} // namespace

std::span<RaidConsumable const> RaidConsumablesFor(RaidConsumableRole role)
{
    switch (role)
    {
        case RaidConsumableRole::Tank:   return kTankKit;
        case RaidConsumableRole::Melee:  return kMeleeKit;
        case RaidConsumableRole::Caster: return kCasterKit;
        case RaidConsumableRole::Healer: return kHealerKit;
        case RaidConsumableRole::Hunter: return kHunterKit;
    }
    return {};
}

uint8 RaidConsumableStock(uint32 item)
{
    for (std::span<RaidConsumable const> kit : { std::span<RaidConsumable const>(kTankKit), std::span<RaidConsumable const>(kMeleeKit),
                                                 std::span<RaidConsumable const>(kCasterKit), std::span<RaidConsumable const>(kHealerKit),
                                                 std::span<RaidConsumable const>(kHunterKit) })
        for (RaidConsumable const& c : kit)
            if (c.item == item)
                return c.stock;
    return 0;
}

bool IsRaidConsumable(uint32 item)
{
    return RaidConsumableStock(item) != 0;
}

std::span<uint32 const> GroupBuffSpells(uint8 cls)
{
    // 21562 / 21564 Prayer of Fortitude r1 / r2, 27683 Prayer of Shadow Protection, 23028 Arcane Brilliance, 21849 / 21850 Gift of the Wild r1 / r2.
    static constexpr uint32 kPriest[] = { 21562, 21564, 27683 };
    static constexpr uint32 kMage[]   = { 23028 };
    static constexpr uint32 kDruid[]  = { 21849, 21850 };
    switch (cls)
    {
        case CLASS_PRIEST: return kPriest;
        case CLASS_MAGE:   return kMage;
        case CLASS_DRUID:  return kDruid;
        default:           return {};
    }
}

bool LearnableGroupBuff(uint32 spell, uint8 level)
{
    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell, DIFFICULTY_NONE);
    if (!si || si->SpellLevel > level)
        return false;
    for (SpellEffectInfo const& e : si->GetEffects())
        if (e.IsEffect() && e.IsAura())
            return true;
    return false;
}

std::span<SupplyFamily const> GetSupplyFamilies()
{
    return kFamilies;
}

SupplyFamily const* FindSupplyFamily(std::string_view name)
{
    for (SupplyFamily const& f : kFamilies)
        if (name == f.name)
            return &f;
    return nullptr;
}

bool SupplyFamilyForClass(SupplyFamily const& family, uint8 cls)
{
    if (!(family.flags & SUPPLY_ANY_CLASS))
        return family.cls == cls;
    if (family.flags & SUPPLY_MANA_USERS)
    {
        // Vanilla mana users. Mages conjure their own runes: mana gems.
        switch (cls)
        {
            case CLASS_PALADIN: case CLASS_HUNTER: case CLASS_PRIEST: case CLASS_SHAMAN:
            case CLASS_WARLOCK: case CLASS_DRUID:
                return true;
            case CLASS_MAGE:
                return std::string_view(family.name) != "Demonic Rune";
            default:
                return false;
        }
    }
    return true;
}

uint16 SupplyTarget(SupplyFamily const& family, uint8 level)
{
    return level >= 55 && family.target_endgame ? family.target_endgame : family.target;
}

uint16 FirstAidMaxForLevel(uint8 level)
{
    // Apprentice 75, Journeyman 150 (level 10), Expert 225 (level 20),
    // Artisan 300 (level 35: the Triage quest).
    return level >= 35 ? 300 : level >= 20 ? 225 : level >= 10 ? 150 : 75;
}

uint16 FirstAidSkillForLevel(uint8 level)
{
    return std::min<uint16>(uint16(level) * 5u, FirstAidMaxForLevel(level));
}

uint32 BestSupplyRank(SupplyFamily const& family, uint8 level)
{
    for (auto it = family.ranks.rbegin(); it != family.ranks.rend(); ++it)
        if (it->min_level <= level && sObjectMgr->GetItemTemplate(it->item))
            return it->item;
    return 0;
}

uint32 BestAmmo(uint8 kind, uint8 level)
{
    if (kind != 1 && kind != 2)
        return 0;
    // [kind - 1][level]: computed once from the template store (item templates are
    // read-only after load), so the snapshot builder / rules may call it from any thread.
    static std::array<std::array<uint32, 61>, 2> const table = []
    {
        std::array<std::array<uint32, 61>, 2> t{};
        for (uint8 k = 1; k <= 2; ++k)
        {
            uint32 const subclass = k == 2 ? ITEM_SUBCLASS_BULLET : ITEM_SUBCLASS_ARROW;
            for (uint8 lvl = 1; lvl <= 60; ++lvl)
            {
                // First pass: Classic-era (expansion 0) vendor-grade ammo; second pass drops the era filter
                // in case the item data does not tag it.
                for (int strict = 1; strict >= 0 && !t[k - 1][lvl]; --strict)
                {
                    float bestDmg = -1.0f;
                    uint32 bestIlvl = 0;
                    int32 bestReq = -1;
                    for (auto const& [id, proto] : sObjectMgr->GetItemTemplateStore())
                    {
                        if (proto.GetClass() != ITEM_CLASS_PROJECTILE || proto.GetSubClass() != subclass)
                            continue;
                        if (proto.GetBaseRequiredLevel() > int32(lvl) || proto.GetQuality() > ITEM_QUALITY_UNCOMMON)
                            continue;
                        if (proto.GetRequiredSkill() || proto.GetRequiredReputationFaction() || proto.GetStartQuest() || proto.GetRequiredSpell())
                            continue;
                        if (strict && proto.GetRequiredExpansion() != 0)
                            continue;
                        ItemDamageAmmoEntry const* dmgRow = sItemDamageAmmoStore.LookupEntry(proto.GetBaseItemLevel());
                        float const dmg = dmgRow ? dmgRow->Quality[proto.GetQuality()] : 0.0f;
                        int32 const req = proto.GetBaseRequiredLevel();
                        if (dmg > bestDmg
                            || (dmg == bestDmg && proto.GetBaseItemLevel() > bestIlvl)
                            || (dmg == bestDmg && proto.GetBaseItemLevel() == bestIlvl && req > bestReq))
                        {
                            bestDmg = dmg;
                            bestIlvl = proto.GetBaseItemLevel();
                            bestReq = req;
                            t[k - 1][lvl] = id;
                        }
                    }
                }
            }
        }
        return t;
    }();
    return table[kind - 1][std::min<uint8>(level, 60)];
}

} // namespace Playerbot::Classic
