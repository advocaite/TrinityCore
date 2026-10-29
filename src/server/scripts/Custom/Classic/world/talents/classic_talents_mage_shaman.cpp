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

// Classic 1.60 (WoW Forever) mage and shaman talents. In the client data these talents are dummy auras (most with proc data) and
// have no server side behaviour; the scripts below implement the Forever tooltips. Rank values come from the talent's aura
// amounts (the core applies the trait rank curve), never from hardcoded ranks.

#include "ScriptMgr.h"
#include "AreaTrigger.h"
#include "CellImpl.h"
#include "Creature.h"
#include "GridNotifiersImpl.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "Unit.h"

enum ClassicMageTalentSpells
{
    SPELL_MAGE_IGNITE_TALENT                = 11119,
    SPELL_MAGE_IGNITE_DOT                   = 412538,   // periodic dummy, 2 s ticks for 4 s (1.60 replacement of 12654)
    SPELL_MAGE_SHATTER                      = 11170,
    SPELL_MAGE_IMPROVED_BLIZZARD            = 11185,
    SPELL_MAGE_CHILLED                      = 12484,    // Improved Blizzard chill, 1.5 s
    SPELL_MAGE_IMPROVED_COUNTERSPELL        = 11255,
    SPELL_MAGE_COUNTERSPELL_SILENCED        = 18469,
    SPELL_MAGE_HEATING_UP_BUFF              = 400625,   // -25% Pyroblast cast time per stack, 3 stacks, 20 s, 1 charge
    SPELL_MAGE_FINGERS_OF_FROST_TALENT      = 400647,   // effect 0: charges, effect 1: proc chance
    SPELL_MAGE_FINGERS_OF_FROST_IGNORE      = 400669,   // ability ignore aura state (frozen) for the spells that need it
    SPELL_MAGE_FINGERS_OF_FROST_STACKS      = 400670    // 2 stacks, one used per spell cast
};

enum ClassicShamanTalentSpells
{
    SPELL_SHAMAN_FLURRY_BUFF                = 16257,
    SPELL_SHAMAN_ELEMENTAL_DEVASTATION_BUFF = 30165,
    SPELL_SHAMAN_MAELSTROM_WEAPON_TALENT    = 408498,   // effect 1: proc chance, effect 2: max stacks
    SPELL_SHAMAN_MAELSTROM_WEAPON_BUFF      = 408505,
    SPELL_SHAMAN_MAELSTROM_READY            = 408501,
    SPELL_SHAMAN_LIGHTNING_BOLT_MAELSTROM   = 1291078,  // lets Lightning Bolt hits activate Maelstrom Weapon (effect 0: chance)
    SPELL_SHAMAN_EARTHBOUND_TALENT          = 1222988,
    SPELL_SHAMAN_EARTHBOUND_ROOT            = 1238289,
    SPELL_SHAMAN_IMPROVED_STORMSTRIKE       = 1223031,  // effect 0: buff chance, effect 2: cooldown reset chance
    SPELL_SHAMAN_IMPROVED_STORMSTRIKE_BUFF  = 1238931,  // 50% mana regeneration while casting, 15 s
    SPELL_SHAMAN_STORMSTRIKE                = 17364,
    SPELL_SHAMAN_ANCESTRAL_GUIDANCE_HEAL    = 409333,
    SPELL_SHAMAN_ANCESTRAL_GUIDANCE_DAMAGE  = 409337
};

// Mage "Chill effects" (Frostbolt, Cone of Cold, Frostfire Bolt, Blast Wave...): mage spells with a snare. Triggered chills do not
// proc (Frost Armor / Ice Armor chill); the Improved Blizzard chill rolls directly.
static bool IsMageChillEffect(SpellInfo const* spellInfo)
{
    return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_MAGE && spellInfo->HasAura(SPELL_AURA_MOD_DECREASE_SPEED);
}

// 400647 - Fingers of Frost: a Chill effect has effect 1 % chance to grant effect 0 charges of Fingers of Frost for 15 s
static void TryGrantFingersOfFrost(Unit* mage)
{
    Aura const* talent = mage->GetAura(SPELL_MAGE_FINGERS_OF_FROST_TALENT);
    if (!talent)
        return;

    AuraEffect const* charges = talent->GetEffect(EFFECT_0);
    AuraEffect const* chance = talent->GetEffect(EFFECT_1);
    if (!charges || !chance || !roll_chance(chance->GetAmount()))
        return;

    mage->CastSpell(mage, SPELL_MAGE_FINGERS_OF_FROST_IGNORE, true);
    mage->CastSpell(mage, SPELL_MAGE_FINGERS_OF_FROST_STACKS, CastSpellExtraArgs(TRIGGERED_FULL_MASK)
        .AddSpellMod(SPELLVALUE_AURA_STACK, std::max(1, charges->GetAmountAsInt())));
}

/*######
## Mage
######*/

// 11119 - Ignite: "Your critical strikes from Fire damage spells cause the target to burn for an additional 40% of your spell's
// damage over 4 sec." Vanilla 12654 is not in the 1.60 client; 412538 (Ignite, periodic dummy 2 s / 4 s) is the DoT. The
// remaining damage of a running Ignite rolls into the new one (spell_proc: fire, harmful spell, damage, hit, critical).
class classic_spell_mage_ignite : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_MAGE_IGNITE_DOT, EFFECT_0 } });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        return eventInfo.GetActionTarget() && eventInfo.GetDamageInfo() && eventInfo.GetDamageInfo()->GetDamage()
            && (eventInfo.GetDamageInfo()->GetSchoolMask() & SPELL_SCHOOL_MASK_FIRE);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* mage = eventInfo.GetActor();
        Unit* target = eventInfo.GetActionTarget();
        SpellInfo const* dot = sSpellMgr->AssertSpellInfo(SPELL_MAGE_IGNITE_DOT, GetCastDifficulty());
        int32 const period = std::max<int32>(1, dot->GetEffect(EFFECT_0).ApplyAuraPeriod);
        int32 const totalTicks = std::max<int32>(1, dot->GetMaxDuration() / period);

        SpellEffectValue amount = CalculatePct(SpellEffectValue(eventInfo.GetDamageInfo()->GetDamage()), aurEff->GetAmount());
        if (AuraEffect const* existing = target->GetAuraEffect(SPELL_MAGE_IGNITE_DOT, EFFECT_0, mage->GetGUID()))
            amount += existing->GetAmount() * existing->GetRemainingTicks();

        amount /= totalTicks;
        if (amount < 1.0)
            return;

        mage->CastSpell(target, SPELL_MAGE_IGNITE_DOT, CastSpellExtraArgs(aurEff).AddSpellBP0(amount));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_mage_ignite::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_mage_ignite::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 412538 - Ignite (DoT): every tick deals its amount as Fire damage (already modified by the critical spell, no bonuses again)
class classic_spell_mage_ignite_dot : public AuraScript
{
    void HandleTick(AuraEffect const* aurEff)
    {
        Unit* caster = GetCaster();
        Unit* target = GetTarget();
        if (!caster || !target->IsAlive())
            return;

        int32 const damage = aurEff->GetAmountAsInt();
        if (damage <= 0)
            return;

        SpellInfo const* spellInfo = GetSpellInfo();
        SpellNonMeleeDamage damageInfo(caster, target, spellInfo, GetAura()->GetSpellVisual(), spellInfo->GetSchoolMask(), GetAura()->GetCastId());
        damageInfo.periodicLog = true;
        caster->CalculateSpellDamageTaken(&damageInfo, damage, spellInfo);
        Unit::DealDamageMods(damageInfo.attacker, damageInfo.target, damageInfo.damage, &damageInfo.absorb);
        caster->SendSpellNonMeleeDamageLog(&damageInfo);
        caster->DealSpellDamage(&damageInfo, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_mage_ignite_dot::HandleTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 11170 - Shatter: "Increases the critical strike chance of all your spells against Frozen targets by 1%" (rank amount).
// 400647 - Fingers of Frost: "treats your next 2 spells cast as if the target were Frozen": every cast mage damage spell uses
// one charge of 400670 and counts as hitting a Frozen target (for Shatter). Bound to all player mage damage spells.
class classic_spell_mage_frozen_target : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MAGE_FINGERS_OF_FROST_STACKS });
    }

    void HandleCast()
    {
        // launch (critical roll) happens after OnCast, so the charge is used here and remembered for this cast
        if (GetSpell()->IsTriggered())
            return;

        if (Aura* fingers = GetCaster()->GetAura(SPELL_MAGE_FINGERS_OF_FROST_STACKS))
        {
            _fingersOfFrost = true;
            fingers->ModStackAmount(-1);
        }
    }

    void CalcCritChance(Unit const* victim, float& critChance)
    {
        Unit* caster = GetCaster();
        AuraEffect const* shatter = caster->GetAuraEffect(SPELL_MAGE_SHATTER, EFFECT_0);
        if (!shatter)
            return;

        bool const frozen = _fingersOfFrost
            || (GetSpell()->IsTriggered() && caster->HasAura(SPELL_MAGE_FINGERS_OF_FROST_STACKS))
            || victim->HasAuraState(AURA_STATE_FROZEN, GetSpellInfo(), caster);
        if (frozen)
            critChance += float(shatter->GetAmount());
    }

    void Register() override
    {
        OnCast += SpellCastFn(classic_spell_mage_frozen_target::HandleCast);
        OnCalcCritChance += SpellOnCalcCritChanceFn(classic_spell_mage_frozen_target::CalcCritChance);
    }

    bool _fingersOfFrost = false;
};

// 400670 - Fingers of Frost (charges): the aura state ignore buff 400669 ends with the last charge
class classic_spell_mage_fingers_of_frost_charges : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MAGE_FINGERS_OF_FROST_IGNORE });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_MAGE_FINGERS_OF_FROST_IGNORE);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_mage_fingers_of_frost_charges::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 400647 - Fingers of Frost: "Gives your Chill effects a 15% chance to grant you the Fingers of Frost effect, which treats your
// next 2 spells cast as if the target were Frozen. Lasts 15 sec." (spell_proc: mage harmful spell hits; the Blizzard chill is
// cast triggered and rolls from classic_spell_mage_improved_blizzard)
class classic_spell_mage_fingers_of_frost : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MAGE_FINGERS_OF_FROST_IGNORE, SPELL_MAGE_FINGERS_OF_FROST_STACKS })
            && ValidateSpellEffect({ { SPELL_MAGE_FINGERS_OF_FROST_TALENT, EFFECT_1 } });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        return IsMageChillEffect(eventInfo.GetSpellInfo());
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        TryGrantFingersOfFrost(GetTarget());
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_mage_fingers_of_frost::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_mage_fingers_of_frost::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 11185 - Improved Blizzard: "Adds a Chill effect to your Blizzard spell. This effect lowers the target's movement speed by 40%
// for 1.5 sec." Blizzard (10, 6141, 8427, 10185-10187) ticks through classic_spell_ground_area_damage; this second script on the
// same tick chills the enemies inside the Blizzard with 12484 (talent amount as the snare), which is a Chill effect for Fingers
// of Frost.
class classic_spell_mage_improved_blizzard : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MAGE_CHILLED, SPELL_MAGE_IMPROVED_BLIZZARD });
    }

    void HandleTick(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        AuraEffect const* talent = caster->GetAuraEffect(SPELL_MAGE_IMPROVED_BLIZZARD, EFFECT_0);
        if (!talent || talent->GetAmount() >= 0)
            return;

        SpellInfo const* spellInfo = GetSpellInfo();
        GuidUnorderedSet targets;
        for (AreaTrigger* areaTrigger : caster->GetAreaTriggers(spellInfo->Id))
            targets.insert(areaTrigger->GetInsideUnits().begin(), areaTrigger->GetInsideUnits().end());

        bool chilled = false;
        for (ObjectGuid const& guid : targets)
        {
            Unit* target = ObjectAccessor::GetUnit(*caster, guid);
            if (!target || !target->IsAlive() || !caster->IsValidAttackTarget(target, spellInfo))
                continue;

            caster->CastSpell(target, SPELL_MAGE_CHILLED, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(talent->GetAmount()));
            chilled = true;
        }

        if (chilled)
            TryGrantFingersOfFrost(caster);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_mage_improved_blizzard::HandleTick, EFFECT_2, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 2139 - Counterspell, with 11255 Improved Counterspell: "Your Counterspell also Silences the target for 4 sec." (18469, the
// talent amount is the duration in ms; vanilla silences on every Counterspell hit)
class classic_spell_mage_counterspell : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MAGE_COUNTERSPELL_SILENCED });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        AuraEffect const* talent = caster->GetAuraEffect(SPELL_MAGE_IMPROVED_COUNTERSPELL, EFFECT_0);
        if (!target || !talent)
            return;

        CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
        if (talent->GetAmountAsInt() > 0)
            args.AddSpellMod(SPELLVALUE_DURATION, talent->GetAmountAsInt());
        caster->CastSpell(target, SPELL_MAGE_COUNTERSPELL_SILENCED, args);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_mage_counterspell::HandleHit, EFFECT_0, SPELL_EFFECT_INTERRUPT_CAST);
    }
};

// 400624 - Heating Up: "Non-periodic critical strikes with Fireball, Frostfire Bolt, Fire Blast, and Scorch reduce the cast time
// of your next Pyroblast cast within 20 sec by 25%, stacking up to 3 times." Adds a stack of 400625 (talent amount = chance);
// 400625 is used by the next Pyroblast (spell_proc: cast phase, requires its spell mod).
class classic_spell_mage_heating_up : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MAGE_HEATING_UP_BUFF });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        if (roll_chance(aurEff->GetAmount()))
            GetTarget()->CastSpell(GetTarget(), SPELL_MAGE_HEATING_UP_BUFF, CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_mage_heating_up::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 11071 - Frostbite: "Gives your Chill effects a $s1% chance to Freeze the target for $12494d." The client data proc chance is
// 100%; the chance is the talent amount, rolled on the mage's Chill effects (spell_proc: mage spell family flag 0x100000, the
// spells Permafrost affects: Frostbolt, Cone of Cold, Frostfire Bolt, Frost Blast, and the Chilled of Frost Armor, Ice Armor and
// Improved Blizzard, which are triggered). The core makes no proc data of its own for this aura (SPELL_ATTR3_CAN_PROC_FROM_PROCS
// with a 100% chance), so without the spell_proc row it never procs.
class classic_spell_mage_frostbite : public AuraScript
{
    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        return roll_chance(GetEffect(EFFECT_0)->GetAmount());
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_mage_frostbite::CheckProc);
    }
};

/*######
## Shaman
######*/

// 16256 - Flurry: "Increases your attack speed by 1% for your next 3 swings after dealing a melee critical strike." 16257 (3
// charges, used by swings) with the talent amount as its haste.
class classic_spell_sha_flurry : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHAMAN_FLURRY_BUFF });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        CastSpellExtraArgs args(aurEff);
        if (aurEff->GetAmount() > 0)
            args.AddSpellBP0(aurEff->GetAmount());
        GetTarget()->CastSpell(GetTarget(), SPELL_SHAMAN_FLURRY_BUFF, args);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_flurry::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 30160 - Elemental Devastation: "Your offensive spell critical strikes will increase your chance to get a critical strike with
// melee attacks by X% for 10 sec." 30165 with the talent amount (its own 3% when the amount is not set).
class classic_spell_sha_elemental_devastation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHAMAN_ELEMENTAL_DEVASTATION_BUFF });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        CastSpellExtraArgs args(aurEff);
        if (aurEff->GetAmount() > 0)
            args.AddSpellBP0(aurEff->GetAmount());
        GetTarget()->CastSpell(GetTarget(), SPELL_SHAMAN_ELEMENTAL_DEVASTATION_BUFF, args);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_elemental_devastation::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 408438 - Lightning Overload: "Gives your Lightning Bolt and Chain Lightning spells a 30% chance to cast a second, similar spell
// on the same target at no additional cost that causes half damage and no threat." The 1.60 overload spells (half damage,
// SPELL_ATTR1_NO_THREAT) per rank; Chain Lightning only rolls on its primary target (the overload chains by itself).
class classic_spell_sha_lightning_overload : public AuraScript
{
    static uint32 GetOverloadSpell(uint32 spellId)
    {
        switch (spellId)
        {
            // Lightning Bolt
            case 403:   return 408439;
            case 529:   return 408440;
            case 548:   return 408441;
            case 915:   return 408442;
            case 943:   return 408443;
            case 6041:  return 408472;
            case 10391: return 408473;
            case 10392: return 408474;
            case 15207: return 408475;
            case 15208: return 408477;
            // Chain Lightning
            case 421:   return 408479;
            case 930:   return 408481;
            case 2860:  return 408482;
            case 10605: return 408484;
            default:    return 0;
        }
    }

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ 408439, 408440, 408441, 408442, 408443, 408472, 408473, 408474, 408475, 408477,
            408479, 408481, 408482, 408484 });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        Spell const* spell = eventInfo.GetProcSpell();
        if (!spellInfo || !spell || spell->IsTriggered() || !GetOverloadSpell(spellInfo->Id) || !eventInfo.GetActionTarget())
            return false;

        // one roll per cast: the explicit target of the bolt / chain
        Unit const* explicitTarget = spell->m_targets.GetUnitTarget();
        return !explicitTarget || explicitTarget == eventInfo.GetActionTarget();
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        if (!roll_chance(aurEff->GetAmount()))
            return;

        eventInfo.GetActor()->CastSpell(eventInfo.GetActionTarget(), GetOverloadSpell(eventInfo.GetSpellInfo()->Id), CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_sha_lightning_overload::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_lightning_overload::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 408498 - Maelstrom Weapon: "When you deal damage with a melee attack, you have a chance to reduce the cast time and Mana cost of
// your next Lightning Bolt spell by 20%. Stacks up to 5 times. Lasts 30 sec." Effect 1 is the chance per melee hit, effect 2 the
// stack count that shows "Maelstrom Ready!" (408501). With 1291078 Lightning Bolt hits can also add a stack (its amount = chance).
// 408505 and 408501 are used by the next Lightning Bolt (spell_proc).
class classic_spell_sha_maelstrom_weapon : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHAMAN_MAELSTROM_WEAPON_BUFF, SPELL_SHAMAN_MAELSTROM_READY })
            && ValidateSpellEffect({ { SPELL_SHAMAN_MAELSTROM_WEAPON_TALENT, EFFECT_2 } });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo const* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return false;

        if (eventInfo.GetTypeMask() & PROC_FLAG_DEAL_MELEE_SWING)
            return true;
        if (eventInfo.GetTypeMask() & PROC_FLAG_DEAL_MELEE_ABILITY)
            return true;

        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_SHAMAN && (spellInfo->SpellFamilyFlags[0] & 0x1)
            && GetTarget()->HasAura(SPELL_SHAMAN_LIGHTNING_BOLT_MAELSTROM);
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* shaman = GetTarget();
        SpellEffectValue chance = GetAura()->GetEffect(EFFECT_1)->GetAmount();
        if (!(eventInfo.GetTypeMask() & PROC_FLAG_DEAL_MELEE_SWING) && !(eventInfo.GetTypeMask() & PROC_FLAG_DEAL_MELEE_ABILITY))
            if (AuraEffect const* lightningBolt = shaman->GetAuraEffect(SPELL_SHAMAN_LIGHTNING_BOLT_MAELSTROM, EFFECT_0))
                chance = lightningBolt->GetAmount();

        if (!roll_chance(chance))
            return;

        shaman->CastSpell(shaman, SPELL_SHAMAN_MAELSTROM_WEAPON_BUFF, true);

        int32 const maxStacks = std::max(1, GetAura()->GetEffect(EFFECT_2)->GetAmountAsInt());
        if (Aura const* buff = shaman->GetAura(SPELL_SHAMAN_MAELSTROM_WEAPON_BUFF))
            if (buff->GetStackAmount() >= maxStacks && !shaman->HasAura(SPELL_SHAMAN_MAELSTROM_READY))
                shaman->CastSpell(shaman, SPELL_SHAMAN_MAELSTROM_READY, true);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_sha_maelstrom_weapon::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_maelstrom_weapon::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 408505 - Maelstrom Weapon (buff): "Maelstrom Ready!" (408501, no duration) ends with the stacks
class classic_spell_sha_maelstrom_weapon_buff : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHAMAN_MAELSTROM_READY });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_SHAMAN_MAELSTROM_READY);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_sha_maelstrom_weapon_buff::HandleRemove, EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1222988 - Earthbound: "Your Earthbind Totem Immobilizes nearby targets for 5 sec when cast." The Earthbind Totem (2630) gets
// its passive 6474 when summoned; then it casts 1238289 (root, 10 yd around the totem).
class classic_spell_sha_earthbind_totem_passive : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHAMAN_EARTHBOUND_ROOT });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* totem = GetTarget();
        Unit* owner = totem->GetOwner();
        if (!owner)
            return;

        AuraEffect const* talent = owner->GetAuraEffect(SPELL_SHAMAN_EARTHBOUND_TALENT, EFFECT_0);
        if (!talent || !roll_chance(talent->GetAmount()))
            return;

        totem->CastSpell(totem, SPELL_SHAMAN_EARTHBOUND_ROOT, true);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_sha_earthbind_totem_passive::HandleApply, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1223031 - Improved Stormstrike (Stormstrike part): "When you Stormstrike, you have a 20% chance to gain 50% mana regeneration
// while casting spells for 15 sec" (1238931, talent effect 0 = chance)
class classic_spell_sha_stormstrike : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHAMAN_IMPROVED_STORMSTRIKE_BUFF });
    }

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        if (AuraEffect const* talent = caster->GetAuraEffect(SPELL_SHAMAN_IMPROVED_STORMSTRIKE, EFFECT_0))
            if (roll_chance(talent->GetAmount()))
                caster->CastSpell(caster, SPELL_SHAMAN_IMPROVED_STORMSTRIKE_BUFF, CastSpellExtraArgs(talent));
    }

    void Register() override
    {
        AfterCast += SpellCastFn(classic_spell_sha_stormstrike::HandleAfterCast);
    }
};

// 1223031 - Improved Stormstrike (proc part): "Stormstrike's cooldown has a 20% chance to reset each time you Dodge or Parry."
// (spell_proc: taken melee, dodge or parry; talent effect 2 = chance)
class classic_spell_sha_improved_stormstrike : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_SHAMAN_IMPROVED_STORMSTRIKE, EFFECT_2 } });
    }

    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        return GetTarget()->GetSpellHistory()->HasCooldown(SPELL_SHAMAN_STORMSTRIKE);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        if (roll_chance(aurEff->GetAmount()))
            GetTarget()->GetSpellHistory()->ResetCooldown(SPELL_SHAMAN_STORMSTRIKE, true);
    }

    void PreventProc(AuraEffect const* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_sha_improved_stormstrike::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_improved_stormstrike::PreventProc, EFFECT_0, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_improved_stormstrike::PreventProc, EFFECT_1, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_improved_stormstrike::HandleProc, EFFECT_2, SPELL_AURA_DUMMY);
    }
};

// 408341-408345 - Fire Nova: "Instantly inflicts X fire damage to enemies within 10 yd of your active Fire totem." Forever
// replaced Fire Nova Totem by this spell; its effect is a dummy, the fire totem casts the per rank damage spell (408423-408428,
// TARGET_DEST_DEST + enemies 10 yd) at its position for the shaman.
class classic_spell_sha_fire_nova : public SpellScript
{
    static uint32 GetDamageSpell(uint32 spellId)
    {
        switch (spellId)
        {
            case 408341: return 408423;
            case 408342: return 408424;
            case 408343: return 408426;
            case 408344: return 408427;
            case 408345: return 408428;
            default:     return 0;
        }
    }

    static Creature* GetFireTotem(Unit* caster)
    {
        // SummonProperties slot 1 = fire totems (Searing, Magma, Flametongue, Frost Resistance...)
        ObjectGuid const& guid = caster->m_SummonSlot[SUMMON_SLOT_TOTEM];
        if (guid.IsEmpty())
            return nullptr;

        Creature* totem = ObjectAccessor::GetCreature(*caster, guid);
        return totem && totem->IsTotem() && totem->IsAlive() ? totem : nullptr;
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ GetDamageSpell(spellInfo->Id) });
    }

    SpellCastResult CheckCast()
    {
        return GetFireTotem(GetCaster()) ? SPELL_CAST_OK : SPELL_FAILED_TOTEMS;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Creature* totem = GetFireTotem(caster);
        if (!totem)
            return;

        totem->CastSpell(totem->GetPosition(), GetDamageSpell(GetSpellInfo()->Id), CastSpellExtraArgs(TRIGGERED_FULL_MASK)
            .SetOriginalCaster(caster->GetGUID()));
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_sha_fire_nova::CheckCast);
        OnEffectHitTarget += SpellEffectFn(classic_spell_sha_fire_nova::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 409324 - Ancestral Guidance: "For the next 10 sec, 25% of your damage is converted to healing on up to 3 nearby party members,
// and 100% of your healing is converted to damage on your most recent Flame Shock target." Damage -> 409333 (party heal, up to
// 3 targets), healing -> 409337 on the enemy carrying the shaman's Flame Shock with the most time left.
class classic_spell_sha_ancestral_guidance : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHAMAN_ANCESTRAL_GUIDANCE_HEAL, SPELL_SHAMAN_ANCESTRAL_GUIDANCE_DAMAGE })
            && ValidateSpellEffect({ { 409324, EFFECT_1 } });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (SpellInfo const* spellInfo = eventInfo.GetSpellInfo())
            if (spellInfo->Id == SPELL_SHAMAN_ANCESTRAL_GUIDANCE_HEAL || spellInfo->Id == SPELL_SHAMAN_ANCESTRAL_GUIDANCE_DAMAGE)
                return false;

        if (DamageInfo const* damageInfo = eventInfo.GetDamageInfo())
            if (damageInfo->GetDamage())
                return true;

        if (HealInfo const* healInfo = eventInfo.GetHealInfo())
            if (healInfo->GetEffectiveHeal())
                return true;

        return false;
    }

    static Unit* GetFlameShockTarget(Unit* shaman)
    {
        std::list<Unit*> enemies;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(shaman, shaman, 40.0f);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(shaman, enemies, check);
        Cell::VisitAllObjects(shaman, searcher, 40.0f);

        Unit* best = nullptr;
        int32 bestDuration = 0;
        for (Unit* enemy : enemies)
        {
            // Flame Shock (8050, 8052, 8053, 10447, 10448, 29228): shaman family flag 0x10000000
            AuraEffect const* flameShock = enemy->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_SHAMAN, flag128(0x10000000, 0, 0, 0), shaman->GetGUID());
            if (flameShock && flameShock->GetBase()->GetDuration() > bestDuration)
            {
                best = enemy;
                bestDuration = flameShock->GetBase()->GetDuration();
            }
        }
        return best;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* shaman = GetTarget();

        if (DamageInfo const* damageInfo = eventInfo.GetDamageInfo(); damageInfo && damageInfo->GetDamage())
        {
            SpellEffectValue const heal = CalculatePct(SpellEffectValue(damageInfo->GetDamage()), aurEff->GetAmount());
            if (heal >= 1.0)
                shaman->CastSpell(shaman, SPELL_SHAMAN_ANCESTRAL_GUIDANCE_HEAL, CastSpellExtraArgs(aurEff)
                    .AddSpellBP0(heal).AddSpellMod(SPELLVALUE_MAX_TARGETS, 3));
            return;
        }

        HealInfo const* healInfo = eventInfo.GetHealInfo();
        if (!healInfo || !healInfo->GetEffectiveHeal())
            return;

        SpellEffectValue const damage = CalculatePct(SpellEffectValue(healInfo->GetEffectiveHeal()), GetAura()->GetEffect(EFFECT_1)->GetAmount());
        if (damage < 1.0)
            return;

        if (Unit* target = GetFlameShockTarget(shaman))
            shaman->CastSpell(target, SPELL_SHAMAN_ANCESTRAL_GUIDANCE_DAMAGE, CastSpellExtraArgs(aurEff).AddSpellBP0(damage));
    }

    void PreventProc(AuraEffect const* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_sha_ancestral_guidance::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_ancestral_guidance::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_ancestral_guidance::PreventProc, EFFECT_1, SPELL_AURA_DUMMY);
    }
};

void AddSC_classic_talents_mage_shaman()
{
    // Mage
    RegisterSpellScript(classic_spell_mage_ignite);
    RegisterSpellScript(classic_spell_mage_ignite_dot);
    RegisterSpellScript(classic_spell_mage_frozen_target);
    RegisterSpellScript(classic_spell_mage_fingers_of_frost_charges);
    RegisterSpellScript(classic_spell_mage_fingers_of_frost);
    RegisterSpellScript(classic_spell_mage_improved_blizzard);
    RegisterSpellScript(classic_spell_mage_counterspell);
    RegisterSpellScript(classic_spell_mage_heating_up);
    RegisterSpellScript(classic_spell_mage_frostbite);

    // Shaman
    RegisterSpellScript(classic_spell_sha_flurry);
    RegisterSpellScript(classic_spell_sha_elemental_devastation);
    RegisterSpellScript(classic_spell_sha_lightning_overload);
    RegisterSpellScript(classic_spell_sha_maelstrom_weapon);
    RegisterSpellScript(classic_spell_sha_maelstrom_weapon_buff);
    RegisterSpellScript(classic_spell_sha_earthbind_totem_passive);
    RegisterSpellScript(classic_spell_sha_stormstrike);
    RegisterSpellScript(classic_spell_sha_improved_stormstrike);
    RegisterSpellScript(classic_spell_sha_fire_nova);
    RegisterSpellScript(classic_spell_sha_ancestral_guidance);
}
