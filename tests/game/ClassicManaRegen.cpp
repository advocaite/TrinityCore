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

#include "../../src/server/game/Entities/Unit/ClassicManaRegen.h"

TEST_CASE("Spirit regeneration is interrupted for 5 seconds after a mana spell", "[ClassicManaRegen]")
{
    CHECK(ClassicManaRegen::IsInterrupted(0, false));
    CHECK(ClassicManaRegen::IsInterrupted(4999, false));
}

TEST_CASE("Spirit regeneration runs at the full rate from the fifth second, combat does not matter", "[ClassicManaRegen]")
{
    CHECK_FALSE(ClassicManaRegen::IsInterrupted(5000, false));
    CHECK_FALSE(ClassicManaRegen::IsInterrupted(60000, false));
}

TEST_CASE("An interrupting aura keeps the regeneration interrupted", "[ClassicManaRegen]")
{
    CHECK(ClassicManaRegen::IsInterrupted(60000, true));
}
