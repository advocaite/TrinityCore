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

#include "../../src/modules/PlayerbotV2/Fleet/PetSnap.h"

using namespace Playerbot::V2::Fleet;

TEST_CASE("A pet with no way to its owner is snapped on the third look", "[ClassicPetSnap]")
{
    PetSnapState st;
    CHECK_FALSE(PetSnapUpdate(st, true, 1000u));
    CHECK_FALSE(PetSnapUpdate(st, true, 2000u));
    CHECK(PetSnapUpdate(st, true, 3000u));
}

TEST_CASE("A path to the owner clears the strikes", "[ClassicPetSnap]")
{
    PetSnapState st;
    CHECK_FALSE(PetSnapUpdate(st, true, 1000u));
    CHECK_FALSE(PetSnapUpdate(st, true, 2000u));
    CHECK_FALSE(PetSnapUpdate(st, false, 3000u));
    CHECK_FALSE(PetSnapUpdate(st, true, 4000u));
    CHECK_FALSE(PetSnapUpdate(st, true, 5000u));
    CHECK(PetSnapUpdate(st, true, 6000u));
}

TEST_CASE("A pet is snapped at most every four seconds", "[ClassicPetSnap]")
{
    PetSnapState st;
    PetSnapUpdate(st, true, 1000u);
    PetSnapUpdate(st, true, 2000u);
    REQUIRE(PetSnapUpdate(st, true, 3000u));
    CHECK_FALSE(PetSnapUpdate(st, true, 4000u));   // 1 s after the snap
    CHECK_FALSE(PetSnapUpdate(st, true, 6000u));   // 3 s after
    CHECK(PetSnapUpdate(st, true, 7000u));         // 4 s after
}

TEST_CASE("The look interval is a second", "[ClassicPetSnap]")
{
    PetSnapState st;
    PetSnapUpdate(st, false, 5000u);
    CHECK_FALSE(PetSnapDue(st, 5500u));
    CHECK(PetSnapDue(st, 6000u));
}
