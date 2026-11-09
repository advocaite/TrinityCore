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

#include "../../src/modules/PlayerbotV2/Api/VirtualSupply.h"

using namespace Playerbot::Classic;

TEST_CASE("VirtualSupply: an item in the bags always comes first", "[virtualsupply]")
{
    REQUIRE(RecoveryItemOrVirtual(8766, true, 1645) == 8766);   // Morning Glory Dew in the bags, not the level's Moonberry Juice
    REQUIRE(RecoveryItemOrVirtual(8766, false, 0) == 8766);
}

TEST_CASE("VirtualSupply: a healer with full bags and no drink still drinks", "[virtualsupply]")
{
    // deadmines_full: bags full of loot, no water left; infinite supplies hands the level's rank instead of nothing.
    REQUIRE(RecoveryItemOrVirtual(0, true, 1645) == 1645);
    REQUIRE(UseVirtually(false, true, true));
}

TEST_CASE("VirtualSupply: nothing from nothing without infinite supplies", "[virtualsupply]")
{
    REQUIRE(RecoveryItemOrVirtual(0, false, 1645) == 0);
    REQUIRE_FALSE(UseVirtually(false, false, true));
    REQUIRE(RecoveryItemOrVirtual(0, true, 0) == 0);   // no rank for the class (water for a warrior)
}

TEST_CASE("VirtualSupply: only food and water are used without the item", "[virtualsupply]")
{
    REQUIRE_FALSE(UseVirtually(false, true, false));   // a quest item or a potion is never conjured from nothing
    REQUIRE_FALSE(UseVirtually(true, true, true));     // the real item is used when the bot has it
}
