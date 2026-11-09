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

#include "../../src/modules/PlayerbotV2/Bot/CompanionLeash.h"

using namespace Playerbot::CompanionLeash;

namespace
{
Inputs Led(float victim_to_leader_yd)
{
    Inputs in;
    in.led = true;
    in.leader_known = true;
    in.victim_to_leader_d2 = victim_to_leader_yd * victim_to_leader_yd;
    return in;
}
}

TEST_CASE("CompanionLeash: a led bot does not walk after a mob far from its player", "[companionleash]")
{
    // UBRS 2026-10-26: a passive companion warrior walked 180 y after an Urok Enforcer and pulled the packs between.
    CHECK(StopsChase(Led(180.0f)));
    CHECK(StopsChase(Led(41.0f)));
    // UBRS 2026-10-28 22:09:44: a mob 33 y from the warlock, 6 y from his rogue, is the group's fight.
    CHECK_FALSE(StopsChase(Led(33.0f)));
    CHECK_FALSE(StopsChase(Led(kGroupFightYd)));
    CHECK_FALSE(StopsChase(Led(0.0f)));
}

TEST_CASE("CompanionLeash: a mob hitting the bot, an unled bot or an unknown player keeps the chase", "[companionleash]")
{
    Inputs in = Led(180.0f);
    in.victim_on_me = true;
    CHECK_FALSE(StopsChase(in));
    in = Led(180.0f);
    in.led = false;   // the autopilot leads: its own pulls
    CHECK_FALSE(StopsChase(in));
    in = Led(180.0f);
    in.leader_known = false;
    CHECK_FALSE(StopsChase(in));
}

TEST_CASE("CompanionLeash: the group's own fight is never leashed", "[companionleash]")
{
    // The owner, 2026-10-27: "bots should always fight if you're fighting".
    Inputs in = Led(45.0f);
    in.leader_target = true;   // the player's own target, 45 y from him (a warlock casting from range)
    CHECK_FALSE(StopsChase(in));
    in = Led(45.0f);
    in.on_member_near_leader = true;   // on the tank, who stands by the player
    CHECK_FALSE(StopsChase(in));
    in = Led(45.0f);   // on nobody of the group (the Urok adds): still leashed
    CHECK(StopsChase(in));
}

TEST_CASE("CompanionLeash: a led bot's fight stand point stays within the leash of its player", "[companionleash]")
{
    using namespace Playerbot::CompanionLeash;
    // UBRS 16:15, English: the healer's arc slot behind a tank 23 y out was 44 y from the player.
    CHECK_FALSE(StandPointNearLeader(44.6f));
    CHECK(StandPointNearLeader(22.0f));
    CHECK(StandPointNearLeader(kLeashYd));
    CHECK(StandPointNearLeader(-1.0f));   // no human leader
}

TEST_CASE("CompanionLeash: the group's fight reaches as far as a ranged player stands from his tank", "[companionleash]")
{
    using namespace Playerbot::CompanionLeash;
    // UBRS 2026-10-28 22:09: the tank fought 32 y from the warlock; his rogue stood 5 y from the mob and did not chase it.
    CHECK(OnMemberNearLeader(32.0f, 5.0f));
    CHECK(OnMemberNearLeader(kGroupFightYd, kGroupFightYd));
    CHECK_FALSE(OnMemberNearLeader(41.0f, 5.0f));
    CHECK_FALSE(OnMemberNearLeader(10.0f, 180.0f));   // an unreachable add far off with the tank on its threat list (Urok)
    CHECK_FALSE(OnMemberNearLeader(-1.0f, 5.0f));
}
