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

#include "../../src/modules/PlayerbotV2/Bot/Raid/RaidHealGroups.h"

using namespace Playerbot::Raid;

namespace
{
    std::vector<uint8_t> const kEightGroups = { 0, 1, 2, 3, 4, 5, 6, 7 };

    std::vector<GroupHealer> Healers(std::initializer_list<uint64_t> ids, int mana = 100)
    {
        std::vector<GroupHealer> v;
        for (uint64_t id : ids)
            v.push_back({ id, mana });
        return v;
    }

    uint32_t Covered(std::map<uint64_t, uint16_t> const& m)
    {
        uint32_t all = 0;
        for (auto const& [h, mask] : m)
        {
            CHECK((all & mask) == 0);   // no group has two healers
            all |= mask;
        }
        return all;
    }
}

TEST_CASE("Each subgroup gets one healer and spare healers float", "[ClassicRaidHealGroups]")
{
    auto const plan = AssignHealGroups(Healers({ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }), kEightGroups, {});
    CHECK(Covered(plan) == 0xFFu);
    CHECK(plan.size() == 8);              // two healers have no group
    CHECK(plan.count(9) == 0);
    CHECK(plan.count(10) == 0);
}

TEST_CASE("Fewer healers than groups: every group is still covered", "[ClassicRaidHealGroups]")
{
    auto const plan = AssignHealGroups(Healers({ 1, 2, 3 }), kEightGroups, {});
    CHECK(Covered(plan) == 0xFFu);
    CHECK(plan.size() == 3);
}

TEST_CASE("A healer keeps its groups from plan to plan", "[ClassicRaidHealGroups]")
{
    auto const first = AssignHealGroups(Healers({ 1, 2, 3, 4 }), { 0, 1, 2, 3 }, {});
    auto const again = AssignHealGroups(Healers({ 1, 2, 3, 4 }), { 0, 1, 2, 3 }, first);
    CHECK(again == first);
}

TEST_CASE("When a healer dies a floater takes its group", "[ClassicRaidHealGroups]")
{
    auto const first = AssignHealGroups(Healers({ 1, 2, 3, 4, 5 }), { 0, 1, 2, 3 }, {});   // healer 5 floats
    auto const plan = AssignHealGroups(Healers({ 1, 2, 4, 5 }), { 0, 1, 2, 3 }, first);     // healer 3 died
    CHECK(Covered(plan) == 0xFu);
    CHECK(plan.at(5) == 0x4u);             // the floater took group 2
    CHECK(plan.at(1) == first.at(1));      // nobody else moved
    CHECK(plan.at(2) == first.at(2));
    CHECK(plan.at(4) == first.at(4));
}

TEST_CASE("With no floater the next group's healer covers both, and gives one back when the healer returns", "[ClassicRaidHealGroups]")
{
    auto const first = AssignHealGroups(Healers({ 1, 2, 3, 4 }), { 0, 1, 2, 3 }, {});
    auto const covered = AssignHealGroups(Healers({ 1, 2, 4 }), { 0, 1, 2, 3 }, first);    // healer 3 died
    CHECK(Covered(covered) == 0xFu);
    CHECK(covered.count(3) == 0);
    CHECK(covered.at(4) == 0xCu);          // groups 2 and 3
    auto const back = AssignHealGroups(Healers({ 1, 2, 3, 4 }), { 0, 1, 2, 3 }, covered);  // rezzed
    CHECK(Covered(back) == 0xFu);
    CHECK(back.at(3) == 0x8u);
    CHECK(back.at(4) == 0x4u);
}

TEST_CASE("A healer that ran dry is out of the plan until it recovers", "[ClassicRaidHealGroups]")
{
    auto const first = AssignHealGroups(Healers({ 1, 2, 3, 4 }), { 0, 1, 2, 3 }, {});
    std::vector<GroupHealer> healers = Healers({ 1, 3, 4 });
    healers.push_back({ 2, kHealGroupOutManaPct - 1 });
    auto const plan = AssignHealGroups(healers, { 0, 1, 2, 3 }, first);
    CHECK(Covered(plan) == 0xFu);
    CHECK(plan.count(2) == 0);
    CHECK(plan.at(3) == 0x6u);             // the next group's healer took group 1 as well
}

TEST_CASE("No healers or no groups gives an empty plan", "[ClassicRaidHealGroups]")
{
    CHECK(AssignHealGroups({}, kEightGroups, {}).empty());
    CHECK(AssignHealGroups(Healers({ 1, 2 }), {}, {}).empty());
}

TEST_CASE("A healer at the mana mark does not drop out and rejoin on every plan", "[ClassicRaidHealGroups]")
{
    auto const first = AssignHealGroups(Healers({ 1, 2, 3, 4 }), { 0, 1, 2, 3 }, {});
    std::vector<GroupHealer> healers = Healers({ 1, 3, 4 });
    healers.push_back({ 2, kHealGroupOutManaPct + 5 });
    CHECK(AssignHealGroups(healers, { 0, 1, 2, 3 }, first).count(2) == 1);   // in the plan: stays above the out mark

    auto const dropped = AssignHealGroups(Healers({ 1, 3, 4 }), { 0, 1, 2, 3 }, first);
    std::vector<GroupHealer> back = Healers({ 1, 3, 4 });
    back.push_back({ 2, kHealGroupBackManaPct - 1 });
    CHECK(AssignHealGroups(back, { 0, 1, 2, 3 }, dropped).count(2) == 0);    // out of the plan: needs the back mark
    back.back().mana_pct = kHealGroupBackManaPct;
    CHECK(AssignHealGroups(back, { 0, 1, 2, 3 }, dropped).count(2) == 1);
}
