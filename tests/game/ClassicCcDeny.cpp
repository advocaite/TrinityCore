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

#include "../../src/modules/PlayerbotV2/Bot/CcDeny.h"

#include <ostream>

namespace Playerbot::CcDeny {
std::ostream& operator<<(std::ostream& os, Order const& o)
{
    return os << "class " << int(o.cls) << (o.deny ? " deny " : " allow ") << Describe(o.kinds);
}
}

using namespace Playerbot::CcDeny;

TEST_CASE("CcDeny: class orders", "[ccdeny]")
{
    CHECK((ParseOrder("@warlock no fear") == Order{ 9, true, Fear }));
    CHECK((ParseOrder("@Warlocks No Fear") == Order{ 9, true, Fear }));
    CHECK((ParseOrder("@mages no sheep") == Order{ 8, true, Polymorph }));
    CHECK((ParseOrder("mage: yes sheep") == Order{ 8, false, Polymorph }));
    CHECK((ParseOrder("@lock off cc") == Order{ 9, true, All }));
    CHECK((ParseOrder("@hunter no freezing trap") == Order{ 3, true, Trap }));
}
TEST_CASE("CcDeny: orders for every class", "[ccdeny]")
{
    CHECK((ParseOrder("no fear") == Order{ 0, true, Fear }));
    CHECK((ParseOrder("yes fear") == Order{ 0, false, Fear }));
    CHECK((ParseOrder("off cc") == Order{ 0, true, All }));
    CHECK((ParseOrder("on cc") == Order{ 0, false, All }));
    CHECK((ParseOrder("cc off") == Order{ 0, true, All }));
    CHECK((ParseOrder("  Sheep  On ") == Order{ 0, false, Polymorph }));
}
TEST_CASE("CcDeny: other orders and chatter are not crowd-control orders", "[ccdeny]")
{
    CHECK_FALSE(ParseOrder("fear"));
    CHECK_FALSE(ParseOrder("no fear please"));
    CHECK_FALSE(ParseOrder("crowdcontrol off"));   // the panel's marked-CC switch keeps its own path
    CHECK_FALSE(ParseOrder("crowd control on"));
    CHECK_FALSE(ParseOrder("@skull no fear"));
    CHECK_FALSE(ParseOrder("aoe off"));
    CHECK_FALSE(ParseOrder("use altar"));
    CHECK_FALSE(ParseOrder("stay on"));
    CHECK_FALSE(ParseOrder("Raveni: no fear"));
}
TEST_CASE("CcDeny: spells and names", "[ccdeny]")
{
    CHECK(KindOfSpell(5782) == Fear);
    CHECK(KindOfSpell(5484) == Fear);
    CHECK(KindOfSpell(118) == Polymorph);
    CHECK(KindOfSpell(710) == Banish);
    CHECK(KindOfSpell(1499) == Trap);
    CHECK(KindOfSpell(686) == 0);   // Shadow Bolt
    CHECK(Describe(Fear | Polymorph) == "fear, polymorph");
    CHECK(Describe(All) == "all crowd control");
    CHECK(Describe(0) == "nothing");
}
