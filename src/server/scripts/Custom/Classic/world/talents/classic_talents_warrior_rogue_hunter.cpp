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

// Classic 1.60 (WoW Forever) warrior, rogue and hunter talents and abilities whose client spell is only a dummy aura or dummy effect.
// Talent ranks come from TraitDefinitionEffectPoints curves: the core puts the rank value into the aura amount (SpellEffectInfo::CalcValue),
// so the scripts read aurEff->GetAmount() and never hardcode rank values.

#include "ScriptMgr.h"
#include "Containers.h"
#include "Creature.h"
#include "DB2Stores.h"
#include "Formulas.h"
#include "Item.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Pet.h"
#include "Player.h"
#include "Random.h"
#include "Spell.h"
#include "StringFormat.h"
#include "World.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "Unit.h"
#include <atomic>
#include <mutex>
#include <unordered_set>

enum ClassicWarriorRogueHunterTalents
{
    // warrior
    SPELL_WARRIOR_TACTICAL_MASTERY              = 1310185,  // baseline (level 14): retain $m1 rage on stance change
    SPELL_WARRIOR_IMPROVED_TACTICAL_MASTERY     = 12295,
    SPELL_WARRIOR_ENRAGE_BUFF                   = 12880,
    SPELL_WARRIOR_BLOOD_CRAZE_REGEN             = 16488,
    SPELL_WARRIOR_MEATHOOK_PULL                 = 403233,
    SPELL_WARRIOR_EXECUTE_DAMAGE                = 20647,
    SPELL_WARRIOR_DEEP_WOUND                    = 412609,   // periodic dummy aura, 3 sec ticks, the amount is the damage per tick
    SPELL_WARRIOR_DEEP_WOUND_DAMAGE             = 412613,   // school damage of one tick
    SPELL_WARRIOR_FLURRY_BUFF                   = 12966,    // 3 charges of haste, the talent rank is the haste
    SPELL_WARRIOR_SWEEPING_STRIKES_STRIKE       = 12723,    // damage of the copied attack (base points: the damage)
    SPELL_WARRIOR_SWEEPING_STRIKES_NORMALIZED   = 26654,
    SPELL_WARRIOR_LAST_STAND_HEALTH             = 12976,    // +health aura, amount = 30% of the warrior's maximum health (set by the script)
    SPELL_WARRIOR_RETALIATION_STRIKE            = 20240,    // weapon strike on the attacker
    SPELL_WARRIOR_IMPROVED_WHIRLWIND_CLASSIC    = 1310315,  // Forever talent (Fury row 6): effect 0 Whirlwind cooldown, effect 1 closest enemy bonus (dummy)

    // rogue
    SPELL_ROGUE_SETUP_COMBO_POINT               = 15250,
    SPELL_ROGUE_SEAL_FATE_COMBO_POINT           = 14189,
    SPELL_ROGUE_CUTTHROAT_BUFF                  = 462707,
    SPELL_ROGUE_QUIETUS                         = 1310728,
    SPELL_ROGUE_SHIV                            = 424799,
    SPELL_ROGUE_SHIV_STRIKE                     = 424800,
    SPELL_ROGUE_POISONED_KNIFE                  = 425012,
    SPELL_ROGUE_POISONED_KNIFE_STRIKE           = 425013,
    SPELL_ROGUE_REDIRECT                        = 438040,

    // hunter
    SPELL_HUNTER_ENTRAPMENT                     = 19184,
    SPELL_HUNTER_ENTRAPMENT_ROOT                = 19185,
    SPELL_HUNTER_SPIRIT_BOND_5_SEC              = 24529,    // rank 2: 1% every 5 sec (area aura cast by the pet: pet and hunter)
    SPELL_HUNTER_SPIRIT_BOND_10_SEC             = 1310725,  // rank 1: 1% every 10 sec
    SPELL_HUNTER_FOCUSED_FIRE                   = 1223755,
    SPELL_HUNTER_RAPID_RECUPERATION             = 1223987,
    SPELL_HUNTER_RAPID_RECUPERATION_BUFF        = 1242512,
    SPELL_HUNTER_LACERATING_STRIKES_BLEED       = 1310536,
    SPELL_HUNTER_KILL_COMMAND_BUFF              = 409396,   // 3 stacks of +20% pet Claw / Bite damage (spell mod on the hunter)
    SPELL_HUNTER_KILL_COMMAND_PET               = 409418,   // on the pet: each Claw / Bite takes one stack
    SPELL_HUNTER_RAPID_KILLING                  = 415405,   // effect 1: the next Shot bonus (dummy)
    SPELL_HUNTER_RAPID_KILLING_BUFF             = 415407,   // next Shot damage, 1 charge
    SPELL_HUNTER_EXPOSE_PREY_STATE              = 1310726,  // gives the defensive aura state (SpellInfo::_LoadAuraState): Mongoose Bite
    SPELL_HUNTER_IMPROVED_MEND_PET              = 19572,    // effect 0: cleanse chance per heal (dummy)
    SPELL_HUNTER_IMPROVED_MEND_PET_DISPEL       = 24406,    // dispel 1 Curse, Disease, Magic or Poison on the pet
};

// The client data of the talent spells these scripts read, once per spell at script validation: name, then per effect the effect,
// aura, value, misc value, trigger spell, period and the class mask, so the first run on the server shows what the amounts are.
static void LogTalentSpellOnce(SpellInfo const* spellInfo)
{
    static std::mutex lock;
    static std::unordered_set<uint32> seen;
    if (!spellInfo)
        return;
    {
        std::lock_guard guard(lock);
        if (!seen.insert(spellInfo->Id).second)
            return;
    }

    std::string effects;
    for (SpellEffectInfo const& effect : spellInfo->GetEffects())
    {
        if (!effect.IsEffect())
            continue;
        effects += Trinity::StringFormat(" [e{} effect={} aura={} value={} misc={} trigger={} period={} mask={:#x}/{:#x}/{:#x}]", uint32(effect.EffectIndex),
            uint32(effect.Effect), uint32(effect.ApplyAuraName), effect.CalcValueAsInt(), effect.MiscValue, effect.TriggerSpell,
            effect.ApplyAuraPeriod, effect.SpellClassMask[0], effect.SpellClassMask[1], effect.SpellClassMask[2]);
    }
    TC_LOG_INFO("playerbot.v2", "[talent_spell] {} \"{}\" duration={} charges={} chance={}{}", spellInfo->Id,
        spellInfo->SpellName ? (*spellInfo->SpellName)[sWorld->GetDefaultDbcLocale()] : "", spellInfo->GetDuration(), spellInfo->ProcCharges,
        spellInfo->ProcChance, effects);
}

static bool IsComboPointBuilder(SpellInfo const* spellInfo)
{
    if (!spellInfo)
        return false;

    for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
        if (spellEffectInfo.IsEffect(SPELL_EFFECT_ENERGIZE) && spellEffectInfo.MiscValue == POWER_COMBO_POINTS)
            return true;

    return false;
}

/*######
## Warrior
######*/

// 2457 Battle Stance, 71 Defensive Stance, 2458 Berserker Stance - stance change rage. Changing stances drops the rage above what
// Tactical Mastery (1310185, baseline at level 14: "You retain up to $m1 Rage") and Improved Tactical Mastery (12295 talent: "retain up
// to an additional $s1 Rage") keep (client tooltip of 1310185: ${$m1+$12295m1}). The core kept all rage on stance change, so neither
// spell did anything. Rage is stored x10.
class classic_spell_warr_stance_rage : public AuraScript
{
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* warrior = GetTarget();
        // not on login (auras load before the player enters the world)
        if (!warrior->IsPlayer() || !warrior->IsInWorld())
            return;

        int32 retain = 0;
        if (AuraEffect const* tacticalMastery = warrior->GetAuraEffect(SPELL_WARRIOR_TACTICAL_MASTERY, EFFECT_0))
            retain += tacticalMastery->GetAmountAsInt();
        if (AuraEffect const* improved = warrior->GetAuraEffect(SPELL_WARRIOR_IMPROVED_TACTICAL_MASTERY, EFFECT_0))
            retain += improved->GetAmountAsInt();

        retain = std::max(0, retain) * 10;
        if (warrior->GetPower(POWER_RAGE) > retain)
            warrior->SetPower(POWER_RAGE, retain);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_warr_stance_rage::HandleApply, EFFECT_0, SPELL_AURA_MOD_SHAPESHIFT, AURA_EFFECT_HANDLE_REAL);
    }
};

// 12317 - Enrage (talent): "Gives you a 30% chance to deal $m1% increased Physical damage for 12 sec after being the victim of any
// damaging attack." The 30% is the talent proc chance (client data); the buff is the vanilla Enrage 12880 (physical damage done) with the
// rank amount.
class classic_spell_warr_enrage_talent : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WARRIOR_ENRAGE_BUFF });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo const* damageInfo = eventInfo.GetDamageInfo();
        return damageInfo && damageInfo->GetDamage() > 0;
    }

    void HandleProc(ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        AuraEffect const* talent = GetEffect(EFFECT_0);
        if (!talent || talent->GetAmountAsInt() <= 0)
            return;

        Unit* warrior = GetTarget();
        warrior->CastSpell(warrior, SPELL_WARRIOR_ENRAGE_BUFF, CastSpellExtraArgs(talent)
            .SetTriggeringSpell(eventInfo.GetProcSpell())
            .AddSpellBP0(talent->GetAmount()));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_warr_enrage_talent::CheckProc);
        OnProc += AuraProcFn(classic_spell_warr_enrage_talent::HandleProc);
    }
};

// 16487 - Blood Craze: "Regenerates $m1% of your total Health over 6 sec after being the victim of a critical strike or suffering more
// than $m2% of your maximum Health from a single attack." 16488 heals a flat amount every 2 sec, so the rank percent of max health is
// split over its ticks. (The default dummy proc cast 16488 with its 1 point base on every hit.)
class classic_spell_warr_blood_craze : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_WARRIOR_BLOOD_CRAZE_REGEN }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo const* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return false;

        if (eventInfo.GetHitMask() & PROC_HIT_CRITICAL)
            return true;

        AuraEffect const* threshold = GetEffect(EFFECT_1);
        return threshold && damageInfo->GetDamage() > GetTarget()->CountPctFromMaxHealth(float(threshold->GetAmount()));
    }

    void HandleProc(ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();

        AuraEffect const* talent = GetEffect(EFFECT_0);
        SpellInfo const* regen = sSpellMgr->GetSpellInfo(SPELL_WARRIOR_BLOOD_CRAZE_REGEN, DIFFICULTY_NONE);
        if (!talent || talent->GetAmountAsInt() <= 0 || !regen || !regen->GetEffect(EFFECT_0).ApplyAuraPeriod)
            return;

        Unit* warrior = GetTarget();
        int32 ticks = std::max(1, regen->GetMaxDuration() / int32(regen->GetEffect(EFFECT_0).ApplyAuraPeriod));
        int32 perTick = std::max(1, int32(warrior->CountPctFromMaxHealth(float(talent->GetAmount())) / ticks));
        warrior->CastSpell(warrior, SPELL_WARRIOR_BLOOD_CRAZE_REGEN, CastSpellExtraArgs(talent).AddSpellBP0(perTick));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_warr_blood_craze::CheckProc);
        OnProc += AuraProcFn(classic_spell_warr_blood_craze::HandleProc);
    }
};

// 12834, 23255, 38716 - Deep Wounds (talent): "Your critical strikes cause the opponent to bleed, dealing $m1% of your melee weapon's
// average damage over 12 sec." The client talent is a proc trigger of a dummy effect (12162 / 23256) that no script finished, so a
// critical strike never made anybody bleed. The talent rank is the percent (aura amount, from the rank curve). The bleed 412609 is a
// periodic dummy aura (3 sec ticks); every tick casts 412613 for the aura amount. Vanilla (VMaNGOS Spell::EffectDummy, GPL-2.0): the
// bleed damage is the percent of the main hand weapon's average damage, spread over the ticks; a new critical strike refreshes the
// bleed and does not stack.
class classic_spell_warr_deep_wounds : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        LogTalentSpellOnce(spellInfo);
        LogTalentSpellOnce(sSpellMgr->GetSpellInfo(SPELL_WARRIOR_DEEP_WOUND, DIFFICULTY_NONE));
        LogTalentSpellOnce(sSpellMgr->GetSpellInfo(SPELL_WARRIOR_DEEP_WOUND_DAMAGE, DIFFICULTY_NONE));
        return ValidateSpellInfo({ SPELL_WARRIOR_DEEP_WOUND, SPELL_WARRIOR_DEEP_WOUND_DAMAGE });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Unit* victim = eventInfo.GetActionTarget();
        return victim && victim->IsAlive() && (eventInfo.GetHitMask() & PROC_HIT_CRITICAL);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* warrior = GetTarget();
        Unit* victim = eventInfo.GetActionTarget();
        SpellInfo const* bleed = sSpellMgr->GetSpellInfo(SPELL_WARRIOR_DEEP_WOUND, DIFFICULTY_NONE);
        if (!victim || !bleed || aurEff->GetAmountAsInt() <= 0)
            return;

        int32 const period = bleed->GetEffect(EFFECT_0).ApplyAuraPeriod;
        if (period <= 0 || bleed->GetMaxDuration() <= 0)
            return;

        float minDamage = 0.0f;
        float maxDamage = 0.0f;
        warrior->CalculateMinMaxDamage(BASE_ATTACK, false, true, minDamage, maxDamage);
        float const total = (minDamage + maxDamage) * 0.5f * float(aurEff->GetAmountAsInt()) / 100.0f;
        int32 const ticks = bleed->GetMaxDuration() / period;
        int32 const perTick = std::max(1, int32(total / float(ticks)));

        victim->RemoveAurasDueToSpell(SPELL_WARRIOR_DEEP_WOUND, warrior->GetGUID());
        warrior->CastSpell(victim, SPELL_WARRIOR_DEEP_WOUND, CastSpellExtraArgs(aurEff).AddSpellBP0(perTick));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_warr_deep_wounds::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_warr_deep_wounds::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 412609 - Deep Wound (the bleed): every tick deals the aura amount through 412613.
class classic_spell_warr_deep_wound_bleed : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WARRIOR_DEEP_WOUND_DAMAGE });
    }

    void HandleTick(AuraEffect const* aurEff)
    {
        PreventDefaultAction();

        Unit* warrior = GetCaster();
        Unit* victim = GetTarget();
        if (!warrior || !victim->IsAlive() || aurEff->GetAmountAsInt() <= 0)
            return;

        warrior->CastSpell(victim, SPELL_WARRIOR_DEEP_WOUND_DAMAGE, CastSpellExtraArgs(aurEff).AddSpellBP0(aurEff->GetAmountAsInt()));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_warr_deep_wound_bleed::HandleTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 12319 - Flurry (warrior talent): "Increases your attack speed by $m1% for your next 3 swings after dealing a melee critical strike."
// The client talent is a dummy aura that nothing handled; the buff is 12966 (3 charges) with the talent rank as the haste.
class classic_spell_warr_flurry : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        LogTalentSpellOnce(spellInfo);
        LogTalentSpellOnce(sSpellMgr->GetSpellInfo(SPELL_WARRIOR_FLURRY_BUFF, DIFFICULTY_NONE));
        return ValidateSpellInfo({ SPELL_WARRIOR_FLURRY_BUFF });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();

        CastSpellExtraArgs args(aurEff);
        if (aurEff->GetAmount() > 0)
            args.AddSpellBP0(aurEff->GetAmount());
        GetTarget()->CastSpell(GetTarget(), SPELL_WARRIOR_FLURRY_BUFF, args);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_warr_flurry::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 12322 - Unbridled Wrath (talent): "Gives you a $m1% chance to generate an additional Rage point when you deal melee damage with a weapon."
// The client talent triggers 12964 (one rage) with the spell's own 100% proc chance; the rank (aura amount) is the chance.
class classic_spell_warr_unbridled_wrath : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        LogTalentSpellOnce(spellInfo);
        LogTalentSpellOnce(sSpellMgr->GetSpellInfo(12964, DIFFICULTY_NONE));
        return true;
    }

    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        AuraEffect const* talent = GetEffect(EFFECT_0);
        return talent && roll_chance(talent->GetAmountAsInt());
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_warr_unbridled_wrath::CheckProc);
    }
};

// 12292 - Sweeping Strikes: "Your next 5 melee attacks strike an additional nearby opponent." The client ability is a dummy aura with 5
// charges; nothing was bound to it (the retail script is bound to 12328, which is Death Wish here). Each melee hit that does damage
// uses a charge; the copy is 12723 with the damage of the attack, on a nearby enemy of the target when there is one.
class classic_spell_warr_sweeping_strikes : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        LogTalentSpellOnce(spellInfo);
        LogTalentSpellOnce(sSpellMgr->GetSpellInfo(SPELL_WARRIOR_SWEEPING_STRIKES_STRIKE, DIFFICULTY_NONE));
        return ValidateSpellInfo({ SPELL_WARRIOR_SWEEPING_STRIKES_STRIKE });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo const* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return false;

        SpellInfo const* spellInfo = damageInfo->GetSpellInfo();
        return !spellInfo || (spellInfo->Id != SPELL_WARRIOR_SWEEPING_STRIKES_STRIKE && spellInfo->Id != SPELL_WARRIOR_SWEEPING_STRIKES_NORMALIZED);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* warrior = GetTarget();
        Unit* other = warrior->SelectNearbyTarget(eventInfo.GetActionTarget());
        if (!other)
            return;

        CastSpellExtraArgs args(aurEff);
        args.AddSpellBP0(eventInfo.GetDamageInfo()->GetDamage());
        warrior->CastSpell(other, SPELL_WARRIOR_SWEEPING_STRIKES_STRIKE, args);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_warr_sweeping_strikes::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_warr_sweeping_strikes::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 12975 - Last Stand: "Temporarily grants you 30% of your maximum health for 20 sec." The client spell is a dummy effect and the health aura
// 12976 carries a flat amount, so nothing gave 30% of the maximum health. VMaNGOS casts 12976 with 30% of the maximum health as the amount.
class classic_spell_warr_last_stand : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        LogTalentSpellOnce(spellInfo);
        return ValidateSpellInfo({ SPELL_WARRIOR_LAST_STAND_HEALTH });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
        args.AddSpellBP0(int32(caster->GetMaxHealth() * 0.3f));
        caster->CastSpell(caster, SPELL_WARRIOR_LAST_STAND_HEALTH, args);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_warr_last_stand::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 20230 - Retaliation: "Instantly counterattack any enemy that strikes you in melee for 15 sec. Melee attacks made from behind cannot be
// counterattacked. A maximum of 30 attacks will cause retaliation." The client aura is a dummy with 30 charges; the strike is 20240. A melee hit
// taken from the front uses a charge and answers with the strike; a stunned warrior does not counterattack (VMaNGOS).
class classic_spell_warr_retaliation : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        LogTalentSpellOnce(spellInfo);
        return ValidateSpellInfo({ SPELL_WARRIOR_RETALIATION_STRIKE });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Unit* warrior = GetTarget();
        Unit* attacker = eventInfo.GetActor();
        return attacker && !warrior->HasUnitState(UNIT_STATE_STUNNED) && warrior->isInFront(attacker);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(eventInfo.GetActor(), SPELL_WARRIOR_RETALIATION_STRIKE, CastSpellExtraArgs(aurEff).SetTriggerFlags(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_warr_retaliation::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_warr_retaliation::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 23922, 23923, 23924, 23925 - Shield Slam: "Slam the target with your shield, causing damage plus your shield block value and dispelling
// 1 magic effect. Has a 50% chance of dispelling" (VMaNGOS: the school damage adds the shield block value, the dispel only works half of
// the time). The client spell has the flat damage only and always dispels.
class classic_spell_warr_shield_slam : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        LogTalentSpellOnce(spellInfo);
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void HandleDispel(SpellEffIndex effIndex)
    {
        if (!roll_chance(50))
            PreventHitDefaultEffect(effIndex);
    }

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        SetEffectValue(GetEffectValue() + int32(GetCaster()->GetClassicShieldBlockValue()));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_warr_shield_slam::HandleDispel, EFFECT_0, SPELL_EFFECT_DISPEL);
        OnEffectLaunchTarget += SpellEffectFn(classic_spell_warr_shield_slam::HandleDamage, EFFECT_1, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 23881, 23892, 23893, 23894 - Bloodthirst: "Instantly attack the target causing damage equal to $s2% of your attack power." The client spell
// has a school damage effect with the rank's flat points (30 / 37 / 43 / 48, the TBC values: 27 damage a hit against armor, damage_parity
// 144) and the percent of attack power in a dummy effect (45) that nothing read. The damage is the percent of attack power, with the damage
// bonuses and the armor and crit of a melee special attack applied on top as for any school damage.
class classic_spell_warr_bloodthirst : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        LogTalentSpellOnce(spellInfo);
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void CalculateDamage(SpellEffectInfo const& spellEffectInfo, Unit* /*victim*/, int32& damage, int32& flatMod, float& /*pctMod*/) const
    {
        if (spellEffectInfo.EffectIndex != EFFECT_0)
            return;

        Unit* warrior = GetCaster();
        float const percent = float(GetSpellInfo()->GetEffect(EFFECT_1).CalcValue(warrior));
        flatMod += int32(warrior->GetTotalAttackPowerValue(BASE_ATTACK) * percent / 100.0f) - damage;
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_warr_bloodthirst::CalculateDamage);
    }
};

// 402927 - Victory Rush (Classic 1.60): "Instantly attack the target causing ${1+$AP*$m3/100} damage and healing you for $s2% of your
// maximum health. Only useable within $402975d after you kill a non-trivial enemy." The client spell's school damage effect has only the 1
// and the percent of attack power (15) sits in a dummy effect that nothing read, so the attack hit for 1 or 2. The damage is 1 plus that
// percent of attack power, with the damage bonuses, armor and crit of a melee special attack on top. One use per kill: the Victorious
// buff (402975) goes when Victory Rush is used, as the retail script does for its own buff.
class classic_spell_warr_victory_rush : public SpellScript
{
    static constexpr uint32 SPELL_VICTORIOUS_CLASSIC = 402975;

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 } });
    }

    void CalculateDamage(SpellEffectInfo const& spellEffectInfo, Unit* /*victim*/, int32& damage, int32& flatMod, float& /*pctMod*/) const
    {
        if (spellEffectInfo.EffectIndex != EFFECT_0)
            return;

        Unit* warrior = GetCaster();
        float const percent = float(GetSpellInfo()->GetEffect(EFFECT_2).CalcValue(warrior));
        flatMod += 1 + int32(warrior->GetTotalAttackPowerValue(BASE_ATTACK) * percent / 100.0f) - damage;
    }

    void ConsumeVictorious()
    {
        GetCaster()->RemoveAurasDueToSpell(SPELL_VICTORIOUS_CLASSIC);
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_warr_victory_rush::CalculateDamage);
        AfterCast += SpellCastFn(classic_spell_warr_victory_rush::ConsumeVictorious);
    }
};

// 1310222 - Spearing Strike (70532): "A brutal attack that deals $s2% weapon damage. Deals an additional ${$s2*$s3}% weapon damage against
// Giants, Dragonkin, and mounted targets. Mounted targets are dismounted." Effect 2 is the multiplier as a dummy: against those targets the
// weapon damage is $s2 x (1 + $s3) percent, and a mounted target is dismounted by the hit.
class classic_spell_warr_spearing_strike : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 } });
    }

    static bool IsSpearingTarget(Unit const* victim)
    {
        uint32 const creatureType = victim->GetCreatureType();
        return creatureType == CREATURE_TYPE_GIANT || creatureType == CREATURE_TYPE_DRAGONKIN || victim->IsMounted();
    }

    void CalculateDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        if (victim && IsSpearingTarget(victim))
            AddPct(pctMod, GetSpellInfo()->GetEffect(EFFECT_2).CalcValue(GetCaster()) * 100);
    }

    void HandleDismount(SpellEffIndex /*effIndex*/)
    {
        Unit* target = GetHitUnit();
        if (!target || !target->IsMounted())
            return;

        target->RemoveAurasByType(SPELL_AURA_MOUNTED);
        target->Dismount();
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_warr_spearing_strike::CalculateDamage);
        OnEffectHitTarget += SpellEffectFn(classic_spell_warr_spearing_strike::HandleDismount, EFFECT_2, SPELL_EFFECT_DUMMY);
    }
};

// 1680 - Whirlwind, with 1310315 - Improved Whirlwind (Forever talent, client 70790): "Reduces the cooldown of your Whirlwind ability by
// ${$m1/-1000} sec and increases the damage it deals to the closest enemy by $s2%." Effect 0 (spell modifier on the cooldown) works by
// itself; effect 1 is a dummy that nothing read, so the closest enemy took no extra damage. The talent aura's effect 1 amount is the rank
// value (TraitDefinitionEffectPoints curve 128138). Whirlwind hits up to $i enemies, picked at random when more stand in reach; the
// closest one is always among them here, and its hit gets the bonus.
class classic_spell_warr_improved_whirlwind_closest : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellEffect({ { SPELL_WARRIOR_IMPROVED_WHIRLWIND_CLASSIC, EFFECT_1 } });
    }

    bool Load() override
    {
        return GetCaster()->HasAura(SPELL_WARRIOR_IMPROVED_WHIRLWIND_CLASSIC);
    }

    void SelectClosest(std::list<WorldObject*>& targets)
    {
        _closest.Clear();
        WorldObject const* caster = GetCaster();
        WorldObject* closest = nullptr;
        float closestDist = 0.0f;
        for (WorldObject* target : targets)
        {
            if (!target->IsUnit())
                continue;
            float const dist = caster->GetExactDist(target);
            if (!closest || dist < closestDist)
            {
                closest = target;
                closestDist = dist;
            }
        }
        if (!closest)
            return;

        // The core trims an area list to the spell's target cap at random after this hook; trim it here with the closest kept, so the core
        // trim has nothing left to drop.
        if (uint32 const maxTargets = GetSpellInfo()->MaxAffectedTargets; maxTargets && targets.size() > maxTargets)
        {
            targets.remove(closest);
            Trinity::Containers::RandomResize(targets, maxTargets - 1);
            targets.push_back(closest);
        }
        _closest = closest->GetGUID();
    }

    void CalculateDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        if (!victim || victim->GetGUID() != _closest)
            return;
        if (AuraEffect const* talent = GetCaster()->GetAuraEffect(SPELL_WARRIOR_IMPROVED_WHIRLWIND_CLASSIC, EFFECT_1))
            AddPct(pctMod, talent->GetAmount());
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(classic_spell_warr_improved_whirlwind_closest::SelectClosest, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        CalcDamage += SpellCalcDamageFn(classic_spell_warr_improved_whirlwind_closest::CalculateDamage);
    }

    ObjectGuid _closest;
};

// 1290261 - Weaponmaster (warrior) and 13960 - Hack and Slash (rogue), Forever talents: "Gives your melee weapon attacks a benefit
// depending on the weapon." The client data has a hidden passive per weapon group with the weapon requirement (SpellEquippedItems):
//   Weaponmaster:   $s1 crit    Axe/Polearm -> 12700 (aura 290)     $s2 armor ignored  Mace/Staff -> 12284 (aura 280)
//                   $s3 extra attack chance  Sword -> 12281 (proc 1257049 extra attack)
//   Hack and Slash: $s1 crit    Dagger/Fist -> 13706 (aura 290)     $s2 armor ignored  Mace -> 13709 (aura 280)
//                   $s3 extra attack chance  Axe/Sword -> 1290312 (proc 16459 extra attack)
// The talent keeps the passive that fits the equipped weapon on the player with the rank value (effect i of the talent -> passive i).
// The core removes item dependent auras when the weapon is unequipped; the heartbeat adds the passive back for a fitting weapon.
class classic_spell_weapon_type_talent : public AuraScript
{
public:
    explicit classic_spell_weapon_type_talent(uint32 critSpell, uint32 armorSpell, uint32 extraAttackSpell) : _passives{ critSpell, armorSpell, extraAttackSpell } { }

private:
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ _passives[0], _passives[1], _passives[2] }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 } });
    }

    void UpdatePassives()
    {
        Player* player = GetUnitOwner()->ToPlayer();
        if (!player)
            return;

        for (uint8 i = 0; i < _passives.size(); ++i)
        {
            uint32 passiveId = _passives[i];
            SpellInfo const* passive = sSpellMgr->GetSpellInfo(passiveId, DIFFICULTY_NONE);
            AuraEffect const* talent = GetEffect(i);
            int32 amount = talent ? talent->GetAmountAsInt() : 0;

            Aura* current = player->GetAura(passiveId, player->GetGUID());
            if (!passive || amount <= 0 || !player->HasItemFitToSpellRequirements(passive))
            {
                if (current)
                    player->RemoveAurasDueToSpell(passiveId, player->GetGUID());
                continue;
            }

            if (current)
            {
                if (AuraEffect const* currentEffect = current->GetEffect(EFFECT_0); currentEffect && currentEffect->GetAmountAsInt() == amount)
                    continue;
                player->RemoveAurasDueToSpell(passiveId, player->GetGUID());
            }

            player->CastSpell(player, passiveId, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(amount));
        }
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        UpdatePassives();
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* owner = GetUnitOwner();
        for (uint32 passiveId : _passives)
            owner->RemoveAurasDueToSpell(passiveId, owner->GetGUID());
    }

    // Hack and Slash has a 1% dummy proc in the client data; the passives do the work
    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        return false;
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_weapon_type_talent::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_weapon_type_talent::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnHeartbeat += AuraHeartbeatFn(classic_spell_weapon_type_talent::UpdatePassives);
        DoCheckProc += AuraCheckProcFn(classic_spell_weapon_type_talent::CheckProc);
    }

    std::array<uint32, 3> _passives;
};

// 12281 - Weaponmaster (sword), 1290312 - Hack and Slash (axe/sword): extra attack passives of the talents above. The proc chance is the
// talent rank ($s3, put into this aura amount by classic_spell_weapon_type_talent; spell_proc Chance 100).
class classic_spell_weapon_extra_attack_chance : public AuraScript
{
    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        AuraEffect const* chance = GetEffect(EFFECT_0);
        return chance && roll_chance(chance->GetAmountAsInt());
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_weapon_extra_attack_chance::CheckProc);
    }
};

// 403228 - Meathook (warrior ability): "Throw out a hooked chain to restrain the target and pull them toward you. May not be used on
// enemies higher than level 61 or which are marked as boss enemies." The client spell is a dummy; the target jumps in front of the
// warrior with 403233 (jump to destination, stun immunity while pulled).
class classic_spell_warr_meathook : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WARRIOR_MEATHOOK_PULL });
    }

    SpellCastResult CheckCast()
    {
        Unit* target = GetExplTargetUnit();
        if (!target)
            return SPELL_FAILED_BAD_TARGETS;

        if (target->GetLevel() > 61)
            return SPELL_FAILED_HIGHLEVEL;

        if (Creature const* creature = target->ToCreature())
            if (creature->isWorldBoss() || creature->IsDungeonBoss())
                return SPELL_FAILED_BAD_TARGETS;

        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* warrior = GetCaster();
        Unit* target = GetHitUnit();
        if (!target || target == warrior || !target->IsAlive())
            return;

        Position dest = warrior->GetFirstCollisionPosition(warrior->GetCombatReach() + target->GetCombatReach(), 0.0f);
        target->CastSpell(dest, SPELL_WARRIOR_MEATHOOK_PULL, CastSpellExtraArgs(TRIGGERED_FULL_MASK)
            .SetOriginalCaster(warrior->GetGUID())
            .SetTriggeringSpell(GetSpell()));
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_warr_meathook::CheckCast);
        OnEffectHitTarget += SpellEffectFn(classic_spell_warr_meathook::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 5308, 20658, 20660, 20661, 20662 - Execute: "Attempt to finish off a wounded foe, causing damage and consuming all rage. Only usable on
// enemies with less than 20% health." The client spell is a dummy effect (base points 125 / 200 / 325 / 450 / 600) next to a marker aura,
// the damage spell is 20647. Vanilla: the dummy casts 20647 for the base points plus 3 / 6 / 9 / 12 / 15 per rage point left after the
// 15 rage cost, then the rage is gone. Without this nothing cast the damage spell (animation, no damage, no log line).
class classic_spell_warr_execute : public SpellScript
{
    static constexpr int32 EXECUTE_RAGE_COST = 150;     // 15 rage, power values are in tenths

    static int32 GetDamagePerRage(uint32 spellId)
    {
        switch (spellId)
        {
            case 5308:  return 3;
            case 20658: return 6;
            case 20660: return 9;
            case 20661: return 12;
            case 20662: return 15;
            default:    return 0;
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_WARRIOR_EXECUTE_DAMAGE }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_0 } });
    }

    // VMaNGOS (scripts/spells/spell_warrior.cpp, WarriorExecuteDummyScript): the damage spell is cast from OnCast, not from the effect, with
    // the base points plus the rage still held times the per-rage factor, and the rage is gone afterwards.
    void HandleCast()
    {
        Unit* warrior = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (!target)
        {
            TC_LOG_ERROR("scripts.classic", "Execute {}: cast without a target unit", GetSpellInfo()->Id);
            return;
        }

        // The cost is taken before the cast: what is left of the rage, plus whatever the cast took beyond the 15 rage
        // (a cost row that drains everything), is what Execute turns into damage.
        int32 spent = GetSpell()->GetPowerTypeCostAmount(POWER_RAGE).value_or(0);
        int32 extraRage = std::max(0, spent - EXECUTE_RAGE_COST) + int32(warrior->GetPower(POWER_RAGE));
        int32 damage = GetSpellInfo()->GetEffect(EFFECT_0).CalcValue(warrior) + (extraRage / 10) * GetDamagePerRage(GetSpellInfo()->Id);

        SpellCastResult result = warrior->CastSpell(target, SPELL_WARRIOR_EXECUTE_DAMAGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK)
            .SetTriggeringSpell(GetSpell())
            .AddSpellBP0(damage));
        TC_LOG_ERROR("scripts.classic", "Execute {}: damage spell {} on {} for {} (extra rage {}) -> cast result {}",
            GetSpellInfo()->Id, SPELL_WARRIOR_EXECUTE_DAMAGE, target->GetGUID().ToString(), damage, extraRage, uint32(result));
        warrior->SetPower(POWER_RAGE, 0);
    }

    void Register() override
    {
        OnCast += SpellCastFn(classic_spell_warr_execute::HandleCast);
    }
};

// 7384, 7887, 11584, 11585 - Overpower: the client spell costs 1 combo point next to its rage, and a warrior gets that point when the target
// dodges (Unit::ProcSkillsAndReactives). Runs that cast it without any dodge say the cost is not enforced on some path; this logs what the
// cast sees (combo points held, combo cost in the spell, ordinary or triggered cast) so a run shows which.
class classic_spell_warr_overpower_probe : public SpellScript
{
    SpellCastResult CheckCastProbe()
    {
        Unit* warrior = GetCaster();
        TC_LOG_ERROR("scripts.classic", "Overpower {}: combo points held {}, combo cost in the cast {}, rage {}, triggered {}",
            GetSpellInfo()->Id, warrior->GetPower(POWER_COMBO_POINTS), GetSpell()->GetPowerTypeCostAmount(POWER_COMBO_POINTS).value_or(-1),
            warrior->GetPower(POWER_RAGE), GetSpell()->IsTriggered());
        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_warr_overpower_probe::CheckCastProbe);
    }
};

// 20647 - Execute damage: the dummy casts it and the cast reports success, yet no damage reached the target in the 135 sweep. This logs what the
// triggered spell itself sees, in order: check, effect launch (the value that will be dealt) and the hit (damage dealt).
class classic_spell_warr_execute_damage_probe : public SpellScript
{
    SpellCastResult CheckCastProbe()
    {
        TC_LOG_ERROR("scripts.classic", "Execute damage 20647: check cast, triggered {}, explicit target {}", GetSpell()->IsTriggered(),
            GetExplTargetUnit() ? GetExplTargetUnit()->GetName() : "none");
        return SPELL_CAST_OK;
    }

    void LaunchProbe(SpellEffIndex /*effIndex*/)
    {
        TC_LOG_ERROR("scripts.classic", "Execute damage 20647: effect launch on {}, effect value {}", GetHitUnit() ? GetHitUnit()->GetName() : "none", GetEffectValue());
    }

    void HitProbe()
    {
        TC_LOG_ERROR("scripts.classic", "Execute damage 20647: hit {}, damage {}", GetHitUnit() ? GetHitUnit()->GetName() : "none", GetHitDamage());
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_warr_execute_damage_probe::CheckCastProbe);
        OnEffectLaunchTarget += SpellEffectFn(classic_spell_warr_execute_damage_probe::LaunchProbe, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
        AfterHit += SpellHitFn(classic_spell_warr_execute_damage_probe::HitProbe);
    }
};

/*######
## Rogue
######*/

// 13983 - Setup: "Gives you a $m1% chance to add a Combo Point to your target after Dodging one of their attacks or fully resisting one
// of their spells. (1s cooldown)" spell_proc: dodge / full resist only, 1 s cooldown. 15250 gives the point; cast on the attacker, so
// Spell::EffectEnergize makes it the combo target. Like VMaNGOS, only when the attacker is the rogue's current victim. (The default
// dummy proc cast 15250 on every hit taken.)
class classic_spell_rog_setup : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_SETUP_COMBO_POINT });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Unit* attacker = eventInfo.GetActor();
        AuraEffect const* talent = GetEffect(EFFECT_0);
        if (!attacker || attacker == GetTarget() || !attacker->IsAlive() || GetTarget()->GetVictim() != attacker || !talent)
            return false;

        return roll_chance(talent->GetAmountAsInt());
    }

    void HandleProc(ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(eventInfo.GetActor(), SPELL_ROGUE_SETUP_COMBO_POINT, CastSpellExtraArgs(GetEffect(EFFECT_0)));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_rog_setup::CheckProc);
        OnProc += AuraProcFn(classic_spell_rog_setup::HandleProc);
    }
};

// 14186 - Seal Fate: "Your critical strikes from abilities that add Combo Points have a $m1% chance to add an additional Combo Point.
// (500ms cooldown)" spell_proc: crits only, 500 ms cooldown, triggered spells may proc (Shiv strike). 14189 adds the point on the target.
class classic_spell_rog_seal_fate : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_SEAL_FATE_COMBO_POINT });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        AuraEffect const* talent = GetEffect(EFFECT_0);
        Unit* target = eventInfo.GetActionTarget();
        if (!talent || !target || target == GetTarget() || !target->IsAlive() || !IsComboPointBuilder(eventInfo.GetSpellInfo()))
            return false;

        return roll_chance(talent->GetAmountAsInt());
    }

    void HandleProc(ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(eventInfo.GetActionTarget(), SPELL_ROGUE_SEAL_FATE_COMBO_POINT, CastSpellExtraArgs(GetEffect(EFFECT_0)));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_rog_seal_fate::CheckProc);
        OnProc += AuraProcFn(classic_spell_rog_seal_fate::HandleProc);
    }
};

// 462708 - Cutthroat: "Your Backstab has a $m1% chance to cause your next Ambush within 10 sec to not require Stealth." 462707 (aura 275,
// ignore the stance requirement of Ambush, 1 charge) is the client buff; its charge is used by the next Ambush (spell_proc).
class classic_spell_rog_cutthroat : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_CUTTHROAT_BUFF });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        AuraEffect const* talent = GetEffect(EFFECT_0);
        // Backstab (rogue family flag 0x4)
        if (!talent || !spellInfo || spellInfo->SpellFamilyName != SPELLFAMILY_ROGUE || !(spellInfo->SpellFamilyFlags[0] & 0x00000004))
            return false;

        return roll_chance(talent->GetAmountAsInt());
    }

    void HandleProc(ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(GetTarget(), SPELL_ROGUE_CUTTHROAT_BUFF, CastSpellExtraArgs(GetEffect(EFFECT_0)));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_rog_cutthroat::CheckProc);
        OnProc += AuraProcFn(classic_spell_rog_cutthroat::HandleProc);
    }
};

// 424799 - Shiv, 425012 - Poisoned Knife (rogue abilities): the client spells are dummies that cast the off-hand strike (424800 / 425013,
// off-hand normalized weapon damage, awards the combo point).
class classic_spell_rog_offhand_strike : public SpellScript
{
    static uint32 GetStrikeSpell(uint32 spellId)
    {
        return spellId == SPELL_ROGUE_POISONED_KNIFE ? SPELL_ROGUE_POISONED_KNIFE_STRIKE : SPELL_ROGUE_SHIV_STRIKE;
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ GetStrikeSpell(spellInfo->Id) });
    }

    SpellCastResult CheckCast()
    {
        Player* player = GetCaster()->ToPlayer();
        if (player && !player->GetWeaponForAttack(OFF_ATTACK, true))
            return SPELL_FAILED_EQUIPPED_ITEM_CLASS;

        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, GetStrikeSpell(GetSpellInfo()->Id), CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetTriggeringSpell(GetSpell()));
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_rog_offhand_strike::CheckCast);
        OnEffectHitTarget += SpellEffectFn(classic_spell_rog_offhand_strike::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 424800 - Shiv, 425013 - Poisoned Knife (the strikes): "with a 100% chance to apply the poison from your off-hand weapon to the target".
// Poisoned Knife also "Refunds $s3 Energy per application of Deadly Poison on the target" (425012 effect 2; counted before the new poison).
class classic_spell_rog_offhand_poison : public SpellScript
{
    static constexpr uint32 DEADLY_POISON_FAMILY_FLAG = 0x00010000;

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Player* rogue = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!rogue || !target || !target->IsAlive())
            return;

        if (GetSpellInfo()->Id == SPELL_ROGUE_POISONED_KNIFE_STRIKE)
        {
            int32 applications = 0;
            for (AuraEffect const* periodic : target->GetAuraEffectsByType(SPELL_AURA_PERIODIC_DAMAGE))
            {
                SpellInfo const* poison = periodic->GetSpellInfo();
                if (periodic->GetCasterGUID() == rogue->GetGUID() && poison->SpellFamilyName == SPELLFAMILY_ROGUE
                    && (poison->SpellFamilyFlags[0] & DEADLY_POISON_FAMILY_FLAG))
                    applications += periodic->GetBase()->GetStackAmount();
            }

            SpellInfo const* knife = sSpellMgr->GetSpellInfo(SPELL_ROGUE_POISONED_KNIFE, DIFFICULTY_NONE);
            if (applications && knife && knife->GetEffects().size() > EFFECT_2)
                if (int32 refund = knife->GetEffect(EFFECT_2).CalcValueAsInt(rogue) * applications; refund > 0)
                    rogue->EnergizeBySpell(rogue, knife, refund, POWER_ENERGY);
        }

        Item* offHand = rogue->GetWeaponForAttack(OFF_ATTACK, true);
        if (!offHand)
            return;

        SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(offHand->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT));
        if (!enchant)
            return;

        for (uint8 s = 0; s < MAX_ITEM_ENCHANTMENT_EFFECTS; ++s)
        {
            if (enchant->Effect[s] != ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
                continue;

            SpellInfo const* poison = sSpellMgr->GetSpellInfo(enchant->EffectArg[s], DIFFICULTY_NONE);
            if (!poison)
                continue;

            rogue->CastSpell(poison->IsPositive() ? static_cast<Unit*>(rogue) : target, poison->Id, CastSpellExtraArgs(offHand));
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_rog_offhand_poison::HandleHit, EFFECT_0, SPELL_EFFECT_NORMALIZED_WEAPON_DMG);
    }
};

// Mutilate (70532): "Instantly attacks with both weapons ... Damage increased by $m4% against Poisoned targets." The cast spell (399956,
// 1241582, 1241584, talent 1310707) holds the bonus as a dummy in effect 3 and triggers one strike per weapon; the strikes did the damage
// without the bonus. Bound to the strikes, which read the bonus of their Mutilate. Poisoned: any poison effect on the target.
class classic_spell_rog_mutilate_strike : public SpellScript
{
    static uint32 GetMutilateSpell(uint32 strikeId)
    {
        switch (strikeId)
        {
            case 399960: case 399961:   return 399956;
            case 1241585: case 1241588: return 1241582;
            case 1241586: case 1241590: return 1241584;
            case 1310705: case 1310706: return 1310707;
            default:                    return 0;
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { GetMutilateSpell(spellInfo->Id), EFFECT_3 } });
    }

    static bool IsPoisoned(Unit const* victim)
    {
        for (auto const& [spellId, application] : victim->GetAppliedAuras())
            if (application->GetBase()->GetSpellInfo()->Dispel == DISPEL_POISON)
                return true;
        return false;
    }

    void CalculateDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        if (!victim || !IsPoisoned(victim))
            return;

        if (SpellInfo const* mutilate = sSpellMgr->GetSpellInfo(GetMutilateSpell(GetSpellInfo()->Id), DIFFICULTY_NONE))
            AddPct(pctMod, mutilate->GetEffect(EFFECT_3).CalcValue(GetCaster()));
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_rog_mutilate_strike::CalculateDamage);
    }
};

// 14278 - Ghostly Strike (70532): "A strike that deals $s1% ($m4% if a Dagger is equipped in your Main Hand) weapon damage". Effect 0 is the
// weapon damage percent and effect 3 the dagger percent as a dummy, so a dagger got the lower one.
class classic_spell_rog_ghostly_strike : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_3 } });
    }

    void CalculateDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*damage*/, int32& /*flatMod*/, float& pctMod) const
    {
        Player* rogue = GetCaster()->ToPlayer();
        if (!rogue)
            return;

        Item const* mainHand = rogue->GetWeaponForAttack(BASE_ATTACK, true);
        if (!mainHand || mainHand->GetTemplate()->GetSubClass() != ITEM_SUBCLASS_WEAPON_DAGGER)
            return;

        float const normal = GetSpellInfo()->GetEffect(EFFECT_0).CalcValue(rogue);
        float const dagger = GetSpellInfo()->GetEffect(EFFECT_3).CalcValue(rogue);
        if (normal > 0.0f)
            pctMod *= dagger / normal;
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_rog_ghostly_strike::CalculateDamage);
    }
};

// 438040 - Redirect (rogue ability): "Removes all your existing combo points from your target and stores them for up to 15 sec. These
// stored combo points will be transferred to the next non-player enemy you hit with a melee or ranged ability." The stored points are
// kept in the self aura (effect 1, 1 proc charge); the transfer sets UnitData ClassicComboTarget like Spell::EffectEnergize.
class classic_spell_rog_redirect : public SpellScript
{
    SpellCastResult CheckCast()
    {
        return GetCaster()->GetPower(POWER_COMBO_POINTS) > 0 ? SPELL_CAST_OK : SPELL_FAILED_NO_COMBO_POINTS;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_rog_redirect::CheckCast);
    }
};

class classic_spell_rog_redirect_aura : public AuraScript
{
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* rogue = GetTarget();
        if (AuraEffect* stored = GetEffect(EFFECT_1))
            stored->SetAmount(rogue->GetPower(POWER_COMBO_POINTS));
        rogue->SetPower(POWER_COMBO_POINTS, 0);
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Unit* target = eventInfo.GetActionTarget();
        AuraEffect const* stored = GetEffect(EFFECT_1);
        if (!stored || stored->GetAmountAsInt() <= 0 || !target || target == GetTarget() || !target->IsAlive())
            return false;

        if (target->IsCharmedOwnedByPlayerOrPlayer() || !GetTarget()->IsValidAttackTarget(target))
            return false;

        return !eventInfo.GetSpellInfo() || eventInfo.GetSpellInfo()->Id != SPELL_ROGUE_REDIRECT;
    }

    void HandleProc(ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Unit* rogue = GetTarget();
        Unit* target = eventInfo.GetActionTarget();
        int32 points = GetEffect(EFFECT_1)->GetAmountAsInt();
        // the ability may have already put points on this target
        if (rogue->GetClassicComboTarget() == target->GetGUID())
            points += rogue->GetPower(POWER_COMBO_POINTS);

        rogue->SetClassicComboTarget(target->GetGUID());
        rogue->SetPower(POWER_COMBO_POINTS, std::min(points, rogue->GetMaxPower(POWER_COMBO_POINTS)));
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_rog_redirect_aura::HandleApply, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        DoCheckProc += AuraCheckProcFn(classic_spell_rog_redirect_aura::CheckProc);
        OnProc += AuraProcFn(classic_spell_rog_redirect_aura::HandleProc);
    }
};

/*######
## Hunter
######*/

// 19184 - Entrapment: "When your traps are triggered, all affected targets are Entrapped, preventing them from moving for ${$m1/1000}
// sec." The rank amount is the root duration in ms (1000-5000) on the vanilla root 19185. Trap effects are cast by the trap object with
// the hunter as original caster: Freezing Trap 3355/14308/14309, Immolation Trap 13797/14298-14301, Explosive Trap 13812/14314/14315.
static void ClassicEntrap(Unit* hunter, Unit* target)
{
    if (!hunter || !target || hunter == target || !target->IsAlive() || !hunter->IsValidAttackTarget(target))
        return;

    AuraEffect const* entrapment = hunter->GetAuraEffect(SPELL_HUNTER_ENTRAPMENT, EFFECT_0);
    if (!entrapment || entrapment->GetAmountAsInt() <= 0)
        return;

    hunter->CastSpell(target, SPELL_HUNTER_ENTRAPMENT_ROOT, CastSpellExtraArgs(TRIGGERED_FULL_MASK)
        .AddSpellMod(SPELLVALUE_DURATION, entrapment->GetAmountAsInt()));
}

class classic_spell_hun_entrapment_trap : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HUNTER_ENTRAPMENT_ROOT });
    }

    void HandleHit()
    {
        ClassicEntrap(GetOriginalCaster(), GetHitUnit());
    }

    void Register() override
    {
        AfterHit += SpellHitFn(classic_spell_hun_entrapment_trap::HandleHit);
    }
};

// 13810 - Frost Trap Aura: the trap makes a persistent area; Entrapment roots each target when the slow lands on it
class classic_spell_hun_entrapment_frost_trap : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HUNTER_ENTRAPMENT_ROOT });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        ClassicEntrap(GetCaster(), GetTarget());
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_hun_entrapment_frost_trap::HandleApply, EFFECT_0, SPELL_AURA_MOD_DECREASE_SPEED, AURA_EFFECT_HANDLE_REAL);
    }
};

// 19578 - Spirit Bond: "While your pet is active, you and your pet will regenerate $m1% of total health every $m2 sec." $m2 is 10 at
// rank 1 and 5 at rank 2 (trait curve on the dummy effect 1). The client regen spells are area pet auras (TC: cast by the pet, on the pet
// and its owner): 1310725 every 10 sec, 24529 every 5 sec. The pet gets the right one when the talent applies and on every heartbeat.
class classic_spell_hun_spirit_bond : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_HUNTER_SPIRIT_BOND_5_SEC, SPELL_HUNTER_SPIRIT_BOND_10_SEC }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    uint32 GetRegenSpell() const
    {
        int32 period = GetSpellInfo()->GetEffect(EFFECT_1).CalcValueAsInt(GetUnitOwner());
        return period > 5 ? SPELL_HUNTER_SPIRIT_BOND_10_SEC : SPELL_HUNTER_SPIRIT_BOND_5_SEC;
    }

    void UpdatePet()
    {
        Player* hunter = GetUnitOwner()->ToPlayer();
        Pet* pet = hunter ? hunter->GetPet() : nullptr;
        if (!pet || !pet->IsAlive() || !pet->IsInWorld())
            return;

        uint32 regen = GetRegenSpell();
        uint32 other = regen == SPELL_HUNTER_SPIRIT_BOND_5_SEC ? SPELL_HUNTER_SPIRIT_BOND_10_SEC : SPELL_HUNTER_SPIRIT_BOND_5_SEC;
        pet->RemoveAurasDueToSpell(other, pet->GetGUID());
        if (!pet->HasAura(regen, pet->GetGUID()))
            pet->CastSpell(pet, regen, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        UpdatePet();
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* hunter = GetUnitOwner()->ToPlayer();
        if (!hunter)
            return;

        for (uint32 regen : { SPELL_HUNTER_SPIRIT_BOND_5_SEC, SPELL_HUNTER_SPIRIT_BOND_10_SEC })
        {
            hunter->RemoveAurasDueToSpell(regen);
            if (Pet* pet = hunter->GetPet())
                pet->RemoveAurasDueToSpell(regen);
        }
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_hun_spirit_bond::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_hun_spirit_bond::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnHeartbeat += AuraHeartbeatFn(classic_spell_hun_spirit_bond::UpdatePet);
    }
};

// 1223987 - Rapid Recuperation: "Hitting a target with your Serpent Sting ability grants you $s1% and consuming Rapid Killing grants you
// $s2% of your Mana regeneration while casting for the next 15 sec." Buff 1242512 (aura 134) with the rank amount. Serpent Sting: the
// talent proc (ranged ability hit, hunter family flag 0x4000). Rapid Killing: classic_spell_hun_rapid_killing_consumed.
class classic_spell_hun_rapid_recuperation : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HUNTER_RAPID_RECUPERATION_BUFF });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER && (spellInfo->SpellFamilyFlags[0] & 0x00004000)
            && spellInfo->HasAura(SPELL_AURA_PERIODIC_DAMAGE);
    }

    void HandleProc(ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();

        AuraEffect const* talent = GetEffect(EFFECT_0);
        if (!talent || talent->GetAmountAsInt() <= 0)
            return;

        GetTarget()->CastSpell(GetTarget(), SPELL_HUNTER_RAPID_RECUPERATION_BUFF, CastSpellExtraArgs(talent).AddSpellBP0(talent->GetAmount()));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_hun_rapid_recuperation::CheckProc);
        OnProc += AuraProcFn(classic_spell_hun_rapid_recuperation::HandleProc);
    }
};

// 415407 - Rapid Killing (buff, 1 charge used by the next shot): when the charge is consumed, Rapid Recuperation gives $s2% (effect 1)
class classic_spell_hun_rapid_killing_consumed : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HUNTER_RAPID_RECUPERATION_BUFF, SPELL_HUNTER_RAPID_RECUPERATION });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        // consumed charges remove the aura with the default mode; expiry, death and cancel use their own
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_DEFAULT)
            return;

        Unit* hunter = GetTarget();
        AuraEffect const* talent = hunter->GetAuraEffect(SPELL_HUNTER_RAPID_RECUPERATION, EFFECT_1);
        if (!talent || talent->GetAmountAsInt() <= 0)
            return;

        hunter->CastSpell(hunter, SPELL_HUNTER_RAPID_RECUPERATION_BUFF, CastSpellExtraArgs(talent).AddSpellBP0(talent->GetAmount()));
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_hun_rapid_killing_consumed::HandleRemove, EFFECT_0, SPELL_AURA_ADD_PCT_MODIFIER, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1310533 - Lacerating Strikes: "Your Mongoose Bite also causes the target to Bleed for damage equal to $s1% of the damage done by
// Mongoose Bite over 21 sec" - bleed 1310536 (periodic damage every 3 sec), the total split over its ticks.
class classic_spell_hun_lacerating_strikes : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HUNTER_LACERATING_STRIKES_BLEED });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        DamageInfo const* damageInfo = eventInfo.GetDamageInfo();
        // Mongoose Bite: hunter family flags 0x2 (word 0) and 0x1 (word 3)
        return spellInfo && damageInfo && damageInfo->GetDamage() > 0 && eventInfo.GetActionTarget()
            && spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER && (spellInfo->SpellFamilyFlags[0] & 0x00000002) && (spellInfo->SpellFamilyFlags[3] & 0x00000001);
    }

    void HandleProc(ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        AuraEffect const* talent = GetEffect(EFFECT_0);
        SpellInfo const* bleed = sSpellMgr->GetSpellInfo(SPELL_HUNTER_LACERATING_STRIKES_BLEED, DIFFICULTY_NONE);
        if (!talent || talent->GetAmountAsInt() <= 0 || !bleed || !bleed->GetEffect(EFFECT_0).ApplyAuraPeriod)
            return;

        int32 ticks = std::max(1, bleed->GetMaxDuration() / int32(bleed->GetEffect(EFFECT_0).ApplyAuraPeriod));
        int32 total = CalculatePct(int32(eventInfo.GetDamageInfo()->GetDamage()), talent->GetAmount());
        GetTarget()->CastSpell(eventInfo.GetActionTarget(), SPELL_HUNTER_LACERATING_STRIKES_BLEED, CastSpellExtraArgs(talent)
            .AddSpellBP0(std::max(1, total / ticks)));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_hun_lacerating_strikes::CheckProc);
        OnProc += AuraProcFn(classic_spell_hun_lacerating_strikes::HandleProc);
    }
};

// 409379 - Kill Command (hunter ability): "Give the command to kill, increasing your pet's damage done from Claw and Bite by 60% for 30
// sec. Each Claw or Bite done by the pet reduces the damage bonus by 20%." The client spell is a dummy aura; 409396 is the client buff
// (+20% Claw / Bite per stack, a spell mod on the hunter that pet spells use) and triggers 409418 on the pet, whose proc takes one stack.
class classic_spell_hun_kill_command : public SpellScript
{
    SpellCastResult CheckCast()
    {
        Player* hunter = GetCaster()->ToPlayer();
        Pet* pet = hunter ? hunter->GetPet() : nullptr;
        if (!pet || !pet->IsAlive())
            return SPELL_FAILED_NO_PET;

        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_hun_kill_command::CheckCast);
    }
};

class classic_spell_hun_kill_command_aura : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HUNTER_KILL_COMMAND_BUFF, SPELL_HUNTER_KILL_COMMAND_PET });
    }

    void HandleApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        Unit* hunter = GetTarget();
        hunter->CastSpell(hunter, SPELL_HUNTER_KILL_COMMAND_BUFF, CastSpellExtraArgs(aurEff));
        if (Aura* buff = hunter->GetAura(SPELL_HUNTER_KILL_COMMAND_BUFF, hunter->GetGUID()))
            buff->SetStackAmount(buff->GetSpellInfo()->StackAmount ? uint8(buff->GetSpellInfo()->StackAmount) : 3);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_HUNTER_KILL_COMMAND_BUFF, GetTarget()->GetGUID());
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_hun_kill_command_aura::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_hun_kill_command_aura::HandleRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 409418 - Kill Command (on the pet): each pet Claw / Bite (hunter family flag 0x8, word 3) takes one stack of the hunter's 409396; the
// pet aura (1 charge) stays until the last stack is used.
class classic_spell_hun_kill_command_pet : public AuraScript
{
    Aura* GetHunterBuff() const
    {
        Unit* hunter = GetCaster();
        return hunter ? hunter->GetAura(SPELL_HUNTER_KILL_COMMAND_BUFF, hunter->GetGUID()) : nullptr;
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER && (spellInfo->SpellFamilyFlags[3] & 0x00000008);
    }

    void PrepareProc(ProcEventInfo& /*eventInfo*/)
    {
        // keep the charge while the hunter has more than one stack left
        if (Aura* buff = GetHunterBuff(); buff && buff->GetStackAmount() > 1)
            PreventDefaultAction();
    }

    void HandleProc(ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        if (Aura* buff = GetHunterBuff())
            buff->ModStackAmount(-1);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_hun_kill_command_pet::CheckProc);
        DoPrepareProc += AuraProcFn(classic_spell_hun_kill_command_pet::PrepareProc);
        OnProc += AuraProcFn(classic_spell_hun_kill_command_pet::HandleProc);
    }
};

// 23989 - Readiness: "When activated, this ability immediately finishes the cooldown on your other Hunter abilities." The client spell is
// a single dummy effect and nothing handled it: the spell was cast, took its own five minute cooldown and reset nothing. Every hunter
// family cooldown except Readiness itself is finished, pet abilities and items are not touched. Same behaviour as the vMaNGOS script
// (GPL-2.0, src/scripts/spells/spell_hunter.cpp, HunterReadinessScript).
class classic_spell_hun_readiness : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* hunter = GetCaster();
        uint32 const readinessId = GetSpellInfo()->Id;
        hunter->GetSpellHistory()->ResetCooldowns([readinessId](SpellHistory::CooldownEntry const& cooldown) -> bool
        {
            if (cooldown.SpellId == readinessId)
                return false;
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(cooldown.SpellId, DIFFICULTY_NONE);
            return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER;
        }, true);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(classic_spell_hun_readiness::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 24531 - Refocus: "Instantly clears the cooldowns of Aimed Shot, Multishot, Volley, and Arcane Shot." The client spell is a single dummy
// effect that nothing handled. Behaviour after the vMaNGOS script (GPL-2.0, src/scripts/spells/spell_hunter.cpp, HunterRefocusScript),
// which clears the cooldown of every spell in those four hunter families except Refocus itself.
class classic_spell_hun_refocus : public SpellScript
{
    // Hunter family flag word 0 of Arcane Shot, Multi-Shot, Volley and Aimed Shot in the client data (SpellFamilyFlags[0]).
    static constexpr uint32 HUNTER_REFOCUS_FAMILY_MASK = 0x800 | 0x1000 | 0x2000 | 0x20000;

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        uint32 const refocusId = GetSpellInfo()->Id;
        GetCaster()->GetSpellHistory()->ResetCooldowns([refocusId](SpellHistory::CooldownEntry const& cooldown) -> bool
        {
            if (cooldown.SpellId == refocusId)
                return false;
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(cooldown.SpellId, DIFFICULTY_NONE);
            return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER && (spellInfo->SpellFamilyFlags[0] & HUNTER_REFOCUS_FAMILY_MASK);
        }, true);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(classic_spell_hun_refocus::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 415405 - Rapid Killing (70532): "when you kill a non-trivial enemy or it dies while afflicted by your Serpent Sting, you gain Rapid Killing,
// increasing the damage of your next Shot ability within $415407d by $m2%." Effect 1 is that bonus as a dummy and nothing gave the buff
// 415407, so it never happened. A kill by the hunter or the hunter's pet, or the death of a target with the hunter's Serpent Sting (hunter
// family flag 0x4000), gives the buff with the talent amount. Non-trivial: a player, or a creature that gives the hunter experience.
class classic_unit_hun_rapid_killing : public UnitScript
{
public:
    classic_unit_hun_rapid_killing() : UnitScript("classic_unit_hun_rapid_killing") { }

    static void TryGrant(Player* hunter, Unit* victim)
    {
        if (!hunter || hunter == victim)
            return;

        AuraEffect const* talent = hunter->GetAuraEffect(SPELL_HUNTER_RAPID_KILLING, EFFECT_1);
        if (!talent || talent->GetAmountAsInt() <= 0)
            return;

        Creature* creature = victim->ToCreature();
        if (!victim->IsPlayer() && (!creature || !Trinity::XP::Gain(hunter, creature)))
            return;

        hunter->CastSpell(hunter, SPELL_HUNTER_RAPID_KILLING_BUFF, CastSpellExtraArgs(talent).AddSpellBP0(talent->GetAmount()));
    }

    void OnUnitDeath(Unit* victim, Unit* killer) override
    {
        Player* killingHunter = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        TryGrant(killingHunter, victim);

        for (AuraEffect const* periodic : victim->GetAuraEffectsByType(SPELL_AURA_PERIODIC_DAMAGE))
        {
            SpellInfo const* sting = periodic->GetSpellInfo();
            if (sting->SpellFamilyName != SPELLFAMILY_HUNTER || !(sting->SpellFamilyFlags[0] & 0x4000))
                continue;

            Player* stingHunter = ObjectAccessor::GetPlayer(*victim, periodic->GetCasterGUID());
            if (stingHunter && stingHunter != killingHunter)
                TryGrant(stingHunter, victim);
        }
    }
};

// 1310532 - Expose Prey (70532): "Grants your melee and ranged attacks against enemies afflicted by Hunter's Mark a $s1% chance to allow an
// activation of Mongoose Bite. Lasts $1310726d." The talent triggers 1310726 on any hit; it now needs Hunter's Mark on the target (hunter
// family flag 0x400, from any hunter) and rolls the talent amount (spell_proc chance 100). 1310726 gives the state Mongoose Bite needs.
class classic_spell_hun_expose_prey : public AuraScript
{
    bool CheckProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        Unit* target = eventInfo.GetActionTarget();
        if (!target || target == GetTarget())
            return false;

        if (!target->GetAuraEffect(SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS, SPELLFAMILY_HUNTER, flag128(0x400, 0, 0, 0)))
            return false;

        return roll_chance(float(aurEff->GetAmount()));
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(classic_spell_hun_expose_prey::CheckProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// Mongoose Bite (1495, 14269, 14270, 14271): uses the Expose Prey activation (1310726, "allow an activation of Mongoose Bite").
class classic_spell_hun_mongoose_bite : public SpellScript
{
    void HandleCast()
    {
        GetCaster()->RemoveAurasDueToSpell(SPELL_HUNTER_EXPOSE_PREY_STATE);
    }

    void Register() override
    {
        OnCast += SpellCastFn(classic_spell_hun_mongoose_bite::HandleCast);
    }
};

// Mend Pet (136, 3111, 3661, 3662, 13542, 13543, 13544) with 19572 Improved Mend Pet: "Gives your Mend Pet spell a $s1% chance of cleansing
// $24406s1 Curse, Disease, Magic, or Poison effect from your pet each time it heals". The talent amount is a dummy. Behaviour from vMaNGOS
// (GPL-2.0, periodic heal of Mend Pet): each heal tick rolls the amount and casts 24406 on the pet.
class classic_spell_hun_mend_pet : public AuraScript
{
    void HandlePeriodic(AuraEffect const* aurEff)
    {
        Unit* hunter = GetCaster();
        if (!hunter)
            return;

        AuraEffect const* talent = hunter->GetAuraEffect(SPELL_HUNTER_IMPROVED_MEND_PET, EFFECT_0);
        if (talent && roll_chance(float(talent->GetAmount())))
            hunter->CastSpell(GetTarget(), SPELL_HUNTER_IMPROVED_MEND_PET_DISPEL, CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_hun_mend_pet::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_HEAL);
    }
};

// 1293525, 1293526, 1293527, talent 1293241 - Summon Hawk (70532): "Command a hawk to dive-bomb your targeted enemy, dealing
// ${$s1+($rap*($s4/100))} Physical damage". Effect 0 is the flat damage and effect 3 the percent of ranged attack power as a dummy that
// nothing read, so the dive did only the flat damage.
class classic_spell_hun_summon_hawk : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_3 } });
    }

    void CalculateDamage(SpellEffectInfo const& spellEffectInfo, Unit* /*victim*/, int32& /*damage*/, int32& flatMod, float& /*pctMod*/) const
    {
        if (spellEffectInfo.EffectIndex != EFFECT_0)
            return;

        Unit* hunter = GetCaster();
        float const percent = float(GetSpellInfo()->GetEffect(EFFECT_3).CalcValue(hunter));
        flatMod += int32(hunter->GetTotalAttackPowerValue(RANGED_ATTACK) * percent / 100.0f);
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_hun_summon_hawk::CalculateDamage);
    }
};

/*######
## Damage done talents without a client aura effect
######*/

// 1223755 - Focused Fire (hunter): "Increases all damage you and your pet deal by $s1% while your pet is active."
// 1310728 - Quietus (rogue): "Your Sinister Strike, Ghostly Strike, and Hemorrhage abilities cause $m1% more damage against targets
// below $m2% health." (rogue family flags 0x2 Sinister Strike, 0x4000000 Ghostly Strike, 0x2000000 Hemorrhage; Shiv and Poisoned Knife
// "benefit from all talents that modify Sinister Strike")
class classic_unit_talent_damage_mods : public UnitScript
{
public:
    classic_unit_talent_damage_mods() : UnitScript("classic_unit_talent_damage_mods") { }

    static float GetFocusedFirePct(Unit const* attacker)
    {
        if (!attacker)
            return 0.0f;

        if (Player const* hunter = attacker->ToPlayer())
        {
            AuraEffect const* talent = hunter->GetAuraEffect(SPELL_HUNTER_FOCUSED_FIRE, EFFECT_0);
            if (!talent)
                return 0.0f;

            Pet* pet = hunter->GetPet();
            return pet && pet->IsAlive() ? float(talent->GetAmount()) : 0.0f;
        }

        if (attacker->IsPet())
        {
            Player* hunter = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
            if (!hunter || hunter == attacker || hunter->GetPet() != attacker)
                return 0.0f;

            if (AuraEffect const* talent = hunter->GetAuraEffect(SPELL_HUNTER_FOCUSED_FIRE, EFFECT_0))
                return float(talent->GetAmount());
        }

        return 0.0f;
    }

    static float GetQuietusPct(Unit const* target, Unit const* attacker, SpellInfo const* spellInfo)
    {
        if (!target || !attacker || !spellInfo || !attacker->IsPlayer() || spellInfo->SpellFamilyName != SPELLFAMILY_ROGUE)
            return 0.0f;

        if (!(spellInfo->SpellFamilyFlags[0] & 0x06000002) && spellInfo->Id != SPELL_ROGUE_SHIV_STRIKE && spellInfo->Id != SPELL_ROGUE_POISONED_KNIFE_STRIKE)
            return 0.0f;

        AuraEffect const* bonus = attacker->GetAuraEffect(SPELL_ROGUE_QUIETUS, EFFECT_0);
        AuraEffect const* threshold = attacker->GetAuraEffect(SPELL_ROGUE_QUIETUS, EFFECT_1);
        if (!bonus || !threshold || !target->HealthBelowPct(float(threshold->GetAmount())))
            return 0.0f;

        return float(bonus->GetAmount());
    }

    void ModifyMeleeDamage(Unit* /*target*/, Unit* attacker, uint32& damage) override
    {
        if (float pct = GetFocusedFirePct(attacker); pct > 0.0f)
            AddPct(damage, pct);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (damage <= 0)
            return;

        float pct = GetFocusedFirePct(attacker) + GetQuietusPct(target, attacker, spellInfo);
        if (pct > 0.0f)
            AddPct(damage, pct);
    }

    void ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* attacker, uint32& damage) override
    {
        if (float pct = GetFocusedFirePct(attacker); pct > 0.0f)
            AddPct(damage, pct);
    }
};

// The warrior damage talents and passives that work through the core's own aura handling (no script): their client data, once at start,
// so a run shows the amounts and class masks the damage parity numbers depend on.
class classic_warrior_talent_data_log : public WorldScript
{
public:
    classic_warrior_talent_data_log() : WorldScript("classic_warrior_talent_data_log") { }

    void OnStartup() override
    {
        for (uint32 spellId : { 12163u, 20111u, 12282u, 12290u, 12963u, 12296u, 12320u, 12317u, 12328u, 1719u, 2687u, 29131u, 12880u, 16493u,
            12966u, 12284u, 12700u, 12281u, 1290261u, 412609u, 412613u })
            LogTalentSpellOnce(sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE));
    }
};

// One line per class talent spell (the spells of the trait node definitions with a class SpellFamilyName) at startup: what each effect is, which
// scripts are bound, whether a spell_proc row exists, and for the effects that need server code a verdict hint:
//   NEEDS_SCRIPT  dummy / script effect, dummy or periodic-dummy aura, class-script aura or proc of a dummy-only spell, with no script bound
//   MOD_NO_MATCH  a modifier aura whose class mask is set but matches no spell of the class (the talent changes nothing)
//   NO_PROC_ROW   a proc aura with no spell_proc row of ours or the retail base (the spell data's own proc flags decide)
// The nodes are the Classic 1.60 trait trees; ranks live on the node, the spell is one. Read with: grep "\[talent_cov\]" Playerbot.log
class classic_talent_coverage_log : public WorldScript
{
public:
    classic_talent_coverage_log() : WorldScript("classic_talent_coverage_log") { }

    void OnStartup() override
    {
        std::unordered_set<uint32> seen;
        for (TraitNodeEntryEntry const* nodeEntry : sTraitNodeEntryStore)
        {
            TraitDefinitionEntry const* definition = sTraitDefinitionStore.LookupEntry(nodeEntry->TraitDefinitionID);
            if (!definition)
                continue;
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(definition->SpellID, DIFFICULTY_NONE);
            if (!spellInfo || spellInfo->SpellFamilyName < SPELLFAMILY_MAGE || spellInfo->SpellFamilyName > SPELLFAMILY_SHAMAN || !seen.insert(spellInfo->Id).second)
                continue;

            std::string scripts;
            auto bounds = sObjectMgr->GetSpellScriptsBounds(spellInfo->Id);
            for (auto itr = bounds.first; itr != bounds.second; ++itr)
                scripts += sObjectMgr->GetScriptName(itr->second.first) + ",";

            SpellProcEntry const* proc = sSpellMgr->GetSpellProcEntry(spellInfo);
            std::string hints;
            std::string effects;
            bool hasProcAura = false;
            for (SpellEffectInfo const& effect : spellInfo->GetEffects())
            {
                if (!effect.IsEffect())
                    continue;
                if (effect.IsAura(SPELL_AURA_PROC_TRIGGER_SPELL) || effect.IsAura(SPELL_AURA_DUMMY) || effect.IsAura(SPELL_AURA_ADD_TARGET_TRIGGER))
                    hasProcAura = true;

                uint32 matches = 0;
                bool const hasMask = bool(effect.SpellClassMask);
                if (hasMask && effect.IsAura())
                    sSpellMgr->ForEachSpellInfo([&](SpellInfo const* other)
                    {
                        if (other->IsAffected(spellInfo->SpellFamilyName, effect.SpellClassMask))
                            ++matches;
                    });

                AuraType const aura = effect.ApplyAuraName;
                SpellInfo const* trigger = effect.TriggerSpell ? sSpellMgr->GetSpellInfo(effect.TriggerSpell, DIFFICULTY_NONE) : nullptr;
                bool triggerDummyOnly = false;
                if (trigger)
                {
                    triggerDummyOnly = true;
                    for (SpellEffectInfo const& t : trigger->GetEffects())
                        if (t.IsEffect() && !t.IsEffect(SPELL_EFFECT_DUMMY) && !t.IsEffect(SPELL_EFFECT_SCRIPT_EFFECT) && !t.IsAura(SPELL_AURA_DUMMY) && !t.IsAura(SPELL_AURA_PERIODIC_DUMMY))
                            triggerDummyOnly = false;
                }

                bool const needsScript = effect.IsEffect(SPELL_EFFECT_DUMMY) || effect.IsEffect(SPELL_EFFECT_SCRIPT_EFFECT) || aura == SPELL_AURA_DUMMY
                    || aura == SPELL_AURA_PERIODIC_DUMMY || aura == SPELL_AURA_OVERRIDE_CLASS_SCRIPTS
                    || ((aura == SPELL_AURA_PROC_TRIGGER_SPELL || aura == SPELL_AURA_ADD_TARGET_TRIGGER) && triggerDummyOnly);
                if (needsScript && scripts.empty())
                    hints += "NEEDS_SCRIPT ";
                if (hasMask && effect.IsAura() && !matches && aura != SPELL_AURA_DUMMY)
                    hints += Trinity::StringFormat("MOD_NO_MATCH(e{}) ", uint32(effect.EffectIndex));

                effects += Trinity::StringFormat(" [e{} {}/{} v={} misc={} trig={} mask={:#x}/{:#x}/{:#x} match={}]", uint32(effect.EffectIndex), uint32(effect.Effect),
                    uint32(aura), effect.CalcValueAsInt(), effect.MiscValue, effect.TriggerSpell, effect.SpellClassMask[0], effect.SpellClassMask[1],
                    effect.SpellClassMask[2], matches);
            }
            if (!proc && hasProcAura)
                hints += Trinity::StringFormat("NO_PROC_ROW(flags {:#x}) ", spellInfo->ProcFlags[0]);

            TC_LOG_INFO("playerbot.v2", "[talent_cov] fam={} id={} \"{}\" ranks={} scripts=[{}] proc={} {}|{}", uint32(spellInfo->SpellFamilyName), spellInfo->Id,
                spellInfo->SpellName ? (*spellInfo->SpellName)[sWorld->GetDefaultDbcLocale()] : "", nodeEntry->MaxRanks, scripts, proc ? "row" : "none", hints,
                effects);
        }
    }
};

// 1289849 - Main Gauche (rogue): "Grants a chance for your Main Hand melee auto attacks to trigger an instant extra attack with your Off Hand
// weapon for $s1% of normal damage." The client aura is a dummy (aura 4) naming the strike, 1289695 (off-hand normalized weapon damage, requires
// an off-hand weapon; effect 2 is the damage percentage). The chance is the spell_proc row of the aura.
class classic_spell_rog_main_gauche : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ spellInfo->GetEffect(EFFECT_0).TriggerSpell });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        // main hand auto attacks only (not abilities, not the off-hand swing, not the strike this aura casts), and an off-hand weapon to strike with
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || eventInfo.GetSpellInfo() || damageInfo->GetAttackType() != BASE_ATTACK)
            return false;

        Player* rogue = GetTarget()->ToPlayer();
        return rogue && rogue->GetWeaponForAttack(OFF_ATTACK, true) && eventInfo.GetActionTarget();
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(eventInfo.GetActionTarget(), GetSpellInfo()->GetEffect(EFFECT_0).TriggerSpell,
            CastSpellExtraArgs(aurEff).AddSpellMod(SPELLVALUE_BASE_POINT1, aurEff->GetAmountAsInt()));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_rog_main_gauche::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_rog_main_gauche::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 14168 - Improved Expose Armor (rogue): "Reduces the Energy cost of your Expose Armor ability by $m1, and refunds $m2 Combo Points when cast with
// $m3 Combo Points." Effect 1 (aura 231) names the refund spell, 1310697 (energize combo points). The refund is given after the finisher has
// spent its points (the finish phase proc, Spell::SpendPendingComboPoints runs first), and only for a cast with at least $m3 points.
class classic_spell_rog_improved_expose_armor : public AuraScript
{
    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Spell const* spell = eventInfo.GetProcSpell();
        AuraEffect const* required = GetEffect(EFFECT_2);
        if (!spell || !required)
            return false;

        int32 comboCost = 0;
        for (SpellPowerCost const& cost : spell->GetPowerCost())
            if (cost.Power == POWER_COMBO_POINTS)
                comboCost = cost.Amount;

        static std::atomic<uint32> logged{ 0 };
        if (logged.fetch_add(1) < 200)
            TC_LOG_ERROR("scripts.classic", "Improved Expose Armor: proc check for spell {} phase {:#x} hit {:#x}, combo cost {}, needed {}, refund {}, points now {}",
                spell->GetSpellInfo()->Id, uint32(eventInfo.GetSpellPhaseMask()), uint32(eventInfo.GetHitMask()), comboCost, required->GetAmountAsInt(),
                GetEffect(EFFECT_1) ? GetEffect(EFFECT_1)->GetAmountAsInt() : -1, GetTarget()->GetPower(POWER_COMBO_POINTS));

        return comboCost >= required->GetAmountAsInt();
    }

    // The refund is given directly: the trigger spell 1310697 refuses a self cast (result 13, bad targets), and the points are the whole effect.
    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        Unit* target = GetTarget();
        int32 const before = target->GetPower(POWER_COMBO_POINTS);
        target->ModifyPower(POWER_COMBO_POINTS, aurEff->GetAmountAsInt());
        static std::atomic<uint32> logged{ 0 };
        if (logged.fetch_add(1) < 200)
            TC_LOG_ERROR("scripts.classic", "Improved Expose Armor: refund of {} points, points {} -> {}", aurEff->GetAmountAsInt(), before, target->GetPower(POWER_COMBO_POINTS));
    }

    // Effect 2 only carries the required point count; it must not reach the default trigger handler (it names no spell).
    void HandleRequirementProc(AuraEffect const* /*aurEff*/, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_rog_improved_expose_armor::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_rog_improved_expose_armor::HandleProc, EFFECT_1, SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE);
        OnEffectProc += AuraEffectProcFn(classic_spell_rog_improved_expose_armor::HandleRequirementProc, EFFECT_2, SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE);
    }
};

// 415370 - Lone Wolf (hunter): "You deal $s2% increased damage with all attacks while you do not have an active pet." Effect 1 is the damage
// aura (aura 79), effect 2 the marker that carries the percentage; the damage aura is held at 0 while a pet is out.
class classic_spell_hun_lone_wolf : public AuraScript
{
    void Update()
    {
        // GetTarget() is empty in the heartbeat hook; the owner is valid in every hook.
        Unit* owner = GetUnitOwner();
        Player* hunter = owner ? owner->ToPlayer() : nullptr;
        AuraEffect* damage = GetEffect(EFFECT_0);
        AuraEffect const* marker = GetEffect(EFFECT_1);
        if (!hunter || !damage || !marker)
            return;

        SpellEffectValue const wanted = hunter->GetPet() ? 0.0 : marker->GetAmount();
        if (damage->GetAmount() != wanted)
            damage->ChangeAmount(wanted);
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Update();
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_hun_lone_wolf::HandleApply, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, AURA_EFFECT_HANDLE_REAL);
        OnHeartbeat += AuraHeartbeatFn(classic_spell_hun_lone_wolf::Update);
    }
};

// 12289 - Improved Hamstring, 12311 - Improved Shield Bash, 19228 - Improved Wing Clip, 19407 - Improved Concussive Shot (70532):
// "Gives your <ability> a $m1% chance to <immobilize / Silence / stun> the target". Each client spell is a proc trigger with 100 as
// spell proc chance, no class mask and the talent chance as the effect amount, so without a row it fired on every melee, ranged
// and spell hit. The spell_proc rows bind each to its one ability; the chance is rolled from the amount here.
class classic_spell_talent_ability_proc_chance : public AuraScript
{
    bool CheckProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        return roll_chance(float(aurEff->GetAmount()));
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(classic_spell_talent_ability_proc_chance::CheckProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

void AddSC_classic_talents_warrior_rogue_hunter()
{
    // warrior
    RegisterSpellScript(classic_spell_warr_stance_rage);
    RegisterSpellScript(classic_spell_warr_enrage_talent);
    RegisterSpellScript(classic_spell_warr_blood_craze);
    RegisterSpellScript(classic_spell_warr_deep_wounds);
    RegisterSpellScript(classic_spell_warr_deep_wound_bleed);
    RegisterSpellScript(classic_spell_warr_flurry);
    RegisterSpellScript(classic_spell_warr_bloodthirst);
    RegisterSpellScript(classic_spell_warr_unbridled_wrath);
    RegisterSpellScript(classic_spell_warr_sweeping_strikes);
    RegisterSpellScript(classic_spell_warr_last_stand);
    RegisterSpellScript(classic_spell_warr_retaliation);
    RegisterSpellScript(classic_spell_warr_shield_slam);
    RegisterSpellScript(classic_spell_warr_victory_rush);
    RegisterSpellScriptWithArgs(classic_spell_weapon_type_talent, "classic_spell_warr_weaponmaster", 12700u, 12284u, 12281u);
    RegisterSpellScript(classic_spell_weapon_extra_attack_chance);
    RegisterSpellScript(classic_spell_warr_meathook);
    RegisterSpellScript(classic_spell_warr_execute);
    RegisterSpellScript(classic_spell_warr_overpower_probe);
    RegisterSpellScript(classic_spell_warr_execute_damage_probe);
    RegisterSpellScript(classic_spell_warr_spearing_strike);
    RegisterSpellScript(classic_spell_warr_improved_whirlwind_closest);

    // rogue
    RegisterSpellScriptWithArgs(classic_spell_weapon_type_talent, "classic_spell_rog_hack_and_slash", 13706u, 13709u, 1290312u);
    RegisterSpellScript(classic_spell_rog_setup);
    RegisterSpellScript(classic_spell_rog_seal_fate);
    RegisterSpellScript(classic_spell_rog_cutthroat);
    RegisterSpellScript(classic_spell_rog_offhand_strike);
    RegisterSpellScript(classic_spell_rog_offhand_poison);
    RegisterSpellAndAuraScriptPair(classic_spell_rog_redirect, classic_spell_rog_redirect_aura);
    RegisterSpellScript(classic_spell_rog_mutilate_strike);
    RegisterSpellScript(classic_spell_rog_ghostly_strike);

    // hunter
    RegisterSpellScript(classic_spell_hun_entrapment_trap);
    RegisterSpellScript(classic_spell_hun_entrapment_frost_trap);
    RegisterSpellScript(classic_spell_hun_spirit_bond);
    RegisterSpellScript(classic_spell_hun_rapid_recuperation);
    RegisterSpellScript(classic_spell_hun_rapid_killing_consumed);
    RegisterSpellScript(classic_spell_hun_lacerating_strikes);
    RegisterSpellAndAuraScriptPair(classic_spell_hun_kill_command, classic_spell_hun_kill_command_aura);
    RegisterSpellScript(classic_spell_hun_kill_command_pet);
    RegisterSpellScript(classic_spell_hun_readiness);
    RegisterSpellScript(classic_spell_hun_refocus);
    RegisterSpellScript(classic_spell_hun_summon_hawk);
    new classic_unit_hun_rapid_killing();
    RegisterSpellScript(classic_spell_hun_expose_prey);
    RegisterSpellScript(classic_spell_hun_mongoose_bite);
    RegisterSpellScript(classic_spell_hun_mend_pet);
    RegisterSpellScript(classic_spell_rog_main_gauche);
    RegisterSpellScript(classic_spell_rog_improved_expose_armor);
    RegisterSpellScript(classic_spell_hun_lone_wolf);
    RegisterSpellScript(classic_spell_talent_ability_proc_chance);

    // Focused Fire, Quietus
    new classic_unit_talent_damage_mods();
    new classic_warrior_talent_data_log();
    new classic_talent_coverage_log();
}
