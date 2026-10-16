#include "BotQueueFiller.h"
#include <set>
#include "ClassicSpec.h"
#include "BotAccountMgr.h"          // altbot (non-pool account) draft exemption
#include "BotPool.h"
#include "../Util/SlowCall.h"
#include "BotPopulationManager.h"    // ProtectFromKick lease for JIT match-fill spawns
#include "BotCharacterFactory.h"
#include "BotComposition.h"
#include "BotIdentityRegistry.h"
#include "OwnerRegistry.h"          // altbot (owner-bound) draft exemption
#include "../Services.h"
#include "../Util/ConfigReader.h"
#include "WorldSession.h"           // account-id lookup for the altbot test
#include "../Session/BotSessionMgr.h"
#include "../Bot/BotIntent.h"
#include "../Bot/BotIntentEmitter.h"
#include "../Bot/BotAI.h"           // ActionKind::DualSpec lockout
#include "../Bot/BotRegistry.h"     // Services::Registry().ai(id)
#include "../Bot/ClassTables.h"  // IsTankSpec / TankSpecForClass etc.
#include "../Threading/IntentQueue.h"
#include "../Diagnostics/PerfCounters.h"
#include "DB2Stores.h"
#include "Player.h"
#include "Map.h"
#include "BattlegroundScore.h"
#include "DisableMgr.h"
#include "Random.h"
#include "Containers.h"
#include "ObjectAccessor.h"
#include "BattlegroundMgr.h"
#include "Battleground.h"
#include "DungeonFinding/LFGMgr.h"
#include "Group.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "RaceMask.h"
#include "Timer.h"
#include "StringFormat.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>
#include <limits>
#include <span>

namespace Playerbot::V2::Fleet {

namespace {

struct RoleNeeds { uint32 tanks; uint32 healers; uint32 dps; };

// Class/role tally of one faction's share of a fill (pool queue + reused + freshly created bots). Drives the
// class spread of the DPS slots and the per-fill mix log.
struct FillMix
{
    std::array<uint32, 16> cls{};
    uint32 tanks = 0, healers = 0, dps = 0;
    void Add(uint8 c, uint8 role)   // role: 0 DPS, 1 tank, 2 healer
    {
        if (c < cls.size()) ++cls[c];
        if (role == 1) ++tanks; else if (role == 2) ++healers; else ++dps;
    }
    uint32 Total() const { return tanks + healers + dps; }
    std::string Describe() const
    {
        static constexpr char const* kNames[] = { "?", "Warrior", "Paladin", "Hunter", "Rogue", "Priest", "DK", "Shaman",
                                                  "Mage", "Warlock", "Monk", "Druid", "DH", "Evoker" };
        std::string out;
        for (size_t c = 1; c < cls.size() && c < std::size(kNames); ++c)
            if (cls[c])
                out += fmt::format("{}{} {}", out.empty() ? "" : ", ", kNames[c], cls[c]);
        return out;
    }
};

// The classes a vanilla battleground roster is drawn from.
constexpr uint8 kVanillaClasses[] = { 1, 2, 3, 4, 5, 7, 8, 9, 11 };   // Warrior Paladin Hunter Rogue Priest Shaman Mage Warlock Druid

// Least-represented vanilla class that `race` can be (random among ties); 0 when the race has none.
uint8 PickBalancedClass(uint8 race, FillMix const& mix)
{
    uint8 best[std::size(kVanillaClasses)];
    size_t n = 0;
    uint32 lowest = std::numeric_limits<uint32>::max();
    for (uint8 c : kVanillaClasses)
    {
        if (!BotComposition::IsLegalRaceClass(race, c)) continue;
        uint32 const have = mix.cls[c];
        if (have < lowest) { lowest = have; n = 0; }
        if (have == lowest) best[n++] = c;
    }
    return n ? best[urand(0, uint32(n) - 1)] : uint8(0);
}

// Stochastic rounding of count * share (share in tenths): 10 bots at 3/10 -> exactly 3; 4 bots -> 1 or 2.
uint32 RoleCap(uint32 count, uint32 tenths)
{
    uint32 const whole = (count * tenths) / 10u;
    return whole + (urand(0, 9) < (count * tenths) % 10u ? 1u : 0u);
}

RoleNeeds NeedsFor(BotQueueFiller::QueueKind kind)
{
    // BG needs intentionally overcommit: pipeline attrition (JIT bots
    // that don't finish setup before queue resolves) drops the
    // effective population by 30-60%. Queueing 2x ensures the BG
    // hits min-players quickly AND keeps the FreeSlotQueue topped up
    // so the BG can auto-pull replacements during prep phase / mid-
    // match. Bots that don't make it into the first instance fall
    // back to the next queue resolution; harmless.
    switch (kind)
    {
        case BotQueueFiller::QueueKind::Dungeon5: return {1, 1, 3};
        case BotQueueFiller::QueueKind::Raid10:   return {2, 3, 5};
        case BotQueueFiller::QueueKind::Raid20:   return {2, 4, 14};
        // BG fill is ROLE-AGNOSTIC: a battleground has no tank/healer slot
        // requirement (any N players form a side), so requiring tanks/healers here
        // only HARMS roster completion — JIT tank/heal CREATION fails on name-pool
        // exhaustion for the few tank/heal-capable race+class combos (DPS creation
        // succeeds), so a side stalled at ~6/10 in PREP waiting for tanks/heals that
        // never spawn and the match never started (live: WSG stuck PREP 6v6,
        // needs=3T/1H/0D). Fill purely as DPS so creation always succeeds and the
        // side reaches MinPlayers. (Healers/tanks among ONLINE bots are still
        // queued by the online-first pass — they just aren't a hard requirement.)
        case BotQueueFiller::QueueKind::Bg:
        default:                                  return {0, 0, 36};  // ~36 per side, role-agnostic
    }
}

// Role/spec predicates and class→spec lookups live in Bot/ClassTables.cpp
// so BotQueueFiller and idle:dual_spec_switch share one source of truth.
// Adding a class or shifting a canonical spec requires editing ClassTables
// only. (Originally inlined here; deduped 2026-05-10 when the idle rule
// landed.)

// A bot already confined to a fill (jit purpose) may be reassigned to another queue, but only while it is still in the open
// world: one that is inside the match it was made for sits in the battleground group on the battleground map, and queueing it
// again is rejected (grouped_not_leader / in_bg). AV 2026-10-11: 67 such bots were picked as "queued" every cycle, the ask for
// the last Horde slot counted as met, and the reuse/create fallback never ran for 7 minutes.
bool CommittedBotReusable(Player const* bot)
{
    if (bot->InBattleground() || bot->GetGroup() || bot->IsBeingTeleported() || !bot->IsAlive()) return false;
    Map const* map = bot->GetMap();
    return !(map && (map->IsDungeon() || map->IsBattlegroundOrArena()));
}

// Bracket -> level midpoint
uint8 BracketMidpoint(uint8 bracket)
{
    // bracket is the bracket "tag" used by BG queues: 0 for 10-19, 1 for 20-29...
    // For our purposes treat bracket as approximate floor of the player level / 10.
    // Convert by midpoint: bracket=2 -> level 25, bracket=7 -> level 75, etc.,
    // clamped to the realm cap (the top bracket is the single max-level band).
    return uint8(std::min<int>(int(MaxPlayerLevel()), int(bracket) * 10 + 15));
}

} // anonymous

void BotQueueFiller::Fill(FillRequest const& req)
{
    // Playerbot.Population.TotalTarget = 0 means owner bots only: no
    // JIT-created or ambient bots logged in to fill BG / LFG queues.
    // A battleground fill answers a real player's queue (or tops up the match that player is in), so it runs even with
    // the ambient fleet off. Dungeon and raid fills stay with the fleet.
    if (req.kind != QueueKind::Bg && !BotPopulationManager::AmbientFleetEnabled())
        return;

    auto needs = NeedsFor(req.kind);
    // Per-role override (LFG top-up). When ANY override is non-zero, the
    // caller is in override-mode — the entire (T, H, D) triple is treated
    // as authoritative deficit, including zero values. This lets the
    // LFG-refill cron say "need 0 tanks, 1 healer, 2 dps" without the
    // tank default of 1 sneaking back in. A request with all overrides
    // zero falls through to NeedsFor(kind) defaults.
    const bool override_mode =
        (req.needs_tank_override |
         req.needs_healer_override |
         req.needs_dps_override) != 0;
    if (override_mode)
    {
        needs.tanks   = req.needs_tank_override;
        needs.healers = req.needs_healer_override;
        needs.dps     = req.needs_dps_override;
    }
    // Apply explicit cap (running-BG top-up). When max_total_bots is set,
    // we want EXACTLY that many bots queued total across roles. Trim the
    // role quotas proportionally so the early-break in the bot loop fires
    // at the right cumulative count. Tanks/healers preserved up to the
    // cap; remainder goes to DPS.
    if (req.max_total_bots > 0)
    {
        const uint32 cap = req.max_total_bots;
        uint32 remaining = cap;
        const uint32 t_cap = std::min<uint32>(needs.tanks, remaining); remaining -= t_cap;
        const uint32 h_cap = std::min<uint32>(needs.healers, remaining); remaining -= h_cap;
        const uint32 d_cap = std::min<uint32>(needs.dps, remaining);
        needs = { t_cap, h_cap, d_cap };
    }
    uint32 total_needed = needs.tanks + needs.healers + needs.dps;
    if (!total_needed) return;

    Services::Perf().record_queue_fill_request();
    // Latency tracking: stamp the start time on the bot's DB row when JIT-
    // spawned (encoded in jit_for_queue tag isn't enough — we'd lose ms
    // resolution). Use process-time-ms via GameTime::Now via OS clock.
    // For online-bot fill we count immediate completion since they queue
    // synchronously in this call.
    uint32 const start_ms_for_completion = getMSTime();

    uint8 const target_level = req.target_level_override
                                   ? req.target_level_override
                                   : BracketMidpoint(req.bracket);

    // Compute the specific queue-type id for this Fill so we can
    // distinguish "bot already in OUR queue" (harmless, TC handles
    // idempotently) from "bot in some OTHER BG queue" (problematic
    // for backfill — the bot will be claimed by whichever BG pops
    // first and we won't get them).
    BattlegroundQueueTypeId const this_qid =
        (req.kind == QueueKind::Bg)
            ? BattlegroundMgr::BGQueueTypeId(
                  uint16(req.instance_id),
                  BattlegroundQueueIdType::Battleground,
                  /*rated*/ false, /*teamSize*/ 0)
            : BATTLEGROUND_QUEUE_NONE;

    // Faction handling. For BG queues, BOTH factions need bots so a match
    // can form (queue waits for N alliance + N horde). For LFG/raid queues,
    // only the player's faction needs filling (group is same-faction).
    bool const player_is_alliance = (req.faction == ALLIANCE);
    bool const is_bg = (req.kind == QueueKind::Bg);

    TC_LOG_INFO("playerbot.v2", "[QueueFill] kind={} bracket={} faction={} instance={} needs={}T/{}H/{}D{}",
                uint32(req.kind), uint32(req.bracket), uint32(req.faction),
                req.instance_id, needs.tanks, needs.healers, needs.dps,
                is_bg ? " (both factions)" : "");

  // Run the fill flow once per faction we need to populate.
  // For BG: 2 passes (player faction + enemy faction).
  // For LFG: 1 pass (player faction only).
  // A running-match top-up is the deficit of ONE side (req.faction). Running the second pass with that same count asked the
  // other side for the wrong number: AV 18 v 23 asked Alliance for 17 (gate: 0 free) from the Horde call and Horde for
  // 17 from the Alliance call, so the leading side's queue kept filling while the lagging side got a mixed-up wave.
  int const faction_passes = (is_bg && !req.single_faction) ? 2 : 1;
  for (int faction_pass = 0; faction_pass < faction_passes; ++faction_pass)
  {
    bool const want_alliance = (faction_pass == 0) ? player_is_alliance : !player_is_alliance;

    // Walk online bots, queue matching candidates by role.
    auto& reg = Services::Lifecycle();
    auto ids = reg.snapshot_ids();
    // Registry order is creation order: the first N eligible bots are the early-spawned role-targeted ones (the
    // raid healers and tanks). A BG roster must not be the head of that list.
    if (is_bg) Trinity::Containers::RandomShuffle(ids);
    uint32 queued_t = 0, queued_h = 0, queued_d = 0;
    FillMix mix;
    uint32 const dps_class_cap = std::max(2u, (needs.dps + 5u) / 6u);

    // Rejection histogram for diagnostics — without this we can't tell whether
    // the pool was small (online count low) or filtered (most bots in WSG queue).
    // Critical for diagnosing AB-stuck-after-WSG style asymmetries.
    uint32 rej_faction = 0, rej_level = 0, rej_grouped = 0, rej_combat = 0,
           rej_dungeon = 0, rej_no_qslot = 0, rej_role = 0, rej_deserter = 0;
    uint32 online_seen = 0, multi_queued = 0;

    // Level window: ±15 for BG (matches level-scale anyway and BG bracket
    // bounds are enforced strictly via PVPDifficulty below). LFG: ±3.
    // Owner directive 2026-05-15 — even with modern level-scaling, a L22
    // player matched with L14 dungeon-mates "reads broken" in chat / UI.
    // Tight window forces JIT-spawn of player-level bots when the online
    // pool is far off, which is sub-second on the modern pipeline and
    // produces a level-coherent group. target_level comes from
    // req.target_level_override which the PlayerbotV2 LFG path sets to
    // the player's actual level (see PlayerbotV2.cpp ~line 932).
    int const level_window = is_bg ? 15 : 3;
    for (auto id : ids)
    {
        if (queued_t >= needs.tanks && queued_h >= needs.healers && queued_d >= needs.dps)
            break;
        ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
        Player* bot = ObjectAccessor::FindConnectedPlayer(g);
        if (!bot) continue;
        ++online_seen;
        if ((bot->GetTeam() == ALLIANCE) != want_alliance)    { ++rej_faction; continue; }
        // BG-side hard bracket bounds (when provided). The BG's
        // PVPDifficulty data declares which levels each bracket accepts;
        // any bot outside that exact range fails the bracket lookup in
        // API::bg_queue and gets `no_bracket`. Filter strictly here so
        // the queue intents we emit can actually land in TC's queue.
        // Falls back to the coarse ±level_window check when bracket
        // bounds aren't supplied (LFG path, legacy callers).
        if (is_bg && req.bracket_min_level > 0 && req.bracket_max_level > 0)
        {
            uint8 const lvl = bot->GetLevel();
            if (lvl < req.bracket_min_level || lvl > req.bracket_max_level)
            { ++rej_level; continue; }
        }
        else if (std::abs(int(bot->GetLevel()) - int(target_level)) > level_window)
        { ++rej_level; continue; }
        // The one shared rule (BotPool.h): ambient, alive, ungrouped, out of combat, not in an instance, never a companion or
        // a real player's alt, and not a resident the world spawner holds for a player. A bot already confined to a fill
        // (jit purpose) may be reassigned here, so Committed passes.
        switch (::Playerbot::Pool::CheckFreeBot(bot, id))
        {
            case ::Playerbot::Pool::Verdict::Free:      break;
            case ::Playerbot::Pool::Verdict::Committed:
                if (!CommittedBotReusable(bot))         { ++rej_grouped; continue; }
                break;
            case ::Playerbot::Pool::Verdict::Combat:    ++rej_combat; continue;
            case ::Playerbot::Pool::Verdict::Instanced:
            case ::Playerbot::Pool::Verdict::Dead:      ++rej_dungeon; continue;
            default:                                    ++rej_grouped; continue;
        }
        // Pre-filter Deserter (BG side): API::bg_queue rejects deserters with
        // Locked. For LFG, the equivalent gate is HasAura(LFG_SPELL_DUNGEON_
        // DESERTER) — silently rejected inside LFGMgr::JoinLfg with no error
        // bubbling back to us. Either way the intent is wasted; skip up-front.
        if (is_bg && bot->IsDeserter())                       { ++rej_deserter; continue; }
        if (!is_bg && bot->HasAura(/*LFG_SPELL_DUNGEON_DESERTER*/ 71041))
                                                              { ++rej_deserter; continue; }
        if (!is_bg && bot->HasAura(/*LFG_SPELL_DUNGEON_COOLDOWN*/ 71328))
                                                              { ++rej_deserter; continue; }
        // BG-queue handling. TC's API::bg_queue is idempotent for the
        // same bg_type_id (returns Ok without re-adding). The real cost
        // is bots in queues for DIFFERENT bg_type_ids: they have 3 queue
        // slots total, and whichever BG pops first claims them, dropping
        // the others. For specific-BG top-up (max_total_bots > 0), skip
        // bots that are in some OTHER queue — bots already in OUR target
        // queue are fine to "queue" again (TC dedups) and tracking the
        // counter just surfaces pool saturation.
        const bool in_our_queue = is_bg &&
            this_qid != BATTLEGROUND_QUEUE_NONE &&
            bot->InBattlegroundQueueForBattlegroundQueueType(this_qid);
        const bool in_any_queue = bot->InBattlegroundQueue();
        if (in_any_queue && !in_our_queue)
        {
            ++multi_queued;
            if (req.max_total_bots > 0) { ++rej_no_qslot; continue; }
            // For LFG we MUST reject — LFGMgr::JoinLfg rejects any player
            // in BattlegroundQueue with LFG_JOIN_CANT_USE_DUNGEONS, silently
            // (no Result propagation back to us). Without this filter the
            // intent is pushed and the bot never actually enters the LFG
            // queue → observed "filler queued 3D, only 1D in LFG queue".
            if (!is_bg)              { ++rej_no_qslot; continue; }
        }
        if (!bot->HasFreeBattlegroundQueueId() && !in_our_queue)
        {
            ++rej_no_qslot;
            continue;
        }
        // LFG state pre-check: bot already queued/proposed/in-dungeon would
        // hit API::lfg_queue's state guard and return Locked. Filter here so
        // the histogram surfaces the count (otherwise these bots look "OK"
        // in the queue log but never actually join the dungeon queue).
        if (!is_bg)
        {
            lfg::LfgState const lst = sLFGMgr->GetState(bot->GetGUID());
            if (lst == lfg::LFG_STATE_QUEUED || lst == lfg::LFG_STATE_PROPOSAL ||
                lst == lfg::LFG_STATE_DUNGEON || lst == lfg::LFG_STATE_FINISHED_DUNGEON)
            {
                ++rej_no_qslot;
                continue;
            }
        }

        uint8 cls = bot->GetClass();
        uint16 spec = uint16(::Playerbot::Classic::EffectiveSpecId(bot));

        // Inline-respec priority for hybrid classes. Without this,
        // pass 1 queues a Shadow Priest / Balance Druid / Enhancement Shaman
        // as DPS, then pass 2 wants to convert them to Healer but the bot is
        // already LFG-queued → JoinLfg's "already queued" guard rejects the
        // second call. Net: bot is Holy-spec but registered as DPS in TC's
        // LFG → matcher never picks them as a healer. Fix: prefer scarce
        // roles (T > H > D) for hybrid classes in pass 1 itself, doing the
        // respec inline. Lockout still gates churn.
        bool const has_tank_path   = TankSpecForClass(cls) != 0;
        bool const has_healer_path = HealerSpecForClass(cls) != 0;
        BotAI* respec_ai_p1 = Services::Registry().ai(id);
        uint32 const lockout_now_ms = getMSTime();
        auto inline_respec = [&](uint32 target_spec) -> bool
        {
            if (target_spec == 0 || spec == target_spec) return false;
            if (respec_ai_p1 && respec_ai_p1->action_recently_tried(
                    BotAI::ActionKind::QueueFillRespec, id, lockout_now_ms))
                return false;   // lockout active — fall through; pass 2 won't help either
            if (auto const* se = sChrSpecializationStore.LookupEntry(target_spec))
            {
                bot->ActivateTalentGroup(se);
                if (respec_ai_p1)
                    respec_ai_p1->note_action_retry(
                        BotAI::ActionKind::QueueFillRespec, id, lockout_now_ms);
                return true;
            }
            return false;
        };

        auto queue_role = [&](Role r)
        {
            Intent it{};
            it.bot_id = id;
            if (req.kind == QueueKind::Bg)
                it.body = QueueIntent{BgQueueIntent{ObjectGuid::Empty, uint16(req.instance_id)}};
            else
                it.body = QueueIntent{LfgQueueIntent{req.instance_id, r}};
            Services::Intents(id).push(std::move(it));
            // REUSE of an online JIT bot (owner directive: reuse over re-create to
            // save the costly create+setup). Re-confine it to THIS BG and reset its
            // unused-timer: refresh the JIT purpose (new bg_type so it stages for the
            // right match) and extend the 10-min kick-protect lease so it isn't
            // logged out mid-match. An unused parked JIT bot's lease still lapses
            // -> LRU retires it (no idle bots lingering).
            if (req.kind == QueueKind::Bg)
            {
                if (BotAI* reuse_ai = Services::Registry().ai(id))
                    reuse_ai->set_bg_jit_purpose(uint16(req.instance_id), getMSTime());
                Services::Population().ProtectFromKick(uint64(id), 10u * 60u * 1000u);
            }
        };

        // Priority pick: scarce roles first (Tank > Healer > DPS). For each
        // role in priority order, if the slot is still open and the bot CAN
        // fill it (either natively or via inline respec), queue them as
        // that role. Critically: a tank-spec bot whose tank slot is already
        // full but who's healer-capable (e.g. Guardian Druid → Resto Druid)
        // gets respec'd to healer if that slot is open. Previous logic's
        // `!IsTankSpec && !IsHealerSpec` guard prevented this, wasting
        // hybrid capacity. Result of earlier log: `already_role=2, no_target
        // _class=8, healer_conv=0` even though both already-role bots could
        // have been respec'd into the open healer slot.
        auto try_queue_role = [&](Role r) -> bool
        {
            // Capacity check.
            if (is_bg && r == Role::Dps && mix.cls[cls < mix.cls.size() ? cls : 0] >= dps_class_cap) return false;
            if (r == Role::Tank   && queued_t >= needs.tanks)   return false;
            if (r == Role::Healer && queued_h >= needs.healers) return false;
            if (r == Role::Dps    && queued_d >= needs.dps)     return false;
            // Spec match? If yes, no respec needed.
            bool spec_matches = false;
            switch (r)
            {
                case Role::Tank:   spec_matches = IsTankSpec(cls, spec);   break;
                case Role::Healer: spec_matches = IsHealerSpec(cls, spec); break;
                case Role::Dps:    spec_matches = !IsTankSpec(cls, spec)
                                              && !IsHealerSpec(cls, spec); break;
                default: return false;
            }
            if (!spec_matches)
            {
                // Need to respec. Class must have a target spec for this role.
                uint32 target = 0;
                if      (r == Role::Tank)   target = TankSpecForClass(cls);
                else if (r == Role::Healer) target = HealerSpecForClass(cls);
                if (target == 0) return false;       // class can't fill role
                if (!inline_respec(target)) return false;
                // After respec, update local spec to avoid re-matching by old spec.
                spec = uint16(target);
            }
            queue_role(r);
            if      (r == Role::Tank)   ++queued_t;
            else if (r == Role::Healer) ++queued_h;
            else                        ++queued_d;
            mix.Add(cls, r == Role::Tank ? 1 : r == Role::Healer ? 2 : 0);
            return true;
        };

        if (try_queue_role(Role::Tank))   continue;
        if (try_queue_role(Role::Healer)) continue;
        if (try_queue_role(Role::Dps))    continue;
        // No role bucket has space the bot can fill — pass 2 still runs.
        ++rej_role;
    }

    TC_LOG_INFO("playerbot.v2",
        "[QueueFill] pool faction={} online_seen={} queued={}T/{}H/{}D "
        "(multi_queued={}) rejected: faction={} level={} grouped={} "
        "combat={} dungeon={} no_qslot={} role_full={} deserter={}",
        want_alliance ? "ALLIANCE" : "HORDE", online_seen,
        queued_t, queued_h, queued_d, multi_queued,
        rej_faction, rej_level, rej_grouped, rej_combat,
        rej_dungeon, rej_no_qslot, rej_role, rej_deserter);

    // Second pass: convert tank-/healer-CAPABLE classes that are currently in
    // the wrong spec. The 4885-bot pool tends to drift toward DPS specs
    // (State_Idle's apply_context_talents picks the leveling/raid build, which
    // for hybrid classes defaults to a DPS spec). Without conversion, a /lfg
    // request for 1T+1H falls almost entirely through to the JIT-spawn path,
    // which takes 5-15s of pipeline work per slot. Activating an existing
    // online bot's tank/healer spec inline is sub-100ms and uses bots the
    // owner already paid to spawn.
    //
    // We only re-spec when role quotas are still unmet after the first pass.
    // Re-spec then queue with the matching role; State_Idle's apply_context
    // _talents detects the spec change in-dungeon and lays in the right
    // talents on the next tick.
    // Pass-2 diagnostics: count candidates evaluated, rejection reasons,
    // respec attempts, and outcome. Without this we can't tell whether
    // pass-2 has zero hybrid candidates, a respec-lockout problem, a
    // missing HealerSpecForClass mapping, or an ActivateTalentGroup
    // failure. Emitted as a single line at end of pass-2 so the log
    // stays readable.
    uint32 p2_seen = 0, p2_filtered_out = 0;
    uint32 p2_already_role_spec = 0, p2_lockout = 0;
    uint32 p2_no_target_class = 0, p2_activate_failed = 0;
    uint32 p2_tank_converted = 0, p2_healer_converted = 0;
    if (queued_t < needs.tanks || queued_h < needs.healers)
    {
        for (auto id : ids)
        {
            if (queued_t >= needs.tanks && queued_h >= needs.healers)
                break;
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            Player* bot = ObjectAccessor::FindConnectedPlayer(g);
            if (!bot) continue;
            if ((bot->GetTeam() == ALLIANCE) != want_alliance) continue;
            if (is_bg && req.bracket_min_level > 0 && req.bracket_max_level > 0)
            {
                uint8 const lvl2 = bot->GetLevel();
                if (lvl2 < req.bracket_min_level || lvl2 > req.bracket_max_level)
                    continue;
            }
            else if (std::abs(int(bot->GetLevel()) - int(target_level)) > level_window)
                continue;
            ++p2_seen;
            {
                auto const v = ::Playerbot::Pool::CheckFreeBot(bot, id);
                if (v != ::Playerbot::Pool::Verdict::Free && v != ::Playerbot::Pool::Verdict::Committed)
                                                          { ++p2_filtered_out; continue; }
                if (v == ::Playerbot::Pool::Verdict::Committed && !CommittedBotReusable(bot))
                                                          { ++p2_filtered_out; continue; }
            }
            if (is_bg && bot->IsDeserter())               { ++p2_filtered_out; continue; }
            if (!is_bg && bot->HasAura(/*LFG_SPELL_DUNGEON_DESERTER*/ 71041))
                                                          { ++p2_filtered_out; continue; }
            if (!is_bg && bot->HasAura(/*LFG_SPELL_DUNGEON_COOLDOWN*/ 71328))
                                                          { ++p2_filtered_out; continue; }
            // Same other-queue exclusion as pass 1.
            const bool in_our_q2 = is_bg &&
                this_qid != BATTLEGROUND_QUEUE_NONE &&
                bot->InBattlegroundQueueForBattlegroundQueueType(this_qid);
            if (req.max_total_bots > 0 &&
                bot->InBattlegroundQueue() && !in_our_q2)
                                                          { ++p2_filtered_out; continue; }
            // For LFG, bot in any BG queue is rejected by LFGMgr::JoinLfg.
            if (!is_bg && bot->InBattlegroundQueue() && !in_our_q2)
                                                          { ++p2_filtered_out; continue; }
            if (!bot->HasFreeBattlegroundQueueId() && !in_our_q2)
                                                          { ++p2_filtered_out; continue; }
            if (!is_bg)
            {
                lfg::LfgState const lst2 = sLFGMgr->GetState(bot->GetGUID());
                if (lst2 == lfg::LFG_STATE_QUEUED || lst2 == lfg::LFG_STATE_PROPOSAL ||
                    lst2 == lfg::LFG_STATE_DUNGEON || lst2 == lfg::LFG_STATE_FINISHED_DUNGEON)
                                                          { ++p2_filtered_out; continue; }
            }

            uint8 const cls  = bot->GetClass();
            uint16 const cur_spec = uint16(::Playerbot::Classic::EffectiveSpecId(bot));

            // Skip bots already counted in pass 1 — IsTankSpec / IsHealerSpec
            // would have routed them to the right bucket above.
            if (IsTankSpec(cls, cur_spec) || IsHealerSpec(cls, cur_spec))
            {
                ++p2_already_role_spec;
                continue;
            }

            // Per-bot respec lockout (mirrors RunRebalance's DualSpec lockout).
            // ActivateTalentGroup calls UpdateTraitConfig which re-learns all
            // passive talent spells via Player::AddSpell. Some passive auras
            // (e.g. Monk Coalescence 450529) hit a TC core assertion when
            // re-applied while already active — preventing rapid-fire respec
            // on the same bot avoids the assertion crash.
            uint32 const now_ms_for_lockout = getMSTime();
            BotId const bot_id = id;
            BotAI* respec_ai = Services::Registry().ai(bot_id);
            bool can_respec = !respec_ai ||
                !respec_ai->action_recently_tried(BotAI::ActionKind::QueueFillRespec,
                                                  bot_id, now_ms_for_lockout);
            if (!can_respec) { ++p2_lockout; continue; }

            auto activate = [&](uint32 target_spec) -> bool {
                if (target_spec == 0 || cur_spec == target_spec) return false;
                if (auto const* spec_entry = sChrSpecializationStore.LookupEntry(target_spec))
                {
                    bot->ActivateTalentGroup(spec_entry);
                    if (respec_ai)
                        respec_ai->note_action_retry(BotAI::ActionKind::QueueFillRespec,
                                                    bot_id, now_ms_for_lockout);
                    return true;
                }
                return false;
            };

            if (queued_t < needs.tanks)
            {
                uint32 const target_spec = TankSpecForClass(cls);
                if (target_spec == 0) { ++p2_no_target_class; }
                else if (activate(target_spec))
                {
                    Intent it{};
                    it.bot_id = id;
                    if (req.kind == QueueKind::Bg)
                        it.body = QueueIntent{BgQueueIntent{ObjectGuid::Empty, uint16(req.instance_id)}};
                    else
                        it.body = QueueIntent{LfgQueueIntent{req.instance_id, Role::Tank}};
                    Services::Intents(id).push(std::move(it));
                    ++queued_t;
                    mix.Add(cls, 1);
                    ++p2_tank_converted;
                    continue;
                }
                else { ++p2_activate_failed; }
            }
            if (queued_h < needs.healers)
            {
                uint32 const target_spec = HealerSpecForClass(cls);
                if (target_spec == 0) { ++p2_no_target_class; }
                else if (activate(target_spec))
                {
                    Intent it{};
                    it.bot_id = id;
                    if (req.kind == QueueKind::Bg)
                        it.body = QueueIntent{BgQueueIntent{ObjectGuid::Empty, uint16(req.instance_id)}};
                    else
                        it.body = QueueIntent{LfgQueueIntent{req.instance_id, Role::Healer}};
                    Services::Intents(id).push(std::move(it));
                    ++queued_h;
                    mix.Add(cls, 2);
                    ++p2_healer_converted;
                }
                else { ++p2_activate_failed; }
            }
        }
        TC_LOG_INFO("playerbot.v2",
            "[QueueFill] pass2 respec faction={} seen={} filtered={} already_role={} "
            "lockout={} no_target_class={} activate_failed={} → tank_conv={} healer_conv={}",
            want_alliance ? "ALLIANCE" : "HORDE",
            p2_seen, p2_filtered_out, p2_already_role_spec, p2_lockout,
            p2_no_target_class, p2_activate_failed,
            p2_tank_converted, p2_healer_converted);
    }

    // Shortfall — JIT-spawn the rest. Each new bot gets full setup pipeline
    // at the bracket midpoint. The pipeline runs across a few ticks so the
    // bot may queue 1-2s after this call.
    // Class candidates per role. JIT picks one of these classes so the bot
    // can fill the right slot — even if exact spec depends on Player::Create
    // defaults, the class-side proficiency is right (Warrior can tank, Priest
    // can heal). Spec gets aligned later by State_Idle's apply_context_talents.
    static constexpr uint8 kTankClasses[]   = { 1, 2, 6, 10, 11, 12 };  // Warrior/Pally/DK/Monk/Druid/DH
    static constexpr uint8 kHealerClasses[] = { 2, 5, 7, 10, 11, 13 };  // Pally/Priest/Sham/Monk/Druid/Evoker
    auto pick_role_class = [](Role r, uint64 seed) -> uint8 {
        if (r == Role::Tank)
            return kTankClasses[seed % (sizeof(kTankClasses)/sizeof(kTankClasses[0]))];
        if (r == Role::Healer)
            return kHealerClasses[seed % (sizeof(kHealerClasses)/sizeof(kHealerClasses[0]))];
        return 0;  // 0 = no hint (random class)
    };

    auto spawn_jit = [&](Role role, uint32 count, char const* role_name) {
        // Faction-correct race candidates. Hinting a race to BotComposition::Roll
        // guarantees the resulting bot is on the right faction without retries.
        static constexpr uint8 kAllianceRaces[] = {
            RACE_HUMAN, RACE_DWARF, RACE_GNOME, RACE_NIGHTELF, RACE_DRAENEI,
            RACE_WORGEN, RACE_PANDAREN_ALLIANCE, RACE_VOID_ELF,
            RACE_LIGHTFORGED_DRAENEI, RACE_KUL_TIRAN, RACE_DARK_IRON_DWARF,
            RACE_MECHAGNOME, RACE_DRACTHYR_ALLIANCE, RACE_EARTHEN_DWARF_ALLIANCE,
        };
        static constexpr uint8 kHordeRaces[] = {
            RACE_ORC, RACE_TROLL, RACE_TAUREN, RACE_UNDEAD_PLAYER, RACE_BLOODELF,
            RACE_GOBLIN, RACE_PANDAREN_HORDE, RACE_NIGHTBORNE,
            RACE_HIGHMOUNTAIN_TAUREN, RACE_ZANDALARI_TROLL, RACE_VULPERA,
            RACE_MAGHAR_ORC, RACE_DRACTHYR_HORDE, RACE_EARTHEN_DWARF_HORDE,
        };
        auto const races = want_alliance ? std::span<uint8 const>(kAllianceRaces)
                                         : std::span<uint8 const>(kHordeRaces);

        uint32 roll_fail = 0, create_fail = 0, success = 0, reused = 0;
        std::string last_create_reason;

        // REUSE pass before creating anything: the DB accumulates offline
        // marked L80 bots from every previous JIT cycle (each seed used to
        // mint ~70 brand-new characters; nothing ever logged the old ones
        // back in -- observed: thousands of offline pool L80s while every
        // cycle paid full character-creation + setup-pipeline cost and the
        // name pools drained). Re-tag + defer-login existing pool-account
        // characters of the right faction/class/level; the same
        // jit_for_queue pipeline hook queues them once in-world. Only the
        // remainder falls through to creation.
        {
            std::string class_in;
            auto append_classes = [&](uint8 const* arr, size_t n)
            {
                for (size_t ci = 0; ci < n; ++ci)
                {
                    if (!class_in.empty()) class_in += ',';
                    class_in += std::to_string(uint32(arr[ci]));
                }
            };
            if (role == Role::Tank)
                append_classes(kTankClasses, sizeof(kTankClasses)/sizeof(kTankClasses[0]));
            else if (role == Role::Healer)
                append_classes(kHealerClasses, sizeof(kHealerClasses)/sizeof(kHealerClasses[0]));
            std::string race_in;
            for (uint8 rc : races)
            {
                if (!race_in.empty()) race_in += ',';
                race_in += std::to_string(uint32(rc));
            }
            const uint8 lvl_lo = (is_bg && req.bracket_min_level > 0)
                ? req.bracket_min_level : uint8(std::max(1, int(target_level) - 3));
            const uint8 lvl_hi = (is_bg && req.bracket_max_level > 0)
                ? req.bracket_max_level : uint8(std::min(int(MaxPlayerLevel()), int(target_level) + 3));
            // totaltime>0 excludes the failed-login corpse pool (hygiene
            // sweeps those); the pool-account join excludes player alts.
            // Random order, and for DPS slots a wider candidate window the class/role spread below picks from: the plain
            // table order is creation order, i.e. the early role-targeted paladins, priests and druids every time.
            bool const spread = (role == Role::Dps);
            uint32 const window = spread ? std::min<uint32>(count * 10u, 400u) : count;
            SLOW_CALL("BotQueueFiller reuse-candidate query (database)");
            auto reuse_res = CharacterDatabase.PQuery(
                "SELECT c.guid, c.class FROM characters c "
                "JOIN playerbot_v2_character pv ON pv.character_guid_low = c.guid "
                "JOIN playerbot_v2_account pa ON pa.account_id = c.account "
                "WHERE c.online = 0 AND c.totaltime > 0 AND pv.owner_account_id = 0 "
                "AND c.level BETWEEN {} AND {} "
                "AND c.race IN ({}) {} "
                "ORDER BY RAND() LIMIT {}",
                uint32(lvl_lo), uint32(lvl_hi), race_in,
                class_in.empty() ? std::string()
                                 : fmt::format("AND c.class IN ({})", class_in),
                window);
            if (!reuse_res)
                TC_LOG_INFO("playerbot.v2",
                    "[QueueFill] reuse query EMPTY role={} lvl={}..{} classes='{}' races_n={} limit={}",
                    role_name, uint32(lvl_lo), uint32(lvl_hi),
                    class_in.empty() ? "any" : class_in, uint32(races.size()), window);
            if (reuse_res)
            {
                struct Cand { uint64 guid; uint8 cls; uint8 role; bool taken; };
                std::vector<Cand> cands;
                do {
                    Field const* f = reuse_res->Fetch();
                    uint64 const g = f[0].GetUInt64();
                    uint8 const c = f[1].GetUInt8();
                    cands.push_back({ g, c, spread ? ClassicRoleRollFor(g, c) : uint8(0), false });
                } while (reuse_res->NextRow());

                std::vector<size_t> picked;
                if (!spread)
                {
                    for (size_t i = 0; i < cands.size() && picked.size() < count; ++i) picked.push_back(i);
                }
                else
                {
                    // Class spread first (one of each class, then two of each...), then the role caps: at most ~3 healers
                    // and ~1 tank per 10 DPS-slot bots. A last round lifts only the class limit.
                    uint32 const cap_h = RoleCap(count, 3), cap_t = RoleCap(count, 1);
                    uint32 const max_round = std::max(2u, (count + 5u) / 6u);
                    uint32 got_h = 0, got_t = 0;
                    for (uint32 round = 1; round <= max_round + 1 && picked.size() < count; ++round)
                    {
                        bool const lift_class_limit = (round > max_round);
                        for (size_t i = 0; i < cands.size() && picked.size() < count; ++i)
                        {
                            Cand& cd = cands[i];
                            if (cd.taken) continue;
                            if (!lift_class_limit && mix.cls[cd.cls < mix.cls.size() ? cd.cls : 0] >= round) continue;
                            if (cd.role == 2 && got_h >= cap_h) continue;
                            if (cd.role == 1 && got_t >= cap_t) continue;
                            cd.taken = true;
                            if (cd.role == 2) ++got_h; else if (cd.role == 1) ++got_t;
                            mix.Add(cd.cls, cd.role);
                            picked.push_back(i);
                        }
                    }
                }
                for (size_t i : picked)
                {
                    const uint64 reuse_guid = cands[i].guid;
                    char const* kind_str = (req.kind == QueueKind::Bg) ? "BG" : "LFG";
                    CharacterDatabase.PExecute(
                        "UPDATE playerbot_v2_character SET distribution_level={}, "
                        "jit_for_queue='{}:{}:{}', last_seen_at=NOW() "
                        "WHERE character_guid_low={}",
                        uint32(target_level), kind_str, req.instance_id, role_name,
                        reuse_guid);
                    Services::Population().DeferLogin(reuse_guid);
                    Services::Population().ProtectFromKick(reuse_guid, 10u * 60u * 1000u);
                    if (!spread) mix.Add(cands[i].cls, role == Role::Tank ? 1 : role == Role::Healer ? 2 : 0);
                    ++reused;
                }
            }
        }
        count = (reused >= count) ? 0u : count - reused;
        success += reused;

        for (uint32 i = 0; i < count; ++i)
        {
            uint8 race_hint = races[uint64(i + std::time(nullptr)) % races.size()];
            uint8 cls_hint  = (role == Role::Dps) ? PickBalancedClass(race_hint, mix)
                                                  : pick_role_class(role, uint64(i + std::time(nullptr)));
            BotComposition::Pick pick = BotComposition::Roll(race_hint, cls_hint);
            if (!pick.race) { ++roll_fail; continue; }
            auto created = BotCharacterFactory::Create(/*ownerSession=*/nullptr,
                pick.name, pick.race, pick.cls, pick.gender);
            // Name-collision retry: rolled names collide against the 40K+
            // existing characters DB at meaningful rates, and a single-shot
            // create made the whole JIT spawn batch fail with 'name already
            // in use' (observed: 36/36 fails on the first autonomous BG
            // seed). Re-roll a fresh pick a few times before surrendering
            // the slot.
            for (int retry = 0;
                 !created.ok && created.reason == "name already in use" && retry < 8;
                 ++retry)
            {
                // Vary the RACE per retry — name pools are per race/gender,
                // so re-rolling the same race draws from the same (possibly
                // exhausted) pool and collides again (observed: tank/heal
                // passes failing all retries while DPS succeeded).
                const uint8 retry_race =
                    races[uint64(i + retry + 1 + std::time(nullptr)) % races.size()];
                if (role == Role::Dps) cls_hint = PickBalancedClass(retry_race, mix);
                pick = BotComposition::Roll(retry_race, cls_hint);
                if (!pick.race) break;
                created = BotCharacterFactory::Create(/*ownerSession=*/nullptr,
                    pick.name, pick.race, pick.cls, pick.gender);
            }
            if (!created.ok)
            {
                ++create_fail;
                last_create_reason = created.reason;
                continue;
            }
            ++success;
            mix.Add(pick.cls, ClassicRoleRollFor(created.guid.GetCounter(), pick.cls));
            // Mark distribution_level + JIT-tag with instance + role so the
            // setup pipeline post-completion hook (BotPopulationManager::
            // DriveSetupPipelines) can fire the matching queue intent once
            // the bot is fully geared/talented/placed in-world.
            // Tag format: "<KIND>:<INSTANCE_ID>:<ROLE>" e.g. "BG:128:DPS",
            // "LFG:271:HEAL". Parsed in the population manager.
            uint64 guid_low = created.guid.GetCounter();
            // A BG bot is created inside the bracket that asked for it, spread over the bracket's levels (twinks as well as 60s).
            uint8 const bot_level = is_bg ? uint8(Battleground::BracketLevelFor(req.bracket_min_level, req.bracket_max_level, MaxPlayerLevel(),
                                                                                    target_level, urand(0, 1000))) : target_level;
            char const* kind_str = (req.kind == QueueKind::Bg) ? "BG" : "LFG";
            char tag_buf[48];
            std::snprintf(tag_buf, sizeof(tag_buf), "%s:%u:%s",
                          kind_str, req.instance_id, role_name);
            CharacterDatabase.PExecute(
                "UPDATE playerbot_v2_character SET distribution_level={}, jit_for_queue='{}', "
                "last_seen_at=NOW() WHERE character_guid_low={}",
                uint32(bot_level), tag_buf, guid_low);
            // DEFERRED login (BG audit follow-up): Create's SaveToDB is
            // async -- a same-tick LoginBot loses the race and the session
            // is kicked with "Player::LoadFromDB failed" (observed: 542
            // JIT chars created in 30 min, 1 in world). SpawnNew solved
            // this with deferred_logins_; route through the same queue.
            Services::Population().DeferLogin(guid_low);
            // Kick-protection lease: between this login and the setup
            // pipeline's queue intent, the bot is overshoot the Reconcile
            // overflow kick would otherwise log straight back out
            // (observed live: online_seen pinned at TotalTarget while
            // every spawn_jit batch "succeeded"). 10 min covers the
            // longest setup pipeline; once queued/ported, the BG-queue
            // and BG-map guards take over.
            Services::Population().ProtectFromKick(guid_low, 10u * 60u * 1000u);
            Services::Perf().record_queue_fill_jit_spawned();
            (void)role;
        }
        // Per-spawn_jit-call summary. Without this, a queue request that
        // hits 5/5 silent Create failures looked indistinguishable from
        // a successful spawn — owner observed 2026-05-15: queued Ragefire,
        // QueueFill logged "JIT-spawning rest" but no JIT setup-complete
        // ever fired, no rows tagged jit_for_queue. Could be roll fail,
        // could be name collision, could be pool starvation — we need
        // to know which.
        if (success != count + reused || roll_fail || create_fail || reused)
            TC_LOG_INFO("playerbot.v2",
                "[QueueFill] spawn_jit role={} requested={} success={} (reused={}) roll_fail={} create_fail={}{}",
                role_name, count + reused, success, reused, roll_fail, create_fail,
                create_fail ? fmt::format(" last_reason='{}'", last_create_reason) : std::string());
    };

    // Skip JIT spawn entirely when caller asked for online-only fill
    // (BG top-up tick uses this to avoid spamming new JIT bots that
    // wouldn't complete setup before the BG's 90s invite window).
    if (!req.online_only)
    {
        if (queued_t < needs.tanks)   spawn_jit(Role::Tank,   needs.tanks - queued_t,   "TANK");
        if (queued_h < needs.healers) spawn_jit(Role::Healer, needs.healers - queued_h, "HEAL");
        if (queued_d < needs.dps)     spawn_jit(Role::Dps,    needs.dps - queued_d,     "DPS");
    }

    if (mix.Total())
        TC_LOG_INFO("playerbot.v2", "[QueueFill] mix faction={} kind={} inst={} total={} {}T/{}H/{}D classes: {}",
                    want_alliance ? "ALLIANCE" : "HORDE", uint32(req.kind), req.instance_id, mix.Total(),
                    mix.tanks, mix.healers, mix.dps, mix.Describe());
    TC_LOG_INFO("playerbot.v2", "[QueueFill] faction={} queued online: {}T {}H {}D; JIT-spawning rest",
                want_alliance ? "ALLIANCE" : "HORDE", queued_t, queued_h, queued_d);
  }  // end faction_pass loop

    // The "online" portion completed synchronously — stamp completion latency
    // (typically <1ms) so we have data even for queues that didn't need JIT.
    // JIT completions get stamped separately in BotPopulationManager when
    // each newly-spawned bot finishes its setup pipeline and its queue
    // intent fires.
    uint32 const elapsed_ms = getMSTime() - start_ms_for_completion;
    Services::Perf().record_queue_fill_completion(Ms{elapsed_ms});
}

void BotQueueFiller::OnMatchEnd(Player* /*bot*/)
{
    // Retention window applied at hygiene cron — JIT bots whose
    // last_seen_at expired get logged out + deleted. Nothing to do per match.
}

// ---------------------------------------------------------------------------
// Ambient BG participation (Playerbot.Bg.AutoJoin*)
// ---------------------------------------------------------------------------
namespace {

// Same evergreen set SeedBgMatches rotates (validated templates with live GOs / scripts).
// Forever runs the 1.12 battlegrounds: WSG 2 (map 489), AB 3 (529), AV 1 (30).
constexpr uint16 kDefaultAmbientBgTypes[] = { 2, 3, 1 };

struct AmbientBgState
{
    std::unordered_map<uint64, uint32> queued_at_ms;   // ambient bots WE queued -> when
    std::unordered_map<uint64, uint32> rest_until_ms;  // per-bot cooldown after a match / queue
    uint32 last_pass_ms = 0;
};
AmbientBgState g_ambient;                              // world thread only

// hold_until_ms: while in the future, TC must not open a NEW instance of this type/bracket for the queue (the human is
// about to get a place in a running one: a bot leaves for them). 0 = no hold.
struct HumanBgWait { ObjectGuid guid; uint16 bg_type; uint32 since_ms; uint32 last_try_ms; uint8 bracket = 0xFF; uint32 hold_until_ms = 0; };
std::vector<HumanBgWait> g_human_waits;                // world thread only (hook + tick)
uint32 g_last_room_pass_ms = 0;
std::unordered_map<uint64, uint32> g_room_victims;     // bot -> leave requested at
std::unordered_map<uint32, uint32> g_room_made_ms;     // bg type -> last time a bot was asked to leave for a human

bool IsPoolBot(Player const* bot, BotId id)
{
    return ::Playerbot::Pool::FreeFactsOf(bot, id).role != ::Playerbot::Pool::BotRole::Owned;
}

bool CarriesBgObjective(Player const* p)
{
    // WSG flags (Silverwing / Warsong), EotS flag, Temple of Kotmogu orbs.
    for (uint32 aura : { 23333u, 23335u, 34976u, 121164u, 121175u, 121176u, 121177u })
        if (p->HasAura(aura)) return true;
    return false;
}

} // anonymous

void BotQueueFiller::AutoJoinTick(uint32 now_ms)
{
    auto const& cfg = Services::Config().bg_auto_join();
    if (!cfg.enabled) return;
    constexpr uint32 kPassMs = 10u * 1000u;
    if (g_ambient.last_pass_ms && (now_ms - g_ambient.last_pass_ms) < kPassMs) return;
    g_ambient.last_pass_ms = now_ms;

    // Types this pass may queue for.
    std::vector<uint16> type_ids;
    if (cfg.types.empty())
        type_ids.assign(std::begin(kDefaultAmbientBgTypes), std::end(kDefaultAmbientBgTypes));
    else
    {
        size_t pos = 0;
        while (pos < cfg.types.size())
        {
            size_t end = cfg.types.find(',', pos);
            if (end == std::string::npos) end = cfg.types.size();
            uint32 v = uint32(std::strtoul(cfg.types.substr(pos, end - pos).c_str(), nullptr, 10));
            if (v) type_ids.push_back(uint16(v));
            pos = end + 1;
        }
    }
    struct TypeInfo { uint16 id; BattlegroundTemplate const* tmpl; uint32 queued[2]; };   // [0]=alliance [1]=horde
    std::vector<TypeInfo> types;
    for (uint16 t : type_ids)
    {
        BattlegroundTemplate const* tmpl = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(t));
        if (!tmpl || tmpl->IsArena() || tmpl->MapIDs.empty()) continue;
        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, t, nullptr)) continue;
        types.push_back({ t, tmpl, {0, 0} });
    }
    if (types.empty()) return;

    auto type_index = [&](uint16 id) -> TypeInfo* {
        for (auto& ti : types) if (ti.id == id) return &ti;
        return nullptr;
    };

    std::vector<Player*> candidates;
    uint32 ambient_queued = 0;
    uint32 const rest_ms    = cfg.cooldown_min * 60u * 1000u;
    uint32 const timeout_ms = cfg.queue_timeout_min * 60u * 1000u;
    auto ids = Services::Lifecycle().snapshot_ids();
    for (BotId id : ids)
    {
        Player* bot = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!bot || !bot->IsInWorld()) continue;
        if (!IsPoolBot(bot, id)) continue;
        uint8 const team_idx = bot->GetTeam() == ALLIANCE ? 0 : 1;

        if (bot->InBattleground())
        {
            // In a match: nothing to do, but the cooldown starts when it ends.
            g_ambient.queued_at_ms.erase(id);
            g_ambient.rest_until_ms[id] = now_ms + rest_ms;
            continue;
        }

        if (bot->InBattlegroundQueue())
        {
            // Count every queued bot (filler, premade or ambient) toward the per-type balance.
            bool invited = false;
            for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
            {
                BattlegroundQueueTypeId q = bot->GetBattlegroundQueueTypeId(i);
                if (q == BATTLEGROUND_QUEUE_NONE) continue;
                if (TypeInfo* ti = type_index(q.BattlemasterListId)) ++ti->queued[team_idx];
                if (bot->IsInvitedForBattlegroundQueueType(q)) invited = true;
            }
            auto it = g_ambient.queued_at_ms.find(id);
            if (it == g_ambient.queued_at_ms.end()) continue;    // someone else's queue (filler / premade)
            ++ambient_queued;
            if (!invited && (now_ms - it->second) >= timeout_ms)
            {
                Intent leave{};
                leave.bot_id = id;
                leave.body   = QueueIntent{BgLeaveIntent{}};
                Services::Intents(id).push(std::move(leave));
                g_ambient.queued_at_ms.erase(it);
                g_ambient.rest_until_ms[id] = now_ms + rest_ms;
                --ambient_queued;
                TC_LOG_INFO("playerbot.v2", "[bg_queue] {} ambient queue timed out after {} min; leaving queue",
                            bot->GetName(), cfg.queue_timeout_min);
            }
            continue;
        }
        if (g_ambient.queued_at_ms.erase(id))
            g_ambient.rest_until_ms[id] = now_ms + rest_ms;    // left the queue some other way

        // ---- candidate checks (cheap first) ----
        if (bot->GetLevel() < cfg.min_level) continue;
        if (auto rit = g_ambient.rest_until_ms.find(id);
            rit != g_ambient.rest_until_ms.end() && int32(now_ms - rit->second) < 0) continue;
        if (bot->IsInFlight()) continue;
        if (bot->GetHealthPct() < 70.0f) continue;
        if (bot->GetPowerType() == POWER_MANA && bot->GetMaxPower(POWER_MANA) > 0 &&
            bot->GetPowerPct(POWER_MANA) < 50.0f) continue;
        if (::Playerbot::Pool::CheckFreeBot(bot, id) != ::Playerbot::Pool::Verdict::Free) continue;   // the shared rule: ambient, ungrouped, alive, out of combat/instances, jit-committed bots excluded
        if (bot->IsDeserter() || !bot->HasFreeBattlegroundQueueId()) continue;
        if (sLFGMgr->GetState(bot->GetGUID()) != lfg::LFG_STATE_NONE) continue;
        if (!Services::Registry().ai(id)) continue;
        candidates.push_back(bot);
    }

    if (candidates.empty() || ambient_queued >= cfg.max_queued) return;

    // Per-bot chance per MINUTE, rolled each 10 s pass.
    uint32 const roll_max  = 10000u;
    uint32 const threshold = (cfg.chance_pct * 100u * kPassMs) / (60u * 1000u);   // of 10000
    if (!threshold) return;

    Trinity::Containers::RandomShuffle(candidates);
    uint32 queued_now = 0;
    for (Player* bot : candidates)
    {
        if (queued_now >= cfg.max_per_tick || ambient_queued >= cfg.max_queued) break;
        if (urand(0, roll_max - 1) >= threshold) continue;

        BotId const id = BotId(bot->GetGUID().GetCounter());
        uint8 const team_idx = bot->GetTeam() == ALLIANCE ? 0 : 1;
        TypeInfo* best = nullptr;
        int32 best_score = std::numeric_limits<int32>::min();
        for (auto& ti : types)
        {
            if (!DB2Manager::GetBattlegroundBracketByLevel(ti.tmpl->MapIDs.front(), bot->GetLevel())) continue;
            uint32 const cap = ti.tmpl->GetMaxPlayersPerTeam();
            if (cap && ti.queued[team_idx] >= cap) continue;   // my side is already a full roster
            int32 const mine = int32(ti.queued[team_idx]), other = int32(ti.queued[1 - team_idx]);
            // Fill the shorter side first, then concentrate on the fuller queue so a match forms
            // instead of spreading bots thinly over every type; jitter breaks ties.
            int32 const score = (other - mine) * 100 + (mine + other) * 5 + int32(urand(0, 9));
            if (score > best_score) { best_score = score; best = &ti; }
        }
        if (!best) continue;

        Intent it{};
        it.bot_id = id;
        it.body   = QueueIntent{BgQueueIntent{ObjectGuid::Empty, best->id}};
        Services::Intents(id).push(std::move(it));
        ++best->queued[team_idx];
        ++ambient_queued;
        ++queued_now;
        g_ambient.queued_at_ms[id] = now_ms;
        TC_LOG_INFO("playerbot.v2",
            "[bg_queue] {} L{} {} ambient auto-join bg_type={} (type queued {}A/{}H, ambient total {}/{})",
            bot->GetName(), uint32(bot->GetLevel()), team_idx == 0 ? "ALLIANCE" : "HORDE", best->id,
            best->queued[0], best->queued[1], ambient_queued, cfg.max_queued);
    }
}

void BotQueueFiller::NoteHumanBgQueue(Player* human, uint32 bg_type_id, uint8 bracket)
{
    if (!human) return;
    if (human->GetSession() && human->GetSession()->IsBot()) return;
    for (auto& w : g_human_waits)
        if (w.guid == human->GetGUID() && w.bg_type == bg_type_id) { w.since_ms = getMSTime(); w.bracket = bracket; return; }
    HumanBgWait w{ human->GetGUID(), uint16(bg_type_id), getMSTime(), 0 };
    w.bracket = bracket;
    g_human_waits.push_back(w);
}

namespace {

// Bots of `team` in `bg` that may leave to free a slot, weakest first: lowest score (kills*3 + HKs - deaths), then lowest
// level. Never an owner's companion, a flag carrier or a bot in combat, nor one already asked to leave.
std::vector<std::pair<std::pair<int32, int32>, Player*>> EvictableBots(Battleground* bg, Team team)
{
    std::vector<std::pair<std::pair<int32, int32>, Player*>> out;
    for (auto const& slot : bg->GetPlayers())
    {
        Player* member = ObjectAccessor::FindConnectedPlayer(slot.first);
        if (!member || !member->GetSession() || !member->GetSession()->IsBot()) continue;
        if (bg->GetPlayerTeam(slot.first) != team) continue;
        if (member->IsInCombat() || CarriesBgObjective(member)) continue;
        if (g_room_victims.count(slot.first.GetCounter())) continue;
        // Pool bots and the JIT match-fill bots (most BG fillers) may leave; a player's own character or companion may not.
        BotId const member_id = BotId(slot.first.GetCounter());
        if (Services::Owners().GetOwner(member_id).account_id != 0) continue;
        BotAI* member_ai = Services::Registry().ai(member_id);
        if (!IsPoolBot(member, member_id) && !(member_ai && member_ai->is_jit_purpose())) continue;
        int32 score = 0;
        if (BattlegroundScore const* sc = bg->GetBattlegroundScore(member))
            score = int32(sc->GetKillingBlows()) * 3 + int32(sc->GetHonorableKills()) - int32(sc->GetDeaths());
        out.push_back({ { score, int32(member->GetLevel()) }, member });
    }
    std::sort(out.begin(), out.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
    return out;
}


struct Placement
{
    Battleground* bg = nullptr;
    uint32 missing = 0;                                     // places the human's party still needs after the free ones
    std::vector<std::pair<std::pair<int32, int32>, Player*>> victims;
    bool any_room = false;                                  // some instance has the places already: TC's queue invites the player there
    std::string why;                                        // per-instance verdicts, for the "no running instance" log
};

uint32 HumansIn(Battleground const* bg)
{
    uint32 n = 0;
    for (auto const& slot : bg->GetPlayers())
    {
        Player const* p = ObjectAccessor::FindConnectedPlayer(slot.first);
        if (p && p->GetSession() && !p->GetSession()->IsBot()) ++n;
    }
    return n;
}

uint32 BotsOnTeam(Battleground const* bg, Team team)
{
    uint32 n = 0;
    for (auto const& slot : bg->GetPlayers())
    {
        Player const* p = ObjectAccessor::FindConnectedPlayer(slot.first);
        if (p && p->GetSession() && p->GetSession()->IsBot() && bg->GetPlayerTeam(slot.first) == team) ++n;
    }
    return n;
}

// The running (or preparing) instance of this type and bracket a real player is put into, mixed or bot-only: the one with the most
// real players, then the one with the most bots on the player's team. A full team makes room by evicting bots (EvictableBots: never a
// human, an owner's companion, a flag carrier or a bot in combat). A new instance is only opened when none can take the player.
Placement ChoosePlacement(Team team, uint32 need, uint32 bg_type_id, uint8 bracket)
{
    Placement best;
    uint32 best_humans = 0, best_bots = 0;
    std::set<uint32> seen;
    auto consider = [&](Battleground* bg)
    {
        if (!bg || !seen.insert(bg->GetInstanceID()).second) return;
        if (bg->isRated() || uint8(bg->GetBracketId()) != bracket)
        { best.why += Trinity::StringFormat(" [inst {} wrong bracket {} / rated]", bg->GetInstanceID(), uint32(bg->GetBracketId())); return; }
        if (bg->GetStatus() != STATUS_IN_PROGRESS && bg->GetStatus() != STATUS_WAIT_JOIN)
        { best.why += Trinity::StringFormat(" [inst {} status {}]", bg->GetInstanceID(), uint32(bg->GetStatus())); return; }
        // a waiting real player is invited into any physically free place (BattlegroundQueue::FillPlayersToBG), balance gate aside, so the
        // places that must be made are counted from the team size: a team already over the cap (12 in a 10-a-side match) loses enough
        // bots to leave room for the player, not just one
        uint32 const taken = std::max(bg->GetPlayersCountByTeam(team), bg->GetInvitedCount(team));
        uint32 const free_slots = Battleground::PhysicalRoom(taken, bg->GetMaxPlayersPerTeam());
        uint32 const missing = Battleground::PlacesToMake(taken, bg->GetMaxPlayersPerTeam(), need);
        auto victims = missing ? EvictableBots(bg, team) : decltype(best.victims){};
        if (missing && victims.size() < missing)             // only real players on that side to swap: not a candidate
        { best.why += Trinity::StringFormat(" [inst {} free {} need {} evictable {}]", bg->GetInstanceID(), free_slots, need, victims.size()); return; }
        if (!missing) best.any_room = true;
        uint32 const humans = HumansIn(bg), bots = BotsOnTeam(bg, team);
        if (!best.bg || Battleground::PlacementBetter(humans, bots, best_humans, best_bots))
        {
            best.bg = bg; best.missing = missing; best.victims = std::move(victims);
            best_humans = humans; best_bots = bots;
        }
    };
    BattlegroundTemplate const* tmpl = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(bg_type_id));
    if (!tmpl) return best;
    for (int32 map_id : tmpl->MapIDs)
        for (Battleground* bg : sBattlegroundMgr->GetBGFreeSlotQueueStore(map_id))
            if (bg && uint32(bg->GetTypeID()) == bg_type_id) consider(bg);
    // full instances leave the free-slot store: reach them through the bots inside
    for (BotId id : Services::Lifecycle().snapshot_ids())
    {
        Player* bot = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        Battleground* bg = bot ? bot->GetBattleground() : nullptr;
        if (bg && uint32(bg->GetTypeID()) == bg_type_id) consider(bg);
    }
    return best;
}

} // namespace

bool BotQueueFiller::ClaimRunningBgForHuman(Player* human, uint32 bg_type_id, uint8 bracket, uint32* open_slots)
{
    if (open_slots) *open_slots = 0;
    if (!human || !Services::Config().behavior_bg_human_priority() || !Services::Config().bg_auto_join().make_room) return false;
    BattlegroundTemplate const* tmpl = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(bg_type_id));
    if (!tmpl || tmpl->IsArena()) return false;
    Team const team = human->GetTeam();
    uint32 const need = human->GetGroup() ? human->GetGroup()->GetMembersCount() : 1u;

    Placement placed = ChoosePlacement(team, need, bg_type_id, bracket);
    Battleground* best = placed.bg;
    uint32 const best_missing = placed.missing;
    std::string const& why = placed.why;
    if (!best)
    {
        TC_LOG_INFO("playerbot.v2", "[bg_queue] HumanPriority: {} queued bg_type={} bracket={}: no running instance can take {} slot(s), a normal fill follows;{}",
                    human->GetName(), bg_type_id, uint32(bracket), need, why.empty() ? " no running instance of this type found" : why);
        return false;
    }
    for (auto& w : g_human_waits)
        if (w.guid == human->GetGUID() && w.bg_type == bg_type_id)
            w.hold_until_ms = getMSTime() + 60u * 1000u;
    // Places left in that instance once the human's party is in, both teams: a side that is mostly empty (playtest, WSG
    // 2026-10-09: three Horde in the instance he was sent to) still needs the bot fill; the caller queues it capped to this.
    if (open_slots)
    {
        uint32 const open_total = best->GetFreeSlotsForTeam(ALLIANCE) + best->GetFreeSlotsForTeam(HORDE);
        *open_slots = open_total > need ? open_total - need : 0u;
    }
    TC_LOG_INFO("playerbot.v2", "[bg_queue] HumanPriority: {} queued bg_type={} bracket={}: joins running bg_inst={} ({} slot(s) free, {} bot(s) to leave, {} place(s) open after the party); no second instance",
                human->GetName(), bg_type_id, uint32(bracket), best->GetInstanceID(), need - best_missing, best_missing,
                open_slots ? *open_slots : 0u);
    return true;
}

bool BotQueueFiller::HasWaitingHuman(uint32 bg_type_id, uint8 bracket)
{
    for (auto const& w : g_human_waits)
    {
        if (w.bg_type != bg_type_id || (w.bracket != 0xFF && w.bracket != bracket)) continue;
        Player* human = ObjectAccessor::FindConnectedPlayer(w.guid);
        if (!human || human->InBattleground()) continue;
        BattlegroundQueueTypeId const qid = BattlegroundMgr::BGQueueTypeId(w.bg_type, BattlegroundQueueIdType::Battleground, false, 0);
        if (human->InBattlegroundQueueForBattlegroundQueueType(qid) && !human->IsInvitedForBattlegroundQueueType(qid))
            return true;
    }
    return false;
}

bool BotQueueFiller::HoldNewBattleground(uint32 bg_type_id, uint8 bracket)
{
    if (g_human_waits.empty()) return false;
    uint32 const now = getMSTime();
    for (auto const& w : g_human_waits)
        if (w.hold_until_ms && w.bg_type == bg_type_id && w.bracket == bracket && int32(w.hold_until_ms - now) > 0)
            return true;
    return false;
}

void BotQueueFiller::MakeRoomForRealPlayers(uint32 now_ms)
{
    if (g_human_waits.empty()) return;
    if (!Services::Config().bg_auto_join().make_room) return;
    constexpr uint32 kPassMs = 3u * 1000u;
    if (g_last_room_pass_ms && (now_ms - g_last_room_pass_ms) < kPassMs) return;
    g_last_room_pass_ms = now_ms;

    std::erase_if(g_room_victims, [&](auto const& kv) { return (now_ms - kv.second) > 30u * 1000u; });

    for (auto it = g_human_waits.begin(); it != g_human_waits.end();)
    {
        Player* human = ObjectAccessor::FindConnectedPlayer(it->guid);
        BattlegroundQueueTypeId const qid = BattlegroundMgr::BGQueueTypeId(
            it->bg_type, BattlegroundQueueIdType::Battleground, false, 0);
        if (!human || human->InBattleground() || !human->InBattlegroundQueueForBattlegroundQueueType(qid) ||
            human->IsInvitedForBattlegroundQueueType(qid))
        {
            it = g_human_waits.erase(it);                      // left, entered, or already has a place
            continue;
        }
        HumanBgWait& w = *it++;
        if (w.last_try_ms && now_ms - w.last_try_ms < 10u * 1000u) continue;
        // one bot has left for this type a moment ago: its place is the player's, wait for the invitation before asking another
        if (RoomRecentlyMade(w.bg_type, now_ms)) continue;

        Team const team = human->GetTeam();
        uint32 const need = human->GetGroup() ? human->GetGroup()->GetMembersCount() : 1u;
        BattlegroundTemplate const* tmpl = sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(w.bg_type));
        if (!tmpl || tmpl->MapIDs.empty()) continue;
        PVPDifficultyEntry const* diff = DB2Manager::GetBattlegroundBracketByLevel(tmpl->MapIDs.front(), human->GetLevel());
        if (!diff) continue;

        // Same choice as ClaimRunningBgForHuman: the instance with the most real players, then the most bots on the player's team.
        Placement placed = ChoosePlacement(team, need, w.bg_type, uint8(diff->GetBracketId()));
        Battleground* target = placed.bg;
        // no instance, or one has the places already (TC's queue invites the player there at once, an eviction elsewhere would be wasted)
        if (!target || placed.missing == 0 || placed.any_room) continue;
        uint32 const missing = placed.missing;
        auto victims = std::move(placed.victims);
        uint32 asked = 0;
        for (auto const& [key, victim] : victims)
        {
            if (asked >= missing) break;
            BotId const vid = BotId(victim->GetGUID().GetCounter());
            Intent leave{};
            leave.bot_id = vid;
            leave.body   = QueueIntent{BgLeaveIntent{}};
            Services::Intents(vid).push(std::move(leave));
            g_room_victims[victim->GetGUID().GetCounter()] = now_ms;
            ++asked;
            TC_LOG_INFO("playerbot.v2",
                "[bg_queue] MakeRoom: bot {} (score {}, L{}) leaves bg_inst={} type={} for real player {} (team {}, {} slot(s))",
                victim->GetName(), key.first, key.second, target->GetInstanceID(), w.bg_type, human->GetName(),
                team == ALLIANCE ? "ALLIANCE" : "HORDE", need);
        }
        if (asked) g_room_made_ms[w.bg_type] = now_ms ? now_ms : 1u;
        w.last_try_ms = now_ms;
    }
}

bool BotQueueFiller::RoomRecentlyMade(uint32 bg_type_id, uint32 now_ms)
{
    auto const it = g_room_made_ms.find(bg_type_id);
    return it != g_room_made_ms.end() && (now_ms - it->second) < 20u * 1000u;
}

} // namespace Playerbot::V2::Fleet
