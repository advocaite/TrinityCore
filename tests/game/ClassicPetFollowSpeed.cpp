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

#include "../../src/server/game/Entities/Unit/PetFollowSpeed.h"

TEST_CASE("PetFollowSpeed: a pet trotting after its owner sends far fewer speed packets", "[petfollowspeed]")
{
    // the gap to the owner wobbles between 3 and 9 yards over 100 updates: the wanted rate follows 0.75 + (d - 3) * 0.05, clamped 1..1.3
    float rate = 1.0f;
    int sent = 0, naive = 0;
    float last_naive = 1.0f;
    for (int i = 0; i < 100; ++i)
    {
        float const d = 6.0f + 3.0f * std::sin(float(i) * 0.7f);
        float const wanted = std::min(std::max(1.0f, 0.75f + (d - 3.0f) * 0.05f), 1.3f);
        if (wanted != last_naive) { ++naive; last_naive = wanted; }
        if (PetCatchUpSpeedWorthSending(rate, wanted)) { rate = wanted; ++sent; }
    }
    CHECK(naive > 30);       // a packet on most updates before
    CHECK(sent * 3 < naive);
}

TEST_CASE("PetFollowSpeed: a real speed change always goes out", "[petfollowspeed]")
{
    CHECK(PetCatchUpSpeedWorthSending(1.0f, 1.3f));    // fell behind: full catch-up
    CHECK(PetCatchUpSpeedWorthSending(1.3f, 1.0f));
    CHECK_FALSE(PetCatchUpSpeedWorthSending(1.1f, 1.12f));
}
