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

#include "../../src/modules/PlayerbotV2/Fleet/StrandedWatch.h"

using namespace Playerbot::V2::Fleet;

TEST_CASE("A bot in lava is moved at once", "[ClassicStrandedWatch]")
{
    StrandedState st;
    CHECK(StrandedUpdate(st, /*in_magma*/ true, /*path_out*/ true, -4830.f, -1020.f, 380.f, 1000u));
}

TEST_CASE("A bot with no way out is moved on the third look, wherever it stands", "[ClassicStrandedWatch]")
{
    StrandedState st;
    CHECK_FALSE(StrandedUpdate(st, false, false, -4830.f, -1020.f, 380.f, 1000u));
    CHECK_FALSE(StrandedUpdate(st, false, false, -4859.f, -936.f, 429.f, 21000u));   // pacing a ledge counts too
    CHECK(StrandedUpdate(st, false, false, -4850.f, -940.f, 429.f, 41000u));
}

TEST_CASE("A path to the player clears the strikes", "[ClassicStrandedWatch]")
{
    StrandedState st;
    CHECK_FALSE(StrandedUpdate(st, false, false, 0.f, 0.f, 0.f, 1000u));
    CHECK_FALSE(StrandedUpdate(st, false, false, 0.f, 0.f, 0.f, 21000u));
    CHECK_FALSE(StrandedUpdate(st, false, true, 0.f, 0.f, 0.f, 41000u));    // a path exists
    CHECK_FALSE(StrandedUpdate(st, false, false, 0.f, 0.f, 0.f, 61000u));   // strike 1 again, not 3
    CHECK_FALSE(StrandedUpdate(st, false, false, 0.f, 0.f, 0.f, 81000u));
    CHECK(StrandedUpdate(st, false, false, 0.f, 0.f, 0.f, 101000u));
}

TEST_CASE("A bot is looked at once per interval", "[ClassicStrandedWatch]")
{
    StrandedState st;
    CHECK(StrandedDue(st, 0u));
    StrandedUpdate(st, false, true, 0.f, 0.f, 0.f, 5000u);
    CHECK_FALSE(StrandedDue(st, 5000u + kStrandedCheckMs - 1u));
    CHECK(StrandedDue(st, 5000u + kStrandedCheckMs));
}
