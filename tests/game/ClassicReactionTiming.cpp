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

#include "../../src/modules/PlayerbotV2/Bot/ReactionTiming.h"

using Playerbot::ReactionTiming;

TEST_CASE("ReactionTiming: the first cast after the GCD end is the sample", "[ReactionTiming]")
{
    ReactionTiming t;
    t.NoteGcdReady(1, 1000);
    t.OnCast(1, 1060);
    REQUIRE(t.gcd().n == 1);
    CHECK(t.gcd().sum == 60);
    CHECK(t.gcd().bucket[1] == 1);       // <=100
    t.OnCast(1, 1200);                   // no pending GCD end: not a sample
    CHECK(t.gcd().n == 1);
}

TEST_CASE("ReactionTiming: a cast before the GCD ends is off-GCD and leaves the sample pending", "[ReactionTiming]")
{
    ReactionTiming t;
    t.NoteGcdReady(1, 1000);
    t.OnCast(1, 900);                    // a trinket, a kick
    CHECK(t.gcd().n == 0);
    t.OnCast(1, 1030);
    CHECK(t.gcd().n == 1);
    CHECK(t.gcd().sum == 30);
}

TEST_CASE("ReactionTiming: a long gap is a bot with nothing to cast, counted apart", "[ReactionTiming]")
{
    ReactionTiming t;
    t.NoteGcdReady(1, 1000);
    t.OnCast(1, 1000 + ReactionTiming::kGcdIdleMs + 1);
    CHECK(t.gcd().n == 0);
    CHECK(t.gcd_idle() == 1);
}

TEST_CASE("ReactionTiming: a newer GCD end replaces the pending one", "[ReactionTiming]")
{
    ReactionTiming t;
    t.NoteGcdReady(1, 1000);
    t.NoteGcdReady(1, 1500);             // a later snapshot: the cast in between started a new GCD
    t.OnCast(1, 1520);
    REQUIRE(t.gcd().n == 1);
    CHECK(t.gcd().sum == 20);
}

TEST_CASE("ReactionTiming: GCD samples are kept apart by role", "[ReactionTiming]")
{
    ReactionTiming t;
    t.NoteGcdReady(1, 1000, 1);   // healer
    t.NoteGcdReady(2, 1000, 2);   // dps
    t.NoteGcdReady(3, 1000);      // no role: other
    t.OnCast(1, 1300);
    t.OnCast(2, 1040);
    t.OnCast(3, 1100);
    CHECK(t.gcd().n == 3);
    CHECK(t.gcd_role(1).sum == 300);
    CHECK(t.gcd_role(2).sum == 40);
    CHECK(t.gcd_role(3).sum == 100);
    CHECK(t.gcd_role(0).n == 0);
    t.NoteGcdReady(1, 5000, 1);
    t.OnCast(1, 5000 + ReactionTiming::kGcdIdleMs + 5);
    CHECK(t.gcd_idle() == 1);
    CHECK(t.gcd_idle_role(1) == 1);
    t.ResetWindow();
    CHECK(t.gcd_role(1).n == 0);
    CHECK(t.gcd_idle_role(1) == 0);
}

TEST_CASE("ReactionTiming: the kick window keeps its first time and ends with the kick", "[ReactionTiming]")
{
    ReactionTiming t;
    t.NoteKickPossible(5, true, 2000);
    t.NoteKickPossible(5, true, 2100);   // still possible: the window stays
    t.OnKick(5, 2340);
    REQUIRE(t.kick().n == 1);
    CHECK(t.kick().sum == 340);
    t.OnKick(5, 2400);                   // no window: not a sample
    CHECK(t.kick().n == 1);
}

TEST_CASE("ReactionTiming: a window that closes is dropped, and a new one starts fresh", "[ReactionTiming]")
{
    ReactionTiming t;
    t.NoteKickPossible(5, true, 2000);
    t.NoteKickPossible(5, false, 2100);
    t.OnKick(5, 2200);
    CHECK(t.kick().n == 0);
    t.NoteKickPossible(5, true, 3000);
    t.OnKick(5, 3100);
    CHECK(t.kick().sum == 100);
}

TEST_CASE("ReactionTiming: hazard order to first step", "[ReactionTiming]")
{
    ReactionTiming t;
    t.OnMove(7, 1000);                   // no order: nothing
    CHECK(t.hazard().n == 0);
    t.NoteHazardOrder(7, true, 1000);
    t.NoteHazardOrder(7, true, 1300);    // still the same order
    t.OnMove(7, 1450);
    REQUIRE(t.hazard().n == 1);
    CHECK(t.hazard().sum == 450);
}

TEST_CASE("ReactionTiming: a stale hazard order is dropped when it is gone", "[ReactionTiming]")
{
    ReactionTiming t;
    t.NoteHazardOrder(7, true, 1000);
    t.NoteHazardOrder(7, false, 1000 + ReactionTiming::kHazardMaxMs + 1);
    t.OnMove(7, 1000 + ReactionTiming::kHazardMaxMs + 2);
    CHECK(t.hazard().n == 0);
}

TEST_CASE("ReactionTiming: histogram buckets and the line", "[ReactionTiming]")
{
    ReactionTiming::Hist h;
    for (uint32_t ms : { 10u, 50u, 51u, 100u, 150u, 250u, 500u, 501u })
        h.add(ms);
    CHECK(h.n == 8);
    CHECK(h.max == 501);
    CHECK(h.bucket[0] == 2);   // 10, 50
    CHECK(h.bucket[1] == 2);   // 51, 100
    CHECK(h.bucket[2] == 1);   // 150
    CHECK(h.bucket[3] == 1);   // 250
    CHECK(h.bucket[4] == 1);   // 500
    CHECK(h.bucket[5] == 1);   // 501
    CHECK(h.line().find("n=8") == 0);
    CHECK(ReactionTiming::Hist{}.line() == "n=0");
}

TEST_CASE("ReactionTiming: flush is due once a minute and Forget clears a bot", "[ReactionTiming]")
{
    ReactionTiming t;
    CHECK_FALSE(t.FlushDue(1000));       // the first call starts the window
    CHECK_FALSE(t.FlushDue(1000 + ReactionTiming::kFlushMs - 1));
    CHECK(t.FlushDue(1000 + ReactionTiming::kFlushMs));
    CHECK_FALSE(t.FlushDue(1000 + ReactionTiming::kFlushMs + 1));
    t.NoteGcdReady(1, 10);
    t.Forget(1);
    t.OnCast(1, 20);
    CHECK(t.gcd().n == 0);
}

#endif
