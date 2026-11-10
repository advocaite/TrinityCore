// The town's service NPCs (Travel/TownNpcIndex) turned into gathering spots (TownGather.h).
#pragma once

#include "TownGather.h"
#include "Travel/TownNpcIndex.h"
#include "UnitDefines.h"

namespace Playerbot::V2::Fleet
{

// Spots within `radius` of (x, y) on `map` that a player of the side can use, heaviest first.
inline std::vector<GatherSpot> TownGatherSpots(uint32 map, float x, float y, float radius, bool alliance)
{
    std::vector<GatherNpc> npcs;
    ::Playerbot::V2::Travel::TownNpcIndex::Instance().ForEachNear(map, x, y, radius, [&](::Playerbot::V2::Travel::TownNpc const& n)
    {
        if (!n.usable_by(alliance)) return;
        float w = 0.25f;   // trainer, vendor, repair, petitioner...
        if (n.npc_flags & (UNIT_NPC_FLAG_BANKER | UNIT_NPC_FLAG_AUCTIONEER)) w = 4.0f;
        else if (n.npc_flags & UNIT_NPC_FLAG_INNKEEPER) w = 3.0f;
        else if (n.npc_flags & (UNIT_NPC_FLAG_FLIGHTMASTER | UNIT_NPC_FLAG_BATTLEMASTER)) w = 2.0f;
        else if (n.npc_flags & UNIT_NPC_FLAG_STABLEMASTER) w = 1.0f;
        npcs.push_back({ n.x, n.y, n.z, w });
    });
    return BuildGatherSpots(npcs);
}

// A town of the other side (Crossroads for an Alliance bot, Goldshire for a Horde one): at least two service NPCs within `radius`
// that the other side may use and this side may not. Its guards kill a bot of this side on sight, so no bot of this side is put
// there (pennywhi, Discord 2026-11-10: Alliance world bots ran into the Crossroads and died to the guards). A goblin town is open
// to both and is no hostile town.
inline bool InHostileTown(uint32 map, float x, float y, bool alliance, float radius = 90.0f)
{
    uint32 hostile = 0;
    ::Playerbot::V2::Travel::TownNpcIndex::Instance().ForEachNear(map, x, y, radius, [&](::Playerbot::V2::Travel::TownNpc const& n)
    {
        if (!n.usable_by(alliance) && n.usable_by(!alliance))
            ++hostile;
    });
    return hostile >= 2;
}

} // namespace Playerbot::V2::Fleet
