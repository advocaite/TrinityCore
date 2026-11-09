// Dungeon entrances the travel graph cannot walk to.
//
// An entrance node (NodeKind::DungeonEntry) whose walk piece holds nothing but entrance nodes is an "entrance pocket": no flight
// master, quest hub, town or dock shares its piece, so no route can reach it on foot. The two seen in the [walk_gaps] lines:
//   - Scholomance (1275,-2553), alone on Caer Darrow. ConnectIslands bridged it to its nearest node by straight line, Aerie Peak in
//     the Hinterlands, 1,133 y away over the Alterac foothills, so a rejoin route was "fly to Aerie Peak, walk over the mountains".
//   - Ruins of Ahn'Qiraj and Ahn'Qiraj Temple (-8112,1527), two entrances joined to each other only. They had walk edges, so
//     ConnectIslands never touched them, and no route reached either one: a companion rejoining its owner in AQ20 waited forever.
//
// The fix: the planner never uses a pocket node as a route end. A goal at the entrance then attaches to the real places within
// kFarAttachYds (Cenarion Hold for AQ, Chillwind Camp and the Western Plaguelands hubs for Scholomance) and walks the last stretch on
// the navmesh; a bot that comes out of the dungeon attaches the same way. A pocket entrance gets no invented Bridge unless no real
// place lies within kFarAttachYds (then the old bridge stays, so an entrance far from everything is no worse off than before).
//
// Kept free of game types so it can be unit tested.
#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace Playerbot::V2::Travel
{

// is_entrance[i]: node i is a dungeon entrance. walk_edges: undirected walk links between node indices (0 .. is_entrance.size()-1).
// Returns, per node, true when the node lies in a walk piece made only of entrance nodes.
inline std::vector<bool> FindEntrancePockets(std::vector<bool> const& is_entrance,
                                             std::vector<std::pair<std::size_t, std::size_t>> const& walk_edges)
{
    std::size_t const n = is_entrance.size();
    std::vector<std::size_t> parent(n);
    for (std::size_t i = 0; i < n; ++i)
        parent[i] = i;
    auto const find = [&](std::size_t v)
    {
        while (parent[v] != v)
            v = parent[v] = parent[parent[v]];
        return v;
    };
    for (auto const& [a, b] : walk_edges)
        if (a < n && b < n)
            parent[find(a)] = find(b);

    std::vector<bool> has_real(n, false);   // indexed by root
    for (std::size_t i = 0; i < n; ++i)
        if (!is_entrance[i])
            has_real[find(i)] = true;

    std::vector<bool> pocket(n, false);
    for (std::size_t i = 0; i < n; ++i)
        pocket[i] = is_entrance[i] && !has_real[find(i)];
    return pocket;
}

// ConnectIslands: does a node with no walk edge get its straight-line Bridge to the nearest node? Yes for every node but a pocket
// entrance that a real place within far_attach_yds can serve (the planner reaches that entrance by its far attach instead).
inline bool KeepIslandBridge(bool entrance_pocket, float nearest_real_place_yds, float far_attach_yds)
{
    return !entrance_pocket || nearest_real_place_yds > far_attach_yds;
}

} // namespace Playerbot::V2::Travel
