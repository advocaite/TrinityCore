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

#include "../../src/modules/PlayerbotV2/Bot/RezRule.h"

using namespace Playerbot::V2;

TEST_CASE("RezRule: only a druid or death knight raises in combat", "[ClassicRezRule]")
{
    CHECK(RezIsCombatRez(11));
    CHECK(RezIsCombatRez(6));
    CHECK_FALSE(RezIsCombatRez(5));   // priest
    CHECK_FALSE(RezIsCombatRez(2));   // paladin
    CHECK_FALSE(RezIsCombatRez(7));   // shaman
}

TEST_CASE("RezRule: the fight is live with a tank or three fighters", "[ClassicRezRule]")
{
    CHECK_FALSE(RezFightLive(0, 0));
    CHECK_FALSE(RezFightLive(0, 2));
    CHECK(RezFightLive(1, 1));
    CHECK(RezFightLive(0, 3));
}

TEST_CASE("RezRule: exactly one rezzer takes the first corpse at any time", "[ClassicRezRule]")
{
    for (std::uint32_t t : {0u, 3000u, 6000u, 9000u, 12000u})
        for (int n = 1; n <= 6; ++n)
        {
            int firsts = 0;
            for (int idx = 0; idx < n; ++idx)
                if (RezRank(idx, n, t) == 0) ++firsts;
            CHECK(firsts == 1);
        }
    CHECK(RezRank(1, 3, 3000) == 0);   // the front turns, so a stuck front rezzer holds the others for one slot only
}
