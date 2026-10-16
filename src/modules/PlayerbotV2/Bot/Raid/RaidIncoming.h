// Damage the raid took in the current pull, by the creature that dealt it and by whether the victim was a tank, printed as [raid_incoming]
// lines at the pull end. The healers' mana is the raid's incoming damage: this says where it came from, so that what a tank or a rule could
// have kept away from the cloth members is visible (n24 mc_garr: 7 deaths, all to Firesworn melee on mages and healers, but no log said how
// much of the healing went there).
// Written from the world thread by the damage hook; the raid brain registers the roles and reads it.
#pragma once

#include "Define.h"
#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Playerbot::Raid {

namespace detail {
struct IncomingRow { std::string name; uint64 tank = 0; uint64 other = 0; uint32 hits_tank = 0; uint32 hits_other = 0; };
inline std::mutex g_incMu;
inline std::unordered_map<uint64, bool> g_incRole;                 // watched victim -> is a tank
inline std::unordered_map<uint32, IncomingRow> g_incRows;          // attacker creature entry
struct DealtRow { std::string name; uint64 damage = 0; uint32 hits = 0; };
inline std::unordered_map<uint32, DealtRow> g_dealtRows;           // enemy creature entry the raid hit
} // namespace detail

inline void IncomingReset()
{
    std::lock_guard<std::mutex> lk(detail::g_incMu);
    detail::g_incRole.clear();
    detail::g_incRows.clear();
    detail::g_dealtRows.clear();
}

inline void IncomingWatch(uint64 victim_low, bool tank)
{
    std::lock_guard<std::mutex> lk(detail::g_incMu);
    detail::g_incRole[victim_low] = tank;
}

// A creature <entry> hit a player for <damage>.
inline void IncomingHit(uint32 entry, char const* name, uint64 victim_low, uint32 damage)
{
    std::lock_guard<std::mutex> lk(detail::g_incMu);
    auto const role = detail::g_incRole.find(victim_low);
    if (role == detail::g_incRole.end())
        return;
    auto& row = detail::g_incRows[entry];
    if (row.name.empty() && name)
        row.name = name;
    if (role->second) { row.tank += damage; ++row.hits_tank; }
    else              { row.other += damage; ++row.hits_other; }
}

// A watched raider dealt <damage> to the creature <entry>.
inline void DealtHit(uint32 entry, char const* name, uint64 attacker_low, uint32 damage)
{
    std::lock_guard<std::mutex> lk(detail::g_incMu);
    if (!detail::g_incRole.count(attacker_low))
        return;
    auto& row = detail::g_dealtRows[entry];
    if (row.name.empty() && name)
        row.name = name;
    row.damage += damage;
    ++row.hits;
}

// What the raid hit, biggest first: the answer to "where did the damage go" (n32 mc_majordomo: 221k dealt, the adds at 89-99 %).
inline std::vector<std::string> DealtTake(uint32 duration_s, size_t max_rows = 10)
{
    std::lock_guard<std::mutex> lk(detail::g_incMu);
    std::vector<std::pair<uint32, detail::DealtRow>> rows(detail::g_dealtRows.begin(), detail::g_dealtRows.end());
    std::sort(rows.begin(), rows.end(), [](auto const& a, auto const& b) { return a.second.damage > b.second.damage; });
    uint32 const secs = std::max<uint32>(duration_s, 1u);
    std::vector<std::string> out;
    for (auto const& [entry, r] : rows)
    {
        if (out.size() >= max_rows)
            break;
        out.push_back(r.name + " (" + std::to_string(entry) + ") took " + std::to_string(r.damage) + " in " + std::to_string(r.hits) + " hits (" +
                      std::to_string(r.damage / secs) + "/s)");
    }
    detail::g_dealtRows.clear();
    return out;
}

// The biggest sources first (by total), at most <max_rows>; clears the table.
inline std::vector<std::string> IncomingTake(uint32 duration_s, size_t max_rows = 8)
{
    std::lock_guard<std::mutex> lk(detail::g_incMu);
    std::vector<std::pair<uint32, detail::IncomingRow>> rows(detail::g_incRows.begin(), detail::g_incRows.end());
    std::sort(rows.begin(), rows.end(), [](auto const& a, auto const& b) { return a.second.tank + a.second.other > b.second.tank + b.second.other; });
    uint32 const secs = std::max<uint32>(duration_s, 1u);
    std::vector<std::string> out;
    for (auto const& [entry, r] : rows)
    {
        if (out.size() >= max_rows)
            break;
        out.push_back(r.name + " (" + std::to_string(entry) + ") on non-tanks " + std::to_string(r.other) + " in " + std::to_string(r.hits_other) +
                      " hits (" + std::to_string(r.other / secs) + "/s), on tanks " + std::to_string(r.tank) + " in " + std::to_string(r.hits_tank) + " hits");
    }
    detail::g_incRole.clear();
    detail::g_incRows.clear();
    return out;
}

} // namespace Playerbot::Raid
