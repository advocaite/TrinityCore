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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/tirisfal_glades/scarlet_monastery/instance_scarlet_monastery.cpp
// (ScriptDev2 lineage, GPL-2)
// Ported: instance_scarlet_monastery (map 189), at_cathedral_entrance (4089)

#include "ScriptMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "DB2Structure.h"
#include "EventMap.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "TemporarySummon.h"
#include "classic_dungeon_encounter_tracker.h"
#include "classic_scarlet_monastery.h"
#include "classic_script_text.h"
#include <set>

namespace
{
enum ClassicSMAshbringerMisc
{
    NPC_SM_SCARLET_SORCERER         = 4294,
    NPC_SM_SCARLET_MYRIDON          = 4295,
    NPC_SM_SCARLET_DEFENDER         = 4298,
    NPC_SM_SCARLET_CHAPLAIN         = 4299,
    NPC_SM_SCARLET_WIZARD           = 4300,
    NPC_SM_SCARLET_CENTURION        = 4301,
    NPC_SM_SCARLET_CHAMPION         = 4302,
    NPC_SM_SCARLET_ABBOT            = 4303,
    NPC_SM_SCARLET_MONK             = 4540,

    NPC_SM_FAIRBANKS                = 4542,
    NPC_SM_COMMANDER_MOGRAINE       = 3976,
    NPC_SM_INQUISITOR_WHITEMANE     = 3977,
    NPC_SM_HIGHLORD_MOGRAINE        = 16062,
    NPC_SM_VORREL_SENGUTZ           = 3981,

    GO_SM_CHAPEL_DOOR               = 104591,
    GO_SM_HIGH_INQUISITOR_DOOR      = 104600,

    SAY_SM_COMMANDER1               = 12390,
    SAY_SM_COMMANDER2               = 12470,
    SAY_SM_COMMANDER3               = 12472,
    SAY_SM_ASHBRINGER1              = 12469,
    SAY_SM_ASHBRINGER2              = 12471,
    SAY_SM_ASHBRINGER3              = 12473,
    YELL_SM_COMMANDER               = 12389,
    YELL_SM_WHITEMANE               = 2973,

    SPELL_SM_AB_EFFECT_000          = 28441,
    SPELL_SM_FORGIVENESS            = 28697,
    SPELL_SM_MOGRAINE_COMETH_DND    = 28688,
    SPELL_SM_ASHBRINGER             = 28282,

    MODEL_SM_HIGHLORD_MOGRAINE      = 16180,

    FACTION_SM_FRIENDLY             = 35,

    AREATRIGGER_SM_CATHEDRAL        = 4089
};

enum ClassicSMMograineStage : uint32
{
    STAGE_MOGRAINE_NOT_STARTED      = 0,
    STAGE_MOGRAINE_IN_PROGRESS      = 1,
    STAGE_MOGRAINE_DIED_ONCE        = 2,
    STAGE_MOGRAINE_REVIVED          = 3,
    STAGE_MOGRAINE_DONE             = 4
};

enum ClassicSMAshbringerEvents
{
    EVENT_SM_KNEEL = 1,
    EVENT_SM_TALK1,
    EVENT_SM_SUMMON,
    EVENT_SM_TALK2,
    EVENT_SM_STAND,
    EVENT_SM_TALK3,
    EVENT_SM_TALK4,
    EVENT_SM_SECOND_DOUBT,
    EVENT_SM_POINT,
    EVENT_SM_ROAR,
    EVENT_SM_TALK5,
    EVENT_SM_SPELL,
    EVENT_SM_FORGIVEN,
    EVENT_SM_DESPAWN
};
}

namespace
{
// Classic 1.60: TC boss states for the DungeonEncounter.db2 encounters of Scarlet Monastery (map 189), driven by
// ClassicDungeonEncounterTracker (boss kill credit, encounter start / end, instance lock)
enum ClassicScarletMonasteryEncounterBosses : uint32
{
    CSM_ENC_VISHAS = 0,
    CSM_ENC_THALNOS = 1,
    CSM_ENC_LOKSEY = 2,
    CSM_ENC_DOAN = 3,
    CSM_ENC_HEROD = 4,
    CSM_ENC_FAIRBANKS = 5,
    CSM_ENC_WHITEMANE = 6,
    CSM_ENC_MAX
};

constexpr ClassicDungeonBossEntry ClassicScarletMonasteryEncounterBossEntries[] =
{
    { CSM_ENC_VISHAS, 3983 }, // Interrogator Vishas
    { CSM_ENC_THALNOS, 4543 }, // Bloodmage Thalnos
    { CSM_ENC_LOKSEY, 3974 }, // Houndmaster Loksey
    { CSM_ENC_DOAN, 6487 }, // Arcanist Doan
    { CSM_ENC_HEROD, 3975 }, // Herod
    { CSM_ENC_FAIRBANKS, 4542 }, // High Inquisitor Fairbanks
    { CSM_ENC_WHITEMANE, 3976 }, // Scarlet Commander Mograine
    { CSM_ENC_WHITEMANE, 3977 } // High Inquisitor Whitemane
};
}

class classic_instance_scarlet_monastery : public InstanceMapScript
{
public:
    classic_instance_scarlet_monastery() : InstanceMapScript(ClassicSMScriptName, 189) { }

    struct classic_instance_scarlet_monastery_InstanceScript : public InstanceScript
    {
        classic_instance_scarlet_monastery_InstanceScript(InstanceMap* map) : InstanceScript(map),
            _savedMograine(*this, "MograineAndWhitemane", 0u), _savedAshbringer(*this, "Ashbringer", 0u),
            _ashbringerActive(false)
        {
            SetHeaders("SM");
            SetBossNumber(CSM_ENC_MAX);
            static constexpr DungeonEncounterData encounters[] =
            {
                { CSM_ENC_VISHAS, {{ 444 }} },
                { CSM_ENC_THALNOS, {{ 2779 }} },
                { CSM_ENC_LOKSEY, {{ 446 }} },
                { CSM_ENC_DOAN, {{ 447 }} },
                { CSM_ENC_HEROD, {{ 448 }} },
                { CSM_ENC_FAIRBANKS, {{ 449 }} },
                { CSM_ENC_WHITEMANE, {{ 450 }} }
            };
            LoadDungeonEncounterData(encounters);
            _encounter[0] = 0;
            _encounter[1] = 0;
        }

        ClassicDungeonEncounterTracker _encounterTracker{ *this, ClassicScarletMonasteryEncounterBossEntries };

        void OnCreatureRemove(Creature* creature) override
        {
            InstanceScript::OnCreatureRemove(creature);
            _encounterTracker.OnCreatureRemove(creature);
        }

        // VMaNGOS Load(): "m_auiEncounter[0] m_auiEncounter[1]", IN_PROGRESS -> NOT_STARTED
        void AfterDataLoad() override
        {
            _encounter[0] = _savedMograine;
            _encounter[1] = _savedAshbringer;
            for (uint32& i : _encounter)
                if (i == IN_PROGRESS)
                    i = NOT_STARTED;
        }

        // VMaNGOS SaveToDB()
        void SaveClassicData()
        {
            _savedMograine = _encounter[0];
            _savedAshbringer = _encounter[1];
        }

        void OnCreatureCreate(Creature* creature) override
        {
            InstanceScript::OnCreatureCreate(creature);
            _encounterTracker.OnCreatureCreate(creature);

            switch (creature->GetEntry())
            {
                case NPC_SM_SCARLET_SORCERER:
                case NPC_SM_SCARLET_MYRIDON:
                case NPC_SM_SCARLET_DEFENDER:
                case NPC_SM_SCARLET_CHAPLAIN:
                case NPC_SM_SCARLET_WIZARD:
                case NPC_SM_SCARLET_CENTURION:
                case NPC_SM_SCARLET_CHAMPION:
                case NPC_SM_SCARLET_ABBOT:
                case NPC_SM_SCARLET_MONK:
                case NPC_SM_FAIRBANKS:
                    _ashbringerReactedNpcs.insert(creature->GetGUID());
                    break;
                case NPC_SM_COMMANDER_MOGRAINE:
                    _mograineGUID = creature->GetGUID();
                    _ashbringerReactedNpcs.insert(creature->GetGUID());
                    break;
                case NPC_SM_INQUISITOR_WHITEMANE:
                    _whitemaneGUID = creature->GetGUID();
                    break;
                case NPC_SM_VORREL_SENGUTZ:
                    _vorrelGUID = creature->GetGUID();
                    break;
                default:
                    break;
            }
        }

        void OnGameObjectCreate(GameObject* go) override
        {
            InstanceScript::OnGameObjectCreate(go);

            switch (go->GetEntry())
            {
                case GO_SM_HIGH_INQUISITOR_DOOR:
                    _doorHighInquisitorGUID = go->GetGUID();
                    break;
                case GO_SM_CHAPEL_DOOR:
                    _chapelDoorGUID = go->GetGUID();
                    break;
                default:
                    break;
            }
        }

        // VMaNGOS OnCreatureDeath
        void OnUnitDeath(Unit* unit) override
        {
            _encounterTracker.OnUnitDeath(unit);

            Creature* creature = unit->ToCreature();
            if (!creature)
                return;

            switch (creature->GetEntry())
            {
                case NPC_SM_COMMANDER_MOGRAINE:
                    if (Creature* whitemane = instance->GetCreature(_whitemaneGUID))
                        if (whitemane->isDead())
                            SetData(CLASSIC_SM_TYPE_MOGRAINE_AND_WHITE, STAGE_MOGRAINE_DONE);
                    break;
                case NPC_SM_INQUISITOR_WHITEMANE:
                    if (Creature* mograine = instance->GetCreature(_mograineGUID))
                        if (mograine->isDead())
                            SetData(CLASSIC_SM_TYPE_MOGRAINE_AND_WHITE, STAGE_MOGRAINE_DONE);
                    break;
                default:
                    break;
            }
        }

        ObjectGuid GetGuidData(uint32 type) const override
        {
            switch (type)
            {
                case CLASSIC_SM_DATA_MOGRAINE:
                    return _mograineGUID;
                case CLASSIC_SM_DATA_WHITEMANE:
                    return _whitemaneGUID;
                case CLASSIC_SM_DATA_VORREL:
                    return _vorrelGUID;
                case CLASSIC_SM_DATA_DOOR_WHITEMANE:
                    return _doorHighInquisitorGUID;
                case CLASSIC_SM_DATA_DOOR_CHAPEL:
                    return _chapelDoorGUID;
                default:
                    break;
            }

            return ObjectGuid::Empty;
        }

        // Replaces the VMaNGOS OnCreatureSpellHit instance hook (see header)
        // VMaNGOS reacted to spell 28441 (AB Effect 000, cast by an Ashbringer wielder) hitting Commander Mograine (3976). His SmartAI
        // row 27 (2026_10_24_62_world_classic_sm_ashbringer_mograine.sql) sets this on that spell hit with the caster's guid.
        void SetGuidData(uint32 type, ObjectGuid data) override
        {
            if (type != CLASSIC_SM_DATA_ASHBRINGER_MOGRAINE_HIT)
                return;

            if (_ashbringerActive || !data.IsPlayer())
                return;

            Creature* mograine = instance->GetCreature(_mograineGUID);
            if (!mograine || mograine->isDead())
                return;

            _ashbringerWielderGUID = data;
            if (Player* player = ObjectAccessor::GetPlayer(instance, data))
                mograine->SetFacingToObject(player);
            _events.ScheduleEvent(EVENT_SM_KNEEL, 1s, 2s);
            _events.ScheduleEvent(EVENT_SM_SUMMON, 20ms);
            _ashbringerActive = true;
        }

        bool IsMograineOrWhitemaneDead()
        {
            Creature* mograine = instance->GetCreature(_mograineGUID);
            Creature* whitemane = instance->GetCreature(_whitemaneGUID);

            // If they are despawned, consider them dead.
            if (!mograine || !whitemane)
                return true;

            return mograine->isDead() || whitemane->isDead();
        }

        void SetData(uint32 type, uint32 data) override
        {
            if (type == CLASSIC_SM_TYPE_MOGRAINE_AND_WHITE)
            {
                uint32& currentState = _encounter[0];

                // If boss was already killed, it's not allowed to do encounter again.
                if (data == STAGE_MOGRAINE_NOT_STARTED && IsMograineOrWhitemaneDead())
                {
                    if (Creature* whitemane = instance->GetCreature(_whitemaneGUID))
                        whitemane->DespawnOrUnsummon();

                    if (Creature* mograine = instance->GetCreature(_mograineGUID))
                        mograine->DespawnOrUnsummon();

                    data = STAGE_MOGRAINE_DONE;
                }
                else if (data == STAGE_MOGRAINE_NOT_STARTED || data == STAGE_MOGRAINE_IN_PROGRESS)
                {
                    if (GameObject* door = instance->GetGameObject(_doorHighInquisitorGUID))
                        door->SetGoState(GO_STATE_READY);

                    // If Whitemane was currently on the way to revive Mograine she needs to be reset.
                    if (data == STAGE_MOGRAINE_NOT_STARTED && currentState == STAGE_MOGRAINE_DIED_ONCE)
                    {
                        if (Creature* whitemane = instance->GetCreature(_whitemaneGUID))
                            whitemane->Respawn(true); // VMaNGOS SetDeathState(JUST_DIED) + Respawn()
                    }

                    if (Creature* mograine = instance->GetCreature(_mograineGUID))
                    {
                        if (data == STAGE_MOGRAINE_IN_PROGRESS && mograine->GetVictim())
                        {
                            std::list<Creature*> mograinesAssist;
                            GetCreatureListWithEntryInGrid(mograinesAssist, mograine, NPC_SM_SCARLET_CHAPLAIN, 82.0f);
                            GetCreatureListWithEntryInGrid(mograinesAssist, mograine, NPC_SM_SCARLET_WIZARD, 82.0f);
                            GetCreatureListWithEntryInGrid(mograinesAssist, mograine, NPC_SM_SCARLET_CENTURION, 82.0f);
                            GetCreatureListWithEntryInGrid(mograinesAssist, mograine, NPC_SM_SCARLET_CHAMPION, 82.0f);
                            GetCreatureListWithEntryInGrid(mograinesAssist, mograine, NPC_SM_SCARLET_ABBOT, 82.0f);
                            GetCreatureListWithEntryInGrid(mograinesAssist, mograine, NPC_SM_SCARLET_MONK, 82.0f);

                            for (Creature* assist : mograinesAssist)
                                if (assist->IsAlive() && assist->AI())
                                    assist->AI()->AttackStart(mograine->GetVictim());
                        }
                    }
                }
                else if (data == STAGE_MOGRAINE_DIED_ONCE)
                {
                    if (GameObject* door = instance->GetGameObject(_doorHighInquisitorGUID))
                        door->SetGoState(GO_STATE_ACTIVE);

                    if (Creature* whitemane = instance->GetCreature(_whitemaneGUID))
                    {
                        ClassicScriptText(YELL_SM_WHITEMANE, whitemane);

                        if (Creature* mograine = instance->GetCreature(_mograineGUID))
                            whitemane->GetMotionMaster()->MovePoint(100, mograine->GetPositionX(), mograine->GetPositionY(), mograine->GetPositionZ(),
                                true, {}, {}, MovementWalkRunSpeedSelectionMode::ForceRun);
                    }
                }
                else if (data == STAGE_MOGRAINE_REVIVED)
                {
                    if (Creature* mograine = instance->GetCreature(_mograineGUID))
                        if (mograine->IsAlive() && !mograine->IsInCombat())
                            CreatureAI::DoZoneInCombat(mograine);

                    if (Creature* whitemane = instance->GetCreature(_whitemaneGUID))
                        if (whitemane->IsAlive() && !whitemane->IsInCombat())
                            CreatureAI::DoZoneInCombat(whitemane);
                }

                currentState = data;

                if (data == STAGE_MOGRAINE_DONE)
                    SaveClassicData();
            }
            else if (type == CLASSIC_SM_TYPE_ASHBRINGER)
            {
                if (data == IN_PROGRESS)
                {
                    instance->LoadGrid(1069.949951f, 1399.140015f);
                    if (GameObject* go = instance->GetGameObject(_chapelDoorGUID))
                    {
                        go->SetGoState(GO_STATE_ACTIVE);
                        go->SetLootState(GO_ACTIVATED);
                        go->SetFlag(GO_FLAG_IN_USE);
                    }

                    if (Creature* whitemane = instance->GetCreature(_whitemaneGUID))
                        if (whitemane->IsAlive() && !whitemane->IsInCombat())
                            whitemane->DespawnOrUnsummon();

                    for (ObjectGuid const& guid : _ashbringerReactedNpcs)
                        if (Creature* scarletNpc = instance->GetCreature(guid))
                            if (scarletNpc->IsAlive() && !scarletNpc->IsInCombat())
                                scarletNpc->SetFaction(FACTION_SM_FRIENDLY);
                }
                _encounter[1] = data;

                if (data == DONE)
                    SaveClassicData();
            }
        }

        uint32 GetData(uint32 type) const override
        {
            if (type == CLASSIC_SM_TYPE_MOGRAINE_AND_WHITE)
                return _encounter[0];
            if (type == CLASSIC_SM_TYPE_ASHBRINGER)
                return _encounter[1];
            return 0;
        }

        void Update(uint32 diff) override
        {
            _encounterTracker.Update(diff);

            if (!_ashbringerActive)
                return;

            _events.Update(diff);
            while (uint32 eventId = _events.ExecuteEvent())
            {
                Creature* mograine = instance->GetCreature(_mograineGUID);
                if (!mograine)
                    continue;

                Creature* highlord = GetClosestCreatureWithEntry(mograine, NPC_SM_HIGHLORD_MOGRAINE, 100.0f);
                switch (eventId)
                {
                    case EVENT_SM_KNEEL:
                        mograine->SetSheath(SHEATH_STATE_UNARMED);
                        mograine->SetStandState(UNIT_STAND_STATE_KNEEL);
                        _events.ScheduleEvent(EVENT_SM_TALK1, 2s);
                        break;
                    case EVENT_SM_TALK1:
                        if (!_ashbringerWielderGUID.IsEmpty())
                            if (Player* player = ObjectAccessor::GetPlayer(instance, _ashbringerWielderGUID))
                                ClassicScriptText(SAY_SM_COMMANDER1, mograine, player);
                        break;
                    case EVENT_SM_SUMMON:
                        if (TempSummon* summon = mograine->SummonCreature(NPC_SM_HIGHLORD_MOGRAINE, 1034.9252f, 1399.0653f, 27.393204f, 6.257956981658935546f, TEMPSUMMON_TIMED_DESPAWN, 400s))
                        {
                            summon->SetFaction(FACTION_SM_FRIENDLY); // VMaNGOS SetFactionTemporary(35, TEMPFACTION_RESTORE_RESPAWN)
                            summon->SetDisplayId(MODEL_SM_HIGHLORD_MOGRAINE);
                            summon->SetVirtualItem(0, 0);            // SetVirtualItem(BASE_ATTACK, EQUIP_UNEQUIP)
                            summon->SetObjectScale(2.0f);
                            summon->CastSpell(summon, SPELL_SM_MOGRAINE_COMETH_DND, false);
                            summon->GetMotionMaster()->MovePoint(0, 1150.3911f, 1398.723f, 32.54613f);
                        }
                        _events.ScheduleEvent(EVENT_SM_TALK2, 48500ms);
                        break;
                    case EVENT_SM_TALK2:
                        if (highlord)
                        {
                            highlord->StopMoving();
                            highlord->SetFacingToObject(mograine);
                            ClassicScriptText(SAY_SM_ASHBRINGER1, highlord);
                        }
                        _events.ScheduleEvent(EVENT_SM_STAND, 4s);
                        break;
                    case EVENT_SM_STAND:
                        mograine->SetSheath(SHEATH_STATE_MELEE);
                        mograine->HandleEmoteCommand(EMOTE_STATE_STAND);
                        mograine->SetStandState(UNIT_STAND_STATE_STAND);
                        if (highlord)
                            mograine->SetFacingToObject(highlord);
                        _events.ScheduleEvent(EVENT_SM_TALK3, 2s);
                        break;
                    case EVENT_SM_TALK3:
                        ClassicScriptText(SAY_SM_COMMANDER2, mograine);
                        _events.ScheduleEvent(EVENT_SM_TALK4, 4s);
                        break;
                    case EVENT_SM_TALK4:
                        if (highlord)
                            ClassicScriptText(SAY_SM_ASHBRINGER2, highlord);
                        _events.ScheduleEvent(EVENT_SM_SECOND_DOUBT, 4s);
                        break;
                    case EVENT_SM_SECOND_DOUBT:
                        if (highlord)
                            highlord->HandleEmoteCommand(EMOTE_ONESHOT_QUESTION);
                        _events.ScheduleEvent(EVENT_SM_POINT, 4s);
                        break;
                    case EVENT_SM_POINT:
                        if (highlord)
                            highlord->HandleEmoteCommand(EMOTE_ONESHOT_POINT);
                        _events.ScheduleEvent(EVENT_SM_ROAR, 3s);
                        break;
                    case EVENT_SM_ROAR:
                        if (highlord)
                            highlord->HandleEmoteCommand(EMOTE_ONESHOT_BATTLE_ROAR);
                        _events.ScheduleEvent(EVENT_SM_TALK5, 4s);
                        break;
                    case EVENT_SM_TALK5:
                        ClassicScriptText(SAY_SM_COMMANDER3, mograine);
                        _events.ScheduleEvent(EVENT_SM_SPELL, 4s);
                        break;
                    case EVENT_SM_SPELL:
                        if (highlord)
                            highlord->CastSpell(mograine, SPELL_SM_FORGIVENESS, false);
                        _events.ScheduleEvent(EVENT_SM_FORGIVEN, 4s);
                        break;
                    case EVENT_SM_FORGIVEN:
                        if (highlord)
                            ClassicScriptText(SAY_SM_ASHBRINGER3, highlord);
                        _events.ScheduleEvent(EVENT_SM_DESPAWN, 4s);
                        break;
                    case EVENT_SM_DESPAWN:
                        if (highlord)
                            highlord->DespawnOrUnsummon();
                        _ashbringerWielderGUID.Clear();
                        break;
                    default:
                        break;
                }
            }
        }

    private:
        uint32 _encounter[CLASSIC_SM_MAX_ENCOUNTER];
        PersistentInstanceScriptValue<uint32> _savedMograine;
        PersistentInstanceScriptValue<uint32> _savedAshbringer;

        ObjectGuid _mograineGUID;
        ObjectGuid _whitemaneGUID;
        ObjectGuid _vorrelGUID;
        ObjectGuid _doorHighInquisitorGUID;
        ObjectGuid _chapelDoorGUID;
        ObjectGuid _ashbringerWielderGUID;
        bool _ashbringerActive;
        std::set<ObjectGuid> _ashbringerReactedNpcs;
        EventMap _events;
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new classic_instance_scarlet_monastery_InstanceScript(map);
    }
};

/*######
## at_cathedral_entrance
######*/

class classic_at_cathedral_entrance : public AreaTriggerScript
{
public:
    classic_at_cathedral_entrance() : AreaTriggerScript("classic_at_cathedral_entrance") { }

    bool OnTrigger(Player* player, AreaTriggerEntry const* areaTrigger) override
    {
        if (areaTrigger->ID != AREATRIGGER_SM_CATHEDRAL)
            return false;

        if (!player->HasAura(SPELL_SM_ASHBRINGER))
            return false;

        InstanceScript* instance = player->GetInstanceScript();
        if (!instance || instance->GetData(CLASSIC_SM_TYPE_ASHBRINGER) != NOT_STARTED)
            return false;

        instance->SetData(CLASSIC_SM_TYPE_ASHBRINGER, IN_PROGRESS);
        if (Creature* mograine = ObjectAccessor::GetCreature(*player, instance->GetGuidData(CLASSIC_SM_DATA_MOGRAINE)))
            if (mograine->IsAlive())
                ClassicScriptText(YELL_SM_COMMANDER, mograine, player);

        return true;
    }
};

void AddSC_classic_instance_scarlet_monastery()
{
    new classic_instance_scarlet_monastery();
    new classic_at_cathedral_entrance();
}
