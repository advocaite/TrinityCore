// TickScheduler - Decides which bots tick this world frame and routes them to
// the bot worker pool. Driven by per-bot ActivityTier and the world tick budget.

#pragma once

#include "Bot/BotTypes.h"
#include <array>
#include <chrono>
#include <unordered_map>
#include <vector>

namespace Playerbot {

class AiWorkerPool;

class TickScheduler
{
public:
    explicit TickScheduler(AiWorkerPool& pool);

    // Called on world thread once per world tick. Selects bots whose next-tick
    // deadline has passed and submits them to the bot worker pool.
    void on_world_tick(Ms now);

    // Bot lifecycle
    void register_bot(BotId id, ActivityTier initial = ActivityTier::Idle);
    void unregister_bot(BotId id);

    // Tier transition (called by bot tick logic via the snapshot publish path).
    //
    // `classified` is the tier the per-tick classifier decided the bot
    // SHOULD be in this frame (see PlayerbotV2.cpp). The scheduler applies
    // an idle→parked ramp on top: a bot classified Idle for
    // kIdleRampToParked consecutive frames is promoted to the parked tier
    // (ActivityTier::Hibernate, 2 s) to slash snapshot-build cost on the
    // long tail of AFK city/inn bots. Any non-Idle classification (combat /
    // active / dead) resets the streak immediately, so a parked bot snaps
    // back to its fast cadence the moment it wakes. On any actual tier
    // change the bot tick resets to `now` (the bot acts this frame on the
    // snapshot that was just built and classified) and the snapshot deadline
    // re-arms to the bot's next phase point at the new tier's period, so
    // there is no reactivity regression. All deadlines are phase-aligned per
    // bot (TickScheduler.cpp NextPhaseDeadline) so bots reset in the same
    // tick spread over the period instead of building in lock-step.
    void set_tier(BotId id, ActivityTier classified, Ms now);

    // Snapshot-build cadence gate. Per-bot tier-driven throttle on
    // `BotSnapshotBuilder::Build` so Build is paid at the bot tick rate
    // (Combat 10Hz / Active 5Hz / Idle 1Hz) instead of every world frame
    // (50Hz). Returns true if it's time to rebuild for `id`; false if
    // the throttle is still holding. On true, advances the per-bot
    // deadline. Unregistered bots return false.
    bool should_build_snapshot(BotId id, Ms now);

    // PvP reaction floor (Playerbot.Pacing.PvpReactionDelayMs): a per-bot minimum
    // for the bot logic-tick and snapshot-build period, whatever the tier. 0 clears it.
    // Set by the publish pass each time the bot's snapshot is rebuilt: the caller
    // passes 0 for PvE and for survival (low HP, dead/ghost) so those never wait.
    // Clearing pulls the next deadlines back inside one tier period.
    void set_pvp_floor(BotId id, Ms floor, Ms now);

    // Diagnostics
    size_t bot_count() const;
    // Bots per effective tier: Combat, Active, Cruise, Idle, Parked (world thread only).
    std::array<uint32, 5> tier_counts() const;

private:
    // Slow-tier snapshot builds allowed per world frame (see should_build_snapshot).
    static constexpr uint32 kLazyBuildsPerTick = 64;
    int64  lazy_budget_now_ = -1;
    uint32 lazy_budget_ = 0;
    // Time budget for the slow tiers per world frame, in ms of wall time since the frame's first snapshot decision
    // (Playerbot.Perf.TickBudgetMs, 0 = off). The parked and idle tiers stop at half of it, the travelling tier at the whole
    // of it; Combat and Active (raid, battleground, grouped, owner-controlled and near-player bots) never wait. A bot that
    // the budget kept waiting for more than four of its periods builds anyway, so the squeeze only stretches cadence.
    uint32 lazy_ms_budget_ = 0;
    uint32 lazy_frame_start_ms_ = 0;
    uint64 n_budget_denied_ = 0;
public:
    void set_lazy_ms_budget(uint32 ms) { lazy_ms_budget_ = ms; }
    uint64 budget_denied_total() const { return n_budget_denied_; }
private:
    // Consecutive Idle classifications required before a bot is ramped to
    // the parked tier (ActivityTier::Hibernate). At Idle's 500 ms cadence
    // this is ~2.5 s of uninterrupted idleness before snapshot-build drops
    // to 0.5 Hz — short enough to reclaim the AFK long tail quickly, long
    // enough that a bot pausing mid-activity isn't parked prematurely.
    static constexpr uint8 kIdleRampToParked = 5;

    struct BotSchedule
    {
        ActivityTier tier         = ActivityTier::Idle;
        Ms           next_tick{0};
        Ms           next_snapshot{0};
        // Count of consecutive frames the classifier returned Idle for this
        // bot. Reset to 0 on any non-Idle classification. When it reaches
        // kIdleRampToParked the bot is promoted to the parked tier. Lives
        // here (per-bot schedule) rather than on BotAI so the cadence layer
        // owns all of its own state.
        uint8        idle_streak  = 0;
        // PvP reaction floor on the tier period (see set_pvp_floor); 0 = none.
        Ms           pvp_floor{0};
    };

    AiWorkerPool& pool_;
    std::unordered_map<BotId, BotSchedule> bots_;
};

} // namespace Playerbot
