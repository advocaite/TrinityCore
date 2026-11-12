// BossCheck - is this creature a boss, for the bots' boss-kill moment, the fight report and the raid layer.
//
// This core never sets CREATURE_FLAG_EXTRA_DUNGEON_BOSS: CREATURE_FLAG_EXTRA_DB_ALLOWED strips it from the templates at load
// and nothing sets it afterwards, so Creature::IsDungeonBoss() is false for every creature. A dungeon boss with an instance
// script runs a BossAI (the script's boss id, its encounter, its doors), which is the one thing every scripted 5-man and raid
// boss has in common; the world-boss flag and the vanilla "boss" rank (classification Obsolete) cover the unscripted ones.
// World thread (reads the creature's AI).

#pragma once

#include "Creature.h"
#include "Map.h"
#include "ScriptedCreature.h"

namespace Playerbot {

inline bool IsBossCreature(Creature const* c)
{
    if (!c)
        return false;
    if (c->IsDungeonBoss() || c->isWorldBoss() || c->GetCreatureClassification() == CreatureClassifications::Obsolete)
        return true;
    return c->GetMap() && c->GetMap()->IsDungeon() && dynamic_cast<BossAI const*>(c->AI()) != nullptr;
}

} // namespace Playerbot
