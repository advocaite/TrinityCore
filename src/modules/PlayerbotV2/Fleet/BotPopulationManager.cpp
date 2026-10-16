#include "BotPool.h"
#include "../Util/SlowCall.h"
#include "BotPopulationManager.h"
#include "ClassicSpec.h"
#include "SoakAnchors.h"
#include "CompanionRole.h"
#include "BotQueueFiller.h"
#include "InstanceGroupCap.h"
#include "InstanceEntry.h"
#include "RaidSubgroupPlan.h"
#include "BotSetupPipeline.h"
#include "WorldBotSpawner.h"
#include "BotAccountMgr.h"
#include "BotCharacterFactory.h"
#include "BotComposition.h"
#include "BotIdentityRegistry.h"
#include "BotNamePool.h"
#include "../Services.h"
#include "BotCoordinationBus.h"   // ArenaTeamForming publish (autonomous arena seed)
#include "../Session/BotSessionMgr.h"
#include "../Util/ConfigReader.h"
#include "../Bot/BotIntent.h"
#include "../Bot/BotAI.h"        // BotAI::ActionKind for per-bot rebalance lockout
#include "../Bot/BotRegistry.h"  // walking online bot AIs
#include "../Bot/ClassTables.h"  // IsTankSpec / TankSpecForClass etc.
#include "../Bot/Formation.h"   // companion slots
#include "../Bot/World/HunterPetsByRace.h"   // companion hunter pet
#include "../Threading/IntentQueue.h"
#include "../Threading/SnapshotPublisher.h"   // `.bot delete`: forget the bot before its row goes
#include "../Threading/TickScheduler.h"
#include "CharacterCache.h"
#include "../Diagnostics/PerfCounters.h"
#include "../Bot/Gear/BotGearGenerator.h"   // weaponless re-gear hygiene (audit B15)
#include "../Bot/Gear/OwnedGearPolicy.h"    // companion gear rules
#include "BotItemScorer.h"   // EffectiveItemLevelForLevel — under-gear detection
#include "Battleground.h"
#include "Item.h"   // re-gear hygiene: StoreNewItem/EquipItem need the full type
#include "Bag.h"   // FreeOneJunkBagSlot: GetBagByPos/GetBagSize need the full type
#include "ObjectMgr.h"   // re-gear hygiene: sObjectMgr->GetItemTemplate for ilvl guard
#include "PlayerbotAPI.h"  // IsItemRenderableInSlot - gear heal
#include "BattlegroundMgr.h"
#include "DungeonFinding/LFGMgr.h"   // LFG-state guard on overflow/hygiene kicks
#include "OwnerRegistry.h"           // altbot (owner-bound) kick exemption
#include "WorldSession.h"            // account-id lookup for the altbot test
#include "StringConvert.h"
#include "Util.h"   // Trinity::Tokenize
#include "Config.h"   // sConfigMgr — Playerbot.Bg.AutoSeed.Matches knob
#include "Player.h"
#include "Chat.h"
#include "Group.h"
#include "GroupMgr.h"
#include "PlayerbotMovement.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Log.h"
#include "Util.h"
#include "StringConvert.h"
#include "RaceMask.h"
#include "Random.h"
#include <algorithm>
#include <unordered_set>
#include <tuple>
#include <array>
#include <cstring>
#include <limits>
#include <cmath>

namespace Playerbot::V2::Fleet {

namespace {

// An owner whose group is bigger than the instance they just entered (or the open world they walked out into) is told once; nobody is kicked
// (Playerbot.Behavior.InstanceGroupCap). Owner = a real player with a grouped companion. 2 s cadence.
void WarnOversizedInstanceGroups(uint32 now_ms)
{
    static uint32 last_ms = 0;
    if (last_ms && getMSTimeDiff(last_ms, now_ms) < 2000u)
        return;
    last_ms = now_ms;
    if (!Services::Config().behavior_instance_group_cap())
        return;
    static std::unordered_map<uint64, uint64> warned;   // owner low guid -> (mapId << 32 | instanceId) already warned
    static std::unordered_map<uint64, uint32> over_cap_since;   // open-world owner low guid -> when the companion surplus began
    std::vector<uint64> seen;
    std::vector<ObjectGuid> trim_owners;   // dismissed after the player lock is released (a logout takes it exclusively)
    {
        std::shared_lock<std::shared_mutex> lock(*HashMapHolder<Player>::GetLock());
        for (auto const& kv : ObjectAccessor::GetPlayers())
        {
            Player* p = kv.second;
            if (!p || !p->IsInWorld() || Services::Lifecycle().is_bot(p->GetGUID().GetCounter()))
                continue;
            Group const* g = p->GetGroup();
            Map const* map = p->GetMap();
            if (!map || map->IsBattlegroundOrArena())
                continue;
            bool const open_world = !map->IsDungeon();
            if (!g && !open_world)
                continue;
            if (open_world)
            {
                // Open world: any owner whose companions out exceed the cap, grouped with them or not (a player who logged in
                // with 9 companions still online from before the cap, playtest 2026-10-08).
                // Companions above the open-world cap (called in an instance, or relogged with a big group) are dismissed once
                // the owner has stood outside for kTrimGraceMs. Invited world bots are not owned, so they are never trimmed.
                uint32 const open_cap = InstanceGroupCapFor(p, nullptr);
                uint64 const owner_low = p->GetGUID().GetCounter();
                if (open_cap && CompanionsOut(p) > open_cap)
                {
                    constexpr uint32 kTrimGraceMs = 10'000;
                    auto [oit, fresh] = over_cap_since.try_emplace(owner_low, now_ms);
                    if (!fresh && getMSTimeDiff(oit->second, now_ms) >= kTrimGraceMs)
                        trim_owners.push_back(p->GetGUID());
                }
                else
                    over_cap_since.erase(owner_low);
                continue;
            }
            std::string name;
            uint32 const cap = InstanceGroupCapFor(p, &name);
            // One warning per entry into an instance, and one when the owner walks out into the open world.
            uint64 const key = open_world ? ~uint64(0) : (uint64(map->GetId()) << 32) | map->GetInstanceId();
            uint64 const owner = p->GetGUID().GetCounter();
            seen.push_back(owner);
            if (!cap || g->GetMembersCount() <= cap)
            {
                warned.erase(owner);
                continue;
            }
            auto it = warned.find(owner);
            if (it != warned.end() && it->second == key)
                continue;
            warned[owner] = key;
            ChatHandler(p->GetSession()).PSendSysMessage("Your group has %u members; %s holds %u.",
                                                        g->GetMembersCount(), name.c_str(), cap);
            TC_LOG_INFO("playerbot.v2", "[InstanceCap] {} group {} > {} holds {}", p->GetName(), g->GetMembersCount(), name, cap);
        }
    }
    for (ObjectGuid const& og : trim_owners)
    {
        Player* p = ObjectAccessor::FindPlayer(og);
        if (!p || !p->IsInWorld() || !p->GetMap() || p->GetMap()->IsDungeon() || p->GetMap()->IsBattlegroundOrArena())
            continue;
        uint32 const cap = InstanceGroupCapFor(p, nullptr);
        std::vector<BotId> out_ids;
        CompanionsOut(p, 0, &out_ids);
        if (!cap || out_ids.size() <= cap)
            continue;
        Group* group = p->GetGroup();
        // Keep the ones grouped with the owner first, then the lowest formation slots; the rest go.
        auto rank = [&](BotId id)
        {
            bool const grouped = group && group->IsMember(ObjectGuid::Create<HighGuid::Player>(id));
            BotAI const* ai = Services::Registry().ai(id);
            return std::make_tuple(grouped ? 0 : 1, ai ? int(ai->formation_slot()) : 255, id);
        };
        std::sort(out_ids.begin(), out_ids.end(), [&](BotId a, BotId b) { return rank(a) < rank(b); });
        uint32 dismissed = 0;
        for (size_t i = cap; i < out_ids.size(); ++i)
        {
            ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(out_ids[i]);
            if (group && group->IsMember(guid))
                group->RemoveMember(guid);
            if (Services::SessionMgr().LogoutBot(guid))
                ++dismissed;
        }
        over_cap_since.erase(og.GetCounter());
        if (dismissed)
        {
            ChatHandler(p->GetSession()).PSendSysMessage(
                "%u of your companions were dismissed: outside an instance you can have %u bots out. Invite world bots for more, or call them again inside an instance.",
                dismissed, cap);
            TC_LOG_INFO("playerbot.v2", "[InstanceCap] {} left the instance with {} companions out: {} dismissed (open world holds {})",
                        p->GetName(), uint32(out_ids.size()), dismissed, cap);
        }
    }
    for (auto it = warned.begin(); it != warned.end();)
        it = std::find(seen.begin(), seen.end(), it->first) == seen.end() ? warned.erase(it) : std::next(it);
}

// 60s reconcile. Earlier experiment dropped this to 5s for faster fill;
// that drove kSpawnRatePerTick=5 to ~60 ops/sec which generated more
// async DB writes than CharacterDatabase.WorkerThreads could drain. Queue
// saturated to 888k pending statements, process consumed ~20 GB RAM
// holding the formatted SQL strings, world thread froze waiting on map
// workers fighting MySQL contention. 60s is the right cadence; if a
// faster fill is needed, raise kSpawnRatePerTick instead so the work
// happens in larger batches per cycle (more writes amortize over fewer
// reconcile ticks).
constexpr uint32 kTickIntervalMs      = 60'000;     // 60s reconcile
constexpr uint32 kHygieneIntervalMs   = 3'600'000;  // 1h hygiene
// Rebalance pass: every 5 min walk online bots, count tank/healer/dps per
// (faction, bracket), and proactively switch hybrid DPS bots when the
// bracket's tank/healer count is below the 1:1:3 target ratio. 5 min is a
// good cadence — fast enough to react before a peak-hour /lfg burst, slow
// enough that idle hybrids aren't churning their talent state on noise.
constexpr uint32 kRebalanceIntervalMs = 5u * 60u * 1000u;
// Per-bot rebalance lockout is enforced via the shared
// BotAI::ActionKind::DualSpec retry table (action_retry_lockout_for
// returns 30 min for that kind). Both rebalance and idle:dual_spec_switch
// stamp the same key, so a bot recently switched by either path is
// excluded from the next pass — they coordinate without an extra
// constant here.
// Cap rebalance switches per cycle: 1 per (faction, bracket) per pass. A
// fleet of 2000 has ~10 brackets × 2 factions = 20 switches every 5min,
// which is well under the talent-apply throttle and prevents a mass-respec
// spike if the fleet just logged in.
constexpr uint32 kRebalanceSwitchesPerBracket = 1;
// Per-Reconcile-cycle spawn budget. Was 5 with 60s reconcile = 5/min,
// then bumped indirectly via 5s reconcile (rolled back). Now 25 per
// 60s = 25/min steady-state spawn rate. Each spawn triggers an async
// Player::SaveToDB transaction (~30-50 prepared statements). 25/min
// produces ~1k async writes/min - well within 12 worker threads'
// drain capacity. Bursts during wipefleet still come from the
// 10-cycle ForceReconcile loop = ~250 spawns immediately.
constexpr uint32 kSpawnRatePerTick    = 25;
// Login rate per Reconcile cycle (60s). Each login adds a Player to a Map,
// triggering relocation notifies + lazy navmesh tile loading on a Map worker
// thread. At 20/cycle the bursts saturated one worker (DK bots cluster in
// Acherus map 609 - all hit the same worker thread), pushing tick time past
// the 60s freeze-detector limit on cold-grid loads. 5/cycle gives the map
// updater room to absorb each batch before the next arrives. Population
// fills slower (300/hour) but stays responsive.
// Was 5 per 60s. Bumped to 25 to match the new spawn budget so logging
// in existing offline bots can keep pace with creation. Map worker
// pressure was the original concern, but navmesh prewarm at module
// init + MapUpdate.Threads=16 in worldserver.conf give us headroom
// for higher login concurrency.
constexpr uint32 kLoginRatePerTick    = 25;
constexpr uint32 kJitMaxAgeDays       = 7;
constexpr float  kHardCapMultiplier   = 1.5f;       // total bots ≤ target * 1.5

// Distribution starts at L10. Below L10 there's nothing to participate in
// (no BG/dungeon/raid queues open until L10 anyway), and L1-9 bots clutter
// starter zones without producing any queue value. Bots that level past 10
// via gameplay come from the natural questing pipeline, not distribution.
// Ten-level bands from L10 up to the realm cap, then a {cap, cap} max-level
// band, derived from MaxPlayerLevel() so the cap is never hardcoded:
//   Midnight (cap 90): {10-19} .. {80-89}, {90,90}
//   Classic 1.60 / Forever (cap 60): {10-19} .. {50-59}, {60,60}
// (A fixed 90-cap table made a Classic realm aim 40% of its MaxHeavy fleet at
// a {90,90} band no bot can reach and spawn L65/L75/L85 bots it can't hold.)
std::vector<std::array<uint8, 2>> LevelBuckets()
{
    std::vector<std::array<uint8, 2>> out;
    uint8 const cap = MaxPlayerLevel();
    if (cap <= 10)
        return { { cap, cap } };
    for (uint32 lo = 10; lo < cap; lo += 10)
        out.push_back({ uint8(lo), uint8(std::min<uint32>(lo + 9, cap - 1u)) });
    out.push_back({ cap, cap });
    return out;
}

// #5 session-rhythm: cheap deterministic 64->32 bit mix (splitmix64
// finalizer) for per-bot stagger/jitter without a global RNG. Same input ->
// same output, so a given bot's jitter is stable within a session and
// varies between bots — no Math.random, no shared state, no lock.
inline uint32 MixHash64to32(uint64 x)
{
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return uint32(x);
}

// True when the bot is a real player's ALT rather than population-shaper
// inventory: either explicitly owner-bound (`.playerbot summon` / adopt),
// or living on a NON-pool account (a character the operator/player marked
// and logged in manually, e.g. Somi). Altbots are never kicked by the
// overflow/hygiene/excess passes and don't count toward TotalTarget —
// they ride on top of the ambient population.
bool IsPlayerOwnedAlt(BotId id, Player const* p)
{
    if (Services::Owners().GetOwner(id).account_id != 0)
        return true;
    if (p)
        if (WorldSession const* sess = p->GetSession())
            return !Services::Accounts().is_pool_account(sess->GetAccountId());
    return false;
}

// True when the bot is a player's own character (it lives on a non-pool account: `.playerbot summon` alts, or a
// character marked by hand). Their gear is the player's choice, so the gear passes only ever fill an empty weapon
// slot. A pool bot adopted with `.playerbot adopt` is owner-bound but still the fleet's character: it is geared like
// any other bot (otherwise an adopted bot levelled with `.playerbot level` keeps its starter gear forever).
bool IsPlayersOwnCharacter(Player const* p)
{
    if (p)
        if (WorldSession const* sess = p->GetSession())
            return !Services::Accounts().is_pool_account(sess->GetAccountId());
    return false;
}

// Returns weight per bucket for the configured shape. Sum of weights = 1.0.
std::vector<float> ShapeWeights(PopulationShape shape, size_t n_buckets, float max_heavy_factor)
{
    std::vector<float> w(n_buckets, 0.0f);
    if (n_buckets == 0) return w;

    switch (shape)
    {
        case PopulationShape::MaxHeavy: {
            // last bucket gets max_heavy_factor; remaining N-1 share the rest
            // weighted by descending pyramid (more at low end).
            w[n_buckets - 1] = max_heavy_factor;
            float remain = 1.0f - max_heavy_factor;
            float pyr_sum = 0.0f;
            for (size_t i = 0; i < n_buckets - 1; ++i)
                pyr_sum += float(n_buckets - 1 - i);
            for (size_t i = 0; i < n_buckets - 1; ++i)
                w[i] = remain * float(n_buckets - 1 - i) / pyr_sum;
            break;
        }
        case PopulationShape::Pyramid: {
            float sum = 0.0f;
            for (size_t i = 0; i < n_buckets; ++i)
                sum += float(n_buckets - i);
            for (size_t i = 0; i < n_buckets; ++i)
                w[i] = float(n_buckets - i) / sum;
            break;
        }
        case PopulationShape::Bell: {
            float mid = float(n_buckets) / 2.0f;
            float sum = 0.0f;
            std::vector<float> tmp(n_buckets);
            for (size_t i = 0; i < n_buckets; ++i)
            {
                float x = (float(i) - mid) / (mid * 0.6f);
                tmp[i] = std::exp(-0.5f * x * x);
                sum += tmp[i];
            }
            for (size_t i = 0; i < n_buckets; ++i)
                w[i] = tmp[i] / sum;
            break;
        }
        case PopulationShape::Flat:
        default:
            for (size_t i = 0; i < n_buckets; ++i)
                w[i] = 1.0f / float(n_buckets);
            break;
    }
    return w;
}

PopulationShape ParseShape(std::string const& s)
{
    if (s == "Pyramid")  return PopulationShape::Pyramid;
    if (s == "Bell")     return PopulationShape::Bell;
    if (s == "Flat")     return PopulationShape::Flat;
    return PopulationShape::MaxHeavy;
}

// The 14 BfA+ allied races (incl. Dracthyr). Each is created in a special /
// phased recruitment scenario, and its race-restricted starter chain grants
// racial abilities the bot would otherwise never learn. StarterQuestAutocomplete
// already gathers these via each quest's AllowableRaces mask + a starter-level
// cap (StarterQuestAutocomplete.cpp:58-87), so no per-quest list is needed here.
bool IsAlliedRace(uint32 race)
{
    switch (race)
    {
        // Alliance
        case RACE_VOID_ELF: case RACE_LIGHTFORGED_DRAENEI: case RACE_DARK_IRON_DWARF:
        case RACE_KUL_TIRAN: case RACE_MECHAGNOME: case RACE_EARTHEN_DWARF_ALLIANCE:
        case RACE_DRACTHYR_ALLIANCE:
        // Horde
        case RACE_NIGHTBORNE: case RACE_HIGHMOUNTAIN_TAUREN: case RACE_MAGHAR_ORC:
        case RACE_ZANDALARI_TROLL: case RACE_VULPERA: case RACE_EARTHEN_DWARF_HORDE:
        case RACE_DRACTHYR_HORDE:
            return true;
    }
    return false;
}

// Bots that must run the starter-only rescue (complete the starter quest chain
// + relocate to capital, kept at start level) even with no distribution bucket,
// because they're created in ISOLATED / phased starter zones whose scripted
// intro grants essential class/racial abilities the bot bot logic can't earn by normal
// questing:
//   - Death Knight  → Acherus (map 609)
//   - Demon Hunter  → Mardum  (map 1481)
//   - Dracthyr/Evoker → Forbidden Reach (map 2081)
//   - all 14 allied races → their recruitment scenarios (heritage/racials)
// Until per-race start-level behavior is scripted, a start-level bot here that
// never runs through distribution sits incomplete (missing spells/racials) and
// often stuck in a no-/sparse-navmesh zone.
//
// NOTE: classic base races (Human/Orc/Dwarf/…) and Worgen/Goblin/Pandaren are
// intentionally NOT here — they start on the natural L1-9 questing path; owner
// scoped this to DK/DH/Dracthyr + the 14 allied races (2026-05-29).
bool BotNeedsStarterOnly(Player const* p)
{
    return p && (p->GetClass() == CLASS_DEATH_KNIGHT
              || p->GetClass() == CLASS_DEMON_HUNTER
              || IsAlliedRace(p->GetRace()));
}

bool RaceIsAlliance(uint32 race)
{
    switch (race)
    {
        case RACE_HUMAN: case RACE_DWARF: case RACE_GNOME: case RACE_NIGHTELF:
        case RACE_DRAENEI: case RACE_WORGEN: case RACE_PANDAREN_ALLIANCE:
        case RACE_VOID_ELF: case RACE_LIGHTFORGED_DRAENEI: case RACE_DARK_IRON_DWARF:
        case RACE_KUL_TIRAN: case RACE_MECHAGNOME: case RACE_DRACTHYR_ALLIANCE:
        case RACE_EARTHEN_DWARF_ALLIANCE:
        case RACE_SKYBORNE_ALLIANCE:   // Classic 1.60 High Order Skyborne
            return true;
    }
    return false;
}

} // anonymous

BotPopulationManager::BotPopulationManager()
    : pipeline_(new BotSetupPipeline())
    , world_spawner_(new WorldBotSpawner(*this))
{
}

BotPopulationManager::~BotPopulationManager()
{
    delete world_spawner_;
    delete pipeline_;
}

void BotPopulationManager::OnWorldTick(uint32 now_ms)
{
    // Granular sub-phase instrumentation. Aggregated across a 60s window
    // and emitted once per window so we can identify which pop sub-phase
    // is the killer at scale (observed 2026-05-18: pop=14321ms/tick at
    // 3160 bots — needed to break that down). Each phase's enter→exit
    // delta accumulates into the per-window total; the window emit logs
    // ALL phase totals so the dominant one is obvious.
    const uint32 wt_start = getMSTime();

    // Hand every async DB read that has come back to its callback. This is the
    // ONLY place the population manager's callbacks run, so they execute on the
    // world thread, in tick order, and need no locking around the caches they
    // fill. Cheap when nothing is outstanding (an empty-vector test).
    queries_.ProcessReadyCallbacks();

    // Second half of a Reconcile cycle whose offline-candidate queries have all
    // answered (see ReconcileBegin). Deliberately here rather than inside the
    // last callback: ReconcileFinish logs bots in and spawns characters, and
    // running that from inside ProcessReadyCallbacks' own iteration is asking
    // for trouble the day one of those paths queues a callback of its own.
    if (reconcile_in_progress_)
    {
        if (reconcile_inflight_ == 0)
            ReconcileFinish();
        else
        {
            // Watchdog. A QueryCallback always fires, so this should be
            // unreachable; if it ever isn't, the cycle must not wedge
            // population shaping forever.
            constexpr uint32 kReconcileWaitTimeoutMs = 30u * 1000u;
            if (getMSTimeDiff(reconcile_started_ms_, wt_start) >= kReconcileWaitTimeoutMs)
            {
                TC_LOG_ERROR("playerbot.v2",
                    "[Population] reconcile abandoned: {} offline-candidate quer(ies) never returned",
                    reconcile_inflight_);
                reconcile_fills_.clear();
                reconcile_inflight_    = 0;
                reconcile_in_progress_ = false;
            }
        }
    }

    const uint32 t_drain_start = getMSTime();

    // Drain deferred LoginBot calls. Each entry was queued by SpawnNew on
    // the previous tick (or earlier this tick); by now the async DB worker
    // has committed the new character row, so the LoginQueryHolder's
    // SELECT will succeed. Without this defer, mass-spawn under load
    // produced "Player::LoadFromDB ... not found in table characters"
    // because LoginBot ran ahead of the async commit.
    //
    // Pass B: cap drain at kLoginDrainPerTick. Supply is ~0.83 logins/sec
    // (Reconcile queues at most 50 per 60s cycle), so even at 1Hz
    // degraded tick the 3/tick cap stays 3.6× above demand. Spreading
    // the burst across multiple ticks removes the ~1.5s stall that a
    // single tick used to absorb when draining 50 entries at once.
    constexpr size_t kLoginDrainPerTick = 3;
    if (!deferred_logins_.empty())
    {
        const size_t drain_count = std::min(deferred_logins_.size(), kLoginDrainPerTick);
        for (size_t i = 0; i < drain_count; ++i)
        {
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(deferred_logins_[i]);
            Services::SessionMgr().LoginBot(g);
        }
        deferred_logins_.erase(deferred_logins_.begin(),
                               deferred_logins_.begin() + drain_count);
    }
    perf_window_drain_ms_ += getMSTimeDiff(t_drain_start, getMSTime());

    // Proximity-driven world bots (Playerbot.World.Enable); a config read and return while disabled.
    const uint32 t_spawner_start = getMSTime();
    world_spawner_->OnWorldTick(now_ms);
    const uint32 slow_spawner_ms = getMSTimeDiff(t_spawner_start, getMSTime());

    // Drive pipelines every tick — cheap, O(active bots) read.
    const uint32 t_pipelines_start = getMSTime();
    { SLOW_CALL("DriveSetupPipelines"); DriveSetupPipelines(); }
    perf_window_pipelines_ms_ += getMSTimeDiff(t_pipelines_start, getMSTime());

    // `.bot add` companions waiting for their first in-world tick.
    const uint32 slow_pipelines_ms = getMSTimeDiff(t_pipelines_start, getMSTime());
    { SLOW_CALL("DrainCompanionSetups"); DrainCompanionSetups(); }
    WarnOversizedInstanceGroups(now_ms);
    // `.bot delete` companions waiting to leave the world.
    DrainCompanionDeletes();

    // Gear backfill every 5 min — re-gears under-geared online bots (separate
    // from the 1h hygiene so it runs after the fleet logs in, not once at boot).
    const uint32 t_gear_start = getMSTime();
    { SLOW_CALL("RunGearBackfill"); RunGearBackfill(now_ms); }
    const uint32 slow_gear_ms = getMSTimeDiff(t_gear_start, getMSTime());

    // Retry bots that were wearing client-crashing gear while in combat. Own
    // (2s) cadence — a levelling bot can be in combat for every 5-min backfill
    // pass in a row, so this must not ride on that one.
    DrainPendingGearHeals(now_ms);

    // Hygiene every 1h — cleans up stale JIT bots and enforces hard cap.
    const uint32 t_hygiene_start = getMSTime();
    { SLOW_CALL("RunHygiene"); RunHygiene(now_ms); }
    // Corpse backlog drain: 10-min cadence independent of the 1h hygiene
    // (the failed-JIT backlog is ~6K rows; at hygiene cadence it would
    // take half a day to clear while every cycle keeps colliding with
    // its dead names).
    if (!last_corpse_sweep_ms_ || (now_ms - last_corpse_sweep_ms_) >= 10u * 60u * 1000u)
    {
        last_corpse_sweep_ms_ = now_ms;
        SweepFailedJitCorpses();
    }
    perf_window_hygiene_ms_ += getMSTimeDiff(t_hygiene_start, getMSTime());

    // Fleet rebalance every 5min — proactive spec rotation per bracket.
    const uint32 t_rebalance_start = getMSTime();
    { SLOW_CALL("RunRebalance"); RunRebalance(now_ms); }
    perf_window_rebalance_ms_ += getMSTimeDiff(t_rebalance_start, getMSTime());

    // BG prep-phase top-up — every 5s, top up active BGs in WAIT_JOIN
    // until each team is at max_per_team. Lets the 2-minute prep window
    // be used to grow the population from initial fill to capacity.
    const uint32 t_bgtopup_start = getMSTime();
    { SLOW_CALL("TopUpActiveBGs"); TopUpActiveBGs(now_ms); }
    { SLOW_CALL("ScrubWorldGear"); ScrubWorldGear(now_ms); }
    { SLOW_CALL("BgGroupHygiene"); BgGroupHygiene(now_ms); }
    { SLOW_CALL("SeedBgMatches"); SeedBgMatches(now_ms); }
    { SLOW_CALL("DriveBgStress"); DriveBgStress(now_ms); }
    { SLOW_CALL("DriveBgSoak"); DriveBgSoak(now_ms); }
    { SLOW_CALL("SeedArenaMatches"); SeedArenaMatches(now_ms); }
    { SLOW_CALL("BotQueueFiller::AutoJoinTick"); BotQueueFiller::AutoJoinTick(now_ms); }
    { SLOW_CALL("BotQueueFiller::MakeRoomForRealPlayers"); BotQueueFiller::MakeRoomForRealPlayers(now_ms); }
    perf_window_bgtopup_ms_ += getMSTimeDiff(t_bgtopup_start, getMSTime());

    // One line for any frame where this manager alone cost 60 ms or more, with the phase split, so a 300 ms 'pop='
    // tick in the TickPerf digest can be traced to its phase (stage B, 2026-10-07: the cause was not visible).
    if (uint32 const pop_ms = getMSTimeDiff(wt_start, getMSTime()); pop_ms >= 60u)
        TC_LOG_INFO("playerbot.v2", "[pop_slow] total={}ms queries+drain={}ms spawner={}ms pipelines={}ms gear={}ms rest={}ms",
                    pop_ms, getMSTimeDiff(t_drain_start, t_spawner_start), slow_spawner_ms, slow_pipelines_ms, slow_gear_ms,
                    pop_ms - std::min(pop_ms, getMSTimeDiff(t_drain_start, t_spawner_start) + slow_spawner_ms + slow_pipelines_ms + slow_gear_ms));

    // Aggregate emit. 60s window matches FleetLog cadence so the two
    // can be cross-referenced in the same log scroll.
    constexpr uint32 kPerfEmitIntervalMs = 60u * 1000u;
    if (perf_window_start_ms_ == 0) perf_window_start_ms_ = wt_start;
    if (getMSTimeDiff(perf_window_start_ms_, wt_start) >= kPerfEmitIntervalMs)
    {
        TC_LOG_INFO("playerbot.v2",
            "[PopPerf] window={}ms drain={}ms pipelines={}ms hygiene={}ms "
            "rebalance={}ms bgtopup={}ms reconcile={}ms (compute={}ms populate={}ms "
            "reconcile_side={}ms login_existing={}ms spawn_new={}ms logout_lru={}ms)",
            getMSTimeDiff(perf_window_start_ms_, wt_start),
            perf_window_drain_ms_, perf_window_pipelines_ms_, perf_window_hygiene_ms_,
            perf_window_rebalance_ms_, perf_window_bgtopup_ms_, perf_window_reconcile_ms_,
            perf_window_compute_targets_ms_, perf_window_populate_actual_ms_,
            perf_window_reconcile_side_ms_, perf_window_login_existing_ms_,
            perf_window_spawn_new_ms_, perf_window_logout_lru_ms_);
        perf_window_start_ms_       = wt_start;
        perf_window_drain_ms_       = 0;
        perf_window_pipelines_ms_   = 0;
        perf_window_hygiene_ms_     = 0;
        perf_window_rebalance_ms_   = 0;
        perf_window_bgtopup_ms_     = 0;
        perf_window_reconcile_ms_   = 0;
        perf_window_compute_targets_ms_ = 0;
        perf_window_populate_actual_ms_ = 0;
        perf_window_reconcile_side_ms_  = 0;
        perf_window_login_existing_ms_  = 0;
        perf_window_spawn_new_ms_       = 0;
        perf_window_logout_lru_ms_      = 0;
    }

    // Reconcile only every kTickIntervalMs ms.
    if (last_tick_ms_ && (now_ms - last_tick_ms_) < kTickIntervalMs)
        return;
    // The previous cycle is still waiting on its offline-candidate rows. Don't
    // stamp last_tick_ms_ — the cycle starts on the first tick after the rows
    // land instead of being skipped for another full interval.
    if (reconcile_in_progress_)
        return;
    last_tick_ms_ = now_ms;

    // Split each cycle's budget per faction based on HordePct. Without
    // this, Reconcile's bucket loop drained the entire pool into Alliance
    // before Horde got serviced (8.3× imbalance observed 2026-05-15).
    // Round-half-up keeps the totals at kSpawnRate/kLoginRate when HordePct
    // is non-integer-friendly (e.g. 50% → 12+13).
    {
        const uint8 horde_pct = Services::Config().population_horde_pct();
        spawn_budget_horde_ = (kSpawnRatePerTick * horde_pct + 50u) / 100u;
        spawn_budget_alli_  = kSpawnRatePerTick - spawn_budget_horde_;
        login_budget_horde_ = (kLoginRatePerTick * horde_pct + 50u) / 100u;
        login_budget_alli_  = kLoginRatePerTick - login_budget_horde_;
    }

    Reconcile();
}

void BotPopulationManager::SweepFailedJitCorpses()
{
    // Failed-JIT corpse sweep. The pre-wave-16d spawn_jit login race
    // ("Player::LoadFromDB failed") left thousands of JIT-tagged
    // characters that NEVER entered the world (totaltime=0, online=0)
    // -- dead rows that exhaust the per-race name pools ('name already
    // in use' on every new JIT batch) and bloat the lifecycle. They are
    // pure bug artifacts: never played, never seen by anyone.
    // Bulk-reclaim them (names released to the pool, both rows dropped)
    // at 500/pass.
    //
    // Async: this is a 500-row JOIN across playerbot_v2_character and
    // characters, and it used to block the world thread for the whole
    // round-trip every 10 minutes (and again on every hygiene pass).
    // corpse_sweep_pending_ keeps the 10-min cadence and the hourly hygiene
    // pass from ever having two of these in flight at once — both act on the
    // rows the SELECT returns, so an overlap would run the same 500 guids
    // through the reclaim twice.
    if (corpse_sweep_pending_)
        return;
    corpse_sweep_pending_ = true;
    queries_.AddCallback(CharacterDatabase.AsyncQuery(
        "SELECT pv.character_guid_low, c.account, c.name "
        "FROM playerbot_v2_character pv "
        "JOIN characters c ON c.guid = pv.character_guid_low "
        "WHERE pv.jit_for_queue IS NOT NULL AND pv.jit_for_queue != '' "
        "AND c.totaltime = 0 AND c.online = 0 "
        "AND pv.last_seen_at < (NOW() - INTERVAL 1 HOUR) LIMIT 500")
        .WithCallback([this](QueryResult corpse_res)
    {
        corpse_sweep_pending_ = false;
        if (!corpse_res)
            return;
        std::string in_list;
        uint32 swept = 0;
        do {
            Field* f = corpse_res->Fetch();
            const uint64 guid    = f[0].GetUInt64();
            const uint32 account = f[1].GetUInt32();
            BotNamePool::Release(f[2].GetString());
            Services::Lifecycle().unmark_as_bot(guid);
            forget_pipeline_caches(guid);
            Services::Accounts().note_character_removed(account);
            if (!in_list.empty()) in_list += ',';
            in_list += std::to_string(guid);
            ++swept;
        } while (corpse_res->NextRow());
        if (swept)
        {
            CharacterDatabase.PExecute(
                "DELETE FROM playerbot_v2_character WHERE character_guid_low IN ({})", in_list);
            CharacterDatabase.PExecute(
                "DELETE FROM characters WHERE guid IN ({})", in_list);
            TC_LOG_INFO("playerbot.v2",
                "[Hygiene] swept {} failed-JIT corpse characters (never entered world)", swept);
        }
    }));
}

// Shield-tank weapon correction (task #8, 2026-06-28). A Protection Warrior (73)
// or Protection Paladin (66) that was generated/looted onto a TWO-HANDED weapon
// runs SHIELD-LESS: a 2H locks out the offhand, so the tank loses block, ~half
// its armor, and its entire Shield Slam / Shield Block kit (it casts those with
// no shield → they fail). The normal backfill CANNOT fix this — a 1H has LOWER
// ilvl than the 2H so the per-slot upgrade guard refuses the swap, and the
// snapshot auto-equip can't put a shield in the offhand while a 2H holds both
// hands (EQUIP_ERR_2HANDED_EQUIPPED). Observed live 2026-06-28: the Deadmines
// prot-warrior tank ran a 2H + empty offhand and died 38× in the harbor. Force
// the correct 1H+shield: equip the generated 1H mainhand FIRST (replaces the 2H,
// freeing the offhand), then the shield. Cheap pre-checks short-circuit every
// non-mis-equipped bot before the generator call. Returns true if it changed a
// slot. World-thread only (mutates the Player) — called from RunGearBackfill.
static bool EnsureShieldTankWeapon(Player* p)
{
    if (!p) return false;
    uint32 const spec = uint32(::Playerbot::Classic::EffectiveSpecId(p));
    if (spec != 73 && spec != 66) return false;       // Prot Warrior / Prot Paladin
    Item* mh = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    Item* oh = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    // Already correct: a shield in the offhand (impossible alongside a 2H).
    if (oh && oh->GetTemplate() &&
        oh->GetTemplate()->GetInventoryType() == INVTYPE_SHIELD)
        return false;
    bool const mh_is_2h = mh && mh->GetTemplate() &&
        mh->GetTemplate()->GetInventoryType() == INVTYPE_2HWEAPON;

    Gear::GearGenerationContext ctx;
    ctx.level  = p->GetLevel();
    ctx.cls    = p->GetClass();
    ctx.race   = p->GetRace();
    ctx.spec   = uint16(spec);
    ctx.bot_id = p->GetGUID().GetCounter();
    ctx.wearer = p;
    ctx.role = ::Playerbot::Pool::RoleOf(ctx.bot_id);
    uint32 oneh_entry = 0, shield_entry = 0;
    for (auto const& gen : Gear::GenerateGearFor(ctx))   // generator now skips 2H mainhand for these specs
    {
        if (gen.slot == EQUIPMENT_SLOT_MAINHAND) oneh_entry = gen.item_entry;
        else if (gen.slot == EQUIPMENT_SLOT_OFFHAND) shield_entry = gen.item_entry;
    }
    auto force_equip = [&](uint32 entry, uint8 slot) -> bool
    {
        if (!entry) return false;
        Item* cur = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (cur && cur->GetEntry() == entry) return false;
        ItemPosCountVec dest;
        if (p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, entry, 1) != EQUIP_ERR_OK)
            return false;
        Item* it = p->StoreNewItem(dest, entry, true);
        if (!it) return false;
        uint16 eq_dest = 0;
        if (p->CanEquipItem(slot, eq_dest, it, /*swap*/ true) != EQUIP_ERR_OK)
            return false;   // generated piece refused (leave it in the bag)
        const uint16 src = (uint16(it->GetBagSlot()) << 8) | it->GetSlot();
        const uint16 dst = (uint16(INVENTORY_SLOT_BAG_0) << 8) | slot;
        p->SwapItem(src, dst);   // SAFE unequip + re-store-displaced
        return true;
    };
    bool changed = false;
    if (mh_is_2h) changed |= force_equip(oneh_entry, EQUIPMENT_SLOT_MAINHAND);
    changed |= force_equip(shield_entry, EQUIPMENT_SLOT_OFFHAND);
    if (changed) p->AutoUnequipOffhandIfNeed();
    return changed;
}

// Warrior / rogue ranged slot (Playerbot.Behavior.TankRangedWeapon): the pull-back opener (Throw / Shoot Bow / Shoot
// Gun) fails SPELL_FAILED_NO_AMMO without a ranged weapon it can fire. Thrown weapons are their own ammo, so the bot
// wears the best thrown weapon of its level and carries a stack of it; a bow or gun it has ammo for stays. The extra
// copies sit in the bags: Spell::CheckCast counts HasItemCount(entry), which includes the worn one. World-thread only.
// Returns true when it equipped or added anything.
static bool EnsureThrownRanged(Player* p)
{
    if (!p || !p->IsAlive() || !Gear::UsesThrownRanged(p->GetClass()) || IsPlayersOwnCharacter(p))
        return false;
    bool changed = false;
    Item* worn = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
    ItemTemplate const* wtpl = worn ? worn->GetTemplate() : nullptr;
    bool const worn_thrown = wtpl && wtpl->GetSubClass() == ITEM_SUBCLASS_WEAPON_THROWN;
    // A bow, gun or crossbow the bot has no ammo for is as good as no weapon.
    bool const worn_usable = worn_thrown || (wtpl && (!p->NeedsAmmo() || p->GetAmmoId() != 0));
    if (!worn_usable && !p->IsInCombat())
    {
        if (uint32 const entry = Gear::PickThrownWeapon(p))
        {
            ItemPosCountVec dest;
            if (p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, entry, 1) == EQUIP_ERR_OK)
                if (Item* it = p->StoreNewItem(dest, entry, true))
                {
                    uint16 eq_dest = 0;
                    if (p->CanEquipItem(EQUIPMENT_SLOT_RANGED, eq_dest, it, /*swap*/ worn != nullptr) == EQUIP_ERR_OK)
                    {
                        p->SwapItem((uint16(it->GetBagSlot()) << 8) | it->GetSlot(),
                                    (uint16(INVENTORY_SLOT_BAG_0) << 8) | EQUIPMENT_SLOT_RANGED);
                        changed = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED) == it;
                    }
                    if (!changed)
                        p->DestroyItem(it->GetBagSlot(), it->GetSlot(), true);   // refused: don't leave the copy
                }
        }
        worn = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
        wtpl = worn ? worn->GetTemplate() : nullptr;
    }
    if (!wtpl || wtpl->GetSubClass() != ITEM_SUBCLASS_WEAPON_THROWN)
        return changed;
    // Stack: up to 200 (or the item's stack limit); refilled once it is down to half.
    uint32 const target = std::min<uint32>(200u, std::max<uint32>(1u, wtpl->GetMaxStackSize()));
    uint32 const have = p->GetItemCount(wtpl->GetId(), /*inBankAlso*/ false);
    if (have > target / 2)
        return changed;
    ItemPosCountVec dest;
    if (p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, wtpl->GetId(), target - have) == EQUIP_ERR_OK &&
        p->StoreNewItem(dest, wtpl->GetId(), true))
    {
        TC_LOG_INFO("playerbot.v2", "[GearBackfill] {} thrown ranged: {} +{} (had {})", p->GetName(), wtpl->GetId(), target - have, have);
        changed = true;
    }
    return changed;
}

// Free ONE bag slot by destroying the least-valuable grey (POOR) vendor-trash
// item, so a strict gear UPGRADE can be staged when the bags are full. This is
// the common case for a bot that has looted a full dungeon — the live blocker
// for the Deadmines healer re-gear: every generated upgrade hit CanStoreNewItem
// == bags_full and the healer stayed ilvl48 (under-healing the harbor). Only
// POOR-quality items are ever destroyed (vendor trash a player drops on sight),
// never quest items, never a non-empty container, never UNCOMMON+ loot. Returns
// true if a slot was freed. Distribution bots only — altbots are guarded out by
// the caller (they never re-gear-swap an occupied slot).
static bool FreeOneJunkBagSlot(Player* p)
{
    if (!p) return false;
    Item* victim = nullptr;
    uint32 victim_val = 0xFFFFFFFFu;
    auto consider = [&](Item* it)
    {
        if (!it) return;
        ItemTemplate const* t = it->GetTemplate();
        if (!t) return;
        if (t->GetQuality() != ITEM_QUALITY_POOR) return;   // grey trash only
        if (it->IsNotEmptyBag()) return;                    // never a full container
        if (t->HasFlag(ITEM_FLAG_HAS_LOOT)) return;         // never a lootable (locked) item
        const uint32 val = t->GetSellPrice() * std::max<uint32>(1u, it->GetCount());
        if (val < victim_val) { victim_val = val; victim = it; }
    };
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        consider(p->GetItemByPos(INVENTORY_SLOT_BAG_0, i));
    for (uint8 b = INVENTORY_SLOT_BAG_START; b < INVENTORY_SLOT_BAG_END; ++b)
        if (Bag* bag = p->GetBagByPos(b))
            for (uint32 s = 0; s < bag->GetBagSize(); ++s)
                consider(p->GetItemByPos(b, uint8(s)));
    if (!victim) return false;
    p->DestroyItem(victim->GetBagSlot(), victim->GetSlot(), true);
    return true;
}

void BotPopulationManager::DrainPendingGearHeals(uint32 now_ms)
{
    if (pending_gear_heal_.empty()) return;

    // 2s cadence rather than every frame: the queue only holds bots that are
    // both badly geared and mid-fight, but a fleet-wide incident could make it
    // large and this walks it. Two seconds is three orders of magnitude faster
    // than the 5-min pass this exists to escape, at negligible cost.
    constexpr uint32 kDrainIntervalMs = 2000;
    if (last_gear_heal_drain_ms_ && (now_ms - last_gear_heal_drain_ms_) < kDrainIntervalMs)
        return;
    last_gear_heal_drain_ms_ = now_ms;

    // Collect first, heal second: HealUnrenderableGear mutates the set, which
    // would invalidate an iterator held across the call.
    constexpr size_t kMaxPerDrain = 8;
    std::vector<uint64> ready;
    ready.reserve(kMaxPerDrain);
    for (auto it = pending_gear_heal_.begin(); it != pending_gear_heal_.end(); )
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(
            ObjectGuid::Create<HighGuid::Player>(*it));
        if (!p || !p->IsInWorld())
        {
            // Logged out - Module::OnPlayerLogin heals it on the way back in,
            // and it cannot be inspected while offline.
            it = pending_gear_heal_.erase(it);
            continue;
        }
        if (!p->IsInCombat())
        {
            ready.push_back(*it);
            if (ready.size() >= kMaxPerDrain)
                break;
        }
        ++it;
    }

    for (uint64 id : ready)
        if (Player* p = ObjectAccessor::FindConnectedPlayer(
                ObjectGuid::Create<HighGuid::Player>(id)))
            HealUnrenderableGear(p);
}

bool BotPopulationManager::HealUnrenderableGear(Player* p)
{
    if (!p || !p->IsInWorld()) return false;

    // Cheap pre-scan; almost every bot exits here. Deliberately BEFORE the
    // combat check: we have to know whether there is anything to heal in order
    // to queue a bot that is merely busy, instead of dropping it silently.
    std::array<uint8, EQUIPMENT_SLOT_END> bad_slots{};
    uint8 bad_count = 0;
    // A player's own characters keep whatever they wear; a bot also loses items bots must not wear (Season of
    // Discovery pieces on Classic), the same way.
    bool const own_character = IsPlayersOwnCharacter(p);
    for (uint8 s = EQUIPMENT_SLOT_START; s < EQUIPMENT_SLOT_END; ++s)
        if (Item const* it = p->GetItemByPos(INVENTORY_SLOT_BAG_0, s))
            if (!::Playerbot::IsItemRenderableInSlot(it, p, s) ||
                (!own_character && !Gear::IsBotWearableItemId(it->GetEntry())))
                bad_slots[bad_count++] = s;

    BotId const id = p->GetGUID().GetCounter();
    if (bad_count == 0)
    {
        pending_gear_heal_.erase(id);
        return false;
    }

    // Armor can't be changed in combat (CanEquipItem / CanUnequipItem refuse
    // with NOT_IN_COMBAT), and falling through to the destroy branch would then
    // delete gear that merely could not be swapped yet. Queue it instead of
    // skipping: the old "retry next backfill pass" left a bot that is fighting
    // whenever the 5-min pass fires wearing a client-crashing item forever
    // (test box, 2026-09-16: 7 online levellers never healed). The drain below
    // catches it within seconds of leaving combat.
    if (p->IsInCombat())
    {
        pending_gear_heal_.insert(id);
        return false;
    }
    pending_gear_heal_.erase(id);
    // A player's own characters follow the backfill's owner invariant: never put
    // generated gear into an occupied slot and never destroy owner items. They
    // only get the unrenderable piece taken off into their bags.
    bool const is_altbot = own_character;

    std::array<uint32, EQUIPMENT_SLOT_END> replacement{};
    if (!is_altbot)
    {
        Gear::GearGenerationContext ctx;
        ctx.level  = p->GetLevel();
        ctx.cls    = p->GetClass();
        ctx.race   = p->GetRace();
        ctx.spec   = uint16(::Playerbot::Classic::EffectiveSpecId(p));
        ctx.bot_id = id;
        ctx.wearer = p;
        ctx.role = ::Playerbot::Pool::RoleOf(id);
        // The generator never returns an unrenderable item (index-time gate).
        for (auto const& g : Gear::GenerateGearFor(ctx))
            if (g.slot < EQUIPMENT_SLOT_END)
                replacement[g.slot] = g.item_entry;
    }

    bool changed = false;
    for (uint8 i = 0; i < bad_count; ++i)
    {
        uint8 const slot = bad_slots[i];
        Item* cur = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!cur) continue;     // an earlier swap in this loop already moved it
        uint32 const bad_entry = cur->GetEntry();
        char const* outcome = nullptr;

        // 1. Swap in the generated piece for this slot. CanEquipNewItem is
        //    checked BEFORE anything is created or destroyed, so a refusal
        //    (proficiency / level / unique) never strands a staged item in
        //    the bags or leaves the slot naked.
        if (uint32 const entry = replacement[slot]; entry && entry != bad_entry)
        {
            uint16 eq_dest = 0;
            if (p->CanEquipNewItem(slot, eq_dest, entry, /*swap*/ true) == EQUIP_ERR_OK)
            {
                ItemPosCountVec dest;
                if (p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, entry, 1) == EQUIP_ERR_OK)
                {
                    // SAFE path: stage in bags, then SwapItem - the displaced
                    // piece is re-stored, where the snapshot's renderability
                    // clamp keeps auto-equip from putting it back on.
                    if (Item* staged = p->StoreNewItem(dest, entry, true))
                    {
                        uint16 const src = (uint16(staged->GetBagSlot()) << 8) | staged->GetSlot();
                        uint16 const dst = (uint16(INVENTORY_SLOT_BAG_0) << 8) | slot;
                        p->SwapItem(src, dst);
                        if (p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot) == staged)
                            outcome = "replaced (old piece to bags)";
                    }
                }
                else
                {
                    // Bags full: replace in place, as the backfill's
                    // direct-equip fallback does.
                    p->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);
                    if (p->EquipNewItem(eq_dest, entry, ItemContext::NONE, true))
                        outcome = "replaced in place (bags full, old piece destroyed)";
                    else
                        outcome = "destroyed (bags full, replacement refused)";
                }
            }
        }

        // 2. No usable replacement: take it off into the bags, exactly like the
        //    client's auto-store (HandleAutoStoreBagItemOpcode).
        if (!outcome)
        {
            uint16 const pos = (uint16(INVENTORY_SLOT_BAG_0) << 8) | slot;
            ItemPosCountVec dest;
            if (p->CanUnequipItem(pos, /*swap*/ false) == EQUIP_ERR_OK &&
                p->CanStoreItem(NULL_BAG, NULL_SLOT, dest, cur, false) == EQUIP_ERR_OK)
            {
                p->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
                p->StoreItem(dest, cur, true);
                outcome = "unequipped to bags (no renderable replacement)";
            }
            else if (!is_altbot)
            {
                // A naked slot is a smaller harm than crashing every client
                // that looks at this bot.
                p->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);
                outcome = "destroyed (no replacement, bags full)";
            }
        }

        if (outcome)
        {
            changed = true;
            TC_LOG_INFO("playerbot.v2",
                "[GearHeal] {} L{}: slot {} item {} has no ItemModifiedAppearance "
                "(client cannot render) - {}",
                p->GetName(), uint32(p->GetLevel()), uint32(slot), bad_entry, outcome);
        }
        else
        {
            // Altbot with full bags. Retried every backfill pass; DEBUG so a
            // stuck owner alt doesn't flood the log.
            TC_LOG_DEBUG("playerbot.v2",
                "[GearHeal] {} [altbot]: slot {} item {} is unrenderable but "
                "cannot be unequipped (bags full) - retry next pass",
                p->GetName(), uint32(slot), bad_entry);
        }
    }
    if (changed)
        p->AutoUnequipOffhandIfNeed();
    return changed;
}

bool BotPopulationManager::SetOwnerGearMode(uint32 account_id, uint8 requested_mode, std::string& reply)
{
    using Gear::Owned::GearMode;
    if (requested_mode >= Gear::Owned::kGearModeCount)
    {
        reply = "unknown gear mode (match, best, bis, t1, t2, t3)";
        return false;
    }
    GearMode const mode = GearMode(requested_mode);
    if (!Gear::Owned::ModeAllowed(Gear::OwnedAllowedModes(), mode))
    {
        reply = std::string("this server does not allow gear mode ") + Gear::Owned::ModeName(mode);
        return false;
    }
    Services::Owners().SetOwnerGearMode(account_id, requested_mode);
    for (BotId const bot : Services::Owners().BotsOwnedBy(account_id))
        RequestGearRefresh(bot);
    reply = std::string("gear mode ") + Gear::Owned::ModeName(mode) +
            (Gear::Owned::IsCapMode(mode) ? " (this applies at level " + std::to_string(MaxPlayerLevel()) + "; below that: match)" : "");
    return true;
}

void BotPopulationManager::RequestGearRefresh(BotId bot)
{
    std::lock_guard lk(gear_rush_mtx_);
    if (std::ranges::find(gear_rush_, bot) == gear_rush_.end())
        gear_rush_.push_back(bot);
}

void BotPopulationManager::RunGearBackfill(uint32 now_ms)
{
    // Owner-requested gear passes (a changed gear mode) go first, ignoring the cadence, the cooldown and the per-sweep cap.
    std::vector<BotId> rush;
    {
        std::lock_guard lk(gear_rush_mtx_);
        rush.swap(gear_rush_);
    }
    for (BotId const id : rush)
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld())
            continue;
        uint32 regear_count = 0;
        BackfillOne(id, p, now_ms, regear_count, /*ignore_cooldown*/ true);
    }

    // 5-min cadence (vs the 1h hygiene). Re-gears under-geared / weaponless
    // ONLINE bots. Two historical roots are now fixed at the source — the equip
    // swap destroyed generated upgrades (EquipItem on an occupied slot, see
    // Player.cpp:11758), and the generator ranked by static base ilvl not the
    // level-scaled effective ilvl bots actually wear (Varethon L16 stuck at
    // effective ItemLevel 5). But GenerateGear is a one-shot setup bit that
    // never re-runs, so the EXISTING fleet (geared under the broken code, or
    // out-leveled past setup) needs a backfill. ONLINE-only, capped 25/pass,
    // OFF the snapshot-build thread. Uses Player::SwapItem (the SAFE unequip +
    // re-store-displaced path, NOT the destructive RemoveItem+EquipItem) and
    // strict guards so it never dupes, churns, or downgrades.
    constexpr uint32 kGearBackfillIntervalMs = 5u * 60u * 1000u;
    // The sweep is spread over frames: at 700 bots the old all-at-once pass (up to 25 re-gears plus a companion upkeep
    // per bot) was a single 300 ms world tick every 5 minutes. A frame now spends at most kGearBudgetMs on it.
    constexpr uint32 kGearBudgetMs = 6u;
    if (gear_scan_pos_ >= gear_scan_.size())
    {
        if (last_gear_backfill_ms_ && (now_ms - last_gear_backfill_ms_) < kGearBackfillIntervalMs)
            return;
        last_gear_backfill_ms_ = now_ms;
        gear_scan_.clear();
        gear_scan_pos_ = 0;
        gear_regear_count_ = 0;
        Services::Registry().for_each([&](BotId id, BotRegistryEntry const&) { gear_scan_.push_back(id); });
    }
    const uint32 t0 = getMSTime();
    while (gear_scan_pos_ < gear_scan_.size() && getMSTimeDiff(t0, getMSTime()) < kGearBudgetMs)
    {
        BotId const id = gear_scan_[gear_scan_pos_++];
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld()) continue;
        // Before the re-gear cap and gate: a well-geared companion still needs its new spells.
        CompanionUpkeep(id, p);
        if (gear_regear_count_ >= 25) continue;     // bound per sweep
        BackfillOne(id, p, now_ms, gear_regear_count_, /*ignore_cooldown*/ false);
    }
}

void BotPopulationManager::CompanionUpkeep(BotId id, Player* p)
{
    if (Services::Owners().OwnerCharacter(id) == 0 || IsPlayersOwnCharacter(p))
        return;
    ::Playerbot::API api(p);
    api.learn_class_quest_spells_free();
    api.learn_trainer_spells_free();
    api.learn_weapon_skills_free();
    api.learn_riding_free();
    // Keep the declared role pinned (memory only otherwise) so the talent walk below follows it, not a re-inference.
    ApplyCompanionRole(p, Role::Unknown, "upkeep");
    if (!p->IsInCombat() && p->GetLevel() >= 10)
        api.apply_classic_talents();
    p->DurabilityRepairAll(false, 0.0f, false);
    GrantCompanionStarterPet(id, p);
    // A companion geared before per-bot gear variety wears the same top pick per slot as every other bot of its class
    // and level; it gets its one re-roll here (or in FinishCompanionSetup when it is called), whichever comes first.
    // A level-cap companion that has had the first re-roll but not the raid-gear variety pass gets it here (RerollCompanionGear).
    if (Services::Owners().GearVaried(id) && !Services::Owners().GearVariedRaid(id) && Gear::RaidGearActive(p->GetLevel()))
    {
        uint32 changed = 0;
        RerollCompanionGear(p, changed);
    }
    if (!Services::Owners().GearVaried(id))
    {
        uint32 changed = 0;
        if (RerollCompanionGear(p, changed) == RerollResult::Done)
        {
            Services::Owners().MarkGearVaried(id);
            TC_LOG_INFO("playerbot.v2", "[Companion] {} one-time varied gear re-roll: {} slot(s) changed", p->GetName(), changed);
        }
    }
}

void BotPopulationManager::GrantCompanionStarterPet(BotId id, Player* p)
{
    if (p->GetClass() != CLASS_HUNTER || p->GetLevel() < 10 || Services::Owners().StarterPetGranted(id))
        return;
    using PetResult = ::Playerbot::API::StarterPetResult;
    PetResult const r = ::Playerbot::API(p).grant_starter_hunter_pet(World::StarterHunterPetEntry(p->GetRace(), p->GetTeam() == ALLIANCE));
    // Tamed or already has a pet: done for good. A pet the owner later abandons is not replaced. Unavailable (no
    // tameable beast in this data) is not retried every pass either. NotNow (mounted, in combat...) waits for the next.
    if (r != PetResult::NotNow)
        Services::Owners().MarkStarterPetGranted(id);
}

void BotPopulationManager::QueueCompanionSetup(BotId bot, uint64 owner_guid_low, uint8 role)
{
    pending_companions_[bot] = PendingCompanion{ owner_guid_low, role, getMSTime() };
}

void BotPopulationManager::QueueCompanionDelete(BotId bot)
{
    pending_companions_.erase(bot);
    pending_deletes_[bot] = getMSTime();
}

void BotPopulationManager::DrainCompanionDeletes()
{
    for (auto it = pending_deletes_.begin(); it != pending_deletes_.end(); )
    {
        BotId const id = it->first;
        ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(id);
        if (ObjectAccessor::FindConnectedPlayer(guid))
        {
            if (getMSTimeDiff(it->second, getMSTime()) > 60u * 1000u)
            {
                TC_LOG_WARN("playerbot.v2", "[Companion] {} still in the world a minute after `.bot delete`, not deleted",
                    guid.ToString());
                it = pending_deletes_.erase(it);
            }
            else
                ++it;
            continue;
        }
        // Same sequence as `.playerbot delete`: forget the bot logic and the bot mark before the character row goes.
        if (Services::Registry().has(id))
        {
            Services::Scheduler().unregister_bot(id);
            Services::Snapshots().remove(id);
            Services::Registry().unregister_bot(id);
        }
        Services::Owners().ClearOwner(id);
        Services::Lifecycle().unmark_as_bot(id);
        uint32 const account = sCharacterCache->GetCharacterAccountIdByGuid(guid);
        Player::DeleteFromDB(guid, account, /*updateRealmChars*/ true, /*deleteFinally*/ true);
        Services::Accounts().note_character_removed(account);
        TC_LOG_INFO("playerbot.v2", "[Companion] {} deleted (`.bot delete`)", guid.ToString());
        it = pending_deletes_.erase(it);
    }
}

Role BotPopulationManager::ApplyCompanionRole(Player* bot, Role requested, char const* where)
{
    BotId const id = bot->GetGUID().GetCounter();
    uint8 const cls = bot->GetClass();
    OwnerRegistry& owners = Services::Owners();

    // 1. what the owner asked for now, 2. what is stored with the companion, 3. what its talents say. Only 1 and 2 are pinned and
    // stored; 3 is a guess about a build that may not be bought yet and is never kept (Fleet/CompanionRole.h).
    Role const declared = owners.DeclaredRole(id);
    Role inferred = Role::Unknown;
    uint32 spec = 0;
    if (requested == Role::Unknown && declared == Role::Unknown)
    {
        spec = ::Playerbot::Classic::EffectiveSpecId(bot);
        inferred = IsTankSpec(cls, uint16(spec))   ? Role::Tank
                 : IsHealerSpec(cls, uint16(spec)) ? Role::Healer
                                                   : Role::Dps;
    }
    Fleet::CompanionRoleChoice const choice = Fleet::ChooseCompanionRole(requested, declared, inferred, cls == CLASS_PALADIN || cls == CLASS_DRUID);
    Role role = choice.role;
    char const* source = choice.source == Fleet::CompanionRoleSource::Declared ? "declared"
                       : choice.source == Fleet::CompanionRoleSource::SpecHybrid ? "spec_hybrid" : "spec";
    if (!choice.pin)
    {
        // Not chosen by anybody: the bot logic keeps reading the live talents, the quota fill still sees it as unset, nothing is stored.
        BotAI* ai = Services::Registry().ai(id);
        if (ai && ai->role_override() != Role::Unknown)
            ai->set_role_override(Role::Unknown);
        if (std::strcmp(where, "upkeep") != 0)
            TC_LOG_INFO("playerbot.v2", "[role] {} role={} source={} spec={} ({}) not stored", bot->GetName(),
                        role == Role::Tank ? "tank" : role == Role::Healer ? "healer" : "dps", source, spec, where);
        return role;
    }
    if (choice.source == Fleet::CompanionRoleSource::SpecHybrid)
        spec = HealerSpecForClass(cls);
    else
    {
        switch (role)
        {
            case Role::Tank:   spec = TankSpecForClass(cls); break;
            case Role::Healer: spec = HealerSpecForClass(cls); break;
            default: break;
        }
        // A class without that role's spec (a warrior healer) plays DPS.
        if (!spec && role != Role::Dps)
        {
            role = Role::Dps;
            source = "fallback";
        }
        if (role == Role::Dps)
        {
            // A DPS spec it already has (Elemental, Fury) is kept; a tank or healer one gives way to the class default.
            uint32 const current = ::Playerbot::Classic::EffectiveSpecId(bot);
            spec = (current && !IsTankSpec(cls, uint16(current)) && !IsHealerSpec(cls, uint16(current)))
                ? current : DpsSpecForClass(cls);
        }
    }

    // The spec the owner saved the bot in, when it still fits the role it plays (a role changed since goes by the class default).
    if (uint32 const saved = owners.StoredSpec(id); saved)
    {
        Role const saved_role = IsTankSpec(cls, uint16(saved)) ? Role::Tank : IsHealerSpec(cls, uint16(saved)) ? Role::Healer : Role::Dps;
        uint32 const chosen = Fleet::ChooseCompanionSpec(spec, saved, saved_role, role, choice.store);
        if (chosen != spec)
        {
            TC_LOG_INFO("playerbot.v2", "[roster_restore] {} spec {} -> {} (saved)", bot->GetName(), spec, chosen);
            spec = chosen;
        }
    }

    BotAI* ai = Services::Registry().ai(id);
    bool const store = choice.store;
    bool const changed = (ai && ai->role_override() != role) || (store && owners.DeclaredRole(id) != role) ||
                         (spec && ::Playerbot::Classic::GetSpecOverride(bot->GetGUID()) != spec);
    if (ai)
        ai->set_role_override(role);
    if (spec)
        ::Playerbot::Classic::SetSpecOverride(bot, spec);
    if (store)
        owners.SetDeclaredRole(id, role);

    // At add, call and login always; the 5-minute upkeep only when it had to put something back.
    if (changed || std::strcmp(where, "upkeep") != 0)
        TC_LOG_INFO("playerbot.v2", "[role] {} role={} source={} spec={} ({})", bot->GetName(),
                    role == Role::Tank ? "tank" : role == Role::Healer ? "healer" : "dps", source, spec, where);
    return role;
}

Role BotPopulationManager::StoreCurrentRole(Player* bot)
{
    BotId const id = bot->GetGUID().GetCounter();
    uint8 const cls = bot->GetClass();
    // The spec it plays now is stored whatever the role's state: the role alone cannot say Retribution from a second DPS spec.
    uint32 const now_spec = ::Playerbot::Classic::EffectiveSpecId(bot);
    Services::Owners().SetStoredSpec(id, now_spec);
    TC_LOG_INFO("playerbot.v2", "[roster_save] {} spec {}", bot->GetName(), now_spec);
    Role const declared = Services::Owners().DeclaredRole(id);
    if (declared != Role::Unknown)
        return declared;
    Role role = Role::Unknown;
    if (BotAI* ai = Services::Registry().ai(id))
        role = ai->role_override();
    if (role == Role::Unknown)
    {
        uint32 const spec = ::Playerbot::Classic::EffectiveSpecId(bot);
        role = IsTankSpec(cls, uint16(spec)) ? Role::Tank : IsHealerSpec(cls, uint16(spec)) ? Role::Healer : Role::Dps;
    }
    return ApplyCompanionRole(bot, role, "save");
}

void BotPopulationManager::DrainCompanionSetups()
{
    if (pending_companions_.empty())
        return;
    constexpr uint32 kCompanionLoginTimeoutMs = 60u * 1000u;
    for (auto it = pending_companions_.begin(); it != pending_companions_.end();)
    {
        BotId const id = it->first;
        PendingCompanion const pending = it->second;
        Player* bot = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        // Wait until the bot is in the world with its bot logic registered (Module::OnPlayerLogin).
        if (!bot || !bot->IsInWorld() || !Services::Registry().has(id))
        {
            // queued_ms is getMSTime(); OnWorldTick's now_ms counts from the module's first tick, so compare on one clock.
            if (getMSTimeDiff(pending.queued_ms, getMSTime()) > kCompanionLoginTimeoutMs)
            {
                TC_LOG_WARN("playerbot.v2", "[Companion] bot {} did not enter the world within {}s, setup dropped",
                            id, kCompanionLoginTimeoutMs / 1000u);
                it = pending_companions_.erase(it);
            }
            else
                ++it;
            continue;
        }
        it = pending_companions_.erase(it);
        Player* owner = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(pending.owner_guid_low));
        if (!owner || !owner->IsInWorld())
        {
            TC_LOG_INFO("playerbot.v2", "[Companion] {} logged in but its owner is offline, left as created", bot->GetName());
            continue;
        }
        FinishCompanionSetup(bot, owner, pending.role);
    }
}

namespace {

// One routine for putting a companion into its owner's group, with the reason spelled out when it cannot (audit B 3.1).
// A bot's group state has two links: GetGroup() (where it acts, a battleground group inside a match) and GetOriginalGroup() (its
// home party while it is in one); both are cleared of any group but the owner's before it joins.

// Can the owner's group take this bot at all? Empty string = yes.
std::string CompanionJoinRefusal(Player const* owner, Player const* bot)
{
    Group const* ownerGroup = owner->GetGroup();
    if (owner->InBattleground() || owner->InBattlegroundQueue() || (ownerGroup && (ownerGroup->isBGGroup() || ownerGroup->isBFGroup())))
        return "you are in a battleground or its queue";
    if (ownerGroup && ownerGroup->isLFGGroup())
        return "you are in a dungeon finder group";
    if (bot->InBattleground() || bot->InBattlegroundQueue())
        return "it is in a battleground or its queue";
    return {};
}

// Takes the bot out of every group but the owner's. A bot's own old group, or a human's two-man party it was half of, ends with it:
// that is logged, so an unexpected disband can be traced. Returns false when the bot still has a foreign group afterwards.
bool LeaveForeignGroups(Player* bot, Group const* ownerGroup)
{
    if (bot->GetGroupInvite())
        bot->UninviteFromGroup();    // a pending invite to another group; AddMember would clear the pointer but leave the invitee entry behind

    for (int link = 0; link < 2; ++link)
    {
        Group* group = link == 0 ? bot->GetGroup() : bot->GetOriginalGroup();   // read afresh: leaving the first can end the group
        if (!group || group == ownerGroup)
            continue;
        uint32 humans = 0;
        for (Group::MemberSlot const& slot : group->GetMemberSlots())
            if (slot.guid != bot->GetGUID())
                if (Player const* member = ObjectAccessor::FindConnectedPlayer(slot.guid))
                    if (member->GetSession() && !member->GetSession()->IsBot())
                        ++humans;
        TC_LOG_INFO("playerbot.v2", "[Companion] {} leaves its group ({} members, {} players left in it{})", bot->GetName(),
                    uint32(group->GetMembersCount()), humans, group->GetMembersCount() <= 2 ? ", the group ends with it" : "");
        group->RemoveMember(bot->GetGUID());
    }
    return (!bot->GetGroup() || bot->GetGroup() == ownerGroup) && (!bot->GetOriginalGroup() || bot->GetOriginalGroup() == ownerGroup);
}

// Puts the owner's companions back in the raid sub-groups they were stored in (RaidSubgroupPlan.h). Run after every join: a bot lands in
// the first free sub-group, and the plan trades places with whoever stands in the way, so the layout converges as the roster logs in.
void RestoreRaidSubgroups(Group* group, Player const* owner)
{
    if (!group || !group->isRaidGroup() || group->isBGGroup() || group->isBFGroup())
        return;
    uint32 const account = owner->GetSession() ? owner->GetSession()->GetAccountId() : 0;
    std::vector<Fleet::SubgroupMember> members;
    for (Group::MemberSlot const& slot : group->GetMemberSlots())
    {
        Fleet::SubgroupMember m;
        m.guid = slot.guid.GetCounter();
        m.group = slot.group;
        BotId const id = slot.guid.GetCounter();
        OwnerBinding const b = Services::Owners().GetOwner(id);
        m.movable = b.account_id != 0 && b.account_id == account;
        if (m.movable)
            m.want = Services::Owners().StoredSubgroup(id);
        members.push_back(m);
    }
    auto name_of = [&](uint64 low)
    {
        ObjectGuid const g = ObjectGuid::Create<HighGuid::Player>(low);
        std::string n;
        sCharacterCache->GetCharacterNameByGuid(g, n);
        return n;
    };
    uint32 moves = 0, swaps = 0;
    Fleet::SubgroupRestoreScope scope;
    for (Fleet::SubgroupMove const& mv : Fleet::PlanSubgroupMoves(members))
    {
        ObjectGuid const a = ObjectGuid::Create<HighGuid::Player>(mv.guid);
        if (mv.swap)
        {
            ObjectGuid const o = ObjectGuid::Create<HighGuid::Player>(mv.other);
            TC_LOG_INFO("playerbot.v2", "[roster_restore] {} (group {}) swaps places with {} (group {})", name_of(mv.guid),
                        uint32(group->GetMemberGroup(a)) + 1u, name_of(mv.other), uint32(group->GetMemberGroup(o)) + 1u);
            group->SwapMembersGroups(a, o);
            ++swaps;
        }
        else
        {
            TC_LOG_INFO("playerbot.v2", "[roster_restore] {} moves from raid group {} to {}", name_of(mv.guid),
                        uint32(group->GetMemberGroup(a)) + 1u, uint32(mv.group) + 1u);
            group->ChangeMembersGroup(a, mv.group);
            ++moves;
        }
    }
    if (moves || swaps)
        TC_LOG_INFO("playerbot.v2", "[roster_restore] raid of {}: {} move(s), {} swap(s)", group->GetMembersCount(), moves, swaps);

    // The order inside the sub-groups: the raid frame follows the member list, which is login order. Hand the owned members' places in
    // the list out again in the order they were saved in.
    std::vector<Fleet::OrderMember> order;
    for (Group::MemberSlot const& slot : group->GetMemberSlots())
    {
        BotId const id = slot.guid.GetCounter();
        Fleet::OrderMember om;
        om.guid = id;
        OwnerBinding const b = Services::Owners().GetOwner(id);
        om.movable = b.account_id != 0 && b.account_id == account;
        om.order = om.movable ? Services::Owners().StoredOrder(id) : uint16(0xFFFF);
        order.push_back(om);
    }
    if (std::vector<uint64> const want = Fleet::PlanMemberOrder(order); !want.empty())
    {
        std::vector<ObjectGuid> guids;
        for (uint64 low : want)
            guids.push_back(ObjectGuid::Create<HighGuid::Player>(low));
        group->ReorderMembers(guids);
        TC_LOG_INFO("playerbot.v2", "[roster_restore] raid of {}: member list put back in the saved order", group->GetMembersCount());
    }
}

// Joins the bot to the owner's group, creating it (a full party becomes a raid, whoever leads it). Empty string = joined.
std::string JoinOwnerGroup(Player* owner, Player* bot)
{
    if (std::string refusal = CompanionJoinRefusal(owner, bot); !refusal.empty())
        return refusal;

    Group* ownerGroup = owner->GetGroup();
    if (ownerGroup && bot->GetGroup() == ownerGroup)
    {
        RestoreRaidSubgroups(ownerGroup, owner);   // `.bot call all` on a raid that came back jumbled
        return {};      // a companion called again is often in the group already
    }

    if (!LeaveForeignGroups(bot, ownerGroup))
        return "it could not leave its other group";

    Group* group = ownerGroup;
    if (!group)
    {
        group = new Group();
        group->Create(owner);   // cannot fail for a home group (it asserts its leader joins)
        sGroupMgr->AddGroup(group);
    }
    if (group->IsFull() && !group->isRaidGroup())
        group->ConvertToRaid();
    if (group->IsFull())
        return "your raid is full (" + std::to_string(group->GetMembersCount()) + " members)";
    if (!group->AddMember(bot))
        return "your raid has no free sub-group slot";
    RestoreRaidSubgroups(group, owner);
    group->BroadcastGroupUpdate();
    return {};
}

} // namespace

void BotPopulationManager::FinishCompanionSetup(Player* bot, Player* owner, uint8 role_value)
{
    BotId const id = bot->GetGUID().GetCounter();
    uint8 const old_level = bot->GetLevel();
    if (owner->GetLevel() > old_level)
    {
        bot->GiveLevel(owner->GetLevel());
        bot->InitTalentForLevel();
        bot->SetXP(0);
    }

    ::Playerbot::API api(bot);
    // A companion levelled here never walks to a trainer, so it would fight with its level 1 kit.
    // Nor does it do its class quests: their abilities (Voidwalker, Bear Form, stances, totems...) and a hunter's pet.
    api.learn_class_quest_spells_free();
    uint32 const spells = api.learn_trainer_spells_free();
    uint32 const weapon_skills = api.learn_weapon_skills_free();
    // Riding at 40 and 60, so it mounts up with its owner.
    uint32 const riding = api.learn_riding_free();

    // The role the owner asked for (`.bot add <class> healer`), else the one stored with the companion (`.bot call`, a
    // relog), else what its talents say; pinned as the bot logic role and the virtual spec, and stored. Before the talent step,
    // which spends the points down the pinned spec's build.
    ApplyCompanionRole(bot, Role(role_value), "setup");
    ::Playerbot::Result const talents = bot->GetLevel() >= 10 ? api.apply_starter_talents() : ::Playerbot::Result::Ok;

    // A new companion's starter kit, or an old companion's pre-variety set, is re-rolled once with the varied
    // generator (OwnerRegistry::GearVaried); after that the re-gear pass keeps it up with its level.
    if (!Services::Owners().GearVaried(id))
    {
        uint32 changed = 0;
        if (RerollCompanionGear(bot, changed) == RerollResult::Done)
        {
            Services::Owners().MarkGearVaried(id);
            TC_LOG_INFO("playerbot.v2", "[Companion] {} one-time varied gear re-roll: {} slot(s) changed", bot->GetName(), changed);
        }
    }
    RegearNow(bot);

    // Its own place in a ring around the owner, so companions don't stand inside each other.
    FormationOffset slot_offset{ 5.0f, 0.0f };
    if (BotAI* ai = Services::Registry().ai(id))
    {
        std::array<bool, 64> used{};
        uint32 const account = owner->GetSession()->GetAccountId();
        for (BotId other : Services::Owners().BotsOwnedBy(account))
            if (other != id)
                if (BotAI const* o = Services::Registry().ai(other))
                    if (o->formation_slot() < used.size())
                        used[o->formation_slot()] = true;
        uint8 slot = 0;
        while (slot + 1u < used.size() && used[slot])
            ++slot;
        ai->set_formation_slot(slot);
        if (ai->formation_type() == FormationType::Free)
            ai->set_formation_type(FormationType::Spread);
        OwnerRegistry::SquadState squad;
        squad.formation_type  = uint8(ai->formation_type());
        squad.formation_slot  = slot;
        squad.follow_distance = ai->follow_distance() > 0.f ? ai->follow_distance() : 5.0f;
        squad.owner_verbose   = ai->verbose_logging();
        Services::Owners().SaveSquadState(id, squad);
        slot_offset = ComputeFormationOffset(ai->formation_type(), slot, squad.follow_distance);
    }

    // Called while dead somewhere else (a wipe the owner already left, a corpse in the instance the owner walked out
    // of): it comes back alive, as a summon does. One dead on the owner's map stays for the owner's release / rez.
    if (!bot->IsAlive() && bot->GetMapId() != owner->GetMapId())
    {
        TC_LOG_INFO("playerbot.v2", "[Companion] {} called while dead on map {}: revived to join {}", bot->GetName(),
                    bot->GetMapId(), owner->GetName());
        bot->ResurrectPlayer(1.0f);
        bot->SpawnCorpseBones();
    }

    // Inside an instance the owner's group holds what the map holds: a companion that would be the one too many
    // stays out of the instance and the group (Playerbot.Behavior.InstanceGroupCap).
    std::string capFull;
    bool const alreadyGrouped = owner->GetGroup() && bot->GetGroup() == owner->GetGroup();
    bool const instanceFull = !alreadyGrouped && CompanionSpawnFull(owner, id, capFull);
    if (instanceFull)
    {
        TC_LOG_INFO("playerbot.v2", "[Companion] {} not brought to {}: {}", bot->GetName(), owner->GetName(), capFull);
        ChatHandler(owner->GetSession()).PSendSysMessage("%s stays behind: %s.", bot->GetName().c_str(), capFull.c_str());
        return;
    }

    // Not in a battleground, a dungeon finder group or another match: it stays where it is, the owner is told why.
    std::string joinFailure = CompanionJoinRefusal(owner, bot);
    if (joinFailure.empty())
    {
        // Bring it to the owner (same map: a near teleport; otherwise a map change), straight into its place.
        // Same angle convention as the follow it gets next (MoveFollow: relative to the owner's facing).
        // Into the owner's instance the companion joins the group first: the door checks look at the group's instance, and a
        // stale lock on another instance is dropped before the port (Fleet/InstanceEntry.h).
        bool const joinFirst = owner->GetMap() && owner->GetMap()->IsDungeon() && !alreadyGrouped;
        if (joinFirst)
            joinFailure = JoinOwnerGroup(owner, bot);
        if (joinFailure.empty())
        {
            float x, y, z;
            owner->GetNearPoint(bot, x, y, z, slot_offset.distance, owner->GetOrientation() + slot_offset.angle_radians);
            Fleet::PrepareInstanceEntry(bot, owner->GetMapId());
            ::Playerbot::BotMovement::SafeTeleport(bot, owner->GetMapId(), x, y, z, owner->GetOrientation(), 0);
            // A level 10+ hunter's pet, once ever. Mid map change it is not in the world yet: CompanionUpkeep grants it then.
            GrantCompanionStarterPet(id, bot);

            // Group with the owner (see JoinOwnerGroup).
            if (!joinFirst)
                joinFailure = JoinOwnerGroup(owner, bot);
        }
    }
    bool const grouped = joinFailure.empty();

    TC_LOG_INFO("playerbot.v2", "[Companion] {} ready for {}: L{} -> L{}, {} trainer spells, {} weapon skills, riding {}, talents {}, grouped {}{}{}",
                bot->GetName(), owner->GetName(), uint32(old_level), uint32(bot->GetLevel()), spells, weapon_skills, riding,
                uint32(talents), grouped, grouped ? "" : ": ", joinFailure);
    if (grouped)
        ChatHandler(owner->GetSession()).PSendSysMessage("%s joined you: level %u.", bot->GetName().c_str(), uint32(bot->GetLevel()));
    else
        ChatHandler(owner->GetSession()).PSendSysMessage("%s is level %u but not in your group: %s.", bot->GetName().c_str(),
                                                          uint32(bot->GetLevel()), joinFailure.c_str());
}

bool BotPopulationManager::RegearNow(Player* p)
{
    if (!p || !p->IsInWorld()) return false;
    BotId const id = p->GetGUID().GetCounter();
    if (!Services::Lifecycle().is_bot(id)) return false;
    uint32 regear_count = 0;
    BackfillOne(id, p, getMSTime(), regear_count, /*ignore_cooldown*/ true);
    return regear_count > 0;
}

// The item level ceiling for a level-cap companion: its owner's average equipped item level plus a little slack, never below
// pre-raid gear and never above the configured tier (Owned::OwnerBasedCap). 0 = no owner gear to follow (not a companion, owner
// offline, Playerbot.Gear.Owned.MatchOwner off): the configured ceiling stands. World thread.
static std::mutex s_gear_override_lock;
static std::unordered_map<BotId, int> s_gear_override;

void BotPopulationManager::SetGearModeOverride(BotId bot, int mode)
{
    std::lock_guard<std::mutex> lock(s_gear_override_lock);
    if (mode < 0)
        s_gear_override.erase(bot);
    else
        s_gear_override[bot] = mode;
}

static Gear::Owned::GearMode CompanionGearMode(Player const* bot)
{
    {
        std::lock_guard<std::mutex> lock(s_gear_override_lock);
        auto const it = s_gear_override.find(bot->GetGUID().GetCounter());
        if (it != s_gear_override.end())
            return Gear::Owned::GearMode(it->second);
    }
    uint32 const account = Services::Owners().GetOwner(bot->GetGUID().GetCounter()).account_id;
    if (!account)
        return Gear::Owned::GearMode::Match;
    return Gear::Owned::EffectiveMode(Gear::Owned::GearMode(Services::Owners().OwnerGearMode(account)), Gear::OwnedAllowedModes(),
                                      bot->GetLevel() >= MaxPlayerLevel());
}

static int32 OwnerGearCap(Player const* bot)
{
    if (!Gear::OwnedMatchOwnerActive())
        return 0;
    uint64 const owner_guid = Services::Owners().OwnerCharacter(bot->GetGUID().GetCounter());
    if (!owner_guid)
        return 0;
    Player const* owner = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(owner_guid));
    if (!owner || owner == bot)
        return 0;
    std::vector<uint32> levels;
    for (uint8 s = EQUIPMENT_SLOT_START; s < EQUIPMENT_SLOT_END; ++s)
    {
        if (s == EQUIPMENT_SLOT_BODY || s == EQUIPMENT_SLOT_TABARD)
            continue;
        if (Item const* it = owner->GetItemByPos(INVENTORY_SLOT_BAG_0, s))
            levels.push_back(it->GetItemLevel(owner));
    }
    return Gear::Owned::OwnerBasedCap(Gear::Owned::AverageItemLevel(levels), Gear::OwnedOwnerSlack(),
                                      Gear::Owned::kPreRaidFloor, Gear::OwnedConfiguredCap());
}

// Raid-ready regear (Playerbot.Behavior.RaidGear) for a bot at the level cap: the generator's role-weighted picks go on
// where the slot is empty or the pick scores at least kRaidUpgradeRatio above the worn piece (Gear::ScoreItemRaid, the
// same score the generator ranks by), so existing gear is only ever upgraded. A two-hander replaces a mainhand + offhand
// pair only when it beats their combined score. Logs one "[gear]" line. Returns the number of slots changed. World thread.
static uint32 RaidRegearPass(Player* p, bool& refused_any, bool only_over_cap = false, bool reroll_all = false, bool vary = false)
{
    refused_any = false;
    uint8 const lvl = p->GetLevel();
    uint8 const cls = p->GetClass();
    Gear::GearGenerationContext ctx;
    ctx.level  = lvl;
    ctx.cls    = cls;
    ctx.race   = p->GetRace();
    ctx.spec   = uint16(::Playerbot::Classic::EffectiveSpecId(p));
    ctx.bot_id = p->GetGUID().GetCounter();
    ctx.wearer = p;
    ctx.role = ::Playerbot::Pool::RoleOf(ctx.bot_id);
    if (!ctx.world_bot())
    {
        ctx.gear_mode = CompanionGearMode(p);
        // "best" ignores what the owner wears; the tier modes carry their own ceiling in the generator.
        if (ctx.gear_mode == Gear::Owned::GearMode::Match)
            ctx.ilvl_cap = OwnerGearCap(p);
    }
    std::vector<Gear::GearItem> const picks = Gear::GenerateGearFor(ctx);
    // Variety (an owned level-cap companion, once): the generator picks among the best few pieces by the bot's guid, but the regear only
    // ever upgraded, so companions geared before variety all still wear the same top piece per slot. A worn piece goes for the bot's own
    // pick when the pick is the same quality or better and scores within 5% of it; a tier-set or best-in-slot choice is the owner's.
    bool const vary_ok = vary && !ctx.world_bot() && !Gear::Owned::IsCapMode(ctx.gear_mode);
    constexpr float kVaryFloor = 0.95f;

    auto slot_score = [&](Item const* it, uint8 slot) -> int32
    {
        return it && it->GetTemplate() ? ::Playerbot::Gear::ScoreItemRaid(it->GetTemplate(), cls, ctx.spec, lvl, slot) : 0;
    };
    // Takes the worn piece off into the bags. False when it can't go there.
    auto stow = [&](uint8 slot)
    {
        Item* worn = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!worn)
            return true;
        uint16 const pos = (uint16(INVENTORY_SLOT_BAG_0) << 8) | slot;
        ItemPosCountVec dest;
        if (p->CanUnequipItem(pos, /*swap*/ false) != EQUIP_ERR_OK ||
            p->CanStoreItem(NULL_BAG, NULL_SLOT, dest, worn, false) != EQUIP_ERR_OK)
            return false;
        p->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
        p->StoreItem(dest, worn, true);
        return true;
    };

    uint32 changed = 0;
    // A pick can be refused only because of the order the slots go on in (a unique ring still worn in the paired slot, a
    // shield next to a two-hander): a second pass picks those up.
    for (uint32 pass = 0; pass < 2 && (pass == 0 || refused_any); ++pass)
    {
        refused_any = false;
        for (Gear::GearItem const& g : picks)
        {
            ItemTemplate const* gtpl = sObjectMgr->GetItemTemplate(g.item_entry);
            if (!gtpl)
                continue;
            Item* cur = p->GetItemByPos(INVENTORY_SLOT_BAG_0, g.slot);
            if (cur && cur->GetEntry() == g.item_entry)
                continue;
            // The offhand cannot be worn next to a two-hander that stayed.
            if (g.slot == EQUIPMENT_SLOT_OFFHAND)
            {
                Item const* mh = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
                if (mh && mh->GetTemplate() && mh->GetTemplate()->GetInventoryType() == INVTYPE_2HWEAPON)
                {
                    refused_any = refused_any || gtpl->GetInventoryType() == INVTYPE_SHIELD;   // the second pass, once the one-hander is on
                    continue;
                }
            }
            int32 const pick_score = ::Playerbot::Gear::ScoreItemRaid(gtpl, cls, ctx.spec, lvl, g.slot);
            Item* off_for_2h = nullptr;
            if (g.slot == EQUIPMENT_SLOT_MAINHAND && gtpl->GetInventoryType() == INVTYPE_2HWEAPON)
                off_for_2h = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
            if (cur)
            {
                int32 cur_score = slot_score(cur, g.slot) + slot_score(off_for_2h, EQUIPMENT_SLOT_OFFHAND);
                // A worn PvP piece goes whatever it scores. Otherwise only a clear upgrade: 10% more, never less.
                bool const worn_pvp = cur->GetTemplate() && ::Playerbot::Gear::IsPvpItem(cur->GetTemplate()) &&
                                      !(ctx.world_bot() && Gear::WorldPvpSet(ctx.bot_id));
                // A world bot's piece above the world cap (or an epic beyond its share) goes down to the pick, whatever it scores.
                bool const worn_over = ctx.world_bot() && Gear::WorldGearOver(cur->GetTemplate(), lvl, ctx.bot_id, g.slot);
                if (only_over_cap && !worn_over)
                    continue;
                // Never swap one over-cap piece for another.
                if (worn_over && Gear::WorldGearOver(gtpl, lvl, ctx.bot_id, g.slot))
                    continue;
                // A world bot's one-time re-roll: whatever it wears that differs from its own per-slot pick goes.
                // A piece of the tier set the owner asked for goes on over whatever is worn, even a better scoring piece.
                bool const tier_pick = Gear::IsTierSetPick(cls, ctx.gear_mode, g.item_entry);
                // An off hand that is not a weapon on a class that must have one (rogue, non-protection warrior, hunter, enhancement shaman) goes for any weapon pick.
                bool const dead_offhand = ::Playerbot::Gear::OffhandMustBeWeapon(cls, ctx.spec) && g.slot == EQUIPMENT_SLOT_OFFHAND && cur->GetTemplate() &&
                                          cur->GetTemplate()->GetClass() != ITEM_CLASS_WEAPON && gtpl->GetClass() == ITEM_CLASS_WEAPON;
                // A protection tank's hands: a two-hander in the main hand goes for the one-hander pick, and an off hand that is not a shield
                // goes for the shield pick, whatever they score (n43 onyxia_full: two of three protection warriors in the Tier 1 gear had no
                // shield, no Shield Slam and a third of the armor; the 10 % upgrade rule kept the two-hander).
                bool const shield_tank = ctx.spec == 73 || ctx.spec == 66;
                bool const shield_fix = shield_tank && cur->GetTemplate() &&
                    ((g.slot == EQUIPMENT_SLOT_MAINHAND && cur->GetTemplate()->GetInventoryType() == INVTYPE_2HWEAPON && gtpl->GetInventoryType() != INVTYPE_2HWEAPON) ||
                     (g.slot == EQUIPMENT_SLOT_OFFHAND && gtpl->GetInventoryType() == INVTYPE_SHIELD && cur->GetTemplate()->GetInventoryType() != INVTYPE_SHIELD));
                bool const vary_swap = vary_ok && cur_score > 0 && !off_for_2h && cur->GetTemplate() &&
                                       gtpl->GetQuality() >= cur->GetTemplate()->GetQuality() && float(pick_score) >= float(cur_score) * kVaryFloor;
                bool const better = shield_fix || vary_swap || tier_pick || worn_pvp || dead_offhand || worn_over || (reroll_all && ctx.world_bot()) || (cur_score <= 0 ? pick_score > cur_score
                                                                : float(pick_score) >= float(cur_score) * Gear::kRaidUpgradeRatio);
                if (!better)
                    continue;
            }
            if (off_for_2h && (p->CanUseItem(gtpl) != EQUIP_ERR_OK || !stow(EQUIPMENT_SLOT_OFFHAND)))
                continue;
            uint16 eq_dest = 0;
            if (p->CanEquipNewItem(g.slot, eq_dest, g.item_entry, /*swap*/ cur != nullptr) != EQUIP_ERR_OK)
            {
                refused_any = true;
                continue;
            }
            ItemPosCountVec dest;
            bool staged = p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, g.item_entry, 1) == EQUIP_ERR_OK;
            if (!staged && FreeOneJunkBagSlot(p))
            {
                dest.clear();
                staged = p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, g.item_entry, 1) == EQUIP_ERR_OK;
            }
            if (!staged)
            {
                // Bags full, no junk to drop: the clearly worse worn piece makes way (as a player with a full pack
                // would), checked equippable first so a refusal never leaves the slot naked.
                if (cur)
                    p->DestroyItem(INVENTORY_SLOT_BAG_0, g.slot, true);
                if (p->EquipNewItem(eq_dest, g.item_entry, ItemContext::NONE, true))
                    ++changed;
                continue;
            }
            Item* item = p->StoreNewItem(dest, g.item_entry, true);
            if (!item)
                continue;
            uint16 const src = (uint16(item->GetBagSlot()) << 8) | item->GetSlot();
            uint16 const dst = (uint16(INVENTORY_SLOT_BAG_0) << 8) | g.slot;
            p->SwapItem(src, dst);
            if (p->GetItemByPos(INVENTORY_SLOT_BAG_0, g.slot) == item)
                ++changed;
            else
                p->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);   // refused after all: don't leave the copy
        }
    }
    if (changed)
        p->AutoUnequipOffhandIfNeed();

    uint32 sum = 0, cnt = 0;
    for (uint8 s = EQUIPMENT_SLOT_START; s < EQUIPMENT_SLOT_END; ++s)
    {
        if (s == EQUIPMENT_SLOT_BODY || s == EQUIPMENT_SLOT_TABARD)
            continue;
        if (Item const* eq = p->GetItemByPos(INVENTORY_SLOT_BAG_0, s))
        {
            sum += eq->GetItemLevel(p);
            ++cnt;
        }
    }
    TC_LOG_INFO("playerbot.v2", "[gear] {} {} mode={} avg_ilvl={} upgraded={} picks={} cap={}", p->GetName(),
                ::Playerbot::Gear::RaidRoleName(::Playerbot::Gear::RaidRoleForBot(cls, ctx.spec)), Gear::Owned::ModeName(ctx.gear_mode), cnt ? sum / cnt : 0u, changed,
                picks.size(), ctx.world_bot() ? Gear::LiveWorldIlvlCap() : (ctx.ilvl_cap > 0 ? ctx.ilvl_cap : Gear::LiveRaidIlvlCap()));
    return changed;
}

BotPopulationManager::RerollResult BotPopulationManager::RerollCompanionGear(Player* p, uint32& changed_slots)
{
    changed_slots = 0;
    if (!p || !p->IsInWorld())
        return RerollResult::NotACompanion;
    BotId const id = p->GetGUID().GetCounter();
    // A player's own character keeps the gear its player chose (see IsPlayersOwnCharacter).
    if (!Services::Lifecycle().is_bot(id) || Services::Owners().GetOwner(id).account_id == 0 || IsPlayersOwnCharacter(p))
        return RerollResult::NotACompanion;
    // Armor can't be changed in combat (CanEquipItem: NOT_IN_COMBAT) and a dead bot can't change gear either; every
    // slot would be refused and the one-time re-roll spent on nothing.
    if (p->IsInCombat() || !p->IsAlive())
        return RerollResult::NotNow;

    // Classic weapon skills are trained: without them every weapon pick of an untrained type is filtered out.
    ::Playerbot::API(p).learn_weapon_skills_free();

    // A level-cap companion is raid-geared instead (role stat weights, upgrades only; see RaidRegearPass).
    if (Gear::RaidGearActive(p->GetLevel()))
    {
        bool refused = false;
        changed_slots = RaidRegearPass(p, refused);
        // Once per companion: swap the clone-top pieces for the bot's own near-equal picks.
        bool const first_vary = !Services::Owners().GearVariedRaid(id);
        if (first_vary)
        {
            bool refused_v = false;
            uint32 const varied = RaidRegearPass(p, refused_v, /*only_over_cap=*/false, /*reroll_all=*/false, /*vary=*/true);
            changed_slots += varied;
            if (!refused_v)
            {
                Services::Owners().MarkGearVariedRaid(id);
                TC_LOG_INFO("playerbot.v2", "[gear_vary] {}: {} piece(s) swapped for its own picks", p->GetName(), varied);
            }
        }
        if (EnsureShieldTankWeapon(p))
            ++changed_slots;
        return RerollResult::Done;
    }

    uint8 const lvl = p->GetLevel();
    Gear::GearGenerationContext ctx;
    ctx.level  = lvl;
    ctx.cls    = p->GetClass();
    ctx.race   = p->GetRace();
    ctx.spec   = uint16(::Playerbot::Classic::EffectiveSpecId(p));
    ctx.bot_id = id;
    ctx.wearer = p;   // Player::CanUseItem + weapon skill gate; the pools already exclude honor-rank and SoD items
    ctx.role = ::Playerbot::Pool::RoleOf(id);
    std::vector<Gear::GearItem> const picks = Gear::GenerateGearFor(ctx);

    // The worn piece stays when it is better than the pick: a higher quality (looted or owner-given blues and
    // epics), or the same quality and more than the variety band above it. Within the band the two are peers, which
    // is exactly the choice the generator varies over, so the swap is never a downgrade.
    auto worn_is_better = [&](Item const* cur, ItemTemplate const* gtpl)
    {
        ItemTemplate const* ctpl = cur->GetTemplate();
        if (!ctpl)
            return false;
        // A companion below the cap is ranked by the role score (a blue counts a little more): the worn piece stays unless
        // the pick is a clear upgrade over it, so the re-roll never swaps for a peer or a lesser piece.
        if (Gear::OwnedLevelGearActive(lvl, ctx.role))
            return !Gear::Owned::IsUpgrade(false, Gear::OwnedSlotScore(ctpl, ctx.cls, ctx.spec, lvl, cur->GetSlot()),
                                           Gear::OwnedSlotScore(gtpl, ctx.cls, ctx.spec, lvl, cur->GetSlot()));
        if (ctpl->GetQuality() != gtpl->GetQuality())
            return ctpl->GetQuality() > gtpl->GetQuality();
        return int32(cur->GetItemLevel(p)) > ::Playerbot::Gear::EffectiveItemLevelForLevel(gtpl, lvl) + Gear::kVarietyIlvlBand;
    };
    // Takes the worn piece off into the bags (the client's auto-store). False when it can't go there.
    auto stow = [&](uint8 slot)
    {
        Item* worn = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!worn)
            return true;
        uint16 const pos = (uint16(INVENTORY_SLOT_BAG_0) << 8) | slot;
        ItemPosCountVec dest;
        if (p->CanUnequipItem(pos, /*swap*/ false) != EQUIP_ERR_OK ||
            p->CanStoreItem(NULL_BAG, NULL_SLOT, dest, worn, false) != EQUIP_ERR_OK)
            return false;
        p->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
        p->StoreItem(dest, worn, true);
        return true;
    };

    // A pick can be refused only because of the order the slots go on in: a unique ring or trinket still worn in
    // the paired slot, or a shield next to a two-hander that the mainhand pick replaces. A second pass picks those up.
    bool refused = false;
    for (uint32 pass = 0; pass < 2 && (pass == 0 || refused); ++pass)
    {
        refused = false;
        for (Gear::GearItem const& g : picks)
        {
            ItemTemplate const* gtpl = sObjectMgr->GetItemTemplate(g.item_entry);
            if (!gtpl)
                continue;
            Item const* cur = p->GetItemByPos(INVENTORY_SLOT_BAG_0, g.slot);
            if (cur && (cur->GetEntry() == g.item_entry || worn_is_better(cur, gtpl)))
                continue;
            // A two-hander needs the offhand free (the generator gives no offhand with one). Taken off only once the
            // two-hander is known to be wearable, and only into the bags.
            if (g.slot == EQUIPMENT_SLOT_MAINHAND && gtpl->GetInventoryType() == INVTYPE_2HWEAPON &&
                p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND))
            {
                if (p->CanUseItem(gtpl) != EQUIP_ERR_OK || !stow(EQUIPMENT_SLOT_OFFHAND))
                    continue;
            }
            uint16 eq_dest = 0;
            if (p->CanEquipNewItem(g.slot, eq_dest, g.item_entry, /*swap*/ cur != nullptr) != EQUIP_ERR_OK)
            {
                refused = true;
                continue;
            }
            ItemPosCountVec dest;
            bool staged = p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, g.item_entry, 1) == EQUIP_ERR_OK;
            if (!staged && FreeOneJunkBagSlot(p))
            {
                dest.clear();
                staged = p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, g.item_entry, 1) == EQUIP_ERR_OK;
            }
            if (!staged)
            {
                // Bags full: an empty slot can still be filled directly. A worn piece is never destroyed for a
                // re-roll; it stays until there is room for it in the bags.
                if (!cur && p->EquipNewItem(eq_dest, g.item_entry, ItemContext::NONE, true))
                    ++changed_slots;
                continue;
            }
            Item* item = p->StoreNewItem(dest, g.item_entry, true);
            if (!item)
                continue;
            // SwapItem: the worn piece goes to the bag slot the new one was staged in.
            uint16 const src = (uint16(item->GetBagSlot()) << 8) | item->GetSlot();
            uint16 const dst = (uint16(INVENTORY_SLOT_BAG_0) << 8) | g.slot;
            p->SwapItem(src, dst);
            if (p->GetItemByPos(INVENTORY_SLOT_BAG_0, g.slot) == item)
                ++changed_slots;
            else
                p->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);   // refused after all: don't leave the copy
        }
    }
    if (changed_slots)
        p->AutoUnequipOffhandIfNeed();
    // A shield tank whose better two-hander stayed on would otherwise run without its shield.
    if (EnsureShieldTankWeapon(p))
        ++changed_slots;
    TC_LOG_INFO("playerbot.v2", "[GearReroll] {} L{}: {} pick(s), {} slot(s) changed", p->GetName(), uint32(lvl),
                picks.size(), changed_slots);
    return RerollResult::Done;
}

// playtest 2026-10-08 (MC raid): WSG bots (Dascard, Nadoc, Brahad) sat in his raid group while he was dead in Molten Core, and their combat in
// the battleground greyed out his Release Spirit ("a member of your group is in combat"). A pool / JIT bot has no business in a human's
// regular group while it is in a battleground: its battleground group is the BG group, and an earlier group (the "original group" a
// battleground restores on leaving, or one it never left) is dropped here. Owned companions keep their owner's group. Every 10 s.
void BotPopulationManager::BgGroupHygiene(uint32 now_ms)
{
    constexpr uint32 kHygieneIntervalMs = 10u * 1000u;
    if (last_bg_group_hygiene_ms_ && (now_ms - last_bg_group_hygiene_ms_) < kHygieneIntervalMs)
        return;
    last_bg_group_hygiene_ms_ = now_ms;

    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        if (Services::Owners().GetOwner(id).account_id != 0)
            return;
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld() || !p->InBattleground())
            return;
        for (Group* g : { p->GetGroup(), p->GetOriginalGroup() })
        {
            if (!g || g->isBGGroup() || g->isBFGroup())
                continue;
            // Only a group that holds a real player matters (a bot-only group harms nobody).
            bool has_human = false;
            for (GroupReference const& ref : g->GetMembers())
                if (Player const* m = ref.GetSource())
                    if (!Services::Lifecycle().is_bot(m->GetGUID().GetCounter())) { has_human = true; break; }
            if (!has_human)
                continue;
            TC_LOG_INFO("playerbot.v2", "[bg_group_leave] {} (bot {}) is in a BG and in a regular group led by {} ({} members): leaving it",
                        p->GetName(), id, g->GetLeaderGUID().ToString(), g->GetMembersCount());
            g->RemoveMember(p->GetGUID());
        }
    });
}

// playtest 2026-10-08: world, pool and BG bots are T1 at most and mostly blue; bots made before that cap (or logged in with a worn T3 piece)
// kept their gear until the 5 minute sweep, and then only 25 per sweep. Every 15 s this walks the online bots, and any bot without an owner
// that wears a piece above the world cap (or an epic beyond its share) is regeared right away under the world rules, up to 8 bots a pass.
// Covers login (the next pass) and BG entry (the bot is online and in the world by then). Logs one [gear] scrub line per bot.
void BotPopulationManager::ScrubWorldGear(uint32 now_ms)
{
    constexpr uint32 kScrubIntervalMs = 15u * 1000u;
    constexpr uint32 kScrubPerPass = 8u;
    if (last_gear_scrub_ms_ && (now_ms - last_gear_scrub_ms_) < kScrubIntervalMs)
        return;
    last_gear_scrub_ms_ = now_ms;

    uint32 scrubbed = 0, rerolled = 0;
    constexpr uint32 kRerollPerPass = 4u;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        if (scrubbed >= kScrubPerPass || !::Playerbot::Pool::UsesWorldGear(::Playerbot::Pool::RoleOf(id)))
            return;
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!p || !p->IsInWorld() || !p->IsAlive() || p->IsInCombat() || IsPlayersOwnCharacter(p) || p->GetLevel() < 5)
            return;
        uint32 over = 0;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item const* eq = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (eq && eq->GetTemplate() && Gear::WorldGearOver(eq->GetTemplate(), p->GetLevel(), id, slot))
                ++over;
        }
        // One re-roll per bot and session: gear picked before the per-slot variety (or by the older rules) is replaced by the bot's
        // own picks. The picks are fixed per bot and slot, so once worn a second pass changes nothing.
        static std::unordered_set<BotId> s_rerolled;
        if (!over)
        {
            if (rerolled >= kRerollPerPass || s_rerolled.count(id))
                return;
            s_rerolled.insert(id);
            ++rerolled;
            bool refused_r = false;
            uint32 const changed_r = RaidRegearPass(p, refused_r, /*only_over_cap=*/false, /*reroll_all=*/true);
            TC_LOG_INFO("playerbot.v2", "[gear] reroll {} (bot {}, level {}): {} slot(s) changed", p->GetName(), id, uint32(p->GetLevel()), changed_r);
            return;
        }
        s_rerolled.insert(id);
        // A bot below the level cap is not "raid gear" for BackfillOne (which only ever upgrades there), so the scrub runs
        // its own replace-only pass at any level; a slot the generator has no under-cap pick for is tried three times, then left.
        static std::unordered_map<BotId, uint8> s_scrub_tries;
        uint8& tries = s_scrub_tries[id];
        if (tries >= 3)
            return;
        ++scrubbed;
        ++tries;
        bool refused = false;
        uint32 const changed = RaidRegearPass(p, refused, /*only_over_cap=*/true);
        if (changed)
            tries = 0;
        uint32 left = 0;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item const* eq = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (eq && eq->GetTemplate() && Gear::WorldGearOver(eq->GetTemplate(), p->GetLevel(), id, slot))
                ++left;
        }
        TC_LOG_INFO("playerbot.v2", "[gear] scrub {} (bot {}, level {}): {} slot(s) above the world cap, replaced {}, {} left{}",
                    p->GetName(), id, uint32(p->GetLevel()), over, changed, left, left && tries >= 3 ? " (no under-cap pick, giving up)" : "");
    });
}

// Role score of an item for a companion below the cap (quality adjusted); the worn item's score for a null template is 0.
static int32 OwnedGearScore(ItemTemplate const* tpl, Gear::GearGenerationContext const& ctx, uint8 slot)
{
    return tpl ? Gear::OwnedSlotScore(tpl, ctx.cls, ctx.spec, ctx.level, slot) : 0;
}

void BotPopulationManager::BackfillOne(BotId id, Player* p, uint32 now_ms, uint32& regear_count, bool ignore_cooldown)
{
    // Unrenderable gear would be a client crash rather than a gearing
    // quality question, so it is checked for every online bot regardless of
    // level and BEFORE the under-gear gate below: a bot in a full high-ilvl
    // set the client cannot draw is by definition NOT under-geared and would
    // return at that gate untouched. Does not consume the regear cap - the
    // scan is 19 appearance lookups and the generator only runs when a slot
    // is bad.
    //
    // NOTE: on the live fleet this finds nothing (verified 2026-09-16, zero
    // [GearHeal] lines with the call confirmed invoked). The gear once
    // blamed for the inspect crash renders fine; the real cause was a
    // corrupted packet serializer, fixed separately. This is defence in
    // depth against a condition that has not actually been observed.
    HealUnrenderableGear(p);

    // Thrown weapon for the pull-back opener, every pass (cheap pre-checks), whatever the under-gear gate below says.
    EnsureThrownRanged(p);

    // L>=5 (was L>=10): an under-geared bot BELOW 10 is exactly the death-
    // spiral victim that most needs help — it can't out-level into the old
    // L>=10 gate because it keeps dying to quest mobs in ilvl-~1 starter gear
    // (observed 2026-06-21: Morthan, L9 warlock, effective ilvl 1, looping
    // die->repair->die on a 5-mob kill quest it can't survive). The old gate
    // only existed to mirror the professions gate (LearnProfessions needs
    // L>=10); GEARING has no such dependency. The `under_geared` test below
    // (weaponless || avg_eff < level) is the real safety — a properly-geared
    // low bot is never touched — so this only rescues genuinely-stuck bots.
    // L1-4 stay excluded: they're in the immediate starter window where
    // ilvl-1 gear IS level-appropriate and quest greens arrive within minutes.
    if (p->GetLevel() < 5) return;

    // Shield-tank weapon correction runs for EVERY online shield-tank each
    // pass (cheap spec/slot pre-check inside), INDEPENDENT of the under-gear
    // gate below: a prot tank on a 2H is "geared" by average ilvl yet
    // critically mis-equipped (no shield). Does not consume the regear cap.
    if (EnsureShieldTankWeapon(p))
        TC_LOG_INFO("playerbot.v2",
            "[GearBackfill] shield-tank weapon fix: {} -> 1H+shield", p->GetName());

    const uint8 lvl = p->GetLevel();
    const bool weaponless =
        !p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);

    // A player's own characters (summoned alts) keep the SAFE legacy behavior:
    // fill an empty mainhand only, never touch armor. A real player's alt may run
    // intentional transmog / twink gear (owner decision 2026-06-10 keeps
    // them combat-functional, but we must not overwrite owner choices).
    // Adopted pool bots are geared normally, see IsPlayersOwnCharacter.
    const bool is_altbot = IsPlayersOwnCharacter(p);

    // A companion (bound to an owner, not the player's own character) is regeared when it levels up and when it enters an
    // instance, whatever its average says. Not in combat or dead (nothing can be equipped then): it stays due.
    ::Playerbot::Pool::BotRole const bot_role = ::Playerbot::Pool::RoleOf(id);
    bool const companion = !is_altbot && Services::Owners().GetOwner(id).account_id != 0;
    uint64 const refresh_key = Gear::Owned::RefreshKey(lvl, (p->GetMap() && (p->GetMap()->IsDungeon() || p->GetMap()->IsRaid())) ? p->GetMap()->GetInstanceId() : 0u,
                                                       companion ? CompanionGearMode(p) : Gear::Owned::GearMode::Match);
    bool refresh_due = false;
    if (companion && p->IsAlive() && !p->IsInCombat())
    {
        auto const rk = gear_refresh_key_.find(id);
        refresh_due = rk == gear_refresh_key_.end() || rk->second != refresh_key;
    }
    // Owned, below the cap: the best green or blue for the level by role weights (Gear::OwnedLevelGearActive).
    bool const owned_lv = companion && Gear::OwnedLevelGearActive(lvl, bot_role);

    // Level-cap bots: raid-ready gear (Playerbot.Behavior.RaidGear). Role stat weights decide, an occupied slot is only
    // upgraded when the pick scores 10% higher; the under-gear tests below (average item level against the level) are
    // blind to a bot in full level-60 greens. A player's own alt keeps its own gear (is_altbot).
    if (!is_altbot && Gear::RaidGearActive(lvl))
    {
        if (!ignore_cooldown && !refresh_due)
        {
            auto it = gear_backfill_skip_until_.find(id);
            if (it != gear_backfill_skip_until_.end() && it->second > now_ms)
                return;
        }
        // Gear can't be changed in combat or while dead.
        if (p->IsInCombat() || !p->IsAlive())
            return;
        ++regear_count;
        {
            ::Playerbot::API api(p);
            api.learn_weapon_skills_free();
            api.learn_riding_free();
        }
        bool refused = false;
        if (companion)
            gear_refresh_key_[id] = refresh_key;
        if (RaidRegearPass(p, refused) > 0)
            gear_backfill_skip_until_.erase(id);
        else
            gear_backfill_skip_until_[id] = now_ms + 30u * 60u * 1000u;   // nothing better to put on: look again in 30 min
        return;
    }

    // Average equipped EFFECTIVE ilvl over filled slots — the value the
    // bot actually wears (GetItemLevel(p) applies 12.0 scaling curves).
    // Also track the WORST slot: see the outlier test below.
    uint32 unusable_mask = 0;   // equipment slots holding a piece above the bot's level
    uint32 eff_sum = 0, eff_cnt = 0;
    uint32 worst_eff = std::numeric_limits<uint32>::max();
    uint8  worst_slot = 0;
    for (uint8 s = EQUIPMENT_SLOT_START; s < EQUIPMENT_SLOT_END; ++s)
    {
        Item const* eq = p->GetItemByPos(INVENTORY_SLOT_BAG_0, s);
        if (!eq) continue;
        // A piece the bot is too low a level to wear (a bot levelled down by command keeps the gear of its old level, and the
        // item level of such a piece would lift the average above its level, hiding that it wears nothing it can use): it is
        // out of the averages and is replaced by any pick it can wear.
        if (!is_altbot && eq->GetTemplate() && p->CanUseItem(eq->GetTemplate()) == EQUIP_ERR_CANT_EQUIP_LEVEL_I)
        {
            unusable_mask |= 1u << s;
            continue;
        }
        uint32 const e = eq->GetItemLevel(p);
        eff_sum += e;
        ++eff_cnt;
        // Shirt and tabard are cosmetic and legitimately sit at ilvl ~1
        // forever; including them would flag every bot wearing one, every
        // pass, permanently.
        if (s == EQUIPMENT_SLOT_BODY || s == EQUIPMENT_SLOT_TABARD)
            continue;
        if (e < worst_eff) { worst_eff = e; worst_slot = s; }
    }
    const uint32 avg_eff = eff_cnt ? eff_sum / eff_cnt : 0;

    // A single catastrophic slot is INVISIBLE to an average. Live case
    // 2026-09-16, Sellarino (L16 Protection warrior): eight good plate
    // pieces, a neck, a trinket and a rare shield, plus a WHITE ilvl-1
    // starter sword in the mainhand. avg_eff sat above its level, so the
    // bot never qualified as under-geared and the weapon was never looked
    // at again. EnsureShieldTankWeapon did not catch it either - it returns
    // as soon as it sees a shield in the offhand and never inspects the
    // mainhand.
    //
    // Flag a bot whose worst non-cosmetic slot wears at less than a third of
    // its own average. Self-calibrating, so it needs no level/ilvl scale
    // assumptions (the linear TargetIlvlForLevel ramp is unusable at low
    // level - it reads ~120 at L16, above anything an L16 can wear). The
    // strict per-slot guards in the re-gear loop below still apply, so this
    // only opens the door: it cannot downgrade a slot, and a bot whose gear
    // is merely uneven sees swapped=0 and parks on the failed-backfill
    // cooldown.
    const bool slot_outlier =
        eff_cnt >= 4 && worst_eff != std::numeric_limits<uint32>::max() &&
        worst_eff * 3 < avg_eff;

    // Under-geared if missing a weapon OR wearing effective ilvl below the
    // character level. Level-appropriate gear has effective ilvl >= level
    // (an L40 green is ~ilvl 40-50), while starter gear sits at ~ilvl 1-5
    // regardless of level — so avg_eff < lvl cleanly separates the ilvl-5/15
    // victims from properly-geared bots WITHOUT the scale mismatch of the
    // linear TargetIlvlForLevel ramp (which is ~120 at L16, far above what
    // any L16 item wears at, so it would flag everyone forever). Cheap: no
    // generator call needed to decide.
    // DUNGEON-READINESS bar (2026-06-28): the open-world `avg_eff < lvl`
    // gate deems a bot wearing level-appropriate GREENS "geared" — fine for
    // questing, but too weak for scaled 5-man content. Live in Deadmines: a
    // Holy Priest healer at avg_eff 48 (L30) could NOT out-heal the harbor
    // Defias caster burst, yet the SAME generator gives the tank ilvl ~75 at
    // L30 — so the healer/rogue (ilvl 36-48) sat 25+ ilvl below their own
    // achievable gear and the squad death-grinded. Inside a dungeon/raid we
    // therefore re-gear toward the generator's potential whenever the worn
    // avg is well under ~2x level (the low-level generator output sits near
    // 2.3-2.5x level). The STRICT per-slot guards below (entry-equality +
    // effective-ilvl-upgrade + quality) still apply, so this can only swap in
    // pieces that are genuinely better and NEVER downgrades a good item — a
    // well-geared dungeon bot (tank ilvl75 / hunter ilvl74 here) is left
    // untouched; only the genuinely under-geared support gets pulled up.
    const bool in_instance_run =
        p->GetMap() && (p->GetMap()->IsDungeon() || p->GetMap()->IsRaid());
    // Dungeon-readiness bar (2026-06-29): the old lvl*2 bar (60 at L30) sat
    // BELOW the generator's own achievable ceiling (~2.3-2.5x level, i.e.
    // 69-75 at L30 — the tank/hunter hit 75), so a support bot at avg_eff 67
    // was deemed "geared" yet sat ~8 ilvl under its OWN best gear and never
    // got pulled up. Measured live in Deadmines: that 8-25 ilvl support
    // deficit is the throughput wall on the scaled ship deck (Ripsnarl 213k +
    // Cookie 186k). Raise the bar to ~2.5x level so support is re-geared
    // toward the generator's potential. The strict per-slot guards below
    // (entry-equality + strict effective-upgrade + quality) mean this only
    // swaps in genuinely better pieces and NEVER downgrades — a bot already
    // at its class/slot ceiling sees swapped=0 and is parked on the
    // failed-backfill cooldown, so the higher bar costs at most one no-op
    // GenerateGearFor per cooldown window. Re-gearing also replaces broken
    // (0%-durability) pieces with fresh 100%-dur gear — the orphaned-DPS
    // durability decay seen on the deck.
    // Classic (level cap 60): the generator's own target is level + 5, so the retail-era 2.5 x level bar (52 at level 21) could
    // never be met and a bot in a dungeon stayed "under-geared", got nothing and was parked every half hour. The bar there is the
    // generator's target.
    const uint32 dungeon_bar = MaxPlayerLevel() <= 60u ? uint32(Gear::TargetIlvlForLevel(lvl)) : (uint32(lvl) * 5u) / 2u;
    // An owned bot below the cap is kept current by its refresh points instead (level-up, instance entry).
    const bool dungeon_undergear = in_instance_run && !owned_lv && (avg_eff < dungeon_bar);
    // An EMPTY slot is invisible to both tests above: the average and the worst slot only look at what is worn, so a
    // companion with a full set but no belt (Testwar, 2026-10-05) was "geared" forever. Altbots never get armor, so
    // only bots count. A slot the pool has nothing for is retried at most once per failed-backfill cooldown.
    uint8 empty_slot = 0xFF;
    if (!is_altbot)
    {
        static constexpr std::array<uint8, 14> kMustWear = {
            EQUIPMENT_SLOT_HEAD, EQUIPMENT_SLOT_NECK, EQUIPMENT_SLOT_SHOULDERS, EQUIPMENT_SLOT_CHEST, EQUIPMENT_SLOT_WAIST,
            EQUIPMENT_SLOT_LEGS, EQUIPMENT_SLOT_FEET, EQUIPMENT_SLOT_WRISTS, EQUIPMENT_SLOT_HANDS, EQUIPMENT_SLOT_FINGER1,
            EQUIPMENT_SLOT_FINGER2, EQUIPMENT_SLOT_TRINKET1, EQUIPMENT_SLOT_TRINKET2, EQUIPMENT_SLOT_BACK };
        for (uint8 s : kMustWear)
            if (!p->GetItemByPos(INVENTORY_SLOT_BAG_0, s)) { empty_slot = s; break; }
        // Classic: paladins, shamans and druids have no ranged weapon (their relics are not generated)
        uint8 const cls = p->GetClass();
        if (empty_slot == 0xFF && cls != CLASS_PALADIN && cls != CLASS_SHAMAN && cls != CLASS_DRUID &&
            !p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED))
            empty_slot = EQUIPMENT_SLOT_RANGED;
    }
    const bool under_geared =
        weaponless || (avg_eff < uint32(lvl)) || dungeon_undergear || slot_outlier || empty_slot != 0xFF || unusable_mask != 0 || (owned_lv && refresh_due);
    if (!under_geared) return;

    // Skip bots parked on the failed-backfill cooldown (could not be
    // re-geared last time — bags full / equip refused) so they do not
    // re-consume the processed-cap and starve re-gearable bots downstream.
    if (!ignore_cooldown && !(owned_lv && refresh_due))
    {
        auto it = gear_backfill_skip_until_.find(id);
        if (it != gear_backfill_skip_until_.end() && it->second > now_ms)
            return;
    }

    // Logged only for a bot that is going to be processed: a parked bot used to print this line every sweep and then return in
    // silence, which read as "the pass ran and did nothing" (Dooda, 2026-10-10).
    TC_LOG_INFO("playerbot.v2",
            "[GearBackfill] candidate {} L{} weaponless={} avg_eff_ilvl={} "
            "(threshold <L{}, dungeon_bar={} hit={}, slot_outlier={} worst_slot={} worst_ilvl={}, empty_slot={}, refresh={}, over_level_mask={:#x})",
            p->GetName(), uint32(lvl), weaponless, avg_eff, uint32(lvl),
            in_instance_run ? dungeon_bar : 0u, dungeon_undergear,
            slot_outlier, uint32(worst_slot),
            worst_eff == std::numeric_limits<uint32>::max() ? 0u : worst_eff,
            empty_slot == 0xFF ? -1 : int32(empty_slot), owned_lv && refresh_due, unusable_mask);

    // Cap counts PROCESSED bots (not just successes): this bounds the
    // world-tick cost of GenerateGearFor (which now does DB2 effective-ilvl
    // lookups per candidate) regardless of how many bots turn out unfixable
    // (sparse pool). At 25/pass every 5 min that is 300 bots/hr — the
    // historical backlog drains overnight without spiking the tick.
    ++regear_count;

    // For altbots we only ever fill an empty weapon slot (legacy-safe).
    const bool armor_ok = !is_altbot;
    // Classic weapon skills are trained; a bot that never met a weapon master would have every weapon pick of a skill
    // it lacks filtered out (Testwar kept a starter sword at 60). A player's own alt keeps its own skills.
    if (!is_altbot)
    {
        ::Playerbot::API api(p);
        api.learn_weapon_skills_free();
        api.learn_riding_free();
        // A companion's spells, talents and repairs: CompanionUpkeep, every pass whatever its gear.
    }

    Gear::GearGenerationContext ctx;
    ctx.level  = lvl;
    ctx.cls    = p->GetClass();
    ctx.race   = p->GetRace();
    ctx.spec   = uint16(::Playerbot::Classic::EffectiveSpecId(p));
    ctx.bot_id = id;
    ctx.wearer = p;
    ctx.role   = bot_role;
    uint32 swapped = 0;
    // Why the slot we actually care about was refused. The parked message
    // used to name three possible causes at once ("bags full / every slot's
    // CanEquipItem refused: proficiency/level/unique"), which is unusable:
    // on the live fleet 87 of 160 re-gear attempts parked and neither we nor
    // the operator could tell which cause fired, or even whether a pick had
    // been generated for the slot at all.
    char const* worst_slot_reason = "no pick generated for this slot";
    uint32 worst_slot_pick = 0;
    for (auto const& g : Gear::GenerateGearFor(ctx))
    {
        bool const is_worst = (slot_outlier && g.slot == worst_slot) || (empty_slot != 0xFF && g.slot == empty_slot);
        if (is_worst) { worst_slot_reason = "reached equip"; worst_slot_pick = g.item_entry; }
        const bool is_weapon = (g.slot == EQUIPMENT_SLOT_MAINHAND ||
                                g.slot == EQUIPMENT_SLOT_OFFHAND ||
                                g.slot == EQUIPMENT_SLOT_RANGED);
        if (!is_weapon && !armor_ok)
        {
            if (is_worst) worst_slot_reason = "altbot: armor slots not touched";
            continue;   // altbot: weapon-only
        }

        Item const* cur = p->GetItemByPos(INVENTORY_SLOT_BAG_0, g.slot);
        // Altbots NEVER have an occupied slot replaced (legacy invariant:
        // owner alts only ever got an EMPTY weapon filled, never a swap).
        if (is_altbot && cur)
        {
            if (is_worst) worst_slot_reason = "altbot: occupied slot never replaced";
            continue;
        }
        // Entry-equality guard: never re-store the item already worn
        // (the generator is deterministic — without this the pass would
        // re-buy the same piece forever -> unbounded bag growth).
        if (cur && cur->GetEntry() == g.item_entry)
        {
            // The generator re-picked what is already worn. For an outlier
            // slot that means the pool genuinely holds nothing better.
            if (is_worst) worst_slot_reason = "generator re-picked the worn item";
            continue;
        }
        ItemTemplate const* gtpl = sObjectMgr->GetItemTemplate(g.item_entry);
        int32 gen_eff = gtpl ? ::Playerbot::Gear::EffectiveItemLevelForLevel(gtpl, lvl) : -1;
        int32 cur_eff = cur ? int32(cur->GetItemLevel(p)) : -1;
        int32 equip_err = -1;
        // Strict effective-ilvl-upgrade guard: only replace an occupied
        // slot when the generated piece actually wears BETTER. Protects
        // already-good gear (incl. owner-curated alt gear) from a
        // downgrade toward the coarse linear target ramp.
        // (Unrenderable worn items never reach these guards: they were
        // already removed by HealUnrenderableGear earlier in this pass.)
        bool const cur_unusable = cur && ((unusable_mask >> g.slot) & 1u);
        if (cur_unusable)
        {
            // Over the bot's level: any pick it can wear replaces it, no comparison with what cannot be used.
        }
        else if (cur && owned_lv)
        {
            // Companion below the cap: the role score decides (blue counts a little more), a clear upgrade only. A two-hander
            // is weighed against the one-hand it replaces plus the offhand that has to go.
            ItemTemplate const* ctpl = cur->GetTemplate();
            int32 cur_score = OwnedGearScore(ctpl, ctx, g.slot);
            if (g.slot == EQUIPMENT_SLOT_MAINHAND && gtpl && gtpl->GetInventoryType() == INVTYPE_2HWEAPON)
                if (Item const* off = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND))
                    cur_score += std::max(0, OwnedGearScore(off->GetTemplate(), ctx, EQUIPMENT_SLOT_OFFHAND));
            if (!gtpl || !Gear::Owned::IsUpgrade(false, cur_score, OwnedGearScore(gtpl, ctx, g.slot)))
            {
                if (is_worst) worst_slot_reason = "owned-bot rule: not a clear upgrade over the worn piece";
                continue;
            }
        }
        else if (cur)
        {
            // Quality guard (operator-reported downgrade): a generated
            // COMMON/white must NEVER replace an equipped UNCOMMON+ (quest
            // green / looted blue). The effective-ilvl guard below is not
            // enough — 12.0 scaling can make a generated white's effective
            // ilvl edge above a low-level green's, swapping out real loot the
            // bot earned and leaving it WORSE. Quality is the stable "real
            // gear" signal; never downgrade across it.
            if (gtpl && cur->GetTemplate() &&
                cur->GetTemplate()->GetQuality() >= gtpl->GetQuality())
            {
                if (is_worst) worst_slot_reason = "quality guard: worn item is same or better quality";
                continue;   // never downgrade across quality (real loot > generated)
            }
            if (gen_eff <= cur_eff)
            {
                if (is_worst) worst_slot_reason = "not a strict effective-ilvl upgrade";
                continue;   // generated piece is not a strict effective-ilvl upgrade
            }
        }
        // A two-hander needs the offhand free: Player::CanEquipItem refuses it (CANT_SWAP / INV_FULL) unless the worn
        // offhand can go to the bags, so a bot with full bags kept its starter mainhand next to an offhand forever
        // (Testwar, 2026-10-05). The generator gives no offhand with a two-hander, so take the worn one off first: to
        // the bags when there is room, otherwise it is destroyed, like any piece a generated upgrade replaces.
        if (g.slot == EQUIPMENT_SLOT_MAINHAND && !is_altbot && gtpl && gtpl->GetInventoryType() == INVTYPE_2HWEAPON &&
            p->CanUseItem(gtpl) == EQUIP_ERR_OK)
        {
            if (Item* off = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND))
            {
                uint16 const off_pos = (uint16(INVENTORY_SLOT_BAG_0) << 8) | EQUIPMENT_SLOT_OFFHAND;
                ItemPosCountVec off_dest;
                if (p->CanUnequipItem(off_pos, /*swap*/ false) == EQUIP_ERR_OK)
                {
                    if (p->CanStoreItem(NULL_BAG, NULL_SLOT, off_dest, off, false) == EQUIP_ERR_OK)
                    {
                        p->RemoveItem(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND, true);
                        p->StoreItem(off_dest, off, true);
                    }
                    else
                        p->DestroyItem(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND, true);
                }
            }
        }
        ItemPosCountVec dest;
        bool staged = (p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, g.item_entry, 1) == EQUIP_ERR_OK);
        if (!staged && !is_altbot && FreeOneJunkBagSlot(p))
        {
            // Bags full — first try the least-destructive route: drop ONE grey
            // vendor-trash item (a player clears junk to equip an upgrade) and
            // retry staging. Only POOR-quality is ever destroyed.
            dest.clear();
            staged = (p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, g.item_entry, 1) == EQUIP_ERR_OK);
        }
        if (!staged)
        {
            // DIRECT-EQUIP fallback when bags are full and there is no grey
            // trash to drop — the live Deadmines blocker (healer/rogue, every
            // bag slot full of looted gear, staging impossible, stuck ilvl48).
            // EquipNewItem places the piece straight into the equip slot and
            // needs NO bag space. For an OCCUPIED slot the quality + effective-
            // ilvl guards above already proved the worn piece (`cur`) strictly
            // worse, so destroy it in place first (exactly what a player does
            // swapping an upgrade with a full pack); for an empty slot just
            // equip. Validate equippability (proficiency/level/unique) BEFORE
            // destroying anything so a refusal never leaves the slot naked.
            if (!is_altbot)
            {
                uint16 eq_dest_direct;
                InventoryResult dmsg =
                    p->CanEquipNewItem(g.slot, eq_dest_direct, g.item_entry, /*swap*/ cur != nullptr);
                if (dmsg == EQUIP_ERR_OK)
                {
                    if (cur)
                        p->DestroyItem(INVENTORY_SLOT_BAG_0, g.slot, true);
                    if (p->EquipNewItem(eq_dest_direct, g.item_entry, ItemContext::NONE, true))
                    {
                        p->AutoUnequipOffhandIfNeed();
                        ++swapped;
                        continue;
                    }
                    if (is_worst) worst_slot_reason = "bags full, direct EquipNewItem returned null";
                }
                else if (is_worst)
                {
                    static thread_local std::string s_reason;
                    s_reason = "bags full, direct-equip refused: CanEquipNewItem="
                             + std::to_string(uint32(dmsg));
                    worst_slot_reason = s_reason.c_str();
                }
            }
            continue;   // bags full and direct-equip not possible — skip slot
        }
        Item* it = p->StoreNewItem(dest, g.item_entry, true);
        if (!it) continue;
        uint16 eq_dest;
        equip_err = int32(p->CanEquipItem(g.slot, eq_dest, it, /*swap*/ true));
        if (equip_err == EQUIP_ERR_OK)
        {
            // SwapItem does the proper unequip + re-store-displaced dance
            // (the destructive EquipItem path is why generated gear used
            // to vanish). Displaced item lands back in the bag.
            const uint16 src = (uint16(it->GetBagSlot()) << 8) | it->GetSlot();
            const uint16 dst = (uint16(INVENTORY_SLOT_BAG_0) << 8) | g.slot;
            p->SwapItem(src, dst);
            p->AutoUnequipOffhandIfNeed();
            ++swapped;
        }
        else if (is_worst)
        {
            static thread_local std::string s_reason;
            s_reason = "staged to bags but CanEquipItem refused: err="
                     + std::to_string(uint32(equip_err));
            worst_slot_reason = s_reason.c_str();
        }
        // CanEquipItem refused (proficiency/level/unique): leave the one
        // stored item in bags for State_Idle auto_equip's throttled
        // retry. Do NOT loop or re-store.
    }
    if (swapped > 0)
    {
        gear_backfill_skip_until_.erase(id);   // made progress — keep eligible
        if (companion)
            gear_refresh_key_[id] = refresh_key;
        TC_LOG_INFO("playerbot.v2",
            "[GearBackfill] re-geared under-geared bot {} (class {} L{}): {} slot(s), "
            "avg_eff_ilvl {} (was below level){}",
            p->GetName(), uint32(p->GetClass()), uint32(lvl), swapped,
            avg_eff, is_altbot ? " [altbot weapon-only]" : "");
    }
    else if (owned_lv && refresh_due)
    {
        // The level-up / instance refresh found nothing better than what is worn: that is the answer, not a failure.
        gear_refresh_key_[id] = refresh_key;
        TC_LOG_INFO("playerbot.v2", "[GearBackfill] {} L{} refresh: already wearing the best it can (avg_eff_ilvl {})",
                    p->GetName(), uint32(lvl), avg_eff);
    }
    else
    {
        // Under-geared but nothing could be re-geared this pass (bags full /
        // every slot's CanEquipItem refused). Park 30 min so it stops
        // hogging the processed-cap; its normal vendor/equip behavior may
        // free bags or equip the bagged items before the next attempt.
        TC_LOG_INFO("playerbot.v2",
            "[GearBackfill] {} L{} under-geared but 0 swaps this pass - parked 30min"
            "{}",
            p->GetName(), uint32(lvl),
            (slot_outlier || empty_slot != 0xFF)
                ? Trinity::StringFormat(" | {} slot {} (worn ilvl {}): pick {} -> {}",
                      slot_outlier ? "worst" : "empty",
                      uint32(slot_outlier ? worst_slot : empty_slot),
                      slot_outlier && worst_eff != std::numeric_limits<uint32>::max() ? worst_eff : 0u,
                      worst_slot_pick, worst_slot_reason)
                : std::string());
        gear_backfill_skip_until_[id] = now_ms + 30u * 60u * 1000u;
    }
}

void BotPopulationManager::forget_pipeline_caches(uint64 guid)
{
    pipeline_row_cache_.erase(guid);
    setup_done_cache_.erase(guid);
    // Also drops the in-flight marker, which doubles as the cancel signal: a
    // pipeline-row fetch that is still outstanding when the character is
    // deleted finds its marker gone and throws the row away instead of
    // re-populating the cache for a guid that no longer exists.
    pipeline_row_fetching_.erase(guid);
}

void BotPopulationManager::RunHygiene(uint32 now_ms)
{
    if (last_hygiene_ms_ && (now_ms - last_hygiene_ms_) < kHygieneIntervalMs)
        return;
    last_hygiene_ms_ = now_ms;

    // 0) Under-gear re-gear moved to RunGearBackfill() (own 5-min cadence, driven
    //    from OnWorldTick) so it runs SOON after the fleet logs in and drains the
    //    historical backlog fast — the 1h hygiene pass fires once at boot before
    //    bots are online, which would waste it.

    // 0b) Failed-JIT corpse sweep — extracted to SweepFailedJitCorpses()
    //     and ALSO driven on a 10-min cadence from OnWorldTick so the
    //     multi-thousand backlog drains in hours, not days.
    SweepFailedJitCorpses();

    // 1) Delete JIT bots not seen in N days — extracted to SweepStaleJitBots()
    //    because its SELECT is now async.
    SweepStaleJitBots();

    // 2) Hard cap: see below. Runs independently of the stale-JIT delete above,
    //    which lands on a later tick.
    HygieneHardCap();
}

void BotPopulationManager::SweepStaleJitBots()
{
    //    Delete JIT bots not seen in N days. Pull (guid, account) so we
    //    can also notify BotAccountMgr to free the pool slot — without
    //    that, the pool count drifts and new spawns hit "account full"
    //    sooner than the hard cap should allow. `c.name` comes along in the
    //    JOIN: it used to be fetched with a separate synchronous
    //    `SELECT name FROM characters WHERE guid=?` PER ROW, so one hygiene
    //    pass could block the world thread on 51 round-trips. It is the same
    //    row the JOIN already visited, so the value is identical.
    //
    //    hygiene_delete_pending_ guards against a second pass starting while
    //    this one is in flight: the rows have not been deleted yet, so an
    //    overlapping pass would re-run LogoutBot / BotNamePool::Release /
    //    note_character_removed for the same 50 characters.
    if (hygiene_delete_pending_)
        return;
    hygiene_delete_pending_ = true;
    queries_.AddCallback(CharacterDatabase.AsyncQuery(Trinity::StringFormat(
        "SELECT pv.character_guid_low, c.account, c.name FROM playerbot_v2_character pv "
        "JOIN characters c ON c.guid = pv.character_guid_low "
        "WHERE pv.jit_for_queue IS NOT NULL "
        "AND pv.last_seen_at IS NOT NULL "
        "AND pv.last_seen_at < (NOW() - INTERVAL {} DAY) LIMIT 50",
        uint32(kJitMaxAgeDays)).c_str())
        .WithCallback([this](QueryResult del_res)
    {
        hygiene_delete_pending_ = false;
        if (!del_res)
            return;
        uint32 deleted = 0;
        do {
            Field* f = del_res->Fetch();
            uint64 guid    = f[0].GetUInt64();
            uint32 account = f[1].GetUInt32();
            // Logout if online (may already be offline).
            Services::SessionMgr().LogoutBot(ObjectGuid::Create<HighGuid::Player>(guid));
            // Release the bot's name back to the pool BEFORE deleting the
            // characters row.
            BotNamePool::Release(f[2].GetString());
            // Drop V2 row + character row + cache entry.
            CharacterDatabase.PExecute(
                "DELETE FROM playerbot_v2_character WHERE character_guid_low={}", guid);
            CharacterDatabase.PExecute(
                "DELETE FROM characters WHERE guid={}", guid);
            // The rows are gone, so the in-memory state mirroring them has to go
            // too. The corpse sweep above already does this; hygiene did not, so
            // the Lifecycle registry kept reporting a deleted guid as a bot and
            // both pipeline caches kept a row for a character that no longer
            // exists. Every other reference to these two containers is an insert
            // or a lookup - this is their only erase - so anything left here
            // drifts upward for the lifetime of the process.
            Services::Lifecycle().unmark_as_bot(guid);
            forget_pipeline_caches(guid);
            // Free the pool-account slot so BotAccountMgr knows there's
            // capacity to spawn a fresh bot on this account next time.
            Services::Accounts().note_character_removed(account);
            ++deleted;
        } while (del_res->NextRow());
        if (deleted)
            TC_LOG_INFO("playerbot.v2", "[Population] hygiene: deleted {} stale JIT bots (>{}d unused)",
                        deleted, kJitMaxAgeDays);
    }));
}

void BotPopulationManager::HygieneHardCap()
{
    //    Hard cap: if total online bots exceed target * multiplier, mass-logout
    //    excess to prevent runaway. No DB access — entirely live-registry work.
    //
    // LP-P1b: this is NOT last-activity ordered — the live registry has no
    // per-bot last-activity timestamp, and for_each visits entries in hash
    // order, not recency order. The earlier "oldest LRU first" wording was
    // misleading; we kick the first `to_kick` ELIGIBLE bots the registry
    // yields. (A true LRU would require either a last-activity field on
    // BotRegistryEntry or a DB sort on last_seen_at; neither exists on this
    // path today. // LP-P1b: needs a last-activity signal on BotRegistryEntry
    // for genuine LRU ordering.)
    //
    // Eligibility mirrors LogoutLRUMany's guard set (skip grouped + in
    // dungeon) and ADDS raid + battleground/arena guards: kicking a bot that
    // is grouped, in a dungeon/raid, or in a BG/arena would yank it out of
    // active content and break its group's run. Without these guards the
    // hygiene kick logged out bots mid-instance.
    auto& cfg = Services::Config();
    uint32 target = cfg.population_total_target();
    if (!target) return;
    uint32 hard_cap = uint32(float(target) * kHardCapMultiplier);
    // Pass I: count + kick from live registry instead of lifecycle.
    // Live registry size IS the online count — no per-id
    // FindConnectedPlayer needed for the tally. At 11K marked vs 3K
    // in-world this saved ~8K hashmap lookups per hygiene cycle.
    // Altbots, BG/LFG-queued bots, and kick-protected JIT fills are
    // excluded from the tally (deliberate overshoot above TotalTarget;
    // see Reconcile) and from the kick loop below.
    const uint32 hyg_tally_now = getMSTime();
    uint32 online = 0;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player const* p = ObjectAccessor::FindConnectedPlayer(g);
        if (p && p->InBattlegroundQueue()) return;
        if (p && sLFGMgr->GetState(p->GetGUID()) != lfg::LFG_STATE_NONE) return;
        if (IsKickProtected(id, hyg_tally_now)) return;
        if (IsPlayerOwnedAlt(id, p)) return;
        if (IsOperatorPersistentBot(uint64(id))) return;  // dev/test bot — off the shaper's books
        ++online;
    });
    if (online > hard_cap)
    {
        uint32 to_kick = online - hard_cap;
        TC_LOG_WARN("playerbot.v2", "[Population] hygiene: hard-cap exceeded ({}>{}); kicking up to {} eligible",
                    online, hard_cap, to_kick);
        std::vector<ObjectGuid> to_logout;
        to_logout.reserve(to_kick);
        Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
        {
            if (to_logout.size() >= to_kick) return;
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            // Resolve the Player and SKIP candidates engaged in content —
            // mirrors LogoutLRUMany's guards (group + dungeon) and adds the
            // raid + BG/arena guards. See the LP-P1b note above.
            Player* p = ObjectAccessor::FindConnectedPlayer(g);
            if (!p) return;
            if (p->GetGroup()) return;
            // Spare BG- and LFG-queued bots (JIT match-fill overshoot) —
            // same rationale as the Reconcile overflow kick.
            if (p->InBattlegroundQueue()) return;
            if (sLFGMgr->GetState(p->GetGUID()) != lfg::LFG_STATE_NONE) return;
            if (IsKickProtected(id, getMSTime())) return;
            // Altbots are a real player's squad members, not population-
            // shaper inventory — never kick them.
            if (IsPlayerOwnedAlt(id, p)) return;
            if (IsOperatorPersistentBot(uint64(id))) return;  // dev/test bot — never rotates out
            if (Map* m = p->GetMap())
            {
                if (m->IsDungeon()) return;            // covers dungeon + raid (IsRaid implies IsDungeon)
                if (m->IsBattlegroundOrArena()) return;
            }
            to_logout.push_back(g);
        });
        for (ObjectGuid g : to_logout)
            Services::SessionMgr().LogoutBot(g);
    }
}

void BotPopulationManager::ForceReconcile()
{
    // A cycle already waiting on its offline-candidate rows: leave it alone and
    // let it finish rather than resetting the budgets underneath it (the budgets
    // it already spent in phase 1 would be handed back, double-spending them in
    // phase 2).
    if (reconcile_in_progress_)
    {
        TC_LOG_INFO("playerbot.v2",
            "[Population] force reconcile ignored: a cycle is still waiting on its "
            "offline-candidate rows (it completes within a tick or two)");
        return;
    }
    // Split each cycle's budget per faction based on HordePct. Without
    // this, Reconcile's bucket loop drained the entire pool into Alliance
    // before Horde got serviced (8.3× imbalance observed 2026-05-15).
    // Round-half-up keeps the totals at kSpawnRate/kLoginRate when HordePct
    // is non-integer-friendly (e.g. 50% → 12+13).
    {
        const uint8 horde_pct = Services::Config().population_horde_pct();
        spawn_budget_horde_ = (kSpawnRatePerTick * horde_pct + 50u) / 100u;
        spawn_budget_alli_  = kSpawnRatePerTick - spawn_budget_horde_;
        login_budget_horde_ = (kLoginRatePerTick * horde_pct + 50u) / 100u;
        login_budget_alli_  = kLoginRatePerTick - login_budget_horde_;
    }
    // Synchronous: `.playerbot wipefleet` calls this ten times in a row to
    // burst-spawn, and each call has to have finished its fill before the next
    // one resets the budgets. One-off operator command — blocking is fine here,
    // and it keeps the burst behaviour exactly as it was.
    ReconcileBegin(/*synchronous=*/true);
}

std::vector<PopulationBucket> BotPopulationManager::ComputeTargets() const
{
    auto& cfg = Services::Config();
    uint32 total = cfg.population_total_target();
    if (!total) return {};

    PopulationShape shape = ParseShape(cfg.population_shape());
    float max_heavy_factor = cfg.population_max_heavy_factor();
    uint8 horde_pct = cfg.population_horde_pct();

    auto const level_buckets = LevelBuckets();
    size_t const N = level_buckets.size();
    auto weights = ShapeWeights(shape, N, max_heavy_factor);

    // B-2: largest-remainder apportionment. The previous per-bucket
    // round-half-up (`weights[i] * total + 0.5`) let the bucket targets
    // sum ABOVE TotalTarget (up to +N/2 bots across N buckets) — and the
    // fleet then held that overshoot forever because every bucket
    // individually sat at its (inflated) target. Floor every bucket,
    // then hand the remaining units to the buckets with the largest
    // fractional parts; targets now sum to exactly `total`.
    std::vector<uint32> bucket_totals(N, 0);
    std::vector<std::pair<float, size_t>> fractions;
    fractions.reserve(N);
    uint32 assigned = 0;
    for (size_t i = 0; i < N; ++i)
    {
        float raw = weights[i] * float(total);
        bucket_totals[i] = uint32(raw);
        assigned += bucket_totals[i];
        fractions.emplace_back(raw - float(bucket_totals[i]), i);
    }
    std::sort(fractions.begin(), fractions.end(),
              [](auto const& a, auto const& b) { return a.first > b.first; });
    for (size_t k = 0; assigned < total && k < fractions.size(); ++k, ++assigned)
        ++bucket_totals[fractions[k].second];

    std::vector<PopulationBucket> out;
    out.reserve(N);
    for (size_t i = 0; i < N; ++i)
    {
        PopulationBucket b{};
        b.level_lo = level_buckets[i][0];
        b.level_hi = level_buckets[i][1];
        b.horde_target    = bucket_totals[i] * horde_pct / 100u;
        b.alliance_target = bucket_totals[i] - b.horde_target;
        out.push_back(b);
    }
    return out;
}

void BotPopulationManager::PopulateActual(std::vector<PopulationBucket>& buckets) const
{
    // Pass I: walk live registry (in-world bots), not lifecycle (all
    // marked, ~11K historical). The lifecycle entries that aren't
    // in-world contribute nothing to bucket counts anyway — they'd be
    // filtered by FindConnectedPlayer == nullptr. Saves N-live vs
    // N-marked iteration cost (10K+ at scale).
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* p = ObjectAccessor::FindConnectedPlayer(g);
        if (!p) return;
        // Altbots are a player's squad, not population-shaper inventory.
        // Counting them here made reconcile_side see the bucket as over-
        // target and log out POOL bots to compensate — summoning 5 alts
        // silently evicted 5 ambient-world bots. Alts ride on top of
        // TotalTarget.
        if (IsPlayerOwnedAlt(id, p)) return;
        uint8 lvl = p->GetLevel();
        bool alliance = p->GetTeam() == ALLIANCE;
        for (auto& b : buckets)
        {
            if (lvl < b.level_lo || lvl > b.level_hi) continue;
            if (alliance) ++b.alliance_actual;
            else          ++b.horde_actual;
            break;
        }
    });
}

PopulationSnapshot BotPopulationManager::Snapshot() const
{
    PopulationSnapshot snap;
    snap.buckets = ComputeTargets();
    PopulateActual(snap.buckets);
    for (auto const& b : snap.buckets)
    {
        snap.total_target += b.alliance_target + b.horde_target;
        snap.total_actual += b.alliance_actual + b.horde_actual;
    }
    return snap;
}

void BotPopulationManager::QueueOfflineCandidateFetch(size_t fill_index, uint8 lo, uint8 hi, uint32 cap)
{
    // Query characters table for V2-marked bots with level ∈ [lo, hi].
    // We use playerbot_v2_character JOINed against characters for level / race.
    //
    // This used to be a synchronous PQuery, run TWICE per bucket (once per
    // faction) from inside Reconcile's bucket loop — a 20-bucket shape meant 40
    // blocking JOINs on the world thread every cycle, and that is the bulk of
    // the 'pop' phase's reconcile time. It is now one async query per bucket;
    // the rows are faction-blind (they always were — the query has no race
    // filter), so a single fetch serves both sides and FilterOfflineCandidates
    // applies the per-side faction + cap afterwards.
    //
    // `cap` is the larger of the two sides' caps, so the 2x row limit is at
    // least as wide as either per-side query would have asked for;
    // FilterOfflineCandidates re-imposes the narrower side's 2x prefix, which
    // makes the selected set identical to what the old per-side query returned.
    //
    // No pending flag of its own: ReconcileBegin is the only caller and it
    // refuses to start while reconcile_in_progress_, so a bucket can never have
    // two of these outstanding.
    ++reconcile_inflight_;
    queries_.AddCallback(CharacterDatabase.AsyncQuery(Trinity::StringFormat(
        "SELECT pv.character_guid_low, c.level, c.race FROM playerbot_v2_character pv "
        "JOIN characters c ON c.guid = pv.character_guid_low "
        "WHERE c.level BETWEEN {} AND {} "
        "AND c.online = 0 "
        "AND pv.distribution_level > 0 "
        "ORDER BY pv.last_seen_at ASC LIMIT {}",
        uint32(lo), uint32(hi), cap * 2).c_str())
        .WithCallback([this, fill_index, gen = reconcile_gen_](QueryResult res)
    {
        // Rows from a cycle the watchdog abandoned: drop them rather than file
        // them against whatever cycle is running now.
        if (gen != reconcile_gen_)
            return;
        if (fill_index < reconcile_fills_.size())
        {
            PendingBucketFill& fill = reconcile_fills_[fill_index];
            fill.ready = true;
            if (res)
            {
                do {
                    Field* f = res->Fetch();
                    OfflineCandidate c{};
                    c.char_guid = f[0].GetUInt64();
                    c.level     = f[1].GetUInt8();
                    c.alliance  = RaceIsAlliance(f[2].GetUInt8());
                    fill.rows.push_back(c);
                } while (res->NextRow());
            }
        }
        if (reconcile_inflight_)
            --reconcile_inflight_;
    }));
}

std::vector<BotPopulationManager::OfflineCandidate>
BotPopulationManager::FetchOfflineCandidatesSync(uint8 lo, uint8 hi, uint32 cap)
{
    std::vector<OfflineCandidate> rows;
    SLOW_CALL("FetchOfflineCandidatesSync (database)");
    auto res = CharacterDatabase.PQuery(
        "SELECT pv.character_guid_low, c.level, c.race FROM playerbot_v2_character pv "
        "JOIN characters c ON c.guid = pv.character_guid_low "
        "WHERE c.level BETWEEN {} AND {} "
        "AND c.online = 0 "
        "AND pv.distribution_level > 0 "
        "ORDER BY pv.last_seen_at ASC LIMIT {}",
        uint32(lo), uint32(hi), cap * 2);
    if (!res) return rows;
    do {
        Field* f = res->Fetch();
        OfflineCandidate c{};
        c.char_guid = f[0].GetUInt64();
        c.level     = f[1].GetUInt8();
        c.alliance  = RaceIsAlliance(f[2].GetUInt8());
        rows.push_back(c);
    } while (res->NextRow());
    return rows;
}

std::vector<BotPopulationManager::OfflineCandidate>
BotPopulationManager::FilterOfflineCandidates(std::vector<OfflineCandidate> const& rows,
                                              bool alliance, uint32 cap)
{
    // Exactly the client-side filter the synchronous version did, including the
    // 2*cap row window: the old query's `LIMIT cap*2` is a PREFIX of `rows`
    // (same ORDER BY, wider limit), so walking only the first 2*cap entries
    // reproduces its result row for row.
    std::vector<OfflineCandidate> out;
    if (!cap) return out;
    size_t const window = std::min<size_t>(rows.size(), size_t(cap) * 2);
    for (size_t i = 0; i < window; ++i)
    {
        OfflineCandidate const& c = rows[i];
        if (c.alliance != alliance) continue;
        out.push_back(c);
        if (out.size() >= cap) break;
    }
    return out;
}

bool BotPopulationManager::LoginExisting(uint64 char_guid_low, bool alliance)
{
    uint32& budget = alliance ? login_budget_alli_ : login_budget_horde_;
    if (!budget) return false;
    ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(char_guid_low);
    auto r = Services::SessionMgr().LoginBot(g);
    if (r.ok)
    {
        --budget;
        TC_LOG_DEBUG("playerbot.v2", "[Population] login existing {} ok", g.ToString());
        return true;
    }
    TC_LOG_DEBUG("playerbot.v2", "[Population] login existing {} failed: {}", g.ToString(), r.reason);
    return false;
}

uint64 BotPopulationManager::SpawnNew(uint8 level, bool alliance, uint8 race_hint)
{
    uint32& budget = alliance ? spawn_budget_alli_ : spawn_budget_horde_;
    if (!budget) return 0;

    // Pass J: spawn cap by marked-bot count. Once the DB holds ≥1.10×
    // target characters, we already have a pool large enough to satisfy
    // any in-world target via LoginExisting — spawning more just inflates
    // the lifecycle registry forever (each new character = permanent DB
    // row + permanent setup-pipeline work + permanent reload-on-boot).
    // 10% overhead per the user's brief: leaves room for distribution
    // shape changes (HordePct, MaxHeavyFactor) without immediate spawning.
    // When the cap trips, the cycle still runs LoginExisting which brings
    // offline chars back in-world; spawn only resumes after target grows
    // or characters are explicitly deleted.
    auto& cfg = Services::Config();
    const uint32 total_target = cfg.population_total_target();
    if (total_target > 0)
    {
        // Owner-bound companions (`.bot add`) are marked bots too, but they
        // ride on top of TotalTarget (see PopulateActual) and never return
        // through LoginExisting (distribution_level 0): counting them here
        // stopped a small ambient world short of its target (10 companions
        // + TotalTarget 20 -> cap 22 -> only 12 world bots ever spawned).
        const uint32 spawn_cap = uint32(float(total_target) * 1.10f);
        const uint32 marked    = pool_marked_count_;
        if (marked >= spawn_cap)
        {
            // Quiet log: spam once per cycle would dominate the [Population]
            // log line below. Just no-op.
            return 0;
        }
    }

    // LP-P1a: roll the (race, class, gender) tuple from BotComposition's
    // WEIGHTED census tables, constrained only by faction. The previous
    // version pre-selected a race via `budget % races.size()` and passed it
    // as a race_hint — that forced a near-uniform race distribution (each
    // faction's 14 races picked round-robin off the budget counter),
    // completely bypassing the per-race census weights in
    // BotComposition.cpp. Result: the world had ~equal numbers of every
    // race instead of the Blizzlike skew (e.g. far more Humans/Blood Elves
    // than Mechagnomes/Vulpera).
    //
    // BotComposition::Roll has no faction parameter — when called with no
    // hints it rolls faction 50/50 internally then applies PickRace(faction)
    // (the weighted table). So we constrain faction the only way the public
    // API allows: re-roll the weighted pick and reject any whose race is on
    // the wrong faction, using the existing RaceIsAlliance() helper. This
    // preserves the intra-faction census weighting (unlike a race_hint) while
    // still guaranteeing a faction-correct character. Each Roll already
    // succeeds ~50% on faction match, so the expected attempt count is ~2;
    // the cap of 8 makes the worst case bounded without a hard failure mode.
    BotComposition::Pick pick{};
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        // A fixed race (Skyborne residents of Zephras Isle) rolls its class around it and needs no faction retry.
        BotComposition::Pick candidate = BotComposition::Roll(race_hint);
        if (!candidate.race)
            continue;
        if (RaceIsAlliance(candidate.race) == alliance)
        {
            pick = candidate;
            break;
        }
    }
    // A failed creation is counted and explained at INFO once a minute: the world spawner's spawn_failed counter alone says
    // nothing about which step fails (the per-failure DEBUG lines are off in a normal run).
    static uint32 s_fail_n = 0, s_fail_last_ms = 0;
    auto note_fail = [&](std::string const& why)
    {
        ++s_fail_n;
        uint32 const now = getMSTime();
        if (!s_fail_last_ms || getMSTimeDiff(s_fail_last_ms, now) >= 60000u)
        {
            TC_LOG_INFO("playerbot.v2", "[Population] spawn failed {} time(s) since the last line; latest ({} L{}): {}",
                        s_fail_n, alliance ? "Alliance" : "Horde", uint32(level), why);
            s_fail_n = 0;
            s_fail_last_ms = now ? now : 1u;
        }
    };
    if (!pick.race)
    {
        note_fail("no race of the wanted faction rolled in 8 tries (or Roll gave up)");
        return 0;
    }

    auto created = BotCharacterFactory::Create(/*ownerSession=*/nullptr,
        pick.name, pick.race, pick.cls, pick.gender);
    if (!created.ok)
    {
        note_fail(created.reason);
        return 0;
    }

    --budget;

    uint64 guid_low = created.guid.GetCounter();

    // Mark distribution_level so the setup pipeline knows what to set.
    CharacterDatabase.PExecute(
        "UPDATE playerbot_v2_character SET distribution_level={} WHERE character_guid_low={}",
        uint32(level), guid_low);

    // Defer LoginBot one world tick. SaveToDB(true) inside Create commits
    // asynchronously - calling LoginBot now would race the async worker
    // and the LoginQueryHolder would see "not found in table characters".
    // OnWorldTick drains deferred_logins_ at the start of every tick; by
    // then the INSERT has landed and LoadFromDB succeeds.
    deferred_logins_.push_back(guid_low);
    ++pool_marked_count_;
    return guid_low;
}

uint32 BotPopulationManager::LogoutLRUMany(uint8 lo, uint8 hi, bool alliance, uint32 count)
{
    // Pass G + I: batch logout, scanning live registry only. Originally
    // LogoutLRU walked the full lifecycle (~10K-11K marked) and returned
    // the FIRST match — Reconcile called it N times for N excess bots,
    // doing N × 10K = 2M iterations per cycle. Pass G batched the scan
    // to find up to `count` victims in ONE pass; Pass I switched the
    // scan target from lifecycle to live registry (in-world only), so
    // we iterate the actual logout-eligible pool (200-3000 typical)
    // instead of every bot ever spawned.
    if (count == 0) return 0;
    std::vector<ObjectGuid> victims;
    victims.reserve(count);
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        if (victims.size() >= count) return;
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* p = ObjectAccessor::FindConnectedPlayer(g);
        if (!p) return;
        if (p->GetLevel() < lo || p->GetLevel() > hi) return;
        if ((p->GetTeam() == ALLIANCE) != alliance) return;
        if (p->GetGroup()) return;
        if (p->GetMap() && p->GetMap()->IsDungeon()) return;
        if (p->GetMap() && p->GetMap()->IsBattlegroundOrArena()) return;
        // Spare BG/LFG-queued bots (JIT match-fill overshoot) — same
        // rationale as the Reconcile overflow kick and hygiene pass.
        if (p->InBattlegroundQueue()) return;
        if (sLFGMgr->GetState(p->GetGUID()) != lfg::LFG_STATE_NONE) return;
        if (IsKickProtected(id, getMSTime())) return;
        // Spare altbots: a real player's alt is under that player's
        // control, not the population shaper's — kicking it yanks a
        // character out of the owner's squad. Owners log them out.
        if (IsPlayerOwnedAlt(id, p)) return;
        if (IsOperatorPersistentBot(uint64(id))) return;  // dev/test bot — never rotates out
        victims.push_back(g);
    });
    for (ObjectGuid v : victims)
        Services::SessionMgr().LogoutBot(v);
    return uint32(victims.size());
}

bool BotPopulationManager::LogoutLRU(uint8 lo, uint8 hi, bool alliance)
{
    // Single-victim wrapper kept for any direct callers; new code should
    // prefer LogoutLRUMany for overshoot loops.
    return LogoutLRUMany(lo, hi, alliance, 1) > 0;
}

void BotPopulationManager::SessionRhythmLogout(uint32 now_ms)
{
    ConfigReader const& cfg = Services::Config();
    if (!cfg.session_rhythm_enabled()) return;
    const float mult = cfg.session_rhythm_multiplier();
    if (mult <= 0.0f) return;                 // <=0 disables the layer
    const uint32 cap = cfg.session_rhythm_max_logouts_per_cycle();
    if (cap == 0) return;

    // 24h ceiling so an extreme archetype*multiplier can't overflow the uint32
    // ms budget (uint16 minutes * 60000 * mult). Real sessions are << 24h.
    constexpr double kBudgetCeilMs = 86'400'000.0;

    auto& reg = Services::Registry();
    std::vector<ObjectGuid> expired;
    expired.reserve(cap);
    std::unordered_set<uint64> seen;          // online set, for stale-entry prune
    seen.reserve(session_start_ms_.size() + 64);

    reg.for_each([&](BotId id, BotRegistryEntry const&)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* p = ObjectAccessor::FindConnectedPlayer(g);
        if (!p) return;
        seen.insert(uint64(id));

        // Eligibility — never yank a bot out of an active social/content
        // commitment; a human finishes the dungeon/BG/group before logging
        // off. Mirrors the LogoutLRUMany guard set exactly.
        if (p->GetGroup()) return;
        if (p->GetMap() && (p->GetMap()->IsDungeon() || p->GetMap()->IsBattlegroundOrArena())) return;
        if (p->InBattlegroundQueue()) return;
        if (sLFGMgr->GetState(p->GetGUID()) != lfg::LFG_STATE_NONE) return;
        if (IsKickProtected(id, now_ms)) return;
        if (IsPlayerOwnedAlt(id, p)) return;  // owner's alt — not ours to log off
        if (IsOperatorPersistentBot(uint64(id))) return;  // dev/test bot — never rotates out

        // Per-bot session budget from the (immutable-after-login) archetype.
        BotAI* ai = reg.ai(id);
        const uint16 base_min = ai ? ai->archetype().target_session_minutes : 90;

        auto it = session_start_ms_.find(uint64(id));
        if (it == session_start_ms_.end())
        {
            // First sighting: stagger the synthetic session start UNIFORMLY
            // back across one session window so the already-online boot cohort
            // expires spread out, not all on the first cycle after this layer
            // goes live.
            const uint32 budget_ms = uint32(std::min(
                double(base_min) * 60000.0 * double(mult), kBudgetCeilMs));
            const uint32 stagger = budget_ms ? (MixHash64to32(uint64(id)) % budget_ms) : 0;
            session_start_ms_[uint64(id)] = now_ms - stagger;
            return;
        }

        if (expired.size() >= cap) return;    // budget spent; leftovers next cycle
        const uint32 login_ms = it->second;
        // Deterministic +/-10% jitter per (bot, session) so a cohort that all
        // logged in together still expires staggered. Salt with the login
        // stamp so the SAME bot draws fresh jitter on its next session.
        const uint32 jitter_pct = 90u + (MixHash64to32(uint64(id) ^ uint64(login_ms)) % 21u);
        const uint32 budget_ms = uint32(std::min(
            double(base_min) * 60000.0 * double(mult) * double(jitter_pct) / 100.0, kBudgetCeilMs));
        if (budget_ms == 0) return;
        if (uint32(now_ms - login_ms) >= budget_ms)
        {
            expired.push_back(g);
            session_start_ms_.erase(it);      // fresh budget on next login
        }
    });

    for (ObjectGuid g : expired)
        Services::SessionMgr().LogoutBot(g);

    // Opportunistic prune: drop stamps for bots no longer online (logged out
    // by overflow/hygiene/owner) so the map can't grow past the live roster.
    if (session_start_ms_.size() > seen.size() + 256)
        for (auto i = session_start_ms_.begin(); i != session_start_ms_.end();)
            i = seen.count(i->first) ? std::next(i) : session_start_ms_.erase(i);

    if (!expired.empty())
        TC_LOG_INFO("playerbot.v2",
            "[Population] session-rhythm: {} bots ended their play session "
            "(cap={}, mult={:.2f}) — roster will rotate as reconcile refills",
            uint32(expired.size()), cap, mult);
}

void BotPopulationManager::ProtectFromKick(uint64 guid_low, uint32 duration_ms)
{
    const uint32 now = getMSTime();
    kick_protect_until_ms_[guid_low] = now + duration_ms;
    // Opportunistic expiry sweep — the map only ever holds the last few
    // JIT batches, but without pruning it grows for the process lifetime.
    if (kick_protect_until_ms_.size() > 256)
        for (auto it = kick_protect_until_ms_.begin(); it != kick_protect_until_ms_.end();)
            it = (int32(it->second - now) < 0) ? kick_protect_until_ms_.erase(it) : std::next(it);
}

bool BotPopulationManager::IsKickProtected(uint64 guid_low, uint32 now_ms) const
{
    auto it = kick_protect_until_ms_.find(guid_low);
    return it != kick_protect_until_ms_.end() && int32(it->second - now_ms) > 0;
}

bool BotPopulationManager::IsAmbientBot(BotId id, Player const* p) const
{
    auto it = pipeline_row_cache_.find(uint64(id));
    // A battleground JIT bot (jit tag still set) belongs to the queue filler, not to the world spawner.
    return it != pipeline_row_cache_.end() && it->second.dist_level > 0 && it->second.jit_tag.empty()
        && !IsPlayerOwnedAlt(id, p) && !(p && IsSoakAnchor(p->GetName()));   // a soak anchor stands in for a player, it is not a resident
}

bool BotPopulationManager::AmbientFleetEnabled()
{
    return Services::Config().population_total_target() > 0 || sConfigMgr->GetBoolDefault("Playerbot.World.Enable", false, true);
}

std::string BotPopulationManager::WorldReport() const { return world_spawner_->Report(); }
WorldLoadFacts BotPopulationManager::WorldLoad() const { return world_spawner_ ? world_spawner_->Load() : WorldLoadFacts{}; }
bool BotPopulationManager::WorldSpawnerPending(uint64 guid_low) const { return world_spawner_->IsPending(guid_low); }

bool BotPopulationManager::IsWorldPlaced(BotId id) const { return world_spawner_ && world_spawner_->IsPlaced(uint64(id)); }

uint64 BotPopulationManager::SpawnAmbientBot(uint8 level, bool alliance, uint8 race)
{
    // SpawnNew spends a per-cycle budget that only the reconcile refills; give it exactly one unit and restore the
    // counter afterwards, so the world spawner never eats (or leaks into) the shaper's own budget.
    uint32& budget = alliance ? spawn_budget_alli_ : spawn_budget_horde_;
    uint32 const saved = budget;
    budget = 1;
    uint64 const guid = SpawnNew(level, alliance, race);
    budget = saved;
    return guid;
}

bool BotPopulationManager::GraduateSkyborne(Player* bot)
{
    return pipeline_ && pipeline_->Graduate(bot);
}

bool BotPopulationManager::IsOperatorPersistentBot(uint64 guid_low) const
{
    // See the declaration in the header for the dist_level == 0 invariant.
    // Same world thread as DriveSetupPipelines (the cache's writer), so no
    // lock is needed.
    auto it = pipeline_row_cache_.find(guid_low);
    return it != pipeline_row_cache_.end() && it->second.dist_level == 0;
}

void BotPopulationManager::ReconcileBegin(bool synchronous)
{
    // Pending flag for the whole cycle: while the offline-candidate queries of
    // the previous cycle are still in flight, a new cycle must not start — it
    // would issue the same queries again and then fill twice off two row sets.
    if (reconcile_in_progress_)
        return;

    const uint32 rec_start = getMSTime();
    // #5 session-rhythm: expire bots whose play session is over FIRST, so the
    // per-bucket fill below logs in replacements within this SAME cycle —
    // online headcount holds at target while WHO is online rotates (the
    // population reads like a living server, not a frozen roster). The post-
    // logout census is what ComputeTargets/PopulateActual then reconcile to.
    SessionRhythmLogout(rec_start);
    const uint32 ct_start = getMSTime();
    auto buckets = ComputeTargets();
    perf_window_compute_targets_ms_ += getMSTimeDiff(ct_start, getMSTime());
    if (buckets.empty()) { perf_window_reconcile_ms_ += getMSTimeDiff(rec_start, getMSTime()); return; }
    const uint32 pa_start = getMSTime();
    PopulateActual(buckets);
    perf_window_populate_actual_ms_ += getMSTimeDiff(pa_start, getMSTime());

    // Spawn-cap census for SpawnNew: marked characters minus owner-bound
    // companions. Once per cycle (SpawnNew keeps it current within it).
    pool_marked_count_ = 0;
    for (BotId id : Services::Lifecycle().snapshot_ids())
        if (Services::Owners().GetOwner(id).account_id == 0)
            ++pool_marked_count_;

    // Per-cycle diagnostic: without this the Population cycle was
    // entirely silent except for hygiene cleanups. Owner asked
    // 2026-05-15 "is population running?" — answer was a DB diff,
    // not a log. Summarize totals per cycle so the log answers
    // the question directly. Spawns/logins are reported by the
    // per-action paths; this is the planning view.
    uint32 sum_alli_actual = 0, sum_alli_target = 0;
    uint32 sum_horde_actual = 0, sum_horde_target = 0;
    for (auto const& b : buckets)
    {
        sum_alli_actual  += b.alliance_actual;
        sum_alli_target  += b.alliance_target;
        sum_horde_actual += b.horde_actual;
        sum_horde_target += b.horde_target;
    }
    TC_LOG_INFO("playerbot.v2",
        "[Population] reconcile buckets={} alli={}/{} horde={}/{} "
        "spawn_budget=A{}/H{} login_budget=A{}/H{}",
        uint32(buckets.size()),
        sum_alli_actual, sum_alli_target,
        sum_horde_actual, sum_horde_target,
        spawn_budget_alli_, spawn_budget_horde_,
        login_budget_alli_, login_budget_horde_);

    // Over-target side of the per-bucket reconcile. This needs no DB read, so it
    // stays in phase 1 and keeps its original position: logouts happen before
    // any login/spawn and before the global overflow kick.
    const uint32 rs_start = getMSTime();
    auto logout_side = [&](PopulationBucket const& b, bool alliance) {
        uint32 const actual = alliance ? b.alliance_actual : b.horde_actual;
        uint32 const target = alliance ? b.alliance_target : b.horde_target;
        if (actual > target)
        {
            uint32 excess = actual - target;
            const uint32 ll_start = getMSTime();
            // Single registry scan finds up to `excess` victims and logs
            // them all out — replaces the prior N × full-scan loop.
            LogoutLRUMany(b.level_lo, b.level_hi, alliance, excess);
            perf_window_logout_lru_ms_ += getMSTimeDiff(ll_start, getMSTime());
        }
    };

    // Under-target buckets go to phase 2, which cannot start until their rows
    // are in hand. One query per bucket (not per side): the row set is
    // faction-blind, and the per-side cap is applied to it in phase 2.
    reconcile_fills_.clear();
    reconcile_inflight_ = 0;
    ++reconcile_gen_;
    for (auto& b : buckets)
    {
        logout_side(b, /*alliance=*/true);
        logout_side(b, /*alliance=*/false);

        bool const alli_short  = b.alliance_actual < b.alliance_target;
        bool const horde_short = b.horde_actual    < b.horde_target;
        if (!alli_short && !horde_short)
            continue;

        // Widest row window either side could ask for this cycle — mirrors the
        // old `std::min(needed, login_budget)` per side.
        uint32 const alli_cap  = alli_short
            ? std::min(b.alliance_target - b.alliance_actual, login_budget_alli_) : 0u;
        uint32 const horde_cap = horde_short
            ? std::min(b.horde_target - b.horde_actual, login_budget_horde_) : 0u;
        uint32 const fetch_cap = std::max(alli_cap, horde_cap);

        size_t const idx = reconcile_fills_.size();
        reconcile_fills_.push_back(PendingBucketFill{ b, {}, /*ready=*/false });
        if (fetch_cap == 0)
        {
            // Nothing to log in (no login budget left, or the deficit is zero on
            // the side that has budget). The old code still issued a LIMIT 0
            // query here and got nothing back; skip the round-trip entirely and
            // let phase 2 go straight to the spawn path, which is what the empty
            // result produced.
            reconcile_fills_.back().ready = true;
            continue;
        }
        if (synchronous)
        {
            // Operator command: block for the rows so the whole cycle completes
            // before the command returns.
            reconcile_fills_.back().rows  = FetchOfflineCandidatesSync(b.level_lo, b.level_hi, fetch_cap);
            reconcile_fills_.back().ready = true;
            continue;
        }
        QueueOfflineCandidateFetch(idx, b.level_lo, b.level_hi, fetch_cap);
    }
    perf_window_reconcile_side_ms_ += getMSTimeDiff(rs_start, getMSTime());

    reconcile_in_progress_ = true;
    reconcile_started_ms_  = getMSTime();
    perf_window_reconcile_ms_ += getMSTimeDiff(rec_start, getMSTime());

    // Nothing is outstanding on the synchronous path, so run phase 2 right here.
    if (synchronous)
        ReconcileFinish();
}

void BotPopulationManager::ReconcileFinish()
{
    const uint32 rec_start = getMSTime();
    reconcile_in_progress_ = false;

    // Belt and braces: reconcile_inflight_ hitting zero is supposed to mean
    // every bucket has its rows. If one is somehow unanswered, abandon the fill
    // rather than run it — filling off a bucket with no rows would log nobody in
    // and spawn the whole deficit instead, which is exactly the
    // proceed-on-a-default this split exists to avoid.
    for (auto const& fill : reconcile_fills_)
        if (!fill.ready)
        {
            TC_LOG_ERROR("playerbot.v2",
                "[Population] reconcile fill abandoned: bucket {}-{} has no candidate rows",
                uint32(fill.bucket.level_lo), uint32(fill.bucket.level_hi));
            reconcile_fills_.clear();
            perf_window_reconcile_ms_ += getMSTimeDiff(rec_start, getMSTime());
            return;
        }

    // Per-bucket fill, BUT each side draws only from its own faction's
    // budget — so Alliance can no longer starve Horde (or vice versa) within
    // a cycle. See the budget reset at OnWorldTick for the split formula.
    const uint32 rs_start = getMSTime();
    auto fill_side = [&](PendingBucketFill const& fill, bool alliance) {
        PopulationBucket const& b = fill.bucket;
        uint32 const actual = alliance ? b.alliance_actual : b.horde_actual;
        uint32 const target = alliance ? b.alliance_target : b.horde_target;
        uint32& login_budget = alliance ? login_budget_alli_ : login_budget_horde_;
        uint32& spawn_budget = alliance ? spawn_budget_alli_ : spawn_budget_horde_;
        if (actual >= target)
            return;
        uint32 needed = target - actual;
        const uint32 le_start = getMSTime();
        auto offline = FilterOfflineCandidates(fill.rows, alliance,
                                               std::min(needed, login_budget));
        for (auto const& c : offline)
        {
            if (!login_budget) break;
            if (LoginExisting(c.char_guid, alliance)) { if (needed) --needed; }
        }
        perf_window_login_existing_ms_ += getMSTimeDiff(le_start, getMSTime());
        const uint32 sn_start = getMSTime();
        while (needed > 0 && spawn_budget > 0)
        {
            // Anywhere in the band, not its midpoint: a band of L14s
            // reads as a bot fleet (ike3 RandomizeFirst rolls urand too).
            uint8 lvl = uint8(urand(b.level_lo, b.level_hi));
            if (SpawnNew(lvl, alliance)) --needed;
            else break;
        }
        perf_window_spawn_new_ms_ += getMSTimeDiff(sn_start, getMSTime());
    };

    for (auto const& fill : reconcile_fills_)
    {
        fill_side(fill, /*alliance=*/true);
        fill_side(fill, /*alliance=*/false);
    }
    reconcile_fills_.clear();
    perf_window_reconcile_side_ms_ += getMSTimeDiff(rs_start, getMSTime());

    // B-10: global TotalTarget enforcement at Reconcile cadence. The
    // per-bucket loop above only sees bots whose level falls inside a
    // bucket range — anything outside every bucket was invisible here,
    // and the only global check was hygiene (hourly, 1.5× hard cap), so
    // a boot-time login overshoot persisted for up to an hour and a
    // ≤1.5× steady-state overshoot persisted forever. Kick the overflow
    // now, with the same content-engagement guards as the hygiene pass
    // (never yank a bot out of a group, dungeon/raid, or BG/arena).
    {
        const uint32 total_target = Services::Config().population_total_target();
        // Count only population-shaper inventory: altbots ride on top of
        // TotalTarget (see PopulateActual), and BG/LFG-queued + kick-
        // protected JIT match-fill bots are deliberate overshoot — if any
        // of them inflated `online`, the kick budget would land on
        // innocent POOL bots instead.
        const uint32 tally_now = getMSTime();
        uint32 online = 0;
        Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
        {
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            Player const* p = ObjectAccessor::FindConnectedPlayer(g);
            if (p && p->InBattlegroundQueue()) return;
            if (p && sLFGMgr->GetState(p->GetGUID()) != lfg::LFG_STATE_NONE) return;
            if (IsKickProtected(id, tally_now)) return;
            if (IsPlayerOwnedAlt(id, p)) return;
            if (IsOperatorPersistentBot(uint64(id))) return;  // dev/test bot — off the shaper's books
            ++online;
        });
        if (total_target > 0 && online > total_target)
        {
            const uint32 to_kick = online - total_target;
            std::vector<ObjectGuid> overflow;
            overflow.reserve(to_kick);
            Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
            {
                if (overflow.size() >= to_kick) return;
                ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
                Player* p = ObjectAccessor::FindConnectedPlayer(g);
                if (!p) return;
                if (p->GetGroup()) return;
                // Queued bots are intentional overshoot: BotQueueFiller
                // JIT-spawns them above TotalTarget to form BG matches and
                // LFG groups, and until the port/proposal they have no
                // group and sit on a normal map — kicking them here would
                // strangle autonomous match seeding. Hygiene reclaims them
                // after the run.
                if (p->InBattlegroundQueue()) return;
                if (sLFGMgr->GetState(p->GetGUID()) != lfg::LFG_STATE_NONE) return;
                // JIT match-fill bots between login and queue-join hold a
                // kick-protection lease (see ProtectFromKick).
                if (IsKickProtected(id, getMSTime())) return;
                // Altbots are a real player's squad members, not
                // population-shaper inventory — never kick them.
                if (IsPlayerOwnedAlt(id, p)) return;
                if (IsOperatorPersistentBot(uint64(id))) return;  // dev/test bot — never rotates out
                if (Map* m = p->GetMap())
                {
                    if (m->IsDungeon()) return;
                    if (m->IsBattlegroundOrArena()) return;
                }
                overflow.push_back(g);
            });
            if (!overflow.empty())
            {
                TC_LOG_INFO("playerbot.v2",
                    "[Population] reconcile: online {} > target {}; logging out {} overflow bot(s)",
                    online, total_target, uint32(overflow.size()));
                for (ObjectGuid g : overflow)
                    Services::SessionMgr().LogoutBot(g);
            }
        }
    }
    perf_window_reconcile_ms_ += getMSTimeDiff(rec_start, getMSTime());
}

void BotPopulationManager::RegisterPrioritySetup(uint64 bot_id)
{
    if (bot_id == 0) return;
    std::lock_guard<std::mutex> lk(priority_mtx_);
    priority_setup_ids_.insert(bot_id);
}

void BotPopulationManager::UnregisterPrioritySetup(uint64 bot_id)
{
    if (bot_id == 0) return;
    std::lock_guard<std::mutex> lk(priority_mtx_);
    priority_setup_ids_.erase(bot_id);
}

void BotPopulationManager::QueuePipelineRowFetch(uint64 guid_low)
{
    if (pipeline_row_cache_.contains(guid_low))
        return;
    // Pending flag: the drive pass runs at 1 Hz, so without this a bot whose
    // row takes longer than a second to come back would get a fresh duplicate
    // SELECT every pass. insert() returning false means one is already in
    // flight for this guid.
    if (!pipeline_row_fetching_.insert(guid_low).second)
        return;
    queries_.AddCallback(CharacterDatabase.AsyncQuery(Trinity::StringFormat(
        "SELECT setup_pipeline_state, distribution_level, IFNULL(jit_for_queue, '') "
        "FROM playerbot_v2_character WHERE character_guid_low={}", guid_low).c_str())
        .WithCallback([this, guid_low](QueryResult res)
    {
        // A missing marker means forget_pipeline_caches ran while this was in
        // flight (the character was deleted) — discard the row.
        if (pipeline_row_fetching_.erase(guid_low) == 0)
            return;
        // No row (character deleted between the drive pass and now): leave the
        // cache empty. The next pass re-issues, exactly as the synchronous
        // version re-SELECTed every pass for a rowless guid.
        if (!res || !res->GetRowCount())
            return;
        Field* f = res->Fetch();
        pipeline_row_cache_.emplace(guid_low,
            PipelineRow{ f[0].GetUInt8(), f[1].GetUInt8(), f[2].GetString() });
    }));
}

void BotPopulationManager::DriveSetupPipelines()
{
    // 1-second throttle: this used to fire every world frame (50Hz),
    // and on a cold cache that meant 10 sync `CharacterDatabase.PQuery`
    // reads × 50Hz = 500 sync DB roundtrips/sec on the WORLD thread.
    // Pipelines aren't latency-critical (they run during multi-tick
    // setup teleport sequences); 1-second cadence is plenty and
    // keeps the world thread off the DB worker. Perf audit
    // a2155f31bccccc961 ranked this as catastrophic-on-cold-cache.
    //
    // Both of those reads are now ASYNC as well: the cold row load goes through
    // QueuePipelineRowFetch (callback fills pipeline_row_cache_) and the
    // pipeline's own resume read is gone entirely — the cached row is handed to
    // RunFor as a StateRef. Nothing in this function blocks on the DB.
    constexpr uint32 kDrivePipelinesIntervalMs = 1000u;
    const uint32 now = getMSTime();
    if (last_drive_pipelines_ms_ &&
        (now - last_drive_pipelines_ms_) < kDrivePipelinesIntervalMs)
        return;
    last_drive_pipelines_ms_ = now;

    // Budget for NEW async row fetches this pass. The queries no longer block
    // the world thread, but a cold cache at boot would otherwise hand the DB
    // worker one query per in-world bot in a single pass (3000 at fleet scale)
    // and starve every other async read behind them. 25/s warms a 3000-bot
    // fleet in two minutes while the already-cached bots keep progressing; in
    // steady state the demand is the ~3 logins per tick, far below the budget.
    uint32 fetch_budget = 25;

    // Priority pass (2026-05-21): bots registered via RegisterPrioritySetup
    // get drained FIRST and without the per-tick budget cap. Existed to fix
    // `.playerbot smoketest` under 1000-bot load: the normal Pass I cap of
    // 10/tick starved freshly-spawned smoketest bots indefinitely because
    // they sit at the tail of the registry's hash order. Operates on a
    // copy of the set so the world thread doesn't hold the mutex across
    // the per-bot work (pipeline RunFor calls into DB, teleport, talents,
    // etc.).
    std::vector<uint64> priority_snapshot;
    {
        std::lock_guard<std::mutex> lk(priority_mtx_);
        priority_snapshot.reserve(priority_setup_ids_.size());
        for (uint64 id : priority_setup_ids_) priority_snapshot.push_back(id);
    }
    for (uint64 id : priority_snapshot)
    {
        if (setup_done_cache_.contains(id)) continue;
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* p = ObjectAccessor::FindConnectedPlayer(g);
        if (!p) continue;  // not in-world yet — re-check next tick
        // Cold-resolve the row state for this bot — same logic as the
        // general pass but without the cap. Cheap because priority set is
        // small (smoketest count, typically <50).
        uint8 pri_state = 0;
        uint8 pri_dist  = 0;
        auto pc = pipeline_row_cache_.find(id);
        if (pc == pipeline_row_cache_.end())
        {
            // Not cached yet: start the async load and move on. The bot is
            // picked up on a later pass, once the row is actually in hand —
            // never driven on a default state. The priority set is served from
            // the fetch budget first (it is small, and these bots are the ones
            // a waiting smoketest needs).
            if (fetch_budget) { QueuePipelineRowFetch(id); --fetch_budget; }
            continue;
        }
        pri_state = pc->second.state;
        pri_dist  = pc->second.dist_level;
        bool pri_done = (pri_state == 0xFF);
        bool pri_complete = false;
        if (!pri_done && pri_dist > 0)
        {
            // The pipeline resumes from (and writes back to) the cached row
            // instead of SELECTing setup_pipeline_state itself.
            BotSetupPipeline::StateRef sref{ pri_state, pri_dist };
            pri_complete = pipeline_->RunFor(p, pri_dist, &sref);
            if (auto cit = pipeline_row_cache_.find(id); cit != pipeline_row_cache_.end())
            {
                cit->second.state      = sref.state;
                cit->second.dist_level = sref.dist_level;
            }
        }
        else if (pri_done)
            pri_complete = true;
        if (pri_complete)
        {
            setup_done_cache_.insert(id);
            // Auto-unregister — the caller (smoketest) doesn't have to
            // clean up after itself when AllDone is reached. Removal
            // here makes the priority set self-pruning.
            std::lock_guard<std::mutex> lk(priority_mtx_);
            priority_setup_ids_.erase(id);
        }
    }

    // Pass I: walk Services::Registry() (live in-world bots) instead of
    // Services::Lifecycle() (all marked, ~11K historical accumulation).
    // The lifecycle registry never decrements on logout — each spawn
    // adds an entry that persists forever — so at scale it scanned 11K
    // ids per second to find ~10 in-world non-cached bots. Live registry
    // is bounded by the actual in-world bot count (200-3000 typical) and
    // its entries are by definition online (no FindConnectedPlayer race).
    int driven = 0;
    // Time budget: one RunFor (teleport, spells, talents, gear) costs 10-25 ms on the world thread, so ten of them in
    // one pass were the 170-210 ms 'pop=' spikes of the 300-bot test. Stop starting new ones after 30 ms; the rest
    // follow on the next pass (a second later).
    constexpr uint32 kPassBudgetMs = 30u;
    const uint32 pass_start_ms = getMSTime();
    std::vector<uint64> live_ids;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        live_ids.push_back(uint64(id));
    });
    for (auto id : live_ids)
    {
        if (driven >= 10) break;  // throttle per tick
        if (getMSTimeDiff(pass_start_ms, getMSTime()) >= kPassBudgetMs) break;
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* p = ObjectAccessor::FindConnectedPlayer(g);
        if (!p) continue;
        // In-memory short-circuit: if RunFor returned true earlier for this
        // bot, skip the DB roundtrip entirely. Without this, the async
        // PersistState write hasn't committed by the time we re-read state
        // here, so we'd re-enter the pipeline and re-fire DoPlaceAndTravel's
        // teleport every tick - the cause of the recent map-worker freezes.
        if (setup_done_cache_.contains(uint64(id))) continue;

        // Pass H: in-memory cache of the pipeline row. With many bots in
        // setup state at scale, the outer SELECT was firing ~10/sec on
        // the world thread — 50-150 ms/sec just on this readback. The
        // cache stores (state, dist_level, jit_tag) keyed by guid;
        // populated lazily on first observation and refreshed only when
        // RunFor reports a state change. RunFor itself also reads/writes
        // state internally — that's a separate hot path tracked under
        // pipelines and not eliminated by this cache.
        uint8 state = 0;
        uint8 dist = 0;
        std::string jit_tag;
        auto cached_it = pipeline_row_cache_.find(uint64(id));
        if (cached_it == pipeline_row_cache_.end())
        {
            // Cold load — one ASYNC SELECT, then cached forever (state
            // mutations are mirrored into the cache below, both by the
            // pipeline's StateRef write-back and by the JIT clear). The bot is
            // simply skipped until the row lands: driving the pipeline off a
            // default state would re-run DoPlaceAndTravel's teleport on a bot
            // that is in fact already done.
            if (fetch_budget) { QueuePipelineRowFetch(uint64(id)); --fetch_budget; }
            continue;
        }
        state   = cached_it->second.state;
        dist    = cached_it->second.dist_level;
        jit_tag = cached_it->second.jit_tag;
        bool already_done = (state == 0xFF);

        // Run pipeline. Returns true when complete (whether just-completed or already-done).
        // The cached row is handed in as the pipeline's resume point, so RunFor /
        // RunStarterOnly no longer SELECT setup_pipeline_state (and
        // distribution_level) back on the world thread on every call.
        bool complete = false;
        BotSetupPipeline::StateRef sref{ state, dist };
        bool ran = false;
        if (!already_done && dist > 0)
        {
            complete = pipeline_->RunFor(p, dist, &sref);
            ran = true;
            if (complete) ++driven;
        }
        else if (!already_done && dist == 0 && BotNeedsStarterOnly(p))
        {
            // Start-level Dracthyr/DK with no distribution bucket: run the
            // starter quest chain + relocate to capital (kept at start level).
            // Count every call toward the throttle — each does a quest reward
            // (DB write), and there can be hundreds of these bots.
            complete = pipeline_->RunStarterOnly(p, &sref);
            ran = true;
            ++driven;
        }
        else if (already_done)
        {
            complete = true;
        }
        // Mirror whatever the pipeline persisted back into the cache, so the
        // next pass resumes from the current bitmask without a DB read.
        if (ran)
            if (auto cit = pipeline_row_cache_.find(uint64(id)); cit != pipeline_row_cache_.end())
            {
                cit->second.state      = sref.state;
                cit->second.dist_level = sref.dist_level;
            }

        // Cache once we know the pipeline is finished. Covers both "we just
        // completed it" and "DB already says done" branches.
        if (complete) setup_done_cache_.insert(uint64(id));

        // Phase D follow-up: if this bot was JIT-spawned for a queue and the
        // setup pipeline is now complete, push the matching queue intent and
        // clear the JIT tag so we fire once and only once.
        // Tag format set by BotQueueFiller: "<KIND>:<INSTANCE_ID>:<ROLE>"
        //   e.g. "BG:128:DPS", "LFG:271:HEAL"
        if (complete && !jit_tag.empty())
        {
            // Parse "<KIND>:<id>:<role>"
            auto first_colon = jit_tag.find(':');
            auto last_colon  = jit_tag.rfind(':');
            if (first_colon != std::string::npos && last_colon != first_colon)
            {
                std::string kind = jit_tag.substr(0, first_colon);
                uint32 instance_id = uint32(std::strtoul(
                    jit_tag.substr(first_colon + 1, last_colon - first_colon - 1).c_str(),
                    nullptr, 10));
                std::string role_str = jit_tag.substr(last_colon + 1);
                Role role = (role_str == "TANK") ? Role::Tank
                          : (role_str == "HEAL") ? Role::Healer
                          : Role::Dps;

                // Spec alignment: pick the right spec id for (class, role)
                // and ActivateTalentGroup so the bot enters the queue with
                // the matching role's talents. Without this, a JIT-tagged
                // "Tank Warrior" might be queued as Arms (default first
                // spec), failing role validation in dungeon finder.
                if (role != Role::Dps)
                {
                    uint32 target_spec = 0;
                    // Canonical role→spec mapping in Bot/ClassTables.cpp.
                    // Single source for all role-switching call sites.
                    uint8 cls = p->GetClass();
                    target_spec = (role == Role::Tank)
                                  ? TankSpecForClass(cls)
                                  : HealerSpecForClass(cls);
                    if (target_spec != 0 && uint32(::Playerbot::Classic::EffectiveSpecId(p)) != target_spec)
                    {
                        if (::Playerbot::Classic::IsSingleSpecClass(cls))
                        {
                            // Classic 1.60: no retail ChrSpecialization row to activate (the
                            // lookup below finds nothing and the bot queued in its DPS spec).
                            // The role is the virtual spec plus its talent build.
                            ::Playerbot::Classic::SetSpecOverride(p, target_spec);
                            ::Playerbot::API(p).apply_classic_talents();
                        }
                        else if (auto const* spec_entry = sChrSpecializationStore.LookupEntry(target_spec))
                            p->ActivateTalentGroup(spec_entry);
                    }
                }

                // A reused JIT character logs in where it was last saved: dead (ghost), mid-fight in a hostile zone, hurt. WSG test
                // 2026-10-08: such bots carried on with world life and were only invited ~80 s later, once alive and idle. A bot taken
                // for a BG is made fit for the queue first: revived (corpse retired, as API::resurrect does), healed, out of combat.
                if (kind == "BG")
                {
                    if (!p->IsAlive())
                    {
                        p->ResurrectPlayer(1.0f, /*applySickness*/ false);
                        p->SpawnCorpseBones();
                    }
                    if (p->IsAlive())
                    {
                        p->SetFullHealth();
                        if (p->GetPowerType() == POWER_MANA)
                            p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                        if (p->IsInCombat())
                            p->CombatStop(true);
                    }
                }

                Intent it{};
                it.bot_id = id;
                if (kind == "BG")
                    it.body = QueueIntent{BgQueueIntent{ObjectGuid::Empty, uint16(instance_id)}};
                else
                    it.body = QueueIntent{LfgQueueIntent{instance_id, role}};
                Services::Intents(id).push(std::move(it));

                // Owner directive 2026-06-22: a JIT BG bot must do ONLY its purpose.
                // Flag it so idle:bg_jit_staging confines it to queue->wait->port
                // (no questing/roaming) — which also keeps it queued+ready so the
                // match can actually form. LFG/dungeon JIT uses the existing LFG
                // proposal flow; BG is the one that needed confinement.
                if (kind == "BG")
                    if (BotAI* jit_ai = Services::Registry().ai(id))
                        jit_ai->set_bg_jit_purpose(uint16(instance_id), getMSTime());

                // Telemetry: JIT path took ~setup-pipeline-time + 1-2 ticks.
                // We don't have the original Filler::Fill timestamp on hand,
                // so approximate via a fixed 2000ms estimate. A future
                // refinement can persist the start_ms in the jit_for_queue
                // tag itself. Good enough for "verify it's <10s in practice".
                Services::Perf().record_queue_fill_completion(Ms{2000});

                TC_LOG_INFO("playerbot.v2",
                    "[Population] JIT setup complete for {} L{}; pushed {} queue intent (instance={}, role={})",
                    p->GetName(), uint32(p->GetLevel()), kind, instance_id, role_str);
            }
            CharacterDatabase.PExecute(
                "UPDATE playerbot_v2_character SET jit_for_queue=NULL, last_seen_at=NOW() "
                "WHERE character_guid_low={}", uint64(id));
            // Mirror the JIT clear into the cache so we don't re-fire the
            // queue intent on the next pipeline tick.
            auto cit = pipeline_row_cache_.find(uint64(id));
            if (cit != pipeline_row_cache_.end()) cit->second.jit_tag.clear();
        }
        // After a successful completion, lock the cache's state to AllDone
        // so subsequent ticks see the same state without re-SELECT. setup_done_cache_
        // also gates the per-call work above; this just keeps both caches consistent.
        if (complete)
        {
            auto cit = pipeline_row_cache_.find(uint64(id));
            if (cit != pipeline_row_cache_.end()) cit->second.state = 0xFF;
        }
    }
}

uint32 BotPopulationManager::ms_until_next_reconcile(uint32 now_ms) const
{
    if (last_tick_ms_ == 0) return 0;
    const uint32 next = last_tick_ms_ + kTickIntervalMs;
    return now_ms >= next ? 0u : (next - now_ms);
}

uint32 BotPopulationManager::ms_until_next_hygiene(uint32 now_ms) const
{
    if (last_hygiene_ms_ == 0) return 0;
    const uint32 next = last_hygiene_ms_ + kHygieneIntervalMs;
    return now_ms >= next ? 0u : (next - now_ms);
}

uint32 BotPopulationManager::ms_until_next_rebalance(uint32 now_ms) const
{
    if (last_rebalance_ms_ == 0) return 0;
    const uint32 next = last_rebalance_ms_ + kRebalanceIntervalMs;
    return now_ms >= next ? 0u : (next - now_ms);
}

// Rebalance pass — proactive spec rotation.
//
// Idea: BotQueueFiller already does reactive role coverage (online bots
// match needed role; if not enough, switch hybrid DPS in place; if still
// not enough, JIT-spawn). The reactive path's worst case is JIT-spawning
// 5-15s of pipeline work per missing slot — that latency is what players
// feel when /lfg pops slowly.
//
// This cron pre-empties that path: every 5 min, walk online bots grouped
// by (faction, bracket), count tanks/healers/DPS, and if a bracket has
// fewer than 1 tank or 1 healer per 5 bots, switch one hybrid DPS bot in
// that bracket into the under-rep role. The next /lfg request in that
// bracket finds the right spec already active and skips the conversion
// step (and avoids a JIT spawn entirely if the fleet was already large
// enough but mis-specced).
//
// Constraints:
//   - Per-bracket cap of 1 switch per cycle prevents mass-respec spikes.
//   - Per-bot 60min lockout via ActionKind::DualSpec (already used by
//     idle:dual_spec_switch) prevents flip-flop respec.
//   - Skips bots that are in combat / dungeon / raid / group — switching
//     a bot mid-content would break their group's plan.
//   - Skips bots whose owner has /setrole-pinned a different role —
//     respect explicit owner intent over rebalance heuristics.
std::string BotPopulationManager::StartBgStress(uint16 bg_type, uint32 cycles, uint32 hold_sec)
{
    if (bg_stress_.phase != BgStress::Phase::Idle || bg_stress_.cycles_left)
        return BgStressReport();
    if (!bg_type || !cycles)
        return "usage: .bot bgstress <bg_type_id> <cycles> [hold_sec]  (1 = Alterac Valley, 2 = Warsong Gulch, 3 = Arathi Basin)";
    bg_stress_ = BgStress{};
    bg_stress_.bg_type = bg_type;
    bg_stress_.cycles_left = cycles;
    bg_stress_.hold_ms = std::max(10u, hold_sec ? hold_sec : 60u) * 1000u;
    return fmt::format("[bgstress] started: bg_type={} cycles={} hold={}s. Watch the [bgstress] and [slow_call] lines.", bg_type,
                       cycles, bg_stress_.hold_ms / 1000u);
}

std::string BotPopulationManager::BgStressReport() const
{
    if (!bg_stress_.cycles_left && bg_stress_.phase == BgStress::Phase::Idle)
        return "[bgstress] not running";
    return fmt::format("[bgstress] running: bg_type={} cycle={} left={} peak_queued={} peak_in_bg={}", bg_stress_.bg_type,
                       bg_stress_.cycle, bg_stress_.cycles_left, bg_stress_.peak_queued, bg_stress_.peak_in_bg);
}

void BotPopulationManager::DriveBgStress(uint32 now_ms)
{
    BgStress& st = bg_stress_;
    if (!st.cycles_left && st.phase == BgStress::Phase::Idle)
        return;
    if (st.phase != BgStress::Phase::Idle && int32(now_ms - st.phase_end_ms) < 0)
        return;

    if (st.phase == BgStress::Phase::Idle)
    {
        // Start a cycle: one fill, the same path the queue hook and the seed use.
        ++st.cycle;
        BotQueueFiller filler;
        filler.Fill(BuildAmbientBgFill(st.bg_type));
        st.phase = BgStress::Phase::Hold;
        st.phase_end_ms = now_ms + st.hold_ms;
        return;
    }

    if (st.phase == BgStress::Phase::Hold)
    {
        // End of the hold: count, log, then every ambient bot in a queue or a match is asked to leave.
        uint32 queued = 0, in_bg = 0, left = 0;
        for (BotId id : Services::Lifecycle().snapshot_ids())
        {
            Player* bot = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
            if (!bot || !bot->IsInWorld() || ::Playerbot::Pool::RoleOf(id) == ::Playerbot::Pool::BotRole::Owned)
                continue;
            if (bot->InBattleground()) ++in_bg;
            else if (bot->InBattlegroundQueue()) ++queued;
            else continue;
            Intent leave{};
            leave.bot_id = id;
            leave.body   = QueueIntent{BgLeaveIntent{}};
            Services::Intents(id).push(std::move(leave));
            ++left;
        }
        st.peak_queued = std::max(st.peak_queued, queued);
        st.peak_in_bg  = std::max(st.peak_in_bg, in_bg);
        TC_LOG_INFO("playerbot.v2", "[bgstress] cycle {} bg_type={} queued={} in_bg={} asked_to_leave={} bots_online={} rss_mb={}",
                    st.cycle, st.bg_type, queued, in_bg, left, Services::Lifecycle().size(), WorldLoad().rss_mb);
        --st.cycles_left;
        st.phase = BgStress::Phase::Rest;
        st.phase_end_ms = now_ms + 20000u;   // let the leave intents drain and the matches close
        return;
    }

    // Rest over: next cycle, or done.
    if (st.cycles_left)
    {
        st.phase = BgStress::Phase::Idle;
        return;
    }
    TC_LOG_INFO("playerbot.v2", "[bgstress] done: {} cycles, peak queued={} peak in_bg={}", st.cycle, st.peak_queued, st.peak_in_bg);
    st = BgStress{};
}

std::string BotPopulationManager::StartBgSoak(uint16 bg_type, uint32 max_minutes)
{
    if (bg_soak_.phase == BgSoak::Phase::Running)
        return BgSoakReport();
    if (!bg_type || !sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(bg_type)))
        return "usage: .bot bgsoak <bg_type_id> [max_minutes]  (1 = Alterac Valley, 2 = Warsong Gulch, 3 = Arathi Basin)";
    bg_soak_ = BgSoak{};
    bg_soak_.bg_type = bg_type;
    bg_soak_.phase = BgSoak::Phase::Running;
    bg_soak_.started_ms = 0;   // set by the first drive tick, which also does the fill
    bg_soak_.deadline_ms = std::max(5u, max_minutes ? max_minutes : 30u) * 60u * 1000u;   // a length until started_ms is known
    return fmt::format("[bgsoak] started: bg_type={} max={} min. Poll `.bot bgsoak`; the final line starts with \"[bgsoak] done\".", bg_type,
                       bg_soak_.deadline_ms / 60000u);
}

std::string BotPopulationManager::BgSoakReport() const
{
    if (bg_soak_.phase == BgSoak::Phase::Idle)
        return "[bgsoak] not started";
    if (bg_soak_.phase == BgSoak::Phase::Done)
        return bg_soak_.result;
    return fmt::format("[bgsoak] running: bg_type={} peak_in_bg={} last_status={}", bg_soak_.bg_type, bg_soak_.peak_in_bg, uint32(bg_soak_.last_status));
}

void BotPopulationManager::DriveBgSoak(uint32 now_ms)
{
    BgSoak& st = bg_soak_;
    if (st.phase != BgSoak::Phase::Running)
        return;
    if (!st.started_ms)
    {
        // First tick: one fill, the same path the queue hook, the seed and the stress test use.
        st.started_ms = now_ms ? now_ms : 1u;
        st.deadline_ms += st.started_ms;
        st.last_log_ms = st.started_ms;
        BotQueueFiller filler;
        filler.Fill(BuildAmbientBgFill(st.bg_type));
        TC_LOG_INFO("playerbot.v2", "[bgsoak] fill queued bg_type={}", st.bg_type);
        return;
    }

    if (int32(now_ms - st.last_poll_ms) < 5000)   // a pass over every bot, every 5 s is enough
        return;
    st.last_poll_ms = now_ms;

    // Who is inside a battleground of this type, and in which state it is.
    uint32 in_bg = 0, queued = 0;
    Battleground* bg = nullptr;
    for (BotId id : Services::Lifecycle().snapshot_ids())
    {
        Player* bot = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!bot || !bot->IsInWorld())
            continue;
        if (Battleground* b = bot->GetBattleground(); b && uint32(b->GetTypeID()) == st.bg_type)
        {
            ++in_bg;
            if (!bg) bg = b;
        }
        else if (bot->InBattlegroundQueue())
            ++queued;
    }
    st.peak_in_bg = std::max(st.peak_in_bg, in_bg);
    if (in_bg && !st.first_in_bg_ms)
        st.first_in_bg_ms = now_ms;
    if (bg)
        st.last_status = uint8(bg->GetStatus());

    if (int32(now_ms - st.last_log_ms) >= 60000)
    {
        st.last_log_ms = now_ms;
        TC_LOG_INFO("playerbot.v2", "[bgsoak] t={}s bg_type={} in_bg={} queued={} status={} peak_in_bg={} bots_online={} rss_mb={}",
                    (now_ms - st.started_ms) / 1000u, st.bg_type, in_bg, queued, uint32(st.last_status), st.peak_in_bg,
                    Services::Lifecycle().size(), WorldLoad().rss_mb);
    }

    char const* result = nullptr;
    if (st.first_in_bg_ms && !in_bg)
        result = "ended";                    // the match ran and the bots have all left it
    else if (bg && bg->GetStatus() == STATUS_WAIT_LEAVE)
        result = nullptr;                    // over, the bots are on their way out: the next tick sees the empty field
    else if (!st.first_in_bg_ms && int32(now_ms - st.started_ms) >= 10 * 60 * 1000)
        result = "never_started";            // ten minutes and no bot ever entered
    else if (int32(now_ms - st.deadline_ms) >= 0)
        result = "timeout";
    if (!result)
        return;

    uint32 left = 0;
    if (!std::strcmp(result, "timeout"))
        for (BotId id : Services::Lifecycle().snapshot_ids())
        {
            Player* bot = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
            if (!bot || !bot->IsInWorld() || ::Playerbot::Pool::RoleOf(id) == ::Playerbot::Pool::BotRole::Owned)
                continue;
            if (!bot->InBattleground() && !bot->InBattlegroundQueue())
                continue;
            Intent leave{};
            leave.bot_id = id;
            leave.body   = QueueIntent{BgLeaveIntent{}};
            Services::Intents(id).push(std::move(leave));
            ++left;
        }
    st.result = fmt::format("[bgsoak] done bg_type={} result={} elapsed={}s peak_in_bg={} last_status={} asked_to_leave={}", st.bg_type, result,
                            (now_ms - st.started_ms) / 1000u, st.peak_in_bg, uint32(st.last_status), left);
    TC_LOG_INFO("playerbot.v2", "{}", st.result);
    st.phase = BgSoak::Phase::Done;
}

BotQueueFiller::FillRequest BotPopulationManager::BuildAmbientBgFill(uint16 bg_type)
{
    BotQueueFiller::FillRequest req{};
    req.kind              = BotQueueFiller::QueueKind::Bg;
    req.bracket           = 7;          // max-level bracket: most complete kits
    req.faction           = ALLIANCE;   // Fill runs both factions for Bg kind
    req.instance_id       = bg_type;
    req.requesting_player = nullptr;
    // Pin level + bracket + per-team CAP exactly like the player-triggered path
    // (OnPlayerJoinedBgQueue). Without these the autonomous seed defaulted to
    // target_level 80±15 with NO cap, so NeedsFor(Bg)=36/side overcommitted ~72
    // fresh bots per cycle — far more than the setup pipeline can level+gear before
    // the next cycle. The pool then filled with half-setup sub-65 bots that the
    // level filter rejects and that never converge into a ready, queued L80 roster,
    // so the matchmaker could never assemble a side (live: WSG stuck PREP 0/0,
    // never IN_PROGRESS). Cap to the BG's MaxPlayersPerTeam so each side spawns just
    // enough; reuse of already-built L80 bots then dominates over fresh creation.
    const uint8 max_level = MaxPlayerLevel();
    req.target_level_override = max_level;
    if (BattlegroundTemplate const* tmpl =
            sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(bg_type)))
    {
        if (uint16 const max_per_team = tmpl->GetMaxPlayersPerTeam())
            req.max_total_bots = uint8(std::min<uint16>(max_per_team, 255));
        if (!tmpl->MapIDs.empty())
        {
            // Resolve the max-level bracket by LEVEL (not a hardcoded bracket
            // id) so it follows the realm cap (80 -> 90 with Midnight).
            if (PVPDifficultyEntry const* diff =
                    DB2Manager::GetBattlegroundBracketByLevel(tmpl->MapIDs.front(), max_level))
            {
                req.bracket_min_level = uint8(std::clamp<int32>(diff->MinLevel, 1, max_level));
                req.bracket_max_level = uint8(std::clamp<int32>(diff->MaxLevel, 1, max_level));
            }
        }
    }
    return req;
}

void BotPopulationManager::SeedBgMatches(uint32 now_ms)
{
    // Autonomous BG seeding (audit B24). The only BG entry point used to be
    // a REAL player's CMSG queue packet — with zero human players, the whole
    // BG subsystem was dormant all night (0 [QueueFill] lines, wins=0).
    // Every kSeedIntervalMs, if no bot BG is currently active, queue both
    // factions of a rotating bg_type so TC's matchmaker forms a bot-vs-bot
    // match organically (Fill handles both factions + JIT spawn shortfall).
    // Population 0 means owner bots only: no ambient fleet, so no ambient
    // matches either (each seed JIT-creates and logs in a full BG roster).
    if (!AmbientFleetEnabled()) return;
    const int32 matches_cap = sConfigMgr->GetIntDefault("Playerbot.Bg.AutoSeed.Matches", 0, true);
    if (matches_cap <= 0) return;
    constexpr uint32 kSeedIntervalMs = 3u * 60u * 1000u;
    if (last_bg_seed_ms_ && (now_ms - last_bg_seed_ms_) < kSeedIntervalMs) return;
    last_bg_seed_ms_ = now_ms;

    // Count active bot-relevant BG instances (any status) — same enumeration
    // TopUpActiveBGs uses. Seed only when below the cap.
    int32 active = 0;
    {
        std::unordered_set<uint32> seen_maps;
        for (BattlemasterListXMapEntry const* xmap : sBattlemasterListXMapStore)
        {
            if (!xmap || xmap->MapID <= 0) continue;
            if (!seen_maps.insert(uint32(xmap->MapID)).second) continue;
            for (Battleground* bg : sBattlegroundMgr->GetBGFreeSlotQueueStore(uint32(xmap->MapID)))
                if (bg) ++active;
        }
    }
    if (active > 0)
        bg_seed_match_seen_ = true;   // current type produced a match — rotate next cycle
    if (active >= matches_cap) return;

    // Saturation guard: when plenty of bots are already BG-queued, the
    // matchmaker is only waiting on in-flight JIT setups — re-seeding
    // would JIT-spawn ANOTHER full deficit on top (the filler can't see
    // setups that haven't queued yet). 72 ≈ both sides of a 36/side fill.
    {
        uint32 already_queued = 0;
        Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
        {
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            if (Player const* p = ObjectAccessor::FindConnectedPlayer(g))
                if (p->InBattlegroundQueue()) ++already_queued;
        });
        if (already_queued >= 72u)
        {
            TC_LOG_INFO("playerbot.v2",
                "[BgSeed] skipped: {} bots already queued (waiting on matchmaker/setups)",
                already_queued);
            return;
        }
    }

    // Rotate through evergreen, bracket-friendly BGs (validated
    // battleground_template ids): WSG, AB, EotS, Twin Peaks, BfG, Kotmogu.
    // Rotation = every normal BG with a live battleground_template row AND
    // a verified data layer.
    //
    // CRITICAL MAP-ID FIX (2026-06-22): WSG and AB exist under TWO
    // BattlemasterList ids each — a LEGACY one bound to the retired map and
    // a MODERN one bound to the live map that actually carries the flag/node
    // GameObjects + the registered BattlegroundScript:
    //   WSG: BML 2  "Warsong Gulch - Classic" -> map 489  (0 GOs, NO script)
    //        BML 1014 "Warsong Gulch"          -> map 2106 (10 GOs, script)
    //   AB : BML 3  "Arathi Basin"             -> map 529  (0 GOs, NO script)
    //        BML 1018 "AB New"                 -> map 2107 (34 GOs, script)
    // (BG map = BattlemasterListXMap[template.ID]; entry teleport lands on
    //  the template's WorldSafeLocs start-loc map. For 489/529 BOTH are the
    //  dead legacy map, so bots reached a live match on an EMPTY field — no
    //  flag GO ever spawned there, so score_delta stayed 0 forever despite
    //  [bgcoord] assigning carriers.) The modern ids land bots on 2106/2107
    //  where the flags exist; BattlegroundScriptMgr::AliasToBaseBg already
    //  maps 1014->WSG and 1018->AB advice. EotS 7 (566), Twin Peaks 108
    //  (726), BfG 120 (761) and Kotmogu 699 (998) are already consistent
    //  (script map == BML map) so they stay on their canonical ids.
    // Silvershard 708 + Seething Shore 894 join once their cart/azerite
    // mechanics are verified live; Deepwind 754 has NO template row on this
    // core (cannot be created).
    // Forever (1.12 ruleset on the classic maps 489/529/30) seeds WSG 2, AB 3
    // and AV 1; the modern ids above have no template or map there.
    // Playerbot.Bg.AutoSeed.Types overrides the list (comma-separated
    // BattlemasterList ids); ids with no battleground_template row are
    // dropped so a stale list can never queue a BG that cannot start.
    static std::vector<uint16> seed_types;
    {
        std::vector<uint16> wanted;
        std::string const list = sConfigMgr->GetStringDefault("Playerbot.Bg.AutoSeed.Types", "", true);
        if (list.empty())
            wanted = { 2, 3, 1 };
        else
            for (std::string_view tok : Trinity::Tokenize(list, ',', false))
                if (Optional<uint32> id = Trinity::StringTo<uint32>(tok))
                    wanted.push_back(uint16(*id));
        seed_types.clear();
        for (uint16 id : wanted)
            if (sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(id)))
                seed_types.push_back(id);
    }
    if (seed_types.empty())
    {
        TC_LOG_WARN("playerbot.v2", "[BgSeed] no seedable battleground template found; skipping");
        return;
    }
    // STICKY rotation: advance to the next type only when the previous
    // seed actually produced a match (active>0 observed since). Rotating
    // every cycle regardless (the old behavior) DILUTED the queues — each
    // 3-min cycle pushed a different bg_type while the prior type's JIT
    // setups were still completing, so no single (type, faction, bracket)
    // queue ever reached MinPlayers and the fleet ballooned with stranded
    // JIT bots (observed: 196 in-world, 51 queued across 4 types, 0
    // matches). Re-seeding the SAME type also self-heals: Fill counts the
    // already-queued bots as coverage and JIT-spawns only the remainder.
    if (bg_seed_match_seen_)
    {
        ++bg_seed_rotation_;
        bg_seed_match_seen_ = false;
    }
    // Test harness: pin the seed to ONE BattlemasterList id so a specific BG
    // can be driven and verified in isolation ("one after the other"). 0 (the
    // default) keeps the normal sticky rotation. Read live (sConfigMgr) so it
    // can be changed via `.reload config` without a restart. Useful BMLs:
    // WSG 1014, AB 1018, EotS 7, Twin Peaks 108, BfG 120, Kotmogu 699.
    const int32 forced_bg_type =
        sConfigMgr->GetIntDefault("Playerbot.Bg.AutoSeed.ForceType", 0, true);
    const uint16 bg_type = forced_bg_type > 0
        ? uint16(forced_bg_type)
        : seed_types[bg_seed_rotation_ % seed_types.size()];

    BotQueueFiller::FillRequest const req = BuildAmbientBgFill(bg_type);
    BotQueueFiller filler;
    filler.Fill(req);
    TC_LOG_INFO("playerbot.v2",
        "[BgSeed] seeded bg_type={} bracket=7 (active={} cap={})",
        uint32(bg_type), active, matches_cap);
}

void BotPopulationManager::SeedArenaMatches(uint32 now_ms)
{
    // Autonomous arena skirmish seeding. Unlike BGs (which the matchmaker
    // assembles from independent solo queuers via BotQueueFiller), an arena
    // queue entry is a GROUP keyed by team size — so the only way to drive
    // bot-vs-bot skirmishes with zero humans is to FORM two arena_type-sized
    // groups (one per faction) and have each leader queue its group. We do
    // that by publishing one ArenaTeamForming CoordEvent per faction;
    // BotGroupBuilder (bus async consumer) picks the bots, invites, and
    // (after the group forms) the leader emits an arena BgQueueIntent.
    //
    // Gated OFF by default so it never disturbs normal BG/arena play.
    // `Playerbot.Bg.AutoSeed.Arena` = desired number of concurrent seeded
    // skirmishes (0 = disabled). One skirmish needs both factions' groups.
    if (!AmbientFleetEnabled()) return;   // owner bots only, see SeedBgMatches
    const int32 arena_cap = sConfigMgr->GetIntDefault("Playerbot.Bg.AutoSeed.Arena", 0, true);
    if (arena_cap <= 0) return;

    constexpr uint32 kArenaSeedIntervalMs = 3u * 60u * 1000u;
    if (last_arena_seed_ms_ && (now_ms - last_arena_seed_ms_) < kArenaSeedIntervalMs) return;
    last_arena_seed_ms_ = now_ms;

    if (!Services::Initialized()) return;

    // Skirmish is 3v3-ONLY in this core (BattlegroundMgr::IsValidQueueId →
    // ArenaSkirmish requires teamSize == 3v3). 2v2/5v5 are rated-only and need
    // a persistent ArenaTeam bots don't have, so skirmish == 3v3.
    constexpr uint8 kArenaType = 3;

    // Default arena BattlemasterList: All Arenas (6) — the canonical skirmish
    // template the live HandleBattlemasterJoinArena uses (it always queues
    // BATTLEGROUND_AA regardless of which specific arena map the match lands
    // on). Honor Playerbot.Bg.AutoSeed.ForceType if it names an arena BML so
    // the same override that pins BG seeds can pin a specific arena
    // (4=Nagrand, 5=Blade's Edge, 6=AllArenas, 8=Ruins, 10=Sewers, 11=RoV).
    uint16 arena_bml = 6;   // BATTLEGROUND_AA
    const int32 forced = sConfigMgr->GetIntDefault("Playerbot.Bg.AutoSeed.ForceType", 0, true);
    if (forced > 0)
    {
        if (BattlegroundTemplate const* ftmpl =
                sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(uint16(forced))))
        {
            if (ftmpl->IsArena())
                arena_bml = uint16(forced);
        }
    }

    // Validate the chosen template is a real arena before publishing.
    BattlegroundTemplate const* tmpl =
        sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(arena_bml));
    if (!tmpl || !tmpl->IsArena())
    {
        TC_LOG_INFO("playerbot.v2",
            "[ArenaSeed] skipped: arena_bml={} has no arena template", uint32(arena_bml));
        return;
    }

    // Concurrency / saturation guard. Count bots ALREADY sitting in an arena
    // queue (InBattlegroundQueue counts arena queues by default). One seeded
    // skirmish parks 2*kArenaType bots in queue; don't pile beyond the cap.
    uint32 arena_queued = 0;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        if (Player const* p = ObjectAccessor::FindConnectedPlayer(g))
            if (p->InBattlegroundQueue(/*ignoreArena*/ false) && !p->InBattleground())
                ++arena_queued;
    });
    const uint32 needed_per_skirmish = uint32(kArenaType) * 2u;
    if (arena_queued >= uint32(arena_cap) * needed_per_skirmish)
    {
        TC_LOG_INFO("playerbot.v2",
            "[ArenaSeed] skipped: {} bots already in BG/arena queue (cap={} skirmishes)",
            arena_queued, arena_cap);
        return;
    }

    // Eligible-idle-bot counter for a faction + level band. Mirrors
    // BotGroupBuilder's picker eligibility (online, in-world, alive, ungrouped,
    // in level band, not in combat, not deserter, not already queued) so we
    // don't fire a request the builder will then fail to satisfy.
    auto count_eligible = [&](uint8 faction_bit, uint8 lo, uint8 hi) -> uint32
    {
        uint32 n = 0;
        Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
        {
            Player* p = ObjectAccessor::FindConnectedPlayer(
                ObjectGuid::Create<HighGuid::Player>(id));
            if (!p || !p->IsInWorld() || !p->IsAlive() || p->GetGroup()) return;
            if (p->IsInCombat() || p->IsDeserter()) return;
            if (p->InBattleground() || p->InBattlegroundQueue(/*ignoreArena*/ false)) return;
            const uint8 lvl = p->GetLevel();
            if (lvl < lo || lvl > hi) return;
            // Race-based team to match BotGroupBuilder::PickCandidates'
            // faction filter (it uses Player::TeamForRace, not GetTeam()).
            const bool is_alliance =
                (Player::TeamForRace(p->GetRace()) == ALLIANCE);
            const uint8 bit = is_alliance ? 0x1 : 0x2;
            if (bit != faction_bit) return;
            ++n;
        });
        return n;
    };

    // ADAPTIVE bracket (2026-06-23): the fixed max-level band is empty in a
    // mostly-leveling fleet, so scan TC's 10-level PvP bands high->low and seed
    // the first where BOTH factions have >=kArenaType idle bots. Bands align to
    // TC bracket boundaries so a formed group shares one matchmaking bracket.
    // Midnight cap is 90: {90,90} is the max-level band, 80-89 a leveling band.
    struct Band { uint8 lo, hi; };
    static constexpr Band kBands[] = {
        {90,90},{80,89},{70,79},{60,69},{50,59},{40,49},{30,39},{20,29},{10,19}};
    uint8 bracket_min = 0, bracket_max = 0;
    uint32 alli_ok = 0, horde_ok = 0;
    for (Band const& b : kBands)
    {
        const uint32 a = count_eligible(0x1, b.lo, b.hi);
        const uint32 h = count_eligible(0x2, b.lo, b.hi);
        if (a >= kArenaType && h >= kArenaType)
        { bracket_min = b.lo; bracket_max = b.hi; alli_ok = a; horde_ok = h; break; }
    }
    if (bracket_min == 0)
    {
        TC_LOG_INFO("playerbot.v2",
            "[ArenaSeed] skipped: no level band has >={}/side idle bots (arena_bml={})",
            uint32(kArenaType), uint32(arena_bml));
        return;
    }

    // Publish one ArenaTeamForming per faction. BotGroupBuilder forms a
    // kArenaType-sized group for each and the leader queues it for the arena
    // skirmish via the Arena queue id. The two groups meet in the matchmaker.
    auto publish_side = [&](uint8 faction_bit, char const* label)
    {
        CoordEvent ev{};
        ev.kind         = CoordSignal::ArenaTeamForming;
        ev.origin_low   = 0;            // system-initiated
        ev.content_id   = arena_bml;    // arena BattlemasterList id
        ev.level_min    = bracket_min;
        ev.level_max    = bracket_max;
        ev.faction_mask = faction_bit;  // bit0=Alliance, bit1=Horde
        ev.arena_type   = kArenaType;
        ev.content_name = label;
        Services::Coordination().Publish(ev);
    };
    publish_side(0x1, "Arena skirmish (Alliance)");
    publish_side(0x2, "Arena skirmish (Horde)");

    TC_LOG_INFO("playerbot.v2",
        "[ArenaSeed] seeded {}v{} arena_bml={} bracket={}-{} (eligible alli={} horde={} queued={} cap={})",
        uint32(kArenaType), uint32(kArenaType), uint32(arena_bml),
        uint32(bracket_min), uint32(bracket_max), alli_ok, horde_ok, arena_queued, arena_cap);
}

// True if a BG contains at least one REAL human player (non-pool account).
// Bots ride on pool accounts; a human is anyone whose session account is not a
// pool account. Used to ensure the top-up only SUPPORTS human battlegrounds and
// never sustains a bot-only match (which is pure server load with no human
// benefit — owner directive 2026-06-17).
static bool BgHasHumanPlayer(Battleground const* bg)
{
    if (!bg) return false;
    for (auto const& kv : bg->GetPlayers())
    {
        Player const* p = ObjectAccessor::FindPlayer(kv.first);
        if (!p) continue;
        if (WorldSession const* sess = p->GetSession())
            if (!Services::Accounts().is_pool_account(sess->GetAccountId()))
                return true;
    }
    return false;
}

void BotPopulationManager::TopUpActiveBGs(uint32 now_ms)
{
    // Two cadences:
    //   - 5s for STATUS_WAIT_JOIN (prep phase) — fast fill before gates open.
    //   - 30s for STATUS_IN_PROGRESS — replace players that left mid-match
    //     without flooding the queue every 5s.
    // Each timer gates independently so we still hit the fast prep cadence
    // even if a running BG is also live.
    constexpr uint32 kPrepTopUpIntervalMs    = 5000;
    constexpr uint32 kRunningTopUpIntervalMs = 10000;

    bool const prep_due = !last_bg_topup_ms_ ||
        (now_ms - last_bg_topup_ms_) >= kPrepTopUpIntervalMs;
    bool const running_due = !last_bg_running_topup_ms_ ||
        (now_ms - last_bg_running_topup_ms_) >= kRunningTopUpIntervalMs;

    if (!prep_due && !running_due) return;
    if (prep_due)    last_bg_topup_ms_         = now_ms;
    if (running_due) last_bg_running_topup_ms_ = now_ms;

    // Walk BattlemasterListXMap (BattlemasterListID → MapID) → per-map
    // free-slot queue. The free-slot queue contains BG instances that
    // still have capacity (TC's existing FreeSlotQueue mechanism). For
    // each BG below max population in prep OR running, re-issue
    // BotQueueFiller::Fill. Newly-queued bots are auto-pulled into the
    // BG by AddToBGFreeSlotQueue when the next queue cycle runs.
    // Dedup by map_id so we don't process the same map's free-slot store
    // multiple times when several BattlemasterLists share a map.
    std::unordered_set<uint32> seen_maps;
    for (BattlemasterListXMapEntry const* xmap : sBattlemasterListXMapStore)
    {
        if (!xmap) continue;
        if (xmap->MapID <= 0) continue;
        uint32 map_id = uint32(xmap->MapID);
        if (!seen_maps.insert(map_id).second) continue;

        auto& free_slot_store = sBattlegroundMgr->GetBGFreeSlotQueueStore(map_id);
        for (Battleground* bg : free_slot_store)
        {
            if (!bg) continue;
            auto const status = bg->GetStatus();
            bool const is_prep    = (status == STATUS_WAIT_JOIN);
            bool const is_running = (status == STATUS_IN_PROGRESS);
            if (!is_prep && !is_running) continue;
            // Only SUSTAIN a RUNNING battleground that has a human in it. A bot-only running match is not refilled: it runs down and ends
            // on its own, so few instances stay alive and real players are put into the ones that are (owner directive 2026-06-17, confirmed
            // by playtest 2026-10-11: as few instances as possible, as many real players in each). PREP BGs are left ungated so a forming
            // human match still fills before its gates open (the human is invited but may not have entered m_Players yet).
            bool const has_human = BgHasHumanPlayer(bg);
            if (is_running && !has_human) continue;
            // A bot just left this type's running match to make room for a waiting human: no top-up until the human has taken the place.
            if (BotQueueFiller::RoomRecentlyMade(uint32(bg->GetTypeID()), now_ms)) continue;
            // Respect each phase's own cadence:
            // - Prep BGs only on prep_due ticks (every 5s)
            // - Running BGs only on running_due ticks (every 30s)
            if (is_prep    && !prep_due)    continue;
            if (is_running && !running_due) continue;
            uint32 const max_per_team = bg->GetMaxPlayersPerTeam();
            uint32 const alli  = bg->GetPlayersCountByTeam(ALLIANCE);
            uint32 const horde = bg->GetPlayersCountByTeam(HORDE);
            if (alli >= max_per_team && horde >= max_per_team) continue;

            uint8 const bracket = uint8(bg->GetBracketId());
            // PVPDifficulty bracket ID is NOT level/10. For AV bracket 0
            // corresponds to L51-60, etc. Derive an authoritative target
            // level from the bracket's MaxLevel so the filler matches the
            // right bot-level pool. Falls back to bot-level / 10 * 10 + 5
            // midpoint if the PVPDifficulty lookup fails (very unlikely).
            uint8 target_level_for_bracket = 0;
            uint8 bracket_min = 0, bracket_max = 0;
            if (PVPDifficultyEntry const* diff =
                    DB2Manager::GetBattlegroundBracketById(bg->GetMapId(),
                                                          BattlegroundBracketId(bracket)))
            {
                const uint8 max_level = MaxPlayerLevel();
                target_level_for_bracket = uint8(std::min<uint16>(diff->MaxLevel, max_level));
                bracket_min = uint8(std::clamp<int32>(diff->MinLevel, 1, max_level));
                bracket_max = uint8(std::clamp<int32>(diff->MaxLevel, 1, max_level));
            }
            BotQueueFiller filler;

            // online_only=true for PREP (5s cadence — pre-queued bots
            // already in-flight will arrive in time, no need to spawn
            // fresh ones). online_only=false for RUNNING (30s cadence,
            // BG is actively bleeding players; allow JIT-spawn so we
            // backfill from offline pool when the online pool is
            // exhausted). The 30s gate naturally rate-limits the JIT
            // storm to one batch per BG per cycle.
            //
            // max_total_bots = EXACTLY the deficit per faction. Without
            // this, Fill queued 36 candidates per side per cycle and
            // the team count overshot max_per_team — observed WSG with
            // 13 alliance vs 7 horde in a 10v10 (user-strict requirement
            // is "10v10 exactly, no overpopulation"). With the cap, we
            // inject exactly (max_per_team - current_count), the BG
            // never overshoots, and once full we naturally skip.
            // PREP used to be online_only: only bots that happened to be online and free filled it. With a 1/10 team and ~117 s of
            // countdown left (live 2026-10-08, 20:23: pool rejects grouped=66 combat=9, nothing queued) the room stayed empty. A prep BG
            // may JIT-spawn the deficit too, but only once per 20 s per BG (was 45 s: a 2 minute prep got two chances, AB 2026-10-11 started 6 v 2): JIT setups take a while to come online and queue, and the
            // filler cannot see setups in flight, so a fresh batch on every 5 s tick would overshoot the team size.
            bool prep_jit_due = false;
            if (is_prep)
            {
                uint32& last_jit = prep_jit_ms_[bg->GetInstanceID()];
                if (!last_jit || (now_ms - last_jit) >= 20000u)
                {
                    last_jit = now_ms ? now_ms : 1u;
                    prep_jit_due = true;
                }
            }
            bool const allow_jit = is_running || prep_jit_due;
            // GetFreeSlotsForTeam respects TC's balance gate (the BG won't
            // accept new alliance bots when alliance is over cap or
            // alliance.invited > horde.invited). Without this check we'd
            // keep pushing bots into TC's queue that can never get pulled
            // in — wasted queue churn and confusing diagnostic output.
            // Observed 2026-05-13 07:40: BG running at alli=13/10
            // horde=7/10 with filler still queueing 3 alli tanks/cycle
            // because alli<max_per_team computed off m_PlayersCount which
            // had drifted from the actual cap-enforced view.
            uint32 const alli_free_tc  = bg->GetFreeSlotsForTeam(ALLIANCE);
            uint32 const horde_free_tc = bg->GetFreeSlotsForTeam(HORDE);
            if (alli < max_per_team && (alli_free_tc > 0 || horde < max_per_team))
            {
                BotQueueFiller::FillRequest req{};
                req.kind = BotQueueFiller::QueueKind::Bg;
                req.bracket = bracket;
                req.faction = ALLIANCE;
                req.instance_id = uint32(bg->GetTypeID());
                req.requesting_player = nullptr;
                req.online_only = !allow_jit;
                req.max_total_bots = uint8(Battleground::TopUpRequest(alli, max_per_team));
                req.single_faction = true;
                if (target_level_for_bracket > 0)
                    req.target_level_override = target_level_for_bracket;
                if (bracket_min > 0 && bracket_max > 0)
                {
                    req.bracket_min_level = bracket_min;
                    req.bracket_max_level = bracket_max;
                }
                filler.Fill(req);
            }
            if (horde < max_per_team && (horde_free_tc > 0 || alli < max_per_team))
            {
                BotQueueFiller::FillRequest req{};
                req.kind = BotQueueFiller::QueueKind::Bg;
                req.bracket = bracket;
                req.faction = HORDE;
                req.instance_id = uint32(bg->GetTypeID());
                req.requesting_player = nullptr;
                req.online_only = !allow_jit;
                req.max_total_bots = uint8(Battleground::TopUpRequest(horde, max_per_team));
                req.single_faction = true;
                if (target_level_for_bracket > 0)
                    req.target_level_override = target_level_for_bracket;
                if (bracket_min > 0 && bracket_max > 0)
                {
                    req.bracket_min_level = bracket_min;
                    req.bracket_max_level = bracket_max;
                }
                filler.Fill(req);
            }
            // Diagnostic: surface the team-balance state alongside the
            // top-up activity so an imbalanced BG is visible in the log.
            TC_LOG_INFO("playerbot.v2",
                "[BgTopUp] bg_inst={} bg_type={} free_slots alli={} horde={} (m_PlayersCount alli={} horde={}, "
                "invited alli={} horde={}, waiting in queue alli={} horde={})",
                bg->GetInstanceID(), uint32(bg->GetTypeID()), alli_free_tc, horde_free_tc, alli, horde,
                bg->GetInvitedCount(ALLIANCE), bg->GetInvitedCount(HORDE),
                sBattlegroundMgr->GetBattlegroundQueue(BattlegroundMgr::BGQueueTypeId(uint16(bg->GetTypeID()), BattlegroundQueueIdType::Battleground,
                    false, 0)).GetQueuedPlayersCount(TEAM_ALLIANCE, BattlegroundBracketId(bracket)),
                sBattlegroundMgr->GetBattlegroundQueue(BattlegroundMgr::BGQueueTypeId(uint16(bg->GetTypeID()), BattlegroundQueueIdType::Battleground,
                    false, 0)).GetQueuedPlayersCount(TEAM_HORDE, BattlegroundBracketId(bracket)));

            TC_LOG_INFO("playerbot.v2",
                "[BgTopUp] bg_inst={} phase={} humans={} jit={} bg_type={} bracket={} alli={}/{} horde={}/{} delay={}ms asked alli={} horde={}",
                bg->GetInstanceID(), is_running ? "RUNNING" : "PREP", has_human ? 1 : 0, allow_jit ? 1 : 0,
                uint32(bg->GetTypeID()), uint32(bracket),
                alli, max_per_team, horde, max_per_team,
                bg->GetStartDelayTime(),
                (alli < max_per_team && (alli_free_tc > 0 || horde < max_per_team)) ? Battleground::TopUpRequest(alli, max_per_team) : 0u,
                (horde < max_per_team && (horde_free_tc > 0 || alli < max_per_team)) ? Battleground::TopUpRequest(horde, max_per_team) : 0u);
        }
    }
}

void BotPopulationManager::RunRebalance(uint32 now_ms)
{
    if (last_rebalance_ms_ && (now_ms - last_rebalance_ms_) < kRebalanceIntervalMs)
        return;
    last_rebalance_ms_ = now_ms;

    if (!Services::Initialized()) return;

    // Group online bots by (faction_alliance, bracket=level/10).
    struct BracketCounts
    {
        std::vector<uint64> tank_ids;   // already in tank spec
        std::vector<uint64> heal_ids;   // already in heal spec
        std::vector<uint64> dps_ids;    // candidates for switch (in DPS spec)
    };
    std::unordered_map<uint32 /*bracket_key*/, BracketCounts> by_bracket;

    auto& id_reg = Services::Lifecycle();   // walk online ids
    auto& ai_reg = Services::Registry();    // ai(id) for ActionKind lockout
    auto ids = id_reg.snapshot_ids();
    for (auto id : ids)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        if (!bot) continue;
        // An owner's companion keeps the role its owner gave it: never drafted into another spec by the fleet balance.
        if (Services::Owners().GetOwner(id).account_id != 0 && !IsPlayersOwnCharacter(bot)) continue;
        // Eligibility filters mirror BotQueueFiller's pass-2 rules so
        // rebalance and reactive switching pull from the same eligible set.
        if (bot->IsInCombat()) continue;
        if (bot->GetGroup()) continue;
        if (bot->GetMap() && bot->GetMap()->IsDungeon()) continue;
        // NOTE on altbots: deliberately NOT excluded here. Owner decision
        // 2026-06-10: while an alt serves as a bot the bot system manages
        // its spec (talents/respec) so it stays combat-functional; the
        // player can respec when they log it in personally. Alts ARE
        // excluded from kicks and queue-drafting (those remove the
        // character from the owner's control — different category).

        const uint8 cls = bot->GetClass();
        const uint8 lvl = bot->GetLevel();
        if (lvl < 10) continue;  // pre-spec characters
        const uint32 bracket = lvl / 10u;
        const bool alliance = bot->GetTeam() == ALLIANCE;
        // Pack (faction, bracket) into one key. Bracket is 1..8 so 4 bits suffice.
        const uint32 key = (alliance ? 0x10000u : 0u) | bracket;

        const uint16 spec = uint16(::Playerbot::Classic::EffectiveSpecId(bot));
        auto& bc = by_bracket[key];
        if (IsTankSpec(cls, spec))        bc.tank_ids.push_back(id);
        else if (IsHealerSpec(cls, spec)) bc.heal_ids.push_back(id);
        else                              bc.dps_ids.push_back(id);
    }

    // Target ratios: 1T:1H:3D per group of 5. Equivalently, want
    // tank_count >= total/5 and heal_count >= total/5 in each bracket.
    // If the bracket is too small for meaningful role coverage (< 5
    // bots), skip — JIT will fill any /lfg there and rebalance churn
    // would be more disruptive than helpful.
    uint32 switched_total = 0;
    for (auto& [key, bc] : by_bracket)
    {
        const size_t total = bc.tank_ids.size() + bc.heal_ids.size() + bc.dps_ids.size();
        if (total < 5) continue;
        const size_t target_each = (total + 4) / 5;  // ceil(total/5)

        auto try_switch_one = [&](Role role) -> bool
        {
            // #4A: prefer the candidate whose ARCHETYPE most wants this role.
            // Rather than respec the first eligible DPS bot (arbitrary), score
            // every eligible candidate by its archetype role_affinity for the
            // target role and switch the highest-affinity one. A HardcoreRaider
            // (high tank/heal affinity) is drafted before a GathererFlipper
            // (pure-DPS affinity), so the fleet's tanks/healers are the bots
            // that "want" to play those roles — fewer reluctant respecs and a
            // more believable role distribution. Eligibility (combat / group /
            // dungeon / lockout / class-can-fill) is unchanged.
            const uint8 role_idx = (role == Role::Tank) ? 0u : 1u;  // role_affinity[Tank,Healer,Dps]

            auto best_it    = bc.dps_ids.end();
            float best_aff  = -1.0f;
            uint32 best_spec = 0;
            for (auto it = bc.dps_ids.begin(); it != bc.dps_ids.end(); ++it)
            {
                BotAI* ai = ai_reg.ai(*it);
                if (!ai) continue;
                const uint64 bot_id = *it;
                ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(bot_id);
                Player* bot = ObjectAccessor::FindConnectedPlayer(g);
                if (!bot) continue;
                // Owner-pinned role check is handled by idle:dual_spec_switch
                // on the bot's own next tick — that rule reads role_override_
                // and switches to the right spec independently. Rebalance
                // doesn't need to read role_override directly; if the owner
                // pinned Tank but rebalance switches to Healer, idle's rule
                // will flip back on the bot's next idle observation, paying
                // the per-bot lockout cost. Net effect: rebalance produces a
                // small amount of churn for owner-pinned bots, capped at one
                // flip per 60min via the lockout. Acceptable.
                // Per-bot lockout via DualSpec ActionKind. Key on
                // `bot_id` so each bot has its own lockout — keying on
                // class would lock out every Warrior for 30min after
                // one Warrior is switched, which is the opposite of
                // what we want for a fleet-wide rebalance.
                if (ai->action_recently_tried(BotAI::ActionKind::DualSpec,
                                              bot_id, now_ms))
                    continue;
                const uint8 cls = bot->GetClass();
                const uint32 target_spec = (role == Role::Tank)
                    ? TankSpecForClass(cls)
                    : HealerSpecForClass(cls);
                if (target_spec == 0) continue;  // class can't fill role
                auto const* spec_entry = sChrSpecializationStore.LookupEntry(target_spec);
                if (!spec_entry) continue;
                const float aff = ai->archetype().role_affinity[role_idx];
                if (aff > best_aff)
                {
                    best_aff  = aff;
                    best_it   = it;
                    best_spec = target_spec;
                }
            }

            if (best_it == bc.dps_ids.end())
                return false;   // no eligible candidate this bracket/role

            const uint64 bot_id = *best_it;
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(bot_id);
            Player* bot = ObjectAccessor::FindConnectedPlayer(g);
            BotAI* ai = ai_reg.ai(bot_id);
            if (!bot || !ai) return false;   // raced away between scan and apply
            const uint8 cls = bot->GetClass();
            auto const* spec_entry = sChrSpecializationStore.LookupEntry(best_spec);
            if (!spec_entry) return false;
            bot->ActivateTalentGroup(spec_entry);
            ai->note_action_retry(BotAI::ActionKind::DualSpec, bot_id, now_ms);
            bc.dps_ids.erase(best_it);   // remove from candidate pool
            if (role == Role::Tank) bc.tank_ids.push_back(bot_id);
            else                    bc.heal_ids.push_back(bot_id);
            ++switched_total;
            TC_LOG_INFO("playerbot.v2",
                "[Rebalance] bracket=0x{:x} switched bot {} ({}) to spec {} ({}) affinity={:.2f}",
                key, bot_id, uint32(cls), best_spec,
                role == Role::Tank ? "tank" : "healer", best_aff);
            return true;
        };

        uint32 switches_in_bracket = 0;
        if (bc.tank_ids.size() < target_each &&
            switches_in_bracket < kRebalanceSwitchesPerBracket)
        {
            if (try_switch_one(Role::Tank)) ++switches_in_bracket;
        }
        if (bc.heal_ids.size() < target_each &&
            switches_in_bracket < kRebalanceSwitchesPerBracket)
        {
            if (try_switch_one(Role::Healer)) ++switches_in_bracket;
        }
    }

    if (switched_total > 0)
    {
        TC_LOG_INFO("playerbot.v2",
            "[Rebalance] cycle complete: {} switches across {} brackets",
            switched_total, by_bracket.size());
    }
}

uint32 BotPopulationManager::ForceRebalance()
{
    // Reset the throttle so RunRebalance fires this call. RunRebalance
    // sets last_rebalance_ms_ on entry; we need to walk it backwards
    // first. We can't easily measure "switches" without restructuring
    // the function — log line counts ('[Rebalance] cycle complete: N')
    // give the operator the answer. This helper exists mostly to
    // expose the bypass to /rebalance whisper.
    last_rebalance_ms_ = 0;
    RunRebalance(getMSTime());
    return 0;
}

std::vector<BotPopulationManager::BracketReport> BotPopulationManager::BracketCoverage() const
{
    std::unordered_map<uint32, BracketReport> by_bracket;
    if (!Services::Initialized()) return {};

    auto& id_reg = Services::Lifecycle();
    auto ids = id_reg.snapshot_ids();
    for (auto id : ids)
    {
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        if (!bot) continue;
        const uint8 lvl = bot->GetLevel();
        if (lvl < 10) continue;
        const uint8 cls = bot->GetClass();
        const uint16 spec = uint16(::Playerbot::Classic::EffectiveSpecId(bot));
        const bool alliance = bot->GetTeam() == ALLIANCE;
        const uint32 key = (alliance ? 0x10000u : 0u) | uint32(lvl / 10u);
        auto& br = by_bracket[key];
        br.bracket_key = key;
        if (IsTankSpec(cls, spec))        ++br.tanks;
        else if (IsHealerSpec(cls, spec)) ++br.healers;
        else                              ++br.dps;
    }
    std::vector<BracketReport> out;
    out.reserve(by_bracket.size());
    for (auto& [k, v] : by_bracket) out.push_back(v);
    // Sort by bracket key so the whisper output reads naturally
    // (alliance / horde grouped via high bit, then bracket low->high).
    std::sort(out.begin(), out.end(),
              [](BracketReport const& a, BracketReport const& b) {
                  return a.bracket_key < b.bracket_key;
              });
    return out;
}

} // namespace Playerbot::V2::Fleet
