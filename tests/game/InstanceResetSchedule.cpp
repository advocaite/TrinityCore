#include "tc_catch2.h"
#include "InstanceResetSchedule.h"
#include "DB2Structure.h"

namespace
{
std::tm LocalDate(int year, int month, int day, int hour)
{
    std::tm date{};
    date.tm_year = year - 1900;
    date.tm_mon = month - 1;
    date.tm_mday = day;
    date.tm_hour = hour;
    date.tm_isdst = -1;
    std::mktime(&date);
    return date;
}
std::time_t At(int year, int month, int day, int hour)
{
    auto date = LocalDate(year, month, day, hour);
    return std::mktime(&date);
}
}

TEST_CASE("Classic raid reset boundaries remain in the future", "[InstanceResetSchedule]")
{
    for (uint8 interval = 1; interval <= 5; ++interval)
        for (int day = 1; day <= 31; ++day)
            for (int hour : { 8, 9, 10 })
            {
                auto date = LocalDate(2026, 10, day, hour);
                auto reset = InstanceResetSchedule::GetResetTime(interval, date, 9, 3);
                auto previous = InstanceResetSchedule::GetResetTime(interval, date, 9, 3, false);
                REQUIRE(reset.has_value());
                REQUIRE(previous.has_value());
                REQUIRE(*reset > std::mktime(&date));
                REQUIRE(*previous < std::mktime(&date));
            }
    REQUIRE_FALSE(InstanceResetSchedule::GetResetTime(0, LocalDate(2026, 10, 1, 10), 9, 3));
    REQUIRE_FALSE(InstanceResetSchedule::GetResetTime(6, LocalDate(2026, 10, 1, 10), 9, 3));
}

TEST_CASE("Classic twice-weekly extensions follow Wednesday and Sunday", "[InstanceResetSchedule]")
{
    REQUIRE(InstanceResetSchedule::GetResetTime(5, LocalDate(2026, 9, 30, 8), 9, 3) == At(2026, 9, 30, 9));
    REQUIRE(InstanceResetSchedule::GetResetTime(5, LocalDate(2026, 9, 30, 9), 9, 3) == At(2026, 10, 4, 9));
    REQUIRE(InstanceResetSchedule::GetResetTime(5, LocalDate(2026, 10, 4, 9), 9, 3) == At(2026, 10, 7, 9));
    REQUIRE(InstanceResetSchedule::GetResetTime(5, LocalDate(2026, 10, 4, 9), 9, 3, false) == At(2026, 9, 30, 9));
}

TEST_CASE("Classic daily weekly and fixed cycles cross months and DST", "[InstanceResetSchedule]")
{
    auto date = LocalDate(2026, 10, 31, 10);
    REQUIRE(InstanceResetSchedule::GetResetTime(1, date, 9, 3) == At(2026, 11, 1, 9));
    REQUIRE(InstanceResetSchedule::GetResetTime(2, date, 9, 3) == At(2026, 11, 4, 9));
    for (uint8 interval : { uint8(3), uint8(4) })
    {
        auto reset = InstanceResetSchedule::GetResetTime(interval, date, 9, 3);
        REQUIRE(reset.has_value());
        auto boundary = *std::localtime(&*reset);
        auto next = InstanceResetSchedule::GetResetTime(interval, boundary, 9, 3);
        auto target = boundary;
        target.tm_mday += interval == 3 ? 3 : 5;
        target.tm_isdst = -1;
        REQUIRE(next == std::mktime(&target));
    }
    REQUIRE(InstanceResetSchedule::GetResetTime(1, LocalDate(2026, 10, 24, 10), 9, 3) == At(2026, 10, 25, 9));
}

TEST_CASE("Fixed raid periods report their duration while alternating periods are calendar based", "[InstanceResetSchedule]")
{
    MapDifficultyEntry difficulty{};
    difficulty.ResetInterval = MAP_DIFFICULTY_RESET_THREE_DAYS;
    CHECK(difficulty.GetRaidDuration() == 3 * 86400);
    difficulty.ResetInterval = MAP_DIFFICULTY_RESET_FIVE_DAYS;
    CHECK(difficulty.GetRaidDuration() == 5 * 86400);
    difficulty.ResetInterval = MAP_DIFFICULTY_RESET_TWICE_WEEKLY;
    CHECK(difficulty.GetRaidDuration() == 0);
}
