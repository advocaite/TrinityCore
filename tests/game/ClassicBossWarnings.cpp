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
#include "../../src/modules/PlayerbotV2/Bot/Raid/RaidCallouts.h"

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

TEST_CASE("Raid callout: the line for each decision", "[ClassicBossWarnings]")
{
    CalloutFacts f;
    f.move_reason = "carrier run";
    f.carried = "Living Bomb";
    Callout c = PickCallout(f);
    CHECK(c.kind == CalloutKind::CarrierRun);
    CHECK(c.text == "Living Bomb on me, running out");
    CHECK_FALSE(c.group_wide);

    f.carried.clear();
    CHECK(PickCallout(f).text == "bomb on me, running out");

    CalloutFacts spread;
    spread.move_reason = "spread";
    CHECK(PickCallout(spread).text == "spread out");
    CHECK(PickCallout(spread).group_wide);

    CalloutFacts wipe;
    wipe.move_reason = "wipe retreat";
    wipe.taunt = true;
    wipe.target = "Ragnaros";
    CHECK(PickCallout(wipe).kind == CalloutKind::WipeRetreat);   // the most urgent line wins

    CalloutFacts taunt;
    taunt.taunt = true;
    taunt.target = "Flamegor";
    CHECK(PickCallout(taunt).text == "taunting Flamegor");
    taunt.target.clear();
    CHECK(PickCallout(taunt).kind == CalloutKind::None);   // no name, nothing to say

    CalloutFacts cc;
    cc.spell = "Polymorph";
    cc.target = "Flamewaker Healer";
    CHECK(PickCallout(cc).kind == CalloutKind::None);      // a short stun is not called
    cc.long_cc = true;
    CHECK(PickCallout(cc).text == "Polymorph on Flamewaker Healer, don't break it");

    CalloutFacts orb;
    orb.move_reason = "orb: walk to the Orb of Domination";
    CHECK(PickCallout(orb).text == "taking the orb");

    CalloutFacts quiet;
    quiet.move_reason = "leave hazard";                    // every few seconds in a fight: not worth a line
    CHECK(PickCallout(quiet).kind == CalloutKind::None);
    quiet.move_reason = "role position";
    CHECK(PickCallout(quiet).kind == CalloutKind::None);
}

TEST_CASE("Raid callout: rate limits per bot and per raid", "[ClassicBossWarnings]")
{
    CalloutGate g;
    uint32_t t = 100000;
    CHECK(g.Allow(1, 10, CalloutKind::Taunt, false, t));
    CHECK_FALSE(g.Allow(1, 10, CalloutKind::CarrierRun, false, t + 3000));                     // same bot, too soon
    CHECK_FALSE(g.Allow(1, 11, CalloutKind::Taunt, false, t + 1000));                          // the raid just heard a line
    CHECK(g.Allow(1, 11, CalloutKind::Taunt, false, t + CalloutGate::kGroupGapMs));            // another bot, after the gap
    CHECK(g.Allow(1, 10, CalloutKind::CarrierRun, false, t + CalloutGate::kBotGapMs));         // same bot, another line, later
    CHECK_FALSE(g.Allow(1, 10, CalloutKind::Taunt, false, t + CalloutGate::kBotGapMs * 2));    // same line again within kSameKindMs
    CHECK(g.Allow(1, 10, CalloutKind::Taunt, false, t + CalloutGate::kSameKindMs));
    CHECK(g.Allow(2, 10, CalloutKind::None, false, t) == false);

    // A group-wide call is said once for the whole raid.
    CalloutGate w;
    CHECK(w.Allow(5, 1, CalloutKind::Spread, true, t));
    CHECK_FALSE(w.Allow(5, 2, CalloutKind::Spread, true, t + 10000));
    CHECK(w.Allow(6, 2, CalloutKind::Spread, true, t + 10000));                               // another raid
    CHECK(w.Allow(5, 3, CalloutKind::Spread, true, t + CalloutGate::kGroupWideMs));

    // At most kWindowMax lines in kWindowMs for one raid, however many bots.
    CalloutGate cap;
    uint32_t said = 0;
    for (uint64_t bot = 1; bot <= 10; ++bot)
        said += cap.Allow(9, bot, CalloutKind::Taunt, false, t + uint32_t(bot) * CalloutGate::kGroupGapMs);
    CHECK(said == CalloutGate::kWindowMax);

    // Prune forgets the old state.
    cap.Prune(t + 200000);
    CHECK(cap.Tracked() == 0);
}
