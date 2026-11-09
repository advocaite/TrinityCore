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

// Classic 1.60 port of VMaNGOS src/scripts/kalimdor/dustwallow_marsh/onyxias_lair/boss_onyxia.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: boss_onyxia, npc_onyxian_whelp
// Note (VMaNGOS): the breath spells rely on spell_target_position rows (see the SQL comment block at the end of the
// VMaNGOS source file) and Heated Ground (22191..22202) needs its trigger chain split in two.

#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MapReference.h"
#include "MotionMaster.h"
#include "MoveSpline.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "TemporarySummon.h"
#include "classic_onyxias_lair.h"
#include "classic_script_text.h"
#include <list>

namespace
{
enum ClassicOnyxia : uint32
{
    SAY_ONY_AGGRO                   = 8286,
    SAY_ONY_KILL                    = 8287,
    SAY_ONY_PHASE_2_TRANS           = 8288,
    SAY_ONY_PHASE_3_TRANS           = 8290,
    EMOTE_ONY_BREATH                = 7213,

    SPELL_ONY_WINGBUFFET            = 18500,
    SPELL_ONY_FLAMEBREATH           = 18435,
    SPELL_ONY_CLEAVE                = 19983,
    SPELL_ONY_TAILSWEEP             = 15847,
    SPELL_ONY_KNOCK_AWAY            = 19633,
    // Classic 1.60: vanilla 18431 is not in the 1.60 client; 22686 Bellowing Roar (same fear, 4 s, wider) is, and there it also
    // triggers 1297001 Lava Eruption, the lair's eruptions: Forever's Onyxia roar
    SPELL_ONY_BELLOWINGROAR         = 22686,

    SPELL_ONY_FIREBALL              = 18392,
    SPELL_ONY_DEEPBREATH            = 23461,
    SPELL_ONY_HEATED_GROUND_EAST    = 22191,
    SPELL_ONY_HEATED_GROUND_WEST    = 22197,
    SPELL_ONY_HOVER                 = 17131,

    SPELL_ONY_BREATH_NORTH_TO_SOUTH = 17086,                    // 20x in "array"
    SPELL_ONY_BREATH_SOUTH_TO_NORTH = 18351,                    // 11x in "array"
    SPELL_ONY_BREATH_EAST_TO_WEST   = 18576,                    // 7x in "array"
    SPELL_ONY_BREATH_WEST_TO_EAST   = 18609,                    // 7x in "array"
    SPELL_ONY_BREATH_SE_TO_NW       = 18564,                    // 12x in "array"
    SPELL_ONY_BREATH_NW_TO_SE       = 18584,                    // 12x in "array"
    SPELL_ONY_BREATH_SW_TO_NE       = 18596,                    // 12x in "array"
    SPELL_ONY_BREATH_NE_TO_SW       = 18617,                    // 12x in "array"

    POINT_ONY_DEPART_FLIGHT         = 20,
    POINT_ONY_LANDING_FLIGHT        = 21,

    ONY_PHASE_ONE                   = 1,
    ONY_PHASE_TWO                   = 2,
    ONY_PHASE_THREE                 = 3
};

float const ONYXIA_AGGRO_RANGE  = 58.0f;
float const ONYXIA_NORMAL_SPEED = 1.28571f;
float const ONYXIA_BREATH_SPEED = 3.0f;

struct ClassicOnyxMove
{
    uint32 uiLocId;
    uint32 uiLocIdEnd;
    uint32 uiSpellId;
    float fX, fY, fZ, fZGround;
};

ClassicOnyxMove const ClassicOnyxMoveData[] =
{
    {0, 4, SPELL_ONY_BREATH_NE_TO_SW,        10.2191f, -247.912f, -65.896f,  -85.84668f},  // north-east
    {1, 5, SPELL_ONY_BREATH_EAST_TO_WEST,   -31.4963f, -250.123f, -65.1278f, -89.127853f}, // east
    {2, 6, SPELL_ONY_BREATH_SE_TO_NW,       -63.5156f, -240.096f, -65.477f,  -85.066696f}, // south-east
    {3, 7, SPELL_ONY_BREATH_SOUTH_TO_NORTH, -65.8444f, -213.809f, -65.2985f, -84.298462f}, // south
    {4, 0, SPELL_ONY_BREATH_SW_TO_NE,       -58.2509f, -189.020f, -65.790f,  -85.292267f}, // south-west
    {5, 1, SPELL_ONY_BREATH_WEST_TO_EAST,   -33.5561f, -182.682f, -65.9457f, -88.945686f}, // west
    {6, 2, SPELL_ONY_BREATH_NW_TO_SE,         6.8951f, -180.246f, -65.896f,  -85.634293f}, // north-west
    {7, 3, SPELL_ONY_BREATH_NORTH_TO_SOUTH,  22.8763f, -217.152f, -65.0548f, -85.054054f}, // north
};

float const ClassicOnyxWhelpSpawnLocations[2][3] =
{
    { -30.127f, -254.463f, -89.440f},
    { -30.817f, -177.106f, -89.258f}
};
}

struct classic_boss_onyxia : public ScriptedAI
{
    classic_boss_onyxia(Creature* creature) : ScriptedAI(creature)
    {
        m_pInstance = creature->GetInstanceScript();
        m_uiMovePoint = 7;
        m_pPointData = GetMoveData();
    }

    uint32 m_uiPhase = ONY_PHASE_ONE;
    uint32 m_uiTransTimer = 0;
    uint32 m_uiTransCount = 0;
    bool m_bTransition = false;

    uint32 m_uiFlameBreathTimer = 0;
    uint32 m_uiCleaveTimer = 0;
    uint32 m_uiTailSweepTimer = 0;
    uint32 m_uiWingBuffetTimer = 0;
    uint32 m_uiKnockAwayTimer = 0;

    uint32 m_uiFireballTimer = 0;
    uint32 m_uiMovementTimer = 0;
    uint32 m_uiDeepBreathTimer = 0;
    bool   m_bDeepBreathIsCasting = false;
    bool   m_bBreathFlight = false;          // flying the Deep Breath pass to the opposite waypoint
    uint32 m_uiMovePoint = 7;

    ClassicOnyxMove const* m_pPointData = nullptr;

    uint32 m_uiSummonWhelpsTimer = 0;
    uint32 m_uiWhelpTimer = 0;
    uint8  m_uiSummonCount = 0;
    uint8  m_uiWhelpsToSummon = 16;
    bool   m_bIsSummoningWhelps = false;

    uint32 m_uiBellowingRoarTimer = 0;

    uint32 m_uiAggroRadiusTimer = 0;
    uint32 m_uiLeashCheckTimer = 0;
    uint32 m_uiSummonCheckTimer = 0;
    uint32 m_uiPhaseDiagTimer = 0;

    InstanceScript* m_pInstance;

    void Reset() override
    {
        m_uiPhase              = ONY_PHASE_ONE;
        m_bTransition          = false;
        m_uiTransTimer         = 0;
        m_uiTransCount         = 0;

        m_uiFlameBreathTimer   = urand(10000, 20000);
        m_uiCleaveTimer        = urand(2000, 5000);
        m_uiWingBuffetTimer    = urand(10000, 20000);
        m_uiKnockAwayTimer     = urand(15000, 25000);
        m_uiTailSweepTimer     = 5000;

        m_uiFireballTimer      = 3000;
        m_uiMovementTimer      = 20000;
        m_uiMovePoint          = 7; // set North as the initial Phase 2 waypoint
        m_pPointData           = GetMoveData();
        m_uiDeepBreathTimer    = 0;
        m_bDeepBreathIsCasting = false;
        m_bBreathFlight        = false;

        m_uiSummonWhelpsTimer  = 5000;
        m_uiWhelpTimer         = 1000;
        m_uiSummonCount        = 0;
        m_uiWhelpsToSummon     = 16;
        m_bIsSummoningWhelps   = false;

        m_uiBellowingRoarTimer = 10000;

        m_uiAggroRadiusTimer   = 5000;
        m_uiLeashCheckTimer    = 5000;
        m_uiSummonCheckTimer   = 5000;

        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_OL_DATA_ONYXIA_EVENT, NOT_STARTED);

        SetCombatMovement(true);
        me->SetCanMelee(true);
        me->SetSpeedRate(MOVE_RUN, ONYXIA_NORMAL_SPEED);

        // Daemon: back to "sleep" mode
        me->SetStandState(UNIT_STAND_STATE_SLEEP);
        me->SetCanFly(false);
        me->SetWalk(true);
        me->SetDisableGravity(false);
    }

    // VMaNGOS IsMoving(): spline movement in progress
    bool IsSplineMoving() const
    {
        return !me->movespline->Finalized();
    }

    void DelayEventIfNeed(uint32& event, uint32 delay)
    {
        if (event < (delay + 150)) // Time runs in increments of 150ms
            event = delay + 150;
    }

    void DelayCastEvents(uint32 delay)
    {
        DelayEventIfNeed(m_uiFlameBreathTimer, delay);
        DelayEventIfNeed(m_uiTailSweepTimer, delay);
        DelayEventIfNeed(m_uiCleaveTimer, delay);
        DelayEventIfNeed(m_uiWingBuffetTimer, delay);
        DelayEventIfNeed(m_uiKnockAwayTimer, delay);
    }

    void CheckForTargetsInAggroRadius(uint32 uiDiff)
    {
        // There is a grid-related bug preventing Onyxia from receiving MoveInLineOfSight calls from units in the front of her chamber, so use this check instead.
        if (m_uiAggroRadiusTimer < uiDiff)
            m_uiAggroRadiusTimer = 1000;
        else
        {
            m_uiAggroRadiusTimer -= uiDiff;
            return;
        }

        if (!me->IsInCombat() && !me->IsInEvadeMode())
        {
            for (MapReference const& ref : me->GetMap()->GetPlayers())
            {
                if (Player* player = ref.GetSource())
                {
                    if (player->IsAlive() && !player->IsGameMaster() && me->IsWithinDistInMap(player, ONYXIA_AGGRO_RANGE) && me->IsValidAttackTarget(player))
                    {
                        player->RemoveAurasByType(SPELL_AURA_MOD_STEALTH);
                        AttackStart(player);
                        DoZoneInCombat();
                        break;
                    }
                }
            }
        }
    }

    void LeashIfOutOfCombatArea(uint32 uiDiff)
    {
        if (m_uiLeashCheckTimer < uiDiff)
            m_uiLeashCheckTimer = 3500;
        else
        {
            m_uiLeashCheckTimer -= uiDiff;
            return;
        }

        if (me->GetPositionX() < -95.0f)
            EnterEvadeMode(EvadeReason::Boundary);
    }

    void SummonPlayerIfOutOfReach(uint32 uiDiff)
    {
        if (m_uiSummonCheckTimer < uiDiff)
            m_uiSummonCheckTimer = 3000;
        else
        {
            m_uiSummonCheckTimer -= uiDiff;
            return;
        }

        /** Teleport victim to the center of the chamber if too far away from Onyxia */
        if (Unit* pVictim = me->GetVictim())
        {
            if (IsOnyxiaFlying() && !IsSplineMoving())
            {
                if (pVictim->GetPositionX() < -105.0f)
                    pVictim->NearTeleportTo(-12.866907f, -216.626007f, -88.057808f, 0.0f);
            }
            else if (!IsOnyxiaFlying())
            {
                // TODO(classic): VMaNGOS also had a commented-out "target unreachable" check here
                if (me->GetDistance2d(pVictim) > 90.0f)
                    pVictim->NearTeleportTo(-12.866907f, -216.626007f, -88.057808f, 0.0f);
            }
        }
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        // Daemon: Fix orientation.
        me->SetStandState(UNIT_STAND_STATE_STAND);

        ClassicScriptText(SAY_ONY_AGGRO, me);
        DoZoneInCombat();
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_OL_DATA_ONYXIA_EVENT, IN_PROGRESS);

        std::list<Creature*> warderList;
        me->GetCreatureListWithEntryInGrid(warderList, CLASSIC_OL_NPC_ONYXIAN_WARDER, 200.0f);
        for (Creature* warder : warderList)
            if (!warder->IsAlive())
                warder->Respawn();
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_OL_DATA_ONYXIA_EVENT, DONE);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        std::list<Creature*> whelpList;
        me->GetCreatureListWithEntryInGrid(whelpList, CLASSIC_OL_NPC_ONYXIAN_WHELP, 200.0f);
        for (Creature* whelp : whelpList)
            whelp->DespawnOrUnsummon();

        ScriptedAI::EnterEvadeMode(why);
    }

    void JustSummoned(Creature* summoned) override
    {
        if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
            summoned->AI()->AttackStart(target);

        ++m_uiSummonCount;
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->GetTypeId() == TYPEID_PLAYER)
            if (roll_chance(50))
                ClassicScriptText(SAY_ONY_KILL, me);
    }

    bool IsOnyxiaFlying() const
    {
        return me->HasAura(SPELL_ONY_HOVER);
    }

    ClassicOnyxMove const* GetMoveData() const
    {
        for (ClassicOnyxMove const& data : ClassicOnyxMoveData)
            if (data.uiLocId == m_uiMovePoint)
                return &data;

        return nullptr;
    }

    void PhaseOne(uint32 uiDiff)
    {
        if (me->IsFlying())
        {
            me->SetWalk(true);
            me->SetCanFly(false);
            me->SetDisableGravity(false);
        }

        if (m_uiFlameBreathTimer < uiDiff)
        {
            if (DoCastVictim(SPELL_ONY_FLAMEBREATH) == SPELL_CAST_OK)
            {
                DelayCastEvents(2000); // 2 sec cast
                m_uiFlameBreathTimer = urand(10000, 20000);
            }
        }
        else
            m_uiFlameBreathTimer -= uiDiff;

        if (m_uiCleaveTimer < uiDiff)
        {
            if (DoCastVictim(SPELL_ONY_CLEAVE) == SPELL_CAST_OK)
                m_uiCleaveTimer = urand(2000, 5000);
        }
        else
            m_uiCleaveTimer -= uiDiff;

        if (m_uiWingBuffetTimer < uiDiff)
        {
            if (me->GetVictim() && me->IsWithinMeleeRange(me->GetVictim()))
            {
                if (DoCastVictim(SPELL_ONY_WINGBUFFET) == SPELL_CAST_OK)
                {
                    DelayCastEvents(1500);
                    m_uiWingBuffetTimer = urand(15000, 30000);
                }
            }
        }
        else
            m_uiWingBuffetTimer -= uiDiff;

        if (m_uiKnockAwayTimer < uiDiff)
        {
            if (Unit* victim = me->GetVictim())
            {
                if (me->IsWithinMeleeRange(victim))
                {
                    if (DoCastVictim(SPELL_ONY_KNOCK_AWAY) == SPELL_CAST_OK)
                    {
                        if (me->GetThreatManager().GetThreat(victim))
                            me->GetThreatManager().ModifyThreatByPercent(victim, -25);

                        DelayCastEvents(1500);
                        m_uiKnockAwayTimer = urand(15000, 30000);
                    }
                }
            }
        }
        else
            m_uiKnockAwayTimer -= uiDiff;

        if (m_uiTailSweepTimer < uiDiff)
        {
            if (DoCastSelf(SPELL_ONY_TAILSWEEP) == SPELL_CAST_OK)
                m_uiTailSweepTimer = 3500;
        }
        else
            m_uiTailSweepTimer -= uiDiff;

        // melee: TC master auto-melee
    }

    void PhaseTwo(uint32 uiDiff)
    {
        if (m_uiMovementTimer < uiDiff)
        {
            m_uiMovementTimer = urand(15000, 25000);
            m_uiFireballTimer = 5000;
            if (DoMovement())
            {
                // casting Deep Breath
                m_uiMovementTimer = urand(20000, 25000);
                m_uiFireballTimer = 10000;
            }
        }
        else
            m_uiMovementTimer -= uiDiff;

        if (m_bDeepBreathIsCasting)
        {
            if (m_uiDeepBreathTimer <= uiDiff)
            {
                me->SetSpeedRate(MOVE_RUN, ONYXIA_BREATH_SPEED);
                if (m_pPointData)
                {
                    me->GetMotionMaster()->MovePoint(m_pPointData->uiLocId, m_pPointData->fX, m_pPointData->fY, m_pPointData->fZ, false);
                    m_bBreathFlight = true;
                }
                // heat up egg pit during Deep Breath
                me->CastSpell(me, SPELL_ONY_HEATED_GROUND_EAST, true);
                me->CastSpell(me, SPELL_ONY_HEATED_GROUND_WEST, true);
                m_bDeepBreathIsCasting = false;
                m_uiDeepBreathTimer = 0;
            }
            else
                m_uiDeepBreathTimer -= uiDiff;
        }

        if (m_uiFireballTimer < uiDiff)
        {
            if (m_uiMovementTimer > 3500 && !IsSplineMoving() && !m_bDeepBreathIsCasting)
            {
                if (Unit* target = me->GetVictim())
                {
                    if (DoCast(target, SPELL_ONY_FIREBALL) == SPELL_CAST_OK)
                    {
                        if (me->GetThreatManager().GetThreat(target))
                            me->GetThreatManager().ModifyThreatByPercent(target, -100);
                        m_uiFireballTimer = 3000;
                    }
                }
            }
        }
        else
            m_uiFireballTimer -= uiDiff;

        if (m_bIsSummoningWhelps)
        {
            if (m_uiSummonCount < m_uiWhelpsToSummon)
            {
                if (m_uiWhelpTimer < uiDiff)
                {
                    me->SummonCreature(CLASSIC_OL_NPC_ONYXIAN_WHELP, ClassicOnyxWhelpSpawnLocations[0][0], ClassicOnyxWhelpSpawnLocations[0][1], ClassicOnyxWhelpSpawnLocations[0][2], 0.0f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 300s);
                    me->SummonCreature(CLASSIC_OL_NPC_ONYXIAN_WHELP, ClassicOnyxWhelpSpawnLocations[1][0], ClassicOnyxWhelpSpawnLocations[1][1], ClassicOnyxWhelpSpawnLocations[1][2], 0.0f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 300s);
                    m_uiWhelpTimer = 1000;
                }
                else
                    m_uiWhelpTimer -= uiDiff;
            }
            else
            {
                m_bIsSummoningWhelps = false;
                m_uiSummonCount = 0;
                m_uiWhelpsToSummon = 5 + urand(0, 2);
                m_uiSummonWhelpsTimer = 30000;
            }
        }
        else
        {
            if (m_uiSummonWhelpsTimer < uiDiff)
                m_bIsSummoningWhelps = true;
            else
                m_uiSummonWhelpsTimer -= uiDiff;
        }
    }

    bool DoMovement()
    {
        me->InterruptNonMeleeSpells(false);
        m_pPointData = GetMoveData();

        uint32 roll = urand(0, 99);
        if (roll < 35)          // Move clockwise
        {
            m_uiMovePoint = (m_uiMovePoint + 1) % 8;
        }
        else if (roll < 70)     // Move counter-clockwise
        {
            m_uiMovePoint = (m_uiMovePoint + 8 - 1) % 8;
        }
        else                    // Deep Breath
        {
            m_uiMovePoint = (m_uiMovePoint + 4) % 8;
            ClassicScriptText(EMOTE_ONY_BREATH, me);

            m_bDeepBreathIsCasting = true;
            // The breath spell has a 5 s cast (SpellAudit: 17086 cast 5000 ms): the wait before she flies is that cast, and its damage lands
            // when it completes. Cast as a plain triggered spell (instant) it hit the raid at once, before anyone could see the warning
            // (onyxia_full n24: 19 and 11 raiders dead in the second she began the wait, every time). She holds still for the cast and the
            // flight starts just after it.
            m_uiDeepBreathTimer = 5500;
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveIdle();
            me->StopMoving();
            if (m_pPointData)
                me->CastSpell(me, m_pPointData->uiSpellId, CastSpellExtraArgs(TriggerCastFlags(TRIGGERED_FULL_MASK & ~(TRIGGERED_IGNORE_CAST_TIME | TRIGGERED_CAST_DIRECTLY))));
            // face destination and clear target
            m_pPointData = GetMoveData();
            if (m_pPointData)
                me->SetFacingTo(me->GetAbsoluteAngle(m_pPointData->fX, m_pPointData->fY));
            me->SetTarget(ObjectGuid::Empty);
            return true;
        }

        m_pPointData = GetMoveData();
        me->SetSpeedRate(MOVE_RUN, ONYXIA_NORMAL_SPEED);
        if (m_pPointData)
            me->GetMotionMaster()->MovePoint(m_pPointData->uiLocId, m_pPointData->fX, m_pPointData->fY, m_pPointData->fZ, false);
        return false;
    }

    void PhaseThree(uint32 uiDiff)
    {
        if (m_uiBellowingRoarTimer < uiDiff)
        {
            // VMaNGOS: CF_INTERRUPT_PREVIOUS
            me->InterruptNonMeleeSpells(false);
            if (DoCastSelf(SPELL_ONY_BELLOWINGROAR) == SPELL_CAST_OK)
            {
                m_uiBellowingRoarTimer = urand(15000, 30000);
                // Do not be interrupted by other casts.
                DelayCastEvents(2000);
            }
        }
        else
            m_uiBellowingRoarTimer -= uiDiff;

        if (m_uiSummonWhelpsTimer < uiDiff)
        {
            uint32 const spawnIdx = urand(0, 1);
            me->SummonCreature(CLASSIC_OL_NPC_ONYXIAN_WHELP, ClassicOnyxWhelpSpawnLocations[spawnIdx][0], ClassicOnyxWhelpSpawnLocations[spawnIdx][1], ClassicOnyxWhelpSpawnLocations[spawnIdx][2], 0.0f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 120s);
            m_uiSummonWhelpsTimer = urand(1000, 10000);
        }
        else
            m_uiSummonWhelpsTimer -= uiDiff;

        PhaseOne(uiDiff);
    }

    void PhaseTransition(uint32 uiDiff, bool bDebut)
    {
        me->ClearUnitState(UNIT_STATE_MELEE_ATTACKING);

        /** P2 Event to take off */
        if (m_uiPhase == ONY_PHASE_TWO)
        {
            /** Stop combat and move to the first waypoint before taking off */
            if (bDebut && m_uiTransCount == 0)
            {
                SetCombatMovement(false);
                me->SetCanMelee(false); // VMaNGOS does not call DoMeleeAttackIfReady in phase 2
                ClassicScriptText(SAY_ONY_PHASE_2_TRANS, me);
                me->InterruptNonMeleeSpells(false);

                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MoveIdle();

                me->SetSpeedRate(MOVE_RUN, ONYXIA_NORMAL_SPEED);
                me->GetMotionMaster()->MovePoint(POINT_ONY_DEPART_FLIGHT, -57.750641f, -215.610077f, -85.094727f, true);
                m_uiTransTimer = 60000; // handled by MovementInform
            }
            /** Take off in progress */
            else if (m_uiTransTimer < uiDiff && m_uiTransCount == 1)
            {
                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MoveIdle();
                me->GetMotionMaster()->MovePoint(0, -57.204933f, -215.592148f, -85.156929f, false);
                m_uiTransTimer = 2000;
                m_uiTransCount = 2;
            }
            /** Fly mode added, move to the first waypoint */
            else if (m_uiTransTimer < uiDiff && m_uiTransCount == 2)
            {
                me->CastSpell(me, SPELL_ONY_HOVER, true); /** Start flying */
                m_bTransition = false;
                m_uiTransTimer = 0;

                m_pPointData = GetMoveData();
                if (m_pPointData)
                    me->GetMotionMaster()->MovePoint(m_pPointData->uiLocId, m_pPointData->fX, m_pPointData->fY, m_pPointData->fZ, false);
            }
        }
        /** P3 event to land */
        else if (m_uiPhase == ONY_PHASE_THREE)
        {
            /** Fly to the landing waypoint */
            if (bDebut && m_uiTransCount == 2)
            {
                ResetThreatList();
                ClassicScriptText(SAY_ONY_PHASE_3_TRANS, me);
                me->SetSpeedRate(MOVE_RUN, ONYXIA_NORMAL_SPEED);
                me->InterruptNonMeleeSpells(false);

                if (me->GetPositionX() < -40.0f)
                    me->GetMotionMaster()->MovePoint(POINT_ONY_LANDING_FLIGHT, -59.895f, -214.876f, -84.855f, false); // South
                else
                    me->GetMotionMaster()->MovePoint(POINT_ONY_LANDING_FLIGHT, -8.86f, -212.752f, -88.542f, false);   // North

                me->RemoveAurasDueToSpell(SPELL_ONY_HOVER); /** Stop flying */
                m_uiTransTimer = 60000; // handled by MovementInform
            }
            /** Landing in progress */
            else if (m_uiTransTimer < uiDiff && m_uiTransCount == 3)
            {
                me->GetMotionMaster()->MovePoint(0, -8.860f, -212.752f, -87.482f, false);
                m_uiTransTimer = 2000;
                m_uiTransCount = 4;
            }
            /** Landed. Restore target and start combat movement.*/
            else if (m_uiTransTimer < uiDiff && m_uiTransCount == 4)
            {
                if (Unit* pVictim = me->GetVictim())
                    me->SetTarget(pVictim->GetGUID());

                SetCombatMovement(true);
                me->SetCanMelee(true);
                if (Unit* pVictim = me->GetVictim())
                    me->GetMotionMaster()->MoveChase(pVictim);

                m_bTransition  = false;
                m_uiTransTimer = 0;

                m_uiFlameBreathTimer   = urand(10000, 15000);
                m_uiTailSweepTimer     = 5000;
                m_uiCleaveTimer        = urand(2000, 5000);
                m_uiWingBuffetTimer    = urand(10000, 20000);
                m_uiKnockAwayTimer     = urand(10000, 20000);
            }
        }

        if (m_uiTransTimer >= uiDiff)
            m_uiTransTimer -= uiDiff;
    }

    void MovementInform(uint32 uiType, uint32 uiPointId) override
    {
        if (uiType != POINT_MOTION_TYPE)
            return;

        if (m_uiPhase == ONY_PHASE_TWO)
            TC_LOG_INFO("scripts.classic", "Onyxia reached point {} at {:.1f}% (waypoint {}, breath flight {})", uiPointId, me->GetHealthPct(),
                m_pPointData ? int32(m_pPointData->uiLocId) : -1, m_bBreathFlight);

        // restore Onyxia's target after movement in Phase 2
        if (m_pPointData && uiPointId == m_pPointData->uiLocId)
        {
            m_bBreathFlight = false;
            if (Unit* pVictim = me->GetVictim())
                me->SetTarget(pVictim->GetGUID());
        }

        switch (uiPointId)
        {
            case POINT_ONY_DEPART_FLIGHT:
                me->SetFacingTo(0.0f);
                m_uiTransTimer = 1000;
                m_uiTransCount = 1;
                me->SetCanFly(true);
                me->SetDisableGravity(true);
                me->HandleEmoteCommand(EMOTE_ONESHOT_LIFTOFF);
                break;
            case POINT_ONY_LANDING_FLIGHT:
                me->SetFacingTo(0.0f);
                m_uiTransTimer = 1000;
                m_uiTransCount = 3;
                me->SetCanFly(false);
                me->SetDisableGravity(false);
                me->HandleEmoteCommand(EMOTE_ONESHOT_LAND);
                me->CastSpell(me, SPELL_ONY_BELLOWINGROAR, true);
                m_uiBellowingRoarTimer = urand(15000, 30000);
                break;
            default:
                break;
        }
    }

    void UpdateAI(uint32 uiDiff) override
    {
        CheckForTargetsInAggroRadius(uiDiff);

        if (!UpdateVictim())
            return;

        /** whenever Onyxia is moving to a waypoint or casting Deep Breath, clear her target */
        if (m_bTransition || m_bDeepBreathIsCasting || (m_uiPhase == ONY_PHASE_TWO && IsSplineMoving()))
            me->SetTarget(ObjectGuid::Empty);

        if (m_bTransition)
        {
            PhaseTransition(uiDiff, false);
            return;
        }

        LeashIfOutOfCombatArea(uiDiff);

        SummonPlayerIfOutOfReach(uiDiff);

        // What holds the landing back below 40 percent, every 2 s (scripts.classic; onyxia_full 245 still landed at about 28 percent).
        if (m_uiPhase == ONY_PHASE_TWO && me->GetHealthPct() < 40.0f)
        {
            if (m_uiPhaseDiagTimer <= uiDiff)
            {
                m_uiPhaseDiagTimer = 2000;
                TC_LOG_INFO("scripts.classic", "Onyxia phase 2 at {:.1f}%: breath cast {}, breath flight {}, spline moving {}, generator {}, move point {}, movement timer {} ms, pos {:.1f},{:.1f},{:.1f}",
                    me->GetHealthPct(), m_bDeepBreathIsCasting, m_bBreathFlight, IsSplineMoving(), uint32(me->GetMotionMaster()->GetCurrentMovementGeneratorType()),
                    m_uiMovePoint, m_uiMovementTimer, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());
            }
            else
                m_uiPhaseDiagTimer -= uiDiff;
        }

        /** Switch to P3 */
        // Only a Deep Breath (its cast and the pass after it) holds the landing back; an ordinary flight between two waypoints is replaced by
        // the landing flight. Waiting for her to stop moving at all let her fly on and breathe again for a minute below 40 percent: in phase 2
        // she is flying or casting most of the time, and the gap between two moves is often too short or none.
        if (me->GetHealthPct() < 40.0f && m_uiPhase == ONY_PHASE_TWO && !m_bDeepBreathIsCasting && !m_bBreathFlight)
        {
            TC_LOG_INFO("scripts.classic", "Onyxia lands for phase 3 at {:.1f}% (spline moving {})", me->GetHealthPct(), IsSplineMoving());
            m_uiPhase = ONY_PHASE_THREE;
            m_bTransition = true;
            PhaseTransition(0, true);
            return;
        }
        /** Switch to P2 */
        else if (me->GetHealthPct() < 65.0f && m_uiPhase == ONY_PHASE_ONE)
        {
            m_uiPhase = ONY_PHASE_TWO;
            m_bTransition = true;
            PhaseTransition(0, true);
            return;
        }

        switch (m_uiPhase)
        {
            case ONY_PHASE_ONE:
                PhaseOne(uiDiff);
                break;
            case ONY_PHASE_TWO:
                PhaseTwo(uiDiff);
                break;
            case ONY_PHASE_THREE:
                PhaseThree(uiDiff);
                break;
            default:
                break;
        }
    }
};

struct classic_npc_onyxian_whelp : public ScriptedAI
{
    classic_npc_onyxian_whelp(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override { }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;
        // melee: TC master auto-melee
    }
};

void AddSC_classic_boss_onyxia()
{
    RegisterCreatureAI(classic_boss_onyxia);
    RegisterCreatureAI(classic_npc_onyxian_whelp);
}
