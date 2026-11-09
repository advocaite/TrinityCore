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

#include "../../src/modules/PlayerbotV2/Travel/NearestNode3D.h"

#include <vector>

using namespace Playerbot::V2::Travel;

namespace
{
struct N { float x, y, z; };

int Pick(std::vector<N> const& v, float bx, float by, float bz, float range, std::vector<bool> const& skip = {})
{
    return PickNearestNode3D(v.size(), [&](std::size_t i, float& x, float& y, float& z) { x = v[i].x; y = v[i].y; z = v[i].z; },
        [&](std::size_t i) { return skip.empty() || !skip[i]; }, bx, by, bz, range);
}
}

TEST_CASE("NearestNode3D: a node on another floor loses to a slightly farther one on the bot's floor", "[nearestnode3d]")
{
    // Ironforge: the bot stands on the forge floor (z 400); a node 10 y away sideways but 100 y up is on the ward above.
    std::vector<N> nodes{ { -4830.f, -1010.f, 500.f }, { -4830.f, -1040.f, 402.f } };
    CHECK(Pick(nodes, -4830.f, -1020.f, 400.f, 800.f) == 1);
}

TEST_CASE("NearestNode3D: nodes already failed are skipped, so a wedged bot tries the next one", "[nearestnode3d]")
{
    std::vector<N> nodes{ { 0.f, 10.f, 0.f }, { 0.f, 20.f, 0.f }, { 0.f, 30.f, 0.f } };
    CHECK(Pick(nodes, 0.f, 0.f, 0.f, 100.f) == 0);
    CHECK(Pick(nodes, 0.f, 0.f, 0.f, 100.f, { true, false, false }) == 1);
    CHECK(Pick(nodes, 0.f, 0.f, 0.f, 100.f, { true, true, false }) == 2);
    CHECK(Pick(nodes, 0.f, 0.f, 0.f, 100.f, { true, true, true }) == -1);
}

TEST_CASE("NearestNode3D: nothing beyond the range", "[nearestnode3d]")
{
    std::vector<N> nodes{ { 0.f, 500.f, 0.f } };
    CHECK(Pick(nodes, 0.f, 0.f, 0.f, 400.f) == -1);
    CHECK(Pick(nodes, 0.f, 0.f, 0.f, 600.f) == 0);
}
