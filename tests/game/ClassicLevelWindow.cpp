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

#include "../../src/modules/PlayerbotV2/Fleet/LevelWindow.h"

using namespace Playerbot::V2::Fleet;

TEST_CASE("A starter zone gets its own range, never level 10s", "[ClassicLevelWindow]")
{
    LevelWindow w = AnchorLevelWindow(1, true, 1, 6, 10, 60);
    CHECK(w.lo == 1);
    CHECK(w.hi == 5);
    w = AnchorLevelWindow(5, true, 1, 6, 10, 60);
    CHECK(w.lo == 1);
    CHECK(w.hi == 7);
}

TEST_CASE("The window leans toward the player's level inside the zone's range", "[ClassicLevelWindow]")
{
    LevelWindow w = AnchorLevelWindow(33, true, 30, 45, 10, 60);
    CHECK(w.lo == 29);
    CHECK(w.hi == 37);
    w = AnchorLevelWindow(44, true, 30, 45, 10, 60);
    CHECK(w.lo == 40);
    CHECK(w.hi == 46);
}

TEST_CASE("A player far outside the zone's range gets the nearest part of it", "[ClassicLevelWindow]")
{
    LevelWindow w = AnchorLevelWindow(1, true, 30, 45, 10, 60);
    CHECK(w.lo == 29);
    CHECK(w.hi == 29);
    w = AnchorLevelWindow(60, true, 10, 20, 10, 60);
    CHECK(w.lo == 21);
    CHECK(w.hi == 21);
}

TEST_CASE("A town without a level range keeps the wide spread", "[ClassicLevelWindow]")
{
    LevelWindow const w = AnchorLevelWindow(1, false, 0, 0, 10, 60);
    CHECK(w.lo == 10);
    CHECK(w.hi == 60);
}
