// BotSwim - pure rules for a bot in deep water. A player's own client swims it: it sets the swimming flag and floats the character
// with its head out of the water. A bot has no client, so nothing did either. Its z came from whatever made its last move: the
// navmesh's water polygons lie on the surface (it stood and ran on top of the water), the ground probes of a goal hit the lake
// floor (it walked down and stood under the water until it drowned). idle:surface_to_breathe then lifted it back onto the top,
// the next goal took it down again, and it went up and down for as long as it stayed in the lake (Discord 2026-11-08, Joy, alexia and
// others: "on the water, then under it, forever", "some stay down until they drown", "they run on the water").
// Tested in tests/game/ClassicBotSwim.cpp.

#pragma once

#include <algorithm>
#include <cmath>

namespace Playerbot::BotMovement::Swim {

// How far under the surface a swimming bot floats: its head stays out (the core reads LIQUID_MAP_IN_WATER for 0 < depth <=
// collision height, UNDER_WATER past it, WATER_WALK at 0). Over half the body under, and never deeper than 1.6 y, which keeps
// it out of the snapshot's drowning check (DEFAULT_COLLISION_HEIGHT 2.03) whatever its race.
constexpr float kSwimFrac = 0.6f;
constexpr float kSwimMinDepth = 0.5f;
constexpr float kSwimMaxDepth = 1.6f;

inline float SwimDepth(float collision_height)
{
    return std::clamp(collision_height * kSwimFrac, kSwimMinDepth, kSwimMaxDepth);
}

// The z a bot swims at over water with this surface.
inline float SwimZ(float level, float collision_height)
{
    return level - SwimDepth(collision_height);
}

// Deep water: a bot standing on the floor would have its head under (the floor lies more than its height below the surface).
// Shallower water is waded on the floor, as before.
inline bool IsDeep(float level, float floor_z, float collision_height)
{
    return level - floor_z > collision_height;
}

// The asked z is from above the water (a bridge, a pier, a deck, a ledge over a pool): not a goal in the water.
constexpr float kAboveSurfaceAsk = 3.0f;
inline bool AskedFromAbove(float ask_z, float level)
{
    return ask_z > level + kAboveSurfaceAsk;
}

// A goal in deep water goes to the swim z, unless the caller asked on purpose for a point well under it that is not the bot's own
// level: an object or a creature on the lake floor (the bot dives for it, and idle:surface_to_breathe brings it up for air). A walk
// step at the bot's own z, a wander point or a surface ask all swim.
constexpr float kDiveMargin = 1.5f;
inline bool IsPurposefulDive(float ask_z, float bot_z, float swim_z)
{
    return ask_z < swim_z - kDiveMargin && std::fabs(ask_z - bot_z) > kDiveMargin;
}

// A bot standing still in deep water is moved to its swim z when it is this far off it (on the floor, or on top of the water).
constexpr float kSettleTolerance = 0.75f;
inline bool NeedsSettle(float bot_z, float swim_z)
{
    return std::fabs(bot_z - swim_z) > kSettleTolerance;
}

// A follower's settle to (dx, dy, dz) from its leader keeps it inside the follow generator's 3D range (combat reaches + follow
// distance), with a margin so the next leader step does not tip it out at once. Out of range, the generator would path it back up
// to the surface by the leader at his next step, and the settle would start again.
constexpr float kFollowRangeMargin = 0.5f;
inline bool SettleKeepsFollowRange(float dx, float dy, float dz, float follow_range)
{
    float const limit = follow_range - kFollowRangeMargin;
    return limit > 0.0f && dx * dx + dy * dy + dz * dz < limit * limit;
}

} // namespace Playerbot::BotMovement::Swim
