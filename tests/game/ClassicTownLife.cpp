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

#include "../../src/modules/PlayerbotV2/Bot/TownLife.h"

using namespace Playerbot::V2::TownLife;

TEST_CASE("Taking part follows the share, a different set each window", "[ClassicTownLife]")
{
    unsigned in_a = 0, in_b = 0, both = 0;
    for (unsigned id = 0; id < 5000; ++id)
    {
        bool const a = TakesPart(id, 1, 1000u, 10u, 20u * 60000u);
        bool const b = TakesPart(id, 1, 1000u + 20u * 60000u, 10u, 20u * 60000u);
        in_a += a; in_b += b; both += (a && b);
    }
    CHECK(in_a > 400);   // about 10% of 5000
    CHECK(in_a < 600);
    CHECK(in_b > 400);
    CHECK(in_b < 600);
    CHECK(both < 120);   // not the same bots again
    CHECK_FALSE(TakesPart(7, 1, 1000u, 0u, 1000u));
    CHECK(TakesPart(7, 1, 1000u, 100u, 1000u));
}

TEST_CASE("Most bots rest at the inn at night, few by day", "[ClassicTownLife]")
{
    CHECK(InnPercent(2) == 35);
    CHECK(InnPercent(23) == 35);
    CHECK(InnPercent(20) == 20);
    CHECK(InnPercent(12) == 8);
}

TEST_CASE("Duel partners are close in level and healthy; one side challenges", "[ClassicTownLife]")
{
    CHECK(DuelPartners(30, 100, 32, 95, 10, 20));
    CHECK_FALSE(DuelPartners(30, 100, 32, 95, 20, 10));   // the higher id waits
    CHECK_FALSE(DuelPartners(30, 100, 40, 100, 10, 20));  // too far apart in level
    CHECK_FALSE(DuelPartners(30, 80, 31, 100, 10, 20));   // hurt
}

TEST_CASE("An LFG call names a dungeon of the bot's level", "[ClassicTownLife]")
{
    CHECK(LfgDungeonFor(5, 0) == nullptr);
    CHECK(LfgDungeonFor(19, 0) != nullptr);
    CHECK(std::string(LfgDungeonFor(13, 0)) == "Ragefire Chasm");
    CHECK(LfgDungeonFor(60, 3) != nullptr);
}

TEST_CASE("A duel pact holds for the pair that made it, for a while", "[ClassicTownLife]")
{
    PactSet(200, 100, 1000u);
    CHECK(PactHolds(200, 100, 5000u));
    CHECK_FALSE(PactHolds(200, 101, 5000u));
    CHECK_FALSE(PactHolds(200, 100, 40000u));   // expired
    CHECK_FALSE(PactHolds(300, 100, 5000u));
}
