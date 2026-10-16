// WalkOrder - the world spawner's request that a bot walk somewhere on its own legs: arrive from the edge of town to its spot, or leave
// toward an inn / out of sight before it logs out. Written by the spawner on the world thread, read by the bot's idle dispatch; plain
// atomics, no locks. The rule obeys while `active` and the spawner clears it (arrival clears itself).
#pragma once

#include <atomic>
#include <cstdint>

namespace Playerbot::V2
{

struct WalkOrder
{
    enum Kind : std::uint8_t { None = 0, Arrive = 1, Leave = 2 };
    std::atomic<bool> active{false};
    std::atomic<std::uint8_t> kind{None};
    std::atomic<float> x{0.0f}, y{0.0f}, z{0.0f};

    void Set(Kind k, float tx, float ty, float tz)
    {
        x.store(tx); y.store(ty); z.store(tz); kind.store(k);
        active.store(true, std::memory_order_release);
    }
    void Clear() { active.store(false, std::memory_order_release); kind.store(None); }
};

} // namespace Playerbot::V2
