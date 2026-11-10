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

#include "../../src/server/game/Groups/BotGroupRules.h"

using namespace BotGroupRules;

namespace
{
LooterCandidate Player(bool inRange = true) { return { inRange, false }; }
LooterCandidate Bot(bool inRange = true) { return { inRange, true }; }
}

TEST_CASE("A raid made of one player and companions still takes and progresses normal quests", "[ClassicBotGroupRules]")
{
    // player + 9 companions: a raid, one player
    REQUIRE_FALSE(QuestRaidRuleApplies(true, 1, 5));
    REQUIRE_FALSE(QuestRaidRuleApplies(true, 5, 5));
    // a raid of six players keeps the vanilla rule
    REQUIRE(QuestRaidRuleApplies(true, 6, 5));
    // a party is never a raid
    REQUIRE_FALSE(QuestRaidRuleApplies(false, 5, 5));
}

TEST_CASE("A bot in another subgroup tags for the players of its group", "[ClassicBotGroupRules]")
{
    // open world, raid group with bots: the player in subgroup 1 shares a tap made from subgroup 2
    REQUIRE(SharesTap(false, false, true, false));
    // the other subgroup's bots stay off the tap list (their loot would only take room)
    REQUIRE_FALSE(SharesTap(false, false, true, true));
    // same subgroup or a raid map: everyone, as before
    REQUIRE(SharesTap(false, true, true, true));
    REQUIRE(SharesTap(true, false, true, true));
}

TEST_CASE("A group without bots taps as before", "[ClassicBotGroupRules]")
{
    REQUIRE_FALSE(SharesTap(false, false, false, false));
    REQUIRE(SharesTap(false, true, false, false));
    REQUIRE(SharesTap(true, false, false, false));
}

TEST_CASE("Round robin skips the bots while a player is near the corpse", "[ClassicBotGroupRules]")
{
    // slot 0 player (leader), 1-4 bots
    std::vector<LooterCandidate> group{ Player(), Bot(), Bot(), Bot(), Bot() };

    // the player looted the last corpse: the turn would pass to bot 1, it stays with the player
    REQUIRE(NextRoundRobinLooter(group, 0, false) == 0);
    // a bot holding the turn loses it at the next kill (the ifneed check keeps only a looter that may take it)
    REQUIRE(NextRoundRobinLooter(group, 2, true) == 0);
    // the player keeps it
    REQUIRE(NextRoundRobinLooter(group, 0, true) == 0);
}

TEST_CASE("Round robin turns pass between the players of a group with bots", "[ClassicBotGroupRules]")
{
    std::vector<LooterCandidate> group{ Player(), Bot(), Player(), Bot() };
    REQUIRE(NextRoundRobinLooter(group, 0, false) == 2);
    REQUIRE(NextRoundRobinLooter(group, 2, false) == 0);
    REQUIRE(NextRoundRobinLooter(group, 1, false) == 2);
}

TEST_CASE("Bots take the turns when no player is near the corpse", "[ClassicBotGroupRules]")
{
    std::vector<LooterCandidate> group{ Player(false), Bot(), Bot() };
    REQUIRE(NextRoundRobinLooter(group, 0, false) == 1);
    REQUIRE(NextRoundRobinLooter(group, 1, false) == 2);
    REQUIRE(NextRoundRobinLooter(group, 2, false) == 1);
    REQUIRE(NextRoundRobinLooter(group, 1, true) == 1);
}

TEST_CASE("Round robin without bots rotates as before", "[ClassicBotGroupRules]")
{
    std::vector<LooterCandidate> group{ Player(), Player(false), Player(), Player() };
    REQUIRE(NextRoundRobinLooter(group, 0, false) == 2);   // the member out of range loses the turn
    REQUIRE(NextRoundRobinLooter(group, 3, false) == 0);   // wraps
    REQUIRE(NextRoundRobinLooter(group, 3, true) == 3);    // ifneed: an in-range looter keeps it
    REQUIRE(NextRoundRobinLooter(group, 1, true) == 2);    // ifneed: out of range passes on
    REQUIRE(NextRoundRobinLooter(group, 4, false) == 0);   // no current looter: the first in range
    REQUIRE(NextRoundRobinLooter({ Player(false), Bot(false) }, 0, false) == 2);   // nobody in range
}

TEST_CASE("A raid of a player and bots that fits a party goes back to one", "[ClassicBotGroupRules]")
{
    // player + 2 bots left in a raid, open world
    REQUIRE(RevertBotRaidToParty(true, false, 3, 5, true, true, false));
    REQUIRE(RevertBotRaidToParty(true, false, 5, 5, true, true, false));
    // six members: stays a raid
    REQUIRE_FALSE(RevertBotRaidToParty(true, false, 6, 5, true, true, false));
    // someone is on a raid map: the instance needs the raid
    REQUIRE_FALSE(RevertBotRaidToParty(true, false, 3, 5, true, true, true));
    // battleground or dungeon finder groups are left alone
    REQUIRE_FALSE(RevertBotRaidToParty(true, true, 3, 5, true, true, false));
    // a raid of players only, or of bots only, keeps its vanilla behaviour
    REQUIRE_FALSE(RevertBotRaidToParty(true, false, 3, 5, false, true, false));
    REQUIRE_FALSE(RevertBotRaidToParty(true, false, 3, 5, true, false, false));
    // already a party
    REQUIRE_FALSE(RevertBotRaidToParty(false, false, 3, 5, true, true, false));
}
