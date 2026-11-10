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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/burning_steppes/blackrock_spire/instance_blackrock_spire.cpp
// (ScriptDev2 lineage, GPL-2)
// Ported: instance_blackrock_spire, at_blackrock_spire, go_father_flame, at_ubrs_the_beast, spell_ubrs_freeze_rookery_egg

#include "ScriptMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureData.h"
#include "DB2Structure.h"
#include "GameObject.h"
#include "GameEventSender.h"
#include "GameObjectAI.h"
#include "GameTime.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MapReference.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "MovementDefines.h"
#include "Player.h"
#include "Random.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "classic_blackrock_spire.h"
#include "classic_dungeon_encounter_tracker.h"
#include "classic_script_text.h"
#include <algorithm>
#include <array>
#include <list>
#include <memory>
#include <vector>

using namespace ClassicBlackrockSpire;

namespace
{
    enum ClassicBrsInstanceData : uint32
    {
        AREATRIGGER_ENTER_UBRS      = 2046,
        EVENT_EMBERSEER_START       = 4884,                     // spell 16533, sent by the Blackrock Altar ritual
        EVENT_SPIDER_EGGS_LOOTED    = 4799,                     // Spire Spider Egg chests 175588 / 175606 (Data6), fired when looted
        NPC_SPIRE_SPIDERLING        = 10375,                    // VMaNGOS spell 16453 Summon Spire Spiderling (egg trap 175590); 10229 has no template
        SPIDER_EGG_SPIDERLINGS      = 3,
        DATA_EMBERSEER_START        = 1,                        // SmartAI 9816 "on data set 1 1": growth phase
        SPELL_ENCAGED_EMBERSEER     = 15282,                    // creature_template_addon aura, removed 5 s into the growth
        SPELL_EMBERSEER_GROWING     = 16048,
        SPELL_FREEZE_ANIM           = 16245,
        SPELL_EMBERSEER_FULL_STRENGTH = 16047,
        AREATRIGGER_STADIUM         = 2026,

        // Arena event dialogue intro and outro - handled by instance
        SAY_NEFARIUS_INTRO_1        = 5635,
        SAY_NEFARIUS_INTRO_2        = 5640,
        SAY_NEFARIUS_LOSE1          = 5709,
        SAY_REND_ATTACK             = 5722,
        SAY_NEFARIUS_WARCHIEF       = 5720,
        SAY_NEFARIUS_PACING         = 5721,
        SAY_NEFARIUS_VICTORY        = 5824,

        // Arena event random taunt - handled on creature death
        SAY_NEFARIUS_TAUNT1         = 5665,
        SAY_NEFARIUS_TAUNT2         = 5671,
        SAY_NEFARIUS_TAUNT3         = 5666,
        SAY_NEFARIUS_TAUNT4         = 5667,
        SAY_NEFARIUS_TAUNT5         = 5668,
        SAY_NEFARIUS_TAUNT6         = 5669,
        SAY_NEFARIUS_TAUNT7         = 5664,
        SAY_NEFARIUS_TAUNT8         = 5719,
        SAY_REND_TAUNT1             = 5672,
        SAY_REND_TAUNT2             = 5678,
        SAY_REND_TAUNT3             = 5673,
        SAY_REND_TAUNT4             = 5674,

        SAY_ROOKERY_EVENT_START     = 5538,

        WAYPOINT_ID_STADIUM         = 10442,

        ITEM_SEAL_OF_ASCENSION      = 12344
    };

    // Waypoint paths as imported from VMaNGOS into world.waypoint_path:
    //   creature_movement_special id X   -> 800000000 + (2000000 + X) * 10
    //   creature_movement_template entry -> 810000000 + entry * 10
    // TODO(classic): VMaNGOS creature_movement_special 10442 (stadium waves) and creature_movement_template 10339 (Gyth)
    // are not imported into world.waypoint_path yet (810101620 for Nefarius is).
    constexpr uint32 CLASSIC_BRS_PATH_STADIUM_WAVES = 800000000 + (2000000 + WAYPOINT_ID_STADIUM) * 10;
    constexpr uint32 CLASSIC_BRS_PATH_NEFARIUS      = 810000000 + NPC_LORD_VICTOR_NEFARIUS * 10;
    constexpr uint32 CLASSIC_BRS_PATH_GYTH          = 810000000 + NPC_GYTH * 10;

    // Bannok Grimaxe placeholders (VMaNGOS guids 44020, 43764, 44327; spawn id = 20000000 + VMaNGOS guid)
    constexpr ObjectGuid::LowType CLASSIC_BRS_BANNOK_SPAWN_1 = 20000000 + 44020;
    constexpr ObjectGuid::LowType CLASSIC_BRS_BANNOK_SPAWN_2 = 20000000 + 43764;
    constexpr ObjectGuid::LowType CLASSIC_BRS_BANNOK_SPAWN_3 = 20000000 + 44327;

    char const* const ClassicBrsEncounterSaveKeys[INSTANCE_BRS_MAX_ENCOUNTER] =
    {
        "enc0", "enc1", "enc2", "enc3", "enc4", "enc5", "enc6", "enc7"
    };

    /* Areatrigger
    1470 Instance Entry
    1628 LBRS, between Spiders and Ogres
    1946 LBRS, ubrs pre-quest giver (1)
    1986 LBRS, ubrs pre-quest giver (2)
    1987 LBRS, ubrs pre-quest giver (3)
    2026 UBRS, stadium event trigger
    2046 UBRS, way to upper
    2066 UBRS, The Beast - Exit (to the dark chamber)
    2067 UBRS, The Beast - Entry
    2068 LBRS, fall out of map
    3726 UBRS, entrance to BWL
    */

    uint32 const aStadiumSpectators[12] =
    {
        NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_ELITE, NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_VETERAN,
        NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_ELITE, NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_VETERAN
    };

    Position const aSpectatorsSpawnLocs[12] =
    {
        { 163.3209f, -340.9818f, 111.0216f, 4.818223f },
        { 164.2471f, -339.0313f, 111.0368f, 1.413717f },
        { 161.124f, -339.5178f, 111.0381f, 3.001966f },
        { 162.5045f, -337.8101f, 111.0367f, 4.13643f },
        { 160.9896f, -337.7715f, 111.0368f, 1.117011f },
        { 161.8347f, -335.7923f, 111.0352f, 2.286381f },
        { 113.9726f, -366.0805f, 116.9195f, 6.252025f },
        { 112.7245f, -368.9635f, 116.9307f, 4.677482f },
        { 110.5757f, -368.2123f, 116.9278f, 4.310963f },
        { 109.3343f, -366.4785f, 116.9261f, 2.740167f },
        { 110.1331f, -363.9824f, 116.9272f, 0.5235988f },
        { 111.9971f, -363.0948f, 116.929f, 5.951573f },
    };

    Position const aSpectatorsTargetLocs[12] =
    {
        { 160.619f, -395.826f, 121.9752f, -1.502597f },
        { 162.1428f, -395.1175f, 121.9751f, -1.67753f },
        { 158.6822f, -395.7097f, 121.9753f, -1.787977f },
        { 164.384f, -395.3787f, 121.9751f, -1.502597f },
        { 156.9669f, -395.2188f, 121.9752f, -1.678662f },
        { 166.2515f, -395.0366f, 121.975f, -1.791467f },
        { 143.814f, -396.7092f, 121.9753f, -1.40136f },
        { 145.3893f, -396.1959f, 121.9752f, -1.419479f },
        { 142.1598f, -396.0284f, 121.9752f, -1.661444f },
        { 147.7274f, -396.3042f, 121.9753f, -1.40136f },
        { 139.9446f, -396.7277f, 121.9753f, -1.428414f },
        { 149.3754f, -395.7497f, 121.9753f, -1.714769f },
    };

    Position const aStadiumLocs[7] =
    {
        { 210.00f, -420.30f, 110.94f, 3.14f },                  // dragons summon location
        { 211.762f, -397.58f, 111.18f, 4.74f },                 // Gyth summon location
        { 163.62f, -420.33f, 110.47f, 0.0f },                   // center of the stadium location (for movement)
        { 164.63f, -444.04f, 121.97f, 3.22f },                  // Lord Nefarius summon position
        { 161.01f, -443.73f, 121.97f, 6.26f },                  // Rend summon position
        { 164.64f, -443.30f, 121.97f, 1.61f },                  // Nefarius move position
        { 165.74f, -466.46f, 116.80f, 0.0f },                   // Rend move position
    };

    // Stadium event description
    uint32 const aStadiumEventNpcs[MAX_STADIUM_WAVES][MAX_STADIUM_MOBS_PER_WAVE] =
    {
        { NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_DRAGON, 0 },
        { NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_DRAGON, 0 },
        { NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_DRAGON, NPC_BLACKHAND_HANDLER, 0 },
        { NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_DRAGON, NPC_BLACKHAND_HANDLER, 0 },
        { NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_DRAGON, NPC_BLACKHAND_HANDLER },
        { NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_DRAGON, NPC_CHROMATIC_DRAGON, NPC_BLACKHAND_HANDLER },
        { NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_DRAGON, NPC_CHROMATIC_DRAGON, NPC_BLACKHAND_HANDLER },
    };

    // VMaNGOS DialogueHelper entries: { text id or marker, speaker entry (0 = no text), delay to next entry (0 = stop) }
    struct ClassicBrsDialogueEntry
    {
        uint32 TextEntry;
        uint32 SpeakerEntry;
        uint32 Timer;
    };

    ClassicBrsDialogueEntry const aStadiumDialogue[] =
    {
        { NPC_LORD_VICTOR_NEFARIUS,  0,                          1000 },
        { SAY_NEFARIUS_INTRO_1,      NPC_LORD_VICTOR_NEFARIUS,   7000 },
        { SAY_NEFARIUS_INTRO_2,      NPC_LORD_VICTOR_NEFARIUS,   5000 },
        { NPC_BLACKHAND_HANDLER,     0,                          0 },
        { SAY_NEFARIUS_LOSE1,        NPC_LORD_VICTOR_NEFARIUS,   3000 },
        { SAY_REND_ATTACK,           NPC_REND_BLACKHAND,         2000 },
        { SAY_NEFARIUS_WARCHIEF,     NPC_LORD_VICTOR_NEFARIUS,   3000 },
        { SAY_NEFARIUS_PACING,       NPC_LORD_VICTOR_NEFARIUS,   0 },
        { SAY_NEFARIUS_VICTORY,      NPC_LORD_VICTOR_NEFARIUS,   5000 },
        { NPC_REND_BLACKHAND,        0,                          0 },
        { 0, 0, 0 },
    };

    bool IsClassicBrsStadiumEntry(uint32 entry)
    {
        switch (entry)
        {
            case NPC_CHROMATIC_WHELP:
            case NPC_CHROMATIC_DRAGON:
            case NPC_BLACKHAND_HANDLER:
            case NPC_GYTH:
            case NPC_REND_BLACKHAND:
                return true;
            default:
                return false;
        }
    }
}

namespace
{
// Classic 1.60: TC boss states for the DungeonEncounter.db2 encounters of Blackrock Spire (map 229), driven by
// ClassicDungeonEncounterTracker (boss kill credit, encounter start / end, instance lock)
enum ClassicBlackrockSpireEncounterBosses : uint32
{
    CBRS_ENC_OMOKK = 0,
    CBRS_ENC_VOSHGAJIN = 1,
    CBRS_ENC_VOONE = 2,
    CBRS_ENC_SMOLDERWEB = 3,
    CBRS_ENC_UROK = 4,
    CBRS_ENC_ZIGRIS = 5,
    CBRS_ENC_HALYCON = 6,
    CBRS_ENC_GIZRUL = 7,
    CBRS_ENC_WYRMTHALAK = 8,
    CBRS_ENC_EMBERSEER = 9,
    CBRS_ENC_REND = 10,
    CBRS_ENC_BEAST = 11,
    CBRS_ENC_DRAKKISATH = 12,
    CBRS_ENC_VALTHALAK = 13,
    CBRS_ENC_MAX
};

constexpr ClassicDungeonBossEntry ClassicBlackrockSpireEncounterBossEntries[] =
{
    { CBRS_ENC_OMOKK, 9196 }, // Highlord Omokk
    { CBRS_ENC_VOSHGAJIN, 9236 }, // Shadow Hunter Vosh'gajin
    { CBRS_ENC_VOONE, 9237 }, // War Master Voone
    { CBRS_ENC_SMOLDERWEB, 10596 }, // Mother Smolderweb
    { CBRS_ENC_UROK, 10584 }, // Urok Doomhowl
    { CBRS_ENC_ZIGRIS, 9736 }, // Quartermaster Zigris
    { CBRS_ENC_HALYCON, 10220 }, // Halycon
    { CBRS_ENC_GIZRUL, 10268 }, // Gizrul the Slavener
    { CBRS_ENC_WYRMTHALAK, 9568 }, // Overlord Wyrmthalak
    { CBRS_ENC_EMBERSEER, 9816 }, // Pyroguard Emberseer
    { CBRS_ENC_REND, 10429 }, // Warchief Rend Blackhand
    { CBRS_ENC_REND, 10339, CLASSIC_DUNGEON_BOSS_ENGAGE_ONLY }, // Gyth
    { CBRS_ENC_BEAST, 10430 }, // The Beast
    { CBRS_ENC_DRAKKISATH, 10363 }, // General Drakkisath
    { CBRS_ENC_VALTHALAK, 16042 } // Lord Valthalak
};
}

class classic_instance_blackrock_spire : public InstanceMapScript
{
public:
    classic_instance_blackrock_spire() : InstanceMapScript(InstanceScriptName, MapId) { }

    struct classic_instance_blackrock_spire_InstanceScript : public InstanceScript
    {
        classic_instance_blackrock_spire_InstanceScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(CBRS_ENC_MAX);
            static constexpr DungeonEncounterData encounters[] =
            {
                { CBRS_ENC_OMOKK, {{ 267 }} },
                { CBRS_ENC_VOSHGAJIN, {{ 268 }} },
                { CBRS_ENC_VOONE, {{ 269 }} },
                { CBRS_ENC_SMOLDERWEB, {{ 270 }} },
                { CBRS_ENC_UROK, {{ 271 }} },
                { CBRS_ENC_ZIGRIS, {{ 272 }} },
                { CBRS_ENC_HALYCON, {{ 274 }} },
                { CBRS_ENC_GIZRUL, {{ 273 }} },
                { CBRS_ENC_WYRMTHALAK, {{ 275 }} },
                { CBRS_ENC_EMBERSEER, {{ 3062 }} },
                { CBRS_ENC_REND, {{ 3063 }} },
                { CBRS_ENC_BEAST, {{ 3068 }} },
                { CBRS_ENC_DRAKKISATH, {{ 3069 }} },
                { CBRS_ENC_VALTHALAK, {{ 3070 }} }
            };
            LoadDungeonEncounterData(encounters);

            _encounter.fill(NOT_STARTED);
            for (uint32 i = 0; i < INSTANCE_BRS_MAX_ENCOUNTER; ++i)
                _saved[i] = std::make_unique<PersistentInstanceScriptValue<uint32>>(*this, ClassicBrsEncounterSaveKeys[i], uint32(NOT_STARTED));

            _fatherFlameTimer = 0;
            _fatherFlameWaveCount = 0;
            _ubrsDoorTimer = 0;
            _ubrsDoorStep = 0;
            _stadiumEventTimer = 0;
            _stadiumWaves = 0;
            _stadiumMobsAlive = 0;
            _stadiumCheckTimer = 1000;
            _bannokSpawned = false;
            _dialogueIndex = 0;
            _dialogueTimer = 0;
            _emberseerCheckTimer = 0;
            _emberseerFightTimer = 0;
        }

        ClassicDungeonEncounterTracker _encounterTracker{ *this, ClassicBlackrockSpireEncounterBossEntries };

        void OnCreatureRemove(Creature* creature) override
        {
            InstanceScript::OnCreatureRemove(creature);
            _encounterTracker.OnCreatureRemove(creature);
        }

        void AfterDataLoad() override
        {
            for (uint32 i = 0; i < INSTANCE_BRS_MAX_ENCOUNTER; ++i)
            {
                _encounter[i] = uint32(*_saved[i]);
                if (_encounter[i] == IN_PROGRESS)
                    _encounter[i] = NOT_STARTED;
            }
        }

        // VMaNGOS OnCreatureCreate: 14.26% chance to spawn Bannok Grimaxe instead of one of his 3 placeholders
        // (done through the TC ZoneScript entry hook instead of UpdateEntry)
        uint32 GetCreatureEntry(ObjectGuid::LowType spawnId, CreatureData const* data) override
        {
            if (data->id == NPC_FIREBRAND_GRUNT)
            {
                switch (spawnId)
                {
                    case CLASSIC_BRS_BANNOK_SPAWN_1:
                    case CLASSIC_BRS_BANNOK_SPAWN_2:
                    case CLASSIC_BRS_BANNOK_SPAWN_3:
                        if (!_bannokSpawned && urand(0, 99) < 5)
                        {
                            _bannokSpawned = true;
                            return NPC_BANNOK_GRIMAXE;
                        }
                        break;
                    default:
                        break;
                }
            }

            return data->id;
        }

        void OnGameObjectCreate(GameObject* go) override
        {
            InstanceScript::OnGameObjectCreate(go);

            bool const ubrsDoorDone = GetData(TYPE_EVENT_DOOR_UBRS) == DONE;

            switch (go->GetEntry())
            {
                case GO_BLACKROCK_ALTAR:
                    _blackRockAltarGUID = go->GetGUID();
                    break;
                case GO_EMBERSEER_IN:
                    _emberseerInDoorGUID = go->GetGUID();
                    if (GetData(TYPE_ROOM_EVENT) == DONE)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                case GO_DOORS:
                    _emberseerCombatDoorGUID = go->GetGUID();
                    break;
                case GO_EMBERSEER_OUT:
                    _emberseerOutDoorGUID = go->GetGUID();
                    if (GetData(TYPE_EMBERSEER) == DONE)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                case GO_GYTH_ENTRY_DOOR:
                    _gythEntryDoorGUID = go->GetGUID();
                    break;
                case GO_GYTH_COMBAT_DOOR:
                    _gythCombatDoorGUID = go->GetGUID();
                    break;
                case GO_GYTH_EXIT_DOOR:
                    _gythExitDoorGUID = go->GetGUID();
                    if (GetData(TYPE_STADIUM) == DONE)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                case GO_DRAKKISATH_DOOR1:
                    _drakkisathDoor1GUID = go->GetGUID();
                    if (GetData(TYPE_DRAKKISATH) == DONE)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                case GO_DRAKKISATH_DOOR2:
                    _drakkisathDoor2GUID = go->GetGUID();
                    if (GetData(TYPE_DRAKKISATH) == DONE)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                case GO_ROOM_1_RUNE: _roomRuneGUID[0] = go->GetGUID(); break;
                case GO_ROOM_2_RUNE: _roomRuneGUID[1] = go->GetGUID(); break;
                case GO_ROOM_3_RUNE: _roomRuneGUID[2] = go->GetGUID(); break;
                case GO_ROOM_4_RUNE: _roomRuneGUID[3] = go->GetGUID(); break;
                case GO_ROOM_5_RUNE: _roomRuneGUID[4] = go->GetGUID(); break;
                case GO_ROOM_6_RUNE: _roomRuneGUID[5] = go->GetGUID(); break;
                case GO_ROOM_7_RUNE: _roomRuneGUID[6] = go->GetGUID(); break;

                case GO_ROOKERY_EGG:
                    _rookeryEggGUIDs.push_back(go->GetGUID());
                    break;
                case GO_FATHER_FLAME:
                    _fatherFlameGUID = go->GetGUID();
                    break;

                case GO_DOOR_URBS:
                    _ubrsDoorGUID = go->GetGUID();
                    if (ubrsDoorDone)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                case GO_BRAZIER01:
                    _brazierGUID[0] = go->GetGUID();
                    if (ubrsDoorDone)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                case GO_BRAZIER02:
                    _brazierGUID[1] = go->GetGUID();
                    if (ubrsDoorDone)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                case GO_BRAZIER03:
                    _brazierGUID[2] = go->GetGUID();
                    if (ubrsDoorDone)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                case GO_BRAZIER04:
                    _brazierGUID[3] = go->GetGUID();
                    if (ubrsDoorDone)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                case GO_BRAZIER05:
                    _brazierGUID[4] = go->GetGUID();
                    if (ubrsDoorDone)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                case GO_BRAZIER06:
                    _brazierGUID[5] = go->GetGUID();
                    if (ubrsDoorDone)
                        go->SetGoState(GO_STATE_ACTIVE);
                    break;
                default:
                    break;
            }
        }

        void OnCreatureCreate(Creature* creature) override
        {
            InstanceScript::OnCreatureCreate(creature);
            _encounterTracker.OnCreatureCreate(creature);

            switch (creature->GetEntry())
            {
                case NPC_LORD_VICTOR_NEFARIUS:
                    _nefariusGUID = creature->GetGUID();
                    break;
                case NPC_REND_BLACKHAND:
                    if (_rendGUID.IsEmpty()) // only save the original Rend
                        _rendGUID = creature->GetGUID();
                    break;
                case NPC_GYTH:
                    _gythGUID = creature->GetGUID();
                    break;
                case NPC_SCARSHIELD_INFILTRATOR:
                    _infiltratorGUID = creature->GetGUID();
                    break;
                case NPC_DRAKKISATH:
                    _drakkisathGUID = creature->GetGUID();
                    break;
                case NPC_THE_BEAST:
                    _beastGUID = creature->GetGUID();
                    break;
                case NPC_BLACKHAND_SUMMONER:
                case NPC_BLACKHAND_VETERAN:
                    _roomEventMobGUIDs.push_back(creature->GetGUID());
                    break;
                case NPC_BLACKHAND_INCANCERATOR:
                    _incanceratorGUIDs.push_back(creature->GetGUID());
                    break;
                case NPC_PYROGUARD_EMBERSEER:
                    _emberseerGUID = creature->GetGUID();
                    break;
                default:
                    break;
            }

            // VMaNGOS OnCreatureEvade() replacement: watch the summoned stadium creatures (see Update())
            if (creature->IsSummon() && IsClassicBrsStadiumEntry(creature->GetEntry()))
                _stadiumEvadeWatch.push_back(creature->GetGUID());
        }

        void SetData(uint32 type, uint32 data) override
        {
            switch (type)
            {
                case TYPE_ROOM_EVENT:
                    if (data == DONE)
                    {
                        TC_LOG_INFO("scripts.classic", "UBRS room event done: opening Emberseer In ({})", _emberseerInDoorGUID.IsEmpty() ? std::string("door not spawned") : _emberseerInDoorGUID.ToString());
                        DoUseDoorOrButton(_emberseerInDoorGUID);
                    }
                    _encounter[TYPE_ROOM_EVENT] = data;
                    break;
                case TYPE_EMBERSEER:
                    // VMaNGOS 2023 map event 4884 (sql/migrations/20230604191452_world.sql): the altar closes
                    // Emberseer In and the inner Doors behind the raid; a wipe reopens them; the kill opens the
                    // inner Doors and Emberseer Out.
                    if (data == IN_PROGRESS)
                    {
                        HandleGameObject(_emberseerInDoorGUID, false);
                        HandleGameObject(_emberseerCombatDoorGUID, false);
                        _emberseerCheckTimer = 1200;
                        _emberseerFightTimer = 68000;
                    }
                    else if (data == DONE)
                    {
                        HandleGameObject(_emberseerCombatDoorGUID, true);
                        HandleGameObject(_emberseerOutDoorGUID, true);
                        _emberseerCheckTimer = 0;
                        _emberseerFightTimer = 0;
                    }
                    else
                    {
                        if (GetData(TYPE_ROOM_EVENT) == DONE)
                            HandleGameObject(_emberseerInDoorGUID, true);
                        HandleGameObject(_emberseerCombatDoorGUID, true);
                        _emberseerCheckTimer = 0;
                        _emberseerFightTimer = 0;
                    }
                    _encounter[TYPE_EMBERSEER] = data;
                    break;
                case TYPE_FLAMEWREATH:
                    _encounter[TYPE_FLAMEWREATH] = data;
                    break;
                case TYPE_STADIUM:
                    // Don't set the same data twice
                    if (_encounter[type] == data)
                        break;
                    // Combat door
                    DoUseDoorOrButton(_gythEntryDoorGUID);
                    // Start event
                    if (data == IN_PROGRESS)
                        StartNextDialogueText(SAY_NEFARIUS_INTRO_1);
                    else if (data == DONE)
                    {
                        // Event complete: remove the summoned spectators
                        DespawnStadiumSpectators();
                        DoUseDoorOrButton(_gythExitDoorGUID);
                    }
                    else if (data == FAIL)
                    {
                        // Despawn Nefarius, Rend and the spectators on fail (the others are despawned on evade)
                        if (Creature* nefarius = instance->GetCreature(_nefariusGUID))
                            nefarius->DespawnOrUnsummon();
                        if (Creature* rend = instance->GetCreature(_rendGUID))
                            rend->DespawnOrUnsummon();
                        if (Creature* gyth = instance->GetCreature(_gythGUID))
                            gyth->DespawnOrUnsummon();
                        DespawnStadiumSpectators();

                        _stadiumEventTimer = 0;
                        _stadiumMobsAlive = 0;
                        _stadiumWaves = 0;
                        _stadiumWaveGroups.clear();
                    }
                    _encounter[type] = data;
                    break;
                case TYPE_VALTHALAK:
                    _encounter[TYPE_VALTHALAK] = data;
                    break;
                case TYPE_EVENT_DOOR_UBRS:
                    if (data == DONE)
                        _ubrsDoorTimer = 2000;
                    _encounter[TYPE_EVENT_DOOR_UBRS] = data;
                    break;
                case TYPE_SOLAKAR:
                    if (data == FAIL && _fatherFlameTimer != 0)
                    {
                        _fatherFlameTimer = 0;
                        _fatherFlameWaveCount = 0;
                    }
                    if (data == IN_PROGRESS)
                        _fatherFlameTimer = 5000;

                    _encounter[TYPE_SOLAKAR] = data;
                    break;
                case TYPE_DRAKKISATH:
                    if (data == DONE)
                    {
                        DoUseDoorOrButton(_drakkisathDoor1GUID);
                        DoUseDoorOrButton(_drakkisathDoor2GUID);
                    }
                    _encounter[TYPE_DRAKKISATH] = data;
                    break;
                case DATA_SORT_ROOM_EVENT_MOBS:
                    DoSortRoomEventMobs();
                    return;
                default:
                    break;
            }

            if (data == DONE)
                SaveEncounterData();
        }

        // VMaNGOS SetData64(TYPE_ROOM_EVENT, guid): a room event mob died
        void SetGuidData(uint32 type, ObjectGuid data) override
        {
            if (type == TYPE_ROOM_EVENT && GetData(TYPE_ROOM_EVENT) == IN_PROGRESS)
            {
                uint8 notEmptyRoomsCount = 0;
                for (uint8 i = 0; i < MAX_ROOMS; ++i)
                {
                    if (!_roomRuneGUID[i].IsEmpty())        // This check is used, to ensure which runes still need processing
                    {
                        _roomEventMobGUIDSorted[i].remove(data);
                        TC_LOG_INFO("scripts.classic", "UBRS room event: room {} has {} mobs left after {} died", i + 1, _roomEventMobGUIDSorted[i].size(), data.ToString());
                        if (_roomEventMobGUIDSorted[i].empty())
                        {
                            DoUseDoorOrButton(_roomRuneGUID[i]);
                            _roomRuneGUID[i].Clear();
                        }
                        else
                            ++notEmptyRoomsCount;           // found a not empty room
                    }
                }
                if (!notEmptyRoomsCount)
                    SetData(TYPE_ROOM_EVENT, DONE);
            }
        }

        uint32 GetData(uint32 type) const override
        {
            switch (type)
            {
                case TYPE_ROOM_EVENT:
                case TYPE_EMBERSEER:
                case TYPE_FLAMEWREATH:
                case TYPE_STADIUM:
                case TYPE_VALTHALAK:
                case TYPE_EVENT_DOOR_UBRS:
                case TYPE_SOLAKAR:
                case TYPE_DRAKKISATH:
                    return _encounter[type];
                default:
                    break;
            }
            return 0;
        }

        ObjectGuid GetGuidData(uint32 type) const override
        {
            switch (type)
            {
                case GO_BLACKROCK_ALTAR:
                    return _blackRockAltarGUID;
                case NPC_LORD_VICTOR_NEFARIUS:
                    return _nefariusGUID;
                case NPC_REND_BLACKHAND:
                    return _rendGUID;
                case NPC_GYTH:
                    return _gythGUID;
                case NPC_SCARSHIELD_INFILTRATOR:
                    return _infiltratorGUID;
                case GO_GYTH_COMBAT_DOOR:
                    return _gythCombatDoorGUID;
                case NPC_DRAKKISATH:
                    return _drakkisathGUID;
                case NPC_THE_BEAST:
                    return _beastGUID;
                default:
                    break;
            }
            return ObjectGuid::Empty;
        }

        void OnUnitDeath(Unit* unit) override
        {
            _encounterTracker.OnUnitDeath(unit);

            Creature* creature = unit->ToCreature();
            if (!creature)
                return;

            switch (creature->GetEntry())
            {
                case NPC_CHROMATIC_WHELP:
                case NPC_CHROMATIC_DRAGON:
                case NPC_BLACKHAND_HANDLER:
                {
                    // check if it's summoned - some npcs with the same entry are already spawned in the instance
                    if (!creature->IsSummon())
                        break;

                    // 5% chance for Rend or Lord Victor Nefarius to taunt players when one of the creature is killed (% is guesswork)
                    // Lord Victor Nefarius
                    if (urand(0, 100) < 5)
                    {
                        if (Creature* nefarius = instance->GetCreature(_nefariusGUID))
                        {
                            static std::array<uint32, 8> const nefariusTaunts =
                            {
                                SAY_NEFARIUS_TAUNT1, SAY_NEFARIUS_TAUNT2, SAY_NEFARIUS_TAUNT3, SAY_NEFARIUS_TAUNT4,
                                SAY_NEFARIUS_TAUNT5, SAY_NEFARIUS_TAUNT6, SAY_NEFARIUS_TAUNT7, SAY_NEFARIUS_TAUNT8
                            };
                            ClassicScriptText(nefariusTaunts[urand(0, uint32(nefariusTaunts.size()) - 1)], nefarius);
                        }
                    }
                    // Warchief Rend Blackhand
                    if (urand(0, 100) < 5)
                    {
                        if (Creature* rend = instance->GetCreature(_rendGUID))
                        {
                            static std::array<uint32, 4> const rendTaunts =
                            {
                                SAY_REND_TAUNT1, SAY_REND_TAUNT2, SAY_REND_TAUNT3, SAY_REND_TAUNT4
                            };
                            ClassicScriptText(rendTaunts[urand(0, uint32(rendTaunts.size()) - 1)], rend);
                        }
                    }
                    --_stadiumMobsAlive;
                    if (!_stadiumMobsAlive && (_stadiumWaves == MAX_STADIUM_WAVES))
                        DoSendNextStadiumWave();
                    break;
                }
                case NPC_GYTH:
                case NPC_REND_BLACKHAND:
                    --_stadiumMobsAlive;
                    if (_stadiumMobsAlive == 0)
                        StartNextDialogueText(SAY_NEFARIUS_VICTORY);
                    break;
                case NPC_BLACKHAND_SUMMONER:
                case NPC_BLACKHAND_VETERAN:
                    // Dragonspire Hall room event: every mob of a room must die before its rune lights and, once all
                    // seven rooms are empty, Emberseer In opens. The Veteran script reports its own death as well
                    // (classic_ubrs_trash.cpp); the Summoner runs SmartAI and reported nothing, so a room holding one
                    // never emptied and the door stayed shut.
                    SetGuidData(TYPE_ROOM_EVENT, creature->GetGUID());
                    break;
                case NPC_PYROGUARD_EMBERSEER:
                    // A forced despawn also passes through JUST_DIED, so only a death during the event counts.
                    if (GetData(TYPE_EMBERSEER) == IN_PROGRESS)
                        SetData(TYPE_EMBERSEER, DONE);
                    break;
                default:
                    break;
            }
        }

        // Spell 16533 "Emberseer Start" (effect SEND_EVENT 4884), cast by the Blackrock Altar once three grouped players
        // have used it. VMaNGOS 2023 event_scripts 4884: start the map event, close the doors, free the Incarcerators
        // (immune flags off, combat pulse) and move Pyroguard Emberseer to his growth phase.
        void ProcessEvent(WorldObject* obj, uint32 eventId, WorldObject* invoker) override
        {
            if (eventId == EVENT_SPIDER_EGGS_LOOTED)
            {
                HatchSpiderEggs(obj, invoker);
                return;
            }

            if (eventId != EVENT_EMBERSEER_START)
                return;
            if (GetData(TYPE_EMBERSEER) == IN_PROGRESS || GetData(TYPE_EMBERSEER) == DONE)
                return;
            Creature* emberseer = instance->GetCreature(_emberseerGUID);
            if (!emberseer || !emberseer->IsAlive())
                return;

            SetData(TYPE_EMBERSEER, IN_PROGRESS);
            for (ObjectGuid const& guid : _incanceratorGUIDs)
            {
                Creature* incarcerator = instance->GetCreature(guid);
                if (!incarcerator || !incarcerator->IsAlive())
                    continue;
                incarcerator->InterruptNonMeleeSpells(false);
                incarcerator->SetImmuneToAll(false);
                if (incarcerator->AI())
                    incarcerator->AI()->DoZoneInCombat();
            }
            // SmartAI 9816: "on data set 1 1" enters the growth phase (5 / 35 / 65 s texts, Growing, Full Strength).
            if (emberseer->AI())
                emberseer->AI()->SetData(DATA_EMBERSEER_START, 1);
        }

        // A Spire Spider Egg (chest 175588 / 175606) was looted: its Data6 event 4799 hatches Spire Spiderlings on the looter.
        // VMaNGOS ran the event from its base database (event_scripts 4799, not in the repo data); the count follows the audit's
        // reading of that row (three per egg). They despawn with their corpses or after five minutes unlooted.
        void HatchSpiderEggs(WorldObject* egg, WorldObject* looter)
        {
            if (!egg)
                return;

            Unit* target = looter ? looter->ToUnit() : nullptr;
            for (uint8 i = 0; i < SPIDER_EGG_SPIDERLINGS; ++i)
            {
                Position const pos = egg->GetNearPosition(2.0f, float(i) * 2.0f * float(M_PI) / float(SPIDER_EGG_SPIDERLINGS));
                if (Creature* spiderling = egg->SummonCreature(NPC_SPIRE_SPIDERLING, pos, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 5min))
                    if (target && spiderling->AI())
                        spiderling->AI()->AttackStart(target);
            }
        }

        // VMaNGOS fail script 48842 (no player alive within 100 yards): Emberseer and the Incarcerators respawn caged,
        // the doors reopen. The fight itself starts 68 s after the altar (65 s growth + 3 s yell), when the flags the
        // template sets (immune to players, uninteractible) come off and he engages the room.
        void UpdateEmberseerEvent(uint32 diff)
        {
            if (GetData(TYPE_EMBERSEER) != IN_PROGRESS)
                return;

            if (_emberseerFightTimer)
            {
                if (_emberseerFightTimer <= diff)
                {
                    _emberseerFightTimer = 0;
                    if (Creature* emberseer = instance->GetCreature(_emberseerGUID))
                    {
                        emberseer->SetImmuneToPC(false);
                        emberseer->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                        if (emberseer->AI())
                            emberseer->AI()->DoZoneInCombat();
                    }
                }
                else
                    _emberseerFightTimer -= diff;
            }

            if (_emberseerCheckTimer <= diff)
            {
                _emberseerCheckTimer = 1200;
                Creature* emberseer = instance->GetCreature(_emberseerGUID);
                if (!emberseer)
                    return;
                bool playerNear = false;
                for (MapReference const& ref : instance->GetPlayers())
                {
                    Player* player = ref.GetSource();
                    if (player && player->IsAlive() && !player->IsGameMaster() && player->IsWithinDist(emberseer, 100.0f))
                    {
                        playerNear = true;
                        break;
                    }
                }
                if (playerNear)
                    return;

                // Wipe: reset everything to the caged state. Not Respawn(true): a forced respawn of a living creature
                // goes through JUST_DIED and would count as the boss kill for the encounter tracker.
                SetData(TYPE_EMBERSEER, NOT_STARTED);
                if (emberseer->IsAlive())
                {
                    if (emberseer->AI())
                        emberseer->AI()->EnterEvadeMode();
                    emberseer->RemoveAurasDueToSpell(SPELL_EMBERSEER_GROWING);
                    emberseer->RemoveAurasDueToSpell(SPELL_EMBERSEER_FULL_STRENGTH);
                    emberseer->RemoveAurasDueToSpell(SPELL_FREEZE_ANIM);
                    emberseer->CastSpell(emberseer, SPELL_ENCAGED_EMBERSEER, true);
                    emberseer->SetImmuneToPC(true);
                    emberseer->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                    emberseer->RestoreFaction();
                    emberseer->SetFullHealth();
                }
                for (ObjectGuid const& guid : _incanceratorGUIDs)
                {
                    if (Creature* incarcerator = instance->GetCreature(guid))
                    {
                        if (!incarcerator->IsAlive())
                            incarcerator->Respawn(true);
                        else
                        {
                            incarcerator->SetImmuneToAll(true);
                            if (incarcerator->AI())
                                incarcerator->AI()->EnterEvadeMode();
                        }
                    }
                }
            }
            else
                _emberseerCheckTimer -= diff;
        }

        void Update(uint32 diff) override
        {
            _encounterTracker.Update(diff);

            DialogueUpdate(diff);

            if (_stadiumEventTimer)
            {
                if (_stadiumEventTimer <= diff)
                    DoSendNextStadiumWave();
                else
                    _stadiumEventTimer -= diff;
            }

            if (_stadiumCheckTimer <= diff)
            {
                _stadiumCheckTimer = 1000;
                CheckStadiumCreatures();
            }
            else
                _stadiumCheckTimer -= diff;

            // The door also opens by proximity: a player carrying the Seal of Ascension within 12 yards of it. The area trigger
            // (2046) stays as it was, but it fires only when the client reports the trigger volume, and it skipped GM players.
            if (GetData(TYPE_EVENT_DOOR_UBRS) != DONE || GetData(TYPE_ROOM_EVENT) == NOT_STARTED)
            {
                if (_ubrsDoorCheckTimer <= diff)
                {
                    _ubrsDoorCheckTimer = 1000;
                    // The rune-room event starts with the same area trigger (2046 sorts the room mobs): start it as soon as a
                    // player is alive in the instance, otherwise the rooms never count and the door behind them stays shut.
                    if (GetData(TYPE_ROOM_EVENT) == NOT_STARTED)
                        for (MapReference const& ref : instance->GetPlayers())
                            if (Player* player = ref.GetSource())
                                if (player->IsAlive())
                                {
                                    DoSortRoomEventMobs();
                                    break;
                                }
                    if (GetData(TYPE_EVENT_DOOR_UBRS) != DONE)
                        if (GameObject* door = instance->GetGameObject(_ubrsDoorGUID))
                        for (MapReference const& ref : instance->GetPlayers())
                        {
                            Player* player = ref.GetSource();
                            if (player && player->IsAlive() && player->IsWithinDist(door, 12.0f) && player->HasItemCount(ITEM_SEAL_OF_ASCENSION, 1))
                            {
                                SetData(TYPE_EVENT_DOOR_UBRS, DONE);
                                break;
                            }
                        }
                }
                else
                    _ubrsDoorCheckTimer -= diff;
            }

            if (_ubrsDoorTimer)
            {
                if (_ubrsDoorTimer <= diff)
                {
                    switch (_ubrsDoorStep)
                    {
                        case 0:
                            DoUseDoorOrButton(_brazierGUID[0]);
                            DoUseDoorOrButton(_brazierGUID[1]);
                            _ubrsDoorTimer = 3000;
                            ++_ubrsDoorStep;
                            break;
                        case 1:
                            DoUseDoorOrButton(_brazierGUID[2]);
                            DoUseDoorOrButton(_brazierGUID[3]);
                            _ubrsDoorTimer = 3000;
                            ++_ubrsDoorStep;
                            break;
                        case 2:
                            DoUseDoorOrButton(_brazierGUID[4]);
                            DoUseDoorOrButton(_brazierGUID[5]);
                            _ubrsDoorTimer = 3000;
                            ++_ubrsDoorStep;
                            break;
                        case 3:
                            DoUseDoorOrButton(_ubrsDoorGUID);
                            _ubrsDoorStep = 0;
                            _ubrsDoorTimer = 0;
                            break;
                        default:
                            break;
                    }
                }
                else
                    _ubrsDoorTimer -= diff;
            }

            UpdateEmberseerEvent(diff);

            if (GetData(TYPE_SOLAKAR) == IN_PROGRESS)
            {
                if (_fatherFlameTimer <= diff)
                {
                    Position const hatcherPos1(55.232342f, -265.751282f, 93.883f, 5.0f);
                    Position const hatcherPos2(60.011333f, -263.914703f, 94.022f, 5.0f);

                    if (_fatherFlameWaveCount == 0) // First wave should be a Rookery Hatcher and there is a text that it has to say.
                    {
                        if (Creature* firstHatcher = SummonRookeryCreature(NPC_ROOKERY_HATCHER, hatcherPos1))
                            ClassicScriptText(SAY_ROOKERY_EVENT_START, firstHatcher);

                        SummonRookeryCreature(NPC_ROOKERY_HATCHER, hatcherPos2);
                        _fatherFlameTimer = urand(30000, 40000);
                        ++_fatherFlameWaveCount;
                    }
                    else if (_fatherFlameWaveCount < 5)
                    {
                        switch (urand(0, 2))
                        {
                            case 0:
                                SummonRookeryCreature(NPC_ROOKERY_GUARDIAN, hatcherPos1);
                                SummonRookeryCreature(NPC_ROOKERY_GUARDIAN, hatcherPos2);
                                break;
                            case 1:
                                SummonRookeryCreature(NPC_ROOKERY_HATCHER, hatcherPos1);
                                SummonRookeryCreature(NPC_ROOKERY_HATCHER, hatcherPos2);
                                break;
                            case 2:
                                SummonRookeryCreature(NPC_ROOKERY_GUARDIAN, hatcherPos1);
                                SummonRookeryCreature(NPC_ROOKERY_HATCHER, hatcherPos2);
                                break;
                            default:
                                break;
                        }
                        _fatherFlameTimer = urand(30000, 40000);
                        ++_fatherFlameWaveCount;
                    }
                    else
                    {
                        SummonRookeryCreature(NPC_SOLAKAR, Position(43.7685f, -259.82f, 91.6483f, 0.0f));
                        SetData(TYPE_SOLAKAR, DONE);
                        _fatherFlameTimer = 0;
                    }
                }
                else
                    _fatherFlameTimer -= diff;
            }
        }

    private:
        void SaveEncounterData()
        {
            for (uint32 i = 0; i < INSTANCE_BRS_MAX_ENCOUNTER; ++i)
                if (uint32(*_saved[i]) != _encounter[i])
                    *_saved[i] = _encounter[i];
        }

        Creature* SummonRookeryCreature(uint32 entry, Position const& pos)
        {
            TempSummon* summon = instance->SummonCreature(entry, pos);
            if (summon)
                summon->SetTempSummonType(TEMPSUMMON_DEAD_DESPAWN);
            return summon;
        }

        void DoSortRoomEventMobs()
        {
            if (GetData(TYPE_ROOM_EVENT) != NOT_STARTED)
                return;

            for (uint8 i = 0; i < MAX_ROOMS; ++i)
            {
                GameObject* rune = instance->GetGameObject(_roomRuneGUID[i]);
                if (!rune)
                    TC_LOG_ERROR("scripts.classic", "UBRS room event: rune {} (room {}) is not spawned in this instance", i + 1, i + 1);
                if (rune)
                {
                    for (ObjectGuid const& guid : _roomEventMobGUIDs)
                    {
                        if (Creature* creature = instance->GetCreature(guid))
                        {
                            if (creature->IsAlive() && creature->GetDistance(rune) < 10.0f)
                                _roomEventMobGUIDSorted[i].push_back(guid);
                        }
                    }
                }
            }

            TC_LOG_INFO("scripts.classic", "UBRS room event started: {} room mobs registered, rooms hold {} {} {} {} {} {} {}", _roomEventMobGUIDs.size(),
                _roomEventMobGUIDSorted[0].size(), _roomEventMobGUIDSorted[1].size(), _roomEventMobGUIDSorted[2].size(), _roomEventMobGUIDSorted[3].size(),
                _roomEventMobGUIDSorted[4].size(), _roomEventMobGUIDSorted[5].size(), _roomEventMobGUIDSorted[6].size());
            SetData(TYPE_ROOM_EVENT, IN_PROGRESS);
        }

        // ---- VMaNGOS DialogueHelper replacement ----
        void StartNextDialogueText(uint32 textEntry)
        {
            for (uint32 i = 0; aStadiumDialogue[i].TextEntry; ++i)
            {
                if (aStadiumDialogue[i].TextEntry == textEntry)
                {
                    _dialogueIndex = i;
                    DoNextDialogueStep();
                    return;
                }
            }
        }

        void DoNextDialogueStep()
        {
            ClassicBrsDialogueEntry const& entry = aStadiumDialogue[_dialogueIndex];

            // Last Dialogue Entry done?
            if (!entry.TextEntry)
            {
                _dialogueTimer = 0;
                return;
            }

            _dialogueTimer = entry.Timer;

            if (entry.SpeakerEntry)
                if (Creature* speaker = GetSpeakerByEntry(entry.SpeakerEntry))
                    ClassicScriptText(entry.TextEntry, speaker);

            ++_dialogueIndex;
            JustDidDialogueStep(entry.TextEntry);
        }

        void DialogueUpdate(uint32 diff)
        {
            if (_dialogueTimer)
            {
                if (_dialogueTimer <= diff)
                    DoNextDialogueStep();
                else
                    _dialogueTimer -= diff;
            }
        }

        Creature* GetSpeakerByEntry(uint32 entry)
        {
            switch (entry)
            {
                case NPC_LORD_VICTOR_NEFARIUS:
                    return instance->GetCreature(_nefariusGUID);
                case NPC_REND_BLACKHAND:
                    return instance->GetCreature(_rendGUID);
                default:
                    break;
            }

            return nullptr;
        }

        void JustDidDialogueStep(uint32 entry)
        {
            switch (entry)
            {
                case NPC_BLACKHAND_HANDLER:
                    _stadiumEventTimer = 1000;
                    // Move the two near the balcony
                    if (Creature* rend = instance->GetCreature(_rendGUID))
                        rend->SetFacingTo(aStadiumLocs[5].GetOrientation());
                    if (Creature* nefarius = instance->GetCreature(_nefariusGUID))
                    {
                        nefarius->GetMotionMaster()->MovePoint(0, aStadiumLocs[5], true, aStadiumLocs[5].GetOrientation());
                        // Summon the spectators and move them to the western balcony
                        for (uint8 i = 0; i < 12; ++i)
                        {
                            if (Creature* spectator = nefarius->SummonCreature(aStadiumSpectators[i], aSpectatorsSpawnLocs[i], TEMPSUMMON_DEAD_DESPAWN, 0s))
                            {
                                // TODO(classic): VMaNGOS SetDetectionDistance(1.0f) (spectators do not aggro from the balcony) has no TC equivalent
                                spectator->SetNoCallAssistance(true);
                                spectator->SetWalk(false);
                                spectator->SetHomePosition(aSpectatorsTargetLocs[i]);
                                spectator->GetMotionMaster()->MovePoint(0, aSpectatorsTargetLocs[i], true, aSpectatorsTargetLocs[i].GetOrientation());
                                _stadiumSpectatorGUIDs.push_back(spectator->GetGUID());
                            }
                        }
                    }
                    break;
                case SAY_NEFARIUS_WARCHIEF:
                    // Prepare for Gyth
                    if (Creature* rend = instance->GetCreature(_rendGUID))
                    {
                        rend->DespawnOrUnsummon(5s);
                        rend->SetWalk(false);
                        rend->GetMotionMaster()->MovePoint(0, aStadiumLocs[6].GetPositionX(), aStadiumLocs[6].GetPositionY(), aStadiumLocs[6].GetPositionZ());
                    }
                    _stadiumEventTimer = 30000;
                    break;
                case SAY_NEFARIUS_PACING:
                    // Make Lord Nefarius walk back and forth while Rend is preparing Gyth
                    if (Creature* nefarius = instance->GetCreature(_nefariusGUID))
                        nefarius->GetMotionMaster()->MovePath(CLASSIC_BRS_PATH_NEFARIUS, true);
                    break;
                case SAY_NEFARIUS_VICTORY:
                    SetData(TYPE_STADIUM, DONE);
                    break;
                case NPC_REND_BLACKHAND:
                    // Despawn Nefarius
                    if (Creature* nefarius = instance->GetCreature(_nefariusGUID))
                    {
                        nefarius->DespawnOrUnsummon(5s);
                        nefarius->GetMotionMaster()->MovePoint(0, aStadiumLocs[6].GetPositionX(), aStadiumLocs[6].GetPositionY(), aStadiumLocs[6].GetPositionZ());
                    }
                    break;
                default:
                    break;
            }
        }

        void DespawnStadiumSpectators()
        {
            for (ObjectGuid const& guid : _stadiumSpectatorGUIDs)
                if (Creature* spectator = instance->GetCreature(guid))
                    spectator->DespawnOrUnsummon();
            _stadiumSpectatorGUIDs.clear();
        }

        void DoSendNextStadiumWave()
        {
            if (_stadiumWaves < MAX_STADIUM_WAVES)
            {
                // Send current wave mobs
                if (Creature* nefarius = instance->GetCreature(_nefariusGUID))
                {
                    Creature* firstMob = nullptr;
                    std::vector<ObjectGuid> waveGroup;
                    for (uint8 i = 0; i < MAX_STADIUM_MOBS_PER_WAVE; ++i)
                    {
                        if (aStadiumEventNpcs[_stadiumWaves][i] == 0)
                            continue;

                        Position pos = nefarius->GetRandomPoint(aStadiumLocs[0], 7.0f);
                        // Halfcircle - suits better the rectangular form
                        pos.Relocate(std::min(aStadiumLocs[0].GetPositionX(), pos.GetPositionX()), pos.GetPositionY(), pos.GetPositionZ(), 0.0f);
                        if (Creature* temp = nefarius->SummonCreature(aStadiumEventNpcs[_stadiumWaves][i], pos, TEMPSUMMON_DEAD_DESPAWN, 0s))
                        {
                            if (!firstMob)
                                firstMob = temp;
                            else
                            {
                                // TODO(classic): VMaNGOS JoinCreatureGroup(firstMob, frand(3, 5), i, OPTION_FORMATION_MOVE | OPTION_AGGRO_TOGETHER);
                                // TC formations need DB spawn ids, so the members follow the leader and the aggro-together part is
                                // emulated in CheckStadiumCreatures().
                                temp->GetMotionMaster()->MoveFollow(firstMob, frand(3.0f, 5.0f), ChaseAngle(float(i)));
                            }
                            waveGroup.push_back(temp->GetGUID());
                            ++_stadiumMobsAlive;
                        }
                    }

                    if (firstMob)
                        firstMob->GetMotionMaster()->MovePath(CLASSIC_BRS_PATH_STADIUM_WAVES, false);

                    if (!waveGroup.empty())
                        _stadiumWaveGroups.push_back(std::move(waveGroup));
                }

                DoUseDoorOrButton(_gythCombatDoorGUID);
            }
            // All waves are cleared - start Gyth intro
            else if (_stadiumWaves == MAX_STADIUM_WAVES)
                StartNextDialogueText(SAY_NEFARIUS_LOSE1);
            else
            {
                // Send Gyth
                if (Creature* nefarius = instance->GetCreature(_nefariusGUID))
                {
                    // Stop Lord Nefarius from moving and put him back in place
                    nefarius->GetMotionMaster()->MoveIdle();
                    nefarius->GetMotionMaster()->MovePoint(0, aStadiumLocs[5], true, aStadiumLocs[5].GetOrientation());

                    if (Creature* gyth = nefarius->SummonCreature(NPC_GYTH, aStadiumLocs[1].GetPositionX(), aStadiumLocs[1].GetPositionY(), aStadiumLocs[1].GetPositionZ(), 0.0f, TEMPSUMMON_DEAD_DESPAWN, 0s))
                    {
                        gyth->SetWalk(false);
                        gyth->GetMotionMaster()->MovePath(CLASSIC_BRS_PATH_GYTH, true);
                    }
                }

                // Set this to 2, because Rend will be summoned later during the fight
                // TODO(classic): Rend is summoned by Gyth's own script, which is not part of the VMaNGOS scripts folder.
                _stadiumMobsAlive = 2;

                DoUseDoorOrButton(_gythCombatDoorGUID);
            }

            ++_stadiumWaves;

            // Stop the timer when all the waves have been sent
            if (_stadiumWaves >= MAX_STADIUM_WAVES)
                _stadiumEventTimer = 0;
            else
                _stadiumEventTimer = 60000;
        }

        // Replaces VMaNGOS InstanceData::OnCreatureEvade (no TC hook): a summoned stadium creature that evades fails the event.
        // Also emulates OPTION_AGGRO_TOGETHER for the wave groups.
        void CheckStadiumCreatures()
        {
            std::vector<ObjectGuid> watch = _stadiumEvadeWatch;
            _stadiumEvadeWatch.clear();
            bool failed = false;
            for (ObjectGuid const& guid : watch)
            {
                Creature* creature = instance->GetCreature(guid);
                if (!creature || !creature->IsAlive())
                    continue;

                if (!failed && creature->IsInEvadeMode())
                {
                    SetData(TYPE_STADIUM, FAIL);
                    creature->DespawnOrUnsummon();
                    failed = true;
                    continue;
                }

                if (failed)
                {
                    // VMaNGOS: every other stadium creature evades as well and is despawned in OnCreatureEvade
                    creature->DespawnOrUnsummon();
                    continue;
                }

                _stadiumEvadeWatch.push_back(guid);
            }

            for (auto itr = _stadiumWaveGroups.begin(); itr != _stadiumWaveGroups.end();)
            {
                Unit* groupVictim = nullptr;
                bool anyAlive = false;
                for (ObjectGuid const& guid : *itr)
                {
                    if (Creature* member = instance->GetCreature(guid))
                    {
                        if (!member->IsAlive())
                            continue;
                        anyAlive = true;
                        if (!groupVictim && member->IsInCombat())
                            groupVictim = member->GetVictim();
                    }
                }

                if (!anyAlive)
                {
                    itr = _stadiumWaveGroups.erase(itr);
                    continue;
                }

                if (groupVictim)
                    for (ObjectGuid const& guid : *itr)
                        if (Creature* member = instance->GetCreature(guid))
                            if (member->IsAlive() && !member->IsInCombat() && member->AI())
                                member->AI()->AttackStart(groupVictim);

                ++itr;
            }
        }

        std::array<uint32, INSTANCE_BRS_MAX_ENCOUNTER> _encounter;
        std::array<std::unique_ptr<PersistentInstanceScriptValue<uint32>>, INSTANCE_BRS_MAX_ENCOUNTER> _saved;

        ObjectGuid _nefariusGUID;
        ObjectGuid _gythGUID;
        ObjectGuid _infiltratorGUID;
        ObjectGuid _drakkisathGUID;
        ObjectGuid _beastGUID;
        ObjectGuid _rendGUID;
        ObjectGuid _emberseerGUID;
        uint32 _emberseerCheckTimer;
        uint32 _emberseerFightTimer;

        ObjectGuid _emberseerInDoorGUID;
        ObjectGuid _emberseerCombatDoorGUID;
        ObjectGuid _emberseerOutDoorGUID;
        ObjectGuid _gythEntryDoorGUID;
        ObjectGuid _gythCombatDoorGUID;
        ObjectGuid _gythExitDoorGUID;
        ObjectGuid _drakkisathDoor1GUID;
        ObjectGuid _drakkisathDoor2GUID;

        ObjectGuid _fatherFlameGUID;
        uint32 _fatherFlameTimer;
        uint32 _fatherFlameWaveCount;

        ObjectGuid _ubrsDoorGUID;
        std::array<ObjectGuid, 6> _brazierGUID;

        ObjectGuid _blackRockAltarGUID;

        uint32 _ubrsDoorTimer;
        uint32 _ubrsDoorStep;
        uint32 _ubrsDoorCheckTimer = 1000;

        uint32 _stadiumEventTimer;
        uint8 _stadiumWaves;
        uint8 _stadiumMobsAlive;
        uint32 _stadiumCheckTimer;

        std::array<ObjectGuid, MAX_ROOMS> _roomRuneGUID;
        std::array<std::list<ObjectGuid>, MAX_ROOMS> _roomEventMobGUIDSorted;
        std::list<ObjectGuid> _roomEventMobGUIDs;
        std::list<ObjectGuid> _incanceratorGUIDs;   // VMaNGOS GetIncanceratorGUIDList() - used by scripts outside this folder
        std::list<ObjectGuid> _rookeryEggGUIDs;     // VMaNGOS GetRookeryEggGUIDList() - used by scripts outside this folder
        std::vector<ObjectGuid> _stadiumSpectatorGUIDs;
        std::vector<ObjectGuid> _stadiumEvadeWatch;
        std::vector<std::vector<ObjectGuid>> _stadiumWaveGroups;

        bool _bannokSpawned;

        uint32 _dialogueIndex;
        uint32 _dialogueTimer;
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new classic_instance_blackrock_spire_InstanceScript(map);
    }
};

/*######
## at_blackrock_spire
######*/

class classic_at_blackrock_spire : public AreaTriggerScript
{
public:
    classic_at_blackrock_spire() : AreaTriggerScript("classic_at_blackrock_spire") { }

    bool OnTrigger(Player* player, AreaTriggerEntry const* areaTrigger) override
    {
        if (!player->IsAlive() || player->IsGameMaster())
            return false;

        InstanceScript* instance = player->GetInstanceScript();
        if (!instance)
            return false;

        switch (areaTrigger->ID)
        {
            case AREATRIGGER_ENTER_UBRS:
                instance->SetData(DATA_SORT_ROOM_EVENT_MOBS, 0);
                if (player->HasItemCount(ITEM_SEAL_OF_ASCENSION, 1)) // the player has the Seal of Ascension
                    if (instance->GetData(TYPE_EVENT_DOOR_UBRS) != DONE)
                        instance->SetData(TYPE_EVENT_DOOR_UBRS, DONE);
                break;
            case AREATRIGGER_STADIUM:
            {
                if (instance->GetData(TYPE_STADIUM) == IN_PROGRESS || instance->GetData(TYPE_STADIUM) == DONE)
                    return false;

                // Respawn Nefarius and Rend for the dialogue event if they are not spawned already.
                if (Creature* nefarius = player->GetMap()->GetCreature(instance->GetGuidData(NPC_LORD_VICTOR_NEFARIUS)))
                    if (!nefarius->IsAlive())
                        nefarius->Respawn();
                if (Creature* rend = player->GetMap()->GetCreature(instance->GetGuidData(NPC_REND_BLACKHAND)))
                    if (!rend->IsAlive())
                        rend->Respawn();

                instance->SetData(TYPE_STADIUM, IN_PROGRESS);
                break;
            }
            default:
                break;
        }
        return false;
    }
};

/*######
## go_father_flame
######*/

// VMaNGOS GameObjectAI::OnUse -> TC GameObjectAI::OnGossipHello (called from GameObject::Use)
struct classic_go_father_flame : public GameObjectAI
{
    classic_go_father_flame(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* /*player*/) override
    {
        if (InstanceScript* instance = me->GetInstanceScript())
            if (instance->GetData(TYPE_SOLAKAR) != IN_PROGRESS && instance->GetData(TYPE_SOLAKAR) != DONE)
                if (Creature* drakki = me->GetMap()->GetCreature(instance->GetGuidData(NPC_DRAKKISATH)))
                    if (drakki->IsAlive())
                        instance->SetData(TYPE_SOLAKAR, IN_PROGRESS);
        return true;
    }
};

/*######
## at_ubrs_the_beast
######*/

class classic_at_ubrs_the_beast : public AreaTriggerScript
{
public:
    classic_at_ubrs_the_beast() : AreaTriggerScript("classic_at_ubrs_the_beast") { }

    bool OnTrigger(Player* player, AreaTriggerEntry const* /*areaTrigger*/) override
    {
        if (!player->IsAlive())
            return false;

        if (InstanceScript* instance = player->GetInstanceScript())
            if (Creature* beast = player->GetMap()->GetCreature(instance->GetGuidData(NPC_THE_BEAST)))
                if (beast->IsAlive() && !beast->IsInCombat() && beast->AI())
                    beast->AI()->AttackStart(player);

        return false;
    }
};

/*######
## spell_ubrs_freeze_rookery_egg
######*/

// 15748 - Freeze Rookery Egg
// 16028 - Freeze Rookery Egg - Prototype
class classic_spell_ubrs_freeze_rookery_egg : public SpellScript
{
    void HandleFreezeEgg(SpellEffIndex effIndex)
    {
        GameObject* go = GetHitGObj();
        if (!go)
            return;

        // VMaNGOS: skip the default effect 0 on the egg and play the "frozen" alternative door state instead
        PreventHitDefaultEffect(effIndex);
        if (go->getLootState() == GO_READY)
            go->UseDoorOrButton(0, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_ubrs_freeze_rookery_egg::HandleFreezeEgg, EFFECT_0, SPELL_EFFECT_ANY);
    }
};

/*######
## go_blackrock_altar
######*/

// The Blackrock Altar (175706, summoning ritual, three casters). The core ritual completes the instant the third player has
// used it, so a bot that walks up and taps it looks like a lockbox. Here every user takes the altar spell (16532, "Emberseer
// Object Visual": a 600 s aura, not a channel) and must keep it and stay at the altar; the ritual completes (event 4884) only
// after three grouped players have held it together for three seconds. A user who dies, walks off, loses the aura or waits
// two minutes alone leaves the ritual, and the aura comes off whoever leaves.
struct classic_go_blackrock_altar : public GameObjectAI
{
    enum
    {
        SPELL_ALTAR_CHANNEL     = 16532,
        EVENT_ALTAR_COMPLETE    = 4884,
        ALTAR_CASTERS           = 3,
        ALTAR_RANGE             = 8,
        ALTAR_HOLD_MS           = 3000,
        ALTAR_WAIT_MS           = 120000,
        ALTAR_TICK_MS           = 500,
        ALTAR_CHANNEL_MS        = 5000
    };

    struct AltarUser
    {
        ObjectGuid guid;
        uint32 waitedMs = 0;
        uint32 channelMs = 0;
    };

    classic_go_blackrock_altar(GameObject* go) : GameObjectAI(go), _tickMs(ALTAR_TICK_MS), _holdMs(0) { }

    bool OnGossipHello(Player* player) override
    {
        InstanceScript* instance = me->GetInstanceScript();
        if (!instance || instance->GetData(TYPE_EMBERSEER) == IN_PROGRESS || instance->GetData(TYPE_EMBERSEER) == DONE)
            return true;

        Prune();
        if (!_users.empty())
        {
            Player* first = ObjectAccessor::GetPlayer(*me, _users.front().guid);
            if (first && !player->IsInSameRaidWith(first))
                return true;
        }

        for (AltarUser const& user : _users)
            if (user.guid == player->GetGUID())
                return true;

        AltarUser user;
        user.guid = player->GetGUID();
        _users.push_back(user);
        player->CastSpell(player, SPELL_ALTAR_CHANNEL, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        ShowChannel(player, _users.back());
        return true;
    }

    void UpdateAI(uint32 diff) override
    {
        if (_users.empty())
            return;

        if (_tickMs > diff)
        {
            _tickMs -= diff;
            return;
        }
        _tickMs = ALTAR_TICK_MS;

        for (AltarUser& user : _users)
        {
            user.waitedMs += ALTAR_TICK_MS;
            // The altar spell is an aura, so the channel (cast bar and beam to the altar) is drawn by hand and renewed
            // every few seconds for as long as the user stays in the ritual.
            if (user.channelMs <= ALTAR_TICK_MS)
            {
                if (Player* player = ObjectAccessor::GetPlayer(*me, user.guid))
                    ShowChannel(player, user);
            }
            else
                user.channelMs -= ALTAR_TICK_MS;
        }

        size_t const before = _users.size();
        Prune();
        if (_users.size() != before)
            _holdMs = 0;

        if (_users.size() < ALTAR_CASTERS)
        {
            _holdMs = 0;
            return;
        }

        _holdMs += ALTAR_TICK_MS;
        if (_holdMs < ALTAR_HOLD_MS)
            return;

        Player* starter = ObjectAccessor::GetPlayer(*me, _users.front().guid);
        for (AltarUser const& user : _users)
            if (Player* player = ObjectAccessor::GetPlayer(*me, user.guid))
            {
                player->RemoveAurasDueToSpell(SPELL_ALTAR_CHANNEL);
                HideChannel(player);
            }
        _users.clear();
        _holdMs = 0;
        if (starter)
            GameEvents::Trigger(EVENT_ALTAR_COMPLETE, starter, me);
    }

private:
    void ShowChannel(Player* player, AltarUser& user)
    {
        SpellCastVisual visual;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(SPELL_ALTAR_CHANNEL, me->GetMap()->GetDifficultyID());
        if (info)
            visual.SpellXSpellVisualID = player->GetCastSpellXSpellVisualId(info);
        // the beam needs a spell visual: when this logs "visual 0" (or no spell info) the client has nothing to draw
        if (!user.waitedMs)
            TC_LOG_INFO("scripts.classic", "UBRS altar: {} channels spell {}: spell info {}, visual {}, difficulty {}", player->GetName(), uint32(SPELL_ALTAR_CHANNEL),
                info ? "found" : "MISSING", visual.SpellXSpellVisualID, uint32(me->GetMap()->GetDifficultyID()));
        player->ClearChannelObjects();
        player->AddChannelObject(me->GetGUID());
        player->SetChannelSpellId(SPELL_ALTAR_CHANNEL);
        player->SetChannelVisual(visual);
        player->SetChannelSpellData(GameTime::GetGameTimeMS(), ALTAR_CHANNEL_MS);
        user.channelMs = ALTAR_CHANNEL_MS - 1000;
    }

    static void HideChannel(Player* player)
    {
        player->ClearChannelObjects();
        player->SetChannelSpellId(0);
        player->SetChannelVisual({});
        player->SetChannelSpellData(0, 0);
    }

    void Prune()
    {
        _users.erase(std::remove_if(_users.begin(), _users.end(), [this](AltarUser const& user)
        {
            Player* player = ObjectAccessor::GetPlayer(*me, user.guid);
            bool const leaves = !player || !player->IsAlive() || !player->IsWithinDist(me, float(ALTAR_RANGE))
                || !player->HasAura(SPELL_ALTAR_CHANNEL) || (_users.size() < ALTAR_CASTERS && user.waitedMs > ALTAR_WAIT_MS);
            if (leaves && player)
            {
                player->RemoveAurasDueToSpell(SPELL_ALTAR_CHANNEL);
                HideChannel(player);
            }
            return leaves;
        }), _users.end());
    }

    std::vector<AltarUser> _users;
    uint32 _tickMs;
    uint32 _holdMs;
};

/*######
## go_rookery_egg
######*/

// Rookery Egg (175124). The vanilla chain is trap spell 16474 -> trap 175622 -> spell 15745 -> Rookery Whelp 10161 and depends
// on those spells existing in the client data. The egg's trap radius is zeroed in the database so the core trap stays quiet and
// this script hatches the whelp itself: a living player within three yards of an egg that is not frozen (VMaNGOS radius 3).
struct classic_go_rookery_egg : public GameObjectAI
{
    enum
    {
        NPC_ROOKERY_WHELP   = 10161,
        EGG_RADIUS          = 3,
        EGG_TICK_MS         = 500
    };

    classic_go_rookery_egg(GameObject* go) : GameObjectAI(go), _tickMs(EGG_TICK_MS) { }

    void UpdateAI(uint32 diff) override
    {
        if (_tickMs > diff)
        {
            _tickMs -= diff;
            return;
        }
        _tickMs = EGG_TICK_MS;

        if (!me->isSpawned() || me->getLootState() != GO_READY || me->GetGoState() != GO_STATE_READY)
            return;

        for (MapReference const& ref : me->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || !player->IsAlive() || !player->IsWithinDist(me, float(EGG_RADIUS)))
                continue;

            if (Creature* whelp = me->SummonCreature(NPC_ROOKERY_WHELP, me->GetPosition(), TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30s))
                if (whelp->AI())
                    whelp->AI()->AttackStart(player);
            me->DespawnOrUnsummon(0ms, 5min);
            return;
        }
    }

private:
    uint32 _tickMs;
};

void AddSC_classic_instance_blackrock_spire()
{
    new classic_instance_blackrock_spire();
    new classic_at_blackrock_spire();
    RegisterGameObjectAI(classic_go_father_flame);
    RegisterGameObjectAI(classic_go_blackrock_altar);
    RegisterGameObjectAI(classic_go_rookery_egg);
    new classic_at_ubrs_the_beast();
    RegisterSpellScript(classic_spell_ubrs_freeze_rookery_egg);
}
