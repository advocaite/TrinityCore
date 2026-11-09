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

// Classic 1.60 (WoW Forever) rogue abilities whose client spell does nothing on its own.

#include "ScriptMgr.h"
#include <algorithm>
#include "Player.h"
#include "Random.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellHistory.h"
#include "SpellScript.h"
#include "Unit.h"

enum ClassicRogueAbilities
{
    // The client has no Blade Flurry strike spell (retail 22482). Sweeping Strikes 12723 is the physical school damage spell with
    // base points 1 and no bonus coefficient that the warrior copy-hit also uses; the damage comes from base point 0.
    SPELL_ROGUE_BLADE_FLURRY_STRIKE = 12723,

    SPELL_ROGUE_RUTHLESSNESS_COMBO_POINT = 14157,
    SPELL_ROGUE_REVEALED_FLAW_CRIT       = 28815
};

// 13877 - Blade Flurry: "Increases your attack speed by 20%. In addition, your attacks strike an additional nearby opponent. Lasts 15 sec."
// The client spell is only the haste aura (aura 319). The retail script bound to this id never ran: its strike spell 22482 is not in the
// client data and it hooks the retail aura type of effect 0. The extra strike repeats the damage of each melee swing or ability on a
// second enemy near the target (spell_proc: melee swing and melee ability, hit phase).
class classic_spell_rog_blade_flurry : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_BLADE_FLURRY_STRIKE });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage())
            return false;

        // the extra strike is itself a melee hit
        if (SpellInfo const* spellInfo = damageInfo->GetSpellInfo())
            if (spellInfo->Id == SPELL_ROGUE_BLADE_FLURRY_STRIKE)
                return false;

        _procTarget = GetTarget()->SelectNearbyTarget(eventInfo.GetActionTarget());
        return _procTarget != nullptr;
    }

    void HandleProc(ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !_procTarget)
            return;

        CastSpellExtraArgs args(GetEffect(EFFECT_0));
        args.AddSpellBP0(SpellEffectValue(damageInfo->GetDamage()));
        GetTarget()->CastSpell(_procTarget, SPELL_ROGUE_BLADE_FLURRY_STRIKE, args);
        _procTarget = nullptr;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_rog_blade_flurry::CheckProc);
        OnProc += AuraProcFn(classic_spell_rog_blade_flurry::HandleProc);
    }

    Unit* _procTarget = nullptr;
};

// 14185 - Preparation: "When activated, this ability immediately finishes the cooldown on your Evasion, Sprint, Vanish, Cold Blood and
// Premeditation abilities." The client spell is a dummy effect on the rogue; every rank of those abilities is reset.
class classic_spell_rog_preparation : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* rogue = GetCaster();
        for (uint32 spellId : { 5277u, 2983u, 8696u, 11305u, 1856u, 1857u, 14177u, 14183u })
            if (rogue->GetSpellHistory()->HasCooldown(spellId))
                rogue->GetSpellHistory()->ResetCooldown(spellId, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_rog_preparation::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 14179 - Relentless Strikes: "Your finishing moves have a 20% chance per combo point to restore 25 energy." The client aura only names the
// dummy 14181 (25 energy in its base points); the chance is the vanilla 20% per combo point spent (not in the client data).
class classic_spell_rog_relentless_strikes : public AuraScript
{
    static constexpr uint32 SPELL_ROGUE_RELENTLESS_STRIKES_EFFECT = 14181;
    static constexpr int32 CHANCE_PER_COMBO_POINT = 20;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_RELENTLESS_STRIKES_EFFECT });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Spell const* procSpell = eventInfo.GetProcSpell();
        if (!procSpell)
            return false;

        int32 points = procSpell->GetPowerTypeCostAmount(POWER_COMBO_POINTS).value_or(0);
        return points > 0 && roll_chance(points * CHANCE_PER_COMBO_POINT);
    }

    void HandleProc(ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();

        SpellInfo const* effect = sSpellMgr->GetSpellInfo(SPELL_ROGUE_RELENTLESS_STRIKES_EFFECT, DIFFICULTY_NONE);
        if (effect && !effect->GetEffects().empty())
        {
            Unit* rogue = GetTarget();
            rogue->EnergizeBySpell(rogue, effect, effect->GetEffect(EFFECT_0).CalcValueAsInt(rogue), POWER_ENERGY);
        }
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_rog_relentless_strikes::CheckProc);
        OnProc += AuraProcFn(classic_spell_rog_relentless_strikes::HandleProc);
    }
};

// 14156 - Ruthlessness: "Gives you a $m1% chance to add a combo point to your target after performing a finishing move." The client aura is a
// proc trigger spell with a proc chance of 100 and the talent rank in its amount, so it fired on every melee ability (builders included) or
// never; 14157 adds the point. Like vMaNGOS, only a finisher that hit gives the chance. The point comes in the finish phase: the finisher
// spends its combo points just before (Spell::_handle_finish_phase), a point added in the hit phase would be spent with them.
class classic_spell_rog_ruthlessness : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_RUTHLESSNESS_COMBO_POINT });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Spell const* procSpell = eventInfo.GetProcSpell();
        AuraEffect const* talent = GetEffect(EFFECT_0);
        if (!procSpell || !talent || procSpell->GetPowerTypeCostAmount(POWER_COMBO_POINTS).value_or(0) <= 0)
            return false;

        constexpr uint32 missed = PROC_HIT_MISS | PROC_HIT_FULL_RESIST | PROC_HIT_DODGE | PROC_HIT_PARRY | PROC_HIT_EVADE | PROC_HIT_IMMUNE | PROC_HIT_DEFLECT | PROC_HIT_REFLECT;
        if (eventInfo.GetHitMask() & missed)
            return false;

        Unit* target = procSpell->m_targets.GetUnitTarget();
        if (!target || target == GetTarget() || !target->IsAlive())
            return false;

        return roll_chance(talent->GetAmountAsInt());
    }

    void HandleProc(ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        if (Unit* target = eventInfo.GetProcSpell()->m_targets.GetUnitTarget())
            GetTarget()->CastSpell(target, SPELL_ROGUE_RUTHLESSNESS_COMBO_POINT, CastSpellExtraArgs(GetEffect(EFFECT_0)));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_rog_ruthlessness::CheckProc);
        OnProc += AuraProcFn(classic_spell_rog_ruthlessness::HandleProc);
    }
};

// 28814 - Revealed Flaw (Bonescythe Armor, 8 pieces): "Your Eviscerate has a chance per combo point to reveal a flaw in your opponent's armor, granting a 100%
// critical hit chance for your next Backstab, Sinister Strike, or Hemorrhage." The client aura is an add-target-trigger effect without a class mask or a
// chance, so every spell the rogue cast put 28815 (+100 crit, one charge) back on it and Sinister Strike and Backstab crit every time (damage_parity 149).
// SpellMgr turns the effect into a proc trigger; this rolls 20 % per combo point the Eviscerate spent, and 28815 spends its charge on the next ability its
// spell mods name.
class classic_spell_rog_revealed_flaw : public AuraScript
{
    static constexpr int32 CHANCE_PER_COMBO_POINT = 20;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROGUE_REVEALED_FLAW_CRIT });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Spell const* procSpell = eventInfo.GetProcSpell();
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        if (!procSpell || !spellInfo || spellInfo->SpellFamilyName != SPELLFAMILY_ROGUE)
            return false;

        switch (spellInfo->Id)
        {
            case 2098: case 6760: case 6761: case 6762: case 8623: case 8624: case 11299: case 11300: case 31016:
                break;
            default:
                return false;
        }

        constexpr uint32 missed = PROC_HIT_MISS | PROC_HIT_FULL_RESIST | PROC_HIT_DODGE | PROC_HIT_PARRY | PROC_HIT_EVADE | PROC_HIT_IMMUNE | PROC_HIT_DEFLECT | PROC_HIT_REFLECT;
        if (eventInfo.GetHitMask() & missed)
            return false;

        return roll_chance(CHANCE_PER_COMBO_POINT * procSpell->GetPowerTypeCostAmount(POWER_COMBO_POINTS).value_or(0));
    }

    void HandleProc(ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(GetTarget(), SPELL_ROGUE_REVEALED_FLAW_CRIT, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_rog_revealed_flaw::CheckProc);
        OnProc += AuraProcFn(classic_spell_rog_revealed_flaw::HandleProc);
    }
};

// 13976 - Initiative: "Gives you an $m1% chance to add an additional combo point to your target after using your Ambush, Garrote, or Cheap Shot
// ability." 13754 - Improved Kick: "Gives your Kick ability a $m1% chance to silence the target for 2 sec." Same data shape as Ruthlessness: the
// proc chance of the client aura is 100 and the talent rank is its amount, so the chance is rolled here against the abilities of the rogue
// family mask (Ambush 0x200, Garrote 0x100, Cheap Shot 0x400, Kick 0x10); the client trigger spell (13977 combo point, 18425 silence) is the
// default proc action.
class classic_spell_rog_ability_chance : public AuraScript
{
public:
    explicit classic_spell_rog_ability_chance(uint32 familyMask) : _familyMask(familyMask) { }

private:
    bool CheckProc(ProcEventInfo& eventInfo)
    {
        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        AuraEffect const* talent = GetEffect(EFFECT_0);
        Unit* target = eventInfo.GetActionTarget();
        if (!spellInfo || !talent || !target || target == GetTarget() || !target->IsAlive())
            return false;

        if (spellInfo->SpellFamilyName != SPELLFAMILY_ROGUE || !(spellInfo->SpellFamilyFlags[0] & _familyMask))
            return false;

        return roll_chance(talent->GetAmountAsInt());
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_rog_ability_chance::CheckProc);
    }

    uint32 _familyMask;
};

// 1943, 8639, 8640, 11273, 11274, 11275 - Rupture, 703, 8631, 8632, 8633, 11289, 11290 - Garrote: patch 1.12 made both bleeds grow with attack power.
// Ported from vMaNGOS (GPL-2.0, game/Spells/SpellAuras.cpp, Aura::CalculateDotDamage, SPELLFAMILY_ROGUE case): every tick of Rupture gets
// attack power * min(combo points, 3) / 100 on top of the spell's own amount (like Rip, only the first three points count), every tick of
// Garrote gets attack power * 3 / 100. The amount is fixed when the bleed is applied, with the combo points the finisher is spending.
class classic_spell_rog_bleed_attack_power : public AuraScript
{
    static constexpr float RUPTURE_AP_PER_COMBO_POINT = 0.01f;      // vMaNGOS: cp / 100 of attack power, up to RUPTURE_AP_MAX_COMBO_POINTS
    static constexpr int32 RUPTURE_AP_MAX_COMBO_POINTS = 3;
    static constexpr float GARROTE_AP_COEFFICIENT = 0.03f;          // vMaNGOS: 3% of attack power per tick

    void CalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& canBeRecalculated)
    {
        Unit* rogue = GetCaster();
        if (!rogue)
            return;

        canBeRecalculated = false;
        float attackPower = rogue->GetTotalAttackPowerValue(BASE_ATTACK);
        if (GetSpellInfo()->SpellFamilyFlags[0] & ROGUE_GARROTE_FAMILY_FLAG)
            amount += attackPower * GARROTE_AP_COEFFICIENT;
        else
        {
            int32 points = std::min<int32>(rogue->GetPower(POWER_COMBO_POINTS), RUPTURE_AP_MAX_COMBO_POINTS);
            amount += attackPower * RUPTURE_AP_PER_COMBO_POINT * points;
        }
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(classic_spell_rog_bleed_attack_power::CalcAmount, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }

    static constexpr uint32 ROGUE_GARROTE_FAMILY_FLAG = 0x00000100;
};

// 2098, 6760, 6761, 6762, 8623, 8624, 11299, 11300, 31016 - Eviscerate: patch 1.12 made it grow with attack power. Ported from vMaNGOS
// (GPL-2.0, scripts/spells/spell_rogue.cpp, RogueEviscerateScript): attack power * combo points * 3 / 100 is added to the damage.
class classic_spell_rog_eviscerate : public SpellScript
{
    static constexpr float EVISCERATE_AP_PER_COMBO_POINT = 0.03f;

    void CalculateDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* /*victim*/, int32& /*damage*/, int32& flatMod, float& /*pctMod*/) const
    {
        Unit* rogue = GetCaster();
        flatMod += int32(rogue->GetTotalAttackPowerValue(BASE_ATTACK) * rogue->GetPower(POWER_COMBO_POINTS) * EVISCERATE_AP_PER_COMBO_POINT);
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_rog_eviscerate::CalculateDamage);
    }
};

// 1856, 1857, 27617 - Vanish: ported from vMaNGOS (GPL-2.0, scripts/spells/spell_rogue.cpp, RogueVanishScript). Vanish also ends effects that let
// an enemy always know where the rogue is (Hunter's Mark, Mind Vision; patch 1.12) and puts the rogue straight into the highest Stealth rank
// known, so the stealth stays after the Vanish aura (10 s in the client data) runs out.
class classic_spell_rog_vanish : public SpellScript
{
    void HandleAfterCast()
    {
        Unit* rogue = GetCaster();
        rogue->RemoveAurasByType(SPELL_AURA_MOD_STALKED);

        Player* player = rogue->ToPlayer();
        if (!player)
            return;

        for (uint32 stealth : { 1787u, 1786u, 1785u, 1784u })
            if (player->HasSpell(stealth))
            {
                player->CastSpell(player, stealth, TRIGGERED_FULL_MASK);
                break;
            }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(classic_spell_rog_vanish::HandleAfterCast);
    }
};

void AddSC_classic_rogue_abilities()
{
    RegisterSpellScript(classic_spell_rog_blade_flurry);
    RegisterSpellScript(classic_spell_rog_preparation);
    RegisterSpellScript(classic_spell_rog_eviscerate);
    RegisterSpellScript(classic_spell_rog_vanish);
    RegisterSpellScript(classic_spell_rog_bleed_attack_power);
    RegisterSpellScript(classic_spell_rog_relentless_strikes);
    RegisterSpellScript(classic_spell_rog_ruthlessness);
    RegisterSpellScript(classic_spell_rog_revealed_flaw);
    RegisterSpellScriptWithArgs(classic_spell_rog_ability_chance, "classic_spell_rog_initiative", 0x00000700u);
    RegisterSpellScriptWithArgs(classic_spell_rog_ability_chance, "classic_spell_rog_improved_kick", 0x00000010u);
}
