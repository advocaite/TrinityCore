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
#include "tc_catch2.h"

#include "../../src/modules/PlayerbotV2/Fleet/OwnerCatchUp.h"

using namespace Playerbot::V2::Fleet;

TEST_CASE("Only a bot by its owner, free to go, boards his flight", "[ClassicOwnerCatchUp]")
{
    CHECK(FlyBoardEligible(true, false, false, false, true, 10.0f));
    CHECK(FlyBoardEligible(true, false, false, false, true, kFlyBoardYards));
    CHECK_FALSE(FlyBoardEligible(true, false, false, false, true, kFlyBoardYards + 1.0f));   // too far off
    CHECK_FALSE(FlyBoardEligible(false, false, false, false, true, 5.0f));                  // dead
    CHECK_FALSE(FlyBoardEligible(true, true, false, false, true, 5.0f));                    // fighting
    CHECK_FALSE(FlyBoardEligible(true, false, true, false, true, 5.0f));                    // already flying
    CHECK_FALSE(FlyBoardEligible(true, false, false, true, true, 5.0f));                    // on a ship or a vehicle
    CHECK_FALSE(FlyBoardEligible(true, false, false, false, false, 5.0f));                  // another map
}

TEST_CASE("Apart: far away, or on another level close by", "[ClassicOwnerCatchUp]")
{
    CHECK_FALSE(CatchUpApart(30.0f, 0.0f));
    CHECK(CatchUpApart(41.0f, 0.0f));
    CHECK(CatchUpApart(20.0f, 15.0f));    // he jumped up a ledge
    CHECK(CatchUpApart(20.0f, -15.0f));   // or the bot is under him
    CHECK_FALSE(CatchUpApart(10.0f, 9.0f));   // close enough, a step or a ramp
    CHECK_FALSE(CatchUpApart(20.0f, 5.0f));
}

TEST_CASE("A bot that gets no closer for 15 looks is brought", "[ClassicOwnerCatchUp]")
{
    CatchUpState st;
    uint32_t t = 1000u;
    for (int i = 0; i < kCatchUpStrikes - 1; ++i, t += kCatchUpCheckMs)
        CHECK_FALSE(CatchUpUpdate(st, true, false, 60.0f, 0.0f, t));
    CHECK(CatchUpUpdate(st, true, false, 60.0f, 0.0f, t));
}

TEST_CASE("A bot closing in on its owner within range is left to walk", "[ClassicOwnerCatchUp]")
{
    CatchUpState st;
    uint32_t t = 1000u;
    float gap = 98.0f;
    for (int i = 0; i < 16; ++i, t += kCatchUpCheckMs, gap -= 3.5f)
        CHECK_FALSE(CatchUpUpdate(st, true, false, gap, 0.0f, t));
}

TEST_CASE("A bot out of range for 15 looks is brought even while it walks closer", "[ClassicOwnerCatchUp]")
{
    CatchUpState st;
    uint32_t t = 1000u;
    float gap = 900.0f;   // he ported from Dun Morogh to Crystal Lake; the bot runs down from Ironforge
    for (int i = 0; i < kCatchUpStrikes - 1; ++i, t += kCatchUpCheckMs, gap -= 7.0f)
        CHECK_FALSE(CatchUpUpdate(st, true, false, gap, 0.0f, t));
    CHECK(CatchUpUpdate(st, true, false, gap, 0.0f, t));
}

TEST_CASE("Falling behind an owner who runs away counts as no way closer", "[ClassicOwnerCatchUp]")
{
    CatchUpState st;
    uint32_t t = 1000u;
    float gap = 45.0f;
    bool brought = false;
    for (int i = 0; i < kCatchUpStrikes && !brought; ++i, t += kCatchUpCheckMs, gap += 2.0f)
        brought = CatchUpUpdate(st, true, false, gap, 0.0f, t);
    CHECK(brought);
}

TEST_CASE("A fight, a stay order or coming close starts the count again", "[ClassicOwnerCatchUp]")
{
    CatchUpState st;
    uint32_t t = 1000u;
    for (int i = 0; i < kCatchUpStrikes - 1; ++i, t += kCatchUpCheckMs)
        CHECK_FALSE(CatchUpUpdate(st, true, false, 60.0f, 0.0f, t));
    CHECK_FALSE(CatchUpUpdate(st, false, false, 60.0f, 0.0f, t));   // not eligible this look
    t += kCatchUpCheckMs;
    for (int i = 0; i < kCatchUpStrikes - 1; ++i, t += kCatchUpCheckMs)
        CHECK_FALSE(CatchUpUpdate(st, true, false, 60.0f, 0.0f, t));
    CHECK_FALSE(CatchUpUpdate(st, true, false, 20.0f, 0.0f, t));    // came within reach
    CHECK(st.strikes == 0);
}

TEST_CASE("A bot left on another continent is brought after the same wait", "[ClassicOwnerCatchUp]")
{
    CatchUpState st;
    uint32_t t = 1000u;
    for (int i = 0; i < kCatchUpStrikes - 1; ++i, t += kCatchUpCheckMs)
        CHECK_FALSE(CatchUpUpdate(st, true, true, 0.0f, 0.0f, t));
    CHECK(CatchUpUpdate(st, true, true, 0.0f, 0.0f, t));
}

TEST_CASE("At most one catch-up per bot in the cooldown", "[ClassicOwnerCatchUp]")
{
    CatchUpState st;
    uint32_t t = 1000u;
    for (int i = 0; i < kCatchUpStrikes; ++i, t += kCatchUpCheckMs)
        CatchUpUpdate(st, true, true, 0.0f, 0.0f, t);
    CHECK(st.snapped_once);
    uint32_t const snapped = st.last_snap_ms;
    bool again = false;
    for (; t < snapped + kCatchUpCooldownMs; t += kCatchUpCheckMs)
        again = again || CatchUpUpdate(st, true, true, 0.0f, 0.0f, t);
    CHECK_FALSE(again);
}
