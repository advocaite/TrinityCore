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

#include "../../src/modules/PlayerbotV2/Bot/Raid/RaidHealSlots.h"
#include <algorithm>

using namespace Playerbot::Raid;

TEST_CASE("four slots for four tanks: the main tank gets two healers", "[classic][heal]")
{
    auto const s = HealSlotOrder(1, 2, { 3, 4 }, 4);
    REQUIRE(s.size() == 4);
    CHECK(std::count(s.begin(), s.end(), 1) == 2);
    CHECK(std::count(s.begin(), s.end(), 2) == 1);
    CHECK(std::count(s.begin(), s.end(), 3) == 1);
    CHECK(std::count(s.begin(), s.end(), 4) == 0);
}

TEST_CASE("a single slot goes to the main tank", "[classic][heal]")
{
    auto const s = HealSlotOrder(1, 2, { 3 }, 1);
    REQUIRE(s.size() == 1);
    CHECK(s[0] == 1);
}

TEST_CASE("a full budget gives main 3, off 2 and every spare 1", "[classic][heal]")
{
    auto const s = HealSlotOrder(1, 2, { 3, 4 }, 99);
    REQUIRE(s.size() == 7);
    CHECK(std::count(s.begin(), s.end(), 1) == 3);
    CHECK(std::count(s.begin(), s.end(), 2) == 2);
    CHECK(std::count(s.begin(), s.end(), 3) == 1);
    CHECK(std::count(s.begin(), s.end(), 4) == 1);
}

TEST_CASE("without an off tank the main tank still comes first, absent tanks add nothing", "[classic][heal]")
{
    auto const s = HealSlotOrder(1, 0, { 3 }, 3);
    REQUIRE(s.size() == 3);
    CHECK(s[0] == 1);
    CHECK(s[1] == 1);
    CHECK(s[2] == 3);
    CHECK(HealSlotOrder(0, 0, {}, 5).empty());
}
