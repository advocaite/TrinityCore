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

#include "../../src/modules/PlayerbotV2/Api/PathLiquid.h"

using namespace Playerbot::BotMovement;

namespace
{
// A canal between y = 40 and y = 50.
bool Canal(float, float y, float) { return y > 40.0f && y < 50.0f; }
}

TEST_CASE("PathLiquid: a crossing between two dry corners is found", "[pathliquid]")
{
    std::vector<PathPoint> const cross = { { 0, 30, 0 }, { 0, 60, 0 } };     // both corners dry, the segment wades the canal
    CHECK(PathTouchesLiquid(cross, Canal));
    std::vector<PathPoint> const bank = { { 0, 30, 0 }, { 40, 30, 0 }, { 40, 20, 0 } };
    CHECK_FALSE(PathTouchesLiquid(bank, Canal));
}

TEST_CASE("PathLiquid: a bot standing in the water may start", "[pathliquid]")
{
    // starts at y = 45 (wet), walks out to y = 60: only the leg to dry land, nothing wet after the start point
    std::vector<PathPoint> const out = { { 0, 45, 0 }, { 0, 60, 0 } };
    CHECK(PathTouchesLiquid(out, Canal));                 // it still crosses 5 yards of canal to get out: wet
    std::vector<PathPoint> const leaving = { { 0, 50, 0 }, { 0, 60, 0 } };
    CHECK_FALSE(PathTouchesLiquid(leaving, Canal));       // stands on the edge, every later point is dry
    std::vector<PathPoint> const one = { { 0, 45, 0 } };
    CHECK_FALSE(PathTouchesLiquid(one, Canal));
}

TEST_CASE("PathLiquid: policy decisions", "[pathliquid]")
{
    CHECK(DecideLiquid(LiquidPolicy::None, true, false) == LiquidAction::Go);
    CHECK(DecideLiquid(LiquidPolicy::RequireDry, false, false) == LiquidAction::Go);
    CHECK(DecideLiquid(LiquidPolicy::RequireDry, true, true) == LiquidAction::UseDryRoute);
    CHECK(DecideLiquid(LiquidPolicy::RequireDry, true, false) == LiquidAction::Refuse);
    CHECK(DecideLiquid(LiquidPolicy::PreferDry, true, true) == LiquidAction::UseDryRoute);
    CHECK(DecideLiquid(LiquidPolicy::PreferDry, true, false) == LiquidAction::Go);    // a follower still follows through water
}

TEST_CASE("PathLiquid: a follower is never refused for water, a town world bot is", "[pathliquid]")
{
    //                                        in_water bg/instance leveling grouped owned
    CHECK(PickLiquidPolicy(false, false, false, false, false) == LiquidPolicy::RequireDry);   // world bot in a capital
    CHECK(PickLiquidPolicy(false, false, false, true,  false) == LiquidPolicy::PreferDry);    // grouped
    CHECK(PickLiquidPolicy(false, false, false, false, true ) == LiquidPolicy::PreferDry);    // companion not yet grouped with its owner
    CHECK(PickLiquidPolicy(true,  false, false, false, false) == LiquidPolicy::None);         // already swimming
    CHECK(PickLiquidPolicy(false, true,  false, true,  true ) == LiquidPolicy::None);         // Blackfathom, Deadmines, BGs
    CHECK(PickLiquidPolicy(false, false, true,  false, false) == LiquidPolicy::None);         // open-world zone
    // with no dry route a PreferDry bot goes through the water, only RequireDry refuses
    CHECK(DecideLiquid(LiquidPolicy::PreferDry, true, false) == LiquidAction::Go);
    CHECK(DecideLiquid(LiquidPolicy::RequireDry, true, false) == LiquidAction::Refuse);
}

TEST_CASE("PathLiquid: a ghost on its corpse run ignores magma and deep water", "[pathliquid]")
{
    CHECK(LiquidMattersFor(true));
    CHECK_FALSE(LiquidMattersFor(false));
}
