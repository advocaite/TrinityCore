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

#include "BattlegroundScript.h"
#include "alterac_valley.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "BattlegroundPackets.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureData.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "GameObjectData.h"
#include "GameTime.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"

constexpr uint32 BG_AV_SCORE_INITIAL_POINTS = 700;
constexpr uint32 BG_AV_EVENT_START_BATTLE = 9166; // Achievement: The Alterac Blitz

enum BG_AV_BroadcastTexts
{
    BG_AV_TEXT_START_ONE_MINUTE     = 10638,
    BG_AV_TEXT_START_HALF_MINUTE    = 10639,
    BG_AV_TEXT_BATTLE_HAS_BEGUN     = 10640,

    BG_AV_TEXT_ALLIANCE_NEAR_LOSE   = 23210,
    BG_AV_TEXT_HORDE_NEAR_LOSE      = 23211
};

enum BG_AV_Sounds
{
    AV_SOUND_NEAR_VICTORY                   = 8456, /// @todo: Not confirmed yet

    AV_SOUND_ALLIANCE_ASSAULTS              = 8212, //tower, grave + enemy boss if someone tries to attack him
    AV_SOUND_HORDE_ASSAULTS                 = 8174,
    AV_SOUND_ALLIANCE_GOOD                  = 8173, //if something good happens for the team:  wins(maybe only through killing the boss), captures mine or grave, destroys tower and defends grave
    AV_SOUND_HORDE_GOOD                     = 8213,
    AV_SOUND_BOTH_TOWER_DEFEND              = 8192,

    AV_SOUND_ALLIANCE_CAPTAIN               = 8232, //gets called when someone attacks them and at the beginning after 5min+rand(x)*10sec (maybe buff)
    AV_SOUND_HORDE_CAPTAIN                  = 8333
};

constexpr Seconds BG_AV_MINE_RESOURCE_TIMER = 45s;

enum class AlteracValleyMine : uint8
{
    North = 0,
    South
};

enum BG_AV_CreatureIds
{
    BG_AV_CREATURE_VANNDAR              = 11948,
    BG_AV_CREATURE_DREKTHAR             = 11946,
    BG_AV_CREATURE_BALINDA              = 11949,
    BG_AV_CREATURE_GALVANGAR            = 11947,
    BG_AV_CREATURE_MORLOCH              = 11657,
    BG_AV_CREATURE_UMI_THORSON          = 13078,
    BG_AV_CREATURE_KEETAR               = 13079,
    BG_AV_CREATURE_TASKMASTER_SNIVVLE   = 11677,
    BG_AV_CREATURE_AGI_RUMBLESTOMP      = 13086,
    BG_AV_CREATURE_MASHA_SWIFTCUT       = 13088,
    BG_AV_CREATURE_HERALD               = 14848,

    BG_AV_CREATURE_STORMPIKE_DEFENDER   = 12050,
    BG_AV_CREATURE_FROSTWOLF_GUARDIAN   = 12053,
    BG_AV_CREATURE_SEASONED_DEFENDER    = 13326,
    BG_AV_CREATURE_SEASONED_GUARDIAN    = 13328,
    BG_AV_CREATURE_VETERAN_DEFENDER     = 13331,
    BG_AV_CREATURE_VETERAN_GUARDIAN     = 13332,
    BG_AV_CREATURE_CHAMPION_DEFENDER    = 13422,
    BG_AV_CREATURE_CHAMPION_GUARDIAN    = 13421
};

enum BG_AV_ObjectIds
{
    //cause the mangos-system is a bit different, we don't use the right go-ids for every node.. if we want to be 100% like another big server, we must take one object for every node
    //snowfall 4flags as eyecandy 179424 (alliance neutral)
    //Banners - stolen from battleground_AB.h ;-)
    BG_AV_OBJECTID_BANNER_A             = 178925, // can only be used by horde
    BG_AV_OBJECTID_BANNER_H             = 178943, // can only be used by alliance
    BG_AV_OBJECTID_BANNER_CONT_A        = 178940, // can only be used by horde
    BG_AV_OBJECTID_BANNER_CONT_H        = 179435, // can only be used by alliance

    BG_AV_OBJECTID_BANNER_A_B           = 178365,
    BG_AV_OBJECTID_BANNER_H_B           = 178364,
    BG_AV_OBJECTID_BANNER_CONT_A_B      = 179286,
    BG_AV_OBJECTID_BANNER_CONT_H_B      = 179287,
    BG_AV_OBJECTID_BANNER_SNOWFALL_N    = 180418,

    //snowfall eyecandy banner:
    BG_AV_OBJECTID_SNOWFALL_CANDY_A     = 179044,
    BG_AV_OBJECTID_SNOWFALL_CANDY_PA    = 179424,
    BG_AV_OBJECTID_SNOWFALL_CANDY_H     = 179064,
    BG_AV_OBJECTID_SNOWFALL_CANDY_PH    = 179425,

    //banners on top of towers:
    BG_AV_OBJECTID_TOWER_BANNER_A       = 178927, //[PH] Alliance A1 Tower Banner BIG
    BG_AV_OBJECTID_TOWER_BANNER_H       = 178955, //[PH] Horde H1 Tower Banner BIG
    BG_AV_OBJECTID_TOWER_BANNER_PA      = 179446, //[PH] Alliance H1 Tower Pre-Banner BIG
    BG_AV_OBJECTID_TOWER_BANNER_PH      = 179436, //[PH] Horde A1 Tower Pre-Banner BIG

    //Auras
    BG_AV_OBJECTID_AURA_A               = 180421,
    BG_AV_OBJECTID_AURA_H               = 180422,
    BG_AV_OBJECTID_AURA_N               = 180423,
    BG_AV_OBJECTID_AURA_A_S             = 180100,
    BG_AV_OBJECTID_AURA_H_S             = 180101,
    BG_AV_OBJECTID_AURA_N_S             = 180102,

    BG_AV_OBJECTID_GATE                 = 180424,
    BG_AV_GHOST_GATE                    = 180322,

    //mine supplies
    BG_AV_OBJECTID_MINE_N               = 178785,
    BG_AV_OBJECTID_MINE_S               = 178784,

    BG_AV_OBJECTID_FIRE                 = 179065,
    BG_AV_OBJECTID_SMOKE                = 179066,

    // Towers
    BG_AV_OBJECTID_SOUTH_BUNKER_CONTROLLED_TOWER_BANNER = 178927,
    BG_AV_OBJECTID_SOUTH_BUNKER_CONTROLLED_BANNER = 178925,
    BG_AV_OBJECTID_SOUTH_BUNKER_CONTESTED_BANNER = 179435,
    BG_AV_OBJECTID_SOUTH_BUNKER_CONTESTED_TOWER_BANNER = 179436,

    BG_AV_OBJECTID_NORTH_BUNKER_CONTROLLED_TOWER_BANNER = 178932,
    BG_AV_OBJECTID_NORTH_BUNKER_CONTROLLED_BANNER = 178929,
    BG_AV_OBJECTID_NORTH_BUNKER_CONTESTED_BANNER = 179439,
    BG_AV_OBJECTID_NORTH_BUNKER_CONTESTED_TOWER_BANNER = 179440,

    BG_AV_OBJECTID_EAST_TOWER_CONTROLLED_TOWER_BANNER = 178956,
    BG_AV_OBJECTID_EAST_TOWER_CONTROLLED_BANNER = 178944,
    BG_AV_OBJECTID_EAST_TOWER_CONTESTED_BANNER = 179449,
    BG_AV_OBJECTID_EAST_TOWER_CONTESTED_TOWER_BANNER = 179450,

    BG_AV_OBJECTID_WEST_TOWER_CONTROLLED_TOWER_BANNER = 178955,
    BG_AV_OBJECTID_WEST_TOWER_CONTROLLED_BANNER = 178943,
    BG_AV_OBJECTID_WEST_TOWER_CONTESTED_BANNER = 179445,
    BG_AV_OBJECTID_WEST_TOWER_CONTESTED_TOWER_BANNER = 179446,

    BG_AV_OBJECTID_TOWER_POINT_CONTROLLED_TOWER_BANNER = 178957,
    BG_AV_OBJECTID_TOWER_POINT_CONTROLLED_BANNER = 178945,
    BG_AV_OBJECTID_TOWER_POINT_CONTESTED_BANNER = 179453,
    BG_AV_OBJECTID_TOWER_POINT_CONTESTED_TOWER_BANNER = 179454,

    BG_AV_OBJECTID_ICEBLOOD_TOWER_CONTROLLED_TOWER_BANNER = 178958,
    BG_AV_OBJECTID_ICEBLOOD_TOWER_CONTROLLED_BANNER = 178946,
    BG_AV_OBJECTID_ICEBLOOD_TOWER_CONTESTED_BANNER = 178940,
    BG_AV_OBJECTID_ICEBLOOD_TOWER_CONTESTED_TOWER_BANNER = 179458,

    BG_AV_OBJECTID_STONEHEARTH_BUNKER_CONTROLLED_TOWER_BANNER = 178948,
    BG_AV_OBJECTID_STONEHEARTH_BUNKER_CONTROLLED_BANNER = 178936,
    BG_AV_OBJECTID_STONEHEARTH_BUNKER_CONTESTED_BANNER = 179443,
    BG_AV_OBJECTID_STONEHEARTH_BUNKER_CONTESTED_TOWER_BANNER = 179444,

    BG_AV_OBJECTID_ICEWING_BUNKER_CONTROLLED_TOWER_BANNER = 178947,
    BG_AV_OBJECTID_ICEWING_BUNKER_CONTROLLED_BANNER = 178935,
    BG_AV_OBJECTID_ICEWING_BUNKER_CONTESTED_BANNER = 179441,
    BG_AV_OBJECTID_ICEWING_BUNKER_CONTESTED_TOWER_BANNER = 179442,

    // Graveyards
    BG_AV_OBJECTID_AID_STATION_ALLIANCE_CONTROLLED = 179465,
    BG_AV_OBJECTID_AID_STATION_HORDE_CONTESTED = 179468,
    BG_AV_OBJECTID_AID_STATION_HORDE_CONTROLLED = 179467,
    BG_AV_OBJECTID_AID_STATION_ALLIANCE_CONTESTED = 179466,

    BG_AV_OBJECTID_STORMPIKE_ALLIANCE_CONTROLLED = 178389,
    BG_AV_OBJECTID_STORMPIKE_HORDE_CONTESTED = 179287,
    BG_AV_OBJECTID_STORMPIKE_HORDE_CONTROLLED = 178388,
    BG_AV_OBJECTID_STORMPIKE_ALLIANCE_CONTESTED = 179286,

    BG_AV_OBJECTID_STONEHEARTH_HORDE_CONTESTED = 179310,
    BG_AV_OBJECTID_STONEHEARTH_HORDE_CONTROLLED = 179285,
    BG_AV_OBJECTID_STONEHEARTH_ALLIANCE_CONTESTED = 179308,
    BG_AV_OBJECTID_STONEHEARTH_ALLIANCE_CONTROLLED = 179284,

    BG_AV_OBJECTID_SNOWFALL_NEUTRAL = 180418,
    BG_AV_OBJECTID_SNOWFALL_HORDE_CONTESTED = 180420,
    BG_AV_OBJECTID_SNOWFALL_ALLIANCE_CONTESTED = 180419,
    BG_AV_OBJECTID_SNOWFALL_HORDE_CONTROLLED = 178364,
    BG_AV_OBJECTID_SNOWFALL_ALLIANCE_CONTROLLED = 178365,

    BG_AV_OBJECTID_ICEBLOOD_HORDE_CONTROLLED = 179483,
    BG_AV_OBJECTID_ICEBLOOD_ALLIANCE_CONTESTED = 179482,
    BG_AV_OBJECTID_ICEBLOOD_ALLIANCE_CONTROLLED = 179481,
    BG_AV_OBJECTID_ICEBLOOD_HORDE_CONTESTED = 179484,

    BG_AV_OBJECTID_FROSTWOLF_HORDE_CONTROLLED = 178393,
    BG_AV_OBJECTID_FROSTWOLF_ALLIANCE_CONTESTED = 179304,
    BG_AV_OBJECTID_FROSTWOLF_ALLIANCE_CONTROLLED = 178394,
    BG_AV_OBJECTID_FROSTWOLF_HORDE_CONTESTED = 179305,

    BG_AV_OBJECTID_FROSTWOLF_HUT_HORDE_CONTROLLED = 179472,
    BG_AV_OBJECTID_FROSTWOLF_HUT_ALLIANCE_CONTESTED = 179471,
    BG_AV_OBJECTID_FROSTWOLF_HUT_ALLIANCE_CONTROLLED = 179470,
    BG_AV_OBJECTID_FROSTWOLF_HUT_HORDE_CONTESTED = 179473
};

enum BG_AV_Nodes
{
    BG_AV_NODES_FIRSTAID_STATION         = 0,
    BG_AV_NODES_STORMPIKE_GRAVE          = 1,
    BG_AV_NODES_STONEHEART_GRAVE         = 2,
    BG_AV_NODES_SNOWFALL_GRAVE           = 3,
    BG_AV_NODES_ICEBLOOD_GRAVE           = 4,
    BG_AV_NODES_FROSTWOLF_GRAVE          = 5,
    BG_AV_NODES_FROSTWOLF_HUT            = 6,
    BG_AV_NODES_DUNBALDAR_SOUTH          = 7,
    BG_AV_NODES_DUNBALDAR_NORTH          = 8,
    BG_AV_NODES_ICEWING_BUNKER           = 9,
    BG_AV_NODES_STONEHEART_BUNKER        = 10,
    BG_AV_NODES_ICEBLOOD_TOWER           = 11,
    BG_AV_NODES_TOWER_POINT              = 12,
    BG_AV_NODES_FROSTWOLF_ETOWER         = 13,
    BG_AV_NODES_FROSTWOLF_WTOWER         = 14,

    BG_AV_NODES_MAX                      = 15
};

enum BG_AV_BUFF
{ /// @todo: Add all other buffs here
    AV_BUFF_ARMOR = 21163,
    AV_BUFF_A_CAPTAIN = 23693, //the buff which the alliance captain does
    AV_BUFF_H_CAPTAIN = 22751 //the buff which the horde captain does
};
enum BG_AV_States
{
    POINT_NEUTRAL              = 0,
    POINT_ASSAULTED            = 1,
    POINT_DESTROYED            = 2,
    POINT_CONTROLED            = 3
};

enum BG_AV_WorldStates
{
    AV_WS_ALLIANCE_REINFORCEMENTS       = 3127,
    AV_WS_HORDE_REINFORCEMENTS          = 3128,
    AV_WS_SHOW_HORDE_REINFORCEMENTS     = 3133,
    AV_WS_SHOW_ALLIANCE_REINFORCEMENTS  = 3134,
    AV_WS_MAX_REINFORCEMENTS            = 3136,

// Graves
    // Alliance
    //Stormpike first aid station
    AV_WS_STORMPIKE_AID_STATION_ALLIANCE_CONTROLLED             = 1325,
    AV_WS_STORMPIKE_AID_STATION_IN_CONFLICT_ALLIANCE_ATTACKING  = 1326,
    AV_WS_STORMPIKE_AID_STATION_HORDE_CONTROLLED                = 1327,
    AV_WS_STORMPIKE_AID_STATION_IN_CONFLICT_HORDE_ATTACKING     = 1328,
    //Stormpike Graveyard
    AV_WS_STORMPIKE_GRAVEYARD_ALLIANCE_CONTROLLED               = 1333,
    AV_WS_STORMPIKE_GRAVEYARD_IN_CONFLICT_ALLIANCE_ATTACKING    = 1335,
    AV_WS_STORMPIKE_GRAVEYARD_HORDE_CONTROLLED                  = 1334,
    AV_WS_STORMPIKE_GRAVEYARD_IN_CONFLICT_HORDE_ATTACKING       = 1336,
    //Stoneheart Grave
    AV_WS_STONEHEARTH_GRAVEYARD_ALLIANCE_CONTROLLED             = 1302,
    AV_WS_STONEHEARTH_GRAVEYARD_IN_CONFLICT_ALLIANCE_ATTACKING  = 1304,
    AV_WS_STONEHEARTH_GRAVEYARD_HORDE_CONTROLLED                = 1301,
    AV_WS_STONEHEARTH_GRAVEYARD_IN_CONFLICT_HORDE_ATTACKING     = 1303,
    //Neutral
    //Snowfall Grave
    AV_WS_SNOWFALL_GRAVEYARD_UNCONTROLLED                       = 1966,
    AV_WS_SNOWFALL_GRAVEYARD_ALLIANCE_CONTROLLED                = 1341,
    AV_WS_SNOWFALL_GRAVEYARD_IN_CONFLICT_ALLIANCE_ATTACKING     = 1343,
    AV_WS_SNOWFALL_GRAVEYARD_HORDE_CONTROLLED                   = 1342,
    AV_WS_SNOWFALL_GRAVEYARD_IN_CONFLICT_HORDE_ATTACKING        = 1344,
    //Horde
    //Iceblood grave
    AV_WS_ICEBLOOD_GRAVEYARD_ALLIANCE_CONTROLLED                = 1346,
    AV_WS_ICEBLOOD_GRAVEYARD_IN_CONFLICT_ALLIANCE_ATTACKING     = 1348,
    AV_WS_ICEBLOOD_GRAVEYARD_HORDE_CONTROLLED                   = 1347,
    AV_WS_ICEBLOOD_GRAVEYARD_IN_CONFLICT_HORDE_ATTACKING        = 1349,
    //Frostwolf Grave
    AV_WS_FROSTWOLF_GRAVEYARD_ALLIANCE_CONTROLLED               = 1337,
    AV_WS_FROSTWOLF_GRAVEYARD_IN_CONFLICT_ALLIANCE_ATTACKING    = 1339,
    AV_WS_FROSTWOLF_GRAVEYARD_HORDE_CONTROLLED                  = 1338,
    AV_WS_FROSTWOLF_GRAVEYARD_IN_CONFLICT_HORDE_ATTACKING       = 1340,
    //Frostwolf Hut
    AV_WS_FROSTWOLF_RELIEF_HUT_ALLIANCE_CONTROLLED              = 1329,
    AV_WS_FROSTWOLF_RELIEF_HUT_IN_CONFLICT_ALLIANCE_ATTACKING   = 1331,
    AV_WS_FROSTWOLF_RELIEF_HUT_HORDE_CONTROLLED                 = 1330,
    AV_WS_FROSTWOLF_RELIEF_HUT_IN_CONFLICT_HORDE_ATTACKING      = 1332,

//Towers
    //Alliance
    //Dunbaldar South Bunker
    AV_WS_DUN_BALDAR_SOUTH_BUNKER_OWNER                             = 1181,
    AV_WS_DUN_BALDAR_SOUTH_BUNKER_ALLIANCE_CONTROLLED               = 1361,
    AV_WS_DUN_BALDAR_SOUTH_BUNKER_DESTROYED                         = 1370,
    AV_WS_DUN_BALDAR_SOUTH_BUNKER_IN_CONFLICT_HORDE_ATTACKING       = 1378,
    AV_WS_DUN_BALDAR_SOUTH_BUNKER_IN_CONFLICT_ALLIANCE_ATTACKING    = 1374, // unused
    //Dunbaldar North Bunker
    AV_WS_DUN_BALDAR_NORTH_BUNKER_OWNER                             = 1182,
    AV_WS_DUN_BALDAR_NORTH_BUNKER_ALLIANCE_CONTROLLED               = 1362,
    AV_WS_DUN_BALDAR_NORTH_BUNKER_DESTROYED                         = 1371,
    AV_WS_DUN_BALDAR_NORTH_BUNKER_IN_CONFLICT_HORDE_ATTACKING       = 1379,
    AV_WS_DUN_BALDAR_NORTH_BUNKER_IN_CONFLICT_ALLIANCE_ATTACKING    = 1375, // unused
    //Icewing Bunker
    AV_WS_ICEWING_BUNKER_OWNER                                      = 1183,
    AV_WS_ICEWING_BUNKER_ALLIANCE_CONTROLLED                        = 1363,
    AV_WS_ICEWING_BUNKER_DESTROYED                                  = 1372,
    AV_WS_ICEWING_BUNKER_IN_CONFLICT_HORDE_ATTACKING                = 1380,
    AV_WS_ICEWING_BUNKER_IN_CONFLICT_ALLIANCE_ATTACKING             = 1376, // unused
    //Stoneheart Bunker
    AV_WS_STONEHEARTH_BUNKER_OWNER                                  = 1184,
    AV_WS_STONEHEARTH_BUNKER_ALLIANCE_CONTROLLED                    = 1364,
    AV_WS_STONEHEARTH_BUNKER_DESTROYED                              = 1373,
    AV_WS_STONEHEARTH_BUNKER_IN_CONFLICT_HORDE_ATTACKING            = 1381,
    AV_WS_STONEHEARTH_BUNKER_IN_CONFLICT_ALLIANCE_ATTACKING         = 1377, // unused
    //Horde
    //Iceblood Tower
    AV_WS_ICEBLOOD_TOWER_OWNER                                      = 1187,
    AV_WS_ICEBLOOD_TOWER_DESTROYED                                  = 1368,
    AV_WS_ICEBLOOD_TOWER_HORDE_CONTROLLED                           = 1385,
    AV_WS_ICEBLOOD_TOWER_IN_CONFLICT_ALLIANCE_ATTACKING             = 1390,
    AV_WS_ICEBLOOD_TOWER_IN_CONFLICT_HORDE_ATTACKING                = 1395, // unused
    //Tower Point
    AV_WS_TOWER_POINT_OWNER                                         = 1188,
    AV_WS_TOWER_POINT_DESTROYED                                     = 1367,
    AV_WS_TOWER_POINT_HORDE_CONTROLLED                              = 1384,
    AV_WS_TOWER_POINT_IN_CONFLICT_ALLIANCE_ATTACKING                = 1389,
    AV_WS_TOWER_POINT_IN_CONFLICT_HORDE_ATTACKING                   = 1394, // unused
    //Frostwolf West
    AV_WS_WEST_FROSTWOLF_TOWER_OWNER                                = 1185,
    AV_WS_WEST_FROSTWOLF_TOWER_DESTROYED                            = 1365,
    AV_WS_WEST_FROSTWOLF_TOWER_HORDE_CONTROLLED                     = 1382,
    AV_WS_WEST_FROSTWOLF_TOWER_IN_CONFLICT_ALLIANCE_ATTACKING       = 1387,
    AV_WS_WEST_FROSTWOLF_TOWER_IN_CONFLICT_HORDE_ATTACKING          = 1392, // unused
    //Frostwolf East
    AV_WS_EAST_FROSTWOLF_TOWER_OWNER                                = 1186,
    AV_WS_EAST_FROSTWOLF_TOWER_DESTROYED                            = 1366,
    AV_WS_EAST_FROSTWOLF_TOWER_HORDE_CONTROLLED                     = 1383,
    AV_WS_EAST_FROSTWOLF_TOWER_IN_CONFLICT_ALLIANCE_ATTACKING       = 1388,
    AV_WS_EAST_FROSTWOLF_TOWER_IN_CONFLICT_HORDE_ATTACKING          = 1393, // unused

//Mines
    AV_WS_IRONDEEP_MINE_OWNER                   = 801,
    AV_WS_IRONDEEP_MINE_TROGG_CONTROLLED        = 1360,
    AV_WS_IRONDEEP_MINE_ALLIANCE_CONTROLLED     = 1358,
    AV_WS_IRONDEEP_MINE_HORDE_CONTROLLED        = 1359,

    AV_WS_COLDTOOTH_MINE_OWNER                  = 804,
    AV_WS_COLDTOOTH_MINE_KOBOLD_CONTROLLED      = 1357,
    AV_WS_COLDTOOTH_MINE_ALLIANCE_CONTROLLED    = 1355,
    AV_WS_COLDTOOTH_MINE_HORDE_CONTROLLED       = 1356,

//Turnins
    AV_WS_IVUS_STORM_CRYSTAL_COUNT                  = 1043,
    AV_WS_IVUS_STORM_CRYSTAL_MAX                    = 1044,
    AV_WS_LOKHOLAR_STORMPIKE_SOLDIERS_BLOOD_COUNT   = 923,
    AV_WS_LOKHOLAR_STORMPIKE_SOLDIERS_BLOOD_MAX     = 922,

//Bosses
    AV_WS_DREKTHAR_ALIVE    = 601,
    AV_WS_VANDAAR_ALIVE     = 602,

//Captains
    AV_WS_GALVAGAR_ALIVE    = 1352,
    AV_WS_BALINDA_ALIVE     = 1351,
};

struct StaticMineInfo
{
    int32 WorldStateOwner;
    int32 WorldStateAllianceControlled;
    int32 WorldStateHordeControlled;
    int32 WorldStateNeutralControlled;
    uint8 TextIdAlliance;
    uint8 TextIdHorde;
};

struct AlteracValleyMineInfo
{
    Team Owner;

    StaticMineInfo StaticInfo;
};

struct StaticNodeInfo
{
    BG_AV_Nodes NodeId;

    struct
    {
        uint8 AllianceCapture;
        uint8 AllianceAttack;
        uint8 HordeCapture;
        uint8 HordeAttack;
    } TextIds;

    struct
    {
        int32 AllianceControl;
        int32 AllianceAssault;
        int32 HordeControl;
        int32 HordeAssault;
        int32 Owner;
    } WorldStateIds;

    struct
    {
        std::string AllianceOrDefend;
        std::string HordeOrDestroy;
    } StringIds;
};

static StaticNodeInfo const BGAVNodeInfo[] =
{
    { BG_AV_NODES_FIRSTAID_STATION,  { 47, 48, 45, 46 }, { 1325, 1326, 1327, 1328, 0 }, { "bg_av_herald_stormpike_aid_station_alliance", "bg_av_herald_stormpike_aid_station_horde" } }, // Stormpike First Aid Station
    { BG_AV_NODES_STORMPIKE_GRAVE,   {  1,  2,  3,  4 }, { 1333, 1335, 1334, 1336, 0 }, { "bg_av_herald_stormpike_alliance", "bg_av_herald_stormpike_horde" } }, // Stormpike Graveyard
    { BG_AV_NODES_STONEHEART_GRAVE,  { 55, 56, 53, 54 }, { 1302, 1304, 1301, 1303, 0 }, { "bg_av_herald_stonehearth_alliance", "bg_av_herald_stonehearth_horde" } }, // Stoneheart Graveyard
    { BG_AV_NODES_SNOWFALL_GRAVE,    {  5,  6,  7,  8 }, { 1341, 1343, 1342, 1344, 0 }, { "bg_av_herald_snowfall_alliance", "bg_av_herald_snowfall_horde" } }, // Snowfall Graveyard
    { BG_AV_NODES_ICEBLOOD_GRAVE,    { 59, 60, 57, 58 }, { 1346, 1348, 1347, 1349, 0 }, { "bg_av_herald_iceblood_alliance", "bg_av_herald_iceblood_horde" } }, // Iceblood Graveyard
    { BG_AV_NODES_FROSTWOLF_GRAVE,   {  9, 10, 11, 12 }, { 1337, 1339, 1338, 1340, 0 }, { "bg_av_herald_frostwolf_alliance", "bg_av_herald_frostwolf_horde" } }, // Frostwolf Graveyard
    { BG_AV_NODES_FROSTWOLF_HUT,     { 51, 52, 49, 50 }, { 1329, 1331, 1330, 1332, 0 }, { "bg_av_herald_frostwolf_hut_alliance", "bg_av_herald_frostwolf_hut_horde" } }, // Frostwolf Hut
    { BG_AV_NODES_DUNBALDAR_SOUTH,   { 16, 15, 14, 13 }, { 1361, 1375, 1370, 1378, 1181 }, { "bg_av_herald_south_bunker_defend", "bg_av_herald_south_bunker_attack" } }, // Dunbaldar South Bunker
    { BG_AV_NODES_DUNBALDAR_NORTH,   { 20, 19, 18, 17 }, { 1362, 1374, 1371, 1379, 1182 }, { "bg_av_herald_north_bunker_defend", "bg_av_herald_south_bunker_attack" } }, // Dunbaldar North Bunker
    { BG_AV_NODES_ICEWING_BUNKER,    { 24, 23, 22, 21 }, { 1363, 1376, 1372, 1380, 1183 }, { "bg_av_herald_icewing_bunker_defend", "bg_av_herald_icewing_bunker_attack" } }, // Icewing Bunker
    { BG_AV_NODES_STONEHEART_BUNKER, { 28, 27, 26, 25 }, { 1364, 1377, 1373, 1381, 1184 }, { "bg_av_herald_stonehearth_bunker_defend", "bg_av_herald_stonehearth_bunker_attack" } }, // Stoneheart Bunker
    { BG_AV_NODES_ICEBLOOD_TOWER,    { 44, 43, 42, 41 }, { 1368, 1390, 1385, 1395, 1188 }, { "bg_av_herald_iceblood_tower_defend", "bg_av_herald_iceblood_tower_attack" } }, // Iceblood Tower
    { BG_AV_NODES_TOWER_POINT,       { 40, 39, 38, 37 }, { 1367, 1389, 1384, 1394, 1187 }, { "bg_av_herald_tower_point_defend", "bg_av_herald_tower_point_attack" } }, // Tower Point
    { BG_AV_NODES_FROSTWOLF_ETOWER,  { 36, 35, 34, 33 }, { 1366, 1388, 1383, 1393, 1186 }, { "bg_av_herald_east_tower_defend", "bg_av_herald_east_tower_attack" } }, // Frostwolf East Tower
    { BG_AV_NODES_FROSTWOLF_WTOWER,  { 32, 31, 30, 29 }, { 1365, 1387, 1382, 1392, 1185 }, { "bg_av_herald_west_tower_defend", "bg_av_herald_west_tower_attack" } }, // Frostwolf West Tower
};

enum Texts
{
    // Herold
    // Towers/Graveyards = 1 - 60
    TEXT_COLDTOOTH_MINE_ALLIANCE_TAKEN  = 61,
    TEXT_IRONDEEP_MINE_ALLIANCE_TAKEN   = 62,
    TEXT_COLDTOOTH_MINE_HORDE_TAKEN     = 63,
    TEXT_IRONDEEP_MINE_HORDE_TAKEN      = 64,
    TEXT_FROSTWOLF_GENERAL_DEAD         = 65, /// @todo: sound is missing
    TEXT_STORMPIKE_GENERAL_DEAD         = 66, /// @todo: sound is missing
    TEXT_ALLIANCE_WINS                  = 67, // NYI /// @todo: sound is missing
    TEXT_HORDE_WINS                     = 68, // NYI /// @todo: sound is missing

    // Taskmaster Snivvle
    TEXT_SNIVVLE_RANDOM                 = 0
};

enum BG_AV_ExploitTeleportLocations
{
    AV_EXPLOIT_TELEPORT_LOCATION_ALLIANCE = 3664,
    AV_EXPLOIT_TELEPORT_LOCATION_HORDE = 3665
};

struct BG_AV_NodeInfo
{
    BG_AV_States State;
    BG_AV_States PrevState;
    uint16       TotalOwner;
    Team         Owner;
    uint16       PrevOwner;
    bool         Tower;
};

inline BG_AV_Nodes& operator++(BG_AV_Nodes& i) { return i = BG_AV_Nodes(i + 1); }

struct battleground_alterac_valley : BattlegroundScript
{
    enum QuestIds
    {
        AV_QUEST_A_SCRAPS1      = 7223,
        AV_QUEST_A_SCRAPS2      = 6781,
        AV_QUEST_H_SCRAPS1      = 7224,
        AV_QUEST_H_SCRAPS2      = 6741,
        AV_QUEST_A_COMMANDER1   = 6942, //soldier
        AV_QUEST_H_COMMANDER1   = 6825,
        AV_QUEST_A_COMMANDER2   = 6941, //leutnant
        AV_QUEST_H_COMMANDER2   = 6826,
        AV_QUEST_A_COMMANDER3   = 6943, //commander
        AV_QUEST_H_COMMANDER3   = 6827,
        AV_QUEST_A_BOSS1        = 7386, // 5 cristal/blood
        AV_QUEST_H_BOSS1        = 7385,
        AV_QUEST_A_BOSS2        = 6881, // 1
        AV_QUEST_H_BOSS2        = 6801,
        AV_QUEST_A_NEAR_MINE    = 5892, //the mine near start location of team
        AV_QUEST_H_NEAR_MINE    = 5893,
        AV_QUEST_A_OTHER_MINE   = 6982, //the other mine ;)
        AV_QUEST_H_OTHER_MINE   = 6985,
        AV_QUEST_A_RIDER_HIDE   = 7026,
        AV_QUEST_H_RIDER_HIDE   = 7002,
        AV_QUEST_A_RIDER_TAME   = 7027,
        AV_QUEST_H_RIDER_TAME   = 7001
    };

    enum Data : uint32
    {
        DATA_DEFENDER_TIER_HORDE    = 1,
        DATA_DEFENDER_TIER_ALLIANCE = 2,
    };

    enum DefenderTier : uint32
    {
        BG_AV_DEFENDER_TIER_DEFENDER,
        BG_AV_DEFENDER_TIER_SEASONED,
        BG_AV_DEFENDER_TIER_VETERAN,
        BG_AV_DEFENDER_TIER_CHAMPION
    };

    enum PvpStats
    {
        PVP_STAT_TOWERS_ASSAULTED       = 61,
        PVP_STAT_GRAVEYARDS_ASSAULTED   = 63,
        PVP_STAT_TOWERS_DEFENDED        = 64,
        PVP_STAT_GRAVEYARDS_DEFENDED    = 65,
        PVP_STAT_SECONDARY_OBJECTIVES   = 82
    };

    enum Misc
    {
        NEAR_LOSE_POINTS        = 140
    };

    enum HonorKillBonus
    {
        HONOR_KILL_BONUS_BOSS               = 4,
        HONOR_KILL_BONUS_CAPTAIN            = 3,
        HONOR_KILL_BONUS_SURVIVING_TOWER    = 2,
        HONOR_KILL_BONUS_SURVIVING_CAPTAIN  = 2,
        HONOR_KILL_BONUS_DESTROY_TOWER      = 3
    };

    enum ReputationGains
    {
        REP_GAIN_BOSS               = 350,
        REP_GAIN_CAPTAIN            = 125,
        REP_GAIN_DESTROY_TOWER      = 12,
        REP_GAIN_SURVIVING_TOWER    = 12,
        REP_GAIN_SURVIVING_CAPTAIN  = 125
    };

    enum ResourceLoss
    {
        RESOURCE_LOSS_TOWER     = -75,
        RESOURCE_LOSS_CAPTAIN   = -100
    };

    enum Spells
    {
        SPELL_COMPLETE_ALTERAC_VALLEY_QUEST = 23658,
    };

    enum Factions
    {
        FACTION_FROSTWOLF_CLAN  = 729,
        FACTION_STORMPIKE_GUARD = 730,
    };

    explicit battleground_alterac_valley(BattlegroundMap* map) : BattlegroundScript(map)
    {
        _teamResources = { BG_AV_SCORE_INITIAL_POINTS, BG_AV_SCORE_INITIAL_POINTS };
        _isInformedNearVictory = { false, false };

        for (uint8 i = 0; i < 2; i++) //forloop for both teams (it just make 0 == alliance and 1 == horde also for both mines 0=north 1=south
        {
            for (uint8 j = 0; j < 9; j++)
                _teamQuestStatus[i][j] = 0;

            _captainBuffTimer[i].Reset(120000 + urand(0, 4) * 60); //as far as i could see, the buff is randomly so i make 2minutes (thats the duration of the buff itself) + 0-4minutes @todo get the right times
        }

        _mineInfo[uint8(AlteracValleyMine::North)] = { TEAM_OTHER, { AV_WS_IRONDEEP_MINE_OWNER, AV_WS_IRONDEEP_MINE_ALLIANCE_CONTROLLED, AV_WS_IRONDEEP_MINE_HORDE_CONTROLLED, AV_WS_IRONDEEP_MINE_TROGG_CONTROLLED, TEXT_IRONDEEP_MINE_ALLIANCE_TAKEN, TEXT_IRONDEEP_MINE_HORDE_TAKEN } };
        _mineInfo[uint8(AlteracValleyMine::South)] = { TEAM_OTHER, { AV_WS_COLDTOOTH_MINE_OWNER, AV_WS_COLDTOOTH_MINE_ALLIANCE_CONTROLLED, AV_WS_COLDTOOTH_MINE_HORDE_CONTROLLED, AV_WS_COLDTOOTH_MINE_KOBOLD_CONTROLLED, TEXT_COLDTOOTH_MINE_ALLIANCE_TAKEN, TEXT_COLDTOOTH_MINE_HORDE_TAKEN } };

        for (BG_AV_Nodes i = BG_AV_NODES_FIRSTAID_STATION; i <= BG_AV_NODES_STONEHEART_GRAVE; ++i) //alliance graves
            InitNode(i, ALLIANCE, false);
        for (BG_AV_Nodes i = BG_AV_NODES_DUNBALDAR_SOUTH; i <= BG_AV_NODES_STONEHEART_BUNKER; ++i) //alliance towers
            InitNode(i, ALLIANCE, true);
        for (BG_AV_Nodes i = BG_AV_NODES_ICEBLOOD_GRAVE; i <= BG_AV_NODES_FROSTWOLF_HUT; ++i) //horde graves
            InitNode(i, HORDE, false);
        for (BG_AV_Nodes i = BG_AV_NODES_ICEBLOOD_TOWER; i <= BG_AV_NODES_FROSTWOLF_WTOWER; ++i) //horde towers
            InitNode(i, HORDE, true);
        InitNode(BG_AV_NODES_SNOWFALL_GRAVE, TEAM_OTHER, false); //give snowfall neutral owner

        _mineResourceTimer.Reset(BG_AV_MINE_RESOURCE_TIMER);
    }

    void OnUpdate(uint32 diff) override
    {
        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return;

        _mineResourceTimer.Update(diff);
        if (_mineResourceTimer.Passed())
        {
            for (AlteracValleyMineInfo const& info : _mineInfo)
            {
                if (info.Owner == TEAM_OTHER)
                    continue;

                UpdateScore(info.Owner, 1);
            }

            _mineResourceTimer.Reset(BG_AV_MINE_RESOURCE_TIMER);
        }

        for (uint8 i = TEAM_ALLIANCE; i <= TEAM_HORDE; i++)
        {
            if (!IsCaptainAlive(TeamId(i)))
                continue;

            _captainBuffTimer[i].Update(diff);
            if (_captainBuffTimer[i].Passed())
            {
                if (i == 0)
                {
                    battleground->CastSpellOnTeam(AV_BUFF_A_CAPTAIN, ALLIANCE);
                    if (Creature const* creature = battlegroundMap->GetCreature(_balindaGUID))
                        creature->AI()->DoAction(ACTION_BUFF_YELL);
                }
                else
                {
                    battleground->CastSpellOnTeam(AV_BUFF_H_CAPTAIN, HORDE);
                    if (Creature const* creature = battlegroundMap->GetCreature(_galvangarGUID))
                        creature->AI()->DoAction(ACTION_BUFF_YELL);
                }

                _captainBuffTimer[i].Reset(120000 + urand(0, 4) * 60000); //as far as i could see, the buff is randomly so i make 2minutes (thats the duration of the buff itself) + 0-4minutes @todo get the right times
            }
        }
    }

    void OnPlayerKilled(Player* victim, Player* /*killer*/) override
    {
        UpdateScore(battleground->GetPlayerTeam(victim->GetGUID()), -1);
    }

    void OnUnitKilled(Creature* victim, Unit* killer) override
    {
        switch (victim->GetEntry())
        {
            case BG_AV_CREATURE_VANNDAR:
            {
                UpdateWorldState(AV_WS_VANDAAR_ALIVE, 0);
                battleground->CastSpellOnTeam(SPELL_COMPLETE_ALTERAC_VALLEY_QUEST, HORDE); //this is a spell which finishes a quest where a player has to kill the boss
                battleground->RewardReputationToTeam(FACTION_FROSTWOLF_CLAN, REP_GAIN_BOSS, HORDE);
                battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(HONOR_KILL_BONUS_BOSS), HORDE);
                battleground->EndBattleground(HORDE);
                break;
            }
            case BG_AV_CREATURE_DREKTHAR:
            {
                UpdateWorldState(AV_WS_DREKTHAR_ALIVE, 0);
                battleground->CastSpellOnTeam(SPELL_COMPLETE_ALTERAC_VALLEY_QUEST, ALLIANCE); //this is a spell which finishes a quest where a player has to kill the boss
                battleground->RewardReputationToTeam(FACTION_STORMPIKE_GUARD, REP_GAIN_BOSS, ALLIANCE);
                battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(HONOR_KILL_BONUS_BOSS), ALLIANCE);
                battleground->EndBattleground(ALLIANCE);
                break;
            }
            case BG_AV_CREATURE_BALINDA:
            {
                UpdateWorldState(AV_WS_BALINDA_ALIVE, 0);
                battleground->RewardReputationToTeam(FACTION_FROSTWOLF_CLAN, REP_GAIN_CAPTAIN, HORDE);
                battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(HONOR_KILL_BONUS_CAPTAIN), HORDE);
                UpdateScore(ALLIANCE, RESOURCE_LOSS_CAPTAIN);
                if (Creature const* herald = FindHerald("bg_av_herald_horde_win"))
                    herald->AI()->Talk(TEXT_STORMPIKE_GENERAL_DEAD);
                break;
            }
            case BG_AV_CREATURE_GALVANGAR:
            {
                UpdateWorldState(AV_WS_GALVAGAR_ALIVE, 0);
                battleground->RewardReputationToTeam(FACTION_STORMPIKE_GUARD, REP_GAIN_CAPTAIN, ALLIANCE);
                battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(HONOR_KILL_BONUS_CAPTAIN), ALLIANCE);
                UpdateScore(HORDE, RESOURCE_LOSS_CAPTAIN);
                if (Creature const* herald = FindHerald("bg_av_herald_alliance_win"))
                    herald->AI()->Talk(TEXT_FROSTWOLF_GENERAL_DEAD);
                break;
            }
            case BG_AV_CREATURE_MORLOCH:
            {
                // if mine is not owned by morloch, then nothing happens
                if (_mineInfo[uint8(AlteracValleyMine::North)].Owner != TEAM_OTHER)
                    break;

                Team const killerTeam = battleground->GetPlayerTeam(Coalesce<Unit>(killer->GetCharmerOrOwnerPlayerOrPlayerItself(), killer)->GetGUID());
                ChangeMineOwner(AlteracValleyMine::North, killerTeam);
                break;
            }
            case BG_AV_CREATURE_TASKMASTER_SNIVVLE:
            {
                if (_mineInfo[uint8(AlteracValleyMine::South)].Owner != TEAM_OTHER)
                    break;

                Team const killerTeam = battleground->GetPlayerTeam(Coalesce<Unit>(killer->GetCharmerOrOwnerPlayerOrPlayerItself(), killer)->GetGUID());
                ChangeMineOwner(AlteracValleyMine::South, killerTeam);
                break;
            }
            case BG_AV_CREATURE_UMI_THORSON:
            case BG_AV_CREATURE_KEETAR:
            {
                Team const killerTeam = battleground->GetPlayerTeam(Coalesce<Unit>(killer->GetCharmerOrOwnerPlayerOrPlayerItself(), killer)->GetGUID());
                ChangeMineOwner(AlteracValleyMine::North, killerTeam);
                break;
            }
            case BG_AV_CREATURE_AGI_RUMBLESTOMP:
            case BG_AV_CREATURE_MASHA_SWIFTCUT:
            {
                Team const killerTeam = battleground->GetPlayerTeam(Coalesce<Unit>(killer->GetCharmerOrOwnerPlayerOrPlayerItself(), killer)->GetGUID());
                ChangeMineOwner(AlteracValleyMine::South, killerTeam);
                break;
            }
        }
    }

    bool IsCaptainAlive(TeamId teamId) const
    {
        if (teamId == TEAM_HORDE)
            return battlegroundMap->GetWorldStateValue(AV_WS_GALVAGAR_ALIVE) == 1;

        if (teamId == TEAM_ALLIANCE)
            return battlegroundMap->GetWorldStateValue(AV_WS_BALINDA_ALIVE) == 1;

        return false;
    }

    void OnStart() override
    {
        UpdateWorldState(AV_WS_SHOW_HORDE_REINFORCEMENTS, 1);
        UpdateWorldState(AV_WS_SHOW_ALLIANCE_REINFORCEMENTS, 1);

        // Achievement: The Alterac Blitz
        TriggerGameEvent(BG_AV_EVENT_START_BATTLE);

        for (ObjectGuid const& guid : _doorGUIDs)
        {
            if (GameObject* gameObject = battlegroundMap->GetGameObject(guid))
            {
                gameObject->UseDoorOrButton();
                Seconds delay = gameObject->GetEntry() == BG_AV_GHOST_GATE ? 0s : 3s;
                gameObject->DespawnOrUnsummon(delay);
            }
        }
    }

    void OnEnd(Team winner) override
    {
        BattlegroundScript::OnEnd(winner);
        //calculate bonuskills for both teams:
        //first towers:
        std::array<uint8, PVP_TEAMS_COUNT> kills = { 0, 0 };
        std::array<uint8, PVP_TEAMS_COUNT> rep = { 0, 0 };

        for (BG_AV_Nodes i = BG_AV_NODES_DUNBALDAR_SOUTH; i <= BG_AV_NODES_FROSTWOLF_WTOWER; ++i)
        {
            if (_nodes[i].State == POINT_CONTROLED)
            {
                if (_nodes[i].Owner == ALLIANCE)
                {
                    rep[TEAM_ALLIANCE] += REP_GAIN_SURVIVING_TOWER;
                    kills[TEAM_ALLIANCE] += HONOR_KILL_BONUS_SURVIVING_TOWER;
                }
                else
                {
                    rep[TEAM_HORDE] += REP_GAIN_SURVIVING_TOWER;
                    kills[TEAM_HORDE] += HONOR_KILL_BONUS_SURVIVING_TOWER;
                }
            }
        }

        for (uint8 i = TEAM_ALLIANCE; i <= TEAM_HORDE; ++i)
        {
            if (IsCaptainAlive(TeamId(i)))
            {
                kills[i] += HONOR_KILL_BONUS_SURVIVING_CAPTAIN;
                rep[i] += REP_GAIN_SURVIVING_CAPTAIN;
            }
            if (rep[i] != 0)
                battleground->RewardReputationToTeam(i == 0 ? FACTION_STORMPIKE_GUARD : FACTION_FROSTWOLF_CLAN, rep[i], i == 0 ? ALLIANCE : HORDE);
            if (kills[i] != 0)
                battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(kills[i]), i == 0 ? ALLIANCE : HORDE);
        }
    }

    void OnPlayerLeft(Player* player) override
    {
        BattlegroundScript::OnPlayerLeft(player);
        if (!player)
            return;

        player->RemoveAurasDueToSpell(AV_BUFF_ARMOR);
    }

    void EventPlayerDestroyedPoint(GameObject* gameobject)
    {
        if (!gameobject)
            return;

        BG_AV_Nodes const node = GetNodeThroughObject(gameobject->GetEntry());
        DestroyNode(node);
        UpdateNodeWorldState(node);

        Team const owner = _nodes[node].Owner;
        if (IsTower(node))
        {
            UpdateScore((owner == ALLIANCE) ? HORDE : ALLIANCE, RESOURCE_LOSS_TOWER);
            battleground->RewardReputationToTeam(owner == ALLIANCE ? FACTION_STORMPIKE_GUARD : FACTION_FROSTWOLF_CLAN, REP_GAIN_DESTROY_TOWER, owner);
            battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(HONOR_KILL_BONUS_DESTROY_TOWER), owner);
        }

        if (StaticNodeInfo const* nodeInfo = GetStaticNodeInfo(node))
            if (Creature const* herald = FindHerald(nodeInfo->StringIds.HordeOrDestroy))
                herald->AI()->Talk(owner == ALLIANCE ? nodeInfo->TextIds.AllianceCapture : nodeInfo->TextIds.HordeCapture);

        battlegroundMap->UpdateSpawnGroupConditions();
    }

    void DoAction(uint32 actionId, WorldObject* source, WorldObject* target) override
    {
        Team const team = battleground->GetPlayerTeam(source->GetGUID());
        TeamId const teamIndex = Battleground::GetTeamIndexByTeamId(team);

        switch (actionId)
        {
            case ACTION_AV_CAPTURE_CAPTURABLE_OBJECT:
                EventPlayerDestroyedPoint(source->ToGameObject());
                break;
            case ACTION_AV_INTERACT_CAPTURABLE_OBJECT:
                if (target && source && source->IsPlayer())
                    HandleInteractCapturableObject(source->ToPlayer(), target->ToGameObject());
                break;
            case ACTION_TURN_IN_SCRAPS:
                _teamQuestStatus[teamIndex][0] += 20;
                break;
            case ACTION_TURN_IN_COMMANDER_1:
                _teamQuestStatus[teamIndex][1]++;
                battleground->RewardReputationToTeam(teamIndex, 1, team);
                if (_teamQuestStatus[teamIndex][1] == 30)
                    TC_LOG_DEBUG("bg.battleground", "BG_AV action {} completed (need to implement some events here", actionId);
                break;
            case ACTION_TURN_IN_COMMANDER_2:
                _teamQuestStatus[teamIndex][2]++;
                battleground->RewardReputationToTeam(teamIndex, 1, team);
                if (_teamQuestStatus[teamIndex][2] == 60)
                    TC_LOG_DEBUG("bg.battleground", "BG_AV action {} completed (need to implement some events here", actionId);
                break;
            case ACTION_TURN_IN_COMMANDER_3:
                _teamQuestStatus[teamIndex][3]++;
                battleground->RewardReputationToTeam(teamIndex, 1, team);
                if (_teamQuestStatus[teamIndex][3] == 120)
                    TC_LOG_DEBUG("bg.battleground", "BG_AV action {} completed (need to implement some events here", actionId);
                break;
            case ACTION_TURN_IN_BOSS_1:
                _teamQuestStatus[teamIndex][4] += 4; //you can turn in 5 or 1 item..
                [[fallthrough]];
            case ACTION_TURN_IN_BOSS_2:
                _teamQuestStatus[teamIndex][4]++;
                if (_teamQuestStatus[teamIndex][4] >= 200)
                    TC_LOG_DEBUG("bg.battleground", "BG_AV action {} completed (need to implement some events here", actionId);
                UpdateWorldState(teamIndex == TEAM_ALLIANCE ? AV_WS_IVUS_STORM_CRYSTAL_COUNT : AV_WS_LOKHOLAR_STORMPIKE_SOLDIERS_BLOOD_COUNT, _teamQuestStatus[teamIndex][4]);
                break;
            case ACTION_TURN_IN_NEAR_MINE:
                _teamQuestStatus[teamIndex][5]++;
                if (_teamQuestStatus[teamIndex][5] == 28)
                {
                    TC_LOG_DEBUG("bg.battleground", "BG_AV action {} completed (need to implement some events here", actionId);
                    if (_teamQuestStatus[teamIndex][6] == 7)
                        TC_LOG_DEBUG("bg.battleground", "BG_AV action {} completed (need to implement some events here - ground assault ready", actionId);
                }
                break;
            case ACTION_TURN_IN_OTHER_MINE:
                _teamQuestStatus[teamIndex][6]++;
                if (_teamQuestStatus[teamIndex][6] == 7)
                {
                    TC_LOG_DEBUG("bg.battleground", "BG_AV action {} completed (need to implement some events here", actionId);
                    if (_teamQuestStatus[teamIndex][5] == 20)
                        TC_LOG_DEBUG("bg.battleground", "BG_AV action {} completed (need to implement some events here - ground assault ready", actionId);
                }
                break;
            case ACTION_TURN_IN_RIDER_HIDE:
                _teamQuestStatus[teamIndex][7]++;
                if (_teamQuestStatus[teamIndex][7] == 25)
                {
                    TC_LOG_DEBUG("bg.battleground", "BG_AV action {} completed (need to implement some events here", actionId);
                    if (_teamQuestStatus[teamIndex][8] == 25)
                        TC_LOG_DEBUG("bg.battleground", "BG_AV action {} completed (need to implement some events here - rider assault ready", actionId);
                }
                break;
            case ACTION_TURN_IN_RIDER_TAME:
                _teamQuestStatus[teamIndex][8]++;
                if (_teamQuestStatus[teamIndex][8] == 25)
                {
                    TC_LOG_DEBUG("bg.battleground", "BG_AV action {} completed (need to implement some events here", actionId);
                    if (_teamQuestStatus[teamIndex][7] == 25)
                        TC_LOG_DEBUG("bg.battleground", "BG_AV action {} completed (need to implement some events here - rider assault ready", actionId);
                }
                break;
            default:
                TC_LOG_ERROR("bg.battleground", "BattlegroundAV::DoAction: {}. Unhandled action.", actionId);
                break;
        }
    }

    void ChangeMineOwner(AlteracValleyMine mine, Team team, bool initial = false)
    {
        if (team != ALLIANCE && team != HORDE)
            team = TEAM_OTHER;

        AlteracValleyMineInfo& mineInfo = _mineInfo[uint8(mine)];

        if (mineInfo.Owner == team && !initial)
            return;

        mineInfo.Owner = team;

        SendMineWorldStates(mine);

        uint8 const textId = team == ALLIANCE ? mineInfo.StaticInfo.TextIdAlliance : mineInfo.StaticInfo.TextIdHorde;

        std::string const stringId = team == ALLIANCE ? "bg_av_herald_mine_alliance" : "bg_av_herald_mine_horde";

        if (Creature const* herald = FindHerald(stringId))
            herald->AI()->Talk(textId);
    }

    static BG_AV_Nodes GetNodeThroughObject(uint32 object)
    {
        switch (object)
        {
            case BG_AV_OBJECTID_AID_STATION_ALLIANCE_CONTROLLED:
            case BG_AV_OBJECTID_AID_STATION_HORDE_CONTESTED:
            case BG_AV_OBJECTID_AID_STATION_HORDE_CONTROLLED:
            case BG_AV_OBJECTID_AID_STATION_ALLIANCE_CONTESTED:
                return BG_AV_NODES_FIRSTAID_STATION;
            case BG_AV_OBJECTID_STORMPIKE_ALLIANCE_CONTROLLED:
            case BG_AV_OBJECTID_STORMPIKE_HORDE_CONTESTED:
            case BG_AV_OBJECTID_STORMPIKE_HORDE_CONTROLLED:
            case BG_AV_OBJECTID_STORMPIKE_ALLIANCE_CONTESTED:
                return BG_AV_NODES_STORMPIKE_GRAVE;
            case BG_AV_OBJECTID_STONEHEARTH_HORDE_CONTESTED:
            case BG_AV_OBJECTID_STONEHEARTH_HORDE_CONTROLLED:
            case BG_AV_OBJECTID_STONEHEARTH_ALLIANCE_CONTESTED:
            case BG_AV_OBJECTID_STONEHEARTH_ALLIANCE_CONTROLLED:
                return BG_AV_NODES_STONEHEART_GRAVE;
            case BG_AV_OBJECTID_SNOWFALL_NEUTRAL:
            case BG_AV_OBJECTID_SNOWFALL_HORDE_CONTESTED:
            case BG_AV_OBJECTID_SNOWFALL_ALLIANCE_CONTESTED:
            case BG_AV_OBJECTID_SNOWFALL_HORDE_CONTROLLED:
            case BG_AV_OBJECTID_SNOWFALL_ALLIANCE_CONTROLLED:
                return BG_AV_NODES_SNOWFALL_GRAVE;
            case BG_AV_OBJECTID_ICEBLOOD_HORDE_CONTROLLED:
            case BG_AV_OBJECTID_ICEBLOOD_ALLIANCE_CONTESTED:
            case BG_AV_OBJECTID_ICEBLOOD_ALLIANCE_CONTROLLED:
            case BG_AV_OBJECTID_ICEBLOOD_HORDE_CONTESTED:
                return BG_AV_NODES_ICEBLOOD_GRAVE;
            case BG_AV_OBJECTID_FROSTWOLF_HORDE_CONTROLLED:
            case BG_AV_OBJECTID_FROSTWOLF_ALLIANCE_CONTESTED:
            case BG_AV_OBJECTID_FROSTWOLF_ALLIANCE_CONTROLLED:
            case BG_AV_OBJECTID_FROSTWOLF_HORDE_CONTESTED:
                return BG_AV_NODES_FROSTWOLF_GRAVE;
            case BG_AV_OBJECTID_FROSTWOLF_HUT_HORDE_CONTROLLED:
            case BG_AV_OBJECTID_FROSTWOLF_HUT_ALLIANCE_CONTESTED:
            case BG_AV_OBJECTID_FROSTWOLF_HUT_ALLIANCE_CONTROLLED:
            case BG_AV_OBJECTID_FROSTWOLF_HUT_HORDE_CONTESTED:
                return BG_AV_NODES_FROSTWOLF_HUT;
            case BG_AV_OBJECTID_SOUTH_BUNKER_CONTROLLED_TOWER_BANNER:
            case BG_AV_OBJECTID_SOUTH_BUNKER_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_SOUTH_BUNKER_CONTESTED_BANNER:
            case BG_AV_OBJECTID_SOUTH_BUNKER_CONTESTED_TOWER_BANNER:
                return BG_AV_NODES_DUNBALDAR_SOUTH;
            case BG_AV_OBJECTID_NORTH_BUNKER_CONTROLLED_TOWER_BANNER:
            case BG_AV_OBJECTID_NORTH_BUNKER_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_NORTH_BUNKER_CONTESTED_BANNER:
            case BG_AV_OBJECTID_NORTH_BUNKER_CONTESTED_TOWER_BANNER:
                return BG_AV_NODES_DUNBALDAR_NORTH;
            case BG_AV_OBJECTID_EAST_TOWER_CONTROLLED_TOWER_BANNER:
            case BG_AV_OBJECTID_EAST_TOWER_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_EAST_TOWER_CONTESTED_BANNER:
            case BG_AV_OBJECTID_EAST_TOWER_CONTESTED_TOWER_BANNER:
                return BG_AV_NODES_FROSTWOLF_ETOWER;
            case BG_AV_OBJECTID_WEST_TOWER_CONTROLLED_TOWER_BANNER:
            case BG_AV_OBJECTID_WEST_TOWER_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_WEST_TOWER_CONTESTED_BANNER:
            case BG_AV_OBJECTID_WEST_TOWER_CONTESTED_TOWER_BANNER:
                return BG_AV_NODES_FROSTWOLF_WTOWER;
            case BG_AV_OBJECTID_TOWER_POINT_CONTROLLED_TOWER_BANNER:
            case BG_AV_OBJECTID_TOWER_POINT_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_TOWER_POINT_CONTESTED_BANNER:
            case BG_AV_OBJECTID_TOWER_POINT_CONTESTED_TOWER_BANNER:
                return BG_AV_NODES_TOWER_POINT;
            case BG_AV_OBJECTID_ICEBLOOD_TOWER_CONTROLLED_TOWER_BANNER:
            case BG_AV_OBJECTID_ICEBLOOD_TOWER_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_ICEBLOOD_TOWER_CONTESTED_BANNER:
            case BG_AV_OBJECTID_ICEBLOOD_TOWER_CONTESTED_TOWER_BANNER:
                return BG_AV_NODES_ICEBLOOD_TOWER;
            case BG_AV_OBJECTID_STONEHEARTH_BUNKER_CONTROLLED_TOWER_BANNER:
            case BG_AV_OBJECTID_STONEHEARTH_BUNKER_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_STONEHEARTH_BUNKER_CONTESTED_BANNER:
            case BG_AV_OBJECTID_STONEHEARTH_BUNKER_CONTESTED_TOWER_BANNER:
                return BG_AV_NODES_STONEHEART_BUNKER;
            case BG_AV_OBJECTID_ICEWING_BUNKER_CONTROLLED_TOWER_BANNER:
            case BG_AV_OBJECTID_ICEWING_BUNKER_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_ICEWING_BUNKER_CONTESTED_BANNER:
            case BG_AV_OBJECTID_ICEWING_BUNKER_CONTESTED_TOWER_BANNER:
                return BG_AV_NODES_ICEWING_BUNKER;
            default:
                TC_LOG_ERROR("bg.battleground", "BattlegroundAV: ERROR! GetPlace got a wrong object :(");
                ABORT();
                return BG_AV_Nodes(0);
        }
    }

    void HandleInteractCapturableObject(Player* player, GameObject* target)
    {
        if (!player || !target)
            return;

        switch (target->GetEntry())
        {
            // graveyards
            case BG_AV_OBJECTID_AID_STATION_ALLIANCE_CONTROLLED:
            case BG_AV_OBJECTID_AID_STATION_HORDE_CONTROLLED:
            case BG_AV_OBJECTID_FROSTWOLF_ALLIANCE_CONTROLLED:
            case BG_AV_OBJECTID_FROSTWOLF_HORDE_CONTROLLED:
            case BG_AV_OBJECTID_FROSTWOLF_HUT_ALLIANCE_CONTROLLED:
            case BG_AV_OBJECTID_FROSTWOLF_HUT_HORDE_CONTROLLED:
            case BG_AV_OBJECTID_ICEBLOOD_ALLIANCE_CONTROLLED:
            case BG_AV_OBJECTID_ICEBLOOD_HORDE_CONTROLLED:
            case BG_AV_OBJECTID_STONEHEARTH_ALLIANCE_CONTROLLED:
            case BG_AV_OBJECTID_STONEHEARTH_HORDE_CONTROLLED:
            case BG_AV_OBJECTID_STORMPIKE_ALLIANCE_CONTROLLED:
            case BG_AV_OBJECTID_STORMPIKE_HORDE_CONTROLLED:
                // Snowfall
            case BG_AV_OBJECTID_SNOWFALL_NEUTRAL:
            case BG_AV_OBJECTID_SNOWFALL_ALLIANCE_CONTROLLED:
            case BG_AV_OBJECTID_SNOWFALL_HORDE_CONTROLLED:
                // towers
            case BG_AV_OBJECTID_EAST_TOWER_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_WEST_TOWER_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_TOWER_POINT_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_ICEBLOOD_TOWER_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_STONEHEARTH_BUNKER_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_ICEWING_BUNKER_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_SOUTH_BUNKER_CONTROLLED_BANNER:
            case BG_AV_OBJECTID_NORTH_BUNKER_CONTROLLED_BANNER:
                EventPlayerAssaultsPoint(player, target->GetEntry());
                break;
                // graveyards
            case BG_AV_OBJECTID_AID_STATION_ALLIANCE_CONTESTED:
            case BG_AV_OBJECTID_AID_STATION_HORDE_CONTESTED:
            case BG_AV_OBJECTID_FROSTWOLF_ALLIANCE_CONTESTED:
            case BG_AV_OBJECTID_FROSTWOLF_HORDE_CONTESTED:
            case BG_AV_OBJECTID_FROSTWOLF_HUT_ALLIANCE_CONTESTED:
            case BG_AV_OBJECTID_FROSTWOLF_HUT_HORDE_CONTESTED:
            case BG_AV_OBJECTID_ICEBLOOD_ALLIANCE_CONTESTED:
            case BG_AV_OBJECTID_ICEBLOOD_HORDE_CONTESTED:
            case BG_AV_OBJECTID_STONEHEARTH_ALLIANCE_CONTESTED:
            case BG_AV_OBJECTID_STONEHEARTH_HORDE_CONTESTED:
            case BG_AV_OBJECTID_STORMPIKE_ALLIANCE_CONTESTED:
            case BG_AV_OBJECTID_STORMPIKE_HORDE_CONTESTED:
                // towers
            case BG_AV_OBJECTID_EAST_TOWER_CONTESTED_BANNER:
            case BG_AV_OBJECTID_WEST_TOWER_CONTESTED_BANNER:
            case BG_AV_OBJECTID_TOWER_POINT_CONTESTED_BANNER:
            case BG_AV_OBJECTID_ICEBLOOD_TOWER_CONTESTED_BANNER:
            case BG_AV_OBJECTID_STONEHEARTH_BUNKER_CONTESTED_BANNER:
            case BG_AV_OBJECTID_ICEWING_BUNKER_CONTESTED_BANNER:
            case BG_AV_OBJECTID_SOUTH_BUNKER_CONTESTED_BANNER:
            case BG_AV_OBJECTID_NORTH_BUNKER_CONTESTED_BANNER:
                EventPlayerDefendsPoint(player, target->GetEntry());
                break;
                // Snowfall special cases (either defend/assault)
            case BG_AV_OBJECTID_SNOWFALL_ALLIANCE_CONTESTED:
            case BG_AV_OBJECTID_SNOWFALL_HORDE_CONTESTED:
            {
                BG_AV_Nodes const node = GetNodeThroughObject(target->GetEntry());
                if (_nodes[node].TotalOwner == TEAM_OTHER)
                    EventPlayerAssaultsPoint(player, target->GetEntry());
                else
                    EventPlayerDefendsPoint(player, target->GetEntry());
                break;
            }
            default:
                break;
        }
    }

    void EventPlayerDefendsPoint(Player* player, uint32 object)
    {
        BG_AV_Nodes node = GetNodeThroughObject(object);

        Team const owner = _nodes[node].Owner;
        Team const team = battleground->GetPlayerTeam(player->GetGUID());

        if (owner == team || _nodes[node].State != POINT_ASSAULTED)
            return;

        TC_LOG_DEBUG("bg.battleground", "player defends point object: {} node: {}", object, node);
        if (_nodes[node].PrevOwner != team)
        {
            TC_LOG_ERROR("bg.battleground", "BG_AV: player defends point which doesn't belong to his team {}", node);
            return;
        }

        DefendNode(node, team);
        UpdateNodeWorldState(node);

        if (StaticNodeInfo const* nodeInfo = GetStaticNodeInfo(node))
        {
            std::string stringId;

            if (IsTower(node))
                stringId = nodeInfo->StringIds.AllianceOrDefend;
            else
                stringId = team == ALLIANCE ? nodeInfo->StringIds.AllianceOrDefend : nodeInfo->StringIds.HordeOrDestroy;

            if (Creature const* herald = FindHerald(stringId))
                herald->AI()->Talk(team == ALLIANCE ? nodeInfo->TextIds.AllianceCapture : nodeInfo->TextIds.HordeCapture);
        }

        // update the statistic for the defending player
        battleground->UpdatePvpStat(player, IsTower(node) ? PVP_STAT_TOWERS_DEFENDED : PVP_STAT_GRAVEYARDS_DEFENDED, 1);
        battlegroundMap->UpdateSpawnGroupConditions();
    }

    void EventPlayerAssaultsPoint(Player* player, uint32 object)
    {
        BG_AV_Nodes node = GetNodeThroughObject(object);
        Team const owner = _nodes[node].Owner; //maybe name it prevowner
        Team const team = battleground->GetPlayerTeam(player->GetGUID());

        TC_LOG_DEBUG("bg.battleground", "bg_av: player assaults point object {} node {}", object, node);
        if (owner == team || team == _nodes[node].TotalOwner)
            return; //surely a gm used this object

        AssaultNode(node, team);
        UpdateNodeWorldState(node);

        if (StaticNodeInfo const* nodeInfo = GetStaticNodeInfo(node))
        {
            std::string stringId;
            if (IsTower(node))
                stringId = nodeInfo->StringIds.HordeOrDestroy;
            else
                stringId = team == ALLIANCE ? nodeInfo->StringIds.AllianceOrDefend : nodeInfo->StringIds.HordeOrDestroy;

            if (Creature const* herald = FindHerald(stringId))
                herald->AI()->Talk(team == ALLIANCE ? nodeInfo->TextIds.AllianceAttack : nodeInfo->TextIds.HordeAttack);
        }

        // update the statistic for the assaulting player
        battleground->UpdatePvpStat(player, (IsTower(node)) ? PVP_STAT_TOWERS_ASSAULTED : PVP_STAT_GRAVEYARDS_ASSAULTED, 1);
        battlegroundMap->UpdateSpawnGroupConditions();
    }

    void UpdateNodeWorldState(BG_AV_Nodes node) const
    {
        if (StaticNodeInfo const* nodeInfo = GetStaticNodeInfo(node))
        {
            uint16 const owner = _nodes[node].Owner;
            BG_AV_States const state = _nodes[node].State;

            UpdateWorldState(nodeInfo->WorldStateIds.AllianceAssault, owner == ALLIANCE && state == POINT_ASSAULTED);
            UpdateWorldState(nodeInfo->WorldStateIds.AllianceControl, owner == ALLIANCE && state >= POINT_DESTROYED);
            UpdateWorldState(nodeInfo->WorldStateIds.HordeAssault, owner == HORDE && state == POINT_ASSAULTED);
            UpdateWorldState(nodeInfo->WorldStateIds.HordeControl, owner == HORDE && state >= POINT_DESTROYED);
            if (nodeInfo->WorldStateIds.Owner)
                UpdateWorldState(nodeInfo->WorldStateIds.Owner, owner == HORDE ? 2 : owner == ALLIANCE ? 1 : 0);
        }

        if (node == BG_AV_NODES_SNOWFALL_GRAVE)
            UpdateWorldState(AV_WS_SNOWFALL_GRAVEYARD_UNCONTROLLED, _nodes[node].Owner == TEAM_OTHER);
    }

    void SendMineWorldStates(AlteracValleyMine mine) const
    {
        AlteracValleyMineInfo const& mineInfo = _mineInfo[uint8(mine)];
        UpdateWorldState(mineInfo.StaticInfo.WorldStateHordeControlled, mineInfo.Owner == HORDE);
        UpdateWorldState(mineInfo.StaticInfo.WorldStateAllianceControlled, mineInfo.Owner == ALLIANCE);
        UpdateWorldState(mineInfo.StaticInfo.WorldStateNeutralControlled, mineInfo.Owner == TEAM_OTHER);
        UpdateWorldState(mineInfo.StaticInfo.WorldStateOwner, mineInfo.Owner == HORDE ? 2 : mineInfo.Owner == ALLIANCE ? 1 : 0);
    }

    void AssaultNode(BG_AV_Nodes node, Team team)
    {
        _nodes[node].PrevOwner = _nodes[node].Owner;
        _nodes[node].Owner = team;
        _nodes[node].PrevState = _nodes[node].State;
        _nodes[node].State = POINT_ASSAULTED;
    }

    void DestroyNode(BG_AV_Nodes node)
    {
        _nodes[node].TotalOwner = _nodes[node].Owner;
        _nodes[node].PrevOwner = _nodes[node].Owner;
        _nodes[node].PrevState = _nodes[node].State;
        _nodes[node].State = (_nodes[node].Tower) ? POINT_DESTROYED : POINT_CONTROLED;
    }

    void InitNode(BG_AV_Nodes node, Team team, bool tower)
    {
        _nodes[node].TotalOwner = team;
        _nodes[node].Owner = team;
        _nodes[node].PrevOwner = 0;
        _nodes[node].State = POINT_CONTROLED;
        _nodes[node].PrevState = _nodes[node].State;
        _nodes[node].State = POINT_CONTROLED;
        _nodes[node].Tower = tower;
    }

    void DefendNode(BG_AV_Nodes node, Team team)
    {
        _nodes[node].PrevOwner = _nodes[node].Owner;
        _nodes[node].Owner = team;
        _nodes[node].PrevState = _nodes[node].State;
        _nodes[node].State = POINT_CONTROLED;
    }

    Team GetPrematureWinner() override
    {
        uint32 const allianceScore = _teamResources[Battleground::GetTeamIndexByTeamId(ALLIANCE)];
        uint32 const hordeScore = _teamResources[Battleground::GetTeamIndexByTeamId(HORDE)];

        if (allianceScore > hordeScore)
            return ALLIANCE;
        if (hordeScore > allianceScore)
            return HORDE;

        return BattlegroundScript::GetPrematureWinner();
    }

    void OnGameObjectCreate(GameObject* gameObject) override
    {
        switch (gameObject->GetEntry())
        {
            case BG_AV_GHOST_GATE:
            case BG_AV_OBJECTID_GATE:
                _doorGUIDs.insert(gameObject->GetGUID());
                break;
            default:
                break;
        }
    }

    void OnCreatureCreate(Creature* creature) override
    {
        switch (creature->GetEntry())
        {
            case BG_AV_CREATURE_GALVANGAR:
                _galvangarGUID = creature->GetGUID();
                break;
            case BG_AV_CREATURE_BALINDA:
                _balindaGUID = creature->GetGUID();
                break;
            case BG_AV_CREATURE_HERALD:
                _heraldGUIDs.insert(creature->GetGUID());
                break;
            default:
                break;
        }
    }

    uint32 GetData(uint32 dataId) const override
    {
        auto getDefenderTierForTeam = [&](TeamId teamId) -> DefenderTier
        {
            if (_teamQuestStatus[teamId][0] < 500)
                return BG_AV_DEFENDER_TIER_DEFENDER;

            if (_teamQuestStatus[teamId][0] < 1000)
                return BG_AV_DEFENDER_TIER_SEASONED;

            if (_teamQuestStatus[teamId][0] < 1500)
                return BG_AV_DEFENDER_TIER_VETERAN;

            return BG_AV_DEFENDER_TIER_CHAMPION;
        };

        switch (dataId)
        {
            case DATA_DEFENDER_TIER_ALLIANCE:
                return getDefenderTierForTeam(TEAM_ALLIANCE);
            case DATA_DEFENDER_TIER_HORDE:
                return getDefenderTierForTeam(TEAM_HORDE);
            default:
                return BattlegroundScript::GetData(dataId);
        }
    }

    Creature* FindHerald(std::string_view stringId) const
    {
        for (ObjectGuid const& guid : _heraldGUIDs)
            if (Creature* creature = battlegroundMap->GetCreature(guid))
                if (creature->HasStringId(stringId))
                    return creature;

        return nullptr;
    }

    static StaticNodeInfo const* GetStaticNodeInfo(BG_AV_Nodes node)
    {
        for (const auto& nodeInfo : BGAVNodeInfo)
            if (nodeInfo.NodeId == node)
                return &nodeInfo;

        return nullptr;
    }

    bool IsTower(BG_AV_Nodes node) const { return _nodes[node].Tower; }

    void UpdateScore(Team team, int16 points)
    {
        ASSERT(team == ALLIANCE || team == HORDE);
        TeamId const teamindex = Battleground::GetTeamIndexByTeamId(team);
        _teamResources[teamindex] += points;

        UpdateWorldState(teamindex == TEAM_HORDE ? AV_WS_HORDE_REINFORCEMENTS : AV_WS_ALLIANCE_REINFORCEMENTS, _teamResources[teamindex]);
        if (points < 0)
        {
            if (_teamResources[teamindex] < 1)
            {
                _teamResources[teamindex] = 0;
                battleground->EndBattleground(teamindex == TEAM_HORDE ? ALLIANCE : HORDE);
            }
            else if (!_isInformedNearVictory[teamindex] && _teamResources[teamindex] < NEAR_LOSE_POINTS)
            {
                if (teamindex == TEAM_ALLIANCE)
                    battleground->SendBroadcastText(BG_AV_TEXT_ALLIANCE_NEAR_LOSE, CHAT_MSG_BG_SYSTEM_ALLIANCE);
                else
                    battleground->SendBroadcastText(BG_AV_TEXT_HORDE_NEAR_LOSE, CHAT_MSG_BG_SYSTEM_HORDE);
                battleground->PlaySoundToAll(AV_SOUND_NEAR_VICTORY);
                _isInformedNearVictory[teamindex] = true;
            }
        }
    }

private:
    /*variables */
    std::array<int32, PVP_TEAMS_COUNT> _teamResources;
    uint32 _teamQuestStatus[PVP_TEAMS_COUNT][9]; //[x][y] x=team y=questcounter

    std::array<BG_AV_NodeInfo, BG_AV_NODES_MAX> _nodes;

    TimeTracker _mineResourceTimer; //ticks for both teams

    std::array<AlteracValleyMineInfo, 2> _mineInfo;

    std::array<TimeTracker, PVP_TEAMS_COUNT> _captainBuffTimer;

    std::array<bool, PVP_TEAMS_COUNT> _isInformedNearVictory;
    GuidUnorderedSet _doorGUIDs;
    ObjectGuid _balindaGUID;
    ObjectGuid _galvangarGUID;
    GuidUnorderedSet _heraldGUIDs;
};

// Classic 1.60: vanilla Alterac Valley on map 30 (port of VMaNGOS BattleGroundAV). The retail script above needs the
// retail banner entries (one per node) and world state spawn group conditions; map 30 has the vanilla spawns, where one
// banner entry serves every graveyard and every node state is a VMaNGOS battleground event. Each event is a spawn group
// 300000 + event1 * 10 + event2 (world SQL 2026_10_12_40), switched here like VMaNGOS BattleGround::SpawnEvent.
// Groups of the state active at creation have no manual flag; the event table below mirrors that.
namespace ClassicAlteracValley
{
    static constexpr uint32 GroupBase = 300000;
    static constexpr uint8 EventNone = 255;
    static constexpr uint8 NodesMax = 15;
    static constexpr uint8 TowersMax = 23;                 // tower defenders: event TowersMax + node
    static constexpr uint8 GraveTypes = 4;                  // defender tiers (armor scraps)
    static constexpr uint8 MaxStates = 2;
    static constexpr int32 CaptureTime = 300000;            // 5 minutes
    static constexpr int32 MineTickTimer = 45000;
    static constexpr int32 MineReclaimTimer = 1200000;      // 20 minutes
    static constexpr int32 SnivvleYellTimer = 70000;
    static constexpr int32 InitialScore = 600;

    enum TeamIndex : uint8
    {
        CAV_TEAM_ALLIANCE           = 0,
        CAV_TEAM_HORDE              = 1,
        CAV_TEAM_NEUTRAL            = 2
    };

    enum NodeState : uint8
    {
        CAV_STATE_ASSAULTED         = 0,
        CAV_STATE_CONTROLLED        = 1
    };

    enum Nodes : uint8
    {
        CAV_NODE_FIRSTAID_STATION   = 0,
        CAV_NODE_STORMPIKE_GRAVE    = 1,
        CAV_NODE_STONEHEART_GRAVE   = 2,
        CAV_NODE_SNOWFALL_GRAVE     = 3,
        CAV_NODE_ICEBLOOD_GRAVE     = 4,
        CAV_NODE_FROSTWOLF_GRAVE    = 5,
        CAV_NODE_FROSTWOLF_HUT      = 6,
        CAV_NODE_DUNBALDAR_SOUTH    = 7,
        CAV_NODE_STONEHEART_BUNKER  = 10,
        CAV_NODE_ICEBLOOD_TOWER     = 11,
        CAV_NODE_FROSTWOLF_WTOWER   = 14
    };

    // VMaNGOS battleground_events (map 30) event1 values
    enum Events : uint8
    {
        CAV_EVENT_GRAVE_DEFENDERS   = 15,                   // + node, event2 = team * 4 + tier (neutral 8)
        CAV_EVENT_TOWER_MARSHALS    = 15,                   // + node, event2 = team * 2 + 1
        CAV_EVENT_MINE_BOSSES       = 46,                   // + mine, event2 = team
        CAV_EVENT_CAPTAIN_A         = 48,
        CAV_EVENT_CAPTAIN_H         = 49,
        CAV_EVENT_MINE              = 50,                   // + mine, event2 = team
        CAV_EVENT_COMMANDER_A_FIRST = 52,                   // 52 - 55 Alliance, 56 - 59 Horde wing commanders
        CAV_EVENT_COMMANDER_A_KARL  = 54,
        CAV_EVENT_COMMANDER_H_FIRST = 56,
        CAV_EVENT_COMMANDER_H_LOUIS = 57,
        CAV_EVENT_COMMANDER_H_LAST  = 59,
        CAV_EVENT_HERALD            = 60,
        CAV_EVENT_BOSS_A            = 61,
        CAV_EVENT_BOSS_H            = 62,
        CAV_EVENT_CAPTAIN_DEAD_A    = 63,
        CAV_EVENT_CAPTAIN_DEAD_H    = 64,
        CAV_EVENT_SNIVVLE           = 65,
        CAV_EVENT_EXPLOSIVES_A      = 66,
        CAV_EVENT_EXPLOSIVES_H      = 67,
        CAV_EVENT_LIEUTENANT_A      = 68,
        CAV_EVENT_LIEUTENANT_H      = 69,
        CAV_EVENT_SUPPLIES_100      = 80,                   // + team; 82, 84, 86 for 200 - 400
        CAV_EVENT_TAMED_5           = 90,                   // + team; 92, 94, 96 for 10 - 20
        CAV_EVENT_LANDMINES_HORDE   = 100,
        CAV_EVENT_LANDMINES_ALLIANCE = 101,
        CAV_EVENT_GHOST_GATE        = 253
    };

    enum Creatures : uint32
    {
        CAV_NPC_LANDMINES_LAYER_A   = 13356,
        CAV_NPC_LANDMINES_LAYER_H   = 13357,
        CAV_NPC_LANDMINES_EXPERT_A  = 13598,
        CAV_NPC_LANDMINES_EXPERT_H  = 13597
    };

    enum GameObjects : uint32
    {
        CAV_GO_GATE                 = 180424
    };

    enum Spells : uint32
    {
        CAV_SPELL_BOSS_KILL_QUEST   = 23658,
        CAV_SPELL_CAPTAIN_BUFF_A    = 23693,
        CAV_SPELL_CAPTAIN_BUFF_H    = 22751,
        CAV_SPELL_UNITS_SEASONED    = 28418,                // + 1 veteran, + 2 champion
        CAV_SPELL_SPIRIT_OF_REDEMPTION = 27827
    };

    enum Sounds : uint32
    {
        CAV_SOUND_ALLIANCE_ASSAULTS = 8212,
        CAV_SOUND_HORDE_ASSAULTS    = 8174,
        CAV_SOUND_ALLIANCE_GOOD     = 8173,
        CAV_SOUND_HORDE_GOOD        = 8213,
        CAV_SOUND_TOWER_DEFEND      = 8192
    };

    enum Quests : uint32
    {
        CAV_QUEST_GRAVE_A           = 7081,
        CAV_QUEST_GRAVE_H           = 7082,
        CAV_QUEST_TOWER_H           = 7101,
        CAV_QUEST_TOWER_A           = 7102,
        CAV_QUEST_MINE_A            = 7122,
        CAV_QUEST_MINE_H            = 7124,
        CAV_QUEST_COMMANDER_A       = 7281,
        CAV_QUEST_COMMANDER_H       = 7282,
        CAV_QUEST_EXPLOSIVES_A      = 7367,
        CAV_QUEST_EXPLOSIVES_H      = 7368
    };

    enum WorldStates : int32
    {
        CAV_WS_SCORE_ALLIANCE       = 3127,
        CAV_WS_SCORE_HORDE          = 3128,
        CAV_WS_SHOW_SCORE_HORDE     = 3133,
        CAV_WS_SHOW_SCORE_ALLIANCE  = 3134,
        CAV_WS_SNOWFALL_NEUTRAL     = 1966,
        CAV_WS_DREKTHAR_ALIVE       = 601,
        CAV_WS_VANNDAR_ALIVE        = 602,
        CAV_WS_BALINDA_ALIVE        = 1351,
        CAV_WS_GALVANGAR_ALIVE      = 1352,
        CAV_WS_IVUS_COUNT           = 1043,
        CAV_WS_LOKHOLAR_COUNT       = 923
    };

    enum Factions : uint32
    {
        CAV_FACTION_STORMPIKE       = 730,
        CAV_FACTION_FROSTWOLF       = 729
    };

    enum PvpStats : uint32
    {
        CAV_PVP_STAT_TOWERS_ASSAULTED     = 61,
        CAV_PVP_STAT_GRAVEYARDS_ASSAULTED = 63,
        CAV_PVP_STAT_TOWERS_DEFENDED      = 64,
        CAV_PVP_STAT_GRAVEYARDS_DEFENDED  = 65
    };

    // honor (bonus kills) and reputation, VMaNGOS BattleGroundAV.h (normal, holiday weekend)
    static constexpr uint32 KillBoss = 6, KillCaptain = 3, KillCommander = 1, KillTower = 2;
    static constexpr uint32 KillSurvivingTower = 3, KillSurvivingCaptain = 3, KillSurvivingGrave = 1, KillSurvivingMine = 1;
    static constexpr std::array<uint32, 2> RepBoss = { 350, 525 }, RepCaptain = { 125, 185 }, RepCommander = { 12, 18 };
    static constexpr std::array<uint32, 2> RepTower = { 12, 18 }, RepSurvivingTower = { 12, 18 }, RepSurvivingCaptain = { 125, 175 };
    static constexpr std::array<uint32, 2> RepOwnedGrave = { 12, 18 }, RepOwnedMine = { 24, 36 };
    static constexpr int32 ResTower = 75, ResCaptain = 100;

    // alliance assaulted, alliance controlled, horde assaulted, horde controlled (index team * 2 + state)
    static constexpr std::array<std::array<int32, 4>, NodesMax> NodeWorldStates =
    { {
        { 1326, 1325, 1328, 1327 },                         // Stormpike Aid Station
        { 1335, 1333, 1336, 1334 },                         // Stormpike Graveyard
        { 1304, 1302, 1303, 1301 },                         // Stonehearth Graveyard
        { 1343, 1341, 1344, 1342 },                         // Snowfall Graveyard
        { 1348, 1346, 1349, 1347 },                         // Iceblood Graveyard
        { 1339, 1337, 1340, 1338 },                         // Frostwolf Graveyard
        { 1331, 1329, 1332, 1330 },                         // Frostwolf Relief Hut
        { 1375, 1361, 1378, 1370 },                         // Dun Baldar South Bunker
        { 1374, 1362, 1379, 1371 },                         // Dun Baldar North Bunker
        { 1376, 1363, 1380, 1372 },                         // Icewing Bunker
        { 1377, 1364, 1381, 1373 },                         // Stonehearth Bunker
        { 1390, 1368, 1395, 1385 },                         // Iceblood Tower
        { 1389, 1367, 1394, 1384 },                         // Tower Point
        { 1388, 1366, 1393, 1383 },                         // East Frostwolf Tower
        { 1387, 1365, 1392, 1382 }                          // West Frostwolf Tower
    } };

    // 1.60 tower owner world states (1 Alliance, 2 Horde), nodes 7 - 14
    static constexpr std::array<int32, 8> TowerOwnerWorldStates = { 1181, 1182, 1183, 1184, 1188, 1187, 1186, 1185 };

    // herald (14848) creature_text groups: Alliance captures / Alliance assaults / Horde captures / Horde assaults
    static constexpr std::array<std::array<uint8, 4>, NodesMax> HeraldTexts =
    { {
        { 47, 48, 45, 46 }, {  1,  2,  3,  4 }, { 55, 56, 53, 54 }, {  5,  6,  7,  8 }, { 59, 60, 57, 58 },
        {  9, 10, 11, 12 }, { 51, 52, 49, 50 }, { 16, 15, 14, 13 }, { 20, 19, 18, 17 }, { 24, 23, 22, 21 },
        { 28, 27, 26, 25 }, { 44, 43, 42, 41 }, { 40, 39, 38, 37 }, { 36, 35, 34, 33 }, { 32, 31, 30, 29 }
    } };

    // mines: 0 north (Irondeep), 1 south (Coldtooth); world states alliance / horde / neutral controlled, owner
    static constexpr std::array<std::array<int32, 3>, 2> MineWorldStates = { { { 1358, 1359, 1360 }, { 1355, 1356, 1357 } } };
    static constexpr std::array<int32, 2> MineOwnerWorldStates = { 801, 804 };
    static constexpr std::array<std::array<uint8, 2>, 2> MineHeraldTexts = { { { 62, 64 }, { 61, 63 } } };
    static constexpr uint8 HeraldTextGeneralDeadH = 65, HeraldTextGeneralDeadA = 66;

    static constexpr Team TeamOf(uint8 teamIdx) { return teamIdx == CAV_TEAM_ALLIANCE ? ALLIANCE : HORDE; }
    static constexpr uint32 FactionOf(uint8 teamIdx) { return teamIdx == CAV_TEAM_ALLIANCE ? CAV_FACTION_STORMPIKE : CAV_FACTION_FROSTWOLF; }
    static constexpr uint32 GroupFor(uint8 event1, uint8 event2) { return GroupBase + event1 * 10 + event2; }

    // VMaNGOS event (event1, event2) of a spawn, from its spawn group; false when it is not an AV event spawn
    static bool GetSpawnEvent(SpawnData const* data, uint8& event1, uint8& event2)
    {
        if (!data || !data->spawnGroupData)
            return false;
        uint32 const groupId = data->spawnGroupData->groupId;
        if (groupId < GroupBase || groupId >= GroupBase + 2560)
            return false;
        event1 = uint8((groupId - GroupBase) / 10);
        event2 = uint8((groupId - GroupBase) % 10);
        return true;
    }

    struct NodeInfo
    {
        uint8 TotalOwner = CAV_TEAM_NEUTRAL;
        uint8 Owner = CAV_TEAM_NEUTRAL;
        uint8 PrevOwner = CAV_TEAM_NEUTRAL;
        uint8 PrevOtherOwner = CAV_TEAM_NEUTRAL;
        uint8 State = CAV_STATE_CONTROLLED;
        uint8 PrevState = CAV_STATE_CONTROLLED;
        int32 Timer = 0;
        bool Tower = false;
    };
}

struct battleground_classic_alterac_valley : BattlegroundScript
{
    explicit battleground_classic_alterac_valley(BattlegroundMap* map) : BattlegroundScript(map)
    {
        using namespace ClassicAlteracValley;
        _holiday = sBattlegroundMgr->IsBGWeekend(battleground->GetTypeID()) ? 1 : 0;
        _activeEvents.fill(EventNone);
        for (auto& team : _teamQuestStatus)
            team.fill(0);
        _scores = { InitialScore, InitialScore };
        _captainBuffTimer = { int32(120000 + urand(0, 4) * 60000), int32(120000 + urand(0, 4) * 60000) };
        _mineOwner = { CAV_TEAM_NEUTRAL, CAV_TEAM_NEUTRAL };
        _mineTimer = { MineTickTimer, MineTickTimer };
        _mineReclaimTimer = { 0, 0 };

        // VMaNGOS BattleGroundAV::Reset: the matching spawn groups are spawned in OnInit
        for (uint8 node = CAV_NODE_FIRSTAID_STATION; node <= CAV_NODE_STONEHEART_GRAVE; ++node)
            InitNode(node, CAV_TEAM_ALLIANCE, false);
        for (uint8 node = CAV_NODE_DUNBALDAR_SOUTH; node <= CAV_NODE_STONEHEART_BUNKER; ++node)
            InitNode(node, CAV_TEAM_ALLIANCE, true);
        for (uint8 node = CAV_NODE_ICEBLOOD_GRAVE; node <= CAV_NODE_FROSTWOLF_HUT; ++node)
            InitNode(node, CAV_TEAM_HORDE, false);
        for (uint8 node = CAV_NODE_ICEBLOOD_TOWER; node <= CAV_NODE_FROSTWOLF_WTOWER; ++node)
            InitNode(node, CAV_TEAM_HORDE, true);
        InitNode(CAV_NODE_SNOWFALL_GRAVE, CAV_TEAM_NEUTRAL, false);

        for (uint8 mine = 0; mine < 2; ++mine)
        {
            _activeEvents[CAV_EVENT_MINE_BOSSES + mine] = CAV_TEAM_NEUTRAL;
            _activeEvents[CAV_EVENT_MINE + mine] = CAV_TEAM_NEUTRAL;
        }
        for (uint8 event1 : { uint8(CAV_EVENT_CAPTAIN_A), uint8(CAV_EVENT_CAPTAIN_H), uint8(CAV_EVENT_HERALD), uint8(CAV_EVENT_SNIVVLE),
            uint8(CAV_EVENT_BOSS_A), uint8(CAV_EVENT_BOSS_H), uint8(CAV_EVENT_LANDMINES_ALLIANCE), uint8(CAV_EVENT_LANDMINES_HORDE),
            uint8(CAV_EVENT_EXPLOSIVES_A), uint8(CAV_EVENT_EXPLOSIVES_H), uint8(CAV_EVENT_LIEUTENANT_A), uint8(CAV_EVENT_LIEUTENANT_H),
            uint8(CAV_EVENT_GHOST_GATE) })
            _activeEvents[event1] = 0;
        for (uint8 event1 = CAV_EVENT_COMMANDER_A_FIRST; event1 <= CAV_EVENT_COMMANDER_H_LAST; ++event1)
            _activeEvents[event1] = 0;
        // supply crates and stable mounts: reset state (VMaNGOS spawns it before the start)
        for (uint8 event1 = CAV_EVENT_SUPPLIES_100; event1 < CAV_EVENT_SUPPLIES_100 + 8; ++event1)
            _activeEvents[event1] = 2;
        for (uint8 event1 = CAV_EVENT_TAMED_5; event1 < CAV_EVENT_TAMED_5 + 8; ++event1)
            _activeEvents[event1] = 2;
    }

    void OnInit() override
    {
        using namespace ClassicAlteracValley;
        UpdateWorldState(CAV_WS_SCORE_ALLIANCE, _scores[CAV_TEAM_ALLIANCE]);
        UpdateWorldState(CAV_WS_SCORE_HORDE, _scores[CAV_TEAM_HORDE]);
        // vanilla has no reinforcement defeat (VMaNGOS removed it), so the counters stay hidden
        UpdateWorldState(CAV_WS_SHOW_SCORE_ALLIANCE, 0);
        UpdateWorldState(CAV_WS_SHOW_SCORE_HORDE, 0);
        UpdateWorldState(CAV_WS_DREKTHAR_ALIVE, 1);
        UpdateWorldState(CAV_WS_VANNDAR_ALIVE, 1);
        UpdateWorldState(CAV_WS_BALINDA_ALIVE, 1);
        UpdateWorldState(CAV_WS_GALVANGAR_ALIVE, 1);
        for (uint8 node = 0; node < NodesMax; ++node)
            UpdateNodeWorldState(node);
        for (uint8 mine = 0; mine < 2; ++mine)
            SendMineWorldStates(mine);

        // all event spawn groups are manual: a despawned group stays despawned
        for (size_t event1 = 0; event1 < _activeEvents.size(); ++event1)
        {
            if (_activeEvents[event1] == EventNone)
                continue;
            uint32 const groupId = GroupFor(uint8(event1), _activeEvents[event1]);
            if (battlegroundMap->GetSpawnGroupData(groupId))
                battlegroundMap->SpawnGroupSpawn(groupId, true);
        }
    }

    void OnStart() override
    {
        using namespace ClassicAlteracValley;
        for (ObjectGuid const& guid : _doors)
            if (GameObject* door = battlegroundMap->GetGameObject(guid))
                door->UseDoorOrButton();

        SpawnEvent(CAV_EVENT_GHOST_GATE, 0, false);
    }

    void OnGameObjectCreate(GameObject* gameObject) override
    {
        if (gameObject->GetEntry() == ClassicAlteracValley::CAV_GO_GATE)
            _doors.insert(gameObject->GetGUID());
    }

    void OnCreatureCreate(Creature* creature) override
    {
        using namespace ClassicAlteracValley;
        uint8 event1, event2;
        if (!GetSpawnEvent(creature->GetCreatureData(), event1, event2))
            return;

        switch (event1)
        {
            case CAV_EVENT_HERALD: _heraldGUID = creature->GetGUID(); break;
            case CAV_EVENT_SNIVVLE: _snivvleGUID = creature->GetGUID(); break;
            case CAV_EVENT_CAPTAIN_A: _captainGUIDs[CAV_TEAM_ALLIANCE] = creature->GetGUID(); break;
            case CAV_EVENT_CAPTAIN_H: _captainGUIDs[CAV_TEAM_HORDE] = creature->GetGUID(); break;
            default: break;
        }
    }

    void OnUpdate(uint32 diff) override
    {
        using namespace ClassicAlteracValley;
        if (!_snivvleDone)
        {
            _snivvleTimer += int32(diff);
            if (_snivvleTimer >= SnivvleYellTimer)
            {
                _snivvleDone = true;
                if (Creature* snivvle = battlegroundMap->GetCreature(_snivvleGUID))
                    snivvle->AI()->Talk(0);
            }
        }

        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return;

        // captain buffs
        for (uint8 team = CAV_TEAM_ALLIANCE; team <= CAV_TEAM_HORDE; ++team)
        {
            _captainBuffTimer[team] -= int32(diff);
            if (_captainBuffTimer[team] > 0)
                continue;

            _captainBuffTimer[team] = int32(120000 + urand(0, 4) * 60000);
            if (_activeEvents[CAV_EVENT_CAPTAIN_DEAD_A + team] == 0)
                continue;

            battleground->CastSpellOnTeam(team == CAV_TEAM_ALLIANCE ? CAV_SPELL_CAPTAIN_BUFF_A : CAV_SPELL_CAPTAIN_BUFF_H, TeamOf(team));
            if (Creature* captain = battlegroundMap->GetCreature(_captainGUIDs[team]))
                if (captain->IsAlive() && captain->AI())
                    captain->AI()->DoAction(ACTION_BUFF_YELL);
        }

        // mine resources, mine reclaim by the neutral owner
        for (uint8 mine = 0; mine < 2; ++mine)
        {
            if (_mineOwner[mine] == CAV_TEAM_NEUTRAL)
                continue;

            _mineTimer[mine] -= int32(diff);
            if (_mineTimer[mine] <= 0)
            {
                UpdateScore(_mineOwner[mine], 1);
                _mineTimer[mine] = MineTickTimer;
            }

            _mineReclaimTimer[mine] -= int32(diff);
            if (_mineReclaimTimer[mine] <= 0)
                ChangeMineOwner(mine, CAV_TEAM_NEUTRAL);
        }

        // assaulted nodes: towers burn down, graveyards go to the assaulting team
        for (uint8 node = 0; node < NodesMax; ++node)
        {
            if (_nodes[node].State != CAV_STATE_ASSAULTED)
                continue;

            _nodes[node].Timer -= int32(diff);
            if (_nodes[node].Timer <= 0)
                EventPlayerDestroyedPoint(node);
        }
    }

    void OnPlayerKilled(Player* victim, Player* /*killer*/) override
    {
        using namespace ClassicAlteracValley;
        if (battleground->GetStatus() != STATUS_IN_PROGRESS || victim->HasAura(CAV_SPELL_SPIRIT_OF_REDEMPTION))
            return;

        Team const team = battleground->GetPlayerTeam(victim->GetGUID());
        if (team == ALLIANCE || team == HORDE)
            UpdateScore(team == ALLIANCE ? CAV_TEAM_ALLIANCE : CAV_TEAM_HORDE, -1);
    }

    void OnUnitKilled(Creature* victim, Unit* killer) override
    {
        using namespace ClassicAlteracValley;
        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return;

        switch (victim->GetEntry())
        {
            case CAV_NPC_LANDMINES_LAYER_A:
            case CAV_NPC_LANDMINES_LAYER_H:
                // landmines stop respawning
                SetEventRespawn(victim->GetEntry() == CAV_NPC_LANDMINES_LAYER_A ? CAV_EVENT_LANDMINES_ALLIANCE : CAV_EVENT_LANDMINES_HORDE, 0, false);
                return;
            case CAV_NPC_LANDMINES_EXPERT_A:
            case CAV_NPC_LANDMINES_EXPERT_H:
                // all landmines go away
                SpawnEvent(victim->GetEntry() == CAV_NPC_LANDMINES_EXPERT_A ? CAV_EVENT_LANDMINES_ALLIANCE : CAV_EVENT_LANDMINES_HORDE, 0, false);
                return;
            default:
                break;
        }

        uint8 event1, event2;
        if (!GetSpawnEvent(victim->GetCreatureData(), event1, event2))
            return;

        Player* player = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        if (!player)
            return;

        Team const killerTeam = battleground->GetPlayerTeam(player->GetGUID());
        if (killerTeam != ALLIANCE && killerTeam != HORDE)
            return;
        uint8 const killerIdx = killerTeam == ALLIANCE ? CAV_TEAM_ALLIANCE : CAV_TEAM_HORDE;

        switch (event1)
        {
            case CAV_EVENT_BOSS_A:
            case CAV_EVENT_BOSS_H:
            {
                uint8 const winner = event1 == CAV_EVENT_BOSS_A ? CAV_TEAM_HORDE : CAV_TEAM_ALLIANCE;
                UpdateWorldState(event1 == CAV_EVENT_BOSS_A ? CAV_WS_VANNDAR_ALIVE : CAV_WS_DREKTHAR_ALIVE, 0);
                battleground->CastSpellOnTeam(CAV_SPELL_BOSS_KILL_QUEST, TeamOf(winner));
                battleground->RewardReputationToTeam(FactionOf(winner), RepBoss[_holiday], TeamOf(winner));
                battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(KillBoss), TeamOf(winner));
                HeraldTalk(event1 == CAV_EVENT_BOSS_A ? HeraldTextGeneralDeadA : HeraldTextGeneralDeadH);
                battleground->EndBattleground(TeamOf(winner));
                break;
            }
            case CAV_EVENT_CAPTAIN_A:
            case CAV_EVENT_CAPTAIN_H:
            {
                uint8 const loser = event1 == CAV_EVENT_CAPTAIN_A ? CAV_TEAM_ALLIANCE : CAV_TEAM_HORDE;
                uint8 const winner = loser == CAV_TEAM_ALLIANCE ? CAV_TEAM_HORDE : CAV_TEAM_ALLIANCE;
                if (_activeEvents[CAV_EVENT_CAPTAIN_DEAD_A + loser] == 0)
                    break;
                UpdateWorldState(loser == CAV_TEAM_ALLIANCE ? CAV_WS_BALINDA_ALIVE : CAV_WS_GALVANGAR_ALIVE, 0);
                battleground->RewardReputationToTeam(FactionOf(winner), RepCaptain[_holiday], TeamOf(winner));
                battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(KillCaptain), TeamOf(winner));
                UpdateScore(loser, -ResCaptain);
                SpawnEvent(CAV_EVENT_CAPTAIN_DEAD_A + loser, 0, true);   // burning keep
                break;
            }
            case CAV_EVENT_MINE_BOSSES:
            case CAV_EVENT_MINE_BOSSES + 1:
                ChangeMineOwner(event1 - CAV_EVENT_MINE_BOSSES, killerIdx);
                break;
            case CAV_EVENT_EXPLOSIVES_A:
            case CAV_EVENT_EXPLOSIVES_H:
                CompleteQuestForAll(event1 == CAV_EVENT_EXPLOSIVES_A ? CAV_QUEST_EXPLOSIVES_A : CAV_QUEST_EXPLOSIVES_H);
                SetEventRespawn(event1, 0, false);
                break;
            case CAV_EVENT_LIEUTENANT_A:
            case CAV_EVENT_LIEUTENANT_H:
            {
                uint8 const winner = event1 == CAV_EVENT_LIEUTENANT_A ? CAV_TEAM_HORDE : CAV_TEAM_ALLIANCE;
                battleground->RewardReputationToTeam(FactionOf(winner), RepCommander[_holiday], TeamOf(winner));
                battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(KillCommander), TeamOf(winner));
                break;
            }
            default:
                if (event1 >= CAV_EVENT_COMMANDER_A_FIRST && event1 <= CAV_EVENT_COMMANDER_H_LAST)
                {
                    uint8 const winner = event1 < CAV_EVENT_COMMANDER_H_FIRST ? CAV_TEAM_HORDE : CAV_TEAM_ALLIANCE;
                    battleground->RewardReputationToTeam(FactionOf(winner), RepCommander[_holiday], TeamOf(winner));
                    battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(KillCommander), TeamOf(winner));
                    SetEventRespawn(event1, 0, false);
                    if (event1 == CAV_EVENT_COMMANDER_A_KARL)
                        CompleteQuestForAll(CAV_QUEST_COMMANDER_A);
                    else if (event1 == CAV_EVENT_COMMANDER_H_LOUIS)
                        CompleteQuestForAll(CAV_QUEST_COMMANDER_H);
                }
                break;
        }
    }

    void DoAction(uint32 actionId, WorldObject* source, WorldObject* target) override
    {
        using namespace ClassicAlteracValley;
        Player* player = Object::ToPlayer(source);
        if (!player)
            return;

        if (actionId == ACTION_AV_INTERACT_CAPTURABLE_OBJECT)
        {
            if (GameObject* banner = Object::ToGameObject(target))
                HandleBannerClick(player, banner);
            return;
        }

        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return;

        Team const team = battleground->GetPlayerTeam(player->GetGUID());
        if (team != ALLIANCE && team != HORDE)
            return;
        uint8 const teamIdx = team == ALLIANCE ? CAV_TEAM_ALLIANCE : CAV_TEAM_HORDE;

        // VMaNGOS BattleGroundAV::HandleQuestComplete (team reputation on top of the quest reward)
        uint32 reputation = 0;
        switch (actionId)
        {
            case ACTION_TURN_IN_SCRAPS:
            {
                uint32& scraps = _teamQuestStatus[teamIdx][0];
                scraps += 20;
                reputation = 1;
                // supply crates every 100 resources
                switch (scraps % 500)
                {
                    case 100: SpawnEvent(CAV_EVENT_SUPPLIES_100 + teamIdx, 0, true); break;
                    case 200: SpawnEvent(CAV_EVENT_SUPPLIES_100 + 2 + teamIdx, 0, true); break;
                    case 300: SpawnEvent(CAV_EVENT_SUPPLIES_100 + 4 + teamIdx, 0, true); break;
                    case 400: SpawnEvent(CAV_EVENT_SUPPLIES_100 + 6 + teamIdx, 0, true); break;
                    case 0:
                        for (uint8 i = 0; i < 4; ++i)
                            SpawnEvent(CAV_EVENT_SUPPLIES_100 + i * 2 + teamIdx, 2, true);
                        break;
                    default:
                        break;
                }
                // 500 / 1000 / 1500: seasoned, veteran, champion defenders (VMaNGOS UpgradeArmor)
                if (scraps == 500 || scraps == 1000 || scraps == 1500)
                {
                    battleground->CastSpellOnTeam(CAV_SPELL_UNITS_SEASONED + scraps / 500 - 1, team);
                    for (uint8 node = 0; node < NodesMax; ++node)
                        if (_nodes[node].Owner == teamIdx && _nodes[node].State == CAV_STATE_CONTROLLED)
                            PopulateNode(node);
                }
                break;
            }
            case ACTION_TURN_IN_COMMANDER_1:
                ++_teamQuestStatus[teamIdx][1];
                reputation = 1;
                break;
            case ACTION_TURN_IN_COMMANDER_2:
                ++_teamQuestStatus[teamIdx][2];
                reputation = 2;
                break;
            case ACTION_TURN_IN_COMMANDER_3:
                ++_teamQuestStatus[teamIdx][3];
                reputation = 5;
                break;
            case ACTION_TURN_IN_BOSS_1:
                _teamQuestStatus[teamIdx][4] += 4;
                reputation = 4;
                [[fallthrough]];
            case ACTION_TURN_IN_BOSS_2:
                ++_teamQuestStatus[teamIdx][4];
                reputation += 1;
                UpdateWorldState(teamIdx == CAV_TEAM_ALLIANCE ? CAV_WS_IVUS_COUNT : CAV_WS_LOKHOLAR_COUNT, _teamQuestStatus[teamIdx][4]);
                break;
            case ACTION_TURN_IN_NEAR_MINE:
                ++_teamQuestStatus[teamIdx][5];
                reputation = 2;
                break;
            case ACTION_TURN_IN_OTHER_MINE:
                ++_teamQuestStatus[teamIdx][6];
                reputation = 3;
                break;
            case ACTION_TURN_IN_RIDER_HIDE:
                ++_teamQuestStatus[teamIdx][7];
                reputation = 1;
                break;
            case ACTION_TURN_IN_RIDER_TAME:
            {
                uint32 const tamed = ++_teamQuestStatus[teamIdx][8];
                reputation = 1;
                // stable mounts every 5 tamed rams / wolves
                switch (tamed % 25)
                {
                    case 5: SpawnEvent(CAV_EVENT_TAMED_5 + teamIdx, 0, true); break;
                    case 10: SpawnEvent(CAV_EVENT_TAMED_5 + 2 + teamIdx, 0, true); break;
                    case 15: SpawnEvent(CAV_EVENT_TAMED_5 + 4 + teamIdx, 0, true); break;
                    case 20: SpawnEvent(CAV_EVENT_TAMED_5 + 6 + teamIdx, 0, true); break;
                    default:
                        if (tamed == 1)
                            for (uint8 i = 0; i < 4; ++i)
                                SpawnEvent(CAV_EVENT_TAMED_5 + i * 2 + teamIdx, 2, true);
                        break;
                }
                break;
            }
            default:
                return;
        }

        if (reputation)
            battleground->RewardReputationToTeam(FactionOf(teamIdx), reputation, team);
    }

    void OnEnd(Team winner) override
    {
        using namespace ClassicAlteracValley;
        BattlegroundScript::OnEnd(winner);

        // VMaNGOS BattleGroundAV::EndBattleGround bonus honor / reputation
        std::array<uint32, 2> towersSurvived = { 0, 0 };
        std::array<uint32, 2> gravesOwned = { 0, 0 };
        std::array<uint32, 2> minesOwned = { 0, 0 };
        for (uint8 node = CAV_NODE_DUNBALDAR_SOUTH; node < NodesMax; ++node)
        {
            uint8 const originalOwner = node <= CAV_NODE_STONEHEART_BUNKER ? CAV_TEAM_ALLIANCE : CAV_TEAM_HORDE;
            if (_nodes[node].State == CAV_STATE_CONTROLLED && _nodes[node].TotalOwner == originalOwner)
                ++towersSurvived[originalOwner];
        }
        for (uint8 node = 0; node < NodesMax; ++node)
            if (!_nodes[node].Tower && _nodes[node].State == CAV_STATE_CONTROLLED && _nodes[node].Owner != CAV_TEAM_NEUTRAL
                && node != CAV_NODE_FROSTWOLF_HUT && node != CAV_NODE_FIRSTAID_STATION)
                ++gravesOwned[_nodes[node].Owner];
        for (uint8 owner : _mineOwner)
            if (owner != CAV_TEAM_NEUTRAL)
                ++minesOwned[owner];

        for (uint8 team = CAV_TEAM_ALLIANCE; team <= CAV_TEAM_HORDE; ++team)
        {
            if (towersSurvived[team])
            {
                battleground->RewardReputationToTeam(FactionOf(team), towersSurvived[team] * RepSurvivingTower[_holiday], TeamOf(team));
                battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(towersSurvived[team] * KillSurvivingTower), TeamOf(team));
            }
            if (gravesOwned[team])
            {
                battleground->RewardReputationToTeam(FactionOf(team), gravesOwned[team] * RepOwnedGrave[_holiday], TeamOf(team));
                battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(gravesOwned[team] * KillSurvivingGrave), TeamOf(team));
            }
            if (minesOwned[team])
            {
                battleground->RewardReputationToTeam(FactionOf(team), minesOwned[team] * RepOwnedMine[_holiday], TeamOf(team));
                battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(minesOwned[team] * KillSurvivingMine), TeamOf(team));
            }
            if (_activeEvents[CAV_EVENT_CAPTAIN_DEAD_A + team] != 0)
            {
                battleground->RewardReputationToTeam(FactionOf(team), RepSurvivingCaptain[_holiday], TeamOf(team));
                battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(KillSurvivingCaptain), TeamOf(team));
            }
        }

        // holiday weekend: 8 bonus kills for both teams, 2 more for the winner (VMaNGOS 1584 / 396 honor at level 60)
        if (_holiday)
        {
            for (Team team : { ALLIANCE, HORDE })
                battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(winner == team ? 10 : 8), team);
        }
    }

    Team GetPrematureWinner() override
    {
        using namespace ClassicAlteracValley;
        if (_scores[CAV_TEAM_ALLIANCE] > _scores[CAV_TEAM_HORDE])
            return ALLIANCE;
        if (_scores[CAV_TEAM_HORDE] > _scores[CAV_TEAM_ALLIANCE])
            return HORDE;
        return BattlegroundScript::GetPrematureWinner();
    }

private:
    // VMaNGOS BattleGround::SpawnEvent: spawning an event despawns the active event2 of the same event1
    void SpawnEvent(uint8 event1, uint8 event2, bool spawn)
    {
        using namespace ClassicAlteracValley;
        if (event2 == EventNone)
            return;

        if (spawn)
        {
            if (_activeEvents[event1] == event2)
                return;
            if (_activeEvents[event1] != EventNone)
                DespawnGroup(GroupFor(event1, _activeEvents[event1]));
            _activeEvents[event1] = event2;
            if (battlegroundMap->GetSpawnGroupData(GroupFor(event1, event2)))
                battlegroundMap->SpawnGroupSpawn(GroupFor(event1, event2), true);
        }
        else
        {
            if (_activeEvents[event1] != event2)
                return;
            _activeEvents[event1] = EventNone;
            DespawnGroup(GroupFor(event1, event2));
        }
    }

    void DespawnGroup(uint32 groupId)
    {
        if (battlegroundMap->GetSpawnGroupData(groupId))
            battlegroundMap->SpawnGroupDespawn(groupId, true);
    }

    // VMaNGOS SetSpawnEventMode: RESPAWN_FORCED respawns the dead of an active event, RESPAWN_STOP stops respawning
    // (the creatures alive keep fighting)
    void SetEventRespawn(uint8 event1, uint8 event2, bool respawn)
    {
        using namespace ClassicAlteracValley;
        uint32 const groupId = GroupFor(event1, event2);
        if (event2 == EventNone || _activeEvents[event1] != event2 || !battlegroundMap->GetSpawnGroupData(groupId))
            return;

        if (respawn)
            battlegroundMap->SpawnGroupSpawn(groupId, true);
        else
            battlegroundMap->SetSpawnGroupInactive(groupId);
    }

    uint8 DefenderTier(uint8 teamIdx) const
    {
        using namespace ClassicAlteracValley;
        if (teamIdx == CAV_TEAM_NEUTRAL)
            return 0;
        uint32 const scraps = _teamQuestStatus[teamIdx][0];
        return scraps < 500 ? 0 : scraps < 1000 ? 1 : scraps < 1500 ? 2 : 3;
    }

    void InitNode(uint8 node, uint8 teamIdx, bool tower)
    {
        using namespace ClassicAlteracValley;
        NodeInfo& info = _nodes[node];
        info.TotalOwner = info.Owner = info.PrevOwner = info.PrevOtherOwner = teamIdx;
        info.State = info.PrevState = CAV_STATE_CONTROLLED;
        info.Timer = 0;
        info.Tower = tower;
        _activeEvents[node] = teamIdx * MaxStates + CAV_STATE_CONTROLLED;
        if (!tower)
            _activeEvents[CAV_EVENT_GRAVE_DEFENDERS + node] = teamIdx * GraveTypes;
        else
        {
            _activeEvents[CAV_EVENT_TOWER_MARSHALS + node] = teamIdx * MaxStates + 1;
            _activeEvents[TowersMax + node] = teamIdx * GraveTypes;
        }
    }

    // VMaNGOS PopulateNode: banner / defenders of a node after a state change
    void PopulateNode(uint8 node)
    {
        using namespace ClassicAlteracValley;
        NodeInfo const& info = _nodes[node];
        uint8 const newTeam = info.Owner;
        uint8 const oldTeam = info.PrevOtherOwner;
        uint8 const tierNew = DefenderTier(newTeam);
        uint8 const tierOld = DefenderTier(oldTeam);

        if (!info.Tower)
        {
            if (info.State == CAV_STATE_CONTROLLED)
            {
                SetEventRespawn(CAV_EVENT_GRAVE_DEFENDERS + node, newTeam * GraveTypes + tierNew, true);
                SpawnEvent(CAV_EVENT_GRAVE_DEFENDERS + node, newTeam * GraveTypes + tierNew, true);
            }
            else
                SetEventRespawn(CAV_EVENT_GRAVE_DEFENDERS + node, oldTeam * GraveTypes + tierOld, false);
        }
        else if (newTeam != CAV_TEAM_NEUTRAL)
        {
            if (info.State == CAV_STATE_CONTROLLED)
            {
                SetEventRespawn(CAV_EVENT_TOWER_MARSHALS + node, newTeam * MaxStates + 1, true);
                SpawnEvent(CAV_EVENT_TOWER_MARSHALS + node, newTeam * MaxStates + 1, true);
                SetEventRespawn(TowersMax + node, newTeam * GraveTypes + tierNew, true);
                SpawnEvent(TowersMax + node, newTeam * GraveTypes + tierNew, true);
            }
            else
            {
                SetEventRespawn(CAV_EVENT_TOWER_MARSHALS + node, oldTeam * MaxStates + 1, false);
                SetEventRespawn(TowersMax + node, oldTeam * GraveTypes + tierOld, false);
            }
        }

        SpawnEvent(node, newTeam * MaxStates + info.State, true);
    }

    void HandleBannerClick(Player* player, GameObject* banner)
    {
        using namespace ClassicAlteracValley;
        if (battleground->GetStatus() != STATUS_IN_PROGRESS || !player->CanUseBattlegroundObject(banner))
            return;

        uint8 event1, event2;
        if (!GetSpawnEvent(banner->GetGameObjectData(), event1, event2) || event1 >= NodesMax)
            return;

        Team const team = battleground->GetPlayerTeam(player->GetGUID());
        if (team != ALLIANCE && team != HORDE)
            return;

        player->RemoveAurasByType(SPELL_AURA_MOD_STEALTH);
        player->RemoveAurasByType(SPELL_AURA_MOD_INVISIBILITY);

        uint8 const teamIdx = team == ALLIANCE ? CAV_TEAM_ALLIANCE : CAV_TEAM_HORDE;
        if (event2 % MaxStates == CAV_STATE_CONTROLLED)
            EventPlayerAssaultsPoint(player, teamIdx, event1);
        else
            EventPlayerDefendsPoint(player, teamIdx, event1);
    }

    void EventPlayerAssaultsPoint(Player* player, uint8 teamIdx, uint8 node)
    {
        using namespace ClassicAlteracValley;
        NodeInfo& info = _nodes[node];
        if (info.Owner == teamIdx || info.TotalOwner == teamIdx)
            return;
        // only an assaulted node without a total owner (Snowfall) can be assaulted again
        if (info.State == CAV_STATE_ASSAULTED && info.TotalOwner != CAV_TEAM_NEUTRAL)
            return;

        info.Timer = CaptureTime;
        info.PrevOwner = info.Owner;
        info.PrevOtherOwner = info.Owner;
        info.Owner = teamIdx;
        info.PrevState = info.State;
        info.State = CAV_STATE_ASSAULTED;

        UpdateNodeWorldState(node);
        PopulateNode(node);

        HeraldTalk(HeraldTexts[node][teamIdx == CAV_TEAM_ALLIANCE ? 1 : 3]);
        battleground->UpdatePvpStat(player, info.Tower ? CAV_PVP_STAT_TOWERS_ASSAULTED : CAV_PVP_STAT_GRAVEYARDS_ASSAULTED, 1);
        battleground->PlaySoundToAll(teamIdx == CAV_TEAM_ALLIANCE ? CAV_SOUND_ALLIANCE_ASSAULTS : CAV_SOUND_HORDE_ASSAULTS);
    }

    void EventPlayerDefendsPoint(Player* player, uint8 teamIdx, uint8 node)
    {
        using namespace ClassicAlteracValley;
        NodeInfo& info = _nodes[node];
        if (info.Owner == teamIdx || info.State != CAV_STATE_ASSAULTED)
            return;

        // first Snowfall capture: the other team assaults it instead
        if (info.TotalOwner == CAV_TEAM_NEUTRAL)
        {
            EventPlayerAssaultsPoint(player, teamIdx, node);
            return;
        }

        if (info.PrevOwner != teamIdx || info.TotalOwner != teamIdx)
            return;

        info.PrevOwner = info.Owner;
        info.PrevOtherOwner = info.Owner;
        info.Owner = teamIdx;
        info.PrevState = info.State;
        info.State = CAV_STATE_CONTROLLED;
        info.Timer = 0;

        PopulateNode(node);
        UpdateNodeWorldState(node);

        HeraldTalk(HeraldTexts[node][teamIdx == CAV_TEAM_ALLIANCE ? 0 : 2]);
        if (info.Tower)
        {
            battleground->UpdatePvpStat(player, CAV_PVP_STAT_TOWERS_DEFENDED, 1);
            battleground->PlaySoundToAll(CAV_SOUND_TOWER_DEFEND);
        }
        else
        {
            battleground->UpdatePvpStat(player, CAV_PVP_STAT_GRAVEYARDS_DEFENDED, 1);
            battleground->PlaySoundToAll(teamIdx == CAV_TEAM_ALLIANCE ? CAV_SOUND_ALLIANCE_GOOD : CAV_SOUND_HORDE_GOOD);
        }
    }

    void EventPlayerDestroyedPoint(uint8 node)
    {
        using namespace ClassicAlteracValley;
        NodeInfo& info = _nodes[node];
        uint8 const owner = info.Owner;
        if (owner == CAV_TEAM_NEUTRAL)
            return;

        info.TotalOwner = owner;
        info.PrevOwner = owner;
        info.PrevState = info.State;
        info.State = CAV_STATE_CONTROLLED;
        info.Timer = 0;

        PopulateNode(node);
        UpdateNodeWorldState(node);

        HeraldTalk(HeraldTexts[node][owner == CAV_TEAM_ALLIANCE ? 0 : 2]);
        if (info.Tower)
        {
            uint8 const loser = owner == CAV_TEAM_ALLIANCE ? CAV_TEAM_HORDE : CAV_TEAM_ALLIANCE;
            UpdateScore(loser, -ResTower);
            battleground->RewardReputationToTeam(FactionOf(owner), RepTower[_holiday], TeamOf(owner));
            battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(KillTower), TeamOf(owner));
            CompleteQuestForAll(owner == CAV_TEAM_ALLIANCE ? CAV_QUEST_TOWER_A : CAV_QUEST_TOWER_H);
        }
        else
        {
            battleground->PlaySoundToAll(owner == CAV_TEAM_ALLIANCE ? CAV_SOUND_ALLIANCE_GOOD : CAV_SOUND_HORDE_GOOD);
            CompleteQuestForAll(owner == CAV_TEAM_ALLIANCE ? CAV_QUEST_GRAVE_A : CAV_QUEST_GRAVE_H);
        }
    }

    void ChangeMineOwner(uint8 mine, uint8 teamIdx)
    {
        using namespace ClassicAlteracValley;
        _mineTimer[mine] = MineTickTimer;
        if (_mineOwner[mine] == teamIdx)
            return;

        _mineOwner[mine] = teamIdx;
        SendMineWorldStates(mine);

        SpawnEvent(CAV_EVENT_MINE + mine, teamIdx, true);
        SpawnEvent(CAV_EVENT_MINE_BOSSES + mine, teamIdx, true);

        if (teamIdx == CAV_TEAM_NEUTRAL)
            return;

        _mineReclaimTimer[mine] = MineReclaimTimer;
        battleground->PlaySoundToAll(teamIdx == CAV_TEAM_ALLIANCE ? CAV_SOUND_ALLIANCE_GOOD : CAV_SOUND_HORDE_GOOD);
        HeraldTalk(MineHeraldTexts[mine][teamIdx]);
        CompleteQuestForAll(teamIdx == CAV_TEAM_ALLIANCE ? CAV_QUEST_MINE_A : CAV_QUEST_MINE_H);
    }

    void UpdateNodeWorldState(uint8 node) const
    {
        using namespace ClassicAlteracValley;
        NodeInfo const& info = _nodes[node];
        for (uint8 i = 0; i < 4; ++i)
            UpdateWorldState(NodeWorldStates[node][i], info.Owner != CAV_TEAM_NEUTRAL && i == info.Owner * MaxStates + info.State ? 1 : 0);
        if (node == CAV_NODE_SNOWFALL_GRAVE)
            UpdateWorldState(CAV_WS_SNOWFALL_NEUTRAL, info.Owner == CAV_TEAM_NEUTRAL ? 1 : 0);
        if (info.Tower)
            UpdateWorldState(TowerOwnerWorldStates[node - CAV_NODE_DUNBALDAR_SOUTH], info.Owner == CAV_TEAM_HORDE ? 2 : 1);
    }

    void SendMineWorldStates(uint8 mine) const
    {
        using namespace ClassicAlteracValley;
        for (uint8 team = CAV_TEAM_ALLIANCE; team <= CAV_TEAM_NEUTRAL; ++team)
            UpdateWorldState(MineWorldStates[mine][team], _mineOwner[mine] == team ? 1 : 0);
        UpdateWorldState(MineOwnerWorldStates[mine], _mineOwner[mine] == CAV_TEAM_HORDE ? 2 : _mineOwner[mine] == CAV_TEAM_ALLIANCE ? 1 : 0);
    }

    // reinforcements: VMaNGOS keeps the counter but a team at 0 does not lose
    void UpdateScore(uint8 teamIdx, int32 points)
    {
        using namespace ClassicAlteracValley;
        if (teamIdx > CAV_TEAM_HORDE)
            return;
        _scores[teamIdx] = std::max(0, _scores[teamIdx] + points);
        UpdateWorldState(teamIdx == CAV_TEAM_HORDE ? CAV_WS_SCORE_HORDE : CAV_WS_SCORE_ALLIANCE, _scores[teamIdx]);
    }

    void HeraldTalk(uint8 textId)
    {
        if (Creature* herald = battlegroundMap->GetCreature(_heraldGUID))
            if (herald->AI())
                herald->AI()->Talk(textId);
    }

    void CompleteQuestForAll(uint32 questId)
    {
        battlegroundMap->DoOnPlayers([questId](Player* player)
        {
            if (player->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
                player->CompleteQuest(questId);
        });
    }

    std::array<ClassicAlteracValley::NodeInfo, ClassicAlteracValley::NodesMax> _nodes;
    std::array<uint8, 256> _activeEvents = { };
    std::array<std::array<uint32, 9>, 2> _teamQuestStatus = { };
    std::array<int32, 2> _scores = { };
    std::array<int32, 2> _captainBuffTimer = { };
    std::array<uint8, 2> _mineOwner = { };
    std::array<int32, 2> _mineTimer = { };
    std::array<int32, 2> _mineReclaimTimer = { };
    std::array<ObjectGuid, 2> _captainGUIDs;
    ObjectGuid _heraldGUID;
    ObjectGuid _snivvleGUID;
    GuidUnorderedSet _doors;
    int32 _snivvleTimer = 0;
    bool _snivvleDone = false;
    uint8 _holiday = 0;
};

// Classic 1.60: Alterac Valley banners on map 30 (one entry for every graveyard / tower state); the node and state come
// from the spawn group of the clicked banner
struct go_classic_av_banner : GameObjectAI
{
    explicit go_classic_av_banner(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        if (me->GetMapId() != 30)
            return false;

        if (ZoneScript* zoneScript = me->GetZoneScript())
            zoneScript->DoAction(ACTION_AV_INTERACT_CAPTURABLE_OBJECT, player, me);

        return true;
    }
};

void AddSC_battleground_alterac_valley()
{
    RegisterBattlegroundMapScript(battleground_alterac_valley, 30);

    // Classic 1.60
    RegisterBattlegroundMapScript(battleground_classic_alterac_valley, 30);
    RegisterGameObjectAI(go_classic_av_banner);
}
