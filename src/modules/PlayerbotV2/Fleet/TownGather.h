// Where people gather in a capital: the bank, the auction house, the inn, the flight master, the battlemaster. Quest-giver hubs are
// the wrong picture of a capital (the quest givers of Ironforge sit in the ward entrance rooms, the crowd stands in the Commons),
// so a town's residents are placed and sent to the service clusters instead. Pure part: clustering and the weighted pick.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Playerbot::V2::Fleet
{

struct GatherNpc
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float weight = 0.0f;   // how much a crowd forms around this NPC: bank / auctioneer / inn high, a lone trainer or vendor low
};

struct GatherSpot
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float weight = 0.0f;
};

constexpr float kGatherCell      = 36.0f;   // NPCs this close share a spot
constexpr float kGatherMinWeight = 4.0f;    // a spot needs a bank's worth: a room of trainers is not a place to stand around
constexpr float kGatherRadius    = 16.0f;   // residents spread this far around a spot

// Clusters the NPCs into grid cells; each cell with enough weight is one spot, at the weighted centre, heaviest first.
inline std::vector<GatherSpot> BuildGatherSpots(std::vector<GatherNpc> const& npcs)
{
    struct Acc { int cx, cy; float w = 0, sx = 0, sy = 0, sz = 0; };
    std::vector<Acc> cells;
    for (GatherNpc const& n : npcs)
    {
        if (n.weight <= 0.0f) continue;
        int const cx = int(std::floor(n.x / kGatherCell)), cy = int(std::floor(n.y / kGatherCell));
        auto it = std::find_if(cells.begin(), cells.end(), [&](Acc const& a) { return a.cx == cx && a.cy == cy; });
        if (it == cells.end()) { cells.push_back(Acc{ cx, cy }); it = cells.end() - 1; }
        it->w += n.weight; it->sx += n.x * n.weight; it->sy += n.y * n.weight; it->sz += n.z * n.weight;
    }
    std::vector<GatherSpot> out;
    for (Acc const& a : cells)
        if (a.w >= kGatherMinWeight)
            out.push_back({ a.sx / a.w, a.sy / a.w, a.sz / a.w, a.w });
    std::sort(out.begin(), out.end(), [](GatherSpot const& l, GatherSpot const& r) { return l.weight > r.weight; });
    return out;
}

// Index of a spot drawn in proportion to its weight; roll in [0, 1). -1 when there are none.
inline int PickGatherSpot(std::vector<GatherSpot> const& spots, float roll)
{
    float total = 0.0f;
    for (GatherSpot const& s : spots) total += s.weight;
    if (spots.empty() || total <= 0.0f) return -1;
    float t = roll * total;
    for (size_t i = 0; i < spots.size(); ++i)
    {
        t -= spots[i].weight;
        if (t < 0.0f) return int(i);
    }
    return int(spots.size()) - 1;
}

} // namespace Playerbot::V2::Fleet
