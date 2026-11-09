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

#include "../../src/modules/PlayerbotV2/Diagnostics/DamageParityMath.h"
#include "../../src/modules/PlayerbotV2/Diagnostics/ScenarioDefs.h"

#include <cctype>
#include <iterator>
#include <set>
#include <string>

using namespace Playerbot::Diagnostics;
using namespace Playerbot::Diagnostics::Parity;

TEST_CASE("a spell's tally counts outcomes and averages the normal hits apart from the crits", "[DamageParity]")
{
    SpellAgg a;
    for (int i = 0; i < 8; ++i) a.AddOutcome(Outcome::Hit, 1000);
    for (int i = 0; i < 2; ++i) a.AddOutcome(Outcome::Crit, 2000);
    a.AddOutcome(Outcome::Miss, 0);
    a.AddOutcome(Outcome::Resist, 0);
    a.AddTick(100);
    a.AddTick(120);

    CHECK(a.Attempts() == 12);
    CHECK(a.Landed() == 10);
    CHECK(a.N(Outcome::Crit) == 2);
    CHECK(a.AvgNormal() == Catch::Approx(1000.0));
    CHECK(a.AvgCrit() == Catch::Approx(2000.0));
    CHECK(a.CritMultiplier() == Catch::Approx(2.0));
    CHECK(a.CritPct() == Catch::Approx(20.0));
    CHECK(a.AvoidPct() == Catch::Approx(100.0 * 2 / 12));
    CHECK(a.min_hit == 1000);
    CHECK(a.max_hit == 2000);
    CHECK(a.ticks == 2);
    CHECK(a.tick_damage == 220);
    CHECK(a.DirectDamage() == 8u * 1000 + 2u * 2000);
    CHECK(a.TotalDamage() == a.DirectDamage() + 220);
}

TEST_CASE("an empty tally has no averages and no division by zero", "[DamageParity]")
{
    SpellAgg a;
    CHECK(a.AvgNormal() == 0.0);
    CHECK(a.CritPct() == 0.0);
    CHECK(a.AvoidPct() == 0.0);
    CHECK(a.CritMultiplier() == 0.0);
    CHECK(a.TotalDamage() == 0);
}

TEST_CASE("an avoided attack deals no damage and does not enter the min and max", "[DamageParity]")
{
    SpellAgg a;
    a.AddOutcome(Outcome::Dodge, 0);
    a.AddOutcome(Outcome::Parry, 0);
    CHECK(a.min_hit == 0);
    CHECK(a.max_hit == 0);
    CHECK(a.Landed() == 0);
    CHECK(a.AvoidPct() == Catch::Approx(100.0));
    CHECK_FALSE(DealsDamage(Outcome::Miss));
    CHECK(DealsDamage(Outcome::Glancing));
}

TEST_CASE("the gap and the tolerance are relative to the expected value", "[DamageParity]")
{
    CHECK(GapPct(1150.0, 1000.0) == Catch::Approx(15.0));
    CHECK(GapPct(850.0, 1000.0) == Catch::Approx(-15.0));
    CHECK(GapPct(0.0, 0.0) == 0.0);
    CHECK(GapPct(5.0, 0.0) > 100.0);
    CHECK(WithinPct(1040.0, 1000.0, 5.0));
    CHECK_FALSE(WithinPct(1060.0, 1000.0, 5.0));
    CHECK(CompareValue(0.86, 0.857, 5.0) == Verdict::Ok);
    CHECK(CompareValue(0.40, 0.857, 20.0) == Verdict::TooLow);
    CHECK(CompareValue(1.50, 0.857, 20.0) == Verdict::TooHigh);
    CHECK(CompareValue(1.0, 0.0, 20.0) == Verdict::NoData);
}

TEST_CASE("the measured coefficient is the change in the hit per point of the stat", "[DamageParity]")
{
    // Shadow Bolt: +500 spell power at 0.857 with Shadow Mastery's 10 %: (510 + 0.857 * 630) * 1.10 = 1150 -> + 471 = 1621.
    double const base = (510.0 + 0.857 * 630.0) * 1.10;
    double const with = (510.0 + 0.857 * 1130.0) * 1.10;
    CHECK(base == Catch::Approx(1150.0).margin(6.0));   // the research note's ~1,150 against the 1,132-1,183 seen on the server
    // The coefficient seen through the 10 % is 0.857 * 1.10; a vanilla 0.857 needs the Shadow Mastery factor taken out by the caller.
    CHECK(CoefficientPerPoint(base, with, 500.0) == Catch::Approx(0.857 * 1.10));
    CHECK(CoefficientPerPoint(1.0, 2.0, 0.0) == 0.0);
}

TEST_CASE("vanilla armor takes armor / (armor + 400 + 85 * attacker level) off, at most 75 percent", "[DamageParity]")
{
    CHECK(VanillaArmorReduction(0.0, 60) == 0.0);
    CHECK(VanillaArmorReduction(3731.0, 60) == Catch::Approx(3731.0 / (3731.0 + 5500.0)));
    CHECK(1.0 - VanillaArmorReduction(3731.0, 60) == Catch::Approx(0.596).margin(0.001));
    // Five Sunders: 3731 - 5 * 520.
    CHECK(1.0 - VanillaArmorReduction(1131.0, 60) == Catch::Approx(0.829).margin(0.001));
    CHECK(VanillaArmorReduction(100000.0, 60) == 0.75);
}

TEST_CASE("a measured crit rate is judged against the sheet with the sampling noise of its size", "[DamageParity]")
{
    CHECK(CritPctConsistent(24.0, 25.0, 400));
    CHECK_FALSE(CritPctConsistent(12.0, 25.0, 400));
    // A small sample says nothing.
    CHECK(CritPctConsistent(0.0, 25.0, 10));
    // 30 hits at 25 %: sigma 7.9 points, so 10 % is still inside three sigma plus the slack.
    CHECK(CritPctConsistent(10.0, 25.0, 30));
    CHECK_FALSE(CritPctConsistent(0.0, 25.0, 300));
}

TEST_CASE("a finisher series must rise with its points", "[DamageParity]")
{
    // Eviscerate-like: base + k * points.
    CHECK(ScalingVerdict({ 500, 700, 900, 1100, 1300 }) == Scaling::Scales);
    // The bug: the points are spent before the damage is computed, so every cast does the one-point damage.
    CHECK(ScalingVerdict({ 500, 502, 498, 501, 499 }) == Scaling::Flat);
    // A dip between two points is its own bug.
    CHECK(ScalingVerdict({ 500, 900, 700, 1100, 1300 }) == Scaling::NotMonotonic);
    // A series that falls overall.
    CHECK(ScalingVerdict({ 1300, 1100, 900, 700, 500 }) == Scaling::NotMonotonic);
    CHECK(ScalingVerdict({ 500 }) == Scaling::TooFewPoints);
    CHECK(ScalingVerdict({ 500, 0, 900 }) == Scaling::TooFewPoints);
    // Noise of a percent between steps is not a dip.
    CHECK(ScalingVerdict({ 500, 700, 695, 1100, 1300 }) == Scaling::Scales);
}

TEST_CASE("the step share says how much one point adds", "[DamageParity]")
{
    // 200 a point over a mean of 900: 0.22.
    CHECK(LinearStepShare({ 500, 700, 900, 1100, 1300 }) == Catch::Approx(200.0 / 900.0));
    CHECK(LinearStepShare({ 500 }) == 0.0);
    CHECK(LinearStepShare({ 500, 500, 500 }) == 0.0);
}

TEST_CASE("a talent ratio is held to the rule it names", "[DamageParity]")
{
    // Improved Shadow Bolt: +20 % on the hits after a crit.
    CHECK(RatioVerdict(1.20, 1.20, 8.0) == Verdict::Ok);
    CHECK(RatioVerdict(1.17, 1.20, 8.0) == Verdict::Ok);
    CHECK(RatioVerdict(1.00, 1.20, 8.0) == Verdict::TooLow);   // the debuff does nothing
    CHECK(RatioVerdict(1.45, 1.20, 8.0) == Verdict::TooHigh);
    // Ruin: a crit is 2.0 times a normal hit instead of 1.5.
    CHECK(RatioVerdict(1.5, 2.0, 8.0) == Verdict::TooLow);
}

TEST_CASE("the reference ranges: a class inside its range passes, outside is below or above", "[DamageParity]")
{
    std::string text;
    // Warlock, Naxxramas gear with buffs: 700-950.
    CHECK(DpsVerdict(9, 3, true, 800.0, &text) == RangeVerdict::In);
    CHECK(text == "700-950");
    CHECK(DpsVerdict(9, 3, true, 600.0) == RangeVerdict::Below);
    CHECK(DpsVerdict(9, 3, true, 1200.0) == RangeVerdict::Above);
    // Without raid buffs 15 percent under the low end is allowed.
    CHECK(DpsVerdict(9, 3, false, 610.0) == RangeVerdict::In);
    CHECK(DpsVerdict(9, 3, false, 590.0) == RangeVerdict::Below);
    // Tier 1 reads the Molten Core column.
    CHECK(DpsVerdict(1, 1, false, 420.0, &text) == RangeVerdict::In);
    CHECK(text == "350-500");
    // A class with two damage specs takes the widest range of the two.
    CHECK(DpsVerdict(11, 3, true, 790.0, &text) == RangeVerdict::In);
    CHECK(text == "450-800");
    // Frost and arcane mages have their own range; fire (and an unknown spec) keeps the mage one.
    CHECK(DpsVerdict(8, 3, true, 564.0, &text, 64) == RangeVerdict::In);
    CHECK(text == "520-710");
    CHECK(DpsVerdict(8, 3, true, 634.0, &text, 62) == RangeVerdict::In);
    CHECK(DpsVerdict(8, 3, true, 564.0, &text, 63) == RangeVerdict::Below);
    CHECK(text == "700-950");
    // No gear, no reference.
    CHECK(DpsVerdict(9, 0, true, 100.0) == RangeVerdict::NoReference);
    // Every class the scenarios cover but the healers has a range (priest: shadow).
    for (Playerbot::Diagnostics::ParityRow const& r : Playerbot::Diagnostics::kParityClasses)
    {
        if (r.cls == ScnClass::Paladin)
            continue;
        CHECK_FALSE(RangeKeysForClass(r.cls).empty());
    }
}

TEST_CASE("the vanilla spell power coefficients are found by name", "[DamageParity]")
{
    CHECK(VanillaSpCoefficient("Shadow Bolt") == Catch::Approx(0.857));
    CHECK(VanillaSpCoefficient("Frostbolt") == Catch::Approx(0.814));
    CHECK(VanillaSpCoefficient("Eviscerate") == 0.0);
}

TEST_CASE("a spell line carries the counts a reader greps for", "[DamageParity]")
{
    SpellAgg a;
    a.spell_id = 11661;
    a.AddOutcome(Outcome::Hit, 1000);
    a.AddOutcome(Outcome::Crit, 2000);
    a.AddOutcome(Outcome::Miss, 0);
    std::string const line = FormatSpellLine("t3", "Warlock", "Shadow Bolt", a, 60.0);
    CHECK(line.find("[dmg_parity] spell leg=t3 class=Warlock spell=11661 name=\"Shadow Bolt\"") == 0);
    CHECK(line.find("attempts=3 hit=1 crit=1 miss=1") != std::string::npos);
    CHECK(line.find("per_min=3.0") != std::string::npos);
}

TEST_CASE("the parity scenarios are in the catalogue under unique names and stay out of a plain runall", "[DamageParity]")
{
    std::set<std::string> names;
    int damage = 0, finisher = 0, audit = 0, economy = 0, incoming = 0, trap = 0, threat = 0, scorecard = 0, support = 0;
    for (ScenarioDef const& d : ScenarioCatalogue())
    {
        if (!d.parity)
            continue;
        CHECK(names.insert(d.name).second);
        CHECK(d.goal == ScenarioGoalKind::Parity);
        CHECK(d.level == 60);
        CHECK(d.full_clear);   // `runall` without a filter skips full clears
        if (d.parity == 8)
        {
            CHECK(d.name == "dps_scorecard_raid");
            CHECK(d.squad.size() == std::size(kScorecardRaid));
            ++scorecard;
            continue;
        }
        if (d.parity == 9)
        {
            CHECK(std::string(d.name).rfind("support_scorecard_", 0) == 0);
            CHECK(d.squad.size() == 1 + std::size(kSupportRaid));   // the tank, then five healers and four dps
            CHECK(d.squad[0].role == ScenarioRoleKind::Tank);
            ++support;
            continue;
        }
        CHECK(d.squad.size() == 1);
        std::string const n = d.name;
        if (d.parity == 1) { ++damage; CHECK(n.rfind("damage_parity_", 0) == 0); }
        if (d.parity == 2) { ++finisher; CHECK(n.rfind("finisher_sweep_", 0) == 0); }
        if (d.parity == 3) { ++audit; CHECK(n.rfind("talent_audit_", 0) == 0); }
        if (d.parity == 4) { ++economy; CHECK(n.rfind("economy_audit_", 0) == 0); }
        if (d.parity == 5) { ++incoming; CHECK(n.rfind("incoming_damage_", 0) == 0); }
        if (d.parity == 6) { ++trap; CHECK(n == "trap_audit_hunter"); CHECK(d.squad[0].cls == ScnClass::Hunter); }
        if (d.parity == 7) { ++threat; CHECK(n.rfind("threat_audit_", 0) == 0); }
    }
    CHECK(economy == 6);    // priest, paladin, shaman, druid, mage, warlock
    CHECK(incoming == 2);   // warrior, paladin
    CHECK(trap == 1);       // hunter
    CHECK(threat == 4);     // rogue, priest, hunter, druid
    CHECK(damage == 18);    // nine classes plus the spec variants: warrior Fury and Protection, mage Fire and Arcane, warlock x3, druid cat, shaman Elemental
    CHECK(scorecard == 1);
    CHECK(support == int(std::size(kSupportTanks)));   // warrior, paladin, druid
    CHECK(finisher == 3);   // rogue, druid, warrior
    CHECK(audit == 9);
    CHECK(FindScenario("damage_parity_warlock") != nullptr);
    CHECK(FindScenario("finisher_sweep_rogue") != nullptr);
    CHECK(FindScenario("finisher_sweep_mage") == nullptr);
    REQUIRE(FindScenario("damage_parity_warrior_fury") != nullptr);
    CHECK(FindScenario("damage_parity_warrior_fury")->parity_spec == 72);
    REQUIRE(FindScenario("damage_parity_warrior_prot") != nullptr);
    CHECK(FindScenario("damage_parity_warrior_prot")->parity_spec == 73);
    CHECK(FindScenario("damage_parity_warrior")->parity_spec == 0);
    REQUIRE(FindScenario("damage_parity_mage_fire") != nullptr);
    CHECK(FindScenario("damage_parity_mage_fire")->parity_spec == 63);
    REQUIRE(FindScenario("damage_parity_mage_arcane") != nullptr);
    CHECK(FindScenario("damage_parity_mage_arcane")->parity_spec == 62);
    REQUIRE(FindScenario("damage_parity_warlock_affliction") != nullptr);
    CHECK(FindScenario("damage_parity_warlock_affliction")->parity_spec == 265);
    REQUIRE(FindScenario("damage_parity_warlock_demonology") != nullptr);
    CHECK(FindScenario("damage_parity_warlock_demonology")->parity_spec == 266);
    REQUIRE(FindScenario("damage_parity_warlock_destruction") != nullptr);
    CHECK(FindScenario("damage_parity_warlock_destruction")->parity_spec == 267);
    REQUIRE(FindScenario("damage_parity_shaman_elemental") != nullptr);
    CHECK(FindScenario("damage_parity_shaman_elemental")->parity_spec == 262);
    CHECK(FindScenario("damage_parity_shaman")->parity_spec == 263);
    CHECK(FindScenario("damage_parity_paladin")->parity_spec == 70);
    CHECK(FindScenario("damage_parity_rogue")->parity_spec == 260);
    CHECK(FindScenario("damage_parity_priest")->parity_spec == 258);
    CHECK(FindScenario("damage_parity_druid")->parity_spec == 102);
    CHECK(FindScenario("damage_parity_hunter")->parity_spec == 254);
}

TEST_CASE("the dps scorecard raid fields a tank, three healers and one bot per dps spec under unique names", "[DamageParity]")
{
    ScenarioDef const* d = FindScenario("dps_scorecard_raid");
    REQUIRE(d != nullptr);
    CHECK(d->parity == 8);
    CHECK(d->squad.size() == 23);
    std::set<std::string> names;
    std::set<std::pair<std::uint8_t, std::uint32_t>> specs;
    int tanks = 0, healers = 0, dps = 0;
    for (ScenarioMember const& m : d->squad)
    {
        CHECK(names.insert(m.name).second);
        CHECK(m.spec != 0);
        CHECK(specs.insert({ m.cls, m.spec }).second);
        for (char c : m.name)
            CHECK(std::isalpha(static_cast<unsigned char>(c)));
        if (m.role == ScenarioRoleKind::Tank) ++tanks;
        else if (m.role == ScenarioRoleKind::Healer) ++healers;
        else ++dps;
    }
    CHECK(tanks == 1);
    CHECK(healers == 3);
    CHECK(dps == 19);
}

TEST_CASE("the summary table row and header line up", "[DamageParity]")
{
    std::string const row = SummaryRow("Warlock", "t3", "t3", false, 812.4, "700-950", RangeVerdict::In, "Shadow Bolt 90000, Corruption 20000");
    CHECK(row == "| Warlock | t3 | t3 | off | 812 | 700-950 | in_range | Shadow Bolt 90000, Corruption 20000 |");
    CHECK(HeaderRow().find("| class | leg | gear | buffs | dps | range | verdict | top gap spells |") == 0);
}
