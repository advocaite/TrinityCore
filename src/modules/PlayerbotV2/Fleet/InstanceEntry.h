// InstanceEntry.h: taking an owner's companion into the instance the owner stands in.
//
// A companion is refused at the door (Map::PlayerCannotEnter) when its own raid lock names a different instance than
// the one the group uses: the saved bots of last night's Molten Core kill hold a lock on that instance, and a fresh
// instance made for an owner without a lock is "locked to a different instance" for every one of them. The owner's
// own bots follow the owner, so such a stale lock on the map is dropped. Bots never collect raid saves any more (the core
// skips them when binding), so this only clears saves written before that. Every refusal is logged with its reason, the
// entrance walk stays the fallback.
#pragma once

#include "DB2Stores.h"
#include "Group.h"
#include "InstanceLockMgr.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "WorldSession.h"

namespace Playerbot::V2::Fleet
{

inline char const* TransferAbortName(TransferAbortReason reason)
{
    switch (reason)
    {
        case TRANSFER_ABORT_NONE:                          return "none";
        case TRANSFER_ABORT_MAP_NOT_ALLOWED:               return "map not allowed";
        case TRANSFER_ABORT_DIFFICULTY:                    return "difficulty not available";
        case TRANSFER_ABORT_NEED_GROUP:                    return "raid group needed";
        case TRANSFER_ABORT_LOCKED_TO_DIFFERENT_INSTANCE:  return "locked to a different instance";
        case TRANSFER_ABORT_ALREADY_COMPLETED_ENCOUNTER:   return "already completed an encounter";
        case TRANSFER_ABORT_TOO_MANY_INSTANCES:            return "too many instances per hour";
        case TRANSFER_ABORT_MAX_PLAYERS:                   return "instance full";
        case TRANSFER_ABORT_ZONE_IN_COMBAT:                return "encounter in progress";
        case TRANSFER_ABORT_NOT_FOUND:                     return "instance not found";
        default:                                           return "other";
    }
}

// Readies `bot` to enter instance map `map_id` and returns why it still cannot (TRANSFER_ABORT_NONE: it can). A bot holds no
// raid save of its own, so a lock that names another instance is dropped. Not an instance map, or the bot is already
// there: nothing to do.
inline TransferAbortReason PrepareInstanceEntry(Player* bot, uint32 map_id)
{
    MapEntry const* entry = sMapStore.LookupEntry(map_id);
    if (!bot || !entry || !entry->IsDungeon() || bot->GetMapId() == map_id)
        return TRANSFER_ABORT_NONE;

    TransferAbortParams denied = Map::PlayerCannotEnter(map_id, bot);
    if (!denied)
        return TRANSFER_ABORT_NONE;

    bool const staleLock = denied.Reason == TRANSFER_ABORT_LOCKED_TO_DIFFERENT_INSTANCE ||
                           denied.Reason == TRANSFER_ABORT_ALREADY_COMPLETED_ENCOUNTER;
    if (staleLock)
    {
        Group const* group = bot->GetGroup();
        Difficulty difficulty = group ? group->GetDifficultyID(entry) : bot->GetDifficultyID(entry);
        if (MapDifficultyEntry const* mapDiff = sDB2Manager.GetDownscaledMapDifficultyData(map_id, difficulty))
            difficulty = Difficulty(mapDiff->DifficultyID);
        std::vector<InstanceLock const*> reset, failed;
        sInstanceLockMgr.ResetInstanceLocksForPlayer(bot->GetGUID(), map_id, difficulty, &reset, &failed);
        TC_LOG_INFO("playerbot.v2", "[instance_enter] {} was refused for map {} ({}): its old save dropped, bots follow the group "
            "(reset {}, still in use {})", bot->GetName(), map_id, TransferAbortName(denied.Reason), reset.size(), failed.size());
        denied = Map::PlayerCannotEnter(map_id, bot);
    }
    if (denied)
        TC_LOG_INFO("playerbot.v2", "[instance_enter] {} cannot enter map {}: {}", bot->GetName(), map_id, TransferAbortName(denied.Reason));
    return denied.Reason;
}

}  // namespace Playerbot::V2::Fleet
