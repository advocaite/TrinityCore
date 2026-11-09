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

#include "../../src/modules/PlayerbotV2/Bot/TradeRule.h"

using Playerbot::AcceptsTradeFrom;

TEST_CASE("TradeRule: same group or owner accepts, strangers decline", "[trade]")
{
    //                         enabled real   owner  in my group
    CHECK(AcceptsTradeFrom(true,  true,  false, true));    // a player in the bot's group or raid
    CHECK(AcceptsTradeFrom(true,  true,  true,  false));   // its owner, even when the group did not form
    CHECK(AcceptsTradeFrom(true,  true,  true,  true));
    CHECK_FALSE(AcceptsTradeFrom(true,  true,  false, false));   // a stranger gets "busy"
    CHECK_FALSE(AcceptsTradeFrom(true,  false, false, true));    // another bot in the group is not a trade partner
    CHECK_FALSE(AcceptsTradeFrom(false, true,  true,  true));    // switched off: nobody
}
