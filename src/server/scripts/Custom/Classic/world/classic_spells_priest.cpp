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

// Classic 1.60 priest spells the client data leaves to server code (ClassicPriestSpells.h).
// Behaviour of Touch of Weakness, Holy Nova and Power Word: Shield (Weakened Soul) follows vMaNGOS (GPL-2.0, vmangos/core
// src/scripts/spells/spell_priest.cpp): the dummy proc casts the rank's damage spell, Holy Nova casts its heal after the damage, a hit
// shield puts Weakened Soul on the target. Amounts come from the client spells.

#include "ScriptMgr.h"
#include "ClassicPriestSpells.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "Unit.h"

// Touch of Weakness / Shadowguard: the proc trigger aura names a dummy spell, so a hit on the priest did nothing. The rank's own
// damage spell is cast at the attacker instead (it carries the shadow damage and, for Touch of Weakness, the attack power debuff).
class classic_spell_pri_touch_of_weakness : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        uint32 const damage = ClassicPriestSpells::Find(ClassicPriestSpells::TouchOfWeakness, spellInfo->Id);
        return damage && ValidateSpellInfo({ damage });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* attacker = eventInfo.GetActor();
        uint32 const damage = ClassicPriestSpells::Find(ClassicPriestSpells::TouchOfWeakness, GetId());
        if (attacker && damage)
            GetTarget()->CastSpell(attacker, damage, aurEff);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_pri_touch_of_weakness::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

class classic_spell_pri_shadowguard : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        uint32 const damage = ClassicPriestSpells::Find(ClassicPriestSpells::Shadowguard, spellInfo->Id);
        return damage && ValidateSpellInfo({ damage });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* attacker = eventInfo.GetActor();
        uint32 const damage = ClassicPriestSpells::Find(ClassicPriestSpells::Shadowguard, GetId());
        if (attacker && damage)
            GetTarget()->CastSpell(attacker, damage, aurEff);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_pri_shadowguard::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

// Holy Nova: the damage spell has one effect, the heal of the same rank is its own spell that vanilla cast with it.
class classic_spell_pri_holy_nova : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        uint32 const heal = ClassicPriestSpells::Find(ClassicPriestSpells::HolyNova, spellInfo->Id);
        return heal && ValidateSpellInfo({ heal });
    }

    void HandleAfterCast()
    {
        Unit* caster = GetCaster();
        uint32 const heal = ClassicPriestSpells::Find(ClassicPriestSpells::HolyNova, GetSpellInfo()->Id);
        if (heal)
            caster->CastSpell(caster, heal, GetSpell());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(classic_spell_pri_holy_nova::HandleAfterCast);
    }
};

// Power Word: Shield: the client spell has only the absorb effect; vanilla put Weakened Soul (15 s, shield immunity) on the target
// in server code and refused a target that carries it. Recasting while it is up is refused here too.
class classic_spell_pri_power_word_shield : public SpellScript
{
    static constexpr uint32 SPELL_WEAKENED_SOUL = 6788;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_WEAKENED_SOUL });
    }

    SpellCastResult CheckCast()
    {
        Unit* target = GetExplTargetUnit();
        if (target && target->HasAura(SPELL_WEAKENED_SOUL))
            return SPELL_FAILED_TARGET_AURASTATE;
        return SPELL_CAST_OK;
    }

    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            GetCaster()->CastSpell(target, SPELL_WEAKENED_SOUL, GetSpell());
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_pri_power_word_shield::CheckCast);
        OnEffectHitTarget += SpellEffectFn(classic_spell_pri_power_word_shield::HandleHit, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

// 1277370, 1277371, 1277372, 1277374, 1277376, 1277377, 1277378 - Divine Grace (70532): "Instantly heals a friendly target below $s2% Health
// for $s1 and removes Weakened Soul from that target. Cannot be cast on self." Effect 1 is a script effect with the health threshold that
// nothing handled: the heal landed at any health and Weakened Soul stayed. The self restriction is a spell attribute.
class classic_spell_pri_divine_grace : public SpellScript
{
    static constexpr uint32 SPELL_WEAKENED_SOUL = 6788;

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_WEAKENED_SOUL }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_1 } });
    }

    SpellCastResult CheckCast()
    {
        Unit* target = GetExplTargetUnit();
        if (target && !target->HealthBelowPct(GetEffectInfo(EFFECT_1).CalcValue(GetCaster())))
            return SPELL_FAILED_TARGET_AURASTATE;
        return SPELL_CAST_OK;
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            target->RemoveAurasDueToSpell(SPELL_WEAKENED_SOUL);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_pri_divine_grace::CheckCast);
        OnEffectHitTarget += SpellEffectFn(classic_spell_pri_divine_grace::HandleScript, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

void AddSC_classic_spells_priest()
{
    RegisterSpellScript(classic_spell_pri_touch_of_weakness);
    RegisterSpellScript(classic_spell_pri_shadowguard);
    RegisterSpellScript(classic_spell_pri_holy_nova);
    RegisterSpellScript(classic_spell_pri_power_word_shield);
    RegisterSpellScript(classic_spell_pri_divine_grace);
}
