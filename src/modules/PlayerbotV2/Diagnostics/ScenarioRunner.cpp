#include "ScenarioRunner.h"

#include "DamageParity.h"
#include "ScenarioDefs.h"
#include "../Bot/BotAI.h"
#include "../Bot/BotRegistry.h"
#include "../Bot/ClassicUpkeep.h"
#include "../Fleet/BotCharacterFactory.h"
#include "../Fleet/BotIdentityRegistry.h"
#include "../Fleet/BotPool.h"
#include "../Fleet/BotPopulationManager.h"
#include "../Fleet/BotSetupPipeline.h"
#include "../Services.h"
#include "../Threading/SnapshotPublisher.h"
#include "../Threading/TickScheduler.h"
#include "../Session/BotSessionMgr.h"

#include "CharacterCache.h"
#include "Creature.h"
#include "CreatureGroups.h"
#include "GameTime.h"
#include "Group.h"
#include "GroupMgr.h"
#include "Log.h"
#include "InstanceLockMgr.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "DB2Stores.h"
#include "Map.h"
#include "MapManager.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "Player.h"
#include "PlayerbotAPI.h"
#include "PlayerbotMovement.h"
#include "Util.h"
#include "fmt/format.h"

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

constexpr std::uint32_t kStepMs = 1000;                 // one staging / sampling step a second; setup is never looped on the world thread
constexpr std::uint32_t kStageTimeoutMs = 8u * 60u * 1000u;
constexpr std::uint32_t kStageTimeoutPerBotMs = 15u * 1000u;   // a raid of 40 takes longer to make, level and gear than five
constexpr std::uint32_t kEnterTimeoutMs = 90u * 1000u;
constexpr std::uint32_t kArriveTimeoutMs = 45u * 1000u;
constexpr int kPullManaPct = 95;                           // every healer at or above this before the ordered Attack
constexpr std::uint32_t kManaWaitMs = 120u * 1000u;      // the longest the Attack waits for it
constexpr std::size_t kStageBatch = 4;                           // squads over five: members set up per step, so the world thread keeps ticking

enum class Phase : std::uint8_t { Idle, Stage, Enter, Arrive, Run, Parity };

struct Member
{
    ScenarioMember def;
    std::uint64_t id = 0;
    bool role_pinned = false;
    bool kit_learned = false;   // the class trainer spells of the squad level are taught (LearnLevelKit)
    bool tier_geared = false;   // the Molten Core scenarios' Tier 1 gear is on (see the staging step)
    bool ready = false;
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
    ScenarioDef const* def = nullptr;
    Phase phase = Phase::Idle;
    std::vector<Member> members;
    std::unique_ptr<ScenarioMonitor> monitor;
    std::uint32_t step_acc_ms = 0;
    std::uint32_t phase_ms = 0;          // time in the current phase
    std::uint32_t run_ms = 0;            // time since the run began (what the failure timestamps count)
    bool goal_reached = false;
    std::uint32_t adds_killed = 0;   // ScenarioDef::goal_adds_needed
    std::string stage_status;
    // Raids: the goal boss's first spawn, the spot the squad is moved to, and whether the main tank has been ordered on it.
    bool boss_spawn_known = false;
    float boss_x = 0.f, boss_y = 0.f, boss_z = 0.f, boss_o = 0.f;
    bool arrive_sent = false;
    std::uint32_t landing_log_ms = 0;     // game time of the last landing position round
    std::uint32_t landing_rounds = 0;     // rounds logged (capped)
    std::uint32_t mana_wait_log_ms = 0;   // run_ms of the last 'waiting for the healers' mana' line
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
    bool tank_probe_done = false;        // the tanks' state a few seconds into the fight was logged
    bool stand_in_detached = false;      // the stand-in logic is off and the runner plays it
    ObjectGuid group_guid;               // the squad's group: a member found in another one is put back before the run starts
    bool group_logged = false;
    bool refusal_logged = false;   // the map's refusal of the squad's entry is logged once per run
    std::string refusal;           // its reason, for the failure line
    std::string arrive_blocker;          // why no landing spot was found (the hostile that blocked the nearest candidate)
    std::uint32_t stage_timeout_ms = kStageTimeoutMs;
    // Classic::RaidSupportTotal readings (tranq shots, curses removed, formation slot give-ups) when the run began.
    std::uint32_t base_tranq = 0, base_decurse = 0, base_giveup = 0;
    bool support_based = false;
    // Playerbot.log's size once a second of the run (run_ms, bytes), the last kReportWindowMs of it: where the failure report
    // starts reading the log.
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

void StockRations(Run& run, Player* p)
{
    if (p->GetPowerType() == POWER_MANA)
        StockRation(run, p, kDrinks, std::size(kDrinks));
    StockRation(run, p, kFoods, std::size(kFoods));
}

// One staging step for one member. True once the bot is in the world, at the level, geared, role-pinned, alive and out of any group.
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
        ::Playerbot::BotMovement::SafeTeleport(p, kStageMap, kStageX, kStageY, kStageZ, 0.f, 0);
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

    // Level, gear and spells through the standard setup pipeline, exactly one pipeline step per call. A character set up for another
    // level (the shared dungeon party is re-levelled for each row) has its pipeline marked done: it runs again for this level.
    if (uint32(p->GetLevel()) != def.level)
    {
        ::Playerbot::V2::Fleet::BotSetupPipeline{}.Reset(p);
        p->GiveLevel(def.level);
        p->InitTalentForLevel();
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

    // Molten Core scenarios field the gear the owner's raid wears: the Tier 1 set plus raid pieces up to that tier's item level. The setup
    // pipeline's generated gear is a level-60 green/blue kit, so a boss fight on it runs ~4x too long (mc_garr: healers out of mana in a
    // fight a T1 squad finishes). The mode override stays for the run (the upkeep regears with it) and is dropped in Finish.
    if (!m.tier_geared && def.level >= 60 && std::string_view(def.name).rfind("mc_", 0) == 0)
    {
        ::Playerbot::V2::Fleet::BotPopulationManager::SetGearModeOverride(id, 2);   // Gear::Owned::GearMode::T1
        bool const changed = ::Playerbot::Services::Population().RegearNow(p);
        m.tier_geared = true;
        TC_LOG_INFO("playerbot.v2", "[scenario] {} {} Tier 1 gear (regear changed items: {})", def.name, m.def.name, changed ? "yes" : "no");
        run.stage_status += fmt::format("{}=t1gear ", m.def.name);
        return false;
    }

    // Food and drink in the bags, as the vendor visit of a bot that had walked here would have left them: the upkeep rules
    // eat and drink between pulls only from what they carry (Deadmines run 3: no drink line from the priest at all, while
    // the tank's healer-mana gate held the pull).
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

// The living hostile creature nearest to a point, with its distance: a squad must not land inside any pack's aggro. Spawned creatures
// only (the encounter's own adds are spawn rows too: Garr's Firesworn stand around him and are asleep until somebody comes near).
Creature const* NearestHostile(Map* map, Player* viewer, std::uint32_t skip_entry, float x, float y, float z, float& dist)
{
    Creature const* best = nullptr;
    float best_d2 = std::numeric_limits<float>::max();
    for (auto const& kv : map->GetCreatureBySpawnIdStore())
    {
        Creature const* c = kv.second;
        if (!c || !c->IsAlive() || c->GetEntry() == skip_entry || !c->IsHostileTo(viewer))
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

// A dungeon checkpoint lands the squad among the trash the group would meet anyway (it is bot-led from the first second), so it must
// only be out of every pack's aggro, not 40 y clear: a 5-man has hostiles everywhere (deadmines_full@sneed: the nearest to any candidate
// spot was 1 y away, 40 y clearance never passes). A level 17-22 creature notices from about 20 y at equal level.
constexpr float kCheckpointClearance = 22.f;

// The longest walk from a checkpoint's landing spot to its creature, as a multiple of the straight distance: a spot on the far side of
// a wall or a chasm is in sight (LoS) but not on the route.
constexpr float kCheckpointPathSlack = 1.8f;

// A raid scenario tests the boss fight, not the trash on the way: staging kills every living hostile on the map except the goal boss
// and the creatures formed up with him (Garr's Firesworn), so the instance stands as it does once the trash is cleared. A real kill
// (no loot recipient, so nothing to loot) rather than a despawn: the respawn timers run as in a cleared instance. Returns how many died.
std::size_t ClearRaidTrash(Run& run, Player* viewer)
{
    Map* map = viewer->GetMap();
    if (!map)
        return 0;
    CreatureGroup* boss_formation = nullptr;
    for (auto const& kv : map->GetCreatureBySpawnIdStore())
        if (kv.second && kv.second->GetEntry() == run.def->arrive_near_entry)
        {
            boss_formation = kv.second->GetFormation();
            break;
        }
    std::vector<Creature*> doomed;
    for (auto const& kv : map->GetCreatureBySpawnIdStore())
    {
        Creature* c = kv.second;
        if (!c || !c->IsAlive() || !c->IsHostileTo(viewer) || c->GetEntry() == run.def->arrive_near_entry || c->GetEntry() == run.def->goal_entry ||
            (run.def->keep_entry && c->GetEntry() == run.def->keep_entry) ||
            // The creatures whose deaths are the goal (Majordomo's Flamewakers) stand for the fight.
            (run.def->goal_adds_needed && (c->GetEntry() == run.def->goal_adds_a || c->GetEntry() == run.def->goal_adds_b)))
            continue;
        if (boss_formation && c->GetFormation() == boss_formation)
            continue;
        doomed.push_back(c);
    }
    for (Creature* c : doomed)
        c->KillSelf(false);
    TC_LOG_INFO("playerbot.v2", "[scenario] {} cleared the trash before placing the raid: {} hostile creatures killed (boss entry {} and its formation left alone)",
                run.def->name, doomed.size(), run.def->arrive_near_entry);
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
        TC_LOG_INFO("playerbot.v2", "[scenario] {} landing round {} {}: {:.0f} y from the landing spot, moving {}, hold {}, rule {}", run.def->name,
                    run.landing_rounds, p->GetName(), std::hypot(p->GetPositionX() - run.arrive_x, p->GetPositionY() - run.arrive_y),
                    p->isMoving() ? 1 : 0, ai && ai->holding_position(now) ? 1 : 0, ai && ai->last_rule_fired() ? ai->last_rule_fired() : "-");
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
    ZLiquidStatus const st = map->GetLiquidStatus(ps, x, y, z, map_liquidHeaderTypeFlags::Magma | map_liquidHeaderTypeFlags::Slime, &ld);
    return (st & MAP_LIQUID_STATUS_IN_CONTACT) != 0;
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
        Creature const* near_c = NearestHostile(map, leader, run.arrive_entry, run.stage_x, run.stage_y, run.stage_z, hd);
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
    std::uint32_t path_rejected = 0;
    // A boss that aggroes on sight (arrive_min_yards) is approached from outside its reach; the ring widens from there, the default one
    // (40 y) keeps its old order.
    std::vector<float> rings = { 40.f, 44.f, 48.f, 52.f, 56.f, 60.f, 64.f, 36.f, 32.f };
    if (run.def && run.def->arrive_min_yards > 40.f)
        rings = { run.def->arrive_min_yards, run.def->arrive_min_yards + 4.f, run.def->arrive_min_yards + 8.f, run.def->arrive_min_yards + 12.f,
                  run.def->arrive_min_yards - 4.f };
    for (float dist : rings)
        for (int k = 0; k < 16; ++k)
        {
            int const n = (k + 1) / 2 * (k % 2 ? 1 : -1);              // 0, 1, -1, 2, -2, ...
            float const a = run.boss_o + float(n) * step;
            float const cx = run.boss_x + std::cos(a) * dist;
            float const cy = run.boss_y + std::sin(a) * dist;
            float h = map->GetHeight(ps, cx, cy, run.boss_z + 3.0f, true, 12.0f);
            if (h <= INVALID_HEIGHT || std::fabs(h - run.boss_z) > 6.0f)
                continue;
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
            float hd = 0.f;
            if (Creature const* c = NearestHostile(map, leader, run.arrive_entry, cx, cy, h, hd); c && hd < clearance)
            {
                if (!nearest_blocker || hd < nearest_blocker_d) { nearest_blocker = c; nearest_blocker_d = hd; }
                continue;
            }
            if (!map->isInLineOfSight(ps, cx, cy, h + 2.0f, run.boss_x, run.boss_y, run.boss_z + 2.0f, LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing))
                continue;
            if (run.checkpoint)
            {
                // The squad walks from here to the checkpoint: the spot must be on the route, not merely in sight of it.
                PathGenerator pg(leader);
                ::Playerbot::BotMovement::SehSafeCalculatePathFrom(pg, cx, cy, h, run.boss_x, run.boss_y, run.boss_z);
                float const straight = std::hypot(run.boss_x - cx, run.boss_y - cy);
                if ((pg.GetPathType() & (PATHFIND_NOPATH | PATHFIND_FARFROMPOLY | PATHFIND_INCOMPLETE)) || pg.GetPathLength() > straight * kCheckpointPathSlack + 10.f)
                {
                    ++path_rejected;
                    continue;
                }
            }
            run.arrive_x = cx; run.arrive_y = cy; run.arrive_z = h + 0.5f;
            run.arrive_o = std::atan2(run.boss_y - cy, run.boss_x - cx);
            if (run.arrive_o < 0.f)
                run.arrive_o += 6.2831853f;
            return true;
        }
    if (nearest_blocker)
        run.arrive_blocker = fmt::format("; the nearest hostile to a candidate spot was entry {} {:.0f} y away (clearance {:.0f} y)", nearest_blocker->GetEntry(),
                                         nearest_blocker_d, clearance);
    if (path_rejected)
        run.arrive_blocker += fmt::format("; {} candidate spots had no walkable route to the creature", path_rejected);
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
    if (!p || !run.stand_in_detached || !p->IsAlive() || !tank || tank == p || p->HasUnitState(UNIT_STATE_CASTING))
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
        // No boss alive in the room is a finding about the setup (a lock from the last run kept it dead), not a fight to wait out.
        run.monitor->NoteSetupFailure(run.run_ms, "", tank ? fmt::format("creature {} ({}) is not alive within 400 y of the main tank at the time of the attack: dead from an earlier run, or not spawned",
                                                                         run.def->goal_entry, run.def->goal_name)
                                                           : "no living tank to order the attack");
        return false;
    }
    LogPullState(run);
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
    ::Playerbot::API api(tank);
    ::Playerbot::Result const res = api.start_attack(boss->GetGUID());
    if (BotAI* ai = ::Playerbot::Services::Registry().ai(tank->GetGUID().GetCounter()))
    {
        ai->enter_manual(BotAI::OwnerCommand::Engage, boss->GetGUID(), GameTime::GetGameTimeMS());
        ai->release_owner_lead();
    }
    TC_LOG_INFO("playerbot.v2", "[scenario] {} ordered {} onto {} ({}) at {} s, {:.0f} y away (start_attack result {})", run.def->name,
                tank->GetName(), boss->GetName(), run.def->goal_entry, run.run_ms / 1000u, tank->GetDistance(boss), int(res));
    // A target the server refuses (immune to players, not interactible) is a fault in the setup, not a fight that happens to be quiet.
    if (res == ::Playerbot::Result::InvalidTarget)
        run.monitor->NoteSetupFailure(run.run_ms, "", fmt::format("{} refused the attack order on {} ({}): the boss is not attackable (immune to players or not interactible)",
                                                                   tank->GetName(), boss->GetName(), run.def->goal_entry));
    return true;
}

// ---- The failure report ----
// On a FAIL one block carries what the diagnosis keeps asking the operator to grep for: the module's own log lines of the last
// kReportWindowMs before the first failure (the tags below) and every bot's state when the run ended. Playerbot.log has no
// timestamps, so the runner notes the file's size each second and reads from the mark of the window's start.
constexpr std::uint32_t kReportWindowMs = 60u * 1000u;
constexpr std::size_t kReportMaxLines = 500;
constexpr std::string_view kReportTags[] = {
    "[raid_pull]", "[pull_back]", "[pull_gate]", "[raid_heal]", "[healer_drink]", "[healer_hold]", "[raid_tank]", "[tank_",
    "[raid_hazard]", "[raid_escape]", "[raid_fight]", "[raid_threat]", "[mark_cc]", "[mark_gate]", "[bot_death]", "[spell_dmg]",
    "[interrupt", "[raid_school", "[mark_cc]", "[pvecoord]" };

std::string LogPath() { return sLog->GetLogsDir() + "Playerbot.log"; }

void NoteLogMark(Run& run)
{
    std::error_code ec;
    std::uintmax_t const size = std::filesystem::file_size(LogPath(), ec);
    if (ec)
        return;
    run.log_marks.emplace_back(run.run_ms, size);
    while (run.log_marks.size() > 1 && run.run_ms - run.log_marks.front().first > kReportWindowMs + 5000u)
        run.log_marks.pop_front();
}

// The tagged lines written since the mark at (or after) `from_ms`; at most the last kReportMaxLines.
std::vector<std::string> LogLinesSince(Run const& run, std::uint32_t from_ms)
{
    std::vector<std::string> out;
    if (run.log_marks.empty())
        return out;
    std::uintmax_t start = run.log_marks.back().second;
    for (auto const& [ms, off] : run.log_marks)
        if (ms >= from_ms) { start = off; break; }
    std::ifstream in(LogPath(), std::ios::binary);
    if (!in)
        return out;
    in.seekg(0, std::ios::end);
    std::uintmax_t const end = std::uintmax_t(in.tellg());
    if (start > end)
        start = 0;   // the file was rotated since: read what is there
    in.seekg(std::streamoff(start));
    std::string line;
    while (std::getline(in, line))
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
            out.erase(out.begin());
    }
    return out;
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
        if (def.parity != 1 && !run.members.empty())
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
    }

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

    // A detached stand-in has no bot logic to come back: it is logged out, and the next staging logs it in with a fresh one.
    if (run.stand_in_detached)
        if (Player* si = StandIn(run))
            ::Playerbot::Services::SessionMgr().LogoutBot(si->GetGUID());
    for (Member const& m : run.members)
        if (Player* p = Find(m.id))
        {
            ::Playerbot::V2::Fleet::BotPopulationManager::SetGearModeOverride(m.id, -1);
            LeaveGroup(p);
            p->CombatStop(true);
            if (!p->IsAlive())
            {
                p->ResurrectPlayer(1.0f);
                p->SpawnCorpseBones();
            }
            ::Playerbot::BotMovement::SafeTeleport(p, kStageMap, kStageX, kStageY, kStageZ, 0.f, 0);
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
            if (char const* rule = ai->last_rule_fired())
                s.rule = rule;
        if (p->GetPowerType() == POWER_MANA && p->GetMaxPower(POWER_MANA) > 0)
            s.mana_pct = 100.f * float(p->GetPower(POWER_MANA)) / float(p->GetMaxPower(POWER_MANA));
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
    if (g_runs.empty())
        return;
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
        run.run_ms += diff_ms;
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
            if (all && run.def->parity)
            {
                // One bot, no group, no instance: it stays where it was staged and meets a target dummy.
                Player* subject = Find(run.members.front().id);
                // On the open ground of the staging spot, not wherever the bot logged out.
                if (subject && run.phase_ms < 60u * 1000u &&
                    (subject->GetMapId() != kStageMap || subject->GetDistance2d(kStageX, kStageY) > 30.f || subject->IsBeingTeleported()))
                {
                    if (!subject->IsBeingTeleported())
                        ::Playerbot::BotMovement::SafeTeleport(subject, kStageMap, kStageX, kStageY, kStageZ, 0.f, 0);
                    return;
                }
                std::string error;
                if (!subject || !Parity::Begin(subject, Parity::Kind(run.def->parity), run.def->parity_leg_s, error))
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
            for (Member const& m : run.members)
            {
                Player* p = Find(m.id);
                if (p && p->IsInWorld() && p->GetMapId() == run.def->map)
                    continue;
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
                LogGear(run);
                TC_LOG_INFO("playerbot.v2", "[scenario] {} squad is inside map {}, run begins", run.def->name, run.def->map);
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
                        ::Playerbot::BotMovement::SafeTeleport(p, run.def->map, run.arrive_x, run.arrive_y, run.arrive_z, run.arrive_o, 0);
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
            for (Member const& m : run.members)
            {
                Player* p = Find(m.id);
                if (p && p->IsInWorld() && !p->IsBeingTeleported() && p->GetMapId() == run.def->map &&
                    std::hypot(p->GetPositionX() - run.arrive_x, p->GetPositionY() - run.arrive_y) < 40.f)
                    ++arrived;
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
                    DetachStandIn(run);
                LogGear(run);
                if (run.checkpoint)
                    TC_LOG_INFO("playerbot.v2", "[scenario] {} squad is in place at checkpoint {}, run begins", run.def->name, run.from_key);
                else
                    TC_LOG_INFO("playerbot.v2", "[scenario] {} raid is in place, run begins; the attack is ordered at {} s", run.def->name, run.def->order_delay_ms / 1000u);
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
            bool attack_due = run.def->order_attack && !run.order_sent && run.run_ms >= run.def->order_delay_ms;
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
            if (Parity::Step(failure))
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
        if (r->phase != Phase::Run)
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
