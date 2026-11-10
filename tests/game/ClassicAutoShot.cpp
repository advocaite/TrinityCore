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

#include "../../src/server/game/Spells/ClassicAutoShot.h"

using namespace ClassicAutoShot;

TEST_CASE("Auto Shot waits half a second after the hunter stops or turns it on", "[ClassicAutoShot]")
{
    CHECK_FALSE(IsWindupDone(10000, 10000));
    CHECK_FALSE(IsWindupDone(10499, 10000));
    CHECK(IsWindupDone(10500, 10000));
    CHECK(IsWindupDone(60000, 10000));   // standing still long ago: the swing timer alone decides
}

TEST_CASE("The wind-up is right across the wrap of the millisecond clock", "[ClassicAutoShot]")
{
    uint32_t const still = 0xFFFFFF00u;
    CHECK_FALSE(IsWindupDone(still + 100u, still));           // wrapped to a small number, 100 ms later
    CHECK(IsWindupDone(uint32_t(still + 600u), still));
}

TEST_CASE("No ammo, a wrong weapon or a dead or friendly target stops Auto Shot", "[ClassicAutoShot]")
{
    for (SpellCastResult r : { SPELL_FAILED_NO_AMMO, SPELL_FAILED_NEED_AMMO, SPELL_FAILED_EQUIPPED_ITEM, SPELL_FAILED_EQUIPPED_ITEM_CLASS,
                               SPELL_FAILED_TARGETS_DEAD, SPELL_FAILED_TARGET_FRIENDLY, SPELL_FAILED_BAD_TARGETS })
    {
        INFO("result " << int(r));
        CHECK(OnFailedShot(r) == FailAction::Stop);
    }
}

TEST_CASE("Range, facing and line of sight only pause Auto Shot", "[ClassicAutoShot]")
{
    for (SpellCastResult r : { SPELL_FAILED_OUT_OF_RANGE, SPELL_FAILED_TOO_CLOSE, SPELL_FAILED_UNIT_NOT_INFRONT, SPELL_FAILED_LINE_OF_SIGHT,
                               SPELL_FAILED_DONT_REPORT, SPELL_FAILED_MOVING })
    {
        INFO("result " << int(r));
        CHECK(OnFailedShot(r) == FailAction::Wait);
    }
}

TEST_CASE("A failed shot's reason is told once, not every update", "[ClassicAutoShot]")
{
    SpellCastResult last = SPELL_CAST_OK;
    CHECK(ShouldReport(SPELL_FAILED_OUT_OF_RANGE, last));
    last = SPELL_FAILED_OUT_OF_RANGE;
    CHECK_FALSE(ShouldReport(SPELL_FAILED_OUT_OF_RANGE, last));   // the next update, same reason: silent
    CHECK(ShouldReport(SPELL_FAILED_LINE_OF_SIGHT, last));        // a new reason is told
    CHECK(ShouldReport(SPELL_FAILED_OUT_OF_RANGE, SPELL_CAST_OK)); // after a shot left (reset to OK) it is told again
    CHECK_FALSE(ShouldReport(SPELL_FAILED_DONT_REPORT, SPELL_CAST_OK));
    CHECK_FALSE(ShouldReport(SPELL_CAST_OK, SPELL_FAILED_OUT_OF_RANGE));
}
