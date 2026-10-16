// BotQueueFiller - Phase D of WORLD_POPULATION_PLAN.
//
// When a player joins a BG / LFG / LFR queue alone, this service fills the
// remaining slots with bots within seconds. Online bots are preferred;
// shortfall is JIT-spawned via BotCharacterFactory + the setup pipeline,
// then queued.
//
// Composition rules per queue kind:
//   5-man dungeon:  1 tank + 1 healer + 3 dps
//   10-man raid:    2 tanks + 3 healers + 5 dps
//   20-man raid:    2 tanks + 4 healers + 14 dps
//   BG (10/15/40):  faction-balanced; dps-heavy with ~10% T+H
//
// The fill is invoked by the Module on hook events (Hooks::OnPlayerJoined*).

#pragma once

#include "Bot/BotTypes.h"
#include <cstdint>

class Player;

namespace Playerbot::V2::Fleet {

class BotQueueFiller
{
public:
    enum class QueueKind : uint8 { Bg, Dungeon5, Raid10, Raid20 };

    struct FillRequest
    {
        QueueKind kind;
        uint8     bracket;       // level bracket — used when target_level_override is 0
        uint32    faction;       // ALLIANCE / HORDE
        uint32    instance_id;   // bg_type_id or dungeon_id
        Player*   requesting_player;
        // When true, Fill() queues only ALREADY-ONLINE bots — no JIT spawn.
        // The periodic BG top-up uses this so it doesn't keep spawning
        // fresh bots that won't complete setup before the 90s invite
        // window expires (manifesting as "not pressing enter" kicks).
        bool      online_only = false;
        // Explicit target level. When non-zero, Fill() uses this instead
        // of BracketMidpoint(bracket). Needed for LFG where the queueing
        // player may be at a level that doesn't map cleanly to a bracket
        // (L21 → bracket 2 → midpoint 35, but the dungeon is L17-24 and
        // bots at L30+ get LFG_LOCKSTATUS_TOO_HIGH_LEVEL rejections).
        uint8     target_level_override = 0;
        // Hard cap on total bots queued in this Fill (T+H+D). When 0,
        // the per-kind NeedsFor() ratio applies (e.g. 3T/3H/30D = 36
        // for BG). When > 0, Fill() exits as soon as the sum of queued
        // bots reaches max_total_bots — used by the periodic running-BG
        // top-up to inject EXACTLY (max_per_team - current_count) bots
        // per faction, no more. Without this, TopUp queued 36 candidates
        // per side every cycle and the team count overshot max_per_team
        // (observed: WSG with 13 alliance vs 7 horde in a 10v10).
        uint8     max_total_bots = 0;
        // BG only: fill req.faction alone. A top-up asks each side for its own deficit; the default (both sides, same count)
        // is for a player's queue, where both sides need the same roster.
        bool      single_faction = false;
        // BG-specific: the PVPDifficulty bracket's actual MIN/MAX levels
        // (from DB2). Bots outside this range fail BG queue with
        // `no_bracket` (PVPDifficultyEntry lookup returns null). Without
        // these, the filler used a coarse ±15-window around the target
        // level, which for AV at L14 pulled in L10-17 bots — none of
        // which AV's bracket data accepts. When non-zero (both fields),
        // the filler clamps eligibility strictly to [min, max].
        uint8     bracket_min_level = 0;
        uint8     bracket_max_level = 0;
        // Per-role overrides for the periodic LFG top-up. When non-zero,
        // Fill uses this T/H/D triple instead of NeedsFor(kind). Used by
        // Module::TopUpPendingLfg to request ONLY the deficit (e.g., a
        // queue with 1T+1D already in just needs 1H+2D more, not the
        // full 1T+1H+3D composition). A bare 0/0/0 means "use the kind's
        // default composition".
        uint8     needs_tank_override   = 0;
        uint8     needs_healer_override = 0;
        uint8     needs_dps_override    = 0;
    };

    // Run a fill pass for the queue request. Synchronous: queues online
    // bots immediately, kicks off async JIT-spawns in the background.
    void Fill(FillRequest const& req);

    // Notify the filler that a JIT-tagged bot's match has ended; will
    // log the bot out after retention window. Wired from the hook for
    // OnBgMatchEnd / OnLfgGroupComplete (when those land).
    void OnMatchEnd(Player* bot);

    // ---- Ambient BG participation (Playerbot.Bg.AutoJoin*, the owner's RandomBotAutoJoinBG) ----
    //
    // Both are world-thread passes driven from BotPopulationManager::OnWorldTick; each keeps
    // its own cadence and does nothing while disabled or with no bots online.
    //
    // AutoJoinTick: every 10 s, eligible idle pool bots (alive, ready, ungrouped, not in an
    // instance / BG / queue / LFG, not owner-bound, no deserter) roll Playerbot.Bg.AutoJoin.Chance
    // percent per minute and queue (BgQueueIntent, the same plumbing as Fill) for the BG type
    // that best balances the two factions; capped fleet-wide by AutoJoin.MaxQueued. Ambient bots
    // still waiting after QueueTimeoutMin leave the queue and rest CooldownMin.
    static void AutoJoinTick(uint32 now_ms);

    // NoteHumanBgQueue: remembers a real player who queued (called from the OnPlayerJoinedBgQueue
    // hook) so MakeRoomForRealPlayers can watch their wait.
    static void NoteHumanBgQueue(Player* human, uint32 bg_type_id, uint8 bracket = 0xFF);

    // ClaimRunningBgForHuman (Playerbot.Behavior.BgHumanPriority): called right after NoteHumanBgQueue. True when a
    // running battleground of that type and bracket has, or can make by a bot leaving, room for the human's party; the
    // human then takes that place instead of a bot fill opening a second instance (and TC is told to hold off creating
    // one, HoldNewBattleground, for 60 s). False: no such battleground, the usual fill proceeds. `open_slots` receives the places left after the party;
    // the caller still queues a bot fill (capped to it) when the human's instance is far from full.
    static bool ClaimRunningBgForHuman(Player* human, uint32 bg_type_id, uint8 bracket, uint32* open_slots = nullptr);

    // True while a real player of this type/bracket is queued and has no invitation yet. FireDueBgPorts holds bot ports for the
    // type meanwhile, so the match's prep timer (it only runs once someone is inside) does not start before the human arrives.
    static bool HasWaitingHuman(uint32 bg_type_id, uint8 bracket);

    // True while a human of this type/bracket is waiting for a place in a running battleground (see above).
    static bool HoldNewBattleground(uint32 bg_type_id, uint8 bracket);

    // MakeRoomForRealPlayers: every 3 s, for each watched human still queued and not yet invited
    // whose team is full in a running BG of that type and bracket, the lowest-scoring bot of that
    // team (kills*3 + HKs - deaths; never a flag carrier, never in combat) gets a BgLeaveIntent.
    // TC's own queue update (scheduled by the leave) then invites the human.
    static void MakeRoomForRealPlayers(uint32 now_ms);

    // True for 20 s after MakeRoomForRealPlayers asked a bot of this battleground type to leave. BgTopUp skips the type meanwhile, so it
    // does not queue a bot into the place that was freed for the waiting human.
    static bool RoomRecentlyMade(uint32 bg_type_id, uint32 now_ms);
};

} // namespace Playerbot::V2::Fleet
