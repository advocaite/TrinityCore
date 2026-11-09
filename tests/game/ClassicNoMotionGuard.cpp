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

#include "../../src/modules/PlayerbotV2/Api/NoMotionGuard.h"

using namespace Playerbot::BotMovement;

TEST_CASE("NoMotionGuard: a goal asked for again and again without moving is dropped within a minute", "[nomotion]")
{
    // Florini: the same move every second, the bot never leaves (-6522.1, -1182.3)
    NoMotionGuard g;
    std::uint32_t t = 1000, dropped_at = 0;
    for (int i = 0; i < 120 && !dropped_at; ++i, t += 1000)
        if (!NoMotionAllows(g, 7, -6522.1f, -1182.3f, t))
            dropped_at = t;
    REQUIRE(dropped_at != 0);
    CHECK(dropped_at - 1000 <= 40000u);
}

TEST_CASE("NoMotionGuard: a bot that is walking is never dropped", "[nomotion]")
{
    NoMotionGuard g;
    for (int i = 0; i < 500; ++i)
        REQUIRE(NoMotionAllows(g, 7, float(i) * 4.0f, 0.f, 1000u + std::uint32_t(i) * 1000u));
}

TEST_CASE("NoMotionGuard: a new goal, or real movement, starts the count over", "[nomotion]")
{
    NoMotionGuard g;
    std::uint32_t t = 1000;
    for (int i = 0; i < 30; ++i, t += 1000) NoMotionAllows(g, 7, 0.f, 0.f, t);   // nearly dropped
    CHECK(NoMotionAllows(g, 8, 0.f, 0.f, t));                                   // another goal
    for (int i = 0; i < 4; ++i, t += 1000) CHECK(NoMotionAllows(g, 8, 0.f, 0.f, t));
    CHECK(NoMotionAllows(g, 8, 10.f, 0.f, t));                                  // it moved 10 yards
    CHECK(g.windows == 0);
}
