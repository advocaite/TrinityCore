// DayRhythm - how busy the world is at this hour of the server machine's day. The bubble target (ZoneBots / CityBots) is scaled by
// it, so the night is quieter (and costs less RAM and CPU) and the evening is busier, the way a real server is. Pure: tested in
// tests/game/ClassicDayRhythm.cpp.
#pragma once

#include <cstdint>

namespace Playerbot::V2::Fleet
{

// Percent of the day target for the local hour 0..23.
inline std::uint32_t RhythmPercent(int hour)
{
    if (hour < 0 || hour > 23) return 100;
    if (hour < 6)  return 60;    // night
    if (hour < 9)  return 80;    // early morning
    if (hour < 18) return 100;   // day
    if (hour < 22) return 125;   // evening, the busiest
    return 80;                   // late evening
}

// `target` scaled by the rhythm, never above `ceiling` (the world cap), and never below 1 for a nonzero target.
inline std::uint32_t RhythmTarget(std::uint32_t target, int hour, std::uint32_t ceiling)
{
    if (!target) return 0;
    std::uint64_t const v = std::uint64_t(target) * RhythmPercent(hour) / 100u;
    std::uint32_t const out = v > ceiling ? ceiling : std::uint32_t(v);
    return out ? out : 1u;
}

} // namespace Playerbot::V2::Fleet
