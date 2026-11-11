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

// Classic 1.60 (Forever): Krol'dok Stronghold (map 3039, Riverglades, levels 40-45). Encounters from the 70862
// DungeonEncounter.db2: 3450 Slavecrusher Grox, 3452 Twilight Commander Zarmok, 3453 Handler Ashka, 3449 Nar'gurk,
// 3451 Overseer Mugg (the creature is Overlord Mugg). Bossman Toof (264773) guards the way to Mugg with his shamans but has
// no encounter of his own.
//
// No sniff of this dungeon exists, and nobody has published its trash yet. What each boss does comes from the public
// dungeon guides: Grox has no special ability, Zarmok's hits bleed, Ashka fights with two wolves, Nar'gurk takes control
// of a player's mind, Mugg leaps around with two weapons and calls Krol'dok Centurions whose hits cut healing. The spells
// are from the 70862 client (Mugg's own Crushing Slam, Mugg's Fury, Set of Maces / Set of Mauls; the trash kit
// 1283361-1283425). Dominate Will and Dark Binding are server-side dummies on live, so Nar'gurk uses Dominate Mind (7645).
// Timers and damage values are INFERRED (picked for a level 40-45 group); a sniff of the fights should replace them.

#include "ScriptMgr.h"
#include "Creature.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "TemporarySummon.h"
#include "classic_dungeon_encounter_tracker.h"
#include <list>
#include <vector>

namespace
{
constexpr uint32 CLASSIC_KDS_MAP_ID = 3039;

enum ClassicKroldokBosses : uint32
{
    CKDS_ENC_GROX = 0,
    CKDS_ENC_ZARMOK = 1,
    CKDS_ENC_ASHKA = 2,
    CKDS_ENC_NARGURK = 3,
    CKDS_ENC_MUGG = 4,
    CKDS_ENC_MAX
};

enum ClassicKroldokCreatures : uint32
{
    NPC_KDS_GROX = 258966,
    NPC_KDS_ZARMOK = 258972,
    NPC_KDS_ASHKA = 259281,
    NPC_KDS_NARGURK = 258964,
    NPC_KDS_MUGG = 258968,
    NPC_KDS_TOOF = 264773,
    NPC_KDS_SHAMAN = 8110002,           // placeholder entries (no live ids known yet)
    NPC_KDS_CENTURION = 8110004,
    NPC_KDS_WAR_WOLF = 8110005
};

enum ClassicKroldokSpells : uint32
{
    // all present in the 70862 client
    SPELL_KDS_CLEAVE = 1283413,
    SPELL_KDS_WAR_STOMP = 1283425,
    SPELL_KDS_REND = 1283423,
    SPELL_KDS_MUSCLE_TEAR = 1283405,
    SPELL_KDS_NET = 1283364,
    SPELL_KDS_SHADOW_BOLT = 1283362,
    SPELL_KDS_DRAIN_BLOOD = 1283266,
    SPELL_KDS_DOMINATE_WILL = 1308318,  // live dummy, cast for the look
    SPELL_KDS_DOMINATE_MIND = 7645,     // the control itself
    SPELL_KDS_CRUSHING_SLAM = 1282397,  // damage and knock back where Mugg lands
    SPELL_KDS_MUGGS_FURY = 1282386,
    SPELL_KDS_SET_OF_MACES = 1282393,
    SPELL_KDS_SET_OF_MAULS = 1282394,
    SPELL_KDS_ENRAGE = 1284435
};

constexpr ClassicDungeonBossEntry ClassicKroldokBossEntries[] =
{
    { CKDS_ENC_GROX, NPC_KDS_GROX },
    { CKDS_ENC_ZARMOK, NPC_KDS_ZARMOK },
    { CKDS_ENC_ASHKA, NPC_KDS_ASHKA },
    { CKDS_ENC_NARGURK, NPC_KDS_NARGURK },
    { CKDS_ENC_MUGG, NPC_KDS_MUGG }
};

constexpr int32 KdsCrushingSlamDamage = 650;   // INFERRED

void KdsCallGuards(Creature* me, uint32 entry, float range, Unit* who)
{
    std::list<Creature*> guards;
    me->GetCreatureListWithEntryInGrid(guards, entry, range);
    for (Creature* guard : guards)
        if (guard->IsAlive() && !guard->IsEngaged() && who)
            guard->AI()->AttackStart(who);
}

// a boss whose whole fight is a few spells on timers
struct KdsTimedSpell
{
    uint32 Spell;
    bool OnVictim;          // false: on itself (an area spell around it)
    Milliseconds First;
    Milliseconds RepeatMin;
    Milliseconds RepeatMax;
};

class KdsTimedBossAI : public ScriptedAI
{
public:
    KdsTimedBossAI(Creature* creature, std::initializer_list<KdsTimedSpell> spells) : ScriptedAI(creature), _spells(spells) { }

    void Reset() override { _events.Reset(); }

    void JustEngagedWith(Unit* /*who*/) override
    {
        for (uint32 i = 0; i < _spells.size(); ++i)
            _events.ScheduleEvent(i + 1, _spells[i].First);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;
        _events.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 ev = _events.ExecuteEvent())
        {
            KdsTimedSpell const& spell = _spells[ev - 1];
            if (spell.OnVictim)
                DoCastVictim(spell.Spell);
            else
                DoCastSelf(spell.Spell);
            _events.Repeat(spell.RepeatMin, spell.RepeatMax);
        }
    }

protected:
    EventMap _events;
    std::vector<KdsTimedSpell> _spells;
};
}

class classic_instance_kroldok_stronghold : public InstanceMapScript
{
public:
    classic_instance_kroldok_stronghold() : InstanceMapScript("classic_instance_kroldok_stronghold", CLASSIC_KDS_MAP_ID) { }

    struct classic_instance_kroldok_stronghold_InstanceScript : public InstanceScript
    {
        classic_instance_kroldok_stronghold_InstanceScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders("CKDS");
            SetBossNumber(CKDS_ENC_MAX);
            static constexpr DungeonEncounterData encounters[] =
            {
                { CKDS_ENC_GROX, {{ 3450 }} },
                { CKDS_ENC_ZARMOK, {{ 3452 }} },
                { CKDS_ENC_ASHKA, {{ 3453 }} },
                { CKDS_ENC_NARGURK, {{ 3449 }} },
                { CKDS_ENC_MUGG, {{ 3451 }} }
            };
            LoadDungeonEncounterData(encounters);
        }

        ClassicDungeonEncounterTracker _encounterTracker{ *this, ClassicKroldokBossEntries };

        void OnCreatureCreate(Creature* creature) override
        {
            InstanceScript::OnCreatureCreate(creature);
            _encounterTracker.OnCreatureCreate(creature);
        }

        void OnCreatureRemove(Creature* creature) override
        {
            InstanceScript::OnCreatureRemove(creature);
            _encounterTracker.OnCreatureRemove(creature);
        }

        void OnUnitDeath(Unit* unit) override
        {
            _encounterTracker.OnUnitDeath(unit);
        }

        void Update(uint32 diff) override
        {
            _encounterTracker.Update(diff);
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new classic_instance_kroldok_stronghold_InstanceScript(map);
    }
};

// Slavecrusher Grox: no special ability in the guides; a heavy hitter with Cleave and War Stomp (INFERRED).
struct classic_boss_kds_grox : public KdsTimedBossAI
{
    classic_boss_kds_grox(Creature* creature) : KdsTimedBossAI(creature, {
        { SPELL_KDS_CLEAVE, true, 6s, 8s, 11s },
        { SPELL_KDS_WAR_STOMP, false, 14s, 20s, 25s } }) { }
};

// Twilight Commander Zarmok: the guides' heavy bleed (Rend and Muscle Tear on the tank) and Shadow Bolt.
struct classic_boss_kds_zarmok : public KdsTimedBossAI
{
    classic_boss_kds_zarmok(Creature* creature) : KdsTimedBossAI(creature, {
        { SPELL_KDS_REND, true, 3s, 12s, 15s },
        { SPELL_KDS_MUSCLE_TEAR, true, 7s, 9s, 12s },
        { SPELL_KDS_SHADOW_BOLT, true, 10s, 10s, 14s } }) { }
};

// War Handler Ashka: Net and Cleave; her two war wolves join the fight with her.
struct classic_boss_kds_ashka : public KdsTimedBossAI
{
    classic_boss_kds_ashka(Creature* creature) : KdsTimedBossAI(creature, {
        { SPELL_KDS_NET, true, 8s, 15s, 20s },
        { SPELL_KDS_CLEAVE, true, 5s, 8s, 11s } }) { }

    void JustEngagedWith(Unit* who) override
    {
        KdsTimedBossAI::JustEngagedWith(who);
        KdsCallGuards(me, NPC_KDS_WAR_WOLF, 25.0f, who);
    }
};

// Nar'gurk the Corruptor: takes control of a player's mind for a while (not the tank), Drain Blood and Shadow Bolt.
struct classic_boss_kds_nargurk : public ScriptedAI
{
    classic_boss_kds_nargurk(Creature* creature) : ScriptedAI(creature) { }

    enum : uint32 { EV_DOMINATE = 1, EV_DRAIN, EV_BOLT };

    void Reset() override { _events.Reset(); }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(EV_BOLT, 2s);             // INFERRED timers
        _events.ScheduleEvent(EV_DRAIN, 9s);
        _events.ScheduleEvent(EV_DOMINATE, 18s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;
        _events.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 ev = _events.ExecuteEvent())
        {
            switch (ev)
            {
                case EV_BOLT: DoCastVictim(SPELL_KDS_SHADOW_BOLT); _events.Repeat(4s, 6s); break;
                case EV_DRAIN:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 30.0f, true))
                        DoCast(target, SPELL_KDS_DRAIN_BLOOD);
                    _events.Repeat(15s, 18s);
                    break;
                case EV_DOMINATE:
                    // only with someone else left to fight; never the tank
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 1, 30.0f, true))
                    {
                        DoCast(target, SPELL_KDS_DOMINATE_WILL, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
                        DoCast(target, SPELL_KDS_DOMINATE_MIND, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
                    }
                    _events.Repeat(30s, 35s);
                    break;
                default: break;
            }
        }
    }

private:
    EventMap _events;
};

// Bossman Toof (after the bridge, no encounter of his own): War Stomp and Cleave, and his shamans join him.
struct classic_boss_kds_toof : public KdsTimedBossAI
{
    classic_boss_kds_toof(Creature* creature) : KdsTimedBossAI(creature, {
        { SPELL_KDS_CLEAVE, true, 5s, 8s, 11s },
        { SPELL_KDS_WAR_STOMP, false, 12s, 18s, 22s } }) { }

    void JustEngagedWith(Unit* who) override
    {
        KdsTimedBossAI::JustEngagedWith(who);
        KdsCallGuards(me, NPC_KDS_SHAMAN, 25.0f, who);
    }
};

// Overlord Mugg (top of the stronghold): fights with a set of maces, leaps onto a player (Crushing Slam: damage and a
// knock back where he lands), calls two Krol'dok Centurions at 70 and 35 percent, and at half health takes up his mauls
// in Mugg's Fury.
struct classic_boss_kds_mugg : public ScriptedAI
{
    classic_boss_kds_mugg(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    enum : uint32 { EV_LEAP = 1, EV_CLEAVE };
    static constexpr uint32 PointLeap = 1;

    void Reset() override
    {
        _events.Reset();
        _summons.DespawnAll();
        _calls = 0;
        _fury = false;
        me->RemoveAurasDueToSpell(SPELL_KDS_SET_OF_MAULS);
        me->RemoveAurasDueToSpell(SPELL_KDS_MUGGS_FURY);
        me->RemoveAurasDueToSpell(SPELL_KDS_ENRAGE);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoCastSelf(SPELL_KDS_SET_OF_MACES, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        _events.ScheduleEvent(EV_CLEAVE, 6s);           // INFERRED timers
        _events.ScheduleEvent(EV_LEAP, 15s);
    }

    void JustSummoned(Creature* summon) override
    {
        _summons.Summon(summon);
        if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true))
            summon->AI()->AttackStart(target);
    }

    void JustDied(Unit* /*killer*/) override { _summons.DespawnAll(); }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellInfo const* /*spellInfo*/) override
    {
        if ((_calls == 0 && me->HealthBelowPctDamaged(70, damage)) || (_calls == 1 && me->HealthBelowPctDamaged(35, damage)))
        {
            ++_calls;
            for (int i = 0; i < 2; ++i)
                me->SummonCreature(NPC_KDS_CENTURION, me->GetRandomNearPosition(10.0f), TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10s);
        }
        if (!_fury && me->HealthBelowPctDamaged(50, damage))
        {
            _fury = true;
            me->RemoveAurasDueToSpell(SPELL_KDS_SET_OF_MACES);
            DoCastSelf(SPELL_KDS_SET_OF_MAULS, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
            DoCastSelf(SPELL_KDS_MUGGS_FURY, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
            DoCastSelf(SPELL_KDS_ENRAGE, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != EFFECT_MOTION_TYPE || id != PointLeap)
            return;
        CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
        args.AddSpellBP0(KdsCrushingSlamDamage);
        me->CastSpell(me->GetPosition(), SPELL_KDS_CRUSHING_SLAM, args);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;
        _events.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_JUMPING))
            return;

        while (uint32 ev = _events.ExecuteEvent())
        {
            switch (ev)
            {
                case EV_CLEAVE: DoCastVictim(SPELL_KDS_CLEAVE); _events.Repeat(7s, 10s); break;
                case EV_LEAP:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 40.0f, true))
                        me->GetMotionMaster()->MoveJump(PointLeap, target->GetPosition(), 25.0f, {}, 8.0f);
                    _events.Repeat(18s, 24s);
                    break;
                default: break;
            }
        }
    }

private:
    EventMap _events;
    SummonList _summons;
    uint32 _calls = 0;
    bool _fury = false;
};

void AddSC_classic_instance_kroldok_stronghold()
{
    new classic_instance_kroldok_stronghold();
    RegisterCreatureAI(classic_boss_kds_grox);
    RegisterCreatureAI(classic_boss_kds_zarmok);
    RegisterCreatureAI(classic_boss_kds_ashka);
    RegisterCreatureAI(classic_boss_kds_nargurk);
    RegisterCreatureAI(classic_boss_kds_toof);
    RegisterCreatureAI(classic_boss_kds_mugg);
}
