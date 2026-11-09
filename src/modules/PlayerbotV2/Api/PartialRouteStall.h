// PartialRouteStall - stops a bot from walking partial routes toward a far goal forever without getting closer.
// A route past the path cap comes back as a partial ending at the reachable polygon nearest the goal. From one pose that end can lie
// down a ramp, from the next one back up it, so every partial is "reachable" and the bot walks between two local minima.
// Judged on net progress, not on the shape of a single partial: every driven partial counts, and only genuinely closing in on the
// goal gives the budget back. Tested in tests/game/ClassicPartialRouteStall.cpp.

#pragma once

#include <cstdint>

namespace Playerbot::BotMovement {

struct PartialRouteStall
{
    std::uint64_t goal_key = 0;
    float best_dist = 1e9f;         // closest the bot has been to the goal since the budget was last refilled
    std::uint32_t stalled = 0;      // partials driven since then without closing in
};

constexpr float kStallMinGainYards = 5.0f;          // closing in by less than this is not progress
constexpr std::uint32_t kStallMaxPartials = 8u;     // a pocket needs a few legs to climb out of; a loop never closes in

// Call before driving a partial route. `dist_to_goal` is the bot's current distance to the goal.
// Returns true when the partial may be driven, false when the bot has walked the budget without getting closer.
inline bool AllowPartialRoute(PartialRouteStall& st, std::uint64_t goal_key, float dist_to_goal)
{
    if (st.goal_key != goal_key || dist_to_goal + kStallMinGainYards < st.best_dist)
    {
        st.goal_key = goal_key;
        st.best_dist = dist_to_goal;
        st.stalled = 0;
        return true;
    }
    if (dist_to_goal < st.best_dist)
        st.best_dist = dist_to_goal;
    ++st.stalled;
    return st.stalled <= kStallMaxPartials;
}

// Whether a partial that ends no closer to the goal is a long route cut by the path cap (walk it) rather than a goal off the navmesh
// (refuse it). Detour's partial for a goal it cannot reach ends at the searched polygon nearest the goal, never meaningfully farther
// than the start; one that ends well farther away is the first stretch of a real route that winds away first. A far goal (more than
// 100 yards) was always walked this way. A near one that winds was refused for ever: the RFC tank on the ledge 18 yards above
// Oggleflint (27 yards away) got a 72-point partial back down into the pit 99 yards from him on every try and stood there for good.
constexpr float kFarGoalYards = 100.0f;
constexpr float kTruncatedRouteSlackYards = 10.0f;   // a partial ending this much farther than the start was cut, not refused

constexpr bool PartialIsTruncatedRoute(float cur_to_dst, float end_to_dst)
{
    return cur_to_dst > kFarGoalYards || end_to_dst > cur_to_dst + kTruncatedRouteSlackYards;
}

} // namespace Playerbot::BotMovement
