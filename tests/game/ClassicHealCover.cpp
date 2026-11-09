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

#include "../../src/modules/PlayerbotV2/Bot/ClassicHealCover.h"
#include "../../src/modules/PlayerbotV2/Combat/Apl/HealClaimStore.h"

using namespace Playerbot::Combat::ClassicHeal;

// MC Garr 2026-10-10, patient 2605: 64 % health, 1406 missing, 1692 of swings coming, 2280 of heals in flight. Five healers piled on.
TEST_CASE("Heals in flight that exceed the missing health plus half the swings refuse another heal", "[ClassicHealCover]")
{
    int64_t const max_hp = 3906, hp = max_hp - 1406;   // 1406 missing, 64 %
    CHECK(HealDeficitOf(max_hp, hp, 1692) == 1406 + 846);
    CHECK(CoveredByHealsInFlight(max_hp, hp, 1692, 2280, 64, 1.0f));
}

TEST_CASE("Heals in flight below the deficit leave room for another heal", "[ClassicHealCover]")
{
    int64_t const max_hp = 3906, hp = max_hp - 1406;
    CHECK_FALSE(CoveredByHealsInFlight(max_hp, hp, 1692, 1000, 64, 1.0f));
    CHECK_FALSE(CoveredByHealsInFlight(max_hp, hp, 1692, 0, 64, 1.0f));
}

TEST_CASE("A critical patient asks for half as much again in flight", "[ClassicHealCover]")
{
    int64_t const max_hp = 4000, hp = 800;   // 20 %, 3200 missing
    CHECK_FALSE(CoveredByHealsInFlight(max_hp, hp, 0, 3300, 20, 1.0f));   // covers the deficit, not 1.5 x it
    CHECK(CoveredByHealsInFlight(max_hp, hp, 0, 4800, 20, 1.0f));
}

TEST_CASE("The tank's healers ask for a margin", "[ClassicHealCover]")
{
    int64_t const max_hp = 4000, hp = 2400;   // 1600 missing
    CHECK(CoveredByHealsInFlight(max_hp, hp, 0, 1700, 60, 1.0f));
    CHECK_FALSE(CoveredByHealsInFlight(max_hp, hp, 0, 1700, 60, kHealTankTroubleCover));
}

// Playtest live MC 2026-10-10: a Life Tapping warlock drew heals and HoTs from every healer.
TEST_CASE("A warlock's own Life Tap is not healed until he is low", "[ClassicHealCover]")
{
    CHECK(LifeTapSkip(true, false, 80, 0));
    CHECK(LifeTapSkip(true, false, 60, 0));
    CHECK_FALSE(LifeTapSkip(true, false, 59, 0));    // low enough: heal him
    CHECK_FALSE(LifeTapSkip(true, false, 80, 900));  // something is hitting him
    CHECK_FALSE(LifeTapSkip(false, false, 80, 0));   // not a warlock
    CHECK_FALSE(LifeTapSkip(true, true, 80, 0));     // a tank warlock
}

TEST_CASE("A rezzed patient after the fight is covered by exact heals, not 1.5 times the deficit", "[ClassicHealCover]")
{
    using namespace Playerbot::Combat::ClassicHeal;
    // MC Magmadar 2026-10-10: a warlock rezzed at 14 % after the fight, max 3100, 3984 healing in flight: a third healer started a heal.
    CHECK(CoveredByHealsInFlight(3100, 434, 0, 3984, 14, 1.0f, /*in_fight*/ false));
    CHECK_FALSE(CoveredByHealsInFlight(3100, 434, 0, 3984, 14, 1.0f, /*in_fight*/ true));   // in a fight the critical margin stays
    CHECK_FALSE(CoveredByHealsInFlight(3100, 434, 0, 2000, 14, 1.0f, false));               // still not covered
}

TEST_CASE("Raid HoTs only go on members who are really hurt and not already covered", "[ClassicHealCover]")
{
    using namespace Playerbot::Combat::ClassicHeal;
    CHECK(HotWorthCasting(true, 60, 1200, 0));
    CHECK_FALSE(HotWorthCasting(true, 80, 600, 0));       // 80 %: a Renew would mostly overheal
    CHECK_FALSE(HotWorthCasting(true, 60, 1200, 1200));   // a direct heal already covers it
    CHECK(HotWorthCasting(true, 70, 900, 400));
    CHECK(HotWorthCasting(false, 85, 300, 0));            // a party keeps the old line
    CHECK_FALSE(HotWorthCasting(false, 90, 300, 0));
}

TEST_CASE("Power Word: Shield is claimed for Weakened Soul's 15 seconds", "[ClassicHealCover]")
{
    using namespace Playerbot::Combat::ClassicHeal;
    CHECK_FALSE(ShieldClaimFresh(0, 5000));
    CHECK(ShieldClaimFresh(1000, 1200));
    CHECK(ShieldClaimFresh(1000, 15999));
    CHECK_FALSE(ShieldClaimFresh(1000, 16000));
}

// MC Magmadar 2026-10-10: feared healers' refused heals left claims, and the claim amounts shrank what the others saw in flight.
TEST_CASE("A refused cast releases this tick's claim on the same patient and no other", "[ClassicHealCover]")
{
    CHECK(ClaimReleasable(false, true, 10000, 10300));      // same patient, 300 ms old
    CHECK(ClaimReleasable(true, false, 10000, 10300));      // the cast named no patient
    CHECK_FALSE(ClaimReleasable(false, false, 10000, 10300));   // a claim on someone else stays
    CHECK_FALSE(ClaimReleasable(false, true, 10000, 12600));    // a heal still running (2.6 s old) stays
}

// The claim book is one per process: a claim written through one WithHealClaims call site must be visible to every other.
TEST_CASE("A heal claim written through one call site is read through another and released", "[ClassicHealCover]")
{
    uint32 const now = 1000;
    WithHealClaims(now, [&](std::unordered_map<uint64, HealClaim>& claims)
    {
        HealClaim& c = claims[42];
        c.patient  = ObjectGuid::Create<HighGuid::Player>(7);
        c.spell    = 635;
        c.amount   = 500.0f;
        c.start_ms = now;
        c.land_ms  = now + 2500;
        return 0;
    });

    size_t const seen = WithHealClaims(now, [](std::unordered_map<uint64, HealClaim> const& c) { return c.count(42); });
    CHECK(seen == 1);

    ReleaseHealClaim(42, ObjectGuid::Create<HighGuid::Player>(7), now);
    size_t const left = WithHealClaims(now, [](std::unordered_map<uint64, HealClaim> const& c) { return c.count(42); });
    CHECK(left == 0);
}
