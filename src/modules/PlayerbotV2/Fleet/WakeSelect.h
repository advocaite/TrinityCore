// WakeSelect - which sleeping residents wake for a player: the pure rule behind WorldBotSpawner::DriveWake.
//
// A sleeper wakes only when it is standing between MinDist and MaxDist of the player (not beside him: it would pop into view),
// is of the wanted faction, and - when the player is in a capital - stands in the capital's own zone, because the target of a
// capital counts only bots of that zone (WorldBotSpawner::CountAmbientNear). Without the zone rule the countryside around a
// city woke up as well, did not count toward the city's target, and the spawner kept waking until nothing was left (the owner,
// Stormwind, 2026-10-09: 280 awake for CityBots 250). At most `want` wake.
// Pure: no game types, tested in tests/game/ClassicWakeSelect.cpp.

#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Playerbot::V2::Fleet {

struct WakeCandidate
{
    uint64_t guid = 0;
    float    dist = 0.0f;      // 2D distance from the player
    uint32_t zone = 0;         // zone saved at logout
    bool     faction_ok = true;// passed the FriendlyPct roll
    bool     wet = false;      // its saved position has water over it (logged out in a canal): it must not wake there
};

struct WakeRule
{
    float    min_dist = 60.0f;
    float    max_dist = 450.0f;
    bool     capital = false;  // the player stands in a capital: only sleepers of `anchor_zone` count
    uint32_t anchor_zone = 0;
};

inline bool WakeEligible(WakeCandidate const& c, WakeRule const& r)
{
    if (!c.faction_ok || c.wet) return false;
    if (c.dist < r.min_dist || c.dist > r.max_dist) return false;
    if (r.capital && c.zone != r.anchor_zone) return false;
    return true;
}

// Candidates in the order given; stops at `want`.
inline std::vector<uint64_t> SelectWake(std::vector<WakeCandidate> const& in, WakeRule const& r, uint32_t want)
{
    std::vector<uint64_t> out;
    for (WakeCandidate const& c : in)
    {
        if (out.size() >= want) break;
        if (WakeEligible(c, r)) out.push_back(c.guid);
    }
    return out;
}

// Fair share across the zones around the player. `awake_by_zone` is how many ambient bots already stand awake in each zone within
// the activity radius. Outdoors the player's own zone (rule.anchor_zone) is to hold about home_share of all the awake bots around
// him and the neighbouring zones the rest; each pick takes a sleeper of the group that is below its share, from the zone with the
// fewest awake bots in that group. A capital keeps its own zone only (WakeEligible). Candidates arrive in random order, so ties
// resolve at random. Stops at `want`.
inline std::vector<uint64_t> SelectWakeShared(std::vector<WakeCandidate> const& in, WakeRule const& r, uint32_t want,
                                              std::unordered_map<uint32_t, uint32_t> awake_by_zone, float home_share = 0.6f)
{
    std::vector<uint64_t> out;
    std::vector<WakeCandidate const*> pool;
    for (WakeCandidate const& c : in)
        if (WakeEligible(c, r)) pool.push_back(&c);
    while (out.size() < want && !pool.empty())
    {
        uint64_t total = 0;
        for (auto const& kv : awake_by_zone) total += kv.second;
        uint32_t const home_awake = awake_by_zone.contains(r.anchor_zone) ? awake_by_zone[r.anchor_zone] : 0u;
        bool const want_home = r.capital || total == 0 || float(home_awake) < home_share * float(total);
        auto best = pool.end();
        uint32_t best_n = 0;
        for (auto it = pool.begin(); it != pool.end(); ++it)
        {
            bool const is_home = (*it)->zone == r.anchor_zone;
            if (is_home != want_home) continue;
            uint32_t const n = awake_by_zone.contains((*it)->zone) ? awake_by_zone[(*it)->zone] : 0u;
            if (best == pool.end() || n < best_n) { best = it; best_n = n; }
        }
        if (best == pool.end())   // nobody of the wanted group sleeps nearby: the other group
            for (auto it = pool.begin(); it != pool.end(); ++it)
            {
                uint32_t const n = awake_by_zone.contains((*it)->zone) ? awake_by_zone[(*it)->zone] : 0u;
                if (best == pool.end() || n < best_n) { best = it; best_n = n; }
            }
        out.push_back((*best)->guid);
        ++awake_by_zone[(*best)->zone];
        pool.erase(best);
    }
    return out;
}

} // namespace Playerbot::V2::Fleet
