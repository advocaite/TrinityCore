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

#include "../../src/server/game/Spells/ClassicSpellRank.h"

#include <map>
#include <set>

namespace
{
    // Immolate: ranks 348 -> 707 -> 1094 -> 2941 -> 11665 -> 11667 -> 11668 -> 25309
    std::map<uint32_t, uint32_t> const ImmolateChain =
    {
        { 348, 707 }, { 707, 1094 }, { 1094, 2941 }, { 2941, 11665 }, { 11665, 11667 }, { 11667, 11668 }, { 11668, 25309 }
    };

    uint32_t Next(uint32_t id)
    {
        auto itr = ImmolateChain.find(id);
        return itr != ImmolateChain.end() ? itr->second : 0;
    }

    uint32_t ResolveFor(std::set<uint32_t> const& active, uint32_t spellId, bool known)
    {
        return ClassicSpellRank::Resolve(spellId, known && !active.count(spellId), Next, [&](uint32_t id) { return active.count(id) != 0; });
    }
}

TEST_CASE("A stale rank 1 button casts the top rank the character knows", "[ClassicSpellRank]")
{
    std::set<uint32_t> const active = { 25309 };            // lower ranks are known but superseded
    CHECK(ResolveFor(active, 348, true) == 25309);          // Karrz: Immolate 348 on the bar, 25309 learned
}

TEST_CASE("The active rank itself is cast as it is", "[ClassicSpellRank]")
{
    std::set<uint32_t> const active = { 25309 };
    CHECK(ResolveFor(active, 25309, true) == 25309);
}

TEST_CASE("A rank that is still the active one is not upgraded", "[ClassicSpellRank]")
{
    std::set<uint32_t> const active = { 11665 };            // level 40: the best known is rank 5
    CHECK(ResolveFor(active, 11665, true) == 11665);
    CHECK(ResolveFor(active, 348, true) == 11665);
}

TEST_CASE("A spell the character does not know is never rewritten", "[ClassicSpellRank]")
{
    std::set<uint32_t> const active = { 25309 };
    CHECK(ResolveFor(active, 348, false) == 348);
}

TEST_CASE("A spell without ranks is unchanged", "[ClassicSpellRank]")
{
    std::set<uint32_t> const active = { 5782 };
    CHECK(ClassicSpellRank::Resolve(5782, true, [](uint32_t) { return 0u; }, [&](uint32_t id) { return active.count(id) != 0; }) == 5782);
}

TEST_CASE("A chain that loops back cannot hang the cast", "[ClassicSpellRank]")
{
    auto loop = [](uint32_t id) { return id == 1 ? 2u : 1u; };
    CHECK(ClassicSpellRank::Resolve(1, true, loop, [](uint32_t) { return false; }) == 1);
}
