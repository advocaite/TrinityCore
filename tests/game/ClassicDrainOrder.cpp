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

#include "../../src/modules/PlayerbotV2/Bot/DrainOrder.h"

#include <set>

using Playerbot::V2::NextDrainStart;

TEST_CASE("A pass that was not cut keeps its start", "[ClassicDrainOrder]")
{
    CHECK(NextDrainStart(0, 100, false) == 0);
    CHECK(NextDrainStart(37, 100, false) == 37);
    CHECK(NextDrainStart(0, 0, true) == 0);   // nobody registered
}

TEST_CASE("A cut pass moves the start on and wraps", "[ClassicDrainOrder]")
{
    CHECK(NextDrainStart(0, 60, true) == 10);
    CHECK(NextDrainStart(55, 60, true) == 5);
    CHECK(NextDrainStart(130, 60, true) == 20);   // a start left over from a larger registry
    CHECK(NextDrainStart(0, 3, true) == 1);       // always at least one bot on
}

TEST_CASE("Under steady overload no bot stays in the tail of the pass", "[ClassicDrainOrder]")
{
    for (std::size_t bots : { 6u, 12u, 60u, 200u })
    {
        std::size_t const tail = bots - bots / 6u;   // the bots from this position on are the ones a limit cuts off
        for (std::size_t bot = 0; bot < bots; ++bot)
        {
            std::size_t start = 0, run = 0, worst = 0;
            for (int pass = 0; pass < 60; ++pass)
            {
                std::size_t const position = (bot + bots - start) % bots;   // 0 = first served this pass
                run = position >= tail ? run + 1 : 0;
                worst = std::max(worst, run);
                start = NextDrainStart(start, bots, true);
            }
            CHECK(worst <= 1);   // with a fixed start the bots at the tail would wait all 60
        }
    }
}
