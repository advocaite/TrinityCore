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

#include "../../src/server/game/Entities/Unit/ClassicArmor.h"

TEST_CASE("A level 60 attacker against 3731 armor is mitigated by the vanilla 40%", "[ClassicArmor]")
{
    // 3731 / (3731 + 400 + 85 * 60) = 3731 / 9231
    CHECK(ClassicArmor::Constant(60) == 5500.0f);
    CHECK(ClassicArmor::Mitigation(3731.0f, 60) == Catch::Approx(0.4042f).margin(0.0005f));
}

TEST_CASE("Five Sunder Armors take a level 60 boss from about 40% to about 21%", "[ClassicArmor]")
{
    CHECK(ClassicArmor::Mitigation(3731.0f - 5 * 450.0f, 60) == Catch::Approx(0.2121f).margin(0.0005f));
}

TEST_CASE("Mitigation is zero without armor and never above 75%", "[ClassicArmor]")
{
    CHECK(ClassicArmor::Mitigation(0.0f, 60) == 0.0f);
    CHECK(ClassicArmor::Mitigation(-50.0f, 60) == 0.0f);
    CHECK(ClassicArmor::Mitigation(1000000.0f, 60) == 0.75f);
}

TEST_CASE("A lower level attacker is mitigated more by the same armor", "[ClassicArmor]")
{
    CHECK(ClassicArmor::Mitigation(2000.0f, 30) > ClassicArmor::Mitigation(2000.0f, 60));
}
