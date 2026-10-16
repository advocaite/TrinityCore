// RaidRoster - the raid's tank and healer quota, filled from companions that have no role yet.
//
// A 40-player raid needs about one healer in three and four tanks, and the owner's companions are mostly paladins and druids
// whose role was never set (OwnerRegistry::DeclaredRole == Unknown). PlanRaidRoles picks which of those take a healer or
// tank role so the raid has its quota; a role anyone gave (the owner's /setrole, a stored inference) is never changed.
// Pure: no game types, tested in tests/game/ClassicRaidRoster.cpp.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Playerbot::Raid {

enum class RosterRole : uint8_t { Dps, Tank, Healer };

struct RosterBot
{
    uint64_t   guid = 0;
    uint8_t    cls = 0;           // Classes enum value (1 warrior, 2 paladin, 5 priest, 7 shaman, 11 druid)
    bool       role_set = false;  // a role was declared for this companion: never changed
    RosterRole role = RosterRole::Dps;
};

struct RosterTargets { uint32_t tanks = 0; uint32_t healers = 0; };

struct RoleChange { uint64_t guid; RosterRole role; };

// Quota for a raid of `size`: a healer per 3.3 raiders (12 of 40), four tanks from 25 up, two from 10.
inline RosterTargets RosterTargetsFor(uint32_t size)
{
    RosterTargets t;
    if (size >= 10)
        t.healers = (size * 3u + 9u) / 10u;
    t.tanks = size >= 25 ? 4u : size >= 10 ? 2u : 0u;
    return t;
}

namespace detail {
constexpr std::array<uint8_t, 4> kHealerClasses = { 5, 2, 11, 7 };   // priest, paladin, druid, shaman
constexpr std::array<uint8_t, 3> kTankClasses   = { 1, 11, 2 };      // warrior, druid, paladin

// The lowest-guid candidate of `cls` leaves the pool and takes `role`. False when the class has none left.
inline bool TakeOne(std::vector<RosterBot const*>& pool, uint8_t cls, RosterRole role, std::vector<RoleChange>& out)
{
    auto best = pool.end();
    for (auto it = pool.begin(); it != pool.end(); ++it)
        if ((*it)->cls == cls && (best == pool.end() || (*it)->guid < (*best)->guid))
            best = it;
    if (best == pool.end())
        return false;
    out.push_back({ (*best)->guid, role });
    pool.erase(best);
    return true;
}

// Fills `want` roles from `classes`. spread: one per class per round, so the raid keeps a mix of classes (healers, for the
// buffs); otherwise a class is used up before the next is touched (tanks: warriors first).
template <size_t N>
void Take(std::vector<RosterBot const*>& pool, std::array<uint8_t, N> const& classes, uint32_t want, RosterRole role,
          std::vector<RoleChange>& out, bool spread)
{
    while (want > 0)
    {
        bool any = false;
        for (uint8_t cls : classes)
        {
            while (want > 0 && TakeOne(pool, cls, role, out))
            {
                --want;
                any = true;
                if (spread)
                    break;
            }
        }
        if (!any)
            return;
    }
}
} // namespace detail

// The role changes that bring the raid to `targets`. Healers are filled first, then tanks, only from bots with no declared
// role that currently play DPS and whose class can do the job.
inline std::vector<RoleChange> PlanRaidRoles(std::vector<RosterBot> const& roster, RosterTargets targets)
{
    uint32_t tanks = 0, healers = 0;
    std::vector<RosterBot const*> pool;
    for (RosterBot const& b : roster)
    {
        tanks += b.role == RosterRole::Tank;
        healers += b.role == RosterRole::Healer;
        if (!b.role_set && b.role == RosterRole::Dps)
            pool.push_back(&b);
    }
    std::vector<RoleChange> out;
    if (healers < targets.healers)
        detail::Take(pool, detail::kHealerClasses, targets.healers - healers, RosterRole::Healer, out, /*spread*/ true);
    if (tanks < targets.tanks)
        detail::Take(pool, detail::kTankClasses, targets.tanks - tanks, RosterRole::Tank, out, /*spread*/ false);
    return out;
}

} // namespace Playerbot::Raid
