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

#ifndef TRINITY_CLASSIC_SPELL_RANK_H
#define TRINITY_CLASSIC_SPELL_RANK_H

#include <cstdint>

// Classic 1.60: a lower rank that a higher known rank superseded casts the higher rank.
//
// Learning a rank marks the lower ranks inactive and tells the client to replace them in the spellbook and on the action bars
// (Player::AddSpell). That only reaches bars the client holds while the rank is learned: bars saved earlier, bars written by
// .learn all, and bars of characters that logged in with the rank already known keep the lower rank id, and the server cast it.
namespace ClassicSpellRank
{
    // The highest active rank above spellId when spellId is a known but superseded rank, else spellId itself. next(id) is the
    // next rank of the chain (0 at the end), active(id) tells whether the character knows that rank as its active one.
    template <typename NextFn, typename ActiveFn>
    uint32_t Resolve(uint32_t spellId, bool knownButSuperseded, NextFn next, ActiveFn active)
    {
        if (!knownButSuperseded)
            return spellId;

        uint32_t best = spellId;
        uint32_t steps = 0;
        for (uint32_t id = next(spellId); id && steps < 64; id = next(id), ++steps)
            if (active(id))
                best = id;
        return best;
    }
}

#endif
