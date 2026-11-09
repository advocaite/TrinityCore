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

#ifndef TRINITY_CLASSIC_ARMOR_H
#define TRINITY_CLASSIC_ARMOR_H

#include <algorithm>

// Classic 1.60 (vanilla 1.12, VMaNGOS SpellCaster::CalcArmorReducedDamage): physical damage reduction of armor.
// mitigation = armor / (armor + 400 + 85 * attacker level), at most 75%. The client's retail ArmorConstant expected-stat table
// (and its 85% cap) made every melee and ranged hit differ from vanilla. Pure: no game types.
namespace ClassicArmor
{
    inline float Constant(unsigned attackerLevel)
    {
        return 400.0f + 85.0f * float(attackerLevel);
    }

    constexpr float MaxMitigation = 0.75f;

    // Share of the damage armor takes off, 0..0.75.
    inline float Mitigation(float armor, unsigned attackerLevel)
    {
        if (armor <= 0.0f)
            return 0.0f;
        return std::min(armor / (armor + Constant(attackerLevel)), MaxMitigation);
    }
}

#endif
