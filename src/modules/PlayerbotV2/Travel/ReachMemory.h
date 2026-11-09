// ReachMemory - what the path checks have already learned about "from around here, that goal cannot be walked to". A failed path (no path,
// a goal off the mesh, a partial route that stops short or loops) is remembered per pair of cells (50 y around the start, 25 y around the
// goal) for every bot, so a second bot standing in the same place does not pay the same Detour call and goal selection can skip a goal
// before committing to it.
// A complete path forgets the pair. Pure: no game types, tested in tests/game/ClassicReachMemory.cpp.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Playerbot::V2::Travel {

class ReachMemory
{
public:
    static constexpr float         kFromCellYds = 50.0f;
    static constexpr float         kToCellYds = 25.0f;
    static constexpr std::uint32_t kTtlMs = 10u * 60u * 1000u;   // geometry does not change; a data fix arrives with a restart
    static constexpr std::uint32_t kMinFails = 2;                // one failure can be a bot standing in a bad spot
    static constexpr std::size_t   kCap = 4096;                  // cells remembered; the oldest go first

    void NoteFail(std::uint32_t map, float fx, float fy, float tx, float ty, std::uint32_t now_ms)
    {
        std::lock_guard<std::mutex> lk(mx_);
        Entry& e = cells_[KeyOf(map, fx, fy, tx, ty)];
        if (e.last_ms && Expired(e, now_ms))
            e.fails = 0;
        ++e.fails;
        e.last_ms = now_ms ? now_ms : 1u;
        ++noted_;
        if (cells_.size() > kCap)
            Prune(now_ms);
    }

    void NoteOk(std::uint32_t map, float fx, float fy, float tx, float ty)
    {
        std::lock_guard<std::mutex> lk(mx_);
        if (!cells_.empty())
            cells_.erase(KeyOf(map, fx, fy, tx, ty));
    }

    bool KnownUnreachable(std::uint32_t map, float fx, float fy, float tx, float ty, std::uint32_t now_ms)
    {
        std::lock_guard<std::mutex> lk(mx_);
        auto it = cells_.find(KeyOf(map, fx, fy, tx, ty));
        bool const hit = it != cells_.end() && it->second.fails >= kMinFails && !Expired(it->second, now_ms);
        if (hit)
            ++skips_;
        return hit;
    }

    std::size_t   size() const { std::lock_guard<std::mutex> lk(mx_); return cells_.size(); }
    std::uint64_t noted() const { std::lock_guard<std::mutex> lk(mx_); return noted_; }
    std::uint64_t skips() const { std::lock_guard<std::mutex> lk(mx_); return skips_; }

    static ReachMemory& Instance()
    {
        static ReachMemory inst;
        return inst;
    }

private:
    struct Key
    {
        std::uint16_t map;
        std::int16_t  fx, fy, tx, ty;
        bool operator==(Key const& o) const { return map == o.map && fx == o.fx && fy == o.fy && tx == o.tx && ty == o.ty; }
    };
    struct KeyHash
    {
        std::size_t operator()(Key const& k) const
        {
            std::uint64_t h = k.map;
            h = h * 1000003u + std::uint16_t(k.fx);
            h = h * 1000003u + std::uint16_t(k.fy);
            h = h * 1000003u + std::uint16_t(k.tx);
            h = h * 1000003u + std::uint16_t(k.ty);
            return std::size_t(h ^ (h >> 29));
        }
    };
    struct Entry { std::uint32_t last_ms = 0; std::uint32_t fails = 0; };

    static std::int16_t Cell(float v, float size) { return std::int16_t(std::clamp(std::floor(v / size), -32000.0f, 32000.0f)); }
    static Key KeyOf(std::uint32_t map, float fx, float fy, float tx, float ty)
    {
        return Key{ std::uint16_t(map), Cell(fx, kFromCellYds), Cell(fy, kFromCellYds), Cell(tx, kToCellYds), Cell(ty, kToCellYds) };
    }
    static bool Expired(Entry const& e, std::uint32_t now_ms) { return std::uint32_t(now_ms - e.last_ms) >= kTtlMs; }

    // Over the cap: the expired cells go first, then the oldest quarter.
    void Prune(std::uint32_t now_ms)
    {
        for (auto it = cells_.begin(); it != cells_.end();)
            it = Expired(it->second, now_ms) ? cells_.erase(it) : std::next(it);
        if (cells_.size() <= kCap)
            return;
        std::vector<std::uint32_t> ages;
        ages.reserve(cells_.size());
        for (auto const& [k, e] : cells_)
            ages.push_back(std::uint32_t(now_ms - e.last_ms));
        std::size_t const drop = ages.size() / 4;
        std::nth_element(ages.begin(), ages.begin() + std::ptrdiff_t(drop), ages.end(), std::greater<>());
        std::uint32_t const cut = ages[drop];
        for (auto it = cells_.begin(); it != cells_.end();)
            it = std::uint32_t(now_ms - it->second.last_ms) >= cut ? cells_.erase(it) : std::next(it);
    }

    mutable std::mutex mx_;
    std::unordered_map<Key, Entry, KeyHash> cells_;
    std::uint64_t noted_ = 0;
    std::uint64_t skips_ = 0;
};

// QuestGoalMemory - the same idea for a quest objective: when bots keep giving up on the walk to one quest's goal (the tracker drops the goal
// after it made no progress), the other bots stop starting that quest for a while instead of each paying the same 90 s wedge
//. Per quest, not per place: the objective is one point.
// Several DIFFERENT bots must have given up, not one bot twice, and the memory lasts 10 minutes. With two drops (any bots) and 30
// minutes, renewed by every new drop, Shadowglen's first quest The Balance of Nature (456) was skipped by every bot of the zone in
// 0.1.34: the bots there wedge on Aldrassil's roots now and then, two drops locked the quest for the whole fleet, and as every
// other Shadowglen quest follows it, 40 bots of level 1 to 5 stood with empty quest logs and wandered (9,599 quest_offer_refused,
// 2,556 quest_truly_stuck). The offer scan also leaves this memory out for starting-zone quests (BotSnapshotBuilder).
class QuestGoalMemory
{
public:
    static constexpr std::uint32_t kTtlMs = 10u * 60u * 1000u;
    static constexpr std::uint32_t kMinBots = 3;   // one bot can be stuck for its own reasons (a pack on the path, its own gear)

    void NoteDropped(std::uint32_t quest_id, std::uint64_t bot_id, std::uint32_t now_ms)
    {
        if (quest_id == 0)
            return;
        std::lock_guard<std::mutex> lk(mx_);
        Entry& e = quests_[quest_id];
        if (e.bots.empty() || std::uint32_t(now_ms - e.last_ms) >= kTtlMs)
            e.bots.clear();
        if (std::find(e.bots.begin(), e.bots.end(), bot_id) == e.bots.end() && e.bots.size() < 8)
            e.bots.push_back(bot_id);
        e.last_ms = now_ms;
    }

    bool KnownUnreachable(std::uint32_t quest_id, std::uint32_t now_ms) const
    {
        if (quest_id == 0)
            return false;
        std::lock_guard<std::mutex> lk(mx_);
        auto const it = quests_.find(quest_id);
        return it != quests_.end() && it->second.bots.size() >= kMinBots && std::uint32_t(now_ms - it->second.last_ms) < kTtlMs;
    }

    static QuestGoalMemory& Instance()
    {
        static QuestGoalMemory inst;
        return inst;
    }

private:
    struct Entry { std::uint32_t last_ms = 0; std::vector<std::uint64_t> bots; };
    mutable std::mutex mx_;
    std::unordered_map<std::uint32_t, Entry> quests_;
};

} // namespace Playerbot::V2::Travel
