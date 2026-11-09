// ForeignClassSpell - whether a spell a bot knows belongs to another class (Api/PlayerbotAPI.cpp purge_foreign_class_spells), or to a
// higher level than its own.
#pragma once

#include <cstdint>

namespace Playerbot::Api {

// taught_by: the union of the class masks (1 << (class - 1)) of the trainers that teach the spell, 0 when no class trainer teaches it.
// A spell no class trainer teaches says nothing about its owner and is kept; one that only other classes' trainers teach is foreign.
constexpr bool IsForeignClassSpell(uint32_t taught_by, uint32_t class_mask)
{
    return taught_by != 0 && (taught_by & class_mask) == 0;
}

// A spell a bot knows that its level may not have yet (Api/PlayerbotAPI.cpp unlearn_spells_above_level). trainer_req_level: the lowest
// level at which a trainer of the bot's class sells it, 0 when none does; spell_level: the rank's own level, which is all a book rank no
// trainer sells carries. A character set down a level keeps every rank it knew: the scenario priest re-levelled from 60 to 14 healed
// with Flash Heal and Renew of level 50 and Greater Heal, near 400 mana each from a 1435 pool, and was out of mana in the first pulls.
constexpr bool IsSpellAboveLevel(uint32_t trainer_req_level, uint32_t spell_level, uint32_t level)
{
    return (trainer_req_level ? trainer_req_level : spell_level) > level;
}

} // namespace Playerbot::Api
