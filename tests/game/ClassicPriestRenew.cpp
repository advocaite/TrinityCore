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

#include "../../src/modules/PlayerbotV2/Combat/Apl/PriestRenewRules.h"

using namespace Playerbot::Combat::ClassicHeal;

// Deadmines, live 2026-10-10: the level-21 priest cast Heal and Flash Heal all fight and no Renew on the tank.
TEST_CASE("The priest Renews a tank that is taking damage before any direct heal", "[ClassicPriestRenew]")
{
    CHECK(RenewTankDue(100, 800, false, 80, 100));      // pull just landed: swings coming, tank still full
    CHECK(RenewTankDue(70, 0, false, 80, 70));          // hurt, nothing incoming
    CHECK_FALSE(RenewTankDue(100, 0, false, 80, 100));  // topped up, nobody hitting it
    CHECK_FALSE(RenewTankDue(60, 500, true, 80, 60));   // already carries this priest's Renew
}

TEST_CASE("The early Renew leaves the emergency band and a dry priest to the direct heals", "[ClassicPriestRenew]")
{
    CHECK_FALSE(RenewTankDue(60, 500, false, 80, 40));  // a member at 40 % belongs to Flash Heal
    CHECK(RenewTankDue(60, 500, false, 80, 41));
    CHECK_FALSE(RenewTankDue(60, 500, false, 34, 60));  // under 35 % mana
    CHECK(RenewTankDue(60, 500, false, 35, 60));
}
