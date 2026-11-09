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

#ifdef TESTS_HAVE_PLAYERBOT_V2

#include "../../src/modules/PlayerbotV2/Bot/DrinkSay.h"
#include <string>

using namespace Playerbot::Classic;

TEST_CASE("DrinkSay: a bot led by a player announces a real drink once per quiet period", "[DrinkSay]")
{
    CHECK(ShouldSayDrinking(true, true, 30, false));
    CHECK(ShouldSayDrinking(true, true, kDrinkSayBelowManaPct - 1, false));
    CHECK_FALSE(ShouldSayDrinking(true, true, kDrinkSayBelowManaPct, false));   // a top-up near full is not announced
    CHECK_FALSE(ShouldSayDrinking(true, true, 30, true));                      // said within the quiet time
}

TEST_CASE("DrinkSay: once per rest, not for the next item or a sit again after a step", "[DrinkSay]")
{
    CHECK(SameRest(0));
    CHECK(SameRest(kDrinkSayRestGapMs - 1));
    CHECK_FALSE(SameRest(kDrinkSayRestGapMs));
    CHECK_FALSE(SameRest(~0u));   // never rested
}

TEST_CASE("DrinkSay: only mana users in a player's group say it", "[DrinkSay]")
{
    CHECK_FALSE(ShouldSayDrinking(false, true, 30, false));   // a world bot or a bot-only group
    CHECK_FALSE(ShouldSayDrinking(true, false, 30, false));   // no mana pool
}

TEST_CASE("DrinkSay: the line is stable per bot and always one of the three", "[DrinkSay]")
{
    for (uint64_t g = 1; g < 50; ++g)
    {
        std::string const a = DrinkSayText(g);
        CHECK(a == DrinkSayText(g));
        CHECK((a == "drinking" || a == "drinking, one moment" || a == "need to drink"));
    }
    CHECK(std::string(DrinkSayText(3)) != std::string(DrinkSayText(4)));
}

#endif
