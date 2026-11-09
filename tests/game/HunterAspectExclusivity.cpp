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

namespace
{
    // Hunter aspects with their 70532 SpellClassOptions family flags.
    struct AspectData
    {
        uint32 Id;
        flag128 Flags;
    };

    AspectData const Aspects[] =
    {
        { 5118,    flag128(0x00200000, 0, 0, 0) }, // Aspect of the Cheetah
        { 13159,   flag128(0x00200000, 0, 0, 0) }, // Aspect of the Pack
        { 13161,   flag128(0, 0, 0x00000010, 0) }, // Aspect of the Beast
        { 13163,   flag128(0x00080000, 0, 0, 0) }, // Aspect of the Monkey
        { 13165,   flag128(0x00100000, 0, 0, 0) }, // Aspect of the Hawk, rank 1
        { 25296,   flag128(0x00100000, 0, 0, 0) }, // Aspect of the Hawk, rank 7
        { 20043,   flag128(0, 0, 0, 0) },          // Aspect of the Wild, rank 1
        { 20190,   flag128(0, 0, 0, 0) },          // Aspect of the Wild, rank 2
        { 34074,   flag128(0, 0x00040000, 0, 0) }, // Aspect of the Viper
        { 469145,  flag128(0, 0, 0, 0) },          // Aspect of the Falcon
    };

    struct HunterSpell
    {
        SpellNameEntry Name{};
        SpellMiscEntry Misc{};
        SpellClassOptionsEntry ClassOptions{};
        SpellEffectEntry Effect{};
        std::unique_ptr<SpellInfo> Info;

        HunterSpell(uint32 id, flag128 flags)
        {
            Name.ID = id;
            ClassOptions.SpellClassSet = SPELLFAMILY_HUNTER;
            ClassOptions.SpellClassMask = flags;
            Effect.Effect = SPELL_EFFECT_APPLY_AURA;
            Effect.EffectAura = SPELL_AURA_MOD_RANGED_ATTACK_POWER;
            Effect.ImplicitTarget[0] = TARGET_UNIT_CASTER;
            SpellInfoLoadHelper load;
            load.Misc = &Misc;
            load.ClassOptions = &ClassOptions;
            load.Effects[0] = &Effect;
            Info = std::make_unique<SpellInfo>(&Name, DIFFICULTY_NONE, load);
            UnitTestDataLoader::InitializeSpellSpecificInfo(*Info);
        }
    };
}

TEST_CASE("Every Classic hunter aspect is in the aspect group", "[HunterAspects]")
{
    for (AspectData const& aspect : Aspects)
    {
        INFO("Spell: " << aspect.Id);
        HunterSpell spell(aspect.Id, aspect.Flags);
        CHECK(spell.Info->GetSpellSpecific() == SPELL_SPECIFIC_ASPECT);
    }
}

TEST_CASE("A hunter has only one aspect of their own at a time", "[HunterAspects]")
{
    for (AspectData const& left : Aspects)
        for (AspectData const& right : Aspects)
        {
            INFO("Aspect pair: " << left.Id << ", " << right.Id);
            HunterSpell leftSpell(left.Id, left.Flags), rightSpell(right.Id, right.Flags);
            CHECK(leftSpell.Info->IsAuraExclusiveBySpecificPerCasterWith(rightSpell.Info.get()));
        }
}

TEST_CASE("Other hunter spells are not aspects", "[HunterAspects]")
{
    CHECK(HunterSpell(19552, flag128()).Info->GetSpellSpecific() != SPELL_SPECIFIC_ASPECT);                   // Improved Aspect of the Monkey (talent)
    CHECK(HunterSpell(1978, flag128(0x00004000, 0, 0, 0)).Info->GetSpellSpecific() != SPELL_SPECIFIC_ASPECT); // Serpent Sting
}
