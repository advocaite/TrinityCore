// AmbientRules - Refactor #3 pass 11/12. Migrates top-level ambient/PvP rules
// out of the State_Idle tail:
//   - idle:assist_friend_pvp  : grouped bot's friend is being attacked by a
//                                hostile player -> start_attack on the attacker.
//   - idle:pvp_help_callout   : bot in low-HP open-world PvP attack -> /say
//                                "help! pvp <zone>" with a 5-min lockout.
//   - idle:yell_lfg           : solo level-60+ bot in a rested area (city/inn)
//                                /yells a role-flavored LFG line every ~22 min.
//   - idle:ambient_emote      : Roleplay-personality bot in a rested area emits
//                                a flavor /wave / /cheer / /salute targeted at
//                                a random groupmate (or untargeted solo).
//                                Added in pass 12 after IdleRule signature was
//                                extended to thread GroupSnapshotView.

#include <mutex>
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
#include "Bot/TownErrand.h"
#include "Bot/TownErrandConfig.h"
#include "Bot/ClassicUpkeep.h"
#include "Combat/Apl/ApClassicHeal.h"   // idle:kind_heal: the class heal and its rank
#include "ClassicSpec.h"                // Classic::IsSingleSpecClass
#include "SharedDefines.h"
#include "Fleet/GateDuels.h"
#include "Fleet/Gathering.h"
#include "Fleet/StrangerBoard.h"
#include "Random.h"
#include "Bot/ClassicUpkeep.h"
#include "Bot/ShareMobs.h"
#include "Bot/ChannelTalk.h"
#include "Bot/ChannelChat.h"
#include "Bot/PartyLink.h"
#include "Bot/BotRegistry.h"
#include "Bot/BotBridge.h"
#include "Fleet/BotPool.h"
#include "Fleet/OwnerRegistry.h"
#include "Fleet/WorldPvpConfig.h"
#include "Util.h"
#include "Log.h"
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

// ---------- idle:world_pvp_flag / idle:world_pvp_ambush ----------
// World PvP of the world bots on a PvP or free-for-all realm (Fleet/WorldPvp.h). A flagged bot keeps its PvP flag on as a player who
// typed /pvp; a hunter (a flagged bot too) sometimes attacks an enemy it sees, a roll per enemy every RollMinutes. Only free world
// bots: never a companion, a hired bot or a bot in a group, never in a battleground, dungeon, raid or capital.
namespace F = ::Playerbot::V2::Fleet;

static bool FreeWorldBotForPvp(BotSnapshotView const& s)
{
    return s.is_alive() && !s.has_owner_character() && !s.in_group() && !s.in_battleground() && !s.is_in_instance()
        && !s.is_in_dungeon() && !s.is_sanctuary() && !::Playerbot::V2::World::IsVanillaCapitalZone(s.zone_id());
}

// The PvP button of the bot follows its temper: on for a flagged bot (it stays flagged everywhere); off for the others, which then
// carry the flag only where the realm puts it on everyone (hostile territory), and lose it 5 minutes after leaving it, as a player.
// A bot whose button was turned on here and whose temper is no longer flagged (the setting changed, a normal realm now) turns it off.
static bool WantsPvpFlag(BotSnapshotView const& s)
{
    F::LiveWorldPvp const w = F::CurrentWorldPvp();
    return F::IsPvpFlagged(w.cfg, w.realm, uint64(s.bot_id()));
}

bool WorldPvpFlagGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (s.in_combat() || s.has_owner_character() || s.in_group() || !s.is_alive() || s.in_battleground()) return false;
    return WantsPvpFlag(s) != s.pvp_wanted();
}

bool WorldPvpFlagFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    if (!emit.set_pvp(WantsPvpFlag(s))) return false;
    ai.set_last_rule_fired("idle:world_pvp_flag");
    return true;
}

bool WorldPvpAmbushGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (s.in_combat() || s.is_casting() || !FreeWorldBotForPvp(s)) return false;
    if (s.hp_pct() < 80 || s.raw().movement.is_mounted) return false;
    F::LiveWorldPvp const w = F::CurrentWorldPvp();
    return F::IsPvpHunter(w.cfg, w.realm, uint64(s.bot_id()));
}

bool WorldPvpAmbushFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    F::LiveWorldPvp const w = F::CurrentWorldPvp();
    uint64 const now_s = uint64(GameTime::GetGameTime());
    float bx, by, bz;
    s.position(bx, by, bz);
    NearbyUnit const* best = nullptr;
    float best_d = 35.0f;   // what a player sees coming: a rogue still has to walk in from here
    auto consider = [&](NearbyUnit const& u)
    {
        if (!u.is_player || u.hp <= 0 || u.untargetable || !u.in_los || u.guid == s.raw().guid) return;
        if (!F::LevelOk(w.cfg, uint32(s.level()), uint32(u.level))) return;
        bool const is_bot = Services::Registry().has(BotId(u.guid.GetCounter()));
        if (is_bot && !w.cfg.attack_bots) return;
        if (ai.start_attack_recently_refused(u.guid.GetCounter(), now_ms)) return;
        float const d = std::hypot(u.x - bx, u.y - by);
        if (d >= best_d || std::fabs(u.z - bz) > 12.0f) return;
        if (!F::AttackRoll(w.cfg, w.realm, uint64(s.bot_id()), u.guid.GetCounter(), now_s)) return;
        best = &u;
        best_d = d;
    };
    for (auto const& u : s.nearby_enemies())
        consider(u);
    // Free-for-all: the bot's own faction is fair game too (start_attack is refused by the server where it is not, and that
    // target is then left alone for a while).
    if (w.realm == F::RealmPvp::Ffa)
        for (auto const& u : s.nearby_friends())
            if (!u.is_in_group)
                consider(u);
    if (!best) return false;
    // The class's own first hit (F::PvpOpeners): the first one known, ready and in range. A caster that opens with a spell does not
    // also start a melee swing (it would run in); a warrior's Charge and every class without a fitting opener start the attack.
    bool const in_form = s.has_aura(768) || s.has_aura(5487) || s.has_aura(9634);   // cat, bear, dire bear
    F::PvpOpener const* list = nullptr;
    int const n = F::PvpOpeners(s.cls(), in_form, list);
    bool const has_seal = s.has_aura(21084) || s.has_aura(20375) || s.has_aura(20165) || s.has_aura(20166) || s.has_aura(21082);
    uint32 opener = 0;
    for (int i = 0; i < n && !opener; ++i)
    {
        F::PvpOpener const& o = list[i];
        if (best_d < o.min_yards || best_d > o.max_yards || (o.needs_seal && !has_seal)) continue;
        if (s.knows_spell(o.spell) && s.is_ready(o.spell)) opener = o.spell;
    }
    bool const melee_too = !opener || opener == 100;   // Charge, or no opener: swing as well
    if (opener && !emit.cast(opener, best->guid)) opener = 0;
    if ((melee_too || !opener) && !emit.start_attack(best->guid) && !opener) return false;
    ai.set_last_rule_fired("idle:world_pvp_ambush");
    TC_LOG_INFO("playerbot.v2", "[world_pvp] {} lvl={} cls={} zone={} attacks {} {} lvl={} at {:.0f} y, opener {} ({} realm)", s.name(),
                uint32(s.level()), uint32(s.cls()), s.zone_id(), Services::Registry().has(BotId(best->guid.GetCounter())) ? "bot" : "player",
                best->guid.GetCounter(), uint32(best->level), best_d, opener, w.realm == F::RealmPvp::Ffa ? "free-for-all" : "PvP");
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
    static bool const on = sConfigMgr->GetBoolDefault("Playerbot.World.TownLife", true, true);
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
    constexpr uint32 kYellThrottleMs = 22u * 60u * 1000u;   // one call per bot per 22 minutes (the realm-wide 20 s faction slot still holds)
    const uint32 yell_now_ms = s.published_at_ms();
    // Below level 60 only a share of the bots call at a time (a different set every 22 minutes).
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
    const Role role = ai.effective_role(s);
    uint8 const role_bit = role == Role::Tank ? V2::ChannelTalk::kTank : role == Role::Healer ? V2::ChannelTalk::kHealer : V2::ChannelTalk::kDps;
    const uint32 sel_seed = (s.bot_id() ^ (yell_now_ms / (22u * 60u * 1000u)));
    // A bot of a dungeon's level names the dungeon.
    char const* dungeon = s.level() < 60 ? V2::TownLife::LfgDungeonFor(s.level(), sel_seed) : nullptr;
    // Playerbot.Chatter.LfgChannel: the call goes to the LookingForGroup channel (or the zone's General), like a player's, with many
    // shapes and never a line that went out a moment ago; one call per faction per 20 s realm-wide. 0 = the old /yell in town.
    if (Services::Config().chatter_lfg_channel())
    {
        uint32 const lfg = V2::ChannelChat::LfgChannelId();
        uint32 const channel = (lfg && urand(0, 99) < 60) ? lfg : 1u;
        std::string line;
        for (uint32 attempt = 0; attempt < 4 && line.empty(); ++attempt)
        {
            line = V2::ChannelTalk::LfgCallLine(role_bit, dungeon, s.level(), urand(0, 0xFFFFFF));
            if (!V2::ChannelChat::LfgCallSlot(s.is_alliance(), line, yell_now_ms))
                line.clear();
        }
        ai.set_last_yell_lfg_ms(yell_now_ms);   // a refused slot waits for the bot's next turn as well
        if (line.empty())
            return false;
        V2::TownLife::Counters().lfg_calls.fetch_add(1, std::memory_order_relaxed);
        emit.emit(ChatIntent{ ChannelChatIntent{ uint8(channel), std::move(line) } });
        ai.set_last_rule_fired("idle:yell_lfg");
        return true;
    }
    std::string line = V2::ChannelTalk::LfgCallLine(role_bit, dungeon, s.level(), sel_seed);
    V2::TownLife::Counters().lfg_calls.fetch_add(1, std::memory_order_relaxed);
    emit.yell_world(line);
    ai.set_last_yell_lfg_ms(yell_now_ms);
    ai.set_last_rule_fired("idle:yell_lfg");
    return true;
}

// ---------- idle:ambient_sit ----------
// No longer sits: a bot sits only while it eats or drinks (idle:ooc_eat_food), never to wait or regenerate. The rule stays because its
// gate keeps ai.last_moving_ms() current, which the fidget, look-around and wander-pause rules read as "has stood still a while".
bool AmbientSitGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (s.is_moving())
        ai.set_last_moving_ms(s.published_at_ms());
    else if (ai.last_moving_ms() == 0)
        ai.set_last_moving_ms(s.published_at_ms());   // just spawned: count the standstill from now
    return false;
}

bool AmbientSitFire(BotSnapshotView const&, BotAI&, GroupSnapshotView const&, BotIntentEmitter&, uint32)
{
    return false;
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
//
// 2026-11-09: on a live realm a stranger walking through Stormwind is greeted now and then, not by every bot he passes (with fifty
// bots in town the old per-bot lockout made five waves a minute, which read as robotic). So a real player is greeted by at most one
// bot every two and a half to four minutes, realm-wide (TakePasserbySlot); bots never greet each other this way (they are in the
// registry); the greeting varies (a wave, a nod, a bow, a salute); and with a program writing the bots' talk (Playerbot.Bridge.Talk)
// one greeting in two also comes with a short /say line to him (BotBridge::QueueGreet).
std::mutex g_passerby_mtx;
std::unordered_map<uint64, uint32> g_passerby_ms;   // real player guid low -> when a bot last greeted him

bool PasserbySlotFree(uint64 player, uint32 now_ms)
{
    uint32 const gap = 150000u + uint32((player * 2654435761ull) % 90000u);
    std::lock_guard<std::mutex> lk(g_passerby_mtx);
    auto it = g_passerby_ms.find(player);
    return it == g_passerby_ms.end() || now_ms - it->second >= gap;
}

bool TakePasserbySlot(uint64 player, uint32 now_ms)
{
    uint32 const gap = 150000u + uint32((player * 2654435761ull) % 90000u);
    std::lock_guard<std::mutex> lk(g_passerby_mtx);
    if (g_passerby_ms.size() > 4096)
        g_passerby_ms.clear();
    auto [it, fresh] = g_passerby_ms.try_emplace(player, now_ms);
    if (!fresh && now_ms - it->second < gap)
        return false;
    it->second = now_ms;
    return true;
}

bool ReactToPasserbyGate(BotSnapshotView const& s, BotAI& ai,
                         GroupSnapshotView const&, uint32)
{
    if (ai.personality().verbosity < Verbosity::Normal) return false;
    if (!s.is_alive()) return false;
    if (::Playerbot::Pool::IsScenarioBot(s.bot_id())) return false;   // a test squad does not greet the town
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
        if (Services::Registry().has(u.guid.GetCounter())) continue;   // another bot
        if (!PasserbySlotFree(u.guid.GetCounter(), s.published_at_ms())) continue;
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
        if (Services::Registry().has(u.guid.GetCounter())) continue;
        if (!PasserbySlotFree(u.guid.GetCounter(), now_ms)) continue;
        // Per-target hard lockout (10 min, ActionKind::GreetPlayer).
        if (ai.action_recently_tried(BotAI::ActionKind::GreetPlayer,
                                     u.guid.GetCounter(), now_ms))
            continue;
        const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
        const float d2 = dx*dx + dy*dy + dz*dz;
        if (d2 < best_d2) { best_d2 = d2; best = u.guid; }
    }
    if (best.IsEmpty()) return false;
    if (!TakePasserbySlot(best.GetCounter(), now_ms)) return false;   // another bot greeted him meanwhile
    // Face them, then a wave (3), a nod (273), a bow (2) or a salute (66), aimed at him.
    static constexpr uint32 kGreets[] = { 3, 3, 3, 273, 273, 2, 66 };
    emit.face_target(best);
    if (!emit.perform_emote(kGreets[urand(0, uint32(std::size(kGreets) - 1))], best)) return false;
    ai.note_action_retry(BotAI::ActionKind::GreetPlayer,
                         best.GetCounter(), now_ms);
    if (best_d2 <= 15.0f * 15.0f && urand(0, 1))
        BotBridge::QueueGreet(s.bot_id(), best);
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
bool LeaveWaterGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!s.is_alive() || s.in_combat() || s.is_casting() || !s.is_swimming()) return false;
    // A group led by a bot gets out too, every member on its own: its leader is swimming as well, and following it kept the whole
    // party in the canal (live 244: four bots 490 s+ in the Stormwind canal behind their bot leader).
    if ((s.in_group() && !g.leader_is_bot()) || s.is_in_dungeon() || s.in_battleground()) return false;
    if (ai.in_post_rescue_grace(now_ms) || ai.in_charter_grace(now_ms)) return false;
    uint8 lo = 0, hi = 0;
    return !::Playerbot::V2::World::VanillaZoneLevelRange(s.zone_id(), lo, hi);
}

bool LeaveWaterFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    // The nearest dry footing first (the builder's water_escape point, a bank or a stair): this rule outranks idle:water_escape, and a
    // walk to a hub across the canal kept the bot swimming.
    if (s.water_escape_valid())
    {
        emit.move_to(s.water_escape_x(), s.water_escape_y(), s.water_escape_z(), /*run*/ true);
        ai.set_last_rule_fired("idle:leave_water");
        return true;
    }
    auto const* hub = Services::Hubs().GetNearestQuestHub(s.raw());
    if (!hub || hub->mapId != s.map_id()) return false;
    return States::ChunkedWalkToward(s, ai, emit, hub->location.GetPositionX(), hub->location.GetPositionY(),
                                     hub->location.GetPositionZ(), "idle:leave_water", now_ms);
}

// ---------- idle:inn_rest ----------
// In a capital a share of the bots (most at night) walk to the nearest inn and stand there for a while. One check of the town NPC index
// per tick, only for the bots whose turn it is. The inn holds Playerbot.World.Town.InnCap bots at once (TownErrand::Spots): with no cap
// every resting bot of the city stood in the one inn (48 bots in the Gilded Rose in 20 minutes, live 273). A bot turned away, or one a
// real player stands next to for PlayerNudgeSec, goes about its errands instead.
constexpr float kInnSearchYards = 150.0f;
constexpr float kInnStandYards = 4.5f;
constexpr uint32 kSpotHoldMs = 30000u;   // a claim not renewed for this long lapses (the bot walked off, logged out or died)

namespace TE = ::Playerbot::V2::TownErrand;

// One line per bot and outcome every five minutes, so a soak shows which bots reached the inn and which found no innkeeper (the counters
// above are bot-ticks and a few bots dominate them).
static void InnLog(BotSnapshotView const& s, char const* what, float x, float y)
{
    static std::mutex mx;
    static std::unordered_map<uint64, uint32> last;
    uint32 const now = s.published_at_ms() ? s.published_at_ms() : 1u;
    uint64 const key = (uint64(s.bot_id()) << 2) | (what[0] == 'r' ? 1u : what[0] == 'i' ? 2u : 0u);
    {
        std::lock_guard<std::mutex> lock(mx);
        uint32& at = last[key];
        if (at && now - at < 300000u)
            return;
        at = now;
    }
    TC_LOG_INFO("playerbot.v2", "[inn] {} {} (zone {}, {:.0f},{:.0f})", s.raw().identity.name, what, s.zone_id(), x, y);
}

// A real player (not a bot) standing within `yards` of the bot.
static bool RealPlayerWithin(BotSnapshotView const& s, float yards)
{
    if (yards <= 0.0f) return false;
    float bx, by, bz;
    s.position(bx, by, bz);
    for (auto const& u : s.nearby_friends())
    {
        if (!u.is_player || u.hp <= 0) continue;
        if (std::hypot(u.x - bx, u.y - by) > yards || std::fabs(u.z - bz) > 6.0f) continue;
        if (!Services::Registry().has(BotId(u.guid.GetCounter()))) return true;
    }
    return false;
}

// Tracks how long a real player has stood next to the standing bot; true once it is PlayerNudgeSec.
static bool PlayerNudgeDue(BotSnapshotView const& s, TE::State& st, uint32 now_ms)
{
    auto const& c = TE::Cfg();
    if (!RealPlayerWithin(s, c.nudge_yards)) { st.near_player_ms = 0; return false; }
    if (st.near_player_ms == 0) { st.near_player_ms = now_ms ? now_ms : 1u; return false; }
    return now_ms - st.near_player_ms >= c.nudge_ms;
}

bool InnRestGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (!TownLifeEnabled()) return false;
    if (!::Playerbot::V2::World::IsVanillaCapitalZone(s.zone_id())) return false;
    if (TE::Cfg().inn_cap == 0) return false;
    if (int32(ai.town_errand().inn_skip_until_ms - s.published_at_ms()) > 0) return false;
    int const hour = TimeBreakdown(GameTime::GetGameTime()).tm_hour;
    uint32 const pct = V2::TownLife::InnPercent(hour) * TE::Cfg().inn_share_pct / 100u;
    if (!V2::TownLife::TakesPart(s.bot_id(), 1u, s.published_at_ms(), pct, 15u * 60u * 1000u)) return false;
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
    if (!inn)
    {
        V2::TownLife::Counters().inn_none.fetch_add(1, std::memory_order_relaxed);
        InnLog(s, "no innkeeper within reach", bx, by);
        return false;
    }
    auto& st = ai.town_errand();
    uint64 const place = TE::Spots::PlaceKey(s.map_id(), inn->x, inn->y);
    uint32 slot = 0;
    if (!TE::Spots::Get().Claim(place, s.bot_id(), now_ms, kSpotHoldMs, TE::Cfg().inn_cap, &slot))
    {
        // Full: this window goes to the errands instead.
        st.inn_skip_until_ms = now_ms + 15u * 60u * 1000u;
        V2::TownLife::Counters().inn_full.fetch_add(1, std::memory_order_relaxed);
        InnLog(s, "inn is full, goes on errands", bx, by);
        return false;
    }
    // Stand a few yards from the innkeeper, on the side the bot comes from, each bot in its own slot of a fan so they stand side by side.
    float tx, ty;
    TE::StandPoint(inn->x, inn->y, bx, by, kInnStandYards + float(slot / 5u) * 1.5f, slot, tx, ty);
    // The stand point is a straight line from the innkeeper and can sit behind a wall; the inn is the goal, so a bot that is within a
    // few yards of the innkeeper, or that cannot get closer than a room's width, rests where it is.
    float const to_inn = std::hypot(bx - inn->x, by - inn->y);
    if (to_inn > kInnStandYards + 6.0f && std::hypot(bx - tx, by - ty) > 4.0f)
    {
        V2::TownLife::Counters().inn_walk.fetch_add(1, std::memory_order_relaxed);
        if (States::ChunkedWalkToward(s, ai, emit, tx, ty, inn->z, "idle:inn_rest", now_ms))
            return true;
        V2::TownLife::Counters().inn_stuck.fetch_add(1, std::memory_order_relaxed);
        if (to_inn > 20.0f) { TE::Spots::Get().Release(place, s.bot_id()); return false; }
    }
    V2::TownLife::Counters().inn_ticks.fetch_add(1, std::memory_order_relaxed);
    if (s.is_moving()) return false;
    // A real player standing next to the resting bot for a while: it gets up and goes about its business.
    if (PlayerNudgeDue(s, st, now_ms))
    {
        st.near_player_ms = 0;
        st.inn_skip_until_ms = now_ms + 15u * 60u * 1000u;
        TE::Spots::Get().Release(place, s.bot_id());
        V2::TownLife::Counters().player_nudges.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    InnLog(s, "rests at the inn", bx, by);
    // A bot sits only while it eats or drinks, so the inn visit is a stand.
    ai.set_last_rule_fired("idle:inn_rest");
    return true;
}

// ---------- idle:mailbox ----------
// A few bots in a town walk to the nearest mailbox and stand there a while, then the window turns over and they go back to their errands.
// The share is picked per time window by hash, so nothing is stored per bot. A mailbox takes Playerbot.World.Town.MailboxCap bots at once.
constexpr float kMailboxSearchYards = 120.0f;
constexpr float kMailboxStandYards = 2.5f;
constexpr uint32 kMailboxWindowMs = 90u * 1000u;

bool MailboxGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (!TownLifeEnabled()) return false;
    if (!::Playerbot::V2::World::IsVanillaCapitalZone(s.zone_id())) return false;
    if (TE::Cfg().mailbox_cap == 0) return false;
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
    uint64 const place = TE::Spots::PlaceKey(s.map_id(), box->x, box->y);
    uint32 slot = 0;
    if (!TE::Spots::Get().Claim(place, s.bot_id(), now_ms, kSpotHoldMs, TE::Cfg().mailbox_cap, &slot))
    {
        V2::TownLife::Counters().mailbox_full.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    V2::TownLife::Counters().mailbox_ticks.fetch_add(1, std::memory_order_relaxed);
    float tx, ty;
    TE::StandPoint(box->x, box->y, bx, by, kMailboxStandYards, slot, tx, ty);
    if (std::hypot(bx - box->x, by - box->y) > kMailboxStandYards + 1.5f)
        return States::ChunkedWalkToward(s, ai, emit, tx, ty, box->z, "idle:mailbox", now_ms);
    if (s.is_moving()) return false;
    ai.set_last_rule_fired("idle:mailbox");
    return true;
}

// ---------- idle:town_errand ----------
// A capital's residents go about their business (Bot/TownErrand.h): each runs to a real place anywhere in the city (the auction house,
// the bank, its class trainer, the inn, a mailbox, the flight master, a vendor, now and then the city gate), stops there a few seconds
// facing the NPC, and runs straight on to the next place. The walk is one pathed move to the place, so the bot runs a smooth line
// through the streets and past the player, instead of the 25 y random wander hops with a stop and a turn at each. Every place holds a
// few bots at once (SpotCap); a bot finding one full picks another.
constexpr uint32 kErrandWalkTimeoutMs = 120000u;
constexpr uint32 kErrandStaleMs = 180000u;
constexpr uint64 kGateKeyBase = 0xE0000000ull;

static bool HasGreyJunkItem(BotSnapshotView const& s)
{
    for (auto const& it : s.raw().inventory.bag_items)
        if (!it.guid.IsEmpty() && !it.is_quest_item && it.quality == ITEM_QUALITY_POOR)
            return true;
    return false;
}

static uint32 PlaceCap(TE::Kind k)
{
    auto const& c = TE::Cfg();
    if (k == TE::Kind::Inn) return std::max<uint32>(1u, c.inn_cap);
    if (k == TE::Kind::Mailbox) return std::max<uint32>(1u, c.mailbox_cap);
    if (k == TE::Kind::Exit) return c.spot_cap * 4u;   // a road: the bots leaving by it walk straight through
    return c.spot_cap;
}

// The places of the city within ErrandRadius of (bx, by) that this bot has a reason to go to.
static void TownDests(BotSnapshotView const& s, float bx, float by, float bz, std::vector<TE::Dest>& out)
{
    auto const& c = TE::Cfg();
    bool const need_vendor = HasGreyJunkItem(s) || (s.lowest_equipped_durability_pct() < 100 && s.gold() > 0);
    // Stormwind stands on layers 20 y and more apart, joined only by steep faces (Fleet/CityLayer.h): a place on another layer is not
    // an errand.
    constexpr float kLayerYards = 18.0f;
    ::Playerbot::V2::Travel::TownNpcIndex::Instance().ForEachNear(s.map_id(), bx, by, c.radius, [&](::Playerbot::V2::Travel::TownNpc const& n)
    {
        if (!n.usable_by(s.is_alliance()) || std::fabs(n.z - bz) > kLayerYards) return;
        uint32 const f = n.npc_flags;
        TE::Kind k = TE::Kind::None;
        float boost = 1.0f;
        if (f & UNIT_NPC_FLAG_AUCTIONEER) k = TE::Kind::Auction;
        else if (f & UNIT_NPC_FLAG_BANKER) k = TE::Kind::Bank;
        else if (f & UNIT_NPC_FLAG_INNKEEPER) k = TE::Kind::Inn;
        else if (f & UNIT_NPC_FLAG_FLIGHTMASTER) k = TE::Kind::Flight;
        else if ((f & UNIT_NPC_FLAG_BATTLEMASTER) && s.level() >= 10) k = TE::Kind::Battlemaster;
        else if ((f & UNIT_NPC_FLAG_STABLEMASTER) && s.cls() == CLASS_HUNTER) k = TE::Kind::Stable;
        else if ((f & UNIT_NPC_FLAG_TRAINER) && n.trainer_class != 0 && n.trainer_class == s.cls()) k = TE::Kind::Trainer;
        else if (f & (UNIT_NPC_FLAG_VENDOR_MASK | UNIT_NPC_FLAG_REPAIR)) { k = TE::Kind::Vendor; if (need_vendor) boost = 5.0f; }
        if (k == TE::Kind::None) return;
        out.push_back({ k, n.spawn_id, n.entry, n.x, n.y, n.z, TE::BaseWeight(k) * boost });
    });
    ::Playerbot::V2::Travel::TownNpcIndex::Instance().ForEachMailboxNear(s.map_id(), bx, by, c.radius, [&](::Playerbot::V2::Travel::TownNpc const& n)
    {
        if (std::fabs(n.z - bz) > kLayerYards) return;
        out.push_back({ TE::Kind::Mailbox, n.spawn_id, 0u, n.x, n.y, n.z, TE::BaseWeight(TE::Kind::Mailbox) });
    });
    if (c.exits)
    {
        V2::GateDuels::GateSpot gate;
        uint32 city = 0;
        if (V2::GateDuels::Board::Get().GateNear(s.map_id(), bx, by, c.radius, s.is_alliance(), gate, city))
            out.push_back({ TE::Kind::Exit, kGateKeyBase + city, 0u, gate.x, gate.y, gate.z, TE::BaseWeight(TE::Kind::Exit) });
    }
}

static void EndErrand(BotSnapshotView const& s, TE::State& st, bool done)
{
    if (st.place) TE::Spots::Get().Release(st.place, s.bot_id());
    if (st.key) st.recent.add(st.key);
    if (done) ++st.errands;
    st.Clear();
}

// A bot placed in the town by the spawner (TownPct) is in its profession mode for 45-90 minutes and lives there. Any other bot in a
// capital runs errands in its town third of each hour (the rpg phase of RpgRules.cpp); out of it, its one errand is the walk out to the
// city gate, from where idle:grind_roam takes it to level (live 275: these bots stood in the city under idle:wander, 25 y hops with a
// stop and a turn at each, 406 of 507 wander lines in a capital).
static bool TownTime(BotSnapshotView const& s, BotAI& ai, uint32 now_ms)
{
    if (ai.in_profession_mode(now_ms)) return true;
    uint32 const seed = uint32(s.bot_id()) * 2654435761u;
    return ((seed % 61u) + now_ms / 60000u) % 61u <= 20u;
}

constexpr uint32 kLeaveMs = 10u * 60u * 1000u;   // out at the gate: this long with no errands, grind_roam leads the bot off

// Choose the next place and claim a stand slot there. False when the city offers nothing.
static bool PickErrand(BotSnapshotView const& s, BotAI& ai, TE::State& st, uint32 now_ms)
{
    float bx, by, bz;
    s.position(bx, by, bz);
    std::vector<TE::Dest> dests;
    dests.reserve(64);
    TownDests(s, bx, by, bz, dests);
    // Out of its town time a bot only leaves; with no gate known it keeps to the errands rather than wander on the spot.
    bool const leaving = !TownTime(s, ai, now_ms)
        && std::any_of(dests.begin(), dests.end(), [](TE::Dest const& d) { return d.kind == TE::Kind::Exit; });
    if (leaving)
        std::erase_if(dests, [](TE::Dest const& d) { return d.kind != TE::Kind::Exit; });
    if (dests.empty()) return false;
    uint64 const bot = s.bot_id();
    uint32 const map = s.map_id();
    auto full = [&](TE::Dest const& d) { return TE::Spots::Get().Full(TE::Spots::PlaceKey(map, d.x, d.y), bot, now_ms, PlaceCap(d.kind)); };
    // Home now (10 minutes in this city, not bound here): the next errand is an inn with room, where idle:homebind binds the
    // hearthstone. The walk to the innkeeper (idle:walk_to_innkeeper_rebind) leaves a capital's residents to this. Every inn full:
    // the usual errands, and the inn next time.
    if (!leaving && ai.hearth_rebind_due(s.zone_id())
        && std::any_of(dests.begin(), dests.end(), [&](TE::Dest const& d) { return d.kind == TE::Kind::Inn && !full(d); }))
        std::erase_if(dests, [](TE::Dest const& d) { return d.kind != TE::Kind::Inn; });
    int const k = TE::Pick(dests, bx, by, TE::Cfg().radius, leaving ? TE::Recent{} : st.recent, float(ai.rng().unit()), full);
    if (k < 0) return false;
    TE::Dest const& d = dests[size_t(k)];
    uint64 const place = TE::Spots::PlaceKey(map, d.x, d.y);
    uint32 slot = 0;
    if (!TE::Spots::Get().Claim(place, bot, now_ms, kSpotHoldMs, PlaceCap(d.kind), &slot))
        return false;
    st.Clear();
    st.kind = d.kind; st.key = d.key; st.place = place; st.entry = d.entry; st.slot = slot;
    st.leaving = leaving;
    st.nx = d.x; st.ny = d.y; st.nz = d.z;
    float const ring = d.kind == TE::Kind::Mailbox ? 2.5f : d.kind == TE::Kind::Exit ? 3.0f + float(slot % 6u) * 1.5f : 3.5f + float(slot / 5u) * 1.5f;
    TE::StandPoint(d.x, d.y, bx, by, ring, slot, st.tx, st.ty);
    st.tz = d.z;
    st.phase = 1;
    st.phase_start_ms = now_ms;
    // One bot in 16 logs its errands, so a soak shows where residents go without a line per errand of every bot.
    if (s.bot_id() % 16u == 0u)
        TC_LOG_INFO("playerbot.v2", "[town] {} -> {} ({:.0f},{:.0f}) {:.0f} y slot {} (zone {})",
                    s.name(), TE::KindName(d.kind), d.x, d.y, std::hypot(d.x - bx, d.y - by), slot, s.zone_id());
    return true;
}

// At the place: face the NPC, sell the junk or repair at a vendor, one gesture now and then, and stop a few seconds.
static void ArriveErrand(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit, TE::State& st, uint32 now_ms)
{
    float bx, by, bz;
    s.position(bx, by, bz);
    if (s.is_mounted())
        emit.dismount();
    if (st.entry)
    {
        for (auto const& u : s.nearby_friends())
        {
            if (u.is_player || u.entry != st.entry || std::hypot(u.x - st.nx, u.y - st.ny) > 15.0f) continue;
            emit.face_target(u.guid);
            uint64 const low = u.guid.GetCounter();
            bool const in_reach = std::hypot(u.x - bx, u.y - by) <= 5.0f;
            if (in_reach && (u.npc_flags & UNIT_NPC_FLAG_VENDOR_MASK) && HasGreyJunkItem(s)
                && !ai.action_recently_tried(BotAI::ActionKind::SellTrash, low, now_ms))
            {
                emit.vendor_sell_trash(u.guid);
                ai.note_action_retry(BotAI::ActionKind::SellTrash, low, now_ms);
            }
            if (in_reach && (u.npc_flags & UNIT_NPC_FLAG_REPAIR) && s.lowest_equipped_durability_pct() < 100 && s.gold() > 0
                && !ai.action_recently_tried(BotAI::ActionKind::Repair, low, now_ms))
            {
                emit.repair_all(u.guid);
                ai.note_action_retry(BotAI::ActionKind::Repair, low, now_ms);
            }
            if (ai.rng().chance(0.4))
            {
                static constexpr uint32 kTalk[] = { EMOTE_ONESHOT_TALK, EMOTE_ONESHOT_TALK, EMOTE_ONESHOT_BOW, EMOTE_ONESHOT_QUESTION, EMOTE_ONESHOT_POINT };
                emit.perform_emote(kTalk[ai.rng().int_range(0, int32(std::size(kTalk)))], u.guid);
            }
            break;
        }
    }
    auto const& c = TE::Cfg();
    uint32 linger = c.linger_min_ms;
    if (c.linger_max_ms > c.linger_min_ms)
        linger += uint32(ai.rng().int_range(0, int32((c.linger_max_ms - c.linger_min_ms) / 100u) + 1)) * 100u;
    if (st.kind == TE::Kind::Exit || st.kind == TE::Kind::Mailbox) linger /= 2u;   // a look out of the gate, a letter posted
    st.phase = 2;
    st.phase_start_ms = now_ms;
    st.linger_until_ms = now_ms + linger;
    st.near_player_ms = 0;
    V2::TownLife::Counters().errands.fetch_add(1, std::memory_order_relaxed);
}

bool TownErrandGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32 now_ms)
{
    if (!TE::Cfg().errands) return false;
    if (int32(ai.town_errand().leave_until_ms - now_ms) > 0) return false;   // left by the gate: off to level
    // A member of a small party of world bots (Fleet/BotParties.h) goes where its leader goes: idle:party_follow sits under this rule.
    {
        std::shared_ptr<std::unordered_map<uint64, V2::Parties::Link> const> keep;
        if (V2::Parties::LinkOf(s, keep)) return false;
    }
    auto const& st = ai.town_errand();
    // A bot on its way out to the city gate is outside the capital's zone for the last yards; it finishes the errand.
    if (!::Playerbot::V2::World::IsVanillaCapitalZone(s.zone_id()) && !(st.phase != 0 && st.kind == TE::Kind::Exit)) return false;
    if (!s.is_alive() || s.in_combat() || s.is_casting() || s.is_stunned() || s.is_rooted()) return false;
    if (s.has_owner_character() || s.in_group() || s.on_transport()) return false;
    if (s.is_in_instance() || s.is_in_dungeon() || s.in_battleground()) return false;
    if (s.raw().social_events.has_duel_request || s.duel_in_progress()) return false;
    if (s.has_actionable_quest() || Classic::RecoverBeforeWalking(s)) return false;   // quest work in reach, or hurt / mid-meal / dry
    return s.enemies_within(30.0f) == 0;
}

bool TownErrandFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    auto& st = ai.town_errand();
    // An errand left long ago (a fight, a quest, a show took over) is not resumed: the next place is chosen from here.
    if (st.phase != 0 && now_ms - st.phase_start_ms > kErrandStaleMs)
        EndErrand(s, st, false);

    if (st.phase == 2)
    {
        if (st.place) TE::Spots::Get().Claim(st.place, s.bot_id(), now_ms, kSpotHoldMs, PlaceCap(st.kind));
        bool const nudged = PlayerNudgeDue(s, st, now_ms);
        if (nudged) V2::TownLife::Counters().player_nudges.fetch_add(1, std::memory_order_relaxed);
        if (now_ms < st.linger_until_ms && !nudged)
        {
            ai.set_last_rule_fired("idle:town_linger");
            return true;
        }
        EndErrand(s, st, true);   // done: straight on to the next place, this tick
    }

    if (st.phase == 0 && !PickErrand(s, ai, st, now_ms))
        return false;

    // On the way.
    float bx, by, bz;
    s.position(bx, by, bz);
    TE::Spots::Get().Claim(st.place, s.bot_id(), now_ms, kSpotHoldMs, PlaceCap(st.kind));
    float const d_npc = std::hypot(bx - st.nx, by - st.ny);
    float const d_stand = std::hypot(bx - st.tx, by - st.ty);
    float const ring = std::hypot(st.tx - st.nx, st.ty - st.ny);
    bool there = (d_stand <= 2.5f || d_npc <= ring + 1.0f) && std::fabs(bz - st.nz) < 6.0f;
    if (!there)
    {
        // No 3 y of progress in 8 s, standing or running against a wall.
        bool const stalled = st.issued && TE::Stalled(st, bx, by, now_ms);
        if (stalled && d_npc <= 15.0f && std::fabs(bz - st.nz) < 8.0f)
            there = true;   // in the room: the stand point is behind a counter or a table
        else if (stalled || now_ms - st.phase_start_ms > kErrandWalkTimeoutMs)
        {
            V2::TownLife::Counters().errand_gave_up.fetch_add(1, std::memory_order_relaxed);
            if (s.bot_id() % 16u == 0u)
                TC_LOG_INFO("playerbot.v2", "[town] {} gave up on the {} at ({:.0f},{:.0f}), {:.0f} y short", s.name(), TE::KindName(st.kind), st.nx, st.ny, d_npc);
            EndErrand(s, st, false);
            return false;
        }
    }
    if (there && st.leaving)
    {
        // Out of the gate: no stop, the bot runs on to level (grind_roam, which lets it through the capital's edge until then).
        st.leave_until_ms = now_ms + kLeaveMs;
        V2::TownLife::Counters().errands.fetch_add(1, std::memory_order_relaxed);
        EndErrand(s, st, true);
        return false;
    }
    if (there)
    {
        ArriveErrand(s, ai, emit, st, now_ms);
        ai.set_last_rule_fired("idle:town_errand");
        return true;
    }
    // One pathed move to the place (the navmesh route goes out as one spline: PlayerbotAPI MoveAlongRoute), re-sent only when the bot
    // has stopped short of it, so it runs through without a stop. The chunked walk is the fallback for a move the path call refused.
    if (!st.issued || (!s.is_moving() && now_ms - st.last_emit_ms >= 1500u))
    {
        if (!emit.move_to(st.tx, st.ty, st.tz, /*run*/ true) && !emit.move_already_emitted(st.tx, st.ty, st.tz))
            States::ChunkedWalkToward(s, ai, emit, st.tx, st.ty, st.tz, "idle:town_errand", now_ms);
        st.issued = true;
        st.last_emit_ms = now_ms;
    }
    ai.set_last_rule_fired("idle:town_errand");
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

// ---------- idle:gate_duel ----------
// A world bot the gate-duel driver (WorldBotSpawner::DriveGateDuels, Fleet/GateDuels.h) put in a pair walks to its spot outside the
// capital's gate. The challenger asks the other for a duel (spell 7266, the game's own request: the other accepts through the duel
// pact in BotAI, the flag and countdown are the game's); an onlooker just stands. The duel is fought by the usual combat code.
bool GateDuelGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (!s.is_alive() || s.in_combat() || s.is_casting() || s.has_owner_character() || s.in_group()) return false;
    if (s.is_in_instance() || s.is_in_dungeon() || s.in_battleground() || s.raw().movement.is_mounted) return false;
    if (s.raw().social_events.has_duel_request || s.duel_in_progress()) return false;
    V2::GateDuels::Assignment a;
    return V2::GateDuels::Board::Get().Find(s.bot_id(), a);
}

bool GateDuelFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    V2::GateDuels::Assignment a;
    if (!V2::GateDuels::Board::Get().Find(s.bot_id(), a)) return false;
    float bx, by, bz;
    s.position(bx, by, bz);
    uint32 const now = s.published_at_ms();
    if (std::hypot(a.x - bx, a.y - by) > 3.0f)
    {
        // The game's own navmesh route to the spot, one leg at a time (a leg ends where the path is cut; the next tick, with the bot standing,
        // routes the rest from there). The straight-line chunk walk is the fallback: it gives up on a goal it cannot step toward, which left
        // pairs standing in the city and then timing out (117: Sinan 252 yd away, moving=false for the whole 120 s).
        if (s.is_moving() || emit.move_to(a.x, a.y, a.z) || emit.move_already_emitted(a.x, a.y, a.z)
            || States::ChunkedWalkToward(s, ai, emit, a.x, a.y, a.z, "idle:gate_duel", now))
        {
            ai.set_last_rule_fired("idle:gate_duel");
            return true;
        }
    }
    if (a.role == V2::GateDuels::Role::Challenger && !s.is_moving())
    {
        // Ask once the pair stands together; at most every 8 s, so a pending request is not asked again.
        if (ai.last_town_show_ms() && now - ai.last_town_show_ms() < 8000u) { ai.set_last_rule_fired("idle:gate_duel"); return true; }
        for (auto const& u : s.nearby_friends())
        {
            if (u.guid.GetCounter() != a.partner || !u.is_player || u.hp <= 0) continue;
            float const d = std::hypot(u.x - bx, u.y - by);
            if (d < 2.0f || d > 12.0f) continue;
            V2::TownLife::PactSet(a.partner, s.bot_id(), now);
            emit.cast(7266u, u.guid);   // Duel
            ai.set_last_town_show_ms(now);
            break;
        }
    }
    else if (a.role == V2::GateDuels::Role::Onlooker && !s.is_moving()
             && (!ai.last_town_show_ms() || now - ai.last_town_show_ms() > 15000u) && urand(0, 99) < 30)
    {
        // An onlooker cheers, claps or laughs now and then.
        static constexpr uint32 kCheer[] = { 4u, 21u, 11u };   // cheer, applaud, laugh
        emit.perform_emote(kCheer[urand(0, 2)]);
        ai.set_last_town_show_ms(now);
    }
    ai.set_last_rule_fired("idle:gate_duel");
    return true;
}

// ---------- idle:gathering ----------
// A world bot the gathering driver (WorldBotSpawner::DriveGatherings, Fleet/Gathering.h) gave a seat outside a capital's gate walks to it
// and stays there; the driver turns it to the middle, sits it down at the fire and runs the talk. Held until the driver lets it go.
bool GatheringGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (!s.is_alive() || s.in_combat() || s.is_casting() || s.has_owner_character() || s.in_group()) return false;
    if (s.is_in_instance() || s.is_in_dungeon() || s.in_battleground() || s.raw().movement.is_mounted) return false;
    if (s.raw().social_events.has_duel_request || s.duel_in_progress()) return false;
    return V2::Gathering::Board::Get().Has(s.bot_id());
}

bool GatheringFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    V2::Gathering::Spot a;
    if (!V2::Gathering::Board::Get().Find(s.bot_id(), a)) return false;
    float bx, by, bz;
    s.position(bx, by, bz);
    uint32 const now = s.published_at_ms();
    if (std::hypot(a.x - bx, a.y - by) > 2.0f)
    {
        if (s.is_moving() || emit.move_to(a.x, a.y, a.z) || emit.move_already_emitted(a.x, a.y, a.z)
            || States::ChunkedWalkToward(s, ai, emit, a.x, a.y, a.z, "idle:gathering", now))
        {
            ai.set_last_rule_fired("idle:gathering");
            return true;
        }
    }
    ai.set_last_rule_fired("idle:gathering");
    return true;
}

// ---------- idle:gate_challenge ----------
// Near a capital's gate a bot now and then challenges a real player of about its level to a duel (Playerbot.World.GateDuels.
// ChallengePlayers). A player is asked at most once every ChallengeCooldownSec, whoever asks; the player may say no.
bool GateChallengeGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    auto const& cfg = Services::Config();
    if (!cfg.gate_duels_enable() || !cfg.gate_duels_challenge()) return false;
    if (!s.is_alive() || s.in_combat() || s.is_casting() || s.has_owner_character() || s.in_group() || s.is_moving()) return false;
    if (s.is_in_instance() || s.is_in_dungeon() || s.in_battleground() || s.raw().movement.is_mounted) return false;
    if (s.hp_pct() < 90 || s.level() < 6 || s.enemies_within(40.0f) > 0) return false;
    if (s.raw().social_events.has_duel_request || s.duel_in_progress()) return false;
    if (!V2::TownLife::TakesPart(s.bot_id(), 6u, s.published_at_ms(), 5u, 60u * 1000u)) return false;
    float bx, by, bz;
    s.position(bx, by, bz);
    return V2::GateDuels::Board::Get().NearGate(s.map_id(), bx, by, cfg.gate_duels_radius(), s.is_alliance());
}

bool GateChallengeFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    float bx, by, bz;
    s.position(bx, by, bz);
    for (auto const& u : s.nearby_friends())
    {
        if (!u.is_player || u.hp <= 0) continue;
        uint64 const id = u.guid.GetCounter();
        if (Services::Registry().has(BotId(id))) continue;   // a real player only
        float const d = std::hypot(u.x - bx, u.y - by);
        if (d < 4.0f || d > 20.0f || !V2::GateDuels::PlayerDuelOk(s.level(), u.level)) continue;
        if (!V2::GateDuels::Board::Get().ChallengeTake(id, s.published_at_ms(), Services::Config().gate_duels_challenge_cd_ms())) return false;
        emit.cast(7266u, u.guid);   // Duel
        V2::TownLife::Counters().gate_challenges.fetch_add(1, std::memory_order_relaxed);
        ai.set_last_rule_fired("idle:gate_challenge");
        return true;
    }
    return false;
}

// ---------- idle:grind_roam ----------
// Playerbot.World.Questing = 0: a bot of the world levels by killing, so with no mob of its level in reach it walks to the nearest place
// where such mobs stand (World::FindGrindSpotNear; the mobs themselves are taken by the usual engage rule, which runs below this one).
// Within 30 yd of the spot, or with a mob to fight in reach, it yields: the local wander and the engage rule do the rest.
bool GrindRoamGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32 now_ms)
{
    if (Services::Config().world_questing() || !s.is_free_world_bot()) return false;
    if (!s.is_alive() || s.in_combat() || s.is_casting() || s.hp_pct() < 60) return false;
    if (s.is_in_instance() || s.is_in_dungeon() || s.in_battleground() || s.raw().movement.is_mounted) return false;
    // A capital is no grinding ground, except for a bot idle:town_errand has just walked out to the gate (the gate spot can lie in the
    // capital's zone, the Gates of Stormwind do).
    if (V2::World::IsVanillaCapitalZone(s.zone_id()) && int32(ai.town_errand().leave_until_ms - now_ms) <= 0) return false;
    if (V2::Parties::Following(s)) return false;   // in a small party: the leader picks the spot (idle:party_follow)
    uint8 const lvl = s.level();
    for (auto const& u : s.nearby_enemies())
    {
        if (!u.guid.IsCreature() || u.no_xp_kill || u.is_pacified || u.untargetable) continue;
        if (u.level > lvl + 4u || u.hp_pct() < 50 || u.victim != ObjectGuid::Empty) continue;
        if (V2::ShareMobs::LeaveMob(s, u)) continue;   // a real player's: not ours to fight
        if (V2::ShareMobs::LeaveElite(s, u)) continue;   // elites and rares are left to players
        return false;   // something to fight in reach: the engage rule takes it
    }
    return true;
}

bool GrindRoamFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    float bx, by, bz;
    s.position(bx, by, bz);
    float gx = 0.f, gy = 0.f, gz = 0.f;
    // A spot a real player is working is skipped for the next one (Playerbot.World.ShareMobs).
    uint64 const seed = s.guid().GetCounter();
    bool found = false;
    for (uint32 attempt = 0; attempt < 4 && !found; ++attempt)
    {
        if (!V2::World::FindGrindSpotNear(s.is_alliance(), s.map_id(), bx, by, s.level(), seed + attempt * 7919u, 700.0f, gx, gy, gz))
            return false;
        found = !V2::ShareMobs::SpotTaken(s, gx, gy);
    }
    if (!found)
        return false;
    if (std::hypot(gx - bx, gy - by) <= 30.0f)
        return false;
    uint32 const now = s.published_at_ms();
    if (s.is_moving() || emit.move_to(gx, gy, gz) || emit.move_already_emitted(gx, gy, gz)
        || States::ChunkedWalkToward(s, ai, emit, gx, gy, gz, "idle:grind_roam", now))
    {
        ai.set_last_rule_fired("idle:grind_roam");
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

// ---------- idle:kind_buff / idle:kind_heal / idle:kind_help / idle:kind_rez ----------
// Small kindnesses of a world bot for a real player it passes (Playerbot.World.Kindness, Fleet/StrangerBoard.h): a priest's
// Fortitude, a mage's Intellect or a druid's Mark on a player who lacks it, a heal from a priest, druid, paladin or shaman for a hurt
// player, a hand against the mob of a player losing the fight, a resurrection for a player lying dead (not released) next to it. Each is rare: one per player per kind per cooldown, whoever
// gives it, and the bot's own rule interval on top.
constexpr float kKindBuffRange = 28.0f;
constexpr float kKindHelpRange = 35.0f;
constexpr float kKindRezRange  = 28.0f;
constexpr float kKindHealRange = 35.0f;   // heals reach 40: slack for the board's one-second cadence
constexpr int32 kKindHealManaPct = 40;
constexpr uint32 kKindRezCooldownMs  = 120000u;

bool KindFree(BotSnapshotView const& s)
{
    if (!Services::Config().kindness_enable() || !s.is_free_world_bot()) return false;
    if (!s.is_alive() || s.is_ghost() || s.is_casting() || s.raw().movement.is_mounted) return false;
    if (s.is_in_instance() || s.in_battleground()) return false;
    return true;
}

uint32 KindRezSpell(uint8 cls)
{
    switch (cls)
    {
        case CLASS_PRIEST:  return 2006;   // Resurrection
        case CLASS_PALADIN: return 7328;   // Redemption
        case CLASS_SHAMAN:  return 2008;   // Ancestral Spirit
        default:            return 0;
    }
}

V2::Strangers::Stranger const* KindBuffTarget(BotSnapshotView const& s, std::vector<V2::Strangers::Stranger> const& list, uint32& spell)
{
    float bx, by, bz; s.position(bx, by, bz);
    // The buff the bot has to give, for a player who holds none of the bots' buffs (the pick then skips the ones he holds).
    uint8 bit = 0;
    spell = Classic::PickStrangerBuff(s, 0, true, bit);
    if (!spell) return nullptr;
    V2::Strangers::Stranger const* t = V2::Strangers::PickBuffTarget(list, s.map_id(), s.is_alliance(), s.group_guid().GetCounter(), bx, by, kKindBuffRange, bit);
    if (!t) return nullptr;
    spell = Classic::PickStrangerBuff(s, t->buffs, t->has_mana, bit);
    return spell ? t : nullptr;
}

bool KindBuffGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (!KindFree(s) || s.in_combat() || s.power_pct(POWER_MANA) < 40) return false;
    auto const list = V2::Strangers::Board::Get().Current();
    uint32 spell = 0;
    return list && !list->empty() && KindBuffTarget(s, *list, spell);
}

bool KindBuffFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    auto const list = V2::Strangers::Board::Get().Current();
    uint32 spell = 0;
    V2::Strangers::Stranger const* t = list ? KindBuffTarget(s, *list, spell) : nullptr;
    if (!t) return false;
    auto& board = V2::Strangers::Board::Get();
    if (!board.cooldowns.Take(t->guid, V2::Strangers::Kind::Buff, s.published_at_ms(), Services::Config().kindness_buff_cd_ms()))
    {
        board.cooldown_skips.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    ObjectGuid const target = ObjectGuid::Create<HighGuid::Player>(t->guid);
    emit.face_target(target);
    if (!emit.cast(spell, target))
    {
        board.cast_fails.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    board.buffs.fetch_add(1, std::memory_order_relaxed);
    TC_LOG_INFO("playerbot.v2", "[kindness] {} buffs a passing player (guid {}) with {}", s.name(), t->guid, spell);
    ai.set_last_rule_fired("idle:kind_buff");
    return true;
}

// The heal a passing healer-class bot gives (the out-of-combat top-up heal of its class, Combat/Apl/ApClassicHeal.h), 0 = none:
// not a healer class, in a druid form, short of mana or the spell not ready.
uint32 KindHealSpell(BotSnapshotView const& s)
{
    if (!Classic::IsSingleSpecClass(s.cls()) || s.power_pct(POWER_MANA) < kKindHealManaPct) return 0;
    if (s.cls() == CLASS_DRUID)
        for (uint32 form : { 768u, 5487u, 9634u, 783u, 1066u, 24858u })   // Cat, Bear, Dire Bear, Travel, Aquatic, Moonkin
            if (s.has_aura(form)) return 0;
    uint32 const spell = Combat::ClassicHeal::OocHealSpell(s);
    return spell && s.is_ready(spell) ? spell : 0;
}

V2::Strangers::Stranger const* KindHealTarget(BotSnapshotView const& s, std::vector<V2::Strangers::Stranger> const& list)
{
    uint32 const pct = Services::Config().kindness_heal_hp_pct();
    if (!pct) return nullptr;
    float bx, by, bz; s.position(bx, by, bz);
    return V2::Strangers::PickHealTarget(list, s.map_id(), s.is_alliance(), s.group_guid().GetCounter(), bx, by, kKindHealRange,
                                         uint8(pct), s.is_pvp());
}

bool KindHealGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (!KindFree(s) || s.in_combat() || !KindHealSpell(s)) return false;
    auto const list = V2::Strangers::Board::Get().Current();
    return list && !list->empty() && KindHealTarget(s, *list);
}

bool KindHealFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    uint32 const spell = KindHealSpell(s);
    auto const list = V2::Strangers::Board::Get().Current();
    V2::Strangers::Stranger const* t = spell && list ? KindHealTarget(s, *list) : nullptr;
    if (!t) return false;
    // The rank for what he is missing, one the bot can pay (a stranger at 55% gets a small heal, not the top rank).
    float const missing = float(t->max_hp > t->hp ? t->max_hp - t->hp : 0u);
    uint32 const rank = Combat::ClassicHeal::AffordableRank(s, spell, Combat::ClassicHeal::PlanRank(s, spell, missing, true).rank);
    if (!rank) return false;
    auto& board = V2::Strangers::Board::Get();
    if (!board.cooldowns.Take(t->guid, V2::Strangers::Kind::Heal, s.published_at_ms(), Services::Config().kindness_heal_cd_ms()))
    {
        board.cooldown_skips.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    ObjectGuid const target = ObjectGuid::Create<HighGuid::Player>(t->guid);
    // A heal with a cast time is refused while the bot walks: it stops for it, as a player would, and walks on after.
    if (s.is_moving())
        emit.stop_movement(true);
    emit.face_target(target);
    if (!emit.cast_rank(rank, target))
    {
        board.cast_fails.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (urand(0, 3) == 0)
    {
        static constexpr char const* kLines[] = { "there you go", "patched you up", "heal inc", "careful out there", "you looked like you needed that" };
        emit.say_world(kLines[urand(0, uint32(std::size(kLines)) - 1)]);
    }
    board.heals.fetch_add(1, std::memory_order_relaxed);
    TC_LOG_INFO("playerbot.v2", "[kindness] {} heals a passing player (guid {}) at {}% with {}", s.name(), t->guid, t->hp_pct, rank);
    ai.set_last_rule_fired("idle:kind_heal");
    return true;
}

bool KindHelpGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (!KindFree(s) || s.in_combat() || s.hp_pct() < 60) return false;
    if (Services::Config().kindness_help_hp_pct() == 0) return false;   // HelpHpPct = 0: no help
    auto const list = V2::Strangers::Board::Get().Current();
    if (!list || list->empty()) return false;
    float bx, by, bz; s.position(bx, by, bz);
    return V2::Strangers::PickHelpTarget(*list, s.map_id(), s.is_alliance(), s.group_guid().GetCounter(), bx, by, kKindHelpRange,
                                         uint8(Services::Config().kindness_help_hp_pct()), uint8(s.level()), 3) != nullptr;
}

bool KindHelpFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    auto const list = V2::Strangers::Board::Get().Current();
    if (!list) return false;
    float bx, by, bz; s.position(bx, by, bz);
    V2::Strangers::Stranger const* t = V2::Strangers::PickHelpTarget(*list, s.map_id(), s.is_alliance(), s.group_guid().GetCounter(), bx, by,
        kKindHelpRange, uint8(Services::Config().kindness_help_hp_pct()), uint8(s.level()), 3);
    if (!t) return false;
    // The mob on him, as the bot sees it.
    ObjectGuid const player = ObjectGuid::Create<HighGuid::Player>(t->guid);
    NearbyUnit const* mob = nullptr;
    for (auto const& u : s.nearby_enemies())
        if (u.guid.IsCreature() && u.victim == player && u.hp > 0 && !u.untargetable) { mob = &u; break; }
    if (!mob) return false;
    auto& board = V2::Strangers::Board::Get();
    if (!board.cooldowns.Take(t->guid, V2::Strangers::Kind::Help, s.published_at_ms(), Services::Config().kindness_help_cd_ms())) return false;
    if (!emit.start_attack(mob->guid)) return false;
    board.helps.fetch_add(1, std::memory_order_relaxed);
    TC_LOG_INFO("playerbot.v2", "[kindness] {} helps a player (guid {}) at {}% against {}", s.name(), t->guid, t->hp_pct, mob->entry);
    ai.set_last_rule_fired("idle:kind_help");
    return true;
}

bool KindRezGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (!KindFree(s) || s.in_combat()) return false;
    uint32 const rez = KindRezSpell(s.cls());
    if (!rez || !s.knows_spell(rez) || !s.is_ready(rez) || s.power_pct(POWER_MANA) < 30) return false;
    auto const list = V2::Strangers::Board::Get().Current();
    if (!list || list->empty()) return false;
    float bx, by, bz; s.position(bx, by, bz);
    return V2::Strangers::PickRezTarget(*list, s.map_id(), s.is_alliance(), s.group_guid().GetCounter(), bx, by, kKindRezRange) != nullptr;
}

bool KindRezFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    auto const list = V2::Strangers::Board::Get().Current();
    if (!list) return false;
    float bx, by, bz; s.position(bx, by, bz);
    V2::Strangers::Stranger const* t = V2::Strangers::PickRezTarget(*list, s.map_id(), s.is_alliance(), s.group_guid().GetCounter(), bx, by, kKindRezRange);
    if (!t) return false;
    auto& board = V2::Strangers::Board::Get();
    if (!board.cooldowns.Take(t->guid, V2::Strangers::Kind::Rez, s.published_at_ms(), kKindRezCooldownMs)) return false;
    ObjectGuid const target = ObjectGuid::Create<HighGuid::Player>(t->guid);
    emit.face_target(target);
    if (!emit.cast(KindRezSpell(s.cls()), target)) return false;
    static constexpr char const* kLines[] = { "rez inc", "hold on, rezzing you", "got you", "rez coming", "don't release, rezzing" };
    emit.say_world(kLines[urand(0, uint32(std::size(kLines)) - 1)]);
    board.rezzes.fetch_add(1, std::memory_order_relaxed);
    TC_LOG_INFO("playerbot.v2", "[kindness] {} resurrects a player (guid {})", s.name(), t->guid);
    ai.set_last_rule_fired("idle:kind_rez");
    return true;
}

// ---------- idle:party_assist / idle:party_follow ----------
// Small parties of world bots (Playerbot.World.Parties, Fleet/BotParties.h): a member joins the fight of a mate within 45 yd, and a
// follower more than 15 yd behind its leader catches up to its place beside it. The leader pulls and picks the grind spot (the
// follower's grind_roam and engage stand down, State_Idle.cpp engage_nearby_mob).
constexpr float kPartyAssistRange = 45.0f;
constexpr float kPartyKeepUp = 15.0f;

bool PartyFree(BotSnapshotView const& s)
{
    if (!s.is_alive() || s.is_ghost() || s.is_casting() || s.in_combat() || s.raw().movement.is_mounted) return false;
    return !s.is_in_instance() && !s.in_battleground();
}

NearbyUnit const* PartyAssistTarget(BotSnapshotView const& s, V2::Parties::Link const& l)
{
    float bx, by, bz; s.position(bx, by, bz);
    for (V2::Parties::Mate const& m : l.mates)
    {
        if (!m.in_combat || !m.target || std::hypot(m.x - bx, m.y - by) > kPartyAssistRange) continue;
        for (auto const& u : s.nearby_enemies())
            if (u.guid.IsCreature() && u.guid.GetCounter() == m.target && u.hp > 0 && !u.untargetable && !u.is_pacified)
                return &u;
    }
    return nullptr;
}

bool PartyAssistGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (!PartyFree(s) || s.hp_pct() < 40) return false;
    std::shared_ptr<std::unordered_map<uint64, V2::Parties::Link> const> keep;
    V2::Parties::Link const* l = V2::Parties::LinkOf(s, keep);
    return l && PartyAssistTarget(s, *l);
}

bool PartyAssistFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    std::shared_ptr<std::unordered_map<uint64, V2::Parties::Link> const> keep;
    V2::Parties::Link const* l = V2::Parties::LinkOf(s, keep);
    NearbyUnit const* mob = l ? PartyAssistTarget(s, *l) : nullptr;
    if (!mob || !emit.start_attack(mob->guid)) return false;
    V2::Parties::Board::Get().assists.fetch_add(1, std::memory_order_relaxed);
    ai.set_last_rule_fired("idle:party_assist");
    return true;
}

V2::Parties::Mate const* PartyLeader(V2::Parties::Link const& l)
{
    for (V2::Parties::Mate const& m : l.mates)
        if (m.guid == l.leader) return &m;
    return nullptr;
}

bool PartyFollowGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (!PartyFree(s)) return false;
    std::shared_ptr<std::unordered_map<uint64, V2::Parties::Link> const> keep;
    V2::Parties::Link const* l = V2::Parties::LinkOf(s, keep);
    if (!l || l->slot == 0) return false;
    V2::Parties::Mate const* lead = PartyLeader(*l);
    float bx, by, bz; s.position(bx, by, bz);
    return lead && std::hypot(lead->x - bx, lead->y - by) > kPartyKeepUp;
}

bool PartyFollowFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    std::shared_ptr<std::unordered_map<uint64, V2::Parties::Link> const> keep;
    V2::Parties::Link const* l = V2::Parties::LinkOf(s, keep);
    V2::Parties::Mate const* lead = l ? PartyLeader(*l) : nullptr;
    if (!lead) return false;
    // Its place beside the leader: 4 yd off, the first follower on one side, the second on the other.
    float const a = l->slot == 1 ? 2.4f : -2.4f;
    float const tx = lead->x + 4.0f * std::cos(a), ty = lead->y + 4.0f * std::sin(a), tz = lead->z;
    uint32 const now = s.published_at_ms();
    if (emit.move_to(tx, ty, tz) || emit.move_already_emitted(tx, ty, tz)
        || States::ChunkedWalkToward(s, ai, emit, tx, ty, tz, "idle:party_follow", now))
    {
        V2::Parties::Board::Get().catchups.fetch_add(1, std::memory_order_relaxed);
        ai.set_last_rule_fired("idle:party_follow");
        return true;
    }
    return false;
}

} // anonymous namespace

void RegisterAmbientRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:party_assist";
        rule.priority = 138;   // under the kindnesses (141-143)
        rule.gate     = &PartyAssistGate;
        rule.fire     = &PartyAssistFire;
        rule.min_interval_ms = 1000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:party_follow";
        rule.priority = 56;    // just above grind_roam (55), so a follower keeps up instead of picking its own spot
        rule.gate     = &PartyFollowGate;
        rule.fire     = &PartyFollowFire;
        rule.min_interval_ms = 1500;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:kind_rez";
        rule.priority = 143;   // above the town shows (inn rest, mailbox, duels: up to 140), below survival and upkeep
        rule.gate     = &KindRezGate;
        rule.fire     = &KindRezFire;
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:kind_heal";
        // Above the hand against his mob (142): a healer passing a player losing a fight heals him rather than hitting the mob.
        // Looked at every 2 s so a healer walking by (in reach for a few seconds) notices; the per-player cooldown keeps it rare.
        rule.priority = 144;
        rule.gate     = &KindHealGate;
        rule.fire     = &KindHealFire;
        rule.min_interval_ms = 2000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:kind_help";
        rule.priority = 142;
        rule.gate     = &KindHelpGate;
        rule.fire     = &KindHelpFire;
        rule.min_interval_ms = 3000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:kind_buff";
        // Above the town errands (133) and the town shows (inn rest 139, mailbox 138, gate duels 140): a capital resident is nearly
        // always on one of them and would never stop for a passing player. Looked at every 5 s, so a bot walking past a player
        // (in range for a few seconds) has a fair chance to see him; the per-player cooldown keeps it rare.
        rule.priority = 141;
        rule.gate     = &KindBuffGate;
        rule.fire     = &KindBuffFire;
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
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
        rule.name     = "idle:world_pvp_ambush";
        // Just under the PvP defence of a friend (705): helping comes first, then the hunt. A roll per enemy per RollMinutes, so a
        // look every second is enough.
        rule.priority = 704;
        rule.min_interval_ms = 1000;
        rule.gate     = &WorldPvpAmbushGate;
        rule.fire     = &WorldPvpAmbushFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:world_pvp_flag";
        rule.priority = 140;
        rule.min_interval_ms = 30000;
        rule.gate     = &WorldPvpFlagGate;
        rule.fire     = &WorldPvpFlagFire;
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
        // Below the shows (136-140) and the AH trip (135), above the city trip (130), the rpg visits (110) and the wander (autoact, 50): a
        // capital resident with nothing else to do runs its errands.
        IdleRule rule;
        rule.name     = "idle:town_errand";
        rule.priority = 133;
        rule.gate     = &TownErrandGate;
        rule.fire     = &TownErrandFire;
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
        rule.name     = "idle:gate_duel";
        rule.priority = 140;   // above the other town shows: the driver chose this bot and spot
        rule.gate     = &GateDuelGate;
        rule.fire     = &GateDuelFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:gathering";
        rule.priority = 139;   // just under the gate duels (140): the driver chose this bot and seat
        rule.gate     = &GatheringGate;
        rule.fire     = &GatheringFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:grind_roam";
        rule.priority = 55;    // just above the autoact cascade (50), which fights what is in reach and wanders
        rule.gate     = &GrindRoamGate;
        rule.fire     = &GrindRoamFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:gate_challenge";
        rule.priority = 134;
        rule.gate     = &GateChallengeGate;
        rule.fire     = &GateChallengeFire;
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
