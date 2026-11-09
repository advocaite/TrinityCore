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

// Classic 1.60 port of VMaNGOS src/scripts/kalimdor/silithus/ruins_of_ahnqiraj/ruins_of_ahnqiraj.h (ScriptDev2 lineage, GPL-2)

#ifndef CLASSIC_RUINS_OF_AHNQIRAJ_H
#define CLASSIC_RUINS_OF_AHNQIRAJ_H

#include "Define.h"

#define ClassicRuinsOfAhnQirajScriptName "classic_instance_ruins_of_ahnqiraj"
#define ClassicRuinsOfAhnQirajDataHeader "CAQ20"

// GetData / SetData ids (VMaNGOS TYPE_*). TYPE_* 0..6 are also the InstanceScript boss ids
// (CLASSIC_AQ20_TYPE_GENERAL_ANDOROV is not a boss and is kept as plain instance data).
enum ClassicAQ20Types : uint32
{
    CLASSIC_AQ20_TYPE_KURINNAXX          = 0,
    CLASSIC_AQ20_TYPE_GENERAL_ANDOROV    = 1,
    CLASSIC_AQ20_TYPE_RAJAXX             = 2,
    CLASSIC_AQ20_TYPE_BURU               = 3,
    CLASSIC_AQ20_TYPE_MOAM               = 4,
    CLASSIC_AQ20_TYPE_AYAMISS            = 5,
    CLASSIC_AQ20_TYPE_OSSIRIAN           = 6,
    CLASSIC_AQ20_MAX_ENCOUNTER           = 7,

    CLASSIC_AQ20_TYPE_QIRAJI_GLADIATOR   = 8,

    CLASSIC_AQ20_MAP_ID                  = 509
};

// GetGuidData ids (VMaNGOS GetData64 DATA_*)
enum ClassicAQ20DataGuids : uint32
{
    CLASSIC_AQ20_DATA_KURINNAXX          = 0,
    CLASSIC_AQ20_DATA_RAJAXX             = 1,
    CLASSIC_AQ20_DATA_BURU               = 2,
    CLASSIC_AQ20_DATA_AYAMISS            = 3,
    CLASSIC_AQ20_DATA_MOAM               = 4,
    CLASSIC_AQ20_DATA_OSSIRIAN           = 5,
    CLASSIC_AQ20_DATA_ANDOROV            = 6,

    CLASSIC_AQ20_DATA_QEEZ               = 7,
    CLASSIC_AQ20_DATA_TUUBID             = 8,
    CLASSIC_AQ20_DATA_DRENN              = 9,
    CLASSIC_AQ20_DATA_XURREM             = 10,
    CLASSIC_AQ20_DATA_YEGGETH            = 11,
    CLASSIC_AQ20_DATA_PAKKON             = 12,
    CLASSIC_AQ20_DATA_ZERRAN             = 13,

    // VMaNGOS calls instance_ruins_of_ahnqiraj::SpawnNewCrystals(guid) directly; TC scripts cannot see the instance class,
    // so SetGuidData(CLASSIC_AQ20_DATA_CRYSTAL, usedCrystal) / SetData(CLASSIC_AQ20_DATA_CRYSTAL_INIT, 0) forward to it.
    CLASSIC_AQ20_DATA_CRYSTAL            = 14,
    CLASSIC_AQ20_DATA_CRYSTAL_INIT       = 15,
    CLASSIC_AQ20_DATA_YEGGETH_SHIELD     = 16,

    // SetData from classic_npc_andorov: a point of his walk was reached (value = ClassicAQ20AndorovPoint)
    CLASSIC_AQ20_DATA_ANDOROV_POINT      = 17
};

// MovePoint ids of Lieutenant General Andorov's walks; classic_npc_andorov forwards every arrival as
// SetData(CLASSIC_AQ20_DATA_ANDOROV_POINT, id) and the instance script orders the next point
enum ClassicAQ20AndorovPoint : uint32
{
    CLASSIC_AQ20_ANDOROV_POINT_CAMP_PATH   = 100,   // + point index: VMaNGOS creature_movement_template 15471 (spawn -> camp)
    CLASSIC_AQ20_ANDOROV_POINT_VALLEY_PATH = 200,   // + point index: VMaNGOS creature_movement_special 15471 (camp -> valley)
    CLASSIC_AQ20_ANDOROV_POINT_LEAVE_PATH  = 300    // + point index: VMaNGOS generic_scripts 1547105 (timed moves, arrivals are ignored)
};

enum ClassicAQ20Entries : uint32
{
    CLASSIC_AQ20_NPC_MOAM                = 15340,
    CLASSIC_AQ20_NPC_AYAMISS             = 15369,
    CLASSIC_AQ20_NPC_OSSIRIAN            = 15339,
    CLASSIC_AQ20_NPC_BURU                = 15370,
    CLASSIC_AQ20_NPC_RAJAXX              = 15341,
    CLASSIC_AQ20_NPC_SWARMGUARD_NEEDLER  = 15344,
    CLASSIC_AQ20_NPC_KURINNAXX           = 15348,
    CLASSIC_AQ20_NPC_COLONEL_ZERRAN      = 15385,
    CLASSIC_AQ20_NPC_MAJOR_YEGGETH       = 15386,
    CLASSIC_AQ20_NPC_QIRAJI_WARRIOR      = 15387,
    CLASSIC_AQ20_NPC_MAJOR_PAKKON        = 15388,
    CLASSIC_AQ20_NPC_CAPTAIN_DRENN       = 15389,
    CLASSIC_AQ20_NPC_CAPTAIN_XURREM      = 15390,
    CLASSIC_AQ20_NPC_CAPTAIN_TUUBID      = 15392,
    CLASSIC_AQ20_NPC_CAPTAIN_QEEZ        = 15391,
    CLASSIC_AQ20_NPC_KALDOREI_ELITE      = 15473,
    CLASSIC_AQ20_NPC_GENERAL_ANDOROV     = 15471,

    // Gossip menu ids
    CLASSIC_AQ20_ANDOROV_GOSSIP_NOT_STARTED = 6629,
    CLASSIC_AQ20_ANDOROV_GOSSIP_IN_PROGRESS = 7048,
    CLASSIC_AQ20_ANDOROV_GOSSIP_DONE        = 7047,

    // VMaNGOS guids (creature spawn id here = 20000000 + VMaNGOS guid)
    CLASSIC_AQ20_ANDOROV_DB_GUID         = 301311,

    // VMaNGOS generic_scripts id
    CLASSIC_AQ20_ANDOROV_START_SCRIPT    = 154710,

    // VMaNGOS creature_spells 154710 (Andorov) and 154730 (Kaldorei Elite)
    CLASSIC_AQ20_SPELL_ANDOROV_STRIKE    = 22591,
    CLASSIC_AQ20_SPELL_ANDOROV_BASH      = 25515,
    CLASSIC_AQ20_SPELL_AURA_OF_COMMAND   = 25516,
    CLASSIC_AQ20_SPELL_ELITE_MORTAL_STRIKE = 16856,
    CLASSIC_AQ20_SPELL_ELITE_CLEAVE      = 26350,

    // Broadcast texts and sounds of the wave event (VMaNGOS creature_movement_scripts 1547102 / 1547103, generic_scripts 1547105,
    // 153921 .. 153851 and 153410; the sounds are the broadcast_text sound_ids set by migration 20221127232342)
    CLASSIC_AQ20_SAY_ANDOROV_VALLEY      = 11012,
    CLASSIC_AQ20_SAY_ANDOROV_READY       = 11032,
    CLASSIC_AQ20_SAY_ANDOROV_LEAVE       = 11560,
    CLASSIC_AQ20_SOUND_RAJAXX_WAVE_1     = 8637,
    CLASSIC_AQ20_SAY_RAJAXX_WAVE_2       = 11440,
    CLASSIC_AQ20_SOUND_RAJAXX_WAVE_2     = 8645,
    CLASSIC_AQ20_SAY_RAJAXX_WAVE_3       = 11439,
    CLASSIC_AQ20_SOUND_RAJAXX_WAVE_3     = 8644,
    CLASSIC_AQ20_SAY_RAJAXX_WAVE_4       = 11438,
    CLASSIC_AQ20_SOUND_RAJAXX_WAVE_4     = 8643,
    CLASSIC_AQ20_SAY_RAJAXX_WAVE_5       = 11437,
    CLASSIC_AQ20_SOUND_RAJAXX_WAVE_5     = 8642,
    CLASSIC_AQ20_SAY_RAJAXX_WAVE_6       = 11436,
    CLASSIC_AQ20_SOUND_RAJAXX_WAVE_6     = 8641,
    CLASSIC_AQ20_SAY_RAJAXX_ENGAGE       = 11435,
    CLASSIC_AQ20_SOUND_RAJAXX_ENGAGE     = 8638,

    // VMaNGOS scripted map events 15391..15385: each wave has 180 s before the event fails
    CLASSIC_AQ20_WAVE_TIME_LIMIT_MS      = 180 * 1000,

    // Crystal Weaknesses
    CLASSIC_AQ20_SPELL_FIRE_WEAKNESS     = 25177,
    CLASSIC_AQ20_SPELL_NATURE_WEAKNESS   = 25180,
    CLASSIC_AQ20_SPELL_FROST_WEAKNESS    = 25178,
    CLASSIC_AQ20_SPELL_ARCANE_WEAKNESS   = 25171,
    CLASSIC_AQ20_SPELL_SHADOW_WEAKNESS   = 25183,

    CLASSIC_AQ20_GO_OSSIRIAN_CRYSTAL     = 180619,
    CLASSIC_AQ20_CRYSTAL_TRIGGER         = 15590
};

// Initial distance between the used crystal and new crystals before expanding the search
#define CLASSIC_AQ20_OSSIRIAN_CRYSTAL_INITIAL_DIST 80.0f
#define CLASSIC_AQ20_OSSIRIAN_CRYSTAL_NUM_ACTIVE 2

// VMaNGOS respawn delays in seconds (non DEBUG_MODE values)
enum ClassicAQ20Respawn : uint32
{
    CLASSIC_AQ20_RESPAWN_3_MINUTES       = 180,
    CLASSIC_AQ20_RESPAWN_5_MINUTES       = 300,
    CLASSIC_AQ20_RESPAWN_15_MINUTES      = 900,
    CLASSIC_AQ20_RESPAWN_FOUR_DAYS       = 345600
};

#endif // CLASSIC_RUINS_OF_AHNQIRAJ_H
