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

#include "../../src/modules/PlayerbotV2/Fleet/HirePolicy.h"

using namespace Playerbot::V2::Fleet::Hire;

// Karrz 2026-10-29: a complete legendary 40-man costs 100g, a legendary 5-man 5g, a blue (Veteran) 5-man 3g.
TEST_CASE("Package prices add up to what the server owner set", "[ClassicHirePolicy]")
{
    Prices const p;
    CHECK(PriceFor(p, Grade::Legend, 60, 60, true) * kRaidPlaces <= 1000000);
    CHECK(PriceFor(p, Grade::Legend, 60, 60, true) * kRaidPlaces > 1000000 - kRaidPlaces);
    CHECK(PriceFor(p, Grade::Legend, 60, 60, false) * kPartyPlaces == 50000);
    CHECK(PriceFor(p, Grade::Veteran, 60, 60, false) * kPartyPlaces == 30000);
    CHECK(PriceFor(p, Grade::Sellsword, 60, 60, false) * kPartyPlaces == 10000);
}

TEST_CASE("A Sellsword below the cap costs less, never under a silver", "[ClassicHirePolicy]")
{
    Prices const p;
    uint64_t const at60 = PriceFor(p, Grade::Sellsword, 60, 60, false);
    uint64_t const at40 = PriceFor(p, Grade::Sellsword, 40, 60, false);
    uint64_t const at20 = PriceFor(p, Grade::Sellsword, 20, 60, false);
    CHECK(at40 < at60);
    CHECK(at20 < at40);
    CHECK(at20 == 277);     // 2500 x 400 / 3600
    CHECK(PriceFor(p, Grade::Sellsword, 1, 60, false) == kMinPriceCopper);
}

TEST_CASE("Veterans and Legends are for the level cap only", "[ClassicHirePolicy]")
{
    Prices const p;
    CHECK(PriceFor(p, Grade::Veteran, 59, 60, false) == 0);
    CHECK(PriceFor(p, Grade::Legend, 15, 60, true) == 0);
    CHECK(PriceFor(p, Grade::None, 60, 60, false) == 0);
    CHECK(GradeForLevel(Grade::Sellsword, 1, 60));
}

TEST_CASE("Brokers sell their own grades", "[ClassicHirePolicy]")
{
    CHECK(BrokerOfEntry(6110000) == Broker::Inn);
    CHECK(BrokerOfEntry(6110105) == Broker::Capital);
    CHECK(BrokerOfEntry(6110202) == Broker::BlackMarket);
    CHECK(BrokerOfEntry(6110300) == Broker::None);
    CHECK(BrokerOfEntry(295) == Broker::None);
    CHECK(BrokerSells(Broker::Inn, Grade::Sellsword));
    CHECK_FALSE(BrokerSells(Broker::Inn, Grade::Veteran));
    CHECK(BrokerSells(Broker::Capital, Grade::Veteran));
    CHECK_FALSE(BrokerSells(Broker::Capital, Grade::Legend));
    CHECK(BrokerSells(Broker::BlackMarket, Grade::Legend));
    CHECK_FALSE(BrokerSells(Broker::BlackMarket, Grade::Sellsword));
}

TEST_CASE("Upkeep is a quarter of the price, only on a day the bot is called", "[ClassicHirePolicy]")
{
    Prices const p;
    CHECK(UpkeepFor(p, 25000) == 6250);
    CHECK(UpkeepFor(p, 0) == 0);                       // owned before hired mode: free
    CHECK(UpkeepFor(p, 200) == kMinPriceCopper);
    Prices off;
    off.upkeep_pct = 0;
    CHECK(UpkeepFor(off, 25000) == 0);
    CHECK_FALSE(UpkeepDue(0, 1000));
    CHECK_FALSE(UpkeepDue(2000, 1999));
    CHECK(UpkeepDue(2000, 2000));
}

TEST_CASE("Who must hire", "[ClassicHirePolicy]")
{
    CHECK_FALSE(Required(Mode::Off, true, false, true));
    CHECK(Required(Mode::Everyone, false, false, true));
    CHECK_FALSE(Required(Mode::Everyone, false, true, true));   // a GM testing
    CHECK(Required(Mode::Everyone, false, true, false));
    CHECK(Required(Mode::PlayerChoice, true, false, true));
    CHECK_FALSE(Required(Mode::PlayerChoice, false, false, true));
}

TEST_CASE("The grade sets the gear", "[ClassicHirePolicy]")
{
    CHECK(GearModeFor(Grade::Legend, true) == 5);
    CHECK(GearModeFor(Grade::Veteran, false) == 1);
    CHECK(GearModeFor(Grade::Sellsword, false) == 0);
    CHECK(GearModeFor(Grade::None, true) == 0);
    CHECK(GearModeFor(Grade::None, false) == -1);
}

TEST_CASE("Default rules are the built-in ones", "[ClassicHirePolicy]")
{
    Rules const r;
    for (Broker b : { Broker::Inn, Broker::Capital, Broker::BlackMarket, Broker::None })
        for (Grade g : { Grade::None, Grade::Sellsword, Grade::Veteran, Grade::Legend })
            CHECK(BrokerSells(r, b, g) == BrokerSells(b, g));
    for (Grade g : { Grade::None, Grade::Sellsword, Grade::Veteran, Grade::Legend })
        for (bool must : { false, true })
            CHECK(GearModeFor(r, g, must) == GearModeFor(g, must));
    CHECK(HealthPctFor(r, Grade::Legend) == 100);
    CHECK(HealthBonusFor(4000, 100) == 0);
}

TEST_CASE("The server owner sets what brokers sell, gear and health", "[ClassicHirePolicy]")
{
    CHECK(ParseGradeList("sellsword, veteran") == ((1u << 1) | (1u << 2)));
    CHECK(ParseGradeList("Legend") == (1u << 3));
    CHECK(ParseGradeList("legends sellswords") == ((1u << 1) | (1u << 3)));
    CHECK(ParseGradeList("none") == 0);
    CHECK(ParseGradeList("") == 0);
    CHECK(ParseGradeList("dragon") == 0);

    Rules r;
    r.sells[uint8_t(Broker::Inn)] = ParseGradeList("sellsword, legend");
    CHECK(BrokerSells(r, Broker::Inn, Grade::Legend));
    CHECK_FALSE(BrokerSells(r, Broker::Inn, Grade::Veteran));
    CHECK_FALSE(BrokerSells(r, Broker::None, Grade::Legend));

    r.gear_mode[uint8_t(Grade::Veteran)] = 3;   // t2
    CHECK(GearModeFor(r, Grade::Veteran, false) == 3);
    CHECK(GearModeFor(r, Grade::None, false) == -1);
    CHECK(GearModeFor(r, Grade::None, true) == 0);

    r.health_pct[uint8_t(Grade::Legend)] = 150;
    CHECK(HealthPctFor(r, Grade::Legend) == 150);
    CHECK(HealthBonusFor(4000, 150) == 2000);
    CHECK(HealthBonusFor(4000, 50) == -2000);
    CHECK(HealthBonusFor(4000, 1) == -3000);     // never under kMinHealthPct (25%)
    r.health_pct[uint8_t(Grade::Legend)] = 99999;
    CHECK(HealthPctFor(r, Grade::Legend) == kMaxHealthPct);
    CHECK(HealthPctFor(r, Grade::None) == 100);
}

TEST_CASE("Money reads like the game's", "[ClassicHirePolicy]")
{
    CHECK(MoneyText(12500) == "1g 25s");
    CHECK(MoneyText(7500) == "75s");
    CHECK(MoneyText(1250) == "12s 50c");
    CHECK(MoneyText(0) == "0c");
}
