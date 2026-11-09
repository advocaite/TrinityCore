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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/eastern_plaguelands/naxxramas/boss_sapphiron.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: boss_sapphiron, npc_sapphiron_blizzard, spell_sapphiron_life_drain (28542), go_sapphiron_birth
// todo: Make sure he's immune to frost damage.

#include "ScriptMgr.h"
#include "Containers.h"
#include "Creature.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "ThreatManager.h"
#include "classic_naxxramas.h"
#include "classic_script_text.h"
#include <algorithm>
#include <cmath>
#include <list>
#include <vector>

namespace
{
enum ClassicNaxxSapphironData : uint32
{
    EMOTE_SAPPH_BREATH       = 7213,
    EMOTE_SAPPH_ENRAGE       = 2384,

    SPELL_SAPPH_ICEBOLT       = 28522,
    SPELL_SAPPH_STUN_IMMUNE   = 28782,

    // SPELL_SUMM_ICEBLOCK = 28535, // we manually summon an iceblock in SpellHitTarget

    //SPELL_FROST_BREATH = 29318,
    SPELL_SAPPH_FROST_BREATH        = 28524, // triggers the damage and explosion visual, 7sec cast
    SPELL_SAPPH_FROST_BREATH_DUMMY  = 30101, // shows the falling ball thing

    SPELL_SAPPH_FROST_AURA   = 28529,
    SPELL_SAPPH_LIFE_DRAIN   = 28542,
    SPELL_SAPPH_BESERK       = 26662,
    SPELL_SAPPH_CLEAVE       = 19983,
    SPELL_SAPPH_TAIL_SWEEP   = 15847,

    // each blizzard has 30sec duration supposedly
    SPELL_SAPPH_SUMMON_BLIZ1      = 28561, // summons creature 16474
    SPELL_SAPPH_SUMMON_BLIZ2      = 28560, // unimplemented script effect
    SPELL_SAPPH_BLIZZARD_PERIODIC = 28534, // triggers 28547 every 3 second
    SPELL_SAPPH_BLIZZARD          = 28547, // deals dmg every 2 seconds. Stationary, lasts for 15sec

    SPELL_SAPPH_PERIODIC_BUFFET   = 29327, // periodically does 29328
    SPELL_SAPPH_WING_BUFFET       = 29328, // is it the spell he does on takeoff, or another one?

    SPELL_SAPPH_SAPPHIRON_DIES    = 29357, // adds camera-shake.

    SPELL_SAPPH_UNK_17131         = 17131, // removed on reset / landing

    GO_SAPPH_ICEBLOCK = 181247,

    MOVE_POINT_SAPPH_LIFTOFF = 1,
    MOVE_POINT_SAPPH_FLYPOINT = 2,

    NPC_SAPPH_WING_BUFFET = 17025,
    NPC_SAPPH_BLIZZARD = 16474
};

enum ClassicNaxxSapphironEvents : uint32
{
    EVENT_SAPPH_MOVE_TO_FLY = 1,
    EVENT_SAPPH_LIFTOFF = 2,
    EVENT_SAPPH_LAND = 3,
    EVENT_SAPPH_LANDED = 4,
    EVENT_SAPPH_ICEBOLT = 5,
    EVENT_SAPPH_BLIZZARD = 6,
    EVENT_SAPPH_LIFEDRAIN = 7,
    EVENT_SAPPH_TAIL_SWEEP = 8,
    EVENT_SAPPH_CLEAVE = 9,
    EVENT_SAPPH_FROST_BREATH_DUMMY = 10,
    EVENT_SAPPH_FROST_BREATH_CAST = 11,
    EVENT_SAPPH_CHECK_EVADE = 12
};

enum ClassicNaxxSapphironPhase
{
    PHASE_SAPPH_GROUND     = 1,
    PHASE_SAPPH_LIFT_OFF   = 2,
    PHASE_SAPPH_AIR_BOLTS  = 3,
    PHASE_SAPPH_AIR_BREATH = 4,
    PHASE_SAPPH_LANDING    = 5
};

float const ClassicSapphLiftOffPosition[3] = { 3521.300f, -5237.560f, 138.261f };
}

struct classic_boss_sapphiron : public ScriptedAI
{
    classic_boss_sapphiron(Creature* creature) : ScriptedAI(creature), m_pInstance(GetClassicNaxxInstance(creature)) { }

    classic_instance_naxxramas_InstanceScript* m_pInstance;

    uint32 Icebolt_Count = 0;

    EventMap events;
    ClassicNaxxSapphironPhase phase = PHASE_SAPPH_GROUND;
    uint32 berserkTimer = 900000;
    std::vector<ObjectGuid> iceboltTargets;
    ObjectGuid wingBuffetCreature;

    void Reset() override
    {
        phase = PHASE_SAPPH_GROUND;
        events.Reset();
        berserkTimer = 900000; // 15 min
        me->RemoveAurasDueToSpell(SPELL_SAPPH_FROST_AURA);
        me->RemoveAurasDueToSpell(SPELL_SAPPH_UNK_17131);
        UnSummonWingBuffet();
        DeleteAndDispellIceBlocks();
        setHover(false, true);
        SetCombatMovement(true);

        if (m_pInstance && m_pInstance->GetData(TYPE_SAPPHIRON) != DONE)
            m_pInstance->SetData(TYPE_SAPPHIRON, NOT_STARTED);
    }

    void UnSummonWingBuffet()
    {
        if (!wingBuffetCreature.IsEmpty())
        {
            if (me->IsInWorld())
                if (Creature* pC = me->GetMap()->GetCreature(wingBuffetCreature))
                    if (TempSummon* tmpS = pC->ToTempSummon())
                        tmpS->UnSummon();
            wingBuffetCreature.Clear();
        }
    }

    void DeleteAndDispellIceBlocks()
    {
        if (!me->IsInWorld())
            return;

        std::list<GameObject*> iceblocks;
        me->GetGameObjectListWithEntryInGrid(iceblocks, GO_SAPPH_ICEBLOCK, 300.0f);
        for (GameObject* ib : iceblocks)
            ib->DespawnOrUnsummon();
    }

    // TODO(classic): VMaNGOS DamageTaken() stops melee attacks of attackers while his melee Z limit is lowered (air
    // phase, SetMeleeZLimit(0)). TC has no per-creature melee Z limit; not ported.

    void AttackStart(Unit* who) override
    {
        if (phase != PHASE_SAPPH_GROUND)
            return;

        ScriptedAI::AttackStart(who);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (phase != PHASE_SAPPH_GROUND)
            return;

        if (m_pInstance)
            m_pInstance->SetData(TYPE_SAPPHIRON, IN_PROGRESS);

        events.ScheduleEvent(EVENT_SAPPH_LIFEDRAIN, 12s);
        events.ScheduleEvent(EVENT_SAPPH_BLIZZARD, 10s);
        events.ScheduleEvent(EVENT_SAPPH_MOVE_TO_FLY, 40s);
        events.ScheduleEvent(EVENT_SAPPH_TAIL_SWEEP, 12s);
        events.ScheduleEvent(EVENT_SAPPH_CLEAVE, 5s);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoCastSelf(SPELL_SAPPH_SAPPHIRON_DIES, true);
        UnSummonWingBuffet();
        DeleteAndDispellIceBlocks();
        if (m_pInstance)
            m_pInstance->SetData(TYPE_SAPPHIRON, DONE);
    }

    void RescheduleIcebolt()
    {
        if (++Icebolt_Count < 5)
            events.Repeat(3500ms);
        else
            events.ScheduleEvent(EVENT_SAPPH_FROST_BREATH_DUMMY, 1500ms);
    }

    void DoIceBolt()
    {
        if (me->GetThreatManager().GetThreatListSize() <= iceboltTargets.size())
        {
            RescheduleIcebolt();
            return;
        }

        std::vector<Unit*> suitableUnits;
        for (ThreatReference const* ref : me->GetThreatManager().GetUnsortedThreatList())
        {
            if (Player* pTarget = ref->GetVictim()->ToPlayer())
            {
                if (!pTarget->IsAlive())
                    continue;

                if (std::find(iceboltTargets.begin(), iceboltTargets.end(), pTarget->GetGUID()) != iceboltTargets.end())
                    continue;

                suitableUnits.push_back(pTarget);
            }
        }
        if (suitableUnits.empty())
        {
            RescheduleIcebolt();
            return;
        }

        Unit* target = suitableUnits[urand(0, suitableUnits.size() - 1)];

        iceboltTargets.push_back(target->GetGUID());
        me->SetFacingToObject(target);
        DoCast(target, SPELL_SAPPH_ICEBOLT, true);

        RescheduleIcebolt();
    }

    void MovementInform(uint32 /*uiType*/, uint32 pointId) override
    {
        if (pointId == MOVE_POINT_SAPPH_LIFTOFF && phase == PHASE_SAPPH_LIFT_OFF)
        {
            events.ScheduleEvent(EVENT_SAPPH_LIFTOFF, 250ms);
        }
    }

    void setHover(bool on, bool onReset = false)
    {
        if (on)
        {
            me->InterruptNonMeleeSpells(false);
            me->AttackStop();
            me->RemoveAllAttackers();
            SetCombatMovement(false);
            me->SetReactState(REACT_PASSIVE);

            me->HandleEmoteCommand(EMOTE_ONESHOT_LIFTOFF);
            me->SetHover(true);

            // TODO(classic): VMaNGOS also resets m_targetNotReachableTimer, clears TEMPFACTION_RESTORE_COMBAT_STOP
            // temporary factions and sets its melee Z limit to 0 (no TC equivalent).
        }
        else
        {
            me->RemoveAurasDueToSpell(SPELL_SAPPH_UNK_17131);
            if (me->IsHovering())
                me->HandleEmoteCommand(EMOTE_ONESHOT_LAND);

            me->SetHover(false);

            if (!onReset)
                me->SetReactState(REACT_AGGRESSIVE);
            // VMaNGOS SetMeleeZLimit(UNIT_DEFAULT_MELEE_Z_LIMIT): no TC equivalent
        }
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (phase == PHASE_SAPPH_GROUND)
        {
            if (!UpdateVictim())
                return;

            // TODO(classic): VMaNGOS UpdateReachable(): evades after 10s of an unreachable (chase) target, and forces a
            // UNIT_FIELD_TARGET update 1s after spawning (client target display hack). Not ported.
        }
        else
        {
            if (me->GetThreatManager().IsThreatListEmpty())
            {
                EnterEvadeMode(EvadeReason::NoHostiles);
                return;
            }
        }

        if (!me->HasAura(SPELL_SAPPH_FROST_AURA))
            DoCastSelf(SPELL_SAPPH_FROST_AURA, true);

        events.Update(uiDiff);

        // VMaNGOS executes at most one event per update
        if (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SAPPH_MOVE_TO_FLY: // MovementInform() will trigger liftoff after this
                    // He does not lift below 10%
                    if (me->GetHealthPct() > 10.0f)
                    {
                        events.Reset();
                        me->ClearUnitState(UNIT_STATE_MELEE_ATTACKING);
                        me->InterruptNonMeleeSpells(false);
                        me->GetMotionMaster()->Clear();
                        me->GetMotionMaster()->MoveIdle();
                        me->GetMotionMaster()->MovePoint(MOVE_POINT_SAPPH_LIFTOFF, ClassicSapphLiftOffPosition[0], ClassicSapphLiftOffPosition[1], ClassicSapphLiftOffPosition[2]);
                        phase = PHASE_SAPPH_LIFT_OFF;
                        me->SetTarget(ObjectGuid::Empty);
                    }
                    break;
                case EVENT_SAPPH_LIFTOFF: // liftoff is triggered from MovementInform()
                {
                    phase = PHASE_SAPPH_AIR_BOLTS;
                    Icebolt_Count = 0;
                    events.ScheduleEvent(EVENT_SAPPH_ICEBOLT, 6s);

                    if (Creature* pWG = me->SummonCreature(NPC_SAPPH_WING_BUFFET, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0, TEMPSUMMON_MANUAL_DESPAWN))
                    {
                        pWG->CastSpell(pWG, SPELL_SAPPH_PERIODIC_BUFFET, true);
                        wingBuffetCreature = pWG->GetGUID();
                    }

                    setHover(true);
                    break;
                }
                case EVENT_SAPPH_LAND:
                {
                    iceboltTargets.clear();
                    // in case something is delayed, and we're not finished
                    // casting the frost breath
                    if (me->IsNonMeleeSpellCast(false))
                    {
                        events.Repeat(100ms);
                        return;
                    }
                    setHover(false);
                    //m_creature->GetMotionMaster()->MovePoint(MOVE_POINT_FLYPOINT, m_creature->GetPositionX(), m_creature->GetPositionY(), 137.7f, MOVE_PATHFINDING | MOVE_FLY_MODE);
                    phase = PHASE_SAPPH_LANDING;
                    events.ScheduleEvent(EVENT_SAPPH_LANDED, 4s);
                    break;
                }
                case EVENT_SAPPH_LANDED:
                {
                    DeleteAndDispellIceBlocks();
                    events.Reset();
                    events.ScheduleEvent(EVENT_SAPPH_LIFEDRAIN, 3s);
                    events.ScheduleEvent(EVENT_SAPPH_BLIZZARD, 1s);
                    events.ScheduleEvent(EVENT_SAPPH_MOVE_TO_FLY, Seconds(urand(50, 70))); // Sampling videos show its 50-70sec between engaging after landing, and disengaging to fly again
                    events.ScheduleEvent(EVENT_SAPPH_TAIL_SWEEP, 12s);
                    events.ScheduleEvent(EVENT_SAPPH_CLEAVE, 5s);

                    SetCombatMovement(true);
                    me->GetMotionMaster()->Clear();
                    // VMaNGOS SelectHostileTarget(): the victim is picked up by UpdateVictim() on the next update
                    phase = PHASE_SAPPH_GROUND;
                    break;
                }
                case EVENT_SAPPH_ICEBOLT:
                {
                    DoIceBolt();
                    break;
                }
                case EVENT_SAPPH_FROST_BREATH_DUMMY:
                {
                    // Looks like the wing buffet dissapears as he starts casting frost breath
                    UnSummonWingBuffet();
                    if (DoCastSelf(SPELL_SAPPH_FROST_BREATH_DUMMY, true) == SPELL_CAST_OK)
                        events.ScheduleEvent(EVENT_SAPPH_FROST_BREATH_CAST, 500ms);
                    else
                        events.Repeat(100ms);
                    break;
                }
                case EVENT_SAPPH_FROST_BREATH_CAST:
                {
                    if (DoCastSelf(SPELL_SAPPH_FROST_BREATH) != SPELL_CAST_OK)
                        events.Repeat(100ms);
                    else
                        events.ScheduleEvent(EVENT_SAPPH_LAND, 7000ms);
                    break;
                }
                case EVENT_SAPPH_BLIZZARD:
                {
                    if (Unit* pUnit = SelectTarget(SelectTargetMethod::Random, 0, [](Unit* u)
                        {
                            Player* p = u->ToPlayer();
                            return p && !p->IsGameMaster();
                        }))
                    {
                        float const angle = frand(0.0f, float(M_PI) * 2);
                        float const x = pUnit->GetPositionX() + std::cos(angle) * 5.0f;
                        float const y = pUnit->GetPositionY() + std::sin(angle) * 5.0f;
                        if (!me->SummonCreature(NPC_SAPPH_BLIZZARD, x, y, 138.0f, 0, TEMPSUMMON_TIMED_DESPAWN, 30000ms))
                            events.Repeat(100ms);
                        else
                            events.Repeat(20s);
                    }
                    break;
                }
                case EVENT_SAPPH_LIFEDRAIN:
                {
                    if (DoCastSelf(SPELL_SAPPH_LIFE_DRAIN) == SPELL_CAST_OK)
                        events.Repeat(24s);
                    else
                        events.Repeat(100ms);
                    break;
                }
                case EVENT_SAPPH_TAIL_SWEEP:
                {
                    if (DoCastSelf(SPELL_SAPPH_TAIL_SWEEP) == SPELL_CAST_OK)
                        events.Repeat(Seconds(urand(7, 10)));
                    else
                        events.Repeat(100ms);
                    break;
                }
                case EVENT_SAPPH_CLEAVE:
                {
                    if (DoCastVictim(SPELL_SAPPH_CLEAVE) == SPELL_CAST_OK)
                        events.Repeat(Seconds(urand(5, 10)));
                    else
                        events.Repeat(100ms);
                    break;
                }
                default:
                    break;
            }
        }

        // Enrage can happen in any phase
        if (berserkTimer < uiDiff)
        {
            if (DoCastSelf(SPELL_SAPPH_BESERK) == SPELL_CAST_OK)
            {
                ClassicScriptText(EMOTE_SAPPH_ENRAGE, me);
                berserkTimer = 300000;
            }
        }
        else
            berserkTimer -= uiDiff;
    }
};

struct classic_npc_sapphiron_blizzard : public ScriptedAI
{
    classic_npc_sapphiron_blizzard(Creature* creature) : ScriptedAI(creature), m_pInstance(GetClassicNaxxInstance(creature))
    {
        me->SetWanderDistance(60.0f);
        me->SetReactState(REACT_PASSIVE);
        events.ScheduleEvent(1, 10ms);
    }

    EventMap events;
    classic_instance_naxxramas_InstanceScript* m_pInstance;
    std::vector<ObjectGuid> previousTargets;

    void Reset() override { }
    void JustAppeared() override { }
    void AttackStart(Unit* /*who*/) override { }
    void MoveInLineOfSight(Unit* /*who*/) override { }
    void JustEngagedWith(Unit* /*who*/) override { }

    void MovementInform(uint32 /*uiType*/, uint32 /*pointId*/) override
    {
        events.Reset();
        PickNewTarget();
        events.ScheduleEvent(1, 8s);
    }

    void SetRandomMove()
    {
        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() != RANDOM_MOTION_TYPE)
        {
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveRandom(me->GetWanderDistance());
        }
    }

    void PickNewTarget()
    {
        // if no sapphiron, move random
        Creature* pSapp = nullptr;
        if (m_pInstance)
            pSapp = m_pInstance->GetSingleCreatureFromStorage(NPC_SAPPHIRON);

        if (!pSapp)
        {
            SetRandomMove();
            return;
        }

        // if only "tank" alive, move random
        if (pSapp->GetThreatManager().GetThreatListSize() < 2)
        {
            SetRandomMove();
            return;
        }

        std::vector<Unit*> suitableUnits;
        bool first = true;
        for (ThreatReference const* ref : pSapp->GetThreatManager().GetSortedThreatList())
        {
            if (first) // skip tank
            {
                first = false;
                continue;
            }

            if (Player* pTarget = ref->GetVictim()->ToPlayer())
            {
                if (std::find(previousTargets.begin(), previousTargets.end(), pTarget->GetGUID()) != previousTargets.end())
                    continue;
                // want to encourage the blizzard to move towards a semi-far-away target to make it spread out
                if (me->GetExactDist(pTarget) < 15.0f)
                    continue;
                suitableUnits.push_back(pTarget);
            }
        }
        // when no suitable units found, we move random, this will generally only happen right before a wipe
        if (suitableUnits.empty())
        {
            SetRandomMove();
            return;
        }

        Unit* target = suitableUnits[urand(0, suitableUnits.size() - 1)];
        previousTargets.push_back(target->GetGUID());
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MovePoint(1, target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
    }

    void UpdateAI(uint32 uiDiff) override
    {
        events.Update(uiDiff);

        if (events.ExecuteEvent())
        {
            PickNewTarget();
            events.Repeat(Seconds(urand(8, 10)));
        }
    }
};

// 28542 - Life Drain (Naxx, Sapphiron)
class classic_spell_sapphiron_life_drain : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Trinity::Containers::RandomResize(targets, urand(7, 10));
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(classic_spell_sapphiron_life_drain::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// 181356 - Sapphiron Birth (trap)
struct classic_go_sapphiron_birth : public GameObjectAI
{
    classic_instance_naxxramas_InstanceScript* m_pInstance;

    explicit classic_go_sapphiron_birth(GameObject* go) : GameObjectAI(go), m_pInstance(GetClassicNaxxInstance(go)) { }

    // VMaNGOS GameObjectAI::OnUse(Unit*). The birth is a trap with no spell (radius 80, one charge): GameObject::Update triggers it by
    // its loot state (GO_READY -> GO_ACTIVATED with the player found in reach) and never calls Use, so OnGossipHello alone never ran and
    // Sapphiron was never summoned (he has no creature row; upstream go_sapphiron_birth answers the same hook).
    void OnLootStateChanged(uint32 state, Unit* unit) override
    {
        if (state == GO_ACTIVATED && unit)
            if (Player* player = unit->ToPlayer())
                Birth(player);
    }

    bool OnGossipHello(Player* user) override
    {
        return Birth(user);
    }

    bool Birth(Player* user)
    {
        if (!user || !user->IsAlive() || user->IsGameMaster())
            return false;

        if (!m_pInstance)
            return false;

        // Only trigger if encounter hasn't started yet
        if (m_pInstance->GetData(TYPE_SAPPHIRON) != NOT_STARTED)
            return false;

        // Don't trigger if Sapphiron already exists
        if (m_pInstance->GetSingleCreatureFromStorage(NPC_SAPPHIRON))
            return false;

        m_pInstance->SetData(TYPE_SAPPHIRON, SPECIAL);

        // Play bones animation that construct Sapphiron
        // It appears, when you assign a cpp script, need to call this manually
        me->SendGameObjectDespawn();

        return true;
    }
};

void AddSC_classic_boss_sapphiron()
{
    RegisterCreatureAI(classic_boss_sapphiron);
    RegisterCreatureAI(classic_npc_sapphiron_blizzard);
    RegisterSpellScript(classic_spell_sapphiron_life_drain);
    RegisterGameObjectAI(classic_go_sapphiron_birth);
}
