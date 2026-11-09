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

#include "../../src/modules/PlayerbotV2/Fleet/RaidSubgroupPlan.h"

#include <map>

using namespace Playerbot::V2::Fleet;

namespace {

// Applies the plan the way Group does (move: set the group; swap: trade) and returns guid -> group.
std::map<std::uint64_t, std::uint8_t> Apply(std::vector<SubgroupMember> const& in, std::vector<SubgroupMove> const& plan)
{
    std::map<std::uint64_t, std::uint8_t> g;
    for (SubgroupMember const& m : in)
        g[m.guid] = m.group;
    for (SubgroupMove const& mv : plan)
    {
        if (mv.swap)
            std::swap(g[mv.guid], g[mv.other]);
        else
            g[mv.guid] = mv.group;
    }
    return g;
}

SubgroupMember Bot(std::uint64_t guid, std::uint8_t group, std::uint8_t want) { return { guid, group, want, true }; }

} // namespace

TEST_CASE("RaidSubgroupPlan: a jumbled raid goes back to its stored sub-groups", "[raidsubgroup]")
{
    // The owner (human) in group 0 with tanks 1,2 and healers 3,4 stored in group 0; DPS 5..9 in group 1. They came back in the order
    // 5,6,7,8 first (filling group 0 behind the owner), then 1,2,3,4,9.
    std::vector<SubgroupMember> m = {
        { 100, 0, kNoSubgroup, false },
        Bot(5, 0, 1), Bot(6, 0, 1), Bot(7, 0, 1), Bot(8, 0, 1),
        Bot(1, 1, 0), Bot(2, 1, 0), Bot(3, 1, 0), Bot(4, 1, 0), Bot(9, 1, 1),
    };
    auto const plan = PlanSubgroupMoves(m);
    auto const g = Apply(m, plan);
    CHECK(g.at(100) == 0);
    for (std::uint64_t t : { 1, 2, 3, 4 })
        CHECK(g.at(t) == 0);
    for (std::uint64_t d : { 5, 6, 7, 8, 9 })
        CHECK(g.at(d) == 1);
}

TEST_CASE("RaidSubgroupPlan: a member already home, one with nothing stored and a human are left alone", "[raidsubgroup]")
{
    std::vector<SubgroupMember> m = { { 100, 0, 3, false }, Bot(1, 0, 0), Bot(2, 1, kNoSubgroup), Bot(3, 2, 2) };
    CHECK(PlanSubgroupMoves(m).empty());
}

TEST_CASE("RaidSubgroupPlan: a move goes to a sub-group with room, a full one is traded into", "[raidsubgroup]")
{
    std::vector<SubgroupMember> room = { Bot(1, 0, 1), Bot(2, 1, kNoSubgroup) };
    auto const p1 = PlanSubgroupMoves(room);
    REQUIRE(p1.size() == 1);
    CHECK_FALSE(p1[0].swap);
    CHECK(p1[0].guid == 1);
    CHECK(p1[0].group == 1);

    std::vector<SubgroupMember> full;
    for (std::uint64_t i = 0; i < 5; ++i)
        full.push_back(Bot(10 + i, 1, kNoSubgroup));
    full.push_back(Bot(1, 0, 1));
    auto const p2 = PlanSubgroupMoves(full);
    REQUIRE(p2.size() == 1);
    CHECK(p2[0].swap);
    CHECK(p2[0].guid == 1);
    auto const g = Apply(full, p2);
    CHECK(g.at(1) == 1);
    CHECK(g.at(p2[0].other) == 0);
}

TEST_CASE("RaidSubgroupPlan: a bot that is home in a full sub-group is never traded out; a bot wanting beyond the raid is ignored", "[raidsubgroup]")
{
    std::vector<SubgroupMember> m;
    for (std::uint64_t i = 0; i < 5; ++i)
        m.push_back(Bot(10 + i, 1, 1));
    m.push_back(Bot(1, 0, 1));
    CHECK(PlanSubgroupMoves(m).empty());                     // nobody there to trade with: stays until a place frees
    m = { Bot(1, 0, 9) };
    CHECK(PlanSubgroupMoves(m).empty());
}

TEST_CASE("RaidSubgroupPlan: a full 40-man layout comes back from any join order", "[raidsubgroup]")
{
    // 8 sub-groups of 5; bot i (0..39) wants sub-group i / 5 except slot 0 which is the human owner.
    std::vector<SubgroupMember> m;
    m.push_back({ 1000, 0, kNoSubgroup, false });
    for (std::uint64_t i = 1; i < 40; ++i)
        m.push_back(Bot(i, 0, std::uint8_t(i / 5)));
    // Joined in reverse: the last bots took the early groups.
    for (std::uint64_t i = 1; i < 40; ++i)
        m[i].group = std::uint8_t((40 - i) / 5);
    // keep 5 per group: the owner is in group 0 with bot 39..36 (they joined first)
    auto const plan = PlanSubgroupMoves(m);
    auto const g = Apply(m, plan);
    for (std::uint64_t i = 1; i < 40; ++i)
        CHECK(g.at(i) == i / 5);
}

TEST_CASE("The member list is put back in the saved order", "[raidsubgroup]")
{
    using Playerbot::V2::Fleet::OrderMember;
    using Playerbot::V2::Fleet::PlanMemberOrder;
    // Human 1 first, then bots that logged in in the wrong order: saved 3, 1, 2 (list indices).
    std::vector<OrderMember> m = {
        { 1, false, 0xFFFF },   // the owner
        { 10, true, 3 },
        { 11, true, 1 },
        { 12, true, 2 },
    };
    std::vector<std::uint64_t> const want = PlanMemberOrder(m);
    REQUIRE(want.size() == 4);
    CHECK(want[0] == 1);     // the human keeps its place
    CHECK(want[1] == 11);
    CHECK(want[2] == 12);
    CHECK(want[3] == 10);
}

TEST_CASE("A list already in the saved order, or with no stored order, is left alone", "[raidsubgroup]")
{
    using Playerbot::V2::Fleet::OrderMember;
    using Playerbot::V2::Fleet::PlanMemberOrder;
    CHECK(PlanMemberOrder({ { 1, false, 0xFFFF }, { 10, true, 1 }, { 11, true, 2 } }).empty());
    CHECK(PlanMemberOrder({ { 10, true, 0xFFFF }, { 11, true, 0xFFFF } }).empty());
    // A bot without a stored order stays where it is while the others trade places around it.
    auto const w = PlanMemberOrder({ { 10, true, 5 }, { 11, true, 0xFFFF }, { 12, true, 2 } });
    REQUIRE(w.size() == 3);
    CHECK(w[0] == 12);
    CHECK(w[1] == 11);
    CHECK(w[2] == 10);
}
