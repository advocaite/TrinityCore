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

#include "../../src/modules/PlayerbotV2/Fleet/OwnerTrail.h"

using namespace Playerbot::V2::Fleet;

namespace
{
// He runs east along y = 0 from x = 0 to x = 12, jumps up onto a ledge 2 y higher at x = 16, runs on to x = 24.
OwnerTrail RunAndJump(uint32_t& t)
{
    OwnerTrail tr;
    t = 1000u;
    for (float x = 0.f; x <= 12.f; x += 1.f, t += 100u)
        TrailRecord(tr, x, 0.f, 0.f, false, true, t);
    TrailRecord(tr, 13.5f, 0.f, 1.5f, true, true, t); t += 100u;   // in the air
    TrailRecord(tr, 15.f, 0.f, 2.5f, true, true, t); t += 100u;
    TrailRecord(tr, 16.f, 0.f, 2.f, false, true, t); t += 100u;    // down on the ledge
    for (float x = 17.f; x <= 24.f; x += 1.f, t += 100u)
        TrailRecord(tr, x, 0.f, 2.f, false, true, t);
    return tr;
}
}

TEST_CASE("His way keeps a step every few yards and the jump with its take-off", "[ClassicOwnerTrail]")
{
    uint32_t t;
    OwnerTrail tr = RunAndJump(t);
    REQUIRE(tr.steps.size() >= 6);
    int jumps = 0;
    for (size_t i = 0; i < tr.steps.size(); ++i)
        if (tr.steps[i].jump)
        {
            ++jumps;
            REQUIRE(i > 0);
            CHECK(tr.steps[i - 1].x == 12.f);    // took off where he last stood
            CHECK(tr.steps[i].x == 16.f);
            CHECK(tr.steps[i].z == 2.f);
            CHECK(tr.steps[i].air_ms == 200u);
        }
    CHECK(jumps == 1);
}

TEST_CASE("A teleport or a taxi forgets the way", "[ClassicOwnerTrail]")
{
    uint32_t t;
    OwnerTrail tr = RunAndJump(t);
    TrailRecord(tr, 500.f, 0.f, 2.f, false, true, t);
    CHECK(tr.steps.size() == 1);
    tr = RunAndJump(t);
    TrailRecord(tr, 25.f, 0.f, 2.f, false, false, t);   // on a flight
    CHECK(tr.steps.empty());
}

TEST_CASE("A bot by his way joins it only with a jump still ahead", "[ClassicOwnerTrail]")
{
    uint32_t t;
    OwnerTrail tr = RunAndJump(t);
    CHECK(TrailJoin(tr, 10.f, 1.f, 0.f) >= 0);       // under the ledge: the jump is ahead
    CHECK(TrailJoin(tr, 20.f, 0.f, 2.f) < 0);        // on the ledge already: the follow walks
    CHECK(TrailJoin(tr, 10.f, 20.f, 0.f) < 0);       // nowhere near his way
}

TEST_CASE("The bot walks his steps, makes the jump from the take-off and is done at the end", "[ClassicOwnerTrail]")
{
    uint32_t t;
    OwnerTrail tr = RunAndJump(t);
    TrailWalk w;
    float x = 9.f, y = 0.5f, z = 0.f;
    TrailBegin(tr, w, TrailJoin(tr, x, y, z), t);
    REQUIRE(w.active);
    bool jumped = false, done = false;
    for (int i = 0; i < 200 && !done; ++i, t += 100u)
    {
        TrailStep target;
        TrailAction const a = TrailStepFor(tr, w, x, y, z, t, target);
        CHECK(a != TrailAction::Lost);
        if (a == TrailAction::Walk || a == TrailAction::Jump)
        {
            if (a == TrailAction::Jump)
            {
                CHECK(x == 12.f);    // from the take-off
                jumped = true;
            }
            x = target.x; y = target.y; z = target.z;   // the move gets there
        }
        done = a == TrailAction::Done;
    }
    CHECK(jumped);
    CHECK(done);
}

TEST_CASE("A bot that does not get to its step, or came down short, has lost the way", "[ClassicOwnerTrail]")
{
    uint32_t t;
    OwnerTrail tr = RunAndJump(t);
    TrailWalk w;
    TrailBegin(tr, w, TrailJoin(tr, 9.f, 0.f, 0.f), t);
    TrailStep target;
    TrailAction a = TrailAction::None;
    for (uint32_t i = 0; i <= kTrailStepMaxMs / 100u + 1u; ++i, t += 100u)
        a = TrailStepFor(tr, w, 9.f, 0.f, 0.f, t, target);   // stands still
    CHECK(a == TrailAction::Lost);
    CHECK_FALSE(w.active);

    // Walked to the take-off, jumped, came down under the ledge instead.
    auto ground = [](TrailStep const& s) { return s.z < 1.f; };   // under the ledge only the ground walks
    CHECK(TrailJoin(tr, 12.f, 0.f, 0.f) < 0);                      // without the walking check it would pick the ledge
    TrailBegin(tr, w, TrailJoin(tr, 12.f, 0.f, 0.f, ground), t);
    a = TrailStepFor(tr, w, 12.f, 0.f, 0.f, t, target);
    CHECK(a == TrailAction::Jump);
    t += 2000u;
    a = TrailStepFor(tr, w, 14.f, 0.f, 0.f, t, target);
    CHECK(a == TrailAction::Lost);
}
