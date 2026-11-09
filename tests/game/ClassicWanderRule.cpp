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

#include "../../src/modules/PlayerbotV2/Bot/WanderRule.h"

using namespace Playerbot::V2;

TEST_CASE("WanderRule: a world bot is never held still by rest", "[wanderrule]")
{
    CHECK(WanderAllowedByRest(5000, /*truly_idle*/ true, /*has_owner*/ false));
    CHECK(WanderAllowedByRest(0, true, false));
}

TEST_CASE("WanderRule: a rested companion with nothing to do still rests", "[wanderrule]")
{
    CHECK_FALSE(WanderAllowedByRest(5000, true, true));
    CHECK(WanderAllowedByRest(0, true, true));
    CHECK(WanderAllowedByRest(5000, false, true));    // it has work: the old rule is unchanged
}

TEST_CASE("WanderRule: a sick world bot steps about, a companion or a moving bot does not", "[wanderrule]")
{
    int steps = 0;
    for (std::uint32_t sec = 0; sec < 60; ++sec)
        if (ResSickStepNow(false, false, sec * 1000u, 17u)) ++steps;
    CHECK(steps == 10);                                                     // once every six seconds
    CHECK_FALSE(ResSickStepNow(true, false, 6000u * 4u - 17u * 1000u, 17u)); // owned or grouped: never
    for (std::uint32_t sec = 0; sec < 12; ++sec)
        CHECK_FALSE(ResSickStepNow(false, true, sec * 1000u, 17u));         // already walking
    // different bots step in different seconds, so a crowd does not move in lockstep
    int together = 0;
    for (std::uint32_t id = 0; id < 60; ++id)
        if (ResSickStepNow(false, false, 6000u, id)) ++together;
    CHECK(together == 10);
}
