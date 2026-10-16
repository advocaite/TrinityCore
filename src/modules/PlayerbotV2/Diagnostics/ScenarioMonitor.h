// ScenarioMonitor - the invariant checks of the bot scenario tests (`.playerbot scenario run <name>`), as pure logic.
//
// A scenario drives a squad of bots through a piece of content with no client logged in. Once a second the runner turns each
// bot into a ScenarioSample and feeds them here; this file decides what counts as a failure and why, so a red run says
// "Scnmage stood 61 s at (x,y,z) while the group walked on" instead of leaving someone to watch it. Nothing in this file
// touches the world: it is tested in tests/game/ClassicScenarioMonitor.cpp.
//
// Checks (each reports once per bot per episode):
//   idle          alive, out of fight, not casting, not recovering, standing still for idle_ms while another bot of the group
//                 is walking (a bot waiting because the whole group waits is group_stalled, not idle)
//   stray         farther than stray_yards from the live tank for stray_ms (ran off / left behind)
//   out_of_bounds below the floor or outside the scenario's box (fell through the map, walked out of the dungeon)
//   wrong_map     on another map than the scenario's once the squad has entered
//   healer_oom    a healer under oom_pct mana for oom_ms
//   death         a bot died: the killer, the spell and the damage come from the [bot_death] data
//   group_stalled nobody moved stall_yards and nobody fought for stall_ms
//   wipe          every member dead (raids: every tank dead, when the scenario says so)
//   timeout       the success condition was not met in time
//   boss_reset    the goal boss was engaged and then evaded / reset
//   dps_hold      a DPS bot sat in a threat hold for dps_hold_ms while the raid was fighting

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Playerbot::Diagnostics {

enum class ScenarioRoleKind : std::uint8_t { Tank, Healer, Dps };

struct ScenarioSample
{
    std::uint64_t id = 0;
    std::string name;
    ScenarioRoleKind role = ScenarioRoleKind::Dps;
    bool alive = true;
    bool in_combat = false;
    bool casting = false;
    std::uint32_t map = 0;
    float x = 0.f, y = 0.f, z = 0.f;
    float hp_pct = 100.f;
    float mana_pct = -1.f;                 // -1 = no mana bar (warrior, rogue)
    std::uint32_t group_size = 0;          // members of the group this bot is in; 0 = in none
    std::string rule;                      // the rule the bot logic fired last (BotAI::last_rule_fired); empty = none
};

enum class ScenarioFailKind : std::uint8_t { Idle, Stray, OutOfBounds, WrongMap, HealerOom, Death, GroupStalled, Wipe, Timeout, SetupFailed, GroupBroken, BossReset, DpsHold, HealerNoRest, SupportMissing };

inline char const* ToString(ScenarioFailKind k)
{
    switch (k)
    {
        case ScenarioFailKind::Idle:         return "idle";
        case ScenarioFailKind::Stray:        return "stray";
        case ScenarioFailKind::OutOfBounds:  return "out_of_bounds";
        case ScenarioFailKind::WrongMap:     return "wrong_map";
        case ScenarioFailKind::HealerOom:    return "healer_oom";
        case ScenarioFailKind::HealerNoRest: return "healer_no_rest";
        case ScenarioFailKind::Death:        return "death";
        case ScenarioFailKind::GroupStalled: return "group_stalled";
        case ScenarioFailKind::Wipe:         return "wipe";
        case ScenarioFailKind::Timeout:      return "timeout";
        case ScenarioFailKind::SetupFailed:  return "setup_failed";
        case ScenarioFailKind::GroupBroken:  return "group_broken";
        case ScenarioFailKind::BossReset:    return "boss_reset";
        case ScenarioFailKind::DpsHold:      return "dps_hold";
        case ScenarioFailKind::SupportMissing: return "support_missing";
    }
    return "?";
}

struct ScenarioFailure
{
    ScenarioFailKind kind = ScenarioFailKind::Timeout;
    std::string bot;                       // empty = the group
    std::string detail;
    std::uint32_t at_ms = 0;               // since the run started
};

// What the [bot_death] line carries, handed over by the death hook.
struct ScenarioDeath
{
    std::uint64_t id = 0;
    std::string name;
    std::uint32_t map = 0;
    float x = 0.f, y = 0.f, z = 0.f;
    std::string killer = "-";
    std::string spell = "melee";
    std::int32_t damage = 0;
    std::string rule = "-";
};

// The goal boss as the runner reads it once a second.
struct ScenarioBossSample
{
    bool found = false;                    // the creature is on the map
    bool alive = false;
    bool in_combat = false;
    bool evading = false;                  // Creature::IsInEvadeMode
    float hp_pct = 100.f;
};

struct ScenarioBounds
{
    bool enabled = false;
    float min_x = 0.f, max_x = 0.f, min_y = 0.f, max_y = 0.f, min_z = 0.f, max_z = 0.f;
};

struct ScenarioLimits
{
    std::uint32_t expected_map = 0;                  // 0 = do not check
    ScenarioBounds bounds;
    std::uint32_t idle_ms = 45000;                   // same window as IdleWatch (Bot/IdleWatch.h)
    float idle_yards = 3.0f;
    float recover_hp_pct = 60.f;                     // below this (or below recover_mana_pct) standing out of combat is eating/drinking
    float recover_mana_pct = 40.f;
    std::uint32_t rest_ms = 90000;                   // a healer under pull_ready_mana_pct this long out of a fight is not drinking back
    float pull_ready_mana_pct = 75.f;               // the tank waits for its healer to reach this (Classic::kPullReadyHealerManaPct)
    float stray_yards = 70.f;
    std::uint32_t stray_ms = 20000;
    float oom_pct = 10.f;
    std::uint32_t oom_ms = 10000;
    std::uint32_t stall_ms = 120000;
    float stall_yards = 8.f;
    std::uint32_t expected_group = 0;                // members every bot must see in its group; 0 = do not check
    // A death is a finding in itself, and what follows it (the dead release, run to a graveyard on another map, the healer has nobody to
    // heal) is noise that buries it: the run ends at this many deaths. 0 = never.
    std::uint32_t max_deaths = 1;
    // Raids lose people. This many deaths are noted (in the trace) but are not failures; the one after is. 0 = every death fails.
    std::uint32_t tolerated_deaths = 0;
    // A DPS bot in a threat hold (combat:threat_hold / threat_wait_tank / raid:threat_hold) this long while the raid fights. 0 = off.
    std::uint32_t dps_hold_ms = 0;
    // Raid: every tank dead is a wipe, whoever else lives.
    bool tanks_dead_is_wipe = false;
    // A boss kill passes the run however many die. The run then stops only on a full wipe, a boss reset, a stall, a timeout or a setup failure;
    // every other finding (deaths, mana, holds, strays) is logged as a warning and no longer fails it.
    bool kill_is_pass = false;
    // Support the encounter calls for, as the least number of casts a run that killed the boss must have seen (0 = not checked): hunters'
    // Tranquilizing Shots on an enrage, curse removals by mages / druids.
    std::uint32_t min_tranq_shots = 0;
    std::uint32_t min_decurses = 0;
};

class ScenarioMonitor
{
public:
    static constexpr std::size_t kTraceKeep = 200;

    explicit ScenarioMonitor(ScenarioLimits limits = {}) : limits_(std::move(limits)) {}

    // Call once a second with every member of the squad. `now_ms` counts from the start of the run. `entered` = the squad has
    // been sent into the content; the map and bounds checks only apply from then on. Returns true when the run cannot go on
    // (wipe, group stalled): the runner ends it as failed.
    bool Feed(std::uint32_t now_ms, std::vector<ScenarioSample> const& samples, bool entered)
    {
        bool terminal = false;
        bool any_alive = false, any_combat = false;
        for (ScenarioSample const& s : samples)
        {
            if (s.alive) any_alive = true;
            if (s.alive && s.in_combat) any_combat = true;
        }

        // Movement bookkeeping first: the idle check needs to know whether anyone else is walking.
        for (ScenarioSample const& s : samples)
        {
            BotState& b = state_[s.id];
            b.name = s.name;
            b.role = s.role;
            if (b.trace.empty() || b.trace.back().second != s.rule)
            {
                b.trace.emplace_back(now_ms, s.rule);
                if (b.trace.size() > kTraceKeep)
                    b.trace.erase(b.trace.begin());
            }
            if (b.has_last)
            {
                float const d = Dist2d(s.x, s.y, b.last_x, b.last_y);
                if (s.alive && d > 0.5f)
                    b.last_moved_ms = now_ms;
            }
            b.last_x = s.x; b.last_y = s.y; b.has_last = true;
        }

        if (!samples.empty() && !any_alive)
        {
            Fail(ScenarioFailKind::Wipe, "", "every member is dead", now_ms, wipe_reported_);
            return true;
        }

        // A raid whose pass bar is the kill ends on an effective wipe too, not only on the last bot dying: onyxia_full n24 sat to the 1200 s timeout with
        // one raider alive after the pull had wiped. Two cases: nobody alive is in combat any more (the rest ran off or hide), and a tenth or less of
        // the squad is left for 30 s.
        if (limits_.kill_is_pass && entered && !samples.empty())
        {
            if (any_combat)
            {
                fight_seen_ = true;
                no_combat_since_ = 0;
                no_combat_active_ = false;
            }
            else if (fight_seen_)
            {
                if (!no_combat_active_) { no_combat_active_ = true; no_combat_since_ = now_ms; }
                if (now_ms - no_combat_since_ >= kEffectiveWipeMs)
                {
                    Fail(ScenarioFailKind::Wipe, "", "the fight is over for the squad: no living member has been in combat for " + std::to_string(kEffectiveWipeMs / 1000u) + " s", now_ms, wipe_reported_);
                    return true;
                }
            }
            std::size_t alive = 0;
            for (ScenarioSample const& s : samples)
                if (s.alive) ++alive;
            if (fight_seen_ && alive * 10 <= samples.size())
            {
                if (!few_alive_active_) { few_alive_active_ = true; few_alive_since_ = now_ms; }
                if (now_ms - few_alive_since_ >= kFewAliveMs)
                {
                    Fail(ScenarioFailKind::Wipe, "", std::to_string(alive) + " of " + std::to_string(samples.size()) + " members alive for " + std::to_string(kFewAliveMs / 1000u) + " s", now_ms, wipe_reported_);
                    return true;
                }
            }
            else
                few_alive_active_ = false;
        }

        ScenarioSample const* tank = nullptr;
        std::size_t tanks_total = 0;
        for (ScenarioSample const& s : samples)
        {
            if (s.role == ScenarioRoleKind::Tank) ++tanks_total;
            if (!tank && s.alive && s.role == ScenarioRoleKind::Tank) tank = &s;
        }
        if (limits_.tanks_dead_is_wipe && tanks_total && !tank)
        {
            Fail(ScenarioFailKind::Wipe, "", "every tank is dead", now_ms, wipe_reported_);
            return true;
        }

        for (ScenarioSample const& s : samples)
        {
            BotState& b = state_[s.id];
            if (!s.alive)
            {
                b.died = true;
                b.reset_episode();
                continue;
            }

            // A raider that fell and came back at a graveyard or a spirit healer stands on another map because it died, which NoteDeath
            // counts: that is no second failure (wrong_map / out of bounds) on top of the death.
            if (entered && !b.died)
            {
                if (limits_.expected_map && s.map != limits_.expected_map)
                {
                    if (!b.wrong_map_reported)
                    {
                        b.wrong_map_reported = true;
                        Fail(ScenarioFailKind::WrongMap, s.name, "on map " + std::to_string(s.map) + ", expected " + std::to_string(limits_.expected_map), now_ms);
                    }
                    continue;   // positions on another map mean nothing against this one's box
                }
                if (limits_.bounds.enabled && !b.oob_reported && !InBounds(s))
                {
                    b.oob_reported = true;
                    Fail(ScenarioFailKind::OutOfBounds, s.name, Pos(s), now_ms);
                }
            }

            // The group the run started with is still the group: a bot that finds itself alone or in a smaller group gets no heals, no
            // pull orders and no loot rules, whatever the rest of the squad does.
            if (limits_.expected_group && s.group_size != limits_.expected_group && !b.group_reported)
            {
                b.group_reported = true;
                Fail(ScenarioFailKind::GroupBroken, s.name, "sees a group of " + std::to_string(s.group_size) + ", expected " + std::to_string(limits_.expected_group), now_ms);
            }

            // idle
            bool const recovering = !s.in_combat && (s.hp_pct < limits_.recover_hp_pct || (s.mana_pct >= 0.f && s.mana_pct < limits_.recover_mana_pct));
            // The tank's pull waits for the healer's mana (Classic::kPullReadyHealerManaPct): a squad standing for its drinking
            // healer is waiting, not idle. A healer that does not recover is the healer_oom / stall rules' to report.
            bool healer_recovering = false;
            for (ScenarioSample const& h : samples)
                if (h.role == ScenarioRoleKind::Healer && h.alive && !h.in_combat && h.mana_pct >= 0.f && h.mana_pct < limits_.pull_ready_mana_pct)
                    healer_recovering = true;
            // ... and so is a squad standing while any member sits eating or drinking (a healer between the pull-ready mana and the
            // 95% it sits to, or a caster topping up): their bot rule says so.
            for (ScenarioSample const& h : samples)
                if (h.id != s.id && h.alive && !h.in_combat &&
                    (h.rule.rfind("idle:dungeon_healer_drink", 0) == 0 || h.rule.rfind("idle:ooc_eat_food", 0) == 0))
                    healer_recovering = true;
            bool const exempt = s.in_combat || s.casting || recovering || any_combat || healer_recovering;
            if (exempt || !b.has_anchor || Dist2d(s.x, s.y, b.anchor_x, b.anchor_y) >= limits_.idle_yards)
            {
                b.anchor_x = s.x; b.anchor_y = s.y; b.anchor_ms = now_ms; b.has_anchor = true; b.idle_reported = false;
            }
            else if (!b.idle_reported && now_ms - b.anchor_ms >= limits_.idle_ms && SomeoneElseWalking(samples, s.id, now_ms))
            {
                b.idle_reported = true;
                Fail(ScenarioFailKind::Idle, s.name, "stood " + std::to_string((now_ms - b.anchor_ms) / 1000u) + " s at " + Pos(s) + " while the group moved on", now_ms);
            }

            // stray
            if (tank && tank->id != s.id && tank->map == s.map)
            {
                float const d = Dist2d(s.x, s.y, tank->x, tank->y);
                if (d <= limits_.stray_yards)
                {
                    b.stray_active = false;
                    b.stray_reported = false;
                }
                else
                {
                    if (!b.stray_active) { b.stray_active = true; b.stray_since_ms = now_ms; }
                    if (!b.stray_reported && now_ms - b.stray_since_ms >= limits_.stray_ms)
                    {
                        b.stray_reported = true;
                        Fail(ScenarioFailKind::Stray, s.name, Trim(d) + " y from " + tank->name + " for " + std::to_string((now_ms - b.stray_since_ms) / 1000u) + " s, at " + Pos(s), now_ms);
                    }
                }
            }

            // DPS held back by the threat rules for too long while the raid fights
            if (limits_.dps_hold_ms && s.role == ScenarioRoleKind::Dps)
            {
                bool const held = any_combat && IsThreatHold(s.rule);
                if (!held)
                {
                    b.hold_active = false;
                    b.hold_reported = false;
                }
                else
                {
                    if (!b.hold_active) { b.hold_active = true; b.hold_since_ms = now_ms; }
                    if (!b.hold_reported && now_ms - b.hold_since_ms >= limits_.dps_hold_ms)
                    {
                        b.hold_reported = true;
                        Fail(ScenarioFailKind::DpsHold, s.name, "held back (" + s.rule + ") for " + std::to_string((now_ms - b.hold_since_ms) / 1000u) + " s", now_ms);
                    }
                }
            }

            // healer out of mana: dry while a fight needs her. Dry between fights is a drink in progress, and healer_no_rest
            // reports one that never recovers.
            if (s.role == ScenarioRoleKind::Healer && s.mana_pct >= 0.f && s.mana_pct < limits_.oom_pct && any_combat)
            {
                if (!b.oom_active) { b.oom_active = true; b.oom_since_ms = now_ms; }
                if (!b.oom_reported && now_ms - b.oom_since_ms >= limits_.oom_ms)
                {
                    b.oom_reported = true;
                    Fail(ScenarioFailKind::HealerOom, s.name, "under " + Trim(limits_.oom_pct) + "% mana for " + std::to_string((now_ms - b.oom_since_ms) / 1000u) + " s", now_ms);
                }
            }
            else
            {
                b.oom_active = false;
                b.oom_reported = false;
            }

            // healer that does not rest: below the tank's pull-ready mana, out of a fight, for rest_ms. The squad waits for it (that is
            // not idle, above), so a healer that never drinks is reported here and not as the tank's idleness.
            if (s.role == ScenarioRoleKind::Healer && !s.in_combat && !any_combat && s.mana_pct >= 0.f && s.mana_pct < limits_.pull_ready_mana_pct)
            {
                if (!b.rest_active) { b.rest_active = true; b.rest_since_ms = now_ms; }
                if (!b.rest_reported && now_ms - b.rest_since_ms >= limits_.rest_ms)
                {
                    b.rest_reported = true;
                    Fail(ScenarioFailKind::HealerNoRest, s.name, "under " + Trim(limits_.pull_ready_mana_pct) + "% mana (" + Trim(s.mana_pct) + "%) out of a fight for " + std::to_string((now_ms - b.rest_since_ms) / 1000u) + " s: it is not drinking back", now_ms);
                }
            }
            else
            {
                b.rest_active = false;
                b.rest_reported = false;
            }
        }

        // The whole group standing still, out of a fight.
        if (any_alive)
        {
            bool moved = any_combat || stall_anchor_.empty();
            if (!moved)
                for (ScenarioSample const& s : samples)
                {
                    if (!s.alive) continue;
                    auto it = stall_anchor_.find(s.id);
                    if (it == stall_anchor_.end() || Dist2d(s.x, s.y, it->second.first, it->second.second) >= limits_.stall_yards)
                    {
                        moved = true;
                        break;
                    }
                }
            if (moved)
            {
                stall_anchor_.clear();
                for (ScenarioSample const& s : samples)
                    if (s.alive) stall_anchor_[s.id] = { s.x, s.y };
                stall_since_ms_ = now_ms;
                stall_reported_ = false;
            }
            else if (!stall_reported_ && now_ms - stall_since_ms_ >= limits_.stall_ms)
            {
                // Where everybody stands and what each bot logic last decided: the one line that says why a whole group waits.
                std::string who;
                for (ScenarioSample const& s : samples)
                    who += (who.empty() ? "" : "; ") + s.name + (s.alive ? "" : " (dead)") + " at " + Trim(s.x) + "," + Trim(s.y) + "," + Trim(s.z) + " hp " + Trim(s.hp_pct) + "%" +
                           (s.mana_pct >= 0.f ? " mana " + Trim(s.mana_pct) + "%" : std::string()) + " rule=" + (s.rule.empty() ? "-" : s.rule);
                Fail(ScenarioFailKind::GroupStalled, "", "nobody moved " + Trim(limits_.stall_yards) + " y or fought for " + std::to_string((now_ms - stall_since_ms_) / 1000u) + " s: " + who, now_ms, stall_reported_);
                terminal = true;
            }
        }
        return terminal;
    }

    void NoteDeath(std::uint32_t now_ms, ScenarioDeath const& d)
    {
        std::string text = d.killer + ", " + d.spell;
        if (d.damage > 0) text += " " + std::to_string(d.damage);
        text += ", at " + Trim(d.x) + "," + Trim(d.y) + "," + Trim(d.z);
        if (d.rule != "-") text += ", rule " + d.rule;
        ++deaths_;
        if (deaths_ > limits_.tolerated_deaths)
            Fail(ScenarioFailKind::Death, d.name, "killed by " + text, now_ms);
        else
            tolerated_.push_back(d.name + " t=" + std::to_string(now_ms / 1000u) + "s died, " + text);
        auto it = state_.find(d.id);
        if (it != state_.end())
        {
            it->second.died = true;
            it->second.trace.emplace_back(now_ms, "DIED");
        }
    }

    // The goal boss, once a second. Engaged (seen in combat) and then out of combat or evading while alive = it reset: the raid wiped
    // on it, or pulled it out of its leash. Returns true when the run cannot go on.
    bool NoteBoss(std::uint32_t now_ms, ScenarioBossSample const& b)
    {
        if (!b.found || !b.alive)
            return false;
        if (b.in_combat && !b.evading)
        {
            boss_engaged_ = true;
            boss_idle_since_ms_ = 0;
            boss_idle_active_ = false;
            return false;
        }
        if (!boss_engaged_)
            return false;
        // A second or two without combat is a leash flicker or a phase gap; a reset is the boss out of combat for good.
        if (!boss_idle_active_) { boss_idle_active_ = true; boss_idle_since_ms_ = now_ms; }
        if (b.evading || now_ms - boss_idle_since_ms_ >= kBossResetMs)
        {
            Fail(ScenarioFailKind::BossReset, "", std::string("the boss ") + (b.evading ? "is evading" : "left combat") + " at " + Trim(b.hp_pct) + "% health", now_ms, boss_reset_reported_);
            boss_reset_ = true;
            return true;
        }
        return false;
    }

    void NoteSetupFailure(std::uint32_t now_ms, std::string const& bot, std::string const& why) { Fail(ScenarioFailKind::SetupFailed, bot, why, now_ms); }
    void NoteTimeout(std::uint32_t now_ms, std::string const& what) { Fail(ScenarioFailKind::Timeout, "", what, now_ms); }
    // A support job the content calls for (a Tranquilizing Shot on Magmadar's Frenzy, a curse removed in Lucifron's room) was never done.
    void NoteSupportMissing(std::uint32_t now_ms, std::string const& what) { Fail(ScenarioFailKind::SupportMissing, "", what, now_ms); }
    // A line for the result that is not a failure (counts the run measured).
    void NoteInfo(std::string const& line) { tolerated_.push_back(line); }

    // The runner is holding the Attack on purpose (the healers' mana wait, itself bounded): the squad standing still is the plan, not a stall.
    // MC Ragnaros 2026-10-13 4839bcf3: group_stalled at 121 s, inside the 120 s wait that ends at 140 s.
    void NoteDeliberateWait(std::uint32_t now_ms) { stall_since_ms_ = now_ms; }

    std::vector<ScenarioFailure> const& failures() const { return failures_; }
    // Findings that do not fail a run whose kill is the pass bar (ScenarioLimits::kill_is_pass).
    std::vector<ScenarioFailure> const& warnings() const { return warnings_; }

    // What each bot's bot logic was doing: one line per rule change, the last `per_bot` of each bot, oldest first. A failure that says
    // "the healer never healed" is answered by reading this (a movement rule that won every tick, a hold, the heal rules).
    std::vector<std::string> Trace(std::size_t per_bot = 25) const
    {
        std::vector<std::string> lines;
        std::vector<BotState const*> bots;
        for (auto const& kv : state_)
            bots.push_back(&kv.second);
        std::sort(bots.begin(), bots.end(), [](BotState const* a, BotState const* b) { return a->name < b->name; });
        for (BotState const* b : bots)
        {
            std::size_t const from = b->trace.size() > per_bot ? b->trace.size() - per_bot : 0;
            for (std::size_t i = from; i < b->trace.size(); ++i)
                lines.push_back(b->name + " t=" + std::to_string(b->trace[i].first / 1000u) + "s " + (b->trace[i].second.empty() ? "-" : b->trace[i].second));
        }
        return lines;
    }
    bool passed(bool goal_reached) const { return goal_reached && failures_.empty(); }
    // The run cannot usefully go on: enough members have died.
    // A run whose pass bar is the kill is never ended by a death count, whatever max_deaths says.
    bool terminal() const { return boss_reset_ || (!limits_.kill_is_pass && limits_.max_deaths != 0 && deaths_ >= limits_.max_deaths); }
    std::uint32_t deaths() const { return deaths_; }
    // Deaths inside the tolerance, as text lines (they are not failures).
    std::vector<std::string> const& tolerated() const { return tolerated_; }
    // The rule changes of each failing bot in the 90 s before its failure, one line each ("idle Scnwarrior t=120s idle:dungeon_hold").
    std::vector<std::string> const& contexts() const { return contexts_; }

    // A raid trace: every tank, the first `healers` healers and the first `dps` DPS bots by name, and any bot named in a failure.
    // The 25 lines of 40 bots would bury what matters.
    std::vector<std::string> TraceSelected(std::size_t per_bot, std::size_t healers, std::size_t dps) const
    {
        std::vector<std::string> lines;
        std::vector<BotState const*> bots;
        for (auto const& kv : state_)
            bots.push_back(&kv.second);
        std::sort(bots.begin(), bots.end(), [](BotState const* a, BotState const* b) { return a->name < b->name; });
        std::size_t nh = 0, nd = 0;
        for (BotState const* b : bots)
        {
            bool pick = b->role == ScenarioRoleKind::Tank;
            if (!pick && b->role == ScenarioRoleKind::Healer && nh < healers) { pick = true; ++nh; }
            else if (!pick && b->role == ScenarioRoleKind::Dps && nd < dps) { pick = true; ++nd; }
            if (!pick)
                for (ScenarioFailure const& f : failures_)
                    if (f.bot == b->name) { pick = true; break; }
            if (!pick)
                continue;
            std::size_t const from = b->trace.size() > per_bot ? b->trace.size() - per_bot : 0;
            for (std::size_t i = from; i < b->trace.size(); ++i)
                lines.push_back(b->name + " t=" + std::to_string(b->trace[i].first / 1000u) + "s " + (b->trace[i].second.empty() ? "-" : b->trace[i].second));
        }
        return lines;
    }


private:
    struct BotState
    {
        float last_x = 0.f, last_y = 0.f;
        bool has_last = false;
        std::uint32_t last_moved_ms = 0;
        float anchor_x = 0.f, anchor_y = 0.f;
        std::uint32_t anchor_ms = 0;
        bool has_anchor = false;
        bool idle_reported = false;
        std::uint32_t stray_since_ms = 0;
        bool stray_active = false;
        bool stray_reported = false;
        std::uint32_t oom_since_ms = 0;
        bool oom_active = false;
        bool oom_reported = false;
        std::uint32_t rest_since_ms = 0;
        bool rest_active = false;
        bool rest_reported = false;
        bool oob_reported = false;
        bool wrong_map_reported = false;
        bool died = false;   // dead at some point of this run: where it stands afterwards (a graveyard, a spirit healer) is its death's doing
        bool group_reported = false;
        ScenarioRoleKind role = ScenarioRoleKind::Dps;
        std::uint32_t hold_since_ms = 0;
        bool hold_active = false;
        bool hold_reported = false;
        std::string name;
        std::vector<std::pair<std::uint32_t, std::string>> trace;   // (ms, rule) at each change

        void reset_episode()
        {
            has_anchor = false; idle_reported = false;
            stray_active = false; stray_reported = false;
            oom_active = false; oom_reported = false;
            rest_active = false; rest_reported = false;
            hold_active = false; hold_reported = false;
        }
    };

    static constexpr std::uint32_t kBossResetMs = 5000;
    static constexpr std::uint32_t kEffectiveWipeMs = 15000;
    static constexpr std::uint32_t kFewAliveMs = 30000;

    static bool IsThreatHold(std::string const& rule)
    {
        return rule == "combat:threat_hold" || rule == "combat:threat_wait_tank" || rule == "raid:threat_hold" ||
               rule == "combat:shield_hold";
    }

    static float Dist2d(float ax, float ay, float bx, float by) { return std::hypot(ax - bx, ay - by); }
    static std::string Trim(float v)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", double(v));
        return buf;
    }
    static std::string Pos(ScenarioSample const& s) { return Trim(s.x) + "," + Trim(s.y) + "," + Trim(s.z); }

    bool InBounds(ScenarioSample const& s) const
    {
        ScenarioBounds const& b = limits_.bounds;
        return s.x >= b.min_x && s.x <= b.max_x && s.y >= b.min_y && s.y <= b.max_y && s.z >= b.min_z && s.z <= b.max_z;
    }

    bool SomeoneElseWalking(std::vector<ScenarioSample> const& samples, std::uint64_t self, std::uint32_t now_ms) const
    {
        for (ScenarioSample const& o : samples)
        {
            if (o.id == self || !o.alive) continue;
            auto it = state_.find(o.id);
            if (it != state_.end() && it->second.last_moved_ms != 0 && now_ms - it->second.last_moved_ms <= 5000u)
                return true;
        }
        return false;
    }

    void Fail(ScenarioFailKind kind, std::string bot, std::string detail, std::uint32_t now_ms)
    {
        // What the bot's bot logic did in the 90 s before it: the last-25 trace of a long run no longer reaches back to an early failure.
        if (!bot.empty())
            for (auto const& kv : state_)
                if (kv.second.name == bot)
                {
                    // The rule in force when the window opens (a hold that never changed has no entry inside it), then the changes.
                    std::size_t n = 0;
                    std::pair<std::uint32_t, std::string> const* before = nullptr;
                    for (auto const& e : kv.second.trace)
                        if (e.first + kContextWindowMs < now_ms)
                            before = &e;
                    auto line = [&](std::pair<std::uint32_t, std::string> const& e)
                    {
                        contexts_.push_back(std::string(ToString(kind)) + " " + bot + " t=" + std::to_string(e.first / 1000u) + "s " + (e.second.empty() ? "-" : e.second));
                    };
                    if (before)
                        line(*before);
                    for (auto const& e : kv.second.trace)
                        if (e.first + kContextWindowMs >= now_ms && e.first <= now_ms && n++ < kContextLines)
                            line(e);
                }
        // The ends of a run stay failures; the rest are what the bots did wrong on the way to a kill.
        bool const ends_run = kind == ScenarioFailKind::Wipe || kind == ScenarioFailKind::Timeout || kind == ScenarioFailKind::SetupFailed ||
                              kind == ScenarioFailKind::GroupBroken || kind == ScenarioFailKind::BossReset || kind == ScenarioFailKind::GroupStalled;
        if (limits_.kill_is_pass && !ends_run)
            warnings_.push_back({ kind, std::move(bot), std::move(detail), now_ms });
        else
            failures_.push_back({ kind, std::move(bot), std::move(detail), now_ms });
    }
    void Fail(ScenarioFailKind kind, std::string bot, std::string detail, std::uint32_t now_ms, bool& once)
    {
        if (once) return;
        once = true;
        Fail(kind, std::move(bot), std::move(detail), now_ms);
    }

    ScenarioLimits limits_;
    std::unordered_map<std::uint64_t, BotState> state_;
    std::vector<ScenarioFailure> failures_;
    std::vector<ScenarioFailure> warnings_;
    std::unordered_map<std::uint64_t, std::pair<float, float>> stall_anchor_;
    std::uint32_t stall_since_ms_ = 0;
    bool stall_reported_ = false;
    bool wipe_reported_ = false;
    bool fight_seen_ = false;
    bool no_combat_active_ = false, few_alive_active_ = false;
    std::uint32_t no_combat_since_ = 0, few_alive_since_ = 0;
    std::uint32_t deaths_ = 0;
    std::vector<std::string> tolerated_;
    std::vector<std::string> contexts_;
    static constexpr std::uint32_t kContextWindowMs = 90000;
    static constexpr std::size_t kContextLines = 40;
    bool boss_engaged_ = false;
    bool boss_idle_active_ = false;
    std::uint32_t boss_idle_since_ms_ = 0;
    bool boss_reset_ = false;
    bool boss_reset_reported_ = false;
};

} // namespace Playerbot::Diagnostics
