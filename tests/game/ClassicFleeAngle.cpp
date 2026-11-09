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

#include "../../src/server/game/Movement/MovementGenerators/ClassicFleeAngle.h"

#include <cmath>

using namespace ClassicFleeAngle;

namespace
{
// Where a step of 'dist' lands, done the way WorldObject::MovePositionToFirstCollision does it (relative angle + facing).
void Step(float x, float y, float orientation, float relativeAngle, float dist, float& outX, float& outY)
{
    float const angle = relativeAngle + orientation;
    outX = x + dist * std::cos(angle);
    outY = y + dist * std::sin(angle);
}

float Dist(float ax, float ay, float bx, float by)
{
    return std::hypot(ax - bx, ay - by);
}
}

TEST_CASE("A feared unit near its attacker runs away, whichever way it faces", "[ClassicFleeAngle]")
{
    // Attacker at the origin, unit 5 yards east of it: away is world angle 0.
    float const ux = 5.0f, uy = 0.0f;
    float const away = std::atan2(uy, ux);
    for (float facing : { 0.0f, 1.0f, Pi / 2.0f, Pi, 4.0f, 3.0f * Pi / 2.0f, 6.2f })
    {
        for (float jitter : { -1.0f, 0.0f, 1.0f })
        {
            float const world = StepWorldAngle(5.0f, away, jitter, 0.0f);
            float nx, ny;
            Step(ux, uy, facing, ToFacingRelative(world, facing), 10.0f, nx, ny);
            INFO("facing " << facing << " jitter " << jitter);
            CHECK(Dist(nx, ny, 0.0f, 0.0f) > Dist(ux, uy, 0.0f, 0.0f) + 8.0f);
        }
    }
}

TEST_CASE("A feared unit far from its attacker drifts back toward it", "[ClassicFleeAngle]")
{
    // Attacker at (10, 10), unit 50 yards north of it.
    float const ax = 10.0f, ay = 10.0f, ux = 10.0f, uy = 60.0f;
    float const away = std::atan2(uy - ay, ux - ax);
    for (float facing : { 0.0f, Pi / 2.0f, Pi, 5.0f })
    {
        for (float jitter : { -1.0f, 0.0f, 1.0f })
        {
            float const world = StepWorldAngle(50.0f, away, jitter, 0.0f);
            float nx, ny;
            Step(ux, uy, facing, ToFacingRelative(world, facing), 10.0f, nx, ny);
            INFO("facing " << facing << " jitter " << jitter);
            CHECK(Dist(nx, ny, ax, ay) < Dist(ux, uy, ax, ay) - 5.0f);
        }
    }
}

TEST_CASE("Inside the quiet band the unit wanders in the rolled world direction", "[ClassicFleeAngle]")
{
    CHECK(StepWorldAngle(35.0f, 1.0f, 1.0f, 2.5f) == Catch::Approx(2.5f));
    float nx, ny;
    Step(0.0f, 0.0f, 4.0f, ToFacingRelative(Pi / 2.0f, 4.0f), 10.0f, nx, ny);
    CHECK(nx == Catch::Approx(0.0f).margin(1e-4));
    CHECK(ny == Catch::Approx(10.0f));
}

TEST_CASE("The relative angle is normalised to [0, 2 pi)", "[ClassicFleeAngle]")
{
    CHECK(ToFacingRelative(0.5f, 1.5f) == Catch::Approx(2.0f * Pi - 1.0f));
    CHECK(ToFacingRelative(7.0f, 0.0f) == Catch::Approx(7.0f - 2.0f * Pi));
    float const r = ToFacingRelative(2.0f * Pi, 0.0f);
    CHECK(r >= 0.0f);
    CHECK(r < 2.0f * Pi);
}
