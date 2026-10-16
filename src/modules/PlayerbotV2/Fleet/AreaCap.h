// A cap on how many world bots may stand in one area. Placement refuses a spot that is already full, and a periodic pass moves the
// bots beyond the cap out of a crowd that formed anyway (many bots after the same quest mob, a sleeping roster saved in one camp).
// Pure rules, tested in tests/game/ClassicAreaCap.cpp.
#pragma once

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace Playerbot::V2::Fleet
{

struct CrowdPoint
{
    std::uint64_t id = 0;
    float x = 0.0f, y = 0.0f;
    std::uint32_t cap = 0;   // how many may stand within the radius of this one (outdoors fewer than in a capital)
};

inline std::uint32_t CountWithin(std::vector<CrowdPoint> const& pts, float x, float y, float radius)
{
    std::uint32_t n = 0;
    float const r2 = radius * radius;
    for (CrowdPoint const& p : pts)
    {
        float const dx = p.x - x, dy = p.y - y;
        if (dx * dx + dy * dy <= r2) ++n;
    }
    return n;
}

// The bots to move out, at most `max_out`: walking the bots in id order, one stays while fewer than its cap are already kept within
// the radius of it; the rest are extras. Deterministic, so the same crowd names the same extras on each pass until they have moved.
inline std::vector<std::uint64_t> ExcessBots(std::vector<CrowdPoint> const& pts, float radius, std::size_t max_out)
{
    std::vector<CrowdPoint> order = pts;
    for (std::size_t i = 1; i < order.size(); ++i)   // insertion sort by id: the lists are a few hundred
        for (std::size_t j = i; j > 0 && order[j].id < order[j - 1].id; --j)
            std::swap(order[j], order[j - 1]);
    std::vector<CrowdPoint> kept;
    std::vector<std::uint64_t> out;
    for (CrowdPoint const& p : order)
    {
        if (p.cap == 0 || CountWithin(kept, p.x, p.y, radius) < p.cap)
            kept.push_back(p);
        else if (out.size() < max_out)
            out.push_back(p.id);
        else
            kept.push_back(p);   // not moved this pass: it still stands there
    }
    return out;
}

// May one more bot be put at (x, y)? The crowd already within the radius must be below the cap.
inline bool RoomFor(std::vector<CrowdPoint> const& pts, float x, float y, float radius, std::uint32_t cap)
{
    return cap == 0 || CountWithin(pts, x, y, radius) < cap;
}

} // namespace Playerbot::V2::Fleet
