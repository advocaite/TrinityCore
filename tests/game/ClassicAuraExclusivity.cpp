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
#include "DummyData.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <memory>
#include <tuple>

void UnitTestDataLoader::InitializeSpellPositivity(SpellInfo& spellInfo)
{
    spellInfo._InitializeSpellPositivity();
}

namespace
{
    // One Classic spell with its 70532 family, word 0 of the class mask, dispel type and a single aura effect.
    struct ClassicSpell
    {
        SpellNameEntry Name{};
        SpellMiscEntry Misc{};
        SpellClassOptionsEntry ClassOptions{};
        SpellCategoriesEntry Categories{};
        SpellEffectEntry Effect{};
        std::unique_ptr<SpellInfo> Info;

        ClassicSpell(uint32 id, uint32 family, uint32 mask0, AuraType aura, Targets target, int8 dispel = 0)
        {
            Name.ID = id;
            ClassOptions.SpellClassSet = family;
            ClassOptions.SpellClassMask = flag128(mask0, 0, 0, 0);
            Categories.DispelType = dispel;
            Effect.Effect = SPELL_EFFECT_APPLY_AURA;
            Effect.EffectAura = aura;
            Effect.ImplicitTarget[0] = target;
            SpellInfoLoadHelper load;
            load.Misc = &Misc;
            load.ClassOptions = &ClassOptions;
            load.Categories = &Categories;
            load.Effects[0] = &Effect;
            Info = std::make_unique<SpellInfo>(&Name, DIFFICULTY_NONE, load);
            UnitTestDataLoader::InitializeSpellPositivity(*Info);
            UnitTestDataLoader::InitializeSpellSpecificInfo(*Info);
        }
    };

    ClassicSpell Blessing(uint32 id, uint32 mask0) { return { id, SPELLFAMILY_PALADIN, mask0, SPELL_AURA_MOD_STAT, TARGET_UNIT_TARGET_ALLY }; }
}

TEST_CASE("Every Classic blessing is one per paladin on a target", "[ClassicAuraExclusivity]")
{
    // 70532 SpellClassOptions word 0 (the vanilla masks)
    std::pair<uint32, uint32> const blessings[] =
    {
        { 19740, 0x10000002 }, // Blessing of Might
        { 25782, 0x10000002 }, // Greater Blessing of Might
        { 19742, 0x10010000 }, // Blessing of Wisdom
        { 20217, 0x11000000 }, // Blessing of Kings
        { 19977, 0x10000000 }, // Blessing of Light
        { 1038,  0x10000100 }, // Salvation
        { 1022,  0x10000080 }, // Blessing of Protection
        { 1044,  0x10000010 }, // Blessing of Freedom
        { 6940,  0x10000000 }, // Blessing of Sacrifice
    };
    for (auto const& [leftId, leftMask] : blessings)
    {
        INFO("Spell: " << leftId);
        ClassicSpell left = Blessing(leftId, leftMask);
        CHECK(left.Info->GetSpellSpecific() == SPELL_SPECIFIC_BLESSING);
        for (auto const& [rightId, rightMask] : blessings)
            CHECK(left.Info->IsAuraExclusiveBySpecificPerCasterWith(Blessing(rightId, rightMask).Info.get()));
        // per paladin only: two paladins' blessings are not exclusive by this rule
        CHECK_FALSE(left.Info->IsAuraExclusiveBySpecificWith(Blessing(19742, 0x10010000).Info.get()));
    }
}

TEST_CASE("Every Classic judgement debuff is one per paladin on a target", "[ClassicAuraExclusivity]")
{
    // 70532 word 0 and aura of each judgement debuff
    std::tuple<uint32, uint32, AuraType> const judgements[] =
    {
        { 20185, 0x00080000, SPELL_AURA_PROC_TRIGGER_SPELL }, // Judgement of Light
        { 20186, 0x00080000, SPELL_AURA_PROC_TRIGGER_SPELL }, // Judgement of Wisdom
        { 20184, 0x00100000, SPELL_AURA_PREVENTS_FLEEING },   // Judgement of Justice
        { 20188, 0x20000000, SPELL_AURA_MOD_DAMAGE_TAKEN },   // Judgement of the Crusader
    };
    for (auto const& [leftId, leftMask, leftAura] : judgements)
    {
        INFO("Spell: " << leftId);
        ClassicSpell left(leftId, SPELLFAMILY_PALADIN, leftMask, leftAura, TARGET_UNIT_TARGET_ENEMY);
        CHECK(left.Info->GetSpellSpecific() == SPELL_SPECIFIC_JUDGEMENT);
        for (auto const& [rightId, rightMask, rightAura] : judgements)
            CHECK(left.Info->IsAuraExclusiveBySpecificPerCasterWith(ClassicSpell(rightId, SPELLFAMILY_PALADIN, rightMask, rightAura, TARGET_UNIT_TARGET_ENEMY).Info.get()));
    }
    // the caster's own proc auras carry the same bits and are not judgements
    CHECK(ClassicSpell(25752, SPELLFAMILY_PALADIN, 0x00080000, SPELL_AURA_PROC_TRIGGER_SPELL, TARGET_UNIT_CASTER).Info->GetSpellSpecific() != SPELL_SPECIFIC_JUDGEMENT);
    CHECK(ClassicSpell(25942, SPELLFAMILY_PALADIN, 0x20000000, SPELL_AURA_PROC_TRIGGER_SPELL, TARGET_UNIT_CASTER).Info->GetSpellSpecific() != SPELL_SPECIFIC_JUDGEMENT);
}

TEST_CASE("Curse of Doom and Curse of Agony rank 1 are curses", "[ClassicAuraExclusivity]")
{
    ClassicSpell doom(603, SPELLFAMILY_WARLOCK, 0x80000000, SPELL_AURA_PERIODIC_DAMAGE, TARGET_UNIT_TARGET_ENEMY, DISPEL_CURSE);
    ClassicSpell agony(980, SPELLFAMILY_WARLOCK, 0x00000400, SPELL_AURA_PERIODIC_DAMAGE, TARGET_UNIT_TARGET_ENEMY, DISPEL_CURSE);
    ClassicSpell weakness(702, SPELLFAMILY_WARLOCK, 0x00008000, SPELL_AURA_MOD_DAMAGE_DONE, TARGET_UNIT_TARGET_ENEMY, DISPEL_CURSE);
    CHECK(doom.Info->GetSpellSpecific() == SPELL_SPECIFIC_CURSE);
    CHECK(agony.Info->GetSpellSpecific() == SPELL_SPECIFIC_CURSE);
    CHECK(doom.Info->IsAuraExclusiveBySpecificPerCasterWith(weakness.Info.get()));
    CHECK(agony.Info->IsAuraExclusiveBySpecificPerCasterWith(weakness.Info.get()));
}

TEST_CASE("Only intellect buffs share Arcane Intellect's group", "[ClassicAuraExclusivity]")
{
    CHECK(ClassicSpell(1459, SPELLFAMILY_MAGE, 0x400, SPELL_AURA_MOD_STAT, TARGET_UNIT_TARGET_ALLY).Info->GetSpellSpecific() == SPELL_SPECIFIC_MAGE_ARCANE_BRILLANCE);
    CHECK(ClassicSpell(17150, SPELLFAMILY_MAGE, 0x400, SPELL_AURA_MOD_DAMAGE_DONE, TARGET_UNIT_TARGET_ALLY).Info->GetSpellSpecific() != SPELL_SPECIFIC_MAGE_ARCANE_BRILLANCE); // Arcane Might
}

TEST_CASE("Only one form of tracking is active at a time", "[ClassicAuraExclusivity]")
{
    ClassicSpell herbs(2383, SPELLFAMILY_GENERIC, 0, SPELL_AURA_TRACK_RESOURCES, TARGET_UNIT_CASTER);
    ClassicSpell minerals(2580, SPELLFAMILY_GENERIC, 0, SPELL_AURA_TRACK_RESOURCES, TARGET_UNIT_CASTER);
    ClassicSpell beasts(1494, SPELLFAMILY_HUNTER, 0, SPELL_AURA_TRACK_CREATURES, TARGET_UNIT_CASTER);
    CHECK(herbs.Info->GetSpellSpecific() == SPELL_SPECIFIC_TRACKER);
    CHECK(herbs.Info->IsAuraExclusiveBySpecificWith(minerals.Info.get()));
    CHECK(herbs.Info->IsAuraExclusiveBySpecificWith(beasts.Info.get()));
}
