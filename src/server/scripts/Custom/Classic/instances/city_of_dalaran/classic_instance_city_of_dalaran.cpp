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

// Classic 1.60 (Forever): City of Dalaran (map 2959, levels 28-33). Spawns and loot were already in the world DB; this file adds
// the instance, the boss fights, the way in and the way to the last boss. Encounters from the 70862 DungeonEncounter.db2:
// 3311 Atrexis the Grave Knight, 3310 Lyn the Ignored, 3298 Arcane Anomaly, 3301 Arcanic Enigma (the client calls the
// encounter "Mana Elemental"), 3302 Unstable Sentinel, 3300 Mana Devourer, 3299 Fel Ancient, 3303 Shade of the Archmage.
//
// No sniff of any fight exists. The abilities come from the public dungeon guides (Forever 1.60.1). Live uses spells the client
// does not ship (Silence 1224125, Arcane Bolt 15230, Arcane Explosion 28450, Evocation 1303599) or spells whose effect is
// server-side (Focal Blast area trigger 38353, Bounding Mana and Fel Timbers are dummies, Malfunction pulses). Those are
// rebuilt here from spells the 70862 client has, with damage values picked for a level 28-33 group. Every timer and
// every number marked INFERRED below is a guess, not live data.
//
// The way in: the Sewer Grate (656283) outside the city, map 0 (388.8, 537.4, 45.8). It needs the Dalaran Sewer Key
// (277507); one key opens it for the whole group (guides), so a group member near the grate with the key is enough. Ghosts can
// always go through, for the corpse run. The landing spot is the sewer start next to Rexxie Copperclutch (INFERRED: the
// live landing spot is not known; this one is on the same navmesh as every boss, checked offline).
//
// The way to the last boss: the Portal To the Violet Chamber (656243) casts 1297791, which the client does not have, so the
// portal did nothing. It now takes the player up to the Purple Parlor once Mana Devourer and Fel Ancient are dead, and the
// portal in the Parlor (656320, spell 1297938, also missing) takes them back down.
//
// Fel Ancient stands behind Prismatic Doors (555247). Live opens them through Grindle Firespark, who has no spawn in our
// data, so the doors open when the three bosses of the city squares (Arcane Anomaly, Arcanic Enigma, Unstable Sentinel) are
// dead (INFERRED).

#include "ScriptMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "Chat.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "TemporarySummon.h"
#include "classic_dungeon_encounter_tracker.h"
#include <vector>

namespace
{
constexpr uint32 CLASSIC_COD_MAP_ID = 2959;

enum ClassicCityOfDalaranBosses : uint32
{
    CCOD_ENC_ATREXIS = 0,
    CCOD_ENC_LYN = 1,
    CCOD_ENC_ARCANE_ANOMALY = 2,
    CCOD_ENC_ARCANIC_ENIGMA = 3,
    CCOD_ENC_UNSTABLE_SENTINEL = 4,
    CCOD_ENC_MANA_DEVOURER = 5,
    CCOD_ENC_FEL_ANCIENT = 6,
    CCOD_ENC_SHADE = 7,
    CCOD_ENC_MAX
};

enum ClassicCityOfDalaranCreatures : uint32
{
    NPC_COD_ARCANE_ANOMALY = 245999,
    NPC_COD_FEL_ANCIENT = 246003,
    NPC_COD_MANA_DEVOURER = 246008,
    NPC_COD_ARCANIC_ENIGMA = 246016,
    NPC_COD_UNSTABLE_SENTINEL = 246017,
    NPC_COD_SHADE = 246020,
    NPC_COD_LYN = 247032,
    NPC_COD_ATREXIS = 247126,
    NPC_COD_ARCANE_MANALING = 246890,   // the live Summon Manaling creature 269467 has no template; this is the dungeon's own Arcane Manaling
    NPC_COD_RISEN_TERROR = 255065       // Raise Dead adds: the sewer undead of the same dungeon (INFERRED)
};

enum ClassicCityOfDalaranObjects : uint32
{
    GO_COD_PRISMATIC_DOOR_FEL_ANCIENT = 555247,
    GO_COD_FEL_CRACKS = 553754,
    GO_COD_SEWER_GRATE = 656283,
    GO_COD_PORTAL_TO_PARLOR = 656243,
    GO_COD_PORTAL_FROM_PARLOR = 656320,
    GO_COD_CITY_DOOR = 665036
};

enum ClassicCityOfDalaranItems : uint32
{
    ITEM_COD_DALARAN_SEWER_KEY = 277507
};

enum ClassicCityOfDalaranSpells : uint32
{
    // all present in the 70862 client
    SPELL_COD_DISARM = 676,
    SPELL_COD_STRIKE = 11976,
    SPELL_COD_THRASH = 3391,
    SPELL_COD_UNHOLY_FRENZY = 8699,
    SPELL_COD_SHADOW_BOLT_VOLLEY = 9081,
    SPELL_COD_CURSE_OF_THORNS = 6909,
    SPELL_COD_ARCANE_BOLT = 13748,
    SPELL_COD_FOCAL_BLAST = 1241501,        // live spell; its area trigger is server-side, so it is cast for the look only
    SPELL_COD_MANAMORPH = 1240869,
    SPELL_COD_SILENCE = 6726,
    SPELL_COD_SUMMON_MANALING = 1304455,    // cast for the look; its creature 269467 has no template
    SPELL_COD_ARCING_SMASH = 8374,
    SPELL_COD_MALFUNCTION = 13319,
    SPELL_COD_ENTANGLE = 720,
    SPELL_COD_FEL_TIMBERS = 1240692,        // live dummy, cast for the look
    SPELL_COD_FELFIRE = 24530,
    SPELL_COD_DRAIN_VITALITY = 1240738,
    SPELL_COD_ARCANE_CASCADE = 1241359,     // 40 arcane damage and 200 mana burned in a cone: the guides' "Debilitate"
    SPELL_COD_BOUNDING_MANA = 1241515,      // live dummy, cast for the look
    SPELL_COD_MASS_POLYMORPH = 1303598,
    SPELL_COD_EVOCATION = 12051
};

// INFERRED landing spots (no sniff): on the navmesh, each one checked with dungeon_reach_check
constexpr Position CodSewerLanding = { 309.8f, 384.0f, 67.5f, 3.90f };
constexpr Position CodParlorLanding = { 440.0f, 360.0f, 294.0f, 6.05f };
constexpr Position CodParlorExitLanding = { 424.0f, 372.0f, 129.1f, 3.80f };

constexpr float CodGrateKeyRange = 40.0f;
constexpr float CodShadeRoomRadius = 32.0f;         // the Parlor is about 48 x 41 yd; leaving it resets the Shade (guides)

constexpr ClassicDungeonBossEntry ClassicCityOfDalaranBossEntries[] =
{
    { CCOD_ENC_ATREXIS, NPC_COD_ATREXIS },
    { CCOD_ENC_LYN, NPC_COD_LYN },
    { CCOD_ENC_ARCANE_ANOMALY, NPC_COD_ARCANE_ANOMALY },
    { CCOD_ENC_ARCANIC_ENIGMA, NPC_COD_ARCANIC_ENIGMA },
    { CCOD_ENC_UNSTABLE_SENTINEL, NPC_COD_UNSTABLE_SENTINEL },
    { CCOD_ENC_MANA_DEVOURER, NPC_COD_MANA_DEVOURER },
    { CCOD_ENC_FEL_ANCIENT, NPC_COD_FEL_ANCIENT },
    { CCOD_ENC_SHADE, NPC_COD_SHADE }
};

// players of the instance within range of a point, alive and attackable by the caster
std::vector<Player*> CodPlayersNear(Creature* me, Position const& pos, float range)
{
    std::vector<Player*> out;
    for (MapReference const& ref : me->GetMap()->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (player && player->IsAlive() && !player->IsGameMaster() && player->GetExactDist(&pos) <= range
            && me->IsValidAttackTarget(player))
            out.push_back(player);
    }
    return out;
}

// a damage spell from the client with our own damage value, so the hit shows in the combat log under that spell's name
void CodHit(Creature* me, Unit* target, uint32 spellId, int32 damage)
{
    CastSpellExtraArgs args(TRIGGERED_FULL_MASK);
    args.AddSpellBP0(damage);
    me->CastSpell(target, spellId, args);
}

// a raised or summoned add joins the fight at once
void CodAddJoins(Creature* me, TempSummon* add)
{
    if (!add)
        return;
    if (Unit* target = me->GetAI()->SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true))
        add->AI()->AttackStart(target);
}
}

class classic_instance_city_of_dalaran : public InstanceMapScript
{
public:
    classic_instance_city_of_dalaran() : InstanceMapScript("classic_instance_city_of_dalaran", CLASSIC_COD_MAP_ID) { }

    struct classic_instance_city_of_dalaran_InstanceScript : public InstanceScript
    {
        classic_instance_city_of_dalaran_InstanceScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders("CCOD");
            SetBossNumber(CCOD_ENC_MAX);
            static constexpr DungeonEncounterData encounters[] =
            {
                { CCOD_ENC_ATREXIS, {{ 3311 }} },
                { CCOD_ENC_LYN, {{ 3310 }} },
                { CCOD_ENC_ARCANE_ANOMALY, {{ 3298 }} },
                { CCOD_ENC_ARCANIC_ENIGMA, {{ 3301 }} },
                { CCOD_ENC_UNSTABLE_SENTINEL, {{ 3302 }} },
                { CCOD_ENC_MANA_DEVOURER, {{ 3300 }} },
                { CCOD_ENC_FEL_ANCIENT, {{ 3299 }} },
                { CCOD_ENC_SHADE, {{ 3303 }} }
            };
            LoadDungeonEncounterData(encounters);
        }

        ClassicDungeonEncounterTracker _encounterTracker{ *this, ClassicCityOfDalaranBossEntries };
        std::vector<ObjectGuid> _felAncientDoors;
        std::vector<ObjectGuid> _cityDoors;

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

        void OnGameObjectCreate(GameObject* go) override
        {
            InstanceScript::OnGameObjectCreate(go);
            if (go->GetEntry() == GO_COD_PRISMATIC_DOOR_FEL_ANCIENT)
            {
                _felAncientDoors.push_back(go->GetGUID());
                UpdateFelAncientDoors();
            }
            else if (go->GetEntry() == GO_COD_CITY_DOOR)
            {
                _cityDoors.push_back(go->GetGUID());
                UpdateCityDoors();
            }
        }

        void OnUnitDeath(Unit* unit) override
        {
            _encounterTracker.OnUnitDeath(unit);
        }

        void Update(uint32 diff) override
        {
            _encounterTracker.Update(diff);
        }

        bool SetBossState(uint32 id, EncounterState state) override
        {
            if (!InstanceScript::SetBossState(id, state))
                return false;
            if (state == DONE)
            {
                UpdateFelAncientDoors();
                UpdateCityDoors();
            }
            return true;
        }

        // The round door at the top of the sewer hill (665036) leads from the Underbelly up to the city: it opens once both sewer
        // bosses, Atrexis and Lyn, are dead (INFERRED: no live data on what opens it; the guides do the sewers first).
        bool SewerBossesDone() const
        {
            return GetBossState(CCOD_ENC_ATREXIS) == DONE && GetBossState(CCOD_ENC_LYN) == DONE;
        }

        void UpdateCityDoors()
        {
            if (!SewerBossesDone())
                return;
            for (ObjectGuid const& guid : _cityDoors)
                if (GameObject* door = instance->GetGameObject(guid))
                    door->SetGoState(GO_STATE_ACTIVE);
        }

        bool SquareBossesDone() const
        {
            return GetBossState(CCOD_ENC_ARCANE_ANOMALY) == DONE && GetBossState(CCOD_ENC_ARCANIC_ENIGMA) == DONE
                && GetBossState(CCOD_ENC_UNSTABLE_SENTINEL) == DONE;
        }

        void UpdateFelAncientDoors()
        {
            if (!SquareBossesDone())
                return;
            for (ObjectGuid const& guid : _felAncientDoors)
                if (GameObject* door = instance->GetGameObject(guid))
                    door->DespawnOrUnsummon();
            _felAncientDoors.clear();
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new classic_instance_city_of_dalaran_InstanceScript(map);
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Atrexis the Grave Knight (sewers, first boss): Disarm on the tank, Strike, Thrash, Unholy Frenzy, Raise Dead (adds).
struct classic_boss_cod_atrexis : public ScriptedAI
{
    classic_boss_cod_atrexis(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    enum : uint32 { EV_DISARM = 1, EV_STRIKE, EV_THRASH, EV_FRENZY, EV_RAISE_DEAD };
    static constexpr uint32 MaxRaised = 6;          // INFERRED cap, so the adds cannot be farmed

    void Reset() override
    {
        _events.Reset();
        _summons.DespawnAll();
        _raised = 0;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(EV_STRIKE, 5s);           // INFERRED timers
        _events.ScheduleEvent(EV_DISARM, 12s);
        _events.ScheduleEvent(EV_THRASH, 8s);
        _events.ScheduleEvent(EV_FRENZY, 15s);
        _events.ScheduleEvent(EV_RAISE_DEAD, 10s);
    }

    void JustSummoned(Creature* summon) override { _summons.Summon(summon); }
    void JustDied(Unit* /*killer*/) override { _summons.DespawnAll(); }

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
                case EV_STRIKE: DoCastVictim(SPELL_COD_STRIKE); _events.Repeat(6s, 9s); break;
                case EV_DISARM: DoCastVictim(SPELL_COD_DISARM); _events.Repeat(20s, 25s); break;
                case EV_THRASH: DoCastSelf(SPELL_COD_THRASH); _events.Repeat(10s, 14s); break;
                case EV_FRENZY: DoCastSelf(SPELL_COD_UNHOLY_FRENZY); _events.Repeat(25s, 30s); break;
                case EV_RAISE_DEAD:
                    if (_raised < MaxRaised)
                    {
                        for (int i = 0; i < 2 && _raised < MaxRaised; ++i, ++_raised)
                        {
                            Position pos = me->GetRandomNearPosition(6.0f);
                            CodAddJoins(me, me->SummonCreature(NPC_COD_RISEN_TERROR, pos, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10s));
                        }
                    }
                    _events.Repeat(18s, 22s);
                    break;
                default: break;
            }
        }
    }

private:
    EventMap _events;
    SummonList _summons;
    uint32 _raised = 0;
};

// Lyn the Ignored (optional, below Atrexis): Raise Dead, Shadow Bolt Volley, Curse of Thorns on the tank.
struct classic_boss_cod_lyn : public ScriptedAI
{
    classic_boss_cod_lyn(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    enum : uint32 { EV_VOLLEY = 1, EV_THORNS, EV_RAISE_DEAD };
    static constexpr uint32 MaxRaised = 6;

    void Reset() override
    {
        _events.Reset();
        _summons.DespawnAll();
        _raised = 0;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(EV_THORNS, 3s);           // INFERRED timers
        _events.ScheduleEvent(EV_VOLLEY, 9s);
        _events.ScheduleEvent(EV_RAISE_DEAD, 6s);
    }

    void JustSummoned(Creature* summon) override { _summons.Summon(summon); }
    void JustDied(Unit* /*killer*/) override { _summons.DespawnAll(); }

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
                case EV_THORNS: DoCastVictim(SPELL_COD_CURSE_OF_THORNS); _events.Repeat(20s); break;
                case EV_VOLLEY: DoCastSelf(SPELL_COD_SHADOW_BOLT_VOLLEY); _events.Repeat(12s, 15s); break;
                case EV_RAISE_DEAD:
                    if (_raised < MaxRaised)
                    {
                        Position pos = me->GetRandomNearPosition(6.0f);
                        CodAddJoins(me, me->SummonCreature(NPC_COD_RISEN_TERROR, pos, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10s));
                        ++_raised;
                    }
                    _events.Repeat(12s, 15s);
                    break;
                default: break;
            }
        }
    }

private:
    EventMap _events;
    SummonList _summons;
    uint32 _raised = 0;
};

// Arcane Anomaly (Krasus' Landing): Arcane Bolt on the tank (about 500), Focal Blast: a 3 s beam that sweeps clockwise or
// counterclockwise; whoever stands in it is hit.
struct classic_boss_cod_arcane_anomaly : public ScriptedAI
{
    classic_boss_cod_arcane_anomaly(Creature* creature) : ScriptedAI(creature) { }

    enum : uint32 { EV_BOLT = 1, EV_FOCAL_BLAST, EV_FOCAL_TICK };
    static constexpr int32 BoltDamage = 500;        // guides: about 500 arcane on the tank
    static constexpr int32 BeamDamage = 300;        // INFERRED, per 0.5 s tick
    static constexpr float BeamRange = 30.0f;       // INFERRED
    static constexpr float BeamStep = 0.35f;        // radians per tick: about 120 degrees over the 3 s

    void Reset() override
    {
        _events.Reset();
        _ticks = 0;
        me->SetReactState(REACT_AGGRESSIVE);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(EV_BOLT, 4s);
        _events.ScheduleEvent(EV_FOCAL_BLAST, 15s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;
        _events.Update(diff);

        while (uint32 ev = _events.ExecuteEvent())
        {
            switch (ev)
            {
                case EV_BOLT:
                    if (_ticks == 0)
                        if (Unit* victim = me->GetVictim())
                            CodHit(me, victim, SPELL_COD_ARCANE_BOLT, BoltDamage);
                    _events.Repeat(6s, 8s);
                    break;
                case EV_FOCAL_BLAST:
                {
                    Unit* target = SelectTarget(SelectTargetMethod::Random, 0, BeamRange, true);
                    if (!target)
                        target = me->GetVictim();
                    me->SetReactState(REACT_PASSIVE);
                    me->AttackStop();
                    me->StopMoving();
                    me->SetFacingToObject(target);
                    _beamFacing = me->GetAbsoluteAngle(target);
                    _beamDirection = urand(0, 1) ? 1.0f : -1.0f;
                    DoCastSelf(SPELL_COD_FOCAL_BLAST, true);
                    _ticks = 6;
                    _events.ScheduleEvent(EV_FOCAL_TICK, 500ms);
                    _events.Repeat(18s, 22s);
                    break;
                }
                case EV_FOCAL_TICK:
                    if (_ticks > 0)
                    {
                        _beamFacing = Position::NormalizeOrientation(_beamFacing + _beamDirection * BeamStep);
                        me->SetFacingTo(_beamFacing);
                        Position const front = me->GetPosition();
                        for (Player* player : CodPlayersNear(me, front, BeamRange))
                        {
                            float const diffAngle = Position::NormalizeOrientation(me->GetAbsoluteAngle(player) - _beamFacing);
                            if (diffAngle < 0.26f || diffAngle > 2 * float(M_PI) - 0.26f)   // 30 degree beam
                                CodHit(me, player, SPELL_COD_ARCANE_BOLT, BeamDamage);
                        }
                        if (--_ticks > 0)
                            _events.ScheduleEvent(EV_FOCAL_TICK, 500ms);
                        else
                        {
                            me->SetReactState(REACT_AGGRESSIVE);
                            if (Unit* victim = me->GetVictim())
                                AttackStart(victim);
                        }
                    }
                    break;
                default: break;
            }
        }
    }

private:
    EventMap _events;
    uint32 _ticks = 0;
    float _beamFacing = 0.0f;
    float _beamDirection = 1.0f;
};

// Arcanic Enigma (southern garden, two Mana Phantoms with it): Manamorph on a random player (dispellable), Silence on the
// players near it, Summon Manaling (adds).
struct classic_boss_cod_arcanic_enigma : public ScriptedAI
{
    classic_boss_cod_arcanic_enigma(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    enum : uint32 { EV_MANAMORPH = 1, EV_SILENCE, EV_MANALING };
    static constexpr float SilenceRange = 10.0f;    // INFERRED: "ranged players can avoid it"
    static constexpr uint32 MaxManalings = 6;

    void Reset() override
    {
        _events.Reset();
        _summons.DespawnAll();
        _manalings = 0;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        me->CallForHelp(20.0f);                     // the two Mana Phantoms next to it
        _events.ScheduleEvent(EV_MANAMORPH, 8s);    // INFERRED timers
        _events.ScheduleEvent(EV_SILENCE, 12s);
        _events.ScheduleEvent(EV_MANALING, 15s);
    }

    void JustSummoned(Creature* summon) override { _summons.Summon(summon); }
    void JustDied(Unit* /*killer*/) override { _summons.DespawnAll(); }

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
                case EV_MANAMORPH:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 1, 40.0f, true))
                        DoCast(target, SPELL_COD_MANAMORPH);
                    else
                        DoCastVictim(SPELL_COD_MANAMORPH);
                    _events.Repeat(15s, 18s);
                    break;
                case EV_SILENCE:
                    for (Player* player : CodPlayersNear(me, me->GetPosition(), SilenceRange))
                        me->CastSpell(player, SPELL_COD_SILENCE, true);
                    _events.Repeat(18s, 22s);
                    break;
                case EV_MANALING:
                    DoCastSelf(SPELL_COD_SUMMON_MANALING, true);
                    for (int i = 0; i < 2 && _manalings < MaxManalings; ++i, ++_manalings)
                    {
                        Position pos = me->GetRandomNearPosition(5.0f);
                        CodAddJoins(me, me->SummonCreature(NPC_COD_ARCANE_MANALING, pos, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10s));
                    }
                    _events.Repeat(20s, 25s);
                    break;
                default: break;
            }
        }
    }

private:
    EventMap _events;
    SummonList _summons;
    uint32 _manalings = 0;
};

// Unstable Sentinel (trade district): Arcing Smash (frontal cleave), Malfunction: pulses within 25 yd; a stun before it starts
// stops it.
struct classic_boss_cod_unstable_sentinel : public ScriptedAI
{
    classic_boss_cod_unstable_sentinel(Creature* creature) : ScriptedAI(creature) { }

    enum : uint32 { EV_SMASH = 1, EV_MALFUNCTION, EV_MALFUNCTION_START, EV_MALFUNCTION_PULSE };
    static constexpr float MalfunctionRange = 25.0f;   // guides
    static constexpr int32 PulseDamage = 150;          // INFERRED, per pulse
    static constexpr uint32 Pulses = 5;                // INFERRED

    void Reset() override
    {
        _events.Reset();
        _pulses = 0;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(EV_SMASH, 6s);
        _events.ScheduleEvent(EV_MALFUNCTION, 18s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;
        _events.Update(diff);

        while (uint32 ev = _events.ExecuteEvent())
        {
            switch (ev)
            {
                case EV_SMASH:
                    if (_pulses == 0 && !me->HasUnitState(UNIT_STATE_CASTING))
                        DoCastVictim(SPELL_COD_ARCING_SMASH);
                    _events.Repeat(8s, 11s);
                    break;
                case EV_MALFUNCTION:
                    me->TextEmote("Unstable Sentinel begins to malfunction!", nullptr, true);
                    _events.ScheduleEvent(EV_MALFUNCTION_START, 2s);    // the window to stun it
                    _events.Repeat(22s, 26s);
                    break;
                case EV_MALFUNCTION_START:
                    if (!me->HasUnitState(UNIT_STATE_STUNNED))
                    {
                        _pulses = Pulses;
                        _events.ScheduleEvent(EV_MALFUNCTION_PULSE, 0s);
                    }
                    break;
                case EV_MALFUNCTION_PULSE:
                    if (_pulses > 0)
                    {
                        for (Player* player : CodPlayersNear(me, me->GetPosition(), MalfunctionRange))
                            CodHit(me, player, SPELL_COD_MALFUNCTION, PulseDamage);
                        if (--_pulses > 0)
                            _events.ScheduleEvent(EV_MALFUNCTION_PULSE, 1s);
                    }
                    break;
                default: break;
            }
        }
    }

private:
    EventMap _events;
    uint32 _pulses = 0;
};

// Mana Devourer (by the Violet Hold): Drain Vitality on the tank, Debilitate (arcane damage and mana burn on the group).
// Everything left alive in its room joins the fight.
struct classic_boss_cod_mana_devourer : public ScriptedAI
{
    classic_boss_cod_mana_devourer(Creature* creature) : ScriptedAI(creature) { }

    enum : uint32 { EV_DRAIN = 1, EV_DEBILITATE };

    void Reset() override { _events.Reset(); }

    void JustEngagedWith(Unit* /*who*/) override
    {
        me->CallForHelp(35.0f);                     // guides: clear the room first
        _events.ScheduleEvent(EV_DRAIN, 5s);        // INFERRED timers
        _events.ScheduleEvent(EV_DEBILITATE, 10s);
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
                case EV_DRAIN: DoCastVictim(SPELL_COD_DRAIN_VITALITY); _events.Repeat(10s, 13s); break;
                case EV_DEBILITATE:
                    for (Player* player : CodPlayersNear(me, me->GetPosition(), 40.0f))
                        me->CastSpell(player, SPELL_COD_ARCANE_CASCADE, true);
                    _events.Repeat(14s, 18s);
                    break;
                default: break;
            }
        }
    }

private:
    EventMap _events;
};

// Fel Ancient (city centre): Entangle (10 s root with damage), Fel Timbers: green pools that burn whoever stands in them.
struct classic_boss_cod_fel_ancient : public ScriptedAI
{
    classic_boss_cod_fel_ancient(Creature* creature) : ScriptedAI(creature) { }

    enum : uint32 { EV_ENTANGLE = 1, EV_FEL_TIMBERS, EV_POOL_TICK };
    static constexpr float PoolRadius = 4.0f;       // INFERRED
    static constexpr int32 PoolDamage = 90;         // INFERRED, per second
    static constexpr Seconds PoolDuration = 15s;    // INFERRED

    struct Pool { Position pos; uint32 ticksLeft; };

    void Reset() override
    {
        _events.Reset();
        _pools.clear();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(EV_ENTANGLE, 10s);
        _events.ScheduleEvent(EV_FEL_TIMBERS, 7s);
        _events.ScheduleEvent(EV_POOL_TICK, 1s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;
        _events.Update(diff);

        while (uint32 ev = _events.ExecuteEvent())
        {
            switch (ev)
            {
                case EV_ENTANGLE:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 40.0f, true))
                        me->CastSpell(target, SPELL_COD_ENTANGLE, true);
                    _events.Repeat(15s, 20s);
                    break;
                case EV_FEL_TIMBERS:
                    DoCastSelf(SPELL_COD_FEL_TIMBERS, true);
                    for (int i = 0; i < 2; ++i)
                        if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 40.0f, true))
                        {
                            Position pos = target->GetPosition();
                            me->SummonGameObject(GO_COD_FEL_CRACKS, pos, QuaternionData::fromEulerAnglesZYX(pos.GetOrientation(), 0.0f, 0.0f),
                                PoolDuration);
                            _pools.push_back({ pos, uint32(PoolDuration.count()) });
                        }
                    _events.Repeat(12s, 15s);
                    break;
                case EV_POOL_TICK:
                    for (Pool& pool : _pools)
                    {
                        for (Player* player : CodPlayersNear(me, pool.pos, PoolRadius))
                            CodHit(me, player, SPELL_COD_FELFIRE, PoolDamage);
                        --pool.ticksLeft;
                    }
                    std::erase_if(_pools, [](Pool const& pool) { return pool.ticksLeft == 0; });
                    _events.Repeat(1s);
                    break;
                default: break;
            }
        }
    }

private:
    EventMap _events;
    std::vector<Pool> _pools;
};

// Shade of the Archmage (Purple Parlor, last boss): Arcane Bolt on the tank, Bounding Mana: a sphere goes out to a player and
// back, then Arcane Explosion hits everyone within 20 yd of where the Shade stood; when its mana runs out it casts Mass
// Polymorph on the group and Evocation, then the cycle starts again. Leaving the room resets it.
struct classic_boss_cod_shade_of_the_archmage : public ScriptedAI
{
    classic_boss_cod_shade_of_the_archmage(Creature* creature) : ScriptedAI(creature) { }

    enum : uint32 { EV_BOLT = 1, EV_BOUNDING_MANA, EV_EXPLOSION, EV_OUT_OF_MANA, EV_EVOCATION, EV_LEASH };
    static constexpr int32 BoltDamage = 600;            // INFERRED
    static constexpr int32 ExplosionDamage = 700;       // INFERRED
    static constexpr float ExplosionRange = 20.0f;      // guides
    static constexpr Seconds ManaCycle = 50s;           // INFERRED: how long its mana lasts

    void Reset() override
    {
        _events.Reset();
        _polymorphing = false;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(EV_BOLT, 3s);
        _events.ScheduleEvent(EV_BOUNDING_MANA, 12s);
        _events.ScheduleEvent(EV_OUT_OF_MANA, ManaCycle);
        _events.ScheduleEvent(EV_LEASH, 2s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;
        _events.Update(diff);

        while (uint32 ev = _events.ExecuteEvent())
        {
            switch (ev)
            {
                case EV_LEASH:
                    if (me->GetExactDist2d(me->GetHomePosition()) > CodShadeRoomRadius)
                    {
                        EnterEvadeMode(EvadeReason::Boundary);
                        return;
                    }
                    _events.Repeat(2s);
                    break;
                case EV_BOLT:
                    if (!_polymorphing)
                        if (Unit* victim = me->GetVictim())
                            CodHit(me, victim, SPELL_COD_ARCANE_BOLT, BoltDamage);
                    _events.Repeat(4s, 6s);
                    break;
                case EV_BOUNDING_MANA:
                    if (!_polymorphing)
                    {
                        if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 40.0f, true))
                            me->CastSpell(target, SPELL_COD_BOUNDING_MANA, true);
                        _explosionAt = me->GetPosition();
                        _events.ScheduleEvent(EV_EXPLOSION, 6s);   // INFERRED: the sphere's trip out and back
                    }
                    _events.Repeat(20s, 24s);
                    break;
                case EV_EXPLOSION:
                    for (Player* player : CodPlayersNear(me, _explosionAt, ExplosionRange))
                        CodHit(me, player, SPELL_COD_ARCANE_BOLT, ExplosionDamage);
                    break;
                case EV_OUT_OF_MANA:
                    _polymorphing = true;
                    me->TextEmote("Shade of the Archmage is out of mana!", nullptr, true);
                    DoCastSelf(SPELL_COD_MASS_POLYMORPH, true);
                    _events.ScheduleEvent(EV_EVOCATION, 1s);
                    break;
                case EV_EVOCATION:
                    DoCastSelf(SPELL_COD_EVOCATION);
                    _polymorphing = false;
                    _events.ScheduleEvent(EV_OUT_OF_MANA, ManaCycle);
                    break;
                default: break;
            }
        }
    }

private:
    EventMap _events;
    Position _explosionAt;
    bool _polymorphing = false;
};

// ---------------------------------------------------------------------------------------------------------------------
// A line in the player's chat, Russian for a Russian client.
static void CodSay(Player* player, char const* en, char const* ru)
{
    ChatHandler(player->GetSession()).SendSysMessage(player->GetSession()->GetSessionDbcLocale() == LOCALE_ruRU ? ru : en);
}

// Sewer Grate (656283, map 0): the way into the dungeon.
struct classic_go_cod_sewer_grate : public GameObjectAI
{
    classic_go_cod_sewer_grate(GameObject* go) : GameObjectAI(go) { }

    static bool HasKeyNear(Player* player)
    {
        if (player->HasItemCount(ITEM_COD_DALARAN_SEWER_KEY))
            return true;
        if (Group* group = player->GetGroup())
            for (GroupReference const& ref : group->GetMembers())
                if (Player* member = ref.GetSource())
                    if (member != player && member->IsInMap(player) && member->GetExactDist(player) <= CodGrateKeyRange
                        && member->HasItemCount(ITEM_COD_DALARAN_SEWER_KEY))
                        return true;
        return false;
    }

    bool OnGossipHello(Player* player) override
    {
        if (!player->IsGameMaster() && player->IsAlive() && !HasKeyNear(player))
        {
            CodSay(player, "The grate is locked. You need the Dalaran Sewer Key.", "Решётка заперта. Нужен ключ от канализации Даларана.");
            return true;
        }

        player->TeleportTo(CLASSIC_COD_MAP_ID, CodSewerLanding.GetPositionX(), CodSewerLanding.GetPositionY(),
            CodSewerLanding.GetPositionZ(), CodSewerLanding.GetOrientation());
        return true;
    }
};

// Portal To the Violet Chamber (656243): up to the Purple Parlor, once Mana Devourer and Fel Ancient are dead.
struct classic_go_cod_portal_to_parlor : public GameObjectAI
{
    classic_go_cod_portal_to_parlor(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        InstanceScript* instance = me->GetInstanceScript();
        bool const open = player->IsGameMaster() || (instance && instance->GetBossState(CCOD_ENC_MANA_DEVOURER) == DONE
            && instance->GetBossState(CCOD_ENC_FEL_ANCIENT) == DONE);
        if (open)
            player->NearTeleportTo(CodParlorLanding);
        else
            CodSay(player, "The portal is dormant until Mana Devourer and Fel Ancient are defeated.",
                "Портал не работает, пока не побеждены Пожиратель маны и Древень Скверны.");
        return true;
    }
};

// The round door from the sewers up to the city (665036): says why it is shut until Atrexis and Lyn are dead.
struct classic_go_cod_city_door : public GameObjectAI
{
    classic_go_cod_city_door(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        if (me->GetGoState() == GO_STATE_ACTIVE)
            return true;                // already open
        InstanceScript* instance = me->GetInstanceScript();
        bool const done = instance && instance->GetBossState(CCOD_ENC_ATREXIS) == DONE && instance->GetBossState(CCOD_ENC_LYN) == DONE;
        if (done || player->IsGameMaster())
            me->SetGoState(GO_STATE_ACTIVE);
        else
            CodSay(player, "The way up is sealed until Atrexis and Lyn are defeated.",
                "Путь наверх закрыт, пока не побеждены Атрексий и Лин.");
        return true;
    }
};

// Portal To the Violet Chamber (656320) in the Purple Parlor: back down.
struct classic_go_cod_portal_from_parlor : public GameObjectAI
{
    classic_go_cod_portal_from_parlor(GameObject* go) : GameObjectAI(go) { }

    bool OnGossipHello(Player* player) override
    {
        player->NearTeleportTo(CodParlorExitLanding);
        return true;
    }
};

void AddSC_classic_instance_city_of_dalaran()
{
    new classic_instance_city_of_dalaran();
    RegisterCreatureAI(classic_boss_cod_atrexis);
    RegisterCreatureAI(classic_boss_cod_lyn);
    RegisterCreatureAI(classic_boss_cod_arcane_anomaly);
    RegisterCreatureAI(classic_boss_cod_arcanic_enigma);
    RegisterCreatureAI(classic_boss_cod_unstable_sentinel);
    RegisterCreatureAI(classic_boss_cod_mana_devourer);
    RegisterCreatureAI(classic_boss_cod_fel_ancient);
    RegisterCreatureAI(classic_boss_cod_shade_of_the_archmage);
    RegisterGameObjectAI(classic_go_cod_sewer_grate);
    RegisterGameObjectAI(classic_go_cod_portal_to_parlor);
    RegisterGameObjectAI(classic_go_cod_portal_from_parlor);
    RegisterGameObjectAI(classic_go_cod_city_door);
}
