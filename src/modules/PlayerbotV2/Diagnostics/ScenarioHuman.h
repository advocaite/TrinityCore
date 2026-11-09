// ScenarioHuman - the scenario runner's stand-in for a player.
//
// A player-led scenario (ScenarioDef::player_lead) detaches one squad member's bot logic, makes it the group leader and plays it like the
// owner: it walks the dungeon, says "pull" and "attack" in the group, and fights the tank's target. For the rest of the squad to treat it as
// a player (the dungeon autopilot stands down for a human leader, the group fight follows him, his orders are obeyed), the checks that ask
// "is this member a person or a bot" read PlaysAsHuman / IsScenarioHuman instead of the session's bot flag alone.
//
// A handful of slots, written by the runner on the world thread and read by the snapshot builders on any thread.

#pragma once

#include "Player.h"
#include "WorldSession.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace Playerbot::Scenario {

inline std::array<std::atomic<std::uint64_t>, 8>& HumanSlots()
{
    static std::array<std::atomic<std::uint64_t>, 8> slots{};
    return slots;
}

// Marks (or unmarks) a character as a scenario's player. Returns false when every slot is taken.
inline bool MarkHuman(std::uint64_t guid_low, bool on)
{
    for (auto& s : HumanSlots())
    {
        std::uint64_t cur = s.load(std::memory_order_relaxed);
        if (on && cur == guid_low)
            return true;
        if (!on && cur == guid_low)
        {
            s.store(0, std::memory_order_relaxed);
            return true;
        }
    }
    if (!on)
        return true;
    for (auto& s : HumanSlots())
    {
        std::uint64_t expected = 0;
        if (s.compare_exchange_strong(expected, guid_low, std::memory_order_relaxed))
            return true;
    }
    return false;
}

inline bool IsScenarioHuman(std::uint64_t guid_low)
{
    if (guid_low == 0)
        return false;
    for (auto const& s : HumanSlots())
        if (s.load(std::memory_order_relaxed) == guid_low)
            return true;
    return false;
}

// A person at the keyboard, or the scenario's stand-in for one.
inline bool PlaysAsHuman(Player const* p)
{
    if (!p)
        return false;
    if (!(p->GetSession() && p->GetSession()->IsBot()))
        return true;
    return IsScenarioHuman(p->GetGUID().GetCounter());
}

// A squad told "yes fear" (ScenarioDef::yes_fear): its members may cast Fear / Scare Beast in a dungeon, as an owner's bots may after that
// order. A scenario squad has no owner account, so the snapshot builder's owner default (Fear off, 0022) would hold it otherwise.
inline std::array<std::atomic<std::uint64_t>, 8>& YesFearSlots()
{
    static std::array<std::atomic<std::uint64_t>, 8> slots{};
    return slots;
}

inline void MarkYesFear(std::uint64_t guid_low, bool on)
{
    for (auto& s : YesFearSlots())
    {
        std::uint64_t cur = s.load(std::memory_order_relaxed);
        if (cur == guid_low)
        {
            if (!on)
                s.store(0, std::memory_order_relaxed);
            return;
        }
    }
    if (!on)
        return;
    for (auto& s : YesFearSlots())
    {
        std::uint64_t expected = 0;
        if (s.compare_exchange_strong(expected, guid_low, std::memory_order_relaxed))
            return;
    }
}

inline bool HasYesFear(std::uint64_t guid_low)
{
    if (guid_low == 0)
        return false;
    for (auto const& s : YesFearSlots())
        if (s.load(std::memory_order_relaxed) == guid_low)
            return true;
    return false;
}

} // namespace Playerbot::Scenario
