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
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "BattlegroundPackets.h"
#include "Creature.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "GameTime.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"

struct battleground_arathi_basin : BattlegroundScript
{
    // Tick intervals and given points: case 0, 1, 2, 3, 4, 5 captured nodes
    static constexpr uint32 BG_AB_TickInterval = 2000;
    static constexpr uint32 BG_AB_TickPoints[6] = { 0, 2, 3, 4, 7, 60 };
    static constexpr uint32 NORMAL_HONOR_TICKS = 160;
    static constexpr uint32 WEEKEND_HONOR_TICKS = 260;
    static constexpr uint32 NORMAL_REPUTATION_TICKS = 120;
    static constexpr uint32 WEEKEND_REPUTATION_TICKS = 160;

    enum PvpStats
    {
        PVP_STAT_BASES_ASSAULTED                    = 926,
        PVP_STAT_BASES_DEFENDED                     = 927,
    };

    enum Events
    {
        AB_EVENT_START_BATTLE                       = 9158, // Achievement: Let's Get This Done

        AB_EVENT_CONTESTED_STABLES_HORDE            = 28523,
        AB_EVENT_CAPTURE_STABLES_HORDE              = 28527,
        AB_EVENT_DEFENDED_STABLES_HORDE             = 28525,
        AB_EVENT_CONTESTED_STABLES_ALLIANCE         = 28522,
        AB_EVENT_CAPTURE_STABLES_ALLIANCE           = 28526,
        AB_EVENT_DEFENDED_STABLES_ALLIANCE          = 28524,

        AB_EVENT_CONTESTED_BLACKSMITH_HORDE         = 8876,
        AB_EVENT_CAPTURE_BLACKSMITH_HORDE           = 8773,
        AB_EVENT_DEFENDED_BLACKSMITH_HORDE          = 8770,
        AB_EVENT_CONTESTED_BLACKSMITH_ALLIANCE      = 8874,
        AB_EVENT_CAPTURE_BLACKSMITH_ALLIANCE        = 8769,
        AB_EVENT_DEFENDED_BLACKSMITH_ALLIANCE       = 8774,

        AB_EVENT_CONTESTED_FARM_HORDE               = 39398,
        AB_EVENT_CAPTURE_FARM_HORDE                 = 39399,
        AB_EVENT_DEFENDED_FARM_HORDE                = 39400,
        AB_EVENT_CONTESTED_FARM_ALLIANCE            = 39401,
        AB_EVENT_CAPTURE_FARM_ALLIANCE              = 39402,
        AB_EVENT_DEFENDED_FARM_ALLIANCE             = 39403,

        AB_EVENT_CONTESTED_GOLD_MINE_HORDE          = 39404,
        AB_EVENT_CAPTURE_GOLD_MINE_HORDE            = 39405,
        AB_EVENT_DEFENDED_GOLD_MINE_HORDE           = 39406,
        AB_EVENT_CONTESTED_GOLD_MINE_ALLIANCE       = 39407,
        AB_EVENT_CAPTURE_GOLD_MINE_ALLIANCE         = 39408,
        AB_EVENT_DEFENDED_GOLD_MINE_ALLIANCE        = 39409,

        AB_EVENT_CONTESTED_LUMBER_MILL_HORDE        = 39387,
        AB_EVENT_CAPTURE_LUMBER_MILL_HORDE          = 39388,
        AB_EVENT_DEFENDED_LUMBER_MILL_HORDE         = 39389,
        AB_EVENT_CONTESTED_LUMBER_MILL_ALLIANCE     = 39390,
        AB_EVENT_CAPTURE_LUMBER_MILL_ALLIANCE       = 39391,
        AB_EVENT_DEFENDED_LUMBER_MILL_ALLIANCE      = 39392
    };

    enum Sounds
    {
        BG_AB_SOUND_NODE_CLAIMED                    = 8192,
        BG_AB_SOUND_NODE_CAPTURED_ALLIANCE          = 8173,
        BG_AB_SOUND_NODE_CAPTURED_HORDE             = 8213,
        BG_AB_SOUND_NODE_ASSAULTED_ALLIANCE         = 8212,
        BG_AB_SOUND_NODE_ASSAULTED_HORDE            = 8174,
        BG_AB_SOUND_NEAR_VICTORY_ALLIANCE           = 8457,
        BG_AB_SOUND_NEAR_VICTORY_HORDE              = 8456
    };

    enum BroadcastTexts
    {
        BG_AB_TEXT_ALLIANCE_NEAR_VICTORY            = 10598,
        BG_AB_TEXT_HORDE_NEAR_VICTORY               = 10599,
    };

    enum Score
    {
        BG_AB_WARNING_NEAR_VICTORY_SCORE            = 1200,
        BG_AB_MAX_TEAM_SCORE                        = 1500
    };

    enum Creatures
    {
        BG_AB_NPC_THE_BLACK_BRIDE                   = 150501,
        BG_AB_NPC_RADULF_LEDER                      = 150505
    };

    enum ObjectTypes
    {
        BG_AB_OBJECTID_CAPTURE_POINT_STABLES        = 227420,
        BG_AB_OBJECTID_CAPTURE_POINT_BLACKSMITH     = 227522,
        BG_AB_OBJECTID_CAPTURE_POINT_FARM           = 227536,
        BG_AB_OBJECTID_CAPTURE_POINT_GOLD_MINE      = 227538,
        BG_AB_OBJECTID_CAPTURE_POINT_LUMBER_MILL    = 227544,

        BG_AB_OBJECTID_GHOST_GATE                   = 180322,
        BG_AB_OBJECTID_ALLIANCE_DOOR                = 322273,
        BG_AB_OBJECTID_HORDE_DOOR                   = 322274
    };

    enum WorldStates
    {
        BG_AB_WS_OCCUPIED_BASES_HORDE               = 1778,
        BG_AB_WS_OCCUPIED_BASES_ALLY                = 1779,
        BG_AB_WS_RESOURCES_ALLY                     = 1776,
        BG_AB_WS_RESOURCES_HORDE                    = 1777,
        BG_AB_WS_RESOURCES_MAX                      = 1780,
        BG_AB_WS_RESOURCES_WARNING                  = 1955,

        BG_AB_WS_STABLE_ICON                        = 1842,             // Stable map icon (NONE)
        BG_AB_WS_STABLE_STATE_ALIENCE               = 1767,             // Stable map state (ALIENCE)
        BG_AB_WS_STABLE_STATE_HORDE                 = 1768,             // Stable map state (HORDE)
        BG_AB_WS_STABLE_STATE_CON_ALI               = 1769,             // Stable map state (CON ALIENCE)
        BG_AB_WS_STABLE_STATE_CON_HOR               = 1770,             // Stable map state (CON HORDE)
        BG_AB_WS_FARM_ICON                          = 1845,             // Farm map icon (NONE)
        BG_AB_WS_FARM_STATE_ALIENCE                 = 1772,             // Farm state (ALIENCE)
        BG_AB_WS_FARM_STATE_HORDE                   = 1773,             // Farm state (HORDE)
        BG_AB_WS_FARM_STATE_CON_ALI                 = 1774,             // Farm state (CON ALIENCE)
        BG_AB_WS_FARM_STATE_CON_HOR                 = 1775,             // Farm state (CON HORDE)
        BG_AB_WS_BLACKSMITH_ICON                    = 1846,             // Blacksmith map icon (NONE)
        BG_AB_WS_BLACKSMITH_STATE_ALIENCE           = 1782,             // Blacksmith map state (ALIENCE)
        BG_AB_WS_BLACKSMITH_STATE_HORDE             = 1783,             // Blacksmith map state (HORDE)
        BG_AB_WS_BLACKSMITH_STATE_CON_ALI           = 1784,             // Blacksmith map state (CON ALIENCE)
        BG_AB_WS_BLACKSMITH_STATE_CON_HOR           = 1785,             // Blacksmith map state (CON HORDE)
        BG_AB_WS_LUMBERMILL_ICON                    = 1844,             // Lumber Mill map icon (NONE)
        BG_AB_WS_LUMBERMILL_STATE_ALIENCE           = 1792,             // Lumber Mill map state (ALIENCE)
        BG_AB_WS_LUMBERMILL_STATE_HORDE             = 1793,             // Lumber Mill map state (HORDE)
        BG_AB_WS_LUMBERMILL_STATE_CON_ALI           = 1794,             // Lumber Mill map state (CON ALIENCE)
        BG_AB_WS_LUMBERMILL_STATE_CON_HOR           = 1795,             // Lumber Mill map state (CON HORDE)
        BG_AB_WS_GOLDMINE_ICON                      = 1843,             // Gold Mine map icon (NONE)
        BG_AB_WS_GOLDMINE_STATE_ALIENCE             = 1787,             // Gold Mine map state (ALIENCE)
        BG_AB_WS_GOLDMINE_STATE_HORDE               = 1788,             // Gold Mine map state (HORDE)
        BG_AB_WS_GOLDMINE_STATE_CON_ALI             = 1789,             // Gold Mine map state (CON ALIENCE
        BG_AB_WS_GOLDMINE_STATE_CON_HOR             = 1790,             // Gold Mine map state (CON HORDE)

        BG_AB_WS_HAD_500_DISADVANTAGE_ALLIANCE      = 3644,
        BG_AB_WS_HAD_500_DISADVANTAGE_HORDE         = 3645,

        BG_AB_WS_FARM_ICON_NEW                      = 8808,             // Farm map icon
        BG_AB_WS_LUMBER_MILL_ICON_NEW               = 8805,             // Lumber Mill map icon
        BG_AB_WS_BLACKSMITH_ICON_NEW                = 8799,             // Blacksmith map icon
        BG_AB_WS_GOLD_MINE_ICON_NEW                 = 8809,             // Gold Mine map icon
        BG_AB_WS_STABLES_ICON_NEW                   = 5834,             // Stable map icon

        BG_AB_WS_FARM_HORDE_CONTROL_STATE           = 17328,
        BG_AB_WS_FARM_ALLIANCE_CONTROL_STATE        = 17325,
        BG_AB_WS_LUMBER_MILL_HORDE_CONTROL_STATE    = 17330,
        BG_AB_WS_LUMBER_MILL_ALLIANCE_CONTROL_STATE = 17326,
        BG_AB_WS_BLACKSMITH_HORDE_CONTROL_STATE     = 17327,
        BG_AB_WS_BLACKSMITH_ALLIANCE_CONTROL_STATE  = 17324,
        BG_AB_WS_GOLD_MINE_HORDE_CONTROL_STATE      = 17329,
        BG_AB_WS_GOLD_MINE_ALLIANCE_CONTROL_STATE   = 17323,
        BG_AB_WS_STABLES_HORDE_CONTROL_STATE        = 17331,
        BG_AB_WS_STABLES_ALLIANCE_CONTROL_STATE     = 17322,
    };

    explicit battleground_arathi_basin(BattlegroundMap* map) : BattlegroundScript(map), _lastTick(0), _isInformedNearVictory(false)
    {
        bool const isBGWeekend = sBattlegroundMgr->IsBGWeekend(battleground->GetTypeID());

        _honorTics = (isBGWeekend) ? WEEKEND_HONOR_TICKS : NORMAL_HONOR_TICKS;
        _reputationTics = (isBGWeekend) ? WEEKEND_REPUTATION_TICKS : NORMAL_REPUTATION_TICKS;
        _honorScoreTics = { { 0, 0 } };
        _reputationScoreTics = { { 0, 0 } };
    }

    void OnInit() override
    {
        BattlegroundScript::OnInit();

        UpdateWorldState(BG_AB_WS_RESOURCES_MAX, BG_AB_MAX_TEAM_SCORE);
        UpdateWorldState(BG_AB_WS_RESOURCES_WARNING, BG_AB_WARNING_NEAR_VICTORY_SCORE);
    }

    void OnUpdate(uint32 diff) override
    {
        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return;

        // Accumulate points
        _lastTick += diff;
        if (_lastTick > BG_AB_TickInterval)
        {
            _lastTick -= BG_AB_TickInterval;

            uint8 ally = 0, horde = 0;
            _CalculateTeamNodes(ally, horde);
            uint8 points[PVP_TEAMS_COUNT] = { ally, horde };

            for (uint8 team = 0; team < PVP_TEAMS_COUNT; ++team)
            {
                if (!points[team])
                    continue;

                battleground->AddPoint(team == TEAM_HORDE ? HORDE : ALLIANCE, BG_AB_TickPoints[points[team]]);
                _honorScoreTics[team] += BG_AB_TickPoints[points[team]];
                _reputationScoreTics[team] += BG_AB_TickPoints[points[team]];

                if (_reputationScoreTics[team] >= _reputationTics)
                {
                    (team == TEAM_ALLIANCE) ? battleground->RewardReputationToTeam(509, 10, ALLIANCE) : battleground->RewardReputationToTeam(510, 10, HORDE);
                    _reputationScoreTics[team] -= _reputationTics;
                }

                if (_honorScoreTics[team] >= _honorTics)
                {
                    battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(1), (team == TEAM_ALLIANCE) ? ALLIANCE : HORDE);
                    _honorScoreTics[team] -= _honorTics;
                }

                const uint32 teamScore = battleground->GetTeamScore(static_cast<TeamId>(team));
                if (!_isInformedNearVictory && teamScore > BG_AB_WARNING_NEAR_VICTORY_SCORE)
                {
                    if (team == TEAM_ALLIANCE)
                    {
                        battleground->SendBroadcastText(BG_AB_TEXT_ALLIANCE_NEAR_VICTORY, CHAT_MSG_BG_SYSTEM_NEUTRAL);
                        battleground->PlaySoundToAll(BG_AB_SOUND_NEAR_VICTORY_ALLIANCE);
                    }
                    else
                    {
                        battleground->SendBroadcastText(BG_AB_TEXT_HORDE_NEAR_VICTORY, CHAT_MSG_BG_SYSTEM_NEUTRAL);
                        battleground->PlaySoundToAll(BG_AB_SOUND_NEAR_VICTORY_HORDE);
                    }
                    _isInformedNearVictory = true;
                }

                if (teamScore > BG_AB_MAX_TEAM_SCORE)
                    battleground->SetTeamPoint(team == TEAM_HORDE ? HORDE : ALLIANCE, BG_AB_MAX_TEAM_SCORE);

                if (team == TEAM_ALLIANCE)
                    UpdateWorldState(BG_AB_WS_RESOURCES_ALLY, teamScore);
                else
                    UpdateWorldState(BG_AB_WS_RESOURCES_HORDE, teamScore);

                // update achievement flags
                // we increased m_TeamScores[team] so we just need to check if it is 500 more than other teams resources
                uint8 otherTeam = (team + 1) % PVP_TEAMS_COUNT;
                if (teamScore > battleground->GetTeamScore(static_cast<TeamId>(otherTeam)) + 500)
                {
                    if (team == TEAM_ALLIANCE)
                        UpdateWorldState(BG_AB_WS_HAD_500_DISADVANTAGE_HORDE, 1);
                    else
                        UpdateWorldState(BG_AB_WS_HAD_500_DISADVANTAGE_ALLIANCE, 1);
                }
            }

            UpdateWorldState(BG_AB_WS_OCCUPIED_BASES_ALLY, ally);
            UpdateWorldState(BG_AB_WS_OCCUPIED_BASES_HORDE, horde);
        }

        // Test win condition
        if (battleground->GetTeamScore(TEAM_ALLIANCE) >= BG_AB_MAX_TEAM_SCORE && battleground->GetTeamScore(TEAM_HORDE) >= BG_AB_MAX_TEAM_SCORE)
            battleground->EndBattleground(TEAM_OTHER); // draw
        else if (battleground->GetTeamScore(TEAM_ALLIANCE) >= BG_AB_MAX_TEAM_SCORE)
            battleground->EndBattleground(ALLIANCE);
        else if (battleground->GetTeamScore(TEAM_HORDE) >= BG_AB_MAX_TEAM_SCORE)
            battleground->EndBattleground(HORDE);
    }

    void OnStart() override
    {
        // Achievement: Let's Get This Done
        TriggerGameEvent(AB_EVENT_START_BATTLE);
    }

    void _CalculateTeamNodes(uint8& alliance, uint8& horde) const
    {
        alliance = 0;
        horde = 0;

        for (ObjectGuid const& guid : _capturePoints)
        {
            if (GameObject const* capturePoint = battlegroundMap->GetGameObject(guid))
            {
                int32 const wsValue = battlegroundMap->GetWorldStateValue(capturePoint->GetGOInfo()->capturePoint.worldState1);
                switch (WorldPackets::Battleground::BattlegroundCapturePointState(wsValue))
                {
                    case WorldPackets::Battleground::BattlegroundCapturePointState::AllianceCaptured:
                        ++alliance;
                        break;
                    case WorldPackets::Battleground::BattlegroundCapturePointState::HordeCaptured:
                        ++horde;
                        break;
                    default:
                        break;
                }
            }
        }
    }

    Team GetPrematureWinner() override
    {
        // How many bases each team owns
        uint8 ally = 0, horde = 0;
        _CalculateTeamNodes(ally, horde);

        if (ally > horde)
            return ALLIANCE;
        if (horde > ally)
            return HORDE;

        // If the values are equal, fall back to the original result (based on number of players on each team)
        return BattlegroundScript::GetPrematureWinner();
    }

    void ProcessEvent(WorldObject* /*source*/, uint32 eventId, WorldObject* invoker) override
    {
        switch (eventId)
        {
            case AB_EVENT_START_BATTLE:
            {
                for (ObjectGuid const& guid : _creaturesToRemoveOnMatchStart)
                    if (Creature* creature = battlegroundMap->GetCreature(guid))
                        creature->DespawnOrUnsummon();

                for (ObjectGuid const& guid : _gameobjectsToRemoveOnMatchStart)
                    if (GameObject* gameObject = battlegroundMap->GetGameObject(guid))
                        gameObject->DespawnOrUnsummon();

                for (ObjectGuid const& guid : _doors)
                {
                    if (GameObject* gameObject = battlegroundMap->GetGameObject(guid))
                    {
                        gameObject->UseDoorOrButton();
                        gameObject->DespawnOrUnsummon(3s);
                    }
                }
                break;
            }
            case AB_EVENT_CONTESTED_BLACKSMITH_ALLIANCE:
                UpdateWorldState(BG_AB_WS_BLACKSMITH_ALLIANCE_CONTROL_STATE, 1);
                UpdateWorldState(BG_AB_WS_BLACKSMITH_HORDE_CONTROL_STATE, 0);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_ASSAULTED_ALLIANCE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_ASSAULTED, 1);
                break;
            case AB_EVENT_DEFENDED_BLACKSMITH_ALLIANCE:
                UpdateWorldState(BG_AB_WS_BLACKSMITH_ALLIANCE_CONTROL_STATE, 2);
                UpdateWorldState(BG_AB_WS_BLACKSMITH_HORDE_CONTROL_STATE, 0);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_ALLIANCE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_DEFENDED, 1);
                break;
            case AB_EVENT_CAPTURE_BLACKSMITH_ALLIANCE:
                UpdateWorldState(BG_AB_WS_BLACKSMITH_ALLIANCE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_ALLIANCE);
                break;
            case AB_EVENT_CONTESTED_BLACKSMITH_HORDE:
                UpdateWorldState(BG_AB_WS_BLACKSMITH_ALLIANCE_CONTROL_STATE, 0);
                UpdateWorldState(BG_AB_WS_BLACKSMITH_HORDE_CONTROL_STATE, 1);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_ASSAULTED_HORDE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_ASSAULTED, 1);
                break;
            case AB_EVENT_DEFENDED_BLACKSMITH_HORDE:
                UpdateWorldState(BG_AB_WS_BLACKSMITH_ALLIANCE_CONTROL_STATE, 0);
                UpdateWorldState(BG_AB_WS_BLACKSMITH_HORDE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_HORDE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_DEFENDED, 1);
                break;
            case AB_EVENT_CAPTURE_BLACKSMITH_HORDE:
                UpdateWorldState(BG_AB_WS_BLACKSMITH_HORDE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_HORDE);
                break;
            case AB_EVENT_CONTESTED_FARM_ALLIANCE:
                UpdateWorldState(BG_AB_WS_FARM_ALLIANCE_CONTROL_STATE, 1);
                UpdateWorldState(BG_AB_WS_FARM_HORDE_CONTROL_STATE, 0);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_ASSAULTED_ALLIANCE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_ASSAULTED, 1);
                break;
            case AB_EVENT_DEFENDED_FARM_ALLIANCE:
                UpdateWorldState(BG_AB_WS_FARM_ALLIANCE_CONTROL_STATE, 2);
                UpdateWorldState(BG_AB_WS_FARM_HORDE_CONTROL_STATE, 0);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_ALLIANCE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_DEFENDED, 1);
                break;
            case AB_EVENT_CAPTURE_FARM_ALLIANCE:
                UpdateWorldState(BG_AB_WS_FARM_ALLIANCE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_ALLIANCE);
                break;
            case AB_EVENT_CONTESTED_FARM_HORDE:
                UpdateWorldState(BG_AB_WS_FARM_ALLIANCE_CONTROL_STATE, 0);
                UpdateWorldState(BG_AB_WS_FARM_HORDE_CONTROL_STATE, 1);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_ASSAULTED_HORDE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_ASSAULTED, 1);
                break;
            case AB_EVENT_DEFENDED_FARM_HORDE:
                UpdateWorldState(BG_AB_WS_FARM_ALLIANCE_CONTROL_STATE, 0);
                UpdateWorldState(BG_AB_WS_FARM_HORDE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_HORDE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_DEFENDED, 1);
                break;
            case AB_EVENT_CAPTURE_FARM_HORDE:
                UpdateWorldState(BG_AB_WS_FARM_HORDE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_HORDE);
                break;
            case AB_EVENT_CONTESTED_GOLD_MINE_ALLIANCE:
                UpdateWorldState(BG_AB_WS_GOLD_MINE_ALLIANCE_CONTROL_STATE, 1);
                UpdateWorldState(BG_AB_WS_GOLD_MINE_HORDE_CONTROL_STATE, 0);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_ASSAULTED_ALLIANCE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_ASSAULTED, 1);
                break;
            case AB_EVENT_DEFENDED_GOLD_MINE_ALLIANCE:
                UpdateWorldState(BG_AB_WS_GOLD_MINE_ALLIANCE_CONTROL_STATE, 2);
                UpdateWorldState(BG_AB_WS_GOLD_MINE_HORDE_CONTROL_STATE, 0);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_ALLIANCE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_DEFENDED, 1);
                break;
            case AB_EVENT_CAPTURE_GOLD_MINE_ALLIANCE:
                UpdateWorldState(BG_AB_WS_GOLD_MINE_ALLIANCE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_ALLIANCE);
                break;
            case AB_EVENT_CONTESTED_GOLD_MINE_HORDE:
                UpdateWorldState(BG_AB_WS_GOLD_MINE_ALLIANCE_CONTROL_STATE, 0);
                UpdateWorldState(BG_AB_WS_GOLD_MINE_HORDE_CONTROL_STATE, 1);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_ASSAULTED_HORDE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_ASSAULTED, 1);
                break;
            case AB_EVENT_DEFENDED_GOLD_MINE_HORDE:
                UpdateWorldState(BG_AB_WS_GOLD_MINE_ALLIANCE_CONTROL_STATE, 0);
                UpdateWorldState(BG_AB_WS_GOLD_MINE_HORDE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_HORDE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_DEFENDED, 1);
                break;
            case AB_EVENT_CAPTURE_GOLD_MINE_HORDE:
                UpdateWorldState(BG_AB_WS_GOLD_MINE_HORDE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_HORDE);
                break;
            case AB_EVENT_CONTESTED_LUMBER_MILL_ALLIANCE:
                UpdateWorldState(BG_AB_WS_LUMBER_MILL_ALLIANCE_CONTROL_STATE, 1);
                UpdateWorldState(BG_AB_WS_LUMBER_MILL_HORDE_CONTROL_STATE, 0);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_ASSAULTED_ALLIANCE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_ASSAULTED, 1);
                break;
            case AB_EVENT_DEFENDED_LUMBER_MILL_ALLIANCE:
                UpdateWorldState(BG_AB_WS_LUMBER_MILL_ALLIANCE_CONTROL_STATE, 2);
                UpdateWorldState(BG_AB_WS_LUMBER_MILL_HORDE_CONTROL_STATE, 0);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_ALLIANCE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_DEFENDED, 1);
                break;
            case AB_EVENT_CAPTURE_LUMBER_MILL_ALLIANCE:
                UpdateWorldState(BG_AB_WS_LUMBER_MILL_ALLIANCE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_ALLIANCE);
                break;
            case AB_EVENT_CONTESTED_LUMBER_MILL_HORDE:
                UpdateWorldState(BG_AB_WS_LUMBER_MILL_ALLIANCE_CONTROL_STATE, 0);
                UpdateWorldState(BG_AB_WS_LUMBER_MILL_HORDE_CONTROL_STATE, 1);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_ASSAULTED_HORDE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_ASSAULTED, 1);
                break;
            case AB_EVENT_DEFENDED_LUMBER_MILL_HORDE:
                UpdateWorldState(BG_AB_WS_LUMBER_MILL_ALLIANCE_CONTROL_STATE, 0);
                UpdateWorldState(BG_AB_WS_LUMBER_MILL_HORDE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_HORDE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_DEFENDED, 1);
                break;
            case AB_EVENT_CAPTURE_LUMBER_MILL_HORDE:
                UpdateWorldState(BG_AB_WS_LUMBER_MILL_HORDE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_HORDE);
                break;
            case AB_EVENT_CONTESTED_STABLES_ALLIANCE:
                UpdateWorldState(BG_AB_WS_STABLES_ALLIANCE_CONTROL_STATE, 1);
                UpdateWorldState(BG_AB_WS_STABLES_HORDE_CONTROL_STATE, 0);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_ASSAULTED_ALLIANCE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_ASSAULTED, 1);
                break;
            case AB_EVENT_DEFENDED_STABLES_ALLIANCE:
                UpdateWorldState(BG_AB_WS_STABLES_ALLIANCE_CONTROL_STATE, 2);
                UpdateWorldState(BG_AB_WS_STABLES_HORDE_CONTROL_STATE, 0);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_ALLIANCE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_DEFENDED, 1);
                break;
            case AB_EVENT_CAPTURE_STABLES_ALLIANCE:
                UpdateWorldState(BG_AB_WS_STABLES_ALLIANCE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_ALLIANCE);
                break;
            case AB_EVENT_CONTESTED_STABLES_HORDE:
                UpdateWorldState(BG_AB_WS_STABLES_ALLIANCE_CONTROL_STATE, 0);
                UpdateWorldState(BG_AB_WS_STABLES_HORDE_CONTROL_STATE, 1);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_ASSAULTED_HORDE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_ASSAULTED, 1);
                break;
            case AB_EVENT_DEFENDED_STABLES_HORDE:
                UpdateWorldState(BG_AB_WS_STABLES_ALLIANCE_CONTROL_STATE, 0);
                UpdateWorldState(BG_AB_WS_STABLES_HORDE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_HORDE);
                if (Player* player = invoker->ToPlayer())
                    battleground->UpdatePvpStat(player, PVP_STAT_BASES_DEFENDED, 1);
                break;
            case AB_EVENT_CAPTURE_STABLES_HORDE:
                UpdateWorldState(BG_AB_WS_STABLES_HORDE_CONTROL_STATE, 2);
                battleground->PlaySoundToAll(BG_AB_SOUND_NODE_CAPTURED_HORDE);
                break;
            default:
                TC_LOG_WARN("bg.events", "BattlegroundAB::ProcessEvent: Unhandled event %u.", eventId);
                break;
        }
    }

    void OnCreatureCreate(Creature* creature) override
    {
        switch (creature->GetEntry())
        {
            case BG_AB_NPC_THE_BLACK_BRIDE:
            case BG_AB_NPC_RADULF_LEDER:
                _creaturesToRemoveOnMatchStart.push_back(creature->GetGUID());
                break;
            default:
                break;
        }
    }

    void OnGameObjectCreate(GameObject* gameObject) override
    {
        if (gameObject->GetGOInfo()->type == GAMEOBJECT_TYPE_CAPTURE_POINT)
            _capturePoints.push_back(gameObject->GetGUID());

        switch (gameObject->GetEntry())
        {
            case BG_AB_OBJECTID_GHOST_GATE:
                _gameobjectsToRemoveOnMatchStart.push_back(gameObject->GetGUID());
                break;
            case BG_AB_OBJECTID_ALLIANCE_DOOR:
            case BG_AB_OBJECTID_HORDE_DOOR:
                _doors.push_back(gameObject->GetGUID());
                break;
            default:
                break;
        }
    }

    void OnEnd(Team winner) override
    {
        BattlegroundScript::OnEnd(winner);

        // Win reward
        if (winner == ALLIANCE)
            battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(1), ALLIANCE);
        if (winner == HORDE)
            battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(1), HORDE);
        // Complete map_end rewards (even if no team wins)
        battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(1), HORDE);
        battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(1), ALLIANCE);
    }

private:
    uint32 _lastTick;
    std::array<uint32, PVP_TEAMS_COUNT> _honorScoreTics;
    std::array<uint32, PVP_TEAMS_COUNT> _reputationScoreTics;
    bool _isInformedNearVictory;
    uint32 _honorTics;
    uint32 _reputationTics;

    GuidVector _gameobjectsToRemoveOnMatchStart;
    GuidVector _creaturesToRemoveOnMatchStart;
    GuidVector _doors;
    GuidVector _capturePoints;
};

// Classic 1.60: vanilla Arathi Basin on the original map 529 (port of VMaNGOS BattleGroundAB). The retail script above
// needs the 2107 remake capture points (gameobject type 42); map 529 has the vanilla banners (type 10 neutral banners,
// type 1 faction / contested banners, lock 1479). Every banner state of every node is a spawn group
// (world SQL 2026_10_11_41), the script switches them like VMaNGOS battleground_events.
namespace ClassicArathiBasin
{
    static constexpr uint32 MapId = 529;
    static constexpr uint8 NodesCount = 5;
    static constexpr uint32 MaxTeamScore = 2000;
    static constexpr uint32 WarningNearVictoryScore = 1800;
    static constexpr Milliseconds CapturingTime = 60s;
    // tick interval and resources for 0..5 occupied nodes
    static constexpr std::array<uint32, NodesCount + 1> TickIntervals = { 0, 12000, 9000, 6000, 3000, 1000 };
    static constexpr std::array<uint32, NodesCount + 1> TickPoints = { 0, 10, 10, 10, 10, 30 };
    // resources per honor / reputation reward (patch 1.10+)
    static constexpr uint32 HonorTicksNormal = 330;
    static constexpr uint32 HonorTicksWeekend = 200;
    static constexpr uint32 ReputationTicksNormal = 200;
    static constexpr uint32 ReputationTicksWeekend = 150;

    enum Actions : uint32
    {
        ACTION_BANNER_CLICKED       = 1                     // source = player, target = banner gameobject
    };

    enum NodeStatus : uint8
    {
        NODE_NEUTRAL                = 0,
        NODE_ALLIANCE_CONTESTED     = 1,
        NODE_HORDE_CONTESTED        = 2,
        NODE_ALLIANCE_OCCUPIED      = 3,
        NODE_HORDE_OCCUPIED         = 4
    };

    enum Nodes : uint8
    {
        NODE_STABLES                = 0,
        NODE_BLACKSMITH             = 1,
        NODE_FARM                   = 2,
        NODE_LUMBER_MILL            = 3,
        NODE_GOLD_MINE              = 4
    };

    enum GameObjects : uint32
    {
        GO_ALLIANCE_DOOR            = 180255,
        GO_HORDE_DOOR               = 180256
    };

    enum SpawnGroups : uint32
    {
        SPAWN_GROUP_NODE_BASE       = 529000,               // + node * 10 + NodeStatus (banner + banner aura; occupied: spirit guide and node NPCs, 2026_10_12_40)
        SPAWN_GROUP_CONTESTED_AURA  = 5,                    // + node * 10: neutral banner aura while contested
        SPAWN_GROUP_FARM_ALLIANCE   = 529050,               // farm decoration while the Alliance owns the farm
        SPAWN_GROUP_FARM_HORDE      = 529051,
        SPAWN_GROUP_GHOST_GATES     = 529060
    };

    enum WorldStates : int32
    {
        WS_OCCUPIED_BASES_HORDE     = 1778,
        WS_OCCUPIED_BASES_ALLIANCE  = 1779,
        WS_RESOURCES_ALLIANCE       = 1776,
        WS_RESOURCES_HORDE          = 1777,
        WS_RESOURCES_MAX            = 1780,
        WS_RESOURCES_WARNING        = 1955
    };

    enum Spells : uint32
    {
        SPELL_QUEST_REWARD_4_BASES  = 24061,
        SPELL_QUEST_REWARD_5_BASES  = 24064
    };

    enum Sounds : uint32
    {
        SOUND_NODE_CLAIMED              = 8192,
        SOUND_NODE_CAPTURED_ALLIANCE    = 8173,
        SOUND_NODE_CAPTURED_HORDE       = 8213,
        SOUND_NODE_ASSAULTED_ALLIANCE   = 8212,
        SOUND_NODE_ASSAULTED_HORDE      = 8174,
        SOUND_NEAR_VICTORY_ALLIANCE     = 8457,
        SOUND_NEAR_VICTORY_HORDE        = 8456
    };

    enum Texts : uint32
    {
        TEXT_ALLIANCE_NEAR_VICTORY  = 10598,
        TEXT_HORDE_NEAR_VICTORY     = 10599
    };

    struct NodeInfo
    {
        uint32 NeutralBanner;
        float X, Y;
        int32 StateWorldState;                              // + 0 Alliance, + 1 Horde, + 2 Alliance contested, + 3 Horde contested
        int32 IconWorldState;                               // map icon while neutral
        int32 WidgetAlliance, WidgetHorde;                  // 1.60 widget: 1 contested, 2 controlled
        uint32 KillCredit;
        std::array<uint32, PVP_TEAMS_COUNT> TextClaimed, TextAssaulted, TextDefended, TextTaken;
    };

    static constexpr std::array<NodeInfo, NodesCount> NodeData =
    { {
        { 180087, 1166.8f, 1200.1f, 1767, 1842, 17322, 17331, 15001, { 10286, 10287 }, { 10199, 10200 }, { 10201, 10202 }, { 10203, 10204 } },
        { 180088,  977.0f, 1046.6f, 1782, 1846, 17324, 17327, 15002, { 10290, 10291 }, { 10211, 10212 }, { 10215, 10216 }, { 10213, 10214 } },
        { 180089,  806.2f,  874.3f, 1772, 1845, 17325, 17328, 15003, { 10288, 10289 }, { 10217, 10218 }, { 10221, 10222 }, { 10219, 10220 } },
        { 180090,  856.2f, 1148.9f, 1792, 1844, 17326, 17330, 15004, { 10284, 10285 }, { 10224, 10225 }, { 10228, 10229 }, { 10226, 10227 } },
        { 180091, 1146.9f,  848.2f, 1787, 1843, 17323, 17329, 15005, { 10282, 10283 }, { 10230, 10231 }, { 10234, 10235 }, { 10232, 10233 } }
    } };

    // NodeStatus -> offset from NodeInfo::StateWorldState (VMaNGOS plusArray)
    static constexpr std::array<int32, 5> StateWorldStateOffset = { 0, 2, 3, 0, 1 };
}

struct battleground_classic_arathi_basin : BattlegroundScript
{
    explicit battleground_classic_arathi_basin(BattlegroundMap* map) : BattlegroundScript(map)
    {
        using namespace ClassicArathiBasin;
        _isWeekend = sBattlegroundMgr->IsBGWeekend(battleground->GetTypeID());
        _honorTicks = _isWeekend ? HonorTicksWeekend : HonorTicksNormal;
        _reputationTicks = _isWeekend ? ReputationTicksWeekend : ReputationTicksNormal;
    }

    void OnInit() override
    {
        using namespace ClassicArathiBasin;
        UpdateWorldState(WS_RESOURCES_MAX, MaxTeamScore);
        UpdateWorldState(WS_RESOURCES_WARNING, WarningNearVictoryScore);
        UpdateWorldState(WS_RESOURCES_ALLIANCE, 0);
        UpdateWorldState(WS_RESOURCES_HORDE, 0);
        UpdateWorldState(WS_OCCUPIED_BASES_ALLIANCE, 0);
        UpdateWorldState(WS_OCCUPIED_BASES_HORDE, 0);
        for (NodeInfo const& node : NodeData)
        {
            UpdateWorldState(node.IconWorldState, 1);
            for (int32 i = 0; i < 4; ++i)
                UpdateWorldState(node.StateWorldState + i, 0);
            UpdateWorldState(node.WidgetAlliance, 0);
            UpdateWorldState(node.WidgetHorde, 0);
        }

        // manual spawn groups: a despawned group stays despawned
        for (uint8 node = 0; node < NodesCount; ++node)
            battlegroundMap->SpawnGroupSpawn(SPAWN_GROUP_NODE_BASE + node * 10 + NODE_NEUTRAL, true);
        battlegroundMap->SpawnGroupSpawn(SPAWN_GROUP_GHOST_GATES, true);
    }

    void OnStart() override
    {
        for (ObjectGuid const& guid : _doors)
            if (GameObject* door = battlegroundMap->GetGameObject(guid))
            {
                door->UseDoorOrButton();
                door->DespawnOrUnsummon(3s);
            }

        battlegroundMap->SpawnGroupDespawn(ClassicArathiBasin::SPAWN_GROUP_GHOST_GATES);
    }

    void OnGameObjectCreate(GameObject* gameObject) override
    {
        switch (gameObject->GetEntry())
        {
            case ClassicArathiBasin::GO_ALLIANCE_DOOR:
            case ClassicArathiBasin::GO_HORDE_DOOR:
                _doors.insert(gameObject->GetGUID());
                break;
            default:
                break;
        }
    }

    void OnUpdate(uint32 diff) override
    {
        using namespace ClassicArathiBasin;
        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return;

        std::array<uint8, PVP_TEAMS_COUNT> occupied = { 0, 0 };
        for (uint8 node = 0; node < NodesCount; ++node)
        {
            // 1 minute from contested to occupied
            if (_nodeTimers[node] > 0ms)
            {
                _nodeTimers[node] -= Milliseconds(diff);
                if (_nodeTimers[node] <= 0ms)
                {
                    _nodeTimers[node] = 0ms;
                    TeamId const teamId = _nodes[node] == NODE_ALLIANCE_CONTESTED ? TEAM_ALLIANCE : TEAM_HORDE;
                    SetNodeStatus(node, NodeStatus(_nodes[node] + 2));
                    NodeOccupied(teamId);
                    battleground->SendBroadcastText(NodeData[node].TextTaken[teamId], teamId == TEAM_ALLIANCE ? CHAT_MSG_BG_SYSTEM_ALLIANCE : CHAT_MSG_BG_SYSTEM_HORDE);
                    battleground->PlaySoundToAll(teamId == TEAM_ALLIANCE ? SOUND_NODE_CAPTURED_ALLIANCE : SOUND_NODE_CAPTURED_HORDE);
                }
            }

            if (_nodes[node] == NODE_ALLIANCE_OCCUPIED)
                ++occupied[TEAM_ALLIANCE];
            else if (_nodes[node] == NODE_HORDE_OCCUPIED)
                ++occupied[TEAM_HORDE];
        }

        for (TeamId team : { TEAM_ALLIANCE, TEAM_HORDE })
        {
            uint8 const points = occupied[team];
            if (!points)
                continue;

            _lastTick[team] += diff;
            if (_lastTick[team] <= TickIntervals[points])
                continue;

            _lastTick[team] -= TickIntervals[points];
            Team const bgTeam = team == TEAM_ALLIANCE ? ALLIANCE : HORDE;
            battleground->AddPoint(bgTeam, TickPoints[points]);
            _honorScoreTicks[team] += TickPoints[points];
            _reputationScoreTicks[team] += TickPoints[points];

            if (_reputationScoreTicks[team] >= _reputationTicks)
            {
                battleground->RewardReputationToTeam(team == TEAM_ALLIANCE ? 509 : 510, 10, bgTeam);
                _reputationScoreTicks[team] -= _reputationTicks;
            }

            if (_honorScoreTicks[team] >= _honorTicks)
            {
                battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(1), bgTeam);
                _honorScoreTicks[team] -= _honorTicks;
            }

            uint32 teamScore = battleground->GetTeamScore(team);
            if (!_isInformedNearVictory && teamScore > WarningNearVictoryScore)
            {
                battleground->SendBroadcastText(team == TEAM_ALLIANCE ? TEXT_ALLIANCE_NEAR_VICTORY : TEXT_HORDE_NEAR_VICTORY, CHAT_MSG_BG_SYSTEM_NEUTRAL);
                battleground->PlaySoundToAll(team == TEAM_ALLIANCE ? SOUND_NEAR_VICTORY_ALLIANCE : SOUND_NEAR_VICTORY_HORDE);
                _isInformedNearVictory = true;
            }

            if (teamScore > MaxTeamScore)
            {
                battleground->SetTeamPoint(bgTeam, MaxTeamScore);
                teamScore = MaxTeamScore;
            }

            UpdateWorldState(team == TEAM_ALLIANCE ? WS_RESOURCES_ALLIANCE : WS_RESOURCES_HORDE, teamScore);
        }

        bool const allianceWins = battleground->GetTeamScore(TEAM_ALLIANCE) >= MaxTeamScore;
        bool const hordeWins = battleground->GetTeamScore(TEAM_HORDE) >= MaxTeamScore;
        if (allianceWins && hordeWins)
            EndBattle(TEAM_OTHER);
        else if (allianceWins)
            EndBattle(ALLIANCE);
        else if (hordeWins)
            EndBattle(HORDE);
    }

    void DoAction(uint32 actionId, WorldObject* source, WorldObject* target) override
    {
        if (actionId != ClassicArathiBasin::ACTION_BANNER_CLICKED)
            return;

        Player* player = Object::ToPlayer(source);
        GameObject* banner = Object::ToGameObject(target);
        if (player && banner)
            HandleBannerClick(player, banner);
    }

    Team GetPrematureWinner() override
    {
        if (battleground->GetTeamScore(TEAM_ALLIANCE) > battleground->GetTeamScore(TEAM_HORDE))
            return ALLIANCE;
        if (battleground->GetTeamScore(TEAM_HORDE) > battleground->GetTeamScore(TEAM_ALLIANCE))
            return HORDE;

        return BattlegroundScript::GetPrematureWinner();
    }

private:
    static int32 GetNodeForBanner(GameObject const* banner)
    {
        using namespace ClassicArathiBasin;
        for (uint8 node = 0; node < NodesCount; ++node)
            if (NodeData[node].NeutralBanner == banner->GetEntry() || banner->GetExactDist2d(NodeData[node].X, NodeData[node].Y) < 10.0f)
                return node;
        return -1;
    }

    void HandleBannerClick(Player* player, GameObject* banner)
    {
        using namespace ClassicArathiBasin;
        if (battleground->GetStatus() != STATUS_IN_PROGRESS || !player->CanUseBattlegroundObject(banner))
            return;

        int32 const nodeIndex = GetNodeForBanner(banner);
        Team const team = battleground->GetPlayerTeam(player->GetGUID());
        if (nodeIndex < 0 || (team != ALLIANCE && team != HORDE))
            return;

        uint8 const node = uint8(nodeIndex);
        TeamId const teamId = Battleground::GetTeamIndexByTeamId(team);

        // only a neutral node or a node of the other team (Alliance statuses are odd, Horde statuses even)
        if (!(_nodes[node] == NODE_NEUTRAL || teamId == _nodes[node] % 2))
            return;

        player->RemoveAurasByType(SPELL_AURA_MOD_STEALTH);
        player->RemoveAurasByType(SPELL_AURA_MOD_INVISIBILITY);
        player->KilledMonsterCredit(NodeData[node].KillCredit);

        ChatMsg const chatMsg = teamId == TEAM_ALLIANCE ? CHAT_MSG_BG_SYSTEM_ALLIANCE : CHAT_MSG_BG_SYSTEM_HORDE;
        NodeStatus const contested = teamId == TEAM_ALLIANCE ? NODE_ALLIANCE_CONTESTED : NODE_HORDE_CONTESTED;
        NodeStatus const occupiedStatus = teamId == TEAM_ALLIANCE ? NODE_ALLIANCE_OCCUPIED : NODE_HORDE_OCCUPIED;
        uint32 sound = teamId == TEAM_ALLIANCE ? SOUND_NODE_ASSAULTED_ALLIANCE : SOUND_NODE_ASSAULTED_HORDE;

        if (_nodes[node] == NODE_NEUTRAL)
        {
            // claim
            battleground->UpdatePvpStat(player, PVP_STAT_BASES_ASSAULTED, 1);
            SetNodeStatus(node, contested);
            _nodeTimers[node] = CapturingTime;
            battleground->SendBroadcastText(NodeData[node].TextClaimed[teamId], chatMsg, player);
            sound = SOUND_NODE_CLAIMED;
        }
        else if (_nodes[node] == NODE_ALLIANCE_CONTESTED || _nodes[node] == NODE_HORDE_CONTESTED)
        {
            if (_prevNodes[node] < NODE_ALLIANCE_OCCUPIED)
            {
                // contested from neutral: assault it again
                battleground->UpdatePvpStat(player, PVP_STAT_BASES_ASSAULTED, 1);
                SetNodeStatus(node, contested);
                _nodeTimers[node] = CapturingTime;
                battleground->SendBroadcastText(NodeData[node].TextAssaulted[teamId], chatMsg, player);
            }
            else
            {
                // our node was assaulted: defend it
                battleground->UpdatePvpStat(player, PVP_STAT_BASES_DEFENDED, 1);
                SetNodeStatus(node, occupiedStatus);
                _nodeTimers[node] = 0ms;
                NodeOccupied(teamId);
                battleground->SendBroadcastText(NodeData[node].TextDefended[teamId], chatMsg, player);
                battleground->SendBroadcastText(NodeData[node].TextTaken[teamId], chatMsg);
            }
        }
        else
        {
            // occupied by the other team: assault
            battleground->UpdatePvpStat(player, PVP_STAT_BASES_ASSAULTED, 1);
            SetNodeStatus(node, contested);
            _nodeTimers[node] = CapturingTime;
            battleground->SendBroadcastText(NodeData[node].TextAssaulted[teamId], chatMsg, player);
        }

        battleground->PlaySoundToAll(sound);
    }

    void SetNodeStatus(uint8 node, ClassicArathiBasin::NodeStatus status)
    {
        using namespace ClassicArathiBasin;
        NodeStatus const old = NodeStatus(_nodes[node]);
        uint32 const groupBase = SPAWN_GROUP_NODE_BASE + node * 10;

        // banners (VMaNGOS battleground_events node / status)
        battlegroundMap->SpawnGroupDespawn(groupBase + old);
        if (old == NODE_ALLIANCE_CONTESTED || old == NODE_HORDE_CONTESTED)
            battlegroundMap->SpawnGroupDespawn(groupBase + SPAWN_GROUP_CONTESTED_AURA);
        if (node == NODE_FARM && (old == NODE_ALLIANCE_OCCUPIED || old == NODE_HORDE_OCCUPIED))
            battlegroundMap->SpawnGroupDespawn(old == NODE_ALLIANCE_OCCUPIED ? SPAWN_GROUP_FARM_ALLIANCE : SPAWN_GROUP_FARM_HORDE);

        battlegroundMap->SpawnGroupSpawn(groupBase + status, true);
        if (status == NODE_ALLIANCE_CONTESTED || status == NODE_HORDE_CONTESTED)
            battlegroundMap->SpawnGroupSpawn(groupBase + SPAWN_GROUP_CONTESTED_AURA, true);
        if (node == NODE_FARM && (status == NODE_ALLIANCE_OCCUPIED || status == NODE_HORDE_OCCUPIED))
            battlegroundMap->SpawnGroupSpawn(status == NODE_ALLIANCE_OCCUPIED ? SPAWN_GROUP_FARM_ALLIANCE : SPAWN_GROUP_FARM_HORDE, true);

        _prevNodes[node] = old;
        _nodes[node] = status;

        // map icons (graveyard conditions use the occupied states too)
        NodeInfo const& info = NodeData[node];
        if (old == NODE_NEUTRAL)
            UpdateWorldState(info.IconWorldState, 0);
        else
            UpdateWorldState(info.StateWorldState + StateWorldStateOffset[old], 0);
        if (status == NODE_NEUTRAL)
            UpdateWorldState(info.IconWorldState, 1);
        else
            UpdateWorldState(info.StateWorldState + StateWorldStateOffset[status], 1);

        // 1.60 widget
        UpdateWorldState(info.WidgetAlliance, status == NODE_ALLIANCE_CONTESTED ? 1 : status == NODE_ALLIANCE_OCCUPIED ? 2 : 0);
        UpdateWorldState(info.WidgetHorde, status == NODE_HORDE_CONTESTED ? 1 : status == NODE_HORDE_OCCUPIED ? 2 : 0);

        uint8 alliance = 0, horde = 0;
        for (uint8 nodeStatus : _nodes)
        {
            if (nodeStatus == NODE_ALLIANCE_OCCUPIED)
                ++alliance;
            else if (nodeStatus == NODE_HORDE_OCCUPIED)
                ++horde;
        }
        UpdateWorldState(WS_OCCUPIED_BASES_ALLIANCE, alliance);
        UpdateWorldState(WS_OCCUPIED_BASES_HORDE, horde);
    }

    // quest credit for holding 4 / 5 bases (VMaNGOS _NodeOccupied)
    void NodeOccupied(TeamId teamId)
    {
        using namespace ClassicArathiBasin;
        NodeStatus const occupiedStatus = teamId == TEAM_ALLIANCE ? NODE_ALLIANCE_OCCUPIED : NODE_HORDE_OCCUPIED;
        uint8 captured = 0;
        for (uint8 node = 0; node < NodesCount; ++node)
            if (_nodes[node] == occupiedStatus && _nodeTimers[node] <= 0ms)
                ++captured;

        Team const team = teamId == TEAM_ALLIANCE ? ALLIANCE : HORDE;
        if (captured >= 5)
            battleground->CastSpellOnTeam(SPELL_QUEST_REWARD_5_BASES, team);
        if (captured >= 4)
            battleground->CastSpellOnTeam(SPELL_QUEST_REWARD_4_BASES, team);
    }

    void EndBattle(Team winner)
    {
        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return;

        // vanilla (VMaNGOS BattleGroundAB::EndBattleGround): winner 1 honorable kill, doubled on the holiday weekend
        if (winner == ALLIANCE || winner == HORDE)
            battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(_isWeekend ? 2 : 1), winner);

        battleground->EndBattleground(winner);
    }

    static constexpr uint32 PVP_STAT_BASES_ASSAULTED = 926;
    static constexpr uint32 PVP_STAT_BASES_DEFENDED = 927;

    std::array<uint8, ClassicArathiBasin::NodesCount> _nodes = { };
    std::array<uint8, ClassicArathiBasin::NodesCount> _prevNodes = { };
    std::array<Milliseconds, ClassicArathiBasin::NodesCount> _nodeTimers = { { 0ms, 0ms, 0ms, 0ms, 0ms } };
    std::array<uint32, PVP_TEAMS_COUNT> _lastTick = { };
    std::array<uint32, PVP_TEAMS_COUNT> _honorScoreTicks = { };
    std::array<uint32, PVP_TEAMS_COUNT> _reputationScoreTicks = { };
    bool _isInformedNearVictory = false;
    bool _isWeekend = false;
    uint32 _honorTicks = ClassicArathiBasin::HonorTicksNormal;
    uint32 _reputationTicks = ClassicArathiBasin::ReputationTicksNormal;
    GuidSet _doors;
};

// Classic 1.60: Arathi Basin banners on map 529 (180087 - 180091 neutral, 180058 - 180061 faction / contested)
struct go_classic_ab_banner : GameObjectAI
{
    explicit go_classic_ab_banner(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        if (me->GetMapId() != ClassicArathiBasin::MapId)
            return false;

        if (ZoneScript* zoneScript = me->GetZoneScript())
            zoneScript->DoAction(ClassicArathiBasin::ACTION_BANNER_CLICKED, player, me);

        return true;
    }
};

void AddSC_battleground_arathi_basin()
{
    RegisterBattlegroundMapScript(battleground_arathi_basin, 2107);

    // Classic 1.60
    RegisterBattlegroundMapScript(battleground_classic_arathi_basin, ClassicArathiBasin::MapId);
    RegisterGameObjectAI(go_classic_ab_banner);
}
