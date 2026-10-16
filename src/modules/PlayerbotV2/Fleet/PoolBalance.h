// PoolBalance - which character the roster seeder creates next so the pool of sleeping residents is balanced between the factions
// in every level bracket (playtest, 2026-10-11: the pool was ~94% Alliance, so no Horde was around when he quested and the Horde
// battleground queues starved). The seeder used to flip a coin per bot; now it creates whichever (faction, bracket) is furthest
// below the target ratio. Pure: no game types, tested in tests/game/ClassicPoolBalance.cpp.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace Playerbot::V2::Fleet {

constexpr std::size_t kPoolBrackets = 5;   // 10-19, 20-29, 30-39, 40-49, 50-60

inline std::size_t PoolBracketOf(std::uint32_t level)
{
    return std::clamp<std::uint32_t>(level / 10u, 1u, kPoolBrackets) - 1u;
}

inline std::uint32_t PoolBracketLo(std::size_t b) { return std::uint32_t(b + 1) * 10u; }
inline std::uint32_t PoolBracketHi(std::size_t b) { return b + 1 == kPoolBrackets ? 60u : std::uint32_t(b + 1) * 10u + 9u; }

struct PoolCounts
{
    std::array<std::uint32_t, kPoolBrackets> alliance{};
    std::array<std::uint32_t, kPoolBrackets> horde{};
};

struct SeedChoice
{
    bool alliance = false;
    std::size_t bracket = 0;
    std::int64_t deficit = 0;   // characters this (faction, bracket) lacks to reach the ratio; <= 0 = nothing is lacking
};

// horde_pct: share of the pool that should be Horde, 5..95. For Horde H' = A * p / (1 - p) is the count that would put a bracket at the
// ratio, so it lacks H' - H; likewise for Alliance. The pick is random in proportion to the shortfalls (roll in [0,1)), so every bracket
// that lacks characters gets some at once, in step with how much it lacks: always taking the single largest shortfall filled one bracket
// first and left the low brackets without Horde for the first hundred creations. deficit in the result is the chosen entry's shortfall;
// when nothing lacks, it is the largest (<= 0) and the caller falls back to its own choice.
inline SeedChoice PickSeed(PoolCounts const& c, std::uint32_t horde_pct, double roll)
{
    double const p = std::clamp<std::uint32_t>(horde_pct, 5u, 95u) / 100.0;
    struct Entry { bool alliance; std::size_t bracket; std::int64_t deficit; };
    std::array<Entry, kPoolBrackets * 2> e{};
    std::int64_t total = 0;
    Entry largest{ false, 0, std::numeric_limits<std::int64_t>::min() };
    for (std::size_t b = 0; b < kPoolBrackets; ++b)
    {
        e[b * 2]     = { false, b, std::int64_t(std::ceil(double(c.alliance[b]) * p / (1.0 - p))) - std::int64_t(c.horde[b]) };
        e[b * 2 + 1] = { true,  b, std::int64_t(std::ceil(double(c.horde[b]) * (1.0 - p) / p)) - std::int64_t(c.alliance[b]) };
    }
    for (Entry const& x : e)
    {
        if (x.deficit > largest.deficit) largest = x;
        if (x.deficit > 0) total += x.deficit;
    }
    if (total <= 0)
        return { largest.alliance, largest.bracket, largest.deficit };
    std::int64_t pick = std::int64_t(std::clamp(roll, 0.0, 0.999999) * double(total));
    for (Entry const& x : e)
    {
        if (x.deficit <= 0) continue;
        if (pick < x.deficit) return { x.alliance, x.bracket, x.deficit };
        pick -= x.deficit;
    }
    return { largest.alliance, largest.bracket, largest.deficit };
}

inline void PoolAdd(PoolCounts& c, bool alliance, std::uint32_t level)
{
    ++(alliance ? c.alliance : c.horde)[PoolBracketOf(level)];
}

} // namespace Playerbot::V2::Fleet
