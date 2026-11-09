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

// Classic 1.60 port of the VMaNGOS Dragons of Nightmare world event (src/game/HardcodedEvents.cpp DragonsOfNightmare, GPL-2).
// VMaNGOS: while any of the four dragons lives, a dead one does not come back; once the last one dies, all four return together
// 4-7 days later. VMaNGOS does it by switching game event 66 off and on; here event 66 is always on
// (2026_10_25_24_world_classic_dragons_of_nightmare_event.sql) and the round timer is the spawns' respawn time: the spawns carry a
// respawn delay of 999999999 s, so a killed dragon waits, and when all four are dead this script gives them one shared respawn time.
// Respawn times live in the characters database, so a restart keeps the round.
// Not ported: VMaNGOS also swaps which dragon stands at which of the four portals each round (an entry change on spawn).

#include "ScriptMgr.h"
#include "Creature.h"
#include "GameTime.h"
#include "GridDefines.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "Random.h"
#include "Util.h"
#include <array>
#include <vector>

namespace
{
    std::array<uint32, 4> constexpr NightmareDragons = { 14887, 14888, 14889, 14890 }; // Ysondre, Lethon, Emeriss, Taerar

    constexpr uint32 CheckInterval = 60 * IN_MILLISECONDS;
    constexpr time_t RoundDelayMin = 4 * DAY;
    constexpr time_t RoundDelayMax = 7 * DAY;

    struct NightmareDragon
    {
        uint32 Entry = 0;
        ObjectGuid::LowType SpawnId = 0;
        uint32 MapId = 0;
        Position SpawnPoint;
    };
}

class classic_world_event_dragons_of_nightmare : public WorldScript
{
public:
    classic_world_event_dragons_of_nightmare() : WorldScript("classic_world_event_dragons_of_nightmare") { }

    void OnStartup() override
    {
        _dragons.clear();
        for (uint32 entry : NightmareDragons)
        {
            for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
            {
                if (data.id != entry)
                    continue;

                NightmareDragon& dragon = _dragons.emplace_back();
                dragon.Entry = entry;
                dragon.SpawnId = spawnId;
                dragon.MapId = data.mapId;
                dragon.SpawnPoint = data.spawnPoint;
                break;
            }
        }

        if (_dragons.size() != NightmareDragons.size())
            TC_LOG_ERROR("scripts", "Dragons of Nightmare: only {} of the 4 dragons have a spawn, the round timer is off", _dragons.size());
    }

    // before the maps update, so no map thread is reading the respawn data while it changes
    void OnBeforeMapsUpdate(uint32 diff) override
    {
        _timer += diff;
        if (_timer < CheckInterval)
            return;
        _timer = 0;

        if (_dragons.size() != NightmareDragons.size())
            return;

        time_t const now = GameTime::GetGameTime();
        bool needsRound = false;
        for (NightmareDragon const& dragon : _dragons)
        {
            // dead = a corpse in the world or a respawn time still to come; anything else (alive, map or grid not loaded) waits
            bool dead = false;
            sMapMgr->DoForAllMapsWithMapId(dragon.MapId, [&](Map* map)
            {
                auto bounds = map->GetCreatureBySpawnIdStore().equal_range(dragon.SpawnId);
                bool inWorld = bounds.first != bounds.second;
                for (auto itr = bounds.first; itr != bounds.second; ++itr)
                    if (itr->second->IsAlive())
                        return;

                time_t respawn = map->GetCreatureRespawnTime(dragon.SpawnId);
                if (respawn > now || inWorld)
                {
                    dead = true;
                    // still on the death's own delay (or none saved yet): the round time is not set
                    if (respawn <= now || respawn > now + RoundDelayMax)
                        needsRound = true;
                }
            });

            if (!dead)
                return;
        }

        // all four dead, and the shared respawn time is already set
        if (!needsRound)
            return;

        time_t const roundRespawn = now + time_t(urand(uint32(RoundDelayMin), uint32(RoundDelayMax)));
        for (NightmareDragon const& dragon : _dragons)
        {
            uint32 gridId = Trinity::ComputeGridCoord(dragon.SpawnPoint.GetPositionX(), dragon.SpawnPoint.GetPositionY()).GetId();
            sMapMgr->DoForAllMapsWithMapId(dragon.MapId, [&](Map* map)
            {
                // an earlier respawn time replaces the death's 999999999 s one, in memory and in the characters database
                map->SaveRespawnTime(SPAWN_TYPE_CREATURE, dragon.SpawnId, dragon.Entry, roundRespawn, gridId);
            });
        }

        TC_LOG_INFO("scripts", "Dragons of Nightmare: all four are dead, they return at {}", TimeToTimestampStr(roundRespawn));
    }

private:
    std::vector<NightmareDragon> _dragons;
    uint32 _timer = 0;
};

void AddSC_classic_world_event_dragons_of_nightmare()
{
    new classic_world_event_dragons_of_nightmare();
}
