#include "ScenarioRunner.h"

#include "DamageParity.h"
#include "ScenarioDefs.h"
#include "PetReviveCheck.h"
#include "../Bot/BotAI.h"
#include "../Bot/BotRegistry.h"
#include "../Bot/Raid/RaidEncounters.h"
#include "../Bot/Raid/RaidFightScript.h"
#include "../Bot/ClassicUpkeep.h"
#include "../Bot/BotSnapshotView.h"
#include "../Bot/Gear/OwnedGearPolicy.h"
#include "../Group/GroupSnapshot.h"
#include "../Api/ClassicSpec.h"
#include "../Api/ClassicSupplies.h"
#include "../Bot/World/HunterPetsByRace.h"
#include "../Fleet/BotCharacterFactory.h"
#include "../Fleet/BotIdentityRegistry.h"
#include "../Fleet/BotPool.h"
#include "../Fleet/BotPopulationManager.h"
#include "../Fleet/BotSetupPipeline.h"
#include "../Services.h"
#include "../PlayerbotV2.h"            // Module::RouteGroupOrder: the player-led stand-in's "pull back" / "attack"
#include "../Bot/Dungeon/DungeonScript.h"   // DungeonScriptMgr::RouteFor: the route the stand-in walks
#include "ScenarioHuman.h"
#include "../Fleet/OwnerCatchUp.h"
#include "../Threading/SnapshotPublisher.h"
#include "../Threading/TickScheduler.h"
#include "../Session/BotSessionMgr.h"

#include "CharacterCache.h"
#include "Creature.h"
#include "CreatureGroups.h"
#include "CreatureAI.h"
#include "GameObject.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "Group.h"
#include "GroupMgr.h"
#include "Log.h"
#include "InstanceLockMgr.h"
#include "MMapManager.h"
#include "PhasingHandler.h"
#include "DetourNavMeshQuery.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "DB2Stores.h"
#include "Map.h"
#include "MapManager.h"
#include "MotionMaster.h"
#include "MoveSpline.h"
#include "ObjectMgr.h"
#include "InstanceScript.h"
#include "GameEventSender.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "Player.h"
#include "WaypointManager.h"
#include "PlayerbotAPI.h"
#include "../Api/ClassicSpec.h"
#include "PlayerbotMovement.h"
#include "Util.h"
#include "fmt/format.h"

#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "TemporarySummon.h"
#include "SpellMgr.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <list>
#include <memory>
#include <atomic>
#include <map>
#include <set>
#include <vector>

namespace Playerbot::Diagnostics::Scenario {

namespace {

// Where bots stranded in an instance from an earlier run are sent first, and where a finished run sends its squad (Stormwind,
// the same spot `.playerbot dungeontest` stages at).
constexpr std::uint32_t kStageMap = 0;
constexpr float kStageX = -8985.0f, kStageY = 511.0f, kStageZ = 79.8f;
// A Horde character cannot stand in Stormwind: the city guards kill it (damage_parity_shaman 152, an Orc dead after 38 s). Its staging spot is Razor Hill,
// a Horde town with open, flat ground around it.
constexpr std::uint32_t kHordeStageMap = 1;
constexpr float kHordeStageX = 340.36f, kHordeStageY = -4686.29f, kHordeStageZ = 16.54f;

// The Stormwind harbour ledge killed two parity runs (a rogue fell off it, a hunter walked into the water). An Alliance parity subject stages at the
// Northshire Abbey start instead: the human racial start (playercreateinfo), open flat ground with no water or ledge; World.cpp checks its map and vmap on boot.
// Dungeon scenarios keep the Stormwind spot they path from.
constexpr float kAllianceParityStageX = -8949.95f, kAllianceParityStageY = -132.493f, kAllianceParityStageZ = 83.5312f;

inline void StagePoint(Player const* p, std::uint32_t& map, float& x, float& y, float& z, bool parity = false)
{
    if (p && p->GetTeam() == HORDE)
    {
        map = kHordeStageMap; x = kHordeStageX; y = kHordeStageY; z = kHordeStageZ;
        return;
    }
    if (parity)
    {
        map = 0; x = kAllianceParityStageX; y = kAllianceParityStageY; z = kAllianceParityStageZ;
        return;
    }
    map = kStageMap; x = kStageX; y = kStageY; z = kStageZ;
}

// Sends the character to its staging spot.
inline void SendToStage(Player* p, bool parity = false)
{
    std::uint32_t map; float x, y, z;
    StagePoint(p, map, x, y, z, parity);
    ::Playerbot::BotMovement::SafeTeleport(p, map, x, y, z, 0.f, 0);
}

constexpr std::uint32_t kStepMs = 1000;                 // one staging / sampling step a second; setup is never looped on the world thread
constexpr std::uint32_t kStageTimeoutMs = 8u * 60u * 1000u;
constexpr std::uint32_t kStageTimeoutPerBotMs = 15u * 1000u;   // a raid of 40 takes longer to make, level and gear than five
constexpr std::uint32_t kEnterTimeoutMs = 90u * 1000u;
constexpr std::uint32_t kArriveTimeoutMs = 45u * 1000u;
constexpr int kPullManaPct = 95;                           // every healer at or above this before the ordered Attack
constexpr std::uint32_t kManaWaitMs = 120u * 1000u;      // the longest the Attack waits for it
constexpr std::uint32_t kPrepWaitMs = 90u * 1000u;       // the longest it waits for the raid's buffs and consumables (PrepOwed)

namespace Owned = ::Playerbot::V2::Gear::Owned;
constexpr std::size_t kStageBatch = 4;                           // squads over five: members set up per step, so the world thread keeps ticking

enum class Phase : std::uint8_t { Idle, Stage, Enter, Arrive, Run, Parity };

struct Member
{
    ScenarioMember def;
    std::uint64_t id = 0;
    bool role_pinned = false;
    std::uint32_t parity_talent_tries = 0;   // damage_parity: attempts to commit the class's classic build
    bool parity_talents = false;
    std::uint32_t parity_pet_tries = 0;   // damage_parity hunter: attempts to give it the pet every hunter has
    bool parity_pet = false;   // damage_parity: the classic build is committed
    bool kit_learned = false;   // the class trainer spells of the squad level are taught (LearnLevelKit)
    bool tier_geared = false;   // the raid's tier gear (RaidGearTier) and the fight's resistance gear are on (see the staging step)
    bool ready = false;
    bool arrival_logged = false;   // LogPlace wrote where it landed on the scenario's map
};

// What one raider did from the boss's first combat second: the damage it (and its pet) dealt, and how its seconds were spent.
// The damage is added from the map's thread (Module::OnDamageTaken); the seconds are counted by the runner's tick.
struct PullStat
{
    std::atomic<std::uint64_t> damage{0};
    std::atomic<std::uint64_t> boss_damage{0};
    std::atomic<std::uint32_t> hits{0};
    std::uint32_t seconds = 0, casting = 0, moving = 0, held = 0, dead = 0;
    std::uint32_t rage_sum = 0, rage_max = 0, rage_ge25 = 0;   // a rage user's rage in display units, sampled once a second
    std::map<std::string, std::uint32_t> rules;   // last bot rule fired, one count per second
};

struct Run
{
    bool flat_ground_done = false;   // damage_parity: the flat spot the measurement runs from is chosen and remembered (Parity::NoteFlatSpot)
    ScenarioDef const* def = nullptr;
    Phase phase = Phase::Idle;
    std::vector<Member> members;
    std::unique_ptr<ScenarioMonitor> monitor;
    std::uint32_t step_acc_ms = 0;
    std::uint32_t phase_ms = 0;          // time in the current phase
    std::uint32_t run_ms = 0;            // time since the run began (what the failure timestamps count)
    std::uint8_t place_logs = 0;         // LogPlace rounds written after the run began (1 s and 3 s in)
    bool goal_reached = false;
    std::uint32_t adds_killed = 0;   // ScenarioDef::goal_adds_needed
    std::uint32_t spared_killed = 0;   // ScenarioDef::spare_entry: dead before the goal boss
    std::string stage_status;
    // Raids: the goal boss's first spawn, the spot the squad is moved to, and whether the main tank has been ordered on it.
    bool boss_spawn_known = false;
    float boss_x = 0.f, boss_y = 0.f, boss_z = 0.f, boss_o = 0.f;
    ObjectGuid::LowType boss_spawn_id = 0;   // the creature row behind boss_x (0: a summoned boss); its waypoint path keeps the landing clear
    bool arrive_sent = false;
    std::uint32_t landing_resend_ms = 0;  // phase_ms of the last landing teleport resend round
    std::uint32_t start_object_used_ms = 0;   // run_ms the main tank used ScenarioDef::start_object (0: not yet)
    std::uint32_t start_event_sent_ms = 0;    // run_ms the main tank sent ScenarioDef::start_event (0: not yet)
    std::uint32_t start_npc_wait_ms = 0;      // run_ms ScenarioDef::start_instance_data was set and the wait for start_npc began (0: not yet)
    std::uint32_t start_npc_done_ms = 0;      // run_ms the main tank talked to ScenarioDef::start_npc (0: not yet)
    bool start_data_set = false;              // ScenarioDef::start_instance_data was set for a fight with no start_npc
    std::uint32_t goal_wait_log_ms = 0;       // run_ms of the last 'waiting for the summoned boss' line
    std::uint32_t landing_log_ms = 0;     // game time of the last landing position round
    std::uint32_t landing_rounds = 0;     // rounds logged (capped)
    std::uint32_t mana_wait_log_ms = 0;   // run_ms of the last 'waiting for the healers' mana' line
    std::uint32_t prep_wait_log_ms = 0;   // run_ms of the last 'waiting for the raid's buffs' line
    bool kits_stocked = false;            // the role kits (flask, elixirs, food) went into the bags at 'in place' (StockRaidKits)
    bool trash_cleared = false;
    // The boss pull's statistics ([pull_stats]): started at the boss's first combat, reported 90 s later and when the run ends.
    // The map is filled once at the start and only its values change afterwards, so the damage hook can look a bot up unlocked.
    std::map<std::uint64_t, PullStat> pull;
    std::atomic<bool> pull_started{false};
    std::uint32_t pull_start_ms = 0;
    bool pull_reported = false;
    std::set<ObjectGuid> engaged_logged;  // hostile creatures whose first engagement was logged ([scenario] engaged), capped per run
    float arrive_x = 0.f, arrive_y = 0.f, arrive_z = 0.f, arrive_o = 0.f;
    bool order_sent = false;
    std::uint32_t attack_run_ms = 0;     // run_ms of the Attack order
    std::uint32_t newcomers_next_ms = 0;  // run_ms of the next look at the hostiles near the raid before the order (LogNewcomers)
    bool tank_probe_done = false;        // the tanks' state a few seconds into the fight was logged
    std::uint32_t owner_skull_next_ms = 0;   // run_ms of the next look at the owner's skull (PlayOwnerSkull)
    ObjectGuid owner_skull;                  // the enemy the runner last put the skull on
    bool stand_in_detached = false;      // the stand-in logic is off and the runner plays it
    // ScenarioDef::player_lead: the stand-in leader's walk and orders (PlayLeader).
    std::vector<::Playerbot::DungeonAdvice::ProgressionPoint> lead_route;
    std::size_t lead_wp = 0;              // the route point he walks to next
    ObjectGuid lead_pull;                 // the mob he said "pull back" on
    std::uint32_t lead_pull_ms = 0;       // run_ms of that order
    bool lead_attack_sent = false;        // "attack" said for this fight
    std::uint32_t lead_wait_since_ms = 0; // run_ms the ready wait began (0: not waiting)
    std::uint32_t lead_log_ms = 0;        // run_ms of the last [scenario_lead] line
    std::set<ObjectGuid> lead_skipped;    // mobs a pull order never started a fight with
    ObjectGuid lead_boss;                 // the run's boss he set out for: he goes on to him until he is dead (PlayLeader)
    // ScenarioGoalKind::FollowRoute (PlayRoute): the leg he is on and how it goes.
    std::size_t route_leg = 0;
    std::uint32_t leg_start_ms = 0;       // run_ms the leg began (0: not begun)
    std::uint32_t leg_at_ms = 0;          // run_ms he reached the leg's point (0: not yet)
    std::uint32_t leg_progress_ms = 0;    // run_ms he last got 2 y closer to the point
    float leg_best = 0.f;                 // his smallest distance to the point on this leg
    bool leg_leapt = false;               // he was set down on the point (no walking route)
    bool leg_water = false;               // a water point: the deep spot found for it when the leg began
    float leg_wx = 0.f, leg_wy = 0.f, leg_wz = 0.f;
    std::uint32_t leg_hold_since_ms = 0;  // run_ms the squad was with him at a point he holds (0: not holding)
    std::map<std::uint64_t, std::uint32_t> leg_joined_ms;     // member -> run_ms it first stood with him at the point
    std::map<std::uint64_t, std::uint32_t> leg_catch_ups;     // member -> its catch-up count when the leg began
    std::uint32_t spot_moved = 0;        // the dps scorecard raid was moved to flat ground
    ObjectGuid group_guid;               // the squad's group: a member found in another one is put back before the run starts
    bool group_logged = false;
    bool refusal_logged = false;   // the map's refusal of the squad's entry is logged once per run
    std::string refusal;           // its reason, for the failure line
    std::string arrive_blocker;          // why no landing spot was found (the hostile that blocked the nearest candidate)
    std::uint32_t stage_timeout_ms = kStageTimeoutMs;
    // Classic::RaidSupportTotal readings (tranq shots, curses removed, formation slot give-ups) when the run began.
    std::uint32_t base_tranq = 0, base_decurse = 0, base_giveup = 0;
    bool support_based = false;
    // Playerbot.log's size once a second of the run (run_ms, bytes), the whole run (at most kLogMarksMax): where the failure report
    // starts and stops reading the log.
    std::deque<std::pair<std::uint32_t, std::uintmax_t>> log_marks;
    // The creature whose first spawn the squad is moved next to before the run: a raid's boss (def->arrive_near_entry, held until the Attack)
    // or a dungeon checkpoint (`name@key`, bot-led from the first second). 0 = the squad plays from the entrance.
    std::uint32_t arrive_entry = 0;
    bool checkpoint = false;
    bool has_stage = false;            // a checkpoint with a listed staging spot (ScenarioCheckpoint::stage_*)
    float stage_x = 0.f, stage_y = 0.f, stage_z = 0.f;
    std::string from_key;
    // Which of the squads running side by side this is (0 = the first): a squad beyond the first is made of characters of its own,
    // with the slot's letter in their names (SquadName).
    int slot = 0;
};

// The scenarios running now: one, or up to g_parallel (`runbatch`) with a squad and an instance each.
std::vector<std::unique_ptr<Run>> g_runs;
std::size_t g_parallel = 1;        // runs started together by one wave of runall (1) / runbatch (5)
bool g_halt_on_fail = false;       // runbatch: a wave with a FAIL stops the batch (the next wave waits for the fix)
bool g_wave_failed = false;

// The name of a squad member in a slot: slot 0 keeps the catalogue's name (the characters that already exist), the others carry the
// slot's letter after the first two letters ("Scnwarrior" -> "Scbnwarrior"), so no name is a substring of another slot's.
std::string SquadName(std::string const& name, int slot)
{
    if (slot <= 0 || name.size() < 3)
        return name;
    return name.substr(0, 2) + char('a' + slot) + name.substr(2);
}

// `scenario runall`: the names still to run, and one line per finished scenario.
std::deque<std::string> g_queue;
std::vector<std::string> g_summary;
bool g_all = false;
std::string g_last_result = "No scenario has run since the server started.";

Player* Find(std::uint64_t id)
{
    return ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
}

::Playerbot::Role ToBotRole(ScenarioRoleKind r)
{
    switch (r)
    {
        case ScenarioRoleKind::Tank:   return ::Playerbot::Role::Tank;
        case ScenarioRoleKind::Healer: return ::Playerbot::Role::Healer;
        default:                       return ::Playerbot::Role::Dps;
    }
}

void LeaveGroup(Player* p)
{
    if (Group* g = p->GetGroup())
        g->RemoveMember(p->GetGUID());
}

// Vanilla food and drink by the level that uses them (the highest whose required level the bot meets is stocked). An id missing
// from this world's item_template is skipped, and what was stocked is logged by name, so a wrong id shows in the log.
struct Ration { std::uint32_t entry; };
constexpr Ration kDrinks[] = { {159}, {1179}, {1205}, {1708}, {1645}, {8766} };   // Refreshing Spring Water ... Morning Glory Dew
constexpr Ration kFoods[]  = { {4540}, {4541}, {4542}, {4544}, {4601}, {8950} };  // Tough Hunk of Bread ... Homemade Cherry Pie
constexpr std::uint32_t kRationStack = 60;   // a full dungeon run: deadmines_full drank 20 and stalled on the empty stack

void StockRation(Run& run, Player* p, Ration const* table, std::size_t n)
{
    ItemTemplate const* pick = nullptr;
    for (std::size_t i = 0; i < n; ++i)
    {
        ItemTemplate const* t = sObjectMgr->GetItemTemplate(table[i].entry);
        if (t && t->GetBaseRequiredLevel() <= int32(p->GetLevel()))
            pick = t;
    }
    if (!pick)
        return;
    std::uint32_t const have = p->GetItemCount(pick->GetId(), false);
    if (have >= kRationStack)
        return;
    ItemPosCountVec dest;
    if (p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, pick->GetId(), kRationStack - have) != EQUIP_ERR_OK || !p->StoreNewItem(dest, pick->GetId(), true))
    {
        TC_LOG_WARN("playerbot.v2", "[scenario] {} could not carry {} (entry {})", p->GetName(), pick->GetName(LOCALE_enUS), pick->GetId());
        return;
    }
    run.stage_status += fmt::format("{}=stocked({}) ", p->GetName(), pick->GetId());
    TC_LOG_INFO("playerbot.v2", "[scenario] {} stocked {} x{} (entry {})", p->GetName(), pick->GetName(LOCALE_enUS), kRationStack - have, pick->GetId());
}

// A scenario character is reused run after run, and every regear leaves the gear it replaced in the bags: by 0.1.23 the deadmines_full priest,
// warrior and mage had no room for a ration ("could not carry Melon Juice"), and the priest never drank (healer_no_rest, then healer_oom).
// Everything carried loose in the backpack and bags goes before the rations and supplies are stocked (bags themselves, quest items and the
// equipped gear stay). Scenario characters only: this runs in the staging of a scenario squad.
void ClearScenarioBags(Run& run, Player* p)
{
    std::vector<std::pair<uint8, uint8>> slots;
    p->ForEachItem(ItemSearchLocation::Inventory, [&](Item* item)
    {
        ItemTemplate const* t = item->GetTemplate();
        if (!item->IsBag() && t && t->GetClass() != ITEM_CLASS_QUEST)
            slots.emplace_back(item->GetBagSlot(), item->GetSlot());
        return ItemSearchCallbackResult::Continue;
    });
    for (auto const& [bag, slot] : slots)
        p->DestroyItem(bag, slot, true);
    if (!slots.empty())
    {
        run.stage_status += fmt::format("{}=bags_cleared({}) ", p->GetName(), slots.size());
        TC_LOG_INFO("playerbot.v2", "[scenario] {} bags cleared: {} items", p->GetName(), slots.size());
    }
}

void StockRations(Run& run, Player* p)
{
    if (p->GetPowerType() == POWER_MANA)
        StockRation(run, p, kDrinks, std::size(kDrinks));
    StockRation(run, p, kFoods, std::size(kFoods));
}

// One staging step for one member. True once the bot is in the world, at the level, geared, role-pinned, alive and out of any group.
// The gear the owner's raid wears in each raid: Molten Core, Zul'Gurub and the Ruins of Ahn'Qiraj on Tier 1 (the 20-raider instances drop
// between the tiers), Blackwing Lair and the Temple of Ahn'Qiraj on Tier 2 (the data has no Tier 2.5 set), Naxxramas on Tier 3. Match:
// not a raid scenario, the setup pipeline's level kit stays.
Owned::GearMode RaidGearTier(std::string_view name)
{
    if (name.starts_with("mc_") || name.starts_with("zg_") || name.starts_with("aq20_"))
        return Owned::GearMode::T1;
    if (name.starts_with("bwl_") || name.starts_with("aq40_"))
        return Owned::GearMode::T2;
    if (name.starts_with("naxx_") || name.starts_with("dps_scorecard") || name.starts_with("support_scorecard"))
        return Owned::GearMode::T3;
    return Owned::GearMode::Match;
}

char const* SchoolName(std::uint8_t school)
{
    switch (school)
    {
        case SPELL_SCHOOL_HOLY:   return "holy";
        case SPELL_SCHOOL_FIRE:   return "fire";
        case SPELL_SCHOOL_NATURE: return "nature";
        case SPELL_SCHOOL_FROST:  return "frost";
        case SPELL_SCHOOL_SHADOW: return "shadow";
        case SPELL_SCHOOL_ARCANE: return "arcane";
        default:                  return "no";
    }
}

bool StageMember(Run& run, Member& m)
{
    ScenarioDef const& def = *run.def;
    ::Playerbot::BotId id = m.id;
    if (!id)
    {
        ObjectGuid const known = sCharacterCache->GetCharacterGuidByName(m.def.name);
        id = known.IsEmpty() ? 0 : known.GetCounter();
        if (!id)
        {
            auto const made = ::Playerbot::V2::BotCharacterFactory::Create(nullptr, m.def.name, m.def.race, m.def.cls, /*gender*/ 0);
            if (!made.ok)
            {
                run.monitor->NoteSetupFailure(run.run_ms, m.def.name, "could not create the character: " + made.reason);
                return false;
            }
            // Its row is not committed yet: log it in on a later step (a login on an uncommitted row leaves a zombie session).
            m.id = made.guid.GetCounter();
            run.stage_status += fmt::format("{}=created ", m.def.name);
            return false;
        }
        m.id = id;
    }

    ::Playerbot::Services::Lifecycle().mark_as_bot(id);
    ::Playerbot::Services::Population().ProtectFromKick(id, 10u * 60u * 1000u);   // pool inventory is otherwise logged out by the shaper
    // Held like an owner's bot: raid-tier gear at level 60, never scrubbed to the world tier or drawn into a battleground fill mid-run.
    ::Playerbot::Pool::MarkScenarioBot(id);

    ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(id);
    Player* p = ObjectAccessor::FindConnectedPlayer(guid);
    if (!p || !p->IsInWorld())
    {
        if (::Playerbot::Services::SessionMgr().IsHeadless(guid))
        {
            // In flight: leave it a few steps; a zombie that never lands is dropped by the stage timeout.
            run.stage_status += fmt::format("{}=login ", m.def.name);
        }
        else
        {
            auto const login = ::Playerbot::Services::SessionMgr().LoginBot(guid);
            run.stage_status += fmt::format("{}={} ", m.def.name, login.ok ? "login" : "login_refused(" + login.reason + ")");
        }
        return false;
    }

    // Out of any instance or old group first: a bot left in a dungeon would re-aggro and never leave combat.
    if (p->GetMap() && (p->GetMap()->IsDungeon() || p->GetMap()->IsBattlegroundOrArena()))
    {
        LeaveGroup(p);
        p->CombatStop(true);
        SendToStage(p, run.def->goal == ScenarioGoalKind::Parity);
        run.stage_status += fmt::format("{}=staged ", m.def.name);
        return false;
    }
    if (p->IsInCombat())
    {
        p->CombatStop(true);
        run.stage_status += fmt::format("{}=incombat ", m.def.name);
        return false;
    }
    if (!p->IsAlive())
    {
        p->ResurrectPlayer(1.0f);
        p->SpawnCorpseBones();
        run.stage_status += fmt::format("{}=revive ", m.def.name);
        return false;
    }
    if (!::Playerbot::Services::Registry().has(id))
    {
        run.stage_status += fmt::format("{}=noai ", m.def.name);
        return false;
    }

    // The role first: the pipeline's gear and talent steps follow the pinned role, and without one they roll a role of their own
    // (a healer in tank gear, then re-geared by the upkeep in the middle of the run).
    if (!m.role_pinned)
    {
        ::Playerbot::V2::Fleet::BotPopulationManager::ApplyCompanionRole(p, ToBotRole(m.def.role), "scenario");
        m.role_pinned = true;
        run.stage_status += fmt::format("{}=role ", m.def.name);
        return false;
    }

    // A damage_parity run played as another specialization (damage_parity_warrior_fury) gets that specialization's talents before it is
    // geared: the role above pins the class's default DPS spec, so the fury run kept the Arms talents (Mortal Strike, no Bloodthirst) and
    // only its rotation rules were Fury's (136: Mortal Strike and Whirlwind cast, no Bloodthirst).
    if (def.parity && def.parity_spec && ::Playerbot::Classic::GetSpecOverride(p->GetGUID()) != def.parity_spec)
    {
        ::Playerbot::Classic::SetSpecOverride(p, def.parity_spec);
        ::Playerbot::API api(p);
        api.apply_talent_build(0);
        run.stage_status += fmt::format("{}=spec ", m.def.name);
        return false;
    }

    // Level, gear and spells through the standard setup pipeline, exactly one pipeline step per call. A character set up for another
    // level (the shared dungeon party is re-levelled for each row) has its pipeline marked done: it runs again for this level.
    if (uint32(p->GetLevel()) != def.level)
    {
        ::Playerbot::V2::Fleet::BotSetupPipeline{}.Reset(p);
        p->GiveLevel(def.level);
        // GiveLevel keeps the experience points: a member set down from the Deadmines row's 21 to Ragefire's 14 carried its level-21
        // points, more than a level 14 needs, and the first experience it gained before entering levelled it to 15, so it never
        // entered (127: "Scnhunter level 15, wrong level, map 0"). A character at its new level starts at zero.
        p->SetXP(0);
        p->InitTalentForLevel();
        // GiveLevel keeps the spells too: the priest set down from the scorecard's 60 to Ragefire's 14 healed with its level-50 ranks
        // and was out of mana in the first pulls (0.1.24 rfc_oggleflint and wc_cobrahn healer_oom).
        if (uint32 const unlearned = ::Playerbot::API(p).unlearn_spells_above_level())
            TC_LOG_INFO("playerbot.v2", "[scenario] {} {}: {} spells above level {} unlearned", def.name, m.def.name, unlearned, def.level);
        m.kit_learned = false;
    }
    // The pipeline's SetLevel step returns early for a bot already at the level (levelled just above, or by an earlier run), so it
    // never trains the class: the squad's level-21 warrior fought without Thunder Clap. Trainer spells up to the level, class
    // quest abilities and weapon skills, once per run and member.
    if (!m.kit_learned)
    {
        ::Playerbot::V2::Fleet::BotSetupPipeline::LearnLevelKit(p);
        m.kit_learned = true;
    }
    if (!::Playerbot::V2::Fleet::BotSetupPipeline{}.RunFor(p, def.level) || uint32(p->GetLevel()) != def.level)
    {
        run.stage_status += fmt::format("{}=setup ", m.def.name);
        return false;
    }

    // A damage_parity run measures the class's own build, committed before the first leg. The setup pipeline's talent step and the idle
    // rule only commit out of combat, and the bot stands in combat with the dummy as soon as it is staged: the paladin (143) first
    // committed its build between the t3 and t3_buffed legs, so the none/t1/t3 legs ran without Seal of Command and Vengeance.
    if (def.parity && !m.parity_talents && m.parity_talent_tries < 20)
    {
        ++m.parity_talent_tries;
        // The base scenario plays the class's own default (warrior: Arms, 71). The override of an earlier run of the same character stays in
        // the registry: the arms run after a fury run committed the Fury build (144: spec 72, Bloodthirst and Whirlwind, no Mortal Strike).
        ::Playerbot::Classic::SetSpecOverride(p, m.def.spec ? m.def.spec : def.parity_spec ? def.parity_spec : (p->GetClass() == CLASS_WARRIOR ? 71u : 0u));
        if (::Playerbot::API(p).apply_talent_build(0) == ::Playerbot::Result::Ok)
            m.parity_talents = true;
        else if (m.parity_talent_tries == 20)
            TC_LOG_WARN("playerbot.v2", "[scenario] {} {}: the classic build was not committed in 20 tries", def.name, m.def.name);
        run.stage_status += fmt::format("{}=talents ", m.def.name);
        return false;
    }

    // A hunter's pet is a fifth to a third of its damage in vanilla; a scenario hunter had none (161: pet_dps 0, no stabled pet for the call rule;
    // the setup pipeline's pet step never ran for a reused character). Every scenario hunter gets the race's starter pet.
    if (p->GetClass() == CLASS_HUNTER && p->GetLevel() >= 10 && !m.parity_pet && m.parity_pet_tries < 20)
    {
        ++m.parity_pet_tries;
        using PetResult = ::Playerbot::API::StarterPetResult;
        PetResult const r = ::Playerbot::API(p).grant_starter_hunter_pet(::Playerbot::V2::World::StarterHunterPetEntry(p->GetRace(), p->GetTeam() == ALLIANCE));
        if (r != PetResult::NotNow)
        {
            m.parity_pet = true;
            TC_LOG_INFO("playerbot.v2", "[scenario] {} {}: starter pet {}", def.name, m.def.name,
                        r == PetResult::Tamed ? "tamed" : r == PetResult::HasPet ? "already there" : "unavailable (no tameable beast in this data)");
        }
        run.stage_status += fmt::format("{}=pet ", m.def.name);
        return false;
    }
    if (m.def.spec)
        ::Playerbot::Classic::SetSpecOverride(p, m.def.spec);   // the dps scorecard: the pipeline rolls a spec of its own, the member's pin stays

    // A raid scenario fields the gear the owner's raid wears for that raid (RaidGearTier: the class tier set plus raid pieces up to its item
    // level) and the resistance gear a vanilla raid dressed for the fight (ScenarioDef::resist_school). The setup pipeline's generated gear
    // is a level-60 green/blue kit, so a boss fight on it runs ~4x too long (mc_garr: healers out of mana in a fight a T1 squad finishes;
    // naxx_patchwerk 124: the tanks at 6.5-7.5k health under Hateful Strikes of up to 22k). The overrides stay for the run (the upkeep
    // regears with them) and are dropped in Finish.
    if (Owned::GearMode const tier = RaidGearTier(def.name); !m.tier_geared && def.level >= 60 && tier != Owned::GearMode::Match)
    {
        ::Playerbot::V2::Fleet::BotPopulationManager::SetGearModeOverride(id, int(tier));
        ::Playerbot::V2::Fleet::BotPopulationManager::SetGearResistOverride(id, def.resist_school);
        ::Playerbot::V2::Fleet::BotPopulationManager::SetGearWearOverride(id, def.wear_entry);
        bool const changed = ::Playerbot::Services::Population().RegearNow(p);
        m.tier_geared = true;
        TC_LOG_INFO("playerbot.v2", "[scenario] {} {} {} gear{}{} (regear changed items: {})", def.name, m.def.name, Owned::ModeName(tier),
                    def.resist_school ? fmt::format(" with {} resistance", SchoolName(def.resist_school)) : std::string(),
                    def.wear_entry ? fmt::format(" wearing item {}", def.wear_entry) : std::string(), changed ? "yes" : "no");
        run.stage_status += fmt::format("{}={}gear ", m.def.name, Owned::ModeName(tier));
        return false;
    }

    // Food and drink in the bags, as the vendor visit of a bot that had walked here would have left them: the upkeep rules
    // eat and drink between pulls only from what they carry (Deadmines run 3: no drink line from the priest at all, while
    // the tank's healer-mana gate held the pull).
    ClearScenarioBags(run, p);
    StockRations(run, p);

    // The class supplies a bot's own upkeep tops up (reagents, mana and healing potions, Demonic Runes) and the party buff spells it learns
    // from them, so a raid squad starts as the owner's bots do instead of waiting on the idle rule.
    {
        ::Playerbot::API api(p);
        api.restock_class_supplies();
    }

    // Full health and mana: a run starts from a rested squad, not from whatever the last one left.
    p->SetFullHealth();
    if (p->GetPowerType() == POWER_MANA)
        p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
    LeaveGroup(p);
    return true;
}

void FormGroup(Run& run)
{
    Player* leader = nullptr;
    for (Member const& m : run.members)
        if (m.def.role == ScenarioRoleKind::Tank)
        {
            leader = Find(m.id);
            break;
        }
    if (!leader)
        leader = Find(run.members.front().id);
    Group* group = new Group();
    group->Create(leader);
    sGroupMgr->AddGroup(group);
    if (run.members.size() > 5)
        group->ConvertToRaid();
    run.group_guid = group->GetGUID();
    for (Member const& m : run.members)
        if (Player* p = Find(m.id); p && p != leader)
            if (!group->AddMember(p))
                TC_LOG_WARN("playerbot.v2", "[scenario] {} could not add {} to the squad's group (members {}, raid {})", run.def->name, m.def.name,
                            group->GetMembersCount(), group->isRaidGroup() ? "yes" : "no");
    group->BroadcastGroupUpdate();
}

// The squad's group is whole: every member is in it and it holds the whole squad. A member found in another group (or none) is
// put back into the squad's group, and what was found is logged once: the group_broken invariant must report the bots' play, not
// a setup that never finished (mc_garr: 24 bots in a group of 2 one second into the run). False while anything is still wrong.
bool GroupSettled(Run& run)
{
    Group* group = run.group_guid.IsEmpty() ? nullptr : sGroupMgr->GetGroupByGUID(run.group_guid);
    if (!group)
        return false;
    bool settled = group->GetMembersCount() == run.members.size();
    std::string found;
    for (Member const& m : run.members)
    {
        Player* p = Find(m.id);
        if (!p || !p->IsInWorld())
        {
            settled = false;
            continue;
        }
        if (p->GetGroup() == group)
            continue;
        settled = false;
        found += fmt::format("{}{}(group of {})", found.empty() ? "" : ", ", m.def.name, p->GetGroup() ? uint32(p->GetGroup()->GetMembersCount()) : 0u);
        if (p->GetGroup())
            p->GetGroup()->RemoveMember(p->GetGUID());
        group->AddMember(p);
    }
    if (!found.empty() && !run.group_logged)
    {
        run.group_logged = true;
        TC_LOG_WARN("playerbot.v2", "[scenario] {} members found outside the squad's group ({} of {} in it), put back: {}", run.def->name,
                    group->GetMembersCount(), run.members.size(), found);
    }
    return settled;
}

// The squad's instance locks for the scenario's map, so a boss killed by an earlier run is alive again. A lock still in use (a map
// that has not unloaded yet) cannot be reset: it is logged, and the run may find the boss already dead.
void ResetLocks(Run& run)
{
    std::uint32_t reset = 0, in_use = 0;
    for (Member const& m : run.members)
    {
        std::vector<InstanceLock const*> done, failed;
        sInstanceLockMgr.ResetInstanceLocksForPlayer(ObjectGuid::Create<HighGuid::Player>(m.id), run.def->map, {}, &done, &failed);
        reset += std::uint32_t(done.size());
        in_use += std::uint32_t(failed.size());
    }
    if (reset || in_use)
        TC_LOG_INFO("playerbot.v2", "[scenario] {} instance locks reset: {} reset, {} still in use", run.def->name, reset, in_use);
    if (in_use)
        TC_LOG_WARN("playerbot.v2", "[scenario] {} {} instance lock(s) are in use by a map that has not unloaded: the boss may still be dead from the last run",
                    run.def->name, in_use);
}

// The entry must open a new instance, never the last run's. A run that ends on a failure leaves its boss in combat inside a map that stays
// loaded, and an instance refuses entry while its encounter is in progress (transfer abort 6, "zone in combat": onyxia_full and mc_geddon, n14
// run 2 and run 4). The squad's group goes back to the instance of its recent owner's lock - temporary locks included, which the lock reset
// above does not touch and cannot drop while a map holds them. So the group's recent instance owner is moved to a member who has no lock
// on the map, or one whose locked instance is no longer loaded, and the recent instance ids are cleared: the entry creates a fresh instance.
void ForgetRecentInstance(Run& run)
{
    std::uint32_t forgotten = 0;
    for (Member const& m : run.members)
        if (Player* p = Find(m.id))
        {
            if (p->GetRecentInstanceId(run.def->map))
                ++forgotten;
            p->SetRecentInstance(run.def->map, 0);
        }

    MapEntry const* mapEntry = sMapStore.LookupEntry(run.def->map);
    Player* leader = run.members.empty() ? nullptr : Find(run.members.front().id);
    Group* group = leader ? leader->GetGroup() : nullptr;
    if (!mapEntry || !group)
    {
        TC_LOG_WARN("playerbot.v2", "[scenario] {} no squad group to clear the recent instance of map {} for", run.def->name, run.def->map);
        return;
    }

    MapDifficultyEntry const* difficulty = sDB2Manager.GetDownscaledMapDifficultyData(run.def->map, group->GetDifficultyID(mapEntry));
    ObjectGuid owner;
    std::uint32_t busy = 0;
    if (difficulty)
    {
        MapDb2Entries const entries{ mapEntry, difficulty };
        for (Member const& m : run.members)
        {
            ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(m.id);
            InstanceLock const* lock = sInstanceLockMgr.FindActiveInstanceLock(guid, entries);
            if (!lock || !sMapMgr->FindMap(run.def->map, lock->GetInstanceId()))
            {
                owner = guid;
                break;
            }
            ++busy;
        }
    }
    if (!owner)
        owner = group->GetLeaderGUID();
    group->SetRecentInstance(run.def->map, owner, 0);
    TC_LOG_INFO("playerbot.v2", "[scenario] {} recent instance of map {} cleared for the squad ({} of {} members had one), owner {} ({} members skipped: their locked instance is still loaded)",
                run.def->name, run.def->map, forgotten, run.members.size(), owner.ToString(), busy);
}

// The goal boss's first spawn in the creature table: the boss room the raid is moved to is found from it.
bool FindBossSpawn(Run& run)
{
    for (auto const& kv : sObjectMgr->GetAllCreatureData())
    {
        CreatureData const& cd = kv.second;
        if (cd.id == run.arrive_entry && cd.mapId == run.def->map)
        {
            run.boss_x = cd.spawnPoint.GetPositionX();
            run.boss_y = cd.spawnPoint.GetPositionY();
            run.boss_z = cd.spawnPoint.GetPositionZ();
            run.boss_o = cd.spawnPoint.GetOrientation();
            run.boss_spawn_id = kv.first;
            run.boss_spawn_known = true;
            return true;
        }
    }
    // No spawn row: a boss a script summons (Ragnaros) is placed from the scenario's lair.
    if (run.def->lair_z != 0.f)
    {
        run.boss_x = run.def->lair_x;
        run.boss_y = run.def->lair_y;
        run.boss_z = run.def->lair_z;
        run.boss_o = run.def->lair_o;
        run.boss_spawn_known = true;
        return true;
    }
    return false;
}

// The creatures the raid is placed next to on purpose: the boss and the adds the scenario keeps through the staging (Venoxis' cobras,
// Sartura's guards stand at their boss's side, so a spot 40 y from the boss is never 40 y from them). Garr's Firesworn are not kept, only
// in his formation, and keep their 40 y.
bool EncounterEntry(Run const& run, std::uint32_t entry)
{
    return entry == run.arrive_entry || (run.def && (entry == run.def->goal_entry || (run.def->keep_entry && entry == run.def->keep_entry) ||
                                                      (run.def->keep_entry_b && entry == run.def->keep_entry_b) ||
                                                      (run.def->keep_entry_c && entry == run.def->keep_entry_c)));
}

// The living hostile creature nearest to a point, with its distance: a squad must not land inside any pack's aggro. Spawned creatures
// only (the encounter's own adds are spawn rows too: Garr's Firesworn stand around him and are asleep until somebody comes near).
Creature const* NearestHostile(Map* map, Player* viewer, Run const& run, float x, float y, float z, float& dist)
{
    Creature const* best = nullptr;
    float best_d2 = std::numeric_limits<float>::max();
    for (auto const& kv : map->GetCreatureBySpawnIdStore())
    {
        Creature const* c = kv.second;
        if (!c || !c->IsAlive() || EncounterEntry(run, c->GetEntry()) || !c->IsHostileTo(viewer))
            continue;
        float const dx = c->GetPositionX() - x, dy = c->GetPositionY() - y, dz = c->GetPositionZ() - z;
        float const d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < best_d2) { best_d2 = d2; best = c; }
    }
    dist = best ? std::sqrt(best_d2) : 0.f;
    return best;
}

// Clear of every hostile by more than a level-60 creature's aggro reach plus a margin (the first mc_garr landing, 40 y from Garr, was
// inside his Firesworn and a hound pack: six dead in four seconds).
constexpr float kArriveClearance = 40.f;

// Clear of every leg of a boss's patrol by this much: the core's aggro reach of a boss against a level 60 is under 20 y, and the squad
// spreads a few yards round the landing spot. Patchwerk walks 215 y along the slime (creature_addon path 800882980, a 60 s wait at the far
// end) and his spawn is one end of it, so a spot 40 y from the spawn was 11 y from the walk (naxx_patchwerk 2026-10-20 124: engaged 6 s
// after the landing, 3 y past the waypoint he had just reached).
constexpr float kPatrolClearance = 30.f;

// A dungeon checkpoint lands the squad among the trash the group would meet anyway (it is bot-led from the first second), so it must
// only be out of every pack's aggro, not 40 y clear: a 5-man has hostiles everywhere (deadmines_full@sneed: the nearest to any candidate
// spot was 1 y away, 40 y clearance never passes). A level 17-22 creature notices from about 20 y at equal level.
constexpr float kCheckpointClearance = 22.f;

// The longest walk from a checkpoint's landing spot to its creature, as a multiple of the straight distance: a spot on the far side of
// a wall or a chasm is in sight (LoS) but not on the route.
constexpr float kCheckpointPathSlack = 1.8f;

// How far round the boss's spawn the clearing loads the map first (ClearRaidTrash): the room, its approaches and the patrols that walk
// into it. A grid is 533 y across, so this loads at most the four grids round the spawn.
constexpr float kClearGridRange = 200.f;

// What the killed trash summons as it dies, and what those summons summon in turn, goes the way of the trash: the clearing stands for a
// raid that has fought it. Zul'Gurub's Mad Servants (15111) at the Edge of Madness cast Portal of Madness (24621) on death (their world DB
// script); each portal (15141, not selectable) summons two Mad Voidwalkers (15146) 6 s later. The clearing killed the eleven servants at the
// landing, the portals came up after it, and about twenty voidwalkers were on the raid 11 s after it was placed (zg_grilek, zg_hazzarah,
// zg_renataki, zg_wushoolay 1110b: 17-20 of 20 dead to Shadow Shock before the order). A summon is killed when it is hostile to the raid,
// not one the scenario keeps (the boss, its kept entries and goal adds), and its summoner is dead or gone; a living summoner's summons
// stand (Victor Nefarius' drakonids, a boss's adds). Run at the clearing, at the placement and every 2 s until the order, as the summons
// of a death come on later map updates. Returns how many died.
std::size_t KillTrashSummons(Run& run, Player* viewer)
{
    Map* map = viewer->GetMap();
    if (!map)
        return 0;
    ScenarioDef const& d = *run.def;
    auto const kept = [&d](uint32 entry)
    {
        return entry == d.arrive_near_entry || entry == d.goal_entry || (d.keep_entry && entry == d.keep_entry) ||
               (d.keep_entry_b && entry == d.keep_entry_b) || (d.keep_entry_c && entry == d.keep_entry_c) ||
               (d.goal_adds_needed && (entry == d.goal_adds_a || entry == d.goal_adds_b));
    };
    std::size_t killed = 0;
    std::map<uint32, std::size_t> by_entry;
    // A portal and its voidwalkers in one call: the second pass finds the voidwalkers' summoner dead.
    for (int pass = 0; pass < 3; ++pass)
    {
        std::vector<Creature*> doomed;
        for (auto const& [guid, c] : map->GetObjectsStore().Data.FindContainer<Creature>())
        {
            if (!c || !c->IsAlive() || c->GetSpawnId() || !c->IsHostileTo(viewer) || kept(c->GetEntry()) || c->IsDungeonBoss() || c->isWorldBoss())
                continue;
            TempSummon const* ts = c->ToTempSummon();
            if (!ts || ts->GetSummonerGUID().IsEmpty() || ts->GetSummonerGUID().IsPlayer())
                continue;
            Unit const* summoner = ts->GetSummonerUnit();
            if (summoner && summoner->IsAlive())
                continue;
            doomed.push_back(c);
        }
        if (doomed.empty())
            break;
        for (Creature* c : doomed)
        {
            ++by_entry[c->GetEntry()];
            c->KillSelf(false);
        }
        killed += doomed.size();
    }
    if (killed)
    {
        std::string text;
        for (auto const& [entry, n] : by_entry)
            text += fmt::format(" {} x{}", entry, n);
        TC_LOG_INFO("playerbot.v2", "[scenario] {} killed {} creatures summoned by dead trash:{}", d.name, killed, text);
    }
    return killed;
}

// A raid scenario tests the boss fight, not the trash on the way: staging kills every living hostile on the map except the goal boss
// and the creatures formed up with him (Garr's Firesworn), so the instance stands as it does once the trash is cleared. A real kill
// (no loot recipient, so nothing to loot) rather than a despawn: the respawn timers run as in a cleared instance. Returns how many died.
std::size_t ClearRaidTrash(Run& run, Player* viewer)
{
    Map* map = viewer->GetMap();
    if (!map)
        return 0;
    // The map's spawn store holds only the creatures of loaded grids, and at this point the raid stands at the entrance: a boss far from
    // it has its room unloaded, so nothing there was killed and the room filled with fresh trash the moment the raid landed in it
    // (release-0130 aq20_ayamiss: "0 object(s) in the map" for Ayamiss 1200 y from the entrance; Hive'Zara Wasps and Stingers 4 y from
    // their spawn points on the raid 11 s after the landing, then ten more at 20 s, and the raid wiped at 164 s). Load the grids round
    // the boss's spawn first, so its room is cleared like the rest.
    for (auto const& kv : sObjectMgr->GetAllCreatureData())
        if (kv.second.id == run.def->arrive_near_entry && kv.second.mapId == map->GetId())
        {
            float const bx = kv.second.spawnPoint.GetPositionX(), by = kv.second.spawnPoint.GetPositionY();
            map->LoadGridsInRange(bx, by, kClearGridRange);
            TC_LOG_INFO("playerbot.v2", "[scenario] {} loaded the grids within {:.0f} y of creature {}'s spawn at {:.1f},{:.1f} before the clearing",
                        run.def->name, kClearGridRange, run.def->arrive_near_entry, bx, by);
        }
    // A boss with no spawn row (Majordomo, Ragnaros: the instance script summons him) is not in the loop above. A summon creates its grid
    // but does not load the grid's spawns, so his room was never cleared either: release-0130 mc_majordomo had a Lava Surger 26 y from the
    // tank at the attack order and Ancient Core Hounds on the raid at 280 s, all made after the clearing had reported "none" left. Load
    // the grids round his lair too.
    if (run.def->lair_z != 0.f)
    {
        map->LoadGridsInRange(run.def->lair_x, run.def->lair_y, kClearGridRange);
        TC_LOG_INFO("playerbot.v2", "[scenario] {} loaded the grids within {:.0f} y of creature {}'s lair at {:.1f},{:.1f} before the clearing",
                    run.def->name, kClearGridRange, run.def->arrive_near_entry, run.def->lair_x, run.def->lair_y);
    }
    CreatureGroup* boss_formation = nullptr;
    for (auto const& kv : map->GetCreatureBySpawnIdStore())
        if (kv.second && kv.second->GetEntry() == run.def->arrive_near_entry)
        {
            boss_formation = kv.second->GetFormation();
            break;
        }
    // Another encounter's boss is set aside without a death: a boss's death runs its script, and Razorgore's phase-one death casts
    // Explosion (20038) on every living player in the map (bwl_broodlord 2026-10-20: 35 of 40 raiders dead in the landing second, killed by
    // nothing that had attacked them, after the clearing had killed Razorgore). A forced despawn runs no JustDied; EnsureBossAlive brings a
    // despawned boss back through its spawn row when its own scenario runs.
    ::Playerbot::Raid::RaidRules const* rules = ::Playerbot::Raid::RulesForMap(map->GetId());
    auto const other_boss = [rules](std::uint32_t entry)
    {
        return rules && std::find(rules->bosses.begin(), rules->bosses.end(), entry) != rules->bosses.end();
    };
    std::vector<Creature*> doomed;
    std::vector<Creature*> set_aside;
    for (auto const& kv : map->GetCreatureBySpawnIdStore())
    {
        Creature* c = kv.second;
        if (!c || !c->IsAlive() || !c->IsHostileTo(viewer) || c->GetEntry() == run.def->arrive_near_entry || c->GetEntry() == run.def->goal_entry ||
            (run.def->keep_entry && c->GetEntry() == run.def->keep_entry) ||
            (run.def->keep_entry_b && c->GetEntry() == run.def->keep_entry_b) ||
            (run.def->keep_entry_c && c->GetEntry() == run.def->keep_entry_c) ||
            // The creatures whose deaths are the goal (Majordomo's Flamewakers) stand for the fight.
            (run.def->goal_adds_needed && (c->GetEntry() == run.def->goal_adds_a || c->GetEntry() == run.def->goal_adds_b)))
            continue;
        if (boss_formation && c->GetFormation() == boss_formation)
            continue;
        // RaidRules::bosses names one entry per encounter (it lines up with the boss route), so the other members of a council (the bug trio,
        // the twins) are not in it: a creature of boss rank is set aside too, never killed into its encounter script. The rank, not the
        // flags: in the world DB all five AQ40 council bosses are classification 3, but only Vem and Vek'nilash carry the dungeon boss flag.
        if (other_boss(c->GetEntry()) || c->IsDungeonBoss() || c->isWorldBoss() ||
            c->GetCreatureTemplate()->Classification == CreatureClassifications::Obsolete)
            set_aside.push_back(c);
        else
            doomed.push_back(c);
    }
    for (Creature* c : doomed)
        c->KillSelf(false);
    KillTrashSummons(run, viewer);
    std::string aside;
    for (Creature* c : set_aside)
    {
        aside += fmt::format(" {} ({})", c->GetName(), c->GetEntry());
        c->DespawnOrUnsummon();
    }
    TC_LOG_INFO("playerbot.v2", "[scenario] {} cleared the trash before placing the raid: {} hostile creatures killed (boss entry {} and its formation left "
                "alone), {} other bosses despawned without a death:{}", run.def->name, doomed.size(), run.def->arrive_near_entry, set_aside.size(),
                aside.empty() ? std::string(" none") : aside);
    // What still lives, by entry: n57 onyxia_full run 1 had 18 Onyxian Whelps on the tanks 3 s into the fight that this clearing never listed.
    // Count, hostility to the viewer, whether it stands in the boss's formation, and the first one's place.
    {
        struct Left { std::size_t n = 0; std::size_t hostile = 0; std::size_t inFormation = 0; float x = 0, y = 0, z = 0; };
        std::map<uint32, Left> left;
        for (auto const& kv : map->GetCreatureBySpawnIdStore())
        {
            Creature* c = kv.second;
            if (!c || !c->IsAlive() || c->GetEntry() == run.def->arrive_near_entry)
                continue;
            Left& l = left[c->GetEntry()];
            if (!l.n) { l.x = c->GetPositionX(); l.y = c->GetPositionY(); l.z = c->GetPositionZ(); }
            ++l.n;
            l.hostile += c->IsHostileTo(viewer) ? 1u : 0u;
            l.inFormation += (boss_formation && c->GetFormation() == boss_formation) ? 1u : 0u;
        }
        std::string text;
        for (auto const& [entry, l] : left)
            text += fmt::format(" [entry {} x{} hostile {} in boss formation {} first at {:.1f},{:.1f},{:.1f}]", entry, l.n, l.hostile, l.inFormation, l.x, l.y, l.z);
        TC_LOG_INFO("playerbot.v2", "[scenario] {} living creatures left after the clearing:{}", run.def->name, text.empty() ? std::string(" none") : text);
    }
    return doomed.size();
}

// The living hostiles within kNewcomerYd of the landing before the attack order, by entry, with where each came from: a spawn row (its
// respawn delay, which says whether a creature the clearing killed is back already) or a summon (its summoner's entry and spawn row). The
// clearing's list covers spawn rows only: zg_grilek / zg_hazzarah / zg_renataki / zg_wushoolay 1110b had about twenty Mad Voidwalkers
// (15146) on the raid at the landing 11 s after it was placed, 17-20 of 20 dead to Shadow Shock before the order, and neither they nor a
// Portal of Madness (15141, whose world DB script summons them) was in that list. Logged at the placement and every 2 s until the order.
constexpr float kNewcomerYd = 80.f;

void LogNewcomers(Run& run, Player* viewer, char const* when)
{
    Map* map = viewer->GetMap();
    if (!map)
        return;
    struct Seen { std::size_t n = 0; std::size_t combat = 0; std::string from; float x = 0, y = 0, z = 0; };
    std::map<std::pair<uint32, std::string>, Seen> seen;
    for (auto const& [guid, c] : map->GetObjectsStore().Data.FindContainer<Creature>())
    {
        if (!c || !c->IsAlive() || !c->IsHostileTo(viewer) || c->GetExactDist2d(run.arrive_x, run.arrive_y) > kNewcomerYd)
            continue;
        std::string from;
        if (c->GetSpawnId())
            from = fmt::format("spawn row {} respawn {} s", c->GetSpawnId(), c->GetRespawnDelay());
        else if (TempSummon const* ts = c->ToTempSummon())
        {
            Unit const* summoner = ts->GetSummonerUnit();
            Creature const* sc = summoner ? summoner->ToCreature() : nullptr;
            from = sc ? fmt::format("summoned by entry {} spawn row {}", sc->GetEntry(), sc->GetSpawnId())
                      : fmt::format("summoned by {}", ts->GetSummonerGUID().IsEmpty() ? std::string("nobody") : ts->GetSummonerGUID().ToString());
        }
        else
            from = "no spawn row, not a summon";
        Seen& s = seen[{ c->GetEntry(), from }];
        if (!s.n) { s.x = c->GetPositionX(); s.y = c->GetPositionY(); s.z = c->GetPositionZ(); }
        ++s.n;
        s.combat += c->IsInCombat() ? 1u : 0u;
    }
    std::string text;
    for (auto const& [key, s] : seen)
        text += fmt::format(" [entry {} x{} in combat {} {} first at {:.1f},{:.1f},{:.1f}]", key.first, s.n, s.combat, key.second, s.x, s.y, s.z);
    TC_LOG_INFO("playerbot.v2", "[scenario] {} hostiles within {:.0f} y of the landing {}:{}", run.def->name, kNewcomerYd, when,
                text.empty() ? std::string(" none") : text);
}

// The goal boss stands alive in its room before the raid is placed. A boss that is dead or waiting out a respawn timer (a lock or a
// persisted raid state from an earlier run, a map that outlived the lock reset) is respawned, and what was found is logged either way: a
// boss scenario measures the fight, so a boss missing at the order is a setup fault to repair here, not a result (mc_geddon on 317cbf20
// failed setup twice with "not alive within 400 y"). Returns whether a living boss is in the map now (a respawn from a timer happens on
// the map's next update, so the caller re-checks at the order).
bool EnsureBossAlive(Run& run, Player* viewer)
{
    Map* map = viewer->GetMap();
    if (!map || !run.arrive_entry)
        return false;
    std::uint32_t objects = 0;
    Creature* corpse = nullptr;
    for (auto const& kv : map->GetCreatureBySpawnIdStore())
    {
        Creature* c = kv.second;
        if (!c || c->GetEntry() != run.arrive_entry)
            continue;
        ++objects;
        if (c->IsAlive())
        {
            TC_LOG_INFO("playerbot.v2", "[scenario] {} boss {} spawn {} is alive at {:.1f},{:.1f},{:.1f}", run.def->name, run.arrive_entry, kv.first,
                        c->GetPositionX(), c->GetPositionY(), c->GetPositionZ());
            return true;
        }
        corpse = c;
    }
    ObjectGuid::LowType spawn_id = 0;
    for (auto const& kv : sObjectMgr->GetAllCreatureData())
        if (kv.second.id == run.arrive_entry && kv.second.mapId == run.def->map)
        {
            spawn_id = kv.first;
            break;
        }
    // A boss the instance script spawns itself (Ragnaros: spawn group 338, manual, spawned when Majordomo is done) has no creature in a fresh
    // map: spawn its group, as the script would.
    if (!objects && spawn_id)
        if (SpawnGroupTemplateData const* group = sObjectMgr->GetSpawnGroupData(SPAWN_TYPE_CREATURE, spawn_id))
            if (group->groupId && (group->flags & SPAWNGROUP_FLAG_MANUAL_SPAWN) && !(group->flags & SPAWNGROUP_FLAG_SYSTEM))
            {
                bool const spawned = map->SpawnGroupSpawn(group->groupId, /*ignoreRespawn*/ true, /*force*/ true);
                TC_LOG_INFO("playerbot.v2", "[scenario] {} boss {} is in manual spawn group {} ({}): spawned the group: {}", run.def->name, run.arrive_entry,
                            group->groupId, group->name, spawned);
                return false;
            }
    // A boss with no spawn row (Ragnaros: Majordomo's script summons him at his lair): summon it as the script does, for two hours.
    if (!objects && !spawn_id && run.def->lair_z != 0.f)
    {
        // One the instance script has already summoned (Instance.MoltenCoreSkipRunes raises Ragnaros as soon as a player is inside) is not in the
        // spawn store but is alive in the map: use it. A second one on the same spot took the tanks' Attack order (start_attack: InvalidTarget on the
        // summon that still carried the script's immune and uninteractible flags) and left the fight unstarted (MC Ragnaros 2026-10-13 d202b196).
        for (auto const& [guid, c] : map->GetObjectsStore().Data.FindContainer<Creature>())
            if (c && c->GetEntry() == run.arrive_entry && c->IsAlive())
            {
                TC_LOG_INFO("playerbot.v2", "[scenario] {} boss {} already stands in the map (guid {}) at {:.1f},{:.1f},{:.1f}: not summoning another", run.def->name,
                            run.arrive_entry, c->GetGUID().GetCounter(), c->GetPositionX(), c->GetPositionY(), c->GetPositionZ());
                return true;
            }
        Position const lair(run.def->lair_x, run.def->lair_y, run.def->lair_z, run.def->lair_o);
        if (Creature* boss = map->SummonCreature(run.arrive_entry, lair, nullptr, std::chrono::hours(2)))
        {
            // The summoning event normally lifts these; a scenario has no event.
            boss->SetImmuneToPC(false);
            boss->SetUninteractible(false);
            TC_LOG_INFO("playerbot.v2", "[scenario] {} boss {} has no spawn row: summoned at its lair {:.1f},{:.1f},{:.1f}", run.def->name, run.arrive_entry,
                        boss->GetPositionX(), boss->GetPositionY(), boss->GetPositionZ());
            return true;
        }
        TC_LOG_WARN("playerbot.v2", "[scenario] {} boss {} has no spawn row and the summon at its lair failed", run.def->name, run.arrive_entry);
        return false;
    }
    time_t const due = spawn_id ? map->GetCreatureRespawnTime(spawn_id) : 0;
    TC_LOG_WARN("playerbot.v2", "[scenario] {} boss {} is not alive: {} object(s) in the map, spawn row {}, respawn due in {} s; respawning it", run.def->name,
                run.arrive_entry, objects, spawn_id ? std::to_string(spawn_id) : "none", due > GameTime::GetGameTime() ? int64(due - GameTime::GetGameTime()) : 0);
    if (corpse)
        corpse->Respawn(true);
    else if (spawn_id)
        map->Respawn(SPAWN_TYPE_CREATURE, spawn_id);
    return false;
}

// Once a second for a dungeon squad (5 or fewer): where each member stands, and the hostile creatures near them: every one in combat
// within 30 y, and the idle ones within 25 y (the aggro reach of a level-capped pack), with whom each attacks. The bot lines never say
// where anybody stood (rfd_glutton 0.1.27 set C: a ghoul the pull had not tagged was on the mage 4 s into the pull, and no line showed
// whether the mage had walked into it or it had come with the pull).
constexpr float kPosLogCombatYd = 30.f;
constexpr float kPosLogIdleYd = 25.f;
constexpr std::size_t kPosLogMobsMax = 16;

void LogPositions(Run& run)
{
    if (run.members.size() > 5)
        return;
    std::string squad, mobs;
    std::set<ObjectGuid> seen;
    std::size_t mob_count = 0, mob_more = 0;
    for (Member const& m : run.members)
    {
        Player* q = Find(m.id);
        if (!q || !q->IsInWorld())
            continue;
        squad += fmt::format(" {}({:.1f},{:.1f},{:.1f} {:.0f}%{}{}{})", q->GetName(), q->GetPositionX(), q->GetPositionY(), q->GetPositionZ(),
                             q->GetHealthPct(), q->IsAlive() ? "" : " dead", q->IsInCombat() ? " c" : "", q->isMoving() ? " m" : "");
        if (!q->IsAlive())
            continue;
        std::list<Creature*> near;
        q->GetCreatureListWithOptionsInGrid(near, kPosLogCombatYd, FindCreatureOptions());
        for (Creature* c : near)
        {
            if (!c || !c->IsAlive() || c->IsCritter() || !c->IsHostileTo(q) || seen.count(c->GetGUID()))
                continue;
            if (!c->IsInCombat() && q->GetDistance(c) > kPosLogIdleYd)
                continue;
            seen.insert(c->GetGUID());
            if (mob_count >= kPosLogMobsMax)
            {
                ++mob_more;
                continue;
            }
            ++mob_count;
            Unit* v = c->GetVictim();
            mobs += fmt::format(" {}/{}({:.1f},{:.1f},{:.1f}{})", c->GetEntry(), c->GetGUID().GetCounter(), c->GetPositionX(), c->GetPositionY(),
                                c->GetPositionZ(), v ? " ->" + v->GetName() : c->IsInCombat() ? std::string(" c") : std::string());
        }
    }
    if (squad.empty())
        return;
    TC_LOG_INFO("playerbot.v2", "[scenario_pos] {} t={}s squad:{} | mobs:{}{}", run.def->name, run.run_ms / 1000u, squad,
                mobs.empty() ? std::string(" none") : mobs, mob_more ? fmt::format(" (+{} more)", mob_more) : std::string());
}

// Where the healers are in the first seconds after the landing and what each is doing (MC Garr 2026-10-10: healers 30 y out from the
// landing by t=4 under a Hold, rule unknown): one line per healer per second, 14 rounds: its distance from the landing spot, whether it
// moves, whether the Hold is on, and its last bot rule.
void LogLanding(Run& run)
{
    if (!run.def || run.def->arrive_near_entry == 0 || run.checkpoint || run.landing_rounds >= 14)
        return;
    std::uint32_t const now = GameTime::GetGameTimeMS();
    if (run.landing_rounds && now - run.landing_log_ms < 1000u)
        return;
    run.landing_log_ms = now;
    ++run.landing_rounds;
    for (Member const& m : run.members)
    {
        if (m.def.role != ScenarioRoleKind::Healer)
            continue;
        Player* p = Find(m.id);
        if (!p || !p->IsInWorld())
            continue;
        BotAI* ai = ::Playerbot::Services::Registry().ai(m.id);
        // What drives the walk: the movement generator, and the spline's end point when one is running.
        auto const& dest = p->movespline->FinalDestination();
        TC_LOG_INFO("playerbot.v2", "[scenario] {} landing round {} {}: {:.0f} y from the landing spot, moving {}, hold {}, rule {}, generator {}, spline {} to {:.0f},{:.0f}", run.def->name,
                    run.landing_rounds, p->GetName(), std::hypot(p->GetPositionX() - run.arrive_x, p->GetPositionY() - run.arrive_y),
                    p->isMoving() ? 1 : 0, ai && ai->holding_position(now) ? 1 : 0, ai && ai->last_rule_fired() ? ai->last_rule_fired() : "-",
                    uint32(p->GetMotionMaster()->GetCurrentMovementGeneratorType()), p->movespline->Finalized() ? "done" : "running", dest.x, dest.y);
    }
}

// Why the fight starts before the order (MC Garr 2026-10-10: "pull start: Firesworn x3" at t=0, tanks dead by t=11): the first time
// each hostile creature is seen in combat after the landing, one line says what it is attacking, how far that is from the landing
// spot, how far the creature is from its home and its spawn, and whether it belongs to the boss's formation. Capped at 24 lines.
Creature* FindBoss(Run& run);
void LogEngagements(Run& run, Player* viewer)
{
    if (!run.def || !run.def->arrive_near_entry || run.engaged_logged.size() >= 24)
        return;
    Map* map = viewer->GetMap();
    if (!map)
        return;
    CreatureGroup* boss_formation = nullptr;
    for (auto const& kv : map->GetCreatureBySpawnIdStore())
        if (kv.second && kv.second->GetEntry() == run.def->arrive_near_entry) { boss_formation = kv.second->GetFormation(); break; }
    for (auto const& kv : map->GetCreatureBySpawnIdStore())
    {
        Creature* c = kv.second;
        if (!c || !c->IsAlive() || !c->IsInCombat() || run.engaged_logged.count(c->GetGUID()) || run.engaged_logged.size() >= 24)
            continue;
        run.engaged_logged.insert(c->GetGUID());
        Unit* victim = c->GetVictim();
        float const home = c->GetExactDist2d(c->GetHomePosition());
        float const to_landing = std::hypot(c->GetPositionX() - run.arrive_x, c->GetPositionY() - run.arrive_y);
        float nearest_raider = -1.f;
        Player* nearest = nullptr;
        for (Member const& m : run.members)
            if (Player* p = Find(m.id); p && p->IsInWorld() && p->GetMapId() == c->GetMapId())
                if (float const d = c->GetExactDist2d(p); nearest_raider < 0.f || d < nearest_raider)
                {
                    nearest_raider = d;
                    nearest = p;
                }
        // The nearest raider's own account of how it got there: its name, its last bot rule and how far it has walked from the landing.
        std::string who = "-";
        if (nearest)
        {
            char const* rule = "-";
            if (BotAI* ai = ::Playerbot::Services::Registry().ai(nearest->GetGUID().GetCounter()))
                if (char const* r = ai->last_rule_fired())
                    rule = r;
            who = fmt::format("{} (rule {}, {:.0f} y from the landing spot)", nearest->GetName(), rule,
                              std::hypot(nearest->GetPositionX() - run.arrive_x, nearest->GetPositionY() - run.arrive_y));
        }
        TC_LOG_INFO("playerbot.v2", "[scenario] {} t={}s engaged: entry {} ({}) attacks {} ({}), {:.0f} y from the landing spot, nearest raider {:.0f} y = {}, {:.0f} y from home, "
                    "boss formation {}, boss {} y away", run.def->name, run.run_ms / 1000u, c->GetEntry(), c->GetGUID().GetCounter(),
                    victim ? victim->GetName() : std::string("-"), victim ? (victim->GetTypeId() == TYPEID_PLAYER ? "player" : "creature") : "-", to_landing,
                    nearest_raider, who, home, (boss_formation && c->GetFormation() == boss_formation) ? "member" : "no",
                    FindBoss(run) ? uint32(c->GetExactDist2d(FindBoss(run))) : 0u);
    }
}

// A spot 32-56 y from the boss's spawn, on the floor of the same room, in sight of him and kArriveClearance from every hostile creature,
// preferring the way he faces (the way in).
// Lava or slime in contact with a body at (x, y, z): the core's own test (in, under, or within a tenth of a yard of the surface).
bool InMagmaContact(Map* map, PhaseShift const& ps, float x, float y, float z)
{
    LiquidData ld;
    ZLiquidStatus const st = map->GetLiquidStatus(ps, x, y, z, map_liquidHeaderTypeFlags::AllLiquids, &ld);
    return (st & MAP_LIQUID_STATUS_IN_CONTACT) != 0 &&
        (ld.type_flags.HasFlag(map_liquidHeaderTypeFlags::Magma) || ld.type_flags.HasFlag(map_liquidHeaderTypeFlags::Slime));
}

bool PickArrival(Run& run, Player* leader)
{
    Map* map = leader->GetMap();
    if (!map || !run.boss_spawn_known)
        return false;
    PhaseShift const& ps = leader->GetPhaseShift();
    if (run.has_stage)
    {
        // The listed spot on the route: no search. Path-checked once, and the nearest hostile is logged so a landing in a pack shows.
        PathGenerator pg(leader);
        ::Playerbot::BotMovement::SehSafeCalculatePathFrom(pg, run.stage_x, run.stage_y, run.stage_z, run.boss_x, run.boss_y, run.boss_z);
        float const straight = std::hypot(run.boss_x - run.stage_x, run.boss_y - run.stage_y);
        if ((pg.GetPathType() & (PATHFIND_NOPATH | PATHFIND_FARFROMPOLY | PATHFIND_INCOMPLETE)) || pg.GetPathLength() > straight * kCheckpointPathSlack + 10.f)
        {
            run.arrive_blocker = fmt::format("; the listed staging spot {:.1f},{:.1f},{:.1f} has no walkable route to the creature (path type {:#x}, length {:.0f} y for {:.0f} y straight)",
                                             run.stage_x, run.stage_y, run.stage_z, uint32(pg.GetPathType()), pg.GetPathLength(), straight);
            return false;
        }
        float hd = 0.f;
        Creature const* near_c = NearestHostile(map, leader, run, run.stage_x, run.stage_y, run.stage_z, hd);
        run.arrive_x = run.stage_x; run.arrive_y = run.stage_y; run.arrive_z = run.stage_z + 0.5f;
        run.arrive_o = std::atan2(run.boss_y - run.stage_y, run.boss_x - run.stage_x);
        if (run.arrive_o < 0.f)
            run.arrive_o += 6.2831853f;
        TC_LOG_INFO("playerbot.v2", "[scenario] {} checkpoint {} staging spot {:.1f},{:.1f},{:.1f}: {:.0f} y from creature {}, path {:.0f} y, nearest hostile {} {:.0f} y",
                    run.def->name, run.from_key, run.stage_x, run.stage_y, run.stage_z, straight, run.arrive_entry, pg.GetPathLength(),
                    near_c ? near_c->GetEntry() : 0u, hd);
        return true;
    }
    float const step = 6.2831853f / 16.f;
    Creature const* nearest_blocker = nullptr;
    float nearest_blocker_d = 0.f;
    float const clearance = run.checkpoint ? kCheckpointClearance : kArriveClearance;
    std::uint32_t path_rejected = 0, height_rejected = 0, los_rejected = 0, patrol_rejected = 0;
    // The boss's patrol, when he walks one: the live creature's path, else the path of his creature row.
    std::vector<WaypointNode> patrol;
    {
        uint32 path_id = 0;
        if (Creature const* boss = FindBoss(run))
            path_id = boss->GetWaypointPathId();
        if (!path_id && run.boss_spawn_id)
            if (CreatureAddon const* addon = sObjectMgr->GetCreatureAddon(run.boss_spawn_id))
                path_id = addon->PathId;
        if (path_id)
            if (WaypointPath const* path = sWaypointMgr->GetPath(path_id))
                patrol = path->Nodes;
    }
    // The distance from a point to the nearest leg of the patrol, the last node joined to the first (a loop, or a walk back along the same
    // line: Patchwerk's nodes 9-14 retrace 2-7).
    auto patrol_distance = [&patrol](float x, float y)
    {
        float best = 9999.f;
        for (std::size_t i = 0; i < patrol.size(); ++i)
        {
            WaypointNode const& a = patrol[i];
            WaypointNode const& b = patrol[(i + 1) % patrol.size()];
            float const dx = b.X - a.X, dy = b.Y - a.Y;
            float const len2 = dx * dx + dy * dy;
            float const t = len2 > 0.f ? std::clamp(((x - a.X) * dx + (y - a.Y) * dy) / len2, 0.f, 1.f) : 0.f;
            best = std::min(best, std::hypot(x - (a.X + t * dx), y - (a.Y + t * dy)));
        }
        return best;
    };
    // The route from a spot to the creature, for a spot out of his sight: the squad walks the last leg on the order, as it does to a
    // boss on patrol.
    auto walkable = [&](float cx, float cy, float h, float& length, float& straight)
    {
        PathGenerator pg(leader);
        ::Playerbot::BotMovement::SehSafeCalculatePathFrom(pg, cx, cy, h, run.boss_x, run.boss_y, run.boss_z);
        straight = std::hypot(run.boss_x - cx, run.boss_y - cy);
        length = pg.GetPathLength();
        return !(pg.GetPathType() & (PATHFIND_NOPATH | PATHFIND_FARFROMPOLY | PATHFIND_INCOMPLETE)) && length <= straight * kCheckpointPathSlack + 10.f;
    };
    // A boss that aggroes on sight (arrive_min_yards) is approached from outside its reach; the ring widens from there, the default one
    // (40 y) keeps its old order.
    std::vector<float> rings = { 40.f, 44.f, 48.f, 52.f, 56.f, 60.f, 64.f, 36.f, 32.f };
    if (run.def && run.def->arrive_min_yards > 40.f)
        rings = { run.def->arrive_min_yards, run.def->arrive_min_yards + 4.f, run.def->arrive_min_yards + 8.f, run.def->arrive_min_yards + 12.f,
                  run.def->arrive_min_yards - 4.f };
    else if (run.def && run.def->arrive_min_yards > 0.f)
        std::erase_if(rings, [&](float d) { return d < run.def->arrive_min_yards; });   // a boss that sees under 40 y (Skeram 28): no 32 or 36 y ring
    // The floor of the boss's own room lies within 6 y of his feet. A boss on a dais has his hall further down and nothing at his height
    // within the rings (aq40_skeram 2026-10-20: no floor within 32-64 y of Skeram's platform at 125.7): a second pass takes a floor up to
    // 20 y below or above him, still in sight of him. A third takes such a floor out of his sight when it has a walkable route to him
    // (aq40_skeram 124: 142 ring spots with no floor near his height, 127 with a floor and no sight of him on his dais; his script sees
    // 28 y, so a raid 36 y off is safe until the tank walks up on the order).
    for (int pass = 0; pass < 3; ++pass)
    {
    float const band = pass == 0 ? 6.0f : 20.0f;
    bool const need_los = pass < 2;
    for (float dist : rings)
        for (int k = 0; k < 16; ++k)
        {
            int const n = (k + 1) / 2 * (k % 2 ? 1 : -1);              // 0, 1, -1, 2, -2, ...
            float const a = run.boss_o + float(n) * step;
            float const cx = run.boss_x + std::cos(a) * dist;
            float const cy = run.boss_y + std::sin(a) * dist;
            float h = map->GetHeight(ps, cx, cy, run.boss_z + 3.0f, true, band + 6.0f);
            if (h <= INVALID_HEIGHT || std::fabs(h - run.boss_z) > band)
            {
                if (need_los)
                    ++height_rejected;
                continue;
            }
            if (pass == 1 && std::fabs(h - run.boss_z) <= 6.0f)
                continue;   // seen in the first pass
            // A floor under a lava or slime sheet is not a landing: the raid would burn from the first second (Ragnaros: the search starts 3 y
            // over the boss, whose feet are in the lava core, and finds the bed 1.4 y under the surface; six raiders died of it, MC 2026-10-13
            // d202b196). The model floor lying over the sheet is the place to stand; with none, or one that is wet too, the candidate goes.
            if (InMagmaContact(map, ps, cx, cy, h + 0.5f))
            {
                float const over = map->GetHeight(ps, cx, cy, h + 4.0f, true, 6.0f);
                if (over <= INVALID_HEIGHT || over <= h + 0.1f || InMagmaContact(map, ps, cx, cy, over + 0.5f))
                    continue;
                h = over;
            }
            if (!patrol.empty() && patrol_distance(cx, cy) < kPatrolClearance)
            {
                if (need_los)
                    ++patrol_rejected;
                continue;
            }
            float hd = 0.f;
            if (Creature const* c = NearestHostile(map, leader, run, cx, cy, h, hd); c && hd < clearance)
            {
                if (!nearest_blocker || hd < nearest_blocker_d) { nearest_blocker = c; nearest_blocker_d = hd; }
                continue;
            }
            bool const in_sight = map->isInLineOfSight(ps, cx, cy, h + 2.0f, run.boss_x, run.boss_y, run.boss_z + 2.0f, LINEOFSIGHT_ALL_CHECKS,
                                                       VMAP::ModelIgnoreFlags::Nothing);
            if (need_los && !in_sight)
            {
                ++los_rejected;
                continue;
            }
            if (!need_los && in_sight)
                continue;   // seen in the second pass
            float route = 0.f, straight = 0.f;
            if (run.checkpoint || !in_sight)
            {
                // The squad walks from here to the checkpoint, or to a boss out of sight: the spot must be on the route, not merely near it.
                if (!walkable(cx, cy, h, route, straight))
                {
                    ++path_rejected;
                    continue;
                }
            }
            run.arrive_x = cx; run.arrive_y = cy; run.arrive_z = h + 0.5f;
            run.arrive_o = std::atan2(run.boss_y - cy, run.boss_x - cx);
            if (run.arrive_o < 0.f)
                run.arrive_o += 6.2831853f;
            if (pass == 1)
                TC_LOG_INFO("playerbot.v2", "[scenario] {} landing floor {:.1f} y {} creature {} (no floor within 6 y of its height in the rings)",
                            run.def->name, std::fabs(h - run.boss_z), h < run.boss_z ? "below" : "above", run.arrive_entry);
            else if (pass == 2)
                TC_LOG_INFO("playerbot.v2", "[scenario] {} landing out of sight of creature {}: floor {:.1f} y {} it, route {:.0f} y for {:.0f} y straight "
                            "(no floor in its sight in the rings)", run.def->name, run.arrive_entry, std::fabs(h - run.boss_z),
                            h < run.boss_z ? "below" : "above", route, straight);
            if (!patrol.empty())
                TC_LOG_INFO("playerbot.v2", "[scenario] {} landing {:.0f} y clear of the {}-node patrol of creature {}", run.def->name,
                            patrol_distance(cx, cy), patrol.size(), run.arrive_entry);
            return true;
        }
    }
    if (nearest_blocker)
        run.arrive_blocker = fmt::format("; the nearest hostile to a candidate spot was entry {} {:.0f} y away (clearance {:.0f} y)", nearest_blocker->GetEntry(),
                                         nearest_blocker_d, clearance);
    if (path_rejected)
        run.arrive_blocker += fmt::format("; {} candidate spots had no walkable route to the creature", path_rejected);
    run.arrive_blocker += fmt::format("; {} candidate spots had no floor within 20 y of the creature's height, {} had no line of sight to it",
                                      height_rejected, los_rejected);
    if (patrol_rejected)
        run.arrive_blocker += fmt::format("; {} lay within {:.0f} y of its {}-node patrol", patrol_rejected, kPatrolClearance, patrol.size());
    return false;
}

Player* MainTank(Run& run)
{
    for (Member const& m : run.members)
        if (m.def.role == ScenarioRoleKind::Tank)
            if (Player* p = Find(m.id); p && p->IsAlive())
                return p;
    return nullptr;
}

Creature* FindBoss(Run& run)
{
    Player* viewer = MainTank(run);
    if (!viewer)
        viewer = Find(run.members.front().id);
    return viewer ? viewer->FindNearestCreature(run.def->goal_entry, 400.f, true) : nullptr;
}

Player* StandIn(Run& run)
{
    if (!run.def->stand_in)
        return nullptr;
    for (Member const& m : run.members)
        if (m.def.name == SquadName(run.def->stand_in, run.slot))
            return Find(m.id);
    return nullptr;
}

// Where a member stands and what moves it: on landing on the scenario's map, when the run begins, and 1 s and 3 s into it. The bots
// of rfc_oggleflint stood 4 to 11 y west of the start point when the run began (RC7, RC8: the healer off the mesh), in RC5 on it,
// and no line said what moved them.
void LogPlace(Run const& run, Player* p, char const* when)
{
    if (!p || !p->IsInWorld())
        return;
    std::string rule = "none";
    if (BotAI* ai = ::Playerbot::Services::Registry().ai(p->GetGUID().GetCounter()))
        if (char const* r = ai->last_rule_fired())
            rule = r;
    MotionMaster* mm = p->GetMotionMaster();
    std::string dest = "-";
    if (p->movespline && !p->movespline->Finalized())
    {
        G3D::Vector3 const& d = p->movespline->FinalDestination();
        dest = fmt::format("({:.1f},{:.1f},{:.1f})", d.x, d.y, d.z);
    }
    TC_LOG_INFO("playerbot.v2", "[scenario] {} place {} {}: ({:.1f},{:.1f},{:.1f}) map {} {:.1f} y from the start, rule {}, generator {}, "
                "spline to {}, combat {}, stand {}", run.def->name, when, p->GetName(), p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(),
                p->GetMapId(), p->GetMapId() == run.def->map ? p->GetExactDist2d(run.def->x, run.def->y) : -1.f, rule,
                mm ? uint32(mm->GetCurrentMovementGeneratorType()) : 0u, dest, p->IsInCombat() ? 1 : 0, uint32(p->GetStandState()));
}

// One line per member when the run begins: class, level, armour, health, mana, whether an off-hand shield is worn, and the equipped
// item entries by slot. A tank that takes too much damage is a gear question first (deadmines_full run 11).
void LogGear(Run const& run)
{
    for (Member const& m : run.members)
    {
        Player* p = Find(m.id);
        if (!p)
            continue;
        std::string items;
        bool shield = false;
        for (std::uint8_t slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            if (Item* it = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            {
                items += fmt::format(" {}:{}", uint32(slot), it->GetEntry());
                if (slot == EQUIPMENT_SLOT_OFFHAND)
                    if (ItemTemplate const* t = it->GetTemplate())
                        shield = t->GetClass() == ITEM_CLASS_ARMOR && t->GetSubClass() == ITEM_SUBCLASS_ARMOR_SHIELD;
            }
        TC_LOG_INFO("playerbot.v2", "[scenario] gear {} class={} level={} armor={} hp={} mana={} shield={} items{}", p->GetName(),
                    uint32(p->GetClass()), uint32(p->GetLevel()), uint32(p->GetArmor()), p->GetMaxHealth(), p->GetMaxPower(POWER_MANA),
                    shield ? "yes" : "no", items);
    }
}

// What each raider carries into the pull: its health and mana, its resistances, and the buffs on it (spell ids: flasks, elixirs, food,
// class buffs, blessings, auras), so a wipe can be read against them (naxx_patchwerk 124: no way to tell what the tanks had drunk).
void LogBuffs(Run const& run)
{
    for (Member const& m : run.members)
    {
        Player* p = Find(m.id);
        if (!p)
            continue;
        std::string auras;
        for (auto const& [id, app] : p->GetAppliedAuras())
            if (app->IsPositive() && app->GetBase() && !app->GetBase()->IsPassive())
                auras += fmt::format("{}{}", auras.empty() ? "" : " ", id);
        TC_LOG_INFO("playerbot.v2", "[scenario] buffs {} hp={}/{} mana={}/{} resist fire={} nature={} frost={} shadow={} arcane={} auras [{}]",
                    p->GetName(), p->GetHealth(), p->GetMaxHealth(), p->GetPower(POWER_MANA), p->GetMaxPower(POWER_MANA),
                    p->GetResistance(SPELL_SCHOOL_FIRE), p->GetResistance(SPELL_SCHOOL_NATURE), p->GetResistance(SPELL_SCHOOL_FROST),
                    p->GetResistance(SPELL_SCHOOL_SHADOW), p->GetResistance(SPELL_SCHOOL_ARCANE), auras);
    }
}

// The role kits (ClassicSupplies RaidConsumablesFor: flask, elixirs, stat food, the fight's resistance potion) into every raider's bags at
// once, inside the instance, as a raid arrives with them. idle:classic_raid_consumables stocks one item per attempt (30 s apart) when
// Playerbot.Raid.ProvideConsumables allows and drinks from the bags; the 20 s before the order never covered that. The rule still does the
// drinking, and the attack waits for it (PrepOwed). The role is the rule's own (RaidConsumableRoleOf, from the bot's latest view).
void StockRaidKits(Run& run)
{
    if (run.kits_stocked)
        return;
    run.kits_stocked = true;
    std::uint32_t items = 0, raiders = 0, no_view = 0;
    for (Member const& m : run.members)
    {
        Player* p = Find(m.id);
        if (!p || !p->IsAlive())
            continue;
        auto const snap = ::Playerbot::Services::Snapshots().latest(m.id);
        auto const gsnap = ::Playerbot::Services::Snapshots().latest_group(m.id);
        if (!snap || !gsnap)
        {
            ++no_view;
            continue;
        }
        Classic::RaidConsumableRole const role = Classic::RaidConsumableRoleOf(BotSnapshotView(*snap), GroupSnapshotView(*gsnap));
        // Tranquilizing Shot is a book (a Molten Core drop) every raiding hunter of the tier has read; no trainer teaches it, so a
        // scenario hunter never had it and the Frenzy checks (min_tranq_shots) could not pass: 0 shots in every bwl_chromaggus run.
        constexpr std::uint32_t kTranquilizingShot = 19801;
        if (p->GetClass() == CLASS_HUNTER && p->GetLevel() >= 60 && !p->HasSpell(kTranquilizingShot))
        {
            p->LearnSpell(kTranquilizingShot, false, 0, /*suppressMessaging*/ true);
            TC_LOG_INFO("playerbot.v2", "[scenario] {} reads the Tranquilizing Shot book ({})", p->GetName(),
                p->HasSpell(kTranquilizingShot) ? "learned" : "NOT learned: spell missing");
        }
        ::Playerbot::API api(p);
        bool any = false;
        for (Classic::RaidConsumable const& c : Classic::RaidConsumablesFor(role))
            if (api.stock_raid_consumable(c.item) == ::Playerbot::Result::Ok)
            {
                ++items;
                any = true;
            }
        raiders += any ? 1u : 0u;
    }
    TC_LOG_INFO("playerbot.v2", "[scenario] {} raid kits stocked: {} items for {} raiders{}", run.def->name, items, raiders,
                no_view ? fmt::format(" ({} without a bot view yet)", no_view) : std::string());
}

// The raiders that still owe a piece of the pre-pull preparation the upkeep rules do at the landing: a class buff or a blessing to cast
// (idle:classic_raid_buffs, idle:classic_blessings), a kit item to drink or eat, or one to stock (idle:classic_raid_consumables). Read from
// the bots' latest views with the rules' own pickers. `detail` names the first few.
std::uint32_t PrepOwed(Run const& run, std::string& detail)
{
    std::uint32_t n = 0;
    for (Member const& m : run.members)
    {
        Player* p = Find(m.id);
        if (!p || !p->IsAlive())
            continue;
        auto const snap = ::Playerbot::Services::Snapshots().latest(m.id);
        auto const gsnap = ::Playerbot::Services::Snapshots().latest_group(m.id);
        if (!snap || !gsnap)
            continue;
        BotSnapshotView const s(*snap);
        GroupSnapshotView const g(*gsnap);
        char const* what = nullptr;
        if (Classic::PickClassBuff(s, g).spell)
            what = "a class buff to cast";
        else if (Classic::PickBlessing(s, g, false).spell)
            what = "a blessing to cast";
        else if (Classic::PickRaidConsumable(s, g))
            what = "a consumable to take";
        else if (Classic::PickRaidConsumableStock(s, g))
            what = "a kit item to stock";
        if (!what)
            continue;
        ++n;
        if (n <= 6)
            detail += fmt::format("{}{} {}", detail.empty() ? "" : ", ", p->GetName(), what);
    }
    if (n > 6)
        detail += ", ...";
    return n;
}

// The stand-in stops being a bot: the bot logic that would route, pull and manage threat is taken off it (what `.playerbot self off` does),
// and PlayStandIn drives it. Its identity mark stays, so the group logic still sees a bot-led group and the tank's autopilot runs.
void DetachStandIn(Run& run)
{
    Player* p = StandIn(run);
    if (!p || run.stand_in_detached)
        return;
    ::Playerbot::BotId const id = p->GetGUID().GetCounter();
    if (::Playerbot::Services::Registry().has(id))
    {
        ::Playerbot::Services::Scheduler().unregister_bot(id);
        ::Playerbot::Services::Snapshots().remove(id);
        ::Playerbot::Services::Registry().unregister_bot(id);
    }
    run.stand_in_detached = true;
    TC_LOG_INFO("playerbot.v2", "[scenario] {} plays {} like a player: its logic is detached, it follows the tank and attacks the tank's target", run.def->name, p->GetName());
}

// One second of a player who only follows the main tank and attacks what the tank attacks (or what is attacking him).
void PlayStandIn(Run& run)
{
    Player* p = StandIn(run);
    Player* tank = MainTank(run);
    if (!p || !run.stand_in_detached || run.def->player_lead || !p->IsAlive() || !tank || tank == p || p->HasUnitState(UNIT_STATE_CASTING))
        return;
    Unit* target = tank->IsInCombat() ? tank->GetVictim() : nullptr;
    if (!target || !target->IsAlive() || !p->IsValidAttackTarget(target))
    {
        target = nullptr;
        if (p->IsInCombat())
            for (Unit* a : p->getAttackers())
                if (a && a->IsAlive())
                {
                    target = a;
                    break;
                }
    }
    if (target)
    {
        if (p->GetVictim() != target)
        {
            p->Attack(target, true);
            p->GetMotionMaster()->MoveChase(target);
        }
        return;
    }
    if (p->GetVictim())
        p->AttackStop();
    if (p->GetDistance(tank) > 10.f || p->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
        if (p->GetDistance(tank) > 6.f)
            p->GetMotionMaster()->MoveFollow(tank, 3.0f);
}

// ---- A player-led run (ScenarioDef::player_lead): the stand-in leads like the owner ----

// Ready to go on: everyone alive and close, health 70 % and up, mana 60 % and up (the healer's 75 %). A player waits for that, but not
// for ever: after kLeadReadyWaitMs he goes on anyway, as the owner does.
constexpr std::uint32_t kLeadReadyWaitMs = 60000;
constexpr float kLeadPullYd = 30.f;        // a pack this close is pulled before he takes another step
constexpr float kLeadGroupYd = 30.f;       // he waits for a member farther back than this
constexpr std::uint32_t kLeadPullGiveUpMs = 15000;   // a pull order that started no fight: the mob is passed by
constexpr float kLeadArriveYd = 12.f;                  // the pulled mob this close to him: the pull is back, he says "attack"
constexpr std::uint32_t kLeadArriveWaitMs = 12000;    // ... or this long after the pull order, wherever the fight is
constexpr float kLeadBossNearYd = 60.f;               // the run's boss this close: he goes to him before the next route point

void LeadLog(Run& run, std::string const& line)
{
    TC_LOG_INFO("playerbot.v2", "[scenario_lead] {} t={}s {}", run.def->name, run.run_ms / 1000u, line);
    run.lead_log_ms = run.run_ms;
}

// ScenarioDef::yes_fear / focus_heal_tank: the owner's orders a bot-only squad has nobody to give, put on at the start of the run and
// taken back at its end (the squad characters are reused by the next scenario).
void GiveSquadOrders(Run& run, bool on)
{
    ScenarioDef const& def = *run.def;
    if (!def.yes_fear && !def.focus_heal_tank)
        return;
    std::uint64_t tank = 0;
    for (Member const& m : run.members)
        if (m.def.role == ScenarioRoleKind::Tank) { tank = m.id; break; }
    for (Member const& m : run.members)
    {
        if (def.yes_fear)
            ::Playerbot::Scenario::MarkYesFear(m.id, on);
        if (def.focus_heal_tank && m.def.role == ScenarioRoleKind::Healer && tank)
            if (BotAI* ai = ::Playerbot::Services::Registry().ai(m.id))
                ai->set_heal_only(on ? std::vector<ObjectGuid>{ ObjectGuid::Create<HighGuid::Player>(tank) } : std::vector<ObjectGuid>{});
    }
    TC_LOG_INFO("playerbot.v2", "[scenario] {} squad orders {}:{}{}", def.name, on ? "given" : "taken back",
        def.yes_fear ? " yes fear (Fear and Scare Beast allowed)" : "", def.focus_heal_tank ? " focus heal on the tank" : "");
}

// The stand-in becomes the group leader and a player for the squad's logic. Called once, after DetachStandIn.
void TakeLead(Run& run)
{
    Player* p = StandIn(run);
    if (!p || !run.def->player_lead || !run.stand_in_detached)
        return;
    ::Playerbot::Scenario::MarkHuman(p->GetGUID().GetCounter(), true);
    if (Group* g = p->GetGroup(); g && g->GetLeaderGUID() != p->GetGUID())
        g->ChangeLeader(p->GetGUID());
    run.lead_route = ::Playerbot::Services::Dungeons().RouteFor(p->GetMapId(), uint32(p->GetMap()->GetDifficultyID()));
    // The route point nearest the entrance is where he starts.
    run.lead_wp = 0;
    float best = 1e9f;
    for (std::size_t i = 0; i < run.lead_route.size(); ++i)
    {
        auto const& w = run.lead_route[i];
        float const d = p->GetExactDist(w.x, w.y, w.z);
        if (d < best) { best = d; run.lead_wp = i; }
    }
    LeadLog(run, fmt::format("{} leads the group like a player: {} route points, starting at point {}", p->GetName(), run.lead_route.size(),
                             run.lead_wp));
}

void DropLead(Run& run)
{
    if (!run.def->player_lead)
        return;
    for (Member const& m : run.members)
        ::Playerbot::Scenario::MarkHuman(m.id, false);
}

// The nearest mob he would have to fight to walk on: alive, hostile, not fighting, in sight, within kLeadPullYd.
Creature* NextPack(Run& run, Player* p)
{
    std::list<Creature*> near;
    p->GetCreatureListWithOptionsInGrid(near, kLeadPullYd, FindCreatureOptions());
    Creature* best = nullptr;
    float best_d = 1e9f;
    for (Creature* c : near)
    {
        if (!c || !c->IsAlive() || c->IsInCombat() || !p->IsValidAttackTarget(c) || c->IsCritter() || c->IsCivilian() ||
            !c->IsHostileTo(p) || run.lead_skipped.count(c->GetGUID()) || !p->IsWithinLOSInMap(c))
            continue;
        float const d = p->GetDistance(c);
        if (d < best_d) { best_d = d; best = c; }
    }
    return best;
}

// What holds him before he goes on: a member dead, far back, or low. Empty when the group is ready.
std::string LeadNotReady(Run& run, Player* p)
{
    for (Member const& m : run.members)
    {
        Player* q = Find(m.id);
        if (!q || q == p)
            continue;
        if (!q->IsAlive())
            return q->GetName() + " is dead";
        if (q->GetMapId() != p->GetMapId() || q->GetDistance(p) > kLeadGroupYd)
            return q->GetName() + " is far back";
        if (q->GetHealthPct() < 70.f)
            return fmt::format("{} has {:.0f} % health", q->GetName(), q->GetHealthPct());
        if (q->GetMaxPower(POWER_MANA) > 0)
        {
            float const mana = 100.f * float(q->GetPower(POWER_MANA)) / float(q->GetMaxPower(POWER_MANA));
            float const need = m.def.role == ScenarioRoleKind::Healer ? 75.f : 60.f;
            if (mana < need)
                return fmt::format("{} has {:.0f} % mana", q->GetName(), mana);
        }
    }
    return {};
}

// The owner's skull in a fight whose script lets the DPS hit only his skull (Script::Fight::skullOnly, Majordomo). In the owner's group he
// rotates the skull himself; a bot-only squad has nobody to do it, and with no skull the DPS stand by design: release-0130 mc_majordomo
// 8304ad94, every mage, warlock and hunter did 0 damage in the first 90 s (raid:drop_target / raid:switch_target, a new pick and a drop
// every tick) while the healers ran dry, wipe at 476 s. Once the attack is ordered the runner plays the owner: the skull goes on an
// engaged, living enemy of the fight, first entry of the kill order first (the Healers, then the Elites), the most hurt one in it, and
// stays there until that enemy dies. Nothing else of the owner's plan is played (no sheep icons): the bot logic is not touched.
void PlayOwnerSkull(Run& run)
{
    constexpr uint8 kSkull = 7;
    constexpr float kFightReach = 120.f;   // the room: the fight's enemies within this of the main tank
    if (!run.order_sent || run.run_ms < run.owner_skull_next_ms)
        return;
    run.owner_skull_next_ms = run.run_ms + 1000u;
    ::Playerbot::Raid::Script::Fight const* fight = ::Playerbot::Raid::Script::FightFor(run.def->map, run.def->goal_entry);
    if (!fight || !fight->skullOnly)
        return;
    Player* lead = MainTank(run);
    if (!lead)
        for (Member const& m : run.members)
            if (Player* p = Find(m.id); p && p->IsAlive())
            {
                lead = p;
                break;
            }
    Group* group = lead ? lead->GetGroup() : nullptr;
    if (!group || !lead->GetMap())
        return;
    ObjectGuid const current = group->GetTargetIcon(kSkull);
    if (!current.IsEmpty())
        if (Creature const* c = ObjectAccessor::GetCreature(*lead, current); c && c->IsAlive())
            return;
    Creature* best = nullptr;
    std::size_t bestRank = 99;
    for (auto const& [guid, c] : lead->GetMap()->GetObjectsStore().Data.FindContainer<Creature>())
    {
        if (!c || !c->IsAlive() || !c->IsInCombat() || c->IsInEvadeMode() || !c->IsHostileTo(lead) || !lead->IsWithinDist(c, kFightReach))
            continue;
        std::size_t const rank = ::Playerbot::Raid::Script::RankOf(*fight, c->GetEntry());
        if (rank == 99)
            continue;
        if (!best || rank < bestRank || (rank == bestRank && (c->GetHealthPct() < best->GetHealthPct() ||
            (c->GetHealthPct() == best->GetHealthPct() && c->GetGUID().GetCounter() < best->GetGUID().GetCounter()))))
        {
            best = c;
            bestRank = rank;
        }
    }
    if (!best || best->GetGUID() == run.owner_skull)
        return;
    group->SetTargetIcon(kSkull, best->GetGUID(), lead->GetGUID());
    run.owner_skull = best->GetGUID();
    TC_LOG_INFO("playerbot.v2", "[scenario] {} the owner's skull goes on {} ({}, {}) at {:.0f} % health, {} s into the run", run.def->name,
                best->GetName(), best->GetEntry(), best->GetGUID().GetCounter(), best->GetHealthPct(), run.run_ms / 1000u);
}

// One second of the owner leading his bots through a dungeon.
void PlayLeader(Run& run)
{
    Player* p = StandIn(run);
    if (!p || !run.def->player_lead || !run.stand_in_detached || !p->IsAlive() || p->HasUnitState(UNIT_STATE_CASTING))
        return;
    Group* g = p->GetGroup();
    if (!g)
        return;
    Player* tank = MainTank(run);

    bool fighting = false;
    for (Member const& m : run.members)
        if (Player* q = Find(m.id); q && q->IsAlive() && q->IsInCombat())
            fighting = true;

    if (fighting)
    {
        run.lead_wait_since_ms = 0;
        Unit* target = tank && tank->IsInCombat() ? tank->GetVictim() : nullptr;
        if (!target || !target->IsAlive() || !p->IsValidAttackTarget(target))
        {
            target = nullptr;
            for (Unit* a : p->getAttackers())
                if (a && a->IsAlive()) { target = a; break; }
        }
        // "attack" once the pull has come back to him, as a player says it: the tank's opener landing 20-40 y out is not that, and
        // the leader running in on it took the damage dealers with him (rfc_oggleflint_led RC2: 14 mobs). A mob on him, a group
        // without a tank, or a pull that never comes back (the tank fights where it is) start it too.
        bool const arrived = target && (!tank || p->GetExactDist(target) <= kLeadArriveYd ||
                                        target->GetVictim() == p ||
                                        run.run_ms >= run.lead_pull_ms + kLeadArriveWaitMs);
        if (!run.lead_attack_sent && arrived)
        {
            p->SetSelection(target->GetGUID());
            ::Playerbot::V2::Module::instance().RouteGroupOrder(p, g, "attack");
            run.lead_attack_sent = true;
            LeadLog(run, fmt::format("says attack on {} (entry {}, {:.0f} y away)", target->GetName(), target->GetEntry(), p->GetExactDist(target)));
        }
        if (!run.lead_attack_sent)
            return;   // he waits at his spot for the pull
        if (target && p->GetVictim() != target)
        {
            p->Attack(target, true);
            p->GetMotionMaster()->MoveChase(target);
        }
        return;
    }

    if (p->GetVictim())
        p->AttackStop();
    run.lead_attack_sent = false;

    // A pull that is under way: wait for the fight it starts, or give the mob up.
    if (!run.lead_pull.IsEmpty())
    {
        Creature* c = ObjectAccessor::GetCreature(*p, run.lead_pull);
        if (c && c->IsAlive() && run.run_ms < run.lead_pull_ms + kLeadPullGiveUpMs)
            return;
        if (c && c->IsAlive())
        {
            run.lead_skipped.insert(run.lead_pull);
            LeadLog(run, fmt::format("gives up on {} (entry {}): no fight {} s after the pull", c->GetName(), c->GetEntry(), kLeadPullGiveUpMs / 1000u));
        }
        run.lead_pull.Clear();
    }

    // Ready before the next pull or step; bounded.
    std::string const waiting = LeadNotReady(run, p);
    if (!waiting.empty())
    {
        if (run.lead_wait_since_ms == 0)
            run.lead_wait_since_ms = run.run_ms ? run.run_ms : 1u;
        if (run.run_ms < run.lead_wait_since_ms + kLeadReadyWaitMs)
        {
            p->StopMoving();
            if (run.run_ms >= run.lead_log_ms + 10000u)
                LeadLog(run, "waits: " + waiting);
            return;
        }
        if (run.run_ms >= run.lead_log_ms + 10000u)
            LeadLog(run, "goes on after a minute's wait: " + waiting);
    }
    else
        run.lead_wait_since_ms = 0;

    if (Creature* c = NextPack(run, p))
    {
        p->StopMoving();
        p->SetSelection(c->GetGUID());
        ::Playerbot::V2::Module::instance().RouteGroupOrder(p, g, "pull back");
        run.lead_pull = c->GetGUID();
        run.lead_pull_ms = run.run_ms;
        LeadLog(run, fmt::format("says pull back on {} (entry {}) at {:.0f} y", c->GetName(), c->GetEntry(), p->GetDistance(c)));
        return;
    }

    // The run's boss, once he is near or the route is done: a player who knows the dungeon goes to him. The route is the bot-led clear's
    // corridor, not a visit to every boss (rfc_oggleflint_led 0.1.27 set B: it passes Oggleflint 31 y away and 20 y above his cave, so
    // no pull ever reached him; the leader walked on to the route's end and stood there for two minutes). Walking at him brings him
    // inside kLeadPullYd in sight, and NextPack above has the tank pull him and what stands with him.
    // Once he has set out for the boss he keeps going to him: the way down can lead away first, more than kLeadBossNearYd from him
    // (b3ca0b4b67 rfc_oggleflint_led: from 45 y above Oggleflint's cave the path went back west and down into the pit; 120 y out he
    // went back to the route, which brought him east to 45 y again, and so on from 690 s until the time limit at 2400 s).
    bool const route_done = run.lead_wp >= run.lead_route.size();
    Creature* boss = nullptr;
    if (!run.lead_boss.IsEmpty())
    {
        boss = ObjectAccessor::GetCreature(*p, run.lead_boss);
        if (!boss || !boss->IsAlive())
        {
            boss = nullptr;
            run.lead_boss.Clear();
        }
    }
    if (!boss && run.def->goal == ScenarioGoalKind::KillCreature && run.def->goal_entry)
        boss = p->FindNearestCreature(run.def->goal_entry, route_done ? 400.f : kLeadBossNearYd, true);
    if (boss)
    {
        run.lead_boss = boss->GetGUID();
        if (p->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE || !p->isMoving())
        {
            p->GetMotionMaster()->MovePoint(0, boss->GetPositionX(), boss->GetPositionY(), boss->GetPositionZ());
            if (run.run_ms >= run.lead_log_ms + 20000u)
                LeadLog(run, fmt::format("walks to {} (entry {}, {:.0f} y away){}", boss->GetName(), boss->GetEntry(), p->GetExactDist(boss),
                                         route_done ? ", the route is done" : ""));
        }
        return;
    }

    // Walk on along the route.
    if (route_done)
    {
        if (run.run_ms >= run.lead_log_ms + 30000u)
            LeadLog(run, "is at the end of the route");
        return;
    }
    auto const& w = run.lead_route[run.lead_wp];
    if (p->GetExactDist(w.x, w.y, w.z) < 4.f)
    {
        ++run.lead_wp;
        return;
    }
    if (p->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE || !p->isMoving())
    {
        p->GetMotionMaster()->MovePoint(0, w.x, w.y, w.z);
        if (run.run_ms >= run.lead_log_ms + 20000u)
            LeadLog(run, fmt::format("walks to route point {} of {}", run.lead_wp, run.lead_route.size()));
    }
}

// ---- A FollowRoute run: the stand-in walks the route like a player, the squad must keep up ----
constexpr float kRouteArriveYd = 5.f;              // he is at a point this close to it
constexpr float kRouteGatherYd = 30.f;             // a member this close to him (3D) is with him
constexpr std::uint32_t kRouteStallMs = 20000;     // no 2 y closer to the point in this long: he takes the jump (is set down on it)
constexpr std::uint32_t kRouteGatherMs = 40000;    // every member must be with him this long after he reaches a point

// A detached stand-in is played like a player, and a player's client swims him. He has left the bot registry, so the module's world
// tick (UpdateSwimState for every headless bot) no longer sees him: the runner does it (lake_swim 2026-11-09: Scnrogue stood on top
// of the lake without the swimming flag while his squad swam around him).
void SwimStandIn(Run& run)
{
    if (!run.stand_in_detached)
        return;
    if (Player* p = StandIn(run); p && p->IsInWorld())
        ::Playerbot::BotMovement::UpdateSwimState(p);
}

// The nearest deep water (BotMovement::SwimLevel) around (cx, cy) the stand-in can walk to: rings 8 y apart out to kWaterSearchYd.
constexpr float kWaterSearchYd = 150.f;
bool FindDeepWater(Player* p, float cx, float cy, float& ox, float& oy, float& oz)
{
    Map* m = p->GetMap();
    if (!m)
        return false;
    for (float r = 0.f; r <= kWaterSearchYd; r += 8.f)
    {
        int const steps = r < 1.f ? 1 : 16;
        for (int i = 0; i < steps; ++i)
        {
            float const a = float(i) * 6.2831853f / float(steps);
            float const x = cx + r * std::cos(a), y = cy + r * std::sin(a);
            m->LoadGrid(x, y);
            float const level = m->GetWaterLevel(p->GetPhaseShift(), x, y);
            if (level <= INVALID_HEIGHT)
                continue;
            float swim_z = 0.f;
            if (!::Playerbot::BotMovement::SwimLevel(p, x, y, level, swim_z))
                continue;
            PathGenerator pg(p);
            if (!::Playerbot::BotMovement::SehSafeCalculatePath(pg, x, y, swim_z) || (pg.GetPathType() & (PATHFIND_NOPATH | PATHFIND_SHORTCUT)))
                continue;
            ox = x; oy = y; oz = swim_z;
            return true;
        }
    }
    return false;
}

void PlayRoute(Run& run)
{
    Player* p = StandIn(run);
    if (!p || run.def->goal != ScenarioGoalKind::FollowRoute || !run.stand_in_detached || !p->IsAlive())
        return;
    std::vector<ScenarioRoutePoint> const& route = run.def->route;
    if (run.route_leg >= route.size())
    {
        run.goal_reached = true;
        return;
    }
    ScenarioRoutePoint w = route[run.route_leg];
    if (!run.leg_start_ms)
    {
        run.leg_water = false;
        run.leg_hold_since_ms = 0;
        if (w.water)
        {
            if (!FindDeepWater(p, w.x, w.y, run.leg_wx, run.leg_wy, run.leg_wz))
            {
                run.monitor->NoteSetupFailure(run.run_ms, p->GetName(), fmt::format("no deep water he can walk to within {:.0f} y of {} ({:.0f},{:.0f})",
                                                                                     kWaterSearchYd, w.label, w.x, w.y));
                return;
            }
            run.leg_water = true;
        }
    }
    if (run.leg_water)
    {
        w.x = run.leg_wx; w.y = run.leg_wy; w.z = run.leg_wz;
    }
    if (!run.leg_start_ms)
    {
        run.leg_start_ms = run.run_ms ? run.run_ms : 1u;
        run.leg_at_ms = 0;
        run.leg_progress_ms = run.leg_start_ms;
        run.leg_best = p->GetExactDist(w.x, w.y, w.z);
        run.leg_leapt = false;
        run.leg_joined_ms.clear();
        run.leg_catch_ups.clear();
        for (Member const& m : run.members)
            run.leg_catch_ups[m.id] = ::Playerbot::V2::Fleet::CatchUpCount(m.id);
        // A walking route there, or the jump only a player can take.
        PathGenerator pg(p);
        ::Playerbot::BotMovement::SehSafeCalculatePath(pg, w.x, w.y, w.z);
        G3D::Vector3 const& end = pg.GetActualEndPosition();
        float const end_gap = std::sqrt((end.x - w.x) * (end.x - w.x) + (end.y - w.y) * (end.y - w.y) + (end.z - w.z) * (end.z - w.z));
        bool const walkable = !(pg.GetPathType() & (PATHFIND_NOPATH | PATHFIND_SHORTCUT)) && end_gap <= 4.f;
        LeadLog(run, fmt::format("leg {} of {}: walks to {} ({:.0f},{:.0f},{:.0f}), {:.0f} y; path type {} ends {:.1f} y from it{}", run.route_leg + 1,
                                 route.size(), w.label, w.x, w.y, w.z, run.leg_best, uint32(pg.GetPathType()), end_gap,
                                 walkable ? "" : ": no walking route, he takes the jump"));
        if (!walkable)
        {
            p->NearTeleportTo(w.x, w.y, w.z, p->GetOrientation());
            run.leg_leapt = true;
            return;
        }
    }

    // A fight on the way is the group's: he fights what is on him and waits for the rest.
    bool fighting = false;
    for (Member const& m : run.members)
        if (Player* q = Find(m.id); q && q->IsAlive() && q->IsInCombat())
            fighting = true;
    if (fighting)
    {
        run.leg_progress_ms = run.run_ms;
        for (Unit* a : p->getAttackers())
            if (a && a->IsAlive())
            {
                if (p->GetVictim() != a)
                {
                    p->Attack(a, true);
                    p->GetMotionMaster()->MoveChase(a);
                }
                return;
            }
        p->StopMoving();
        return;
    }
    if (p->GetVictim())
        p->AttackStop();

    if (!run.leg_at_ms)
    {
        float const d = p->GetExactDist(w.x, w.y, w.z);
        if (d < kRouteArriveYd)
        {
            run.leg_at_ms = run.run_ms ? run.run_ms : 1u;
            p->StopMoving();
            LeadLog(run, fmt::format("is at {} after {} s ({})", w.label, (run.run_ms - run.leg_start_ms) / 1000u,
                                     run.leg_leapt ? "set down: the jump" : "walked"));
            return;
        }
        if (d < run.leg_best - 2.f)
        {
            run.leg_best = d;
            run.leg_progress_ms = run.run_ms;
        }
        if (run.run_ms - run.leg_progress_ms >= kRouteStallMs)
        {
            LeadLog(run, fmt::format("no closer to {} for {} s at ({:.0f},{:.0f},{:.0f}), {:.0f} y off: he takes the jump", w.label, kRouteStallMs / 1000u,
                                     p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), d));
            p->NearTeleportTo(w.x, w.y, w.z, p->GetOrientation());
            run.leg_leapt = true;
            run.leg_progress_ms = run.run_ms;
            return;
        }
        if (p->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE || !p->isMoving())
            p->GetMotionMaster()->MovePoint(0, w.x, w.y, w.z);
        return;
    }

    // At the point: the squad gathers on him, walking or brought.
    std::string missing;
    Player* farthest = nullptr;
    float farthest_d = 0.f;
    for (Member const& m : run.members)
    {
        Player* q = Find(m.id);
        if (!q || q == p)
            continue;
        bool const with = q->IsAlive() && q->GetMap() == p->GetMap() && q->GetExactDist(p) <= kRouteGatherYd;
        if (with)
        {
            run.leg_joined_ms.emplace(m.id, run.run_ms);
            continue;
        }
        float const d = q->GetMap() == p->GetMap() ? q->GetExactDist(p) : 99999.f;
        BotAI* ai = ::Playerbot::Services::Registry().ai(m.id);
        missing += fmt::format("{}{} {:.0f} y at ({:.0f},{:.0f},{:.0f}) rule={}", missing.empty() ? "" : "; ", m.def.name, d, q->GetPositionX(),
                               q->GetPositionY(), q->GetPositionZ(), ai && ai->last_rule_fired() ? ai->last_rule_fired() : "-");
        if (!farthest || d > farthest_d) { farthest = q; farthest_d = d; }
    }
    if (missing.empty() && w.hold_s)
    {
        // A point he holds: the squad stays with him there (in the lake: longer than a bot's breath) before he goes on.
        if (!run.leg_hold_since_ms)
        {
            run.leg_hold_since_ms = run.run_ms ? run.run_ms : 1u;
            LeadLog(run, fmt::format("holds at {} for {} s", w.label, w.hold_s));
        }
        if (run.run_ms - run.leg_hold_since_ms < w.hold_s * 1000u)
            return;
    }
    if (missing.empty())
    {
        std::string walked, brought;
        for (Member const& m : run.members)
        {
            if (Find(m.id) == p)
                continue;
            std::uint32_t const joined = run.leg_joined_ms.count(m.id) ? run.leg_joined_ms[m.id] : run.run_ms;
            std::uint32_t const behind = joined > run.leg_at_ms ? (joined - run.leg_at_ms) / 1000u : 0u;
            bool const was_brought = ::Playerbot::V2::Fleet::CatchUpCount(m.id) != run.leg_catch_ups[m.id];
            (was_brought ? brought : walked) += fmt::format("{}{} (+{} s)", (was_brought ? brought : walked).empty() ? "" : ", ", m.def.name, behind);
        }
        LeadLog(run, fmt::format("the squad is with him at {}, {} s after he got there; walked: {}; brought (catch-up): {}", w.label,
                                 (run.run_ms - run.leg_at_ms) / 1000u, walked.empty() ? "-" : walked, brought.empty() ? "-" : brought));
        ++run.route_leg;
        run.leg_start_ms = 0;
        return;
    }
    if (run.run_ms - run.leg_at_ms >= kRouteGatherMs)
    {
        run.monitor->NoteFollowLost(run.run_ms, farthest ? farthest->GetName() : std::string(),
                                    fmt::format("not with {} at {} {} s after he got there ({}): {}", p->GetName(), w.label, kRouteGatherMs / 1000u,
                                                run.leg_leapt ? "he took the jump" : "he walked", missing));
        return;
    }
    if (run.run_ms >= run.lead_log_ms + 10000u)
        LeadLog(run, "waits at " + std::string(w.label) + " for: " + missing);
}

// What the owner's Attack does in a raid: the main tank is ordered onto the boss (an Engage order, the same one `attack` whispers).
// ---- The boss pull's statistics ----
constexpr std::uint32_t kPullReportAfterMs = 90u * 1000u;

char const* ClassName(std::uint8_t cls)
{
    switch (cls)
    {
        case ScnClass::Warrior: return "warrior";
        case ScnClass::Paladin: return "paladin";
        case ScnClass::Hunter:  return "hunter";
        case ScnClass::Rogue:   return "rogue";
        case ScnClass::Priest:  return "priest";
        case ScnClass::Shaman:  return "shaman";
        case ScnClass::Mage:    return "mage";
        case ScnClass::Warlock: return "warlock";
        case ScnClass::Druid:   return "druid";
        default:                return "?";
    }
}

char const* RoleName(ScenarioRoleKind r)
{
    return r == ScenarioRoleKind::Tank ? "tank" : r == ScenarioRoleKind::Healer ? "healer" : "dps";
}

void ReportPull(Run& run, char const* when)
{
    if (!run.pull_started.load())
        return;
    std::uint32_t const secs = (run.run_ms - run.pull_start_ms) / 1000u;
    std::uint64_t total = 0, by_role[3] = { 0, 0, 0 };
    for (Member const& m : run.members)
    {
        auto const it = run.pull.find(m.id);
        if (it == run.pull.end())
            continue;
        std::uint64_t const d = it->second.damage.load();
        total += d;
        by_role[m.def.role == ScenarioRoleKind::Tank ? 0 : m.def.role == ScenarioRoleKind::Healer ? 1 : 2] += d;
    }
    TC_LOG_INFO("playerbot.v2", "[pull_stats] {} {} after {} s of the boss fight: raid damage {} ({}/s): tanks {}, healers {}, dps {} ({}/s per dps bot)",
                run.def->name, when, secs, total, secs ? total / secs : 0u, by_role[0], by_role[1], by_role[2],
                secs ? by_role[2] / secs / std::max<std::size_t>(1, std::count_if(run.members.begin(), run.members.end(),
                    [](Member const& m) { return m.def.role == ScenarioRoleKind::Dps; })) : 0u);
    for (Member const& m : run.members)
    {
        auto const it = run.pull.find(m.id);
        if (it == run.pull.end())
            continue;
        PullStat const& st = it->second;
        // The eight rules that took the most seconds (a rule counts for the second only when it was the last to fire).
        std::vector<std::pair<std::uint32_t, std::string>> top;
        for (auto const& [rule, n] : st.rules)
            top.emplace_back(n, rule);
        std::sort(top.begin(), top.end(), [](auto const& a, auto const& b) { return a.first > b.first; });
        std::string rules;
        for (std::size_t i = 0; i < top.size() && i < 8; ++i)
            rules += fmt::format(" {}={}", top[i].second, top[i].first);
        if (st.rage_max)
            rules += fmt::format(" | rage avg {} max {} seconds at 25+ {}", st.seconds ? st.rage_sum / st.seconds : 0u, st.rage_max, st.rage_ge25);
        TC_LOG_INFO("playerbot.v2", "[pull_stats] {} {} {} {} damage={} boss={} hits={} seconds={} casting={} moving={} held={} dead={} rules{}",
                    run.def->name, m.def.name, ClassName(m.def.cls), RoleName(m.def.role), st.damage.load(), st.boss_damage.load(), st.hits.load(),
                    st.seconds, st.casting, st.moving, st.held, st.dead, rules.empty() ? " -" : rules);
    }
}

// Once a second of the run: starts the pull at the boss's first combat and counts each raider's second.
void SamplePull(Run& run)
{
    if (run.def->goal != ScenarioGoalKind::KillCreature || !run.def->arrive_near_entry)
        return;
    if (!run.pull_started.load())
    {
        Creature* boss = FindBoss(run);
        if (!boss || !boss->IsAlive() || !boss->IsInCombat())
            return;
        for (Member const& m : run.members)
            run.pull.try_emplace(m.id);
        run.pull_start_ms = run.run_ms;
        run.pull_started.store(true);
        return;
    }
    std::uint32_t const now = GameTime::GetGameTimeMS();
    for (Member const& m : run.members)
    {
        auto it = run.pull.find(m.id);
        Player* p = Find(m.id);
        if (it == run.pull.end() || !p)
            continue;
        PullStat& st = it->second;
        ++st.seconds;
        if (!p->IsAlive())
        {
            ++st.dead;
            continue;
        }
        if (p->IsNonMeleeSpellCast(false))
            ++st.casting;
        if (p->isMoving())
            ++st.moving;
        if (p->GetPowerType() == POWER_RAGE)
        {
            std::uint32_t const rage = std::uint32_t(p->GetPower(POWER_RAGE) / 10);
            st.rage_sum += rage;
            st.rage_max = std::max(st.rage_max, rage);
            st.rage_ge25 += rage >= 25 ? 1u : 0u;
        }
        if (BotAI* ai = ::Playerbot::Services::Registry().ai(m.id))
        {
            if (ai->holding_position(now))
                ++st.held;
            if (char const* r = ai->last_rule_fired())
                ++st.rules[r];
        }
    }
    if (!run.pull_reported && run.run_ms - run.pull_start_ms >= kPullReportAfterMs)
    {
        run.pull_reported = true;
        ReportPull(run, "report");
    }
}

// What the squad stands in at the pull: health and buffs per role, so a scenario that fails can be told from a raid that is under-prepared
// (MC Ragnaros 2026-10-13: healers died to 2000 damage with ~2000 health at the hit). One line per role.
// Every raider's footing at the order: how many stand on the navmesh, and the ones that do not (position, nearest-poly probe). A bot with no
// polygon under it cannot path anywhere (mc_lucifron: srcpoly=0, void ahead, 55-70 y from its target for the whole fight),
// which reads as a rule that never acts. 2 = a polygon within 5 y vertically, 1 = one only far above or below, 0 = no tile or no mesh, -1 = probe fault.
void LogRaiderFooting(Run const& run)
{
    std::uint32_t total = 0, on_mesh = 0;
    for (Member const& m : run.members)
    {
        Player* p = Find(m.id);
        if (!p || !p->IsAlive())
            continue;
        ++total;
        int probe = 0;
        if (Map* map = p->GetMap())
        {
            uint32 const terrain_map = PhasingHandler::GetTerrainMapId(p->GetPhaseShift(), p->GetMapId(), map->GetTerrain(), p->GetPositionX(), p->GetPositionY());
            if (dtNavMeshQuery const* q = MMAP::MMapManager::instance()->GetNavMeshQuery(terrain_map, p->GetMapId(), p->GetInstanceId()))
            {
                dtQueryFilter f;
                f.setIncludeFlags(0xFFFF);
                float ext[3] = { 3.0f, 5.0f, 3.0f };
                float pt[3] = { p->GetPositionY(), p->GetPositionZ(), p->GetPositionX() };   // TC -> Detour (y, z, x)
                int r = ::Playerbot::BotMovement::SehSafeNearestPolyProbe(q, pt, ext, &f);
                if (r != 0)
                    probe = r > 0 ? 2 : -1;
                else
                {
                    float ext2[3] = { 3.0f, 200.0f, 3.0f };
                    probe = ::Playerbot::BotMovement::SehSafeNearestPolyProbe(q, pt, ext2, &f);
                }
            }
        }
        if (probe == 2)
            ++on_mesh;
        else
            TC_LOG_INFO("playerbot.v2", "[scenario] {} footing: {} stands at {:.1f},{:.1f},{:.1f} with no navmesh polygon under it (probe {})", run.def->name,
                        p->GetName(), p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), probe);
    }
    TC_LOG_INFO("playerbot.v2", "[scenario] {} footing: {} of {} raiders stand on the navmesh", run.def->name, on_mesh, total);
}

void LogPullState(Run const& run)
{
    // Power Word: Fortitude / Prayer of Fortitude, Mark of the Wild / Gift of the Wild, Arcane Intellect / Brilliance, Blessings and Greater Blessings.
    static constexpr uint32 kFortitude[] = { 1243, 1244, 1245, 2791, 10937, 10938, 21562, 21564 };
    static constexpr uint32 kWild[] = { 1126, 5232, 6756, 5234, 8907, 9884, 9885, 21849, 21850 };
    auto has_any = [](Player const* p, auto const& ids) { for (uint32 id : ids) if (p->HasAura(id)) return true; return false; };
    struct Tally { uint32 n = 0, hp = 0, hp_min = ~0u, fort = 0, wild = 0, auras = 0; };
    std::map<std::string, Tally> by_role;
    for (Member const& m : run.members)
    {
        Player* p = Find(m.id);
        if (!p || !p->IsAlive())
            continue;
        Tally& t = by_role[std::to_string(uint32(m.def.role))];
        ++t.n;
        uint32 const hp = uint32(p->GetMaxHealth());
        t.hp += hp;
        t.hp_min = std::min(t.hp_min, hp);
        t.fort += has_any(p, kFortitude) ? 1u : 0u;
        t.wild += has_any(p, kWild) ? 1u : 0u;
        t.auras += uint32(p->GetAppliedAuras().size());
    }
    for (auto const& [role, t] : by_role)
        TC_LOG_INFO("playerbot.v2", "[scenario] {} pull state role {}: {} raiders, max health avg {} (lowest {}), Fortitude {}, Mark of the Wild {}, {} auras each on average",
                    run.def->name, role, t.n, t.n ? t.hp / t.n : 0u, t.hp_min == ~0u ? 0u : t.hp_min, t.fort, t.wild, t.n ? t.auras / t.n : 0u);
}

// Where the tanks stand and what they are doing: at the order and a few seconds into the fight (MC Ragnaros 2026-10-13 2318f9d0: the three
// assigned tanks stayed at 80 % with no target for 12 s while a fourth, off-duty druid held the boss).
void LogTankProbe(Run& run, char const* when)
{
    Creature* boss = FindBoss(run);
    for (Member const& m : run.members)
    {
        if (m.def.role != ScenarioRoleKind::Tank)
            continue;
        Player* p = Find(m.id);
        if (!p)
            continue;
        BotAI* ai = ::Playerbot::Services::Registry().ai(m.id);
        char const* rule = ai && ai->last_rule_fired() ? ai->last_rule_fired() : "-";
        Unit* victim = p->GetVictim();
        TC_LOG_INFO("playerbot.v2", "[scenario] {} tank {} {}: {} at {:.1f},{:.1f},{:.1f}, {:.1f} y from the boss, hp {}%, in combat {}, moving {}, rooted {}, victim {}, rule={}",
                    run.def->name, m.def.name, when, p->IsAlive() ? "alive" : "dead", p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(),
                    boss ? p->GetDistance(boss) : -1.0f, p->GetMaxHealth() ? int(100 * uint64(p->GetHealth()) / p->GetMaxHealth()) : 0,
                    p->IsInCombat() ? 1 : 0, p->isMoving() ? 1 : 0, p->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_STUNNED) ? 1 : 0,
                    victim ? victim->GetName() : std::string("-"), rule);
    }
}

bool IssueAttack(Run& run)
{
    Player* tank = MainTank(run);
    Creature* boss = FindBoss(run);
    if (!tank || !boss)
    {
        // No boss alive in the room is a finding about the setup (a lock from the last run kept it dead), not a fight to wait out. A raid
        // that died before the order is named as such: bwl_broodlord 2026-10-20 wiped in the landing second, the one tank left alive had
        // released to the graveyard 1157 y away, and the failure read as a missing boss.
        std::size_t alive = 0;
        for (Member const& m : run.members)
            if (Player* p = Find(m.id); p && p->IsAlive())
                ++alive;
        std::string reason;
        if (alive * 2 < run.members.size())
            reason = fmt::format("{} of {} members are dead before the attack order: killed on the way in or at the landing (the [bot_death] lines of the "
                                 "run say by what)", run.members.size() - alive, run.members.size());
        else if (!tank)
            reason = "no living tank to order the attack";
        else
            reason = fmt::format("creature {} ({}) is not alive within 400 y of the main tank {} at {:.1f},{:.1f},{:.1f}, {:.0f} y from the landing spot, "
                                 "at the time of the attack: dead from an earlier run, or not spawned", run.def->goal_entry, run.def->goal_name,
                                 tank->GetName(), tank->GetPositionX(), tank->GetPositionY(), tank->GetPositionZ(),
                                 std::hypot(tank->GetPositionX() - run.arrive_x, tank->GetPositionY() - run.arrive_y));
        run.monitor->NoteSetupFailure(run.run_ms, "", reason);
        return false;
    }
    LogPullState(run);
    LogRaiderFooting(run);
    // How many of the boss's entry stand within 400 y, and where: a leftover from an earlier run (Ragnaros summoned twice, a lair summon that
    // was never cleared) would explain tanks that read 16-20 y from "the boss" at the order and a fight that starts on the wrong one.
    {
        std::list<Creature*> same;
        tank->GetCreatureListWithEntryInGrid(same, run.def->goal_entry, 400.0f);
        std::string where;
        for (Creature* c : same)
            where += fmt::format(" [{} guid {} {} at {:.1f},{:.1f},{:.1f}{}]", c->GetName(), c->GetGUID().GetCounter(), c->IsAlive() ? "alive" : "dead",
                                 c->GetPositionX(), c->GetPositionY(), c->GetPositionZ(), c == boss ? ", the target" : "");
        TC_LOG_INFO("playerbot.v2", "[scenario] {} creature {} ({}) count within 400 y at the order: {}{}", run.def->name, run.def->goal_name,
                    run.def->goal_entry, same.size(), where);
    }
    LogBuffs(run);
    // An encounter the instance holds in progress shuts the tank's advance (gate encounter_in_progress) for the whole run: name which one
    // (release-0130 zg_marli: the gate was shut from the first tick, before anybody had engaged anything).
    if (InstanceMap* im = tank->GetMap()->ToInstanceMap())
        if (InstanceScript* is = im->GetInstanceScript(); is && is->IsEncounterInProgress())
        {
            std::string states;
            for (uint32 i = 0; i < is->GetEncounterCount(); ++i)
                if (EncounterState const st = is->GetBossState(i); st == IN_PROGRESS || st == SPECIAL)
                    states += fmt::format(" boss {} {}", i, st == IN_PROGRESS ? "IN_PROGRESS" : "SPECIAL");
            TC_LOG_WARN("playerbot.v2", "[scenario] {} the instance holds an encounter in progress at the order:{}", run.def->name,
                        states.empty() ? std::string(" none of its boss states (the script's own flag)") : states);
        }
    run.order_sent = true;
    run.attack_run_ms = run.run_ms ? run.run_ms : 1u;
    LogTankProbe(run, "at the order");
    // The Hold ends with the order, for everyone: the mana wait renews it every tick for 30 s at a time, so after a long wait the whole squad
    // would stand in it for up to half a minute into the fight (MC Ragnaros 2026-10-13 2318f9d0: three assigned tanks with no target and 7 of 24
    // DPS hitting for the first 12 s, the boss on an off-duty druid). The main tank gets the Engage below.
    for (Member const& m : run.members)
        if (BotAI* ai = ::Playerbot::Services::Registry().ai(m.id))
            if (ai->holding_position(GameTime::GetGameTimeMS()))
                ai->exit_manual();
    // A fight the raid opens on an add (ScenarioDef::start_add): the order is onto the nearest living one, as a raid leader marks it. Mandokir
    // sits on his raptor immune to players until his Speaker dies; the order onto him was refused and the run failed its setup at 20 s
    // while the raid went on to pull the Speaker itself and kill the boss (sweep-zg-0134 zg_mandokir: goal reached, setup_failed).
    Creature* target = boss;
    if (run.def->start_add)
        if (Creature* add = tank->FindNearestCreature(run.def->start_add, 400.f, true))
            target = add;
    ::Playerbot::API api(tank);
    ::Playerbot::Result const res = api.start_attack(target->GetGUID());
    if (BotAI* ai = ::Playerbot::Services::Registry().ai(tank->GetGUID().GetCounter()))
    {
        ai->enter_manual(BotAI::OwnerCommand::Engage, target->GetGUID(), GameTime::GetGameTimeMS());
        ai->release_owner_lead();
    }
    TC_LOG_INFO("playerbot.v2", "[scenario] {} ordered {} onto {} ({}) at {} s, {:.0f} y away (start_attack result {})", run.def->name,
                tank->GetName(), target->GetName(), target->GetEntry(), run.run_ms / 1000u, tank->GetDistance(target), int(res));
    // A target the server refuses (immune to players, not interactible) is a fault in the setup, not a fight that happens to be quiet.
    // Not when the boss is already fighting the raid: a boss script (Bot/Raid/RaidFightScript.h) owns the pull and had the DPS on it before
    // this order, and Attack on a mob the bot already fights is refused as an invalid target.
    if (res == ::Playerbot::Result::InvalidTarget && !target->IsInCombat())
        run.monitor->NoteSetupFailure(run.run_ms, "", fmt::format("{} refused the attack order on {} ({}): the {} is not attackable (immune to players or not interactible)",
                                                                   tank->GetName(), target->GetName(), target->GetEntry(), target == boss ? "boss" : "opening add"));
    return true;
}

// ---- The failure report ----
// On a FAIL one block carries what the diagnosis keeps asking the operator to grep for: the module's own log lines of the last
// kReportWindowMs before the first failure (the tags below) and every bot's state when the run ended. Playerbot.log has no
// timestamps, so the runner notes the file's size each second and reads from the mark of the window's start.
constexpr std::uint32_t kReportWindowMs = 60u * 1000u;
constexpr std::size_t kReportMaxLines = 500;
// A wipe is judged at the end of the fight, long after it was lost: the window before it only shows the corpses (bwl_broodlord 256:
// first death at 25 s, the wipe at 648 s, 7 tagged lines in the report). When a death comes before the first failure, a second
// window reads the log from kFirstDeathLeadMs before that death to kFirstDeathTailMs after it, from its start.
constexpr std::uint32_t kFirstDeathLeadMs = 30u * 1000u;
constexpr std::uint32_t kFirstDeathTailMs = 15u * 1000u;
constexpr std::size_t kLogMarksMax = 3600;
constexpr std::string_view kReportTags[] = {
    "[raid_pull]", "[pull_back]", "[pull_gate]", "[raid_heal]", "[healer_drink]", "[healer_hold]", "[raid_tank]", "[tank_",
    "[raid_hazard]", "[raid_escape]", "[raid_fight]", "[raid_threat]", "[mark_cc]", "[mark_gate]", "[bot_death]", "[spell_dmg]",
    "[interrupt", "[raid_school", "[mark_cc]", "[pvecoord]", "[taunt]", "[raid_retaunt]", "[catch_up]", "[owner_steps]", "[fly_with_owner]", "[follow_drop]",
    "[scenario_lead]", "[sight_pull]", "[help_runner]" };

std::string LogPath() { return sLog->GetLogsDir() + "Playerbot.log"; }

void NoteLogMark(Run& run)
{
    std::error_code ec;
    std::uintmax_t const size = std::filesystem::file_size(LogPath(), ec);
    if (ec)
        return;
    run.log_marks.emplace_back(run.run_ms, size);
    while (run.log_marks.size() > kLogMarksMax)
        run.log_marks.pop_front();
}

// The tagged lines written between the mark at (or after) `from_ms` and the mark at (or after) `to_ms` (0 = to the end of the file).
// keep_first: the first kReportMaxLines of them, else the last.
std::vector<std::string> LogLinesBetween(Run const& run, std::uint32_t from_ms, std::uint32_t to_ms, bool keep_first)
{
    std::vector<std::string> out;
    if (run.log_marks.empty())
        return out;
    std::uintmax_t start = run.log_marks.back().second;
    for (auto const& [ms, off] : run.log_marks)
        if (ms >= from_ms) { start = off; break; }
    std::uintmax_t stop = 0;
    if (to_ms)
        for (auto const& [ms, off] : run.log_marks)
            if (ms >= to_ms) { stop = off; break; }
    std::ifstream in(LogPath(), std::ios::binary);
    if (!in)
        return out;
    in.seekg(0, std::ios::end);
    std::uintmax_t const end = std::uintmax_t(in.tellg());
    if (start > end)
        start = 0;   // the file was rotated since: read what is there
    if (!stop || stop > end || stop < start)
        stop = end;
    in.seekg(std::streamoff(start));
    std::string line;
    while (std::uintmax_t(in.tellg()) < stop && std::getline(in, line))
    {
        bool keep = false;
        for (std::string_view tag : kReportTags)
            if (line.find(tag) != std::string::npos) { keep = true; break; }
        if (!keep)
            continue;
        // Squads running side by side share the log: keep the lines that name one of this squad's characters.
        if (g_parallel > 1)
        {
            bool mine = false;
            for (Member const& m : run.members)
                if (line.find(m.def.name) != std::string::npos) { mine = true; break; }
            if (!mine)
                continue;
        }
        if (line.size() > 400)
            line.resize(400);
        out.push_back(line);
        if (out.size() > kReportMaxLines)
        {
            if (keep_first)
            {
                out.pop_back();
                break;
            }
            out.erase(out.begin());
        }
    }
    return out;
}

// The tagged lines written since the mark at (or after) `from_ms`; at most the last kReportMaxLines.
std::vector<std::string> LogLinesSince(Run const& run, std::uint32_t from_ms)
{
    return LogLinesBetween(run, from_ms, 0, false);
}

struct KeySpell { std::uint32_t spell; char const* name; };
constexpr KeySpell kWarriorSpells[] = { {355, "Taunt"}, {2565, "ShieldBlock"}, {72, "ShieldBash"}, {2687, "Bloodrage"}, {6343, "ThunderClap"}, {1160, "DemoShout"}, {871, "ShieldWall"} };
constexpr KeySpell kPriestSpells[]  = { {17, "PWShield"}, {586, "Fade"}, {8122, "PsychicScream"} };
constexpr KeySpell kMageSpells[]    = { {122, "FrostNova"}, {2139, "Counterspell"}, {1953, "Blink"} };
constexpr KeySpell kHunterSpells[]  = { {781, "Disengage"}, {5384, "FeignDeath"}, {19263, "Deterrence"}, {1499, "FreezingTrap"} };
constexpr KeySpell kRogueSpells[]   = { {1766, "Kick"}, {1856, "Vanish"}, {2094, "Blind"}, {1784, "Stealth"} };

template <std::size_t N>
void AppendCooldowns(std::string& out, Player* p, KeySpell const (&table)[N])
{
    for (KeySpell const& k : table)
    {
        if (!p->HasSpell(k.spell))
            continue;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(k.spell, DIFFICULTY_NONE);
        if (!info)
            continue;
        auto const left = std::chrono::duration_cast<std::chrono::milliseconds>(p->GetSpellHistory()->GetRemainingCooldown(info)).count();
        out += left > 0 ? fmt::format(" {}={:.0f}s", k.name, float(left) / 1000.f) : fmt::format(" {}=ready", k.name);
    }
}

// One line a member: class, hp, mana / rage / energy, position, the rule its bot logic fired last, and its key cooldowns.
std::string MemberState(Member const& m)
{
    Player* p = Find(m.id);
    if (!p || !p->IsInWorld())
        return fmt::format("{} not in the world", m.def.name);
    std::string s = fmt::format("{} class={} lvl={} {} hp={}/{} ({:.0f}%)", m.def.name, uint32(p->GetClass()), uint32(p->GetLevel()),
                                p->IsAlive() ? "alive" : "DEAD", p->GetHealth(), p->GetMaxHealth(), float(p->GetHealthPct()));
    Powers const pt = p->GetPowerType();
    s += fmt::format(" {}={}/{}", pt == POWER_MANA ? "mana" : pt == POWER_RAGE ? "rage" : pt == POWER_ENERGY ? "energy" : "power",
                     pt == POWER_RAGE ? p->GetPower(pt) / 10 : p->GetPower(pt), pt == POWER_RAGE ? p->GetMaxPower(pt) / 10 : p->GetMaxPower(pt));
    s += fmt::format(" pos=({:.1f},{:.1f},{:.1f}) combat={}", p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), p->IsInCombat() ? "yes" : "no");
    if (Unit* v = p->GetVictim())
        s += fmt::format(" victim={}({})", v->GetEntry(), v->GetGUID().GetCounter());
    if (BotAI* ai = ::Playerbot::Services::Registry().ai(m.id))
        if (char const* rule = ai->last_rule_fired())
            s += fmt::format(" rule={}", rule);
    s += " cd:";
    switch (p->GetClass())
    {
        case CLASS_WARRIOR: AppendCooldowns(s, p, kWarriorSpells); break;
        case CLASS_PRIEST:  AppendCooldowns(s, p, kPriestSpells);  break;
        case CLASS_MAGE:    AppendCooldowns(s, p, kMageSpells);    break;
        case CLASS_HUNTER:  AppendCooldowns(s, p, kHunterSpells);  break;
        case CLASS_ROGUE:   AppendCooldowns(s, p, kRogueSpells);   break;
        default: break;
    }
    return s;
}

std::string FailureReport(Run const& run)
{
    std::string text;
    std::uint32_t const first_ms = run.monitor->failures().empty() ? run.run_ms : run.monitor->failures().front().at_ms;
    std::uint32_t const from_ms = first_ms > kReportWindowMs ? first_ms - kReportWindowMs : 0;
    text += fmt::format("[scenario] report window={}..{} s (first failure at {} s), the tagged Playerbot.log lines of it, then each bot's state at the end", from_ms / 1000u,
                        run.run_ms / 1000u, first_ms / 1000u);
    for (Member const& m : run.members)
        text += "\n[scenario] report state " + MemberState(m);
    for (std::string const& l : LogLinesSince(run, from_ms))
        text += "\n[scenario] report log " + l;
    // The first death, when it came before the window above: how the fight was lost, not how it ended.
    for (ScenarioFailure const& w : run.monitor->warnings())
    {
        if (w.kind != ScenarioFailKind::Death)
            continue;
        if (w.at_ms < from_ms)
        {
            std::uint32_t const lead_ms = w.at_ms > kFirstDeathLeadMs ? w.at_ms - kFirstDeathLeadMs : 0;
            text += fmt::format("\n[scenario] report first-death window={}..{} s (first death {} at {} s)", lead_ms / 1000u,
                                (w.at_ms + kFirstDeathTailMs) / 1000u, w.bot, w.at_ms / 1000u);
            for (std::string const& l : LogLinesBetween(run, lead_ms, w.at_ms + kFirstDeathTailMs, true))
                text += "\n[scenario] report death-log " + l;
        }
        break;
    }
    return text;
}

void WriteResult(std::string const& text)
{
    g_last_result = text;
    for (auto const& line : Trinity::Tokenize(text, '\n', true))
        TC_LOG_INFO("playerbot.v2", "{}", std::string(line));
    std::ofstream out(sLog->GetLogsDir() + "Scenario.log", std::ios::app);
    if (out)
        out << text << '\n';
}

// The support tallies a run is judged against start at its first second: earlier runs on the same map left theirs behind.
void BaseSupport(Run& run)
{
    run.base_tranq = Classic::RaidSupportTotal(Classic::RaidSupportKind::Tranq, run.def->map);
    run.base_decurse = Classic::RaidSupportTotal(Classic::RaidSupportKind::Decurse, run.def->map);
    run.base_giveup = Classic::RaidSupportTotal(Classic::RaidSupportKind::FormationGiveUp, run.def->map);
    run.support_based = true;
}

bool BeginScenario(std::string const& name, int slot, std::string& error);
void StartNext();

// Takes the squad out of the content and ends the run.
void Finish(Run& run, bool timed_out, std::string const& note = {})
{
    ScenarioDef const& def = *run.def;
    ReportPull(run, "final");
    if (def.parity)
    {
        // The damage_parity family: the dummy, the stat bonuses and the gear override go; a bot whose bot logic was taken off for a sweep is logged out
        // (the next staging logs it in with a fresh one).
        Parity::End();
        PetRevive::End();
        if (def.parity != 1 && def.parity != 8 && def.parity != 9 && !run.members.empty())
            ::Playerbot::Services::SessionMgr().LogoutBot(ObjectGuid::Create<HighGuid::Player>(run.members.front().id));
    }
    if (timed_out)
        run.monitor->NoteTimeout(run.run_ms, note.empty() ? fmt::format("goal not met in {} s", def.timeout_ms / 1000u) : note);

    if (run.support_based)
    {
        std::uint32_t const tranq = Classic::RaidSupportTotal(Classic::RaidSupportKind::Tranq, def.map) - run.base_tranq;
        std::uint32_t const decurse = Classic::RaidSupportTotal(Classic::RaidSupportKind::Decurse, def.map) - run.base_decurse;
        std::uint32_t const giveups = Classic::RaidSupportTotal(Classic::RaidSupportKind::FormationGiveUp, def.map) - run.base_giveup;
        if (def.limits.min_tranq_shots || def.limits.min_decurses || tranq || decurse || giveups)
            run.monitor->NoteInfo(fmt::format("support: tranquilizing shots {}, curses removed {}, formation slot give-ups {}", tranq, decurse, giveups));
        // Judged once the boss is dead: a run that ended on another failure has not had its chance to show the support.
        if (run.goal_reached)
        {
            if (def.limits.min_tranq_shots && tranq < def.limits.min_tranq_shots)
                run.monitor->NoteSupportMissing(run.run_ms, fmt::format("{} Tranquilizing Shot(s) cast, at least {} expected: the hunters never tranquilized the Frenzy ([raid_tranq])",
                                                                       tranq, def.limits.min_tranq_shots));
            if (def.limits.min_decurses && decurse < def.limits.min_decurses)
                run.monitor->NoteSupportMissing(run.run_ms, fmt::format("{} curse(s) removed, at least {} expected: the boss's curse was never cleansed ([raid_dispel])",
                                                                       decurse, def.limits.min_decurses));
        }
        if (def.spare_entry)
            run.monitor->NoteInfo(fmt::format("spared adds: {} killed before {}", run.spared_killed, def.goal_name));
        if (run.goal_reached && run.spared_killed)
            run.monitor->NoteSupportMissing(run.run_ms, fmt::format("{} add(s) of entry {} died before {}: the raid was to leave them alive until he fell ([raid_fight])",
                                                                   run.spared_killed, def.spare_entry, def.goal_name));
    }

    if (run.goal_reached)
        run.monitor->DemoteToWarningsOnGoal();
    bool const pass = run.monitor->passed(run.goal_reached);
    std::string text = fmt::format("[scenario] result={} name={} level={} squad={} run_s={} goal={} failures={} warnings={}",
                                   pass ? "PASS" : "FAIL", def.name, uint32(def.level), run.members.size(), run.run_ms / 1000u,
                                   run.goal_reached ? "reached" : "not_reached", run.monitor->failures().size(), run.monitor->warnings().size());
    if (!run.from_key.empty())
        text += " from=" + run.from_key;
    for (ScenarioFailure const& f : run.monitor->failures())
        text += fmt::format("\n[scenario] fail kind={} bot={} at_s={} {}", ToString(f.kind), f.bot.empty() ? "-" : f.bot, f.at_ms / 1000u, f.detail);
    // What the bots did wrong on the way, in a run whose pass bar is the kill: deaths, mana, holds, strays.
    for (ScenarioFailure const& f : run.monitor->warnings())
        text += fmt::format("\n[scenario] warn kind={} bot={} at_s={} {}", ToString(f.kind), f.bot.empty() ? "-" : f.bot, f.at_ms / 1000u, f.detail);
    for (std::string const& d : run.monitor->tolerated())
        text += "\n[scenario] note " + d;
    // Each failing bot's rule changes in the 90 s before its failure, also on a run that goes on to fail for another reason.
    for (std::string const& c : run.monitor->contexts())
        text += "\n[scenario] context " + c;
    if (!pass)
    {
        // A raid: the tanks, a few healers and a few DPS, and whoever failed; 40 bots' traces would bury the cause.
        auto const trace = run.members.size() > 5 ? run.monitor->TraceSelected(25, 3, 3) : run.monitor->Trace();
        for (std::string const& line : trace)
            text += "\n[scenario] trace " + line;
    }
    WriteResult(text);
    if (!pass)
    {
        // The report goes to Scenario.log only (it re-quotes up to 500 log lines; the module log has them already).
        std::string const report = FailureReport(run);
        std::ofstream out(sLog->GetLogsDir() + "Scenario.log", std::ios::app);
        if (out)
            out << report << '\n';
        TC_LOG_INFO("playerbot.v2", "[scenario] failure report of {} written to Scenario.log ({} lines)", def.name,
                    std::count(report.begin(), report.end(), '\n') + 1);
    }

    if (g_all)
    {
        std::string line = fmt::format("{} {}", def.name, pass ? "PASS" : "FAIL");
        if (pass)
            line += fmt::format(" in {} s", run.run_ms / 1000u);
        else if (!run.monitor->failures().empty())
        {
            ScenarioFailure const& f = run.monitor->failures().front();
            line += fmt::format(" {} {}{} at {} s: {}", ToString(f.kind), f.bot, f.bot.empty() ? "" : " ", f.at_ms / 1000u, f.detail);
        }
        else
            line += " goal not reached";
        g_summary.push_back(line);
        TC_LOG_INFO("playerbot.v2", "[scenario_all] {}", line);
        if (!pass)
            g_wave_failed = true;
    }

    DropLead(run);
    GiveSquadOrders(run, false);
    // A detached stand-in has no bot logic to come back: it is logged out, and the next staging logs it in with a fresh one.
    if (run.stand_in_detached)
        if (Player* si = StandIn(run))
            ::Playerbot::Services::SessionMgr().LogoutBot(si->GetGUID());
    for (Member const& m : run.members)
        if (Player* p = Find(m.id))
        {
            ::Playerbot::V2::Fleet::BotPopulationManager::SetGearModeOverride(m.id, -1);
            ::Playerbot::V2::Fleet::BotPopulationManager::SetGearResistOverride(m.id, 0);
            ::Playerbot::V2::Fleet::BotPopulationManager::SetGearWearOverride(m.id, 0);
            LeaveGroup(p);
            p->CombatStop(true);
            if (!p->IsAlive())
            {
                p->ResurrectPlayer(1.0f);
                p->SpawnCorpseBones();
            }
            SendToStage(p, run.def->goal == ScenarioGoalKind::Parity);
        }
    // Take the run out of the list but keep it alive to the end of this function: `run` is a reference into it.
    std::unique_ptr<Run> owned;
    for (auto it = g_runs.begin(); it != g_runs.end(); ++it)
        if (it->get() == &run)
        {
            owned = std::move(*it);
            g_runs.erase(it);
            break;
        }
    StartNext();

    // The squad leaves the world with the run: a test character standing at the staging spot is seen by players (244: the dps
    // scorecard's Tier 3 raid stood in Northshire after its runs, at the parity staging spot). Only the characters the run started
    // next (runall, the next wave) or a run still going takes over stay: they are staged again at once.
    std::set<std::string> still_used;
    for (auto const& r : g_runs)
        for (Member const& m : r->members)
            still_used.insert(m.def.name);
    std::uint32_t logged_out = 0;
    for (Member const& m : run.members)
        if (m.id && !still_used.count(m.def.name) && Find(m.id))
            if (::Playerbot::Services::SessionMgr().LogoutBot(ObjectGuid::Create<HighGuid::Player>(m.id)))
                ++logged_out;
    if (logged_out)
        TC_LOG_INFO("playerbot.v2", "[scenario] {}: {} squad character(s) logged out at the end of the run", def.name, logged_out);
}

void Sample(Run& run, std::vector<ScenarioSample>& out)
{
    out.clear();
    for (Member const& m : run.members)
    {
        Player* p = Find(m.id);
        ScenarioSample s;
        s.id = m.id;
        s.name = m.def.name;
        s.role = m.def.role;
        if (!p || !p->IsInWorld())
        {
            s.alive = false;
            out.push_back(std::move(s));
            continue;
        }
        s.alive = p->IsAlive() && !p->HasAuraType(SPELL_AURA_GHOST);   // a released ghost is dead, not a bot on another map
        s.in_combat = p->IsInCombat();
        s.casting = p->HasUnitState(UNIT_STATE_CASTING);
        s.map = p->GetMapId();
        s.x = p->GetPositionX(); s.y = p->GetPositionY(); s.z = p->GetPositionZ();
        s.hp_pct = float(p->GetHealthPct());
        s.group_size = p->GetGroup() ? uint32(p->GetGroup()->GetMembersCount()) : 0u;
        if (BotAI* ai = ::Playerbot::Services::Registry().ai(m.id))
        {
            if (char const* rule = ai->last_rule_fired())
                s.rule = rule;
            // A squad is a bot-led group and runs the dungeon autopilot (its tank pulls, the damage dealers hold back until the tank
            // brings the pack). It does not stand in for a player's group: with both at once (RC8, rfc_oggleflint) the group fight sent the
            // damage dealers 40-53 y after the tank's pull while the autopilot expected them to wait, and the tank died to the extra
            // Earthborers they woke. The idle check reads 0 here unless a squad is ever led by a player.
            s.group_idle_ms = ai->group_idle_ms(GameTime::GetGameTimeMS());
        }
        if (p->GetPowerType() == POWER_MANA && p->GetMaxPower(POWER_MANA) > 0)
            s.mana_pct = 100.f * float(p->GetPower(POWER_MANA)) / float(p->GetMaxPower(POWER_MANA));
        if (run.def->limits.water_under_ms || run.def->limits.water_flips || run.def->limits.water_noswim_ms)
        {
            float swim_z = 0.f;
            s.deep_water = s.alive && ::Playerbot::BotMovement::SwimLevel(p, s.x, s.y, s.z, swim_z);
            s.head_under = p->IsUnderWater();
            s.swim_flag = p->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING);
        }
        out.push_back(std::move(s));
    }
}

// Starts the named scenario: the catalogue entry, a goal creature this world's DB knows, a fresh run in the Stage phase.
bool BeginScenario(std::string const& full_name, int slot, std::string& error)
{
    // `name@key`: a checkpoint start inside a dungeon scenario.
    std::string name = full_name, from_key;
    if (std::size_t const at = full_name.find('@'); at != std::string::npos)
    {
        name = full_name.substr(0, at);
        from_key = full_name.substr(at + 1);
    }
    ScenarioDef const* def = FindScenario(name);
    if (!def)
    {
        error = "no scenario '" + name + "'; see `.playerbot scenario list`";
        return false;
    }
    ScenarioCheckpoint const* cp = nullptr;
    if (!from_key.empty())
    {
        for (ScenarioCheckpoint const& c : def->checkpoints)
            if (from_key == c.key) { cp = &c; break; }
        if (!cp)
        {
            std::string keys;
            for (ScenarioCheckpoint const& c : def->checkpoints)
                keys += std::string(keys.empty() ? "" : ", ") + c.key;
            error = fmt::format("scenario '{}' has no checkpoint '{}' ({})", name, from_key, keys.empty() ? "it has none" : "use " + keys);
            return false;
        }
        if (def->arrive_near_entry)
        {
            error = fmt::format("scenario '{}' is a raid: it already starts in the boss room", name);
            return false;
        }
    }
    if (def->goal == ScenarioGoalKind::KillCreature)
    {
        CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(def->goal_entry);
        if (!ct)
        {
            error = fmt::format("the goal creature {} ({}) is not in this world's creature_template: the scenario's data is wrong", def->goal_entry, def->goal_name);
            return false;
        }
        if (ct->Name != def->goal_name)
            TC_LOG_WARN("playerbot.v2", "[scenario] {} goal creature {} is '{}' in the DB, the scenario says '{}'", def->name, def->goal_entry, ct->Name, def->goal_name);
    }
    auto owned = std::make_unique<Run>();
    Run& run = *owned;
    run.def = def;
    run.slot = slot;
    run.phase = Phase::Stage;
    ScenarioLimits limits = def->limits;
    limits.expected_group = std::uint32_t(def->squad.size());
    // A raid's box is drawn round its entrance (MakeRaidScenario), and a boss far from it stands outside: every raider was out of bounds
    // in the landing second (release-0130 aq20_ayamiss: 20 warnings at -9669,1563, Ayamiss 1260 y from the entrance). The box also takes
    // in the boss's room, with the same margins as round the entrance's z and 300 y across the floor.
    if (def->arrive_near_entry && limits.bounds.enabled)
        for (auto const& kv : sObjectMgr->GetAllCreatureData())
            if (kv.second.id == def->arrive_near_entry && kv.second.mapId == def->map)
            {
                Position const& sp = kv.second.spawnPoint;
                limits.bounds.min_x = std::min(limits.bounds.min_x, sp.GetPositionX() - 300.f);
                limits.bounds.max_x = std::max(limits.bounds.max_x, sp.GetPositionX() + 300.f);
                limits.bounds.min_y = std::min(limits.bounds.min_y, sp.GetPositionY() - 300.f);
                limits.bounds.max_y = std::max(limits.bounds.max_y, sp.GetPositionY() + 300.f);
                limits.bounds.min_z = std::min(limits.bounds.min_z, sp.GetPositionZ() - 250.f);
                limits.bounds.max_z = std::max(limits.bounds.max_z, sp.GetPositionZ() + 250.f);
                break;
            }
    run.monitor = std::make_unique<ScenarioMonitor>(limits);
    run.stage_timeout_ms = kStageTimeoutMs + std::uint32_t(def->squad.size()) * kStageTimeoutPerBotMs;
    run.arrive_entry = cp ? cp->creature_entry : def->arrive_near_entry;
    run.checkpoint = cp != nullptr;
    if (cp && cp->stage_z != 0.f)
    {
        run.has_stage = true;
        run.stage_x = cp->stage_x; run.stage_y = cp->stage_y; run.stage_z = cp->stage_z;
    }
    run.from_key = from_key;
    if (run.arrive_entry && !FindBossSpawn(run))
    {
        error = fmt::format("no spawn of creature {} on map {} in this world's creature table: the scenario's data is wrong", run.arrive_entry, def->map);
        return false;
    }
    for (ScenarioMember const& sm : def->squad)
    {
        Member m;
        m.def = sm;
        m.def.name = SquadName(sm.name, slot);
        run.members.push_back(m);
    }
    TC_LOG_INFO("playerbot.v2", "[scenario] start name={} level={} squad={}{}{}", def->name, uint32(def->level), def->squad.size(),
                from_key.empty() ? std::string() : " from=" + from_key, slot ? fmt::format(" slot={}", slot) : std::string());
    g_runs.push_back(std::move(owned));
    return true;
}

// runall: the next queued scenario, or the totals when the queue is empty. A scenario that cannot even start is a FAIL line, not the end.
void StartNext()
{
    // A wave (one scenario for runall, g_parallel side by side for runbatch) starts only when the previous one has ended.
    if (g_all && g_runs.empty() && g_halt_on_fail && g_wave_failed && !g_queue.empty())
    {
        TC_LOG_INFO("playerbot.v2", "[scenario_all] the last wave had a FAIL: the batch stops here with {} scenario(s) not started; fix, then `runbatch` again", g_queue.size());
        g_queue.clear();
    }
    if (g_all && g_runs.empty())
    {
        g_wave_failed = false;
        while (!g_queue.empty() && g_runs.size() < g_parallel)
        {
            std::string const name = g_queue.front();
            g_queue.pop_front();
            std::string error;
            // Slots are taken in order and a wave starts from none: the next free slot is the number running.
            if (BeginScenario(name, int(g_runs.size()), error))
                continue;
            std::string const line = fmt::format("{} FAIL cannot start: {}", name, error);
            g_summary.push_back(line);
            g_wave_failed = true;
            TC_LOG_INFO("playerbot.v2", "[scenario_all] {}", line);
        }
    }
    if (g_all && g_runs.empty() && g_queue.empty())
    {
        g_all = false;
        std::size_t pass = 0;
        for (std::string const& l : g_summary)
            if (l.find(" PASS") != std::string::npos)
                ++pass;
        TC_LOG_INFO("playerbot.v2", "[scenario_all] done pass={} fail={}", pass, g_summary.size() - pass);
    }
}

} // namespace

void MarkTestCharacters()
{
    // Every name a squad can carry: the catalogue's (slot 0) and the slot letters runbatch gives (StartBatch: up to eight side by side).
    std::set<std::string> names;
    for (ScenarioDef const& d : ScenarioCatalogue())
        for (ScenarioMember const& sm : d.squad)
            for (int slot = 0; slot < 8; ++slot)
                names.insert(SquadName(sm.name, slot));
    std::uint32_t found = 0;
    for (std::string const& n : names)
    {
        ObjectGuid const g = sCharacterCache->GetCharacterGuidByName(n);
        if (g.IsEmpty())
            continue;
        ::Playerbot::Pool::MarkScenarioBot(g.GetCounter());
        ++found;
    }
    TC_LOG_INFO("playerbot.v2", "[scenario] {} scenario test character(s) marked (of {} catalogue names): kept out of the world pool, battleground "
                "and Group Finder fills, and trades", found, names.size());
}

std::uint32_t LogoutIdleTestBots(std::string* names)
{
    // The characters a running scenario holds stay; everything else marked as a test character leaves the world.
    std::set<std::uint64_t> in_run;
    for (auto const& r : g_runs)
        for (Member const& m : r->members)
            if (m.id)
                in_run.insert(m.id);
    std::uint32_t n = 0;
    for (std::uint64_t const id : ::Playerbot::Pool::ScenarioBots())
    {
        if (in_run.count(id))
            continue;
        Player* p = Find(id);
        if (!p)
            continue;
        if (!::Playerbot::Services::SessionMgr().LogoutBot(p->GetGUID()))
            continue;   // not a bot session (a GM logged into a test character himself): left alone
        if (names)
        {
            if (!names->empty())
                *names += ", ";
            *names += p->GetName();
        }
        ++n;
    }
    return n;
}

bool Start(std::string const& name, std::string& error)
{
    if (!g_runs.empty())
    {
        error = fmt::format("scenario '{}' is already running", g_runs.front()->def->name);
        return false;
    }
    g_all = false;
    g_queue.clear();
    g_parallel = 1;
    return BeginScenario(name, 0, error);
}

static bool StartWaves(std::string const& filter, std::size_t parallel, bool halt_on_fail, std::string& error);

bool StartAll(std::string const& filter, std::string& error)
{
    return StartWaves(filter, 1, false, error);
}

bool StartBatch(std::string const& filter, std::size_t size, std::string& error)
{
    return StartWaves(filter, std::clamp<std::size_t>(size ? size : 5, 1, 8), true, error);
}

static bool StartWaves(std::string const& filter, std::size_t parallel, bool halt_on_fail, std::string& error)
{
    if (!g_runs.empty())
    {
        error = fmt::format("scenario '{}' is already running", g_runs.front()->def->name);
        return false;
    }
    g_queue.clear();
    g_summary.clear();
    // No filter = the five-man dungeons; the raids (an hour of staging and fighting) only when the filter names them (`runall mc`).
    for (ScenarioDef const& d : ScenarioCatalogue())
        if (filter.empty() ? (d.arrive_near_entry == 0 && !d.full_clear) : std::string(d.name).find(filter) != std::string::npos)
            g_queue.push_back(d.name);
    if (g_queue.empty())
    {
        error = "no scenario matches '" + filter + "'";
        return false;
    }
    // A raid (forty bots) or a whole-dungeon clear is never run beside other squads.
    for (std::string const& n : g_queue)
        if (ScenarioDef const* d = FindScenario(n); d && (d->arrive_near_entry || d->full_clear))
            parallel = 1;
    g_parallel = parallel;
    g_halt_on_fail = halt_on_fail && parallel > 1;
    g_wave_failed = false;
    g_all = true;
    StartNext();
    return true;
}

bool Abort()
{
    if (g_runs.empty())
        return false;
    g_queue.clear();    // an aborted runall stops after the running ones
    std::vector<Run*> running;
    for (auto const& r : g_runs)
        running.push_back(r.get());
    for (Run* r : running)
        Finish(*r, true, "aborted by the operator");
    return true;
}

bool Running() { return !g_runs.empty(); }

static void TickRun(Run& run, std::uint32_t diff_ms);

void Tick(std::uint32_t diff_ms)
{
    // The first tick (the character cache is loaded): the catalogue's characters from earlier sessions are test characters again.
    static bool s_marked = false;
    if (!s_marked)
    {
        s_marked = true;
        MarkTestCharacters();
    }
    if (g_runs.empty())
    {
        // No run: no test character belongs in the world. Whatever is still online (an aborted run, a login from elsewhere) is logged out.
        static std::uint32_t s_idle_ms = 0;
        s_idle_ms += diff_ms;
        if (s_idle_ms >= 30u * 1000u)
        {
            s_idle_ms = 0;
            std::string names;
            if (std::uint32_t const n = LogoutIdleTestBots(&names))
                TC_LOG_INFO("playerbot.v2", "[scenario] no run: {} test character(s) logged out ({})", n, names);
        }
        return;
    }
    // Finish() removes the run and may start the next wave: tick a copy of the list, and only runs that are still there.
    std::vector<Run*> running;
    for (auto const& r : g_runs)
        running.push_back(r.get());
    for (Run* r : running)
    {
        bool alive = false;
        for (auto const& q : g_runs)
            if (q.get() == r) { alive = true; break; }
        if (alive)
            TickRun(*r, diff_ms);
    }
}

static void TickRun(Run& run, std::uint32_t diff_ms)
{
    if (run.phase == Phase::Idle)
        return;
    // A death ends the run at once, before the dead bot can release and turn one failure into a cascade of wrong_map ones.
    if (run.phase == Phase::Run && run.monitor->terminal())
    {
        Finish(run, false);
        return;
    }
    run.step_acc_ms += diff_ms;
    run.phase_ms += diff_ms;
    if (run.phase == Phase::Run)
    {
        run.run_ms += diff_ms;
        if (run.place_logs < 2 && run.run_ms >= (run.place_logs ? 3000u : 1000u))
        {
            char const* when = run.place_logs ? "3 s in" : "1 s in";
            ++run.place_logs;
            for (Member const& m : run.members)
                LogPlace(run, Find(m.id), when);
        }
    }
    if (run.step_acc_ms < kStepMs)
        return;
    run.step_acc_ms = 0;

    switch (run.phase)
    {
        case Phase::Stage:
        {
            run.stage_status.clear();
            bool all = true;
            // A raid is set up a few members at a time: each pipeline run is a database write and a gear roll on the world thread.
            std::size_t budget = run.members.size() > 5 ? kStageBatch : run.members.size();
            for (Member& m : run.members)
            {
                if (m.ready) continue;
                if (!budget)
                {
                    all = false;
                    continue;
                }
                --budget;
                m.ready = StageMember(run, m);
                all = all && m.ready;
            }
            if (!run.monitor->failures().empty())
            {
                Finish(run, false);   // a character that cannot be made: the run cannot start
                return;
            }
            if (all && (run.def->parity == 8 || run.def->parity == 9))
            {
                // The dps scorecard: the whole raid on the staging spot, grouped, then on flat ground, then the measurement begins.
                bool placed = true;
                for (Member const& m : run.members)
                {
                    Player* p = Find(m.id);
                    if (!p || !p->IsInWorld() || p->IsBeingTeleported())
                    {
                        placed = false;
                        continue;
                    }
                    if (run.group_guid.IsEmpty() && (p->GetMapId() != kStageMap || p->GetDistance2d(kStageX, kStageY) > 30.f))
                    {
                        ::Playerbot::BotMovement::SafeTeleport(p, kStageMap, kStageX, kStageY, kStageZ, 0.f, 0);
                        placed = false;
                    }
                }
                if (!placed)
                {
                    if (run.phase_ms >= run.stage_timeout_ms)
                    {
                        run.monitor->NoteSetupFailure(run.run_ms, "", "the raid did not reach the staging spot");
                        Finish(run, false);
                    }
                    return;
                }
                if (run.group_guid.IsEmpty())
                {
                    FormGroup(run);
                    return;
                }
                if (!GroupSettled(run))
                {
                    if (run.phase_ms >= run.stage_timeout_ms + 120u * 1000u)
                    {
                        run.monitor->NoteSetupFailure(run.run_ms, "", "the raid group did not settle");
                        Finish(run, false);
                    }
                    return;
                }
                Player* leader = nullptr;
                for (Member const& m : run.members)
                    if (m.def.role == ScenarioRoleKind::Tank)
                        leader = Find(m.id);
                if (!leader)
                    leader = Find(run.members.front().id);
                float sx = 0.f, sy = 0.f, sz = 0.f, so = 0.f;
                if (leader && run.spot_moved == 0 && Parity::FindOpenSpot(leader, 15.0f, sx, sy, sz, so) && leader->GetExactDist(sx, sy, sz) > 1.5f)
                {
                    TC_LOG_INFO("playerbot.v2", "[scenario] {} measures from flat ground at {:.1f},{:.1f},{:.1f}", run.def->name, sx, sy, sz);
                    for (Member const& m : run.members)
                        if (Player* p = Find(m.id))
                            ::Playerbot::BotMovement::SafeTeleport(p, kStageMap, sx, sy, sz, so, 0);
                    run.spot_moved = 1;
                    return;
                }
                std::vector<Parity::GroupMember> raid;
                for (Member const& m : run.members)
                {
                    Player* p = Find(m.id);
                    if (!p)
                        continue;
                    Parity::GroupMember gm;
                    gm.player = p;
                    gm.spec = m.def.spec;
                    gm.label = m.def.name;
                    gm.melee = m.def.melee;
                    gm.role = m.def.role == ScenarioRoleKind::Tank ? 2 : m.def.role == ScenarioRoleKind::Healer ? 1 : 0;
                    gm.scored = run.def->parity == 9 || m.def.role == ScenarioRoleKind::Dps;   // the support scorecard audits everyone
                    raid.push_back(std::move(gm));
                }
                std::string error;
                if (!Parity::BeginGroup(raid, run.def->parity_leg_s, error, run.def->parity == 9))
                {
                    run.monitor->NoteSetupFailure(run.run_ms, "", error.empty() ? "the raid could not begin" : error);
                    Finish(run, false);
                    return;
                }
                TC_LOG_INFO("playerbot.v2", "[scenario] {} {} bots staged and grouped, the measurement begins", run.def->name, raid.size());
                run.phase = Phase::Parity;
                run.phase_ms = 0;
                return;
            }
            if (all && run.def->parity)
            {
                // One bot, no group, no instance: it stays where it was staged and meets a target dummy.
                Player* subject = Find(run.members.front().id);
                // On the open ground of the staging spot, not wherever the bot logged out.
                std::uint32_t stage_map = kStageMap; float stage_x = kStageX, stage_y = kStageY, stage_z = kStageZ;
                StagePoint(subject, stage_map, stage_x, stage_y, stage_z, true);
                if (subject && run.phase_ms < 60u * 1000u &&
                    (subject->GetMapId() != stage_map || subject->GetDistance2d(stage_x, stage_y) > 30.f || subject->IsBeingTeleported()))
                {
                    if (!subject->IsBeingTeleported())
                        ::Playerbot::BotMovement::SafeTeleport(subject, stage_map, stage_x, stage_y, stage_z, 0.f, 0);
                    return;
                }
                // pet_revive_hunter: no dummy, the runner plays the check itself (PetReviveCheck.h).
                if (run.def->parity == PetRevive::kParityKind)
                {
                    std::string error;
                    if (!PetRevive::Begin(subject, error))
                    {
                        run.monitor->NoteSetupFailure(run.run_ms, run.members.front().def.name, error.empty() ? "the bot is gone" : error);
                        Finish(run, false);
                        return;
                    }
                    run.phase = Phase::Parity;
                    run.phase_ms = 0;
                    return;
                }
                // The staging spot is a ledge: a dummy 15 y ahead stands 11 y lower with no line of sight. Measure from flat ground nearby.
                if (subject && !run.flat_ground_done && run.phase_ms < 120u * 1000u && !subject->IsBeingTeleported())
                {
                    float sx = 0.f, sy = 0.f, sz = 0.f, so = 0.f;
                    if (Parity::FindOpenSpot(subject, 15.0f, sx, sy, sz, so))
                    {
                        if (subject->GetExactDist(sx, sy, sz) > 1.5f)
                        {
                            TC_LOG_INFO("playerbot.v2", "[scenario] {} measures from flat ground at {:.1f},{:.1f},{:.1f} ({:.1f} y from the staging spot)", subject->GetName(), sx, sy, sz,
                                        subject->GetExactDist(sx, sy, sz));
                            // Once only, and remembered: each search starts from where the bot stands, so a bot nudged off the ground by its idle
                            // rules between ticks was moved again and again, 30 y, 6 y, 24 y, down into the harbour water (162: it drowned).
                            run.flat_ground_done = true;
                            Parity::NoteFlatSpot(subject->GetMapId(), sx, sy, sz, so);
                            ::Playerbot::BotMovement::SafeTeleport(subject, subject->GetMapId(), sx, sy, sz, so, 0);
                            return;
                        }
                        run.flat_ground_done = true;
                        Parity::NoteFlatSpot(subject->GetMapId(), sx, sy, sz, so);
                        subject->SetFacingTo(so);
                    }
                    else
                        TC_LOG_INFO("playerbot.v2", "[scenario] {}: no flat ground within 48 y of the staging spot, measuring from where it stands", subject->GetName());
                }
                std::string error;
                if (!subject || !Parity::Begin(subject, Parity::Kind(run.def->parity), run.def->parity_leg_s, error, run.def->parity_spec))
                {
                    run.monitor->NoteSetupFailure(run.run_ms, run.members.front().def.name, error.empty() ? "the bot is gone" : error);
                    Finish(run, false);
                    return;
                }
                TC_LOG_INFO("playerbot.v2", "[scenario] {} {} staged, the measurement begins", run.def->name, subject->GetName());
                run.phase = Phase::Parity;
                run.phase_ms = 0;
                return;
            }
            if (all)
            {
                ResetLocks(run);
                FormGroup(run);
                ForgetRecentInstance(run);
                for (Member const& m : run.members)
                    if (Player* p = Find(m.id))
                        ::Playerbot::BotMovement::SafeTeleport(p, run.def->map, run.def->x, run.def->y, run.def->z, run.def->o, 0);
                TC_LOG_INFO("playerbot.v2", "[scenario] {} squad staged and grouped, sent to map {}", run.def->name, run.def->map);
                run.phase = Phase::Enter;
                run.phase_ms = 0;
            }
            else if (run.phase_ms >= run.stage_timeout_ms)
            {
                run.monitor->NoteSetupFailure(run.run_ms, "", "squad not ready after " + std::to_string(run.stage_timeout_ms / 1000u) + " s: " + run.stage_status);
                Finish(run, false);
            }
            return;
        }
        case Phase::Enter:
        {
            bool all_in = true;
            std::string missing;   // who is not on the map and why, for the failure line
            for (Member& m : run.members)
            {
                Player* p = Find(m.id);
                // On the map and done teleporting: a member still finishing the entry teleport drops the landing teleport sent the next second
                // (bwl_razorgore 4f7b18e3: Mcwarra reached the entrance one second before the landing and stayed there, 39 of 40 arrived).
                if (p && p->IsInWorld() && p->GetMapId() == run.def->map && !p->IsBeingTeleported())
                {
                    if (!m.arrival_logged)
                    {
                        m.arrival_logged = true;
                        LogPlace(run, p, "on landing");
                    }
                    continue;
                }
                all_in = false;
                // The first teleport of a member can be dropped (it was mid-teleport, in combat, dead, or its session was between maps):
                // every step sends it again until it lands (Mc raid 2026-10-10: 2 of 40 never left the staging map).
                char const* why = "offline";
                if (p)
                {
                    if (!p->IsInWorld()) why = "not in world";
                    else if (p->IsBeingTeleported()) why = "teleporting";
                    else if (p->GetLevel() != run.def->level) why = "wrong level";
                    else if (!p->IsAlive()) why = "dead";
                    else if (p->IsInCombat()) why = "in combat";
                    else
                    {
                        why = "teleport resent";
                        if (!::Playerbot::BotMovement::SafeTeleport(p, run.def->map, run.def->x, run.def->y, run.def->z, run.def->o, 0))
                        {
                            // The map's own entry check names the refusal (instance full, encounter in progress, another group's instance,
                            // the hourly instance limit, access requirements): the bare "refused" hid it for three runs.
                            TransferAbortParams const denied = Map::PlayerCannotEnter(run.def->map, p);
                            run.refusal = fmt::format("transfer abort reason {} (arg {})", uint32(denied.Reason), uint32(denied.Arg));
                            why = denied ? run.refusal.c_str() : "teleport refused, the entry check passed";
                            if (!run.refusal_logged)
                            {
                                run.refusal_logged = true;
                                TC_LOG_WARN("playerbot.v2", "[scenario] {} {} cannot enter map {}: {}", run.def->name, m.def.name, run.def->map, run.refusal);
                            }
                        }
                    }
                }
                missing += fmt::format("{}{}(level {}, {}, map {})", missing.empty() ? "" : ", ", m.def.name, p ? uint32(p->GetLevel()) : 0u, why,
                                       p ? p->GetMapId() : 0u);
            }
            run.stage_status = missing;
            bool const group_whole = all_in && GroupSettled(run);
            if (all_in && !group_whole)
                run.stage_status = "all on the map, the squad group is not whole yet";
            if (group_whole && run.arrive_entry)
            {
                run.phase = Phase::Arrive;
                run.phase_ms = 0;
                TC_LOG_INFO("playerbot.v2", "[scenario] {} squad is inside map {}, moving it to the room of creature {}", run.def->name, run.def->map,
                            run.arrive_entry);
            }
            else if (group_whole)
            {
                run.phase = Phase::Run;
                run.phase_ms = 0;
                run.run_ms = 0;
                BaseSupport(run);
                DetachStandIn(run);
                TakeLead(run);
                GiveSquadOrders(run, true);
                LogGear(run);
                TC_LOG_INFO("playerbot.v2", "[scenario] {} squad is inside map {}, run begins", run.def->name, run.def->map);
                for (Member const& m : run.members)
                    LogPlace(run, Find(m.id), "at the run's start");
            }
            else if (run.phase_ms >= kEnterTimeoutMs)
            {
                run.monitor->NoteSetupFailure(run.run_ms, "", "squad did not arrive on map " + std::to_string(run.def->map) + ": " + run.stage_status);
                Finish(run, false);
            }
            return;
        }
        case Phase::Arrive:
        {
            Player* leader = MainTank(run);
            if (!leader)
                leader = Find(run.members.front().id);
            if (!run.arrive_sent)
            {
                if (leader && !run.trash_cleared && !run.checkpoint && run.def->arrive_near_entry)
                {
                    run.trash_cleared = true;
                    ClearRaidTrash(run, leader);
                    EnsureBossAlive(run, leader);
                }
                if (!leader || !PickArrival(run, leader))
                {
                    run.monitor->NoteSetupFailure(run.run_ms, "", fmt::format("no floor in sight of creature {} within 32-64 y of its spawn at {:.1f},{:.1f},{:.1f} and clear of hostiles: cannot place the raid{}",
                                                  run.arrive_entry, run.boss_x, run.boss_y, run.boss_z, run.arrive_blocker));
                    Finish(run, false);
                    return;
                }
                for (Member const& m : run.members)
                    if (Player* p = Find(m.id))
                    {
                        // A walk or follow still running from the staging is cancelled with the landing, as a taxi flight is: mc_garr and mc_geddon
                        // had the healers walking 7 y/s from the landing spot under the Hold, rule idle:owner_holding,
                        // and pulled the boss before the order.
                        p->GetMotionMaster()->Clear();
                        p->StopMoving();
                        ::Playerbot::BotMovement::SafeTeleport(p, run.def->map, run.arrive_x, run.arrive_y, run.arrive_z, run.arrive_o, 0);
                    }
                run.arrive_sent = true;
                // Hold from the moment of the landing, not from "in place": the 40 raiders arrive over seconds, and until the Hold
                // began they walked toward Garr and into his adds (MC Garr 2026-10-10: the first Firesworn engagements came with the
                // nearest raider 8-14 y from them, 3 s into the run, 37-50 y from the landing spot). The Hold outlasts the arrival wait
                // and is renewed when the run starts.
                if (!run.checkpoint)   // a dungeon checkpoint is bot-led from the first second
                    for (Member const& m : run.members)
                        if (BotAI* ai = ::Playerbot::Services::Registry().ai(m.id))
                            ai->enter_manual(BotAI::OwnerCommand::Hold, ObjectGuid::Empty, GameTime::GetGameTimeMS(), kArriveTimeoutMs + run.def->order_delay_ms);
                run.phase_ms = 0;
                TC_LOG_INFO("playerbot.v2", "[scenario] {} raid sent to {:.1f},{:.1f},{:.1f}, {:.0f} y from creature {} at {:.1f},{:.1f},{:.1f}", run.def->name,
                            run.arrive_x, run.arrive_y, run.arrive_z, std::hypot(run.arrive_x - run.boss_x, run.arrive_y - run.boss_y),
                            run.arrive_entry, run.boss_x, run.boss_y, run.boss_z);
                return;
            }
            if (leader)
                LogEngagements(run, leader);
            LogLanding(run);
            std::size_t arrived = 0;
            // A member the landing teleport did not move is sent again every 5 s, as the entry teleport is (Phase::Enter); the core drops a
            // teleport now and then (one that comes while another is being finished).
            bool const resend = run.phase_ms - run.landing_resend_ms >= 5000u;
            if (resend)
                run.landing_resend_ms = run.phase_ms;
            for (Member const& m : run.members)
            {
                Player* p = Find(m.id);
                if (p && p->IsInWorld() && !p->IsBeingTeleported() && p->GetMapId() == run.def->map &&
                    std::hypot(p->GetPositionX() - run.arrive_x, p->GetPositionY() - run.arrive_y) < 40.f)
                    ++arrived;
                else if (resend && p && p->IsInWorld() && !p->IsBeingTeleported() && p->GetMapId() == run.def->map && p->IsAlive())
                {
                    p->GetMotionMaster()->Clear();
                    p->StopMoving();
                    bool const sent = ::Playerbot::BotMovement::SafeTeleport(p, run.def->map, run.arrive_x, run.arrive_y, run.arrive_z, run.arrive_o, 0);
                    TC_LOG_INFO("playerbot.v2", "[scenario] {} {} is {:.0f} y from the landing spot at {:.1f},{:.1f},{:.1f} {} s after it was sent: landing teleport sent again ({})",
                                run.def->name, m.def.name, std::hypot(p->GetPositionX() - run.arrive_x, p->GetPositionY() - run.arrive_y),
                                p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), run.phase_ms / 1000u, sent ? "accepted" : "refused");
                }
            }
            if (arrived == run.members.size() && GroupSettled(run))
            {
                // Full health and mana at the moment the run starts: staging topped them up minutes ago (the entry teleports, the
                // trash clear and the walk-in came after), and an Mc raid's healers began the fight at 51 % mana (Garr 2026-10-10).
                for (Member const& m : run.members)
                    if (Player* p = Find(m.id); p && p->IsAlive())
                    {
                        p->SetFullHealth();
                        if (p->GetPowerType() == POWER_MANA)
                            p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                        // A rested squad also has its long cooldowns back: a raid waits them out between pulls, and a run started minutes
                        // after the last one inherited its Innervates (6 min) and mana potions (n10 run 4: both druids dry at 125 s with
                        // the Innervate still down from run 3).
                        p->GetSpellHistory()->ResetAllCooldowns();
                    }
                // A raid's squad holds its place until the Attack, as bots do for a human owner who has not given it yet. A dungeon
                // checkpoint is bot-led from the first second: no Hold, no ordered attack.
                if (!run.checkpoint)
                    for (Member const& m : run.members)
                        if (BotAI* ai = ::Playerbot::Services::Registry().ai(m.id))
                            ai->enter_manual(BotAI::OwnerCommand::Hold, ObjectGuid::Empty, GameTime::GetGameTimeMS(), run.def->order_delay_ms);
                run.phase = Phase::Run;
                run.phase_ms = 0;
                run.run_ms = 0;
                BaseSupport(run);
                if (run.checkpoint)
                {
                    DetachStandIn(run);
                    TakeLead(run);
                }
                LogGear(run);
                if (!run.checkpoint)
                    StockRaidKits(run);
                if (run.checkpoint)
                    TC_LOG_INFO("playerbot.v2", "[scenario] {} squad is in place at checkpoint {}, run begins", run.def->name, run.from_key);
                else
                    TC_LOG_INFO("playerbot.v2", "[scenario] {} raid is in place, run begins; the attack is ordered at {} s", run.def->name, run.def->order_delay_ms / 1000u);
                if (!run.checkpoint)
                    for (Member const& m : run.members)
                        if (Player* p = Find(m.id))
                        {
                            if (run.trash_cleared)
                                KillTrashSummons(run, p);
                            LogNewcomers(run, p, "at the placement");
                            run.newcomers_next_ms = 2000u;
                            break;
                        }
            }
            else if (run.phase_ms >= kArriveTimeoutMs)
            {
                run.monitor->NoteSetupFailure(run.run_ms, "", fmt::format("only {} of {} raiders arrived at the boss room", arrived, run.members.size()));
                Finish(run, false);
            }
            return;
        }
        case Phase::Run:
        {
            if (!run.checkpoint && !run.order_sent && run.newcomers_next_ms && run.run_ms >= run.newcomers_next_ms)
            {
                run.newcomers_next_ms += 2000u;
                for (Member const& m : run.members)
                    if (Player* p = Find(m.id))
                    {
                        if (run.trash_cleared)
                            KillTrashSummons(run, p);
                        LogNewcomers(run, p, fmt::format("{} s after the placement", run.run_ms / 1000u).c_str());
                        break;
                    }
            }
            bool attack_due = run.def->order_attack && !run.order_sent && run.run_ms >= run.def->order_delay_ms;
            // The Attack waits for the raid's preparation first: the class buffs, blessings, flasks, elixirs and food the upkeep rules put up
            // at the landing (naxx_patchwerk 124: 40 raiders, the 20 s order came with the tanks unbuffed and nothing drunk). Bounded like
            // the mana wait below; a pack that comes early ends it, and the Hold is kept up meanwhile.
            if (attack_due && !run.checkpoint && run.run_ms < run.def->order_delay_ms + kPrepWaitMs)
            {
                std::string owed;
                std::uint32_t const n = PrepOwed(run, owed);
                bool in_combat = false;
                for (Member const& m : run.members)
                    if (Player* p = Find(m.id); p && p->IsAlive() && p->IsInCombat())
                        in_combat = true;
                if (n && !in_combat)
                {
                    attack_due = false;
                    for (Member const& m : run.members)
                        if (BotAI* ai = ::Playerbot::Services::Registry().ai(m.id))
                            ai->enter_manual(BotAI::OwnerCommand::Hold, ObjectGuid::Empty, GameTime::GetGameTimeMS(), 30000u);
                    run.monitor->NoteDeliberateWait(run.run_ms);
                    if (!run.prep_wait_log_ms || run.run_ms - run.prep_wait_log_ms >= 10000u)
                    {
                        run.prep_wait_log_ms = run.run_ms ? run.run_ms : 1u;
                        TC_LOG_INFO("playerbot.v2",
                                    "[scenario] {} the attack waits for the raid's buffs and consumables: {} raiders owe one ({}), {} s past the order time",
                                    run.def->name, n, owed, (run.run_ms - run.def->order_delay_ms) / 1000u);
                    }
                }
            }
            if (attack_due && !run.checkpoint)
            {
                // The Attack waits for the healers' mana, as a raid leader waits for a drink: the buffs, the consumables and the
                // trash-free wait before the pull cost the healers 20 % (MC Garr 2026-10-10: 80 % at the pull after a full refill at
                // 'in place'). Bounded, so a squad that cannot drink (no water) still gets its run; the Hold is kept up meanwhile.
                int lowest = 100;
                bool in_combat = false;   // a pack came early: the pull has happened, the wait is over
                for (Member const& m : run.members)
                {
                    Player* p = Find(m.id);
                    if (!p || !p->IsAlive())
                        continue;
                    in_combat = in_combat || p->IsInCombat();
                    if (m.def.role == ScenarioRoleKind::Healer && p->GetPowerType() == POWER_MANA && p->GetMaxPower(POWER_MANA))
                        lowest = std::min(lowest, int(100 * uint64(p->GetPower(POWER_MANA)) / p->GetMaxPower(POWER_MANA)));
                }
                if (in_combat && lowest < kPullManaPct && !run.mana_wait_log_ms)
                    TC_LOG_INFO("playerbot.v2", "[scenario] {} the raid is in combat before the attack: no wait for the healers' mana (lowest {}%)", run.def->name, lowest);
                if (!in_combat && lowest < kPullManaPct && run.run_ms < run.def->order_delay_ms + kManaWaitMs)
                {
                    for (Member const& m : run.members)
                        if (BotAI* ai = ::Playerbot::Services::Registry().ai(m.id))
                            ai->enter_manual(BotAI::OwnerCommand::Hold, ObjectGuid::Empty, GameTime::GetGameTimeMS(), 30000u);
                    run.monitor->NoteDeliberateWait(run.run_ms);
                    if (run.run_ms - run.mana_wait_log_ms >= 10000u || !run.mana_wait_log_ms)
                    {
                        run.mana_wait_log_ms = run.run_ms ? run.run_ms : 1u;
                        TC_LOG_INFO("playerbot.v2", "[scenario] {} the attack waits for the healers' mana: lowest {}% (needs {}%), {} s past the order time",
                                    run.def->name, lowest, kPullManaPct, (run.run_ms - run.def->order_delay_ms) / 1000u);
                        // Who the wait is on besides the healers: the least healthy member and the three emptiest mana pools, with what each bot logic last
                        // decided (MC Ragnaros 2026-10-13 4839bcf3: hunters, mages and warlocks at 4-16 % mana before any fight).
                        std::string hurt, dry;
                        int hurt_pct = 101;
                        std::vector<std::pair<int, std::string>> pools;
                        for (Member const& m : run.members)
                        {
                            Player* p = Find(m.id);
                            if (!p || !p->IsAlive())
                                continue;
                            BotAI* ai = ::Playerbot::Services::Registry().ai(m.id);
                            char const* rule = ai && ai->last_rule_fired() ? ai->last_rule_fired() : "-";
                            int const hp = p->GetMaxHealth() ? int(100 * uint64(p->GetHealth()) / p->GetMaxHealth()) : 100;
                            if (hp < hurt_pct)
                            {
                                hurt_pct = hp;
                                hurt = fmt::format("{} {}% rule={}", m.def.name, hp, rule);
                            }
                            if (p->GetPowerType() == POWER_MANA && p->GetMaxPower(POWER_MANA))
                                pools.emplace_back(int(100 * uint64(p->GetPower(POWER_MANA)) / p->GetMaxPower(POWER_MANA)),
                                                   fmt::format("{} {}% rule={}", m.def.name, int(100 * uint64(p->GetPower(POWER_MANA)) / p->GetMaxPower(POWER_MANA)), rule));
                        }
                        std::sort(pools.begin(), pools.end());
                        for (size_t i = 0; i < pools.size() && i < 3; ++i)
                            dry += (dry.empty() ? "" : "; ") + pools[i].second;
                        TC_LOG_INFO("playerbot.v2", "[scenario] {} wait detail: least healthy {}; emptiest mana {}", run.def->name, hurt.empty() ? "-" : hurt,
                                    dry.empty() ? "-" : dry);
                    }
                    attack_due = false;
                }
            }
            // A boss the raid fights after others (ScenarioDef::start_instance_data with no start_npc: Hakkar after the five High Priests): the
            // instance gets their finished states at the attack, as the raid's own kills would have set them.
            if (attack_due && !run.def->start_npc && !run.def->start_instance_data.empty() && !run.start_data_set)
                if (Player* tank = MainTank(run))
                {
                    run.start_data_set = true;
                    InstanceScript* is = tank->GetInstanceScript();
                    std::string set;
                    if (is)
                        for (auto const& [type, value] : run.def->start_instance_data)
                        {
                            is->SetData(type, value);
                            set += fmt::format(" {}={}", type, value);
                        }
                    TC_LOG_INFO("playerbot.v2", "[scenario] {} instance data set before the attack:{}", run.def->name,
                                !set.empty() ? set : std::string(" none (no instance script)"));
                }
            // A fight the leader starts by talking to a creature (ScenarioDef::start_npc: Vaelastrasz's gossip, Lord Victor Nefarius's): the
            // instance gets the earlier steps the runner skips, the main tank talks once the creature offers it, then the attack waits for the
            // boss to turn hostile (Vaelastrasz), or for the boss the fight summons later (Nefarian) while the raid fights on its own.
            if (attack_due && run.def->start_npc)
            {
                Player* tank = MainTank(run);
                auto const hold = [&run](std::uint32_t ms)
                {
                    for (Member const& m : run.members)
                        if (BotAI* ai = ::Playerbot::Services::Registry().ai(m.id))
                            ai->enter_manual(BotAI::OwnerCommand::Hold, ObjectGuid::Empty, GameTime::GetGameTimeMS(), ms);
                    run.monitor->NoteDeliberateWait(run.run_ms);
                };
                if (!run.start_npc_done_ms && tank)
                {
                    if (!run.start_npc_wait_ms)
                    {
                        run.start_npc_wait_ms = run.run_ms ? run.run_ms : 1u;
                        InstanceScript* is = tank->GetInstanceScript();
                        std::string set;
                        if (is)
                            for (auto const& [type, value] : run.def->start_instance_data)
                            {
                                is->SetData(type, value);
                                set += fmt::format(" {}={}", type, value);
                            }
                        TC_LOG_INFO("playerbot.v2", "[scenario] {} instance data set before the start:{}", run.def->name,
                                    !set.empty() ? set : is ? std::string(" none") : std::string(" none (no instance script)"));
                    }
                    Creature* npc = tank->FindNearestCreature(run.def->start_npc, 200.0f, true);
                    bool const offers = npc && (!run.def->start_gossip_steps || npc->HasNpcFlag(UNIT_NPC_FLAG_GOSSIP));
                    if (!offers && run.run_ms - run.start_npc_wait_ms < 60000u)
                    {
                        hold(5000u);
                        attack_due = false;
                    }
                    else
                    {
                        run.start_npc_done_ms = run.run_ms ? run.run_ms : 1u;
                        std::string how;
                        if (!npc)
                            how = "none alive within 200 y";
                        else if (!npc->AI())
                            how = "it has no script";
                        else
                        {
                            if (!offers)
                                how = "no gossip offered after 60 s, talked anyway; ";
                            if (run.def->start_gossip_steps)
                            {
                                bool const hello = npc->AI()->OnGossipHello(tank);
                                how += fmt::format("gossip {}", hello ? "opened" : "refused by the script");
                                for (std::uint8_t step = 0; hello && step < run.def->start_gossip_steps; ++step)
                                {
                                    GossipMenu& menu = tank->PlayerTalkClass->GetGossipMenu();
                                    if (menu.Empty())
                                    {
                                        how += fmt::format(", no option at step {}", step + 1);
                                        break;
                                    }
                                    std::uint32_t const index = menu.GetMenuItems().front().OrderIndex;
                                    std::uint32_t const menu_id = menu.GetMenuId();
                                    npc->AI()->OnGossipSelect(tank, menu_id, index);
                                    how += fmt::format(", option {} of menu {}", index, menu_id);
                                }
                            }
                            if (run.def->start_npc_action)
                            {
                                npc->AI()->DoAction(run.def->start_npc_action);
                                how += fmt::format("{}script action {}", how.empty() ? "" : ", ", run.def->start_npc_action);
                            }
                        }
                        // A boss that comes later: the raid fights what the start brings (the drakonids) on its own from here, as a raid does
                        // before its leader's Attack on the boss.
                        if (run.def->goal_wait_ms)
                            for (Member const& m : run.members)
                                if (BotAI* ai = ::Playerbot::Services::Registry().ai(m.id))
                                    if (ai->holding_position(GameTime::GetGameTimeMS()))
                                        ai->exit_manual();
                        TC_LOG_INFO("playerbot.v2", "[scenario] {} {} talks to creature {} to start the fight: {}", run.def->name, tank->GetName(),
                                    run.def->start_npc, how);
                    }
                }
                if (attack_due && run.start_npc_done_ms)
                {
                    Creature* boss = FindBoss(run);
                    std::uint32_t const since = run.run_ms - run.start_npc_done_ms;
                    if (run.def->goal_wait_ms)
                    {
                        if (!boss && since < run.def->goal_wait_ms)
                        {
                            run.monitor->NoteDeliberateWait(run.run_ms);
                            if (!run.goal_wait_log_ms || run.run_ms - run.goal_wait_log_ms >= 30000u)
                            {
                                run.goal_wait_log_ms = run.run_ms ? run.run_ms : 1u;
                                TC_LOG_INFO("playerbot.v2", "[scenario] {} the raid fights the start of the encounter; creature {} ({}) is not out yet, {} s after the start",
                                            run.def->name, run.def->goal_entry, run.def->goal_name, since / 1000u);
                            }
                            attack_due = false;
                        }
                        else if (!boss)
                            TC_LOG_WARN("playerbot.v2", "[scenario] {} creature {} is still not out {} s after the start", run.def->name,
                                        run.def->goal_entry, since / 1000u);
                    }
                    else
                    {
                        bool const ready = boss && tank && boss->IsHostileTo(tank) && !boss->IsImmuneToPC() && !boss->IsUninteractible() &&
                                           !boss->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                        if (!ready && since < 60000u)
                        {
                            hold(5000u);
                            attack_due = false;
                        }
                        else if (!ready)
                            TC_LOG_WARN("playerbot.v2", "[scenario] {} creature {} is still not hostile and attackable 60 s after the start", run.def->name,
                                        run.def->goal_entry);
                    }
                }
            }
            // A fight the leader starts with a game object (Chromaggus' lever): the main tank uses it, then the attack waits until the boss
            // can be attacked. The Hold stays up meanwhile.
            if (attack_due && run.def->start_object)
            {
                Player* tank = MainTank(run);
                Creature* boss = FindBoss(run);
                if (!run.start_object_used_ms && tank)
                {
                    GameObject* go = tank->FindNearestGameObject(run.def->start_object, 200.0f);
                    run.start_object_used_ms = run.run_ms ? run.run_ms : 1u;
                    if (go)
                        go->Use(tank);
                    TC_LOG_INFO("playerbot.v2", "[scenario] {} {} uses game object {}{} to start the fight", run.def->name, tank->GetName(),
                                run.def->start_object, go ? fmt::format(" at {:.1f},{:.1f},{:.1f}", go->GetPositionX(), go->GetPositionY(), go->GetPositionZ())
                                                          : std::string(": none within 200 y"));
                }
                bool const locked = boss && (boss->IsUninteractible() || boss->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE) || boss->IsImmuneToPC());
                if (locked && run.run_ms - run.start_object_used_ms < 30000u)
                {
                    for (Member const& m : run.members)
                        if (BotAI* ai = ::Playerbot::Services::Registry().ai(m.id))
                            ai->enter_manual(BotAI::OwnerCommand::Hold, ObjectGuid::Empty, GameTime::GetGameTimeMS(), 5000u);
                    run.monitor->NoteDeliberateWait(run.run_ms);
                    attack_due = false;
                }
                else if (locked)
                    TC_LOG_WARN("playerbot.v2", "[scenario] {} creature {} is still not attackable 30 s after game object {} was used", run.def->name,
                                run.def->goal_entry, run.def->start_object);
            }
            // A boss that comes out on a game event (Gahz'ranka, the Mudskunk Lure): the main tank sends it, as a player using the lure
            // does, then the attack waits for the boss. Without it the instance script despawns him as he spawns, and the run failed its
            // setup at the attack (release-0130 zg_gahzranka: "alive at -11675.7,-1728.9,-7.1" at the placing, not alive 22 s later).
            if (attack_due && run.def->start_event)
            {
                Player* tank = MainTank(run);
                if (!run.start_event_sent_ms && tank)
                {
                    run.start_event_sent_ms = run.run_ms ? run.run_ms : 1u;
                    GameEvents::Trigger(run.def->start_event, tank, nullptr);
                    TC_LOG_INFO("playerbot.v2", "[scenario] {} {} sends game event {} to bring out creature {}", run.def->name, tank->GetName(),
                                run.def->start_event, run.def->goal_entry);
                }
                Creature* boss = FindBoss(run);
                if (!boss && run.start_event_sent_ms && run.run_ms - run.start_event_sent_ms < 30000u)
                {
                    for (Member const& m : run.members)
                        if (BotAI* ai = ::Playerbot::Services::Registry().ai(m.id))
                            ai->enter_manual(BotAI::OwnerCommand::Hold, ObjectGuid::Empty, GameTime::GetGameTimeMS(), 5000u);
                    run.monitor->NoteDeliberateWait(run.run_ms);
                    attack_due = false;
                }
                else if (!boss && run.start_event_sent_ms)
                    TC_LOG_WARN("playerbot.v2", "[scenario] {} creature {} is still not out 30 s after game event {} was sent", run.def->name,
                                run.def->goal_entry, run.def->start_event);
            }
            if (attack_due && !IssueAttack(run))
            {
                Finish(run, false);
                return;
            }
            if (run.order_sent && !run.tank_probe_done && run.run_ms >= run.attack_run_ms + 6000u)
            {
                run.tank_probe_done = true;
                LogTankProbe(run, "6 s into the fight");
            }
            PlayStandIn(run);
            PlayOwnerSkull(run);
            if (run.def->goal == ScenarioGoalKind::FollowRoute)
                PlayRoute(run);
            else
                PlayLeader(run);
            SwimStandIn(run);
            LogPositions(run);
            LogLanding(run);
            SamplePull(run);
            if (run.run_ms < 30000u)
                if (Player* viewer = Find(run.members.front().id))
                    LogEngagements(run, viewer);
            NoteLogMark(run);
            std::vector<ScenarioSample> samples;
            Sample(run, samples);
            bool terminal = run.monitor->Feed(run.run_ms, samples, /*entered*/ true);
            // A boss that yields when the goal is met (Majordomo evades with his adds dead) is not a reset.
            if (run.def->arrive_near_entry && !run.goal_reached)
            {
                ScenarioBossSample bs;
                if (Creature* boss = FindBoss(run))
                {
                    bs.found = true;
                    bs.alive = boss->IsAlive();
                    bs.in_combat = boss->IsInCombat();
                    bs.evading = boss->IsInEvadeMode();
                    bs.hp_pct = float(boss->GetHealthPct());
                }
                if (run.monitor->NoteBoss(run.run_ms, bs))
                    terminal = true;
            }
            if (run.goal_reached)
                Finish(run, false);
            else if (terminal)
                Finish(run, false);
            else if (run.run_ms >= run.def->timeout_ms)
                Finish(run, true);
            return;
        }
        case Phase::Parity:
        {
            if (run.phase_ms >= run.def->timeout_ms)
            {
                Finish(run, true, fmt::format("{} did not finish in {} s", run.def->name, run.def->timeout_ms / 1000u));
                return;
            }
            std::string failure;
            if (run.def->parity == PetRevive::kParityKind ? PetRevive::Step(failure) : Parity::Step(failure))
            {
                if (failure.empty())
                    run.goal_reached = true;
                else
                    run.monitor->NoteSetupFailure(run.phase_ms, run.members.front().def.name, failure);
                Finish(run, false);
            }
            return;
        }
        case Phase::Idle:
            return;
    }
}

void NoteBotDeath(std::uint64_t guid_low, ScenarioDeath death)
{
    for (auto const& r : g_runs)
    {
        // A death at the landing (phase Arrive) is part of the verdict as much as one in the fight: bwl_broodlord 2026-10-20 lost 35 raiders
        // before the order and the result file named none of them.
        if (r->phase != Phase::Run && r->phase != Phase::Arrive)
            continue;
        for (Member const& m : r->members)
            if (m.id == guid_low)
            {
                death.id = guid_low;
                r->monitor->NoteDeath(r->run_ms, death);
                return;
            }
    }
}

void NoteDamage(Unit* attacker, Unit* victim, std::int32_t amount)
{
    if (g_runs.empty() || amount <= 0 || !attacker)
        return;
    Unit* const owner = attacker->GetCharmerOrOwnerOrSelf();
    if (!owner || !owner->IsPlayer())
        return;
    std::uint64_t const id = owner->GetGUID().GetCounter();
    for (auto const& r : g_runs)
    {
        if (!r->pull_started.load())
            continue;
        auto const it = r->pull.find(id);
        if (it == r->pull.end())
            continue;
        it->second.damage.fetch_add(std::uint64_t(amount), std::memory_order_relaxed);
        it->second.hits.fetch_add(1u, std::memory_order_relaxed);
        if (victim && r->def && victim->GetEntry() == r->def->goal_entry)
            it->second.boss_damage.fetch_add(std::uint64_t(amount), std::memory_order_relaxed);
        return;
    }
}

void NoteCreatureKilled(std::uint32_t map, std::uint32_t entry, bool tapped)
{
    // Squads on the same map do not exist side by side (a wave holds distinct dungeons), so the map and the entry name the run.
    for (auto const& r : g_runs)
    {
        if (r->phase != Phase::Run || !r->def || r->def->goal != ScenarioGoalKind::KillCreature || map != r->def->map)
            continue;
        // A death the raiders had no part in is no kill (bwl_razorgore n24 2026-10-14 passed with Razorgore at 94% and the raid wiped), and
        // neither is one after the whole squad is dead.
        if (!tapped)
            continue;
        bool any_alive = false;
        for (Member const& m : r->members)
            if (Player* p = Find(m.id); p && p->IsInWorld() && p->IsAlive())
            {
                any_alive = true;
                break;
            }
        if (!any_alive)
            continue;
        if (r->def->spare_entry && entry == r->def->spare_entry && !r->goal_reached)
            ++r->spared_killed;
        if (entry == r->def->goal_entry)
            r->goal_reached = true;
        else if (r->def->goal_adds_needed && (entry == r->def->goal_adds_a || entry == r->def->goal_adds_b) && ++r->adds_killed >= r->def->goal_adds_needed)
            r->goal_reached = true;
    }
}

std::string Status()
{
    if (g_runs.empty())
        return "No scenario is running.";
    std::string text;
    for (auto const& rp : g_runs)
    {
        Run const& r = *rp;
        if (r.phase == Phase::Parity)
        {
            text += std::string(text.empty() ? "" : "\n") + fmt::format("Scenario {}: {}", r.def->name, Parity::Status());
            continue;
        }
        char const* phase = r.phase == Phase::Stage ? "staging" : r.phase == Phase::Enter ? "entering" : r.phase == Phase::Arrive ? "moving to the boss room" : "running";
        text += std::string(text.empty() ? "" : "\n") + fmt::format("Scenario {}: {} for {} s, {} failure(s) so far.", r.def->name, phase,
                                       (r.phase == Phase::Run ? r.run_ms : r.phase_ms) / 1000u, r.monitor->failures().size());
        if (r.phase == Phase::Stage && !r.stage_status.empty())
            text += " [" + r.stage_status + "]";
        for (ScenarioFailure const& f : r.monitor->failures())
            text += fmt::format("\n  {} {} at {} s: {}", ToString(f.kind), f.bot.empty() ? "-" : f.bot, f.at_ms / 1000u, f.detail);
    }
    return text;
}

std::string LastResult() { return g_last_result; }

std::string Summary()
{
    if (g_summary.empty())
        return g_all ? "runall: nothing has finished yet." : "No runall has finished.";
    std::string text = g_all ? fmt::format("runall in progress, {} finished, {} to go:", g_summary.size(), g_queue.size()) : "runall:";
    for (std::string const& l : g_summary)
        text += "\n  " + l;
    return text;
}

std::string List()
{
    std::string text = "Scenarios:";
    for (ScenarioDef const& d : ScenarioCatalogue())
        text += fmt::format("\n  {} - {}", d.name, d.summary);
    return text;
}

} // namespace Playerbot::Diagnostics::Scenario
