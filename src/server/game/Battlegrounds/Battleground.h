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

#ifndef __BATTLEGROUND_H
#define __BATTLEGROUND_H

#include "DBCEnums.h"
#include "Duration.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include "Position.h"
#include "SharedDefines.h"
#include "UniqueTrackablePtr.h"
#include "ZoneScript.h"
#include <deque>
#include <map>
#include <vector>

class BattlegroundMap;
class Creature;
class GameObject;
class Group;
class Player;
class Transport;
class Unit;
class WorldObject;
class WorldPacket;
struct BattlegroundPlayerScoreTemplate;
struct BattlegroundScore;
struct BattlegroundTemplate;
struct PVPDifficultyEntry;
struct WorldSafeLocsEntry;
enum class HonorGainSource : uint8;

namespace WorldPackets
{
    namespace Battleground
    {
        struct PVPMatchStatistics;
        struct BattlegroundPlayerPosition;
    }

}

enum BattlegroundBroadcastTexts
{
    BG_TEXT_ALLIANCE_WINS       = 10633,
    BG_TEXT_HORDE_WINS          = 10634,

    BG_TEXT_START_TWO_MINUTES   = 18193,
    BG_TEXT_START_ONE_MINUTE    = 18194,
    BG_TEXT_START_HALF_MINUTE   = 18195,
    BG_TEXT_BATTLE_HAS_BEGUN    = 18196,
};

enum BattlegroundSounds
{
    SOUND_HORDE_WINS                = 8454,
    SOUND_ALLIANCE_WINS             = 8455,
    SOUND_BG_START                  = 3439,
    SOUND_BG_START_L70ETC           = 11803
};

enum BattlegroundQuests
{
    SPELL_WS_QUEST_REWARD           = 43483,
    SPELL_AB_QUEST_REWARD           = 43484,
    SPELL_AV_QUEST_REWARD           = 43475,
    SPELL_AV_QUEST_KILLED_BOSS      = 23658,
    SPELL_EY_QUEST_REWARD           = 43477,
    SPELL_SA_QUEST_REWARD           = 61213,
    SPELL_AB_QUEST_REWARD_4_BASES   = 24061,
    SPELL_AB_QUEST_REWARD_5_BASES   = 24064
};

enum BattlegroundMarksCount
{
    ITEM_WINNER_COUNT               = 3,
    ITEM_LOSER_COUNT                = 1
};

enum BattlegroundCreatures
{
    BG_CREATURE_ENTRY_A_SPIRITGUIDE      = 13116,           // alliance
    BG_CREATURE_ENTRY_H_SPIRITGUIDE      = 13117            // horde
};

enum BattlegroundSpells
{
    SPELL_SPIRIT_HEAL_CHANNEL_AOE   = 22011,                // used for AoE resurrections
    SPELL_SPIRIT_HEAL_PLAYER_AURA   = 156758,               // individual player timers for resurrection
    SPELL_SPIRIT_HEAL_CHANNEL_SELF  = 305122,               // channel visual for individual area spirit healers
    SPELL_WAITING_FOR_RESURRECT     = 2584,                 // Waiting to Resurrect
    VISUAL_SPIRIT_HEAL_CHANNEL      = 3060,
    SPELL_SPIRIT_HEAL               = 22012,                // Spirit Heal
    SPELL_RESURRECTION_VISUAL       = 24171,                // Resurrection Impact Visual
    SPELL_ARENA_PREPARATION         = 32727,                // use this one, 32728 not correct
    SPELL_PREPARATION               = 44521,                // Preparation
    SPELL_SPIRIT_HEAL_MANA          = 44535,                // Spirit Heal
    SPELL_RECENTLY_DROPPED_ALLIANCE_FLAG = 42792,           // makes Alliance flag unselectable
    SPELL_RECENTLY_DROPPED_HORDE_FLAG = 50326,              // makes Horde flag unselectable
    SPELL_RECENTLY_DROPPED_NEUTRAL_FLAG = 50327,            // makes Neutral flag unselectable
    SPELL_AURA_PLAYER_INACTIVE      = 43681,                // Inactive
    SPELL_HONORABLE_DEFENDER_25Y    = 68652,                // +50% honor when standing at a capture point that you control, 25yards radius (added in 3.2)
    SPELL_HONORABLE_DEFENDER_60Y    = 66157,                // +50% honor when standing at a capture point that you control, 60yards radius (added in 3.2), probably for 40+ player battlegrounds
    SPELL_MERCENARY_CONTRACT_HORDE  = 193472,
    SPELL_MERCENARY_CONTRACT_ALLIANCE = 193475,
    SPELL_MERCENARY_HORDE_1         = 193864,
    SPELL_MERCENARY_HORDE_REACTIONS = 195838,
    SPELL_MERCENARY_ALLIANCE_1      = 193863,
    SPELL_MERCENARY_ALLIANCE_REACTIONS = 195843,
    SPELL_MERCENARY_SHAPESHIFT      = 193970,
    SPELL_PET_SUMMONED              = 6962 // used after resurrection
};

enum BattlegroundTimeIntervals
{
    CHECK_PLAYER_POSITION_INVERVAL  = 1000,                 // ms
    RESURRECTION_INTERVAL           = 30000,                // ms
    //REMIND_INTERVAL                 = 10000,                // ms
    INVITATION_REMIND_TIME          = 20000,                // ms
    INVITE_ACCEPT_WAIT_TIME         = 90000,                // ms
    TIME_AUTOCLOSE_BATTLEGROUND     = 120000,               // ms
    MAX_OFFLINE_TIME                = 300,                  // secs
    RESPAWN_ONE_DAY                 = 86400,                // secs
    RESPAWN_IMMEDIATELY             = 0,                    // secs
    BUFF_RESPAWN_TIME               = 180,                  // secs
    BATTLEGROUND_COUNTDOWN_MAX      = 120,                  // secs
    ARENA_COUNTDOWN_MAX             = 60,                   // secs
    PLAYER_POSITION_UPDATE_INTERVAL = 5000,                 // ms
};

enum BattlegroundStartTimeIntervals
{
    BG_START_DELAY_2M               = 120000,               // ms (2 minutes)
    BG_START_DELAY_1M               = 60000,                // ms (1 minute)
    BG_START_DELAY_30S              = 30000,                // ms (30 seconds)
    BG_START_DELAY_15S              = 15000,                // ms (15 seconds) Used only in arena
    BG_START_DELAY_NONE             = 0                     // ms
};

enum BattlegroundBuffObjects
{
    BG_OBJECTID_SPEEDBUFF_ENTRY     = 179871,
    BG_OBJECTID_REGENBUFF_ENTRY     = 179904,
    BG_OBJECTID_BERSERKERBUFF_ENTRY = 179905
};

uint32 const Buff_Entries[3] = { BG_OBJECTID_SPEEDBUFF_ENTRY, BG_OBJECTID_REGENBUFF_ENTRY, BG_OBJECTID_BERSERKERBUFF_ENTRY };

enum BattlegroundStatus
{
    STATUS_NONE         = 0,                                // first status, should mean bg is not instance
    STATUS_WAIT_QUEUE   = 1,                                // means bg is empty and waiting for queue
    STATUS_WAIT_JOIN    = 2,                                // this means, that BG has already started and it is waiting for more players
    STATUS_IN_PROGRESS  = 3,                                // means bg is running
    STATUS_WAIT_LEAVE   = 4                                 // means some faction has won BG and it is ending
};

struct BattlegroundPlayer
{
    time_t OfflineRemoveTime;                              // for tracking and removing offline players from queue after 5 minutes
    ::Team Team;                                           // Player's team
    bool Mercenary;
    BattlegroundQueueTypeId queueTypeId;
};

struct BattlegroundObjectInfo
{
    BattlegroundObjectInfo() : object(nullptr), timer(0), spellid(0) { }

    GameObject  *object;
    int32       timer;
    uint32      spellid;
};

enum ArenaType
{
    ARENA_TYPE_2v2          = 2,
    ARENA_TYPE_3v3          = 3,
    ARENA_TYPE_5v5          = 5
};

enum BattlegroundStartingEvents
{
    BG_STARTING_EVENT_NONE  = 0x00,
    BG_STARTING_EVENT_1     = 0x01,
    BG_STARTING_EVENT_2     = 0x02,
    BG_STARTING_EVENT_3     = 0x04,
    BG_STARTING_EVENT_4     = 0x08
};

enum BattlegroundStartingEventsIds
{
    BG_STARTING_EVENT_FIRST     = 0,
    BG_STARTING_EVENT_SECOND    = 1,
    BG_STARTING_EVENT_THIRD     = 2,
    BG_STARTING_EVENT_FOURTH    = 3
};
#define BG_STARTING_EVENT_COUNT 4

#define BG_AWARD_ARENA_POINTS_MIN_LEVEL 71
#define ARENA_TIMELIMIT_POINTS_LOSS    -16

enum BattlegroundPlayerPositionConstants
{
    PLAYER_POSITION_ICON_NONE           = 0,
    PLAYER_POSITION_ICON_HORDE_FLAG     = 1,
    PLAYER_POSITION_ICON_ALLIANCE_FLAG  = 2,

    PLAYER_POSITION_ARENA_SLOT_NONE     = 1,
    PLAYER_POSITION_ARENA_SLOT_1        = 2,
    PLAYER_POSITION_ARENA_SLOT_2        = 3,
    PLAYER_POSITION_ARENA_SLOT_3        = 4,
    PLAYER_POSITION_ARENA_SLOT_4        = 5,
    PLAYER_POSITION_ARENA_SLOT_5        = 6
};

enum class BattlegroundQueueIdType : uint8
{
    Battleground        = 0,
    Arena               = 1,
    Wargame             = 2,
    Cheat               = 3,
    ArenaSkirmish       = 4,
    SoloShuffle         = 6,
    RatedSoloShuffle    = 7,
    SoloRBG             = 8,
    RatedSoloRBG        = 9
};

enum class BattlegroundPointCaptureStatus
{
    AllianceControlled,
    AllianceCapturing,
    Neutral,
    HordeCapturing,
    HordeControlled
};

/*
This class is used to:
1. Add player to battleground
2. Remove player from battleground
3. some certain cases, same for all battlegrounds
4. It has properties same for all battlegrounds
*/
class TC_GAME_API Battleground
{
    public:
        Battleground(BattlegroundTemplate const* battlegroundTemplate);
        Battleground(Battleground const&);
        virtual ~Battleground();

        Battleground& operator=(Battleground const&) = delete;

        void Update(uint32 diff);

        void Reset();

        /* Battleground */
        // Get methods:
        char const* GetName() const;
        BattlegroundQueueTypeId GetQueueID() const { return m_QueueID; }
        BattlegroundTypeId GetTypeID() const;
        BattlegroundBracketId GetBracketId() const;
        uint32 GetInstanceID() const        { return m_InstanceID; }
        BattlegroundStatus GetStatus() const { return m_Status; }
        uint32 GetClientInstanceID() const  { return m_ClientInstanceID; }
        uint32 GetElapsedTime() const       { return m_StartTime; }
        Milliseconds GetInProgressDuration() const { return (m_Events & BG_STARTING_EVENT_4) ? Milliseconds(m_StartTime - StartDelayTimes[BG_STARTING_EVENT_FIRST]) : 0ms; }
        uint32 GetRemainingTime() const     { return m_EndTime; }
        uint32 GetMaxPlayers() const;
        uint32 GetMinPlayers() const;

        uint32 GetMinLevel() const;
        uint32 GetMaxLevel() const;

        uint32 GetMaxPlayersPerTeam() const;
        uint32 GetMinPlayersPerTeam() const;

        int32 GetStartDelayTime() const     { return m_StartDelayTime; }
        uint8 GetArenaType() const          { return m_QueueID.TeamSize; }
        PvPTeamId GetWinner() const { return _winnerTeamId; }
        uint32 GetScriptId() const;
        uint32 GetBonusHonorFromKill(uint32 kills) const;

        // Set methods:
        //here we can count minlevel and maxlevel for players
        void SetQueueId(BattlegroundQueueTypeId queueId) { m_QueueID = queueId; }
        void SetBracket(PVPDifficultyEntry const* bracketEntry);
        void SetInstanceID(uint32 InstanceID) { m_InstanceID = InstanceID; }
        void SetStatus(BattlegroundStatus Status) { m_Status = Status; }
        void SetClientInstanceID(uint32 InstanceID) { m_ClientInstanceID = InstanceID; }
        void SetElapsedTime(uint32 Time)        { m_StartTime = Time; }
        void SetRemainingTime(uint32 Time)      { m_EndTime = Time; }
        void SetWinner(PvPTeamId winnerTeamId) { _winnerTeamId = winnerTeamId; }
        std::unordered_set<uint32> const* GetPvpStatIds() const { return _pvpStatIds; }

        void ModifyStartDelayTime(int diff) { m_StartDelayTime -= diff; }
        void SetStartDelayTime(int Time)    { m_StartDelayTime = Time; }

        void AddToBGFreeSlotQueue();                        //this queue will be useful when more battlegrounds instances will be available
        void RemoveFromBGFreeSlotQueue();                   //this method could delete whole BG instance, if another free is available
        void RemoveFromBGFreeSlotQueueOnShutdown() { m_InBGFreeSlotQueue = false; }

        // a player who entered without being invited counts as invited from then on (his leaving decrements it)
        void RaiseInvitedCountTo(Team team, uint32 n) { uint32& c = (team == ALLIANCE) ? m_InvitedAlliance : m_InvitedHorde; if (c < n) c = n; }
        // never below zero: a player who entered without a queue invitation was never counted, a wrapped counter closes the team for good
        void DecreaseInvitedCount(Team team)      { uint32& invited = (team == ALLIANCE) ? m_InvitedAlliance : m_InvitedHorde; invited = AfterLeave(invited); }
        static constexpr uint32 AfterLeave(uint32 invited) { return invited ? invited - 1 : 0; }
        void IncreaseInvitedCount(Team team)      { (team == ALLIANCE) ? ++m_InvitedAlliance : ++m_InvitedHorde; }

        uint32 GetInvitedCount(Team team) const   { return (team == ALLIANCE) ? m_InvitedAlliance : m_InvitedHorde; }
        bool HasFreeSlots() const;
        uint32 GetFreeSlotsForTeam(Team team) const;
        // The level of a bot created on demand for a bracket: always inside [bracketMin, bracketMax] (capped at the realm's top level), spread
        // over the bracket by `roll`, so a 10-19 queue is filled with level 10-19 bots, a twink queue as full as a level 60 one.
        // Without a bracket range the fallback level is used.
        static constexpr uint32 BracketLevelFor(uint32 bracketMin, uint32 bracketMax, uint32 realmCap, uint32 fallback, uint32 roll)
        {
            if (bracketMin == 0 || bracketMax < bracketMin) return fallback;
            uint32 const hi = bracketMax < realmCap ? bracketMax : realmCap;
            uint32 const lo = bracketMin < hi ? bracketMin : hi;
            return lo + roll % (hi - lo + 1);
        }
        // Where a real player is put when several instances of a battleground are running: the instance with the most real players first,
        // then the one with the most bots on the player's team (more bots to swap out). Fewer instances, more real players in each.
        static constexpr bool PlacementBetter(uint32 humansA, uint32 teamBotsA, uint32 humansB, uint32 teamBotsB)
        {
            return humansA != humansB ? humansA > humansB : teamBotsA > teamBotsB;
        }
        // Order in which live instances of one battleground draw from the queue (more real players first, then the emptiest)
        static constexpr bool FillsBefore(uint32 humansA, uint32 playersA, uint32 humansB, uint32 playersB)
        {
            return humansA != humansB ? humansA > humansB : playersA < playersB;
        }
        // The balance gate behind GetFreeSlotsForTeam: how many more of "this" team may join. Players already inside count as invited.
        static uint32 BalancedFreeSlots(uint32 thisTeamInvitedCount, uint32 otherTeamInvitedCount, uint32 thisTeamPlayersCount, uint32 otherTeamPlayersCount,
            uint32 maxPlayersPerTeam, uint32 minPlayersPerTeam);
        // How many bots a running or preparing match asks for per top-up cycle for one team. The balance gate only lets one more join at a
        // time when the sides are level, but the queue invites both sides in turn, so the whole shortfall is asked for. Nothing is asked
        // for a team that is full.
        // Places a team still has when the balance gate is ignored (a real player is waiting): team size minus those inside or invited.
        static constexpr uint32 PhysicalRoom(uint32 insideOrInvited, uint32 maxPlayersPerTeam)
        {
            return insideOrInvited < maxPlayersPerTeam ? maxPlayersPerTeam - insideOrInvited : 0u;
        }
        // Bots that must leave so `need` real players fit: the shortfall, plus whatever the team is already over its size.
        static constexpr uint32 PlacesToMake(uint32 taken, uint32 maxPlayersPerTeam, uint32 need)
        {
            return PhysicalRoom(taken, maxPlayersPerTeam) >= need ? 0u
                 : need - PhysicalRoom(taken, maxPlayersPerTeam) + (taken > maxPlayersPerTeam ? taken - maxPlayersPerTeam : 0u);
        }
        static constexpr uint32 TopUpRequest(uint32 thisTeamPlayers, uint32 maxPlayersPerTeam)
        {
            return thisTeamPlayers < maxPlayersPerTeam ? maxPlayersPerTeam - thisTeamPlayers : 0u;
        }
        // A waiting real player's team is full of bots but none can leave this moment (all in combat or carrying a flag): the player waits
        // for one to become free instead of a second instance being opened. False when the places can be made now, or when the team is
        // mostly real players and there is nobody to swap.
        static constexpr bool WaitForBotToFree(uint32 placesToMake, uint32 evictableNow, uint32 botsOnTeam)
        {
            return placesToMake > evictableNow && botsOnTeam >= placesToMake;
        }
        // The shortfall minus the bots an earlier top-up already created or reused for this side that have not queued yet (their setup is
        // still running). WSG 2026-10-21: one player's queue opened ten instances, because every 20 s prep tick asked again for places
        // the bots in setup were about to take, and the surplus formed new battlegrounds.
        static constexpr uint32 TopUpAfterInFlight(uint32 thisTeamPlayers, uint32 maxPlayersPerTeam, uint32 inFlight)
        {
            uint32 const request = TopUpRequest(thisTeamPlayers, maxPlayersPerTeam);
            return request > inFlight ? request - inFlight : 0u;
        }

        bool isArena() const;
        bool isBattleground() const;
        bool isRated() const        { return m_QueueID.Rated; }

        typedef std::map<ObjectGuid, BattlegroundPlayer> BattlegroundPlayerMap;
        BattlegroundPlayerMap const& GetPlayers() const { return m_Players; }
        uint32 GetPlayersSize() const { return uint32(m_Players.size()); }

        typedef std::map<ObjectGuid, BattlegroundScore*> BattlegroundScoreMap;
        uint32 GetPlayerScoresSize() const { return uint32(PlayerScores.size()); }

        void StartBattleground();

        GameObject* GetBGObject(uint32 type, bool logError = true);
        Creature* GetBGCreature(uint32 type, bool logError = true);

        // Location
        uint32 GetMapId() const;

        // Map pointers
        void SetBgMap(BattlegroundMap* map);
        BattlegroundMap* GetBgMap() const;
        BattlegroundMap* FindBgMap() const { return m_Map; }

        WorldSafeLocsEntry const* GetTeamStartPosition(TeamId teamId) const;

        // The closed start doors of a team, read from the spawned door objects (not from a position table): the centroid of the
        // doors that close the team's start room and the unit vector from it into the room. Valid only while at least one such
        // door is closed. Cached for 2 s per team; `searcher` must be near the team's start (a player of that team).
        struct StartGateInfo
        {
            bool Valid = false;
            float X = 0.0f, Y = 0.0f, Z = 0.0f;       // centroid of the closing doors
            float InX = 0.0f, InY = 0.0f;             // unit vector from the doors into the start room
            uint8 Doors = 0;
            uint32 StampMs = 0;
        };
        StartGateInfo const& GetStartGate(TeamId teamId, WorldObject const* searcher) const;

        // The single rule for mounting in a battleground: nullptr when a mount is allowed, otherwise the reason it is refused
        // (the start doors are closed / the area is indoors). Spell.cpp and the playerbot API both ask this.
        char const* GetMountRefusal(Player const* player) const;
        // Why the terrain data says "indoors" at the player's position (wmo group flags, the WMOAreaTable row that applied, the
        // area flags): the evidence for a wrong indoor answer. Diagnostics only; not cheap, call from throttled logs.
        std::string DescribeIndoors(Player const* player) const;
        // Classic.MountIndoorsRules: mounting is refused in an enclosed start area while the doors are closed (checkStartArea) and indoors
        bool IsMountRefused(Player const* player, bool checkStartArea) const;
        // Warsong Gulch starts in the closed flag rooms, the Arathi Basin and Alterac Valley start areas are open ground
        static constexpr bool IsEnclosedStartArea(uint32 mapId) { return mapId == 489; }
        // the client's indoor test on the MOGP flags of the wmo group: interior (0x2000) and not exterior (0x8)
        static constexpr bool IsIndoorWmoGroup(uint32 mogpFlags) { return (mogpFlags & 0x2000) != 0 && (mogpFlags & 0x8) == 0; }

        float GetStartMaxDist() const;

        // Packet Transfer
        // method that should fill worldpacket with actual world states (not yet implemented for all battlegrounds!)
        void SendPacketToTeam(Team team, WorldPacket const* packet, Player* except = nullptr) const;
        void SendPacketToAll(WorldPacket const* packet) const;

        void SendChatMessage(Creature* source, uint8 textId, WorldObject* target = nullptr);
        void SendBroadcastText(uint32 id, ChatMsg msgType, WorldObject const* target = nullptr);

        template<class Do>
        void BroadcastWorker(Do& _do);

        void PlaySoundToTeam(uint32 SoundID, Team team);
        void PlaySoundToAll(uint32 SoundID);
        void CastSpellOnTeam(uint32 SpellID, Team team);
        void RemoveAuraOnTeam(uint32 SpellID, Team team);
        void RewardHonorToTeam(uint32 Honor, Team team);
        void RewardReputationToTeam(uint32 faction_id, uint32 Reputation, Team team);
        void UpdateWorldState(int32 worldStateId, int32 value, bool hidden = false);
        virtual void EndBattleground(Team winner);
        void BlockMovement(Player* player);

        void SendMessageToAll(uint32 entry, ChatMsg type, Player const* source = nullptr);

        // Raid Group
        Group* GetBgRaid(Team team) const { return team == ALLIANCE ? m_BgRaids[TEAM_ALLIANCE] : m_BgRaids[TEAM_HORDE]; }
        void SetBgRaid(Team team, Group* bg_raid);

        virtual void BuildPvPLogDataPacket(WorldPackets::Battleground::PVPMatchStatistics& pvpLogData) const;

        BattlegroundScore const* GetBattlegroundScore(Player* player) const;

        bool UpdatePlayerScore(Player* player, uint32 type, uint32 value, bool doAddHonor = true, Optional<HonorGainSource> source = {});
        void UpdatePvpStat(Player* player, uint32 pvpStatId, uint32 value);

        static TeamId GetTeamIndexByTeamId(Team team) { return team == ALLIANCE ? TEAM_ALLIANCE : TEAM_HORDE; }
        uint32 GetPlayersCountByTeam(Team team) const { return m_PlayersCount[GetTeamIndexByTeamId(team)]; }
        uint32 GetAlivePlayersCountByTeam(Team team) const;   // used in arenas to correctly handle death in spirit of redemption / last stand etc. (killer = killed) cases
        void UpdatePlayersCountByTeam(Team team, bool remove)
        {
            if (remove)
                --m_PlayersCount[GetTeamIndexByTeamId(team)];
            else
                ++m_PlayersCount[GetTeamIndexByTeamId(team)];
        }

        virtual void CheckWinConditions() { }

        // used for rated arena battles
        void SetArenaMatchmakerRating(Team team, uint32 MMR){ m_ArenaTeamMMR[GetTeamIndexByTeamId(team)] = MMR; }
        uint32 GetArenaMatchmakerRating(Team team) const          { return m_ArenaTeamMMR[GetTeamIndexByTeamId(team)]; }

        // must be implemented in BG subclass if need AND call base class generic code
        virtual void HandleKillPlayer(Player* player, Player* killer);
        virtual void HandleKillUnit(Creature* /*creature*/, Unit* /*killer*/);

        // Battleground events
        void EventPlayerLoggedIn(Player* player);
        void EventPlayerLoggedOut(Player* player);

        virtual WorldSafeLocsEntry const* GetExploitTeleportLocation(Team /*team*/) { return nullptr; }
        // GetExploitTeleportLocation(TeamId) must be implemented in the battleground subclass.
        void TeleportPlayerToExploitLocation(Player* player);

        virtual void AddPlayer(Player* player, BattlegroundQueueTypeId queueId);                // must be implemented in BG subclass

        void AddOrSetPlayerToCorrectBgGroup(Player* player, Team team);

        virtual void RemovePlayerAtLeave(ObjectGuid guid, bool Transport, bool SendPacket);

        virtual bool HandlePlayerUnderMap(Player* /*player*/) { return false; }

        // since arenas can be AvA or Hvh, we have to get the "temporary" team of a player
        Team GetPlayerTeam(ObjectGuid guid) const;
        bool IsPlayerInBattleground(ObjectGuid guid) const;
        bool IsPlayerMercenaryInBattleground(ObjectGuid guid) const;

        bool ToBeDeleted() const { return m_SetDeleteThis; }
        void SetDeleteThis() { m_SetDeleteThis = true; }

        void RewardXPAtKill(Player* killer, Player* victim);
        bool CanAwardArenaPoints() const { return GetMinLevel() >= BG_AWARD_ARENA_POINTS_MIN_LEVEL; }

        uint32 GetTeamScore(TeamId teamId) const;

        Team GetPrematureWinner();

        // because BattleGrounds with different types and same level range has different m_BracketId
        uint8 GetUniqueBracketId() const;

        void AddPlayerPosition(WorldPackets::Battleground::BattlegroundPlayerPosition const& position);
        void RemovePlayerPosition(ObjectGuid guid);

        BattlegroundPlayer const* GetBattlegroundPlayerData(ObjectGuid const& playerGuid) const
        {
            auto itr = m_Players.find(playerGuid);
            if (itr == m_Players.end())
                return nullptr;

            return &itr->second;
        }

        void AddPoint(Team team, uint32 points = 1) { m_TeamScores[GetTeamIndexByTeamId(team)] += points; }
        void SetTeamPoint(Team team, uint32 points = 0) { m_TeamScores[GetTeamIndexByTeamId(team)] = points; }
        void RemovePoint(Team team, uint32 points = 1) { m_TeamScores[GetTeamIndexByTeamId(team)] -= points; }

        Trinity::unique_weak_ptr<Battleground> GetWeakPtr() const { return m_weakRef; }
        void SetWeakPtr(Trinity::unique_weak_ptr<Battleground> weakRef) { m_weakRef = std::move(weakRef); }

    protected:
        // this method is called, when BG cannot spawn its own spirit guide, or something is wrong, It correctly ends Battleground
        void EndNow();
        void PlayerAddedToBGCheckIfBGIsRunning(Player* player);

        Player* _GetPlayer(ObjectGuid guid, bool offlineRemove, char const* context) const;
        Player* _GetPlayer(BattlegroundPlayerMap::iterator itr, char const* context) { return _GetPlayer(itr->first, itr->second.OfflineRemoveTime != 0, context); }
        Player* _GetPlayer(BattlegroundPlayerMap::const_iterator itr, char const* context) const { return _GetPlayer(itr->first, itr->second.OfflineRemoveTime != 0, context); }
        Player* _GetPlayerForTeam(Team team, BattlegroundPlayerMap::const_iterator itr, char const* context) const;

        /* Pre- and post-update hooks */

        /**
         * @brief Pre-update hook.
         *
         * Will be called before battleground update is started. Depending on
         * the result of this call actual update body may be skipped.
         *
         * @param diff a time difference between two worldserver update loops in
         * milliseconds.
         *
         * @return @c true if update must be performed, @c false otherwise.
         *
         * @see Update(), PostUpdateImpl().
         */
        virtual bool PreUpdateImpl(uint32 /* diff */) { return true; }

        /**
         * @brief Post-update hook.
         *
         * Will be called after battleground update has passed. May be used to
         * implement custom update effects in subclasses.
         *
         * @param diff a time difference between two worldserver update loops in
         * milliseconds.
         *
         * @see Update(), PreUpdateImpl().
         */
        virtual void PostUpdateImpl(uint32 /* diff */) { }

        void _ProcessOfflineQueue();
        void _ProcessProgress(uint32 diff);
        void _ProcessLeave(uint32 diff);
        void _ProcessJoin(uint32 diff);
        void _CheckSafePositions(uint32 diff);
        void _ProcessPlayerPositionBroadcast(uint32 diff);

        // Scorekeeping
        BattlegroundScoreMap PlayerScores;                // Player scores
        // must be implemented in BG subclass
        virtual void RemovePlayer(Player* /*player*/, ObjectGuid /*guid*/, uint32 /*team*/) { }

        // Player lists, those need to be accessible by inherited classes
        BattlegroundPlayerMap m_Players;

        // these are important variables used for starting messages
        uint8 m_Events;
        BattlegroundStartTimeIntervals StartDelayTimes[BG_STARTING_EVENT_COUNT];
        // this must be filled in constructors!
        uint32 StartMessageIds[BG_STARTING_EVENT_COUNT];

        int32 m_TeamScores[PVP_TEAMS_COUNT];

    private:
        // Battleground
        BattlegroundQueueTypeId m_QueueID;
        uint32 m_InstanceID;                                // Battleground Instance's GUID!
        BattlegroundStatus m_Status;
        uint32 m_ClientInstanceID;                          // the instance-id which is sent to the client and without any other internal use
        uint32 m_StartTime;
        uint32 m_ResetStatTimer;
        uint32 m_ValidStartPositionTimer;
        int32 m_EndTime;                                    // it is set to 120000 when bg is ending and it decreases itself
        bool   m_InBGFreeSlotQueue;                         // used to make sure that BG is only once inserted into the BattlegroundMgr.BGFreeSlotQueue[bgTypeId] deque
        bool   m_SetDeleteThis;                             // used for safe deletion of the bg after end / all players leave

        PvPTeamId _winnerTeamId;
        int32  m_StartDelayTime;
        bool   m_PrematureCountDown;
        uint32 m_LastPlayerPositionBroadcast;

        // Player lists
        std::deque<ObjectGuid> m_OfflineQueue;              // Player GUID

        // Invited counters are useful for player invitation to BG - do not allow, if BG is started to one faction to have 2 more players than another faction
        // Invited counters will be changed only when removing already invited player from queue, removing player from battleground and inviting player to BG
        // Invited players counters
        uint32 m_InvitedAlliance;
        uint32 m_InvitedHorde;

        // Raid Group
        Group* m_BgRaids[PVP_TEAMS_COUNT];                   // 0 - alliance, 1 - horde

        // Players count by team
        uint32 m_PlayersCount[PVP_TEAMS_COUNT];

        uint32 m_ArenaTeamMMR[PVP_TEAMS_COUNT];

        // Start location
        BattlegroundMap* m_Map;
        mutable StartGateInfo _startGate[2];
        Position StartPosition[PVP_TEAMS_COUNT];

        BattlegroundTemplate const* _battlegroundTemplate;
        PVPDifficultyEntry const* _pvpDifficultyEntry;
        std::unordered_set<uint32> const* _pvpStatIds;

        std::vector<WorldPackets::Battleground::BattlegroundPlayerPosition> _playerPositions;

        // Time when the first message "the battle will begin in 2minutes" is send (or 1m for arenas)
        time_t _preparationStartTime;

        Trinity::unique_weak_ptr<Battleground> m_weakRef;
};
#endif
