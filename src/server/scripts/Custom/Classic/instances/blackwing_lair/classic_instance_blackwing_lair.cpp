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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/burning_steppes/blackwing_lair/instance_blackwing_lair.cpp
// (ScriptDevZero lineage, GPL-2)
// Scripts: instance_blackwing_lair, go_orbe_domination, go_oeuf_raz, go_suppression, spell_bwl_suppression_aura (22247),
//          at_orb_of_command, at_enter_vael_room, npc_death_talon, npc_blackwing_technician, npc_corrupted_whelp

#include "ScriptMgr.h"
#include "Corpse.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureData.h"
#include "DB2Stores.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MapReference.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "QuaternionData.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "ThreatManager.h"
#include "classic_blackwing_lair.h"
#include "classic_script_text.h"
#include <algorithm>
#include <list>
#include <map>
#include <vector>

namespace
{
Position const ClassicBwlEggSpawnCoords[] =
{
    {-7579.49f, -1051.48f, 408.157f, 0.523599f},
    {-7563.15f, -1088.71f, 413.381f, -0.453786f},
    {-7568.27f, -1097.68f, 413.381f, 2.79253f},
    {-7578.64f, -1089.95f, 413.381f, 2.21657f},
    {-7568.62f, -1086.58f, 413.381f, 0.855211f},
    {-7572.49f, -1095.03f, 413.381f, -2.86234f},
    {-7569.38f, -1079.73f, 413.381f, -2.68781f},
    {-7576.92f, -1083.69f, 413.381f, -2.89725f},
    {-7580.80f, -1067.29f, 408.490f, -2.98451f},
    {-7592.38f, -1035.68f, 408.157f, 1.62316f},
    {-7588.84f, -1053.79f, 408.157f, -1.72788f},
    {-7597.53f, -1094.54f, 408.490f, 2.37365f},
    {-7594.37f, -1102.90f, 408.490f, -0.907571f},
    {-7584.68f, -1075.84f, 408.490f, 3.01942f},
    {-7601.14f, -1077.11f, 408.218f, -1.27409f},
    {-7599.00f, -1044.77f, 408.157f, -1.02974f},
    {-7604.36f, -1060.25f, 408.157f, -2.77507f},
    { -7618.1f, -1069.33f, 408.490f, -1.32645f},
    {-7619.76f, -1058.94f, 408.490f, 1.81514f},
    {-7586.36f, -1024.43f, 408.490f, -2.93215f},
    {-7609.94f, -1035.11f, 408.490f, -1.93731f},
    {-7628.32f, -1044.57f, 408.490f, -0.174533f},
    {-7592.35f, -1010.84f, 408.490f, -2.54818f},
    {-7611.60f, -1020.32f, 413.381f, 3.08923f},
    {-7626.69f, -1011.71f, 413.381f, 0.226893f},
    {-7577.84f, -1035.97f, 408.490f, -1.11701f},
    {-7564.89f, -1058.87f, 408.490f, 2.28638f},
    {-7566.00f, -1045.93f, 408.490f, 3.05433f},
    {-7554.42f, -1061.50f, 408.490f, -2.28638f},
    {-7549.48f, -1069.96f, 408.490f, -0.523599f},
};

enum ClassicBwlInstanceMisc : uint32
{
    CLASSIC_BWL_AT_ORB_OF_COMMAND           = 3847,
    CLASSIC_BWL_AT_ENTER_VAEL_ROOM          = 3626,

    CLASSIC_BWL_EGGS_COUNT                  = 30,

    CLASSIC_BWL_SPELL_NATURE_IMMUNITY       = 7941,
    CLASSIC_BWL_SPELL_FROST_IMMUNITY        = 7940,
    CLASSIC_BWL_SPELL_FIRE_IMMUNITY         = 7942,
    CLASSIC_BWL_SPELL_MIND_EXHAUSTION       = 23958,
    CLASSIC_BWL_SPELL_WARMING_FLAMES        = 23040,
    CLASSIC_BWL_SPELL_DESTROY_EGG           = 19873,

    CLASSIC_BWL_GO_DOOR_RAZORGORE_ENTER     = 176964,
    CLASSIC_BWL_GO_DOOR_RAZORGORE_EXIT      = 176965,
    CLASSIC_BWL_GO_DOOR_NEFARIAN            = 176966,
    CLASSIC_BWL_GO_DOOR_CHROMAGGUS_ENTER    = 179115,
    CLASSIC_BWL_GO_DOOR_CHROMAGGUS_SIDE     = 179116,
    CLASSIC_BWL_GO_LEVER_CHROMAGGUS_SIDE    = 179148,
    CLASSIC_BWL_GO_DOOR_CHROMAGGUS_EXIT     = 179117,
    CLASSIC_BWL_GO_DOOR_VAELASTRASZ         = 179364,
    CLASSIC_BWL_GO_DOOR_LASHLAYER           = 179365,
    CLASSIC_BWL_GO_BLACK_DRAGON_EGG         = 177807,
    CLASSIC_BWL_GO_ORB_OF_DOMINATION        = 177808,
    CLASSIC_BWL_GO_SUPPRESSION_ENGINE       = 179784,

    CLASSIC_BWL_SAY_RAZORGORE_DEATH         = 9591,

    CLASSIC_BWL_QUEST_BLACKHANDS_COMMAND    = 7761
};

// DungeonEncounter.db2 ids used by TC for Blackwing Lair; only registered when present in the client data
struct ClassicBwlEncounterId
{
    uint32 BossId;
    uint32 DungeonEncounterId;
};

ClassicBwlEncounterId const ClassicBwlEncounterIds[] =
{
    { CLASSIC_BWL_TYPE_RAZORGORE,   610 },
    { CLASSIC_BWL_TYPE_VAELASTRASZ, 611 },
    { CLASSIC_BWL_TYPE_LASHLAYER,   612 },
    { CLASSIC_BWL_TYPE_FIREMAW,     613 },
    { CLASSIC_BWL_TYPE_EBONROC,     614 },
    { CLASSIC_BWL_TYPE_FLAMEGOR,    615 },
    { CLASSIC_BWL_TYPE_CHROMAGGUS,  616 },
    { CLASSIC_BWL_TYPE_NEFARIAN,    617 }
};

// VMaNGOS AddAura(spell, ADD_AURA_PERMANENT)
void ClassicBwlAddPermanentAura(Unit* unit, uint32 spellId)
{
    if (Aura* aura = unit->AddAura(spellId, unit))
    {
        aura->SetMaxDuration(-1);
        aura->SetDuration(-1);
    }
}

// VMaNGOS DeleteLater() on a spawned creature: removed for the instance lifetime
void ClassicBwlRemoveCreature(Creature* creature)
{
    creature->DespawnOrUnsummon(0s, Seconds(7 * DAY));
}
}

/*######
## blackwing_technicians_helper
######*/

class ClassicBwlTechniciansHelper
{
public:
    explicit ClassicBwlTechniciansHelper(InstanceScript* pInstance) : m_bUpdated(false), m_uiTechniciansUpdate(0), m_pInstance(pInstance) { }

    // VMaNGOS moves every technician's threat into one shared table (all technicians attack the same target)
    void AddTechnician(Creature* pTechnician)
    {
        if (!pTechnician)
            return;

        m_vTechniciansGuid.push_back(pTechnician->GetGUID());

        // Copy the list, it is invalidated by ResetThreat
        std::vector<std::pair<Unit*, float>> threatList;
        for (ThreatReference const* ref : pTechnician->GetThreatManager().GetUnsortedThreatList())
            threatList.emplace_back(ref->GetVictim(), ref->GetThreat());

        for (auto const& [victim, threat] : threatList)
        {
            // VMaNGOS only merges creature victims here
            if (Creature* pCreature = victim->ToCreature())
            {
                m_mThreatGuid[pCreature->GetGUID()] += threat;
                pTechnician->GetThreatManager().ResetThreat(pCreature);
            }
        }
    }

    void RemoveTechnician(Creature* pTechnician)
    {
        for (auto itr = m_vTechniciansGuid.begin(); itr != m_vTechniciansGuid.end(); ++itr)
        {
            if (pTechnician->GetGUID() == *itr)
            {
                m_vTechniciansGuid.erase(itr);
                if (m_vTechniciansGuid.empty())
                    m_mThreatGuid.clear();
                return;
            }
        }
    }

    ObjectGuid GetVictimGuid() const
    {
        float fMaxThreat = 0.0f;
        ObjectGuid victimGuid;

        for (auto const& itr : m_mThreatGuid)
        {
            if (fMaxThreat <= itr.second)
            {
                fMaxThreat = itr.second;
                victimGuid = itr.first;
            }
        }
        return victimGuid;
    }

    void RemovePotentialVictim(ObjectGuid victimGuid)
    {
        // Victim died, must be removed from threat list or we can get stuck on it.
        m_mThreatGuid.erase(victimGuid);
    }

    InstanceScript* GetInstance() const { return m_pInstance; }

    void RecalculateThreat()
    {
        // Update when m_uiTechniciansUpdate == 0 (buffered update)
        if (m_uiTechniciansUpdate)
            --m_uiTechniciansUpdate;
        else
            m_bUpdated = false;

        if (!m_bUpdated)
        {
            m_uiTechniciansUpdate = uint32(m_vTechniciansGuid.size());
            m_bUpdated = true;
            for (ObjectGuid const& guid : m_vTechniciansGuid)
            {
                if (Creature* pCreature = m_pInstance->instance->GetCreature(guid))
                {
                    if (!pCreature->IsAlive())
                        continue;

                    // Copy the list, since it gets invalidated by ResetThreat
                    std::vector<std::pair<Unit*, float>> threatList;
                    for (ThreatReference const* ref : pCreature->GetThreatManager().GetUnsortedThreatList())
                        threatList.emplace_back(ref->GetVictim(), ref->GetThreat());

                    for (auto const& [victim, threat] : threatList)
                    {
                        m_mThreatGuid[victim->GetGUID()] += threat;
                        pCreature->GetThreatManager().ResetThreat(victim);
                    }
                }
            }
        }
    }

private:
    bool m_bUpdated;
    uint32 m_uiTechniciansUpdate;
    std::vector<ObjectGuid> m_vTechniciansGuid;
    std::map<ObjectGuid, float> m_mThreatGuid;
    InstanceScript* const m_pInstance;
};

/*######
## instance_blackwing_lair
######*/

class classic_instance_blackwing_lair : public InstanceMapScript
{
public:
    classic_instance_blackwing_lair() : InstanceMapScript(ClassicBlackwingLairScriptName, CLASSIC_BWL_MAP_ID) { }

    struct classic_instance_blackwing_lair_InstanceScript : public InstanceScript
    {
        explicit classic_instance_blackwing_lair_InstanceScript(InstanceMap* map) : InstanceScript(map),
            m_vaelEvent(*this, "VaelEvent", uint32(NOT_STARTED)),
            m_scepterRun(*this, "ScepterRun", uint32(NOT_STARTED)),
            m_chromBreath(*this, "ChromBreath", urand(0, 19)),     // 5 * 4 possible colours
            m_nefColor(*this, "NefColor", urand(0, 19)),
            m_scepterRunTimeSaved(*this, "ScepterRunTime", uint32(0)),
            m_scepterChampion(*this, "ScepterChampion", uint64(0)),
            m_hBlackwingTechnicians(this)
        {
            SetHeaders(ClassicBlackwingLairDataHeader);
            SetBossNumber(CLASSIC_BWL_MAX_BOSS_ENCOUNTER);

            std::vector<DungeonEncounterData> encounters;
            for (ClassicBwlEncounterId const& enc : ClassicBwlEncounterIds)
                if (sDungeonEncounterStore.LookupEntry(enc.DungeonEncounterId))
                    encounters.push_back({ enc.BossId, {{ enc.DungeonEncounterId }} });
            LoadDungeonEncounterData(encounters);
        }

        // VMaNGOS m_auiEncounter[8] / [9] and saved m_auiData entries
        PersistentInstanceScriptValue<uint32> m_vaelEvent;
        PersistentInstanceScriptValue<uint32> m_scepterRun;
        PersistentInstanceScriptValue<uint32> m_chromBreath;
        PersistentInstanceScriptValue<uint32> m_nefColor;
        PersistentInstanceScriptValue<uint32> m_scepterRunTimeSaved;
        PersistentInstanceScriptValue<uint64> m_scepterChampion;   // player guid counter

        // VMaNGOS writes DATA_SCEPTER_RUN_TIME every tick but only saves it with the other data; the live value is kept here
        uint32 m_scepterRunTime = 0;
        uint32 m_howEgg = 0;
        uint32 m_eggState = 0;

        ObjectGuid m_guids[CLASSIC_BWL_MAX_DATAS];
        std::list<ObjectGuid> m_lVaelGobs;
        std::vector<ObjectGuid> m_razorgoreGuards;
        GuidList m_pendingGoDeletes;
        uint32 m_uiGuardCheckTimer = 1000;
        ClassicBwlTechniciansHelper m_hBlackwingTechnicians;

        void AfterDataLoad() override
        {
            m_scepterRunTime = m_scepterRunTimeSaved;
        }

        // VMaNGOS saves only when an encounter is DONE or the scepter run state changes
        void SaveScepterRunTime()
        {
            if (uint32(m_scepterRunTimeSaved) != m_scepterRunTime)
                m_scepterRunTimeSaved = m_scepterRunTime;
        }

        void QueueGoDelete(GameObject* pGo)
        {
            m_pendingGoDeletes.push_back(pGo->GetGUID());
        }

        void OnGameObjectCreate(GameObject* pGo) override
        {
            InstanceScript::OnGameObjectCreate(pGo);

            switch (pGo->GetEntry())
            {
                case CLASSIC_BWL_GO_DOOR_RAZORGORE_ENTER:
                    m_guids[CLASSIC_BWL_DATA_DOOR_RAZORGORE_ENTER] = pGo->GetGUID();
                    break;
                case CLASSIC_BWL_GO_DOOR_RAZORGORE_EXIT:
                    m_guids[CLASSIC_BWL_DATA_DOOR_RAZORGORE_EXIT] = pGo->GetGUID();
                    if (GetBossState(CLASSIC_BWL_TYPE_RAZORGORE) == DONE)
                        pGo->SetGoState(GO_STATE_ACTIVE);
                    break;
                case CLASSIC_BWL_GO_DOOR_NEFARIAN:
                    m_guids[CLASSIC_BWL_DATA_DOOR_NEFARIAN] = pGo->GetGUID();
                    break;
                case CLASSIC_BWL_GO_DOOR_CHROMAGGUS_ENTER:
                    m_guids[CLASSIC_BWL_DATA_DOOR_CHROMAGGUS_ENTER] = pGo->GetGUID();
                    break;
                case CLASSIC_BWL_GO_DOOR_CHROMAGGUS_SIDE:
                    m_guids[CLASSIC_BWL_DATA_DOOR_CHROMAGGUS_SIDE] = pGo->GetGUID();
                    if (GetBossState(CLASSIC_BWL_TYPE_CHROMAGGUS) == DONE)
                        pGo->SetGoState(GO_STATE_ACTIVE);
                    break;
                case CLASSIC_BWL_GO_DOOR_CHROMAGGUS_EXIT:
                    m_guids[CLASSIC_BWL_DATA_DOOR_CHROMAGGUS_EXIT] = pGo->GetGUID();
                    if (GetBossState(CLASSIC_BWL_TYPE_CHROMAGGUS) == DONE)
                        pGo->SetGoState(GO_STATE_ACTIVE);
                    break;
                case CLASSIC_BWL_GO_DOOR_VAELASTRASZ:
                    m_guids[CLASSIC_BWL_DATA_DOOR_VAELASTRASZ] = pGo->GetGUID();
                    if (GetBossState(CLASSIC_BWL_TYPE_VAELASTRASZ) == DONE)
                        pGo->SetGoState(GO_STATE_ACTIVE);
                    break;
                case CLASSIC_BWL_GO_DOOR_LASHLAYER:
                    m_guids[CLASSIC_BWL_DATA_DOOR_LASHLAYER] = pGo->GetGUID();
                    if (GetBossState(CLASSIC_BWL_TYPE_LASHLAYER) == DONE)
                        QueueGoDelete(pGo);
                    break;
                case CLASSIC_BWL_GO_ORB_OF_DOMINATION:
                    m_guids[CLASSIC_BWL_DATA_ORB_DOMINATION_GUID] = pGo->GetGUID();
                    [[fallthrough]]; // VMaNGOS: "no break - intended?"
                case CLASSIC_BWL_GO_BLACK_DRAGON_EGG:
                    if (GetBossState(CLASSIC_BWL_TYPE_RAZORGORE) == DONE)
                        QueueGoDelete(pGo);
                    break;
                case CLASSIC_BWL_GO_SUPPRESSION_ENGINE:
                    if (GetBossState(CLASSIC_BWL_TYPE_LASHLAYER) == DONE)
                        QueueGoDelete(pGo);
                    break;
                default:
                    break;
            }
        }

        // VMaNGOS OnCreatureRespawn randomizes the whelp entry with UpdateEntry(); TC picks the entry before creation
        uint32 GetCreatureEntry(ObjectGuid::LowType /*spawnId*/, CreatureData const* data) override
        {
            switch (data->id)
            {
                case CLASSIC_BWL_NPC_CORRUPTED_GREEN_WHELP:
                case CLASSIC_BWL_NPC_CORRUPTED_BLUE_WHELP:
                case CLASSIC_BWL_NPC_CORRUPTED_RED_WHELP:
                case CLASSIC_BWL_NPC_CORRUPTED_BRONZE_WHELP:
                    switch (urand(0, 3))
                    {
                        case 0: return CLASSIC_BWL_NPC_CORRUPTED_BLUE_WHELP;
                        case 1: return CLASSIC_BWL_NPC_CORRUPTED_GREEN_WHELP;
                        case 2: return CLASSIC_BWL_NPC_CORRUPTED_RED_WHELP;
                        default: return CLASSIC_BWL_NPC_CORRUPTED_BRONZE_WHELP;
                    }
                default:
                    break;
            }
            return data->id;
        }

        bool AllDrakesDone() const
        {
            return GetBossState(CLASSIC_BWL_TYPE_FIREMAW) == DONE &&
                GetBossState(CLASSIC_BWL_TYPE_EBONROC) == DONE &&
                GetBossState(CLASSIC_BWL_TYPE_FLAMEGOR) == DONE;
        }

        // VMaNGOS OnCreatureCreate + OnCreatureRespawn (whelp respawn handling is in the whelp AI's JustAppeared)
        void OnCreatureCreate(Creature* pCreature) override
        {
            InstanceScript::OnCreatureCreate(pCreature);

            switch (pCreature->GetEntry())
            {
                case CLASSIC_BWL_NPC_RAZORGORE:
                    m_guids[CLASSIC_BWL_DATA_RAZORGORE_GUID] = pCreature->GetGUID();
                    break;
                case CLASSIC_BWL_NPC_VAELASTRASZ:
                    m_guids[CLASSIC_BWL_DATA_VAELASTRASZ_GUID] = pCreature->GetGUID();
                    if (GetBossState(CLASSIC_BWL_TYPE_VAELASTRASZ) == DONE)
                        ClassicBwlRemoveCreature(pCreature);
                    break;
                case CLASSIC_BWL_NPC_LASHLAYER:
                    m_guids[CLASSIC_BWL_DATA_LASHLAYER_GUID] = pCreature->GetGUID();
                    break;
                case CLASSIC_BWL_NPC_FIREMAW:
                    m_guids[CLASSIC_BWL_DATA_FIREMAW_GUID] = pCreature->GetGUID();
                    break;
                case CLASSIC_BWL_NPC_EBONROC:
                    m_guids[CLASSIC_BWL_DATA_EBONROC_GUID] = pCreature->GetGUID();
                    break;
                case CLASSIC_BWL_NPC_FLAMEGOR:
                    m_guids[CLASSIC_BWL_DATA_FLAMEGOR_GUID] = pCreature->GetGUID();
                    break;
                case CLASSIC_BWL_NPC_CHROMAGGUS:
                    m_guids[CLASSIC_BWL_DATA_CHROMAGGUS_GUID] = pCreature->GetGUID();
                    break;
                case CLASSIC_BWL_NPC_NEFARIAN:
                    m_guids[CLASSIC_BWL_DATA_NEFARIAN_GUID] = pCreature->GetGUID();
                    break;
                case CLASSIC_BWL_NPC_LORD_NEFARIAN:
                    if (pCreature->IsSummon()) // second nefarius summoned temporarily for vael event
                        break;
                    m_guids[CLASSIC_BWL_DATA_NEFARIUS_GUID] = pCreature->GetGUID();
                    if (GetBossState(CLASSIC_BWL_TYPE_NEFARIAN) == DONE)
                        ClassicBwlRemoveCreature(pCreature);
                    break;
                case CLASSIC_BWL_NPC_GRETHOK_THE_CONTROLLER:
                    m_guids[CLASSIC_BWL_DATA_GRETOK_GUID] = pCreature->GetGUID();
                    [[fallthrough]]; // VMaNGOS: "no break - intended?"
                case CLASSIC_BWL_NPC_BLACKWING_GUARDSMAN:
                    if (std::find(m_razorgoreGuards.begin(), m_razorgoreGuards.end(), pCreature->GetGUID()) == m_razorgoreGuards.end())
                        m_razorgoreGuards.push_back(pCreature->GetGUID());
                    if (GetBossState(CLASSIC_BWL_TYPE_RAZORGORE) == DONE)
                        ClassicBwlRemoveCreature(pCreature);
                    break;
                case CLASSIC_BWL_NPC_ORB_OF_DOMINATION:
                    m_guids[CLASSIC_BWL_DATA_TRIGGER_GUID] = pCreature->GetGUID();
                    break;
                case CLASSIC_BWL_NPC_DEATH_TALON_SEETHER:
                case CLASSIC_BWL_NPC_DEATH_TALON_WYRMKIN:
                case CLASSIC_BWL_NPC_DEATH_TALON_FLAMESCALE:
                case CLASSIC_BWL_NPC_DEATH_TALON_CAPTAIN:
                case CLASSIC_BWL_NPC_DEATH_TALON_HATCHER:
                case CLASSIC_BWL_NPC_BLACKWING_TASKMASTER:
                    if (GetBossState(CLASSIC_BWL_TYPE_LASHLAYER) == DONE)
                        ClassicBwlRemoveCreature(pCreature);
                    break;
                case CLASSIC_BWL_NPC_BLACKWING_TECHNICIAN:
                    if (pCreature->GetPositionZ() < 420.0f)
                    {
                        if (uint32(m_vaelEvent) == DONE)
                        {
                            ClassicBwlRemoveCreature(pCreature);
                            break;
                        }
                        if (GetBossState(CLASSIC_BWL_TYPE_RAZORGORE) != DONE)
                            pCreature->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE); // VMaNGOS UNIT_FLAG_SPAWNING
                        m_lVaelGobs.push_back(pCreature->GetGUID());
                    }
                    [[fallthrough]]; // VMaNGOS: "no break - intended?"
                case CLASSIC_BWL_NPC_BLACKWING_WARLOCK:
                case CLASSIC_BWL_NPC_BLACKWING_SPELLBINDER:
                case CLASSIC_BWL_NPC_DEATH_TALON_WYRMGUARD:
                case CLASSIC_BWL_NPC_DEATH_TALON_OVERSEER:
                    if (AllDrakesDone())
                        ClassicBwlRemoveCreature(pCreature);
                    break;
                default:
                    break;
            }
        }

        // VMaNGOS OnPlayerDeath
        void OnUnitDeath(Unit* unit) override
        {
            if (unit->GetTypeId() == TYPEID_PLAYER)
                m_hBlackwingTechnicians.RemovePotentialVictim(unit->GetGUID());
        }

        ObjectGuid GetGuidData(uint32 data) const override
        {
            if (data == CLASSIC_BWL_DATA_SCEPTER_CHAMPION)
            {
                uint64 const counter = m_scepterChampion;
                return counter ? ObjectGuid::Create<HighGuid::Player>(counter) : ObjectGuid::Empty;
            }

            if (data < CLASSIC_BWL_MAX_DATAS)
                return m_guids[data];
            return ObjectGuid::Empty;
        }

        void SetGuidData(uint32 type, ObjectGuid data) override
        {
            if (type == CLASSIC_BWL_DATA_SCEPTER_CHAMPION)
                m_scepterChampion = uint64(data.GetCounter());
        }

        uint32 GetData(uint32 uiType) const override
        {
            if (uiType < CLASSIC_BWL_MAX_BOSS_ENCOUNTER)
                return GetBossState(uiType);

            switch (uiType)
            {
                case CLASSIC_BWL_TYPE_VAEL_EVENT:       return m_vaelEvent;
                case CLASSIC_BWL_TYPE_SCEPTER_RUN:      return m_scepterRun;
                case CLASSIC_BWL_DATA_EGG:              return m_eggState;
                case CLASSIC_BWL_DATA_HOW_EGG:          return m_howEgg;
                case CLASSIC_BWL_DATA_CHROM_BREATH:     return m_chromBreath;
                case CLASSIC_BWL_DATA_NEF_COLOR:        return m_nefColor;
                case CLASSIC_BWL_DATA_SCEPTER_RUN_TIME: return m_scepterRunTime;
                default:
                    break;
            }
            return 0;
        }

        // Opens (state != ACTIVE -> toggle) or closes (state == ACTIVE -> toggle) a door the way VMaNGOS does
        void ToggleDoorIf(uint32 doorData, bool whenOpen)
        {
            if (GameObject* pGo = instance->GetGameObject(m_guids[doorData]))
            {
                bool const isOpen = pGo->GetGoState() == GO_STATE_ACTIVE;
                if (isOpen == whenOpen)
                    DoUseDoorOrButton(m_guids[doorData]);
            }
        }

        bool SetEncounter(uint32 type, uint32 data)
        {
            // TC blocks DONE -> other state transitions
            if (GetBossState(type) == DONE && data != DONE)
                return false;

            SetBossState(type, EncounterState(data));
            return true;
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            switch (uiType)
            {
                case CLASSIC_BWL_DATA_SCEPTER_RUN_TIME:
                    m_scepterRunTime = uiData;
                    break;
                case CLASSIC_BWL_TYPE_RAZORGORE:
                {
                    if (!SetEncounter(uiType, uiData))
                        return;
                    if (uiData == IN_PROGRESS)
                        ToggleDoorIf(CLASSIC_BWL_DATA_DOOR_RAZORGORE_ENTER, true);    // Open -> close
                    else if (uiData == FAIL)
                    {
                        m_howEgg = 0;
                        RespawnEggs();
                        SetData(CLASSIC_BWL_DATA_EGG, FAIL);
                        ToggleDoorIf(CLASSIC_BWL_DATA_DOOR_RAZORGORE_ENTER, false);   // Closed -> open
                    }
                    else if (uiData == DONE)
                    {
                        ToggleDoorIf(CLASSIC_BWL_DATA_DOOR_RAZORGORE_EXIT, false);
                        ToggleDoorIf(CLASSIC_BWL_DATA_DOOR_RAZORGORE_ENTER, false);
                        for (ObjectGuid const& guid : m_lVaelGobs)
                            if (Creature* pCreature = instance->GetCreature(guid))
                                pCreature->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE); // VMaNGOS UNIT_FLAG_SPAWNING
                    }
                    break;
                }
                case CLASSIC_BWL_TYPE_VAELASTRASZ:
                {
                    if (!SetEncounter(uiType, uiData))
                        return;
                    if (uiData == IN_PROGRESS)
                        ToggleDoorIf(CLASSIC_BWL_DATA_DOOR_RAZORGORE_EXIT, true);
                    else if (uiData == DONE)
                    {
                        ToggleDoorIf(CLASSIC_BWL_DATA_DOOR_RAZORGORE_EXIT, false);
                        ToggleDoorIf(CLASSIC_BWL_DATA_DOOR_VAELASTRASZ, false);
                    }
                    else if (uiData == FAIL)
                        ToggleDoorIf(CLASSIC_BWL_DATA_DOOR_RAZORGORE_EXIT, false);
                    break;
                }
                case CLASSIC_BWL_TYPE_LASHLAYER:
                {
                    if (!SetEncounter(uiType, uiData))
                        return;
                    if (uiData == DONE)
                        ToggleDoorIf(CLASSIC_BWL_DATA_DOOR_LASHLAYER, false);
                    break;
                }
                case CLASSIC_BWL_TYPE_FIREMAW:
                case CLASSIC_BWL_TYPE_EBONROC:
                case CLASSIC_BWL_TYPE_FLAMEGOR:
                    if (!SetEncounter(uiType, uiData))
                        return;
                    break;
                case CLASSIC_BWL_TYPE_CHROMAGGUS:
                {
                    if (!SetEncounter(uiType, uiData))
                        return;
                    if (uiData == DONE)
                        ToggleDoorIf(CLASSIC_BWL_DATA_DOOR_CHROMAGGUS_EXIT, false);
                    break;
                }
                case CLASSIC_BWL_TYPE_NEFARIAN:
                {
                    if (!SetEncounter(uiType, uiData))
                        return;
                    if (uiData == DONE || uiData == FAIL)
                        ToggleDoorIf(CLASSIC_BWL_DATA_DOOR_NEFARIAN, false);
                    if (uiData == IN_PROGRESS)
                        ToggleDoorIf(CLASSIC_BWL_DATA_DOOR_NEFARIAN, true);
                    break;
                }
                case CLASSIC_BWL_TYPE_VAEL_EVENT:
                {
                    m_vaelEvent = uiData;
                    if (uiData == DONE)
                    {
                        for (ObjectGuid const& guid : m_lVaelGobs)
                            if (Creature* pCreature = instance->GetCreature(guid))
                                pCreature->GetMotionMaster()->MovePoint(0, -7608.0f, -888.0f, 432.0f, true); // VMaNGOS MonsterMoveWithSpeed (run speed, pathfinding)
                    }
                    break;
                }
                case CLASSIC_BWL_DATA_EGG:
                {
                    if (uiData == IN_PROGRESS)
                    {
                        if (++m_howEgg >= CLASSIC_BWL_EGGS_COUNT)
                            m_eggState = DONE;
                    }
                    else if (uiData == FAIL)
                    {
                        if (Creature* pCreature = instance->GetCreature(m_guids[CLASSIC_BWL_DATA_TRIGGER_GUID]))
                            pCreature->DespawnOrUnsummon();
                        m_guids[CLASSIC_BWL_DATA_TRIGGER_GUID].Clear();
                        m_eggState = FAIL;
                    }
                    break;
                }
                case CLASSIC_BWL_TYPE_SCEPTER_RUN:
                    m_scepterRun = uiData;
                    break;
                case CLASSIC_BWL_GOSSIP_OPTION_NEFARIUS:
                {
                    if (Creature* pCreature = instance->GetCreature(m_guids[CLASSIC_BWL_DATA_NEFARIUS_GUID]))
                        if (pCreature->AI())
                            pCreature->AI()->DoAction(CLASSIC_BWL_ACTION_NEFARIUS_START);
                    break;
                }
                default:
                    break;
            }

            if (uiData == DONE || uiType == CLASSIC_BWL_TYPE_SCEPTER_RUN)
                SaveScepterRunTime();
        }

        // TODO(classic): VMaNGOS CheckConditionCriteriaMeet(CONDITION_SCEPTER_FAIL = 1 / CONDITION_SCEPTER_WIN = 2) gates the scepter
        // run loot ("From the Desk of Lord Victor Nefarius" / Red Scepter Shard). TC has no per-instance condition hook: FAIL can be
        // expressed as CONDITION_INSTANCE_INFO (data 9 == FAIL); WIN needs GetData(9) == DONE and player == GetGuidData(24).

        void RespawnEggs()
        {
            Creature* pCreature = instance->GetCreature(m_guids[CLASSIC_BWL_DATA_RAZORGORE_GUID]);
            if (!pCreature)
                return;

            std::list<GameObject*> lGameObjects;
            pCreature->GetGameObjectListWithEntryInGrid(lGameObjects, CLASSIC_BWL_GO_BLACK_DRAGON_EGG, 250.0f);
            for (GameObject* pGo : lGameObjects)
                pGo->Delete();

            // Not linked to Razorgore: he is despawned/respawned right after a phase one wipe
            for (Position const& position : ClassicBwlEggSpawnCoords)
                pCreature->SummonGameObject(CLASSIC_BWL_GO_BLACK_DRAGON_EGG, position, QuaternionData::fromEulerAnglesZYX(position.GetOrientation(), 0.0f, 0.0f), 0s, GO_SUMMON_TIMED_DESPAWN);
        }

        ClassicBwlTechniciansHelper* GetTechnicianHelper() { return &m_hBlackwingTechnicians; }

        void Update(uint32 diff) override
        {
            // VMaNGOS GameObject::DeleteLater() from OnObjectCreate
            if (!m_pendingGoDeletes.empty())
            {
                for (ObjectGuid const& guid : m_pendingGoDeletes)
                    if (GameObject* pGo = instance->GetGameObject(guid))
                        pGo->DespawnOrUnsummon(0ms, Seconds(7 * DAY));
                m_pendingGoDeletes.clear();
            }

            // VMaNGOS OnCreatureEnterCombat(Grethok / Blackwing Guardsman): TC has no instance hook, poll instead
            if (m_uiGuardCheckTimer <= diff)
            {
                m_uiGuardCheckTimer = 1000;

                bool guardEngaged = false;
                for (ObjectGuid const& guid : m_razorgoreGuards)
                    if (Creature* pGuard = instance->GetCreature(guid))
                        if (pGuard->IsAlive() && pGuard->IsInCombat())
                            guardEngaged = true;

                if (guardEngaged)
                {
                    Creature* pRazorgore = instance->GetCreature(m_guids[CLASSIC_BWL_DATA_RAZORGORE_GUID]);
                    if (pRazorgore && pRazorgore->IsAlive() && !pRazorgore->IsInCombat())
                    {
                        if (GetBossState(CLASSIC_BWL_TYPE_RAZORGORE) == NOT_STARTED)
                            SetData(CLASSIC_BWL_TYPE_RAZORGORE, IN_PROGRESS);
                        CreatureAI::DoZoneInCombat(pRazorgore);
                    }
                }
            }
            else
                m_uiGuardCheckTimer -= diff;
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new classic_instance_blackwing_lair_InstanceScript(map);
    }
};

using ClassicBwlInstanceScript = classic_instance_blackwing_lair::classic_instance_blackwing_lair_InstanceScript;

/*######
## go_orbe_domination
######*/

struct classic_go_orbe_domination : public GameObjectAI
{
    classic_go_orbe_domination(GameObject* go) : GameObjectAI(go) { }

    // VMaNGOS SetUInt64Value(UNIT_FIELD_CHANNEL_OBJECT) + SetUInt32Value(UNIT_CHANNEL_SPELL) (channel visual without a cast)
    static void SetChannel(Unit* unit, ObjectGuid target, uint32 spellId)
    {
        unit->ClearChannelObjects();
        if (!target.IsEmpty())
            unit->AddChannelObject(target);
        unit->SetChannelSpellId(spellId);
        SpellCastVisual visual;
        if (spellId)
            if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, unit->GetMap()->GetDifficultyID()))
                visual.SpellXSpellVisualID = unit->GetCastSpellXSpellVisualId(spellInfo);
        unit->SetChannelVisual(visual);
    }

    // Every refusal says why in the log (classic.bwl.orb): the orb used to swallow the click silently, so a player who could not
    // take control had nothing to go on.
    bool OnGossipHello(Player* pPlayer) override
    {
        InstanceScript* pInstance = me->GetInstanceScript();
        if (!pInstance)
        {
            TC_LOG_INFO("classic.bwl.orb", "Orb of Domination used by {}: no instance script", pPlayer->GetName());
            return true;
        }

        if (pPlayer->HasAura(CLASSIC_BWL_SPELL_MIND_EXHAUSTION))
        {
            TC_LOG_INFO("classic.bwl.orb", "Orb of Domination used by {}: refused, Mind Exhaustion", pPlayer->GetName());
            return true;
        }

        Creature* pCreature = me->GetMap()->GetCreature(pInstance->GetGuidData(CLASSIC_BWL_DATA_RAZORGORE_GUID));
        if (!pCreature)
        {
            TC_LOG_INFO("classic.bwl.orb", "Orb of Domination used by {}: refused, Razorgore not found", pPlayer->GetName());
            return true;
        }

        if (pCreature->HasUnitState(UNIT_STATE_POSSESSED))
        {
            TC_LOG_INFO("classic.bwl.orb", "Orb of Domination used by {}: refused, Razorgore is already controlled", pPlayer->GetName());
            return true;
        }

        if (pCreature->IsInEvadeMode() || !pCreature->IsAlive() || pInstance->GetData(CLASSIC_BWL_DATA_EGG) == DONE)
        {
            TC_LOG_INFO("classic.bwl.orb", "Orb of Domination used by {}: refused, evade={} alive={} eggsDone={}", pPlayer->GetName(),
                pCreature->IsInEvadeMode(), pCreature->IsAlive(), pInstance->GetData(CLASSIC_BWL_DATA_EGG) == DONE);
            return true;
        }

        // The orb works from the start of the encounter. A raid that clicks it before Razorgore has been pulled starts the fight
        // (he engages the player, JustEngagedWith sets the boss state) instead of the click being swallowed.
        if (!pCreature->IsInCombat() && pCreature->AI())
        {
            TC_LOG_INFO("classic.bwl.orb", "Orb of Domination used by {}: Razorgore not engaged yet, engaging the player", pPlayer->GetName());
            pCreature->AI()->AttackStart(pPlayer);
        }

        SpellCastResult exhausted = pPlayer->CastSpell(pPlayer, CLASSIC_BWL_SPELL_MIND_EXHAUSTION, true);
        SpellCastResult possessed = pPlayer->CastSpell(pCreature, CLASSIC_BWL_SPELL_POSSESS, true);
        TC_LOG_INFO("classic.bwl.orb", "Orb of Domination used by {}: Mind Exhaustion cast result {}, Possess cast result {}", pPlayer->GetName(),
            uint32(exhausted), uint32(possessed));

        if (possessed == SPELL_CAST_OK)
        {
            if (Creature* pTrigger = me->GetMap()->GetCreature(pInstance->GetGuidData(CLASSIC_BWL_DATA_TRIGGER_GUID)))
            {
                pCreature->GetThreatManager().AddThreat(pTrigger, 100.0f);
                SetChannel(pTrigger, pCreature->GetGUID(), CLASSIC_BWL_SPELL_POSSESS_VISUAL);
            }
        }

        return true;
    }
};

/*######
## go_oeuf_raz
######*/

namespace
{
enum ClassicBwlEggsOfRaz : uint32
{
    CLASSIC_BWL_SAY_EGGS_BROKEN_1 = 9961,
    CLASSIC_BWL_SAY_EGGS_BROKEN_2 = 9962,
    CLASSIC_BWL_SAY_EGGS_BROKEN_3 = 9963
};
}

struct classic_go_oeuf_raz : public GameObjectAI
{
    classic_go_oeuf_raz(GameObject* go) : GameObjectAI(go) { }

    bool m_bUsed = false;

    // VMaNGOS OnUse(Unit*): used by a player or by Razorgore's Destroy Egg (activate object -> Use)
    void HandleUse(Unit* pUser)
    {
        if (!pUser || m_bUsed)
            return;

        if (InstanceScript* pInstance = me->GetInstanceScript())
        {
            m_bUsed = true;
            pInstance->SetData(CLASSIC_BWL_DATA_EGG, IN_PROGRESS);

            if (pUser->GetTypeId() == TYPEID_UNIT && pUser->GetEntry() == CLASSIC_BWL_NPC_RAZORGORE)
            {
                switch (urand(0, 5))
                {
                    case 0: ClassicScriptText(CLASSIC_BWL_SAY_EGGS_BROKEN_1, pUser); break;
                    case 1: ClassicScriptText(CLASSIC_BWL_SAY_EGGS_BROKEN_2, pUser); break;
                    case 2: ClassicScriptText(CLASSIC_BWL_SAY_EGGS_BROKEN_3, pUser); break;
                    default: break;
                }

                if (pInstance->GetData(CLASSIC_BWL_DATA_EGG) == DONE)
                {
                    pUser->RemoveAllAuras();
                    if (pUser->GetMaxHealth() != CLASSIC_BWL_RAZORGORE_MAX_HEALTH_DURING_POSESSION)
                        pUser->SetMaxHealth(CLASSIC_BWL_RAZORGORE_MAX_HEALTH_DURING_POSESSION);
                    pUser->CastSpell(pUser, CLASSIC_BWL_SPELL_WARMING_FLAMES, true);
                }
            }
            me->Delete();
        }
    }

    bool OnGossipHello(Player* player) override
    {
        HandleUse(player);
        return true;
    }

    // TC GameObject::Use() does not call the AI for non-player users; Destroy Egg (19873, activate object) hits the egg here
    void SpellHit(WorldObject* caster, SpellInfo const* /*spellInfo*/) override
    {
        if (Unit* unitCaster = caster ? caster->ToUnit() : nullptr)
            HandleUse(unitCaster);
    }
};

/*######
## go_chromaggus_lever
######*/

// The lever opens Chromaggus's side portcullis; the boss script sees the open door and walks out. The template used to keep the
// stock go_chromaggus_lever script, which belongs to the stock instance script and does nothing on our map, so a pull of the lever
// left the door shut and Chromaggus unattackable (scenario bwl_chromaggus 2026-10-28, "still not attackable 30 s after 179148").
// OnLootStateChanged covers every way in: a player click, a bot use, or a script calling Use().
struct classic_go_chromaggus_lever : public GameObjectAI
{
    classic_go_chromaggus_lever(GameObject* go) : GameObjectAI(go) { }

    void OnLootStateChanged(uint32 state, Unit* unit) override
    {
        if (state != GO_ACTIVATED)
            return;

        InstanceScript* pInstance = me->GetInstanceScript();
        if (!pInstance)
            return;

        GameObject* pDoor = me->GetMap()->GetGameObject(pInstance->GetGuidData(CLASSIC_BWL_DATA_DOOR_CHROMAGGUS_SIDE));
        if (!pDoor)
        {
            TC_LOG_INFO("classic.bwl.lever", "Chromaggus lever used by {}: side door not loaded", unit ? unit->GetName() : "nobody");
            return;
        }

        pDoor->SetGoState(GO_STATE_ACTIVE);
        TC_LOG_INFO("classic.bwl.lever", "Chromaggus lever used by {}: side door opened", unit ? unit->GetName() : "nobody");
    }
};

/*###############
## go_suppression
################*/

struct classic_go_suppression : public GameObjectAI
{
    classic_go_suppression(GameObject* go) : GameObjectAI(go), m_uiFumeTimer(urand(0, 5 * IN_MILLISECONDS)) { }

    uint32 m_uiFumeTimer;

    // VMaNGOS OnUse: the device is disarmed (TC: Disarm Trap / open lock -> GameObject::Use(player) -> OnGossipHello)
    bool OnGossipHello(Player* /*player*/) override
    {
        InstanceScript* pInstance = me->GetInstanceScript();
        if (!pInstance)
            return false;

        // As long as Broodlord Lashlayer is alive, the GO will rearm on a random timer from 30 sec to 2 min
        // It will not rearm for the instance lifetime after Broodlord Lashlayer death
        if (me->GetGoState() == GO_STATE_READY)
        {
            if (pInstance->GetData(CLASSIC_BWL_TYPE_LASHLAYER) != DONE)
                m_uiFumeTimer = urand(30, 2 * MINUTE) * IN_MILLISECONDS;
            else
                m_uiFumeTimer = WEEK * IN_MILLISECONDS;

            me->SetFlag(GO_FLAG_IN_USE);
            me->SetGoState(GO_STATE_ACTIVE);
            me->SetLootState(GO_READY);
        }

        return true;
    }

    // Visual effects for each GO is played on a 5 seconds timer. Sniff show that the GO should also be used (trap spell is cast).
    void UpdateAI(uint32 uiDiff) override
    {
        if (m_uiFumeTimer)
        {
            if (m_uiFumeTimer <= uiDiff)
            {
                if (me->GetGoState() == GO_STATE_READY)
                {
                    me->SendCustomAnim(0);
                    if (uint32 trapSpell = me->GetGOInfo()->trap.spell)
                        me->CastSpell(nullptr, trapSpell, true);
                }
                else
                {
                    me->RemoveFlag(GO_FLAG_IN_USE);
                    me->SetGoState(GO_STATE_READY);
                }

                m_uiFumeTimer = 5 * IN_MILLISECONDS;
            }
            else
                m_uiFumeTimer -= uiDiff;
        }
    }
};

// 22247 - Suppression Aura
// VMaNGOS OnCheckTarget: stealthed units are not affected.
// TODO(classic): VMaNGOS data has all 3 effects TARGET_DEST_CASTER / TARGET_UNIT_DEST_AREA_ENEMY; verify 1.60 client data.
class classic_spell_bwl_suppression_aura : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject* obj)
        {
            Unit* unit = obj->ToUnit();
            return unit && unit->HasStealthAura();
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(classic_spell_bwl_suppression_aura::FilterTargets, EFFECT_ALL, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

/*######
## at_orb_of_command (Blackrock Spire, map 0)
######*/

class classic_at_orb_of_command : public AreaTriggerScript
{
public:
    classic_at_orb_of_command() : AreaTriggerScript("classic_at_orb_of_command") { }

    bool OnTrigger(Player* pPlayer, AreaTriggerEntry const* pAt) override
    {
        if (pAt->ID == CLASSIC_BWL_AT_ORB_OF_COMMAND)
        {
            Corpse* pCorpse = pPlayer->GetCorpse();
            if (pCorpse &&
                pPlayer->isDead() &&
                pPlayer->GetQuestRewardStatus(CLASSIC_BWL_QUEST_BLACKHANDS_COMMAND) &&
                pCorpse->GetMapId() == CLASSIC_BWL_MAP_ID)
            {
                pPlayer->ResurrectPlayer(0.5f);
                pPlayer->SpawnCorpseBones();
                pPlayer->TeleportTo(CLASSIC_BWL_MAP_ID, -7672.32f, -1107.05f, 396.651f, 0.785398f);
            }
        }

        return false;
    }
};

/*######
## at_enter_vael_room
######*/

class classic_at_enter_vael_room : public AreaTriggerScript
{
public:
    classic_at_enter_vael_room() : AreaTriggerScript("classic_at_enter_vael_room") { }

    bool OnTrigger(Player* pPlayer, AreaTriggerEntry const* pAt) override
    {
        if (pAt->ID == CLASSIC_BWL_AT_ENTER_VAEL_ROOM)
        {
            if (pPlayer->IsGameMaster())
                return false;

            if (InstanceScript* pInstance = pPlayer->GetInstanceScript())
            {
                if (pInstance->GetData(CLASSIC_BWL_TYPE_VAEL_EVENT) != DONE)
                    pInstance->SetData(CLASSIC_BWL_TYPE_VAEL_EVENT, DONE);
            }
        }

        return false;
    }
};

/*######
## npc_death_talon
######*/

namespace
{
enum ClassicBwlDeathTalon : uint32
{
    CLASSIC_BWL_SPELL_DT_CLEAVE                = 15284,
    CLASSIC_BWL_SPELL_DT_WARSTOMP              = 24375,
    CLASSIC_BWL_SPELL_DT_FIREBLAST             = 20623,
    CLASSIC_BWL_SPELL_BROODPOWER_BLUE          = 22285,
    CLASSIC_BWL_SPELL_BROODPOWER_BLACK         = 22287,
    CLASSIC_BWL_SPELL_BROODPOWER_BRONZE        = 22286,
    CLASSIC_BWL_SPELL_BROODPOWER_RED           = 22283,
    CLASSIC_BWL_SPELL_BROODPOWER_GREEN         = 22288,

    CLASSIC_BWL_SPELL_DT_FIRE_VULNERABILITY    = 22277,
    CLASSIC_BWL_SPELL_DT_FROST_VULNERABILITY   = 22278,
    CLASSIC_BWL_SPELL_DT_SHADOW_VULNERABILITY  = 22279,
    CLASSIC_BWL_SPELL_DT_NATURE_VULNERABILITY  = 22280,
    CLASSIC_BWL_SPELL_DT_ARCANE_VULNERABILITY  = 22281
};

uint32 const ClassicBwlBroodPowers[] =
{
    CLASSIC_BWL_SPELL_BROODPOWER_BLUE, CLASSIC_BWL_SPELL_BROODPOWER_BLACK, CLASSIC_BWL_SPELL_BROODPOWER_BRONZE,
    CLASSIC_BWL_SPELL_BROODPOWER_RED, CLASSIC_BWL_SPELL_BROODPOWER_GREEN
};

uint32 const ClassicBwlSchoolSensibilities[] =
{
    CLASSIC_BWL_SPELL_DT_FIRE_VULNERABILITY, CLASSIC_BWL_SPELL_DT_FROST_VULNERABILITY, CLASSIC_BWL_SPELL_DT_SHADOW_VULNERABILITY,
    CLASSIC_BWL_SPELL_DT_NATURE_VULNERABILITY, CLASSIC_BWL_SPELL_DT_ARCANE_VULNERABILITY
};
}

struct classic_npc_death_talon : public ScriptedAI
{
    classic_npc_death_talon(Creature* creature) : ScriptedAI(creature)
    {
        m_uiBroodPower = RandomPower();
        m_uiSchoolSensibility = RandomSensibility();
        m_bIsOverSeer = (creature->GetEntry() == CLASSIC_BWL_NPC_DEATH_TALON_OVERSEER);
    }

    uint32 m_uiCleaveTimer = 0;
    uint32 m_uiWarStompTimer = 0;
    uint32 m_uiFireBlastTimer = 0;
    uint32 m_uiBroodPower = 0;
    uint32 m_uiSchoolSensibility = 0;
    bool m_bIsOverSeer = false;

    static uint32 RandomPower() { return ClassicBwlBroodPowers[urand(0, 4)]; }
    static uint32 RandomSensibility() { return ClassicBwlSchoolSensibilities[urand(0, 4)]; }

    void Reset() override
    {
        m_uiCleaveTimer     = urand(5000, 9000);
        m_uiWarStompTimer   = 8000;
        m_uiFireBlastTimer  = 8000;
    }

    void JustDied(Unit* /*killer*/) override
    {
        m_uiBroodPower = RandomPower();
        m_uiSchoolSensibility = RandomSensibility();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        // aggro Master Elementalist with the pull
        if (!m_bIsOverSeer)
            me->CallForHelp(15.0f);
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (!m_bIsOverSeer && !me->HasAura(m_uiBroodPower))
            me->AddAura(m_uiBroodPower, me);

        if (!me->HasAura(m_uiSchoolSensibility))
            me->AddAura(m_uiSchoolSensibility, me);

        if (!UpdateVictim())
            return;

        if (m_uiCleaveTimer < uiDiff)
        {
            if (DoCastVictim(CLASSIC_BWL_SPELL_DT_CLEAVE) == SPELL_CAST_OK)
                m_uiCleaveTimer = urand(5000, 9000);
        }
        else
            m_uiCleaveTimer -= uiDiff;

        if (!m_bIsOverSeer)
        {
            if (m_uiWarStompTimer < uiDiff)
            {
                if (DoCastSelf(CLASSIC_BWL_SPELL_DT_WARSTOMP) == SPELL_CAST_OK)
                    m_uiWarStompTimer = urand(8000, 14000);
            }
            else
                m_uiWarStompTimer -= uiDiff;
        }
        else
        {
            if (m_uiFireBlastTimer < uiDiff)
            {
                if (Unit* pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                {
                    if (DoCast(pTarget, CLASSIC_BWL_SPELL_DT_FIREBLAST) == SPELL_CAST_OK)
                        m_uiFireBlastTimer = 10000;
                }
            }
            else
                m_uiFireBlastTimer -= uiDiff;
        }
    }
};

/*######
## npc_blackwing_technician
######*/

namespace
{
enum ClassicBwlTechnician : uint32
{
    CLASSIC_BWL_SPELL_BOMB          = 22334,
    CLASSIC_BWL_SPELL_POISON_BOTTLE = 22335
};
}

struct classic_npc_blackwing_technician : public ScriptedAI
{
    classic_npc_blackwing_technician(Creature* creature) : ScriptedAI(creature), m_pTechnicianHelper(nullptr)
    {
        m_bAdded = false;
        m_bVaelGob = (creature->GetPositionZ() < 420.0f);

        if (ClassicBwlInstanceScript* pBlackwingLair = dynamic_cast<ClassicBwlInstanceScript*>(creature->GetInstanceScript()))
            m_pTechnicianHelper = pBlackwingLair->GetTechnicianHelper();
    }

    uint32 m_uiPoisonBottleTimer = 2000;
    uint32 m_uiAggroSyncTimer = 5000;
    uint32 m_uiEmoteTimer = 0;
    bool m_bVaelGob;
    bool m_bAdded;
    ClassicBwlTechniciansHelper* m_pTechnicianHelper;

    void Reset() override
    {
        if (m_pTechnicianHelper && m_bAdded)
        {
            m_pTechnicianHelper->RemoveTechnician(me);
            m_bAdded = false;
        }
        m_uiPoisonBottleTimer = 2000;
        m_uiAggroSyncTimer = 5000;
        if (!m_bVaelGob)
            m_uiEmoteTimer = urand(0, 1) ? 1000 : 3000;
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (!m_bVaelGob)
            ScriptedAI::MoveInLineOfSight(pWho);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (m_pTechnicianHelper && !m_bAdded)
        {
            m_pTechnicianHelper->AddTechnician(me);
            m_bAdded = true;
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (m_pTechnicianHelper && m_bAdded)
        {
            m_pTechnicianHelper->RemoveTechnician(me);
            m_bAdded = false;
        }
    }

    void UpdateAI(uint32 uiDiff) override
    {
        if (m_bVaelGob && me->GetPositionZ() >= 430.0f)
        {
            ClassicBwlRemoveCreature(me);
            return;
        }

        if (m_uiEmoteTimer)
        {
            if (m_uiEmoteTimer <= uiDiff)
            {
                me->SetEmoteState(EMOTE_STATE_USE_STANDING_NO_SHEATHE); // VMaNGOS HandleEmote(EMOTE_STATE_USESTANDING_NOSHEATHE)
                m_uiEmoteTimer = 0;
            }
            else
                m_uiEmoteTimer -= uiDiff;
        }

        // Port: the technicians never call UpdateVictim() (their threat is reset constantly); evade when nobody is left
        if (me->IsEngaged() && me->GetThreatManager().IsThreatListEmpty())
        {
            EnterEvadeMode(EvadeReason::NoHostiles);
            return;
        }

        // If we don't have a current victim and we're not added to the helper, stop AI.
        if (!me->GetVictim() && !m_bAdded)
            return;

        if (m_pTechnicianHelper)
        {
            if (m_uiAggroSyncTimer <= uiDiff)
            {
                m_pTechnicianHelper->RecalculateThreat();
                m_uiAggroSyncTimer = 2500;
            }
            else
                m_uiAggroSyncTimer -= uiDiff;
        }

        Unit* victim = me->GetVictim();
        if (m_pTechnicianHelper)
        {
            ObjectGuid victimGuid = m_pTechnicianHelper->GetVictimGuid();
            if (!victimGuid.IsEmpty())
                victim = ObjectAccessor::GetUnit(*me, victimGuid);
        }

        if (!victim)
            return;

        AttackStart(victim);

        if (m_uiPoisonBottleTimer <= uiDiff)
            m_uiPoisonBottleTimer = 0;
        else
            m_uiPoisonBottleTimer -= uiDiff;

        if (me->IsWithinLOSInMap(victim))
        {
            if (!m_uiPoisonBottleTimer && DoCast(victim, CLASSIC_BWL_SPELL_POISON_BOTTLE) == SPELL_CAST_OK)
                m_uiPoisonBottleTimer = urand(3500, 6000);
            else
                me->CastSpell(victim->GetPosition(), CLASSIC_BWL_SPELL_BOMB, false);
        }
    }
};

/*######
## npc_corrupted_whelp
######*/

struct classic_npc_corrupted_whelp : public ScriptedAI
{
    classic_npc_corrupted_whelp(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override { }

    void AddImmunity()
    {
        switch (me->GetEntry())
        {
            case CLASSIC_BWL_NPC_CORRUPTED_GREEN_WHELP:  ClassicBwlAddPermanentAura(me, CLASSIC_BWL_SPELL_NATURE_IMMUNITY); break;
            case CLASSIC_BWL_NPC_CORRUPTED_BLUE_WHELP:   ClassicBwlAddPermanentAura(me, CLASSIC_BWL_SPELL_FROST_IMMUNITY); break;
            case CLASSIC_BWL_NPC_CORRUPTED_RED_WHELP:    ClassicBwlAddPermanentAura(me, CLASSIC_BWL_SPELL_FIRE_IMMUNITY); break;
            // VMaNGOS gives the bronze whelp its custom spell 33020; 70532 has no arcane immunity aura, so the school immunity is set directly.
            case CLASSIC_BWL_NPC_CORRUPTED_BRONZE_WHELP: me->ApplySpellImmune(0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_ARCANE, true); break;
            default: break;
        }
    }

    // VMaNGOS instance OnCreatureCreate / OnCreatureRespawn for the whelps (entry randomized in GetCreatureEntry)
    void JustAppeared() override
    {
        ScriptedAI::JustAppeared();

        me->RemoveAllAuras();
        AddImmunity();

        InstanceScript* pInstance = me->GetInstanceScript();
        if (!pInstance)
            return;

        uint32 const lashlayer = pInstance->GetData(CLASSIC_BWL_TYPE_LASHLAYER);
        if (lashlayer == DONE)
            ClassicBwlRemoveCreature(me);
        else if (lashlayer == IN_PROGRESS)
        {
            me->SetVisible(false);
            me->SetUninteractible(true);
            me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE); // VMaNGOS UNIT_FLAG_SPAWNING
            me->SetImmuneToNPC(true);
            me->DespawnOrUnsummon(1s);
        }
        else if (lashlayer == FAIL)
        {
            me->SetVisible(true);
            me->SetUninteractible(false);
            me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            me->SetImmuneToNPC(false);
        }
    }

    // VMaNGOS instance OnCreatureEvade
    void EnterEvadeMode(EvadeReason why) override
    {
        ScriptedAI::EnterEvadeMode(why);
        AddImmunity();
    }

    void UpdateAI(uint32 /*uiDiff*/) override
    {
        UpdateVictim();
    }
};

void AddSC_classic_instance_blackwing_lair()
{
    new classic_instance_blackwing_lair();
    RegisterGameObjectAI(classic_go_orbe_domination);
    RegisterGameObjectAI(classic_go_oeuf_raz);
    RegisterGameObjectAI(classic_go_suppression);
    RegisterGameObjectAI(classic_go_chromaggus_lever);
    RegisterSpellScript(classic_spell_bwl_suppression_aura);
    new classic_at_orb_of_command();
    new classic_at_enter_vael_room();
    RegisterCreatureAI(classic_npc_death_talon);
    RegisterCreatureAI(classic_npc_blackwing_technician);
    RegisterCreatureAI(classic_npc_corrupted_whelp);
}
