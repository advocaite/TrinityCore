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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/eastern_plaguelands/naxxramas/instance_naxxramas.cpp (GPL-2)
// Scripts: instance_naxxramas, at_naxxramas, spirit_of_naxxramas_ai, naxxramas_gargoyle_ai, naxxramas_plague_slime_ai,
//          toxic_tunnel_ai, dark_touched_warriorAI, mob_craftsman_omarion, spell_gargoyle_stoneform,
//          spell_unrelenting_rider_shadow_bolt_volley

#include "ScriptMgr.h"
#include "ChatPackets.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureData.h"
#include "DB2Stores.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MapReference.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ReputationMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "WorldSession.h"
#include "classic_boss_pack.h"
#include "classic_naxxramas.h"
#include "classic_script_text.h"
#include <vector>

namespace
{
enum NaxxEvents
{
    EVENT_BIGGLESWORTH_DIED_YELL = 1,
    EVENT_THADDIUS_SCREAM,
    EVENT_WINGBOSS_DEAD,

    EVENT_KT_LK_DIALOGUE_1,
    EVENT_KT_LK_DIALOGUE_2,
    EVENT_KT_LK_DIALOGUE_3,
    EVENT_KT_LK_DIALOGUE_4,
    EVENT_KT_LK_DIALOGUE_5,
    EVENT_KT_LK_DIALOGUE_GATE_OPEN,

    EVENT_SUMMON_FROGGER_WAVE,

    EVENT_4HM_DIALOGUE_1, // Sir Zeliek yells: Invaders! Cease this foolish venture at once! Turn away while you still can!
    EVENT_4HM_DIALOGUE_2, // Lady Blaumeux yells: Come, Zeliek, do not drive them out. Not until we've had our fun!
    EVENT_4HM_DIALOGUE_3, // Highlord Mograine yells: Enough prattling. Let them come. We shall grind their bones to dust.
    EVENT_4HM_DIALOGUE_4, // Lady Blaumeux yells: I do hope they stay long enough for me to... introduce myself.
    EVENT_4HM_DIALOGUE_5, // Sir Zeliek yells: Perhaps they will come to their senses... and run away as fast as they can.
    EVENT_4HM_DIALOGUE_6, // Thane Korth'azz yells: I've heard about enough a' yer snivelin'!Shut your flytrap before I shut it for ye'!
    EVENT_4HM_DIALOGUE_7, // Highlord Mograine yells: Conserve your anger. Harness your rage. You will all have outlets for your frustrations soon enough.

    EVENT_DKWING_INTRO_2,
    EVENT_DKWING_INTRO_3,
    EVENT_DKWING_INTRO_4,

    EVENT_SPAWN_SAPPHIRON
};

// DungeonEncounter.db2 ids (present in the 1.60 client data); only registered when present in the client data
struct ClassicNaxxEncounterId
{
    uint32 BossId;
    uint32 DungeonEncounterId;
};

ClassicNaxxEncounterId const ClassicNaxxEncounterIds[] =
{
    { TYPE_ANUB_REKHAN,   1107 },
    { TYPE_FAERLINA,      1110 },
    { TYPE_MAEXXNA,       1116 },
    { TYPE_NOTH,          1117 },
    { TYPE_HEIGAN,        1112 },
    { TYPE_LOATHEB,       1115 },
    { TYPE_RAZUVIOUS,     1113 },
    { TYPE_GOTHIK,        1109 },
    { TYPE_FOUR_HORSEMEN, 1121 },
    { TYPE_PATCHWERK,     1118 },
    { TYPE_GROBBULUS,     1111 },
    { TYPE_GLUTH,         1108 },
    { TYPE_THADDIUS,      1120 },
    { TYPE_SAPPHIRON,     1119 },
    { TYPE_KELTHUZAD,     1114 }
};

// VMaNGOS spawn guids -> TC spawn ids
constexpr ObjectGuid::LowType ClassicNaxxCreatureSpawnId(uint32 vmangosGuid) { return 20000000 + vmangosGuid; }
constexpr ObjectGuid::LowType ClassicNaxxGameObjectSpawnId(uint32 vmangosGuid) { return 30000000 + vmangosGuid; }

// VMaNGOS Geometry::IsPointLeftOfLine (2D cross product)
struct ClassicNaxxVector2
{
    float x;
    float y;
};

bool ClassicNaxxIsPointLeftOfLine(ClassicNaxxVector2 const& a, ClassicNaxxVector2 const& b, Position const& p)
{
    return ((b.x - a.x) * (p.GetPositionY() - a.y) - (b.y - a.y) * (p.GetPositionX() - a.x)) > 0.0f;
}

ClassicNaxxVector2 const DK_DOOR_A = { 2600.15f, -3008.61f };
ClassicNaxxVector2 const DK_DOOR_B = { 2579.34f, -3029.44f };

constexpr uint32 FACTION_ID_ARGENT_DAWN_NAXX = 529;
}

/*######
## instance_naxxramas
######*/

classic_instance_naxxramas_InstanceScript::classic_instance_naxxramas_InstanceScript(InstanceMap* map) : InstanceScript(map),
    m_faerlinaHaveGreeted(false),
    m_thaddiusHaveGreeted(false),
    m_haveDoneDKWingIntro(false),
    m_horsemenDeathCounter(0),
    m_sapphironSpawnState(*this, "SapphironSpawnState", NOT_STARTED),
    m_fChamberCenterX(0.0f),
    m_fChamberCenterY(0.0f),
    m_fChamberCenterZ(0.0f),
    m_uiSewageSlimeCheckTimer(500),
    m_pendingSapphironSpawn(false)
{
    SetHeaders(ClassicNaxxramasDataHeader);
    SetBossNumber(MAX_ENCOUNTER);

    std::vector<DungeonEncounterData> encounters;
    for (ClassicNaxxEncounterId const& enc : ClassicNaxxEncounterIds)
        if (sDungeonEncounterStore.LookupEntry(enc.DungeonEncounterId))
            encounters.push_back({ enc.BossId, {{ enc.DungeonEncounterId }} });
    LoadDungeonEncounterData(encounters);

    Initialize();
}

void classic_instance_naxxramas_InstanceScript::Initialize()
{
    for (uint32& i : m_auiEncounter)
        i = NOT_STARTED;
    m_events.Reset();
    // 2-5min, no idea if it's correct
    m_events.ScheduleEvent(EVENT_THADDIUS_SCREAM, Milliseconds(urand(1000 * 60 * 2, 1000 * 60 * 5)));

    m_events.ScheduleEvent(EVENT_SUMMON_FROGGER_WAVE, 6s);
}

// VMaNGOS Load(): boss states come from the TC save data (IN_PROGRESS/FAIL/SPECIAL are loaded as NOT_STARTED by TC)
void classic_instance_naxxramas_InstanceScript::AfterDataLoad()
{
    for (uint32 i = 0; i < MAX_ENCOUNTER; ++i)
    {
        EncounterState state = GetBossState(i);
        m_auiEncounter[i] = (state == TO_BE_DECIDED || state == IN_PROGRESS) ? uint32(NOT_STARTED) : uint32(state);
    }

    // VMaNGOS saved TYPE_SAPPHIRON == SPECIAL (Sapphiron summoning started)
    if (m_auiEncounter[TYPE_SAPPHIRON] != DONE && uint32(m_sapphironSpawnState) == SPECIAL)
        m_auiEncounter[TYPE_SAPPHIRON] = SPECIAL;

    // TODO(classic): VMaNGOS loaded a saved Thaddius SPECIAL as FAIL and kept other saved SPECIAL states (e.g. 4hm);
    // TC's save data only keeps NOT_STARTED/DONE boss states.
}

void classic_instance_naxxramas_InstanceScript::SetTeleporterVisualState(GameObject* pGO, uint32 uiData)
{
    if (uiData == DONE)
        pGO->SetGoState(GO_STATE_ACTIVE);
    else
        pGO->SetGoState(GO_STATE_READY);
}

void classic_instance_naxxramas_InstanceScript::SetTeleporterState(GameObject* pGO, uint32 uiData)
{
    SetTeleporterVisualState(pGO, uiData);
    if (uiData == DONE)
        pGO->RemoveFlag(GO_FLAG_NOT_SELECTABLE);
    else
        pGO->SetFlag(GO_FLAG_NOT_SELECTABLE);
}

uint8 classic_instance_naxxramas_InstanceScript::GetNumEndbossDead() const
{
    uint8 ret = 0;
    if (GetData(TYPE_MAEXXNA) == DONE)
        ++ret;
    if (GetData(TYPE_THADDIUS) == DONE)
        ++ret;
    if (GetData(TYPE_FOUR_HORSEMEN) == DONE)
        ++ret;
    if (GetData(TYPE_LOATHEB) == DONE)
        ++ret;
    return ret;
}

bool classic_instance_naxxramas_InstanceScript::HandleEvadeOutOfHome(Creature* pWho)
{
    if (pWho->IsInEvadeMode())
        return false;

    uint32 entry = pWho->GetEntry();
    float dist;
    switch (entry)
    {
        case NPC_GROBBULUS:
            dist = 180.0f;
            break;
        case NPC_FAERLINA:
            if (pWho->GetPositionZ() > 266.0f)
            {
                pWho->AI()->EnterEvadeMode();
                return false;
            }
            return true;
        case NPC_ANUB_REKHAN:
            dist = 130.0f;
            break;
        case NPC_NOTH:
            dist = 120.0f;
            break;
        case NPC_HEIGAN:
        {
            // evade if brought out of room towards bat/grub/beast gauntlet
            if (pWho->GetPositionX() > 2825.0f || pWho->GetPositionY() < -3737.0f)
            {
                pWho->AI()->EnterEvadeMode();
                return false;
            }
            dist = 90.0f;
            break;
        }
        case NPC_LOATHEB:
            dist = 100.0f;
            break;
        case NPC_GOTHIK:
            dist = 150.0f;
            break;
        case NPC_RAZUVIOUS:
            if (pWho->GetPositionZ() > 285.0f)
            {
                pWho->AI()->EnterEvadeMode();
                return false;
            }
            return true;
        case NPC_KELTHUZAD:
            dist = 130.0f;
            break;
        case NPC_BLAUMEUX:
        case NPC_MOGRAINE:
        case NPC_ZELIEK:
        case NPC_THANE:
        {
            if (ClassicNaxxIsPointLeftOfLine(DK_DOOR_A, DK_DOOR_B, pWho->GetPosition()))
            {
                for (uint32 horseman : { NPC_BLAUMEUX, NPC_MOGRAINE, NPC_ZELIEK, NPC_THANE })
                    if (Creature* pC = GetSingleCreatureFromStorage(horseman))
                        if (pC->IsAlive())
                            pC->AI()->EnterEvadeMode();
                return false;
            }

            return true;
        }
        default:
            TC_LOG_ERROR("scripts", "classic_instance_naxxramas::HandleEvadeOutOfHome called for unsupported creature {}", pWho->GetEntry());
            dist = 9999.0f;
            break;
    }

    Position const& home = pWho->GetHomePosition();
    if (pWho->GetDistance2d(home.GetPositionX(), home.GetPositionY()) > dist)
    {
        pWho->AI()->EnterEvadeMode();
        return false;
    }
    return true;
}

void classic_instance_naxxramas_InstanceScript::OnCreatureEnterCombat(Creature* creature)
{
    if (creature->GetEntry() == NPC_SewageSlime)
    {
        std::list<Creature*> sewageSlimes;
        creature->GetCreatureListWithEntryInGrid(sewageSlimes, NPC_SewageSlime, 100.0f);
        for (Creature* pC : sewageSlimes)
        {
            if (!pC->IsInCombat())
            {
                pC->CastSpell(pC, 28033, true); // aggro all in los
            }
        }
    }
}

bool classic_instance_naxxramas_InstanceScript::WingsAreCleared() const
{
    // All bosses must be dead, not just the end bosses. Some bosses aren't gated
    // so we just check them all
    for (uint32 i = 0; i < TYPE_SAPPHIRON; ++i)
    {
        if (GetData(i) != DONE)
            return false;
    }

    return true;
}

void classic_instance_naxxramas_InstanceScript::UpdateAutomaticBossEntranceDoor(NaxxGOs which, uint32 uiData, int requiredPreBossData)
{
    if (requiredPreBossData > -1 && requiredPreBossData != DONE)
        return;

    if (GameObject* pGo = GetSingleGameObjectFromStorage(which))
        UpdateAutomaticBossEntranceDoor(pGo, uiData, requiredPreBossData);
}

void classic_instance_naxxramas_InstanceScript::UpdateAutomaticBossEntranceDoor(GameObject* pGO, uint32 uiData, int requiredPreBossData)
{
    if (requiredPreBossData > -1 && requiredPreBossData != DONE)
        return;

    if (!pGO)
    {
        TC_LOG_ERROR("scripts", "classic_instance_naxxramas::UpdateAutomaticBossEntranceDoor called with nullptr GO");
        return;
    }
    if (uiData == IN_PROGRESS || uiData == SPECIAL)
    {
        pGO->SetFlag(GO_FLAG_NOT_SELECTABLE);
        pGO->SetGoState(GO_STATE_READY);
    }
    else
    {
        //pGO->RemoveFlag(GO_FLAG_NOT_SELECTABLE);
        pGO->SetGoState(GO_STATE_ACTIVE);
    }
}

void classic_instance_naxxramas_InstanceScript::UpdateManualDoor(NaxxGOs which, uint32 uiData)
{
    if (GameObject* pGo = GetSingleGameObjectFromStorage(which))
        UpdateManualDoor(pGo, uiData);
}

void classic_instance_naxxramas_InstanceScript::UpdateManualDoor(GameObject* pGO, uint32 uiData)
{
    if (uiData == DONE)
        pGO->RemoveFlag(GO_FLAG_LOCKED);
    else
        pGO->SetFlag(GO_FLAG_LOCKED);
}

void classic_instance_naxxramas_InstanceScript::UpdateBossGate(NaxxGOs which, uint32 uiData)
{
    if (GameObject* pGo = GetSingleGameObjectFromStorage(which))
        UpdateBossGate(pGo, uiData);
}

void classic_instance_naxxramas_InstanceScript::UpdateBossGate(GameObject* pGO, uint32 uiData)
{
    if (!pGO)
    {
        TC_LOG_ERROR("scripts", "classic_instance_naxxramas::UpdateBossGate called with nullptr GO");
        return;
    }
    if (uiData == DONE)
        pGO->SetGoState(GO_STATE_ACTIVE);
    else
        pGO->SetGoState(GO_STATE_READY);
}

void classic_instance_naxxramas_InstanceScript::UpdateTeleporters(uint32 uiType, uint32 uiData)
{
    // todo: what was the reason behind these? Should they despawn after 30 minutes?
    // DoRespawnGameObject(GO_<WING>_PORTAL, 30 * MINUTE);
    switch (uiType)
    {
        case TYPE_MAEXXNA:
            if (GameObject* pGO = GetSingleGameObjectFromStorage(GO_ARAC_EYE_BOSS))
                SetTeleporterVisualState(pGO, uiData);
            if (GameObject* pGO = GetSingleGameObjectFromStorage(GO_ARAC_EYE_RAMP))
                SetTeleporterVisualState(pGO, uiData);
            if (GameObject* pGO = GetSingleGameObjectFromStorage(GO_ARAC_PORTAL))
                SetTeleporterState(pGO, uiData);
            break;
        case TYPE_THADDIUS:
            if (GameObject* pGO = GetSingleGameObjectFromStorage(GO_CONS_EYE_BOSS))
                SetTeleporterVisualState(pGO, uiData);
            if (GameObject* pGO = GetSingleGameObjectFromStorage(GO_CONS_EYE_RAMP))
                SetTeleporterVisualState(pGO, uiData);
            if (GameObject* pGO = GetSingleGameObjectFromStorage(GO_CONS_PORTAL))
                SetTeleporterState(pGO, uiData);
            break;
        case TYPE_LOATHEB:
            if (GameObject* pGO = GetSingleGameObjectFromStorage(GO_PLAG_EYE_BOSS))
                SetTeleporterVisualState(pGO, uiData);
            if (GameObject* pGO = GetSingleGameObjectFromStorage(GO_PLAG_EYE_RAMP))
                SetTeleporterVisualState(pGO, uiData);
            if (GameObject* pGO = GetSingleGameObjectFromStorage(GO_PLAG_PORTAL))
                SetTeleporterState(pGO, uiData);
            break;
        case TYPE_FOUR_HORSEMEN:
            if (GameObject* pGO = GetSingleGameObjectFromStorage(GO_MILI_EYE_BOSS))
                SetTeleporterVisualState(pGO, uiData);
            if (GameObject* pGO = GetSingleGameObjectFromStorage(GO_MILI_EYE_RAMP))
                SetTeleporterVisualState(pGO, uiData);
            if (GameObject* pGO = GetSingleGameObjectFromStorage(GO_MILI_PORTAL))
                SetTeleporterState(pGO, uiData);
            break;
        default:
            TC_LOG_ERROR("scripts", "classic_instance_naxxramas::UpdateTeleporters called with unsupported type {}", uiType);
            break;
    }

    if (GameObject* pGO = GetSingleGameObjectFromStorage(GO_HUB_PORTAL))
        pGO->SetGoState(WingsAreCleared() ? GO_STATE_ACTIVE : GO_STATE_READY);
}

void classic_instance_naxxramas_InstanceScript::OnCreatureCreate(Creature* pCreature)
{
    InstanceScript::OnCreatureCreate(pCreature);

    switch (pCreature->GetEntry())
    {
        case NPC_ANUB_REKHAN:
        case NPC_FAERLINA:
        case NPC_MAEXXNA:
        case NPC_PATCHWERK:
        case NPC_GROBBULUS:
        case NPC_GLUTH:
        case NPC_THADDIUS:
        //case NPC_STALAGG:
        //case NPC_FEUGEN:
        case NPC_NOTH:
        case NPC_HEIGAN:
        case NPC_LOATHEB:
        case NPC_RAZUVIOUS:
        case NPC_GOTHIK:
        case NPC_ZELIEK:
        case NPC_THANE:
        case NPC_BLAUMEUX:
        case NPC_MOGRAINE:
        case NPC_SAPPHIRON:
        case NPC_KELTHUZAD:
        case NPC_MR_BIGGLESWORTH:
            m_mNpcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetGUID();
            break;

        case NPC_SUB_BOSS_TRIGGER:
            if (m_auiEncounter[TYPE_GOTHIK] != IN_PROGRESS)
                m_lGothTriggerList.push_back(pCreature->GetGUID());
            break;
        case NPC_SewageSlime:
            pCreature->SetWanderDistance(30.0f);
            m_sewageSlimeGuids.insert(pCreature->GetGUID());
            break;
        case NPC_BileSludge:
        {
            // hack to prevent the endless amounts of adds to spawn in case something bugs out
            std::list<Creature*> clist;
            pCreature->GetCreatureListWithEntryInGrid(clist, NPC_BileSludge, 50.0f);
            if (clist.size() > 20)
                pCreature->DespawnOrUnsummon();
            break;
        }
        default:
            break;
    }

    // 4hm
    if (pCreature->GetEntry() >= NPC_MOGRAINE && pCreature->GetEntry() <= NPC_BLAUMEUX)
    {
        if (pCreature->GetSpawnId())
            m_mHorsemenSpawnIds[pCreature->GetEntry()] = pCreature->GetSpawnId();

        // VMaNGOS respawned dead horsemen here when the encounter was not done. TC only creates living spawns, dead
        // ones are respawned in SetData(TYPE_FOUR_HORSEMEN, FAIL).
        if (m_auiEncounter[TYPE_FOUR_HORSEMEN] != DONE && pCreature->isDead())
            pCreature->Respawn();
    }

    OnCreatureRespawn(pCreature);
}

void classic_instance_naxxramas_InstanceScript::OnGameObjectCreate(GameObject* pGo)
{
    InstanceScript::OnGameObjectCreate(pGo);
    OnObjectCreate(pGo);
}

void classic_instance_naxxramas_InstanceScript::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
        case GO_ARAC_ANUB_DOOR:
        case GO_ARAC_ANUB_GATE:
        case GO_ARAC_FAER_WEB:
        case GO_ARAC_FAER_DOOR:
        case GO_ARAC_MAEX_INNER_DOOR:
        case GO_ARAC_MAEX_OUTER_DOOR:
        case GO_PLAG_SLIME01_DOOR:
        case GO_PLAG_SLIME02_DOOR:
        case GO_PLAG_NOTH_ENTRY_DOOR:
        case GO_PLAG_NOTH_EXIT_DOOR:
        case GO_PLAG_HEIG_ENTRY_DOOR:
        case GO_PLAG_HEIG_EXIT_DOOR:
        case GO_PLAG_HEIG_OLD_EXIT_DOOR:
        case GO_PLAG_LOAT_DOOR:
        case GO_MILI_GOTH_ENTRY_GATE:
        case GO_MILI_GOTH_EXIT_GATE:
        case GO_MILI_GOTH_COMBAT_GATE:
        case GO_MILI_HORSEMEN_DOOR:
        case GO_CHEST_HORSEMEN_NORM:
        case GO_CONS_PATH_EXIT_DOOR:
        case GO_CONS_GLUT_EXIT_DOOR:
        case GO_CONS_THAD_DOOR:
        case GO_KELTHUZAD_WATERFALL_DOOR:
        case GO_KELTHUZAD_DOOR:
        case GO_ARAC_EYE_RAMP:
        case GO_PLAG_EYE_RAMP:
        case GO_MILI_EYE_RAMP:
        case GO_CONS_EYE_RAMP:
        case GO_ARAC_PORTAL:
        case GO_PLAG_PORTAL:
        case GO_MILI_PORTAL:
        case GO_CONS_PORTAL:
        case GO_ARAC_EYE_BOSS:
        case GO_PLAG_EYE_BOSS:
        case GO_MILI_EYE_BOSS:
        case GO_CONS_EYE_BOSS:
        case GO_KT_WINDOW_1:
        case GO_KT_WINDOW_2:
        case GO_KT_WINDOW_3:
        case GO_KT_WINDOW_4:
        case GO_CONS_NOX_TESLA_FEUGEN:
        case GO_CONS_NOX_TESLA_STALAGG:
        case GO_HUB_PORTAL:
        case GO_SAPPHIRON_SPAWN:
            m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetGUID();
            break;
        default:
            break;
    }

    if (pGo->GetEntry() == GO_CHEST_HORSEMEN_NORM)
        m_uiHorsemenChestGUID = pGo->GetGUID();

    if (pGo->GetGoType() == GAMEOBJECT_TYPE_TRAP)
    {
        uint32 uiGoEntry = pGo->GetEntry();

        if ((uiGoEntry >= 181517 && uiGoEntry <= 181524) || uiGoEntry == 181678)
            m_alHeiganTrapGuids[0].push_back(pGo->GetGUID());
        else if ((uiGoEntry >= 181510 && uiGoEntry <= 181516) || (uiGoEntry >= 181525 && uiGoEntry <= 181531) || uiGoEntry == 181533 || uiGoEntry == 181676)
            m_alHeiganTrapGuids[1].push_back(pGo->GetGUID());
        else if ((uiGoEntry >= 181534 && uiGoEntry <= 181544) || uiGoEntry == 181532 || uiGoEntry == 181677)
            m_alHeiganTrapGuids[2].push_back(pGo->GetGUID());
        else if (uiGoEntry >= 181545 && uiGoEntry <= 181552)
        {
            if (pGo->GetSpawnId() != ClassicNaxxGameObjectSpawnId(533119) && pGo->GetSpawnId() != ClassicNaxxGameObjectSpawnId(533123)) // duplicates
                m_alHeiganTrapGuids[3].push_back(pGo->GetGUID());
        }

        if (pGo->GetSpawnId() >= ClassicNaxxGameObjectSpawnId(0))
        {
            switch (pGo->GetSpawnId() - ClassicNaxxGameObjectSpawnId(0))
            {
                case 533181:
                case 533182:
                case 533183:
                case 533184:
                case 533187:
                case 533188:
                case 533189:
                case 533190:
                case 533191:
                case 533192:
                case 533193:
                case 533194:
                case 533195:
                case 533197:
                case 533199:
                case 533200:
                    m_alHeiganTrapGuids[3].push_back(pGo->GetGUID());
                    break;
                case 533185:
                case 533196:
                case 533198:
                    m_alHeiganTrapGuids[2].push_back(pGo->GetGUID());
                    break;
                //  case 533186:
                default:
                    break;
            }
        }
    }

    switch (pGo->GetEntry())
    {
        // Arac wing
        case GO_ARAC_ANUB_DOOR:
            // starts closed by default, but must make sure it can be interracted with
            pGo->RemoveFlag(GameObjectFlags(GO_FLAG_NOT_SELECTABLE | GO_FLAG_IN_USE));
            break;
        case GO_ARAC_ANUB_GATE:
            UpdateManualDoor(pGo, m_auiEncounter[TYPE_ANUB_REKHAN]);
            if (m_auiEncounter[TYPE_ANUB_REKHAN] == DONE)
                pGo->RemoveFlag(GO_FLAG_NOT_SELECTABLE);
            break;
        case GO_ARAC_FAER_WEB:
            pGo->SetGoState(GO_STATE_ACTIVE);
            break;
        case GO_ARAC_FAER_DOOR:
            UpdateManualDoor(pGo, m_auiEncounter[TYPE_FAERLINA]);
            // todo: unable to get the door to be properly locked.
            // It has the locked flags, and it displays as locked ingame,
            // but with green text, aka it can be clicked and opened.
            // hackfix by setting no interract flag unless it should be openable.
            if (m_auiEncounter[TYPE_FAERLINA] == DONE)
                pGo->RemoveFlag(GO_FLAG_NOT_SELECTABLE);
            else
                pGo->SetFlag(GO_FLAG_NOT_SELECTABLE);
            break;
        case GO_ARAC_MAEX_OUTER_DOOR:
            UpdateBossGate(pGo, m_auiEncounter[TYPE_FAERLINA]);
            break;
        case GO_ARAC_MAEX_INNER_DOOR:
            pGo->SetGoState(GO_STATE_ACTIVE);
            break;

        // Plague wing
        case GO_PLAG_NOTH_ENTRY_DOOR:
            UpdateAutomaticBossEntranceDoor(pGo, m_auiEncounter[TYPE_NOTH]);
            break;
        case GO_PLAG_NOTH_EXIT_DOOR:
            UpdateBossGate(pGo, m_auiEncounter[TYPE_NOTH]);
            break;
        case GO_PLAG_HEIG_ENTRY_DOOR:
            UpdateAutomaticBossEntranceDoor(pGo, m_auiEncounter[TYPE_HEIGAN]);
            break;
        case GO_PLAG_HEIG_OLD_EXIT_DOOR:
        case GO_PLAG_LOAT_DOOR:
            UpdateBossGate(pGo, m_auiEncounter[TYPE_HEIGAN]);
            break;

        // Millitary wing
        case GO_MILI_GOTH_ENTRY_GATE:
            UpdateAutomaticBossEntranceDoor(pGo, m_auiEncounter[TYPE_RAZUVIOUS]);
            break;
        case GO_MILI_GOTH_EXIT_GATE:
            UpdateBossGate(pGo, m_auiEncounter[TYPE_GOTHIK]);
            break;
        case GO_MILI_HORSEMEN_DOOR:
            UpdateManualDoor(pGo, m_auiEncounter[TYPE_GOTHIK]);
            break;
        case GO_MILI_GOTH_COMBAT_GATE:
            pGo->SetGoState(GO_STATE_ACTIVE);
            break;
        case GO_CHEST_HORSEMEN_NORM:
            //todo: anything to be done?
            break;

        // Cons wing doors
        case GO_CONS_PATH_EXIT_DOOR:
            UpdateBossGate(pGo, m_auiEncounter[TYPE_PATCHWERK]);
            break;
        case GO_CONS_GLUT_EXIT_DOOR:
            UpdateBossGate(pGo, m_auiEncounter[TYPE_GLUTH]);
            [[fallthrough]]; // VMaNGOS falls through (missing break)
        case GO_CONS_THAD_DOOR:
            UpdateManualDoor(pGo, m_auiEncounter[TYPE_GLUTH]);
            break;

        // Frostwyrm lair
        case GO_KELTHUZAD_WATERFALL_DOOR:
        case GO_KELTHUZAD_DOOR:
            UpdateBossGate(pGo, m_auiEncounter[TYPE_SAPPHIRON]);
            break;

        // Teleporters visual thing
        case GO_ARAC_EYE_RAMP:
        case GO_ARAC_EYE_BOSS:
            SetTeleporterVisualState(pGo, m_auiEncounter[TYPE_MAEXXNA]);
            break;
        case GO_PLAG_EYE_RAMP:
        case GO_PLAG_EYE_BOSS:
            SetTeleporterVisualState(pGo, m_auiEncounter[TYPE_LOATHEB]);
            break;
        case GO_MILI_EYE_RAMP:
        case GO_MILI_EYE_BOSS:
            SetTeleporterVisualState(pGo, m_auiEncounter[TYPE_FOUR_HORSEMEN]);
            break;
        case GO_CONS_EYE_RAMP:
        case GO_CONS_EYE_BOSS:
            SetTeleporterVisualState(pGo, m_auiEncounter[TYPE_THADDIUS]);
            break;

        // Actual teleporters
        case GO_ARAC_PORTAL:
            SetTeleporterState(pGo, m_auiEncounter[TYPE_MAEXXNA]);
            break;
        case GO_PLAG_PORTAL:
            SetTeleporterState(pGo, m_auiEncounter[TYPE_LOATHEB]);
            break;
        case GO_MILI_PORTAL:
            SetTeleporterState(pGo, m_auiEncounter[TYPE_FOUR_HORSEMEN]);
            break;
        case GO_CONS_PORTAL:
            SetTeleporterState(pGo, m_auiEncounter[TYPE_THADDIUS]);
            break;

        case GO_KT_WINDOW_1:
        case GO_KT_WINDOW_2:
        case GO_KT_WINDOW_3:
        case GO_KT_WINDOW_4:
            if (m_auiEncounter[TYPE_KELTHUZAD] == DONE)
                pGo->SetGoState(GO_STATE_ACTIVE);
            else
                pGo->SetGoState(GO_STATE_READY);
            break;

        case GO_CONS_NOX_TESLA_FEUGEN:
        case GO_CONS_NOX_TESLA_STALAGG:
            if (m_auiEncounter[TYPE_THADDIUS] == DONE)
                pGo->SetGoState(GO_STATE_READY);
            else
                pGo->SetGoState(GO_STATE_ACTIVE);
            break;
        case GO_SAPPHIRON_SPAWN:
        {
            // Server crash handling:
            // - spawn Sapphiron immediately
            // - remove spawn anim bones
            // (deferred to the next Update: objects are not summoned/deleted while being added to the map)
            if (GetData(TYPE_SAPPHIRON) == SPECIAL)
            {
                if (m_auiEncounter[TYPE_SAPPHIRON] != DONE)
                    m_pendingSapphironSpawn = true;
                m_pendingGoDeletes.push_back(pGo->GetGUID());
            }
            break;
        }
        default:
            break;
    }
}

void classic_instance_naxxramas_InstanceScript::OnCreatureRespawn(Creature* pCreature)
{
    bool forcedDespawn = false;
    switch (pCreature->GetEntry())
    {
        case NPC_ANUB_REKHAN:
            forcedDespawn = (GetData(TYPE_ANUB_REKHAN) == DONE);
            break;
        case NPC_FAERLINA:
            forcedDespawn = (GetData(TYPE_FAERLINA) == DONE);
            break;
        case NPC_MAEXXNA:
            forcedDespawn = (GetData(TYPE_MAEXXNA) == DONE);
            break;
        case NPC_PATCHWERK:
            forcedDespawn = (GetData(TYPE_PATCHWERK) == DONE);
            break;
        case NPC_GROBBULUS:
            forcedDespawn = (GetData(TYPE_GROBBULUS) == DONE);
            break;
        case NPC_GLUTH:
            forcedDespawn = (GetData(TYPE_GLUTH) == DONE);
            break;
        case NPC_THADDIUS:
            forcedDespawn = (GetData(TYPE_THADDIUS) == DONE);
            break;
        case NPC_NOTH:
            forcedDespawn = (GetData(TYPE_NOTH) == DONE);
            break;
        case NPC_HEIGAN:
            forcedDespawn = (GetData(TYPE_HEIGAN) == DONE);
            break;
        case NPC_LOATHEB:
            forcedDespawn = (GetData(TYPE_LOATHEB) == DONE);
            break;
        case NPC_RAZUVIOUS:
            forcedDespawn = (GetData(TYPE_RAZUVIOUS) == DONE);
            break;
        case NPC_GOTHIK:
            forcedDespawn = (GetData(TYPE_GOTHIK) == DONE);
            break;
        case NPC_ZELIEK:
        case NPC_THANE:
        case NPC_BLAUMEUX:
        case NPC_MOGRAINE:
            forcedDespawn = (GetData(TYPE_FOUR_HORSEMEN) == DONE);
            break;
        case NPC_SAPPHIRON:
            forcedDespawn = (GetData(TYPE_SAPPHIRON) == DONE);
            break;
        case NPC_KELTHUZAD:
            forcedDespawn = (GetData(TYPE_KELTHUZAD) == DONE);
            break;
        default:
            break;
    }

    // Something, probably silly, makes gothik respawn, thus the trash
    // linked to him gets a chance to respawn as well. Force-despawning his
    // trash like this as well to prevent that. There should be no deathknight captains,
    // deathknight cavaliers or necro knights after gothik, so this works.
    // The hardcoded dbGUID is a lonely shade of naxxramas
    if (GetData(TYPE_GOTHIK) == DONE)
    {
        uint32 e = pCreature->GetEntry();
        if (e == NPC_UnholyAxe ||
            e == NPC_UnholyStaff ||
            e == NPC_UnholySwords ||
            e == NPC_NecroKnight ||
            e == NPC_DeathKnightCaptain ||
            e == NPC_DeathKnightCavalier ||
            pCreature->GetSpawnId() == ClassicNaxxCreatureSpawnId(88470))
        {
            forcedDespawn = true;
        }
    }

    // VMaNGOS AddObjectToRemoveList()
    if (forcedDespawn)
        pCreature->DespawnOrUnsummon(0s, Seconds(7 * DAY));
}

bool classic_instance_naxxramas_InstanceScript::IsEncounterInProgress() const
{
    for (uint32 i : m_auiEncounter)
        if (i == IN_PROGRESS || i == SPECIAL)
            return true;

    return false;
}

void classic_instance_naxxramas_InstanceScript::SetData(uint32 uiType, uint32 uiData)
{
    bool sameStateAsLast = false;
    if (uiType < MAX_ENCOUNTER)
        sameStateAsLast = (m_auiEncounter[uiType] == uiData);

    switch (uiType)
    {
        case TYPE_ANUB_REKHAN:
            m_auiEncounter[uiType] = uiData;
            if (GameObject* pGo = GetSingleGameObjectFromStorage(GO_ARAC_ANUB_DOOR))
            {
                if (uiData == IN_PROGRESS)
                    pGo->SetGoState(GO_STATE_READY);
                else
                    pGo->SetGoState(GO_STATE_ACTIVE);
            }
            UpdateManualDoor(GO_ARAC_ANUB_GATE, uiData);
            break;
        case TYPE_FAERLINA:
            // a Faerlina that resets alive brings back the Worshippers and Followers killed in the failed attempt
            if (m_auiEncounter[uiType] == IN_PROGRESS && (uiData == NOT_STARTED || uiData == FAIL))
                ClassicRespawnBossPack(instance, NPC_FAERLINA, { NPC_NaxxramasFollower, NPC_NaxxramasWorshipper });
            m_auiEncounter[uiType] = uiData;
            UpdateAutomaticBossEntranceDoor(GO_ARAC_FAER_WEB, uiData);

            UpdateManualDoor(GO_ARAC_FAER_DOOR, uiData);
            UpdateBossGate(GO_ARAC_MAEX_OUTER_DOOR, uiData);
            // todo: unable to get the door to be properly locked.
            // It has the locked flags, and it displays as locked ingame,
            // but with green text, aka it can be clicked and opened.
            // hackfix by setting no interract flag unless it should be openable.
            if (GameObject* pGo = GetSingleGameObjectFromStorage(GO_ARAC_FAER_DOOR))
            {
                if (uiData == DONE)
                    pGo->RemoveFlag(GO_FLAG_NOT_SELECTABLE);
                else
                    pGo->SetFlag(GO_FLAG_NOT_SELECTABLE);
            }
            break;
        case TYPE_MAEXXNA:
            if (uiData == DONE)
                m_events.ScheduleEvent(EVENT_WINGBOSS_DEAD, 10s);
            m_auiEncounter[uiType] = uiData;
            UpdateAutomaticBossEntranceDoor(GO_ARAC_MAEX_INNER_DOOR, uiData, m_auiEncounter[TYPE_FAERLINA]);
            UpdateTeleporters(uiType, uiData);
            break;
        case TYPE_NOTH:
            m_auiEncounter[uiType] = uiData;
            UpdateAutomaticBossEntranceDoor(GO_PLAG_NOTH_ENTRY_DOOR, uiData);
            UpdateBossGate(GO_PLAG_NOTH_EXIT_DOOR, uiData);
            UpdateBossGate(GO_PLAG_HEIG_ENTRY_DOOR, uiData);
            break;
        case TYPE_HEIGAN:
            m_auiEncounter[uiType] = uiData;
            // entry door is controlled by boss script
            UpdateBossGate(GO_PLAG_LOAT_DOOR, uiData);
            UpdateBossGate(GO_PLAG_HEIG_OLD_EXIT_DOOR, uiData);
            break;
        case TYPE_LOATHEB:
            if (uiData == DONE)
                m_events.ScheduleEvent(EVENT_WINGBOSS_DEAD, 10s);
            m_auiEncounter[uiType] = uiData;
            UpdateAutomaticBossEntranceDoor(GO_PLAG_LOAT_DOOR, uiData, m_auiEncounter[TYPE_HEIGAN]);
            UpdateTeleporters(uiType, uiData);
            break;
        case TYPE_RAZUVIOUS:
            m_auiEncounter[uiType] = uiData;
            UpdateBossGate(GO_MILI_GOTH_ENTRY_GATE, uiData);
            break;
        case TYPE_GOTHIK:
            m_auiEncounter[uiType] = uiData;
            UpdateAutomaticBossEntranceDoor(GO_MILI_GOTH_ENTRY_GATE, uiData);
            UpdateBossGate(GO_MILI_GOTH_EXIT_GATE, uiData);
            if (GameObject* pGO = GetSingleGameObjectFromStorage(GO_MILI_GOTH_COMBAT_GATE))
            {
                switch (uiData)
                {
                    case IN_PROGRESS:
                        pGO->SetGoState(GO_STATE_READY);
                        break;
                    case SPECIAL:
                        pGO->SetGoState(GO_STATE_ACTIVE);
                        break;
                    case FAIL:
                        //if (m_auiEncounter[TYPE_GOTHIK] == IN_PROGRESS)
                        pGO->SetGoState(GO_STATE_ACTIVE);
                        break;
                    case DONE:
                        pGO->SetGoState(GO_STATE_ACTIVE);
                        m_events.ScheduleEvent(EVENT_4HM_DIALOGUE_1, 10s); // todo: don't know if it should trigger here or when opening 4hm door
                        break;
                    default:
                        break;
                }
            }
            UpdateManualDoor(GO_MILI_HORSEMEN_DOOR, uiData);
            break;
        case TYPE_FOUR_HORSEMEN:
            if (uiData == DONE)
                m_events.ScheduleEvent(EVENT_WINGBOSS_DEAD, 10s);
            m_auiEncounter[uiType] = uiData;

            UpdateAutomaticBossEntranceDoor(GO_MILI_HORSEMEN_DOOR, uiData, m_auiEncounter[TYPE_GOTHIK]);
            UpdateTeleporters(uiType, uiData);
            if (uiData == SPECIAL)
            {
                ++m_horsemenDeathCounter;
                if (m_horsemenDeathCounter >= 4)
                {
                    SetData(TYPE_FOUR_HORSEMEN, DONE);
                    return; // the nested call already did the rest
                }
            }
            else if (uiData == FAIL)
            {
                m_horsemenDeathCounter = 0;
                for (uint32 i = NPC_MOGRAINE; i <= NPC_BLAUMEUX; i++)
                {
                    Creature* p = GetSingleCreatureFromStorage(i, true);
                    if (p && p->IsInWorld())
                    {
                        if (p->isDead())
                            p->Respawn();
                    }
                    else
                    {
                        // corpse already removed: respawn the spawn point
                        auto itr = m_mHorsemenSpawnIds.find(i);
                        if (itr != m_mHorsemenSpawnIds.end())
                            instance->Respawn(SPAWN_TYPE_CREATURE, itr->second);
                    }
                }
            }
            else if (uiData == DONE)
            {
                // spawns it for 30 minutes?
                DoRespawnGameObject(m_uiHorsemenChestGUID);
                if (Creature* pZeliek = GetSingleCreatureFromStorage(NPC_ZELIEK))
                {
                    for (uint32 spiritEntry : { 16775u, 16776u, 16777u, 16778u })
                    {
                        std::list<Creature*> spirits;
                        pZeliek->GetCreatureListWithEntryInGrid(spirits, spiritEntry, 300.0f);
                        for (Creature* pC : spirits)
                            pC->DespawnOrUnsummon();
                    }
                }

                // reputation
                if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(FACTION_ID_ARGENT_DAWN_NAXX))
                {
                    for (MapReference const& ref : instance->GetPlayers())
                        if (Player* pPlayer = ref.GetSource())
                            pPlayer->GetReputationMgr().ModifyReputation(factionEntry, 100);
                }
                else
                    TC_LOG_ERROR("scripts", "4hm just died. Unable to find Argent Dawn faction for reputation");
            }
            break;
        case TYPE_PATCHWERK:
            m_auiEncounter[uiType] = uiData;
            UpdateBossGate(GO_CONS_PATH_EXIT_DOOR, uiData);
            break;
        case TYPE_GROBBULUS:
            UpdateAutomaticBossEntranceDoor(GO_CONS_PATH_EXIT_DOOR, uiData, m_auiEncounter[TYPE_PATCHWERK]);
            m_auiEncounter[uiType] = uiData;
            break;
        case TYPE_GLUTH:
            m_auiEncounter[uiType] = uiData;
            UpdateBossGate(GO_CONS_GLUT_EXIT_DOOR, uiData);
            UpdateManualDoor(GO_CONS_THAD_DOOR, uiData);
            break;
        case TYPE_THADDIUS:
            // Only set the same state once
            if (uiData == m_auiEncounter[uiType])
                break;

            if (uiData == DONE)
                m_events.ScheduleEvent(EVENT_WINGBOSS_DEAD, 10s);
            m_auiEncounter[uiType] = uiData;

            UpdateAutomaticBossEntranceDoor(GO_CONS_THAD_DOOR, uiData, m_auiEncounter[TYPE_GLUTH]);

            UpdateTeleporters(uiType, uiData);
            break;
        case TYPE_SAPPHIRON:
            if (uiData == DONE)
                m_events.ScheduleEvent(EVENT_KT_LK_DIALOGUE_1, 12s);

            // Start Sapphiron summoning process
            if (uiData == SPECIAL)
            {
                m_events.ScheduleEvent(EVENT_SPAWN_SAPPHIRON, Milliseconds(SPAWN_ANIM_TIMER));
                m_sapphironSpawnState = uint32(SPECIAL); // VMaNGOS saved the instance data on Sapphiron SPECIAL
            }

            m_auiEncounter[uiType] = uiData;
            UpdateBossGate(GO_KELTHUZAD_WATERFALL_DOOR, uiData);
            // GO_KELTHUZAD_DOOR is opened at the end of EVENT_KT_LK_DIALOGUE
            break;
        case TYPE_KELTHUZAD:
            UpdateAutomaticBossEntranceDoor(GO_KELTHUZAD_DOOR, uiData, m_auiEncounter[TYPE_SAPPHIRON]);
            switch (uiData)
            {
                case SPECIAL:
                {
                    if (instance->GetPlayers().empty())
                        return;

                    bool bCanBegin = true;

                    for (MapReference const& ref : instance->GetPlayers())
                    {
                        if (Player* pPlayer = ref.GetSource())
                        {
                            if (!pPlayer->IsWithinDist2d(m_fChamberCenterX, m_fChamberCenterY, 15.0f))
                                bCanBegin = false;
                        }
                    }

                    if (bCanBegin)
                        m_auiEncounter[uiType] = IN_PROGRESS;

                    break;
                }
                case FAIL:
                    m_auiEncounter[uiType] = NOT_STARTED;
                    break;
                default:
                    m_auiEncounter[uiType] = uiData;
                    break;
            }
            break;
        default:
            break;
    }

    // TODO(classic): VMaNGOS incremented sInstanceStatistics wipe counters here (uiData == FAIL && !sameStateAsLast,
    // boss in combat for > 10s). No TC equivalent.
    (void)sameStateAsLast;

    // Mirror the boss state into TC (instance save data / lockouts / encounter frames).
    if (uiType < MAX_ENCOUNTER && GetBossState(uiType) != DONE)
        SetBossState(uiType, EncounterState(m_auiEncounter[uiType]));
}

uint32 classic_instance_naxxramas_InstanceScript::GetData(uint32 uiType) const
{
    if (uiType < MAX_ENCOUNTER)
        return m_auiEncounter[uiType];

    TC_LOG_ERROR("scripts", "classic_instance_naxxramas::GetData() called with {} as param. {} is MAX_ENCOUNTERS", uiType, uint32(MAX_ENCOUNTER));
    return 0;
}

ObjectGuid classic_instance_naxxramas_InstanceScript::GetGuidData(uint32 /*uiData*/) const
{
    TC_LOG_DEBUG("scripts", "classic_instance_naxxramas::GetGuidData called. Not implemented");
    return ObjectGuid::Empty;
}

// VMaNGOS searched m_mNpcEntryGuidStore by mistake (so it never found anything); unused by the scripts
ObjectGuid classic_instance_naxxramas_InstanceScript::GetGOUuid(NaxxGOs which) const
{
    auto it = m_mGoEntryGuidStore.find(which);
    if (it == m_mGoEntryGuidStore.end())
    {
        TC_LOG_ERROR("scripts", "classic_instance_naxxramas::GetGOUuid called with param {}, not found", uint32(which));
        return ObjectGuid::Empty;
    }
    return it->second;
}

void classic_instance_naxxramas_InstanceScript::SetGothTriggers()
{
    Creature* pGoth = GetSingleCreatureFromStorage(NPC_GOTHIK);

    if (!pGoth)
        return;

    for (ObjectGuid const& guid : m_lGothTriggerList)
    {
        if (Creature* pTrigger = instance->GetCreature(guid))
        {
            GothTrigger pGt;
            pGt.bIsAnchorHigh = (pTrigger->GetPositionZ() >= (pGoth->GetPositionZ() - 5.0f));
            pGt.bIsRightSide = IsInRightSideGothArea(pTrigger);

            m_mGothTriggerMap[pTrigger->GetGUID()] = pGt;
        }
    }
}

Creature* classic_instance_naxxramas_InstanceScript::GetClosestAnchorForGoth(Creature* pSource, bool bRightSide)
{
    std::list<Creature*> lList;

    for (auto const& itr : m_mGothTriggerMap)
    {
        if (!itr.second.bIsAnchorHigh)
            continue;

        if (itr.second.bIsRightSide != bRightSide)
            continue;

        if (Creature* pCreature = instance->GetCreature(itr.first))
            lList.push_back(pCreature);
    }

    if (!lList.empty())
    {
        lList.sort(Trinity::ObjectDistanceOrderPred(pSource));
        return lList.front();
    }

    return nullptr;
}

void classic_instance_naxxramas_InstanceScript::GetGothSummonPointCreatures(std::list<Creature*> &lList, bool bRightSide)
{
    for (auto const& itr : m_mGothTriggerMap)
    {
        if (itr.second.bIsAnchorHigh)
            continue;

        if (itr.second.bIsRightSide != bRightSide)
            continue;

        if (Creature* pCreature = instance->GetCreature(itr.first))
            lList.push_back(pCreature);
    }
}

bool classic_instance_naxxramas_InstanceScript::IsInRightSideGothArea(Unit const* pUnit)
{
    if (GameObject* pCombatGate = GetSingleGameObjectFromStorage(GO_MILI_GOTH_COMBAT_GATE))
        return (pCombatGate->GetPositionY() >= pUnit->GetPositionY());

    TC_LOG_ERROR("scripts", "left/right side check, Gothik combat area failed.");
    return true;
}

void classic_instance_naxxramas_InstanceScript::SetChamberCenterCoords(float fX, float fY, float fZ)
{
    m_fChamberCenterX = fX;
    m_fChamberCenterY = fY;
    m_fChamberCenterZ = fZ;
}

void classic_instance_naxxramas_InstanceScript::ToggleKelThuzadWindows(bool setOpen)
{
    for (uint32 i = GO_KT_WINDOW_1; i <= GO_KT_WINDOW_4; i++)
    {
        if (GameObject* pGo = GetSingleGameObjectFromStorage(i))
            pGo->SetGoState(setOpen ? GO_STATE_ACTIVE : GO_STATE_READY);
    }
}

void classic_instance_naxxramas_InstanceScript::OnUnitDeath(Unit* unit)
{
    if (Player* player = unit->ToPlayer())
        OnPlayerDeath(player);
    else if (Creature* creature = unit->ToCreature())
        OnCreatureDeath(creature);
}

void classic_instance_naxxramas_InstanceScript::OnPlayerDeath(Player* p)
{
    if (m_auiEncounter[TYPE_ANUB_REKHAN] == IN_PROGRESS)
    {
        // On player death we spawn 5 scarabs under the player. Since the player
        // can die from falldmg or other sources, anubs script impl of KilledUnit may not
        // be called, thus we need to do it here.
        if (Creature* pAnub = GetSingleCreatureFromStorage(NPC_ANUB_REKHAN))
        {
            //pAnub->AI()->DoCast(p, 29105, true);
            // TODO(classic): VMaNGOS pAnub->SendSpellGo(p, 28864) (visual only of Summon Corpse Scarabs); no TC equivalent
            for (int i = 0; i < 5; i++)
            {
                if (Creature* cs = pAnub->SummonCreature(16698, p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), 0.0f,
                    TEMPSUMMON_CORPSE_DESPAWN))
                {
                    if (cs->AI())
                    {
                        cs->AI()->DoZoneInCombat();
                        if (Unit* csTarget = pAnub->AI() ? pAnub->AI()->SelectTarget(SelectTargetMethod::Random, 0) : nullptr)
                        {
                            cs->AI()->AttackStart(csTarget);
                            cs->GetThreatManager().AddThreat(csTarget, 5000.0f);
                        }
                    }
                }
            }
        }
    }
}

void classic_instance_naxxramas_InstanceScript::OnCreatureDeath(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
        case NPC_MR_BIGGLESWORTH:
        {
            if (GetData(TYPE_KELTHUZAD) != DONE)
            {
                m_events.ScheduleEvent(EVENT_BIGGLESWORTH_DIED_YELL, 1s);
                // TODO(classic): VMaNGOS sInstanceStatistics.IncrementCustomCounter(MR_BIGGLESWORTH_KILLS)
            }
            break;
        }
        case NPC_FrenziedBat:
        case NPC_PlaguedBat:
        case NPC_MutatedGrub:
        case NPC_PlagueBeast:
            pCreature->DespawnOrUnsummon(10s);
            break;
        case NPC_EmbalmingSlime:
            pCreature->DespawnOrUnsummon(30s);
            break;
        case NPC_LightningTotem:
            pCreature->DespawnOrUnsummon();
            break;
        default:
            break;
    }
}

void classic_instance_naxxramas_InstanceScript::Update(uint32 diff)
{
    // Deferred parts of OnObjectCreate (GO_SAPPHIRON_SPAWN crash recovery)
    if (m_pendingSapphironSpawn)
    {
        m_pendingSapphironSpawn = false;
        instance->SummonCreature(NPC_SAPPHIRON, Position(aSapphPositions[0], aSapphPositions[1], aSapphPositions[2], aSapphPositions[3]));
    }
    if (!m_pendingGoDeletes.empty())
    {
        for (ObjectGuid const& guid : m_pendingGoDeletes)
            if (GameObject* pGo = instance->GetGameObject(guid))
                pGo->Delete();
        m_pendingGoDeletes.clear();
    }

    // VMaNGOS InstanceData::OnCreatureEnterCombat for Sewage Slimes (TC has no instance-wide combat hook)
    if (m_uiSewageSlimeCheckTimer <= diff)
    {
        m_uiSewageSlimeCheckTimer = 500;
        for (auto itr = m_sewageSlimeGuids.begin(); itr != m_sewageSlimeGuids.end();)
        {
            Creature* slime = instance->GetCreature(*itr);
            if (!slime)
            {
                m_sewageSlimesInCombat.erase(*itr);
                itr = m_sewageSlimeGuids.erase(itr);
                continue;
            }

            if (slime->IsInCombat())
            {
                if (m_sewageSlimesInCombat.insert(*itr).second)
                    OnCreatureEnterCombat(slime);
            }
            else
                m_sewageSlimesInCombat.erase(*itr);
            ++itr;
        }
    }
    else
        m_uiSewageSlimeCheckTimer -= diff;

    m_events.Update(diff);
    while (uint32 l_EventId = m_events.ExecuteEvent())
    {
        switch (l_EventId)
        {
            case EVENT_BIGGLESWORTH_DIED_YELL:
                DoOrSimulateScriptTextForThisInstance(KELTHUZAD_SAY_CAT_DIED, NPC_KELTHUZAD);
                break;
            case EVENT_THADDIUS_SCREAM:
                if (m_auiEncounter[TYPE_THADDIUS] != DONE)
                {
                    if (m_auiEncounter[TYPE_THADDIUS] != IN_PROGRESS && m_auiEncounter[TYPE_THADDIUS] != SPECIAL)
                        DoOrSimulateScriptTextForThisInstance(THADDIUS_SAY_SCREAM1 + urand(0, 3), NPC_THADDIUS);
                    m_events.ScheduleEvent(EVENT_THADDIUS_SCREAM, Minutes(urand(5, 10)));
                }
                break;
            case EVENT_WINGBOSS_DEAD:
                if (uint8 numDead = GetNumEndbossDead())
                    DoOrSimulateScriptTextForThisInstance(KELTHUZAD_SAY_TAUNT1 + numDead - 1, NPC_KELTHUZAD);
                break;
            case EVENT_KT_LK_DIALOGUE_1:
                DoOrSimulateScriptTextForThisInstance(SAY_SAPP_DIALOG1, NPC_KELTHUZAD);
                m_events.ScheduleEvent(EVENT_KT_LK_DIALOGUE_2, 5s);
                break;
            case EVENT_KT_LK_DIALOGUE_2:
                DoOrSimulateScriptTextForThisInstance(SAY_SAPP_DIALOG2_LICH, NPC_LICH_KING);
                m_events.ScheduleEvent(EVENT_KT_LK_DIALOGUE_3, 16500ms);
                break;
            case EVENT_KT_LK_DIALOGUE_3:
                DoOrSimulateScriptTextForThisInstance(SAY_SAPP_DIALOG3, NPC_KELTHUZAD);
                m_events.ScheduleEvent(EVENT_KT_LK_DIALOGUE_4, 6s);
                break;
            case EVENT_KT_LK_DIALOGUE_4:
                DoOrSimulateScriptTextForThisInstance(SAY_SAPP_DIALOG4_LICH, NPC_LICH_KING);
                m_events.ScheduleEvent(EVENT_KT_LK_DIALOGUE_5, 8s);
                break;
            case EVENT_KT_LK_DIALOGUE_5:
                DoOrSimulateScriptTextForThisInstance(SAY_SAPP_DIALOG5, NPC_KELTHUZAD);
                m_events.ScheduleEvent(EVENT_KT_LK_DIALOGUE_GATE_OPEN, 5500ms);
                break;
            case EVENT_KT_LK_DIALOGUE_GATE_OPEN:
                UpdateBossGate(GO_KELTHUZAD_DOOR, DONE);
                break;
            case EVENT_SUMMON_FROGGER_WAVE:
            {
                static constexpr float pos[6][4] = {
                    {3128.66f, -3121.27f, 293.341f, 4.73893f},
                    {3154.58f, -3126.18f, 293.591f, 4.43020f},
                    {3175.28f, -3134.76f, 293.437f, 4.24492f},
                    {3129.630f, -3157.652f, 293.32f, 4.73893f},
                    {3144.894f, -3159.587f, 293.32f, 4.43020f},
                    {3159.510f, -3166.001f, 293.27f, 4.24492f} };

                for (int i = 0; i < 3; i++)
                {
                    if (Creature* frogger = instance->SummonCreature(NPC_LivingPoison, Position(pos[i][0], pos[i][1], pos[i][2], pos[i][3]), nullptr, 13s))
                        frogger->GetMotionMaster()->MovePoint(0, pos[i + 3][0], pos[i + 3][1], pos[i + 3][2]);
                }
                m_events.Repeat(6s);
                break;
            }
            case EVENT_4HM_DIALOGUE_1:
                DoOrSimulateScriptTextForMap(SAY_4HM_DIALOGUE_1, NPC_ZELIEK, GetSingleCreatureFromStorage(NPC_ZELIEK, true));
                m_events.ScheduleEvent(EVENT_4HM_DIALOGUE_2, 7s);
                break;
            case EVENT_4HM_DIALOGUE_2:
                DoOrSimulateScriptTextForMap(SAY_4HM_DIALOGUE_2, NPC_BLAUMEUX, GetSingleCreatureFromStorage(NPC_BLAUMEUX, true));
                m_events.ScheduleEvent(EVENT_4HM_DIALOGUE_3, 7s);
                break;
            case EVENT_4HM_DIALOGUE_3:
                DoOrSimulateScriptTextForMap(SAY_4HM_DIALOGUE_3, NPC_MOGRAINE, GetSingleCreatureFromStorage(NPC_MOGRAINE, true));
                m_events.ScheduleEvent(EVENT_4HM_DIALOGUE_4, 7s);
                break;
            case EVENT_4HM_DIALOGUE_4:
                DoOrSimulateScriptTextForMap(SAY_4HM_DIALOGUE_4, NPC_BLAUMEUX, GetSingleCreatureFromStorage(NPC_BLAUMEUX, true));
                m_events.ScheduleEvent(EVENT_4HM_DIALOGUE_5, 7s);
                break;
            case EVENT_4HM_DIALOGUE_5:
                DoOrSimulateScriptTextForMap(SAY_4HM_DIALOGUE_5, NPC_ZELIEK, GetSingleCreatureFromStorage(NPC_ZELIEK, true));
                m_events.ScheduleEvent(EVENT_4HM_DIALOGUE_6, 6s);
                break;
            case EVENT_4HM_DIALOGUE_6:
                DoOrSimulateScriptTextForMap(SAY_4HM_DIALOGUE_6, NPC_THANE, GetSingleCreatureFromStorage(NPC_THANE, true));
                m_events.ScheduleEvent(EVENT_4HM_DIALOGUE_7, 7s);
                break;
            case EVENT_4HM_DIALOGUE_7:
                DoOrSimulateScriptTextForMap(SAY_4HM_DIALOGUE_7, NPC_MOGRAINE, GetSingleCreatureFromStorage(NPC_MOGRAINE, true));
                break;
            case EVENT_DKWING_INTRO_2:
                DoOrSimulateScriptTextForMap(SAY_ZELI_TAUNT3, NPC_ZELIEK, GetSingleCreatureFromStorage(NPC_ZELIEK, true));
                m_events.ScheduleEvent(EVENT_DKWING_INTRO_3, 5s);
                break;
            case EVENT_DKWING_INTRO_3:
                DoOrSimulateScriptTextForMap(SAY_MOG_TAUNT3, NPC_MOGRAINE, GetSingleCreatureFromStorage(NPC_MOGRAINE, true));
                m_events.ScheduleEvent(EVENT_DKWING_INTRO_4, 6200ms);
                break;
            case EVENT_DKWING_INTRO_4:
                DoOrSimulateScriptTextForMap(SAY_BLAU_TAUNT3, NPC_BLAUMEUX, GetSingleCreatureFromStorage(NPC_BLAUMEUX, true));
                break;
            case EVENT_SPAWN_SAPPHIRON:
            {
                if (Player* pPlayer = GetPlayerInMap())
                    pPlayer->SummonCreature(NPC_SAPPHIRON, aSapphPositions[0], aSapphPositions[1], aSapphPositions[2], aSapphPositions[3], TEMPSUMMON_DEAD_DESPAWN);
                break;
            }
            default:
                break;
        }
    }
}

void classic_instance_naxxramas_InstanceScript::onNaxxramasAreaTrigger(Player* pPlayer, AreaTriggerEntry const* pAt)
{
    switch (pAt->ID)
    {
        case AREATRIGGER_HUB_TO_FROSTWYRM:
            if (WingsAreCleared() || pPlayer->IsGameMaster())
                pPlayer->TeleportTo(toFrostwyrmTPPos);
            break;
        case AREATRIGGER_KELTHUZAD:
            OnKTAreaTrigger(pAt);
            break;
        case AREATRIGGER_FAERLINA:
            if (!m_faerlinaHaveGreeted)
            {
                m_faerlinaHaveGreeted = true;
                if (Creature* pFaerlina = GetSingleCreatureFromStorage(NPC_FAERLINA))
                {
                    if (pFaerlina->IsAlive())
                        ClassicScriptText(SAY_FAERLINA_GREET, pFaerlina);
                }
            }
            break;
        case AREATRIGGER_THADDIUS_ENTRANCE:
            if (!m_thaddiusHaveGreeted)
            {
                m_thaddiusHaveGreeted = true;
                if (Creature* pThaddius = GetSingleCreatureFromStorage(NPC_THADDIUS))
                {
                    if (pThaddius->IsAlive())
                        ClassicScriptText(SAY_THADDIUS_GREET, pThaddius);
                }
            }
            break;
        case AREATRIGGER_START_DK_WING:
            if (!m_haveDoneDKWingIntro)
            {
                m_haveDoneDKWingIntro = true;
                if (GetData(TYPE_FOUR_HORSEMEN) != DONE)
                {
                    DoOrSimulateScriptTextForMap(SAY_KORT_TAUNT1, NPC_THANE, GetSingleCreatureFromStorage(NPC_THANE, true));
                    m_events.ScheduleEvent(EVENT_DKWING_INTRO_2, 5500ms);
                }
            }
            break;
        default:
            break;
    }
}

// ---- VMaNGOS ScriptedInstance helpers ----

Creature* classic_instance_naxxramas_InstanceScript::GetSingleCreatureFromStorage(uint32 uiEntry, bool bSkipDebugLog)
{
    auto find = m_mNpcEntryGuidStore.find(uiEntry);
    if (find != m_mNpcEntryGuidStore.end())
        return instance->GetCreature(find->second);

    // Output log, possible reason is not added GO to map, or not yet loaded;
    if (!bSkipDebugLog)
        TC_LOG_DEBUG("scripts", "classic_instance_naxxramas: requested creature with entry {} is not created or not loaded.", uiEntry);

    return nullptr;
}

GameObject* classic_instance_naxxramas_InstanceScript::GetSingleGameObjectFromStorage(uint32 uiEntry)
{
    auto find = m_mGoEntryGuidStore.find(uiEntry);
    if (find != m_mGoEntryGuidStore.end())
        return instance->GetGameObject(find->second);

    TC_LOG_DEBUG("scripts", "classic_instance_naxxramas: requested gameobject with entry {} is not created or not loaded.", uiEntry);
    return nullptr;
}

Creature* classic_instance_naxxramas_InstanceScript::GetCreature(ObjectGuid const& guid)
{
    return instance->GetCreature(guid);
}

GameObject* classic_instance_naxxramas_InstanceScript::GetGameObject(ObjectGuid const& guid)
{
    return instance->GetGameObject(guid);
}

Player* classic_instance_naxxramas_InstanceScript::GetPlayerInMap(bool bOnlyAlive, bool bCanBeGamemaster)
{
    for (MapReference const& ref : instance->GetPlayers())
    {
        Player* pPlayer = ref.GetSource();
        if (pPlayer && (!bOnlyAlive || pPlayer->IsAlive()) && (bCanBeGamemaster || !pPlayer->IsGameMaster()))
            return pPlayer;
    }

    return nullptr;
}

void classic_instance_naxxramas_InstanceScript::DoOrSimulateScriptTextForThisInstance(int32 iTextEntry, uint32 uiCreatureEntry)
{
    // Prevent debug output in GetSingleCreatureFromStorage
    DoOrSimulateScriptTextForMap(iTextEntry, uiCreatureEntry, GetSingleCreatureFromStorage(uiCreatureEntry, true));
}

void classic_instance_naxxramas_InstanceScript::DoOrSimulateScriptTextForMap(int32 iTextEntry, uint32 uiCreatureEntry, Creature* pSource)
{
    if (pSource && pSource->IsInWorld())
    {
        ClassicScriptText(uint32(iTextEntry), pSource);
        return;
    }

    // Speaker not loaded: simulate the text map-wide with the creature's name
    BroadcastTextEntry const* bct = sBroadcastTextStore.LookupEntry(uint32(iTextEntry));
    CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(uiCreatureEntry);
    if (!bct || !cInfo)
        return;

    // TODO(classic): VMaNGOS used the text's own chat type; simulated texts are always sent as monster yells here
    for (MapReference const& ref : instance->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!player)
            continue;

        LocaleConstant locale = player->GetSession()->GetSessionDbcLocale();
        WorldPackets::Chat::Chat packet;
        packet.Initialize(CHAT_MSG_MONSTER_YELL, LANG_UNIVERSAL, nullptr, nullptr, DB2Manager::GetBroadcastTextValue(bct, locale), 0, "", locale);
        packet.SenderName = cInfo->Name;
        packet.BroadcastTextID = bct->ID;
        player->SendDirectMessage(packet.Write());
    }
}

classic_instance_naxxramas_InstanceScript* GetClassicNaxxInstance(WorldObject const* obj)
{
    if (!obj)
        return nullptr;

    return dynamic_cast<classic_instance_naxxramas_InstanceScript*>(obj->GetInstanceScript());
}

class classic_instance_naxxramas : public InstanceMapScript
{
public:
    classic_instance_naxxramas() : InstanceMapScript(ClassicNaxxramasScriptName, MAP_NAXXRAMAS) { }

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new classic_instance_naxxramas_InstanceScript(map);
    }
};

/*######
## at_naxxramas
######*/

class classic_at_naxxramas : public AreaTriggerScript
{
public:
    classic_at_naxxramas() : AreaTriggerScript("classic_at_naxxramas") { }

    bool OnTrigger(Player* pPlayer, AreaTriggerEntry const* pAt) override
    {
        if (!pPlayer->IsAlive())
            return false;

        // Allow GMs to use teleporter
        if (pPlayer->IsGameMaster() && pAt->ID != AREATRIGGER_HUB_TO_FROSTWYRM)
            return false;

        if (classic_instance_naxxramas_InstanceScript* pInstance = GetClassicNaxxInstance(pPlayer))
            pInstance->onNaxxramasAreaTrigger(pPlayer, pAt);

        return false;
    }
};

/*######
## spirit_of_naxxramas_ai
######*/

struct classic_spirit_of_naxxramas_ai : public ScriptedAI
{
    classic_spirit_of_naxxramas_ai(Creature* pCreature) : ScriptedAI(pCreature)
    {
        portalTimer = 5000;
        shadowboltVolleyTimer = 6000;
        me->CastSpell(me, 18950, true); // stealth detection
    }

    ObjectGuid portal;
    uint32 portalTimer;
    uint32 shadowboltVolleyTimer;

    void DespawnPortal()
    {
        if (portal.IsEmpty())
            return;

        if (Creature* pPortal = me->GetMap()->GetCreature(portal))
            pPortal->DespawnOrUnsummon();

        portal.Clear();
    }

    void Reset() override
    {
        portalTimer = 5000;
        shadowboltVolleyTimer = 6000;
        DespawnPortal();
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DespawnPortal();
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        if (portalTimer)
        {
            if (portalTimer < diff)
            {
                // summon portal of shadows
                if (Creature* pCreature = me->SummonCreature(16420, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0.0f,
                    TEMPSUMMON_TIMED_DESPAWN, 60s))
                {
                    // TODO(classic): VMaNGOS me->SendSpellGo(me, 28383) (visual of the manual summon); no TC equivalent
                    portal = pCreature->GetGUID();
                    pCreature->CastSpell(pCreature, 28384, true); // pCreature casts portal of shadow spell on self
                    portalTimer = 0;
                }
            }
            else
                portalTimer -= diff;
        }

        // casting shadowbolt volley every 10 sec
        if (shadowboltVolleyTimer < diff)
        {
            if (!me->IsNonMeleeSpellCast(false) && DoCastSelf(28599) == SPELL_CAST_OK)
                shadowboltVolleyTimer = 10000;
        }
        else
            shadowboltVolleyTimer -= diff;
    }
};

/*######
## naxxramas_gargoyle_ai
######*/

namespace
{
enum ClassicNaxxGargoyle
{
    SPELL_NAXX_INVISIBILITY_AND_STEALTH_DETECTION = 18950,
    SPELL_NAXX_GARGOYLE_STONESKIN                 = 28995, // Periodic Heal and Damage Immunity
    SPELL_NAXX_GARGOYLE_STONEFORM_VISUAL          = 29153, // Dummy Aura
    SPELL_NAXX_GARGOYLE_ACID_VOLLEY               = 29325,

    BCT_NAXX_STRANGE_NOISE                        = 10755, // %s emits a strange noise.

    SPELL_NAXX_SHADOW_MARK                        = 27825
};

ClassicNaxxVector2 const plagueWingEntrancePos[2] =
{
    { 2971.66f, -3481.36f },
    { 2958.53f, -3468.17f }
};
}

struct classic_naxxramas_gargoyle_ai : public ScriptedAI
{
    classic_naxxramas_gargoyle_ai(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_uiAcidVolleyTimer = urand(2800, 6500);
        EnterStoneform();

        if (me->GetDefaultMovementType() == IDLE_MOTION_TYPE && me->GetEntry() == NPC_StoneskinGargoyle)
            me->CastSpell(me, SPELL_NAXX_INVISIBILITY_AND_STEALTH_DETECTION, true);
    }

    void EnterStoneform()
    {
        if (me->GetDefaultMovementType() == IDLE_MOTION_TYPE && me->GetEntry() == NPC_StoneskinGargoyle)
            me->CastSpell(me, SPELL_NAXX_GARGOYLE_STONEFORM_VISUAL, true);
    }

    uint32 m_uiAcidVolleyTimer;

    void Reset() override
    {
        m_uiAcidVolleyTimer = urand(2800, 6500);
    }

    void JustReachedHome() override
    {
        EnterStoneform();
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (me->HasAura(SPELL_NAXX_GARGOYLE_STONEFORM_VISUAL))
        {
            if (pWho->GetTypeId() == TYPEID_PLAYER
                && !me->IsInCombat()
                && me->IsWithinDistInMap(pWho, 17.0f)
                && me->IsWithinLOSInMap(pWho)
                && !pWho->HasAuraType(SPELL_AURA_FEIGN_DEATH)
                && !pWho->HasAuraType(SPELL_AURA_MOD_UNATTACKABLE))
            {
                AttackStart(pWho);
            }
        }
        else
            ScriptedAI::MoveInLineOfSight(pWho);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (me->HasAura(SPELL_NAXX_GARGOYLE_STONEFORM_VISUAL))
            me->RemoveAurasDueToSpell(SPELL_NAXX_GARGOYLE_STONEFORM_VISUAL, ObjectGuid::Empty, 0, AURA_REMOVE_BY_CANCEL);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        // Gargoyles cannot leave the plague wing. Tested on Classic.
        if (!ClassicNaxxIsPointLeftOfLine(plagueWingEntrancePos[0], plagueWingEntrancePos[1], me->GetPosition()))
        {
            EnterEvadeMode();
            return;
        }

        if (me->GetHealthPct() < 30.0f && !me->IsNonMeleeSpellCast(false) && !me->HasAura(SPELL_NAXX_GARGOYLE_STONESKIN))
        {
            if (DoCastSelf(SPELL_NAXX_GARGOYLE_STONESKIN) == SPELL_CAST_OK)
            {
                me->CastSpell(me, SPELL_NAXX_GARGOYLE_STONESKIN, true);
                ClassicScriptText(BCT_NAXX_STRANGE_NOISE, me);
            }
        }

        if (m_uiAcidVolleyTimer < diff && !me->IsNonMeleeSpellCast(false))
        {
            // supposedly the first gargoyle in plague wing did not do the acid volley, so
            // hackfix here to skip him
            if (me->GetSpawnId() != ClassicNaxxCreatureSpawnId(88095))
            {
                if (DoCastSelf(SPELL_NAXX_GARGOYLE_ACID_VOLLEY) == SPELL_CAST_OK) // acid volley
                    m_uiAcidVolleyTimer = 8000;
            }
        }
        else if (m_uiAcidVolleyTimer >= diff)
            m_uiAcidVolleyTimer -= diff;
    }
};

/*######
## naxxramas_plague_slime_ai
######*/

struct classic_naxxramas_plague_slime_ai : public ScriptedAI
{
    classic_naxxramas_plague_slime_ai(Creature* pCreature) : ScriptedAI(pCreature)
    {
        colorChangeTimer = 0;
        prev_spell = 0;
    }

    uint32 colorChangeTimer;
    uint32 prev_spell;

    void ChangeColor()
    {
        uint32 spell = urand(28987, 28990);

        if (SpellInfo const* entry = sSpellMgr->GetSpellInfo(spell, DIFFICULTY_NONE))
            if (uint32 newEntry = uint32(entry->GetEffect(EFFECT_0).MiscValue))
                me->UpdateEntry(newEntry);

        if (prev_spell)
            me->RemoveAurasDueToSpell(prev_spell);

        DoCastSelf(spell, true);
        me->SetObjectScale(2.0f); // updateentry and the actual spells screws up the scale...
        prev_spell = spell;
    }

    void Reset() override
    {
        colorChangeTimer = 0;
        ChangeColor();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        me->CallForHelp(10.0f);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        if (colorChangeTimer < diff)
        {
            colorChangeTimer = urand(9000, 12000); // todo: no idea if timer is correct
            ChangeColor();
        }
        else
            colorChangeTimer -= diff;
    }
};

/*######
## toxic_tunnel_ai
######*/

struct classic_toxic_tunnel_ai : public ScriptedAI
{
    classic_toxic_tunnel_ai(Creature* pCreature) : ScriptedAI(pCreature)
    {
        checktime = 0;
        _evadeTimer = 0;
    }

    uint32 checktime;
    uint32 _evadeTimer;

    void Reset() override
    {
        checktime = 0;
        _evadeTimer = 0;
    }

    void AttackStart(Unit* /*who*/) override { }
    void MoveInLineOfSight(Unit* /*who*/) override { }

    void JustEngagedWith(Unit* /*who*/) override
    {
        // Poison aura is hitting someone. Start a short timer to evade & drop combat
        if (!_evadeTimer)
            _evadeTimer = 5000;
    }

    void UpdateAI(uint32 diff) override
    {
        if (_evadeTimer)
        {
            if (_evadeTimer <= diff)
            {
                EnterEvadeMode();
                _evadeTimer = 0;
            }
            else
                _evadeTimer -= diff;
        }

        // creature_template_addons should make this aura permanent, but check anyway due
        // to some reports of it not recasting
        if (checktime <= diff)
        {
            checktime = 5000;
            if (!me->HasAura(28370))
                me->CastSpell(me, 28370, true);
        }
        else
            checktime -= diff;
    }
};

/*######
## dark_touched_warriorAI
######*/

struct classic_dark_touched_warriorAI : public ScriptedAI
{
    classic_dark_touched_warriorAI(Creature* pCreature) : ScriptedAI(pCreature), hasFled(false) { }

    bool hasFled;

    void Reset() override
    {
        hasFled = false;
    }

    void FleeToHorse()
    {
        if (!me->GetVictim() || me->HasAuraType(SPELL_AURA_PREVENTS_FLEEING))
            return;

        if (Creature* pNearest = me->FindNearestCreature(NPC_DeathchargerSteed, 100.0f, true))
        {
            me->GetMotionMaster()->MoveSeekAssistance(pNearest->GetPositionX(), pNearest->GetPositionY(), pNearest->GetPositionZ());
            me->SetTarget(ObjectGuid::Empty);

            me->UpdateSpeed(MOVE_RUN);
            // VMaNGOS InterruptSpellsWithInterruptFlags(SPELL_INTERRUPT_FLAG_MOVEMENT)
            me->InterruptNonMeleeSpells(false);
        }
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;

        if (!hasFled && me->GetHealthPct() < 50.0f)
        {
            hasFled = true;
            FleeToHorse();
        }
    }
};

/*######
## mob_craftsman_omarion
######*/

namespace
{
enum OmarionMisc
{
    QUEST_OMARIONS_HANDBOOK = 9233,

    BC_TAILOR_TEXT        = 12251, // I am a master tailor, Omarion.
    BC_BLACKSMITH_TEXT    = 12269, // I am a master blacksmith, Omarion.
    BC_LEATHERWORKER_TEXT = 12257, // I am a master leatherworker, Omarion.
    BC_NO_CRAFT_TEXT      = 12279, // Omarion, I am not a craftsman. Can you still help me?
    BC_CLOSE_NO_CRAFTER   = 12281, // Thank you, Omarion. You have taken a fatal blow for the team on this day.
    BC_CLOSE_CRAFTER      = 12270, // I need to go. Evil stirs. Die well, Omarion.

    GOSSIP_MENU_INTRO   = 8507,
    GOSSIP_MENU_CRAFTER = 8508,
    GOSSIP_MENU_NOCRAFT = 8516,

    GOSSIP_OPT_NOT_CRAFTSMAN   = 1,

    GOSSIP_SELECT_TAILOR  = GOSSIP_ACTION_INFO_DEF + 1,
    GOSSIP_SELECT_BS      = GOSSIP_ACTION_INFO_DEF + 2,
    GOSSIP_SELECT_LW      = GOSSIP_ACTION_INFO_DEF + 3,
    GOSSIP_SELECT_NOCRAFT = GOSSIP_ACTION_INFO_DEF + 4,

    GOSSIP_SELECT_CRAFT_BEGIN     = GOSSIP_ACTION_INFO_DEF + 10,

    GOSSIP_SELECT_GLACIAL_GLOVES  = GOSSIP_SELECT_CRAFT_BEGIN + 1,  // tailor honored
    GOSSIP_SELECT_GLACIAL_WRISTS  = GOSSIP_SELECT_CRAFT_BEGIN + 2,  // tailor honored
    GOSSIP_SELECT_GLACIAL_CHEST   = GOSSIP_SELECT_CRAFT_BEGIN + 3,  // tailor exalted
    GOSSIP_SELECT_GLACIAL_CLOAK   = GOSSIP_SELECT_CRAFT_BEGIN + 4,  // tailor exalted

    GOSSIP_SELECT_POLAR_GLOVES    = GOSSIP_SELECT_CRAFT_BEGIN + 5,  // LW honored
    GOSSIP_SELECT_POLAR_WRISTS    = GOSSIP_SELECT_CRAFT_BEGIN + 6, // LW honored
    GOSSIP_SELECT_POLAR_CHEST     = GOSSIP_SELECT_CRAFT_BEGIN + 7, // LW exalted

    GOSSIP_SELECT_ICYSCALE_GLOVES = GOSSIP_SELECT_CRAFT_BEGIN + 8, // LW honored
    GOSSIP_SELECT_ICYSCALE_WRISTS = GOSSIP_SELECT_CRAFT_BEGIN + 9, // LW honored
    GOSSIP_SELECT_ICYSCALE_CHEST  = GOSSIP_SELECT_CRAFT_BEGIN + 10, // LW exalted

    GOSSIP_SELECT_ICEBANE_GLOVES  = GOSSIP_SELECT_CRAFT_BEGIN + 11, // BS exalted
    GOSSIP_SELECT_ICEBANE_WRISTS  = GOSSIP_SELECT_CRAFT_BEGIN + 12, // BS exalted
    GOSSIP_SELECT_ICEBANE_CHEST   = GOSSIP_SELECT_CRAFT_BEGIN + 13, // BS exalted

    GOSSIP_CLOSE = 100,

    ITEM_OMARIONS_HANDBOOK = 22719
};

// VMaNGOS ADD_GOSSIP_ITEM(icon, broadcastTextId, ...): option text from BroadcastText in the player's locale
std::string ClassicNaxxGossipText(Player* player, uint32 broadcastTextId)
{
    if (BroadcastTextEntry const* bct = sBroadcastTextStore.LookupEntry(broadcastTextId))
        return DB2Manager::GetBroadcastTextValue(bct, player->GetSession()->GetSessionDbcLocale(), player->GetGender());
    return "";
}

void ClassicNaxxAddGossip(Player* player, uint32 broadcastTextId, uint32 sender, uint32 action)
{
    AddGossipItemFor(player, GossipOptionNpc::None, ClassicNaxxGossipText(player, broadcastTextId), sender, action);
}

void ClassicNaxxAddGossip(Player* player, char const* text, uint32 sender, uint32 action)
{
    AddGossipItemFor(player, GossipOptionNpc::None, text, sender, action);
}

void LearnCraftIfCan(uint32 learnId, uint32 knowId, Player* pPlayer, ReputationRank minRank, uint32 currSkill)
{
    ReputationRank argentDawnRep = pPlayer->GetReputationRank(FACTION_ID_ARGENT_DAWN_NAXX);
    if (argentDawnRep < minRank)
        return;

    if (currSkill < 300)
        return;

    if (!pPlayer->HasSpell(knowId))
        pPlayer->CastSpell(pPlayer, learnId, false);
}
}

struct classic_mob_craftsman_omarion : public ScriptedAI
{
    classic_mob_craftsman_omarion(Creature* creature) : ScriptedAI(creature) { }

    bool OnGossipHello(Player* pPlayer) override
    {
        ClearGossipMenuFor(pPlayer);

        uint32 tailorSkill      = pPlayer->GetSkillValue(SKILL_TAILORING);
        uint32 blacksmithSkill  = pPlayer->GetSkillValue(SKILL_BLACKSMITHING);
        uint32 leatherworkSkill = pPlayer->GetSkillValue(SKILL_LEATHERWORKING);

        if (tailorSkill >= 225)
            ClassicNaxxAddGossip(pPlayer, BC_TAILOR_TEXT, GOSSIP_SELECT_TAILOR, GOSSIP_SELECT_TAILOR);

        if (blacksmithSkill >= 225)
            ClassicNaxxAddGossip(pPlayer, BC_BLACKSMITH_TEXT, GOSSIP_SELECT_BS, GOSSIP_SELECT_BS);

        if (leatherworkSkill >= 225)
            ClassicNaxxAddGossip(pPlayer, BC_LEATHERWORKER_TEXT, GOSSIP_SELECT_LW, GOSSIP_SELECT_LW);

        ClassicNaxxAddGossip(pPlayer, BC_NO_CRAFT_TEXT, GOSSIP_SENDER_MAIN, GOSSIP_SELECT_NOCRAFT);

        // TODO(classic): npc_text 8507 / 8508 / 8516 must exist in the TC world DB (VMaNGOS npc_text ids)
        SendGossipMenuFor(pPlayer, GOSSIP_MENU_INTRO, me->GetGUID());
        me->HandleEmoteCommand(EMOTE_ONESHOT_LAUGH);
        return true;
    }

    bool OnGossipSelect(Player* pPlayer, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const uiSender = pPlayer->PlayerTalkClass->GetGossipOptionSender(gossipListId);
        uint32 const uiAction = pPlayer->PlayerTalkClass->GetGossipOptionAction(gossipListId);
        ClearGossipMenuFor(pPlayer);

        uint32 tailorSkill      = pPlayer->GetSkillValue(SKILL_TAILORING);
        uint32 blacksmithSkill  = pPlayer->GetSkillValue(SKILL_BLACKSMITHING);
        uint32 leatherworkSkill = pPlayer->GetSkillValue(SKILL_LEATHERWORKING);
        ReputationRank argentDawnRep = pPlayer->GetReputationRank(FACTION_ID_ARGENT_DAWN_NAXX);

        ReputationRank BOOK_REQ_RANK   = REP_REVERED;
        ReputationRank CRACT1_REQ_RANK = REP_REVERED;
        ReputationRank CRAFT2_REQ_RANK = REP_EXALTED;

        if (uiAction == GOSSIP_CLOSE)
        {
            CloseGossipMenuFor(pPlayer);
            return true;
        }

        // if rep < honored, spit on player and be done with it.
        if (argentDawnRep < BOOK_REQ_RANK)
        {
            // DoScriptText(-1999913, pCreature, pPlayer); // spit on player -- Not in sniffs. Need confirmation
            CloseGossipMenuFor(pPlayer);
            return true;
        }

        switch (uiAction)
        {
            case GOSSIP_SELECT_TAILOR:
                if (argentDawnRep >= CRACT1_REQ_RANK)
                {
                    ClassicNaxxAddGossip(pPlayer, "Glacial Gloves", GOSSIP_SELECT_TAILOR, GOSSIP_SELECT_GLACIAL_GLOVES);
                    ClassicNaxxAddGossip(pPlayer, "Glacial Wrists", GOSSIP_SELECT_TAILOR, GOSSIP_SELECT_GLACIAL_WRISTS);
                }
                if (argentDawnRep >= CRAFT2_REQ_RANK)
                {
                    ClassicNaxxAddGossip(pPlayer, "Glacial Vest", GOSSIP_SELECT_TAILOR, GOSSIP_SELECT_GLACIAL_CHEST);
                    ClassicNaxxAddGossip(pPlayer, "Glacial Cloak", GOSSIP_SELECT_TAILOR, GOSSIP_SELECT_GLACIAL_CLOAK);
                }
                ClassicNaxxAddGossip(pPlayer, BC_CLOSE_CRAFTER, GOSSIP_SELECT_TAILOR, GOSSIP_CLOSE);
                SendGossipMenuFor(pPlayer, GOSSIP_MENU_CRAFTER, me->GetGUID());
                return true;
            case GOSSIP_SELECT_BS:
                if (argentDawnRep >= CRACT1_REQ_RANK)
                {
                    ClassicNaxxAddGossip(pPlayer, "Icebane Gauntlets", GOSSIP_SELECT_BS, GOSSIP_SELECT_ICEBANE_GLOVES);
                    ClassicNaxxAddGossip(pPlayer, "Icebane Bracers", GOSSIP_SELECT_BS, GOSSIP_SELECT_ICEBANE_WRISTS);
                }
                if (argentDawnRep >= CRAFT2_REQ_RANK)
                    ClassicNaxxAddGossip(pPlayer, "Icebane Breastplate", GOSSIP_SELECT_BS, GOSSIP_SELECT_ICEBANE_CHEST);
                ClassicNaxxAddGossip(pPlayer, BC_CLOSE_CRAFTER, GOSSIP_SELECT_BS, GOSSIP_CLOSE);
                SendGossipMenuFor(pPlayer, GOSSIP_MENU_CRAFTER, me->GetGUID());
                return true;
            case GOSSIP_SELECT_LW:
                if (argentDawnRep >= CRACT1_REQ_RANK)
                {
                    ClassicNaxxAddGossip(pPlayer, "Polar Gloves", GOSSIP_SELECT_LW, GOSSIP_SELECT_POLAR_GLOVES);
                    ClassicNaxxAddGossip(pPlayer, "Icy Scale Gauntlets", GOSSIP_SELECT_LW, GOSSIP_SELECT_ICYSCALE_GLOVES);

                    ClassicNaxxAddGossip(pPlayer, "Polar Bracers", GOSSIP_SELECT_LW, GOSSIP_SELECT_POLAR_WRISTS);
                    ClassicNaxxAddGossip(pPlayer, "Icy Scale Bracers", GOSSIP_SELECT_LW, GOSSIP_SELECT_ICYSCALE_WRISTS);
                }
                if (argentDawnRep >= CRAFT2_REQ_RANK)
                {
                    ClassicNaxxAddGossip(pPlayer, "Polar Tunic", GOSSIP_SELECT_LW, GOSSIP_SELECT_POLAR_CHEST);
                    ClassicNaxxAddGossip(pPlayer, "Icy Scale Breastplate", GOSSIP_SELECT_LW, GOSSIP_SELECT_ICYSCALE_CHEST);
                }
                ClassicNaxxAddGossip(pPlayer, BC_CLOSE_CRAFTER, GOSSIP_SELECT_LW, GOSSIP_CLOSE);
                SendGossipMenuFor(pPlayer, GOSSIP_MENU_CRAFTER, me->GetGUID());
                return true;
            case GOSSIP_SELECT_NOCRAFT:
            {
                if (argentDawnRep >= BOOK_REQ_RANK)
                {
                    ClassicNaxxAddGossip(pPlayer, BC_CLOSE_NO_CRAFTER, GOSSIP_SENDER_MAIN, GOSSIP_CLOSE);
                    SendGossipMenuFor(pPlayer, GOSSIP_MENU_NOCRAFT, me->GetGUID());
                    if (!pPlayer->HasItemCount(ITEM_OMARIONS_HANDBOOK, 1, true))
                        pPlayer->AddItem(ITEM_OMARIONS_HANDBOOK, 1);
                }
                return true;
            }

            /***************************
            *       Craft spells
            ****************************/

            case GOSSIP_SELECT_GLACIAL_GLOVES:
                LearnCraftIfCan(28212, 28205, pPlayer, CRACT1_REQ_RANK, tailorSkill);
                break;
            case GOSSIP_SELECT_GLACIAL_WRISTS:
                LearnCraftIfCan(28215, 28209, pPlayer, CRACT1_REQ_RANK, tailorSkill);
                break;
            case GOSSIP_SELECT_GLACIAL_CHEST:
                // spell castbar bug, displays as "glacial gloves"
                LearnCraftIfCan(28213, 28207, pPlayer, CRAFT2_REQ_RANK, tailorSkill);
                break;
            case GOSSIP_SELECT_GLACIAL_CLOAK:
                LearnCraftIfCan(28214, 28208, pPlayer, CRAFT2_REQ_RANK, tailorSkill);
                break;
            case GOSSIP_SELECT_POLAR_GLOVES:
                LearnCraftIfCan(28229, 28220, pPlayer, CRACT1_REQ_RANK, leatherworkSkill);
                break;
            case GOSSIP_SELECT_POLAR_WRISTS:
                LearnCraftIfCan(28230, 28221, pPlayer, CRACT1_REQ_RANK, leatherworkSkill);
                break;
            case GOSSIP_SELECT_POLAR_CHEST:
                LearnCraftIfCan(28228, 28219, pPlayer, CRAFT2_REQ_RANK, leatherworkSkill);
                break;
            case GOSSIP_SELECT_ICYSCALE_GLOVES:
                LearnCraftIfCan(28232, 28223, pPlayer, CRACT1_REQ_RANK, leatherworkSkill);
                break;
            case GOSSIP_SELECT_ICYSCALE_WRISTS:
                LearnCraftIfCan(28233, 28224, pPlayer, CRACT1_REQ_RANK, leatherworkSkill);
                break;
            case GOSSIP_SELECT_ICYSCALE_CHEST:
                LearnCraftIfCan(28231, 28222, pPlayer, CRAFT2_REQ_RANK, leatherworkSkill);
                break;
            case GOSSIP_SELECT_ICEBANE_GLOVES:
                LearnCraftIfCan(28248, 28243, pPlayer, CRACT1_REQ_RANK, blacksmithSkill);
                break;
            case GOSSIP_SELECT_ICEBANE_WRISTS:
                LearnCraftIfCan(28249, 28244, pPlayer, CRACT1_REQ_RANK, blacksmithSkill);
                break;
            case GOSSIP_SELECT_ICEBANE_CHEST:
                LearnCraftIfCan(28245, 28242, pPlayer, CRAFT2_REQ_RANK, blacksmithSkill);
                break;
            default:
                break;
        }

        ClassicNaxxAddGossip(pPlayer, BC_CLOSE_CRAFTER, uiSender, GOSSIP_CLOSE);
        SendGossipMenuFor(pPlayer, GOSSIP_MENU_CRAFTER, me->GetGUID());
        return true;
    }
};

/*######
## spell_gargoyle_stoneform (29153 - Gargoyle Stoneform Visual)
######*/

class classic_spell_gargoyle_stoneform : public AuraScript
{
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        // using stand state 9 in sniff
        GetTarget()->SetStandState(UnitStandStateType(9));
        GetTarget()->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->SetStandState(UNIT_STAND_STATE_STAND);
        GetTarget()->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(classic_spell_gargoyle_stoneform::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(classic_spell_gargoyle_stoneform::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

/*######
## spell_unrelenting_rider_shadow_bolt_volley (27831 - Shadow Bolt Volley (Naxx, Unrelenting Rider))
######*/

class classic_spell_unrelenting_rider_shadow_bolt_volley : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        // Shadow Bolt volley which should only target players with the Shadow Mark debuff
        targets.remove_if([](WorldObject* target)
        {
            Unit* unit = target->ToUnit();
            return !unit || !unit->HasAura(SPELL_NAXX_SHADOW_MARK);
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(classic_spell_unrelenting_rider_shadow_bolt_volley::FilterTargets, EFFECT_ALL, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

void AddSC_classic_instance_naxxramas()
{
    new classic_instance_naxxramas();
    new classic_at_naxxramas();
    RegisterCreatureAI(classic_spirit_of_naxxramas_ai);
    RegisterCreatureAI(classic_naxxramas_gargoyle_ai);
    RegisterCreatureAI(classic_naxxramas_plague_slime_ai);
    RegisterCreatureAI(classic_toxic_tunnel_ai);
    RegisterCreatureAI(classic_dark_touched_warriorAI);
    RegisterCreatureAI(classic_mob_craftsman_omarion);
    RegisterSpellScript(classic_spell_gargoyle_stoneform);
    RegisterSpellScript(classic_spell_unrelenting_rider_shadow_bolt_volley);
}
