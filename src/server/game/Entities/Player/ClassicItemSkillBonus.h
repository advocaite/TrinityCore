#ifndef TRINITY_CLASSIC_ITEM_SKILL_BONUS_H
#define TRINITY_CLASSIC_ITEM_SKILL_BONUS_H

#include "DB2Structure.h"
#include "Player.h"
#include <algorithm>
#include <span>
#include <unordered_set>

namespace ClassicItemSkillBonus
{
    template<class Parent>
    int32 StoredBonus(uint32 skill, std::span<uint32 const> skills, std::span<int32 const> amounts, Parent const& parent)
    {
        int32 bonus = 0;
        std::unordered_set<uint32> visited;
        for (; skill && visited.insert(skill).second; skill = parent(skill))
            for (std::size_t i = 0; i < std::min(skills.size(), amounts.size()); ++i)
                if (skills[i] == skill)
                    bonus += amounts[i];
        return bonus;
    }

    inline bool Restore(uint32 skill, int32 bonus, SkillStatusMap const& status,
        UF::ActivePlayerData const& data, UF::MutableFieldReference<UF::ActivePlayerData, false> fields)
    {
        auto itr = status.find(skill);
        if (!bonus || itr == status.end() || itr->second.uState == SKILL_DELETED
            || itr->second.pos >= PLAYER_MAX_SKILLS || !data.Skill->SkillRank[itr->second.pos])
            return false;
        uint32 pos = itr->second.pos;
        auto temporary = fields.ModifyValue(&UF::ActivePlayerData::Skill).ModifyValue(&UF::SkillInfo::SkillTempBonus, pos);
        UF::SetUpdateFieldValue(temporary, uint16(int16(data.Skill->SkillTempBonus[pos]) + bonus));
        return true;
    }
}
#endif
