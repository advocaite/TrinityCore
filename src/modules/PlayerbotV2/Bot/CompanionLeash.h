// CompanionLeash - how far from the player it follows a led bot walks after its target (State_InCombat melee gap-close).
//
// Pure function on plain values, so the rule is unit-tested without a server.
//
// A bot the dungeon autopilot is suspended for (a human leads, Passive, a follow / stay order) fights where the player is. A mob
// standing 40 y (kGroupFightYd) or more from him is not walked after unless it is hitting the bot itself: it comes to the group, or the group goes
// to it on the player's order (UBRS 2026-10-26: the Urok challenge put its adds in combat with everyone on the map, and a passive
// companion warrior walked 180 y after one of them, pulling the Scarshield packs in between). The tank's own leash
// ([tank_chase], 30 y) is the same distance.
// The group's own fight is never leashed (the owner, 2026-10-27: "bots should always fight if you're fighting"): the player's own
// target, or a mob fighting a group member (within kGroupFightYd of that member) who stands within kGroupFightYd of the player, is
// walked after. The Urok adds, unreachable 180 y off with the tank beside the player on their threat lists, stay leashed.

#pragma once

namespace Playerbot::CompanionLeash {

constexpr float kLeashYd = 30.0f;
// The group's own fight reaches as far as the Passive join and the fight's hold on the follow do (GroupControl::kFightJoinYd): a
// player casting from range stands 30 to 36 y behind his tank. With 30 y here a warlock's rogue and cat druid stood beside him while
// the tank fought 32 y out, "does not chase ... (5 y away)" (UBRS 2026-10-28 22:09, Karrz: "they just stay close to me and do not
// fight until I move close").
constexpr float kGroupFightYd = 40.0f;

struct Inputs
{
    bool led = false;            // the autopilot is suspended for this bot (a human leads, Passive, follow / stay order)
    bool leader_known = false;   // the player it follows is alive on this map
    float victim_to_leader_d2 = 0.0f;
    bool victim_on_me = false;   // the mob is hitting this bot: it comes anyway
    bool leader_target = false;  // the player it follows has this mob as his target
    bool on_member_near_leader = false;   // the mob is on a group member (or a pet of one) within kGroupFightYd of the player,
                                          // and within kGroupFightYd of that member (OnMemberNearLeader)
};

// A stand point a led bot takes for the fight (the raid ranged / healer arc slot behind the tank) stays within the leash of the player
// it follows: the slots sit 22-25 y behind the tank, so with the tank 23 y out from the player a healer was placed 44 y from him, on the
// far side of the fight (UBRS 2026-10-27 16:15, English: "the bots should not be running all the way over there"). `point_to_leader`
// < 0 = no human leader to keep to.
inline bool StandPointNearLeader(float point_to_leader)
{
    return point_to_leader < 0.0f || point_to_leader <= kLeashYd;
}

// The mob is on a group member who stands within reach of the player, and the mob is fighting him there. 2D yards.
inline bool OnMemberNearLeader(float member_to_leader, float mob_to_member)
{
    return member_to_leader >= 0.0f && member_to_leader <= kGroupFightYd && mob_to_member >= 0.0f && mob_to_member <= kGroupFightYd;
}

// The chase reaches as far as the group's fight does (kGroupFightYd), the join's own reach: with 30 y here a rogue 27 y out with the
// tank stopped 6 y short of a mob 33 y from the player (UBRS 2026-10-28 22:09:44, Kinari).
inline bool StopsChase(Inputs const& in)
{
    return in.led && in.leader_known && !in.victim_on_me && !in.leader_target && !in.on_member_near_leader &&
           in.victim_to_leader_d2 > kGroupFightYd * kGroupFightYd;
}

} // namespace Playerbot::CompanionLeash
