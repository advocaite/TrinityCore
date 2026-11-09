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

#include "../../src/modules/PlayerbotV2/Bot/TownErrand.h"

#include <cmath>
#include <set>

using namespace Playerbot::V2::TownErrand;

TEST_CASE("A place takes its cap of bots and no more; a lapsed claim frees the slot", "[ClassicTownErrand]")
{
    Spots spots;
    std::uint64_t const inn = Spots::PlaceKey(0, -8867.f, 673.f);
    std::set<std::uint32_t> slots;
    for (std::uint64_t bot = 1; bot <= 4; ++bot)
    {
        std::uint32_t slot = 99;
        REQUIRE(spots.Claim(inn, bot, 1000u, 30000u, 4u, &slot));
        slots.insert(slot);
    }
    REQUIRE(slots.size() == 4);                                    // each in its own slot
    REQUIRE_FALSE(spots.Claim(inn, 5, 1000u, 30000u, 4u));         // the fifth is turned away
    REQUIRE(spots.Full(inn, 5, 1000u, 4u));
    REQUIRE_FALSE(spots.Full(inn, 2, 1000u, 4u));                  // a holder is never refused its own place
    REQUIRE(spots.Claim(inn, 2, 2000u, 30000u, 4u));               // renewing
    spots.Release(inn, 3);
    std::uint32_t slot = 99;
    REQUIRE(spots.Claim(inn, 5, 3000u, 30000u, 4u, &slot));        // the freed place goes to the next
    REQUIRE(spots.Count(inn, 3000u) == 4);
    // Nobody renews: all lapse, the place is empty again.
    REQUIRE(spots.Count(inn, 40000u) == 0);
    REQUIRE(spots.Claim(inn, 9, 40000u, 30000u, 4u, &slot));
    REQUIRE(slot == 0);
}

TEST_CASE("The bankers of one bank are one place, the bank and the auction house are two", "[ClassicTownErrand]")
{
    REQUIRE(Spots::PlaceKey(0, -8925.f, 613.f) == Spots::PlaceKey(0, -8921.f, 615.f));
    REQUIRE(Spots::PlaceKey(0, -8925.f, 613.f) != Spots::PlaceKey(0, -8815.f, 660.f));
    REQUIRE(Spots::PlaceKey(0, 10.f, 10.f) != Spots::PlaceKey(1, 10.f, 10.f));
}

TEST_CASE("The pick crosses the city, skips full and recent places", "[ClassicTownErrand]")
{
    std::vector<Dest> d = {
        { Kind::Vendor,  1, 0, 10.f, 0.f, 0.f, BaseWeight(Kind::Vendor) },    // next door
        { Kind::Bank,    2, 0, 150.f, 0.f, 0.f, BaseWeight(Kind::Bank) },
        { Kind::Auction, 3, 0, 0.f, 200.f, 0.f, BaseWeight(Kind::Auction) },
        { Kind::Flight,  4, 0, 900.f, 0.f, 0.f, BaseWeight(Kind::Flight) },   // beyond the radius
    };
    Recent recent;
    auto none_full = [](Dest const&) { return false; };
    int counts[4] = {};
    for (int i = 0; i < 1000; ++i)
    {
        int const k = Pick(d, 0.f, 0.f, 300.f, recent, float(i) / 1000.f, none_full);
        REQUIRE(k >= 0);
        ++counts[k];
    }
    REQUIRE(counts[3] == 0);
    REQUIRE(counts[0] < counts[1] / 5);   // next door is rare
    REQUIRE(counts[1] > 0);
    REQUIRE(counts[2] > 0);

    recent.add(2);
    auto bank_full = [](Dest const& x) { return x.key == 3; };
    for (int i = 0; i < 100; ++i)
    {
        int const k = Pick(d, 0.f, 0.f, 300.f, recent, float(i) / 100.f, bank_full);
        REQUIRE(k == 0);   // the bank was just visited, the auction house is full: only the vendor is left
    }
    recent.add(1);
    REQUIRE(Pick(d, 0.f, 0.f, 300.f, recent, 0.5f, bank_full) == -1);
}

TEST_CASE("Bots at one place stand side by side on the near side", "[ClassicTownErrand]")
{
    std::set<std::pair<int, int>> seen;
    for (std::uint32_t slot = 0; slot < 5; ++slot)
    {
        float sx, sy;
        StandPoint(0.f, 0.f, 50.f, 0.f, 4.f, slot, sx, sy);
        REQUIRE(std::hypot(sx, sy) == Catch::Approx(4.f).margin(0.01));
        REQUIRE(sx > 0.f);   // the side the bot comes from
        seen.insert({ int(std::lround(sx * 10.f)), int(std::lround(sy * 10.f)) });
    }
    REQUIRE(seen.size() == 5);
}

TEST_CASE("A walk that stops getting closer is stalled after the window", "[ClassicTownErrand]")
{
    State st;
    REQUIRE_FALSE(Stalled(st, 0.f, 0.f, 1000u));
    REQUIRE_FALSE(Stalled(st, 1.f, 0.f, 5000u));
    REQUIRE_FALSE(Stalled(st, 5.f, 0.f, 8000u));    // 5 y: progress, the clock restarts
    REQUIRE_FALSE(Stalled(st, 5.5f, 0.f, 15000u));
    REQUIRE(Stalled(st, 5.5f, 0.f, 16001u));
}
