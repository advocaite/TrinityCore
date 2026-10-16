// ClassicSupplies - vanilla class tools and reagents for bots on Classic 1.60 (Forever).
//
// Vanilla spells need items a bot never collects on its own: shaman totems
// (class quests), rogue poisons (crafted), Symbol of Kings for Greater
// Blessings, candles / powder / berries for the group buffs, Rebirth seeds,
// Ankhs, Flash Powder, soul shards. Every class is also kept in a small stock
// of vendor food, water (mana users), healing and mana potions and bandages,
// as Karrz's factory does (PlayerbotFactory::InitFood / InitPotions). ike3's playerbots hand these to bots
// (PlayerbotFactory::InitReagents); PlayerbotV2 does the same through
// PlayerbotAPI::restock_class_supplies when PlayerbotV2.Classic.ClassSupplies
// is on.
//
// Each family is a rank list in ascending order; a bot carries the highest
// rank its level allows. Item ids are the 1.12 ids, which the 1.60 client
// data keeps. An id the server has no template for is skipped.

#pragma once

#include "Define.h"
#include <span>
#include <string_view>

namespace Playerbot::Classic {

struct SupplyRank
{
    uint32 item;
    uint8 min_level;
};

// SupplyFamily::flags.
enum SupplyFlags : uint8
{
    SUPPLY_ANY_CLASS     = 0x01,   // every class (cls is 0): food, potions, bandages
    SUPPLY_MANA_USERS    = 0x02,   // only classes that use mana (drinks, mana potions)
    SUPPLY_FIRST_AID     = 0x04,   // needs First Aid (bandages); the restock teaches it
    SUPPLY_ALL_RANKS     = 0x08,   // any rank in the bags counts towards the target
};

struct SupplyFamily
{
    char const* name;
    uint8 cls;
    uint16 target;                       // stack size the restock tops up to
    std::span<SupplyRank const> ranks;   // ascending
    uint8 flags = 0;                     // SupplyFlags
    uint16 target_endgame = 0;           // target from level 55 (raid-level bots), 0 = same as target
};

TC_GAME_API std::span<SupplyFamily const> GetSupplyFamilies();

// Whether a bot of class `cls` carries this family.
TC_GAME_API bool SupplyFamilyForClass(SupplyFamily const& family, uint8 cls);

// Stack size the restock tops the family up to at `level`.
TC_GAME_API uint16 SupplyTarget(SupplyFamily const& family, uint8 level);

// First Aid skill (value, max) a bot of `level` is given so it can use the
// bandage rank BestSupplyRank picks for that level.
constexpr uint32 SUPPLY_SKILL_FIRST_AID = 129;
TC_GAME_API uint16 FirstAidSkillForLevel(uint8 level);
TC_GAME_API uint16 FirstAidMaxForLevel(uint8 level);

// The family called `name` ("Instant Poison", "Deadly Poison", ...), or nullptr.
TC_GAME_API SupplyFamily const* FindSupplyFamily(std::string_view name);

// Highest rank of `family` usable at `level` that the server knows; 0 when none.
TC_GAME_API uint32 BestSupplyRank(SupplyFamily const& family, uint8 level);

// ---- Group buff spells ----
// The party versions of the class buffs (Prayer of Fortitude, Prayer of Shadow Protection, Arcane Brilliance, Gift of the Wild) and
// their higher ranks: a vanilla player trains them at 48-60, but the Forever trainers do not list them (MC mc_garr 2026-10-12: "spell
// not known" for every one of them on level 60 bots), so the supplies restock teaches the ones the bot's level allows.
TC_GAME_API std::span<uint32 const> GroupBuffSpells(uint8 cls);
// Whether `spell` exists in the spell data, is within a bot of `level`, and has an aura effect (Gift of the Wild has none in Forever's data:
// learning it would give a bot a buff that buffs nobody).
TC_GAME_API bool LearnableGroupBuff(uint32 spell, uint8 level);

// ---- Ammo for the equipped bow / crossbow / gun (Playerbot.Behavior.AmmoSupply) ----
// Kinds match the snapshot's ammo_kind: 1 = arrows (bow, crossbow), 2 = bullets (gun).
// The restock tops the bags up to AMMO_TARGET (several stacks) once the stock is under
// AMMO_LOW_PERCENT of it.
constexpr uint32 AMMO_TARGET = 1000;
constexpr uint32 AMMO_LOW_PERCENT = 30;
constexpr uint32 AmmoLowBelow() { return AMMO_TARGET * AMMO_LOW_PERCENT / 100; }

// Best projectile of `kind` a bot of `level` can use (required level <= level, highest
// ammo damage, then item level), picked from the item template store; 0 when none.
TC_GAME_API uint32 BestAmmo(uint8 kind, uint8 level);

// ---- Raid consumables (Playerbot.Raid.ProvideConsumables) ----
// Flasks, elixirs and food a raider drinks out of combat before a pull, by raid role. A kit is in order of
// priority; a bot uses the entries whose buff it lacks, from the bags (stocked for it when the config asks).
enum class RaidConsumableRole : uint8 { Tank, Melee, Caster, Healer, Hunter };

struct RaidConsumable
{
    uint32 item;
    bool   food;     // buff is "Well Fed" (the eating spell triggers it) rather than the item's own spell
    uint8  stock;    // how many a bot is kept stocked with
    bool   flask = false;   // a flask: it fills both elixir slots, so an elixir used on top of it removes it (MC Garr 2026-10-11: Mageblood took the +2000 mana of a Flask of Distilled Wisdom off five healers)
    bool   combat = false;  // drunk in the fight when the encounter data orders it (RaidRules::resistPotion), never in the pre-pull; only stocked
};

TC_GAME_API std::span<RaidConsumable const> RaidConsumablesFor(RaidConsumableRole role);

// Whether `item` is in any role's kit (the only items a raid stock request may hand out).
TC_GAME_API bool IsRaidConsumable(uint32 item);

// The stock a bot is kept at for `item`, 0 when it is in no kit.
TC_GAME_API uint8 RaidConsumableStock(uint32 item);

// An elixir of the kit while a flask of the kit is up: the flask fills both elixir slots, so the elixir on top removes it.
template <typename Buffed>
bool ElixirOverFlask(RaidConsumableRole role, RaidConsumable const& c, Buffed buffed)
{
    if (c.food || c.flask || c.combat)
        return false;
    for (RaidConsumable const& k : RaidConsumablesFor(role))
        if (k.flask && buffed(k))
            return true;
    return false;
}

// The first entry of the role's kit the bot should use now: in the bags and its buff not up, and not an elixir that would
// take a flask off. nullptr = none.
// Plain function pointers keep the header free of <functional>.
template <typename InBags, typename Buffed>
RaidConsumable const* NextRaidConsumable(RaidConsumableRole role, InBags in_bags, Buffed buffed)
{
    for (RaidConsumable const& c : RaidConsumablesFor(role))
        if (!c.combat && !buffed(c) && in_bags(c.item) && !ElixirOverFlask(role, c, buffed))
            return &c;
    return nullptr;
}

// The first entry of the role's kit the bags hold fewer than `stock` of (count(item) -> held), skipping entries
// whose buff is already up and elixirs while a flask is up: the one to hand the bot. nullptr = stocked.
template <typename Count, typename Buffed>
RaidConsumable const* NextRaidConsumableToStock(RaidConsumableRole role, Count count, Buffed buffed)
{
    for (RaidConsumable const& c : RaidConsumablesFor(role))
        if (!buffed(c) && count(c.item) < c.stock && !ElixirOverFlask(role, c, buffed))
            return &c;
    return nullptr;
}

} // namespace Playerbot::Classic
