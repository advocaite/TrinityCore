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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/burning_steppes/blackrock_spire/boss_urok.cpp
// (ScriptDev2 lineage, GPL-2). Urok Doomhowl challenge event.
// Ported: go_urok_challenge, npc_urok_enforcer, npc_urok_ogre_magus, event_banner_destroyed

#include "ScriptMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "Map.h"
#include "MotionMaster.h"
#include "MovementDefines.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "QuaternionData.h"
#include "ScriptedCreature.h"
#include "Common.h"
#include "TemporarySummon.h"
#include <array>

namespace
{
    enum ClassicUrokData : uint32
    {
        GO_SUMMON_CIRCLE        = 175571,
        GO_AUTEL_OFFRANDES      = 175621,
        GO_CHALLENGE_UROK       = 175584,

        NPC_UROK_MASSACRER      = 10601, // Urok Enforcer (melee)
        NPC_UROK_MAGE           = 10602, // Urok Ogre Magus (caster)
        NPC_UROK_DOOMHOWL       = 10584, // the boss

        SPELL_DECLENCHER_DEFIS  = 16447, // right click on GO_CHALLENGE_UROK
        SPELL_UROK_SUMMONED     = 16473, // lightning visual
        SPELL_UROK_ADD_SUMMONED = 16562, // teleport visual
        SPELL_KILL_UROK_ADD     = 16452,
        SPELL_DESTROY_SPEAR     = 16557,
        SPELL_DESTROY_SPEAR2    = 16558,
        SPELL_BLOODLUST         = 6742,
        SPELL_SLOW              = 13747,
        SPELL_ARCANE_BOLT       = 15979,

        // VMaNGOS creature_spells 106010 (Urok Enforcer)
        SPELL_UROK_ENFORCER_STRIKE   = 14516,
        SPELL_UROK_ENFORCER_PUNCTURE = 15976,

        EVENT_ID_BANNER_DESTROYED = 4777,

        UROK_POINT_BANNER       = 2
    };

    constexpr float UROK_CHALLENGER_RANGE = 60.0f;   // players this close to the pile are the challengers the adds attack
}

void ClassicBrsUrokDefineGoChallenge(Creature* creature, ObjectGuid gobjGUID);

/*######
## go_urok_challenge (GO_CHALLENGE_UROK)
######*/

// VMaNGOS GameObjectAI::OnUse -> TC GameObjectAI::OnGossipHello (called from GameObject::Use)
struct classic_go_urok_challenge : public GameObjectAI
{
    classic_go_urok_challenge(GameObject* go) : GameObjectAI(go), _actived(true), _step(0), _timer(0), _spellTimer(0), _nbDeadUnderlings(0) { }

    // Called every map update: handles the add spawn timers.
    void UpdateAI(uint32 diff) override
    {
        if (_spellTimer < diff)
            _spellTimer = 0;
        else
            _spellTimer -= diff;

        if (_actived)
        {
            if (_timer < diff)
            {
                switch (_step)
                {
                    case 0:
                        SpawnRune(0, -13.7275f, -384.816f, 48.9746f, 3.68265f);
                        SpawnRune(1, -27.8804f, -385.891f, 48.5067f, 3.7001f);
                        SpawnRune(2, -24.8824f, -369.619f, 49.7059f, 3.4034f);
                        SpawnRune(3, -12.3689f, -376.475f, 49.335f, 5.044f);
                        SpawnRune(4, -34.5437f, -370.233f, 50.3396f, 5.35816f);
                        SpawnRune(5, -47.9095f, -369.089f, 51.5425f, 1.37881f);
                        _timer = 3000;
                        break;
                    case 1:
                        SpawnAtRune(0, NPC_UROK_MAGE, 0);
                        SpawnAtRune(2, NPC_UROK_MASSACRER, 1);
                        SpawnAtRune(3, NPC_UROK_MASSACRER, 2);
                        _timer = 10000;
                        break;
                    default:
                        break;
                }
                ++_step;
            }
            else
                _timer -= diff;
        }
    }

    void SpawnAtRune(uint32 rune, uint32 entry, int i = 0)
    {
        if (GameObject* go = ObjectAccessor::GetGameObject(*me, _runes[rune]))
        {
            if (Creature* invoc = me->SummonCreature(entry, go->GetPositionX(), go->GetPositionY(), go->GetPositionZ(), 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 400000ms))
            {
                invoc->SetRespawnDelay(7 * DAY);
                // Spawn visual
                // TODO(classic): VMaNGOS only sends the SMSG_SPELL_GO visual (SendSpellGo); TC casts the (spawn/dummy) spell instead.
                if (entry == NPC_UROK_DOOMHOWL)
                    invoc->CastSpell(invoc, SPELL_UROK_SUMMONED, true);
                else
                {
                    _guidCurrentUnderlings[i] = invoc->GetGUID();
                    ClassicBrsUrokDefineGoChallenge(invoc, me->GetGUID());
                    invoc->CastSpell(invoc, SPELL_UROK_ADD_SUMMONED, true);
                }
                // Put in combat with the challengers: the players at the pile, not everyone in the instance. Players elsewhere in
                // Blackrock Spire were held in combat by adds that could never reach them, for the 400 s despawn timer that never
                // runs out while they are in combat (UBRS 2026-10-26: every bot healing or helping them joined that combat too).
                GameObject* pile = me;
                me->GetMap()->DoOnPlayers([invoc, pile](Player* player)
                {
                    if (player->IsAlive() && !invoc->IsFriendlyTo(player) && invoc->AI() &&
                        player->IsWithinDist(pile, UROK_CHALLENGER_RANGE))
                        invoc->AI()->AttackStart(player);
                });
            }
        }
    }

    void SpawnBoss()
    {
        SpawnAtRune(5, NPC_UROK_DOOMHOWL);
        DespawnRunes();
    }

    void SpawnRune(uint32 i, float x, float y, float z, float o)
    {
        if (GameObject* go = me->SummonGameObject(GO_SUMMON_CIRCLE, x, y, z, o, QuaternionData::fromEulerAnglesZYX(o, 0.0f, 0.0f), 0s))
            _runes[i] = go->GetGUID();
    }

    void DespawnRunes()
    {
        for (ObjectGuid const& guid : _runes)
            if (GameObject* go = ObjectAccessor::GetGameObject(*me, guid))
                go->DespawnOrUnsummon();
    }

    Unit* NearestOgre()
    {
        Unit* massacrer = me->FindNearestCreature(NPC_UROK_MASSACRER, 20.0f);
        Unit* mage      = me->FindNearestCreature(NPC_UROK_MAGE, 20.0f);
        Unit* target    = nullptr;
        if (!mage)
            target = massacrer;
        else if (massacrer)
        {
            if (me->GetDistance(massacrer) < me->GetDistance(mage))
                target = massacrer;
            else
                target = mage;
        }
        else
            target = mage;
        // Otherwise massacrer = mage = nullptr, no target.
        return target;
    }

    // Right click on the gameobject.
    bool OnGossipHello(Player* player) override
    {
        if (_actived)
        {
            // Cast the 10k damage spell if it is ready.
            if (!_spellTimer)
            {
                if (Unit* target = NearestOgre())
                {
                    _spellTimer = 30000;
                    player->CastSpell(target, SPELL_KILL_UROK_ADD, false);
                }
            }
            return true;
        }
        return true;
    }

    void EventBannerDestroyed(ObjectGuid /*sourceGuid*/)
    {
        if (!_actived)
            return;
        _actived = false;
        DespawnRunes();
        me->DespawnOrUnsummon();
    }

    void UrokUnderlingDied(ObjectGuid creatureGUID)
    {
        if (!_actived)
            return;

        for (int i = 0; i < 3; ++i)
        {
            if (creatureGUID == _guidCurrentUnderlings[i])
            {
                ++_nbDeadUnderlings;
                if (_nbDeadUnderlings < 8)
                {
                    uint32 entry = urand(0, 1) ? NPC_UROK_MAGE : NPC_UROK_MASSACRER;
                    SpawnAtRune(RuneOrder[_nbDeadUnderlings + 2], entry, i);
                }
                else if (_nbDeadUnderlings == 8)
                {
                    SpawnBoss();
                }
            }
        }
    }

private:
    static constexpr uint8 RuneOrder[10] = { 0, 2, 3, 1, 4, 5, 1, 2, 3, 5 };

    bool _actived;
    uint32 _step;
    uint32 _timer;
    uint32 _spellTimer;
    std::array<ObjectGuid, 6> _runes;
    uint8 _nbDeadUnderlings;
    std::array<ObjectGuid, 3> _guidCurrentUnderlings;
};

/*######
## Urok underlings (npc_urok_enforcer, npc_urok_ogre_magus)
######*/

struct classic_brs_urok_underlingAI : public ScriptedAI
{
    classic_brs_urok_underlingAI(Creature* creature) : ScriptedAI(creature), _timer(0) { }

    void Reset() override
    {
        _timer = 0;
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (GameObject* gobj = ObjectAccessor::GetGameObject(*me, _guidMound))
            if (classic_go_urok_challenge* moundAI = dynamic_cast<classic_go_urok_challenge*>(gobj->AI()))
                moundAI->UrokUnderlingDied(me->GetGUID());
    }

    void MovementInform(uint32 type, uint32 pointId) override
    {
        if (type != POINT_MOTION_TYPE)
            return;
        if (pointId != UROK_POINT_BANNER)
            return;
        HitBanner();
    }

    void AttackStart(Unit* unit) override
    {
        if (me->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            return;
        ScriptedAI::AttackStart(unit);
    }

    void BannerDestroyed()
    {
        if (GameObject* go = me->FindNearestGameObject(GO_CHALLENGE_UROK, 40.0f))
            if (classic_go_urok_challenge* pedestalAI = dynamic_cast<classic_go_urok_challenge*>(go->AI()))
                pedestalAI->EventBannerDestroyed(me->GetGUID());
    }

    bool HitBanner()
    {
        if (GameObject* go = me->FindNearestGameObject(GO_CHALLENGE_UROK, CONTACT_DISTANCE + 1.0f))
        {
            me->CastSpell(go->GetPosition(), SPELL_DESTROY_SPEAR, false);
            _timer = 11000;
            return true;
        }
        return false;
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
        {
            if (_timer < diff)
            {
                if (!HitBanner())
                {
                    if (GameObject* go = me->FindNearestGameObject(GO_CHALLENGE_UROK, 50.0f))
                    {
                        float x, y, z;
                        go->GetContactPoint(me, x, y, z, CONTACT_DISTANCE);
                        me->GetMotionMaster()->MovePoint(UROK_POINT_BANNER, x, y, z);
                        _timer = 10000;
                    }
                }
            }
            else
                _timer -= diff;
            return;
        }

        AbilityCombatUpdate(diff);
    }

    virtual void AbilityCombatUpdate(uint32 /*diff*/) { }

    void SetMoundGuid(ObjectGuid moundGuid)
    {
        _guidMound = moundGuid;
    }

protected:
    uint32 _timer;
    ObjectGuid _guidMound;
};

void ClassicBrsUrokDefineGoChallenge(Creature* creature, ObjectGuid gobjGUID)
{
    if (classic_brs_urok_underlingAI* underlingAI = dynamic_cast<classic_brs_urok_underlingAI*>(creature->AI()))
        underlingAI->SetMoundGuid(gobjGUID);
}

// TODO(classic): VMaNGOS runs the generic creature_spells lists here (UpdateSpellsList, creature_spells 106010 / 106020).
// TC has no creature spell lists, so the two lists are hardcoded below from vmangos_world.creature_spells
// (delays in seconds; VMaNGOS castFlags 32 = aura not present, 64 = only in melee, 136 = main ranged spell + not in melee).
// Target types that TC cannot express 1:1 (Bloodlust: random friendly, targetParam2 70) are approximated.
struct classic_npc_urok_enforcer : public classic_brs_urok_underlingAI
{
    classic_npc_urok_enforcer(Creature* creature) : classic_brs_urok_underlingAI(creature)
    {
        InitializeSpells();
    }

    void InitializeSpells()
    {
        _strikeTimer = urand(8000, 12000);
        _punctureTimer = urand(1000, 4000);
    }

    void Reset() override
    {
        classic_brs_urok_underlingAI::Reset();
        InitializeSpells();
    }

    void AbilityCombatUpdate(uint32 diff) override
    {
        // 14516 Strike: victim, initial 8-12s, repeat 8-12s
        if (_strikeTimer <= diff)
        {
            if (DoCastVictim(SPELL_UROK_ENFORCER_STRIKE) == SPELL_CAST_OK)
                _strikeTimer = urand(8000, 12000);
        }
        else
            _strikeTimer -= diff;

        // 15976 Puncture: victim, aura not present, initial 1-4s, repeat 1-8s
        if (_punctureTimer <= diff)
        {
            Unit* victim = me->GetVictim();
            if (victim && !victim->HasAura(SPELL_UROK_ENFORCER_PUNCTURE, me->GetGUID()))
            {
                if (DoCast(victim, SPELL_UROK_ENFORCER_PUNCTURE) == SPELL_CAST_OK)
                    _punctureTimer = urand(1000, 8000);
            }
            else
                _punctureTimer = urand(1000, 8000);
        }
        else
            _punctureTimer -= diff;
    }

private:
    uint32 _strikeTimer;
    uint32 _punctureTimer;
};

struct classic_npc_urok_ogre_magus : public classic_brs_urok_underlingAI
{
    classic_npc_urok_ogre_magus(Creature* creature) : classic_brs_urok_underlingAI(creature)
    {
        InitializeSpells();
    }

    void InitializeSpells()
    {
        _bloodlustTimer = 0;
        _slowTimer = urand(8000, 11000);
        _arcaneBoltTimer = 0;
    }

    void Reset() override
    {
        classic_brs_urok_underlingAI::Reset();
        InitializeSpells();
    }

    void AbilityCombatUpdate(uint32 diff) override
    {
        // 6742 Bloodlust: friendly target, aura not present, initial 0s, repeat 30-35s
        if (_bloodlustTimer <= diff)
        {
            Unit* target = me;
            if (Creature* ally = me->FindNearestCreature(urand(0, 1) ? NPC_UROK_MASSACRER : NPC_UROK_MAGE, 30.0f))
                if (!ally->HasAura(SPELL_BLOODLUST))
                    target = ally;

            if (target->HasAura(SPELL_BLOODLUST) || DoCast(target, SPELL_BLOODLUST) == SPELL_CAST_OK)
                _bloodlustTimer = urand(30000, 35000);
        }
        else
            _bloodlustTimer -= diff;

        // 13747 Slow: random hostile, initial 8-11s, repeat 17-24s
        if (_slowTimer <= diff)
        {
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                if (DoCast(target, SPELL_SLOW) == SPELL_CAST_OK)
                    _slowTimer = urand(17000, 24000);
        }
        else
            _slowTimer -= diff;

        // 15979 Arcane Bolt: victim, repeat 4-6s in melee range / 2-3s out of melee range (main ranged spell)
        if (_arcaneBoltTimer <= diff)
        {
            if (Unit* victim = me->GetVictim())
            {
                bool const inMelee = me->IsWithinMeleeRange(victim);
                if (DoCast(victim, SPELL_ARCANE_BOLT) == SPELL_CAST_OK)
                    _arcaneBoltTimer = inMelee ? urand(4000, 6000) : urand(2000, 3000);
            }
        }
        else
            _arcaneBoltTimer -= diff;
    }

private:
    uint32 _bloodlustTimer;
    uint32 _slowTimer;
    uint32 _arcaneBoltTimer;
};

/*######
## event_banner_destroyed (event 4777)
######*/

class classic_event_banner_destroyed : public EventScript
{
public:
    classic_event_banner_destroyed() : EventScript("classic_event_banner_destroyed") { }

    // TC: object = VMaNGOS "target", invoker = VMaNGOS "source"
    void OnTrigger(WorldObject* object, WorldObject* invoker, uint32 /*eventId*/) override
    {
        // we go through the source because target is null.... maybe because it is a summoned object.
        if (!object)
        {
            if (Creature* source = invoker ? invoker->ToCreature() : nullptr)
                if (classic_brs_urok_underlingAI* underlingAI = dynamic_cast<classic_brs_urok_underlingAI*>(source->AI()))
                    underlingAI->BannerDestroyed();
        }
        else if (GameObject* go = object->ToGameObject())
        {
            if (classic_go_urok_challenge* pedestalAI = dynamic_cast<classic_go_urok_challenge*>(go->AI()))
                pedestalAI->EventBannerDestroyed(invoker ? invoker->GetGUID() : ObjectGuid::Empty);
        }
    }
};

void AddSC_classic_boss_urok()
{
    RegisterGameObjectAI(classic_go_urok_challenge);
    RegisterCreatureAI(classic_npc_urok_enforcer);
    RegisterCreatureAI(classic_npc_urok_ogre_magus);
    new classic_event_banner_destroyed();
}
