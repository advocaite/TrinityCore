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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/burning_steppes/blackrock_spire/blackrock_spire.h
// (ScriptDev2 lineage, GPL-2)

#ifndef CLASSIC_BLACKROCK_SPIRE_H
#define CLASSIC_BLACKROCK_SPIRE_H

#include "Define.h"

namespace ClassicBlackrockSpire
{
    constexpr uint32 MapId = 229;
    constexpr char const* InstanceScriptName = "classic_instance_blackrock_spire";
    constexpr char const* DataHeader = "CLBRS";

    enum ClassicBrsData : uint32
    {
        MAX_ROOMS                   = 7,
        MAX_STADIUM_WAVES           = 7,
        MAX_STADIUM_MOBS_PER_WAVE   = 5,

        TYPE_ROOM_EVENT             = 0,
        TYPE_EMBERSEER              = 1,
        TYPE_FLAMEWREATH            = 2,                        // Only summon once per instance
        TYPE_STADIUM                = 3,
        TYPE_VALTHALAK              = 4,                        // Only summon once per instance
        TYPE_EVENT_DOOR_UBRS        = 5,                        // UBRS door event
        TYPE_SOLAKAR                = 6,                        // Rookery event
        TYPE_DRAKKISATH             = 7,
        INSTANCE_BRS_MAX_ENCOUNTER  = 8,

        // TC port only: SetData(DATA_SORT_ROOM_EVENT_MOBS, 0) replaces VMaNGOS instance_blackrock_spire::DoSortRoomEventMobs()
        DATA_SORT_ROOM_EVENT_MOBS   = 100
        // VMaNGOS SetData64(TYPE_ROOM_EVENT, guid) -> SetGuidData(TYPE_ROOM_EVENT, guid)
        // VMaNGOS GetData64(<npc/go entry>) -> GetGuidData(<npc/go entry>)
    };

    enum ClassicBrsCreatures : uint32
    {
        NPC_SCARSHIELD_INFILTRATOR  = 10299,
        NPC_BLACKHAND_SUMMONER      = 9818,
        NPC_BLACKHAND_VETERAN       = 9819,
        NPC_BLACKHAND_INCANCERATOR  = 10316,
        NPC_PYROGUARD_EMBERSEER     = 9816,
        NPC_BLACKHAND_ELITE         = 10317,
        NPC_LORD_VICTOR_NEFARIUS    = 10162,
        NPC_REND_BLACKHAND          = 10429,
        NPC_GYTH                    = 10339,
        NPC_SOLAKAR                 = 10264,
        NPC_ROOKERY_GUARDIAN        = 10258,
        NPC_ROOKERY_HATCHER         = 10683,
        NPC_DRAKKISATH              = 10363,
        NPC_THE_BEAST               = 10430,
        NPC_CHROMATIC_WHELP         = 10442,                    // related to Gyth arena event
        NPC_CHROMATIC_DRAGON        = 10447,
        NPC_BLACKHAND_HANDLER       = 10742,

        NPC_FIREBRAND_GRUNT         = 9259,
        NPC_BANNOK_GRIMAXE          = 9596
    };

    enum ClassicBrsGameObjects : uint32
    {
        // Doors
        GO_EMBERSEER_IN             = 175244,
        GO_DOORS                    = 175705,
        GO_EMBERSEER_OUT            = 175153,
        GO_GYTH_ENTRY_DOOR          = 164726,
        GO_GYTH_COMBAT_DOOR         = 175185,                   // control in boss_script, because will auto-close after each wave
        GO_GYTH_EXIT_DOOR           = 175186,
        GO_DRAKKISATH_DOOR1         = 175946,
        GO_DRAKKISATH_DOOR2         = 175947,

        GO_BLACKROCK_ALTAR          = 175706,

        GO_ROOM_7_RUNE              = 175194,
        GO_ROOM_3_RUNE              = 175195,
        GO_ROOM_6_RUNE              = 175196,
        GO_ROOM_1_RUNE              = 175197,
        GO_ROOM_5_RUNE              = 175198,
        GO_ROOM_2_RUNE              = 175199,
        GO_ROOM_4_RUNE              = 175200,

        GO_ROOKERY_EGG              = 175124,
        GO_FATHER_FLAME             = 175245,

        // UBRS door event
        GO_DOOR_URBS                = 164725,
        GO_BRAZIER01                = 175528,
        GO_BRAZIER02                = 175529,
        GO_BRAZIER03                = 175530,
        GO_BRAZIER04                = 175531,
        GO_BRAZIER05                = 175532,
        GO_BRAZIER06                = 175533
    };
}

#endif
