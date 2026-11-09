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

#include "../../src/modules/PlayerbotV2/Bot/ThreatLimits.h"

using namespace Playerbot;

// The raid brain and the generic hold read one limit: a caster between 1.10 and 1.20 times the tank's threat used to be released by the
// brain and held for good by the generic gate.
TEST_CASE("Melee never releases above the tank's threat and casters keep a tenth of margin", "[ClassicThreatLimits]")
{
    CHECK(ThreatHardRatio(true)  == 1.0f);
    CHECK(ThreatHardRatio(false) == 1.10f);
    CHECK(ThreatHardRatio(false) > ThreatHardRatio(true));
}

// Raid pulls: the tank takes a while to run in, so the DPS open eight seconds after its first hit on a boss (the owner's Attack still releases at once).
// The generic hold, the raid brain's hold and every boss fight script read this one number.
TEST_CASE("A raid's DPS wait eight seconds after the tank's first hit on a boss", "[ClassicThreatLimits]")
{
    CHECK(kRaidBossOpenWaitMs == 8000u);
    // The "forgotten Attack" release (Playerbot.Raid.DpsWaitTimeoutMs) is the one that actually opened the live Lucifron fight at 6 s; it was
    // limited to six seconds in the config reader. A raid may now be set up to eight, a party stays at six.
    CHECK(kRaidDpsWaitTimeoutMaxMs == 8000u);
    CHECK(kPartyDpsWaitTimeoutMs == 6000u);
    CHECK(kRaidDpsWaitTimeoutMaxMs > kPartyDpsWaitTimeoutMs);
}

// Past the hard limit the hold waits for the tank's threat to rise. A tank that is stunned, feared, dead or out of reach never raises it
// (audit 2026-10-16: DPS and healers idle for whole stuns and Infernos), so the wait ends after six seconds of the tank standing still;
// a tank that keeps building threat keeps the hold.
TEST_CASE("A hold past the hard limit releases after six seconds of a tank whose threat does not move", "[ClassicThreatLimits]")
{
    CHECK(kStalledTankHoldMs == 6000u);

    StalledTankClock stalled;
    CHECK_FALSE(StalledTankHoldExpired(stalled, 10000u, 5000.0f, 4000u));   // first poll starts the clock
    CHECK_FALSE(StalledTankHoldExpired(stalled, 13000u, 5000.0f, 4000u));   // 3 s, still waiting
    CHECK_FALSE(StalledTankHoldExpired(stalled, 15900u, 5000.0f, 4000u));   // just under
    CHECK(StalledTankHoldExpired(stalled, 16000u, 5000.0f, 4000u));         // 6 s at a standstill: released

    StalledTankClock building;
    CHECK_FALSE(StalledTankHoldExpired(building, 10000u, 5000.0f, 4000u));
    CHECK_FALSE(StalledTankHoldExpired(building, 14000u, 5600.0f, 4000u));  // the tank gained: the clock restarts here
    CHECK_FALSE(StalledTankHoldExpired(building, 16000u, 5600.0f, 4000u));  // 2 s since the gain
    CHECK_FALSE(StalledTankHoldExpired(building, 19900u, 5600.0f, 4000u));
    CHECK(StalledTankHoldExpired(building, 20000u, 5600.0f, 4000u));        // 6 s since the last gain

    StalledTankClock gap;
    CHECK_FALSE(StalledTankHoldExpired(gap, 10000u, 5000.0f, 4000u));
    CHECK_FALSE(StalledTankHoldExpired(gap, 15000u, 5000.0f, 4000u));       // 5 s unpolled: a new hold, the clock restarts
    CHECK_FALSE(StalledTankHoldExpired(gap, 20000u, 5000.0f, 4000u));       // another 5 s gap
    CHECK_FALSE(StalledTankHoldExpired(gap, 23000u, 5000.0f, 4000u));
    CHECK(StalledTankHoldExpired(gap, 26000u, 5000.0f, 4000u));             // 6 s of steady polls

    StalledTankClock wiped;
    CHECK_FALSE(StalledTankHoldExpired(wiped, 10000u, 40000.0f, 4000u));
    CHECK_FALSE(StalledTankHoldExpired(wiped, 11000u, 0.0f, 4000u));        // the threat is wiped while the hold goes on
    CHECK_FALSE(StalledTankHoldExpired(wiped, 13000u, 1500.0f, 4000u));     // the tank rebuilds: a rise over the last poll, the clock restarts
    CHECK_FALSE(StalledTankHoldExpired(wiped, 17000u, 3000.0f, 4000u));
    CHECK_FALSE(StalledTankHoldExpired(wiped, 21000u, 4500.0f, 4000u));     // still building, far under the old 40000: held
    CHECK_FALSE(StalledTankHoldExpired(wiped, 24000u, 4500.0f, 4000u));
    CHECK(StalledTankHoldExpired(wiped, 27000u, 4500.0f, 4000u));            // 6 s at 4500: a real standstill is still released
}
