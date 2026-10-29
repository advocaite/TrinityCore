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
#include "Creature.h"
#include "DB2Stores.h"
#include "Item.h"
#include "Pet.h"
#include "Player.h"
#include "Random.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "Unit.h"

enum ClassicWarriorRogueHunterTalents
{
    // warrior
    SPELL_WARRIOR_TACTICAL_MASTERY              = 1310185,  // baseline (level 14): retain $m1 rage on stance change
    SPELL_WARRIOR_IMPROVED_TACTICAL_MASTERY     = 12295,
    SPELL_WARRIOR_ENRAGE_BUFF                   = 12880,
    SPELL_WARRIOR_BLOOD_CRAZE_REGEN             = 16488,
    SPELL_WARRIOR_MEATHOOK_PULL                 = 403233,

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
};

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

// 1310222 - Spearing Strike (talent ability): "A brutal attack that deals $s2% weapon damage. Deals an additional ${$s2*$s3}% weapon
// damage against Giants, Dragonkin, and mounted targets. Mounted targets are dismounted." Effect 2 is a dummy that holds the multiplier,
// so the bonus and the dismount were missing.
class classic_spell_warr_spearing_strike : public SpellScript
{
    static bool IsSpearingTarget(Unit const* target)
    {
        uint32 type = target->GetCreatureType();
        return target->IsMounted() || type == CREATURE_TYPE_GIANT || type == CREATURE_TYPE_DRAGONKIN;
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 } });
    }

    void HandleHit()
    {
        Unit* target = GetHitUnit();
        if (!target || !IsSpearingTarget(target))
            return;

        // the weapon damage effects deal $s2%: the bonus is that times $s3
        SetHitDamage(GetHitDamage() + int32(GetHitDamage() * GetEffectInfo(EFFECT_2).CalcValue(GetCaster())));
        if (target->IsMounted())
            target->RemoveAurasByType(SPELL_AURA_MOUNTED);
    }

    void Register() override
    {
        OnHit += SpellHitFn(classic_spell_warr_spearing_strike::HandleHit);
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

void AddSC_classic_talents_warrior_rogue_hunter()
{
    // warrior
    RegisterSpellScript(classic_spell_warr_stance_rage);
    RegisterSpellScript(classic_spell_warr_enrage_talent);
    RegisterSpellScript(classic_spell_warr_blood_craze);
    RegisterSpellScriptWithArgs(classic_spell_weapon_type_talent, "classic_spell_warr_weaponmaster", 12700u, 12284u, 12281u);
    RegisterSpellScript(classic_spell_weapon_extra_attack_chance);
    RegisterSpellScript(classic_spell_warr_meathook);
    RegisterSpellScript(classic_spell_warr_spearing_strike);

    // rogue
    RegisterSpellScriptWithArgs(classic_spell_weapon_type_talent, "classic_spell_rog_hack_and_slash", 13706u, 13709u, 1290312u);
    RegisterSpellScript(classic_spell_rog_setup);
    RegisterSpellScript(classic_spell_rog_seal_fate);
    RegisterSpellScript(classic_spell_rog_cutthroat);
    RegisterSpellScript(classic_spell_rog_offhand_strike);
    RegisterSpellScript(classic_spell_rog_offhand_poison);
    RegisterSpellAndAuraScriptPair(classic_spell_rog_redirect, classic_spell_rog_redirect_aura);

    // hunter
    RegisterSpellScript(classic_spell_hun_entrapment_trap);
    RegisterSpellScript(classic_spell_hun_entrapment_frost_trap);
    RegisterSpellScript(classic_spell_hun_spirit_bond);
    RegisterSpellScript(classic_spell_hun_rapid_recuperation);
    RegisterSpellScript(classic_spell_hun_rapid_killing_consumed);
    RegisterSpellScript(classic_spell_hun_lacerating_strikes);
    RegisterSpellAndAuraScriptPair(classic_spell_hun_kill_command, classic_spell_hun_kill_command_aura);
    RegisterSpellScript(classic_spell_hun_kill_command_pet);

    // Focused Fire, Quietus
    new classic_unit_talent_damage_mods();
}
