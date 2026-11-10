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

#ifndef TRINITY_CLASSIC_RAGE_H
#define TRINITY_CLASSIC_RAGE_H

#include <cstdint>

// Classic 1.60 (vanilla, VMaNGOS conversion): rage from melee damage. A hit gives rage from the damage dealt (plus absorbed); a
// dodge or parry gives it from 75% of the damage the swing would have done. Dual Wield Specialization raises the rage of off-hand
// attacks by a percentage. The damage stays fractional until the rage itself is worked out: rounding it down to whole points
// first threw the talent's bonus away on small hits (3 damage * 1.1 = 3, anything under 10 damage got nothing at all).
namespace ClassicRage
{
    // The damage a melee swing counts for rage. bonusPct: Dual Wield Specialization's off-hand bonus (0 for other swings).
    inline float SwingRageDamage(bool dodgeOrParry, uint32_t cleanDamage, uint32_t damagePlusAbsorb, float bonusPct)
    {
        float damage = dodgeOrParry ? float(cleanDamage) * 0.75f : float(damagePlusAbsorb);
        return damage * (1.0f + bonusPct / 100.0f);
    }

    // The level's rage conversion value: rage = damage / conversion * 7.5 when dealing damage, * 2.5 when taking it.
    inline float Conversion(float level)
    {
        return 0.0091107836f * level * level + 3.225598133f * level + 4.2652911f;
    }

    // Rage points (not tenths) for the damage, before rage auras and the server rate.
    inline float RageFromDamage(float damage, float level, bool attacker)
    {
        return damage / Conversion(level) * (attacker ? 7.5f : 2.5f);
    }
}

#endif
