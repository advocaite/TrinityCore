// RaceStartZone: low-level bots stand in the start zone of their own race.

#include "tc_catch2.h"

#include "../../src/modules/PlayerbotV2/Bot/World/RaceStartZone.h"

using namespace Playerbot::V2::World;

TEST_CASE("A start zone takes only its own races at the start levels", "[ClassicRaceStartZone]")
{
    REQUIRE(RaceFitsStartZone(5, 4, 85));      // undead in Tirisfal
    REQUIRE_FALSE(RaceFitsStartZone(2, 4, 85));   // orc in Tirisfal
    REQUIRE_FALSE(RaceFitsStartZone(8, 4, 85));   // troll in Tirisfal
    REQUIRE_FALSE(RaceFitsStartZone(6, 4, 85));   // tauren in Tirisfal
    REQUIRE(RaceFitsStartZone(2, 4, 14));
    REQUIRE(RaceFitsStartZone(8, 4, 14));
    REQUIRE(RaceFitsStartZone(3, 4, 1));
    REQUIRE(RaceFitsStartZone(7, 4, 1));
    REQUIRE_FALSE(RaceFitsStartZone(1, 4, 1));    // human in Dun Morogh
}

TEST_CASE("Outside a start zone nobody is limited", "[ClassicRaceStartZone]")
{
    REQUIRE(RaceFitsStartZone(2, 4, 1519));       // Stormwind
    REQUIRE(RaceFitsStartZone(1, 60, 1519));
    REQUIRE(RaceFitsStartZone(5, 25, 130));       // Silverpine
    REQUIRE(RaceFitsStartZone(10, 4, 85));        // a race without a start zone here
}

TEST_CASE("A start zone takes nobody above the start levels", "[ClassicRaceStartZone]")
{
    REQUIRE(RaceFitsStartZone(5, kStartZoneMaxLevel, 85));
    REQUIRE_FALSE(RaceFitsStartZone(5, kStartZoneMaxLevel + 1, 85));   // even its own race
    REQUIRE_FALSE(RaceFitsStartZone(1, 25, 85));  // Jamini, a level 25 human priest, in Deathknell
    REQUIRE_FALSE(RaceFitsStartZone(1, 60, 12));
    REQUIRE_FALSE(RaceFitsStartZone(10, kStartZoneMaxLevel + 1, 85));
}

TEST_CASE("RaceForStartZone names a race that starts there", "[ClassicRaceStartZone]")
{
    for (std::uint32_t zone : { 12u, 14u, 1u, 141u, 85u, 215u })
        for (std::uint64_t seed = 0; seed < 4; ++seed)
        {
            std::uint8_t const race = RaceForStartZone(zone, seed);
            REQUIRE(race != 0);
            REQUIRE(StartZoneOfRace(race) == zone);
        }
    REQUIRE(RaceForStartZone(1519, 0) == 0);
}
