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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/burning_steppes/molten_core/molten_core.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: mob_firewalker, mob_ancient_core_hound, mob_core_hound, mob_firelord, mob_lava_surger

#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "ThreatManager.h"
#include "World.h"
#include "classic_molten_core.h"
#include "classic_script_text.h"
#include <list>

/*######
## mob_firewalker
######*/

namespace
{
enum ClassicMcFirewalker : uint32
{
    SPELL_MC_FIREBLOSSOM         = 19637,
    SPELL_MC_FIREBLOSSOM_CASTING = 19636,
    SPELL_MC_INCITE_FLAMES       = 19635
};
}

struct classic_mob_firewalker : public ScriptedAI
{
    classic_mob_firewalker(Creature* creature) : ScriptedAI(creature), m_pInstance(creature->GetInstanceScript()) { }

    InstanceScript* m_pInstance;
    uint32 m_uiFireBlossomCasting_Timer = 6000;
    uint32 m_uiFireBlossom_Timer = 0;
    uint32 m_uiFireBlossomPreparing_Timer = 0;
    uint32 m_uiInciteFlames_Timer = 20000;
    uint32 m_uiNbBlossom = 0;

    void Reset() override
    {
        m_uiFireBlossomCasting_Timer   =  6000;
        m_uiFireBlossom_Timer          =     0;
        m_uiFireBlossomPreparing_Timer =     0;
        m_uiInciteFlames_Timer         = 20000;
        m_uiNbBlossom                  =     0;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!UpdateVictim())
            return;

        if (m_uiFireBlossomCasting_Timer < uiDiff)
        {
            if (DoCastSelf(SPELL_MC_FIREBLOSSOM_CASTING) == SPELL_CAST_OK)
                m_uiFireBlossomCasting_Timer = 12000;

            m_uiNbBlossom = 6;
            m_uiFireBlossomPreparing_Timer = 1000;
        }
        else
            m_uiFireBlossomCasting_Timer -= uiDiff;

        if (m_uiInciteFlames_Timer < uiDiff)
        {
            if (DoCastVictim(SPELL_MC_INCITE_FLAMES) == SPELL_CAST_OK)
                m_uiInciteFlames_Timer = 20000;
        }
        else
            m_uiInciteFlames_Timer -= uiDiff;

        if (m_uiFireBlossomPreparing_Timer < uiDiff)
        {
            if (m_uiNbBlossom > 0)
            {
                // The whole raid is on the threat list (zone combat), so a plain random pick often chose someone out of range or behind a
                // pillar: the cast failed and that blossom was lost. Pick only targets the blossom can reach.
                SpellInfo const* blossomInfo = sSpellMgr->GetSpellInfo(SPELL_MC_FIREBLOSSOM, me->GetMap()->GetDifficultyID());
                float const blossomRange = blossomInfo ? blossomInfo->GetMaxRange(false, me) : 0.0f;
                auto const canReach = [this, blossomRange](Unit* target)
                {
                    return (blossomRange <= 0.0f || me->IsWithinDistInMap(target, blossomRange)) && me->IsWithinLOSInMap(target);
                };
                if (Unit* pUnit = SelectTarget(SelectTargetMethod::Random, 0, canReach))
                {
                    SpellCastResult const result = DoCast(pUnit, SPELL_MC_FIREBLOSSOM);
                    // Off by default; Logger.scripts.classic.firewalker=3,Console Server shows every blossom and the targets it chose from.
                    if (Log::instance()->ShouldLog("scripts.classic.firewalker", LOG_LEVEL_INFO))
                    {
                        uint32 reachable = 0;
                        uint32 total = 0;
                        for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
                        {
                            ++total;
                            if (canReach(ref->GetVictim()))
                                ++reachable;
                        }
                        TC_LOG_INFO("scripts.classic.firewalker", "Firewalker {}: Fire Blossom on {} at {:.1f} yd (range {:.1f}), result {}, {} of {} on the threat list in range and sight",
                            me->GetGUID().ToString(), pUnit->GetName(), me->GetDistance(pUnit), blossomRange, uint32(result), reachable, total);
                    }
                }

                m_uiFireBlossomPreparing_Timer = 1000;
                m_uiNbBlossom = m_uiNbBlossom - 1;
            }
        }
        else
            m_uiFireBlossomPreparing_Timer -= uiDiff;
    }
};

/*######
## mob_ancient_core_hound
######*/

namespace
{
enum ClassicMcAncientCoreHound : uint32
{
    SPELL_MC_CONE_OF_FIRE          = 19630,
    SPELL_MC_VICIOUS_BITE          = 19319,
    SPELL_MC_BITE                  = 19771,

    // Random Debuff (each hound has only one of these)
    SPELL_MC_GROUND_STOMP          = 19364,
    SPELL_MC_ANCIENT_DREAD         = 19365,
    SPELL_MC_CAUTERIZING_FLAMES    = 19366,
    SPELL_MC_WITHERING_HEAT        = 19367,
    SPELL_MC_ANCIENT_DESPAIR       = 19369,
    SPELL_MC_ANCIENT_HYSTERIA      = 19372
};
}

struct classic_mob_ancient_core_hound : public ScriptedAI
{
    classic_mob_ancient_core_hound(Creature* creature) : ScriptedAI(creature), m_pInstance(creature->GetInstanceScript()) { }

    uint32 m_uiConeOfFireTimer = 0;
    uint32 m_uiRandomDebuffTimer = 0;
    uint32 m_uiBiteTimer = 0;
    uint32 RandDebuff = SPELL_MC_GROUND_STOMP;

    InstanceScript* m_pInstance;

    void Reset() override
    {
        switch (urand(0, 5))
        {
            case 0: RandDebuff = SPELL_MC_GROUND_STOMP; break;
            case 1: RandDebuff = SPELL_MC_ANCIENT_DREAD; break;
            case 2: RandDebuff = SPELL_MC_CAUTERIZING_FLAMES; break;
            case 3: RandDebuff = SPELL_MC_WITHERING_HEAT; break;
            case 4: RandDebuff = SPELL_MC_ANCIENT_DESPAIR; break;
            case 5: RandDebuff = SPELL_MC_ANCIENT_HYSTERIA; break;
        }
        m_uiConeOfFireTimer   = urand(4000, 7000);
        m_uiRandomDebuffTimer = urand(12000, 15000);
        m_uiBiteTimer         = 4000;

        me->SetNoCallAssistance(true);
        // VMaNGOS never calls DoMeleeAttackIfReady: every swing is replaced by Vicious Bite (see UpdateAI)
        me->SetCanMelee(false);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (m_pInstance && m_pInstance->GetData(CLASSIC_MC_TYPE_MAGMADAR) == DONE)
        {
            me->SetRespawnTime(7 * DAY);
            me->SaveRespawnTime();
        }
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!UpdateVictim())
            return;

        if (m_uiConeOfFireTimer < uiDiff)
        {
            me->CastSpell(me, SPELL_MC_CONE_OF_FIRE, false);
            m_uiConeOfFireTimer = urand(6000, 8000);
        }
        else
            m_uiConeOfFireTimer -= uiDiff;

        if (m_uiRandomDebuffTimer < uiDiff)
        {
            if (DoCastSelf(RandDebuff) == SPELL_CAST_OK)
                m_uiRandomDebuffTimer = urand(14000, 24000);
        }
        else
            m_uiRandomDebuffTimer -= uiDiff;

        Unit* victim = me->GetVictim();
        if (!victim)
            return;

        if (m_uiBiteTimer < uiDiff)
        {
            if (me->IsWithinMeleeRange(victim))
            {
                me->CastSpell(victim, SPELL_MC_BITE, false);
                m_uiBiteTimer = 6000;
            }
        }
        else
            m_uiBiteTimer -= uiDiff;

        if (me->isAttackReady())
        {
            // If we are within range melee the target
            if (me->IsWithinMeleeRange(victim))
            {
                me->CastSpell(victim, SPELL_MC_VICIOUS_BITE, true);
                me->resetAttackTimer();
            }
        }
    }
};

/*######
## mob_core_hound
######*/

namespace
{
enum ClassicMcCoreHound : uint32
{
    SPELL_MC_FULL_HEAL         = 17683,
    SPELL_MC_SERRATED_BITE     = 19771,
    SPELL_MC_FIRE_NOVA_VISUAL  = 19823,
    SPELL_MC_PACIFY_SELF       = 19951,

    BCT_MC_FAKE_DEATH          = 7866,
    BCT_MC_REVIVE              = 7867
};
}

struct classic_mob_core_hound : public ScriptedAI
{
    classic_mob_core_hound(Creature* creature) : ScriptedAI(creature), m_pInstance(creature->GetInstanceScript()) { }

    InstanceScript* m_pInstance;

    uint32 m_uiSerratedBiteTimer = 0;
    uint32 m_uiResurrectTimer = 10000;

    bool m_bDead = false;
    bool m_bAllowDeath = false; // VMaNGOS invincibility hp threshold (removed in Kill_Self)

    void ResurrectSelf()
    {
        me->RemoveAurasDueToSpell(SPELL_MC_PACIFY_SELF);
        me->CastSpell(me, SPELL_MC_FULL_HEAL, true);
        me->SetStandState(UNIT_STAND_STATE_STAND);
        me->AttackStop();
        me->SetCanMelee(true);
        SetCombatMovement(true);

        m_bDead = false;
    }

    void FeignDeath()
    {
        if (!me->GetVictim())
            return;

        ClassicScriptText(BCT_MC_FAKE_DEATH, me);
        me->SetHealth(1);
        me->RemoveAllAuras();
        me->CastSpell(me, SPELL_MC_PACIFY_SELF, true);
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveIdle();
        me->SetStandState(UNIT_STAND_STATE_DEAD); // UNIT_DYNFLAG_DEAD does not exist in TC master
        me->SetCanMelee(false);
        SetCombatMovement(false);

        m_uiResurrectTimer = 10000;
        m_bDead = true;
    }

    void Kill_Self()
    {
        m_bAllowDeath = true;
        me->KillSelf();
        // VMaNGOS ForcedDespawn()s right after the kill (corpse removed immediately; Core Hounds have no loot)
        me->DespawnOrUnsummon();
    }

    void Reset() override
    {
        m_uiSerratedBiteTimer = urand(4000, 7000);
        m_uiResurrectTimer = 10000;
        m_bAllowDeath = false;

        ResurrectSelf();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (m_bAllowDeath)
            return;

        // Instance.MoltenCoreHoundsLinked = 0 (default): the hound just dies, a pack can be killed one by one
        if (!sWorld->getBoolConfig(CONFIG_MC_CORE_HOUNDS_LINKED))
            return;

        if (damage >= me->GetHealth())
        {
            // time to fake death (VMaNGOS keeps the hound at 1 hp through its invincibility threshold)
            damage = 0;
            if (!m_bDead)
                FeignDeath();
            if (me->GetHealth() > 1 && m_bDead)
                me->SetHealth(1);
        }
    }

    void UpdateAI(uint32 uiDiff) override
    {
        // Return since we have no target
        if (!UpdateVictim() || !m_pInstance)
            return;

        // Resurrect pack if not died during 10 seconds
        if (m_bDead)
        {
            if (m_uiResurrectTimer < uiDiff)
            {
                bool resurrectionOkay = false;

                std::list<Creature*> coreHoundList;
                me->GetCreatureListWithEntryInGrid(coreHoundList, CLASSIC_MC_NPC_CORE_HOUND, 100.0f);
                for (Creature* hound : coreHoundList)
                {
                    if (hound && hound->IsInCombat())
                    {
                        if (hound->GetHealth() > 1)
                            resurrectionOkay = true;
                    }
                }

                if (resurrectionOkay)
                {
                    ResurrectSelf();
                    DoCastSelf(SPELL_MC_FIRE_NOVA_VISUAL, true);
                    ClassicScriptText(BCT_MC_REVIVE, me);
                }
                else
                    Kill_Self();
            }
            else
                m_uiResurrectTimer -= uiDiff;

            return;
        }

        // Serrated Bite
        if (m_uiSerratedBiteTimer < uiDiff)
        {
            if (DoCastVictim(SPELL_MC_SERRATED_BITE) == SPELL_CAST_OK)
                m_uiSerratedBiteTimer = urand(4000, 7000);
        }
        else
            m_uiSerratedBiteTimer -= uiDiff;

        // melee: TC master auto-melee (disabled via SetCanMelee(false) while feigning death)
    }
};

/*######
## mob_firelord
######*/

namespace
{
enum ClassicMcFirelord : uint32
{
    SPELL_MC_INCINERATE_AURA   = 19396,
    SPELL_MC_INCINERATE        = 19397,
    SPELL_MC_LAVASPAWN         = 19569,
    SPELL_MC_SOULBURN          = 19393
};
}

struct classic_mob_firelord : public ScriptedAI
{
    classic_mob_firelord(Creature* creature) : ScriptedAI(creature), m_pInstance(creature->GetInstanceScript()) { }

    InstanceScript* m_pInstance;

    uint32 m_uiSummonLavaSpawnTimer = 0;
    uint32 m_uiSoulBurnTimer = 0;

    void Reset() override
    {
        m_uiSummonLavaSpawnTimer  = urand(7500, 12500);
        m_uiSoulBurnTimer         = urand(4000, 6000);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (!me->HasAura(SPELL_MC_INCINERATE_AURA))
            DoCastSelf(SPELL_MC_INCINERATE_AURA, true);
    }

    void JustSummoned(Creature* summoned) override
    {
        if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
            summoned->AI()->AttackStart(target);
    }

    void UpdateAI(uint32 uiDiff) override
    {
        // Return since we have no target
        if (!UpdateVictim())
            return;

        // Summon Lava Spawn
        if (m_uiSummonLavaSpawnTimer < uiDiff)
        {
            if (DoCastSelf(SPELL_MC_LAVASPAWN) == SPELL_CAST_OK)
                m_uiSummonLavaSpawnTimer = urand(15000, 20000);
        }
        else
            m_uiSummonLavaSpawnTimer -= uiDiff;

        // Soul Burn
        if (m_uiSoulBurnTimer < uiDiff)
        {
            if (Unit* pTarget = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true))
            {
                if (DoCast(pTarget, SPELL_MC_SOULBURN, true) == SPELL_CAST_OK)
                    m_uiSoulBurnTimer = urand(3000, 4000);
            }
        }
        else
            m_uiSoulBurnTimer -= uiDiff;
    }
};

/*######
## mob_lava_surger
######*/

namespace
{
enum ClassicMcLavaSurger : uint32
{
    SPELL_MC_SURGE            = 19196
};
}

struct classic_mob_lava_surger : public ScriptedAI
{
    classic_mob_lava_surger(Creature* creature) : ScriptedAI(creature), m_pInstance(creature->GetInstanceScript()) { }

    uint32 m_uiSurgeTimer = 0;
    InstanceScript* m_pInstance;

    void Reset() override
    {
        m_uiSurgeTimer = urand(1000, 2000);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (m_pInstance && m_pInstance->GetData(CLASSIC_MC_TYPE_GARR) == DONE)
        {
            me->SetRespawnTime(7 * DAY);
            me->SaveRespawnTime();
        }
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!UpdateVictim())
            return;

        if (me->HasAuraType(SPELL_AURA_MOD_STUN))     // don't update CDs while Banished
            return;

        // Surge Timer
        if (m_uiSurgeTimer < uiDiff)
        {
            // VMaNGOS: ATTACKING_TARGET_FARTHEST, spell range of Surge, SELECT_FLAG_PLAYER | SELECT_FLAG_IN_LOS
            SpellInfo const* surgeInfo = sSpellMgr->GetSpellInfo(SPELL_MC_SURGE, me->GetMap()->GetDifficultyID());
            float const maxRange = surgeInfo ? surgeInfo->GetMaxRange(false, me) : 0.0f;
            Creature* self = me;
            if (Unit* pTarget = SelectTarget(SelectTargetMethod::MaxDistance, 0, [self, maxRange](Unit* target)
                {
                    return target->GetTypeId() == TYPEID_PLAYER && self->IsWithinLOSInMap(target)
                        && (maxRange <= 0.0f || self->IsWithinDistInMap(target, maxRange));
                }))
            {
                if (me->GetDistance2d(pTarget) > 7.0f)
                {
                    if (DoCast(pTarget, SPELL_MC_SURGE) == SPELL_CAST_OK)
                        m_uiSurgeTimer = urand(5000, 6000);
                }
            }
        }
        else
            m_uiSurgeTimer -= uiDiff;
    }
};

void AddSC_classic_molten_core()
{
    RegisterCreatureAI(classic_mob_firewalker);
    RegisterCreatureAI(classic_mob_ancient_core_hound);
    RegisterCreatureAI(classic_mob_core_hound);
    RegisterCreatureAI(classic_mob_lava_surger);
    RegisterCreatureAI(classic_mob_firelord);
}
