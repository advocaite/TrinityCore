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

// Classic 1.60 (Forever): Excavation Site: Wetlands (map 2998, zone 16732). Spawns from the other Forever server world DB, encounters from the
// 70532 DungeonEncounter.db2: 3480 Saltspine, 3481 Shadetooth, 3644 Highland Horror, 3482 Relic Guardian (last, OrderIndex 2000).
// The bosses have no scripts of ours except the Relic Guardian, so the boss states come from ClassicDungeonEncounterTracker.
//
// Relic Guardian (260326): spawns uninteractible (template unit_flags) next to the Archival Panel (640041, a QUICK_OPEN button that
// anyone can use). Its two official yells are "Archival process suspended. Initializing stabilization protocol." and "Stabilization
// resolved. Defensive measures reactivated." (BroadcastText 310733 / 310735). INFERRED, no sniff of the fight: using the panel
// suspends the archiving, the first yell plays, and after the stabilization the second yell plays and the Guardian fights the
// player who used the panel. Its abilities are unknown, so it fights in melee only. After a wipe it goes back to sleep and the
// panel wakes it again. Its death drops the Titan Relic that starts Elder Knowledge (Horde) or Lost Relic Carry (Alliance).

#include "ScriptMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "classic_dungeon_encounter_tracker.h"

namespace
{
constexpr uint32 CLASSIC_ESW_MAP_ID = 2998;

enum ClassicExcavationSiteBosses : uint32
{
    CESW_ENC_SALTSPINE = 0,
    CESW_ENC_SHADETOOTH = 1,
    CESW_ENC_HIGHLAND_HORROR = 2,
    CESW_ENC_RELIC_GUARDIAN = 3,
    CESW_ENC_MAX
};

enum ClassicExcavationSiteCreatures : uint32
{
    NPC_ESW_SALTSPINE = 260322,
    NPC_ESW_SHADETOOTH = 260325,
    NPC_ESW_RELIC_GUARDIAN = 260326,
    NPC_ESW_HIGHLAND_HORROR = 260808
};

constexpr ClassicDungeonBossEntry ClassicExcavationSiteBossEntries[] =
{
    { CESW_ENC_SALTSPINE, NPC_ESW_SALTSPINE },
    { CESW_ENC_SHADETOOTH, NPC_ESW_SHADETOOTH },
    { CESW_ENC_HIGHLAND_HORROR, NPC_ESW_HIGHLAND_HORROR },
    { CESW_ENC_RELIC_GUARDIAN, NPC_ESW_RELIC_GUARDIAN }
};

enum ClassicRelicGuardianTexts : uint8
{
    SAY_RELIC_GUARDIAN_STABILIZATION_START = 0,
    SAY_RELIC_GUARDIAN_STABILIZATION_END = 1
};

enum ClassicRelicGuardianMisc : int32
{
    DATA_RELIC_GUARDIAN_AWAKEN = 1,
    EVENT_RELIC_GUARDIAN_DEFEND = 1
};

constexpr Milliseconds RelicGuardianStabilizationTime = 5s;     // INFERRED: the time between the two yells is not known
constexpr float RelicGuardianPanelRange = 40.0f;                // the panel stands 19 yd from the Guardian
}

class classic_instance_excavation_site_wetlands : public InstanceMapScript
{
public:
    classic_instance_excavation_site_wetlands() : InstanceMapScript("classic_instance_excavation_site_wetlands", CLASSIC_ESW_MAP_ID) { }

    struct classic_instance_excavation_site_wetlands_InstanceScript : public InstanceScript
    {
        classic_instance_excavation_site_wetlands_InstanceScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders("CESW");
            SetBossNumber(CESW_ENC_MAX);
            static constexpr DungeonEncounterData encounters[] =
            {
                { CESW_ENC_SALTSPINE, {{ 3480 }} },
                { CESW_ENC_SHADETOOTH, {{ 3481 }} },
                { CESW_ENC_HIGHLAND_HORROR, {{ 3644 }} },
                { CESW_ENC_RELIC_GUARDIAN, {{ 3482 }} }
            };
            LoadDungeonEncounterData(encounters);
        }

        ClassicDungeonEncounterTracker _encounterTracker{ *this, ClassicExcavationSiteBossEntries };

        void OnCreatureCreate(Creature* creature) override
        {
            InstanceScript::OnCreatureCreate(creature);
            _encounterTracker.OnCreatureCreate(creature);
        }

        void OnCreatureRemove(Creature* creature) override
        {
            InstanceScript::OnCreatureRemove(creature);
            _encounterTracker.OnCreatureRemove(creature);
        }

        void OnUnitDeath(Unit* unit) override
        {
            _encounterTracker.OnUnitDeath(unit);
        }

        void Update(uint32 diff) override
        {
            _encounterTracker.Update(diff);
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new classic_instance_excavation_site_wetlands_InstanceScript(map);
    }
};

struct classic_boss_relic_guardian : public ScriptedAI
{
    classic_boss_relic_guardian(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        Sleep();
    }

    void JustReachedHome() override
    {
        Sleep();
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id != DATA_RELIC_GUARDIAN_AWAKEN || _waking || !me->IsAlive() || me->IsEngaged() || me->IsInEvadeMode())
            return;

        _waking = true;
        _waker = guid;
        Talk(SAY_RELIC_GUARDIAN_STABILIZATION_START);
        _events.ScheduleEvent(EVENT_RELIC_GUARDIAN_DEFEND, RelicGuardianStabilizationTime);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_RELIC_GUARDIAN_DEFEND)
        {
            Talk(SAY_RELIC_GUARDIAN_STABILIZATION_END);
            me->SetUninteractible(false);
            me->SetReactState(REACT_AGGRESSIVE);

            Player* target = ObjectAccessor::GetPlayer(*me, _waker);
            if (!target || !target->IsAlive() || !me->IsValidAttackTarget(target))
                target = me->SelectNearestPlayer(RelicGuardianPanelRange);

            if (target)
                AttackStart(target);
            else
                Sleep();                // nobody left at the panel: back to archiving
        }

        UpdateVictim();
    }

private:
    void Sleep()
    {
        _events.Reset();
        _waking = false;
        _waker.Clear();
        me->SetReactState(REACT_PASSIVE);
        me->SetUninteractible(true);
    }

    EventMap _events;
    ObjectGuid _waker;
    bool _waking = false;
};

// Archival Panel (640041): wakes the Relic Guardian; the button itself works as usual
struct classic_go_excavation_archival_panel : public GameObjectAI
{
    classic_go_excavation_archival_panel(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        if (Creature* guardian = me->FindNearestCreature(NPC_ESW_RELIC_GUARDIAN, RelicGuardianPanelRange))
            guardian->AI()->SetGUID(player->GetGUID(), DATA_RELIC_GUARDIAN_AWAKEN);
        return false;
    }
};

void AddSC_classic_instance_excavation_site_wetlands()
{
    new classic_instance_excavation_site_wetlands();
    RegisterCreatureAI(classic_boss_relic_guardian);
    RegisterGameObjectAI(classic_go_excavation_archival_panel);
}
