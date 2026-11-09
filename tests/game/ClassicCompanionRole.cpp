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

#include "../../src/modules/PlayerbotV2/Fleet/CompanionRole.h"

using namespace Playerbot::V2::Fleet;
using Playerbot::Role;

// Stormwind 2026-10-09: 285 stored companion roles, healer priests stored as dps. The login and upkeep read the talents before
// the setup step bought them, and the guess was stored for good.
TEST_CASE("A role the owner asked for is pinned and stored", "[ClassicCompanionRole]")
{
    CompanionRoleChoice const c = ChooseCompanionRole(Role::Healer, Role::Dps, Role::Dps, false);
    CHECK(c.role == Role::Healer);
    CHECK(c.source == CompanionRoleSource::Declared);
    CHECK(c.pin);
    CHECK(c.store);
}

TEST_CASE("A stored role is kept whatever the talents say", "[ClassicCompanionRole]")
{
    CompanionRoleChoice const c = ChooseCompanionRole(Role::Unknown, Role::Healer, Role::Dps, false);
    CHECK(c.role == Role::Healer);
    CHECK(c.source == CompanionRoleSource::Declared);
    CHECK(c.pin);
    CHECK(c.store);
}

TEST_CASE("A role read from the talents is never stored", "[ClassicCompanionRole]")
{
    // a priest whose points are not spent yet reads as dps: that guess must not be kept
    CompanionRoleChoice const dps = ChooseCompanionRole(Role::Unknown, Role::Unknown, Role::Dps, false);
    CHECK(dps.role == Role::Dps);
    CHECK(dps.source == CompanionRoleSource::Spec);
    CHECK_FALSE(dps.pin);
    CHECK_FALSE(dps.store);

    CompanionRoleChoice const tank = ChooseCompanionRole(Role::Unknown, Role::Unknown, Role::Tank, false);
    CHECK(tank.role == Role::Tank);          // a warrior with a tank build plays it, still not stored
    CHECK_FALSE(tank.store);

    CompanionRoleChoice const healer = ChooseCompanionRole(Role::Unknown, Role::Unknown, Role::Healer, false);
    CHECK(healer.role == Role::Healer);
    CHECK_FALSE(healer.store);
}

TEST_CASE("A hybrid that reads as tank heals for now and is not stored", "[ClassicCompanionRole]")
{
    CompanionRoleChoice const c = ChooseCompanionRole(Role::Unknown, Role::Unknown, Role::Tank, true);
    CHECK(c.role == Role::Healer);
    CHECK(c.source == CompanionRoleSource::SpecHybrid);
    CHECK(c.pin);            // the healer spec is pinned so the talent walk leaves the Protection build
    CHECK_FALSE(c.store);    // the raid quota fill may still make it a tank
}

TEST_CASE("No role and no reading is dps and not stored", "[ClassicCompanionRole]")
{
    CompanionRoleChoice const c = ChooseCompanionRole(Role::Unknown, Role::Unknown, Role::Unknown, false);
    CHECK(c.role == Role::Dps);
    CHECK_FALSE(c.store);
}

TEST_CASE("A stored spec is restored while it fits the stored role", "[ClassicCompanionRole]")
{
    // Retribution (saved dps) with a stored dps role: the class default (picked) gives way to the saved spec.
    CHECK(ChooseCompanionSpec(/*picked*/ 3, /*saved*/ 1, Role::Dps, Role::Dps, true) == 1);
    // A saved Holy (healer) spec for a paladin saved as a healer.
    CHECK(ChooseCompanionSpec(2, 2, Role::Healer, Role::Healer, true) == 2);
}

TEST_CASE("A stored spec of another role, or none, leaves the role's pick", "[ClassicCompanionRole]")
{
    // Saved Retribution, now asked to heal: the role rule's pick stands.
    CHECK(ChooseCompanionSpec(2, 1, Role::Dps, Role::Healer, true) == 2);
    // Nothing stored.
    CHECK(ChooseCompanionSpec(3, 0, Role::Dps, Role::Dps, true) == 3);
    // A role nobody chose (a guess from talents) never takes the stored spec.
    CHECK(ChooseCompanionSpec(3, 1, Role::Dps, Role::Dps, false) == 3);
}
