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

#include "../../src/modules/PlayerbotV2/Fleet/AreaCap.h"

using namespace Playerbot::V2::Fleet;

namespace
{
std::vector<CrowdPoint> Camp(std::uint64_t first_id, int n, float cx, float cy, std::uint32_t cap)
{
    std::vector<CrowdPoint> v;
    for (int i = 0; i < n; ++i)
        v.push_back({ first_id + std::uint64_t(i), cx + float(i % 5), cy + float(i / 5), cap });
    return v;
}
}

TEST_CASE("A crowd over the cap names its extras, the cap stays", "[ClassicAreaCap]")
{
    auto const camp = Camp(100, 30, 0.f, 0.f, 12);
    auto const out = ExcessBots(camp, 50.0f, 100);
    CHECK(out.size() == 18);
    CHECK(out.front() == 112);   // ids 100..111 stay
}

TEST_CASE("A crowd under the cap, or two camps apart, is left alone", "[ClassicAreaCap]")
{
    CHECK(ExcessBots(Camp(1, 12, 0.f, 0.f, 12), 50.0f, 100).empty());
    auto two = Camp(1, 10, 0.f, 0.f, 12);
    auto const far_camp = Camp(100, 10, 500.f, 0.f, 12);
    two.insert(two.end(), far_camp.begin(), far_camp.end());
    CHECK(ExcessBots(two, 50.0f, 100).empty());
}

TEST_CASE("Only max_out are moved per pass and a cap of 0 is no cap", "[ClassicAreaCap]")
{
    CHECK(ExcessBots(Camp(1, 30, 0.f, 0.f, 12), 50.0f, 3).size() == 3);
    CHECK(ExcessBots(Camp(1, 30, 0.f, 0.f, 0), 50.0f, 100).empty());
}

TEST_CASE("Placement refuses a full spot", "[ClassicAreaCap]")
{
    auto const camp = Camp(1, 12, 0.f, 0.f, 12);
    CHECK_FALSE(RoomFor(camp, 3.f, 1.f, 50.0f, 12));
    CHECK(RoomFor(camp, 300.f, 0.f, 50.0f, 12));
    CHECK(RoomFor(camp, 3.f, 1.f, 50.0f, 0));
}
