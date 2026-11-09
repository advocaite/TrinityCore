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

#include "../../src/modules/PlayerbotV2/Fleet/PoolBalance.h"

#include <cstdlib>
#include <set>

using namespace Playerbot::V2::Fleet;

TEST_CASE("PoolBalance: levels map to seven brackets, 1-9 included and 60 on its own", "[poolbalance]")
{
    CHECK(PoolBracketOf(1) == 0);
    CHECK(PoolBracketOf(9) == 0);
    CHECK(PoolBracketOf(10) == 1);
    CHECK(PoolBracketOf(19) == 1);
    CHECK(PoolBracketOf(20) == 2);
    CHECK(PoolBracketOf(49) == 4);
    CHECK(PoolBracketOf(50) == 5);
    CHECK(PoolBracketOf(59) == 5);
    CHECK(PoolBracketOf(60) == 6);
    CHECK(PoolBracketLo(0) == 1);
    CHECK(PoolBracketHi(0) == 9);
    CHECK(PoolBracketLo(5) == 50);
    CHECK(PoolBracketHi(5) == 59);
    CHECK(PoolBracketLo(6) == 60);
    CHECK(PoolBracketHi(6) == 60);
    CHECK(PoolBracketHi(2) == 29);
    CHECK(PoolBracketCap(3000) == 429);
    CHECK(PoolBracketCap(1500) == 215);
    CHECK(PoolBracketCap(600) == 86);
}

namespace
{
// Deterministic stand-in for urand: a fixed-stride walk through [0,1).
struct Rolls { double v = 0.0; double next() { v = std::fmod(v + 0.6180339887498949, 1.0); return v; } };
}

TEST_CASE("PoolBalance: only the Horde side of a lopsided pool is created, in proportion to what each bracket lacks", "[poolbalance]")
{
    PoolCounts c;
    c.alliance = { 0, 386, 376, 375, 416, 540, 54 };       // the live census of 69b0d8c2
    c.horde    = { 0, 0, 0, 0, 0, 70, 6 };
    Rolls r;
    std::array<int, kPoolBrackets> made{};
    for (int i = 0; i < 100; ++i)
    {
        SeedChoice const s = PickSeed(c, 50, r.next());
        REQUIRE_FALSE(s.alliance);
        made[s.bracket]++;
        c.horde[s.bracket]++;
    }
    for (std::size_t b = 1; b < 5; ++b)   // bracket 0 is empty on both sides: the ratio rule alone has nothing to compare there
        CHECK(made[b] >= 15);                        // every empty bracket gets Horde at once, not after the 50-60s are done
    CHECK(made[5] >= 10);
}

TEST_CASE("PoolBalance: a lopsided pool is brought to the ratio in every bracket, then grows evenly", "[poolbalance]")
{
    PoolCounts c;
    c.alliance = { 470, 480, 500, 490, 450, 460, 455 };
    c.horde    = { 20, 15, 10, 30, 5, 8, 4 };
    Rolls r;
    std::uint32_t horde_made = 0;
    for (int i = 0; i < 6000; ++i)
    {
        SeedChoice const s = PickSeed(c, 50, r.next());
        if (s.deficit <= 0) break;
        (s.alliance ? c.alliance : c.horde)[s.bracket]++;
        if (!s.alliance) ++horde_made;
    }
    for (std::size_t b = 0; b < kPoolBrackets; ++b)
        CHECK(std::abs(int(c.alliance[b]) - int(c.horde[b])) <= 2);
    CHECK(horde_made > 2000);
}

TEST_CASE("PoolBalance: the target ratio is honoured", "[poolbalance]")
{
    PoolCounts c;
    c.alliance = { 100, 100, 100, 100, 100, 100, 100 };
    Rolls r;
    for (int i = 0; i < 2000; ++i)
    {
        SeedChoice const s = PickSeed(c, 40, r.next());
        if (s.deficit <= 0) break;
        (s.alliance ? c.alliance : c.horde)[s.bracket]++;
    }
    for (std::size_t b = 0; b < kPoolBrackets; ++b)
        CHECK(std::abs(double(c.horde[b]) / double(c.horde[b] + c.alliance[b]) - 0.40) < 0.02);
}

TEST_CASE("PoolBalance: a balanced pool reports nothing lacking", "[poolbalance]")
{
    PoolCounts c;
    c.alliance = { 10, 10, 10, 10, 10, 10, 10 };
    c.horde = c.alliance;
    CHECK(PickSeed(c, 50, 0.5).deficit <= 0);
    PoolAdd(c, false, 33);
    CHECK(c.horde[3] == 11);
}

TEST_CASE("PoolBalance: with a bracket cap the short side of the short bracket is filled first and a full one is never grown", "[poolbalance]")
{
    // cap 100 per bracket, 50/50: each (faction, bracket) targets 50. Alliance is full everywhere and Horde is empty in the 1-9 bracket.
    PoolCounts c;
    c.alliance = { 50, 50, 50, 50, 50, 50, 50 };
    c.horde    = { 0, 50, 50, 50, 50, 50, 50 };
    SeedChoice s = PickSeed(c, 50, 0.3, 100);
    CHECK_FALSE(s.alliance);
    CHECK(s.bracket == 0);
    CHECK(s.deficit == 50);
    // Everything at its target: nothing is created.
    c.horde[0] = 50;
    CHECK(PickSeed(c, 50, 0.3, 100).deficit <= 0);
    // A lopsided bracket (Alliance over its share) is not grown, only the short side.
    c.alliance[3] = 90;
    c.horde[3] = 10;
    s = PickSeed(c, 50, 0.9, 100);
    CHECK_FALSE(s.alliance);
    CHECK(s.bracket == 3);
}

TEST_CASE("PoolBalance: a faction at its share of a bracket has no room, the other faction does", "[poolbalance]")
{
    PoolCounts c;
    c.alliance[0] = 50;
    c.horde[0] = 3;
    CHECK_FALSE(PoolHasRoom(c, 5, true, 100, 50));
    CHECK(PoolHasRoom(c, 5, false, 100, 50));
    CHECK(PoolHasRoom(c, 30, true, 100, 50));   // another bracket is empty
}

TEST_CASE("PoolBalance: every battleground bracket, level 60 alone included, gets a full Alterac Valley side of spare bots on each faction", "[poolbalance]")
{
    // The plain sizes (Fleet/BotSize.h): Low 600, Normal 1500, High 3000. An empty roster is seeded until nothing is lacking; each band
    // from 10-19 to 60 must then hold at least 40 bots per faction (one Alterac Valley side) and the factions must be level.
    for (std::uint32_t reserve : { 600u, 1500u, 3000u })
    {
        PoolCounts c;
        Rolls r;
        std::uint32_t const cap = PoolBracketCap(reserve);
        for (int i = 0; i < 10000; ++i)
        {
            SeedChoice const s = PickSeed(c, 50, r.next(), cap);
            if (s.deficit <= 0) break;
            (s.alliance ? c.alliance : c.horde)[s.bracket]++;
        }
        for (std::size_t b = 1; b < kPoolBrackets; ++b)
        {
            CHECK(c.alliance[b] >= 40);
            CHECK(c.horde[b] >= 40);
            CHECK(c.alliance[b] == c.horde[b]);
        }
    }
}
