/* This file is part of TrinityCore; licensed under GPL version 2 or later. */
#ifndef TRINITY_INSTANCE_RESET_SCHEDULE_H
#define TRINITY_INSTANCE_RESET_SCHEDULE_H

#include "DBCEnums.h"
#include <chrono>
#include <ctime>
#include <optional>

namespace InstanceResetSchedule
{
// Fixed Classic cycles share a civil-calendar anchor (1970-01-01), so all
// players receive the same boundary and a restart cannot move the schedule.
// Twice-weekly resets use the configured weekly day and that day + 4,
// for an alternating three/four-day server schedule. The fixed-cycle phase
// is a deterministic server policy, not a claim about an official reset epoch.
inline std::optional<std::time_t> GetResetTime(uint8 interval, std::tm dateTime,
    int32 resetHour, int32 resetDay, bool next = true)
{
    using namespace std::chrono;
    if (interval == MAP_DIFFICULTY_RESET_ANYTIME || interval > MAP_DIFFICULTY_RESET_TWICE_WEEKLY)
        return std::nullopt;

    std::time_t now = std::mktime(&dateTime);
    sys_days civilDate = year{ dateTime.tm_year + 1900 } / month{ unsigned(dateTime.tm_mon + 1) } / day{ unsigned(dateTime.tm_mday) };
    for (int32 distance = 0; distance <= 7; ++distance)
    {
        int32 offset = next ? distance : -distance;
        sys_days date = civilDate + days{ offset };
        int32 weekday = std::chrono::weekday{ date }.c_encoding();
        int64 dayNumber = date.time_since_epoch().count();
        bool scheduled = interval == MAP_DIFFICULTY_RESET_DAILY
            || (interval == MAP_DIFFICULTY_RESET_WEEKLY && weekday == resetDay)
            || (interval == MAP_DIFFICULTY_RESET_THREE_DAYS && dayNumber % 3 == 0)
            || (interval == MAP_DIFFICULTY_RESET_FIVE_DAYS && dayNumber % 5 == 0)
            || (interval == MAP_DIFFICULTY_RESET_TWICE_WEEKLY && (weekday == resetDay || weekday == (resetDay + 4) % 7));
        if (!scheduled)
            continue;

        std::tm candidate = dateTime;
        candidate.tm_mday += offset;
        candidate.tm_hour = resetHour;
        candidate.tm_min = candidate.tm_sec = 0;
        candidate.tm_isdst = -1; // Recompute DST for the candidate date.
        std::time_t reset = std::mktime(&candidate);
        if ((next && reset > now) || (!next && reset < now))
            return reset;
    }
    return std::nullopt;
}
}
#endif
