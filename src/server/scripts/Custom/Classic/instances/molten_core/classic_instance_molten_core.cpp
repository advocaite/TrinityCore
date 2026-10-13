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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/burning_steppes/molten_core/instance_molten_core.cpp (GPL-2)
// Scripts: instance_molten_core, go_rune_MC

#include "ScriptMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureData.h"
#include "DB2Stores.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MapReference.h"
#include "Player.h"
#include "TemporarySummon.h"
#include "classic_boss_pack.h"
#include "classic_molten_core.h"
#include "classic_script_text.h"
#include <array>
#include <list>
#include <vector>

namespace
{
// Rune fire animations (removed when the rune is doused)
enum ClassicMcRuneFire : uint32
{
    CLASSIC_MC_GO_RUNE_FIRE_SULFURON = 178187,
    CLASSIC_MC_GO_RUNE_FIRE_GEDDON   = 178188,
    CLASSIC_MC_GO_RUNE_FIRE_SHAZZRAH = 178189,
    CLASSIC_MC_GO_RUNE_FIRE_GOLEMAGG = 178190,
    CLASSIC_MC_GO_RUNE_FIRE_GARR     = 178191,
    CLASSIC_MC_GO_RUNE_FIRE_MAGMADAR = 178192,
    CLASSIC_MC_GO_RUNE_FIRE_GEHENNAS = 178193
};

// DungeonEncounter.db2 ids used by TC for Molten Core; only registered when present in the client data
struct ClassicMcEncounterId
{
    uint32 BossId;
    uint32 DungeonEncounterId;
};

ClassicMcEncounterId const ClassicMcEncounterIds[] =
{
    { CLASSIC_MC_TYPE_LUCIFRON,  663 },
    { CLASSIC_MC_TYPE_MAGMADAR,  664 },
    { CLASSIC_MC_TYPE_GEHENNAS,  665 },
    { CLASSIC_MC_TYPE_GARR,      666 },
    { CLASSIC_MC_TYPE_SHAZZRAH,  667 },
    { CLASSIC_MC_TYPE_GEDDON,    668 },
    { CLASSIC_MC_TYPE_SULFURON,  669 },
    { CLASSIC_MC_TYPE_GOLEMAGG,  670 },
    { CLASSIC_MC_TYPE_MAJORDOMO, 671 },
    { CLASSIC_MC_TYPE_RAGNAROS,  672 }
};

bool ClassicMcIsBossType(uint32 type)
{
    return type < CLASSIC_MC_MAX_ENCOUNTER;
}
}

class classic_instance_molten_core : public InstanceMapScript
{
public:
    classic_instance_molten_core() : InstanceMapScript(ClassicMoltenCoreScriptName, CLASSIC_MC_MAP_ID) { }

    struct classic_instance_molten_core_InstanceScript : public InstanceScript
    {
        explicit classic_instance_molten_core_InstanceScript(InstanceMap* map) : InstanceScript(map),
            m_rune0(*this, "Rune0", NOT_STARTED), m_rune1(*this, "Rune1", NOT_STARTED), m_rune2(*this, "Rune2", NOT_STARTED),
            m_rune3(*this, "Rune3", NOT_STARTED), m_rune4(*this, "Rune4", NOT_STARTED), m_rune5(*this, "Rune5", NOT_STARTED),
            m_rune6(*this, "Rune6", NOT_STARTED)
        {
            SetHeaders(ClassicMoltenCoreDataHeader);
            SetBossNumber(CLASSIC_MC_MAX_ENCOUNTER);

            std::vector<DungeonEncounterData> encounters;
            for (ClassicMcEncounterId const& enc : ClassicMcEncounterIds)
                if (sDungeonEncounterStore.LookupEntry(enc.DungeonEncounterId))
                    encounters.push_back({ enc.BossId, {{ enc.DungeonEncounterId }} });
            LoadDungeonEncounterData(encounters);

            m_runeStates = { &m_rune0, &m_rune1, &m_rune2, &m_rune3, &m_rune4, &m_rune5, &m_rune6 };
            m_dataDomoSpawned = NOT_STARTED;
        }

        ObjectGuid m_uiLucifronGUID, m_uiMagmadarGUID, m_uiGehennasGUID, m_uiGarrGUID, m_uiGeddonGUID, m_uiShazzrahGUID, m_uiSulfuronGUID, m_uiGolemaggGUID, m_uiMajorDomoGUID, m_uiRagnarosGUID, m_uiFlamewakerPriestGUID;
        ObjectGuid m_uiRuneKoroGUID, m_uiRuneZethGUID, m_uiRuneMazjGUID, m_uiRuneTheriGUID, m_uiRuneBlazGUID, m_uiRuneKressGUID, m_uiRuneMohnGUID, m_uiHotCoalsGUID, m_uiFirelordCacheGUID;

        // VMaNGOS m_RuneSates[7] (saved)
        PersistentInstanceScriptValue<uint32> m_rune0, m_rune1, m_rune2, m_rune3, m_rune4, m_rune5, m_rune6;
        std::array<PersistentInstanceScriptValue<uint32>*, 7> m_runeStates;
        std::array<ObjectGuid, 7> m_GOUseGuidList;
        uint32 m_dataDomoSpawned;

        uint32 RuneState(uint8 idx) const { return uint32(*m_runeStates[idx]); }

        void OnGameObjectCreate(GameObject* pGo) override
        {
            InstanceScript::OnGameObjectCreate(pGo);

            switch (pGo->GetEntry())
            {
                case CLASSIC_MC_RUNE_SULFURON:
                    m_uiRuneKoroGUID = pGo->GetGUID();
                    if (RuneState(0) == DONE)
                        m_GOUseGuidList[0] = pGo->GetGUID();
                    break;
                case CLASSIC_MC_RUNE_GEDDON:
                    m_uiRuneZethGUID = pGo->GetGUID();
                    if (RuneState(1) == DONE)
                        m_GOUseGuidList[1] = pGo->GetGUID();
                    break;
                case CLASSIC_MC_RUNE_SHAZZRAH:
                    m_uiRuneMazjGUID = pGo->GetGUID();
                    if (RuneState(2) == DONE)
                        m_GOUseGuidList[2] = pGo->GetGUID();
                    break;
                case CLASSIC_MC_RUNE_GOLEMAGG:
                    m_uiRuneTheriGUID = pGo->GetGUID();
                    if (RuneState(3) == DONE)
                        m_GOUseGuidList[3] = pGo->GetGUID();
                    break;
                case CLASSIC_MC_RUNE_GARR:
                    m_uiRuneBlazGUID = pGo->GetGUID();
                    if (RuneState(4) == DONE)
                        m_GOUseGuidList[4] = pGo->GetGUID();
                    break;
                case CLASSIC_MC_RUNE_MAGMADAR:
                    m_uiRuneKressGUID = pGo->GetGUID();
                    if (RuneState(5) == DONE)
                        m_GOUseGuidList[5] = pGo->GetGUID();
                    break;
                case CLASSIC_MC_RUNE_GEHENNAS:
                    m_uiRuneMohnGUID = pGo->GetGUID();
                    if (RuneState(6) == DONE)
                        m_GOUseGuidList[6] = pGo->GetGUID();
                    break;
                case CLASSIC_MC_GO_HOT_COALS:
                    m_uiHotCoalsGUID = pGo->GetGUID();
                    pGo->SetFlag(GO_FLAG_IN_USE);
                    break;
                case CLASSIC_MC_RUNE_MAJORDOMO:
                    m_uiFirelordCacheGUID = pGo->GetGUID();
                    break;
                default:
                    break;
            }
        }

        // VMaNGOS swaps Lava Annihilator <-> Firelord randomly on create/respawn (UpdateEntry). TC picks the entry before
        // the creature is created instead.
        uint32 GetCreatureEntry(ObjectGuid::LowType /*spawnId*/, CreatureData const* data) override
        {
            switch (data->id)
            {
                case CLASSIC_MC_NPC_LAVA_ANNIHILATOR:
                    return urand(0, 1) ? uint32(CLASSIC_MC_NPC_FIRELORD) : uint32(CLASSIC_MC_NPC_LAVA_ANNIHILATOR);
                case CLASSIC_MC_NPC_FIRELORD:
                    return urand(0, 1) ? uint32(CLASSIC_MC_NPC_LAVA_ANNIHILATOR) : uint32(CLASSIC_MC_NPC_FIRELORD);
                default:
                    break;
            }
            return data->id;
        }

        // VMaNGOS AddObjectToRemoveList(): trash of a defeated boss disappears whenever it (re)spawns
        static void RemoveTrash(Creature* creature)
        {
            creature->DespawnOrUnsummon(0s, Seconds(7 * DAY));
        }

        // VMaNGOS OnCreatureCreate + OnCreatureRespawn (TC calls OnCreatureCreate for every (re)spawn object)
        void OnCreatureCreate(Creature* pCreature) override
        {
            InstanceScript::OnCreatureCreate(pCreature);

            switch (pCreature->GetEntry())
            {
                case CLASSIC_MC_NPC_LUCIFRON:
                    m_uiLucifronGUID = pCreature->GetGUID();
                    break;
                case CLASSIC_MC_NPC_MAGMADAR:
                    m_uiMagmadarGUID = pCreature->GetGUID();
                    break;
                case CLASSIC_MC_NPC_GEHENNAS:
                    m_uiGehennasGUID = pCreature->GetGUID();
                    break;
                case CLASSIC_MC_NPC_GEDDON:
                    m_uiGeddonGUID = pCreature->GetGUID();
                    break;
                case CLASSIC_MC_NPC_SHAZZRAH:
                    m_uiShazzrahGUID = pCreature->GetGUID();
                    break;
                case CLASSIC_MC_NPC_SULFURON:
                    m_uiSulfuronGUID = pCreature->GetGUID();
                    break;
                case CLASSIC_MC_NPC_GOLEMAGG:
                    m_uiGolemaggGUID = pCreature->GetGUID();
                    break;
                case CLASSIC_MC_NPC_MAJORDOMO:
                    // TODO(classic): VMaNGOS OnCreatureRespawn also marked Majordomo DONE and removed him when he *respawned*;
                    // summoned creatures do not respawn in TC, so only the guid is tracked here.
                    m_uiMajorDomoGUID = pCreature->GetGUID();
                    break;
                case CLASSIC_MC_NPC_RAGNAROS:
                    m_uiRagnarosGUID = pCreature->GetGUID();
                    break;
                case CLASSIC_MC_NPC_FLAMEWAKER_PRIEST:
                    m_uiFlamewakerPriestGUID = pCreature->GetGUID();
                    if (GetBossState(CLASSIC_MC_TYPE_SULFURON) == DONE)
                        RemoveTrash(pCreature);
                    break;
                case CLASSIC_MC_NPC_CORE_RAGER:
                    if (GetBossState(CLASSIC_MC_TYPE_GOLEMAGG) == DONE)
                        RemoveTrash(pCreature);
                    break;
                case CLASSIC_MC_NPC_FLAMEWAKER:
                    if (GetBossState(CLASSIC_MC_TYPE_GEHENNAS) == DONE)
                        RemoveTrash(pCreature);
                    break;
                case CLASSIC_MC_NPC_FLAMEWAKER_PROTECTOR:
                    if (GetBossState(CLASSIC_MC_TYPE_LUCIFRON) == DONE)
                        RemoveTrash(pCreature);
                    break;
                case CLASSIC_MC_NPC_CORE_HOUND:
                case CLASSIC_MC_NPC_ANCIENT_CORE_HOUND:
                    if (GetBossState(CLASSIC_MC_TYPE_MAGMADAR) == DONE)
                        RemoveTrash(pCreature);
                    break;
                case CLASSIC_MC_NPC_GARR:
                    m_uiGarrGUID = pCreature->GetGUID();
                    break;
                case CLASSIC_MC_NPC_FIRESWORN:
                case CLASSIC_MC_NPC_LAVA_SURGER:
                    if (GetBossState(CLASSIC_MC_TYPE_GARR) == DONE)
                        RemoveTrash(pCreature);
                    break;
                case CLASSIC_MC_NPC_LAVA_SPAWN:
                {
                    // Prevent exponential lava spawn creation in case of evade bug
                    std::list<Creature*> lavaSpawnList;
                    pCreature->GetCreatureListWithEntryInGrid(lavaSpawnList, CLASSIC_MC_NPC_LAVA_SPAWN, 100.0f);
                    if (lavaSpawnList.size() > CLASSIC_MC_MAX_LAVA_SPAWNS)
                        pCreature->DespawnOrUnsummon();
                    break;
                }
                default:
                    break;
            }
        }

        void OnUnitDeath(Unit* unit) override
        {
            // Magmadar has no C++ script in VMaNGOS (EventAI sets TYPE_MAGMADAR). Safety net so Majordomo can still be
            // summoned when Magmadar runs on a DB AI that does not set the instance data.
            if (unit->GetEntry() == CLASSIC_MC_NPC_MAGMADAR && unit->GetTypeId() == TYPEID_UNIT)
                SetData(CLASSIC_MC_TYPE_MAGMADAR, DONE);
        }

        // A boss that is not killed resets with his pack: the adds the raid killed during the failed attempt come back.
        void RespawnBossPack(uint32 bossType)
        {
            struct Pack { uint32 type; uint32 boss; std::vector<uint32> adds; };
            static std::vector<Pack> const packs =
            {
                { CLASSIC_MC_TYPE_LUCIFRON,  CLASSIC_MC_NPC_LUCIFRON,  { CLASSIC_MC_NPC_FLAMEWAKER_PROTECTOR } },
                { CLASSIC_MC_TYPE_GEHENNAS,  CLASSIC_MC_NPC_GEHENNAS,  { CLASSIC_MC_NPC_FLAMEWAKER } },
                { CLASSIC_MC_TYPE_GARR,      CLASSIC_MC_NPC_GARR,      { CLASSIC_MC_NPC_FIRESWORN } },
                { CLASSIC_MC_TYPE_SULFURON,  CLASSIC_MC_NPC_SULFURON,  { CLASSIC_MC_NPC_FLAMEWAKER_PRIEST } },
                { CLASSIC_MC_TYPE_GOLEMAGG,  CLASSIC_MC_NPC_GOLEMAGG,  { CLASSIC_MC_NPC_CORE_RAGER } },
                { CLASSIC_MC_TYPE_MAJORDOMO, CLASSIC_MC_NPC_MAJORDOMO, { CLASSIC_MC_NPC_FLAMEWAKER_HEALER, CLASSIC_MC_NPC_FLAMEWAKER_ELITE } },
            };
            Pack const* pack = nullptr;
            for (Pack const& p : packs)
                if (p.type == bossType)
                    pack = &p;
            if (!pack)
                return;

            ClassicRespawnBossPack(instance, pack->boss, pack->adds);
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            if (ClassicMcIsBossType(uiType))
            {
                // TC blocks DONE -> other state transitions
                if (GetBossState(uiType) == DONE && uiData != DONE)
                    return;

                bool const wasFighting = GetBossState(uiType) == IN_PROGRESS;
                SetBossState(uiType, EncounterState(uiData));
                if (wasFighting && (uiData == NOT_STARTED || uiData == FAIL))
                    RespawnBossPack(uiType);

                if (uiType == CLASSIC_MC_TYPE_MAJORDOMO && uiData == DONE)
                    DoRespawnGameObject(m_uiFirelordCacheGUID, Seconds(HOUR));
                return;
            }

            switch (uiType)
            {
                case CLASSIC_MC_DATA_RUNE_ACTIVE_0:
                case CLASSIC_MC_DATA_RUNE_ACTIVE_1:
                case CLASSIC_MC_DATA_RUNE_ACTIVE_2:
                case CLASSIC_MC_DATA_RUNE_ACTIVE_3:
                case CLASSIC_MC_DATA_RUNE_ACTIVE_4:
                case CLASSIC_MC_DATA_RUNE_ACTIVE_5:
                case CLASSIC_MC_DATA_RUNE_ACTIVE_6:
                    *m_runeStates[uiType - CLASSIC_MC_DATA_RUNE_ACTIVE_0] = uiData;
                    break;
                case CLASSIC_MC_DATA_DOMO_SPAWNED:
                    m_dataDomoSpawned = uiData;
                    break;
                default:
                    break;
            }
        }

        uint32 GetData(uint32 uiType) const override
        {
            if (ClassicMcIsBossType(uiType))
                return GetBossState(uiType);

            switch (uiType)
            {
                case CLASSIC_MC_DATA_RUNE_ACTIVE_0:
                case CLASSIC_MC_DATA_RUNE_ACTIVE_1:
                case CLASSIC_MC_DATA_RUNE_ACTIVE_2:
                case CLASSIC_MC_DATA_RUNE_ACTIVE_3:
                case CLASSIC_MC_DATA_RUNE_ACTIVE_4:
                case CLASSIC_MC_DATA_RUNE_ACTIVE_5:
                case CLASSIC_MC_DATA_RUNE_ACTIVE_6:
                    return RuneState(uint8(uiType - CLASSIC_MC_DATA_RUNE_ACTIVE_0));
                case CLASSIC_MC_DATA_DOMO_SPAWNED:
                    return m_dataDomoSpawned;
                default:
                    break;
            }

            return 0;
        }

        ObjectGuid GetGuidData(uint32 uiData) const override
        {
            switch (uiData)
            {
                case CLASSIC_MC_DATA_SULFURON:
                    return m_uiSulfuronGUID;
                case CLASSIC_MC_DATA_GOLEMAGG:
                    return m_uiGolemaggGUID;
                case CLASSIC_MC_DATA_GARR:
                    return m_uiGarrGUID;
                case CLASSIC_MC_DATA_MAJORDOMO:
                    return m_uiMajorDomoGUID;
                default:
                    break;
            }

            return ObjectGuid::Empty;
        }

        void RemoveRuneFire(GameObject* pRune, Unit* pUser, uint32 goEntry, ObjectGuid& guid)
        {
            if (GameObject* pGoFireAnim = pRune->FindNearestGameObject(goEntry, 20.f))
            {
                pGoFireAnim->Delete();
                pRune->Use(pUser);
                guid.Clear();
            }
        }

        void Update(uint32 /*uiDiff*/) override
        {
            // VMaNGOS RemoveAllObjectsInRemoveList(): TC map processes its remove list itself

            Player* firstPlayer = nullptr;
            for (MapReference const& ref : instance->GetPlayers())
            {
                if (Player* player = ref.GetSource())
                {
                    firstPlayer = player;
                    break;
                }
            }

            if (!firstPlayer)
                return;

            for (ObjectGuid& guid : m_GOUseGuidList)
            {
                if (guid.IsEmpty())
                    continue;

                GameObject* pRune = instance->GetGameObject(guid);
                if (!pRune)
                    continue;

                switch (pRune->GetEntry())
                {
                    case CLASSIC_MC_RUNE_SULFURON: RemoveRuneFire(pRune, firstPlayer, CLASSIC_MC_GO_RUNE_FIRE_SULFURON, guid); break;
                    case CLASSIC_MC_RUNE_GEDDON:   RemoveRuneFire(pRune, firstPlayer, CLASSIC_MC_GO_RUNE_FIRE_GEDDON, guid); break;
                    case CLASSIC_MC_RUNE_SHAZZRAH: RemoveRuneFire(pRune, firstPlayer, CLASSIC_MC_GO_RUNE_FIRE_SHAZZRAH, guid); break;
                    case CLASSIC_MC_RUNE_GOLEMAGG: RemoveRuneFire(pRune, firstPlayer, CLASSIC_MC_GO_RUNE_FIRE_GOLEMAGG, guid); break;
                    case CLASSIC_MC_RUNE_GARR:     RemoveRuneFire(pRune, firstPlayer, CLASSIC_MC_GO_RUNE_FIRE_GARR, guid); break;
                    case CLASSIC_MC_RUNE_MAGMADAR: RemoveRuneFire(pRune, firstPlayer, CLASSIC_MC_GO_RUNE_FIRE_MAGMADAR, guid); break;
                    case CLASSIC_MC_RUNE_GEHENNAS: RemoveRuneFire(pRune, firstPlayer, CLASSIC_MC_GO_RUNE_FIRE_GEHENNAS, guid); break;
                    default: break;
                }
            }
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new classic_instance_molten_core_InstanceScript(map);
    }
};

/*######
## go_rune_MC
######*/

struct classic_go_rune_MC : public GameObjectAI
{
    classic_go_rune_MC(GameObject* go) : GameObjectAI(go) { }

    static bool UpdateRune(InstanceScript* pInstance, GameObject* pGo, uint32 typeBoss, uint32 typeRune, uint32 objectEntry)
    {
        if (pInstance->GetData(typeBoss) == DONE)
        {
            if (pInstance->GetData(typeRune) != DONE)
                pInstance->SetData(typeRune, DONE);

            if (GameObject* rune = pGo->FindNearestGameObject(objectEntry, 20.f))
                rune->Delete();

            return true; // Updated!
        }
        return false; // Not Updated!
    }

    bool OnGossipHello(Player* pPlayer) override
    {
        InstanceScript* pInstance = me->GetInstanceScript();
        if (!pInstance)
            return false;

        switch (me->GetEntry())
        {
            case CLASSIC_MC_RUNE_SULFURON:
                if (!UpdateRune(pInstance, me, CLASSIC_MC_TYPE_SULFURON, CLASSIC_MC_DATA_RUNE_ACTIVE_0, CLASSIC_MC_GO_RUNE_FIRE_SULFURON))
                    return true;
                break;
            case CLASSIC_MC_RUNE_GEDDON:
                if (!UpdateRune(pInstance, me, CLASSIC_MC_TYPE_GEDDON, CLASSIC_MC_DATA_RUNE_ACTIVE_1, CLASSIC_MC_GO_RUNE_FIRE_GEDDON))
                    return true;
                break;
            case CLASSIC_MC_RUNE_SHAZZRAH:
                if (!UpdateRune(pInstance, me, CLASSIC_MC_TYPE_SHAZZRAH, CLASSIC_MC_DATA_RUNE_ACTIVE_2, CLASSIC_MC_GO_RUNE_FIRE_SHAZZRAH))
                    return true;
                break;
            case CLASSIC_MC_RUNE_GOLEMAGG:
                if (!UpdateRune(pInstance, me, CLASSIC_MC_TYPE_GOLEMAGG, CLASSIC_MC_DATA_RUNE_ACTIVE_3, CLASSIC_MC_GO_RUNE_FIRE_GOLEMAGG))
                    return true;
                break;
            case CLASSIC_MC_RUNE_GARR:
                if (!UpdateRune(pInstance, me, CLASSIC_MC_TYPE_GARR, CLASSIC_MC_DATA_RUNE_ACTIVE_4, CLASSIC_MC_GO_RUNE_FIRE_GARR))
                    return true;
                break;
            case CLASSIC_MC_RUNE_MAGMADAR:
                if (!UpdateRune(pInstance, me, CLASSIC_MC_TYPE_MAGMADAR, CLASSIC_MC_DATA_RUNE_ACTIVE_5, CLASSIC_MC_GO_RUNE_FIRE_MAGMADAR))
                    return true;
                break;
            case CLASSIC_MC_RUNE_GEHENNAS:
                if (!UpdateRune(pInstance, me, CLASSIC_MC_TYPE_GEHENNAS, CLASSIC_MC_DATA_RUNE_ACTIVE_6, CLASSIC_MC_GO_RUNE_FIRE_GEHENNAS))
                    return true;
                break;
            default:
                break;
        }

        if (pInstance->GetData(CLASSIC_MC_TYPE_RAGNAROS) == DONE ||
            pInstance->GetData(CLASSIC_MC_DATA_DOMO_SPAWNED) == DONE)
            return false;

        for (uint32 rune = CLASSIC_MC_DATA_RUNE_ACTIVE_0; rune <= CLASSIC_MC_DATA_RUNE_ACTIVE_6; ++rune)
            if (pInstance->GetData(rune) != DONE)
                return false;

        pInstance->SetData(CLASSIC_MC_DATA_DOMO_SPAWNED, DONE);

        // Summon Majordomo
        if (pInstance->GetData(CLASSIC_MC_TYPE_MAJORDOMO) != DONE)
        {
            // Default case
            if (Creature* pMajorDomo = pPlayer->SummonCreature(CLASSIC_MC_NPC_MAJORDOMO, 758.089f, -1176.71f, -118.640f, 3.12414f, TEMPSUMMON_MANUAL_DESPAWN, Milliseconds(2 * HOUR * IN_MILLISECONDS)))
                ClassicScriptText(CLASSIC_MC_SAY_RUNES_DESTROYED, pMajorDomo);
        }
        else
        {
            // Server crash/shutdown during Ragnaros encounter.
            // Summon Majordomo in Ragnaros chamber and add gossip to resummon Ragnaros.
            if (Creature* pMajorDomo = pPlayer->SummonCreature(CLASSIC_MC_NPC_MAJORDOMO, 847.103f, -816.153f, -229.775f, 4.344f, TEMPSUMMON_TIMED_DESPAWN, Milliseconds(2 * HOUR * IN_MILLISECONDS)))
            {
                // VMaNGOS: UNIT_FIELD_FLAGS = PET_RENAME | IMMUNE_TO_PLAYER | IN_COMBAT
                pMajorDomo->SetImmuneToPC(true);
                pMajorDomo->SetFaction(CLASSIC_MC_FACTION_DOMO_FRIENDLY);
                pMajorDomo->CombatStop();
                pMajorDomo->ReplaceAllNpcFlags(UNIT_NPC_FLAG_GOSSIP);
            }
        }

        return false;
    }
};

void AddSC_classic_instance_molten_core()
{
    new classic_instance_molten_core();
    RegisterGameObjectAI(classic_go_rune_MC);
}
