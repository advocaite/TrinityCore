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

#include "../../src/modules/PlayerbotV2/Travel/ReachMemory.h"

using Playerbot::V2::Travel::ReachMemory;
using Playerbot::V2::Travel::QuestGoalMemory;

TEST_CASE("ReachMemory: two failures from one cell to one goal make the goal known unreachable", "[reachmemory]")
{
    ReachMemory m;
    CHECK_FALSE(m.KnownUnreachable(2991, 4000, 1880, 3289, 1758, 1000));
    m.NoteFail(2991, 4000, 1880, 3289, 1758, 1000);
    CHECK_FALSE(m.KnownUnreachable(2991, 4000, 1880, 3289, 1758, 1100));   // one failure can be a bad spot
    m.NoteFail(2991, 4010, 1890, 3290, 1760, 1200);                         // another bot, same cells
    CHECK(m.KnownUnreachable(2991, 4005, 1885, 3288, 1757, 1300));
    CHECK(m.skips() == 1);
}

TEST_CASE("ReachMemory: another start cell, goal cell or map is not blocked", "[reachmemory]")
{
    ReachMemory m;
    m.NoteFail(2991, 4000, 1880, 3289, 1758, 1000);
    m.NoteFail(2991, 4000, 1880, 3289, 1758, 1000);
    CHECK(m.KnownUnreachable(2991, 4000, 1880, 3289, 1758, 1001));
    CHECK_FALSE(m.KnownUnreachable(2991, 4000 + 60, 1880, 3289, 1758, 1001));   // other start cell
    CHECK_FALSE(m.KnownUnreachable(2991, 4000, 1880, 3289 + 30, 1758, 1001));   // other goal cell
    CHECK_FALSE(m.KnownUnreachable(1, 4000, 1880, 3289, 1758, 1001));           // other map
}

TEST_CASE("ReachMemory: a complete path forgets the pair", "[reachmemory]")
{
    ReachMemory m;
    m.NoteFail(0, -8900, 600, -9200, 300, 5000);
    m.NoteFail(0, -8900, 600, -9200, 300, 5000);
    REQUIRE(m.KnownUnreachable(0, -8900, 600, -9200, 300, 5001));
    m.NoteOk(0, -8900, 600, -9200, 300);
    CHECK_FALSE(m.KnownUnreachable(0, -8900, 600, -9200, 300, 5002));
}

TEST_CASE("ReachMemory: a failure is forgotten after the ttl, and the count starts again", "[reachmemory]")
{
    ReachMemory m;
    m.NoteFail(0, 1, 1, 500, 500, 1000);
    m.NoteFail(0, 1, 1, 500, 500, 1000);
    CHECK(m.KnownUnreachable(0, 1, 1, 500, 500, 1000 + ReachMemory::kTtlMs - 1));
    CHECK_FALSE(m.KnownUnreachable(0, 1, 1, 500, 500, 1000 + ReachMemory::kTtlMs));
    m.NoteFail(0, 1, 1, 500, 500, 1000 + ReachMemory::kTtlMs + 5);          // first failure of a new window
    CHECK_FALSE(m.KnownUnreachable(0, 1, 1, 500, 500, 1000 + ReachMemory::kTtlMs + 6));
}

TEST_CASE("ReachMemory: the table never grows past its cap and keeps the newest", "[reachmemory]")
{
    ReachMemory m;
    std::uint32_t t = 1;
    for (int i = 0; i < int(ReachMemory::kCap) + 500; ++i)
    {
        float const gx = float(i) * 30.0f;
        m.NoteFail(0, 0, 0, gx, 0, t);
        m.NoteFail(0, 0, 0, gx, 0, t);
        t += 10;
    }
    CHECK(m.size() <= ReachMemory::kCap);
    float const last = float(ReachMemory::kCap + 499) * 30.0f;
    CHECK(m.KnownUnreachable(0, 0, 0, last, 0, t));
}

TEST_CASE("QuestGoalMemory: two bots giving up on a quest make it known unreachable until the time is up", "[reachmemory]")
{
    QuestGoalMemory m;
    CHECK_FALSE(m.KnownUnreachable(4286, 1000));
    m.NoteDropped(4286, 1000);
    CHECK_FALSE(m.KnownUnreachable(4286, 1100));                         // one bot may be stuck for its own reasons
    m.NoteDropped(4286, 2000);
    CHECK(m.KnownUnreachable(4286, 3000));
    CHECK_FALSE(m.KnownUnreachable(4287, 3000));                         // another quest is not affected
    CHECK_FALSE(m.KnownUnreachable(4286, 2000 + QuestGoalMemory::kTtlMs));
    m.NoteDropped(4286, 2000 + QuestGoalMemory::kTtlMs + 5);             // an old count does not carry over the expiry
    CHECK_FALSE(m.KnownUnreachable(4286, 2000 + QuestGoalMemory::kTtlMs + 10));
    CHECK_FALSE(m.KnownUnreachable(0, 3000));
}
