// ConfigReader - Loads playerbot.conf and exposes typed accessors. Hot-reload
// supported for HR-yes keys (per CONFIG.md).
//
// Backed by TrinityCore's existing ConfigMgr. No new file format.

#pragma once

#include "Bot/BotTypes.h"
#include <string>

namespace Playerbot {

class ConfigReader
{
public:
    // Returns true if config loaded (or default values acceptable for missing keys).
    bool load(std::string const& file_path);
    bool reload();

    // ----- Accessors (HR-yes unless noted) ----
    // Threading
    uint32 ai_worker_threads()      const { return ai_worker_threads_; }
    bool   fleet_thread_enabled()   const { return fleet_thread_enabled_; }
    uint32 tick_budget_ms()         const { return tick_budget_ms_; }

    // #5 Phase 4: parallelize BotSnapshotBuilder::Build per Map* across a
    // fixed worker pool during the world-thread snapshot window. When false
    // the snapshot loop runs exactly as before (serial, world thread). Kept
    // as a kill-switch so the operator can disable instantly if a race
    // surfaces at scale. NOT hot-reloadable for the pool size (the pool is
    // sized once at first use); the enable flag is read every tick so it can
    // be flipped live. (PlayerbotV2.ParallelSnapshotBuild)
    bool   parallel_snapshot_build() const { return parallel_snapshot_build_; }
    // Worker count for the snapshot build pool. 0 = auto (cores-2, clamped to
    // [1,24]); the world thread also participates so effective parallelism is
    // this+1. (PlayerbotV2.SnapshotBuildThreads)
    uint32 snapshot_build_threads()  const { return snapshot_build_threads_; }

    // Population
    uint32 population_total_target() const { return pop_target_; }
    uint8  population_horde_pct()    const { return pop_horde_pct_; }
    std::string const& population_shape() const { return pop_shape_; }
    float  population_max_heavy_factor() const { return pop_max_heavy_factor_; }

    // Interaction
    std::string const& accept_invites_policy() const { return accept_invites_; }
    bool   auto_leader_handover() const { return auto_leader_handover_; }
    char   command_prefix() const { return command_prefix_; }

    // Raid brain (Bot/Raid): read once per config load, not per tick.
    bool   raid_enabled() const { return raid_enabled_; }
    bool   boss_warnings_enabled() const { return boss_warnings_enabled_; }
    bool   raid_auto_accept_rez() const { return raid_auto_accept_rez_; }

    // Housing
    bool   housing_enabled() const { return housing_enabled_; }
    uint8  neighborhood_target_occupancy() const { return nbhd_target_pct_; }
    bool   housing_strict_eligibility() const { return housing_strict_; }

    // Diagnostics
    uint16 health_endpoint_port() const { return health_port_; }
    std::string const& log_level() const { return log_level_; }

    // Lifecycle: when true, on first OnWorldUpdate after Module::Init the
    // BotSessionMgr submits a headless login for every marked-as-bot
    // character that isn't already in-world (capped by the same default as
    // .playerbot loginall). Default false to avoid surprise mass spawns
    // on shared dev servers. Useful for "headless test farm" boots.
    bool   auto_resume_on_boot() const { return auto_resume_on_boot_; }
    uint32 auto_resume_cap()     const { return auto_resume_cap_; }

    // Auto-spawn-on-boot: target population the module brings up after the
    // first AutoResume pass. If marked-bot count is below this target on
    // first OnWorldUpdate, the module batch-creates the shortfall via
    // BotComposition::Roll + BotCharacterFactory::Create + LoginBot. Hard
    // cap baked in (200) to prevent surprise mass-spawn from a config
    // typo. Default 0 = feature disabled.
    uint32 auto_spawn_on_boot()  const { return auto_spawn_on_boot_; }

    // Per-bot personality variance: when true, register_bot uses
    // RandomPersonality(SeedForBot(id)) to give each bot a deterministic
    // weighted-random personality (Aggression / RiskTolerance / Verbosity
    // / etc). When false, every bot gets DefaultPersonality (all Normal),
    // useful for predictable testing where personality variance would
    // confound results. Default true so a fleet looks like a Blizzlike
    // population out of the box.
    bool   random_personality()  const { return random_personality_; }

    // #4A: per-bot ARCHETYPE assignment. When true, each bot is given a
    // deterministic weighted-random play archetype (CasualSolo /
    // HardcoreRaider / SocialGuildie / GathererFlipper / PvPer /
    // AltoholicExplorer) on login via RollArchetype(SeedForBot(id)), the
    // archetype_id is persisted to playerbot_v2_character, and the population
    // rebalancer prefers high-role-affinity bots when respeccing into starved
    // tank/healer slots. When false every bot reads as CasualSolo (id 0) for
    // uniform, predictable testing. Default true so the fleet is heterogeneous
    // out of the box. (PlayerbotV2.Archetype.Enabled)
    bool   archetype_enabled()   const { return archetype_enabled_; }

    // #4B-1 Part 2: AH buy-side master switch. When true (default), the
    // idle:ah_buy_reagents rule may emit AH buyouts for reagents the bot is
    // short on (and, for Reseller-archetype bots, cheap flippable trade goods).
    // The buy path moves gold bot->bot through the auction house, and the AH
    // deposit + house cut on each sale destroys a slice of that gold — so the
    // buy/sell loop is a net gold SINK over time, not an accumulation. Set to 0
    // on a realm where you want bots to sell on the AH but never buy from it
    // (one-directional supply), e.g. when measuring listing behavior in
    // isolation. The sell side (idle:ah_post_surplus, dynamic pricing) is
    // unaffected by this toggle. (PlayerbotV2.Economy.BuyEnabled) HR-yes.
    bool   economy_buy_enabled() const { return economy_buy_enabled_; }

    // Bot-as-player auction behaviour (idle:ah_post_surplus, idle:ah_cancel_undercut,
    // idle:ah_buy_reagents, idle:rpg_ah_trip). Off by default: the core AuctionHouseBot
    // is the one actor on the auction house, and the two systems underquote each other.
    // (PlayerbotV2.Economy.BotAuctions)
    bool   economy_bot_auctions_enabled() const { return economy_bot_auctions_enabled_; }

    // Classic 1.60: bots get their class tools and reagents for free (shaman
    // totems, rogue poisons, Symbol of Kings, candles, seeds, shards; see
    // ClassicSupplies.h), as ike3's playerbots do. Without them totems,
    // poisons and the group buffs can't be cast. (PlayerbotV2.Classic.ClassSupplies)
    bool   classic_class_supplies() const { return classic_class_supplies_; }

    // #4B-1(b): per-unit FAIR-VALUE multiple over an item's vendor SellPrice
    // used as the buy-side reagent price CEILING. The snapshot builder
    // computes each buyable reagent's ceiling = vendor SellPrice * this (or a
    // quality-based flat floor when SellPrice == 0), and idle:ah_buy_reagents
    // refuses any listing/commodity whose per-unit price exceeds it. This is
    // the anti price-pump guard: a human can't post a wildly over-priced
    // reagent and bleed bot gold through the buy loop. Default 15 — a typical
    // healthy AH price for trade goods sits a single-digit multiple over
    // vendor sell, so 15x leaves generous headroom while still capping the
    // absurd. (PlayerbotV2.Economy.MaxReagentVendorMultiple)
    uint32 economy_max_reagent_vendor_multiple() const { return economy_max_reagent_vendor_multiple_; }

    // Session-rhythm logout (#5 living-server realism): bots log off after
    // their archetype's target_session_minutes (jittered) so the online
    // roster CHURNS like a human playerbase instead of every bot staying
    // online forever — the single most visible non-human tell. The population
    // manager re-logs other offline bots to hold TotalTarget, so headcount is
    // stable while WHO is online rotates. (PlayerbotV2.SessionRhythm.*)
    //   Enabled         — master switch (default true).
    //   Multiplier      — global scale on target_session_minutes; set <1 to
    //                     shorten sessions for a fast churn soak, >1 to
    //                     lengthen. Default 1.0. <=0 disables.
    //   MaxLogoutsPerCycle — cap on session-expiry logouts per 60s reconcile
    //                     so the roster rotates smoothly instead of a mass
    //                     exodus; ~matches the login refill rate. Default 25.
    bool   session_rhythm_enabled()    const { return session_rhythm_enabled_; }
    float  session_rhythm_multiplier() const { return session_rhythm_multiplier_; }
    uint32 session_rhythm_max_logouts_per_cycle() const { return session_rhythm_max_logouts_per_cycle_; }

    // Phase E: guild-ecosystem opt-outs (GUILD_PLAN.md).
    // - guilds_enabled: master switch. False disables Phase A.2 founder
    //   election, Phase B recruitment, Phase C chat suite, Phase D
    //   events, Phase E recruitment posts. Existing bot guilds keep
    //   their members but are no longer managed (hygiene cron stops).
    // - guilds_target_count_per_faction: overrides
    //   BotGuildMgr::kDefaultTargetGuildsPerFaction (6). Drop to 0/1/2
    //   on shared dev servers to keep the guild fleet small. The
    //   manager does NOT disband existing guilds when this value
    //   shrinks; just stops electing new founders.
    // - guilds_events_enabled: separate toggle so operators can keep
    //   the rest of Phase B/C ecosystem but skip the timed events.
    // - guilds_recruitment_channel_enabled: trade-channel recruit
    //   posts (Phase E.1) opt-out.
    bool   guilds_enabled() const { return guilds_enabled_; }
    uint8  guilds_target_count_per_faction() const { return guilds_target_count_per_faction_; }
    uint16 guilds_max_members_per_guild() const { return guilds_max_members_per_guild_; }
    bool   guilds_events_enabled() const { return guilds_events_enabled_; }
    bool   guilds_recruitment_channel_enabled() const { return guilds_recruitment_channel_enabled_; }
    // Per-account cap on alts a player can summon as bots via the
    // `.playerbot summon` command. Default 5 matches a 5-man dungeon
    // group (tank + healer + 3 dps). Operators can lower for shared
    // realms or raise for solo-realm power users. Hard zero disables
    // the player-facing summon entirely without removing the command.
    uint8  max_alts_as_bots() const { return max_alts_as_bots_; }

    // #1B WedgeWatchdog: ms of sustained no-progress before a stuck bot is
    // reported. Default 90s (PlayerbotV2.WedgeWatchdog.ThresholdMs). Lower for
    // a chatty wedge log on a test farm; raise to silence transient flukes.
    uint32 wedge_watchdog_threshold_ms() const { return wedge_watchdog_threshold_ms_; }
    // Longer gate for CombatLoop wedges (combat legitimately holds position).
    uint32 wedge_watchdog_combat_threshold_ms() const { return wedge_watchdog_combat_threshold_ms_; }
    // Yards a bot must move (same map) to reset its stationary clock; physical
    // displacement is the primary wedge gate (kills travelling-bot false positives).
    float  wedge_watchdog_min_displacement() const { return wedge_watchdog_min_displacement_; }

    // Optional auto-remediation — DEFAULT OFF. The watchdog's primary job is
    // DETECT + TRACK (the [wedge] log + persistent stuck-objective ledger, both
    // always-on). Auto-abandoning a quest is a band-aid: it hides WHICH content
    // is broken and impedes root-cause debugging. So it is opt-in via a single
    // flag governing ALL wedge categories (GoalUnreachable / CombatLoop /
    // NoProgress). When an operator chooses fleet liveness over debuggability
    // and enables it, a bot stuck past RemediationMs has the picker's current
    // objective TEMPORARILY blacklisted (5 min — the quest resurfaces and is
    // re-tested, NOT permanently abandoned). No teleport, no relocation. Leave
    // OFF while debugging stuck content (the ledger is the worklist).
    //   (PlayerbotV2.WedgeWatchdog.RemediationEnabled [default false] / .RemediationMs)
    bool   wedge_remediation_enabled() const { return wedge_remediation_enabled_; }
    uint32 wedge_remediation_ms()      const { return wedge_remediation_ms_; }

    // No-progress watchdog (the movement-INDEPENDENT complement to the
    // displacement-gated wedge detector above). Flags a bot that keeps acting
    // (moving / looping idle rules) yet makes no measurable progress on a
    // COMPOSITE metric — XP/level, quests turned in, gold, total skill points
    // (covers gathering/fishing/crafting), and bag-item count (covers a
    // capped-skill gatherer hoarding mats). Because legitimate non-XP activity
    // advances one of those, a productive bot never trips it; only a bot truly
    // looping in place (e.g. innkeeper<->service oscillation, picker_none, a
    // death-cycle) does. NoProgressMs is the stall window; bots whose archetype
    // dominant activity is Profession get a 3x window (intentional low-XP
    // grinders). NoProgressRadiusYards is the net displacement past which the
    // bot counts as TRAVELLING (anchor resets) rather than stuck-in-place, so a
    // long cross-continent walk is never flagged. Remediation reuses the same
    // blacklist-objective -> repick/relocate path as the wedge remediator.
    //   (PlayerbotV2.WedgeWatchdog.NoProgress{Enabled,Ms,RadiusYards})
    bool   wedge_noprogress_enabled() const { return wedge_noprogress_enabled_; }
    uint32 wedge_noprogress_ms()      const { return wedge_noprogress_ms_; }
    float  wedge_noprogress_radius()  const { return wedge_noprogress_radius_; }

    // #1C fleet-vitals alerting thresholds. A 60s vitals bucket that breaches
    // one of these fires a throttled [fleet_alert] TC_LOG_ERROR naming the
    // metric + value + threshold. A threshold of 0 disables that specific
    // alert (so an operator can silence individual signals without losing the
    // others). Defaults are tuned for a 2000-bot fleet on the dev box.
    //   - alert_wedged_bots: max acceptable concurrently-wedged bot count.
    //   - alert_tick_p99_us: max acceptable tick p99 latency in microseconds.
    //   - alert_intent_drop_per_min: max acceptable IntentQueue drops/min
    //     (sustained drops = an bot worker outrunning the world-thread drain).
    //   - alert_path_fail_per_min: max acceptable path-validation failures/min.
    //   - alert_throttle_ms: per-metric re-emit suppression window (default 5m)
    //     so a sustained breach logs once per window, not once per sample.
    uint32 alert_wedged_bots()        const { return alert_wedged_bots_; }
    uint32 alert_tick_p99_us()        const { return alert_tick_p99_us_; }
    uint32 alert_intent_drop_per_min() const { return alert_intent_drop_per_min_; }
    uint32 alert_path_fail_per_min()  const { return alert_path_fail_per_min_; }
    uint32 alert_throttle_ms()        const { return alert_throttle_ms_; }

    // ── Ranged-pull discipline / tank detour gate (2026-07-02 SFK wedge) ──
    bool  pull_gate_enabled()          const { return pull_gate_enabled_; }
    float pull_gate_max_ratio()        const { return pull_gate_max_ratio_; }
    float pull_gate_min_extra_yards()  const { return pull_gate_min_extra_yards_; }
    // Stage 3: mob-INITIATED combat variant (proximity aggro puts a non-tank
    // bot in combat before either pre-combat pull gate above ever evaluates
    // the target). When true, a sustained (>6s) untankable lock triggers
    // dungeon:untankable_disengage. Separate kill-switch from pull_gate_
    // enabled_ so an operator can keep the pre-combat gates but disable the
    // disengage (or vice versa) while diagnosing. (PlayerbotV2.PullGate.DisengageEnabled)
    bool  pull_gate_disengage_enabled() const { return pull_gate_disengage_enabled_; }
    // Stage 4: tank long-detour CHASE COMMITMENT. Once a tank commits to a
    // long-but-complete corridor toward its current victim, hold the
    // commitment (re-plan only every few seconds, never re-pick) instead of
    // oscillating; give up loudly after TankCommitMaxMs so a genuinely
    // unreachable pull still resolves via the normal disengage/escape paths.
    // Independent kill-switch from pull_gate_enabled_ so an operator can
    // disable just the commitment behavior while diagnosing.
    bool   tank_commit_enabled()  const { return tank_commit_enabled_; }
    uint32 tank_commit_max_ms()   const { return tank_commit_max_ms_; }

    // OOC dungeon step-hold (2026-07-03 WC/SFK stutter fix): when a bot's
    // live movement spline is already heading toward ~the same OOC
    // advance/route step, skip re-emitting MoveTo instead of restarting the
    // spline. Kill switch for diagnosing without recompiling.
    //   (PlayerbotV2.Move.StepHoldEnabled)
    bool move_step_hold_enabled() const { return move_step_hold_enabled_; }

    // Route-aware in-combat advance (2026-07-03, increment 1b): when a
    // dungeon has route_waypoints and the OOC route cursor is armed for the
    // current map, the in-combat boss-advance family (rules (0)/(0c) in
    // DungeonCombatPositioning) steps toward the SAME route crumb instead of
    // computing its own direct-to-boss step — fixes the WC crumb-14 wedge
    // where the two step sources diverged and alternately re-aimed the
    // spline forever. Kill switch for diagnosing without recompiling.
    //   (PlayerbotV2.Move.RouteAwareCombatAdvance)
    bool route_aware_combat_advance() const { return route_aware_combat_advance_; }

    // Combat re-aim reachability gate (2026-07-03, increment 1e): do not
    // chase / gap-close / self-acquire an attacker that is not "fightable" —
    // untargetable / pacified / already dead (the fightable_attackers_count()
    // flag tally), OR its own chase generator reports it cannot path to us
    // (Creature::CanNotReachTarget, snapshot NearbyUnit::cannot_reach — the
    // aggro-but-unreachable window; the flags alone never encode path-
    // unreachability). Such a pack otherwise steals movement ownership from
    // dungeon navigation every combat frame (WC corridor freeze, tank frozen
    // 15+ min re-aiming at an ordinary-targetable z-disconnected ledge pack).
    // Kill switch for diagnosing without recompiling.
    //   (PlayerbotV2.Combat.SkipUnfightable)
    bool combat_skip_unfightable() const { return combat_skip_unfightable_; }

    // Group threat discipline (port of Karrz's wow-1.12 GroupPveRestraint /
    // MCRaidHoldDamage). Non-tank bots in a group with a tank hold damage
    // until the tank has an opening threat lead and stay under max_ratio of
    // the tank's threat. Leads are level-60 values, scaled by bot level.
    //   (PlayerbotV2.Threat.*)
    bool   threat_hold_enabled() const         { return threat_hold_enabled_; }
    bool   raid_dps_wait_attack() const        { return raid_dps_wait_attack_; }
    bool   raid_provide_consumables() const    { return raid_provide_consumables_; }
    // Playerbot.Consumables.PvE / .PvP: whether bots use (and are stocked with) flasks, elixirs, buff food, combat
    // potions, healthstones and resistance potions in PvE (open world, dungeons, raids) or in PvP (battlegrounds, arenas).
    bool   consumables_pve() const             { return consumables_pve_; }
    bool   consumables_pvp() const             { return consumables_pvp_; }
    uint32 raid_dps_wait_timeout_ms() const    { return raid_dps_wait_timeout_ms_; }
    bool   threat_hold_open_world() const      { return threat_hold_open_world_; }
    float  threat_max_ratio() const            { return threat_max_ratio_; }
    float  threat_opening_lead_boss() const    { return threat_opening_lead_boss_; }
    float  threat_opening_lead_trash() const   { return threat_opening_lead_trash_; }
    uint32 threat_opening_wait_boss_ms() const { return threat_opening_wait_boss_ms_; }
    uint32 threat_opening_wait_trash_ms() const{ return threat_opening_wait_trash_ms_; }

    // Behaviour switches (Playerbot.Behavior.<Name>, default on = current behaviour). Each gate
    // makes its behaviour fall back to the previous simple path. Cached at load, so they are
    // readable from the bot worker threads as well as the world thread.
    bool behavior_owner_follow_hold()   const { return bhv_owner_follow_hold_; }
    bool behavior_fallback()            const { return bhv_fallback_; }
    bool behavior_pull_back()           const { return bhv_pull_back_; }
    bool behavior_tank_ranged_weapon()  const { return bhv_tank_ranged_weapon_; }
    bool behavior_ammo_supply()         const { return bhv_ammo_supply_; }
    bool behavior_raid_gear()           const { return bhv_raid_gear_; }
    bool behavior_accept_owner_trade()  const { return bhv_accept_owner_trade_; }
    bool behavior_formation()          const { return bhv_formation_; }
    bool behavior_distance_order()      const { return bhv_distance_order_; }
    bool behavior_owner_main_tank()     const { return bhv_owner_main_tank_; }
    bool behavior_heal_claims()         const { return bhv_heal_claims_; }
    bool behavior_cast_facing()         const { return bhv_cast_facing_; }
    bool behavior_ground_snap()         const { return bhv_ground_snap_; }
    bool behavior_re_face()             const { return bhv_re_face_; }
    bool behavior_avoid_circles()       const { return bhv_avoid_circles_; }
    bool behavior_pack_stop()           const { return bhv_pack_stop_; }
    bool behavior_raid_spacing()        const { return bhv_raid_spacing_; }
    bool behavior_spread_out()          const { return bhv_spread_out_; }
    bool behavior_loose_mob_hold()      const { return bhv_loose_mob_hold_; }
    bool behavior_raid_buff_hold()      const { return bhv_raid_buff_hold_; }
    bool behavior_dungeon_loot()        const { return bhv_dungeon_loot_; }
    bool behavior_warrior_stance_dance() const { return bhv_warrior_stance_dance_; }
    bool behavior_bot_whisperable()     const { return bhv_bot_whisperable_; }
    bool chatter_conversation()         const { return chatter_conversation_; }
    bool behavior_world_bot_say_reply() const { return bhv_world_bot_say_reply_; }
    bool behavior_instance_group_cap()  const { return bhv_instance_group_cap_; }
    uint32 behavior_open_world_group_cap() const { return bhv_open_world_group_cap_; }
    bool behavior_bg_human_priority()   const { return bhv_bg_human_priority_; }
    bool behavior_tank_markers()        const { return bhv_tank_markers_; }
    bool behavior_mark_cc()             const { return bhv_mark_cc_; }
    bool behavior_dungeon_flow()        const { return bhv_dungeon_flow_; }
    bool behavior_dungeon_autopilot()   const { return bhv_dungeon_autopilot_; }
    bool behavior_marks_need_order()    const { return bhv_marks_need_order_; }
    bool behavior_pull_recover()        const { return bhv_pull_recover_; }
    bool behavior_loot_policy()         const { return bhv_loot_policy_; }
    bool behavior_wipe_release_wait()   const { return bhv_wipe_release_wait_; }
    bool behavior_ready_check_info()    const { return bhv_ready_check_info_; }
    bool behavior_purge_duty()          const { return bhv_purge_duty_; }
    bool behavior_ranged_slow()         const { return bhv_ranged_slow_; }
    bool behavior_ike3_commands()       const { return bhv_ike3_commands_; }
    bool behavior_owner_commands()      const { return bhv_owner_commands_; }
    bool behavior_fidget()              const { return bhv_fidget_; }
    // Players who log in without the ForeverBots add-on get one line with its download link.
    bool   addon_hint_enabled()         const { return addon_hint_enabled_; }
    uint32 addon_hint_delay_sec()       const { return addon_hint_delay_sec_; }
    std::string const& addon_hint_url() const { return addon_hint_url_; }
    bool behavior_pull_discipline()     const { return bhv_pull_discipline_; }
    bool behavior_solo_flee()           const { return bhv_solo_flee_; }
    bool behavior_druid_heal_form()     const { return bhv_druid_heal_form_; }
    bool behavior_party_banter()        const { return bhv_party_banter_; }
    bool behavior_social_emotes()       const { return bhv_social_emotes_; }
    bool behavior_polite_world_bots()   const { return bhv_polite_world_bots_; }
    bool behavior_quest_accept()        const { return bhv_quest_accept_; }
    bool behavior_quest_share()         const { return bhv_quest_share_; }
    bool behavior_quest_turn_in()       const { return bhv_quest_turn_in_; }
    bool behavior_quest_loot()          const { return bhv_quest_loot_; }

    // Bot pacing. Playerbot.Cast.*: OFF by default (Karrz's VMaNGOS bots change no cast time or
    // haste); speeds are PERCENT of normal cast time and GCD (lower = faster).
    // Playerbot.Pacing.*: the decision cadence model (see Pacing). Plain values, safe to read
    // from any thread.
    struct CastPacing
    {
        bool   enabled          = false;
        uint32 raid_pct         = 80;
        uint32 dungeon_pct      = 90;
        uint32 world_pct        = 100;
        uint32 bg_pct           = 115;
        uint32 world_pvp_pct    = 115;
        uint32 variance_pct     = 8;
        uint32 min_pct          = 70;
        uint32 max_pct          = 140;
    };
    CastPacing const& cast_pacing() const { return cast_pacing_; }

    // Playerbot.Pacing.*, Karrz's VMaNGOS model. decision_delay_ms is a CEILING on how long a
    // bot waits to decide again after a cast (no tier is allowed a longer period; the shipped
    // tiers are already faster, so it never slows a bot). pvp_reaction_delay_ms is a FLOOR on
    // the per-bot bot logic-tick / snapshot period while the bot is in PvP (battleground, active duel,
    // or in combat with players); never in PvE. 0 = off, 0..3000.
    struct Pacing
    {
        uint32 decision_delay_ms     = 600;
        uint32 pvp_reaction_delay_ms = 700;
    };
    Pacing const& pacing() const { return pacing_; }

    // Playerbot.Bg.AutoJoin.*: ambient bots queue for battlegrounds on their own (see
    // BotQueueFiller::AutoJoinTick) and the lowest-scoring bot of a team leaves a full running
    // battleground when a real player is waiting for a place (BotQueueFiller::MakeRoomForRealPlayers).
    struct BgAutoJoin
    {
        bool   enabled         = true;
        uint32 chance_pct      = 3;     // per bot per minute, 0..100
        uint32 max_queued      = 24;    // ambient bots queued fleet-wide at any one time
        uint32 max_per_tick    = 2;     // new queues per 10 s pass
        uint32 cooldown_min    = 20;    // after leaving a BG / queue before the bot queues again
        uint32 queue_timeout_min = 25;  // an ambient bot still waiting this long leaves the queue
        uint32 min_level       = 10;
        bool   make_room       = true;  // MakeRoomForRealPlayers
        std::string types;              // comma list of BattlemasterList ids; empty = built-in list
    };
    BgAutoJoin const& bg_auto_join() const { return bg_auto_join_; }

private:
    CastPacing cast_pacing_;
    Pacing pacing_;
    BgAutoJoin bg_auto_join_;
    void apply_from_loaded_config();
    std::string file_path_;

    // Cached typed values.
    uint32 ai_worker_threads_  = 0;
    bool   fleet_thread_enabled_ = true;
    uint32 tick_budget_ms_ = 10;
    bool   parallel_snapshot_build_ = true;
    uint32 snapshot_build_threads_  = 0;

    uint32 pop_target_   = 0;      // Classic default: no ambient world bots
    uint8  pop_horde_pct_ = 50;
    std::string pop_shape_ = "MaxHeavy";
    float  pop_max_heavy_factor_ = 0.40f;

    std::string accept_invites_ = "friends_or_higher";
    bool        auto_leader_handover_ = true;
    char        command_prefix_ = '!';

    bool   raid_enabled_     = true;
    bool   boss_warnings_enabled_ = true;
    bool   raid_auto_accept_rez_ = true;

    bool   housing_enabled_  = true;
    uint8  nbhd_target_pct_  = 60;
    bool   housing_strict_   = true;

    uint16 health_port_ = 0;
    std::string log_level_ = "info";

    bool   auto_resume_on_boot_ = false;
    uint32 auto_resume_cap_     = 100;
    uint32 auto_spawn_on_boot_  = 0;
    bool   random_personality_  = true;
    bool   archetype_enabled_   = true;
    bool   economy_buy_enabled_ = true;
    bool   economy_bot_auctions_enabled_ = false;
    bool   classic_class_supplies_ = true;
    uint32 economy_max_reagent_vendor_multiple_ = 15;
    bool   session_rhythm_enabled_               = true;
    float  session_rhythm_multiplier_            = 1.0f;
    uint32 session_rhythm_max_logouts_per_cycle_ = 25;

    bool   guilds_enabled_                           = true;
    uint8  guilds_target_count_per_faction_          = 6;
    uint16 guilds_max_members_per_guild_             = 75;
    bool   guilds_events_enabled_                    = true;
    bool   guilds_recruitment_channel_enabled_       = true;
    uint8  max_alts_as_bots_                         = 5;
    uint32 wedge_watchdog_threshold_ms_              = 90000;
    uint32 wedge_watchdog_combat_threshold_ms_       = 180000;
    float  wedge_watchdog_min_displacement_          = 20.0f;
    bool   wedge_remediation_enabled_                = true;   // temp-skip UNREACHABLE objectives (GoalUnreachable/CombatLoop only; NoProgress never abandons); detect+track always-on
    uint32 wedge_remediation_ms_                     = 180000;
    bool   wedge_noprogress_enabled_                 = true;
    uint32 wedge_noprogress_ms_                      = 900000;   // 15 min stall window
    float  wedge_noprogress_radius_                  = 150.0f;   // net travel = not stuck

    uint32 alert_wedged_bots_         = 5;
    uint32 alert_tick_p99_us_         = 200000;   // 200ms
    uint32 alert_intent_drop_per_min_ = 600;      // ~10/sec sustained
    uint32 alert_path_fail_per_min_   = 300;
    uint32 alert_throttle_ms_         = 300000;   // 5 min

    bool  pull_gate_enabled_         = true;
    float pull_gate_max_ratio_       = 3.0f;
    float pull_gate_min_extra_yards_ = 40.0f;
    bool  pull_gate_disengage_enabled_ = true;
    bool   tank_commit_enabled_  = true;
    uint32 tank_commit_max_ms_   = 45000;
    bool   move_step_hold_enabled_ = true;
    bool   route_aware_combat_advance_ = true;
    bool   combat_skip_unfightable_    = true;
    bool   threat_hold_enabled_          = true;
    bool   raid_dps_wait_attack_         = true;
    bool   raid_provide_consumables_     = false;
    bool   consumables_pve_              = true;
    bool   consumables_pvp_              = false;
    uint32 raid_dps_wait_timeout_ms_     = 10000;
    bool   threat_hold_open_world_       = false;
    float  threat_max_ratio_             = 0.9f;
    float  threat_opening_lead_boss_     = 900.0f;
    float  threat_opening_lead_trash_    = 250.0f;
    uint32 threat_opening_wait_boss_ms_  = 6000;
    uint32 threat_opening_wait_trash_ms_ = 2000;

    bool   bhv_owner_follow_hold_    = true;
    bool   bhv_fallback_             = true;
    bool   bhv_pull_back_            = true;
    bool   bhv_tank_ranged_weapon_   = true;
    bool   bhv_ammo_supply_          = true;
    bool   bhv_raid_gear_            = true;
    bool   bhv_accept_owner_trade_   = true;
    bool   bhv_formation_            = true;
    bool   bhv_distance_order_       = true;
    bool   bhv_owner_main_tank_      = true;
    bool   bhv_heal_claims_          = true;
    bool   bhv_cast_facing_          = true;
    bool   bhv_ground_snap_          = true;
    bool   bhv_re_face_              = true;
    bool   bhv_avoid_circles_        = true;
    bool   bhv_pack_stop_            = true;
    bool   bhv_raid_spacing_         = true;
    bool   bhv_spread_out_           = true;
    bool   bhv_loose_mob_hold_       = true;
    bool   bhv_raid_buff_hold_       = true;
    bool   bhv_dungeon_loot_         = true;
    bool   bhv_warrior_stance_dance_ = true;
    bool   bhv_bot_whisperable_      = true;
    bool   chatter_conversation_     = true;
    bool   bhv_world_bot_say_reply_  = true;
    bool   bhv_instance_group_cap_   = false;
    uint32 bhv_open_world_group_cap_ = 5;
    bool   bhv_bg_human_priority_    = true;
    bool   bhv_tank_markers_         = true;
    bool   bhv_mark_cc_              = true;
    bool   bhv_dungeon_flow_         = true;
    bool   bhv_dungeon_autopilot_    = true;
    bool   bhv_marks_need_order_     = true;
    bool   bhv_pull_recover_         = true;
    bool   bhv_loot_policy_          = true;
    bool   bhv_wipe_release_wait_    = true;
    bool   bhv_ready_check_info_     = true;
    bool   bhv_purge_duty_           = true;
    bool   bhv_ranged_slow_          = true;
    bool   bhv_ike3_commands_        = true;
    bool   bhv_owner_commands_       = true;
    bool   bhv_fidget_               = true;
    bool        addon_hint_enabled_   = false;
    uint32      addon_hint_delay_sec_ = 30;
    std::string addon_hint_url_       = "https://github.com/Karrz1/Karrz-playerbots-wow-forever/raw/main/addon/ForeverBots-addon.zip";
    bool   bhv_pull_discipline_      = true;
    bool   bhv_solo_flee_            = true;
    bool   bhv_druid_heal_form_      = true;
    bool   bhv_party_banter_         = true;
    bool   bhv_social_emotes_        = true;
    bool   bhv_polite_world_bots_    = true;
    bool   bhv_quest_accept_         = true;
    bool   bhv_quest_share_          = true;
    bool   bhv_quest_turn_in_        = true;
    bool   bhv_quest_loot_           = true;
};

} // namespace Playerbot
