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

#include "../../src/modules/PlayerbotV2/Fleet/BotSize.h"

using namespace Playerbot::V2::Fleet;

TEST_CASE("No Bots line keeps the old keys", "[ClassicBotSize]")
{
    PlainInput in;
    CHECK(!ResolvePlainBots(in));
    in.per_zone = "nonsense";
    CHECK(!ResolvePlainBots(in));
}

TEST_CASE("A size alone gives its preset", "[ClassicBotSize]")
{
    PlainInput in;
    in.size = ParseBotSize(" High ");
    auto b = ResolvePlainBots(in);
    REQUIRE(b.has_value());
    CHECK(b->reserve == 3000);
    CHECK(b->active_cap == 500);
    CHECK(b->zone_lo == 50);
    CHECK(b->zone_hi == 70);
    CHECK(b->in_city == 120);
    CHECK((ParseBotSize("low") == BotSize::Low));
    CHECK((ParseBotSize("Normal") == BotSize::Normal));
    CHECK(!ParseBotSize("huge"));
}

TEST_CASE("Own numbers override the size one by one", "[ClassicBotSize]")
{
    PlainInput in;
    in.size = BotSize::Low;
    in.active_cap = 300;
    in.per_zone = "70-50";
    auto b = ResolvePlainBots(in);
    REQUIRE(b.has_value());
    CHECK(b->active_cap == 300);
    CHECK(b->zone_lo == 50);
    CHECK(b->zone_hi == 70);
    CHECK(b->in_city == 50);       // Low's city number
    CHECK(b->reserve == 600);      // Low's reserve
    in.active_cap = 900;
    CHECK(ResolvePlainBots(in)->reserve == 900);   // never fewer characters than may be awake
}

TEST_CASE("Zone range parses and rolls inside it", "[ClassicBotSize]")
{
    uint32_t lo = 0, hi = 0;
    CHECK(ParseRange("50-70", lo, hi));
    CHECK((lo == 50 && hi == 70));
    CHECK(ParseRange(" 60 ", lo, hi));
    CHECK((lo == 60 && hi == 60));
    CHECK(!ParseRange("50-", lo, hi));
    CHECK(!ParseRange("abc", lo, hi));
    for (uint32_t d = 0; d < 1000; ++d)
    {
        uint32_t const r = ZoneRoll(50, 70, d * 7919u);
        CHECK((r >= 50 && r <= 70));
    }
    CHECK(ZoneRoll(60, 60, 12345) == 60);
}

TEST_CASE("The cap is shared evenly", "[ClassicBotSize]")
{
    // 10 zones of 60 with a cap of 500: each gets 50
    std::vector<PlaceWant> ten(10, PlaceWant{ 0, 60, 1 });
    for (uint32_t v : ShareCap(ten, 500))
        CHECK(v == 50);
    // under the cap nothing is cut
    std::vector<PlaceWant> two = { { 1, 60, 1 }, { 2, 120, 1 } };
    auto got = ShareCap(two, 500);
    CHECK(got[0] == 60);
    CHECK(got[1] == 120);
    // 100 players in 100 zones, cap 500: 5 each, never 0
    std::vector<PlaceWant> many(100, PlaceWant{ 0, 60, 1 });
    for (uint32_t v : ShareCap(many, 500))
        CHECK(v == 5);
    std::vector<PlaceWant> crowd(1000, PlaceWant{ 0, 60, 1 });
    for (uint32_t v : ShareCap(crowd, 500))
        CHECK(v == 1);
}

TEST_CASE("Players in one zone share its bots", "[ClassicBotSize]")
{
    CHECK(PerPlayer(60, 1) == 60);
    CHECK(PerPlayer(60, 2) == 30);
    CHECK(PerPlayer(61, 2) == 31);
    CHECK(PerPlayer(0, 3) == 0);
    CHECK(PerPlayer(2, 5) == 1);
}

TEST_CASE("Bots.Size = Off in the file means no world bots, not a size", "[ClassicBotSize]")
{
    CHECK(IsBotSizeOff("Off"));
    CHECK(IsBotSizeOff(" OFF "));
    CHECK(IsBotSizeOff("\"off\""));
    CHECK(IsBotSizeOff("none"));
    CHECK(IsBotSizeOff("0"));
    CHECK(!IsBotSizeOff("Low"));
    CHECK(!IsBotSizeOff(""));
    CHECK(!IsBotSizeOff("of"));
    CHECK(!ParseBotSize("Off"));   // Low / Normal / High stay the only sizes
    PlainInput in;
    in.size = ParseBotSize("Off");
    CHECK(!ResolvePlainBots(in));   // on its own it sets no numbers; the spawner itself stops for Off
}

TEST_CASE("The panel's world bot choice", "[ClassicBotSize]")
{
    CHECK((ParsePopLevel("Off") == PopLevel::Off));
    CHECK((ParsePopLevel(" 0 ") == PopLevel::Off));
    CHECK((ParsePopLevel("low") == PopLevel::Low));
    CHECK((ParsePopLevel("Medium") == PopLevel::Medium));
    CHECK((ParsePopLevel("normal") == PopLevel::Medium));
    CHECK((ParsePopLevel("HIGH") == PopLevel::High));
    CHECK(!ParsePopLevel("file"));
    CHECK(!ParsePopLevel(""));
    CHECK(std::string(PopLevelWord(PopLevel::Medium)) == "medium");

    PlainBots const off = PopLevelBots(PopLevel::Off, 1500);
    CHECK(off.active_cap == 0);
    CHECK(off.zone_hi == 0);
    CHECK(off.in_city == 0);
    CHECK(off.reserve == 1500);   // the characters stay; only nobody logs in
    PlainBots const med = PopLevelBots(PopLevel::Medium, 0);
    CHECK(med.active_cap == 250);
    CHECK(med.zone_lo == 30);
    CHECK(med.zone_hi == 45);
    CHECK(med.in_city == 80);
    CHECK(PopLevelBots(PopLevel::High, 0).active_cap == 500);
    CHECK(PopLevelBots(PopLevel::Low, 0).active_cap == 120);
}

TEST_CASE("A step down logs bots out first, a step up does not", "[ClassicBotSize]")
{
    PlainBots const high = PresetBots(BotSize::High);
    PlainBots const low = PresetBots(BotSize::Low);
    CHECK(PopLevelNeedsDrain(high.active_cap, high.zone_hi, high.in_city, low));
    CHECK(PopLevelNeedsDrain(low.active_cap, low.zone_hi, low.in_city, PopLevelBots(PopLevel::Off, 600)));
    CHECK(!PopLevelNeedsDrain(low.active_cap, low.zone_hi, low.in_city, high));
    CHECK(!PopLevelNeedsDrain(high.active_cap, high.zone_hi, high.in_city, high));
}
