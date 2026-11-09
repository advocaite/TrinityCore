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

#include "../../src/modules/PlayerbotV2/Travel/WalkLinkTable.h"

#include <cmath>
#include <set>
#include <tuple>
#include <vector>

using namespace Playerbot::V2::Travel;

namespace
{
struct Pt { float x, y, z; };

float Straight(WalkLink const& l)
{
    return std::hypot(l.bx - l.ax, l.by - l.ay);
}

bool Has(float ax, float ay, float bx, float by)
{
    for (WalkLink const& l : kWalkLinks)
        if (std::fabs(l.ax - ax) < 1.0f && std::fabs(l.ay - ay) < 1.0f && std::fabs(l.bx - bx) < 1.0f && std::fabs(l.by - by) < 1.0f)
            return true;
    return false;
}
}

TEST_CASE("Every walk link is a plausible checked path", "[ClassicWalkLinks]")
{
    CHECK(std::size(kWalkLinks) >= 87);
    std::set<std::tuple<std::uint32_t, int, int, int, int>> seen;
    for (WalkLink const& l : kWalkLinks)
    {
        float const d = Straight(l);
        INFO("link " << l.map << " (" << l.ax << "," << l.ay << ") -> (" << l.bx << "," << l.by << ")");
        CHECK(d > 5.0f);                       // two different places
        CHECK(d <= 1600.0f);                   // the candidate range of the offline check
        CHECK(l.length >= d - 1.0f);           // a path is never shorter than the straight line
        CHECK(l.length <= 4000.0f);            // the checker's LONG verdict is not in the table
        CHECK(seen.insert({ l.map, int(l.ax), int(l.ay), int(l.bx), int(l.by) }).second);
    }
}

TEST_CASE("The Elwynn, Redridge and Burning Steppes links are in, the ones the check refused are not", "[ClassicWalkLinks]")
{
    CHECK(Has(-9246, -2045, -9468, -1182));    // Lakeshire to the Redridge road, path 1164 y for 891 y
    CHECK(Has(-7504, -2188, -8365, -2738));    // Flame Crest to Morgan's Vigil
    CHECK(Has(-9942, -147, -9468, -1182));         // Elwynn to the Redridge road, the link between Stormwind and Burning Steppes
    CHECK(Has(-7492, -2170, -6687, -2163));        // Flame Crest to Kargath: complete, 3482 y the long way round, so it is the cost
    CHECK_FALSE(Has(-10516, -1262, -9468, -1182));  // Darkshire to the Redridge road: PARTIAL
    CHECK_FALSE(Has(-6555, -1100, -5596, -504));    // Thorium Point to Ironforge: 4835 y, over the 4000 y limit
    CHECK_FALSE(Has(-9429, -2231, -10457, -3279));  // Lakeshire to Blasted Lands: 4175 y
    CHECK_FALSE(Has(-4613, -929, -4126, -1092));    // the Ironforge pair: CLIFF
}

TEST_CASE("A link end names the node at its position", "[ClassicWalkLinks]")
{
    std::vector<Pt> pts = { { 0.f, 0.f, 0.f }, { 100.f, 0.f, 0.f }, { 100.5f, 0.f, 0.2f }, { 100.f, 50.f, 90.f } };
    CHECK(MatchWalkLinkEnd(pts, 0.f, 0.f, 0.f) == 0);
    CHECK(MatchWalkLinkEnd(pts, 100.4f, 0.f, 0.1f) == 2);    // the nearer of two close nodes
    CHECK(MatchWalkLinkEnd(pts, 100.f, 50.f, 0.f) == -1);    // same x,y but another level: no node there
    CHECK(MatchWalkLinkEnd(pts, 50.f, 0.f, 0.f) == -1);
    CHECK(MatchWalkLinkEnd(std::vector<Pt>{}, 0.f, 0.f, 0.f) == -1);
}
