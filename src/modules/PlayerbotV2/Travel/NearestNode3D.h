// NearestNode3D - the nearest travel-graph node to a bot, measured in 3D and skipping nodes it already failed to reach. A city has
// floors on top of each other (Ironforge: the Great Forge at z~400, the wards at z~500): the nearest node by x/y alone is often on
// another floor, unreachable from where the bot stands, so a wedged bot asked for that node again and again and never got out.
// Pure rules, tested in tests/game/ClassicNearestNode3D.cpp.

#pragma once

#include <cstddef>
#include <functional>

namespace Playerbot::V2::Travel {

constexpr float kNearestNodeVerticalWeight = 3.0f;   // a yard of height counts like this many yards of distance

// Index of the nearest accepted candidate within max_range, -1 when none. `at(i, x, y, z)` fills candidate i; accept(i) filters.
template <typename At, typename Accept>
inline int PickNearestNode3D(std::size_t count, At const& at, Accept const& accept, float bx, float by, float bz, float max_range)
{
    int best = -1;
    float best_d2 = max_range * max_range;
    for (std::size_t i = 0; i < count; ++i)
    {
        float x, y, z;
        at(i, x, y, z);
        float const dx = x - bx, dy = y - by, dz = (z - bz) * kNearestNodeVerticalWeight;
        float const d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < best_d2 && accept(i))
        {
            best_d2 = d2;
            best = int(i);
        }
    }
    return best;
}

} // namespace Playerbot::V2::Travel
