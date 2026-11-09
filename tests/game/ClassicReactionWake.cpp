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


#include "tc_catch2.h"

#ifdef TESTS_HAVE_PLAYERBOT_V2

#include "../../src/modules/PlayerbotV2/Threading/ReactionWake.h"
#include "../../src/modules/PlayerbotV2/Threading/TickScheduler.h"
#include "../../src/modules/PlayerbotV2/Threading/AiWorkerPool.h"

using namespace Playerbot;
namespace RW = Playerbot::ReactionWake;

namespace
{
RW::Input Make(int64_t busy, int64_t next_tick_in, int64_t period = 100, int64_t since = 1000000)
{
    RW::Input in;
    in.busy_ms = busy;
    in.next_tick_in_ms = next_tick_in;
    in.period_ms = period;
    in.since_last_wake_ms = since;
    return in;
}
}

TEST_CASE("ReactionWake: a GCD that ends between two tick slots is woken at its end", "[ReactionWake]")
{
    // Ticks at +30, +130, +230; the GCD ends at +120: the tick at +130 would be 10 ms late, so no gain.
    CHECK(RW::Delay(Make(120, 30)) == -1);
    // The GCD ends at +100: the next tick after it is +130, 30 ms late: wake.
    CHECK(RW::Delay(Make(100, 30)) == 100);
    // A 1.5 s GCD with ticks at +10, +110, ...: the tick at +1510 is 10 ms late; at +1500 the slot lands on it exactly: no wake.
    CHECK(RW::Delay(Make(1500, 10)) == -1);
    CHECK(RW::Delay(Make(1450, 10)) == 1450);   // next tick after the end is +1510, 60 ms late
}

TEST_CASE("ReactionWake: a tick already due counts as now", "[ReactionWake]")
{
    CHECK(RW::Delay(Make(270, -5)) == 270);   // ticks at 0, 100, 200, 300: 30 ms late
    CHECK(RW::Delay(Make(300, 0)) == -1);     // the slot at 300 is the end
}

TEST_CASE("ReactionWake: very short and very long busy times are left to the regular tick", "[ReactionWake]")
{
    CHECK(RW::Delay(Make(RW::kMinBusyMs - 1, 90)) == -1);
    CHECK(RW::Delay(Make(RW::kMaxBusyMs + 1, 90)) == -1);
    CHECK(RW::Delay(Make(0, 90)) == -1);
    CHECK(RW::Delay(Make(-20, 90)) == -1);
}

TEST_CASE("ReactionWake: no second wake inside the minimum gap", "[ReactionWake]")
{
    CHECK(RW::Delay(Make(100, 30, 100, RW::kMinGapMs - 1)) == -1);
    CHECK(RW::Delay(Make(100, 30, 100, RW::kMinGapMs)) == 100);
}

TEST_CASE("ReactionWake: a nonsense period is ignored", "[ReactionWake]")
{
    CHECK(RW::Delay(Make(100, 30, 0)) == -1);
}

TEST_CASE("TickScheduler::wake_after_busy arms a wake only for a Combat bot and counts it", "[ReactionWake]")
{
    AiWorkerPool pool(1);
    TickScheduler sched(pool);
    Ms const now{ 10000 };

    CHECK_FALSE(sched.wake_after_busy(99, 270, now));   // unknown bot

    sched.register_bot(1, ActivityTier::Active);
    CHECK_FALSE(sched.wake_after_busy(1, 270, now));    // not a Combat bot

    sched.register_bot(2, ActivityTier::Combat);        // next tick due now, ticks at +0, +100, +200, +300: 270 ends 30 ms before one
    REQUIRE(sched.wakes_total() == 0);
    CHECK(sched.wake_after_busy(2, 270, now));
    CHECK(sched.wakes_total() == 1);
    CHECK_FALSE(sched.wake_after_busy(2, 270, Ms{ now.count() + 10 }));   // inside the minimum gap
    CHECK(sched.wakes_total() == 1);
}

TEST_CASE("TickScheduler: an armed wake makes the snapshot build due at the wake time, ahead of the regular deadline", "[ReactionWake]")
{
    AiWorkerPool pool(1);
    TickScheduler sched(pool);
    Ms const t{ 20000 };
    // A 3 s floor makes the regular period long; pick a bot whose next regular build is later than t + 300.
    bool found = false;
    for (BotId id = 10; id < 200 && !found; ++id)
    {
        sched.register_bot(id, ActivityTier::Combat);
        sched.set_pvp_floor(id, Ms{ 3000 }, t);
        if (!sched.should_build_snapshot(id, t))
            continue;                                    // first build is due at once; it re-arms within 3 s
        if (sched.should_build_snapshot(id, Ms{ t.count() + 299 }))
            continue;                                    // regular build before the wake: not this bot
        // Nothing is due until the regular deadline, which lies beyond t + 299.
        REQUIRE(sched.wake_after_busy(id, 270, t));
        CHECK_FALSE(sched.should_build_snapshot(id, Ms{ t.count() + 269 }));
        CHECK(sched.should_build_snapshot(id, Ms{ t.count() + 270 }));
        found = true;
    }
    CHECK(found);
}

#endif
