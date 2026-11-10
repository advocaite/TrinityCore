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

// Classic 1.60 (WoW Forever) warlock and druid talents and abilities whose client spell is only a dummy aura or dummy effect.
// Talent ranks come from TraitDefinitionEffectPoints curves: the core puts the rank value into the aura amount (SpellEffectInfo::CalcValue),
// so the scripts read aurEff->GetAmount() and never hardcode rank values. Tooltip formulas quoted below are the raw client Spell.db2 ones.

#include "ScriptMgr.h"
#include "GameTime.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "Random.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "Unit.h"
#include <mutex>
#include <unordered_map>

enum ClassicWarlockDruidTalentSpells
{
    // warlock
    SPELL_CWD_LIFE_TAP_ENERGIZE             = 31818,    // energize mana (bp = mana gained)
    SPELL_CWD_IMPROVED_LIFE_TAP             = 18182,
    SPELL_CWD_METAMORPHOSIS                 = 403816,   // effect 3: "increasing mana gained from Life Tap by $403816s4%"
    SPELL_CWD_DEMONIC_ENERGIES              = 1225214,
    SPELL_CWD_DEMONIC_ENERGIES_HEAL         = 1243028,  // heal, targets the caster's pet
    SPELL_CWD_IMPROVED_SHADOW_BOLT          = 17793,
    SPELL_CWD_SHADOW_VULNERABILITY          = 17794,    // SPELL_AURA_MOD_SCHOOL_MASK_DAMAGE_FROM_CASTER, shadow, 12 sec
    SPELL_CWD_SOUL_SIPHON                   = 17804,
    SPELL_CWD_NIGHTFALL                     = 18094,
    SPELL_CWD_PYROCLASM_STUN                = 18093,    // stun, 3 sec
    SPELL_CWD_SHADOW_TRANCE                 = 17941,    // next Shadow Bolt instant, 10 sec
    SPELL_CWD_MASTER_DEMONOLOGIST           = 23785,
    SPELL_CWD_MD_IMP                        = 23759,    // area aura pet: fire damage done
    SPELL_CWD_MD_VOIDWALKER                 = 23760,    // area aura pet: physical damage taken
    SPELL_CWD_MD_SUCCUBUS                   = 23761,    // area aura pet: shadow damage done
    SPELL_CWD_MD_FELHUNTER                  = 23762,    // area aura pet: magic damage taken
    SPELL_CWD_DEMONIC_KNOWLEDGE             = 412732,
    SPELL_CWD_DEMONIC_KNOWLEDGE_OWNER       = 1243120,  // area aura owner: spell damage + healing done (cast by the demon)
    SPELL_CWD_DEMONIC_KNOWLEDGE_PET         = 412735,   // spell damage + healing done on the demon itself
    SPELL_CWD_DEMONIC_PACT                  = 425464,
    SPELL_CWD_BURNING_SHADOW                = 18789,    // Demonic Sacrifice: Imp
    SPELL_CWD_FEL_STAMINA                   = 18790,    // Demonic Sacrifice: Voidwalker
    SPELL_CWD_TOUCH_OF_FIRE                 = 18791,    // Demonic Sacrifice: Succubus / Incubus
    SPELL_CWD_FEL_ENERGY                    = 18792,    // Demonic Sacrifice: Felhunter
    SPELL_CWD_SHADOW_AND_FLAME              = 426316,
    SPELL_CWD_SHADOW_AND_FLAME_SHADOW       = 1293816,  // "Shadow": shadow damage done, 20 sec
    SPELL_CWD_SHADOW_AND_FLAME_FIRE         = 426311,   // "Flame": fire damage done, 20 sec
    SPELL_CWD_IMMOLATE_MARKER               = 1282590,  // hidden Immolate aura required by Conflagrate (classic_spell_warl_immolate)
    SPELL_CWD_SOUL_HARVEST                  = 437032,
    SPELL_CWD_SOUL_HARVEST_BUFF             = 1242853,  // mana regen % + regen while casting, 10 sec
    SPELL_CWD_BANE_OF_HAVOC                 = 1225228,
    SPELL_CWD_BANE_OF_HAVOC_TRACKER         = 1243338,  // warlock side proc aura
    SPELL_CWD_BANE_OF_HAVOC_DAMAGE          = 1243339,
    SPELL_CWD_IMPROVED_HEALTHSTONE_R1       = 18692,
    SPELL_CWD_IMPROVED_HEALTHSTONE_R2       = 18693,
    SPELL_CWD_GRIMOIRE_OF_SYNERGY           = 426301,
    SPELL_CWD_GRIMOIRE_OF_SYNERGY_BUFF      = 426303,   // all damage done, 15 sec
    SPELL_CWD_DECIMATION                    = 440870,
    SPELL_CWD_DECIMATION_BUFF               = 440873,   // Soul Fire cast time (bp0) and no Soul Shard, 10 sec

    // druid
    SPELL_CWD_THICK_HIDE                    = 16929,
    SPELL_CWD_PREDATORY_STRIKES             = 16972,
    SPELL_CWD_THICK_HIDE_ARMOR              = 9970701,  // serverside: SPELL_AURA_MOD_BASE_RESISTANCE armor (bp = armor)
    SPELL_CWD_BLOOD_FRENZY                  = 16958,
    SPELL_CWD_BLOOD_FRENZY_RAGE             = 16959,
    SPELL_CWD_FUROR                         = 17056,
    SPELL_CWD_FUROR_RAGE                    = 17057,
    SPELL_CWD_ECLIPSE                       = 408248,
    SPELL_CWD_ECLIPSE_BUFF                  = 408255,
    SPELL_CWD_DREAMSTATE                    = 408258,
    SPELL_CWD_DREAMSTATE_BUFF               = 408261,
    SPELL_CWD_KING_OF_THE_JUNGLE            = 417046,
    SPELL_CWD_KING_OF_THE_JUNGLE_ENERGIZE   = 417045,
    SPELL_CWD_REND_AND_TEAR                 = 1223246,
    SPELL_CWD_LEADER_OF_THE_PACK            = 17007,
    SPELL_CWD_LEADER_OF_THE_PACK_AURA       = 24932,    // area aura party, 45 yd: +3% critical strike chance; Cat, Bear and Dire Bear Form only
    SPELL_CWD_HEART_OF_THE_WILD             = 17003,
    SPELL_CWD_HEART_OF_THE_WILD_BEAR        = 24899,    // total stat percentage (stamina), bp = amount
    SPELL_CWD_HEART_OF_THE_WILD_CAT         = 24900,    // total stat percentage (strength), bp = amount
    SPELL_CWD_FERAL_SWIFTNESS               = 17002,    // effect 0: dodge, effect 1: Cat Form movement speed (dummy)
    SPELL_CWD_FERAL_SWIFTNESS_CAT           = 24867,    // movement speed, bp = amount
    SPELL_CWD_NATURAL_REACTION_RAGE         = 417053,   // energize rage (rage x10)
    SPELL_CWD_OVERGROWTH                    = 17245,    // effect 0: extra Entangling Roots targets (dummy)
};

enum ClassicWarlockDruidTalentMisc
{
    SKILL_CWD_AFFLICTION                    = 355,
    ITEM_CWD_SOUL_SHARD                     = 6265,

    NPC_CWD_IMP                             = 416,
    NPC_CWD_FELHUNTER                       = 417,
    NPC_CWD_VOIDWALKER                      = 1860,
    NPC_CWD_SUCCUBUS                        = 1863,
    NPC_CWD_INCUBUS                         = 185317,
    NPC_CWD_FELGUARD                        = 213450,

    CWD_STARFIRE_FAMILY_FLAG_0              = 0x4,      // druid SpellFamilyFlags[0] bit of every Starfire rank
};

namespace ClassicTalentsWarlockDruid
{
    enum class DemonKind : uint8
    {
        None,
        Imp,
        Voidwalker,
        Succubus,
        Felhunter,
        Felguard
    };

    DemonKind GetDemonKind(uint32 entry)
    {
        switch (entry)
        {
            case NPC_CWD_IMP:        return DemonKind::Imp;
            case NPC_CWD_VOIDWALKER: return DemonKind::Voidwalker;
            case NPC_CWD_SUCCUBUS:
            case NPC_CWD_INCUBUS:    return DemonKind::Succubus;
            case NPC_CWD_FELHUNTER:  return DemonKind::Felhunter;
            case NPC_CWD_FELGUARD:   return DemonKind::Felguard;
            default:                 return DemonKind::None;
        }
    }

    // Demonic Sacrifice buff of each demon (VMaNGOS WarlockDemonicSacrificeScript)
    uint32 GetSacrificeBuff(DemonKind kind)
    {
        switch (kind)
        {
            case DemonKind::Imp:        return SPELL_CWD_BURNING_SHADOW;
            case DemonKind::Voidwalker: return SPELL_CWD_FEL_STAMINA;
            case DemonKind::Succubus:   return SPELL_CWD_TOUCH_OF_FIRE;
            case DemonKind::Felhunter:  return SPELL_CWD_FEL_ENERGY;
            default:                    return 0;
        }
    }

    Pet* GetActiveDemon(Player* player, DemonKind* kind = nullptr)
    {
        Pet* pet = player->GetPet();
        if (!pet || !pet->IsInWorld() || !pet->IsAlive())
            return nullptr;

        DemonKind demon = GetDemonKind(pet->GetEntry());
        if (demon == DemonKind::None)
            return nullptr;

        if (kind)
            *kind = demon;
        return pet;
    }

    // Applies an aura cast by the demon on itself (area auras reach the warlock) and forces its effect amounts
    // (the helper spells carry the base rank values or per level points, the talent amounts come from the trait curves)
    void SetDemonAura(Unit* caster, Unit* target, uint32 spellId, std::initializer_list<std::pair<SpellEffIndex, SpellEffectValue>> amounts)
    {
        Aura* aura = target->GetAura(spellId, caster->GetGUID());
        if (!aura)
        {
            caster->CastSpell(target, spellId, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
            aura = target->GetAura(spellId, caster->GetGUID());
            if (!aura)
                return;
        }

        for (auto const& [effIndex, amount] : amounts)
            if (AuraEffect* aurEff = aura->GetEffect(effIndex))
                if (aurEff->GetAmount() != amount)
                    aurEff->ChangeAmount(amount);
    }

    // 23785 - Master Demonologist: "Grants both the Warlock and the summoned demon an effect as long as that demon is active.
    // Imp - Increases Fire damage done by $m1%. Voidwalker - Reduces Physical damage taken by $m2%. Succubus/Incubus - Increases Shadow
    // damage done by $m3%. Felhunter - Reduces Magic damage taken by $m4%." The vanilla area auras 23759..23762 (cast by the demon, area
    // aura pet: the demon and its owner) carry the effect.
    void SyncMasterDemonologist(Player* player, Pet* pet, DemonKind kind)
    {
        struct DemonAura { DemonKind Kind; uint32 SpellId; SpellEffIndex TalentEffect; float Sign; };
        static DemonAura const demonAuras[] =
        {
            { DemonKind::Imp,        SPELL_CWD_MD_IMP,        EFFECT_0,  1.0f },
            { DemonKind::Voidwalker, SPELL_CWD_MD_VOIDWALKER, EFFECT_1, -1.0f },
            { DemonKind::Succubus,   SPELL_CWD_MD_SUCCUBUS,   EFFECT_2,  1.0f },
            { DemonKind::Felhunter,  SPELL_CWD_MD_FELHUNTER,  EFFECT_3, -1.0f },
        };

        for (DemonAura const& demonAura : demonAuras)
        {
            SpellEffectValue amount = 0.0;
            if (demonAura.Kind == kind)
                if (AuraEffect const* talent = player->GetAuraEffect(SPELL_CWD_MASTER_DEMONOLOGIST, demonAura.TalentEffect))
                    amount = talent->GetAmount() * demonAura.Sign;

            if (amount == 0.0)
            {
                pet->RemoveAurasDueToSpell(demonAura.SpellId, pet->GetGUID());
                continue;
            }

            SetDemonAura(pet, pet, demonAura.SpellId, { { EFFECT_0, amount } });
        }
    }

    // 412732 - Demonic Knowledge: "Increases your spell damage and your Demon pet's spell damage by up to $s1% of your level while you have
    // a summoned Demon pet active." 1243120 (cast by the demon, area aura owner) buffs the warlock, 412735 the demon.
    void SyncDemonicKnowledge(Player* player, Pet* pet)
    {
        SpellEffectValue bonus = 0.0;
        if (AuraEffect const* talent = player->GetAuraEffect(SPELL_CWD_DEMONIC_KNOWLEDGE, EFFECT_0))
            bonus = std::floor(player->GetLevel() * talent->GetAmount() / 100.0);

        if (bonus <= 0.0)
        {
            pet->RemoveAurasDueToSpell(SPELL_CWD_DEMONIC_KNOWLEDGE_OWNER, pet->GetGUID());
            pet->RemoveAurasDueToSpell(SPELL_CWD_DEMONIC_KNOWLEDGE_PET, pet->GetGUID());
            return;
        }

        SetDemonAura(pet, pet, SPELL_CWD_DEMONIC_KNOWLEDGE_OWNER, { { EFFECT_0, bonus }, { EFFECT_1, bonus } });

        bool const applied = pet->HasAura(SPELL_CWD_DEMONIC_KNOWLEDGE_PET, pet->GetGUID());
        SetDemonAura(pet, pet, SPELL_CWD_DEMONIC_KNOWLEDGE_PET, { { EFFECT_0, bonus }, { EFFECT_1, bonus } });
        if (!applied)
        {
            // the client spell lasts 15 sec, the talent lasts as long as the demon
            if (Aura* aura = pet->GetAura(SPELL_CWD_DEMONIC_KNOWLEDGE_PET, pet->GetGUID()))
            {
                aura->SetMaxDuration(-1);
                aura->SetDuration(-1);
            }
        }
    }

    void SyncDemonTalents(Player* player)
    {
        if (!player->IsInWorld())
            return;

        DemonKind kind = DemonKind::None;
        Pet* pet = GetActiveDemon(player, &kind);
        if (!pet)
            return;

        SyncMasterDemonologist(player, pet, kind);
        SyncDemonicKnowledge(player, pet);
    }

    // aura hooks (heartbeat, apply/remove) run while the unit iterates or changes its auras: apply the demon auras on the next update
    void ScheduleDemonTalentSync(Unit* unit, Milliseconds delay = 0ms)
    {
        Player* player = unit ? unit->ToPlayer() : nullptr;
        if (!player || !player->IsInWorld())
            return;

        player->m_Events.AddEventAtOffset([player]() { SyncDemonTalents(player); }, delay);
    }

    bool IsAfflictionSpell(uint32 spellId)
    {
        SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
            if (itr->second->SkillLine == SKILL_CWD_AFFLICTION)
                return true;
        return false;
    }

    bool IsThickHideForm(ShapeshiftForm form)
    {
        return form == FORM_CAT_FORM || form == FORM_BEAR_FORM || form == FORM_DIRE_BEAR_FORM || form == FORM_MOONKIN_FORM;
    }

    // 16929 - Thick Hide: "While in Bear Form, Cat Form, Dire Bear Form, or Moonkin Form, you gain $s2 additional base Armor per level and
    // another ${$m1/100}.2 base Armor for each point of defense skill beyond five times your level. This amount can be further increased
    // by multipliers from those forms." The serverside helper adds it as base armor (SPELL_AURA_MOD_BASE_RESISTANCE: the form armor
    // multipliers apply on top).
    void UpdateThickHide(Unit* druid)
    {
        AuraEffect const* perDefense = druid->GetAuraEffect(SPELL_CWD_THICK_HIDE, EFFECT_0);
        AuraEffect const* perLevel = druid->GetAuraEffect(SPELL_CWD_THICK_HIDE, EFFECT_1);
        if (!perDefense || !perLevel || !IsThickHideForm(druid->GetShapeshiftForm()))
        {
            druid->RemoveAurasDueToSpell(SPELL_CWD_THICK_HIDE_ARMOR);
            return;
        }

        int32 const level = int32(druid->GetLevel());
        // the client data value is per level (3, the Wowhead tooltip shows the level 60 total 180); a curve value above 10 is already a total
        SpellEffectValue armor = perLevel->GetAmount() <= 10.0 ? perLevel->GetAmount() * level : perLevel->GetAmount();
        if (Player* player = druid->ToPlayer())
        {
            int32 const extraDefense = int32(player->GetSkillValue(SKILL_DEFENSE)) - level * 5;
            if (extraDefense > 0)
                armor += extraDefense * perDefense->GetAmount() / 100.0;
        }

        armor = std::floor(armor);
        if (armor <= 0.0)
        {
            druid->RemoveAurasDueToSpell(SPELL_CWD_THICK_HIDE_ARMOR);
            return;
        }

        if (AuraEffect* current = druid->GetAuraEffect(SPELL_CWD_THICK_HIDE_ARMOR, EFFECT_0))
        {
            if (current->GetAmount() != armor)
                current->ChangeAmount(armor);
            return;
        }

        druid->CastSpell(druid, SPELL_CWD_THICK_HIDE_ARMOR, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(armor));
    }

    // 16972 - Predatory Strikes: "Increases your melee attack power in Cat, Bear and Dire Bear Form by $s1% of your level." The client
    // aura has the percent on effect 0 and an attack power effect 1 with no value; the attack power is filled in here, for the forms only.
    void UpdatePredatoryStrikes(Unit* druid, bool leavingForm = false)
    {
        AuraEffect* percent = druid->GetAuraEffect(SPELL_CWD_PREDATORY_STRIKES, EFFECT_0);
        AuraEffect* attackPower = druid->GetAuraEffect(SPELL_CWD_PREDATORY_STRIKES, EFFECT_1);
        if (!percent || !attackPower)
            return;

        ShapeshiftForm const form = druid->GetShapeshiftForm();
        bool const feral = !leavingForm && (form == FORM_CAT_FORM || form == FORM_BEAR_FORM || form == FORM_DIRE_BEAR_FORM);
        SpellEffectValue const amount = feral ? std::floor(percent->GetAmount() * druid->GetLevel() / 100.0) : 0.0;
        if (attackPower->GetAmount() != amount)
            attackPower->ChangeAmount(amount);
    }
}

using namespace ClassicTalentsWarlockDruid;

/*######
## Warlock
######*/

// 1454, 1455, 1456, 11687, 11688, 11689 - Life Tap (client: dummy effect, nothing happened)
// Vanilla (VMaNGOS WarlockLifeTapScript): converts health into the same amount of mana. The health cost is the effect value plus 80% of
// the warlock's shadow spell damage (VMaNGOS spell_template coefficient 0.8, reduced for ranks learned below level 20); the spell fails
// when the warlock has no more health than that. 18182 Improved Life Tap: "Increases the amount of Mana awarded by your Life Tap spell
// by $s1%". 403816 Metamorphosis (effect 3) adds its own %. 1225214 Demonic Energies (effect 1): "When you gain Mana from Life Tap, your
// summoned demon gains $m2% of the Mana you gain."
class classic_talent_warl_life_tap : public SpellScript
{
    static constexpr float LIFE_TAP_SPELL_POWER_COEFFICIENT = 0.8f;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_LIFE_TAP_ENERGIZE });
    }

    int32 CalcHealthCost() const
    {
        Unit* caster = GetCaster();
        SpellInfo const* spellInfo = GetSpellInfo();

        float coefficient = LIFE_TAP_SPELL_POWER_COEFFICIENT;
        // vanilla level penalty (Unit::SpellDamageBonusDone ClassicSpellLevelPenalty)
        if (spellInfo->SpellLevel > 0 && spellInfo->SpellLevel < 20)
            coefficient *= 1.0f - (20.0f - float(spellInfo->SpellLevel)) * 0.0375f;

        double cost = GetEffectInfo(EFFECT_0).CalcValue(caster);
        cost += caster->SpellBaseDamageBonusDone(spellInfo->GetSchoolMask()) * coefficient;
        return std::max(int32(cost), 0);
    }

    SpellCastResult CheckCast()
    {
        if (int64(GetCaster()->GetHealth()) <= int64(CalcHealthCost()))
            return SPELL_FAILED_FIZZLE;
        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        int32 const cost = CalcHealthCost();
        if (cost <= 0 || int64(caster->GetHealth()) <= int64(cost))
            return;

        // not shown in the combat log (vanilla)
        caster->ModifyHealth(-cost);

        int32 mana = cost;
        if (AuraEffect const* improved = caster->GetAuraEffect(SPELL_CWD_IMPROVED_LIFE_TAP, EFFECT_0))
            AddPct(mana, improved->GetAmountAsInt());
        if (AuraEffect const* metamorphosis = caster->GetAuraEffect(SPELL_CWD_METAMORPHOSIS, EFFECT_3))
            AddPct(mana, metamorphosis->GetAmountAsInt());

        if (mana <= 0)
            return;

        caster->CastSpell(caster, SPELL_CWD_LIFE_TAP_ENERGIZE, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(mana));

        if (AuraEffect const* energies = caster->GetAuraEffect(SPELL_CWD_DEMONIC_ENERGIES, EFFECT_1))
            if (Player* player = caster->ToPlayer())
                if (Pet* pet = GetActiveDemon(player))
                    if (pet->GetMaxPower(POWER_MANA) > 0)
                        if (int32 petMana = CalculatePct(mana, energies->GetAmountAsInt()); petMana > 0)
                            caster->EnergizeBySpell(pet, GetSpellInfo(), petMana, POWER_MANA);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_talent_warl_life_tap::CheckCast);
        OnEffectHitTarget += SpellEffectFn(classic_talent_warl_life_tap::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1225214 - Demonic Energies (effect 0): "You heal your pet for $s1% of all spell damage you deal." (proc: harmful spell + periodic)
// The Life Tap part (effect 1) is in classic_talent_warl_life_tap.
class classic_talent_warl_demonic_energies : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_DEMONIC_ENERGIES_HEAL });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return false;

        Player* player = GetTarget()->ToPlayer();
        Pet* pet = player ? GetActiveDemon(player) : nullptr;
        return pet && pet->GetHealth() < pet->GetMaxHealth();
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        int32 const heal = CalculatePct(int32(eventInfo.GetDamageInfo()->GetDamage()), aurEff->GetAmountAsInt());
        if (heal <= 0)
            return;

        // 1243028 targets the caster's pet
        GetTarget()->CastSpell(GetTarget(), SPELL_CWD_DEMONIC_ENERGIES_HEAL, CastSpellExtraArgs(aurEff).AddSpellBP0(heal));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_talent_warl_demonic_energies::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_talent_warl_demonic_energies::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 17793 - Improved Shadow Bolt: "Your Shadow Bolt critical strikes increase Shadow damage taken by the target from your attacks by $m1%
// for $17794d." 17794 Shadow Vulnerability is SPELL_AURA_MOD_SCHOOL_MASK_DAMAGE_FROM_CASTER (shadow).
class classic_talent_warl_improved_shadow_bolt : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_SHADOW_VULNERABILITY });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        // Shadow Bolt: warlock family flag 0x1
        return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_WARLOCK && (spellInfo->SpellFamilyFlags[0] & 0x1)
            && (eventInfo.GetHitMask() & PROC_HIT_CRITICAL) && eventInfo.GetActionTarget();
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        CastSpellExtraArgs args(aurEff);
        if (aurEff->GetAmount() > 0)
            args.AddSpellBP0(aurEff->GetAmount());
        GetTarget()->CastSpell(eventInfo.GetActionTarget(), SPELL_CWD_SHADOW_VULNERABILITY, args);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_talent_warl_improved_shadow_bolt::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_talent_warl_improved_shadow_bolt::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 980, 1014, 6217, 11711, 11712, 11713 - Curse of Agony: the damage per tick ramps up in three stages of four ticks, half, full and one and a
// half of the listed tick (the total over the 12 ticks stays the listed damage). Behaviour from vMaNGOS (GPL-2.0), Aura::HandlePeriodicDamage:
// fdamage += (-1 + (tick - 1) / 4) * (base / 2). The tick amount itself still comes from the client spell data.
class classic_spell_warl_curse_of_agony : public AuraScript
{
    static constexpr uint32 TICKS_PER_STAGE = 4;

    void CalculateDamage(AuraEffect const* aurEff, Unit const* /*victim*/, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        // GetTickNumber is already counted for the tick being calculated (first tick = 1)
        uint32 const tick = std::max<uint32>(aurEff->GetTickNumber(), 1);
        uint32 const stage = (tick - 1) / TICKS_PER_STAGE;
        pctMod *= 0.5f + 0.5f * float(std::min<uint32>(stage, 2));
    }

    void Register() override
    {
        DoEffectCalcDamageAndHealing += AuraEffectCalcDamageFn(classic_spell_warl_curse_of_agony::CalculateDamage, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// 18094 - Nightfall: "Gives your Corruption, Drain Soul, Drain Life, and Wrack spells a $m1% chance to cause you to enter a Shadow Trance after
// damaging the opponent." The client spell is a dummy aura, nothing cast 17941 Shadow Trance, so the talent never procced. Each tick of the
// four spells' periodic damage is one roll.
class classic_talent_warl_nightfall : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_SHADOW_TRANCE });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        // Corruption: warlock family flag [0] 0x2, Drain Life: [0] 0x8, Drain Soul: [0] 0x4000, Wrack 1316697: [1] 0x40000
        return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_WARLOCK
            && ((spellInfo->SpellFamilyFlags[0] & 0x400A) || (spellInfo->SpellFamilyFlags[1] & 0x40000));
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();

        if (!roll_chance(float(aurEff->GetAmount())))
            return;

        GetTarget()->CastSpell(GetTarget(), SPELL_CWD_SHADOW_TRANCE, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetTriggeringAura(aurEff));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_talent_warl_nightfall::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_talent_warl_nightfall::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 18073 - Pyroclasm: "Gives your Rain of Fire, Hellfire and Soul Fire spells a $s1% chance to stun the target for 3 sec." Effect 0 is a dummy
// with the chance, effect 1 triggers 18350 (a spell with only a dummy effect that nothing handled), so the stun 18093 was never cast.
// Soul Fire and Hellfire share the warlock family flag 0x40, Rain of Fire is 0x20 (the spell_proc row disables effect 1).
class classic_talent_warl_pyroclasm : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_PYROCLASM_STUN });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_WARLOCK && (spellInfo->SpellFamilyFlags[0] & 0x60)
            && eventInfo.GetActionTarget();
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        if (!roll_chance(float(aurEff->GetAmount())))
            return;

        GetTarget()->CastSpell(eventInfo.GetActionTarget(), SPELL_CWD_PYROCLASM_STUN, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetTriggeringAura(aurEff));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_talent_warl_pyroclasm::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_talent_warl_pyroclasm::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 689, 699, 709, 7651, 11699, 11700 - Drain Life; 1120, 8288, 8289, 11675 - Drain Soul; 1316697 - Wrack
// 17804 - Soul Siphon: "Increases the damage done or health drained by your Drain Life, Drain Soul, and Wrack spells by $s1% per each
// of your other Affliction effects active on the target, up to a maximum increase of ${$m1*3}%." Affliction effects: the warlock's auras
// on the target from the Affliction skill line (355). Drain Life heals for the damage it deals, so the health drained follows.
class classic_talent_warl_soul_siphon : public AuraScript
{
    void CalcDamage(AuraEffect const* aurEff, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod)
    {
        if (aurEff->GetAuraType() != SPELL_AURA_PERIODIC_DAMAGE && aurEff->GetAuraType() != SPELL_AURA_PERIODIC_LEECH)
            return;

        Unit* caster = GetCaster();
        if (!caster || !victim)
            return;

        AuraEffect const* talent = caster->GetAuraEffect(SPELL_CWD_SOUL_SIPHON, EFFECT_0);
        if (!talent || talent->GetAmount() <= 0.0)
            return;

        uint32 effects = 0;
        for (auto const& [spellId, aurApp] : victim->GetAppliedAuras())
        {
            Aura const* aura = aurApp->GetBase();
            if (spellId == GetId() || aura->GetCasterGUID() != caster->GetGUID())
                continue;

            if (IsAfflictionSpell(spellId))
                ++effects;
        }

        if (!effects)
            return;

        float const bonus = std::min<float>(float(effects * talent->GetAmount()), float(talent->GetAmount() * 3.0));
        AddPct(pctMod, bonus);
    }

    void Register() override
    {
        DoEffectCalcDamageAndHealing += AuraEffectCalcDamageFn(classic_talent_warl_soul_siphon::CalcDamage, EFFECT_ALL, SPELL_AURA_ANY);
    }
};

// 1120, 8288, 8289, 11675 - Drain Soul
// 437032 - Soul Harvest: "Killing a non-trivial target afflicted by your Drain Soul increases your Mana regeneration by $m1% for
// $1242853d and allows $m2% of normal Mana regeneration to continue while casting."
class classic_talent_warl_soul_harvest : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_CWD_SOUL_HARVEST_BUFF }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_DEATH)
            return;

        Player* warlock = Object::ToPlayer(GetCaster());
        if (!warlock || !warlock->isHonorOrXPTarget(GetTarget()))
            return;

        AuraEffect const* regen = warlock->GetAuraEffect(SPELL_CWD_SOUL_HARVEST, EFFECT_0);
        AuraEffect const* whileCasting = warlock->GetAuraEffect(SPELL_CWD_SOUL_HARVEST, EFFECT_1);
        if (!regen || !whileCasting)
            return;

        warlock->CastSpell(warlock, SPELL_CWD_SOUL_HARVEST_BUFF, CastSpellExtraArgs(TRIGGERED_FULL_MASK)
            .AddSpellMod(SPELLVALUE_BASE_POINT0, regen->GetAmount())
            .AddSpellMod(SPELLVALUE_BASE_POINT1, whileCasting->GetAmount()));
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(classic_talent_warl_soul_harvest::HandleRemove, EFFECT_1, SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }
};

// 23785 - Master Demonologist (see SyncMasterDemonologist) and 412732 - Demonic Knowledge (see SyncDemonicKnowledge).
// The demon auras are refreshed when the talent is learned or removed, every heartbeat and right after a demon is summoned.
class classic_talent_warl_demon_talent_sync : public AuraScript
{
    void HandleApplyOrRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        ScheduleDemonTalentSync(GetUnitOwner());
    }

    // GetUnitOwner, not GetTarget: on the heartbeat there is no current aura application
    void HandleHeartbeat()
    {
        ScheduleDemonTalentSync(GetUnitOwner());
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_talent_warl_demon_talent_sync::HandleApplyOrRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_talent_warl_demon_talent_sync::HandleApplyOrRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnHeartbeat += AuraHeartbeatFn(classic_talent_warl_demon_talent_sync::HandleHeartbeat);
    }
};

// 688 - Summon Imp, 691 - Summon Felhunter, 697 - Summon Voidwalker, 712 - Summon Succubus, 713 - Summon Incubus, 427733 - Summon Felguard
// Summoning a demon cancels the Demonic Sacrifice buff (vanilla, VMaNGOS Unit::SummonPet / patch 1.12).
// 425464 - Demonic Pact: "Your Demonic Sacrifice effect is no longer cancelled by summoning a different Demon pet. Resummoning the
// sacrificed pet will still cancel the effect."
class classic_talent_warl_summon_demon : public SpellScript
{
    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        DemonKind const summoned = GetDemonKind(uint32(GetEffectInfo(EFFECT_0).MiscValue));
        uint32 const summonedBuff = GetSacrificeBuff(summoned);
        bool const demonicPact = caster->HasAura(SPELL_CWD_DEMONIC_PACT);

        for (uint32 buff : { SPELL_CWD_BURNING_SHADOW, SPELL_CWD_FEL_STAMINA, SPELL_CWD_TOUCH_OF_FIRE, SPELL_CWD_FEL_ENERGY })
        {
            if (demonicPact && buff != summonedBuff)
                continue;
            caster->RemoveAurasDueToSpell(buff);
        }

        ScheduleDemonTalentSync(caster, 500ms);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(classic_talent_warl_summon_demon::HandleAfterCast);
    }
};

// 18788 - Demonic Sacrifice: "Sacrifices your summoned demon to grant you an effect that lasts 30 min." The client spell only kills the
// demon (instakill on the pet); the buff of the sacrificed demon (VMaNGOS WarlockDemonicSacrificeScript): Imp 18789 Burning Shadow,
// Voidwalker 18790 Fel Stamina, Succubus/Incubus 18791 Touch of Fire, Felhunter 18792 Fel Energy.
class classic_talent_warl_demonic_sacrifice : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_BURNING_SHADOW, SPELL_CWD_FEL_STAMINA, SPELL_CWD_TOUCH_OF_FIRE, SPELL_CWD_FEL_ENERGY });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* demon = GetHitUnit();
        if (!demon || demon->GetOwnerGUID() != caster->GetGUID())
            return;

        uint32 const buff = GetSacrificeBuff(GetDemonKind(demon->GetEntry()));
        if (!buff)
            return;

        for (uint32 other : { SPELL_CWD_BURNING_SHADOW, SPELL_CWD_FEL_STAMINA, SPELL_CWD_TOUCH_OF_FIRE, SPELL_CWD_FEL_ENERGY })
            if (other != buff)
                caster->RemoveAurasDueToSpell(other);

        caster->CastSpell(caster, buff, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_talent_warl_demonic_sacrifice::HandleHit, EFFECT_0, SPELL_EFFECT_INSTAKILL);
    }
};

// 17962, 18930, 18931, 18932, 1293817, 1293818 - Conflagrate: "Ignites a target that is already afflicted by your Immolate spell, dealing
// Fire damage and consuming your Immolate effect." (the consume was missing: the Immolate DoT and its hidden marker 1282590 are removed)
// 426316 - Shadow and Flame: "Hitting an enemy with Conflagrate increases all Shadow damage you deal by $m3% for $1293816d [...]
// Conflagrate has a $m2% chance not to consume Immolate".
class classic_talent_warl_conflagrate : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_SHADOW_AND_FLAME_SHADOW });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        AuraEffect const* keepChance = caster->GetAuraEffect(SPELL_CWD_SHADOW_AND_FLAME, EFFECT_1);
        if (AuraEffect const* shadowBonus = caster->GetAuraEffect(SPELL_CWD_SHADOW_AND_FLAME, EFFECT_2))
            caster->CastSpell(caster, SPELL_CWD_SHADOW_AND_FLAME_SHADOW, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(shadowBonus->GetAmount()));

        if (keepChance && roll_chance(keepChance->GetAmount()))
            return;

        // Immolate: warlock family flag 0x4
        if (AuraEffect* immolate = target->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_WARLOCK, flag128(0x4, 0, 0, 0), caster->GetGUID()))
            immolate->GetBase()->Remove();
        target->RemoveAurasDueToSpell(SPELL_CWD_IMMOLATE_MARKER, caster->GetGUID());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_talent_warl_conflagrate::HandleHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 412758, 1293812, 1293813 - Incinerate (70532): "Deals $s1 Fire damage to your target and an additional $m2% damage if the target is
// afflicted by Immolate." Effect 1 is the bonus as a dummy. The Immolate is the warlock's own (warlock family flag 0x4), as for Conflagrate.
class classic_spell_warl_incinerate : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void HandleCalcDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod)
    {
        Unit* caster = GetCaster();
        if (!caster || !victim)
            return;

        if (victim->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_WARLOCK, flag128(0x4, 0, 0, 0), caster->GetGUID()))
            AddPct(pctMod, GetEffectInfo(EFFECT_1).CalcValue(caster));
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_warl_incinerate::HandleCalcDamage);
    }
};

// 440870 - Decimation (70532): "When you cast Shadow Bolt or Searing Pain on an enemy below $m3% health, they deal $m4% increased damage, and
// for the next $440873d your Soul Fire spell has its cast time reduced by $m1% and costs no Soul Shards." Effect 3 is a damage spell modifier
// on Shadow Bolt and Searing Pain that the client data applies at any health, so the talent keeps it at 0 and the bonus is given per target by
// classic_spell_warl_decimation. Effects 0 and 2 (cast time, health threshold) are dummies.
class classic_talent_warl_decimation : public AuraScript
{
    void CalculateAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/)
    {
        amount = 0;
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(classic_talent_warl_decimation::CalculateAmount, EFFECT_3, SPELL_AURA_ADD_PCT_MODIFIER);
    }
};

// Shadow Bolt (686, 695, 705, 1088, 1106, 7641, 11659, 11660, 11661, 25307) and Searing Pain (5676, 17919, 17920, 17921, 17922, 17923) with
// Decimation: on a target below $m3% health, +$m4% damage and 440873 (Soul Fire cast time -$m1%, no Soul Shard) on the warlock.
class classic_spell_warl_decimation : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_CWD_DECIMATION, EFFECT_3 } }) && ValidateSpellInfo({ SPELL_CWD_DECIMATION_BUFF });
    }

    // the talent's health threshold, or 0 when the warlock does not have it
    static int32 GetThreshold(Unit const* caster)
    {
        AuraEffect const* threshold = caster->GetAuraEffect(SPELL_CWD_DECIMATION, EFFECT_2);
        return threshold ? threshold->GetAmountAsInt() : 0;
    }

    void HandleCalcDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod)
    {
        Unit* caster = GetCaster();
        int32 const threshold = GetThreshold(caster);
        if (!victim || threshold <= 0 || !victim->HealthBelowPct(threshold))
            return;

        // effect 3 of the talent at the warlock's rank (the aura itself is kept at 0)
        SpellInfo const* talentInfo = sSpellMgr->AssertSpellInfo(SPELL_CWD_DECIMATION, DIFFICULTY_NONE);
        AddPct(pctMod, talentInfo->GetEffect(EFFECT_3).CalcValue(caster));
    }

    void HandleBeforeHit(SpellMissInfo missInfo)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        int32 const threshold = GetThreshold(caster);
        if (missInfo != SPELL_MISS_NONE || !target || threshold <= 0 || !target->HealthBelowPct(threshold))
            return;

        AuraEffect const* castTime = caster->GetAuraEffect(SPELL_CWD_DECIMATION, EFFECT_0);
        CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
        if (castTime && castTime->GetAmount() > 0)
            args.AddSpellBP0(-castTime->GetAmountAsInt());
        caster->CastSpell(caster, SPELL_CWD_DECIMATION_BUFF, args);
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_warl_decimation::HandleCalcDamage);
        BeforeHit += BeforeSpellHitFn(classic_spell_warl_decimation::HandleBeforeHit);
    }
};

// 17877, 18867, 18868, 18869, 18870, 18871 - Shadowburn
// 426316 - Shadow and Flame: "[...] hitting an enemy with Shadowburn increases all Fire damage you deal by $m4% for $426311d. [...]
// Shadowburn has a $m2% chance to instantly refund a Soul Shard." (Shadowburn costs a Soul Shard)
class classic_talent_warl_shadowburn : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_SHADOW_AND_FLAME_FIRE });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (AuraEffect const* fireBonus = caster->GetAuraEffect(SPELL_CWD_SHADOW_AND_FLAME, EFFECT_3))
            caster->CastSpell(caster, SPELL_CWD_SHADOW_AND_FLAME_FIRE, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(fireBonus->GetAmount()));

        if (_shardRefunded || GetSpell()->IsTriggered())
            return;

        AuraEffect const* refundChance = caster->GetAuraEffect(SPELL_CWD_SHADOW_AND_FLAME, EFFECT_1);
        if (!refundChance || !roll_chance(refundChance->GetAmount()))
            return;

        if (Player* player = caster->ToPlayer())
        {
            player->AddItem(ITEM_CWD_SOUL_SHARD, 1);
            _shardRefunded = true;
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_talent_warl_shadowburn::HandleHit, EFFECT_1, SPELL_EFFECT_SCHOOL_DAMAGE);
    }

    bool _shardRefunded = false;
};

// 1243338 - Bane of Havoc (warlock side, proc on the warlock's damage). Deals $1225228s1% of the damage done to other targets to the
// cursed target as 1243339.
class classic_talent_warl_bane_of_havoc_tracker : public AuraScript
{
public:
    ObjectGuid const& GetHavocTarget() const { return _havocTarget; }
    void SetHavocTarget(ObjectGuid const& guid) { _havocTarget = guid; }

private:
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_BANE_OF_HAVOC, SPELL_CWD_BANE_OF_HAVOC_DAMAGE });
    }

    Unit* GetHavocUnit() const
    {
        if (_havocTarget.IsEmpty())
            return nullptr;

        Unit* warlock = GetTarget();
        Unit* havoc = ObjectAccessor::GetUnit(*warlock, _havocTarget);
        if (!havoc || !havoc->IsAlive() || !havoc->HasAura(SPELL_CWD_BANE_OF_HAVOC, warlock->GetGUID()))
            return nullptr;
        return havoc;
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return false;

        if (SpellInfo const* spellInfo = eventInfo.GetSpellInfo())
            if (spellInfo->Id == SPELL_CWD_BANE_OF_HAVOC_DAMAGE)
                return false;

        Unit* victim = eventInfo.GetActionTarget();
        if (!victim || victim == GetTarget() || victim->GetGUID() == _havocTarget)
            return false;

        return GetHavocUnit() != nullptr;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* havoc = GetHavocUnit();
        if (!havoc)
            return;

        int32 pct = aurEff->GetAmountAsInt();
        if (AuraEffect const* bane = havoc->GetAuraEffect(SPELL_CWD_BANE_OF_HAVOC, EFFECT_0, GetTarget()->GetGUID()))
            pct = bane->GetAmountAsInt();

        int32 const damage = CalculatePct(int32(eventInfo.GetDamageInfo()->GetDamage()), pct);
        if (damage <= 0)
            return;

        GetTarget()->CastSpell(havoc, SPELL_CWD_BANE_OF_HAVOC_DAMAGE, CastSpellExtraArgs(aurEff).AddSpellBP0(damage));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_talent_warl_bane_of_havoc_tracker::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_talent_warl_bane_of_havoc_tracker::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

    ObjectGuid _havocTarget;
};

// 1225228 - Bane of Havoc: "Afflicts the target for $d, causing $s1% of all damage done by the Warlock to other targets to also be dealt
// to the cursed target. Bane of Havoc is limited to 1 target". The curse (dummy) puts the tracker 1243338 on the warlock and removes the
// warlock's previous Bane of Havoc.
class classic_talent_warl_bane_of_havoc : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_BANE_OF_HAVOC_TRACKER });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        Unit* target = GetTarget();
        Aura* tracker = caster->GetAura(SPELL_CWD_BANE_OF_HAVOC_TRACKER, caster->GetGUID());
        if (!tracker)
        {
            caster->CastSpell(caster, SPELL_CWD_BANE_OF_HAVOC_TRACKER, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
            tracker = caster->GetAura(SPELL_CWD_BANE_OF_HAVOC_TRACKER, caster->GetGUID());
        }

        classic_talent_warl_bane_of_havoc_tracker* script = tracker ? tracker->GetScript<classic_talent_warl_bane_of_havoc_tracker>() : nullptr;
        if (!script)
            return;

        ObjectGuid const previous = script->GetHavocTarget();
        script->SetHavocTarget(target->GetGUID());

        if (!previous.IsEmpty() && previous != target->GetGUID())
            if (Unit* oldTarget = ObjectAccessor::GetUnit(*caster, previous))
                oldTarget->RemoveAurasDueToSpell(SPELL_CWD_BANE_OF_HAVOC, caster->GetGUID());
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        Aura* tracker = caster->GetAura(SPELL_CWD_BANE_OF_HAVOC_TRACKER, caster->GetGUID());
        if (!tracker)
            return;

        classic_talent_warl_bane_of_havoc_tracker* script = tracker->GetScript<classic_talent_warl_bane_of_havoc_tracker>();
        if (!script || script->GetHavocTarget() == GetTarget()->GetGUID())
            tracker->Remove();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_talent_warl_bane_of_havoc::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_talent_warl_bane_of_havoc::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 6201, 6202, 5699, 11729, 11730 - Create Healthstone (client: script effect, nothing was created)
// Vanilla (VMaNGOS WarlockCreateHealthstoneScript): the item depends on 18692 / 18693 Improved Healthstone ("Increases the amount of
// Health restored by your Healthstone by $s1%"): rank 1 and rank 2 have their own, stronger healthstone items.
class classic_talent_warl_create_healthstone : public SpellScript
{
    static uint32 GetItemId(Unit const* caster, uint32 spellId)
    {
        uint32 rank = 0;
        if (caster->HasAura(SPELL_CWD_IMPROVED_HEALTHSTONE_R2))
            rank = 2;
        else if (AuraEffect const* rank1 = caster->GetAuraEffect(SPELL_CWD_IMPROVED_HEALTHSTONE_R1, EFFECT_0))
            rank = rank1->GetAmount() >= 20.0 ? 2 : 1;     // single trait spell: the curve gives the rank 2 value

        static uint32 const items[5][3] =
        {
            { 5512, 19004, 19005 },     // Minor Healthstone
            { 5511, 19006, 19007 },     // Lesser Healthstone
            { 5509, 19008, 19009 },     // Healthstone
            { 5510, 19010, 19011 },     // Greater Healthstone
            { 9421, 19012, 19013 },     // Major Healthstone
        };

        switch (spellId)
        {
            case 6201:  return items[0][rank];
            case 6202:  return items[1][rank];
            case 5699:  return items[2][rank];
            case 11729: return items[3][rank];
            case 11730: return items[4][rank];
            default:    return 0;
        }
    }

    SpellCastResult CheckCast()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return SPELL_CAST_OK;

        uint32 const itemId = GetItemId(player, GetSpellInfo()->Id);
        if (!itemId)
            return SPELL_CAST_OK;

        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, 1);
        if (msg != EQUIP_ERR_OK)
        {
            player->SendEquipError(msg, nullptr, nullptr, itemId);
            return SPELL_FAILED_DONT_REPORT;
        }

        return SPELL_CAST_OK;
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* target = GetHitUnit();
        if (!target || !target->IsPlayer())
            return;

        if (uint32 const itemId = GetItemId(target, GetSpellInfo()->Id))
            CreateItem(itemId, ItemContext::NONE);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_talent_warl_create_healthstone::CheckCast);
        OnEffectHitTarget += SpellEffectFn(classic_talent_warl_create_healthstone::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

/*######
## Druid
######*/

// Classic 1.60 (client 70374) Furor, second part: "Shapeshifting into Cat Form restores ${100*($m2/100)}% of the Energy you had when last
// in Cat Form, plus ${10*($m2/100)} Energy per sec spent outside Cat Form, up to ${100*($m2/100)} Energy. Bear Form and Dire Bear Form
// forfeit this Energy and halt its restoration while active." ($m2 = effect 1 of 17056, 20% per rank)
namespace ClassicFurorCat
{
    struct State
    {
        int32 Energy = 0;               // energy when the druid last left Cat Form (0 after a bear form)
        TimePoint CountingSince;        // start of the time that restores energy (left Cat Form, or left a bear form)
        bool Counting = false;          // false while in a bear form
    };

    static std::unordered_map<ObjectGuid, State> States;
    static std::mutex StatesLock;

    static void LeftCatForm(Unit* druid)
    {
        std::lock_guard<std::mutex> guard(StatesLock);
        State& state = States[druid->GetGUID()];
        state.Energy = druid->GetPower(POWER_ENERGY);
        state.CountingSince = GameTime::Now();
        state.Counting = true;
    }

    static void EnteredBearForm(Unit* druid)
    {
        std::lock_guard<std::mutex> guard(StatesLock);
        auto itr = States.find(druid->GetGUID());
        if (itr != States.end())
            itr->second = State();      // energy forfeited, restoration halted
    }

    static void LeftBearForm(Unit* druid)
    {
        std::lock_guard<std::mutex> guard(StatesLock);
        auto itr = States.find(druid->GetGUID());
        if (itr != States.end())
        {
            itr->second.CountingSince = GameTime::Now();
            itr->second.Counting = true;
        }
    }

    static void EnteredCatForm(Unit* druid)
    {
        AuraEffect const* furor = druid->GetAuraEffect(SPELL_CWD_FUROR, EFFECT_1);
        State state;
        {
            std::lock_guard<std::mutex> guard(StatesLock);
            auto itr = States.find(druid->GetGUID());
            if (itr == States.end())
                return;
            state = itr->second;
        }
        if (!furor || furor->GetAmount() <= 0)
            return;

        float const factor = float(furor->GetAmount()) / 100.0f;
        float energy = state.Energy * factor;
        if (state.Counting)
            energy += 10.0f * factor * std::chrono::duration<float>(GameTime::Now() - state.CountingSince).count();
        energy = std::min(energy, 100.0f * factor);
        if (int32(energy) > druid->GetPower(POWER_ENERGY))
            druid->SetPower(POWER_ENERGY, int32(energy));
    }
}

// 17007 - Leader of the Pack: "While in Cat Form, Bear Form or Dire Bear Form, the Leader of the Pack increases ranged and melee critical strike
// chance of all party members within 45 yards by 3%." The client talent is a dummy aura; the effect is the passive party area aura 24932, which
// nothing cast, so no party ever got the critical strike chance. Behaviour from vMaNGOS (GPL-2.0): the aura is cast on the druid when a
// feral form is entered with the talent known and removed when the form ends.
static void UpdateLeaderOfThePack(Unit* druid, ShapeshiftForm form)
{
    bool const feral = form == FORM_CAT_FORM || form == FORM_BEAR_FORM || form == FORM_DIRE_BEAR_FORM;
    if (feral && druid->HasAura(SPELL_CWD_LEADER_OF_THE_PACK))
    {
        if (!druid->HasAura(SPELL_CWD_LEADER_OF_THE_PACK_AURA))
            druid->CastSpell(druid, SPELL_CWD_LEADER_OF_THE_PACK_AURA, TRIGGERED_FULL_MASK);
    }
    else
        druid->RemoveAurasDueToSpell(SPELL_CWD_LEADER_OF_THE_PACK_AURA);
}

// 17003 - Heart of the Wild: "Increases your Intellect by $s1%. In addition, while in Bear or Dire Bear Form your Stamina is increased by $s2% and
// while in Cat Form your Strength is increased by $s3%." The client talent holds the intellect aura and two dummy effects with the form
// amounts; the form auras 24899 (bear, stamina) and 24900 (cat, strength) are in the client data but nothing cast them, so the form bonus
// never applied. Behaviour from vMaNGOS (GPL-2.0, Aura::HandleAuraModShapeshift): the form aura is cast with the talent amount of the form
// when the form is entered and removed when it ends.
static void UpdateHeartOfTheWild(Unit* druid, ShapeshiftForm form)
{
    uint32 wanted = 0;
    SpellEffIndex amountIndex = EFFECT_0;
    if (form == FORM_BEAR_FORM || form == FORM_DIRE_BEAR_FORM)
    {
        wanted = SPELL_CWD_HEART_OF_THE_WILD_BEAR;
        amountIndex = EFFECT_1;
    }
    else if (form == FORM_CAT_FORM)
    {
        wanted = SPELL_CWD_HEART_OF_THE_WILD_CAT;
        amountIndex = EFFECT_2;
    }

    for (uint32 formAura : { SPELL_CWD_HEART_OF_THE_WILD_BEAR, SPELL_CWD_HEART_OF_THE_WILD_CAT })
        if (formAura != wanted)
            druid->RemoveAurasDueToSpell(formAura);

    if (!wanted)
        return;

    AuraEffect const* talent = druid->GetAuraEffect(SPELL_CWD_HEART_OF_THE_WILD, amountIndex);
    int32 const amount = talent ? talent->GetAmountAsInt() : 0;
    if (Aura* current = druid->GetAura(wanted))
    {
        AuraEffect const* currentEffect = current->GetEffect(EFFECT_0);
        if (amount > 0 && currentEffect && currentEffect->GetAmountAsInt() == amount)
            return;

        druid->RemoveAurasDueToSpell(wanted);
    }

    if (amount > 0)
        druid->CastSpell(druid, wanted, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(amount));
}

// 17002 - Feral Swiftness (70532): "Increases your movement speed while in Cat Form by $m2%, and increases your chance to Dodge by $m1%."
// Effect 0 is the dodge aura; effect 1 is the Cat Form speed as a dummy, so the speed never applied. Behaviour from vMaNGOS (GPL-2.0,
// Aura::HandleAuraModShapeshift): the speed aura 24867 is cast in Cat Form and removed when the form ends; here with the talent amount.
static void UpdateFeralSwiftness(Unit* druid, ShapeshiftForm form)
{
    AuraEffect const* talent = form == FORM_CAT_FORM ? druid->GetAuraEffect(SPELL_CWD_FERAL_SWIFTNESS, EFFECT_1) : nullptr;
    int32 const amount = talent ? talent->GetAmountAsInt() : 0;
    if (Aura* current = druid->GetAura(SPELL_CWD_FERAL_SWIFTNESS_CAT))
    {
        AuraEffect const* currentEffect = current->GetEffect(EFFECT_0);
        if (amount > 0 && currentEffect && currentEffect->GetAmountAsInt() == amount)
            return;

        druid->RemoveAurasDueToSpell(SPELL_CWD_FERAL_SWIFTNESS_CAT);
    }

    if (amount > 0)
        druid->CastSpell(druid, SPELL_CWD_FERAL_SWIFTNESS_CAT, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(amount));
}

// 768 - Cat Form, 5487 - Bear Form, 9634 - Dire Bear Form, 24858 - Moonkin Form
// 16929 - Thick Hide: base armor while in these forms (UpdateThickHide).
// 17056 - Furor: "Gives you a $m1% chance to gain 10 Rage when shapeshifting into Bear Form or Dire Bear Form." (17057: 10 rage), and the
// Cat Form energy (ClassicFurorCat)
class classic_talent_dru_form_talents : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_THICK_HIDE_ARMOR, SPELL_CWD_FUROR_RAGE, SPELL_CWD_LEADER_OF_THE_PACK_AURA,
            SPELL_CWD_HEART_OF_THE_WILD_BEAR, SPELL_CWD_HEART_OF_THE_WILD_CAT, SPELL_CWD_FERAL_SWIFTNESS_CAT });
    }

    void HandleApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        Unit* druid = GetTarget();
        UpdateThickHide(druid);
        UpdatePredatoryStrikes(druid);

        ShapeshiftForm const form = ShapeshiftForm(aurEff->GetMiscValue());
        UpdateLeaderOfThePack(druid, form);
        UpdateHeartOfTheWild(druid, form);
        UpdateFeralSwiftness(druid, form);
        if (form == FORM_BEAR_FORM || form == FORM_DIRE_BEAR_FORM)
        {
            ClassicFurorCat::EnteredBearForm(druid);
            if (AuraEffect const* furor = druid->GetAuraEffect(SPELL_CWD_FUROR, EFFECT_0))
                if (roll_chance(furor->GetAmount()))
                    druid->CastSpell(druid, SPELL_CWD_FUROR_RAGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        }
        else if (form == FORM_CAT_FORM)
            ClassicFurorCat::EnteredCatForm(druid);
    }

    void HandleRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        Unit* druid = GetTarget();
        druid->RemoveAurasDueToSpell(SPELL_CWD_THICK_HIDE_ARMOR);
        UpdatePredatoryStrikes(druid, true);
        // the unit already stands in the next form (or none) when this runs
        UpdateLeaderOfThePack(druid, druid->GetShapeshiftForm());
        UpdateHeartOfTheWild(druid, druid->GetShapeshiftForm());
        UpdateFeralSwiftness(druid, druid->GetShapeshiftForm());

        ShapeshiftForm const form = ShapeshiftForm(aurEff->GetMiscValue());
        if (form == FORM_CAT_FORM)
            ClassicFurorCat::LeftCatForm(druid);
        else if (form == FORM_BEAR_FORM || form == FORM_DIRE_BEAR_FORM)
            ClassicFurorCat::LeftBearForm(druid);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_talent_dru_form_talents::HandleApply, EFFECT_0, SPELL_AURA_MOD_SHAPESHIFT, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_talent_dru_form_talents::HandleRemove, EFFECT_0, SPELL_AURA_MOD_SHAPESHIFT, AURA_EFFECT_HANDLE_REAL);
    }
};

// 17007 - Leader of the Pack: the talent learned or lost while in a feral form
class classic_talent_dru_leader_of_the_pack : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_LEADER_OF_THE_PACK_AURA });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        UpdateLeaderOfThePack(GetTarget(), GetTarget()->GetShapeshiftForm());
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_CWD_LEADER_OF_THE_PACK_AURA);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_talent_dru_leader_of_the_pack::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_talent_dru_leader_of_the_pack::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 17003 - Heart of the Wild: the talent learned, ranked up or lost while in a form (the first effect is the intellect aura, the form
// amounts are the dummy effects 1 and 2)
class classic_talent_dru_heart_of_the_wild : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_CWD_HEART_OF_THE_WILD_BEAR, SPELL_CWD_HEART_OF_THE_WILD_CAT }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 } });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        UpdateHeartOfTheWild(GetTarget(), GetTarget()->GetShapeshiftForm());
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_CWD_HEART_OF_THE_WILD_BEAR);
        GetTarget()->RemoveAurasDueToSpell(SPELL_CWD_HEART_OF_THE_WILD_CAT);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_talent_dru_heart_of_the_wild::HandleApply, EFFECT_2, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        AfterEffectRemove += AuraEffectRemoveFn(classic_talent_dru_heart_of_the_wild::HandleRemove, EFFECT_2, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 417051 - Natural Reaction (70532): "Increases your dodge chance by $m1%, and gives you a $m2% chance to gain ${$417053m1/10} Rage each time you
// dodge." Effect 0 is the dodge aura; the rage chance ($m2) is effect 1, a dummy aura with no proc data, so a dodge gave nothing. A dodged melee
// attack rolls the chance and casts 417053.
class classic_talent_dru_natural_reaction : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_CWD_NATURAL_REACTION_RAGE }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    bool CheckProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        return (eventInfo.GetHitMask() & PROC_HIT_DODGE) && roll_chance(aurEff->GetAmount());
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_CWD_NATURAL_REACTION_RAGE, CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(classic_talent_dru_natural_reaction::CheckProc, EFFECT_1, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(classic_talent_dru_natural_reaction::HandleProc, EFFECT_1, SPELL_AURA_DUMMY);
    }
};

// 17002 - Feral Swiftness: the talent learned or lost while in Cat Form
class classic_talent_dru_feral_swiftness : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_CWD_FERAL_SWIFTNESS_CAT }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        UpdateFeralSwiftness(GetTarget(), GetTarget()->GetShapeshiftForm());
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_CWD_FERAL_SWIFTNESS_CAT);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_talent_dru_feral_swiftness::HandleApply, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL_OR_REAPPLY_MASK);
        AfterEffectRemove += AuraEffectRemoveFn(classic_talent_dru_feral_swiftness::HandleRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 16850 - Improved Starfire: "Reduces the cast time of your Starfire spell by $s1 sec and gives you a $s2% chance to stun the target for 3 sec."
// Effect 0 is the cast time modifier; effect 1 is a proc trigger spell (stun 16922) with a proc chance of 100 and the talent rank in its amount,
// so it stunned on every Starfire that reached it, or never. The chance is rolled here (Starfire: druid family flag 0x4).
class classic_spell_dru_improved_starfire : public AuraScript
{
    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        AuraEffect const* chance = GetEffect(EFFECT_1);
        Unit* target = eventInfo.GetActionTarget();
        if (!spellInfo || !chance || !target || target == GetTarget() || !target->IsAlive())
            return false;

        if (spellInfo->SpellFamilyName != SPELLFAMILY_DRUID || !(spellInfo->SpellFamilyFlags[0] & 0x00000004))
            return false;

        return roll_chance(chance->GetAmountAsInt());
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_dru_improved_starfire::CheckProc);
    }
};

// 16929 - Thick Hide: keeps the form armor up to date (level up, defense skill, talent learned or removed while shapeshifted)
class classic_talent_dru_thick_hide : public AuraScript
{
    void Refresh()
    {
        // GetUnitOwner, not GetTarget: on the heartbeat there is no current aura application (GetTarget crashed there)
        Unit* druid = GetUnitOwner();
        if (!druid || !druid->IsInWorld())
            return;

        druid->m_Events.AddEventAtOffset([druid]() { UpdateThickHide(druid); }, 0ms);
    }

    void HandleApplyOrRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Refresh();
    }

    void HandleHeartbeat()
    {
        Refresh();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_talent_dru_thick_hide::HandleApplyOrRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_talent_dru_thick_hide::HandleApplyOrRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnHeartbeat += AuraHeartbeatFn(classic_talent_dru_thick_hide::HandleHeartbeat);
    }
};

// 16972 - Predatory Strikes: keeps the feral attack power up to date (talent learned while shapeshifted, level up)
class classic_talent_dru_predatory_strikes : public AuraScript
{
    void Refresh()
    {
        Unit* druid = GetUnitOwner();
        if (!druid || !druid->IsInWorld())
            return;

        druid->m_Events.AddEventAtOffset([druid]() { UpdatePredatoryStrikes(druid); }, 0ms);
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Refresh();
    }

    void HandleHeartbeat()
    {
        Refresh();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_talent_dru_predatory_strikes::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnHeartbeat += AuraHeartbeatFn(classic_talent_dru_predatory_strikes::HandleHeartbeat);
    }
};

// 16958 - Blood Frenzy (Primal Fury in vanilla): "Gives you a $m1% chance to gain an additional $/10;16959s1 Rage any time you get a critical
// strike while in Bear Form or Dire Bear Form. In addition, your non-periodic critical strikes from Cat Form abilities that generate Combo
// Points have a $m2% chance to add an additional Combo Point." (proc: melee swing + melee ability)
class classic_talent_dru_blood_frenzy : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_BLOOD_FRENZY_RAGE });
    }

    static bool GeneratesComboPoints(SpellInfo const* spellInfo)
    {
        // Claw, Shred, Rake, Ravage, Pounce: energize combo points (Spell::EffectEnergize)
        for (SpellEffectInfo const& effect : spellInfo->GetEffects())
            if (effect.IsEffect(SPELL_EFFECT_ENERGIZE) && effect.MiscValue == POWER_COMBO_POINTS)
                return true;
        return false;
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (!(eventInfo.GetHitMask() & PROC_HIT_CRITICAL) || !eventInfo.GetActionTarget())
            return false;

        switch (GetTarget()->GetShapeshiftForm())
        {
            case FORM_BEAR_FORM:
            case FORM_DIRE_BEAR_FORM:
                return true;
            case FORM_CAT_FORM:
            {
                SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
                return GetTarget()->IsPlayer() && spellInfo && !(eventInfo.GetTypeMask() & PROC_FLAG_DEAL_HARMFUL_PERIODIC)
                    && GeneratesComboPoints(spellInfo);
            }
            default:
                return false;
        }
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* druid = GetTarget();
        if (druid->GetShapeshiftForm() == FORM_CAT_FORM)
        {
            AuraEffect const* comboChance = GetEffect(EFFECT_1);
            if (!comboChance || !roll_chance(comboChance->GetAmount()))
                return;

            // combo points belong to one target (game rule 112, Spell::EffectEnergize)
            Unit* target = eventInfo.GetActionTarget();
            if (druid->GetClassicComboTarget() != target->GetGUID())
            {
                druid->SetPower(POWER_COMBO_POINTS, 0);
                druid->SetClassicComboTarget(target->GetGUID());
            }
            druid->SetPower(POWER_COMBO_POINTS, druid->GetPower(POWER_COMBO_POINTS) + 1);
            return;
        }

        if (roll_chance(aurEff->GetAmount()))
            druid->CastSpell(druid, SPELL_CWD_BLOOD_FRENZY_RAGE, CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_talent_dru_blood_frenzy::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_talent_dru_blood_frenzy::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 408248 - Eclipse: "Your Wrath spell reduces the cast time of your next $s3 Starfire spells by ${$m2/-1000}.2 sec. Stores up to $408255u
// charges. Lasts $408255d." Each Wrath hit adds $s3 stacks of 408255 (max stack 4, the buff effect 0 is the per charge cast time change
// in ms, multiplied by the stacks: "Your next $m2 Starfire casts have ${$w1/$w2/1000}.1 sec reduced cast time").
class classic_talent_dru_eclipse : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_ECLIPSE_BUFF });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        // Wrath: druid family flag 0x1
        return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_DRUID && (spellInfo->SpellFamilyFlags[0] & 0x1);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();

        Unit* druid = GetTarget();
        SpellInfo const* buffInfo = sSpellMgr->AssertSpellInfo(SPELL_CWD_ECLIPSE_BUFF, GetCastDifficulty());

        AuraEffect const* reductionEffect = GetEffect(EFFECT_1);
        AuraEffect const* chargesEffect = GetEffect(EFFECT_2);
        int32 const charges = chargesEffect && chargesEffect->GetAmount() > 0 ? chargesEffect->GetAmountAsInt() : 2;

        // per charge cast time change in ms (negative); without a usable talent value keep the client buff value
        SpellEffectValue perCharge = buffInfo->GetEffect(EFFECT_0).CalcValue(druid);
        if (reductionEffect && std::abs(reductionEffect->GetAmount()) >= 100.0)
            perCharge = -std::abs(reductionEffect->GetAmount());

        Aura* buff = druid->GetAura(SPELL_CWD_ECLIPSE_BUFF, druid->GetGUID());
        uint32 const stacks = buff ? buff->GetStackAmount() : 0;
        uint32 const maxStacks = buffInfo->StackAmount ? buffInfo->StackAmount : 4;
        uint32 const newStacks = std::min<uint32>(maxStacks, stacks + uint32(charges));

        if (!buff)
        {
            druid->CastSpell(druid, SPELL_CWD_ECLIPSE_BUFF, CastSpellExtraArgs(aurEff).AddSpellBP0(perCharge));
            buff = druid->GetAura(SPELL_CWD_ECLIPSE_BUFF, druid->GetGUID());
            if (!buff)
                return;
        }

        if (buff->GetStackAmount() != newStacks)
            buff->SetStackAmount(uint8(newStacks));
        buff->RefreshDuration();
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_talent_dru_eclipse::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_talent_dru_eclipse::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 408255 - Eclipse (buff): the client data has a proc charge on any harmful spell; the Starfire script consumes the stacks instead.
// Effect 0 is a plain dummy (-500 ms per Starfire), so the client did not know about it: the cast bar showed 2.5 sec (the
// server sends the real cast time) but the Starfire button tooltip stayed at 3.0 sec. While the buff is up a player gets a
// flat cast time spell modifier on the Starfire class flag (druid family flag 0x4), which the client also uses for the
// tooltip. The server applies the same modifier to the real cast time, so the Starfire script no longer subtracts it.
class classic_talent_dru_eclipse_buff : public AuraScript
{
public:
    ~classic_talent_dru_eclipse_buff()
    {
        delete _castTimeMod;
    }

private:
    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        return false;
    }

    void HandleApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        Player* player = GetTarget()->ToPlayer();
        if (!player || _castTimeMod)
            return;

        SpellFlatModifierByClassMask* mod = new SpellFlatModifierByClassMask(SpellModOp::ChangeCastTime, GetId(), GetAura(),
            flag128(CWD_STARFIRE_FAMILY_FLAG_0, 0, 0, 0));
        mod->value = aurEff->GetAmountAsInt();
        _castTimeMod = mod;
        player->AddSpellMod(_castTimeMod, true);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (!_castTimeMod)
            return;

        if (Player* player = GetTarget()->ToPlayer())
            player->AddSpellMod(_castTimeMod, false);

        delete _castTimeMod;
        _castTimeMod = nullptr;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_talent_dru_eclipse_buff::CheckProc);
        AfterEffectApply += AuraEffectApplyFn(classic_talent_dru_eclipse_buff::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_talent_dru_eclipse_buff::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }

    SpellModifier* _castTimeMod = nullptr;
};

// 2912, 8949, 8950, 8951, 9875, 9876, 25298 - Starfire: one 408255 Eclipse charge reduces the cast time. For players the
// reduction comes from the buff's cast time spell modifier (see classic_talent_dru_eclipse_buff); other casters get it here.
class classic_talent_dru_eclipse_starfire : public SpellScript
{
    int32 CalcCastTime(int32 castTime) override
    {
        Unit* caster = GetCaster();
        if (AuraEffect const* buff = caster->GetAuraEffect(SPELL_CWD_ECLIPSE_BUFF, EFFECT_0, caster->GetGUID()))
        {
            _usesCharge = true;
            if (!caster->IsPlayer())
                castTime = std::max(0, castTime + int32(buff->GetBaseAmount()));
        }
        return castTime;
    }

    void HandleAfterCast()
    {
        if (!_usesCharge)
            return;

        Unit* caster = GetCaster();
        if (Aura* buff = caster->GetAura(SPELL_CWD_ECLIPSE_BUFF, caster->GetGUID()))
            buff->ModStackAmount(-1);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(classic_talent_dru_eclipse_starfire::HandleAfterCast);
    }

    bool _usesCharge = false;
};

// 408258 - Dreamstate: "Your damaging non-periodic spell critical strikes grant you $m1% of your mana regeneration while casting for
// $408261d." (408261: SPELL_AURA_MOD_MANA_REGEN_INTERRUPT)
class classic_talent_dru_dreamstate : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_DREAMSTATE_BUFF });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        return eventInfo.GetSpellInfo() && damageInfo && damageInfo->GetDamage() && (eventInfo.GetHitMask() & PROC_HIT_CRITICAL)
            && !(eventInfo.GetTypeMask() & PROC_FLAG_DEAL_HARMFUL_PERIODIC);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(GetTarget(), SPELL_CWD_DREAMSTATE_BUFF, CastSpellExtraArgs(aurEff).AddSpellBP0(aurEff->GetAmount()));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_talent_dru_dreamstate::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_talent_dru_dreamstate::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 5217 - Tiger's Fury
// 417046 - King of the Jungle: "Tiger's Fury now instantly grants you $s1 Energy." (417045: energize energy)
class classic_talent_dru_king_of_the_jungle : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CWD_KING_OF_THE_JUNGLE_ENERGIZE });
    }

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        if (AuraEffect const* talent = caster->GetAuraEffect(SPELL_CWD_KING_OF_THE_JUNGLE, EFFECT_0))
            if (talent->GetAmount() > 0.0)
                caster->CastSpell(caster, SPELL_CWD_KING_OF_THE_JUNGLE_ENERGIZE, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(talent->GetAmount()));
    }

    void Register() override
    {
        AfterCast += SpellCastFn(classic_talent_dru_king_of_the_jungle::HandleAfterCast);
    }
};

// 22568, 22827, 22828, 22829, 31018 - Ferocious Bite: "Finishing move that causes damage per combo point and converts each extra point of
// energy into ($f1+$AP/410) additional damage." The client spell has the damage effect (base points per combo point) and a dummy effect 1
// holding the energy factor in percent (100 / 150 / 200 / 250 / 270); nothing read it, so the extra energy was never converted into damage
// and stayed in the pool. Added to the damage effect before the damage bonuses: the energy left after the 35 energy cost times the factor,
// plus attack power * 3% per combo point; the energy is used up. Behaviour from vMaNGOS (GPL-2.0, src/scripts/spells/spell_druid.cpp,
// DruidFerociousBiteScript: patch 1.12 "AP * 3% * combo + energy * factor + damage"). The retail script bound to the first rank needs a
// retail spell.
class classic_spell_dru_ferocious_bite : public SpellScript
{
    static constexpr float ATTACK_POWER_PER_COMBO_POINT = 0.03f;    // vMaNGOS: 3% of attack power per combo point (patch 1.12)

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } }) && spellInfo->GetEffect(EFFECT_1).Effect == SPELL_EFFECT_DUMMY;
    }

    void HandleLaunchDamage(SpellEffIndex /*effIndex*/)
    {
        Unit* druid = GetCaster();
        // the combo points are spent after the effects (Spell::SpendPendingComboPoints), so they are still on the druid here
        int32 const comboPoints = druid->GetPower(POWER_COMBO_POINTS);
        int32 const energy = druid->GetPower(POWER_ENERGY);
        SpellEffectValue bonus = druid->GetTotalAttackPowerValue(BASE_ATTACK) * comboPoints * ATTACK_POWER_PER_COMBO_POINT;
        bonus += energy * (GetEffectInfo(EFFECT_1).CalcValue(druid) / 100.0);
        TC_LOG_INFO("scripts.classic", "Ferocious Bite {}: combo points {}, energy left {}, energy factor {}%, attack power {}, base damage {}, bonus {}",
            GetSpellInfo()->Id, comboPoints, energy, GetEffectInfo(EFFECT_1).CalcValue(druid), druid->GetTotalAttackPowerValue(BASE_ATTACK),
            GetEffectValue(), bonus);
        SetEffectValue(GetEffectValue() + bonus);
        druid->SetPower(POWER_ENERGY, 0);
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(classic_spell_dru_ferocious_bite::HandleLaunchDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 1079, 9492, 9493, 9752, 9894, 9896 - Rip: patch 1.12 "now increases in potency with greater attack power". Behaviour from vMaNGOS
// (GPL-2.0, src/game/Spells/SpellAuras.cpp, periodic damage, SPELLFAMILY_DRUID Rip): every tick deals attack power * combo points / 100 more,
// the combo points counting up to 4 (a fifth adds nothing). The combo points are those the cast spends, read when the aura is created.
class classic_spell_dru_rip : public AuraScript
{
    static constexpr int32 MAX_COMBO_POINTS_FOR_ATTACK_POWER = 4;   // vMaNGOS caps the attack power part at 4 combo points

    void CalculateAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& canBeRecalculated)
    {
        canBeRecalculated = false;
        Unit* druid = GetCaster();
        if (!druid || !druid->IsPlayer())
            return;

        int32 const comboPoints = std::min<int32>(druid->GetPower(POWER_COMBO_POINTS), MAX_COMBO_POINTS_FOR_ATTACK_POWER);
        amount += druid->GetTotalAttackPowerValue(BASE_ATTACK) * comboPoints / 100.0;
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(classic_spell_dru_rip::CalculateAmount, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// Entangling Roots (339, 1062, 5195, 5196, 9852, 9853) with 17245 Overgrowth (70532): "You may $?s17245[have up to ${$17245m1+1} targets][only
// have 1 target] Rooted at a time." Nothing limited the targets. Each druid's rooted targets are kept in order of rooting; when a new one is
// rooted beyond 1 + the Overgrowth amount, the Roots on the oldest is removed. Nature's Grasp roots (19970-19975) do not have the limit.
class classic_spell_dru_entangling_roots_limit : public AuraScript
{
    struct RootedTarget
    {
        ObjectGuid Target;
        uint32 SpellId;
    };

    static std::mutex _lock;
    static std::unordered_map<ObjectGuid, std::vector<RootedTarget>> _rootedByCaster;

    void AfterApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* druid = GetCaster();
        if (!druid)
            return;

        uint32 limit = 1;
        if (AuraEffect const* overgrowth = druid->GetAuraEffect(SPELL_CWD_OVERGROWTH, EFFECT_0))
            limit += uint32(std::max(overgrowth->GetAmountAsInt(), 0));

        ObjectGuid const targetGuid = GetTarget()->GetGUID();
        std::vector<RootedTarget> released;
        {
            std::lock_guard<std::mutex> guard(_lock);
            std::vector<RootedTarget>& rooted = _rootedByCaster[druid->GetGUID()];
            std::erase_if(rooted, [&](RootedTarget const& entry) { return entry.Target == targetGuid; });
            rooted.push_back({ targetGuid, GetId() });
            while (rooted.size() > limit)
            {
                released.push_back(rooted.front());
                rooted.erase(rooted.begin());
            }
        }

        // removing them runs their AfterRemove (which takes the lock), so outside of it
        for (RootedTarget const& entry : released)
            if (Unit* oldTarget = ObjectAccessor::GetUnit(*druid, entry.Target))
                oldTarget->RemoveAurasDueToSpell(entry.SpellId, druid->GetGUID());
    }

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        ObjectGuid const targetGuid = GetTarget()->GetGUID();
        uint32 const spellId = GetId();
        std::lock_guard<std::mutex> guard(_lock);
        auto itr = _rootedByCaster.find(GetCasterGUID());
        if (itr == _rootedByCaster.end())
            return;

        std::erase_if(itr->second, [&](RootedTarget const& entry) { return entry.Target == targetGuid && entry.SpellId == spellId; });
        if (itr->second.empty())
            _rootedByCaster.erase(itr);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_dru_entangling_roots_limit::AfterApply, EFFECT_0, SPELL_AURA_MOD_ROOT, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_dru_entangling_roots_limit::AfterRemove, EFFECT_0, SPELL_AURA_MOD_ROOT, AURA_EFFECT_HANDLE_REAL);
    }
};

std::mutex classic_spell_dru_entangling_roots_limit::_lock;
std::unordered_map<ObjectGuid, std::vector<classic_spell_dru_entangling_roots_limit::RootedTarget>> classic_spell_dru_entangling_roots_limit::_rootedByCaster;

// 18562 - Swiftmend: "Consumes a Rejuvenation or Regrowth effect on a friendly target to instantly heal them an amount equal to 12 sec of
// Rejuvenation or 18 sec of Regrowth." The client spell is a plain heal of 1 and consumed nothing. The heal is now the periodic effect's
// base amount times the ticks of 12 / 18 sec, the effect is removed, and the cast needs one of the two from the druid on the target.
class classic_spell_dru_swiftmend : public SpellScript
{
    static constexpr uint32 FAMILY_REJUVENATION = 0x10;
    static constexpr uint32 FAMILY_REGROWTH = 0x40;

    // vMaNGOS (GPL-2.0, src/scripts/spells/spell_druid.cpp, DruidSwiftmendScript): the Rejuvenation or Regrowth with the shortest duration
    // left is consumed, the heal is its tick amount times 4 ticks ("12 sec of Rejuvenation") or 6 ticks ("18 sec of Regrowth")
    static constexpr int32 TICKS_REJUVENATION = 4;
    static constexpr int32 TICKS_REGROWTH = 6;

    static AuraEffect const* FindHot(Unit const* druid, Unit const* target)
    {
        AuraEffect const* best = nullptr;
        for (AuraEffect const* effect : target->GetAuraEffectsByType(SPELL_AURA_PERIODIC_HEAL))
        {
            SpellInfo const* info = effect->GetSpellInfo();
            if (effect->GetCasterGUID() == druid->GetGUID() && info->SpellFamilyName == SPELLFAMILY_DRUID
                && (info->SpellFamilyFlags[0] & (FAMILY_REJUVENATION | FAMILY_REGROWTH))
                && (!best || effect->GetBase()->GetDuration() < best->GetBase()->GetDuration()))
                best = effect;
        }
        return best;
    }

    SpellCastResult CheckCast()
    {
        Unit* target = GetExplTargetUnit();
        if (!target || !FindHot(GetCaster(), target))
            return SPELL_FAILED_BAD_TARGETS;
        return SPELL_CAST_OK;
    }

    void HandleLaunchHeal(SpellEffIndex /*effIndex*/)
    {
        Unit* target = GetHitUnit();
        AuraEffect const* hot = target ? FindHot(GetCaster(), target) : nullptr;
        if (!hot)
            return;

        int32 ticks = (hot->GetSpellInfo()->SpellFamilyFlags[0] & FAMILY_REJUVENATION) ? TICKS_REJUVENATION : TICKS_REGROWTH;
        SetEffectValue(hot->GetAmount() * ticks);
        _consumedSpell = hot->GetId();
    }

    void HandleHitHeal(SpellEffIndex /*effIndex*/)
    {
        if (_consumedSpell)
            if (Unit* target = GetHitUnit())
                target->RemoveAurasDueToSpell(_consumedSpell, GetCaster()->GetGUID());
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_dru_swiftmend::CheckCast);
        OnEffectLaunchTarget += SpellEffectFn(classic_spell_dru_swiftmend::HandleLaunchHeal, EFFECT_0, SPELL_EFFECT_HEAL);
        OnEffectHitTarget += SpellEffectFn(classic_spell_dru_swiftmend::HandleHitHeal, EFFECT_0, SPELL_EFFECT_HEAL);
    }

private:
    uint32 _consumedSpell = 0;
};

// 22842 - Frenzied Regeneration: "Converts up to 10 rage per second into health for 10 sec. Each point of rage is converted into 10 health."
// The client aura is a periodic trigger spell effect (1 s) without a trigger spell, so it never did anything; the heal spell 22845
// exists (heal effect, base points 0). Every tick takes up to 10 rage and casts 22845 for 10 health per rage point (power values are in
// tenths). The vanilla rule is flat health per rage (the percent of maximum health came with a later patch).
class classic_spell_dru_frenzied_regeneration : public AuraScript
{
    static constexpr int32 RAGE_PER_TICK = 100;     // 10 rage
    static constexpr int32 HEALTH_PER_RAGE = 10;
    static constexpr uint32 SPELL_FRENZIED_REGENERATION_HEAL = 22845;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_FRENZIED_REGENERATION_HEAL });
    }

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        PreventDefaultAction();

        Unit* druid = GetTarget();
        int32 rage = std::min<int32>(RAGE_PER_TICK, druid->GetPower(POWER_RAGE));
        if (rage <= 0)
            return;

        druid->ModifyPower(POWER_RAGE, -rage);
        int32 heal = (rage / 10) * HEALTH_PER_RAGE;
        druid->CastSpell(druid, SPELL_FRENZIED_REGENERATION_HEAL, CastSpellExtraArgs(aurEff).AddSpellBP0(heal));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_dru_frenzied_regeneration::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

/*######
## Damage hooks without a client aura effect
######*/

// 1223246 - Rend and Tear (druid): "Increases damage done by your melee abilities on Bleeding targets by $m1%."
// 426301 - Grimoire of Synergy (warlock): "Damage done by you or your summoned demon has a $h% chance to increase the damage done by the
// other by $426303s1% for $426303d." The talent spell is an area aura owner effect on the pet, so the warlock side is checked by spell.
class classic_unit_talent_warlock_druid : public UnitScript
{
public:
    classic_unit_talent_warlock_druid() : UnitScript("classic_unit_talent_warlock_druid") { }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!target || !attacker || !spellInfo || damage <= 0 || spellInfo->DmgClass != SPELL_DAMAGE_CLASS_MELEE || !attacker->IsPlayer())
            return;

        AuraEffect const* talent = attacker->GetAuraEffect(SPELL_CWD_REND_AND_TEAR, EFFECT_0);
        if (!talent || talent->GetAmount() <= 0.0)
            return;

        if (target->HasAuraWithMechanic(UI64LIT(1) << MECHANIC_BLEED))
            AddPct(damage, talent->GetAmount());
    }

    static bool HasGrimoireOfSynergy(Player const* warlock)
    {
        return warlock->HasAura(SPELL_CWD_GRIMOIRE_OF_SYNERGY) || warlock->HasSpell(SPELL_CWD_GRIMOIRE_OF_SYNERGY);
    }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!attacker || !victim || !damage || attacker == victim)
            return;

        Unit* other = nullptr;
        if (Player* warlock = attacker->ToPlayer())
        {
            if (warlock->GetClass() != CLASS_WARLOCK || !HasGrimoireOfSynergy(warlock))
                return;
            other = GetActiveDemon(warlock);
        }
        else if (attacker->IsPet())
        {
            Player* warlock = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
            if (!warlock || warlock == attacker || warlock->GetClass() != CLASS_WARLOCK || warlock->GetPet() != attacker
                || GetDemonKind(attacker->GetEntry()) == DemonKind::None || !HasGrimoireOfSynergy(warlock))
                return;
            other = warlock;
        }

        if (!other || !other->IsAlive() || other == victim)
            return;

        SpellInfo const* talentInfo = sSpellMgr->GetSpellInfo(SPELL_CWD_GRIMOIRE_OF_SYNERGY, DIFFICULTY_NONE);
        float const chance = talentInfo && talentInfo->ProcChance ? float(talentInfo->ProcChance) : 10.0f;
        if (!roll_chance(chance))
            return;

        attacker->CastSpell(other, SPELL_CWD_GRIMOIRE_OF_SYNERGY_BUFF, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }
};

// 1328621 - Bloom (Forever druid talent): "When your Rejuvenation or Regrowth spell expires due to its duration, the target is
// instantly healed for an amount equal to $s1% of your Spirit. If either spell expires due to being dispelled, the target is healed
// for $s2% of your Spirit instead." Bound to the Rejuvenation and Regrowth ranks; the heal is 1328622.
class classic_spell_dru_bloom : public AuraScript
{
    static constexpr uint32 SPELL_DRU_BLOOM      = 1328621;
    static constexpr uint32 SPELL_DRU_BLOOM_HEAL = 1328622;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_DRU_BLOOM, EFFECT_1 } }) && ValidateSpellInfo({ SPELL_DRU_BLOOM_HEAL });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        AuraRemoveMode const removeMode = GetTargetApplication()->GetRemoveMode();
        SpellEffIndex talentEffect;
        if (removeMode == AURA_REMOVE_BY_EXPIRE)
            talentEffect = EFFECT_0;
        else if (removeMode == AURA_REMOVE_BY_ENEMY_SPELL)
            talentEffect = EFFECT_1;
        else
            return;

        Unit* druid = GetCaster();
        Unit* target = GetTarget();
        if (!druid || !target->IsAlive())
            return;

        AuraEffect const* talent = druid->GetAuraEffect(SPELL_DRU_BLOOM, talentEffect);
        if (!talent)
            return;

        int32 const amount = int32(CalculatePct(druid->GetStat(STAT_SPIRIT), talent->GetAmount()));
        if (amount <= 0)
            return;

        // the heal spell targets its caster, so the target casts it on itself with the druid as the original caster
        CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, amount);
        args.SetOriginalCaster(druid->GetGUID());
        target->CastSpell(target, SPELL_DRU_BLOOM_HEAL, args);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_dru_bloom::HandleRemove, EFFECT_ALL, SPELL_AURA_PERIODIC_HEAL, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_classic_talents_warlock_druid()
{
    RegisterSpellScript(classic_spell_dru_bloom);
    // warlock
    RegisterSpellScript(classic_talent_warl_life_tap);
    RegisterSpellScript(classic_talent_warl_demonic_energies);
    RegisterSpellScript(classic_talent_warl_improved_shadow_bolt);
    RegisterSpellScript(classic_talent_warl_nightfall);
    RegisterSpellScript(classic_talent_warl_pyroclasm);
    RegisterSpellScript(classic_spell_warl_curse_of_agony);
    RegisterSpellScript(classic_talent_warl_soul_siphon);
    RegisterSpellScript(classic_talent_warl_soul_harvest);
    RegisterSpellScript(classic_talent_warl_demon_talent_sync);
    RegisterSpellScript(classic_talent_warl_summon_demon);
    RegisterSpellScript(classic_talent_warl_demonic_sacrifice);
    RegisterSpellScript(classic_talent_warl_conflagrate);
    RegisterSpellScript(classic_spell_warl_incinerate);
    RegisterSpellScript(classic_talent_warl_decimation);
    RegisterSpellScript(classic_spell_warl_decimation);
    RegisterSpellScript(classic_talent_warl_shadowburn);
    RegisterSpellScript(classic_talent_warl_bane_of_havoc_tracker);
    RegisterSpellScript(classic_talent_warl_bane_of_havoc);
    RegisterSpellScript(classic_talent_warl_create_healthstone);

    // druid
    RegisterSpellScript(classic_talent_dru_form_talents);
    RegisterSpellScript(classic_talent_dru_thick_hide);
    RegisterSpellScript(classic_talent_dru_leader_of_the_pack);
    RegisterSpellScript(classic_talent_dru_heart_of_the_wild);
    RegisterSpellScript(classic_talent_dru_feral_swiftness);
    RegisterSpellScript(classic_talent_dru_natural_reaction);
    RegisterSpellScript(classic_spell_dru_improved_starfire);
    RegisterSpellScript(classic_talent_dru_blood_frenzy);
    RegisterSpellScript(classic_talent_dru_eclipse);
    RegisterSpellScript(classic_talent_dru_eclipse_buff);
    RegisterSpellScript(classic_talent_dru_eclipse_starfire);
    RegisterSpellScript(classic_talent_dru_dreamstate);
    RegisterSpellScript(classic_talent_dru_king_of_the_jungle);
    RegisterSpellScript(classic_spell_dru_ferocious_bite);
    RegisterSpellScript(classic_spell_dru_frenzied_regeneration);
    RegisterSpellScript(classic_spell_dru_swiftmend);
    RegisterSpellScript(classic_spell_dru_rip);
    RegisterSpellScript(classic_spell_dru_entangling_roots_limit);
    RegisterSpellScript(classic_talent_dru_predatory_strikes);

    new classic_unit_talent_warlock_druid();
}
