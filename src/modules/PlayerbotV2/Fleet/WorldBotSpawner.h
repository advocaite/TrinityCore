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
#include "BotSize.h"
#include "GateDuels.h"
#include "Gathering.h"
#include "MovingBubble.h"
#include "StrandedWatch.h"
#include "WorldLoadFacts.h"
#include "WorldFactionMix.h"
#include "PoolBalance.h"
#include "PoolBudget.h"

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

    // The world bot choice of the ForeverBots panel (Fleet/BotSize.h PopLevel), set by a GM in game (.bot population, add-on POP_REQ).
    // It applies on the next world tick, is saved in the characters database and read back at the next start. nullopt = the files
    // decide (Bots.* lines, Playerbot.World.Enable).
    void SetPopulation(std::optional<PopLevel> level);
    std::optional<PopLevel> Population() const { return pop_level_; }
    // One plain line for the panel and the command: the choice, what it gives, and whether bots are still logging out.
    std::string PopulationLine() const;
    bool PopulationDraining() const { return drain_until_ms_ != 0; }

private:
    struct Config
    {
        bool   enable           = false;
        uint32 max_bots         = 100;     // ambient bots online at once, over all players
        uint32 bots_per_player  = 25;      // target per real player (within ActivityRadius)
        uint32 city_bots        = 0;       // target when the player is in a capital/town (0 = BotsPerPlayer)
        uint32 zone_bots        = 0;       // target when the player is in a leveling zone (0 = BotsPerPlayer)
        uint32 start_zone_bots  = 25;      // the same in a start zone (Mulgore, Durotar, ...), where a real realm has a few new characters (0 = ZoneBots)
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
        bool   natural_moves    = true;    // bots arrive on foot from out of sight and leave out of sight (the city gate in a capital) before they log out
        bool   idle_swap        = true;    // a bot the idle watch keeps finding standing, out of every player's sight, is placed again and walks in
        uint32 idle_swap_strikes = 2;      // idle-watch strikes before the swap
        uint32 idle_swap_cd_ms  = 180000;  // a bot is swapped at most this often
        bool   day_rhythm       = true;    // scale the bubble target by the hour of the day (Fleet/DayRhythm.h)
        uint32 jump_grace_ms    = 20000;   // bots around the place a player jumped away from are kept this long (an instance trip: ZoneGraceSec)
        uint32 area_cap_outdoors = 14;     // most world bots within area_radius of one another outdoors (0 = no cap; Fleet/AreaCap.h)
        uint32 area_cap_town    = 40;      // the same in a capital
        uint32 area_cap_start   = 10;      // the same in a start zone: one camp and one quest giver for every new character
        float  area_radius      = 45.0f;
        uint32 pool_horde_pct   = 50;      // share of the roster that should be Horde, in every level bracket (PoolBalance.h)
        uint32 variable_max     = 2000;    // temporary residents allowed to exist next to the core roster (PoolBudget.h); 0 = none
        uint32 variable_ttl_h   = 24;      // hours a temporary resident stays offline before it is deleted
        bool   travel_soak      = false;   // send a few world bots between places of the travel graph and log how each leg ends (Travel/SoakBoard.h)
        uint32 travel_soak_bots = 8;       // bots on a soak leg at the same time
        uint32 travel_soak_leg_s = 1200;   // seconds a leg may take before it counts as stuck
        bool   moving_enable    = true;    // a riding player's bubble follows his heading (Fleet/MovingBubble.h)
        MovingRule moving;
        uint32 moving_spawn_factor = 2;    // x spawn rate while he moves
        float  moving_despawn_behind = 450.0f;   // bots this far behind a moving player log out (out of sight); 0 = only DespawnRadius
        // Plain bot numbers (Bots.* lines, Fleet/BotSize.h). plain = at least one Bots.* line is set: max_bots, pool_max, city_bots,
        // zone_bots (= zone_hi) and bots_per_player then come from them, each zone rolls its number in [zone_lo, zone_hi], the cap
        // is shared evenly over the occupied places, and a place nobody is in for leave_zone_ms loses its bots fast. day_rhythm is off
        // unless Bots.DayRhythm = 1 (the numbers are flat).
        bool   plain            = false;
        uint32 zone_lo          = 0;
        uint32 zone_hi          = 0;
        uint32 leave_zone_ms    = 20000;   // Bots.LeaveZoneSec: a zone without a player for this long logs its bots out (0 = old rule only)
        std::string plain_summary;         // for the start line
    };

    struct Anchor
    {
        ObjectGuid guid;
        uint32 map = 0;
        float  x = 0, y = 0, z = 0;
        uint32 zone = 0;
        uint8  level = 1;
        bool   alliance = true;
        Motion motion;                     // smoothed velocity; a ghost has none
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
    void PublishStrangers();
    void CollectAnchors(std::vector<Anchor>& out);
    void PublishParties(uint32 now_ms);
    void DrivePending(uint32 now_ms, std::vector<Anchor> const& anchors);
    void DriveDespawn(uint32 now_ms, std::vector<Anchor> const& anchors);
    void DriveSpawn(uint32 now_ms, std::vector<Anchor> const& anchors);
    void DriveWake(uint32 now_ms, Anchor const& a, uint32 want);
    // Roster seeding: creates residents (level spread, both factions) one by one, lets the setup pipeline gear them and send them
    // to a zone hub for their level, then logs them out so they sleep there. No player needed; stops at PoolMax.
    void DriveSeed(uint32 now_ms);   // log in the pool bots that sleep around this anchor, no placement
    void DriveRetire(uint32 now_ms);   // deletes temporary residents that nobody has needed for VariableTtlHours
    void DriveTravelSoak(uint32 now_ms);   // gives soak legs to world bots and logs how they end
    void FinishSpawn(Anchor const& a, std::vector<bool> const& slots, std::vector<uint8> const& slot_levels,
                     QueryResult res, bool global = false);
    void DriveGlobalSpawn(uint32 now_ms, std::vector<Anchor> const& anchors);
    // Classic 1.60: a Skyborne bot that reached level 10 on Zephras Isle is carried once to a leveling town (the isle has no exit).
    void DriveGraduation(uint32 now_ms);
    // Residents standing in lava or on ground with no way out are placed again (Fleet/StrandedWatch.h).
    void DriveStranded(uint32 now_ms, std::vector<Anchor> const& anchors);
    void DriveIdleSwap(uint32 now_ms, std::vector<Anchor> const& anchors);   // out-of-sight bots that stand around: placed again, walk in
    void DriveStuckLedger(uint32 now_ms);   // bots the idle watch keeps finding standing: why they cannot move, and back onto the mesh if off it
    // A bot the spawner no longer wants: out of every player's sight it logs out at once; in sight it first walks to the nearest inn
    // (or away from the player) and logs out once it is out of view or after a minute. True when the bot was dealt with (logged out
    // or sent walking).
    bool RetireBot(Player* bot, uint32 now_ms, std::vector<Anchor> const& anchors);
    void DriveLeaving(uint32 now_ms, std::vector<Anchor> const& anchors);
    // A crowd over the area cap sends its extras to other spots next to the nearest player (Fleet/AreaCap.h).
    void DriveCrowd(uint32 now_ms, std::vector<Anchor> const& anchors);
    void DriveGateDuels(uint32 now_ms, std::vector<Anchor> const& anchors);   // pairs of bots duelling outside a capital gate (GateDuels.h)
    void DriveGatherings(uint32 now_ms, std::vector<Anchor> const& anchors);  // bots at a campfire and in a group outside the gate, talking (Gathering.h)
    void FormStreetChats(uint32 now_ms, std::vector<Anchor> const& anchors);  // and small chats inside the cities where a player walks
    bool GatherFree(BotId id, Player* p, uint32 now_ms) const;                // a world bot free to join a gathering or a chat now
    // Level range of the bots around this anchor: its zone's, lifted to MinLevel, except on Zephras Isle (Fleet/SkyborneIsle.h).
    void WindowFor(Anchor const& a, uint8& lo, uint8& hi) const;
    bool PlaceNear(Player* bot, Player* anchor);
    uint32 AreaCapFor(uint32 zone) const;
    void StepOffStack(Player* bot);
    uint32 CountAmbientNear(Anchor const& a) const;
    std::unordered_map<uint32, uint32> AwakeByZone(Anchor const& a) const;
    uint32 SpawnRateFor(Anchor const& a) const;
    uint32 TargetFor(Anchor const& a) const;   // bubble size for this anchor: city_bots / zone_bots / bots_per_player
    // Plain bot numbers: each occupied place's number for this tick (rolled zone number, even share of the cap) and how many
    // players are in it; a place's roll and when it last had a player.
    void PlanPlaces(uint32 now_ms, std::vector<Anchor> const& anchors, std::vector<Anchor> const& keep_anchors);
    uint32 PlaceWantFor(Anchor const& a);
    std::unordered_map<uint64, uint32> place_target_, place_players_, zone_roll_, place_seen_ms_;
    std::string plain_logged_;
    std::optional<PopLevel> pop_level_;       // the panel's choice (SetPopulation); loaded once from the database
    bool   pop_loaded_ = false;
    bool   pop_changed_ = false;              // SetPopulation since the last LoadConfig: the config is read again at once
    bool   drain_requested_ = false;          // SetPopulation stepped down: the next tick starts the drain
    uint32 drain_until_ms_ = 0;               // a step down / Off: every world bot not busy logs out until then (or none is left)
    uint32 drain_started_ms_ = 0;
    void LoadPopulationOnce();
    void DriveDrain(uint32 now_ms, std::vector<Anchor> const& anchors);

    struct Ghost { Anchor a; uint32 until_ms; };   // where a player was before an instance / far jump: keeps its bots for a grace
    std::vector<Ghost> ghosts_;
    std::unordered_map<uint64, Anchor> prev_anchor_;
    // Per player: last position seen by the motion tracker and his smoothed velocity (Playerbot.World.Moving.*).
    struct MotionTrack { uint32 map = 0; float x = 0.0f, y = 0.0f; uint32 ms = 0; Motion m; };
    std::unordered_map<uint64, MotionTrack> motion_;
    void TrackMotion(uint32 now_ms, std::vector<Anchor>& anchors);
    bool InBubbleOf(Anchor const& a, float x, float y) const;   // counts toward a's target (corridor while he moves)
    uint64 n_moving_placed_ = 0;       // bots put ahead of a moving player
    uint64 n_moving_woken_ = 0;        // sleepers woken ahead of a moving player
    uint64 n_left_behind_ = 0;         // bots logged out far behind a moving player
    struct PlaceDest { float x = 0.0f, y = 0.0f; uint32 until_ms = 0; };
    std::unordered_map<uint64, PlaceDest> place_dest_;   // where a bot sent by PlaceNear is walking to, for a while: the crowd count uses it, not its position on the way
    // World bots under level 10 made before they stopped being given a generated set (migration 0031): each is stripped to its starter outfit once, on login.
    std::unordered_set<uint64> gear_reset_pending_, gear_reset_tried_;
    bool gear_reset_loaded_ = false, gear_reset_inflight_ = false;
    void LoadGearResetPending();
    void ResetStarterGearOnce(Player* bot);
    float last_place_x_ = 0.0f, last_place_y_ = 0.0f, last_place_z_ = 0.0f;   // where the last PlaceNear sent the bot (it lands later)
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
    int64  pool_count_ = -1;                        // ambient characters in the DB, island residents not counted (-1 = not loaded yet)
    int64  variable_count_ = 0;                     // temporary residents in the DB, plus the island residents created before the flag existed
    uint32 variable_sweep_ms_ = 0;                  // when temporary residents were last retired
    bool   variable_sweep_inflight_ = false;
    uint32 n_created_variable_ = 0;                 // temporary residents created since start
    uint32 soak_ms_ = 0;                            // when the travel soak last looked for a bot to send
    uint32 soak_summary_ms_ = 0;
    uint32 soak_next_id_ = 1;
    uint32 n_retired_ = 0;                          // temporary residents deleted since start
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
    // Why PlaceNear's candidate spots were refused (index: 0 no ground, 1 water, 2 no navmesh point, 3 lava, 4 no complete path, 5 area full) and
    // how many calls found none at all; reported in the [world] line so a placement that never succeeds names its cause.
    uint64 n_place_reject_[6] = {};
    uint64 n_place_none_ = 0;
    uint64 n_place_calls_ = 0;
    uint32 last_have_ = 0, last_target_ = 0;   // the last anchor a spawn pass found at its bubble target: how many it has and what it may have ([world] line)
    uint64 n_at_target_ = 0;                   // passes that skipped an anchor because its bubble was full
    struct Seeding { uint32 started_ms = 0; uint32 setup_done_ms = 0; };
    std::unordered_map<uint64, Seeding> seeding_;
    uint32 last_grad_ms_ = 0;
    uint32 last_stranded_ms_ = 0;
    uint32 last_ledger_ms_ = 0;
    std::unordered_map<uint64, uint32> ledger_ms_;   // per bot: when its last ledger line was written
    uint32 last_crowd_ms_ = 0;
    // Gate duels: the gate found per city (state 0 = not tried, 1 = known, 2 = no gate found, tried again at retry_ms), the pairs in
    // progress, and the bots resting after a duel.
    struct GateState { uint8 state = 0; uint32 retry_ms = 0; float x = 0, y = 0, z = 0; };
    struct GateSlot
    {
        uint32 city = 0, slot = 0, since_ms = 0, started_ms = 0;
        uint64 a = 0, b = 0, watch[2] = { 0, 0 };
        bool started = false;
    };
    GateState gate_state_[GateDuels::kCityCount];
    std::vector<GateSlot> gate_slots_;
    std::unordered_map<uint64, uint32> gate_rest_;
    uint32 gate_why_ms_[6] = {};   // last "why no duels" line per city (kCityCount)
    uint32 last_gate_ms_ = 0;
    // Gatherings outside the gates (Gathering.h): the rings in progress, the bots resting after one, the Basic Campfire's object entry
    // (learnt from the first fire a bot lights: 0 = not yet, UINT32_MAX = the spell gave none).
    struct GatherRing
    {
        uint32 city = 0, ring = 0, since_ms = 0, until_ms = 0;   // ring: 0 campfire, 1 standing, Gathering::kStreetRing+ a street chat
        bool   sit = false;
        std::string place;            // a street chat: where it stands ("the bank")
        float  cx = 0, cy = 0, cz = 0;
        std::vector<uint64> members;
        ObjectGuid fire;
        uint32 fire_try_ms = 0;
        // The talk: a script (script >= 0) or the chat model (model), the bots taking part, when the next line is due.
        int32  script = -1;
        uint32 line = 0, next_line_ms = 0, turns_left = 0, topic = 0, asked_ms = 0;
        bool   model = false, waiting = false;
        std::vector<uint64> talkers;
        std::string chat;
        uint64 key = 0;
        std::vector<uint32> recent;   // scripts said lately in this ring
    };
    std::vector<GatherRing> gather_rings_;
    std::unordered_map<uint64, uint32> gather_rest_;
    uint32 last_gather_ms_ = 0;
    uint32 gather_form_ms_[6] = {};
    uint32 gather_why_ms_[6] = {};
    uint32 street_form_ms_[6] = {};
    uint32 street_seq_ = 0;
    uint32 campfire_entry_ = 0;
    uint64 gather_seq_ = 0;
    std::unordered_map<uint64, uint32> leaving_;   // bot guid low -> module ms when it was sent walking away
    uint64 n_arrived_walking_ = 0;
    uint64 n_left_walking_ = 0;
    uint64 n_idle_swaps_ = 0;
    uint32 last_swap_pass_ms_ = 0;
    std::unordered_map<uint64, uint32> swap_ms_;   // per bot: when it was last swapped
    uint32 swap_burst_until_ms_ = 0;   // module ms until which a jump (teleport / hearth) swaps the bubble at a higher rate
    uint32 owned_online_ = 0;          // companions and a player's own bots in the world (outside the world cap)
    uint32 tied_online_ = 0;           // ambient bots in a group, instance or battleground (outside the world cap)
    uint64 n_evicted_ = 0;             // bots recycled early, inside DespawnRadius, to make room at the world cap
    uint32 parties_formed_ = 0;        // small parties of world bots formed / ended (Fleet/BotParties.h)
    uint32 parties_ended_ = 0;
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
