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

#include "../../src/modules/PlayerbotV2/Bot/Battleground/BgTactics.h"

using namespace Playerbot::BgTactics;

TEST_CASE("Battleground: a capture is dropped when an enemy is at the banner or the banner is gone", "[battleground]")
{
    CHECK(ShouldAbortCapture(true, false, 0.f) == CaptureAbort::None);
    CHECK(ShouldAbortCapture(true, true, 30.f) == CaptureAbort::None);
    CHECK(ShouldAbortCapture(true, true, 11.f) == CaptureAbort::EnemyNear);
    CHECK(ShouldAbortCapture(true, true, 3.f) == CaptureAbort::EnemyNear);
    CHECK(ShouldAbortCapture(false, true, 3.f) == CaptureAbort::BannerGone);
}

TEST_CASE("Battleground: defenders are only sent when they can arrive before the node flips", "[battleground]")
{
    uint32_t const ab = CaptureWindowMs(3), av = CaptureWindowMs(1);
    CHECK(ab == 60000u);
    CHECK(av == 300000u);
    CHECK(CanArriveBeforeFlip(200.f, ab, 0));           // 200 yd at 14 yd/s = 14 s
    CHECK(!CanArriveBeforeFlip(200.f, ab, 50000));      // 10 s left
    CHECK(CanArriveBeforeFlip(100.f, ab, 50000));       // 7 s needed
    CHECK(!CanArriveBeforeFlip(1.f, ab, 60000));        // already flipped
    CHECK(!CanArriveBeforeFlip(1.f, ab, 90000));
    CHECK(CanArriveBeforeFlip(1000.f, av, 100000));     // AV towers have five minutes
}

TEST_CASE("Battleground: the pickup runner gets two escorts when the team can spare them", "[battleground]")
{
    CHECK(RunnerEscortCount(10) == 2);
    CHECK(RunnerEscortCount(8) == 2);
    CHECK(RunnerEscortCount(5) == 1);
}

TEST_CASE("Battleground: stand-still watchdog fires after a minute without moving", "[battleground]")
{
    StallWatch w;
    CHECK(!w.Update(1000, 10.f, 10.f, false));
    CHECK(!w.Update(30000, 11.f, 10.f, false));
    CHECK(w.Update(61000, 11.f, 11.f, false));
    CHECK(!w.Update(62000, 11.f, 11.f, false));         // fires once per window
    CHECK(w.Update(121000, 11.f, 11.f, false));
    // moving resets
    CHECK(!w.Update(130000, 40.f, 40.f, false));
    CHECK(!w.Update(185000, 40.f, 40.f, false));
    // busy resets
    CHECK(!w.Update(200000, 40.f, 40.f, true));
    CHECK(!w.Update(250000, 40.f, 40.f, false));
    CHECK(w.Update(262000, 40.f, 40.f, false));
}

TEST_CASE("Arathi Basin: a team that owns every node and holds them camps the enemy graveyard", "[battleground][ab]")
{
    CHECK(CampEnemyGraveyard(5, 5, 0));
    CHECK(!CampEnemyGraveyard(5, 5, 1));
    CHECK(!CampEnemyGraveyard(4, 5, 0));
    CHECK(!CampEnemyGraveyard(0, 0, 0));
}

TEST_CASE("Warsong Gulch: the pickup runner rushes the stand only with company", "[battleground][wsg]")
{
    CHECK(RunnerMayRush(0, 0));     // nobody hostile near: run
    CHECK(!RunnerMayRush(0, 3));    // alone against defenders: fight, wait for the escorts
    CHECK(!RunnerMayRush(1, 2));
    CHECK(RunnerMayRush(2, 5));     // two friends beside it
}

TEST_CASE("Warsong Gulch: an escort returns to its runner when it has drifted away", "[battleground][wsg]")
{
    CHECK(!EscortMustReturn(10.f, false));
    CHECK(EscortMustReturn(41.f, false));
    CHECK(!EscortMustReturn(41.f, true));    // an enemy in front of it wins
    CHECK(EscortMustReturn(-1.f, false));    // runner out of sight: follow it
    CHECK(!EscortMustReturn(-1.f, true));
}
