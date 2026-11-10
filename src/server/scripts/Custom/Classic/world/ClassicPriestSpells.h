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

#ifndef CLASSIC_PRIEST_SPELLS_H
#define CLASSIC_PRIEST_SPELLS_H

#include <cstddef>
#include <cstdint>

// Classic 1.60 priest spells whose client data names a spell nothing else casts (70374 SpellEffect.db2, SpellAudit.tsv). Vanilla
// did these in server code (VMaNGOS Spell.cpp / UnitAuraProcHandler.cpp); the ranks are paired here so the scripts and the tests
// read one table. Pure: no game types.

namespace ClassicPriestSpells
{
    struct RankPair
    {
        uint32_t Spell;     // the trained rank
        uint32_t Follow;    // the spell the rank has to cast
    };

    // Touch of Weakness (Undead): the aura (proc trigger) points at 28598, a dummy with a single DUMMY effect. The shadow damage
    // and the attack power debuff are the per-rank spells 2943 / 19249..19254 (effect 1 SCHOOL_DAMAGE, effect 2 MOD_ATTACK_POWER).
    inline constexpr RankPair TouchOfWeakness[] =
    {
        { 2652, 2943 }, { 19261, 19249 }, { 19262, 19251 }, { 19264, 19252 }, { 19265, 19253 }, { 19266, 19254 },
    };

    // Shadowguard (Troll): the aura points at 28376, a dummy; the shadow damage is the per-rank spells 28377..28382.
    inline constexpr RankPair Shadowguard[] =
    {
        { 18137, 28377 }, { 19308, 28378 }, { 19309, 28379 }, { 19310, 28380 }, { 19311, 28381 }, { 19312, 28382 },
    };

    // Holy Nova: the damage spell has one effect; the heal is the per-rank spell 23455 / 23458 / 23459 / 27803..27805 (client data
    // effect 10 HEAL, 10 yard radius), cast together with it.
    inline constexpr RankPair HolyNova[] =
    {
        { 15237, 23455 }, { 15430, 23458 }, { 15431, 23459 }, { 27799, 27803 }, { 27800, 27804 }, { 27801, 27805 },
    };

    template <std::size_t N>
    constexpr uint32_t Find(RankPair const (&table)[N], uint32_t spell)
    {
        for (RankPair const& pair : table)
            if (pair.Spell == spell)
                return pair.Follow;
        return 0;
    }
}

#endif
