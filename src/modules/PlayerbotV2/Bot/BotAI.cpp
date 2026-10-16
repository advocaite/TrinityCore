#include "BotAI.h"
#include "TownLife.h"
#include "BotSnapshotView.h"
#include "TradeRule.h"
#include "BotTrade.h"
#include "BotIntentEmitter.h"
#include "BotActivityTier.h"
#include "Group/GroupSnapshot.h"
#include "States/StateBase.h"
#include "fmt/format.h"
#include "GameTime.h"
#include "SharedDefines.h"  // SPELL_FAILED_NO_AMMO
#include "Log.h"  // TC_LOG_DEBUG for verbose-logging set_last_rule_fired
#include "PlayerbotAPI.h"  // API::CastFailName for [cast_fail]
#include "../Services.h"   // Services::Config() behaviour switches
#include "Util/ConfigReader.h"
#include "BotTextMgr.h"  // ike3 speech lines (hello / level-up)
#include <cstring>  // std::strcmp for cross-TU-safe watchdog exempt match
#include <cmath>    // std::sqrt for death-blackspot deflection geometry

namespace Playerbot {

BotAI::BotAI(BotId id, BotPersonality personality, BotRng rng)
    : bot_id_(id),
      personality_(std::move(personality)),
      rng_(rng)
{}

void BotAI::set_rally_point(uint32 map, float x, float y, float z)
{
    uint32 const now = GameTime::GetGameTimeMS();
    std::lock_guard<std::mutex> lk(fallback_mtx_);
    rally_point_ = { true, map, x, y, z, now ? now : 1u };
}

void BotAI::note_cast_outcome(uint32 code, uint32 spell_id, uint32 now_ms, std::string const& bot_name)
{
    // A warrior with no thrown weapon equipped fails Throw with EQUIPPED_ITEM, not NO_AMMO: the same "no ranged opener".
    if (code == uint32(SPELL_FAILED_NO_AMMO) ||
        (code == uint32(SPELL_FAILED_EQUIPPED_ITEM) && (spell_id == 2764 || spell_id == 2480 || spell_id == 7918 || spell_id == 7919)))
        last_no_ammo_ms_.store(now_ms ? now_ms : 1u, std::memory_order_relaxed);
    // A refused cast started no global cooldown: release the emitter's GCD hold (note_gcd_started).
    // NOT_READY keeps it: the GCD (or a cooldown) the stale snapshot missed is what refused it.
    if (code != 0 && code != uint32(SPELL_FAILED_NOT_READY))
        clear_gcd_started();
    CastFailWindow& w = cast_fail_;
    if (w.started_ms == 0)
        w.started_ms = now_ms ? now_ms : 1u;
    if (code != 0)
    {
        CastFailWindow::Reason* r = nullptr;
        for (uint8 i = 0; i < w.used; ++i)
            if (w.reasons[i].code == code) { r = &w.reasons[i]; break; }
        if (!r && w.used < CastFailWindow::kReasons)
        {
            r = &w.reasons[w.used++];
            *r = CastFailWindow::Reason{};
            r->code = code;
        }
        if (!r)
            ++w.other;
        else
        {
            ++r->count;
            bool seen = false;
            for (uint8 i = 0; i < r->nspells; ++i)
                if (r->spells[i] == spell_id) { seen = true; break; }
            if (!seen && r->nspells < CastFailWindow::kSpells)
                r->spells[r->nspells++] = spell_id;
        }
    }
    if (now_ms - w.started_ms < 10000u)
        return;
    if (w.used > 0 || w.other > 0)
    {
        std::string line;
        for (uint8 i = 0; i < w.used; ++i)
        {
            CastFailWindow::Reason const& r = w.reasons[i];
            if (!line.empty()) line += ", ";
            line += fmt::format("{} x{} (", API::CastFailName(r.code), r.count);
            for (uint8 k = 0; k < r.nspells; ++k)
                line += fmt::format("{}{}", k ? " " : "", r.spells[k]);
            line += ')';
        }
        if (w.other > 0)
            line += fmt::format(", other x{}", w.other);
        TC_LOG_INFO("playerbot.v2", "[cast_fail] {} {}s: {}", bot_name, (now_ms - w.started_ms + 500u) / 1000u, line);
    }
    w = CastFailWindow{};
    w.started_ms = now_ms ? now_ms : 1u;
}

void BotAI::set_last_rule_fired(char const* name)
{
    last_rule_fired_ = name;
    ++rule_fire_seq_;
    if (!name) return;

    // Watchdog accounting. Counts consecutive same-rule fires within a
    // 30 s window; if >= 60, suppress the rule for 60 s. Captures the
    // "rule fires forever because intent silently fails" wedge pattern
    // generically — see kRuleWatchdogFireCount comment in BotAI.h.
    // Excludes idle:wander (legitimate fast-firing fallback rule).
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        constexpr char const* kExempt[] = {
            "idle:wander",            // ambient fallback, fires every tick by design
            "idle:ambient_emote",     // throttled internally
            "combat:opener",          // pulses fast at engage
            // Intentional every-tick HOLD states (R6, 2026-06-03): the bot is
            // deliberately standing by — the core flies it (taxi) or carries it
            // (ship/zeppelin/elevator). These re-fire every tick BY DESIGN and
            // are not stuck loops; suppressing them is meaningless and made
            // idle:taxi_flight the single loudest line in Watchdog.log (568).
            "idle:taxi_flight",       // riding a taxi flight (DispatchIdle stand-down)
            "idle:on_transport_wait", // riding a ship/zeppelin/elevator
            // A12 (2026-06-07): two more deliberate every-tick HOLD states the R6
            // pass missed — the bot is standing AT a dock/FM node waiting for the
            // ship to dock / the flight master to enter scan range. Suppressing a
            // hold state for 60s is the wrong tool (it makes the bot abandon the
            // wait); the genuinely-unreachable-FM case is now handled by the A3
            // goal blacklist, not the watchdog.
            "idle:wait_for_transport",
            "idle:wait_for_flightmaster",
            // The group-rejoin journey's own holds (CoreFlowRules): aboard a
            // ship paused at a stop that is not its arrival, and at the dock
            // waiting for the ship. RejoinRide hands back itself when it
            // loses track of the ride.
            "idle:rejoin_ride",
            "idle:rejoin_wait_transport",
            // RpgRules: a world bot standing at an NPC it is visiting, for a
            // bounded 8-20 s (the rule ends the linger itself).
            "idle:rpg_linger",
        };
        bool exempt = false;
        for (char const* e : kExempt)
            if (std::strcmp(e, name) == 0) { exempt = true; break; }  // content, not pointer (cross-TU literals don't share an address without /OPT:ICF)
        // Combat APL rule fires use spell names directly (e.g. "Frostbolt",
        // "Slam", "Tiger Palm") — those legitimately re-fire many times
        // per second during combat and shouldn't be watchdog-suppressed.
        // We can't easily enumerate every spell name, but they all share
        // the property of NOT starting with a known prefix. Watchdog only
        // applies to engineered idle:/dead:/group: rule names.
        if (!exempt)
        {
            const bool watchable =
                (name[0] == 'i' && name[1] == 'd' && name[2] == 'l' && name[3] == 'e' && name[4] == ':')
             || (name[0] == 'd' && name[1] == 'e' && name[2] == 'a' && name[3] == 'd' && name[4] == ':')
             || (name[0] == 'g' && name[1] == 'r' && name[2] == 'o' && name[3] == 'u' && name[4] == 'p' && name[5] == ':');
            if (!watchable) exempt = true;
        }
        if (!exempt)
        {
            if (rule_watchdog_name_ == name &&
                now_ms - rule_watchdog_window_start_ms_ < kRuleWatchdogWindowMs)
            {
                if (++rule_watchdog_count_ == kRuleWatchdogFireCount)
                {
                    rule_watchdog_suppress_until_[name] = now_ms + kRuleWatchdogSuppressMs;
                    // Dedicated 'playerbot.v2.watchdog' logger — operators
                    // route this to its own log file via a worldserver.conf
                    // Appender entry, so wedge alerts don't get buried in
                    // the main Playerbot.log noise (which can be 100+ MB).
                    // Conf snippet (add to worldserver.conf):
                    //   Logger.playerbot.v2.watchdog=4,Console Watchdog
                    //   Appender.Watchdog=2,4,1,Watchdog.log,w
                    TC_LOG_WARN("playerbot.v2.watchdog",
                        "bot={} rule={} fires>={} window_ms={} suppress_ms={}",
                        uint32(bot_id_), name, kRuleWatchdogFireCount,
                        kRuleWatchdogWindowMs, kRuleWatchdogSuppressMs);
                }
            }
            else
            {
                rule_watchdog_name_ = name;
                rule_watchdog_count_ = 1;
                rule_watchdog_window_start_ms_ = now_ms;
            }
        }
    }

    // Ring-buffer the last ~16 rules for /history. Skip duplicates of the
    // most recent entry (don't spam with the same rule firing every tick).
    const size_t prev = (rule_history_head_ + kRuleHistoryCap - 1) % kRuleHistoryCap;
    if (rule_history_[prev] == name) return;
    rule_history_[rule_history_head_] = name;
    rule_history_head_ = (rule_history_head_ + 1) % kRuleHistoryCap;
    if (rule_history_size_ < kRuleHistoryCap) ++rule_history_size_;
    if (verbose_logging_)
        TC_LOG_DEBUG("playerbot.v2", "bot {} rule: {}", bot_id_, name);
}

bool BotAI::whisper_rate_check(std::string const& target,
                                std::string const& text,
                                uint32 now_ms)
{
    if (text.empty()) return true;
    // 64-bit FNV-1a — fast, sufficient for de-dup. We don't need
    // collision-resistance against an adversary, only that two
    // identical replies hash the same.
    constexpr uint64_t FNV_PRIME = 1099511628211ull;
    uint64_t h = 14695981039346656037ull;
    for (char c : text) { h ^= static_cast<uint8_t>(c); h *= FNV_PRIME; }
    constexpr uint32 kDedupWindowMs = 500;
    if (last_whisper_target_ == target &&
        last_whisper_hash_   == h &&
        (now_ms - last_whisper_ms_) < kDedupWindowMs)
        return false;
    last_whisper_target_ = target;
    last_whisper_hash_   = h;
    last_whisper_ms_     = now_ms;
    return true;
}

void BotAI::transition_to(BotState s)
{
    if (s == state_) return;
    prev_state_ = state_;
    state_ = s;
    state_entered_ = Ms{0};   // Set externally by tick when it has 'now'
}

void BotAI::tick(BotSnapshotView snapshot,
                 GroupSnapshotView group,
                 BotIntentEmitter& emit)
{
    ++tick_count_;

    // --- Watchdog movement-progress credit ---
    // The per-rule watchdog (note_rule_fired) counts same-rule re-fires and
    // suppresses a rule after kRuleWatchdogFireCount, but has no position
    // signal — so a bot legitimately walking a LONG chunked route (e.g.
    // idle:pursue_quest_goal every tick across a 600y+ trek out of a starter
    // zone) trips it after ~15s and gets yanked off-course by the high-priority
    // idle:watchdog_escape. Credit real movement: if the bot has moved
    // meaningfully (net) since the anchor, it's PROGRESSING, not wedged — clear
    // the watchdog and re-anchor here, BEFORE the rules dispatch (so this tick's
    // WatchdogGate sees the cleared suppress). Oscillation-in-place (no net
    // movement from the anchor) never reaches this, so a genuinely stuck bot
    // still trips the watchdog and gets its escape. 15y > snapshot position
    // jitter, and a walking bot (~7y/s) re-anchors every ~2s — far inside the
    // 60-fires/15s suppress threshold, so pursue keeps owning the trek.
    {
        float wx, wy, wz; snapshot.position(wx, wy, wz);
        const uint32 wnow = snapshot.published_at_ms();
        const uint32 pbc  = snapshot.path_blocked_count();

        // (a) NET POSITION progress: bot moved >= 15y from the anchor.
        constexpr float kWatchdogProgressY = 15.0f;
        const float adx = wx - watchdog_anchor_x_;
        const float ady = wy - watchdog_anchor_y_;
        const bool moved = !watchdog_anchor_set_ ||
            (adx * adx + ady * ady) >= kWatchdogProgressY * kWatchdogProgressY;
        if (moved)
        {
            watchdog_anchor_x_   = wx;
            watchdog_anchor_y_   = wy;
            watchdog_anchor_set_ = true;
        }

        // (b) MOVE-SUCCESS: the bot's pathing isn't failing. The escape (a 50y
        // random teleport) is only appropriate for a genuinely NoPath-trapped
        // bot; for a bot whose moves SUCCEED but re-fires the same rule a lot
        // (long chunked trek, or two rules spline-thrashing every tick so the
        // MotionMaster spline is reset before the bot traverses it — net
        // displacement pinned near zero), the escape just disrupts it. Credit
        // move-success: only consider the bot path-wedged when the global
        // path_blocked_count grew by >= 3 within a ~10s window (the same mark
        // pursue/walk rules use via check_anchor_wedge). Re-baseline the window.
        if (wnow - watchdog_blocks_window_ms_ > 10000u)
        {
            watchdog_blocks_baseline_  = pbc;
            watchdog_blocks_window_ms_ = wnow;
        }
        const uint32 blocks_grew =
            (pbc > watchdog_blocks_baseline_) ? (pbc - watchdog_blocks_baseline_) : 0u;
        const bool pathing_failing = blocks_grew >= 3u;

        if (moved || !pathing_failing)
            clear_rule_watchdog();
    }

    // Transport boarding-Z latch. The elevator exit rule needs to
    // know what Z the bot was at when it boarded so the "did the
    // platform carry me to a different floor" decision works. Latch
    // on false→true transition; clear on true→false. Ships pass
    // through this latch too — harmless since the exit rule gates
    // on metadata kind, not raw Z.
    // Normalise newly-summoned pets to defensive. Aggressive mode (the
    // server default for hunter pets) makes the pet pull every mob in
    // its 25y leash radius — breaking CC, dragging adds onto the group,
    // chasing respawns out of LoS — all things that actively hinder
    // bot progress. Detect any pet-guid transition (Empty→guid or
    // guid→different-guid) and fire one PetSetReactStateIntent{1}.
    // The intent is GCD-free so it doesn't compete with combat casts.
    {
        const ObjectGuid cur_pet = snapshot.pet_guid();
        if (!cur_pet.IsEmpty() && cur_pet != last_known_pet_guid_)
        {
            emit.pet_set_react_state(1);   // REACT_DEFENSIVE
        }
        last_known_pet_guid_ = cur_pet;
    }

    // Pet taunt autocast. Growl / Torment / Suffering on autocast rip mobs
    // off the group's tank (the owner's raid change: pet Growl off in groups).
    // Off while the bot is grouped with another member who tanks (role Tank,
    // or the coordinator's main tank) and the bot is not a tank itself; the
    // single-target taunts back on when solo or when the bot tanks (an area
    // taunt like Suffering is only ever switched off). Re-applied whenever
    // the pet changes; the snapshot carries the live autocast state, so a
    // toggle is only re-emitted if it has not shown up after 2s.
    {
        auto const& ps = snapshot.raw().pet;
        const ObjectGuid pg = ps.pet_guid;
        if (!pg.IsEmpty() && ps.pet_alive && ps.pet_taunt_count > 0)
        {
            auto const& po = snapshot.raw().pve_order;
            const bool me_tank = effective_role(snapshot) == Role::Tank ||
                                 (po.active && po.tank_duty != 0);
            GroupMemberSummary const* other_tank = nullptr;
            if (!me_tank && group.exists())
                if (auto const* mems = group.members())
                    for (auto const& m : *mems)
                    {
                        if (m.guid == snapshot.guid() || !m.online) continue;
                        if (m.role == Role::Tank || (po.active && m.guid == po.main_tank))
                        { other_tank = &m; break; }
                    }
            const int8 want = other_tank ? 0 : 1;
            const uint32 now_pt = snapshot.published_at_ms();
            const bool changed = pg != pet_taunt_pet_ || want != pet_taunt_want_;
            if (changed || now_pt - pet_taunt_emit_ms_ >= 2000u)
            {
                bool emitted = false;
                for (uint8 i = 0; i < ps.pet_taunt_count; ++i)
                {
                    const bool on   = (ps.pet_taunt_autocast_mask >> i) & 1u;
                    const bool area = (ps.pet_taunt_area_mask >> i) & 1u;
                    const bool desired = want == 1 ? (area ? on : true) : false;
                    if (on == desired) continue;
                    emit.pet_toggle_autocast(ps.pet_taunt_spells[i], desired);
                    emitted = true;
                }
                if (emitted)
                    pet_taunt_emit_ms_ = now_pt;
                if (changed)
                {
                    if (emitted)
                        TC_LOG_INFO("playerbot.v2", "[pet_taunt] {} {} taunt autocast {}{}",
                            snapshot.name(), ps.pet_name, want ? "on" : "off",
                            other_tank ? " (grouped with tank " + other_tank->name + ")"
                                       : std::string(me_tank ? " (bot is the tank)" : " (no tank in group)"));
                    pet_taunt_pet_  = pg;
                    pet_taunt_want_ = want;
                }
            }
        }
    }

    if (snapshot.on_transport() && !was_on_transport_)
    {
        float bx, by, bz; snapshot.position(bx, by, bz);
        elevator_boarding_z_   = bz;
        transport_prev_z_      = bz;
        transport_z_stable_ms_ = 0;
        was_on_transport_      = true;
    }
    else if (!snapshot.on_transport() && was_on_transport_)
    {
        elevator_boarding_z_   = 0.f;
        transport_prev_z_      = 0.f;
        transport_z_stable_ms_ = 0;
        was_on_transport_      = false;
    }
    else if (snapshot.on_transport())
    {
        // Track Z stability while attached. The snapshot's
        // transport_stopped is always-true for type-11 elevators
        // (snapshot builder dynamic_casts to type-15 Transport class
        // and falls back to true when that fails), so we proxy
        // "platform at rest" with "world-Z hasn't moved this tick".
        // Reset stable_ms on any Z delta ≥ 0.3y; otherwise accumulate
        // the snapshot's tick interval. The elevator step_off rule
        // requires ≥500ms stable before firing, so it cannot step off
        // mid-ascent at the moment |bz - boarding_z| first crosses
        // 15y — it waits until the platform has settled at the next
        // stop frame.
        float bx, by, bz; snapshot.position(bx, by, bz);
        const float dz = std::fabs(bz - transport_prev_z_);
        if (dz < 0.3f)
        {
            // Snapshot publishes at ~5 Hz under load; clamp the
            // per-tick increment to 1000ms so a hitched snapshot
            // doesn't fake long stability.
            const uint32 inc = 200;
            transport_z_stable_ms_ =
                std::min<uint32>(transport_z_stable_ms_ + inc, 60000u);
        }
        else
        {
            transport_z_stable_ms_ = 0;
        }
        transport_prev_z_ = bz;
    }

    // Per-zone activity accumulator. Used by idle:rebind_hearth_activity
    // to detect zones the bot has "settled into" (cumulative >30min) and
    // rebind hearth there. Skip while in BG/dungeon (those are transient
    // and shouldn't accumulate hearth-relevant time). Delta is bounded
    // by the bot tick interval (typically 200-1000ms).
    if (snapshot.is_alive() && !snapshot.in_battleground() && !snapshot.is_in_dungeon())
    {
        const uint32 now_ms = snapshot.published_at_ms();
        if (last_activity_tick_ms_ != 0)
        {
            uint32 dt = now_ms - last_activity_tick_ms_;
            // Cap dt at 60s to absorb logout/login gaps that would
            // otherwise dump an hour of "activity" into the current
            // zone on the post-login first tick.
            if (dt > 60u * 1000u) dt = 60u * 1000u;
            note_zone_activity(snapshot.zone_id(), dt);
        }
        last_activity_tick_ms_ = now_ms;
    }
    else
    {
        last_activity_tick_ms_ = 0;  // pause accumulator
    }

    // Death-spiral memory clears once the gear is healthy again — a successful
    // repair (or fresh upgrades) is the natural end of a broken-gear spiral.
    // Cheap snapshot read; only matters while a spiral is actually armed.
    if (consecutive_same_spot_deaths() != 0 && snapshot.is_alive() &&
        snapshot.lowest_equipped_durability_pct() >= 90)
        reset_death_spiral();


    // 1. Reactive transitions driven by snapshot (cheap, no events).
    // Routed through transition_to() so previous_state() tracks correctly.
    const bool prev_alive = (state_ != BotState::Dead && state_ != BotState::Resurrecting);
    // A raid duty tank the PvE plan gave an engaged mob (PveOrder::tank_target) is
    // fighting even while no mob has hit it yet: an off tank 40-60y from the pack
    // stayed in Idle for the whole Firelord fight (MC 2026-10-07: "Zorinero 100%>-
    // (assigned Firelord)" for 160 s, [tank_advance] gate=off_tank_duty) because
    // only the InCombat state starts the attack on the assigned mob, and that
    // state only began once the bot itself was in combat. Same in reverse: the
    // bot stays in InCombat until its assignment ends.
    PveOrder const& duty_po = snapshot.raw().pve_order;
    const bool raid_tank_on_duty = snapshot.is_alive() && !snapshot.in_battleground() &&
        duty_po.active && !duty_po.tank_target.IsEmpty() && effective_role(snapshot) == Role::Tank;
    if (!snapshot.is_alive() && state_ != BotState::Dead && state_ != BotState::Resurrecting)
    {
        // Death transition. When dying inside an instance, count
        // toward this run's per-bot deaths tally for /diag visibility.
        if (snapshot.is_in_instance())
            note_dungeon_death();
        // Open-world death-spiral memory: ONLY out in the world (not in a
        // dungeon/raid instance, not in a BG). A normal dungeon/raid wipe is a
        // hard fight, not a broken-gear spiral, so it must never arm the
        // counter. Records the death position so repeated deaths in the same
        // spot escalate the State_Dead / State_InCombat escape.
        if (!snapshot.is_in_instance() && !snapshot.in_battleground())
        {
            note_open_world_death(snapshot.raw().position.x,
                                  snapshot.raw().position.y,
                                  snapshot.raw().position.z,
                                  snapshot.published_at_ms());
            // Owner idea (2026-06-22): after repeated deaths in the same spot, mark
            // it a travel blackspot so movement routes AROUND it next time instead
            // of walking back into the killing field. Quest stays valid.
            if (consecutive_same_spot_deaths() >= kBlackspotDeathThreshold)
                arm_death_blackspot(snapshot.raw().position.x,
                                    snapshot.raw().position.y,
                                    snapshot.published_at_ms());
        }
        // Fresh-death entry: clear the per-death recovery latches. These are
        // otherwise reset ONLY in DispatchDead's revival block (State_Dead.cpp),
        // which is gated on the snapshot observing is_alive==true. A bot that
        // reclaims its corpse / spirit-resurrects revives at 50% HP inside the
        // camp that killed it and frequently re-dies before the next snapshot —
        // and dead bots tick at Hibernate cadence (~2s), so the brief alive
        // frame is never observed. The latch corpse_recovery_emitted_ then stays
        // true across the undetected revive->re-death, DispatchDead skips its
        // release branch (guarded by !corpse_recovery_emitted()), is_ghost()
        // never flips, and the bot wedges in dead:waiting_release forever
        // (observed: Bramwell L4 Elwynn). The only reliable signal of a NEW
        // death is this Idle/Combat->Dead transition edge, so re-baseline the
        // recovery FSM here. (The revival-block reset stays as belt-and-braces.)
        set_corpse_recovery_emitted(false);
        set_release_pending_at_ms(0);
        set_ghost_since_ms(0);
        set_rez_acked(false);
        set_reincarnation_attempted(false);
        set_reincarnation_attempt_ms(0);
        set_corpse_run_last_dist(-1.0f);
        set_dead_watchdog_ms(0);   // fresh bounded-recovery window
        reset_death_recovery_extras();     // wipe/wait logs, portal run
        // The owner's release order is kept: one that reached the bot between
        // its real death and this edge (a snapshot or more later) must still
        // release it. Only an order from before this death is dropped: it was
        // left by a revival this bot logic never observed. The death instant is
        // Module::OnDeath's stamp; if that is missing or plainly from an
        // older death, fall back to this edge minus a small slack.
        if (const uint32 order = release_order_ms())
        {
            constexpr uint32 kDeathEdgeSlackMs = 10000u;
            const uint32 edge_ms = snapshot.published_at_ms();
            uint32 died = died_at_ms();
            if (died == 0 || died > edge_ms || edge_ms - died > 60000u)
                died = edge_ms > kDeathEdgeSlackMs ? edge_ms - kDeathEdgeSlackMs : 0u;
            if (order < died && clear_release_order_before(died - 1u))
                TC_LOG_INFO("playerbot.v2",
                    "[death-recovery] {} died; dropped a stale release order from {}s before this death",
                    snapshot.name(), (died - order) / 1000u);
            else if (order >= died)
                TC_LOG_INFO("playerbot.v2",
                    "[death-recovery] {} died with a release order already received ({}s ago): releasing",
                    snapshot.name(), edge_ms >= order ? (edge_ms - order) / 1000u : 0u);
        }
        transition_to(BotState::Dead);
    }
    else if (snapshot.is_alive() && state_ == BotState::Dead)
    {
        // Post-revive demotion. DispatchDead's own is_alive block rarely runs
        // (this demotes first), so the revival is logged and the per-death
        // extras cleared here. Rebuff / eat / drink then run from the idle
        // upkeep rules (idle:self_buff, idle:group_utility, idle:ooc_eat_food).
        TC_LOG_INFO("playerbot.v2", "[death-recovery] {} revived at ({:.1f},{:.1f},{:.1f}) map {} hp {}%",
            snapshot.name(), snapshot.raw().position.x, snapshot.raw().position.y,
            snapshot.raw().position.z, snapshot.map_id(), snapshot.hp_pct());
        reset_death_recovery_extras();
        // This death's release order is spent. Clear only orders latched up to
        // this snapshot: a later one is for a newer death (OrderRelease only
        // latches on a dead bot) and the next death edge keeps it.
        clear_release_order_before(snapshot.published_at_ms());
        rejoin_run_ = RejoinRun{};
        transition_to(BotState::Idle);
    }
    // LoggingIn is the bootstrap state. Once we have a live, alive snapshot
    // the bot is in-world and ready — promote to Idle so the regular
    // dispatch chain runs. Without this, bots that login outside combat
    // stay in LoggingIn (a stub dispatcher) forever.
    else if (snapshot.is_alive() && state_ == BotState::LoggingIn)
        transition_to(BotState::Idle);
    // Treat any active attacker as "in combat" — TC's combat-flag
    // propagation can lag by a tick when a mob aggro-transfers (e.g.
    // tank dies, mob picks a new threat target). Without this, the
    // bot's state stays Idle for that tick, the follow rule fires,
    // and the bot walks away from a mob that is actively hitting it.
    // Observed 2026-05-15: "tank dies, group walks away, mobs follow
    // and kill them".
    else if ((snapshot.in_combat() || !snapshot.raw().combat.attackers.empty() || raid_tank_on_duty) &&
             state_ != BotState::InCombat &&
             state_ != BotState::Dead)
    {
        set_combat_entered_ms(snapshot.published_at_ms());
        transition_to(BotState::InCombat);
    }
    else if (state_ == BotState::InCombat &&
             !snapshot.in_combat() &&
             snapshot.raw().combat.attackers.empty() && !raid_tank_on_duty)
    {
        // Combat just ended without dying — record a "kill" for the
        // tank-pull-pacing cooldown and increment the per-run dungeon
        // contribution counter when in an instance. Heuristic: surviving
        // a fight = group landed a kill. ONLY for fights that lasted
        // ≥1.5s: TC's combat flag flaps (aggro transfers, brief proximity
        // flags) produce InCombat→Idle transitions every few seconds, and
        // each one reset the post-kill pacing timer — a tank standing in
        // a flap-prone spot never saw the timer expire and tank_advance
        // starved (2026-06-11 Stockades stall).
        const uint32 fight_ms =
            snapshot.published_at_ms() - combat_entered_ms();
        if (combat_entered_ms() != 0 && fight_ms >= 1500)
        {
            note_kill(snapshot.published_at_ms());
            if (prev_alive && snapshot.is_in_instance())
                note_dungeon_kill();
        }
        transition_to(BotState::Idle);
    }

    // Map-change reset for per-run contribution counters. When the bot
    // crosses a map boundary (entering or leaving an instance), the
    // previous run's kill/death tally should reset so /diag reflects
    // the CURRENT run rather than lifetime totals. Last-seen map_id
    // primes on first observation so a fresh login doesn't reset
    // before any kills accrue.
    const uint32 cur_map = snapshot.map_id();
    if (last_seen_map_id_ != cur_map)
    {
        if (last_seen_map_id_ != 0)
        {
            reset_dungeon_contribution();
            // Waypoint index is per-run; reset on map change so the next
            // dungeon entry starts at progression_waypoints[0] rather
            // than continuing from wherever the previous run left off.
            set_dungeon_waypoint_index(0);
            // A map change means the bot left the spiralling area (or was
            // graveyard-ported away) — clear the open-world death-spiral memory
            // so deaths on the new map start a fresh count.
            reset_death_spiral();
        }
        // Drop stale movement on ANY real map change — including 0 -> N, since
        // map 0 is a valid map (the dungeontest staging area lives there), so
        // this clear is gated by its own primed flag rather than the != 0
        // contribution guard above. A cross-map teleport (LFG dungeon port, BG,
        // hearth, areatrigger) does NOT clear the server-side MotionMaster, so a
        // POINT/CHASE/FOLLOW generator computed on the OLD map survives and
        // re-splines toward its now-meaningless target, walking the bot
        // dead-straight into the void (observed: Dunghealer crawling toward the
        // map-0 staging XY (-8985,511) on map 36 for 13+ min). clear_generators
        // =true because StopMoving() alone leaves the generator to re-issue;
        // also drop the regroup convergence sample so a pre-port baseline can't
        // false-trigger a divergence re-issue on the new map.
        if (map_change_primed_)
        {
            reset_regroup_tracking();
            clear_dungeon_cross();   // a cross-target on the old map must not survive
            emit.stop_movement(/*clear_generators*/ true);
        }
        map_change_primed_ = true;
        last_seen_map_id_  = cur_map;
    }

    // Owner-led / small-party-in-a-raid-map autopilot suspension, settled before any
    // state reads dungeon_active() this tick.
    update_dungeon_autopilot_hold(snapshot, group);

    // 2. Primary state dispatch.
    dispatch_primary(snapshot, group, emit);

    // 3. Cross-cutting layers.
    dispatch_layers(snapshot, group, emit);
}

void BotAI::update_dungeon_autopilot_hold(BotSnapshotView s, GroupSnapshotView g)
{
    // An owner Stay / Hold: the dungeon autopilot (converge, rejoin, advance, pull) walks nobody who was told to stay.
    owner_hold_ = holding_position(s.published_at_ms());
    // A companion's party that is not a raid group, inside a raid map (MC 2026-10-06: the owner's
    // 6-man party), never runs the raid autopilot: the tank advanced and pulled Molten Giants
    // the party could not survive while the owner was still forming the raid. It follows its owner
    // (State_InGroup) and defends (InCombat); converting to a raid group re-arms the run.
    // Playerbot.Behavior.OwnerFollowHold off: no hold, and a follow order left over from before
    // the switch was turned off is released.
    const bool follow_hold_on = Services::Config().behavior_owner_follow_hold();
    if (!follow_hold_on)
        set_owner_follow_order(false);
    if (!Services::Config().behavior_fallback())   // Playerbot.Behavior.Fallback off: drop a leftover plan
        end_fallback();
    const bool hold = follow_hold_on && s.is_in_raid() && !s.in_battleground() &&
                      s.is_companion_with_owner() && g.exists() && !g.is_raid();
    if (hold != raid_map_party_hold_)
    {
        raid_map_party_hold_ = hold;
        TC_LOG_INFO("playerbot.v2", "[owner_order] {} {} the raid autopilot on map {}: {}",
            s.name(), hold ? "suspends" : "resumes", s.map_id(),
            hold ? "a party, not a raid group, in a raid map (follows its owner and defends)"
                 : (g.exists() && g.is_raid() ? "the group is a raid group" : "left the raid map / owner's group"));
    }

    // The owner engaging a mob himself no longer releases his follow order or opens an autopilot window (playtest, Deadmines
    // 2026-10-10: the tank ran off and pulled everything in the first room once he attacked anything). A suspended bot helps on
    // the owner's target and defends the group (State_InGroup assist chain, the tank included); only an order (attack / pull /
    // resume / aggressive, release_owner_lead) hands the pulls back to the autopilot.

    // Playerbot.Behavior.DungeonFlow / DungeonAutopilot: a human leads this 5-man, so the bots follow him and the autopilot
    // stands down (dungeon_autopilot_suspended). Every owner order (release_owner_lead) opens a release
    // window; a human who is dead, offline, on another map or beyond 80 y for 30 s hands the run back to the autopilot.
    {
        bool lead_hold = false;
        if (Services::Config().behavior_dungeon_flow() && Services::Config().behavior_dungeon_autopilot() &&
            s.is_in_dungeon() && !s.is_in_raid() && !s.in_battleground() && g.exists() && !g.leader_is_bot())
        {
            float bx = 0.f, by = 0.f, bz = 0.f;
            s.position(bx, by, bz);
            bool human_near = false;
            if (auto const* mems = g.members())
                for (GroupMemberSummary const& m : *mems)
                    if (!m.is_bot && m.online && m.is_alive && m.map_id == s.map_id())
                    {
                        float const dx = m.x - bx, dy = m.y - by;
                        if (dx * dx + dy * dy <= 80.f * 80.f) { human_near = true; break; }
                    }
            uint32 const now_ms = s.published_at_ms();
            if (human_near)
                human_seen_ms_ = now_ms ? now_ms : 1u;
            bool const present = human_seen_ms_ != 0 && now_ms - human_seen_ms_ < 30000u;
            int64_t const steady = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            lead_hold = present && steady >= human_release_until_.load(std::memory_order_relaxed);
        }
        else
            human_seen_ms_ = 0;
        if (lead_hold != human_lead_hold_)
        {
            human_lead_hold_ = lead_hold;
            TC_LOG_INFO("playerbot.v2", "[dungeon_flow] {} {} the dungeon autopilot on map {}: {}",
                s.name(), lead_hold ? "suspends" : "resumes", s.map_id(),
                lead_hold ? "a human leads (follows him, advances/pulls on his orders)" : "order / fight window or no human near");
        }
    }

    // A fallback is anchored on its map: leaving it (zone-out, graveyard port) ends the order.
    if (fallback_active() && fallback_plan().map != s.map_id() && end_fallback())
        TC_LOG_INFO("playerbot.v2", "[owner_order] {} drops the fallback order: left the rally's map", s.name());
}

void BotAI::dispatch_primary(BotSnapshotView s, GroupSnapshotView g,
                              BotIntentEmitter& em)
{
    switch (state_)
    {
        case BotState::LoggingIn:    States::DispatchLoggingIn(*this, s, g, em);    break;
        case BotState::LoggingOut:   States::DispatchLoggingOut(*this, s, g, em);   break;
        case BotState::Idle:         States::DispatchIdle(*this, s, g, em);         break;
        case BotState::Travelling:   States::DispatchTravelling(*this, s, g, em);   break;
        case BotState::Questing:     States::DispatchQuesting(*this, s, g, em);     break;
        case BotState::InCombat:     States::DispatchInCombat(*this, s, g, em);     break;
        case BotState::Looting:      States::DispatchLooting(*this, s, g, em);      break;
        case BotState::Dead:         States::DispatchDead(*this, s, g, em);         break;
        case BotState::Resurrecting: States::DispatchResurrecting(*this, s, g, em); break;
        default:
            // Cross-cutting states are not primary; treat as Idle.
            States::DispatchIdle(*this, s, g, em);
            break;
    }
}

void BotAI::dispatch_layers(BotSnapshotView s, GroupSnapshotView g,
                             BotIntentEmitter& em)
{
    // Always-on social auto-responses. Run before in-group / state dispatch
    // so they fire regardless of whether the bot is grouped or what primary
    // state it's in. Cheap: each is a single snapshot-flag check + at most
    // one intent emit.
    //
    // Duel challenges: friendly initiators (group / guild / social-friend)
    // get auto-accepted so owner sparring works without a manual whisper;
    // strangers get auto-declined to avoid being trolled into PvP. The
    // friend flag is resolved on the world thread by the snapshot builder
    // (Player::GetSocial::HasFriend + group/guild membership), so the bot logic
    // worker just inspects a single bool here.
    if (s.has_duel_request())
    {
        // Context filter: never auto-accept duels inside an active
        // BG / arena / dungeon / raid. Server-side duel handlers usually
        // reject those anyway, but the bot's emit still costs a packet
        // round-trip + clutters the duel-state machine while the
        // instance is gating combat. Auto-decline regardless of friend
        // status; the owner can manually accept once the bot is out.
        const bool in_instance_or_pvp =
            s.in_battleground() || s.is_in_dungeon() || s.is_in_raid();
        // Playerbot.Behavior.PoliteWorldBots: a world bot (no owner) treats a REAL stranger like a person would:
        // it says no with a few words, or now and then says yes when the levels are close and nothing is fighting.
        const bool polite = Services::Config().behavior_polite_world_bots() && !s.has_owner_character() &&
                            !s.in_group() && s.duel_initiator_is_real() && !s.duel_initiator_name().empty();
        if (in_instance_or_pvp)
        {
            if (polite)
                em.whisper(s.duel_initiator_name(), "sorry, can't duel in here");
            em.duel_decline();
        }
        else if (s.duel_initiator_is_friend())
            em.duel_accept();
        else if (!s.duel_initiator_is_real() && !s.has_owner_character() && !s.in_group()
                 && V2::TownLife::PactHolds(s.bot_id(), s.duel_initiator().GetCounter(), s.published_at_ms()))
            em.duel_accept();   // another world bot asked for this duel (idle:town_duel)
        else if (polite)
        {
            // Answer a moment after the request (0-2 s, per bot), once; the verdict is stable for a minute so a
            // request seen on two ticks does not flip.
            const uint32 now_ms = s.published_at_ms();
            if ((now_ms / 1000u + uint32(s.bot_id())) % 3u == 0u)
            {
                const int32 diff = int32(s.level()) - int32(s.duel_initiator_level());
                const uint32 h = ((uint32(s.bot_id()) + 1u) * 2654435761u) ^
                                 (uint32(s.duel_initiator().GetCounter()) * 2246822519u) ^ ((now_ms / 60000u) * 3266489917u);
                const bool close = diff >= -5 && diff <= 5;
                const bool fit = s.hp_pct() >= 70 && !s.in_combat() && s.is_alive() && !s.is_moving();
                if (close && fit && ((h >> 9) % 3u) == 0u)
                {
                    em.duel_accept();
                    em.say_world("alright, you're on");
                }
                else
                {
                    static constexpr char const* kNo[] = {
                        "sorry, not up for a duel", "no thanks, not right now", "maybe later", "sorry, can't right now",
                    };
                    static constexpr char const* kFar[] = {
                        "sorry, we're not close enough in level", "ha, not a fair fight, sorry",
                    };
                    em.whisper(s.duel_initiator_name(), close ? kNo[(h >> 5) % 4u] : kFar[(h >> 5) % 2u]);
                    em.duel_decline();
                }
            }
        }
        else
            em.duel_decline();
    }
    // The duel is on (the countdown ran out): attack the opponent, so an accepted duel is a fight and not a bot
    // standing still being hit. start_attack refuses an invalid target; the guard only avoids re-arming the swing.
    if (s.duel_in_progress() && !s.duel_opponent().IsEmpty() && s.victim() != s.duel_opponent() && s.is_alive())
        em.start_attack(s.duel_opponent());
    // Trades opened on the bot are likewise declined automatically. Window
    // stays open server-side until either side cancels — without this the
    // bot would sit there with the dialog up indefinitely.
    if (s.has_trade_request() &&
        AcceptsTradeFrom(Services::Config().behavior_accept_owner_trade(), s.trade_partner_is_real(),
                         !s.raw().owner_guid.IsEmpty() && s.raw().social_events.trade_partner_guid == s.raw().owner_guid,
                         s.raw().social_events.trade_partner_in_my_group))
    {
        // A gift trade from the owner or a player in the bot's own group or raid (gear, potions, money): accepted once he has
        // accepted, see API::trade_with. Gear that is an upgrade is a gift and worn by the auto-equip rules afterwards; anything
        // else he puts in is bought at the fair price. Everyone else falls to the cases below.
        em.trade_accept_owner(s.raw().social_events.trade_partner_guid);
    }
    else if (s.has_trade_request() && s.trade_partner_is_real() && !s.has_owner_character() && !s.in_group())
    {
        // A free-roaming bot trades with anyone at the fair price: it buys what he puts in, and sells what it listed to him
        // (whisper "what do you have", then "buy 2"). See BotTrade.h.
        em.trade_bargain(s.raw().social_events.trade_partner_guid);
    }
    else if (s.has_trade_request())
    {
        // A companion that belongs to someone else's group, or a trade with another bot: cancelled at once, silently.
        em.trade_decline();
    }

    // Bag room: a bot that never leaves its group never reaches a vendor, so its bags only fill and it can neither take loot
    // nor a trade. Clears bag rot when fewer than BotTrade::kKeepFreeSlots slots are free (at most every 30 s, out of combat).
    if (!s.in_combat() && s.is_alive() && s.raw().bags.bag_free_slots < Playerbot::BotTrade::kKeepFreeSlots &&
        s.published_at_ms() - last_bag_room_ms_ >= 30000u)
    {
        last_bag_room_ms_ = s.published_at_ms();
        em.make_bag_room(Playerbot::BotTrade::kKeepFreeSlots);
    }

    if (s.in_group())
        States::DispatchInGroup(*this, s, g, em);

    // Level-up announcement. Compares the snapshot's current level against
    // the last value we observed; on a strict increase, drops a /p chat ping
    // so the rest of the group sees the ding. Suppressed on the first tick
    // (last_seen_level_ == 0) to avoid announcing on every login. Only fires
    // when grouped — solo bots ding silently. Single emit per ding even if
    // the bot levels multiple times in the same tick (rare): the announce
    // text reports the new level only.
    {
        const uint8 cur = s.level();
        const uint8 prev = last_seen_level_;
        // Personality gate (parity with group-join greet below). Silent/
        // Terse bots level up silently; Normal+ announce. Without this,
        // bots configured as Silent still emit a /p Ding! on every level
        // — inconsistent with how the same bot stays quiet for greets.
        //
        // Ding stagger 2026-05-21: don't /say immediately. Capture the
        // level + an emit time 1.5–5.5s in the future, jittered by
        // bot_id, so a raid that levels on the same XP-grant doesn't
        // stack 25 identical /p Ding! lines inside one server tick.
        if (cur > 0 && prev > 0 && cur > prev && s.in_group()
            && personality_.verbosity >= Verbosity::Normal
            && pending_ding_level_ == 0)
        {
            const uint32 now_ms = s.published_at_ms();
            const uint32 jitter = 1500u + (uint32(s.bot_id()) * 2654435761u) % 4000u;
            pending_ding_level_     = cur;
            pending_ding_say_at_ms_ = now_ms + jitter;
        }
        if (cur != prev)
            last_seen_level_ = cur;

        // Fire the pending ding when its scheduled time arrives. Reset the
        // slot regardless of whether the announce went out (e.g. lost-group
        // edge case) so a future level still queues cleanly.
        if (pending_ding_level_ > 0)
        {
            const uint32 now_ms = s.published_at_ms();
            if (now_ms >= pending_ding_say_at_ms_)
            {
                if (s.in_group() && personality_.verbosity >= Verbosity::Normal)
                {
                    // ike3's level-up lines ("Ding!", "Yess, I am level 24!", ...): chance-gated,
                    // and one bot per group when several ding off the same kill.
                    std::string line = BotChatter::LevelUpLine(pending_ding_level_, s.cls(), s.race(),
                        s.zone_id(), s.area_id(), s.group_guid().GetCounter(), s.bot_id(), now_ms);
                    if (!line.empty())
                    {
                        em.say(std::move(line));
                        if (Services::Config().behavior_social_emotes() && !s.big_raid())
                            em.perform_emote(4 /*EMOTE_ONESHOT_CHEER*/);   // Playerbot.Behavior.SocialEmotes
                    }
                }
                pending_ding_level_     = 0;
                pending_ding_say_at_ms_ = 0;
            }
        }
    }

    // Group-join greet. When the bot transitions Empty → non-Empty group
    // (joined a new party/raid), drop a single /p chat to signal presence.
    // The first observation only primes the field; without that priming step
    // every bot would say "Hi!" once per server boot the moment the snapshot
    // first sees its existing group. Re-greets on subsequent group changes
    // so a bot that switches party-to-raid or rejoins a different group
    // greets each fresh group once. Solo bots (group_guid == Empty) reset
    // the greet priming so a future re-join greets cleanly.
    //
    // Personality-aware: Silent / Terse bots stay quiet. Roleplay bots get a
    // flavour-text variant ("Hail and well met!"). The intent is to keep the
    // social layer feeling individuated rather than uniform "Hi!" spam from
    // every bot in a fresh raid.
    {
        const ObjectGuid cur_grp = s.group_guid();
        if (!group_greet_primed_)
        {
            // First ever observation — capture and skip the greet.
            last_seen_group_ = cur_grp;
            group_greet_primed_ = true;
        }
        else if (cur_grp != last_seen_group_)
        {
            if (!cur_grp.IsEmpty() && personality_.verbosity >= Verbosity::Normal && !s.big_raid())
            {
                // ike3's hello lines; "Hello, lead the way!" style when it is a companion joining
                // its owner. One greeting per group per few seconds, so calling ten companions at
                // once does not produce ten hellos.
                std::string line = BotChatter::GreetLine(s.is_companion_with_owner(),
                    cur_grp.GetCounter(), s.bot_id(), s.published_at_ms());
                if (!line.empty())
                    em.say(std::move(line));
            }
            last_seen_group_ = cur_grp;
        }
    }

    // ike3 speech that no game event announces: companions' combat / ammo callouts and world
    // bots' General / Trade suggestions. Rate-limited inside; at most one chat intent.
    if (!s.big_raid())   // quiet in 40-man raids
        BotChatter::SpeechTick(*this, s, em);

    // Future: detect AtVendor / AtMailbox / AtAh / InInstance / Decorating
    // by inspecting position + nearby NPCs / objects + active interaction.
}

Role BotAI::effective_role(BotSnapshotView const& s) const
{
    if (role_override_ != Role::Unknown)
        return role_override_;
    // A tank-spec bot the raid's tank roster does not need plays DPS.
    if (s.raw().pve_order.active && s.raw().pve_order.raid_dps_tank && s.my_role() == Role::Tank)
        return Role::Dps;
    return s.my_role();
}

// --- Death blackspots (owner idea, 2026-06-22) ---------------------------------
void BotAI::arm_death_blackspot(float x, float y, uint32 now_ms)
{
    // Refresh an overlapping active spot; else reuse the oldest (most-expired) slot.
    int oldest = 0;
    uint32 oldest_exp = 0xFFFFFFFFu;
    for (int i = 0; i < 4; ++i)
    {
        DeathBlackspot& b = death_blackspots_[i];
        const float dx = x - b.x, dy = y - b.y;
        if (b.expiry_ms > now_ms && (dx * dx + dy * dy) <= b.r * b.r)
        {
            b.expiry_ms = now_ms + kBlackspotTtlMs;   // still dying here — extend
            return;
        }
        if (b.expiry_ms < oldest_exp) { oldest_exp = b.expiry_ms; oldest = i; }
    }
    death_blackspots_[oldest] = DeathBlackspot{ x, y, kBlackspotRadius, now_ms + kBlackspotTtlMs };
}

bool BotAI::in_death_blackspot(float x, float y, uint32 now_ms) const
{
    for (int i = 0; i < 4; ++i)
    {
        DeathBlackspot const& b = death_blackspots_[i];
        if (b.expiry_ms <= now_ms || b.r <= 0.f) continue;
        const float dx = x - b.x, dy = y - b.y;
        if (dx * dx + dy * dy <= b.r * b.r) return true;
    }
    return false;
}

bool BotAI::deflect_for_blackspot(float fx, float fy, float tx, float ty,
                                  uint32 now_ms, float& ox, float& oy) const
{
    for (int i = 0; i < 4; ++i)
    {
        DeathBlackspot const& b = death_blackspots_[i];
        if (b.expiry_ms <= now_ms || b.r <= 0.f) continue;
        const float dgx = tx - fx, dgy = ty - fy;
        const float glen = std::sqrt(dgx * dgx + dgy * dgy);
        if (glen < 1.0f) continue;
        const float ux = dgx / glen, uy = dgy / glen;
        const float vcx = b.x - fx, vcy = b.y - fy;
        const float dC = std::sqrt(vcx * vcx + vcy * vcy);
        // Standing INSIDE the blackspot: head straight out, away from its center.
        if (dC <= b.r)
        {
            const float awx = (dC > 0.01f) ? -vcx / dC : -ux;
            const float awy = (dC > 0.01f) ? -vcy / dC : -uy;
            ox = fx + awx * (b.r + 15.0f);
            oy = fy + awy * (b.r + 15.0f);
            return true;
        }
        const float proj = vcx * ux + vcy * uy;     // distance along the path to C
        if (proj <= 0.0f) continue;                  // blackspot is behind us
        const float perpx = vcx - proj * ux, perpy = vcy - proj * uy;
        const float perp  = std::sqrt(perpx * perpx + perpy * perpy);
        const float clear = b.r + 12.0f;
        if (perp >= clear) continue;                 // straight path already clears it
        // Deflect: offset the projected point perpendicular, AWAY from the center,
        // so the bot skirts the blackspot's edge while still advancing to the goal.
        float awx, awy;
        if (perp > 0.01f) { awx = -perpx / perp; awy = -perpy / perp; }
        else              { awx = -uy;           awy = ux; }   // center on the line: pick a side
        ox = fx + ux * proj + awx * clear;
        oy = fy + uy * proj + awy * clear;
        return true;
    }
    return false;
}

} // namespace Playerbot
