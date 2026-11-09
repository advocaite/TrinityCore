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

#include "../../src/modules/PlayerbotV2/Commands/TypedName.h"

using namespace Playerbot::V2::TypedName;

namespace {
// Kar's surname is "kar" (Karrz's alt, 2026-10-25); Bob has none; nobody else exists.
std::string Surname(std::string const& first)
{
    if (Folded(first) == "kar") return "kar";
    return {};
}
}

TEST_CASE("One word is the name as typed", "[ClassicTypedName]")
{
    CHECK(Pick("Kar", Surname) == "Kar");
    CHECK(Pick("  kar  ", Surname) == "kar");
    CHECK(Pick("", Surname).empty());
}

// The party frame shows "Kar kar": players type the name and the surname (Discord / Karrz 2026-10-24 and 25).
TEST_CASE("Name and surname is the name", "[ClassicTypedName]")
{
    CHECK(Pick("Kar kar", Surname) == "Kar");
    CHECK(Pick("kar KAR", Surname) == "kar");
    CHECK(Pick("Kar   kar ", Surname) == "Kar");
}

TEST_CASE("Words that are not a name and its surname are joined", "[ClassicTypedName]")
{
    CHECK(Pick("First Last", Surname) == "FirstLast");
    CHECK(Pick("Kar other", Surname) == "Karother");
    CHECK(Pick("Bob smith", Surname) == "Bobsmith");
}
