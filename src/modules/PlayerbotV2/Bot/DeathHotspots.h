// DeathHotspots - where bots keep dying. Every bot death by a creature is noted with the killer's level; a 100 y cell
// that saw DeathsNeeded deaths within WindowSec is a hotspot, and it is dangerous for a bot that is more than
// LevelMargin levels below the strongest killer seen there. Quest pursuit uses it to give up objectives inside a
// hotspot, so a bot stops walking into the camp that has just killed a dozen of its peers (Booty Bay Bloodsail camp,
// 2026-10-07: 20 Fireball deaths). Written on the world thread (Module::OnDeath), read from the bot workers.

#pragma once

#include "Define.h"
#include <cmath>
#include <deque>
#include <mutex>
#include <unordered_map>

namespace Playerbot {

class DeathHotspots
{
public:
    static constexpr float  kCell         = 100.0f;
    static constexpr uint32 kWindowSec    = 20u * 60u;
    static constexpr uint32 kDeathsNeeded = 3;
    static constexpr int    kLevelMargin  = 2;

    static DeathHotspots& Instance()
    {
        static DeathHotspots inst;
        return inst;
    }

    void NoteDeath(uint32 map, float x, float y, uint8 killer_level, uint64 now_s)
    {
        std::lock_guard<std::mutex> lk(mtx_);
        std::deque<Death>& q = cells_[Key(map, x, y)];
        Prune(q, now_s);
        q.push_back({ now_s, killer_level });
        if (q.size() > 64) q.pop_front();
    }

    // True when (x,y) lies in a hotspot whose strongest recent killer is more than kLevelMargin levels above the bot.
    bool Dangerous(uint32 map, float x, float y, uint8 bot_level, uint64 now_s)
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = cells_.find(Key(map, x, y));
        if (it == cells_.end()) return false;
        Prune(it->second, now_s);
        if (it->second.size() < kDeathsNeeded) return false;
        uint8 top = 0;
        for (Death const& d : it->second) top = d.killer_level > top ? d.killer_level : top;
        return int(top) - int(bot_level) > kLevelMargin;
    }

private:
    struct Death { uint64 at_s; uint8 killer_level; };

    static uint64 Key(uint32 map, float x, float y)
    {
        int32 const cx = int32(std::floor(x / kCell));
        int32 const cy = int32(std::floor(y / kCell));
        return (uint64(map) << 40) ^ (uint64(uint32(cx) & 0xFFFFF) << 20) ^ uint64(uint32(cy) & 0xFFFFF);
    }
    static void Prune(std::deque<Death>& q, uint64 now_s)
    {
        while (!q.empty() && now_s > q.front().at_s + kWindowSec) q.pop_front();
    }

    std::mutex mtx_;
    std::unordered_map<uint64, std::deque<Death>> cells_;
};

} // namespace Playerbot
