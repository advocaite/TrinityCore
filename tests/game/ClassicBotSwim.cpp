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

#include "../../src/modules/PlayerbotV2/Api/BotSwim.h"

using namespace Playerbot::BotMovement::Swim;

namespace {
constexpr float kHuman = 2.03128f;   // DEFAULT_COLLISION_HEIGHT
// The core's liquid status at depth d = level - z for a unit of height h (TerrainInfo::GetLiquidStatus).
enum class Status { Above, WaterWalk, InWater, UnderWater };
Status At(float level, float z, float h)
{
    float const delta = level - z;
    if (delta > h) return Status::UnderWater;
    if (delta > 0.0f) return Status::InWater;
    if (delta > -0.1f) return Status::WaterWalk;
    return Status::Above;
}
}

TEST_CASE("BotSwim: a swimming bot floats in the water with its head out, whatever its height", "[botswim]")
{
    for (float h : { 1.0f, 1.5f, kHuman, 2.6f, 3.5f })
    {
        float const z = SwimZ(10.0f, h);
        INFO("height " << h << " swim z " << z);
        // In the water (IsInWater() true, so every in-water rule sees it) and not under it (no breath bar, no surface_to_breathe).
        REQUIRE(At(10.0f, z, h) == Status::InWater);
        // The snapshot's drowning check reads the default height: it must not fire for a tall bot either.
        REQUIRE(At(10.0f, z, kHuman) != Status::UnderWater);
        // Not on top of the water: the old surface z (WATER_WALK) is what made the bots "run on the water".
        REQUIRE(z < 10.0f);
    }
}

TEST_CASE("BotSwim: deep water is water over the bot's head when it stands on the floor", "[botswim]")
{
    REQUIRE(IsDeep(0.0f, -5.0f, kHuman));          // the Deadmines cove, a lake
    REQUIRE_FALSE(IsDeep(0.0f, -1.5f, kHuman));    // a stream or a shore: waded on the floor, as before
    REQUIRE_FALSE(IsDeep(0.0f, -2.0f, kHuman));
    REQUIRE(IsDeep(0.0f, -2.0f, 1.2f));            // a gnome swims where a human wades
}

TEST_CASE("BotSwim: an ask from a bridge or a deck over the water is not a swim", "[botswim]")
{
    REQUIRE(AskedFromAbove(20.0f, 10.0f));
    REQUIRE_FALSE(AskedFromAbove(10.0f, 10.0f));   // the surface (a walk step at the bot's own z, idle:surface_to_breathe's + 0.5)
    REQUIRE_FALSE(AskedFromAbove(10.5f, 10.0f));
    REQUIRE_FALSE(AskedFromAbove(4.0f, 10.0f));    // the lake bed
}

TEST_CASE("BotSwim: a goal in deep water is swum to, a dive to the floor on purpose keeps its depth", "[botswim]")
{
    float const swim = SwimZ(10.0f, kHuman);
    // A walk step at its own z from the surface, a wander point, the old surface_to_breathe ask: all swim.
    REQUIRE_FALSE(IsPurposefulDive(10.0f, 10.0f, swim));
    REQUIRE_FALSE(IsPurposefulDive(10.5f, 4.0f, swim));
    // A bot that sank to the bed asks for the next step at its own z (the bed): that is not a dive, it swims up.
    REQUIRE_FALSE(IsPurposefulDive(4.0f, 4.0f, swim));
    // From the surface to an object lying on the bed: a dive.
    REQUIRE(IsPurposefulDive(4.0f, swim, swim));
}

TEST_CASE("BotSwim: a bot standing still on the bed or on top of the water settles at the swim z, one already there stays", "[botswim]")
{
    float const swim = SwimZ(10.0f, kHuman);
    REQUIRE(NeedsSettle(4.0f, swim));     // on the lake bed: up before it drowns
    REQUIRE(NeedsSettle(10.0f, swim));    // on top of the water: down into it
    REQUIRE_FALSE(NeedsSettle(swim, swim));
    REQUIRE_FALSE(NeedsSettle(swim + 0.5f, swim));
}

TEST_CASE("BotSwim: a follower settles only where it stays in follow range, so the follow does not lift it straight back up", "[botswim]")
{
    // lake_swim 2026-11-09: follow distance 2 y, reaches 1.5 + 1.5: range 5 y. The leader on the surface, the follower 4.6 y off:
    // 0.9 y down took it out of range and the next leader step pathed it back up, 14 times in 25 s.
    float const range = 1.5f + 1.5f + 2.0f;
    REQUIRE_FALSE(SettleKeepsFollowRange(4.6f, 0.0f, -0.9f, range));
    // The same follower closer in, or a leader who swims at the same depth: it settles.
    REQUIRE(SettleKeepsFollowRange(3.0f, 0.0f, -0.9f, range));
    REQUIRE(SettleKeepsFollowRange(4.3f, 0.0f, 0.0f, range));
    // No range left after the margin: never.
    REQUIRE_FALSE(SettleKeepsFollowRange(0.0f, 0.0f, 0.0f, 0.4f));
}
