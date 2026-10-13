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

#include "AppenderCrashRing.h"
#include "LogMessage.h"
#include "StringConvert.h"
#include <algorithm>
#include <atomic>
#include <cstring>

namespace
{
// Fixed storage: the crash handler reads it without taking a lock or allocating. Appenders can be called from several threads at once
// (synchronous logging), so each writer reserves its own slot with one atomic increment; a line being written while the process dies,
// or while the ring wraps onto it, may come out cut, which is acceptable for a crash report.
char s_lines[AppenderCrashRing::MaxLines][AppenderCrashRing::LineChars];
std::atomic<uint64> s_written{ 0 };         // total lines ever written
std::atomic<uint32> s_capacity{ 200 };      // lines kept (configurable, at most MaxLines)
}

AppenderCrashRing::AppenderCrashRing(uint8 id, std::string name, LogLevel level, AppenderFlags flags, std::vector<std::string_view> const& args) :
    Appender(id, std::move(name), level, flags)
{
    if (args.size() > 3)
    {
        Optional<uint32> lines = Trinity::StringTo<uint32>(args[3]);
        if (!lines || *lines == 0 || *lines > MaxLines)
            throw InvalidAppenderArgsException("Log::CreateAppenderFromConfig: crash ring appender needs a line count between 1 and 1024");
        s_capacity = *lines;
    }
}

void AppenderCrashRing::_write(LogMessage const* message)
{
    uint64 const index = s_written.fetch_add(1, std::memory_order_acq_rel);
    char* slot = s_lines[index % s_capacity.load(std::memory_order_relaxed)];

    size_t length = std::min<size_t>(message->prefix.size(), LineChars - 1);
    std::memcpy(slot, message->prefix.data(), length);
    size_t const textLength = std::min<size_t>(message->text.size(), LineChars - 1 - length);
    std::memcpy(slot + length, message->text.data(), textLength);
    length += textLength;
    slot[length] = '\0';
}

size_t AppenderCrashRing::Snapshot(char* out, size_t capacity)
{
    if (!out || !capacity)
        return 0;

    uint64 const written = s_written.load(std::memory_order_acquire);
    uint32 const kept = s_capacity.load(std::memory_order_relaxed);
    uint64 const first = written > kept ? written - kept : 0;

    // the newest lines matter most: walk back from the newest to find how many fit (each line + '\n', plus the final NUL)
    uint64 start = written;
    size_t needed = 1;
    while (start > first)
    {
        size_t const length = strnlen(s_lines[(start - 1) % kept], LineChars);
        if (needed + length + 1 > capacity)
            break;
        needed += length + 1;
        --start;
    }

    size_t used = 0;
    for (uint64 i = start; i < written; ++i)
    {
        char const* line = s_lines[i % kept];
        size_t const length = strnlen(line, LineChars);
        if (used + length + 2 > capacity)
            break;
        std::memcpy(out + used, line, length);
        used += length;
        out[used++] = '\n';
    }
    out[used] = '\0';
    return used;
}
