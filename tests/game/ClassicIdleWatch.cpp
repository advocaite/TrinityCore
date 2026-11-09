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

#include "../../src/modules/PlayerbotV2/Bot/IdleWatch.h"

#include <set>

using namespace Playerbot::V2;

TEST_CASE("IdleWatch: a bot standing still is found stuck after 45 seconds, whatever rule holds it", "[idlewatch]")
{
    IdleWatch w;
    std::uint32_t found_at = 0;
    for (std::uint32_t t = 1000; t < 200000 && !found_at; t += 500)
        if (IdleStuck(w, 10.f, 20.f, t, false))
            found_at = t;
    REQUIRE(found_at != 0);
    CHECK(found_at - 1000 >= kIdleStuckMs);
    CHECK(found_at - 1000 <= kIdleStuckMs + 1000);
    CHECK(w.strikes == 1);
}

TEST_CASE("IdleWatch: a walking bot, or one with a reason to stand, is never found stuck", "[idlewatch]")
{
    IdleWatch walking;
    for (std::uint32_t t = 1000; t < 600000; t += 500)
        REQUIRE_FALSE(IdleStuck(walking, float(t) / 100.f, 0.f, t, false));
    IdleWatch fighting;
    for (std::uint32_t t = 1000; t < 600000; t += 500)
        REQUIRE_FALSE(IdleStuck(fighting, 5.f, 5.f, t, true));     // in combat, casting, resting, grouped...
}

TEST_CASE("IdleWatch: a reason to stand restarts the clock, and strikes climb while it stays stuck", "[idlewatch]")
{
    IdleWatch w;
    std::uint32_t t = 1000;
    for (; t < 40000; t += 500) IdleStuck(w, 0.f, 0.f, t, false);
    IdleStuck(w, 0.f, 0.f, t, true);                                // a fight at 40 s
    bool early = false;
    for (std::uint32_t u = t + 500; u < t + 40000; u += 500) early |= IdleStuck(w, 0.f, 0.f, u, false);
    CHECK_FALSE(early);                                              // the 45 s start over after the fight
    int found = 0;
    for (std::uint32_t u = t + 40000; u < t + 300000; u += 500) found += IdleStuck(w, 0.f, 0.f, u, false);
    CHECK(found >= 4);
    CHECK(w.strikes == std::uint32_t(found) + 0u);
    IdleStuck(w, 200.f, 0.f, t + 400000, false);                    // it went 200 yards: next time starts from the first activity
    CHECK(w.strikes == 0);
}

TEST_CASE("IdleWatch: strikes and bot ids spread a stuck crowd over different hubs", "[idlewatch]")
{
    std::set<std::size_t> picks;
    for (std::uint32_t bot = 0; bot < 40; ++bot)
        picks.insert(IdlePickHub(9, 1, bot));
    CHECK(picks.size() == kIdleHubChoices);
    CHECK(IdlePickHub(9, 1, 7) != IdlePickHub(9, 2, 7));            // the same bot, stuck again: another hub
    CHECK(IdlePickHub(0, 3, 7) == 0);
    CHECK(IdlePickHub(2, 5, 9) < 2);
}

TEST_CASE("IdleWatch: only the rules that walk toward a quest objective are objective rules", "[idlewatch]")
{
    using Playerbot::V2::IsObjectiveRule;
    CHECK(IsObjectiveRule("idle:pursue_quest_goal"));
    CHECK(IsObjectiveRule("idle:far_same_map_travel"));
    CHECK_FALSE(IsObjectiveRule("idle:watch_walk"));
    CHECK_FALSE(IsObjectiveRule("idle:travel_to_hub"));
    CHECK_FALSE(IsObjectiveRule("idle:pursue_quest"));
    CHECK_FALSE(IsObjectiveRule(nullptr));
}

TEST_CASE("A walk that never gets closer to its goal is caught; a real route or a break is not", "[ClassicIdleWatch]")
{
    using namespace Playerbot::V2;
    GoalProgress g;
    // Back and forth in a room 1650 y from the goal, a step every second.
    bool caught = false;
    std::uint32_t t = 1000;
    for (; t < 1000 + kGoalProgressMs + 2000; t += 1000)
        caught |= GoalStalled(g, 7, 1650.f + float(t % 4000) / 200.f, t);
    REQUIRE(caught);

    // Closing 7 y a second: never caught.
    GoalProgress h;
    for (std::uint32_t u = 1000; u < 400000; u += 1000)
        REQUIRE_FALSE(GoalStalled(h, 7, 3000.f - float(u) / 1000.f * 7.f, u));

    // A long fight in the middle: the clock starts again when the walk resumes.
    GoalProgress f;
    REQUIRE_FALSE(GoalStalled(f, 7, 900.f, 1000u));
    REQUIRE_FALSE(GoalStalled(f, 7, 900.f, 1000u + kGoalProgressMs + 5000u));
    // A new goal starts from scratch.
    REQUIRE_FALSE(GoalStalled(f, 8, 900.f, 1000u + 2u * kGoalProgressMs));
}
