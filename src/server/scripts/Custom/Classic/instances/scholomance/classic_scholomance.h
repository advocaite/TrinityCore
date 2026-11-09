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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/western_plaguelands/scholomance/scholomance.h
// (ScriptDev2 lineage, GPL-2)

#ifndef CLASSIC_SCHOLOMANCE_H
#define CLASSIC_SCHOLOMANCE_H

#include "Define.h"

namespace ClassicScholomance
{
    constexpr uint32 MapId = 289;
    constexpr char const* InstanceScriptName = "classic_instance_scholomance";
    constexpr char const* DataHeader = "CLSCH";

    enum ClassicScholomanceObjects : uint32
    {
        GO_GATE_KIRTONOS        = 175570,
        GO_BRAZIER_KIRTONOS     = 175564,
        GO_GATE_GANDLING        = 177374,
        GO_GATE_MALICIA         = 177375,
        GO_GATE_THEOLEN         = 177377,
        GO_GATE_POLKELT         = 177376,
        GO_GATE_RAVENIAN        = 177372,
        GO_GATE_BAROV           = 177373,
        GO_GATE_ILLUCIA         = 177371,
        GO_VIEWING_ROOM_DOOR    = 175167,

        SOUND_SCREECH           = 557,

        NPC_KIRTONOS            = 10506,
        NPC_GANDLING            = 1853,
        NPC_VECTUS              = 10432,
        NPC_MARDUKE             = 10433,
        NPC_RISEN_GUARDIAN      = 11598,

        NPC_J_EEVEE             = 14500
    };

    enum ClassicScholomanceData : uint32
    {
        TYPE_GANDLING           = 0,
        TYPE_THEOLEN            = 1,
        TYPE_MALICIA            = 2,
        TYPE_ILLUCIABAROV       = 3,
        TYPE_ALEXEIBAROV        = 4,
        TYPE_POLKELT            = 5,
        TYPE_RAVENIAN           = 6,
        TYPE_KIRTONOS           = 7,
        TYPE_VIEWING_ROOM_DOOR  = 14,
        TYPE_DARKREAVER         = 15,
        INSTANCE_SCHOLOMANCE_MAX_ENCOUNTER = 16,

        // GetGuidData (the GO_GATE_* entries above are keys too; SetGuidData(gate entry, guid) registers a Risen Guardian behind that gate)
        DATA_VECTUS             = 16,
        DATA_MARDUKE            = 17
    };
}

#endif
