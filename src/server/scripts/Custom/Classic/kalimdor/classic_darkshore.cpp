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

// Classic 1.60 port of VMaNGOS src/scripts/kalimdor/darkshore/darkshore.cpp (ScriptDev2 lineage, GPL-2)
// Ported: npc_kerlonian (5321), npc_prospector_remtravel (731), npc_threshwackonator (2078), npc_therylune (945),
//         npc_volcor (994, 995), npc_rabid_thistle_bear, npc_terenthis, go_beached_quest

#include "ScriptMgr.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "Item.h"
#include "MotionMaster.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"
#include "ScriptedFollowerAI.h"
#include "ScriptedGossip.h"
#include "SpellInfo.h"
#include "TemporarySummon.h"
#include "classic_script_text.h"

namespace
{
struct ClassicEscortPoint
{
    float X, Y, Z, O;
    uint32 WaitMs;
};

// VMaNGOS npc_escortAI reads its path from world.script_waypoint (by creature entry). TC EscortAI needs the path
// supplied by the script, so the VMaNGOS rows are embedded here (node ids are 0-based and contiguous, as TC requires).
template <std::size_t N, typename RunPredicate>
void LoadClassicEscortPath(EscortAI* ai, ClassicEscortPoint const (&points)[N], RunPredicate isRunning)
{
    ai->ResetPath();
    for (uint32 i = 0; i < N; ++i)
    {
        Optional<Milliseconds> waitTime;
        if (points[i].WaitMs)
            waitTime = Milliseconds(points[i].WaitMs);
        ai->AddWaypoint(i, points[i].X, points[i].Y, points[i].Z, points[i].O, waitTime, isRunning(i));
    }
}

// VMaNGOS script_waypoint entry 2917 (pointid 0..48, stored 0-based here)
ClassicEscortPoint const EscortPath2917[] =
{
    { 4675.81f, 598.615f, 17.6457f, 2.9069f, 0 },  // 0
    { 4672.84f, 599.325f, 16.4176f, 2.9069f, 0 },  // 1
    { 4663.45f, 607.43f, 10.4948f, 2.4295f, 0 },  // 2
    { 4655.97f, 613.761f, 8.5233f, 2.4392f, 0 },  // 3
    { 4640.8f, 623.999f, 8.3771f, 2.5479f, 0 },  // 4
    { 4631.68f, 630.801f, 6.415f, 2.5008f, 5000 },  // 5: SAY_REM_RAMP1_1
    { 4633.53f, 632.476f, 6.5098f, 0.7358f, 0 },  // 6
    { 4639.41f, 637.121f, 13.3381f, 0.6686f, 0 },  // 7
    { 4641.19f, 638.577f, 13.4229f, 0.6856f, 5000 },  // 8
    { 4641.19f, 638.577f, 13.4229f, 0.6856f, 3000 },  // 9: SAY_REM_RAMP1_2
    { 4639.63f, 637.234f, 13.3398f, 3.8524f, 0 },  // 10: ambush
    { 4631.68f, 630.801f, 6.415f, 3.8219f, 0 },  // 11
    { 4624.71f, 631.724f, 6.264f, 3.0099f, 0 },  // 12
    { 4623.53f, 629.719f, 6.2013f, 4.1804f, 5000 },  // 13: SAY_REM_BOOK
    { 4632.03f, 641.491f, 7.404f, 0.9454f, 0 },  // 14
    { 4628.08f, 646.288f, 7.551f, 2.2597f, 0 },  // 15
    { 4623.37f, 649.335f, 7.229f, 2.5674f, 0 },  // 16
    { 4625.96f, 643.41f, 6.641f, 5.1245f, 0 },  // 17
    { 4622.62f, 637.222f, 6.3129f, 4.2174f, 0 },  // 18: SAY_REM_TENT1_1
    { 4619.76f, 637.386f, 6.3121f, 3.0843f, 5000 },  // 19: SAY_REM_TENT1_2
    { 4620.03f, 637.368f, 6.3121f, 6.2166f, 0 },  // 20: ambush
    { 4624.15f, 637.56f, 6.3139f, 0.0466f, 0 },  // 21
    { 4622.97f, 634.016f, 6.295f, 4.391f, 0 },  // 22
    { 4616.93f, 630.303f, 6.2392f, 3.6928f, 0 },  // 23
    { 4614.55f, 616.983f, 5.6876f, 4.5356f, 0 },  // 24
    { 4610.28f, 610.029f, 5.4425f, 4.1617f, 0 },  // 25
    { 4601.15f, 604.112f, 2.0549f, 3.7166f, 0 },  // 26
    { 4573.92f, 582.566f, 0.7498f, 3.811f, 0 },  // 27
    { 4566.72f, 564.078f, 1.3431f, 4.341f, 0 },  // 28
    { 4599.08f, 572.583f, 1.2019f, 0.257f, 0 },  // 29
    { 4606.51f, 565.907f, 1.2697f, 5.5512f, 5000 },  // 30: SAY_REM_MOSS + EMOTE_REM_MOSS
    { 4606.51f, 565.907f, 1.2697f, 5.5512f, 1000 },  // 31: SAY_REM_MOSS_PROGRESS
    { 4599.08f, 572.583f, 1.2019f, 2.4096f, 0 },  // 32
    { 4566.72f, 564.078f, 1.3431f, 3.3986f, 0 },  // 33
    { 4553.62f, 572.625f, 1.2596f, 2.5635f, 0 },  // 34
    { 4550.97f, 567.231f, 1.3455f, 4.2557f, 0 },  // 35
    { 4549.4f, 567.61f, 1.3309f, 2.9047f, 5000 },  // 36: SAY_REM_PROGRESS
    { 4549.65f, 567.126f, 1.3474f, 5.1892f, 0 },  // 37: ambush
    { 4553.62f, 572.625f, 1.2596f, 0.9455f, 0 },  // 38
    { 4573.92f, 582.566f, 0.7498f, 0.4554f, 0 },  // 39
    { 4594.21f, 598.533f, 1.0341f, 0.6667f, 0 },  // 40
    { 4601.19f, 604.283f, 2.0602f, 0.6891f, 0 },  // 41
    { 4609.54f, 610.845f, 5.4022f, 0.6661f, 0 },  // 42
    { 4624.8f, 618.076f, 5.8515f, 0.4425f, 0 },  // 43
    { 4632.41f, 623.778f, 7.2862f, 0.643f, 0 },  // 44
    { 4645.92f, 621.984f, 8.58f, 6.1512f, 0 },  // 45
    { 4658.67f, 611.093f, 8.8918f, 5.5763f, 0 },  // 46
    { 4671.92f, 599.752f, 16.0124f, 5.5753f, 5000 },  // 47: SAY_REM_REMEMBER
    { 4676.98f, 600.65f, 17.8257f, 0.1756f, 5000 },  // 48: EMOTE_REM_END
};
// VMaNGOS script_waypoint entry 3584 (pointid 0..20, stored 0-based here)
ClassicEscortPoint const EscortPath3584[] =
{
    { 4520.4f, 420.235f, 33.5284f, 4.0904f, 2000 },  // 0
    { 4512.26f, 408.881f, 32.9308f, 4.0904f, 0 },  // 1
    { 4507.94f, 396.47f, 32.9476f, 4.3774f, 0 },  // 2
    { 4507.53f, 383.781f, 32.995f, 4.6801f, 0 },  // 3
    { 4512.1f, 374.02f, 33.166f, 5.1503f, 0 },  // 4
    { 4519.75f, 373.241f, 33.1574f, 6.1817f, 0 },  // 5
    { 4592.41f, 369.127f, 31.4893f, 6.2266f, 0 },  // 6
    { 4598.55f, 364.801f, 31.4947f, 5.6694f, 0 },  // 7
    { 4602.76f, 357.649f, 32.9265f, 5.2444f, 0 },  // 8
    { 4597.88f, 352.629f, 34.0317f, 3.9411f, 0 },  // 9
    { 4590.23f, 350.9f, 36.2977f, 3.3639f, 0 },  // 10
    { 4581.5f, 348.254f, 38.3878f, 3.4359f, 0 },  // 11
    { 4572.05f, 348.059f, 42.3539f, 3.1622f, 0 },  // 12
    { 4564.75f, 344.041f, 44.2463f, 3.6448f, 0 },  // 13
    { 4556.63f, 341.003f, 47.6755f, 3.4996f, 0 },  // 14
    { 4554.38f, 334.968f, 48.8003f, 4.3555f, 0 },  // 15
    { 4557.63f, 329.783f, 49.9532f, 5.2723f, 0 },  // 16
    { 4563.32f, 316.829f, 53.2409f, 5.1263f, 0 },  // 17
    { 4566.09f, 303.127f, 55.0396f, 4.9119f, 0 },  // 18
    { 4561.65f, 295.456f, 57.0984f, 4.1877f, 4000 },  // 19: SAY_THERYLUNE_FINISH
    { 4551.03f, 293.333f, 57.1534f, 3.3389f, 2000 },  // 20
};
// VMaNGOS script_waypoint entry 3692 (pointid 1..15, stored 0-based here)
ClassicEscortPoint const EscortPath3692[] =
{
    { 4608.43f, -6.32f, 69.74f, 0.0f, 1000 },  // 1: stand up
    { 4608.43f, -6.32f, 69.74f, 0.0f, 4000 },  // 2: SAY_START
    { 4604.54f, -5.17f, 69.51f, 2.8542f, 0 },  // 3
    { 4604.26f, -2.02f, 69.42f, 1.6595f, 0 },  // 4
    { 4607.75f, 3.79f, 70.13f, 1.0299f, 1000 },  // 5: first ambush
    { 4607.75f, 3.79f, 70.13f, 1.0299f, 0 },  // 6: SAY_FIRST_AMBUSH
    { 4619.77f, 27.47f, 70.4f, 1.1011f, 0 },  // 7
    { 4626.28f, 42.46f, 68.75f, 1.1611f, 0 },  // 8
    { 4633.13f, 51.17f, 67.4f, 0.9044f, 0 },  // 9
    { 4639.67f, 79.03f, 61.74f, 1.3402f, 0 },  // 10
    { 4647.54f, 94.25f, 59.92f, 1.0936f, 0 },  // 11: second ambush
    { 4682.08f, 113.47f, 54.83f, 0.5078f, 0 },  // 12
    { 4705.28f, 137.81f, 53.36f, 0.8094f, 0 },  // 13: last ambush
    { 4730.3f, 158.76f, 52.33f, 0.6971f, 0 },  // 14
    { 4756.47f, 195.65f, 53.61f, 0.9538f, 10000 },  // 15: SAY_END
};
}

enum DarkshoreFactions
{
    FACTION_ESCORT_A_NEUTRAL_PASSIVE = 10
};

/*####
# npc_kerlonian
####*/

enum KerlonianData
{
    SAY_KER_START           = 6540,
    EMOTE_KER_SLEEP_1       = 6811,
    EMOTE_KER_SLEEP_2       = 6542,
    EMOTE_KER_SLEEP_3       = 6541,
    SAY_KER_SLEEP_1         = 6813,
    SAY_KER_SLEEP_2         = 6543,
    SAY_KER_SLEEP_3         = 6544,
    SAY_KER_SLEEP_4         = 6545,
    EMOTE_KER_AWAKEN        = 6612,
    SAY_KER_ALERT_1         = 6867,
    SAY_KER_ALERT_2         = 6868,
    SAY_KER_END             = 6643,

    SPELL_SLEEP_VISUAL      = 25148,
    SPELL_AWAKEN            = 17536,

    QUEST_SLEEPER_AWAKENED  = 5321,

    NPC_LILADRIS            = 11219, // attackers entries unknown

    FACTION_KERLONIAN_SLEEP = 35
};

// TODO: make concept similar as "ringo" -escort. Find a way to run the scripted attacks, _if_ player are choosing road. (VMaNGOS comment)
struct classic_npc_kerlonian : public FollowerAI
{
    // Blizzard hotfix 6 November 2026: "Kerlonian Evershade falls asleep less often". No numbers were given; ours (VMaNGOS:
    // first nap after 10-45 s, then every 25-90 s) are doubled.
    static uint32 FirstNapTimer() { return urand(20000, 90000); }
    static uint32 NextNapTimer() { return urand(50000, 180000); }

    classic_npc_kerlonian(Creature* creature) : FollowerAI(creature), _fallAsleepTimer(FirstNapTimer()) { }

    void Reset() override
    {
        _fallAsleepTimer = FirstNapTimer();
    }

    void JustAppeared() override
    {
        me->SetImmuneToNPC(true);
        FollowerAI::JustAppeared();
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_SLEEPER_AWAKENED)
            return;

        me->SetStandState(UNIT_STAND_STATE_STAND);
        me->SetImmuneToNPC(false);
        ClassicScriptText(SAY_KER_START, me, player);
        StartFollow(player, FACTION_ESCORT_A_NEUTRAL_PASSIVE, quest->GetQuestId());
    }

    void MoveInLineOfSight(Unit* who) override
    {
        FollowerAI::MoveInLineOfSight(who);

        if (!me->GetVictim() && !HasFollowState(STATE_FOLLOW_COMPLETE) && who->GetEntry() == NPC_LILADRIS)
        {
            if (me->IsWithinDistInMap(who, INTERACTION_DISTANCE * 5))
            {
                if (Player* player = GetLeaderForFollower())
                {
                    if (player->GetQuestStatus(QUEST_SLEEPER_AWAKENED) == QUEST_STATUS_INCOMPLETE)
                        player->GroupEventHappens(QUEST_SLEEPER_AWAKENED, me);

                    ClassicScriptText(SAY_KER_END, me);
                }

                SetFollowComplete();
            }
        }
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        if (HasFollowState(STATE_FOLLOW_INPROGRESS | STATE_FOLLOW_PAUSED) && spellInfo->Id == SPELL_AWAKEN)
            ClearSleeping();
    }

    void SetSleeping()
    {
        SetFollowPaused(true);

        switch (urand(0, 2))
        {
            case 0: ClassicScriptText(EMOTE_KER_SLEEP_1, me); break;
            case 1: ClassicScriptText(EMOTE_KER_SLEEP_2, me); break;
            case 2: ClassicScriptText(EMOTE_KER_SLEEP_3, me); break;
        }

        switch (urand(0, 3))
        {
            case 0: ClassicScriptText(SAY_KER_SLEEP_1, me); break;
            case 1: ClassicScriptText(SAY_KER_SLEEP_2, me); break;
            case 2: ClassicScriptText(SAY_KER_SLEEP_3, me); break;
            case 3: ClassicScriptText(SAY_KER_SLEEP_4, me); break;
        }

        me->SetStandState(UNIT_STAND_STATE_SLEEP);
        me->SetFaction(FACTION_KERLONIAN_SLEEP);
        DoCastSelf(SPELL_SLEEP_VISUAL);
    }

    void ClearSleeping()
    {
        me->RemoveAurasDueToSpell(SPELL_SLEEP_VISUAL);
        me->SetStandState(UNIT_STAND_STATE_STAND);
        me->SetFaction(FACTION_ESCORT_A_NEUTRAL_PASSIVE);
        ClassicScriptText(EMOTE_KER_AWAKEN, me);
        SetFollowPaused(false);
    }

    void UpdateFollowerAI(uint32 diff) override
    {
        if (!UpdateVictim())
        {
            if (!HasFollowState(STATE_FOLLOW_INPROGRESS))
                return;

            if (!HasFollowState(STATE_FOLLOW_PAUSED))
            {
                if (_fallAsleepTimer < diff)
                {
                    SetSleeping();
                    _fallAsleepTimer = NextNapTimer();
                }
                else
                    _fallAsleepTimer -= diff;
            }

            return;
        }
    }

private:
    uint32 _fallAsleepTimer;
};

/*####
# npc_prospector_remtravel
####*/

enum ProspectorRemtravelData
{
    SAY_REM_START           = 925,
    SAY_REM_AGGRO           = 941,
    SAY_REM_RAMP1_1         = 926,
    SAY_REM_RAMP1_2         = 927,
    SAY_REM_BOOK            = 928,
    SAY_REM_TENT1_1         = 929,
    SAY_REM_TENT1_2         = 930,
    SAY_REM_MOSS            = 931,
    EMOTE_REM_MOSS          = 932,
    SAY_REM_MOSS_PROGRESS   = 933,
    SAY_REM_PROGRESS        = 935,
    SAY_REM_REMEMBER        = 936,
    EMOTE_REM_END           = 937,

    QUEST_ABSENT_MINDED_PT2 = 731,

    NPC_GRAVEL_SCOUT        = 2158,
    NPC_GRAVEL_BONE         = 2159,
    NPC_GRAVEL_GEO          = 2160
};

struct classic_npc_prospector_remtravel : public EscortAI
{
    classic_npc_prospector_remtravel(Creature* creature) : EscortAI(creature) { }

    void JustAppeared() override
    {
        me->SetImmuneToNPC(true);
        me->RestoreFaction();
        EscortAI::JustAppeared();
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_ABSENT_MINDED_PT2)
            return;

        me->SetFaction(FACTION_ESCORT_A_NEUTRAL_PASSIVE);
        ClassicScriptText(SAY_REM_START, me, player);
        me->HandleEmoteCommand(EMOTE_ONESHOT_QUESTION);
        me->SetImmuneToNPC(false);

        LoadClassicEscortPath(this, EscortPath2917, [](uint32) { return false; });
        Start(true, player->GetGUID(), quest, true);
    }

    void WaypointReached(uint32 waypointId, uint32 /*pathId*/) override
    {
        Player* player = GetPlayerForEscort();
        if (!player)
            return;

        switch (waypointId)
        {
            case 5:
                ClassicScriptText(SAY_REM_RAMP1_1, me, player);
                break;
            case 9:
                ClassicScriptText(SAY_REM_RAMP1_2, me, player);
                me->HandleEmoteCommand(EMOTE_ONESHOT_QUESTION);
                break;
            case 10:
                me->SummonCreature(NPC_GRAVEL_SCOUT, 4639.86f, 631.96f, 7.48f, 4.7f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 30s);
                break;
            case 13:
                ClassicScriptText(SAY_REM_BOOK, me, player);
                me->HandleEmoteCommand(EMOTE_ONESHOT_QUESTION);
                break;
            case 18:
                ClassicScriptText(SAY_REM_TENT1_1, me, player);
                break;
            case 19:
                ClassicScriptText(SAY_REM_TENT1_2, me, player);
                me->HandleEmoteCommand(EMOTE_ONESHOT_QUESTION);
                break;
            case 20:
                DoSpawnCreature(NPC_GRAVEL_SCOUT, -10.0f, 5.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 30s);
                DoSpawnCreature(NPC_GRAVEL_SCOUT, -10.0f, 7.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 30s);
                break;
            case 30:
                ClassicScriptText(SAY_REM_MOSS, me, player);
                ClassicScriptText(EMOTE_REM_MOSS, me, player);
                break;
            case 31:
                ClassicScriptText(SAY_REM_MOSS_PROGRESS, me, player);
                break;
            case 36:
                ClassicScriptText(SAY_REM_PROGRESS, me, player);
                me->HandleEmoteCommand(EMOTE_ONESHOT_QUESTION);
                break;
            case 37:
                me->SummonCreature(NPC_GRAVEL_BONE, 4564.11f, 553.67f, 5.21f, 2.26f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 30s);
                me->SummonCreature(NPC_GRAVEL_GEO, 4569.33f, 549.43f, 5.61f, 2.4f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 30s);
                break;
            case 47:
                ClassicScriptText(SAY_REM_REMEMBER, me, player);
                me->HandleEmoteCommand(EMOTE_ONESHOT_TALK);
                break;
            case 48:
                ClassicScriptText(EMOTE_REM_END, me, player);
                player->GroupEventHappens(QUEST_ABSENT_MINDED_PT2, me);
                break;
            default:
                break;
        }
    }

    void Reset() override { }

    void JustEngagedWith(Unit* who) override
    {
        if (urand(0, 1))
            ClassicScriptText(SAY_REM_AGGRO, me, who);
    }

    void JustSummoned(Creature* summoned) override
    {
        if (Player* player = GetPlayerForEscort())
            summoned->AI()->AttackStart(player);
    }
};

/*####
# npc_threshwackonator
####*/

enum ThreshwackonatorData
{
    EMOTE_START             = 3012,
    SAY_AT_CLOSE            = 2704,
    QUEST_GYROMAST_REV      = 2078,
    NPC_GELKAK              = 6667,
    FACTION_HOSTILE         = 14
};

#define GOSSIP_ITEM_INSERT_KEY "[PH] Insert key"

struct classic_npc_threshwackonator : public FollowerAI
{
    classic_npc_threshwackonator(Creature* creature) : FollowerAI(creature) { }

    void Reset() override { }

    void MoveInLineOfSight(Unit* who) override
    {
        FollowerAI::MoveInLineOfSight(who);

        if (!me->GetVictim() && !HasFollowState(STATE_FOLLOW_COMPLETE) && who->GetEntry() == NPC_GELKAK)
        {
            if (me->IsWithinDistInMap(who, 10.0f))
            {
                ClassicScriptText(SAY_AT_CLOSE, who);
                DoAtEnd();
            }
        }
    }

    void DoAtEnd()
    {
        me->SetFaction(FACTION_HOSTILE);

        if (Player* holder = GetLeaderForFollower())
            AttackStart(holder);

        SetFollowComplete();
    }

    bool OnGossipHello(Player* player) override
    {
        if (player->GetQuestStatus(QUEST_GYROMAST_REV) == QUEST_STATUS_INCOMPLETE)
            AddGossipItemFor(player, GossipOptionNpc::None, GOSSIP_ITEM_INSERT_KEY, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);

        SendGossipMenuFor(player, player->GetGossipTextId(me), me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
        if (action == GOSSIP_ACTION_INFO_DEF + 1)
        {
            CloseGossipMenuFor(player);
            ClassicScriptText(EMOTE_START, me);
            StartFollow(player);
        }
        return true;
    }
};

/*####
# npc_therylune
####*/

enum TheryluneData
{
    SAY_THERYLUNE_START         = 1189,
    SAY_THERYLUNE_FINISH        = 1188,

    NPC_THERYSIL                = 3585,

    QUEST_ID_THERYLUNE_ESCAPE   = 945,

    FACTION_THERYLUNE_ESCORT    = 79
};

struct classic_npc_therylune : public EscortAI
{
    classic_npc_therylune(Creature* creature) : EscortAI(creature) { }

    void Reset() override { }

    void JustAppeared() override
    {
        me->SetImmuneToNPC(true);
        me->RestoreFaction(); // VMaNGOS TEMPFACTION_RESTORE_RESPAWN
        EscortAI::JustAppeared();
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_ID_THERYLUNE_ESCAPE)
            return;

        // VMaNGOS walks, then SetRun() at point 19
        LoadClassicEscortPath(this, EscortPath3584, [](uint32 i) { return i > 19; });
        Start(true, player->GetGUID(), quest);
        ClassicScriptText(SAY_THERYLUNE_START, me, player);
        me->SetFaction(FACTION_THERYLUNE_ESCORT);
        me->SetImmuneToNPC(false);
    }

    void WaypointReached(uint32 waypointId, uint32 /*pathId*/) override
    {
        switch (waypointId)
        {
            case 17:
                if (Player* player = GetPlayerForEscort())
                    player->GroupEventHappens(QUEST_ID_THERYLUNE_ESCAPE, me);
                break;
            case 19:
                if (Player* player = GetPlayerForEscort())
                    ClassicScriptText(SAY_THERYLUNE_FINISH, me, player);
                // SetRun(): nodes after 19 are run nodes (see OnQuestAccept)
                break;
            default:
                break;
        }
    }
};

/*######
# npc_volcor
######*/

enum VolcorData
{
    SAY_START                   = 1237,
    SAY_END                     = 1243,
    SAY_FIRST_AMBUSH            = 1250,
    SAY_AGGRO_1                 = 1251,
    SAY_AGGRO_2                 = 1252,
    SAY_AGGRO_3                 = 1253,
    SAY_END_2                   = 1241,
    SAY_END_3                   = 1244,
    SAY_ESCAPE                  = 1236,

    NPC_BLACKWOOD_SHAMAN        = 2171,
    NPC_BLACKWOOD_URSA          = 2170,
    NPC_GRIMCLAW                = 3695,

    SPELL_MOONSTALKER_FORM      = 10849,

    WAYPOINT_ID_QUEST_STEALTH   = 16,
    CLASSIC_FACTION_FRIENDLY            = 35,

    QUEST_ESCAPE_THROUGH_FORCE   = 994,
    QUEST_ESCAPE_THROUGH_STEALTH = 995
};

struct ClassicSummonLocation
{
    float m_fX, m_fY, m_fZ, m_fO;
};

// Spawn locations
static ClassicSummonLocation const VolcorSpawnLocs[] =
{
    { 4630.2f, 22.6f, 70.1f, 2.4f },
    { 4603.8f, 53.5f, 70.4f, 5.4f },
    { 4627.5f, 100.4f, 62.7f, 5.8f },
    { 4692.8f, 75.8f, 56.7f, 3.1f },
    { 4747.8f, 152.8f, 54.6f, 2.4f },
    { 4711.7f, 109.1f, 53.5f, 2.4f },
};

// Escape Through Stealth
static ClassicSummonLocation const VolcorLocations[] =
{
    { 4604.54f, -5.17f, 69.51f, 0.0f },
    { 4604.26f, -2.02f, 69.42f, 0.0f },
    { 4607.75f, 3.79f, 70.13f, 0.0f },
    { 4607.75f, 3.79f, 70.13f, 0.0f },
    { 4619.77f, 27.47f, 70.40f, 0.0f },
    { 4640.33f, 33.74f, 68.22f, 0.0f }
};

struct classic_npc_volcor : public EscortAI
{
    classic_npc_volcor(Creature* creature) : EscortAI(creature), _questId(0), _escortFinished(false),
        _stealthDialogueStep(0), _stealthDialogueTimer(0), _forceDialogueStep(0), _forceDialogueTimer(0) { }

    void Reset() override
    {
        if (!HasEscortState(STATE_ESCORT_ESCORTING))
        {
            _questId = 0;
            _stealthDialogueStep = 0;
            _stealthDialogueTimer = 0;
        }
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        // shouldn't always use text on agro
        switch (urand(0, 4))
        {
            case 0: ClassicScriptText(SAY_AGGRO_1, me); break;
            case 1: ClassicScriptText(SAY_AGGRO_2, me); break;
            case 2: ClassicScriptText(SAY_AGGRO_3, me); break;
            default: break;
        }
    }

    void JustAppeared() override
    {
        me->SetImmuneToNPC(true);
        me->SetNpcFlag(NPCFlags(UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP));
        me->SetHomePosition(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0.0f);
        me->RestoreFaction(); // VMaNGOS TEMPFACTION_RESTORE_RESPAWN

        EscortAI::JustAppeared();
    }

    void MoveInLineOfSight(Unit* who) override
    {
        // No combat for this quest
        if (_questId == QUEST_ESCAPE_THROUGH_STEALTH)
            return;

        EscortAI::MoveInLineOfSight(who);
    }

    void JustSummoned(Creature* summoned) override
    {
        summoned->AI()->AttackStart(me);
    }

    void MovementInform(uint32 moveType, uint32 pointId) override
    {
        EscortAI::MovementInform(moveType, pointId);

        if (moveType != POINT_MOTION_TYPE)
            return;

        if (Player* player = GetPlayerForEscort())
        {
            if (_questId == QUEST_ESCAPE_THROUGH_STEALTH && player->GetQuestStatus(QUEST_ESCAPE_THROUGH_STEALTH) == QUEST_STATUS_INCOMPLETE)
            {
                if (pointId < 5)
                    me->GetMotionMaster()->MovePoint(pointId + 1, VolcorLocations[pointId + 1].m_fX, VolcorLocations[pointId + 1].m_fY, VolcorLocations[pointId + 1].m_fZ);
                else if (pointId == 5)
                {
                    player->GroupEventHappens(QUEST_ESCAPE_THROUGH_STEALTH, me);
                    me->DisappearAndDie();
                }
            }
        }
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_ESCAPE_THROUGH_FORCE || quest->GetQuestId() == QUEST_ESCAPE_THROUGH_STEALTH)
            StartEscort(player, quest);
    }

    // Wrapper to handle start function for both quests
    void StartEscort(Player* player, Quest const* quest)
    {
        me->SetStandState(UNIT_STAND_STATE_STAND);
        me->SetFacingToObject(player);
        _questId = quest->GetQuestId();

        if (quest->GetQuestId() == QUEST_ESCAPE_THROUGH_STEALTH)
        {
            // Note: faction may not be correct, but only this way works fine
            me->SetFaction(CLASSIC_FACTION_FRIENDLY);

            // VMaNGOS Start(bRun = true) then paused; the stealth route uses MovePoint (running)
            LoadClassicEscortPath(this, EscortPath3692, [](uint32) { return true; });
            Start(true, player->GetGUID(), quest);
            SetEscortPaused(true);
            me->SetWalk(false);
        }
        else
        {
            _escortFinished = false;
            _forceDialogueStep = 0;
            _forceDialogueTimer = 0;
            LoadClassicEscortPath(this, EscortPath3692, [](uint32) { return false; });
            Start(true, player->GetGUID(), quest);
            me->SetImmuneToNPC(false);
        }
    }

    void WaypointReached(uint32 waypointId, uint32 /*pathId*/) override
    {
        if (_questId == QUEST_ESCAPE_THROUGH_STEALTH)
            return;

        // VMaNGOS script_waypoint ids for this entry start at 1
        switch (waypointId + 1)
        {
            case 2:
                ClassicScriptText(SAY_START, me);
                break;
            case 5:
                me->SummonCreature(NPC_BLACKWOOD_SHAMAN, VolcorSpawnLocs[0].m_fX, VolcorSpawnLocs[0].m_fY, VolcorSpawnLocs[0].m_fZ, VolcorSpawnLocs[0].m_fO, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 20s);
                me->SummonCreature(NPC_BLACKWOOD_URSA, VolcorSpawnLocs[1].m_fX, VolcorSpawnLocs[1].m_fY, VolcorSpawnLocs[1].m_fZ, VolcorSpawnLocs[1].m_fO, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 20s);
                break;
            case 6:
                ClassicScriptText(SAY_FIRST_AMBUSH, me);
                break;
            case 11:
                me->SummonCreature(NPC_BLACKWOOD_SHAMAN, VolcorSpawnLocs[2].m_fX, VolcorSpawnLocs[2].m_fY, VolcorSpawnLocs[2].m_fZ, VolcorSpawnLocs[2].m_fO, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 20s);
                me->SummonCreature(NPC_BLACKWOOD_URSA, VolcorSpawnLocs[3].m_fX, VolcorSpawnLocs[3].m_fY, VolcorSpawnLocs[3].m_fZ, VolcorSpawnLocs[3].m_fO, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 20s);
                [[fallthrough]];
            case 13:
                me->SummonCreature(NPC_BLACKWOOD_URSA, VolcorSpawnLocs[4].m_fX, VolcorSpawnLocs[4].m_fY, VolcorSpawnLocs[4].m_fZ, VolcorSpawnLocs[4].m_fO, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 20s);
                me->SummonCreature(NPC_BLACKWOOD_URSA, VolcorSpawnLocs[5].m_fX, VolcorSpawnLocs[5].m_fY, VolcorSpawnLocs[5].m_fZ, VolcorSpawnLocs[5].m_fO, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 20s);
                break;
            case 15:
                if (Player* player = GetPlayerForEscort())
                    player->GroupEventHappens(QUEST_ESCAPE_THROUGH_FORCE, me);
                SetEscortPaused(true);
                _forceDialogueStep = 0;
                _forceDialogueTimer = 3000;
                _escortFinished = true;
                break;
            default:
                break;
        }
    }

    void FinishForceEvent()
    {
        _escortFinished = false;
        me->DespawnOrUnsummon();
        me->SetNpcFlag(NPCFlags(UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP));
    }

    void UpdateAI(uint32 diff) override
    {
        if (Player* player = GetPlayerForEscort())
        {
            if (_stealthDialogueStep < 4 && _questId == QUEST_ESCAPE_THROUGH_STEALTH && player->GetQuestStatus(QUEST_ESCAPE_THROUGH_STEALTH) == QUEST_STATUS_INCOMPLETE)
            {
                if (_stealthDialogueTimer < diff)
                {
                    switch (_stealthDialogueStep)
                    {
                        case 0:
                            me->HandleEmoteCommand(EMOTE_ONESHOT_BOW);
                            _stealthDialogueTimer = 1000;
                            ++_stealthDialogueStep;
                            break;
                        case 1:
                            ClassicScriptText(SAY_ESCAPE, me, player);
                            _stealthDialogueTimer = 4000;
                            ++_stealthDialogueStep;
                            break;
                        case 2:
                            DoCastSelf(SPELL_MOONSTALKER_FORM);
                            _stealthDialogueTimer = 4000;
                            ++_stealthDialogueStep;
                            break;
                        case 3:
                            me->GetMotionMaster()->MovePoint(0, VolcorLocations[0].m_fX, VolcorLocations[0].m_fY, VolcorLocations[0].m_fZ);
                            _stealthDialogueTimer = 30000;
                            ++_stealthDialogueStep;
                            break;
                        default:
                            break;
                    }
                }
                else
                    _stealthDialogueTimer -= diff;
            }
            else if (_escortFinished && _questId == QUEST_ESCAPE_THROUGH_FORCE)
            {
                if (_forceDialogueTimer < diff)
                {
                    switch (_forceDialogueStep)
                    {
                        case 0:
                            ClassicScriptText(SAY_END, me);
                            _forceDialogueTimer = 1000;
                            ++_forceDialogueStep;
                            break;
                        case 1:
                            if (Creature* grimclaw = me->FindNearestCreature(NPC_GRIMCLAW, 40.0f))
                            {
                                grimclaw->SetFacingToObject(me);
                                _forceDialogueTimer = 3000;
                                ++_forceDialogueStep;
                            }
                            else
                                FinishForceEvent();
                            break;
                        case 2:
                            if (Creature* grimclaw = me->FindNearestCreature(NPC_GRIMCLAW, 40.0f))
                            {
                                grimclaw->SetWalk(false);
                                grimclaw->GetMotionMaster()->MovePoint(0, me->GetPositionX() + 3.0f, me->GetPositionY() + 2.0f, me->GetPositionZ());
                                ClassicScriptText(SAY_END_2, grimclaw);
                                _forceDialogueTimer = 4000;
                                ++_forceDialogueStep;
                            }
                            else
                                FinishForceEvent();
                            break;
                        case 3:
                            ClassicScriptText(SAY_END_3, me);
                            _forceDialogueTimer = 5000;
                            ++_forceDialogueStep;
                            break;
                        case 4:
                            if (Creature* grimclaw = me->FindNearestCreature(NPC_GRIMCLAW, 40.0f))
                                grimclaw->DespawnOrUnsummon();
                            FinishForceEvent();
                            break;
                        default:
                            break;
                    }
                }
                else
                    _forceDialogueTimer -= diff;

                return;
            }
        }

        EscortAI::UpdateAI(diff);
    }

private:
    uint32 _questId;
    bool _escortFinished;
    uint16 _stealthDialogueStep;
    uint32 _stealthDialogueTimer;
    uint16 _forceDialogueStep;
    uint32 _forceDialogueTimer;
};

/*####
# npc_rabid_thistle_bear (Alita)
####*/

enum RabidThistleBearData
{
    SPELL_TRAPPED_BEAR                  = 9439,
    NPC_CAPTURED_RABID_THISTLE_BEAR     = 11836
};

struct classic_npc_rabid_thistle_bear : public FollowerAI
{
    classic_npc_rabid_thistle_bear(Creature* creature) : FollowerAI(creature), _capturedTimer(-1) { }

    void Reset() override { }

    void UpdateFollowerAI(uint32 diff) override
    {
        int32 signedDiff = int32(diff);
        if (_capturedTimer >= 0)
        {
            if (_capturedTimer < signedDiff)
            {
                _capturedTimer = -1;
                me->DisappearAndDie();
            }
            else
                _capturedTimer -= signedDiff;
        }

        if (!UpdateVictim())
            return;

        // TODO(classic): VMaNGOS also runs the creature_spells list here (UpdateSpellsList); TC has no creature spell lists.
    }

    void StartFollowing(Player* player)
    {
        _capturedTimer = 300000;
        StartFollow(player);
    }

    void JustAppeared() override
    {
        FollowerAI::JustAppeared();
        _capturedTimer = -1;
    }

    // VMaNGOS EffectDummyCreature_npc_rabid_thistle_bear (SPELL_TRAPPED_BEAR, effect 0)
    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_TRAPPED_BEAR)
            return;

        Player* player = caster ? caster->ToPlayer() : nullptr;
        if (!player)
            return;

        me->UpdateEntry(NPC_CAPTURED_RABID_THISTLE_BEAR);
        player->KilledMonsterCredit(me->GetEntry(), me->GetGUID());
        EnterEvadeMode();
        StartFollowing(player);
    }

private:
    int32 _capturedTimer;
};

/*####
# npc_terenthis
####*/

enum TerenthisData
{
    NPC_SENTINEL_SELARIN        = 3694,
    QUEST_HOW_BIG_A_THREAT_2    = 985
};

// TODO(classic): VMaNGOS QuestRewarded_npc_terenthis only decides whether the DB quest_end_scripts (994/995 -> script 994,
// 985 -> script 985) run afterwards. TC has no quest end scripts, so those DB scripts must be converted separately
// (SmartAI). The sentinel summon below is the C++ part of the event.
struct classic_npc_terenthis : public ScriptedAI
{
    classic_npc_terenthis(Creature* creature) : ScriptedAI(creature) { }

    void OnQuestReward(Player* /*player*/, Quest const* quest, LootItemType /*type*/, uint32 /*opt*/) override
    {
        if (quest->GetQuestId() == QUEST_ESCAPE_THROUGH_FORCE || quest->GetQuestId() == QUEST_ESCAPE_THROUGH_STEALTH)
        {
            if (me->FindNearestCreature(NPC_SENTINEL_SELARIN, 40.0f))
                return; // prevent starting db script if sentinel is already spawned

            // VMaNGOS TEMPSUMMON_TIMED_COMBAT_OR_DEAD_DESPAWN (not in TC) -> closest TC type
            if (TempSummon* sentinel = me->SummonCreature(NPC_SENTINEL_SELARIN, 6409.01f, 381.597f, 13.7997f, 1.0f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 120s))
                sentinel->SetWalk(false); // let quest_end_script take over from here
        }
        // QUEST_HOW_BIG_A_THREAT_2: VMaNGOS only blocks the quest_end_script when Grimclaw is already spawned (no C++ action)
    }
};

/*####
# go_beached_quest
####*/

// would have thought taking it out of quest_template would have been necessary
// but turns out the quest adds the item afterwards with its check(so never adds),
// so it's fine to leave it in quest_template. (VMaNGOS comment)
struct classic_go_beached_quest : public GameObjectAI
{
    classic_go_beached_quest(GameObject* go) : GameObjectAI(go) { }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        uint32 id = 0;
        uint32 count = 1;
        uint32 questId = quest->GetQuestId();
        // Beached Sea turtle (12289)
        if (questId == 4722 || questId == 4727 || questId == 4732)
            id = 12289;
        // Beached Sea turtle, different item. (12292)
        else if (questId == 4725 || questId == 4731)
            id = 12292;
        // Beached Sea Creature (12242)
        else if (questId == 4723 || questId == 4728 || questId == 4730 || questId == 4733)
            id = 12242;

        if (!id)
            return;

        // the whole point is to NOT check the current item count
        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, id, count);
        if (msg == EQUIP_ERR_OK)
        {
            if (Item* item = player->StoreNewItem(dest, id, true))
                player->SendNewItem(item, count, true, false);
        }
    }
};

void AddSC_classic_darkshore()
{
    RegisterCreatureAI(classic_npc_kerlonian);
    RegisterCreatureAI(classic_npc_prospector_remtravel);
    RegisterCreatureAI(classic_npc_threshwackonator);
    RegisterCreatureAI(classic_npc_therylune);
    RegisterCreatureAI(classic_npc_volcor);
    RegisterCreatureAI(classic_npc_rabid_thistle_bear);
    RegisterCreatureAI(classic_npc_terenthis);
    RegisterGameObjectAI(classic_go_beached_quest);
}
