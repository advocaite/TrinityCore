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

#include "../../src/modules/PlayerbotV2/Bot/AddonHint.h"

using namespace Playerbot::V2;

TEST_CASE("AddonHint: due after the grace period, once", "[addonhint]")
{
    AddonHintTracker t;
    t.Login(7, 1000, 30000);
    CHECK(t.TakeDue(30999).empty());
    CHECK(t.Pending(7));
    auto due = t.TakeDue(31000);
    REQUIRE(due.size() == 1);
    CHECK(due[0] == 7);
    CHECK_FALSE(t.Pending(7));
    CHECK(t.TakeDue(99000).empty());
}

TEST_CASE("AddonHint: hello or logout cancels the hint", "[addonhint]")
{
    AddonHintTracker t;
    t.Login(1, 0, 100);
    t.Login(2, 0, 100);
    t.Login(3, 0, 100);
    t.Hello(1);
    t.Logout(2);
    auto due = t.TakeDue(100);
    REQUIRE(due.size() == 1);
    CHECK(due[0] == 3);
}

TEST_CASE("AddonHint: a new login restarts the grace period", "[addonhint]")
{
    AddonHintTracker t;
    t.Login(5, 0, 100);
    t.Login(5, 90, 100);
    CHECK(t.TakeDue(100).empty());
    CHECK(t.TakeDue(190).size() == 1);
}

TEST_CASE("AddonHint: the millisecond clock may wrap", "[addonhint]")
{
    AddonHintTracker t;
    t.Login(9, 0xFFFFFF00u, 1000);
    CHECK(t.TakeDue(0xFFFFFFF0u).empty());
    CHECK(t.TakeDue(0x000000FFu).empty());
    CHECK(t.TakeDue(0x000003F0u).size() == 1);
}

TEST_CASE("AddonHint: the greeting text", "[addonhint]")
{
    CHECK(kAddonHelloText == "hello");
}

TEST_CASE("AddonHint: only .bot lines count as add-on commands", "[addonhint]")
{
    CHECK(IsAddonBotCommand(".bot add mage dps"));
    CHECK(IsAddonBotCommand(".bot add warrior tank"));
    CHECK_FALSE(IsAddonBotCommand(".bot"));
    CHECK_FALSE(IsAddonBotCommand(".bot "));
    CHECK_FALSE(IsAddonBotCommand(".bot    "));
    CHECK_FALSE(IsAddonBotCommand(".gm on"));
    CHECK_FALSE(IsAddonBotCommand(".botadd mage"));
    CHECK_FALSE(IsAddonBotCommand("hello"));
    CHECK_FALSE(IsAddonBotCommand(""));
}
