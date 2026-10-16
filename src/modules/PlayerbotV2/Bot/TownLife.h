// TownLife - the small shows of a living town: bots resting at the inn at night, pairs duelling, a campfire, an LFG call for a
// dungeon of their level. Pure rules and one small shared table; the rules that use them are in Bot/States/Rules/AmbientRules.cpp.
//
// The cost is kept low by taking part, not by counting: a bot takes part in a show only when a hash of (bot, show, time window) falls
// under the show's share, so only a few percent of the world bots are ever on show at once, with no global counter. Tested in
// tests/game/ClassicTownLife.cpp.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace Playerbot::V2::TownLife
{

// What the shows did since the server started, for the [world] report line. Counted when a rule fires, so inn_ticks and mailbox_ticks
// are bot-ticks on the show (walking or standing), duels and campfires are single events. idle_town and idle_field count idle-watch
// strikes (a bot stood still too long), split by capital and elsewhere, which is how a frozen bot shows up in the log.
struct Stats
{
    std::atomic<std::uint32_t> inn_ticks{0}, mailbox_ticks{0}, duels{0}, campfires{0}, lfg_calls{0}, idle_town{0}, idle_field{0};
};
inline Stats& Counters()
{
    static Stats stats;
    return stats;
}

// Does this bot take part in `show` during the window of `window_ms` that `now_ms` falls in? `pct` of the bots do, a different set
// each window.
inline bool TakesPart(std::uint32_t bot_id, std::uint32_t show, std::uint32_t now_ms, std::uint32_t pct, std::uint32_t window_ms)
{
    if (!pct || !window_ms) return false;
    std::uint32_t const w = now_ms / window_ms;
    std::uint32_t h = (bot_id + 1u) * 2654435761u;
    h ^= (w + 1u) * 2246822519u;
    h ^= (show + 1u) * 3266489917u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return (h % 100u) < pct;
}

// Share of the bots resting at the inn for the local hour: most at night, some in the evening, few by day.
inline std::uint32_t InnPercent(int hour)
{
    if (hour < 0 || hour > 23) return 8;
    if (hour < 6 || hour >= 23) return 35;
    if (hour >= 18) return 20;
    return 8;
}

// Duel partners: close in level, both healthy. The lower id challenges so a pair does not challenge each other at once.
inline bool DuelPartners(std::uint32_t level_a, int hp_pct_a, std::uint32_t level_b, int hp_pct_b, std::uint64_t id_a, std::uint64_t id_b)
{
    int const d = int(level_a) - int(level_b);
    return d >= -3 && d <= 3 && hp_pct_a >= 90 && hp_pct_b >= 90 && id_a < id_b;
}

// Dungeons of the 1.12 game by the level of the party that runs them, for an LFG call. nullptr below the first.
struct LfgBand { std::uint32_t lo, hi; char const* name; };
inline constexpr LfgBand kLfgBands[] = {
    { 13, 17, "Ragefire Chasm" }, { 15, 20, "Wailing Caverns" }, { 18, 23, "the Deadmines" }, { 22, 27, "Shadowfang Keep" },
    { 24, 29, "Blackfathom Deeps" }, { 26, 31, "the Stockade" }, { 29, 34, "Gnomeregan" }, { 30, 36, "Razorfen Kraul" },
    { 34, 41, "the Scarlet Monastery" }, { 37, 45, "Razorfen Downs" }, { 40, 47, "Uldaman" }, { 44, 51, "Zul'Farrak" },
    { 46, 54, "Maraudon" }, { 50, 57, "the Sunken Temple" }, { 52, 60, "Blackrock Depths" }, { 58, 60, "Stratholme" },
};

// The dungeon a bot of `level` calls for, one of the bands it fits, chosen by `pick`. nullptr when none fits.
inline char const* LfgDungeonFor(std::uint32_t level, std::uint32_t pick)
{
    std::uint32_t fit[16];
    std::size_t n = 0;
    for (LfgBand const& b : kLfgBands)
        if (level >= b.lo && level <= b.hi && n < 16) fit[n++] = std::uint32_t(&b - kLfgBands);
    return n ? kLfgBands[fit[pick % n]].name : nullptr;
}

// A duel one bot asked another for: the one asked accepts it. Entries expire; the table is tiny (a few pairs at a time).
inline std::mutex& PactMutex() { static std::mutex m; return m; }
inline std::unordered_map<std::uint64_t, std::pair<std::uint64_t, std::uint32_t>>& Pacts()
{
    static std::unordered_map<std::uint64_t, std::pair<std::uint64_t, std::uint32_t>> t;
    return t;
}

inline void PactSet(std::uint64_t target, std::uint64_t initiator, std::uint32_t now_ms)
{
    std::lock_guard<std::mutex> g(PactMutex());
    auto& t = Pacts();
    if (t.size() > 256)
        for (auto it = t.begin(); it != t.end();)
            it = (now_ms - it->second.second > 60000u) ? t.erase(it) : std::next(it);
    t[target] = { initiator, now_ms };
}

inline bool PactHolds(std::uint64_t target, std::uint64_t initiator, std::uint32_t now_ms)
{
    std::lock_guard<std::mutex> g(PactMutex());
    auto const it = Pacts().find(target);
    return it != Pacts().end() && it->second.first == initiator && now_ms - it->second.second <= 30000u;
}

} // namespace Playerbot::V2::TownLife
