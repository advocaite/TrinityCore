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
#include "../../src/modules/PlayerbotV2/Bot/Raid/BossTactics.h"
#include "../../src/modules/PlayerbotV2/Bot/Raid/RaidBanter.h"

using namespace Playerbot::Raid;
namespace CcDeny = Playerbot::CcDeny;

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

TEST_CASE("Boss tactics: the player's words", "[ClassicBossWarnings]")
{
    auto o = ParseTacticsOrder("tactics Lucifron: adds first, no aoe and tank here");
    REQUIRE(o.has_value());
    CHECK(o->boss == "lucifron");
    CHECK((o->kill == BossTactics::Kill::AddsFirst));
    CHECK(o->no_aoe == true);
    CHECK(o->tank_here);
    CHECK(o->unknown.empty());

    o = ParseTacticsOrder("Tactics for Baron Geddon: boss first, no fear, no sheep.");
    REQUIRE(o.has_value());
    CHECK(o->boss == "baron geddon");
    CHECK((o->kill == BossTactics::Kill::BossFirst));
    CHECK(o->cc_add == (CcDeny::Fear | CcDeny::Polymorph));

    o = ParseTacticsOrder("tactics: kill the adds first");      // the target
    REQUIRE(o.has_value());
    CHECK(o->boss.empty());
    CHECK((o->kill == BossTactics::Kill::AddsFirst));

    o = ParseTacticsOrder("tactics adds first, no aoe");        // no colon, the target
    REQUIRE(o.has_value());
    CHECK(o->boss.empty());
    CHECK((o->kill == BossTactics::Kill::AddsFirst));
    CHECK(o->no_aoe == true);

    o = ParseTacticsOrder("tactics on lucifron adds first, no aoe");   // the name in front, no colon
    REQUIRE(o.has_value());
    CHECK(o->boss == "lucifron");
    CHECK((o->kill == BossTactics::Kill::AddsFirst));
    CHECK(o->no_aoe == true);

    o = ParseTacticsOrder("tactics lucifron");                  // show
    REQUIRE(o.has_value());
    CHECK(o->boss == "lucifron");
    CHECK(o->Show());

    o = ParseTacticsOrder("tactics");                           // list
    REQUIRE(o.has_value());
    CHECK(o->boss.empty());
    CHECK(o->Show());

    o = ParseTacticsOrder("tactics geddon: clear");
    REQUIRE(o.has_value());
    CHECK(o->clear);

    o = ParseTacticsOrder("tactics: adds first, dance in a circle");
    REQUIRE(o.has_value());
    REQUIRE(o->unknown.size() == 1);
    CHECK(o->unknown[0] == "dance in a circle");
    CHECK_FALSE(o->Show());

    o = ParseTacticsOrder("tactics: fear ok, aoe on, tank anywhere");
    REQUIRE(o.has_value());
    CHECK(o->cc_remove == CcDeny::Fear);
    CHECK(o->no_aoe == false);
    CHECK(o->spot_off);

    CHECK_FALSE(ParseTacticsOrder("tacticsss").has_value());
    CHECK_FALSE(ParseTacticsOrder("no fear").has_value());
    CHECK_FALSE(ParseTacticsOrder("what tactics do you know").has_value());
}

TEST_CASE("Boss tactics: kept, changed and read back", "[ClassicBossWarnings]")
{
    BossTactics t;
    ApplyTacticsOrder(t, *ParseTacticsOrder("tactics lucifron: adds first, no aoe, no fear, tank here"), 409, 1012.46f, -480.24f, -180.0f);
    CHECK((t.kill == BossTactics::Kill::AddsFirst));
    CHECK(t.no_aoe);
    CHECK(t.cc_deny == CcDeny::Fear);
    CHECK(t.spot);
    CHECK(t.spot_map == 409);
    CHECK(DescribeTactics(t) == "adds first, no AoE, no fear, tank at your spot");

    std::string const stored = SerializeTactics(t);
    CHECK(stored == "kill=adds;aoe=0;cc=1;spot=409,1012.5,-480.2,-180.0");
    BossTactics const back = ParseStoredTactics(stored);
    CHECK((back.kill == t.kill));
    CHECK(back.no_aoe);
    CHECK(back.cc_deny == CcDeny::Fear);
    CHECK(back.spot_map == 409);
    CHECK(back.x == Catch::Approx(1012.5f));
    CHECK(back.y == Catch::Approx(-480.2f));

    // A later order changes only what it names.
    ApplyTacticsOrder(t, *ParseTacticsOrder("tactics: aoe ok, fear ok"), 409, 0, 0, 0);
    CHECK((t.kill == BossTactics::Kill::AddsFirst));
    CHECK_FALSE(t.no_aoe);
    CHECK(t.cc_deny == 0);
    CHECK(t.spot);

    ApplyTacticsOrder(t, *ParseTacticsOrder("tactics: clear"), 409, 0, 0, 0);
    CHECK(t.Empty());
    CHECK(SerializeTactics(t).empty());
    CHECK(DescribeTactics(t) == "nothing");
    CHECK(ParseStoredTactics("").Empty());
    CHECK((ParseStoredTactics("kill=adds;future=7;spot=bad").kill == BossTactics::Kill::AddsFirst));   // unknown parts left out
    CHECK_FALSE(ParseStoredTactics("spot=bad").spot);
}

TEST_CASE("Boss tactics: boss names as players type them", "[ClassicBossWarnings]")
{
    CHECK(BossNameMatches("lucifron", "Lucifron"));
    CHECK(BossNameMatches("geddon", "Baron Geddon"));
    CHECK(BossNameMatches("baron geddon", "Baron Geddon"));
    CHECK(BossNameMatches("majo", "Majordomo Executus"));
    CHECK(BossNameMatches("rag", "Rag"));                  // the whole name, however short
    CHECK_FALSE(BossNameMatches("rag", "Ragnaros"));       // too short to be sure
    CHECK(BossNameMatches("ragnaros", "Ragnaros"));
    CHECK_FALSE(BossNameMatches("magmadar", "Lucifron"));
    CHECK_FALSE(BossNameMatches("baron magmadar", "Baron Geddon"));
    CHECK(BossNameMatches("venoxis", "High Priest Venoxis"));
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

TEST_CASE("Raid banter: the leader's lines", "[ClassicBossWarnings]")
{
    CHECK(LeaderLine(Mishap::StoodInBad, BanterLevel::Off, "Karrz", 0).empty());
    CHECK(LeaderLine(Mishap::Count, BanterLevel::Harsh, "Karrz", 0).empty());
    CHECK(LeaderLine(Mishap::StoodInBad, BanterLevel::Mild, "Karrz", 0) == "Karrz, out of the bad stuff pls");
    CHECK(LeaderLine(Mishap::StoodInBad, BanterLevel::Harsh, "Karrz", 0) == "Karrz GET OUT OF IT");
    CHECK(LeaderLine(Mishap::MissedKick, BanterLevel::Harsh, "Shadow Bolt", 0) == "NOBODY KICKED Shadow Bolt??");
    CHECK(LeaderLine(Mishap::BrokeCc, BanterLevel::Mild, "Gun", 1) == "careful Gun, that was cc'd");
    CHECK(LeaderLine(Mishap::BrokeCc, BanterLevel::Harsh, "", 0) == "WHO BROKE THE CC");          // nobody seen: the nameless lines
    CHECK(LeaderLine(Mishap::ExtraPull, BanterLevel::Mild, "", 2) == "adds incoming, tanks");
    CHECK(LeaderLine(Mishap::ExtraPull, BanterLevel::Harsh, "Brok", 1) == "Brok has adds, tanks GRAB THEM");
    // A wipe or a kill with nobody to name skips the lines that need a name.
    CHECK(LeaderLine(Mishap::Wipe, BanterLevel::Mild, "", 2) == "wipe, it happens. run back and rebuff");
    CHECK(LeaderLine(Mishap::Wipe, BanterLevel::Harsh, "", 0) == "that was a mess. run back and stay out of the bad stuff");
    CHECK(LeaderLine(Mishap::Wipe, BanterLevel::Harsh, "Brok", 0) == "WIPE. Brok died first. again");
    CHECK(LeaderLine(Mishap::BossDown, BanterLevel::Mild, "", 2) == "gg, nice one everyone");
    CHECK(LeaderLine(Mishap::BossDown, BanterLevel::Harsh, "Brok", 2) == "gg. Brok, we'll talk about dying later");

    // Standing in the bad stuff and a missed kick are always about someone (a raider, a spell): no line without the name.
    CHECK(LeaderLine(Mishap::StoodInBad, BanterLevel::Mild, "", 0).empty());
    CHECK(LeaderLine(Mishap::MissedKick, BanterLevel::Harsh, "", 1).empty());

    // Every line, every pick: never empty when on (a name is there, or the mishap has nameless lines), never a leftover slot.
    for (uint8_t m = 0; m < uint8_t(Mishap::Count); ++m)
        for (BanterLevel l : { BanterLevel::Mild, BanterLevel::Harsh })
            for (uint32_t pick = 0; pick < 6; ++pick)
                for (char const* name : { "", "Brok" })
                {
                    std::string const s = LeaderLine(Mishap(m), l, name, pick);
                    INFO(int(m) << " " << int(l) << " " << pick << " " << name);
                    bool const needsName = Mishap(m) == Mishap::StoodInBad || Mishap(m) == Mishap::MissedKick;
                    CHECK(s.empty() == (needsName && !*name));
                    CHECK(s.find("{}") == std::string::npos);
                }
}

TEST_CASE("Raid banter: after a wipe", "[ClassicBossWarnings]")
{
    CHECK(WipeReply(BanterLevel::Off, "Brok", false, 0).empty());
    CHECK(WipeReply(BanterLevel::Mild, "Brok", true, 0) == "my bad");
    CHECK(WipeReply(BanterLevel::Harsh, "Brok", true, 3) == "i was rooted ok");
    CHECK(WipeReply(BanterLevel::Harsh, "Brok", false, 0) == "that was Brok's fault");
    CHECK(WipeReply(BanterLevel::Mild, "", false, 5) == "healers?");                    // no one to blame: the next nameless line
    for (BanterLevel l : { BanterLevel::Mild, BanterLevel::Harsh })
        for (uint32_t pick = 0; pick < 14; ++pick)
            for (char const* blame : { "", "Brok" })
            {
                std::string const s = WipeReply(l, blame, false, pick);
                CHECK_FALSE(s.empty());
                CHECK(s.find("{}") == std::string::npos);
            }
}

TEST_CASE("Raid banter: what the program is told", "[ClassicBossWarnings]")
{
    CHECK(MishapDetail(Mishap::StoodInBad, "Karrz", "Ragnaros") == "Karrz keeps standing in a damaging area (fighting Ragnaros)");
    CHECK(MishapDetail(Mishap::MissedKick, "Shadow Bolt", "") == "nobody interrupted the enemy cast Shadow Bolt");
    CHECK(MishapDetail(Mishap::BrokeCc, "", "") == "someone broke a crowd-controlled enemy early");
    CHECK(MishapDetail(Mishap::ExtraPull, "Brok", "") == "an extra pack of enemies was pulled into the fight, they attack Brok");
    CHECK(MishapDetail(Mishap::Wipe, "Brok", "Lucifron") == "the raid just wiped (fighting Lucifron); Brok died first");
    CHECK(MishapDetail(Mishap::BossDown, "", "Lucifron") == "the raid just killed the boss Lucifron");
    CHECK(MishapDetail(Mishap::BossDown, "Brok", "Lucifron") == "the raid just killed the boss Lucifron; Brok died during the fight");
    CHECK(std::string(MishapWord(Mishap::BrokeCc)) == "broke_cc");
    CHECK(std::string(MishapWord(Mishap::Count)) == "mishap");
}

TEST_CASE("Raid banter: config and rate limits", "[ClassicBossWarnings]")
{
    CHECK((BanterLevelFromConfig(0) == BanterLevel::Off));
    CHECK((BanterLevelFromConfig(-3) == BanterLevel::Off));
    CHECK((BanterLevelFromConfig(1) == BanterLevel::Mild));
    CHECK((BanterLevelFromConfig(2) == BanterLevel::Harsh));
    CHECK((BanterLevelFromConfig(9) == BanterLevel::Harsh));
    CHECK(std::string(BanterLevelWord(BanterLevel::Harsh)) == "harsh");

    uint32_t const t = 100000;
    BanterGate g;
    CHECK_FALSE(g.Allow(1, Mishap::StoodInBad, "Karrz", BanterLevel::Off, t));
    CHECK(g.Allow(1, Mishap::StoodInBad, "Karrz", BanterLevel::Mild, t));
    CHECK_FALSE(g.Allow(1, Mishap::MissedKick, "Bolt", BanterLevel::Mild, t + 5000));                        // leader gap
    CHECK(g.Allow(2, Mishap::MissedKick, "Bolt", BanterLevel::Mild, t + 5000));                              // another raid
    CHECK(g.Allow(1, Mishap::MissedKick, "Bolt", BanterLevel::Mild, t + BanterGate::kLeaderGapMs));
    CHECK_FALSE(g.Allow(1, Mishap::StoodInBad, "Gun", BanterLevel::Mild, t + BanterGate::kLeaderGapMs * 2)); // same kind
    CHECK(g.Allow(1, Mishap::StoodInBad, "Gun", BanterLevel::Mild, t + BanterGate::kSameKindMs));
    CHECK_FALSE(g.Allow(1, Mishap::StoodInBad, "Karrz", BanterLevel::Mild, t + 2 * BanterGate::kSameKindMs - 1000));
    CHECK(g.Allow(1, Mishap::StoodInBad, "Karrz", BanterLevel::Mild, t + BanterGate::kSameNameMs + 1000));   // same raider, later

    // Harsh speaks more often; the wipe and the kill always get their line, once per fight end.
    BanterGate h;
    CHECK(h.Allow(3, Mishap::StoodInBad, "A", BanterLevel::Harsh, t));
    CHECK(h.Allow(3, Mishap::BrokeCc, "B", BanterLevel::Harsh, t + BanterGate::kLeaderGapHarshMs));
    CHECK(h.Allow(3, Mishap::Wipe, "", BanterLevel::Harsh, t + BanterGate::kLeaderGapHarshMs + 500));
    CHECK_FALSE(h.Allow(3, Mishap::BossDown, "", BanterLevel::Harsh, t + BanterGate::kLeaderGapHarshMs + 900));
    CHECK(h.Allow(3, Mishap::BossDown, "", BanterLevel::Harsh, t + BanterGate::kLeaderGapHarshMs + 500 + BanterGate::kFightEndMs));

    h.Prune(t + 500000);
    CHECK(h.Tracked() == 0);
}
