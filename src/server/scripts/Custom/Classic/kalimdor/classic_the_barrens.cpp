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

// Classic 1.60 port of VMaNGOS src/scripts/kalimdor/the_barrens/the_barrens.cpp (ScriptDev2 lineage, GPL-2)
// Quest support: 863 (Escape!), 898 (Free From the Hold), 1719 (The Affray), 2458 (Mission: Possible But Not Probable)
// Ported: npc_gilthares, npc_twiggy_flathead, npc_wizzlecranks_shredder, npc_mission_possible_but_not_probable,
//         npc_sarilus_foulborne

#include "ScriptMgr.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"
#include "SpellInfo.h"
#include "TemporarySummon.h"
#include "classic_script_text.h"
#include <initializer_list>
#include <list>

/*######
# npc_gilthares
######*/

enum Gilthares
{
    SAY_GIL_START               = 1065,
    SAY_GIL_AT_LAST             = 1066,
    SAY_GIL_PROCEED             = 1067,
    SAY_GIL_FREEBOOTERS         = 1068,
    SAY_GIL_AGGRO_1             = 1074,
    SAY_GIL_AGGRO_2             = 1075,
    SAY_GIL_AGGRO_3             = 1072,
    SAY_GIL_AGGRO_4             = 1073,
    SAY_GIL_ALMOST              = 1069,
    SAY_GIL_SWEET               = 1070,
    SAY_GIL_FREED               = 1071,

    QUEST_FREE_FROM_HOLD        = 898,
    AREA_MERCHANT_COAST         = 391,

    // TODO(classic): import VMaNGOS script_waypoint (entry 3465) as waypoint_path with this id (entry * 8), node id = pointid
    PATH_ESCORT_GILTHARES       = 3465 * 8
};

struct classic_npc_gilthares : public EscortAI
{
    classic_npc_gilthares(Creature* creature) : EscortAI(creature) { }

    void Reset() override { }

    // VMaNGOS JustRespawned
    void JustAppeared() override
    {
        me->SetImmuneToNPC(true);
        EscortAI::JustAppeared();
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_FREE_FROM_HOLD)
            return;

        me->SetFaction(FACTION_ESCORTEE_H_NEUTRAL_ACTIVE);
        me->SetImmuneToNPC(false);
        me->SetStandState(UNIT_STAND_STATE_STAND);

        ClassicScriptText(SAY_GIL_START, me, player);

        LoadPath(PATH_ESCORT_GILTHARES);
        Start(true, player->GetGUID(), quest);
    }

    void WaypointReached(uint32 waypointId, uint32 /*pathId*/) override
    {
        Player* player = GetPlayerForEscort();
        if (!player)
            return;

        switch (waypointId)
        {
            case 16:
                ClassicScriptText(SAY_GIL_AT_LAST, me, player);
                break;
            case 17:
                ClassicScriptText(SAY_GIL_PROCEED, me, player);
                break;
            case 18:
                ClassicScriptText(SAY_GIL_FREEBOOTERS, me, player);
                break;
            case 37:
                ClassicScriptText(SAY_GIL_ALMOST, me, player);
                break;
            case 47:
                ClassicScriptText(SAY_GIL_SWEET, me, player);
                break;
            case 53:
                ClassicScriptText(SAY_GIL_FREED, me, player);
                player->GroupEventHappens(QUEST_FREE_FROM_HOLD, me);
                break;
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        //not always use
        if (urand(0, 3))
            return;

        //only aggro text if not player and only in this area
        if (who->GetTypeId() != TYPEID_PLAYER && me->GetAreaId() == AREA_MERCHANT_COAST)
        {
            //appears to be pretty much random (possible only if escorter not in combat with who yet?)
            switch (urand(0, 3))
            {
                case 0: ClassicScriptText(SAY_GIL_AGGRO_1, me, who); break;
                case 1: ClassicScriptText(SAY_GIL_AGGRO_2, me, who); break;
                case 2: ClassicScriptText(SAY_GIL_AGGRO_3, me, who); break;
                case 3: ClassicScriptText(SAY_GIL_AGGRO_4, me, who); break;
            }
        }
    }
};

/*#####
## npc_twiggy_flathead
#####*/

enum TwiggyFlathead
{
    SAY_BIG_WILL_READY          = 2421,
    SAY_TWIGGY_BEGIN            = 2301,
    SAY_TWIGGY_FRAY             = 2318,
    SAY_TWIGGY_DOWN             = 2355,
    SAY_TWIGGY_OVER             = 2320,

    NPC_BIG_WILL                = 6238,
    NPC_AFFRAY_CHALLENGER       = 6240,
    NPC_AFFRAY_SPECTATOR        = 6249,

    FACTION_TWIGGY_FRIENDLY     = 35,
    FACTION_TWIGGY_MONSTER      = 16,
    FACTION_TWIGGY_CREATURE     = 7,

    // SetGUID id used to start the event (VMaNGOS at_twiggy_flathead -> CanStartEvent)
    DATA_TWIGGY_START_EVENT     = 1
};

Position const AffrayChallengerLoc[6] =
{
    { -1683.0f, -4326.0f, 2.79f, 0.00f },
    { -1682.0f, -4329.0f, 2.79f, 0.00f },
    { -1683.0f, -4330.0f, 2.79f, 0.00f },
    { -1680.0f, -4334.0f, 2.79f, 1.49f },
    { -1674.0f, -4326.0f, 2.79f, 3.49f },
    { -1677.0f, -4334.0f, 2.79f, 1.66f }
};

struct classic_npc_twiggy_flathead : public ScriptedAI
{
    classic_npc_twiggy_flathead(Creature* creature) : ScriptedAI(creature)
    {
        Initialize();
    }

    void Initialize()
    {
        _eventInProgress = false;

        _eventTimer = 2000;
        _emoteTimer = 1000;
        _step = 0;
        _challengerCount = 0;
        _challengerDeathTimer = 0;

        _playerGUID.Clear();
        _bigWillGUID.Clear();

        for (ObjectGuid& guid : _affrayChallenger)
            guid.Clear();
    }

    void Reset() override
    {
        Initialize();
    }

    // started by classic_at_twiggy_flathead (areatrigger 522, below)
    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id != DATA_TWIGGY_START_EVENT)
            return;

        if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
            CanStartEvent(player);
    }

    bool CanStartEvent(Player* player)
    {
        if (!_eventInProgress)
        {
            _eventInProgress = true;
            _playerGUID = player->GetGUID();
            ClassicScriptText(SAY_TWIGGY_BEGIN, me, player);
            return true;
        }

        TC_LOG_DEBUG("scripts", "classic_npc_twiggy_flathead event already in progress, need to wait.");
        return false;
    }

    void SetChallengers()
    {
        for (uint8 i = 0; i < 6; ++i)
        {
            Creature* challenger = me->SummonCreature(NPC_AFFRAY_CHALLENGER, AffrayChallengerLoc[i], TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 600s);
            if (!challenger)
            {
                TC_LOG_DEBUG("scripts", "classic_npc_twiggy_flathead event cannot summon challenger as expected.");
                continue;
            }

            challenger->SetFaction(FACTION_TWIGGY_FRIENDLY);
            challenger->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);   // VMaNGOS UNIT_FLAG_SPAWNING
            challenger->HandleEmoteCommand(EMOTE_ONESHOT_ROAR);
            _affrayChallenger[i] = challenger->GetGUID();
        }
    }

    void SetChallengerReady(Unit* unit)
    {
        unit->SetUninteractible(false);                         // VMaNGOS UNIT_FLAG_UNINTERACTIBLE
        unit->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
        unit->HandleEmoteCommand(EMOTE_ONESHOT_ROAR);
        unit->SetFaction(FACTION_TWIGGY_MONSTER);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!_eventInProgress)
            return;

        if (_challengerDeathTimer)
        {
            if (_challengerDeathTimer <= diff)
            {
                for (ObjectGuid& guid : _affrayChallenger)
                {
                    Creature* challenger = ObjectAccessor::GetCreature(*me, guid);
                    if (challenger && !challenger->IsAlive() && challenger->isDead())
                    {
                        ClassicScriptText(SAY_TWIGGY_DOWN, me);
                        challenger->RemoveCorpse();
                        guid.Clear();
                        continue;
                    }
                }
                _challengerDeathTimer = 2500;
            }
            else
                _challengerDeathTimer -= diff;
        }

        if (_eventTimer < diff)
        {
            Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);

            if (!player || player->isDead())
            {
                Reset();
                return;     // VMaNGOS falls through into the step switch after Reset(), which re-summons the challengers
            }

            switch (_step)
            {
                case 0:
                    SetChallengers();
                    _challengerDeathTimer = 2500;
                    _eventTimer = 5000;
                    ++_step;
                    break;
                case 1:
                    ClassicScriptText(SAY_TWIGGY_FRAY, me);
                    if (Creature* challenger = ObjectAccessor::GetCreature(*me, _affrayChallenger[_challengerCount]))
                        SetChallengerReady(challenger);
                    else
                    {
                        Reset();
                        return;
                    }
                    ++_challengerCount;
                    _eventTimer = 25000;
                    if (_challengerCount == 6)
                        ++_step;
                    break;
                case 2:
                    if (Creature* bigWill = me->SummonCreature(NPC_BIG_WILL, -1713.79f, -4342.09f, 6.05f, 6.15f, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 300s))
                    {
                        _bigWillGUID = bigWill->GetGUID();
                        bigWill->SetFaction(FACTION_TWIGGY_FRIENDLY);
                        bigWill->GetMotionMaster()->MovePoint(0, -1682.31f, -4329.68f, 2.78f);
                    }
                    _eventTimer = 15000;
                    ++_step;
                    break;
                case 3:
                    if (Creature* bigWill = ObjectAccessor::GetCreature(*me, _bigWillGUID))
                    {
                        bigWill->SetFaction(FACTION_TWIGGY_CREATURE);
                        ClassicScriptText(SAY_BIG_WILL_READY, bigWill);
                    }
                    _eventTimer = 5000;
                    ++_step;
                    break;
                case 4:
                {
                    Creature* bigWill = ObjectAccessor::GetCreature(*me, _bigWillGUID);
                    if (bigWill && bigWill->isDead())
                    {
                        ClassicScriptText(SAY_TWIGGY_OVER, me);
                        Reset();
                        return;
                    }
                    else if (!bigWill)
                    {
                        Reset();
                        return;
                    }
                    _eventTimer = 5000;
                    break;
                }
            }
        }
        else
            _eventTimer -= diff;

        if (_emoteTimer < diff)
        {
            for (uint8 i = _challengerCount; i < 6; ++i)
            {
                if (Creature* challenger = ObjectAccessor::GetCreature(*me, _affrayChallenger[i]))
                {
                    if (!urand(0, 4))
                        challenger->HandleEmoteCommand(EMOTE_ONESHOT_ROAR);
                }
            }

            std::list<Creature*> spectators;
            me->GetCreatureListWithEntryInGrid(spectators, NPC_AFFRAY_SPECTATOR, 30.0f);
            for (Creature* spectator : spectators)
            {
                switch (urand(0, 10))
                {
                    case 0:
                        spectator->HandleEmoteCommand(EMOTE_ONESHOT_CHEER);
                        break;
                    case 1:
                        spectator->HandleEmoteCommand(EMOTE_ONESHOT_RUDE);
                        break;
                }
            }
            _emoteTimer = 2000;
        }
        else
            _emoteTimer -= diff;
    }

private:
    bool _eventInProgress;

    uint32 _eventTimer;
    uint32 _step;
    uint32 _challengerCount;
    uint32 _challengerDeathTimer;
    uint32 _emoteTimer;

    ObjectGuid _playerGUID;
    ObjectGuid _bigWillGUID;
    ObjectGuid _affrayChallenger[6];
};

// Areatrigger 522, the Affray ring (VMaNGOS at_twiggy_flathead): a player with The Affray (1719) still open who steps into the
// ring makes Twiggy start the event. Without it nobody started the event and the Berserker Stance chain stopped here.
class classic_at_twiggy_flathead : public AreaTriggerScript
{
public:
    classic_at_twiggy_flathead() : AreaTriggerScript("classic_at_twiggy_flathead") { }

    bool OnTrigger(Player* player, AreaTriggerEntry const* /*trigger*/) override
    {
        static constexpr uint32 QUEST_THE_AFFRAY = 1719;
        static constexpr uint32 NPC_TWIGGY_FLATHEAD = 6248;

        if (!player->IsAlive() || player->GetQuestStatus(QUEST_THE_AFFRAY) != QUEST_STATUS_INCOMPLETE)
            return false;

        if (Creature* twiggy = player->FindNearestCreature(NPC_TWIGGY_FLATHEAD, 30.0f))
            twiggy->AI()->SetGUID(player->GetGUID(), DATA_TWIGGY_START_EVENT);

        return false;
    }
};

/*#####
## npc_wizzlecranks_shredder
#####*/

enum WizzlecranksShredder
{
    SAY_START                   = 1031,
    SAY_STARTUP1                = 1039,
    SAY_STARTUP2                = 1032,
    SAY_MERCENARY               = 1040,
    SAY_PROGRESS_1              = 1033,
    SAY_PROGRESS_2              = 1043,
    SAY_PROGRESS_3              = 1041,
    SAY_END                     = 1044,

    QUEST_ESCAPE                = 863,
    CLASSIC_FACTION_RATCHET             = 637,
    NPC_PILOT_WIZZ              = 3451,
    NPC_MERCENARY               = 3282,

    // TODO(classic): import VMaNGOS script_waypoint (entry 3439) as waypoint_path with this id (entry * 8), node id = pointid.
    // VMaNGOS starts running, walks from point 9 (SetRun(false)) and runs again from point 18 (SetRun()); in TC the
    // walk/run state comes from the path nodes' MoveType, so encode it there.
    PATH_ESCORT_WIZZLECRANK     = 3439 * 8
};

struct classic_npc_wizzlecranks_shredder : public EscortAI
{
    classic_npc_wizzlecranks_shredder(Creature* creature) : EscortAI(creature)
    {
        _isPostEvent = false;
        _postEventTimer = 1000;
        _postEventCount = 0;
    }

    void Reset() override
    {
        // the escort is over before the end scene: an evade then must not stop it halfway (quest never credited)
        if (!HasEscortState(STATE_ESCORT_ESCORTING) && !_isPostEvent)
        {
            if (me->GetStandState() == UNIT_STAND_STATE_DEAD)
                me->SetStandState(UNIT_STAND_STATE_STAND);

            _isPostEvent = false;
            _postEventTimer = 1000;
            _postEventCount = 0;
        }
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_ESCAPE)
            return;

        ClassicScriptText(SAY_START, me);
        me->SetFaction(CLASSIC_FACTION_RATCHET);

        LoadPath(PATH_ESCORT_WIZZLECRANK);
        // EscortAI despawns 1 s after the last point and ignores that point's 40 s wait, so the shredder vanished right
        // after "Looks like we're out of the woods" and the end scene (quest credit, Pilot Wizzlecrank) never ran.
        // The end scene despawns the shredder itself (UpdateEscortAI, step 4).
        SetDespawnAtEnd(false);
        Start(true, player->GetGUID(), quest);
    }

    void WaypointReached(uint32 waypointId, uint32 /*pathId*/) override
    {
        switch (waypointId)
        {
            case 0:
                if (Player* player = GetPlayerForEscort())
                    ClassicScriptText(SAY_STARTUP1, me, player);
                break;
            case 9:
                me->SetWalk(true);  // VMaNGOS SetRun(false) - see PATH_ESCORT_WIZZLECRANK note
                break;
            case 17:
                if (Creature* mercenary = me->SummonCreature(NPC_MERCENARY, 1128.489f, -3037.611f, 92.701f, 1.472f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 120s))
                {
                    ClassicScriptText(SAY_MERCENARY, mercenary);
                    me->SummonCreature(NPC_MERCENARY, 1160.172f, -2980.168f, 97.313f, 3.690f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 120s);
                }
                break;
            case 24:
                _isPostEvent = true;
                break;
        }
    }

    void WaypointStarted(uint32 waypointId, uint32 /*pathId*/) override
    {
        switch (waypointId)
        {
            case 9:
                if (Player* player = GetPlayerForEscort())
                    ClassicScriptText(SAY_STARTUP2, me, player);
                break;
            case 18:
                if (Player* player = GetPlayerForEscort())
                    ClassicScriptText(SAY_PROGRESS_1, me, player);
                me->SetWalk(false); // VMaNGOS SetRun() - see PATH_ESCORT_WIZZLECRANK note
                break;
        }
    }

    void JustSummoned(Creature* summoned) override
    {
        if (summoned->GetEntry() == NPC_PILOT_WIZZ)
        {
            me->SetStandState(UNIT_STAND_STATE_DEAD);
            me->RestoreFaction();
        }

        if (summoned->GetEntry() == NPC_MERCENARY)
            summoned->AI()->AttackStart(me);
    }

    void UpdateEscortAI(uint32 diff) override
    {
        if (UpdateVictim())
            return;

        if (_isPostEvent)
        {
            if (_postEventTimer < diff)
            {
                switch (_postEventCount)
                {
                    case 0:
                        ClassicScriptText(SAY_PROGRESS_2, me);
                        break;
                    case 1:
                        ClassicScriptText(SAY_PROGRESS_3, me);
                        break;
                    case 2:
                        ClassicScriptText(SAY_END, me);
                        break;
                    case 3:
                        if (Player* player = GetPlayerForEscort())
                            player->GroupEventHappens(QUEST_ESCAPE, me);
                        // VMaNGOS summons at 0,0,0 (= own position)
                        me->SummonCreature(NPC_PILOT_WIZZ, me->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, 180s);
                        me->SetHomePosition(me->GetRespawnPosition());   // VMaNGOS ResetHomePosition()
                        break;
                    case 4:
                        // broken down shredder: gone after the pilot's 3 minutes, then respawns at the drill site
                        _isPostEvent = false;
                        _postEventCount = 0;
                        _postEventTimer = 1000;
                        me->DespawnOrUnsummon(175s);
                        return;
                }

                ++_postEventCount;
                _postEventTimer = 5000;
            }
            else
                _postEventTimer -= diff;
        }
    }

private:
    bool _isPostEvent;
    uint32 _postEventTimer;
    uint32 _postEventCount;
};

/*
 * 'Mission: Possible But Not Probable' support:
 * Grand Foreman Puzik Gallywix
 * Mutated Venture Co. Drone
 * Venture Co. Patroller
 * Venture Co. Lookout
 */

enum MissionPossible
{
    NPC_MUTATED_VENTURE_CO_DRONE        = 7310,
    NPC_VENTURE_CO_PATROLLER            = 7308,
    NPC_VENTURE_CO_LOOKOUT              = 7307,
    NPC_GRAND_FOREMAN_PUZIK_GALLYWIX    = 7288,

    SPELL_JUGGLER_VEIN_RUPTURE          = 10265,
    SPELL_LUNG_PUNCTURE                 = 10266,
    SPELL_SLUSH                         = 10267,
    SPELL_DECIMATE                      = 10268
};

// VMaNGOS checks SpellEntry::IsFitToFamily<SPELLFAMILY_ROGUE, CF_ROGUE_*>(); the class family masks of the Classic
// DB2 data are not guaranteed to match vanilla, so the (vanilla 1.12) rank ids are listed explicitly.
namespace
{
bool IsSpellInList(uint32 spellId, std::initializer_list<uint32> ids)
{
    for (uint32 id : ids)
        if (id == spellId)
            return true;
    return false;
}

bool IsRogueGarrote(uint32 spellId)    { return IsSpellInList(spellId, { 703, 8631, 8632, 8633, 11289, 11290 }); }
bool IsRogueAmbush(uint32 spellId)     { return IsSpellInList(spellId, { 8676, 8724, 8725, 11267, 11268, 11269 }); }
bool IsRogueRupture(uint32 spellId)    { return IsSpellInList(spellId, { 1943, 8639, 8640, 11273, 11274, 11275 }); }
bool IsRogueEviscerate(uint32 spellId) { return IsSpellInList(spellId, { 2098, 6760, 6761, 6762, 8623, 8624, 11299, 11300 }); }
}

struct classic_npc_mission_possible_but_not_probable : public ScriptedAI
{
    classic_npc_mission_possible_but_not_probable(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override { }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        uint32 spellId = 0;

        switch (me->GetEntry())
        {
            case NPC_MUTATED_VENTURE_CO_DRONE:
                if (IsRogueGarrote(spellInfo->Id) || IsRogueAmbush(spellInfo->Id))
                    spellId = SPELL_JUGGLER_VEIN_RUPTURE;
                break;
            case NPC_VENTURE_CO_PATROLLER:
                if (IsRogueRupture(spellInfo->Id))
                    spellId = SPELL_LUNG_PUNCTURE;
                break;
            case NPC_VENTURE_CO_LOOKOUT:
                if (IsRogueEviscerate(spellInfo->Id))
                    spellId = SPELL_SLUSH;
                break;
            case NPC_GRAND_FOREMAN_PUZIK_GALLYWIX:
                if (IsRogueAmbush(spellInfo->Id))
                    spellId = SPELL_DECIMATE;
                break;
            default:
                return;
        }

        if (spellId)
            DoCastSelf(spellId);
    }
};

/*
 * Sarilus Foulborne
 */

enum SarilusFoulborne
{
    SPELL_SARILUS_ELEMENTALS_PASSIVE    = 6488,
    SPELL_SARILUS_ELEMENTALS            = 6490,
    SPELL_FEED_SARILUS_PASSIVE          = 6498,
    SPELL_FROSTBOLT                     = 20806
};

struct classic_npc_sarilus_foulborne : public ScriptedAI
{
    classic_npc_sarilus_foulborne(Creature* creature) : ScriptedAI(creature)
    {
        Initialize();
    }

    void Initialize()
    {
        _elementalsTimer = urand(1500, 9000);
        _frostboltTimer = urand(3500, 4500);
    }

    void Reset() override
    {
        Initialize();
    }

    void JustSummoned(Creature* summoned) override
    {
        summoned->CastSpell(summoned, SPELL_SARILUS_ELEMENTALS_PASSIVE, true);
        summoned->CastSpell(summoned, SPELL_FEED_SARILUS_PASSIVE, true);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        if (_elementalsTimer < diff)
        {
            if (DoCastSelf(SPELL_SARILUS_ELEMENTALS) == SPELL_CAST_OK)
                _elementalsTimer = 9000;
        }
        else
            _elementalsTimer -= diff;

        if (_frostboltTimer < diff)
        {
            if (DoCastVictim(SPELL_FROSTBOLT) == SPELL_CAST_OK)
                _frostboltTimer = urand(3500, 4500);
        }
        else
            _frostboltTimer -= diff;
    }

private:
    uint32 _elementalsTimer;
    uint32 _frostboltTimer;
};

void AddSC_classic_the_barrens()
{
    RegisterCreatureAI(classic_npc_gilthares);
    RegisterCreatureAI(classic_npc_twiggy_flathead);
    new classic_at_twiggy_flathead();
    RegisterCreatureAI(classic_npc_wizzlecranks_shredder);
    RegisterCreatureAI(classic_npc_mission_possible_but_not_probable);
    RegisterCreatureAI(classic_npc_sarilus_foulborne);
}
