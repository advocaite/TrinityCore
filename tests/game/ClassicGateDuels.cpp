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

#include "../../src/modules/PlayerbotV2/Fleet/GateDuels.h"

#include <set>

using namespace Playerbot::V2::GateDuels;

TEST_CASE("Pairs are close in level, healthy, and never share a bot", "[ClassicGateDuels]")
{
    std::vector<Candidate> c;
    for (std::uint64_t id = 1; id <= 20; ++id)
        c.push_back({ id, std::uint32_t(20 + id * 2), 100 });   // levels 22..60 step 2
    for (std::uint32_t seed = 0; seed < 50; ++seed)
    {
        std::vector<Pair> const p = PickPairs(c, 3, seed);
        CHECK(p.size() == 3);
        std::set<std::uint64_t> seen;
        for (Pair const& pr : p)
        {
            CHECK(pr.challenger < pr.other);
            CHECK(seen.insert(pr.challenger).second);
            CHECK(seen.insert(pr.other).second);
            int const gap = std::abs(int(20 + pr.challenger * 2) - int(20 + pr.other * 2));
            CHECK(gap <= kMaxLevelGap);
        }
    }
}

TEST_CASE("Hurt bots, low levels and levels too far apart are not paired", "[ClassicGateDuels]")
{
    CHECK(PickPairs({ { 1, 30, 100 }, { 2, 30, 89 } }, 2, 0).empty());     // one is hurt
    CHECK(PickPairs({ { 1, 5, 100 }, { 2, 5, 100 } }, 2, 0).empty());      // below level 6
    CHECK(PickPairs({ { 1, 30, 100 }, { 2, 34, 100 } }, 2, 0).empty());    // 4 levels apart
    CHECK(PickPairs({ { 1, 30, 100 }, { 2, 33, 100 } }, 2, 0).size() == 1);
    CHECK(PickPairs({ { 1, 30, 100 } }, 2, 0).empty());                    // one bot is no pair
    CHECK(PickPairs({ { 1, 30, 100 }, { 2, 30, 100 } }, 0, 0).empty());    // none wanted
}

TEST_CASE("The pairs change with the seed, so the duellists rotate", "[ClassicGateDuels]")
{
    std::vector<Candidate> c;
    for (std::uint64_t id = 1; id <= 30; ++id)
        c.push_back({ id, 40, 100 });
    std::set<std::uint64_t> challengers;
    for (std::uint32_t seed = 0; seed < 20; ++seed)
        for (Pair const& pr : PickPairs(c, 1, seed)) { challengers.insert(pr.challenger); challengers.insert(pr.other); }
    CHECK(challengers.size() > 12);
}

TEST_CASE("A player's duel is taken within 10 levels", "[ClassicGateDuels]")
{
    CHECK(PlayerDuelOk(30, 30));
    CHECK(PlayerDuelOk(30, 40));
    CHECK(PlayerDuelOk(40, 30));
    CHECK_FALSE(PlayerDuelOk(30, 41));
    CHECK_FALSE(PlayerDuelOk(60, 49));
}

TEST_CASE("The gate is the road point past the city edge", "[ClassicGateDuels]")
{
    // a road running east, the city ends after x = 30
    std::vector<PathPt> path;
    for (int i = 0; i <= 10; ++i)
        path.push_back({ float(i * 10), 0.0f, 5.0f + float(i), i * 10 <= 30 });
    float x = 0, y = 0, z = 0;
    REQUIRE(GateFromPath(path, 14.0f, x, y, z));
    CHECK(x == 60.0f);   // first outside point is x=40; two 10-yard steps pass 14 yards
    CHECK(z == 11.0f);
    // fewer yards left on the path than asked for: the end of the path
    REQUIRE(GateFromPath(path, 500.0f, x, y, z));
    CHECK(x == 100.0f);
    // never leaves the city / starts outside / too short
    std::vector<PathPt> in(path);
    for (PathPt& p : in) p.inside = true;
    CHECK_FALSE(GateFromPath(in, 14.0f, x, y, z));
    std::vector<PathPt> out(path);
    out.front().inside = false;
    CHECK_FALSE(GateFromPath(out, 14.0f, x, y, z));
    CHECK_FALSE(GateFromPath({ { 0, 0, 0, true } }, 14.0f, x, y, z));
}

TEST_CASE("The slots of a gate stand apart and the duellists face each other", "[ClassicGateDuels]")
{
    Layout l[3] = { LayoutFor(100.0f, 200.0f, 0), LayoutFor(100.0f, 200.0f, 1), LayoutFor(100.0f, 200.0f, 2) };
    for (Layout const& k : l)
        CHECK(std::hypot(k.a.x - k.b.x, k.a.y - k.b.y) == Catch::Approx(kFighterGap).margin(0.01));
    for (int i = 0; i < 3; ++i)
        for (int j = i + 1; j < 3; ++j)
        {
            float const cix = (l[i].a.x + l[i].b.x) / 2, ciy = (l[i].a.y + l[i].b.y) / 2;
            float const cjx = (l[j].a.x + l[j].b.x) / 2, cjy = (l[j].a.y + l[j].b.y) / 2;
            CHECK(std::hypot(cix - cjx, ciy - cjy) > 15.0f);
        }
    // everything stays within the gate's surroundings
    for (Layout const& k : l)
        for (Pt const& p : { k.a, k.b, k.onlooker[0], k.onlooker[1] })
            CHECK(std::hypot(p.x - 100.0f, p.y - 200.0f) < 40.0f);
}

TEST_CASE("The board holds an assignment until it is cleared", "[ClassicGateDuels]")
{
    Board& b = Board::Get();
    b.ClearAll();
    Assignment a;
    CHECK_FALSE(b.Find(7, a));
    b.Set(7, Assignment{ 3, Role::Challenger, 9, 1.0f, 2.0f, 3.0f });
    REQUIRE(b.Find(7, a));
    CHECK(a.partner == 9);
    CHECK(a.role == Role::Challenger);
    b.Clear(7);
    CHECK_FALSE(b.Find(7, a));
}

TEST_CASE("A gate is near only on its map, within the radius, for its faction", "[ClassicGateDuels]")
{
    Board& b = Board::Get();
    b.ClearAll();
    b.SetGate(0, GateSpot{ true, 0, 100.0f, 100.0f, 10.0f, true });
    CHECK(b.NearGate(0, 130.0f, 100.0f, 80.0f, true));
    CHECK_FALSE(b.NearGate(0, 300.0f, 100.0f, 80.0f, true));
    CHECK_FALSE(b.NearGate(1, 130.0f, 100.0f, 80.0f, true));
    CHECK_FALSE(b.NearGate(0, 130.0f, 100.0f, 80.0f, false));
    b.ClearGate(0);
    CHECK_FALSE(b.NearGate(0, 130.0f, 100.0f, 80.0f, true));
}

TEST_CASE("A player is challenged once per cooldown, whichever bot asks", "[ClassicGateDuels]")
{
    Board& b = Board::Get();
    b.ClearAll();
    constexpr std::uint32_t cd = 600000u;
    CHECK(b.ChallengeTake(42, 1000u, cd));
    CHECK_FALSE(b.ChallengeTake(42, 1000u + 599000u, cd));
    CHECK(b.ChallengeTake(43, 1000u + 599000u, cd));          // another player is free
    CHECK(b.ChallengeTake(42, 1000u + cd, cd));               // the cooldown is over
}

TEST_CASE("Every city has distinct, plausible data", "[ClassicGateDuels]")
{
    std::set<std::uint32_t> zones;
    for (City const& c : kCities)
    {
        CHECK(zones.insert(c.zone).second);
        CHECK(std::hypot(c.cx - c.tx, c.cy - c.ty) > 100.0f);   // the road target is outside the city
        CHECK(c.name != nullptr);
    }
    CHECK(kCityCount == 6);
}
