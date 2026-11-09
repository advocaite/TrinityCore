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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/stranglethorn_vale/zulgurub/zulgurub.h (ScriptDev2 lineage, GPL-2)

#ifndef CLASSIC_ZULGURUB_H
#define CLASSIC_ZULGURUB_H

#include "Define.h"

#define ClassicZulGurubScriptName "classic_instance_zulgurub"
#define ClassicZulGurubDataHeader "CZG"

// SetData/GetData/GetGuidData ids keep the VMaNGOS values (DB scripts of the vanilla data use them too).
enum ClassicZulGurubData : uint32
{
    CLASSIC_ZG_TYPE_HAKKAR_POWER        = 0,    // set data triggered by spell 24693
    CLASSIC_ZG_TYPE_ARLOKK              = 1,
    CLASSIC_ZG_TYPE_JEKLIK              = 2,
    CLASSIC_ZG_TYPE_VENOXIS             = 3,
    CLASSIC_ZG_TYPE_MARLI               = 4,
    CLASSIC_ZG_TYPE_OHGAN               = 5,
    CLASSIC_ZG_TYPE_THEKAL              = 6,
    CLASSIC_ZG_TYPE_THEKAL_DEATH_TIME   = 7,    // set to last time someone fake died
    CLASSIC_ZG_TYPE_THEKAL_REZ_TIME     = 8,    // set to last time someone started rezzing
    CLASSIC_ZG_TYPE_HAKKAR              = 9,
    CLASSIC_ZG_TYPE_RANDOM_BOSS         = 10,
    CLASSIC_ZG_TYPE_JINDO               = 11,
    CLASSIC_ZG_TYPE_GAHZRANKA           = 12,

    // GetGuidData (VMaNGOS GetData64)
    CLASSIC_ZG_DATA_JINDO               = 13,
    CLASSIC_ZG_DATA_LORKHAN             = 14,
    CLASSIC_ZG_DATA_THEKAL              = 15,
    CLASSIC_ZG_DATA_ZATH                = 16,
    CLASSIC_ZG_DATA_HAKKAR              = 17,
    CLASSIC_ZG_DATA_GAHZRANKA           = 18,
    CLASSIC_ZG_DATA_THEKAL_NEED_REZ     = 19,

    // TC only: VMaNGOS CheckConditionCriteriaMeet() instance conditions 1 and 2 exposed as GetData (0/1),
    // usable by CONDITION_INSTANCE_INFO rows of the Thekal encounter.
    CLASSIC_ZG_DATA_THEKAL_COND_CAN_REZ   = 20, // 1: "Thekal Encounter - Has Unit That Can Rez"
    CLASSIC_ZG_DATA_THEKAL_COND_NEEDS_REZ = 21, // 2: "Thekal Encounter - Has Unit That Needs Rez"

    CLASSIC_ZG_MAP_ID                   = 309,
    CLASSIC_ZG_ZONE_ID                  = 1977
};

// TC InstanceScript boss ids (internal). The first five are the High Priests counted for Hakkar Power.
enum ClassicZulGurubBosses : uint32
{
    CLASSIC_ZG_BOSS_ARLOKK              = 0,
    CLASSIC_ZG_BOSS_JEKLIK              = 1,
    CLASSIC_ZG_BOSS_VENOXIS             = 2,
    CLASSIC_ZG_BOSS_MARLI               = 3,
    CLASSIC_ZG_BOSS_THEKAL              = 4,
    CLASSIC_ZG_BOSS_HAKKAR              = 5,
    CLASSIC_ZG_BOSS_JINDO               = 6,
    CLASSIC_ZG_BOSS_GAHZRANKA           = 7,
    CLASSIC_ZG_MAX_BOSSES               = 8,

    CLASSIC_ZG_HIGH_PRIEST_COUNT        = 5
};

enum ClassicZulGurubEntries : uint32
{
    CLASSIC_ZG_SPELL_THEKAL_RESURRECTION = 24173,
    CLASSIC_ZG_SPELL_HAKKAR_POWER        = 24692,
    CLASSIC_ZG_SPELL_HAKKAR_POWER_DOWN   = 24693,

    // classic_boss_thekal / the zealots: DoAction() when a partner's Resurrection lands on them
    CLASSIC_ZG_ACTION_ZEALOT_RESURRECT   = 1,

    CLASSIC_ZG_NPC_LORKHAN               = 11347,
    CLASSIC_ZG_NPC_ZATH                  = 11348,
    CLASSIC_ZG_NPC_THEKAL                = 14509,
    CLASSIC_ZG_NPC_JINDO                 = 11380,
    CLASSIC_ZG_NPC_HAKKAR                = 14834,
    CLASSIC_ZG_NPC_VENOXIS               = 14507,
    CLASSIC_ZG_NPC_ARLOKK                = 14515,
    CLASSIC_ZG_NPC_MARLI                 = 14510,
    CLASSIC_ZG_NPC_RAZZASHI_SKITTERER    = 14880,
    CLASSIC_ZG_NPC_RAZZASHI_VENOMBROOD   = 14532,
    CLASSIC_ZG_NPC_HAKARI_SHADOWCASTER   = 11338,
    CLASSIC_ZG_NPC_RAZZASHI_BROODWIDOW   = 11370,
    CLASSIC_ZG_NPC_GAHZRANKA             = 15114,
    CLASSIC_ZG_NPC_JEKLIK                = 14517,
    CLASSIC_ZG_NPC_NIGHTMARE_ILLUSION    = 15163,

    // Edge of Madness bosses
    CLASSIC_ZG_BOSS_ENTRY_GRILEK         = 15082,
    CLASSIC_ZG_BOSS_ENTRY_HAZZARAH       = 15083,
    CLASSIC_ZG_BOSS_ENTRY_RENATAKI       = 15084,
    CLASSIC_ZG_BOSS_ENTRY_WUSHOOLAY      = 15085,

    // Gahz'ranka spawn: 20000000 + VMaNGOS creature guid 302411 (used when the creature object is not loaded)
    CLASSIC_ZG_GAHZRANKA_SPAWN_ID        = 20302411
};

#endif // CLASSIC_ZULGURUB_H
