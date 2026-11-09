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

#include "../../src/modules/PlayerbotV2/Bot/InstanceFullHold.h"

using namespace Playerbot::V2;

TEST_CASE("InstanceFullHold: a full refusal holds the bot for the hold time", "[ClassicInstanceFullHold]")
{
    CHECK(InstanceFullHolds(229, 1000, 229, 1000, 30000));
    CHECK(InstanceFullHolds(229, 1000, 229, 30999, 30000));
    CHECK_FALSE(InstanceFullHolds(229, 1000, 229, 31000, 30000));   // time to ask again
}

TEST_CASE("InstanceFullHold: no refusal on record, or another map, holds nothing", "[ClassicInstanceFullHold]")
{
    CHECK_FALSE(InstanceFullHolds(0, 0, 229, 5000, 30000));
    CHECK_FALSE(InstanceFullHolds(229, 1000, 469, 2000, 30000));   // refused for UBRS, now rejoining Blackwing Lair
}

TEST_CASE("InstanceFullHold: a wrapped millisecond clock still counts the elapsed time", "[ClassicInstanceFullHold]")
{
    CHECK(InstanceFullHolds(229, 0xFFFFFF00u, 229, 0x00000100u, 30000));
    CHECK_FALSE(InstanceFullHolds(229, 0xFFFFFF00u, 229, 0x00010000u, 30000));
}
