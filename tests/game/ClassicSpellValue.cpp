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

#include "../../src/server/game/Spells/ClassicSpellValue.h"

using namespace ClassicSpellValue;

namespace
{
    constexpr uint8_t SchoolDamage = 2;
    constexpr uint8_t ApplyAura = 6;
    constexpr uint16_t PeriodicDamage = 3;

    // VMaNGOS 1.12 spell_template, effect 1 and 2 of Flamestrike 11829 (Defias Evoker): 75 + 1d13 fire, then 15 + 1 every 2 s.
    Value const FlamestrikeDirect = { SchoolDamage, 0, 75, 13, 0.0f };
    Value const FlamestrikeDot = { ApplyAura, PeriodicDamage, 15, 1, 0.0f };
    // Molten Giant Smash 18944: 809 + 1d181 = 810..990 (the Classic client data carries it as 900 +-20%).
    Value const Smash = { SchoolDamage, 0, 809, 181, 0.0f };
}

TEST_CASE("Flamestrike hits for the vanilla 76 to 88, not the 479 of the client data", "[ClassicSpellValue]")
{
    CHECK(Mean(FlamestrikeDirect) == 82.0f);
    CHECK(Mean(FlamestrikeDirect) < 100.0f);
    CHECK(Variance(FlamestrikeDirect) == Catch::Approx(12.0f / 82.0f));
    // the core rolls +-variance/2 around the mean: 82 +- 6 = 76..88
    CHECK(Mean(FlamestrikeDirect) * (1.0f - Variance(FlamestrikeDirect) * 0.5f) == Catch::Approx(76.0f));
    CHECK(Mean(FlamestrikeDirect) * (1.0f + Variance(FlamestrikeDirect) * 0.5f) == Catch::Approx(88.0f));
}

TEST_CASE("A one-sided die adds exactly one and has no spread", "[ClassicSpellValue]")
{
    CHECK(Mean(FlamestrikeDot) == 16.0f);
    CHECK(Variance(FlamestrikeDot) == 0.0f);
}

TEST_CASE("No dice leaves the base points as they are", "[ClassicSpellValue]")
{
    Value const flat = { SchoolDamage, 0, 120, 0, 0.0f };
    CHECK(Mean(flat) == 120.0f);
    CHECK(Variance(flat) == 0.0f);
}

TEST_CASE("Molten Giant Smash keeps its known 900 +-10%", "[ClassicSpellValue]")
{
    CHECK(Mean(Smash) == 900.0f);
    CHECK(Variance(Smash) == Catch::Approx(0.2f));
}

TEST_CASE("A negative or zero mean has no spread to divide by", "[ClassicSpellValue]")
{
    Value const none = { SchoolDamage, 0, 0, 0, 0.0f };
    CHECK(Variance(none) == 0.0f);
    Value const negative = { ApplyAura, PeriodicDamage, -20, 5, 0.0f };
    CHECK(Variance(negative) > 0.0f);
}

TEST_CASE("A row only applies to the same effect and aura", "[ClassicSpellValue]")
{
    CHECK(MatchesEffect(FlamestrikeDirect, SchoolDamage, 0));
    CHECK_FALSE(MatchesEffect(FlamestrikeDirect, ApplyAura, 0));
    CHECK(MatchesEffect(FlamestrikeDot, ApplyAura, PeriodicDamage));
    CHECK_FALSE(MatchesEffect(FlamestrikeDot, ApplyAura, 8));
}

TEST_CASE("Every caster of a legacy spell takes the vanilla value, Forever's own spells keep theirs", "[ClassicSpellValue]")
{
    CHECK(AppliesToSpell(11829));       // creature spell
    CHECK(AppliesToSpell(25307));       // player spell (the caster no longer matters)
    CHECK(AppliesToSpell(999999));
    CHECK_FALSE(AppliesToSpell(1259652));   // Forever's own percentage spells
}

// VMaNGOS 1.12 spell_template, Shadow Bolt rank 10 (25307): 481 + 1d57, 3.1 points per level. The 70291 client data rolled 254..261.
TEST_CASE("Player Shadow Bolt rank 10 hits for the vanilla 482 to 538", "[ClassicSpellValue]")
{
    Value const shadowBolt10 = { SchoolDamage, 0, 481, 57, 3.1f };
    float const mean = Mean(shadowBolt10);
    CHECK(mean == 510.0f);
    CHECK(mean > 2.0f * 261.0f - 20.0f);                               // the client's roughly half is gone
    CHECK(mean * (1.0f - Variance(shadowBolt10) * 0.5f) == Catch::Approx(482.0f));
    CHECK(mean * (1.0f + Variance(shadowBolt10) * 0.5f) == Catch::Approx(538.0f));
}

TEST_CASE("Points per level only count above the spell's level and up to its maximum", "[ClassicSpellValue]")
{
    Value const shadowBolt10 = { SchoolDamage, 0, 481, 57, 3.1f };
    // a level 60 caster of a level 60 spell gets nothing on top
    CHECK(ValueAtLevel(shadowBolt10, 60, 60, 0) == Catch::Approx(510.0f));
    // a low rank direct damage (7 + 1, 0.6 per level, spell level 1, last level counted 6): 8 + 5 * 0.6 = 11
    Value const immolateDirect = { SchoolDamage, 0, 7, 1, 0.6f };
    CHECK(ValueAtLevel(immolateDirect, 60, 1, 6) == Catch::Approx(11.0f));
    CHECK(ValueAtLevel(immolateDirect, 1, 1, 6) == Catch::Approx(8.0f));
    // a caster below the spell's level is not penalised below the base
    CHECK(ValueAtLevel(immolateDirect, 1, 5, 6) == Catch::Approx(8.0f));
}

TEST_CASE("Spells whose client value was already vanilla keep it", "[ClassicSpellValue]")
{
    // Curse of Agony rank 1 (980): 6 + 1 = 7 per tick; the PC logged base 6 on a tick
    Value const agony1 = { ApplyAura, PeriodicDamage, 6, 1, 0.0f };
    CHECK(Mean(agony1) == 7.0f);
    CHECK(Variance(agony1) == 0.0f);
    CHECK(ValueAtLevel(agony1, 60, 8, 0) == 7.0f);
}

TEST_CASE("The vanilla value is the base: percentage modifiers and spell power come on top", "[ClassicSpellValue]")
{
    Value const shadowBolt10 = { SchoolDamage, 0, 481, 57, 3.1f };
    float const base = ValueAtLevel(shadowBolt10, 60, 60, 0);
    // Devastation / Shadow Mastery style +x% is applied by ApplyEffectModifiers to this base, the spell power coefficient
    // (0.857, from the client) is added by Unit::SpellDamageBonusDone: the order is unchanged, only the base is replaced
    float const withTalents = base * 1.15f;
    float const withSpellPower = withTalents + 638.0f * 0.857f;
    CHECK(withTalents == Catch::Approx(586.5f));
    CHECK(withSpellPower == Catch::Approx(1133.3f).margin(0.1f));
    CHECK(withSpellPower > 847.0f);     // the old hit of the client data
}

// VMaNGOS 1.12 Eviscerate rank 8 (31018 area): base points plus points per combo point on top; the client's PointsPerResource differs.
TEST_CASE("A finisher adds its vanilla points per combo point", "[ClassicSpellValue]")
{
    Value eviscerate = { SchoolDamage, 0, 199, 1, 0.0f, 0, 25.0f };    // 200 + 25 per combo point (illustrative numbers)
    CHECK(WithComboPoints(eviscerate, Mean(eviscerate), 0) == Catch::Approx(200.0f));
    CHECK(WithComboPoints(eviscerate, Mean(eviscerate), 5) == Catch::Approx(325.0f));
    Value flat = { SchoolDamage, 0, 100, 0, 0.0f };
    CHECK(WithComboPoints(flat, 100.0f, 5) == 100.0f);                  // no per-combo points: the combo points add nothing
}

TEST_CASE("A modifier aura row only applies to the client effect with the same misc value", "[ClassicSpellValue]")
{
    constexpr uint16_t AddPctModifier = 108;
    constexpr uint16_t ModDamageDone = 79;
    // Ruin: ADD_PCT_MODIFIER with misc value 7 (crit damage bonus), VMaNGOS base points 99
    Value const ruin = { ApplyAura, AddPctModifier, 99, 1, 0.0f, 7 };
    CHECK(MatchesEffect(ruin, ApplyAura, AddPctModifier, 7));
    CHECK_FALSE(MatchesEffect(ruin, ApplyAura, AddPctModifier, 0));     // reworked to modify something else: the client value stays
    CHECK_FALSE(MatchesEffect(ruin, ApplyAura, ModDamageDone, 7));
}

TEST_CASE("Periodic and absorb auras do not care about the misc value", "[ClassicSpellValue]")
{
    CHECK_FALSE(AuraNeedsMiscValue(3));
    CHECK_FALSE(AuraNeedsMiscValue(8));
    CHECK_FALSE(AuraNeedsMiscValue(69));
    CHECK(AuraNeedsMiscValue(107));
    CHECK(AuraNeedsMiscValue(108));
    CHECK(AuraNeedsMiscValue(79));
    Value const agony1 = { ApplyAura, PeriodicDamage, 6, 1, 0.0f, 0 };
    CHECK(MatchesEffect(agony1, ApplyAura, PeriodicDamage, 5));
}

TEST_CASE("The vanilla mana cost is flat plus a share of the base mana", "[ClassicSpellValue]")
{
    // Shadow Bolt rank 10: 380 flat. Seal of Command style percent cost: 14 % of 1000 base mana = 140
    Cost const shadowBolt10 = { 380, 0 };
    CHECK(ManaCost(shadowBolt10, 1300) == 380);
    Cost const percent = { 0, 14 };
    CHECK(ManaCost(percent, 1000) == 140);
    Cost const both = { 20, 10 };
    CHECK(ManaCost(both, 500) == 70);
}

TEST_CASE("The vanilla spell power factor replaces the client's only when the table has one", "[ClassicSpellValue]")
{
    // Shadow Bolt rank 10: 3.0 s cast, 3.0 / 3.5 = 0.857
    Value shadowBolt10 = { SchoolDamage, 0, 481, 57, 3.1f };
    shadowBolt10.BonusCoefficient = 0.857f;
    CHECK(BonusCoefficientOr(&shadowBolt10, 1.0f) == Catch::Approx(0.857f));
    // a row without a coefficient (-1, the default) keeps the client's
    Value const noCoefficient = { SchoolDamage, 0, 100, 0, 0.0f };
    CHECK(BonusCoefficientOr(&noCoefficient, 0.43f) == Catch::Approx(0.43f));
    // no row at all
    CHECK(BonusCoefficientOr(nullptr, 0.43f) == Catch::Approx(0.43f));
    // an explicit 0 is a real vanilla value (no spell power scaling): it wins over the client's
    Value zero = { SchoolDamage, 0, 100, 0, 0.0f };
    zero.BonusCoefficient = 0.0f;
    CHECK(BonusCoefficientOr(&zero, 0.43f) == 0.0f);
}

TEST_CASE("A creature's vanilla health, mana and armor spread linearly over its level range", "[ClassicSpellValue][ClassicCreatureStats]")
{
    CHECK(CreatureStatAtLevel(222, 247, 11, 12, 11) == 222);
    CHECK(CreatureStatAtLevel(222, 247, 11, 12, 12) == 247);
    CHECK(CreatureStatAtLevel(1000, 2000, 20, 22, 21) == 1500);
    CHECK(CreatureStatAtLevel(826088, 826088, 63, 63, 63) == 826088);
    // a level outside the range is clamped to the nearest end, and a degenerate range gives the minimum
    CHECK(CreatureStatAtLevel(100, 200, 10, 12, 5) == 100);
    CHECK(CreatureStatAtLevel(100, 200, 10, 12, 60) == 200);
    CHECK(CreatureStatAtLevel(100, 200, 12, 10, 11) == 100);
}

TEST_CASE("A creature with a vanilla melee row hits for the row per swing whatever its swing time", "[ClassicSpellValue][ClassicCreatureMelee]")
{
    CHECK(CreatureClassicHit(101.0f, 0.0f, 1.0f, 2000.0f, 0.0f, 1.0f, 0.0f, 1.0f) == Catch::Approx(101.0f));
    CHECK(CreatureClassicHit(101.0f, 0.0f, 1.0f, 1150.0f, 0.0f, 1.0f, 0.0f, 1.0f) == Catch::Approx(101.0f));
    CHECK(CreatureClassicHit(1999.0f, 0.0f, 1.0f, 2800.0f, 0.0f, 1.0f, 0.0f, 1.0f) == Catch::Approx(1999.0f));
    // base percent, flat and total modifiers still apply
    CHECK(CreatureClassicHit(100.0f, 0.0f, 1.0f, 2000.0f, 10.0f, 0.5f, 5.0f, 2.0f) == Catch::Approx(120.0f));
    // attack power from auras: 140 attack power is 10 damage per second, a 2 second swing adds 20, a 1 second swing 10
    CHECK(CreatureClassicHit(100.0f, 140.0f, 1.0f, 2000.0f, 0.0f, 1.0f, 0.0f, 1.0f) == Catch::Approx(120.0f));
    CHECK(CreatureClassicHit(100.0f, 140.0f, 1.0f, 1000.0f, 0.0f, 1.0f, 0.0f, 1.0f) == Catch::Approx(110.0f));
}

TEST_CASE("A vanilla table coefficient is already level adjusted and is not penalised again", "[ClassicSpellValue]")
{
    // Fireball rank 1 (133, spell level 1): VMaNGOS effectBonusCoefficient 0.123 = the client's 0.429 x 0.2875. Penalising the
    // table value a second time gave 0.035.
    Value const fireball1 = { SchoolDamage, 0, 13, 9, 0.6f, 0, 0.0f, 0.123f };
    CHECK(BonusCoefficientWithLevelPenalty(&fireball1, 0.429f, 1) == Catch::Approx(0.123f));
    // a row without a coefficient leaves the client's value, which takes the penalty once
    Value const noCoefficient = { SchoolDamage, 0, 13, 9, 0.6f };
    CHECK(BonusCoefficientWithLevelPenalty(&noCoefficient, 0.429f, 1) == Catch::Approx(0.429f * 0.2875f));
    CHECK(BonusCoefficientWithLevelPenalty(nullptr, 0.429f, 1) == Catch::Approx(0.429f * 0.2875f));
}

TEST_CASE("The level penalty only touches spells learned below level 20", "[ClassicSpellValue]")
{
    CHECK(LevelPenalty(1) == Catch::Approx(0.2875f));
    CHECK(LevelPenalty(10) == Catch::Approx(0.625f));
    CHECK(LevelPenalty(19) == Catch::Approx(0.9625f));
    CHECK(LevelPenalty(20) == 1.0f);
    CHECK(LevelPenalty(60) == 1.0f);
    // creature spells and spells without a level are not penalised
    CHECK(LevelPenalty(0) == 1.0f);
    // Flash of Light rank 1 (19750, level 20) keeps the client's 0.429 whole
    CHECK(BonusCoefficientWithLevelPenalty(nullptr, 0.429f, 20) == Catch::Approx(0.429f));
}
