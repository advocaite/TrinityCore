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

// Classic 1.60 (WoW Forever) priest and paladin talents. In the client data these talents are dummy auras (or dummy spells) with
// no server side behaviour; the scripts below implement the Forever tooltips with the helper spells found in the 1.60 data.
// Talent rank values come from the trait curves, so every amount is read from the talent aura effect, never hardcoded.

#include "ScriptMgr.h"
#include "GameTime.h"
#include "Player.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "Unit.h"

namespace
{
// Base mana cost of a spell (flat cost plus the percentage of base mana), before cost modifiers
int32 GetClassicBaseManaCost(SpellInfo const* spellInfo, Unit const* caster)
{
    for (SpellPowerEntry const* power : spellInfo->PowerCosts)
    {
        if (!power || power->PowerType != POWER_MANA)
            continue;

        int32 cost = power->ManaCost;
        if (power->PowerCostPct > 0.0f)
            cost += int32(CalculatePct(float(caster->GetCreateMana()), power->PowerCostPct));
        return cost;
    }
    return 0;
}
}

/*######
## Priest
######*/

enum ClassicPriestTalents
{
    SPELL_PRI_INSPIRATION_BUFF          = 14893,
    SPELL_PRI_BLESSED_RECOVERY_HOT      = 27813,
    SPELL_PRI_BLESSED_RECOVERY_HEAL     = 1240755,
    SPELL_PRI_PRAYER_OF_MENDING_BOUNCE  = 401882,
    SPELL_PRI_RENEWED_HOPE              = 425280,
    SPELL_PRI_WEAKENED_SOUL             = 6788,
    SPELL_PRI_DIVINE_AEGIS_ABSORB       = 431624,
    SPELL_PRI_POWER_IN_LIGHT            = 1309969,
    SPELL_PRI_EARLY_DEMISE              = 1310076,
    SPELL_PRI_LITANY_OF_LIGHT_ENERGIZE  = 1317007
};

// 14892 - Inspiration: "Your non-periodic critical heals increase your target's Armor by X% for 15 sec." Critical direct heals cast
// 14893 (armor percent) on the healed target with the talent amount. Crit only + triggered heals (Penance, Holy Shock) via spell_proc.
class classic_spell_pri_inspiration : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRI_INSPIRATION_BUFF });
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
        GetTarget()->CastSpell(eventInfo.GetActionTarget(), SPELL_PRI_INSPIRATION_BUFF, args);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pri_inspiration::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pri_inspiration::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 15326 - Blackout, 15257 - Shadow Weaving, 14531 - Martyrdom, 15270 - Spirit Tap (talents): "a $m1% chance to ..." The chance is effect
// 0's rank value. Blackout and Shadow Weaving can proc from procs and had no restriction, so the core made no proc data for them and they
// never procced; Martyrdom and Spirit Tap procced at the client data's 100%. The spell_proc rows give each one what its tooltip names
// (Shadow damage spells, critical strikes taken, kills); the script rolls the amount.
class classic_spell_pri_talent_chance : public AuraScript
{
    bool CheckProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        return roll_chance(aurEff->GetAmount());
    }

    void Register() override
    {
        DoCheckEffectProc += AuraCheckEffectProcFn(classic_spell_pri_talent_chance::CheckProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// 27811 - Blessed Recovery: "After being struck by a melee or ranged critical hit, or suffering more than 30% of your maximum Health from a
// single attack, heal 8% of the damage taken over 6 sec. Refreshing this effect carries over any remaining healing." Effect 0 is the heal
// percentage, effect 1 the health threshold. The heal is 27813 (1.60: periodic dummy, 3 ticks) whose amount holds the total; the remaining
// part of a running one is added on refresh.
class classic_spell_pri_blessed_recovery : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_PRI_BLESSED_RECOVERY_HOT }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        if (!damageInfo || !damageInfo->GetDamage() || !GetTarget()->IsAlive())
            return false;

        if (eventInfo.GetHitMask() & PROC_HIT_CRITICAL)
            return true;

        AuraEffect const* threshold = GetEffect(EFFECT_1);
        return threshold && damageInfo->GetDamage() > GetTarget()->CountPctFromMaxHealth(float(threshold->GetAmount()));
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        Unit* target = GetTarget();
        int32 heal = CalculatePct(int32(eventInfo.GetDamageInfo()->GetDamage()), aurEff->GetAmount());

        if (Aura* running = target->GetAura(SPELL_PRI_BLESSED_RECOVERY_HOT, target->GetGUID()))
        {
            if (AuraEffect const* runningEff = running->GetEffect(EFFECT_0))
                if (uint32 totalTicks = runningEff->GetTotalTicks())
                    heal += int32(int64(runningEff->GetAmountAsInt()) * runningEff->GetRemainingTicks() / totalTicks);
            running->Remove();
        }

        if (heal > 0)
            target->CastSpell(target, SPELL_PRI_BLESSED_RECOVERY_HOT, CastSpellExtraArgs(aurEff).AddSpellBP0(heal));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pri_blessed_recovery::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pri_blessed_recovery::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 27813 - Blessed Recovery (periodic dummy, 2 sec, 6 sec): each tick heals an equal share of the total (1240755)
class classic_spell_pri_blessed_recovery_hot : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRI_BLESSED_RECOVERY_HEAL });
    }

    void HandleTick(AuraEffect const* aurEff)
    {
        uint32 totalTicks = aurEff->GetTotalTicks();
        if (!totalTicks)
            return;

        int32 heal = aurEff->GetAmountAsInt() / int32(totalTicks);
        if (heal > 0)
            GetTarget()->CastSpell(GetTarget(), SPELL_PRI_BLESSED_RECOVERY_HEAL, CastSpellExtraArgs(aurEff).AddSpellBP0(heal));
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_pri_blessed_recovery_hot::HandleTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 401859, 1240826, 1240827 - Prayer of Mending (ranks 1-3): "Places a spell on the target that heals them the next time they take damage
// or receive non-periodic healing. When the heal occurs, Prayer of Mending jumps to a party or raid member within 20 yards. Jumps up to
// 5 times and lasts 30 sec after each jump." Cast: effect 0 holds the heal (base + 42.9% healing power), effect 1 the charges.
struct ClassicPrayerOfMendingRank
{
    uint32 Cast;
    uint32 Aura;    // 30 sec, dummy, stacks = charges, limited to one target per caster
    uint32 Heal;
    uint32 Jump;    // dummy, ally area 20 yd
};

static ClassicPrayerOfMendingRank const ClassicPrayerOfMendingRanks[] =
{
    { 401859,  401877,  401863,  401880  },
    { 1240826, 1240848, 1313379, 1240918 },
    { 1240827, 1240849, 1313381, 1240919 },
};

static ClassicPrayerOfMendingRank const* GetClassicPrayerOfMendingRank(uint32 spellId)
{
    for (ClassicPrayerOfMendingRank const& rank : ClassicPrayerOfMendingRanks)
        if (rank.Cast == spellId || rank.Aura == spellId || rank.Heal == spellId || rank.Jump == spellId)
            return &rank;
    return nullptr;
}

struct ClassicPrayerOfMendingJump
{
    int32 Heal;
    int32 Charges;
};

static void CastClassicPrayerOfMendingAura(Unit* caster, Unit* target, uint32 auraId, int32 heal, int32 charges)
{
    CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
    args.AddSpellBP0(heal);
    args.AddSpellMod(SPELLVALUE_AURA_STACK, std::max(charges, 1));
    caster->CastSpell(target, auraId, args);
}

class classic_spell_pri_prayer_of_mending : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        ClassicPrayerOfMendingRank const* rank = GetClassicPrayerOfMendingRank(spellInfo->Id);
        return rank && ValidateSpellInfo({ rank->Aura, rank->Heal, rank->Jump }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        SpellEffectInfo const& healEffect = GetEffectInfo();
        int32 heal = healEffect.CalcValueAsInt(caster)
            + int32(caster->SpellBaseHealingBonusDone(GetSpellInfo()->GetSchoolMask()) * healEffect.BonusCoefficient);
        int32 charges = GetSpellInfo()->GetEffect(EFFECT_1).CalcValueAsInt(caster);

        CastClassicPrayerOfMendingAura(caster, target, GetClassicPrayerOfMendingRank(GetSpellInfo()->Id)->Aura, heal, charges);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_pri_prayer_of_mending::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 401877, 1240848, 1240849 - Prayer of Mending (aura): damage taken or a direct heal received heals the holder with the stored amount,
// then the holder casts the jump spell carrying the heal and the remaining charges
class classic_spell_pri_prayer_of_mending_aura : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        ClassicPrayerOfMendingRank const* rank = GetClassicPrayerOfMendingRank(spellInfo->Id);
        return rank && ValidateSpellInfo({ rank->Heal, rank->Jump });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (!GetTarget()->IsAlive())
            return false;
        if (DamageInfo* damageInfo = eventInfo.GetDamageInfo())
            return damageInfo->GetDamage() > 0;
        if (HealInfo* healInfo = eventInfo.GetHealInfo())
            return healInfo->GetHeal() > 0;
        return false;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        ClassicPrayerOfMendingRank const* rank = GetClassicPrayerOfMendingRank(GetId());
        Unit* holder = GetTarget();
        Unit* caster = GetCaster();
        ObjectGuid const casterGuid = GetCasterGUID();
        int32 const heal = aurEff->GetAmountAsInt();
        int32 const charges = GetStackAmount();

        // removed first: the heal below must not consume a charge of a new application
        Remove();

        if (!caster)
            return;

        caster->CastSpell(holder, rank->Heal, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(heal));

        if (charges > 1)
            holder->CastSpell(holder, rank->Jump, CastSpellExtraArgs(TRIGGERED_FULL_MASK)
                .SetOriginalCaster(casterGuid)
                .SetCustomArg(ClassicPrayerOfMendingJump{ heal, charges - 1 }));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pri_prayer_of_mending_aura::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pri_prayer_of_mending_aura::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 401880, 1240918, 1240919 - Prayer of Mending (jump): picks one injured party or raid member of the holder within 20 yards (not the
// holder) and places Prayer of Mending on it for the original priest with the remaining charges
class classic_spell_pri_prayer_of_mending_jump : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        ClassicPrayerOfMendingRank const* rank = GetClassicPrayerOfMendingRank(spellInfo->Id);
        return rank && ValidateSpellInfo({ rank->Aura, SPELL_PRI_PRAYER_OF_MENDING_BOUNCE });
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Unit* holder = GetCaster();
        targets.remove_if([holder](WorldObject* object)
        {
            Unit* unit = object->ToUnit();
            return !unit || unit == holder || !unit->IsAlive() || !holder->IsInRaidWith(unit);
        });
        Trinity::SelectRandomInjuredTargets(targets, 1, true);
    }

    void HandleJump(SpellEffIndex /*effIndex*/)
    {
        Unit* priest = GetOriginalCaster();
        Unit* target = GetHitUnit();
        ClassicPrayerOfMendingJump const* jump = std::any_cast<ClassicPrayerOfMendingJump>(&GetSpell()->m_customArg);
        if (!priest || !target || !jump)
            return;

        CastClassicPrayerOfMendingAura(priest, target, GetClassicPrayerOfMendingRank(GetSpellInfo()->Id)->Aura, jump->Heal, jump->Charges);
        GetCaster()->CastSpell(target, SPELL_PRI_PRAYER_OF_MENDING_BOUNCE, TRIGGERED_FULL_MASK);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(classic_spell_pri_prayer_of_mending_jump::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ALLY);
        OnEffectHitTarget += SpellEffectFn(classic_spell_pri_prayer_of_mending_jump::HandleJump, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 402174, 1240720, 1240721, 1316995 - Penance (ranks 1-4): "Launches a volley of holy light at the target, causing Holy damage to an enemy,
// or healing to an ally, instantly and every 1 sec for 2 sec." The dummy starts the rank's 2 sec channel (enemy or ally version); the
// channel aura triggers the damage or heal spell at once and every second (1.60 data).
class classic_spell_pri_penance : public SpellScript
{
    static std::pair<uint32, uint32> GetChannelSpells(uint32 spellId) // damage, heal
    {
        switch (spellId)
        {
            case 402174:  return { 402261,  402277  };
            case 1240720: return { 1240734, 1240732 };
            case 1240721: return { 1240736, 1240733 };
            case 1316995: return { 1316994, 1316992 };
            default:      return { 0, 0 };
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        auto [damage, heal] = GetChannelSpells(spellInfo->Id);
        return ValidateSpellInfo({ damage, heal });
    }

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (!target)
            return SPELL_FAILED_BAD_TARGETS;

        if (!caster->IsFriendlyTo(target))
        {
            if (!caster->IsValidAttackTarget(target))
                return SPELL_FAILED_BAD_TARGETS;
            if (!caster->isInFront(target))
                return SPELL_FAILED_UNIT_NOT_INFRONT;
        }
        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        auto [damage, heal] = GetChannelSpells(GetSpellInfo()->Id);
        caster->CastSpell(target, caster->IsFriendlyTo(target) ? heal : damage,
            CastSpellExtraArgs(TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD).SetTriggeringSpell(GetSpell()));
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_pri_penance::CheckCast);
        OnEffectHitTarget += SpellEffectFn(classic_spell_pri_penance::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 425280 - Renewed Hope: "Your heals from Flash Heal, Binding Heal, Lesser Heal, Heal, Greater Heal, and Penance gain 10% increased critical
// strike chance when cast on targets with Weakened Soul, and reduce the remaining duration of Weakened Soul on their target by 5 sec."
// The talent aura (proc limited to those heals by spell_proc) shortens Weakened Soul by effect 1 (-5000 ms) ...
class classic_spell_pri_renewed_hope : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_PRI_WEAKENED_SOUL }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    void HandleProc(AuraEffect const* /*aurEff*/, ProcEventInfo& eventInfo)
    {
        Unit* target = eventInfo.GetActionTarget();
        AuraEffect const* reduction = GetEffect(EFFECT_1);
        if (!target || !reduction)
            return;

        if (Aura* weakenedSoul = target->GetAura(SPELL_PRI_WEAKENED_SOUL))
        {
            int32 duration = weakenedSoul->GetDuration() + reduction->GetAmountAsInt();
            if (duration <= 0)
                weakenedSoul->Remove();
            else
                weakenedSoul->SetDuration(duration);
        }
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_pri_renewed_hope::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// ... and the heals themselves get the crit bonus (effect 0) on targets with Weakened Soul: Flash Heal, Binding Heal, Lesser Heal, Heal,
// Greater Heal (all ranks) and the Penance heal spells
class classic_spell_pri_renewed_hope_crit : public SpellScript
{
    void CalcCritChance(Unit const* victim, float& critChance)
    {
        if (!victim || !victim->HasAura(SPELL_PRI_WEAKENED_SOUL))
            return;

        if (AuraEffect const* renewedHope = GetCaster()->GetAuraEffect(SPELL_PRI_RENEWED_HOPE, EFFECT_0))
            critChance += float(renewedHope->GetAmount());
    }

    void Register() override
    {
        OnCalcCritChance += SpellOnCalcCritChanceFn(classic_spell_pri_renewed_hope_crit::CalcCritChance);
    }
};

// 431622 - Divine Aegis: "Your critical heals create a protective shield on the target, absorbing 15% of the amount healed. Lasts 12 sec."
// (431624, crit only + triggered heals via spell_proc)
class classic_spell_pri_divine_aegis : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRI_DIVINE_AEGIS_ABSORB });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        HealInfo* healInfo = eventInfo.GetHealInfo();
        return healInfo && healInfo->GetHeal() && eventInfo.GetActionTarget() && (eventInfo.GetHitMask() & PROC_HIT_CRITICAL);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        int32 absorb = CalculatePct(int32(eventInfo.GetHealInfo()->GetHeal()), aurEff->GetAmount());
        if (absorb > 0)
            GetTarget()->CastSpell(eventInfo.GetActionTarget(), SPELL_PRI_DIVINE_AEGIS_ABSORB, CastSpellExtraArgs(aurEff).AddSpellBP0(absorb));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pri_divine_aegis::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pri_divine_aegis::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 1309969 - Power in Light: "Your Smite and Penance spells deal 15% increased damage to targets afflicted with your Holy Fire." Bound to
// Smite (all ranks) and the Penance damage spells.
class classic_spell_pri_power_in_light : public SpellScript
{
    void HandleCalcDamage(SpellEffectInfo const& /*spellEffectInfo*/, Unit* victim, int32& /*damage*/, int32& /*flatMod*/, float& pctMod)
    {
        Unit* caster = GetCaster();
        AuraEffect const* talent = caster->GetAuraEffect(SPELL_PRI_POWER_IN_LIGHT, EFFECT_0);
        if (!talent || !victim)
            return;

        // Holy Fire (all ranks): family flag 0x100000
        if (victim->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_PRIEST, flag128(0x00100000, 0, 0, 0), caster->GetGUID()))
            AddPct(pctMod, talent->GetAmount());
    }

    void Register() override
    {
        CalcDamage += SpellCalcDamageFn(classic_spell_pri_power_in_light::HandleCalcDamage);
    }
};

// 1310076 - Early Demise: "Increases Shadow Word: Death's critical strike chance on targets at or below 20% health by 30%." Effect 0 is the
// crit bonus, effect 1 the health threshold. Bound to the Shadow Word: Death ranks.
class classic_spell_pri_early_demise : public SpellScript
{
    void CalcCritChance(Unit const* victim, float& critChance)
    {
        Unit* caster = GetCaster();
        AuraEffect const* crit = caster->GetAuraEffect(SPELL_PRI_EARLY_DEMISE, EFFECT_0);
        AuraEffect const* threshold = caster->GetAuraEffect(SPELL_PRI_EARLY_DEMISE, EFFECT_1);
        if (!victim || !crit || !threshold)
            return;

        if (victim->GetHealthPct() <= float(threshold->GetAmount()))
            critChance += float(crit->GetAmount());
    }

    void Register() override
    {
        OnCalcCritChance += SpellOnCalcCritChanceFn(classic_spell_pri_early_demise::CalcCritChance);
    }
};

// 1317006 - Litany of Light: "When you cast a healing spell, gain Mana equal to 20% of the base cost of the spell if your previous heal was a
// different spell." The talent aura (proc trigger spell without a spell) procs at cast (spell_proc); the energize is 1317007. Ranks of the
// same spell count as the same spell (compared by name).
class classic_spell_pri_litany_of_light : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PRI_LITANY_OF_LIGHT_ENERGIZE });
    }

    static bool IsPenance(uint32 spellId)
    {
        return spellId == 402174 || spellId == 1240720 || spellId == 1240721 || spellId == 1316995;
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Spell const* spell = eventInfo.GetProcSpell();
        if (!spell || spell->IsTriggered())
            return false;

        SpellInfo const* spellInfo = spell->GetSpellInfo();
        if (spellInfo->HasEffect(SPELL_EFFECT_HEAL) || spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL))
            return true;

        // dummy cast spells: Prayer of Mending, and Penance cast on an ally
        if (GetClassicPrayerOfMendingRank(spellInfo->Id))
            return true;
        if (IsPenance(spellInfo->Id))
            if (Unit const* target = spell->m_targets.GetUnitTarget())
                return GetTarget()->IsFriendlyTo(target);
        return false;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        SpellInfo const* spellInfo = eventInfo.GetProcSpell()->GetSpellInfo();
        std::string name = (*spellInfo->SpellName)[DEFAULT_LOCALE];
        bool const differentSpell = !_lastHeal.empty() && _lastHeal != name;
        _lastHeal = std::move(name);
        if (!differentSpell)
            return;

        Unit* priest = GetTarget();
        int32 mana = CalculatePct(GetClassicBaseManaCost(spellInfo, priest), aurEff->GetAmount());
        if (mana > 0)
            priest->CastSpell(priest, SPELL_PRI_LITANY_OF_LIGHT_ENERGIZE, CastSpellExtraArgs(aurEff).AddSpellBP0(mana));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pri_litany_of_light::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pri_litany_of_light::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }

    std::string _lastHeal;
};

// 1277462, 1277634, 1277638, 1277639, 1277640 - Contingency Plan (ranks 1-5): "Place a Holy ward on an ally for 30 sec. The next time this
// ally takes damage dropping their Health below 35%, they will gain a shield absorbing X damage and begin healing for Y Health over 15 sec."
// Effect 0 is the health threshold; the rank's shield and heal over time are cast by the priest, then the ward is gone.
class classic_spell_pri_contingency_plan : public AuraScript
{
    static std::pair<uint32, uint32> GetRankSpells(uint32 spellId) // shield, heal over time
    {
        switch (spellId)
        {
            case 1277462: return { 1277463, 1277456 };
            case 1277634: return { 1277464, 1277457 };
            case 1277638: return { 1277465, 1277458 };
            case 1277639: return { 1277467, 1277459 };
            case 1277640: return { 1277466, 1277460 };
            default:      return { 0, 0 };
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        auto [shield, hot] = GetRankSpells(spellInfo->Id);
        return ValidateSpellInfo({ shield, hot });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        Unit* target = GetTarget();
        return damageInfo && damageInfo->GetDamage() && target->IsAlive() && target->HealthBelowPct(float(GetEffect(EFFECT_0)->GetAmount()));
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        Unit* target = GetTarget();
        Unit* caster = GetCaster();
        Unit* source = caster ? caster : target;
        auto [shield, hot] = GetRankSpells(GetId());

        CastSpellExtraArgs args(aurEff);
        args.SetOriginalCaster(GetCasterGUID());
        source->CastSpell(target, shield, args);
        source->CastSpell(target, hot, args);
        Remove();
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pri_contingency_plan::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pri_contingency_plan::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

/*######
## Paladin
######*/

enum ClassicPaladinTalents
{
    SPELL_PAL_EYE_FOR_AN_EYE_DAMAGE         = 25997,
    SPELL_PAL_REDOUBT_BUFF                  = 20128,
    SPELL_PAL_INFUSION_OF_LIGHT_BUFF        = 437063,
    SPELL_PAL_CONSECRATED_GROUND            = 1310905,
    SPELL_PAL_LIGHTS_VIGIL_ENERGIZE         = 1311601,
    SPELL_PAL_SANCTIFIED_JUDGEMENT          = 1311074,
    SPELL_PAL_SANCTIFIED_JUDGEMENT_ENERGIZE = 1311077,
    SPELL_PAL_IMPROVED_SEAL_OF_FURY         = 1314103,
    SPELL_PAL_IMPROVED_SEAL_OF_FURY_ENERGIZE= 1314104,
    SPELL_PAL_SACRED_SHIELD_ABSORB          = 412018,
    SPELL_PAL_SACRED_SHIELD_FLASH_OF_LIGHT  = 412020
};

// 9799 - Eye for an Eye: "All critical strikes against you cause 15% of the damage taken to the attacker as well. The damage caused by Eye
// for an Eye will not exceed 50% of the Paladin's total health." (25997; crit only + triggered via spell_proc)
class classic_spell_pal_eye_for_an_eye : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PAL_EYE_FOR_AN_EYE_DAMAGE });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo* damageInfo = eventInfo.GetDamageInfo();
        Unit* attacker = eventInfo.GetActor();
        return damageInfo && damageInfo->GetDamage() && (eventInfo.GetHitMask() & PROC_HIT_CRITICAL)
            && attacker && attacker != GetTarget() && attacker->IsAlive();
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        Unit* paladin = GetTarget();
        int32 damage = CalculatePct(int32(eventInfo.GetDamageInfo()->GetDamage()), aurEff->GetAmount());
        damage = std::min<int32>(damage, int32(paladin->CountPctFromMaxHealth(50.0f)));
        if (damage > 0)
            paladin->CastSpell(eventInfo.GetActor(), SPELL_PAL_EYE_FOR_AN_EYE_DAMAGE, CastSpellExtraArgs(aurEff).AddSpellBP0(damage));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pal_eye_for_an_eye::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pal_eye_for_an_eye::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 20127 - Redoubt: "Damaging melee attacks against you have a 10% chance to increase your chance to block by X%. Lasts 10 sec or 5 blocks."
// (20128 with the talent amount; the 10% chance and the melee hits are the talent's own proc data)
class classic_spell_pal_redoubt : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PAL_REDOUBT_BUFF });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        CastSpellExtraArgs args(aurEff);
        if (aurEff->GetAmountAsInt() > 0)
            args.AddSpellBP0(aurEff->GetAmountAsInt());
        GetTarget()->CastSpell(GetTarget(), SPELL_PAL_REDOUBT_BUFF, args);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_pal_redoubt::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 426065 - Infusion of Light: "Your Holy Shock and Flash of Light critical hits reduce the cast time of your next Holy Light cast within
// 15 sec by 1.0 sec." Crits of Holy Shock / Flash of Light (spell_proc family mask) give 437063 (Holy Light cast time, 1 charge) with the
// talent amount.
class classic_spell_pal_infusion_of_light : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PAL_INFUSION_OF_LIGHT_BUFF });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        CastSpellExtraArgs args(aurEff);
        if (aurEff->GetAmountAsInt() != 0)
            args.AddSpellBP0(aurEff->GetAmountAsInt());
        GetTarget()->CastSpell(GetTarget(), SPELL_PAL_INFUSION_OF_LIGHT_BUFF, args);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_pal_infusion_of_light::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 1280353 - Consecration (Consecrated Ground debuff): "Gives your Holy spells 10% increased damage against the first 4 enemies that enter your
// Consecration." The debuff (Holy damage taken from the caster) has no points in the data: it takes the paladin's Consecrated Ground
// (1310905) amount, nothing without the talent.
class classic_spell_pal_consecrated_ground : public AuraScript
{
    void CalcAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& /*canBeRecalculated*/)
    {
        amount = 0;
        if (Unit* caster = GetCaster())
            if (AuraEffect const* talent = caster->GetAuraEffect(SPELL_PAL_CONSECRATED_GROUND, EFFECT_0))
                amount = talent->GetAmount();
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(classic_spell_pal_consecrated_ground::CalcAmount, EFFECT_1, SPELL_AURA_MOD_SCHOOL_MASK_DAMAGE_FROM_CASTER);
    }
};

// 1310911, 1311590, 1311595 - Light's Vigil (ranks 1-3): "Applies Light's Vigil to the target for 30 sec. Your next Holy Shock cast on them
// triggers no cooldown and causes enemy targets to suffer X Holy damage and refund 75% of Light's Vigil's Mana cost, or allied targets to
// heal their party for Y. You may only have one Light's Vigil active per party."
struct ClassicLightsVigilRank
{
    uint32 Cast;
    uint32 AllyAura;
    uint32 EnemyAura;
    uint32 PartyHeal;
    uint32 Damage;
};

static ClassicLightsVigilRank const ClassicLightsVigilRanks[] =
{
    { 1310911, 1310909, 1310910, 1310912, 1310914 },
    { 1311590, 1311593, 1311594, 1311591, 1311592 },
    { 1311595, 1311597, 1311599, 1311596, 1311598 },
};

static ClassicLightsVigilRank const* GetClassicLightsVigilRank(uint32 castSpellId)
{
    for (ClassicLightsVigilRank const& rank : ClassicLightsVigilRanks)
        if (rank.Cast == castSpellId)
            return &rank;
    return nullptr;
}

static bool IsClassicLightsVigilAura(uint32 spellId)
{
    for (ClassicLightsVigilRank const& rank : ClassicLightsVigilRanks)
        if (rank.AllyAura == spellId || rank.EnemyAura == spellId)
            return true;
    return false;
}

// The cast puts the ally or enemy version on the target. One Light's Vigil per paladin: the previous one is removed, and the new one is
// registered as the paladin's single target aura (removed with the paladin like the Limit N auras).
class classic_spell_pal_lights_vigil : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        ClassicLightsVigilRank const* rank = GetClassicLightsVigilRank(spellInfo->Id);
        return rank && ValidateSpellInfo({ rank->AllyAura, rank->EnemyAura, rank->PartyHeal, rank->Damage, SPELL_PAL_LIGHTS_VIGIL_ENERGIZE });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        std::vector<Aura*> previous;
        for (Aura* aura : caster->GetSingleCastAuras())
            if (IsClassicLightsVigilAura(aura->GetId()))
                previous.push_back(aura);
        for (Aura* aura : previous)
            aura->Remove();

        ClassicLightsVigilRank const* rank = GetClassicLightsVigilRank(GetSpellInfo()->Id);
        uint32 const auraId = caster->IsFriendlyTo(target) ? rank->AllyAura : rank->EnemyAura;
        caster->CastSpell(target, auraId, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetTriggeringSpell(GetSpell()));

        if (Aura* vigil = target->GetAura(auraId, caster->GetGUID()))
        {
            if (!vigil->IsSingleTarget())
            {
                vigil->SetIsSingleTarget(true);
                caster->GetSingleCastAuras().push_front(vigil);
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_pal_lights_vigil::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1310909, 1310910, 1311593, 1311594, 1311597, 1311599 - Light's Vigil (marks): the data gives them one charge on any helpful / harmful spell
// taken; only the paladin's Holy Shock consumes them (classic_spell_pal_holy_shock_lights_vigil), so they never proc by themselves
class classic_spell_pal_lights_vigil_mark : public AuraScript
{
    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        return false;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pal_lights_vigil_mark::CheckProc);
    }
};

// 20473, 20929, 20930, 1311606 - Holy Shock (all ranks, extra script): on a target with the paladin's Light's Vigil the mark is consumed,
// enemies take the rank's Holy damage and the paladin gets back 75% (Light's Vigil effect 1) of Light's Vigil's mana cost, allies heal
// their party; the Holy Shock cooldown is reset after the cast
class classic_spell_pal_holy_shock_lights_vigil : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PAL_LIGHTS_VIGIL_ENERGIZE });
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        for (ClassicLightsVigilRank const& rank : ClassicLightsVigilRanks)
        {
            Aura* vigil = target->GetAura(rank.AllyAura, caster->GetGUID());
            if (!vigil)
                vigil = target->GetAura(rank.EnemyAura, caster->GetGUID());
            if (!vigil)
                continue;

            vigil->Remove();
            _vigilConsumed = true;

            CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
            args.SetTriggeringSpell(GetSpell());
            if (caster->IsFriendlyTo(target))
                caster->CastSpell(target, rank.PartyHeal, args);
            else
            {
                caster->CastSpell(target, rank.Damage, args);

                SpellInfo const* vigilInfo = sSpellMgr->AssertSpellInfo(rank.Cast, DIFFICULTY_NONE);
                int32 refundPct = vigilInfo->GetEffects().size() > 1 ? vigilInfo->GetEffect(EFFECT_1).CalcValueAsInt(caster) : 75;
                int32 mana = CalculatePct(GetClassicBaseManaCost(vigilInfo, caster), refundPct);
                if (mana > 0)
                    caster->CastSpell(caster, SPELL_PAL_LIGHTS_VIGIL_ENERGIZE, CastSpellExtraArgs(TRIGGERED_FULL_MASK)
                        .SetTriggeringSpell(GetSpell()).AddSpellBP0(mana));
            }
            break;
        }
    }

    void HandleAfterCast()
    {
        if (_vigilConsumed)
            GetCaster()->GetSpellHistory()->ResetCooldown(GetSpellInfo()->Id, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_pal_holy_shock_lights_vigil::HandleHit, EFFECT_0, SPELL_EFFECT_DUMMY);
        AfterCast += SpellCastFn(classic_spell_pal_holy_shock_lights_vigil::HandleAfterCast);
    }

    bool _vigilConsumed = false;
};

// 1310912, 1311591, 1311596 - Light's Vigil (heal): "heal their party": only the target and its party members in the area
class classic_spell_pal_lights_vigil_party_heal : public SpellScript
{
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Unit* target = GetExplTargetUnit();
        targets.remove_if([target](WorldObject* object)
        {
            Unit* unit = object->ToUnit();
            return !unit || !target || (unit != target && !unit->IsInPartyWith(target));
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(classic_spell_pal_lights_vigil_party_heal::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ALLY);
    }
};

// 1311606 - Holy Shock (Forever talent, level 30): "Blasts the target with Holy energy, causing Holy damage to an enemy, or healing to an
// ally." The dummy casts 1311604 (damage) or 1311605 (heal); same handling as classic_spell_pal_holy_shock for the trainer ranks.
class classic_spell_pal_holy_shock_forever : public SpellScript
{
    static constexpr uint32 SPELL_HOLY_SHOCK_DAMAGE = 1311604;
    static constexpr uint32 SPELL_HOLY_SHOCK_HEAL   = 1311605;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HOLY_SHOCK_DAMAGE, SPELL_HOLY_SHOCK_HEAL });
    }

    SpellCastResult CheckCast()
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (!target)
            return SPELL_FAILED_BAD_TARGETS;

        if (!caster->IsFriendlyTo(target))
        {
            if (!caster->IsValidAttackTarget(target))
                return SPELL_FAILED_BAD_TARGETS;
            if (!caster->isInFront(target))
                return SPELL_FAILED_UNIT_NOT_INFRONT;
        }
        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        caster->CastSpell(target, caster->IsFriendlyTo(target) ? SPELL_HOLY_SHOCK_HEAL : SPELL_HOLY_SHOCK_DAMAGE, GetSpell());
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_pal_holy_shock_forever::CheckCast);
        OnEffectHitTarget += SpellEffectFn(classic_spell_pal_holy_shock_forever::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 20271 - Judgement (extra script) for 1311074 Sanctified Judgement: "Gives your Judgement ability a 100% chance to return 60% of the Mana
// cost of the judged seal." Effect 0 is the chance, effect 1 the percentage; the energize is 1311077.
class classic_spell_pal_sanctified_judgement : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PAL_SANCTIFIED_JUDGEMENT_ENERGIZE });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        AuraEffect const* chance = caster->GetAuraEffect(SPELL_PAL_SANCTIFIED_JUDGEMENT, EFFECT_0);
        AuraEffect const* pct = caster->GetAuraEffect(SPELL_PAL_SANCTIFIED_JUDGEMENT, EFFECT_1);
        if (!chance || !pct || !roll_chance(float(chance->GetAmount())))
            return;

        for (auto const& [auraId, aurApp] : caster->GetAppliedAuras())
        {
            if (!GetClassicSealJudgement(auraId) || aurApp->GetBase()->GetCasterGUID() != caster->GetGUID())
                continue;

            int32 mana = CalculatePct(GetClassicBaseManaCost(aurApp->GetBase()->GetSpellInfo(), caster), pct->GetAmount());
            if (mana > 0)
                caster->CastSpell(caster, SPELL_PAL_SANCTIFIED_JUDGEMENT_ENERGIZE, CastSpellExtraArgs(TRIGGERED_FULL_MASK)
                    .SetTriggeringSpell(GetSpell()).AddSpellBP0(mana));
            break;
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_pal_sanctified_judgement::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 1310927 - Light's Fury (the Seal of Fury shield) for 1314103 Improved Seal of Fury: "When Seal of Fury's shield is fully absorbed, restore
// X Mana, increased by 15% per level the attacker is above you, up to 45%." Effect 0 is the mana (per level), effect 1 the percentage per
// level, effect 2 the level cap; the energize is 1314104.
class classic_spell_pal_improved_seal_of_fury : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PAL_IMPROVED_SEAL_OF_FURY_ENERGIZE });
    }

    void AfterAbsorb(AuraEffect* aurEff, DamageInfo& dmgInfo, uint32& absorbAmount)
    {
        // the remaining amount is reduced after this hook: fully absorbed when this absorb uses it up
        if (int64(absorbAmount) < int64(aurEff->GetAmountAsInt()))
            return;

        Unit* paladin = GetTarget();
        AuraEffect const* mana = paladin->GetAuraEffect(SPELL_PAL_IMPROVED_SEAL_OF_FURY, EFFECT_0);
        if (!mana)
            return;

        int32 amount = mana->GetAmountAsInt();
        if (Unit* attacker = dmgInfo.GetAttacker())
        {
            AuraEffect const* pctPerLevel = paladin->GetAuraEffect(SPELL_PAL_IMPROVED_SEAL_OF_FURY, EFFECT_1);
            AuraEffect const* maxLevels = paladin->GetAuraEffect(SPELL_PAL_IMPROVED_SEAL_OF_FURY, EFFECT_2);
            int32 levels = std::clamp<int32>(int32(attacker->GetLevel()) - int32(paladin->GetLevel()), 0, maxLevels ? maxLevels->GetAmountAsInt() : 0);
            if (pctPerLevel && levels > 0)
                amount += CalculatePct(amount, pctPerLevel->GetAmountAsInt() * levels);
        }

        if (amount > 0)
            paladin->CastSpell(paladin, SPELL_PAL_IMPROVED_SEAL_OF_FURY_ENERGIZE, CastSpellExtraArgs(aurEff).AddSpellBP0(amount));
    }

    void Register() override
    {
        AfterEffectAbsorb += AuraEffectAbsorbFn(classic_spell_pal_improved_seal_of_fury::AfterAbsorb, EFFECT_0);
    }
};

// Holy Light (635, 639, 647, 1026, 1042, 3472, 10328, 10329, 25292) and Flash of Light (19750, 19939-19943):
// - 19977, 19978, 19979 Blessing of Light (and 25890 Greater Blessing of Light): "increasing the effects of Holy Light spells used on the
//   target by up to X and the effect Flash of Light spells used on the target by up to Y". Effect 0 is the Holy Light bonus, effect 1 the
//   Flash of Light bonus; the best blessing on the target counts (no stacking between paladins), scaled by the heal coefficient (vanilla).
// - 412019 Sacred Shield: Flash of Light gets the shield's crit bonus (412018 effect 1, +50%) on targets with the paladin's Sacred Shield.
class classic_spell_pal_holy_light_flash_of_light : public SpellScript
{
    static bool IsHolyLight(SpellInfo const* spellInfo)
    {
        return (spellInfo->SpellFamilyFlags[0] & 0x80000000) != 0;
    }

    static bool IsBlessingOfLight(uint32 spellId)
    {
        return spellId == 19977 || spellId == 19978 || spellId == 19979 || spellId == 25890;
    }

    void HandleCalcHealing(SpellEffectInfo const& spellEffectInfo, Unit* victim, int32& /*healing*/, int32& flatMod, float& /*pctMod*/)
    {
        if (!victim)
            return;

        SpellEffIndex const blessingEffect = IsHolyLight(GetSpellInfo()) ? EFFECT_0 : EFFECT_1;
        int32 bonus = 0;
        for (AuraEffect const* aurEff : victim->GetAuraEffectsByType(SPELL_AURA_DUMMY))
            if (aurEff->GetEffIndex() == blessingEffect && IsBlessingOfLight(aurEff->GetId()))
                bonus = std::max(bonus, aurEff->GetAmountAsInt());

        if (bonus > 0)
            flatMod += int32(bonus * spellEffectInfo.BonusCoefficient);
    }

    void CalcCritChance(Unit const* victim, float& critChance)
    {
        if (!victim || IsHolyLight(GetSpellInfo()))
            return;

        if (AuraEffect const* sacredShield = victim->GetAuraEffect(SPELL_PAL_SACRED_SHIELD_ABSORB, EFFECT_1, GetCaster()->GetGUID()))
            critChance += float(sacredShield->GetAmount());
    }

    void Register() override
    {
        CalcHealing += SpellCalcHealingFn(classic_spell_pal_holy_light_flash_of_light::HandleCalcHealing);
        OnCalcCritChance += SpellOnCalcCritChanceFn(classic_spell_pal_holy_light_flash_of_light::CalcCritChance);
    }
};

// 412019 - Sacred Shield: "Each time the target takes damage they gain a Sacred Shield, absorbing [36 * (38.258376 + 0.904195 * L + 0.161311 *
// L * L) / 100] damage and increasing the Paladin's chance to critically hit with Flash of Light by 50% for up to 6 sec. In addition, causes
// your Flash of Light to heal targets with Sacred Shield for an additional 100% over 12 sec. They cannot gain this effect more than once
// every 6 sec. Lasts 1 min." Damage taken: 412018 (6 sec, effect 0 of 412019 is the 6 sec limit); the paladin's Flash of Light: 412020
// (12 sec heal over time, effect 2 of 412019 is the percentage). One target per paladin is the spell's own Limit N attribute.
class classic_spell_pal_sacred_shield : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_PAL_SACRED_SHIELD_ABSORB, SPELL_PAL_SACRED_SHIELD_FLASH_OF_LIGHT })
            && ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 } });
    }

    static bool IsFlashOfLight(SpellInfo const* spellInfo)
    {
        return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_PALADIN && (spellInfo->SpellFamilyFlags[0] & 0x40000000) != 0;
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (eventInfo.GetTypeMask() & PROC_FLAG_TAKE_ANY_DAMAGE)
        {
            DamageInfo* damageInfo = eventInfo.GetDamageInfo();
            return damageInfo && (damageInfo->GetDamage() || damageInfo->GetAbsorb()) && GameTime::Now() >= _nextShield;
        }

        HealInfo* healInfo = eventInfo.GetHealInfo();
        return healInfo && healInfo->GetHeal() && eventInfo.GetActor() && eventInfo.GetActor()->GetGUID() == GetCasterGUID()
            && IsFlashOfLight(healInfo->GetSpellInfo());
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        Unit* target = GetTarget();
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (eventInfo.GetTypeMask() & PROC_FLAG_TAKE_ANY_DAMAGE)
        {
            _nextShield = GameTime::Now() + Seconds(std::max(aurEff->GetAmountAsInt(), 1));

            SpellInfo const* shieldInfo = sSpellMgr->AssertSpellInfo(SPELL_PAL_SACRED_SHIELD_ABSORB, DIFFICULTY_NONE);
            float const level = float(caster->GetLevel());
            float const scale = 38.258376f + 0.904195f * level + 0.161311f * level * level;
            int32 absorb = int32(shieldInfo->GetEffect(EFFECT_0).CalcValue(caster) * scale / 100.0f);
            if (absorb > 0)
                caster->CastSpell(target, SPELL_PAL_SACRED_SHIELD_ABSORB, CastSpellExtraArgs(aurEff).AddSpellBP0(absorb));
            return;
        }

        SpellInfo const* hotInfo = sSpellMgr->AssertSpellInfo(SPELL_PAL_SACRED_SHIELD_FLASH_OF_LIGHT, DIFFICULTY_NONE);
        int32 const period = hotInfo->GetEffect(EFFECT_0).ApplyAuraPeriod;
        int32 const ticks = period > 0 ? std::max(hotInfo->GetMaxDuration() / period, 1) : 1;
        AuraEffect const* pct = GetEffect(EFFECT_2);
        int32 perTick = CalculatePct(int32(eventInfo.GetHealInfo()->GetHeal()), pct ? pct->GetAmount() : 100.0) / ticks;
        if (perTick > 0)
            caster->CastSpell(target, SPELL_PAL_SACRED_SHIELD_FLASH_OF_LIGHT, CastSpellExtraArgs(aurEff).AddSpellBP0(perTick));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pal_sacred_shield::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pal_sacred_shield::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }

    TimePoint _nextShield = TimePoint::min();
};

void AddSC_classic_talents_priest_paladin()
{
    // Priest
    RegisterSpellScript(classic_spell_pri_inspiration);
    RegisterSpellScript(classic_spell_pri_talent_chance);
    RegisterSpellScript(classic_spell_pri_blessed_recovery);
    RegisterSpellScript(classic_spell_pri_blessed_recovery_hot);
    RegisterSpellScript(classic_spell_pri_prayer_of_mending);
    RegisterSpellScript(classic_spell_pri_prayer_of_mending_aura);
    RegisterSpellScript(classic_spell_pri_prayer_of_mending_jump);
    RegisterSpellScript(classic_spell_pri_penance);
    RegisterSpellScript(classic_spell_pri_renewed_hope);
    RegisterSpellScript(classic_spell_pri_renewed_hope_crit);
    RegisterSpellScript(classic_spell_pri_divine_aegis);
    RegisterSpellScript(classic_spell_pri_power_in_light);
    RegisterSpellScript(classic_spell_pri_early_demise);
    RegisterSpellScript(classic_spell_pri_litany_of_light);
    RegisterSpellScript(classic_spell_pri_contingency_plan);

    // Paladin
    RegisterSpellScript(classic_spell_pal_eye_for_an_eye);
    RegisterSpellScript(classic_spell_pal_redoubt);
    RegisterSpellScript(classic_spell_pal_infusion_of_light);
    RegisterSpellScript(classic_spell_pal_consecrated_ground);
    RegisterSpellScript(classic_spell_pal_lights_vigil);
    RegisterSpellScript(classic_spell_pal_lights_vigil_mark);
    RegisterSpellScript(classic_spell_pal_holy_shock_lights_vigil);
    RegisterSpellScript(classic_spell_pal_lights_vigil_party_heal);
    RegisterSpellScript(classic_spell_pal_holy_shock_forever);
    RegisterSpellScript(classic_spell_pal_sanctified_judgement);
    RegisterSpellScript(classic_spell_pal_improved_seal_of_fury);
    RegisterSpellScript(classic_spell_pal_holy_light_flash_of_light);
    RegisterSpellScript(classic_spell_pal_sacred_shield);
}
