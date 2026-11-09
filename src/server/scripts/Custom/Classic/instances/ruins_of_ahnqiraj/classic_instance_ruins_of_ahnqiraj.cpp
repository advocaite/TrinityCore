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

// Classic 1.60 port of VMaNGOS src/scripts/kalimdor/silithus/ruins_of_ahnqiraj/instance_ruins_of_ahnqiraj.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: instance_ruins_of_ahnqiraj
// VMaNGOS patch checks (sWorld.GetWowPatch()) always take the 1.10+ branch (Classic 1.60 client).

#include "ScriptMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureData.h"
#include "CreatureGroups.h"
#include "DB2Stores.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuaternionData.h"
#include "ReputationMgr.h"
#include "TemporarySummon.h"
#include "ThreatManager.h"
#include "classic_ruins_of_ahnqiraj.h"
#include "classic_script_text.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <list>
#include <random>
#include <unordered_map>
#include <vector>

namespace
{
// DungeonEncounter.db2 ids used by TC for AQ20; only registered when present in the client data
struct ClassicAQ20EncounterId
{
    uint32 BossId;
    uint32 DungeonEncounterId;
};

ClassicAQ20EncounterId const ClassicAQ20EncounterIds[] =
{
    { CLASSIC_AQ20_TYPE_KURINNAXX, 718 },
    { CLASSIC_AQ20_TYPE_RAJAXX,    719 },
    { CLASSIC_AQ20_TYPE_MOAM,      720 },
    { CLASSIC_AQ20_TYPE_BURU,      721 },
    { CLASSIC_AQ20_TYPE_AYAMISS,   722 },
    { CLASSIC_AQ20_TYPE_OSSIRIAN,  723 }
};

bool ClassicAQ20IsBossType(uint32 type)
{
    return type < CLASSIC_AQ20_MAX_ENCOUNTER && type != CLASSIC_AQ20_TYPE_GENERAL_ANDOROV;
}

struct ClassicAQ20SpawnLocation
{
    float x, y, z;
};

std::array<ClassicAQ20SpawnLocation, 11> const ClassicAQ20CrystalSpawn =
{{
    { -9407.164062f, 1959.240845f, 85.558998f }, // central spawn, not initially spawned since it was a nerfed mechanic added in 1.11
    { -9357.931641f, 1930.596802f, 85.556198f },
    { -9383.113281f, 2011.042725f, 85.556389f },
    { -9243.36f, 1979.04f, 85.556f },
    { -9281.68f, 1886.66f, 85.5558f },
    { -9241.8f, 1806.39f, 85.5557f },
    { -9366.78f, 1781.76f, 85.5561f },
    { -9297.668945f, 1747.256348f, 85.5566f },
    { -9430.37f, 1786.86f, 85.557f },
    { -9187.087891f, 1940.501099f, 85.5564f },
    { -9406.73f, 1863.13f, 85.5558f }
}};

// VMaNGOS spawns Andorov + his 4 Kaldorei Elites from DB (guids 301311..301315, spawn_flags 2 = not spawned by default)
// with GetMap()->LoadCreatureSpawnWithGroup(ANDOROV_DB_GUID). Those spawns are not in the TC world DB, so they are summoned
// here at the same positions (slot 0 = Andorov, 1..4 = elites; follow distance/angle = VMaNGOS creature_groups).
struct ClassicAQ20SquadSlot
{
    uint32 Entry;
    float X, Y, Z, O;
    float FollowDist, FollowAngle;
};

std::array<ClassicAQ20SquadSlot, 5> const ClassicAQ20AndorovSquad =
{{
    { CLASSIC_AQ20_NPC_GENERAL_ANDOROV, -8538.18f, 1486.1f,  32.3905f, 4.05218f, 0.0f,   0.0f },
    { CLASSIC_AQ20_NPC_KALDOREI_ELITE,  -8540.54f, 1488.2f,  32.4336f, 4.31096f, 4.856f, 3.7f },
    { CLASSIC_AQ20_NPC_KALDOREI_ELITE,  -8538.09f, 1488.29f, 32.4843f, 4.11898f, 4.743f, 3.2f },
    { CLASSIC_AQ20_NPC_KALDOREI_ELITE,  -8536.24f, 1486.57f, 32.4327f, 3.92699f, 4.813f, 2.6f },
    { CLASSIC_AQ20_NPC_KALDOREI_ELITE,  -8535.95f, 1484.21f, 32.3366f, 4.06662f, 4.929f, 2.1f }
}};

// VMaNGOS creature_movement_template 15471 (migration 20240619170801): once Kurinnaxx is dead the squad walks from its spawn to the
// camp. The last point (-8870.72 1648.40) is listed twice there with 1 s waits; the walk ends on it once.
std::array<Position, 12> const ClassicAQ20AndorovCampPath =
{{
    { -8583.434f, 1453.6205f, 31.924183f },
    { -8623.171f, 1470.4077f, 31.90701f },
    { -8638.66f,  1486.67f,   32.8086f },
    { -8652.15f,  1500.83f,   32.6656f },
    { -8668.32f,  1520.76f,   32.0071f },
    { -8694.01f,  1552.81f,   32.0866f },
    { -8710.79f,  1571.02f,   26.5108f },
    { -8737.69f,  1596.18f,   21.5802f },
    { -8771.53f,  1614.14f,   21.5793f },
    { -8809.16f,  1622.42f,   20.1552f },
    { -8845.42f,  1633.81f,   19.7999f },
    { -8870.72f,  1648.4f,    21.5113f, 5.63741f }
}};

// VMaNGOS creature_movement_special 15471 (migration 20221127232342): the walk from the camp to the valley when the event starts
std::array<Position, 4> const ClassicAQ20AndorovValleyPath =
{{
    { -8881.382813f, 1611.213135f, 21.441124f },
    { -8894.543945f, 1589.428223f, 21.454288f },
    { -8916.958008f, 1557.906372f, 21.431034f },
    { -8939.951172f, 1551.126953f, 21.566868f, 2.877214f }
}};

// Squad positions at the camp (creature_movement_scripts 1547101, generic_scripts 153912) and in the valley (creature_movement_scripts
// 1547103); slot 0 = Andorov, 1..4 = the elites in VMaNGOS guid order 301312..301315
std::array<Position, 5> const ClassicAQ20SquadCampPos =
{{
    { -8870.721f, 1648.401f, 21.51133f, 5.637414f },
    { -8868.31f,  1651.6f,   21.5113f,  5.45f },
    { -8869.52f,  1650.0f,   21.5113f,  5.48f },
    { -8871.92f,  1646.8f,   21.5113f,  5.78f },
    { -8873.13f,  1645.21f,  21.5113f,  5.44f }
}};

std::array<Position, 5> const ClassicAQ20SquadValleyPos =
{{
    { -8939.951172f, 1551.126953f, 21.566868f, 2.877214f },
    { -8937.8f,  1556.23f, 21.4079f, 2.97824f },
    { -8939.17f, 1554.02f, 21.5905f, 2.89695f },
    { -8940.74f, 1548.23f, 21.6848f, 2.6754f },
    { -8941.52f, 1545.34f, 21.8723f, 2.96206f }
}};

// generic_scripts 1547105: Andorov leaves once Rajaxx is dead; each move has its VMaNGOS delay and travel time (ms)
struct ClassicAQ20TimedMove
{
    uint32 At;
    uint32 Travel;
    Position Pos;
};

std::array<ClassicAQ20TimedMove, 4> const ClassicAQ20AndorovLeavePath =
{{
    {  3000, 3967, { -8969.76f, 1547.85f, 22.0191f } },
    {  7000, 6114, { -8941.51f, 1552.21f, 21.7021f } },
    { 12000, 4472, { -8917.92f, 1563.85f, 21.4079f } },
    { 15000, 6462, { -8888.59f, 1593.67f, 21.4053f } }
}};

// generic_scripts 153911 .. 153851 and 153411: the waves in order. Each wave is the creature_formations group (VMaNGOS creature_groups)
// led by that captain; SuccessText / SuccessSound is what General Rajaxx does once the wave is dead (after the last one he comes himself).
struct ClassicAQ20Wave
{
    uint32 LeaderEntry;
    uint32 SuccessText;
    uint32 SuccessSound;
};

std::array<ClassicAQ20Wave, 7> const ClassicAQ20Waves =
{{
    { CLASSIC_AQ20_NPC_CAPTAIN_QEEZ,   0,                              CLASSIC_AQ20_SOUND_RAJAXX_WAVE_1 },
    { CLASSIC_AQ20_NPC_CAPTAIN_TUUBID, CLASSIC_AQ20_SAY_RAJAXX_WAVE_2, CLASSIC_AQ20_SOUND_RAJAXX_WAVE_2 },
    { CLASSIC_AQ20_NPC_CAPTAIN_DRENN,  CLASSIC_AQ20_SAY_RAJAXX_WAVE_3, CLASSIC_AQ20_SOUND_RAJAXX_WAVE_3 },
    { CLASSIC_AQ20_NPC_CAPTAIN_XURREM, CLASSIC_AQ20_SAY_RAJAXX_WAVE_4, CLASSIC_AQ20_SOUND_RAJAXX_WAVE_4 },
    { CLASSIC_AQ20_NPC_MAJOR_YEGGETH,  CLASSIC_AQ20_SAY_RAJAXX_WAVE_5, CLASSIC_AQ20_SOUND_RAJAXX_WAVE_5 },
    { CLASSIC_AQ20_NPC_MAJOR_PAKKON,   CLASSIC_AQ20_SAY_RAJAXX_WAVE_6, CLASSIC_AQ20_SOUND_RAJAXX_WAVE_6 },
    { CLASSIC_AQ20_NPC_COLONEL_ZERRAN, CLASSIC_AQ20_SAY_RAJAXX_ENGAGE, CLASSIC_AQ20_SOUND_RAJAXX_ENGAGE }
}};

enum ClassicAQ20InstanceMisc : uint32
{
    CLASSIC_AQ20_GO_AQ_DOOR              = 176149,

    // VMaNGOS creature_linking_template (migration 20240407052628): the captains aggro with Rajaxx within this range
    CLASSIC_AQ20_CAPTAIN_LINK_RANGE      = 150,
    CLASSIC_AQ20_FACTION_CENARION_CIRCLE = 609,

    // generic_scripts 154710 texts
    CLASSIC_AQ20_SAY_ANDOROV_START_1     = 11028,
    CLASSIC_AQ20_SAY_ANDOROV_START_2     = 11477,

    // VMaNGOS pylon guid kept spawned by OnObjectCreate (30000000 + 399461)
    CLASSIC_AQ20_PYLON_KEPT_SPAWN_ID     = 30399461
};

bool ClassicAQ20IsRajaxxWaveEntry(uint32 entry)
{
    switch (entry)
    {
        case CLASSIC_AQ20_NPC_CAPTAIN_QEEZ:
        case CLASSIC_AQ20_NPC_CAPTAIN_TUUBID:
        case CLASSIC_AQ20_NPC_CAPTAIN_DRENN:
        case CLASSIC_AQ20_NPC_CAPTAIN_XURREM:
        case CLASSIC_AQ20_NPC_MAJOR_YEGGETH:
        case CLASSIC_AQ20_NPC_MAJOR_PAKKON:
        case CLASSIC_AQ20_NPC_COLONEL_ZERRAN:
        case CLASSIC_AQ20_NPC_SWARMGUARD_NEEDLER:
        case CLASSIC_AQ20_NPC_QIRAJI_WARRIOR:
            return true;
        default:
            return false;
    }
}

// VMaNGOS CreatureGroup::GetOriginalLeaderGuid().GetEntry()
uint32 ClassicAQ20GetFormationLeaderEntry(Creature* creature)
{
    if (CreatureGroup* group = creature->GetFormation())
        if (CreatureData const* data = sObjectMgr->GetCreatureData(group->GetLeaderSpawnId()))
            return data->id;
    return 0;
}
}

class classic_instance_ruins_of_ahnqiraj : public InstanceMapScript
{
public:
    classic_instance_ruins_of_ahnqiraj() : InstanceMapScript(ClassicRuinsOfAhnQirajScriptName, CLASSIC_AQ20_MAP_ID) { }

    struct classic_instance_ruins_of_ahnqiraj_InstanceScript : public InstanceScript
    {
        explicit classic_instance_ruins_of_ahnqiraj_InstanceScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(ClassicRuinsOfAhnQirajDataHeader);
            SetBossNumber(CLASSIC_AQ20_MAX_ENCOUNTER);

            std::vector<DungeonEncounterData> encounters;
            for (ClassicAQ20EncounterId const& enc : ClassicAQ20EncounterIds)
                if (sDungeonEncounterStore.LookupEntry(enc.DungeonEncounterId))
                    encounters.push_back({ enc.BossId, {{ enc.DungeonEncounterId }} });
            LoadDungeonEncounterData(encounters);

            m_uiAndorovState = NOT_STARTED;
            m_uiGladiatorDeath = 0;
            m_uiRajaxxEventResetTimer = 2000;
            m_bRajaxxEventIsToReset = false;
            m_squadRespawnTimer.fill(0);
            m_andorovStep = AndorovStep::None;
            m_andorovStepTime = 0;
            m_andorovStepStage = 0;
            m_squadAtCamp = false;
            m_waveIndex = 0;
            m_waveTimeLeft = 0;
            m_waveCheckTimer = 0;
        }

        ObjectGuid m_uiKurinnaxxGUID, m_uiBuruGUID, m_uiAyamissGUID, m_uiMoamGUID, m_uiOssirianGUID, m_uiAndorovGUID;
        GuidList m_lKaldoreiElites;
        GuidList m_lYeggethShieldList;
        GuidList m_lOssirianPylons;
        std::unordered_map<ObjectGuid, uint32> crystalIndexes;
        GuidList crystalGuids;
        std::deque<uint32> crystalIndexHistory;

        ObjectGuid m_uiQeezGUID, m_uiTuubidGUID, m_uiDrennGUID, m_uiXurremGUID, m_uiYeggethGUID, m_uiPakkonGUID, m_uiZerranGUID, m_uiRajaxxGUID;

        uint32 m_uiAndorovState;   // VMaNGOS m_auiEncounter[TYPE_GENERAL_ANDOROV] (not a boss in TC; not saved, VMaNGOS resets it on load)
        uint32 m_uiGladiatorDeath;

        uint32 m_uiRajaxxEventResetTimer;
        bool m_bRajaxxEventIsToReset;

        ObjectGuid m_doorGuid;

        // Replaces VMaNGOS InstanceData::OnCreatureEnterCombat / OnCreatureEvade (no TC hooks): polled in Update()
        struct CombatWatch
        {
            bool Engaged = false;
            bool Evading = false;
        };
        std::unordered_map<ObjectGuid, CombatWatch> m_combatWatch;

        // Andorov squad (summoned, see ClassicAQ20AndorovSquad)
        std::array<ObjectGuid, 5> m_squadGuids;
        std::array<uint32, 5> m_squadRespawnTimer;   // ms, 0 = no pending respawn
        bool m_squadAtCamp;                           // once the squad walked to the camp it (re)spawns there (VMaNGOS home positions)

        // The Andorov event timeline, driven from Update(): VMaNGOS generic_scripts 154710, creature_movement_scripts 1547102 / 1547103,
        // the scripted map events of generic_scripts 1539x1, 153912 (failure), 153410 / 153411 (Rajaxx), creature_ai_events 1547104 /
        // 1547105 and generic_scripts 1547105 (Andorov leaves)
        enum class AndorovStep : uint8
        {
            None,
            WalkToCamp,     // creature_movement_template 15471
            Start,          // generic_scripts 154710
            WalkToValley,   // creature_movement_special 15471
            Valley,         // creature_movement_scripts 1547102 + 1547103
            Waves,          // the seven scripted map events
            Rajaxx,         // generic_scripts 153410
            Done,           // creature_ai_events 1547104 / 1547105: out-of-combat timers once Rajaxx is dead
            Leave           // generic_scripts 1547105
        };
        AndorovStep m_andorovStep;
        uint32 m_andorovStepTime;    // ms since the step began (Done: out-of-combat ms)
        uint32 m_andorovStepStage;   // delayed commands of the step already run

        // The wave groups, built from the spawn data and creature_formations when the event starts
        struct WaveSpawn
        {
            ObjectGuid::LowType SpawnId;
            uint8 Wave;
        };
        std::vector<WaveSpawn> m_waveSpawns;
        uint8 m_waveIndex;
        uint32 m_waveTimeLeft;       // ms left of the scripted map event's time limit
        uint32 m_waveCheckTimer;     // VMaNGOS checks the event's conditions every second

        bool IsEncounterInProgress() const override
        {
            if (InstanceScript::IsEncounterInProgress())
                return true;
            return m_uiAndorovState == IN_PROGRESS || m_uiAndorovState == SPECIAL;
        }

        ObjectGuid GetGuidData(uint32 uiData) const override
        {
            switch (uiData)
            {
                case CLASSIC_AQ20_DATA_OSSIRIAN:  return m_uiOssirianGUID;
                case CLASSIC_AQ20_DATA_BURU:      return m_uiBuruGUID;
                case CLASSIC_AQ20_DATA_AYAMISS:   return m_uiAyamissGUID;
                case CLASSIC_AQ20_DATA_MOAM:      return m_uiMoamGUID;
                case CLASSIC_AQ20_DATA_ANDOROV:   return m_uiAndorovGUID;
                case CLASSIC_AQ20_DATA_KURINNAXX: return m_uiKurinnaxxGUID;
                case CLASSIC_AQ20_DATA_QEEZ:      return m_uiQeezGUID;
                case CLASSIC_AQ20_DATA_TUUBID:    return m_uiTuubidGUID;
                case CLASSIC_AQ20_DATA_DRENN:     return m_uiDrennGUID;
                case CLASSIC_AQ20_DATA_XURREM:    return m_uiXurremGUID;
                case CLASSIC_AQ20_DATA_YEGGETH:   return m_uiYeggethGUID;
                case CLASSIC_AQ20_DATA_PAKKON:    return m_uiPakkonGUID;
                case CLASSIC_AQ20_DATA_ZERRAN:    return m_uiZerranGUID;
                case CLASSIC_AQ20_DATA_RAJAXX:    return m_uiRajaxxGUID;
                case CLASSIC_AQ20_DATA_YEGGETH_SHIELD:
                {
                    // VMaNGOS picks a random member of the Yeggeth group list (its loop is broken; intent kept)
                    if (m_lYeggethShieldList.empty())
                        return ObjectGuid::Empty;
                    auto itr = m_lYeggethShieldList.begin();
                    std::advance(itr, urand(0, uint32(m_lYeggethShieldList.size()) - 1));
                    return *itr;
                }
                default:
                    break;
            }
            return ObjectGuid::Empty;
        }

        void SetGuidData(uint32 type, ObjectGuid data) override
        {
            if (type == CLASSIC_AQ20_DATA_CRYSTAL)
                SpawnNewCrystals(data);
        }

        // VMaNGOS OnCreatureEnterCombat
        void OnCreatureEnterCombat(Creature* pCreature)
        {
            // creature_linking_template FLAG_AGGRO_ON_AGGRO: the captains join when Rajaxx is pulled past them
            if (pCreature->GetEntry() == CLASSIC_AQ20_NPC_RAJAXX)
            {
                Unit* pEnemy = pCreature->GetVictim();
                if (!pEnemy)
                    pEnemy = pCreature->GetThreatManager().GetCurrentVictim();
                if (!pEnemy)
                    return;
                for (uint32 i = CLASSIC_AQ20_DATA_QEEZ; i <= CLASSIC_AQ20_DATA_ZERRAN; ++i)
                    if (Creature* pCaptain = instance->GetCreature(GetGuidData(i)))
                        if (pCaptain->IsAlive() && !pCaptain->IsInCombat() && pCaptain->IsAIEnabled() &&
                            pCaptain->IsWithinDist(pCreature, float(CLASSIC_AQ20_CAPTAIN_LINK_RANGE)))
                            pCaptain->AI()->AttackStart(pEnemy);
                return;
            }

            if (!ClassicAQ20IsRajaxxWaveEntry(pCreature->GetEntry()))
                return;

            /** Create Yeggeth list for Rajaxx Shield ability */
            if (ClassicAQ20GetFormationLeaderEntry(pCreature) == CLASSIC_AQ20_NPC_MAJOR_YEGGETH)
                m_lYeggethShieldList.push_back(pCreature->GetGUID());

            // Fight Change in 1.10.1
            // https://wowpedia.fandom.com/wiki/General_Rajaxx?oldid=180046
            // "as soon as you pull the liutenant runs in with his group and starts to fight"
            if (GetBossState(CLASSIC_AQ20_TYPE_RAJAXX) != DONE &&
                (m_uiAndorovState == NOT_STARTED || m_uiAndorovState == FAIL))
            {
                if (Creature* pAndorov = instance->GetCreature(m_uiAndorovGUID))
                {
                    if (!pAndorov->IsAlive())
                        RespawnSquadMember(0, pAndorov);
                    // generic_scripts 154710 (see SetData)
                    SetData(CLASSIC_AQ20_TYPE_GENERAL_ANDOROV, IN_PROGRESS);
                }
            }
            m_bRajaxxEventIsToReset = false;
        }

        // VMaNGOS OnCreatureEvade
        void OnCreatureEvade(Creature* pCreature)
        {
            if (pCreature->GetEntry() == CLASSIC_AQ20_NPC_RAJAXX)
            {
                // TC keeps the encounter (and the instance entry block) in progress until the boss state is reset
                if (GetBossState(CLASSIC_AQ20_TYPE_RAJAXX) == IN_PROGRESS)
                    SetData(CLASSIC_AQ20_TYPE_RAJAXX, NOT_STARTED);
                return;
            }

            if (ClassicAQ20IsRajaxxWaveEntry(pCreature->GetEntry()))
            {
                // If any creature from Rajaxx's wave is on evade mode, reset Rajaxx.
                m_uiRajaxxEventResetTimer = 2000;
                m_bRajaxxEventIsToReset = true;
                return;
            }

            if (pCreature->GetEntry() == CLASSIC_AQ20_NPC_KALDOREI_ELITE)
            {
                if (Creature* pAndorov = instance->GetCreature(m_uiAndorovGUID))
                {
                    if (pAndorov->IsInCombat())
                    {
                        // VMaNGOS pCreature->AddThreatsOf(pAndorov)
                        // TODO(classic): TC may ignore threat added while the elite is still in evade mode.
                        for (ThreatReference const* ref : pAndorov->GetThreatManager().GetUnsortedThreatList())
                            pCreature->GetThreatManager().AddThreat(ref->GetVictim(), ref->GetThreat(), nullptr, true, true);
                    }
                }
            }
        }

        void OnCreatureCreate(Creature* pCreature) override
        {
            InstanceScript::OnCreatureCreate(pCreature);

            switch (pCreature->GetEntry())
            {
                case CLASSIC_AQ20_NPC_KURINNAXX:       m_uiKurinnaxxGUID = pCreature->GetGUID(); break;
                case CLASSIC_AQ20_NPC_CAPTAIN_QEEZ:    m_uiQeezGUID = pCreature->GetGUID(); break;
                case CLASSIC_AQ20_NPC_CAPTAIN_TUUBID:  m_uiTuubidGUID = pCreature->GetGUID(); break;
                case CLASSIC_AQ20_NPC_CAPTAIN_DRENN:   m_uiDrennGUID = pCreature->GetGUID(); break;
                case CLASSIC_AQ20_NPC_CAPTAIN_XURREM:  m_uiXurremGUID = pCreature->GetGUID(); break;
                case CLASSIC_AQ20_NPC_MAJOR_YEGGETH:   m_uiYeggethGUID = pCreature->GetGUID(); break;
                case CLASSIC_AQ20_NPC_MAJOR_PAKKON:    m_uiPakkonGUID = pCreature->GetGUID(); break;
                case CLASSIC_AQ20_NPC_COLONEL_ZERRAN:  m_uiZerranGUID = pCreature->GetGUID(); break;
                case CLASSIC_AQ20_NPC_RAJAXX:
                    m_uiRajaxxGUID = pCreature->GetGUID();
                    m_combatWatch[pCreature->GetGUID()] = CombatWatch();
                    break;
                case CLASSIC_AQ20_NPC_BURU:            m_uiBuruGUID = pCreature->GetGUID(); break;
                case CLASSIC_AQ20_NPC_AYAMISS:         m_uiAyamissGUID = pCreature->GetGUID(); break;
                case CLASSIC_AQ20_NPC_MOAM:            m_uiMoamGUID = pCreature->GetGUID(); break;
                case CLASSIC_AQ20_NPC_OSSIRIAN:        m_uiOssirianGUID = pCreature->GetGUID(); break;
                case CLASSIC_AQ20_NPC_GENERAL_ANDOROV:
                    pCreature->SetImmuneToNPC(true);
                    m_uiAndorovGUID = pCreature->GetGUID();
                    break;
                case CLASSIC_AQ20_NPC_KALDOREI_ELITE:
                    pCreature->SetImmuneToNPC(true);
                    m_lKaldoreiElites.push_back(pCreature->GetGUID());
                    m_combatWatch[pCreature->GetGUID()] = CombatWatch();
                    break;
                default:
                    break;
            }

            if (ClassicAQ20IsRajaxxWaveEntry(pCreature->GetEntry()))
                m_combatWatch[pCreature->GetGUID()] = CombatWatch();
        }

        void OnGameObjectCreate(GameObject* pGo) override
        {
            InstanceScript::OnGameObjectCreate(pGo);

            switch (pGo->GetEntry())
            {
                case CLASSIC_AQ20_GO_OSSIRIAN_CRYSTAL:
                    // VMaNGOS hides the DB-spawned pylons (except one); crystals summoned by SpawnNewCrystals have no spawn id.
                    // TODO(classic): the TC world has no DB spawns of 180619 on map 509; kept for parity.
                    if (!pGo->GetSpawnId())
                        break;
                    m_lOssirianPylons.push_back(pGo->GetGUID());
                    if (pGo->GetSpawnId() != CLASSIC_AQ20_PYLON_KEPT_SPAWN_ID)
                        pGo->DespawnOrUnsummon(0s, Seconds(7 * DAY));
                    break;
                default:
                    break;
            }
        }

        // VMaNGOS OnCreatureDeath
        void OnUnitDeath(Unit* unit) override
        {
            Creature* pCreature = unit->ToCreature();
            if (!pCreature)
                return;

            switch (pCreature->GetEntry())
            {
                case CLASSIC_AQ20_NPC_RAJAXX:
                    GiveRepAfterRajaxxDeath(pCreature);
                    // Rajaxx is DB-scripted in VMaNGOS (no C++ script sets TYPE_RAJAXX); safety net so the Andorov/door
                    // logic sees the encounter as done.
                    SetData(CLASSIC_AQ20_TYPE_RAJAXX, DONE);
                    break;
                case CLASSIC_AQ20_NPC_SWARMGUARD_NEEDLER:
                case CLASSIC_AQ20_NPC_QIRAJI_WARRIOR:
                    pCreature->DespawnOrUnsummon(3s, Seconds(CLASSIC_AQ20_RESPAWN_FOUR_DAYS));
                    break;
                case CLASSIC_AQ20_NPC_GENERAL_ANDOROV:
                    pCreature->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                    [[fallthrough]];
                case CLASSIC_AQ20_NPC_KALDOREI_ELITE:
                    // No respawn before Kurinaxx's death or during Rajaxx event.
                    if (GetBossState(CLASSIC_AQ20_TYPE_KURINNAXX) != DONE || GetBossState(CLASSIC_AQ20_TYPE_RAJAXX) == IN_PROGRESS)
                        SetSquadMemberRespawnTime(pCreature, CLASSIC_AQ20_RESPAWN_FOUR_DAYS);
                    else
                        SetSquadMemberRespawnTime(pCreature, CLASSIC_AQ20_RESPAWN_15_MINUTES);
                    break;
                default:
                    break;
            }
        }

        uint32 GetData(uint32 uiType) const override
        {
            switch (uiType)
            {
                case CLASSIC_AQ20_TYPE_QIRAJI_GLADIATOR:
                    return m_uiGladiatorDeath;
                case CLASSIC_AQ20_TYPE_GENERAL_ANDOROV:
                    return m_uiAndorovState;
                case CLASSIC_AQ20_TYPE_KURINNAXX:
                case CLASSIC_AQ20_TYPE_RAJAXX:
                case CLASSIC_AQ20_TYPE_BURU:
                case CLASSIC_AQ20_TYPE_MOAM:
                case CLASSIC_AQ20_TYPE_AYAMISS:
                case CLASSIC_AQ20_TYPE_OSSIRIAN:
                    return GetBossState(uiType);
                default:
                    return 0;
            }
        }

        void SetBoss(uint32 uiType, uint32 uiData)
        {
            // TC blocks DONE -> other state transitions
            if (GetBossState(uiType) == DONE && uiData != DONE)
                return;
            SetBossState(uiType, EncounterState(uiData));
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            switch (uiType)
            {
                case CLASSIC_AQ20_TYPE_QIRAJI_GLADIATOR:
                    if (uiData > 0)
                        m_uiGladiatorDeath = m_uiGladiatorDeath + uiData;
                    else
                        m_uiGladiatorDeath = 0;
                    return;
                case CLASSIC_AQ20_DATA_CRYSTAL_INIT:
                    SpawnNewCrystals(ObjectGuid::Empty);
                    return;
                case CLASSIC_AQ20_TYPE_KURINNAXX:
                    SetBoss(uiType, uiData);
                    break;
                case CLASSIC_AQ20_TYPE_GENERAL_ANDOROV:
                    if (uiData == NOT_STARTED || uiData == FAIL)
                    {
                        if (Creature* pAndorov = instance->GetCreature(m_uiAndorovGUID))
                            pAndorov->SetGossipMenuId(CLASSIC_AQ20_ANDOROV_GOSSIP_NOT_STARTED);
                    }
                    if (uiData == IN_PROGRESS)
                    {
                        if (m_uiAndorovState == IN_PROGRESS)
                            return;   // generic_scripts 154710 already runs
                        SetAndorovSquadImmunity(false);
                        if (Creature* pAndorov = instance->GetCreature(m_uiAndorovGUID))
                        {
                            pAndorov->SetGossipMenuId(CLASSIC_AQ20_ANDOROV_GOSSIP_IN_PROGRESS);
                            pAndorov->RemoveNpcFlag(UNIT_NPC_FLAG_VENDOR);
                        }
                    }
                    m_uiAndorovState = uiData;
                    if (uiData == IN_PROGRESS)
                        StartAndorovEvent();
                    break;
                case CLASSIC_AQ20_DATA_ANDOROV_POINT:
                    OnAndorovPointReached(uiData);
                    return;
                case CLASSIC_AQ20_TYPE_RAJAXX:
                    if (uiData == NOT_STARTED)
                    {
                        /** Respawn Andorov and elite in 15 minutes */
                        if (GetBossState(CLASSIC_AQ20_TYPE_KURINNAXX) == DONE)
                            SetAndorovSquadRespawnTime(CLASSIC_AQ20_RESPAWN_15_MINUTES);
                    }
                    if (uiData == DONE && GetBossState(CLASSIC_AQ20_TYPE_RAJAXX) != DONE)
                    {
                        if (Creature* pAndorov = instance->GetCreature(m_uiAndorovGUID))
                        {
                            // World of Warcraft Client Patch 1.10.0 (2006-03-28)
                            // - Lieutenant General Andorov will now offer supplies if kept alive through the battle.
                            pAndorov->SetNpcFlag(UNIT_NPC_FLAG_VENDOR);
                            pAndorov->SetGossipMenuId(CLASSIC_AQ20_ANDOROV_GOSSIP_DONE);
                            SetSquadMemberRespawnTime(pAndorov, CLASSIC_AQ20_RESPAWN_FOUR_DAYS);
                        }
                        SetAndorovSquadImmunity(true);
                    }
                    m_bRajaxxEventIsToReset = false;
                    SetBoss(uiType, uiData);
                    break;
                case CLASSIC_AQ20_TYPE_OSSIRIAN:
                    if (uiData == FAIL || uiData == DONE)
                    {
                        for (ObjectGuid const& crystalGuid : crystalGuids)
                        {
                            if (GameObject* invoc = instance->GetGameObject(crystalGuid))
                                invoc->Delete();
                        }

                        crystalGuids.clear();
                        crystalIndexes.clear();
                        crystalIndexHistory.clear();
                    }
                    SetBoss(uiType, uiData);
                    break;
                case CLASSIC_AQ20_TYPE_BURU:
                case CLASSIC_AQ20_TYPE_MOAM:
                case CLASSIC_AQ20_TYPE_AYAMISS:
                    SetBoss(uiType, uiData);
                    break;
                default:
                    return;
            }
            // VMaNGOS SaveToDB on DONE: TC saves boss states itself.
        }

        void Update(uint32 uiDiff) override
        {
            UpdateCombatWatch();

            /*
            Fight Change in 1.10.1

            This fight has changed somewhat, first off if someone dies
            they are no longer able to return via release and run back,
            there is a big door that closes and you cannot get past it.

            https://wowwiki-archive.fandom.com/wiki/General_Rajaxx?oldid=121120#Fight_Change_in_1.10.1
            */
            if (IsAnyBossInCombat())
            {
                if (m_doorGuid.IsEmpty())
                {
                    Position const doorPos(-8526.0f, 1507.4f, 49.0f, 4.20662f);
                    if (GameObject* pAQDoor = GameObject::CreateGameObject(CLASSIC_AQ20_GO_AQ_DOOR, instance, doorPos,
                        QuaternionData(0.0f, 0.0f, 0.861534f, -0.5077f), 255, GO_STATE_READY))
                    {
                        pAQDoor->SetSpawnedByDefault(false);
                        if (instance->AddToMap(pAQDoor))
                            m_doorGuid = pAQDoor->GetGUID();
                        else
                            delete pAQDoor;
                    }
                }
            }
            else
            {
                if (!m_doorGuid.IsEmpty())
                {
                    if (GameObject* pAQDoor = instance->GetGameObject(m_doorGuid))
                        pAQDoor->Delete();
                    m_doorGuid.Clear();
                }
            }

            if (GetBossState(CLASSIC_AQ20_TYPE_KURINNAXX) == DONE && GetBossState(CLASSIC_AQ20_TYPE_RAJAXX) != DONE && m_uiAndorovGUID.IsEmpty())
                SummonAndorovSquad();   // VMaNGOS LoadCreatureSpawnWithGroup + MoveWaypoint(0): the squad walks to the camp

            if (m_bRajaxxEventIsToReset)
            {
                if (m_uiRajaxxEventResetTimer < uiDiff)
                {
                    if (Creature* pRajaxx = instance->GetCreature(m_uiRajaxxGUID))
                        if (pRajaxx->IsAIEnabled())
                            pRajaxx->AI()->EnterEvadeMode(EvadeReason::Other);
                    m_bRajaxxEventIsToReset = false;
                }
                else
                    m_uiRajaxxEventResetTimer -= uiDiff;
            }

            for (auto iter = crystalIndexes.begin(); iter != crystalIndexes.end();)
            {
                // Crystal was despawned but still in our index for some reason
                if (!instance->GetGameObject(iter->first))
                    iter = crystalIndexes.erase(iter);
                else
                    ++iter;
            }

            UpdateSquadRespawns(uiDiff);
            UpdateAndorovEvent(uiDiff);
        }

        /* Private methods */

        void UpdateCombatWatch()
        {
            // Callbacks may summon creatures (OnCreatureCreate -> m_combatWatch insert): collect first, then dispatch
            GuidVector enteredCombat;
            GuidVector evaded;
            for (auto itr = m_combatWatch.begin(); itr != m_combatWatch.end();)
            {
                Creature* creature = instance->GetCreature(itr->first);
                if (!creature)
                {
                    itr = m_combatWatch.erase(itr);
                    continue;
                }

                bool const engaged = creature->IsAlive() && creature->IsEngaged();
                bool const evading = creature->IsAlive() && creature->IsInEvadeMode();
                if (engaged && !itr->second.Engaged)
                    enteredCombat.push_back(itr->first);
                if (evading && !itr->second.Evading)
                    evaded.push_back(itr->first);
                itr->second.Engaged = engaged;
                itr->second.Evading = evading;
                ++itr;
            }

            for (ObjectGuid const& guid : enteredCombat)
                if (Creature* creature = instance->GetCreature(guid))
                    OnCreatureEnterCombat(creature);
            for (ObjectGuid const& guid : evaded)
                if (Creature* creature = instance->GetCreature(guid))
                    OnCreatureEvade(creature);
        }

        bool IsAnyBossInCombat()
        {
            if (GetData(CLASSIC_AQ20_TYPE_GENERAL_ANDOROV) == IN_PROGRESS)
                return true;

            for (uint32 i = CLASSIC_AQ20_DATA_KURINNAXX; i <= CLASSIC_AQ20_DATA_ZERRAN; ++i)
            {
                ObjectGuid guid = GetGuidData(i);
                if (guid.IsEmpty())
                    continue;
                if (Creature* pCreature = instance->GetCreature(guid))
                    if (pCreature->IsAlive() && pCreature->GetVictim())
                        return true;
            }
            return false;
        }

        /* Andorov event */

        void SetAndorovStep(AndorovStep step)
        {
            m_andorovStep = step;
            m_andorovStepTime = 0;
            m_andorovStepStage = 0;
        }

        // One delayed command of the current step (VMaNGOS script row delay): true once, when its delay has elapsed
        bool AndorovStepDue(uint32 stage, uint32 atMs)
        {
            if (m_andorovStepStage != stage || m_andorovStepTime < atMs)
                return false;
            ++m_andorovStepStage;
            return true;
        }

        // Waypoints walk; the orientation of a path's last point is its final facing
        void MoveAndorovTo(Creature* pAndorov, uint32 pointId, Position const& pos)
        {
            Optional<float> finalOrient;
            if (pos.GetOrientation() != 0.0f)
                finalOrient = pos.GetOrientation();
            pAndorov->GetMotionMaster()->MovePoint(pointId, pos, true, finalOrient, {}, MovementWalkRunSpeedSelectionMode::ForceWalk);
        }

        // VMaNGOS creature_groups: the elites follow Andorov while he walks
        void FollowAndorov(Creature* pAndorov)
        {
            for (uint32 i = 1; i < m_squadGuids.size(); ++i)
                if (Creature* pElite = instance->GetCreature(m_squadGuids[i]))
                {
                    ClassicAQ20SquadSlot const& slot = ClassicAQ20AndorovSquad[i];
                    if (pElite->IsAlive() && !pElite->IsInCombat())
                        pElite->GetMotionMaster()->MoveFollow(pAndorov, slot.FollowDist, ChaseAngle(slot.FollowAngle));
                }
        }

        // creature_movement_scripts 1547101 / 1547103: Andorov stops where he is, the elites walk to their spots, and those become home
        void PlaceSquad(std::array<Position, 5> const& positions)
        {
            for (uint32 i = 0; i < m_squadGuids.size(); ++i)
            {
                Creature* member = instance->GetCreature(m_squadGuids[i]);
                if (!member || !member->IsAlive())
                    continue;

                member->SetHomePosition(positions[i]);
                if (member->IsInCombat())
                    continue;

                member->GetMotionMaster()->Clear();
                if (i == 0)
                    member->GetMotionMaster()->MoveIdle();
                else
                    member->GetMotionMaster()->MovePoint(0, positions[i], true, positions[i].GetOrientation(), {},
                        MovementWalkRunSpeedSelectionMode::ForceWalk);
            }
        }

        // creature_movement_template 15471: the squad walks from its spawn to the camp
        void StartCampWalk(Creature* pAndorov)
        {
            SetAndorovStep(AndorovStep::WalkToCamp);
            pAndorov->SetGossipMenuId(CLASSIC_AQ20_ANDOROV_GOSSIP_IN_PROGRESS);
            MoveAndorovTo(pAndorov, CLASSIC_AQ20_ANDOROV_POINT_CAMP_PATH, ClassicAQ20AndorovCampPath[0]);
            FollowAndorov(pAndorov);
        }

        // classic_npc_andorov::MovementInform
        void OnAndorovPointReached(uint32 pointId)
        {
            Creature* pAndorov = instance->GetCreature(m_uiAndorovGUID);
            if (!pAndorov || !pAndorov->IsAlive())
                return;

            if (pointId >= CLASSIC_AQ20_ANDOROV_POINT_CAMP_PATH && pointId < CLASSIC_AQ20_ANDOROV_POINT_VALLEY_PATH)
            {
                if (m_andorovStep != AndorovStep::WalkToCamp)
                    return;
                uint32 const next = pointId - CLASSIC_AQ20_ANDOROV_POINT_CAMP_PATH + 1;
                if (next < ClassicAQ20AndorovCampPath.size())
                {
                    MoveAndorovTo(pAndorov, CLASSIC_AQ20_ANDOROV_POINT_CAMP_PATH + next, ClassicAQ20AndorovCampPath[next]);
                    return;
                }
                // creature_movement_scripts 1547101: the squad waits at the camp for the raid
                m_andorovStep = AndorovStep::None;
                m_squadAtCamp = true;
                PlaceSquad(ClassicAQ20SquadCampPos);
                pAndorov->SetGossipMenuId(CLASSIC_AQ20_ANDOROV_GOSSIP_NOT_STARTED);
                return;
            }

            if (pointId >= CLASSIC_AQ20_ANDOROV_POINT_VALLEY_PATH && pointId < CLASSIC_AQ20_ANDOROV_POINT_LEAVE_PATH)
            {
                if (m_andorovStep != AndorovStep::Start && m_andorovStep != AndorovStep::WalkToValley)
                    return;
                uint32 const next = pointId - CLASSIC_AQ20_ANDOROV_POINT_VALLEY_PATH + 1;
                if (next < ClassicAQ20AndorovValleyPath.size())
                {
                    MoveAndorovTo(pAndorov, CLASSIC_AQ20_ANDOROV_POINT_VALLEY_PATH + next, ClassicAQ20AndorovValleyPath[next]);
                    return;
                }
                // creature_movement_special 15471 point 4: creature_movement_scripts 1547102 then 1547103
                SetAndorovStep(AndorovStep::Valley);
            }
        }

        // generic_scripts 154710 (gossip 6629, or a wave pulled first): the whole event starts here
        void StartAndorovEvent()
        {
            Creature* pAndorov = instance->GetCreature(m_uiAndorovGUID);
            if (!pAndorov || !pAndorov->IsAlive())
                return;

            BuildWaveSpawns();
            m_bRajaxxEventIsToReset = false;
            SetAndorovStep(AndorovStep::Start);
            pAndorov->HandleEmoteCommand(EMOTE_ONESHOT_SHOUT);
            ClassicScriptText(CLASSIC_AQ20_SAY_ANDOROV_START_1, pAndorov);
        }

        void UpdateAndorovEvent(uint32 uiDiff)
        {
            if (m_andorovStep == AndorovStep::None)
                return;

            Creature* pAndorov = instance->GetCreature(m_uiAndorovGUID);
            bool const andorovAlive = pAndorov && pAndorov->IsAlive();

            switch (m_andorovStep)
            {
                case AndorovStep::WalkToCamp:
                case AndorovStep::WalkToValley:
                    // Continued by OnAndorovPointReached; without Andorov the walk (and the event, as in VMaNGOS) stops here
                    if (!andorovAlive)
                        m_andorovStep = AndorovStep::None;
                    return;
                case AndorovStep::Start:
                case AndorovStep::Valley:
                    if (!andorovAlive)
                    {
                        m_andorovStep = AndorovStep::None;
                        return;
                    }
                    break;
                case AndorovStep::Done:
                    // creature_ai_events 1547104 / 1547105 are out-of-combat timers of phase 2 with condition 3704 (Rajaxx is dead)
                    if (!andorovAlive || pAndorov->IsInCombat() || GetBossState(CLASSIC_AQ20_TYPE_RAJAXX) != DONE)
                        return;
                    break;
                default:
                    break;
            }

            m_andorovStepTime += uiDiff;

            switch (m_andorovStep)
            {
                case AndorovStep::Start:
                    if (AndorovStepDue(0, 2000))
                    {
                        MoveAndorovTo(pAndorov, CLASSIC_AQ20_ANDOROV_POINT_VALLEY_PATH, ClassicAQ20AndorovValleyPath[0]);
                        FollowAndorov(pAndorov);
                    }
                    if (AndorovStepDue(1, 3000))
                    {
                        ClassicScriptText(CLASSIC_AQ20_SAY_ANDOROV_START_2, pAndorov);
                        m_andorovStep = AndorovStep::WalkToValley;
                    }
                    break;
                case AndorovStep::Valley:
                    // 1547102 (point 4, 5 s wait) then 1547103 (point 5): the squad takes position and the first wave is called
                    if (AndorovStepDue(0, 2000))
                    {
                        ClassicScriptText(CLASSIC_AQ20_SAY_ANDOROV_VALLEY, pAndorov);
                        pAndorov->HandleEmoteCommand(EMOTE_ONESHOT_SHOUT);
                    }
                    if (AndorovStepDue(1, 5000))
                    {
                        pAndorov->HandleEmoteCommand(EMOTE_ONESHOT_POINT);
                        PlaceSquad(ClassicAQ20SquadValleyPos);
                    }
                    if (AndorovStepDue(2, 11000))
                    {
                        pAndorov->HandleEmoteCommand(EMOTE_ONESHOT_READY1H);
                        ClassicScriptText(CLASSIC_AQ20_SAY_ANDOROV_READY, pAndorov);
                    }
                    if (AndorovStepDue(3, 13000))
                        pAndorov->SetEmoteState(EMOTE_STATE_READY1H);
                    if (AndorovStepDue(4, 17000))
                        StartWave(0);
                    break;
                case AndorovStep::Waves:
                    UpdateWave(uiDiff);
                    break;
                case AndorovStep::Rajaxx:
                    // generic_scripts 153410: one second after his yell Rajaxx enters combat with the zone
                    if (AndorovStepDue(0, 1000))
                    {
                        if (Creature* pRajaxx = instance->GetCreature(m_uiRajaxxGUID))
                            if (pRajaxx->IsAlive() && pRajaxx->IsAIEnabled())
                                CreatureAI::DoZoneInCombat(pRajaxx);
                        SetAndorovStep(AndorovStep::Done);
                    }
                    break;
                case AndorovStep::Done:
                    if (AndorovStepDue(0, 5000))
                        pAndorov->SetImmuneToNPC(true);
                    if (AndorovStepDue(1, 130000))
                    {
                        // generic_scripts 1547105
                        SetAndorovStep(AndorovStep::Leave);
                        pAndorov->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                        ClassicScriptText(CLASSIC_AQ20_SAY_ANDOROV_LEAVE, pAndorov);
                    }
                    break;
                case AndorovStep::Leave:
                    for (uint32 i = 0; i < ClassicAQ20AndorovLeavePath.size(); ++i)
                    {
                        if (!AndorovStepDue(i, ClassicAQ20AndorovLeavePath[i].At))
                            continue;
                        if (!andorovAlive)
                            continue;
                        ClassicAQ20TimedMove const& move = ClassicAQ20AndorovLeavePath[i];
                        // VMaNGOS gives each move its travel time; the matching speed is yards per second
                        Optional<float> speed;
                        float const dist = pAndorov->GetExactDist(move.Pos);
                        if (dist > 1.0f)
                            speed = dist / (float(move.Travel) / float(IN_MILLISECONDS));
                        pAndorov->GetMotionMaster()->MovePoint(CLASSIC_AQ20_ANDOROV_POINT_LEAVE_PATH + i, move.Pos, true, {}, speed);
                    }
                    if (AndorovStepDue(uint32(ClassicAQ20AndorovLeavePath.size()), 20000))
                    {
                        // generic_scripts 154712 on the group: Andorov and the elites despawn for four days
                        for (uint32 i = 0; i < m_squadGuids.size(); ++i)
                        {
                            if (Creature* member = instance->GetCreature(m_squadGuids[i]))
                            {
                                SetSquadMemberRespawnTime(member, CLASSIC_AQ20_RESPAWN_FOUR_DAYS);
                                member->DespawnOrUnsummon(0s, Seconds(CLASSIC_AQ20_RESPAWN_FOUR_DAYS));
                            }
                        }
                        m_andorovStep = AndorovStep::None;
                    }
                    break;
                default:
                    break;
            }
        }

        /* Rajaxx waves */

        // The waves are the creature_formations groups (VMaNGOS creature_groups) led by the captains: every spawn of a wave entry on
        // this map whose formation leader is one of them. Their grids are loaded so the whole wave exists when it is called.
        void BuildWaveSpawns()
        {
            m_waveSpawns.clear();
            for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
            {
                if (data.mapId != CLASSIC_AQ20_MAP_ID || !ClassicAQ20IsRajaxxWaveEntry(data.id))
                    continue;

                uint32 leaderEntry = data.id;
                if (FormationInfo const* formation = sFormationMgr->GetFormationInfo(spawnId))
                    if (CreatureData const* leader = sObjectMgr->GetCreatureData(formation->LeaderSpawnId))
                        leaderEntry = leader->id;

                for (uint8 wave = 0; wave < ClassicAQ20Waves.size(); ++wave)
                {
                    if (ClassicAQ20Waves[wave].LeaderEntry != leaderEntry)
                        continue;
                    m_waveSpawns.push_back({ spawnId, wave });
                    instance->LoadGrid(data.spawnPoint.GetPositionX(), data.spawnPoint.GetPositionY());
                    break;
                }
            }

            if (m_waveSpawns.empty())
                TC_LOG_ERROR("scripts", "classic_instance_ruins_of_ahnqiraj: no creature_formations group led by a Rajaxx captain on map {}",
                    uint32(CLASSIC_AQ20_MAP_ID));
        }

        Creature* GetWaveCreature(ObjectGuid::LowType spawnId) const
        {
            auto const& store = instance->GetCreatureBySpawnIdStore();
            auto itr = store.find(spawnId);
            return itr != store.end() ? itr->second : nullptr;
        }

        // generic_scripts 1539x0 / 1539x1: the group enters combat with the zone and becomes the scripted map event's targets
        void StartWave(uint8 wave)
        {
            m_waveIndex = wave;
            m_waveTimeLeft = CLASSIC_AQ20_WAVE_TIME_LIMIT_MS;
            m_waveCheckTimer = IN_MILLISECONDS;
            m_bRajaxxEventIsToReset = false;
            SetAndorovStep(AndorovStep::Waves);

            for (WaveSpawn const& spawn : m_waveSpawns)
            {
                if (spawn.Wave != wave)
                    continue;
                if (Creature* member = GetWaveCreature(spawn.SpawnId))
                    if (member->IsAlive() && member->IsAIEnabled())
                        CreatureAI::DoZoneInCombat(member);
            }
        }

        // The map event's conditions, checked every second: every target dead is the success, a target alive and out of combat
        // (condition 1100) or the time limit is the failure
        void UpdateWave(uint32 uiDiff)
        {
            m_waveTimeLeft = m_waveTimeLeft > uiDiff ? m_waveTimeLeft - uiDiff : 0;
            if (m_waveCheckTimer > uiDiff)
            {
                m_waveCheckTimer -= uiDiff;
                return;
            }
            m_waveCheckTimer = IN_MILLISECONDS;

            bool allDead = true;
            for (WaveSpawn const& spawn : m_waveSpawns)
            {
                if (spawn.Wave != m_waveIndex)
                    continue;
                Creature* member = GetWaveCreature(spawn.SpawnId);
                if (!member || !member->IsAlive())
                    continue;
                allDead = false;
                if (!member->IsInCombat())
                {
                    FailAndorovEvent();
                    return;
                }
            }

            if (allDead)
                OnWaveSuccess();
            else if (!m_waveTimeLeft)
                FailAndorovEvent();
        }

        // generic_scripts 153921 .. 153851 (Rajaxx comments the dead wave, the next one comes) and 153411 (the last wave is dead)
        void OnWaveSuccess()
        {
            ClassicAQ20Wave const& wave = ClassicAQ20Waves[m_waveIndex];
            if (Creature* pRajaxx = instance->GetCreature(m_uiRajaxxGUID))
            {
                if (pRajaxx->IsAlive())
                {
                    if (wave.SuccessText)
                        ClassicScriptText(wave.SuccessText, pRajaxx);
                    // The first wave's sound is played to the whole map (flag 4); the yells carry their broadcast_text sound
                    for (MapReference const& ref : instance->GetPlayers())
                        if (Player* player = ref.GetSource())
                            pRajaxx->PlayDirectSound(wave.SuccessSound, player);
                }
            }

            if (uint32(m_waveIndex) + 1 < ClassicAQ20Waves.size())
            {
                StartWave(m_waveIndex + 1);
                return;
            }

            // 153411: Andorov's part is done, Rajaxx comes himself (153410, continued in UpdateAndorovEvent)
            SetData(CLASSIC_AQ20_TYPE_GENERAL_ANDOROV, DONE);
            SetAndorovStep(AndorovStep::Rajaxx);
        }

        // generic_scripts 153912: a wave survived its time limit or dropped out of combat
        void FailAndorovEvent()
        {
            SetData(CLASSIC_AQ20_TYPE_GENERAL_ANDOROV, FAIL);
            m_andorovStep = AndorovStep::None;

            // Respawn Captain Qeez .. Colonel Zerran with their groups
            for (WaveSpawn const& spawn : m_waveSpawns)
                ResetWaveSpawn(spawn.SpawnId);

            // 154711: Andorov and the elites despawn and come back at the camp in 15 minutes
            m_squadAtCamp = true;
            for (uint32 i = 0; i < m_squadGuids.size(); ++i)
            {
                if (Creature* member = instance->GetCreature(m_squadGuids[i]))
                {
                    member->SetHomePosition(ClassicAQ20SquadCampPos[i]);
                    SetSquadMemberRespawnTime(member, CLASSIC_AQ20_RESPAWN_15_MINUTES);
                    member->DespawnOrUnsummon(1s, Seconds(CLASSIC_AQ20_RESPAWN_15_MINUTES));
                }
            }
        }

        // SCRIPT_COMMAND_RESPAWN_CREATURE: a dead wave creature comes back, a fighting one resets, one waiting for its respawn comes now
        void ResetWaveSpawn(ObjectGuid::LowType spawnId)
        {
            if (Creature* creature = GetWaveCreature(spawnId))
            {
                if (!creature->IsAlive())
                {
                    if (creature->getDeathState() == CORPSE)
                        creature->RemoveCorpse(false, false);
                    creature->Respawn();
                }
                else if (creature->IsInCombat() && creature->IsAIEnabled())
                    creature->AI()->EnterEvadeMode(EvadeReason::Other);
                return;
            }
            instance->Respawn(SPAWN_TYPE_CREATURE, spawnId);
        }

        int32 FindSquadSlot(ObjectGuid const& guid) const
        {
            for (uint32 i = 0; i < m_squadGuids.size(); ++i)
                if (m_squadGuids[i] == guid)
                    return int32(i);
            return -1;
        }

        Creature* SummonSquadMember(uint32 slot)
        {
            ClassicAQ20SquadSlot const& data = ClassicAQ20AndorovSquad[slot];
            // The squad spawns at the instance entrance and walks to the camp; afterwards the camp is its home (VMaNGOS sets the
            // home positions there on death and on failure, so every later respawn is at the camp)
            Position const pos = m_squadAtCamp ? ClassicAQ20SquadCampPos[slot] : Position(data.X, data.Y, data.Z, data.O);
            Creature* member = instance->SummonCreature(data.Entry, pos);
            if (!member)
                return nullptr;

            m_squadGuids[slot] = member->GetGUID();
            m_squadRespawnTimer[slot] = 0;
            member->SetHomePosition(pos);

            if (slot == 0)
            {
                m_uiAndorovGUID = member->GetGUID();
                // creature_ai_events 1547101: the event state resets with Andorov
                m_andorovStep = AndorovStep::None;
                SetData(CLASSIC_AQ20_TYPE_GENERAL_ANDOROV, NOT_STARTED);
            }

            if (m_squadAtCamp)
                return member;

            if (slot > 0)
            {
                if (Creature* pAndorov = instance->GetCreature(m_squadGuids[0]))
                    if (pAndorov->IsAlive())
                        member->GetMotionMaster()->MoveFollow(pAndorov, data.FollowDist, ChaseAngle(data.FollowAngle));
            }
            else
                StartCampWalk(member);
            return member;
        }

        // VMaNGOS GetMap()->LoadCreatureSpawnWithGroup(ANDOROV_DB_GUID)
        // TODO(classic): if the VMaNGOS spawns 301311..301315 are imported as a manual-spawn spawn group, spawn the group here instead.
        Creature* SummonAndorovSquad()
        {
            Creature* pAndorov = SummonSquadMember(0);
            if (!pAndorov)
                return nullptr;
            for (uint32 i = 1; i < ClassicAQ20AndorovSquad.size(); ++i)
                SummonSquadMember(i);
            return pAndorov;
        }

        // VMaNGOS pAndorov->Respawn()
        Creature* RespawnSquadMember(uint32 slot, Creature* current)
        {
            if (current && current->GetSpawnId())
            {
                current->Respawn(true);
                return current;
            }
            if (current)
                current->DespawnOrUnsummon();
            return SummonSquadMember(slot);
        }

        // VMaNGOS pCreature->SetRespawnTime(delay) for Andorov / Kaldorei Elites
        void SetSquadMemberRespawnTime(Creature* member, uint32 seconds)
        {
            if (member->GetSpawnId())
            {
                member->SetRespawnDelay(seconds);
                if (!member->IsAlive())
                {
                    member->SetRespawnTime(seconds);
                    member->SaveRespawnTime();
                }
                return;
            }

            int32 slot = FindSquadSlot(member->GetGUID());
            if (slot < 0)
                return;
            // "four days" = never within the instance lifetime
            m_squadRespawnTimer[slot] = seconds >= CLASSIC_AQ20_RESPAWN_FOUR_DAYS ? 0 : seconds * IN_MILLISECONDS;
        }

        void UpdateSquadRespawns(uint32 uiDiff)
        {
            for (uint32 slot = 0; slot < m_squadRespawnTimer.size(); ++slot)
            {
                if (!m_squadRespawnTimer[slot])
                    continue;

                if (m_squadRespawnTimer[slot] > uiDiff)
                {
                    m_squadRespawnTimer[slot] -= uiDiff;
                    continue;
                }

                m_squadRespawnTimer[slot] = 0;
                Creature* member = instance->GetCreature(m_squadGuids[slot]);
                if (member && member->IsAlive())
                    continue;
                RespawnSquadMember(slot, member);
            }
        }

        void SetAndorovSquadRespawnTime(uint32 nextRespawnDelay)
        {
            if (Creature* pAndorov = instance->GetCreature(m_uiAndorovGUID))
            {
                if (!pAndorov->IsAlive())
                    SetSquadMemberRespawnTime(pAndorov, nextRespawnDelay);
            }
            for (ObjectGuid const& guid : m_lKaldoreiElites)
            {
                if (Creature* pElite = instance->GetCreature(guid))
                {
                    if (!pElite->IsAlive())
                        SetSquadMemberRespawnTime(pElite, nextRespawnDelay);
                }
            }
        }

        void SetAndorovSquadImmunity(bool immune)
        {
            if (Creature* pAndorov = instance->GetCreature(m_uiAndorovGUID))
                pAndorov->SetImmuneToNPC(immune);
            for (ObjectGuid const& guid : m_lKaldoreiElites)
            {
                if (Creature* pElite = instance->GetCreature(guid))
                    pElite->SetImmuneToNPC(immune);
            }
        }

        void GiveRepAfterRajaxxDeath(Creature* pRajaxx)
        {
            FactionEntry const* factionEntry = sFactionStore.LookupEntry(CLASSIC_AQ20_FACTION_CENARION_CIRCLE); // Cenarion Circle
            if (!factionEntry)
            {
                TC_LOG_ERROR("scripts", "Rajaxx just died, unable to find Cenarion Circle faction");
                return;
            }

            bool andorovAlive = false;
            if (Creature* pAndorov = instance->GetCreature(m_uiAndorovGUID))
                if (pAndorov->IsAlive())
                    andorovAlive = true;

            std::list<Creature*> helpers;
            pRajaxx->GetCreatureListWithEntryInGrid(helpers, CLASSIC_AQ20_NPC_KALDOREI_ELITE, 400.0f);

            int32 helpersAlive = 0;
            for (Creature* helper : helpers)
            {
                if (helper->IsAlive())
                {
                    helpersAlive++;
                    if (!andorovAlive)
                    {
                        // kaldorei despawn if andorov is dead
                        helper->DespawnOrUnsummon(0s, Seconds(CLASSIC_AQ20_RESPAWN_FOUR_DAYS));
                    }
                }
            }

            if (andorovAlive)
                helpersAlive++;

            // Rep gain was buffed in patch 1.10.
            int32 const repForKill = 50;
            int32 const repPerHelper = 45;

            auto GiveRep = [factionEntry, helpersAlive, repForKill, repPerHelper](Player* pPlayer)
            {
                if (pPlayer->GetReputationMgr().GetRank(factionEntry) <= REP_EXALTED)
                {
                    for (int32 i = 0; i < 3; i++)
                        pPlayer->GetReputationMgr().ModifyReputation(factionEntry, repForKill);

                    // you get 45 rep for every alive helper
                    for (int32 i = 0; i < helpersAlive; i++)
                        pPlayer->GetReputationMgr().ModifyReputation(factionEntry, repPerHelper);
                }
            };

            // VMaNGOS: loot recipient + every member of its group. TC: every player on Rajaxx's tap list.
            for (ObjectGuid const& tapperGuid : pRajaxx->GetTapList())
                if (Player* pPlayer = instance->GetPlayer(tapperGuid))
                    if (pPlayer->IsInWorld())
                        GiveRep(pPlayer);
        }

        /**
        * The spawn logic is as follows (approximated as much as possible from multiple videos):
        * 1. From the last crystal used, find a crystal location within 80 yards.
        * 2. Spawn two crystals. One at, and one near to, this location
        * There are some rules regarding how many crystals should be spawned and where.
        * 1. Only 2 crystals should be spawned at once
        * 2. We should not spawn the same crystal as the one used
        * 3. We should maintain a short history of spawned locations to ensure some variety
        */
        void SpawnNewCrystals(ObjectGuid usedCrystal)
        {
            uint32 used = 0;
            auto previous = crystalIndexes.find(usedCrystal);
            if (previous != crystalIndexes.end())
            {
                used = previous->second;
                crystalIndexes.erase(previous);
            }

            // Truncate the history first
            crystalIndexHistory.resize(2);

            crystalIndexHistory.push_front(used);

            ClassicAQ20SpawnLocation previousLoc = ClassicAQ20CrystalSpawn.at(used);
            // Expand the search after a certain number of attempts otherwise
            // it may be impossible to spawn one within range and we deadlock
            uint32 attempts = 0;
            float maxDistanceLimit = CLASSIC_AQ20_OSSIRIAN_CRYSTAL_INITIAL_DIST;
            float minDistanceLimit = maxDistanceLimit / 2;

            // We already have another crystal spawned. Use that as the hint
            if (!crystalIndexes.empty())
            {
                maxDistanceLimit *= 0.75f;
                minDistanceLimit *= 0.75f;
                uint32 hint = crystalIndexes.begin()->second;
                previousLoc = ClassicAQ20CrystalSpawn.at(hint);
                crystalIndexHistory.push_front(hint);
            }

            std::vector<uint32> possibleIndexes;
            possibleIndexes.reserve(ClassicAQ20CrystalSpawn.size());
            for (uint32 i = 0; i < ClassicAQ20CrystalSpawn.size(); ++i)
            {
                auto iter = std::find(crystalIndexHistory.cbegin(), crystalIndexHistory.cend(), i);
                if (iter != crystalIndexHistory.cend())
                    continue;

                possibleIndexes.push_back(i);
            }

            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(possibleIndexes.begin(), possibleIndexes.end(), g);

            while (crystalIndexes.size() < CLASSIC_AQ20_OSSIRIAN_CRYSTAL_NUM_ACTIVE)
            {
                // Safety: VMaNGOS loops forever if every candidate index was consumed
                if (possibleIndexes.empty())
                    return;

                for (auto iter = possibleIndexes.begin(); iter != possibleIndexes.end() && crystalIndexes.size() < CLASSIC_AQ20_OSSIRIAN_CRYSTAL_NUM_ACTIVE;)
                {
                    if (++attempts >= 5)
                    {
                        // increase distance to search further each extra attempt
                        minDistanceLimit *= 0.9f;
                        maxDistanceLimit *= 1.1f;
                    }

                    uint32 newIndex = *iter;
                    ClassicAQ20SpawnLocation const& newLoc = ClassicAQ20CrystalSpawn.at(newIndex);
                    float dist = std::sqrt(std::pow(newLoc.x - previousLoc.x, 2.0f) + std::pow(newLoc.y - previousLoc.y, 2.0f));
                    if (dist > maxDistanceLimit || dist < minDistanceLimit)
                    {
                        ++iter;
                        continue;
                    }

                    GameObject* pCrystal = GameObject::CreateGameObject(CLASSIC_AQ20_GO_OSSIRIAN_CRYSTAL, instance,
                        Position(newLoc.x, newLoc.y, newLoc.z, 0.0f), QuaternionData(), 255, GO_STATE_READY);
                    if (pCrystal)
                    {
                        pCrystal->SetSpawnedByDefault(false);
                        if (!instance->AddToMap(pCrystal))
                        {
                            delete pCrystal;
                            pCrystal = nullptr;
                        }
                    }

                    if (!pCrystal)
                    {
                        TC_LOG_ERROR("scripts", "[OSSIRIAN] Unable to spawn crystal {} at position #{}", uint32(CLASSIC_AQ20_GO_OSSIRIAN_CRYSTAL), newIndex);
                        return;
                    }

                    // Use the new location as a hint and reduce the limit so the next one
                    // spawns close as well
                    maxDistanceLimit *= 0.50f;
                    minDistanceLimit *= 0.50f;
                    previousLoc = newLoc;
                    iter = possibleIndexes.erase(iter); // remove selected index

                    crystalGuids.push_back(pCrystal->GetGUID());
                    crystalIndexes[pCrystal->GetGUID()] = newIndex;
                }
            }
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new classic_instance_ruins_of_ahnqiraj_InstanceScript(map);
    }
};

void AddSC_classic_instance_ruins_of_ahnqiraj()
{
    new classic_instance_ruins_of_ahnqiraj();
}
