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

#ifndef PLAYERBOT_V2_FLEET_WORLD_PVP_CONFIG_H
#define PLAYERBOT_V2_FLEET_WORLD_PVP_CONFIG_H

// Reads the Playerbot.World.Pvp.* lines (Fleet/WorldPvp.h) and the realm's GameType. The rules ask for it every tick, so it is
// read again at most every 15 s (a .reload config reaches it within that).

#include "Fleet/WorldPvp.h"
#include "Config.h"
#include "GameTime.h"
#include "World.h"
#include <algorithm>
#include <atomic>
#include <mutex>

namespace Playerbot::V2::Fleet
{
    inline WorldPvpCfg ReadWorldPvpCfg()
    {
        auto pct = [](char const* key, int32 def) { return uint32_t(std::clamp<int32>(sConfigMgr->GetIntDefault(key, def, true), 0, 100)); };
        WorldPvpCfg c;
        c.enable = sConfigMgr->GetBoolDefault("Playerbot.World.Pvp.Enable", true, true);
        c.flagged_pct = pct("Playerbot.World.Pvp.FlaggedPct", 50);
        c.hunter_pct = pct("Playerbot.World.Pvp.HunterPct", 30);
        c.attack_pct = pct("Playerbot.World.Pvp.AttackPct", 40);
        c.ffa_hunter_pct = pct("Playerbot.World.Pvp.FfaHunterPct", 60);
        c.ffa_attack_pct = pct("Playerbot.World.Pvp.FfaAttackPct", 60);
        c.roll_minutes = uint32_t(std::clamp<int32>(sConfigMgr->GetIntDefault("Playerbot.World.Pvp.RollMinutes", 10, true), 1, 240));
        c.levels_below = uint32_t(std::clamp<int32>(sConfigMgr->GetIntDefault("Playerbot.World.Pvp.LevelsBelow", 5, true), 0, 60));
        c.levels_above = uint32_t(std::clamp<int32>(sConfigMgr->GetIntDefault("Playerbot.World.Pvp.LevelsAbove", 2, true), 0, 60));
        c.attack_bots = sConfigMgr->GetBoolDefault("Playerbot.World.Pvp.AttackBots", true, true);
        c.border_friendly_pct = pct("Playerbot.World.Pvp.BorderFriendlyPct", 85);
        c.contested_friendly_pct = pct("Playerbot.World.Pvp.ContestedFriendlyPct", 70);
        return c;
    }

    struct LiveWorldPvp { WorldPvpCfg cfg; RealmPvp realm = RealmPvp::None; };

    inline LiveWorldPvp CurrentWorldPvp()
    {
        static std::mutex mx;
        static LiveWorldPvp live;
        static uint64_t read_at = 0;
        uint64_t const now = uint64_t(GameTime::GetGameTime());
        std::lock_guard<std::mutex> lock(mx);
        if (!read_at || now >= read_at + 15)
        {
            live.cfg = ReadWorldPvpCfg();
            live.realm = RealmPvpOf(::World::instance()->getIntConfig(CONFIG_GAME_TYPE));
            read_at = now ? now : 1;
        }
        return live;
    }
} // namespace Playerbot::V2::Fleet

#endif
