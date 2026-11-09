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

#include "../../src/modules/PlayerbotV2/Bot/ConjureGate.h"
#include "../../src/modules/PlayerbotV2/Bot/RaidFight.h"

using namespace Playerbot::V2;

TEST_CASE("RaidFight: the owner's Attack counts for the window after it was spoken", "[ClassicRaidFight]")
{
    CHECK_FALSE(OwnerAttackRecent(5000, 0, 10000));        // never spoken
    CHECK(OwnerAttackRecent(5000, 4000, 10000));
    CHECK(OwnerAttackRecent(13999, 4000, 10000));
    CHECK_FALSE(OwnerAttackRecent(14000, 4000, 10000));    // the window has run out
    CHECK_FALSE(OwnerAttackRecent(3000, 4000, 10000));     // a clock that went backwards is not a recent Attack
}

TEST_CASE("RaidFight: a ranged DPS with its target in reach and in sight casts instead of walking to its slot", "[ClassicRaidFight]")
{
    CHECK(CastsInsteadOfSlotWalk(true, true, true, 15.5f, 2.0f));      // the live case: Cassanny, target 15.5 y, LoS
    CHECK(CastsInsteadOfSlotWalk(true, true, true, 30.0f, 2.0f));      // inside 28 + the target's reach
    CHECK_FALSE(CastsInsteadOfSlotWalk(true, true, true, 31.0f, 2.0f));
    CHECK_FALSE(CastsInsteadOfSlotWalk(true, true, false, 10.0f, 2.0f));   // no line of sight: the LoS step owns it
    CHECK_FALSE(CastsInsteadOfSlotWalk(true, false, true, 10.0f, 2.0f));   // no target: waiting for the Attack
    CHECK_FALSE(CastsInsteadOfSlotWalk(false, true, true, 10.0f, 2.0f));   // healers and melee keep their own rules
}

TEST_CASE("ConjureGate: a conjure is made out of combat and only with the mana to pay for it", "[ClassicRaidFight]")
{
    CHECK(ConjureManaCost(0, 0.0f, 1000) == 0);
    CHECK(ConjureManaCost(80, 0.0f, 1000) == 80);
    CHECK(ConjureManaCost(0, 7.0f, 1000) == 70);       // percent of base mana
    CHECK(MayConjure(false, false, 300, 70));
    CHECK(MayConjure(false, false, 70, 70));
    CHECK_FALSE(MayConjure(false, false, 69, 70));     // NO_POWER: the live warlock at low mana
    CHECK_FALSE(MayConjure(true, true, 900, 70));      // in combat
    CHECK_FALSE(MayConjure(false, true, 900, 70));     // the moment after combat
    // A mage drinks before it conjures (UBRS 2026-10-29: a Mana Ruby at 4 % mana between two pulls).
    CHECK(MageConjureWaitsForMana(4));
    CHECK(MageConjureWaitsForMana(kMageConjureMinManaPct - 1));
    CHECK_FALSE(MageConjureWaitsForMana(kMageConjureMinManaPct));
    CHECK_FALSE(MageConjureWaitsForMana(100));
}
