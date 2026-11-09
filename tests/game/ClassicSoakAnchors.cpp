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

#include "../../src/modules/PlayerbotV2/Fleet/SoakAnchors.h"

using namespace Playerbot::V2::Fleet;

TEST_CASE("The anchor list is split on commas, trimmed and lower-cased", "[ClassicSoakAnchors]")
{
    auto const s = ParseSoakAnchors(" Karrzone , Stormy;Orgy ,, ");
    REQUIRE(s.size() == 3);
    REQUIRE(s.contains("karrzone"));
    REQUIRE(s.contains("stormy"));
    REQUIRE(s.contains("orgy"));
}

TEST_CASE("An empty list names nobody", "[ClassicSoakAnchors]")
{
    REQUIRE(ParseSoakAnchors("").empty());
    REQUIRE(ParseSoakAnchors("  ,; ").empty());
}
