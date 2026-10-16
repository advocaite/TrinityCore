// InstanceGroupCap.h: how many companions an owner may have out. Open world: Playerbot.Behavior.OpenWorldGroupCap (5)
// owned companions at once; more only by inviting world bots, which are not counted. Inside an instance: what the map
// holds, and the group never grows past it.
// Playerbot.Behavior.InstanceGroupCap (default off). Header-only so the `.bot` command table and the
// fleet share one rule. An instance uses the map's own data (MapDifficulty MaxPlayers, then the Map
// entry), falling back to 5 for a dungeon and 40 for a raid. Outside an instance (open world) the cap is
// Playerbot.Behavior.OpenWorldGroupCap (default 5, a normal party): raid bots are called inside the raid.
// Battleground and arena maps are not capped here (the battleground has its own limits).
#pragma once

#include "DB2Structure.h"
#include "Group.h"
#include "Map.h"
#include "Player.h"
#include "WorldSession.h"
#include "../Services.h"
#include "ObjectAccessor.h"
#include "OwnerRegistry.h"
#include "../Util/ConfigReader.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Playerbot::V2::Fleet
{

// The most players the owner's current map holds for a group; 0 = no cap (feature off, battleground/arena).
// `map_name` is set for an instance and left empty in the open world.
inline uint32 InstanceGroupCapFor(Player const* owner, std::string* map_name = nullptr)
{
    if (!owner || !Playerbot::Services::Config().behavior_instance_group_cap())
        return 0;
    Map const* map = owner->GetMap();
    if (map && map->IsBattlegroundOrArena())
        return 0;
    if (!map || !map->IsDungeon())
        return std::min<uint32>(Playerbot::Services::Config().behavior_open_world_group_cap(), MAX_RAID_SIZE);
    if (map_name)
        *map_name = map->GetMapName() ? map->GetMapName() : "";
    InstanceMap const* im = map->ToInstanceMap();
    uint32 cap = im ? im->GetMaxPlayers() : 0;
    // A raid is as big as the map says: the difficulty row can name a smaller flex size than the 40-player classic raid
    // the map is (Molten Core refused a 10-bot add, playtest 2026-10-08), so the larger of the two counts.
    if (map->IsRaid() && map->GetEntry() && map->GetEntry()->MaxPlayers > 0)
        cap = std::max<uint32>(cap, uint32(map->GetEntry()->MaxPlayers));
    if (!cap)
        cap = map->IsRaid() ? MAX_RAID_SIZE : 5;
    return std::min<uint32>(cap, MAX_RAID_SIZE);
}

// "<map> is full: 5/5" in an instance, the open-world wording otherwise (`map_name` empty). In the open world `have` is
// the number of companions out.
inline std::string InstanceGroupFullText(std::string const& map_name, uint32 have, uint32 cap)
{
    if (map_name.empty())
        return "You have " + std::to_string(have) + " of " + std::to_string(cap) +
               " bots out in the open world: dismiss some, invite world bots for more, or call the rest inside an instance";
    return map_name + " is full: " + std::to_string(have) + "/" + std::to_string(cap);
}

// True (and `msg` set) when the owner's group, human included, is already at the cap of the map they are in.
inline bool InstanceGroupFull(Player const* owner, std::string& msg)
{
    std::string name;
    uint32 const cap = InstanceGroupCapFor(owner, &name);
    if (!cap)
        return false;
    Group const* group = owner->GetGroup();
    uint32 const have = group ? group->GetMembersCount() : 1;
    if (have < cap)
        return false;
    msg = InstanceGroupFullText(name, have, cap);
    return true;
}

// The owner's companions (bots owned by the account) that are online right now, `except` left out. Invited world bots are
// not owned, so they never count. O(registry): keep it out of per-tick paths.
inline uint32 CompanionsOut(Player const* owner, Playerbot::BotId except = 0, std::vector<Playerbot::BotId>* ids = nullptr)
{
    uint32 n = 0;
    if (!owner || !owner->GetSession())
        return 0;
    for (Playerbot::BotId id : Playerbot::Services::Owners().BotsOwnedBy(owner->GetSession()->GetAccountId()))
    {
        if (id == except || !ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id)))
            continue;
        ++n;
        if (ids)
            ids->push_back(id);
    }
    return n;
}

// True (and `msg` set) when one more companion may not come out: the open world holds
// Playerbot.Behavior.OpenWorldGroupCap of the owner's companions (a group may grow past it with invited world bots), an
// instance holds what the map holds and its group never grows past that. `except` = the companion being brought (already
// online, not counted).
inline bool CompanionSpawnFull(Player const* owner, Playerbot::BotId except, std::string& msg)
{
    std::string name;
    uint32 const cap = InstanceGroupCapFor(owner, &name);
    if (!cap)
        return false;
    if (!name.empty() && InstanceGroupFull(owner, msg))
        return true;
    uint32 const out = CompanionsOut(owner, except);
    if (out < cap)
        return false;
    msg = InstanceGroupFullText(name, out, cap);
    return true;
}

} // namespace Playerbot::V2::Fleet
