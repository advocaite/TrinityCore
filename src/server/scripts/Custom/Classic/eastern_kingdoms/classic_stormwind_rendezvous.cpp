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

// Classic 1.60 port of VMaNGOS src/scripts/eastern_kingdoms/stormwind_city/quest_stormwind_rendezvous.cpp (+ .h)
// Quest support: 6402 (Stormwind Rendezvous) -> 6403 (The Great Masquerade): Squire Rowe signals Marshal Windsor, who
// walks the player from the gates to the keep, where Lady Katrana Prestor is revealed as Onyxia (npc_squire_rowe,
// npc_reginald_windsor).
//
// Ported interface between the two: Rowe calls Windsor->AI()->SetGUID(playerGuid, WINDSOR_GUID_PLAYER) and
// SetGUID(roweGuid, WINDSOR_GUID_ROWE) (VMaNGOS sets npc_reginald_windsorAI::playerGUID / m_squireRoweGuid directly);
// Windsor calls Rowe->AI()->DoAction(ACTION_ROWE_RESET) where VMaNGOS PokeRowe() calls ResetCreature().
// TC does not update a corpse's AI, so the part VMaNGOS runs from UpdateAI_corpse (the keep settling down after Windsor's
// death) is scheduled on the creature's event queue, which keeps running while the corpse lies there.
// at_stormwind_gates (pre-1.12 Windsor spawn) is not ported (not requested).

#include "ScriptMgr.h"
#include "DB2Stores.h"
#include "GameObject.h"
#include "GossipDef.h"
#include "Group.h"
#include "Map.h"
#include "MotionMaster.h"
#include "Player.h"
#include "QuaternionData.h"
#include "QuestDef.h"
#include "Random.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellInfo.h"
#include "TemporarySummon.h"
#include "ThreatManager.h"
#include "WorldSession.h"
#include "classic_script_text.h"
#include <array>
#include <cmath>
#include <list>
#include <string>

enum StormwindRendezvousData
{
    QUEST_STORMWIND_RENDEZVOUS      = 6402,
    QUEST_THE_GREAT_MASQUERADE      = 6403,

    NPC_REGINALD_WINDSOR            = 12580,
    NPC_ONYXIA_ELITE_GUARD          = 12739,
    NPC_ANDUIN_WRYNN                = 1747,
    NPC_BOLVAR_FORDRAGON            = 1748,
    NPC_KATRANA_PRESTOR             = 1749,
    NPC_STORMWIND_CITY_GUARD        = 68,
    NPC_STORMWIND_CITY_PATROL       = 1976,
    NPC_MERCUTIO                    = 12581,
    NPC_MARCUS_JONATHAN             = 466,
    NPC_STORMWIND_ROYAL_GUARD       = 1756,
    NPC_LADY_ONYXIA                 = 12756,
    NPC_ROWE                        = 17804,

    FACTION_BOLVAR_COMBAT           = 11,

    GOSSIP_ROWE_COMPLETED           = 9066,
    GOSSIP_ROWE_READY               = 9065,
    GOSSIP_ROWE_BUSY                = 9064,
    GOSSIP_ROWE_NOTHING             = 9063,
    GOSSIP_WINDSOR_MENU             = 5633,
    GOSSIP_WINDSOR_READY_TEXT       = 8256,     // broadcast text of the "I'm ready" option

    GO_FLARE_OF_JUSTICE             = 181987,

    SAY_SIGNAL_SENT                 = 14389,
    SAY_HISS                        = 8245,
    SAY_WINDSOR1                    = 8091,
    SAY_WINDSOR2                    = 8090,
    SAY_WINDSOR3                    = 8107,
    SAY_WINDSOR4                    = 8109,
    SAY_ONYXIA1                     = 8119,
    SAY_MARCUS1                     = 8121,
    SAY_WINDSOR5                    = 8123,
    SAY_WINDSOR6                    = 8133,
    SAY_MARCUS2                     = 8124,
    SAY_MARCUS3                     = 8125,
    SAY_MARCUS4                     = 8132,
    SAY_WINDSOR7                    = 8126,
    SAY_WINDSOR8                    = 8134,
    SAY_MARCUS5                     = 8127,
    SAY_MARCUS6                     = 8128,
    SAY_MARCUS7                     = 8129,
    SAY_MARCUS8                     = 8130,
    SAY_WINDSOR9                    = 8205,
    SAY_WINDSOR10                   = 8206,
    SAY_WINDSOR11                   = 8207,
    SAY_WINDSOR12                   = 8208,
    SAY_WINDSOR13                   = 8210,
    SAY_BOLVAR1                     = 8212,
    SAY_WINDSOR14                   = 8211,
    SAY_ONYXIA2                     = 8214,
    SAY_ONYXIA3                     = 8215,
    SAY_ONYXIA4                     = 8216,
    SAY_WINDSOR15                   = 8218,
    SAY_WINDSOR16                   = 8226,
    SAY_WINDSOR17                   = 8227,
    SAY_WINDSOR18                   = 8219,
    SAY_WINDSOR19                   = 8228,
    SAY_BOLVAR2                     = 8236,
    SAY_ONYXIA5                     = 8235,
    SAY_BOLVAR3                     = 8237,
    SAY_ONYXIA6                     = 8238,
    SAY_ONYXIA7                     = 8239,
    SAY_WINDSOR20                   = 8247,
    SAY_ONYXIA8                     = 8246,
    SAY_ONYXIA9                     = 8248,
    SAY_BOLVAR4                     = 8266,
    SAY_BOLVAR5                     = 8249,
    SAY_WINDSOR21                   = 8250,
    SAY_WINDSOR22                   = 8251,

    MOUNT_WINDSOR                   = 2410,

    SPELL_GREATER_INVISIBILITY      = 16380,
    SPELL_INVISIBILITY              = 23452,
    SPELL_WINDSOR_DEATH             = 20465,
    SPELL_WINDSOR_READ_TABLETS      = 20358,
    SPELL_PRESTOR_DESPAWNS          = 20466,
    SPELL_WINDSOR_DISMISS_HORSE     = 20000,    // summons Mercutio 12581
    SPELL_DEATH_TOUCH               = 5,

    DISPLAY_INVISIBLE               = 11686,

    // interface between Rowe and Windsor, see the header comment
    WINDSOR_GUID_PLAYER             = 0,
    WINDSOR_GUID_ROWE               = 1,
    ACTION_ROWE_RESET               = 1
};

Position const RoweWaypoints[] =
{
    { -9058.07f, 441.32f, 93.06f, 3.84f },
    { -9084.88f, 419.23f, 92.42f, 3.83f }
};

Position const WindsorFirstWaypoint = { -9050.406250f, 443.974792f, 93.056458f, 0.659825f };
Position const WindsorSummon = { -9148.40f, 371.32f, 91.0f, 0.70f };

// VMaNGOS QUEST_STATUS_COMPLETE also covers rewarded quests; TC reports those as QUEST_STATUS_REWARDED
static bool ClassicIsQuestDone(Player const* player, uint32 questId)
{
    QuestStatus status = player->GetQuestStatus(questId);
    return status == QUEST_STATUS_COMPLETE || status == QUEST_STATUS_REWARDED;
}

/*
 * Squire Rowe
 */

struct classic_npc_squire_rowe : public ScriptedAI
{
    classic_npc_squire_rowe(Creature* creature) : ScriptedAI(creature), m_uiTimer(2000), m_uiStep(0), m_bEventProcessed(false), m_bWindsorUp(false) { }

    uint32 m_uiTimer;
    uint32 m_uiStep;
    bool m_bEventProcessed;
    bool m_bWindsorUp;
    ObjectGuid m_playerGuid;

    void Reset() override { }

    void ResetCreature()
    {
        m_playerGuid.Clear();
        m_uiTimer = 2000;
        m_uiStep = 0;
        m_bEventProcessed = false;
        me->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
        m_bWindsorUp = false;
    }

    // VMaNGOS constructor / JustRespawned -> ResetCreature()
    void JustAppeared() override
    {
        ScriptedAI::JustAppeared();
        ResetCreature();
    }

    // VMaNGOS npc_reginald_windsorAI::PokeRowe()
    void DoAction(int32 action) override
    {
        if (action == ACTION_ROWE_RESET)
            ResetCreature();
    }

    void MovementInform(uint32 type, uint32 pointId) override
    {
        if (!m_bEventProcessed || type != POINT_MOTION_TYPE)
            return;

        switch (pointId)
        {
            case 1:
                me->GetMotionMaster()->MovePoint(2, RoweWaypoints[1].GetPositionX(), RoweWaypoints[1].GetPositionY(), RoweWaypoints[1].GetPositionZ());
                break;
            case 2:
                me->HandleEmoteCommand(EMOTE_ONESHOT_KNEEL);
                m_uiTimer = 5000;
                ++m_uiStep;
                break;
            case 4:
                me->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                ClassicScriptText(SAY_SIGNAL_SENT, me);
                m_bEventProcessed = false;
                break;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (m_bEventProcessed)
        {
            if (m_uiTimer < diff)
            {
                switch (m_uiStep)
                {
                    case 0:
                        me->SetSpeedRate(MOVE_RUN, 1.1f);
                        me->GetMotionMaster()->MovePoint(1, RoweWaypoints[0].GetPositionX(), RoweWaypoints[0].GetPositionY(), RoweWaypoints[0].GetPositionZ());
                        m_uiTimer = 1000;
                        ++m_uiStep;
                        break;
                    case 2:
                        me->SummonGameObject(GO_FLARE_OF_JUSTICE, -9095.839844f, 411.178986f, 92.244499f, 2.303830f,
                            QuaternionData(0.0f, 0.0f, 0.913545f, 0.406738f), 10s);
                        me->GetMotionMaster()->MovePoint(3, RoweWaypoints[0].GetPositionX(), RoweWaypoints[0].GetPositionY(), RoweWaypoints[0].GetPositionZ());
                        m_uiTimer = 1500;
                        ++m_uiStep;
                        break;
                    case 3:
                        if (Creature* windsor = me->SummonCreature(NPC_REGINALD_WINDSOR, WindsorSummon, TEMPSUMMON_MANUAL_DESPAWN, 90min))
                        {
                            windsor->setActive(true); // VMaNGOS summons him as an active object

                            if (CreatureAI* windsorAI = windsor->AI())
                            {
                                if (!m_playerGuid.IsEmpty())
                                    windsorAI->SetGUID(m_playerGuid, WINDSOR_GUID_PLAYER);

                                windsorAI->SetGUID(me->GetGUID(), WINDSOR_GUID_ROWE);
                            }

                            m_bWindsorUp = true;
                            windsor->Mount(MOUNT_WINDSOR);
                            windsor->SetWalk(false);
                            windsor->SetSpeedRate(MOVE_RUN, 1.0f);
                            windsor->GetMotionMaster()->MovePoint(0, WindsorFirstWaypoint.GetPositionX(), WindsorFirstWaypoint.GetPositionY(), WindsorFirstWaypoint.GetPositionZ());
                            windsor->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
                        }
                        ++m_uiStep;
                        break;
                    case 4:
                    {
                        Position home = me->GetRespawnPosition();
                        me->GetMotionMaster()->MovePoint(4, home.GetPositionX(), home.GetPositionY(), home.GetPositionZ(), true, home.GetOrientation());
                        ++m_uiStep;
                        break;
                    }
                }
            }
            else
                m_uiTimer -= diff;
        }
        else
            ScriptedAI::UpdateAI(diff);
    }

    bool OnGossipHello(Player* player) override
    {
        if (ClassicIsQuestDone(player, QUEST_STORMWIND_RENDEZVOUS) && !ClassicIsQuestDone(player, QUEST_THE_GREAT_MASQUERADE))
        {
            if (!m_bWindsorUp)
            {
                AddGossipItemFor(player, GossipOptionNpc::None, "Let Marshal Windsor know that I am ready.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
                SendGossipMenuFor(player, GOSSIP_ROWE_READY, me->GetGUID());
            }
            else
                SendGossipMenuFor(player, GOSSIP_ROWE_BUSY, me->GetGUID());
        }
        else if (ClassicIsQuestDone(player, QUEST_THE_GREAT_MASQUERADE))
            SendGossipMenuFor(player, GOSSIP_ROWE_COMPLETED, me->GetGUID());
        else
            SendGossipMenuFor(player, GOSSIP_ROWE_NOTHING, me->GetGUID());

        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = GetGossipActionFor(player, gossipListId);
        ClearGossipMenuFor(player);

        switch (action)
        {
            case GOSSIP_ACTION_INFO_DEF:
                if (ClassicIsQuestDone(player, QUEST_STORMWIND_RENDEZVOUS))
                {
                    m_bEventProcessed = true;
                    m_playerGuid = player->GetGUID();
                    me->SetWalk(false);
                    me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                }

                CloseGossipMenuFor(player);
                break;
        }

        return true;
    }
};

/*
 * Reginald Windsor: The Great Masquerade (VMaNGOS npc_reginald_windsor)
 */

// VMaNGOS WindsorWaypoints: the walk from the gates to the keep
Position const WindsorWaypoints[] =
{
    { -9050.406250f, 443.974792f, 93.056458f, 0.659825f },
    { -8968.008789f, 509.771759f, 96.350754f, 0.679460f },
    { -8954.638672f, 519.920410f, 96.355453f, 0.680187f },
    { -8933.738281f, 500.683533f, 93.842247f, 5.941573f },
    { -8923.248047f, 496.464294f, 93.858475f, 0.688045f },
    { -8907.830078f, 509.035645f, 93.842529f, 2.163023f },
    { -8927.302734f, 542.173523f, 94.291695f, 0.606364f },
    { -8825.773438f, 623.565918f, 93.838066f, 5.484471f },
    { -8795.209961f, 590.400269f, 97.495560f, 0.644063f },
    { -8769.717773f, 608.193298f, 97.130692f, 5.507248f },
    { -8736.326172f, 574.955811f, 97.385048f, 3.886185f },
    { -8749.043945f, 560.525330f, 97.400307f, 5.502535f },
    { -8730.701172f, 540.466370f, 101.105370f, 5.431850f },
    { -8713.182617f, 519.765442f, 97.185402f, 0.707678f },
    { -8673.321289f, 554.135986f, 97.267708f, 6.174048f },
    { -8651.262695f, 551.696045f, 97.002983f, 5.525308f },
    { -8618.138672f, 518.573425f, 103.123642f, 5.497819f },
    { -8566.013672f, 465.536804f, 104.597160f, 5.481625f },
    { -8548.403320f, 466.680695f, 104.533554f, 5.387382f },
    { -8529.294922f, 443.376495f, 104.917046f, 5.399162f },
    { -8507.087891f, 415.847321f, 108.385857f, 5.371674f },
    { -8486.496094f, 389.750427f, 108.590248f, 5.391308f },
    { -8455.687500f, 351.054321f, 120.885910f, 5.391308f },
    { -8446.392578f, 339.602783f, 121.329506f, 5.359892f }
};

// VMaNGOS WindsorEventMove: 0 General Jonathan's spot, 1-6 the six city guards, 7-12 where they kneel, 13 the General's
// kneeling spot, 14 Anduin's spot in the keep, 15 Bolvar's spot in the keep
Position const WindsorEventMove[] =
{
    { -8964.973633f, 512.194519f, 96.355247f, 3.835189f },
    { -8963.196289f, 510.056549f, 96.355240f, 3.835189f },
    { -8961.235352f, 507.696808f, 96.595337f, 3.835189f },
    { -8959.596680f, 505.725403f, 96.595490f, 3.835189f },
    { -8967.410156f, 515.123535f, 96.354881f, 3.835189f },
    { -8968.840820f, 516.844482f, 96.595253f, 3.835189f },
    { -8970.687500f, 519.065796f, 96.595245f, 3.835189f },
    { -8958.585938f, 506.907959f, 96.595634f, 2.294317f },
    { -8960.827148f, 505.079407f, 96.593971f, 2.255047f },
    { -8962.866211f, 503.415009f, 96.591331f, 2.255047f },
    { -8969.562500f, 520.014587f, 96.595673f, 5.388790f },
    { -8971.773438f, 518.239868f, 96.594200f, 5.388790f },
    { -8973.923828f, 516.513611f, 96.590904f, 5.475183f },
    { -8976.549805f, 514.405701f, 96.590057f, 5.388790f },
    { -8506.340820f, 338.364441f, 120.88584f, 6.219385f },
    { -8449.006836f, 337.693451f, 121.32955f, 5.826686f }
};

Position const OnyxiaShoutPosition = { -9075.6f, 466.11f, 120.383f, 6.27f };
Position const MercutioLeavePosition = { -9148.395508f, 371.322174f, 90.543655f, 0.0f };
Position const BolvarAfterFightPosition = { -8447.39f, 335.35f, 121.747f, 1.29f };

// VMaNGOS GetRandomGuardText()
static uint32 ClassicRandomGuardText()
{
    switch (urand(0, 7))
    {
        case 0: return 8167; // Light be with you, sir.
        case 1: return 8170; // You are an inspiration to us all, sir.
        case 2: return 8172; // There walks a hero.
        case 3: return 8175; // Make way!
        case 4: return 8177; // We are but dirt beneath your feet, sir.
        case 5: return 8180; // A moment I shall remember for always.
        case 6: return 8183; // ...nerves of thorium.
    }
    return 8184; // A living legend...
}

// VMaNGOS ADD_GOSSIP_ITEM(broadcast text id): the localized BroadcastText when the client data has it
static std::string ClassicWindsorGossipText(Player* player, uint32 broadcastTextId, char const* fallback)
{
    if (BroadcastTextEntry const* bct = sBroadcastTextStore.LookupEntry(broadcastTextId))
    {
        char const* text = DB2Manager::GetBroadcastTextValue(bct, player->GetSession()->GetSessionDbLocaleIndex(), player->GetGender());
        if (text && *text)
            return text;
    }
    return fallback;
}

// VMaNGOS: 1000 ms plus the walking time to a point (distance / walk speed)
static uint32 ClassicWalkTimeTo(Unit const* walker, float speedOf, Position const& target)
{
    float const x = walker->GetPositionX() - target.GetPositionX();
    float const y = walker->GetPositionY() - target.GetPositionY();
    return 1000 + uint32(std::sqrt(x * x + y * y) / (speedOf * 0.001f));
}

struct classic_npc_reginald_windsor : public ScriptedAI
{
    classic_npc_reginald_windsor(Creature* creature) : ScriptedAI(creature) { ResetCreature(); }

    uint32 Timer;
    uint32 Tick;
    std::array<ObjectGuid, 30> GuardsGUIDs;
    std::array<ObjectGuid, 10> DragsGUIDs;
    ObjectGuid playerGUID;
    bool Begin;
    bool SummonHorse;
    bool ShooHorse;
    bool BeginQuest;
    bool NeedCheck;
    bool GuardNeed[6];
    bool PhaseFinale;
    bool CombatJustEnded;
    bool GreetPlayer;
    bool QuestAccepted;
    bool m_bRoweKnows;
    uint32 FinalTimer;
    uint32 GuardTimer[6];
    uint32 m_uiDespawnTimer;
    ObjectGuid m_squireRoweGuid;
    uint32 m_bolvarFaction;

    Player* GetPlayer() const { return me->GetMap()->GetPlayer(playerGUID); }
    Creature* GetGuard(uint8 num) const { return me->GetMap()->GetCreature(GuardsGUIDs[num]); }
    Creature* FindActor(uint32 entry) const { return me->FindNearestCreature(entry, 150.0f); }

    void Reset() override { }

    void ResetCreature()
    {
        m_bRoweKnows = false;
        m_squireRoweGuid.Clear();

        m_uiDespawnTimer = 5 * MINUTE * IN_MILLISECONDS;
        Timer = 3000;
        Tick = 0;
        playerGUID.Clear();
        GreetPlayer = false;
        QuestAccepted = false;
        BeginQuest = false;
        Begin = true;
        SummonHorse = false;
        ShooHorse = false;
        NeedCheck = false;
        PhaseFinale = false;
        CombatJustEnded = false;
        FinalTimer = 0;
        m_bolvarFaction = 0;

        for (uint8 i = 0; i < 6; i++)
        {
            GuardTimer[i] = 0;
            GuardNeed[i] = false;
        }
        GuardsGUIDs.fill(ObjectGuid::Empty);
        DragsGUIDs.fill(ObjectGuid::Empty);
    }

    // VMaNGOS constructor / JustRespawned -> ResetCreature()
    void JustAppeared() override
    {
        ScriptedAI::JustAppeared();
        ResetCreature();
    }

    // Squire Rowe hands over the player and himself (VMaNGOS writes playerGUID / m_squireRoweGuid directly)
    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        switch (id)
        {
            case WINDSOR_GUID_PLAYER:
                playerGUID = guid;
                break;
            case WINDSOR_GUID_ROWE:
                m_squireRoweGuid = guid;
                break;
            default:
                break;
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        PokeRowe();
        me->DespawnOrUnsummon(7min);
    }

    // let Squire Rowe know he can allow players to proceed with the quest now
    void PokeRowe()
    {
        if (m_bRoweKnows)
            return;

        if (Creature* rowe = me->GetMap()->GetCreature(m_squireRoweGuid))
        {
            if (CreatureAI* roweAI = rowe->AI())
            {
                roweAI->DoAction(ACTION_ROWE_RESET);
                m_bRoweKnows = true;
            }
        }
    }

    void CompleteQuest()
    {
        Player* player = GetPlayer();
        if (!player)
            return;

        if (Group* group = player->GetGroup())
        {
            for (GroupReference const& groupRef : group->GetMembers())
            {
                Player* member = groupRef.GetSource();
                if (member->GetQuestStatus(QUEST_THE_GREAT_MASQUERADE) == QUEST_STATUS_INCOMPLETE)
                    member->CompleteQuest(QUEST_THE_GREAT_MASQUERADE);
            }
        }
        else if (player->GetQuestStatus(QUEST_THE_GREAT_MASQUERADE) == QUEST_STATUS_INCOMPLETE)
            player->CompleteQuest(QUEST_THE_GREAT_MASQUERADE);
    }

    static void MoveHome(Creature* creature)
    {
        Position const home = creature->GetRespawnPosition();
        creature->GetMotionMaster()->MovePoint(0, home, true, home.GetOrientation());
    }

    static void FaceHome(Creature* creature)
    {
        creature->SetFacingTo(creature->GetRespawnPosition().GetOrientation());
    }

    // VMaNGOS EndScene(): Bolvar and Anduin walk back to their spots, Onyxia fades out
    void EndScene()
    {
        if (Creature* bolvar = FindActor(NPC_BOLVAR_FORDRAGON))
        {
            bolvar->SetStandState(UNIT_STAND_STATE_STAND);
            MoveHome(bolvar);
        }

        if (Creature* anduin = FindActor(NPC_ANDUIN_WRYNN))
        {
            MoveHome(anduin);
            FinalTimer = ClassicWalkTimeTo(anduin, anduin->GetSpeed(MOVE_WALK), anduin->GetRespawnPosition());
            PhaseFinale = true;
        }

        if (Creature* onyxia = FindActor(NPC_LADY_ONYXIA))
        {
            onyxia->RemoveAurasDueToSpell(SPELL_GREATER_INVISIBILITY);
            onyxia->CastSpell(onyxia, SPELL_INVISIBILITY, true);
        }
    }

    // VMaNGOS UpdateAI_corpse(): run from the corpse's event queue, since TC stops updating the AI of a dead creature
    void ScheduleCorpseFinale()
    {
        if (!PhaseFinale)
            return;

        me->m_Events.AddEventAtOffset([this]()
        {
            if (Creature* bolvar = FindActor(NPC_BOLVAR_FORDRAGON))
                FaceHome(bolvar);
            if (Creature* anduin = FindActor(NPC_ANDUIN_WRYNN))
                FaceHome(anduin);
            if (Creature* onyxia = FindActor(NPC_LADY_ONYXIA))
            {
                FaceHome(onyxia);
                onyxia->UpdateEntry(NPC_KATRANA_PRESTOR);
            }

            me->m_Events.AddEventAtOffset([this]() { PokeRowe(); }, 5s);
        }, Milliseconds(FinalTimer));
    }

    // Stormwind's guards salute Windsor on his way to the keep (each guard once, not in the keep itself)
    void MoveInLineOfSight(Unit* who) override
    {
        if (!who || !who->IsAlive() || !NeedCheck)
            return;

        if (who->GetEntry() != NPC_STORMWIND_CITY_GUARD && who->GetEntry() != NPC_STORMWIND_ROYAL_GUARD && who->GetEntry() != NPC_STORMWIND_CITY_PATROL)
            return;

        if (who->GetDistance2d(me) >= 8.0f)
            return;

        if (me->GetPositionY() < 360.0f)
            return;

        for (ObjectGuid const& guid : GuardsGUIDs)
            if (guid == who->GetGUID())
                return;

        who->SetFacingToObject(me);
        who->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);
        ClassicScriptText(ClassicRandomGuardText(), who);

        size_t slot = 0;
        while (!GuardsGUIDs[slot].IsEmpty() && slot < GuardsGUIDs.size() - 1)
            ++slot;
        GuardsGUIDs[slot] = who->GetGUID();
    }

    // Onyxia's "Windsor Death" finishes him: he lies there feigning death until Death Touch after his last words
    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_WINDSOR_DEATH)
            return;

        me->SetUnitFlag(UNIT_FLAG_PREVENT_EMOTES_FROM_CHAT_TEXT);
        me->SetUnitFlag3(UNIT_FLAG3_FAKE_DEAD);
        me->SetStandState(UNIT_STAND_STATE_DEAD);
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_THE_GREAT_MASQUERADE)
            return;

        BeginQuest = true;
        QuestAccepted = true;
        GreetPlayer = false;
        playerGUID = player->GetGUID();
        m_uiDespawnTimer = 30 * MINUTE * IN_MILLISECONDS;
        me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
    }

    bool OnGossipHello(Player* player) override
    {
        if (player == GetPlayer() && QuestAccepted)
            AddGossipItemFor(player, GossipOptionNpc::None, ClassicWindsorGossipText(player, GOSSIP_WINDSOR_READY_TEXT, "I'm ready, as are my forces. Let's finish this."), GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
        else if (me->IsQuestGiver())
            player->PrepareQuestMenu(me->GetGUID());

        SendGossipMenuFor(player, GOSSIP_WINDSOR_MENU, me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = GetGossipActionFor(player, gossipListId);
        ClearGossipMenuFor(player);

        if (action == GOSSIP_ACTION_INFO_DEF)
        {
            BeginQuest = true;
            ClassicScriptText(SAY_WINDSOR12, me);
            me->ReplaceAllNpcFlags(UNIT_NPC_FLAG_NONE);
        }

        CloseGossipMenuFor(player);
        return true;
    }

    void SummonCityGuards()
    {
        for (uint8 i = 0; i < 6; i++)
        {
            if (Creature* guard = me->SummonCreature(NPC_STORMWIND_CITY_GUARD, WindsorEventMove[i + 1], TEMPSUMMON_TIMED_DESPAWN, 240s))
            {
                GuardsGUIDs[i] = guard->GetGUID();
                guard->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
            }
        }
    }

    // the royal guards around Lady Prestor are her dragonkin in disguise
    void RevealElites(Creature* onyxia)
    {
        std::list<Creature*> guards;
        onyxia->GetCreatureListWithEntryInGrid(guards, NPC_STORMWIND_ROYAL_GUARD, 25.0f);
        size_t slot = 0;
        for (Creature* guard : guards)
        {
            if (slot >= DragsGUIDs.size())
                break;
            DragsGUIDs[slot++] = guard->GetGUID();
            guard->UpdateEntry(NPC_ONYXIA_ELITE_GUARD);
            guard->AIM_Initialize();
            guard->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            if (!urand(0, 2))
                ClassicScriptText(SAY_HISS, guard);
        }
    }

    void ElitesAttackBolvar(Creature* bolvar)
    {
        m_bolvarFaction = bolvar->GetFaction();
        bolvar->SetFaction(FACTION_BOLVAR_COMBAT);
        for (ObjectGuid const& guid : DragsGUIDs)
        {
            if (guid.IsEmpty())
                break;
            Creature* elite = me->GetMap()->GetCreature(guid);
            if (!elite)
                continue;
            elite->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
            elite->GetThreatManager().AddThreat(bolvar, 5000.0f);
            elite->SetTarget(bolvar->GetGUID());
            bolvar->EngageWithTarget(elite);
            elite->EngageWithTarget(bolvar);
        }
    }

    void RestoreBolvarFaction(Creature* bolvar)
    {
        if (m_bolvarFaction)
            bolvar->SetFaction(m_bolvarFaction);
        m_bolvarFaction = 0;
    }

    static void DespawnOnyxia(Creature* onyxia)
    {
        onyxia->SetNpcFlag(UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);
        onyxia->DespawnOrUnsummon(0s, 30min);
    }

    void UpdateAI(uint32 diff) override
    {
        // in case of idle / afk players
        if (m_uiDespawnTimer < diff)
        {
            PokeRowe();
            me->DespawnOrUnsummon();
            return;
        }
        else
            m_uiDespawnTimer -= diff;

        for (uint8 i = 0; i < 6; i++)
        {
            if (!GuardNeed[i])
                continue;

            if (GuardTimer[i] < diff)
            {
                if (Creature* guard = GetGuard(i))
                {
                    guard->SetFacingTo(WindsorEventMove[i + 7].GetOrientation());
                    guard->SetStandState(UNIT_STAND_STATE_KNEEL);
                    GuardNeed[i] = false;
                }
            }
            else
                GuardTimer[i] -= diff;
        }

        if (Begin)
        {
            if (me->GetDistance2d(WindsorWaypoints[0].GetPositionX(), WindsorWaypoints[0].GetPositionY()) < 2.0f)
            {
                if (Timer <= diff)
                {
                    Begin = false;
                    me->SetWalk(true);
                    me->SetSpeedRate(MOVE_WALK, 1.0f);

                    if (Creature* rowe = me->FindNearestCreature(NPC_ROWE, 50.0f))
                        rowe->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);

                    Timer = 2000;
                    SummonHorse = true;
                }
                else
                    Timer -= diff;
            }
        }

        if (SummonHorse)
        {
            if (Timer <= diff)
            {
                SummonHorse = false;
                me->Dismount();
                DoCastSelf(SPELL_WINDSOR_DISMISS_HORSE, true);
                if (Creature* mercutio = me->FindNearestCreature(NPC_MERCUTIO, 10.0f))
                {
                    mercutio->SetWalk(false);
                    me->SetFacingToObject(mercutio);
                }
                ShooHorse = true;
                Timer = 2000;
            }
            else
                Timer -= diff;
        }

        if (ShooHorse)
        {
            if (Timer <= diff)
            {
                ShooHorse = false;
                if (Creature* mercutio = me->FindNearestCreature(NPC_MERCUTIO, 10.0f))
                {
                    ClassicScriptText(SAY_WINDSOR1, me);
                    mercutio->GetMotionMaster()->MovePoint(0, MercutioLeavePosition);
                }
                GreetPlayer = true;
                Timer = 5000;
            }
            else
                Timer -= diff;
        }

        if (!BeginQuest)
        {
            if (GreetPlayer)
            {
                if (Timer <= diff)
                {
                    if (Player* player = GetPlayer())
                    {
                        me->SetFacingToObject(player);
                        me->SetNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
                        ClassicScriptText(SAY_WINDSOR2, me, player);
                    }
                    GreetPlayer = false;
                }
                else
                    Timer -= diff;
            }
            return;
        }

        if (Timer >= diff)
        {
            Timer -= diff;
            return;
        }

        float const walkSpeed = me->GetSpeed(MOVE_WALK);
        uint32 eventGuardId = 6;
        switch (Tick)
        {
            case 0:
                m_uiDespawnTimer = 20 * MINUTE * IN_MILLISECONDS;
                ClassicScriptText(SAY_WINDSOR3, me);
                Timer = 5000;
                break;
            case 1:
                me->SetFacingTo(0.659f);
                ClassicScriptText(SAY_WINDSOR4, me);
                Timer = 5000;
                break;
            case 2:
                SummonCityGuards();
                if (Creature* onyxia = me->SummonCreature(NPC_KATRANA_PRESTOR, OnyxiaShoutPosition, TEMPSUMMON_TIMED_DESPAWN, 10s))
                {
                    onyxia->SetDisplayId(DISPLAY_INVISIBLE);
                    onyxia->SetUninteractible(true);
                    ClassicScriptText(SAY_ONYXIA1, onyxia);
                }
                if (Creature* general = FindActor(NPC_MARCUS_JONATHAN))
                {
                    general->GetMotionMaster()->MovePoint(0, WindsorEventMove[0]);
                    general->Dismount();
                }
                me->GetMotionMaster()->MovePoint(0, WindsorWaypoints[1]);
                Timer = ClassicWalkTimeTo(me, walkSpeed, WindsorWaypoints[1]);
                break;
            case 3:
                if (Creature* general = FindActor(NPC_MARCUS_JONATHAN))
                    ClassicScriptText(SAY_MARCUS1, general);
                Timer = 5000;
                break;
            case 4:
                ClassicScriptText(SAY_WINDSOR5, me);
                Timer = 5000;
                break;
            case 5:
                ClassicScriptText(SAY_WINDSOR6, me);
                Timer = 5000;
                break;
            case 6:
                if (Creature* general = FindActor(NPC_MARCUS_JONATHAN))
                    ClassicScriptText(SAY_MARCUS2, general);
                Timer = 5000;
                break;
            case 7:
                if (Creature* general = FindActor(NPC_MARCUS_JONATHAN))
                    ClassicScriptText(SAY_MARCUS3, general);
                Timer = 5000;
                break;
            case 8:
                if (Creature* general = FindActor(NPC_MARCUS_JONATHAN))
                    ClassicScriptText(SAY_MARCUS4, general);
                Timer = 10000;
                break;
            case 9:
                ClassicScriptText(SAY_WINDSOR7, me);
                Timer = 5000;
                break;
            case 10:
                ClassicScriptText(SAY_WINDSOR8, me);
                Timer = 5000;
                break;
            case 11:
                if (Creature* general = FindActor(NPC_MARCUS_JONATHAN))
                {
                    if (Creature* guard = GetGuard(0))
                        general->SetFacingToObject(guard);
                    ClassicScriptText(SAY_MARCUS5, general);
                }
                break;
            case 12:
                eventGuardId = 0;
                break;
            case 13:
                eventGuardId = 1;
                break;
            case 14:
                eventGuardId = 2;
                Timer = 5000;
                break;
            case 15:
                if (Creature* general = FindActor(NPC_MARCUS_JONATHAN))
                {
                    if (Creature* guard = GetGuard(3))
                        general->SetFacingToObject(guard);
                    ClassicScriptText(SAY_MARCUS6, general);
                }
                break;
            case 16:
                eventGuardId = 3;
                break;
            case 17:
                eventGuardId = 4;
                break;
            case 18:
                eventGuardId = 5;
                Timer = 5000;
                break;
            case 19:
                if (Creature* general = FindActor(NPC_MARCUS_JONATHAN))
                {
                    general->SetFacingToObject(me);
                    ClassicScriptText(SAY_MARCUS7, general);
                }
                Timer = 5000;
                break;
            case 20:
                if (Creature* general = FindActor(NPC_MARCUS_JONATHAN))
                    general->HandleEmoteCommand(EMOTE_ONESHOT_SALUTE);
                Timer = 5000;
                break;
            case 21:
                if (Creature* general = FindActor(NPC_MARCUS_JONATHAN))
                    ClassicScriptText(SAY_MARCUS8, general);
                Timer = 5000;
                break;
            case 22:
                if (Creature* general = FindActor(NPC_MARCUS_JONATHAN))
                {
                    general->GetMotionMaster()->MovePoint(0, WindsorEventMove[13]);
                    Timer = ClassicWalkTimeTo(general, walkSpeed, WindsorEventMove[13]);
                }
                else
                    Timer = 1000;
                break;
            case 23:
                if (Creature* general = FindActor(NPC_MARCUS_JONATHAN))
                {
                    general->SetStandState(UNIT_STAND_STATE_KNEEL);
                    general->SetFacingTo(WindsorEventMove[13].GetOrientation());
                    me->SetFacingToObject(general);
                    ClassicScriptText(SAY_WINDSOR9, me);
                }
                Timer = 10000;
                break;
            case 24:
                if (Creature* guard = GetGuard(0))
                    me->SetFacingToObject(guard);
                ClassicScriptText(SAY_WINDSOR10, me);
                Timer = 5000;
                break;
            case 28:
                NeedCheck = true;
                break;
            case 29:
                // the General stands up and goes back to his post (VMaNGOS ForcedDespawn + Respawn)
                if (Creature* general = FindActor(NPC_MARCUS_JONATHAN))
                    general->DespawnOrUnsummon(0s, 1s);
                break;
            case 43:
                // at the keep's gate: Windsor waits until the player tells him they are ready
                m_uiDespawnTimer = 10 * MINUTE * IN_MILLISECONDS;
                BeginQuest = false;
                me->ReplaceAllNpcFlags(UNIT_NPC_FLAG_GOSSIP);
                ClassicScriptText(SAY_WINDSOR11, me);
                break;
            case 50:
                ClassicScriptText(SAY_WINDSOR13, me);
                if (Creature* bolvar = FindActor(NPC_BOLVAR_FORDRAGON))
                    bolvar->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);
                if (Creature* onyxia = FindActor(NPC_KATRANA_PRESTOR))
                    onyxia->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);
                Timer = 4000;
                break;
            case 51:
                if (Creature* bolvar = FindActor(NPC_BOLVAR_FORDRAGON))
                    ClassicScriptText(SAY_BOLVAR1, bolvar);
                break;
            case 52:
                if (Creature* anduin = FindActor(NPC_ANDUIN_WRYNN))
                {
                    anduin->SetWalk(false);
                    anduin->GetMotionMaster()->MovePoint(0, WindsorEventMove[14]);
                }
                Timer = 5000;
                break;
            case 53:
                if (Creature* onyxia = FindActor(NPC_KATRANA_PRESTOR))
                {
                    me->SetFacingToObject(onyxia);
                    ClassicScriptText(SAY_WINDSOR14, me);
                }
                Timer = 5000;
                break;
            case 54:
                if (Creature* onyxia = FindActor(NPC_KATRANA_PRESTOR))
                    ClassicScriptText(SAY_ONYXIA2, onyxia);
                Timer = 4000;
                break;
            case 55:
                if (Creature* onyxia = FindActor(NPC_KATRANA_PRESTOR))
                    ClassicScriptText(SAY_ONYXIA3, onyxia);
                Timer = 10000;
                break;
            case 56:
                if (Creature* onyxia = FindActor(NPC_KATRANA_PRESTOR))
                    ClassicScriptText(SAY_ONYXIA4, onyxia);
                Timer = 5000;
                break;
            case 57:
                ClassicScriptText(SAY_WINDSOR15, me);
                Timer = 5000;
                break;
            case 58:
                ClassicScriptText(SAY_WINDSOR16, me, me);
                Timer = 5000;
                break;
            case 59:
                ClassicScriptText(SAY_WINDSOR17, me);
                Timer = 4000;
                break;
            case 60:
                ClassicScriptText(SAY_WINDSOR18, me);
                Timer = 5000;
                break;
            case 61:
                ClassicScriptText(SAY_WINDSOR19, me, me);
                Timer = 2000;
                break;
            case 62:
                if (Creature* onyxia = FindActor(NPC_KATRANA_PRESTOR))
                    me->CastSpell(onyxia, SPELL_WINDSOR_READ_TABLETS, false);
                Timer = 10000;
                break;
            case 63:
                if (Creature* onyxia = FindActor(NPC_KATRANA_PRESTOR))
                {
                    onyxia->UpdateEntry(NPC_LADY_ONYXIA);
                    onyxia->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_NPC);
                }
                Timer = 1000;
                break;
            case 64:
                if (Creature* bolvar = FindActor(NPC_BOLVAR_FORDRAGON))
                    ClassicScriptText(SAY_BOLVAR2, bolvar, bolvar);
                Timer = 2000;
                break;
            case 65:
                if (Creature* bolvar = FindActor(NPC_BOLVAR_FORDRAGON))
                {
                    bolvar->SetWalk(false);
                    bolvar->GetMotionMaster()->MovePoint(0, WindsorEventMove[15], true, 5.740616f);
                    Timer = ClassicWalkTimeTo(bolvar, walkSpeed, WindsorEventMove[15]);
                }
                if (Creature* onyxia = FindActor(NPC_LADY_ONYXIA))
                    ClassicScriptText(SAY_ONYXIA5, onyxia);
                else
                    Timer = 4000;
                break;
            case 66:
                if (Creature* bolvar = FindActor(NPC_BOLVAR_FORDRAGON))
                    ClassicScriptText(SAY_BOLVAR3, bolvar);
                break;
            case 67:
                if (Creature* onyxia = FindActor(NPC_LADY_ONYXIA))
                    ClassicScriptText(SAY_ONYXIA6, onyxia);
                Timer = 2000;
                break;
            case 68:
                if (Creature* onyxia = FindActor(NPC_LADY_ONYXIA))
                {
                    ClassicScriptText(SAY_ONYXIA7, onyxia);
                    RevealElites(onyxia);
                }
                Timer = 4000;
                break;
            case 69:
                if (Creature* onyxia = FindActor(NPC_LADY_ONYXIA))
                    onyxia->CastSpell(me, SPELL_WINDSOR_DEATH, false);
                if (Creature* bolvar = FindActor(NPC_BOLVAR_FORDRAGON))
                    bolvar->SetEmoteState(EMOTE_STATE_READY_UNARMED);
                Timer = 1500;
                break;
            case 70:
                ClassicScriptText(SAY_WINDSOR20, me);
                Timer = 1000;
                break;
            case 71:
                if (Creature* onyxia = FindActor(NPC_LADY_ONYXIA))
                {
                    ClassicScriptText(SAY_ONYXIA8, onyxia);
                    if (Creature* bolvar = FindActor(NPC_BOLVAR_FORDRAGON))
                        ElitesAttackBolvar(bolvar);
                }
                Timer = 5000;
                break;
            case 72:
                if (Creature* onyxia = FindActor(NPC_LADY_ONYXIA))
                {
                    ClassicScriptText(SAY_ONYXIA9, onyxia);
                    onyxia->CastSpell(onyxia, SPELL_PRESTOR_DESPAWNS, true);
                }
                Timer = 1000;
                break;
            case 73:
                if (Creature* onyxia = FindActor(NPC_LADY_ONYXIA))
                    DespawnOnyxia(onyxia);
                Timer = 15000;
                break;
            case 74:
                if (Creature* bolvar = FindActor(NPC_BOLVAR_FORDRAGON))
                    ClassicScriptText(SAY_BOLVAR4, bolvar, bolvar);
                [[fallthrough]];
            case 75:
                if (Creature* onyxia = FindActor(NPC_LADY_ONYXIA))
                    DespawnOnyxia(onyxia);
                Tick = 100; // come back when combat is done
                PhaseFinale = true;
                break;
            case 76:
                if (Creature* bolvar = FindActor(NPC_BOLVAR_FORDRAGON))
                {
                    bolvar->SetEmoteState(EMOTE_ONESHOT_NONE);
                    ClassicScriptText(SAY_BOLVAR5, bolvar);
                    bolvar->SetWalk(true);
                    bolvar->SetNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
                    bolvar->SetStandState(UNIT_STAND_STATE_KNEEL);
                }
                Timer = 1000;
                break;
            case 77:
                CompleteQuest();
                ClassicScriptText(SAY_WINDSOR21, me);
                Timer = 8000;
                break;
            case 78:
                ClassicScriptText(SAY_WINDSOR22, me);
                EndScene();
                BeginQuest = false;
                ScheduleCorpseFinale();
                me->CastSpell(me, SPELL_DEATH_TOUCH, true);
                break;
            default:
                break;
        }

        if (eventGuardId < 6)
        {
            if (Creature* guard = GetGuard(eventGuardId))
            {
                guard->GetMotionMaster()->MovePoint(0, WindsorEventMove[eventGuardId + 7]);
                GuardTimer[eventGuardId] = ClassicWalkTimeTo(guard, walkSpeed, WindsorEventMove[eventGuardId + 7]);
            }
            else
                GuardTimer[eventGuardId] = 1000;
            GuardNeed[eventGuardId] = true;
            Timer = 1000;
        }

        if (Tick > 26 && Tick < 44)
        {
            Position const& point = WindsorWaypoints[Tick - 25];
            me->GetMotionMaster()->MovePoint(0, point, true, point.GetOrientation());
            Timer = ClassicWalkTimeTo(me, walkSpeed, point);
        }
        else if (Tick > 44 && Tick < 50)
        {
            Position const& point = WindsorWaypoints[Tick - 26];
            me->GetMotionMaster()->MovePoint(0, point, true, point.GetOrientation());
            Timer = ClassicWalkTimeTo(me, walkSpeed, point);
        }
        else if (PhaseFinale)
        {
            if (Creature* bolvar = FindActor(NPC_BOLVAR_FORDRAGON))
            {
                if (!bolvar->IsInCombat() && !CombatJustEnded)
                {
                    bolvar->SetWalk(true);
                    bolvar->GetMotionMaster()->MovePoint(0, BolvarAfterFightPosition, true, BolvarAfterFightPosition.GetOrientation());
                    RestoreBolvarFaction(bolvar);
                    CombatJustEnded = true;
                    Timer = 5000;
                    Tick = 76;
                    return;
                }
            }
        }

        Tick++;
    }
};

void AddSC_classic_stormwind_rendezvous()
{
    RegisterCreatureAI(classic_npc_squire_rowe);
    RegisterCreatureAI(classic_npc_reginald_windsor);
}
