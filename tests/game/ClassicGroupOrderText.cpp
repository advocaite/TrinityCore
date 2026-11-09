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

#include "../../src/modules/PlayerbotV2/Bot/GroupOrderText.h"

using namespace Playerbot::V2;

TEST_CASE("GroupOrder: exact orders are normalized", "[grouporder]")
{
    CHECK(ClassifyGroupOrder("follow", false) == "follow");
    CHECK(ClassifyGroupOrder("  Follow \t", false) == "follow");
    CHECK(ClassifyGroupOrder("assist", false) == "attack");
    CHECK(ClassifyGroupOrder("pullback", false) == "pull back");
    CHECK(ClassifyGroupOrder("Pull And Back", false) == "pull back");
}
TEST_CASE("GroupOrder: everyday words for stay and follow", "[grouporder]")
{
    CHECK(ClassifyGroupOrder("wait here", false) == "stay");
    CHECK(ClassifyGroupOrder("Wait here!", false) == "stay");
    CHECK(ClassifyGroupOrder("wait", false) == "stay");
    CHECK(ClassifyGroupOrder("stay here.", false) == "stay");
    CHECK(ClassifyGroupOrder("Follow me", false) == "follow");
    CHECK(ClassifyGroupOrder("come with me", false) == "follow");
    CHECK(ClassifyGroupOrder("\xD0\xB6\xD0\xB4\xD0\xB8", false) == "stay");                                   // "жди"
    CHECK(ClassifyGroupOrder("\xD0\xB7\xD0\xB0 \xD0\xBC\xD0\xBD\xD0\xBE\xD0\xB9", false) == "follow");    // "за мной"
    CHECK_FALSE(ClassifyGroupOrder("wait for the tank", false));
    CHECK_FALSE(ClassifyGroupOrder("!", false));
}
TEST_CASE("GroupOrder: mount, dismount and come here are orders, not chatter for the AI chat", "[grouporder]")
{
    CHECK(ClassifyGroupOrder("mount", false) == "mount");
    CHECK(ClassifyGroupOrder("Dismount", false) == "dismount");
    CHECK(ClassifyGroupOrder("mount up", false) == "mount");
    CHECK(ClassifyGroupOrder("mount up!", false) == "mount");
    CHECK(ClassifyGroupOrder("get off your mount", false) == "dismount");
    CHECK(ClassifyGroupOrder("come here", false) == "come");
    CHECK(ClassifyGroupOrder("\xD0\xBF\xD0\xBE \xD0\xBA\xD0\xBE\xD0\xBD\xD1\x8F\xD0\xBC", false) == "mount");
    CHECK_FALSE(ClassifyGroupOrder("nice mount", false).has_value());
}

TEST_CASE("GroupOrder: panel argument orders are recognized", "[grouporder]")
{
    CHECK(ClassifyGroupOrder("distance 8", false) == "distance 8");
    CHECK(ClassifyGroupOrder("assign skull 3", false) == "assign skull 3");
    CHECK(ClassifyGroupOrder("tank cross", false) == "tank cross");
    CHECK(ClassifyGroupOrder("formation circle", false) == "formation circle");
    CHECK(ClassifyGroupOrder("cc off", false) == "cc off");
}
TEST_CASE("GroupOrder: chatter and squad-prefix lines are not orders", "[grouporder]")
{
    CHECK_FALSE(ClassifyGroupOrder("", false));
    CHECK_FALSE(ClassifyGroupOrder("nice pull!", false));
    CHECK_FALSE(ClassifyGroupOrder("follow me please", false));
    CHECK_FALSE(ClassifyGroupOrder(";tank pull", false));
}
TEST_CASE("GroupOrder: owner words need the config switch", "[grouporder]")
{
    CHECK_FALSE(ClassifyGroupOrder("summon", false));
    CHECK(ClassifyGroupOrder("summon", true) == "summon");
    CHECK(ClassifyGroupOrder("drop quest Foo", true) == "drop quest foo");
    CHECK_FALSE(ClassifyGroupOrder("drop quest Foo", false));
    // The panel's stance re-send (stance only, an attack under way is kept).
    CHECK(ClassifyGroupOrder("stance passive", true) == "stance passive");
    CHECK(ClassifyGroupOrder("Stance Aggressive", true) == "stance aggressive");
    CHECK_FALSE(ClassifyGroupOrder("stance passive", false));
    // The three stances (0.1.29): Defensive, Aggressive and the full Passive, and their re-sends.
    CHECK(ClassifyGroupOrder("Passive", true) == "passive");
    CHECK(ClassifyGroupOrder("defensive", true) == "defensive");
    CHECK(ClassifyGroupOrder("stance defensive", true) == "stance defensive");
    CHECK(ClassifyGroupOrder("stance nofight", true) == "stance nofight");
    // Ignore target / Clear: the owner's list, a group order whether or not the plain words are on.
    CHECK(ClassifyGroupOrder("Ignore Target", false) == "ignore target");
    CHECK(ClassifyGroupOrder("ignore clear", false) == "ignore clear");
    CHECK_FALSE(ClassifyGroupOrder("ignore", false));
}
TEST_CASE("GroupOrder: a line addressed to a group bot by name is its order without the ';'", "[grouporder]")
{
    auto const is_bot = [](std::string_view name) { return name == "Lentha" || name == "baxter" || name == "Baxter"; };
    CHECK(AddressedGroupOrder("Lentha use altar", is_bot) == "Lentha:use altar");
    CHECK(AddressedGroupOrder("  Lentha   use altar  ", is_bot) == "Lentha:use altar");
    CHECK(AddressedGroupOrder("Baxter, follow", is_bot) == "Baxter:follow");
    CHECK(AddressedGroupOrder("Baxter: stay", is_bot) == "Baxter:stay");
    CHECK(AddressedGroupOrder("all use altar", is_bot) == "squad:use altar");
    CHECK(AddressedGroupOrder("Everyone follow", is_bot) == "squad:follow");
    // Not a group bot, no order after the name, chatter, or the ';' form (which has its own path).
    CHECK_FALSE(AddressedGroupOrder("Karrz use altar", is_bot));
    CHECK_FALSE(AddressedGroupOrder("Lentha", is_bot));
    CHECK_FALSE(AddressedGroupOrder("Lentha   ", is_bot));
    CHECK_FALSE(AddressedGroupOrder("nice pull Lentha", is_bot));
    CHECK_FALSE(AddressedGroupOrder(";Lentha:use altar", is_bot));
    CHECK_FALSE(AddressedGroupOrder("", is_bot));
}
TEST_CASE("GroupOrder: addon messages count only under the ForeverBots prefix", "[grouporder]")
{
    CHECK(kAddonOrderPrefix.size() <= 16);
    CHECK(ClassifyAddonOrder(kAddonOrderPrefix, "stay", false) == "stay");
    CHECK(ClassifyAddonOrder(kAddonOrderPrefix, "pull back", false) == "pull back");
    CHECK_FALSE(ClassifyAddonOrder("OTHER", "stay", false));
    CHECK_FALSE(ClassifyAddonOrder("", "stay", false));
    CHECK_FALSE(ClassifyAddonOrder(kAddonOrderPrefix, "hello there", false));
    CHECK_FALSE(ClassifyAddonOrder(kAddonOrderPrefix, ";follow", false));
}
