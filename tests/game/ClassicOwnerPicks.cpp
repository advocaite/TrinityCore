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
#include "tc_catch2.h"

#include "../../src/modules/PlayerbotV2/Bot/OwnerPicks.h"
#include "../../src/modules/PlayerbotV2/Bot/CcDeny.h"

#include <ostream>
#include <set>
#include <utility>

namespace Playerbot::OwnerPicks {
std::ostream& operator<<(std::ostream& os, Order const& o)
{
    if (o.clear_all) return os << "clear all";
    return os << SlotName(o.slot) << " " << o.spell;
}
}

using namespace Playerbot::OwnerPicks;

TEST_CASE("OwnerPicks: one pick per slot", "[ownerpicks]")
{
    CHECK((ParseOrder("prefer blessing might") == Order{ false, Blessing, 19740 }));
    CHECK((ParseOrder("Prefer Blessing  Kings") == Order{ false, Blessing, 20217 }));
    CHECK((ParseOrder("prefer bless salv") == Order{ false, Blessing, 1038 }));
    CHECK((ParseOrder("prefer aura fire") == Order{ false, Aura, 19891 }));
    CHECK((ParseOrder("prefer aura fire resistance") == Order{ false, Aura, 19891 }));
    CHECK((ParseOrder("prefer aura devotion") == Order{ false, Aura, 465 }));
    CHECK((ParseOrder("prefer aspect monkey") == Order{ false, Aspect, 13163 }));
    CHECK((ParseOrder("prefer demon voidwalker") == Order{ false, Demon, 697 }));
    CHECK((ParseOrder("prefer pet felhunter") == Order{ false, Demon, 691 }));
}
TEST_CASE("OwnerPicks: totems by element", "[ownerpicks]")
{
    CHECK((ParseOrder("prefer earth stoneskin") == Order{ false, TotemEarth, 8071 }));
    CHECK((ParseOrder("prefer totem earth strength of earth") == Order{ false, TotemEarth, 8075 }));
    CHECK((ParseOrder("prefer earth totem tremor") == Order{ false, TotemEarth, 8143 }));
    CHECK((ParseOrder("prefer earth tremor totem") == Order{ false, TotemEarth, 8143 }));
    CHECK((ParseOrder("prefer water fire") == Order{ false, TotemWater, 8184 }));    // Fire Resistance Totem is a water totem
    CHECK((ParseOrder("prefer fire frost") == Order{ false, TotemFire, 8181 }));
    CHECK((ParseOrder("prefer water mana spring") == Order{ false, TotemWater, 5675 }));
    CHECK((ParseOrder("prefer air windfury") == Order{ false, TotemAir, 8512 }));
    CHECK((ParseOrder("prefer air grace of air") == Order{ false, TotemAir, 8835 }));
}
TEST_CASE("OwnerPicks: back to the plan", "[ownerpicks]")
{
    CHECK((ParseOrder("prefer blessing auto") == Order{ false, Blessing, 0 }));
    CHECK((ParseOrder("prefer earth auto") == Order{ false, TotemEarth, 0 }));
    CHECK((ParseOrder("prefer clear") == Order{ true, Blessing, 0 }));
    CHECK((ParseOrder("prefer auto") == Order{ true, Blessing, 0 }));
}
TEST_CASE("OwnerPicks: other lines are not picks", "[ownerpicks]")
{
    CHECK_FALSE(ParseOrder("blessing might"));
    CHECK_FALSE(ParseOrder("prefer"));
    CHECK_FALSE(ParseOrder("prefer blessing"));
    CHECK_FALSE(ParseOrder("prefer blessing fire"));      // a word of another slot
    CHECK_FALSE(ParseOrder("prefer earth windfury"));
    CHECK_FALSE(ParseOrder("prefer totem aspect hawk"));
    CHECK_FALSE(ParseOrder("prefer aspect viper"));
    CHECK_FALSE(ParseOrder("preferred blessing might"));
    CHECK_FALSE(ParseOrder("prefer mightiness"));
}
TEST_CASE("OwnerPicks: the choice table", "[ownerpicks]")
{
    std::set<std::pair<int, uint32_t>> seen;
    for (Choice const& c : kChoices)
    {
        CHECK(c.spell != 0);
        CHECK(!c.words[0].empty());
        CHECK(seen.insert({ int(c.slot), c.spell }).second);
        CHECK(FindChoice(c.spell) == &c);   // every spell belongs to one slot
        // The add-on sends "prefer <slot word> <first word>": each one must parse back to this choice.
        static constexpr std::string_view kSlotWord[] = { "blessing", "aura", "earth", "fire", "water", "air", "aspect", "demon" };
        CHECK((ParseOrder("prefer " + std::string(kSlotWord[c.slot]) + " " + std::string(c.words[0])) == Order{ false, c.slot, c.spell }));
    }
    CHECK(SlotClass(Blessing) == 2);
    CHECK(SlotClass(TotemAir) == 7);
    CHECK(SlotClass(Aspect) == 3);
    CHECK(SlotClass(Demon) == 9);
    CHECK(TravelAspect(5118));
    CHECK_FALSE(TravelAspect(13165));
    CHECK((ParseSwitch(" OFF ") == std::optional<bool>(false)));
    CHECK((ParseSwitch("on") == std::optional<bool>(true)));
    CHECK_FALSE(ParseSwitch("attack"));
}
TEST_CASE("OwnerPicks: the add-on's danger spell lines are crowd-control orders", "[ownerpicks]")
{
    // ForeverBots.lua DANGER: "@<class> no <kind>", "@<class> off cc", "@<class> on cc", whispered to the bot.
    using Playerbot::CcDeny::ParseOrder;
    namespace K = Playerbot::CcDeny;
    CHECK((ParseOrder("@warrior no fear") == K::Order{ 1, true, K::Fear }));
    CHECK((ParseOrder("@paladin no fear") == K::Order{ 2, true, K::Fear }));
    CHECK((ParseOrder("@hunter no trap") == K::Order{ 3, true, K::Trap }));
    CHECK((ParseOrder("@hunter no sleep") == K::Order{ 3, true, K::Sleep }));
    CHECK((ParseOrder("@rogue no sap") == K::Order{ 4, true, K::Sap }));
    CHECK((ParseOrder("@priest no shackle") == K::Order{ 5, true, K::Shackle }));
    CHECK((ParseOrder("@mage no sheep") == K::Order{ 8, true, K::Polymorph }));
    CHECK((ParseOrder("@warlock no banish") == K::Order{ 9, true, K::Banish }));
    CHECK((ParseOrder("@warlock no seduce") == K::Order{ 9, true, K::Charm }));
    CHECK((ParseOrder("@druid no sleep") == K::Order{ 11, true, K::Sleep }));
    CHECK((ParseOrder("@druid off cc") == K::Order{ 11, true, K::All }));
    CHECK((ParseOrder("@druid on cc") == K::Order{ 11, false, K::All }));
    // and no "prefer" line is one
    CHECK_FALSE(ParseOrder("prefer blessing might").has_value());
}
