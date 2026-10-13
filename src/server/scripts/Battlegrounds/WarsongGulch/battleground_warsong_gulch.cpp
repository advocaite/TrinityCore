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

#include "AreaTrigger.h"
#include "BattlegroundScript.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "BattlegroundPackets.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "GameTime.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "Timer.h"

struct battleground_warsong_gulch : BattlegroundScript
{
    static constexpr Seconds FLAG_ASSAULT_TIMER = 30s;

    enum Misc
    {
        MAX_TEAM_SCORE                          = 3,
        FLAG_BRUTAL_ASSAULT_STACK_COUNT         = 5
    };

    enum Spells : uint32
    {
        SPELL_FOCUSED_ASSAULT                   = 46392,
        SPELL_BRUTAL_ASSAULT                    = 46393,
        SPELL_CAPTURED_ALLIANCE_COSMETIC_FX     = 262508,
        SPELL_CAPTURED_HORDE_COSMETIC_FX        = 262512,
        SPELL_WARSONG_FLAG                      = 23333,
        SPELL_WARSONG_FLAG_DROPPED              = 23334,
        SPELL_SILVERWING_FLAG                   = 23335,
        SPELL_SILVERWING_FLAG_DROPPED           = 23336,
        SPELL_QUICK_CAP_TIMER                   = 183317,
    };

    enum AreaTriggers : uint32
    {
        AT_CAPTURE_POINT_ALLIANCE               = 30,
        AT_CAPTURE_POINT_HORDE                  = 31
    };

    enum Events : uint32
    {
        EVENT_START_BATTLE                      = 35912
    };

    enum PvpStats : uint32
    {
        PVP_STAT_FLAG_CAPTURES                  = 928,
        PVP_STAT_FLAG_RETURNS                   = 929
    };

    enum WorldStates : int32
    {
        WORLD_STATE_FLAG_STATE_ALLIANCE             = 1545,
        WORLD_STATE_FLAG_STATE_HORDE                = 1546,
        WORLD_STATE_FLAG_STATE_NEUTRAL              = 1547,
        WORLD_STATE_HORDE_FLAG_COUNT_PICKED_UP      = 17712,
        WORLD_STATE_ALLIANCE_FLAG_COUNT_PICKED_UP   = 17713,
        WORLD_STATE_FLAG_CAPTURES_ALLIANCE          = 1581,
        WORLD_STATE_FLAG_CAPTURES_HORDE             = 1582,
        WORLD_STATE_FLAG_CAPTURES_MAX               = 1601,
        WORLD_STATE_FLAG_CAPTURES_MAX_NEW           = 17303,
        WORLD_STATE_FLAG_CONTROL_HORDE              = 2338,
        WORLD_STATE_FLAG_CONTROL_ALLIANCE           = 2339,
        WORLD_STATE_STATE_TIMER                     = 4248,
        WORLD_STATE_STATE_TIMER_ACTIVE              = 4247
    };

    enum Texts
    {
        TEXT_START_ONE_MINUTE                   = 10015,
        TEXT_START_HALF_MINUTE                  = 10016,
        TEXT_BATTLE_HAS_BEGUN                   = 10014,
        TEXT_CAPTURED_HORDE_FLAG                = 9801,
        TEXT_CAPTURED_ALLIANCE_FLAG             = 9802,
        TEXT_FLAGS_PLACED                       = 9803,
        TEXT_ALLIANCE_FLAG_PICKED_UP            = 9804,
        TEXT_ALLIANCE_FLAG_DROPPED              = 9805,
        TEXT_HORDE_FLAG_PICKED_UP               = 9807,
        TEXT_HORDE_FLAG_DROPPED                 = 9806,
        TEXT_ALLIANCE_FLAG_RETURNED             = 9808,
        TEXT_HORDE_FLAG_RETURNED                = 9809
    };

    enum Sounds
    {
        SOUND_FLAG_CAPTURED_ALLIANCE            = 8173,
        SOUND_FLAG_CAPTURED_HORDE               = 8213,
        SOUND_FLAG_PLACED                       = 8232,
        SOUND_FLAG_RETURNED                     = 8192,
        SOUND_HORDE_FLAG_PICKED_UP              = 8212,
        SOUND_ALLIANCE_FLAG_PICKED_UP           = 8174,
        SOUND_FLAGS_RESPAWNED                   = 8232
    };

    enum GameObjects
    {
        OBJECT_ALLIANCE_DOOR                    = 309704,
        OBJECT_PORTCULLIS_009                   = 309705,
        OBJECT_PORTCULLIS_002                   = 309883,
        OBJECT_COLLISION_PC_SIZE                = 242273,
        OBJECT_HORDE_GATE_1                     = 352709,
        OBJECT_HORDE_GATE_2                     = 352710,
        OBJECT_ALLIANCE_FLAG_IN_BASE            = 227741,
        OBJECT_HORDE_FLAG_IN_BASE               = 227740
    };

    static constexpr std::array<std::array<uint32, 3>, 2> HONOR_REWARDS =
    {{
        { 20, 40, 40 },
        { 60, 40, 80 }
    }};

    explicit battleground_warsong_gulch(BattlegroundMap* map) : BattlegroundScript(map), _lastFlagCaptureTeam(TEAM_OTHER), _bothFlagsKept(false), _flags({ }), _assaultStackCount(0), _capturePointAreaTriggers({ })
    {
        _flagAssaultTimer.Reset(FLAG_ASSAULT_TIMER);

        if (sBattlegroundMgr->IsBGWeekend(battleground->GetTypeID()))
        {
            _reputationCapture = 45;
            _honorWinKills = 3;
            _honorEndKills = 4;
        }
        else
        {
            _reputationCapture = 35;
            _honorWinKills = 1;
            _honorEndKills = 2;
        }
    }

    void OnUpdate(uint32 diff) override
    {
        BattlegroundScript::OnUpdate(diff);

        if (battleground->GetStatus() == STATUS_IN_PROGRESS)
        {
            if (battleground->GetElapsedTime() >= 17 * MINUTE * IN_MILLISECONDS)
            {
                if (battleground->GetTeamScore(TEAM_ALLIANCE) == 0)
                {
                    if (battleground->GetTeamScore(TEAM_HORDE) == 0) // No one scored - result is tie
                        battleground->EndBattleground(TEAM_OTHER);
                    else // Horde has more points and thus wins
                        battleground->EndBattleground(HORDE);
                }
                else if (battleground->GetTeamScore(TEAM_HORDE) == 0) // Alliance has > 0, Horde has 0, alliance wins
                    battleground->EndBattleground(ALLIANCE);
                else if (battleground->GetTeamScore(TEAM_HORDE) == battleground->GetTeamScore(TEAM_ALLIANCE)) // Team score equal, winner is team that scored the last flag
                    battleground->EndBattleground(_lastFlagCaptureTeam);
                else if (battleground->GetTeamScore(TEAM_HORDE) > battleground->GetTeamScore(TEAM_ALLIANCE))  // Last but not least, check who has the higher score
                    battleground->EndBattleground(HORDE);
                else
                    battleground->EndBattleground(ALLIANCE);
            }
        }

        if (_bothFlagsKept)
        {
            _flagAssaultTimer.Update(diff);
            if (_flagAssaultTimer.Passed())
            {
                _flagAssaultTimer.Reset(FLAG_ASSAULT_TIMER);
                if (_assaultStackCount < std::numeric_limits<uint8>::max())
                {
                    _assaultStackCount++;

                    // update assault debuff stacks
                    DoForFlagKeepers([&](Player* player) -> void
                    {
                        ApplyAssaultDebuffToPlayer(player);
                    });
                }
            }
        }
    }

    void OnStart() override
    {
        BattlegroundScript::OnStart();
        for (ObjectGuid door : _doors)
        {
            if (GameObject* gameObject = battlegroundMap->GetGameObject(door))
            {
                gameObject->UseDoorOrButton();
                gameObject->DespawnOrUnsummon(3s);
            }
        }

        UpdateWorldState(WORLD_STATE_STATE_TIMER_ACTIVE, 1);
        UpdateWorldState(WORLD_STATE_STATE_TIMER, std::chrono::system_clock::to_time_t(GameTime::GetSystemTime() + 15min));

        // players joining later are not eligibles
        TriggerGameEvent(EVENT_START_BATTLE);
    }

    void OnEnd(Team winner) override
    {
        BattlegroundScript::OnEnd(winner);
        // Win reward
        if (winner == ALLIANCE)
            battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(_honorWinKills), ALLIANCE);
        if (winner == HORDE)
            battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(_honorWinKills), HORDE);

        // Complete map_end rewards (even if no team wins)
        battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(_honorEndKills), ALLIANCE);
        battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(_honorEndKills), HORDE);
    }

    template <std::invocable<Player*> Action>
    void DoForFlagKeepers(Action const& action) const
    {
        for (ObjectGuid flagGUID : _flags)
            if (GameObject const* flag = battlegroundMap->GetGameObject(flagGUID))
                if (Player* carrier = ObjectAccessor::FindPlayer(flag->GetFlagCarrierGUID()))
                    action(carrier);
    }

    void ResetAssaultDebuff()
    {
        _bothFlagsKept = false;
        _assaultStackCount = 0;
        _flagAssaultTimer.Reset(FLAG_ASSAULT_TIMER);
        DoForFlagKeepers([&](Player* player) -> void
        {
            RemoveAssaultDebuffFromPlayer(player);
        });
    }

    void ApplyAssaultDebuffToPlayer(Player* player) const
    {
        if (_assaultStackCount == 0)
            return;

        uint32 spellId = SPELL_FOCUSED_ASSAULT;
        if (_assaultStackCount >= FLAG_BRUTAL_ASSAULT_STACK_COUNT)
        {
            player->RemoveAurasDueToSpell(SPELL_FOCUSED_ASSAULT);
            spellId = SPELL_BRUTAL_ASSAULT;
        }

        Aura* aura = player->GetAura(spellId);
        if (!aura)
        {
            player->CastSpell(player, spellId, true);
            aura = player->GetAura(spellId);
        }

        if (aura)
            aura->SetStackAmount(_assaultStackCount);
    }

    void RemoveAssaultDebuffFromPlayer(Player* player) const
    {
        player->RemoveAurasDueToSpell(SPELL_FOCUSED_ASSAULT);
        player->RemoveAurasDueToSpell(SPELL_BRUTAL_ASSAULT);
    }

    FlagState GetFlagState(TeamId team) const
    {
        if (GameObject const* flag = battlegroundMap->GetGameObject(_flags[team]))
            return flag->GetFlagState();

        return FlagState(0);
    }

    ObjectGuid const& GetFlagCarrierGUID(TeamId team) const
    {
        if (GameObject const* flag = battlegroundMap->GetGameObject(_flags[team]))
            return flag->GetFlagCarrierGUID();

        return ObjectGuid::Empty;
    }

    void HandleFlagRoomCapturePoint()
    {
        DoForFlagKeepers([&](Player* player) -> void
        {
            TeamId const team = Battleground::GetTeamIndexByTeamId(battleground->GetPlayerTeam(player->GetGUID()));
            if (AreaTrigger* trigger = battlegroundMap->GetAreaTrigger(_capturePointAreaTriggers[team]))
                if (trigger->GetInsideUnits().contains(player->GetGUID()))
                    if (CanCaptureFlag(trigger, player))
                        OnCaptureFlag(trigger, player);
        });
    }

    void UpdateFlagState(uint32 team, FlagState value) const
    {
        auto transformValueToOtherTeamControlWorldState = [](FlagState state)
        {
            switch (state)
            {
                case FlagState::InBase:
                case FlagState::Dropped:
                case FlagState::Respawning:
                    return 1;
                case FlagState::Taken:
                    return 2;
                default:
                    return 0;
            }
        };

        if (team == ALLIANCE)
        {
            UpdateWorldState(WORLD_STATE_FLAG_STATE_ALLIANCE, AsUnderlyingType(value));
            UpdateWorldState(WORLD_STATE_FLAG_CONTROL_HORDE, transformValueToOtherTeamControlWorldState(value));
        }
        else
        {
            UpdateWorldState(WORLD_STATE_FLAG_STATE_HORDE, AsUnderlyingType(value));
            UpdateWorldState(WORLD_STATE_FLAG_CONTROL_ALLIANCE, transformValueToOtherTeamControlWorldState(value));
        }
    }

    void UpdateTeamScore(TeamId team) const
    {
        if (team == TEAM_ALLIANCE)
            UpdateWorldState(WORLD_STATE_FLAG_CAPTURES_ALLIANCE, battleground->GetTeamScore(team));
        else
            UpdateWorldState(WORLD_STATE_FLAG_CAPTURES_HORDE, battleground->GetTeamScore(team));
    }

    void OnGameObjectCreate(GameObject* gameObject) override
    {
        BattlegroundScript::OnGameObjectCreate(gameObject);
        switch (gameObject->GetEntry())
        {
            case OBJECT_ALLIANCE_DOOR:
            case OBJECT_PORTCULLIS_009:
            case OBJECT_PORTCULLIS_002:
            case OBJECT_COLLISION_PC_SIZE:
            case OBJECT_HORDE_GATE_1:
            case OBJECT_HORDE_GATE_2:
                _doors.insert(gameObject->GetGUID());
                break;
            case OBJECT_ALLIANCE_FLAG_IN_BASE:
                _flags[TEAM_ALLIANCE] = gameObject->GetGUID();
                break;
            case OBJECT_HORDE_FLAG_IN_BASE:
                _flags[TEAM_HORDE] = gameObject->GetGUID();
                break;
            default:
                break;
        }
    }

    void OnAreaTriggerCreate(AreaTrigger* areaTrigger) override
    {
        BattlegroundScript::OnAreaTriggerCreate(areaTrigger);
        if (!areaTrigger->IsStaticSpawn())
            return;

        switch (areaTrigger->GetEntry())
        {
            case AT_CAPTURE_POINT_ALLIANCE:
                _capturePointAreaTriggers[TEAM_ALLIANCE] = areaTrigger->GetGUID();
                break;
            case AT_CAPTURE_POINT_HORDE:
                _capturePointAreaTriggers[TEAM_HORDE] = areaTrigger->GetGUID();
                break;
            default:
                break;
        }
    }

    void OnFlagStateChange(GameObject* flagInBase, FlagState oldValue, FlagState newValue, Player* player) override
    {
        BattlegroundScript::OnFlagStateChange(flagInBase, oldValue, newValue, player);

        Team const team = flagInBase->GetEntry() == OBJECT_HORDE_FLAG_IN_BASE ? HORDE : ALLIANCE;
        TeamId const otherTeamId = Battleground::GetTeamIndexByTeamId(GetOtherTeam(team));

        UpdateFlagState(team, newValue);

        switch (newValue)
        {
            case FlagState::InBase:
            {
                if (battleground->GetStatus() == STATUS_IN_PROGRESS)
                {
                    ResetAssaultDebuff();
                    if (player)
                    {
                        // flag got returned to base by player interaction
                        battleground->UpdatePvpStat(player, PVP_STAT_FLAG_RETURNS, 1);      // +1 flag returns

                        if (team == ALLIANCE)
                        {
                            battleground->SendBroadcastText(TEXT_ALLIANCE_FLAG_RETURNED, CHAT_MSG_BG_SYSTEM_ALLIANCE, player);
                            battleground->PlaySoundToAll(SOUND_FLAG_RETURNED);
                        }
                        else
                        {
                            battleground->SendBroadcastText(TEXT_HORDE_FLAG_RETURNED, CHAT_MSG_BG_SYSTEM_HORDE, player);
                            battleground->PlaySoundToAll(SOUND_FLAG_RETURNED);
                        }
                    }
                    // Flag respawned due to timeout/capture
                    else if (GetFlagState(otherTeamId) != FlagState::Respawning)
                    {
                        // if other flag is respawning, we will let that one handle the message and sound to prevent double message/sound.
                        battleground->SendBroadcastText(TEXT_FLAGS_PLACED, CHAT_MSG_BG_SYSTEM_NEUTRAL);
                        battleground->PlaySoundToAll(SOUND_FLAGS_RESPAWNED);
                    }

                    HandleFlagRoomCapturePoint();
                }
                break;
            }
            case FlagState::Dropped:
            {
                player->RemoveAurasDueToSpell(SPELL_QUICK_CAP_TIMER);
                RemoveAssaultDebuffFromPlayer(player);

                uint32 recentlyDroppedSpellId = SPELL_RECENTLY_DROPPED_HORDE_FLAG;
                if (team == ALLIANCE)
                {
                    recentlyDroppedSpellId = SPELL_RECENTLY_DROPPED_ALLIANCE_FLAG;
                    battleground->SendBroadcastText(TEXT_ALLIANCE_FLAG_DROPPED, CHAT_MSG_BG_SYSTEM_ALLIANCE, player);
                }
                else
                    battleground->SendBroadcastText(TEXT_HORDE_FLAG_DROPPED, CHAT_MSG_BG_SYSTEM_HORDE, player);

                player->CastSpell(player, recentlyDroppedSpellId, true);
                break;
            }
            case FlagState::Taken:
            {
                if (team == HORDE)
                {
                    battleground->SendBroadcastText(TEXT_HORDE_FLAG_PICKED_UP, CHAT_MSG_BG_SYSTEM_HORDE, player);
                    battleground->PlaySoundToAll(SOUND_HORDE_FLAG_PICKED_UP);
                }
                else
                {
                    battleground->SendBroadcastText(TEXT_ALLIANCE_FLAG_PICKED_UP, CHAT_MSG_BG_SYSTEM_ALLIANCE, player);
                    battleground->PlaySoundToAll(SOUND_ALLIANCE_FLAG_PICKED_UP);
                }

                if (GetFlagState(otherTeamId) == FlagState::Taken)
                    _bothFlagsKept = true;

                ApplyAssaultDebuffToPlayer(player);

                flagInBase->CastSpell(player, SPELL_QUICK_CAP_TIMER, true);
                player->StartCriteria(CriteriaStartEvent::BeSpellTarget, SPELL_QUICK_CAP_TIMER, Seconds(GameTime::GetGameTime() - flagInBase->GetFlagTakenFromBaseTime()));
                break;
            }
            case FlagState::Respawning:
                ResetAssaultDebuff();
                break;
            default:
                break;
        }
    }

    bool CanCaptureFlag(AreaTrigger* areaTrigger, Player* player) override
    {
        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return false;

        Team const team = battleground->GetPlayerTeam(player->GetGUID());
        TeamId const teamId = Battleground::GetTeamIndexByTeamId(team);
        TeamId const otherTeamId = Battleground::GetTeamIndexByTeamId(GetOtherTeam(team));

        if (areaTrigger->GetGUID() != _capturePointAreaTriggers[teamId])
            return false;

        // check if enemy flag's carrier is this player
        if (GetFlagCarrierGUID(otherTeamId) != player->GetGUID())
            return false;

        // check that team's flag is in base
        return GetFlagState(teamId) == FlagState::InBase;
    }

    void OnCaptureFlag(AreaTrigger* areaTrigger, Player* player) override
    {
        BattlegroundScript::OnCaptureFlag(areaTrigger, player);

        Team winner = TEAM_OTHER;

        Team const team = battleground->GetPlayerTeam(player->GetGUID());
        TeamId const teamId = Battleground::GetTeamIndexByTeamId(team);
        TeamId const otherTeamId = Battleground::GetTeamIndexByTeamId(GetOtherTeam(team));

        /*
            1. Update flag states & score world states
            2. update points
            3. chat message & sound
            4. update criterias & achievements
            5. remove all related auras
            ?. Reward honor & reputation
        */

        // 1. update the flag states
        for (ObjectGuid const& flagGuid: _flags)
            if (GameObject const* flag = battlegroundMap->GetGameObject(flagGuid))
                flag->HandleCustomTypeCommand(GameObjectType::SetNewFlagState(FlagState::Respawning, player));

        // 2. update points
        if (battleground->GetTeamScore(teamId) < MAX_TEAM_SCORE)
            battleground->AddPoint(team, 1);

        UpdateTeamScore(teamId);

        // 3. chat message & sound
        if (team == ALLIANCE)
        {
            battleground->SendBroadcastText(TEXT_CAPTURED_HORDE_FLAG, CHAT_MSG_BG_SYSTEM_HORDE, player);
            battleground->PlaySoundToAll(SOUND_FLAG_CAPTURED_ALLIANCE);
            battleground->RewardReputationToTeam(890, _reputationCapture, ALLIANCE);
            player->CastSpell(player, SPELL_CAPTURED_ALLIANCE_COSMETIC_FX);
        }
        else
        {
            battleground->SendBroadcastText(TEXT_CAPTURED_ALLIANCE_FLAG, CHAT_MSG_BG_SYSTEM_ALLIANCE, player);
            battleground->PlaySoundToAll(SOUND_FLAG_CAPTURED_HORDE);
            battleground->RewardReputationToTeam(889, _reputationCapture, HORDE);
            player->CastSpell(player, SPELL_CAPTURED_HORDE_COSMETIC_FX);
        }

        // 4. update criteria's for achievement, player score etc.
        battleground->UpdatePvpStat(player, PVP_STAT_FLAG_CAPTURES, 1);      // +1 flag captures

        // 5. Remove all related auras
        RemoveAssaultDebuffFromPlayer(player);

        if (GameObject const* flag = battlegroundMap->GetGameObject(_flags[otherTeamId]))
            player->RemoveAurasDueToSpell(flag->GetGOInfo()->newflag.pickupSpell, flag->GetGUID());

        player->RemoveAurasDueToSpell(SPELL_QUICK_CAP_TIMER);

        player->RemoveAurasWithInterruptFlags(SpellAuraInterruptFlags::PvPActive);

        battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(2), team);

        // update last flag capture to be used if teamscore is equal
        SetLastFlagCapture(team);

        if (battleground->GetTeamScore(teamId) == MAX_TEAM_SCORE)
            winner = team;

        if (winner)
        {
            UpdateWorldState(WORLD_STATE_FLAG_STATE_ALLIANCE, 1);
            UpdateWorldState(WORLD_STATE_FLAG_STATE_HORDE, 1);
            UpdateWorldState(WORLD_STATE_STATE_TIMER_ACTIVE, 0);

            battleground->RewardHonorToTeam(HONOR_REWARDS[BattlegroundMgr::IsBGWeekend(battleground->GetTypeID()) ? 1 : 0][0], winner);
            battleground->EndBattleground(winner);
        }
    }

    Team GetPrematureWinner() override
    {
        if (battleground->GetTeamScore(TEAM_ALLIANCE) > battleground->GetTeamScore(TEAM_HORDE))
            return ALLIANCE;
        if (battleground->GetTeamScore(TEAM_HORDE) > battleground->GetTeamScore(TEAM_ALLIANCE))
            return HORDE;

        return BattlegroundScript::GetPrematureWinner();
    }

    void SetLastFlagCapture(Team team)
    {
        _lastFlagCaptureTeam = team;
    }

private:
    Team _lastFlagCaptureTeam;
    bool _bothFlagsKept;
    GuidSet _doors;
    std::array<ObjectGuid, PVP_TEAMS_COUNT> _flags;

    TimeTracker _flagAssaultTimer;
    uint8 _assaultStackCount;
    std::array<ObjectGuid, PVP_TEAMS_COUNT> _capturePointAreaTriggers;

    uint32 _honorWinKills;
    uint32 _honorEndKills;
    uint32 _reputationCapture;
};

// Classic 1.60: vanilla Warsong Gulch on the original map 489 (port of VMaNGOS BattleGroundWS). The retail script above
// needs the 2106 remake objects (type 36 flags, capture area triggers 30 / 31). Map 489 has the vanilla flag stands
// (type 24) and dropped flags (type 26), and the 1.60 client has no capture area trigger (3646 / 3647), so captures are
// checked by distance to the vanilla trigger positions. Flag stands / ghost gates are spawn groups (world SQL 2026_10_11_41).
namespace ClassicWarsongGulch
{
    static constexpr uint32 MapId = 489;
    static constexpr uint32 MaxTeamScore = 3;
    static constexpr Milliseconds FlagRespawnTime = 23s;    // after a capture
    static constexpr Milliseconds FlagDropTime = 10s;       // dropped flag returns to its base
    static constexpr float CaptureRadius = 6.0f;            // areatrigger radius 5 + tolerance
    static constexpr std::array<Position, PVP_TEAMS_COUNT> CapturePoints =
    { {
        { 1539.89f, 1481.36f, 352.659f },                   // 3646 Silverwing Hold, Alliance capture point
        { 918.496f, 1434.04f, 346.054f }                    // 3647 Warsong Lumber Mill, Horde capture point
    } };

    enum Actions : uint32
    {
        ACTION_FLAG_CLICKED         = 1,                    // source = player, target = flag gameobject
        ACTION_FLAG_AURA_REMOVED    = 2                     // source = flag carrier
    };

    enum GameObjects : uint32
    {
        GO_SILVERWING_FLAG          = 179830,               // Alliance flag in the Alliance base, taken by the Horde
        GO_WARSONG_FLAG             = 179831,
        GO_SILVERWING_FLAG_DROPPED  = 179785,
        GO_WARSONG_FLAG_DROPPED     = 179786,
        GO_DOOR_FIRST               = 179916,               // 179916 - 179921 Razorfen doors / portcullis
        GO_DOOR_LAST                = 179921
    };

    enum Spells : uint32
    {
        SPELL_WARSONG_FLAG          = 23333,                // Horde flag carried by an Alliance player
        SPELL_SILVERWING_FLAG       = 23335
    };

    enum SpawnGroups : uint32
    {
        SPAWN_GROUP_ALLIANCE_FLAG   = 489001,
        SPAWN_GROUP_HORDE_FLAG      = 489002,
        SPAWN_GROUP_GHOST_GATES     = 489003,
        SPAWN_GROUP_SPIRIT_GUIDES   = 489004                // VMaNGOS WS_EVENT_SPIRITGUIDES_SPAWN (world SQL 2026_10_12_40)
    };

    enum WorldStates : int32
    {
        WS_FLAG_TAKEN_ALLIANCE      = 1545,                 // 0 in base, 1 carried, -1 on the ground
        WS_FLAG_TAKEN_HORDE         = 1546,
        WS_CAPTURES_ALLIANCE        = 1581,
        WS_CAPTURES_HORDE           = 1582,
        WS_CAPTURES_MAX             = 1601,
        WS_CAPTURES_MAX_NEW         = 17303,
        WS_FLAG_STATE_HORDE         = 2338,                 // 2 = the Horde carries the Alliance flag
        WS_FLAG_STATE_ALLIANCE      = 2339,
        WS_BATTLE_IN_PROGRESS       = 489000                // server only, used by the flag room graveyard conditions
    };

    enum Texts : uint32
    {
        TEXT_CAPTURED_HORDE_FLAG    = 9801,
        TEXT_CAPTURED_ALLIANCE_FLAG = 9802,
        TEXT_FLAGS_PLACED           = 9803,
        TEXT_ALLIANCE_FLAG_PICKED   = 9804,
        TEXT_ALLIANCE_FLAG_DROPPED  = 9805,
        TEXT_HORDE_FLAG_DROPPED     = 9806,
        TEXT_HORDE_FLAG_PICKED      = 9807,
        TEXT_ALLIANCE_FLAG_RETURNED = 9808,
        TEXT_HORDE_FLAG_RETURNED    = 9809,
        TEXT_ALLIANCE_FLAG_RESPAWN  = 10022,
        TEXT_HORDE_FLAG_RESPAWN     = 10023
    };

    enum Sounds : uint32
    {
        SOUND_FLAG_CAPTURED_ALLIANCE    = 8173,
        SOUND_FLAG_CAPTURED_HORDE       = 8213,
        SOUND_FLAG_RETURNED             = 8192,
        SOUND_HORDE_FLAG_PICKED_UP      = 8212,
        SOUND_ALLIANCE_FLAG_PICKED_UP   = 8174,
        SOUND_FLAGS_RESPAWNED           = 8232
    };

    enum class WsgFlagState : uint8
    {
        OnBase,
        WaitRespawn,
        OnPlayer,
        OnGround
    };

    // indexed by the team owning the flag
    static constexpr uint32 CarrierSpell(TeamId flagTeam) { return flagTeam == TEAM_ALLIANCE ? SPELL_SILVERWING_FLAG : SPELL_WARSONG_FLAG; }
    static constexpr uint32 FlagSpawnGroup(TeamId flagTeam) { return flagTeam == TEAM_ALLIANCE ? SPAWN_GROUP_ALLIANCE_FLAG : SPAWN_GROUP_HORDE_FLAG; }
    static constexpr uint32 DroppedFlagEntry(TeamId flagTeam) { return flagTeam == TEAM_ALLIANCE ? GO_SILVERWING_FLAG_DROPPED : GO_WARSONG_FLAG_DROPPED; }
    static constexpr Team TeamOf(TeamId teamId) { return teamId == TEAM_ALLIANCE ? ALLIANCE : HORDE; }
    static constexpr TeamId OtherTeam(TeamId teamId) { return teamId == TEAM_ALLIANCE ? TEAM_HORDE : TEAM_ALLIANCE; }
}

struct battleground_classic_warsong_gulch : BattlegroundScript
{
    explicit battleground_classic_warsong_gulch(BattlegroundMap* map) : BattlegroundScript(map)
    {
        _isWeekend = sBattlegroundMgr->IsBGWeekend(battleground->GetTypeID());
        _reputationCapture = _isWeekend ? 45 : 35;
    }

    void OnInit() override
    {
        using namespace ClassicWarsongGulch;
        UpdateWorldState(WS_CAPTURES_MAX, MaxTeamScore);
        UpdateWorldState(WS_CAPTURES_MAX_NEW, MaxTeamScore);
        UpdateWorldState(WS_CAPTURES_ALLIANCE, 0);
        UpdateWorldState(WS_CAPTURES_HORDE, 0);
        UpdateWorldState(WS_FLAG_TAKEN_ALLIANCE, 0);
        UpdateWorldState(WS_FLAG_TAKEN_HORDE, 0);
        UpdateWorldState(WS_FLAG_STATE_ALLIANCE, 1);
        UpdateWorldState(WS_FLAG_STATE_HORDE, 1);
        UpdateWorldState(WS_BATTLE_IN_PROGRESS, 0, true);

        // manual spawn groups: a despawned group stays despawned
        battlegroundMap->SpawnGroupSpawn(SPAWN_GROUP_ALLIANCE_FLAG, true);
        battlegroundMap->SpawnGroupSpawn(SPAWN_GROUP_HORDE_FLAG, true);
        battlegroundMap->SpawnGroupSpawn(SPAWN_GROUP_GHOST_GATES, true);
    }

    void OnStart() override
    {
        using namespace ClassicWarsongGulch;
        for (ObjectGuid const& guid : _doors)
            if (GameObject* door = battlegroundMap->GetGameObject(guid))
            {
                door->UseDoorOrButton();
                door->DespawnOrUnsummon(3s);
            }

        battlegroundMap->SpawnGroupDespawn(SPAWN_GROUP_GHOST_GATES);
        battlegroundMap->SpawnGroupSpawn(SPAWN_GROUP_SPIRIT_GUIDES, true);
        UpdateWorldState(WS_BATTLE_IN_PROGRESS, 1, true);
    }

    void OnGameObjectCreate(GameObject* gameObject) override
    {
        if (gameObject->GetEntry() >= ClassicWarsongGulch::GO_DOOR_FIRST && gameObject->GetEntry() <= ClassicWarsongGulch::GO_DOOR_LAST)
            _doors.insert(gameObject->GetGUID());
    }

    void OnUpdate(uint32 diff) override
    {
        using namespace ClassicWarsongGulch;
        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return;

        Milliseconds const elapsed(diff);

        if (_respawnTimer > 0ms)
        {
            _respawnTimer -= elapsed;
            if (_respawnTimer <= 0ms)
                RespawnFlagsAfterCapture();
        }

        for (TeamId flagTeam : { TEAM_ALLIANCE, TEAM_HORDE })
        {
            switch (_flagStates[flagTeam])
            {
                case WsgFlagState::OnGround:
                    _dropTimers[flagTeam] -= elapsed;
                    if (_dropTimers[flagTeam] <= 0ms)
                        RespawnFlagAfterDrop(flagTeam);
                    break;
                case WsgFlagState::OnPlayer:
                {
                    Player* carrier = ObjectAccessor::GetPlayer(battlegroundMap, _flagKeepers[flagTeam]);
                    // carrier left the battleground / lost the aura without the drop handler: back to base (VMaNGOS RemovePlayer)
                    if (!carrier || !carrier->HasAura(CarrierSpell(flagTeam)))
                    {
                        battleground->RemovePlayerPosition(_flagKeepers[flagTeam]);
                        _flagKeepers[flagTeam].Clear();
                        PlaceFlagInBase(flagTeam);
                        UpdateFlagState(OtherTeam(flagTeam), 1);
                        break;
                    }

                    // capture: carrier at his own flag stand while his own flag is in base
                    TeamId const carrierTeam = OtherTeam(flagTeam);
                    if (_flagStates[carrierTeam] == WsgFlagState::OnBase && carrier->IsAlive()
                        && carrier->GetExactDist2d(CapturePoints[carrierTeam]) <= CaptureRadius
                        && std::abs(carrier->GetPositionZ() - CapturePoints[carrierTeam].GetPositionZ()) <= CaptureRadius)
                        CaptureFlag(carrier, carrierTeam);
                    break;
                }
                default:
                    break;
            }

            if (battleground->GetStatus() != STATUS_IN_PROGRESS)
                return;
        }
    }

    void DoAction(uint32 actionId, WorldObject* source, WorldObject* target) override
    {
        using namespace ClassicWarsongGulch;
        Player* player = Object::ToPlayer(source);
        if (!player)
            return;

        switch (actionId)
        {
            case ACTION_FLAG_CLICKED:
                if (GameObject* flag = Object::ToGameObject(target))
                    HandleFlagClick(player, flag);
                break;
            case ACTION_FLAG_AURA_REMOVED:
                HandleFlagDrop(player);
                break;
            default:
                break;
        }
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
    void HandleFlagClick(Player* player, GameObject* flag)
    {
        using namespace ClassicWarsongGulch;
        if (battleground->GetStatus() != STATUS_IN_PROGRESS || !player->CanUseBattlegroundObject(nullptr))
            return;

        Team const team = battleground->GetPlayerTeam(player->GetGUID());
        if (team != ALLIANCE && team != HORDE)
            return;

        TeamId const playerTeam = Battleground::GetTeamIndexByTeamId(team);
        switch (flag->GetEntry())
        {
            case GO_SILVERWING_FLAG:
                if (playerTeam == TEAM_HORDE && _flagStates[TEAM_ALLIANCE] == WsgFlagState::OnBase)
                    PickUpFlag(TEAM_ALLIANCE, player, true);
                break;
            case GO_WARSONG_FLAG:
                if (playerTeam == TEAM_ALLIANCE && _flagStates[TEAM_HORDE] == WsgFlagState::OnBase)
                    PickUpFlag(TEAM_HORDE, player, true);
                break;
            case GO_SILVERWING_FLAG_DROPPED:
            case GO_WARSONG_FLAG_DROPPED:
            {
                TeamId const flagTeam = flag->GetEntry() == GO_SILVERWING_FLAG_DROPPED ? TEAM_ALLIANCE : TEAM_HORDE;
                if (_flagStates[flagTeam] != WsgFlagState::OnGround || flag->GetGUID() != _droppedFlags[flagTeam] || !player->IsWithinDistInMap(flag, 10.0f))
                    break;

                if (playerTeam == flagTeam)
                    ReturnFlag(flagTeam, player);
                else
                    PickUpFlag(flagTeam, player, false);
                break;
            }
            default:
                break;
        }
    }

    void PickUpFlag(TeamId flagTeam, Player* player, bool fromBase)
    {
        using namespace ClassicWarsongGulch;
        if (fromBase)
            battlegroundMap->SpawnGroupDespawn(FlagSpawnGroup(flagTeam));
        else
            RemoveDroppedFlag(flagTeam);

        _flagKeepers[flagTeam] = player->GetGUID();
        _flagStates[flagTeam] = WsgFlagState::OnPlayer;

        player->RemoveAurasByType(SPELL_AURA_MOD_STEALTH);
        player->RemoveAurasByType(SPELL_AURA_MOD_INVISIBILITY);
        player->CastSpell(player, CarrierSpell(flagTeam), true);

        // the carrier icon on the map (SMSG_BATTLEGROUND_PLAYER_POSITIONS): the classic carrier spells do not carry the position aura
        // in this client's spell data, so the script registers the carrier
        WorldPackets::Battleground::BattlegroundPlayerPosition position;
        position.Guid = player->GetGUID();
        position.Pos = player->GetPosition();
        position.IconID = flagTeam == TEAM_ALLIANCE ? PLAYER_POSITION_ICON_ALLIANCE_FLAG : PLAYER_POSITION_ICON_HORDE_FLAG;
        position.ArenaSlot = PLAYER_POSITION_ARENA_SLOT_NONE;   // the client draws a battleground carrier only for slot 1
        battleground->AddPlayerPosition(position);

        if (flagTeam == TEAM_ALLIANCE)
        {
            battleground->SendBroadcastText(TEXT_ALLIANCE_FLAG_PICKED, CHAT_MSG_BG_SYSTEM_HORDE, player);
            battleground->PlaySoundToAll(SOUND_ALLIANCE_FLAG_PICKED_UP);
            UpdateWorldState(WS_FLAG_TAKEN_ALLIANCE, 1);
        }
        else
        {
            battleground->SendBroadcastText(TEXT_HORDE_FLAG_PICKED, CHAT_MSG_BG_SYSTEM_ALLIANCE, player);
            battleground->PlaySoundToAll(SOUND_HORDE_FLAG_PICKED_UP);
            UpdateWorldState(WS_FLAG_TAKEN_HORDE, 1);
        }

        UpdateFlagState(OtherTeam(flagTeam), 2);
    }

    void HandleFlagDrop(Player* player)
    {
        using namespace ClassicWarsongGulch;
        TeamId flagTeam = TEAM_NEUTRAL;
        for (TeamId team : { TEAM_ALLIANCE, TEAM_HORDE })
            if (_flagStates[team] == WsgFlagState::OnPlayer && _flagKeepers[team] == player->GetGUID())
                flagTeam = team;

        if (flagTeam == TEAM_NEUTRAL)                       // not a carrier (capture clears the keeper before removing the aura)
            return;

        _flagKeepers[flagTeam].Clear();
        battleground->RemovePlayerPosition(player->GetGUID());
        UpdateFlagState(OtherTeam(flagTeam), 1);

        if (battleground->GetStatus() != STATUS_IN_PROGRESS || player->GetMap() != battlegroundMap)
        {
            PlaceFlagInBase(flagTeam);
            return;
        }

        if (GameObject* dropped = GameObject::CreateGameObject(DroppedFlagEntry(flagTeam), battlegroundMap, player->GetPosition(),
            QuaternionData::fromEulerAnglesZYX(player->GetOrientation(), 0.f, 0.f), 255, GO_STATE_READY))
        {
            dropped->SetSpawnedByDefault(false);
            dropped->SetRespawnTime(int32(std::chrono::duration_cast<Seconds>(FlagDropTime).count()) + 5);
            if (battlegroundMap->AddToMap(dropped))
                _droppedFlags[flagTeam] = dropped->GetGUID();
            else
                delete dropped;
        }

        if (_droppedFlags[flagTeam].IsEmpty())              // could not place it, return it to base instead
        {
            PlaceFlagInBase(flagTeam);
            return;
        }

        _flagStates[flagTeam] = WsgFlagState::OnGround;
        _dropTimers[flagTeam] = FlagDropTime;

        if (flagTeam == TEAM_ALLIANCE)
        {
            battleground->SendBroadcastText(TEXT_ALLIANCE_FLAG_DROPPED, CHAT_MSG_BG_SYSTEM_ALLIANCE, player);
            UpdateWorldState(WS_FLAG_TAKEN_ALLIANCE, -1);
        }
        else
        {
            battleground->SendBroadcastText(TEXT_HORDE_FLAG_DROPPED, CHAT_MSG_BG_SYSTEM_HORDE, player);
            UpdateWorldState(WS_FLAG_TAKEN_HORDE, -1);
        }
    }

    void ReturnFlag(TeamId flagTeam, Player* player)
    {
        using namespace ClassicWarsongGulch;
        PlaceFlagInBase(flagTeam);
        battleground->SendBroadcastText(flagTeam == TEAM_ALLIANCE ? TEXT_ALLIANCE_FLAG_RETURNED : TEXT_HORDE_FLAG_RETURNED,
            flagTeam == TEAM_ALLIANCE ? CHAT_MSG_BG_SYSTEM_ALLIANCE : CHAT_MSG_BG_SYSTEM_HORDE, player);
        battleground->PlaySoundToAll(SOUND_FLAG_RETURNED);
        battleground->UpdatePvpStat(player, PVP_STAT_FLAG_RETURNS, 1);
    }

    void RespawnFlagAfterDrop(TeamId flagTeam)
    {
        using namespace ClassicWarsongGulch;
        PlaceFlagInBase(flagTeam);
        battleground->SendBroadcastText(flagTeam == TEAM_ALLIANCE ? TEXT_ALLIANCE_FLAG_RESPAWN : TEXT_HORDE_FLAG_RESPAWN, CHAT_MSG_BG_SYSTEM_NEUTRAL);
        battleground->PlaySoundToAll(SOUND_FLAGS_RESPAWNED);
    }

    void PlaceFlagInBase(TeamId flagTeam)
    {
        using namespace ClassicWarsongGulch;
        RemoveDroppedFlag(flagTeam);
        _flagStates[flagTeam] = WsgFlagState::OnBase;
        battlegroundMap->SpawnGroupSpawn(FlagSpawnGroup(flagTeam), true);
        UpdateWorldState(flagTeam == TEAM_ALLIANCE ? WS_FLAG_TAKEN_ALLIANCE : WS_FLAG_TAKEN_HORDE, 0);
    }

    void RemoveDroppedFlag(TeamId flagTeam)
    {
        if (GameObject* dropped = battlegroundMap->GetGameObject(_droppedFlags[flagTeam]))
            dropped->Delete();
        _droppedFlags[flagTeam].Clear();
    }

    void CaptureFlag(Player* player, TeamId carrierTeam)
    {
        using namespace ClassicWarsongGulch;
        TeamId const flagTeam = OtherTeam(carrierTeam);
        Team const team = TeamOf(carrierTeam);

        // keeper cleared before the aura goes, so the aura removal is not handled as a drop
        _flagKeepers[flagTeam].Clear();
        battleground->RemovePlayerPosition(player->GetGUID());
        player->RemoveAurasDueToSpell(CarrierSpell(flagTeam));

        // both flags leave the field until they are placed again
        _flagStates[flagTeam] = WsgFlagState::WaitRespawn;
        _flagStates[carrierTeam] = WsgFlagState::WaitRespawn;
        battlegroundMap->SpawnGroupDespawn(FlagSpawnGroup(carrierTeam));
        UpdateWorldState(flagTeam == TEAM_ALLIANCE ? WS_FLAG_TAKEN_ALLIANCE : WS_FLAG_TAKEN_HORDE, 0);

        if (battleground->GetTeamScore(carrierTeam) < MaxTeamScore)
            battleground->AddPoint(team, 1);

        if (team == ALLIANCE)
        {
            battleground->SendBroadcastText(TEXT_CAPTURED_HORDE_FLAG, CHAT_MSG_BG_SYSTEM_ALLIANCE, player);
            battleground->PlaySoundToAll(SOUND_FLAG_CAPTURED_ALLIANCE);
            battleground->RewardReputationToTeam(890, _reputationCapture, ALLIANCE);
        }
        else
        {
            battleground->SendBroadcastText(TEXT_CAPTURED_ALLIANCE_FLAG, CHAT_MSG_BG_SYSTEM_HORDE, player);
            battleground->PlaySoundToAll(SOUND_FLAG_CAPTURED_HORDE);
            battleground->RewardReputationToTeam(889, _reputationCapture, HORDE);
        }

        // vanilla: flag capture bonus honor = 2 honorable kills for the whole team
        battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(2), team);

        UpdateFlagState(carrierTeam, 1);
        UpdateWorldState(carrierTeam == TEAM_ALLIANCE ? WS_CAPTURES_ALLIANCE : WS_CAPTURES_HORDE, battleground->GetTeamScore(carrierTeam));
        battleground->UpdatePvpStat(player, PVP_STAT_FLAG_CAPTURES, 1);

        if (battleground->GetTeamScore(carrierTeam) >= MaxTeamScore)
        {
            UpdateWorldState(WS_FLAG_STATE_ALLIANCE, 1);
            UpdateWorldState(WS_FLAG_STATE_HORDE, 1);
            EndBattle(team);
            return;
        }

        _respawnTimer = FlagRespawnTime;
    }

    void RespawnFlagsAfterCapture()
    {
        using namespace ClassicWarsongGulch;
        for (TeamId flagTeam : { TEAM_ALLIANCE, TEAM_HORDE })
            if (_flagStates[flagTeam] == WsgFlagState::WaitRespawn)
                PlaceFlagInBase(flagTeam);

        battleground->SendBroadcastText(TEXT_FLAGS_PLACED, CHAT_MSG_BG_SYSTEM_NEUTRAL);
        battleground->PlaySoundToAll(SOUND_FLAGS_RESPAWNED);
    }

    // carrierTeam: the team holding the enemy flag; value 2 = carried, 1 = not carried
    void UpdateFlagState(TeamId carrierTeam, int32 value)
    {
        using namespace ClassicWarsongGulch;
        UpdateWorldState(carrierTeam == TEAM_ALLIANCE ? WS_FLAG_STATE_ALLIANCE : WS_FLAG_STATE_HORDE, value);
    }

    void EndBattle(Team winner)
    {
        if (battleground->GetStatus() != STATUS_IN_PROGRESS)
            return;

        // vanilla (VMaNGOS BattleGroundWS::EndBattleGround): winner 1 honorable kill (+2 on the holiday weekend),
        // weekend completion bonus 3 honorable kills for both teams
        if (_isWeekend)
        {
            battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(3), ALLIANCE);
            battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(3), HORDE);
        }
        if (winner == ALLIANCE || winner == HORDE)
            battleground->RewardHonorToTeam(battleground->GetBonusHonorFromKill(_isWeekend ? 3 : 1), winner);

        battleground->EndBattleground(winner);
    }

    static constexpr uint32 PVP_STAT_FLAG_CAPTURES = 928;
    static constexpr uint32 PVP_STAT_FLAG_RETURNS = 929;

    std::array<ClassicWarsongGulch::WsgFlagState, PVP_TEAMS_COUNT> _flagStates = { { ClassicWarsongGulch::WsgFlagState::OnBase, ClassicWarsongGulch::WsgFlagState::OnBase } };
    std::array<ObjectGuid, PVP_TEAMS_COUNT> _flagKeepers;
    std::array<ObjectGuid, PVP_TEAMS_COUNT> _droppedFlags;
    std::array<Milliseconds, PVP_TEAMS_COUNT> _dropTimers = { { 0ms, 0ms } };
    Milliseconds _respawnTimer = 0ms;
    GuidSet _doors;
    bool _isWeekend = false;
    uint32 _reputationCapture = 35;
};

// Classic 1.60: 179830 / 179831 flag stands, 179785 / 179786 dropped flags on map 489
struct go_classic_wsg_flag : GameObjectAI
{
    explicit go_classic_wsg_flag(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        if (me->GetMapId() != ClassicWarsongGulch::MapId)
            return false;

        if (ZoneScript* zoneScript = me->GetZoneScript())
            zoneScript->DoAction(ClassicWarsongGulch::ACTION_FLAG_CLICKED, player, me);

        return true;
    }
};

// Classic 1.60: 23333 - Warsong Flag, 23335 - Silverwing Flag (flag carried on map 489)
class spell_classic_wsg_flag_carrier : public AuraScript
{
    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/) const
    {
        Unit* target = GetTarget();
        if (!target->IsPlayer() || target->GetMapId() != ClassicWarsongGulch::MapId)
            return;

        if (ZoneScript* zoneScript = target->FindZoneScript())
            zoneScript->DoAction(ClassicWarsongGulch::ACTION_FLAG_AURA_REMOVED, target, target);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_classic_wsg_flag_carrier::AfterRemove, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_battleground_warsong_gulch()
{
    RegisterBattlegroundMapScript(battleground_warsong_gulch, 2106);

    // Classic 1.60
    RegisterBattlegroundMapScript(battleground_classic_warsong_gulch, ClassicWarsongGulch::MapId);
    RegisterGameObjectAI(go_classic_wsg_flag);
    RegisterSpellScript(spell_classic_wsg_flag_carrier);
}
