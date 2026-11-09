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

#include "../../src/server/scripts/Custom/Classic/world/ClassicPriestSpells.h"

using namespace ClassicPriestSpells;

TEST_CASE("Priest rank spells follow the client data", "[ClassicPriestSpells]")
{
    CHECK(Find(TouchOfWeakness, 2652) == 2943);
    CHECK(Find(TouchOfWeakness, 19266) == 19254);
    CHECK(Find(Shadowguard, 18137) == 28377);
    CHECK(Find(Shadowguard, 19312) == 28382);
    CHECK(Find(HolyNova, 15237) == 23455);
    CHECK(Find(HolyNova, 27801) == 27805);
    CHECK(Find(HolyNova, 132157) == 0);
}
