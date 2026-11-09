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

#include "../../src/modules/PlayerbotV2/Bot/Raid/DispelWorth.h"
#include "../../src/modules/PlayerbotV2/Bot/RunToTankRules.h"
#include "../../src/modules/PlayerbotV2/Combat/Apl/PriestRenewRules.h"

// MC Garr 2026-10-10 mana audit: 330 dispels, 37 Inner Fires, a healer that ran through the Firesworn.
TEST_CASE("A weak damage-over-time is dispelled only once the carrier is hurt", "[ClassicManaAudit]")
{
    using namespace Playerbot::Raid;
    CHECK_FALSE(DispelWorthIt(false, 75, 100));
    CHECK_FALSE(DispelWorthIt(false, 75, 76));
    CHECK(DispelWorthIt(false, 75, 75));
    CHECK(DispelWorthIt(false, 0, 100));      // a curse does its harm at full health: always
    CHECK(DispelWorthIt(true, 75, 100));      // urgent rules ignore the health gate
}

TEST_CASE("A non-urgent dispel does not take the last mana of a healer", "[ClassicManaAudit]")
{
    using namespace Playerbot::Raid;
    CHECK_FALSE(HealerMayDispel(false, true, 39));
    CHECK(HealerMayDispel(false, true, 40));
    CHECK(HealerMayDispel(false, false, 5));  // a mage has no heals to save it for
    CHECK(HealerMayDispel(true, true, 5));
}

TEST_CASE("Inner Fire in an instance fight waits for comfortable mana", "[ClassicManaAudit]")
{
    using namespace Playerbot::Combat::ClassicHeal;
    CHECK_FALSE(InnerFireWorthRecast(true, true, 59));
    CHECK(InnerFireWorthRecast(true, true, 60));
    CHECK(InnerFireWorthRecast(false, true, 10));   // between pulls
    CHECK(InnerFireWorthRecast(true, false, 10));   // open world
}

TEST_CASE("A healer does not run to the tank across enemies that are not on the tank", "[ClassicManaAudit]")
{
    using namespace Playerbot;
    // healer at (0,0), tank at (40,0)
    CHECK(EnemyBlocksRunToTank(false, 0, 0, 40, 0, 20, 5));      // an add beside the path, not on the tank
    CHECK_FALSE(EnemyBlocksRunToTank(true, 0, 0, 40, 0, 20, 5)); // the tank's own mob
    CHECK_FALSE(EnemyBlocksRunToTank(false, 0, 0, 40, 0, 20, 15));
    CHECK_FALSE(EnemyBlocksRunToTank(false, 0, 0, 40, 0, -20, 0)); // behind the healer
}
