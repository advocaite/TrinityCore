// ApTankThreat.h - multi-mob threat spreading for the Classic tank rotations
// (Protection warrior, bear druid, Protection paladin) in dungeons and raids.
//
// The tank rotations already hold their own victim (Sunder / Maul /
// Consecration), taunt the assigned or a loose mob (BotSnapshotView::
// taunt_candidate) and walk onto it (pickup_candidate). What they lacked is
// the rest of a pack: Core Hounds, BWL whelps / dragonkin, Naxx plague and
// military packs put 3-6 mobs on one tank, and each mob the tank has not put
// threat on is one the DPS may not open on (ThreatDiscipline TankNotSecured)
// and one a heal pulls off the tank. These helpers pick the mob a tank's
// single-target threat ability goes to besides its victim ("tab-Sunder"),
// keep the tank's area threat off mobs another tank holds (the coordinator's
// PveOrder::tank_target / claimed_by_other_tanks, and live victims), and
// count the loose mobs a Challenging Shout / Roar is worth spending on.
//
// Pure functions of the snapshot (worker-thread safe). Logs [raid_tank].

#pragma once

#include "Bot/BotSnapshotView.h"
#include "Bot/ClassicUpkeep.h"     // Classic::BossFight
#include "Bot/ThreatDiscipline.h"
#include "Log.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Playerbot::Combat::TankThreat {

// Reach of a taunt-type ability (Taunt, Growl) read from the spell data of the
// rank the bot knows, less a yard of slack so the cast does not fail on a
// step; `fallback` when the data carries no range. MC 2026-10-06 (1bd2b1fe):
// a hard-coded 10 y reach left loose Giants un-taunted at 10-18 y while the
// tanks only walked toward them.
inline float TauntReach(BotSnapshotView const& s, uint32 spell, float fallback)
{
    uint32 id = s.resolve_rank(spell);
    if (!id) id = spell;
    SpellInfo const* si = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
    const float r = si ? si->GetMaxRange(/*positive*/ false) : 0.f;
    return r > 3.f ? r - 1.f : fallback;
}

// An engaged, fightable mob with a threat table.
inline bool Engaged(NearbyUnit const& u)
{
    return u.hp > 0 && !u.is_player && !u.untargetable && !u.is_pacified
        && u.unit_in_combat && u.has_threat_list;
}

inline float Dist2(BotSnapshotView const& s, NearbyUnit const& u)
{
    auto const& p = s.raw().position;
    const float dx = u.x - p.x, dy = u.y - p.y, dz = u.z - p.z;
    return dx * dx + dy * dy + dz * dz;
}

// The bot's front half-circle (the spell engine's in-front check: a melee
// ability on a mob behind the tank turns it around first).
inline bool InFront(BotSnapshotView const& s, NearbyUnit const& u)
{
    auto const& p = s.raw().position;
    const float a = std::atan2(u.y - p.y, u.x - p.x);
    float d = std::remainder(a - p.o, 2.0f * float(M_PI));
    return std::fabs(d) <= float(M_PI) / 2.0f;
}

// Another tank holds the mob, or the raid plan gave it to another tank.
inline bool OtherTanksMob(BotSnapshotView const& s, NearbyUnit const& u)
{
    if (ThreatVictim(u.victim_kind) == ThreatVictim::Tank) return true;
    return s.claim_blocks(u);
}

// The tank's own victim is safe to leave for a GCD: dead / none, held by
// another tank, or on this tank with at least the group's opening lead.
inline bool VictimSettled(BotSnapshotView const& s)
{
    NearbyUnit const* v = s.victim_info();
    if (!v || v->hp <= 0) return true;
    ThreatVictim const k = ThreatVictim(v->victim_kind);
    if (k == ThreatVictim::Tank) return true;
    if (k != ThreatVictim::Me) return false;
    return !v->has_threat_list || v->my_threat >= ThreatOpeningLead(s, *v);
}

// The mob a single-target threat ability goes to besides the victim, within
// `range`, or nullptr. Never a crowd-controlled mob, one another tank holds
// or was given, or one nobody pulled. Best first:
//   1. a loose mob (hitting a non-tank member): the nearest;
//   2. a mob on this tank under the opening lead: the lowest share of it.
// Only while the victim is settled (VictimSettled). In a boss fight only
// mobs in front, so the tank never turns the boss's adds - or the boss -
// around for a Sunder.
inline NearbyUnit const* SpreadTarget(BotSnapshotView const& s, float range)
{
    if (!s.in_combat() || !s.is_in_instance() || s.in_battleground()) return nullptr;
    if (!VictimSettled(s)) return nullptr;
    const bool boss_fight = Classic::BossFight(s);
    const ObjectGuid victim = s.victim();
    const float r2 = range * range;
    NearbyUnit const* loose = nullptr;
    float loose_d2 = r2;
    NearbyUnit const* low = nullptr;
    float low_share = 1.0f;
    for (NearbyUnit const& u : s.nearby_enemies())
    {
        if (u.guid == victim || !Engaged(u) || u.is_cc_locked || !u.in_los) continue;
        if (OtherTanksMob(s, u)) continue;
        const float d2 = Dist2(s, u);
        if (d2 > r2) continue;
        if (boss_fight && !InFront(s, u)) continue;
        ThreatVictim const k = ThreatVictim(u.victim_kind);
        if (k == ThreatVictim::Member || k == ThreatVictim::MemberPet)
        {
            if (d2 < loose_d2) { loose = &u; loose_d2 = d2; }
            continue;
        }
        if (k != ThreatVictim::Me) continue;
        const float lead = ThreatOpeningLead(s, u);
        if (lead <= 0.f) continue;
        const float share = u.my_threat / lead;
        if (share < low_share) { low = &u; low_share = share; }
    }
    return loose ? loose : low;
}

// [raid_tank] line for a threat ability sent to a SpreadTarget mob.
inline void LogSpread(BotSnapshotView const& s, char const* what, NearbyUnit const& u)
{
    if (!s.is_in_instance()) return;
    if (!RaidLogDue(RaidLogKey(s, RaidLogKind::TankSpread), s.published_at_ms(), 3000u)) return;
    ThreatVictim const k = ThreatVictim(u.victim_kind);
    TC_LOG_INFO("playerbot.v2",
        "[raid_tank] {} {} -> secondary entry {} ({}) {} my_threat={:.0f} lead={:.0f} dist={:.0f}y",
        s.name(), what, u.entry, u.guid.GetCounter(),
        (k == ThreatVictim::Member || k == ThreatVictim::MemberPet) ? "loose" : "on_me",
        u.my_threat, ThreatOpeningLead(s, u), std::sqrt(Dist2(s, u)));
}

// A tank AoE within `radius` would rip a mob another tank holds: the mob is
// on another tank and this tank's threat is already near the holder's (a
// melee needs 110% to pull; the AoE adds to every mob it lands on). Mobs
// still loose or on this tank are fair game. Logs the veto.
inline bool AoeRipsOtherTank(BotSnapshotView const& s, float radius, char const* what)
{
    if (!s.is_in_instance() || s.in_battleground()) return false;
    const float r2 = radius * radius;
    for (NearbyUnit const& u : s.nearby_enemies())
    {
        if (!Engaged(u) || Dist2(s, u) > r2) continue;
        if (ThreatVictim(u.victim_kind) != ThreatVictim::Tank) continue;
        if (u.tank_threat <= 0.f || u.my_threat < u.tank_threat * 0.8f) continue;
        if (RaidLogDue(RaidLogKey(s, RaidLogKind::TankAoeHold), s.published_at_ms(), 5000u))
            TC_LOG_INFO("playerbot.v2",
                "[raid_tank] {} holds {}: entry {} ({}) held by another tank, my_threat={:.0f} holder~{:.0f}",
                s.name(), what, u.entry, u.guid.GetCounter(), u.my_threat, u.tank_threat);
        return true;
    }
    return false;
}

// Loose mobs (on a non-tank member) within `range` no other tank was given.
inline size_t UnclaimedLooseWithin(BotSnapshotView const& s, float range)
{
    const float r2 = range * range;
    size_t n = 0;
    for (NearbyUnit const& u : s.nearby_enemies())
    {
        if (u.hp <= 0 || u.is_player || u.untargetable || u.is_pacified) continue;
        if (ThreatVictim(u.victim_kind) != ThreatVictim::Member) continue;
        if (s.claim_blocks(u)) continue;
        if (Dist2(s, u) <= r2) ++n;
    }
    return n;
}

// A crowd-controlled mob within `range`: an area taunt (Challenging Shout /
// Roar) would set it on the tank, and the next swing breaks the CC.
inline NearbyUnit const* CcLockedWithin(BotSnapshotView const& s, float range)
{
    const float r2 = range * range;
    for (NearbyUnit const& u : s.nearby_enemies())
        if (u.hp > 0 && !u.is_player && u.is_cc_locked && Dist2(s, u) <= r2)
            return &u;
    return nullptr;
}

// Area taunt gate (Challenging Shout / Roar, the owner: 3+ loose within 10 yd):
// the loose mobs nobody else was given, and no crowd-controlled mob in reach.
inline bool AreaTauntWorth(BotSnapshotView const& s, float range, char const* what)
{
    if (UnclaimedLooseWithin(s, range) < 3) return false;
    if (NearbyUnit const* cc = CcLockedWithin(s, range))
    {
        if (s.is_in_instance() && RaidLogDue(RaidLogKey(s, RaidLogKind::TankAoeHold), s.published_at_ms(), 5000u))
            TC_LOG_INFO("playerbot.v2", "[raid_tank] {} holds {}: crowd-controlled entry {} ({}) within {:.0f}y",
                s.name(), what, cc->entry, cc->guid.GetCounter(), range);
        return false;
    }
    return true;
}

// ---- Demoralizing Shout / Roar upkeep over a pack ----
// The engaged mobs within `range` (the shout's 10 yd) carrying no Demo from
// anyone - Shout and Roar don't stack, so any caster's counts - or one with
// under `refresh_ms` left (NearbyUnit::ap_debuff_ms). Crowd-controlled mobs
// are not counted (the callers veto the shout with one in reach anyway).
// `boss_missing`: one of them is a boss (Classic::IsBossUnit).
constexpr int32 kDemoRefreshMs = 3000;

inline size_t DemoMissingWithin(BotSnapshotView const& s, float range, int32 refresh_ms, bool* boss_missing = nullptr)
{
    const float r2 = range * range;
    size_t n = 0;
    thread_local std::vector<ObjectGuid> seen;
    seen.clear();
    auto visit = [&](NearbyUnit const& u)
    {
        if (!Engaged(u) || u.is_cc_locked || Dist2(s, u) > r2) return;
        if (std::find(seen.begin(), seen.end(), u.guid) != seen.end()) return;
        seen.push_back(u.guid);
        if (u.ap_debuff_ms >= refresh_ms) return;
        ++n;
        if (boss_missing && Classic::IsBossUnit(u)) *boss_missing = true;
    };
    for (NearbyUnit const& u : s.attackers())
        visit(u);
    for (NearbyUnit const& u : s.nearby_enemies())
        visit(u);
    return n;
}

// Demo is worth its GCD and rage: 2+ mobs in reach lack it (or are about to
// lose it), or a boss in reach does.
inline bool DemoRefreshWorth(BotSnapshotView const& s, float range)
{
    bool boss = false;
    return DemoMissingWithin(s, range, kDemoRefreshMs, &boss) >= 2 || boss;
}

// Engaged mobs (Engaged: in combat with a threat table, fightable) within
// `range` of the tank, crowd-controlled ones not counted - the pack a tank's
// point-blank AoE (Thunder Clap) would hit. Attackers and nearby enemies,
// each mob once.
inline size_t EngagedWithin(BotSnapshotView const& s, float range)
{
    const float r2 = range * range;
    thread_local std::vector<ObjectGuid> seen;
    seen.clear();
    auto visit = [&](NearbyUnit const& u)
    {
        if (!Engaged(u) || u.is_cc_locked || Dist2(s, u) > r2) return;
        if (std::find(seen.begin(), seen.end(), u.guid) == seen.end())
            seen.push_back(u.guid);
    };
    for (NearbyUnit const& u : s.attackers())
        visit(u);
    for (NearbyUnit const& u : s.nearby_enemies())
        visit(u);
    return seen.size();
}

} // namespace Playerbot::Combat::TankThreat
