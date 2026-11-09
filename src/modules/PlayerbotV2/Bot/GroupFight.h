// GroupFight - the one rule a bot in a player's group follows once that group fights, outside raids.
//
// Pure functions on plain values, so the rules are unit-tested without a server (tests/game/ClassicGroupFight.cpp).
//
// The owner, 2026-10-28: "simple rules like you must fight, you must heal, if you're not in line of sight, move. Follow the leader's
// orders. Attack. Pick up aggro." In UBRS that night the bots stood beside him through pulls because a dozen rules, each right on its own,
// held them back at once: Passive, an old Stay from "use altar", the follow recall, 30 y leashes, the tank's healer leash, the raid
// group's threat holds and its ranged slot 22 y behind the tank (a UBRS 10-man is a raid group on a party map). The list, in order:
//   1. Anyone in the group fights: every bot fights. No order needed, no target to name.
//   2. Healers heal first.
//   3. Out of reach or out of sight: move until it is not.
//   4. The leader's orders win: Stop, Flee, Follow, Stay, Pull back given in the last few seconds (or during this fight).
//      A one-off verb (use, loot) never leaves a lasting state.
//   5. Tanks pick up aggro: a mob on a non-tank first, then the leader's target.
//   6. Out of combat only: Passive, Stay, Follow, formation, leash, follow distance.
// Every hold that can keep a grouped bot from fighting asks BotAI::group_fight() and steps aside while it is true.
// Raid maps (20 and 40 players) keep their boss tactics: the gate is off while a boss fights there and covers the trash between the
// bosses (the owner: "make sure that in the middle of a raid the rogues or the druids aren't standing idle" on trash). A raid group in a
// dungeon (UBRS is a 10-man) plays it as a dungeon (BotSnapshotView::raid_rules).

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Playerbot::GroupFight {

// How far from the human leader a member's fight still counts as the group's (GroupControl::kFightJoinYd, the same reach).
constexpr float kGroupFightYd = 40.0f;
// A leader's Stop / Flee / Follow / Stay / Pull back is obeyed over the fight this long after it was given. An older one, given before
// the fight began, is out-of-combat behaviour and waits until the fight is over (the Stay set by "use altar" at 22:16:06 kept two
// bots out of the Emberseer fight, UBRS 2026-10-28).
constexpr uint32_t kOrderWinsMs = 10000;
// A damage dealer or tank idle this long while its group fights is a fault (logged, and the scenario check fails on it). Five seconds:
// a target swap, an opener lost to a moving mob or a walk into sight takes up to about 4 s (sfk RC8: 3.7 and 4.0 s); the UBRS faults
// this catches ran 18 s.
constexpr uint32_t kIdleFaultMs = 5000;
// The leader's combat flag alone holds the group in the fight this long. After that it counts only while a mob in combat with the group
// is within kGroupFightYd of him: a mob that fled, or one his pet chased off, kept him flagged in combat with nothing near, and his
// bots stood still up to 67 s, neither fighting nor following (KailON, Southsea Privateers, 0.1.27.2).
constexpr uint32_t kLeaderFlagOnlyMs = 8000;

enum class Order : uint8_t { None, Stop, Flee, Follow, Stay, PullBack, Fallback };

enum class Mode : uint8_t
{
    Off,        // no group fight, a raid map, a battleground, or not led by a player: the old rules decide
    Fight,      // the group fights and no order holds the bot: it fights (rules 1-3, 5)
    Ordered,    // the group fights, but a fresh leader order holds the bot (rule 4)
};

struct Inputs
{
    bool led_by_human = false;          // a player leads the group (the bot's owner or the group leader)
    bool raid_map = false;              // a raid instance map (not a raid group in a dungeon)
    bool battleground = false;
    bool self_alive = false;
    bool leader_alive = false;
    bool leader_in_combat = false;
    bool member_fighting_near_leader = false;   // another living member within kGroupFightYd of the leader is in combat
    bool leader_fight_near = true;      // a mob in combat with the group (or its pets) is within kGroupFightYd of the leader
    uint32_t leader_flag_only_ms = 0;   // how long the leader has been in combat with nothing near him (0 while something is)
    bool self_in_combat = false;        // the bot itself has aggro: it fights whatever (self-defence is never held)
    Order order = Order::None;          // the leader's standing order to this bot
    uint32_t order_age_ms = 0;          // how long ago it was given
    uint32_t fight_age_ms = 0;          // how long the group's fight has been going (0: unknown / just started)
};

inline bool GroupFighting(Inputs const& in)
{
    bool const leader_fights = in.leader_in_combat && (in.leader_fight_near || in.leader_flag_only_ms < kLeaderFlagOnlyMs);
    return in.leader_alive && (leader_fights || in.member_fighting_near_leader);
}

// Whether a leader order still beats the fight: Stop, Flee, Pull back and Fallback always while fresh; Follow and Stay while fresh, or
// when given during this fight (the leader called the bot back on purpose).
inline bool OrderHolds(Inputs const& in)
{
    switch (in.order)
    {
        case Order::None:
            return false;
        case Order::Stop:
        case Order::Flee:
        case Order::PullBack:
        case Order::Fallback:
            return in.order_age_ms <= kOrderWinsMs;
        case Order::Follow:
        case Order::Stay:
            return in.order_age_ms <= kOrderWinsMs || (in.fight_age_ms > 0 && in.order_age_ms < in.fight_age_ms);
    }
    return false;
}

// The leader's Pull / Pull back reached a bot that does not pull (pull_ms: when, 0 = never) and no Attack has answered it since
// (attack_ms) nor any later order (later_order_ms: the leader's Follow, Stay, Stop... given since, 0 = none): the tank brings the mob back
// and the rest hold where they stand (rule 4), for kOrderWinsMs at most. A Pull back never answered must not mask later orders.
// Before this the order reached only the pulling tank: rfc_oggleflint_led (0.1.26 RC2) put the hunter, mage and priest in the fight the
// moment the tank's opener landed, they ran 22-41 y at the pack the tank was bringing back, and the hunter opened on a second one:
// 14 mobs, the tank dead at 301 s.
inline bool PullWaitPending(uint32_t pull_ms, uint32_t attack_ms, uint32_t later_order_ms, uint32_t now_ms)
{
    return pull_ms != 0 && attack_ms < pull_ms && later_order_ms <= pull_ms && now_ms >= pull_ms && now_ms - pull_ms <= kOrderWinsMs;
}

inline Mode Decide(Inputs const& in)
{
    if (!in.led_by_human || in.raid_map || in.battleground || !in.self_alive || !GroupFighting(in))
        return Mode::Off;
    // A Stay or Follow never keeps a bot from answering a mob that is hitting it.
    if (OrderHolds(in) && !(in.self_in_combat && (in.order == Order::Stay || in.order == Order::Follow)))
        return Mode::Ordered;
    return Mode::Fight;
}

inline char const* ModeName(Mode m)
{
    switch (m)
    {
        case Mode::Off:     return "off";
        case Mode::Fight:   return "fight";
        case Mode::Ordered: return "ordered";
    }
    return "?";
}

// ---- Target (rule 1) ----
// The group's mobs a bot may pick, as the snapshot shows them. A mob outside the group's fight (an add in combat with someone across the
// map, the Urok challenge of 2026-10-26) is never a candidate: the caller filters with GroupControl::LeaderFightMayJoin.
struct Candidate
{
    bool alive = true;
    bool leader_ordered = false;        // the leader's Attack / Pull target, or his /focus, while the order is fresh
    bool priority_add = false;          // the dungeon script's priority add (a healer, a summoner): PveOrder::kill_focus
    bool tank_target = false;           // the main tank's victim
    bool leader_target = false;         // what the leader hits or casts at
    bool on_member = false;             // its victim is a group member or a member's pet
    bool on_me = false;                 // it is hitting the bot itself
    bool crowd_controlled = false;      // polymorphed, banished, shackled, sapped...: not to be broken
    bool held_by_group = false;         // ... by a group member (the mage's own sheep), not by someone outside the group
    bool forbidden = false;             // an ignored or immune entry, a raid script's forbidden target
    float dist = 0.0f;                  // yards from the bot
};

constexpr size_t kNone = size_t(-1);

inline size_t PickFreeTarget(std::vector<Candidate> const& c);

// The damage dealer's pick: the leader's order, the script's priority add, the tank's target, the leader's target, then the nearest
// mob on a member. A mob on the bot itself is a fallback only (the tank takes it back, rule 5), so the group keeps one kill target.
// A mob the group holds is left alone while there is anything else to fight; once every mob left is one of the group's holds, the
// group opens on the nearest (rule 1, everyone fights: wc_cobrahn_led RC10, the mage and hunter stood 6 s beside the sheeped Deviate
// Guardian, the last mob of the fight, until the sheep broke).
inline size_t PickTarget(std::vector<Candidate> const& c)
{
    size_t const pick = PickFreeTarget(c);
    if (pick != kNone)
        return pick;
    size_t held = kNone;
    for (size_t i = 0; i < c.size(); ++i)
    {
        if (!c[i].alive || c[i].forbidden)
            continue;
        if (!c[i].crowd_controlled || !c[i].held_by_group)
            return kNone;   // a mob someone else holds, or one the free pick passed over: not the group's last hold
        if (held == kNone || c[i].dist < c[held].dist)
            held = i;
    }
    return held;
}

// PickTarget without the group's holds: crowd-controlled mobs are never picked.
inline size_t PickFreeTarget(std::vector<Candidate> const& c)
{
    auto best = [&](auto pred) -> size_t
    {
        size_t pick = kNone;
        for (size_t i = 0; i < c.size(); ++i)
        {
            if (!c[i].alive || c[i].crowd_controlled || c[i].forbidden || !pred(c[i]))
                continue;
            if (pick == kNone || c[i].dist < c[pick].dist)
                pick = i;
        }
        return pick;
    };
    size_t i;
    if ((i = best([](Candidate const& x) { return x.leader_ordered; })) != kNone) return i;
    if ((i = best([](Candidate const& x) { return x.priority_add; })) != kNone) return i;
    if ((i = best([](Candidate const& x) { return x.tank_target; })) != kNone) return i;
    if ((i = best([](Candidate const& x) { return x.leader_target; })) != kNone) return i;
    if ((i = best([](Candidate const& x) { return x.on_member && !x.on_me; })) != kNone) return i;
    return best([](Candidate const& x) { return x.on_me; });
}

// The tank's pick (rule 5): a mob on a group member who is not a tank, nearest first (the taunt or the pull-off takes it); then the
// leader's order, then the leader's target, then a mob on a member. A tank that keeps what it has while a loose mob eats the casters is
// the UBRS 2026-10-28 22:16 fight: both tanks waiting on the raid plan's assignment, six Incarcerators loose on the mages for 18 s.
struct TankCandidate
{
    Candidate mob;
    bool on_non_tank = false;           // its victim is a group member that is not a tank (or a member's pet)
};

inline size_t PickTankTarget(std::vector<TankCandidate> const& c)
{
    size_t pick = kNone;
    for (size_t i = 0; i < c.size(); ++i)
    {
        Candidate const& m = c[i].mob;
        if (!m.alive || m.crowd_controlled || m.forbidden || !c[i].on_non_tank)
            continue;
        if (pick == kNone || m.dist < c[pick].mob.dist)
            pick = i;
    }
    if (pick != kNone)
        return pick;
    std::vector<Candidate> plain;
    plain.reserve(c.size());
    for (auto const& t : c)
    {
        Candidate m = t.mob;
        m.priority_add = false;   // the tank does not burn the priority add: it holds what the group fights
        m.tank_target = false;
        plain.push_back(m);
    }
    return PickTarget(plain);
}

// ---- Idle check (the [group_idle] log and the scenario check) ----
// A grouped non-healer in Fight mode that has done nothing (no cast, no swing landing, no move) since `idle_since_ms` is idle. A healer
// with nobody to heal is not idle. Dead, stunned, feared or otherwise controlled bots are not either.
inline bool IdleFault(Mode mode, bool healer, bool controlled, uint32_t idle_since_ms, uint32_t now_ms)
{
    return mode == Mode::Fight && !healer && !controlled && idle_since_ms != 0 && now_ms - idle_since_ms > kIdleFaultMs;
}

} // namespace Playerbot::GroupFight
