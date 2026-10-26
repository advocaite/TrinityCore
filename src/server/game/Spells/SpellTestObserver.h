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

#ifndef TRINITY_SPELL_TEST_OBSERVER_H
#define TRINITY_SPELL_TEST_OBSERVER_H

#include "Define.h"

class AuraApplication;
class ProcEventInfo;
class Spell;
class WorldObject;

namespace WorldPackets::CombatLog
{
    class CombatLogServerPacket;
}

// The spell test module (src/modules/SpellTest, CMake option BUILD_SPELL_TEST) records what its test characters cast,
// deal, gain and proc through this. Without the module nothing registers, and each hook is a null check.
class TC_GAME_API SpellTestObserver
{
public:
    virtual ~SpellTestObserver() = default;

    // Every combat log message (spell go, damage, heal, periodic tick, energize, melee swing, absorb...) before it is sent
    virtual void OnCombatLog(WorldObject const* sender, WorldPackets::CombatLog::CombatLogServerPacket const* combatLog) = 0;
    // Every spell that finishes its cast, triggered ones and those the client is never told about included
    virtual void OnSpellGo(Spell const* spell) = 0;
    virtual void OnCastFailed(Spell const* spell, int32 result) = 0;
    // After the aura's effects are applied, and before they are removed
    virtual void OnAuraApplied(AuraApplication const* aurApp) = 0;
    virtual void OnAuraRemoved(AuraApplication const* aurApp, uint32 removeMode) = 0;
    // The aura passed its proc roll and procs now
    virtual void OnAuraProc(AuraApplication const* aurApp, uint32 procEffectMask, ProcEventInfo const& eventInfo) = 0;

    static SpellTestObserver* Instance;
};

#endif // TRINITY_SPELL_TEST_OBSERVER_H
