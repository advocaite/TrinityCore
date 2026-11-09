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

#include "../../src/modules/PlayerbotV2/Bot/Gear/BisPools.cpp"
#include <set>
#include <string>

using namespace Playerbot::Gear;

TEST_CASE("Every role has a pool; entries are named, tiered 1 to 3 and not repeated", "[ClassicBisPools]")
{
    for (Loadout::Role r : { Loadout::Role::Tank, Loadout::Role::Healer, Loadout::Role::Caster, Loadout::Role::Melee, Loadout::Role::Hunter })
    {
        auto const pool = Bis::PoolFor(r);
        REQUIRE_FALSE(pool.empty());
        std::set<std::string> seen;
        for (Bis::PoolEntry const& e : pool)
        {
            CHECK_FALSE(e.name.empty());
            CHECK(e.tier >= 1);
            CHECK(e.tier <= 3);
            CHECK(seen.insert(std::string(e.name)).second);
        }
    }
}

TEST_CASE("A tier sees its own entries and the earlier ones, not later ones", "[ClassicBisPools]")
{
    int t1 = 0, t3 = 0;
    for (Bis::PoolEntry const& e : Bis::PoolFor(Loadout::Role::Melee))
    {
        t1 += Bis::InTier(e, 1);
        t3 += Bis::InTier(e, 3);
    }
    CHECK(t1 > 0);
    CHECK(t3 > t1);
}
