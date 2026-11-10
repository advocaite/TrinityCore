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

// Classic 1.60 port of VMaNGOS src/scripts/kalimdor/silithus/ruins_of_ahnqiraj/boss_kurinnaxx.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: boss_kurinnaxx
// Follows the VMaNGOS rework of 2026-09-28 (46417908): sniff timers, the sand trap through 26524 (TC
// spell_kurinnaxx_sand_trap picks a random enemy), Wide Slash and Trash cast on self, frenzy as a zone emote.

#include "ScriptMgr.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ScriptedCreature.h"
#include "classic_ruins_of_ahnqiraj.h"
#include "classic_script_text.h"

namespace
{
enum ClassicAQ20Kurinnaxx : uint32
{
    EMOTE_AQ20_KURINNAXX_FRENZY        = 2384,   // VMaNGOS 46417908 (was 10645)

    GO_AQ20_KURINNAXX_TRAP             = 180647,

    SPELL_AQ20_KURINNAXX_MORTALWOUND   = 25646,
    SPELL_AQ20_KURINNAXX_SUMMON_SANDTRAP = 26524,
    SPELL_AQ20_KURINNAXX_ENRAGE        = 26527,
    SPELL_AQ20_KURINNAXX_WIDE_SLASH    = 25814,
    SPELL_AQ20_KURINNAXX_TRASH         = 3391,

    SAY_AQ20_BREACHED                  = 11720   // yelled by Ossirian
};
}

struct classic_boss_kurinnaxx : public ScriptedAI
{
    classic_boss_kurinnaxx(Creature* creature) : ScriptedAI(creature), m_pInstance(creature->GetInstanceScript()) { }

    InstanceScript* m_pInstance;

    uint32 m_uiMortalWound_Timer = 0;
    uint32 m_uiSandTrap_Timer = 0;
    uint32 m_uiCleanSandTrap_Timer = 0;
    uint32 m_uiTrash_Timer = 0;
    uint32 m_uiWideSlash_Timer = 0;
    bool m_bHasEnraged = false;

    void Reset() override
    {
        m_uiMortalWound_Timer = 8000;
        m_uiSandTrap_Timer = 8000;
        m_uiCleanSandTrap_Timer = 0;
        m_uiTrash_Timer = 16000;
        m_uiWideSlash_Timer = 11000;
        m_bHasEnraged = false;
    }

    // VMaNGOS JustRespawned (TC: JustAppeared also runs on first spawn / grid load; DONE is never overwritten)
    void JustAppeared() override
    {
        ScriptedAI::JustAppeared();

        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_AQ20_TYPE_KURINNAXX, NOT_STARTED);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_AQ20_TYPE_KURINNAXX, IN_PROGRESS);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (!m_pInstance)
            return;

        // VMaNGOS DoOrSimulateScriptTextForMap(SAY_BREACHED, NPC_OSSIRIAN, map)
        // TODO(classic): VMaNGOS simulates the yell with Ossirian's name when his grid is not loaded; here it is skipped then.
        if (Creature* pOssirian = me->GetMap()->GetCreature(m_pInstance->GetGuidData(CLASSIC_AQ20_DATA_OSSIRIAN)))
            ClassicScriptText(SAY_AQ20_BREACHED, pOssirian);

        m_pInstance->SetData(CLASSIC_AQ20_TYPE_KURINNAXX, DONE);
    }

    void UpdateAI(uint32 uiDiff) override
    {
        // Despawn an untriggered sand trap after 5 seconds
        if (m_uiCleanSandTrap_Timer)
        {
            if (m_uiCleanSandTrap_Timer <= uiDiff)
            {
                if (GameObject* pTrap = GetClosestGameObjectWithEntry(me, GO_AQ20_KURINNAXX_TRAP, me->GetVisibilityRange()))
                {
                    pTrap->SendGameObjectDespawn();
                    pTrap->Delete();
                }
                m_uiCleanSandTrap_Timer = 0;
            }
            else
                m_uiCleanSandTrap_Timer -= uiDiff;
        }

        if (!UpdateVictim())
            return;

        /* Enrage */
        if (me->GetHealthPct() <= 30.0f && !m_bHasEnraged)
        {
            if (DoCastSelf(SPELL_AQ20_KURINNAXX_ENRAGE) == SPELL_CAST_OK)
            {
                ClassicScriptZoneEmote(EMOTE_AQ20_KURINNAXX_FRENZY, me);
                m_bHasEnraged = true;
            }
        }

        /* Mortal wound */
        if (m_uiMortalWound_Timer < uiDiff)
        {
            if (DoCastVictim(SPELL_AQ20_KURINNAXX_MORTALWOUND) == SPELL_CAST_OK)
                m_uiMortalWound_Timer = 8000;
        }
        else
            m_uiMortalWound_Timer -= uiDiff;

        /* Sand trap: 26524 makes a random enemy cast 25648, which summons the trap under them */
        if (m_uiSandTrap_Timer < uiDiff)
        {
            if (DoCastSelf(SPELL_AQ20_KURINNAXX_SUMMON_SANDTRAP, true) == SPELL_CAST_OK)
            {
                m_uiSandTrap_Timer = 8000;
                m_uiCleanSandTrap_Timer = 5000;
            }
        }
        else
            m_uiSandTrap_Timer -= uiDiff;

        /* WideSlash */
        if (m_uiWideSlash_Timer < uiDiff)
        {
            if (DoCastSelf(SPELL_AQ20_KURINNAXX_WIDE_SLASH) == SPELL_CAST_OK)
                m_uiWideSlash_Timer = 11000;
        }
        else
            m_uiWideSlash_Timer -= uiDiff;

        /* Trash */
        if (m_uiTrash_Timer < uiDiff)
        {
            if (DoCastSelf(SPELL_AQ20_KURINNAXX_TRASH) == SPELL_CAST_OK)
                m_uiTrash_Timer = 16000;
        }
        else
            m_uiTrash_Timer -= uiDiff;
    }
};

void AddSC_classic_boss_kurinnaxx()
{
    RegisterCreatureAI(classic_boss_kurinnaxx);
}
