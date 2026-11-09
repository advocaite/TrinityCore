#ifndef PLAYERBOT_UTIL_CHEST_LOCK_H
#define PLAYERBOT_UTIL_CHEST_LOCK_H

// A chest object (herb, ore vein, locked box) whose lock needs a skill. A player opens these with a spell (Herb Gathering,
// Mining, Pick Lock) that checks the skill; GameObject::Use() does not, so the bot rules must never click them.

#include "DB2Stores.h"
#include "GameObjectData.h"
#include "SharedDefines.h"

namespace Playerbot {

inline bool ChestNeedsSkill(GameObjectTemplate const* info)
{
    if (!info || info->type != GAMEOBJECT_TYPE_CHEST)
        return false;
    LockEntry const* lock = sLockStore.LookupEntry(info->GetLockId());
    if (!lock)
        return false;
    for (uint8 i = 0; i < MAX_LOCK_CASE; ++i)
        if (lock->Type[i] == LOCK_KEY_SKILL && SkillByLockType(LockType(lock->Index[i])) != SKILL_NONE)
            return true;
    return false;
}

} // namespace Playerbot

#endif
