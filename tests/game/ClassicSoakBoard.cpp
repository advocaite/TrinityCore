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

#include "../../src/modules/PlayerbotV2/Travel/SoakBoard.h"

using namespace Playerbot::V2::Travel;

namespace
{
SoakGoal Goal(std::uint32_t map = 0, float x = -8900, float y = 600)
{
    SoakGoal g;
    g.id = 1; g.map = map; g.x = x; g.y = y; g.z = 100; g.name = "Stormwind"; g.started_ms = 1000; g.timeout_ms = 60000;
    return g;
}
}

TEST_CASE("SoakBoard: a bot has one assignment at a time", "[soakboard]")
{
    SoakBoard b;
    CHECK(b.Assign(7, Goal()));
    CHECK_FALSE(b.Assign(7, Goal(1)));
    CHECK(b.Has(7));
    CHECK(b.Active() == 1);
    SoakGoal g;
    REQUIRE(b.Get(7, g));
    CHECK(g.map == 0);
    CHECK_FALSE(b.Has(8));
}

TEST_CASE("SoakBoard: arriving means the goal's map and 25 y in x, y", "[soakboard]")
{
    SoakGoal const g = Goal(1, 100, 200);
    CHECK(SoakArrived(g, 1, 110, 210));
    CHECK(SoakArrived(g, 1, 100 + 25, 200));
    CHECK_FALSE(SoakArrived(g, 1, 100 + 26, 200));
    CHECK_FALSE(SoakArrived(g, 0, 100, 200));   // right place, wrong map
}

TEST_CASE("SoakBoard: a leg times out after its time and the clock may wrap", "[soakboard]")
{
    SoakGoal g = Goal();
    CHECK_FALSE(SoakTimedOut(g, 1000 + 59999));
    CHECK(SoakTimedOut(g, 1000 + 60000));
    g.started_ms = 0xFFFFFF00u;
    CHECK_FALSE(SoakTimedOut(g, 100));
    CHECK(SoakTimedOut(g, 0xFFFFFF00u + 60000u));
}

TEST_CASE("SoakBoard: finishing moves the leg to the outcomes once and counts it", "[soakboard]")
{
    SoakBoard b;
    b.Assign(7, Goal());
    b.Finish(7, SoakResult::TimedOut, 5000, 0, -8800.f, 700.f, 110.f, 3, 1);
    b.Finish(7, SoakResult::Arrived, 5100, 0, -8900.f, 600.f, 100.f, 3, 3);   // ignored: the leg is over
    CHECK_FALSE(b.Has(7));
    auto const out = b.Drain();
    REQUIRE(out.size() == 1);
    CHECK(out[0].guid == 7);
    CHECK(out[0].result == SoakResult::TimedOut);
    CHECK(out[0].duration_ms == 4000);
    CHECK(out[0].plan_legs == 3);
    CHECK(out[0].leg_index == 1);
    CHECK(b.Drain().empty());
    CHECK(b.Total(SoakResult::TimedOut) == 1);
    CHECK(b.Total(SoakResult::Arrived) == 0);
}

TEST_CASE("SoakBoard: legs that stop are grouped by the 50 y cell they stop in", "[soakboard]")
{
    SoakBoard b;
    for (std::uint64_t i = 1; i <= 3; ++i)
    {
        b.Assign(i, Goal());
        b.Finish(i, SoakResult::TimedOut, 5000, 0, 10.f + float(i), 20.f, 0.f, 1, 0);   // all in cell 0/0/0
    }
    b.Assign(9, Goal());
    b.Finish(9, SoakResult::NoRoute, 5000, 1, -60.f, 20.f, 0.f, 0, 0);                  // cell 1/-2/0
    b.Assign(10, Goal());
    b.Finish(10, SoakResult::Arrived, 5000, 0, 500.f, 500.f, 0.f, 1, 1);                // arrivals are not stops
    auto const top = b.TopStops(5);
    REQUIRE(top.size() == 2);
    CHECK(top[0].first == "0/0/0");
    CHECK(top[0].second == 3);
    CHECK(top[1].first == "1/-2/0");
    CHECK(SoakCell(0, -0.5f, 49.9f) == "0/-1/0");
}

TEST_CASE("SoakBoard: a cancelled leg leaves no outcome", "[soakboard]")
{
    SoakBoard b;
    b.Assign(7, Goal());
    b.Cancel(7);
    CHECK_FALSE(b.Has(7));
    CHECK(b.Drain().empty());
}
