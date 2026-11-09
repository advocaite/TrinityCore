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

#ifndef __ALTERAC_VALLEY_H
#define __ALTERAC_VALLEY_H

#include "Define.h"

enum AlteracValleySharedActions
{
    ACTION_BUFF_YELL = -30001,
    ACTION_AV_INTERACT_CAPTURABLE_OBJECT = 1,
    ACTION_AV_CAPTURE_CAPTURABLE_OBJECT = 2,

    ACTION_TURN_IN_SCRAPS = 3,
    ACTION_TURN_IN_COMMANDER_1 = 4,
    ACTION_TURN_IN_COMMANDER_2 = 5,
    ACTION_TURN_IN_COMMANDER_3 = 6,
    ACTION_TURN_IN_BOSS_1,
    ACTION_TURN_IN_BOSS_2,
    ACTION_TURN_IN_NEAR_MINE,
    ACTION_TURN_IN_OTHER_MINE,
    ACTION_TURN_IN_RIDER_HIDE,
    ACTION_TURN_IN_RIDER_TAME,

    // classic: a smith upgrades the ground troops (source the player, target the smith)
    ACTION_CAV_UPGRADE_ARMOR,

    // classic assault NPCs
    ACTION_CAV_START_WORLD_BOSS_RITUAL = 100,     // blood collector: the summoning resources are in
    ACTION_CAV_LEADER_DIED                        // assault troop: its leader died, carry on alone
};

// Classic Alterac Valley assaults (VMaNGOS BattleGroundAV challenges). The battleground script counts the resources the
// assault quests deliver; the NPC scripts read them with GetData and spend them with SetData(DATA_CAV_CHALLENGE + ..., 0).
enum AlteracValleyClassicChallenges : uint8
{
    CAV_CHALLENGE_SOLDIER_AIR       = 0,
    CAV_CHALLENGE_LIEUTENANT_AIR    = 1,
    CAV_CHALLENGE_COMMANDER_AIR     = 2,
    CAV_CHALLENGE_HIDE_CAVALRY      = 3,
    CAV_CHALLENGE_TAMED_CAVALRY     = 4,
    CAV_CHALLENGE_IRONDEEP_GROUND   = 5,
    CAV_CHALLENGE_COLDTOOTH_GROUND  = 6,
    CAV_CHALLENGE_BLOOD_WORLDBOSS   = 7,
    CAV_CHALLENGE_MAX
};

enum AlteracValleyClassicData
{
    DATA_CAV_CHALLENGE              = 100,      // + team * CAV_CHALLENGE_MAX + challenge: resources delivered
    DATA_CAV_CHALLENGE_GOAL         = 120,      // + team * CAV_CHALLENGE_MAX + challenge: resources an assault needs
    DATA_CAV_REINFORCEMENT          = 140,      // + team: ground troop tier, 0 basic to 3 champion
    DATA_CAV_ARMOR_SCRAPS           = 142       // + team: armor scraps delivered
};

// VMaNGOS script_waypoint paths of the assault NPCs: waypoint_path 811000000 + entry * 10
constexpr uint32 AlteracValleyAssaultPathId(uint32 entry) { return 811000000 + entry * 10; }

#endif
