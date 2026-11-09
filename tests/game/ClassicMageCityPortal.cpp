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

#include "../../src/modules/PlayerbotV2/Bot/MageCityPortal.h"

#include <algorithm>
#include <vector>

using namespace Playerbot::V2::MageCity;

TEST_CASE("MageCityPortal: the city a request names", "[magecity]")
{
    REQUIRE(FindCity("ironforge"));
    CHECK(FindCity("ironforge")->portal == 11416);
    CHECK(FindCity("if")->teleport == 3562);
    CHECK(FindCity("storm")->portal == 10059);                 // a part of the English name
    CHECK(FindCity("thunder bluff")->portal == 11420);
    CHECK(FindCity("thunder")->teleport == 3566);
    CHECK(FindCity("uc")->portal == 11418);
    CHECK(FindCity("org")->portal == 11417);
    CHECK(FindCity("darnassus")->portal == 11419);
    // The Russian names: "stalgorn", "Stalgorn", "gromovoy utyos" (with e and with yo)
    CHECK(FindCity("\xd1\x81\xd1\x82\xd0\xb0\xd0\xbb\xd1\x8c\xd0\xb3\xd0\xbe\xd1\x80\xd0\xbd")->portal == 11416);
    CHECK(FindCity("\xd0\xa1\xd1\x82\xd0\xb0\xd0\xbb\xd1\x8c\xd0\xb3\xd0\xbe\xd1\x80\xd0\xbd")->portal == 11416);
    CHECK(FindCity("\xd0\xb3\xd1\x80\xd0\xbe\xd0\xbc\xd0\xbe\xd0\xb2\xd0\xbe\xd0\xb9 \xd1\x83\xd1\x82\xd0\xb5\xd1\x81")->portal == 11420);
    CHECK(FindCity("\xd0\xb3\xd1\x80\xd0\xbe\xd0\xbc\xd0\xbe\xd0\xb2\xd0\xbe\xd0\xb9 \xd1\x83\xd1\x82\xd1\x91\xd1\x81")->portal == 11420);
    CHECK(FindCity("karazhan") == nullptr);                     // not a capital: the command falls back to the spell names
    CHECK(FindCity("st") == nullptr);                           // two letters must be a short form
    CHECK(FindCity("") == nullptr);
}

TEST_CASE("MageCityPortal: the spell the mage knows", "[magecity]")
{
    // A level 40 Alliance mage: the Alliance teleports and portals, whatever language their names are in.
    std::vector<std::uint32_t> const known = { 3561, 3562, 3565, 10059, 11416, 11419 };
    auto const knows = [&known](std::uint32_t id) { return std::find(known.begin(), known.end(), id) != known.end(); };
    CHECK(CitySpell("ironforge", true, knows) == 11416);
    CHECK(CitySpell("ironforge", false, knows) == 3562);
    CHECK(CitySpell("stormwind", true, knows) == 10059);
    CHECK(CitySpell("orgrimmar", true, knows) == 0);            // a Horde city: not known
    CHECK(CitySpell("karazhan", true, knows) == 0);

    // Level 20-39: the teleports only.
    std::vector<std::uint32_t> const young = { 3561, 3562, 3565 };
    auto const knowsYoung = [&young](std::uint32_t id) { return std::find(young.begin(), young.end(), id) != young.end(); };
    CHECK(CitySpell("ironforge", true, knowsYoung) == 0);
    CHECK(CitySpell("ironforge", false, knowsYoung) == 3562);
}
