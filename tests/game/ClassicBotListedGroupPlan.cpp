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

#include "../../src/modules/PlayerbotV2/BotListedGroupPlan.h"

#include <algorithm>
#include <vector>

using namespace Playerbot::V2;

TEST_CASE("BotListedGroupPlan: the activity list reads", "[listedgroups]")
{
    auto const list = ParseListedActivities("214272:229:58:60:10, 100:36:15:20");
    REQUIRE(list.size() == 2);
    CHECK(list[0] == ListedActivity{ 214272, 229, 58, 60, 10 });
    CHECK(list[1] == ListedActivity{ 100, 36, 15, 20, 5 });     // no size: a 5-man
    CHECK(ParseListedActivities("1:2:3:4:99")[0].size == 10);   // held to 5-10
}

TEST_CASE("BotListedGroupPlan: a bad entry is skipped, levels are clamped and ordered", "[listedgroups]")
{
    auto const list = ParseListedActivities("x:1:2:3,5:6:7,1:2:3:4:5:6,9:8:90:0,,7:7:30:20");
    REQUIRE(list.size() == 2);
    CHECK(list[0] == ListedActivity{ 9, 8, 1, 80 });
    CHECK(list[1] == ListedActivity{ 7, 7, 20, 30 });
    CHECK(ParseListedActivities("").empty());
    CHECK(ParseListedActivities("0:229:1:60").empty());     // no activity id
}

TEST_CASE("BotListedGroupPlan: the party size is the map limit held to 5-10", "[listedgroups]")
{
    CHECK(ListedGroupSize(5) == 5);
    CHECK(ListedGroupSize(10) == 10);
    CHECK(ListedGroupSize(40) == 10);
    CHECK(ListedGroupSize(0) == 5);
}

TEST_CASE("BotListedGroupPlan: the lifetime lies between the minimum and the maximum", "[listedgroups]")
{
    CHECK(ListedGroupLifetimeMs(300, 1200, 0.0f) == 300000);
    CHECK(ListedGroupLifetimeMs(300, 1200, 1.0f) == 1200000);
    CHECK(ListedGroupLifetimeMs(300, 1200, 0.5f) == 750000);
    CHECK(ListedGroupLifetimeMs(600, 300, 0.7f) == 600000);     // a maximum below the minimum is the minimum
}

TEST_CASE("BotListedGroupPlan: what a listed group does", "[listedgroups]")
{
    CHECK(DecideListedGroup(true, 5, false, 1000, 5000) == ListedGroupAction::Keep);
    CHECK(DecideListedGroup(true, 5, false, 5000, 5000) == ListedGroupAction::Expire);
    CHECK(DecideListedGroup(true, 6, true, 1000, 5000) == ListedGroupAction::Handover);     // a player joined
    CHECK(DecideListedGroup(true, 6, true, 9000, 5000) == ListedGroupAction::Handover);     // before the lifetime check
    CHECK(DecideListedGroup(false, 0, false, 1000, 5000) == ListedGroupAction::Drop);
    CHECK(DecideListedGroup(true, 1, false, 1000, 5000) == ListedGroupAction::Drop);
}

TEST_CASE("BotListedGroupPlan: the list is busiest in the evening and quietest at night", "[listedgroups]")
{
    CHECK(ListedGroupDaylight(20) > ListedGroupDaylight(12));
    CHECK(ListedGroupDaylight(12) > ListedGroupDaylight(4));
    CHECK(ListedGroupDaylight(44) == ListedGroupDaylight(20));      // an hour past 23 wraps
    CHECK(ListedGroupTarget(6, 1.0f) == 6);
    CHECK(ListedGroupTarget(6, 0.25f) == 2);
    CHECK(ListedGroupTarget(1, 0.25f) == 1);        // one while a player is online
    CHECK(ListedGroupTarget(0, 1.0f) == 0);         // switched off
}

TEST_CASE("BotListedGroupPlan: a listed group lacks a scarce role and is rarely full", "[listedgroups]")
{
    // healer wanted, one missing: 5-man is 1 tank, 0 healer, 3 damage
    ListedGroupShape healer = PlanListedGroup(5, 0.1f, 0.9f, 0.0f);
    CHECK(healer.have == LfgRoleGap{ 1, 0, 3 });
    CHECK(healer.missing == LfgRoleGap{ 0, 1, 0 });
    CHECK(healer.members == 4);

    // tank wanted
    ListedGroupShape tank = PlanListedGroup(5, 0.6f, 0.9f, 0.0f);
    CHECK(tank.have == LfgRoleGap{ 0, 1, 3 });

    // two healers missing from a 10-man, which has 3
    ListedGroupShape two = PlanListedGroup(10, 0.1f, 0.1f, 0.0f);
    CHECK(two.have == LfgRoleGap{ 1, 1, 6 });
    CHECK(two.missing.healer == 2);

    // open damage places: a 5-man with the most open stays at two members or more
    ListedGroupShape open = PlanListedGroup(5, 0.9f, 0.9f, 0.99f);
    CHECK(open.members >= 3);
    CHECK(open.members < 5);
}

TEST_CASE("BotListedGroupPlan: every shape seats at least two and never more than the party", "[listedgroups]")
{
    for (uint32 size : { 5u, 10u })
        for (float role : { 0.0f, 0.5f, 0.9f })
            for (float count : { 0.0f, 0.9f })
                for (float open : { 0.0f, 0.5f, 0.99f })
                {
                    ListedGroupShape const shape = PlanListedGroup(size, role, count, open);
                    CHECK(shape.members >= size / 2 + 1);
                    CHECK(shape.members < size);
                    CHECK(shape.members + shape.missing.Total() == size);
                }
}

TEST_CASE("BotListedGroupPlan: a dungeon without a listed group comes first", "[listedgroups]")
{
    std::vector<ListedActivity> const all{ { 1, 10, 1, 10 }, { 2, 20, 1, 10 }, { 3, 30, 1, 10 }, { 4, 40, 1, 10 } };
    auto first = [](uint32 n) { return n - 1; };        // a fixed "random" source
    // nothing listed: all four, in some order
    auto order = ListedActivityOrder(all, {}, first);
    REQUIRE(order.size() == 4);
    // dungeons 2 and 4 already have a group: 1 and 3 come before them, whatever the shuffle gives
    for (uint32 pick : { 0u, 1u })
    {
        auto const o = ListedActivityOrder(all, { 2, 4 }, [&](uint32 n) { return std::min(pick, n - 1); });
        REQUIRE(o.size() == 4);
        CHECK((o[0] == 0 || o[0] == 2));
        CHECK((o[1] == 0 || o[1] == 2));
        CHECK((o[2] == 1 || o[2] == 3));
        CHECK((o[3] == 1 || o[3] == 3));
    }
    // all listed: every dungeon is still offered (a second group)
    CHECK(ListedActivityOrder(all, { 1, 2, 3, 4 }, first).size() == 4);
}
