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

#include "../../src/modules/PlayerbotV2/Bot/Gear/RaidLoadout.cpp"

using namespace Playerbot::Gear::Loadout;

TEST_CASE("Every role has a chest and a main wish, and no slot is wished twice", "[ClassicRaidLoadout]")
{
    for (Role r : { Role::Tank, Role::Healer, Role::Caster, Role::Melee, Role::Hunter })
        for (bool agi : { false, true })
            for (std::uint8_t school : { 0, 2, 3, 4, 5 })
            {
                auto const wishes = WishesFor(r, agi, school);
                REQUIRE_FALSE(wishes.empty());
                bool chest = false;
                std::uint32_t seen = 0;
                for (EnchantWish const& w : wishes)
                {
                    CHECK_FALSE(w.names.empty());
                    CHECK((seen & (1u << w.slot)) == 0);
                    seen |= 1u << w.slot;
                    chest = chest || w.slot == SLOT_CHEST;
                }
                CHECK(chest);
            }
}

TEST_CASE("Every role enchants each slot the dps scorecard's readiness audit checks", "[ClassicRaidLoadout]")
{
    for (Role r : { Role::Tank, Role::Healer, Role::Caster, Role::Melee, Role::Hunter })
        for (std::uint8_t school : { 0, 2, 3, 4, 5 })
        {
            auto const wishes = WishesFor(r, false, school);
            std::uint32_t seen = 0;
            for (EnchantWish const& w : wishes)
                seen |= 1u << w.slot;
            for (std::uint8_t slot : { SLOT_HEAD, SLOT_SHOULDERS, SLOT_BACK, SLOT_CHEST, SLOT_WRISTS, SLOT_LEGS, SLOT_FEET, SLOT_MAINHAND })
                CHECK((seen & (1u << slot)) != 0);
            // Gloves: every role but a caster with no school (a priest or warlock takes none of the school gloves it has no school for).
            if (r != Role::Caster || school != 0)
                CHECK((seen & (1u << SLOT_HANDS)) != 0);
        }
}

TEST_CASE("Gloves follow the main stat and the caster's school", "[ClassicRaidLoadout]")
{
    auto hands = [](std::span<EnchantWish const> w) -> std::string_view
    {
        for (EnchantWish const& e : w)
            if (e.slot == SLOT_HANDS)
                return e.names;
        return {};
    };
    CHECK(hands(WishesFor(Role::Melee, false, 0)).find("Strength") != std::string_view::npos);
    CHECK(hands(WishesFor(Role::Melee, true, 0)).find("Agility") != std::string_view::npos);
    CHECK(hands(WishesFor(Role::Caster, false, 2)).find("Fire Power") != std::string_view::npos);
    CHECK(hands(WishesFor(Role::Caster, false, 4)).find("Frost Power") != std::string_view::npos);
    CHECK(hands(WishesFor(Role::Caster, false, 5)).find("Shadow Power") != std::string_view::npos);
    CHECK(hands(WishesFor(Role::Caster, false, 3)).find("Natural Power") != std::string_view::npos);
    CHECK(hands(WishesFor(Role::Healer, false, 0)) == "Enchant Gloves - Healing Power");
}

TEST_CASE("The first candidate the data knows is taken", "[ClassicRaidLoadout]")
{
    auto knows = [](std::string_view n) { return n == "B" || n == "C"; };
    CHECK(FirstKnown("A|B|C", knows) == "B");
    CHECK(FirstKnown("C", knows) == "C");
    CHECK(FirstKnown("A|D", knows).empty());
    CHECK(FirstKnown("", knows).empty());
}

TEST_CASE("Enchants follow the bot's gear level", "[ClassicRaidLoadout]")
{
    CHECK(TierForAverageItemLevel(0) == EnchantTier::Cheap);
    CHECK(TierForAverageItemLevel(kStandardEnchantIlvl - 1) == EnchantTier::Cheap);
    CHECK(TierForAverageItemLevel(kStandardEnchantIlvl) == EnchantTier::Standard);
    CHECK(TierForAverageItemLevel(83) == EnchantTier::Standard);   // Blackwing Lair
    CHECK(TierForAverageItemLevel(88) == EnchantTier::Naxx);       // Ahn'Qiraj 40, tier 2.5

    CHECK(TierOfEnchant("Might of the Scourge") == EnchantTier::Naxx);
    CHECK(TierOfEnchant("Fortitude of the Scourge") == EnchantTier::Naxx);
    CHECK(TierOfEnchant("Power of the Scourge") == EnchantTier::Naxx);
    CHECK(TierOfEnchant("Enchant Weapon - Crusader") == EnchantTier::Standard);
    CHECK(TierOfEnchant("Zandalar Signet of Might") == EnchantTier::Standard);
    CHECK(TierOfEnchant("Arcanum of Rapidity") == EnchantTier::Standard);
    CHECK(TierOfEnchant("Enchant Bracer - Strength") == EnchantTier::Cheap);

    // A shoulder wish in greens gets nothing, in BWL gear the Zandalar signet, in tier 2.5 the Scourge enchant.
    auto shoulders = [](EnchantTier t)
    {
        for (EnchantWish const& w : WishesFor(Role::Melee, false, 0))
            if (w.slot == SLOT_SHOULDERS)
                return FirstKnown(w.names, [&](std::string_view n) { return EnchantAllowed(n, t); });
        return std::string_view{};
    };
    CHECK(shoulders(EnchantTier::Cheap).empty());
    CHECK(shoulders(EnchantTier::Standard) == "Zandalar Signet of Might");
    CHECK(shoulders(EnchantTier::Naxx) == "Might of the Scourge");

    // Every role's cheap tier still has bracers.
    for (Role r : { Role::Tank, Role::Healer, Role::Caster, Role::Melee, Role::Hunter })
        for (EnchantWish const& w : WishesFor(r, false, 2))
            if (w.slot == SLOT_WRISTS && r != Role::Healer)
                CHECK_FALSE(FirstKnown(w.names, [](std::string_view n) { return EnchantAllowed(n, EnchantTier::Cheap); }).empty());
}
