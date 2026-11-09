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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/eastern_plaguelands/naxxramas/boss_kelthuzad.cpp (ScriptDev2 lineage, GPL-2)
// Scripts: boss_kelthuzad, unstoppable_abomination_ai, soldier_frozen_wastes_ai, soul_weaver_ai, mob_guardian_icecrownAI,
//          mob_shadow_fissure, spell_kelthuzad_void_blast (27812)
// Also implements classic_instance_naxxramas_InstanceScript::OnKTAreaTrigger (as VMaNGOS did in this file).
//
// NOT ported: spell_chains_of_kelthuzad (28410 aura script). The VMaNGOS script only toggled the VMaNGOS-specific
// PlayerAI::enablePositiveSpells flag so charmed players would heal/buff; TC master charmed players are driven by
// SimpleCharmedPlayerAI which has its own spell selection and no such switch.

#include "ScriptMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "EventMap.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "MapReference.h"
#include "Player.h"
#include "QuaternionData.h"
#include "Random.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "ThreatManager.h"
#include "classic_naxxramas.h"
#include "classic_script_text.h"
#include <algorithm>
#include <cmath>
#include <list>
#include <utility>
#include <vector>

namespace
{
enum KelthuzadData
{
    SAY_KT_SUMMON_MINIONS               = 12999,         // start of phase 1

    SAY_KT_AGGRO1                       = 12995,         // pray for mercy
    SAY_KT_AGGRO2                       = 12996,         // Scream your dying breath!
    SAY_KT_AGGRO3                       = 12997,         // The end is upon you!

    SAY_KT_SLAY1                        = 13021,
    EMOTE_KT_SLAY                       = 13022,

    SAY_KT_DEATH                        = 13019,

    SAY_KT_CHAIN1                       = 13017,         // Your soul, is bound to me now!
    SAY_KT_CHAIN2                       = 13018,         // there will be  no escape
    SAY_KT_FROST_BLAST                  = 13020,         // I will freeze the blood in your veins!

    SAY_KT_REQUEST_AID                  = 12998,         // Master! I require aid!
    SAY_KT_ANSWER_REQUEST               = 12994,         // Very well... warriors of the frozen wastes, rise up! I command you to fight, kill, and die for your master. Let none survive...

    SAY_KT_SPECIAL1_MANA_DET            = 13492,
    // VMaNGOS never found vanilla broadcast text ids for these (kept as sd2 script_texts entries there);
    // no matching text exists in the 1.12 broadcast_text data, so the lines are skipped in this port:
    // SAY_KT_SPECIAL3_MANA_DET         = -1533107,      // Enough! I grow tired of these distractions! (unused in code)
    // SAY_KT_SPECIAL2_DISPELL          = -1533108,      // Fools, you have spread your powers too thin. Be free, my minions!
    // EMOTE_KT_GUARDIAN                = -1533134,      // at each guardian summon, cant see that it's used in vanilla

    SPELL_KT_VISUAL_CHANNEL             = 29423,         // channeled throughout phase one

    //spells to be casted
    SPELL_KT_FROST_BOLT                 = 28478,
    SPELL_KT_FROST_BOLT_NOVA            = 28479,

    SPELL_KT_CHAINS_OF_KELTHUZAD           = 28408,
    SPELL_KT_CHAINS_OF_KELTHUZAD_SCALE     = 28409,
    SPELL_KT_CHAINS_OF_KELTHUZAD_EFFECTS   = 28410,

    SPELL_KT_MANA_DETONATION            = 27819,
    SPELL_KT_MANA_DETONATION_DAMAGE     = 27820,
    SPELL_KT_SHADOW_FISSURE             = 27810,
    SPELL_KT_VOID_BLAST                 = 27812,
    SPELL_KT_FROST_BLAST                = 27808,
    SPELL_KT_BERSERK                    = 28498,

    SPELL_KT_DISPELL_SHACKLES           = 28471          // not used, doing it "manually"
};

enum KTAddSpells
{
    // Guardian of Icecrown
    SPELL_KT_BLOOD_TAP = 28470,

    // Abomination
    SPELL_KT_MORTAL_WOUND = 28467
};

enum KTEvents
{
    // phase one
    EVENT_KT_SKELETON = 1,
    EVENT_KT_SOUL_WEAVER,
    EVENT_KT_ABOMINATION,
    EVENT_KT_PHASE_TWO_INTRO,
    EVENT_KT_PHASE_TWO_START,
    EVENT_KT_DESPAWN_PORTAL,
    EVENT_KT_PUT_IN_COMBAT,

    // phase two
    EVENT_KT_FROSTBOLT_VOLLEY,
    EVENT_KT_FROST_BLAST,
    EVENT_KT_FROSTBOLT,
    EVENT_KT_SHADOW_FISSURE,
    EVENT_KT_DETONATE_MANA,
    EVENT_KT_CHAINS,

    // phase three
    EVENT_KT_REQUEST_REPLY,
    EVENT_KT_SUMMON_GUARDIAN
};

// the shiny thing in center that despawns after pull
constexpr float ktPullPortal[3] = { 3716.379883f, -5106.779785f, 132.9f };

// Center position of each alcove
constexpr uint32 KT_NUM_ALCOVES = 7;
constexpr float ktAlcoves[KT_NUM_ALCOVES][2] =
{
    { 3768.40f, -5072.00f},
    { 3729.30f, -5044.10f},
    { 3683.00f, -5054.05f},
    { 3654.15f, -5093.48f},
    { 3664.55f, -5140.50f},
    { 3704.00f, -5170.00f},
    { 3751.95f, -5158.90f}
};

// z-coordinate in the alcoves
constexpr float ktAlcoveZ = 143.5f;

// number of soulweavers total, one in each alcove
constexpr uint32 KT_NUM_SOULWEAVER = 7;
// each soulweaver position
constexpr float ktSoulweaverPos[KT_NUM_SOULWEAVER][2] =
{
    {3754.95f, -5164.93f},
    {3701.89f, -5176.95f},
    {3656.83f, -5145.56f},
    {3647.53f, -5093.56f},
    {3678.48f, -5050.46f},
    {3730.87f, -5035.93f},
    {3774.78f, -5067.68f},
};

// number of abominations, 3 in each alcove
constexpr uint32 KT_NUM_ABOM = 21;
// each abomination position
constexpr float ktAbomPos[KT_NUM_ABOM][2] =
{
    {3740.70f, -5160.89f},
    {3756.42f, -5151.09f},
    {3748.99f, -5155.72f},

    {3694.11f, -5163.96f},
    {3713.90f, -5168.14f},
    {3704.76f, -5166.21f},

    {3661.65f, -5132.06f},
    {3672.37f, -5147.84f},
    {3666.83f, -5139.67f},

    {3658.81f, -5086.46f},
    {3654.80f, -5104.04f},
    {3656.76f, -5095.47f},

    {3691.83f, -5052.45f},
    {3675.15f, -5062.94f},
    {3683.48f, -5057.71f},

    {3738.15f, -5050.12f},
    {3717.76f, -5046.03f},
    {3728.48f, -5047.99f},

    {3772.53f, -5083.21f},
    {3760.03f, -5064.65f},
    {3765.85f, -5073.22f}
};

// total number of soulweaver and abomination waves
constexpr uint32 KT_NUM_UNDEAD_SPAWNS = 14;

// milliseconds since pull for each abomination spawn
constexpr uint32 ktAbominationSpawnMs[KT_NUM_UNDEAD_SPAWNS] =
{
    44000,
    72000,
    100000,
    130000,
    153000,
    176000,
    193000,
    212000,
    232000,
    252000,
    268000,
    285000,
    300000,
    318000,
};

// milliseconds since pull for each soulweaver spawn
constexpr uint32 ktSoulweaverSpawnMs[KT_NUM_UNDEAD_SPAWNS] =
{
    44000,
    68000,
    97000,
    130000,
    155000,
    170000,
    190000,
    213000,
    235000,
    256000,
    271000,
    285000,
    294000,
    300000,
};

constexpr uint32 KT_NUM_WINDOW_PORTALS = 4;
constexpr float ktWindowPortals[KT_NUM_WINDOW_PORTALS][2] =
{
    {3760.57f, -5173.93f},
    {3700.14f, -5185.68f},
    {3732.62f, -5027.67f},
    {3783.36f, -5062.35f}
};

//todo: no idea what the pull range should be
constexpr float KT_ALCOVE_ADD_PULL_RADIUS = 30.0f;
}

struct classic_kt_p1AddAI : public ScriptedAI
{
    classic_kt_p1AddAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        me->SetNoSearchAssistance(true);
        hasAggroed = false;
    }

    bool hasAggroed;

    void Reset() override = 0;

    void ActualAttack(Unit* target)
    {
        me->GetThreatManager().AddThreat(target, 300.0f);
        ScriptedAI::AttackStart(target);
        hasAggroed = true;
    }

    void JustEngagedWith(Unit* /*pWho*/) override
    {
        // want to prevent the creature from aggroing unless we explicitly do it through base class
    }

    void AttackStart(Unit* pWho) override
    {
        if (hasAggroed)
            ScriptedAI::AttackStart(pWho);
        // want to prevent the creature from aggroing unless we explicitly do it through this class
        else if (me->GetDistance2d(pWho) < KT_ALCOVE_ADD_PULL_RADIUS)
        {
            ActualAttack(pWho);
        }
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (hasAggroed)
        {
            ScriptedAI::MoveInLineOfSight(pWho);
        }
        else if (me->IsHostileTo(pWho) && me->GetDistance2d(pWho) < KT_ALCOVE_ADD_PULL_RADIUS) //todo: no idea what the pull range should be
        {
            ScriptedAI::MoveInLineOfSight(pWho);
        }
    }

    void SpellHit(WorldObject* pCaster, SpellInfo const* /*spellInfo*/) override
    {
        if (!hasAggroed)
        {
            if (Unit* pUnit = pCaster->ToUnit())
                ActualAttack(pUnit);
        }
    }
};

struct classic_boss_kelthuzad : public ScriptedAI
{
    classic_boss_kelthuzad(Creature* pCreature) : ScriptedAI(pCreature), m_pInstance(GetClassicNaxxInstance(pCreature))
    {
        if (!m_pInstance)
            TC_LOG_ERROR("scripts", "classic_boss_kelthuzad::ctor failed to get classic_instance_naxxramas");
        // VMaNGOS: pCreature->SetCreatureSummonLimit(240); TC master has no per-creature summon limit
    }

    classic_instance_naxxramas_InstanceScript* m_pInstance;

    std::vector<std::pair<ObjectGuid, int>> guardians;
    std::vector<ObjectGuid> p1_adds;

    int32 p1Timer = 0;
    bool hasPutInCombat = false;
    bool p3Started = false;
    EventMap events;
    ObjectGuid pullPortalGuid;
    uint32 numSummonedGuardians = 0;
    uint32 nextBanshee = 30000, nextAbom = 30000;
    uint32 numSkeletons = 0, numAboms = 0, numBanshees = 0;
    uint32 enrageTimer = 1000 * 60 * 19;
    uint32 timeSinceLastFrostBlast = 0;
    uint32 timeSinceLastShadowFissure = 0;
    uint32 timeSinceLastAEFrostBolt = 0;
    uint32 killSayTimer = 0;

    void Reset() override
    {
        me->SetFullHealth();
        events.Reset();
        // no info on enragetimer in vanilla, but wotlk has a 19min enrage and uses a spell from 1.11 dbc
        enrageTimer = 1000 * 60 * 19;
        numSkeletons = 0;
        numAboms = 0;
        numBanshees = 0;
        nextBanshee = 30000;
        nextAbom = 30000;
        p3Started = false;
        numSummonedGuardians = 0;
        timeSinceLastFrostBlast = 0;
        timeSinceLastShadowFissure = 0;
        timeSinceLastAEFrostBolt = 0;
        killSayTimer = 0;
        hasPutInCombat = false;

        me->RemoveAurasDueToSpell(SPELL_KT_VISUAL_CHANNEL);
        // VMaNGOS UNIT_FLAG_IMMUNE_TO_PLAYER | UNIT_FLAG_NOT_SELECTABLE
        me->SetImmuneToPC(true);
        me->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);

        EvadeAllGuardians();

        SummonPullPortalIfNeeded();
    }

    void SummonPullPortalIfNeeded()
    {
        if (!pullPortalGuid.IsEmpty() || !me->IsInWorld())
            return;

        if (GameObject* pGO = me->SummonGameObject(GO_HUB_PORTAL, ktPullPortal[0], ktPullPortal[1], ktPullPortal[2], 0.0f, QuaternionData::fromEulerAnglesZYX(0.0f, 0.0f, 0.0f), 0s))
        {
            pGO->SetObjectScale(2.0f);
            pullPortalGuid = pGO->GetGUID();
            // (VMaNGOS removed and re-added the GO to the map to force a scale update; not needed in TC)
        }
    }

    // VMaNGOS spawned the pull portal from Reset() in the AI constructor; TC: once the creature is in the world
    void JustAppeared() override
    {
        ScriptedAI::JustAppeared();
        SummonPullPortalIfNeeded();
    }

    void KilledUnit(Unit* /*pVictim*/) override
    {
        if (!killSayTimer)
        {
            ClassicScriptText(urand(0, 1) ? SAY_KT_SLAY1 : EMOTE_KT_SLAY, me);
            killSayTimer = 5000;
        }
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        ClassicScriptText(SAY_KT_DEATH, me);
        if (m_pInstance)
            m_pInstance->SetData(TYPE_KELTHUZAD, DONE);

        EvadeAllGuardians();
    }

    void MoveInLineOfSight(Unit* /*pWho*/) override { }

    void AttackStart(Unit* who) override
    {
        if (me->IsImmuneToPC())
            return;

        ScriptedAI::AttackStart(who);
    }

    void JustEngagedWith(Unit* /*pWho*/) override
    {
        if (me->IsImmuneToPC())
            return;

        DoZoneInCombat();
    }

    bool CheckForEnemyPlayers()
    {
        // VMaNGOS GetAlivePlayerListInRange(me, players, 75.0f) without gamemasters
        bool anyPlayers = false;
        for (MapReference const& ref : me->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || !player->IsAlive() || player->IsGameMaster() || !me->IsWithinDistInMap(player, 75.0f))
                continue;

            anyPlayers = true;
            me->EngageWithTarget(player);
        }

        return anyPlayers;
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
        {
            m_pInstance->SetData(TYPE_KELTHUZAD, NOT_STARTED);
            m_pInstance->ToggleKelThuzadWindows(false);
        }
        DespawnAllIntroCreatures();
        EvadeAllGuardians();
    }

    void EvadeAllGuardians()
    {
        if (!m_pInstance)
            return;

        for (auto const& guardian : guardians)
        {
            if (Creature* pCreature = m_pInstance->GetCreature(guardian.first))
            {
                if (pCreature->IsAIEnabled())
                    pCreature->AI()->EnterEvadeMode();
            }
        }
    }

    void DespawnAllIntroCreatures()
    {
        if (m_pInstance)
        {
            for (ObjectGuid const& guid : p1_adds)
            {
                if (Creature* pSoldier = m_pInstance->GetCreature(guid))
                    pSoldier->DespawnOrUnsummon();
            }
        }
        p1_adds.clear();
    }

    void StartEncounter()
    {
        if (!m_pInstance)
            return;

        m_pInstance->ToggleKelThuzadWindows(false);
        me->SetFullHealth();
        // on pull there are in each alcove:
        // 3 aboms
        // 1 banshee
        // around 10 skeletons?

        // during p1, 14 aboms, 14 banshees and 120 skeletons should attack, gradually faster
        m_pInstance->SetData(TYPE_KELTHUZAD, IN_PROGRESS);
        ClassicScriptText(SAY_KT_SUMMON_MINIONS, me);
        DoCastAOE(SPELL_KT_VISUAL_CHANNEL);

        events.ScheduleEvent(EVENT_KT_DESPAWN_PORTAL,  Seconds(7));
        events.ScheduleEvent(EVENT_KT_PUT_IN_COMBAT,   Seconds(20));
        events.ScheduleEvent(EVENT_KT_PHASE_TWO_INTRO, Minutes(5) + Seconds(20));

        p1Timer = 320000;
        events.ScheduleEvent(EVENT_KT_SKELETON, Seconds(20));
        for (uint32 i : ktAbominationSpawnMs)
            events.ScheduleEvent(EVENT_KT_ABOMINATION, Milliseconds(i));
        for (uint32 i : ktSoulweaverSpawnMs)
            events.ScheduleEvent(EVENT_KT_SOUL_WEAVER, Milliseconds(i));

        m_pInstance->DoUseDoorOrButton(pullPortalGuid);

        for (auto const& alcove : ktAlcoves)
        {
            for (int j = 0; j < 10; j++)
            {
                double angle = rand_norm() * 2.0 * M_PI;
                double relDistance = rand_norm() + rand_norm();
                if (relDistance > 1)
                    relDistance = 1 - relDistance;
                float const x = alcove[0];
                float const y = alcove[1];
                float const radius = 14.0f;
                float thisX = x + float(std::sin(angle) * relDistance * radius);
                float thisY = y + float(std::cos(angle) * relDistance * radius);
                if (Creature* pCreature = me->SummonCreature(NPC_SOLDIER_FROZEN, thisX, thisY, ktAlcoveZ, frand(0, float(M_PI) * 2),
                    TEMPSUMMON_MANUAL_DESPAWN))
                {
                    p1_adds.push_back(pCreature->GetGUID());
                    pCreature->SetHomePosition(x, y, ktAlcoveZ, me->GetOrientation());
                    pCreature->SetWanderDistance(radius);
                }
            }
        }
        for (auto const& position : ktAbomPos)
        {
            if (Creature* pCreature = me->SummonCreature(NPC_UNSTOPPABLE_ABOM, position[0], position[1], ktAlcoveZ, frand(0, float(M_PI) * 2),
                TEMPSUMMON_MANUAL_DESPAWN))
            {
                p1_adds.push_back(pCreature->GetGUID());
                pCreature->SetWanderDistance(5.0f);
            }
        }
        for (auto const& position : ktSoulweaverPos)
        {
            if (Creature* pCreature = me->SummonCreature(NPC_SOUL_WEAVER, position[0], position[1], ktAlcoveZ, frand(0, float(M_PI) * 2),
                TEMPSUMMON_MANUAL_DESPAWN))
            {
                p1_adds.push_back(pCreature->GetGUID());
                pCreature->SetWanderDistance(5.0f);
            }
        }
    }

    // VMaNGOS SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER): while Kel'Thuzad is
    // immune to players (phase 1) TC keeps no reliable threat list, so pick a random alive player in the room instead.
    Unit* SelectRandomAlivePlayer(float range) const
    {
        std::vector<Player*> players;
        for (MapReference const& ref : me->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (player && player->IsAlive() && !player->IsGameMaster() && me->IsWithinDistInMap(player, range))
                players.push_back(player);
        }

        if (players.empty())
            return nullptr;

        return players[urand(0, uint32(players.size() - 1))];
    }

    bool SpawnAndSendP1Creature(uint32 type)
    {
        float const* spawnLoc = ktAlcoves[urand(0, KT_NUM_ALCOVES - 1)];
        if (Unit* pTarget = SelectRandomAlivePlayer(100.0f))
        {
            float spawnAng = 3.14f + pTarget->GetAbsoluteAngle(spawnLoc[0], spawnLoc[1]);
            if (Creature* pAdd = me->SummonCreature(type, spawnLoc[0], spawnLoc[1], ktAlcoveZ, spawnAng, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 1000ms))
            {
                CreatureAI::DoZoneInCombat(pAdd);
                if (classic_kt_p1AddAI* addAI = dynamic_cast<classic_kt_p1AddAI*>(pAdd->AI()))
                    addAI->ActualAttack(pTarget);
                return true;
            }
        }
        return false;
    }

    void UpdateP1(uint32 diff)
    {
        if (!m_pInstance || m_pInstance->GetData(TYPE_KELTHUZAD) != IN_PROGRESS)
            return;

        p1Timer -= int32(diff);

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_KT_DESPAWN_PORTAL:
                    if (GameObject* pGO = m_pInstance->GetGameObject(pullPortalGuid))
                    {
                        pullPortalGuid.Clear();
                        pGO->Delete();
                    }
                    break;
                case EVENT_KT_PUT_IN_COMBAT:
                    DoZoneInCombat();
                    CheckForEnemyPlayers();
                    hasPutInCombat = true;
                    break;
                case EVENT_KT_SKELETON:
                {
                    if (numSkeletons < 120)
                    {
                        if (SpawnAndSendP1Creature(NPC_SOLDIER_FROZEN))
                        {
                            uint32 repeat_next = std::max(uint32(3750 - 25 * numSkeletons), uint32(2000));
                            events.Repeat(Milliseconds(repeat_next));
                            ++numSkeletons;
                        }
                        else
                            events.Repeat(100ms);
                    }
                    break;
                }
                case EVENT_KT_ABOMINATION:
                {
                    SpawnAndSendP1Creature(NPC_UNSTOPPABLE_ABOM);
                    ++numAboms;
                    break;
                }
                case EVENT_KT_SOUL_WEAVER:
                {
                    SpawnAndSendP1Creature(NPC_SOUL_WEAVER);
                    ++numBanshees;
                    break;
                }
                case EVENT_KT_PHASE_TWO_INTRO:
                {
                    // ToDo: slightly hard to figure the exact delay between this event (the yell and despawn of adds)
                    // until he engages. Most vanilla timers say 20 seconds, but he always engages earlier than that.
                    // Seen it at around 10 seconds in a german video (https://www.youtube.com/watch?v=QafmVXupeHc),
                    // and as late as ~17-18 sec in another one (https://www.youtube.com/watch?v=6RpqjIFbQYw https://www.youtube.com/watch?v=wSQtlvVebm0)
                    events.Reset();
                    events.ScheduleEvent(EVENT_KT_PHASE_TWO_START, Seconds(15));
                    if (numBanshees < 14)
                    {
                        SpawnAndSendP1Creature(NPC_SOUL_WEAVER);
                        ++numBanshees;
                    }
                    if (numAboms < 14)
                    {
                        SpawnAndSendP1Creature(NPC_UNSTOPPABLE_ABOM);
                        ++numAboms;
                    }
                    if (numSkeletons < 120)
                    {
                        SpawnAndSendP1Creature(NPC_SOLDIER_FROZEN);
                        ++numSkeletons;
                    }

                    ClassicScriptText(urand(SAY_KT_AGGRO1, SAY_KT_AGGRO3), me);
                    me->RemoveAurasDueToSpell(SPELL_KT_VISUAL_CHANNEL);
                    DespawnAllIntroCreatures();
                    break;
                }
                case EVENT_KT_PHASE_TWO_START:
                    // engage!
                    events.Reset();
                    events.ScheduleEvent(EVENT_KT_FROSTBOLT,        Seconds(10));
                    events.ScheduleEvent(EVENT_KT_SHADOW_FISSURE,   Seconds(14));
                    events.ScheduleEvent(EVENT_KT_DETONATE_MANA,    Seconds(20));
                    events.ScheduleEvent(EVENT_KT_FROSTBOLT_VOLLEY, Seconds(30));
                    events.ScheduleEvent(EVENT_KT_FROST_BLAST,      Seconds(50));
                    events.ScheduleEvent(EVENT_KT_CHAINS,           Seconds(60));
                    me->RemoveAurasDueToSpell(SPELL_KT_VISUAL_CHANNEL);
                    me->SetImmuneToPC(false);
                    me->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                    me->InterruptNonMeleeSpells(true);

                    ResetThreatList();
                    DoZoneInCombat();
                    if (Unit* pUnit = SelectTarget(SelectTargetMethod::MinDistance, 0))
                        AttackStart(pUnit);

                    break;
            }
        }
    }

    void DoChains()
    {
        if (DoCastSelf(SPELL_KT_CHAINS_OF_KELTHUZAD) != SPELL_CAST_OK)
        {
            events.Repeat(Seconds(2));
            return;
        }

        ResetThreatList();
        ClassicScriptText(urand(0, 1) ? SAY_KT_CHAIN1 : SAY_KT_CHAIN2, me);
        // Wowwiki the useless has this on 60sec cd,
        // but sampling a random vanilla video, the shortest cd was 60sec, with one as high as 142sec.
        // Setting this to 60-75 as a slight buff
        events.Repeat(Seconds(urand(60, 75)));
    }

    void UpdateP2P3(uint32 diff)
    {
        timeSinceLastFrostBlast += diff;
        timeSinceLastShadowFissure += diff;
        timeSinceLastAEFrostBolt += diff;

        if (!UpdateVictim())
            return;

        if (me->GetHealthPct() < 40.0f && !p3Started)
        {
            ClassicScriptText(SAY_KT_REQUEST_AID, me);
            events.ScheduleEvent(EVENT_KT_REQUEST_REPLY, Seconds(3));
            if (m_pInstance)
                m_pInstance->ToggleKelThuzadWindows(true);
            events.ScheduleEvent(EVENT_KT_SUMMON_GUARDIAN, Seconds(5));
            p3Started = true;
        }

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_KT_REQUEST_REPLY:
                    if (m_pInstance)
                        m_pInstance->DoOrSimulateScriptTextForThisInstance(SAY_KT_ANSWER_REQUEST, NPC_LICH_KING);
                    break;
                case EVENT_KT_SUMMON_GUARDIAN:
                {
                    // Can be seen in videos they spawn with some delay between eachother.
                    // Not found a clear pattern, but a good guess is one spawn every 5 sec until all 5 has spawned.
                    if (numSummonedGuardians < 5)
                    {
                        // we can re-use the soulweave positions for where to spawn the guardians
                        // todo: is it completely random, or do we avoid re-using the same alcove twize?
                        int portalIndex = urand(0, KT_NUM_WINDOW_PORTALS - 1);
                        float const* pos = ktWindowPortals[portalIndex];

                        if (Creature* pCreature = me->SummonCreature(NPC_GUARDIAN, pos[0], pos[1], ktAlcoveZ, 0.0f, TEMPSUMMON_MANUAL_DESPAWN))
                        {
                            guardians.push_back(std::make_pair(pCreature->GetGUID(), portalIndex));
                            ++numSummonedGuardians;
                            events.Repeat(Seconds(7));
                            CreatureAI::DoZoneInCombat(pCreature);
                            if (Unit* pTarget = SelectTarget(SelectTargetMethod::Random, 0))
                            {
                                if (pCreature->IsAIEnabled())
                                    pCreature->AI()->AttackStart(pTarget);
                            }
                        }
                        else
                            events.Repeat(100ms);
                    }
                    break;
                }
                case EVENT_KT_FROSTBOLT_VOLLEY:
                {
                    if (timeSinceLastFrostBlast < 5000)
                    {
                        events.Repeat(Milliseconds(5000 - timeSinceLastFrostBlast));
                        break;
                    }
                    else if (timeSinceLastShadowFissure < 5000)
                    {
                        events.Repeat(Milliseconds(5000 - timeSinceLastShadowFissure));
                        break;
                    }
                    if (DoCastSelf(SPELL_KT_FROST_BOLT_NOVA) == SPELL_CAST_OK)
                    {
                        events.Repeat(Seconds(urand(15, 17)));
                        timeSinceLastAEFrostBolt = 0;
                    }
                    else
                        events.Repeat(Seconds(1));
                    break;
                }
                case EVENT_KT_FROST_BLAST:
                {
                    if (timeSinceLastShadowFissure < 5000)
                    {
                        events.Repeat(Milliseconds(5000 - timeSinceLastShadowFissure));
                        break;
                    }
                    else if (timeSinceLastAEFrostBolt < 8000)
                    {
                        events.Repeat(Milliseconds(8000 - timeSinceLastAEFrostBolt));
                        break;
                    }
                    if (me->IsNonMeleeSpellCast(false))
                    {
                        events.Repeat(Seconds(1));
                        break;
                    }
                    if (Unit* pUnit = SelectTarget(SelectTargetMethod::Random, 1))
                    {
                        if (DoCast(pUnit, SPELL_KT_FROST_BLAST) == SPELL_CAST_OK)
                        {
                            events.Repeat(Seconds(urand(30, 60)));
                            timeSinceLastFrostBlast = 0;
                            if (urand(0, 1))
                                ClassicScriptText(SAY_KT_FROST_BLAST, me);
                            break;
                        }
                        else
                            events.Repeat(Seconds(1));
                    }
                    else
                        events.Repeat(Seconds(1));
                    break;
                }
                case EVENT_KT_FROSTBOLT:
                {
                    events.Repeat(Seconds(urand(5, 7))); // todo: this is guesswork
                    DoCastVictim(SPELL_KT_FROST_BOLT);
                    break;
                }
                case EVENT_KT_SHADOW_FISSURE:
                {
                    if (timeSinceLastFrostBlast < 5000)
                    {
                        events.Repeat(Milliseconds(5000 - timeSinceLastFrostBlast));
                        break;
                    }
                    else if (timeSinceLastAEFrostBolt < 8000)
                    {
                        events.Repeat(Milliseconds(8000 - timeSinceLastAEFrostBolt));
                        break;
                    }
                    if (me->IsNonMeleeSpellCast(false))
                    {
                        events.Repeat(Seconds(2));
                        break;
                    }
                    if (Unit* pUnit = SelectTarget(SelectTargetMethod::Random, 1))
                    {
                        if (DoCast(pUnit, SPELL_KT_SHADOW_FISSURE) == SPELL_CAST_OK)
                        {
                            events.Repeat(Seconds(urand(10, 20)));
                            timeSinceLastShadowFissure = 0;
                        }
                        else
                            events.Repeat(Seconds(1));
                    }
                    else
                        events.Repeat(Seconds(1));
                    break;
                }
                case EVENT_KT_DETONATE_MANA:
                {
                    if (me->IsNonMeleeSpellCast(false))
                    {
                        events.Repeat(Seconds(2));
                        break;
                    }

                    events.Repeat(Seconds(urand(20, 25)));

                    // VMaNGOS SELECT_FLAG_POWER_MANA | SELECT_FLAG_PLAYER
                    if (Unit* pUnit = SelectTarget(SelectTargetMethod::Random, 0, [](Unit* target)
                        {
                            return target->GetTypeId() == TYPEID_PLAYER && target->GetPowerType() == POWER_MANA;
                        }))
                    {
                        if (DoCast(pUnit, SPELL_KT_MANA_DETONATION) == SPELL_CAST_OK)
                        {
                            if (urand(0, 1))
                                ClassicScriptText(SAY_KT_SPECIAL1_MANA_DET, me);
                            break;
                        }
                    }
                    break;
                }
                case EVENT_KT_CHAINS:
                    DoChains();
                    break;
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!m_pInstance)
            return;

        if (hasPutInCombat)
        {
            // won't have a victim if we are in p1, even if selectHostileTarget returns true, so check that before return
            if (!me->IsImmuneToPC())
            {
                if (!UpdateVictim())
                    return;
            }
            else if (!CheckForEnemyPlayers())
            {
                // VMaNGOS SelectHostileTarget() found nothing to fight during phase 1 -> evade
                EnterEvadeMode(EvadeReason::NoHostiles);
                return;
            }
        }

        if (m_pInstance->GetData(TYPE_KELTHUZAD) != IN_PROGRESS)
            return;

        killSayTimer -= std::min(killSayTimer, diff);

        if (enrageTimer < diff)
        {
            me->CastSpell(me, SPELL_KT_BERSERK, true);
            enrageTimer = 300000;
        }
        else
            enrageTimer -= diff;

        events.Update(diff);

        if (me->IsImmuneToPC())
            UpdateP1(diff);
        else
        {
            if (!m_pInstance->HandleEvadeOutOfHome(me))
                return;
            UpdateP2P3(diff);
        }
    }
};

struct classic_unstoppable_abomination_ai : public classic_kt_p1AddAI
{
    classic_unstoppable_abomination_ai(Creature* pCreature) : classic_kt_p1AddAI(pCreature)
    {
        Reset();
    }

    uint32 mortalWoundTimer = 7500;

    void Reset() override
    {
        mortalWoundTimer = 7500;
        me->SetMaxHealth(90000);
        me->SetHealth(90000);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        if (mortalWoundTimer < diff)
        {
            if (me->GetVictim() && me->IsWithinMeleeRange(me->GetVictim()))
                if (DoCastVictim(SPELL_KT_MORTAL_WOUND) == SPELL_CAST_OK)
                    mortalWoundTimer = 7500;
        }
        else
            mortalWoundTimer -= diff;
    }
};

struct classic_soldier_frozen_wastes_ai : public classic_kt_p1AddAI
{
    classic_soldier_frozen_wastes_ai(Creature* pCreature) : classic_kt_p1AddAI(pCreature)
    {
        Reset();
    }

    void Reset() override
    {
        me->SetMaxHealth(2000);
        me->SetHealth(2000);
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;

        // to avoid melees being able to dps while casters hold aggro, this is most likely a logic that's supposed to exist
        if (!me->IsWithinMeleeRange(me->GetVictim()))
        {
            if (Unit* pNearest = SelectTarget(SelectTargetMethod::MinDistance, 0))
            {
                if (me->GetVictim() != pNearest && me->IsWithinMeleeRange(pNearest))
                    ScriptedAI::AttackStart(pNearest);
            }
        }
    }
};

struct classic_soul_weaver_ai : public classic_kt_p1AddAI
{
    classic_soul_weaver_ai(Creature* pCreature) : classic_kt_p1AddAI(pCreature)
    {
        Reset();
    }

    bool hasHitSomeone = false; // unused in VMaNGOS as well

    void Reset() override
    {
        hasHitSomeone = false;
        me->SetMaxHealth(70000);
        me->SetHealth(70000);
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;

        // to avoid melees being able to dps while casters hold aggro, this is most likely a logic that's supposed to exist
        if (!me->IsWithinMeleeRange(me->GetVictim()))
        {
            if (Unit* pNearest = SelectTarget(SelectTargetMethod::MinDistance, 0))
            {
                if (me->GetVictim() != pNearest && me->IsWithinMeleeRange(pNearest))
                    ScriptedAI::AttackStart(pNearest);
            }
        }
    }
};

struct classic_mob_guardian_icecrownAI : public ScriptedAI
{
    classic_mob_guardian_icecrownAI(Creature* pCreature) : ScriptedAI(pCreature), m_pInstance(GetClassicNaxxInstance(pCreature)),
        bloodTapTimer(15000) { }

    classic_instance_naxxramas_InstanceScript* m_pInstance;
    uint32 bloodTapTimer;

    void Reset() override
    {
        // Not sure if this was 18 or 15sec cd in vanilla, was 15 in wotlk. But making it 15 as we already
        // overpower last phase anyway
        bloodTapTimer = 15000;
    }

    void JustReachedHome() override
    {
        me->DespawnOrUnsummon();
    }

    void DispellShackle(Creature* pC)
    {
        if (pC->HasAura(9484))
            pC->RemoveAurasDueToSpell(9484);
        else if (pC->HasAura(9485))
            pC->RemoveAurasDueToSpell(9485);
        else if (pC->HasAura(10955))
            pC->RemoveAurasDueToSpell(10955);
    }

    void SpellHit(WorldObject* /*pCaster*/, SpellInfo const* spell) override
    {
        // if hit by any shackle spell we check how many other guardians are shackled.
        // If more than 3, we release everyone.
        switch (spell->Id)
        {
            case 10955:
            case 9485:
            case 9484:
            {
                std::list<Creature*> guardians;
                me->GetCreatureListWithEntryInGrid(guardians, NPC_GUARDIAN, 130.0f);
                uint32 numShackled = 0;
                for (Creature* pC : guardians)
                {
                    if (pC->HasAura(9484) || pC->HasAura(9485) || pC->HasAura(10955))
                        ++numShackled;
                }

                if (numShackled > 3)
                {
                    // TODO(classic): VMaNGOS made Kel'Thuzad yell "Fools, you have spread your powers too thin.
                    // Be free, my minions!" here (sd2 text -1533108); no vanilla broadcast text exists for it.
                    for (Creature* pC : guardians)
                        DispellShackle(pC);
                }
                break;
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        if (bloodTapTimer < diff)
        {
            if (DoCastVictim(SPELL_KT_BLOOD_TAP) == SPELL_CAST_OK)
                bloodTapTimer = 15000;
        }
        else bloodTapTimer -= diff;
    }
};

struct classic_mob_shadow_fissure : public ScriptedAI
{
    classic_mob_shadow_fissure(Creature* pCreature) : ScriptedAI(pCreature), timer(3000), haveCasted(false) { }

    uint32 timer;
    bool haveCasted;

    void Reset() override
    {
        timer = 3000;
        haveCasted = false;
    }

    void JustEngagedWith(Unit* /*pWho*/) override { }
    void AttackStart(Unit* /*pWho*/) override { }
    void MoveInLineOfSight(Unit* /*pWho*/) override { }

    void UpdateAI(uint32 diff) override
    {
        if (haveCasted)
            return;

        if (timer < diff)
        {
            me->CastSpell(me, SPELL_KT_VOID_BLAST, true);
            haveCasted = true;
            me->DespawnOrUnsummon(2250ms);
        }
        else
            timer -= diff;
    }
};

// VMaNGOS implemented this instance member here as well (needs boss_kelthuzadAI)
void classic_instance_naxxramas_InstanceScript::OnKTAreaTrigger(AreaTriggerEntry const* /*pAT*/)
{
    if (GetData(TYPE_KELTHUZAD) != NOT_STARTED)
        return;

    if (Creature* pKT = GetSingleCreatureFromStorage(NPC_KELTHUZAD))
    {
        if (classic_boss_kelthuzad* ai = dynamic_cast<classic_boss_kelthuzad*>(pKT->AI()))
            ai->StartEncounter();
    }
}

// 27812 - Void Blast (Kel'Thuzad)
class classic_spell_kelthuzad_void_blast : public SpellScript
{
    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        // If target has the chains of kel'thuzad aura the spell should not do any damage.
        // This check should not be necessary as you should be friendly to the caster of
        // the spell, but some bug caused players to take damage anyway, and even if that is fixed,
        // this is a safetycheck.
        if (Unit* target = GetHitUnit())
            if (target->HasAura(SPELL_KT_CHAINS_OF_KELTHUZAD_EFFECTS))
                SetHitDamage(0);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(classic_spell_kelthuzad_void_blast::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// 27819 - Detonate Mana (Kel'Thuzad)
// VMaNGOS SpellAuras.cpp (periodic trigger, 27819): each tick burns half of the carrier's current mana and deals that much as
// Mana Detonation (27820) to everyone around the carrier. The stock TrinityCore script on this id burned a tenth of the maximum
// mana and dealt ten times that (the whole pool); the vanilla aura carries no trigger spell of its own, so without a script
// nothing happens at all.
class classic_spell_kelthuzad_detonate_mana : public AuraScript
{
    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_KT_MANA_DETONATION_DAMAGE });
    }

    void HandleScript(AuraEffect const* aurEff)
    {
        PreventDefaultAction();

        Unit* target = GetTarget();
        if (int32 const mana = int32(target->GetPower(POWER_MANA) / 2))
        {
            int32 const burned = -target->ModifyPower(POWER_MANA, -mana);
            CastSpellExtraArgs args(aurEff);
            args.AddSpellBP0(burned);
            target->CastSpell(target, SPELL_KT_MANA_DETONATION_DAMAGE, args);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(classic_spell_kelthuzad_detonate_mana::HandleScript, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

void AddSC_classic_boss_kelthuzad()
{
    RegisterCreatureAI(classic_boss_kelthuzad);
    RegisterCreatureAI(classic_unstoppable_abomination_ai);
    RegisterCreatureAI(classic_soldier_frozen_wastes_ai);
    RegisterCreatureAI(classic_soul_weaver_ai);
    RegisterCreatureAI(classic_mob_guardian_icecrownAI);
    RegisterCreatureAI(classic_mob_shadow_fissure);
    RegisterSpellScript(classic_spell_kelthuzad_void_blast);
    RegisterSpellScript(classic_spell_kelthuzad_detonate_mana);
}
