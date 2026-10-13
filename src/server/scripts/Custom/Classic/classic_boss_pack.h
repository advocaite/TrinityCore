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

// Classic raid boss packs: a boss that resets alive (wipe, evade) brings back the adds the raid killed during the failed
// attempt. creature_formations does not link the adds to their boss, so packs are plain spawns, found by entry within
// 80 y of the boss's home position.

#ifndef CLASSIC_BOSS_PACK_H
#define CLASSIC_BOSS_PACK_H

#include "Creature.h"
#include "CreatureData.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "SpawnData.h"
#include <algorithm>
#include <vector>

inline void ClassicRespawnBossPack(Map* map, uint32 bossEntry, std::vector<uint32> const& adds)
{
    if (!map || adds.empty())
        return;

    Creature* boss = nullptr;
    for (auto const& pair : map->GetCreatureBySpawnIdStore())
        if (pair.second->GetEntry() == bossEntry && pair.second->IsAlive())
        {
            boss = pair.second;
            break;
        }
    if (!boss)
        return;
    Position const home = boss->GetHomePosition();

    std::vector<Creature*> dead;
    for (auto const& pair : map->GetCreatureBySpawnIdStore())
    {
        Creature* c = pair.second;
        if (c->IsAlive() || c->IsSummon() || std::find(adds.begin(), adds.end(), c->GetEntry()) == adds.end())
            continue;
        if (c->GetHomePosition().GetExactDist2d(home) <= 80.0f)
            dead.push_back(c);
    }
    for (Creature* c : dead)
        c->Respawn();

    // spawns whose object is already unloaded wait as respawn entries (non-compatibility mode)
    std::vector<RespawnInfo const*> pending;
    map->GetRespawnInfo(pending, SPAWN_TYPEMASK_CREATURE);
    std::vector<ObjectGuid::LowType> due;
    for (RespawnInfo const* ri : pending)
        if (std::find(adds.begin(), adds.end(), ri->entry) != adds.end())
            if (CreatureData const* data = sObjectMgr->GetCreatureData(ri->spawnId))
                if (Position(data->spawnPoint).GetExactDist2d(home) <= 80.0f)
                    due.push_back(ri->spawnId);
    for (ObjectGuid::LowType spawnId : due)
        map->Respawn(SPAWN_TYPE_CREATURE, spawnId);
}

#endif
