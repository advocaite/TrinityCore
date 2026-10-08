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

// Classic 1.60 class spell scripts.

#include "ScriptMgr.h"
#include "AreaTrigger.h"
#include "Creature.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "Unit.h"

/*######
## classic_spell_ground_area_damage
######*/

// Classic 1.60 ground spells (official client data): the spell creates an area trigger (effect 1) and puts a periodic dummy aura
// on the caster (effect 2); every tick damages the enemies inside the caster's area trigger of that spell. The client data has
// no damage spell or per-rank damage for them (placeholder values on effect 0): the tick damage per rank is the vanilla one
// (VMaNGOS spell_template, persistent area aura base points + 1), the spell power share per tick the vanilla coefficient.
struct ClassicGroundAreaDamage
{
    uint32 TickDamage;
    float Coefficient;      // spell power per tick
};

static std::unordered_map<uint32, ClassicGroundAreaDamage> const ClassicGroundAreaDamages =
{
    // Blizzard (frost, 1 s, 8 s channel): 33% spell power over 8 ticks
    { 10,    {  25, 0.0417f } }, { 6141,  {  44, 0.0417f } }, { 8427,  {  65, 0.0417f } },
    { 10185, {  90, 0.0417f } }, { 10186, { 117, 0.0417f } }, { 10187, { 149, 0.0417f } },
    // Flamestrike burning ground (fire, 2 s, 8 s; the direct hit is effect 0): ~12% spell power over 4 ticks
    { 2120,  {  12, 0.0305f } }, { 2121,  {  22, 0.0305f } }, { 8422,  {  35, 0.0305f } },
    { 8423,  {  49, 0.0305f } }, { 10215, {  66, 0.0305f } }, { 10216, {  85, 0.0305f } },
    // Rain of Fire (fire, 2 s, 8 s channel): 33% spell power over 4 ticks
    { 5740,  {  42, 0.0833f } }, { 6219,  {  96, 0.0833f } }, { 11677, { 155, 0.0833f } }, { 11678, { 226, 0.0833f } },
    // Volley (arcane, 1 s, 6 s channel): no spell power
    { 1510,  {  50, 0.0f } }, { 14294, {  65, 0.0f } }, { 14295, {  80, 0.0f } },
};

class classic_spell_ground_area_damage : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ClassicGroundAreaDamages.contains(spellInfo->Id);
    }

    void HandleTick(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        SpellInfo const* spellInfo = GetSpellInfo();
        ClassicGroundAreaDamage const& values = ClassicGroundAreaDamages.at(spellInfo->Id);

        GuidUnorderedSet targets;
        for (AreaTrigger* areaTrigger : caster->GetAreaTriggers(spellInfo->Id))
            targets.insert(areaTrigger->GetInsideUnits().begin(), areaTrigger->GetInsideUnits().end());

        for (ObjectGuid const& guid : targets)
        {
            Unit* target = ObjectAccessor::GetUnit(*caster, guid);
            if (!target || !target->IsAlive() || !caster->IsValidAttackTarget(target, spellInfo))
                continue;

            int32 damage = values.TickDamage;
            if (values.Coefficient > 0.0f)
                damage += int32(caster->SpellBaseDamageBonusDone(spellInfo->GetSchoolMask()) * values.Coefficient);
            damage = target->SpellDamageBonusTaken(caster, spellInfo, damage, DOT);

            SpellNonMeleeDamage damageInfo(caster, target, spellInfo, GetAura()->GetSpellVisual(), spellInfo->GetSchoolMask(), GetAura()->GetCastId());
            damageInfo.periodicLog = true;
            caster->CalculateSpellDamageTaken(&damageInfo, damage, spellInfo);
            Unit::DealDamageMods(damageInfo.attacker, damageInfo.target, damageInfo.damage, &damageInfo.absorb);
            caster->SendSpellNonMeleeDamageLog(&damageInfo);
            caster->DealSpellDamage(&damageInfo, true);
        }
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        // channels (Blizzard, Rain of Fire, Volley): the ground effect ends with the channel
        if (GetSpellInfo()->IsChanneled())
            if (Unit* caster = GetCaster())
                caster->RemoveAreaTrigger(GetSpellInfo()->Id);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_ground_area_damage::HandleTick, EFFECT_2, SPELL_AURA_PERIODIC_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_ground_area_damage::HandleRemove, EFFECT_2, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// Classic 1.60 (WoW Forever), Shen'dar Highlands cave, quest 94489 (official beta sniff 70205): clicking an injured druid makes the
// player cast 1276047 (dummy) on it. Credit "Injured Druids healed" (257964) and the druid's own hidden objective (its entry).
enum ClassicHealInjuredDruid
{
    NPC_INJURED_DRUIDS_HEALED_CREDIT = 257964
};

class classic_spell_heal_injured_druid : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        Creature* druid = GetHitCreature();
        if (!player || !druid)
            return;

        player->KilledMonsterCredit(NPC_INJURED_DRUIDS_HEALED_CREDIT);
        player->KilledMonsterCredit(druid->GetEntry());
        druid->SetStandState(UNIT_STAND_STATE_STAND);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_heal_injured_druid::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Discipline (99134): the item's spell applies Motivated to a Deathguard, but its dummy aura does not
// grant objective credit on its own.
enum ClassicDiscipline
{
    QUEST_DISCIPLINE                  = 99134,
    SPELL_MOTIVATED                   = 1319421,
    NPC_DEATHGUARD_LINNEA            = 1495,
    NPC_DEATHGUARD_DILLINGER         = 1496,
    NPC_DEATHGUARD_SIMMER            = 1519,
    NPC_DEATHGUARD_BURGESS           = 1652,
    NPC_DEATHGUARD_ABRAHAM           = 1735,
    NPC_DEATHGUARD_RANDOLPH          = 1736,
    NPC_DEATHGUARD_OLIVER            = 1737,
    NPC_DEATHGUARD_TERRENCE          = 1738,
    NPC_DEATHGUARD_PHILLIP           = 1739,
    NPC_DEATHGUARD_SALTAIN           = 1740,
    NPC_DEATHGUARD_BARTRAND          = 1741,
    NPC_DEATHGUARD_BARTHOLOMEW       = 1742,
    NPC_DEATHGUARD_LAWRENCE          = 1743,
    NPC_DEATHGUARD_MORT              = 1744,
    NPC_DEATHGUARD_MORRIS            = 1745,
    NPC_DEATHGUARD_CYRUS             = 1746,
    NPC_DEATHGUARD_GAVIN             = 2209,
    NPC_DEATHGUARD_ROYANN            = 2210,
    NPC_DEATHGUARD_LUNDMARK          = 5725,
    NPC_DEATHGUARD_KRISTOF           = 251001,
    NPC_DEATHGUARD_BILLMUTH          = 257655,
    NPC_DEATHGUARD_LIZABETHA         = 272101,
    NPC_DEATHGUARD_VETERAN           = 275103
};

class classic_spell_executors_motivator : public AuraScript
{
    static bool IsDeathguard(uint32 entry)
    {
        switch (entry)
        {
            case NPC_DEATHGUARD_LINNEA: case NPC_DEATHGUARD_DILLINGER: case NPC_DEATHGUARD_SIMMER: case NPC_DEATHGUARD_BURGESS:
            case NPC_DEATHGUARD_ABRAHAM: case NPC_DEATHGUARD_RANDOLPH: case NPC_DEATHGUARD_OLIVER: case NPC_DEATHGUARD_TERRENCE:
            case NPC_DEATHGUARD_PHILLIP: case NPC_DEATHGUARD_SALTAIN: case NPC_DEATHGUARD_BARTRAND: case NPC_DEATHGUARD_BARTHOLOMEW:
            case NPC_DEATHGUARD_LAWRENCE: case NPC_DEATHGUARD_MORT: case NPC_DEATHGUARD_MORRIS: case NPC_DEATHGUARD_CYRUS:
            case NPC_DEATHGUARD_GAVIN: case NPC_DEATHGUARD_ROYANN: case NPC_DEATHGUARD_LUNDMARK: case NPC_DEATHGUARD_KRISTOF:
            case NPC_DEATHGUARD_BILLMUTH: case NPC_DEATHGUARD_LIZABETHA: case NPC_DEATHGUARD_VETERAN:
                return true;
            default:
                return false;
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return spellInfo->Id == SPELL_MOTIVATED;
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        Creature* deathguard = GetTarget()->ToCreature();
        if (!player || !deathguard || !IsDeathguard(deathguard->GetEntry()) || player->GetQuestStatus(QUEST_DISCIPLINE) != QUEST_STATUS_INCOMPLETE)
            return;

        // Objective 479408 uses Cyrus's entry as credit for the Deathguard group, regardless of the guard hit.
        std::function<bool(QuestObjective const*)> const isDisciplineObjective = [](QuestObjective const* objective)
        {
            return objective->QuestID == QUEST_DISCIPLINE;
        };
        player->UpdateQuestObjectiveProgress(QUEST_OBJECTIVE_MONSTER, NPC_DEATHGUARD_CYRUS, 1, deathguard->GetGUID(), nullptr, &isDisciplineObjective);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_executors_motivator::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// Classic 1.60 (WoW Forever) profession books: Mining for Dummies (247840, 1245608), Wild Harvest (247841, 1245609), Pelt Collecting for
// Beginners (247846, 1245610). "Increases your <profession> skill by $m1. Cannot raise <profession> skill over $m2. You will learn
// <profession> if it is not already trained and you do not already know two other professions." Both effects are dummies.
class classic_spell_profession_book : public SpellScript
{
    struct BookProfession
    {
        uint32 SpellId;
        uint32 SkillId;
        uint32 ApprenticeSpell;
    };

    static BookProfession const* GetProfession(uint32 spellId)
    {
        static BookProfession const books[] =
        {
            { 1245608, SKILL_MINING,    2575 },     // Mining (Apprentice)
            { 1245609, SKILL_HERBALISM, 2366 },     // Herb Gathering (Apprentice)
            { 1245610, SKILL_SKINNING,  8613 },     // Skinning (Apprentice)
        };
        for (BookProfession const& book : books)
            if (book.SpellId == spellId)
                return &book;
        return nullptr;
    }

    uint32 GetCap() const
    {
        return uint32(std::max<int32>(GetSpellInfo()->GetEffect(EFFECT_1).CalcValue(), 1));
    }

    SpellCastResult CheckCast()
    {
        Player* player = GetCaster()->ToPlayer();
        BookProfession const* book = GetProfession(GetSpellInfo()->Id);
        if (!player || !book)
            return SPELL_FAILED_DONT_REPORT;

        if (player->HasSkill(book->SkillId))
            return player->GetPureSkillValue(book->SkillId) < GetCap() ? SPELL_CAST_OK : SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;

        return player->GetFreePrimaryProfessionPoints() > 0 ? SPELL_CAST_OK : SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        BookProfession const* book = GetProfession(GetSpellInfo()->Id);
        if (!player || !book)
            return;

        if (!player->HasSkill(book->SkillId))
            player->LearnSpell(book->ApprenticeSpell, false);
        if (!player->HasSkill(book->SkillId))
            return;

        uint16 const current = player->GetPureSkillValue(book->SkillId);
        uint16 const target = uint16(std::min<uint32>(current + uint32(GetEffectValue()), GetCap()));
        if (target > current)
            player->SetSkill(book->SkillId, player->GetSkillStep(book->SkillId), target, player->GetPureMaxSkillValue(book->SkillId));
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_profession_book::CheckCast);
        OnEffectHitTarget += SpellEffectFn(classic_spell_profession_book::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 20154, 21084, 20287-20293 - Seal of Righteousness: each melee swing that lands adds the rank's Holy damage spell. Vanilla formula on the
// seal's points (which grow per level) and the weapon: slower weapons hit harder, two-handers more (sniffs of the official beta:
// 20154 -> 25742, 21084 -> 25741, 3-6 damage at low level)
class classic_spell_pal_seal_of_righteousness : public AuraScript
{
    static uint32 GetDamageSpell(uint32 sealId)
    {
        switch (sealId)
        {
            case 20154: return 25742;
            case 21084: return 25741;
            case 20287: return 25740;
            case 20288: return 25739;
            case 20289: return 25738;
            case 20290: return 25737;
            case 20291: return 25736;
            case 20292: return 25735;
            case 20293: return 25713;
            default:    return 0;
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ GetDamageSpell(spellInfo->Id) });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        return GetTarget()->IsPlayer() && eventInfo.GetActionTarget() && eventInfo.GetDamageInfo()
            && eventInfo.GetDamageInfo()->GetAttackType() == BASE_ATTACK;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Player* player = GetTarget()->ToPlayer();
        Item* weapon = player->GetWeaponForAttack(BASE_ATTACK, true);
        float const speed = (weapon ? weapon->GetTemplate()->GetDelay() : BASE_ATTACK_TIME) / 1000.0f;
        float const points = float(aurEff->GetAmount());

        float damage;
        if (weapon && weapon->GetTemplate()->GetInventoryType() == INVTYPE_2HWEAPON)
            damage = 1.2f * points * 1.2f * 1.03f * speed / 100.0f + 1.0f;
        else
            damage = 0.85f * std::ceil(points * 1.2f * 1.03f * speed / 100.0f) - 1.0f;
        damage += 0.03f * (player->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE) + player->GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE)) / 2.0f;

        // spell power is added by the damage spell itself (its bonus coefficient)
        player->CastSpell(eventInfo.GetActionTarget(), GetDamageSpell(GetId()), CastSpellExtraArgs(aurEff)
            .AddSpellMod(SPELLVALUE_BASE_POINT0, std::max(int32(damage) + 1, 1)));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pal_seal_of_righteousness::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pal_seal_of_righteousness::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// 20271 - Judgement: unleashes the active seal's judgement on the target; the seal stays (sniff of the official beta 2026-10-06: Seal of
// Righteousness -> 20187, Seal of the Crusader -> 21183, Seal of Righteousness keeps adding Holy damage to the swings after Judgement)
enum ClassicPaladinJudgement
{
    SPELL_JUDGEMENT_OF_THE_CRUSADER_REFRESH_PROC = 25942,
    SPELL_JUDGEMENT_OF_THE_CRUSADER_REFRESH      = 25943
};

static bool IsCrusaderJudgement(uint32 spellId)
{
    switch (spellId)
    {
        case 21183: case 20188: case 20300: case 20301: case 20302: case 20303:
            return true;
        default:
            return false;
    }
}

class classic_spell_pal_judgement : public SpellScript
{
    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        for (auto const& [auraId, aurApp] : caster->GetAppliedAuras())
        {
            uint32 judgement = GetClassicSealJudgement(auraId);
            if (!judgement || aurApp->GetBase()->GetCasterGUID() != caster->GetGUID())
                continue;

            caster->CastSpell(target, judgement, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetTriggeringSpell(GetSpell()));
            // Judgement of the Crusader: the paladin's swings keep it on the target (25942 procs 25943, which casts it again)
            if (IsCrusaderJudgement(judgement))
                caster->CastSpell(caster, SPELL_JUDGEMENT_OF_THE_CRUSADER_REFRESH_PROC, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetTriggeringSpell(GetSpell()));
            break;
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_pal_judgement::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 25943 - Judgement of the Crusader (refresh, from the 25942 swing proc): casts the paladin's Judgement of the Crusader on the target
// again while it is on it (sniff 2026-10-06: every swing after the judgement shows 25942, 21183, 25943)
class classic_spell_pal_judgement_of_the_crusader_refresh : public SpellScript
{
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!target)
            return;

        for (auto const& [auraId, aurApp] : target->GetAppliedAuras())
        {
            if (!IsCrusaderJudgement(auraId) || aurApp->GetBase()->GetCasterGUID() != caster->GetGUID())
                continue;

            caster->CastSpell(target, auraId, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetTriggeringSpell(GetSpell()));
            break;
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_pal_judgement_of_the_crusader_refresh::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 20473, 20929, 20930 - Holy Shock (trainer, levels 40/48/56): heals a friendly target or damages an enemy with the rank's spell
// (the retail script only knows rank 1)
class classic_spell_pal_holy_shock : public SpellScript
{
    static std::pair<uint32, uint32> GetRankSpells(uint32 spellId) // damage, heal
    {
        switch (spellId)
        {
            case 20473: return { 25912, 25914 };
            case 20929: return { 25911, 25913 };
            case 20930: return { 25902, 25903 };
            default:    return { 0, 0 };
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        auto [damage, heal] = GetRankSpells(spellInfo->Id);
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

        auto [damage, heal] = GetRankSpells(GetSpellInfo()->Id);
        caster->CastSpell(target, caster->IsFriendlyTo(target) ? heal : damage, GetSpell());
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_pal_holy_shock::CheckCast);
        OnEffectHitTarget += SpellEffectFn(classic_spell_pal_holy_shock::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 1515 - Tame Beast: a 20 second channel in Classic (periodic aura on the beast); when it runs out the beast is tamed with 13481
// (SPELL_EFFECT_TAME_CREATURE). The retail script on 1515 (spell_hun_tame_beast) keeps its cast checks; it tames instantly in
// retail, so the Classic channel ended without a pet.
class classic_spell_hun_tame_beast_channel : public AuraScript
{
    static constexpr uint32 SPELL_TAME_BEAST_TAME = 13481;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_TAME_BEAST_TAME });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        if (Unit* caster = GetCaster())
            caster->CastSpell(GetTarget(), SPELL_TAME_BEAST_TAME, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(caster->GetGUID()));
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_hun_tame_beast_channel::HandleRemove, EFFECT_1, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1280003, 1280046, 1271103 - Taming Rod (Skyborne hunter quests Taming the Beast 94978, 94979, 94013), and 1277794, 1278028,
// 1278029 (human hunter quests 94792, 94863, 94864, not sniffed yet): a 20 second channel with a
// dummy aura on the beast; when it runs out the rod's tame spell charms the beast for 12 sec and completes the quest (sniff of the
// official beta: channel 20000 ms, then 1280004 / 1280044 / 1271102)
class classic_spell_hun_taming_rod : public AuraScript
{
    static uint32 GetTameSpell(uint32 channelSpellId)
    {
        switch (channelSpellId)
        {
            case 1280003: return 1280004;   // Windsong Crawler (94978)
            case 1280046: return 1280044;   // Ornery Galestrider (94979)
            case 1271103: return 1271102;   // Vuldren Alpha (94013)
            case 1277794: return 1277851;   // Rockhide Boar (94792, human)
            case 1278028: return 1278060;   // Gray Forest Wolf (94863, human)
            case 1278029: return 1278061;   // Young Forest Bear (94864, human)
            default:      return 0;
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ GetTameSpell(spellInfo->Id) });
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        if (Unit* caster = GetCaster())
            caster->CastSpell(GetTarget(), GetTameSpell(GetId()), CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(caster->GetGUID()));
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_hun_taming_rod::HandleRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 348, 707, 1094, 2941, 11665, 11667, 11668, 25309 - Immolate: Classic 1.60 added a script effect (EFFECT_2) that puts the hidden
// Immolate aura 1282590 on the target (sniff: both auras on the target, same caster). Every Conflagrate rank needs it
// (SpellAuraRestrictions TargetAuraSpell 1282590), so without it Conflagrate could never be cast.
class classic_spell_warl_immolate : public SpellScript
{
    static constexpr uint32 SPELL_IMMOLATE_CONFLAGRATE_MARKER = 1282590;

    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ SPELL_IMMOLATE_CONFLAGRATE_MARKER }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_2 } });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        // the marker targets its caster, so the target casts it on itself for the warlock
        Unit* target = GetHitUnit();
        target->CastSpell(target, SPELL_IMMOLATE_CONFLAGRATE_MARKER, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetOriginalCaster(GetCaster()->GetGUID()));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_warl_immolate::HandleScript, EFFECT_2, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

void AddSC_classic_spell_scripts()
{
    RegisterSpellScript(classic_spell_warl_immolate);
    RegisterSpellScript(classic_spell_hun_taming_rod);
    RegisterSpellScript(classic_spell_hun_tame_beast_channel);
    RegisterSpellScript(classic_spell_pal_holy_shock);
    RegisterSpellScript(classic_spell_pal_judgement_of_the_crusader_refresh);
    RegisterSpellScript(classic_spell_pal_seal_of_righteousness);
    RegisterSpellScript(classic_spell_pal_judgement);
    RegisterSpellScript(classic_spell_profession_book);
    RegisterSpellScript(classic_spell_ground_area_damage);
    RegisterSpellScript(classic_spell_heal_injured_druid);
    RegisterSpellScript(classic_spell_executors_motivator);
}
