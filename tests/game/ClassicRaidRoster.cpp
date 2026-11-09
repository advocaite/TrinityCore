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

#include "../../src/modules/PlayerbotV2/Bot/Raid/RaidRoster.h"
#include <map>

using namespace Playerbot::Raid;

namespace
{
constexpr uint8_t WARRIOR = 1, PALADIN = 2, PRIEST = 5, SHAMAN = 7, MAGE = 8, DRUID = 11;

RosterBot Bot(uint64_t guid, uint8_t cls, bool set = false, RosterRole role = RosterRole::Dps)
{
    RosterBot b;
    b.guid = guid; b.cls = cls; b.role_set = set; b.role = role;
    return b;
}

std::map<uint64_t, RosterRole> Changes(std::vector<RoleChange> const& v)
{
    std::map<uint64_t, RosterRole> m;
    for (RoleChange const& c : v)
        m[c.guid] = c.role;
    return m;
}
}

TEST_CASE("raid quota: a healer per three and a bit raiders, four tanks from 25", "[classic][roster]")
{
    CHECK(RosterTargetsFor(40).healers == 12);
    CHECK(RosterTargetsFor(40).tanks == 4);
    CHECK(RosterTargetsFor(20).healers == 6);
    CHECK(RosterTargetsFor(20).tanks == 2);
    CHECK(RosterTargetsFor(5).healers == 0);
    CHECK(RosterTargetsFor(5).tanks == 0);
}

TEST_CASE("unset paladins and druids fill the healer quota, spread over classes", "[classic][roster]")
{
    std::vector<RosterBot> roster;
    for (uint64_t i = 1; i <= 6; ++i) roster.push_back(Bot(i, PALADIN));
    for (uint64_t i = 11; i <= 14; ++i) roster.push_back(Bot(i, DRUID));
    auto const got = Changes(PlanRaidRoles(roster, { 0, 4 }));
    REQUIRE(got.size() == 4);
    // Paladin, druid, paladin, druid: lowest guids of each class first.
    CHECK(got.at(1) == RosterRole::Healer);
    CHECK(got.at(11) == RosterRole::Healer);
    CHECK(got.at(2) == RosterRole::Healer);
    CHECK(got.at(12) == RosterRole::Healer);
}

TEST_CASE("a declared role is never changed and a bot that already heals counts", "[classic][roster]")
{
    std::vector<RosterBot> roster = {
        Bot(1, PALADIN, /*set*/ true, RosterRole::Dps),       // the owner made this one DPS
        Bot(2, PRIEST, true, RosterRole::Healer),             // a healer already
        Bot(3, PALADIN),
        Bot(4, PALADIN),
    };
    auto const got = Changes(PlanRaidRoles(roster, { 0, 2 }));
    REQUIRE(got.size() == 1);
    CHECK(got.at(3) == RosterRole::Healer);
    CHECK(got.count(1) == 0);
}

TEST_CASE("classes that cannot heal or tank are left alone, shortage stays short", "[classic][roster]")
{
    std::vector<RosterBot> roster = { Bot(1, MAGE), Bot(2, MAGE), Bot(3, SHAMAN) };
    auto const got = Changes(PlanRaidRoles(roster, { 0, 5 }));
    REQUIRE(got.size() == 1);
    CHECK(got.at(3) == RosterRole::Healer);
}

TEST_CASE("tanks are filled after healers, from warriors first, without reusing a healer pick", "[classic][roster]")
{
    std::vector<RosterBot> roster = { Bot(1, WARRIOR), Bot(2, WARRIOR), Bot(3, DRUID), Bot(4, PALADIN) };
    auto const got = Changes(PlanRaidRoles(roster, { 2, 1 }));
    REQUIRE(got.size() == 3);
    CHECK(got.at(4) == RosterRole::Healer);   // the paladin heals (priest first, none, then paladin)
    CHECK(got.at(1) == RosterRole::Tank);
    CHECK(got.at(2) == RosterRole::Tank);
}

TEST_CASE("a full raid needs no changes", "[classic][roster]")
{
    std::vector<RosterBot> roster = { Bot(1, PALADIN, true, RosterRole::Healer), Bot(2, WARRIOR, true, RosterRole::Tank), Bot(3, DRUID) };
    CHECK(PlanRaidRoles(roster, { 1, 1 }).empty());
}
