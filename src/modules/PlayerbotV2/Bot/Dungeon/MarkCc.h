// MarkCc.h - crowd control by raid symbol (Playerbot.Behavior.MarkCc).
//
// The owner puts a raid icon on a caster bot and the same icon on an NPC: that bot crowd-controls the NPC until the
// icon on the mob becomes a skull (then the raid kills it). PveGroupCoordinator::PlanMarkOrders picks the bot / mob
// pairs (PveOrder::cc_target); the combat and idle states cast through TryCast below. The spell lists are the vanilla
// ones per class (rank 1 ids; rank resolution is the snapshot view's). Header-only, no Player access.

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include "Log.h"
#include "SharedDefines.h"
#include "ObjectMgr.h"

#include <array>
#include <cmath>
#include <span>
#include <mutex>
#include <unordered_map>

#include "../BotSnapshot.h"

namespace Playerbot::MarkCc {

// Creature type bits: 1 << Creature::GetCreatureType() (BEAST 1, DRAGONKIN 2, DEMON 3, ELEMENTAL 4, GIANT 5, UNDEAD 6,
// HUMANOID 7, CRITTER 8, MECHANICAL 9).
constexpr uint32 kBeast = 1u << 1, kDragonkin = 1u << 2, kDemon = 1u << 3, kElemental = 1u << 4, kGiant = 1u << 5,
                 kUndead = 1u << 6, kHumanoid = 1u << 7, kCritter = 1u << 8;

// Every creature type (a trap or a root is not type gated).
constexpr uint32 kAnyType = (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) |
                            (1u << 8) | (1u << 9) | (1u << 10);

constexpr uint32 kStealth = 1784;
constexpr uint32 kFreezingTrap = 1499, kEnslaveDemon = 1098, kEntanglingRoots = 339;

struct Option
{
    uint32 spell;       // rank 1 spell id
    uint32 type_mask;   // creature types the spell can hold
    float  range;       // cast range used by the gate (a little under the spell's)
    bool   ooc_only;    // never in combat (Sap)
    bool   needs_stealth;
    bool   self_cast = false;      // cast with no target: a ground trap laid at the caster's feet (Freezing Trap)
    bool   needs_reagent = false;  // the spell's reagent (Soul Shard) must be in the bags
    bool   needs_no_pet = false;   // the caster must have no active pet (Enslave Demon takes the pet slot)
    bool   root = false;           // a root aura, not a CC lock: the caster's own root on the mob counts as held
    uint32 top_rank = 0;           // a higher rank to use when the spell chain does not resolve rank 1 to it (Banish 18647)
};

// The id to check and cast: rank 1 when known (the view resolves its higher ranks), else the listed top rank.
template <class View>
uint32 SpellFor(View const& s, Option const& o)
{
    return !s.knows_spell(o.spell) && o.top_rank && s.knows_spell(o.top_rank) ? o.top_rank : o.spell;
}

// In preference order per class: the first known spell whose type mask fits the mob is used.
inline constexpr std::array<Option, 1> kMage      = {{ { 118, kBeast | kHumanoid | kCritter, 28.f, false, false } }};
// Vanilla: Banish holds Demon and Elemental, Enslave Demon only a Demon (Soul Shard, and the pet slot must be free),
// Fear a Beast / Dragonkin / Humanoid / Giant (undead are immune).
inline constexpr std::array<Option, 3> kWarlock   = {{ { kEnslaveDemon, kDemon, 28.f, false, false, false, true, true },
                                                       { 710, kDemon | kElemental, 28.f, false, false, false, false, false, false, 18647 },
                                                       { 5782, kBeast | kDragonkin | kHumanoid | kGiant, 18.f, false, false } }};
inline constexpr std::array<Option, 1> kPriest    = {{ { 9484, kUndead, 28.f, false, false } }};
// Freezing Trap (ground trap at the hunter's feet, any type, not in combat: vanilla traps cannot be laid in combat, the
// mob must come within reach of the hunter) first; in combat Scare Beast still holds a beast.
inline constexpr std::array<Option, 2> kHunter    = {{ { kFreezingTrap, kAnyType, 12.f, true, false, true },
                                                       { 1513, kBeast, 28.f, false, false } }};
inline constexpr std::array<Option, 1> kRogue     = {{ { 6770, kHumanoid, 9.f, true, true } }};
inline constexpr std::array<Option, 2> kDruid     = {{ { 2637, kBeast | kDragonkin, 28.f, false, false },
                                                       { kEntanglingRoots, kAnyType, 28.f, false, false, false, false, false, true } }};
inline constexpr std::array<Option, 1> kPaladin   = {{ { 2878, kUndead, 18.f, false, false } }};

inline std::span<Option const> OptionsFor(uint8 cls)
{
    switch (cls)
    {
        case CLASS_MAGE:    return kMage;
        case CLASS_WARLOCK: return kWarlock;
        case CLASS_PRIEST:  return kPriest;
        case CLASS_HUNTER:  return kHunter;
        case CLASS_ROGUE:   return kRogue;
        case CLASS_DRUID:   return kDruid;
        case CLASS_PALADIN: return kPaladin;
        default:            return {};
    }
}

inline bool IsMarkCcClass(uint8 cls) { return !OptionsFor(cls).empty(); }

// A mob's creature type: the snapshot's, else the template's (a view built without one reads 0, which holds nothing).
inline uint8 TypeOf(NearbyUnit const& u)
{
    if (u.creature_type)
        return u.creature_type;
    CreatureTemplate const* t = sObjectMgr->GetCreatureTemplate(u.entry);
    return t ? uint8(t->type) : uint8(0);
}

// The option this bot can hold `creature_type` with (known spell, type fits, Sap only out of combat), else nullptr.
// `ooc` = neither the bot nor the mob is in combat.
template <class View>
Option const* PickOption(View const& s, uint8 cls, uint8 creature_type, bool ooc)
{
    if (creature_type == 0) return nullptr;   // a player: never by symbol
    for (Option const& o : OptionsFor(cls))
        if ((o.type_mask & (1u << creature_type)) && (!o.ooc_only || ooc) && s.knows_spell(SpellFor(s, o)) &&
            (!o.needs_no_pet || !s.has_pet()) && (!o.needs_reagent || s.has_reagents(SpellFor(s, o))))
            return &o;
    return nullptr;
}

// [mark_cc] refusal: why an assigned crowd-control bot does not cast (Garr 2026-10-08: the owner's banish marks did not hold
// the Firesworn and the log said nothing), once per 5 s per bot.
inline void LogRefusal(uint64 bot_low, uint32 now_ms, char const* name, char const* why, uint32 entry, int ctype, float dist)
{
    static std::mutex mu;
    static std::unordered_map<uint64, uint32> last;
    {
        std::lock_guard<std::mutex> lk(mu);
        uint32& l = last[bot_low];
        if (l && uint32(now_ms - l) < 5000u) return;
        l = now_ms ? now_ms : 1u;
    }
    TC_LOG_INFO("playerbot.v2", "[mark_cc] {} does not cast: {} (entry {} type {} dist {:.0f})", name, why, entry, ctype, dist);
}

// The bot's own crowd control still holds a mob other than `except`. Vanilla Polymorph / Shackle / Banish / Hibernate hold one target per
// caster, so a second cast breaks the first: every crowd-control decider (the group CC pick and the marked CC) asks this one check.
template <class View>
bool CasterHoldsAnother(View const& s, ObjectGuid except)
{
    if (s.cls() == CLASS_ROGUE)
        return false;   // a rogue's Sap holds any number of mobs
    for (NearbyUnit const& n : s.raw().combat.nearby_enemies)
        if (n.is_cc_locked && n.cc_caster == s.guid() && n.guid != except)
            return true;
    return false;
}

// Cast the bot's crowd control on `target` (the PveOrder::cc_target mob) when it is not held yet. True when it emitted.
template <class View, class Emit>
bool TryCast(View const& s, Emit& emit, ObjectGuid target)
{
    if (target.IsEmpty())
        return false;
    uint32 const now = s.published_at_ms();
    uint64 const low = s.guid().GetCounter();
    if (s.is_casting())
        return false;   // busy: the cast in flight is most likely this one
    NearbyUnit const* u = nullptr;
    for (NearbyUnit const& n : s.raw().combat.nearby_enemies)
        if (n.guid == target) { u = &n; break; }
    if (!u)
        return LogRefusal(low, now, s.name().c_str(), "mob not in view", target.GetEntry(), -1, 0.f), false;
    if (u->hp <= 0 || u->untargetable)
        return false;
    if (u->is_cc_locked)
        return false;   // held (by us or another): the goal
    if (CasterHoldsAnother(s, target))
        return LogRefusal(low, now, s.name().c_str(), "this caster's crowd control already holds another mob", u->entry, TypeOf(*u), 0.f), false;
    // The raid's current kill target, or a mob already under half health, is being killed: a broken hold is not re-applied
    // (Garr 2026-10-08: the warlocks re-banished the add the raid was finishing and Garr enraged).
    if (s.raw().pve_order.kill_focus == target)
        return LogRefusal(low, now, s.name().c_str(), "the raid's kill target", u->entry, TypeOf(*u), 0.f), false;
    if (u->max_hp > 0 && u->hp * 2 < u->max_hp)
        return LogRefusal(low, now, s.name().c_str(), "mob under half health, being killed", u->entry, TypeOf(*u), 0.f), false;
    if (u->root_caster == s.guid())
        return false;   // our own Entangling Roots still holds it (a root is not a CC lock)
    bool const ooc = !s.in_combat() && !u->unit_in_combat;
    Option const* o = PickOption(s, s.cls(), TypeOf(*u), ooc);
    auto const& p = s.raw().position;
    float const dx = u->x - p.x, dy = u->y - p.y, dz = u->z - p.z;
    float const dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!o)
        return LogRefusal(low, now, s.name().c_str(), "no spell of this class holds that creature type (or not known / no shard)",
                          u->entry, TypeOf(*u), dist), false;
    if (o->needs_stealth && !s.has_aura(kStealth))
    {
        if (!s.in_combat() && s.is_ready(kStealth)) return emit.cast(kStealth);
        return false;
    }
    uint32 const spell = SpellFor(s, *o);
    // Range and standing still come before the ready check, which refuses a hard cast while the bot walks: a caster held in its
    // raid formation slot 26-35 y from the mob was "not ready" for 26 s and never walked in (Majordomo n78: Gorham and Elladu).
    // Out of reach: walk into it instead of standing out of range (Garr 2026-10-09: the banishers kept their formation slot 38-57 yd
    // from the Firesworn, 103 refusals, and the adds ran loose later). The stop point is a few yards inside the range on the line
    // from the mob.
    if (dist > o->range)
    {
        LogRefusal(low, now, s.name().c_str(), "out of range, moving in", u->entry, TypeOf(*u), dist);
        float const stop = std::max(o->range - 5.0f, 5.0f);
        float const f = stop / dist;   // fraction of the mob->bot line
        return emit.move_to(u->x + (p.x - u->x) * f, u->y + (p.y - u->y) * f, u->z + (p.z - u->z) * f, /*run=*/true);
    }
    // In range but walking (a formation step): stop, the cast goes out next tick (a hard cast is refused while moving).
    if (s.is_moving() && s.knows_spell(spell))
    {
        emit.stop_movement();
        return true;
    }
    if (!s.is_ready(spell))
        return LogRefusal(low, now, s.name().c_str(), "spell not ready", u->entry, TypeOf(*u), dist), false;
    bool const ok = o->self_cast ? emit.cast(spell) : emit.cast(spell, target);
    if (ok)
        TC_LOG_INFO("playerbot.v2", "[mark_cc] cast {} spell={} on entry={} ({}) type={} dist={:.0f}",
            s.name(), spell, u->entry, target.GetCounter(), u->creature_type, dist);
    return ok;
}

} // namespace Playerbot::MarkCc
