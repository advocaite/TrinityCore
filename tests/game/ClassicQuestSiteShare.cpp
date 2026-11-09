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

#include "../../src/modules/PlayerbotV2/Fleet/QuestSiteShare.h"

using namespace Playerbot::V2::Fleet;

namespace
{
std::vector<SiteWorker> Crowd(std::uint64_t n, std::uint64_t skip)
{
    std::vector<SiteWorker> v;
    for (std::uint64_t id = 1; id <= n; ++id)
        if (id != skip)
            v.push_back({ id, false });
    return v;
}
}

TEST_CASE("The first bots at a spot keep it, the rest give it up", "[ClassicQuestSiteShare]")
{
    unsigned stay = 0, leave = 0;
    for (std::uint64_t id = 1; id <= 12; ++id)
        (YieldQuestSite(id, Crowd(12, id), 5u) ? leave : stay)++;
    REQUIRE(stay == 5);
    REQUIRE(leave == 7);
    REQUIRE_FALSE(YieldQuestSite(1, Crowd(12, 1), 5u));
    REQUIRE(YieldQuestSite(12, Crowd(12, 12), 5u));
}

TEST_CASE("A real player and a companion take a slot before any world bot", "[ClassicQuestSiteShare]")
{
    std::vector<SiteWorker> near = Crowd(3, 1000);   // three world bots with low ids
    near.push_back({ 900, true });
    near.push_back({ 901, true });
    // two of five slots are held for the player side: a bot with id 1000 has 3 world bots + 2 ahead
    REQUIRE(YieldQuestSite(1000, near, 5u));
    REQUIRE_FALSE(YieldQuestSite(1000, near, 6u));
}

TEST_CASE("A small group or cap 0 never yields", "[ClassicQuestSiteShare]")
{
    REQUIRE_FALSE(YieldQuestSite(50, Crowd(4, 50), 5u));
    REQUIRE_FALSE(YieldQuestSite(50, Crowd(40, 50), 0u));
}
