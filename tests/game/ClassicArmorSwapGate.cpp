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

#include "../../src/modules/PlayerbotV2/Bot/Gear/ArmorSwapGate.h"

using namespace Playerbot::Gear;

TEST_CASE("Armor is only swapped when the bot is neither fighting nor casting", "[ClassicArmorSwapGate]")
{
    CHECK(CanSwapArmorNow(false, false));
    CHECK_FALSE(CanSwapArmorNow(true, false));    // result 61
    CHECK_FALSE(CanSwapArmorNow(false, true));    // result 40
    CHECK_FALSE(CanSwapArmorNow(true, true));
}
