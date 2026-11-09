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

#include "../../src/modules/PlayerbotV2/DungeonFinderPlan.h"

#include <set>
#include <vector>

using namespace Playerbot::V2;

namespace {
// the default Playerbot.Lfg.BotGroups.Activities table (Util/ConfigReader.cpp)
std::vector<ListedActivity> Table()
{
    return ParseListedActivities(
        "203776:43:15:24,204032:289:58:60,204288:389:13:18,204544:36:17:26,204800:33:20:30,205056:48:24:32,"
        "205312:34:24:32,205568:90:29:38,205824:47:29:38,206080:189:30:38,206336:129:37:46,206592:70:41:51,"
        "206848:209:44:54,207104:349:46:55,207360:109:50:60,207616:230:52:60,207872:229:55:60,208128:429:54:60,"
        "208384:429:56:60,208640:429:56:60,208896:329:58:60,211712:189:36:44,211968:189:38:46,212224:189:33:41,"
        "214272:229:59:60:10");
}

std::vector<uint32> Activities(std::vector<ListedActivity> const& table, std::vector<size_t> const& picks)
{
    std::vector<uint32> out;
    for (size_t i : picks)
        out.push_back(table[i].activity);
    return out;
}
}

TEST_CASE("DungeonFinder: every dungeon of the default table has a name and its own door", "[dungeonfinder]")
{
    std::set<uint32> doors;
    for (ListedActivity const& act : Table())
    {
        FinderDungeonInfo const* info = FinderDungeonFor(act.activity);
        REQUIRE(info);
        CHECK(info->entrance != 0);
        doors.insert(info->entrance);
    }
    // the four Scarlet Monastery wings and the three Dire Maul wings each have their own door; LBRS and UBRS share Blackrock Spire's
    CHECK(doors.size() == Table().size() - 1);
}

TEST_CASE("DungeonFinder: short words and names find one dungeon", "[dungeonfinder]")
{
    auto const table = Table();
    CHECK(Activities(table, MatchFinderDungeon(table, "rfc")) == std::vector<uint32>{ 204288 });
    CHECK(Activities(table, MatchFinderDungeon(table, "DM")) == std::vector<uint32>{ 204544 });           // Deadmines, as players say it
    CHECK(Activities(table, MatchFinderDungeon(table, "dmn")) == std::vector<uint32>{ 208640 });
    CHECK(Activities(table, MatchFinderDungeon(table, "Zul'Farrak")) == std::vector<uint32>{ 206848 });
    CHECK(Activities(table, MatchFinderDungeon(table, "library")) == std::vector<uint32>{ 212224 });
    CHECK(Activities(table, MatchFinderDungeon(table, "ubrs")) == std::vector<uint32>{ 214272 });
}

TEST_CASE("DungeonFinder: a word in several names lists them all", "[dungeonfinder]")
{
    auto const table = Table();
    CHECK(MatchFinderDungeon(table, "scarlet").size() == 4);
    CHECK(MatchFinderDungeon(table, "maul").size() == 3);
    CHECK(MatchFinderDungeon(table, "blackrock").size() == 3);
    CHECK(MatchFinderDungeon(table, "nowhere").empty());
    CHECK(MatchFinderDungeon(table, "  ").empty());
}

TEST_CASE("DungeonFinder: any for my level picks 5-player dungeons of that level", "[dungeonfinder]")
{
    auto const table = Table();
    CHECK(Activities(table, FinderDungeonsForLevel(table, 15)) == std::vector<uint32>{ 203776, 204288 });     // WC, RFC
    // level 60: Upper Blackrock Spire (10 players) only by name
    for (size_t i : FinderDungeonsForLevel(table, 60))
        CHECK(table[i].size == 5);
    CHECK(FinderDungeonsForLevel(table, 60).size() == 8);
}

TEST_CASE("DungeonFinder: below every dungeon the nearest one is offered", "[dungeonfinder]")
{
    auto const table = Table();
    CHECK(Activities(table, FinderDungeonsForLevel(table, 8)) == std::vector<uint32>{ 204288 });              // RFC 13-18
}

TEST_CASE("DungeonFinder: a configured door wins over the named one", "[dungeonfinder]")
{
    auto const overrides = ParseFinderEntrances("208896:2214, bad, 1:0,206080:x");
    REQUIRE(overrides.size() == 1);
    CHECK(FinderEntranceTrigger(208896, overrides) == 2214);
    CHECK(FinderEntranceTrigger(206080, overrides) == 45);
    CHECK(FinderEntranceTrigger(999, overrides) == 0);
}

TEST_CASE("DungeonFinder: the group goes in when complete or with what came after the wait", "[dungeonfinder]")
{
    CHECK(DecideFinderEnter(5, 5, 0, 90000) == FinderFormAction::Enter);
    CHECK(DecideFinderEnter(3, 5, 10000, 90000) == FinderFormAction::Wait);
    CHECK(DecideFinderEnter(3, 5, 90000, 90000) == FinderFormAction::Enter);
    CHECK(DecideFinderEnter(1, 5, 90000, 90000) == FinderFormAction::GiveUp);
    CHECK(DecideFinderEnter(10, 10, 0, 90000) == FinderFormAction::Enter);
}
