// Apl_Baseline_Common.h — shared helpers + rule macros for the
// per-class Apl_Baseline_<Class>.cpp files. Lives inline so the
// per-class TUs don't pay any link-time cost.
//
// Each class baseline is registered separately via its own
// RegisterApl_Baseline_<Class>() function. Apl_Baseline.cpp owns the
// top-level RegisterApl_Baseline() aggregator that fans out to the non-vanilla classes (the nine vanilla ones have only the Classic rotations).
//
// Why this split: the previous monolithic Apl_Baseline.cpp (~1180 lines
// covering all 13 classes) made parallel coverage work impossible —
// every agent had to edit the same file. Per-class files let the
// wago.tools coverage audit drive per-class rebuilds in parallel.

#pragma once

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/States/MaintainHelpers.h"   // RaidPointInUnpulledReach
#include "Bot/ThreatDiscipline.h"         // RaidLogDue
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"
#include "Log.h"

#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace Playerbot::Combat {

// Sub-namespace keeps these helpers out of the global Combat namespace
// (they'd collide with the equivalents in some spec rotations).
namespace baseline_common {

inline bool HasLiveTarget(ApPredicateContext const& ctx)
{
    return !ctx.bot.victim().IsEmpty();
}

inline bool AlwaysInCombat(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat();
}

// Beyond this an attacker is not taken for one beside the bot (melee reach plus a step).
constexpr float kRetaliateMeleeYds = 8.0f;

inline void DoAutoAttack(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ObjectGuid t = ctx.bot.victim();
    if (t.IsEmpty()) t = ctx.bot.current_target();
    // Retaliate fallback (2026-06-17): in combat with no victim/target the bot
    // looped "Engage auto attack" as a no-op while taking damage (the CombatLoop
    // wedge cluster). Engage whatever is meleeing us (attackers are adjacent and
    // reachable), else the nearest visible enemy, so combat actually resolves.
    // Strictly additive: only runs when no target was already selected.
    //
    // The nearest attacker, not the first listed. An attacker is not always adjacent: a ranged mob shoots from where it stands, and in
    // a dungeon that can be beside the next, unpulled pack. Chasing it walks the bot into that pack's aggro reach (SM Library,
    // 4c455c9969, both runs: the tank's victim died, the retaliate went to a Scarlet Beastmaster shooting it from 30 y inside the
    // next room, the tank walked in and pulled 6 more mobs; the wipe). Such an attacker is left to the ranged damage dealers, and the
    // bot does not fall through to "the nearest visible enemy" either, which could be one of the idle mobs of that same room.
    if (t.IsEmpty())
    {
        float bx, by, bz;
        ctx.bot.position(bx, by, bz);
        bool const in_dungeon = ctx.bot.is_in_instance() && !ctx.bot.in_battleground();
        NearbyUnit const* nearest = nullptr;
        NearbyUnit const* left = nullptr;
        float nearest_d2 = 0.f;
        for (auto const& u : ctx.bot.attackers())
        {
            if (u.hp <= 0) continue;
            float const dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
            float const d2 = dx * dx + dy * dy + dz * dz;
            if (in_dungeon && d2 > kRetaliateMeleeYds * kRetaliateMeleeYds &&
                ::Playerbot::States::RaidPointInUnpulledReach(ctx.bot, u.x, u.y, u.z, /*exempt_victim_pack*/ false))
            {
                left = &u;
                continue;
            }
            if (!nearest || d2 < nearest_d2) { nearest = &u; nearest_d2 = d2; }
        }
        if (nearest)
            t = nearest->guid;
        else if (left)
        {
            uint32 const now = ctx.bot.published_at_ms();
            if (::Playerbot::RaidLogDue((uint64(ctx.bot.guid().GetCounter()) << 8) | 37u, now, 5000u))
                TC_LOG_INFO("playerbot.v2", "[retaliate_hold] {} does not chase entry {} ({}) hitting it from {:.0f} y: it stands in an idle pack's reach",
                    ctx.bot.name(), left->entry, left->guid.GetCounter(),
                    std::sqrt((left->x - bx) * (left->x - bx) + (left->y - by) * (left->y - by) + (left->z - bz) * (left->z - bz)));
            return;
        }
    }
    if (t.IsEmpty())
        for (auto const& u : ctx.bot.nearby_enemies())
            if (u.hp > 0 && u.in_los) { t = u.guid; break; }
    if (!t.IsEmpty()) e.start_attack(t);
}

} // namespace baseline_common

// Rule macros — each generates a Should/Do pair the per-class rotations
// can reference directly. They're defined in the per-class anonymous
// namespace so identifiers don't leak across TUs.

// Simple "ready + cast at victim" rule. is_ready (not just knows_spell):
// the old knows-only predicate let an on-cooldown spell pass and — before
// the engine's emission feedback — consume the whole rotation tick doing
// nothing; 31 baseline rules across 11 class files starved their own
// fillers this way (audit B14). is_ready folds knows_spell, GCD, stun,
// cast-in-progress, and the depleted-charge gate in one check.
#define BASELINE_SPELL_RULE(NAME, SPELL)                                  \
    bool Should##NAME(ApPredicateContext const& ctx)                      \
    {                                                                     \
        if (!::Playerbot::Combat::baseline_common::HasLiveTarget(ctx))    \
            return false;                                                 \
        return ctx.bot.is_ready(SPELL);                                   \
    }                                                                     \
    void Do##NAME(ApPredicateContext const& ctx, BotIntentEmitter& e)     \
    {                                                                     \
        e.cast(SPELL, ctx.bot.victim());                                  \
    }

// Interrupt rule — only fires when the current victim is casting an
// interruptible spell. Generic interrupts have 10-30s CDs; firing them
// blindly burns the CD before a real caster needs it silenced.
#define BASELINE_INTERRUPT_RULE(NAME, SPELL)                              \
    bool Should##NAME(ApPredicateContext const& ctx)                      \
    {                                                                     \
        if (!::Playerbot::Combat::baseline_common::HasLiveTarget(ctx))    \
            return false;                                                 \
        if (!ctx.bot.knows_spell(SPELL)) return false;                    \
        if (!ctx.bot.is_ready(SPELL)) return false;                       \
        NearbyUnit const* t = ctx.bot.victim_info();                      \
        if (!t || !t->is_casting || !t->is_interruptible) return false;   \
        return true;                                                      \
    }                                                                     \
    void Do##NAME(ApPredicateContext const& ctx, BotIntentEmitter& e)     \
    {                                                                     \
        e.cast(SPELL, ctx.bot.victim());                                  \
    }

// Self-cast — no target, fires on self.
#define BASELINE_SELF_RULE(NAME, SPELL)                                   \
    bool Should##NAME(ApPredicateContext const& ctx)                      \
    {                                                                     \
        return ctx.bot.knows_spell(SPELL) && ctx.bot.is_ready(SPELL);     \
    }                                                                     \
    void Do##NAME(ApPredicateContext const&, BotIntentEmitter& e)         \
    {                                                                     \
        e.cast(SPELL, ObjectGuid::Empty);                                 \
    }

// Self-cast gated on HP threshold (defensive CDs).
#define BASELINE_DEFENSIVE_RULE(NAME, SPELL, HP_BELOW)                    \
    bool Should##NAME(ApPredicateContext const& ctx)                      \
    {                                                                     \
        if (ctx.bot.hp_pct() >= (HP_BELOW)) return false;                 \
        return ctx.bot.knows_spell(SPELL) && ctx.bot.is_ready(SPELL);     \
    }                                                                     \
    void Do##NAME(ApPredicateContext const&, BotIntentEmitter& e)         \
    {                                                                     \
        e.cast(SPELL, ObjectGuid::Empty);                                 \
    }

// Apply a debuff to the current victim only if it's not already up.
// Avoids per-tick re-applications that waste GCDs and reset stack
// counters on snapshot-aware classes (warlock DoTs, hunter Mark).
#define BASELINE_DEBUFF_RULE(NAME, SPELL)                                 \
    bool Should##NAME(ApPredicateContext const& ctx)                      \
    {                                                                     \
        if (!::Playerbot::Combat::baseline_common::HasLiveTarget(ctx))    \
            return false;                                                 \
        if (!ctx.bot.knows_spell(SPELL)) return false;                    \
        if (!ctx.bot.is_ready(SPELL)) return false;                       \
        return ctx.bot.find_aura(SPELL, ctx.bot.victim()) == nullptr;     \
    }                                                                     \
    void Do##NAME(ApPredicateContext const& ctx, BotIntentEmitter& e)     \
    {                                                                     \
        e.cast(SPELL, ctx.bot.victim());                                  \
    }

// Self-buff — keep a self-aura up. Re-fires after the aura falls off
// (e.g. Power Word: Fortitude, Mark of the Wild, Arcane Intellect).
#define BASELINE_SELFBUFF_RULE(NAME, SPELL)                               \
    bool Should##NAME(ApPredicateContext const& ctx)                      \
    {                                                                     \
        if (!ctx.bot.knows_spell(SPELL)) return false;                    \
        if (!ctx.bot.is_ready(SPELL)) return false;                       \
        return ctx.bot.find_aura(SPELL, ObjectGuid::Empty) == nullptr;    \
    }                                                                     \
    void Do##NAME(ApPredicateContext const&, BotIntentEmitter& e)         \
    {                                                                     \
        e.cast(SPELL, ObjectGuid::Empty);                                 \
    }

// AoE rule — fires only when at least N enemies are within range.
// `RANGE` is yards; `MIN_TARGETS` is the threshold (typically 3).
#define BASELINE_AOE_RULE(NAME, SPELL, RANGE, MIN_TARGETS)                \
    bool Should##NAME(ApPredicateContext const& ctx)                      \
    {                                                                     \
        if (!::Playerbot::Combat::baseline_common::HasLiveTarget(ctx))    \
            return false;                                                 \
        if (!ctx.bot.knows_spell(SPELL)) return false;                    \
        if (!ctx.bot.is_ready(SPELL)) return false;                       \
        return ctx.bot.enemies_within(RANGE) >= (MIN_TARGETS);            \
    }                                                                     \
    void Do##NAME(ApPredicateContext const& ctx, BotIntentEmitter& e)     \
    {                                                                     \
        e.cast(SPELL, ctx.bot.victim());                                  \
    }

// Execute-range rule — fires when victim is under HP_BELOW% HP.
#define BASELINE_EXECUTE_RULE(NAME, SPELL, HP_BELOW)                      \
    bool Should##NAME(ApPredicateContext const& ctx)                      \
    {                                                                     \
        if (!::Playerbot::Combat::baseline_common::HasLiveTarget(ctx))    \
            return false;                                                 \
        if (!ctx.bot.knows_spell(SPELL)) return false;                    \
        if (!ctx.bot.is_ready(SPELL)) return false;                       \
        NearbyUnit const* v = ctx.bot.victim_info();                      \
        if (!v || v->max_hp <= 0) return false;                           \
        return (v->hp * 100 / v->max_hp) <= (HP_BELOW);                   \
    }                                                                     \
    void Do##NAME(ApPredicateContext const& ctx, BotIntentEmitter& e)     \
    {                                                                     \
        e.cast(SPELL, ctx.bot.victim());                                  \
    }

// Per-class registration entry points — called by RegisterApl_Baseline()
// in Apl_Baseline.cpp. Each per-class .cpp defines exactly one of these.
void RegisterApl_Baseline_DK();
void RegisterApl_Baseline_Monk();
void RegisterApl_Baseline_DH();
void RegisterApl_Baseline_Evoker();

} // namespace Playerbot::Combat
