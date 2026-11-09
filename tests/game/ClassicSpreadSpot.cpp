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

#include "../../src/modules/PlayerbotV2/Bot/World/SpreadSpot.h"

#include <set>
#include <utility>

using namespace Playerbot::V2::World;

namespace
{
SpreadHub Hub(float x, float y, std::uint32_t lo, std::uint32_t hi, std::uint32_t mask, float town_dist, float radius = 30.0f)
{
    SpreadHub h;
    h.x = x; h.y = y; h.z = 300.0f; h.radius = radius; h.min_level = lo; h.max_level = hi; h.faction_mask = mask; h.dist_to_town = town_dist;
    return h;
}
}

TEST_CASE("SpreadSpot: a hub must fit the level, the faction and the zone", "[spreadspot]")
{
    std::vector<SpreadHub> const hubs = {
        Hub(0, 0, 40, 50, 3, 10),        // fits
        Hub(0, 0, 10, 20, 3, 10),        // wrong level
        Hub(0, 0, 40, 50, 2, 10),        // Horde only
        Hub(0, 0, 40, 50, 3, 900),       // another zone
        Hub(0, 0, 40, 50, 4, 10),        // neutral
    };
    CHECK(SpreadHubSuits(hubs[0], 46, true));
    CHECK_FALSE(SpreadHubSuits(hubs[1], 46, true));
    CHECK_FALSE(SpreadHubSuits(hubs[2], 46, true));
    CHECK_FALSE(SpreadHubSuits(hubs[3], 46, true));
    CHECK(SpreadHubSuits(hubs[4], 46, true));
    CHECK(PickSpreadHub({ hubs[1], hubs[2] }, 46, true, 5) == -1);     // nothing fits: the caller keeps the town point
}

TEST_CASE("SpreadSpot: forty bots of one level never share a spot", "[spreadspot]")
{
    std::vector<SpreadHub> const hubs = { Hub(0, 0, 43, 50, 3, 0), Hub(150, 40, 40, 50, 3, 160), Hub(-90, 120, 44, 52, 3, 150) };
    std::set<std::pair<int, int>> spots;
    std::set<int> used;
    for (std::uint64_t seed = 1000; seed < 1040; ++seed)
    {
        int const i = PickSpreadHub(hubs, 46, true, seed);
        REQUIRE(i >= 0);
        used.insert(i);
        float dx, dy;
        SpreadOffset(seed, hubs[i].radius, dx, dy);
        CHECK(std::sqrt(dx * dx + dy * dy) <= kSpreadMaxOffset + 0.01f);
        spots.insert({ int(std::lround((hubs[i].x + dx) * 10)), int(std::lround((hubs[i].y + dy) * 10)) });
    }
    CHECK(used.size() == 3);       // over every hub of the zone
    CHECK(spots.size() == 40);     // not one shared coordinate (to 0.1 yard)
}

TEST_CASE("SpreadSpot: a small hub keeps its crowd on its own ground", "[spreadspot]")
{
    for (std::uint64_t seed = 1; seed < 200; ++seed)
    {
        float dx, dy;
        SpreadOffset(seed, 6.0f, dx, dy);
        CHECK(std::sqrt(dx * dx + dy * dy) <= 3.01f);
    }
}

TEST_CASE("SpreadSpot: bots walking to one single-giver hub aim at different spots, none on the giver (hubs hold two or more givers, so a real radius)", "[spreadspot]")
{
    std::set<std::pair<int, int>> spots;
    for (std::uint64_t seed = 1000; seed < 1040; ++seed)
    {
        float x = 0.f, y = 0.f;
        HubStandPoint(2126.6f, 1305.9f, 20.0f, seed, x, y);
        REQUIRE(std::hypot(x - 2126.6f, y - 1305.9f) >= 0.5f);    // never on the giver's own coordinates
        REQUIRE(std::hypot(x - 2126.6f, y - 1305.9f) <= kSpreadMaxOffset);
        spots.insert({ int(x * 2.0f), int(y * 2.0f) });
    }
    REQUIRE(spots.size() >= 36);                                    // 40 bots, at most a few share a half-yard cell
}

TEST_CASE("GiverStandPoint keeps bots off the giver and spreads them", "[ClassicSpreadSpot]")
{
    using namespace Playerbot::V2::World;
    std::set<std::pair<int, int>> spots;
    for (std::uint64_t seed = 2000; seed < 2040; ++seed)
    {
        float x = 0.f, y = 0.f;
        GiverStandPoint(2126.6f, 1305.9f, 2140.0f, 1290.0f, seed, x, y);
        float const d = std::hypot(x - 2126.6f, y - 1305.9f);
        REQUIRE(d >= 1.95f);
        REQUIRE(d <= 4.05f);                    // inside the 4.5 yd accept reach
        spots.insert({ int(x * 2.0f), int(y * 2.0f) });
    }
    REQUIRE(spots.size() >= 24);
    float ax = 0.f, ay = 0.f, bx = 0.f, by = 0.f;     // the same bot approaching from two nearby positions keeps (nearly) its spot
    GiverStandPoint(0.f, 0.f, 20.0f, 1.0f, 77u, ax, ay);
    GiverStandPoint(0.f, 0.f, 8.0f, 0.5f, 77u, bx, by);
    REQUIRE(std::hypot(ax - bx, ay - by) < 0.01f);
}
