// RaidHealGroups - which raid subgroup each triage healer looks after first. Pure; tested in tests/game/ClassicRaidHealGroups.cpp.
//
// the owner, live MC 2026-10-10: when a warlock Life Taps, druids, priests and paladins all put HoTs and heals on him. Every triage healer
// picked the lowest member of the whole raid, so the whole raid's healers piled onto the same few members. Now each raid subgroup has
// one healer who heals it first (RaidHealGroups.h -> PveOrder::heal_groups -> ClassicHeal::PickPatient); the tank healers keep their
// tanks (RaidHealSlots.h) and are not in this plan.
//
// Sticky: a healer keeps the groups it had while it can still heal. A group whose healer died or ran dry goes to a healer with no
// group (a floater) if there is one, else to the healer of the nearest following group, which then covers both until the first is back.
// When a healer returns and another covers two groups, one of them goes back. With fewer healers than groups some cover several.

#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace Playerbot::Raid {

// Under this mana a healer is out of the plan (its groups are covered by the others) until it recovers.
constexpr int kHealGroupOutManaPct = 10;
constexpr int kHealGroupBackManaPct = 20;   // a healer that dropped out returns above this

struct GroupHealer
{
    uint64_t id = 0;
    int mana_pct = 100;
};

inline bool HealGroupHas(uint16_t mask, uint8_t group)
{
    return group < 16 && (mask & (1u << group)) != 0;
}

// `healers`: the triage healers that are alive. `groups`: the raid subgroups that have a live member, ascending. `previous`: healer ->
// mask of groups it covered in the last plan. Returns healer -> mask (healers with no group are left out).
inline std::map<uint64_t, uint16_t> AssignHealGroups(std::vector<GroupHealer> const& healers, std::vector<uint8_t> const& groups,
                                                      std::map<uint64_t, uint16_t> const& previous)
{
    std::map<uint64_t, uint16_t> result;
    std::vector<uint64_t> avail;
    // A healer already in the plan stays in down to kHealGroupOutManaPct; one that left needs kHealGroupBackManaPct to return, so a healer
    // whose mana hovers at the mark does not drop out and rejoin on every plan (groups moving between healers each time).
    for (GroupHealer const& h : healers)
    {
        auto const had = previous.find(h.id);
        bool const in_plan = had != previous.end() && had->second != 0;
        if (h.mana_pct >= (in_plan ? kHealGroupOutManaPct : kHealGroupBackManaPct))
            avail.push_back(h.id);
    }
    if (avail.empty() || groups.empty())
        return result;
    std::sort(avail.begin(), avail.end());
    avail.erase(std::unique(avail.begin(), avail.end()), avail.end());

    std::map<uint8_t, uint64_t> owner;
    std::map<uint64_t, uint32_t> load;
    for (uint64_t h : avail)
        load[h] = 0;

    // Keep what each healer had.
    for (uint8_t g : groups)
        for (uint64_t h : avail)
        {
            auto const it = previous.find(h);
            if (it != previous.end() && HealGroupHas(it->second, g))
            {
                owner[g] = h;
                ++load[h];
                break;
            }
        }

    auto next_floater = [&]() -> uint64_t
    {
        for (uint64_t h : avail)
            if (load[h] == 0)
                return h;
        return 0;
    };

    // Groups nobody covers: a floater first, else the least loaded healer among the groups that follow (the nearest on a tie).
    for (size_t i = 0; i < groups.size(); ++i)
    {
        uint8_t const g = groups[i];
        if (owner.count(g))
            continue;
        uint64_t pick = next_floater();
        if (!pick)
        {
            uint32_t best_load = ~0u;
            for (size_t step = 1; step < groups.size(); ++step)
            {
                auto const it = owner.find(groups[(i + step) % groups.size()]);
                if (it != owner.end() && load[it->second] < best_load)
                {
                    best_load = load[it->second];
                    pick = it->second;
                }
            }
            if (!pick)
                pick = avail.front();   // a single group, or the first one before anyone owns any
        }
        owner[g] = pick;
        ++load[pick];
    }

    // A healer with no group takes one from a healer that covers two or more (the busiest first).
    for (;;)
    {
        uint64_t floater = next_floater();
        if (!floater)
            break;
        uint64_t busiest = 0;
        uint32_t most = 1;
        for (uint64_t h : avail)
            if (load[h] > most)
            {
                most = load[h];
                busiest = h;
            }
        if (!busiest)
            break;
        for (auto it = owner.rbegin(); it != owner.rend(); ++it)
            if (it->second == busiest)
            {
                it->second = floater;
                --load[busiest];
                ++load[floater];
                break;
            }
    }

    for (auto const& [g, h] : owner)
        result[h] = uint16_t(result[h] | (1u << g));
    return result;
}

} // namespace Playerbot::Raid
