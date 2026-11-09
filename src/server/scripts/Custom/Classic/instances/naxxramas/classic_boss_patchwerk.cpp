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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/eastern_plaguelands/naxxramas/boss_patchwerk.cpp (GPL-2)
// Scripts: boss_patchwerk

#include "ScriptMgr.h"
#include "Creature.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "ThreatManager.h"
#include "classic_naxxramas.h"
#include "classic_script_text.h"

namespace
{
enum ClassicNaxxPatchwerkData : uint32
{
    SAY_PATCHWERK_AGGRO1          = 13068,
    SAY_PATCHWERK_AGGRO2          = 13069,
    SAY_PATCHWERK_SLAY            = 13071,
    SAY_PATCHWERK_DEATH           = 13070,

    EMOTE_PATCHWERK_BERSERK       = 4428,
    EMOTE_PATCHWERK_ENRAGE        = 2384,

    SPELL_PATCHWERK_SUMMON_PLAYER = 20477,
    SPELL_PATCHWERK_HATEFUL_STRIKE = 28308,
    SPELL_PATCHWERK_ENRAGE        = 28131, // 5% enrage soft enrage
    SPELL_PATCHWERK_BERSERK       = 26662, // 7min hard enrage: the vanilla Berserk (+500% damage, +150% attack speed), the spell his
                                          // VMaNGOS row carries; 27680 is the Berserk with +150% movement speed instead
    SPELL_PATCHWERK_SLIMEBOLT     = 32309  // Added in patch 1.12
};

enum ClassicNaxxPatchwerkEvents : uint32
{
    EVENT_PATCHWERK_BERSERK = 1,
    EVENT_PATCHWERK_HATEFULSTRIKE,
    EVENT_PATCHWERK_SLIMEBOLT
};

constexpr Milliseconds NAXX_PATCHWERK_BERSERK_TIMER      = 7min;  // 7 minutes enrage
constexpr Milliseconds NAXX_PATCHWERK_HATEFUL_CD         = 1200ms;

// 30 sec after berserk he starts throwing slime at ppl
// this was added in 1.12.1 to cope with guilds kiting him
constexpr Milliseconds NAXX_PATCHWERK_SLIMEBOLT_INITIAL   = NAXX_PATCHWERK_BERSERK_TIMER + 30s;
constexpr Milliseconds NAXX_PATCHWERK_SLIMEBOLT_REPEAT_CD = 5000ms;
}

struct classic_boss_patchwerk : public ScriptedAI
{
    classic_boss_patchwerk(Creature* creature) : ScriptedAI(creature)
    {
        m_pInstance = GetClassicNaxxInstance(creature);
        m_bEnraged = false;
        m_bBerserk = false;
        m_failedStrikes = 0;
    }

    classic_instance_naxxramas_InstanceScript* m_pInstance;

    EventMap m_events;

    bool   m_bEnraged;
    bool   m_bBerserk;
    uint32 m_failedStrikes;
    ObjectGuid m_previousTarget;

    void Reset() override
    {
        m_events.Reset();
        m_bEnraged = false;
        m_bBerserk = false;
        m_failedStrikes = 0;
        m_previousTarget.Clear();
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        if (urand(0, 4))
            return;

        ClassicScriptText(SAY_PATCHWERK_SLAY, me);
    }

    void JustDied(Unit* /*killer*/) override
    {
        ClassicScriptText(SAY_PATCHWERK_DEATH, me);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_PATCHWERK, DONE);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_PATCHWERK, FAIL);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        ClassicScriptText(urand(0, 1) ? SAY_PATCHWERK_AGGRO1 : SAY_PATCHWERK_AGGRO2, me);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_PATCHWERK, IN_PROGRESS);

        m_events.ScheduleEvent(EVENT_PATCHWERK_BERSERK, NAXX_PATCHWERK_BERSERK_TIMER);
        m_events.ScheduleEvent(EVENT_PATCHWERK_HATEFULSTRIKE, NAXX_PATCHWERK_HATEFUL_CD);
        // VMaNGOS: #if SUPPORTED_CLIENT_BUILD >= CLIENT_BUILD_1_12_1
        m_events.ScheduleEvent(EVENT_PATCHWERK_SLIMEBOLT, NAXX_PATCHWERK_SLIMEBOLT_INITIAL);
    }

    void DoHatefulStrike()
    {
        // The ability is used on highest HP target in melee
        // current tank cannot be hit by hateful as long as there are other players in melee

        // todo: can it hit anything other than players?

        SpellInfo const* pHatefulStrike = sSpellMgr->GetSpellInfo(SPELL_PATCHWERK_HATEFUL_STRIKE, me->GetMap()->GetDifficultyID());
        if (!pHatefulStrike)
        {
            TC_LOG_ERROR("scripts", "classic_boss_patchwerk - Hateful Strike spell does not exist?!");
            return;
        }

        Unit* mainTank = me->GetVictim();

        // Shouldnt really be possible, but hey, weirder things have happened
        if (!mainTank)
            return;

        ObjectGuid const mainTankGuid = mainTank->GetGUID();

        Unit* pTarget = nullptr;
        uint64 uiHighestHP = 0;
        uint8 threatListPosition = 0;

        for (ThreatReference const* ref : me->GetThreatManager().GetSortedThreatList())
        {
            // Only top 4 players on threat in melee range are targetted.
            if (threatListPosition > 3)
                break;

            Player* pTempTarget = ref->GetVictim()->ToPlayer();
            if (!pTempTarget)
                continue;

            if (!me->IsInMap(pTempTarget))
                continue;

            // VMaNGOS CanReachWithMeleeSpellAttack
            if (!me->IsWithinMeleeRange(pTempTarget))
                continue;

            if (pTempTarget->IsImmunedToSpell(pHatefulStrike, MAX_EFFECT_MASK, me))
                continue;

            // Skipping maintank, only using him if there is no other viable target
            // todo: not sure if this is correct. Should we target the MT over the offtanks, if the offtanks have less hp?
            if (pTempTarget->GetGUID() != mainTankGuid)
            {
                // target has higher hp than anyone checked so far
                if (pTempTarget->GetHealth() > uiHighestHP)
                {
                    pTarget = pTempTarget;
                    uiHighestHP = pTarget->GetHealth();
                }
            }

            threatListPosition++;
        }

        // If we found no viable target, we choose the maintank
        if (!pTarget)
            pTarget = mainTank;

        if (pTarget->GetGUID() != m_previousTarget)
        {
            me->SetInFront(pTarget);
            me->SetTarget(pTarget->GetGUID());
            m_previousTarget = pTarget->GetGUID();
        }

        if (me->CastSpell(pTarget, SPELL_PATCHWERK_HATEFUL_STRIKE, false) == SPELL_FAILED_OUT_OF_RANGE)
        {
            if (++m_failedStrikes >= 3)
            {
                if (Player* pPlayer = pTarget->ToPlayer())
                    if (!pPlayer->IsBeingTeleported())
                        me->CastSpell(pPlayer, SPELL_PATCHWERK_SUMMON_PLAYER, true);
            }
        }
        else
            m_failedStrikes = 0;
    }

    // VMaNGOS CustomGetTarget(): standard threat-based target selection, but the displayed target / facing is only switched
    // back from the hateful strike target to the current victim when the swing timer is ready or the victim is out of melee.
    // TODO(classic): TC UpdateVictim() (Creature::SelectVictim) already re-faces the victim every update and handles evade,
    // so only the target-guid restore part of CustomGetTarget is reproduced here.
    void UpdatePatchwerkDisplayedTarget()
    {
        Unit* target = me->GetVictim();
        if (!target)
            return;

        if (me->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_CONFUSED | UNIT_STATE_FLEEING)
            || (me->HasAuraType(SPELL_AURA_MOD_FEAR) && !me->HasAuraType(SPELL_AURA_PREVENTS_FLEEING)) || me->HasAuraType(SPELL_AURA_MOD_CONFUSE))
            return;

        if (!me->isAttackReady(BASE_ATTACK) && me->IsWithinMeleeRange(target)) // he does not have offhand attack
            return;

        if (target->GetGUID() != m_previousTarget)
        {
            me->SetInFront(target);
            me->SetTarget(target->GetGUID());
            m_previousTarget = target->GetGUID();
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        UpdatePatchwerkDisplayedTarget();

        // Soft Enrage at 5%
        if (!m_bEnraged)
        {
            if (me->GetHealthPct() < 5.0f)
            {
                DoCastSelf(SPELL_PATCHWERK_ENRAGE);
                ClassicScriptText(EMOTE_PATCHWERK_ENRAGE, me);
                m_bEnraged = true;
            }
        }

        m_events.Update(diff);
        while (uint32 eventId = m_events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_PATCHWERK_BERSERK:
                    if (DoCastSelf(SPELL_PATCHWERK_BERSERK) == SPELL_CAST_OK)
                    {
                        ClassicScriptText(EMOTE_PATCHWERK_BERSERK, me);
                        m_bBerserk = true;
                    }
                    else
                        m_events.Repeat(100ms);
                    break;
                case EVENT_PATCHWERK_HATEFULSTRIKE:
                    DoHatefulStrike();
                    m_events.Repeat(NAXX_PATCHWERK_HATEFUL_CD);
                    break;
                case EVENT_PATCHWERK_SLIMEBOLT:
                    if (DoCastVictim(SPELL_PATCHWERK_SLIMEBOLT) == SPELL_CAST_OK)
                        m_events.Repeat(NAXX_PATCHWERK_SLIMEBOLT_REPEAT_CD);
                    else
                        m_events.Repeat(100ms);
                    break;
                default:
                    break;
            }
        }
    }
};

void AddSC_classic_boss_patchwerk()
{
    RegisterCreatureAI(classic_boss_patchwerk);
}
