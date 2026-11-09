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

#include "../../src/modules/PlayerbotV2/Bot/InstanceSchools.h"

using namespace Playerbot::Classic;

namespace
{
constexpr std::uint32_t kMoltenCore = 409, kNaxxramas = 533, kBlackwingLair = 469, kAhnQiraj = 531, kDeadmines = 36, kOpenWorld = 0;
std::span<InstanceSchool const> const kTable = kInstanceSchools;
}

// MC 2026-10-11: five priests spent a refresh round of 40 Shadow Protection casts in Molten Core, where the raid takes fire.
TEST_CASE("Shadow Protection is wanted only where the bosses deal shadow damage", "[ClassicInstanceSchools]")
{
    CHECK_FALSE(InstanceDealsSchool(kTable, kMoltenCore, Resist::Shadow));
    CHECK_FALSE(InstanceDealsSchool(kTable, kAhnQiraj, Resist::Shadow));
    CHECK_FALSE(InstanceDealsSchool(kTable, kDeadmines, Resist::Shadow));
    CHECK_FALSE(InstanceDealsSchool(kTable, kOpenWorld, Resist::Shadow));
    CHECK(InstanceDealsSchool(kTable, kNaxxramas, Resist::Shadow));
    CHECK(InstanceDealsSchool(kTable, kBlackwingLair, Resist::Shadow));   // Shadow Flame (Ebonroc, Flamegor, Nefarian)
}

TEST_CASE("An instance deals every school listed for it, and a buff that is no resistance is always wanted", "[ClassicInstanceSchools]")
{
    CHECK(InstanceDealsSchool(kTable, kMoltenCore, Resist::Fire));
    CHECK(InstanceDealsSchool(kTable, kNaxxramas, Resist::Frost));
    CHECK_FALSE(InstanceDealsSchool(kTable, kNaxxramas, Resist::Fire));
    CHECK(InstanceDealsSchool(kTable, kDeadmines, Resist::None));
    CHECK(InstanceDealsSchool(kTable, kOpenWorld, Resist::None));
}

TEST_CASE("The main school of an instance is its first row", "[ClassicInstanceSchools]")
{
    CHECK(InstanceMainSchool(kTable, kMoltenCore) == Resist::Fire);
    CHECK(InstanceMainSchool(kTable, kNaxxramas) == Resist::Frost);   // not Shadow, listed second
    CHECK(InstanceMainSchool(kTable, kAhnQiraj) == Resist::Nature);
    CHECK(InstanceMainSchool(kTable, kBlackwingLair) == Resist::Fire);   // Shadow listed second
    CHECK(InstanceMainSchool(kTable, kDeadmines) == Resist::None);
}
