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

#ifndef APPENDERCRASHRING_H
#define APPENDERCRASHRING_H

#include "Appender.h"
#include <vector>

/// Keeps the last log lines in a fixed block of memory so that a crash report can print what the server was doing right before it
/// died (no file to flush, no allocation at crash time). Config: `Appender.CrashRing=4,3,7` (type 4, info and above, with timestamp,
/// level and filter prefix), optional1 = number of lines kept (default 200, at most 1024); link it to the loggers whose lines should
/// appear (`Logger.root=5,Console Server CrashRing`).
class TC_COMMON_API AppenderCrashRing : public Appender
{
    public:
        static constexpr AppenderType type = APPENDER_CRASH_RING;

        static constexpr uint32 MaxLines = 1024;
        static constexpr uint32 LineChars = 480;

        AppenderCrashRing(uint8 id, std::string name, LogLevel level, AppenderFlags flags, std::vector<std::string_view> const& args);
        AppenderType getType() const override { return type; }

        /// Copies the newest kept lines that fit into `out` (at most `capacity` bytes, NUL terminated), oldest of them first, one per
        /// '\n'. Only reads fixed memory, so it is safe to call from the crash handler. Returns the number of bytes written (without the NUL).
        static size_t Snapshot(char* out, size_t capacity);

    private:
        void _write(LogMessage const* message) override;
};

#endif
