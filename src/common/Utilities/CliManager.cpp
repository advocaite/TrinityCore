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

#include "CliManager.h"
#include <cstdio>

namespace
{
    std::recursive_mutex _cliMutex;
    CliManager::Handler* _cliHandler = nullptr;
    uint32 _guardDepth = 0; // only touched while _cliMutex is held
}

void CliManager::SetHandler(Handler* handler)
{
    std::lock_guard<std::recursive_mutex> lock(_cliMutex);
    _cliHandler = handler;
}

std::recursive_mutex& CliManager::GetMutex()
{
    return _cliMutex;
}

CliManager::OutputGuard::OutputGuard() : _lock(_cliMutex)
{
    if (_guardDepth++ == 0 && _cliHandler)
        _cliHandler->Suspend();
}

CliManager::OutputGuard::~OutputGuard()
{
    if (--_guardDepth == 0)
    {
        // everything printed inside the guard has to be on screen before the prompt is drawn again
        fflush(stdout);
        fflush(stderr);
        if (_cliHandler)
            _cliHandler->Resume();
    }
}
