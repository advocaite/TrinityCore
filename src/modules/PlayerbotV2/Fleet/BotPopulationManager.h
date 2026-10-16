// BotPopulationManager - Distribution shaper that keeps the world stocked
// with bots at every level bracket per a configured shape. Phase A of
// WORLD_POPULATION_PLAN.md.
//
// Ticks every 60s from Module::OnWorldUpdate (configurable). Each tick:
//   1. Snapshot current online bot counts per (level_bucket, faction).
//   2. Compute target counts from config (Total / Shape / AllianceRatio).
//   3. Reconcile: prefer login of offline bots that fit; else JIT-create
//      via BotCharacterFactory; else logout LRU when over-target.
//   4. Throttle: max SpawnRatePerTick creations + LoginRatePerTick logins.
//
// Idempotence: bots with distribution_level > 0 keep their level. The
// shaper only ever raises a bot's distribution level, never lowers it.

#pragma once

#include "Bot/BotTypes.h"
#include "AsyncCallbackProcessor.h"   // QueryCallbackProcessor member (async DB reads)
#include "DatabaseEnvFwd.h"           // QueryCallbackProcessor / QueryResult aliases
#include "WorldLoadFacts.h"
#include "BotQueueFiller.h"
#include <unordered_map>
#include <mutex>
#include <unordered_set>
#include <vector>
#include <string>

class Player;

namespace Playerbot::V2::Fleet {

class BotSetupPipeline;
class WorldBotSpawner;

enum class PopulationShape : uint8 { MaxHeavy, Pyramid, Bell, Flat };

struct PopulationBucket
{
    uint8  level_lo;
    uint8  level_hi;
    uint32 alliance_target;
    uint32 horde_target;
    uint32 alliance_actual = 0;
    uint32 horde_actual    = 0;
};

struct PopulationSnapshot
{
    uint32 total_target = 0;
    uint32 total_actual = 0;
    std::vector<PopulationBucket> buckets;
};

class BotPopulationManager
{
public:
    BotPopulationManager();
    ~BotPopulationManager();

    // Tick driver. Called from Module::OnWorldUpdate. Internally rate-limits
    // to TickIntervalSec (default 60) so per-frame calls are cheap.
    void OnWorldTick(uint32 now_ms);

    // Force an immediate reconciliation regardless of the tick gate. Used
    // by .playerbot population force / restart handlers.
    void ForceReconcile();

    // Snapshot current state for diagnostics. Computes target on demand.
    PopulationSnapshot Snapshot() const;

    // Diagnostic accessors for /health. All return last-tick values; safe
    // to read from the world thread without locks (writers are also the
    // world thread). The "ms_until_next_*" helpers return 0 if the gate
    // has already elapsed (next call will fire immediately).
    uint32 spawn_budget() const     { return spawn_budget_alli_ + spawn_budget_horde_; }
    uint32 login_budget() const     { return login_budget_alli_ + login_budget_horde_; }
    uint32 last_tick_ms() const     { return last_tick_ms_; }
    uint32 last_hygiene_ms() const  { return last_hygiene_ms_; }
    // Wall-clock ms remaining until the next reconcile / hygiene fires.
    uint32 ms_until_next_reconcile(uint32 now_ms) const;
    uint32 ms_until_next_hygiene(uint32 now_ms) const;
    uint32 ms_until_next_rebalance(uint32 now_ms) const;
    uint32 last_rebalance_ms() const { return last_rebalance_ms_; }
    // Marker count of bots whose RunFor returned true since process start.
    size_t setup_done_cache_size() const { return setup_done_cache_.size(); }

    // Drop a guid from both pipeline caches. Call this wherever a bot character
    // stops existing (hygiene delete, corpse sweep): every other use of these
    // two containers inserts or looks up, so without an erase they only ever
    // grow, and a recycled guid would be served a stale pipeline row.
    void forget_pipeline_caches(uint64 guid);

    // Run the per-bot setup pipeline for any in-world bot whose
    // distribution_level > 0 but setup_pipeline_state != AllDone. Cheap
    // when nothing pending. Called every tick alongside reconcile.
    void DriveSetupPipelines();

    // Priority-setup register. Bots in this set are drained from
    // DriveSetupPipelines FIRST and without the per-tick 10-bot cap.
    // Used by `.playerbot smoketest`: under a 1000+ bot population the
    // normal cap starves freshly-spawned smoketest bots indefinitely.
    // Auto-removes entries when their bot reaches AllDone or when
    // explicitly unregistered (e.g. test cleanup). Caller responsible
    // for unregistering on abort. Thread-safe — guarded by priority_mtx_.
    void RegisterPrioritySetup(uint64 bot_id);
    void UnregisterPrioritySetup(uint64 bot_id);

    // Hygiene cleanup (Phase E of WORLD_POPULATION_PLAN):
    //   - Delete JIT-spawned bots that haven't been used in N days.
    //   - Hard-cap protection: log out everyone if total online > 2x target.
    // Runs at most every kHygieneIntervalMs (default 1h).
    void RunHygiene(uint32 now_ms);

    // Re-gear backfill for under-geared / weaponless ONLINE bots. Separate from
    // the 1h hygiene so it runs SOON after login (the 1h pass fires once at boot
    // before the fleet is online, wasting it) and drains the historical
    // under-geared backlog at 25 bots / 5 min. Reuses the gear generator + the
    // SAFE SwapItem equip path. 2026-06-17.
    void RunGearBackfill(uint32 now_ms);

    // Runs one bot through the same gear pass right away, ignoring the 5-min cadence and the failed-pass cooldown.
    // Used after `.playerbot level`, which otherwise leaves a freshly levelled bot in its old gear until the next
    // pass. World thread only. Returns true if the bot qualified for re-gearing.
    bool RegearNow(Player* p);

    // The damage_parity scenarios (Diagnostics/DamageParity.cpp): the gear mode the bot is geared with, whoever owns it (Gear::Owned::GearMode
    // value; -1 clears the override). A scenario bot has no owner account to choose t1 / t3 for it. Thread safe.
    static void SetGearModeOverride(BotId bot, int mode);

    // An owner's choice of gear for the companions it owns (Gear::Owned::GearMode value; `gear t3` to a bot, `.bot gear t3`, the
    // panel). Refused, with the reason in `reply`, when the server does not allow the mode (Playerbot.OwnedGear.AllowedModes).
    // Otherwise stored for the account (kept across restarts) and every companion of the owner is regeared at the next frame
    // (RequestGearRefresh). Thread safe. `reply` is the line to tell the owner.
    bool SetOwnerGearMode(uint32 account_id, uint8 requested_mode, std::string& reply);
    // Queues a companion for a gear pass at the next frame (ignoring the cooldown); a bot in combat or dead is left to the 5-minute
    // sweep, which notices the changed refresh key. Thread safe.
    void RequestGearRefresh(BotId bot);

    // A full re-roll of a companion's equipment with the per-bot varied generator (`.bot regear`, and once for every
    // companion geared before variety existed: OwnerRegistry::GearVaried, marked here on success). Unlike the
    // re-gear pass, which only fills under-geared slots, every generated pick goes on unless the worn piece is
    // better: a higher quality, or the same quality more than Gear::kVarietyIlvlBand effective item levels above it
    // (real loot and owner-given gear stay). Replaced pieces go to the bags; a slot whose piece cannot go to the bags
    // is left as it is. A player's own character is never touched. World thread only.
    enum class RerollResult : uint8 { Done, NotNow, NotACompanion };   // NotNow: in combat or dead
    RerollResult RerollCompanionGear(Player* p, uint32& changed_slots);

    // One-step companion (`.bot add`). Once the newly created bot is in the world: level it to its owner, teach its
    // class trainer spells, set the requested role (a Role value; Role::Unknown keeps the class default), apply
    // talents and gear, bring it to the owner and put it in the owner's group (made a raid when full). Dropped if the
    // bot does not log in within a minute or the owner is offline by then. World thread only.
    void QueueCompanionSetup(BotId bot, uint64 owner_guid_low, uint8 role);

    // Resolves and pins a companion's role: `requested` (the owner's word) if not Role::Unknown, else the role stored with
    // the companion (OwnerRegistry::DeclaredRole, migration 0024), else the one its talents say. Sets the BotAI role
    // override and the Classic virtual spec (what tank/healer readers, the talent build and the gear generator follow) and
    // stores the role. Logs "[role] <bot> role=<r> source=<declared|spec|fallback>" (at setup/call/login always, from the
    // upkeep only when it had to put something back). A class that cannot fill the role plays DPS (source=fallback).
    // Returns the role pinned. World thread only.
    static Role ApplyCompanionRole(Player* bot, Role requested, char const* where);

    // `.bot save`: stores the role the companion plays right now (its bot logic role, else what its talents say) when none is stored yet, so a
    // call or relog brings it back as it was saved instead of re-reading talents that may have changed or not been bought. A role
    // already stored (declared) is left as it is. Returns the role stored. World thread only.
    static Role StoreCurrentRole(Player* bot);

    // `.bot delete`: the companion's character is deleted for good once it has left the world (the caller logs it
    // out first). Given up, with a log line, if it is still in the world a minute later. World thread only.
    void QueueCompanionDelete(BotId bot);

    // One bot's step of RunGearBackfill (also RegearNow). Increments regear_count when the bot is processed.
    void BackfillOne(BotId id, Player* p, uint32 now_ms, uint32& regear_count, bool ignore_cooldown);
    // An owner's companion never walks to a trainer, a weapon master or a repair vendor (world errands it skips), so
    // each RunGearBackfill pass teaches it its new ranks, class-quest abilities and talents, and mends its gear. Runs
    // for every companion, whatever its gear. Cheap; no-op for any other bot.
    void CompanionUpkeep(BotId id, Player* p);
    // A hunter companion's starter pet, granted once per bot (OwnerRegistry::StarterPetGranted).
    void GrantCompanionStarterPet(BotId id, Player* p);

    struct PendingCompanion
    {
        uint64 owner_guid_low = 0;
        uint8  role = 0;
        uint32 queued_ms = 0;
    };
    std::unordered_map<uint64, PendingCompanion> pending_companions_;
    void DrainCompanionSetups();
    void FinishCompanionSetup(Player* bot, Player* owner, uint8 role);
    std::unordered_map<uint64, uint32> pending_deletes_;   // bot -> queued at (getMSTime)
    void DrainCompanionDeletes();

    // Bots found wearing unrenderable gear while in combat, awaiting an
    // out-of-combat moment. Small by construction: only bots that actually have
    // a bad item AND are mid-fight. Entries are dropped when the bot logs out
    // (the login hook heals it) or once the heal succeeds.
    std::unordered_set<uint64> pending_gear_heal_;
    uint32 last_gear_heal_drain_ms_ = 0;

    // Removes equipped items the CLIENT CANNOT DRAW (no ItemModifiedAppearance;
    // see ::Playerbot::IsItemRenderableInSlot) from an online bot. Such an item
    // crashes every client that inspects or renders the bot, so this is keyed on
    // the item alone - never on whether the bot counts as under-geared, which a
    // bot in a full high-ilvl unrenderable set does not. Runs at bot login and
    // for every online bot on each gear-backfill pass. World thread only.
    // Returns true if any slot changed.
    //
    // A bot that is IN COMBAT cannot have armor swapped, so it is queued in
    // pending_gear_heal_ and retried by DrainPendingGearHeals() as soon as it
    // drops out of combat.
    bool HealUnrenderableGear(Player* p);

    // Retries the bots queued by HealUnrenderableGear() while they were in
    // combat. Called from OnWorldTick on a short cadence: waiting for the 5-min
    // backfill pass meant a bot that fights through every pass kept its
    // client-crashing gear indefinitely (test box, 2026-09-16: 7 online
    // levellers never healed). World thread only.
    void DrainPendingGearHeals(uint32 now_ms);

    // Failed-JIT corpse reclaim (500/pass). Called from RunHygiene and on
    // a 10-min OnWorldTick cadence while the backlog drains.
    void SweepFailedJitCorpses();

    // Kick-protection lease. JIT match-fill bots (BotQueueFiller spawn_jit)
    // ride ABOVE TotalTarget by design, but between their deferred login
    // and the moment the setup pipeline fires the queue intent they look
    // like ordinary overshoot -- no group, no BG queue, normal map -- and
    // the Reconcile overflow kick logged them out before they could ever
    // queue (observed live: [QueueFill] online_seen stuck at target while
    // spawn_jit succeeded every cycle). The filler leases protection at
    // spawn; every kick path checks it. World-thread only (no lock).
    void ProtectFromKick(uint64 guid_low, uint32 duration_ms);
    bool IsKickProtected(uint64 guid_low, uint32 now_ms) const;

    // Operator-created persistent bots (distribution_level == 0) are dev/test
    // subjects spawned via `.playerbot create`, not ambient population. The
    // reconciler never refills them (its candidate query filters
    // distribution_level > 0) and the setup pipeline stamps a positive level
    // on every shaped/ambient bot, so dist_level == 0 uniquely identifies an
    // operator bot. They must stay online for the life of the process — every
    // ambient logout path (session-rhythm, overflow LRU, hygiene hard-cap)
    // exempts them. dist_level is read from pipeline_row_cache_, which
    // DriveSetupPipelines fills for every in-world bot within a second or two of
    // login (1 Hz drive pass plus one async round-trip);
    // an as-yet-uncached bot returns false (not exempt), but that
    // window is far shorter than any session/overflow eligibility horizon.
    bool IsOperatorPersistentBot(uint64 guid_low) const;

    // ---- World bot spawner support (Fleet/WorldBotSpawner) ----
    // True once the bot's setup pipeline has finished (level, gear, talents, first placement).
    bool SetupDone(uint64 guid_low) const { return setup_done_cache_.contains(guid_low); }
    // True for an ambient pool bot: a positive distribution level and not a player's own alt or companion.
    bool IsAmbientBot(BotId id, Player const* p) const;
    // Creates one new pool character at `level` and queues its login, outside the reconcile budgets and without the
    // TotalTarget-based pool cap (the world spawner enforces its own). Returns the guid counter, 0 on failure.
    // `race` (0 = the weighted roll) fixes the race: Classic 1.60 Skyborne residents of Zephras Isle.
    uint64 SpawnAmbientBot(uint8 level, bool alliance, uint8 race = 0);
    // Classic 1.60: places a Skyborne bot that outgrew Zephras Isle in a vanilla leveling town (BotSetupPipeline::Graduate).
    bool GraduateSkyborne(Player* bot);
    // True when ambient (non-companion) bots are wanted at all: the shaper (TotalTarget > 0) or the world spawner
    // (Playerbot.World.Enable). Gate for the BG / LFG queue filling and the BG / arena seeding, which JIT-create bots.
    static bool AmbientFleetEnabled();
    // One-line world spawner status (`.bot world`).
    std::string WorldReport() const;
    WorldLoadFacts WorldLoad() const;
    // True for a bot the world spawner placed: BG / LFG queue fills leave these where they are.
    bool IsWorldPlaced(BotId id) const;
    bool WorldSpawnerPending(uint64 guid_low) const;

    // Defer a headless login to the next world tick(s). REQUIRED for any
    // login that immediately follows BotCharacterFactory::Create: the
    // factory's SaveToDB commits asynchronously, and a same-tick LoginBot
    // races the DB worker -- Player::LoadFromDB fails and the session is
    // kicked ("WorldSession::HandlePlayerLogin Player::LoadFromDB failed",
    // observed live: 542 JIT chars created / 1 ever entered world).
    // OnWorldTick drains kLoginDrainPerTick per tick.
    void DeferLogin(uint64 guid_low) { deferred_logins_.push_back(guid_low); }

    // BG prep-phase top-up. Walks all known BG maps via BattlemasterList,
    // finds BGs in STATUS_WAIT_JOIN with free slots, and re-issues
    // BotQueueFiller::Fill for each so additional bots join the queue and
    // get auto-pulled into the BG by the FreeSlotQueue mechanism. Keeps
    // running until the BG is fully populated (max players per team) or
    // gates open (STATUS_IN_PROGRESS). Rate-limited to once per 5s.
    void TopUpActiveBGs(uint32 now_ms);
    // World / pool / BG bots (no owner) wearing gear above the world cap (Playerbot.World.Gear.*) are regeared at once, not on the 5 minute sweep.
    void ScrubWorldGear(uint32 now_ms);
    // Owner-less bots in a battleground must not sit in a human's party / raid (their combat greys out the human's Release Spirit).
    void BgGroupHygiene(uint32 now_ms);

    // Autonomous BG seeding (audit B24): with no real player ever queueing,
    // OnPlayerJoinedBgQueue never fires, BotQueueFiller never runs, and the
    // entire BG subsystem (13 scripts, idle rules, outcome learning) sits
    // dormant — wins=0/losses=0 forever. Every few minutes, when no BG is
    // active, seed one bot-vs-bot match by Fill()ing both factions of a
    // rotating bg_type at the max-level bracket. Gated by
    // Playerbot.Bg.AutoSeed.Matches in playerbot.conf (0 disables).
    void SeedBgMatches(uint32 now_ms);
    // The one battleground fill request (max-level bracket, per-team cap from the template, bracket bounds from DB2). The
    // autonomous seed and the stress test build it here, and the human queue hook fills the same way, so every BG fill
    // ends in BotQueueFiller::Fill with the same shape.
    static BotQueueFiller::FillRequest BuildAmbientBgFill(uint16 bg_type);

public:
    // `.bot bgstress <bg_type> <cycles> [hold_sec]`: repeated fill / hold / leave cycles of one battleground type to find
    // what the fill path costs at scale (100+ bots queued and released again and again). One summary line per cycle with
    // queued, in-battleground and memory numbers; slow calls show up as [slow_call]. Returns a status line.
    std::string StartBgStress(uint16 bg_type, uint32 cycles, uint32 hold_sec);
    std::string BgStressReport() const;

    // `.bot bgsoak <bg_type> [max_minutes]`: plays ONE bot-only battleground of that type (1 = Alterac Valley, 2 = Warsong Gulch, 3 = Arathi Basin)
    // from the fill to the end of the match, or for max_minutes (default 30) and then lets the bots leave. One [bgsoak] line a minute
    // and a final "[bgsoak] done ... result=ended|timeout|never_started". scripts\run-bg-soak.ps1 drives it over SOAP and adds the
    // server-side fault count (the Map::Update SEH lines of Server.log) and whether the worldserver is still up.
    std::string StartBgSoak(uint16 bg_type, uint32 max_minutes);
    std::string BgSoakReport() const;

    // Autonomous ARENA seeding. Same rationale as SeedBgMatches but for
    // arena skirmishes, which the BG matchmaker (BotQueueFiller) cannot
    // drive because arenas queue as a GROUP keyed by team size, not as
    // independent solo queuers. Every few minutes, when arena seeding is
    // enabled, publish one ArenaTeamForming CoordEvent per faction;
    // BotGroupBuilder forms an arena_type-sized group per side and the
    // leader queues it via the Arena queue id. Gated by
    // Playerbot.Bg.AutoSeed.Arena in playerbot.conf (0 = off, default).
    // Honors Playerbot.Bg.AutoSeed.ForceType when that id is an arena BML.
    void SeedArenaMatches(uint32 now_ms);

    // Fleet rebalance pass (proactive spec rotation): walks online bots
    // grouped by (faction, bracket), counts tank/healer/DPS, and
    // proactively switches under-rep brackets' hybrid DPS bots into
    // tank/healer specs. Reduces JIT-spawn pressure during peak queue
    // hours by getting the fleet's spec mix right BEFORE
    // BotQueueFiller::Fill demands it. Per-bot 60min cooldown + 1
    // switch per tick globally so a fleet of 2000 doesn't experience a
    // mass-respec spike. Runs at most every kRebalanceIntervalMs.
    void RunRebalance(uint32 now_ms);

    // Bypass the 5-min throttle and run a rebalance pass right now.
    // Used by the `/rebalance` diagnostic whisper to verify the cron's
    // behaviour without waiting for the next natural cycle. Returns
    // the number of switches applied.
    uint32 ForceRebalance();

    // Live per-bracket spec coverage report. Walks online bots and
    // counts tank/heal/dps per (faction, level-bracket). Used by the
    // /popstats whisper so operators can verify rebalance is producing
    // the 1:1:3 mix and spot brackets that are tank/heal-starved.
    struct BracketReport
    {
        uint32 bracket_key  = 0;   // (alliance ? 0x10000 : 0) | (level/10)
        uint16 tanks        = 0;
        uint16 healers      = 0;
        uint16 dps          = 0;
    };
    std::vector<BracketReport> BracketCoverage() const;

private:
    // state of the running `.bot bgstress` (see StartBgStress)
    void DriveBgStress(uint32 now_ms);
    void DriveBgSoak(uint32 now_ms);
    struct BgSoak
    {
        enum class Phase : uint8 { Idle, Running, Done } phase = Phase::Idle;
        uint16 bg_type = 0;
        uint32 started_ms = 0, deadline_ms = 0, last_log_ms = 0, last_poll_ms = 0, first_in_bg_ms = 0, peak_in_bg = 0;
        uint8 last_status = 0;
        std::string result;            // the final line once Done
    } bg_soak_;
    struct BgStress { uint16 bg_type = 0; uint32 cycles_left = 0, cycle = 0, hold_ms = 60000, phase_end_ms = 0;
                      enum class Phase : uint8 { Idle, Hold, Rest } phase = Phase::Idle; uint32 peak_queued = 0, peak_in_bg = 0; } bg_stress_;

    // guid_low -> getMSTime() expiry of the kick-protection lease.
    std::unordered_map<uint64, uint32> kick_protect_until_ms_;

    // #5 session-rhythm: guid_low -> getMSTime() the bot's CURRENT play
    // session began. Stamped lazily on first Reconcile sighting (staggered so
    // the boot cohort doesn't all expire together) and erased on session-end
    // logout so the next login gets a fresh budget. World-thread only (only
    // Reconcile touches it) -> no lock. See SessionRhythmLogout.
    std::unordered_map<uint64, uint32> session_start_ms_;

    // Reconcile is split across the async-query boundary.
    //
    //   ReconcileBegin()  — the census (ComputeTargets / PopulateActual), the
    //                       session-rhythm logouts, the marked-bot tally, the
    //                       per-cycle log line and the over-target per-bucket
    //                       logouts. None of that reads the DB. It finishes by
    //                       issuing ONE async offline-candidate query per
    //                       bucket that is under target.
    //   ReconcileFinish() — the deficit fill (login an existing offline bot,
    //                       else spawn a new one) and the global TotalTarget
    //                       overflow kick, in the original order. Runs on the
    //                       world thread from OnWorldTick as soon as every
    //                       issued query has come back.
    //
    // The fill genuinely WAITS for the rows: if a query is still in flight,
    // ReconcileFinish has not run, so nothing logs in and nothing spawns on a
    // default. reconcile_in_progress_ is the pending flag — ReconcileBegin is a
    // no-op while a cycle is still waiting, so the same queries can never be
    // issued twice.
    // `synchronous` is for the operator commands only (`.playerbot population
    // force`, `.playerbot wipefleet`): they are one-off, they expect the spawns
    // to have happened by the time the command returns, and wipefleet in
    // particular calls ForceReconcile ten times in a row to burst-spawn. In that
    // mode the candidate rows are read with a blocking query and phase 2 runs
    // before the call returns. The world tick never uses it.
    void Reconcile() { ReconcileBegin(/*synchronous=*/false); }
    void ReconcileBegin(bool synchronous);
    void ReconcileFinish();
    // #5: log out bots whose online time exceeded their archetype's
    // (jittered) target_session_minutes, so the online roster churns like a
    // human playerbase. Runs at the TOP of Reconcile so the per-bucket fill
    // replaces the vacancies in the SAME cycle (stable headcount, rotating
    // membership). Config-gated (PlayerbotV2.SessionRhythm.*).
    void SessionRhythmLogout(uint32 now_ms);
    std::vector<PopulationBucket> ComputeTargets() const;
    void PopulateActual(std::vector<PopulationBucket>& buckets) const;

    // Offline bot characters that could be logged back in, used for the
    // "prefer login of existing bot" path. The rows come back from the async
    // query in last_seen_at order and are NOT faction-filtered (the query
    // never was); FilterOfflineCandidates applies the faction and the cap the
    // way the old synchronous call did.
    struct OfflineCandidate { uint64 char_guid; uint8 level; bool alliance; };

    // Issues the offline-candidate SELECT for one bucket. `cap` is the largest
    // number of logins either faction could use this cycle; the row limit is
    // 2*cap exactly as before, and FilterOfflineCandidates re-applies the
    // smaller per-side cap as a prefix so the selection is identical to the
    // per-side query it replaces.
    void QueueOfflineCandidateFetch(size_t fill_index, uint8 lo, uint8 hi, uint32 cap);

    // Starts the async cold load of one bot's pipeline row into
    // pipeline_row_cache_. No-op when the row is cached or already in flight.
    void QueuePipelineRowFetch(uint64 guid_low);

    // Hygiene's stale-JIT reclaim (bots tagged for a queue and unused for
    // kJitMaxAgeDays). Split out of RunHygiene because it is now async.
    void SweepStaleJitBots();

    // Hygiene's hard-cap kick. Split out alongside SweepStaleJitBots purely so
    // RunHygiene stays readable; no DB access of its own.
    void HygieneHardCap();
    // The same SELECT as QueueOfflineCandidateFetch, run blocking. ONLY for the
    // operator-command path (see ReconcileBegin's `synchronous`); never called
    // from the world tick.
    static std::vector<OfflineCandidate> FetchOfflineCandidatesSync(uint8 lo, uint8 hi, uint32 cap);
    static std::vector<OfflineCandidate> FilterOfflineCandidates(
        std::vector<OfflineCandidate> const& rows, bool alliance, uint32 cap);

    // JIT spawn one new bot at the given (level, faction). Returns the new
    // character's GUID counter, or 0 on failure. Decrements the faction's
    // per-cycle spawn budget on success.
    uint64 SpawnNew(uint8 level, bool alliance, uint8 race_hint = 0);

    // Login an offline bot character. Decrements the faction's per-cycle
    // login budget on success. Returns true on success.
    bool LoginExisting(uint64 char_guid, bool alliance);

    // Logout an over-target bot. LRU policy on last_seen_at.
    bool LogoutLRU(uint8 lo, uint8 hi, bool alliance);
    // Pass G: batch variant — single registry scan, up to `count`
    // victims. Replaces the prior N × full-scan loop. Returns the
    // number of bots logged out (≤ count).
    uint32 LogoutLRUMany(uint8 lo, uint8 hi, bool alliance, uint32 count);

    uint32 last_tick_ms_         = 0;
    uint32 last_hygiene_ms_      = 0;
    uint32 last_gear_backfill_ms_ = 0;
    uint32 last_gear_scrub_ms_ = 0;
    uint32 last_bg_group_hygiene_ms_ = 0;
    std::unordered_map<uint32, uint32> prep_jit_ms_;   // BG instance id -> last prep-phase JIT fill (TopUpActiveBGs)
    // Sweep in progress: bots still to visit and the re-gear count so far (RunGearBackfill spends a few ms per frame).
    std::vector<BotId> gear_scan_;
    size_t gear_scan_pos_ = 0;
    uint32 gear_regear_count_ = 0;
    // Per-bot cooldown for the gear backfill: a bot that PASSES the under-gear
    // gate but cannot actually be re-geared (bags full -> StoreNewItem fails, or
    // CanEquipItem refuses every slot -> swapped=0) is parked here for 30 min so
    // it does NOT re-consume the 25/pass processed-cap every pass and starve the
    // re-gearable bots downstream. Cleared as soon as a re-gear succeeds.
    std::unordered_map<uint64, uint32> gear_backfill_skip_until_;
    // Companion gear refresh points: bot -> Owned::RefreshKey (level, instance id) at its last gear pass. A different key (it
    // levelled, entered or left an instance) makes the next backfill pass regear it whatever the cooldown says.
    std::unordered_map<uint64, uint64> gear_refresh_key_;
    std::mutex gear_rush_mtx_;
    std::vector<BotId> gear_rush_;
    uint32 last_rebalance_ms_    = 0;
    uint32 last_drive_pipelines_ms_ = 0;
    uint32 last_bg_topup_ms_     = 0;
    uint32 last_bg_running_topup_ms_ = 0;  // separate timer for STATUS_IN_PROGRESS BG refill
    uint32 last_bg_seed_ms_      = 0;      // autonomous BG seeding cadence (SeedBgMatches)
    uint32 last_arena_seed_ms_   = 0;      // autonomous arena seeding cadence (SeedArenaMatches)
    uint32 bg_seed_rotation_     = 0;      // round-robins the seeded bg_type
    uint32 last_corpse_sweep_ms_ = 0;      // 10-min failed-JIT sweep cadence
    bool   bg_seed_match_seen_   = false;  // sticky rotation: advance only after a match formed
    // Per-faction reconcile budgets, reset each cycle. Splitting these
    // per side fixes faction starvation: previously both factions drew
    // from a single 25/25 pool and the bucket loop always processed
    // Alliance first, burning the whole pool before Horde got a turn.
    // Observed 2026-05-15: alli=603/1252 vs horde=73/1248 with HordePct=50.
    uint32 spawn_budget_alli_   = 0;
    uint32 spawn_budget_horde_  = 0;
    uint32 login_budget_alli_   = 0;
    uint32 login_budget_horde_  = 0;
    // Marked bot characters that are population inventory (owner-bound
    // companions excluded), counted once per Reconcile and bumped by each
    // SpawnNew; SpawnNew's 1.10x-of-TotalTarget spawn cap compares this.
    uint32 pool_marked_count_   = 0;

    // In-memory cache of bots whose pipeline is finished. It exists because the
    // pipeline's state used to be re-read from the DB on every pass, and a
    // not-yet-committed write meant RunFor re-ran every tick (re-invoking
    // DoPlaceAndTravel, which TeleportTo's the bot) until the DB worker drained
    // its queue — tens of minutes under load. The row is no longer read back at
    // all (pipeline_row_cache_ is the authority and the pipeline writes into it
    // via BotSetupPipeline::StateRef), but this stays as the cheap first gate:
    // it skips a done bot before any map/hash lookups.
    std::unordered_set<uint64> setup_done_cache_;

    // Priority-setup set — see RegisterPrioritySetup. Drained first and
    // without the per-tick budget cap in DriveSetupPipelines. Bounded to
    // small N (typically <100, one smoketest worth). The mutex protects
    // against the smoketest worker thread mutating the set while the
    // world thread reads it inside DriveSetupPipelines.
    std::unordered_set<uint64> priority_setup_ids_;
    std::mutex priority_mtx_;

    // Pass H: in-memory mirror of the playerbot_v2_character row so
    // DriveSetupPipelines doesn't re-SELECT once per second per in-world
    // non-done bot. Loaded lazily on first observation, refreshed when
    // we mutate state inline (jit_for_queue=NULL, AllDone transition).
    // Pipeline's own internal ReadState/PersistState are separate and
    // not eliminated by this cache.
    struct PipelineRow
    {
        uint8 state      = 0;
        uint8 dist_level = 0;
        std::string jit_tag;
    };
    std::unordered_map<uint64, PipelineRow> pipeline_row_cache_;

    // Async DB reads. Every population-path SELECT that used to block the world
    // thread is issued through this processor and handled by a callback that
    // also runs on the world thread (ProcessReadyCallbacks at the top of
    // OnWorldTick), so the callbacks need no locking around the caches they
    // fill. Each call site below carries its own pending flag so an in-flight
    // query is never issued a second time.
    QueryCallbackProcessor queries_;

    // Pipeline-row cold loads in flight, by guid_low. DriveSetupPipelines skips
    // a bot whose row is still being fetched rather than re-issuing the SELECT
    // on the next pass (it runs at 1 Hz, so without this a slow DB would get a
    // fresh duplicate query every second per uncached bot).
    std::unordered_set<uint64> pipeline_row_fetching_;

    // Failed-JIT corpse sweep (10-min cadence) and the hygiene stale-JIT delete
    // (1h cadence). Both act on the rows their SELECT returns, so a second pass
    // must not start while the first is still in flight — it would hand the same
    // 500/50 rows to the same delete path twice.
    bool corpse_sweep_pending_   = false;
    bool hygiene_delete_pending_ = false;

    // The in-flight state of one Reconcile cycle (see ReconcileBegin /
    // ReconcileFinish). reconcile_fills_ holds one entry per under-target
    // bucket: the bucket as the census saw it, plus the rows its query brings
    // back. reconcile_inflight_ counts queries not yet answered; phase 2 runs
    // when it reaches zero.
    struct PendingBucketFill
    {
        PopulationBucket bucket{};
        std::vector<OfflineCandidate> rows;   // last_seen_at order, both factions
        bool ready = false;                   // its query has come back
    };
    std::vector<PendingBucketFill> reconcile_fills_;
    uint32 reconcile_inflight_    = 0;
    bool   reconcile_in_progress_ = false;
    uint32 reconcile_started_ms_  = 0;   // watchdog stamp, see kReconcileWaitTimeoutMs
    // Bumped by every ReconcileBegin and captured by each query's callback. If
    // the watchdog ever abandons a cycle, a late row set from it carries the old
    // generation and is dropped instead of being filed against — and counted
    // towards — the cycle that has since started.
    uint32 reconcile_gen_         = 0;

    // Deferred login queue. SpawnNew creates a character via async SaveToDB
    // and pushes the guid here instead of calling LoginBot directly. The
    // next OnWorldTick drains this queue - by that point the async DB
    // worker has committed the INSERT, so LoginBot's LoginQueryHolder can
    // SELECT the row. Without this, mass-spawn under load (wipefleet)
    // produced "Player::LoadFromDB ... not found in table characters"
    // because LoginBot fired before the commit landed.
    std::vector<uint64> deferred_logins_;

    // Granular pop-subphase perf accounting. Aggregated across a 60s
    // window and emitted via [PopPerf] once per window so we can identify
    // which sub-phase dominates at scale (observed 2026-05-18: pop=14s/tick
    // at 3160 bots — needed the breakdown to find the killer). Reset
    // each emit. See OnWorldTick / Reconcile / LogoutLRU for accumulation.
    uint32 perf_window_start_ms_         = 0;
    uint32 perf_window_drain_ms_         = 0;
    uint32 perf_window_pipelines_ms_     = 0;
    uint32 perf_window_hygiene_ms_       = 0;
    uint32 perf_window_rebalance_ms_     = 0;
    uint32 perf_window_bgtopup_ms_       = 0;
    uint32 perf_window_reconcile_ms_     = 0;
    uint32 perf_window_compute_targets_ms_ = 0;
    uint32 perf_window_populate_actual_ms_ = 0;
    uint32 perf_window_reconcile_side_ms_  = 0;
    uint32 perf_window_login_existing_ms_  = 0;
    uint32 perf_window_spawn_new_ms_       = 0;
    uint32 perf_window_logout_lru_ms_      = 0;

    BotSetupPipeline* pipeline_ = nullptr;
    WorldBotSpawner*  world_spawner_ = nullptr;   // Playerbot.World.* proximity spawner (off by default)
};

} // namespace Playerbot::V2::Fleet
