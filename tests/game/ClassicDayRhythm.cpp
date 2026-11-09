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

#include "../../src/modules/PlayerbotV2/Fleet/DayRhythm.h"

using namespace Playerbot::V2::Fleet;

TEST_CASE("The night is quiet, the evening is busy", "[ClassicDayRhythm]")
{
    CHECK(RhythmPercent(3) == 60);
    CHECK(RhythmPercent(12) == 100);
    CHECK(RhythmPercent(20) == 125);
    CHECK(RhythmPercent(23) == 80);
    CHECK(RhythmPercent(-1) == 100);
    CHECK(RhythmPercent(24) == 100);
}

TEST_CASE("The target follows the hour but never passes the world cap", "[ClassicDayRhythm]")
{
    CHECK(RhythmTarget(60, 3, 220) == 36);
    CHECK(RhythmTarget(60, 20, 220) == 75);
    CHECK(RhythmTarget(200, 20, 220) == 220);   // 250 capped
    CHECK(RhythmTarget(1, 3, 220) == 1);        // a nonzero target stays nonzero
    CHECK(RhythmTarget(0, 20, 220) == 0);
}
