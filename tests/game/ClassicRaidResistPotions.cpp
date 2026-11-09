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

#include <algorithm>

#include "../../src/modules/PlayerbotV2/Bot/Raid/RaidEncounters.h"
#include "../../src/modules/PlayerbotV2/Bot/ResistGear.h"

using namespace Playerbot::Raid;
using Playerbot::Classic::ResistSwapMap;
using Playerbot::Classic::ResistSwapSlot;
using Playerbot::Classic::FightCloakMap;
using Playerbot::Classic::KeepFightCloak;
using Playerbot::Classic::kOnyxiaScaleCloak;

namespace
{
constexpr uint32_t kFire = 13457, kFrost = 13456, kNature = 13458, kShadow = 13459;

uint32_t PotionFor(uint32_t map, uint32_t boss)
{
    RaidRules const* r = RulesForMap(map);
    REQUIRE(r != nullptr);
    for (ResistPotion const& p : r->resistPotion)
        if (p.enemy == boss)
        {
            CHECK(p.who == Who::Anyone);
            return p.item;
        }
    return 0;
}
}

// Per-fight Greater Protection Potion, by the school of the boss's spells (vMaNGOS spell data).
TEST_CASE("each raid boss with a school-bound hit orders the matching resistance potion", "[ClassicRaidResistPotions]")
{
    CHECK(PotionFor(469, 13020) == kFire);     // Vaelastrasz: Flame Breath
    CHECK(PotionFor(469, 11983) == kFire);     // Firemaw: Flame Buffet
    CHECK(PotionFor(469, 14601) == kShadow);   // Ebonroc: Shadow Flame
    CHECK(PotionFor(469, 11981) == kShadow);   // Flamegor
    CHECK(PotionFor(469, 11583) == kShadow);   // Nefarian
    CHECK(PotionFor(469, 10162) == kFire);     // Victor Nefarius' drakonids: Brood Power Black / Red
    CHECK(PotionFor(531, 15509) == kNature);   // Huhuran: Poison Bolt Volley
    CHECK(PotionFor(533, 15989) == kFrost);    // Sapphiron
}

TEST_CASE("the frozen Molten Core and Onyxia potion orders are unchanged", "[ClassicRaidResistPotions]")
{
    CHECK(PotionFor(409, 12056) == kFire);   // Baron Geddon
    CHECK(PotionFor(409, 11502) == kFire);   // Ragnaros
    CHECK(PotionFor(249, 10184) == kFire);   // Onyxia
}

TEST_CASE("a tank keeps the armor of its head, shoulders, chest, legs, hands and feet when it dresses for a school", "[ClassicRaidResistPotions]")
{
    for (std::uint8_t slot : { 0, 2, 4, 6, 7, 9 })   // head, shoulders, chest, legs, feet, hands
    {
        CHECK(ResistSwapSlot(slot, false));
        CHECK_FALSE(ResistSwapSlot(slot, true));
    }
    for (std::uint8_t slot : { 1, 5, 8, 10, 11, 14 })   // neck, waist, wrists, rings, cloak
    {
        CHECK(ResistSwapSlot(slot, false));
        CHECK(ResistSwapSlot(slot, true));
    }
    for (std::uint8_t slot : { 3, 12, 13, 15, 16, 17, 18 })   // shirt, trinkets, weapons, ranged, tabard
    {
        CHECK_FALSE(ResistSwapSlot(slot, false));
        CHECK_FALSE(ResistSwapSlot(slot, true));
    }
}

TEST_CASE("the gear swap skips the frozen Molten Core and Onyxia maps", "[ClassicRaidResistPotions]")
{
    CHECK_FALSE(ResistSwapMap(409));
    CHECK_FALSE(ResistSwapMap(249));
    for (std::uint32_t map : { 469u, 531u, 533u, 309u, 509u })
        CHECK(ResistSwapMap(map));
}

TEST_CASE("the Onyxia Scale Cloak is worn and kept only in Blackwing Lair", "[ClassicRaidResistPotions]")
{
    CHECK(kOnyxiaScaleCloak == 15138u);
    CHECK(FightCloakMap(469));
    for (std::uint32_t map : { 409u, 249u, 531u, 533u, 309u, 509u, 0u })
        CHECK_FALSE(FightCloakMap(map));
    CHECK(KeepFightCloak(469, kOnyxiaScaleCloak));
    CHECK_FALSE(KeepFightCloak(469, 12345u));
    CHECK_FALSE(KeepFightCloak(409, kOnyxiaScaleCloak));
}
