// Classic 1.60 (Forever) Warrior - vanilla rotations.
//
// Registered over the retail Arms / Fury / Protection rotations and the
// warrior baseline (spec 0), so on Forever every warrior runs one of these.
// The spec comes from Playerbot::Classic::EffectiveSpecId (talents or the
// owner's setrole/setspec): 73 = tank, everything else = melee DPS.
//
// Spell ids are vanilla rank 1; the snapshot view and API::cast_spell
// resolve them to the highest rank the bot knows. Reactive abilities
// (Overpower, Revenge, Execute) are gated by the snapshot's aura-state
// check, so their predicates only test rage and stance.
//
// Priorities follow Karrz's ike3 vanilla strategies (Karrz1/wow-1.12
// raid10-revamp, playerbot/strategy/warrior/*Strategy.cpp, MANGOSBOT_ZERO).

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Apl_Baseline_Common.h"
#include "ApInterrupt.h"
#include "ApTankThreat.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/ClassicUpkeep.h"   // Classic::BossFight, NoteCooldownUse
#include "Bot/ThreatDiscipline.h"
#include "Group/GroupSnapshot.h"
#include "Log.h"
#include "Services.h"
#include "Util/ConfigReader.h"   // Playerbot.Behavior.WarriorStanceDance
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <mutex>
#include <unordered_map>

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::DoAutoAttack;
using ::Playerbot::Combat::baseline_common::HasLiveTarget;

// ---- Vanilla spell ids (rank 1) ----
constexpr uint32 BATTLE_STANCE      = 2457;
constexpr uint32 DEFENSIVE_STANCE   = 71;
constexpr uint32 BERSERKER_STANCE   = 2458;
constexpr uint32 HEROIC_STRIKE      = 78;
constexpr uint32 REND               = 772;
constexpr uint32 CHARGE             = 100;
constexpr uint32 THUNDER_CLAP       = 6343;
constexpr uint32 BLOODRAGE          = 2687;
constexpr uint32 BATTLE_SHOUT       = 6673;
constexpr uint32 OVERPOWER          = 7384;
constexpr uint32 DEMORALIZING_SHOUT = 1160;
constexpr uint32 REVENGE            = 6572;
constexpr uint32 SHIELD_BASH        = 72;
constexpr uint32 SUNDER_ARMOR       = 7386;
constexpr uint32 TAUNT              = 355;
constexpr uint32 CHALLENGING_SHOUT  = 1161;
constexpr uint32 SHIELD_BLOCK       = 2565;
constexpr uint32 EXECUTE            = 5308;
constexpr uint32 CLEAVE             = 845;
constexpr uint32 SHIELD_WALL        = 871;
constexpr uint32 BERSERKER_RAGE     = 18499;
constexpr uint32 WHIRLWIND          = 1680;
constexpr uint32 PUMMEL             = 6552;
constexpr uint32 RECKLESSNESS       = 1719;
constexpr uint32 MORTAL_STRIKE      = 12294;   // Arms talent
constexpr uint32 BLOODTHIRST        = 23881;   // Fury talent
constexpr uint32 SHIELD_SLAM        = 23922;   // Protection talent
constexpr uint32 LAST_STAND         = 12975;   // Protection talent
constexpr uint32 CONCUSSION_BLOW    = 12809;   // Protection talent
constexpr uint32 DEATH_WISH         = 12292;   // Fury talent
constexpr uint32 SWEEPING_STRIKES   = 12328;   // Arms talent
constexpr uint32 MOCKING_BLOW       = 694;     // Battle Stance only
// Nefarian's class call for warriors (BWL): forces Berserker Stance for
// ~30 s. Every stance switch fails meanwhile (and the aura puts the warrior
// straight back), so a stance rule that keeps trying - the Defensive switch
// back, the Battle Stance dance, a DPS stance fix - spends a GCD per tick.
constexpr uint32 NEFARIAN_WARRIOR_CALL = 23397;
// Tactical Mastery (Arms talent), ranks 1-5: a stance change keeps up to 5
// rage per rank (vanilla; without it the switch keeps none).
constexpr uint32 TACTICAL_MASTERY_RANKS[] = { 12295, 12676, 12677, 12678, 12679 };

constexpr uint8 POWER_RAGE = 1;

constexpr uint32 SPEC_FURY       = 72;
constexpr uint32 SPEC_PROTECTION = 73;

int32 Rage(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_RAGE); }

// A Thunder Clap from any warrior is on the victim with more than 4 s left (the debuff does not stack across casters).
static bool ThunderClapUp(ApPredicateContext const& ctx)
{
    AuraEntry const* a = ctx.bot.find_aura_any_caster(THUNDER_CLAP, ctx.bot.victim());
    return a && a->remaining.count() > 4000;
}

bool Ready(ApPredicateContext const& ctx, uint32 spell)
{
    return ctx.bot.knows_spell(spell) && ctx.bot.is_ready(spell);
}

bool InStance(ApPredicateContext const& ctx, uint32 stance)
{
    return ctx.bot.has_aura(stance);
}

// The warrior's stance is forced (Nefarian's class call): no stance switch
// is attempted until the aura expires, then the usual rules resume.
bool StanceForced(ApPredicateContext const& ctx)
{
    return ctx.bot.has_aura(NEFARIAN_WARRIOR_CALL);
}

float DistToVictim(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return 1000.f;
    auto const& me = ctx.bot.raw().position;
    float const dx = v->x - me.x, dy = v->y - me.y, dz = v->z - me.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool VictimCastingInterruptible(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    return v && v->is_casting && v->is_interruptible;
}

int32 VictimHpPct(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v || v->max_hp <= 0) return 100;
    return int32(int64(v->hp) * 100 / v->max_hp);
}

// DPS stance: Fury lives in Berserker Stance once it is trained (level 30);
// Arms and low-level warriors stay in Battle Stance (Overpower, Charge).
uint32 DpsStance(ApPredicateContext const& ctx)
{
    if (ctx.bot.spec() == SPEC_FURY && ctx.bot.knows_spell(BERSERKER_STANCE))
        return BERSERKER_STANCE;
    return BATTLE_STANCE;
}

// ======================= Shared rules =======================

bool ShouldBloodrage(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && Rage(ctx) < 20 && ctx.bot.hp_pct() > 40 && Ready(ctx, BLOODRAGE);
}
void DoBloodrage(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BLOODRAGE); }

bool ShouldBattleShout(ApPredicateContext const& ctx)
{
    return Rage(ctx) >= 10 && Ready(ctx, BATTLE_SHOUT) && !ctx.bot.has_aura(BATTLE_SHOUT);
}
void DoBattleShout(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BATTLE_SHOUT); }

bool ShouldBerserkerRageFear(ApPredicateContext const& ctx)
{
    // MECHANIC_FEAR = 5, MECHANIC_SAPPED = 30. Berserker Stance only in vanilla.
    return InStance(ctx, BERSERKER_STANCE) && Ready(ctx, BERSERKER_RAGE)
        && (ctx.bot.has_mechanic(5) || ctx.bot.has_mechanic(30));
}
void DoBerserkerRage(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BERSERKER_RAGE); }

bool ShouldExecute(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Rage(ctx) >= 15 && VictimHpPct(ctx) <= 20 && (Ready(ctx, EXECUTE) && ctx.bot.victim_in_spell_range(EXECUTE))
        && !InStance(ctx, DEFENSIVE_STANCE);
}
void DoExecute(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(EXECUTE, ctx.bot.victim()); }

bool InRaid(ApPredicateContext const& ctx)
{
    return ctx.group.is_raid() || ctx.bot.is_in_raid();
}

// Raid groups: Cleave and Whirlwind go through the melee AoE gate (no swing
// into a crowd-controlled mob, none into a mob the tank has not secured yet).
bool ShouldCleave(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Rage(ctx) >= 45 && ctx.bot.enemies_within(6.0f) >= 2 && Ready(ctx, CLEAVE)
        && MeleeAoeSafe(ctx.bot, InRaid(ctx), 6.0f, /*tank*/ ctx.bot.spec() == SPEC_PROTECTION, "Cleave");
}
void DoCleave(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CLEAVE, ctx.bot.victim()); }

bool ShouldHeroicStrike(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Rage(ctx) >= 50 && Ready(ctx, HEROIC_STRIKE);
}
void DoHeroicStrike(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(HEROIC_STRIKE, ctx.bot.victim()); }

// ======================= DPS =======================

bool ShouldDpsStance(ApPredicateContext const& ctx)
{
    uint32 const want = DpsStance(ctx);
    return !StanceForced(ctx) && !InStance(ctx, want) && Ready(ctx, want);
}
void DoDpsStance(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(DpsStance(ctx)); }

bool ShouldCharge(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat() || !HasLiveTarget(ctx)) return false;
    if (!InStance(ctx, BATTLE_STANCE) || !(Ready(ctx, CHARGE) && ctx.bot.victim_in_spell_range(CHARGE))) return false;
    float const d = DistToVictim(ctx);
    return d >= 8.f && d <= 25.f;
}
void DoCharge(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CHARGE, ctx.bot.victim()); }

bool ShouldPummel(ApPredicateContext const& ctx)
{
    // Interrupt rotation (ApInterrupt.h): the plan's pick in melee range.
    return HasLiveTarget(ctx) && InStance(ctx, BERSERKER_STANCE) && Rage(ctx) >= 10
        && Interrupt::Should(ctx, PUMMEL);
}
void DoPummel(ApPredicateContext const& ctx, BotIntentEmitter& e) { Interrupt::Cast(ctx, e, PUMMEL); }

// Death Wish / Recklessness: boss fights only (Classic::BossFight - inside a
// dungeon or raid an engaged boss or encounter, never trash).
bool ShouldDeathWish(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Rage(ctx) >= 10 && Classic::BossFight(ctx.bot) && Ready(ctx, DEATH_WISH);
}
void DoDeathWish(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(DEATH_WISH);
    Classic::NoteCooldownUse(ctx.bot, DEATH_WISH, "Death Wish");
}

bool ShouldRecklessness(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InStance(ctx, BERSERKER_STANCE) && Classic::BossFight(ctx.bot)
        && VictimHpPct(ctx) <= 30 && Ready(ctx, RECKLESSNESS);
}
void DoRecklessness(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RECKLESSNESS);
    Classic::NoteCooldownUse(ctx.bot, RECKLESSNESS, "Recklessness");
}

bool ShouldSweepingStrikes(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InStance(ctx, BATTLE_STANCE) && Rage(ctx) >= 30
        && ctx.bot.enemies_within(6.0f) >= 2 && Ready(ctx, SWEEPING_STRIKES);
}
void DoSweepingStrikes(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SWEEPING_STRIKES); }

bool ShouldOverpower(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InStance(ctx, BATTLE_STANCE) && Rage(ctx) >= 5 && (Ready(ctx, OVERPOWER) && ctx.bot.victim_in_spell_range(OVERPOWER));
}
void DoOverpower(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(OVERPOWER, ctx.bot.victim()); }

bool ShouldMortalStrike(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Rage(ctx) >= 30 && (Ready(ctx, MORTAL_STRIKE) && ctx.bot.victim_in_spell_range(MORTAL_STRIKE));
}
void DoMortalStrike(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(MORTAL_STRIKE, ctx.bot.victim()); }

bool ShouldBloodthirst(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Rage(ctx) >= 30 && (Ready(ctx, BLOODTHIRST) && ctx.bot.victim_in_spell_range(BLOODTHIRST));
}
void DoBloodthirst(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(BLOODTHIRST, ctx.bot.victim()); }

bool ShouldWhirlwind(ApPredicateContext const& ctx)
{
    // Bloodthirst (30 rage) comes first: below 55 rage, Whirlwind waits while Bloodthirst is about to come off cooldown.
    if (ctx.bot.knows_spell(BLOODTHIRST) && Rage(ctx) < 55 && ctx.bot.cd_remaining(BLOODTHIRST).count() <= 1500)
        return false;
    return HasLiveTarget(ctx) && InStance(ctx, BERSERKER_STANCE) && Rage(ctx) >= 25
        && ctx.bot.enemies_within(8.0f) >= 1 && (Ready(ctx, WHIRLWIND) && ctx.bot.victim_in_spell_range(WHIRLWIND))
        && MeleeAoeSafe(ctx.bot, InRaid(ctx), 8.0f, /*tank*/ false, "Whirlwind");
}
void DoWhirlwind(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(WHIRLWIND, ctx.bot.victim()); }

// Rend while leveling: worth it before the big talent strikes exist.
bool ShouldRend(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx) || Rage(ctx) < 10 || InStance(ctx, BERSERKER_STANCE)) return false;
    if (ctx.bot.knows_spell(MORTAL_STRIKE) || ctx.bot.knows_spell(BLOODTHIRST)) return false;
    if (VictimHpPct(ctx) < 40) return false;
    return (Ready(ctx, REND) && ctx.bot.victim_in_spell_range(REND)) && !ctx.bot.has_aura(REND, ctx.bot.victim());
}
void DoRend(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(REND, ctx.bot.victim()); }

bool ShouldThunderClapDps(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InStance(ctx, BATTLE_STANCE) && Rage(ctx) >= 20
        && ctx.bot.enemies_within(8.0f) >= 3 && (Ready(ctx, THUNDER_CLAP) && ctx.bot.victim_in_spell_range(THUNDER_CLAP))
        && !ThunderClapUp(ctx)
        && AoeSafeAroundSelf(ctx.bot, 8.0f, /*damaging*/ true, /*tank*/ false, "Thunder Clap", THUNDER_CLAP);
}
void DoThunderClap(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(THUNDER_CLAP, ctx.bot.victim()); }

ApRule const kDpsRules[] = {
    { ShouldBerserkerRageFear, DoBerserkerRage,   "classic_war:berserker_rage" },
    { ShouldDpsStance,         DoDpsStance,       "classic_war:stance" },
    { ShouldCharge,            DoCharge,          "classic_war:charge" },
    { ShouldPummel,            DoPummel,          "classic_war:pummel" },
    { ShouldBloodrage,         DoBloodrage,       "classic_war:bloodrage" },
    { ShouldBattleShout,       DoBattleShout,     "classic_war:battle_shout" },
    { ShouldDeathWish,         DoDeathWish,       "classic_war:death_wish" },
    { ShouldRecklessness,      DoRecklessness,    "classic_war:recklessness" },
    { ShouldExecute,           DoExecute,         "classic_war:execute" },
    { ShouldSweepingStrikes,   DoSweepingStrikes, "classic_war:sweeping_strikes" },
    { ShouldOverpower,         DoOverpower,       "classic_war:overpower" },
    { ShouldMortalStrike,      DoMortalStrike,    "classic_war:mortal_strike" },
    { ShouldBloodthirst,       DoBloodthirst,     "classic_war:bloodthirst" },
    { ShouldWhirlwind,         DoWhirlwind,       "classic_war:whirlwind" },
    { ShouldThunderClapDps,    DoThunderClap,     "classic_war:thunder_clap" },
    { ShouldRend,              DoRend,            "classic_war:rend" },
    { ShouldCleave,            DoCleave,          "classic_war:cleave" },
    { ShouldHeroicStrike,      DoHeroicStrike,    "classic_war:heroic_strike" },
    { HasLiveTarget,           DoAutoAttack,      "classic_war:auto_attack" },
};

// ======================= Protection =======================

// ---- Battle Stance dance (Mocking Blow, Thunder Clap) ----
// Two Battle-Stance-only abilities a Protection warrior leaves Defensive
// Stance for: Defensive -> Battle -> the ability -> Defensive. The dance is
// committed per bot: started at most once per kDanceLockoutMs (no stance
// flapping when the ability falls through), held in Battle Stance only while
// its ability is still wanted and off its own cooldown, for at most
// kDanceHoldMs, and ended the moment the ability is sent, so
// classic_prot:defensive_stance switches back on the next tick.
constexpr uint32 kDanceHoldMs    = 3000;
constexpr uint32 kDanceLockoutMs = 10000;

struct StanceDance
{
    uint32 spell = 0;        // the ability the dance is for; 0 = done
    uint32 started_ms = 0;
};

std::mutex g_danceMu;
std::unordered_map<uint64, StanceDance> g_dances;

StanceDance DanceOf(ApPredicateContext const& ctx)
{
    std::lock_guard<std::mutex> lock(g_danceMu);
    auto const it = g_dances.find(ctx.bot.guid().GetCounter());
    return it != g_dances.end() ? it->second : StanceDance{};
}

void DanceBegin(ApPredicateContext const& ctx, uint32 spell)
{
    uint32 const now = ctx.bot.published_at_ms();
    std::lock_guard<std::mutex> lock(g_danceMu);
    if (g_dances.size() > 4096)
        for (auto it = g_dances.begin(); it != g_dances.end();)
            it = (now - it->second.started_ms > 60'000u) ? g_dances.erase(it) : std::next(it);
    g_dances[ctx.bot.guid().GetCounter()] = { spell, now };
}

void DanceEnd(ApPredicateContext const& ctx)
{
    std::lock_guard<std::mutex> lock(g_danceMu);
    auto const it = g_dances.find(ctx.bot.guid().GetCounter());
    if (it != g_dances.end())
        it->second.spell = 0;   // started_ms stays: the lockout runs on
}

// The dance for `spell` was started and is still inside its hold window.
bool DanceActive(ApPredicateContext const& ctx, uint32 spell)
{
    StanceDance const d = DanceOf(ctx);
    return d.spell == spell && ctx.bot.published_at_ms() - d.started_ms < kDanceHoldMs;
}

bool DanceLockedOut(ApPredicateContext const& ctx)
{
    StanceDance const d = DanceOf(ctx);
    return d.started_ms != 0 && ctx.bot.published_at_ms() - d.started_ms < kDanceLockoutMs;
}

// Rage a stance change keeps: 5 per Tactical Mastery rank, none without it.
int32 RageAfterStanceSwitch(ApPredicateContext const& ctx)
{
    int32 keep = 0;
    // Highest rank first; resolve_rank returns the id itself only when that
    // rank is the one known (a chain walk reports the top known rank).
    for (int r = int(std::size(TACTICAL_MASTERY_RANKS)) - 1; r >= 0; --r)
        if (ctx.bot.resolve_rank(TACTICAL_MASTERY_RANKS[r]) == TACTICAL_MASTERY_RANKS[r])
        {
            keep = 5 * (r + 1);
            break;
        }
    return std::min(Rage(ctx), keep);
}

// The spell's own cooldown is clear (a running GCD aside).
bool OffOwnCooldown(ApPredicateContext const& ctx, uint32 spell)
{
    return ctx.bot.knows_spell(spell) && ctx.bot.cd_remaining(spell) <= ctx.bot.gcd_remaining();
}

// A stance change is castable: Ready, or only the GCD is in the way and the
// stance spell does not wait on it (vanilla stances sit off the GCD - read
// from the spell data, so a client where they do wait keeps Ready).
bool StanceReady(ApPredicateContext const& ctx, uint32 stance)
{
    if (Ready(ctx, stance)) return true;
    if (!ctx.bot.knows_spell(stance) || !ctx.bot.gcd_active() || ctx.bot.is_casting() || ctx.bot.is_stunned())
        return false;
    SpellInfo const* si = sSpellMgr->GetSpellInfo(ctx.bot.resolve_rank(stance), DIFFICULTY_NONE);
    return si && si->StartRecoveryCategory == 0 && si->StartRecoveryTime == 0
        && ctx.bot.cd_remaining(stance).count() == 0;
}

// A boss whose victim is this tank is casting: never trade Defensive Stance
// for an ability then.
bool BossCastingOnMe(ApPredicateContext const& ctx)
{
    for (NearbyUnit const& u : ctx.bot.nearby_enemies())
        if (u.hp > 0 && u.is_casting && u.victim == ctx.bot.guid() && Classic::IsBossUnit(u))
            return true;
    return false;
}

// -- Mocking Blow: backup taunt --
// A mob of the tank's taunt pick (taunt_candidate: the assigned mob, else an
// unclaimed loose one) on a non-tank member, in melee reach, while Taunt is
// on its cooldown (a resisted Taunt is on cooldown too; the snapshot reports
// no resist). Trash only, or in a boss fight a healer under half health it
// would kill; never while a boss on this tank is casting.
constexpr float kMockingBlowRange = 5.0f;
constexpr int32 kMockingBlowRage  = 10;
constexpr int32 kHealerDyingPct   = 50;

bool HealerDyingTo(ApPredicateContext const& ctx, NearbyUnit const& u)
{
    if (auto const* members = ctx.group.members())
        for (GroupMemberSummary const& m : *members)
            if (m.guid == u.victim)
                return m.role == Role::Healer && m.is_alive && m.hp_pct() <= kHealerDyingPct;
    return false;
}

NearbyUnit const* MockingBlowTarget(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat() || !ctx.bot.knows_spell(MOCKING_BLOW) || !ctx.bot.knows_spell(TAUNT))
        return nullptr;
    if (OffOwnCooldown(ctx, TAUNT)) return nullptr;    // Taunt is the tool while it is up
    NearbyUnit const* t = ctx.bot.taunt_candidate(kMockingBlowRange);
    if (!t || t->hp <= 0 || !t->in_los || t->is_cc_locked) return nullptr;
    ThreatVictim const k = ThreatVictim(t->victim_kind);
    if (k != ThreatVictim::Member && k != ThreatVictim::MemberPet) return nullptr;
    if (BossCastingOnMe(ctx)) return nullptr;
    if (Classic::BossFight(ctx.bot) && !HealerDyingTo(ctx, *t)) return nullptr;
    return t;
}

void LogDance(ApPredicateContext const& ctx, char const* what, NearbyUnit const* t)
{
    if (!ctx.bot.is_in_instance()) return;
    if (!RaidLogDue(RaidLogKey(ctx.bot, RaidLogKind::TankDance), ctx.bot.published_at_ms(), 3000u)) return;
    if (t)
        TC_LOG_INFO("playerbot.v2", "[raid_tank] {} {} -> entry {} ({}) rage={} keep={}", ctx.bot.name(), what,
            t->entry, t->guid.GetCounter(), Rage(ctx), RageAfterStanceSwitch(ctx));
    else
        TC_LOG_INFO("playerbot.v2", "[raid_tank] {} {} rage={} keep={} engaged8y={}", ctx.bot.name(), what,
            Rage(ctx), RageAfterStanceSwitch(ctx), TankThreat::EngagedWithin(ctx.bot, 8.0f));
}

bool ShouldMockingBlowStance(ApPredicateContext const& ctx)
{
    return Services::Config().behavior_warrior_stance_dance()   // off: a Protection warrior never leaves Defensive Stance
        && !StanceForced(ctx) && InStance(ctx, DEFENSIVE_STANCE) && !DanceLockedOut(ctx)
        && Ready(ctx, MOCKING_BLOW) && StanceReady(ctx, BATTLE_STANCE)
        && RageAfterStanceSwitch(ctx) >= kMockingBlowRage
        && MockingBlowTarget(ctx) != nullptr;
}
void DoMockingBlowStance(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    NearbyUnit const* t = MockingBlowTarget(ctx);
    if (t && e.cast(BATTLE_STANCE))
    {
        DanceBegin(ctx, MOCKING_BLOW);
        LogDance(ctx, "Battle Stance for Mocking Blow (Taunt on cooldown)", t);
    }
}

bool ShouldMockingBlow(ApPredicateContext const& ctx)
{
    return InStance(ctx, BATTLE_STANCE) && DanceActive(ctx, MOCKING_BLOW) && Rage(ctx) >= kMockingBlowRage
        && Ready(ctx, MOCKING_BLOW) && MockingBlowTarget(ctx) != nullptr;
}
void DoMockingBlow(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (NearbyUnit const* t = MockingBlowTarget(ctx))
        if (e.cast(MOCKING_BLOW, t->guid))
        {
            DanceEnd(ctx);   // back to Defensive Stance on the next tick
            LogDance(ctx, "Mocking Blow, back to Defensive Stance", t);
            if (t->guid != ctx.bot.victim())
                e.start_attack(t->guid);
        }
}

// -- Thunder Clap on a trash pack --
// 3+ engaged mobs within the clap's 8 yd, trash only, no crowd-controlled mob
// or another tank's mob in reach, and 20 rage kept through the stance change
// (Tactical Mastery 4+). Once per dance lockout (the clap's cooldown is
// shorter): the switch costs every point of rage above what is kept.
constexpr float kThunderClapRange = 8.0f;
constexpr int32 kThunderClapRage  = 20;

bool ThunderClapWorth(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && HasLiveTarget(ctx) && !Classic::BossFight(ctx.bot) && !BossCastingOnMe(ctx)
        && TankThreat::EngagedWithin(ctx.bot, kThunderClapRange) >= 3
        && MeleeAoeSafe(ctx.bot, InRaid(ctx), kThunderClapRange, /*tank*/ true, "Thunder Clap")
        && AoeSafeAroundSelf(ctx.bot, kThunderClapRange, /*damaging*/ true, /*tank*/ true, "Thunder Clap", THUNDER_CLAP)
        && !TankThreat::AoeRipsOtherTank(ctx.bot, kThunderClapRange, "Thunder Clap")
        && !ThunderClapUp(ctx);
}

bool ShouldThunderClapStance(ApPredicateContext const& ctx)
{
    return Services::Config().behavior_warrior_stance_dance()   // off: a Protection warrior never leaves Defensive Stance
        && !StanceForced(ctx) && InStance(ctx, DEFENSIVE_STANCE) && !DanceLockedOut(ctx)
        && (Ready(ctx, THUNDER_CLAP) && ctx.bot.victim_in_spell_range(THUNDER_CLAP)) && StanceReady(ctx, BATTLE_STANCE)
        && RageAfterStanceSwitch(ctx) >= kThunderClapRage
        && ThunderClapWorth(ctx);
}
void DoThunderClapStance(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (e.cast(BATTLE_STANCE))
    {
        DanceBegin(ctx, THUNDER_CLAP);
        LogDance(ctx, "Battle Stance for Thunder Clap", nullptr);
    }
}

bool ShouldTankThunderClap(ApPredicateContext const& ctx)
{
    return InStance(ctx, BATTLE_STANCE) && DanceActive(ctx, THUNDER_CLAP) && Rage(ctx) >= kThunderClapRage
        && (Ready(ctx, THUNDER_CLAP) && ctx.bot.victim_in_spell_range(THUNDER_CLAP)) && ThunderClapWorth(ctx);
}
void DoTankThunderClap(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (e.cast(THUNDER_CLAP, ctx.bot.victim()))
    {
        DanceEnd(ctx);
        LogDance(ctx, "Thunder Clap, back to Defensive Stance", nullptr);
    }
}

// Stay in Battle Stance only while the dance's ability is still wanted and
// waits on nothing but the GCD (the stance change's own); any other case
// ends the dance at once.
bool DanceHold(ApPredicateContext const& ctx)
{
    if (!InStance(ctx, BATTLE_STANCE)) return false;
    if (DanceActive(ctx, MOCKING_BLOW))
        return OffOwnCooldown(ctx, MOCKING_BLOW) && Rage(ctx) >= kMockingBlowRage && MockingBlowTarget(ctx) != nullptr;
    if (DanceActive(ctx, THUNDER_CLAP))
        return OffOwnCooldown(ctx, THUNDER_CLAP) && Rage(ctx) >= kThunderClapRage && ThunderClapWorth(ctx);
    return false;
}

bool ShouldDefensiveStance(ApPredicateContext const& ctx)
{
    return !StanceForced(ctx) && !InStance(ctx, DEFENSIVE_STANCE) && StanceReady(ctx, DEFENSIVE_STANCE) && !DanceHold(ctx);
}
void DoDefensiveStance(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DEFENSIVE_STANCE); }

// ---- Fear: Berserker Rage stance dance (raid boss fears) ----
// A boss this tank holds (its victim is the tank) casting a spell that
// applies a fear (SPELL_AURA_MOD_FEAR: Onyxia's / Nefarian's Bellowing Roar):
// Berserker Stance -> Berserker Rage (fear immunity) -> back to Defensive
// (classic_prot:defensive_stance once Berserker Rage is on cooldown). Never
// a dance without a fear cast and Berserker Rage ready. Instant fears
// (Magmadar's Panic) give no cast to react to: Tremor Totem / Fear Ward cover
// those.
NearbyUnit const* BossFearCastOnMe(ApPredicateContext const& ctx)
{
    for (NearbyUnit const& u : ctx.bot.nearby_enemies())
    {
        if (u.hp <= 0 || !Classic::IsBossUnit(u) || !u.is_casting || !u.casting_spell_id || u.victim != ctx.bot.guid())
            continue;
        SpellInfo const* si = sSpellMgr->GetSpellInfo(u.casting_spell_id, DIFFICULTY_NONE);
        if (si && si->HasAura(SPELL_AURA_MOD_FEAR))
            return &u;
    }
    return nullptr;
}

bool ShouldFearDanceStance(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && !StanceForced(ctx) && !InStance(ctx, BERSERKER_STANCE) && Ready(ctx, BERSERKER_STANCE)
        && Ready(ctx, BERSERKER_RAGE) && BossFearCastOnMe(ctx) != nullptr;
}
void DoFearDanceStance(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    NearbyUnit const* u = BossFearCastOnMe(ctx);
    if (u && e.cast(BERSERKER_STANCE))
        TC_LOG_INFO("playerbot.v2", "[raid_fear] {} Berserker Stance for Berserker Rage: entry {} ({}) casting fear {} ({}ms left)",
            ctx.bot.name(), u->entry, u->guid.GetCounter(), u->casting_spell_id, u->cast_remaining.count());
}

bool ShouldFearDanceRage(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && InStance(ctx, BERSERKER_STANCE) && Ready(ctx, BERSERKER_RAGE)
        && (BossFearCastOnMe(ctx) != nullptr || ctx.bot.has_mechanic(5));
}
void DoFearDanceRage(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (e.cast(BERSERKER_RAGE))
        TC_LOG_INFO("playerbot.v2", "[raid_fear] {} Berserker Rage (fear {}), back to Defensive Stance next",
            ctx.bot.name(), ctx.bot.has_mechanic(5) ? "landed" : "incoming");
}

// Tank survival cooldowns belong to a tank on duty: in a coordinated dungeon /
// raid group (PveOrder active) only the main / off / spare tank uses them (a
// Protection warrior playing DPS keeps them); alone or ungrouped, any
// Protection warrior.
bool TankOnDuty(ApPredicateContext const& ctx)
{
    auto const& po = ctx.bot.raw().pve_order;
    return !po.active || po.tank_duty != 0;
}

// Last Stand at 25% health; Shield Wall at 20%, or at 25% once Last Stand is
// spent (up already, on cooldown or not trained).
bool ShouldLastStand(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.hp_pct() <= 25 && TankOnDuty(ctx) && Ready(ctx, LAST_STAND);
}
void DoLastStand(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LAST_STAND);
    Classic::NoteCooldownUse(ctx.bot, LAST_STAND, "Last Stand");
}

bool ShouldShieldWall(ApPredicateContext const& ctx)
{
    int32 const hp = ctx.bot.hp_pct();
    bool const last_stand_spent = !Ready(ctx, LAST_STAND) || ctx.bot.has_aura(LAST_STAND);
    return ctx.bot.in_combat() && (hp <= 20 || (hp <= 25 && last_stand_spent)) && TankOnDuty(ctx)
        && InStance(ctx, DEFENSIVE_STANCE) && Ready(ctx, SHIELD_WALL) && !ctx.bot.has_aura(SHIELD_WALL);
}
void DoShieldWall(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHIELD_WALL);
    Classic::NoteCooldownUse(ctx.bot, SHIELD_WALL, "Shield Wall");
}

// ---- Threat: loose attackers (the owner's LooseAttackersTrigger) ----
// A "loose attacker" is a mob hitting a NON-TANK group member (snapshot
// victim_kind == Member). Taunt's reach is read from the spell data
// (TankThreat::TauntReach, 10 yd when the data has none); the pick-up rule
// below walks the tank into range of the rest.
constexpr float kTauntRangeFallback = 10.0f;
float TauntRange(ApPredicateContext const& ctx)
{
    return TankThreat::TauntReach(ctx.bot, TAUNT, kTauntRangeFallback);
}

// The tank's current mob is secured when it is on us / another tank (or we
// have none) - only then may the tank leave it for a loose attacker.
bool VictimSecured(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return true;
    ThreatVictim const k = ThreatVictim(v->victim_kind);
    return v->hp <= 0 || k == ThreatVictim::Me || k == ThreatVictim::Tank;
}

// Challenging Shout when 3+ loose attackers are within 10 yd (the owner) - only
// those no other tank was given (its owner is coming; two tanks would burn
// both shouts on one pack), and never with a crowd-controlled mob in reach.
bool ShouldChallengingShout(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && Rage(ctx) >= 5 && Ready(ctx, CHALLENGING_SHOUT)
        && TankThreat::AreaTauntWorth(ctx.bot, 10.0f, "Challenging Shout");
}
void DoChallengingShout(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (e.cast(CHALLENGING_SHOUT))
        if (ctx.bot.is_in_instance())
            TC_LOG_INFO("playerbot.v2", "[raid_tank] {} Challenging Shout, {} loose within 10y", ctx.bot.name(),
                TankThreat::UnclaimedLooseWithin(ctx.bot, 10.0f));
}

bool ShouldTaunt(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && InStance(ctx, DEFENSIVE_STANCE) && Ready(ctx, TAUNT)
        && ctx.bot.taunt_candidate(TauntRange(ctx)) != nullptr;
}
void DoTaunt(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    // Taunt forces the mob onto us; switch our swings / Sunders onto it too.
    if (auto const* t = ctx.bot.taunt_candidate(TauntRange(ctx)))
        if (e.cast(TAUNT, t->guid))
        {
            if (ctx.bot.is_in_instance())
                TC_LOG_INFO("playerbot.v2", "[taunt] {} Taunt -> entry {} ({}) {}", ctx.bot.name(), t->entry,
                    t->guid.GetCounter(), t == ctx.bot.assigned_tank_mob() ? "assigned" : "loose");
            if (t->guid != ctx.bot.victim())
                e.start_attack(t->guid);
        }
}

// Taunt on cooldown / out of range: switch victim to the nearest loose
// attacker and build threat on it (Revenge / Shield Slam / Sunder below).
bool ShouldPickUpLoose(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    // An assigned raid mob is always ours to hold; a loose mob only once
    // the current one is secured.
    if (!ctx.bot.assigned_tank_mob() && !VictimSecured(ctx)) return false;
    NearbyUnit const* t = ctx.bot.pickup_candidate();
    return t && t->guid != ctx.bot.victim();
}
void DoPickUpLoose(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.pickup_candidate())
        e.start_attack(t->guid);
}

bool ShouldShieldBash(ApPredicateContext const& ctx)
{
    // Interrupt rotation (ApInterrupt.h; needs a shield equipped).
    return HasLiveTarget(ctx) && !InStance(ctx, BERSERKER_STANCE) && Rage(ctx) >= 10
        && Interrupt::Should(ctx, SHIELD_BASH);
}
void DoShieldBash(ApPredicateContext const& ctx, BotIntentEmitter& e) { Interrupt::Cast(ctx, e, SHIELD_BASH); }

// A caster that stands off and casts at the group (deadmines_full: the Defias Evoker's Flamestrike went through all run, two Shield
// Bashes in 100 s): the Shield Bash is melee, so the tank turns on the caster to close the gap, and the interrupt rule above
// fires once it is in reach. Only while the bash is ready and the caster is in sight within 20 y.
NearbyUnit const* StandOffCaster(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat() || !TankOnDuty(ctx) || InStance(ctx, BERSERKER_STANCE) || Rage(ctx) < 10) return nullptr;
    if (!Interrupt::HasShield(ctx.bot) || !Ready(ctx, SHIELD_BASH)) return nullptr;
    NearbyUnit const* best = nullptr;
    float best_d = 0.f;
    for (NearbyUnit const& u : ctx.bot.nearby_enemies())
    {
        if (!Interrupt::Casting(u) || !Interrupt::EngagedWithGroup(u) || !u.is_ranged_caster || !u.in_los) continue;
        if (u.guid == ctx.bot.victim()) continue;
        if (u.cast_remaining.count() < 1200) continue;   // too late to arrive: the next cast
        float const d = Interrupt::Dist(ctx.bot, u);
        if (d <= 6.f || d > 20.f) continue;
        if (!best || d < best_d) { best = &u; best_d = d; }
    }
    return best;
}
bool ShouldCloseOnCaster(ApPredicateContext const& ctx) { return StandOffCaster(ctx) != nullptr; }
void DoCloseOnCaster(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (NearbyUnit const* u = StandOffCaster(ctx))
        e.start_attack(u->guid);
}

bool ShouldConcussionBlow(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Rage(ctx) >= 15 && VictimCastingInterruptible(ctx) && (Ready(ctx, CONCUSSION_BLOW) && ctx.bot.victim_in_spell_range(CONCUSSION_BLOW));
}
void DoConcussionBlow(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CONCUSSION_BLOW, ctx.bot.victim()); }

bool ShouldShieldBlock(ApPredicateContext const& ctx)
{
    // Needs a shield. It builds no threat, so a low-level tank's few rage go to Sunder / Revenge first (a Shield
    // Block every 5 s at 90% health starved Sunder and the tank lost Rhahk'Zor to the healer's threat): it is cast
    // when the tank is actually hurt, or when rage is plentiful.
    // Two or more mobs on the tank is the fight Shield Block is for (deadmines_full: one Shield Block in a whole run, the tank
    // died to Miners at 248 s with the healer behind): from 90% health on, 10 rage.
    int32 const hp = ctx.bot.hp_pct();
    size_t const on_me = ctx.bot.attackers_count();
    return ctx.bot.in_combat() && InStance(ctx, DEFENSIVE_STANCE) && on_me >= 1
        && (hp <= 70 ? Rage(ctx) >= 10 : (hp <= 90 && (on_me >= 2 ? Rage(ctx) >= 10 : Rage(ctx) >= 40)))
        && Interrupt::HasShield(ctx.bot) && Ready(ctx, SHIELD_BLOCK) && !ctx.bot.has_aura(SHIELD_BLOCK);
}
void DoShieldBlock(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHIELD_BLOCK); }

// ---- Threat spread over a pack (tab-Sunder) ----
// Single-target threat goes to the victim until it carries the group's
// opening lead, then to the pack mob in melee reach that needs it most (a
// loose one first): TankThreat::SpreadTarget. The victim itself is not
// switched (the pick-up rule owns it); the ability is cast on the other mob.
constexpr float kSpreadMeleeRange = 6.0f;

NearbyUnit const* SpreadTarget(ApPredicateContext const& ctx)
{
    return TankThreat::SpreadTarget(ctx.bot, kSpreadMeleeRange);
}

bool ShouldRevenge(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InStance(ctx, DEFENSIVE_STANCE) && Rage(ctx) >= 5 && Ready(ctx, REVENGE);
}
void DoRevenge(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (NearbyUnit const* t = SpreadTarget(ctx))
    {
        if (e.cast(REVENGE, t->guid))
            TankThreat::LogSpread(ctx.bot, "Revenge", *t);
        return;
    }
    e.cast(REVENGE, ctx.bot.victim());
}

// Sunder Armor on the spread target (the victim already has its lead).
bool ShouldTabSunder(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InStance(ctx, DEFENSIVE_STANCE) && Rage(ctx) >= 15 && Ready(ctx, SUNDER_ARMOR)
        && SpreadTarget(ctx) != nullptr;
}
void DoTabSunder(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (NearbyUnit const* t = SpreadTarget(ctx))
        if (e.cast(SUNDER_ARMOR, t->guid))
            TankThreat::LogSpread(ctx.bot, "Sunder Armor", *t);
}

// Tank Cleave: the second swing lands on a pack mob. Earlier (30 rage) while
// a mob in reach still needs threat; never into a CC'd mob (MeleeAoeSafe) or
// one another tank holds close to its threat (AoeRipsOtherTank).
bool ShouldTankCleave(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx) || !Ready(ctx, CLEAVE) || ctx.bot.enemies_within(6.0f) < 2) return false;
    const int32 need = SpreadTarget(ctx) ? 30 : 45;
    return Rage(ctx) >= need
        && MeleeAoeSafe(ctx.bot, InRaid(ctx), 6.0f, /*tank*/ true, "Cleave")
        && !TankThreat::AoeRipsOtherTank(ctx.bot, 6.0f, "Cleave");
}

bool ShouldShieldSlam(ApPredicateContext const& ctx)
{
    // Needs a shield: without one the core refuses every cast (EQUIPPED_ITEM_CLASS) and this rule, ahead of Sunder, ate the tank's rage
    // all fight (n43 onyxia_full: 69 refusals, two of three warrior tanks without a shield, rage idle at 25+ for 40 % of the fight).
    return HasLiveTarget(ctx) && Rage(ctx) >= 20 && Interrupt::HasShield(ctx.bot)
        && (Ready(ctx, SHIELD_SLAM) && ctx.bot.victim_in_spell_range(SHIELD_SLAM));
}
void DoShieldSlam(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SHIELD_SLAM, ctx.bot.victim()); }

// Build threat with Sunder Armor until 5 stacks, then keep it up.
bool ShouldSunderBuild(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx) || Rage(ctx) < 15 || !Ready(ctx, SUNDER_ARMOR)) return false;
    // Sunder stacks are shared by every caster: a second tank must not keep sundering a mob already at 5 stacks.
    AuraEntry const* up = ctx.bot.find_aura_any_caster(SUNDER_ARMOR, ctx.bot.victim());
    return !up || up->stacks < 5 || up->remaining.count() < 5000;
}
void DoSunder(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SUNDER_ARMOR, ctx.bot.victim()); }

// Demoralizing Shout over the pack in reach (10 yd): 2+ engaged mobs carry
// no Demoralizing Shout / Roar from anyone or under 3 s of it, or a boss in
// reach does (TankThreat::DemoRefreshWorth, NearbyUnit::ap_debuff_ms). Only
// with rage to spare: the shout's 10 and 15 more for the next Sunder /
// Revenge. Never with a crowd-controlled mob in reach (MeleeAoeSafe in
// raids, AoeSafe everywhere).
constexpr int32 kDemoShoutRage = 25;
bool ShouldDemoShout(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && HasLiveTarget(ctx) && Rage(ctx) >= kDemoShoutRage
        && Ready(ctx, DEMORALIZING_SHOUT)
        && TankThreat::DemoRefreshWorth(ctx.bot, 10.0f)
        && MeleeAoeSafe(ctx.bot, InRaid(ctx), 10.0f, /*tank*/ true, "Demoralizing Shout")
        // No damage (a sheep survives it), but it aggroes every mob it lands on.
        && AoeSafeAroundSelf(ctx.bot, 10.0f, /*damaging*/ false, /*tank*/ true, "Demoralizing Shout");
}
void DoDemoShout(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DEMORALIZING_SHOUT); }

bool ShouldTankHeroicStrike(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Rage(ctx) >= 60 && Ready(ctx, HEROIC_STRIKE);
}

bool ShouldSunderFiller(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Rage(ctx) >= 25 && Ready(ctx, SUNDER_ARMOR);
}

ApRule const kTankRules[] = {
    { ShouldLastStand,        DoLastStand,       "classic_prot:last_stand" },
    { ShouldShieldWall,       DoShieldWall,      "classic_prot:shield_wall" },
    { ShouldFearDanceRage,    DoFearDanceRage,   "classic_prot:fear_berserker_rage" },
    { ShouldFearDanceStance,  DoFearDanceStance, "classic_prot:fear_berserker_stance" },
    { ShouldMockingBlow,      DoMockingBlow,     "classic_prot:mocking_blow" },
    { ShouldTankThunderClap,  DoTankThunderClap, "classic_prot:thunder_clap" },
    { ShouldDefensiveStance,  DoDefensiveStance, "classic_prot:defensive_stance" },
    { ShouldChallengingShout, DoChallengingShout, "classic_prot:challenging_shout" },
    { ShouldTaunt,            DoTaunt,           "classic_prot:taunt" },
    { ShouldMockingBlowStance, DoMockingBlowStance, "classic_prot:mocking_blow_stance" },
    { ShouldPickUpLoose,      DoPickUpLoose,     "classic_prot:pick_up_loose" },
    { ShouldShieldBash,       DoShieldBash,      "classic_prot:shield_bash" },
    { ShouldCloseOnCaster,    DoCloseOnCaster,   "classic_prot:close_on_caster" },
    { ShouldConcussionBlow,   DoConcussionBlow,  "classic_prot:concussion_blow" },
    { ShouldBloodrage,        DoBloodrage,       "classic_prot:bloodrage" },
    { ShouldShieldBlock,      DoShieldBlock,     "classic_prot:shield_block" },
    { ShouldThunderClapStance, DoThunderClapStance, "classic_prot:thunder_clap_stance" },
    { ShouldRevenge,          DoRevenge,         "classic_prot:revenge" },
    { ShouldShieldSlam,       DoShieldSlam,      "classic_prot:shield_slam" },
    { ShouldTabSunder,        DoTabSunder,       "classic_prot:tab_sunder" },
    { ShouldSunderBuild,      DoSunder,          "classic_prot:sunder_build" },
    { ShouldDemoShout,        DoDemoShout,       "classic_prot:demoralizing_shout" },
    { ShouldBattleShout,      DoBattleShout,     "classic_prot:battle_shout" },
    { ShouldTankCleave,       DoCleave,          "classic_prot:cleave" },
    { ShouldTankHeroicStrike, DoHeroicStrike,    "classic_prot:heroic_strike" },
    { ShouldSunderFiller,     DoSunder,          "classic_prot:sunder" },
    { HasLiveTarget,          DoAutoAttack,      "classic_prot:auto_attack" },
};

} // namespace

void RegisterApl_Classic_Warrior()
{
    ApRotation const dps{ std::span<ApRule const>(kDpsRules) };
    ApRotation const tank{ std::span<ApRule const>(kTankRules) };
    RegisterRotationOverride(CLASS_WARRIOR, 0, dps);
    RegisterRotationOverride(CLASS_WARRIOR, 71, dps);
    RegisterRotationOverride(CLASS_WARRIOR, SPEC_FURY, dps);
    RegisterRotationOverride(CLASS_WARRIOR, SPEC_PROTECTION, tank);
}

} // namespace Playerbot::Combat
