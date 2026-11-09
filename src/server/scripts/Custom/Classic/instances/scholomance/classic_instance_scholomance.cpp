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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/western_plaguelands/scholomance/instance_scholomance.cpp
// (ScriptDev2 lineage, GPL-2)
// Ported: instance_scholomance, go_brazier_herald, go_viewing_room_door, the Divination Scryer event (VMaNGOS event_scripts 8436)
// Added: Darkmaster Gandling's Shadow Portal spell scripts (VMaNGOS Spell::EffectDummy 17950 + the room portal events)

#include "ScriptMgr.h"
#include "Containers.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureAIImpl.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "classic_dungeon_encounter_tracker.h"
#include "classic_scholomance.h"
#include <array>
#include <bitset>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <vector>

using namespace ClassicScholomance;

namespace
{
    // Save keys for the VMaNGOS m_auiEncounter[] array (VMaNGOS saves the whole array as "a b c ...").
    char const* const ClassicSchEncounterSaveKeys[INSTANCE_SCHOLOMANCE_MAX_ENCOUNTER] =
    {
        "enc0", "enc1", "enc2", "enc3", "enc4", "enc5", "enc6", "enc7",
        "enc8", "enc9", "enc10", "enc11", "enc12", "enc13", "enc14", "enc15"
    };
}

namespace
{
// Classic 1.60: TC boss states for the DungeonEncounter.db2 encounters of Scholomance (map 289), driven by
// ClassicDungeonEncounterTracker (boss kill credit, encounter start / end, instance lock)
enum ClassicScholomanceEncounterBosses : uint32
{
    CSCHOLO_ENC_KIRTONOS = 0,
    CSCHOLO_ENC_JANDICE = 1,
    CSCHOLO_ENC_RATTLEGORE = 2,
    CSCHOLO_ENC_MARDUK = 3,
    CSCHOLO_ENC_VECTUS = 4,
    CSCHOLO_ENC_KORMOK = 5,
    CSCHOLO_ENC_RAS = 6,
    CSCHOLO_ENC_MALICIA = 7,
    CSCHOLO_ENC_KRASTINOV = 8,
    CSCHOLO_ENC_POLKELT = 9,
    CSCHOLO_ENC_RAVENIAN = 10,
    CSCHOLO_ENC_ALEXEI = 11,
    CSCHOLO_ENC_ILLUCIA = 12,
    CSCHOLO_ENC_GANDLING = 13,
    CSCHOLO_ENC_MAX
};

constexpr ClassicDungeonBossEntry ClassicScholomanceEncounterBossEntries[] =
{
    { CSCHOLO_ENC_KIRTONOS, 10506 }, // Kirtonos the Herald
    { CSCHOLO_ENC_JANDICE, 10503 }, // Jandice Barov
    { CSCHOLO_ENC_RATTLEGORE, 11622 }, // Rattlegore
    { CSCHOLO_ENC_MARDUK, 10433 }, // Marduk Blackpool
    { CSCHOLO_ENC_VECTUS, 10432 }, // Vectus
    { CSCHOLO_ENC_KORMOK, 16118 }, // Kormok
    { CSCHOLO_ENC_RAS, 10508 }, // Ras Frostwhisper
    { CSCHOLO_ENC_MALICIA, 10505 }, // Instructor Malicia
    { CSCHOLO_ENC_KRASTINOV, 11261 }, // Doctor Theolen Krastinov
    { CSCHOLO_ENC_POLKELT, 10901 }, // Lorekeeper Polkelt
    { CSCHOLO_ENC_RAVENIAN, 10507 }, // The Ravenian
    { CSCHOLO_ENC_ALEXEI, 10504 }, // Lord Alexei Barov
    { CSCHOLO_ENC_ILLUCIA, 10502 }, // Lady Illucia Barov
    { CSCHOLO_ENC_GANDLING, 1853 } // Darkmaster Gandling
};
}

class classic_instance_scholomance : public InstanceMapScript
{
public:
    classic_instance_scholomance() : InstanceMapScript(InstanceScriptName, MapId) { }

    struct classic_instance_scholomance_InstanceScript : public InstanceScript
    {
        classic_instance_scholomance_InstanceScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(CSCHOLO_ENC_MAX);
            static constexpr DungeonEncounterData encounters[] =
            {
                { CSCHOLO_ENC_KIRTONOS, {{ 2805 }} },
                { CSCHOLO_ENC_JANDICE, {{ 2804 }} },
                { CSCHOLO_ENC_RATTLEGORE, {{ 2811 }} },
                { CSCHOLO_ENC_MARDUK, {{ 2809 }} },
                { CSCHOLO_ENC_VECTUS, {{ 2813 }} },
                { CSCHOLO_ENC_KORMOK, {{ 3055 }} },
                { CSCHOLO_ENC_RAS, {{ 2810 }} },
                { CSCHOLO_ENC_MALICIA, {{ 2803 }} },
                { CSCHOLO_ENC_KRASTINOV, {{ 2802 }} },
                { CSCHOLO_ENC_POLKELT, {{ 2808 }} },
                { CSCHOLO_ENC_RAVENIAN, {{ 2812 }} },
                { CSCHOLO_ENC_ALEXEI, {{ 2807 }} },
                { CSCHOLO_ENC_ILLUCIA, {{ 2806 }} },
                { CSCHOLO_ENC_GANDLING, {{ 2801 }} }
            };
            LoadDungeonEncounterData(encounters);

            _encounter.fill(NOT_STARTED);
            for (uint32 i = 0; i < INSTANCE_SCHOLOMANCE_MAX_ENCOUNTER; ++i)
                _saved[i] = std::make_unique<PersistentInstanceScriptValue<uint32>>(*this, ClassicSchEncounterSaveKeys[i], uint32(NOT_STARTED));

            _summonGandlingPending = false;
        }

        ClassicDungeonEncounterTracker _encounterTracker{ *this, ClassicScholomanceEncounterBossEntries };

        void OnCreatureRemove(Creature* creature) override
        {
            InstanceScript::OnCreatureRemove(creature);
            _encounterTracker.OnCreatureRemove(creature);

            if (creature->GetEntry() == NPC_RISEN_GUARDIAN)
                ReleaseRoomGuardian(creature->GetGUID());
        }

        void OnCreatureCreate(Creature* creature) override
        {
            InstanceScript::OnCreatureCreate(creature);
            _encounterTracker.OnCreatureCreate(creature);

            switch (creature->GetEntry())
            {
                case NPC_VECTUS:
                    _vectusGUID = creature->GetGUID();
                    break;
                case NPC_MARDUKE:
                    _mardukeGUID = creature->GetGUID();
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
                case GO_GATE_KIRTONOS:
                    _gateKirtonosGUID = go->GetGUID();
                    break;
                case GO_GATE_GANDLING:
                    _gateGandlingGUID = go->GetGUID();
                    break;
                case GO_GATE_MALICIA:
                    _gateMiliciaGUID = go->GetGUID();
                    break;
                case GO_GATE_THEOLEN:
                    _gateTheolenGUID = go->GetGUID();
                    break;
                case GO_GATE_POLKELT:
                    _gatePolkeltGUID = go->GetGUID();
                    break;
                case GO_GATE_RAVENIAN:
                    _gateRavenianGUID = go->GetGUID();
                    break;
                case GO_GATE_BAROV:
                    _gateBarovGUID = go->GetGUID();
                    break;
                case GO_GATE_ILLUCIA:
                    _gateIlluciaGUID = go->GetGUID();
                    break;
                case GO_BRAZIER_KIRTONOS:
                    _brazierKirtonosGUID = go->GetGUID();
                    break;
                case GO_VIEWING_ROOM_DOOR:
                    if (GetData(TYPE_VIEWING_ROOM_DOOR) == DONE)
                        go->UseDoorOrButton();
                    break;
                default:
                    break;
            }
        }

        uint32 GetData(uint32 type) const override
        {
            if (type == TYPE_GANDLING)
            {
                // VMaNGOS: GetData() itself switches Gandling to SPECIAL once all six bosses are dead
                if (_encounter[TYPE_GANDLING] == NOT_STARTED && _encounter[TYPE_ALEXEIBAROV] == DONE && _encounter[TYPE_THEOLEN] == DONE
                    && _encounter[TYPE_RAVENIAN] == DONE && _encounter[TYPE_POLKELT] == DONE && _encounter[TYPE_MALICIA] == DONE
                    && _encounter[TYPE_ILLUCIABAROV] == DONE)
                    _encounter[TYPE_GANDLING] = SPECIAL;
                return _encounter[TYPE_GANDLING];
            }

            if (type < INSTANCE_SCHOLOMANCE_MAX_ENCOUNTER)
                return _encounter[type];

            return 0;
        }

        ObjectGuid GetGuidData(uint32 type) const override
        {
            switch (type)
            {
                case DATA_VECTUS:
                    return _vectusGUID;
                case DATA_MARDUKE:
                    return _mardukeGUID;
                case GO_GATE_GANDLING:
                    return _gateGandlingGUID;
                case GO_GATE_MALICIA:
                    return _gateMiliciaGUID;
                case GO_GATE_THEOLEN:
                    return _gateTheolenGUID;
                case GO_GATE_POLKELT:
                    return _gatePolkeltGUID;
                case GO_GATE_RAVENIAN:
                    return _gateRavenianGUID;
                case GO_GATE_BAROV:
                    return _gateBarovGUID;
                case GO_GATE_ILLUCIA:
                    return _gateIlluciaGUID;
                default:
                    break;
            }

            return ObjectGuid::Empty;
        }

        // Shadow Portal: a Risen Guardian was raised in the room behind gate `gateEntry`. The gate stays closed
        // until every guardian registered behind it is dead or gone (ReleaseRoomGuardian).
        void SetGuidData(uint32 gateEntry, ObjectGuid guardianGuid) override
        {
            ObjectGuid const gateGuid = GetGuidData(gateEntry);
            if (gateGuid.IsEmpty())
                return;

            if (GameObject* gate = instance->GetGameObject(gateGuid))
                if (gate->GetGoState() != GO_STATE_READY)
                    gate->SetGoState(GO_STATE_READY);

            _roomGuardians[gateGuid].insert(guardianGuid);
        }

        void OnUnitDeath(Unit* unit) override
        {
            _encounterTracker.OnUnitDeath(unit);

            switch (unit->GetEntry())
            {
                case NPC_KIRTONOS:
                    OpenDoorIfClosed(_gateKirtonosGUID);
                    break;
                case NPC_RISEN_GUARDIAN:
                    ReleaseRoomGuardian(unit->GetGUID());
                    break;
                default:
                    break;
            }
        }

        void SetData(uint32 type, uint32 data) override
        {
            switch (type)
            {
                case TYPE_GANDLING:
                    _encounter[TYPE_GANDLING] = data;
                    if (data == FAIL || data == DONE)
                    {
                        OpenAllRooms();
                        OpenDoorIfClosed(_gateMiliciaGUID);
                        OpenDoorIfClosed(_gateTheolenGUID);
                        OpenDoorIfClosed(_gatePolkeltGUID);
                        OpenDoorIfClosed(_gateRavenianGUID);
                        OpenDoorIfClosed(_gateBarovGUID);
                        OpenDoorIfClosed(_gateIlluciaGUID);
                    }
                    break;
                case TYPE_KIRTONOS:
                    _encounter[TYPE_KIRTONOS] = data;
                    if (data == IN_PROGRESS)
                    {
                        if (GameObject* go = instance->GetGameObject(_gateKirtonosGUID))
                            if (go->GetGoState() == GO_STATE_ACTIVE)
                                DoUseDoorOrButton(_gateKirtonosGUID);
                    }
                    else if (data == FAIL)
                    {
                        OpenDoorIfClosed(_gateKirtonosGUID);
                        if (GameObject* go = instance->GetGameObject(_brazierKirtonosGUID))
                            go->ResetDoorOrButton();
                    }
                    break;
                case TYPE_ALEXEIBAROV:
                case TYPE_THEOLEN:
                case TYPE_RAVENIAN:
                case TYPE_POLKELT:
                case TYPE_MALICIA:
                case TYPE_ILLUCIABAROV:
                case TYPE_VIEWING_ROOM_DOOR:
                case TYPE_DARKREAVER:
                    _encounter[type] = data;
                    break;
                default:
                    break;
            }

            if (data == DONE)
                SaveEncounterData();

            SummonGandlingIfPossible();
        }

        void AfterDataLoad() override
        {
            for (uint32 i = 0; i < INSTANCE_SCHOLOMANCE_MAX_ENCOUNTER; ++i)
            {
                _encounter[i] = uint32(*_saved[i]);
                if (_encounter[i] == IN_PROGRESS)
                    _encounter[i] = NOT_STARTED;
            }

            // VMaNGOS summons Gandling directly from Load(); done on the first map update here (grids are not loaded yet)
            _summonGandlingPending = true;
        }

        void Update(uint32 diff) override
        {
            _encounterTracker.Update(diff);

            if (_summonGandlingPending)
            {
                _summonGandlingPending = false;
                SummonGandlingIfPossible();
            }
        }

    private:
        void OpenDoorIfClosed(ObjectGuid guid)
        {
            if (GameObject* go = instance->GetGameObject(guid))
                if (go->GetGoState() != GO_STATE_ACTIVE)
                    DoUseDoorOrButton(guid);
        }

        void SaveEncounterData()
        {
            for (uint32 i = 0; i < INSTANCE_SCHOLOMANCE_MAX_ENCOUNTER; ++i)
                if (uint32(*_saved[i]) != _encounter[i])
                    *_saved[i] = _encounter[i];
        }

        // A Risen Guardian died or despawned: once its room is empty the gate opens again
        void ReleaseRoomGuardian(ObjectGuid guardianGuid)
        {
            for (auto itr = _roomGuardians.begin(); itr != _roomGuardians.end(); ++itr)
            {
                if (!itr->second.erase(guardianGuid))
                    continue;

                if (itr->second.empty())
                {
                    if (GameObject* gate = instance->GetGameObject(itr->first))
                        gate->SetGoState(GO_STATE_ACTIVE);
                    _roomGuardians.erase(itr);
                }
                return;
            }
        }

        void OpenAllRooms()
        {
            for (auto const& [gateGuid, guardians] : _roomGuardians)
                if (GameObject* gate = instance->GetGameObject(gateGuid))
                    gate->SetGoState(GO_STATE_ACTIVE);
            _roomGuardians.clear();
        }

        void SummonGandlingIfPossible()
        {
            if (GetData(TYPE_GANDLING) == SPECIAL)
            {
                if (TempSummon* gandling = instance->SummonCreature(NPC_GANDLING, Position(180.771f, -5.4286f, 75.5702f, 1.29154f)))
                    gandling->SetTempSummonType(TEMPSUMMON_DEAD_DESPAWN);
                SetData(TYPE_GANDLING, IN_PROGRESS);
            }
        }

        mutable std::array<uint32, INSTANCE_SCHOLOMANCE_MAX_ENCOUNTER> _encounter;
        std::array<std::unique_ptr<PersistentInstanceScriptValue<uint32>>, INSTANCE_SCHOLOMANCE_MAX_ENCOUNTER> _saved;
        bool _summonGandlingPending;

        ObjectGuid _vectusGUID;
        ObjectGuid _mardukeGUID;

        ObjectGuid _gateKirtonosGUID;
        ObjectGuid _gateGandlingGUID;
        ObjectGuid _gateMiliciaGUID;
        ObjectGuid _gateTheolenGUID;
        ObjectGuid _gatePolkeltGUID;
        ObjectGuid _gateRavenianGUID;
        ObjectGuid _gateBarovGUID;
        ObjectGuid _gateIlluciaGUID;
        ObjectGuid _brazierKirtonosGUID;

        std::map<ObjectGuid /*gate*/, std::set<ObjectGuid /*Risen Guardian*/>> _roomGuardians;
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new classic_instance_scholomance_InstanceScript(map);
    }
};

/*######
## go_brazier_herald
######*/

// VMaNGOS pGOOpen hook -> TC GameObjectAI::OnGossipHello (called from GameObject::Use); return values kept.
struct classic_go_brazier_herald : public GameObjectAI
{
    classic_go_brazier_herald(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        if (InstanceScript* instance = me->GetInstanceScript())
        {
            switch (instance->GetData(TYPE_KIRTONOS))
            {
                case IN_PROGRESS:
                case DONE:
                    return false;
                default:
                    break;
            }

            instance->SetData(TYPE_KIRTONOS, IN_PROGRESS);
            me->PlayDirectSound(SOUND_SCREECH);

            player->SummonCreature(NPC_KIRTONOS, 315.028f, 70.53845f, 102.1496f, 0.3859715f, TEMPSUMMON_DEAD_DESPAWN, 900000ms);
        }

        return true;
    }
};

/*######
## go_viewing_room_door
######*/

struct classic_go_viewing_room_door : public GameObjectAI
{
    classic_go_viewing_room_door(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        // Save door state to database
        if (InstanceScript* instance = player->GetInstanceScript())
            instance->SetData(TYPE_VIEWING_ROOM_DOOR, DONE);
        return false;
    }
};

/*######
## Shadow Portal (Darkmaster Gandling)
######*/

enum ClassicGandlingShadowPortal : uint32
{
    SPELL_SHADOW_PORTAL_HALL_OF_SECRETS         = 17863,
    SPELL_SHADOW_PORTAL_HALL_OF_THE_DAMNED      = 17939,
    SPELL_SHADOW_PORTAL_THE_COVEN               = 17943,
    SPELL_SHADOW_PORTAL_THE_SHADOW_VAULT        = 17944,
    SPELL_SHADOW_PORTAL_BAROV_FAMILY_VAULT      = 17946,
    SPELL_SHADOW_PORTAL_VAULT_OF_THE_RAVENIAN   = 17948,

    // SPELL_EFFECT_SEND_EVENT MiscValue of the six room portals, named after the room's boss as the VMaNGOS event_scripts do
    EVENT_PORTAL_POLKELT                        = 5618,
    EVENT_PORTAL_KRASTINOV                      = 5619,
    EVENT_PORTAL_MALICIA                        = 5620,
    EVENT_PORTAL_ILLUCIA                        = 5621,
    EVENT_PORTAL_ALEXEI                         = 5622,
    EVENT_PORTAL_RAVENIAN                       = 5623
};

// VMaNGOS event_scripts 5618-5623 (world database, 2026-09-28): the gate that closes and where the Risen Guardians rise.
struct ClassicGandlingRoom
{
    uint32 Event;
    uint32 Gate;
    Position Guardians[4];      // the fourth is unused when its x is 0 (Polkelt's room raises four, the others three)
};

ClassicGandlingRoom const ClassicGandlingRooms[] =
{
    { EVENT_PORTAL_POLKELT,   GO_GATE_POLKELT,  { { 256.289f, 0.652f, 84.924f, 4.765f }, { 241.345f, 4.231f, 84.924f, 5.062f }, { 249.715f, -5.978f, 85.106f, 3.177f }, { 230.05f, -9.946f, 85.317f, 5.847f } } },
    { EVENT_PORTAL_KRASTINOV, GO_GATE_THEOLEN,  { { 180.707f, -75.818f, 84.925f, 1.396f }, { 185.689f, -64.627f, 84.925f, 5.55f }, { 175.7f, -55.238f, 85.229f, 4.772f }, { 0.0f, 0.0f, 0.0f, 0.0f } } },
    { EVENT_PORTAL_MALICIA,   GO_GATE_MALICIA,  { { 123.306f, 3.933f, 85.312f, 6.056f }, { 110.892f, -6.463f, 85.312f, 0.436f }, { 102.454f, 4.374f, 85.312f, 2.182f }, { 0.0f, 0.0f, 0.0f, 0.0f } } },
    { EVENT_PORTAL_ILLUCIA,   GO_GATE_ILLUCIA,  { { 239.556f, -4.945f, 72.674f, 1.4525f }, { 226.854f, 0.262f, 72.673f, 3.161f }, { 248.115f, 2.809f, 72.669f, 4.668f }, { 0.0f, 0.0f, 0.0f, 0.0f } } },
    { EVENT_PORTAL_ALEXEI,    GO_GATE_BAROV,    { { 185.616f, -42.912f, 75.4812f, 4.45059f }, { 177.746f, -42.7475f, 75.4812f, 4.88692f }, { 181.825f, -42.5812f, 75.4812f, 4.66003f }, { 0.0f, 0.0f, 0.0f, 0.0f } } },
    { EVENT_PORTAL_RAVENIAN,  GO_GATE_RAVENIAN, { { 128.806f, -7.874f, 75.482f, 5.62f }, { 130.415f, -1.113f, 75.482f, 2.688f }, { 124.162f, 5.816f, 75.482f, 5.061f }, { 0.0f, 0.0f, 0.0f, 0.0f } } }
};

constexpr std::array<uint32, 6> ClassicGandlingRoomPortals =
{
    SPELL_SHADOW_PORTAL_HALL_OF_SECRETS, SPELL_SHADOW_PORTAL_HALL_OF_THE_DAMNED, SPELL_SHADOW_PORTAL_THE_COVEN,
    SPELL_SHADOW_PORTAL_THE_SHADOW_VAULT, SPELL_SHADOW_PORTAL_BAROV_FAMILY_VAULT, SPELL_SHADOW_PORTAL_VAULT_OF_THE_RAVENIAN
};

// 17950 - Shadow Portal. VMaNGOS Spell::EffectDummy: the target loses all threat, then one of the six room portals is cast on it.
class classic_spell_gandling_shadow_portal : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(ClassicGandlingRoomPortals);
    }

    bool Load() override
    {
        return InstanceHasScript(GetCaster(), InstanceScriptName);
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        caster->GetThreatManager().ModifyThreatByPercent(target, -100);
        caster->CastSpell(target, Trinity::Containers::SelectRandomContainerElement(ClassicGandlingRoomPortals), true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_gandling_shadow_portal::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 17863, 17939, 17943, 17944, 17946, 17948 - Shadow Portal (one per room). EFFECT_0 ports the target to the room's
// spell_target_position; EFFECT_1 sends the room event: the room's gate closes and the Risen Guardians rise where the
// VMaNGOS event_scripts put them (five minutes or until dead). The instance opens the gate again once they are dead or gone.
class classic_spell_gandling_shadow_portal_room : public SpellScript
{
    bool Load() override
    {
        return InstanceHasScript(GetCaster(), InstanceScriptName);
    }

    void HandleSendEvent(SpellEffIndex /*effIndex*/)
    {
        ClassicGandlingRoom const* room = nullptr;
        for (ClassicGandlingRoom const& candidate : ClassicGandlingRooms)
            if (int32(candidate.Event) == GetEffectInfo().MiscValue)
                room = &candidate;
        if (!room)
            return;

        Unit* caster = GetCaster();
        InstanceScript* instance = caster->GetInstanceScript();
        if (!instance)
            return;

        uint8 raised = 0;
        for (Position const& pos : room->Guardians)
        {
            if (pos.GetPositionX() == 0.0f)
                continue;
            if (Creature* guardian = caster->SummonCreature(NPC_RISEN_GUARDIAN, pos, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 5min))
            {
                instance->SetGuidData(room->Gate, guardian->GetGUID());
                ++raised;
            }
        }

        TC_LOG_DEBUG("scripts", "classic_spell_gandling_shadow_portal_room: spell {} closed gate {} and raised {} Risen Guardians", GetSpellInfo()->Id, room->Gate, raised);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(classic_spell_gandling_shadow_portal_room::HandleSendEvent, EFFECT_1, SPELL_EFFECT_SEND_EVENT);
    }
};

/*######
## go_divination_scryer
######*/

// VMaNGOS event_scripts 8436 -> generic_scripts 843600-843611 (world database, 2026-10-26). Place Scryer (23204, the Divination
// Scryer 18746 of Judgment and Redemption and The Darkreaver Menace) calls ten waves of spirits out of the Great Ossuary; the tenth is
// Death Knight Darkreaver. A wave starts when everything the last one raised is dead. The Ossuary door closes for the event. The event
// fails when a wave runs out of time or no living player is left within 60 yards of the Ossuary, and it runs once per instance
// (VMaNGOS condition 571: TYPE_DARKREAVER still NOT_STARTED).
enum ClassicDivinationScryer : uint32
{
    GO_THE_GREAT_OSSUARY        = 179746,
    GO_OSSUARY_DOOR             = 179724,   // Doodad_PallyDoor01

    NPC_SHADOWED_SPIRIT         = 14511,
    NPC_CORRUPTED_SPIRIT        = 14512,
    NPC_MALICIOUS_SPIRIT        = 14513,
    NPC_ASPECT_OF_CORRUPTION    = 14519,

    SAY_SPIRIT_EMERGES          = 0         // broadcast 9781, "%s emerges from the darkness, drawn out by the divination scryer!"
};

struct ClassicScryerSummon
{
    uint8 Wave;
    uint32 DelaySeconds;                    // after the wave starts
    uint32 Entry;
    uint32 DespawnMs;                       // 0: gone at death (TEMPSUMMON_CORPSE_DESPAWN); else TEMPSUMMON_TIMED_OR_DEAD_DESPAWN
    bool Attacks;                           // attacks the player when it appears (Darkreaver does not)
    Position Pos;
};

ClassicScryerSummon const ClassicScryerSummons[] =
{
    { 1,   1, 14514, 0, true, { 103.035f, 141.296f, 93.9416f, 1.41372f } },
    { 1,   1, 14514, 0, true, { 121.763f, 136.966f, 93.483f, 1.01229f } },
    { 1,   1, 14514, 0, true, { 161.478f, 169.113f, 93.1686f, 2.6529f } },
    { 1,   1, 14514, 0, true, { 118.552f, 193.25f, 93.1587f, 5.06145f } },
    { 1,   1, 14514, 0, true, { 134.204f, 138.349f, 93.1686f, 1.41372f } },
    { 1,   1, 14514, 0, true, { 101.088f, 195.53f, 93.1686f, 6.10865f } },
    { 1,   1, 14514, 0, true, { 160.644f, 149.564f, 93.1686f, 3.22886f } },
    { 1,   1, 14514, 0, true, { 150.902f, 139.409f, 93.1686f, 2.25147f } },
    { 1,   1, 14514, 0, true, { 135.822f, 193.203f, 93.5791f, 1.16937f } },
    { 1,   1, 14514, 0, true, { 157.055f, 191.011f, 93.1686f, 2.02458f } },
    { 2,   1, 14518, 300000, true, { 132.562f, 174.16f, 95.2267f, 3.71755f } },
    { 2,   1, 14514, 0, true, { 130.401f, 179.008f, 94.5192f, 5.11381f } },
    { 2,   1, 14514, 0, true, { 129.104f, 171.006f, 94.203f, 1.22173f } },
    { 2,   1, 14514, 0, true, { 137.853f, 171.902f, 95.9426f, 5.51524f } },
    { 3,   1, 14513, 0, true, { 158.209f, 141.706f, 95.1845f, 2.11185f } },
    { 3,   1, 14513, 0, true, { 102.186f, 195.79f, 93.3086f, 4.46804f } },
    { 4,   1, 14513, 0, true, { 133.682f, 194.397f, 93.3896f, 2.04204f } },
    { 4,   1, 14513, 0, true, { 106.945f, 140.678f, 95.442f, 3.26377f } },
    { 4,   1, 14513, 0, true, { 160.685f, 175.273f, 93.1686f, 0.226893f } },
    { 5,   1, 14513, 0, true, { 159.269f, 186.271f, 93.1686f, 2.23402f } },
    { 5,   1, 14513, 0, true, { 114.373f, 138.182f, 93.3719f, 5.58505f } },
    { 5,   1, 14513, 0, true, { 99.6381f, 144.21f, 93.1686f, 1.23918f } },
    { 5,   1, 14513, 0, true, { 145.126f, 196.999f, 93.1686f, 2.77507f } },
    { 6,   1, 14520, 300000, true, { 134.731f, 172.909f, 95.8094f, 3.05433f } },
    { 7,   1, 14512, 0, true, { 103.429f, 187.578f, 93.1686f, 6.14356f } },
    { 7,   1, 14512, 0, true, { 107.785f, 188.747f, 93.1142f, 3.49066f } },
    { 7,   1, 14512, 0, true, { 139.5f, 172.236f, 95.9087f, 5.5676f } },
    { 7,   1, 14512, 0, true, { 136.063f, 174.658f, 95.6329f, 2.21657f } },
    { 7,   1, 14512, 0, true, { 138.514f, 168.074f, 94.9902f, 1.18682f } },
    { 7,   1, 14512, 0, true, { 161.308f, 146.645f, 93.4221f, 0.261799f } },
    { 7,   1, 14512, 0, true, { 156.757f, 142.998f, 93.6772f, 5.51524f } },
    { 7,   1, 14512, 0, true, { 156.156f, 147.682f, 93.1686f, 2.84489f } },
    { 7,   1, 14512, 0, true, { 104.292f, 191.483f, 93.1686f, 5.51524f } },
    { 8,   1, 14519, 300000, true, { 134.094f, 172.661f, 95.672f, 0.750492f } },
    { 9,   1, 14511, 0, true, { 104.977f, 170.914f, 93.1266f, 0.10472f } },
    { 9,   1, 14511, 0, true, { 157.179f, 143.024f, 93.7753f, 2.14675f } },
    { 9,   1, 14511, 0, true, { 151.155f, 193.707f, 93.5431f, 4.10152f } },
    { 9,  30, 14511, 0, true, { 150.797f, 193.709f, 93.4583f, 3.9619f } },
    { 9,  60, 14511, 0, true, { 156.827f, 142.947f, 93.7397f, 2.26893f } },
    { 9,  90, 14511, 0, true, { 150.328f, 193.797f, 93.4219f, 4.04916f } },
    { 9, 120, 14511, 0, true, { 156.587f, 142.843f, 93.7598f, 2.35619f } },
    { 9, 150, 14511, 0, true, { 149.988f, 194.053f, 93.5163f, 4.13643f } },
    { 9, 180, 14511, 0, true, { 156.118f, 142.653f, 93.786f, 2.25147f } },
    { 9, 210, 14521, 300000, true, { 136.457f, 172.565f, 95.8663f, 3.19395f } },
    { 10,  1, 14516, 600000, false, { 135.97f, 170.855f, 95.7897f, 2.98451f } }
};

struct ClassicScryerWave
{
    uint32 TimeLimitSeconds;                // the map event's time limit; running out fails the event
    uint32 CheckAfterSeconds;               // the "all dead" and "player nearby" checks start here (VMaNGOS Edit Map Event)
    uint32 Emoter;                          // the first of these to appear emotes SAY_SPIRIT_EMERGES (0: none)
};

ClassicScryerWave const ClassicScryerWaves[] =
{
    { 300,   3, 0 },
    { 300,   3, 0 },
    { 300,   3, NPC_MALICIOUS_SPIRIT },
    { 300,   3, 0 },
    { 300,   3, 0 },
    { 300,   3, 0 },
    { 300,   3, NPC_CORRUPTED_SPIRIT },
    { 300,   3, NPC_ASPECT_OF_CORRUPTION },
    { 400, 213, NPC_SHADOWED_SPIRIT },
    { 600,   3, 0 }
};

struct classic_go_divination_scryer : public GameObjectAI
{
    classic_go_divination_scryer(GameObject* go) : GameObjectAI(go), _instance(go->GetInstanceScript()) { }

    void UpdateAI(uint32 diff) override
    {
        if (!_started)
        {
            _started = true;
            Start();
            return;
        }

        if (!_wave)
            return;

        _waveElapsed += diff;
        uint32 const elapsedSeconds = _waveElapsed / IN_MILLISECONDS;

        for (std::size_t i = 0; i < std::size(ClassicScryerSummons); ++i)
            if (ClassicScryerSummons[i].Wave == _wave && !_summoned[i] && elapsedSeconds >= ClassicScryerSummons[i].DelaySeconds)
                Summon(i);

        ClassicScryerWave const& wave = ClassicScryerWaves[_wave - 1];
        if (elapsedSeconds >= wave.TimeLimitSeconds)
        {
            Fail();
            return;
        }

        if (elapsedSeconds < wave.CheckAfterSeconds)
            return;

        GameObject* ossuary = me->FindNearestGameObject(GO_THE_GREAT_OSSUARY, 50.0f);
        if (!(ossuary ? ossuary : me)->SelectNearestPlayer(60.0f))
        {
            Fail();
            return;
        }

        for (ObjectGuid const& guid : _waveSummons)
            if (Creature* summon = ObjectAccessor::GetCreature(*me, guid))
                if (summon->IsAlive())
                    return;

        if (_wave < std::size(ClassicScryerWaves))
            StartWave(_wave + 1);
        else
            Finish();
    }

private:
    void Start()
    {
        if (!_instance || _instance->GetData(TYPE_DARKREAVER) != NOT_STARTED)
            return;

        _instance->SetData(TYPE_DARKREAVER, IN_PROGRESS);
        if (GameObject* door = me->FindNearestGameObject(GO_OSSUARY_DOOR, 100.0f))
            door->SetGoState(GO_STATE_READY);

        StartWave(1);
    }

    void StartWave(uint8 wave)
    {
        _wave = wave;
        _waveElapsed = 0;
        _waveSummons.clear();
        _emoted = false;
    }

    void Summon(std::size_t index)
    {
        _summoned.set(index);
        ClassicScryerSummon const& data = ClassicScryerSummons[index];
        TempSummon* summon = data.DespawnMs
            ? me->SummonCreature(data.Entry, data.Pos, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, Milliseconds(data.DespawnMs))
            : me->SummonCreature(data.Entry, data.Pos, TEMPSUMMON_CORPSE_DESPAWN);
        if (!summon)
            return;

        _waveSummons.push_back(summon->GetGUID());
        _allSummons.push_back(summon->GetGUID());

        if (!_emoted && summon->GetEntry() == ClassicScryerWaves[_wave - 1].Emoter)
        {
            _emoted = true;
            summon->AI()->Talk(SAY_SPIRIT_EMERGES);
        }

        if (data.Attacks)
            if (Player* player = SelectAttackTarget())
                summon->AI()->AttackStart(player);
    }

    // VMaNGOS attacks the script's target, the player who placed the scryer; another player in reach when that one is gone
    Player* SelectAttackTarget() const
    {
        if (Player* owner = ObjectAccessor::GetPlayer(*me, me->GetOwnerGUID()))
            if (owner->IsAlive() && me->IsWithinDist(owner, 60.0f))
                return owner;
        return me->SelectNearestPlayer(60.0f);
    }

    // VMaNGOS generic_scripts 843600. The spirits still standing are sent away too, so a failed room does not stay full of them.
    void Fail()
    {
        _wave = 0;
        _instance->SetData(TYPE_DARKREAVER, FAIL);
        OpenDoor();

        for (ObjectGuid const& guid : _allSummons)
            if (Creature* summon = ObjectAccessor::GetCreature(*me, guid))
                if (summon->IsAlive())
                    summon->DespawnOrUnsummon();

        me->DespawnOrUnsummon();
    }

    // VMaNGOS generic_scripts 843611
    void Finish()
    {
        _wave = 0;
        _instance->SetData(TYPE_DARKREAVER, DONE);
        OpenDoor();
    }

    void OpenDoor()
    {
        if (GameObject* door = me->FindNearestGameObject(GO_OSSUARY_DOOR, 100.0f))
            door->SetGoState(GO_STATE_ACTIVE);
    }

    InstanceScript* _instance;
    bool _started = false;
    uint8 _wave = 0;
    uint32 _waveElapsed = 0;
    bool _emoted = false;
    std::bitset<std::size(ClassicScryerSummons)> _summoned;
    std::vector<ObjectGuid> _waveSummons;
    std::vector<ObjectGuid> _allSummons;
};

void AddSC_classic_instance_scholomance()
{
    new classic_instance_scholomance();
    RegisterGameObjectAI(classic_go_brazier_herald);
    RegisterGameObjectAI(classic_go_viewing_room_door);
    RegisterGameObjectAI(classic_go_divination_scryer);
    RegisterSpellScript(classic_spell_gandling_shadow_portal);
    RegisterSpellScript(classic_spell_gandling_shadow_portal_room);
}
