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

#include "../../src/modules/PlayerbotV2/Api/TalentCode.h"

using namespace Playerbot::Classic::TalentCode;

namespace
{
// Tabs as the server lays them out from the 70862 trait data (ClassicSpec GetTalentTabs): ranks and row of each talent, row by row.
std::vector<Slot> Tab(std::vector<std::vector<uint8_t>> const& rows)
{
    std::vector<Slot> tab;
    for (size_t r = 0; r < rows.size(); ++r)
        for (uint8_t ranks : rows[r])
            tab.push_back({ ranks, uint8_t(r) });
    return tab;
}

Layout Mage()
{
    return {
        Tab({ { 2, 5, 3 }, { 2, 2, 5, 2 }, { 2, 3, 1 }, { 2, 2, 3, 1 }, { 1, 5 }, { 3 }, { 1 } }),      // Arcane, 18
        Tab({ { 5, 5 }, { 5, 2, 3 }, { 3, 3, 1, 2 }, { 3, 2, 1, 3 }, { 3, 1 }, { 3 }, { 1 } }),         // Fire, 17
        Tab({ { 2, 5, 3 }, { 5, 3, 2, 3 }, { 3, 3, 1 }, { 2, 1, 3 }, { 3, 1, 2 }, { 5 }, { 1 } }),      // Frost, 18
    };
}

Layout Warrior()
{
    return {
        Tab({ { 3, 5, 3 }, { 2, 3, 2 }, { 1, 3 }, { 1, 3, 2 }, { 2, 1, 5 }, { 2, 3 }, { 1 } }),              // Arms, 17
        Tab({ { 5, 5 }, { 5, 5 }, { 3, 1, 3, 5 }, { 5, 1, 2 }, { 2, 1, 2, 2 }, { 2, 3 }, { 1 } }),          // Fury, 18
        Tab({ { 2, 5, 5 }, { 5, 3, 3 }, { 1, 2, 3, 3 }, { 3, 1, 2 }, { 2, 1, 3 }, { 5 }, { 1 } }),      // Protection, 18
    };
}
}

// Dumpins (#bot-feedback 11-12): the whole link the calculator gives, copied from the browser.
TEST_CASE("A Wowhead Forever talent calculator link gives its class and points", "[ClassicTalentCode]")
{
    Parsed const p = Parse("https://www.wowhead.com/forever/talent-calc/warrior/v2--05003201_t0/2BEffh");
    REQUIRE(p.ok());
    CHECK(p.cls == 1);
    REQUIRE(p.tabs.size() == 3);
    CHECK(p.tabs[0].empty());
    CHECK(p.tabs[1].empty());
    CHECK(p.tabs[2] == std::vector<uint8_t>{ 0, 5, 0, 0, 3, 2, 0, 1 });

    Check const c = Validate(p, Warrior());
    REQUIRE(c.ok());
    CHECK(c.total == 11);
    CHECK(c.perTab == std::vector<int>{ 0, 0, 11 });
}

TEST_CASE("The code alone, with or without the version and the levelling order, is taken", "[ClassicTalentCode]")
{
    for (char const* text : { "050205223101311531-5500003-003", "v2050205223101311531-5500003-003", "050205223101311531-5500003-003_t0/2C1BA",
                              "  050205223101311531-5500003-003 please" })
    {
        INFO(text);
        Parsed const p = Parse(text);
        REQUIRE(p.ok());
        CHECK(p.cls == 0);
        Check const c = Validate(p, Mage());
        REQUIRE(c.ok());
        CHECK(c.perTab == std::vector<int>{ 35, 13, 3 });
        CHECK(c.total == 51);
    }
}

TEST_CASE("A link's class word is read, an unknown one or a link without a build is refused", "[ClassicTalentCode]")
{
    CHECK(Parse("https://www.wowhead.com/forever/talent-calc/mage/v2-55").cls == 8);
    CHECK(Parse("www.wowhead.com/forever/talent-calc/Druid/v2-55").cls == 11);
    CHECK(Parse("https://www.wowhead.com/forever/talent-calc/deathknight/v2-55").error == Error::UnknownClass);
    CHECK(Parse("https://www.wowhead.com/forever/talent-calc/mage").error == Error::NoCode);
    CHECK(Parse("https://www.wowhead.com/forever/talent-calc/mage/").error == Error::NoCode);
}

TEST_CASE("Something that is no code is refused", "[ClassicTalentCode]")
{
    CHECK(Parse("").error == Error::Empty);
    CHECK(Parse("000-00").error == Error::Empty);
    CHECK(Parse("05a").error == Error::BadChar);
    CHECK(Parse("5-5-5-5").error == Error::TooManyTabs);
}

TEST_CASE("A code that does not fit the class's tabs is refused, with where", "[ClassicTalentCode]")
{
    // A link of an older calculator layout: 19 digits for the 18 Arcane talents.
    Check const tooLong = Validate(Parse("v20502205003100311531-230510301001-003"), Mage());
    CHECK(tooLong.error == Error::TabTooLong);
    CHECK(tooLong.tab == 0);

    // 5 points in Missile Barrage (2 ranks).
    Check const over = Validate(Parse("0502005"), Mage());
    CHECK(over.error == Error::OverMax);
    CHECK(over.tab == 0);
    CHECK(over.slot == 6);

    // Arcane Meditation (fourth row) with nothing above it.
    Check const locked = Validate(Parse("0000000000001"), Mage());
    CHECK(locked.error == Error::RowLocked);
    CHECK(locked.slot == 12);

    // Every talent of every tab at its ranks: far over 51.
    std::string full;
    for (auto const& tab : Mage())
    {
        if (!full.empty())
            full += '-';
        for (Slot const& s : tab)
            full += char('0' + s.maxRanks);
    }
    CHECK(Validate(Parse(full), Mage()).error == Error::TooManyPoints);
}

TEST_CASE("A build is written back as the calculator writes it", "[ClassicTalentCode]")
{
    CHECK(Encode({ { 0, 5, 0 }, {}, { 0, 0, 3 } }) == "05--003");
    CHECK(Encode({ { 0, 0 }, { 0 }, { 0, 5, 0, 0, 3, 2, 0, 1, 0, 0 } }) == "--05003201");
    CHECK(Encode({ { 2, 0 }, { 0 }, { 0 } }) == "2");
    CHECK(Encode({ {}, {}, {} }).empty());
    Parsed const p = Parse("050205223101311531-5500003-003");
    CHECK(Encode(p.tabs) == "050205223101311531-5500003-003");
    CHECK(Link(8, "05--003") == "https://www.wowhead.com/forever/talent-calc/mage/v205--003");
    CHECK(Parse(Link(1, "--05003201")).tabs == Parse("--05003201").tabs);
    CHECK(Link(6, "1").empty());   // no death knights in Forever
}

TEST_CASE("A levelling bot buys the tab with most points first, row by row", "[ClassicTalentCode]")
{
    // 13 Fire, 35 Arcane, 3 Frost: Arcane first, then Fire, then Frost.
    std::vector<Step> const steps = LevellingOrder(Parse("050205223101311531-5500003-003"));
    REQUIRE(!steps.empty());
    CHECK(steps.front().tab == 0);
    CHECK(steps.front().slot == 1);
    CHECK(steps.front().ranks == 5);
    size_t firstFire = 0, firstFrost = 0;
    for (size_t i = 0; i < steps.size(); ++i)
    {
        if (steps[i].tab == 1 && !firstFire)
            firstFire = i;
        if (steps[i].tab == 2 && !firstFrost)
            firstFrost = i;
    }
    CHECK(firstFire > 0);
    CHECK(firstFrost > firstFire);
    for (size_t i = 1; i < steps.size(); ++i)
        if (steps[i].tab == steps[i - 1].tab)
            CHECK(steps[i].slot > steps[i - 1].slot);

    // A tie goes to the earlier tab.
    std::vector<Step> const tie = LevellingOrder(Parse("5-5"));
    REQUIRE(tie.size() == 2);
    CHECK(tie[0].tab == 0);
}
