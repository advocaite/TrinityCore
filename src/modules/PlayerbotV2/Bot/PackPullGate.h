// PackPullGate - the dungeon pack stop's memory of which idle packs the tank can only walk to the long way round
// (State_Idle: idle:dungeon_tank_pull_spot / pull_from_range).
// The pack stop holds the advance for every unpulled mob whose aggro reach lies within one step, in any direction, and
// line of sight is enough: a pack on the corridor 15 yards above the tank, 160 yards of walk away, held it as well as a
// pack in front of it. Such a pack is no pull for this tank from here (its pull spot is across the gap or on the lava,
// its opener a chase along the whole walk), so the stop passes it by: the route reaches it from its own side later. The
// verdict is kept per pack, so several of them are all passed by. A single remembered mob was not enough (RC1
// rfc_oggleflint: the tank stood at the tunnel below two such packs, each verdict overwriting the other, for good).
// Tested in tests/game/ClassicPackPullGate.cpp.

#pragma once

#include <cstdint>
#include <unordered_map>

namespace Playerbot {

class PackPullGate
{
public:
    static constexpr std::uint32_t kGatedMs = 15000;   // a long walk is re-measured after this (the tank has moved on)
    static constexpr std::uint32_t kWalkableMs = 5000; // a short walk is re-measured after this

    enum class Verdict : std::uint8_t { Unknown, Walkable, Gated };

    Verdict Get(std::uint64_t pack, std::uint32_t now_ms) const
    {
        auto const it = entries_.find(pack);
        if (it == entries_.end() || std::int32_t(now_ms - it->second.until_ms) >= 0)
            return Verdict::Unknown;
        return it->second.gated ? Verdict::Gated : Verdict::Walkable;
    }

    void Note(std::uint64_t pack, bool gated, std::uint32_t now_ms)
    {
        for (auto it = entries_.begin(); it != entries_.end();)
        {
            if (std::int32_t(now_ms - it->second.until_ms) >= 0) it = entries_.erase(it);
            else ++it;
        }
        entries_[pack] = Entry{ now_ms + (gated ? kGatedMs : kWalkableMs), gated };
    }

    void Clear() { entries_.clear(); }

private:
    struct Entry { std::uint32_t until_ms; bool gated; };
    std::unordered_map<std::uint64_t, Entry> entries_;
};

} // namespace Playerbot
