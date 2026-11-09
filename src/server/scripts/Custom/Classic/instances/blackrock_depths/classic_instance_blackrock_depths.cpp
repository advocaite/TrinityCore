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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/burning_steppes/blackrock_depths/instance_blackrock_depths.cpp
// (ScriptDev2 lineage, GPL-2)

#include "ScriptMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "TemporarySummon.h"
#include "classic_blackrock_depths.h"
#include "classic_dungeon_encounter_tracker.h"
#include "classic_script_text.h"
#include <array>
#include <list>
#include <memory>
#include <vector>

namespace
{
// Random emotes for Grim Guzzler patrons
Emote const BrdPatronsEmotes[] =
{
    EMOTE_ONESHOT_EXCLAMATION, EMOTE_ONESHOT_CHEER, EMOTE_ONESHOT_CHEER, EMOTE_ONESHOT_LAUGH, EMOTE_ONESHOT_LAUGH, EMOTE_ONESHOT_LAUGH
};

// Used to summon the patrol in Grim Guzzler
float const BrdBarPatrolPositions[2][4] =
{
    { 872.7059f, -232.5491f, -43.7525f, 2.069044f },
    { 865.5645f, -219.7471f, -43.7033f, 2.033881f }
};

uint32 const BrdBarPatrolId[3] = { NPC_FIREGUARD_DESTROYER, NPC_ANVILRAGE_OFFICER, NPC_ANVILRAGE_OFFICER };

struct BrdArenaCylinder
{
    float CenterX;
    float CenterY;
    float CenterZ;
    uint32 Radius;
    uint32 Height;
};

BrdArenaCylinder const BrdArenaCrowdVolume = { 595.78f, -188.65f, -38.63f, 69, 10 };

// Keys of the persisted VMaNGOS m_auiEncounter[0..19] values (VMaNGOS saved them as a space separated string)
char const* const BrdSaveKeys[BRD_SAVED_ENCOUNTERS] =
{
    "RingOfLaw", "Vault", "Rocknot", "TombOfSeven", "Lyceum", "IronHall", "Thunderbrew", "RelicCoffer", "Doomgrip", "Ribbly",
    "ArgelmachAggro", "Patrol", "Theldren", "Nagmara", "Bridge", "Plugger", "QuestJailBreak", "JailDughal", "JailSupplyRoom", "JailTobias"
};
}

namespace
{
// Classic 1.60: TC boss states for the DungeonEncounter.db2 encounters of Blackrock Depths (map 230), driven by
// ClassicDungeonEncounterTracker (boss kill credit, encounter start / end, instance lock)
enum ClassicBlackrockDepthsEncounterBosses : uint32
{
    CBRD_ENC_GERSTAHN = 0,
    CBRD_ENC_ROCCOR = 1,
    CBRD_ENC_GREBMAR = 2,
    CBRD_ENC_RING_OF_LAW = 3,
    CBRD_ENC_LOREGRAIN = 4,
    CBRD_ENC_INCENDIUS = 5,
    CBRD_ENC_STILGISS = 6,
    CBRD_ENC_FINEOUS = 7,
    CBRD_ENC_BAELGAR = 8,
    CBRD_ENC_ANGERFORGE = 9,
    CBRD_ENC_ARGELMACH = 10,
    CBRD_ENC_HURLEY = 11,
    CBRD_ENC_PHALANX = 12,
    CBRD_ENC_RIBBLY = 13,
    CBRD_ENC_PLUGGER = 14,
    CBRD_ENC_VAULT = 15,
    CBRD_ENC_FLAMELASH = 16,
    CBRD_ENC_SEVEN = 17,
    CBRD_ENC_MAGMUS = 18,
    CBRD_ENC_MOIRA = 19,
    CBRD_ENC_THAURISSAN = 20,
    CBRD_ENC_MAX
};

constexpr ClassicDungeonBossEntry ClassicBlackrockDepthsEncounterBossEntries[] =
{
    { CBRD_ENC_GERSTAHN, 9018 }, // High Interrogator Gerstahn
    { CBRD_ENC_ROCCOR, 9025 }, // Lord Roccor
    { CBRD_ENC_GREBMAR, 9319 }, // Houndmaster Grebmar
    { CBRD_ENC_RING_OF_LAW, 9027 }, // Gorosh the Dervish
    { CBRD_ENC_RING_OF_LAW, 9028 }, // Grizzle
    { CBRD_ENC_RING_OF_LAW, 9029 }, // Eviscerator
    { CBRD_ENC_RING_OF_LAW, 9030 }, // Ok'thor the Breaker
    { CBRD_ENC_RING_OF_LAW, 9031 }, // Anub'shiah
    { CBRD_ENC_RING_OF_LAW, 9032 }, // Hedrum the Creeper
    { CBRD_ENC_LOREGRAIN, 9024 }, // Pyromancer Loregrain
    { CBRD_ENC_INCENDIUS, 9017 }, // Lord Incendius
    { CBRD_ENC_STILGISS, 9041 }, // Warder Stilgiss
    { CBRD_ENC_STILGISS, 9042, CLASSIC_DUNGEON_BOSS_ENGAGE_ONLY }, // Verek
    { CBRD_ENC_FINEOUS, 9056 }, // Fineous Darkvire
    { CBRD_ENC_BAELGAR, 9016 }, // Bael'Gar
    { CBRD_ENC_ANGERFORGE, 9033 }, // General Angerforge
    { CBRD_ENC_ARGELMACH, 8983 }, // Golem Lord Argelmach
    { CBRD_ENC_HURLEY, 9537 }, // Hurley Blackbreath
    { CBRD_ENC_PHALANX, 9502 }, // Phalanx
    { CBRD_ENC_RIBBLY, 9543 }, // Ribbly Screwspigot
    { CBRD_ENC_PLUGGER, 9499 }, // Plugger Spazzring
    { CBRD_ENC_VAULT, 9476 }, // Watchman Doomgrip (The Vault)
    { CBRD_ENC_FLAMELASH, 9156 }, // Ambassador Flamelash
    { CBRD_ENC_SEVEN, 9034 }, // Hate'rel
    { CBRD_ENC_SEVEN, 9035 }, // Anger'rel
    { CBRD_ENC_SEVEN, 9036 }, // Vile'rel
    { CBRD_ENC_SEVEN, 9037 }, // Gloom'rel
    { CBRD_ENC_SEVEN, 9038 }, // Seeth'rel
    { CBRD_ENC_SEVEN, 9039 }, // Doom'rel
    { CBRD_ENC_SEVEN, 9040 }, // Dope'rel
    { CBRD_ENC_MAGMUS, 9938 }, // Magmus
    { CBRD_ENC_MOIRA, 8929 }, // Princess Moira Bronzebeard
    { CBRD_ENC_THAURISSAN, 9019 } // Emperor Dagran Thaurissan
};
}

class classic_instance_blackrock_depths : public InstanceMapScript
{
public:
    classic_instance_blackrock_depths() : InstanceMapScript(ClassicBRDScriptName, BRD_MAP_ID) { }

    struct classic_instance_blackrock_depths_InstanceScript : public InstanceScript
    {
        classic_instance_blackrock_depths_InstanceScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders("BRDC");
            SetBossNumber(CBRD_ENC_MAX);
            static constexpr DungeonEncounterData encounters[] =
            {
                { CBRD_ENC_GERSTAHN, {{ 227 }} },
                { CBRD_ENC_ROCCOR, {{ 228 }} },
                { CBRD_ENC_GREBMAR, {{ 229 }} },
                { CBRD_ENC_RING_OF_LAW, {{ 230 }} },
                { CBRD_ENC_LOREGRAIN, {{ 231 }} },
                { CBRD_ENC_INCENDIUS, {{ 232 }} },
                { CBRD_ENC_STILGISS, {{ 233 }} },
                { CBRD_ENC_FINEOUS, {{ 234 }} },
                { CBRD_ENC_BAELGAR, {{ 235 }} },
                { CBRD_ENC_ANGERFORGE, {{ 236 }} },
                { CBRD_ENC_ARGELMACH, {{ 237 }} },
                { CBRD_ENC_HURLEY, {{ 238 }} },
                { CBRD_ENC_PHALANX, {{ 239 }} },
                { CBRD_ENC_RIBBLY, {{ 240 }} },
                { CBRD_ENC_PLUGGER, {{ 241 }} },
                { CBRD_ENC_VAULT, {{ 2791 }} },
                { CBRD_ENC_FLAMELASH, {{ 242 }} },
                { CBRD_ENC_SEVEN, {{ 243 }} },
                { CBRD_ENC_MAGMUS, {{ 244 }} },
                { CBRD_ENC_MOIRA, {{ 2789 }} },
                { CBRD_ENC_THAURISSAN, {{ 2790 }} }
            };
            LoadDungeonEncounterData(encounters);

            for (uint32 i = 0; i < BRD_SAVED_ENCOUNTERS; ++i)
                _savedEncounter[i] = std::make_unique<PersistentInstanceScriptValue<uint32>>(*this, BrdSaveKeys[i], uint32(NOT_STARTED));

            Initialize();
        }

        ClassicDungeonEncounterTracker _encounterTracker{ *this, ClassicBlackrockDepthsEncounterBossEntries };

        void OnCreatureRemove(Creature* creature) override
        {
            InstanceScript::OnCreatureRemove(creature);
            _encounterTracker.OnCreatureRemove(creature);
        }

        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::array<std::unique_ptr<PersistentInstanceScriptValue<uint32>>, BRD_SAVED_ENCOUNTERS> _savedEncounter;

        ObjectGuid m_uiEmperorGUID;
        ObjectGuid m_uiPrincessGUID;
        ObjectGuid m_uiPhalanxGUID;
        ObjectGuid m_uiHaterelGUID;
        ObjectGuid m_uiAngerrelGUID;
        ObjectGuid m_uiVilerelGUID;
        ObjectGuid m_uiGloomrelGUID;
        ObjectGuid m_uiSeethrelGUID;
        ObjectGuid m_uiDoomrelGUID;
        ObjectGuid m_uiDoperelGUID;

        ObjectGuid m_uiTheldrenGUID;
        ObjectGuid m_uiGrimstoneGUID;
        ObjectGuid m_uiChallengerPlayerGUID;
        ObjectGuid m_uiArenaSpoilsGUID;

        ObjectGuid m_uiGoArena1GUID;
        ObjectGuid m_uiGoArena2GUID;
        ObjectGuid m_uiGoArena3GUID;
        ObjectGuid m_uiGoArena4GUID;
        ObjectGuid m_uiGoShadowLockGUID;
        ObjectGuid m_uiGoShadowMechGUID;
        ObjectGuid m_uiGoShadowGiantGUID;
        ObjectGuid m_uiGoShadowDummyGUID;
        ObjectGuid m_uiGoBarKegGUID;
        ObjectGuid m_uiGoBarKegTrapGUID;
        ObjectGuid m_uiGoBarDoorGUID;
        ObjectGuid m_uiGoTombEnterGUID;
        ObjectGuid m_uiGoTombExitGUID;
        ObjectGuid m_uiGoLyceumGUID;
        ObjectGuid m_uiGoGolemNGUID;
        ObjectGuid m_uiGoGolemSGUID;
        ObjectGuid m_uiGoThroneGUID;

        ObjectGuid m_uiDwarfRuneA01GUID;
        ObjectGuid m_uiDwarfRuneB01GUID;
        ObjectGuid m_uiDwarfRuneC01GUID;
        ObjectGuid m_uiDwarfRuneD01GUID;
        ObjectGuid m_uiDwarfRuneE01GUID;
        ObjectGuid m_uiDwarfRuneF01GUID;
        ObjectGuid m_uiDwarfRuneG01GUID;
        ObjectGuid m_uiFlamelashGUID;
        uint32 m_uiSpiritTimer[DWARF_RUNES_MAX];
        std::list<ObjectGuid> m_burningSpirits;

        ObjectGuid m_uiMagmusGUID;

        ObjectGuid m_uiRocknotGUID;
        ObjectGuid m_uiNagmaraGUID;

        ObjectGuid m_uiGolemLordArgelmachGUID;
        ObjectGuid m_uiPluggerSpazzringGUID;

        ObjectGuid m_uiSpectralChaliceGUID;
        ObjectGuid m_uiSevensChestGUID;
        ObjectGuid m_uiGoSecretDoorGUID;

        uint32 m_uiBarAleCount;
        uint32 m_uiThunderbrewCount;
        uint32 m_uiRelicCofferDoorCount;

        std::vector<ObjectGuid> m_lRibblySCronyMobGUIDList;
        std::vector<ObjectGuid> m_lArenaSpectatorMobGUIDList;
        std::vector<ObjectGuid> m_lArgelmachProtectorsMobGUIDList;
        std::vector<ObjectGuid> m_sBarPatronNpcGuids;
        std::vector<ObjectGuid> m_sBarPatrolGuids;

        bool m_bDoorDughalOpened;
        bool m_bDoorTobiasOpened;
        bool m_bDoorCrestOpened;
        bool m_bDoorJazOpened;
        bool m_bDoorShillOpened;
        bool m_bDoorSupplyOpened;

        bool m_bIsTheldrenInvocated;
        bool m_bBarHostile;

        uint8 m_uiStolenAles;
        uint32 m_uiDagranTimer;
        uint32 m_uiPatronEmoteTimer;
        uint32 m_uiPatrolTimer;

        ObjectGuid m_uiOgrabisiGUID;
        ObjectGuid m_uiShillGUID;
        ObjectGuid m_uiCrestGUID;
        ObjectGuid m_uiJazGUID;
        ObjectGuid m_uiGoJailSupplyRoomGUID;
        ObjectGuid m_uiGoJailSupplyCrateGUID;

        void Initialize()
        {
            for (uint32& i : m_auiEncounter)
                i = NOT_STARTED;

            for (uint32& i : m_uiSpiritTimer)
                i = 5 * IN_MILLISECONDS;

            m_uiThunderbrewCount = 0;
            m_uiRelicCofferDoorCount = 0;
            m_bIsTheldrenInvocated = false;

            m_uiBarAleCount = 0;
            m_uiStolenAles = 0;

            m_uiDagranTimer = 0;
            m_uiPatronEmoteTimer = 2000;
            m_uiPatrolTimer = 0;

            m_bBarHostile = false;

            m_bDoorDughalOpened = false;
            m_bDoorTobiasOpened = false;
            m_bDoorCrestOpened = false;
            m_bDoorJazOpened = false;
            m_bDoorShillOpened = false;
            m_bDoorSupplyOpened = false;
        }

        // VMaNGOS Load(): restore the saved values, IN_PROGRESS -> NOT_STARTED
        void AfterDataLoad() override
        {
            for (uint32 i = 0; i < BRD_SAVED_ENCOUNTERS; ++i)
            {
                uint32 value = *_savedEncounter[i];
                if (value == IN_PROGRESS)
                    value = NOT_STARTED;
                m_auiEncounter[i] = value;
            }
        }

        // VMaNGOS SetData(): the whole m_auiEncounter[0..19] string is saved whenever some data is set to DONE
        void SaveEncounterData()
        {
            for (uint32 i = 0; i < BRD_SAVED_ENCOUNTERS; ++i)
                if (uint32(*_savedEncounter[i]) != m_auiEncounter[i])
                    *_savedEncounter[i] = m_auiEncounter[i];
        }

        void DoOpenDoor(ObjectGuid guid)
        {
            if (!guid)
                return;

            if (GameObject* go = instance->GetGameObject(guid))
            {
                if (go->GetGoType() == GAMEOBJECT_TYPE_DOOR || go->GetGoType() == GAMEOBJECT_TYPE_BUTTON)
                {
                    if (go->getLootState() == GO_READY)
                        go->UseDoorOrButton(0, false);
                }
                else
                    TC_LOG_ERROR("scripts", "classic_instance_blackrock_depths: DoOpenDoor, but gameobject entry {} is type {}.", go->GetEntry(), go->GetGoType());
            }
        }

        void DoResetDoor(ObjectGuid guid)
        {
            if (!guid)
                return;

            if (GameObject* go = instance->GetGameObject(guid))
            {
                if (go->GetGoType() == GAMEOBJECT_TYPE_DOOR || go->GetGoType() == GAMEOBJECT_TYPE_BUTTON)
                    go->ResetDoorOrButton();
                else
                    TC_LOG_ERROR("scripts", "classic_instance_blackrock_depths: DoResetDoor, but gameobject entry {} is type {}.", go->GetEntry(), go->GetGoType());
            }
        }

        void OnCreatureCreate(Creature* creature) override
        {
            InstanceScript::OnCreatureCreate(creature);
            _encounterTracker.OnCreatureCreate(creature);

            switch (creature->GetEntry())
            {
                case NPC_EMPEROR:
                    m_uiEmperorGUID = creature->GetGUID();
                    break;
                case NPC_PRINCESS:
                    m_uiPrincessGUID = creature->GetGUID();
                    break;
                case NPC_PHALANX:
                    m_uiPhalanxGUID = creature->GetGUID();
                    break;
                case NPC_HATEREL:
                    m_uiHaterelGUID = creature->GetGUID();
                    break;
                case NPC_ANGERREL:
                    m_uiAngerrelGUID = creature->GetGUID();
                    break;
                case NPC_VILEREL:
                    m_uiVilerelGUID = creature->GetGUID();
                    break;
                case NPC_GLOOMREL:
                    m_uiGloomrelGUID = creature->GetGUID();
                    break;
                case NPC_SEETHREL:
                    m_uiSeethrelGUID = creature->GetGUID();
                    break;
                case NPC_DOOMREL:
                    m_uiDoomrelGUID = creature->GetGUID();
                    break;
                case NPC_DOPEREL:
                    m_uiDoperelGUID = creature->GetGUID();
                    break;
                case NPC_THELDREN:
                    m_uiTheldrenGUID = creature->GetGUID();
                    break;
                case NPC_RIBBLY_S_CRONY:
                    m_lRibblySCronyMobGUIDList.push_back(creature->GetGUID());
                    break;
                case NPC_MAGMUS:
                    m_uiMagmusGUID = creature->GetGUID();
                    break;
                // Arena Crowd
                case NPC_ARENA_SPECTATOR:
                case NPC_SHADOWFORGE_PEASANT:
                case NPC_SHADOWFORGE_CITIZEN:
                case NPC_SHADOWFORGE_SENATOR:
                case NPC_ANVILRAGE_SOLDIER:
                case NPC_ANVILRAGE_MEDIC:
                case NPC_ANVILRAGE_OFFICER:
                    if (creature->GetPositionZ() < BrdArenaCrowdVolume.CenterZ || creature->GetPositionZ() > BrdArenaCrowdVolume.CenterZ + BrdArenaCrowdVolume.Height ||
                        !creature->IsWithinDist2d(BrdArenaCrowdVolume.CenterX, BrdArenaCrowdVolume.CenterY, float(BrdArenaCrowdVolume.Radius)))
                        break;
                    m_lArenaSpectatorMobGUIDList.push_back(creature->GetGUID());
                    if (m_auiEncounter[TYPE_RING_OF_LAW] == DONE)
                        creature->SetFaction(BRD_FACTION_ARENA_NEUTRAL);    // VMaNGOS SetFactionTemporary(.., TEMPFACTION_RESTORE_RESPAWN)
                    break;
                case NPC_WRATH_HAMMER_CONSTRUCT:
                case NPC_GOLEM_RAVAGE:
                    m_lArgelmachProtectorsMobGUIDList.push_back(creature->GetGUID());
                    break;
                case NPC_GOLEM_LORD_ARGELMACH:
                    m_uiGolemLordArgelmachGUID = creature->GetGUID();
                    break;
                case NPC_PLUGGER_SPAZZRING:
                    m_uiPluggerSpazzringGUID = creature->GetGUID();
                    break;
                case NPC_GUZZLING_PATRON:
                case NPC_GRIM_PATRON:
                case NPC_HAMMERED_PATRON:
                    m_sBarPatronNpcGuids.push_back(creature->GetGUID());
                    if (GetData(TYPE_PLUGGER) == DONE)
                    {
                        creature->SetFaction(BRD_FACTION_DARK_IRON);        // VMaNGOS SetFactionTemporary(.., TEMPFACTION_RESTORE_RESPAWN)
                        creature->SetStandState(UNIT_STAND_STATE_STAND);
                        m_bBarHostile = true;
                    }
                    break;
                case NPC_PRIVATE_ROCKNOT:
                    m_uiRocknotGUID = creature->GetGUID();
                    break;
                case NPC_MISTRESS_NAGMARA:
                    m_uiNagmaraGUID = creature->GetGUID();
                    break;
                case NPC_OGRABISI:
                    m_uiOgrabisiGUID = creature->GetGUID();
                    break;
                case NPC_SHILL:
                    m_uiShillGUID = creature->GetGUID();
                    break;
                case NPC_CREST:
                    m_uiCrestGUID = creature->GetGUID();
                    break;
                case NPC_JAZ:
                    m_uiJazGUID = creature->GetGUID();
                    break;
                case NPC_GRIMSTONE:
                    m_uiGrimstoneGUID = creature->GetGUID();
                    break;
                case NPC_FLAMELASH:
                    m_uiFlamelashGUID = creature->GetGUID();
                    break;
            }
        }

        void OnGameObjectCreate(GameObject* go) override
        {
            InstanceScript::OnGameObjectCreate(go);

            switch (go->GetEntry())
            {
                case GO_ARENA1:
                    m_uiGoArena1GUID = go->GetGUID();
                    break;
                case GO_ARENA2:
                    m_uiGoArena2GUID = go->GetGUID();
                    break;
                case GO_ARENA3:
                    m_uiGoArena3GUID = go->GetGUID();
                    // Re-open the door for saved instance after restart
                    if (GetData(TYPE_RING_OF_LAW) == DONE)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                case GO_ARENA4:
                    m_uiGoArena4GUID = go->GetGUID();
                    break;
                case GO_SHADOW_LOCK:
                    m_uiGoShadowLockGUID = go->GetGUID();
                    break;
                case GO_SHADOW_MECHANISM:
                    m_uiGoShadowMechGUID = go->GetGUID();
                    break;
                case GO_SHADOW_GIANT_DOOR:
                    m_uiGoShadowGiantGUID = go->GetGUID();
                    break;
                case GO_SHADOW_DUMMY:
                    m_uiGoShadowDummyGUID = go->GetGUID();
                    break;
                case GO_BAR_KEG_SHOT:
                    m_uiGoBarKegGUID = go->GetGUID();
                    break;
                case GO_BAR_KEG_TRAP:
                    m_uiGoBarKegTrapGUID = go->GetGUID();
                    break;
                case GO_BAR_DOOR:
                    m_uiGoBarDoorGUID = go->GetGUID();
                    if (GetData(TYPE_ROCKNOT) == DONE)
                        go->SetGoState(GO_STATE_DESTROYED);         // VMaNGOS GOState(2) (GO_STATE_ACTIVE_ALTERNATIVE)
                    if (GetData(TYPE_NAGMARA) == DONE || GetData(TYPE_PLUGGER) == DONE)
                        go->SetGoState(GO_STATE_ACTIVE);            // VMaNGOS GOState(0)
                    break;
                case GO_TOMB_ENTER:
                    m_uiGoTombEnterGUID = go->GetGUID();
                    break;
                case GO_TOMB_EXIT:
                    m_uiGoTombExitGUID = go->GetGUID();
                    if (GetData(TYPE_TOMB_OF_SEVEN) == DONE)
                        go->UseDoorOrButton();
                    break;
                case GO_LYCEUM:
                    m_uiGoLyceumGUID = go->GetGUID();
                    break;
                case GO_GOLEM_ROOM_N:
                    m_uiGoGolemNGUID = go->GetGUID();
                    if (GetData(TYPE_LYCEUM) == DONE)
                        go->UseDoorOrButton();
                    break;
                case GO_GOLEM_ROOM_S:
                    m_uiGoGolemSGUID = go->GetGUID();
                    if (GetData(TYPE_LYCEUM) == DONE)
                        go->UseDoorOrButton();
                    break;
                case GO_THRONE_ROOM:
                    m_uiGoThroneGUID = go->GetGUID();
                    if (GetData(TYPE_IRON_HALL) == DONE)
                        go->UseDoorOrButton();
                    break;
                case GO_SPECTRAL_CHALICE:
                    m_uiSpectralChaliceGUID = go->GetGUID();
                    break;
                case GO_CHEST_SEVEN:
                    m_uiSevensChestGUID = go->GetGUID();
                    break;
                case GO_SECRET_DOOR:
                    m_uiGoSecretDoorGUID = go->GetGUID();
                    break;
                case GO_JAIL_DOOR_SUPPLY:
                    m_uiGoJailSupplyRoomGUID = go->GetGUID();
                    break;
                case GO_JAIL_SUPPLY_CRATE:
                    m_uiGoJailSupplyCrateGUID = go->GetGUID();
                    break;
                case GO_ARENA_SPOILS:
                    m_uiArenaSpoilsGUID = go->GetGUID();
                    break;
                case GO_DWARF_RUNE_A01:
                    m_uiDwarfRuneA01GUID = go->GetGUID();
                    break;
                case GO_DWARF_RUNE_B01:
                    m_uiDwarfRuneB01GUID = go->GetGUID();
                    break;
                case GO_DWARF_RUNE_C01:
                    m_uiDwarfRuneC01GUID = go->GetGUID();
                    break;
                case GO_DWARF_RUNE_D01:
                    m_uiDwarfRuneD01GUID = go->GetGUID();
                    break;
                case GO_DWARF_RUNE_E01:
                    m_uiDwarfRuneE01GUID = go->GetGUID();
                    break;
                case GO_DWARF_RUNE_F01:
                    m_uiDwarfRuneF01GUID = go->GetGUID();
                    break;
                case GO_DWARF_RUNE_G01:
                    m_uiDwarfRuneG01GUID = go->GetGUID();
                    break;
            }
        }

        // VMaNGOS OnCreatureDeath
        void OnUnitDeath(Unit* unit) override
        {
            _encounterTracker.OnUnitDeath(unit);

            if (unit->GetTypeId() != TYPEID_UNIT)
                return;

            switch (unit->GetEntry())
            {
                case NPC_BURNING_SPIRIT:
                    m_burningSpirits.remove(unit->GetGUID());
                    break;
                case NPC_SHADOWFORGE_SENATOR:
                    // Emperor Dagran Thaurissan performs a random yell upon the death
                    // of Shadowforge Senators in the Throne Room
                    if (Creature* dagran = instance->GetCreature(GetGuidData(DATA_EMPEROR)))
                    {
                        if (!dagran->IsAlive())
                            return;

                        if (m_uiDagranTimer > 0)
                            return;

                        uint32 textId;
                        switch (urand(0, 3))
                        {
                            case 0: textId = YELL_SENATOR_1; break;
                            case 1: textId = YELL_SENATOR_2; break;
                            case 2: textId = YELL_SENATOR_3; break;
                            default: textId = YELL_SENATOR_4; break;
                        }
                        ClassicScriptText(textId, dagran);
                        m_uiDagranTimer = 45000;    // set a timer of 45 sec to avoid Emperor Thaurissan to spam yells in case many senators are killed in a short amount of time
                    }
                    break;
            }
        }

        void HandleBarPatrons(uint8 eventType)
        {
            switch (eventType)
            {
                // case for periodical handle of random emotes
                case PATRON_EMOTE:
                    if (GetData(TYPE_PLUGGER) == DONE)
                        return;

                    for (ObjectGuid const& guid : m_sBarPatronNpcGuids)
                    {
                        // About 5% of patrons do emote at a given time
                        // So avoid executing follow up code for the 95% others
                        if (urand(0, 100) < 4)
                        {
                            // Only three emotes are seen in data: laugh, cheer and exclamation
                            // the last one appearing the least and the first one appearing the most
                            // emotes are stored in a table and frequency is handled there
                            if (Creature* patron = instance->GetCreature(guid))
                                patron->HandleEmoteCommand(BrdPatronsEmotes[urand(0, 5)]);
                        }
                    }
                    return;
                // case for Rocknot event when breaking the barrel
                case PATRON_PISSED:
                    // Three texts are said, one less often than the two others
                    // Only by patrons near the broken barrel react to Rocknot's rampage
                    if (GameObject* go = instance->GetGameObject(m_uiGoBarKegTrapGUID))
                    {
                        for (ObjectGuid const& guid : m_sBarPatronNpcGuids)
                        {
                            if (Creature* patron = instance->GetCreature(guid))
                            {
                                if (patron->GetPositionZ() > go->GetPositionZ() - 1 && patron->IsWithinDist2d(go->GetPositionX(), go->GetPositionY(), 18.0f))
                                {
                                    uint32 textId = 0;
                                    switch (urand(0, 4))
                                    {
                                        case 0: textId = SAY_PISSED_PATRON_3; break;
                                        case 1:  // case is double to give this text twice the chance of the previous one do be displayed
                                        case 2: textId = SAY_PISSED_PATRON_2; break;
                                        // covers the two remaining cases
                                        default: textId = SAY_PISSED_PATRON_1; break;
                                    }
                                    ClassicScriptText(textId, patron);
                                }
                            }
                        }
                    }
                    return;
                // case when Plugger is killed
                case PATRON_HOSTILE:
                    if (m_bBarHostile)
                        return;

                    m_bBarHostile = true;

                    for (ObjectGuid const& guid : m_sBarPatronNpcGuids)
                    {
                        if (Creature* patron = instance->GetCreature(guid))
                        {
                            patron->SetFaction(BRD_FACTION_DARK_IRON);      // VMaNGOS SetFactionTemporary(.., TEMPFACTION_RESTORE_RESPAWN)
                            patron->SetStandState(UNIT_STAND_STATE_STAND);
                            patron->SetEmoteState(EMOTE_ONESHOT_NONE);      // VMaNGOS HandleEmote(0)
                            patron->SetDefaultMovementType(RANDOM_MOTION_TYPE);
                            patron->SetWanderDistance(3.0f);
                            patron->GetMotionMaster()->Initialize();
                        }
                    }
                    // Mistress Nagmara and Private Rocknot despawn if the bar turns hostile
                    if (Creature* rocknot = instance->GetCreature(GetGuidData(DATA_ROCKNOT)))
                    {
                        if (rocknot->HasAura(15064)) // don't despawn if Rocknot and Nagmara are kissing under the stairs
                            return;
                        ClassicScriptText(SAY_ROCKNOT_DESPAWN, rocknot);
                        rocknot->DespawnOrUnsummon();
                    }
                    if (Creature* nagmara = instance->GetCreature(GetGuidData(DATA_NAGMARA)))
                    {
                        nagmara->CastSpell(nagmara, SPELL_NAGMARA_VANISH, true);
                        nagmara->DespawnOrUnsummon();
                    }
                    return;
            }
        }

        void HandleBarPatrol(uint8 step)
        {
            if (GetData(TYPE_PATROL) == DONE)
                return;

            switch (step)
            {
                case 0:
                    if (Creature* plugger = instance->GetCreature(GetGuidData(DATA_PLUGGER)))
                    {
                        // if relevant, open the bar door
                        if (GameObject* go = instance->GetGameObject(m_uiGoBarDoorGUID))
                        {
                            if (go->GetGoState() == GO_STATE_READY) // Closed
                                DoUseDoorOrButton(m_uiGoBarDoorGUID);
                        }

                        // One Fireguard Destroyer and two Anvilrage Officers are spawned
                        for (uint32 entry : BrdBarPatrolId)
                        {
                            // spawn them behind the bar door
                            Position spawnPos = plugger->GetRandomPoint(Position(BrdBarPatrolPositions[0][0], BrdBarPatrolPositions[0][1], BrdBarPatrolPositions[0][2]), 2.0f);
                            if (Creature* summoned = plugger->SummonCreature(entry, spawnPos.GetPositionX(), spawnPos.GetPositionY(), spawnPos.GetPositionZ(), BrdBarPatrolPositions[0][3], TEMPSUMMON_DEAD_DESPAWN, 0s))
                            {
                                m_sBarPatrolGuids.push_back(summoned->GetGUID());
                                // move them to the Grim Guzzler
                                Position movePos = plugger->GetRandomPoint(Position(BrdBarPatrolPositions[1][0], BrdBarPatrolPositions[1][1], BrdBarPatrolPositions[1][2]), 2.0f);
                                summoned->GetMotionMaster()->MoveIdle();
                                summoned->GetMotionMaster()->MovePoint(0, movePos.GetPositionX(), movePos.GetPositionY(), movePos.GetPositionZ());
                            }
                        }
                        // start timer to handle the yells
                        m_uiPatrolTimer = 5000;
                        break;
                    }
                    [[fallthrough]];    // VMaNGOS falls through when Plugger is not found
                case 1:
                    for (ObjectGuid const& guid : m_sBarPatrolGuids)
                    {
                        if (Creature* creature = instance->GetCreature(guid))
                        {
                            if (creature->GetEntry() == NPC_ANVILRAGE_OFFICER)
                            {
                                ClassicScriptText(YELL_PATROL_1, creature);
                                SetData(TYPE_PATROL, SPECIAL); // temporary set the status to special before the next yell: event will then be complete
                                m_uiPatrolTimer = 3000;
                                break;
                            }
                        }
                    }
                    break;
                case 2:
                    for (ObjectGuid const& guid : m_sBarPatrolGuids)
                    {
                        if (Creature* creature = instance->GetCreature(guid))
                        {
                            if (creature->GetEntry() == NPC_ANVILRAGE_OFFICER)
                            {
                                ClassicScriptText(YELL_PATROL_2, creature);
                                SetData(TYPE_PATROL, DONE);
                                m_uiPatrolTimer = 0;
                                break;
                            }
                        }
                    }
                    break;
            }
        }

        void BeginTheldrenEvent(ObjectGuid playerGuid)
        {
            SetData(DATA_THELDREN, IN_PROGRESS);

            m_uiChallengerPlayerGUID = playerGuid;
        }

        void ReplacePrincessIfPossible()
        {
            Map::PlayerList const& players = instance->GetPlayers();
            if (players.empty())
                return;

            bool needsReplacing = true;
            for (MapReference const& itr : players)
            {
                if (Player* player = itr.GetSource())
                {
                    // if at least one player didn't complete the quest, return false
                    if ((player->GetTeam() == ALLIANCE && !player->GetQuestRewardStatus(QUEST_FATE_KINGDOM))
                        || (player->GetTeam() == HORDE && !player->GetQuestRewardStatus(QUEST_ROYAL_RESCUE)))
                        needsReplacing = false;
                }
            }

            if (needsReplacing)
            {
                if (Creature* moira = instance->GetCreature(GetGuidData(DATA_PRINCESS)))
                    moira->UpdateEntry(NPC_HIGH_PRIESTESS);
            }
        }

        // VMaNGOS CustomSpellCasted(27517, caster, target): called by classic_spell_brd_summon_thelrin_dnd
        void SetGuidData(uint32 type, ObjectGuid data) override
        {
            switch (type)
            {
                case DATA_ARENA_CHALLENGER:
                    // On invoque pas 2 fois ...
                    if (m_bIsTheldrenInvocated)
                        return;

                    if (data.IsPlayer())
                    {
                        m_bIsTheldrenInvocated = true;
                        BeginTheldrenEvent(data);
                    }
                    break;
                default:
                    break;
            }
        }

        void SetData(uint32 type, uint32 data) override
        {
            TC_LOG_DEBUG("scripts", "classic_instance_blackrock_depths: SetData update (Type: {} Data {})", type, data);

            switch (type)
            {
                case TYPE_RING_OF_LAW:
                    if (data == DONE)
                    {
                        for (ObjectGuid const& guid : m_lArenaSpectatorMobGUIDList)
                        {
                            if (Creature* creature = instance->GetCreature(guid))
                            {
                                if (creature->IsAlive())
                                    creature->SetFaction(BRD_FACTION_ARENA_NEUTRAL);
                            }
                        }
                    }
                    m_auiEncounter[TYPE_RING_OF_LAW] = data;
                    break;
                case TYPE_VAULT:
                    m_auiEncounter[TYPE_VAULT] = data;
                    break;
                case TYPE_ROCKNOT:
                    if (data == SPECIAL)
                        ++m_uiBarAleCount;
                    else
                    {
                        if (data == DONE)
                            HandleBarPatrons(PATRON_PISSED);
                        m_auiEncounter[2] = data;
                    }
                    break;
                case TYPE_TOMB_OF_SEVEN:
                    switch (data)
                    {
                        case IN_PROGRESS:
                            DoUseDoorOrButton(m_uiGoTombEnterGUID);
                            break;
                        case FAIL:
                            if (m_auiEncounter[3] == IN_PROGRESS)//prevent use more than one time
                                DoUseDoorOrButton(m_uiGoTombEnterGUID);
                            break;
                        case DONE:
                            // VMaNGOS passes HOUR * IN_MILLISECONDS as respawn time in seconds (chest effectively stays)
                            DoRespawnGameObject(m_uiSevensChestGUID, Seconds(HOUR * IN_MILLISECONDS));
                            DoUseDoorOrButton(m_uiGoTombExitGUID);
                            DoUseDoorOrButton(m_uiGoTombEnterGUID);
                            break;
                    }
                    m_auiEncounter[TYPE_TOMB_OF_SEVEN] = data;
                    break;
                case TYPE_LYCEUM:
                    if (data == IN_PROGRESS && GetData(TYPE_LYCEUM) == DONE)
                        data = DONE;
                    if (data == DONE)
                    {
                        DoOpenDoor(m_uiGoGolemNGUID);
                        DoOpenDoor(m_uiGoGolemSGUID);
                        if (Creature* magmus = instance->GetCreature(m_uiMagmusGUID))
                        {
                            ClassicScriptText(YELL_MAGMUS, magmus);
                            std::list<Creature*> anvilrageList;
                            GetCreatureListWithEntryInGrid(anvilrageList, magmus, 8901, 400.0f);

                            for (Creature* anvilrage : anvilrageList)
                                anvilrage->SetRespawnDelay(4 * DAY);
                        }
                    }
                    m_auiEncounter[TYPE_LYCEUM] = data;
                    break;
                case TYPE_IRON_HALL:
                    switch (data)
                    {
                        case IN_PROGRESS:
                            DoResetDoor(m_uiGoGolemNGUID);
                            DoResetDoor(m_uiGoGolemSGUID);
                            break;
                        case FAIL:
                            DoOpenDoor(m_uiGoGolemNGUID);
                            DoOpenDoor(m_uiGoGolemSGUID);
                            break;
                        case DONE:
                            DoOpenDoor(m_uiGoGolemNGUID);
                            DoOpenDoor(m_uiGoGolemSGUID);
                            DoOpenDoor(m_uiGoThroneGUID);
                            ReplacePrincessIfPossible();
                            break;
                    }
                    m_auiEncounter[TYPE_IRON_HALL] = data;
                    break;
                case TYPE_THUNDERBREW:
                    if (data == IN_PROGRESS)
                    {
                        m_uiThunderbrewCount++;
                        if (m_uiThunderbrewCount == 3)
                            m_auiEncounter[TYPE_THUNDERBREW] = DONE;
                    }
                    break;
                case TYPE_RELIC_COFFER:
                    if (data == IN_PROGRESS)
                    {
                        m_uiRelicCofferDoorCount++;
                        if (m_uiRelicCofferDoorCount == 12)
                            m_auiEncounter[TYPE_RELIC_COFFER] = DONE;
                    }
                    break;
                case TYPE_DOOMGRIP:
                    if (data == DONE)
                        DoUseDoorOrButton(m_uiGoSecretDoorGUID);
                    m_auiEncounter[TYPE_DOOMGRIP] = data;
                    break;
                case TYPE_RIBBLY:
                    if (data == DONE)
                    {
                        for (ObjectGuid const& guid : m_lRibblySCronyMobGUIDList)
                        {
                            if (Creature* creature = instance->GetCreature(guid))
                            {
                                if (creature->IsAlive())
                                {
                                    creature->SetFaction(14);
                                    Unit* victim = creature->GetVictim();
                                    if (creature->AI() && victim)
                                        creature->AI()->AttackStart(victim);
                                }
                            }
                        }
                    }
                    m_auiEncounter[TYPE_RIBBLY] = data;
                    break;
                case DATA_ARGELMACH_AGGRO:
                    if (data == IN_PROGRESS)
                    {
                        if (Creature* argelmach = instance->GetCreature(m_uiGolemLordArgelmachGUID))
                            if (Unit* victim = argelmach->GetVictim())
                                for (ObjectGuid const& guid : m_lArgelmachProtectorsMobGUIDList)
                                    if (Creature* protector = instance->GetCreature(guid))
                                        if (protector->IsAlive() && protector->AI() && protector->IsWithinDist(argelmach, 80.0f))
                                            protector->AI()->AttackStart(victim);
                    }
                    m_auiEncounter[DATA_ARGELMACH_AGGRO] = data;
                    break;
                case TYPE_PATROL:
                    if (data == IN_PROGRESS)
                        HandleBarPatrol(0);
                    m_auiEncounter[11] = data;
                    break;
                case DATA_THELDREN:
                    if (data == DONE)
                    {
                        // Give kill credit for quest The Challenge (9015)
                        for (MapReference const& itr : instance->GetPlayers())
                        {
                            if (Player* player = itr.GetSource())
                                player->KilledMonsterCredit(NPC_THELDREN_KILL_CREDIT);
                        }

                        // Spawn "Arena Spoils" chest with sick loot
                        DoRespawnGameObject(m_uiArenaSpoilsGUID);
                    }
                    m_auiEncounter[12] = data;
                    break;
                case TYPE_NAGMARA:
                    m_auiEncounter[13] = data;
                    break;
                case TYPE_BRIDGE:
                    m_auiEncounter[14] = data;
                    break;
                case TYPE_PLUGGER:
                    if (data == SPECIAL)
                    {
                        if (instance->GetCreature(m_uiPluggerSpazzringGUID))
                        {
                            ++m_uiStolenAles;
                            if (m_uiStolenAles == 3)
                                data = IN_PROGRESS;
                        }
                    }
                    m_auiEncounter[15] = data;
                    break;
                case TYPE_QUEST_JAIL_BREAK:
                    m_auiEncounter[16] = data;
                    break;
                case TYPE_JAIL_DUGHAL:
                    m_auiEncounter[17] = data;
                    break;
                case TYPE_JAIL_SUPPLY_ROOM:
                    m_auiEncounter[18] = data;
                    break;
                case TYPE_JAIL_TOBIAS:
                    m_auiEncounter[19] = data;
                    break;
                case GO_JAIL_DOOR_DUGHAL: m_bDoorDughalOpened = data != 0; break;
                case GO_JAIL_DOOR_TOBIAS: m_bDoorTobiasOpened = data != 0; break;
                case GO_JAIL_DOOR_CREST:  m_bDoorCrestOpened  = data != 0; break;
                case GO_JAIL_DOOR_JAZ:    m_bDoorJazOpened    = data != 0; break;
                case GO_JAIL_DOOR_SHILL:  m_bDoorShillOpened  = data != 0; break;
                case GO_JAIL_DOOR_SUPPLY: m_bDoorSupplyOpened = data != 0; break;
                case EVENT_BAR_PATRONS:
                    HandleBarPatrons(uint8(data));
                    break;
                case TYPE_FLAMELASH:
                    if (data == NOT_STARTED || data == FAIL || data == DONE)
                    {
                        for (uint8 i = 0; i < DWARF_RUNES_MAX; i++)
                        {
                            if (GameObject* rune = instance->GetGameObject(GetGuidData(GO_DWARF_RUNE_A01 + i)))
                                rune->ResetDoorOrButton();
                        }

                        for (uint32& i : m_uiSpiritTimer)
                            i = 5 * IN_MILLISECONDS;

                        for (ObjectGuid const& guid : m_burningSpirits)
                        {
                            if (Creature* summon = instance->GetCreature(guid))
                                if (!summon->IsInCombat() || data != DONE)
                                    summon->DespawnOrUnsummon();
                        }

                        m_burningSpirits.clear();
                    }
                    else if (data == IN_PROGRESS)
                    {
                        for (uint8 i = 0; i < DWARF_RUNES_MAX; i++)
                        {
                            if (GameObject* rune = instance->GetGameObject(GetGuidData(GO_DWARF_RUNE_A01 + i)))
                                rune->UseDoorOrButton();
                        }
                    }
                    m_auiEncounter[20] = data;
                    break;
            }

            if (data == DONE)
                SaveEncounterData();
        }

        uint32 GetData(uint32 type) const override
        {
            switch (type)
            {
                case TYPE_RING_OF_LAW:
                    return m_auiEncounter[0];
                case TYPE_VAULT:
                    return m_auiEncounter[1];
                case TYPE_ROCKNOT:
                    if (m_auiEncounter[2] == IN_PROGRESS && m_uiBarAleCount == 3)
                        return SPECIAL;
                    else
                        return m_auiEncounter[2];
                case TYPE_TOMB_OF_SEVEN:
                    return m_auiEncounter[3];
                case TYPE_LYCEUM:
                    return m_auiEncounter[4];
                case TYPE_IRON_HALL:
                    return m_auiEncounter[5];
                case TYPE_THUNDERBREW:
                    return m_auiEncounter[6];
                case TYPE_RELIC_COFFER:
                    return m_auiEncounter[7];
                case TYPE_DOOMGRIP:
                    return m_auiEncounter[8];
                case TYPE_RIBBLY:
                    return m_auiEncounter[9];
                case DATA_ARGELMACH_AGGRO:
                    return m_auiEncounter[10];
                case TYPE_PATROL:
                    return m_auiEncounter[11];
                case DATA_THELDREN:
                    return m_auiEncounter[12];
                case TYPE_NAGMARA:
                    return m_auiEncounter[13];
                case TYPE_BRIDGE:
                    return m_auiEncounter[14];
                case TYPE_PLUGGER:
                    return m_auiEncounter[15];
                case TYPE_QUEST_JAIL_BREAK:
                    return m_auiEncounter[16];
                case TYPE_JAIL_DUGHAL:
                    return m_auiEncounter[17];
                case TYPE_JAIL_SUPPLY_ROOM:
                    return m_auiEncounter[18];
                case TYPE_JAIL_TOBIAS:
                    return m_auiEncounter[19];
                case GO_JAIL_DOOR_DUGHAL: return m_bDoorDughalOpened;
                case GO_JAIL_DOOR_TOBIAS: return m_bDoorTobiasOpened;
                case GO_JAIL_DOOR_CREST:  return m_bDoorCrestOpened;
                case GO_JAIL_DOOR_JAZ:    return m_bDoorJazOpened;
                case GO_JAIL_DOOR_SHILL:  return m_bDoorShillOpened;
                case GO_JAIL_DOOR_SUPPLY: return m_bDoorSupplyOpened;
                case TYPE_FLAMELASH:
                    return m_auiEncounter[20];
            }
            return 0;
        }

        // VMaNGOS GetData64
        ObjectGuid GetGuidData(uint32 type) const override
        {
            switch (type)
            {
                case DATA_EMPEROR:
                    return m_uiEmperorGUID;
                case DATA_PRINCESS:
                    return m_uiPrincessGUID;
                case DATA_PHALANX:
                    return m_uiPhalanxGUID;
                case DATA_HATEREL:
                    return m_uiHaterelGUID;
                case DATA_ANGERREL:
                    return m_uiAngerrelGUID;
                case DATA_VILEREL:
                    return m_uiVilerelGUID;
                case DATA_GLOOMREL:
                    return m_uiGloomrelGUID;
                case DATA_SEETHREL:
                    return m_uiSeethrelGUID;
                case DATA_DOOMREL:
                    return m_uiDoomrelGUID;
                case DATA_DOPEREL:
                    return m_uiDoperelGUID;

                case DATA_ARENA1:
                    return m_uiGoArena1GUID;
                case DATA_ARENA2:
                    return m_uiGoArena2GUID;
                case DATA_ARENA3:
                    return m_uiGoArena3GUID;
                case DATA_ARENA4:
                    return m_uiGoArena4GUID;

                case DATA_GO_BAR_KEG:
                    return m_uiGoBarKegGUID;
                case DATA_GO_BAR_KEG_TRAP:
                    return m_uiGoBarKegTrapGUID;
                case DATA_GO_BAR_DOOR:
                    return m_uiGoBarDoorGUID;
                case DATA_GO_CHALICE:
                    return m_uiSpectralChaliceGUID;
                case DATA_GO_TOMB_EXIT:
                    return m_uiGoTombExitGUID;

                case DATA_ROCKNOT:
                    return m_uiRocknotGUID;
                case DATA_NAGMARA:
                    return m_uiNagmaraGUID;
                case DATA_PLUGGER:
                    return m_uiPluggerSpazzringGUID;

                case NPC_OGRABISI:
                    return m_uiOgrabisiGUID;
                case NPC_SHILL:
                    return m_uiShillGUID;
                case NPC_CREST:
                    return m_uiCrestGUID;
                case NPC_JAZ:
                    return m_uiJazGUID;
                case GO_JAIL_DOOR_SUPPLY:
                    return m_uiGoJailSupplyRoomGUID;
                case GO_JAIL_SUPPLY_CRATE:
                    return m_uiGoJailSupplyCrateGUID;
                case GO_DWARF_RUNE_A01:
                    return m_uiDwarfRuneA01GUID;
                case GO_DWARF_RUNE_B01:
                    return m_uiDwarfRuneB01GUID;
                case GO_DWARF_RUNE_C01:
                    return m_uiDwarfRuneC01GUID;
                case GO_DWARF_RUNE_D01:
                    return m_uiDwarfRuneD01GUID;
                case GO_DWARF_RUNE_E01:
                    return m_uiDwarfRuneE01GUID;
                case GO_DWARF_RUNE_F01:
                    return m_uiDwarfRuneF01GUID;
                case GO_DWARF_RUNE_G01:
                    return m_uiDwarfRuneG01GUID;

                case DATA_ARENA_CHALLENGER:
                    return m_uiChallengerPlayerGUID;
                case NPC_GRIMSTONE:
                    return m_uiGrimstoneGUID;
            }
            return ObjectGuid::Empty;
        }

        void Update(uint32 diff) override
        {
            _encounterTracker.Update(diff);

            if (m_uiDagranTimer)
            {
                if (m_uiDagranTimer <= diff)
                    m_uiDagranTimer = 0;
                else
                    m_uiDagranTimer -= diff;
            }

            // Every second some of the patrons will do one random emote if they are not hostile (i.e. Plugger event is not done/in progress)
            if (m_uiPatronEmoteTimer)
            {
                if (m_uiPatronEmoteTimer <= diff)
                {
                    HandleBarPatrons(PATRON_EMOTE);
                    m_uiPatronEmoteTimer = 1250;
                }
                else
                    m_uiPatronEmoteTimer -= diff;
            }

            if (m_uiPatrolTimer)
            {
                if (m_uiPatrolTimer <= diff)
                {
                    switch (GetData(TYPE_PATROL))
                    {
                        case IN_PROGRESS:
                            HandleBarPatrol(1);
                            break;
                        case SPECIAL:
                            HandleBarPatrol(2);
                            break;
                        default:
                            break;
                    }
                }
                else
                    m_uiPatrolTimer -= diff;
            }

            if (GetData(TYPE_FLAMELASH) == IN_PROGRESS)
            {
                // cMaNGOS boss_ambassador_flamelash: a spirit that reaches Flamelash casts Burning Spirit on him.
                // Taken off the list first, the cast kills the spirit and its death would edit the list again.
                if (Creature* flamelash = instance->GetCreature(m_uiFlamelashGUID))
                {
                    std::vector<Creature*> arrived;
                    for (auto itr = m_burningSpirits.begin(); itr != m_burningSpirits.end();)
                    {
                        Creature* spirit = instance->GetCreature(*itr);
                        if (spirit && spirit->IsAlive() && spirit->IsWithinDistInMap(flamelash, 2 * CONTACT_DISTANCE))
                        {
                            arrived.push_back(spirit);
                            itr = m_burningSpirits.erase(itr);
                        }
                        else
                            ++itr;
                    }

                    for (Creature* spirit : arrived)
                        spirit->CastSpell(flamelash, SPELL_BURNING_SPIRIT, true);
                }

                for (uint8 i = 0; i < DWARF_RUNES_MAX; i++)
                {
                    if (m_uiSpiritTimer[i] < diff)
                    {
                        if (Creature* flamelash = instance->GetCreature(m_uiFlamelashGUID))
                        {
                            if (GameObject* rune = instance->GetGameObject(GetGuidData(GO_DWARF_RUNE_A01 + i)))
                            {
                                if (m_burningSpirits.size() < size_t(BURNING_SPIRIT_MAX))
                                {
                                    // VMaNGOS Map::SummonCreature at the rune position; the rune is used as summoner here
                                    if (Creature* spirit = rune->SummonCreature(NPC_BURNING_SPIRIT, rune->GetPositionX(), rune->GetPositionY(), rune->GetPositionZ(), rune->GetOrientation(), TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 60s))
                                    {
                                        spirit->SetWalk(false);
                                        spirit->GetMotionMaster()->MoveFollow(flamelash, 0.0f, ChaseAngle(0.0f));
                                        m_burningSpirits.push_back(spirit->GetGUID());
                                    }
                                    m_uiSpiritTimer[i] = urand(15 * IN_MILLISECONDS, 30 * IN_MILLISECONDS);
                                }
                                else
                                    m_uiSpiritTimer[i] = 1 * IN_MILLISECONDS;
                            }
                        }
                    }
                    else
                        m_uiSpiritTimer[i] -= diff;
                }
            }
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new classic_instance_blackrock_depths_InstanceScript(map);
    }
};

void AddSC_classic_instance_blackrock_depths()
{
    new classic_instance_blackrock_depths();
}
