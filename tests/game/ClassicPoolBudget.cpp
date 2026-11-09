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

#include "../../src/modules/PlayerbotV2/Fleet/PoolBudget.h"

using namespace Playerbot::V2::Fleet;

TEST_CASE("PoolBudget: a full core sends new characters to the variable pool", "[poolbudget]")
{
    PoolBudget b{ 5506, 3000, 12, 2000 };   // the 2026-10-16 numbers
    CHECK(ChoosePoolSlot(b, false) == PoolSlot::Variable);
    CHECK(ChoosePoolSlot(b, true) == PoolSlot::Variable);
}

TEST_CASE("PoolBudget: a core with room takes the roster bots, the island never does", "[poolbudget]")
{
    PoolBudget b{ 100, 3000, 0, 2000 };
    CHECK(ChoosePoolSlot(b, false) == PoolSlot::Core);
    CHECK(ChoosePoolSlot(b, true) == PoolSlot::Variable);
}

TEST_CASE("PoolBudget: nothing is created when both budgets are full", "[poolbudget]")
{
    PoolBudget b{ 3000, 3000, 2000, 2000 };
    CHECK(ChoosePoolSlot(b, false) == PoolSlot::None);
    CHECK(ChoosePoolSlot(b, true) == PoolSlot::None);
}

TEST_CASE("PoolBudget: a zero variable budget switches the temporary bots off", "[poolbudget]")
{
    PoolBudget b{ 5000, 3000, 0, 0 };
    CHECK(ChoosePoolSlot(b, true) == PoolSlot::None);
    CHECK(ChoosePoolSlot(b, false) == PoolSlot::None);
}

TEST_CASE("PoolBudget: a created character is counted against the slot it took", "[poolbudget]")
{
    PoolBudget b{ 2999, 3000, 1999, 2000 };
    PoolCountCreated(b, PoolSlot::Core);
    CHECK(b.core == 3000);
    CHECK(b.variable == 1999);
    PoolCountCreated(b, PoolSlot::Variable);
    CHECK(b.variable == 2000);
    PoolCountCreated(b, PoolSlot::None);
    CHECK(b.core == 3000);
    CHECK(b.variable == 2000);
}

TEST_CASE("PoolBudget: only an idle temporary character is retired", "[poolbudget]")
{
    constexpr std::int64_t ttl = 24 * 3600;
    RetireFacts f;
    f.variable = true;
    f.offline_seconds = ttl;
    CHECK(CanRetire(f, ttl));

    RetireFacts young = f;   young.offline_seconds = ttl - 1;    CHECK_FALSE(CanRetire(young, ttl));
    RetireFacts core = f;    core.variable = false;              CHECK_FALSE(CanRetire(core, ttl));
    RetireFacts online = f;  online.online = true;               CHECK_FALSE(CanRetire(online, ttl));
    RetireFacts owned = f;   owned.owned = true;                 CHECK_FALSE(CanRetire(owned, ttl));
    RetireFacts group = f;   group.grouped = true;               CHECK_FALSE(CanRetire(group, ttl));
    RetireFacts queue = f;   queue.queue_tagged = true;          CHECK_FALSE(CanRetire(queue, ttl));
}

TEST_CASE("PoolBudget: with a census the bracket decides, not the roster total", "[poolbudget]")
{
    PoolBudget over{ 5420, 3000, 10, 2000 };   // a roster that grew past its cap earlier
    CHECK(ChoosePoolSlot(over, false) == PoolSlot::Variable);
    CHECK(ChoosePoolSlot(BalancedBudget(over, true), false) == PoolSlot::Core);        // the 1-9 bracket is short: the roster takes it
    CHECK(ChoosePoolSlot(BalancedBudget(over, false), false) == PoolSlot::Variable);   // its bracket is full: temporary
    PoolBudget room{ 10, 3000, 0, 2000 };
    CHECK(ChoosePoolSlot(BalancedBudget(room, false), false) == PoolSlot::Variable);   // a full (faction, bracket) is refused even with room in total
}
