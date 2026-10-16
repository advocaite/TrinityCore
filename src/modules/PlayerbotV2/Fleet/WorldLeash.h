// WorldLeash - the open-world anchors (real players and recently left spots) as the world bot spawner sees them,
// published for the bot bot worker threads. An ambient bot that picks a quest objective far from every anchor on its map
// would walk out of the population bubble and be logged out as an orphan (levelling lost, a new bot spawned instead);
// idle:pursue_quest_goal asks LeashBlocks() and takes another objective.
#pragma once

#include "Define.h"
#include <mutex>
#include <vector>

namespace Playerbot::V2::Fleet {

class WorldLeash
{
public:
    struct Anchor { uint32 map; float x, y; };

    static void Publish(std::vector<Anchor> anchors, float radius)
    {
        std::lock_guard<std::mutex> g(Mutex());
        Anchors() = std::move(anchors);
        Radius() = radius;
    }
    static void Clear() { Publish({}, 0.0f); }

    // True when there are anchors on `map` and the point is farther than the leash radius from all of them.
    static bool Blocks(uint32 map, float x, float y)
    {
        std::lock_guard<std::mutex> g(Mutex());
        float const r = Radius();
        if (r <= 0.0f) return false;
        bool any = false;
        for (Anchor const& a : Anchors())
        {
            if (a.map != map) continue;
            any = true;
            float const dx = a.x - x, dy = a.y - y;
            if (dx * dx + dy * dy <= r * r) return false;
        }
        return any;
    }

private:
    static std::mutex& Mutex() { static std::mutex m; return m; }
    static std::vector<Anchor>& Anchors() { static std::vector<Anchor> a; return a; }
    static float& Radius() { static float r = 0.0f; return r; }
};

} // namespace Playerbot::V2::Fleet
