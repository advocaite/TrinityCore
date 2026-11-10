#include "ConfigReader.h"
#include "../Bot/ThreatLimits.h"
#include "Config.h"
#include "Log.h"
#include "StringConvert.h"
#include "Util.h"
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
    pop_spawn_cap_factor_ = std::max(1.0f, float(c.GetFloatDefault("Playerbot.Population.SpawnCapFactor", 1.10f, true)));

    accept_invites_       = c.GetStringDefault("Playerbot.Interact.AutoAcceptInvites", "friends_or_higher", true);
    auto_leader_handover_ = c.GetBoolDefault  ("Playerbot.Interact.AutoLeaderHandover", true, true);
    {
        std::string prefix = c.GetStringDefault("Playerbot.Interact.CommandPrefix", "!", true);
        command_prefix_ = prefix.empty() ? '!' : prefix[0];
    }

    raid_enabled_      = c.GetBoolDefault("Playerbot.Raid.Enable", true, true);
    boss_warnings_enabled_ = c.GetBoolDefault("Playerbot.Raid.BossWarnings", true, true);
    raid_callouts_enabled_ = c.GetBoolDefault("Playerbot.Raid.Callouts", true, true);
    raid_banter_ = c.GetIntDefault("Playerbot.Raid.Banter", 1, true);
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

    guilds_enabled_                          = c.GetBoolDefault("PlayerbotV2.Guilds.Enabled", true, true);
    guilds_target_count_per_faction_         = static_cast<uint8>(c.GetIntDefault("PlayerbotV2.Guilds.TargetCountPerFaction", 6, true));
    guilds_max_members_per_guild_            = static_cast<uint16>(c.GetIntDefault("PlayerbotV2.Guilds.MaxMembersPerGuild", 75, true));
    guilds_events_enabled_                   = c.GetBoolDefault("PlayerbotV2.Guilds.EventsEnabled", true, true);
    guilds_recruitment_channel_enabled_      = c.GetBoolDefault("PlayerbotV2.Guilds.RecruitmentChannelEnabled", true, true);
    guilds_invite_players_                   = c.GetBoolDefault("PlayerbotV2.Guilds.InvitePlayers", false, true);
    guilds_bank_deposit_                     = c.GetBoolDefault("PlayerbotV2.Guilds.BankDeposit", false, true);

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
    raid_consumables_in_dungeons_ = c.GetBoolDefault("Playerbot.Raid.ConsumablesInDungeons", true, true);
    raid_resist_gear_         = c.GetBoolDefault("Playerbot.Raid.ResistGear", false, true);
    raid_onyxia_cloak_        = c.GetBoolDefault("Playerbot.Raid.OnyxiaCloak", true, true);
    consumables_pve_          = c.GetBoolDefault("Playerbot.Consumables.PvE", true, true);
    consumables_pvp_          = c.GetBoolDefault("Playerbot.Consumables.PvP", false, true);
    {
        static constexpr char const* kSwitch[kAlwaysSwitchCount] = { "Supplies", "Repair", "WorldBuffs", "Consumables" };
        static constexpr char const* kPlace[kAlwaysPlaceCount]   = { "Raid", "World", "Battleground" };
        for (uint8 w = 0; w < kAlwaysSwitchCount; ++w)
            for (uint8 p = 0; p < kAlwaysPlaceCount; ++p)
            {
                // Supplies and Repair on everywhere; world buffs and consumables on in raids and dungeons only.
                bool const def = w == uint8(AlwaysSwitch::Supplies) || w == uint8(AlwaysSwitch::Repair) || p == uint8(AlwaysPlace::Raid);
                always_[w][p] = c.GetBoolDefault(std::string("Playerbot.AlwaysBuffs.") + kSwitch[w] + "." + kPlace[p], def, true);
            }
    }
    raid_dps_wait_timeout_ms_ = uint32(std::clamp<int64>(int64(c.GetIntDefault("Playerbot.Raid.DpsWaitTimeoutMs", int(kRaidDpsWaitTimeoutMaxMs), true)), 1000, int64(kRaidDpsWaitTimeoutMaxMs)));
    threat_hold_open_world_ = c.GetBoolDefault("PlayerbotV2.Threat.HoldInOpenWorld", true, true);
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
    gear_raid_enchants_       = c.GetBoolDefault("Playerbot.Gear.RaidEnchants", true, true);
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
    {
        ChatModelConfig& m = chat_model_;
        m.enable            = c.GetBoolDefault("Playerbot.ChatModel.Enable", false, true);
        m.whispers          = c.GetBoolDefault("Playerbot.ChatModel.Whispers", true, true);
        m.party             = c.GetBoolDefault("Playerbot.ChatModel.Party", true, true);
        m.say_reply         = c.GetBoolDefault("Playerbot.ChatModel.Say", true, true);
        m.host              = c.GetStringDefault("Playerbot.ChatModel.Host", "127.0.0.1", true);
        m.port              = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.Port", 11434, true), 1, 65535));
        m.model             = c.GetStringDefault("Playerbot.ChatModel.Model", "llama3.2:3b", true);
        m.timeout_ms        = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.TimeoutMs", 45000, true), 1000, 120000));
        m.max_tokens        = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.MaxTokens", 60, true), 10, 400));
        m.temperature       = std::clamp(float(c.GetFloatDefault("Playerbot.ChatModel.Temperature", 0.8f, true)), 0.0f, 2.0f);
        m.keep_alive        = c.GetStringDefault("Playerbot.ChatModel.KeepAlive", "30m", true);
        m.think             = c.GetBoolDefault("Playerbot.ChatModel.Think", false, true);
        m.context_length    = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.ContextLength", 2048, true), 512, 32768));
        m.party_chance      = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.PartyChance", 100, true), 0, 100));
        m.second_reply_chance = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.SecondReplyChance", 35, true), 0, 100));
        m.quiet_chance      = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.QuietChance", 30, true), 0, 100));
        m.say_chance        = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.SayChance", 100, true), 0, 100));
        m.say_per_minute    = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.SayPerMinute", 10, true), 0, 600));
        m.date_in_prompt    = c.GetBoolDefault("Playerbot.ChatModel.DateInPrompt", true, true);
        m.bot_cooldown_ms   = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.BotCooldownMs", 3000, true), 0, 600000));
        m.group_cooldown_ms = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.GroupCooldownMs", 0, true), 0, 600000));
        m.max_per_minute    = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.MaxPerMinute", 30, true), 1, 600));
        m.max_queue         = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.MaxQueue", 6, true), 1, 64));
        m.max_wait_ms       = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.MaxWaitMs", 60000, true), 2000, 300000));
        m.history_lines     = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.HistoryLines", 8, true), 0, 20));
        m.history_minutes   = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.ChatModel.HistoryMinutes", 10, true), 1, 240));
        m.language          = c.GetStringDefault("Playerbot.ChatModel.Language", "auto", true);
        m.extra_prompt      = c.GetStringDefault("Playerbot.ChatModel.ExtraPrompt", "", true);
    }
    {
        BridgeConfig& b = bridge_;
        auto num = [&](char const* key, int32 def, int32 lo, int32 hi) { return uint32(std::clamp<int32>(c.GetIntDefault(key, def, true), lo, hi)); };
        b.enable             = c.GetBoolDefault("Playerbot.Bridge.Enable", false, true);
        b.folder             = c.GetStringDefault("Playerbot.Bridge.Folder", "BotBridge", true);
        b.crash_folder       = c.GetStringDefault("Playerbot.Bridge.CrashFolder", "Crashes", true);
        b.chat               = c.GetBoolDefault("Playerbot.Bridge.Chat", true, true);
        b.events             = c.GetBoolDefault("Playerbot.Bridge.Events", true, true);
        b.prompt_in_chat     = c.GetBoolDefault("Playerbot.Bridge.PromptInChat", true, true);
        b.client_timeout_sec = num("Playerbot.Bridge.ClientTimeoutSec", 120, 5, 86400);
        b.event_cooldown_sec = num("Playerbot.Bridge.EventCooldownSec", 300, 0, 86400);
        b.max_events_per_sec = num("Playerbot.Bridge.MaxEventsPerSec", 30, 1, 1000);
        b.reply_max_age_sec  = num("Playerbot.Bridge.ReplyMaxAgeSec", 60, 5, 3600);
        b.bot_line_gap_ms    = num("Playerbot.Bridge.BotLineGapMs", 8000, 0, 600000);
        b.max_line_bytes     = num("Playerbot.Bridge.MaxLineBytes", 200, 20, 255);
        b.rotate_mb          = num("Playerbot.Bridge.RotateMB", 20, 1, 1024);
        b.keep_days          = num("Playerbot.Bridge.KeepDays", 3, 1, 365);
        b.spell_fail_count   = num("Playerbot.Bridge.SpellFailCount", 5, 2, 1000);
        b.death_loop_count   = num("Playerbot.Bridge.DeathLoopCount", 3, 2, 100);
        b.read_ms            = num("Playerbot.Bridge.ReadMs", 200, 50, 5000);
        b.swearing           = c.GetBoolDefault("Playerbot.Bridge.Swearing", false, true);
        b.moments            = c.GetBoolDefault("Playerbot.Bridge.Moments", true, true);
        b.moments_per_minute = num("Playerbot.Bridge.MomentsPerMinute", 6, 0, 120);
        b.talk               = c.GetBoolDefault("Playerbot.Bridge.Talk", true, true);
        b.actions                = c.GetBoolDefault("Playerbot.Bridge.Actions.Enable", false, true);
        b.actions_owned_bots     = c.GetBoolDefault("Playerbot.Bridge.Actions.OwnedBots", false, true);
        b.actions_allowed        = c.GetStringDefault("Playerbot.Bridge.Actions.Allowed", "all", true);
        b.actions_bot_gap_ms     = num("Playerbot.Bridge.Actions.BotGapMs", 5000, 1000, 600000);
        b.actions_per_sec        = num("Playerbot.Bridge.Actions.MaxPerSec", 20, 1, 200);
        b.actions_max_age_sec    = num("Playerbot.Bridge.Actions.MaxAgeSec", 30, 5, 600);
        b.actions_follow_minutes = num("Playerbot.Bridge.Actions.FollowMinutes", 10, 1, 60);
        b.actions_max_goto_yards = num("Playerbot.Bridge.Actions.MaxGoToYards", 200, 10, 500);
        b.actions_near_yards     = num("Playerbot.Bridge.Actions.NearYards", 60, 20, 100);
        b.actions_spells         = c.GetBoolDefault("Playerbot.Bridge.Actions.Spells", true, true);
    }
    chatter_channel_replies_  = c.GetBoolDefault("Playerbot.Chatter.ChannelReplies.Enable", true, true);
    chatter_channel_question_pct_ = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.Chatter.ChannelReplies.QuestionPct", 60, true), 0, 100));
    chatter_channel_greet_pct_    = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.Chatter.ChannelReplies.GreetPct", 40, true), 0, 100));
    chatter_lfg_channel_      = c.GetBoolDefault("Playerbot.Chatter.LfgChannel", true, true);
    bhv_world_bot_say_reply_  = c.GetBoolDefault("Playerbot.Behavior.WorldBotSayReply", true, true);
    bhv_instance_group_cap_   = c.GetBoolDefault("Playerbot.Behavior.InstanceGroupCap", true, true);
    bhv_open_world_group_cap_ = std::clamp<int32>(c.GetIntDefault("Playerbot.Behavior.OpenWorldGroupCap", 40, true), 1, 40);
    lfg_enable_               = c.GetBoolDefault("Playerbot.Lfg.Enable", true, true);
    lfg_instant_fill_         = c.GetBoolDefault("Playerbot.Lfg.InstantFillIfNobody", false, true);
    lfg_trickle_              = c.GetBoolDefault("Playerbot.Lfg.Trickle", true, true);
    lfg_level_range_          = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.Lfg.LevelRange", 5, true), 0, 80));
    lfg_bot_fill_seconds_     = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.Lfg.BotFillSeconds", 180, true), 0, 3600));
    lfg_categories_.clear();
    std::string const lfgCategories = c.GetStringDefault("Playerbot.Lfg.Categories", "", true);
    for (std::string_view token : Trinity::Tokenize(lfgCategories, ',', false))
        if (Optional<uint32> id = Trinity::StringTo<uint32>(token))
            lfg_categories_.push_back(*id);
    lfg_activity_maps_.clear();
    std::string const lfgActivityMaps = c.GetStringDefault("Playerbot.Lfg.ActivityMaps",
        "203776:43,204032:289,204288:389,204544:36,204800:33,205056:48,205312:34,205568:90,205824:47,206080:189,206336:129,206592:70,206848:209,207104:349,207360:109,207616:230,207872:229,208128:429,208384:429,208640:429,208896:329,211712:189,211968:189,212224:189,214272:229:10,214016:309,214528:249,214784:409,215040:469,215296:533,215552:509,215808:531", true);
    for (std::string_view token : Trinity::Tokenize(lfgActivityMaps, ',', false))
    {
        // "activity:map" or "activity:map:size"
        std::vector<std::string_view> const parts = Trinity::Tokenize(token, ':', true);
        if (parts.size() != 2 && parts.size() != 3)
            continue;
        Optional<uint32> activity = Trinity::StringTo<uint32>(parts[0]);
        Optional<uint32> map = Trinity::StringTo<uint32>(parts[1]);
        Optional<uint32> size = parts.size() == 3 ? Trinity::StringTo<uint32>(parts[2]) : Optional<uint32>(0u);
        if (activity && map && size)
            lfg_activity_maps_.push_back(LfgActivityMap{ *activity, *map, *size });
    }
    lfg_bot_groups_max_       = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.Lfg.BotGroups.Max", 25, true), 0, 40));
    lfg_bot_groups_max_bots_  = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.Lfg.BotGroups.MaxBots", 150, true), 0, 400));
    lfg_bot_groups_category_  = uint32(std::max<int32>(c.GetIntDefault("Playerbot.Lfg.BotGroups.Category", 2, true), 0));
    lfg_bot_groups_min_seconds_ = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.Lfg.BotGroups.MinMinutes", 5, true), 1, 120)) * 60;
    lfg_bot_groups_max_seconds_ = std::max(lfg_bot_groups_min_seconds_,
        uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.Lfg.BotGroups.MaxMinutes", 20, true), 1, 240)) * 60);
    lfg_bot_groups_interval_seconds_ = uint32(std::clamp<int32>(c.GetIntDefault("Playerbot.Lfg.BotGroups.IntervalSeconds", 30, true), 10, 3600));
    // one entry per vanilla dungeon of the Group Finder: activity id x 256 : map : level min : level max
    static char const* const kDefaultListedActivities =
        "203776:43:15:24,204032:289:58:60,204288:389:13:18,204544:36:17:26,204800:33:20:30,205056:48:24:32,"
        "205312:34:24:32,205568:90:29:38,205824:47:29:38,206080:189:30:38,206336:129:37:46,206592:70:41:51,"
        "206848:209:44:54,207104:349:46:55,207360:109:50:60,207616:230:52:60,207872:229:55:60,208128:429:54:60,"
        "208384:429:56:60,208640:429:56:60,208896:329:58:60,211712:189:36:44,211968:189:38:46,212224:189:33:41,"
        "214272:229:59:60:10";
    lfg_bot_groups_activities_ = V2::ParseListedActivities(c.GetStringDefault("Playerbot.Lfg.BotGroups.Activities", kDefaultListedActivities, true));
    lfg_finder_enable_        = c.GetBoolDefault("Playerbot.Lfg.Finder.Enable", true, true);
    // empty = the doors named in DungeonFinderPlan.h
    lfg_finder_entrances_     = V2::ParseFinderEntrances(c.GetStringDefault("Playerbot.Lfg.Finder.Entrances", "", true));
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
    bhv_fly_with_owner_       = c.GetBoolDefault("Playerbot.Behavior.FlyWithOwner", true, true);
    bhv_ride_with_owner_      = c.GetBoolDefault("Playerbot.Behavior.RideWithOwner", true, true);
    bhv_catch_up_to_owner_    = c.GetBoolDefault("Playerbot.Behavior.CatchUpToOwner", true, true);
    bhv_follow_owner_steps_   = c.GetBoolDefault("Playerbot.Behavior.FollowOwnerSteps", true, true);
    bhv_passive_leash_yd_     = std::max(0.0f, float(c.GetFloatDefault("Playerbot.Behavior.PassiveLeashYd", 25.0f, true)));
    bhv_aggressive_radius_yd_ = std::max(1.0f, float(c.GetFloatDefault("Playerbot.Behavior.AggressiveRadiusYd", 10.0f, true)));
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
    bhv_invite_passing_players_ = c.GetBoolDefault("Playerbot.Behavior.InvitePassingPlayers", false, true);
    world_questing_           = c.GetBoolDefault("Playerbot.World.Questing", false, true);
    kindness_enable_          = c.GetBoolDefault("Playerbot.World.Kindness.Enable", true, true);
    kindness_buff_cd_ms_      = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.Kindness.BuffCooldownSec", 900, true), 60, 86400)) * 1000u;
    kindness_help_hp_pct_     = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.Kindness.HelpHpPct", 60, true), 0, 90));
    kindness_help_cd_ms_      = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.Kindness.HelpCooldownSec", 180, true), 0, 3600)) * 1000u;
    kindness_heal_hp_pct_     = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.Kindness.HealHpPct", 60, true), 0, 95));
    kindness_heal_cd_ms_      = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.Kindness.HealCooldownSec", 45, true), 10, 3600)) * 1000u;
    share_mobs_enable_        = c.GetBoolDefault("Playerbot.World.ShareMobs.Enable", true, true);
    share_mobs_camp_radius_   = std::clamp(float(c.GetFloatDefault("Playerbot.World.ShareMobs.CampRadius", 35.0f, true)), 10.0f, 100.0f);
    share_mobs_quest_radius_  = std::clamp(float(c.GetFloatDefault("Playerbot.World.ShareMobs.QuestRadius", 120.0f, true)), 0.0f, 400.0f);
    parties_enable_           = c.GetBoolDefault("Playerbot.World.Parties.Enable", true, true);
    parties_pct_              = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.Parties.Pct", 30, true), 0, 100));
    parties_max_size_         = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.Parties.MaxSize", 3, true), 2, 3));
    parties_life_ms_          = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.Parties.LifeMin", 15, true), 1, 240)) * 60000u;
    gate_duels_enable_        = c.GetBoolDefault("Playerbot.World.GateDuels.Enable", true, true);
    gate_duels_pairs_         = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.GateDuels.PairsPerGate", 3, true), 1, 3));
    gate_duels_small_pairs_   = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.GateDuels.SmallCityPairs", 1, true), 0, 3));
    gate_duels_radius_        = std::clamp(float(c.GetFloatDefault("Playerbot.World.GateDuels.Radius", 80.0f, true)), 30.0f, 200.0f);
    gate_duels_challenge_     = c.GetBoolDefault("Playerbot.World.GateDuels.ChallengePlayers", true, true);
    gate_duels_challenge_cd_ms_ = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.GateDuels.ChallengeCooldownSec", 600, true), 30, 86400)) * 1000u;
    {
        GatheringConfig& g = gathering_;
        g.enable           = c.GetBoolDefault("Playerbot.World.Gathering.Enable", true, true);
        g.bots_per_gate    = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.Gathering.BotsPerGate", 8, true), 3, 12));
        g.radius           = std::clamp(float(c.GetFloatDefault("Playerbot.World.Gathering.Radius", 90.0f, true)), 30.0f, 200.0f);
        g.all_cities       = c.GetBoolDefault("Playerbot.World.Gathering.AllCities", false, true);
        g.stay_ms          = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.Gathering.StayMin", 10, true), 2, 120)) * 60000u;
        g.talk_gap_ms      = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.Gathering.TalkGapSec", 40, true), 10, 1800)) * 1000u;
        g.campfire         = c.GetBoolDefault("Playerbot.World.Gathering.Campfire", true, true);
        g.use_chat_model   = c.GetBoolDefault("Playerbot.World.Gathering.UseChatModel", true, true);
        g.model_per_minute = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.Gathering.ChatModelPerMinute", 4, true), 1, 60));
        g.city_chats       = uint32(std::clamp<int64_t>(c.GetIntDefault("Playerbot.World.Gathering.CityChats", 3, true), 0, 6));
    }
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
        q.pvp_reaction_delay_ms = pct("Playerbot.Pacing.PvpReactionDelayMs", 630, 0, 3000);
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
