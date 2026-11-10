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

// Classic 1.60: VMaNGOS spell_scripts (DB scripts run by SPELL_EFFECT_SCRIPT_EFFECT / dummy effects) that TC has no handler for.
// Generated data source: classic_re/classic_dbscripts.py, spell_script_names in sql/custom/world/2026_10_12_32_world_classic_spell_scripts.sql.

#include "ScriptMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "MotionMaster.h"
#include "ObjectDefines.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "Unit.h"
#include <unordered_map>

namespace
{
// VMaNGOS SCRIPT_COMMAND_CAST_SPELL rows of spell_scripts: the scripted spell makes a unit cast a second spell on itself (triggered).
// Two rows guarded by team conditions (VMaNGOS conditions 2 = Horde, 3 = Alliance) become the alliance / horde spell.
struct ClassicDbScriptCast
{
    uint32 AllianceSpell;
    uint32 HordeSpell;
    bool HitUnitCasts;      // true: the unit hit by the script effect casts (VMaNGOS data_flags swap), false: the caster casts
};

std::unordered_map<uint32, ClassicDbScriptCast> const ClassicDbScriptCasts =
{
    // class mounts: summon visual
    { 5784,  { 31725, 31725, false } },     // Summon Felsteed -> Summon Nightmare (visual)
    { 23161, { 31725, 31725, false } },     // Summon Dreadsteed -> Summon Nightmare (visual)
    { 13819, { 31726, 31726, false } },     // Summon Warhorse -> Summon Holy Mount Visual
    { 23214, { 31726, 31726, false } },     // Summon Charger -> Summon Holy Mount Visual
    // Lunar Festival teleport traps (Moonglade beams): home faction goes to the city, the other faction back to Moonglade
    { 26406, { 26454, 26451, true } },      // Lunar Teleport Cap: Stormwind
    { 26408, { 26452, 26451, true } },      // Lunar Teleport Cap: Ironforge
    { 26409, { 26450, 26451, true } },      // Lunar Teleport Cap: Darnassus
    { 26410, { 26451, 26456, true } },      // Lunar Teleport Cap: Undercity
    { 26412, { 26451, 26455, true } },      // Lunar Teleport Cap: Thunder Bluff
    { 26414, { 26451, 26453, true } },      // Lunar Teleport Cap: Orgrimmar
    // Winter Veil: Jingling Bell
    { 26528, { 26529, 26529, false } },     // Summon Reindeer -> Summon Reindeer (summon spell)
};

enum ClassicDbScriptSpells
{
    SPELL_CLASSIC_SOUL_LINK_AURA = 25228
};
}

/*######
## classic_spell_dbscript_cast (script effect -> unit casts a spell on itself)
######*/

class classic_spell_dbscript_cast : public SpellScript
{
    bool Validate(SpellInfo const* spellInfo) override
    {
        auto itr = ClassicDbScriptCasts.find(spellInfo->Id);
        return itr != ClassicDbScriptCasts.end() && ValidateSpellInfo({ itr->second.AllianceSpell, itr->second.HordeSpell });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        auto itr = ClassicDbScriptCasts.find(GetSpellInfo()->Id);
        if (itr == ClassicDbScriptCasts.end())
            return;

        Unit* unit = itr->second.HitUnitCasts ? GetHitUnit() : GetCaster();
        if (!unit)
            return;

        uint32 spellId = itr->second.AllianceSpell;
        if (Player* player = unit->ToPlayer())
            if (player->GetTeam() == HORDE)
                spellId = itr->second.HordeSpell;

        unit->CastSpell(unit, spellId, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_dbscript_cast::HandleScript, EFFECT_ALL, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

/*######
## classic_spell_warl_soul_link (19028)
######*/

// Soul Link 19028: the demon (dummy effect target) casts 25228 on itself, triggered. A pet area aura, it also reaches the
// master, with the demon as caster: both deal 3% more damage, and the demon takes 30% of the damage done to the master.
// (18814, OLDSoul Link Effect, is that split alone, as a hidden aura.)
class classic_spell_warl_soul_link : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CLASSIC_SOUL_LINK_AURA });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* demon = GetHitUnit();
        if (!caster || !demon || demon == caster)
            return;

        demon->CastSpell(demon, SPELL_CLASSIC_SOUL_LINK_AURA, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_warl_soul_link::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

/*######
## Darkmoon Faire Steam Tonk (24935 Summon RC Tank, 24936 RC Tank Control, 24937 Using Control Console)
######*/

enum ClassicSteamTonk
{
    SPELL_CLASSIC_SUMMON_RC_TANK        = 24934,
    SPELL_CLASSIC_RC_TANK_ROOT          = 24935,
    SPELL_CLASSIC_RC_TANK_CONTROL       = 24936,
    SPELL_CLASSIC_USING_CONTROL_CONSOLE = 24937,
    SPELL_CLASSIC_DAMAGED_TONK          = 27771,
    SPELL_CLASSIC_TONK_STUN             = 9179,
    NPC_CLASSIC_DARKMOON_STEAM_TONK     = 15328,
    GO_CLASSIC_TONK_CONTROL_CONSOLE     = 180524
};

// VMaNGOS spell_scripts 24935: the target summons the tonk, 2 s later it casts RC Tank Control
class classic_spell_summon_rc_tank : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CLASSIC_SUMMON_RC_TANK, SPELL_CLASSIC_RC_TANK_CONTROL });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* target = GetHitUnit();
        if (!target)
            return;

        target->CastSpell(target, SPELL_CLASSIC_SUMMON_RC_TANK, true);
        target->m_Events.AddEventAtOffset([target]()
        {
            target->CastSpell(target, SPELL_CLASSIC_RC_TANK_CONTROL, true);
        }, 2s);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_summon_rc_tank::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// VMaNGOS Spell::EffectScriptEffect 24936: possess the nearest steam tonk (5 yd)
class classic_spell_rc_tank_control : public SpellScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CLASSIC_USING_CONTROL_CONSOLE });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (Creature* tonk = caster->FindNearestCreature(NPC_CLASSIC_DARKMOON_STEAM_TONK, 5.0f, true))
        {
            tonk->SetWalk(false);
            caster->CastSpell(tonk, SPELL_CLASSIC_USING_CONTROL_CONSOLE, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_rc_tank_control::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// VMaNGOS ControllingSteamTonkAuraScript: control ends -> damaged tonk, 3 s stun, unroot, console ready again
class classic_spell_using_control_console : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CLASSIC_DAMAGED_TONK, SPELL_CLASSIC_TONK_STUN });
    }

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* tonk = GetTarget();
        Player* player = Object::ToPlayer(GetCaster());
        if (!player)
            return;

        tonk->CastSpell(tonk, SPELL_CLASSIC_DAMAGED_TONK, true);
        player->CastSpell(player, SPELL_CLASSIC_TONK_STUN, true);
        player->RemoveAurasDueToSpell(SPELL_CLASSIC_RC_TANK_ROOT);

        if (GameObject* console = player->FindNearestGameObject(GO_CLASSIC_TONK_CONTROL_CONSOLE, INTERACTION_DISTANCE))
        {
            console->SetGoState(GO_STATE_READY);
            console->SetLootState(GO_READY);
            console->RemoveFlag(GO_FLAG_IN_USE);
        }
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(classic_spell_using_control_console::AfterRemove, EFFECT_0, SPELL_AURA_MOD_POSSESS, AURA_EFFECT_HANDLE_REAL);
    }
};

/*######
## classic_go_strat_service_entrance (Stratholme Service Entrance Gate 175368)
######*/

// VMaNGOS gameobject_scripts 6851: using the gate starts Magistrate Barthilas (C++ classic_boss_magistrate_barthilas) on his
// waypoint path (VMaNGOS creature_movement_template 10435, imported as path 810104350)
enum ClassicStratServiceEntrance
{
    NPC_CLASSIC_MAGISTRATE_BARTHILAS = 10435,
    PATH_CLASSIC_BARTHILAS_RUN       = 810104350
};

struct classic_go_strat_service_entrance : public GameObjectAI
{
    classic_go_strat_service_entrance(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* /*player*/) override
    {
        if (Creature* barthilas = me->FindNearestCreature(NPC_CLASSIC_MAGISTRATE_BARTHILAS, 50.0f, true))
            if (!barthilas->IsInCombat())
                barthilas->GetMotionMaster()->MovePath(PATH_CLASSIC_BARTHILAS_RUN, false);
        return false;
    }
};

void AddSC_classic_dbscript_spells()
{
    RegisterSpellScript(classic_spell_dbscript_cast);
    RegisterSpellScript(classic_spell_warl_soul_link);
    RegisterSpellScript(classic_spell_summon_rc_tank);
    RegisterSpellScript(classic_spell_rc_tank_control);
    RegisterSpellScript(classic_spell_using_control_console);
    RegisterGameObjectAI(classic_go_strat_service_entrance);
}
