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

#include "../../src/modules/PlayerbotV2/Travel/EntrancePockets.h"

#include <utility>
#include <vector>

using namespace Playerbot::V2::Travel;

TEST_CASE("Travel: a lone dungeon entrance is a pocket", "[playerbot][travel][entrance]")
{
    // 0 Chillwind Camp, 1 Andorhal hub (walk-linked), 2 Entrance to Scholomance (no walk edge).
    std::vector<bool> const is_entrance{ false, false, true };
    auto const pocket = FindEntrancePockets(is_entrance, { { 0, 1 } });
    CHECK_FALSE(pocket[0]);
    CHECK_FALSE(pocket[1]);
    CHECK(pocket[2]);
}

TEST_CASE("Travel: entrances joined only to each other are a pocket", "[playerbot][travel][entrance]")
{
    // 0 Cenarion Hold FP, 1 Cenarion Hold hub, 2 Ruins of Ahn'Qiraj, 3 Ahn'Qiraj Temple.
    std::vector<bool> const is_entrance{ false, false, true, true };
    auto const pocket = FindEntrancePockets(is_entrance, { { 0, 1 }, { 2, 3 } });
    CHECK_FALSE(pocket[0]);
    CHECK_FALSE(pocket[1]);
    CHECK(pocket[2]);
    CHECK(pocket[3]);
}

TEST_CASE("Travel: an entrance walk-linked to a real place is not a pocket", "[playerbot][travel][entrance]")
{
    // 0 Thorium Point FP, 1 Entrance to Blackrock Depths, 2 Entrance to Blackrock Spire (via 1).
    std::vector<bool> const is_entrance{ false, true, true };
    auto const pocket = FindEntrancePockets(is_entrance, { { 1, 2 }, { 0, 1 } });
    CHECK_FALSE(pocket[0]);
    CHECK_FALSE(pocket[1]);
    CHECK_FALSE(pocket[2]);
}

TEST_CASE("Travel: edges out of range are ignored", "[playerbot][travel][entrance]")
{
    std::vector<bool> const is_entrance{ true, false };
    auto const pocket = FindEntrancePockets(is_entrance, { { 0, 7 } });
    CHECK(pocket[0]);
    CHECK_FALSE(pocket[1]);
}

TEST_CASE("Travel: a pocket entrance keeps its bridge only when no real place can serve it", "[playerbot][travel][entrance]")
{
    constexpr float kFar = 1500.0f;
    CHECK_FALSE(KeepIslandBridge(true, 1174.0f, kFar));   // Scholomance: Chillwind Camp is in reach, no bridge to Aerie Peak
    CHECK_FALSE(KeepIslandBridge(true, 1472.0f, kFar));   // Ahn'Qiraj: Cenarion Hold is in reach
    CHECK(KeepIslandBridge(true, 2400.0f, kFar));         // far from everything: the old bridge stays
    CHECK(KeepIslandBridge(false, 0.0f, kFar));           // any other island node is bridged as before
}
