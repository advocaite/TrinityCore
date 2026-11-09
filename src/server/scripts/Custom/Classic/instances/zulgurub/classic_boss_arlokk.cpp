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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/stranglethorn_vale/zulgurub/boss_arlokk.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: go_gong_of_bethekk, boss_arlokk, mob_prowler

#include "ScriptMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "TemporarySummon.h"
#include "ThreatManager.h"
#include "classic_script_text.h"
#include "classic_zulgurub.h"
#include <cmath>

namespace
{
enum ClassicZgArlokk : uint32
{
    ARLOKK_DISABLE_TIMER            = 1000000,

    SAY_ARLOKK_AGGRO                = 10461,
    SAY_ARLOKK_FEAST_PANTHER        = 10472,
    SAY_ARLOKK_DEATH                = 10450,

    SPELL_ARLOKK_SHADOWWORDPAIN     = 24212,
    SPELL_ARLOKK_GOUGE              = 12540,
    SPELL_ARLOKK_MARK               = 24210,
    SPELL_ARLOKK_CLEAVE             = 26350,    // Perhaps not right. Not a red aura... (unused)
    SPELL_ARLOKK_PANTHER_TRANSFORM  = 24190,
    SPELL_ARLOKK_BACKSTAB           = 15582,
    SPELL_ARLOKK_TOURBILLON         = 15589,
    SPELL_ARLOKK_ATTAQUE_MENTALE    = 15587,    // unused
    SPELL_ARLOKK_ROSSER             = 3391,     // Thrash
    SPELL_ARLOKK_RAVAGE             = 24213,

    SPELL_PROWLER_SNEAK             = 22766,

    NPC_ARLOKK_ZULIAN_PROWLER       = 15101,
    ARLOKK_MAX_PANTHER_COUNT        = 30,

    GO_ARLOKK_FORCE_FIELD           = 180497,
    GO_ARLOKK_GONG                  = 180526
};

// VMaNGOS ScriptedAI::EnterVanish / LeaveVanish / Ambush
void ClassicZgArlokkEnterVanish(Creature* me)
{
    me->SetVisible(false);
    me->SetUnitFlag(UnitFlags(UNIT_FLAG_UNINTERACTIBLE | UNIT_FLAG_NON_ATTACKABLE));
    // TODO(classic): VMaNGOS also InterruptSpellsCastedOnMe(true) - no TC equivalent
    me->RemoveAllAttackers();
    me->AttackStop();
    me->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    me->SetCanMelee(false);     // VMaNGOS does not call DoMeleeAttackIfReady() while vanished
    me->GetThreatManager().ResetAllThreat();
}

void ClassicZgArlokkLeaveVanish(Creature* me)
{
    me->SetVisible(true);
    me->RemoveUnitFlag(UnitFlags(UNIT_FLAG_UNINTERACTIBLE | UNIT_FLAG_NON_ATTACKABLE));
    me->SetCanMelee(true);
}

void ClassicZgArlokkAmbush(Creature* me, Unit* pNewVictim, uint32 embushSpellId)
{
    if (!pNewVictim)
        return;
    // Se position derriere
    float o = pNewVictim->GetOrientation();
    float x = pNewVictim->GetPositionX() - 4.0f * std::cos(o);
    float y = pNewVictim->GetPositionY() - 4.0f * std::sin(o);
    float z = pNewVictim->GetPositionZ();
    me->NearTeleportTo(x, y, z, 0.0f);
    me->SetFacingToObject(pNewVictim);
    // Embush
    if (embushSpellId)
        me->CastSpell(pNewVictim, embushSpellId, true);
    if (me->AI())
        me->AI()->AttackStart(pNewVictim);
}

// VMaNGOS SetBaseWeaponDamage(default + pct) / ResetStats()
void ClassicZgArlokkSetDamagePct(Creature* me, float pct)
{
    me->SetStatPctModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, pct);
    me->UpdateDamagePhysical(BASE_ATTACK);
}
}

/*######
## go_gong_of_bethekk
######*/

// VMaNGOS summons Arlokk from the world database: event_scripts 9066 (the gong's goober event)
// (9066, delay 10, command 10 SUMMON_CREATURE, 14515, 900000 ms, ..., -11540.7, -1627.71, 41.27, 0.1): ten seconds after
// the gong she appears at the spot below, timed-or-dead despawn after 15 minutes.
Position const ArlokkSummonPos = { -11540.7f, -1627.71f, 41.27f, 0.1f };
constexpr uint32 ArlokkSummonDelayMs = 10 * IN_MILLISECONDS;
constexpr Milliseconds ArlokkSummonDespawn = 900000ms;

struct classic_go_gong_of_bethekk : public GameObjectAI
{
    classic_go_gong_of_bethekk(GameObject* go) : GameObjectAI(go) { }

    uint32 m_summonTimer = 0;

    bool OnGossipHello(Player* /*player*/) override
    {
        InstanceScript* pInstance = me->GetInstanceScript();
        if (!pInstance)
            return true;

        if (pInstance->GetData(CLASSIC_ZG_TYPE_ARLOKK) == DONE || pInstance->GetData(CLASSIC_ZG_TYPE_ARLOKK) == IN_PROGRESS)
            return true;

        pInstance->SetData(CLASSIC_ZG_TYPE_ARLOKK, IN_PROGRESS);
        m_summonTimer = ArlokkSummonDelayMs;

        // default goober use: the gong rings; event 9066 is the timer above
        return false;
    }

    void UpdateAI(uint32 diff) override
    {
        if (!m_summonTimer)
            return;
        if (m_summonTimer > diff)
        {
            m_summonTimer -= diff;
            return;
        }
        m_summonTimer = 0;

        // Her script despawns her on evade and respawns the gong, so a wipe can ring it again.
        if (!me->SummonCreature(CLASSIC_ZG_NPC_ARLOKK, ArlokkSummonPos, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, ArlokkSummonDespawn))
            TC_LOG_ERROR("scripts", "classic_go_gong_of_bethekk: could not summon High Priestess Arlokk ({}) in map {}", uint32(CLASSIC_ZG_NPC_ARLOKK), me->GetMapId());
    }
};

/*######
## boss_arlokk
######*/

struct classic_boss_arlokk : public ScriptedAI
{
    classic_boss_arlokk(Creature* creature) : ScriptedAI(creature), m_pInstance(creature->GetInstanceScript()) { }

    InstanceScript* m_pInstance;

    uint32 m_uiShadowWordPain_Timer = 0;
    uint32 m_uiGouge_Timer = 0;
    uint32 m_uiMark_Timer = 0;
    uint32 m_uiThrash_Timer = 0;
    uint32 m_uiVanish_Timer = 0;
    uint32 m_uiVisible_Timer = 0;
    uint32 m_uiBackstab_Timer = 0;
    uint32 m_uiTourbillon_Timer = 0;
    uint32 m_uiRosser_Timer = 0;
    uint32 m_uiRavage_Timer = 0;
    uint32 m_uiSwitchToTroll_Timer = 0;

    uint32 m_uiSummon_Timer = 0;
    uint32 m_uiSummonCount = 0;

    ObjectGuid m_uiMarkedGUID;

    bool m_bIsPhaseTwo = false;
    bool m_bIsVanished = false;

    void Reset() override
    {
        m_uiShadowWordPain_Timer = 8000;
        m_uiGouge_Timer = 14000;
        m_uiMark_Timer = 35000;
        m_uiThrash_Timer = urand(5000, 9000);
        m_uiVanish_Timer = 35000;
        m_uiSwitchToTroll_Timer = ARLOKK_DISABLE_TIMER;
        m_uiVisible_Timer = 6000;
        m_uiBackstab_Timer = 6000;
        m_uiRosser_Timer = urand(5000, 9000);
        m_uiTourbillon_Timer = 4000;
        m_uiRavage_Timer = 15000;

        m_uiSummon_Timer = 5000;
        m_uiSummonCount = 0;

        m_bIsPhaseTwo = false;
        m_bIsVanished = false;

        m_uiMarkedGUID.Clear();

        me->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
        ClassicZgArlokkSetDamagePct(me, 1.0f);      // VMaNGOS ResetStats()
        me->SetObjectScale(1.0f);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        ClassicScriptText(SAY_ARLOKK_AGGRO, me);
        DoZoneInCombat();
        if (GameObject* door = me->FindNearestGameObject(GO_ARLOKK_FORCE_FIELD, 100.0f))
            door->UseDoorOrButton();
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            if (m_pInstance->GetData(CLASSIC_ZG_TYPE_ARLOKK) != DONE)
                m_pInstance->SetData(CLASSIC_ZG_TYPE_ARLOKK, NOT_STARTED);

        if (GameObject* door = me->FindNearestGameObject(GO_ARLOKK_FORCE_FIELD, 100.0f))
            door->ResetDoorOrButton();
        if (GameObject* gong = me->FindNearestGameObject(GO_ARLOKK_GONG, 100.0f, false))
            gong->Respawn();
        // we should be summoned, so despawn
        me->DespawnOrUnsummon();
    }

    void JustDied(Unit* /*killer*/) override
    {
        ClassicScriptText(SAY_ARLOKK_DEATH, me);

        me->RemoveAurasDueToSpell(SPELL_ARLOKK_PANTHER_TRANSFORM);
        ClassicZgArlokkLeaveVanish(me);
        me->SetObjectScale(1.0f);

        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_ZG_TYPE_ARLOKK, DONE);
        if (GameObject* door = me->FindNearestGameObject(GO_ARLOKK_FORCE_FIELD, 100.0f))
            door->ResetDoorOrButton();

        // Remove a Hakkar Power stack.
        me->CastSpell(me, CLASSIC_ZG_SPELL_HAKKAR_POWER_DOWN, true);
    }

    void DoSummonSinglePhanter(float x, float y, float z, Unit* pTarget)
    {
        if (Creature* pInvoc = me->SummonCreature(NPC_ARLOKK_ZULIAN_PROWLER, x, y, z, 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15s))
            if (pTarget && pInvoc->AI())
                pInvoc->AI()->AttackStart(pTarget);
    }

    void DoSummonPhanters()
    {
        Unit* pUnit = ObjectAccessor::GetUnit(*me, m_uiMarkedGUID);
        DoSummonSinglePhanter(-11532.7998f, -1649.6734f, 41.4800f, pUnit);
        DoSummonSinglePhanter(-11532.9970f, -1606.4840f, 41.2979f, pUnit);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        if (Unit* pUnit = ObjectAccessor::GetUnit(*me, m_uiMarkedGUID))
        {
            if (pUnit->IsAlive())
            {
                pSummoned->AI()->AttackStart(pUnit);
                ++m_uiSummonCount;
                return;
            }
        }
        if (Unit* pTarget = SelectTarget(SelectTargetMethod::Random, 0))
        {
            pSummoned->AI()->AttackStart(pTarget);
            ++m_uiSummonCount;
            return;
        }
        // Nobody left?
        pSummoned->DespawnOrUnsummon();
        EnterEvadeMode(EvadeReason::NoHostiles);
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!UpdateVictim())
            return;

        if (!m_bIsPhaseTwo && !m_bIsVanished) // P1
        {
            if (m_uiShadowWordPain_Timer < uiDiff)
            {
                if (DoCastVictim(SPELL_ARLOKK_SHADOWWORDPAIN) == SPELL_CAST_OK)
                    m_uiShadowWordPain_Timer = 15000;
            }
            else
                m_uiShadowWordPain_Timer -= uiDiff;

            if (m_uiBackstab_Timer < uiDiff)
            {
                if (DoCastVictim(SPELL_ARLOKK_BACKSTAB) == SPELL_CAST_OK)
                    m_uiBackstab_Timer = urand(6000, 12000);
            }
            else
                m_uiBackstab_Timer -= uiDiff;

            if (m_uiMark_Timer < uiDiff)
            {
                if (Unit* pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                {
                    // Remove the mark from the previous player
                    if (Unit* pOldMark = ObjectAccessor::GetUnit(*me, m_uiMarkedGUID))
                        pOldMark->RemoveAurasDueToSpell(SPELL_ARLOKK_MARK);

                    if (Player* pMark = pTarget->GetCharmerOrOwnerPlayerOrPlayerItself())
                    {
                        if (DoCast(pMark, SPELL_ARLOKK_MARK) == SPELL_CAST_OK)
                        {
                            m_uiMarkedGUID = pMark->GetGUID();
                            ClassicScriptText(SAY_ARLOKK_FEAST_PANTHER, me, pMark);
                            m_uiMark_Timer = 15000;
                        }
                    }
                    else
                    {
                        m_uiMarkedGUID.Clear();
                        TC_LOG_ERROR("scripts", "classic_boss_arlokk could not acquire a new target to mark.");
                    }
                }
            }
            else
                m_uiMark_Timer -= uiDiff;
        }
        else if (!m_bIsVanished) // P2: panther
        {
            if (m_uiThrash_Timer < uiDiff)
            {
                if (DoCastVictim(SPELL_ARLOKK_ROSSER) == SPELL_CAST_OK)
                    m_uiThrash_Timer = urand(5000, 9000);
            }
            else
                m_uiThrash_Timer -= uiDiff;

            // Ravage Timer
            if (m_uiRavage_Timer <= uiDiff)
            {
                if (DoCastVictim(SPELL_ARLOKK_RAVAGE) == SPELL_CAST_OK)
                    m_uiRavage_Timer = 16000;
            }
            else
                m_uiRavage_Timer -= uiDiff;

            // Gouge_Timer
            if (m_uiGouge_Timer < uiDiff)
            {
                if (DoCastVictim(SPELL_ARLOKK_GOUGE) == SPELL_CAST_OK)
                {
                    if (Unit* victim = me->GetVictim())
                        if (GetThreat(victim))
                            ModifyThreatByPercent(victim, -80);

                    m_uiGouge_Timer = urand(17000, 27000);
                }
            }
            else
                m_uiGouge_Timer -= uiDiff;

            if (m_uiTourbillon_Timer < uiDiff)
            {
                if (DoCastVictim(SPELL_ARLOKK_TOURBILLON) == SPELL_CAST_OK)
                    m_uiTourbillon_Timer = 16000;
            }
            else
                m_uiTourbillon_Timer -= uiDiff;
        }

        if (m_uiSummonCount <= (ARLOKK_MAX_PANTHER_COUNT - 1))
        {
            if (m_uiSummon_Timer < uiDiff)
            {
                DoSummonPhanters();
                m_uiSummon_Timer = 5000;
            }
            else
                m_uiSummon_Timer -= uiDiff;
        }

        //
        // Phase handling
        //
        // Troll -> Vanish
        if (m_uiVanish_Timer < uiDiff)
        {
            // Invisible model
            me->RemoveAurasDueToSpell(SPELL_ARLOKK_PANTHER_TRANSFORM);
            ClassicZgArlokkEnterVanish(me);

            m_bIsVanished = true;

            // Vanishes between 35 and 50 sec
            m_uiVisible_Timer = urand(35000, 50000);
            m_uiVanish_Timer = ARLOKK_DISABLE_TIMER;
        }
        else
            m_uiVanish_Timer -= uiDiff;

        // Vanish -> Panther
        if (m_bIsVanished)
        {
            if (m_uiVisible_Timer < uiDiff)
            {
                // Panther transform
                me->CastSpell(me, SPELL_ARLOKK_PANTHER_TRANSFORM, false);
                me->SetObjectScale(1.7f);

                ClassicZgArlokkSetDamagePct(me, 1.35f);

                // And ambush
                Unit* pVanishTarget = SelectTarget(SelectTargetMethod::Random, 0);
                ClassicZgArlokkLeaveVanish(me);
                ClassicZgArlokkAmbush(me, pVanishTarget, SPELL_ARLOKK_BACKSTAB);

                m_bIsPhaseTwo = true;
                m_bIsVanished = false;
                // 45 sec as PANTHER + 1 min as TROLL
                m_uiSwitchToTroll_Timer = 45000;
                m_uiVanish_Timer = m_uiSwitchToTroll_Timer + 30000;
            }
            else
                m_uiVisible_Timer -= uiDiff;
        }
        // else: melee (TC master auto-melee; disabled while vanished)

        // Panther -> Troll
        // Back to TROLL after 45 sec as panther.
        if (m_bIsPhaseTwo)
        {
            if (m_uiSwitchToTroll_Timer < uiDiff)
            {
                me->RemoveAurasDueToSpell(SPELL_ARLOKK_PANTHER_TRANSFORM);
                m_bIsPhaseTwo = false;
                m_uiSwitchToTroll_Timer = ARLOKK_DISABLE_TIMER;
            }
            else
                m_uiSwitchToTroll_Timer -= uiDiff;
        }
    }
};

/*######
## mob_prowler (summoned panthers)
######*/

struct classic_mob_prowler : public ScriptedAI
{
    classic_mob_prowler(Creature* creature) : ScriptedAI(creature) { }

    ObjectGuid m_uiArlokkGuid;
    uint32 m_uiThrash_Timer = 0;
    uint32 m_uiUpdateTarget_Timer = 0;

    classic_boss_arlokk* GetArlokkAI()
    {
        if (m_uiArlokkGuid.IsEmpty())
            if (Creature* pArlokk = me->FindNearestCreature(CLASSIC_ZG_NPC_ARLOKK, 100.0f, true))
                m_uiArlokkGuid = pArlokk->GetGUID();

        if (Creature* pArlokk = ObjectAccessor::GetCreature(*me, m_uiArlokkGuid))
            if (pArlokk->IsAlive())
                return dynamic_cast<classic_boss_arlokk*>(pArlokk->AI());
        return nullptr;
    }

    void Reset() override
    {
        DoCastSelf(SPELL_PROWLER_SNEAK);
        m_uiThrash_Timer = urand(5000, 9000);
        m_uiUpdateTarget_Timer = 2000;
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (classic_boss_arlokk* pArlokkAI = GetArlokkAI())
            if (pArlokkAI->m_uiSummonCount)
                pArlokkAI->m_uiSummonCount--;
    }

    void UpdateAI(uint32 uiDiff) override
    {
        // VMaNGOS does not call SelectHostileTarget() here: the panthers are forced onto the marked target
        if (m_uiUpdateTarget_Timer <= uiDiff)
        {
            Unit* pMarkedTarget = nullptr;
            if (classic_boss_arlokk* pArlokkAI = GetArlokkAI())
                pMarkedTarget = ObjectAccessor::GetUnit(*me, pArlokkAI->m_uiMarkedGUID);
            else
            {
                me->DespawnOrUnsummon();
                return;
            }
            if (Unit* victim = me->GetVictim())
                if (GetThreat(victim))
                    ModifyThreatByPercent(victim, -100);
            if (pMarkedTarget)
            {
                AttackStart(pMarkedTarget);
                DoCastSelf(SPELL_PROWLER_SNEAK);
            }
            m_uiUpdateTarget_Timer = 2000;
        }
        else
            m_uiUpdateTarget_Timer -= uiDiff;

        if (m_uiThrash_Timer <= uiDiff)
        {
            DoCastSelf(SPELL_ARLOKK_ROSSER);
            m_uiThrash_Timer = urand(5000, 9000);
        }
        else
            m_uiThrash_Timer -= uiDiff;

        // melee: TC master auto-melee
    }
};

void AddSC_classic_boss_arlokk()
{
    RegisterGameObjectAI(classic_go_gong_of_bethekk);
    RegisterCreatureAI(classic_boss_arlokk);
    RegisterCreatureAI(classic_mob_prowler);
}
