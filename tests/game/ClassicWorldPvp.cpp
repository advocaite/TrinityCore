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

#include "../../src/modules/PlayerbotV2/Fleet/WorldPvp.h"

using namespace Playerbot::V2::Fleet;

TEST_CASE("Only PvP, RP-PvP and free-for-all realms count", "[ClassicWorldPvp]")
{
    CHECK((RealmPvpOf(0) == RealmPvp::None));
    CHECK((RealmPvpOf(6) == RealmPvp::None));
    CHECK((RealmPvpOf(4) == RealmPvp::None));
    CHECK((RealmPvpOf(1) == RealmPvp::PvP));
    CHECK((RealmPvpOf(8) == RealmPvp::PvP));
    CHECK((RealmPvpOf(16) == RealmPvp::Ffa));
}

TEST_CASE("A normal realm or the setting off: no flags, no hunters, no change to the mix", "[ClassicWorldPvp]")
{
    WorldPvpCfg c;
    for (uint64_t b = 1; b < 2000; ++b)
    {
        CHECK(!IsPvpFlagged(c, RealmPvp::None, b));
        CHECK(!IsPvpHunter(c, RealmPvp::None, b));
    }
    CHECK(PvpContestedPct(c, RealmPvp::None, 100) == 100);
    c.enable = false;
    for (uint64_t b = 1; b < 2000; ++b)
        CHECK(!IsPvpHunter(c, RealmPvp::PvP, b));
    CHECK(PvpContestedPct(c, RealmPvp::PvP, 100) == 100);
    CHECK(PvpBorderPct(c, RealmPvp::Ffa, 100) == 100);
}

TEST_CASE("Shares come out near the settings, and the same bot always gets the same temper", "[ClassicWorldPvp]")
{
    WorldPvpCfg c;
    uint32_t flagged = 0, hunters = 0, ffa_hunters = 0, hunter_not_flagged = 0;
    for (uint64_t b = 1000; b < 11000; ++b)
    {
        bool const f = IsPvpFlagged(c, RealmPvp::PvP, b), h = IsPvpHunter(c, RealmPvp::PvP, b);
        flagged += f; hunters += h; ffa_hunters += IsPvpHunter(c, RealmPvp::Ffa, b);
        if (h && !f) ++hunter_not_flagged;
        CHECK(IsPvpHunter(c, RealmPvp::PvP, b) == h);
    }
    CHECK(hunter_not_flagged == 0);              // every hunter is flagged
    CHECK(hunters > 2700); CHECK(hunters < 3300);           // 30 %
    CHECK(flagged > 6200); CHECK(flagged < 6800);           // 30 % + half of the other 70 % = 65 %
    CHECK(ffa_hunters > 5700); CHECK(ffa_hunters < 6300);   // 60 % on free-for-all
}

TEST_CASE("One roll per enemy per window: the same in the window, varies across enemies and windows", "[ClassicWorldPvp]")
{
    WorldPvpCfg c;   // 40 %, 10 minutes
    uint64_t const bot = 4242;
    uint32_t yes = 0;
    for (uint64_t e = 1; e <= 5000; ++e)
    {
        bool const a = AttackRoll(c, RealmPvp::PvP, bot, e, 600 * 7);
        CHECK(AttackRoll(c, RealmPvp::PvP, bot, e, 600 * 7 + 599) == a);   // the whole window
        yes += a;
    }
    CHECK(yes > 1800); CHECK(yes < 2200);
    uint32_t changed = 0;
    for (uint64_t w = 0; w < 200; ++w)
        changed += AttackRoll(c, RealmPvp::PvP, bot, 77, w * 600) != AttackRoll(c, RealmPvp::PvP, bot, 77, (w + 1) * 600);
    CHECK(changed > 40);   // a passed-by enemy may be jumped next time
    c.attack_pct = 0;
    c.ffa_attack_pct = 0;
    for (uint64_t e = 1; e <= 500; ++e)
        CHECK(!AttackRoll(c, RealmPvp::Ffa, bot, e, 1234));
}

TEST_CASE("Level window of a hunter", "[ClassicWorldPvp]")
{
    WorldPvpCfg c;   // 5 under, 2 over
    CHECK(LevelOk(c, 40, 40));
    CHECK(LevelOk(c, 40, 35));
    CHECK(!LevelOk(c, 40, 34));
    CHECK(LevelOk(c, 40, 42));
    CHECK(!LevelOk(c, 40, 43));
    CHECK(LevelOk(c, 3, 1));
}

TEST_CASE("On a PvP realm the border and contested zones get enemy bots", "[ClassicWorldPvp]")
{
    WorldPvpCfg c;
    CHECK(PvpBorderPct(c, RealmPvp::PvP, 100) == 85);
    CHECK(PvpContestedPct(c, RealmPvp::PvP, 100) == 70);
    CHECK(PvpContestedPct(c, RealmPvp::Ffa, 100) == 70);
    CHECK(PvpContestedPct(c, RealmPvp::PvP, 50) == 50);   // a file that already asks for more enemies keeps its number
}

TEST_CASE("Every class opens an ambush its own way", "[ClassicWorldPvp]")
{
    PvpOpener const* o = nullptr;
    CHECK(PvpOpeners(1, false, o) == 1);  CHECK(o[0].spell == 100);    // warrior: Charge, 8-25 y
    CHECK(o[0].min_yards == 8.f);
    CHECK(PvpOpeners(2, false, o) == 2);  CHECK(o[0].spell == 20271);  CHECK(o[0].needs_seal);   CHECK(o[1].spell == 853);
    CHECK(PvpOpeners(3, false, o) == 1);  CHECK(o[0].spell == 1978);   // hunter: Serpent Sting
    CHECK(PvpOpeners(4, false, o) == 0);  CHECK(o == nullptr);         // rogue: its own stealth opener
    CHECK(PvpOpeners(5, false, o) == 2);  CHECK(o[0].spell == 589);    // priest: Shadow Word: Pain
    CHECK(PvpOpeners(7, false, o) == 2);  CHECK(o[0].spell == 8042);   CHECK(o[1].spell == 403);
    CHECK(PvpOpeners(8, false, o) == 2);  CHECK(o[0].spell == 116);    // mage: Frostbolt
    CHECK(PvpOpeners(9, false, o) == 2);  CHECK(o[0].spell == 172);    // warlock: Corruption
    CHECK(PvpOpeners(11, false, o) == 2); CHECK(o[0].spell == 8921);   // druid: Moonfire
    CHECK(PvpOpeners(11, true, o) == 0);                               // druid in cat / bear form: walks in
    for (uint8_t cls : { 1, 2, 3, 5, 7, 8, 9, 11 })
    {
        int const n = PvpOpeners(cls, false, o);
        for (int i = 0; i < n; ++i)
        {
            CHECK(o[i].max_yards <= 35.f);   // never beyond what the hunter looks at
            CHECK(o[i].min_yards < o[i].max_yards);
        }
    }
}
