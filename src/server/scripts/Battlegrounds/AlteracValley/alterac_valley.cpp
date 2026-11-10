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

#include "alterac_valley.h"
#include "ScriptMgr.h"
#include "Battleground.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "Map.h"
#include "MapReference.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellAuras.h"
#include "TemporarySummon.h"
#include "WaypointManager.h"
#include "WorldSession.h"

#include <chrono>

enum Spells
{
    SPELL_CHARGE                                  = 22911,
    SPELL_CLEAVE                                  = 20684,
    SPELL_DEMORALIZING_SHOUT                      = 23511,
    SPELL_ENRAGE                                  = 8599,
    SPELL_WHIRLWIND                               = 13736,

    // tower bowmen
    SPELL_SHOOT                                   = 22121,
    SPELL_ROOT_SELF                               = 17507,

    // mine layers and explosives experts
    SPELL_BOMB                                    = 9143,
    SPELL_FLASH_BOMB                              = 29419,
    SPELL_LANDMINE                                = 22498
};

enum Texts
{
    BROADCAST_TEXT_KORRAK_SPAWN                   = 9038
};

enum Events
{
    EVENT_CHARGE_TARGET        = 1,
    EVENT_CLEAVE               = 2,
    EVENT_DEMORALIZING_SHOUT   = 3,
    EVENT_WHIRLWIND            = 4,
    EVENT_ENRAGE               = 5,
    EVENT_CHECK_RESET          = 6,
    EVENT_WHIRLWIND_END        = 7,

    EVENT_BOMB                 = 1,
    EVENT_FLASH_BOMB           = 2,
    EVENT_LANDMINE             = 3
};

// Marshals and Warmasters of the towers and graveyards (VMaNGOS npc_WarMaster): charge whoever comes within 22 yards, a double
// Whirlwind that roots them for 4 seconds, and Enrage below 30% health
struct npc_av_marshal_or_warmaster : public ScriptedAI
{
    npc_av_marshal_or_warmaster(Creature* creature) : ScriptedAI(creature), _inWhirlwind(false) { }

    void Reset() override
    {
        events.Reset();
        EndWhirlwind();
        events.ScheduleEvent(EVENT_CHARGE_TARGET, 12s);
        events.ScheduleEvent(EVENT_CLEAVE, 8s);
        events.ScheduleEvent(EVENT_DEMORALIZING_SHOUT, 4s);
        events.ScheduleEvent(EVENT_WHIRLWIND, 12s);
        events.ScheduleEvent(EVENT_ENRAGE, 1s);
        events.ScheduleEvent(EVENT_CHECK_RESET, 5s);
    }

    void JustAppeared() override
    {
        Reset();
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (!me->IsEngaged() && me->IsWithinDistInMap(who, 22.0f) && me->IsValidAttackTarget(who) && me->IsWithinLOSInMap(who))
        {
            AttackStart(who);
            DoCast(who, SPELL_CHARGE);
            return;
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void EndWhirlwind()
    {
        if (!_inWhirlwind)
            return;

        _inWhirlwind = false;
        me->SetControlled(false, UNIT_STATE_ROOT);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_CHARGE_TARGET:
                {
                    Unit* victim = me->GetVictim();
                    float const dist = me->GetDistance(victim);
                    if (dist >= 8.0f && dist <= 25.0f)
                    {
                        DoCast(victim, SPELL_CHARGE);
                        events.ScheduleEvent(EVENT_CHARGE_TARGET, 12s, 18s);
                    }
                    else
                        events.ScheduleEvent(EVENT_CHARGE_TARGET, 5s, 10s);
                    break;
                }
                case EVENT_CLEAVE:
                    DoCastVictim(SPELL_CLEAVE);
                    events.ScheduleEvent(EVENT_CLEAVE, 8s, 10s);
                    break;
                case EVENT_DEMORALIZING_SHOUT:
                    DoCast(me, SPELL_DEMORALIZING_SHOUT);
                    events.ScheduleEvent(EVENT_DEMORALIZING_SHOUT, 14s, 20s);
                    break;
                case EVENT_WHIRLWIND:
                    DoCast(me, SPELL_WHIRLWIND);
                    if (!_inWhirlwind)
                    {
                        // the second Whirlwind follows 2 seconds later; both are cast standing still
                        _inWhirlwind = true;
                        me->SetControlled(true, UNIT_STATE_ROOT);
                        events.ScheduleEvent(EVENT_WHIRLWIND, 2s);
                        events.ScheduleEvent(EVENT_WHIRLWIND_END, 4s);
                    }
                    else
                        events.ScheduleEvent(EVENT_WHIRLWIND, 11s, 13s);
                    break;
                case EVENT_WHIRLWIND_END:
                    EndWhirlwind();
                    break;
                case EVENT_ENRAGE:
                    if (me->HealthBelowPct(31))
                    {
                        DoCast(me, SPELL_ENRAGE);
                        events.ScheduleEvent(EVENT_ENRAGE, 120s);
                    }
                    else
                        events.ScheduleEvent(EVENT_ENRAGE, 1s);
                    break;
                case EVENT_CHECK_RESET:
                {
                    Position const& _homePosition = me->GetHomePosition();
                    if (me->GetDistance2d(_homePosition.GetPositionX(), _homePosition.GetPositionY()) > 50.0f)
                    {
                        EnterEvadeMode();
                        return;
                    }
                    events.ScheduleEvent(EVENT_CHECK_RESET, 5s);
                    break;
                }
            }
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

private:
    EventMap events;
    bool _inWhirlwind;
};

// Tower and bunker bowmen (VMaNGOS npc_AlteracBowman): rooted in place, they shoot anything hostile in sight up to 80 yards
// and melee only what stands next to them; with nothing left in sight they reset after 3 seconds
struct npc_av_bowman : public ScriptedAI
{
    static constexpr float ShootRange = 80.0f;

    npc_av_bowman(Creature* creature) : ScriptedAI(creature), _shootTimer(0), _resetTimer(3000)
    {
        SetCombatMovement(false);
    }

    void Reset() override
    {
        _shootTimer = 100;
        _resetTimer = 3000;
    }

    void JustAppeared() override
    {
        RootSelf();
    }

    void JustReachedHome() override
    {
        RootSelf();
    }

    void RootSelf()
    {
        if (Aura* root = me->AddAura(SPELL_ROOT_SELF, me))
        {
            root->SetMaxDuration(-1);
            root->SetDuration(-1);
        }
    }

    bool InShootRange(Unit const* target) const
    {
        return me->IsWithinDistInMap(target, ShootRange) && me->IsWithinLOSInMap(target);
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (!me->GetVictim() && me->IsValidAttackTarget(who) && InShootRange(who))
            AttackStart(who);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!me->IsInEvadeMode() && me->IsEngaged() && (!me->GetVictim() || !InShootRange(me->GetVictim())))
        {
            if (_resetTimer <= diff)
            {
                _resetTimer = 3000;
                EnterEvadeMode();
                return;
            }
            _resetTimer -= diff;
        }

        if (!UpdateVictim())
            return;

        if (_shootTimer <= diff)
        {
            if (!me->IsWithinMeleeRange(me->GetVictim()) && !me->HasUnitState(UNIT_STATE_CASTING))
            {
                DoCastVictim(SPELL_SHOOT);
                _shootTimer = 1700;
            }
        }
        else
            _shootTimer -= diff;
    }

private:
    uint32 _shootTimer;
    uint32 _resetTimer;
};

// Mine layers and explosives experts (VMaNGOS MineNPC_AI): they pull the whole area into the fight, throw bombs, lay landmines
// and Flash Bomb druids in a feral form and pets within 35 yards (the fear only takes on beasts)
struct npc_av_mine_layer : public ScriptedAI
{
    npc_av_mine_layer(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        events.Reset();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
        events.ScheduleEvent(EVENT_BOMB, 2700ms);
        events.ScheduleEvent(EVENT_FLASH_BOMB, 5s);
        events.ScheduleEvent(EVENT_LANDMINE, 9s);
    }

    Unit* SelectFlashBombTarget() const
    {
        for (MapReference const& ref : me->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (player->IsInFeralForm() && me->GetDistance2d(player) < 35.0f && me->IsValidAttackTarget(player))
                return player;

            if (Pet* pet = player->GetPet())
                if (me->GetDistance2d(pet) < 35.0f && me->IsValidAttackTarget(pet))
                    return pet;
        }
        return nullptr;
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_BOMB:
                    DoCastVictim(SPELL_BOMB);
                    events.ScheduleEvent(EVENT_BOMB, 7500ms);
                    break;
                case EVENT_FLASH_BOMB:
                    if (Unit* target = SelectFlashBombTarget())
                    {
                        DoCast(target, SPELL_FLASH_BOMB);
                        events.ScheduleEvent(EVENT_FLASH_BOMB, 7500ms);
                    }
                    else
                        events.ScheduleEvent(EVENT_FLASH_BOMB, 1s);
                    break;
                case EVENT_LANDMINE:
                    DoCast(me, SPELL_LANDMINE);
                    events.ScheduleEvent(EVENT_LANDMINE, 9s);
                    break;
                default:
                    break;
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

private:
    EventMap events;
};

// Korrak the Bloodrager (VMaNGOS npc_korrak_the_bloodrager) joins only a battle that has gone on for two hours: on his first
// spawn he leaves and comes back at the two hour mark with a yell
struct npc_av_korrak_the_bloodrager : public ScriptedAI
{
    static constexpr Milliseconds AppearAfter = 2h;

    npc_av_korrak_the_bloodrager(Creature* creature) : ScriptedAI(creature) { }

    void JustAppeared() override
    {
        BattlegroundMap* bgMap = me->GetMap()->ToBattlegroundMap();
        Battleground* bg = bgMap ? bgMap->GetBG() : nullptr;
        if (!bg)
            return;

        Milliseconds const elapsed(bg->GetElapsedTime());
        if (elapsed < AppearAfter)
        {
            me->DespawnOrUnsummon(0s, std::chrono::ceil<Seconds>(AppearAfter - elapsed));
            return;
        }

        me->Yell(BROADCAST_TEXT_KORRAK_SPAWN);
    }
};

struct go_av_capturable_object : public GameObjectAI
{
    go_av_capturable_object(GameObject* go) : GameObjectAI(go) { }

    void Reset() override
    {
        me->setActive(true);
    }

    bool OnGossipHello(Player* player) override
    {
        if (me->GetGoState() != GO_STATE_READY)
            return true;

        if (ZoneScript* zonescript = me->GetZoneScript())
        {
            zonescript->DoAction(ACTION_AV_INTERACT_CAPTURABLE_OBJECT, player, me);
            return false;
        }

        return true;
    }
};

struct go_av_contested_object : public go_av_capturable_object
{
    go_av_contested_object(GameObject* go) : go_av_capturable_object(go) { }

    void Reset() override
    {
        go_av_capturable_object::Reset();
        _scheduler.Schedule(4min, [&](TaskContext const&)
        {
            if (ZoneScript* zonescript = me->GetZoneScript())
                zonescript->DoAction(ACTION_AV_CAPTURE_CAPTURABLE_OBJECT, me, me);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

private:
    TaskScheduler _scheduler;
};

/*
 * Classic assaults (VMaNGOS battleground_alterac.cpp: npc_AV_blood_collector, npc_AV_troops_chief, npc_cavalry, av_warrider,
 * go_av_beacon, npc_worldboss_a_av / npc_worldboss_h_av, npc_frostwolf_shaman, npc_druid_of_the_grove).
 *
 * The assault quests deliver resources that the battleground script counts (alterac_valley.h, DATA_CAV_CHALLENGE). With
 * enough of them the quest givers offer the assault:
 *   - air: the wing commanders, once rescued and back at their base, hand out beacons for the spell focus spots (a war rider
 *     or gryphon comes to the planted beacon a minute later unless the enemy disables it), or leave themselves as a war
 *     rider over the enemy base;
 *   - cavalry: the wolf / ram rider commander leads eight riders to the enemy base;
 *   - ground: the quartermaster calls a field commander with ten troops of the current tier; the Assault Orders he hands out
 *     send them marching;
 *   - world boss: the blood collector walks to the summoning site; ten players at the ritual call Lokholar or Ivus.
 * VMaNGOS steered its escorts with npc_escortAI over script_waypoint; here they run the same points (waypoint_path
 * 811000000 + entry * 10) with MovePath, which already pauses for combat and resumes after it.
 */
namespace AlteracValleyAssaults
{
enum Creatures : uint32
{
    NPC_QUARTERMASTER_A             = 12096,
    NPC_QUARTERMASTER_H             = 12097,
    NPC_WAR_RIDER                   = 13178,
    NPC_AERIE_GRYPHON               = 13161,
    NPC_WING_COMMANDER_GUSE         = 13179,
    NPC_WING_COMMANDER_JEZTOR       = 13180,
    NPC_WING_COMMANDER_MULVERICK    = 13181,
    NPC_WING_COMMANDER_ICHMAN       = 13437,
    NPC_WING_COMMANDER_SLIDORE      = 13438,
    NPC_WING_COMMANDER_VIPORE       = 13439,
    NPC_GUSES_WAR_RIDER             = 14943,
    NPC_JEZTORS_WAR_RIDER           = 14944,
    NPC_MULVERICKS_WAR_RIDER        = 14945,
    NPC_SLIDORES_GRYPHON            = 14946,
    NPC_ICHMANS_GRYPHON             = 14947,
    NPC_VIPORES_GRYPHON             = 14948,
    NPC_WOLF_RIDER                  = 13440,
    NPC_WOLF_RIDER_COMMANDER        = 13441,
    NPC_RAM_RIDER                   = 13576,
    NPC_RAM_RIDER_COMMANDER         = 13577,
    NPC_FIELD_MARSHAL_TERAVAINE     = 13446,
    NPC_WARMASTER_GARRICK           = 13449,
    NPC_STORMPIKE_COMMANDO          = 13524,    // + troop tier: Seasoned, Veteran, Champion Commando
    NPC_FROSTWOLF_REAVER            = 13528,    // + troop tier: Seasoned, Veteran, Champion Reaver
    NPC_PRIMALIST_THURLOGA          = 13236,
    NPC_FROSTWOLF_SHAMAN            = 13284,
    NPC_ARCHDRUID_RENFERAL          = 13442,
    NPC_DRUID_OF_THE_GROVE          = 13443,
    NPC_LOKHOLAR                    = 13256,
    NPC_IVUS                        = 13419,
    NPC_EARTHBIND_TOTEM             = 2630
};

enum GameObjects : uint32
{
    GO_ALTAR_OF_SUMMONING           = 178465,   // Horde ritual: Lokholar
    GO_CIRCLE_OF_CALLING            = 178670,   // Alliance ritual: Ivus
    GO_GUSE_BEACON                  = 178545,
    GO_JEZTOR_BEACON                = 178547,
    GO_MULVERICK_BEACON             = 178549,
    GO_VIPORE_BEACON                = 178724,
    GO_SLIDORE_BEACON               = 178725,
    GO_ICHMAN_BEACON                = 178726
};

enum Items : uint32
{
    ITEM_MULVERICKS_BEACON          = 17323,
    ITEM_GUSES_BEACON               = 17324,
    ITEM_JEZTORS_BEACON             = 17325,
    ITEM_ICHMANS_BEACON             = 17505,
    ITEM_VIPORES_BEACON             = 17506,
    ITEM_SLIDORES_BEACON            = 17507,
    ITEM_STORMPIKE_ASSAULT_ORDERS   = 17353,
    ITEM_FROSTWOLF_ASSAULT_ORDERS   = 17442
};

enum Quests : uint32
{
    QUEST_BEGIN_THE_ATTACK          = 6846,     // Alliance: Assault Orders to Field Marshal Teravaine
    QUEST_LAUNCH_THE_ATTACK         = 6901      // Horde: Assault Orders to Warmaster Garrick
};

enum Spells : uint32
{
    SPELL_RITUAL_CHANNEL            = 11206,    // Uldaman Boss Object Visual: the summoners channel at the ritual
    SPELL_WAR_RIDER_TRANSFORM       = 24085,
    SPELL_VIPORE_FORM               = 5759,

    // Primalist Thurloga
    SPELL_CHAIN_LIGHTNING           = 16006,
    SPELL_EARTHBIND_TOTEM           = 15786,
    SPELL_FLAME_SHOCK               = 15616,
    SPELL_LIGHTNING_BOLT            = 15234,
    SPELL_HEALING_WAVE              = 12492,
    // Arch Druid Renferal, Druid of the Grove
    SPELL_ENTANGLING_ROOTS          = 22127,
    SPELL_STARFIRE                  = 21668,
    SPELL_THORNS                    = 22128,
    SPELL_REJUVENATION              = 15981,
    // Frostwolf Shaman
    SPELL_FROST_SHOCK               = 21401,
    SPELL_LIGHTNING_SHIELD          = 12550,

    // wing commanders (their VMaNGOS spell lists)
    SPELL_STRIKE                    = 15580,
    SPELL_SUNDER_ARMOR              = 15572,
    SPELL_REND                      = 16509,

    // war riders and gryphons
    SPELL_FIREBALL                  = 22088,
    SPELL_FIREBALL_VOLLEY           = 15285,
    SPELL_STUN_BOMB_ATTACK          = 21188,

    // Lokholar the Ice Lord
    SPELL_BLIZZARD                  = 21367,
    SPELL_ICE_FROSTBOLT             = 21369,
    SPELL_FROST_NOVA                = 14907,
    SPELL_ICE_FROST_SHOCK           = 19133,
    SPELL_ICE_BLAST                 = 15878,
    SPELL_ICE_TOMB                  = 16869,
    SPELL_SWELL_OF_SOULS            = 21307,
    // Ivus the Forest Lord
    SPELL_IVUS_ROOTS                = 20654,
    SPELL_FAERIE_FIRE               = 21670,
    SPELL_MOONFIRE                  = 21669,
    SPELL_WRATH                     = 21667
};

// VMaNGOS DoScriptText ids, sent with the chat type VMaNGOS gives them
enum BroadcastTexts : uint32
{
    SAY_LOKHOLAR_SPAWNED            = 8626,     // yell, Thurloga
    SAY_PRIMALIST_THURLOGA          = 8632,
    SAY_ARCHDRUID_RENFERAL          = 8735,
    SAY_WOLFRIDER_COMMANDER         = 8890,
    SAY_WARCRY_HORDE                = 8891,
    SAY_RAMRIDER_COMMANDER          = 8906,
    SAY_WARCRY_ALLIANCE             = 8908,
    YELL_WOLFRIDER                  = 8887,
    YELL_RAMRIDER                   = 8905,
    YELL_PATROL                     = 8907,
    YELL_REAVERS                    = 8913,
    YELL_GUSE                       = 10339,
    YELL_JEZTOR                     = 10342,
    YELL_MULVERICK                  = 10347,
    YELL_VIPORE                     = 10357,
    YELL_ICHMAN                     = 10358,
    YELL_LOKHOLAR_SPAWN_1           = 8616,
    YELL_LOKHOLAR_SPAWN_2           = 8617,
    YELL_LOKHOLAR_KILLED_PLAYER     = 8618,
    YELL_LOKHOLAR_REACHED_BASE      = 8740,
    YELL_IVUS_SPAWNED               = 8736,
    YELL_IVUS_PAST_FIELD            = 8737,
    YELL_IVUS_REACHED_BASE          = 8739
};

// npc_text
enum GossipTexts : uint32
{
    TEXT_QUARTERMASTER              = 6255,
    TEXT_GROUND_ALMOST              = 6731,
    TEXT_GROUND_HALF                = 6732,
    TEXT_GROUND_FEW                 = 6733,
    TEXT_SMITH_BASIC                = 6073,
    TEXT_SMITH_SEASONED             = 6217,
    TEXT_SMITH_VETERAN              = 6218,
    TEXT_THURLOGA_1                 = 6098,     // + 0..2
    TEXT_RENFERAL_1                 = 6175      // + 0..2
};

// the options of a script menu are gossip_menu_option rows of menu 91000000 + entry
constexpr uint32 MenuOf(uint32 entry) { return 91000000 + entry; }

enum GossipOptions : uint32
{
    OPTION_QUARTERMASTER_VENDOR     = 1,
    OPTION_QUARTERMASTER_STATUS     = 2,
    OPTION_QUARTERMASTER_ASSAULT    = 3,
    OPTION_SMITH_STATUS             = 1,
    OPTION_SMITH_UPGRADE            = 2,        // + troop tier: seasoned, veteran, champion
    OPTION_WING_COMMANDER_BEACON    = 1,
    OPTION_WING_COMMANDER_AIR       = 2,
    OPTION_CAVALRY_ASSAULT          = 1,
    OPTION_COLLECTOR_STATUS         = 1         // + 0..2
};

enum GossipActions : uint32
{
    ACTION_VENDOR                   = GOSSIP_ACTION_TRADE,
    ACTION_STATUS                   = GOSSIP_ACTION_INFO_DEF + 1,   // + text step
    ACTION_UPGRADE                  = GOSSIP_ACTION_INFO_DEF + 20,
    ACTION_LAUNCH                   = GOSSIP_ACTION_INFO_DEF + 21,
    ACTION_LAUNCH_AIR               = GOSSIP_ACTION_INFO_DEF + 22
};

enum Displays : uint32
{
    DISPLAY_WAR_RIDER               = 11012,
    DISPLAY_AERIE_GRYPHON           = 1148,
    MOUNT_RAM                       = 2786,
    MOUNT_WOLF                      = 1166,
    MOUNT_THURLOGA                  = 12242,
    MOUNT_RENFERAL                  = 9695
};

enum AssaultData
{
    DATA_LEADER                     = 0,        // SetGUID on a troop: its leader
    DATA_TROOP                      = 1         // SetGUID on a leader: one of its troops
};

enum AssaultEvents
{
    EVENT_RALLY                     = 1,
    EVENT_TRANSFORM,
    EVENT_TAKE_OFF,
    EVENT_CHECK_RITUAL,
    EVENT_RETURN_HOME,
    EVENT_START_PATH,
    EVENT_YELL,
    EVENT_FIND_TARGET,
    EVENT_DESPAWN,

    EVENT_SPELL_1                   = 20,
    EVENT_SPELL_2,
    EVENT_SPELL_3,
    EVENT_SPELL_4,
    EVENT_SPELL_5,
    EVENT_SPELL_6
};

constexpr uint32 FactionStormpike = 730;
constexpr uint32 FactionFrostwolf = 729;

static constexpr Position HordeBase = { -1338.6f, -328.16f, 90.8f };
static constexpr Position AllianceBase = { 575.116f, -51.90f, 37.62f };

TeamId PlayerTeam(Player const* player)
{
    return player->GetBGTeam() == ALLIANCE ? TEAM_ALLIANCE : TEAM_HORDE;
}

uint32 Challenge(WorldObject const* object, uint32 team, uint32 challenge)
{
    ZoneScript* zoneScript = object->GetZoneScript();
    return zoneScript ? zoneScript->GetData(DATA_CAV_CHALLENGE + team * CAV_CHALLENGE_MAX + challenge) : 0;
}

bool ChallengeReady(WorldObject const* object, uint32 team, uint32 challenge)
{
    ZoneScript* zoneScript = object->GetZoneScript();
    if (!zoneScript)
        return false;
    uint32 const goal = zoneScript->GetData(DATA_CAV_CHALLENGE_GOAL + team * CAV_CHALLENGE_MAX + challenge);
    return goal && zoneScript->GetData(DATA_CAV_CHALLENGE + team * CAV_CHALLENGE_MAX + challenge) >= goal;
}

void SpendChallenge(WorldObject const* object, uint32 team, uint32 challenge)
{
    if (ZoneScript* zoneScript = object->GetZoneScript())
        zoneScript->SetData(DATA_CAV_CHALLENGE + team * CAV_CHALLENGE_MAX + challenge, 0);
}

// VMaNGOS ends its gossip checks with the reputation an assault needs; it forces that to neutral with the player's side
bool HasAssaultReputation(Player const* player, ReputationRank rank)
{
    return player->GetReputationRank(PlayerTeam(player) == TEAM_ALLIANCE ? FactionStormpike : FactionFrostwolf) >= rank;
}

// The VMaNGOS escort of an entry, up to the point where VMaNGOS stops it. A troop that carries on alone starts at the
// point nearest to it.
WaypointPath AssaultPath(uint32 entry, uint32 lastNode, WaypointMoveType moveType, WorldObject const* resumeFrom = nullptr)
{
    WaypointPath path;
    WaypointPath const* dbPath = sWaypointMgr->GetPath(AlteracValleyAssaultPathId(entry));
    if (!dbPath)
        return path;

    path.Id = dbPath->Id;
    path.MoveType = moveType;

    std::size_t first = 0;
    if (resumeFrom)
    {
        float nearest = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < dbPath->Nodes.size(); ++i)
        {
            float const dist = resumeFrom->GetExactDistSq(dbPath->Nodes[i].X, dbPath->Nodes[i].Y, dbPath->Nodes[i].Z);
            if (dist < nearest)
            {
                nearest = dist;
                first = i;
            }
        }
    }

    for (std::size_t i = first; i < dbPath->Nodes.size() && dbPath->Nodes[i].Id <= lastNode; ++i)
        path.Nodes.push_back(dbPath->Nodes[i]);

    return path;
}

void StartPath(Creature* creature, WaypointPath& path)
{
    if (path.Nodes.empty())
        return;

    path.BuildSegments();
    creature->GetMotionMaster()->MovePath(path, false);
}

// the VMaNGOS escort rescue / assault start: the NPC can be fought and fights for its side
void JoinTheFight(Creature* creature)
{
    creature->SetImmuneToPC(false);
    creature->SetImmuneToNPC(false);
    creature->SetPvP(true);
}

void Shout(Creature* creature, uint32 broadcastTextId, Unit* target = nullptr)
{
    creature->Yell(broadcastTextId, target);
    creature->HandleEmoteCommand(EMOTE_ONESHOT_SHOUT);
}
}

using namespace AlteracValleyAssaults;

// A leader of assault troops (VMaNGOS JoinCreatureGroup with OPTION_FORMATION_MOVE | OPTION_AGGRO_TOGETHER): at its rally point
// it calls its troops, who then follow it; they fight together, and when it dies the troops march on along their own path.
struct npc_av_assault_leader : public ScriptedAI
{
    npc_av_assault_leader(Creature* creature) : ScriptedAI(creature) { }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == DATA_TROOP)
            _troops.push_back(guid);
    }

    void JustEngagedWith(Unit* who) override
    {
        ForEachTroop([who](Creature* troop)
        {
            if (!troop->IsEngaged())
                troop->AI()->AttackStart(who);
        });
    }

    void JustDied(Unit* /*killer*/) override
    {
        ForEachTroop([](Creature* troop) { troop->AI()->DoAction(ACTION_CAV_LEADER_DIED); });
        _troops.clear();
    }

    void Rally(uint32 warcry)
    {
        ForEachTroop([this, warcry](Creature* troop)
        {
            troop->Say(warcry);
            troop->SetWalk(false);
            troop->GetMotionMaster()->MoveFollow(me, me->GetDistance(troop), ChaseAngle(me->GetRelativeAngle(troop)), {}, false,
                MOTION_SLOT_DEFAULT);
        });
    }

    template <typename Worker>
    void ForEachTroop(Worker&& worker)
    {
        for (ObjectGuid const& guid : _troops)
            if (Creature* troop = ObjectAccessor::GetCreature(*me, guid))
                if (troop->IsAlive() && troop->AI())
                    worker(troop);
    }

protected:
    GuidVector _troops;
};

// Wolf riders, ram riders, commandos and reavers (VMaNGOS npc_cavalry): mounted riders get off to fight. They follow their
// leader; once it is dead they march on alone, and leaderless riders fall back after ten minutes.
struct npc_av_assault_troop : public ScriptedAI
{
    npc_av_assault_troop(Creature* creature) : ScriptedAI(creature) { }

    bool IsRider() const { return me->GetEntry() == NPC_WOLF_RIDER || me->GetEntry() == NPC_RAM_RIDER; }

    uint32 PathEntry() const
    {
        if (IsRider())
            return me->GetEntry();
        return me->GetEntry() < NPC_FROSTWOLF_REAVER ? uint32(NPC_STORMPIKE_COMMANDO) : uint32(NPC_FROSTWOLF_REAVER);
    }

    void Reset() override
    {
        if (me->GetEntry() == NPC_RAM_RIDER)
            me->Mount(MOUNT_RAM);
        else if (me->GetEntry() == NPC_WOLF_RIDER)
            me->Mount(MOUNT_WOLF);
    }

    void JustAppeared() override
    {
        Reset();
    }

    void SetGUID(ObjectGuid const& guid, int32 id) override
    {
        if (id == DATA_LEADER)
            _leader = guid;
    }

    void JustEngagedWith(Unit* who) override
    {
        if (IsRider())
            me->Dismount();

        // the fight is where it went back to before following on (VMaNGOS combat start position)
        me->SetHomePosition(me->GetPosition());

        if (Creature* leader = ObjectAccessor::GetCreature(*me, _leader))
            if (leader->IsAlive() && !leader->IsEngaged() && leader->AI())
                leader->AI()->AttackStart(who);
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_CAV_LEADER_DIED)
            return;

        _leader.Clear();
        WaypointPath path = AssaultPath(PathEntry(), std::numeric_limits<uint32>::max(), WaypointMoveType::Run, me);
        StartPath(me, path);
        if (IsRider())
            _events.ScheduleEvent(EVENT_DESPAWN, 10min);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        if (_events.ExecuteEvent() == EVENT_DESPAWN)
        {
            me->DespawnOrUnsummon();
            return;
        }

        UpdateVictim();
    }

private:
    ObjectGuid _leader;
    EventMap _events;
};

// Frostwolf Wolf Rider Commander and Stormpike Ram Rider Commander: the cavalry assault (Ram Hide Harnesses / Frostwolf hides
// and the tamed mounts of Empty Stables)
struct npc_av_cavalry_commander : public npc_av_assault_leader
{
    npc_av_cavalry_commander(Creature* creature) : npc_av_assault_leader(creature) { }

    bool IsAlliance() const { return me->GetEntry() == NPC_RAM_RIDER_COMMANDER; }
    TeamId Team() const { return IsAlliance() ? TEAM_ALLIANCE : TEAM_HORDE; }
    uint32 RallyNode() const { return IsAlliance() ? 5 : 2; }

    void Reset() override
    {
        _events.Reset();
        me->Mount(IsAlliance() ? MOUNT_RAM : MOUNT_WOLF);
    }

    void JustAppeared() override
    {
        Reset();
    }

    void JustEngagedWith(Unit* who) override
    {
        me->Dismount();
        npc_av_assault_leader::JustEngagedWith(who);
    }

    bool OnGossipHello(Player* player) override
    {
        InitGossipMenuFor(player, MenuOf(me->GetEntry()));
        player->PrepareQuestMenu(me->GetGUID());

        TeamId const team = PlayerTeam(player);
        if (team == Team() && ChallengeReady(me, team, CAV_CHALLENGE_HIDE_CAVALRY) && ChallengeReady(me, team, CAV_CHALLENGE_TAMED_CAVALRY))
        {
            if (HasAssaultReputation(player, REP_NEUTRAL))
                AddGossipItemFor(player, MenuOf(me->GetEntry()), OPTION_CAVALRY_ASSAULT, GOSSIP_SENDER_MAIN, ACTION_LAUNCH);
            me->HandleEmoteCommand(EMOTE_ONESHOT_BOW);
        }

        SendGossipMenuFor(player, player->GetGossipTextId(me), me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = GetGossipActionFor(player, gossipListId);
        CloseGossipMenuFor(player);
        if (action != ACTION_LAUNCH || _launched || PlayerTeam(player) != Team()
            || !ChallengeReady(me, Team(), CAV_CHALLENGE_HIDE_CAVALRY) || !ChallengeReady(me, Team(), CAV_CHALLENGE_TAMED_CAVALRY))
            return true;

        _launched = true;
        SpendChallenge(me, Team(), CAV_CHALLENGE_HIDE_CAVALRY);
        Shout(me, IsAlliance() ? YELL_RAMRIDER : YELL_WOLFRIDER);
        JoinTheFight(me);

        // eight riders in two columns (VMaNGOS checkCavalryStatus)
        for (uint8 i = 0; i < 8; ++i)
        {
            Position pos;
            if (IsAlliance())
                pos = i < 4 ? Position(610.0f + 2.0f * i, -35.0f - 3.0f * i, 45.0f, 0.6f)
                            : Position(607.0f + 2.0f * (i % 5), -37.0f - 3.0f * (i % 5), 45.0f, 0.6f);
            else
                pos = i < 4 ? Position(-1230.0f - 4.0f * i, -611.0f - 3.0f * i, 54.0f, 5.4f)
                            : Position(-1223.0f - 4.0f * (i % 5), -619.0f - 3.0f * (i % 5), 54.0f, 2.22f);

            if (Creature* rider = me->SummonCreature(IsAlliance() ? NPC_RAM_RIDER : NPC_WOLF_RIDER, pos, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10s))
            {
                rider->AI()->SetGUID(me->GetGUID(), DATA_LEADER);
                _troops.push_back(rider->GetGUID());
            }
        }

        WaypointPath path = AssaultPath(me->GetEntry(), IsAlliance() ? 92 : 81, WaypointMoveType::Run);
        for (WaypointNode& node : path.Nodes)
            if (node.Id == RallyNode())
                node.Delay = 6s;
        StartPath(me, path);
        return true;
    }

    void WaypointReached(uint32 nodeId, uint32 /*pathId*/) override
    {
        if (nodeId != RallyNode())
            return;

        me->Say(IsAlliance() ? SAY_RAMRIDER_COMMANDER : SAY_WOLFRIDER_COMMANDER);
        _events.ScheduleEvent(EVENT_RALLY, 6s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        if (_events.ExecuteEvent() == EVENT_RALLY)
            Rally(IsAlliance() ? SAY_WARCRY_ALLIANCE : SAY_WARCRY_HORDE);

        UpdateVictim();
    }

private:
    EventMap _events;
    bool _launched = false;
};

// Field Marshal Teravaine and Warmaster Garrick (VMaNGOS npc_AV_troops_chief): the quartermaster calls them with ten troops;
// the Assault Orders handed to them start the march
struct npc_av_assault_chief : public npc_av_assault_leader
{
    npc_av_assault_chief(Creature* creature) : npc_av_assault_leader(creature) { }

    bool IsAlliance() const { return me->GetEntry() == NPC_FIELD_MARSHAL_TERAVAINE; }

    void Reset() override
    {
        _events.Reset();
    }

    void OnQuestReward(Player* player, Quest const* quest, LootItemType /*type*/, uint32 /*opt*/) override
    {
        if (quest->GetQuestId() != QUEST_BEGIN_THE_ATTACK && quest->GetQuestId() != QUEST_LAUNCH_THE_ATTACK)
            return;

        me->HandleEmoteCommand(EMOTE_ONESHOT_SHOUT);
        if (_started)
            return;

        _started = true;
        SpendChallenge(me, PlayerTeam(player), CAV_CHALLENGE_IRONDEEP_GROUND);
        JoinTheFight(me);

        // they walk to the rally point and run from there on
        WaypointPath path = AssaultPath(me->GetEntry(), IsAlliance() ? 48 : 53, WaypointMoveType::Walk);
        for (WaypointNode& node : path.Nodes)
        {
            if (node.Id == 2)
                node.Delay = 6s;
            if (node.Id >= 2)
                node.MoveType = WaypointMoveType::Run;
        }
        StartPath(me, path);
    }

    void WaypointReached(uint32 nodeId, uint32 /*pathId*/) override
    {
        if (nodeId != 2)
            return;

        me->Say(IsAlliance() ? SAY_RAMRIDER_COMMANDER : SAY_WOLFRIDER_COMMANDER);
        _events.ScheduleEvent(EVENT_RALLY, 6s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        if (_events.ExecuteEvent() == EVENT_RALLY)
            Rally(IsAlliance() ? SAY_WARCRY_ALLIANCE : SAY_WARCRY_HORDE);

        UpdateVictim();
    }

private:
    EventMap _events;
    bool _started = false;
};

// Stormpike and Frostwolf Quartermasters: the ground assault (Irondeep and Coldtooth Supplies)
struct npc_av_quartermaster : public ScriptedAI
{
    npc_av_quartermaster(Creature* creature) : ScriptedAI(creature) { }

    bool IsAlliance() const { return me->GetEntry() == NPC_QUARTERMASTER_A; }
    TeamId Team() const { return IsAlliance() ? TEAM_ALLIANCE : TEAM_HORDE; }

    bool GroundAssaultReady() const
    {
        return ChallengeReady(me, Team(), CAV_CHALLENGE_IRONDEEP_GROUND) || ChallengeReady(me, Team(), CAV_CHALLENGE_COLDTOOTH_GROUND);
    }

    bool OnGossipHello(Player* player) override
    {
        uint32 const menuId = MenuOf(me->GetEntry());
        InitGossipMenuFor(player, menuId);
        player->PrepareQuestMenu(me->GetGUID());

        bool const ownSide = PlayerTeam(player) == Team();
        if (ownSide)
            AddGossipItemFor(player, menuId, OPTION_QUARTERMASTER_STATUS, GOSSIP_SENDER_MAIN, ACTION_STATUS);
        if (ownSide && GroundAssaultReady())
        {
            if (HasAssaultReputation(player, REP_NEUTRAL))
                AddGossipItemFor(player, menuId, OPTION_QUARTERMASTER_ASSAULT, GOSSIP_SENDER_MAIN, ACTION_LAUNCH);
            me->HandleEmoteCommand(EMOTE_ONESHOT_BOW);
        }
        if (me->IsVendor())
            AddGossipItemFor(player, menuId, OPTION_QUARTERMASTER_VENDOR, GOSSIP_SENDER_MAIN, ACTION_VENDOR);

        SendGossipMenuFor(player, TEXT_QUARTERMASTER, me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = GetGossipActionFor(player, gossipListId);
        switch (action)
        {
            case ACTION_VENDOR:
                player->GetSession()->SendListInventory(me->GetGUID());
                break;
            case ACTION_STATUS:
            {
                // VMaNGOS GossipHello_npc_AVBlood_collector: how close the supplies are to an assault
                uint32 const irondeep = Challenge(me, Team(), CAV_CHALLENGE_IRONDEEP_GROUND);
                uint32 const coldtooth = Challenge(me, Team(), CAV_CHALLENGE_COLDTOOTH_GROUND);
                uint32 text = TEXT_GROUND_FEW;
                if (irondeep + coldtooth > 230)
                    text = TEXT_GROUND_ALMOST;
                else if (irondeep + coldtooth > 140)
                    text = TEXT_GROUND_HALF;
                else if (irondeep > 210 && coldtooth > 50)
                    text = TEXT_GROUND_ALMOST;
                else if (irondeep > 110 && coldtooth > 20)
                    text = TEXT_GROUND_HALF;
                SendGossipMenuFor(player, text, me->GetGUID());
                break;
            }
            case ACTION_LAUNCH:
                CloseGossipMenuFor(player);
                if (PlayerTeam(player) == Team() && GroundAssaultReady())
                    LaunchGroundAssault(player);
                break;
            default:
                CloseGossipMenuFor(player);
                break;
        }
        return true;
    }

    // VMaNGOS checkTroopsStatus: the field commander and ten troops of the tier the smith reached
    void LaunchGroundAssault(Player* player)
    {
        SpendChallenge(me, Team(), CAV_CHALLENGE_IRONDEEP_GROUND);
        Shout(me, IsAlliance() ? YELL_PATROL : YELL_REAVERS);

        uint32 const orders = IsAlliance() ? ITEM_STORMPIKE_ASSAULT_ORDERS : ITEM_FROSTWOLF_ASSAULT_ORDERS;
        if (!player->HasItemCount(orders, 1, true))
            player->AddItem(orders, 1);

        uint32 tier = 0;
        if (ZoneScript* zoneScript = me->GetZoneScript())
            tier = std::min<uint32>(zoneScript->GetData(DATA_CAV_REINFORCEMENT + uint32(Team())), 3);

        Creature* chief = IsAlliance()
            ? me->SummonCreature(NPC_FIELD_MARSHAL_TERAVAINE, -243.75f, -431.32f, 20.0f, 2.59f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10s)
            : me->SummonCreature(NPC_WARMASTER_GARRICK, -470.38f, -51.84f, 41.0f, 5.93f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10s);
        if (!chief)
            return;

        for (uint8 i = 0; i < 10; ++i)
        {
            Position pos;
            if (IsAlliance())
                pos = i < 5 ? Position(-240.9f - i, -431.11f - i, 20.2f, 2.59f) : Position(-238.35f - i % 5, -432.42f - i % 5, 20.2f, 2.59f);
            else
                pos = i < 5 ? Position(-472.2f - 0.4f * i, -48.4f - 1.6f * i, 41.3f, 5.93f)
                            : Position(-474.8f - 0.4f * (i - 5), -48.7f - 1.6f * (i - 5), 41.3f, 5.93f);

            uint32 const entry = (IsAlliance() ? NPC_STORMPIKE_COMMANDO : NPC_FROSTWOLF_REAVER) + tier;
            if (Creature* troop = me->SummonCreature(entry, pos, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10s))
            {
                troop->AI()->SetGUID(chief->GetGUID(), DATA_LEADER);
                chief->AI()->SetGUID(troop->GetGUID(), DATA_TROOP);
            }
        }
    }
};

// Smith Regzar and Murgot Deepforge: armor scraps upgrade the troops once someone honored asks for it
struct npc_av_smith : public ScriptedAI
{
    npc_av_smith(Creature* creature) : ScriptedAI(creature) { }

    bool OnGossipHello(Player* player) override
    {
        uint32 const menuId = MenuOf(me->GetEntry());
        InitGossipMenuFor(player, menuId);
        player->PrepareQuestMenu(me->GetGUID());

        ZoneScript* zoneScript = me->GetZoneScript();
        TeamId const team = PlayerTeam(player);
        uint32 const tier = zoneScript ? zoneScript->GetData(DATA_CAV_REINFORCEMENT + uint32(team)) : 3;
        uint32 const scraps = zoneScript ? zoneScript->GetData(DATA_CAV_ARMOR_SCRAPS + uint32(team)) : 0;
        if (tier >= 3)
        {
            SendGossipMenuFor(player, player->GetGossipTextId(me), me->GetGUID());
            return true;
        }

        AddGossipItemFor(player, menuId, OPTION_SMITH_STATUS, GOSSIP_SENDER_MAIN, ACTION_STATUS);
        if (HasAssaultReputation(player, REP_HONORED) && scraps >= (tier + 1) * 500)
            AddGossipItemFor(player, menuId, OPTION_SMITH_UPGRADE + tier, GOSSIP_SENDER_MAIN, ACTION_UPGRADE);

        static constexpr std::array<uint32, 3> TierTexts = { TEXT_SMITH_BASIC, TEXT_SMITH_SEASONED, TEXT_SMITH_VETERAN };
        SendGossipMenuFor(player, TierTexts[tier], me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = GetGossipActionFor(player, gossipListId);
        ZoneScript* zoneScript = me->GetZoneScript();
        if (!zoneScript)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        if (action == ACTION_STATUS)
        {
            // VMaNGOS: barely any (6784), about half (6780, 6781, 6783), almost enough (6778, 6779, 6782) for the next tier
            uint32 const scraps = zoneScript->GetData(DATA_CAV_ARMOR_SCRAPS + uint32(PlayerTeam(player)));
            uint32 text = 6784;
            if (scraps < 500)
                text = scraps <= 200 ? 6784 : scraps < 400 ? 6780 : 6778;
            else if (scraps < 1000)
                text = scraps <= 700 ? 6784 : scraps < 900 ? 6781 : 6779;
            else if (scraps < 1500)
                text = scraps <= 1200 ? 6784 : scraps < 1400 ? 6783 : 6782;
            SendGossipMenuFor(player, text, me->GetGUID());
            return true;
        }

        CloseGossipMenuFor(player);
        if (action == ACTION_UPGRADE && HasAssaultReputation(player, REP_HONORED))
            zoneScript->DoAction(ACTION_CAV_UPGRADE_ARMOR, player, me);
        return true;
    }
};

// Wing Commanders Guse, Jeztor and Mulverick (Horde) and Slidore, Vipore and Ichman (Alliance). Held captive at first: a player
// of their side frees them and they run home, where they take the Call of Air turn-ins; enough of those buy a beacon, or the
// commander's own flight over the enemy base.
struct npc_av_wing_commander : public ScriptedAI
{
    npc_av_wing_commander(Creature* creature) : ScriptedAI(creature) { }

    bool IsAlliance() const
    {
        return me->GetEntry() == NPC_WING_COMMANDER_ICHMAN || me->GetEntry() == NPC_WING_COMMANDER_SLIDORE
            || me->GetEntry() == NPC_WING_COMMANDER_VIPORE;
    }

    TeamId Team() const { return IsAlliance() ? TEAM_ALLIANCE : TEAM_HORDE; }
    Position const& Base() const { return IsAlliance() ? AllianceBase : HordeBase; }

    uint8 AirChallenge() const
    {
        switch (me->GetEntry())
        {
            case NPC_WING_COMMANDER_GUSE:
            case NPC_WING_COMMANDER_SLIDORE:
                return CAV_CHALLENGE_SOLDIER_AIR;
            case NPC_WING_COMMANDER_JEZTOR:
            case NPC_WING_COMMANDER_VIPORE:
                return CAV_CHALLENGE_LIEUTENANT_AIR;
            default:
                return CAV_CHALLENGE_COMMANDER_AIR;
        }
    }

    // VMaNGOS gave the Alliance soldier assault Vipore's beacon and the lieutenant assault Slidore's; the gossip ("Give me the
    // beacon, Slidore!") and the Call of Air quests pair them the other way round
    uint32 Beacon() const
    {
        switch (me->GetEntry())
        {
            case NPC_WING_COMMANDER_GUSE: return ITEM_GUSES_BEACON;
            case NPC_WING_COMMANDER_JEZTOR: return ITEM_JEZTORS_BEACON;
            case NPC_WING_COMMANDER_MULVERICK: return ITEM_MULVERICKS_BEACON;
            case NPC_WING_COMMANDER_SLIDORE: return ITEM_SLIDORES_BEACON;
            case NPC_WING_COMMANDER_VIPORE: return ITEM_VIPORES_BEACON;
            default: return ITEM_ICHMANS_BEACON;
        }
    }

    uint32 LaunchYell() const
    {
        switch (me->GetEntry())
        {
            case NPC_WING_COMMANDER_GUSE: return YELL_GUSE;
            case NPC_WING_COMMANDER_JEZTOR: return YELL_JEZTOR;
            case NPC_WING_COMMANDER_MULVERICK: return YELL_MULVERICK;
            case NPC_WING_COMMANDER_VIPORE: return YELL_VIPORE;
            case NPC_WING_COMMANDER_ICHMAN: return YELL_ICHMAN;
            default: return 0;
        }
    }

    uint32 WarRider() const
    {
        switch (me->GetEntry())
        {
            case NPC_WING_COMMANDER_GUSE: return NPC_GUSES_WAR_RIDER;
            case NPC_WING_COMMANDER_JEZTOR: return NPC_JEZTORS_WAR_RIDER;
            case NPC_WING_COMMANDER_MULVERICK: return NPC_MULVERICKS_WAR_RIDER;
            case NPC_WING_COMMANDER_SLIDORE: return NPC_SLIDORES_GRYPHON;
            case NPC_WING_COMMANDER_VIPORE: return NPC_VIPORES_GRYPHON;
            default: return NPC_ICHMANS_GRYPHON;
        }
    }

    // the point where VMaNGOS stops the escort home
    uint32 HomeNode() const
    {
        switch (me->GetEntry())
        {
            case NPC_WING_COMMANDER_GUSE: return 74;
            case NPC_WING_COMMANDER_JEZTOR: return 84;
            case NPC_WING_COMMANDER_MULVERICK: return 97;
            case NPC_WING_COMMANDER_SLIDORE: return 66;
            case NPC_WING_COMMANDER_VIPORE: return 76;
            default: return 92;
        }
    }

    bool AtBase() const { return me->GetDistance(Base()) < 15.0f; }

    void Reset() override
    {
        _events.Reset();

        // Vipore and Ichman sit in their prison
        if (!_rescued && (me->GetEntry() == NPC_WING_COMMANDER_VIPORE || me->GetEntry() == NPC_WING_COMMANDER_ICHMAN)
            && me->GetDistance(me->GetHomePosition()) < 5.0f)
            me->SetStandState(UNIT_STAND_STATE_SIT);
    }

    void JustAppeared() override
    {
        Reset();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (IsAlliance())
        {
            _events.ScheduleEvent(EVENT_SPELL_1, 4s, 6s);
            _events.ScheduleEvent(EVENT_SPELL_2, 9s, 13s);
        }
        else
        {
            _events.ScheduleEvent(EVENT_SPELL_1, 4s, 9s);
            _events.ScheduleEvent(EVENT_SPELL_2, 1s, 10s);
        }
    }

    bool OnGossipHello(Player* player) override
    {
        if (_launched)
            return true;

        TeamId const team = PlayerTeam(player);
        if (!AtBase())
        {
            // a captive: one of its side sets it free
            if (team == Team() && !_rescued)
            {
                _rescued = true;
                CloseGossipMenuFor(player);
                if (me->GetEntry() == NPC_WING_COMMANDER_VIPORE)
                    DoCastSelf(SPELL_VIPORE_FORM);
                me->SetStandState(UNIT_STAND_STATE_STAND);
                me->SetFaction(player->GetFaction());
                JoinTheFight(me);
                me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
                WaypointPath path = AssaultPath(me->GetEntry(), HomeNode(), WaypointMoveType::Run);
                StartPath(me, path);
                return true;
            }
        }
        else
            me->SetNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);

        uint32 const menuId = MenuOf(me->GetEntry());
        InitGossipMenuFor(player, menuId);
        player->PrepareQuestMenu(me->GetGUID());
        if (team == Team() && AtBase() && ChallengeReady(me, team, AirChallenge()))
        {
            if (HasAssaultReputation(player, REP_NEUTRAL))
            {
                AddGossipItemFor(player, menuId, OPTION_WING_COMMANDER_BEACON, GOSSIP_SENDER_MAIN, ACTION_LAUNCH);
                AddGossipItemFor(player, menuId, OPTION_WING_COMMANDER_AIR, GOSSIP_SENDER_MAIN, ACTION_LAUNCH_AIR);
            }
            me->HandleEmoteCommand(EMOTE_ONESHOT_BOW);
        }

        SendGossipMenuFor(player, player->GetGossipTextId(me), me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = GetGossipActionFor(player, gossipListId);
        CloseGossipMenuFor(player);
        if (_launched || PlayerTeam(player) != Team() || !AtBase() || !ChallengeReady(me, Team(), AirChallenge()))
            return true;

        if (action == ACTION_LAUNCH)
        {
            SpendChallenge(me, Team(), AirChallenge());
            if (uint32 const yell = LaunchYell())
                Shout(me, yell);
            if (!player->HasItemCount(Beacon(), 1, true))
                player->AddItem(Beacon(), 1);
        }
        else if (action == ACTION_LAUNCH_AIR)
        {
            // VMaNGOS checkAerialStatus: five seconds later it turns into its war rider, rises and leaves
            SpendChallenge(me, Team(), AirChallenge());
            if (uint32 const yell = LaunchYell())
                Shout(me, yell);
            _launched = true;
            _events.ScheduleEvent(EVENT_TRANSFORM, 5s);
        }
        return true;
    }

    void WaypointPathEnded(uint32 /*nodeId*/, uint32 /*pathId*/) override
    {
        me->SetNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_TRANSFORM:
                    me->RemoveNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
                    me->SetDisplayId(IsAlliance() ? DISPLAY_AERIE_GRYPHON : DISPLAY_WAR_RIDER);
                    me->SetDisableGravity(true);
                    me->SetCanFly(true);
                    SetCombatMovement(false);
                    DoCastSelf(SPELL_WAR_RIDER_TRANSFORM);
                    me->GetMotionMaster()->MovePoint(0, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ() + 30.0f, false);
                    _events.ScheduleEvent(EVENT_TAKE_OFF, 5s);
                    break;
                case EVENT_TAKE_OFF:
                    me->SummonCreature(WarRider(), me->GetPosition(), TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10s);
                    me->DespawnOrUnsummon();
                    return;
                case EVENT_SPELL_1:
                    if (UpdateVictim())
                        DoCastVictim(SPELL_STRIKE);
                    if (IsAlliance())
                        _events.ScheduleEvent(EVENT_SPELL_1, 7s, 9s);
                    else
                        _events.ScheduleEvent(EVENT_SPELL_1, 10s, 17s);
                    break;
                case EVENT_SPELL_2:
                    if (UpdateVictim())
                        DoCastVictim(IsAlliance() ? SPELL_REND : SPELL_SUNDER_ARMOR);
                    if (IsAlliance())
                        _events.ScheduleEvent(EVENT_SPELL_2, 18s, 34s);
                    else
                        _events.ScheduleEvent(EVENT_SPELL_2, 6s, 15s);
                    break;
                default:
                    break;
            }
        }

        if (!UpdateVictim())
        {
            _events.CancelEvent(EVENT_SPELL_1);
            _events.CancelEvent(EVENT_SPELL_2);
        }
    }

private:
    EventMap _events;
    bool _rescued = false;
    bool _launched = false;
};

// War riders and gryphons (VMaNGOS av_warrider). The wing commanders' own fly to the enemy base and circle it; the ones a
// beacon calls circle the beacon. Both dive at anything within 50 yards and bomb it from 30.
struct npc_av_war_rider : public ScriptedAI
{
    npc_av_war_rider(Creature* creature) : ScriptedAI(creature) { }

    bool IsCommanderRider() const { return me->GetEntry() >= NPC_GUSES_WAR_RIDER && me->GetEntry() <= NPC_VIPORES_GRYPHON; }
    float PatrolRadius() const { return IsCommanderRider() ? 55.0f : 20.0f; }

    void JustAppeared() override
    {
        me->SetDisableGravity(true);
        me->SetCanFly(true);

        // VMaNGOS sends only the commanders' riders anywhere; the beacon ones stayed idle there, here they hold the beacon spot
        if (IsCommanderRider())
        {
            bool const horde = me->GetEntry() <= NPC_MULVERICKS_WAR_RIDER;
            me->SetHomePosition(horde ? Position(618.4f, -87.97f, 85.77f) : Position(-1311.53f, -355.28f, 130.93f));
            me->GetMotionMaster()->MoveTargetedHome();
        }
        else
        {
            me->SetHomePosition(me->GetPosition());
            me->GetMotionMaster()->MoveRandom(PatrolRadius());
        }

        _events.ScheduleEvent(EVENT_FIND_TARGET, 1s);
    }

    void JustReachedHome() override
    {
        me->GetMotionMaster()->MoveRandom(PatrolRadius());
    }

    void AttackStart(Unit* who) override
    {
        ScriptedAI::AttackStartCaster(who, 25.0f);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(EVENT_SPELL_1, 0s);
        _events.ScheduleEvent(EVENT_SPELL_2, 8s);
        _events.ScheduleEvent(EVENT_SPELL_3, 13s);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        _events.CancelEvent(EVENT_SPELL_1);
        _events.CancelEvent(EVENT_SPELL_2);
        _events.CancelEvent(EVENT_SPELL_3);
        ScriptedAI::EnterEvadeMode(why);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (!me->IsEngaged())
        {
            if (_events.ExecuteEvent() == EVENT_FIND_TARGET)
            {
                if (!me->IsInEvadeMode())
                    if (Unit* target = me->SelectNearestTarget(50.0f))
                        AttackStart(target);
                _events.ScheduleEvent(EVENT_FIND_TARGET, 1s);
            }
            return;
        }

        if (!UpdateVictim())
            return;

        if (me->HasUnitState(UNIT_STATE_CASTING) || !me->IsWithinDistInMap(me->GetVictim(), 30.0f))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SPELL_1:
                    DoCastVictim(SPELL_FIREBALL);
                    _events.ScheduleEvent(EVENT_SPELL_1, 5s);
                    break;
                case EVENT_SPELL_2:
                    DoCastVictim(SPELL_FIREBALL_VOLLEY);
                    _events.ScheduleEvent(EVENT_SPELL_2, 7500ms);
                    break;
                case EVENT_SPELL_3:
                    DoCastVictim(SPELL_STUN_BOMB_ATTACK);
                    _events.ScheduleEvent(EVENT_SPELL_3, 9s);
                    break;
                case EVENT_FIND_TARGET:
                    _events.ScheduleEvent(EVENT_FIND_TARGET, 1s);
                    break;
                default:
                    break;
            }
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

private:
    EventMap _events;
};

// A planted beacon (VMaNGOS go_av_beacon): a minute later a war rider or gryphon comes for it, unless an enemy disables it first
struct go_av_beacon : public GameObjectAI
{
    go_av_beacon(GameObject* go) : GameObjectAI(go) { }

    bool IsAlliance() const
    {
        return me->GetEntry() == GO_VIPORE_BEACON || me->GetEntry() == GO_SLIDORE_BEACON || me->GetEntry() == GO_ICHMAN_BEACON;
    }

    void InitializeAI() override
    {
        // VMaNGOS: the faction of the other side, whose players are the ones to use (disable) it
        me->SetFaction(IsAlliance() ? 83 : 84);
        _timer = 60s;
    }

    void UpdateAI(uint32 diff) override
    {
        if (_called)
            return;

        _timer -= Milliseconds(diff);
        if (_timer > 0s)
            return;

        _called = true;
        me->SummonCreature(IsAlliance() ? NPC_AERIE_GRYPHON : NPC_WAR_RIDER, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ() + 30.0f,
            me->GetOrientation(), TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10s);
        me->DespawnOrUnsummon();
    }

    bool OnGossipHello(Player* player) override
    {
        if (PlayerTeam(player) != (IsAlliance() ? TEAM_ALLIANCE : TEAM_HORDE))
        {
            _called = true;
            me->DespawnOrUnsummon();
        }
        return true;
    }

private:
    Milliseconds _timer = 60s;
    bool _called = false;
};

// Primalist Thurloga and Arch Druid Renferal, the blood collectors: with enough Stormpike Soldier's Blood or Storm Crystals they
// lead their summoners to the ritual site and call ten players to it
struct npc_av_world_boss_summoner : public ScriptedAI
{
    npc_av_world_boss_summoner(Creature* creature) : ScriptedAI(creature) { }

    bool IsAlliance() const { return me->GetEntry() == NPC_ARCHDRUID_RENFERAL; }
    TeamId Team() const { return IsAlliance() ? TEAM_ALLIANCE : TEAM_HORDE; }
    uint32 Summoners() const { return IsAlliance() ? NPC_DRUID_OF_THE_GROVE : NPC_FROSTWOLF_SHAMAN; }
    uint32 MountDisplay() const { return IsAlliance() ? MOUNT_RENFERAL : MOUNT_THURLOGA; }
    uint32 SummonersMount() const { return IsAlliance() ? MOUNT_RENFERAL : MOUNT_WOLF; }
    uint32 MountNode() const { return IsAlliance() ? 3 : 6; }
    uint32 DismountNode() const { return IsAlliance() ? 48 : 42; }
    uint32 RitualNode() const { return IsAlliance() ? 49 : 43; }

    bool Riding() const { return _pathStarted && !_atRitual && _mounted; }

    void Reset() override
    {
        _events.Reset();
        if (Riding())
            me->Mount(MountDisplay());
        if (!_ritualGUID.IsEmpty())
            _events.ScheduleEvent(EVENT_CHECK_RITUAL, 1s);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (Riding())
            me->Dismount();

        if (IsAlliance())
        {
            _events.ScheduleEvent(EVENT_SPELL_1, 3s);           // Entangling Roots
            _events.ScheduleEvent(EVENT_SPELL_2, 10s);          // Rejuvenation
            _events.ScheduleEvent(EVENT_SPELL_3, 0s);           // Starfire
        }
        else
        {
            DoCastSelf(SPELL_EARTHBIND_TOTEM);
            _events.ScheduleEvent(EVENT_SPELL_1, 10s);          // Chain Lightning
            _events.ScheduleEvent(EVENT_SPELL_2, 5s);           // Flame Shock
            _events.ScheduleEvent(EVENT_SPELL_3, 7500ms);       // Lightning Bolt
            _events.ScheduleEvent(EVENT_SPELL_4, 0s);           // Healing Wave
            _events.ScheduleEvent(EVENT_SPELL_5, 0s);           // Earthbind Totem
        }
        if (!_ritualGUID.IsEmpty())
            _events.ScheduleEvent(EVENT_CHECK_RITUAL, 1s);
    }

    bool OnGossipHello(Player* player) override
    {
        uint32 const menuId = MenuOf(me->GetEntry());
        InitGossipMenuFor(player, menuId);
        player->PrepareQuestMenu(me->GetGUID());

        if (PlayerTeam(player) == Team())
        {
            uint32 const collected = Challenge(me, Team(), CAV_CHALLENGE_BLOOD_WORLDBOSS);
            uint32 const step = collected >= 160 ? 2 : collected >= 100 ? 1 : 0;
            AddGossipItemFor(player, menuId, OPTION_COLLECTOR_STATUS + step, GOSSIP_SENDER_MAIN, ACTION_STATUS + step);
        }

        SendGossipMenuFor(player, player->GetGossipTextId(me), me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = GetGossipActionFor(player, gossipListId);
        if (action < ACTION_STATUS || action > ACTION_STATUS + 2)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        SendGossipMenuFor(player, (IsAlliance() ? TEXT_RENFERAL_1 : TEXT_THURLOGA_1) + action - ACTION_STATUS, me->GetGUID());
        return true;
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_CAV_START_WORLD_BOSS_RITUAL || _pathStarted)
            return;

        _pathStarted = true;
        JoinTheFight(me);

        // Thurloga walks to her wolf, rides, and walks the last steps; Renferal runs throughout
        WaypointPath path = AssaultPath(me->GetEntry(), RitualNode(), WaypointMoveType::Run);
        for (WaypointNode& node : path.Nodes)
            if (node.Id >= DismountNode() || (!IsAlliance() && node.Id < MountNode()))
                node.MoveType = WaypointMoveType::Walk;
        StartPath(me, path);
    }

    void SetSummonersMounted(bool mounted)
    {
        std::list<Creature*> summoners;
        me->GetCreatureListWithEntryInGrid(summoners, Summoners(), 40.0f);
        for (Creature* summoner : summoners)
        {
            if (mounted)
                summoner->Mount(SummonersMount());
            else
                summoner->Dismount();
        }
    }

    void WaypointReached(uint32 nodeId, uint32 /*pathId*/) override
    {
        if (nodeId == MountNode())
        {
            _mounted = true;
            me->Mount(MountDisplay());
            SetSummonersMounted(true);
        }
        else if (nodeId == DismountNode())
        {
            _mounted = false;
            me->Dismount();
            SetSummonersMounted(false);
        }
    }

    void WaypointPathEnded(uint32 nodeId, uint32 /*pathId*/) override
    {
        if (nodeId != RitualNode() || _atRitual)
            return;

        _atRitual = true;
        me->Say(IsAlliance() ? SAY_ARCHDRUID_RENFERAL : SAY_PRIMALIST_THURLOGA);
        DoCastSelf(SPELL_RITUAL_CHANNEL);

        std::list<Creature*> summoners;
        me->GetCreatureListWithEntryInGrid(summoners, Summoners(), 30.0f);
        for (Creature* summoner : summoners)
            summoner->CastSpell(me, SPELL_RITUAL_CHANNEL);

        Position const ritual = IsAlliance() ? Position(-199.993f, -343.217f, 6.77662f, 3.68265f) : Position(-360.139f, -133.403f, 26.4856f, 4.41568f);
        if (GameObject* go = me->SummonGameObject(IsAlliance() ? GO_CIRCLE_OF_CALLING : GO_ALTAR_OF_SUMMONING, ritual,
            QuaternionData::fromEulerAnglesZYX(ritual.GetOrientation(), 0.0f, 0.0f), 0s, GO_SUMMON_TIMED_DESPAWN))
            _ritualGUID = go->GetGUID();
        _events.ScheduleEvent(EVENT_CHECK_RITUAL, 1s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_CHECK_RITUAL:
                {
                    // the ritual object goes away once the ten have called the boss (VMaNGOS SAY_LOKHOLAR_SPAWNED)
                    GameObject* ritual = ObjectAccessor::GetGameObject(*me, _ritualGUID);
                    if (ritual && ritual->isSpawned())
                    {
                        _events.ScheduleEvent(EVENT_CHECK_RITUAL, 1s);
                        break;
                    }

                    _ritualGUID.Clear();
                    if (!IsAlliance())
                        me->Yell(SAY_LOKHOLAR_SPAWNED);
                    _events.ScheduleEvent(EVENT_RETURN_HOME, IsAlliance() ? 6s : 10min);
                    break;
                }
                case EVENT_RETURN_HOME:
                {
                    // the collector and its summoners go back to their camp
                    std::list<Creature*> summoners;
                    me->GetCreatureListWithEntryInGrid(summoners, Summoners(), 100.0f);
                    for (Creature* summoner : summoners)
                        summoner->DespawnOrUnsummon(0s, 5s);
                    me->DespawnOrUnsummon(0s, 5s);
                    return;
                }
                default:
                    break;
            }
        }

        if (!UpdateVictim())
            return;

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SPELL_1:
                    if (IsAlliance())
                    {
                        DoCastVictim(SPELL_ENTANGLING_ROOTS);
                        _events.ScheduleEvent(EVENT_SPELL_1, 9s, 13s);
                    }
                    else
                    {
                        DoCastVictim(SPELL_CHAIN_LIGHTNING);
                        _events.ScheduleEvent(EVENT_SPELL_1, 15s, 18s);
                    }
                    break;
                case EVENT_SPELL_2:
                    if (IsAlliance())
                    {
                        DoCastSelf(SPELL_REJUVENATION, true);
                        _events.ScheduleEvent(EVENT_SPELL_2, 17s);
                    }
                    else
                    {
                        DoCastVictim(SPELL_FLAME_SHOCK);
                        _events.ScheduleEvent(EVENT_SPELL_2, 8s, 9s);
                    }
                    break;
                case EVENT_SPELL_3:
                    if (IsAlliance())
                    {
                        DoCastVictim(SPELL_STARFIRE);
                        _events.ScheduleEvent(EVENT_SPELL_3, 7s);
                    }
                    else
                    {
                        DoCastVictim(SPELL_LIGHTNING_BOLT);
                        _events.ScheduleEvent(EVENT_SPELL_3, 11s, 12s);
                    }
                    break;
                case EVENT_SPELL_4:
                    if (me->HealthBelowPct(70))
                    {
                        DoCastSelf(SPELL_HEALING_WAVE);
                        _events.ScheduleEvent(EVENT_SPELL_4, 7s);
                    }
                    else
                        _events.ScheduleEvent(EVENT_SPELL_4, 1s);
                    break;
                case EVENT_SPELL_5:
                    if (!me->FindNearestCreature(NPC_EARTHBIND_TOTEM, 20.0f))
                    {
                        DoCastSelf(SPELL_EARTHBIND_TOTEM);
                        _events.ScheduleEvent(EVENT_SPELL_5, 14s);
                    }
                    else
                        _events.ScheduleEvent(EVENT_SPELL_5, 1s);
                    break;
                default:
                    break;
            }
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

private:
    EventMap _events;
    ObjectGuid _ritualGUID;
    bool _pathStarted = false;
    bool _mounted = false;
    bool _atRitual = false;
};

// Frostwolf Shamans and Druids of the Grove: they ride and channel with their leader, and pick it up again after a fight
struct npc_av_ritual_summoner : public ScriptedAI
{
    npc_av_ritual_summoner(Creature* creature) : ScriptedAI(creature) { }

    bool IsDruid() const { return me->GetEntry() == NPC_DRUID_OF_THE_GROVE; }

    void Reset() override
    {
        _events.Reset();
        DoCastSelf(IsDruid() ? SPELL_THORNS : SPELL_LIGHTNING_SHIELD);
        if (_wasMounted)
            me->Mount(IsDruid() ? MOUNT_RENFERAL : MOUNT_WOLF);
        if (_wasChanneling)
            DoCastSelf(SPELL_RITUAL_CHANNEL);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _wasMounted = me->IsMounted();
        _wasChanneling = me->HasAura(SPELL_RITUAL_CHANNEL);
        if (_wasChanneling)
            me->InterruptNonMeleeSpells(true);
        me->Dismount();

        if (IsDruid())
        {
            _events.ScheduleEvent(EVENT_SPELL_1, 3s);           // Entangling Roots
            _events.ScheduleEvent(EVENT_SPELL_2, 0s);           // Starfire
        }
        else
        {
            _events.ScheduleEvent(EVENT_SPELL_1, 5s);           // Frost Shock
            _events.ScheduleEvent(EVENT_SPELL_2, 0s);           // Healing Wave
            _events.ScheduleEvent(EVENT_SPELL_3, 9s, 11s);      // Lightning Shield
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SPELL_1:
                    DoCastVictim(IsDruid() ? SPELL_ENTANGLING_ROOTS : SPELL_FROST_SHOCK);
                    if (IsDruid())
                        _events.ScheduleEvent(EVENT_SPELL_1, 9s, 13s);
                    else
                        _events.ScheduleEvent(EVENT_SPELL_1, 5s, 6s);
                    break;
                case EVENT_SPELL_2:
                    if (IsDruid())
                    {
                        DoCastVictim(SPELL_STARFIRE);
                        _events.ScheduleEvent(EVENT_SPELL_2, 7s, 9500ms);
                    }
                    else if (me->HealthBelowPct(70))
                    {
                        DoCastSelf(SPELL_HEALING_WAVE);
                        _events.ScheduleEvent(EVENT_SPELL_2, 7s);
                    }
                    else
                        _events.ScheduleEvent(EVENT_SPELL_2, 1s);
                    break;
                case EVENT_SPELL_3:
                    if (!me->HasAura(SPELL_LIGHTNING_SHIELD))
                        DoCastSelf(SPELL_LIGHTNING_SHIELD);
                    _events.ScheduleEvent(EVENT_SPELL_3, 9s, 11s);
                    break;
                default:
                    break;
            }
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

private:
    EventMap _events;
    bool _wasMounted = false;
    bool _wasChanneling = false;
};

// Lokholar the Ice Lord and Ivus the Forest Lord (VMaNGOS npc_worldboss_h_av / npc_worldboss_a_av): called at the ritual,
// they end it and march on the enemy base
struct npc_av_world_boss : public ScriptedAI
{
    npc_av_world_boss(Creature* creature) : ScriptedAI(creature) { }

    bool IsIvus() const { return me->GetEntry() == NPC_IVUS; }

    void JustAppeared() override
    {
        // the ritual is over: the channel visuals stop and the ritual object goes
        for (MapReference const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                player->RemoveAurasDueToSpell(SPELL_RITUAL_CHANNEL);

        std::list<GameObject*> rituals;
        me->GetGameObjectListWithEntryInGrid(rituals, IsIvus() ? GO_CIRCLE_OF_CALLING : GO_ALTAR_OF_SUMMONING, 200.0f);
        for (GameObject* ritual : rituals)
            ritual->DespawnOrUnsummon();

        for (uint32 entry : { IsIvus() ? NPC_DRUID_OF_THE_GROVE : NPC_FROSTWOLF_SHAMAN, IsIvus() ? NPC_ARCHDRUID_RENFERAL : NPC_PRIMALIST_THURLOGA })
        {
            std::list<Creature*> summoners;
            me->GetCreatureListWithEntryInGrid(summoners, entry, 200.0f);
            for (Creature* summoner : summoners)
                summoner->InterruptNonMeleeSpells(true);
        }

        if (IsIvus())
            me->Yell(YELL_IVUS_SPAWNED);
        else
        {
            me->Yell(YELL_LOKHOLAR_SPAWN_1);
            _events.ScheduleEvent(EVENT_YELL, 3s);
        }
        _events.ScheduleEvent(EVENT_START_PATH, 1s);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (IsIvus())
        {
            _events.ScheduleEvent(EVENT_SPELL_1, 8500ms);       // roots
            _events.ScheduleEvent(EVENT_SPELL_2, 5500ms);       // Faerie Fire
            _events.ScheduleEvent(EVENT_SPELL_3, 7s);           // Moonfire
            _events.ScheduleEvent(EVENT_SPELL_4, 10s);          // Starfire
            _events.ScheduleEvent(EVENT_SPELL_5, 1s);           // Wrath
        }
        else
        {
            _events.ScheduleEvent(EVENT_SPELL_1, 10s);          // Blizzard
            _events.ScheduleEvent(EVENT_SPELL_2, 1s);           // Frostbolt
            _events.ScheduleEvent(EVENT_SPELL_3, 8500ms);       // Frost Nova
            _events.ScheduleEvent(EVENT_SPELL_4, 4s);           // Frost Shock
            _events.ScheduleEvent(EVENT_SPELL_5, 9s);           // Ice Blast
            _events.ScheduleEvent(EVENT_SPELL_6, 5500ms);       // Ice Tomb
        }
    }

    void KilledUnit(Unit* victim) override
    {
        if (IsIvus() || !victim->IsPlayer())
            return;

        me->Yell(YELL_LOKHOLAR_KILLED_PLAYER);
        DoCastSelf(SPELL_SWELL_OF_SOULS, true);
    }

    void WaypointReached(uint32 nodeId, uint32 /*pathId*/) override
    {
        if (IsIvus())
        {
            if (nodeId == 1)
                me->Yell(YELL_IVUS_PAST_FIELD);
            else if (nodeId == 21)
                me->Yell(YELL_IVUS_REACHED_BASE);
        }
        else if (nodeId == 30)
            me->Yell(YELL_LOKHOLAR_REACHED_BASE);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (!me->IsEngaged())
        {
            while (uint32 eventId = _events.ExecuteEvent())
            {
                if (eventId == EVENT_YELL)
                    me->Yell(YELL_LOKHOLAR_SPAWN_2);
                else if (eventId == EVENT_START_PATH)
                {
                    WaypointPath path = AssaultPath(me->GetEntry(), IsIvus() ? 21 : 31, WaypointMoveType::Walk);
                    StartPath(me, path);
                }
            }
            return;
        }

        if (!UpdateVictim())
            return;

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_YELL:
                    me->Yell(YELL_LOKHOLAR_SPAWN_2);
                    break;
                case EVENT_START_PATH:
                {
                    WaypointPath path = AssaultPath(me->GetEntry(), IsIvus() ? 21 : 31, WaypointMoveType::Walk);
                    StartPath(me, path);
                    break;
                }
                case EVENT_SPELL_1:
                    if (IsIvus())
                    {
                        DoCastSelf(SPELL_IVUS_ROOTS);
                        _events.ScheduleEvent(EVENT_SPELL_1, 6s, 8s);
                    }
                    else
                    {
                        DoCastVictim(SPELL_BLIZZARD);
                        _events.ScheduleEvent(EVENT_SPELL_1, 10s, 12s);
                    }
                    break;
                case EVENT_SPELL_2:
                    if (IsIvus())
                    {
                        DoCastVictim(SPELL_FAERIE_FIRE);
                        _events.ScheduleEvent(EVENT_SPELL_2, 3s, 7s);
                    }
                    else
                    {
                        DoCastVictim(SPELL_ICE_FROSTBOLT);
                        _events.ScheduleEvent(EVENT_SPELL_2, 4500ms, 6500ms);
                    }
                    break;
                case EVENT_SPELL_3:
                    if (IsIvus())
                    {
                        DoCastVictim(SPELL_MOONFIRE);
                        _events.ScheduleEvent(EVENT_SPELL_3, 8s, 12s);
                    }
                    else
                    {
                        DoCastVictim(SPELL_FROST_NOVA);
                        _events.ScheduleEvent(EVENT_SPELL_3, 8s, 12s);
                    }
                    break;
                case EVENT_SPELL_4:
                    if (IsIvus())
                    {
                        DoCastVictim(SPELL_STARFIRE);
                        _events.ScheduleEvent(EVENT_SPELL_4, 3s, 7s);
                    }
                    else
                    {
                        DoCastVictim(SPELL_ICE_FROST_SHOCK);
                        _events.ScheduleEvent(EVENT_SPELL_4, 3s, 5s);
                    }
                    break;
                case EVENT_SPELL_5:
                    if (IsIvus())
                    {
                        DoCastVictim(SPELL_WRATH);
                        _events.ScheduleEvent(EVENT_SPELL_5, 1s, 3s);
                    }
                    else
                    {
                        DoCastVictim(SPELL_ICE_BLAST);
                        _events.ScheduleEvent(EVENT_SPELL_5, 10s, 12s);
                    }
                    break;
                case EVENT_SPELL_6:
                    DoCastVictim(SPELL_ICE_TOMB);
                    _events.ScheduleEvent(EVENT_SPELL_6, 8s, 12s);
                    break;
                default:
                    break;
            }
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }
    }

private:
    EventMap _events;
};

class at_av_exploit : public AreaTriggerScript
{
public:
    at_av_exploit() : AreaTriggerScript("at_av_exploit") { }

    bool OnTrigger(Player* player, AreaTriggerEntry const* /*trigger*/) override
    {
        if (Battleground* battleground = player->GetBattleground())
            if (battleground->GetStatus() == STATUS_WAIT_JOIN)
                battleground->TeleportPlayerToExploitLocation(player);

        return true;
    }
};

template<AlteracValleySharedActions ActionId>
class quest_alterac_valley : public QuestScript
{
public:
    quest_alterac_valley(char const* scriptName) : QuestScript(scriptName) { }

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        if (newStatus != QUEST_STATUS_REWARDED)
            return;

        if (ZoneScript* zoneScript = player->FindZoneScript())
            zoneScript->DoAction(ActionId, player, player);
    }
};

void AddSC_alterac_valley()
{
    RegisterCreatureAI(npc_av_marshal_or_warmaster);
    RegisterCreatureAI(npc_av_bowman);
    RegisterCreatureAI(npc_av_mine_layer);
    RegisterCreatureAI(npc_av_korrak_the_bloodrager);
    RegisterGameObjectAI(go_av_capturable_object);
    RegisterGameObjectAI(go_av_contested_object);
    RegisterCreatureAI(npc_av_quartermaster);
    RegisterCreatureAI(npc_av_smith);
    RegisterCreatureAI(npc_av_wing_commander);
    RegisterCreatureAI(npc_av_cavalry_commander);
    RegisterCreatureAI(npc_av_assault_chief);
    RegisterCreatureAI(npc_av_assault_troop);
    RegisterCreatureAI(npc_av_war_rider);
    RegisterGameObjectAI(go_av_beacon);
    RegisterCreatureAI(npc_av_world_boss_summoner);
    RegisterCreatureAI(npc_av_ritual_summoner);
    RegisterCreatureAI(npc_av_world_boss);
    new at_av_exploit();
    new quest_alterac_valley<ACTION_TURN_IN_SCRAPS>("quest_alterac_valley_armor_scraps");
    new quest_alterac_valley<ACTION_TURN_IN_COMMANDER_1>("quest_alterac_valley_call_of_air_slidore_guse");
    new quest_alterac_valley<ACTION_TURN_IN_COMMANDER_2>("quest_alterac_valley_call_of_air_vipore_jeztor");
    new quest_alterac_valley<ACTION_TURN_IN_COMMANDER_3>("quest_alterac_valley_call_of_air_ichman_mulverick");
    new quest_alterac_valley<ACTION_TURN_IN_BOSS_1>("quest_alterac_valley_boss_5");
    new quest_alterac_valley<ACTION_TURN_IN_BOSS_2>("quest_alterac_valley_boss_1");
    new quest_alterac_valley<ACTION_TURN_IN_NEAR_MINE>("quest_alterac_valley_near_mine");
    new quest_alterac_valley<ACTION_TURN_IN_OTHER_MINE>("quest_alterac_valley_other_mine");
    new quest_alterac_valley<ACTION_TURN_IN_RIDER_HIDE>("quest_alterac_valley_ram_harnesses");
    new quest_alterac_valley<ACTION_TURN_IN_RIDER_TAME>("quest_alterac_valley_empty_stables");
}
