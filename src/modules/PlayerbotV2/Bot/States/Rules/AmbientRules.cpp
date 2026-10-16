// AmbientRules - Refactor #3 pass 11/12. Migrates top-level ambient/PvP rules
// out of the State_Idle tail:
//   - idle:assist_friend_pvp  : grouped bot's friend is being attacked by a
//                                hostile player -> start_attack on the attacker.
//   - idle:pvp_help_callout   : bot in low-HP open-world PvP attack -> /say
//                                "help! pvp <zone>" with a 5-min lockout.
//   - idle:yell_lfg           : solo level-60+ bot in a rested area (city/inn)
//                                /yells a role-flavored LFG line every ~45 min.
//   - idle:ambient_emote      : Roleplay-personality bot in a rested area emits
//                                a flavor /wave / /cheer / /salute targeted at
//                                a random groupmate (or untargeted solo).
//                                Added in pass 12 after IdleRule signature was
//                                extended to thread GroupSnapshotView.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "DB2Stores.h"
#include "World.h"
#include "Util/ConfigReader.h"
#include "Services.h"
#include "Bot/DeathHotspots.h"
#include "Bot/States/MaintainHelpers.h"   // States::ChunkedWalkToward
#include "Travel/QuestHubDatabase.h"
#include "Travel/TownNpcIndex.h"
#include "Bot/TownLife.h"
#include "Bot/BotRegistry.h"
#include "Fleet/OwnerRegistry.h"
#include "Util.h"
#include "Config.h"
#include "Bot/World/ZonePicker.h"
#include "GameTime.h"
#include "UnitDefines.h"   // UNIT_NPC_FLAG_FLIGHTMASTER / _QUESTGIVER

#include <string>

namespace Playerbot {

namespace {

// ---------- idle:assist_friend_pvp ----------
bool AssistFriendPvpGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    if (!s.is_alive()) return false;
    if (!s.in_group()) return false;
    if (s.in_combat()) return false;
    if (s.in_battleground() || s.is_in_dungeon()) return false;
    if (s.is_casting()) return false;
    return true;
}

bool AssistFriendPvpFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    float self_xa = 0.f, self_ya = 0.f, self_za = 0.f;
    s.position(self_xa, self_ya, self_za);
    ObjectGuid best_attacker;
    float      best_d2 = 1e9f;
    for (auto const& e : s.nearby_enemies())
    {
        if (!e.is_player) continue;
        if (e.hp <= 0) continue;
        if (e.victim.IsEmpty()) continue;
        if (e.victim == s.raw().guid) continue;
        bool victim_is_friend = false;
        for (auto const& f : s.nearby_friends())
        {
            if (!f.is_player) continue;
            if (f.guid != e.victim) continue;
            if (f.hp <= 0) continue;
            victim_is_friend = true;
            break;
        }
        if (!victim_is_friend) continue;
        const float dx = e.x - self_xa;
        const float dy = e.y - self_ya;
        const float dz = e.z - self_za;
        const float d2 = dx*dx + dy*dy + dz*dz;
        if (d2 < best_d2)
        {
            best_d2 = d2;
            best_attacker = e.guid;
        }
    }
    if (best_attacker.IsEmpty() || best_d2 >= 40.0f * 40.0f) return false;
    if (!emit.start_attack(best_attacker)) return false;
    ai.set_last_rule_fired("idle:assist_friend_pvp");
    return true;
}

// ---------- idle:assist_group_pve ----------
// PvE mirror of assist_friend_pvp: a group member is in combat with
// a creature but the bot is OOC. Real players see "my groupmate is
// being attacked" and open up. Without this, a hunter who pulls a
// mob solo while two other hunters stand 20y away gets no support.
//
// Open-world only for v1. BG scripts and dungeon scripts own their
// own engagement choreography (skull mark, tank pull, etc.) — a
// generic assist would risk pulling extra mobs the script doesn't
// want pulled. Re-evaluate dungeon scope only with evidence that
// the existing tank/heal/engage chain misses cases.
bool AssistGroupPveGate(BotSnapshotView const& s, BotAI& ai,
                        GroupSnapshotView const& g, uint32)
{
    if (!s.is_alive()) return false;
    // Told to stay: keep the spot (it still fights back when attacked).
    if (ai.is_manual_mode(s.published_at_ms()) && ai.owner_command() == BotAI::OwnerCommand::Stay) return false;
    if (!g.exists()) return false;
    if (s.in_combat()) return false;
    if (s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.in_battleground() || s.is_in_dungeon()) return false;
    return true;
}

bool AssistGroupPveFire(BotSnapshotView const& s, BotAI& ai,
                        GroupSnapshotView const& g,
                        BotIntentEmitter& emit, uint32)
{
    auto const* members = g.members();
    if (!members || members->empty()) return false;
    float bx, by, bz; s.position(bx, by, bz);
    ObjectGuid best_attacker;
    float      best_d2 = 40.0f * 40.0f;     // engage range cap
    for (auto const& e : s.nearby_enemies())
    {
        if (e.is_player)        continue;   // PvE flavour only
        if (e.hp <= 0)          continue;
        if (e.victim.IsEmpty()) continue;
        if (e.victim == s.raw().guid) continue;  // already on us → in_combat
        // e.victim must be a live group member on our map (excluding self).
        bool victim_is_groupmate = false;
        for (auto const& m : *members)
        {
            if (m.guid == s.raw().guid) continue;
            if (m.guid != e.victim)     continue;
            if (m.hp <= 0)              continue;
            if (m.map_id != s.map_id()) continue;
            victim_is_groupmate = true;
            break;
        }
        if (!victim_is_groupmate) continue;
        const float dx = e.x - bx, dy = e.y - by, dz = e.z - bz;
        const float d2 = dx*dx + dy*dy + dz*dz;
        if (d2 < best_d2) { best_d2 = d2; best_attacker = e.guid; }
    }
    if (best_attacker.IsEmpty()) return false;
    if (!emit.start_attack(best_attacker)) return false;
    ai.set_last_rule_fired("idle:assist_group_pve");
    return true;
}

// ---------- idle:pvp_help_callout ----------
bool PvpHelpCalloutGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,uint32)
{
    if (!s.is_alive()) return false;
    if (!s.in_group()) return false;
    if (s.in_battleground()) return false;
    if (!s.under_player_attack()) return false;
    if (s.hp_pct() >= 40) return false;
    const uint32 pw_now_ms = s.published_at_ms();
    const uint64 pw_key = (uint64(7) << 32) | uint64(0xDEADu);
    return !ai.action_recently_tried(BotAI::ActionKind::BgCallout, pw_key, pw_now_ms);
}

bool PvpHelpCalloutFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    uint32 const area_id = s.area_id();
    std::string area_name;
    if (auto const* ae = sAreaTableStore.LookupEntry(area_id))
        area_name = ae->AreaName[sWorld->GetDefaultDbcLocale()];
    if (area_name.empty())
        area_name = std::to_string(area_id);
    const std::string msg = "help! pvp " + area_name;
    if (!emit.say(msg)) return false;
    const uint32 pw_now_ms = s.published_at_ms();
    const uint64 pw_key = (uint64(7) << 32) | uint64(0xDEADu);
    ai.note_action_retry(BotAI::ActionKind::BgCallout, pw_key, pw_now_ms);
    ai.set_last_rule_fired("idle:pvp_help_callout");
    return true;
}

// Playerbot.World.TownLife (default 1): the shows below and the level-gated LFG yells. Read once at start.
bool TownLifeEnabled()
{
    static bool const on = sConfigMgr->GetBoolDefault("Playerbot.World.TownLife", true);
    return on;
}

// ---------- idle:yell_lfg ----------
// A solo bot in a rested area (town / inn) now and then calls for a group. The level-60 bots call for any group; a bot of a dungeon's
// level names that dungeon, but only a few take part at a time. Nothing here forms a group: a bot joins one only when a real player
// invites it (the invite rules), so a dungeon is never run by bots alone and costs no RAM until a player asks for it.
bool YellLfgGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,uint32)
{
    if (s.in_group()) return false;
    if (s.level() < 13) return false;
    if (s.has_owner_character()) return false;
    // A rested place: the rested pool fills in inns and cities, but a level-60 bot has none, so a capital counts as well.
    if (s.raw().identity.rest_bonus_xp <= 0 && !::Playerbot::V2::World::IsVanillaCapitalZone(s.zone_id())) return false;
    if (ai.personality().verbosity == Verbosity::Silent) return false;
    if (s.is_moving() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    constexpr uint32 kYellThrottleMs = 45u * 60u * 1000u;
    const uint32 yell_now_ms = s.published_at_ms();
    // Below level 60 only a share of the bots call at a time (a different set every 45 minutes).
    if (s.level() < 60 && (!TownLifeEnabled() || !V2::TownLife::TakesPart(s.bot_id(), 3u, yell_now_ms, 15u, kYellThrottleMs))) return false;

    if (ai.last_yell_lfg_ms() == 0)
    {
        const uint32 stagger = (s.bot_id() * 9173u) % kYellThrottleMs;
        const uint32 base = yell_now_ms > kYellThrottleMs
                            ? yell_now_ms - kYellThrottleMs
                            : 0u;
        ai.set_last_yell_lfg_ms(base + stagger);
    }
    const uint32 last_yell = ai.last_yell_lfg_ms();
    return yell_now_ms != 0 && yell_now_ms >= last_yell &&
           (yell_now_ms - last_yell) >= kYellThrottleMs;
}

bool YellLfgFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    const uint32 yell_now_ms = s.published_at_ms();
    constexpr char const* kTankYells[] = {
        "Tank LFG, msg me!",
        "Looking for a group, can tank.",
        "Tank here, any dungeon, whisper me.",
    };
    constexpr char const* kHealerYells[] = {
        "Healer LFG.",
        "Looking for a group to heal, whisper me.",
        "Healer available, msg me for runs.",
    };
    constexpr char const* kDpsYells[] = {
        "DPS LFG.",
        "Looking for any group.",
        "Hi! Anyone running a dungeon tonight?",
        "lfg, can do most stuff.",
    };
    constexpr uint32 kTankSize = sizeof(kTankYells) / sizeof(kTankYells[0]);
    constexpr uint32 kHealerSize = sizeof(kHealerYells) / sizeof(kHealerYells[0]);
    constexpr uint32 kDpsSize = sizeof(kDpsYells) / sizeof(kDpsYells[0]);
    const Role role = ai.effective_role(s);
    char const* phrase = nullptr;
    const uint32 sel_seed = (s.bot_id() ^ (yell_now_ms / (45u * 60u * 1000u)));
    if (role == Role::Tank)
        phrase = kTankYells[sel_seed % kTankSize];
    else if (role == Role::Healer)
        phrase = kHealerYells[sel_seed % kHealerSize];
    else
        phrase = kDpsYells[sel_seed % kDpsSize];
    // A bot of a dungeon's level names the dungeon.
    std::string line = phrase;
    if (s.level() < 60)
        if (char const* dungeon = V2::TownLife::LfgDungeonFor(s.level(), sel_seed))
        {
            char const* rolename = role == Role::Tank ? "tank" : role == Role::Healer ? "healer" : "dps";
            line = std::string("LFG ") + dungeon + ", " + rolename + " here. Whisper me!";
        }
    V2::TownLife::Counters().lfg_calls.fetch_add(1, std::memory_order_relaxed);
    emit.yell_world(line);
    ai.set_last_yell_lfg_ms(yell_now_ms);
    ai.set_last_rule_fired("idle:yell_lfg");
    return true;
}

// ---------- idle:ambient_sit ----------
// Side-effect "sit when resting" — SetStandStateIntent is idempotent
// server-side so re-emitting on an already-sitting bot is a no-op. Fire
// returns FALSE so subsequent registry rules still run (the legacy code
// did this unconditionally as a side effect before any return).
//
// Requires sustained idle: 10 seconds of standing still without combat
// or casting. Without this gate the bot sat on EVERY tiny pause during
// movement — between path segments, after each enemy kill before
// re-engaging, while a snapshot tick computed the next destination,
// etc. Looked frankly broken from the observer's perspective.
// Verified 2026-05-20: user reported "all bots sit on every tiny stop
// during movement". 10s threshold matches the standard MMO "out of
// combat → rest" feel without flooding sit animations.
bool AmbientSitGate(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const&, uint32)
{
    // Common safety gates first — these hold regardless of WHERE the bot
    // is allowed to sit.
    if (s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    // Don't sit while anything hostile is in pull range. Even in a
    // hub guards / event mobs / world-PvP attackers can be 40y away.
    if (s.enemies_within(40.0f) > 0) return false;

    const uint32 now_ms = s.published_at_ms();
    if (s.is_moving())
    {
        ai.set_last_moving_ms(now_ms);
        return false;
    }
    // is_moving == false. last_moving_ms == 0 means "never moved since
    // boot" — those bots just spawned, give them a beat before sitting.
    const uint32 last_move = ai.last_moving_ms();
    if (last_move == 0)
    {
        ai.set_last_moving_ms(now_ms);
        return false;
    }
    const uint32 idle_ms = now_ms - last_move;

    // I-P3g: broadened sit locations. The original gate was hub-only
    // (operator-annotated city/village footprint, kind 3/4) — but real
    // players also rest while parked at a flight master, waiting on a
    // quest giver, or just standing still in a quiet safe spot after a
    // while. Three tiers, each with conservative thresholds:
    //
    //   1. City / village footprint  — 30s standstill (settled in a hub).
    //   2. Parked at a flight master or quest giver anywhere — 30s. These
    //      are natural "I'm waiting here" spots (queuing a taxi, reading
    //      quest text), so the same hub threshold reads naturally.
    //   3. Anywhere else that is safe — 90s standstill. Much longer so a
    //      bot pausing mid-route to recompute a path / scan objectives
    //      doesn't flop down by the roadside; only a genuinely settled
    //      bot reaches 90s of continuous standstill.
    constexpr uint32 kHubIdleMs    = 30u * 1000u;
    constexpr uint32 kAnywhereIdleMs = 90u * 1000u;

    if (s.in_city() || s.in_village())
        return idle_ms >= kHubIdleMs;

    if (s.nearest_npc_with_flag(uint32(UNIT_NPC_FLAG_FLIGHTMASTER) |
                                uint32(UNIT_NPC_FLAG_QUESTGIVER)) != nullptr)
        return idle_ms >= kHubIdleMs;

    return idle_ms >= kAnywhereIdleMs;
}

bool AmbientSitFire(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const&,
                    BotIntentEmitter& emit, uint32)
{
    // Edge-paced: one SIT per 60s settled window. The original per-tick
    // re-emit ("idempotent server-side") still executed ~6 intents/sec per
    // settled bot — the TOP intent in /diag fleet-wide (Uraimus, Dolanaar
    // inn, 2026-06-11) — and Unit::SetStandState re-sends the stand-state
    // self-packet on every call.
    const uint32 sit_now_ms = s.published_at_ms();
    if (!ai.action_recently_tried(BotAI::ActionKind::AmbientSit, 0, sit_now_ms))
    {
        emit.emit(SetStandStateIntent{1});
        ai.note_action_retry(BotAI::ActionKind::AmbientSit, 0, sit_now_ms);
    }
    return false;   // intentional: side effect only, don't claim the tick
}

// ---------- idle:look_at_npc ----------
// Real players don't stand staring at their pet's tail — they
// idle-rotate their camera and "look at" nearby quest-givers /
// vendors / mailboxes. Without this, bots are eerily still in a hub,
// always facing whatever they last targeted. The rule picks a
// random nearby_friend with NPC flags (vendor/quest/banker/etc.)
// every ~30–60s (jittered per-bot) and emits face_target — pure
// cosmetic, no behaviour impact. Skipped while moving/in combat/
// casting/mounted.
//
// I-P3d: cadence is tracked via ai.last_look_around_ms() (added to
// BotAI mirroring last_ambient_emote_ms_). A per-bot jitter spreads
// the fire window across 30–60s so a hub full of bots doesn't all
// snap their heads at the same NPC on the same tick. Personality
// verbosity gate same as emote.
bool LookAroundGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (ai.personality().verbosity < Verbosity::Normal) return false;
    if (s.is_moving() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (!s.is_alive()) return false;
    // Need at least one NPC-flagged friendly in range.
    bool any = false;
    for (auto const& f : s.nearby_friends())
        if (f.npc_flags != 0) { any = true; break; }
    if (!any) return false;

    // I-P3d: jittered 30–60s cadence. Base 30s + a stable per-bot 0–30s
    // offset (deterministic from bot_id so the same bot keeps a steady
    // personal rhythm rather than re-rolling every tick).
    const uint32 now_ms = s.published_at_ms();
    const uint32 jitter = (s.bot_id() * 2654435761u) % 30000u;     // 0–30s
    const uint32 interval_ms = 30000u + jitter;                    // 30–60s
    const uint32 last_ms = ai.last_look_around_ms();
    return now_ms == 0 || last_ms == 0 || (now_ms - last_ms) >= interval_ms;
}

bool LookAroundFire(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const&,
                    BotIntentEmitter& emit, uint32)
{
    const uint32 now_ms = s.published_at_ms();
    // I-P3c: pick ONE candidate via an up-front index, then select it
    // in a second pass. The old `% candidate_count` reservoir was buggy:
    // it picks index i only when (seed % (i+1)) == 0, which for any seed
    // is overwhelmingly biased toward the first candidate (i=0 always
    // matches since seed % 1 == 0) — so the bot almost always faced the
    // nearest NPC. Count first, derive a single random index, select it.
    uint32 candidate_count = 0;
    for (auto const& f : s.nearby_friends())
        if (f.npc_flags != 0) ++candidate_count;
    if (candidate_count == 0) return false;
    const uint32 idx = (now_ms ^ (s.bot_id() * 2654435761u)) % candidate_count;
    uint32 seen = 0;
    ObjectGuid pick;
    for (auto const& f : s.nearby_friends())
    {
        if (f.npc_flags == 0) continue;
        if (seen == idx) { pick = f.guid; break; }
        ++seen;
    }
    if (pick.IsEmpty()) return false;
    if (!emit.face_target(pick)) return false;
    ai.set_last_look_around_ms(now_ms);
    ai.set_last_rule_fired("idle:look_at_npc");
    return true;
}

// ---------- idle:ambient_emote ----------
bool AmbientEmoteGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    // Pre-fix: Roleplay-only. Result: Normal/Chatty bots in capital
    // cities sat in dead silence indefinitely, looking like a mannequin
    // display rather than a living city. Now: Normal+ (so Silent/Terse
    // still stay quiet). Cadence varies by personality so Chatty bots
    // emote more often than Normal.
    if (ai.personality().verbosity < Verbosity::Normal) return false;
    if (s.raw().identity.rest_bonus_xp <= 0) return false;
    if (s.is_moving() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.raw().social_events.has_duel_request) return false;
    const uint32 kThrottleMs =
        ai.personality().verbosity == Verbosity::Roleplay ? 2u * 60u * 1000u :
        ai.personality().verbosity == Verbosity::Chatty   ? 3u * 60u * 1000u :
                                                            5u * 60u * 1000u;
    const uint32 now_ms = s.published_at_ms();
    const uint32 last_ms = ai.last_ambient_emote_ms();
    return now_ms == 0 || (now_ms - last_ms) >= kThrottleMs;
}

bool AmbientEmoteFire(BotSnapshotView const& s, BotAI& ai,
                      GroupSnapshotView const& g,
                      BotIntentEmitter& emit, uint32)
{
    const uint32 now_ms = s.published_at_ms();
    // Pool expanded 2026-05-21: 3=wave, 4=bow, 34=cheer, 39=flex, 66=hug,
    // 113=love, 25=point ("window shopping"), 16=talk (chat to NPCs/
    // bystanders), 21=shy. Mixed-personality pool — humans browse
    // vendors, point at things, chat at NPCs, not just wave/hug.
    constexpr uint32 kEmotePool[] = {
        3, 4, 66, 34, 39, 113, 25, 16, 21,
    };
    constexpr uint32 kPoolSize = sizeof(kEmotePool) / sizeof(uint32);
    const uint32 sel = ((now_ms / 1000u) + s.bot_id()) % kPoolSize;
    ObjectGuid target;
    if (g.exists())
    {
        if (auto const* members = g.members())
        {
            // I-P3c: same reservoir bug as look_at_npc — `% (seen+1)`
            // biases hard toward the first eligible member. Count the
            // eligible (online, same-map, non-self) members, derive one
            // random index up-front, then select that index in a second
            // pass so the emote target is uniformly random.
            uint32 eligible = 0;
            for (auto const& m : *members)
            {
                if (m.guid == s.raw().guid) continue;
                if (!m.online || m.map_id != s.map_id()) continue;
                ++eligible;
            }
            if (eligible > 0)
            {
                const uint32 idx =
                    (now_ms ^ (s.bot_id() * 2654435761u)) % eligible;
                uint32 seen = 0;
                for (auto const& m : *members)
                {
                    if (m.guid == s.raw().guid) continue;
                    if (!m.online || m.map_id != s.map_id()) continue;
                    if (seen == idx) { target = m.guid; break; }
                    ++seen;
                }
            }
        }
    }
    emit.emit(PerformEmoteIntent{kEmotePool[sel], target});
    ai.set_last_ambient_emote_ms(now_ms);
    ai.set_last_rule_fired("idle:ambient_emote");
    return true;
}

// ---------- idle:ambient_state_emote ----------
// I-P3e: the existing ambient_emote pool is all ONESHOT emotes (wave,
// bow, cheer...) — a quick animation that immediately resets. Real
// players who park in a hub for a while strike SUSTAINED state emotes:
// they dance on the mailbox, sit at the inn, lie down ("sleep") in a
// quiet corner. Without a sustained emote the city looks like everyone
// is perpetually fidgeting but never settling.
//
// This rule emits a single low-frequency state emote when the bot has
// been settled in a hub for a good while. State emotes persist
// server-side until the unit moves (WoW clears EMOTE_STATE on any
// movement), and the gate already bails on movement/combat, so we never
// re-emit while moving — the animation clears itself the instant the
// bot walks. Conservative cadence (~8 min) so a tavern isn't a rave.
bool AmbientStateEmoteGate(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&, uint32)
{
    if (ai.personality().verbosity < Verbosity::Normal) return false;
    if (!s.is_alive()) return false;
    if (s.is_moving() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.raw().social_events.has_duel_request) return false;
    // Hub-only — sustained emotes look out of place mid-world.
    if (!s.in_city() && !s.in_village()) return false;
    // Don't strike a pose with anything hostile around.
    if (s.enemies_within(40.0f) > 0) return false;
    // Require a sustained standstill (reuses the ambient_sit move tracker).
    const uint32 now_ms = s.published_at_ms();
    const uint32 last_move = ai.last_moving_ms();
    if (last_move == 0 || (now_ms - last_move) < 30u * 1000u) return false;

    constexpr uint32 kThrottleMs = 8u * 60u * 1000u;   // ~8 min
    const uint32 last_ms = ai.last_ambient_state_emote_ms();
    return now_ms == 0 || last_ms == 0 || (now_ms - last_ms) >= kThrottleMs;
}

bool AmbientStateEmoteFire(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&,
                           BotIntentEmitter& emit, uint32)
{
    const uint32 now_ms = s.published_at_ms();
    // Sustained "state" emote ids (persist until movement), verified
    // against SharedDefines.h Emote enum:
    //   10 = EMOTE_STATE_DANCE, 12 = EMOTE_STATE_SLEEP, 13 = EMOTE_STATE_SIT.
    // DANCE reads most "alive"; weight the pool toward it.
    constexpr uint32 kStatePool[] = { 10 /*dance*/, 10 /*dance*/, 12 /*sleep*/ };
    constexpr uint32 kPoolSize = sizeof(kStatePool) / sizeof(uint32);
    const uint32 sel = (now_ms ^ (s.bot_id() * 2654435761u)) % kPoolSize;
    emit.emit(PerformEmoteIntent{kStatePool[sel], ObjectGuid::Empty});
    ai.set_last_ambient_state_emote_ms(now_ms);
    ai.set_last_rule_fired("idle:ambient_state_emote");
    return true;
}

// ---------- idle:fidget ----------
// Playerbot.Behavior.Fidget: a bot standing still out of combat does the small things a
// person does while waiting: a hop, a short emote, now and then a dance or a sit. The rule
// is evaluated every 20 s (min_interval_ms) and fires on a per-bot, per-window hash with
// probability 1/3, so each bot's next fidget lands 20-90 s after the last, never in step
// with its neighbours. Unlike ambient_emote this is not limited to rested areas or to
// chatty personalities. Stateless: no BotAI field. Never in combat, never while anything
// hostile is near or the group is fighting, never mounted / in a BG / dungeon / raid.
bool FidgetGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32)
{
    if (!Services::Config().behavior_fidget()) return false;
    if (!s.is_alive() || s.in_combat() || s.is_casting() || s.is_moving()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.in_battleground() || s.is_in_dungeon() || s.is_in_raid()) return false;
    if (s.raw().social_events.has_duel_request) return false;
    if (s.enemies_within(40.0f) > 0) return false;
    if (auto const* members = g.members())
        for (auto const& m : *members)
            if (m.guid != s.raw().guid && m.online && m.in_combat && m.map_id == s.map_id())
                return false;
    // Settled for at least 10 s (the ambient_sit gate maintains last_moving_ms).
    const uint32 now_ms = s.published_at_ms();
    const uint32 last_move = ai.last_moving_ms();
    if (last_move == 0 || now_ms < last_move || (now_ms - last_move) < 10u * 1000u) return false;
    const uint32 window = now_ms / 20000u;
    const uint32 h = (s.bot_id() * 2654435761u) ^ (window * 2246822519u);
    return ((h >> 7) % 3u) == 0u;
}

bool FidgetFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,
                BotIntentEmitter& emit, uint32)
{
    const uint32 now_ms = s.published_at_ms();
    const uint32 h = ((s.bot_id() + 1u) * 2654435761u) ^ ((now_ms / 20000u) * 3266489917u);
    const uint32 pick = (h >> 11) % 20u;
    if (pick < 5)
        emit.jump(2.0f);                                          // a little hop
    else if (pick < 8)
        emit.perform_emote(6 /*QUESTION*/, ObjectGuid::Empty);    // looks about, puzzled
    else if (pick < 11)
        emit.perform_emote(24 /*SHY*/, ObjectGuid::Empty);
    else if (pick < 14)
        emit.perform_emote(11 /*LAUGH*/, ObjectGuid::Empty);
    else if (pick < 16)
        emit.perform_emote(23 /*FLEX*/, ObjectGuid::Empty);
    else if (pick < 18)
        emit.emit(SetStandStateIntent{1});                        // sit down for a bit
    else
        emit.perform_emote(10 /*DANCE*/, ObjectGuid::Empty);
    ai.set_last_rule_fired("idle:fidget");
    return true;
}

// ---------- idle:react_to_passerby ----------
// I-P3h: bots never acknowledged real players walking past — they'd
// stand frozen next to a human who waved at them, which immediately
// breaks the illusion. This rule occasionally faces and /waves at a
// nearby NON-grouped player.
//
// Bot-on-bot spam guard: there is currently no is_bot flag on
// NearbyUnit, so we cannot positively exclude other bots. We therefore
// (a) only react to is_player units that are NOT in our group, and
// (b) rate-limit HARD — the rule's coarse 30s min_interval_ms (one
// greet attempt per bot per 30s at most) plus a per-target 10-min
// lockout (ActionKind::GreetPlayer). Even if two bots greet each other,
// the per-target lockout caps it to one wave per pair per 10 min, which
// reads as natural rather than spammy.
// I-P3h: needs a NearbyUnit.is_bot flag (owned by BotSnapshotBuilder)
// to fully suppress bot-on-bot greets — until then the hard rate-limit
// keeps it from looking robotic.
bool ReactToPasserbyGate(BotSnapshotView const& s, BotAI& ai,
                         GroupSnapshotView const&, uint32)
{
    if (ai.personality().verbosity < Verbosity::Normal) return false;
    if (!s.is_alive()) return false;
    if (s.is_moving() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.raw().social_events.has_duel_request) return false;
    // Cheap pre-check: at least one eligible (real, non-grouped, alive)
    // player nearby. The actual rate-limiting is the rule's coarse 30s
    // min_interval_ms plus the per-target 10-min GreetPlayer lockout in
    // the fire — so we don't need a separate per-bot timestamp here.
    for (auto const& u : s.nearby_friends())
    {
        if (!u.is_player) continue;
        if (u.is_in_group) continue;     // skip grouped players (likely in a party)
        if (u.hp <= 0) continue;
        return true;
    }
    return false;
}

bool ReactToPasserbyFire(BotSnapshotView const& s, BotAI& ai,
                         GroupSnapshotView const& g,
                         BotIntentEmitter& emit, uint32)
{
    const uint32 now_ms = s.published_at_ms();
    // Build a quick set of our own group member guids so we never greet
    // someone actually in our party (is_in_group is a coarse "in ANY
    // group" flag; this is the precise "in OUR group" exclusion).
    auto in_our_group = [&](ObjectGuid gid) -> bool
    {
        if (!g.exists()) return false;
        if (auto const* members = g.members())
            for (auto const& m : *members)
                if (m.guid == gid) return true;
        return false;
    };

    float bx, by, bz; s.position(bx, by, bz);
    ObjectGuid best;
    float best_d2 = 30.0f * 30.0f;       // only greet players within 30y
    for (auto const& u : s.nearby_friends())
    {
        if (!u.is_player) continue;
        if (u.is_in_group) continue;
        if (u.hp <= 0) continue;
        if (u.guid == s.raw().guid) continue;
        if (in_our_group(u.guid)) continue;
        // Per-target hard lockout (10 min, ActionKind::GreetPlayer).
        if (ai.action_recently_tried(BotAI::ActionKind::GreetPlayer,
                                     u.guid.GetCounter(), now_ms))
            continue;
        const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
        const float d2 = dx*dx + dy*dy + dz*dz;
        if (d2 < best_d2) { best_d2 = d2; best = u.guid; }
    }
    if (best.IsEmpty()) return false;
    // Face them, then wave (3 = EMOTE_ONESHOT_WAVE aimed at the target).
    emit.face_target(best);
    if (!emit.perform_emote(3, best)) return false;
    ai.note_action_retry(BotAI::ActionKind::GreetPlayer,
                         best.GetCounter(), now_ms);
    ai.set_last_rule_fired("idle:react_to_passerby");
    return true;
}

// ---------- idle:leave_death_hotspot ----------
// Wander and grind drift into places that have just killed several bots (DeathHotspots). A solo bot standing in such
// a cell, well below the killers' level, walks back to the nearest hub of its level instead of waiting for the next
// pull. Quest objectives inside a hotspot are dropped separately (idle:pursue_quest_goal).
bool LeaveHotspotGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32 now_ms)
{
    if (!s.is_alive() || s.in_combat() || s.is_casting() || s.raw().movement.is_mounted) return false;
    if (s.in_group() || s.is_in_dungeon() || s.in_battleground()) return false;
    if (ai.in_post_rescue_grace(now_ms) || ai.in_charter_grace(now_ms)) return false;
    float bx, by, bz; s.position(bx, by, bz);
    return ::Playerbot::DeathHotspots::Instance().Dangerous(s.map_id(), bx, by, uint8(s.level()),
                                                            uint64(GameTime::GetGameTime()));
}

bool LeaveHotspotFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    auto const* hub = Services::Hubs().GetNearestQuestHub(s.raw());
    if (!hub || hub->mapId != s.map_id()) return false;
    float const hx = hub->location.GetPositionX(), hy = hub->location.GetPositionY(), hz = hub->location.GetPositionZ();
    uint64 const t = uint64(GameTime::GetGameTime());
    if (::Playerbot::DeathHotspots::Instance().Dangerous(s.map_id(), hx, hy, uint8(s.level()), t))
        return false;   // the hub is a hotspot too: nowhere better to go
    if (States::ChunkedWalkToward(s, ai, emit, hx, hy, hz, "idle:leave_death_hotspot", now_ms))
        return true;
    return false;
}

// ---------- idle:leave_water ----------
// A solo bot that ends up swimming inside a capital (a canal, a harbour) walks to the nearest hub to get out; capitals
// are the only place this applies, a swim in the open world is a river crossing and is left to the router.
bool LeaveWaterGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32 now_ms)
{
    if (!s.is_alive() || s.in_combat() || s.is_casting() || !s.is_swimming()) return false;
    if (s.in_group() || s.is_in_dungeon() || s.in_battleground()) return false;
    if (ai.in_post_rescue_grace(now_ms) || ai.in_charter_grace(now_ms)) return false;
    uint8 lo = 0, hi = 0;
    return !::Playerbot::V2::World::VanillaZoneLevelRange(s.zone_id(), lo, hi);
}

bool LeaveWaterFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    auto const* hub = Services::Hubs().GetNearestQuestHub(s.raw());
    if (!hub || hub->mapId != s.map_id()) return false;
    return States::ChunkedWalkToward(s, ai, emit, hub->location.GetPositionX(), hub->location.GetPositionY(),
                                     hub->location.GetPositionZ(), "idle:leave_water", now_ms);
}

// ---------- idle:inn_rest ----------
// In a capital a share of the bots (most at night) walk to the nearest inn and sit there for a while. One check of the town NPC index
// per tick, only for the bots whose turn it is.
constexpr float kInnSearchYards = 150.0f;
constexpr float kInnStandYards = 5.0f;

bool InnRestGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (!TownLifeEnabled()) return false;
    if (!::Playerbot::V2::World::IsVanillaCapitalZone(s.zone_id())) return false;
    int const hour = TimeBreakdown(GameTime::GetGameTime()).tm_hour;
    if (!V2::TownLife::TakesPart(s.bot_id(), 1u, s.published_at_ms(), V2::TownLife::InnPercent(hour), 15u * 60u * 1000u)) return false;
    if (!s.is_alive() || s.in_combat() || s.is_casting() || s.has_owner_character() || s.in_group()) return false;
    if (s.is_in_instance() || s.is_in_dungeon() || s.in_battleground() || s.raw().movement.is_mounted) return false;
    return s.enemies_within(40.0f) == 0 && !s.has_actionable_quest();
}

bool InnRestFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    float bx, by, bz;
    s.position(bx, by, bz);
    auto const* inn = ::Playerbot::V2::Travel::TownNpcIndex::Instance().Nearest(
        s.map_id(), bx, by, bz, UNIT_NPC_FLAG_INNKEEPER, s.is_alliance(), kInnSearchYards);
    if (!inn) return false;
    // Stand a few yards from the innkeeper, on the side the bot comes from, so a crowd does not stack on one point.
    float const dx = bx - inn->x, dy = by - inn->y, len = std::hypot(dx, dy);
    float const ux = len > 0.5f ? dx / len : 1.0f, uy = len > 0.5f ? dy / len : 0.0f;
    float const stand = kInnStandYards + float(s.bot_id() % 5u);
    float const tx = inn->x + ux * stand, ty = inn->y + uy * stand;
    if (std::hypot(bx - tx, by - ty) > 4.0f)
        return States::ChunkedWalkToward(s, ai, emit, tx, ty, inn->z, "idle:inn_rest", now_ms);
    V2::TownLife::Counters().inn_ticks.fetch_add(1, std::memory_order_relaxed);
    if (s.is_moving()) return false;
    emit.sit();
    ai.set_last_rule_fired("idle:inn_rest");
    return true;
}

// ---------- idle:mailbox ----------
// A few bots in a town walk to the nearest mailbox and stand there a while, then the window turns over and they go back to their errands.
// The share is picked per time window by hash, so nothing is stored per bot.
constexpr float kMailboxSearchYards = 120.0f;
constexpr float kMailboxStandYards = 2.5f;
constexpr uint32 kMailboxWindowMs = 90u * 1000u;

bool MailboxGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (!TownLifeEnabled()) return false;
    if (!::Playerbot::V2::World::IsVanillaCapitalZone(s.zone_id())) return false;
    if (!V2::TownLife::TakesPart(s.bot_id(), 5u, s.published_at_ms(), 4u, kMailboxWindowMs)) return false;
    if (!s.is_alive() || s.in_combat() || s.is_casting() || s.has_owner_character() || s.in_group()) return false;
    if (s.is_in_instance() || s.is_in_dungeon() || s.in_battleground() || s.raw().movement.is_mounted) return false;
    return s.enemies_within(40.0f) == 0 && !s.has_actionable_quest();
}

bool MailboxFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    float bx, by, bz;
    s.position(bx, by, bz);
    auto const* box = ::Playerbot::V2::Travel::TownNpcIndex::Instance().NearestMailbox(s.map_id(), bx, by, bz, kMailboxSearchYards);
    if (!box) return false;
    V2::TownLife::Counters().mailbox_ticks.fetch_add(1, std::memory_order_relaxed);
    float const dx = bx - box->x, dy = by - box->y, len = std::hypot(dx, dy);
    if (len > kMailboxStandYards + 1.5f)
    {
        float const ux = dx / len, uy = dy / len;
        return States::ChunkedWalkToward(s, ai, emit, box->x + ux * kMailboxStandYards, box->y + uy * kMailboxStandYards, box->z, "idle:mailbox", now_ms);
    }
    if (s.is_moving()) return false;
    ai.set_last_rule_fired("idle:mailbox");
    return true;
}

// ---------- idle:town_duel ----------
// A bot in a town challenges a nearby world bot of about its level (spell 7266, Duel); the other accepts (BotAI duel pact), the duel runs
// the usual way. A share of the bots take part, a pair at a time, at most once every 15 minutes per bot.
bool TownDuelGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (!TownLifeEnabled()) return false;
    if (!V2::TownLife::TakesPart(s.bot_id(), 2u, s.published_at_ms(), 6u, 20u * 60u * 1000u)) return false;
    if (!s.is_alive() || s.in_combat() || s.is_casting() || s.has_owner_character() || s.in_group()) return false;
    if (s.is_in_instance() || s.is_in_dungeon() || s.in_battleground() || s.raw().movement.is_mounted || s.is_moving()) return false;
    if (s.hp_pct() < 90 || s.level() < 6 || s.enemies_within(40.0f) > 0 || s.has_actionable_quest()) return false;
    if (s.raw().social_events.has_duel_request || s.duel_in_progress()) return false;
    uint32 const now = s.published_at_ms();
    if (ai.last_town_show_ms() && now - ai.last_town_show_ms() < 15u * 60u * 1000u) return false;
    if (!V2::TownLife::TakesPart(s.bot_id(), 2u, now, 6u, 20u * 60u * 1000u)) return false;
    float bx, by, bz;
    s.position(bx, by, bz);
    return ::Playerbot::V2::World::IsVanillaCapitalZone(s.zone_id())
        || ::Playerbot::V2::Travel::TownNpcIndex::Instance().CountNear(s.map_id(), bx, by, 60.0f, s.is_alliance(), 0xFFFFFFFFu) >= 3;
}

bool TownDuelFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    float bx, by, bz;
    s.position(bx, by, bz);
    for (auto const& u : s.nearby_friends())
    {
        if (!u.is_player || u.is_in_group || u.hp <= 0 || u.max_hp <= 0) continue;
        float const d = std::hypot(u.x - bx, u.y - by);
        if (d < 6.0f || d > 25.0f) continue;
        uint64 const other = u.guid.GetCounter();
        if (!Services::Registry().has(BotId(other)) || Services::Owners().GetOwner(BotId(other)).account_id != 0) continue;   // a world bot only
        int const other_hp = int((int64(u.hp) * 100) / u.max_hp);
        if (!V2::TownLife::DuelPartners(s.level(), s.hp_pct(), u.level, other_hp, s.bot_id(), other)) continue;
        uint32 const now = s.published_at_ms();
        V2::TownLife::PactSet(other, s.bot_id(), now);
        emit.cast(7266u, u.guid);   // Duel
        V2::TownLife::Counters().duels.fetch_add(1, std::memory_order_relaxed);
        ai.set_last_town_show_ms(now);
        ai.set_last_rule_fired("idle:town_duel");
        return true;
    }
    return false;
}

// ---------- idle:campfire ----------
// Outdoors at night, a few bots that stand still light a campfire (Basic Campfire, 818); the usual ambient sit does the rest.
bool CampfireGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (!TownLifeEnabled()) return false;
    if (!s.is_alive() || s.in_combat() || s.is_casting() || s.has_owner_character() || s.in_group() || s.is_moving()) return false;
    if (s.is_in_instance() || s.is_in_dungeon() || s.in_battleground() || s.raw().movement.is_mounted || s.is_swimming()) return false;
    if (s.enemies_within(40.0f) > 0 || !s.knows_spell(818u)) return false;
    int const hour = TimeBreakdown(GameTime::GetGameTime()).tm_hour;
    if (hour >= 6 && hour < 18) return false;
    uint32 const now = s.published_at_ms();
    uint8 lo = 0, hi = 0;
    if (!::Playerbot::V2::World::VanillaZoneLevelRange(s.zone_id(), lo, hi)) return false;   // outdoors, not in a capital
    if (ai.last_moving_ms() == 0 || now - ai.last_moving_ms() < 30u * 1000u) return false;   // has stood still a while
    if (ai.last_town_show_ms() && now - ai.last_town_show_ms() < 20u * 60u * 1000u) return false;
    return V2::TownLife::TakesPart(s.bot_id(), 4u, now, 4u, 30u * 60u * 1000u);
}

bool CampfireFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    ai.set_last_town_show_ms(s.published_at_ms());
    emit.cast(818u);
    V2::TownLife::Counters().campfires.fetch_add(1, std::memory_order_relaxed);
    ai.set_last_rule_fired("idle:campfire");
    return true;
}

} // anonymous namespace

void RegisterAmbientRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:leave_water";
        rule.priority = 888;   // just under the survival hazards, above everything a bot does for its own sake
        rule.gate     = &LeaveWaterGate;
        rule.fire     = &LeaveWaterFire;
        rule.min_interval_ms = 1000;
        r.register_rule(std::move(rule));
    }
    {
        // 709: above the quest funnel (698-730 band's lower part) and the assists; below survival hazards.
        IdleRule rule;
        rule.name     = "idle:leave_death_hotspot";
        rule.priority = 709;
        rule.gate     = &LeaveHotspotGate;
        rule.fire     = &LeaveHotspotFire;
        rule.min_interval_ms = 1000;
        r.register_rule(std::move(rule));
    }
    {
        // 706: just above assist_friend_pvp (705). PvE assist isn't
        // a higher emergency than PvP defense, but the cheaper gate
        // (no per-enemy player check) firing first avoids redundant
        // work when both want to run. Both still in survival band.
        IdleRule rule;
        rule.name     = "idle:assist_group_pve";
        rule.priority = 706;
        rule.gate     = &AssistGroupPveGate;
        rule.fire     = &AssistGroupPveFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:assist_friend_pvp";
        // Survival-tier priority (705). Was 215 — below mail/vendor/
        // gossip — which meant a bot whose groupmate was being killed
        // by an enemy player would run to the vendor / type "ding!" /
        // mail-thank first. World-PvP defense is an emergency, not
        // flavor. Sits just below the 880-900 survival hazards and
        // BG/dungeon dispatch (720/718) but above all utility/social.
        rule.priority = 705;
        rule.gate     = &AssistFriendPvpGate;
        rule.fire     = &AssistFriendPvpFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:pvp_help_callout";
        rule.priority = 703;        // paired emergency with assist_friend
        rule.gate     = &PvpHelpCalloutGate;
        rule.fire     = &PvpHelpCalloutFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:yell_lfg";
        rule.priority = 100;
        rule.gate     = &YellLfgGate;
        rule.fire     = &YellLfgFire;
        // Yell-LFG has an internal 45min throttle; coarse 30s skip
        // saves the per-tick gate eval (rest_bonus_xp + in_group +
        // verbosity reads) for 149/150 ticks.
        rule.min_interval_ms = 30000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:ambient_emote";
        rule.priority = 80;   // ambient flavor, lowest priority
        rule.gate     = &AmbientEmoteGate;
        rule.fire     = &AmbientEmoteFire;
        // Internal 5min throttle on emote cadence; 10s coarse skip.
        rule.min_interval_ms = 10000;
        r.register_rule(std::move(rule));
    }
    {
        // Look-around — face a random nearby NPC every 30s. Pure
        // cosmetic; pure side-effect (returns true but no movement).
        // Priority 70 (below ambient_emote 80) so emote wins when
        // both are ready. Throttle 30s.
        IdleRule rule;
        rule.name     = "idle:look_at_npc";
        rule.priority = 70;
        rule.gate     = &LookAroundGate;
        rule.fire     = &LookAroundFire;
        rule.min_interval_ms = 30000;
        r.register_rule(std::move(rule));
    }
    {
        // Priority 90 — above ambient_emote (80) so the sit emit fires
        // BEFORE the emote (matches legacy inline order). Fire returns
        // false so subsequent rules still run.
        IdleRule rule;
        rule.name     = "idle:ambient_sit";
        rule.priority = 90;
        rule.gate     = &AmbientSitGate;
        rule.fire     = &AmbientSitFire;
        r.register_rule(std::move(rule));
    }
    {
        // I-P3h: greet a nearby non-grouped real player. Priority 75 sits
        // between look_at_npc (70) and ambient_emote (80): reacting to a
        // passer-by matters more than idly facing an NPC but less than the
        // self-directed emote cadence. Coarse 30s skip; the heavy lifting
        // is the per-target 10-min GreetPlayer lockout inside the fire.
        IdleRule rule;
        rule.name     = "idle:react_to_passerby";
        rule.priority = 75;
        rule.gate     = &ReactToPasserbyGate;
        rule.fire     = &ReactToPasserbyFire;
        rule.min_interval_ms = 30000;
        r.register_rule(std::move(rule));
    }
    {
        // Fidget (Playerbot.Behavior.Fidget): below every other ambient rule (60), stateless
        // per-bot random cadence; see FidgetGate.
        IdleRule rule;
        rule.name     = "idle:fidget";
        rule.priority = 60;
        rule.gate     = &FidgetGate;
        rule.fire     = &FidgetFire;
        rule.min_interval_ms = 20000;
        r.register_rule(std::move(rule));
    }
    {
        // I-P3e: sustained state emote (dance/sleep) when settled in a hub.
        // Lowest priority (65) — pure ambient flavor, below look_at_npc.
        // Internal ~8min throttle; coarse 60s skip.
        IdleRule rule;
        rule.name     = "idle:ambient_state_emote";
        rule.priority = 65;
        rule.gate     = &AmbientStateEmoteGate;
        rule.fire     = &AmbientStateEmoteFire;
        rule.min_interval_ms = 60000;
        r.register_rule(std::move(rule));
    }
    {
        // Town life (Bot/TownLife.h): just above the rpg errands (110-135), so a bot whose turn it is does the show instead of strolling, and
        // below upkeep, quests and travel. Each takes part only for a share of the bots.
        IdleRule rule;
        rule.name     = "idle:inn_rest";
        rule.priority = 139;
        rule.gate     = &InnRestGate;
        rule.fire     = &InnRestFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:mailbox";
        rule.priority = 138;
        rule.gate     = &MailboxGate;
        rule.fire     = &MailboxFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:town_duel";
        rule.priority = 137;
        rule.gate     = &TownDuelGate;
        rule.fire     = &TownDuelFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:campfire";
        rule.priority = 136;
        rule.gate     = &CampfireGate;
        rule.fire     = &CampfireFire;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
