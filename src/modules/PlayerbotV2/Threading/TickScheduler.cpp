#include "TickScheduler.h"
#include "AiWorkerPool.h"
#include "Bot/BotActivityTier.h"
#include "Config.h"
#include "Timer.h"

namespace Playerbot {

namespace {

// Per-bot phase-aligned deadline: the next instant strictly after `now` at
// which (t + phase(id)) is a multiple of `period`, i.e. a point in
// (now, now + period].
//
// WHY: the old re-arm was `now + period`. Bots whose deadlines were set in the
// same world tick — a whole raid entering combat on one pull, a party/raid
// logging in together (GroupRelogin brings 40 bots in within one second), every
// bot's first build after register — got identical deadlines and stayed phase-
// locked forever, because they all re-armed from the same `now` with the same
// period. The world thread then paid ALL of their snapshot builds (+ LoS
// raycasts, 30s spellbook rescans, bot ticks and the following intent drain) in
// ONE tick per period and nothing in the ticks between: a 40-bot raid on one
// map (serial build path — a single Map* never fans out) turned a ~3 ms average
// snapshot phase into a ~30-150 ms spike every 100-150 ms.
//
// Anchoring each bot to its own phase within the period spreads the fleet
// evenly over the period regardless of when deadlines were reset, and a bot
// still never waits longer than one period of its tier. A Fibonacci hash of the
// id keeps consecutive bot ids (a freshly created batch) far apart in phase.
// Steady-state cadence is exactly `period` (no drift), since a bot that fires
// at its phase point re-arms to the next one.
Ms NextPhaseDeadline(BotId id, Ms now, Ms period)
{
    const int64 per = period.count();
    if (per <= 1)
        return now + period;
    const uint64 phase = ((uint64(id) * 0x9E3779B97F4A7C15ull) >> 32) % uint64(per);
    const int64 into   = int64((uint64(now.count()) + phase) % uint64(per));
    return Ms{ now.count() + (per - into) };
}

// Tier period raised to the bot's PvP reaction floor.
Ms EffectivePeriod(ActivityTier tier, Ms pvp_floor)
{
    Ms const p = TickPeriodFor(tier);
    return pvp_floor.count() > p.count() ? pvp_floor : p;
}

} // namespace

TickScheduler::TickScheduler(AiWorkerPool& pool) : pool_(pool)
{
    lazy_ms_budget_ = sConfigMgr->GetIntDefault("Playerbot.Perf.TickBudgetMs", 0, true) > 0
        ? uint32(sConfigMgr->GetIntDefault("Playerbot.Perf.TickBudgetMs", 0, true)) : 0u;
}

void TickScheduler::on_world_tick(Ms now)
{
    // Collect due bot ids into a local buffer, then enqueue once with
    // a single mutex acquisition in AiWorkerPool::schedule_ticks.
    // At 1000+ bots × ~50 Hz × ~30% due-ratio = ~15K mutex acq/sec
    // saved per second (was: one acq per due bot).
    thread_local std::vector<BotId> due_buf;
    due_buf.clear();
    due_buf.reserve(bots_.size());
    for (auto& [id, sched] : bots_)
    {
        if (sched.next_tick.count() <= now.count())
        {
            due_buf.push_back(id);
            // Phase-aligned (see NextPhaseDeadline) with the SAME phase as the
            // snapshot deadline below, so a bot's bot tick still lands on the
            // world tick that just rebuilt its snapshot.
            sched.next_tick = NextPhaseDeadline(id, now, EffectivePeriod(sched.tier, sched.pvp_floor));
        }
    }
    pool_.schedule_ticks(due_buf);
}

void TickScheduler::register_bot(BotId id, ActivityTier initial)
{
    bots_[id] = BotSchedule{initial, Ms{0}};
}

void TickScheduler::unregister_bot(BotId id)
{
    bots_.erase(id);
    // No new ticks can be scheduled now; let one already queued/running finish before the caller
    // destroys the bot's registry entry (use-after-free otherwise).
    pool_.wait_not_in_flight(id);
}

void TickScheduler::set_tier(BotId id, ActivityTier classified, Ms now)
{
    auto it = bots_.find(id);
    if (it == bots_.end()) return;
    auto& sched = it->second;

    // Idle→Parked ramp. Track consecutive Idle classifications; once the
    // bot has been continuously Idle for kIdleRampToParked frames, park it
    // (ActivityTier::Hibernate, 2 s). The ramp keys on Idle SPECIFICALLY:
    // every other classification — Combat / Active / Cruise / dead-Idle wake
    // — falls into the else branch and resets the streak, so the bot leaves
    // (or never enters) the parked tier the instant it does anything. In
    // particular Cruise (solo open-world travel/quest, 300 ms) resets the
    // streak exactly like Active and therefore CAN NEVER be parked — a
    // travelling bot must never freeze. The streak only advances on a *true*
    // Idle classification — not on the synthesised parked tier — so an
    // already-parked-and-still-idle bot just stays parked (streak pinned at
    // the ramp threshold).
    ActivityTier effective = classified;
    if (classified == ActivityTier::Idle)
    {
        if (sched.idle_streak < kIdleRampToParked)
            ++sched.idle_streak;
        if (sched.idle_streak >= kIdleRampToParked)
            effective = ActivityTier::Hibernate;   // parked
    }
    else
    {
        sched.idle_streak = 0;   // Combat / Active / Cruise / dead-wake — never park
    }

    if (sched.tier == effective) return;
    sched.tier = effective;
    // Tier change. set_tier is only called from the post-build publish pass,
    // i.e. right after THIS bot's snapshot was rebuilt in this world tick, so
    // the snapshot the new tier was classified from is already fresh:
    //   * bot tick -> now: the bot acts on that fresh snapshot this very world
    //     tick (TickScheduler::on_world_tick runs after the publish pass). This
    //     is the reactivity guarantee: a bot entering combat / being owner-
    //     controlled / starting to move reacts on this frame, never on the
    //     parked 2 s cadence.
    //   * snapshot -> the bot's next phase point at the NEW tier's period, so
    //     a promotion (e.g. Idle 500 ms -> Combat 100 ms) rebuilds within one
    //     new period. It used to reset to `now`, which (a) rebuilt a snapshot
    //     that was already fresh on the very next tick and (b) re-armed every
    //     bot that changed tier in the same tick (a raid pull) to an identical
    //     deadline, phase-locking the whole group into one-tick build spikes.
    sched.next_tick = now;
    sched.next_snapshot = NextPhaseDeadline(id, now, EffectivePeriod(effective, sched.pvp_floor));
}

bool TickScheduler::should_build_snapshot(BotId id, Ms now)
{
    auto it = bots_.find(id);
    if (it == bots_.end()) return false;
    if (it->second.next_snapshot.count() > now.count()) return false;
    // Burst cap for the slow tiers: after a long frame every bot is due at once (295 builds = 370 ms in one tick,
    // which stalls the next frame the same way). Combat/Active always build; Cruise/Idle/Parked bots beyond the cap
    // simply stay due and build on the next frames (deadline untouched).
    if (it->second.tier != ActivityTier::Combat && it->second.tier != ActivityTier::Active)
    {
        if (now.count() != lazy_budget_now_)
        {
            lazy_budget_now_ = now.count();
            lazy_budget_ = kLazyBuildsPerTick;
            lazy_frame_start_ms_ = getMSTime();
        }
        if (lazy_budget_ == 0) return false;
        if (lazy_ms_budget_)
        {
            Ms const per = EffectivePeriod(it->second.tier, it->second.pvp_floor);
            bool const light = it->second.tier == ActivityTier::Idle || it->second.tier == ActivityTier::Hibernate;
            uint32 const limit = light ? lazy_ms_budget_ / 2u : lazy_ms_budget_;
            bool const starving = now.count() - it->second.next_snapshot.count() > 4 * per.count();
            if (!starving && getMSTimeDiff(lazy_frame_start_ms_, getMSTime()) >= limit)
            {
                ++n_budget_denied_;
                return false;
            }
        }
        --lazy_budget_;
    }
    // Advance using the tier's bot logic-tick period — snapshot cadence
    // matches bot tick cadence so an bot worker always sees fresh data
    // when its tick fires. Phase-aligned per bot (see NextPhaseDeadline) so
    // bots reset in the same tick do not stay phase-locked.
    it->second.next_snapshot = NextPhaseDeadline(id, now, EffectivePeriod(it->second.tier, it->second.pvp_floor));
    return true;
}

void TickScheduler::set_pvp_floor(BotId id, Ms floor, Ms now)
{
    auto it = bots_.find(id);
    if (it == bots_.end()) return;
    auto& sched = it->second;
    if (sched.pvp_floor.count() == floor.count()) return;
    sched.pvp_floor = floor;
    // Floor lifted (left PvP, or a survival bypass): do not sit out the rest of a
    // floor-length wait; re-arm to the next phase point of the plain tier period.
    // Floor raised: deadlines already armed run out, the next re-arm uses the floor.
    if (floor.count() == 0)
    {
        Ms const per = TickPeriodFor(sched.tier);
        Ms const nt  = NextPhaseDeadline(id, now, per);
        if (sched.next_tick.count() > nt.count())     sched.next_tick = nt;
        if (sched.next_snapshot.count() > nt.count()) sched.next_snapshot = nt;
    }
}

std::array<uint32, 5> TickScheduler::tier_counts() const
{
    std::array<uint32, 5> n{};
    for (auto const& [id, sched] : bots_)
    {
        uint8 const t = uint8(sched.tier);
        if (t < n.size()) ++n[t];
    }
    return n;
}

size_t TickScheduler::bot_count() const
{
    return bots_.size();
}

} // namespace Playerbot
