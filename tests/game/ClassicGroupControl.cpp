/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tc_catch2.h"

#include "../../src/modules/PlayerbotV2/Bot/GroupControl.h"

#include <cstdint>

namespace
{
struct FakeGuid
{
    uint64_t v = 0;
    bool operator==(FakeGuid const& o) const { return v == o.v; }
    bool IsEmpty() const { return v == 0; }
};
using Member = Playerbot::GroupControl::Member<FakeGuid>;
}

TEST_CASE("GroupControl: the leader controls when he is a player", "[groupcontrol]")
{
    std::vector<Member> others = { { { 5 }, false }, { { 7 }, true }, { { 9 }, true } };
    CHECK(Playerbot::GroupControl::ResolveController<FakeGuid>({ 9 }, others).v == 9);
}

TEST_CASE("GroupControl: a bot leader hands control to the first player", "[groupcontrol]")
{
    std::vector<Member> others = { { { 5 }, false }, { { 7 }, true }, { { 9 }, true } };
    CHECK(Playerbot::GroupControl::ResolveController<FakeGuid>({ 5 }, others).v == 7);
    // The bot itself leads (it is not in `others`).
    CHECK(Playerbot::GroupControl::ResolveController<FakeGuid>({ 1 }, others).v == 7);
}

TEST_CASE("GroupControl: a group of bots has no controller", "[groupcontrol]")
{
    std::vector<Member> others = { { { 5 }, false }, { { 6 }, false } };
    CHECK(Playerbot::GroupControl::ResolveController<FakeGuid>({ 5 }, others).IsEmpty());
    CHECK(Playerbot::GroupControl::ResolveController<FakeGuid>({ 5 }, {}).IsEmpty());
}

TEST_CASE("GroupControl: a passive bot defends within the leash only", "[groupcontrol]")
{
    using namespace Playerbot::GroupControl;
    CHECK(PassiveMayDefend(false, 24.9f, 25.0f));
    CHECK(PassiveMayDefend(false, 25.0f, 25.0f));
    CHECK_FALSE(PassiveMayDefend(false, 25.1f, 25.0f));
    CHECK(PassiveMayDefend(true, 90.0f, 25.0f));    // a mob on the bot itself is always fought
    CHECK(PassiveMayDefend(false, 90.0f, 0.0f));    // leash off
}

TEST_CASE("GroupControl: aggressive attacks inside the radius only", "[groupcontrol]")
{
    using namespace Playerbot::GroupControl;
    CHECK(AggressiveMayAttack(10.0f, 10.0f));
    CHECK_FALSE(AggressiveMayAttack(10.5f, 10.0f));
}

TEST_CASE("GroupControl: a companion is released when its owner removes it from the group", "[groupcontrol]")
{
    using namespace Playerbot::GroupControl;
    // method: 0 default, 1 kick, 2 leave, 3 LFG kick
    CHECK(ReleaseCompanionOnRemoval(1, false, true, false));        // the owner's uninvite
    CHECK_FALSE(ReleaseCompanionOnRemoval(1, false, false, false)); // a kick by someone else
    CHECK(ReleaseCompanionOnRemoval(2, false, false, false));       // it leaves on its own
    CHECK_FALSE(ReleaseCompanionOnRemoval(0, false, true, false));  // an internal removal (a regroup, a convert's rebuild)
    CHECK_FALSE(ReleaseCompanionOnRemoval(3, false, true, false));  // an LFG kick
    CHECK_FALSE(ReleaseCompanionOnRemoval(1, true, true, false));   // a battleground group
    CHECK_FALSE(ReleaseCompanionOnRemoval(2, false, false, true));  // the bot is logging out (the owner going offline, a map change)
}

TEST_CASE("GroupControl: Passive holds only until the player fights", "[groupcontrol]")
{
    using namespace Playerbot::GroupControl;
    // The owner, 2026-10-27: "bots should always fight if you're fighting".
    CHECK(PassiveHolds(true, true, true, false));          // the player is not fighting: nothing is started
    CHECK_FALSE(PassiveHolds(true, true, true, true));     // he is: his bots join
    CHECK(PassiveHolds(true, false, true, true));          // a bot leader's fight is no player's fight
    CHECK(PassiveHolds(true, true, false, true));          // a dead player fights nothing
    CHECK_FALSE(PassiveHolds(false, true, true, false));   // not Passive at all
    // UBRS 2026-10-28 22:05-22:10: the warlock lets his tank pull and is not in combat himself; the group's fight releases his bots.
    CHECK_FALSE(PassiveHolds(true, true, true, false, true));
    CHECK(PassiveHolds(true, false, true, false, true));    // a bot-led group's fight is still no player's
    CHECK(PassiveHolds(true, true, false, false, true));    // nor a dead player's
}

TEST_CASE("GroupControl: the player's fight is joined where he is, not across the map", "[groupcontrol]")
{
    using namespace Playerbot::GroupControl;
    // His own target, wherever it stands (a warlock casting from 36 y; a mob 60 y off he has just tagged).
    CHECK(LeaderFightMayJoin(true, 60.0f, -1.0f, -1.0f));
    // A mob near him, on anyone.
    CHECK(LeaderFightMayJoin(false, 25.0f, -1.0f, -1.0f));
    // A mob on the tank who stands by him, fighting the tank there (the dragon 45 y out, the tank 35 y out and on it).
    CHECK(LeaderFightMayJoin(false, 45.0f, 35.0f, 5.0f));
    // UBRS 2026-10-26: the companion tank beside the player, in combat with unreachable Urok adds 180 y away: not joined.
    CHECK_FALSE(LeaderFightMayJoin(false, 180.0f, 3.0f, 178.0f));
    // A member fighting far across the map (a bot ported in in combat, a chase): not joined.
    CHECK_FALSE(LeaderFightMayJoin(false, 150.0f, 148.0f, 4.0f));
    // Unknown positions never join on their own.
    CHECK_FALSE(LeaderFightMayJoin(false, -1.0f, -1.0f, -1.0f));
}

TEST_CASE("GroupControl: a bot fighting the group's fight is not walked back to its leader", "[groupcontrol]")
{
    using namespace Playerbot::GroupControl;
    // UBRS 2026-10-27 14:41: the warlock owner casts at a mob 35 y off, the tank holds it. The rogue (3 y from the owner, where the
    // follow pinned it) chases the mob, and stays on it at 33 y from the owner.
    CHECK(FightOwnsMovement(true, /*leader_target*/ true, /*group_fight*/ true, false, 35.f, 3.f));
    CHECK(FightOwnsMovement(true, false, true, false, 35.f, 33.f));
    // The owner's own target is his fight at any range he can cast.
    CHECK(FightOwnsMovement(true, true, false, false, 42.f, 20.f));
    // A mob on the bot is fought where it is.
    CHECK(FightOwnsMovement(true, false, false, /*mob_on_me*/ true, 44.f, 30.f));
    // UBRS 16:15: a mob in combat that is not the group's (nobody of the group on it, not the owner's target) is not followed off.
    CHECK_FALSE(FightOwnsMovement(true, false, false, false, 20.f, 10.f));
    // A member's mob beyond the Passive join reach of the owner: the follow keeps the bot with him.
    CHECK_FALSE(FightOwnsMovement(true, false, true, false, kFightJoinYd + 1.f, 20.f));
    // UBRS 2026-10-28 22:14:03: the Dreadweaver 41 y from the warlock, 9 y from the mage it hit, the mage 33 y from him.
    CHECK(FightOwnsMovement(true, false, true, false, 41.f, 20.f, 33.f, 9.f));
    CHECK_FALSE(FightOwnsMovement(true, false, true, false, 41.f, 20.f, 45.f, 9.f));
    // No victim, or the bot far from the leader (a chase that ran off): the follow takes it back.
    CHECK_FALSE(FightOwnsMovement(false, true, true, true, 10.f, 10.f));
    CHECK_FALSE(FightOwnsMovement(true, true, true, true, 10.f, kFightMoveLeashYd));
}
