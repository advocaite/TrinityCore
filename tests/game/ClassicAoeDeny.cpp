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
#include "tc_catch2.h"

#include "../../src/modules/PlayerbotV2/Bot/AoeDeny.h"

using namespace Playerbot::AoeDeny;

TEST_CASE("AoeDeny: what aoe off leaves out", "[aoedeny]")
{
    // harmful area or cone spells, damaging ground areas: Blizzard, Arcane Explosion, Consecration, Whirlwind
    CHECK(IsAoe(false, true, 0, 10));
    // chained: Chain Lightning, Multi-Shot, Cleave
    CHECK(IsAoe(false, false, 3, 421));
    CHECK(IsAoe(false, false, 2, 845));
    // the area totems, whose summon spell is not harmful
    CHECK(IsAoe(true, false, 0, 8190));
    CHECK(IsAoe(true, false, 0, 1535));
}
TEST_CASE("AoeDeny: what aoe off keeps", "[aoedeny]")
{
    CHECK_FALSE(IsAoe(false, false, 0, 686));    // Shadow Bolt
    CHECK_FALSE(IsAoe(false, false, 1, 403));    // Lightning Bolt (one target)
    CHECK_FALSE(IsAoe(true, true, 0, 596));      // Prayer of Healing: a friendly area
    CHECK_FALSE(IsAoe(true, true, 0, 6673));     // Battle Shout
    CHECK_FALSE(IsAoe(true, false, 0, 3599));    // Searing Totem
}
