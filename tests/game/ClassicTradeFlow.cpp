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

#include "../../src/modules/PlayerbotV2/Api/TradeFlow.h"

using namespace Playerbot::Trade;

TEST_CASE("TradeFlow: a proposal is answered with a begin at once, so the window opens", "[tradeflow]")
{
    CHECK(NextStep(false, 0) == Step::Begin);
    CHECK(NextStep(false, 500) == Step::Begin);
}

TEST_CASE("TradeFlow: once begun the bot waits, and never begins twice", "[tradeflow]")
{
    CHECK(NextStep(true, 100) == Step::Wait);
    CHECK(NextStep(true, kTradeIdleMs - 1) == Step::Wait);
}

TEST_CASE("TradeFlow: a window nobody finishes is closed, begun or not", "[tradeflow]")
{
    CHECK(NextStep(true, kTradeIdleMs) == Step::Cancel);
    CHECK(NextStep(false, kTradeIdleMs + 1) == Step::Cancel);
}

TEST_CASE("TradeFlow: a new window after a finished trade is begun again, however fast the retry", "[tradeflow]")
{
    Seen seen;
    Adopt(seen, 10, 1000);
    CHECK(NextStep(seen.begun, 0) == Step::Begin);
    seen.begun = true;                                  // the bot opened window 10
    Adopt(seen, 10, 1200);                              // the same window on the next tick: nothing changes
    CHECK(seen.begun);
    CHECK(seen.first_ms == 1000);

    Adopt(seen, 11, 1300);                              // 100 ms after window 10 closed the player proposes again
    CHECK_FALSE(seen.begun);
    CHECK(seen.first_ms == 1300);
    CHECK(NextStep(seen.begun, 0) == Step::Begin);
}

TEST_CASE("TradeFlow: the idle clock starts with the window, not with the bot's first trade", "[tradeflow]")
{
    Seen seen;
    Adopt(seen, 1, 5000);
    seen.begun = true;
    Adopt(seen, 2, 5000 + kTradeIdleMs + 10);           // a long-closed window, then a fresh proposal
    CHECK(NextStep(seen.begun, 0) == Step::Begin);
    CHECK(seen.first_ms == 5000 + kTradeIdleMs + 10);
    Adopt(seen, 3, 0);                                  // a clock that reads 0 still marks the window as started
    CHECK(seen.first_ms == 1);
}
