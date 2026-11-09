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

#include "../../src/modules/PlayerbotV2/Api/FollowDrop.h"

#include <cmath>

using namespace Playerbot::FollowDrop;

namespace
{
    // A gallery at z 122 for y above -410 and the arena floor at z 110.9 below it (the UBRS stadium case, 11 y down).
    bool Stadium(float, float y, float hint, float& z)
    {
        float const floor_z = y > -410.f ? 122.0f : 110.9f;
        if (floor_z > hint + 3.0f)
            return false;
        z = floor_z;
        return true;
    }

    bool AlwaysClear(Vec const&, Vec const&) { return true; }
    bool NeverClear(Vec const&, Vec const&) { return false; }

    // Level ground at z 100 along x, with a gap from x 10 to x 14.
    bool GapGround(float x, float, float, float& z)
    {
        if (x >= 10.f && x < 14.f)
            return false;
        z = 100.0f;
        return true;
    }
}

TEST_CASE("Follow drop: an owner 11 y below the gallery, no walking route, sends the bot to the edge", "[ClassicFollowDrop]")
{
    Vec const bot{ 147.f, -392.f, 122.f }, owner{ 148.f, -420.f, 110.9f };
    Plan const p = PlanFollowDrop(bot, owner, false, Stadium, AlwaysClear);
    REQUIRE(p.mode == Mode::Walk);
    CHECK_FALSE(p.leap);
    CHECK(p.ledge.y == Catch::Approx(-409.f).margin(1.01));
    CHECK(p.ledge.z == Catch::Approx(122.f).margin(0.01));
    CHECK(p.land.z == Catch::Approx(110.9f).margin(0.01));
    CHECK(p.walk > 15.f);
}

TEST_CASE("Follow drop: at the edge the bot drops", "[ClassicFollowDrop]")
{
    Vec const bot{ 147.f, -408.5f, 122.f }, owner{ 148.f, -420.f, 110.9f };
    Plan const p = PlanFollowDrop(bot, owner, false, Stadium, AlwaysClear);
    CHECK(p.mode == Mode::Drop);
    CHECK(p.land.y < -409.f);
    CHECK(p.land.z == Catch::Approx(110.9f).margin(0.01));
}

TEST_CASE("Follow drop: a walking route that is fine means no drop", "[ClassicFollowDrop]")
{
    Vec const bot{ 147.f, -392.f, 122.f }, owner{ 148.f, -420.f, 110.9f };
    CHECK(PlanFollowDrop(bot, owner, true, Stadium, AlwaysClear).mode == Mode::None);
}

TEST_CASE("Follow drop: an owner more than 50 y away is not followed off a ledge", "[ClassicFollowDrop]")
{
    Vec const bot{ 147.f, -350.f, 122.f }, owner{ 148.f, -425.f, 110.9f };
    CHECK(PlanFollowDrop(bot, owner, false, Stadium, AlwaysClear).mode == Mode::None);
}

TEST_CASE("Follow drop: a wall between the lip and the landing blocks the drop", "[ClassicFollowDrop]")
{
    Vec const bot{ 147.f, -392.f, 122.f }, owner{ 148.f, -420.f, 110.9f };
    CHECK(PlanFollowDrop(bot, owner, false, Stadium, NeverClear).mode == Mode::None);
}

TEST_CASE("Follow drop: a landing that is not the owner's floor is not taken", "[ClassicFollowDrop]")
{
    Vec const bot{ 147.f, -392.f, 122.f }, owner{ 148.f, -420.f, 95.f };
    CHECK(PlanFollowDrop(bot, owner, false, Stadium, AlwaysClear).mode == Mode::None);
}

TEST_CASE("Follow drop: a wall that rises ahead is no ledge", "[ClassicFollowDrop]")
{
    auto wall = [](float, float y, float hint, float& z)
    {
        z = y < -400.f ? 126.0f : 122.0f;
        return z <= hint + 5.0f;
    };
    Vec const bot{ 147.f, -392.f, 122.f }, owner{ 148.f, -420.f, 122.f };
    CHECK(PlanFollowDrop(bot, owner, false, wall, AlwaysClear).mode == Mode::None);
}

TEST_CASE("Follow drop: a level gap of 4 y is leapt from its edge", "[ClassicFollowDrop]")
{
    Vec const bot{ 4.f, 0.f, 100.f }, owner{ 20.f, 0.f, 100.f };
    Plan p = PlanFollowDrop(bot, owner, false, GapGround, AlwaysClear);
    REQUIRE(p.mode == Mode::Walk);
    CHECK(p.leap);
    CHECK(p.ledge.x == Catch::Approx(9.f).margin(1.01));
    CHECK(p.land.x >= 13.9f);
    Vec const at_edge{ 9.f, 0.f, 100.f };
    p = PlanFollowDrop(at_edge, owner, false, GapGround, AlwaysClear);
    CHECK(p.mode == Mode::Leap);
}

TEST_CASE("Follow drop: a gap wider than a jump is not leapt", "[ClassicFollowDrop]")
{
    auto wide = [](float x, float, float, float& z)
    {
        if (x >= 10.f && x < 24.f)
            return false;
        z = 100.0f;
        return true;
    };
    Vec const bot{ 4.f, 0.f, 100.f }, owner{ 30.f, 0.f, 100.f };
    CHECK(PlanFollowDrop(bot, owner, false, wide, AlwaysClear).mode == Mode::None);
}

TEST_CASE("Follow drop: a bot that walked along the rim still finds the edge toward the owner", "[ClassicFollowDrop]")
{
    Vec const bot{ 110.f, -392.f, 122.f }, owner{ 148.f, -420.f, 110.9f };
    Plan const p = PlanFollowDrop(bot, owner, false, Stadium, AlwaysClear);
    REQUIRE(p.mode == Mode::Walk);
    CHECK(p.land.z == Catch::Approx(110.9f).margin(0.01));
}

TEST_CASE("Follow drop: an edge sideways of the owner (bearing 90 degrees off) is found", "[ClassicFollowDrop]")
{
    // The edge is behind the bot's right shoulder: only the wide bearings reach it.
    auto side = [](float x, float, float hint, float& z)
    {
        float const floor_z = x > 20.f ? 110.0f : 122.0f;
        if (floor_z > hint + 3.0f)
            return false;
        z = floor_z;
        return true;
    };
    Vec const bot{ 10.f, 0.f, 122.f }, owner{ 12.f, 40.f, 110.f };
    // The owner is 40 y north on the low floor of a ledge that runs along x = 20, i.e. due east of the bot.
    Plan const p = PlanFollowDrop(bot, owner, false, side, AlwaysClear);
    REQUIRE(p.mode == Mode::Walk);
    CHECK(p.ledge.x == Catch::Approx(20.f).margin(1.6));
}
