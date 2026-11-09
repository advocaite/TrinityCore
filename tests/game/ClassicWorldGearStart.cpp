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

#include "../../src/modules/PlayerbotV2/Bot/Gear/WorldGearStart.h"

using namespace Playerbot::Gear;

TEST_CASE("A world bot below level 10 gets no generated gear", "[ClassicWorldGearStart]")
{
    CHECK_FALSE(GeneratesGear(true, 1));
    CHECK_FALSE(GeneratesGear(true, 9));
    CHECK(GeneratesGear(true, 10));
    CHECK(GeneratesGear(true, 60));
}

TEST_CASE("A player's companion is geared at every level", "[ClassicWorldGearStart]")
{
    CHECK(GeneratesGear(false, 1));
    CHECK(GeneratesGear(false, 9));
    CHECK(GeneratesGear(false, 40));
}

TEST_CASE("A level 1-9 world bot gets a replacement for a starter piece the client cannot draw", "[ClassicWorldGearStart]")
{
    CHECK(GeneratesGear(true, 1, true));
    CHECK(GeneratesGear(true, 9, true));
}
