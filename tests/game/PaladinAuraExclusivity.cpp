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
#include "Creature.h"
#include "DummyData.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <memory>

void UnitTestDataLoader::InitializeSpellSpecificInfo(SpellInfo& spellInfo)
{
    spellInfo._LoadSpellSpecific();
}

namespace
{
    constexpr uint32 AuraRanks[] = { 465, 10290, 643, 10291, 1032, 10292, 10293, 20218,
        7294, 10298, 10299, 10300, 10301, 19746,
        19876, 19895, 19896, 19888, 19897, 19898, 19891, 19899, 19900 };

    // Minimal native active-aura shape: Paladin family, native attribute and
    // party/raid area effect. Values and effect execution are outside this test.
    struct AuraSpell
    {
        SpellNameEntry Name{};
        SpellMiscEntry Misc{};
        SpellClassOptionsEntry ClassOptions{};
        SpellEffectEntry Effect{};
        std::unique_ptr<SpellInfo> Info;

        explicit AuraSpell(uint32 id, uint32 effect = SPELL_EFFECT_APPLY_AREA_AURA_PARTY,
            bool native = true, uint32 family = SPELLFAMILY_PALADIN)
        {
            Name.ID = id;
            Misc.Attributes[15] = native ? SPELL_ATTR15_UNK13 : 0;
            ClassOptions.SpellClassSet = family;
            Effect.Effect = effect;
            Effect.EffectAura = SPELL_AURA_MOD_RESISTANCE;
            Effect.ImplicitTarget[0] = TARGET_UNIT_CASTER;
            SpellInfoLoadHelper load;
            load.Misc = &Misc;
            load.ClassOptions = &ClassOptions;
            load.Effects[0] = &Effect;
            Info = std::make_unique<SpellInfo>(&Name, DIFFICULTY_NONE, load);
            UnitTestDataLoader::InitializeSpellSpecificInfo(*Info);
        }
    };

    struct DetachedAura final : Aura
    {
        using Aura::Aura;
        void Remove(AuraRemoveMode mode) override { _Remove(mode); }
        void FillTargetMap(std::unordered_map<Unit*, uint32>&, Unit*) override { }
    };
}

TEST_CASE("All native Paladin aura ranks use the existing aura exclusivity category", "[PaladinAuras]")
{
    for (uint32 id : AuraRanks)
    {
        INFO("Spell: " << id);
        CHECK(AuraSpell(id).Info->GetSpellSpecific() == SPELL_SPECIFIC_AURA);
        CHECK(AuraSpell(id, SPELL_EFFECT_APPLY_AREA_AURA_RAID).Info->GetSpellSpecific() == SPELL_SPECIFIC_AURA);
    }
}

TEST_CASE("Native Paladin aura recognition does not classify unrelated spells", "[PaladinAuras]")
{
    CHECK(AuraSpell(10290, SPELL_EFFECT_APPLY_AREA_AURA_PARTY, false).Info->GetSpellSpecific() != SPELL_SPECIFIC_AURA);
    CHECK(AuraSpell(10290, SPELL_EFFECT_APPLY_AURA).Info->GetSpellSpecific() != SPELL_SPECIFIC_AURA);
    CHECK(AuraSpell(10290, SPELL_EFFECT_APPLY_AREA_AURA_PARTY, true, SPELLFAMILY_MAGE).Info->GetSpellSpecific() != SPELL_SPECIFIC_AURA);
    CHECK(AuraSpell(20138).Info->GetSpellSpecific() != SPELL_SPECIFIC_AURA); // Talent
    CHECK(AuraSpell(21084).Info->GetSpellSpecific() == SPELL_SPECIFIC_SEAL);
    for (uint32 id : { 465u, 32223u, 183435u, 317920u })
        CHECK(AuraSpell(id, SPELL_EFFECT_APPLY_AURA, false).Info->GetSpellSpecific() == SPELL_SPECIFIC_AURA);
}

TEST_CASE("Paladin aura exclusivity only rejects auras from the same caster", "[PaladinAuras]")
{
    Creature owner(false);
    ObjectGuid caster = ObjectGuid::Create<HighGuid::Player>(99911);
    ObjectGuid otherCaster = ObjectGuid::Create<HighGuid::Player>(99912);
    for (uint32 leftId : AuraRanks)
        for (uint32 rightId : AuraRanks)
        {
            INFO("Aura pair: " << leftId << ", " << rightId);
            AuraSpell leftSpell(leftId), rightSpell(rightId);
            DetachedAura left(AuraCreateInfo(ObjectGuid::Empty, leftSpell.Info.get(), DIFFICULTY_NONE, 1, &owner).SetCasterGUID(caster));
            DetachedAura sameCaster(AuraCreateInfo(ObjectGuid::Empty, rightSpell.Info.get(), DIFFICULTY_NONE, 1, &owner).SetCasterGUID(caster));
            DetachedAura differentCaster(AuraCreateInfo(ObjectGuid::Empty, rightSpell.Info.get(), DIFFICULTY_NONE, 1, &owner).SetCasterGUID(otherCaster));
            CHECK_FALSE(left.CanStackWith(&sameCaster));
            // Different spells from different Paladins remain compatible.
            if (leftId != rightId)
                CHECK(left.CanStackWith(&differentCaster));
        }
}
