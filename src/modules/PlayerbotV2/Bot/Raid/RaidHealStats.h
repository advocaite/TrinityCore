// Per-healer heal casts of the current raid pull, for the [raid_encounter] pull end line ("healer throughput").
// Written by the heal emitter (bot worker threads), read and reset by the raid brain.
#pragma once

#include "Define.h"
#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Playerbot::Raid {

namespace detail {
struct HealStat { std::string name; uint32 casts = 0; uint32 first_ms = 0; uint32 last_ms = 0; };
inline std::mutex g_healStatMu;
inline std::unordered_map<uint64, HealStat> g_healStats;
} // namespace detail

// One heal cast was sent by `guid_low` (raid healing only).
inline void NoteHealCastSent(uint64 guid_low, std::string const& name, uint32 now_ms)
{
    std::lock_guard<std::mutex> lk(detail::g_healStatMu);
    auto& st = detail::g_healStats[guid_low];
    if (st.casts == 0) { st.name = name; st.first_ms = now_ms; }
    ++st.casts;
    st.last_ms = now_ms;
}

inline void ResetHealStats()
{
    std::lock_guard<std::mutex> lk(detail::g_healStatMu);
    detail::g_healStats.clear();
}

// "Name 12/min, Name2 3/min" over the pull of `duration_s` seconds, healers that cast nothing are not listed here (the
// caller names the healer count). Returns the text and clears the stats.
inline std::string TakeHealStats(uint32 duration_s, uint32& healers_cast)
{
    std::lock_guard<std::mutex> lk(detail::g_healStatMu);
    std::vector<detail::HealStat> rows;
    for (auto const& [g, st] : detail::g_healStats)
        rows.push_back(st);
    detail::g_healStats.clear();
    healers_cast = uint32(rows.size());
    std::sort(rows.begin(), rows.end(), [](detail::HealStat const& a, detail::HealStat const& b) { return a.name < b.name; });
    std::string out;
    uint32 const secs = std::max<uint32>(duration_s, 1u);
    for (detail::HealStat const& r : rows)
    {
        if (!out.empty()) out += ", ";
        out += r.name + " " + std::to_string(r.casts * 60u / secs) + "/min(" + std::to_string(r.casts) + ")";
    }
    return out;
}

} // namespace Playerbot::Raid
