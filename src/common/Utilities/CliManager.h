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

#ifndef TRINITY_CLIMANAGER_H
#define TRINITY_CLIMANAGER_H

#include "Define.h"
#include <mutex>

/**
 * Coordinates everything that writes to the console while the interactive prompt ("TC> <typed text>")
 * is visible. Any code that prints to stdout/stderr (log lines, command output) does so inside an
 * OutputGuard. The guard asks the prompt owner (the console input in CliRunnable.cpp) to take the prompt
 * and the half-typed command off the screen first, and to draw them again, on a fresh line below the
 * output, once the output is done.
 *
 * Rules for code running inside a guard:
 *  - the output must end with a newline
 *  - guards nest, only the outermost one suspends/resumes the prompt
 *
 * This lives in common (and not in CliRunnable.cpp) because the console log appender is part of common.
 */
class TC_COMMON_API CliManager
{
public:
    class TC_COMMON_API Handler
    {
    public:
        virtual ~Handler() = default;

        /// Remove "<prompt><typed text>" from the screen and leave the cursor at the start of the row the prompt was on
        virtual void Suspend() = 0;

        /// Redraw "<prompt><typed text>" at the cursor (which is at the start of an empty row) and restore the cursor position inside the text
        virtual void Resume() = 0;
    };

    class TC_COMMON_API OutputGuard
    {
    public:
        OutputGuard();
        ~OutputGuard();

        OutputGuard(OutputGuard const&) = delete;
        OutputGuard& operator=(OutputGuard const&) = delete;

    private:
        std::unique_lock<std::recursive_mutex> _lock;
    };

    /// Registers the object that owns the prompt, nullptr unregisters. Must be unregistered before the handler is destroyed
    static void SetHandler(Handler* handler);

    /// The lock that protects the prompt state. Prompt owners must hold it while they modify what is on screen
    static std::recursive_mutex& GetMutex();
};

#endif // TRINITY_CLIMANAGER_H
