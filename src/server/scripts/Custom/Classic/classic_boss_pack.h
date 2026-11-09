/*
 * Classic raid boss packs: a boss that resets alive (wipe, evade) brings back the adds the raid killed during the failed
 * attempt. Instance.PersistRaidTrash otherwise keeps every killed spawn dead until the lockout resets, and
 * creature_formations is empty in this DB, so nothing links an add to its boss. Packs are plain spawns, found by entry
 * within 80 y of the boss's home position.
 */
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
