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

#include "../../src/server/game/Entities/Unit/ClassicRage.h"

using namespace ClassicRage;

TEST_CASE("Dual Wield Specialization adds its bonus on small off-hand hits", "[ClassicRageSwing]")
{
    // 5/5 Dual Wield Specialization (+50%): a 3 damage off-hand hit counts as 4.5, not 4
    CHECK(SwingRageDamage(false, 3, 3, 50.0f) == Catch::Approx(4.5f));
    // 1/5 (+10%): every hit under 10 damage used to get no bonus at all
    for (uint32_t dmg = 1; dmg < 10; ++dmg)
    {
        INFO("damage " << dmg);
        CHECK(SwingRageDamage(false, dmg, dmg, 10.0f) > float(dmg));
        CHECK(RageFromDamage(SwingRageDamage(false, dmg, dmg, 10.0f), 20.0f, true) > RageFromDamage(float(dmg), 20.0f, true));
    }
}

TEST_CASE("A swing without the talent counts its damage plus absorb", "[ClassicRageSwing]")
{
    CHECK(SwingRageDamage(false, 999, 120, 0.0f) == Catch::Approx(120.0f));
}

TEST_CASE("A dodged or parried swing counts 75% of its damage, fraction kept", "[ClassicRageSwing]")
{
    CHECK(SwingRageDamage(true, 10, 0, 0.0f) == Catch::Approx(7.5f));
    CHECK(SwingRageDamage(true, 1, 0, 0.0f) == Catch::Approx(0.75f));
    CHECK(SwingRageDamage(true, 10, 0, 20.0f) == Catch::Approx(9.0f));
}

TEST_CASE("Rage conversion is unchanged: damage / conversion * 7.5 dealt, * 2.5 taken", "[ClassicRageSwing]")
{
    float const c60 = 0.0091107836f * 3600.0f + 3.225598133f * 60.0f + 4.2652911f;
    CHECK(Conversion(60.0f) == Catch::Approx(c60));
    CHECK(RageFromDamage(c60, 60.0f, true) == Catch::Approx(7.5f));
    CHECK(RageFromDamage(c60, 60.0f, false) == Catch::Approx(2.5f));
}
