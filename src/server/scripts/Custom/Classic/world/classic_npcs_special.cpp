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

// Classic 1.60 port of VMaNGOS src/scripts/world/npcs_special.cpp (ScriptDev2 lineage, GPL-2)
// Quest support: 6124, 6129 (Curing the Sick: Sickly Deer / Sickly Gazelle)

#include "ScriptMgr.h"
#include "Creature.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "TemporarySummon.h"
#include "Player.h"
#include "SpellInfo.h"
#include "GameObject.h"
#include "MovementDefines.h"
#include "QuaternionData.h"
#include "QuestDef.h"
#include "WaypointDefines.h"
#include <functional>

/*
 * Curing the Sick
 */

enum SicklyCritter
{
    SICKLY_SPELL_APPLY_SALVE        = 19512,
    SICKLY_SPELL_SICKLY_AURA        = 19502,

    SICKLY_NPC_SICKLY_DEER          = 12298,
    SICKLY_NPC_SICKLY_GAZELLE       = 12296,

    SICKLY_NPC_CURED_DEER           = 12299,
    SICKLY_NPC_CURED_GAZELLE        = 12297,

    SICKLY_MODEL_CURED_DEER         = 347,
    SICKLY_MODEL_CURED_GAZELLE      = 1547
};

struct classic_npc_sickly_critter : public CritterAI
{
    classic_npc_sickly_critter(Creature* creature) : CritterAI(creature), _team(HORDE)
    {
        ResetCreature();
    }

    void JustAppeared() override
    {
        ResetCreature();
        CritterAI::JustAppeared();
    }

    void ResetCreature()
    {
        _isHit = false;
        _modify = false;
        _timer = 1500;
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SICKLY_SPELL_APPLY_SALVE)
            return;

        if (_isHit)
            return;

        Player* player = caster ? caster->ToPlayer() : nullptr;
        if (!player)
            return;

        _playerGUID = player->GetGUID();

        if (me->GetEntry() != SICKLY_NPC_SICKLY_DEER && me->GetEntry() != SICKLY_NPC_SICKLY_GAZELLE)
            return;

        _team = player->GetTeam();
        _modify = true;

        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveFleeing(player);
        me->DespawnOrUnsummon(10s);

        _isHit = true;
    }

    void UpdateAI(uint32 diff) override
    {
        if (_modify)
        {
            if (_timer < diff)
            {
                // VMaNGOS rewards through RewardPlayerAndGroupAtCast (ReqCreatureOrGOId = cured entry, ReqSpellCast = 19512);
                // TC quest objectives 6124/6129 are QUEST_OBJECTIVE_MONSTER on the cured entry, so give kill credit for it.
                uint32 curedEntry = SICKLY_NPC_CURED_GAZELLE;
                switch (_team)
                {
                    case ALLIANCE:
                        curedEntry = SICKLY_NPC_CURED_DEER;
                        me->SetEntry(SICKLY_NPC_CURED_DEER);
                        me->SetDisplayId(SICKLY_MODEL_CURED_DEER);
                        break;
                    default: // HORDE
                        me->SetEntry(SICKLY_NPC_CURED_GAZELLE);
                        me->SetDisplayId(SICKLY_MODEL_CURED_GAZELLE);
                        break;
                }

                me->RemoveAurasDueToSpell(SICKLY_SPELL_SICKLY_AURA);
                _modify = false;

                if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                    player->RewardPlayerAndGroupAtEvent(curedEntry, me);
            }
            else
                _timer -= diff;
        }

        CritterAI::UpdateAI(diff);
    }

private:
    bool _isHit;
    bool _modify;
    uint32 _timer;
    Team _team;
    ObjectGuid _playerGUID;
};

/*######
## classic_npc_malfunctioning_cyclone_construct (250929)
######*/

// Classic 1.60 (WoW Forever), Zephras Isle orchard, quest Standing Our Ground (92693; official beta sniffs, builds 70170 / 70205):
// "I'm ready to fight, Aamelia." gives the talk credit, Aamelia runs to the orchard's edge, then Ferauu the Bludgeon and six Bandit
// Henchmen appear on the road, Ferauu yells and they walk down to the orchard. ~20 s after they appeared the construct, which
// wanders by its post otherwise, walks over, yells, kills them with Chain Lightning and yells again; Aamelia casts Make Your Stand
// (the Ferauu credit) and runs home.
enum CycloneConstructEvent
{
    NPC_AAMELIA_WINDFIELD           = 252800,
    NPC_FERAUU_THE_BLUDGEON         = 252863,
    NPC_BANDIT_HENCHMAN             = 252875,
    SPELL_CONSTRUCT_CHAIN_LIGHTNING = 1257106,
    SPELL_MAKE_YOUR_STAND           = 1257130,
    QUEST_STANDING_OUR_GROUND       = 92693,
    GOSSIP_MENU_AAMELIA             = 40780,

    ACTION_START_ORCHARD_EVENT      = 1,
    ACTION_ORCHARD_EVENT_DONE       = 2,

    EVENT_BANDITS_APPEAR            = 1,
    EVENT_CONSTRUCT_GO,
    EVENT_CONSTRUCT_STRIKE,
    EVENT_CONSTRUCT_DONE,
    EVENT_AAMELIA_GO_HOME,

    POINT_CONSTRUCT_STRIKE          = 1,
    POINT_CONSTRUCT_HOME            = 2,
    POINT_AAMELIA_HOME              = 100
};

static constexpr Milliseconds CycloneBanditsDelay = 1s;
static constexpr Milliseconds CycloneStrikeDelay = 20s;
static constexpr float CycloneWanderDistance = 15.0f;

static Position const FerauuStart       = { 2110.0f, 1543.5f, 664.7f, 3.6f };
static Position const FerauuEnd         = { 2071.0f, 1571.1f, 655.7f, 3.6f };
static Position const ConstructStrikeAt = { 2069.0f, 1557.4f, 655.2f, 1.6f };

// start (on the road) -> end (in the orchard) of each henchman
static std::pair<Position, Position> const HenchmanPaths[] =
{
    { { 2115.7f, 1548.3f, 665.8f, 3.6f }, { 2078.6f, 1573.9f, 657.0f, 3.6f } },
    { { 2117.1f, 1550.8f, 666.7f, 3.6f }, { 2075.6f, 1575.6f, 656.5f, 3.6f } },
    { { 2114.5f, 1550.3f, 666.0f, 3.6f }, { 2074.4f, 1572.8f, 656.0f, 3.6f } },
    { { 2106.5f, 1535.5f, 664.9f, 3.6f }, { 2073.2f, 1559.9f, 656.2f, 3.6f } },
    { { 2108.4f, 1537.7f, 665.1f, 3.6f }, { 2070.3f, 1562.3f, 655.9f, 3.6f } },
    { { 2105.8f, 1537.8f, 664.6f, 3.6f }, { 2068.4f, 1560.0f, 654.7f, 3.6f } },
};

struct classic_npc_malfunctioning_cyclone_construct : public ScriptedAI
{
    classic_npc_malfunctioning_cyclone_construct(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    void JustAppeared() override
    {
        _events.Reset();
        _busy = false;
        me->GetMotionMaster()->MoveRandom(CycloneWanderDistance);
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_START_ORCHARD_EVENT || _busy)
            return;

        _busy = true;
        _events.ScheduleEvent(EVENT_BANDITS_APPEAR, CycloneBanditsDelay);
    }

    void JustSummoned(Creature* summon) override
    {
        _summons.Summon(summon);
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        _summons.Despawn(summon);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        if (id == POINT_CONSTRUCT_STRIKE)
        {
            me->Yell("THREAT DETECTED. ADMINISTERING VIOLENCE.", LANG_UNIVERSAL);
            _events.ScheduleEvent(EVENT_CONSTRUCT_STRIKE, 3s);
        }
        else if (id == POINT_CONSTRUCT_HOME)
            me->GetMotionMaster()->MoveRandom(CycloneWanderDistance);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_BANDITS_APPEAR:
                {
                    _summons.DespawnAll();
                    if (Creature* ferauu = me->SummonCreature(NPC_FERAUU_THE_BLUDGEON, FerauuStart, TEMPSUMMON_MANUAL_DESPAWN))
                    {
                        ferauu->Yell("Little apple farmers! Ferauu is here to collect what is owed!", LANG_UNIVERSAL);
                        ferauu->GetMotionMaster()->MovePoint(0, FerauuEnd);
                    }
                    for (auto const& [start, end] : HenchmanPaths)
                        if (Creature* henchman = me->SummonCreature(NPC_BANDIT_HENCHMAN, start, TEMPSUMMON_MANUAL_DESPAWN))
                            henchman->GetMotionMaster()->MovePoint(0, end);
                    _events.ScheduleEvent(EVENT_CONSTRUCT_GO, CycloneStrikeDelay);
                    break;
                }
                case EVENT_CONSTRUCT_GO:
                    me->GetMotionMaster()->Clear();
                    me->GetMotionMaster()->MovePoint(POINT_CONSTRUCT_STRIKE, ConstructStrikeAt);
                    break;
                case EVENT_CONSTRUCT_STRIKE:
                {
                    if (Creature* ferauu = me->FindNearestCreature(NPC_FERAUU_THE_BLUDGEON, 40.0f))
                        me->CastSpell(ferauu, SPELL_CONSTRUCT_CHAIN_LIGHTNING, true);
                    for (ObjectGuid const& guid : _summons)
                        if (Creature* bandit = ObjectAccessor::GetCreature(*me, guid))
                            if (bandit->IsAlive())
                            {
                                bandit->KillSelf();
                                bandit->DespawnOrUnsummon(10s);
                            }
                    _events.ScheduleEvent(EVENT_CONSTRUCT_DONE, 3s);
                    break;
                }
                case EVENT_CONSTRUCT_DONE:
                    me->Yell("FUNCTION FULFILLED. RESUMING REST MODE.", LANG_UNIVERSAL);
                    me->GetMotionMaster()->MovePoint(POINT_CONSTRUCT_HOME, me->GetHomePosition());
                    if (Creature* aamelia = me->FindNearestCreature(NPC_AAMELIA_WINDFIELD, 250.0f))
                        if (aamelia->AI())
                            aamelia->AI()->DoAction(ACTION_ORCHARD_EVENT_DONE);
                    _busy = false;
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    SummonList _summons;
    bool _busy = false;
};

/*######
## classic_npc_aamelia_windfield (252800)
######*/

// her run to the orchard's edge (official beta sniff 70205)
static Position const AameliaPath[] =
{
    { 1939.4f, 1615.4f, 647.4f },
    { 1950.4f, 1609.0f, 647.2f },
    { 1965.9f, 1603.3f, 647.1f },
    { 1974.1f, 1600.9f, 647.3f },
    { 1984.5f, 1599.6f, 647.5f },
    { 2003.9f, 1598.9f, 648.7f },
    { 2023.8f, 1595.1f, 650.3f },
    { 2034.4f, 1592.7f, 651.5f },
    { 2045.7f, 1588.5f, 652.5f },
};

struct classic_npc_aamelia_windfield : public ScriptedAI
{
    classic_npc_aamelia_windfield(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        _events.Reset();
        _fighters.clear();
        _busy = false;
        _returning = false;
    }

    bool OnGossipSelect(Player* player, uint32 menuId, uint32 gossipListId) override
    {
        // "I'm ready to fight, Aamelia." (option 136302) is the menu's only option, order 0
        if (menuId != GOSSIP_MENU_AAMELIA || gossipListId != 0)
            return false;

        CloseGossipMenuFor(player);
        player->KilledMonsterCredit(NPC_AAMELIA_WINDFIELD);
        _fighters.insert(player->GetGUID());

        if (!_busy && !_returning)
            StartRun();
        return true;
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        if (id == POINT_AAMELIA_HOME)
        {
            _returning = false;
            me->SetFacingTo(me->GetHomePosition().GetOrientation());
            if (!_fighters.empty()) // someone got ready while she ran home
                StartRun();
            return;
        }

        if (id + 1 < std::size(AameliaPath))
        {
            me->GetMotionMaster()->MovePoint(id + 1, AameliaPath[id + 1]);
            return;
        }

        if (Creature* construct = me->FindNearestCreature(250929, 250.0f))
            if (construct->AI())
                construct->AI()->DoAction(ACTION_START_ORCHARD_EVENT);
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_ORCHARD_EVENT_DONE || !_busy)
            return;

        DoCastSelf(SPELL_MAKE_YOUR_STAND, true);
        for (ObjectGuid const& guid : _fighters)
            if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
                if (player->GetQuestStatus(QUEST_STANDING_OUR_GROUND) == QUEST_STATUS_INCOMPLETE && player->IsWithinDistInMap(me, 100.0f))
                    player->KilledMonsterCredit(NPC_FERAUU_THE_BLUDGEON);
        _fighters.clear();
        _busy = false;
        _events.ScheduleEvent(EVENT_AAMELIA_GO_HOME, 5s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (_events.ExecuteEvent() == EVENT_AAMELIA_GO_HOME)
        {
            _returning = true;
            me->GetMotionMaster()->MovePoint(POINT_AAMELIA_HOME, me->GetHomePosition());
        }
    }

private:
    void StartRun()
    {
        _busy = true;
        me->Say("It's time, stranger. One way or another, let's finish this.", LANG_UNIVERSAL);
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MovePoint(0, AameliaPath[0]);
    }

    EventMap _events;
    GuidUnorderedSet _fighters;
    bool _busy = false;
    bool _returning = false;
};

/*######
## classic_npc_belathaan_brightwish (256247)
######*/

// Classic 1.60 (WoW Forever), Gustberry Lowlands, quest A Firm Response (93746, official beta sniff 70205): the player confronts
// Belathaan ("Belathaan! The Windshapers demand to know..."), High Priestess Lorthuna and two Living Storms come down from the sky
// next to him and hover there; ~31 s later the storms strike him with lightning (he dies, the player gets the credit) and turn on the
// player. Lorthuna stays out of the fight.
enum BelathaanEvent
{
    NPC_BELATHAAN_BRIGHTWISH        = 256247,
    NPC_HIGH_PRIESTESS_LORTHUNA     = 256249,
    NPC_LIVING_STORM                = 256250,
    GOSSIP_MENU_BELATHAAN           = 41553,
    SPELL_LIGHTNING_FROM_SKY_VFX    = 414887,
    SPELL_STORM_LIGHTNING_STRIKE    = 1268650,

    SAY_BELATHAAN_NOT_WITH_ME       = 0,
    SAY_BELATHAAN_BE_REASONABLE     = 1,
    SAY_LORTHUNA_MATTERS_NOT        = 0,
    SAY_LORTHUNA_GOODBYE            = 1,

    EVENT_BELATHAAN_STORMS_ARRIVE   = 1,
    EVENT_BELATHAAN_SAY_NOT_WITH_ME,
    EVENT_LORTHUNA_SAY_MATTERS_NOT,
    EVENT_BELATHAAN_SAY_REASONABLE,
    EVENT_LORTHUNA_SAY_GOODBYE,
    EVENT_BELATHAAN_STORMS_STRIKE,
    EVENT_BELATHAAN_DIE
};

static Position const BelathaanLorthuna = { 2853.2f, 896.5f, 773.2f, 3.1f };

static Position const BelathaanStorms[] =
{
    { 2853.7f, 890.9f, 773.1f, 2.5f },
    { 2852.4f, 901.7f, 773.4f, 3.6f },
};

struct classic_npc_belathaan_brightwish : public ScriptedAI
{
    classic_npc_belathaan_brightwish(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    void JustAppeared() override
    {
        _events.Reset();
        _summons.DespawnAll();
        _players.clear();
        _busy = false;
    }

    void JustSummoned(Creature* summon) override
    {
        _summons.Summon(summon);
        summon->SetImmuneToPC(true);                    // hover next to him until they strike (Lorthuna for good)
        summon->SetReactState(REACT_PASSIVE);
        summon->SetFacingToObject(me);
        summon->CastSpell(summon, SPELL_LIGHTNING_FROM_SKY_VFX, true);
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        _summons.Despawn(summon);
    }

    bool OnGossipSelect(Player* player, uint32 menuId, uint32 gossipListId) override
    {
        if (menuId != GOSSIP_MENU_BELATHAAN || gossipListId != 0)
            return false;

        CloseGossipMenuFor(player);
        _players.insert(player->GetGUID());
        if (!_busy)
        {
            _busy = true;
            _events.ScheduleEvent(EVENT_BELATHAAN_STORMS_ARRIVE, 5700ms);
            _events.ScheduleEvent(EVENT_BELATHAAN_SAY_NOT_WITH_ME, 17900ms);
            _events.ScheduleEvent(EVENT_LORTHUNA_SAY_MATTERS_NOT, 23400ms);
            _events.ScheduleEvent(EVENT_BELATHAAN_SAY_REASONABLE, 32400ms);
            _events.ScheduleEvent(EVENT_LORTHUNA_SAY_GOODBYE, 36200ms);
            _events.ScheduleEvent(EVENT_BELATHAAN_STORMS_STRIKE, 36500ms);
        }
        return true;
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_BELATHAAN_STORMS_ARRIVE:
                    // the storms leave a minute out of combat (longer than the wait for the strike), their corpses a minute after dying
                    me->SummonCreature(NPC_HIGH_PRIESTESS_LORTHUNA, BelathaanLorthuna, TEMPSUMMON_TIMED_DESPAWN, 75s);
                    for (Position const& pos : BelathaanStorms)
                        me->SummonCreature(NPC_LIVING_STORM, pos, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 60s);
                    break;
                case EVENT_BELATHAAN_SAY_NOT_WITH_ME:
                    Talk(SAY_BELATHAAN_NOT_WITH_ME);
                    break;
                case EVENT_BELATHAAN_SAY_REASONABLE:
                    Talk(SAY_BELATHAAN_BE_REASONABLE);
                    break;
                case EVENT_LORTHUNA_SAY_MATTERS_NOT:
                case EVENT_LORTHUNA_SAY_GOODBYE:
                    for (ObjectGuid const& guid : _summons)
                        if (Creature* lorthuna = ObjectAccessor::GetCreature(*me, guid))
                            if (lorthuna->GetEntry() == NPC_HIGH_PRIESTESS_LORTHUNA)
                                lorthuna->AI()->Talk(eventId == EVENT_LORTHUNA_SAY_MATTERS_NOT ? SAY_LORTHUNA_MATTERS_NOT : SAY_LORTHUNA_GOODBYE);
                    break;
                case EVENT_BELATHAAN_STORMS_STRIKE:
                {
                    Player* target = nullptr;
                    for (ObjectGuid const& guid : _players)
                    {
                        if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
                        {
                            if (player->IsWithinDistInMap(me, 60.0f))
                            {
                                player->KilledMonsterCredit(NPC_BELATHAAN_BRIGHTWISH);
                                if (!target)
                                    target = player;
                            }
                        }
                    }

                    for (ObjectGuid const& guid : _summons)
                    {
                        Creature* storm = ObjectAccessor::GetCreature(*me, guid);
                        if (!storm || storm->GetEntry() != NPC_LIVING_STORM)
                            continue;

                        storm->CastSpell(me, SPELL_STORM_LIGHTNING_STRIKE, true);
                        storm->SetImmuneToPC(false);
                        storm->SetReactState(REACT_AGGRESSIVE);
                        if (target)
                            storm->AI()->AttackStart(target);
                    }

                    _players.clear();
                    _events.ScheduleEvent(EVENT_BELATHAAN_DIE, 1s);
                    break;
                }
                case EVENT_BELATHAAN_DIE:
                    // the lightning kills him; if it did not, he still goes down. He comes back with his spawn's respawn time
                    _busy = false;
                    if (me->IsAlive())
                        me->KillSelf();
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    SummonList _summons;
    GuidUnorderedSet _players;
    bool _busy = false;
};

/*######
## Zephras Isle story scenes (official beta sniff 70205 of 2026-10-04): timed dialog helper
######*/

// One line of a scene: at AtMs after the start, the nearest creature of Speaker (within 120 yd of the scene owner) says Group.
struct ClassicSceneLine
{
    uint32 AtMs;
    uint32 Speaker;
    uint8 Group;
};

class ClassicSceneDialog
{
public:
    void Start(ClassicSceneLine const* lines, size_t count)
    {
        _lines = lines;
        _count = count;
        _next = 0;
        _elapsed = 0;
    }

    void Stop() { _lines = nullptr; }
    bool IsActive() const { return _lines != nullptr; }

    void Update(Creature* owner, uint32 diff)
    {
        if (!_lines)
            return;

        _elapsed += diff;
        while (_next < _count && _lines[_next].AtMs <= _elapsed)
        {
            ClassicSceneLine const& line = _lines[_next++];
            Creature* speaker = line.Speaker == owner->GetEntry() ? owner : owner->FindNearestCreature(line.Speaker, 120.0f);
            if (speaker && speaker->IsAIEnabled())
                speaker->AI()->Talk(line.Group);
        }

        if (_next >= _count)
            _lines = nullptr;
    }

private:
    ClassicSceneLine const* _lines = nullptr;
    size_t _count = 0;
    size_t _next = 0;
    uint32 _elapsed = 0;
};

// credit every player near the scene who has the quest open
static void ClassicSceneCredit(Creature* owner, uint32 questId, uint32 creditEntry, float range)
{
    std::list<Player*> players;
    owner->GetPlayerListInGrid(players, range);
    for (Player* player : players)
        if (player->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
            player->KilledMonsterCredit(creditEntry);
}

enum ZephrasStory
{
    NPC_TALAANIS_SHADOWSONG         = 252476,
    NPC_VALENNIA_STORMFIST          = 252383,
    NPC_HOLO_LORTHUNA               = 264712,
    NPC_HOLO_HAALIEN                = 264713,
    NPC_MUSTER_VALENNIA             = 253844,
    NPC_MUSTER_AYESSA               = 253813,
    NPC_MUSTER_ELAADRIN             = 253812,

    QUEST_THE_CULTS_TRUE_PLANS      = 94568,
    QUEST_DESPERATE_TIMES           = 92640,
    QUEST_MAKING_OUR_MOVE           = 92947,

    GOSSIP_MENU_TALAANIS_CRYSTAL    = 42169,
    GOSSIP_MENU_VALENNIA_NOD        = 41312,

    SPELL_RECALL_CRYSTAL_PROJECTION = 1269135,
    SPELL_RECALL_CRYSTAL_FADE       = 1292781,

    EVENT_CRYSTAL_HOLOGRAMS         = 1,
    EVENT_CRYSTAL_FADE,
    EVENT_CRYSTAL_DESPAWN
};

/*######
## classic_npc_talaanis_shadowsong (252476): quest The Cult's True Plans (94568), the recall crystal
######*/

// "High Elder, what about this crystal?" (menu 42169): Talaanis and Valennia activate the turncoat's recall crystal; a broken recording
// of Lorthuna and Commander Haalien plays above it (credit 264712), then they work out what the wards are.
static ClassicSceneLine const RecallCrystalLines[] =
{
    {   900, NPC_TALAANIS_SHADOWSONG, 0 },  // Wait, yes... this crystal! Valennia, is this...
    {  4800, NPC_VALENNIA_STORMFIST,  3 },  // ...a recall crystal!
    {  9100, NPC_TALAANIS_SHADOWSONG, 1 },  // This is surely what the turncoat hoped to offer us...
    { 15200, NPC_VALENNIA_STORMFIST,  4 },  // The recall crystal is badly damaged, though...
    { 22900, NPC_TALAANIS_SHADOWSONG, 2 },  // Activate it, Valennia.
    { 29000, NPC_HOLO_LORTHUNA,       0 },  // ...do not delay. Muster our forces...
    { 38700, NPC_HOLO_LORTHUNA,       1 },  // ...quickly. With our forces...
    { 45400, NPC_HOLO_LORTHUNA,       2 },  // I alone am.....this task...
    { 53500, NPC_VALENNIA_STORMFIST,  5 },  // The crystal is too damaged to play any further...
    { 59500, NPC_TALAANIS_SHADOWSONG, 3 },  // Wards... what is this about wards, Valennia?
    { 63700, NPC_VALENNIA_STORMFIST,  6 },  // The anchor pylons keep us suspended in this realm...
    { 73700, NPC_TALAANIS_SHADOWSONG, 4 },  // ...this is her plan. Bring down the wards and let her 'Windlord' in!
};

static Position const RecallCrystalLorthuna = { 2114.7f, 545.6f, 735.4f, 0.0f };
static Position const RecallCrystalHaalien  = { 2118.8f, 546.1f, 735.4f, 3.19f };

struct classic_npc_talaanis_shadowsong : public ScriptedAI
{
    classic_npc_talaanis_shadowsong(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    void JustAppeared() override
    {
        _events.Reset();
        _dialog.Stop();
        _summons.DespawnAll();
    }

    void JustSummoned(Creature* summon) override
    {
        _summons.Summon(summon);
        summon->SetReactState(REACT_PASSIVE);
        summon->SetUninteractible(true);
        summon->CastSpell(summon, SPELL_RECALL_CRYSTAL_PROJECTION, true);
    }

    bool OnGossipSelect(Player* player, uint32 menuId, uint32 gossipListId) override
    {
        if (menuId != GOSSIP_MENU_TALAANIS_CRYSTAL || gossipListId != 0)
            return false;

        CloseGossipMenuFor(player);
        if (!_dialog.IsActive())
        {
            _dialog.Start(RecallCrystalLines, std::size(RecallCrystalLines));
            _events.ScheduleEvent(EVENT_CRYSTAL_HOLOGRAMS, 24s);
            _events.ScheduleEvent(EVENT_CRYSTAL_FADE, 49100ms);
            _events.ScheduleEvent(EVENT_CRYSTAL_DESPAWN, 50400ms);
        }
        return true;
    }

    void UpdateAI(uint32 diff) override
    {
        _dialog.Update(me, diff);
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_CRYSTAL_HOLOGRAMS:
                    me->SummonCreature(NPC_HOLO_LORTHUNA, RecallCrystalLorthuna, TEMPSUMMON_TIMED_DESPAWN, 60s);
                    me->SummonCreature(NPC_HOLO_HAALIEN, RecallCrystalHaalien, TEMPSUMMON_TIMED_DESPAWN, 60s);
                    break;
                case EVENT_CRYSTAL_FADE:
                    for (ObjectGuid const& guid : _summons)
                        if (Creature* holo = ObjectAccessor::GetCreature(*me, guid))
                            holo->CastSpell(holo, SPELL_RECALL_CRYSTAL_FADE, true);
                    ClassicSceneCredit(me, QUEST_THE_CULTS_TRUE_PLANS, NPC_HOLO_LORTHUNA, 40.0f);
                    break;
                case EVENT_CRYSTAL_DESPAWN:
                    _summons.DespawnAll();
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    SummonList _summons;
    ClassicSceneDialog _dialog;
};

/*######
## classic_npc_valennia_stormfist (252383): quest Desperate Times (92640)
######*/

// "<Nod at Valennia.>" (menu 41312) gives "Speak with Valennia Stormfist" and she lays out the plan.
static ClassicSceneLine const DesperateTimesLines[] =
{
    {   500, NPC_VALENNIA_STORMFIST, 0 },   // With the cult running roughshod over most of the island...
    { 10400, NPC_VALENNIA_STORMFIST, 1 },   // I believe that with a proper distraction at the shrine...
    { 24600, NPC_VALENNIA_STORMFIST, 2 },   // Go and speak with the leaders of the Windshapers and the High Order...
};

struct classic_npc_valennia_stormfist : public ScriptedAI
{
    classic_npc_valennia_stormfist(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override { _dialog.Stop(); }

    bool OnGossipSelect(Player* player, uint32 menuId, uint32 gossipListId) override
    {
        if (menuId != GOSSIP_MENU_VALENNIA_NOD || gossipListId != 0)
            return false;

        CloseGossipMenuFor(player);
        player->KilledMonsterCredit(NPC_VALENNIA_STORMFIST);
        if (!_dialog.IsActive())
            _dialog.Start(DesperateTimesLines, std::size(DesperateTimesLines));
        return true;
    }

    void UpdateAI(uint32 diff) override
    {
        _dialog.Update(me, diff);
    }

private:
    ClassicSceneDialog _dialog;
};

/*######
## classic_npc_valennia_muster (253844): quest Making Our Move (92947), the leaders in Valanaar
######*/

// Accepting Making Our Move from Valennia at the muster: Elaadrin and Ayessa report for the assault on the shrine.
static ClassicSceneLine const MusterLines[] =
{
    {   900, NPC_MUSTER_ELAADRIN, 0 },      // We came as soon as we were told what was happening...
    {  7000, NPC_MUSTER_AYESSA,   0 },      // ...and the Windshapers. We are ready to support an assault on the Shrine, Valennia.
    { 12100, NPC_MUSTER_VALENNIA, 0 },      // You have my thanks. Being truthful, however...
    { 21700, NPC_MUSTER_AYESSA,   1 },      // We understand. As it happens Elaadrin and I spoke...
    { 32600, NPC_MUSTER_VALENNIA, 1 },      // Outside... of Zephras?
    { 39000, NPC_MUSTER_ELAADRIN, 1 },      // It's... coming from a little further away than that...
    { 43900, NPC_MUSTER_VALENNIA, 2 },      // Very well then. Time is not with us...
};

struct classic_npc_valennia_muster : public ScriptedAI
{
    classic_npc_valennia_muster(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override { _dialog.Stop(); }

    void OnQuestAccept(Player* /*player*/, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_MAKING_OUR_MOVE && !_dialog.IsActive())
            _dialog.Start(MusterLines, std::size(MusterLines));
    }

    void UpdateAI(uint32 diff) override
    {
        _dialog.Update(me, diff);
    }

private:
    ClassicSceneDialog _dialog;
};

/*######
## classic_npc_ayessa_spire (253849): quest Confront Lorthuna (92646), the battle at the spire
######*/

// The party talks to Ayessa on the spire; she and Elaadrin cross the bridge with their shamans and mages. Lorthuna's ritual calls
// Rohash, the Lord of the East Wind, who throws her from the spire, sends Baron Anvillaxx and his storms at the allies and stays to
// talk until Muln Earthfury and Archmage Ansirem Runeweaver come through a transplanar rift. Rohash leaves (credit 256634) and the
// leaders open the way back to Valanaar (the Portal to Valanaar stands at the spire).
enum ConfrontLorthuna
{
    NPC_SPIRE_AYESSA            = 253849,
    NPC_SPIRE_ELAADRIN          = 253847,
    NPC_SPIRE_SHAMAN            = 256618,
    NPC_SPIRE_MAGE              = 256619,
    NPC_SPIRE_LORTHUNA          = 252957,
    NPC_ROHASH                  = 255833,
    NPC_BARON_ANVILLAXX         = 256617,
    NPC_MALEVOLENT_STORM        = 255830,
    NPC_MULN_EARTHFURY          = 256620,
    NPC_ANSIREM_RUNEWEAVER      = 256621,
    NPC_EARTHEN_RING_SHAMAN     = 257862,
    NPC_KIRIN_TOR_MAGE          = 257866,
    NPC_CONFRONT_CREDIT         = 256634,
    GO_TRANSPLANAR_RIFT         = 617075,
    GO_PORTAL_TO_VALANAAR       = 617084,
    SPELL_ELAADRIN_OPEN_PORTAL  = 1269466,

    QUEST_CONFRONT_LORTHUNA     = 92646,
    QUEST_CONFRONT_LORTHUNA_2   = 93835,    // the same quest on the other story line (same credit 256634)
    NPC_TEXT_SPIRE_AYESSA       = 20253849,

    SPELL_LORTHUNA_RITUAL       = 1265896,
    SPELL_STORM_ARRIVAL         = 1269456,
    SPELL_TELEPORT_VISUAL       = 364884,

    EVENT_SPIRE_MARCH           = 1,
    EVENT_SPIRE_FACE,
    EVENT_SPIRE_ROHASH,
    EVENT_SPIRE_THROW,
    EVENT_SPIRE_LORTHUNA_GONE,
    EVENT_SPIRE_BARON,
    EVENT_SPIRE_STORMS,
    EVENT_SPIRE_RIFT,
    EVENT_SPIRE_GUARDS,
    EVENT_SPIRE_BLAST,
    EVENT_SPIRE_ROHASH_LEAVES,
    EVENT_SPIRE_PORTAL,
    EVENT_SPIRE_LEAVE,
    EVENT_SPIRE_RESET
};

static ClassicSceneLine const ConfrontLorthunaLines[] =
{
    {  25900, NPC_SPIRE_LORTHUNA,     0 },  // Ayessa... my former sister...
    {  31100, NPC_SPIRE_AYESSA,       0 },  // Lorthuna! Stop this madness...
    {  38300, NPC_SPIRE_LORTHUNA,     1 },  // Bring the wards down? The wards have been down for days, fools...
    {  46500, NPC_SPIRE_ELAADRIN,     0 },  // I'll make this easy for you, witch...
    {  55600, NPC_SPIRE_LORTHUNA,     2 },  // Empty threats, Elaadrin...
    {  65700, NPC_SPIRE_LORTHUNA,     3 },  // Ahh... yes! The call has been answered...
    {  70900, NPC_ROHASH,             0 },  // Who dares call the Lord of the East Wind?
    {  76500, NPC_ROHASH,             1 },  // Wait... what is this place?
    {  82500, NPC_ROHASH,             2 },  // So the rumors were true...
    {  90600, NPC_SPIRE_LORTHUNA,     4 },  // Yes! Yes! It is by my will that the way to Zephras is open to you!...
    {  99000, NPC_ROHASH,             3 },  // You dare to command me, mortal?...
    { 103800, NPC_SPIRE_LORTHUNA,     5 },  // No! Never, I would never dare!
    { 105300, NPC_ROHASH,             4 },  // Well, you certainly never will again, insect. Hah!
    { 108800, NPC_ROHASH,             5 },  // Enjoy your remaining time as "guests"...
    { 116600, NPC_ROHASH,             6 },  // ...the TRUE children of Skywall...
    { 162000, NPC_MULN_EARTHFURY,     0 },  // Sorry that we are late, Ayessa.
    { 166100, NPC_ANSIREM_RUNEWEAVER, 0 },  // It took a bit of work to convince Archmage Rhonin...
    { 172700, NPC_SPIRE_ELAADRIN,     1 },  // Ansirem! This isn't how I expected our first meeting to go...
    { 178000, NPC_SPIRE_AYESSA,       1 },  // Nor is this how I envisioned our first meeting, Muln.
    { 181600, NPC_ROHASH,             7 },  // Well if this isn't an inspirational sight...
    { 186500, NPC_MULN_EARTHFURY,     1 },  // We have no quarrel with you, Lord of the East Wind...
    { 189900, NPC_ROHASH,             8 },  // Very well, mortals. For now enjoy your respite...
    { 204500, NPC_ANSIREM_RUNEWEAVER, 1 },  // I think it's time for us to be leaving this place...
    { 209200, NPC_SPIRE_AYESSA,       2 },  // Muln Earthfury of the Earthen Ring--the Windshapers would be happy...
    { 214900, NPC_SPIRE_ELAADRIN,     2 },  // Yes, and you, Archmage Runeweaver...
};

// the leaders' walk over the wind bridge to Lorthuna's spire (official monster moves)
static WaypointNode const SpireAyessaPath[] =
{
    { 0, 2977.6f, 103.1f, 1170.2f }, { 1, 2967.0f, 97.3f, 1175.8f }, { 2, 2954.3f, 83.1f, 1181.0f }, { 3, 2952.4f, 74.3f, 1181.9f },
    { 4, 2952.8f, 59.9f, 1182.0f }, { 5, 2957.9f, 50.8f, 1182.0f }, { 6, 2962.7f, 44.0f, 1182.0f }, { 7, 2971.7f, 45.2f, 1182.0f },
    { 8, 2986.4f, 51.7f, 1182.0f },
};
static WaypointNode const SpireElaadrinPath[] =
{
    { 0, 2996.1f, 107.5f, 1164.7f }, { 1, 2980.7f, 102.1f, 1169.2f }, { 2, 2966.7f, 94.0f, 1176.8f }, { 3, 2957.4f, 83.5f, 1180.7f },
    { 4, 2954.8f, 75.6f, 1181.7f }, { 5, 2954.7f, 64.8f, 1182.0f }, { 6, 2959.3f, 52.5f, 1182.0f }, { 7, 2969.9f, 47.6f, 1182.0f },
    { 8, 2979.9f, 54.1f, 1182.0f },
};

static Position const SpireLorthunaPos      = { 2986.6f, 59.9f, 1182.0f, 4.33f };
static Position const SpireLorthunaFall     = { 2965.4f, 9.5f, 1165.4f, 4.33f };
static Position const SpireRohashPos        = { 2991.8f, 70.8f, 1182.0f, 4.08f };
static Position const SpireBaronPos         = { 2986.4f, 59.3f, 1182.0f, 4.28f };
static Position const SpireFirstStorms[]    = { { 2978.8f, 60.2f, 1182.0f, 4.60f }, { 2991.2f, 54.2f, 1182.0f, 4.16f } };
static Position const SpireStormWave[]      =
{
    { 2977.9f, 73.6f, 1182.0f, 5.59f }, { 2976.2f, 68.3f, 1182.0f, 5.78f }, { 3003.1f, 63.6f, 1182.0f, 3.31f },
    { 3002.0f, 69.2f, 1182.0f, 4.13f }, { 3001.3f, 58.8f, 1182.0f, 2.89f }, { 2981.5f, 77.2f, 1182.0f, 5.37f },
};
static Position const SpireMulnPos          = { 2983.3f, 42.9f, 1182.0f, 1.12f };
static Position const SpireAnsiremPos       = { 2977.4f, 45.6f, 1182.0f, 1.16f };
static Position const SpireEarthenShamanPos = { 2985.8f, 41.6f, 1182.0f, 1.25f };
static Position const SpireKirinTorMagePos  = { 2974.9f, 45.3f, 1182.0f, 1.25f };
static Position const SpireRiftPos          = { 2977.96f, 40.88f, 1181.88f, 1.05f };
static Position const SpirePortalHomePos    = { 2982.0f, 55.08f, 1181.88f, 1.18f };

struct classic_npc_ayessa_spire : public ScriptedAI
{
    classic_npc_ayessa_spire(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    void JustAppeared() override
    {
        _events.Reset();
        _dialog.Stop();
        _summons.DespawnAll();
        _busy = false;
    }

    void JustSummoned(Creature* summon) override
    {
        _summons.Summon(summon);
        switch (summon->GetEntry())
        {
            case NPC_SPIRE_LORTHUNA:
                summon->SetReactState(REACT_PASSIVE);
                summon->SetImmuneToAll(true);
                summon->CastSpell(summon, SPELL_LORTHUNA_RITUAL, true);
                break;
            case NPC_ROHASH:
                summon->SetReactState(REACT_PASSIVE);
                summon->SetImmuneToAll(true);
                summon->CastSpell(summon, SPELL_STORM_ARRIVAL, true);
                break;
            case NPC_MULN_EARTHFURY:
            case NPC_ANSIREM_RUNEWEAVER:
            case NPC_EARTHEN_RING_SHAMAN:
            case NPC_KIRIN_TOR_MAGE:
                summon->SetReactState(REACT_PASSIVE);
                summon->CastSpell(summon, SPELL_TELEPORT_VISUAL, true);
                break;
            default:
                break;
        }
    }

    static bool HasConfrontQuest(Player* player)
    {
        return player->GetQuestStatus(QUEST_CONFRONT_LORTHUNA) == QUEST_STATUS_INCOMPLETE
            || player->GetQuestStatus(QUEST_CONFRONT_LORTHUNA_2) == QUEST_STATUS_INCOMPLETE;
    }

    bool OnGossipHello(Player* player) override
    {
        InitGossipMenuFor(player, 0);
        if (HasConfrontQuest(player) && !_busy)
            AddGossipItemFor(player, GossipOptionNpc::None, "I am ready to confront Lorthuna.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
        SendGossipMenuFor(player, NPC_TEXT_SPIRE_AYESSA, me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 /*gossipListId*/) override
    {
        CloseGossipMenuFor(player);
        if (_busy || !HasConfrontQuest(player))
            return true;

        _busy = true;
        _dialog.Start(ConfrontLorthunaLines, std::size(ConfrontLorthunaLines));
        me->SummonCreature(NPC_SPIRE_LORTHUNA, SpireLorthunaPos, TEMPSUMMON_MANUAL_DESPAWN);
        _events.ScheduleEvent(EVENT_SPIRE_MARCH, 2s);
        _events.ScheduleEvent(EVENT_SPIRE_FACE, 25400ms);
        _events.ScheduleEvent(EVENT_SPIRE_ROHASH, 66400ms);
        _events.ScheduleEvent(EVENT_SPIRE_THROW, 103300ms);
        _events.ScheduleEvent(EVENT_SPIRE_LORTHUNA_GONE, 107s);
        _events.ScheduleEvent(EVENT_SPIRE_BARON, 117s);
        _events.ScheduleEvent(EVENT_SPIRE_STORMS, 145700ms);
        _events.ScheduleEvent(EVENT_SPIRE_RIFT, 153s);
        _events.ScheduleEvent(EVENT_SPIRE_GUARDS, 154200ms);
        _events.ScheduleEvent(EVENT_SPIRE_BLAST, 159s);
        _events.ScheduleEvent(EVENT_SPIRE_ROHASH_LEAVES, 198300ms);
        _events.ScheduleEvent(EVENT_SPIRE_PORTAL, 220200ms);
        _events.ScheduleEvent(EVENT_SPIRE_LEAVE, 223500ms);
        _events.ScheduleEvent(EVENT_SPIRE_RESET, 226s);
        return true;
    }

    void ForAllies(std::function<void(Creature*)> const& fn)
    {
        for (uint32 entry : { NPC_SPIRE_SHAMAN, NPC_SPIRE_MAGE })
        {
            std::list<Creature*> allies;
            me->GetCreatureListWithEntryInGrid(allies, entry, 60.0f);
            for (Creature* ally : allies)
                fn(ally);
        }
    }

    Creature* Summoned(uint32 entry)
    {
        for (ObjectGuid const& guid : _summons)
            if (Creature* summon = ObjectAccessor::GetCreature(*me, guid))
                if (summon->GetEntry() == entry)
                    return summon;
        return nullptr;
    }

    void UpdateAI(uint32 diff) override
    {
        _dialog.Update(me, diff);
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SPIRE_MARCH:
                {
                    WaypointPath ayessaPath(0, std::vector<WaypointNode>(std::begin(SpireAyessaPath), std::end(SpireAyessaPath)), WaypointMoveType::Run);
                    ayessaPath.BuildSegments();
                    me->GetMotionMaster()->MovePath(ayessaPath, false);
                    if (Creature* elaadrin = me->FindNearestCreature(NPC_SPIRE_ELAADRIN, 40.0f))
                    {
                        WaypointPath elaadrinPath(0, std::vector<WaypointNode>(std::begin(SpireElaadrinPath), std::end(SpireElaadrinPath)), WaypointMoveType::Run);
                        elaadrinPath.BuildSegments();
                        elaadrin->GetMotionMaster()->MovePath(elaadrinPath, false);
                    }
                    ForAllies([this](Creature* ally) { ally->GetMotionMaster()->MoveFollow(me, frand(3.0f, 8.0f), ChaseAngle(frand(2.4f, 3.9f))); });
                    break;
                }
                case EVENT_SPIRE_FACE:
                    if (Creature* lorthuna = Summoned(NPC_SPIRE_LORTHUNA))
                    {
                        me->SetFacingToObject(lorthuna);
                        if (Creature* elaadrin = me->FindNearestCreature(NPC_SPIRE_ELAADRIN, 40.0f))
                            elaadrin->SetFacingToObject(lorthuna);
                    }
                    break;
                case EVENT_SPIRE_ROHASH:
                    me->SummonCreature(NPC_ROHASH, SpireRohashPos, TEMPSUMMON_MANUAL_DESPAWN);
                    break;
                case EVENT_SPIRE_THROW:
                    if (Creature* lorthuna = Summoned(NPC_SPIRE_LORTHUNA))
                    {
                        lorthuna->InterruptNonMeleeSpells(false);
                        lorthuna->GetMotionMaster()->MoveJump(0, SpireLorthunaFall, 20.0f, {}, 6.0f);
                    }
                    break;
                case EVENT_SPIRE_LORTHUNA_GONE:
                    if (Creature* lorthuna = Summoned(NPC_SPIRE_LORTHUNA))
                        lorthuna->DespawnOrUnsummon(1s);
                    break;
                case EVENT_SPIRE_BARON:
                {
                    std::vector<Creature*> attackers;
                    if (Creature* baron = me->SummonCreature(NPC_BARON_ANVILLAXX, SpireBaronPos, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 120s))
                        attackers.push_back(baron);
                    for (Position const& pos : SpireFirstStorms)
                        if (Creature* storm = me->SummonCreature(NPC_MALEVOLENT_STORM, pos, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 120s))
                            attackers.push_back(storm);
                    for (Creature* attacker : attackers)
                    {
                        attacker->CastSpell(attacker, SPELL_STORM_ARRIVAL, true);
                        if (Player* player = attacker->SelectNearestPlayer(60.0f))
                            attacker->AI()->AttackStart(player);
                    }
                    if (!attackers.empty())
                        ForAllies([&attackers](Creature* ally) { ally->AI()->AttackStart(attackers.front()); });
                    break;
                }
                case EVENT_SPIRE_STORMS:
                    for (Position const& pos : SpireStormWave)
                        if (Creature* storm = me->SummonCreature(NPC_MALEVOLENT_STORM, pos, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 60s))
                        {
                            storm->SetReactState(REACT_PASSIVE);
                            storm->SetImmuneToPC(true);
                            storm->CastSpell(storm, SPELL_STORM_ARRIVAL, true);
                        }
                    break;
                case EVENT_SPIRE_RIFT:
                    me->SummonGameObject(GO_TRANSPLANAR_RIFT, SpireRiftPos, QuaternionData::fromEulerAnglesZYX(SpireRiftPos.GetOrientation(), 0.0f, 0.0f), 80s);
                    me->SummonCreature(NPC_MULN_EARTHFURY, SpireMulnPos, TEMPSUMMON_TIMED_DESPAWN, 75s);
                    me->SummonCreature(NPC_ANSIREM_RUNEWEAVER, SpireAnsiremPos, TEMPSUMMON_TIMED_DESPAWN, 75s);
                    break;
                case EVENT_SPIRE_GUARDS:
                    me->SummonCreature(NPC_EARTHEN_RING_SHAMAN, SpireEarthenShamanPos, TEMPSUMMON_TIMED_DESPAWN, 74s);
                    me->SummonCreature(NPC_KIRIN_TOR_MAGE, SpireKirinTorMagePos, TEMPSUMMON_TIMED_DESPAWN, 74s);
                    break;
                case EVENT_SPIRE_BLAST:
                    // Muln holds the storms and Ansirem blasts them (1269433 / 1269457 on the official beta)
                    for (ObjectGuid const& guid : _summons)
                        if (Creature* summon = ObjectAccessor::GetCreature(*me, guid))
                            if (summon->IsAlive() && (summon->GetEntry() == NPC_MALEVOLENT_STORM || summon->GetEntry() == NPC_BARON_ANVILLAXX))
                                summon->KillSelf();
                    ForAllies([](Creature* ally) { ally->AI()->EnterEvadeMode(); });
                    break;
                case EVENT_SPIRE_ROHASH_LEAVES:
                    ClassicSceneCredit(me, QUEST_CONFRONT_LORTHUNA, NPC_CONFRONT_CREDIT, 120.0f);
                    ClassicSceneCredit(me, QUEST_CONFRONT_LORTHUNA_2, NPC_CONFRONT_CREDIT, 120.0f);
                    if (Creature* rohash = Summoned(NPC_ROHASH))
                    {
                        rohash->CastSpell(rohash, SPELL_STORM_ARRIVAL, true);
                        rohash->DespawnOrUnsummon(2s);
                    }
                    break;
                case EVENT_SPIRE_PORTAL:
                    // Elaadrin opens the Portal to Valanaar (1269466 on the official beta); it stays for the party to go home
                    if (Creature* elaadrin = me->FindNearestCreature(NPC_SPIRE_ELAADRIN, 60.0f))
                        elaadrin->CastSpell(elaadrin, SPELL_ELAADRIN_OPEN_PORTAL, true);
                    me->SummonGameObject(GO_PORTAL_TO_VALANAAR, SpirePortalHomePos,
                        QuaternionData::fromEulerAnglesZYX(SpirePortalHomePos.GetOrientation(), 0.0f, 0.0f), 120s);
                    break;
                case EVENT_SPIRE_LEAVE:
                    me->CastSpell(me, SPELL_TELEPORT_VISUAL, true);
                    if (Creature* elaadrin = me->FindNearestCreature(NPC_SPIRE_ELAADRIN, 60.0f))
                    {
                        elaadrin->CastSpell(elaadrin, SPELL_TELEPORT_VISUAL, true);
                        elaadrin->DespawnOrUnsummon(2s, 30s);
                    }
                    break;
                case EVENT_SPIRE_RESET:
                    // back at their place on the spire for the next group
                    me->DespawnOrUnsummon(0s, 30s);
                    break;
                default:
                    break;
            }
        }
    }

private:
    EventMap _events;
    SummonList _summons;
    ClassicSceneDialog _dialog;
    bool _busy = false;
};

void AddSC_classic_npcs_special()
{
    RegisterCreatureAI(classic_npc_talaanis_shadowsong);
    RegisterCreatureAI(classic_npc_valennia_stormfist);
    RegisterCreatureAI(classic_npc_valennia_muster);
    RegisterCreatureAI(classic_npc_ayessa_spire);
    RegisterCreatureAI(classic_npc_belathaan_brightwish);
    RegisterCreatureAI(classic_npc_sickly_critter);
    RegisterCreatureAI(classic_npc_malfunctioning_cyclone_construct);
    RegisterCreatureAI(classic_npc_aamelia_windfield);
}
