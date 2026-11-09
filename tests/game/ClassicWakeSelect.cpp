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

#include "../../src/modules/PlayerbotV2/Fleet/WakeSelect.h"

using namespace Playerbot::V2::Fleet;

namespace
{
constexpr uint32_t STORMWIND = 1519, ELWYNN = 12;

WakeCandidate C(uint64_t guid, float dist, uint32_t zone, bool faction_ok = true)
{
    WakeCandidate c;
    c.guid = guid; c.dist = dist; c.zone = zone; c.faction_ok = faction_ok;
    return c;
}
}

TEST_CASE("WakeSelect distance window and faction", "[wake]")
{
    WakeRule r;   // not a capital
    CHECK_FALSE(WakeEligible(C(1, 59.9f, ELWYNN), r));      // beside the player: would pop into view
    CHECK(WakeEligible(C(1, 60.0f, ELWYNN), r));
    CHECK(WakeEligible(C(1, 450.0f, ELWYNN), r));
    CHECK_FALSE(WakeEligible(C(1, 450.1f, ELWYNN), r));
    CHECK_FALSE(WakeEligible(C(1, 200.0f, ELWYNN, false), r));
}

TEST_CASE("WakeSelect in a capital takes only the capital's own zone", "[wake]")
{
    WakeRule r;
    r.capital = true; r.anchor_zone = STORMWIND;
    std::vector<WakeCandidate> in = { C(1, 100, STORMWIND), C(2, 120, ELWYNN), C(3, 300, STORMWIND), C(4, 90, ELWYNN) };
    auto const out = SelectWake(in, r, 10);
    REQUIRE(out.size() == 2);
    CHECK(out[0] == 1);
    CHECK(out[1] == 3);

    // outside a capital the zone does not matter
    r.capital = false;
    CHECK(SelectWake(in, r, 10).size() == 4);
}

TEST_CASE("WakeSelect never wakes a sleeper that lies in water", "[wake]")
{
    WakeRule r;
    WakeCandidate wet = C(1, 100, STORMWIND);
    wet.wet = true;                       // logged out in a canal on an earlier visit
    CHECK_FALSE(WakeEligible(wet, r));
    CHECK(SelectWake({ wet, C(2, 100, STORMWIND) }, r, 5) == std::vector<uint64_t>{ 2 });
}

TEST_CASE("WakeSelect never wakes more than wanted", "[wake]")
{
    WakeRule r;
    std::vector<WakeCandidate> in;
    for (uint64_t i = 1; i <= 50; ++i) in.push_back(C(i, 100, ELWYNN));
    CHECK(SelectWake(in, r, 0).empty());
    CHECK(SelectWake(in, r, 7).size() == 7);
    CHECK(SelectWake(in, r, 500).size() == 50);
}

// ---- fair share across the zones around the player (SelectWakeShared) ----

TEST_CASE("Waking holds the player's zone near its share and spreads the rest over the neighbours", "[ClassicWakeSelect]")
{
    using namespace Playerbot::V2::Fleet;
    WakeRule r;
    r.anchor_zone = 15;
    r.min_dist = 0.0f;
    std::vector<WakeCandidate> c;
    for (uint32_t i = 0; i < 40; ++i)
    {
        WakeCandidate w;
        w.guid = 1000 + i;
        w.dist = 100.0f;
        w.zone = (i % 3 == 0) ? 15u : (i % 3 == 1 ? 17u : 400u);   // home, Barrens, Thousand Needles
        c.push_back(w);
    }
    auto const picked = SelectWakeShared(c, r, 20, {});
    REQUIRE(picked.size() == 20);
    uint32_t home = 0, n17 = 0, n400 = 0;
    for (uint64_t g : picked)
    {
        uint32_t const z = c[g - 1000].zone;
        if (z == 15u) ++home; else if (z == 17u) ++n17; else ++n400;
    }
    CHECK(home >= 11);       // about 60%
    CHECK(home <= 13);
    CHECK(n17 >= 3);         // the neighbours both get some
    CHECK(n400 >= 3);
}

TEST_CASE("Waking counts the bots already awake and tops up the zone that is behind", "[ClassicWakeSelect]")
{
    using namespace Playerbot::V2::Fleet;
    WakeRule r;
    r.anchor_zone = 15;
    r.min_dist = 0.0f;
    std::vector<WakeCandidate> c;
    for (uint32_t i = 0; i < 20; ++i)
    {
        WakeCandidate w; w.guid = 1 + i; w.dist = 100.0f; w.zone = (i % 2) ? 15u : 17u;
        c.push_back(w);
    }
    // 50 awake in the home zone already, 0 elsewhere: the next ones come from the neighbour
    auto const picked = SelectWakeShared(c, r, 4, { { 15u, 50u } });
    for (uint64_t g : picked)
        CHECK(c[g - 1].zone == 17u);
}

TEST_CASE("A capital wakes its own zone only, and a wanted group that sleeps nowhere falls back to the other", "[ClassicWakeSelect]")
{
    using namespace Playerbot::V2::Fleet;
    WakeRule r;
    r.anchor_zone = 1537;
    r.capital = true;
    r.min_dist = 0.0f;
    std::vector<WakeCandidate> c;
    for (uint32_t i = 0; i < 6; ++i)
    {
        WakeCandidate w; w.guid = 1 + i; w.dist = 100.0f; w.zone = (i < 3) ? 1537u : 1u;
        c.push_back(w);
    }
    auto const picked = SelectWakeShared(c, r, 10, {});
    CHECK(picked.size() == 3);

    WakeRule o;
    o.anchor_zone = 15;
    o.min_dist = 0.0f;
    std::vector<WakeCandidate> n;
    for (uint32_t i = 0; i < 5; ++i)
    {
        WakeCandidate w; w.guid = 1 + i; w.dist = 100.0f; w.zone = 17u;
        n.push_back(w);
    }
    CHECK(SelectWakeShared(n, o, 5, {}).size() == 5);   // nobody sleeps at home: the neighbours fill it
}
