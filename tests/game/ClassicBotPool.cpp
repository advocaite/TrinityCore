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

#include "../../src/modules/PlayerbotV2/Fleet/BotPool.h"

using namespace Playerbot::Pool;

TEST_CASE("BotPool roles", "[botpool]")
{
    CHECK(ClassifyRole({}) == BotRole::Ambient);
    CHECK(ClassifyRole({ true, false, false }) == BotRole::Owned);
    CHECK(ClassifyRole({ false, true, false }) == BotRole::Owned);      // a real player's alt
    CHECK(ClassifyRole({ false, false, true }) == BotRole::Bg);
    CHECK(ClassifyRole({ true, false, true }) == BotRole::Owned);       // ownership wins over a fill tag
    CHECK_FALSE(UsesWorldGear(BotRole::Owned));
    CHECK(UsesWorldGear(BotRole::Ambient));
    CHECK(UsesWorldGear(BotRole::Bg));
}

TEST_CASE("BotPool eligibility", "[botpool]")
{
    FreeFacts f;
    CHECK(CheckFree(f) == Verdict::Free);

    { FreeFacts x = f; x.role = BotRole::Owned;  CHECK(CheckFree(x) == Verdict::NotAmbient); }
    { FreeFacts x = f; x.role = BotRole::Bg;     CHECK(CheckFree(x) == Verdict::Committed); }
    { FreeFacts x = f; x.in_world = false;       CHECK(CheckFree(x) == Verdict::Absent); }
    { FreeFacts x = f; x.teleporting = true;     CHECK(CheckFree(x) == Verdict::Absent); }
    { FreeFacts x = f; x.alive = false;          CHECK(CheckFree(x) == Verdict::Dead); }
    { FreeFacts x = f; x.grouped = true;         CHECK(CheckFree(x) == Verdict::Grouped); }
    { FreeFacts x = f; x.in_combat = true;       CHECK(CheckFree(x) == Verdict::Combat); }
    { FreeFacts x = f; x.on_instance_map = true; CHECK(CheckFree(x) == Verdict::Instanced); }
    { FreeFacts x = f; x.world_busy = true;      CHECK(CheckFree(x) == Verdict::WorldBusy); }

    // An owned bot is never free, whatever else is true of it.
    FreeFacts owned = f;
    owned.role = BotRole::Owned;
    CHECK(CheckFree(owned) != Verdict::Free);
}
