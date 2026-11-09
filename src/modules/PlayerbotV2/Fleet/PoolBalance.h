// PoolBalance - which character the roster seeder creates next so the pool of sleeping residents is balanced between the factions
// in every level bracket (playtest, 2026-10-11: the pool was ~94% Alliance, so no Horde was around when he quested and the Horde
// battleground queues starved). The seeder used to flip a coin per bot; now it creates whichever (faction, bracket) is furthest
// below the target ratio. 2026-10-18: the roster is balanced at every level, 1-9 included, and each level bracket has a cap of its own,
// so a faction one player has used for hours cannot pull the roster toward it and the 60s cannot crowd out the low brackets. Pure: no game types, tested in tests/game/ClassicPoolBalance.cpp.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace Playerbot::V2::Fleet {

// 2026-11-02: the bands are the battleground brackets (10-19 .. 50-59, then 60 alone), so every bracket has its own share of the roster.
// With 50-60 as one band only about one bot in eleven there was level 60, so the level 60 queues had few spare bots of their own.
constexpr std::size_t kPoolBrackets = 7;   // 1-9, 10-19, 20-29, 30-39, 40-49, 50-59, 60

inline std::size_t PoolBracketOf(std::uint32_t level)
{
    if (level >= 60u)
        return kPoolBrackets - 1u;
    return std::clamp<std::uint32_t>(level / 10u, 0u, kPoolBrackets - 2u);
}

inline std::uint32_t PoolBracketLo(std::size_t b) { return b == 0 ? 1u : b + 1 == kPoolBrackets ? 60u : std::uint32_t(b) * 10u; }
inline std::uint32_t PoolBracketHi(std::size_t b) { return b + 1 == kPoolBrackets ? 60u : std::uint32_t(b) * 10u + 9u; }

// The cap of one level bracket: the standing roster split evenly over the brackets (rounded up).
inline std::uint32_t PoolBracketCap(std::uint32_t pool_max)
{
    return (pool_max + std::uint32_t(kPoolBrackets) - 1u) / std::uint32_t(kPoolBrackets);
}

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

// How many characters one faction may hold in a bracket: its share of the bracket cap (horde_pct 5..95).
inline std::int64_t PoolTarget(std::uint32_t bracket_cap, std::uint32_t horde_pct, bool alliance)
{
    double const p = std::clamp<std::uint32_t>(horde_pct, 5u, 95u) / 100.0;
    return std::int64_t(std::ceil(double(bracket_cap) * (alliance ? 1.0 - p : p)));
}

// Does the standing roster have room for one more character of this faction at this level? A full (faction, bracket) sends the creation
// to the temporary pool, so the roster stays balanced however much one faction is played.
inline bool PoolHasRoom(PoolCounts const& c, std::uint32_t level, bool alliance, std::uint32_t bracket_cap, std::uint32_t horde_pct)
{
    std::size_t const b = PoolBracketOf(level);
    return std::int64_t((alliance ? c.alliance : c.horde)[b]) < PoolTarget(bracket_cap, horde_pct, alliance);
}

// bracket_cap > 0: every (faction, bracket) has a target of its share of the cap, and the shortfall is target - count, so the short side
// of the short bracket is filled first and a full one is never grown. bracket_cap == 0: the old rule, the ratio between the factions.
// The pick is random in proportion to the shortfalls (roll in [0,1)), so every bracket that lacks characters gets some at once, in step
// with how much it lacks: always taking the single largest shortfall filled one bracket first and left the low brackets without Horde
// for the first hundred creations. deficit in the result is the chosen entry's shortfall; when nothing lacks, it is the largest (<= 0)
// and the caller creates nothing (cap) or falls back to its own choice (ratio).
inline SeedChoice PickSeed(PoolCounts const& c, std::uint32_t horde_pct, double roll, std::uint32_t bracket_cap = 0)
{
    double const p = std::clamp<std::uint32_t>(horde_pct, 5u, 95u) / 100.0;
    struct Entry { bool alliance; std::size_t bracket; std::int64_t deficit; };
    std::array<Entry, kPoolBrackets * 2> e{};
    std::int64_t total = 0;
    Entry largest{ false, 0, std::numeric_limits<std::int64_t>::min() };
    for (std::size_t b = 0; b < kPoolBrackets; ++b)
    {
        if (bracket_cap)
        {
            e[b * 2]     = { false, b, PoolTarget(bracket_cap, horde_pct, false) - std::int64_t(c.horde[b]) };
            e[b * 2 + 1] = { true,  b, PoolTarget(bracket_cap, horde_pct, true) - std::int64_t(c.alliance[b]) };
            continue;
        }
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
