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

// Classic 1.60 port of VMaNGOS src/scripts/kalimdor/silithus/ruins_of_ahnqiraj/ruins_of_ahnqiraj.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: mob_anubisath_guardian, mob_qiraji_swarmguard, mob_flesh_hunter, mob_tornado_ossirian, mob_qiraji_gladiator,
//          mob_silicate_feeder, mob_hive_zara_soldier, mob_obsidian_destroyer, mob_hive_zara_stinger, mob_qiraji_warrior,
//          mob_swarmguard_needler, boss_tuubid, spell_aq20_drain_mana (25676, 25754), spell_rajaxx_thundercrash (25599)

#include "ScriptMgr.h"
#include "Containers.h"
#include "Creature.h"
#include "CreatureData.h"
#include "CreatureGroups.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuaternionData.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "ThreatManager.h"
#include "classic_ruins_of_ahnqiraj.h"
#include "classic_script_text.h"
#include <list>

namespace
{
// Anubisath guardian
enum ClassicAQ20AnubisathGuardian : uint32
{
    SPELL_AQ20_GUARDIAN_METEOR       = 24340,
    SPELL_AQ20_GUARDIAN_PLAGUE       = 22997,
    SPELL_AQ20_GUARDIAN_SHADOW_STORM = 26546,
    SPELL_AQ20_GUARDIAN_THUNDER_CLAP = 26554,
    SPELL_AQ20_GUARDIAN_REFLECT_ARFR = 13022,
    SPELL_AQ20_GUARDIAN_REFLECT_FSSH = 19595,
    SPELL_AQ20_GUARDIAN_ENRAGE       = 8269, //8559,
    SPELL_AQ20_GUARDIAN_EXPLODE      = 25699,
    SPELL_AQ20_GUARDIAN_INIT_EXPLODE = 25698,

    EMOTE_AQ20_GUARDIAN_FRENZY       = 10677,

    NPC_AQ20_ANU_WARRIOR             = 15537,
    NPC_AQ20_ANU_SWARM               = 15538,

    GO_AQ20_SMALL_OBSIDIAN_CHUNK     = 181068
};

// Flesh hunter
enum ClassicAQ20FleshHunter : uint32
{
    SPELL_AQ20_FH_TRASH              = 3391,
    SPELL_AQ20_FH_CONSUME            = 25371, //26186, //25371,
    SPELL_AQ20_FH_CONSUME_HEAL       = 25378,
    SPELL_AQ20_FH_POISON_BOLT        = 25424,
    SPELL_AQ20_FH_CONSUME_DMG        = 25373,
    SPELL_AQ20_FH_SPLIT              = 25383
};

// Ossirian tornado
enum ClassicAQ20Tornado : uint32
{
    SPELL_AQ20_TORNADO_SANDSTORM     = 25160,
    SPELL_AQ20_TORNADO_AURA          = 10092
};

// Obsidian destroyer (VMaNGOS 46417908, sniffed: its Drain Mana is 25676, Moam's is 25754)
enum ClassicAQ20ObsidianDestroyer : uint32
{
    SPELL_AQ20_OD_RESET_MANA         = 23777,   // Zero Mana/Full Health DND
    SPELL_AQ20_OD_PURGE              = 25756,
    SPELL_AQ20_OD_DRAINMANA          = 25676
};

// Hive'Zara soldier
enum ClassicAQ20HiveZaraSoldier : uint32
{
    SPELL_AQ20_HZS_VENOM_SPIT        = 25497,
    SPELL_AQ20_HZS_RETALIATION       = 22857
};

// Silicate feeder
enum ClassicAQ20SilicateFeeder : uint32
{
    SPELL_AQ20_SF_CLOUD_OF_DISEASE   = 17742,
    FACTION_AQ20_SF_PASSIVE          = 7,
    FACTION_AQ20_SF_HOSTILE          = 14
};

// Qiraji swarmguard
enum ClassicAQ20QirajiSwarmguard : uint32
{
    SPELL_AQ20_QS_SUNDERING_CLEAVE   = 25174
};

// Qiraji gladiator
enum ClassicAQ20QirajiGladiator : uint32
{
    SPELL_AQ20_QG_TRAMPLE            = 5568,
    SPELL_AQ20_QG_UPPERCUT           = 10966,
    SPELL_AQ20_QG_VENGEANCE          = 25164
};

// Hive'Zara stinger
enum ClassicAQ20HiveZaraStinger : uint32
{
    SPELL_AQ20_HZST_CHARGE           = 25190
};

// Captain Tuubid
enum ClassicAQ20Tuubid : uint32
{
    // SAY_TUUBID_KILL = -1900117 (VMaNGOS script_texts id with no row in the VMaNGOS DB and no broadcast text)
    SPELL_AQ20_TUUBID_ATTACK_ORDER   = 25471,
    SPELL_AQ20_TUUBID_CLEAVE         = 26350,
    SPELL_AQ20_TUUBID_SUNDER_ARMOR   = 24317
};

// Qiraji warrior
enum ClassicAQ20QirajiWarrior : uint32
{
    SPELL_AQ20_QW_ENRAGE             = 8599,
    SPELL_AQ20_QW_THUNDERCLAP        = 15588,
    SPELL_AQ20_QW_UPPERCUT           = 10966
};

// Swarmguard needler
enum ClassicAQ20SwarmguardNeedler : uint32
{
    SPELL_AQ20_SN_CLEAVE             = 20684
};

// Spell scripts
enum ClassicAQ20SpellScripts : uint32
{
    SPELL_AQ20_DRAIN_MANA_MOAM             = 25754,
    SPELL_AQ20_DRAIN_MANA_MOAM_EFFECT      = 25671,   // 500 mana
    SPELL_AQ20_DRAIN_MANA_DESTROYER        = 25676,
    SPELL_AQ20_DRAIN_MANA_DESTROYER_EFFECT = 25755    // 250 mana
};

// VMaNGOS CreatureGroup::GetOriginalLeaderGuid().GetEntry()
uint32 ClassicAQ20TrashFormationLeaderEntry(Creature* creature)
{
    if (CreatureGroup* group = creature->GetFormation())
        if (CreatureData const* data = sObjectMgr->GetCreatureData(group->GetLeaderSpawnId()))
            return data->id;
    return 0;
}

void ClassicAQ20SummonObsidianChunk(Creature* creature, uint32 entry)
{
    // VMaNGOS: SummonGameObject(...) + SetRespawnTime(345600)
    creature->SummonGameObject(entry, creature->GetPosition(), QuaternionData(), Seconds(CLASSIC_AQ20_RESPAWN_FOUR_DAYS), GO_SUMMON_TIMED_DESPAWN);
}
}

/*######
## mob_anubisath_guardian
######*/

struct classic_mob_anubisath_guardian : public ScriptedAI
{
    classic_mob_anubisath_guardian(Creature* creature) : ScriptedAI(creature) { }

    uint32 m_uiSpell1 = 0;
    uint32 m_uiSpell2 = 0;
    uint32 m_uiSpell3 = 0;
    uint32 m_uiSpell4 = 0;
    uint32 m_uiNPCSummon = 0;

    uint32 m_uiSpell1_Timer = 0;
    uint32 m_uiSpell2_Timer = 0;
    uint32 m_uiSummon_Timer = 0;
    uint32 m_uiExplode_Timer = 0;

    uint8 m_uiSummonCount = 0;

    bool m_bIsEnraged = false;
    bool m_bIsExploding = false;

    void Reset() override
    {
        m_uiSpell1 = urand(0, 1) ? SPELL_AQ20_GUARDIAN_METEOR : SPELL_AQ20_GUARDIAN_PLAGUE;
        m_uiSpell2 = urand(0, 1) ? SPELL_AQ20_GUARDIAN_SHADOW_STORM : SPELL_AQ20_GUARDIAN_THUNDER_CLAP;
        m_uiSpell3 = urand(0, 1) ? SPELL_AQ20_GUARDIAN_REFLECT_ARFR : SPELL_AQ20_GUARDIAN_REFLECT_FSSH;
        m_uiSpell4 = urand(0, 1) ? SPELL_AQ20_GUARDIAN_ENRAGE : SPELL_AQ20_GUARDIAN_INIT_EXPLODE;
        m_uiNPCSummon = urand(0, 1) ? NPC_AQ20_ANU_WARRIOR : NPC_AQ20_ANU_SWARM;

        m_uiSpell1_Timer = 10000;
        m_uiSpell2_Timer = 20000;
        m_uiSummon_Timer = 10000;
        m_bIsEnraged = false;
        m_bIsExploding = false;
        m_uiSummonCount = 0;
        m_uiExplode_Timer = 6000;

        me->RemoveAllAuras();
    }

    void JustDied(Unit* /*killer*/) override
    {
        ClassicAQ20SummonObsidianChunk(me, GO_AQ20_SMALL_OBSIDIAN_CHUNK);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoCastSelf(m_uiSpell3);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        if (Unit* victim = me->GetVictim())
            pSummoned->AI()->AttackStart(victim);
        ++m_uiSummonCount;
    }

    void SummonedCreatureDespawn(Creature* /*summon*/) override
    {
        --m_uiSummonCount;
    }

    void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (!m_bIsEnraged && (me->GetHealth() * 100 / me->GetMaxHealth()) < 10)
        {
            if (m_uiSpell4 == SPELL_AQ20_GUARDIAN_ENRAGE)
            {
                if (Unit* victim = me->GetVictim())
                    DoCast(victim, m_uiSpell4);
                ClassicScriptText(EMOTE_AQ20_GUARDIAN_FRENZY, me);
                m_bIsEnraged = true;
            }
            else
            {
                me->CastSpell(me, m_uiSpell4, false);
                m_bIsExploding = true;
                m_uiExplode_Timer = 6000;
            }
        }
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!UpdateVictim())
            return;

        if (m_uiExplode_Timer < uiDiff && m_bIsExploding)
        {
            if (DoCastSelf(SPELL_AQ20_GUARDIAN_EXPLODE) == SPELL_CAST_OK)
                m_uiExplode_Timer = 15000;
        }
        else
            m_uiExplode_Timer -= uiDiff;

        if (m_uiSpell1_Timer < uiDiff)
        {
            /** Spell1 shall be cast on random target */
            if (Unit* pUnit = SelectTarget(SelectTargetMethod::Random, 0))
            {
                // VMaNGOS also sends a SpellGo for the visual (SendSpellGo); not available in TC.
                if (DoCast(pUnit, m_uiSpell1) == SPELL_CAST_OK)
                    m_uiSpell1_Timer = 15000;
            }
        }
        else
            m_uiSpell1_Timer -= uiDiff;

        if (m_uiSpell2_Timer < uiDiff)
        {
            if (DoCastVictim(m_uiSpell2) == SPELL_CAST_OK)
                m_uiSpell2_Timer = 15000;
        }
        else
            m_uiSpell2_Timer -= uiDiff;

        if (m_uiSummon_Timer < uiDiff)
        {
            if (m_uiSummonCount < 4)
            {
                me->SummonCreature(m_uiNPCSummon, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0,
                    TEMPSUMMON_TIMED_DESPAWN, 60s);
                // VMaNGOS: visual animation of the teleportation spell (SendSpellGo 25681); not available in TC.
            }
            m_uiSummon_Timer = 15000;
        }
        else
            m_uiSummon_Timer -= uiDiff;
    }
};

/*######
## mob_tornado_ossirian
######*/

struct classic_mob_tornado_ossirian : public ScriptedAI
{
    classic_mob_tornado_ossirian(Creature* creature) : ScriptedAI(creature)
    {
        SetCombatMovement(false);
    }

    void JustAppeared() override
    {
        ScriptedAI::JustAppeared();

        // VMaNGOS does this in the constructor
        me->CastSpell(me, SPELL_AQ20_TORNADO_SANDSTORM, false);
        me->CastSpell(me, SPELL_AQ20_TORNADO_AURA, false);
        me->SetDefaultMovementType(RANDOM_MOTION_TYPE);
        me->SetWanderDistance(55.0f);
        me->GetMotionMaster()->Initialize();
    }

    void Reset() override { }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
    }

    void UpdateAI(uint32 /*uiDiff*/) override
    {
        if (!UpdateVictim())
            return;
    }
};

/*######
## mob_flesh_hunter
######*/

struct classic_mob_flesh_hunter : public ScriptedAI
{
    classic_mob_flesh_hunter(Creature* creature) : ScriptedAI(creature) { }

    ObjectGuid m_uiConsumeVictim;

    uint32 m_uiPoisonBolt_Timer = 0;
    uint32 m_uiTrash_Timer = 0;
    uint32 m_uiConsume_Timer = 0;
    uint32 m_uiConsumeDamage_Timer = 0;

    bool m_bPlayerConsumed = false;
    bool m_bPlayerConsumedCharged = false;

    void Reset() override
    {
        m_uiPoisonBolt_Timer = 3000;
        m_uiTrash_Timer = 5000;
        m_uiConsume_Timer = 3000;
        m_uiConsumeDamage_Timer = 1000;

        m_uiConsumeVictim.Clear();
        m_bPlayerConsumed = false;
        m_bPlayerConsumedCharged = false;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
    }

    void KilledUnit(Unit* pWho) override
    {
        if (pWho->GetGUID() == m_uiConsumeVictim)
            DoCastSelf(SPELL_AQ20_FH_CONSUME_HEAL);
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!UpdateVictim())
            return;

        if (m_uiPoisonBolt_Timer < uiDiff)
        {
            if (Unit* pTarget = SelectTarget(SelectTargetMethod::Random, 0))
            {
                if (DoCast(pTarget, SPELL_AQ20_FH_POISON_BOLT) == SPELL_CAST_OK)
                    m_uiPoisonBolt_Timer = 3000;
            }
        }
        else
            m_uiPoisonBolt_Timer -= uiDiff;

        if (m_uiConsume_Timer < uiDiff)
        {
            if (Unit* pTarget = SelectTarget(SelectTargetMethod::MaxThreat, 0))
            {
                if (DoCast(pTarget, SPELL_AQ20_FH_CONSUME) == SPELL_CAST_OK)
                {
                    m_uiConsumeVictim = pTarget->GetGUID();
                    m_bPlayerConsumed = true;
                    m_uiConsume_Timer = 30000;
                }
            }
        }
        else
            m_uiConsume_Timer -= uiDiff;

        if (Unit* pConsumeTarget = ObjectAccessor::GetUnit(*me, m_uiConsumeVictim))
        {
            if (pConsumeTarget->HasAura(SPELL_AQ20_FH_CONSUME))
            {
                if (m_uiConsumeDamage_Timer < uiDiff)
                {
                    if (DoCast(pConsumeTarget, SPELL_AQ20_FH_CONSUME_DMG) == SPELL_CAST_OK)
                    {
                        me->GetMotionMaster()->Initialize();
                        me->StopMoving();
                        me->GetThreatManager().ResetThreat(pConsumeTarget);
                        m_uiConsumeDamage_Timer = 1000;
                        m_bPlayerConsumedCharged = true;
                        // VMaNGOS SetHealth(health - maxHealth / 10); clamped so the uint64 cannot wrap
                        uint64 const loss = pConsumeTarget->GetMaxHealth() / 10;
                        uint64 const health = pConsumeTarget->GetHealth();
                        pConsumeTarget->SetHealth(health > loss ? health - loss : 1);
                    }
                }
                else
                    m_uiConsumeDamage_Timer -= uiDiff;

                if (!pConsumeTarget->IsAlive())
                {
                    if (Unit* pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                        me->GetMotionMaster()->MoveChase(pTarget);
                    me->SetFullHealth();
                }
            }
            else
            {
                if (pConsumeTarget->IsAlive() && m_bPlayerConsumedCharged)
                {
                    if (DoCast(pConsumeTarget, SPELL_AQ20_FH_SPLIT) == SPELL_CAST_OK)
                    {
                        m_bPlayerConsumedCharged = false;
                        if (Unit* victim = me->GetVictim())
                            me->GetMotionMaster()->MoveChase(victim);
                    }
                }
            }
        }

        if (m_uiTrash_Timer < uiDiff)
        {
            if (DoCastVictim(SPELL_AQ20_FH_TRASH) == SPELL_CAST_OK)
                m_uiTrash_Timer = 5000 + urand(0, 1999);
        }
        else
            m_uiTrash_Timer -= uiDiff;
    }
};

/*######
## mob_obsidian_destroyer
######*/

struct classic_mob_obsidian_destroyer : public ScriptedAI
{
    classic_mob_obsidian_destroyer(Creature* creature) : ScriptedAI(creature) { }

    // VMaNGOS 46417908 turned this into EventAI: mana reset (23777) on spawn, on reaching home and on aggro,
    // Drain Mana every 6 to 7 seconds, Purge checked every second at full mana, Drop Obsidian on death.
    uint32 m_uiDrainMana_Timer = 0;
    uint32 m_uiPurge_Timer = 0;

    void Reset() override
    {
        m_uiDrainMana_Timer = urand(6000, 7000);
        m_uiPurge_Timer = 0;
    }

    void JustAppeared() override
    {
        ScriptedAI::JustAppeared();
        DoCastSelf(SPELL_AQ20_OD_RESET_MANA, true);
    }

    void JustReachedHome() override
    {
        DoCastSelf(SPELL_AQ20_OD_RESET_MANA, true);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoCastSelf(SPELL_AQ20_OD_RESET_MANA, true);
    }

    void JustDied(Unit* /*killer*/) override
    {
        // VMaNGOS casts Drop Obsidian (27627, summons 181068); summoned directly as before
        ClassicAQ20SummonObsidianChunk(me, GO_AQ20_SMALL_OBSIDIAN_CHUNK);
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!UpdateVictim())
            return;

        if (m_uiPurge_Timer < uiDiff)
        {
            if (me->GetPower(POWER_MANA) >= me->GetMaxPower(POWER_MANA))
                DoCastSelf(SPELL_AQ20_OD_PURGE);
            m_uiPurge_Timer = 1000;
        }
        else
            m_uiPurge_Timer -= uiDiff;

        if (m_uiDrainMana_Timer < uiDiff)
        {
            DoCastSelf(SPELL_AQ20_OD_DRAINMANA);
            m_uiDrainMana_Timer = urand(6000, 7000);
        }
        else
            m_uiDrainMana_Timer -= uiDiff;
    }
};

/*######
## mob_hive_zara_soldier
######*/

struct classic_mob_hive_zara_soldier : public ScriptedAI
{
    classic_mob_hive_zara_soldier(Creature* creature) : ScriptedAI(creature) { }

    uint32 m_uiVenomSpit_Timer = 0;
    bool m_bRetaliation = false;

    void Reset() override
    {
        m_uiVenomSpit_Timer = 5000;
        m_bRetaliation = false;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!UpdateVictim())
            return;

        if (m_uiVenomSpit_Timer < uiDiff)
        {
            if (Unit* pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                if (DoCast(pTarget, SPELL_AQ20_HZS_VENOM_SPIT) == SPELL_CAST_OK)
                    m_uiVenomSpit_Timer = urand(5000, 10000);
        }
        else
            m_uiVenomSpit_Timer -= uiDiff;

        if (me->GetHealthPct() < 20.0f && !m_bRetaliation)
        {
            me->CastSpell(me, SPELL_AQ20_HZS_RETALIATION, false);
            m_bRetaliation = true;
        }
    }
};

/*######
## mob_silicate_feeder
######*/

struct classic_mob_silicate_feeder : public ScriptedAI
{
    classic_mob_silicate_feeder(Creature* creature) : ScriptedAI(creature) { }

    bool m_bIsAttacked = false;

    void Reset() override
    {
        me->SetFaction(FACTION_AQ20_SF_PASSIVE);
        m_bIsAttacked = false;
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoCastSelf(SPELL_AQ20_SF_CLOUD_OF_DISEASE);
    }

    void UpdateAI(uint32 /*uiDiff*/) override
    {
        if (!UpdateVictim())
            return;

        if (!m_bIsAttacked)
        {
            me->SetFaction(FACTION_AQ20_SF_HOSTILE);
            DoZoneInCombat();
            m_bIsAttacked = true;
        }
    }
};

/*######
## mob_qiraji_swarmguard
######*/

struct classic_mob_qiraji_swarmguard : public ScriptedAI
{
    classic_mob_qiraji_swarmguard(Creature* creature) : ScriptedAI(creature) { }

    uint32 m_uiSunder_Timer = 0;

    void Reset() override
    {
        m_uiSunder_Timer = 2000;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (me->IsWalking())
            me->SetWalk(false);

        if (!UpdateVictim())
            return;

        if (m_uiSunder_Timer < uiDiff)
        {
            if (DoCastVictim(SPELL_AQ20_QS_SUNDERING_CLEAVE) == SPELL_CAST_OK)
                m_uiSunder_Timer = urand(8000, 12000);
        }
        else
            m_uiSunder_Timer -= uiDiff;
    }
};

/*######
## mob_qiraji_gladiator
######*/

struct classic_mob_qiraji_gladiator : public ScriptedAI
{
    classic_mob_qiraji_gladiator(Creature* creature) : ScriptedAI(creature), m_pInstance(creature->GetInstanceScript()) { }

    InstanceScript* m_pInstance;
    uint32 m_uiTrample_Timer = 0;
    uint32 m_uiUppercut_Timer = 0;
    bool m_bIsEnraged = false;

    void Reset() override
    {
        m_uiTrample_Timer = 4000;
        m_uiUppercut_Timer = 9000;
        m_bIsEnraged = false;
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_AQ20_TYPE_QIRAJI_GLADIATOR, 0);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_AQ20_TYPE_QIRAJI_GLADIATOR, 0);
        DoZoneInCombat();
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_AQ20_TYPE_QIRAJI_GLADIATOR, 1);
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!UpdateVictim())
            return;

        if (m_pInstance && m_pInstance->GetData(CLASSIC_AQ20_TYPE_QIRAJI_GLADIATOR) > 0 && !m_bIsEnraged)
        {
            if (DoCastSelf(SPELL_AQ20_QG_VENGEANCE) == SPELL_CAST_OK)
                m_bIsEnraged = true;
        }

        if (m_uiTrample_Timer < uiDiff)
        {
            if (DoCastVictim(SPELL_AQ20_QG_TRAMPLE) == SPELL_CAST_OK)
                m_uiTrample_Timer = urand(4000, 6000);
        }
        else
            m_uiTrample_Timer -= uiDiff;

        if (m_uiUppercut_Timer < uiDiff)
        {
            if (DoCastVictim(SPELL_AQ20_QG_UPPERCUT) == SPELL_CAST_OK)
                m_uiUppercut_Timer = urand(10000, 15000);
        }
        else
            m_uiUppercut_Timer -= uiDiff;
    }
};

/*######
## mob_hive_zara_stinger
######*/

struct classic_mob_hive_zara_stinger : public ScriptedAI
{
    classic_mob_hive_zara_stinger(Creature* creature) : ScriptedAI(creature) { }

    uint32 m_uiCharge_Timer = 0;
    uint32 m_uiChargeCasted_Timer = 0;
    bool m_bChargeCasted = false;

    void Reset() override
    {
        m_uiCharge_Timer = 3000;
        m_uiChargeCasted_Timer = 0;
        m_bChargeCasted = false;
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!UpdateVictim())
            return;

        if (m_uiCharge_Timer < uiDiff)
        {
            Unit* pTarget = SelectTarget(SelectTargetMethod::Random, 0);
            if (pTarget && DoCast(pTarget, SPELL_AQ20_HZST_CHARGE) == SPELL_CAST_OK)
            {
                m_uiCharge_Timer = 5000;
                m_bChargeCasted = true;
                m_uiChargeCasted_Timer = 500;
            }
        }
        else
        {
            m_uiCharge_Timer -= uiDiff;
            if (m_bChargeCasted)
            {
                m_uiChargeCasted_Timer -= uiDiff;
                if (m_uiChargeCasted_Timer < uiDiff)
                {
                    m_bChargeCasted = false;
                    if (Unit* victim = me->GetVictim())
                        me->GetMotionMaster()->MoveChase(victim);
                }
            }
        }
    }
};

/*######
## boss_tuubid
######*/

struct classic_boss_tuubid : public ScriptedAI
{
    classic_boss_tuubid(Creature* creature) : ScriptedAI(creature) { }

    uint32 m_uiAttackOrder_Timer = 0;
    uint32 m_uiCleave_Timer = 0;
    uint32 m_uiSunderArmor_Timer = 0;
    ObjectGuid m_uiMarkedGUID;

    void Reset() override
    {
        m_uiMarkedGUID.Clear();
        m_uiAttackOrder_Timer = 5000;
        m_uiCleave_Timer = 8000;
        m_uiSunderArmor_Timer = 6000;
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!UpdateVictim())
            return;

        if (m_uiAttackOrder_Timer < uiDiff)
        {
            if (Unit* pTarget = SelectTarget(SelectTargetMethod::Random, 0))
            {
                if (Unit* pOldMark = ObjectAccessor::GetUnit(*me, m_uiMarkedGUID))
                    pOldMark->RemoveAurasDueToSpell(SPELL_AQ20_TUUBID_ATTACK_ORDER);

                if (Player* pMark = pTarget->GetCharmerOrOwnerPlayerOrPlayerItself())
                {
                    DoCast(pMark, SPELL_AQ20_TUUBID_ATTACK_ORDER);
                    m_uiMarkedGUID = pMark->GetGUID();
                    // TODO(classic): VMaNGOS DoScriptText(SAY_TUUBID_KILL = -1900117, me, pMark); the text has no row in the
                    // VMaNGOS DB (script_texts) and no broadcast text id, so nothing is said.
                }
                else
                {
                    m_uiMarkedGUID.Clear();
                    TC_LOG_ERROR("scripts", "boss_tuubid could not accuire a new target to mark.");
                }
                m_uiAttackOrder_Timer = 9000;
            }
        }
        else
            m_uiAttackOrder_Timer -= uiDiff;

        if (m_uiCleave_Timer < uiDiff)
        {
            if (DoCastVictim(SPELL_AQ20_TUUBID_CLEAVE) == SPELL_CAST_OK)
                m_uiCleave_Timer = 10000;
        }
        else
            m_uiCleave_Timer -= uiDiff;

        if (m_uiSunderArmor_Timer < uiDiff)
        {
            if (DoCastSelf(SPELL_AQ20_TUUBID_SUNDER_ARMOR) == SPELL_CAST_OK)
                m_uiSunderArmor_Timer = 15000;
        }
        else
            m_uiSunderArmor_Timer -= uiDiff;
    }
};

namespace
{
// Shared "marking target system" of the Tuubid wave (Qiraji Warrior / Swarmguard Needler)
struct ClassicAQ20TuubidFollower
{
    ObjectGuid m_uiTuubidGuid;
    uint32 m_uiUpdateTarget_Timer = 2000;
    bool m_bisTuubidAlive = true;

    void ResetFollower()
    {
        m_uiTuubidGuid.Clear();
        m_uiUpdateTarget_Timer = 2000;
        m_bisTuubidAlive = true;
    }

    classic_boss_tuubid* GetTuubidAI(Creature* me)
    {
        if (m_uiTuubidGuid.IsEmpty())
            if (Creature* pTuubid = me->FindNearestCreature(CLASSIC_AQ20_NPC_CAPTAIN_TUUBID, 100.0f, true))
                m_uiTuubidGuid = pTuubid->GetGUID();

        if (Creature* pTuubid = me->GetMap()->GetCreature(m_uiTuubidGuid))
            if (pTuubid->IsAlive() && pTuubid->IsAIEnabled())
                return dynamic_cast<classic_boss_tuubid*>(pTuubid->AI());
        return nullptr;
    }

    // Returns false when UpdateAI must stop (VMaNGOS: SelectHostileTarget()/GetVictim() checks)
    template<class AI>
    bool UpdateFollowerVictim(AI* ai, Creature* me)
    {
        // VMaNGOS returns (does nothing) without a creature group ("should not happen").
        // TODO(classic): the VMaNGOS creature_groups of the Rajaxx waves (leaders 301058 Tuubid, 301169 Yeggeth, ...) are not
        // in the TC world DB (creature_formations); without them the followers use normal threat targeting here.
        bool const followsTuubid = ClassicAQ20TrashFormationLeaderEntry(me) == CLASSIC_AQ20_NPC_CAPTAIN_TUUBID && m_bisTuubidAlive;
        if (!followsTuubid || !me->GetVictim())
        {
            // Threat calculation
            if (!ai->UpdateVictimPublic())
                return false;
        }
        return me->GetVictim() != nullptr;
    }

    void UpdateMarkedTarget(ScriptedAI* ai, Creature* me, uint32 uiDiff)
    {
        if (m_uiUpdateTarget_Timer < uiDiff)
        {
            if (classic_boss_tuubid* pTuubidAI = GetTuubidAI(me))
            {
                if (Unit* victim = ObjectAccessor::GetUnit(*me, pTuubidAI->m_uiMarkedGUID))
                    ai->AttackStart(victim);
            }
            else
            {
                /** Means that Tuubid is down, creature update their target list */
                m_bisTuubidAlive = false;
            }
            m_uiUpdateTarget_Timer = 1400;
        }
        else
            m_uiUpdateTarget_Timer -= uiDiff;
    }
};
}

/*######
## mob_qiraji_warrior
######*/

struct classic_mob_qiraji_warrior : public ScriptedAI
{
    classic_mob_qiraji_warrior(Creature* creature) : ScriptedAI(creature) { }

    ClassicAQ20TuubidFollower m_follower;
    uint32 m_uiThunderclap_Timer = 0;
    uint32 m_uiUppercut_Timer = 0;
    bool m_bHasEnraged = false;

    bool UpdateVictimPublic() { return UpdateVictim(); }

    void Reset() override
    {
        m_uiThunderclap_Timer = urand(6000, 12000);
        m_uiUppercut_Timer = urand(10000, 15000);
        m_follower.ResetFollower();
        m_bHasEnraged = false;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
    }

    void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (!m_bHasEnraged && ((me->GetHealth() * 100) / me->GetMaxHealth()) <= 20 && !me->IsNonMeleeSpellCast(false))
        {
            if (Unit* victim = me->GetVictim())
                DoCast(victim, SPELL_AQ20_QW_ENRAGE);
            m_bHasEnraged = true;
        }
    }

    void UpdateAI(uint32 uiDiff) override
    {
        /** Needed for "marking target system" */
        if (!m_follower.UpdateFollowerVictim(this, me))
            return;

        m_follower.UpdateMarkedTarget(this, me, uiDiff);

        if (m_uiThunderclap_Timer < uiDiff)
        {
            // VMaNGOS checks GetDistance2d(m_creature) (distance to itself, always < 5)
            if (DoCastVictim(SPELL_AQ20_QW_THUNDERCLAP) == SPELL_CAST_OK)
                m_uiThunderclap_Timer = 6000;
        }
        else
            m_uiThunderclap_Timer -= uiDiff;

        if (m_uiUppercut_Timer < uiDiff)
        {
            if (DoCastVictim(SPELL_AQ20_QW_UPPERCUT) == SPELL_CAST_OK)
                m_uiUppercut_Timer = 10000;
        }
        else
            m_uiUppercut_Timer -= uiDiff;
    }
};

/*######
## mob_swarmguard_needler
######*/

struct classic_mob_swarmguard_needler : public ScriptedAI
{
    classic_mob_swarmguard_needler(Creature* creature) : ScriptedAI(creature) { }

    ClassicAQ20TuubidFollower m_follower;
    uint32 m_uiCleave_Timer = 0;

    bool UpdateVictimPublic() { return UpdateVictim(); }

    void Reset() override
    {
        m_follower.ResetFollower();
        m_uiCleave_Timer = urand(6000, 12000);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
    }

    void UpdateAI(uint32 uiDiff) override
    {
        /** Needed for "marking target system" */
        if (!m_follower.UpdateFollowerVictim(this, me))
            return;

        m_follower.UpdateMarkedTarget(this, me, uiDiff);

        if (m_uiCleave_Timer < uiDiff)
        {
            if (DoCastVictim(SPELL_AQ20_SN_CLEAVE) == SPELL_CAST_OK)
                m_uiCleave_Timer = 8000;
        }
        else
            m_uiCleave_Timer -= uiDiff;
    }
};

/*######
## npc_andorov (Lieutenant General Andorov)
## VMaNGOS creature_spells 154710, gossip_scripts 6629, creature_movement_template / creature_movement_special 15471 (the walks and the
## wave event are driven by classic_instance_ruins_of_ahnqiraj, which owns the timeline)
######*/

struct classic_npc_andorov : public ScriptedAI
{
    classic_npc_andorov(Creature* creature) : ScriptedAI(creature), m_pInstance(creature->GetInstanceScript()) { }

    InstanceScript* m_pInstance;
    uint32 m_uiStrike_Timer = 0;
    uint32 m_uiBash_Timer = 0;
    uint32 m_uiAuraOfCommand_Timer = 0;

    void Reset() override
    {
        m_uiStrike_Timer = urand(9000, 13000);
        m_uiBash_Timer = urand(25000, 29000);
        m_uiAuraOfCommand_Timer = urand(9000, 13000);
    }

    // gossip_scripts 6629: opening the NOT_STARTED menu starts generic_scripts 154710 (there is no gossip_menu_option for 6629)
    bool OnGossipHello(Player* /*player*/) override
    {
        if (m_pInstance && me->GetGossipMenuId() == CLASSIC_AQ20_ANDOROV_GOSSIP_NOT_STARTED)
            m_pInstance->SetData(CLASSIC_AQ20_TYPE_GENERAL_ANDOROV, IN_PROGRESS);
        return false;   // the menu itself (and the vendor after the event) is still shown
    }

    // Every point of the camp / valley walks is ordered by the instance script; it continues the walk on arrival
    void MovementInform(uint32 uiType, uint32 uiPointId) override
    {
        if (uiType != POINT_MOTION_TYPE || !m_pInstance)
            return;
        m_pInstance->SetData(CLASSIC_AQ20_DATA_ANDOROV_POINT, uiPointId);
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!UpdateVictim())
            return;

        if (m_uiStrike_Timer < uiDiff)
        {
            if (DoCastVictim(CLASSIC_AQ20_SPELL_ANDOROV_STRIKE) == SPELL_CAST_OK)
                m_uiStrike_Timer = urand(10000, 14000);
        }
        else
            m_uiStrike_Timer -= uiDiff;

        if (m_uiBash_Timer < uiDiff)
        {
            if (DoCastVictim(CLASSIC_AQ20_SPELL_ANDOROV_BASH) == SPELL_CAST_OK)
                m_uiBash_Timer = urand(25000, 45000);
        }
        else
            m_uiBash_Timer -= uiDiff;

        if (m_uiAuraOfCommand_Timer < uiDiff)
        {
            if (DoCastSelf(CLASSIC_AQ20_SPELL_AURA_OF_COMMAND) == SPELL_CAST_OK)
                m_uiAuraOfCommand_Timer = urand(30000, 32000);
        }
        else
            m_uiAuraOfCommand_Timer -= uiDiff;
    }
};

/*######
## npc_kaldorei_elite
## VMaNGOS creature_spells 154730
######*/

struct classic_npc_kaldorei_elite : public ScriptedAI
{
    classic_npc_kaldorei_elite(Creature* creature) : ScriptedAI(creature) { }

    uint32 m_uiMortalStrike_Timer = 0;
    uint32 m_uiCleave_Timer = 0;

    void Reset() override
    {
        m_uiMortalStrike_Timer = urand(5000, 15000);
        m_uiCleave_Timer = urand(10000, 30000);
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!UpdateVictim())
            return;

        if (m_uiMortalStrike_Timer < uiDiff)
        {
            if (DoCastVictim(CLASSIC_AQ20_SPELL_ELITE_MORTAL_STRIKE) == SPELL_CAST_OK)
                m_uiMortalStrike_Timer = urand(5000, 15000);
        }
        else
            m_uiMortalStrike_Timer -= uiDiff;

        if (m_uiCleave_Timer < uiDiff)
        {
            if (DoCastVictim(CLASSIC_AQ20_SPELL_ELITE_CLEAVE) == SPELL_CAST_OK)
                m_uiCleave_Timer = urand(10000, 30000);
        }
        else
            m_uiCleave_Timer -= uiDiff;
    }
};

// 25676 - Drain Mana (Moam)
// 25754 - Drain Mana (Obsidian Destroyer)
// VMaNGOS: max 6 targets, skip targets without mana. The Classic 1.60 client data has EFFECT_0 = SCRIPT_EFFECT
// (TARGET_UNIT_SRC_AREA_ENEMY) instead of VMaNGOS' power drain, so the drain effect spell is cast on each hit target.
class classic_spell_aq20_drain_mana : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_AQ20_DRAIN_MANA_MOAM_EFFECT, SPELL_AQ20_DRAIN_MANA_DESTROYER_EFFECT });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        // Avoid targeting players with no mana
        targets.remove_if([](WorldObject* target)
        {
            Unit* unit = target->ToUnit();
            return !unit || unit->GetPowerType() != POWER_MANA || unit->GetPowerPct(POWER_MANA) < 1.0f;
        });
        Trinity::Containers::RandomResize(targets, 6);
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        uint32 const effectSpell = GetSpellInfo()->Id == SPELL_AQ20_DRAIN_MANA_MOAM ? SPELL_AQ20_DRAIN_MANA_MOAM_EFFECT : SPELL_AQ20_DRAIN_MANA_DESTROYER_EFFECT;
        GetCaster()->CastSpell(GetHitUnit(), effectSpell, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(classic_spell_aq20_drain_mana::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(classic_spell_aq20_drain_mana::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 25599 - Thundercrash (General Rajaxx)
// percent from health with min
class classic_spell_rajaxx_thundercrash : public SpellScript
{
    static void HandleDamageCalc(SpellScript const&, SpellEffectInfo const& /*spellEffectInfo*/, Unit const* victim, int32& damage, int32& /*flatMod*/, float& /*pctMod*/)
    {
        int32 newDamage = int32(victim->GetHealth() / 2);
        if (newDamage < 200)
            newDamage = 200;
        damage = newDamage;
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_rajaxx_thundercrash::HandleDamageCalc);
    }
};

void AddSC_classic_ruins_of_ahnqiraj()
{
    RegisterCreatureAI(classic_mob_anubisath_guardian);
    RegisterCreatureAI(classic_mob_qiraji_swarmguard);
    RegisterCreatureAI(classic_mob_flesh_hunter);
    RegisterCreatureAI(classic_mob_tornado_ossirian);
    RegisterCreatureAI(classic_mob_qiraji_gladiator);
    RegisterCreatureAI(classic_mob_silicate_feeder);
    RegisterCreatureAI(classic_mob_hive_zara_soldier);
    RegisterCreatureAI(classic_mob_obsidian_destroyer);
    RegisterCreatureAI(classic_mob_hive_zara_stinger);
    RegisterCreatureAI(classic_mob_qiraji_warrior);
    RegisterCreatureAI(classic_mob_swarmguard_needler);
    RegisterCreatureAI(classic_boss_tuubid);
    RegisterCreatureAI(classic_npc_andorov);
    RegisterCreatureAI(classic_npc_kaldorei_elite);
    RegisterSpellScript(classic_spell_aq20_drain_mana);
    RegisterSpellScript(classic_spell_rajaxx_thundercrash);
}
