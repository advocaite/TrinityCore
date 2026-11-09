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

// Classic 1.60 port of VMaNGOS src/scripts/kalimdor/silithus/ruins_of_ahnqiraj/boss_moam.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: boss_moam
// Follows the VMaNGOS rework of 2026-09-28 (46417908, sniff based): Moam's own Drain Mana is 25754 (25676 belongs to
// the Obsidian Destroyer), Double Attack, mana reset with 23777, the Mana Fiends come from 25684 and Arcane Eruption
// fires once Moam can pay its 25000 mana.

#include "ScriptMgr.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "ObjectAccessor.h"
#include "QuaternionData.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "TemporarySummon.h"
#include "classic_ruins_of_ahnqiraj.h"
#include "classic_script_text.h"
#include <algorithm>

namespace
{
enum ClassicAQ20Moam : uint32
{
    // VMaNGOS uses script_texts -1509000 / -1509001 / -1509028 which have no rows in the VMaNGOS DB;
    // mapped to the matching Classic broadcast texts.
    EMOTE_AQ20_MOAM_AGGRO             = 11441,  // %s senses your fear.
    EMOTE_AQ20_MOAM_MANA_FULL         = 11473,  // %s bristles with energy.
    EMOTE_AQ20_MOAM_DRAIN             = 11474,  // %s drains your mana and turns to stone.

    SPELL_AQ20_MOAM_TRAMPLE           = 15550,
    SPELL_AQ20_MOAM_DOUBLE_ATTACK     = 18941,
    SPELL_AQ20_MOAM_RESET_MANA        = 23777,  // Zero Mana/Full Health DND (spell_gen_zero_mana_full_health)
    SPELL_AQ20_MOAM_ARCANEERUPTION    = 25672,
    SPELL_AQ20_MOAM_DRAINMANA         = 25754,
    SPELL_AQ20_MOAM_SUMMON_MANA_FIEND = 25684,  // spell_moam_summon_mana_fiends casts 25681, 25682 and 25683
    SPELL_AQ20_MOAM_ENERGIZE          = 25685,

    // mana fiend
    NPC_AQ20_MANA_FIEND               = 15527,

    GO_AQ20_LARGE_OBSIDIAN_CHUNK      = 181069,

    // Arcane Eruption costs 25000 mana; the sniff shows it at 25884 mana, not only at 100%
    AQ20_MOAM_ERUPTION_MANA           = 25000
};
}

/*######
## boss_moam
######*/

struct classic_boss_moam : public ScriptedAI
{
    classic_boss_moam(Creature* creature) : ScriptedAI(creature), m_pInstance(creature->GetInstanceScript()) { }

    InstanceScript* m_pInstance;

    uint32 m_uiTrample_Timer = 0;
    uint32 m_uiDoubleAttack_Timer = 0;
    uint32 m_uiStoneForm_Timer = 0;
    uint32 m_uiTurnBackFromStone_Timer = 0;
    uint32 m_uiDrainMana_Timer = 0;
    ObjectGuid m_OGvictim;          // Memorize last target before turning into stone, then take it back.
    GuidList m_manaFiendGuids;

    void Reset() override
    {
        m_uiTrample_Timer = urand(11000, 13000);
        m_uiDoubleAttack_Timer = urand(8000, 13000);
        m_uiStoneForm_Timer = 90000;
        m_uiTurnBackFromStone_Timer = 90000;
        m_uiDrainMana_Timer = 6000;

        me->SetCanMelee(true); // in case the group wiped during the stone phase
        m_OGvictim.Clear();
        DoCastSelf(SPELL_AQ20_MOAM_RESET_MANA, true);
        DespawnManaFiends();

        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_AQ20_TYPE_MOAM, NOT_STARTED);
    }

    void JustReachedHome() override
    {
        DoCastSelf(SPELL_AQ20_MOAM_RESET_MANA, true);
        DespawnManaFiends();

        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_AQ20_TYPE_MOAM, FAIL);
    }

    void DespawnManaFiends()
    {
        for (ObjectGuid const& guid : m_manaFiendGuids)
            if (Creature* fiend = ObjectAccessor::GetCreature(*me, guid))
                fiend->DespawnOrUnsummon();
        m_manaFiendGuids.clear();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
        DoCastSelf(SPELL_AQ20_MOAM_RESET_MANA, true);
        ClassicScriptText(EMOTE_AQ20_MOAM_AGGRO, me);

        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_AQ20_TYPE_MOAM, IN_PROGRESS);
    }

    void JustDied(Unit* /*killer*/) override
    {
        // VMaNGOS now casts Drop Obsidian (27631, summons 181069); the chunk is summoned directly here with the
        // four day respawn the old script used.
        me->SummonGameObject(GO_AQ20_LARGE_OBSIDIAN_CHUNK, me->GetPosition(), QuaternionData(), Seconds(CLASSIC_AQ20_RESPAWN_FOUR_DAYS), GO_SUMMON_TIMED_DESPAWN);

        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_AQ20_TYPE_MOAM, DONE);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        if (pSummoned->GetEntry() != NPC_AQ20_MANA_FIEND)
            return;

        m_manaFiendGuids.push_back(pSummoned->GetGUID());

        // VMaNGOS: the fiend casts Random Aggro (25680, a dummy) on itself, which picks a random target from Moam's
        // threat list. The dummy has no script here, so the pick is done directly.
        if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
        {
            pSummoned->GetThreatManager().AddThreat(target, 100.0f);
            pSummoned->AI()->AttackStart(target);
        }
    }

    void SummonedCreatureDespawn(Creature* pSummoned) override
    {
        if (pSummoned->GetEntry() == NPC_AQ20_MANA_FIEND)
            m_manaFiendGuids.remove(pSummoned->GetGUID());
    }

    void SummonedCreatureDies(Creature* pSummoned, Unit* /*killer*/) override
    {
        if (pSummoned->GetEntry() != NPC_AQ20_MANA_FIEND)
            return;

        m_manaFiendGuids.remove(pSummoned->GetGUID());

        // all three fiends dead: Moam leaves the stone form at once
        if (m_manaFiendGuids.empty() && me->HasAura(SPELL_AQ20_MOAM_ENERGIZE))
            ExitStoneForm();
    }

    void ExitStoneForm()
    {
        // take back the target memorized before turning into stone
        if (Unit* victim = ObjectAccessor::GetUnit(*me, m_OGvictim))
            AttackStart(victim);
        // VMaNGOS only selects (and discards) a random target when the memorized victim is gone

        me->RemoveAurasDueToSpell(SPELL_AQ20_MOAM_ENERGIZE);
        me->SetCanMelee(true); // VMaNGOS resumes DoMeleeAttackIfReady
        m_uiStoneForm_Timer = 90000;
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!UpdateVictim() && !me->HasAura(SPELL_AQ20_MOAM_ENERGIZE))
            return;

        // Stone form: back out at full mana or after 90 seconds
        if (me->HasAura(SPELL_AQ20_MOAM_ENERGIZE))
        {
            if (m_uiTurnBackFromStone_Timer <= uiDiff)
                m_uiTurnBackFromStone_Timer = 0;
            else
                m_uiTurnBackFromStone_Timer -= uiDiff;

            if (me->GetPower(POWER_MANA) >= me->GetMaxPower(POWER_MANA) || m_uiTurnBackFromStone_Timer == 0)
                ExitStoneForm();

            return;
        }

        // Arcane Eruption as soon as Moam can pay for it
        if (me->GetPower(POWER_MANA) >= int32(std::min<uint32>(AQ20_MOAM_ERUPTION_MANA, me->GetMaxPower(POWER_MANA))))
        {
            if (Unit* victim = me->GetVictim())
                if (DoCast(victim, SPELL_AQ20_MOAM_ARCANEERUPTION) == SPELL_CAST_OK)
                    ClassicScriptText(EMOTE_AQ20_MOAM_MANA_FULL, me);
        }

        // Stone form with three Mana Fiends every 90 seconds
        if (m_uiStoneForm_Timer < uiDiff)
        {
            if (Unit* victim = me->GetVictim())
            {
                if (DoCastSelf(SPELL_AQ20_MOAM_ENERGIZE) == SPELL_CAST_OK)
                {
                    DoCastSelf(SPELL_AQ20_MOAM_SUMMON_MANA_FIEND, true);

                    m_uiStoneForm_Timer = 90000;
                    m_uiTurnBackFromStone_Timer = 90000;
                    m_OGvictim = victim->GetGUID();
                    me->AttackStop();
                    me->SetCanMelee(false); // VMaNGOS skips DoMeleeAttackIfReady during the stone phase
                    ClassicScriptText(EMOTE_AQ20_MOAM_DRAIN, me);
                    return;
                }
            }
        }
        else
            m_uiStoneForm_Timer -= uiDiff;

        if (m_uiTrample_Timer < uiDiff)
        {
            if (DoCastVictim(SPELL_AQ20_MOAM_TRAMPLE) == SPELL_CAST_OK)
                m_uiTrample_Timer = urand(11000, 13000);
        }
        else
            m_uiTrample_Timer -= uiDiff;

        if (m_uiDoubleAttack_Timer < uiDiff)
        {
            if (DoCastSelf(SPELL_AQ20_MOAM_DOUBLE_ATTACK) == SPELL_CAST_OK)
                m_uiDoubleAttack_Timer = urand(8000, 13000);
        }
        else
            m_uiDoubleAttack_Timer -= uiDiff;

        if (m_uiDrainMana_Timer < uiDiff)
        {
            DoCastSelf(SPELL_AQ20_MOAM_DRAINMANA);
            m_uiDrainMana_Timer = 6000;
        }
        else
            m_uiDrainMana_Timer -= uiDiff;
    }
};

void AddSC_classic_boss_moam()
{
    RegisterCreatureAI(classic_boss_moam);
}
