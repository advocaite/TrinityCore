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
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <memory>

namespace
{
    struct NativeSniper
    {
        SpellNameEntry Name{};
        SpellMiscEntry Misc{};
        SpellAuraOptionsEntry Options{};
        SpellClassOptionsEntry Family{};
        SpellEffectEntry Effects[2]{};
        SpellDurationEntry Duration{};
        std::unique_ptr<SpellInfo> Info;

        explicit NativeSniper(uint32 id = 1310785, uint8 charges = 3, AuraType modifier = SPELL_AURA_ADD_FLAT_MODIFIER)
        {
            // Fresh70170 accessible rows: Effect1358439, Options248466,
            // Misc857374, Duration1. Family84345 is the loaded70124 record;
            // its current70170 parity is recorded separately from gameplay.
            Name.ID = Misc.SpellID = Options.SpellID = Family.SpellID = id;
            Misc.Attributes[0] = 4259858;
            Misc.Attributes[2] = 131072;
            Misc.Attributes[4] = 402653184;
            Misc.Attributes[15] = 8192;
            Misc.RangeIndex = 582;
            Misc.DurationIndex = 1;
            Misc.SchoolMask = SPELL_SCHOOL_MASK_NORMAL;
            Options.ID = 248466;
            Options.ProcChance = 100;
            Options.ProcCharges = charges;
            Options.ProcTypeMask[0] = 320;
            Family.ID = 84345;
            Family.ModalNextSpell = 75;
            Family.SpellClassSet = SPELLFAMILY_HUNTER;
            Family.SpellClassMask[3] = 8388608;
            Effects[0].ID = 1340617;
            Effects[0].SpellID = id;
            Effects[0].Effect = SPELL_EFFECT_NORMALIZED_WEAPON_DMG;
            Effects[0].EffectBasePoints = 225;
            Effects[0].EffectBonusCoefficient = 1;
            Effects[1].ID = 1358439;
            Effects[1].SpellID = id;
            Effects[1].EffectIndex = 1;
            Effects[1].Effect = SPELL_EFFECT_APPLY_AURA;
            Effects[1].EffectAura = modifier;
            Effects[1].EffectBasePoints = 10;
            Effects[1].EffectMiscValue[0] = int32(SpellModOp::Range);
            Effects[1].EffectSpellClassMask[0] = 399873;
            Effects[1].ImplicitTarget[0] = TARGET_UNIT_CASTER;
            Duration.ID = 1;
            Duration.Duration = Duration.MaxDuration = 10000;
            Build();
        }

        void Build()
        {
            SpellInfoLoadHelper load;
            load.Misc = &Misc;
            load.AuraOptions = &Options;
            load.ClassOptions = &Family;
            load.Effects[0] = &Effects[0];
            load.Effects[1] = &Effects[1];
            Info = std::make_unique<SpellInfo>(&Name, DIFFICULTY_NONE, load);
            Info->DurationEntry = &Duration;
        }
    };

}

TEST_CASE("Native charged spell modifiers get default consumption rules", "[NativeChargedSpellmods]")
{
    for (AuraType type : { SPELL_AURA_ADD_FLAT_MODIFIER, SPELL_AURA_ADD_PCT_MODIFIER })
    {
        NativeSniper native(1310785, 3, type);
        auto entry = SpellMgr::GenerateDefaultSpellProcEntry(*native.Info);
        REQUIRE(entry.has_value());
        CHECK(entry->Charges == 3);
        CHECK(entry->Chance == 100);
        CHECK(entry->ProcFlags[0] == 320);
        CHECK(entry->ProcFlags[1] == 0);
        CHECK(entry->SpellFamilyName == SPELLFAMILY_HUNTER);
        CHECK(entry->SpellFamilyMask.IsEqual(399873, 0, 0, 0));
        CHECK(entry->SpellPhaseMask == PROC_SPELL_PHASE_HIT);
        CHECK(entry->AttributesMask & PROC_ATTR_REQ_SPELLMOD);
        CHECK((entry->DisableEffectsMask & 2) == 0);
    }
}

TEST_CASE("Unmarked or uncharged spell modifiers retain explicit-data requirement", "[NativeChargedSpellmods]")
{
    NativeSniper uncharged(1310785, 0);
    CHECK_FALSE(SpellMgr::GenerateDefaultSpellProcEntry(*uncharged.Info).has_value());
    NativeSniper unmarked;
    unmarked.Misc.Attributes[15] = 0;
    unmarked.Build();
    CHECK_FALSE(SpellMgr::GenerateDefaultSpellProcEntry(*unmarked.Info).has_value());
    NativeSniper noFlags;
    noFlags.Options.ProcTypeMask[0] = 0;
    noFlags.Build();
    CHECK_FALSE(SpellMgr::GenerateDefaultSpellProcEntry(*noFlags.Info).has_value());
}

TEST_CASE("Mixed trigger and modifier auras preserve their existing proc rules", "[NativeChargedSpellmods]")
{
    NativeSniper mixed;
    mixed.Effects[0].Effect = SPELL_EFFECT_APPLY_AURA;
    mixed.Effects[0].EffectAura = SPELL_AURA_PROC_TRIGGER_SPELL;
    mixed.Effects[0].EffectTriggerSpell = 75;
    mixed.Effects[0].EffectSpellClassMask[0] = 1;
    mixed.Build();
    auto entry = SpellMgr::GenerateDefaultSpellProcEntry(*mixed.Info);
    REQUIRE(entry.has_value());
    CHECK_FALSE(entry->AttributesMask & PROC_ATTR_REQ_SPELLMOD);
    CHECK(entry->SpellFamilyMask.IsEqual(1, 0, 0, 0));
    CHECK(entry->DisableEffectsMask & 2);
}

TEST_CASE("Default proc generator preserves legacy hit restrictions", "[NativeChargedSpellmods]")
{
    for (auto [type, mask] : { std::pair{SPELL_AURA_REFLECT_SPELLS, PROC_HIT_REFLECT},
        std::pair{SPELL_AURA_MOD_WEAPON_CRIT_PERCENT, PROC_HIT_CRITICAL},
        std::pair{SPELL_AURA_MOD_BLOCK_PERCENT, PROC_HIT_BLOCK} })
    {
        NativeSniper native(90000001, 2, type);
        auto entry = SpellMgr::GenerateDefaultSpellProcEntry(*native.Info);
        REQUIRE(entry.has_value());
        CHECK(entry->HitMask == mask);
        CHECK_FALSE(entry->AttributesMask & PROC_ATTR_REQ_SPELLMOD);
        CHECK(entry->Charges == 2);
    }
}

TEST_CASE("Default proc generator preserves phase cooldown and loop protections", "[NativeChargedSpellmods]")
{
    NativeSniper native(90000001, 2, SPELL_AURA_PROC_TRIGGER_SPELL);
    native.Options.ProcTypeMask[0] = 0;
    native.Options.ProcTypeMask[1] = PROC_FLAG_2_CAST_SUCCESSFUL;
    native.Build();
    native.Info->ProcCooldown = 1000;
    auto entry = SpellMgr::GenerateDefaultSpellProcEntry(*native.Info);
    REQUIRE(entry.has_value());
    CHECK(entry->SpellPhaseMask == PROC_SPELL_PHASE_CAST);
    CHECK(entry->Cooldown == 1000ms);

    native.Options.ProcTypeMask[0] = PROC_FLAG_DEAL_HARMFUL_SPELL;
    native.Options.ProcTypeMask[1] = 0;
    native.Options.ProcCharges = 0;
    native.Misc.Attributes[3] = SPELL_ATTR3_CAN_PROC_FROM_PROCS;
    native.Effects[1].EffectSpellClassMask[0] = 0;
    native.Effects[1].EffectTriggerSpell = 75;
    native.Build();
    native.Info->ProcCooldown = 0;
    CHECK_FALSE(SpellMgr::GenerateDefaultSpellProcEntry(*native.Info).has_value());
}
