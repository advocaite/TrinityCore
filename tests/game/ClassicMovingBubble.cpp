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
#include "tc_catch2.h"

#include "../../src/modules/PlayerbotV2/Fleet/MovingBubble.h"

using namespace Playerbot::V2::Fleet;

namespace
{
// A rider going east at `speed` y/s, after `secs` seconds of half-second ticks.
Motion Ride(float speed, int secs, float min_speed = 9.0f)
{
    Motion m;
    for (int i = 0; i < secs * 2; ++i)
        m = StepMotion(m, speed * 0.5f, 0.0f, 500, min_speed);
    return m;
}
}

TEST_CASE("A mounted player counts as moving, a running one does not", "[ClassicMovingBubble]")
{
    CHECK(Ride(11.2f, 4).moving);    // 60% mount
    CHECK(Ride(14.0f, 4).moving);    // 100% mount
    CHECK_FALSE(Ride(7.0f, 10).moving);   // on foot
    CHECK_FALSE(Ride(11.2f, 4).Speed() < 10.0f);
}

TEST_CASE("Moving stops below 60% of the threshold, not at once", "[ClassicMovingBubble]")
{
    Motion m = Ride(11.2f, 4);
    m = StepMotion(m, 0.0f, 0.0f, 500, 9.0f);   // one frame stopped
    CHECK(m.moving);
    for (int i = 0; i < 8; ++i)
        m = StepMotion(m, 0.0f, 0.0f, 500, 9.0f);
    CHECK_FALSE(m.moving);
}

TEST_CASE("A teleport resets the motion", "[ClassicMovingBubble]")
{
    Motion m = Ride(11.2f, 4);
    m = StepMotion(m, 800.0f, 0.0f, 500, 9.0f);
    CHECK_FALSE(m.moving);
    CHECK(m.Speed() == 0.0f);
}

TEST_CASE("Standing, the bubble is the circle", "[ClassicMovingBubble]")
{
    MovingRule const r;
    Motion const still;
    CHECK(InBubble(0, 0, still, -500, 0, 600, r));
    CHECK(InBubble(0, 0, still, 0, 590, 600, r));
    CHECK_FALSE(InBubble(0, 0, still, 0, 610, 600, r));
}

TEST_CASE("Riding, bots behind him and off his road do not count", "[ClassicMovingBubble]")
{
    MovingRule const r;
    Motion const m = Ride(11.2f, 4);
    CHECK(InBubble(0, 0, m, 400, 0, 600, r));      // ahead
    CHECK(InBubble(0, 0, m, -100, 50, 600, r));    // just behind
    CHECK_FALSE(InBubble(0, 0, m, -300, 0, 600, r));   // left behind
    CHECK_FALSE(InBubble(0, 0, m, 100, 400, 600, r));  // off to the side
    CHECK_FALSE(InBubble(0, 0, m, 700, 0, 600, r));    // beyond the radius
}

TEST_CASE("Lead spots lie ahead, beyond the minimum, inside the radius and the corridor", "[ClassicMovingBubble]")
{
    MovingRule const r;
    Motion const m = Ride(11.2f, 4);
    float lo = 0, hi = 0;
    LeadRange(m, 600, r, lo, hi);
    CHECK(lo == 130.0f);
    CHECK(hi > 250.0f);
    CHECK(hi <= 510.0f);
    for (float ua : { 0.0f, 0.5f, 0.999f })
        for (float us : { 0.0f, 0.5f, 0.999f })
        {
            float x = 0, y = 0;
            LeadSpot(100, 200, m, 600, r, ua, us, x, y);
            CHECK(x - 100 >= lo - 0.01f);
            CHECK(x - 100 <= hi + 0.01f);
            CHECK(std::fabs(y - 200) <= r.corridor_yards * 0.6f + 0.01f);
            CHECK(InBubble(100, 200, m, x, y, 600, r));
        }
}

TEST_CASE("Lead spots follow the heading", "[ClassicMovingBubble]")
{
    MovingRule const r;
    Motion m;
    for (int i = 0; i < 8; ++i)
        m = StepMotion(m, 0.0f, -6.0f, 500, 9.0f);   // riding south at 12 y/s
    REQUIRE(m.moving);
    float x = 0, y = 0;
    LeadSpot(0, 0, m, 600, r, 0.5f, 0.5f, x, y);
    CHECK(y < -130.0f);
    CHECK(std::fabs(x) < 1.0f);
}

TEST_CASE("Left behind only when moving and past the distance", "[ClassicMovingBubble]")
{
    Motion const m = Ride(11.2f, 4);
    CHECK(LeftBehind(0, 0, m, -500, 0, 450));
    CHECK_FALSE(LeftBehind(0, 0, m, -400, 0, 450));
    CHECK_FALSE(LeftBehind(0, 0, m, -500, 0, 0));
    CHECK_FALSE(LeftBehind(0, 0, Motion{}, -500, 0, 450));
}
