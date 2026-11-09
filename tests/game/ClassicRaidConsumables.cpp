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

#include "ClassicSupplies.h"
#include <algorithm>
#include <set>

using namespace Playerbot::Classic;

namespace
{
constexpr RaidConsumableRole kRoles[] = { RaidConsumableRole::Tank, RaidConsumableRole::Melee, RaidConsumableRole::Caster,
                                          RaidConsumableRole::Healer, RaidConsumableRole::Hunter };
constexpr uint32 FLASK_OF_THE_TITANS = 13510;
}

TEST_CASE("every raid role has a kit of unique, known raid consumables", "[classic][consumables]")
{
    for (RaidConsumableRole role : kRoles)
    {
        auto const kit = RaidConsumablesFor(role);
        REQUIRE_FALSE(kit.empty());
        std::set<uint32> seen;
        for (RaidConsumable const& c : kit)
        {
            CHECK(seen.insert(c.item).second);
            CHECK(IsRaidConsumable(c.item));
            CHECK(c.stock > 0);
        }
    }
    CHECK_FALSE(IsRaidConsumable(0));
    CHECK_FALSE(IsRaidConsumable(6948));   // Hearthstone is not a raid consumable
}

TEST_CASE("a tank's kit starts with the Flask of the Titans", "[classic][consumables]")
{
    auto const kit = RaidConsumablesFor(RaidConsumableRole::Tank);
    REQUIRE_FALSE(kit.empty());
    CHECK(kit.front().item == FLASK_OF_THE_TITANS);
    CHECK_FALSE(kit.front().food);
}

TEST_CASE("the next raid consumable is the first one missing its buff that the bags hold", "[classic][consumables]")
{
    auto const kit = RaidConsumablesFor(RaidConsumableRole::Tank);
    auto none_buffed = [](RaidConsumable const&) { return false; };
    auto all_buffed  = [](RaidConsumable const&) { return true; };
    auto holds_all   = [](uint32) { return true; };
    auto holds_none  = [](uint32) { return false; };

    // Everything in the bags, nothing buffed: the kit's first entry.
    RaidConsumable const* next = NextRaidConsumable(RaidConsumableRole::Tank, holds_all, none_buffed);
    REQUIRE(next != nullptr);
    CHECK(next->item == kit.front().item);

    // The flask is up: the elixirs behind it would take it off, so the next entry is the food.
    auto flask_buffed = [&](RaidConsumable const& c) { return c.item == kit.front().item; };
    next = NextRaidConsumable(RaidConsumableRole::Tank, holds_all, flask_buffed);
    REQUIRE(next != nullptr);
    CHECK(next->food);

    // No flask up and none in the bags: an elixir is still used.
    next = NextRaidConsumable(RaidConsumableRole::Tank, [&](uint32 i) { return i == kit[1].item; }, none_buffed);
    REQUIRE(next != nullptr);
    CHECK(next->item == kit[1].item);

    // Only the last entry used before the pull is in the bags (a combat potion is never used out of the fight).
    uint32 last = 0;
    for (RaidConsumable const& c : kit)
        if (!c.combat)
            last = c.item;
    REQUIRE(last != 0);
    next = NextRaidConsumable(RaidConsumableRole::Tank, [&](uint32 i) { return i == last; }, none_buffed);
    REQUIRE(next != nullptr);
    CHECK(next->item == last);

    // Fully buffed, or nothing in the bags: nothing to use.
    CHECK(NextRaidConsumable(RaidConsumableRole::Tank, holds_all, all_buffed) == nullptr);
    CHECK(NextRaidConsumable(RaidConsumableRole::Tank, holds_none, none_buffed) == nullptr);
}

TEST_CASE("the first raid consumable missing from the bags is the one to stock", "[classic][consumables]")
{
    auto const kit = RaidConsumablesFor(RaidConsumableRole::Healer);
    auto none_buffed = [](RaidConsumable const&) { return false; };
    // Bags empty: stock the first entry. Bags hold the first: stock the second.
    RaidConsumable const* stock = NextRaidConsumableToStock(RaidConsumableRole::Healer, [](uint32) { return 0u; }, none_buffed);
    REQUIRE(stock != nullptr);
    CHECK(stock->item == kit.front().item);
    stock = NextRaidConsumableToStock(RaidConsumableRole::Healer, [&](uint32 i) { return i == kit.front().item ? 5u : 0u; }, none_buffed);
    REQUIRE(stock != nullptr);
    CHECK(stock->item == kit[1].item);
    // A buffed entry needs no stock; everything stocked needs none.
    CHECK(NextRaidConsumableToStock(RaidConsumableRole::Healer, [](uint32) { return 99u; }, none_buffed) == nullptr);
}

TEST_CASE("an elixir is never used or stocked while a flask of the kit is up", "[classic][consumables]")
{
    // MC Garr 2026-10-11: Mageblood used on top of a Flask of Distilled Wisdom took the flask's +2000 mana off the healer.
    auto const kit = RaidConsumablesFor(RaidConsumableRole::Healer);
    auto flask_buffed = [&](RaidConsumable const& c) { return c.flask; };
    auto holds_all = [](uint32) { return true; };
    RaidConsumable const* next = NextRaidConsumable(RaidConsumableRole::Healer, holds_all, flask_buffed);
    REQUIRE(next != nullptr);
    CHECK(next->food);
    RaidConsumable const* stock = NextRaidConsumableToStock(RaidConsumableRole::Healer, [](uint32) { return 0u; }, flask_buffed);
    REQUIRE(stock != nullptr);
    CHECK(stock->food);
    CHECK(kit.front().flask);
    CHECK(RaidConsumablesFor(RaidConsumableRole::Tank).front().flask);
    CHECK(RaidConsumablesFor(RaidConsumableRole::Caster).front().flask);
}

TEST_CASE("a combat potion is stocked but never used before the pull", "[classic][consumables]")
{
    auto const kit = RaidConsumablesFor(RaidConsumableRole::Tank);
    auto it = std::find_if(kit.begin(), kit.end(), [](RaidConsumable const& c) { return c.item == 13457; });   // Greater Fire Protection Potion
    REQUIRE(it != kit.end());
    CHECK(it->combat);
    CHECK(IsRaidConsumable(13457));
    auto none_buffed = [](RaidConsumable const&) { return false; };
    // Only the potion in the bags: nothing to use out of combat.
    CHECK(NextRaidConsumable(RaidConsumableRole::Tank, [](uint32 i) { return i == 13457; }, none_buffed) == nullptr);
    // Everything else stocked, the potion missing: it is the one to stock, flask up or not.
    auto holds_all_but_potion = [](uint32 i) { return i == 13457 ? 0u : 99u; };
    RaidConsumable const* stock = NextRaidConsumableToStock(RaidConsumableRole::Tank, holds_all_but_potion, none_buffed);
    REQUIRE(stock != nullptr);
    CHECK(stock->item == 13457);
    auto flask_buffed = [](RaidConsumable const& c) { return c.flask; };
    stock = NextRaidConsumableToStock(RaidConsumableRole::Tank, holds_all_but_potion, flask_buffed);
    REQUIRE(stock != nullptr);
    CHECK(stock->item == 13457);
    // Every role carries the four Greater Protection Potions (fire 13457, shadow 13459, nature 13458, frost 13456) and Hourglass Sand 19183 (the cure for
    // Chromaggus' Brood Affliction: Bronze, RaidRules::cureItem), and only those are combat items; the casters' stock-only extras are listed below.
    for (RaidConsumableRole role : { RaidConsumableRole::Melee, RaidConsumableRole::Caster, RaidConsumableRole::Healer, RaidConsumableRole::Hunter })
    {
        std::set<uint32> combat;
        for (RaidConsumable const& c : RaidConsumablesFor(role))
            if (c.combat)
                combat.insert(c.item);
        std::set<uint32> expected{ 13457, 13459, 13458, 13456, 19183 };
        // The caster kit also stocks what is used outside the pre-pull picker: the weapon oil (idle:classic_weapon), the mana potion and the runes (fight rule).
        if (role == RaidConsumableRole::Caster)
            expected.insert({ 20749, 13444, 12662, 20520 });
        CHECK(combat == expected);
    }
}
