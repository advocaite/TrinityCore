// WorldBotSpawner - ambient "world" bots that live around the real players, the way ike3 / mod-playerbots random
// bots do: they log in near a player, wander and quest at a level that suits the zone, and log out again once the
// player has moved on. Off by default (Playerbot.World.Enable = 0).
//
// How it differs from the BotPopulationManager shaper: the shaper keeps a FIXED headcount spread over level
// brackets all over the world (Playerbot.Population.TotalTarget). This spawner keeps a headcount PER REAL PLAYER:
//   - each real player in the open world is an "anchor";
//   - ambient bots on the anchor's map within ActivityRadius count toward that anchor's target (BotsPerPlayer);
//   - a deficit is filled by logging in an offline pool bot of a level that suits the anchor's zone, or by creating
//     a new one, and the bot is moved (once its setup pipeline has finished) to a spot in the spawn ring around the
//     anchor, out of the anchor's sight when possible;
//   - an ambient bot farther than DespawnRadius from every anchor (or when no real player is online) is logged out
//     once it has been placed for a while, is out of combat and is not in a group, instance or battleground.
// The two modes are exclusive: with Playerbot.Population.TotalTarget > 0 this spawner stays off (a warning is logged).
//
// World thread only. All the work is a no-op while the feature is disabled.

#pragma once

#include "Bot/BotTypes.h"
#include "AsyncCallbackProcessor.h"
#include "DatabaseEnvFwd.h"
#include "ObjectGuid.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

#include "AreaCap.h"
#include "StrandedWatch.h"
#include "WorldLoadFacts.h"
#include "WorldFactionMix.h"
#include "PoolBalance.h"

class Player;

namespace Playerbot::V2::Fleet {

class BotPopulationManager;

class WorldBotSpawner
{
public:
    explicit WorldBotSpawner(BotPopulationManager& owner);

    // Called from BotPopulationManager::OnWorldTick every world frame; rate-limits itself to Playerbot.World.TickMs.
    void OnWorldTick(uint32 now_ms);

    // True while a bot we logged in / created is waiting for its setup and placement; queue fillers leave it alone.
    bool IsPending(uint64 guid_low) const { return pending_.contains(guid_low); }

    // One-line state summary for the logs / a diagnostic command.
    std::string Report() const;
    // Numbers for the load-test row: residents awake, roster bots asleep, process memory and CPU.
    WorldLoadFacts Load() const;
    // A bot this spawner placed (or is placing) in the world. The battleground filler never drafts these.
    bool IsPlaced(uint64 guid_low) const { return placed_.contains(guid_low) || pending_.contains(guid_low); }

private:
    struct Config
    {
        bool   enable           = false;
        uint32 max_bots         = 100;     // ambient bots online at once, over all players
        uint32 bots_per_player  = 25;      // target per real player (within ActivityRadius)
        uint32 city_bots        = 0;       // target when the player is in a capital/town (0 = BotsPerPlayer)
        uint32 zone_bots        = 0;       // target when the player is in a leveling zone (0 = BotsPerPlayer)
        float  activity_radius  = 300.0f;  // a bot this close to an anchor counts toward it
        float  despawn_radius   = 450.0f;  // a bot farther than this from every anchor is logged out
        float  ring_min         = 60.0f;   // spawn ring (placement distance from the anchor)
        float  ring_max         = 220.0f;
        uint32 tick_ms          = 3000;
        uint32 spawn_per_tick   = 3;
        uint32 despawn_per_tick = 6;
        uint32 city_spawn_per_tick = 0;    // spawn rate while the player is in a capital (0 = SpawnPerTick)
        float  wake_radius      = 0.0f;    // >0: sleeping residents (pool bots saved this close to a player) are logged in first, in place
        uint32 zone_grace_ms    = 300000;  // bots around a spot the player left (instance, hearth) stay this long
        uint32 placed_grace_ms  = 60000;   // a freshly placed bot is not despawned for this long
        uint32 min_level        = 10;      // starter zones have no navmesh: bots below 10 would stand still
        uint32 max_level        = 60;
        uint32 friendly_pct     = 80;      // share of new bots of the anchor's faction (zones not in the zone mix)
        FactionMix mix;                    // per-zone share of the anchor's faction: contested zones get enemy bots (WorldFactionMix.h)
        bool   global_fill      = false;   // spread the rest of the MaxBots budget over the whole world, not only near players
        uint32 global_per_tick  = 8;
        bool   filler_hub_only  = true;    // world-wide filler reuses only pool bots saved inside a quest hub / town
        uint32 rss_budget_mb    = 0;       // 0 = off. Over it the world-wide filler stops adding bots; 15% over, the bubble too
        uint32 town_pct         = 70;      // share of bots placed in a town that stay in town (profession mode)
        bool   seed_roster      = false;   // build the sleeping roster up to PoolMax without waiting for a player to arrive
        uint32 seed_in_flight   = 8;       // seeded bots set up at the same time
        uint32 pool_max         = 400;     // ambient characters allowed to exist (online or not)
        bool   natural_moves    = true;    // bots arrive on foot from out of sight and leave toward an inn before they log out
        bool   day_rhythm       = true;    // scale the bubble target by the hour of the day (Fleet/DayRhythm.h)
        uint32 jump_grace_ms    = 20000;   // bots around the place a player jumped away from are kept this long (an instance trip: ZoneGraceSec)
        uint32 area_cap_outdoors = 14;     // most world bots within area_radius of one another outdoors (0 = no cap; Fleet/AreaCap.h)
        uint32 area_cap_town    = 40;      // the same in a capital
        float  area_radius      = 45.0f;
        uint32 pool_horde_pct   = 50;      // share of the roster that should be Horde, in every level bracket (PoolBalance.h)
        uint32 skyborne_seed_pct = 5;      // share of the roster seeds that are level 1-9 Skyborne residents of Zephras Isle (Classic 1.60)
    };

    struct Anchor
    {
        ObjectGuid guid;
        uint32 map = 0;
        float  x = 0, y = 0, z = 0;
        uint32 zone = 0;
        uint8  level = 1;
        bool   alliance = true;
    };

    // A bot we have asked to log in (or created) and have not placed yet.
    struct Pending
    {
        ObjectGuid anchor;
        uint32 queued_ms = 0;
        bool   global = false;             // world-wide filler: no placement, the setup pipeline puts it in a leveling town
        bool   wake = false;               // a sleeper woken where it was saved: stepped off a spot another bot already stands on
        bool   unstacked = false;
    };

    // A bot placed by us; not despawned until placed_ms + grace.
    struct Placed
    {
        uint32 placed_ms = 0;
    };

    void LoadConfig();
    void CollectAnchors(std::vector<Anchor>& out);
    void DrivePending(uint32 now_ms, std::vector<Anchor> const& anchors);
    void DriveDespawn(uint32 now_ms, std::vector<Anchor> const& anchors);
    void DriveSpawn(uint32 now_ms, std::vector<Anchor> const& anchors);
    void DriveWake(uint32 now_ms, Anchor const& a, uint32 want);
    // Roster seeding: creates residents (level spread, both factions) one by one, lets the setup pipeline gear them and send them
    // to a zone hub for their level, then logs them out so they sleep there. No player needed; stops at PoolMax.
    void DriveSeed(uint32 now_ms);   // log in the pool bots that sleep around this anchor, no placement
    void FinishSpawn(Anchor const& a, std::vector<bool> const& slots, std::vector<uint8> const& slot_levels,
                     QueryResult res, bool global = false);
    void DriveGlobalSpawn(uint32 now_ms, std::vector<Anchor> const& anchors);
    // Classic 1.60: a Skyborne bot that reached level 10 on Zephras Isle is carried once to a leveling town (the isle has no exit).
    void DriveGraduation(uint32 now_ms);
    // Residents standing in lava or on ground with no way out are placed again (Fleet/StrandedWatch.h).
    void DriveStranded(uint32 now_ms, std::vector<Anchor> const& anchors);
    void DriveStuckLedger(uint32 now_ms);   // bots the idle watch keeps finding standing: why they cannot move, and back onto the mesh if off it
    // A bot the spawner no longer wants: out of every player's sight it logs out at once; in sight it first walks to the nearest inn
    // (or away from the player) and logs out once it is out of view or after a minute. True when the bot was dealt with (logged out
    // or sent walking).
    bool RetireBot(Player* bot, uint32 now_ms, std::vector<Anchor> const& anchors);
    void DriveLeaving(uint32 now_ms, std::vector<Anchor> const& anchors);
    // A crowd over the area cap sends its extras to other spots next to the nearest player (Fleet/AreaCap.h).
    void DriveCrowd(uint32 now_ms, std::vector<Anchor> const& anchors);
    // Level range of the bots around this anchor: its zone's, lifted to MinLevel, except on Zephras Isle (Fleet/SkyborneIsle.h).
    void WindowFor(Anchor const& a, uint8& lo, uint8& hi) const;
    bool PlaceNear(Player* bot, Player* anchor);
    void StepOffStack(Player* bot);
    uint32 CountAmbientNear(Anchor const& a) const;
    std::unordered_map<uint32, uint32> AwakeByZone(Anchor const& a) const;
    uint32 SpawnRateFor(Anchor const& a) const;
    uint32 TargetFor(Anchor const& a) const;   // bubble size for this anchor: city_bots / zone_bots / bots_per_player

    struct Ghost { Anchor a; uint32 until_ms; };   // where a player was before an instance / far jump: keeps its bots for a grace
    std::vector<Ghost> ghosts_;
    std::unordered_map<uint64, Anchor> prev_anchor_;
    float last_place_dist_ = 0.0f;                  // ring distance chosen by the last PlaceNear (the teleport lands later)
    BotPopulationManager& owner_;
    Config cfg_;
    uint32 last_tick_ms_ = 0;
    uint32 now_ms_ = 0;
    uint32 rss_read_ms_ = 0;
    uint64 rss_kb_ = 0;                             // process RSS, read every 5 s
    bool   rss_capped_ = false;                     // the RSS budget is reached: no new bots                             // clock passed to OnWorldTick (module uptime)
    uint32 last_cfg_ms_ = 0;
    uint32 last_report_ms_ = 0;
    uint32 last_mem_ms_ = 0;       // [mem] line every 5 min
    bool   warned_exclusive_ = false;

    std::unordered_map<uint64, Pending> pending_;   // bot guid low -> pending placement
    std::unordered_map<uint64, Placed>  placed_;    // bot guid low -> placement time
    size_t next_anchor_ = 0;                        // round robin over anchors with a deficit

    QueryCallbackProcessor queries_;
    bool   query_inflight_ = false;
    uint32 query_started_ms_ = 0;
    int64  pool_count_ = -1;                        // ambient characters in the DB (-1 = not loaded yet)
    bool   pool_count_inflight_ = false;
    PoolCounts pool_counts_;                        // the roster by faction and level bracket (levels 10+)
    bool   pool_counts_inflight_ = false;
    uint32 pool_counts_ms_ = 0;                     // when the census was last read

    // Counters since start, for Report().
    uint32 anchors_ = 0;
    uint32 ambient_online_ = 0;
    uint64 n_logged_in_ = 0;
    uint64 n_created_ = 0;
    uint64 n_placed_ = 0;
    uint64 n_despawned_ = 0;
    uint64 n_abandoned_ = 0;
    struct Seeding { uint32 started_ms = 0; uint32 setup_done_ms = 0; };
    std::unordered_map<uint64, Seeding> seeding_;
    uint32 last_grad_ms_ = 0;
    uint32 last_stranded_ms_ = 0;
    uint32 last_ledger_ms_ = 0;
    std::unordered_map<uint64, uint32> ledger_ms_;   // per bot: when its last ledger line was written
    uint32 last_crowd_ms_ = 0;
    std::unordered_map<uint64, uint32> leaving_;   // bot guid low -> module ms when it was sent walking away
    uint64 n_arrived_walking_ = 0;
    uint64 n_left_walking_ = 0;
    uint32 swap_burst_until_ms_ = 0;   // module ms until which a jump (teleport / hearth) swaps the bubble at a higher rate
    uint32 owned_online_ = 0;          // companions and a player's own bots in the world (outside the world cap)
    uint32 tied_online_ = 0;           // ambient bots in a group, instance or battleground (outside the world cap)
    uint64 n_evicted_ = 0;             // bots recycled early, inside DespawnRadius, to make room at the world cap
    uint64 n_crowd_moved_ = 0;
    uint64 n_stranded_moved_ = 0;      // residents found in lava or with no way out and placed again
    std::unordered_map<uint64, StrandedState> stranded_;
    uint64 n_graduated_ = 0;           // Skyborne bots moved off Zephras Isle at level 10
    uint64 n_seeded_ = 0;              // residents created by the seeding pass and put to sleep
    uint64 n_woken_ = 0;               // sleeping residents logged in at their saved position
    uint64 n_wake_dry_ = 0;            // wake passes that found nobody asleep nearby (the ring path then places new residents)
    std::unordered_map<uint64, uint32> wake_dry_until_;   // anchor guid -> module ms until which the wake pass is skipped
    uint64 cpu_prev_ms_ = 0;           // process CPU time at the last report
    uint32 cpu_prev_at_ = 0;           // module ms of that reading
    double cpu_pct_ = 0.0;             // process CPU over the last report interval, in percent of one core
    uint64 n_global_settled_ = 0;      // world-wide filler bots whose setup finished
    uint32 all_online_ = 0;            // every bot in the world this tick (ambient, raid, BG, companions)
    uint32 global_online_ = 0;         // ambient bots farther than ActivityRadius from every player (this tick)
    // Why nothing was spawned, since start (also in the [world] line).
    uint32 last_rules_ms_ = 0;
    uint32 soak_anchors_ = 0;          // configured soak anchors (bots standing in for a player) found this tick
    uint32 anchors_in_instance_ = 0;   // real players skipped this tick: in an instance, raid, battleground or arena
    uint64 n_no_match_ = 0;            // slots with no offline pool bot of the wanted faction and level range
    uint64 n_pool_full_ = 0;           // slots that could not create a character: PoolMax reached
    uint64 n_spawn_failed_ = 0;        // character creation failed
    uint64 n_login_failed_ = 0;        // LoginBot refused
    uint32 last_why_ms_ = 0;           // rate limit of the reason log line
    uint32 last_tick_cost_ms_ = 0;
    uint64 tick_cost_sum_ms_ = 0;      // since start, for the average
    uint64 tick_count_ = 0;
    uint64 baseline_rss_kb_ = 0;       // process RSS while no ambient bot was online
    uint32 max_tick_cost_ms_ = 0;
};

} // namespace Playerbot::V2::Fleet
