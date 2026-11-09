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

#include "../../src/modules/PlayerbotV2/Bot/Raid/BossWarningData_MC.h"

using namespace Playerbot::Raid;

namespace
{
    std::vector<BossWarning> const kRules = {
        { 1, 100, "Panic",  false, 10000, 30000, 5000, 100 },
        { 1, 200, "Bolt",   true,      0,     0,    0, 0 },
        { 1, 300, "Boom",   false, 60000,     0, 10000, 0 },
    };
    std::vector<uint8_t> const kNoAura(3, 0);

    size_t Count(std::vector<BossWarnEvent> const& ev, size_t rule)
    {
        size_t n = 0;
        for (BossWarnEvent const& e : ev)
            n += e.rule == rule;
        return n;
    }
}

TEST_CASE("Predicted ability warns lead time before the first earliest time", "[ClassicBossWarnings]")
{
    BossWarnTracker t(&kRules);
    t.Start(1000);
    CHECK(t.Tick(5000, 0, 0, kNoAura).empty());          // 6 s to go, lead is 5 s
    auto ev = t.Tick(6200, 0, 0, kNoAura);                // 4.8 s to go
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].rule == 0);
    CHECK_FALSE(ev[0].casting);
    CHECK(ev[0].secs == 5);
    CHECK(t.Tick(6400, 0, 0, kNoAura).empty());          // only once
}

TEST_CASE("Cast bar announces once per cast with the time left", "[ClassicBossWarnings]")
{
    BossWarnTracker t(&kRules);
    t.Start(0);
    auto ev = t.Tick(2000, 200, 2500, kNoAura);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].casting);
    CHECK(ev[0].secs == 3);
    CHECK(Count(t.Tick(2250, 200, 2250, kNoAura), 1) == 0);   // same cast, still on the bar
    CHECK(Count(t.Tick(3000, 0, 0, kNoAura), 1) == 0);        // bar gone
    CHECK(Count(t.Tick(4000, 200, 2500, kNoAura), 1) == 1);   // next cast
}

TEST_CASE("Seeing the aura land re-anchors the cycle", "[ClassicBossWarnings]")
{
    BossWarnTracker t(&kRules);
    t.Start(0);
    t.Tick(5500, 0, 0, kNoAura);                          // warned for the 10 s mark
    std::vector<uint8_t> aura = { 1, 0, 0 };
    CHECK(t.Tick(13000, 0, 0, aura).empty());             // landed late (script rolled high): no warning now
    CHECK(t.Tick(20000, 0, 0, aura).empty());             // aura still up: not a new cast
    CHECK(t.Tick(25000, 0, 0, kNoAura).empty());          // next earliest is 13 s + 30 s = 43 s; warn at 38 s
    CHECK(t.Tick(37000, 0, 0, kNoAura).empty());
    auto ev = t.Tick(38500, 0, 0, kNoAura);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].secs == 5);
}

TEST_CASE("Unseen cast moves on to the next cycle after the grace period", "[ClassicBossWarnings]")
{
    BossWarnTracker t(&kRules);
    t.Start(0);
    REQUIRE(t.Tick(6000, 0, 0, kNoAura).size() == 1);     // earliest 10 s
    CHECK(t.Tick(10000 + BossWarnTracker::kLateGraceMs, 0, 0, kNoAura).empty());   // cycle advanced to 40 s
    CHECK(t.Tick(30000, 0, 0, kNoAura).empty());
    REQUIRE(t.Tick(36000, 0, 0, kNoAura).size() == 1);
}

TEST_CASE("One-shot timer fires once and never repeats", "[ClassicBossWarnings]")
{
    static std::vector<BossWarning> const one = { { 1, 300, "Boom", false, 60000, 0, 10000, 0 } };
    BossWarnTracker t(&one);
    t.Start(0);
    CHECK(t.Tick(49000, 0, 0, {}).empty());
    REQUIRE(t.Tick(52000, 0, 0, {}).size() == 1);
    CHECK(t.Tick(75000, 0, 0, {}).empty());
    CHECK(t.Tick(400000, 0, 0, {}).empty());
}

TEST_CASE("Warning text reads like a callout", "[ClassicBossWarnings]")
{
    CHECK(FormatBossWarning("Magmadar", kRules[0], { 0, false, 5 }) == "Magmadar: Panic in about 5 sec");
    CHECK(FormatBossWarning("Magmadar", kRules[0], { 0, false, 0 }) == "Magmadar: Panic any moment now");
    CHECK(FormatBossWarning("Shazzrah", kRules[1], { 1, true, 2 }) == "Shazzrah is casting Bolt (2 sec)");
    CHECK(FormatBossWarning("Shazzrah", kRules[1], { 1, true, 0 }) == "Shazzrah is casting Bolt");
}

TEST_CASE("Molten Core table is sane", "[ClassicBossWarnings]")
{
    auto const& rules = MoltenCoreBossWarnings();
    CHECK(rules.size() >= 10);
    for (BossWarning const& r : rules)
    {
        CHECK(r.label != nullptr);
        CHECK(std::string(r.label).size() > 3);
        // a rule must be able to warn: a cast bar or a timer
        CHECK((r.warnOnCast || r.firstMs || r.repeatMs));
        // the lead can't exceed the time it counts down from
        if (r.firstMs)
            CHECK(r.leadMs <= r.firstMs);
        if (r.repeatMs)
            CHECK(r.leadMs < r.repeatMs);
    }
    // the bosses the user named are covered
    bool magmadarPanic = false;
    for (BossWarning const& r : rules)
        magmadarPanic = magmadarPanic || (r.boss == 11982 && r.spell == 19408);
    CHECK(magmadarPanic);
}
