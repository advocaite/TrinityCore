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

#include "../../src/modules/PlayerbotV2/Bot/MeleeReach.h"

using namespace Playerbot::MeleeReach;

TEST_CASE("MeleeReach: small mobs keep the nominal 5 y", "[meleereach]")
{
    CHECK(Of(0.0f) == Catch::Approx(5.0f));
    CHECK(Of(1.5f) == Catch::Approx(5.0f));   // a humanoid: 1.5 + 1.5 + 4/3 - 0.5 = 3.83, under the nominal range
    CHECK(InReach(4.9f * 4.9f, 1.5f));
    CHECK_FALSE(InReach(5.1f * 5.1f, 1.5f));
}

TEST_CASE("MeleeReach: a big creature is in reach from further out", "[meleereach]")
{
    // UBRS 2026-10-27: the rogue swinging at a dragon from 6-8 y counted as out of melee, stopped on the companion leash and stood.
    float const r = Of(6.0f);   // 1.5 + 6 + 4/3 - 0.5
    CHECK(r == Catch::Approx(8.3333f).margin(0.001f));
    CHECK(InReach(7.5f * 7.5f, 6.0f));
    CHECK_FALSE(InReach(9.0f * 9.0f, 6.0f));
    CHECK(Of(-1.0f) == Catch::Approx(5.0f));   // a bad reach never shrinks it
}
