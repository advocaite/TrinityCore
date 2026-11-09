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

#include "../../src/modules/PlayerbotV2/Bot/PlayerCraftOrder.h"

#include <vector>

using namespace Playerbot::V2::PlayerCraft;

TEST_CASE("PlayerCraftOrder: parsing a craft order", "[playercraft]")
{
    SECTION("not a craft order")
    {
        CHECK(ParseRequest("follow", 20).type == Kind::None);
        CHECK(ParseRequest("crafty bandage", 20).type == Kind::None);
        CHECK(ParseRequest("craft", 20).type == Kind::None);
        CHECK(ParseRequest("craft   ", 20).type == Kind::None);
    }
    SECTION("list")
    {
        CHECK(ParseRequest("craft list", 20).type == Kind::List);
        CHECK(ParseRequest("  CRAFT   List ", 20).type == Kind::List);
    }
    SECTION("by name, count first, last or absent")
    {
        Request r = ParseRequest("craft Linen Bandage", 20);
        CHECK(r.type == Kind::Craft);
        CHECK(r.name == "linen bandage");
        CHECK(r.count == 1);
        r = ParseRequest("craft linen bandage x5", 20);
        CHECK(r.name == "linen bandage");
        CHECK(r.count == 5);
        r = ParseRequest("craft 5x linen bandage", 20);
        CHECK(r.name == "linen bandage");
        CHECK(r.count == 5);
        r = ParseRequest("craft linen bandage 7", 20);
        CHECK(r.name == "linen bandage");
        CHECK(r.count == 7);
    }
    SECTION("the count is clamped to 1..max")
    {
        CHECK(ParseRequest("craft linen bandage x500", 20).count == 20);
        CHECK(ParseRequest("craft linen bandage x0", 20).count == 1);
    }
    SECTION("a number inside the name is not a count")
    {
        Request const r = ParseRequest("craft elixir of 5 wonders", 20);
        CHECK(r.name == "elixir of 5 wonders");
        CHECK(r.count == 1);
    }
    SECTION("by item link, with and without colour code and count")
    {
        Request r = ParseRequest("craft |cffffffff|Hitem:1251::::::::60:::::|h[Linen Bandage]|h|r x3", 20);
        CHECK(r.type == Kind::Craft);
        CHECK(r.item_entry == 1251);
        CHECK(r.name.empty());
        CHECK(r.count == 3);
        r = ParseRequest("craft |Hitem:2996::::|h[Bolt of Linen Cloth]|h", 20);
        CHECK(r.item_entry == 2996);
        CHECK(r.count == 1);
        r = ParseRequest("craft 4 |Hitem:2996::::|h[Bolt of Linen Cloth]|h", 20);
        CHECK(r.item_entry == 2996);
        CHECK(r.count == 4);
    }
}

TEST_CASE("PlayerCraftOrder: matching a recipe", "[playercraft]")
{
    std::vector<Option> const opts = {
        { 3275, 1251, 1, "linen bandage" },
        { 3276, 1659, 1, "heavy linen bandage" },
        { 2963, 2996, 1, "bolt of linen cloth" },
    };
    CHECK(Match(ParseRequest("craft linen bandage", 20), opts) == 0);       // exact beats substring of "heavy linen bandage"
    CHECK(Match(ParseRequest("craft heavy", 20), opts) == 1);               // prefix
    CHECK(Match(ParseRequest("craft cloth", 20), opts) == 2);               // substring
    CHECK(Match(ParseRequest("craft |Hitem:2996::::|h[x]|h", 20), opts) == 2);  // link by entry
    CHECK(Match(ParseRequest("craft |Hitem:9999::::|h[x]|h", 20), opts) == -1);
    CHECK(Match(ParseRequest("craft sword", 20), opts) == -1);

    std::vector<Option> const ties = { { 20, 1, 1, "bandage b" }, { 10, 2, 1, "bandage a" } };
    CHECK(Match(ParseRequest("craft bandage", 20), ties) == 1);             // same length: lower spell id
}

TEST_CASE("PlayerCraftOrder: a generic name gets the best rank", "[playercraft]")
{
    // "craft water" fitted every Conjure Water rank and the shortest name (rank 1) won: a player asked a level 60 mage and got 151 mana water.
    std::vector<Option> const water = {
        { 5504, 5350, 3, "conjured water", 0 * 1000 + 5 },
        { 10138, 8077, 3, "conjured mineral water", 25 * 1000 + 25 },
        { 10140, 8079, 10, "conjured crystal water", 45 * 1000 + 45 },
        { 5505, 1179, 3, "conjured fresh water", 5 * 1000 + 10 },
    };
    CHECK(Match(ParseRequest("craft water", 20), water) == 2);              // the best rank
    CHECK(Match(ParseRequest("craft conjured water", 20), water) == 0);     // an exact name still means exactly that item
    CHECK(Match(ParseRequest("craft crystal water", 20), water) == 2);
    CHECK(Match(ParseRequest("craft mineral", 20), water) == 1);
}

TEST_CASE("PlayerCraftOrder: a recipe name when no item name fits", "[playercraft]")
{
    // "craft food" named no item (Conjured Muffin .. Conjured Cinnamon Roll), so the add-on's Conjure food order found nothing.
    std::vector<Option> const mage = {
        { 587, 5349, 2, "conjured muffin", 1 * 1000 + 5, "conjure food" },
        { 28612, 22895, 20, "conjured cinnamon roll", 55 * 1000 + 65, "conjure food" },
        { 10144, 8075, 3, "conjured sourdough", 35 * 1000 + 45, "conjure food" },
        { 5504, 5350, 3, "conjured water", 0 * 1000 + 5, "conjure water" },
        { 10140, 8079, 10, "conjured crystal water", 45 * 1000 + 45, "conjure water" },
    };
    CHECK(Match(ParseRequest("craft food", 20), mage) == 1);                // the best Conjure Food rank
    CHECK(Match(ParseRequest("craft conjure food", 20), mage) == 1);        // the exact recipe name is every rank: still the best
    CHECK(Match(ParseRequest("craft water", 20), mage) == 4);               // an item name fits first, as before
    CHECK(Match(ParseRequest("craft muffin", 20), mage) == 0);
    CHECK(Match(ParseRequest("craft sword", 20), mage) == -1);
}

TEST_CASE("PlayerCraftOrder: the best rank the requester can use", "[playercraft]")
{
    // A level 60 mage gave Conjured Crystal Water (level 55) to a level 30 player, who could not drink it.
    std::vector<Option> const mage = {
        { 5504, 5350, 3, "conjured water", 0 * 1000 + 5, "conjure water" },
        { 10138, 8077, 3, "conjured mineral water", 25 * 1000 + 25, "conjure water" },
        { 10140, 8079, 10, "conjured crystal water", 55 * 1000 + 55, "conjure water" },
        { 587, 5349, 2, "conjured muffin", 1 * 1000 + 5, "conjure food" },
        { 10144, 8075, 3, "conjured sourdough", 35 * 1000 + 45, "conjure food" },
        { 28612, 22895, 20, "conjured cinnamon roll", 55 * 1000 + 65, "conjure food" },
    };
    CHECK(Match(ParseRequest("craft water", 20), mage, 30) == 1);           // mineral (25), not crystal (55)
    CHECK(Match(ParseRequest("craft water", 20), mage, 60) == 2);
    CHECK(Match(ParseRequest("craft water", 20), mage, 10) == 0);
    CHECK(Match(ParseRequest("craft food", 20), mage, 40) == 4);            // sourdough (35)
    CHECK(Match(ParseRequest("craft food", 20), mage, 0) == 5);             // no level given: the best rank
    CHECK(Match(ParseRequest("craft crystal water", 20), mage, 30) == 2);   // a name only one item fits still gets that item
}

TEST_CASE("PlayerCraftOrder: water and food on a server in another language", "[playercraft]")
{
    // A ruRU server names its items in Russian, so "craft water" fitted no name (Tornn, 2026-10-27). The conjured kind comes from the
    // spell data: the English words, and the Russian ones, find it whatever the names are.
    std::vector<Option> const mage = {
        { 5504, 5350, 3, "\xd1\x81\xd0\xbe\xd1\x82\xd0\xb2\xd0\xbe\xd1\x80\xd0\xb5\xd0\xbd\xd0\xbd\xd0\xb0\xd1\x8f", 0 * 1000 + 5, "", Conjured::Water },
        { 10138, 8077, 3, "x", 25 * 1000 + 25, "", Conjured::Water },
        { 10140, 8079, 10, "y", 55 * 1000 + 55, "", Conjured::Water },
        { 587, 5349, 2, "z", 1 * 1000 + 5, "", Conjured::Food },
        { 10144, 8075, 3, "w", 35 * 1000 + 45, "", Conjured::Food },
        { 2963, 2996, 1, "water bolt of linen", 1 * 1000 + 5 },             // not conjured: a name with "water" in it
    };
    CHECK(Match(ParseRequest("craft water x20", 20), mage, 60) == 2);
    CHECK(Match(ParseRequest("craft water", 20), mage, 30) == 1);           // the usable rank still wins
    CHECK(Match(ParseRequest("craft food", 20), mage, 60) == 4);
    CHECK(Match(ParseRequest("craft Drink", 20), mage, 60) == 2);
    CHECK(Match(ParseRequest("craft \xd0\xb2\xd0\xbe\xd0\xb4\xd1\x83 x20", 20), mage, 60) == 2);   // "craft vodu x20"
    CHECK(Match(ParseRequest("craft \xd0\x95\xd0\xb4\xd0\xb0", 20), mage, 60) == 4);               // "craft Eda"
    CHECK(Match(ParseRequest("craft y", 20), mage, 30) == 2);               // a name still works

    // A bot that conjures nothing: the keyword falls back to the names.
    std::vector<Option> const tailor = { { 2963, 2996, 1, "water bolt of linen", 1 * 1000 + 5 } };
    CHECK(Match(ParseRequest("craft water", 20), tailor) == 0);
    CHECK(Match(ParseRequest("craft food", 20), tailor) == -1);
}

TEST_CASE("PlayerCraftOrder: planning the casts", "[playercraft]")
{
    Plan p = PlanOrder({ 5, 1, 20, 10 });
    CHECK(p.casts == 5);
    CHECK(p.items == 5);
    CHECK_FALSE(p.trimmed);
    CHECK(p.refusal == nullptr);

    p = PlanOrder({ 5, 2, 20, 10 });           // rounds up: 3 casts make 6
    CHECK(p.casts == 3);
    CHECK(p.items == 6);

    p = PlanOrder({ 10, 1, 20, 4 });           // reagents run out first
    CHECK(p.casts == 4);
    CHECK(p.trimmed);

    p = PlanOrder({ 50, 1, 20, 100 });         // the per order cap
    CHECK(p.items == 20);
    CHECK(p.trimmed);

    p = PlanOrder({ 5, 1, 20, 0 });
    CHECK(std::string(p.refusal) == "no_reagents");
    p = PlanOrder({ 5, 30, 20, 3 });           // one cast already makes more than the cap
    CHECK(std::string(p.refusal) == "too_many");
}

TEST_CASE("PlayerCraftOrder: a unique item the bot already carries is not made again", "[playercraft]")
{
    CHECK(CarryRoom(0, 5) == 0xFFFFFFFFu);     // no limit
    CHECK(CarryRoom(1, 0) == 1);
    CHECK(CarryRoom(1, 1) == 0);               // Mana Jade: one carried
    CHECK(CarryRoom(5, 3) == 2);

    Plan p = PlanOrder({ 1, 1, 20, 1000, CarryRoom(1, 1) });
    CHECK(std::string(p.refusal) == "already_carried");

    p = PlanOrder({ 3, 1, 20, 1000, CarryRoom(1, 0) });      // asked for three, may hold one
    CHECK(p.refusal == nullptr);
    CHECK(p.casts == 1);
    CHECK(p.items == 1);
    CHECK(p.trimmed);

    p = PlanOrder({ 5, 1, 20, 10, CarryRoom(0, 0) });        // no limit changes nothing
    CHECK(p.items == 5);
}

TEST_CASE("PlayerCraftOrder: how a finished order reaches the requester", "[playercraft]")
{
    CHECK(DeliveryFor(false, false) == Delivery::Mail);
    CHECK(DeliveryFor(true, false) == Delivery::Trade);      // conjured: the mail refuses it
    CHECK(DeliveryFor(false, true) == Delivery::Bound);      // soulbound: neither route
    CHECK(DeliveryFor(true, true) == Delivery::Bound);
}

TEST_CASE("PlayerCraftOrder: a cast waits for mana", "[playercraft]")
{
    // Out of mana, each cast stood the drinking mage up again and it bobbed up and down on the spot (Tornn, 2026-10-27).
    CHECK(WaitForMana(true, 300, 2000, 400));       // drinking, short of the cost: keep drinking
    CHECK(WaitForMana(true, 1500, 2000, 400));      // drinking with enough for one cast: finish the drink first
    CHECK_FALSE(WaitForMana(true, 2000, 2000, 400)); // the drink is done: cast
    CHECK(WaitForMana(false, 300, 2000, 400));      // not drinking yet, short of the cost: leave the drink rule to sit it down
    CHECK_FALSE(WaitForMana(false, 400, 2000, 400));
    CHECK_FALSE(WaitForMana(true, 0, 0, 0));         // no mana cost (a tailoring recipe): never waits
}
