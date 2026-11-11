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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/stormwind_city/stormwind_city.cpp (ScriptDev2 lineage, GPL-2)
// Quest support: 1640 (Beat Bartleby), 1447 (The Missing Diplomat part 8)

#include "ScriptMgr.h"
#include "Containers.h"
#include "Map.h"
#include "Group.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptedCreature.h"
#include "TaskScheduler.h"
#include "TemporarySummon.h"
#include "ThreatManager.h"
#include "classic_script_text.h"

/*######
## npc_bartleby
######*/

enum BartlebyData
{
    CLASSIC_FACTION_ENEMY       = 168,
    QUEST_BEAT          = 1640
};

struct classic_npc_bartleby : public ScriptedAI
{
    classic_npc_bartleby(Creature* creature) : ScriptedAI(creature)
    {
        _normalFaction = creature->GetFaction();
    }

    void Reset() override
    {
        if (me->GetFaction() != _normalFaction)
            me->SetFaction(_normalFaction);
    }

    // VMaNGOS AttackedBy override (attack back unless friendly) is TC's default behaviour; nothing to port.

    void DamageTaken(Unit* doneBy, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (!doneBy)
            return;

        if (damage > me->GetHealth() || ((me->GetHealth() - damage) * 100 / me->GetMaxHealth() < 15))
        {
            damage = 0;

            if (Player* player = doneBy->ToPlayer())
                player->AreaExploredOrEventHappens(QUEST_BEAT);

            EnterEvadeMode();
        }
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_BEAT)
        {
            me->SetFaction(CLASSIC_FACTION_ENEMY);
            AttackStart(player);
        }
    }

private:
    uint32 _normalFaction;
};

/*######
## npc_dashel_stonefist
######*/

//-----------------------------------------------------------------------------
// Full quest event implementation (Missing Diplomat part 8 id:1447).
// Author: Kampeador
//-----------------------------------------------------------------------------
enum DashelStonefistData
{
    // ids from "broadcast_text" table
    SAY_PROGRESS_1_DAS = 1961, // Now you're gonna get it good, "PlayerName".
    SAY_PROGRESS_2_DAS = 1712, // Okay, okay! Enough fighting. No one else needs to get hurt.
    SAY_PROGRESS_3_DAS = 1713, // It's okay, boys. Back off. You've done enough. I'll meet up with you later.
    SAY_PROGRESS_4_THU = 1716, // All right, boss. You sure though? Just seems like a waste of good practice.
    SAY_PROGRESS_5_THU = 1715, // Yeah, okay, boss. No problem.
    // quest id
    QUEST_MISSING_DIPLO_PT8 = 1447,
    // NPCs that helps Dashel
    NPC_OLD_TOWN_THUG = 4969,
    // factions
    FACTION_NEUTRAL = 189,
    FACTION_IRONFORGE = 122, // original faction taken from DB
    FACTION_FRIENDLY_TO_ALL = 35,
    // quest phases
    MDQP_NONE = 0, // Dashel returns his spawn point

    // Occur only if thugs are alive
    MDQP_SAY1 = 1,
    MDQP_SAY2 = 2,
    MDQP_SAY3 = 4,
    MDQP_THUG_WALK_AWAY_1 = 5,
    MDQP_THUG_WALK_AWAY_2 = 6,

    MDQP_QUEST_COMPLETE = 7 // Triggers quest complete
};

Position const DashelThugResetPosition[] =
{
    { -8669.338867f, 448.362976f, 99.740005f },
    { -8686.397461f, 447.595703f, 99.994408f }
};

// VMaNGOS Player::GroupEventFailHappens (not in TC): fail the quest for the player and his group members on the map
static void ClassicGroupEventFailHappens(Player* player, uint32 questId)
{
    if (Group* group = player->GetGroup())
    {
        for (GroupReference const& itr : group->GetMembers())
        {
            Player* member = itr.GetSource();
            if (member && member->IsInMap(player) && member->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
                member->FailQuest(questId);
        }
    }
    else if (player->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
        player->FailQuest(questId);
}

struct classic_npc_dashel_stonefist : public ScriptedAI
{
    // old town thugs
    ObjectGuid m_thugs[2];
    // current event phase
    uint32 m_eventPhase;
    // check if an event has been started.
    bool m_dialogStarted;
    // player guid to trigger: quest completed
    ObjectGuid m_playerGuid;
    // timer to switch between phases
    uint32 m_nextPhaseDelayTimer;
    // used to check if 1 or more thugs are alive
    bool m_thugsAlive;
    // used to check if its a quest fight or not.
    bool m_questFightStarted;

    // if quest fight is active, then dialog event will be triggered
    inline void startQuestFight() { m_questFightStarted = true; }

    classic_npc_dashel_stonefist(Creature* creature) : ScriptedAI(creature), m_eventPhase(MDQP_NONE), m_dialogStarted(false),
        m_nextPhaseDelayTimer(3000), m_thugsAlive(false), m_questFightStarted(false) { }

    void Reset() override
    {
        if (m_questFightStarted)
        {
            // Reset() during quest fight -> quest failed.
            if (Player* player = ObjectAccessor::GetPlayer(*me, m_playerGuid))
                ClassicGroupEventFailHappens(player, QUEST_MISSING_DIPLO_PT8);

            // remove thugs
            for (ObjectGuid const& guid : m_thugs)
            {
                if (Creature* thug = ObjectAccessor::GetCreature(*me, guid))
                    if (thug->IsAlive())
                        if (TempSummon* summon = thug->ToTempSummon())
                            summon->UnSummon();
            }
        }

        // clear thug guids
        for (ObjectGuid& guid : m_thugs)
            guid.Clear();

        m_questFightStarted = false;
        m_eventPhase = MDQP_NONE;
        m_dialogStarted = false;
        m_nextPhaseDelayTimer = 3000; // MDQP_SAY1 phase delay
        m_thugsAlive = false;
        // restore some flags
        me->SetNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
        // restore faction
        me->SetFaction(FACTION_IRONFORGE);
        // reset player guid
        m_playerGuid.Clear();
    }

    // VMaNGOS: after the surrender Dashel only drops combat and walks home (no Reset()); TC's default evade always calls
    // Reset(), which would end the dialog, so skip it while the dialog is running.
    void EnterEvadeMode(EvadeReason why) override
    {
        if (!m_dialogStarted)
        {
            ScriptedAI::EnterEvadeMode(why);
            return;
        }

        if (!_EnterEvadeMode(why))
            return;

        me->AddUnitState(UNIT_STATE_EVADE);
        me->GetMotionMaster()->MoveTargetedHome();
    }

    void DamageTaken(Unit* /*doneBy*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (m_questFightStarted)
        {
            if (damage > me->GetHealth() || me->GetHealthPct() < 20.0f)
            {
                damage = 0;

                // Dashel says: Okay, okay! Enough fighting. No one else needs to get hurt.
                ClassicScriptText(SAY_PROGRESS_2_DAS, me);

                me->RemoveAllAuras();
                me->SetFaction(FACTION_FRIENDLY_TO_ALL);

                // check if thugs are alive
                for (ObjectGuid const& guid : m_thugs)
                {
                    if (Creature* thug = ObjectAccessor::GetCreature(*me, guid))
                    {
                        if (!thug->IsAlive())
                            continue;

                        thug->RemoveAllAuras();
                        thug->GetThreatManager().ClearAllThreat();
                        thug->CombatStop();

                        thug->SetFaction(FACTION_FRIENDLY_TO_ALL);

                        thug->GetMotionMaster()->MoveTargetedHome();

                        m_thugsAlive = true;
                    }
                }

                m_questFightStarted = false;
                m_dialogStarted = true;
                m_eventPhase = MDQP_NONE;

                // DeleteThreatList + CombatStop + MoveTargetedHome (Reset() is skipped, see EnterEvadeMode)
                me->GetThreatManager().ClearAllThreat();
                EnterEvadeMode(EvadeReason::Other);
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        switch (m_eventPhase)
        {
            case MDQP_SAY1: // Occurs only if thugs are alive
            {
                if (m_nextPhaseDelayTimer < diff)
                {
                    // Dashel says: It's okay, boys. Back off. You've done enough.I'll meet up with you later.
                    ClassicScriptText(SAY_PROGRESS_3_DAS, me);
                    // switch phase
                    m_nextPhaseDelayTimer = 3000;
                    m_eventPhase = MDQP_SAY2;
                }
                else
                    m_nextPhaseDelayTimer -= diff;
                break;
            }
            case MDQP_SAY2: // Occurs only if thugs are alive
            {
                if (m_nextPhaseDelayTimer < diff)
                {
                    // recheck for safety, thugs can be killed by gm command, etc.
                    if (Creature* thug = ObjectAccessor::GetCreature(*me, m_thugs[0]))
                        if (thug->IsAlive())
                            ClassicScriptText(SAY_PROGRESS_4_THU, thug);

                    // switch phase
                    m_nextPhaseDelayTimer = 1500;
                    m_eventPhase = MDQP_SAY3;
                }
                else
                    m_nextPhaseDelayTimer -= diff;
                break;
            }
            case MDQP_SAY3: // Occurs only if thugs are alive
            {
                if (m_nextPhaseDelayTimer < diff)
                {
                    // recheck for safety, thugs can be killed by gm command, etc.
                    if (Creature* thug = ObjectAccessor::GetCreature(*me, m_thugs[1]))
                        if (thug->IsAlive())
                            ClassicScriptText(SAY_PROGRESS_5_THU, thug);

                    // switch phase
                    m_nextPhaseDelayTimer = 1000;
                    m_eventPhase = MDQP_THUG_WALK_AWAY_1;
                }
                else
                    m_nextPhaseDelayTimer -= diff;
                break;
            }
            case MDQP_THUG_WALK_AWAY_1: // Occurs only if thugs are alive
            {
                if (m_nextPhaseDelayTimer < diff)
                {
                    ResetThug(0);

                    m_nextPhaseDelayTimer = 1500;
                    m_eventPhase = MDQP_THUG_WALK_AWAY_2;
                }
                else
                    m_nextPhaseDelayTimer -= diff;
                break;
            }
            case MDQP_THUG_WALK_AWAY_2: // Occurs only if thugs are alive
            {
                if (m_nextPhaseDelayTimer < diff)
                {
                    // Second thug goes away
                    ResetThug(1);

                    m_nextPhaseDelayTimer = 1000;
                    m_eventPhase = MDQP_QUEST_COMPLETE;
                }
                else
                    m_nextPhaseDelayTimer -= diff;
                break;
            }
            case MDQP_QUEST_COMPLETE:
            {
                if (m_nextPhaseDelayTimer < diff)
                {
                    // Set quest completed
                    if (Player* player = ObjectAccessor::GetPlayer(*me, m_playerGuid))
                        player->GroupEventHappens(QUEST_MISSING_DIPLO_PT8, me);

                    Reset();
                }
                else
                    m_nextPhaseDelayTimer -= diff;
                break;
            }
            default: // MDQP_NONE
                UpdateVictim();
                break;
        }
    }

    void ResetThug(int thug)
    {
        if (thug >= 2)
            return;

        if (Creature* pThug = ObjectAccessor::GetCreature(*me, m_thugs[thug]))
        {
            if (pThug->IsAlive())
            {
                Position const& pos = DashelThugResetPosition[thug];
                pThug->GetMotionMaster()->MovePoint(0, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), true, {}, {},
                    MovementWalkRunSpeedSelectionMode::ForceWalk);
                pThug->DespawnOrUnsummon(3s);
            }
        }
    }

    // Dashel returns to his spawn point
    void JustReachedHome() override
    {
        // switch to correct dialog phase, depends if thugs are alive.
        if (m_dialogStarted)
        {
            if (m_thugsAlive)
                m_eventPhase = MDQP_SAY1;
            else
                m_eventPhase = MDQP_QUEST_COMPLETE;
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        // case: something weird happened, killed by GM command, etc.
        if (m_dialogStarted || m_questFightStarted)
        {
            // remove thugs
            for (ObjectGuid& guid : m_thugs)
            {
                if (Creature* thug = ObjectAccessor::GetCreature(*me, guid))
                    if (TempSummon* summon = thug->ToTempSummon())
                        summon->UnSummon();
                guid.Clear();
            }
        }
    }

    void SummonedCreatureDies(Creature* creature, Unit* /*killer*/) override
    {
        // If the thug died for whatever reason, clear the guid.
        for (ObjectGuid& guid : m_thugs)
        {
            if (guid == creature->GetGUID())
                guid.Clear();
        }
    }

    void SummonedCreatureDespawn(Creature* creature) override
    {
        for (ObjectGuid& guid : m_thugs)
        {
            if (guid == creature->GetGUID())
                guid.Clear();
        }
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_MISSING_DIPLO_PT8)
            return;

        // On official in some cases: he didn't say this phrase. I am not sure if it was a bug or a random feature.
        // Dashel says: Now you're gonna get it good, "PlayerName".
        ClassicScriptText(SAY_PROGRESS_1_DAS, me, player);

        me->SetFaction(FACTION_NEUTRAL);
        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);

        m_playerGuid = player->GetGUID();

        // spawn thugs and make them focus player.
        Creature* thug1 = me->SummonCreature(NPC_OLD_TOWN_THUG, -8676.075195f, 443.744019f, 99.632210f, 3.981758f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 30s);
        if (!thug1 || !thug1->AI())
            return;

        m_thugs[0] = thug1->GetGUID();

        thug1->AI()->AttackStart(player);
        thug1->SetImmuneToNPC(false);

        // thug 2
        if (Creature* thug2 = me->SummonCreature(NPC_OLD_TOWN_THUG, -8685.416992f, 443.130829f, 99.526917f, 5.759635f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 30s))
        {
            m_thugs[1] = thug2->GetGUID();
            if (thug2->AI())
                thug2->AI()->AttackStart(player);
            thug2->SetImmuneToNPC(false);
        }
        // start quest fight.
        startQuestFight();
        // make Dashel focus player.
        AttackStart(player);
    }
};

/*######
## The Attack! (quest 434): Tyrion, Tyrion's Spybot, Lord Gregor Lescovar, Marzon the Silent Blade
## Port of the VMaNGOS quest event (quest_start_scripts 434, creature_movement_scripts 175406-175408, 777905-777932,
## 885605, generic_scripts 4340/4341). The VMaNGOS import only carried part of it into SmartAI, so the spybot never
## left Tyrion and Lescovar never walked to the garden.
######*/

enum TheAttackData
{
    QUEST_THE_ATTACK                = 434,

    NPC_TYRION                      = 7766,
    NPC_TYRIONS_SPYBOT              = 8856,
    NPC_PRIESTESS_TYRIONA           = 7779,
    NPC_LORD_GREGOR_LESCOVAR        = 1754,
    NPC_MARZON_THE_SILENT_BLADE     = 1755,

    PATH_TYRIONA_TO_LESCOVAR        = 810077790,   // 33 points: Tyrion -> garden guards (5) -> library (14) -> back out (30) -> hide (32)
    PATH_LESCOVAR_TO_GARDEN         = 810017540,   // 13 points: library -> garden door (6) -> garden (7, waits 60 s) -> back (8 ...)
    PATH_SPYBOT_HOME                = 810088560,   // 6 points: from the castle side door back to Tyrion (5)

    SPELL_ILLUSION_TYRIONA          = 11048,
    SPELL_ILLUSION_TYRION           = 11067,
    SPELL_SPYBOT_SNEAK              = 11013,
    SPELL_MARZON_SNEAK              = 7104,

    FACTION_DEFIAS_BROTHERHOOD      = 34,

    // broadcast_text ids
    SAY_SPYBOT_BY_YOUR_COMMAND      = 4593,
    SAY_TYRION_WAIT_HERE            = 3761,
    SAY_SPYBOT_GOOD_DAY             = 3781,
    SAY_GUARD_LIBRARY               = 3783,
    SAY_SPYBOT_THANK_YOU            = 3782,
    SAY_SPYBOT_GUEST_ARRIVED        = 3762,
    SAY_LESCOVAR_THANK_YOU          = 3784,
    SAY_SPYBOT_USE_THE_TIME         = 3791,
    SAY_LESCOVAR_MEDITATION         = 322,
    SAY_GUARD_YES_SIR               = 3690,
    EMOTE_LESCOVAR_WAITS            = 3721,
    SAY_LESCOVAR_WHAT_NEWS          = 323,
    SAY_MARZON_VANCLEEF             = 324,
    SAY_LESCOVAR_MEDDLER_SHAW       = 326,
    SAY_MARZON_I_WILL_RETURN        = 325,
    SAY_TYRION_KILL_THEM            = 4613,
    SAY_TYRION_GREAT_JOB            = 4612,
    SAY_LESCOVAR_AGGRO              = 3934,
    SAY_MARZON_AGGRO                = 3936,

    ACTION_SPYBOT_GO                = 1,
    ACTION_SPYBOT_HOME,
    ACTION_SPYBOT_HIDDEN,
    ACTION_SPYBOT_RETURN,
    ACTION_SPYBOT_RESET,
    ACTION_LESCOVAR_START,
    ACTION_CONVERSATION_DONE,
    ACTION_LESCOVAR_DIED,
    ACTION_MARZON_DIED,
    ACTION_LESCOVAR_BACK
};

ObjectGuid::LowType const SPAWN_TYRIONS_SPYBOT = 20045707;
ObjectGuid::LowType const SPAWN_ROYAL_GUARD_A  = 20010523;   // VMaNGOS guid 10523
ObjectGuid::LowType const SPAWN_ROYAL_GUARD_B  = 20010524;   // VMaNGOS guid 10524

Position const TheAttackGuardHome[2] = { { -8394.09f, 449.427f, 123.76f, 0.645772f }, { -8388.84f, 453.084f, 123.76f, 3.80482f } };
Position const TheAttackGuardHall[2] = { { -8356.38f, 404.744f, 122.384f }, { -8353.65f, 405.941f, 122.344f } };
Position const TheAttackGuardPost[2] = { { -8367.22f, 396.232f, 122.393f, 0.663225f }, { -8342.47f, 414.809f, 122.393f, 3.76991f } };
Position const TheAttackMarzonSpawn  = { -8401.24f, 491.133f, 123.843f, 4.03171f };
Position const TheAttackMarzonWalk[2] = { { -8406.11f, 481.173f, 123.793f }, { -8405.02f, 470.18f, 123.793f } };
Position const TheAttackSpybotHidden = { -8430.7f, 442.358f, 122.358f, 0.856207f };   // its spawn spot, start of PATH_SPYBOT_HOME
Position const TheAttackSpybotAtTyrion = { -8408.25f, 451.896f, 123.76f, 5.52986f };

static Creature* TheAttackBySpawnId(Map* map, ObjectGuid::LowType spawnId)
{
    for (auto const& pair : Trinity::Containers::MapEqualRange(map->GetCreatureBySpawnIdStore(), spawnId))
        if (pair.second->IsAlive())
            return pair.second;
    return nullptr;
}

static void TheAttackGuardsHome(Map* map)
{
    ObjectGuid::LowType const ids[2] = { SPAWN_ROYAL_GUARD_A, SPAWN_ROYAL_GUARD_B };
    for (uint8 i = 0; i < 2; ++i)
    {
        if (Creature* guard = TheAttackBySpawnId(map, ids[i]))
        {
            if (guard->IsInCombat())
                continue;
            guard->SetWalk(true);
            guard->GetMotionMaster()->MovePoint(0, TheAttackGuardHome[i], true, TheAttackGuardHome[i].GetOrientation());
        }
    }
}

struct classic_npc_tyrion : public ScriptedAI
{
    classic_npc_tyrion(Creature* creature) : ScriptedAI(creature) { }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_THE_ATTACK || _active)
            return;

        _active = true;
        _won = false;
        _lescovarDead = false;
        _marzonDead = false;
        _playerGuid = player->GetGUID();
        _timeLeft = 600 * IN_MILLISECONDS;
        _checkTimer = 1000;
        uint32 const gen = ++_gen;

        me->SetFacingTo(1.8326f);
        me->SetEmoteState(EMOTE_STATE_USE_STANDING);
        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);

        _scheduler.Schedule(4s, [this, gen](TaskContext /*context*/)
        {
            if (gen != _gen)
                return;
            me->SetEmoteState(EMOTE_ONESHOT_NONE);
            if (Creature* spybot = Spybot())
            {
                spybot->AI()->DoAction(ACTION_SPYBOT_RESET);
                ClassicScriptText(SAY_SPYBOT_BY_YOUR_COMMAND, spybot);
            }
        })
        .Schedule(7s, [this, gen](TaskContext /*context*/)
        {
            if (gen != _gen)
                return;
            ClassicScriptText(SAY_TYRION_WAIT_HERE, me, ObjectAccessor::GetPlayer(*me, _playerGuid));
            me->SetFacingTo(5.7548f);
            me->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
            if (Creature* spybot = Spybot())
                spybot->CastSpell(spybot, SPELL_ILLUSION_TYRIONA, true);
        })
        .Schedule(9s, [this, gen](TaskContext /*context*/)
        {
            if (gen != _gen)
                return;
            if (Creature* spybot = Spybot())
                spybot->UpdateEntry(NPC_PRIESTESS_TYRIONA);
        })
        .Schedule(12s, [this, gen](TaskContext /*context*/)
        {
            if (gen != _gen)
                return;
            me->SetFacingTo(0.0523599f);
        })
        .Schedule(14s, [this, gen](TaskContext /*context*/)
        {
            if (gen != _gen)
                return;
            if (Creature* spybot = Spybot())
                spybot->AI()->DoAction(ACTION_SPYBOT_GO);
            else
                _failNow = true;   // no spybot on the map: give the quest back instead of a dead event
        });
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_CONVERSATION_DONE:
            {
                if (!_active)
                    return;
                uint32 const gen = _gen;
                _scheduler.Schedule(2s, [this, gen](TaskContext /*context*/)
                {
                    if (gen != _gen)
                        return;
                    me->HandleEmoteCommand(EMOTE_ONESHOT_FLEX);
                    ClassicScriptText(SAY_TYRION_KILL_THEM, me);
                });
                break;
            }
            case ACTION_LESCOVAR_DIED:
                _lescovarDead = true;
                CheckWon();
                break;
            case ACTION_MARZON_DIED:
                _marzonDead = true;
                CheckWon();
                break;
            case ACTION_LESCOVAR_BACK:
                // Lescovar walked back from the garden alive: the guards are on their way back, the chance is gone
                if (_active && !_won)
                    _failNow = true;
                break;
            case ACTION_SPYBOT_HIDDEN:
                if (_active && _won)
                    if (Creature* spybot = Spybot())
                        spybot->AI()->DoAction(ACTION_SPYBOT_RETURN);
                break;
            case ACTION_SPYBOT_HOME:
            {
                if (!_active || !_won)
                    return;
                me->HandleEmoteCommand(EMOTE_ONESHOT_CHEER);
                me->SetFacingTo(1.82793f);
                ClassicScriptText(SAY_TYRION_GREAT_JOB, me);
                uint32 const gen = _gen;
                _scheduler.Schedule(3s, [this, gen](TaskContext /*context*/)
                {
                    if (gen != _gen)
                        return;
                    me->SetFacingTo(0.0523599f);
                    EndEvent();
                });
                break;
            }
            default:
                break;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);

        if (!_active)
            return;

        if (_failNow && !_won)
        {
            Fail();
            return;
        }

        if (_timeLeft <= diff)
        {
            if (_won)
                EndEvent();   // the spybot never made it home: give the quest back anyway
            else
                Fail();
            return;
        }
        _timeLeft -= diff;

        if (_won)
            return;

        if (_checkTimer <= diff)
        {
            _checkTimer = 1000;
            // VMaNGOS condition 6306: the player is dead or more than 200 yards from Tyrion
            Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
            if (!player || !player->IsInMap(me))
                Fail();
            else if (player->GetQuestStatus(QUEST_THE_ATTACK) == QUEST_STATUS_NONE)
                Fail();   // abandoned
            else if (player->isDead() || !player->IsWithinDistInMap(me, 200.0f))
                Fail();
        }
        else
            _checkTimer -= diff;
    }

private:
    Creature* Spybot() const { return TheAttackBySpawnId(me->GetMap(), SPAWN_TYRIONS_SPYBOT); }

    void CheckWon()
    {
        if (!_active || _won || !_lescovarDead || !_marzonDead)
            return;

        // VMaNGOS generic_scripts 4340: the guards go back to the door, the spybot comes home
        _won = true;
        _timeLeft = 90 * IN_MILLISECONDS;
        TheAttackGuardsHome(me->GetMap());
        if (Creature* spybot = Spybot())
            spybot->AI()->DoAction(ACTION_SPYBOT_RETURN);
    }

    void Fail()
    {
        // VMaNGOS generic_scripts 4341
        if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid))
            if (player->IsInMap(me))
                ClassicGroupEventFailHappens(player, QUEST_THE_ATTACK);

        TheAttackGuardsHome(me->GetMap());
        if (Creature* spybot = Spybot())
            spybot->AI()->DoAction(ACTION_SPYBOT_RESET);
        EndEvent();
    }

    void EndEvent()
    {
        ++_gen;
        _active = false;
        _won = false;
        _failNow = false;
        _playerGuid.Clear();
        me->SetEmoteState(EMOTE_ONESHOT_NONE);
        me->SetNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
    }

    TaskScheduler _scheduler;
    ObjectGuid _playerGuid;
    uint32 _gen = 0;
    uint32 _timeLeft = 0;
    uint32 _checkTimer = 0;
    bool _active = false;
    bool _won = false;
    bool _failNow = false;
    bool _lescovarDead = false;
    bool _marzonDead = false;
};

struct classic_npc_tyrions_spybot : public ScriptedAI
{
    classic_npc_tyrions_spybot(Creature* creature) : ScriptedAI(creature) { }

    void DoAction(int32 action) override
    {
        switch (action)
        {
            case ACTION_SPYBOT_GO:
                me->SetVisible(true);
                me->SetWalk(true);
                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MovePath(PATH_TYRIONA_TO_LESCOVAR, false);
                break;
            case ACTION_SPYBOT_RETURN:
                if (!_hidden)
                    return;   // not out of sight yet: comes home when it gets there (ACTION_SPYBOT_HIDDEN)
                _hidden = false;
                me->SetVisible(true);
                me->SetWalk(true);
                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MovePath(PATH_SPYBOT_HOME, false);
                break;
            case ACTION_SPYBOT_RESET:
                // back next to Tyrion as itself (event start, or the event failed)
                _scheduler.CancelAll();
                _hidden = false;
                me->RemoveAurasDueToSpell(SPELL_ILLUSION_TYRIONA);
                me->RemoveAurasDueToSpell(SPELL_ILLUSION_TYRION);
                me->RemoveAurasDueToSpell(SPELL_SPYBOT_SNEAK);
                if (me->GetEntry() != NPC_TYRIONS_SPYBOT)
                    me->UpdateEntry(NPC_TYRIONS_SPYBOT);
                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MoveIdle();
                if (me->GetExactDist2d(&TheAttackSpybotAtTyrion) > 2.0f)
                    me->NearTeleportTo(TheAttackSpybotAtTyrion);
                me->SetFacingTo(TheAttackSpybotAtTyrion.GetOrientation());
                me->SetVisible(true);
                break;
            default:
                break;
        }
    }

    void WaypointReached(uint32 nodeId, uint32 pathId) override
    {
        if (pathId == PATH_SPYBOT_HOME && nodeId == 5)
        {
            // VMaNGOS 885605: stop next to Tyrion (also after the walk at server start)
            _scheduler.Schedule(1ms, [this](TaskContext /*context*/)
            {
                me->GetMotionMaster()->Clear();
                me->GetMotionMaster()->MoveIdle();
                if (Creature* tyrion = me->FindNearestCreature(NPC_TYRION, 30.0f))
                    tyrion->AI()->DoAction(ACTION_SPYBOT_HOME);
            });
            return;
        }

        if (pathId != PATH_TYRIONA_TO_LESCOVAR)
            return;

        switch (nodeId)
        {
            case 5: // the two Royal Guards at the garden door (VMaNGOS 777905)
                me->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
                GuardFacing(1.13883f, 3.25305f);
                ClassicScriptText(SAY_SPYBOT_GOOD_DAY, me);
                _scheduler.Schedule(1s, [this](TaskContext /*context*/)
                {
                    for (Creature* guard : Guards())
                        guard->HandleEmoteCommand(EMOTE_ONESHOT_KNEEL);
                })
                .Schedule(4s, [this](TaskContext /*context*/)
                {
                    if (Creature* guard = TheAttackBySpawnId(me->GetMap(), SPAWN_ROYAL_GUARD_B))
                    {
                        guard->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
                        ClassicScriptText(SAY_GUARD_LIBRARY, guard);
                    }
                })
                .Schedule(7s, [this](TaskContext /*context*/)
                {
                    GuardFacing(TheAttackGuardHome[0].GetOrientation(), TheAttackGuardHome[1].GetOrientation());
                })
                .Schedule(8s, [this](TaskContext /*context*/)
                {
                    me->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
                    ClassicScriptText(SAY_SPYBOT_THANK_YOU, me);
                });
                break;
            case 14: // Lescovar in the library (VMaNGOS 777914)
                _scheduler.Schedule(1s, [this](TaskContext /*context*/)
                {
                    me->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
                    ClassicScriptText(SAY_SPYBOT_GUEST_ARRIVED, me);
                    if (Creature* lescovar = me->FindNearestCreature(NPC_LORD_GREGOR_LESCOVAR, 30.0f))
                        lescovar->SetFacingTo(3.66982f);
                })
                .Schedule(5s, [this](TaskContext /*context*/)
                {
                    if (Creature* lescovar = me->FindNearestCreature(NPC_LORD_GREGOR_LESCOVAR, 30.0f))
                    {
                        lescovar->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
                        ClassicScriptText(SAY_LESCOVAR_THANK_YOU, lescovar);
                    }
                })
                .Schedule(11s, [this](TaskContext /*context*/)
                {
                    me->HandleEmoteCommand(EMOTE_ONESHOT_BOW);
                    ClassicScriptText(SAY_SPYBOT_USE_THE_TIME, me);
                })
                .Schedule(12s, [this](TaskContext /*context*/)
                {
                    if (Creature* lescovar = me->FindNearestCreature(NPC_LORD_GREGOR_LESCOVAR, 30.0f))
                        lescovar->AI()->DoAction(ACTION_LESCOVAR_START);
                });
                break;
            case 30: // out of the castle: back to its own look (VMaNGOS 777930)
                _scheduler.Schedule(2s, [this](TaskContext /*context*/)
                {
                    me->RemoveAurasDueToSpell(SPELL_ILLUSION_TYRIONA);
                    me->CastSpell(me, SPELL_ILLUSION_TYRION, true);
                })
                .Schedule(5s, [this](TaskContext /*context*/)
                {
                    me->UpdateEntry(NPC_TYRIONS_SPYBOT);
                })
                .Schedule(6s, [this](TaskContext /*context*/)
                {
                    me->CastSpell(me, SPELL_SPYBOT_SNEAK, true);
                });
                break;
            case 32: // VMaNGOS despawns it here and respawns it on success; it waits out of sight instead (no respawn timer games)
                _scheduler.Schedule(1ms, [this](TaskContext /*context*/)
                {
                    me->GetMotionMaster()->Clear();
                    me->GetMotionMaster()->MoveIdle();
                    me->RemoveAurasDueToSpell(SPELL_ILLUSION_TYRION);
                    me->RemoveAurasDueToSpell(SPELL_SPYBOT_SNEAK);
                    me->SetVisible(false);
                    me->NearTeleportTo(TheAttackSpybotHidden);
                    _hidden = true;
                    if (Creature* tyrion = me->FindNearestCreature(NPC_TYRION, 60.0f))
                        tyrion->AI()->DoAction(ACTION_SPYBOT_HIDDEN);
                });
                break;
            default:
                break;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

private:
    std::vector<Creature*> Guards() const
    {
        std::vector<Creature*> guards;
        if (Creature* a = TheAttackBySpawnId(me->GetMap(), SPAWN_ROYAL_GUARD_A))
            guards.push_back(a);
        if (Creature* b = TheAttackBySpawnId(me->GetMap(), SPAWN_ROYAL_GUARD_B))
            guards.push_back(b);
        return guards;
    }

    void GuardFacing(float a, float b) const
    {
        if (Creature* guard = TheAttackBySpawnId(me->GetMap(), SPAWN_ROYAL_GUARD_A))
            guard->SetFacingTo(a);
        if (Creature* guard = TheAttackBySpawnId(me->GetMap(), SPAWN_ROYAL_GUARD_B))
            guard->SetFacingTo(b);
    }

    TaskScheduler _scheduler;
    bool _hidden = false;
};

struct classic_npc_lord_gregor_lescovar : public ScriptedAI
{
    classic_npc_lord_gregor_lescovar(Creature* creature) : ScriptedAI(creature) { }

    void DoAction(int32 action) override
    {
        if (action != ACTION_LESCOVAR_START || _walking)
            return;

        _walking = true;
        me->SetWalk(true);
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MovePath(PATH_LESCOVAR_TO_GARDEN, false);
    }

    void JustSummoned(Creature* summon) override
    {
        if (summon->GetEntry() == NPC_MARZON_THE_SILENT_BLADE)
            _marzonGuid = summon->GetGUID();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        ClassicScriptText(SAY_LESCOVAR_AGGRO, me);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (_hostile)
            if (Creature* tyrion = me->FindNearestCreature(NPC_TYRION, 120.0f))
                tyrion->AI()->DoAction(ACTION_LESCOVAR_DIED);
        _scheduler.CancelAll();
    }

    void WaypointReached(uint32 nodeId, uint32 pathId) override
    {
        if (pathId != PATH_LESCOVAR_TO_GARDEN)
            return;

        switch (nodeId)
        {
            case 6: // at the garden door: sends the guards away (VMaNGOS 175406)
                me->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
                ClassicScriptText(SAY_LESCOVAR_MEDITATION, me);
                _scheduler.Schedule(3s, [this](TaskContext /*context*/)
                {
                    if (Creature* guard = TheAttackBySpawnId(me->GetMap(), SPAWN_ROYAL_GUARD_A))
                        guard->SetFacingTo(1.09244f);
                    if (Creature* guard = TheAttackBySpawnId(me->GetMap(), SPAWN_ROYAL_GUARD_B))
                        guard->SetFacingTo(3.27191f);
                })
                .Schedule(5s, [this](TaskContext /*context*/)
                {
                    if (Creature* guard = TheAttackBySpawnId(me->GetMap(), SPAWN_ROYAL_GUARD_A))
                        guard->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);
                    if (Creature* guard = TheAttackBySpawnId(me->GetMap(), SPAWN_ROYAL_GUARD_B))
                    {
                        guard->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);
                        ClassicScriptText(SAY_GUARD_YES_SIR, guard);
                    }
                })
                .Schedule(7s, [this](TaskContext /*context*/)
                {
                    MoveGuards(TheAttackGuardHall, false);
                })
                .Schedule(19s, [this](TaskContext /*context*/)
                {
                    MoveGuards(TheAttackGuardPost, true);
                });
                break;
            case 7: // in the garden, waits 60 s for Marzon (VMaNGOS 175407)
                _scheduler.Schedule(1s, [this](TaskContext /*context*/)
                {
                    ClassicScriptText(EMOTE_LESCOVAR_WAITS, me);
                })
                .Schedule(23s, [this](TaskContext /*context*/)
                {
                    if (TempSummon* marzon = me->SummonCreature(NPC_MARZON_THE_SILENT_BLADE, TheAttackMarzonSpawn, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 120s))
                    {
                        marzon->CastSpell(marzon, SPELL_MARZON_SNEAK, true);
                        marzon->SetWalk(true);
                    }
                })
                .Schedule(24s, [this](TaskContext /*context*/)
                {
                    if (Creature* marzon = Marzon())
                        marzon->GetMotionMaster()->MovePoint(1, TheAttackMarzonWalk[0]);
                })
                .Schedule(29s, [this](TaskContext /*context*/)
                {
                    if (Creature* marzon = Marzon())
                        marzon->GetMotionMaster()->MovePoint(2, TheAttackMarzonWalk[1], true, TheAttackMarzonWalk[1].GetAbsoluteAngle(me));
                })
                .Schedule(34s, [this](TaskContext /*context*/)
                {
                    me->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
                    ClassicScriptText(SAY_LESCOVAR_WHAT_NEWS, me);
                })
                .Schedule(35s, [this](TaskContext /*context*/)
                {
                    if (Creature* marzon = Marzon())
                        marzon->RemoveAurasDueToSpell(SPELL_MARZON_SNEAK);
                })
                .Schedule(37s, [this](TaskContext /*context*/)
                {
                    if (Creature* marzon = Marzon())
                        ClassicScriptText(SAY_MARZON_VANCLEEF, marzon);
                })
                .Schedule(42s, [this](TaskContext /*context*/)
                {
                    me->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
                    ClassicScriptText(SAY_LESCOVAR_MEDDLER_SHAW, me);
                })
                .Schedule(46s, [this](TaskContext /*context*/)
                {
                    me->SetFaction(FACTION_DEFIAS_BROTHERHOOD);
                    _hostile = true;
                })
                .Schedule(47s, [this](TaskContext /*context*/)
                {
                    if (Creature* marzon = Marzon())
                    {
                        ClassicScriptText(SAY_MARZON_I_WILL_RETURN, marzon);
                        marzon->SetFaction(FACTION_DEFIAS_BROTHERHOOD);
                    }

                    // the conversation was overheard: quest event for every player close by who has the quest (VMaNGOS: 80 yards)
                    for (MapReference const& ref : me->GetMap()->GetPlayers())
                        if (Player* player = ref.GetSource())
                            if (player->IsWithinDistInMap(me, 80.0f) && player->GetQuestStatus(QUEST_THE_ATTACK) == QUEST_STATUS_INCOMPLETE)
                                player->AreaExploredOrEventHappens(QUEST_THE_ATTACK);

                    if (Creature* tyrion = me->FindNearestCreature(NPC_TYRION, 60.0f))
                        tyrion->AI()->DoAction(ACTION_CONVERSATION_DONE);
                });
                break;
            case 8: // the 60 s are over and he is still alive: back to normal (VMaNGOS 175408)
                _scheduler.CancelAll();
                me->RestoreFaction();
                _hostile = false;
                if (Creature* marzon = Marzon())
                    if (!marzon->IsInCombat())
                        marzon->DespawnOrUnsummon();
                if (Creature* tyrion = me->FindNearestCreature(NPC_TYRION, 60.0f))
                    tyrion->AI()->DoAction(ACTION_LESCOVAR_BACK);
                break;
            case 13: // home in the library
                _walking = false;
                me->SetFacingTo(me->GetHomePosition().GetOrientation());
                break;
            default:
                break;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);
        UpdateVictim();
    }

private:
    Creature* Marzon() const
    {
        Creature* marzon = ObjectAccessor::GetCreature(*me, _marzonGuid);
        return marzon && marzon->IsAlive() ? marzon : nullptr;
    }

    void MoveGuards(Position const (&to)[2], bool face)
    {
        ObjectGuid::LowType const ids[2] = { SPAWN_ROYAL_GUARD_A, SPAWN_ROYAL_GUARD_B };
        for (uint8 i = 0; i < 2; ++i)
        {
            if (Creature* guard = TheAttackBySpawnId(me->GetMap(), ids[i]))
            {
                guard->SetWalk(true);
                if (face)
                    guard->GetMotionMaster()->MovePoint(0, to[i], true, to[i].GetOrientation());
                else
                    guard->GetMotionMaster()->MovePoint(0, to[i]);
            }
        }
    }

    TaskScheduler _scheduler;
    ObjectGuid _marzonGuid;
    bool _walking = false;
    bool _hostile = false;
};

struct classic_npc_marzon_the_silent_blade : public ScriptedAI
{
    classic_npc_marzon_the_silent_blade(Creature* creature) : ScriptedAI(creature) { }

    void JustEngagedWith(Unit* /*who*/) override
    {
        ClassicScriptText(SAY_MARZON_AGGRO, me);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (me->GetFaction() == FACTION_DEFIAS_BROTHERHOOD)
            if (Creature* tyrion = me->FindNearestCreature(NPC_TYRION, 120.0f))
                tyrion->AI()->DoAction(ACTION_MARZON_DIED);
    }
};

void AddSC_classic_stormwind_city()
{
    RegisterCreatureAI(classic_npc_bartleby);
    RegisterCreatureAI(classic_npc_dashel_stonefist);
    RegisterCreatureAI(classic_npc_tyrion);
    RegisterCreatureAI(classic_npc_tyrions_spybot);
    RegisterCreatureAI(classic_npc_lord_gregor_lescovar);
    RegisterCreatureAI(classic_npc_marzon_the_silent_blade);
}
