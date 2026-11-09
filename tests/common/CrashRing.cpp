/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tc_catch2.h"
#include "AppenderCrashRing.h"
#include "LogMessage.h"
#include <string>

namespace
{
void Write(AppenderCrashRing& ring, std::string text)
{
    LogMessage message(LOG_LEVEL_INFO, "test", std::move(text));
    ring.write(&message);
}
}

// The crash report prints these lines: they must come out oldest first, only the last N, and cut (not overflow) when too long.
TEST_CASE("crash ring keeps the last lines in order", "[CrashRing]")
{
    std::vector<std::string_view> const args = { "", "", "", "3" };
    AppenderCrashRing ring(0, "CrashRing", LOG_LEVEL_INFO, APPENDER_FLAGS_NONE, args);

    for (int i = 1; i <= 5; ++i)
        Write(ring, "line " + std::to_string(i));

    char buffer[256];
    size_t const length = AppenderCrashRing::Snapshot(buffer, sizeof(buffer));
    CHECK(std::string(buffer, length) == "line 3\nline 4\nline 5\n");
    CHECK(buffer[length] == '\0');
}

TEST_CASE("crash ring snapshot respects the buffer size and long lines", "[CrashRing]")
{
    std::vector<std::string_view> const args = { "", "", "", "3" };
    AppenderCrashRing ring(0, "CrashRing", LOG_LEVEL_INFO, APPENDER_FLAGS_NONE, args);

    Write(ring, "first");
    Write(ring, std::string(2000, 'x'));        // longer than a slot
    Write(ring, "last");

    char buffer[64];
    size_t const length = AppenderCrashRing::Snapshot(buffer, sizeof(buffer));
    CHECK(length < sizeof(buffer));
    CHECK(buffer[length] == '\0');

    char big[4096];
    size_t const bigLength = AppenderCrashRing::Snapshot(big, sizeof(big));
    std::string const all(big, bigLength);
    CHECK(all.ends_with("last\n"));
    CHECK(all.find(std::string(AppenderCrashRing::LineChars, 'x')) == std::string::npos);   // cut below the slot size, with the NUL

    CHECK(AppenderCrashRing::Snapshot(nullptr, 10) == 0);
    CHECK(AppenderCrashRing::Snapshot(buffer, 0) == 0);
}

TEST_CASE("crash ring rejects a bad line count", "[CrashRing]")
{
    CHECK_THROWS_AS(AppenderCrashRing(0, "CrashRing", LOG_LEVEL_INFO, APPENDER_FLAGS_NONE, { "", "", "", "0" }), InvalidAppenderArgsException);
    CHECK_THROWS_AS(AppenderCrashRing(0, "CrashRing", LOG_LEVEL_INFO, APPENDER_FLAGS_NONE, { "", "", "", "1025" }), InvalidAppenderArgsException);
}
