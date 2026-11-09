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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/uldaman/boss_archaedas.cpp (MaNGOS / ScriptDev2 lineage, GPL-2)
// Archaedas is activated when 1 person clicks on his altar.
// Every 10 seconds he will awaken one of his minions along the wall.
// At 66%, he will awaken the 6 Earthen Guardians.
// At 33%, he will awaken the 2 Vault Warders
// On his death the vault door opens.
// Ported: boss_archaedas, mob_archaedas_minions

#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "classic_uldaman.h"
#include "classic_script_text.h"
#include <initializer_list>

enum ArchaedasTexts
{
    SAY_AGGRO           = 3400,
    SAY_SUMMON          = 6536,
    SAY_SUMMON_2        = 6537,
    SAY_SLAY            = 6215
};

struct classic_boss_archaedas : public ScriptedAI
{
    classic_boss_archaedas(Creature* creature) : ScriptedAI(creature)
    {
        _instance = creature->GetInstanceScript();
        Initialize();
        _justCreated = true;
    }

    void Initialize()
    {
        _tremorTimer = 60000;
        _awakenTimer = 0;
        _wallMinionTimer = 10000;
        _roomCheck = 500;
        _wakingUp = false;
        _guardiansAwake = false;
        _vaultWardersAwake = false;
    }

    bool UnitIsOutside(Unit* unit) const
    {
        Position const spawn = me->GetRespawnPosition();    // VMaNGOS GetRespawnCoord() in the constructor
        return !unit->IsWithinDist2d(spawn.GetPositionX(), spawn.GetPositionY(), 38.0f);
    }

    void Reset() override
    {
        Initialize();
        me->SetUninteractible(true);                        // VMaNGOS UNIT_FLAG_UNINTERACTIBLE
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        // Being woken up from the altar, start the awaken sequence
        if (spellInfo->Id == SPELL_ARCHAEDAS_AWAKEN && !_wakingUp)
        {
            ClassicScriptText(SAY_AGGRO, me);
            _awakenTimer = 4000;
            _wakingUp = true;
        }
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        ClassicScriptText(SAY_SLAY, me);
    }

    // He goes back to his spawn point after reset, stone him after.
    void JustReachedHome() override
    {
        Reset();
        if (_instance)
            _instance->SetData(ULDAMAN_ENCOUNTER_ARCHAEDAS, NOT_STARTED);
    }

    void UpdateAI(uint32 diff) override
    {
        if (_justCreated)
        {
            _justCreated = false;
            JustReachedHome();
        }

        if (!_instance || _instance->GetData(ULDAMAN_ENCOUNTER_ARCHAEDAS) != IN_PROGRESS)
            return;

        // we're still doing awaken animation
        if (_wakingUp && _awakenTimer >= 0)
        {
            _awakenTimer -= int32(diff);
            return; // dont do anything until we are done
        }
        else if (_wakingUp && _awakenTimer <= 0)
        {
            _wakingUp = false;
            if (Unit* target = me->SelectNearestTarget(80.0f))
                AttackStart(target);
            return; // dont want to continue until we finish the AttackStart method
        }

        //Return since we have no target
        if (!UpdateVictim())
            return;

        // check if the target is still inside the room
        if (_roomCheck <= diff)
        {
            if (UnitIsOutside(me) || UnitIsOutside(me->GetVictim()))
            {
                EnterEvadeMode(EvadeReason::Boundary);
                return;
            }
            _roomCheck = 500;
        }
        else
            _roomCheck -= diff;

        // wake a wall minion every 10 s while above a third of his health; once the Vault Warders are up the wall stays asleep
        // (classic: the dwarves stop coming in the last phase)
        if (!_vaultWardersAwake)
        {
            if (_wallMinionTimer <= diff)
            {
                _instance->SetData(ULDAMAN_ENCOUNTER_ARCHAEDAS, IN_PROGRESS);
                _wallMinionTimer = 10000;
            }
            else
                _wallMinionTimer -= diff;
        }

        //If we are <66 summon the guardians
        if (!_guardiansAwake && me->GetHealthPct() <= 66.0f)
        {
            me->CastSpell(me, SPELL_AWAKEN_EARTHEN_GUARDIAN, false);
            ClassicScriptText(SAY_SUMMON, me);
            _guardiansAwake = true;
        }

        //If we are <33 summon the vault warders
        if (!_vaultWardersAwake && me->GetHealthPct() <= 33.0f)
        {
            // Despawn the furniture
            if (Creature* target = me->GetMap()->GetCreature(_instance->GetGuidData(DATA64_VAULT_FURNITURE_1)))
                target->DespawnOrUnsummon();
            if (Creature* target = me->GetMap()->GetCreature(_instance->GetGuidData(DATA64_VAULT_FURNITURE_2)))
                target->DespawnOrUnsummon();
            // fix factions now or they'll look green for a brief moment
            for (uint32 dataId : { uint32(DATA64_VAULT_WARDER_1), uint32(DATA64_VAULT_WARDER_2) })
            {
                if (Creature* target = me->GetMap()->GetCreature(_instance->GetGuidData(dataId)))
                {
                    target->SetImmuneToPC(false);
                    target->SetImmuneToNPC(false);
                    target->SetUninteractible(false);
                    target->SetFaction(415);    // VMaNGOS SetFactionTemporary(415, TEMPFACTION_RESTORE_RESPAWN | TEMPFACTION_RESTORE_COMBAT_STOP)
                    target->CastSpell(target, SPELL_STONE_DWARF_AWAKEN, false);
                }
            }
            me->CastSpell(me, SPELL_AWAKEN_VAULT_WARDER, false);
            ClassicScriptText(SAY_SUMMON_2, me);
            _vaultWardersAwake = true;
        }

        if (_tremorTimer <= diff)
        {
            //Cast
            DoCastVictim(SPELL_GROUND_TREMOR);
            //45 seconds until we should cast this agian
            _tremorTimer = 45000;
        }
        else
            _tremorTimer -= diff;
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        if (Unit* target = me->SelectNearestHostileUnitInAggroRange(true))
        {
            if (!UnitIsOutside(target))
            {
                AttackStart(target);
                return;
            }
        }
        if (_instance)
            _instance->SetData(ULDAMAN_ENCOUNTER_ARCHAEDAS, FAIL);
        ScriptedAI::EnterEvadeMode(why);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (_instance)
            _instance->SetData(ULDAMAN_ENCOUNTER_ARCHAEDAS, DONE);
    }

private:
    InstanceScript* _instance;

    uint32 _tremorTimer;
    int32 _awakenTimer;
    uint32 _wallMinionTimer;
    uint32 _roomCheck;
    bool _wakingUp;
    bool _guardiansAwake;
    bool _vaultWardersAwake;
    bool _justCreated;
};

/* ScriptData
SDName: mob_archaedas_minions
SD%Complete: 100
SDComment: These mobs are initially frozen until Archaedas awakens them
one at a time.
EndScriptData */

struct classic_mob_archaedas_minions : public ScriptedAI
{
    classic_mob_archaedas_minions(Creature* creature) : ScriptedAI(creature)
    {
        _instance = creature->GetInstanceScript();
        Initialize();
    }

    void Initialize()
    {
        _arcingTimer = 3000;
        _trampleTimer = urand(4000, 10000);
        _reconstructTimer = urand(4000, 10000);
        _awakenTimer = 4000;
        _wakeSpellHit = false;
        _wokenUp = false;
        _wakingUp = false;
        _awake = false;
    }

    void Reset() override
    {
        Initialize();
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        Unit* target = me->SelectNearestHostileUnitInAggroRange(true);
        if (!target && _instance)
        {
            if (Unit* archaedas = ObjectAccessor::GetUnit(*me, _instance->GetGuidData(DATA64_ARCHAEDAS)))
                target = archaedas->GetVictim();
        }
        if (target)
            AttackStart(target);
    }

    void JoinCombat()
    {
        if (_instance)
            _instance->SetGuidData(SET_DATA64_UNFREEZE, me->GetGUID()); // unfreeze
        _awake = true;
        if (Unit* victim = me->SelectNearestTarget(80.0f))
            AttackStart(victim);
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        // time to wake up, start animation
        if (spellInfo->Id == SPELL_AWAKEN_EARTHEN_DWARF
            || spellInfo->Id == SPELL_AWAKEN_EARTHEN_GUARDIAN)
        {
            _wakeSpellHit = true;
            _wokenUp = true;
        }
        // SPELL_AWAKEN_VAULT_WARDER has 5s cast, wake when it lands
        else if (spellInfo->Id == SPELL_AWAKEN_VAULT_WARDER)
        {
            _wakeSpellHit = true;
            JoinCombat();
        }
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (_awake)
            ScriptedAI::MoveInLineOfSight(who);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!_instance || _instance->GetData(ULDAMAN_ENCOUNTER_ARCHAEDAS) != IN_PROGRESS)
        {
            if (_wakeSpellHit)
                Reset();
            return;
        }
        // 1st phase of wake, wait for awakening spell to land 1s
        if (_wokenUp && _awakenTimer <= (diff + 3000))
        {
            _wakingUp = true;
            me->CastSpell(me, SPELL_STONE_DWARF_AWAKEN, false);
            _wokenUp = false;
            _awakenTimer -= diff;
            return; // do nothing for 2s
        }
        // 2d phase of wake, wait for visual spell 3s
        else if (_wakingUp && _awakenTimer <= diff)
        {
            _wakingUp = false;
            // Wake him only if Archaedas is in combat
            if (_instance->GetData(ULDAMAN_ENCOUNTER_ARCHAEDAS) == IN_PROGRESS)
                JoinCombat();
            _awakenTimer = 4000;
            return; // dont want to continue until we finish the AttackStart method
        }
        else if (_wokenUp || _wakingUp)
        {
            _awakenTimer -= diff;
            return;
        }

        if (_awake && me->GetEntry() == NPC_EARTHEN_CUSTODIAN
            && _reconstructTimer <= diff)
        {
            if (Unit* archaedas = ObjectAccessor::GetUnit(*me, _instance->GetGuidData(DATA64_ARCHAEDAS)))
            {
                if (archaedas->GetHealthPct() < 50.0f)
                    DoCast(archaedas, SPELL_RECONSTRUCT);
            }
            _reconstructTimer = 10000;
        }
        else if (_awake)
            _reconstructTimer -= diff;

        //Return since we have no target
        if (!UpdateVictim())
            return;

        if (me->GetEntry() == NPC_VAULT_WARDER && _trampleTimer <= diff)
        {
            DoCastVictim(SPELL_TRAMPLE);
            _trampleTimer = 10000;
        }
        else
            _trampleTimer -= diff;
    }

private:
    uint32 _arcingTimer;
    uint32 _trampleTimer;
    uint32 _reconstructTimer;
    uint32 _awakenTimer;
    bool _wakeSpellHit;
    bool _wokenUp;
    bool _wakingUp;
    bool _awake;
    InstanceScript* _instance;
};

void AddSC_classic_boss_archaedas()
{
    RegisterCreatureAI(classic_boss_archaedas);
    RegisterCreatureAI(classic_mob_archaedas_minions);
}
