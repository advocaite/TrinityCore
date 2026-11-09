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

#include "../../src/modules/PlayerbotV2/LfgRoleGap.h"

#include <vector>

using namespace Playerbot::V2;

namespace {
LfgRoleGap Gap(std::vector<uint8> const& roles) { return ComputeLfgRoleGap(roles); }
}

TEST_CASE("LfgRoleGap: an empty group lacks the whole party", "[lfggap]")
{
    CHECK(Gap({}) == LfgRoleGap{ 1, 1, 3 });
}

TEST_CASE("LfgRoleGap: a lone damage dealer needs a tank, a healer and two more damage", "[lfggap]")
{
    CHECK(Gap({ kLfgRoleDamage }) == LfgRoleGap{ 1, 1, 2 });
    CHECK(Gap({ 0 }) == LfgRoleGap{ 1, 1, 2 });         // no role known counts as damage
}

TEST_CASE("LfgRoleGap: real players take their own role first", "[lfggap]")
{
    CHECK(Gap({ kLfgRoleTank, kLfgRoleHealer }) == LfgRoleGap{ 0, 0, 3 });
    CHECK(Gap({ kLfgRoleTank, kLfgRoleHealer, kLfgRoleDamage, kLfgRoleDamage, kLfgRoleDamage }) == LfgRoleGap{ 0, 0, 0 });
}

TEST_CASE("LfgRoleGap: a second tank takes a free damage place", "[lfggap]")
{
    CHECK(Gap({ kLfgRoleTank, kLfgRoleTank }) == LfgRoleGap{ 0, 1, 2 });
}

TEST_CASE("LfgRoleGap: a player who ticked every role takes the first free one", "[lfggap]")
{
    uint8 const all = kLfgRoleTank | kLfgRoleHealer | kLfgRoleDamage;
    CHECK(Gap({ all }) == LfgRoleGap{ 0, 1, 3 });
    CHECK(Gap({ all, all }) == LfgRoleGap{ 0, 0, 3 });
}

TEST_CASE("LfgRoleGap: a damage dealer never takes the tank or healer place", "[lfggap]")
{
    CHECK(Gap({ kLfgRoleTank, kLfgRoleHealer, kLfgRoleDamage, kLfgRoleDamage, kLfgRoleDamage, kLfgRoleDamage }) == LfgRoleGap{ 0, 0, 0 });
    // four damage dealers: the last place goes to the tank, and one damage dealer makes room for the healer
    CHECK(Gap({ 0, 0, 0, 0 }) == LfgRoleGap{ 1, 1, 0 });
    CHECK(PlanLfgSeats(std::vector<uint8>{ 0, 0, 0, 0 }).dropDamage == 1);
    // six damage dealers in a 5-man: two make room for a tank and a healer
    std::vector<uint8> const six(6, kLfgRoleDamage);
    CHECK(PlanLfgSeats(six, 5).gap == LfgRoleGap{ 1, 1, 0 });
    CHECK(PlanLfgSeats(six, 5).dropDamage == 2);
}

TEST_CASE("LfgRoleGap: Blackrock Spire, three healers and seven damage dealers still lack a tank", "[lfggap]")
{
    // the group finder group of 2026-11-08: 3 healers, 3 rogues (the player too), 2 warlocks, a hunter, a mage
    std::vector<uint8> members(3, kLfgRoleHealer);
    members.insert(members.end(), 7, kLfgRoleDamage);
    LfgSeatPlan const plan = PlanLfgSeats(members, 10);
    CHECK(plan.gap == LfgRoleGap{ 1, 0, 0 });
    CHECK(plan.dropDamage == 1);
    // nine of them (before the player joined): the tenth place is the tank's, nobody has to leave
    members.pop_back();
    CHECK(PlanLfgSeats(members, 10).gap == LfgRoleGap{ 1, 0, 0 });
    CHECK(PlanLfgSeats(members, 10).dropDamage == 0);
}

TEST_CASE("LfgRoleGap: real players past the places leave the tank place open only when room can be made", "[lfggap]")
{
    // five tanks in a 5-man: one tank place, then the damage places; no damage dealer to make room for a healer
    std::vector<uint8> const tanks(5, kLfgRoleTank);
    CHECK(PlanLfgSeats(tanks, 5).gap == LfgRoleGap{ 0, 0, 0 });
    CHECK(PlanLfgSeats(tanks, 5).dropDamage == 0);
}

TEST_CASE("LfgRoleGap: the role mix of a 5-man party and a 10-man group", "[lfggap]")
{
    CHECK(LfgRoleMix(5) == LfgRoleGap{ 1, 1, 3 });
    CHECK(LfgRoleMix(10) == LfgRoleGap{ 1, 3, 6 });
    CHECK(LfgRoleMix(10).Total() == 10);
    CHECK(LfgRoleMix(3) == LfgRoleGap{ 1, 1, 3 });          // below a party counts as a party
    CHECK(LfgRoleMix(20) == LfgRoleGap{ 2, 6, 12 });
    CHECK(LfgRoleMix(40) == LfgRoleGap{ 4, 13, 23 });
}

TEST_CASE("LfgRoleGap: a 10-man group (Upper Blackrock Spire) lacks 1 tank, 3 healers, 6 damage when empty", "[lfggap]")
{
    CHECK(ComputeLfgRoleGap({}, 10) == LfgRoleGap{ 1, 3, 6 });
    CHECK(ComputeLfgRoleGap(std::vector<uint8>{ kLfgRoleDamage }, 10) == LfgRoleGap{ 1, 3, 5 });
}

TEST_CASE("LfgRoleGap: real players in a 10-man group take their roles first", "[lfggap]")
{
    std::vector<uint8> const tankAndHealers{ kLfgRoleTank, kLfgRoleHealer, kLfgRoleHealer };
    CHECK(ComputeLfgRoleGap(tankAndHealers, 10) == LfgRoleGap{ 0, 1, 6 });
    std::vector<uint8> const two{ kLfgRoleDamage, kLfgRoleDamage };
    CHECK(ComputeLfgRoleGap(two, 10) == LfgRoleGap{ 1, 3, 4 });
}

TEST_CASE("LfgRoleGap: a full 10-man group of the right mix needs no bots, an 11th member takes no place", "[lfggap]")
{
    std::vector<uint8> ten{ kLfgRoleTank, kLfgRoleHealer, kLfgRoleHealer, kLfgRoleHealer };
    ten.insert(ten.end(), 6, kLfgRoleDamage);
    CHECK(ComputeLfgRoleGap(ten, 10).Total() == 0);
    std::vector<uint8> eleven = ten;
    eleven.push_back(kLfgRoleDamage);
    CHECK(ComputeLfgRoleGap(eleven, 10).Total() == 0);
    // ten damage dealers lack the tank and the healers: four of them make room
    std::vector<uint8> const allDamage(10, kLfgRoleDamage);
    CHECK(PlanLfgSeats(allDamage, 10).gap == LfgRoleGap{ 1, 3, 0 });
    CHECK(PlanLfgSeats(allDamage, 10).dropDamage == 4);
}

TEST_CASE("LfgRoleGap: the 5-man default is unchanged by the size parameter", "[lfggap]")
{
    CHECK(ComputeLfgRoleGap({}, 5) == ComputeLfgRoleGap({}));
    std::vector<uint8> const six{ kLfgRoleTank, kLfgRoleHealer, kLfgRoleDamage, kLfgRoleDamage, kLfgRoleDamage, kLfgRoleDamage };
    CHECK(ComputeLfgRoleGap(six, 5).Total() == 0);
}

TEST_CASE("LfgFill: nobody in range fills at once, whatever time is left", "[lfgfill]")
{
    CHECK(DecideLfgFill(180000, 0, true) == LfgFillAction::FillNow);
    CHECK(DecideLfgFill(0, 0, true) == LfgFillAction::FillNow);
    CHECK(DecideLfgFill(-5000, 0, true) == LfgFillAction::FillNow);
}

TEST_CASE("LfgFill: with real players in range the bots wait for the full time", "[lfgfill]")
{
    CHECK(DecideLfgFill(180000, 1, true) == LfgFillAction::Wait);
    CHECK(DecideLfgFill(1, 7, true) == LfgFillAction::Wait);
    CHECK(DecideLfgFill(0, 3, true) == LfgFillAction::FillAfterWait);
    CHECK(DecideLfgFill(-1000, 3, true) == LfgFillAction::FillAfterWait);
}

TEST_CASE("LfgFill: with InstantFillIfNobody off an empty server still waits", "[lfgfill]")
{
    CHECK(DecideLfgFill(60000, 0, false) == LfgFillAction::Wait);
    CHECK(DecideLfgFill(0, 0, false) == LfgFillAction::FillAfterWait);
    CHECK(DecideLfgFill(60000, 4, false) == LfgFillAction::Wait);
}

TEST_CASE("LfgRoleGap: the party size of an activity", "[lfggap]")
{
    // the Map table says 10 for most classic dungeons: a dungeon is a 5-man unless the activity names a size
    CHECK(LfgActivityPartySize(0, false, 10) == 5);
    CHECK(LfgActivityPartySize(0, false, 5) == 5);
    CHECK(LfgActivityPartySize(10, false, 10) == 10);      // Upper Blackrock Spire
    // a raid is as big as its map
    CHECK(LfgActivityPartySize(0, true, 40) == 40);
    CHECK(LfgActivityPartySize(0, true, 20) == 20);
    CHECK(LfgActivityPartySize(0, true, 0) == 5);          // a raid map without a limit
    // held to 5-40
    CHECK(LfgActivityPartySize(2, false, 10) == 5);
    CHECK(LfgActivityPartySize(99, false, 10) == 40);
}

TEST_CASE("LfgRoleGap: a ten-man group lacks 1 tank, 3 healers and 6 damage", "[lfggap]")
{
    LfgRoleGap const mix = LfgRoleMix(LfgActivityPartySize(10, false, 10));
    CHECK(mix == LfgRoleGap{ 1, 3, 6 });
    std::vector<uint8> const own{ kLfgRoleDamage };
    LfgRoleGap const gap = ComputeLfgRoleGap(own, 10);
    CHECK(gap == LfgRoleGap{ 1, 3, 5 });
    CHECK(gap.Total() == 9);                                // 9 bots make a 10-man with the player
}

TEST_CASE("LfgRoleGap: a player joining a part-filled UBRS group leaves the rest to fill", "[lfggap]")
{
    // 4 bots (tank, healer, 2 damage) and the player (damage): 5 of 10 places taken, 2 healers and 3 damage dealers still due
    std::vector<uint8> const members{ kLfgRoleTank, kLfgRoleHealer, kLfgRoleDamage, kLfgRoleDamage, kLfgRoleDamage };
    LfgRoleGap const gap = ComputeLfgRoleGap(members, 10);
    CHECK(gap == LfgRoleGap{ 0, 2, 3 });
    CHECK(gap.Total() == 5);                                // 5 more bots: 10 in all
    // a 5-man with 3 bots and the player lacks one place
    std::vector<uint8> const small{ kLfgRoleTank, kLfgRoleDamage, kLfgRoleDamage, kLfgRoleDamage };
    CHECK(ComputeLfgRoleGap(small, 5) == LfgRoleGap{ 0, 1, 0 });
}

TEST_CASE("LfgRoleGap: the fill of a joined group is spread over at most a minute", "[lfggap]")
{
    CHECK(LfgJoinedFillSeconds(180) == 60);
    CHECK(LfgJoinedFillSeconds(60) == 60);
    CHECK(LfgJoinedFillSeconds(20) == 20);
    CHECK(LfgJoinedFillSeconds(0) == 0);                    // fill off stays off
}

TEST_CASE("LfgRoleGap: a role the class cannot play is not counted", "[lfggap]")
{
    CHECK(LfgRoleForClass(4, kLfgRoleTank) == 0);                       // rogue picked Tank
    CHECK(LfgRoleForClass(8, kLfgRoleHealer) == 0);                     // mage picked Heal
    CHECK(LfgRoleForClass(1, kLfgRoleTank) == kLfgRoleTank);            // warrior
    CHECK(LfgRoleForClass(11, kLfgRoleHealer) == kLfgRoleHealer);       // druid
    CHECK(LfgRoleForClass(2, kLfgRoleTank | kLfgRoleHealer) == (kLfgRoleTank | kLfgRoleHealer));
    CHECK(LfgRoleForClass(5, kLfgRoleTank | kLfgRoleDamage) == kLfgRoleDamage);
    CHECK(LfgRoleForClass(4, kLfgRoleDamage) == kLfgRoleDamage);
}

TEST_CASE("LfgRoleGap: a rogue leader who picked Tank still gets a tank (Scarlet Monastery 2026-11-08)", "[lfggap]")
{
    uint8 const own = LfgRoleForClass(4, kLfgRoleTank);
    CHECK(Gap({ own ? own : kLfgRoleDamage }) == LfgRoleGap{ 1, 1, 2 });
}
