#include "ConfigReader.h"
#include "Config.h"
#include "Log.h"
#include <algorithm>

namespace Playerbot {

bool ConfigReader::load(std::string const& file_path)
{
    file_path_ = file_path;

    std::vector<std::string> args;   // No CLI overrides; world.conf takes care of those.
    std::string err;

    // TrinityCore's sConfigMgr is a singleton; we layer our keys onto its state.
    // If the file doesn't exist, fall through to defaults silently.
    if (!file_path_.empty() && !sConfigMgr->LoadAdditionalFile(file_path_, true, err))
    {
        TC_LOG_WARN("playerbot.v2", "[PlayerbotV2] playerbot.conf missing or unreadable ({}): {}", file_path_, err);
        // Defaults are acceptable; not a hard failure.
    }
    apply_from_loaded_config();

    // B-1 observability: playerbot.conf is layered with override=true, so
    // a Playerbot.* key edited in worldserver.conf is silently beaten by
    // the same key in playerbot.conf — operators repeatedly chased "my
    // TotalTarget change does nothing". State the effective values and
    // the precedence once per load/reload so the log answers it.
    TC_LOG_INFO("playerbot.v2",
        "[Config] effective population: TotalTarget={} "
        "HordePct={} Shape={} MaxHeavyFactor={:.2f} (Playerbot.* keys in '{}' override worldserver.conf)",
        pop_target_,
        pop_horde_pct_, pop_shape_, pop_max_heavy_factor_,
        file_path_.empty() ? "<none>" : file_path_);
    return true;
}

bool ConfigReader::reload()
{
    return load(file_path_);
}

void ConfigReader::apply_from_loaded_config()
{
    auto& c = *sConfigMgr;

    ai_worker_threads_     = c.GetIntDefault ("Playerbot.AiWorkerThreads",       0, true);
    fleet_thread_enabled_  = c.GetBoolDefault("Playerbot.FleetThreadEnabled",    true, true);
    tick_budget_ms_        = c.GetIntDefault ("Playerbot.TickBudgetMs",          10, true);
    // #5 Phase 4 parallel snapshot Build. DEFAULT ON. The 2026-06-15 crash
    // (ACCESS_VIOLATION in Build->objective_blacklisted, serial AND parallel) was
    // ultimately ABI SKEW: another session's uncommitted BotAI.h change (added
    // quest_mem_mtx_ + 5 maps, changing BotAI's layout) was only partially picked
    // up by incremental builds — 317 playerbot .obj remained compiled against the
    // OLD layout, so TUs disagreed on objective_blacklist_'s offset -> garbage
    // read. Fixed by a clean recompile of all playerbot TUs. The genuine parallel
    // concern (concurrent Registry().ai() rehash) was separately fixed by
    // pre-resolving the BotAI* on the world thread. Kill-switch: set =0 to serial.
    parallel_snapshot_build_ = c.GetBoolDefault("PlayerbotV2.ParallelSnapshotBuild", true, true);
    snapshot_build_threads_  = c.GetIntDefault ("PlayerbotV2.SnapshotBuildThreads", 0, true);

    // Classic 1.60 (Forever) default: 0 = no ambient world bots, owner bots only.
    pop_target_     = c.GetIntDefault ("Playerbot.Population.TotalTarget", 0, true);
    pop_horde_pct_  = static_cast<uint8>(c.GetIntDefault("Playerbot.Population.HordePct", 50, true));
    pop_shape_      = c.GetStringDefault("Playerbot.Population.Shape",     "MaxHeavy", true);
    pop_max_heavy_factor_ = float(c.GetFloatDefault("Playerbot.Population.MaxHeavyFactor", 0.40f, true));

    accept_invites_       = c.GetStringDefault("Playerbot.Interact.AutoAcceptInvites", "friends_or_higher", true);
    auto_leader_handover_ = c.GetBoolDefault  ("Playerbot.Interact.AutoLeaderHandover", true, true);
    {
        std::string prefix = c.GetStringDefault("Playerbot.Interact.CommandPrefix", "!", true);
        command_prefix_ = prefix.empty() ? '!' : prefix[0];
    }

    raid_enabled_      = c.GetBoolDefault("Playerbot.Raid.Enable", true, true);
    boss_warnings_enabled_ = c.GetBoolDefault("Playerbot.Raid.BossWarnings", true, true);
    raid_auto_accept_rez_ = c.GetBoolDefault("Playerbot.Raid.AutoAcceptOwnerRez", true, true);

    housing_enabled_   = c.GetBoolDefault("Playerbot.Housing.Enabled", false, true);
    nbhd_target_pct_   = static_cast<uint8>(c.GetIntDefault("Playerbot.Housing.NeighborhoodTargetOccupancy", 60, true));
    {
        std::string strict = c.GetStringDefault("Playerbot.Housing.PlotPurchaseEligibilityCheck", "strict", true);
        housing_strict_ = (strict != "lenient");
    }

    health_port_ = static_cast<uint16>(c.GetIntDefault("Playerbot.Diag.HealthEndpointPort", 0, true));
    log_level_   = c.GetStringDefault("Playerbot.Log.Level", "info", true);

    auto_resume_on_boot_ = c.GetBoolDefault("Playerbot.V2.AutoResumeOnBoot", false, true);
    auto_resume_cap_     = c.GetIntDefault ("Playerbot.V2.AutoResumeCap",    100, true);
    auto_spawn_on_boot_  = c.GetIntDefault ("Playerbot.V2.AutoSpawnOnBoot",  0, true);
    random_personality_  = c.GetBoolDefault("Playerbot.V2.RandomPersonality", true, true);
    archetype_enabled_   = c.GetBoolDefault("PlayerbotV2.Archetype.Enabled", true, true);
    economy_buy_enabled_ = c.GetBoolDefault("PlayerbotV2.Economy.BuyEnabled", true, true);
    economy_bot_auctions_enabled_ = c.GetBoolDefault("PlayerbotV2.Economy.BotAuctions", false, true);
    classic_class_supplies_ = c.GetBoolDefault("PlayerbotV2.Classic.ClassSupplies", true, true);
    economy_max_reagent_vendor_multiple_ = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.Economy.MaxReagentVendorMultiple", 15, true));
    session_rhythm_enabled_    = c.GetBoolDefault("PlayerbotV2.SessionRhythm.Enabled", false, true);
    session_rhythm_multiplier_ = float(c.GetFloatDefault("PlayerbotV2.SessionRhythm.Multiplier", 1.0f, true));
    session_rhythm_max_logouts_per_cycle_ = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.SessionRhythm.MaxLogoutsPerCycle", 25, true));

    guilds_enabled_                          = c.GetBoolDefault("PlayerbotV2.Guilds.Enabled", false, true);
    guilds_target_count_per_faction_         = static_cast<uint8>(c.GetIntDefault("PlayerbotV2.Guilds.TargetCountPerFaction", 6, true));
    guilds_max_members_per_guild_            = static_cast<uint16>(c.GetIntDefault("PlayerbotV2.Guilds.MaxMembersPerGuild", 75, true));
    guilds_events_enabled_                   = c.GetBoolDefault("PlayerbotV2.Guilds.EventsEnabled", true, true);
    guilds_recruitment_channel_enabled_      = c.GetBoolDefault("PlayerbotV2.Guilds.RecruitmentChannelEnabled", true, true);

    max_alts_as_bots_ = static_cast<uint8>(
        c.GetIntDefault("PlayerbotV2.MaxAltsAsBots", 5, true));

    wedge_watchdog_threshold_ms_ = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.WedgeWatchdog.ThresholdMs", 90000, true));
    wedge_watchdog_combat_threshold_ms_ = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.WedgeWatchdog.CombatThresholdMs", 180000, true));
    wedge_watchdog_min_displacement_ = static_cast<float>(
        c.GetFloatDefault("PlayerbotV2.WedgeWatchdog.MinDisplacementYards", 20.0f, true));
    wedge_remediation_enabled_ = c.GetBoolDefault("PlayerbotV2.WedgeWatchdog.RemediationEnabled", true, true);
    wedge_remediation_ms_ = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.WedgeWatchdog.RemediationMs", 180000, true));
    wedge_noprogress_enabled_ = c.GetBoolDefault("PlayerbotV2.WedgeWatchdog.NoProgressEnabled", true, true);
    wedge_noprogress_ms_ = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.WedgeWatchdog.NoProgressMs", 900000, true));
    wedge_noprogress_radius_ = static_cast<float>(
        c.GetFloatDefault("PlayerbotV2.WedgeWatchdog.NoProgressRadiusYards", 150.0f, true));

    alert_wedged_bots_         = static_cast<uint32>(c.GetIntDefault("PlayerbotV2.Alert.WedgedBots",        5, true));
    alert_tick_p99_us_         = static_cast<uint32>(c.GetIntDefault("PlayerbotV2.Alert.TickP99Us",         200000, true));
    alert_intent_drop_per_min_ = static_cast<uint32>(c.GetIntDefault("PlayerbotV2.Alert.IntentDropPerMin",  600, true));
    alert_path_fail_per_min_   = static_cast<uint32>(c.GetIntDefault("PlayerbotV2.Alert.PathFailPerMin",     300, true));
    alert_throttle_ms_         = static_cast<uint32>(c.GetIntDefault("PlayerbotV2.Alert.ThrottleMs",         300000, true));

    pull_gate_enabled_         = c.GetBoolDefault("PlayerbotV2.PullGate.Enabled",       true, true);
    pull_gate_max_ratio_       = float(c.GetFloatDefault("PlayerbotV2.PullGate.MaxDetourRatio", 3.0f, true));
    pull_gate_min_extra_yards_ = float(c.GetFloatDefault("PlayerbotV2.PullGate.MinExtraYards",  40.0f, true));
    pull_gate_disengage_enabled_ = c.GetBoolDefault("PlayerbotV2.PullGate.DisengageEnabled", true, true);
    tank_commit_enabled_  = c.GetBoolDefault("PlayerbotV2.PullGate.TankCommitEnabled", true, true);
    tank_commit_max_ms_   = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.PullGate.TankCommitMaxMs", 45000, true));

    move_step_hold_enabled_ = c.GetBoolDefault("PlayerbotV2.Move.StepHoldEnabled", true, true);
    route_aware_combat_advance_ = c.GetBoolDefault(
        "PlayerbotV2.Move.RouteAwareCombatAdvance", true, true);
    combat_skip_unfightable_ = c.GetBoolDefault(
        "PlayerbotV2.Combat.SkipUnfightable", true, true);

    threat_hold_enabled_    = c.GetBoolDefault("PlayerbotV2.Threat.HoldEnabled", true, true);
    raid_dps_wait_attack_   = c.GetBoolDefault("Playerbot.Raid.DpsWaitForAttack", true, true);
    raid_provide_consumables_ = c.GetBoolDefault("Playerbot.Raid.ProvideConsumables", true, true);
    consumables_pve_          = c.GetBoolDefault("Playerbot.Consumables.PvE", true, true);
    consumables_pvp_          = c.GetBoolDefault("Playerbot.Consumables.PvP", false, true);
    raid_dps_wait_timeout_ms_ = uint32(std::clamp<int64>(int64(c.GetIntDefault("Playerbot.Raid.DpsWaitTimeoutMs", 6000, true)), 1000, 6000));
    threat_hold_open_world_ = c.GetBoolDefault("PlayerbotV2.Threat.HoldInOpenWorld", false, true);
    threat_max_ratio_ = std::clamp(float(c.GetFloatDefault("PlayerbotV2.Threat.MaxRatio", 0.9f, true)), 0.3f, 1.2f);
    threat_opening_lead_boss_  = std::max(0.0f, float(c.GetFloatDefault("PlayerbotV2.Threat.OpeningLeadBoss", 900.0f, true)));
    threat_opening_lead_trash_ = std::max(0.0f, float(c.GetFloatDefault("PlayerbotV2.Threat.OpeningLeadTrash", 250.0f, true)));
    threat_opening_wait_boss_ms_  = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.Threat.OpeningWaitBossMs", 6000, true));
    threat_opening_wait_trash_ms_ = static_cast<uint32>(
        c.GetIntDefault("PlayerbotV2.Threat.OpeningWaitTrashMs", 2000, true));

    bhv_owner_follow_hold_    = c.GetBoolDefault("Playerbot.Behavior.OwnerFollowHold", true, true);
    bhv_fallback_             = c.GetBoolDefault("Playerbot.Behavior.Fallback", true, true);
    bhv_pull_back_            = c.GetBoolDefault("Playerbot.Behavior.PullBack", true, true);
    bhv_tank_ranged_weapon_   = c.GetBoolDefault("Playerbot.Behavior.TankRangedWeapon", true, true);
    bhv_ammo_supply_          = c.GetBoolDefault("Playerbot.Behavior.AmmoSupply", true, true);
    bhv_raid_gear_            = c.GetBoolDefault("Playerbot.Behavior.RaidGear", true, true);
    bhv_accept_owner_trade_   = c.GetBoolDefault("Playerbot.Behavior.AcceptOwnerTrade", true, true);
    bhv_formation_            = c.GetBoolDefault("Playerbot.Behavior.Formation", true, true);
    bhv_distance_order_       = c.GetBoolDefault("Playerbot.Behavior.DistanceOrder", true, true);
    bhv_owner_main_tank_      = c.GetBoolDefault("Playerbot.Behavior.OwnerMainTank", true, true);
    bhv_heal_claims_          = c.GetBoolDefault("Playerbot.Behavior.HealClaims", true, true);
    bhv_cast_facing_          = c.GetBoolDefault("Playerbot.Behavior.CastFacing", true, true);
    bhv_ground_snap_          = c.GetBoolDefault("Playerbot.Behavior.GroundSnap", true, true);
    bhv_re_face_              = c.GetBoolDefault("Playerbot.Behavior.ReFace", true, true);
    bhv_avoid_circles_        = c.GetBoolDefault("Playerbot.Behavior.AvoidCircles", true, true);
    bhv_pack_stop_            = c.GetBoolDefault("Playerbot.Behavior.PackStop", true, true);
    bhv_raid_spacing_         = c.GetBoolDefault("Playerbot.Behavior.RaidSpacing", true, true);
    bhv_spread_out_           = c.GetBoolDefault("Playerbot.Behavior.SpreadOut", true, true);
    bhv_loose_mob_hold_       = c.GetBoolDefault("Playerbot.Behavior.LooseMobHold", true, true);
    bhv_raid_buff_hold_       = c.GetBoolDefault("Playerbot.Behavior.RaidBuffHold", true, true);
    bhv_dungeon_loot_         = c.GetBoolDefault("Playerbot.Behavior.DungeonLoot", true, true);
    bhv_warrior_stance_dance_ = c.GetBoolDefault("Playerbot.Behavior.WarriorStanceDance", true, true);
    bhv_bot_whisperable_      = c.GetBoolDefault("Playerbot.Behavior.BotWhisperable", true, true);
    chatter_conversation_     = c.GetBoolDefault("Playerbot.Chatter.Conversation", true, true);
    bhv_world_bot_say_reply_  = c.GetBoolDefault("Playerbot.Behavior.WorldBotSayReply", true, true);
    bhv_instance_group_cap_   = c.GetBoolDefault("Playerbot.Behavior.InstanceGroupCap", false, true);
    bhv_open_world_group_cap_ = std::clamp<int32>(c.GetIntDefault("Playerbot.Behavior.OpenWorldGroupCap", 5, true), 1, 40);
    bhv_bg_human_priority_    = c.GetBoolDefault("Playerbot.Behavior.BgHumanPriority", true, true);
    bhv_tank_markers_         = c.GetBoolDefault("Playerbot.Behavior.TankMarkers", true, true);
    bhv_mark_cc_              = c.GetBoolDefault("Playerbot.Behavior.MarkCc", true, true);
    bhv_dungeon_flow_         = c.GetBoolDefault("Playerbot.Behavior.DungeonFlow", true, true);
    bhv_dungeon_autopilot_    = c.GetBoolDefault("Playerbot.Behavior.DungeonAutopilot", true, true);
    bhv_marks_need_order_     = c.GetBoolDefault("Playerbot.Behavior.MarksNeedOrder", true, true);
    bhv_pull_recover_         = c.GetBoolDefault("Playerbot.Behavior.PullRecover", true, true);
    bhv_loot_policy_          = c.GetBoolDefault("Playerbot.Behavior.LootPolicy", true, true);
    bhv_wipe_release_wait_    = c.GetBoolDefault("Playerbot.Behavior.WipeReleaseWait", true, true);
    bhv_ready_check_info_     = c.GetBoolDefault("Playerbot.Behavior.ReadyCheckInfo", true, true);
    bhv_purge_duty_           = c.GetBoolDefault("Playerbot.Behavior.PurgeDuty", true, true);
    bhv_ranged_slow_          = c.GetBoolDefault("Playerbot.Behavior.RangedSlow", true, true);
    bhv_ike3_commands_        = c.GetBoolDefault("Playerbot.Behavior.Ike3Commands", true, true);
    bhv_owner_commands_       = c.GetBoolDefault("Playerbot.Behavior.OwnerCommands", true, true);
    bhv_fidget_               = c.GetBoolDefault("Playerbot.Behavior.Fidget", true, true);
    addon_hint_enabled_       = c.GetBoolDefault("Playerbot.AddonHint.Enable", false, true);
    addon_hint_delay_sec_     = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.AddonHint.DelaySec", 30, true), 5, 600));
    addon_hint_url_           = c.GetStringDefault("Playerbot.AddonHint.Url", addon_hint_url_, true);
    bhv_pull_discipline_      = c.GetBoolDefault("Playerbot.Behavior.PullDiscipline", true, true);
    bhv_solo_flee_            = c.GetBoolDefault("Playerbot.Behavior.SoloFlee", true, true);
    bhv_druid_heal_form_      = c.GetBoolDefault("Playerbot.Behavior.DruidHealForm", true, true);
    bhv_party_banter_         = c.GetBoolDefault("Playerbot.Behavior.PartyBanter", true, true);
    bhv_social_emotes_        = c.GetBoolDefault("Playerbot.Behavior.SocialEmotes", true, true);
    bhv_polite_world_bots_    = c.GetBoolDefault("Playerbot.Behavior.PoliteWorldBots", true, true);
    bhv_quest_accept_         = c.GetBoolDefault("Playerbot.Behavior.QuestAccept", true, true);
    bhv_quest_share_          = c.GetBoolDefault("Playerbot.Behavior.QuestShare", true, true);
    bhv_quest_turn_in_        = c.GetBoolDefault("Playerbot.Behavior.QuestTurnIn", true, true);
    bhv_quest_loot_           = c.GetBoolDefault("Playerbot.Behavior.QuestLoot", true, true);

    {
        auto pct = [&](char const* key, int32 def, int32 lo, int32 hi) {
            return uint32(std::clamp<int32>(c.GetIntDefault(key, def, true), lo, hi));
        };
        CastPacing p;
        p.enabled       = c.GetBoolDefault("Playerbot.Cast.Enable", false, true);
        p.raid_pct      = pct("Playerbot.Cast.RaidSpeedPct",     75, 10, 300);
        p.dungeon_pct   = pct("Playerbot.Cast.DungeonSpeedPct",  90, 10, 300);
        p.world_pct     = pct("Playerbot.Cast.WorldSpeedPct",   100, 10, 300);
        p.bg_pct        = pct("Playerbot.Cast.BgSpeedPct",      115, 10, 300);
        p.world_pvp_pct = pct("Playerbot.Cast.WorldPvpSpeedPct", 115, 10, 300);
        p.variance_pct  = pct("Playerbot.Cast.PerBotVariancePct", 8, 0, 30);
        p.min_pct       = pct("Playerbot.Cast.MinPct",           70, 10, 100);
        p.max_pct       = pct("Playerbot.Cast.MaxPct",          140, 100, 300);
        cast_pacing_ = p;
        Pacing q;
        q.decision_delay_ms     = pct("Playerbot.Pacing.DecisionDelayMs",     600, 100, 5000);
        q.pvp_reaction_delay_ms = pct("Playerbot.Pacing.PvpReactionDelayMs", 700, 0, 3000);
        pacing_ = q;
        BgAutoJoin j;
        j.enabled          = c.GetBoolDefault("Playerbot.Bg.AutoJoin", false, true);
        j.chance_pct       = pct("Playerbot.Bg.AutoJoin.Chance",          3, 0, 100);
        j.max_queued       = pct("Playerbot.Bg.AutoJoin.MaxQueued",      24, 0, 500);
        j.max_per_tick     = pct("Playerbot.Bg.AutoJoin.MaxPerTick",      2, 1, 50);
        j.cooldown_min     = pct("Playerbot.Bg.AutoJoin.CooldownMin",    20, 0, 600);
        j.queue_timeout_min = pct("Playerbot.Bg.AutoJoin.QueueTimeoutMin", 25, 1, 240);
        j.min_level        = pct("Playerbot.Bg.AutoJoin.MinLevel",       10, 1, 255);
        j.make_room        = c.GetBoolDefault("Playerbot.Bg.MakeRoomForPlayers", true, true);
        j.types            = c.GetStringDefault("Playerbot.Bg.AutoJoin.Types", "", true);
        bg_auto_join_ = std::move(j);
    }
}

} // namespace Playerbot
