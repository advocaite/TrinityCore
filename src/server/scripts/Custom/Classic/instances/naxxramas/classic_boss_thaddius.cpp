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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/eastern_plaguelands/naxxramas/boss_thaddius.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: boss_thaddius, boss_stalagg, boss_feugen, npc_tesla_coil,
//          spell_thaddius_positive_charge (28062), spell_thaddius_negative_charge (28085),
//          spell_thaddius_magnetic_pull (28337), spell_thaddius_positive_charge_aura (28059),
//          spell_thaddius_negative_charge_aura (28084)

#include "ScriptMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "ThreatManager.h"
#include "classic_naxxramas.h"
#include "classic_script_text.h"
#include <algorithm>
#include <random>
#include <vector>

namespace
{
enum ClassicNaxxStalaggFeugen : uint32
{
    // Stalagg
    SAY_STAL_AGGRO          = 13083,
    SAY_STAL_SLAY           = 13085,
    SAY_STAL_DEATH          = 13084,

    // Feugen
    SAY_FEUG_AGGRO          = 13023,
    SAY_FEUG_SLAY           = 13025,
    SAY_FEUG_DEATH          = 13024,

    SPELL_THAD_WARSTOMP          = 28125,
    SPELL_THAD_FLASH             = 28127, // TODO: stun spell supposedly used by feugen? Cant find any sources for it
    SPELL_THAD_POWERSURGE        = 28134,
    SPELL_THAD_STATIC_FIELD      = 28135,
    SPELL_THAD_MAGNETIC_PULL     = 28337
};

enum ClassicNaxxThaddiusAddEvents : uint32
{
    EVENT_THAD_WARSTOMP = 1,
    EVENT_THAD_STATIC_FIELD,
    EVENT_THAD_POWERSURGE,
    EVENT_THAD_MAGNETIC_PULL
};

enum ClassicNaxxThaddius : uint32
{
    SAY_THAD_AGGRO_1 = 13086,
    SAY_THAD_AGGRO_2 = 13087,
    SAY_THAD_AGGRO_3 = 13088,

    SAY_THAD_SLAY    = 13096,
    SAY_THAD_ELECT   = 13090,
    SAY_THAD_DEATH   = 13089,

    SPELL_THADIUS_SPAWN             = 28160,
    SPELL_THADIUS_LIGHTNING_VISUAL  = 28136,
    SPELL_THAD_BALL_LIGHTNING       = 28299,
    SPELL_THAD_CHAIN_LIGHTNING      = 28167,
    SPELL_THAD_BESERK               = 27680,

    SPELL_THAD_POLARITY_SHIFT       = 28089,

    SPELL_POSITIVE_CHARGE_APPLY     = 28059,
    SPELL_POSITIVE_CHARGE_TICK      = 28062,
    SPELL_POSITIVE_CHARGE_AMP       = 29659,

    SPELL_NEGATIVE_CHARGE_APPLY     = 28084,
    SPELL_NEGATIVE_CHARGE_TICK      = 28085,
    SPELL_NEGATIVE_CHARGE_AMP       = 29660
};

enum ClassicNaxxThaddiusCoils : uint32
{
    EMOTE_LOSING_LINK = 12156, // not sure if existed in vanilla
    EMOTE_TESLA_OVERLOAD = 12178, // confirmed existed in vanilla

    SPELL_FEUGEN_CHAIN = 28111,
    SPELL_STALAGG_CHAIN = 28096,
    SPELL_FEUGEN_TESLA_PASSIVE = 28109,
    SPELL_STALAGG_TESLA_PASSIVE = 28097,
    SPELL_SHOCK_OVERLOAD = 28159,
    SPELL_TESLA_SHOCK = 28099
};

enum ClassicNaxxStalagFeugen
{
    eSTALAGG = 0,
    eFEUGEN = 1
};

enum ClassicNaxxThaddiusPhase
{
    THAD_NOT_STARTED,
    THAD_PHASE1,
    THAD_TRANSITION,
    THAD_PHASE2
};

enum ClassicNaxxThaddiusEvents : uint32
{
    EVENT_THAD_SHIFT = 1,                // polarity shift
    EVENT_THAD_SHIFT_TALK,               // polarity shift yell (hack? couldn't find any event for cast finish)
    EVENT_THAD_CHAIN,                    // chain lightning
    EVENT_THAD_BERSERK,                  // enrage timer
    EVENT_THAD_REVIVE_FEUGEN,            // timer until feugen is revived (if stalagg still lives)
    EVENT_THAD_REVIVE_STALAGG,           // timer until stalagg is revived (if feugen still lives)
    EVENT_THAD_TRANSITION_1,             // timer until overload emote
    EVENT_THAD_TRANSITION_2,             // timer until thaddius gets zapped by the coils
    EVENT_THAD_TRANSITION_3,             // timer until thaddius engages
    EVENT_THAD_ENABLE_BALL_LIGHTNING,    // grace period after thaddius aggro after which he starts being a baller (e.g. tossing ball lightning at out of range targets)

    EVENT_THAD_POLARITY_CHANGE
};

//static constexpr uint32 ADDS_RESPAWN_TIMER = 5000;      // adds respawn after 5 sec if not both area killed in that window.

constexpr Milliseconds CLASSIC_THAD_ENRAGE_TIMER = 5min;   // 5 min enrage once p2 starts

// Initial polarity shift is 10s, after that every 30 sec
Milliseconds ClassicThadPolarityShiftTimer(bool initial = false) { return initial ? 10s : 30s; }

// Chain lightning timer. TODO: confirm timers. Atm is guess
Milliseconds ClassicThadChainLightningTimer() { return Milliseconds(urand(5000, 7000)); }

constexpr float ClassicThadAddPositions[2][4] =
{
    { 3449.03f, -2934.74f, 312.18f, 5.41f },
    { 3508.85f, -2994.08f, 312.18f, 2.33f },
};
constexpr float ClassicThadTeslaCoilPositions[2][3] =
{
    //{ 3487.76f, -2911.2f, 319.406f },
    { 3487.10f, -2911.50f, 319.526f },
    { 3527.81f, -2952.38f, 319.526f }
};

void ClassicThadUnsummon(Creature* creature)
{
    if (!creature)
        return;
    if (TempSummon* tmpSumm = creature->ToTempSummon())
        tmpSumm->UnSummon();
    else
        TC_LOG_ERROR("scripts", "Thaddius: creature {} was not a temp summon", creature->GetGUID().ToString());
}
}

struct classic_npc_tesla_coil : public ScriptedAI
{
    classic_npc_tesla_coil(Creature* creature) : ScriptedAI(creature), m_pInstance(GetClassicNaxxInstance(creature))
    {
        // VMaNGOS Scripted_NoMovementAI
        SetCombatMovement(false);
    }

    classic_instance_naxxramas_InstanceScript* m_pInstance;
    bool m_bToFeugen = false;
    ObjectGuid m_guid;
    uint32 shockTimer = 0;
    bool hadLink = true;

    void Reset() override
    {
        me->SetWanderDistance(0.01f);
        me->SetDefaultMovementType(RANDOM_MOTION_TYPE);
        me->GetMotionMaster()->Initialize();
        shockTimer = 0;
        hadLink = true;
    }

    void MoveInLineOfSight(Unit* /*who*/) override { }

    void JustEngagedWith(Unit* /*who*/) override
    {
        //m_creature->SetInCombatWithZone();
    }

    void CastChain()
    {
        // VMaNGOS casts the chain on itself; the spell's TARGET_UNIT_NEARBY_ENTRY picks the add (spell_script_target).
        // TC has no conditions for it, so the add is passed as explicit target (TC falls back to it).
        // TODO(classic): add `conditions` rows (SOURCE_TYPE_SPELL_IMPLICIT_TARGET) 28096 -> 15929, 28111 -> 15930.
        Creature* add = me->GetMap()->GetCreature(m_guid);
        DoCast(add ? add : me, m_bToFeugen ? SPELL_FEUGEN_CHAIN : SPELL_STALAGG_CHAIN);
    }

    void ReApplyChain(uint32 uiEntry, ObjectGuid guid)
    {
        m_guid = guid;
        if (uiEntry == NPC_FEUGEN)
            m_bToFeugen = true;
        else if (uiEntry == NPC_STALAGG)
            m_bToFeugen = false;
        else
            TC_LOG_ERROR("scripts", "classic_npc_tesla_coil::ReApplyChain got entry which was not stalagg or feugen.");

        CastChain();
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (Creature* add = me->GetMap()->GetCreature(m_guid))
        {
            if (add->IsInCombat() && !me->IsInCombat())
            {
                DoZoneInCombat();
            }

            if (me->GetDistance2d(add) > 60.0f)
            {
                me->InterruptNonMeleeSpells(true);
                //if (add->HasAura(m_bToFeugen ? SPELL_FEUGEN_CHAIN : SPELL_STALAGG_CHAIN))
                if (hadLink)
                    ClassicScriptText(EMOTE_LOSING_LINK, me);
                hadLink = false;

                if (shockTimer < uiDiff)
                {
                    shockTimer = 1500;
                    // todo: not sure if should target nearest or random target
                    if (Unit* shockTarget = SelectTarget(SelectTargetMethod::MinDistance, 0, 0.0f, true))
                    {
                        DoCast(shockTarget, SPELL_TESLA_SHOCK, true);
                    }
                }
                else
                    shockTimer -= uiDiff;
            }
            else
            {
                shockTimer = 0;
                if (!me->IsNonMeleeSpellCast(true))
                    CastChain();
                hadLink = true;
            }
        }
    }
};

struct classic_boss_thaddius_adds_shared : public ScriptedAI
{
    classic_boss_thaddius_adds_shared(Creature* creature, ClassicNaxxStalagFeugen sOrF) : ScriptedAI(creature), m_pInstance(GetClassicNaxxInstance(creature)), m_SorF(sOrF) { }

    classic_instance_naxxramas_InstanceScript* m_pInstance;
    ClassicNaxxStalagFeugen m_SorF;
    bool m_bFakeDeath = false;
    uint32 fakeDeathTimer = 0;
    bool bothDeath = false;
    uint32 timeSincePull = 0;
    EventMap m_events;
    ObjectGuid otherAdd;

    void Reset() override
    {
        m_events.Reset();
        timeSincePull = 0;
        fakeDeathTimer = 0;
        m_bFakeDeath = false;
        bothDeath = false;

        // We might Reset while faking death, so undo this
        me->SetUninteractible(false);
        me->SetFullHealth();
        me->SetStandState(UNIT_STAND_STATE_STAND);
    }

    static Milliseconds WarstompTimer()
    {
        // best guess timer based on
        // https://www.youtube.com/watch?v=GmE5JufAcT0
        // and https://www.youtube.com/watch?v=3hen_d6cb-Y
        return Milliseconds(urand(15000, 20000));
    }
    static Milliseconds PowerSurgeTimer()
    {
        // https://www.youtube.com/watch?v=3hen_d6cb-Y
        // Timer seems correct based on above video
        return Milliseconds(urand(25000, 30000));
    }
    static Milliseconds MagneticPullTimer()
    {
        // Every 20 seconds, but from videos can be seen to drift a tiny bit.
        // guides mention 20.5sec, so lets just make it that.
        return 20500ms;
    }
    static Milliseconds StaticFiledTimer()
    {
        //https://www.youtube.com/watch?v=GmE5JufAcT0
        // animation can be seen in video, pretty much exactly every 6 seconds
        return 6000ms;
    }

    Creature* GetOtherAdd()
    {
        return me->GetMap()->GetCreature(otherAdd);
    }

    // VMaNGOS Aggro()
    void JustEngagedWith(Unit* pWho) override
    {
        if (m_bFakeDeath)
            return;

        if (!m_pInstance)
            return;

        DoZoneInCombat();
        m_pInstance->SetData(TYPE_THADDIUS, IN_PROGRESS);

        if (Creature* pOtherAdd = GetOtherAdd())
        {
            if (!pOtherAdd->IsInCombat())
                pOtherAdd->AI()->AttackStart(pWho);
        }
    }

    void JustReachedHome() override
    {
        if (!m_pInstance)
            return;
        m_events.Reset();
        m_pInstance->SetData(TYPE_THADDIUS, FAIL);
    }

    bool HandleMagneticPull()
    {
        if (m_bFakeDeath)
            return false;

        Unit* myVictim = me->GetVictim();

        if (!myVictim)
            return false;

        Creature* pOtherAdd = GetOtherAdd();

        if (!pOtherAdd)
            return false;

        if (classic_boss_thaddius_adds_shared* otherAI = dynamic_cast<classic_boss_thaddius_adds_shared*>(pOtherAdd->AI()))
        {
            if (otherAI->m_bFakeDeath)
                return false; // can, presumably, only do it when both are alive
        }
        Unit* otherVictim = pOtherAdd->GetVictim();

        if (!otherVictim)
            return false;

        ThreatManager& myThreat = me->GetThreatManager();
        ThreatManager& otherThreat = pOtherAdd->GetThreatManager();

        float myTankThreat = myThreat.GetThreat(myVictim);
        float myOtherTankThreat = myThreat.GetThreat(otherVictim);

        float otherTankThreat = otherThreat.GetThreat(otherVictim);
        float otherAddMyVictimThreat = otherThreat.GetThreat(myVictim);
        //todo: VERIFY NEGATIVE THREAT OK
        // set the two entries in feugen's threat table to be equal to the ones in stalagg's
        myThreat.AddThreat(otherVictim, otherTankThreat - myOtherTankThreat, nullptr, true, true);
        myThreat.AddThreat(myVictim, otherAddMyVictimThreat - myTankThreat, nullptr, true, true);

        // set the two entries in stalagg's threat table to be equal to the ones in feugen's
        otherThreat.AddThreat(myVictim, myTankThreat - otherAddMyVictimThreat, nullptr, true, true);
        otherThreat.AddThreat(otherVictim, myOtherTankThreat - otherTankThreat, nullptr, true, true);

        me->InterruptNonMeleeSpells(true);
        me->CastSpell(otherVictim, SPELL_THAD_MAGNETIC_PULL, true);
        pOtherAdd->InterruptNonMeleeSpells(true);
        pOtherAdd->CastSpell(myVictim, SPELL_THAD_MAGNETIC_PULL, true);

        // @hack prevent mmaps clusterfucks from breaking tesla while tanks are midair
        // feugen->AddAura(SPELL_ROOT_SELF, feugen);
        // stalagg->AddAura(SPELL_ROOT_SELF, stalagg);

        // and make both attack their respective new tanks
        AttackStart(otherVictim);
        pOtherAdd->AI()->AttackStart(myVictim);
        return true;
    }

    void HandleReviveEvent()
    {
        Reset();
        ResetThreatList();

        if (Unit* nearestTarget = SelectTarget(SelectTargetMethod::MinDistance, 0))
        {
            JustEngagedWith(nearestTarget);
        }
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (m_bFakeDeath)
        {
            if (fakeDeathTimer < uiDiff)
            {
                if (!bothDeath)
                    HandleReviveEvent();
            }
            else
                fakeDeathTimer -= uiDiff;
            return;
        }

        if (!UpdateVictim())
            return;

        timeSincePull += uiDiff;
        m_events.Update(uiDiff);
        while (uint32 l_EventId = m_events.ExecuteEvent())
        {
            switch (l_EventId)
            {
                case EVENT_THAD_WARSTOMP:
                    // need to delay warstomp if we have just done the pull. If warstomp hits midair the
                    // pull effect is cancelled and tanks fall down
                    if (timeSincePull < 5000)
                    {
                        m_events.Repeat(Milliseconds(5000 - timeSincePull));
                    }
                    else
                    {
                        if (DoCastSelf(SPELL_THAD_WARSTOMP) == SPELL_CAST_OK)
                            m_events.Repeat(WarstompTimer());
                        else
                            m_events.Repeat(100ms);
                    }
                    break;
                case EVENT_THAD_STATIC_FIELD:
                    if (DoCastSelf(SPELL_THAD_STATIC_FIELD) == SPELL_CAST_OK)
                        m_events.Repeat(StaticFiledTimer());
                    else
                        m_events.Repeat(100ms);
                    break;
                case EVENT_THAD_POWERSURGE:
                    if (DoCastSelf(SPELL_THAD_POWERSURGE) == SPELL_CAST_OK)
                        m_events.Repeat(PowerSurgeTimer());
                    else
                        m_events.Repeat(100ms);
                    break;
                case EVENT_THAD_MAGNETIC_PULL:
                    if (HandleMagneticPull())
                    {
                        m_events.Repeat(MagneticPullTimer());
                        timeSincePull = 0;
                    }
                    else
                        m_events.Repeat(100ms);
                    break;
                default:
                    break;
            }
        }
    }

    void AttackStart(Unit* pWho) override
    {
        if (m_bFakeDeath)
            return;
        ScriptedAI::AttackStart(pWho);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& uiDamage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (uiDamage < me->GetHealth())
            return;

        // Prevent glitch if in fake death
        if (m_bFakeDeath)
        {
            uiDamage = 0;
            return;
        }

        if (Creature* pOtherAdd = GetOtherAdd())
        {
            if (classic_boss_thaddius_adds_shared* otherAI = dynamic_cast<classic_boss_thaddius_adds_shared*>(pOtherAdd->AI()))
            {
                if (otherAI->m_bFakeDeath)
                {
                    otherAI->bothDeath = true;
                    bothDeath = true;
                    if (m_pInstance)
                        m_pInstance->SetData(TYPE_THADDIUS, SPECIAL);
                }
            }
        }

        if (m_SorF == eSTALAGG)
            ClassicScriptText(SAY_STAL_DEATH, me);
        else
            ClassicScriptText(SAY_FEUG_DEATH, me);

        // VMaNGOS: damage = 0 and SetHealth(0). TC kills a unit at 0 health on the next damage, so 1 hp is kept.
        uiDamage = me->GetHealth() > 1 ? uint32(me->GetHealth() - 1) : 0;

        fakeDeathTimer = 5000;
        m_bFakeDeath = true;
        me->InterruptNonMeleeSpells(false);
        me->StopMoving();
        me->RemoveAllAurasOnDeath(); // todo: will this remove the chain?
        me->SetUninteractible(true);
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveIdle();
        me->SetStandState(UNIT_STAND_STATE_DEAD);
        me->AttackStop();
        // TODO(classic): VMaNGOS also ClearComboPointHolders() / ClearAllReactives() (no TC equivalent needed)
    }
};

struct classic_boss_thaddius : public ScriptedAI
{
    classic_boss_thaddius(Creature* creature) : ScriptedAI(creature), m_pInstance(GetClassicNaxxInstance(creature)) { }

    classic_instance_naxxramas_InstanceScript* m_pInstance;

    uint32 m_uiBallLightningTimer = 1000;

    ClassicNaxxThaddiusPhase m_Phase = THAD_NOT_STARTED;
    ObjectGuid addGuids[2];
    ObjectGuid coilGuids[2];

    EventMap m_events;

    std::random_device m_randDevice;
    std::mt19937 m_random{ m_randDevice() };
    uint32 killSayCooldown = 0;

    // VMaNGOS does this in the AI constructor
    void JustAppeared() override
    {
        ScriptedAI::JustAppeared();

        if (m_pInstance)
        {
            CheckSpawnAdds();
        }

        // Apply the "stun" aura again, which makes him darker and not moving
        if (!me->HasAura(SPELL_THADIUS_SPAWN))
            DoCastSelf(SPELL_THADIUS_SPAWN, true);

        // He is not targetable other than in p2
        me->SetUninteractible(true);
    }

    // Helper for CheckSpawnAdds
    void HandleCheckSpawnAdd(ClassicNaxxStalagFeugen whichAdd)
    {
        // Respawn or revive the add itself
        Creature* addCreature = me->GetMap()->GetCreature(addGuids[whichAdd]);
        Creature* coilCreature = me->GetMap()->GetCreature(coilGuids[whichAdd]);
        uint32 addEntry = whichAdd == eSTALAGG ? NPC_STALAGG : NPC_FEUGEN;

        // Simply unsummoning the add and the coil creature if they still exist
        if (addCreature)
        {
            ClassicThadUnsummon(addCreature);
            addCreature = nullptr;
        }
        if (!addCreature)
        {
            // Summonming a new add
            if (Creature* pC = me->SummonCreature(addEntry, ClassicThadAddPositions[whichAdd][0], ClassicThadAddPositions[whichAdd][1], ClassicThadAddPositions[whichAdd][2], ClassicThadAddPositions[whichAdd][3],
                TEMPSUMMON_MANUAL_DESPAWN))
            {
                addCreature = pC;
                addGuids[whichAdd] = pC->GetGUID();
            }
            else
                TC_LOG_ERROR("scripts", "Thaddius: failed spawning add {}", uint32(whichAdd));
        }

        if (coilCreature)
            ClassicThadUnsummon(coilCreature);

        // Summoning a new coil
        if (Creature* tc = me->SummonCreature(NPC_TESLA_COIL, ClassicThadTeslaCoilPositions[whichAdd][0], ClassicThadTeslaCoilPositions[whichAdd][1], ClassicThadTeslaCoilPositions[whichAdd][2], 0, TEMPSUMMON_MANUAL_DESPAWN))
        {
            coilGuids[whichAdd] = tc->GetGUID();
            if (classic_npc_tesla_coil* pTeslaAI = dynamic_cast<classic_npc_tesla_coil*>(tc->AI()))
                pTeslaAI->ReApplyChain(addEntry, addGuids[whichAdd]);
            else
                TC_LOG_ERROR("scripts", "classic_boss_thaddius::HandleCheckSpawnAdd failed to cast tesla coil AI to classic_npc_tesla_coil");
        }
        else
            TC_LOG_ERROR("scripts", "classic_boss_thaddius::HandleCheckSpawnAdd failed to spawn teslaCoil");

        // Making sure the coil GO does its animation
        uint32 coilGOEntry = whichAdd == eSTALAGG ? GO_CONS_NOX_TESLA_STALAGG : GO_CONS_NOX_TESLA_FEUGEN;
        if (m_pInstance)
            if (GameObject* tc = m_pInstance->GetSingleGameObjectFromStorage(coilGOEntry))
                tc->SetGoState(GO_STATE_ACTIVE);
    }

    // Respawn stalagg and feugen and the tesla coils corresponding to them. Activates the tesla coil GO and sets the chain
    void CheckSpawnAdds()
    {
        if (m_pInstance)
            if (m_pInstance->GetData(TYPE_THADDIUS) == DONE)
                return;

        HandleCheckSpawnAdd(eSTALAGG);
        HandleCheckSpawnAdd(eFEUGEN);
        if (Creature* pFeugen = me->GetMap()->GetCreature(addGuids[eFEUGEN]))
        {
            if (classic_boss_thaddius_adds_shared* addAI = dynamic_cast<classic_boss_thaddius_adds_shared*>(pFeugen->AI()))
                addAI->otherAdd = addGuids[eSTALAGG];
        }
        if (Creature* pStalagg = me->GetMap()->GetCreature(addGuids[eSTALAGG]))
        {
            if (classic_boss_thaddius_adds_shared* addAI = dynamic_cast<classic_boss_thaddius_adds_shared*>(pStalagg->AI()))
                addAI->otherAdd = addGuids[eFEUGEN];
        }
    }

    // Unsummons the tesla coil creature and stops animation of the coil GO
    void HandleUnsummonCoil(ClassicNaxxStalagFeugen which)
    {
        // Unsummon the tesla coil creature
        if (Creature* tc = me->GetMap()->GetCreature(coilGuids[which]))
            if (TempSummon* tmpSumm = tc->ToTempSummon())
                tmpSumm->UnSummon();
        coilGuids[which].Clear();

        // Make the tesla coil game object stop animating
        uint32 goEntry = which == eSTALAGG ? GO_CONS_NOX_TESLA_STALAGG : GO_CONS_NOX_TESLA_FEUGEN;

        if (m_pInstance)
            if (GameObject* pG = m_pInstance->GetSingleGameObjectFromStorage(goEntry))
                pG->SetGoState(GO_STATE_READY);
    }

    void HandleUnsummonAdd(ClassicNaxxStalagFeugen which)
    {
        if (Creature* add = me->GetMap()->GetCreature(addGuids[which]))
            if (TempSummon* tmpSumm = add->ToTempSummon())
                tmpSumm->UnSummon();
        addGuids[which].Clear();
    }

    void Reset() override
    {
        m_events.Reset();
        m_uiBallLightningTimer = 1000;
        m_Phase = THAD_NOT_STARTED;
        killSayCooldown = 0;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        /*
        switch (urand(0, 2))
        {
        case 0: DoScriptText(SAY_AGGRO_1, m_creature); break;
        case 1: DoScriptText(SAY_AGGRO_2, m_creature); break;
        case 2: DoScriptText(SAY_AGGRO_3, m_creature); break;
        }
        // Make Attackable
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        */
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
        {
            m_pInstance->SetData(TYPE_THADDIUS, FAIL);
            CheckSpawnAdds();
        }

        // Apply the "stun" aura again, which makes him darker and not moving
        if (!me->HasAura(SPELL_THADIUS_SPAWN))
            DoCastSelf(SPELL_THADIUS_SPAWN, true);

        // He is not targetable other than in p2
        me->SetUninteractible(true);
    }

    void KilledUnit(Unit* pVictim) override
    {
        if (pVictim->GetTypeId() != TYPEID_PLAYER)
            return;

        if (!killSayCooldown)
        {
            ClassicScriptText(SAY_THAD_SLAY, me);
            killSayCooldown = 5000;
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        ClassicScriptText(SAY_THAD_DEATH, me);

        if (m_pInstance)
        {
            m_pInstance->SetData(TYPE_THADDIUS, DONE);

            //they will despawn themself.
            // Make them TEMPSUMMON_TIMED_DEAD_DESPAWN with 10-20sec despawn time,
        }
    }

    void TransitionToPhase(ClassicNaxxThaddiusPhase newPhase)
    {
        m_Phase = newPhase;
        switch (m_Phase)
        {
            case THAD_NOT_STARTED:
                // VMaNGOS m_creature->OnLeaveCombat()
                EnterEvadeMode(EvadeReason::NoHostiles);
                break;
            case THAD_PHASE1:
                DoZoneInCombat();
                break;
            case THAD_TRANSITION:
                // Source: https://www.youtube.com/watch?v=GmE5JufAcT0
                // 3:18     adds dead
                // 3:28 +10 tesla coil overloads emote (link dissapears, GO still animating)
                // 3:31 +3  tesla coils shoots beam thing at thaddius. SPELL_THADIUS_SPAWN removed
                // 3:33 +2  thaddius engage. GO stops animating
                m_events.ScheduleEvent(EVENT_THAD_TRANSITION_1, 10000ms);
                m_events.ScheduleEvent(EVENT_THAD_TRANSITION_2, 13000ms);
                m_events.ScheduleEvent(EVENT_THAD_TRANSITION_3, 14000ms);
                break;
            case THAD_PHASE2:
                ResetThreatList();
                if (Unit* pTarget = SelectTarget(SelectTargetMethod::MinDistance, 0))
                {
                    AttackStart(pTarget);
                }
                m_events.ScheduleEvent(EVENT_THAD_BERSERK, CLASSIC_THAD_ENRAGE_TIMER);
                m_events.ScheduleEvent(EVENT_THAD_SHIFT, ClassicThadPolarityShiftTimer(true));
                m_events.ScheduleEvent(EVENT_THAD_CHAIN, ClassicThadChainLightningTimer());
                break;
            default:
                TC_LOG_ERROR("scripts", "classic_boss_thaddius in undefined phase-state");
        }
    }

    void UpdateTransitionPhase(uint32 diff)
    {
        m_events.Update(diff);
        while (uint32 l_EventId = m_events.ExecuteEvent())
        {
            switch (l_EventId)
            {
                case EVENT_THAD_TRANSITION_1:
                    // stop link to adds, coils do overload emote
                    HandleUnsummonAdd(eSTALAGG);
                    HandleUnsummonAdd(eFEUGEN);
                    if (Creature* coil = me->GetMap()->GetCreature(coilGuids[0]))
                    {
                        ClassicScriptText(EMOTE_TESLA_OVERLOAD, coil);
                    }
                    if (Creature* coil = me->GetMap()->GetCreature(coilGuids[1]))
                    {
                        ClassicScriptText(EMOTE_TESLA_OVERLOAD, coil);
                    }
                    break;
                case EVENT_THAD_TRANSITION_2:
                    // coils shoot beam at thaddius. SPELL_THADIUS_SPAWN removed
                    // TODO(classic): 28159 is TARGET_UNIT_NEARBY_ENTRY (spell_script_target 15928): add a `conditions` row
                    if (Creature* coil = me->GetMap()->GetCreature(coilGuids[0]))
                    {
                        coil->CastSpell(me, SPELL_SHOCK_OVERLOAD, true);
                    }
                    if (Creature* coil = me->GetMap()->GetCreature(coilGuids[1]))
                    {
                        coil->CastSpell(me, SPELL_SHOCK_OVERLOAD, true);
                    }
                    me->RemoveAurasDueToSpell(SPELL_THADIUS_SPAWN);
                    me->SetUninteractible(false);
                    DoCastSelf(SPELL_THADIUS_LIGHTNING_VISUAL, true);
                    break;
                case EVENT_THAD_TRANSITION_3:
                    HandleUnsummonCoil(eSTALAGG);
                    HandleUnsummonCoil(eFEUGEN);
                    TransitionToPhase(THAD_PHASE2);
                    break;
                default:
                    break;
            }
        }
    }

    void RemoveDebuffsFromPlayer(Player* pPlayer)
    {
        pPlayer->RemoveAurasDueToSpell(SPELL_POSITIVE_CHARGE_AMP);
        pPlayer->RemoveAurasDueToSpell(SPELL_POSITIVE_CHARGE_APPLY);
        pPlayer->RemoveAurasDueToSpell(SPELL_POSITIVE_CHARGE_TICK);
        pPlayer->RemoveAurasDueToSpell(SPELL_NEGATIVE_CHARGE_AMP);
        pPlayer->RemoveAurasDueToSpell(SPELL_NEGATIVE_CHARGE_APPLY);
        pPlayer->RemoveAurasDueToSpell(SPELL_NEGATIVE_CHARGE_TICK);
    }

    void DoPolarityShift()
    {
        ClassicScriptText(SAY_THAD_ELECT, me);

        // this is kinda ugly :/
        std::vector<Player*> playerVec;
        for (MapReference const& p : me->GetMap()->GetPlayers())
        {
            Player* pPlayer = p.GetSource();
            if (!pPlayer || !pPlayer->IsAlive())
                continue;

            playerVec.push_back(pPlayer);
        }
        std::shuffle(playerVec.begin(), playerVec.end(), m_random);
        size_t firstHalf = playerVec.size() / 2;
        for (size_t i = 0; i < firstHalf; i++)
        {
            Player* pPlayer = playerVec[i];
            RemoveDebuffsFromPlayer(pPlayer);
            pPlayer->CastSpell(pPlayer, SPELL_POSITIVE_CHARGE_APPLY, true);
        }
        // The second half gets the negative charge. (VMaNGOS starts this loop at 0, which strips the positive charge again and
        // leaves every player negative; vanilla splits the raid in two.)
        for (size_t i = firstHalf; i < playerVec.size(); i++)
        {
            Player* pPlayer = playerVec[i];
            RemoveDebuffsFromPlayer(pPlayer);
            pPlayer->CastSpell(pPlayer, SPELL_NEGATIVE_CHARGE_APPLY, true);
        }
    }

    void DoSpellChain()
    {
        Unit* pTarget = SelectTarget(SelectTargetMethod::Random, 0);
        if (!pTarget || DoCast(pTarget, SPELL_THAD_CHAIN_LIGHTNING) != SPELL_CAST_OK)
            m_events.Repeat(100ms);
        else
            m_events.Repeat(ClassicThadChainLightningTimer());
    }

    void UpdateP2(uint32 diff)
    {
        if (!m_pInstance)
            return;

        if (!UpdateVictim())
            return;

        m_events.Update(diff);
        while (uint32 l_EventId = m_events.ExecuteEvent())
        {
            switch (l_EventId)
            {
                case EVENT_THAD_SHIFT:
                    if (DoCastSelf(SPELL_THAD_POLARITY_SHIFT, CastSpellExtraArgs(TRIGGERED_IGNORE_CAST_IN_PROGRESS)) == SPELL_CAST_OK)
                    {
                        m_events.Repeat(30000ms);
                        m_events.ScheduleEvent(EVENT_THAD_POLARITY_CHANGE, 3000ms);
                    }
                    else
                        m_events.Repeat(100ms);
                    break;
                case EVENT_THAD_CHAIN:
                    DoSpellChain();
                    break;
                case EVENT_THAD_BERSERK:
                    if (DoCastSelf(SPELL_THAD_BESERK) != SPELL_CAST_OK)
                        m_events.Repeat(100ms);
                    break;
                case EVENT_THAD_POLARITY_CHANGE:
                    DoPolarityShift();
                    break;
                default:
                    break;
            }
        }

        // m_uiBallLightningTimer reinitialized to 1sec while the victim is in melee range, and only negated if target is oor.
        // This will prevent the boss from starting to spam balls of lightning if the boss is being moved with lag or something
        // (TC: melee swings are done by the core, so only the range check of VMaNGOS' DoMeleeAttackIfReady() branch remains)
        Unit* victim = me->GetVictim();
        if (!victim)
            return;

        if (!me->IsWithinMeleeRange(victim))
            m_uiBallLightningTimer -= std::min(diff, m_uiBallLightningTimer);
        else
            m_uiBallLightningTimer = 1000;

        // If we were not in melee range, it's time to start hurl some balls of lightning
        if (m_uiBallLightningTimer < diff && !me->IsNonMeleeSpellCast(false))
        {
            if (DoCast(victim, SPELL_THAD_BALL_LIGHTNING) == SPELL_CAST_OK)
                m_uiBallLightningTimer = 1500;
        }
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!m_pInstance)
            return;

        if (m_Phase != THAD_NOT_STARTED)
        {
            if (me->GetThreatManager().IsThreatListEmpty(true))
            {
                TransitionToPhase(THAD_NOT_STARTED);
                return;
            }
        }
        else
        {
            // He can act a bit weird if you stand next to him on server startup :/
            if (me->IsInCombat())
            {
                TransitionToPhase(THAD_NOT_STARTED);
                return;
            }
        }

        killSayCooldown -= std::min(killSayCooldown, uiDiff);

        switch (m_Phase)
        {
            case THAD_NOT_STARTED:
                // IN_PROGRESS is set by the adds when pulled
                if (m_pInstance->GetData(TYPE_THADDIUS) == IN_PROGRESS)
                    TransitionToPhase(THAD_PHASE1);
                break;
            case THAD_PHASE1:
                // if wipe during add-phase the adds will set FAIL
                if (m_pInstance->GetData(TYPE_THADDIUS) == FAIL)
                {
                    TransitionToPhase(THAD_NOT_STARTED);
                }
                // if adds are successfully killed, they set SPECIAL
                else if (m_pInstance->GetData(TYPE_THADDIUS) == SPECIAL)
                {
                    TransitionToPhase(THAD_TRANSITION);
                }
                break;
            case THAD_TRANSITION:
                UpdateTransitionPhase(uiDiff);
                break;
            case THAD_PHASE2:
                UpdateP2(uiDiff);
                break;
            default:
                TC_LOG_ERROR("scripts", "classic_boss_thaddius in undefined phase-state");
        }
    }
};

struct classic_boss_stalagg : public classic_boss_thaddius_adds_shared
{
    classic_boss_stalagg(Creature* creature) : classic_boss_thaddius_adds_shared(creature, eSTALAGG) { }

    void JustEngagedWith(Unit* pWho) override
    {
        ClassicScriptText(SAY_STAL_AGGRO, me);
        m_events.ScheduleEvent(EVENT_THAD_WARSTOMP, WarstompTimer());
        m_events.ScheduleEvent(EVENT_THAD_POWERSURGE, PowerSurgeTimer());
        m_events.ScheduleEvent(EVENT_THAD_MAGNETIC_PULL, MagneticPullTimer());
        classic_boss_thaddius_adds_shared::JustEngagedWith(pWho);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (bothDeath && m_pInstance && m_pInstance->GetData(TYPE_THADDIUS) != SPECIAL)
            m_pInstance->SetData(TYPE_THADDIUS, SPECIAL);
    }

    void KilledUnit(Unit* pVictim) override
    {
        if (pVictim->GetTypeId() == TYPEID_PLAYER)
            ClassicScriptText(SAY_STAL_SLAY, me);
    }
};

struct classic_boss_feugen : public classic_boss_thaddius_adds_shared
{
    classic_boss_feugen(Creature* creature) : classic_boss_thaddius_adds_shared(creature, eFEUGEN) { }

    void JustEngagedWith(Unit* pWho) override
    {
        ClassicScriptText(SAY_FEUG_AGGRO, me);

        m_events.ScheduleEvent(EVENT_THAD_WARSTOMP, WarstompTimer());
        m_events.ScheduleEvent(EVENT_THAD_STATIC_FIELD, StaticFiledTimer());

        classic_boss_thaddius_adds_shared::JustEngagedWith(pWho);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (bothDeath && m_pInstance && m_pInstance->GetData(TYPE_THADDIUS) != SPECIAL)
            m_pInstance->SetData(TYPE_THADDIUS, SPECIAL);
    }

    void KilledUnit(Unit* pVictim) override
    {
        if (pVictim->GetTypeId() == TYPEID_PLAYER)
            ClassicScriptText(SAY_FEUG_SLAY, me);
    }
};

// Shared logic of 28059 (Positive Charge) and 28084 (Negative Charge)
template <uint32 ApplySpellId, uint32 AmpSpellId>
class classic_spell_thaddius_charge_aura_base : public AuraScript
{
protected:
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ AmpSpellId });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Unit* target = GetTarget();
        // Only process in Naxxramas to avoid performance issues
        if (target->GetMapId() != MAP_NAXXRAMAS)
            return;

        uint8 numStacks = 0;
        // Finding the amount of other players within 13yd that has the same polarity
        for (MapReference const& it : target->GetMap()->GetPlayers())
        {
            Player* pPlayer = it.GetSource();
            if (!pPlayer)
                continue;
            if (pPlayer->GetGUID() == target->GetGUID())
                continue;
            if (!pPlayer->IsAlive())
                continue;
            // 2d distance should be good enough
            if (pPlayer->HasAura(ApplySpellId) && target->GetDistance2d(pPlayer) < 13.0f)
            {
                ++numStacks;
            }
        }
        if (numStacks > 0)
        {
            Aura* amp = target->GetAura(AmpSpellId);
            if (!amp)
                amp = target->AddAura(AmpSpellId, target);
            if (amp)
                amp->SetStackAmount(numStacks);
        }
        else
        {
            target->RemoveAurasDueToSpell(AmpSpellId);
        }
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        // Remove amplify effect on remove
        if (target->HasAura(AmpSpellId))
            target->RemoveAurasDueToSpell(AmpSpellId);
    }
};

// 28059 - Positive Charge (Thaddius)
class classic_spell_thaddius_positive_charge_aura : public classic_spell_thaddius_charge_aura_base<SPELL_POSITIVE_CHARGE_APPLY, SPELL_POSITIVE_CHARGE_AMP>
{
    void Register() override
    {
        // VMaNGOS OnPeriodicTrigger: the default trigger (28062) still happens
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_thaddius_positive_charge_aura::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_thaddius_positive_charge_aura::HandleRemove, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 28084 - Negative Charge (Thaddius)
class classic_spell_thaddius_negative_charge_aura : public classic_spell_thaddius_charge_aura_base<SPELL_NEGATIVE_CHARGE_APPLY, SPELL_NEGATIVE_CHARGE_AMP>
{
    void Register() override
    {
        // VMaNGOS OnPeriodicTrigger: the default trigger (28085) still happens
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_thaddius_negative_charge_aura::HandlePeriodic, EFFECT_1, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_thaddius_negative_charge_aura::HandleRemove, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 28062 - Positive Charge (Thaddius)
class classic_spell_thaddius_positive_charge : public SpellScript
{
    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        // Target also has positive charge, so no damage
        if (Unit* target = GetHitUnit())
            if (target->HasAura(SPELL_POSITIVE_CHARGE_APPLY))
                SetHitDamage(0);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_thaddius_positive_charge::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 28085 - Negative Charge (Thaddius)
class classic_spell_thaddius_negative_charge : public SpellScript
{
    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        // Target also has negative charge, so no damage
        if (Unit* target = GetHitUnit())
            if (target->HasAura(SPELL_NEGATIVE_CHARGE_APPLY))
                SetHitDamage(0);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_thaddius_negative_charge::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 28337 - Magnetic Pull (Thaddius)
class classic_spell_thaddius_magnetic_pull : public SpellScript
{
    void HandlePull(SpellEffIndex effIndex)
    {
        Unit* target = GetHitUnit();
        Unit* caster = GetCaster();
        if (!target || !caster)
            return;

        PreventHitDefaultEffect(effIndex);

        float speedXY = float(GetEffectInfo().MiscValue) * 0.1f;
        if (speedXY <= 0.0f)
            return;
        float speedZ = target->GetDistance(caster) / speedXY * 0.5f * 20.0f;
        target->KnockbackFrom(caster->GetPosition(), -speedXY, speedZ);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_thaddius_magnetic_pull::HandlePull, EFFECT_0, SPELL_EFFECT_ANY);
    }
};

void AddSC_classic_boss_thaddius()
{
    RegisterCreatureAI(classic_boss_thaddius);
    RegisterCreatureAI(classic_boss_stalagg);
    RegisterCreatureAI(classic_boss_feugen);
    RegisterCreatureAI(classic_npc_tesla_coil);
    RegisterSpellScript(classic_spell_thaddius_positive_charge);
    RegisterSpellScript(classic_spell_thaddius_negative_charge);
    RegisterSpellScript(classic_spell_thaddius_magnetic_pull);
    RegisterSpellScript(classic_spell_thaddius_positive_charge_aura);
    RegisterSpellScript(classic_spell_thaddius_negative_charge_aura);
}
