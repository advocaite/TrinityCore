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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/burning_steppes/molten_core/boss_baron_geddon.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: boss_baron_geddon

#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "ScriptedCreature.h"
#include "ClassicAuraCaster.h"
#include "SpellAuraEffects.h"
#include "SpellDefines.h"
#include "SpellScript.h"
#include "classic_molten_core.h"
#include "classic_script_text.h"

namespace
{
enum ClassicMcBaronGeddon : uint32
{
    EMOTE_GEDDON_SERVICE        = 8253,

    SPELL_GEDDON_INFERNO        = 19695,
    SPELL_GEDDON_IGNITEMANA     = 19659,
    SPELL_GEDDON_LIVINGBOMB     = 20475,
    SPELL_GEDDON_ARMAGEDDON     = 20478,
    SPELL_GEDDON_INFERNO_DAMAGE = 19698,
    SPELL_GEDDON_LIVINGBOMB_EXPLOSION = 20476
};
}

struct classic_boss_baron_geddon : public ScriptedAI
{
    classic_boss_baron_geddon(Creature* creature) : ScriptedAI(creature), m_pInstance(creature->GetInstanceScript()) { }

    uint32 m_uiIgniteManaTimer = 0;
    uint32 m_uiLivingBombTimer = 0;
    uint32 m_uiInfernoTimer = 0;
    uint32 m_uiRestoreTargetTimer = 0;
    uint32 InfCount = 0;
    uint32 Tick = 0;
    bool m_bInferno = false;
    bool m_bArmageddon = false;

    InstanceScript* m_pInstance;

    void Reset() override
    {
        m_uiIgniteManaTimer    = urand(10000, 15000);
        m_uiLivingBombTimer    = urand(15000, 20000);
        m_uiInfernoTimer       = urand(18000, 24000);
        m_uiRestoreTargetTimer = 0;
        m_bInferno             = false;
        m_bArmageddon          = false;

        if (m_pInstance && me->IsAlive())
            m_pInstance->SetData(CLASSIC_MC_TYPE_GEDDON, NOT_STARTED);

        me->ClearUnitState(UNIT_STATE_ROOT);
        SetCombatMovement(true);
        me->SetCanMelee(true);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_MC_TYPE_GEDDON, IN_PROGRESS);
        DoZoneInCombat();
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(CLASSIC_MC_TYPE_GEDDON, DONE);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        if (m_bArmageddon)
            return;

        // If we are <5% hp cast Armageddon
        if (me->GetHealthPct() < 5.0f)
        {
            me->InterruptNonMeleeSpells(true);
            SetCombatMovement(false);
            me->CastSpell(me, SPELL_GEDDON_ARMAGEDDON, true);
            ClassicScriptText(EMOTE_GEDDON_SERVICE, me);
            m_bArmageddon = true;
            me->SetCanMelee(false); // VMaNGOS returns before DoMeleeAttackIfReady from now on
            return;
        }

        if (m_uiLivingBombTimer < diff)
        {
            if (Unit* pTarget = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true))
            {
                if (DoCast(pTarget, SPELL_GEDDON_LIVINGBOMB) == SPELL_CAST_OK)
                {
                    me->SetInFront(pTarget);
                    me->SetTarget(pTarget->GetGUID());
                    m_uiLivingBombTimer = urand(12000, 15000);
                    m_uiRestoreTargetTimer = 800;
                }
            }
        }
        else
            m_uiLivingBombTimer -= diff;

        // Restore original target after casting Living Bomb on someone
        if (m_uiRestoreTargetTimer)
        {
            if (m_uiRestoreTargetTimer <= diff)
            {
                if (Unit* pTarget = me->GetVictim())
                {
                    me->SetInFront(pTarget);
                    me->SetTarget(pTarget->GetGUID());
                    m_uiRestoreTargetTimer = 0;
                }
            }
            else
                m_uiRestoreTargetTimer -= diff;
        }

        if (m_uiIgniteManaTimer < diff)
        {
            if (DoCastSelf(SPELL_GEDDON_IGNITEMANA) == SPELL_CAST_OK)
                m_uiIgniteManaTimer = urand(20000, 30000);
        }
        else
            m_uiIgniteManaTimer -= diff;

        if (m_uiInfernoTimer < diff)
        {
            if (DoCastSelf(SPELL_GEDDON_INFERNO) == SPELL_CAST_OK)
            {
                m_uiInfernoTimer = urand(18000, 24000);
                InfCount = 0;
                Tick = 1000;
                m_bInferno = true;
                me->SetControlled(true, UNIT_STATE_ROOT); // the state, the root flag and a stop: AddUnitState alone leaves a walk in progress running
                me->SetCanMelee(false); // VMaNGOS skips DoMeleeAttackIfReady while Inferno ticks
            }
        }
        else
            m_uiInfernoTimer -= diff;

        // Inferno damage increases with each tick
        if (m_bInferno)
        {
            // He channels in place (VMaNGOS): whatever re-armed a walk (a root aura that fell off, a new victim's chase) is stopped again.
            if (me->isMoving())
                me->StopMoving();
            if (!me->HasUnitState(UNIT_STATE_ROOT))
                me->SetControlled(true, UNIT_STATE_ROOT);
            if (Tick >= 1000)
            {
                int32 damage = 0;
                switch (InfCount)
                {
                    case 0:
                    case 1:
                        damage = 500;
                        break;
                    case 2:
                    case 3:
                        damage = 1000;
                        break;
                    case 4:
                    case 5:
                        damage = 2000;
                        break;
                    case 6:
                        damage = 3000;
                        break;
                    case 7:
                        damage = 5000;
                        m_bInferno = false;
                        me->SetControlled(false, UNIT_STATE_ROOT);
                        me->SetCanMelee(true);
                        break;
                    default:
                        break;
                }
                CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
                args.AddSpellBP0(damage);
                me->CastSpell(me, SPELL_GEDDON_INFERNO_DAMAGE, args);
                InfCount++;
                Tick = 0;
            }
            Tick += diff;
            return;
        }

        // melee: TC master auto-melee
    }
};

// 20475 - Living Bomb (MC, Baron Geddon)
// The explosion goes off on the bomb's holder, but VMaNGOS names the aura's caster (Geddon) as the original caster: the flat 3200 fire damage is
// Geddon's, with no spell power, crit or talent of the holder. TrinityCore's periodic trigger casts it from the holder alone, which let a mage
// carrier's spell power lift it to 5539. The target selection (holder as the centre, the raid as Geddon's enemies) is unchanged.
class classic_spell_geddon_living_bomb : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_GEDDON_LIVINGBOMB_EXPLOSION });
    }

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        PreventDefaultAction();

        Unit* holder = GetTarget();
        Unit* caster = GetCaster();

        CastSpellExtraArgs args(TRIGGERED_FULL_MASK & ~(TRIGGERED_IGNORE_POWER_COST | TRIGGERED_IGNORE_REAGENT_COST));
        args.SetTriggeringAura(aurEff);
        if (ClassicAuraCaster::UseAuraCaster(caster != nullptr, caster && caster->IsInWorld(), caster == holder))
            args.SetOriginalCaster(caster->GetGUID());
        holder->CastSpell(holder, SPELL_GEDDON_LIVINGBOMB_EXPLOSION, args);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_geddon_living_bomb::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

void AddSC_classic_boss_baron_geddon()
{
    RegisterCreatureAI(classic_boss_baron_geddon);
    RegisterSpellScript(classic_spell_geddon_living_bomb);
}
