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

#include "../../src/modules/PlayerbotV2/Bot/TankHazardHold.h"

using namespace Playerbot;

TEST_CASE("TankHazardHold: a tank stays put for a cast that depends on the mob's facing", "[tankhazardhold]")
{
    CHECK(TankHoldsWhileMobCasts(/*zone_is_this_spell*/ false, /*ground_area_on_victim*/ false));
}

TEST_CASE("TankHazardHold: a tank does not stay for a cast that lands on its own spot", "[tankhazardhold]")
{
    // The Defias Evoker's Flamestrike: the zone under the tank is the spell being cast again.
    CHECK_FALSE(TankHoldsWhileMobCasts(true, true));
    CHECK_FALSE(TankHoldsWhileMobCasts(true, false));
    // A different ground spell cast on the same victim (Blizzard while a Flamestrike zone burns).
    CHECK_FALSE(TankHoldsWhileMobCasts(false, true));
}
