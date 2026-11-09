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

#include "../../src/modules/PlayerbotV2/Bot/SpecWords.h"

using namespace Playerbot::V2::SpecWords;

// Discord 2026-10-25: an owner respecs a summoned alt by whispering a spec name, not a ChrSpecialization id.
TEST_CASE("A spec name is the class's spec of that name", "[ClassicSpecWords]")
{
    CHECK(Parse(CLASS_PRIEST, "holy").spec == 257);
    CHECK(Parse(CLASS_PALADIN, "holy").spec == 65);
    CHECK(Parse(CLASS_WARRIOR, "arms").spec == 71);
    CHECK(Parse(CLASS_WARRIOR, "prot").spec == 73);
    CHECK(Parse(CLASS_PALADIN, "Prot").spec == 66);
    CHECK(Parse(CLASS_MAGE, "fire").spec == 63);
    CHECK(Parse(CLASS_MAGE, "FROST").spec == 64);
    CHECK(Parse(CLASS_MAGE, "arcane").spec == 62);
    CHECK(Parse(CLASS_HUNTER, "beast mastery").spec == 253);
    CHECK(Parse(CLASS_DRUID, "bear").spec == 104);
    CHECK(Parse(CLASS_MAGE, "fire").kind == Kind::Spec);
}

TEST_CASE("A spec name of another class is not taken", "[ClassicSpecWords]")
{
    CHECK(Parse(CLASS_MAGE, "holy").kind == Kind::None);
    CHECK(Parse(CLASS_WARRIOR, "fire").kind == Kind::None);
    CHECK(Parse(CLASS_ROGUE, "prot").kind == Kind::None);
}

TEST_CASE("A role word asks for the class's spec of that role", "[ClassicSpecWords]")
{
    CHECK(Parse(CLASS_WARRIOR, "tank").kind == Kind::Tank);
    CHECK(Parse(CLASS_PRIEST, "heal").kind == Kind::Healer);
    CHECK(Parse(CLASS_PRIEST, "healer").kind == Kind::Healer);
    CHECK(Parse(CLASS_MAGE, "dps").kind == Kind::Dps);
}

TEST_CASE("A spec id passes through, nonsense and nothing are refused", "[ClassicSpecWords]")
{
    CHECK(Parse(CLASS_PRIEST, "257").kind == Kind::Spec);
    CHECK(Parse(CLASS_PRIEST, "257").spec == 257);
    CHECK(Parse(CLASS_PRIEST, "").kind == Kind::None);
    CHECK(Parse(CLASS_PRIEST, "  ").kind == Kind::None);
    CHECK(Parse(CLASS_PRIEST, "wizard").kind == Kind::None);
    CHECK(Parse(CLASS_PRIEST, "12345678901").kind == Kind::None);
}

// 253 (Karrz 2026-10-25): "Setspec: playing as spec 72"; the 70532 data has no spec names.
TEST_CASE("A Classic spec answers to its own name", "[ClassicSpecWords]")
{
    CHECK(std::string(Label(72)) == "Fury");
    CHECK(std::string(Label(257)) == "Holy");
    CHECK(std::string(Label(66)) == "Protection");
    CHECK(std::string(Label(104)) == "Guardian");
    CHECK(Label(0) == nullptr);
    CHECK(Label(250) == nullptr);
    for (std::uint32_t spec : { 71u, 72u, 73u, 65u, 66u, 70u, 253u, 254u, 255u, 259u, 260u, 261u, 256u, 257u, 258u, 262u, 263u,
                                264u, 62u, 63u, 64u, 265u, 266u, 267u, 102u, 103u, 104u, 105u })
        CHECK(Label(spec) != nullptr);
}
