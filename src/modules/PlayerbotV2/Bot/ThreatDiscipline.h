// ThreatDiscipline.h - shared group threat rules for non-tank damage, AoE and
// healing (port of Karrz's wow-1.12 GroupPveRestraint / MCRaidHoldDamage,
// generalised to any group with a tank).
//
// Pure functions of the bot's snapshot (NearbyUnit my_threat / tank_threat /
// victim_kind / is_cc_locked / unit_in_combat, stamped by BotSnapshotBuilder
// from each mob's ThreatManager), so they are safe on the bot worker threads
// and usable from the class rotations (Combat/Apl) as well as the states.
//
//   * ThreatOpeningLead: the threat a tank must hold on a mob before the rest
//     of the group opens on it (config, level scaled). State_InCombat's
//     single-target hold (EvaluateThreatHold) and the AoE / heal gates below
//     share it so all three agree on when a mob counts as "tanked".
//   * FindAoeHazard / AoeSafe: an AoE must not land on a crowd-controlled mob
//     (it breaks the CC), on a mob nobody has pulled yet (it pulls it), or -
//     for a non-tank - on a mob the tank has not established threat on yet
//     (one Arcane Explosion over a fresh pack peels every mob off the tank),
//     nor on a mob the raid school rule (PveOrder::school_hints) marks as
//     avoiding the AoE's school (pass the AoE's spell id).
//   * RaidLogDue: per-key rate limit for the [raid_threat] / [raid_pull]
//     decision logs (40 bots evaluate these every tick).

#pragma once

#include "BotIntentEmitter.h"   // SpellSchoolMaskOf (raid school rule)
#include "BotSnapshot.h"
#include "BotSnapshotView.h"
#include "../Services.h"
#include "Util/ConfigReader.h"
#include "Log.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace Playerbot {

// True once per `interval_ms` for `key`. Shared across the bot worker threads.
inline bool RaidLogDue(uint64 key, uint32 now_ms, uint32 interval_ms)
{
    static std::mutex mu;
    static std::unordered_map<uint64, uint32> last;
    std::lock_guard<std::mutex> lock(mu);
    if (last.size() > 4096)
        for (auto it = last.begin(); it != last.end();)
            it = (now_ms - it->second > 60'000u) ? last.erase(it) : std::next(it);
    auto [it, inserted] = last.try_emplace(key, now_ms);
    if (inserted) return true;
    if (now_ms - it->second < interval_ms) return false;
    it->second = now_ms;
    return true;
}

// Log channels for RaidLogDue keys (low byte), so one bot's lines of
// different kinds do not throttle each other.
enum class RaidLogKind : uint8
{
    ThreatHold = 1, AoeHold = 2, PullHeld = 4, PullSkip = 5,
    OpenerHold = 6,                            // [raid_pull] non-tank opener on an unpulled mob (PrologueRules)
    PullBackOpener = 7,                        // [pull_back] no ranged opener for this tank (PrologueRules)
    Escape = 16, CancelHeal = 17, Rez = 18,   // [raid_escape], overheal cancel, rez spread
    Interrupt = 32, Tranq = 33, FearHold = 34, // [raid_interrupt], [raid_tranq], [raid_threat] fear hold
    Hazard = 48, Los = 49, Cone = 50,          // [raid_hazard], [raid_los], [raid_cone] (State_Idle)
    TankHazard = 51,                           // [raid_hazard] tank variant (State_Idle)
    TankSpread = 64, TankAoeHold = 65,         // [raid_tank] (Combat/Apl/ApTankThreat.h)
    TankDance = 66,                            // [raid_tank] warrior Battle Stance dance (Apl_Classic_Warrior)
};

inline uint64 RaidLogKey(BotSnapshotView const& s, RaidLogKind k)
{
    return (uint64(s.guid().GetCounter()) << 8) | uint64(k);
}

// Threat discipline applies here: enabled, not a battleground, inside an
// instance (or the open world when configured), and the group has a tank.
inline bool ThreatDisciplineApplies(BotSnapshotView const& s)
{
    if (!Services::Initialized()) return false;
    ConfigReader const& cfg = Services::Config();
    if (!cfg.threat_hold_enabled() || s.in_battleground()) return false;
    if (!s.is_in_instance() && !cfg.threat_hold_open_world()) return false;
    return !s.group_tank().IsEmpty();
}

// the owner: bosses need ~900 threat (three tank GCDs at 60) before anyone opens,
// trash ~250 so the group rolls from one pack member to the next. Threat
// scales with damage, i.e. with level - scaled down for levelling dungeons
// (floor 15% so a level-15 tank still gets a lead).
inline float ThreatOpeningLead(BotSnapshotView const& s, NearbyUnit const& u)
{
    ConfigReader const& cfg = Services::Config();
    const float scale = std::clamp(float(s.level()) / 60.0f, 0.15f, 1.0f);
    return ((u.is_dungeon_boss || u.is_boss_rank) ? cfg.threat_opening_lead_boss() : cfg.threat_opening_lead_trash()) * scale;
}

// A tank-held mob the tank has not secured yet against THIS bot: the tank is
// still under its opening lead, or this bot's threat (incl. pet) is already
// at the configured ratio of the tank's.
inline bool TankNotSecured(BotSnapshotView const& s, NearbyUnit const& u)
{
    if (!u.has_threat_list || !u.unit_in_combat) return false;
    if (s.big_raid() && !(u.is_dungeon_boss || u.is_boss_rank)) return false;
    if (u.tank_threat < ThreatOpeningLead(s, u)) return true;
    return u.my_threat >= u.tank_threat * Services::Config().threat_max_ratio();
}

enum class AoeHazardKind : uint8 { None, CrowdControlled, Unpulled, NotTanked, SchoolAvoided };

inline char const* AoeHazardName(AoeHazardKind k)
{
    switch (k)
    {
        case AoeHazardKind::CrowdControlled: return "crowd_controlled";
        case AoeHazardKind::Unpulled:        return "unpulled";
        case AoeHazardKind::NotTanked:       return "not_tanked";
        case AoeHazardKind::SchoolAvoided:   return "school_avoided";
        default:                             return "none";
    }
}

struct AoeHazard
{
    NearbyUnit const* unit = nullptr;
    AoeHazardKind     kind = AoeHazardKind::None;
};

// First enemy within `radius` of (cx, cy, cz) an AoE must not hit.
//   * crowd-controlled (any caster), when the AoE deals damage;
//   * in a tanked group: a mob nobody has engaged yet (the AoE would pull it);
//   * for a non-tank in a tanked group: a mob a tank is holding but has not
//     secured yet (TankNotSecured).
//   * school_spell != 0: a mob whose raid school hint (PveOrder::SchoolHint)
//     avoids that spell's school (Plague Slime immunity), in or out of a group.
// Mobs already on this bot or another non-tank member are fair game (the
// fight is on them anyway); players and un-fightable props are ignored.
// The hard vetoes (crowd-controlled, school avoided) win over the threat ones
// whatever the scan order, so a caller that only honours those (Frost Nova's
// self-defence root, the fears) still sees them.
inline AoeHazard FindAoeHazard(BotSnapshotView const& s, float cx, float cy, float cz,
                               float radius, bool damaging, bool caster_is_tank, uint32 school_spell = 0)
{
    AoeHazard hit;
    if (s.in_battleground()) return hit;
    PveOrder const& po = s.raw().pve_order;
    uint8 const aoe_school = school_spell
        ? BotIntentEmitter::SpellSchoolMaskOf(school_spell, s.raw().consumables.wand_school_mask) : uint8(0);
    auto school_avoided = [&](NearbyUnit const& u) -> bool
    {
        if (!aoe_school) return false;
        PveOrder::SchoolHint const* h = po.school_hint_for(u.guid);
        return h && (h->avoid_mask & aoe_school) != 0;
    };
    const bool tanked = ThreatDisciplineApplies(s);
    const bool me_tank = caster_is_tank || s.group_tank() == s.guid() ||
                         s.raw().group.my_role == Role::Tank;
    const float r2 = radius * radius;
    auto check = [&](NearbyUnit const& u) -> bool
    {
        if (u.hp <= 0 || u.is_player || u.untargetable || u.is_pacified) return false;
        const float dx = u.x - cx, dy = u.y - cy, dz = u.z - cz;
        if (dx * dx + dy * dy + dz * dz > r2) return false;
        // A mob a bot has symbol CC duty on counts as held before the aura lands (MarkCc: the AoE must not break it).
        if (damaging && (u.is_cc_locked || s.raw().pve_order.cc_reserved_mob(u.guid)))
        { hit = { &u, AoeHazardKind::CrowdControlled }; return true; }
        if (school_avoided(u))
        { hit = { &u, AoeHazardKind::SchoolAvoided }; return true; }
        if (hit.unit || !tanked || !u.has_threat_list) return false;   // keep the first soft hazard
        if (!u.unit_in_combat)
        { hit = { &u, AoeHazardKind::Unpulled }; return false; }
        // Raid only: a party's AoE is not held for the tank's lead (the Unpulled and CC guards above still apply).
        if (s.is_in_raid() && !me_tank && ThreatVictim(u.victim_kind) == ThreatVictim::Tank && TankNotSecured(s, u))
        { hit = { &u, AoeHazardKind::NotTanked }; return false; }
        return false;
    };
    for (NearbyUnit const& u : s.attackers())
        if (check(u)) return hit;
    for (NearbyUnit const& u : s.nearby_enemies())
        if (check(u)) return hit;
    return hit;
}

// AoE gate for the class rotations. Logs the vetoed AoE in raids.
// school_spell: the AoE's spell id for the raid school rule (0 = not checked).
inline bool AoeSafe(BotSnapshotView const& s, float cx, float cy, float cz, float radius,
                    bool damaging, bool caster_is_tank, char const* what, uint32 school_spell = 0)
{
    AoeHazard const h = FindAoeHazard(s, cx, cy, cz, radius, damaging, caster_is_tank, school_spell);
    if (!h.unit) return true;
    if (s.is_in_raid() && RaidLogDue(RaidLogKey(s, RaidLogKind::AoeHold), s.published_at_ms(), 5000u))
        TC_LOG_INFO("playerbot.v2",
            "[raid_threat] {} holds AoE {}: entry {} ({}) {} within {:.0f}y (tank_threat={:.0f} mine={:.0f})",
            s.name(), what, h.unit->entry, h.unit->guid.GetCounter(), AoeHazardName(h.kind), radius,
            h.unit->tank_threat, h.unit->my_threat);
    return false;
}

inline bool AoeSafeAroundSelf(BotSnapshotView const& s, float radius, bool damaging,
                              bool caster_is_tank, char const* what, uint32 school_spell = 0)
{
    float x, y, z; s.position(x, y, z);
    return AoeSafe(s, x, y, z, radius, damaging, caster_is_tank, what, school_spell);
}

// Melee AoE (warrior Cleave, Whirlwind) in a raid group: only two of
// FindAoeHazard's vetoes apply - a crowd-controlled mob within `radius` (the
// swing breaks the CC) and, for a non-tank, a tank-held mob the tank has not
// secured yet against this bot. A mob nobody pulled is not one: a melee AoE
// reaches a few yards and the bot is already in the pack. Outside raid
// groups (and in battlegrounds) the rotation's own conditions apply unchanged.
inline bool MeleeAoeSafe(BotSnapshotView const& s, bool raid_group, float radius, bool caster_is_tank, char const* what)
{
    if (!raid_group || s.in_battleground()) return true;
    const bool tanked = ThreatDisciplineApplies(s);
    const bool me_tank = caster_is_tank || s.group_tank() == s.guid() || s.raw().group.my_role == Role::Tank;
    float cx, cy, cz; s.position(cx, cy, cz);
    const float r2 = radius * radius;
    NearbyUnit const* hit = nullptr;
    AoeHazardKind kind = AoeHazardKind::None;
    auto check = [&](NearbyUnit const& u) -> bool
    {
        if (u.hp <= 0 || u.is_player || u.untargetable || u.is_pacified) return false;
        const float dx = u.x - cx, dy = u.y - cy, dz = u.z - cz;
        if (dx * dx + dy * dy + dz * dz > r2) return false;
        if (u.is_cc_locked || s.raw().pve_order.cc_reserved_mob(u.guid)) { hit = &u; kind = AoeHazardKind::CrowdControlled; return true; }
        if (tanked && !me_tank && u.has_threat_list && u.unit_in_combat
            && ThreatVictim(u.victim_kind) == ThreatVictim::Tank && TankNotSecured(s, u))
        { hit = &u; kind = AoeHazardKind::NotTanked; return true; }
        return false;
    };
    bool found = false;
    for (NearbyUnit const& u : s.attackers())
        if ((found = check(u))) break;
    if (!found)
        for (NearbyUnit const& u : s.nearby_enemies())
            if ((found = check(u))) break;
    if (!found) return true;
    if (RaidLogDue(RaidLogKey(s, RaidLogKind::AoeHold), s.published_at_ms(), 5000u))
        TC_LOG_INFO("playerbot.v2",
            "[raid_threat] {} holds melee AoE {}: entry {} ({}) {} within {:.0f}y (tank_threat={:.0f} mine={:.0f})",
            s.name(), what, hit->entry, hit->guid.GetCounter(), AoeHazardName(kind), radius,
            hit->tank_threat, hit->my_threat);
    return false;
}

// Around the bot's victim (Multi-Shot, Chain Lightning: the jumps land on the
// victim's neighbours). No victim -> nothing to hit -> safe.
inline bool AoeSafeAroundVictim(BotSnapshotView const& s, float radius, char const* what, uint32 school_spell = 0)
{
    NearbyUnit const* v = s.victim_info();
    if (!v) return true;
    return AoeSafe(s, v->x, v->y, v->z, radius, /*damaging*/ true, /*caster_is_tank*/ false, what, school_spell);
}

// Nearest enemy within `radius` of the bot that nobody has pulled yet: a mob
// with a threat list that is not in combat - FindAoeHazard's "unpulled" test,
// without its tanked-group precondition. nullptr when there is none.
inline NearbyUnit const* NearestUnpulled(BotSnapshotView const& s, float radius, float* out_dist = nullptr)
{
    float x, y, z; s.position(x, y, z);
    NearbyUnit const* best = nullptr;
    float best_d2 = radius * radius;
    for (NearbyUnit const& u : s.nearby_enemies())
    {
        if (u.hp <= 0 || u.is_player || u.untargetable || u.is_pacified) continue;
        if (!u.has_threat_list || u.unit_in_combat) continue;
        const float dx = u.x - x, dy = u.y - y, dz = u.z - z;
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 <= best_d2) { best_d2 = d2; best = &u; }
    }
    if (best && out_dist) *out_dist = std::sqrt(best_d2);
    return best;
}

// Raid gate for the emergency AoE fears (Psychic Scream, Howl of Terror). A
// feared mob runs up to ~40y in a random direction and drags any pack it runs
// into, so in a raid group or an instance the fear holds while an unpulled mob
// is within 40y of the caster. In a raid it is also only for a bot an elite (or
// boss) is actually hitting - not for a bot standing next to mobs the tanks
// hold. Elsewhere the callers' own conditions apply unchanged.
inline bool RaidFearAllowed(BotSnapshotView const& s, bool raid_group, char const* what)
{
    if ((!raid_group && !s.is_in_instance()) || s.in_battleground()) return true;
    constexpr float kFearPullRange = 40.0f;
    float d = 0.f;
    if (NearbyUnit const* u = NearestUnpulled(s, kFearPullRange, &d))
    {
        if (RaidLogDue(RaidLogKey(s, RaidLogKind::FearHold), s.published_at_ms(), 5000u))
            TC_LOG_INFO("playerbot.v2",
                "[raid_threat] {} holds fear {} (unpulled pack within {:.0f}y: entry {} ({}))",
                s.name(), what, d, u->entry, u->guid.GetCounter());
        return false;
    }
    if (!raid_group)
        return true;
    for (NearbyUnit const& u : s.nearby_enemies())
        if (u.hp > 0 && !u.is_player && u.victim == s.guid() && (u.is_elite_or_rare || (u.is_dungeon_boss || u.is_boss_rank)))
            return true;
    return false;
}

} // namespace Playerbot
