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
#include "Item.h"
#include "ItemTemplate.h"
#include "GridNotifiersImpl.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "DB2Stores.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "Unit.h"
#include <cmath>

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
    SPELL_MAGE_FINGERS_OF_FROST_STACKS      = 400670,   // 2 stacks, one used per spell cast
    SPELL_MAGE_COLD_SNAP                    = 12472,
    SPELL_MAGE_ICE_LANCE                    = 400640,
    SPELL_MAGE_COMBUSTION                   = 11129,
    SPELL_MAGE_COMBUSTION_BUFF              = 28682,    // +10% Fire critical chance per stack
    SPELL_MAGE_ARCANE_CONCENTRATION         = 11213,    // effect 0: proc trigger of Clearcasting 12536, the amount is the chance
    SPELL_MAGE_FIRE_POWER                   = 11124,    // effect 0: dummy, % per Fire effect of the mage on the target (70532)
    SPELL_MAGE_FROST_WARDING                = 11189,    // 70630 effect 2: % more absorb for Ice Barrier and Frost Ward
    SPELL_MAGE_FRIGID_POWER                 = 1326891,  // 70630: Frost damage buff that Ice Barrier grants for its duration
    SPELL_MAGE_WINTERS_CHILL_TALENT         = 11180,    // effect 1: proc chance
    SPELL_MAGE_WINTERS_CHILL                = 12579     // 70630 effect 1: % spell power for the caster's next Cone of Cold, per stack
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
    SPELL_SHAMAN_ANCESTRAL_GUIDANCE_DAMAGE  = 409337,
    SPELL_SHAMAN_ANCESTRAL_FORTITUDE        = 16177     // Ancestral Healing: armor percent on the healed target, 15 s
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

// 11180 - Winter's Chill (client 70630): the talent's effect 1 % chance to put Winter's Chill 12579 on the target
static void TryApplyWintersChill(Unit* mage, Unit* target)
{
    AuraEffect const* chance = mage->GetAuraEffect(SPELL_MAGE_WINTERS_CHILL_TALENT, EFFECT_1);
    if (!chance || !target || !roll_chance(float(chance->GetAmount())))
        return;

    mage->CastSpell(target, SPELL_MAGE_WINTERS_CHILL, true);
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

// 12472 - Cold Snap: "When activated, this spell finishes the cooldown on all Frost spells you recently cast." The client spell is a bare
// dummy effect and nothing handled it, so the spell used up its own cooldown and reset nothing (Ice Block, Frost Nova, Cone of Cold, Ice
// Barrier and Frost Ward stayed on cooldown). Every Frost mage spell on cooldown except Cold Snap itself is reset.
// Same behaviour as vMaNGOS (GPL-2.0, src/scripts/spells/spell_mage.cpp, MageColdSnapScript): Frost spells of the mage family are reset.
class classic_spell_mage_cold_snap : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->GetSpellHistory()->ResetCooldowns([](SpellHistory::CooldownEntry const& cooldown)
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(cooldown.SpellId, DIFFICULTY_NONE);
            return spellInfo && spellInfo->Id != SPELL_MAGE_COLD_SNAP && spellInfo->SpellFamilyName == SPELLFAMILY_MAGE
                && (spellInfo->GetSchoolMask() & SPELL_SCHOOL_MASK_FROST);
        }, true);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(classic_spell_mage_cold_snap::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 400640 - Ice Lance: "Deals Frost damage to an enemy target, three times as much if the target is Frozen." The client spell is a damage
// effect, a dummy effect with the 300 percent and a second dummy effect; nothing used the 300 percent, so a Frozen target took the base hit.
// The damage is multiplied (before the critical roll) when the target is Frozen by Frost Nova or another root, or when a charge of Fingers
// of Frost was on the mage as the cast began (classic_spell_mage_frozen_target spends the charge in OnCast, after BeforeCast).
class classic_spell_mage_ice_lance : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_MAGE_FINGERS_OF_FROST_STACKS }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void CheckFingersOfFrost()
    {
        _fingersOfFrost = !GetSpell()->IsTriggered() && GetCaster()->HasAura(SPELL_MAGE_FINGERS_OF_FROST_STACKS);
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (_fingersOfFrost || GetHitUnit()->HasAuraState(AURA_STATE_FROZEN, GetSpellInfo(), caster))
            SetHitDamage(CalculatePct(GetHitDamage(), GetEffectInfo(EFFECT_1).CalcValue(caster)));

        // client 70630: a Fingers of Frost empowered Ice Lance can apply Winter's Chill
        if (_fingersOfFrost)
            TryApplyWintersChill(caster, GetHitUnit());
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(classic_spell_mage_ice_lance::CheckFingersOfFrost);
        OnEffectHitTarget += SpellEffectFn(classic_spell_mage_ice_lance::HandleHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }

    bool _fingersOfFrost = false;
};

// 11129 - Combustion: "Each of your Fire damage spell hits increases your critical strike chance with Fire damage spells by 10%. This
// effect lasts until you have caused 3 critical strikes with Fire spells." The cast puts one stack of 28682 on the mage (trigger effect)
// and the client data gives 11129 three proc charges and 28682 an endless duration, with no handler for either: the first stack stayed
// for ever and no more were added. 11129 now procs on fire critical hits only (spell_proc), so each critical spends a charge, and
// ends 28682 with it; 28682 procs on every fire damage hit and adds a stack.
class classic_spell_mage_combustion : public AuraScript
{
    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_MAGE_COMBUSTION_BUFF);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_mage_combustion::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_mage_combustion::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 28682 - Combustion (critical chance): every fire damage spell hit adds a stack (up to the stacks of the client spell). As in vMaNGOS
// (GPL-2.0, src/scripts/spells/spell_mage.cpp, MageCombustionBuffScript / MageCombustionProcScript), losing the buff by cancelling or
// dispelling it ends the Combustion proc aura 11129 too, which starts its cooldown.
class classic_spell_mage_combustion_stacks : public AuraScript
{
    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_MAGE_COMBUSTION);
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        GetAura()->ModStackAmount(1);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_mage_combustion_stacks::HandleProc, EFFECT_0, SPELL_AURA_ADD_FLAT_MODIFIER);
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_mage_combustion_stacks::HandleRemove, EFFECT_0, SPELL_AURA_ADD_FLAT_MODIFIER, AURA_EFFECT_HANDLE_REAL);
    }
};

// Rank of a talent: Forever's talents are trait nodes with one spell, the ranks are held on the node entry of the active spec's trait config.
// Returns 0 when the player does not have the talent.
static int32 GetClassicTalentRank(Player const* player, uint32 spellId)
{
    UF::TraitConfig const* config = player->GetClassicSpecGroupConfig(player->GetActiveTalentGroup() == 1);
    if (!config)
        return 0;

    for (UF::TraitEntry const& entry : config->Entries)
    {
        TraitNodeEntryEntry const* nodeEntry = sTraitNodeEntryStore.LookupEntry(entry.TraitNodeEntryID);
        TraitDefinitionEntry const* definition = nodeEntry ? sTraitDefinitionStore.LookupEntry(nodeEntry->TraitDefinitionID) : nullptr;
        if (definition && uint32(definition->SpellID) == spellId)
            return entry.Rank + entry.GrantedRanks + entry.BonusRanks;
    }
    return 0;
}

// 29441 - Magic Absorption: "Increases all resistances by 2 and causes all spells you fully resist to restore 1% of your total mana." (per talent rank). The
// client aura is a resistance aura plus a dummy aura; the mana is the energize 29442 (VMaNGOS casts it with amount percent of the maximum
// mana as the amount). spell_proc: harmful spells and abilities taken that were fully resisted.
class classic_spell_mage_magic_absorption : public AuraScript
{
    static constexpr uint32 SPELL_MAGE_MAGIC_ABSORPTION_ENERGIZE = 29442;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MAGE_MAGIC_ABSORPTION_ENERGIZE });
    }

    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        return GetTarget()->GetPowerType() == POWER_MANA && GetTarget()->ToPlayer();
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        Player* mage = GetTarget()->ToPlayer();
        // 1% of the maximum mana per rank (vanilla 1/2/3/4/5%)
        int32 const rank = std::max(1, GetClassicTalentRank(mage, GetId()));
        mage->EnergizeBySpell(mage, sSpellMgr->AssertSpellInfo(SPELL_MAGE_MAGIC_ABSORPTION_ENERGIZE, DIFFICULTY_NONE),
            CalculatePct(mage->GetMaxPower(POWER_MANA), rank), POWER_MANA);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_mage_magic_absorption::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_mage_magic_absorption::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 29074 - Master of the Elements: "Your Fire and Frost spell criticals will refund a percent of their base mana cost." The client spell is a
// dummy aura whose amount (10) is the percent; nothing handled it. The mana the critical spell cost is given back by percent (spell_proc:
// Fire and Frost, direct damage, critical hit).
class classic_spell_mage_master_of_elements : public AuraScript
{
    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Spell const* spell = eventInfo.GetProcSpell();
        return spell && spell->GetPowerTypeCostAmount(POWER_MANA).value_or(0) > 0;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        int32 const cost = eventInfo.GetProcSpell()->GetPowerTypeCostAmount(POWER_MANA).value_or(0);
        GetTarget()->EnergizeBySpell(GetTarget(), GetSpellInfo(), CalculatePct(cost, aurEff->GetAmount()), POWER_MANA);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_mage_master_of_elements::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_mage_master_of_elements::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 11213 - Arcane Concentration: "Gives you a $s1% chance of entering a Clearcasting state after any damage spell hits a target." The client
// spell is a proc trigger (Clearcasting 12536) with 100 as spell proc chance and the talent chance as the effect amount, so it entered
// Clearcasting on every damage spell hit. The chance is rolled from the amount (spell_proc: harmful spell, damage, hit).
class classic_spell_mage_arcane_concentration : public AuraScript
{
    bool CheckProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        return roll_chance(float(aurEff->GetAmount()));
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(classic_spell_mage_arcane_concentration::CheckProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 11071 - Frostbite (70532): "Gives your Chill effects a $s1% chance to Freeze the target for $12494d." The client spell is a proc trigger
// (Frostbite 12494) with no class mask and the talent chance as the effect amount. The row limits it to the mage's Chill spells (family
// flag 0x100000) that hit; the chance is rolled from the amount.
class classic_spell_mage_frostbite : public AuraScript
{
    bool CheckProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        return roll_chance(float(aurEff->GetAmount()));
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(classic_spell_mage_frostbite::CheckProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 11124 - Fire Power (70532): "Increases the damage done by your Fire spells by $s1% per your Fire effects on the target, up to a maximum
// increase of ${$s1*4}%. Effects which mix multiple schools of magic do not qualify." The talent is a dummy. Fire effects: the mage's
// auras on the target whose school is Fire alone. Bound to the mage's Fire damage spells, direct damage and periodic damage.
namespace ClassicFirePower
{
    static constexpr uint32 MAX_EFFECTS = 4;

    static float GetBonusPct(Unit const* caster, Unit const* victim)
    {
        if (!caster || !victim)
            return 0.0f;

        AuraEffect const* talent = caster->GetAuraEffect(SPELL_MAGE_FIRE_POWER, EFFECT_0);
        if (!talent || talent->GetAmount() <= 0.0)
            return 0.0f;

        uint32 effects = 0;
        for (auto const& applied : victim->GetAppliedAuras())
        {
            Aura const* aura = applied.second->GetBase();
            if (aura->GetCasterGUID() != caster->GetGUID())
                continue;

            if (aura->GetSpellInfo()->GetSchoolMask() == SPELL_SCHOOL_MASK_FIRE)
                ++effects;
        }

        return float(talent->GetAmount()) * float(std::min(effects, MAX_EFFECTS));
    }
}

class classic_spell_mage_fire_power : public SpellScript
{
    void HandleCalcDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod)
    {
        if (float const bonus = ClassicFirePower::GetBonusPct(GetCaster(), victim); bonus > 0.0f)
            AddPct(pctMod, bonus);
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_mage_fire_power::HandleCalcDamage);
    }
};

class classic_spell_mage_fire_power_aura : public AuraScript
{
    void HandleCalcDamage(AuraEffect const* aurEff, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod)
    {
        if (aurEff->GetAuraType() != SPELL_AURA_PERIODIC_DAMAGE)
            return;

        if (float const bonus = ClassicFirePower::GetBonusPct(GetCaster(), victim); bonus > 0.0f)
            AddPct(pctMod, bonus);
    }

    void Register() override
    {
        DoEffectCalcDamageAndHealing += AuraEffectCalcDamageFn(classic_spell_mage_fire_power_aura::HandleCalcDamage, EFFECT_ALL, SPELL_AURA_ANY);
    }
};

// Client 70630, Frost Ward (6143, 8461, 8462, 10177, 28609) and Ice Barrier (11426, 13031-13033): "Absorbs
// ${$s1+($SPFR*0.1)*($11189s3/100+1)} damage". The core adds 10% of Frost spell damage (classic_spell_value bonus_coeff) and Frost Warding
// effect 2 already raises the base points (a percent modifier); here it raises the spell damage part too. Frost Ward effect 1 is a dummy
// in 70630 (it was the reflect aura): "$w2% chance to negate Frost damage", the warded spell is absorbed whole and the ward keeps its amount.
// Ice Barrier also grants Frigid Power 1326891 for its duration.
class classic_spell_mage_frost_ward_absorb : public AuraScript
{
    void CalcAbsorb(AuraEffect const* /*aurEff*/, Unit* /*victim*/, int32& /*absorb*/, int32& flatMod, float& /*pctMod*/)
    {
        Unit* caster = GetCaster();
        if (!caster || flatMod <= 0)
            return;

        if (AuraEffect const* warding = caster->GetAuraEffect(SPELL_MAGE_FROST_WARDING, EFFECT_2))
            if (warding->GetAuraType() == SPELL_AURA_ADD_PCT_MODIFIER && warding->GetAmount() > 0)
                flatMod = int32(std::round(float(flatMod) * (1.0f + float(warding->GetAmount()) / 100.0f)));
    }

    void Absorb(AuraEffect* /*aurEff*/, DamageInfo& dmgInfo, uint32& absorbAmount)
    {
        AuraEffect const* negate = GetEffect(EFFECT_1);
        if (!negate || negate->GetAuraType() != SPELL_AURA_DUMMY || negate->GetAmount() <= 0)
            return;

        if (!dmgInfo.GetSpellInfo() || !roll_chance(float(negate->GetAmount())))
            return;

        absorbAmount = dmgInfo.GetDamage();
        PreventDefaultAction();
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (!GetSpellInfo()->HasAura(SPELL_AURA_ADD_PCT_MODIFIER))   // Ice Barrier of 70630 (its Frost damage effects), not Frost Ward
            return;

        Unit* target = GetTarget();
        if (sSpellMgr->GetSpellInfo(SPELL_MAGE_FRIGID_POWER, DIFFICULTY_NONE))
            target->CastSpell(target, SPELL_MAGE_FRIGID_POWER, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetTriggeringAura(GetEffect(EFFECT_0)));
    }

    void Register() override
    {
        DoEffectCalcDamageAndHealing += AuraEffectCalcAbsorbFn(classic_spell_mage_frost_ward_absorb::CalcAbsorb, EFFECT_0);
        OnEffectAbsorb += AuraEffectAbsorbFn(classic_spell_mage_frost_ward_absorb::Absorb, EFFECT_0);
        AfterEffectApply += AuraEffectApplyFn(classic_spell_mage_frost_ward_absorb::HandleApply, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL);
    }
};

// 11180 - Winter's Chill, client 70630: "Gives your Frostbolt, Frost Blast, and Fingers of Frost empowered Ice Lance spells a $m2% chance to
// apply Winter's Chill, increasing the target's damage taken from your next Cone of Cold by $12579s1% and it gains an additional $s3% of
// your spell damage, stacking up to $s1 times." The row limits the proc to Frostbolt and Frost Blast (family flag 0x20); the empowered
// Ice Lance rolls in classic_spell_mage_ice_lance. 70630 gives the talent one proc charge, which would end the passive talent on its first
// proc: it keeps none.
class classic_spell_mage_winters_chill : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_MAGE_WINTERS_CHILL });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetAura()->SetCharges(0);
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        TryApplyWintersChill(GetTarget(), eventInfo.GetActionTarget());
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_mage_winters_chill::HandleApply, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
        OnEffectProc += AuraEffectProcFn(classic_spell_mage_winters_chill::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// Cone of Cold (120, 8492, 10159, 10160, 10161) with Winter's Chill 12579 of client 70630 on the target: the debuff's effect 0 (damage
// taken from the caster's Cone of Cold, per stack) is the core's; its effect 1 adds that % of the caster's Frost spell damage, and the
// Cone of Cold spends the debuff. A 70532 debuff (no effect 1) stays as it was.
class classic_spell_mage_cone_of_cold_winters_chill : public SpellScript
{
    void HandleCalcDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& flatMod, float& /*pctMod*/)
    {
        Unit* caster = GetCaster();
        if (AuraEffect const* bonus = victim->GetAuraEffect(SPELL_MAGE_WINTERS_CHILL, EFFECT_1, caster->GetGUID()))
            flatMod += int32(float(caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FROST)) * float(bonus->GetAmount()) / 100.0f);
    }

    void SpendWintersChill()
    {
        Unit* target = GetHitUnit();
        if (target && target->GetAuraEffect(SPELL_MAGE_WINTERS_CHILL, EFFECT_1, GetCaster()->GetGUID()))
            target->RemoveAurasDueToSpell(SPELL_MAGE_WINTERS_CHILL, GetCaster()->GetGUID());
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_mage_cone_of_cold_winters_chill::HandleCalcDamage);
        AfterHit += SpellHitFn(classic_spell_mage_cone_of_cold_winters_chill::SpendWintersChill);
    }
};

// 1326080, 1326081, 1326082 - Arcane Wrath (70532): "Launches a projectile of pure Arcane energy at the enemy target, causing $s1 Arcane
// damage. This spell gains additional Spell Damage equal to $s2% of your maximum Mana." Effect 1 is the percentage as a dummy; the extra
// spell damage counts like spell power, with the damage effect's coefficient.
class classic_spell_mage_arcane_wrath : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void HandleCalcDamage(SpellEffectInfo const& spellEffectInfo, Unit* /*victim*/, int32& /*damage*/, int32& flatMod, float& /*pctMod*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        float const spellDamage = CalculatePct(float(caster->GetMaxPower(POWER_MANA)), GetEffectInfo(EFFECT_1).CalcValue(caster));
        flatMod += int32(spellDamage * spellEffectInfo.BonusCoefficient);
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_mage_arcane_wrath::HandleCalcDamage);
    }
};

// 400588 - Missile Barrage (70532): "Gives your Arcane Blast spell a $m1% chance, your Arcane Missile hits a ${$m1/4}% chance, and your Arcane
// Wrath, Fireball, Frostbolt, and Frostfire Bolt spells a ${$m1/2}% chance to [give 400589]". The client proc data is a flat 50% on any
// harmful spell. The spell_proc row sets the chance to 100 and lets the triggered missile hits proc; the chance per spell is rolled here.
class classic_spell_mage_missile_barrage : public AuraScript
{
    static bool IsHalfChanceSpell(SpellInfo const* spellInfo)
    {
        switch (spellInfo->Id)
        {
            case 1326080: case 1326081: case 1326082:   // Arcane Wrath
            case 401502: case 1237312: case 1237313:    // Frostfire Bolt
                return true;
            default:
                break;
        }
        // Fireball (word 0 0x1), Frostbolt (0x20)
        return spellInfo->SpellFamilyName == SPELLFAMILY_MAGE && (spellInfo->SpellFamilyFlags[0] & (0x1 | 0x20));
    }

    bool CheckProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        if (!spellInfo)
            return false;

        float const chance = float(aurEff->GetAmount());
        if (spellInfo->SpellFamilyName == SPELLFAMILY_MAGE && (spellInfo->SpellFamilyFlags[0] & 0x20000000))  // Arcane Blast
            return roll_chance(chance);
        if (spellInfo->SpellFamilyName == SPELLFAMILY_MAGE && (spellInfo->SpellFamilyFlags[0] & 0x200000))    // Arcane Missiles hits
            return roll_chance(chance / 4.0f);
        if (IsHalfChanceSpell(spellInfo))
            return roll_chance(chance / 2.0f);
        return false;
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(classic_spell_mage_missile_barrage::CheckProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
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

    // "Instantly attack with both weapons": the client spell has one normalized weapon damage effect, which the spell code runs with the
    // main hand only, so the off-hand weapon gets its swing here, after the main hand hit.
    void HandleOffHandStrike()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target || !target->IsAlive() || !caster->haveOffhandWeapon())
            return;

        caster->AttackerStateUpdate(target, OFF_ATTACK, true);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(classic_spell_sha_stormstrike::HandleOffHandStrike);
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

// 8026, 8028, 8029, 10445, 16343, 16344 - Flametongue Weapon Proc; 8248, 8253, 10523, 16389 - Flametongue Totem Proc. The client spells are a dummy
// effect (base points 326 .. 2498) that nothing handled, so Flametongue dealt no damage. Behaviour ported from vMaNGOS (GPL-2.0, src/scripts/spells/
// spell_shaman.cpp ShamanFlametongueProcDummyScript): the dummy casts the Flametongue Attack spell 10444 for base points * weapon speed in seconds / 100.
// vMaNGOS adds 3.85 * spell damage * speed / 100 to that; here 10444 carries the spell power coefficient of the classic spell value table (0.1), which is
// the same term at the usual 2.6 s weapon speed, so only the base points part is passed. The speed is that of the item that procced (the enchanted
// weapon), or the main hand weapon of the shaman whose melee hit procced the totem aura.
class classic_spell_sha_flametongue_proc : public SpellScript
{
    static constexpr uint32 SPELL_SHAMAN_FLAMETONGUE_ATTACK = 10444;
    static constexpr float FLAMETONGUE_DAMAGE_PER_SECOND_PCT = 0.01f;   // base points are per 100 seconds of weapon speed

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHAMAN_FLAMETONGUE_ATTACK });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        Item* weapon = GetCastItem();
        if (!weapon)
            if (Player* player = caster->ToPlayer())
                weapon = player->GetWeaponForAttack(BASE_ATTACK, true);

        float const delaySeconds = weapon ? float(weapon->GetTemplate()->GetDelay()) / 1000.0f : float(caster->GetBaseAttackTime(BASE_ATTACK)) / 1000.0f;
        int32 const basePoints = GetEffectValue();
        int32 const damage = int32(std::lround(float(basePoints) * FLAMETONGUE_DAMAGE_PER_SECOND_PCT * delaySeconds));

        CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
        args.SetTriggeringSpell(GetSpell());
        args.AddSpellBP0(damage);
        if (GetCastItem())
            args.SetCastItem(GetCastItem());
        caster->CastSpell(target, SPELL_SHAMAN_FLAMETONGUE_ATTACK, args);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_sha_flametongue_proc::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 6495 - Sentry Totem: "Summons a Sentry Totem that allows the shaman to see through its eyes." The dummy aura on the shaman moves the view to the
// totem while the aura lasts and back when it ends. Behaviour ported from vMaNGOS (GPL-2.0, SpellAuras.cpp, Aura::HandleAuraDummy case 6495:
// the totem in the air slot becomes the camera). Nothing in this core set the viewpoint for the spell.
class classic_spell_sha_sentry_totem : public AuraScript
{
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = GetTarget()->ToPlayer();
        if (!player || player->GetViewpoint())
            return;

        // Air totems sit in the last of the four totem summon slots
        Creature* totem = ObjectAccessor::GetCreature(*player, player->m_SummonSlot[SUMMON_SLOT_TOTEM_4]);
        if (totem && totem->IsTotem() && totem->IsAlive())
            player->SetViewpoint(totem, true);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = GetTarget()->ToPlayer();
        if (!player)
            return;

        // Only a totem view is ours to end; a Far Sight view stays
        if (WorldObject* viewpoint = player->GetViewpoint())
            if (Creature* totem = viewpoint->ToCreature(); totem && totem->IsTotem())
                player->SetViewpoint(totem, false);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_sha_sentry_totem::HandleApply, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_sha_sentry_totem::HandleRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 20860, 20865, 20866, 20867, 20868, 20870, 20871 - Rockbiter Weapon Proc: the client spell is a script effect with the threat in its base points.
// Behaviour ported from vMaNGOS (GPL-2.0, SpellEffects.cpp Spell::EffectScriptEffect, shaman cases 20865-20871): the shaman adds base points times the
// main hand attack time in seconds as threat, and only on a target that already has threat on him. vMaNGOS leaves out rank 1 (20860); the client
// data has the same effect on it, so it is bound here too.
class classic_spell_sha_rockbiter_threat : public SpellScript
{
    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target || !target->CanHaveThreatList())
            return;

        if (target->GetThreatManager().GetThreat(caster) <= 0.0f)
            return;

        int32 const basePoints = GetEffectValue();
        target->GetThreatManager().AddThreat(caster, float(basePoints) * float(caster->GetBaseAttackTime(BASE_ATTACK)) / 1000.0f, GetSpellInfo(), true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_sha_rockbiter_threat::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 408490, 1238299, 1238300, 1326047 - Lava Burst (70532, the same in 70790): "You hurl molten lava at the target, dealing $s1 Fire damage.
// If your Flame Shock is on the target, Lava Burst deals $s2% increased damage." Effect 1 is the bonus as a dummy (20 on all four ranks).
// Flame Shock is told by the shaman family flag its periodic damage carries (8050, 8052, 8053, 10447, 10448, 29228).
class classic_spell_sha_lava_burst : public SpellScript
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

        if (victim->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_SHAMAN, flag128(0x10000000, 0, 0, 0), caster->GetGUID()))
            AddPct(pctMod, GetEffectInfo(EFFECT_1).CalcValue(caster));
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_sha_lava_burst::HandleCalcDamage);
    }
};

// 1064, 10622, 10623 - Chain Heal, with Riptide 408521, 1239242, 1239243 (70532): "increases the effectiveness of your Chain Heal casts
// directly on that target by $s3%." Riptide effect 2 is the bonus as a dummy aura; only the first target of the Chain Heal gets it.
class classic_spell_sha_chain_heal_riptide : public SpellScript
{
    void HandleCalcHealing(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*healing*/, int32& /*flatMod*/, float& pctMod)
    {
        Unit* caster = GetCaster();
        if (!caster || !victim || victim != GetExplTargetUnit())
            return;

        // Riptide: shaman family flag 0x10 in the third word
        if (AuraEffect const* riptide = victim->GetAuraEffect(SPELL_AURA_DUMMY, SPELLFAMILY_SHAMAN, flag128(0, 0, 0x10, 0), caster->GetGUID()))
            AddPct(pctMod, riptide->GetAmount());
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(classic_spell_sha_chain_heal_riptide::HandleCalcHealing);
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

// 324, 325, 905, 945, 8134, 10431, 10432 - Lightning Shield (ranks 1-7): "Protects the caster. When a melee or ranged attack hits the caster, the attacker
// takes Nature damage. 3 charges." The client spell is a proc trigger aura whose trigger spell 26545 has a bare dummy effect, so a hit on the shaman cast
// nothing. Behaviour ported from vMaNGOS (GPL-2.0, UnitAuraProcHandler.cpp Unit::HandleProcTriggerSpell, shaman case: the aura rank selects the damage spell
// 26363-26370, cast on the attacker). Damage amounts and spell power coefficients are those of the damage spells (classic_spell_value).
class classic_spell_sha_lightning_shield : public AuraScript
{
    static uint32 GetDamageSpell(uint32 auraSpellId)
    {
        switch (auraSpellId)
        {
            case 324:   return 26364;
            case 325:   return 26365;
            case 905:   return 26366;
            case 945:   return 26367;
            case 8134:  return 26369;
            case 10431: return 26370;
            case 10432: return 26363;
            default:    return 0;
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ GetDamageSpell(spellInfo->Id) });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* attacker = eventInfo.GetActor();
        if (!attacker || !attacker->IsAlive())
            return;

        GetTarget()->CastSpell(attacker, GetDamageSpell(GetSpellInfo()->Id), CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_lightning_shield::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 8515, 10609, 10612 - Windfury Totem (passive, ranks 1-3); 8230, 8250, 10521, 15036 - Flametongue Totem (passive, ranks 1-4): the totem's area aura on
// the party members. The client data has no trigger spell on these effects: the Windfury ones carry the Windfury Totem attack spell (8516, 10608, 10610)
// in the base points of the effect, Flametongue rank 1, 3 and 4 the proc spell (8253, 10523, 16389) there and rank 2 (8250) nothing at all, so the party
// members' melee hits cast nothing and neither totem did anything. Windfury grants the member 1 extra attack (the spell is a self buff), Flametongue
// casts its proc spell (fire damage by weapon speed, classic_spell_sha_flametongue_proc) on the target of the hit.
class classic_spell_sha_weapon_totem_aura : public AuraScript
{
    struct TotemProc
    {
        uint32 AuraSpellId;
        uint32 ProcSpellId;
        bool OnSelf;
    };

    static constexpr TotemProc Procs[] =
    {
        { 8515,  8516,  true  }, { 10609, 10608, true  }, { 10612, 10610, true  },     // Windfury Totem
        { 8230,  8253,  false }, { 8250,  8248,  false }, { 10521, 10523, false }, { 15036, 16389, false }   // Flametongue Totem
    };

    static TotemProc const* Find(uint32 auraSpellId)
    {
        for (TotemProc const& proc : Procs)
            if (proc.AuraSpellId == auraSpellId)
                return &proc;
        return nullptr;
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        TotemProc const* proc = Find(spellInfo->Id);
        return proc && ValidateSpellInfo({ proc->ProcSpellId });
    }

    // melee hits only: the 1.60 data lets a spell hit of the party member proc these auras too
    bool CheckProc(ProcEventInfo& eventInfo)
    {
        return (eventInfo.GetTypeMask() & PROC_FLAG_DEAL_MELEE_SWING) || (eventInfo.GetTypeMask() & PROC_FLAG_DEAL_MELEE_ABILITY);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        TotemProc const* proc = Find(GetSpellInfo()->Id);
        Unit* member = GetTarget();
        Unit* target = proc->OnSelf ? member : eventInfo.GetActionTarget();
        if (!target || !target->IsAlive())
            return;

        member->CastSpell(target, proc->ProcSpellId, CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_sha_weapon_totem_aura::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_weapon_totem_aura::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 16176 - Ancestral Healing: "Your critical heals increase your target's Armor by X% for 15 sec." Critical direct heals cast 16177 (armor percent) on the
// healed target with the talent amount, like Inspiration (priest). The client spell is a dummy aura with the buff as its trigger spell and nothing used it.
// Crit only + triggered heals via spell_proc.
class classic_spell_sha_ancestral_healing : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SHAMAN_ANCESTRAL_FORTITUDE });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        HealInfo* healInfo = eventInfo.GetHealInfo();
        return healInfo && healInfo->GetHeal() && eventInfo.GetActionTarget() && (eventInfo.GetHitMask() & PROC_HIT_CRITICAL);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        CastSpellExtraArgs args(aurEff);
        if (aurEff->GetAmountAsInt() > 0)
            args.AddSpellBP0(aurEff->GetAmountAsInt());
        GetTarget()->CastSpell(eventInfo.GetActionTarget(), SPELL_SHAMAN_ANCESTRAL_FORTITUDE, args);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_sha_ancestral_healing::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_ancestral_healing::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
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
    RegisterSpellScript(classic_spell_mage_cold_snap);
    RegisterSpellScript(classic_spell_mage_ice_lance);
    RegisterSpellScript(classic_spell_mage_combustion);
    RegisterSpellScript(classic_spell_mage_combustion_stacks);
    RegisterSpellScript(classic_spell_mage_master_of_elements);
    RegisterSpellScript(classic_spell_mage_magic_absorption);
    RegisterSpellScript(classic_spell_mage_arcane_concentration);
    RegisterSpellScript(classic_spell_mage_frostbite);
    RegisterSpellAndAuraScriptPair(classic_spell_mage_fire_power, classic_spell_mage_fire_power_aura);
    RegisterSpellScript(classic_spell_mage_arcane_wrath);
    RegisterSpellScript(classic_spell_mage_frost_ward_absorb);
    RegisterSpellScript(classic_spell_mage_winters_chill);
    RegisterSpellScript(classic_spell_mage_cone_of_cold_winters_chill);
    RegisterSpellScript(classic_spell_mage_missile_barrage);

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
    RegisterSpellScript(classic_spell_sha_lava_burst);
    RegisterSpellScript(classic_spell_sha_chain_heal_riptide);
    RegisterSpellScript(classic_spell_sha_flametongue_proc);
    RegisterSpellScript(classic_spell_sha_sentry_totem);
    RegisterSpellScript(classic_spell_sha_rockbiter_threat);
    RegisterSpellScript(classic_spell_sha_ancestral_guidance);
    RegisterSpellScript(classic_spell_sha_lightning_shield);
    RegisterSpellScript(classic_spell_sha_weapon_totem_aura);
    RegisterSpellScript(classic_spell_sha_ancestral_healing);
}
