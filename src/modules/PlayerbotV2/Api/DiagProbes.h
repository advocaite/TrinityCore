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

#ifndef PLAYERBOT_DIAG_PROBES_H
#define PLAYERBOT_DIAG_PROBES_H

#include "Config.h"
#include "Log.h"
#include <atomic>
#include <chrono>

/// Per-packet probes ([whisper_probe], [invite_probe], [inspect_probe], [cmd_probe], [raid_icon], [party_update], [chat_opcode]):
/// log what a client packet carried. They are off unless `Playerbot.Diag.Probes = 1` (a test session; `.reload config` applies it
/// within a few seconds), so a normal run does not write a line per chat message or party update. Cheap enough for the packet loop:
/// the config is read at most once every 5 seconds.
namespace Playerbot::Probes
{
    inline bool Enabled()
    {
        static std::atomic<bool> enabled{ false };
        static std::atomic<int64> nextRead{ 0 };

        int64 const now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if (now >= nextRead.load(std::memory_order_relaxed))
        {
            nextRead.store(now + 5, std::memory_order_relaxed);
            enabled.store(sConfigMgr->GetBoolDefault("Playerbot.Diag.Probes", false, true), std::memory_order_relaxed);
        }
        return enabled.load(std::memory_order_relaxed);
    }
}

#define PROBE_LOG(...) \
    do { if (Playerbot::Probes::Enabled()) TC_LOG_INFO("playerbot.v2", __VA_ARGS__); } while (0)

#endif
