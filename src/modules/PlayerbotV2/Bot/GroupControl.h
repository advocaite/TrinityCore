// GroupControl - who commands a bot that is in a group with players, and how far a stance lets it go.
//
// Pure functions on plain values (templated on the guid type), so the rules are unit-tested without a server.
//
// A bot in a group with a player is "led": it has a controller, and it does what the group does (no world errands, orders
// reach it). The controller is its owner when the owner is in the group (handled by the snapshot builder, not here); with no
// owner in the group it is the group leader when that is a player, else the first player in the group. A group of bots only
// has none and the bots stay autonomous.

#pragma once

#include <vector>

namespace Playerbot::GroupControl {

template <class Guid>
struct Member
{
    Guid guid;
    bool human = false;   // a player (not a bot session)
};

// `others` are the group's members other than the bot itself, in group order.
template <class Guid>
Guid ResolveController(Guid const& leader, std::vector<Member<Guid>> const& others)
{
    for (auto const& m : others)
        if (m.human && m.guid == leader)
            return leader;
    for (auto const& m : others)
        if (m.human)
            return m.guid;
    return Guid{};
}

// Passive holds only while the group is not fighting: Passive keeps a bot from STARTING a fight, and once the player is in combat his
// bots fight with him (the owner, 2026-10-27: "bots should always fight if you're fighting"). The group's fight counts as much as his
// own: a warlock who lets his tank pull is not in combat himself, and his rogue and cat druid stood beside him through every UBRS pull
// with the kill focus unread ("passive_now 1" on each opener, "on the focus 0", 2026-10-28 22:05-22:10; the owner: "when we're in
// battle, they should just be going full ham"). `group_fighting` = a living member within kFightJoinYd of the human leader is in
// combat; which mobs a bot then joins is LeaderFightMayJoin's to say, so an unreachable add across the map still is not one.
inline bool PassiveHolds(bool passive_stance, bool leader_human, bool leader_alive, bool leader_in_combat, bool group_fighting = false)
{
    return passive_stance && !(leader_human && leader_alive && (leader_in_combat || group_fighting));
}

// Which mobs a Passive bot joins once the player fights (PassiveHolds false): his own target, a mob within kFightJoinYd of him, or a
// mob on a group member who stands within kFightJoinYd of him while the mob is within kFightJoinYd of that member (it is fighting
// him there). Never a fight elsewhere on the map: UBRS 2026-10-26, passive bots ran across the instance after a companion tank that
// was in combat with unreachable Urok adds far from the player. Distances are 2D yards; a negative distance means unknown.
constexpr float kFightJoinYd = 40.0f;
inline bool LeaderFightMayJoin(bool leader_target, float mob_to_leader, float member_to_leader, float mob_to_member)
{
    if (leader_target)
        return true;
    if (mob_to_leader >= 0.f && mob_to_leader <= kFightJoinYd)
        return true;
    return member_to_leader >= 0.f && member_to_leader <= kFightJoinYd && mob_to_member >= 0.f && mob_to_member <= kFightJoinYd;
}

// A bot fighting the group's fight moves for it (its chase, the combat gap-close), not after its leader: the follow / formation of a
// grouped bot stands down while its living victim is the leader's own target, is on the bot, or is the main tank's mob or on a group
// member within `kFightJoinYd` of the leader (the same reach the Passive join has), and the bot is within `kFightMoveLeashYd` of him.
// The follow re-issued every tick replaced the chase start_attack gave it, so a rogue next to a warlock owner casting from 30 y stood
// beside him and swung at nothing (UBRS 2026-10-27, Eman: "all he's doing is following me around when he should be fighting"). Any
// mob in combat was too wide: a mob some other pull put in combat is not the group's fight (UBRS 16:15: "they should be staying close").
// The group's mob counts as far out as the Passive join reaches (LeaderFightMayJoin): near him, or on a member near him and fighting that
// member there. Measured from the player alone, a Dreadweaver 41 y out on the mage by him was no fight of the group's, and the follow
// recall walked the cat druid back to the warlock as she ran at it (UBRS 2026-10-28 22:14:03-11, Dorianu 242 damage in a 21 s pull).
constexpr float kFightMoveLeashYd = 45.f;
inline bool FightOwnsMovement(bool live_victim, bool leader_target, bool group_fight, bool mob_on_me, float mob_to_leader, float bot_to_leader,
                              float member_to_leader = -1.f, float mob_to_member = -1.f)
{
    if (!live_victim || bot_to_leader < 0.f || bot_to_leader >= kFightMoveLeashYd)
        return false;
    if (leader_target || mob_on_me)
        return true;
    return group_fight && LeaderFightMayJoin(false, mob_to_leader, member_to_leader, mob_to_member);
}

// Passive: a mob already on the bot is always fought (it cannot be ignored); a mob on someone else is taken only while it is
// within `leash` yards of the controller, so a bot never leaves the group to chase one. leash <= 0 switches the leash off.
inline bool PassiveMayDefend(bool mob_on_self, float mob_to_controller, float leash)
{
    if (mob_on_self || leash <= 0.f)
        return true;
    return mob_to_controller <= leash;
}

// Aggressive: any hostile within `radius` yards of the controller, nothing beyond it.
inline bool AggressiveMayAttack(float mob_to_controller, float radius)
{
    return mob_to_controller <= radius;
}

// A companion leaves its owner for good when the owner removes it from the group (a kick) or it leaves on its own, and it is let go like a
// dismissal. Not a battleground group (its bots come and go with the match), not a bot that is logging out, not an internal removal
// (method 0, e.g. a regroup) and not a kick by anyone but the owner. `method` is the core's RemoveMethod: 0 default, 1 kick, 2 leave, 3 LFG kick.
inline bool ReleaseCompanionOnRemoval(unsigned method, bool bg_group, bool kicked_by_owner, bool logging_out)
{
    if (bg_group || logging_out)
        return false;
    if (method == 1)
        return kicked_by_owner;
    return method == 2;
}

}  // namespace Playerbot::GroupControl
