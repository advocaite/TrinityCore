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

#include "../../src/modules/PlayerbotV2/Fleet/TownGather.h"

using namespace Playerbot::V2::Fleet;

TEST_CASE("A bank and an auction house make a gathering spot; a room of trainers does not", "[ClassicTownGather]")
{
    std::vector<GatherNpc> npcs;
    npcs.push_back({ -4900.f, -1000.f, 500.f, 4.0f });   // banker
    npcs.push_back({ -4905.f, -1005.f, 500.f, 4.0f });   // auctioneer
    for (int i = 0; i < 8; ++i)
        npcs.push_back({ -4750.f + float(i), -1100.f, 500.f, 0.25f });   // a ward's trainers: 2.0 in all
    auto const spots = BuildGatherSpots(npcs);
    REQUIRE(spots.size() == 1);
    CHECK(spots[0].weight == 8.0f);
    CHECK(spots[0].x > -4906.f);
    CHECK(spots[0].x < -4899.f);
}

TEST_CASE("Spots are sorted heaviest first and the pick follows the weight", "[ClassicTownGather]")
{
    std::vector<GatherNpc> npcs;
    npcs.push_back({ 0.f, 0.f, 0.f, 4.0f });
    npcs.push_back({ 505.f, 0.f, 0.f, 4.0f });
    npcs.push_back({ 510.f, 0.f, 0.f, 4.0f });
    auto const spots = BuildGatherSpots(npcs);
    REQUIRE(spots.size() == 2);
    CHECK(spots[0].weight == 8.0f);
    CHECK(PickGatherSpot(spots, 0.0f) == 0);
    CHECK(PickGatherSpot(spots, 0.66f) == 0);   // 8 of 12 = 0.667
    CHECK(PickGatherSpot(spots, 0.7f) == 1);
    CHECK(PickGatherSpot({}, 0.5f) == -1);
}
