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

#include "../../src/modules/PlayerbotV2/Bot/Placement.h"

using namespace Playerbot::Placement;

namespace
{
constexpr Shape kAll[] = { Shape::Close, Shape::Circle, Shape::Arc, Shape::Spread, Shape::Line, Shape::Column, Shape::Spear, Shape::Arrow };

float MinGap(std::vector<Slot> const& v)
{
    float best = 1e9f;
    for (size_t i = 0; i < v.size(); ++i)
        for (size_t j = i + 1; j < v.size(); ++j)
            best = std::min(best, std::hypot(v[i].forward - v[j].forward, v[i].left - v[j].left));
    return best;
}
float MaxReach(std::vector<Slot> const& v)
{
    float r = 0.0f;
    for (Slot const& s : v) r = std::max(r, std::hypot(s.forward, s.left));
    return r;
}
}

TEST_CASE("Shape words", "[ClassicPlacement]")
{
    Shape s = Shape::Close;
    CHECK(ParseShape("circle", s)); CHECK(s == Shape::Circle);
    CHECK(ParseShape("far", s));    CHECK(s == Shape::Spread);
    CHECK(ParseShape("spear", s));  CHECK(s == Shape::Spear);
    CHECK(ParseShape("wedge", s));  CHECK(s == Shape::Arrow);
    CHECK(!ParseShape("blob", s));
    for (Shape k : kAll)
    {
        Shape back = Shape::Close;
        CHECK(ParseShape(ShapeName(k), back));
        CHECK(back == k);
    }
    float g = 0.0f;
    CHECK(ParseGap("", g));      CHECK(g == 4.0f);
    CHECK(ParseGap("tight", g)); CHECK(g == 2.5f);
    CHECK(ParseGap("wide", g));  CHECK(g == 8.0f);
    CHECK(!ParseGap("huge", g));
}

TEST_CASE("Every shape gives one spot per bot, apart, near the anchor", "[ClassicPlacement]")
{
    for (Shape k : kAll)
        for (uint32_t n : { 1u, 2u, 3u, 5u, 10u, 25u, 40u })
            for (float gap : { 2.5f, 4.0f, 8.0f })
            {
                INFO(ShapeName(k) << " n " << n << " gap " << gap);
                std::vector<Slot> const v = Slots(k, n, gap);
                REQUIRE(v.size() == n);
                if (n > 1)
                    CHECK(MinGap(v) >= 1.4f);   // nobody on top of another bot
                CHECK(MaxReach(v) <= 60.0f);    // 40 bots on a wide gap still stay within reach of the spot
            }
    CHECK(Slots(Shape::Line, 0, 4.0f).empty());
    CHECK(Slots(Shape::Line, 99, 4.0f).size() == kMaxBots);
}

TEST_CASE("Front shapes put slot 0 in front", "[ClassicPlacement]")
{
    for (Shape k : { Shape::Spear, Shape::Arrow, Shape::Arc })
    {
        std::vector<Slot> const v = Slots(k, 9, 4.0f);
        for (size_t i = 1; i < v.size(); ++i)
            CHECK(v[0].forward >= v[i].forward - 1e-3f);
        CHECK(std::fabs(v[0].left) < 1e-3f);
    }
    // a line runs across the facing, a column along it
    std::vector<Slot> const line = Slots(Shape::Line, 5, 4.0f), col = Slots(Shape::Column, 5, 4.0f);
    for (Slot const& s : line) CHECK(std::fabs(s.forward) < 1e-3f);
    for (Slot const& s : col)  CHECK(std::fabs(s.left) < 1e-3f);
}

TEST_CASE("Circle and arc keep out of a boss's reach", "[ClassicPlacement]")
{
    for (Shape k : { Shape::Circle, Shape::Arc })
        for (Slot const& s : Slots(k, 6, 2.5f, 12.0f))
            CHECK(std::hypot(s.forward, s.left) >= 11.99f);
    for (Slot const& s : Slots(Shape::Arc, 7, 4.0f, 8.0f))
        CHECK(s.forward >= -1e-3f);   // the front half only
}

TEST_CASE("Spread is far apart", "[ClassicPlacement]")
{
    CHECK(MinGap(Slots(Shape::Spread, 16, 4.0f)) >= 9.99f);
    CHECK(MinGap(Slots(Shape::Spread, 40, 2.5f)) >= 5.99f);
}

TEST_CASE("Local slots turn with the facing", "[ClassicPlacement]")
{
    float x = 0, y = 0;
    ToWorld(Slot{ 5.0f, 0.0f }, 100.0f, 200.0f, 0.0f, x, y);
    CHECK(x == Catch::Approx(105.0f)); CHECK(y == Catch::Approx(200.0f));
    ToWorld(Slot{ 5.0f, 0.0f }, 100.0f, 200.0f, 3.14159265f / 2.0f, x, y);
    CHECK(x == Catch::Approx(100.0f).margin(1e-3)); CHECK(y == Catch::Approx(205.0f));
    ToWorld(Slot{ 0.0f, 3.0f }, 0.0f, 0.0f, 0.0f, x, y);   // left of a facing along +x is +y
    CHECK(x == Catch::Approx(0.0f).margin(1e-3)); CHECK(y == Catch::Approx(3.0f));
}

TEST_CASE("Place here never puts a bot on the player", "[ClassicPlacement]")
{
    // owner report 2026-11-04: one bot, Column, Place here: the only slot was the middle, his own spot, and the bot ran onto him.
    for (Shape k : kAll)
        for (uint32_t n : { 1u, 2u, 5u, 23u, 40u })
        {
            INFO(ShapeName(k) << " n " << n);
            std::vector<Slot> v = Slots(k, n, 4.0f);
            if (!RingShape(k))
                PushAhead(v, 4.0f);
            for (Slot const& s : v)
                CHECK(std::hypot(s.forward, s.left) >= 2.99f);
            if (!RingShape(k))
                for (Slot const& s : v)
                    CHECK(s.forward >= 3.99f);
        }
    // a shape already far enough ahead is not moved
    std::vector<Slot> v = { Slot{ 10.0f, 1.0f } };
    PushAhead(v, 4.0f);
    CHECK(v[0].forward == 10.0f);
}
