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
#include "CellImpl.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Containers.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Formulas.h"
#include "GameObject.h"
#include "GameTime.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "QuestDef.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "ThreatManager.h"
#include "Unit.h"
#include "WorldSession.h"
#include <mutex>
#include <unordered_map>

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

// 1259705 - Read Ley Line, 1259686 - Skysight (Skyborne racials): the spell only triggers a 30 sec buff (Energized 1270842 /
// Elemental Blessing 1259688). Next to its spell focus object (Ley Line 602735 / 613248, focus 2246 / Elemental Convergence
// 616992, focus 2271) the official server casts the 15 min version first (Energized 1259691 / Elemental Blessing 1270893); the
// 30 sec trigger still shows a SPELL_GO there but adds no aura (sniffs: Alliance Skyborne hunter at ley lines, Windshaper at
// convergences, every cast away from one got only the 30 sec aura). Energized 1259691 also completes "Reading the Ley Lines" (92597).
class classic_spell_skyborne_racial_focus : public SpellScript
{
public:
    explicit classic_spell_skyborne_racial_focus(uint32 spellFocus, uint32 longBuffSpell) : _spellFocus(spellFocus), _longBuffSpell(longBuffSpell) { }

private:
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ _longBuffSpell }) && ValidateSpellEffect({ { spellInfo->Id, EFFECT_0 } });
    }

    void HandleTrigger(SpellEffIndex effIndex)
    {
        Unit* caster = GetCaster();
        GameObject* focus = nullptr;
        Trinity::GameObjectFocusCheck check(caster, _spellFocus);
        Trinity::GameObjectSearcher searcher(caster, focus, check);
        Cell::VisitGridObjects(caster, searcher, 50.0f); // focus radius of both objects is 25
        if (!focus)
            return;

        PreventHitDefaultEffect(effIndex);
        caster->CastSpell(caster, _longBuffSpell, CastSpellExtraArgs(TRIGGERED_FULL_MASK).SetTriggeringSpell(GetSpell()));
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(classic_spell_skyborne_racial_focus::HandleTrigger, EFFECT_0, SPELL_EFFECT_TRIGGER_SPELL);
    }

    uint32 _spellFocus;
    uint32 _longBuffSpell;
};

// 1319421 - Motivated (Executor's Motivator, undead quest Discipline 99134): a dummy aura on a Tirisfal Deathguard. The official
// server credits the objective (Deathguard Cyrus 1746, 5 needed) for any of them, with an empty victim GUID (sniff of 2026-10-06:
// QUEST_UPDATE_ADD_CREDIT 99134 / 1746, 1..5 of 5; Motivated was seen on 1496, 1652, 1735, 1738, 1742-1746, 2210, 5725, 272101).
class classic_spell_executors_motivator : public AuraScript
{
    static constexpr uint32 QUEST_DISCIPLINE = 99134;
    static constexpr uint32 NPC_DEATHGUARD_CYRUS = 1746;

    static bool IsTirisfalDeathguard(uint32 entry)
    {
        switch (entry)
        {
            case 1495: case 1496: case 1519: case 1652: case 1735: case 1736: case 1737: case 1738: case 1739: case 1740:
            case 1741: case 1742: case 1743: case 1744: case 1745: case 1746: case 2209: case 2210: case 5725:
            case 251001: case 257655: case 272101: case 275103:
                return true;
            default:
                return false;
        }
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* player = Object::ToPlayer(GetCaster());
        if (!player || !IsTirisfalDeathguard(GetTarget()->GetEntry()) || player->GetQuestStatus(QUEST_DISCIPLINE) != QUEST_STATUS_INCOMPLETE)
            return;

        std::function<bool(QuestObjective const*)> const isDiscipline = [](QuestObjective const* objective) { return objective->QuestID == QUEST_DISCIPLINE; };
        player->UpdateQuestObjectiveProgress(QUEST_OBJECTIVE_MONSTER, NPC_DEATHGUARD_CYRUS, 1, ObjectGuid::Empty, nullptr, &isDiscipline);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_executors_motivator::HandleApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1289195-1289200 - Alchemy / Blacksmithing / Enchanting / Engineering / Leatherworking / Tailoring Certification (items 271621-271627,
// 1000 Merchant Favor at the trade company vendors): "unlocks the <profession> title for all characters on your account; to display
// it they must have 300 skill". The spell completes a hidden quest (95944-95949, not in our data); the account unlock is kept in
// account_classic_certification and Player::UpdateClassicProfessionTitles gives the title to characters with 300 skill.
class classic_spell_profession_certification : public SpellScript
{
    static uint32 SkillOf(uint32 spellId)
    {
        switch (spellId)
        {
            case 1289195: return SKILL_ALCHEMY;
            case 1289196: return SKILL_BLACKSMITHING;
            case 1289197: return SKILL_ENCHANTING;
            case 1289198: return SKILL_ENGINEERING;
            case 1289199: return SKILL_LEATHERWORKING;
            case 1289200: return SKILL_TAILORING;
            default: return 0;
        }
    }

    void HandleAfterCast()
    {
        Player* player = GetCaster()->ToPlayer();
        uint32 skillId = SkillOf(GetSpellInfo()->Id);
        if (!player || !skillId)
            return;

        // written directly: UpdateClassicProfessionTitles reads it right away
        CharacterDatabase.DirectPExecute("REPLACE INTO account_classic_certification (account, skill) VALUES ({}, {})",
            player->GetSession()->GetAccountId(), skillId);
        player->UpdateClassicProfessionTitles();
    }

    void Register() override
    {
        AfterCast += SpellCastFn(classic_spell_profession_certification::HandleAfterCast);
    }
};

// 1225472 - Thrill of Adventure (Legacy, Adventure tree 1188): after a kill that gives experience the official server casts 1261483
// on the killer, 10 sec of health (aura 8) and mana (aura 24) every 2 sec. Sniff of a level 1-10 dwarf shaman (5/5, aura amount 5):
// applied after 72 of 73 kills (the other one was before the talent), ticks of 1% of max health / mana (0.84 health, 0.96 mana per
// tick at 84 / 96), so the talent amount is the total percent restored over the 5 ticks.
class classic_player_thrill_of_adventure : public PlayerScript
{
public:
    classic_player_thrill_of_adventure() : PlayerScript("classic_player_thrill_of_adventure") { }

    static constexpr uint32 SPELL_THRILL_OF_ADVENTURE = 1225472;
    static constexpr uint32 SPELL_THRILL_OF_ADVENTURE_REGEN = 1261483;

    void OnCreatureKill(Player* killer, Creature* killed) override
    {
        AuraEffect const* talent = killer->GetAuraEffect(SPELL_THRILL_OF_ADVENTURE, EFFECT_0);
        // tooltip: "each time you deliver the killing blow to a non-trivial enemy. Ineffective in dungeons, raids, and battlegrounds."
        if (!talent || !talent->GetAmount() || !Trinity::XP::Gain(killer, killed) || killer->GetMap()->Instanceable())
            return;

        SpellInfo const* regen = sSpellMgr->GetSpellInfo(SPELL_THRILL_OF_ADVENTURE_REGEN, DIFFICULTY_NONE);
        if (!regen || regen->GetEffects().size() < 2 || !regen->GetEffect(EFFECT_0).ApplyAuraPeriod)
            return;

        int32 ticks = std::max(1, regen->GetMaxDuration() / int32(regen->GetEffect(EFFECT_0).ApplyAuraPeriod));
        float pctPerTick = float(talent->GetAmount()) / ticks;
        int32 health = std::max(1, int32(CalculatePct(float(killer->GetMaxHealth()), pctPerTick) + 0.5f));
        int32 mana = killer->GetMaxPower(POWER_MANA) ? std::max(1, int32(CalculatePct(float(killer->GetMaxPower(POWER_MANA)), pctPerTick) + 0.5f)) : 0;

        CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, health);
        args.AddSpellMod(SPELLVALUE_BASE_POINT1, mana);
        killer->CastSpell(killer, SPELL_THRILL_OF_ADVENTURE_REGEN, args);
    }
};

// 1225503 - Reagent Economy (Legacy, Adventure tree 1188): its effect is aura 215, which this core handles as arena preparation
// (unit flag PREPARATION + aura state). The perk must not put the player in that state; the reagent effect itself is not known yet.
class classic_spell_legacy_reagent_economy : public AuraScript
{
    void PreventArenaPreparation(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        PreventDefaultAction();
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(classic_spell_legacy_reagent_economy::PreventArenaPreparation, EFFECT_0, SPELL_AURA_ARENA_PREPARATION, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(classic_spell_legacy_reagent_economy::PreventArenaPreparation, EFFECT_0, SPELL_AURA_ARENA_PREPARATION, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1225469 - Dedicated Study (Legacy, Professions tree 1187), a castable spell: "Increases your skill by 1 in your lowest tradeskill among
// your Primary and Secondary tradeskills. If you have already reached 300 skill in your two current Primary Tradeskills and all three
// Secondary Tradeskills, you will gain $s1 of a random Elemental Essence."
class classic_spell_legacy_dedicated_study : public SpellScript
{
    static constexpr uint16 MaxTradeskill = 300;
    static constexpr uint32 ElementalEssences[] = { 7076, 7078, 7080, 7082 };   // Earth, Fire, Water, Air

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;

        uint32 lowestSkill = 0;
        uint16 lowestValue = std::numeric_limits<uint16>::max();
        uint32 primaries = 0;
        bool allMaxed = true;
        for (SkillLineEntry const* skillLine : sSkillLineStore)
        {
            if (skillLine->ParentSkillLineID || (skillLine->CategoryID != SKILL_CATEGORY_PROFESSION && skillLine->CategoryID != SKILL_CATEGORY_SECONDARY))
                continue;
            if (!player->HasSkill(skillLine->ID))
                continue;

            uint16 value = player->GetPureSkillValue(skillLine->ID);
            if (skillLine->CategoryID == SKILL_CATEGORY_PROFESSION)
                ++primaries;
            if (value < MaxTradeskill)
                allMaxed = false;
            if (value < player->GetPureMaxSkillValue(skillLine->ID) && value < lowestValue)
            {
                lowestValue = value;
                lowestSkill = skillLine->ID;
            }
        }

        bool secondariesMaxed = player->GetPureSkillValue(SKILL_COOKING) >= MaxTradeskill && player->GetPureSkillValue(129 /* First Aid */) >= MaxTradeskill
            && player->GetPureSkillValue(SKILL_FISHING) >= MaxTradeskill;
        if (allMaxed && primaries >= 2 && secondariesMaxed)
        {
            player->AddItem(Trinity::Containers::SelectRandomContainerElement(ElementalEssences), std::max(1, GetEffectValueAsInt()));
            return;
        }

        if (lowestSkill)
            player->SetSkill(lowestSkill, player->GetSkillStep(lowestSkill), lowestValue + 1, player->GetPureMaxSkillValue(lowestSkill));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_legacy_dedicated_study::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 11196 - Recently Bandaged: Legacy perk Field Medicine 1225475 (Adventure tree) shortens it by its amount in ms (negative) when the
// bandage user has the perk. "Ineffective in dungeons, raids, and battlegrounds."
class classic_spell_recently_bandaged_field_medicine : public AuraScript
{
    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player const* bandager = Object::ToPlayer(GetCaster());
        if (!bandager || bandager->GetMap()->Instanceable())
            return;

        int32 reduction = bandager->GetClassicLegacyPerkAmount(1225475, EFFECT_0);
        if (reduction >= 0)
            return;

        Aura* aura = GetAura();
        int32 duration = std::max(1000, aura->GetMaxDuration() + reduction);
        aura->SetMaxDuration(duration);
        aura->SetDuration(duration);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_recently_bandaged_field_medicine::HandleApply, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 1225477 - High Alert (Legacy, Adventure tree 1188): stealth detection as if your level were higher. "Ineffective in Battlegrounds."
class classic_spell_legacy_high_alert : public AuraScript
{
    void CalculateAmount(AuraEffect const* /*aurEff*/, SpellEffectValue& amount, bool& canBeRecalculated)
    {
        canBeRecalculated = true;
        if (GetUnitOwner()->GetMap()->IsBattlegroundOrArena())
            amount = 0;
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(classic_spell_legacy_high_alert::CalculateAmount, EFFECT_0, SPELL_AURA_MOD_STEALTH_DETECT);
    }
};

// Seal of Fury (Forever, 1311649 1311656 20163 20419 20421 20422 20423): "Fills the Paladin with divine fury for 30 sec, causing melee
// attacks to deal an additional N Holy damage. While a shield is equipped, each attack also grants an absorb shield equal to 50% of the
// Holy damage dealt." Its judgement (Judgement of Fury) is in the seal table of SpellInfo.cpp.
static uint32 ClassicSealOfFuryDamageSpell(uint32 sealId)
{
    switch (sealId)
    {
        case 1311649: return 1311647;
        case 1311656: return 1311654;
        case 20163:   return 20231;
        case 20419:   return 20415;
        case 20421:   return 20416;
        case 20422:   return 20417;
        case 20423:   return 20418;
        default:      return 0;
    }
}

class classic_spell_pal_seal_of_fury : public AuraScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        return ValidateSpellInfo({ ClassicSealOfFuryDamageSpell(spellInfo->Id) });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        return eventInfo.GetActionTarget() && eventInfo.GetDamageInfo() && eventInfo.GetDamageInfo()->GetAttackType() == BASE_ATTACK
            && !eventInfo.GetSpellInfo();
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(eventInfo.GetActionTarget(), ClassicSealOfFuryDamageSpell(GetId()), CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pal_seal_of_fury::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pal_seal_of_fury::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// Seal of Fury Holy damage (1311647 1311654 20231 20415-20418): with a shield equipped the paladin gains Light's Fury (1310927), an
// absorb shield of the seal's effect 1 percent (50) of the damage dealt
class classic_spell_pal_seal_of_fury_damage : public SpellScript
{
    static constexpr uint32 SPELL_LIGHTS_FURY = 1310927;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_LIGHTS_FURY });
    }

    void HandleAfterHit()
    {
        Player* paladin = Object::ToPlayer(GetCaster());
        if (!paladin || GetHitDamage() <= 0)
            return;

        Item const* offhand = paladin->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
        if (!offhand || offhand->GetTemplate()->GetClass() != ITEM_CLASS_ARMOR || offhand->GetTemplate()->GetSubClass() != ITEM_SUBCLASS_ARMOR_SHIELD)
            return;

        for (uint32 sealId : { 1311649u, 1311656u, 20163u, 20419u, 20421u, 20422u, 20423u })
        {
            if (Aura const* seal = paladin->GetAura(sealId))
            {
                // effect 1 is a plain value (Effect 0), not an aura effect
                int32 absorb = CalculatePct(GetHitDamage(), seal->GetSpellInfo()->GetEffect(EFFECT_1).CalcValue(paladin));
                if (absorb > 0)
                    paladin->CastSpell(paladin, SPELL_LIGHTS_FURY, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(absorb));
                break;
            }
        }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(classic_spell_pal_seal_of_fury_damage::HandleAfterHit);
    }
};

// Judgement of Fury (20183 20411-20414 1311650 1311655): "Unleashing this Seal's energy causes Holy damage to an enemy and taunts the
// target to attack you for 4 sec." Effect 1 (dummy) is the taunt, effect 2 (dummy) the extra threat.
class classic_spell_pal_judgement_of_fury : public SpellScript
{
    void HandleTaunt(SpellEffIndex /*effIndex*/)
    {
        Creature* target = Object::ToCreature(GetHitUnit());
        Unit* paladin = GetCaster()->ToUnit();
        if (!target || !paladin || !target->CanHaveThreatList())
            return;

        target->GetThreatManager().MatchUnitThreatToHighestThreat(paladin);
        if (target->IsAIEnabled())
            target->AI()->AttackStart(paladin);
    }

    // effect 2 has no implicit target, so it runs once in hit mode against the judged unit
    void HandleThreat(SpellEffIndex /*effIndex*/)
    {
        Creature* target = Object::ToCreature(GetExplTargetUnit());
        Unit* paladin = GetCaster()->ToUnit();
        if (target && paladin && target->CanHaveThreatList())
            target->GetThreatManager().AddThreat(paladin, float(GetEffectValue()), GetSpellInfo(), true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_pal_judgement_of_fury::HandleTaunt, EFFECT_1, SPELL_EFFECT_DUMMY);
        OnEffectHit += SpellEffectFn(classic_spell_pal_judgement_of_fury::HandleThreat, EFFECT_2, SPELL_EFFECT_DUMMY);
    }
};

// 26573, 20116, 20922, 20923, 20924 - Consecration (Classic 1.60, official dungeon sniff): the cast puts a 1 sec periodic dummy on the
// paladin for 8 sec and creates area trigger 41311-41316 at his feet (no data on our server); each tick the official server casts the
// rank's Consecration damage (1280345-1280349) at the consecrated spot, hitting the enemies standing in it.
class classic_spell_pal_consecration : public AuraScript
{
    static uint32 TickSpell(uint32 spellId)
    {
        switch (spellId)
        {
            case 26573: return 1280345;
            case 20116: return 1280346;
            case 20922: return 1280347;
            case 20923: return 1280348;
            case 20924: return 1280349;
            default:    return 0;
        }
    }

    bool Validate(SpellInfo const* spellInfo) override
    {
        return TickSpell(spellInfo->Id) && ValidateSpellInfo({ TickSpell(spellInfo->Id) });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        _center = GetUnitOwner()->GetPosition();
    }

    void HandleTick(AuraEffect const* aurEff)
    {
        PreventDefaultAction();
        Unit* paladin = GetUnitOwner();
        paladin->CastSpell(_center, TickSpell(GetId()), CastSpellExtraArgs(aurEff));
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_pal_consecration::HandleApply, EFFECT_2, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_pal_consecration::HandleTick, EFFECT_2, SPELL_AURA_PERIODIC_DUMMY);
    }

    Position _center;
};

// 15286 - Vampiric Embrace (vanilla, VMaNGOS UnitAuraProcHandler): a debuff on the enemy. Shadow damage the priest deals to it
// heals the priest's party (15290, the priest included) for 20% of the damage. The retail script treated it as a self buff and
// made the debuffed enemy cast the heal.
class classic_spell_pri_vampiric_embrace : public AuraScript
{
    static constexpr uint32 SPELL_VAMPIRIC_EMBRACE_HEAL = 15290;

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_VAMPIRIC_EMBRACE_HEAL });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        DamageInfo const* damageInfo = eventInfo.GetDamageInfo();
        return damageInfo && damageInfo->GetDamage() && eventInfo.GetActor() && eventInfo.GetActor()->GetGUID() == GetCasterGUID();
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* priest = eventInfo.GetActor();
        int32 heal = std::max<int32>(1, CalculatePct(int32(eventInfo.GetDamageInfo()->GetDamage()), aurEff->GetAmount()));

        CastSpellExtraArgs args(aurEff);
        args.AddSpellMod(SPELLVALUE_BASE_POINT0, heal);
        priest->CastSpell(priest, SPELL_VAMPIRIC_EMBRACE_HEAL, args);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(classic_spell_pri_vampiric_embrace::CheckProc);
        OnEffectProc += AuraEffectProcFn(classic_spell_pri_vampiric_embrace::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// High Alert follows the map: recalculated when entering or leaving a battleground
class classic_player_high_alert_map : public PlayerScript
{
public:
    classic_player_high_alert_map() : PlayerScript("classic_player_high_alert_map") { }

    void OnMapChanged(Player* player) override
    {
        if (AuraEffect* highAlert = player->GetAuraEffect(1225477, EFFECT_0))
            highAlert->RecalculateAmount();
    }
};

// 5217 - Tiger's Fury (Classic 1.60): a dummy aura, effect 0 = 15, "increases physical damage done by 15% for 6 sec". Applied to melee
// swings and physical abilities; the periodic hook carries no school, so bleeds are left alone.
class classic_unit_tigers_fury : public UnitScript
{
public:
    classic_unit_tigers_fury() : UnitScript("classic_unit_tigers_fury") { }

    static constexpr uint32 SPELL_TIGERS_FURY = 5217;

    static void Apply(Unit* attacker, uint32& damage)
    {
        if (!attacker || !damage || attacker->GetClass() != CLASS_DRUID)
            return;

        if (AuraEffect const* fury = attacker->GetAuraEffect(SPELL_TIGERS_FURY, EFFECT_0))
            damage = AddPct(damage, fury->GetAmount());
    }

    void ModifyMeleeDamage(Unit* /*target*/, Unit* attacker, uint32& damage) override
    {
        Apply(attacker, damage);
    }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!spellInfo || damage <= 0 || !(spellInfo->GetSchoolMask() & SPELL_SCHOOL_MASK_NORMAL))
            return;

        uint32 value = uint32(damage);
        Apply(attacker, value);
        damage = int32(value);
    }
};

// Tauren racial Cultivation (1.60): "Cultivate a nearby herb, growing a duplicate you can harvest without requiring Herbalism skill.
// Each herb may only be cultivated once." Official sniffs (2026-09-28, 2026-10-01): the spellbook spell 20552 (instant, dummy on game
// objects within 5 yd) makes the server start 1312643 (3 sec cast) on the herb; when it completes a duplicate herb appears on the same
// spot, created by the player: same model and loot, lock 4522 = key spell 1312650 (open lock without skill, the player does not know
// it; Player allows lock key spells of the target). Peacebloom 1618 -> 657382, Wilted Peacebloom 656160 -> 657454; the other twins are
// made by 2026_10_14_00_world_classic_tauren_cultivation.sql. A twin is found by model + loot.
namespace ClassicCultivation
{
    static constexpr uint32 SPELL_CULTIVATE_HERB = 1312643;
    static constexpr uint32 LOCK_CULTIVATED_HERB = 4522;
    static constexpr float SEARCH_RANGE = 5.0f;
    static constexpr Seconds TWIN_DURATION = 5min;
    static constexpr Seconds HERB_COOLDOWN = 15min;    // "each herb may only be cultivated once" (until it has been picked and regrown)

    static bool IsHerbalismLock(uint32 lockId)
    {
        LockEntry const* lock = sLockStore.LookupEntry(lockId);
        if (!lock)
            return false;
        for (uint8 i = 0; i < MAX_LOCK_CASE; ++i)
            if (lock->Type[i] == LOCK_KEY_SKILL && lock->Index[i] == LOCKTYPE_HERBALISM)
                return true;
        return false;
    }

    // herb (model, loot) -> twin entry
    static uint32 GetTwinEntry(GameObjectTemplate const* herb)
    {
        static std::unordered_map<uint64, uint32> const twins = []
        {
            std::unordered_map<uint64, uint32> map;
            for (auto const& [entry, goInfo] : sObjectMgr->GetGameObjectTemplates())
                if (goInfo.type == GAMEOBJECT_TYPE_CHEST && goInfo.chest.open == LOCK_CULTIVATED_HERB)
                    map.emplace(uint64(goInfo.displayId) << 32 | goInfo.chest.chestLoot, entry);
            return map;
        }();
        if (!herb || herb->type != GAMEOBJECT_TYPE_CHEST || !IsHerbalismLock(herb->chest.open))
            return 0;
        auto itr = twins.find(uint64(herb->displayId) << 32 | herb->chest.chestLoot);
        return itr != twins.end() ? itr->second : 0;
    }

    static std::unordered_map<ObjectGuid, TimePoint> CultivatedHerbs;
    static std::mutex CultivatedHerbsLock;

    static bool WasCultivated(ObjectGuid const& herb)
    {
        std::lock_guard<std::mutex> guard(CultivatedHerbsLock);
        auto itr = CultivatedHerbs.find(herb);
        if (itr == CultivatedHerbs.end())
            return false;
        if (GameTime::Now() - itr->second < HERB_COOLDOWN)
            return true;
        CultivatedHerbs.erase(itr);
        return false;
    }

    static void MarkCultivated(ObjectGuid const& herb)
    {
        std::lock_guard<std::mutex> guard(CultivatedHerbsLock);
        CultivatedHerbs[herb] = GameTime::Now();
    }

    static bool CanCultivate(GameObject const* go)
    {
        return go && go->isSpawned() && go->GetGoState() == GO_STATE_READY && !go->GetOwnerGUID()
            && GetTwinEntry(go->GetGOInfo()) && !WasCultivated(go->GetGUID());
    }

    static GameObject* FindHerb(Unit* caster)
    {
        std::list<GameObject*> objects;
        Trinity::AllWorldObjectsInRange check(caster, SEARCH_RANGE);
        Trinity::GameObjectListSearcher<Trinity::AllWorldObjectsInRange> searcher(caster, objects, check);
        Cell::VisitGridObjects(caster, searcher, SEARCH_RANGE);
        GameObject* best = nullptr;
        for (GameObject* go : objects)
            if (CanCultivate(go) && (!best || caster->GetExactDist2dSq(go) < caster->GetExactDist2dSq(best)))
                best = go;
        return best;
    }
}

// 20552 - Cultivation (Tauren racial, spellbook): starts Cultivate (1312643) on the nearest herb within 5 yd
class classic_spell_tauren_cultivation : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ ClassicCultivation::SPELL_CULTIVATE_HERB });
    }

    SpellCastResult CheckCast()
    {
        return ClassicCultivation::FindHerb(GetCaster()) ? SPELL_CAST_OK : SPELL_FAILED_NO_VALID_TARGETS;
    }

    void HandleAfterCast()
    {
        if (GameObject* herb = ClassicCultivation::FindHerb(GetCaster()))
            GetCaster()->CastSpell(herb, ClassicCultivation::SPELL_CULTIVATE_HERB, CastSpellExtraArgs(TRIGGERED_IGNORE_GCD));
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_tauren_cultivation::CheckCast);
        AfterCast += SpellCastFn(classic_spell_tauren_cultivation::HandleAfterCast);
    }
};

// 1312643 - Cultivate: the herb's twin grows on the same spot, for the caster only
class classic_spell_tauren_cultivate_herb : public SpellScript
{
    SpellCastResult CheckCast()
    {
        GameObject* herb = GetExplTargetGObj();
        return ClassicCultivation::CanCultivate(herb) ? SPELL_CAST_OK : SPELL_FAILED_BAD_TARGETS;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* caster = GetCaster()->ToPlayer();
        GameObject* herb = GetHitGObj();
        if (!caster || !ClassicCultivation::CanCultivate(herb))
            return;

        uint32 twinEntry = ClassicCultivation::GetTwinEntry(herb->GetGOInfo());
        GameObject* twin = GameObject::CreateGameObject(twinEntry, herb->GetMap(), *herb, herb->GetLocalRotation(), 255, GO_STATE_READY);
        if (!twin)
            return;

        ClassicCultivation::MarkCultivated(herb->GetGUID());
        PhasingHandler::InheritPhaseShift(twin, caster);
        twin->SetPrivateObjectOwner(caster->GetGUID());
        twin->SetRespawnTime(ClassicCultivation::TWIN_DURATION.count());
        caster->AddGameObject(twin);
        herb->GetMap()->AddToMap(twin);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(classic_spell_tauren_cultivate_herb::CheckCast);
        OnEffectHitTarget += SpellEffectFn(classic_spell_tauren_cultivate_herb::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Classic 1.60 (client 70374) warrior talent Gore Drinker 1323967: "Your Enrage, Berserker Rage, Bloodrage, Death Wish, and Bloodthirst
// abilities cause your next $m1 melee attacks to restore ${$s2}.1% of your maximum Health." (both effects are server-side dummies: 4
// attacks, 0.5 / 1% by rank). The buff 1323969 procs on melee attacks (SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE) and heals with 1323968
// for its amount.
namespace ClassicGoreDrinker
{
    static constexpr uint32 SPELL_GORE_DRINKER_TALENT = 1323967;
    static constexpr uint32 SPELL_GORE_DRINKER_BUFF = 1323969;

    static void Apply(Unit* warrior)
    {
        AuraEffect const* attacks = warrior->GetAuraEffect(SPELL_GORE_DRINKER_TALENT, EFFECT_0);
        AuraEffect const* healthPct = warrior->GetAuraEffect(SPELL_GORE_DRINKER_TALENT, EFFECT_1);
        if (!attacks || !healthPct || attacks->GetAmount() <= 0 || healthPct->GetAmount() <= 0)
            return;

        int32 heal = std::max(1, int32(warrior->CountPctFromMaxHealth(healthPct->GetAmount())));
        warrior->CastSpell(warrior, SPELL_GORE_DRINKER_BUFF, CastSpellExtraArgs(TRIGGERED_FULL_MASK).AddSpellBP0(heal));
        if (Aura* buff = warrior->GetAura(SPELL_GORE_DRINKER_BUFF))
            buff->SetCharges(uint8(attacks->GetAmount()));
    }
}

// 2687 - Bloodrage, 18499 - Berserker Rage, 12328 - Death Wish, 23881, 23892, 23893, 23894 - Bloodthirst: Gore Drinker
class classic_spell_warr_gore_drinker : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ ClassicGoreDrinker::SPELL_GORE_DRINKER_BUFF });
    }

    void HandleAfterCast()
    {
        ClassicGoreDrinker::Apply(GetCaster());
    }

    void Register() override
    {
        AfterCast += SpellCastFn(classic_spell_warr_gore_drinker::HandleAfterCast);
    }
};

// 12880 - Enrage (talent proc buff): Gore Drinker
class classic_spell_warr_gore_drinker_enrage : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ ClassicGoreDrinker::SPELL_GORE_DRINKER_BUFF });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        ClassicGoreDrinker::Apply(GetTarget());
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(classic_spell_warr_gore_drinker_enrage::HandleApply, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 324, 325, 905, 945, 8134, 10431, 10432 - Lightning Shield: every rank triggers the dummy 26545 in 1.60, so cast the rank's
// damage spell on the attacker instead (26364-26370, 26363)
class classic_spell_sha_lightning_shield : public AuraScript
{
    static uint32 GetDamageSpell(uint32 shieldId)
    {
        switch (shieldId)
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
        if (Unit* attacker = eventInfo.GetActor())
            GetTarget()->CastSpell(attacker, GetDamageSpell(GetId()), CastSpellExtraArgsInit{
                .TriggerFlags = TRIGGERED_FULL_MASK,
                .TriggeringAura = aurEff
            });
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(classic_spell_sha_lightning_shield::HandleProc, EFFECT_0, SPELL_AURA_PROC_TRIGGER_SPELL);
    }
};

void AddSC_classic_spell_scripts()
{
    RegisterSpellScript(classic_spell_warr_gore_drinker);
    RegisterSpellScript(classic_spell_warr_gore_drinker_enrage);
    RegisterSpellScript(classic_spell_tauren_cultivation);
    RegisterSpellScript(classic_spell_tauren_cultivate_herb);
    new classic_unit_tigers_fury();
    RegisterSpellScript(classic_spell_pri_vampiric_embrace);
    RegisterSpellScript(classic_spell_pal_consecration);
    RegisterSpellScript(classic_spell_pal_seal_of_fury);
    RegisterSpellScript(classic_spell_pal_seal_of_fury_damage);
    RegisterSpellScript(classic_spell_pal_judgement_of_fury);
    RegisterSpellScript(classic_spell_legacy_high_alert);
    new classic_player_high_alert_map();
    RegisterSpellScript(classic_spell_legacy_dedicated_study);
    RegisterSpellScript(classic_spell_recently_bandaged_field_medicine);
    new classic_player_thrill_of_adventure();
    RegisterSpellScript(classic_spell_legacy_reagent_economy);
    RegisterSpellScript(classic_spell_profession_certification);
    RegisterSpellScript(classic_spell_executors_motivator);
    RegisterSpellScriptWithArgs(classic_spell_skyborne_racial_focus, "classic_spell_skyborne_read_ley_line", 2246, 1259691);
    RegisterSpellScriptWithArgs(classic_spell_skyborne_racial_focus, "classic_spell_skyborne_skysight", 2271, 1270893);
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
    RegisterSpellScript(classic_spell_sha_lightning_shield);
}
