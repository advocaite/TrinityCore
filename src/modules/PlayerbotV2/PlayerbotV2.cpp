// Playerbot V2 - Module entry implementation
// All TrinityCore hook surfaces from PlayerbotV2.h are wired here. Some carry
// dispatch (login/logout register the bot; OnDeath taps loot; DamageTaken /
// HealReceived push event-bus entries; Whisper routes through the command
// parser). Hooks marked "Intentionally a no-op" rely on snapshot deltas
// instead of event pushes â€” see per-handler comments for the rationale.

#include "Util/SlowCall.h"
#include "PlayerbotV2.h"
#include "Services.h"
#include "Fleet/RaidSubgroupPlan.h"
#include "Bot/BotRegistry.h"
#include "Bot/BotSnapshot.h"
#include "Bot/World/LearnFlightpaths.h"
#include "Bot/BotSnapshotBuilder.h"
#include "Bot/QuestReverseIndex.h"
#include "Bot/QuestObjectiveLocations.h"
#include "Bot/World/NavmeshPrewarm.h"
#include "Bot/World/ZonePicker.h"
#include "PlayerbotMovement.h"
#include "Session/BotSessionMgr.h"
#include "Fleet/BotCharacterFactory.h"
#include "Fleet/BotNamePool.h"
#include "Fleet/BotPopulationManager.h"
#include "Fleet/BotGuildMgr.h"
#include "Fleet/BotCoordinationBus.h"
#include "Fleet/CraftOrderBoard.h"
#include "Fleet/PlayerCraftOrders.h"
#include "Fleet/BotQueueFiller.h"
#include "Fleet/JunkQuestResolver.h"
#include "Bot/ClassTables.h"   // MaxPlayerLevel()
#include "Bot/Battleground/BgTeamCoordinator.h"
#include "Bot/Dungeon/PveGroupCoordinator.h"
#include "Bot/Raid/BossWarnings.h"
#include "Bot/Raid/RaidBrain.h"
#include "DB2Stores.h"
#include "World.h"
#include "WorldSession.h"
#include "DungeonFinding/LFGMgr.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "PhasingHandler.h"
#include "SpellAuraDefines.h"
#include "Unit.h"
#include "Bot/BotPersonality.h"
#include "Bot/BotArchetype.h"
#include "Bot/BotRng.h"
#include "Bot/BotCommandParser.h"
#include "Bot/BotChatReactor.h"
#include "Bot/GroupOrderText.h"
#include "Bot/AddonHint.h"
#include "Bot/BotTextMgr.h"
#include "Chat.h"
#include "Bot/BotIntent.h"
#include "Threading/IntentQueue.h"
#include "Creature.h"
#include "GameTime.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Group/GroupSnapshotBuilder.h"
#include "Group.h"
#include "GitRevision.h"
#include <filesystem>
#include <fstream>
#include "GroupReference.h"
#include "GroupMgr.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "Pet.h"
#include "Fleet/PetSnap.h"
#include "WorldSession.h"
#include "Bot/BotCommandParser.h"
#include "Threading/SnapshotPublisher.h"
#include "Threading/TickScheduler.h"
#include "Threading/SnapshotBuildPool.h"
#include "Persistence/PlayerbotMigrationMgr.h"
#include "Diagnostics/PerfCounters.h"
#include "Diagnostics/DamageParity.h"
#include "Diagnostics/ScenarioRunner.h"
#include "Util/ConfigReader.h"
#include "World/WorldMetadata.h"
#include "Fleet/BotIdentityRegistry.h"
#include "Fleet/OwnerRegistry.h"
#include "Bot/BotAI.h"
#include "Bot/DeathHotspots.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/CastPacing.h"
#include "PlayerbotAPI.h"
#include "Fleet/BotCharacterFactory.h"
#include "Fleet/BotComposition.h"
#include "Combat/ApRegistry.h"
#include "Player.h"
#include "Spell.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "Timer.h"
#include "GameTime.h"     // GameTime::GetGameTimeMS for FleetStatus stuck-bot check
#include "DatabaseEnv.h"  // CharacterDatabase — R7 leveling-target hydration + #1C vitals row
#include "fmt/format.h"   // #1C fleet-vitals async row INSERT formatting
#include "Config.h"       // sConfigMgr â€” V1-active coexistence warning at boot
#include "WorldSession.h" // HandleMoveTeleportAck / HandleMoveWorldportAck for headless bots
#include "WorldPacket.h"
#include "MovementPackets.h"
#include "Opcodes.h"
#include "MotionMaster.h"
#include "PlayerbotMovement.h"  // BotMovement::SafeTeleport for rescue paths
#include "ObjectMgr.h"
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>
#include "Bot/DeathCause.h"

#if !TRINITY_PLAYERBOT_V2
#error "PlayerbotV2 module compiled without TRINITY_PLAYERBOT_V2 defined; CMake misconfiguration"
#endif

namespace Playerbot {
// Defined in Bot/BotSnapshotResetCheck.cpp. Aborts at boot if
// BotSnapshot::reset_for_reuse() leaves any container/field non-default —
// guards the recycle pool (SNAPSHOT_PERF_BACKLOG Tier 3.1) against drift.
bool VerifyResetClearsAll();
}

namespace Playerbot::V2 {

// Players still to be told about the add-on (world thread only: the login hook, the add-on's hello and the world update all run there).
static AddonHintTracker g_addon_hints;

namespace {

// Coarse spatial bucket key for the per-tick real-player proximity set.
// Quantises a world position to a ~106 yd grid cell (well above the
// classifier's "near a real player" intent of ≤~100 yd) and packs it with
// map_id into a single uint64. Building one such set per tick (over the
// handful of connected real players) lets the tier classifier answer
// "is this bot near a real player?" with an O(1) set lookup instead of a
// per-bot grid search. To keep the boundary honest (two entities ≤100 yd
// apart can straddle a cell edge), each real player stamps its own cell
// AND the 8 neighbouring cells; a bot is then "near" when its single cell
// is occupied. That guarantees any bot within one cell (~106 yd) of a real
// player matches, with no per-bot radius test — the cost (9 inserts per
// real player) is trivial since real players are few.
constexpr float kRealPlayerCellSize = 106.0f;   // ~1.5× SIZE_OF_GRID-cell granularity, > 100y intent
inline uint64 PackPlayerCellKey(uint32 mapId, int32 cx, int32 cy)
{
    // 16 bits map | 24 bits cx | 24 bits cy (cells, biased to unsigned).
    // World coords span ±17066 → /106 ≈ ±161 cells, far inside 24 bits.
    uint64 const ux = uint64(uint32(cx + (1 << 23))) & 0xFFFFFF;
    uint64 const uy = uint64(uint32(cy + (1 << 23))) & 0xFFFFFF;
    return (uint64(mapId) << 48) | (ux << 24) | uy;
}
inline int32 PlayerCellCoord(float v) { return int32(std::floor(v / kRealPlayerCellSize)); }

// Drives any pending teleport ack on a headless bot. Real clients send
// CMSG_MOVE_TELEPORT_ACK / CMSG_MOVE_WORLDPORT_ACK to finalize teleports;
// without those acks the player sits in WaitingForTeleportAck forever and
// Player::TeleportTo's effect is silently dropped (the m_position never
// advances to m_teleport_dest). The most visible symptom is the corpse-run
// flow: Player::RepopAtGraveyard issues TeleportTo, the bot stays at the
// death location instead of the graveyard, then ResurrectPlayer revives in
// place. We close the loop by manufacturing the ack on the world thread
// â€” same call the network handler would make on a real packet.
void DriveTeleportAck(Player* p)
{
    if (!p) return;
    WorldSession* sess = p->GetSession();
    if (!sess) return;

    // Selfbot guard: only manufacture acks for HEADLESS sessions. A bot
    // driven through a REAL client (`.playerbot self`) performs the genuine
    // teleport handshake — forging the worldport ack advances the server's
    // packet sequence while the client is still loading, desyncing it
    // permanently (live 2026-06-11: user's selfbot LFG-ported into Ragefire
    // Chasm, server completed the transfer, client stuck on the loading
    // screen forever).
    if (!Services::SessionMgr().IsHeadless(p->GetGUID()))
        return;

    // Far teleport: fully synchronous server-side. The intermediate
    // SuspendTokenResponse step is a network-only round-trip; we can
    // jump straight to the worldport ack which performs the map-change
    // and packet sequence.
    if (p->GetTeleportState() == TeleportState::WaitingForWorldPortAck ||
        p->GetTeleportState() == TeleportState::WaitingForSuspendTokenResponse)
    {
        sess->HandleMoveWorldportAck();
        return;
    }

    // Near teleport (same map). HandleMoveTeleportAck only validates
    // MoverGUID + the WaitingForTeleportAck state; AckIndex / MoveTime
    // are unused beyond the debug log. Forge the minimum payload.
    if (p->GetTeleportState() == TeleportState::WaitingForTeleportAck)
    {
        WorldPacket data(CMSG_MOVE_TELEPORT_ACK, 8 + 4 + 4);
        data << p->GetGUID();
        data << int32(0);
        data << int32(GameTime::GetGameTimeMS());
        WorldPackets::Movement::MoveTeleportAck ackPacket(std::move(data));
        ackPacket.Read();
        sess->HandleMoveTeleportAck(ackPacket);
    }
}

// Recover Z when the bot has fallen below the world. Conservative on
// purpose: we only act when the bot is more than kBelowGroundCutoff
// yards under the navmesh ground level, which catches "popped below
// map" and severe underground-feet glitches but ignores cosmetic
// 1-2y drift. Snapping aggressively (small threshold, both
// directions) caused visible twitching: a moving bot finishes its
// spline â†’ Idle generator â†’ we pull Z down a fraction â†’ next tick
// the bot logic emits move_to and the spline starts again, repeat. Real
// players get small Z corrections via client-side ground snap and
// MSG_MOVE_HEARTBEAT updates; we approximate the catastrophic case
// only.
constexpr float kBelowGroundCutoff = 5.0f;

// High-Z stuck rescue threshold. Bots whose stored position landed them
// far ABOVE the actual ground (typical case: pre-fix ZonePicker data
// row with a wrong Z value baked into characters.position_z) sit forever
// at e.g. (1129, -4828, 205) on Northrend while real ground is Z~39.
// Path queries fail (no nearby poly) and the bot ends up an idle
// scarecrow. We only act when the delta exceeds kHighAboveCutoff so
// legitimate above-floor positions (rooftops, mountain ledges, multi-
// story buildings — typically ≤30y above local ground) aren't yanked
// down. Idle/not-flying/not-falling/not-transport gates above still
// apply, so legitimate flight is never disturbed.
constexpr float kHighAboveCutoff = 100.0f;

// Rescue bots whose saved position landed them in an orphaned BG / arena
// instance (the prior server crashed mid-BG before SaveToDB returned them
// to their entry point). Symptoms: bot is in a BG map but has no queue
// invite, no team set, no encounter â€” purely a ghost spawn. The BG init
// script still spawns flags/doors â†’ BIH churn â†’ SnapToGroundIfDrifted
// can crash (see project_v2_bih_crash.md). Teleport to entry point if
// known, else to Stormwind/Orgrimmar by faction.
//
// Fires at most once per bot per server uptime â€” once they're out of the
// BG map they stay out. Edge-triggered via Player m_bgData state: if the
// bot is in a BG map but Player::InBattleground() is false (no proper
// queue/instance association), they're orphaned.
void RescueOrphanedBgBot(Player* p)
{
    if (!p) return;
    Map const* m = p->FindMap();
    if (!m) return;
    if (!m->IsBattlegroundOrArena()) return;
    if (p->InBattleground()) return;             // legitimately in BG
    if (p->IsBeingTeleported()) return;          // mid-teleport, leave alone
    // Try entry point first â€” the pre-BG location saved into m_bgData.joinPos.
    WorldLocation entry = p->GetBattlegroundEntryPoint();
    if (entry.GetMapId() != 0 && entry.GetMapId() != MAPID_INVALID
        && entry.GetMapId() != m->GetId())
    {
        TC_LOG_WARN("playerbot.v2",
            "[RescueOrphanedBgBot] {} stuck in BG map {}; teleporting to entry point",
            p->GetName(), m->GetId());
        Playerbot::BotMovement::SafeTeleport(p, entry, /*options*/ 0);
        return;
    }
    // No valid entry point â€” fall back to faction capital.
    const bool alliance = (p->GetTeam() == ALLIANCE);
    WorldLocation safe = alliance
        ? WorldLocation(/*EasternKingdoms*/ 0, Position(-8443.0f,  335.0f, 121.0f, 0.f))   // Stormwind Keep front
        : WorldLocation(/*Kalimdor*/         1, Position( 1924.0f, -4147.0f,  40.0f, 0.f)); // Orgrimmar Grommash Hold
    TC_LOG_WARN("playerbot.v2",
        "[RescueOrphanedBgBot] {} stuck in BG map {} with no entry point; teleporting to capital",
        p->GetName(), m->GetId());
    Playerbot::BotMovement::SafeTeleport(p, safe, /*options*/ 0);
}

// Per-bot last-attempted-snap timestamp. Throttles the per-tick BIH
// height query that happens for every idle bot every world tick — at
// 2000 bots × 50Hz = 100K UpdateAllowedPositionZ calls/sec which fan
// out into Map::GetHeight + DynamicMapTree probes. Parked idle bots
// don't move, so once-every-2-seconds Z-snap is more than enough; an
// actively-moving bot is filtered out earlier by isMoving() so the
// throttle never trips for them. Keyed by Player GUID counter.
constexpr uint32 kSnapZIntervalMs = 2000;
// Per-bot 2s throttle for SnapToGroundIfDrifted. Single-threaded
// access — called only from Module::OnWorldUpdate's reg.for_each loop
// on the world thread, so no synchronization is needed. (Pre-2026-05-21
// this was guarded by std::shared_mutex; the mutex was unused because
// the only caller is single-threaded, costing ~2000 lock/unlock per
// tick at fleet scale for nothing.)
std::unordered_map<uint64, uint32> g_last_snap_z_ms;

void SnapToGroundIfDrifted(Player* p)
{
    if (!p || !p->IsAlive()) return;
    // Selfbot guard: a real client's Z is authoritative from its own
    // movement packets — server-side relocation rubber-bands the player.
    // Only headless sessions need the drift rescue.
    if (!Services::SessionMgr().IsHeadless(p->GetGUID())) return;
    if (p->IsBeingTeleported()) return;
    if (p->isMoving()) return;
    if (p->GetMotionMaster()->GetCurrentMovementGeneratorType() != IDLE_MOTION_TYPE)
        return;
    if (p->GetTransport() || p->GetVehicle()) return;
    if (p->IsFlying() || p->IsFalling()) return;   // legitimate Z != ground

    // Per-bot 2s throttle. Single-threaded (world-tick ForEachBot
    // caller), no mutex required.
    const uint64 key = p->GetGUID().GetCounter();
    const uint32 now = getMSTime();
    {
        auto it = g_last_snap_z_ms.find(key);
        if (it != g_last_snap_z_ms.end() && getMSTimeDiff(it->second, now) < kSnapZIntervalMs)
            return;
    }
    g_last_snap_z_ms[key] = now;
    // Skip ground-snap on BG maps. BG instances cycle dynamic GameObjects
    // (flags, doors, capture-point gobs) rapidly during prep/start/end,
    // and DynamicMapTree::getHeight occasionally hits a "invalid node
    // overlap" exception when the BIH is mid-rebuild from concurrent
    // GO add/remove. A crashing 1052-bot fleet doesn't deserve a Z snap;
    // BG terrain is well-curated so drift is rare anyway.
    if (p->InBattleground()) return;

    const float curZ = p->GetPositionZ();
    float groundZ = curZ;
    // Defensive: DynamicMapTree::getHeight can throw std::logic_error
    // ("invalid node overlap") from the BIH balance path when collision
    // data is mid-rebuild. Catching here keeps a single bot's Z-snap
    // from taking down the world thread. Crash 2026-05-13 05:59 had
    // this exception escape into World::Update.
    try
    {
        p->UpdateAllowedPositionZ(p->GetPositionX(), p->GetPositionY(), groundZ);
    }
    catch (std::exception const& e)
    {
        TC_LOG_WARN("playerbot.v2",
            "[SnapToGroundIfDrifted] {} caught {} during UpdateAllowedPositionZ; skipping",
            p->GetName(), e.what());
        return;
    }
    catch (...)
    {
        TC_LOG_WARN("playerbot.v2",
            "[SnapToGroundIfDrifted] {} unknown exception in UpdateAllowedPositionZ; skipping",
            p->GetName());
        return;
    }
    // Rescue from below-ground glitches.
    if (curZ < groundZ - kBelowGroundCutoff)
    {
        p->NearTeleportTo(p->GetPositionX(), p->GetPositionY(), groundZ, p->GetOrientation());
        return;
    }
    // Rescue from far-above-ground stuck positions (stale stored Z from
    // pre-fix ZonePicker data, off-mesh spawns on top of a sealed building,
    // etc.). All the idle/not-flying/not-falling/not-transport gates above
    // ensure we never disturb a legitimately-airborne bot. The 100y delta
    // avoids snapping bots off rooftops, towers, or mountain platforms.
    if (curZ > groundZ + kHighAboveCutoff)
    {
        TC_LOG_WARN("playerbot.v2",
            "[SnapToGroundIfDrifted] {} stuck high (Z={:.1f} vs ground={:.1f}); teleporting down",
            p->GetName(), curZ, groundZ);
        p->NearTeleportTo(p->GetPositionX(), p->GetPositionY(), groundZ, p->GetOrientation());
    }
}

// Global off-mesh stuck rescue. Per-rule wedge guards (Pass 14 on
// idle:wander, plus existing ones on travel/hub/portal/dock rules) only
// fire when the rule itself sees the failures — but higher-priority
// emergency rules (idle:watchdog_escape, idle:flee_hazard,
// dead:walking_to_corpse) grab the tick first and never escalate.
// Bot 88584 on 2026-05-18 logged idle:watchdog_escape at blocks=8,10,12
// with no rescue. Result: 1500+ wasted move_to/sec from a handful of
// off-mesh bots.
//
// This catches the symptom at the bot level regardless of which rule
// emitted: count path failures in a 30s window, and after a threshold,
// teleport to homebind. Homebind innkeepers always sit on navmesh-valid
// polys, so the bot lands somewhere it can pathfind from. Cooldown
// between rescues prevents teleport-spam if homebind itself is somehow
// bad (rare but theoretically possible if owner moved homebind to a
// broken spawn point).
struct GlobalStuckState
{
    uint32 baseline_blocks = 0;
    uint32 baseline_ms     = 0;
    uint32 last_rescue_ms  = 0;
};
// When the same bot needs rescue again within this window, its homebind
// is probably itself unreachable (observed 2026-05-18: bot Marinon
// rescued twice to map=2081 1621,536,Z=201.5 — Forbidden Reach Dracthyr
// starter, a flying-only zone with sparse navmesh). Escalate the second
// rescue to faction capital, which sits on dense navmesh-valid plaza.
constexpr uint32 kRescueRepeatWindowMs = 10u * 60u * 1000u;     // 10 min
// Single-threaded access — GlobalStuckRescue is only called from
// Module::OnWorldUpdate's reg.for_each loop on the world thread.
// No synchronization needed. (Pre-2026-05-21 was guarded by
// std::shared_mutex; the mutex was unused because the only caller
// path is single-threaded.)
std::unordered_map<uint64, GlobalStuckState> g_global_stuck;

constexpr uint32 kStuckThreshold      = 40;        // blocks in window (raised from 15 — favour path retries over rescue)
constexpr uint32 kStuckWindowMs       = 60u * 1000u; // 60 s (raised from 30 s)
constexpr uint32 kStuckRescueCooldown = 300u * 1000u; // 5 min (raised from 120 s)

struct CapitalInnkeeperCache
{
    bool  loaded = false;
    bool  alliance_valid = false;
    float alliance_x = 0.f, alliance_y = 0.f, alliance_z = 0.f;
    bool  horde_valid = false;
    float horde_x = 0.f, horde_y = 0.f, horde_z = 0.f;
};
static CapitalInnkeeperCache s_inn_cache;
static std::once_flag s_inn_cache_once;

WorldLocation FindCapitalRescuePos(bool alliance)
{
    std::call_once(s_inn_cache_once, []()
    {
        constexpr float kSW_cx = -8850.f, kSW_cy = 660.f;
        constexpr float kOR_cx =  1900.f, kOR_cy = -4300.f;
        float best_sw_dsq = 600.f * 600.f;
        float best_or_dsq = 600.f * 600.f;
        auto const& all = sObjectMgr->GetAllCreatureData();
        for (auto const& [spawn_id, cd] : all)
        {
            if (cd.mapId != 0 && cd.mapId != 1) continue;
            auto const* ct = sObjectMgr->GetCreatureTemplate(cd.id);
            if (!ct) continue;
            if (!(ct->npcflag & UNIT_NPC_FLAG_INNKEEPER)) continue;
            if (cd.mapId == 0)
            {
                const float dx = cd.spawnPoint.GetPositionX() - kSW_cx;
                const float dy = cd.spawnPoint.GetPositionY() - kSW_cy;
                const float dsq = dx*dx + dy*dy;
                if (dsq < best_sw_dsq)
                {
                    best_sw_dsq = dsq;
                    s_inn_cache.alliance_x = cd.spawnPoint.GetPositionX();
                    s_inn_cache.alliance_y = cd.spawnPoint.GetPositionY();
                    s_inn_cache.alliance_z = cd.spawnPoint.GetPositionZ();
                    s_inn_cache.alliance_valid = true;
                }
            }
            else if (cd.mapId == 1)
            {
                const float dx = cd.spawnPoint.GetPositionX() - kOR_cx;
                const float dy = cd.spawnPoint.GetPositionY() - kOR_cy;
                const float dsq = dx*dx + dy*dy;
                if (dsq < best_or_dsq)
                {
                    best_or_dsq = dsq;
                    s_inn_cache.horde_x = cd.spawnPoint.GetPositionX();
                    s_inn_cache.horde_y = cd.spawnPoint.GetPositionY();
                    s_inn_cache.horde_z = cd.spawnPoint.GetPositionZ();
                    s_inn_cache.horde_valid = true;
                }
            }
        }
        s_inn_cache.loaded = true;
        TC_LOG_INFO("playerbot.v2",
            "[GlobalStuckRescue] Capital innkeeper cache: alliance={} ({:.1f},{:.1f},{:.1f}), horde={} ({:.1f},{:.1f},{:.1f})",
            s_inn_cache.alliance_valid, s_inn_cache.alliance_x, s_inn_cache.alliance_y, s_inn_cache.alliance_z,
            s_inn_cache.horde_valid, s_inn_cache.horde_x, s_inn_cache.horde_y, s_inn_cache.horde_z);
    });
    if (alliance && s_inn_cache.alliance_valid)
        return WorldLocation(0, Position(s_inn_cache.alliance_x, s_inn_cache.alliance_y, s_inn_cache.alliance_z, 0.f));
    if (!alliance && s_inn_cache.horde_valid)
        return WorldLocation(1, Position(s_inn_cache.horde_x, s_inn_cache.horde_y, s_inn_cache.horde_z, 0.f));
    return alliance
        ? WorldLocation(0, Position(-8868.f, 671.f, 98.f, 0.f))
        : WorldLocation(1, Position(1633.f, -4439.f, 17.f, 0.f));
}

void GlobalStuckRescue(Player* p, BotAI* ai)
{
    if (!p || !ai) return;
    if (!p->IsInWorld()) return;
    if (p->IsBeingTeleported()) return;
    if (p->IsFlying() || p->IsFalling()) return;
    // Taxi flight (UNIT_STATE_IN_FLIGHT) — the core owns movement; a homebind
    // teleport here would abort the flight and strand the bot. IsFlying() above
    // is a movement FLAG (free-flight mounts) and is NOT set during a taxi
    // spline, so this needs its own guard.
    if (p->IsInFlight()) return;
    if (p->InBattleground()) return;        // BG handles its own respawn loop
    if (p->GetMap() && p->GetMap()->IsDungeon()) return;   // dungeon party in progress
    if (p->GetMap() && p->GetMap()->IsRaid()) return;      // raid encounter — never yank
    if (p->GetTransport() || p->GetVehicle()) return;
    if (p->GetGroup() != nullptr) return;   // grouped bot — leader-follow handles travel
    // LFG-pipeline guard. The IsDungeon()/IsRaid()/group guards above only cover a
    // bot that is CURRENTLY on the instance map AND still grouped. A dungeon-finder
    // member that is path-blocked at the instance ENTRANCE (the portal sits on a
    // world map — e.g. Ragefire's is inside Orgrimmar), mid-teleport between world
    // and instance, or transiently solo for a tick while the LFG group settles is
    // NONE of those — so a homebind teleport here yanks it out of the run and
    // collapses the whole group (observed live: a path-blocked squad member was
    // teleported to Stormwind and the Deadmines run dissolved). A teleport-rescue
    // is the wrong tool for a dungeon-runner anyway ([[feedback_no_teleport_rescue]]).
    // Skip the rescue for any bot in the LFG pipeline (queued → proposal → role
    // check → in-dungeon → finished) or with dungeon-run mode active; the LFG /
    // dungeon system owns its location and a stuck member re-paths toward the group.
    if (ai->dungeon_run_engaged()) return;
    if (sLFGMgr->GetState(p->GetGUID()) != lfg::LFG_STATE_NONE) return;
    // Combat: flee_hazard legitimately fires mid-encounter for world-boss
    // ground AoEs, elite quest mob cleave puddles, etc. Teleporting out
    // would ruin the fight (the bot abandons combat AND the player's
    // expectation that the bot is helping). Block while in combat; the
    // path failures will continue but the per-rule wedges still apply,
    // and once combat ends the threshold can trip on the next window.
    if (p->IsInCombat()) return;
    // Corpse run / ghost: dead:walking_to_corpse fires for a legitimate
    // post-wipe recovery. The State_Dead 5-min timeout already escalates
    // to SpiritResurrect when the corpse path is truly unreachable; we
    // mustn't pre-empt that path with a homebind teleport (which would
    // strand the corpse and not actually resurrect the bot).
    if (!p->IsAlive()) return;
    if (p->HasPlayerFlag(PLAYER_FLAGS_GHOST)) return;
    // Charter founders + signers were just deliberately teleported onto
    // the petitioner plaza by BotGuildMgr. Their path_blocks counter
    // grows from in-plaza walks (5y movement to petitioner / signer ring
    // jitter); homebind-rescuing them sends the founder cross-continent
    // and the FSM aborts. The 20-min charter-grace fully covers the
    // FSM budget — skip the global rescue while it's armed.
    if (ai->in_charter_grace(getMSTime())) return;
    // No-teleport-rescue for objective-unreachable bots. When the bot is wedged
    // because its CURRENT quest objective is genuinely unpathable, the fast
    // NoPath blacklist (note_obj_observed → ~6s) has already / is about to flag
    // the objective so the snapshot Builder's picker moves to a reachable quest.
    // That reroute is the correct fix (the bot walks somewhere it CAN path and
    // path_blocked_count resets) — yanking it to homebind/capital instead would
    // violate the no-teleport-rescue principle and strand it mid-zone. Defer the
    // rescue while the wedge is attributable to the objective walk AND that
    // objective is currently blacklisted; genuine off-mesh / fall-through cases
    // (last rule was wander / watchdog_escape / hub travel, no objective
    // blacklist) still escalate to the teleport below as a last resort.
    {
        char const* lr = ai->last_rule_fired();
        const bool on_objective_walk =
            lr != nullptr && std::string_view(lr) == "idle:quest_path";
        if (on_objective_walk && ai->current_objective_blacklisted(getMSTime()))
            return;
    }

    const uint32 cur     = ai->path_blocked_count();
    const uint32 now     = getMSTime();
    const uint64 key     = p->GetGUID().GetCounter();

    GlobalStuckState st{};
    {
        auto it = g_global_stuck.find(key);
        if (it != g_global_stuck.end()) st = it->second;
    }

    // First observation OR baseline aged out → reset baseline.
    if (st.baseline_ms == 0 || getMSTimeDiff(st.baseline_ms, now) > kStuckWindowMs)
    {
        st.baseline_blocks = cur;
        st.baseline_ms     = now;
        g_global_stuck[key] = st;
        return;
    }

    // Counter regressed below the baseline. path_blocked_count_ now resets to
    // 0 on every successful move (note_move_succeeded), so `cur < baseline`
    // means the bot made real progress since the window opened — it is NOT
    // wedged. Re-baseline and bail; without this guard the uint32 subtraction
    // below underflows to ~4e9 and trips an immediate false rescue.
    if (cur < st.baseline_blocks)
    {
        st.baseline_blocks = cur;
        st.baseline_ms     = now;
        g_global_stuck[key] = st;
        return;
    }

    // Within the cooldown window after a recent rescue → don't re-rescue.
    if (st.last_rescue_ms != 0 && getMSTimeDiff(st.last_rescue_ms, now) < kStuckRescueCooldown)
        return;

    const uint32 grew = cur - st.baseline_blocks;
    if (grew < kStuckThreshold) return;

    // If we already rescued this bot recently (within 10 min), its
    // homebind is itself unreachable — observed for Dracthyr bots whose
    // homebind landed on map 2081 (Forbidden Reach), a flying-only zone
    // with sparse navmesh. Escalate to faction capital, which always sits
    // on dense navmesh-valid terrain (Stormwind / Orgrimmar plaza).
    const bool homebind_proven_bad =
        st.last_rescue_ms != 0 &&
        getMSTimeDiff(st.last_rescue_ms, now) < kRescueRepeatWindowMs;

    WorldLocation tgt;
    auto snap = Services::Snapshots().latest(key);
    if (!homebind_proven_bad &&
        snap && snap->travel.homebind_map_id != 0)
    {
        tgt = WorldLocation(snap->travel.homebind_map_id,
                            snap->travel.homebind_x,
                            snap->travel.homebind_y,
                            snap->travel.homebind_z, 0.f);
    }
    else
    {
        const bool alliance = (p->GetTeam() == ALLIANCE);
        tgt = FindCapitalRescuePos(alliance);
        if (homebind_proven_bad)
        {
            TC_LOG_WARN("playerbot.v2",
                "[GlobalStuckRescue] {} previous homebind rescue didn't stick — "
                "escalating to faction capital instead", p->GetName());
        }
    }

    TC_LOG_WARN("playerbot.v2",
        "[GlobalStuckRescue] {} path_blocks grew {}→{} in {}ms; teleporting to homebind (map={} {:.1f},{:.1f},{:.1f})",
        p->GetName(), st.baseline_blocks, cur,
        getMSTimeDiff(st.baseline_ms, now),
        tgt.GetMapId(), tgt.GetPositionX(), tgt.GetPositionY(), tgt.GetPositionZ());

    Playerbot::BotMovement::SafeTeleport(p, tgt, /*options*/ 0);

    // Post-teleport state reset. Without this, the rules' next tick
    // would carry forward:
    //   - the IntentQueue's pending move_to entries from the prior
    //     stuck position (now firing against fresh coords, which fails
    //     just as fast),
    //   - the rule-wedge slots still marking "idle:wander is wedged",
    //   - the monotonic path_blocked_count primed near the rescue
    //     threshold, so the very next failure would re-rescue.
    // Drain the intent queue inline (single consumer = world thread =
    // here), then ai.reset_after_rescue clears the wedge/counter state
    // and arms the grace timer that travel rules consult.
    if (auto* q = Services::Registry().intents(key))
    {
        Intent discarded;
        while (q->pop(discarded)) { /* drop */ }
    }
    ai->reset_after_rescue(now);

    st.last_rescue_ms  = now;
    st.baseline_blocks = cur;
    st.baseline_ms     = now;
    g_global_stuck[key] = st;
}

// Companion tank threat (the owner's AiPlayerbot.TankThreatMultiplier on his
// VMaNGOS bots): a grouped bot owned by a human whose current duty is tank
// generates Playerbot.TankThreatMultiplier x threat, so the owner and the DPS
// bots rarely pull aggro off it. Duty = the PvE coordinator's order when one
// is active (tank_duty != 0; a tank the raid roster turned into DPS has
// tank_duty 0), else the bot's effective role (owner /setrole, else spec).
// Every other bot is held at 1.0, so leaving the group, dismissal (ownership
// cleared) or losing the duty resets it with the bot's next published
// snapshot. Applied core-side in ThreatManager::AddThreat to the bot's own
// generated threat only. World thread (post-barrier publish pass).
// Non-tank bots of such a group (not in a battleground, inside a dungeon or raid map)
// get Playerbot.DpsThreatMultiplier instead (Karrz's VMaNGOS 0.8: DPS pull less); 1.0 = off.
void UpdateTankThreatMultiplier(BotId id, Player* p, BotAI const* ai,
                                BotSnapshot const& snap, float tank_mult, float dps_mult)
{
    if (!p)
        return;
    float want = 1.0f;
    std::string duty = "none";
    // Not in a battleground: no tank duty there, and the threat it multiplied would only be an NPC's (AV).
    if ((tank_mult != 1.0f || dps_mult != 1.0f) && p->GetGroup() && !snap.bg.in_battleground &&
        Services::Owners().GetOwner(id).account_id != 0)
    {
        PveOrder const& po = snap.pve_order;
        bool tank = false;
        if (po.active)
        {
            tank = po.tank_duty != 0;
            duty = std::to_string(uint32(po.tank_duty));
        }
        else
        {
            BotSnapshotView const view(snap);
            tank = (ai ? ai->effective_role(view) : view.my_role()) == Role::Tank;
            duty = tank ? "role-tank" : "none";
        }
        if (tank)
            want = tank_mult;
        else if (dps_mult != 1.0f && (snap.instance_ctx.is_in_dungeon || snap.instance_ctx.is_in_raid))
            want = dps_mult;
    }
    API api(p);
    if (api.threat_multiplier() == want)
        return;
    api.set_threat_multiplier(want);
    TC_LOG_DEBUG("playerbot.v2", "[tank_threat] {} threat x{:.2f} (duty {})",
                p->GetName(), want, duty);
}

// Bot cast pacing (Playerbot.Cast.*, OFF by default): when enabled, raid bots cast faster
// than the shipped rates, BG and world-PvP bots slower, with a constant per-bot skill
// offset. Context comes from the published snapshot (CastPacing.h); the multiplier is
// applied core-side to this bot's cast time and GCD (API::set_cast_speed_multiplier).
// Rewritten only when the value changes, and logged once per change. World thread
// (post-barrier publish pass).
void UpdateCastSpeed(Player* p, BotSnapshot const& snap, ConfigReader::CastPacing const& cfg)
{
    if (!p)
        return;
    BotSnapshotView const view(snap);
    CastContext const ctx = ClassifyCastContext(view);
    float const want = CastSpeedMultiplier(cfg, ctx, snap.guid);
    API api(p);
    if (api.cast_speed_multiplier() == want)
        return;
    api.set_cast_speed_multiplier(want);
    TC_LOG_DEBUG("playerbot.v2", "[cast_speed] {} context={} cast/gcd x{:.2f} (skill {:+d}%)",
                p->GetName(), CastContextName(ctx), want,
                cfg.enabled ? CastSkillOffsetPct(snap.guid, cfg.variance_pct) : 0);
}

// PvP reaction floor (Playerbot.Pacing.PvpReactionDelayMs, the owner's AiPlayerbot.PvpReactionDelay):
// while the bot is in PvP - a battleground/arena, an active duel, or in combat with players
// (ClassifyCastContext BG/WorldPvp) - its bot tick and snapshot period is at least the floor,
// so a fleet does not out-react real players. PvE (raid/dungeon/world) never gets it. Survival
// bypass: a dead/ghost bot (release, resurrection) or one below 35% HP is ticked at its normal
// tier period, so healing, defensives and ress are not starved. World thread (publish pass).
void UpdatePvpFloor(BotId id, Player* p, BotSnapshot const& snap, uint32 floor_ms, Ms now)
{
    uint32 ms = 0;
    if (floor_ms && p)
    {
        BotSnapshotView const view(snap);
        CastContext const ctx = ClassifyCastContext(view);
        bool const duel = p->duel && p->duel->State == DUEL_STATE_IN_PROGRESS;
        bool const pvp  = ctx == CastContext::Battleground || ctx == CastContext::WorldPvp || duel;
        bool const survival = !view.is_alive() || view.is_ghost() || view.hp_pct() < 35;
        if (pvp && !survival)
            ms = floor_ms;
    }
    Services::Scheduler().set_pvp_floor(id, Ms{ms}, now);
}

} // namespace

Module& Module::instance()
{
    static Module m;
    return m;
}

void Module::Init()
{
    if (initialized_)
        return;

    TC_LOG_INFO("server.loading", "WoW Forever Bots {} by Karrz, based on agatho's PlayerbotV2 (built {}).",
                "0.1", __DATE__);
    TC_LOG_INFO("server.loading", "[PlayerbotV2] Initializing module.");

    // Apply schema migrations before bringing up Services. If this fails,
    // refuse to initialize so the server doesn't run with a corrupt schema.
    PlayerbotMigrationMgr migrations;
    if (!migrations.run_all())
    {
        TC_LOG_ERROR("server.loading", "[PlayerbotV2] Schema migration failed; module disabled.");
        return;
    }

    // The bots' speech lines (shared DB, migration 0019). Loaded before any bot can tick; read-only
    // afterwards, so bot workers and the world thread read it without a lock.
    BotTextMgr::instance().Load();

    // Bring up Services (SnapshotPublisher, IntentQueue, AiWorkerPool,
    // FleetThread, TickScheduler, BotRegistry). Adds threads.
    Services::Init();

    // A capital's navmesh wades the canals. In a zone with no leveling range an ungrouped bot needs a dry route or does not
    // move (RequireDry); a follower (group, companion) takes a dry route when one exists and otherwise follows through the
    // water (PreferDry). A bot already in the water is exempt (its way out ends on a wet shore). BG, raid and instance
    // movement are untouched.
    ::Playerbot::BotMovement::g_LiquidPathPolicy = [](Player* p) -> ::Playerbot::BotMovement::LiquidPolicy
    {
        if (!p || !p->GetMap())
            return ::Playerbot::BotMovement::LiquidPolicy::None;
        uint8 lo = 0, hi = 0;
        return ::Playerbot::BotMovement::PickLiquidPolicy(
            p->IsInWater(),
            p->InBattleground() || p->GetMap()->IsDungeon() || p->GetMap()->IsBattlegroundOrArena(),
            ::Playerbot::V2::World::VanillaZoneLevelRange(p->GetZoneId(), lo, hi),
            p->GetGroup() != nullptr,
            Services::Owners().GetOwner(p->GetGUID().GetCounter()).account_id != 0);
    };

    // Register class/spec rotations (single-threaded; runs before bot workers
    // dispatch ticks). Read-only afterwards; safe for concurrent lookup.
    Combat::RegisterAllRotations();

    // Tier 3.1 guard: prove BotSnapshot::reset_for_reuse() clears every
    // member before we start recycling snapshots in Build(). A leak here is a
    // silent stale-data correctness bug; abort at boot instead.
    Playerbot::VerifyResetClearsAll();

    // Pre-warm grids + nav tiles for high-traffic bot zones (8 capitals + 3
    // class-starter maps). Sync; ~1-3s of disk I/O on cold cache, paid once
    // at startup so the population manager's batched logins don't cascade
    // into per-tick navmesh loads on the map worker thread.
    World::PrewarmCommonZones();

    // Pin all battleground terrain (grid maps + vmap tiles + mmap tiles)
    // resident for the server's lifetime. BG maps are instanced; without
    // the pin their tiles unload when the last match ends and the next
    // match re-reads them from disk synchronously on map-update threads,
    // which showed up as 4-10s world-tick spikes around match boundaries
    // once the 2026-06-12 nav regen gave BG maps real vmap/mmap data.
    World::PinBattlegroundTerrain();

    // Reconcile BotNamePool against the characters table â€” any name with
    // is_used=1 whose used_by_guid no longer exists in characters gets
    // released. Catches SQL-wipe / mid-create-crash orphans so a fresh
    // boot doesn't see a phantom-claimed pool.
    Fleet::BotNamePool::ReconcileOnBoot();

    // Re-roll bot appearances the client would reject (retail-only
    // customization options in the Classic 1.60 data crashed the owner's
    // client when such a bot came into view). Before any bot logs in.
    BotCharacterFactory::RepairAppearances();

    // Load operator-curated world knowledge (roads, cities, danger zones,
    // hubs, vendors, mailboxes, innkeepers, crossroads). Populated via
    // `.playerbot meta add` GM commands during play; consumed by snapshot
    // builder + mmaps_generator. Synchronous one-shot load — table is
    // bounded at hundreds-to-thousands of rows, takes <100ms. Future
    // mutations through HandleMeta update both DB and in-memory cache
    // directly so the cache doesn't drift from the source of truth.
    Playerbot::V2::World::WorldMetadataStore::Instance().ReloadFromDb();

    // Future inits (as subsystems land):
    //   - ConfigReader::load(...) (Util/)
    //   - PopulationManager / LfgMediator / BgFiller (Fleet/)
    //   - EncounterRegistry::RegisterAll() (Combat/Encounters/)

    initialized_ = true;

    if (Services::Config().auto_resume_on_boot())
    {
        // B-2: clamp the boot fill to Population.TotalTarget. AutoResumeCap
        // is an independent knob (default 100), so a 10-bot target still
        // boot-logged 100+ sessions which then sat overshot until the
        // population manager trimmed them — boot now respects the target
        // immediately and Reconcile only ever tops UP from here.
        auto_resume_pending_ = Services::Config().auto_resume_cap();
        if (uint32 target = Services::Config().population_total_target())
            auto_resume_pending_ = std::min(auto_resume_pending_, target);
        TC_LOG_INFO("server.loading",
            "[PlayerbotV2] AutoResumeOnBoot=true; will batch-login marked bots over multiple ticks (cap {}).",
            auto_resume_pending_);
    }
    auto_spawn_pending_ = Services::Config().auto_spawn_on_boot();
    if (auto_spawn_pending_)
    {
        TC_LOG_INFO("server.loading",
            "[PlayerbotV2] AutoSpawnOnBoot={}; will create new bots on first world tick to reach that target (hard cap 200).",
            auto_spawn_pending_);
    }
    // #1B: apply the configured wedge-confirm thresholds + displacement gate.
    wedge_watchdog_.set_threshold_ms(Services::Config().wedge_watchdog_threshold_ms());
    wedge_watchdog_.set_combat_threshold_ms(Services::Config().wedge_watchdog_combat_threshold_ms());
    wedge_watchdog_.set_min_displacement(Services::Config().wedge_watchdog_min_displacement());
    wedge_watchdog_.set_noprogress_enabled(Services::Config().wedge_noprogress_enabled());
    wedge_watchdog_.set_noprogress_ms(Services::Config().wedge_noprogress_ms());
    wedge_watchdog_.set_noprogress_radius(Services::Config().wedge_noprogress_radius());

    TC_LOG_INFO("server.loading", "[PlayerbotV2] Initialization complete.");
}

// ---- Bot-group hygiene -----------------------------------------------------
//
// A group of pure bots must NOT persist across a server restart: it reloads
// with stale leader/position state and the followers wedge chasing a leader
// that's nowhere near where it was (the follow_recall spam in the logs). Only
// a group that contains a human is allowed to persist. And once that human has
// been logged out for more than 30 minutes, the group is disbanded too — the
// bots would otherwise follow an absent leader forever.
//
// Two entry points:
//   * on_shutdown=true  (Module::Shutdown): disband every pure-bot group so
//     none survive to the next boot. Human groups persist.
//   * on_shutdown=false (periodic OnWorldUpdate): disband human groups whose
//     human member(s) have ALL been offline ≥30 min. Pure-bot groups are left
//     alone at runtime — they're legitimate (dungeon / BG / quest) and the
//     walk-first follow recovery handles any transient wedging.
namespace
{
    constexpr uint32 kHumanOfflineDisbandMs = 30u * 60u * 1000u;
    // group-id → first tick we observed ALL its humans offline. Cleared when a
    // human comes back online or the group is disbanded. World-thread only.
    std::unordered_map<ObjectGuid::LowType, uint32> g_group_humans_offline_since;

    // purge_pure_bot=true  → disband every pure-bot group (used at shutdown and
    //                        once at boot, so none persist across a restart /
    //                        survive a crash). Human groups are left intact.
    // purge_pure_bot=false → runtime pass: leave pure-bot groups (legit dungeon
    //                        / BG / quest groups) and only disband human groups
    //                        whose human member(s) have ALL been offline ≥30min.
    //
    // only_these (boot purge): the group ids that existed when the module's first tick ran. The purge runs when the boot gate opens,
    // which with no human logged in is 60 s later, and a group formed in that minute is live, not persisted. 2026-10-09: the
    // scenario runner's squad was disbanded mid-Deadmines by this purge ("sees a group of 0" on all five at once).
    std::unordered_set<ObjectGuid::LowType> g_boot_group_ids;
    void DisbandStaleBotGroups(bool purge_pure_bot, uint32 now_ms, std::unordered_set<ObjectGuid::LowType> const* only_these = nullptr)
    {
        if (!Services::Initialized())
            return;
        std::vector<std::pair<Group*, char const*>> to_disband;
        for (auto const& [gid, group] : sGroupMgr->GetGroupStore())
        {
            if (!group)
                continue;
            if (only_these && !only_these->count(gid))
                continue;
            bool has_bot = false, has_human = false, any_human_online = false;
            for (auto const& slot : group->GetMemberSlots())
            {
                if (Services::Lifecycle().is_bot(slot.guid.GetCounter()))
                {
                    has_bot = true;
                }
                else
                {
                    has_human = true;
                    if (ObjectAccessor::FindConnectedPlayer(slot.guid))
                        any_human_online = true;
                }
            }
            // Groups with no bot member are not our concern.
            if (!has_bot)
                continue;

            if (!has_human)
            {
                g_group_humans_offline_since.erase(gid);
                if (purge_pure_bot)
                {
                    // LFG dungeon groups that are actively inside an instance
                    // are NOT stale persisted leftovers — they're live runs
                    // (e.g. a pure-bot dungeon test that queued via LFG). Skip
                    // them at boot so the boot purge doesn't eject bots from a
                    // dungeon they just entered. The purge targets groups that
                    // survived a hard-kill/crash without running Shutdown, which
                    // means they exist in the DB but have no live members — those
                    // groups will have no connected players in their slots.
                    if (group->isLFGGroup())
                    {
                        bool any_connected = false;
                        for (auto const& slot : group->GetMemberSlots())
                            if (ObjectAccessor::FindConnectedPlayer(slot.guid))
                            { any_connected = true; break; }
                        if (any_connected)
                            continue;   // live LFG run — leave it alone
                    }
                    to_disband.emplace_back(group, "pure-bot");
                }
                continue;
            }

            // Human-containing group: persists across shutdown/boot by design.
            if (purge_pure_bot)
                continue;
            if (any_human_online)
            {
                g_group_humans_offline_since.erase(gid);   // reset the AFK timer
                continue;
            }
            // All humans offline — start / check the 30-min disband timer.
            auto it = g_group_humans_offline_since.find(gid);
            if (it == g_group_humans_offline_since.end())
                g_group_humans_offline_since[gid] = now_ms;
            else if (now_ms - it->second >= kHumanOfflineDisbandMs)
                to_disband.emplace_back(group, "human-afk-30min");
        }
        // Disband AFTER iterating — Group::Disband() mutates the GroupStore.
        for (auto const& [g, reason] : to_disband)
        {
            g_group_humans_offline_since.erase(g->GetGUID().GetCounter());
            TC_LOG_INFO("playerbot.v2",
                "[group_hygiene] disbanding bot group {} reason={}",
                g->GetGUID().GetCounter(), reason);
            g->Disband();
        }
    }
}

void Module::Shutdown()
{
    if (!initialized_)
        return;

    TC_LOG_INFO("server.loading", "[PlayerbotV2] Shutting down module.");

    // Disband pure-bot groups BEFORE Services::Shutdown() (which tears down the
    // bot-identity registry is_bot() relies on) so they don't persist to the
    // next boot and reload into wedged follow loops.
    DisbandStaleBotGroups(/*purge_pure_bot=*/true, getMSTime());

    // #5 Phase 4: join the parallel snapshot-build workers before tearing down
    // Services — the build tasks call into Services (Perf / Registry) and must
    // be quiesced first. stop() joins all worker threads; destruction would
    // also join, but doing it here makes the ordering explicit and ensures no
    // worker is mid-Build when Services::Shutdown() runs.
    if (snapshot_build_pool_)
    {
        snapshot_build_pool_->stop();
        snapshot_build_pool_.reset();
    }

    // Log every headless bot out while ALL services are still alive: ~WorldSession runs LogoutPlayer, whose
    // hooks (OnPlayerLogout, group/guild) reach Population/Guilds/Perf, which Services::Shutdown destroys
    // before the session manager. Update() performs the logout for each kicked session and reaps it.
    Services::SessionMgr().LogoutAll();
    for (int pass = 0; pass < 10 && Services::SessionMgr().active_count() > 0; ++pass)
        Services::SessionMgr().Update(0);

    // Stop threads first; then tear down state.
    Services::Shutdown();

    // Release the battleground terrain pins before static destruction, which
    // would otherwise unload their navmeshes from an already-destroyed
    // MMapManager.
    World::UnpinBattlegroundTerrain();

    initialized_ = false;
}

// Pets of playerbots that cannot walk to their owner are set down next to them (Fleet/PetSnap.h), the way a player's pet ends up beside
// its owner when it falls behind. Each pet is looked at every kPetSnapCheckMs; runs on the world thread, before the maps update.
static void DrivePetSnap(uint32 now_ms)
{
    static std::unordered_map<uint64, Fleet::PetSnapState> state;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const&)
    {
        Player* owner = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
        if (!owner || !owner->IsInWorld() || !owner->IsAlive() || !owner->GetMap() || owner->IsBeingTeleported() || owner->IsInFlight())
            return;
        Pet* pet = owner->GetPet();
        if (!pet || !pet->IsInWorld() || !pet->IsAlive() || pet->GetMap() != owner->GetMap() || pet->IsInCombat())
            return;
        Fleet::PetSnapState& st = state[uint64(id)];
        if (!Fleet::PetSnapDue(st, now_ms))
            return;
        if (pet->GetExactDist(owner) <= Fleet::kPetSnapMinGap)
        {
            Fleet::PetSnapUpdate(st, false, now_ms);
            return;
        }
        PathGenerator pg(pet);
        ::Playerbot::BotMovement::SehSafeCalculatePath(pg, owner->GetPositionX(), owner->GetPositionY(), owner->GetPositionZ());
        PathType const type = pg.GetPathType();
        G3D::Vector3 const& end = pg.GetActualEndPosition();
        float const dx = end.x - owner->GetPositionX(), dy = end.y - owner->GetPositionY(), dz = end.z - owner->GetPositionZ();
        float const end_gap = std::sqrt(dx * dx + dy * dy + dz * dz);
        bool const unreachable = (type & (PATHFIND_NOPATH | PATHFIND_SHORTCUT)) || ((type & PATHFIND_INCOMPLETE) && end_gap > 4.0f);
        if (!Fleet::PetSnapUpdate(st, unreachable, now_ms))
            return;
        pet->StopMoving();
        pet->NearTeleportTo(Position(owner->GetPositionX(), owner->GetPositionY(), owner->GetPositionZ(), owner->GetOrientation()));
        TC_LOG_INFO("playerbot.v2", "[pet_snap] {}'s pet {} had no way to its owner at ({:.0f},{:.0f},{:.0f}), set down next to {}",
                    owner->GetName(), pet->GetName(), owner->GetPositionX(), owner->GetPositionY(), owner->GetPositionZ(), owner->GetName());
    });
    if (state.size() > 2048u)
        for (auto it = state.begin(); it != state.end();)
            it = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(it->first)) ? std::next(it) : state.erase(it);
}

void Module::OnWorldUpdate(std::chrono::milliseconds diff)
{
    if (!initialized_) return;
    static std::chrono::milliseconds total{0};
    // #5 Phase 4: next_version is read-modify-write per built snapshot. Under
    // parallel-by-Map* Build a plain ++ is a torn counter (duplicate/garbage
    // versions break the bot worker's staleness/version checks). Versions need
    // only be UNIQUE + monotonic, not contiguous, so an atomic fetch_add is
    // the race-free fix. Pre-assigned per bot before the parallel region, so
    // the parallel tasks never touch this counter themselves.
    static std::atomic<SnapshotVer> next_version{1};
    static TickId      next_tick    = 1;
    total += diff;
    {
        static std::chrono::milliseconds last_hint_check{0};
        if (total - last_hint_check >= std::chrono::milliseconds(2000))
        {
            last_hint_check = total;
            SendDueAddonHints();
        }
    }
    ::Playerbot::Diagnostics::Scenario::Tick(std::uint32_t(diff.count()));   // bot scenario tests (.playerbot scenario)

    // [cast_state] (playtest, WSG start room 2026-10-11: a healer stood with the casting animation on while casting nothing). Twice a
    // second, every bot in a battleground: a line when the spell it is casting changes (spell id, generic or channel, 0 = none), and a
    // [cast_stale] line when the server holds no cast at all but the unit data still names a channel spell: what a client draws as a
    // cast that never ends. The log answers which of the two it was; nothing is cleared here.
    {
        static std::chrono::milliseconds last_cast_check{0};
        static std::unordered_map<BotId, uint32> cast_seen;
        static std::unordered_map<BotId, uint32> cast_since;
        static std::unordered_map<BotId, uint32> stale_logged_ms;
        static uint32 cast_lines = 0;
        if (total - last_cast_check >= std::chrono::milliseconds(500))
        {
            last_cast_check = total;
            uint32 const now_ms = GameTime::GetGameTimeMS();
            Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
            {
                if (!e.ai) return;
                Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
                if (!p || !p->IsInWorld() || !p->GetMap() || !(p->GetMap()->IsBattleground() || p->GetMap()->IsRaid())) { cast_seen.erase(id); cast_since.erase(id); return; }
                Spell const* gen = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
                Spell const* chan = p->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
                uint32 const now_spell = chan ? chan->GetSpellInfo()->Id : gen ? gen->GetSpellInfo()->Id : 0u;
                uint32& seen = cast_seen[id];
                if (seen != now_spell)
                {
                    cast_since[id] = now_ms;
                    if (cast_lines < 1500)
                    {
                        ++cast_lines;
                        TC_LOG_INFO("playerbot.v2", "[cast_state] {} spell {} -> {} ({}) casting_state={} mounted={} combat={}", p->GetName(), seen, now_spell,
                                    chan ? "channel" : gen ? "generic" : "none", p->HasUnitState(UNIT_STATE_CASTING), p->IsMounted(), p->IsInCombat());
                    }
                    seen = now_spell;
                }
                else if (now_spell && cast_since[id] && now_ms - cast_since[id] >= 8000u && cast_lines < 1500)
                {
                    // One cast that has been the current spell for 8 s or more: a cast bar that is not filling.
                    ++cast_lines;
                    cast_since[id] = now_ms;
                    Spell const* cur = chan ? chan : gen;
                    TC_LOG_INFO("playerbot.v2", "[cast_long] {} still on spell {} ({}) state={} timer={} ms target={} casting_state={} combat={}", p->GetName(), now_spell,
                                chan ? "channel" : "generic", uint32(cur->getState()), cur->GetTimer(), cur->m_targets.GetUnitTargetGUID().GetCounter(),
                                p->HasUnitState(UNIT_STATE_CASTING), p->IsInCombat());
                }
                if (!gen && !chan && !p->HasUnitState(UNIT_STATE_CASTING) && p->GetChannelSpellId() != 0)
                {
                    uint32& last = stale_logged_ms[id];
                    if (!last || now_ms - last >= 5000u)
                    {
                        last = now_ms ? now_ms : 1u;
                        TC_LOG_INFO("playerbot.v2", "[cast_stale] {} no server cast but channel spell {} still in unit data (visual {})", p->GetName(),
                                    p->GetChannelSpellId(), p->GetChannelSpellXSpellVisualId());
                    }
                }
            });
        }
    }

    // Bots hovering over the floor in a battleground: the start points (AB Trollbane Hall z=-9.0 over a courtyard at -15) sit a few
    // yards above the ground. A player falls client-side; a bot never does, so it stood in the air until it walked. Once a second,
    // a quarter of the bots (by id) that stand still in a battleground and are 2..30 y above the floor drop to it with a falling
    // spline (playtest, AB test 2026-10-08, "bots flying in the air").
    {
        static std::chrono::milliseconds last_settle{0};
        static uint32 settle_phase = 0;
        if (total - last_settle >= std::chrono::milliseconds(1000))
        {
            last_settle = total;
            uint32 const phase = settle_phase++ & 3u;
            Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
            {
                if (!e.ai || (id & 3u) != phase) return;
                Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id));
                if (!p || !p->IsInWorld() || !p->GetMap() || !p->GetMap()->IsBattleground()) return;
                // A bot already mounted indoors in a battleground (start tunnel / hall) gets off: nobody rides in there.
                if (p->IsMounted() && !p->IsOutdoors())
                {
                    p->RemoveAurasByType(SPELL_AURA_MOUNTED);
                    static uint32 dm_logged = 0;
                    if (dm_logged++ < 100)
                        TC_LOG_INFO("playerbot.v2", "[bg_indoor_dismount] {} map={} ({:.1f},{:.1f})", p->GetName(), p->GetMapId(),
                                    p->GetPositionX(), p->GetPositionY());
                    return;
                }
                float const z0 = p->GetPositionZ();
                if (::Playerbot::BotMovement::SettleToGround(p, 2.0f, 30.0f))
                {
                    static uint32 logged = 0;
                    if (logged++ < 200)
                        TC_LOG_INFO("playerbot.v2", "[hover_settle] {} map={} ({:.1f},{:.1f}) z {:.1f} -> ground",
                                    p->GetName(), p->GetMapId(), p->GetPositionX(), p->GetPositionY(), z0);
                }
            });
        }
    }

    // First-tick: build quest reverse-indices (KillCredit aliases + creature
    // labels). Templates are guaranteed loaded by world-update time; doing
    // this once on first tick avoids a per-snapshot lazy build under contention.
    {
        static bool reverse_indices_built = false;
        if (!reverse_indices_built)
        {
            EnsureQuestReverseIndicesBuilt();
            // Quest objective locations for quests without QuestPOI (vanilla):
            // spawn / loot-source / areatrigger clusters. One-time DB + store
            // walk on the world thread; read-only afterwards.
            EnsureQuestObjectiveLocationsBuilt();
            reverse_indices_built = true;
        }
    }

    // Boot-time human-login priority gate. AutoResume / AutoSpawn are
    // deferred until either (a) a non-bot WorldSession is in-world, or
    // (b) kBootGraceMs has elapsed since the first OnWorldUpdate.
    //
    // Without this gate, AutoSpawn fired synchronously on tick #1, doing
    // up to 200 Ã— BotCharacterFactory::Create (each SaveToDB ~30ms) on
    // the world thread â€” ~6s of blocking during which inbound human
    // login packets sat in the session queue and the player's client
    // timed out / appeared frozen. Observed 2026-05-15 right after the
    // V2 wipe: server came up, AutoSpawn ran, owner couldn't log in.
    //
    // Sticky once opened â€” first human triggers it for the rest of boot.
    constexpr uint32 kBootGraceMs = 60'000;          // 60s lights-out fallback
    if (!boot_gate_open_)
    {
        if (boot_first_tick_at_.count() == 0)
        {
            boot_first_tick_at_ = total;
            for (auto const& kv : sGroupMgr->GetGroupStore())
                g_boot_group_ids.insert(kv.first);   // what is persisted from the last run; anything formed later is live
        }
        const uint32 since_first_tick_ms =
            uint32((total - boot_first_tick_at_).count());

        // Scan WorldSessionMgr for any non-bot session that has loaded
        // a Player (i.e. real human is in-world). Bot sessions live in
        // BotSessionMgr's own map; sWorld->m_sessions covers humans.
        bool human_present = false;
        // Fully qualify ::World â€” Playerbot::V2::World is a sibling
        // namespace (CapitalsTable etc.) and shadows the global class.
        for (auto const& kv : ::World::instance()->GetAllSessions())
        {
            WorldSession* session = kv.second;
            if (!session) continue;
            Player* p = session->GetPlayer();
            if (!p) continue;
            // BotSession reuses real account ids, so account-id alone can't
            // distinguish a bot from a human; check the V2 registry instead.
            if (Services::Lifecycle().is_bot(p->GetGUID().GetCounter()))
                continue;
            human_present = true;
            break;
        }

        if (human_present || since_first_tick_ms >= kBootGraceMs)
        {
            boot_gate_open_ = true;
            TC_LOG_INFO("playerbot.v2",
                "[PlayerbotV2] boot gate OPEN ({}); bot spawn/login passes resume.",
                human_present ? "human in-world" : "grace expired");
        }
    }

    // One-time boot purge: a hard crash (Shutdown never ran) can leave
    // persisted pure-bot groups in the DB; they reload with stale state and
    // wedge. Once the registry is ready (boot gate open), disband any that
    // survived. Bots haven't formed new groups yet at this point, so every
    // pure-bot group present is a stale persisted one.
    {
        static bool boot_group_purge_done = false;
        if (!boot_group_purge_done && boot_gate_open_)
        {
            boot_group_purge_done = true;
            DisbandStaleBotGroups(/*purge_pure_bot=*/true, getMSTime(), &g_boot_group_ids);
        }
    }

    // Periodic bot-group hygiene (every 60s): disband human groups whose
    // human member(s) have all been offline ≥30 min so the bots stop
    // following an absent leader. Pure-bot runtime groups are left alone
    // (handled at shutdown / boot). Cheap — a few dozen groups, once a minute.
    {
        static std::chrono::milliseconds last_group_hygiene{0};
        constexpr std::chrono::milliseconds kGroupHygieneInterval{60'000};
        if (total - last_group_hygiene >= kGroupHygieneInterval)
        {
            last_group_hygiene = total;
            DisbandStaleBotGroups(/*purge_pure_bot=*/false, getMSTime());
        }
    }

    // Auto-resume: log in marked bots up to AutoResumeCap, spread over
    // many ticks. Previously this fired LoginAll(cap) on a single tick,
    // submitting 100+ Player::SaveToDB calls simultaneously. Each save
    // ends with REPLACE INTO battlenet_account_mounts keyed by the bot's
    // shared bnet account row â€” multiple bots per pool account race on
    // the same row lock. Innodb_lock_wait_timeout=50s, FreezeDetector
    // fires at 60s. Observed 2026-05-16 crash: "MySQL errno 1205 Lock
    // wait timeout exceeded" â†’ world thread hung 60s.
    //
    // Spreading the resume kBootSpawnPerTick at a time keeps the bnet
    // write rate well below the row-lock contention threshold.
    constexpr uint32 kBootResumePerTick = 5;
    if (auto_resume_pending_ > 0 && boot_gate_open_)
    {
        const uint32 batch = std::min(auto_resume_pending_, kBootResumePerTick);
        // LoginAll's cap is "stop when active_count >= cap". Active count
        // grows as we resume; passing (current + batch) acts as a per-call
        // batch limit. Re-evaluates each tick.
        const uint32 active_now = Services::SessionMgr().active_count();
        auto r = Services::SessionMgr().LoginAll(active_now + batch);
        if (r.attempted > 0)
            TC_LOG_INFO("playerbot.v2",
                "[PlayerbotV2] AutoResume batch: attempted={} submitted={} already_in_world={} remaining={}.",
                r.attempted, r.succeeded, r.skipped_already_in_world,
                auto_resume_pending_ - r.attempted);
        // Decrement by attempts (not just successes) so a steady stream
        // of "already in world" entries doesn't starve us into an infinite
        // loop. After cap ticks, pending hits 0 either way.
        if (r.attempted >= auto_resume_pending_)
            auto_resume_pending_ = 0;
        else
            auto_resume_pending_ -= r.attempted;
        // If no attempts AND no progress, the registry is exhausted â€”
        // drain so we don't spin forever.
        if (r.attempted == 0)
            auto_resume_pending_ = 0;
    }

    // Auto-spawn: top up the marked-bot count to AutoSpawnOnBoot. Spread
    // over multiple ticks (kBootSpawnPerTick per tick) so each tick's
    // synchronous SaveToDB cost (~30ms Ã— per-tick limit) leaves headroom
    // for human-login packet processing on the world thread. At 5/tick
    // and 1Hz module tick that's ~150ms/sec of DB work â€” invisible to
    // humans. A 200-bot boot completes in ~40s instead of a 6s freeze.
    // Hard-capped at AUTO_SPAWN_HARD_CAP overall.
    constexpr uint32 AUTO_SPAWN_HARD_CAP = 200;
    constexpr uint32 kBootSpawnPerTick   = 5;
    if (auto_spawn_pending_ && boot_gate_open_)
    {
        const uint32 currentMarked = uint32(Services::Lifecycle().size());
        const uint32 target_total = std::min(
            auto_spawn_pending_ + currentMarked, currentMarked + AUTO_SPAWN_HARD_CAP);
        if (currentMarked >= target_total)
        {
            auto_spawn_pending_ = 0;
            TC_LOG_INFO("playerbot.v2",
                "[PlayerbotV2] AutoSpawn: target reached (marked={}).", currentMarked);
        }
        else
        {
            const uint32 remaining = target_total - currentMarked;
            const uint32 batch = std::min(remaining, kBootSpawnPerTick);
            uint32 created = 0, login_ok = 0, login_fail = 0;
            for (uint32 i = 0; i < batch; ++i)
            {
                auto picked = BotComposition::Roll();
                if (picked.race == 0) break;
                auto r = BotCharacterFactory::Create(
                    /*ownerSession*/ nullptr, picked.name, picked.race, picked.cls, picked.gender);
                if (!r.ok) continue;
                ++created;
                auto login = Services::SessionMgr().LoginBot(r.guid);
                if (login.ok) ++login_ok; else ++login_fail;
                if (auto_spawn_pending_ > 0) --auto_spawn_pending_;
            }
            if (created > 0)
                TC_LOG_INFO("playerbot.v2",
                    "[PlayerbotV2] AutoSpawn batch: created={} login_ok={} login_fail={} "
                    "remaining_target={} marked_now={}.",
                    created, login_ok, login_fail, auto_spawn_pending_,
                    uint32(Services::Lifecycle().size()));
        }
    }

    const uint32 tick_start_ms = getMSTime();
    // Per-phase latency capture so the FleetStatus log line surfaces which
    // subsystem owns the world-thread cost. Without this we triage perf at
    // scale by guesswork. Phases sum to ~tick latency; gap is unaccounted-for
    // (TC core, scripts, etc).
    uint32 t_snap_start_ms = 0, t_snap_ms = 0;
    uint32 t_snap_setup_ms = 0, t_snap_build_ms = 0, t_group_build_ms = 0;
    uint32 t_bgport_ms = 0, t_drain_ms = 0, t_session_ms = 0;
    uint32 t_population_ms = 0, t_scheduler_ms = 0;
    uint32 snaps_built_this_tick = 0;
    uint32 snaps_skipped_this_tick = 0;
    size_t intents_drained_this_tick = 0;

    // Per ARCHITECTURE.md Â§1.1:
    //   1. publish per-bot snapshots
    //   2. (intent draining via PlayerbotAPI lands when API surface widens)
    //   3. let TickScheduler decide which bots run this tick

    // 1. Publish snapshots for every registered bot whose Player* is in-world.
    t_snap_start_ms = getMSTime();
    auto& reg  = Services::Registry();
    auto& pub  = Services::Snapshots();
    auto& sch  = Services::Scheduler();
    const TickId tick = next_tick++;
    // Group snapshot dedup cache (group_snap_cache_, world-thread only).
    // GroupSnapshotBuilder::Build walks every group member (HP/mana/casting/
    // auras) so for a 5-bot party each bot was paying 5× the work (and a
    // 40-bot raid 40×). The first build per group-guid is shared by every
    // member that rebuilds its own snapshot within kGroupSnapMaxAgeMs.
    //   Pre-dedup cost: N_groups × N_members² member-walks per tick.
    //   Post-dedup:    N_groups × N_members  member-walks per window.
    // The window used to be ONE tick. That was enough while a group's bots
    // rebuilt in lock-step (all in the same tick), but the scheduler now
    // spreads every bot over its tier period (TickScheduler NextPhaseDeadline),
    // so a 40-bot raid has a few members rebuilding in EVERY tick — a per-tick
    // cache would re-walk all 40 members every tick (~1 ms each in a raid).
    // Half the Combat period bounds the staleness of the shared group view
    // below the bots' own snapshot cadence while keeping raid group builds at
    // ~20 Hz. Entries are also rebuilt when the group's member count or
    // leader changed (a join/leave/promote is never served from a stale view).
    constexpr uint32 kGroupSnapMaxAgeMs = 50;
    const uint32 group_cache_now_ms = getMSTime();
    for (auto it = group_snap_cache_.begin(); it != group_snap_cache_.end(); )
    {
        if (!it->second.snap ||
            getMSTimeDiff(it->second.built_ms, group_cache_now_ms) >= kGroupSnapMaxAgeMs)
            it = group_snap_cache_.erase(it);
        else
            ++it;
    }

    // --- Per-tick real-player proximity/group precompute (Tier 2.1) ------
    // The tier classifier below must answer "is this bot near, or grouped
    // with, a REAL (non-bot) player?" without doing a per-bot grid search
    // (that would re-introduce the build cost we're trying to shed). So we
    // enumerate the connected real players ONCE here — there are only a
    // handful even on a busy server — and stash:
    //   * real_player_cells  : the coarse grid cell of every real player
    //                          (see PackPlayerCellKey). A bot is "near" a
    //                          real player when its own cell is occupied.
    //   * real_player_groups : the GUID of every group that contains at
    //                          least one real player. A bot is "grouped
    //                          with a real player" when its group_guid is
    //                          in this set.
    // Real players are enumerated from World::GetAllSessions() (the global
    // human session map — bot sessions are flagged via the V2 lifecycle
    // registry's is_bot()), same pattern as the human-present boot gate
    // above. Runs on the world thread before the bot loop; both containers
    // are read-only when consumed by the tier classifier.
    //
    // #5 Phase 4: these are plain function-local (NOT thread_local). The tier
    // classifier that reads them runs in the POST-BARRIER world-thread pass
    // (set_tier is world-thread-only anyway), so a single world-thread-visible
    // copy is exactly what's needed. They were thread_local before parallel
    // Build, which would have read EMPTY on worker threads — the audit's
    // thread_local trap. Keeping them local also means zero cross-thread
    // sharing during the parallel region.
    std::unordered_set<uint64>     real_player_cells;
    std::unordered_set<ObjectGuid> real_player_groups;
    {
        const uint32 rp_t0 = getMSTime();
        auto& lifecycle = Services::Lifecycle();
        for (auto const& kv : ::World::instance()->GetAllSessions())
        {
            WorldSession* session = kv.second;
            if (!session) continue;
            Player* rp = session->GetPlayer();
            if (!rp) continue;
            if (lifecycle.is_bot(rp->GetGUID().GetCounter()))
                continue;   // a V2 bot, not a real human
            const uint32 rp_map = rp->GetMapId();
            const int32  rp_cx  = PlayerCellCoord(rp->GetPositionX());
            const int32  rp_cy  = PlayerCellCoord(rp->GetPositionY());
            for (int32 dx = -1; dx <= 1; ++dx)
                for (int32 dy = -1; dy <= 1; ++dy)
                    real_player_cells.insert(
                        PackPlayerCellKey(rp_map, rp_cx + dx, rp_cy + dy));
            if (Group const* rg = rp->GetGroup())
                real_player_groups.insert(rg->GetGUID());
        }
        t_snap_setup_ms += getMSTimeDiff(rp_t0, getMSTime());
    }
    // ---------------------------------------------------------------------

    // Team-level BG coordinator (BG audit N60) + group-level dungeon/raid
    // coordinator. MUST run before the snapshot pass below so the orders
    // they compute are copied into THIS tick's snapshots by the builder.
    // Both are internally throttled (750ms / 500ms plan cadence); the
    // off-cadence cost is one timestamp compare each.
    {
        const uint32 coord_t0 = getMSTime();
        Services::BgCoordinator().Update(coord_t0);
        Services::PveCoordinator().Update(coord_t0);
        Raid::GetBrain().Update(coord_t0);   // after the PvE plan: reads its main tank
        Raid::UpdateBossWarnings(coord_t0);
        t_snap_setup_ms += getMSTimeDiff(coord_t0, getMSTime());
    }

    // ============================ #5 Phase 4 =============================
    // PARALLEL-BY-Map* SNAPSHOT BUILD.
    //
    // Pipeline (see the audit for the per-surface rationale):
    //   (A) WORLD THREAD, per bot, in registry order:
    //         DriveTeleportAck (every tick), should_build_snapshot tier gate
    //         (mutates per-bot deadline — TickScheduler is world-thread-only),
    //         then the latency-tolerant rescue trio (RescueOrphanedBgBot /
    //         SnapToGroundIfDrifted / GlobalStuckRescue — all touch live core
    //         mutating state). Bots that pass the gate are collected, grouped
    //         by Map* POINTER, each pre-assigned a UNIQUE snapshot version.
    //   (B) PARALLEL: one task per Map* partition builds ONLY the per-bot
    //         BotSnapshots (pure per-bot CPU + read-only core reads). Each
    //         Map's bots build sequentially on a single worker, so a given bot
    //         / BotAI is touched by exactly one thread. Results are staged.
    //   (C) BARRIER join.
    //   (D) WORLD THREAD, in the SAME registry order as (A): tier classify +
    //         set_tier, group-snapshot dedup + GroupSnapshotBuilder::Build +
    //         publish_group, per-bot publish, perf. Everything the audit marks
    //         must-stay-world-thread lives here, after the barrier.
    //
    // Order preservation: (A) appends to build_order in registry order; (D)
    // iterates build_order in that same order, so set_tier / publish happen in
    // the identical sequence as the old serial loop → byte-identical behavior.
    // The only data that crosses the barrier is each bot's own freshly-built
    // BotSnapshot (single-owner) plus scalar timing.

    struct BuiltSlot
    {
        BotId                                bot_id = 0;
        Player*                              player = nullptr;   // valid for this tick (world quiescent)
        BotAI*                               ai     = nullptr;   // pre-resolved on world thread (#5 Phase 4)
        std::shared_ptr<BotSnapshot const>   snap;               // null = Build returned {}
        uint32                               build_us = 0;       // this bot's Build wall time (TickPerf slowest-build)
    };
    // Partition: Map* -> the slots whose bots live on that map. A vector of
    // (Map*, slots) preserves a stable iteration; build_order records the
    // global registry order for the post-barrier serial pass.
    std::vector<BuiltSlot> build_order;
    std::unordered_map<Map const*, std::vector<size_t>> by_map;   // Map* -> indices into build_order
    build_order.reserve(256);

    {
        const uint32 selectA_t0 = getMSTime();
        reg.for_each([&](BotId id, BotRegistryEntry const& e)
        {
            Player* p = ObjectAccessor::FindConnectedPlayer(
                            ObjectGuid::Create<HighGuid::Player>(id));
            // Drive any outstanding teleport ack BEFORE building the snapshot
            // so position-based fields reflect the post-teleport location.
            // This MUST run every tick: it advances ack/packet state for the
            // teleport handshake and is latency-sensitive (a dropped/late ack
            // wedges the bot in WORLD_STATE_LOGGEDIN), so it stays outside the
            // tier-throttle gate below. World-thread (mutates packet state).
            if (p)
                DriveTeleportAck(p);
            // Per-bot tier-driven snapshot cadence gate. Mutates the per-bot
            // deadline inside TickScheduler (world-thread-only), so it runs
            // here in the serial selection pass, never on a worker.
            if (!sch.should_build_snapshot(id, total)) { ++snaps_skipped_this_tick; return; }

            // Latency-tolerant per-bot setup. All three touch live core
            // mutating state (teleport, ground snap, motion master) and stay
            // on the world thread. Gated behind should_build_snapshot so they
            // run at the bot's tier rate. (Subsumes PERF-P1b + PERF-P3b.)
            if (p)
            {
                // Rescue ghost-BG bots BEFORE SnapToGroundIfDrifted — the
                // BG map's BIH can crash that call. RescueOrphanedBgBot is
                // a no-op for bots not in an orphaned BG map.
                RescueOrphanedBgBot(p);
                SnapToGroundIfDrifted(p);
                // Resolve the bot's bot logic ONCE here on the world thread (registry
                // quiescent under for_each's shared_lock) and thread it through
                // the BuiltSlot. Build workers MUST NOT call Registry().ai()
                // (unlocked map lookup races a concurrent rehash → garbage
                // pointer → AV). Same pointer GlobalStuckRescue used before.
                if (BotAI* ai = e.ai.get())
                    GlobalStuckRescue(p, ai);
            }

            // Bot is due to build this tick. Record it in registry order and
            // bucket it by its Map* pointer for the parallel partition. A bot
            // with no in-world Player still gets a slot so Build (which returns
            // {} for null/!IsInWorld) is invoked uniformly and the post-barrier
            // pass sees the same null result it would have serially.
            const size_t slot = build_order.size();
            build_order.push_back(BuiltSlot{ id, p, e.ai.get(), {} });
            Map const* m = p ? p->GetMap() : nullptr;
            by_map[m].push_back(slot);
        });
        t_snap_setup_ms += getMSTimeDiff(selectA_t0, getMSTime());
    }

    // Pre-assign a unique, monotonic version to each due bot BEFORE the
    // parallel region so the build tasks never touch the shared counter.
    // Contiguity is not required — only uniqueness + monotonicity — so a
    // single fetch_add reserving the whole block is sufficient and race-free.
    const SnapshotVer version_base =
        next_version.fetch_add(build_order.size(), std::memory_order_relaxed);

    // Decide serial vs. parallel for THIS tick. Read the kill-switch every
    // tick so the operator can flip it live. The pool is created + started
    // lazily on first parallel tick (a box that never enables pays nothing).
    const bool want_parallel =
        Services::Config().parallel_snapshot_build() && build_order.size() > 1 && by_map.size() > 1;
    if (want_parallel && !snapshot_build_pool_)
    {
        snapshot_build_pool_ = std::make_unique<Playerbot::SnapshotBuildPool>(
            Services::Config().snapshot_build_threads());
        snapshot_build_pool_->start();
        TC_LOG_INFO("playerbot.v2",
            "[PlayerbotV2] #5 Phase 4 parallel snapshot Build ENABLED "
            "(build workers={}, world thread participates).",
            snapshot_build_pool_->worker_count());
    }

    // Per-task accumulated build time, summed into t_snap_build_ms after the
    // barrier (the world-thread accumulators are NOT touched from workers).
    std::atomic<uint32> par_build_ms_acc{0};

    {
        const uint32 build_phase_t0 = getMSTime();

        // Build closure for one Map*'s slots [start,end). ONE task == one Map*
        // == one worker thread for the whole batch. This partition-by-map rule is
        // LOAD-BEARING for thread-safety, not just cache warmth:
        // BotSnapshotBuilder::Build runs Detour pathfinding (route-feasibility
        // probes → PathGenerator::CalculatePath → dtNavMeshQuery::findPath), and
        // TrinityCore keeps a SINGLE dtNavMeshQuery per (mapId, instanceId) that
        // is explicitly NOT thread-safe (MMapManager.h: "the returned
        // dtNavMeshQuery const* is NOT threadsafe"; MMapManager.cpp: "single
        // dtNavMeshQuery for every instance, since those are not thread safe").
        // findPath mutates that query's node pool / open list, so two workers
        // pathing for bots on the SAME map instance concurrently corrupts it and
        // BuildPolyPath spins forever → world thread hangs in run_and_wait → 60s
        // FreezeDetector abort (observed 2026-06-16 after a chunking experiment
        // split one map across workers; reverted). Keeping all of a map's bots on
        // one worker serializes that map's pathfinding, which is the invariant the
        // shared query requires. (start/end kept as params for the serial path;
        // the parallel path always passes the full range.)
        auto build_chunk =
            [&](std::vector<size_t> const& slots, size_t start, size_t end)
        {
            const uint32 part_t0 = getMSTime();
            for (size_t i = start; i < end; ++i)
            {
                BuiltSlot& bs = build_order[slots[i]];
                // Version is pre-assigned by slot index (unique + monotonic).
                const SnapshotVer ver = version_base + static_cast<SnapshotVer>(slots[i]);
                // Per-bot microsecond timing (steady_clock: getMSTime's 1 ms
                // grain reads 0 for most builds) for the TickPerf slowest-build
                // attribution after the barrier. Slot-local, so race-free.
                const auto bot_t0 = std::chrono::steady_clock::now();
                bs.snap = BotSnapshotBuilder::Build(bs.player, bs.ai, ver, tick);
                bs.build_us = static_cast<uint32>(std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - bot_t0).count());
            }
            par_build_ms_acc.fetch_add(
                getMSTimeDiff(part_t0, getMSTime()), std::memory_order_relaxed);
        };

        if (want_parallel && snapshot_build_pool_)
        {
            // Fan out ONE task per Map* on that map's STABLE sticky lane
            // (map_worker_slot_) so the builder's thread_local recycle/LoS pools
            // stay warm via ping-pong reuse AND each map-instance's shared
            // dtNavMeshQuery is only ever touched by one thread per batch. Do NOT
            // split a single map across lanes — see the build_chunk note above
            // (concurrent findPath on the shared per-instance query → freeze).
            std::vector<Playerbot::SnapshotBuildPool::LaneTask> tasks;
            tasks.reserve(by_map.size());
            for (auto& [m, slots] : by_map)
            {
                if (slots.empty()) continue;
                // Sticky lane for this Map*. New maps get the next round-robin
                // slot; despawned maps leave a harmless stale key.
                uint32 lane;
                auto it = map_worker_slot_.find(static_cast<void const*>(m));
                if (it != map_worker_slot_.end())
                    lane = it->second;
                else
                {
                    lane = next_worker_slot_++;
                    map_worker_slot_.emplace(static_cast<void const*>(m), lane);
                }
                // Capture a POINTER to the actual by_map element (which outlives
                // run_and_wait), NOT a reference to the per-iteration `slots`
                // binding — that would dangle once the loop iteration ends. by_map
                // is not mutated during the batch, so &slots is stable for the run.
                tasks.push_back({ lane,
                    [&build_chunk, sp = &slots]() {
                        build_chunk(*sp, 0, sp->size());
                    } });
            }
            snapshot_build_pool_->run_and_wait(tasks);
        }
        else
        {
            // Serial fallback (kill-switch off, or <=1 partition): build every
            // partition inline on the world thread. Identical behavior + same
            // thread_local pools (the world thread's) as the pre-Phase-4 path.
            for (auto& [m, slots] : by_map)
                build_chunk(slots, 0, slots.size());
        }

        t_snap_build_ms += par_build_ms_acc.load(std::memory_order_relaxed);
        // The build-phase wall time minus accounted per-partition build time
        // is fan-out/barrier overhead; fold it into setup so the phase sums
        // still reconcile against total. (Cheap; diagnostics only.)
        const uint32 build_phase_ms = getMSTimeDiff(build_phase_t0, getMSTime());
        const uint32 acc = par_build_ms_acc.load(std::memory_order_relaxed);
        if (build_phase_ms > acc)
            t_snap_setup_ms += (build_phase_ms - acc);
    }

    // ---- (D) POST-BARRIER WORLD-THREAD PASS (registry order) ------------
    // Tier classify + set_tier, group dedup/build + publish_group, per-bot
    // publish, perf. Every item here is must-stay-world-thread per the audit.
    {
        const uint32 publish_t0 = getMSTime();
        // Group-build sub-span is accumulated separately into t_group_build_ms
        // below; track it here too so it can be subtracted out of the publish
        // span when folding into t_snap_build_ms (keeps the TickPerf
        // build/group sub-buckets non-overlapping, as in the serial path).
        const uint32 group_ms_before = t_group_build_ms;
        // Companion tank threat multiplier (UpdateTankThreatMultiplier); 1.0 disables.
        const float tank_threat_mult = std::clamp(
            float(sConfigMgr->GetFloatDefault("Playerbot.TankThreatMultiplier", 3.0f, true)), 1.0f, 10.0f);
        const float dps_threat_mult = std::clamp(
            float(sConfigMgr->GetFloatDefault("Playerbot.DpsThreatMultiplier", 1.0f, true)), 0.3f, 1.5f);
        ConfigReader::CastPacing const cast_pacing_cfg = Services::Config().cast_pacing();
        uint32 const pvp_floor_ms = Services::Config().pacing().pvp_reaction_delay_ms;
        for (BuiltSlot& bs : build_order)
        {
            BotId          id   = bs.bot_id;
            Player*        p    = bs.player;
            auto&          snap = bs.snap;
            if (!snap)
                continue;   // Build returned {} (null/!IsInWorld) — nothing to publish.

            ++snaps_built_this_tick;
            if (p && bs.build_us > perf_slowest_build_us_)
            {
                perf_slowest_build_us_  = bs.build_us;
                perf_slowest_build_bot_ = p->GetName();
            }
            // Promote/demote the scheduler tier from the freshly built
            // snapshot. The world thread is the single writer to the
            // scheduler, so this is safe without synchronization.
            //
            // Tier 2.1 classifier. The OLD code pinned EVERY alive OOC bot to
            // Active (150 ms), so the snapshot-build throttle was effectively
            // dead (build_rate ~100%, snaps_skipped=0). The classifier below
            // keeps every bot that needs prompt reactivity on a fast cadence,
            // and lets the genuinely-AFK long tail fall to Idle → Parked:
            //
            //   dead                                  -> Idle    (500 ms)
            //   in combat                             -> Combat  (~100 ms)
            //   owner-controlled / grouped-with-real- -> Active  (150 ms)
            //     player / near-real-player /             (responsiveness-
            //     path-blocked / casting                   critical set)
            //   else, solo open-world, moving OR      -> Cruise  (300 ms)
            //     has-objective                           (dominant long-haul
            //                                              travel/quest pop;
            //                                              never ramps to Parked)
            //   alive, OOC, stationary, solo, no real -> Idle    (500 ms)
            //     player nearby, no objective            → ramps to Parked
            //                                             (Hibernate, 2 s)
            //                                             after N idle frames
            //                                             (handled in set_tier)
            //
            // Cruise (#5 b1) splits the old all-or-nothing Active gate: the
            // OLD code pinned every (moving OR has_objective) bot to Active
            // (150 ms), so a 230-bot questing fleet ran build_rate ~93%. Solo
            // travellers now build at half rate (300 ms) while the latency-
            // sensitive set above is untouched.
            //
            // Every input is an ALREADY-POPULATED snapshot field plus the
            // O(1) per-tick real-player set lookups built above — no new
            // per-bot grid search or builder cost. set_tier resets
            // next_snapshot=now on any tier change, so a bot that re-enters
            // combat / gets controlled / starts moving / has a real player
            // walk up rebuilds on the very next frame (no reactivity loss).
            ActivityTier tier;
            if (!snap->vitals.is_alive)
            {
                tier = ActivityTier::Idle;
            }
            else if (snap->vitals.in_combat)
            {
                tier = ActivityTier::Combat;
            }
            else
            {
                // "Owner-controlled" proxy: the bot is bound to an owner
                // (owner_name populated by the builder). Owned bots take
                // squad/whisper commands and follow the owner, so they must
                // stay responsive even while standing still.
                const bool owner_controlled = !snap->owner_name.empty();
                const bool grouped_with_real =
                    !snap->group.group_guid.IsEmpty() &&
                    real_player_groups.find(snap->group.group_guid) != real_player_groups.end();
                const bool near_real_player =
                    real_player_cells.find(PackPlayerCellKey(
                        snap->position.map_id,
                        PlayerCellCoord(snap->position.x),
                        PlayerCellCoord(snap->position.y))) != real_player_cells.end();
                const bool moving =
                    snap->movement.is_moving || snap->movement.is_swimming;

                // ACTIVE INTENT — a bot that wants to act but is momentarily
                // stationary must NOT be parked. is_moving alone mis-classified
                // these as idle (2026-06-01 regression): a path-BLOCKED bot
                // (stuck — its unstick recovery ladder must run at full cadence),
                // a bot chasing an unreached quest OBJECTIVE, and a bot mid-CAST
                // all have is_moving=false. Parking them starved the unstick
                // counters (they accrue inside the tier-gated snapshot build) and
                // froze them — the worker skips stale parked snapshots, so a
                // half-finished move stalled and a cast never completed (the
                // "kneel when idle" + "stuck casting" + worsened back-and-forth
                // the owner observed). Only a truly objective-less, unblocked,
                // non-casting idle AFK bot (the L80 city-filler tail) still falls
                // through to Idle->Parked, preserving the build-throttle win.
                const bool path_blocked  = snap->path_telemetry.count > 0;
                const bool has_objective = snap->quest_log.current_quest_id != 0;
                const bool casting       = snap->cast.is_casting;
                // INSTANCE GROUP RUN — a bot doing coordinated 5-man/raid content
                // must react at full cadence even while momentarily stationary
                // (a tank holding between pulls, a DPS waiting on a cohesion gate).
                // The tank-advance / cohesion rules read the SHARED GroupSnapshot,
                // which is only re-published to THIS bot on the ticks it is
                // processed. A parked dungeon tank demoted to Idle/Parked sees a
                // FROZEN group view — a healer that already ran up still reads
                // "healer_far 52y", a member whose combat tag already dropped still
                // reads "in combat" — so the advance gate blocks, the tank stays
                // parked, and the stale view never refreshes: a self-reinforcing
                // wedge (live 2026-06-28: pure-bot Deadmines squad stuck at the
                // entrance with all 5 stacked + out of combat while the gate read a
                // phantom far-healer / in-combat). Mirrors grouped_with_real, for
                // bot-only instance groups. Instances are bounded so the cost is
                // negligible. Open-world groups are NOT included (is_in_instance).
                const bool instance_group_run =
                    !snap->group.group_guid.IsEmpty() &&
                    snap->instance_ctx.is_in_instance;

                // RESPONSIVENESS-CRITICAL set keeps the full 150 ms Active
                // cadence:
                //   owner_controlled / grouped_with_real / near_real_player —
                //     human-facing: a real player is watching or depending on
                //     this bot, so it must react at full speed.
                //   path_blocked — the unstick recovery ladder accrues its
                //     counters inside the tier-gated snapshot build and must
                //     run at full cadence or the bot wedges (see the 2026-06-01
                //     regression note above).
                //   casting — a slower cadence can let a cast stall / never
                //     complete ("stuck casting" regression); keep it Active so
                //     casts finish.
                // (in_combat already routed to Combat above; dead to Idle.)
                const bool responsiveness_critical =
                    owner_controlled || grouped_with_real || near_real_player ||
                    path_blocked || casting || instance_group_run;

                if (responsiveness_critical)
                {
                    tier = ActivityTier::Active;
                }
                else if (moving || has_objective)
                {
                    // CRUISE (300 ms) — the dominant long-haul population: a
                    // solo, non-human-facing bot that is merely travelling or
                    // questing in the open world. Its movement spline runs
                    // server-side; the bot logic only needs to issue the next waypoint
                    // a touch less often, so half-rate snapshot builds are
                    // tolerable. NOT path_blocked and NOT casting (those are
                    // Active above), so no unstick/cast starvation. Cruise never
                    // ramps to Parked (set_tier resets the idle streak), so the
                    // bot can't freeze mid-travel; a block / combat / nearby
                    // real player promotes it to Active/Combat next frame.
                    tier = ActivityTier::Cruise;
                }
                else
                {
                    tier = ActivityTier::Idle;   // set_tier ramps to Parked after N
                }
            }
            sch.set_tier(id, tier, total);

            // Build a fresh group snapshot when the bot is in a group; clear
            // the slot otherwise so a former groupmate doesn't see stale data.
            // Dedup: reuse a cached snapshot keyed by the group's GUID (see
            // group_snap_cache_ above — at most kGroupSnapMaxAgeMs old, same
            // member count + leader). Multi-bot groups (parties, raids, full
            // BG raids) thereby pay the heavy GroupSnapshotBuilder::Build once
            // per window instead of once per bot. Solo / no-group bots get a
            // null publish. World-thread-only (RULE 5): the cache is a shared
            // cross-bot view and GroupSnapshotBuilder walks live group members.
            const uint32 group_t0 = getMSTime();
            if (Group const* g = p ? p->GetGroup() : nullptr)
            {
                ObjectGuid const ggid = g->GetGUID();
                auto cache_it = group_snap_cache_.find(ggid);
                if (cache_it != group_snap_cache_.end() &&
                    cache_it->second.snap->members.size() == g->GetMembersCount() &&
                    cache_it->second.snap->leader == g->GetLeaderGUID())
                {
                    // Cache hit — reuse the shared_ptr. publish_group takes
                    // a const shared_ptr by value so the refcount bumps and
                    // both bots end up pointing at the same snapshot.
                    pub.publish_group(id, cache_it->second.snap);
                }
                else
                {
                    auto gsnap = GroupSnapshotBuilder::Build(p, snap->version);
                    if (gsnap)
                    {
                        group_snap_cache_[ggid] = GroupSnapCacheEntry{ gsnap, getMSTime() };
                        pub.publish_group(id, gsnap);
                    }
                    else
                    {
                        pub.publish_group(id, {});
                    }
                }
            }
            else
            {
                pub.publish_group(id, {});
            }
            t_group_build_ms += getMSTimeDiff(group_t0, getMSTime());

            UpdateTankThreatMultiplier(id, p, bs.ai, *snap, tank_threat_mult, dps_threat_mult);
            UpdateCastSpeed(p, *snap, cast_pacing_cfg);
            UpdatePvpFloor(id, p, *snap, pvp_floor_ms, total);

            pub.publish(id, std::move(snap));
            Services::Perf().record_snapshot_publish();
        }
        // Publish/classify time is world-thread post-barrier cost; fold into
        // the build sub-bucket so the existing TickPerf line still attributes
        // it to the snapshot phase. Subtract the group sub-span (already in
        // t_group_build_ms) so build/group stay non-overlapping.
        const uint32 publish_span = getMSTimeDiff(publish_t0, getMSTime());
        const uint32 group_span   = t_group_build_ms - group_ms_before;
        t_snap_build_ms += (publish_span > group_span) ? (publish_span - group_span) : 0u;
    }

    t_snap_ms = getMSTimeDiff(t_snap_start_ms, getMSTime());

    // 1.5. Stagger-fire any deferred BG ports + LFG proposal accepts whose
    //      delay has elapsed (see OnBGInvitationReceived /
    //      OnLfgProposalReceived). MUST run before DrainIntents so the
    //      freshly-pushed intents fire this same tick.
    {
        const uint32 t0 = getMSTime();
        FireDueBgPorts();
        FireDueLfgAccepts();
        TopUpPendingLfg(getMSTime());
        t_bgport_ms = getMSTimeDiff(t0, getMSTime());
    }

    // 2. Drain pending intents and execute them via PlayerbotAPI on this
    //    (world) thread. The BotIntentExecutor file owns the variant visitor.
    {
        const uint32 t0 = getMSTime();
        intents_drained_this_tick = DrainIntents();
        t_drain_ms = getMSTimeDiff(t0, getMSTime());
    }

    // 2.5. Tick headless bot sessions so async login callbacks land and
    //      WorldSession query holders progress. Sessions are NOT in
    //      sWorld->m_sessions (would kick the GM's session sharing the
    //      account), so we drive their per-tick Update ourselves.
    {
        const uint32 t0 = getMSTime();
        Services::SessionMgr().Update(static_cast<uint32>(diff.count()));
        t_session_ms = getMSTimeDiff(t0, getMSTime());
    }

    // 2.6. Population shaper tick (Phase A of WORLD_POPULATION_PLAN).
    //      Internally rate-limits to its tick interval (default 60s) so
    //      this is cheap to call every world frame.
    {
        const uint32 t0 = getMSTime();
        Services::Population().OnWorldTick(static_cast<uint32>(total.count()));
        t_population_ms = getMSTimeDiff(t0, getMSTime());
    }

    // 2.7. Pets that cannot walk to their bot owner are set down next to it.
    DrivePetSnap(getMSTime());

    // 2b. Bot guild manager tick (GUILD_PLAN.md Phase A.2). Internally
    //     60s rate-limited; cheap to call every frame. Elects founders
    //     when guild slots are open + sweeps stale name reservations
    //     + aborts founders that exceeded the 30-min FSM budget.
    Services::Guilds().Tick(getMSTime());

    // 2c. Craft-order board tick (#4B-2). Ages out stale Claimed orders
    //     (timeout -> Fail + refund so escrow can't be trapped behind a stuck
    //     crafter) and prunes finished rows. Self-rate-limited cheaply via a
    //     local accumulator — the board only needs minute-granularity since
    //     the claim timeout is 30 min.
    craft_board_total_ += diff;
    if (craft_board_total_ - last_craft_board_at_ >= kCraftBoardIntervalMs)
    {
        last_craft_board_at_ = craft_board_total_;
        if (Services::Initialized())
            Services::CraftOrders().Tick(GameTime::GetGameTimeMS());
    }

    // Owner craft orders ("craft <item> x5" whispered to a bot): one pass a second.
    player_craft_total_ += diff;
    if (player_craft_total_ - last_player_craft_at_ >= std::chrono::milliseconds(1000))
    {
        last_player_craft_at_ = player_craft_total_;
        if (Services::Initialized())
            ::Playerbot::V2::PlayerCraftOrders::Tick(GameTime::GetGameTimeMS());
    }

    // 3. Schedule tick for due bots.
    {
        const uint32 t0 = getMSTime();
        Services::Scheduler().on_world_tick(total);
        t_scheduler_ms = getMSTimeDiff(t0, getMSTime());
    }

    // 3b. Runtime wedge watchdog (#1B). Slow cadence (kWedgeWatchdogIntervalMs)
    //     because it only reads cheap per-bot fields + the eventually-consistent
    //     snapshot. Walks the registry, classifies each in-world bot's wedge
    //     state, emits ONE structured [wedge] line per stuck episode, records
    //     the category in PerfCounters, and rebuilds the active-wedge list the
    //     `.playerbot wedges` digest reads. World-thread only — same lifecycle
    //     slot as GlobalStuckRescue + the fleet log.
    wedge_wd_total_ += diff;
    if (wedge_wd_total_ - last_wedge_wd_at_ >= kWedgeWatchdogIntervalMs)
    {
        last_wedge_wd_at_ = wedge_wd_total_;
        wedge_watchdog_.Tick(GameTime::GetGameTimeMS());
    }

    // 3c. Group-level no-progress DIAGNOSE watchdog (nav-robustness program).
    //     READ-ONLY: classifies a wedged dungeon group (stranded / split /
    //     cohered-idle / false-combat) and emits one [group_wedge] line — the
    //     "diagnose" stage the remediation pass is built on. Self-throttled, so
    //     it's safe to call every world tick.
    group_wedge_watchdog_.Update(GameTime::GetGameTimeMS());

    // 4. Periodic fleet-status log (overnight friendly). Walks the registry +
    //    snapshots once per kFleetLogIntervalMs and emits one INFO line so the
    //    operator can see fleet health by tailing worldserver.log without
    //    needing a GM session. Cheap aggregation; capped to a single log
    //    line so it doesn't spam.
    constexpr std::chrono::milliseconds kFleetLogIntervalMs{60 * 1000};   // 60s â€” gives readable TickPerf data without log spam
    fleet_log_total_ += diff;
    if (fleet_log_total_ - last_fleet_log_at_ >= kFleetLogIntervalMs)
    {
        last_fleet_log_at_ = fleet_log_total_;
        uint32 in_world = 0, alive = 0, in_combat = 0, with_quest = 0, stuck = 0;
        uint32 levels_total = 0, level_max = 0;
        uint32 quests_total = 0;
        const uint32 now_ms = GameTime::GetGameTimeMS();
        // Last rule each bot fired, counted per rule (raid rules excluded): shows which of AH, mail, hub, travel and quest rules
        // the world population is actually running. last_rule_fired() points at a string literal, so the pointer is the key.
        std::unordered_map<std::string_view, uint32> rule_census;
        Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
        {
            if (!e.ai) return;
            ++in_world;
            if (char const* rule = e.ai->last_rule_fired(); rule && std::string_view(rule).rfind("raid", 0) != 0)
                ++rule_census[rule];
            if (auto snap = Services::Snapshots().latest(id))
            {
                if (snap->vitals.is_alive)  ++alive;
                if (snap->vitals.in_combat) ++in_combat;
                if (snap->identity.level > level_max) level_max = snap->identity.level;
                levels_total += snap->identity.level;
                if (snap->quest_log.current_quest_id != 0) ++with_quest;
                quests_total += static_cast<uint32>(snap->quest_log.quests.size());
            }
            // Currently blacklisted = deadline in future. Bots whose timer
            // has expired aren't actively stuck (they're allowed to retry).
            const uint32 dead = e.ai->objective_track().blacklisted_until_ms;
            if (dead != 0 && dead > now_ms) ++stuck;
        });
        {
            std::vector<std::pair<std::string_view, uint32>> ranked(rule_census.begin(), rule_census.end());
            std::sort(ranked.begin(), ranked.end(), [](auto const& a, auto const& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
            std::string line;
            for (size_t i = 0; i < ranked.size() && i < 40; ++i)
                line += fmt::format("{}{}={}", i ? ", " : "", ranked[i].first, ranked[i].second);
            TC_LOG_INFO("playerbot.v2", "[rule_census] bots={} rules={} | {}", in_world, ranked.size(), line);
        }
        const uint32 avg_level = in_world > 0 ? levels_total / in_world : 0;
        TC_LOG_INFO("playerbot.v2",
            "[PlayerbotV2] FleetStatus: in_world={} alive={} in_combat={} avg_lvl={} max_lvl={} "
            "with_current_quest={} quests_in_log={} stuck_bots={} marked={}",
            in_world, alive, in_combat, avg_level, level_max,
            with_quest, quests_total, stuck, Services::Lifecycle().size());

        // #1C fleet-vitals sample (rolling window + persistence + alerting).
        // Runs on the SAME 60s cadence; reuses the census just aggregated so
        // the registry isn't walked twice. avg_level passed x100 fixed-point.
        const uint32 avg_level_x100 = in_world > 0
            ? static_cast<uint32>((uint64(levels_total) * 100ull) / in_world)
            : 0u;
        SampleFleetVitals(total, now_ms, in_world, alive, in_combat, avg_level_x100);

        if (sConfigMgr->GetBoolDefault("Playerbot.LoadTest.Enable", false, true))
        {
            uint32 raid = 0, bg = 0;
            Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
            {
                if (!e.ai) return;
                if (Player const* bp = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(id)))
                {
                    if (bp->InBattleground()) ++bg;
                    else if (Group const* g = bp->GetGroup(); g && g->isRaidGroup()) ++raid;
                }
            });
            EmitLoadTestRow(in_world, raid, bg);
        }
        loadtest_hist_.Reset();

        // Per-phase tick-cost averages + worst-case latency for the window.
        // Sum-of-phases vs total reveals unaccounted-for cost (TC core,
        // script callbacks, other modules). Aim: total avg < 100ms, max
        // < 200ms at 2000-bot scale. Reset window after emit.
        if (perf_ticks_in_window_ > 0)
        {
            const uint32 n = perf_ticks_in_window_;
            const uint32 sum_phases =
                perf_snap_ms_total_ + perf_bgport_ms_total_ +
                perf_drain_ms_total_ + perf_session_ms_total_ +
                perf_pop_ms_total_ + perf_sched_ms_total_;
            TC_LOG_INFO("playerbot.v2",
                "[PlayerbotV2] TickPerf (avg ms over {} ticks): "
                "snap={:.2f} (setup={:.2f} build={:.2f} group={:.2f}) "
                "bgport={:.2f} drain={:.2f} session={:.2f} "
                "pop={:.2f} sched={:.2f} | phases_sum={:.2f} total={:.2f} "
                "max={}ms",
                n,
                double(perf_snap_ms_total_) / n,
                double(perf_snap_setup_ms_total_) / n,
                double(perf_snap_build_ms_total_) / n,
                double(perf_group_build_ms_total_) / n,
                double(perf_bgport_ms_total_) / n,
                double(perf_drain_ms_total_) / n,
                double(perf_session_ms_total_) / n,
                double(perf_pop_ms_total_) / n,
                double(perf_sched_ms_total_) / n,
                double(sum_phases) / n,
                double(perf_total_ms_total_) / n,
                perf_total_ms_max_);

            // Throughput counters — exposes "how much work happened" vs the
            // ms timings above which show "how long it took". A high
            // snap_built rate at a low snap_build_ms means Builder is fast;
            // a low snap_built rate at a high snap_build_ms means a few
            // bots are dominating cost. Same for intents.
            const uint32 snap_total = perf_snap_built_count_ + perf_snap_skipped_count_;
            TC_LOG_INFO("playerbot.v2",
                "[PlayerbotV2] TickPerf throughput: snaps_built={} snaps_skipped={} "
                "(build_rate={:.1f}%) intents_drained={} ({:.1f}/tick)",
                perf_snap_built_count_, perf_snap_skipped_count_,
                snap_total > 0 ? 100.0 * double(perf_snap_built_count_) / double(snap_total) : 0.0,
                perf_intents_drained_,
                double(perf_intents_drained_) / double(n));

            // Spike attribution. The averages above hide a once-per-period
            // spike, so name it: the phase breakdown of the single worst tick,
            // each phase's own worst case, and the slowest single bot build /
            // drained intent batch of the window.
            {
                WorstTickBreakdown const& w = perf_worst_tick_;
                TC_LOG_INFO("playerbot.v2",
                    "[PlayerbotV2] TickPerf max (ms): total={} worst_tick[snap={} (setup={} build={} group={}) "
                    "bgport={} drain={} session={} pop={} sched={} other={} built={} drained={}] "
                    "phase_max[snap={} (setup={} build={} group={}) bgport={} drain={} session={} "
                    "pop={} sched={} other={}]",
                    perf_total_ms_max_,
                    w.snap, w.setup, w.build, w.group, w.bgport, w.drain, w.session, w.pop, w.sched,
                    w.other, w.snaps_built, w.intents_drained,
                    perf_snap_ms_max_, perf_snap_setup_ms_max_, perf_snap_build_ms_max_,
                    perf_group_build_ms_max_, perf_bgport_ms_max_, perf_drain_ms_max_,
                    perf_session_ms_max_, perf_pop_ms_max_, perf_sched_ms_max_, perf_other_ms_max_);
                TC_LOG_INFO("playerbot.v2",
                    "[PlayerbotV2] TickPerf slowest: build={} {:.2f}ms | drain_batch={} {:.2f}ms "
                    "intents={} | intent={} {:.2f}ms",
                    perf_slowest_build_bot_.empty() ? "-" : perf_slowest_build_bot_.c_str(),
                    double(perf_slowest_build_us_) / 1000.0,
                    perf_slowest_drain_bot_.empty() ? "-" : perf_slowest_drain_bot_.c_str(),
                    double(perf_slowest_drain_us_) / 1000.0,
                    perf_slowest_drain_count_,
                    (perf_slowest_intent_kind_ && *perf_slowest_intent_kind_) ? perf_slowest_intent_kind_ : "-",
                    double(perf_slowest_intent_us_) / 1000.0);
            }

            // BG advice cache hit rate. Sustained hit-rate < 90% means a
            // script's snapshot consumption isn't in the cache key and
            // advice is rebuilding too often. Total grows with BG-bot
            // count × ticks, so high totals are expected at scale; the
            // RATE is the signal.
            auto perf_snap_diag = Playerbot::Services::Initialized()
                ? Playerbot::Services::Perf().snapshot()
                : Playerbot::PerfCounters::Snapshot{};
            uint64_t adv_hits   = perf_snap_diag.bg_advice_cache_hits_total;
            uint64_t adv_misses = perf_snap_diag.bg_advice_cache_misses_total;
            uint64_t adv_total  = adv_hits + adv_misses;
            TC_LOG_INFO("playerbot.v2",
                "[PlayerbotV2] BgAdviceCache: hits={} misses={} (hit_rate={:.1f}%)",
                adv_hits, adv_misses,
                adv_total > 0 ? 100.0 * double(adv_hits) / double(adv_total) : 0.0);

            perf_ticks_in_window_     = 0;
            perf_snap_ms_total_       = 0;
            perf_snap_setup_ms_total_ = 0;
            perf_snap_build_ms_total_ = 0;
            perf_group_build_ms_total_ = 0;
            perf_snap_built_count_    = 0;
            perf_snap_skipped_count_  = 0;
            perf_intents_drained_     = 0;
            perf_bgport_ms_total_     = 0;
            perf_drain_ms_total_      = 0;
            perf_session_ms_total_    = 0;
            perf_pop_ms_total_        = 0;
            perf_sched_ms_total_      = 0;
            perf_total_ms_total_      = 0;
            perf_total_ms_max_        = 0;
            perf_snap_ms_max_         = 0;
            perf_snap_setup_ms_max_   = 0;
            perf_snap_build_ms_max_   = 0;
            perf_group_build_ms_max_  = 0;
            perf_bgport_ms_max_       = 0;
            perf_drain_ms_max_        = 0;
            perf_session_ms_max_      = 0;
            perf_pop_ms_max_          = 0;
            perf_sched_ms_max_        = 0;
            perf_other_ms_max_        = 0;
            perf_worst_tick_          = WorstTickBreakdown{};
            perf_slowest_build_us_    = 0;
            perf_slowest_build_bot_.clear();
            perf_slowest_drain_us_    = 0;
            perf_slowest_drain_count_ = 0;
            perf_slowest_drain_bot_.clear();
            perf_slowest_intent_us_   = 0;
            perf_slowest_intent_kind_ = "";
        }
    }

    // Record world-update wall-clock latency for diagnostics.
    const uint32 elapsed = getMSTimeDiff(tick_start_ms, getMSTime());
    Services::Perf().record_world_update_latency(std::chrono::milliseconds{elapsed});

    // Accumulate per-phase costs for the next FleetStatus emit window.
    ++perf_ticks_in_window_;
    perf_snap_ms_total_       += t_snap_ms;
    perf_snap_setup_ms_total_ += t_snap_setup_ms;
    perf_snap_build_ms_total_ += t_snap_build_ms;
    perf_group_build_ms_total_ += t_group_build_ms;
    perf_snap_built_count_    += snaps_built_this_tick;
    perf_snap_skipped_count_  += snaps_skipped_this_tick;
    perf_intents_drained_     += static_cast<uint32>(intents_drained_this_tick);
    perf_bgport_ms_total_  += t_bgport_ms;
    perf_drain_ms_total_   += t_drain_ms;
    perf_session_ms_total_ += t_session_ms;
    perf_pop_ms_total_     += t_population_ms;
    perf_sched_ms_total_   += t_scheduler_ms;
    perf_total_ms_total_   += elapsed;
    loadtest_hist_.Add(elapsed);

    // Spike attribution: per-phase worst case + the worst tick's breakdown.
    const uint32 phases_ms = t_snap_ms + t_bgport_ms + t_drain_ms + t_session_ms +
                             t_population_ms + t_scheduler_ms;
    const uint32 other_ms  = elapsed > phases_ms ? elapsed - phases_ms : 0u;
    perf_snap_ms_max_        = std::max(perf_snap_ms_max_,        t_snap_ms);
    perf_snap_setup_ms_max_  = std::max(perf_snap_setup_ms_max_,  t_snap_setup_ms);
    perf_snap_build_ms_max_  = std::max(perf_snap_build_ms_max_,  t_snap_build_ms);
    perf_group_build_ms_max_ = std::max(perf_group_build_ms_max_, t_group_build_ms);
    perf_bgport_ms_max_      = std::max(perf_bgport_ms_max_,      t_bgport_ms);
    perf_drain_ms_max_       = std::max(perf_drain_ms_max_,       t_drain_ms);
    perf_session_ms_max_     = std::max(perf_session_ms_max_,     t_session_ms);
    perf_pop_ms_max_         = std::max(perf_pop_ms_max_,         t_population_ms);
    perf_sched_ms_max_       = std::max(perf_sched_ms_max_,       t_scheduler_ms);
    perf_other_ms_max_       = std::max(perf_other_ms_max_,       other_ms);
    if (elapsed > perf_total_ms_max_)
    {
        perf_total_ms_max_ = elapsed;
        WorstTickBreakdown& w = perf_worst_tick_;
        w.snap    = t_snap_ms;       w.setup   = t_snap_setup_ms;
        w.build   = t_snap_build_ms; w.group   = t_group_build_ms;
        w.bgport  = t_bgport_ms;     w.drain   = t_drain_ms;
        w.session = t_session_ms;    w.pop     = t_population_ms;
        w.sched   = t_scheduler_ms;  w.other   = other_ms;
        w.snaps_built     = snaps_built_this_tick;
        w.intents_drained = static_cast<uint32>(intents_drained_this_tick);
    }
}

// One load-test CSV row for the minute just ended (Playerbot.LoadTest.Enable). Appends to Playerbot.LoadTest.CsvPath
// (default playerbot_loadtest.csv beside the server), writing the header first when the file is new. The row carries its own
// PASS/FAIL against Playerbot.LoadTest.Max* so one glance at the last column says whether the build holds the scenario.
void V2::Module::EmitLoadTestRow(uint32 in_world, uint32 raid, uint32 bg)
{
    if (!loadtest_hist_.Count())
        return;
    Diagnostics::LoadRow row;
    row.unix_time = uint64(GameTime::GetGameTime());
    row.build = GitRevision::GetHash();
    row.label = sConfigMgr->GetStringDefault("Playerbot.LoadTest.Label", "", true);
    row.bots_awake = in_world;
    row.bots_raid = raid;
    row.bots_bg = bg;
    Fleet::WorldLoadFacts const wf = Services::Population().WorldLoad();
    row.bots_world = wf.awake;
    row.bots_sleeping = wf.sleeping;
    row.ticks = loadtest_hist_.Count();
    row.tick_avg_ms = loadtest_hist_.Avg();
    row.tick_p50_ms = loadtest_hist_.Percentile(50);
    row.tick_p99_ms = loadtest_hist_.Percentile(99);
    row.tick_max_ms = loadtest_hist_.Max();
    SlowCallWorst const worst = SlowCallWorst::Instance().Take();
    row.slowest_call = worst.name ? worst.name : "";
    row.slowest_call_ms = worst.ms;
    row.rss_mb = wf.rss_mb;
    row.cpu_pct = wf.cpu_pct;

    Diagnostics::LoadLimits limits;
    limits.max_p99_ms = uint32(sConfigMgr->GetIntDefault("Playerbot.LoadTest.MaxP99Ms", int(limits.max_p99_ms), true));
    limits.max_avg_ms = double(sConfigMgr->GetIntDefault("Playerbot.LoadTest.MaxAvgMs", int(limits.max_avg_ms), true));
    limits.max_rss_mb = uint32(sConfigMgr->GetIntDefault("Playerbot.LoadTest.MaxRssMb", int(limits.max_rss_mb), true));
    limits.max_slow_call_ms = uint32(sConfigMgr->GetIntDefault("Playerbot.LoadTest.MaxSlowCallMs", int(limits.max_slow_call_ms), true));

    std::string const path = sConfigMgr->GetStringDefault("Playerbot.LoadTest.CsvPath", "playerbot_loadtest.csv", true);
    bool const fresh = !std::filesystem::exists(path);
    std::ofstream out(path, std::ios::app);
    if (!out)
    {
        TC_LOG_ERROR("playerbot.v2", "[loadtest] cannot append to '{}'", path);
        return;
    }
    if (fresh)
        out << Diagnostics::CsvHeader() << '\n';
    out << Diagnostics::CsvRow(row, limits) << '\n';
    TC_LOG_INFO("playerbot.v2", "[loadtest] {}", Diagnostics::CsvRow(row, limits));
}

V2::Module::TickPerfSnapshot V2::Module::tickperf_snapshot() const
{
    TickPerfSnapshot s{};
    s.ticks_in_window      = perf_ticks_in_window_;
    s.snap_ms_total        = perf_snap_ms_total_;
    s.snap_setup_ms_total  = perf_snap_setup_ms_total_;
    s.snap_build_ms_total  = perf_snap_build_ms_total_;
    s.group_build_ms_total = perf_group_build_ms_total_;
    s.snap_built_count     = perf_snap_built_count_;
    s.snap_skipped_count   = perf_snap_skipped_count_;
    s.intents_drained      = perf_intents_drained_;
    s.bgport_ms_total      = perf_bgport_ms_total_;
    s.drain_ms_total       = perf_drain_ms_total_;
    s.session_ms_total     = perf_session_ms_total_;
    s.pop_ms_total         = perf_pop_ms_total_;
    s.sched_ms_total       = perf_sched_ms_total_;
    s.total_ms_total       = perf_total_ms_total_;
    s.total_ms_max         = perf_total_ms_max_;
    return s;
}

// #1C fleet-vitals sampler. World-thread, 60s cadence (driven from the
// FleetStatus block). Builds one VitalsBucket from the supplied census + the
// live PerfCounters / wedge-watchdog state, pushes it into the in-memory
// rolling window, async-persists one row (survives restart -> real trend), and
// evaluates the config-driven alert thresholds with per-metric throttling.
void Module::SampleFleetVitals(std::chrono::milliseconds total, uint32 now_ms,
                               uint32 in_world, uint32 alive, uint32 in_combat,
                               uint32 avg_level_x100)
{
    if (!Services::Initialized())
        return;

    PerfCounters& perf = Services::Perf();
    PerfCounters::Snapshot const ps = perf.snapshot();

    // ---- Derive per-window RATES from cumulative-counter deltas. ----
    // Path-fail = NoPath + FarFromPolyStart + FarFromPolyEnd (PathOutcome
    // indices 1/2/3; the array is fixed at 6 — guarded below).
    uint64 path_fail_total = 0;
    if (ps.path_outcomes.size() >= 4)
        path_fail_total = ps.path_outcomes[1] + ps.path_outcomes[2] + ps.path_outcomes[3];
    const uint64 intents_exec_total = ps.intents_executed_total;
    const uint64 intents_drop_total = ps.intents_dropped_total;

    // Elapsed wall-clock since the previous sample (ms). First sample has no
    // baseline -> rates emit 0 and we just seed the cumulative anchors.
    const uint64 elapsed_ms = vitals_sample_ready_
        ? static_cast<uint64>((total - last_vitals_sample_at_).count())
        : 0ull;

    uint32 path_fail_per_min = 0;
    uint32 intents_per_sec   = 0;
    uint32 intents_dropped   = 0;   // delta over the window
    uint32 intents_executed  = 0;   // delta over the window
    if (vitals_sample_ready_ && elapsed_ms > 0)
    {
        const uint64 d_pathfail = path_fail_total    >= last_path_fail_total_
            ? path_fail_total - last_path_fail_total_ : 0;
        const uint64 d_exec     = intents_exec_total >= last_intents_exec_total_
            ? intents_exec_total - last_intents_exec_total_ : 0;
        const uint64 d_drop     = intents_drop_total >= last_intents_drop_total_
            ? intents_drop_total - last_intents_drop_total_ : 0;

        path_fail_per_min = static_cast<uint32>((d_pathfail * 60000ull) / elapsed_ms);
        intents_per_sec   = static_cast<uint32>((d_exec * 1000ull) / elapsed_ms);
        intents_dropped   = static_cast<uint32>(d_drop);
        intents_executed  = static_cast<uint32>(d_exec);
    }

    // Roll the cumulative anchors forward for the next sample.
    last_path_fail_total_    = path_fail_total;
    last_intents_exec_total_ = intents_exec_total;
    last_intents_drop_total_ = intents_drop_total;
    last_vitals_sample_at_   = total;
    vitals_sample_ready_     = true;

    // ---- Wedge census from the watchdog's most-recent active list. ----
    PerfCounters::VitalsBucket b;
    b.sample_at_ms = now_ms;
    b.in_world  = in_world;
    b.alive     = alive;
    b.in_combat = in_combat;
    {
        auto const& active = wedge_watchdog_.active();
        b.wedged = static_cast<uint32>(active.size());
        for (auto const& wi : active)
        {
            const size_t ci = static_cast<size_t>(wi.cat);
            if (ci < b.wedged_by_category.size())
                ++b.wedged_by_category[ci];
        }
    }
    b.path_fail_per_min = path_fail_per_min;
    b.intents_per_sec   = intents_per_sec;
    b.intents_dropped   = intents_dropped;
    b.intents_executed  = intents_executed;
    b.tick_p50_us       = perf.tick_latency_percentile_us(0.50);
    b.tick_p99_us       = perf.tick_latency_percentile_us(0.99);
    b.avg_level_x100    = avg_level_x100;

    // ---- Push into the in-memory rolling window. ----
    perf.push_vitals_bucket(b);

    // ---- Persist one row (async; survives restart -> real trend). ----
    // CharacterDatabase hosts the playerbot_v2_* schema (see
    // PlayerbotMigrationMgr::Db()), so the table is referenced unqualified.
    // Execute() is non-blocking; ordering doesn't matter for an append-only
    // sample table. avg_level written as a float (x100 -> /100.0).
    CharacterDatabase.Execute(fmt::format(
        "INSERT INTO playerbot_v2_fleet_vitals_sample "
        "(in_world, alive, in_combat, wedged, "
        " wedged_navmesh, wedged_offmesh, wedged_travel, "
        " wedged_combatloop, wedged_pickernone, wedged_goalunreach, "
        " tick_p50_us, tick_p99_us, intents_per_sec, intents_dropped, "
        " path_fail_per_min, avg_level) "
        "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {:.2f})",
        b.in_world, b.alive, b.in_combat, b.wedged,
        b.wedged_by_category[static_cast<size_t>(Diagnostics::WedgeCategory::Navmesh)],
        b.wedged_by_category[static_cast<size_t>(Diagnostics::WedgeCategory::OffMesh)],
        b.wedged_by_category[static_cast<size_t>(Diagnostics::WedgeCategory::Travel)],
        b.wedged_by_category[static_cast<size_t>(Diagnostics::WedgeCategory::CombatLoop)],
        b.wedged_by_category[static_cast<size_t>(Diagnostics::WedgeCategory::PickerNone)],
        b.wedged_by_category[static_cast<size_t>(Diagnostics::WedgeCategory::GoalUnreachable)],
        b.tick_p50_us, b.tick_p99_us, b.intents_per_sec, b.intents_dropped,
        b.path_fail_per_min, double(b.avg_level_x100) / 100.0).c_str());

    // ---- Alerting: throttled [fleet_alert] on threshold breach. ----
    // A threshold of 0 disables that metric. The first sample (no rate
    // baseline) still alerts on the gauge metrics (wedged / tick p99) but the
    // rate metrics are 0 so they can't false-fire.
    ConfigReader const& cfg = Services::Config();
    const uint32 throttle_ms = cfg.alert_throttle_ms();
    auto fire = [&](uint32& last_ms, uint32 thresh, uint32 value,
                    char const* metric, char const* unit)
    {
        if (thresh == 0 || value <= thresh)
            return;
        // Per-metric throttle: at most one line per throttle window.
        if (last_ms != 0 && throttle_ms != 0 && (now_ms - last_ms) < throttle_ms)
            return;
        last_ms = now_ms;
        TC_LOG_ERROR("playerbot.v2",
            "[fleet_alert] {} breached: value={}{} threshold={}{} "
            "(in_world={} wedged={} tick_p99={}us intents_dropped/win={} path_fail/min={})",
            metric, value, unit, thresh, unit,
            b.in_world, b.wedged, b.tick_p99_us, b.intents_dropped, b.path_fail_per_min);
    };
    fire(last_alert_wedged_ms_,   cfg.alert_wedged_bots(),        b.wedged,            "wedged_bots",       "");
    fire(last_alert_p99_ms_,      cfg.alert_tick_p99_us(),        b.tick_p99_us,       "tick_p99_us",       "us");
    fire(last_alert_drop_ms_,     cfg.alert_intent_drop_per_min(),b.intents_dropped,   "intent_drop/win",   "");
    fire(last_alert_pathfail_ms_, cfg.alert_path_fail_per_min(),  b.path_fail_per_min, "path_fail/min",     "");
}

// Hook handler bodies â€” concrete for login/logout so a connected real player
// (or eventually a bot character) registers with the bot registry.
//
// Note: these handlers fire for ALL Player logins, not just bots. For now,
// every connected player gets a registry entry â€” once Fleet::BotLifecycleManager
// lands, registration is gated to characters flagged as bots.

// ---- Hook handler stubs ---------------------------------------------------
// Every hook is wired but inert until the corresponding subsystem lands.
// Each one will dispatch to a Fleet or Bot-layer component per FEATURE_MATRIX.md.

void Module::OnPlayerLogin(Player* p)
{
    if (!initialized_ || !p) return;
    const BotId id = p->GetGUID().GetCounter();

    // Real players are not driven by V2 but DO get owner-login greetings
    // from any of their currently-online bots. Bots queue a one-shot
    // WhisperIntent (executed on the next world-thread drain) so real
    // players see "Welcome back, <player>!" from their squad on relog â€”
    // small touch that makes the ownerâ†’bot relationship feel alive.
    if (!Services::Lifecycle().is_bot(id))
    {
        if (WorldSession const* sess = p->GetSession();
            sess && !sess->IsBot())
        {
            const uint32 owner_account = sess->GetAccountId();
            // Bots bound to the whole account treat this character as their owner while it is online.
            Services::Owners().NoteOwnerCharacterOnline(owner_account, id);
            if (Services::Config().addon_hint_enabled())
                g_addon_hints.Login(id, GameTime::GetGameTimeMS(), Services::Config().addon_hint_delay_sec() * 1000);
            auto owned = Services::Owners().BotsOwnedBy(owner_account);
            std::string const& owner_name = p->GetName();
            for (BotId bot_id : owned)
            {
                if (!Services::Registry().has(bot_id)) continue;  // not online
                Player* obot = ObjectAccessor::FindConnectedPlayer(
                    ObjectGuid::Create<HighGuid::Player>(bot_id));
                if (!obot) continue;
                // Queue via the bot's intent queue â€” runs on the next
                // world-tick drain (so we don't whisper inside the login
                // path while the new player's session is still mid-init).
                if (Services::HasIntents(bot_id))
                {
                    Intent it{};
                    it.bot_id = bot_id;
                    it.body = ChatIntent{WhisperIntent{
                        owner_name,
                        std::string("Welcome back!")}};
                    Services::Intents(bot_id).push(std::move(it));
                }
            }
        }

        // Restart resilience: a server restart logs every bot out, and the
        // world-population pipeline respawns the fleet in arbitrary order —
        // a human's dungeon group could sit half-empty for many minutes
        // (2026-06-11: user back in Deadmines, tank + leader offline, run
        // dead). Group membership persists in group_member, so when the
        // human logs back in, spawn-login every offline bot member of their
        // group immediately. LoginBot is idempotent (refuses while a login
        // is in flight), so racing the population pipeline is harmless.
        if (Group const* g = p->GetGroup())
        {
            for (Group::MemberSlot const& slot : g->GetMemberSlots())
            {
                const BotId member_id = slot.guid.GetCounter();
                if (member_id == id) continue;
                if (!Services::Lifecycle().is_bot(member_id)) continue;
                if (ObjectAccessor::FindConnectedPlayer(slot.guid)) continue;
                auto res = Services::SessionMgr().LoginBot(slot.guid);
                TC_LOG_INFO("playerbot.v2",
                    "[GroupRelogin] {} logged in with offline bot groupmate {} — spawn-login {}",
                    p->GetName(), slot.name, res.ok ? "submitted" : res.reason);
            }
        }
        return;
    }
    if (Services::Registry().has(id)) return;

    // Raid relog placement (2026-10-07, MC wipe 17:16): a restart logs the whole group out wherever its last fight ended, so the bots
    // log back in on the spot they were standing at, next to a pack that has since reset to its spawn, while the leader logs in at
    // the entrance. The 40 bots pulled three Firelords, an Annihilator and a hound onto themselves with no tank in reach. A bot that
    // logs into a raid map joins its human group leader there, like a raid zoning in together; once, at login, never afterwards.
    if (Map* map = p->GetMap(); map && map->IsRaid() && p->IsInWorld())
    {
        if (Group const* g = p->GetGroup())
        {
            Player* human = nullptr;
            for (GroupReference const& ref : g->GetMembers())
            {
                Player* m = ref.GetSource();
                if (m && m != p && m->IsInWorld() && m->GetMap() == map && m->GetSession() && !m->GetSession()->IsBot()
                    && (!human || m->GetGUID() == g->GetLeaderGUID()))
                    human = m;
            }
            if (human && p->GetExactDist(human) > 30.0f)
            {
                TC_LOG_INFO("playerbot.v2", "[raid_login] {} logged into map {} {:.0f} y from {} ({:.0f},{:.0f},{:.0f}): placed beside them",
                    p->GetName(), map->GetId(), p->GetExactDist(human), human->GetName(), p->GetPositionX(), p->GetPositionY(), p->GetPositionZ());
                p->NearTeleportTo(human->GetPositionX(), human->GetPositionY(), human->GetPositionZ(), human->GetOrientation());
            }
        }
    }

    // Real clients drive Player::CanNeverSee past its first guard by sending
    // CMSG_MOVE_INIT_ACTIVE_MOVER_COMPLETE, which sets PLAYER_LOCAL_FLAG_OVERRIDE_TRANSPORT_SERVER_TIME.
    // Without it, Player::CanNeverSee always returns true, CanSeeOrDetect
    // always returns false, and every offensive spell on a creature fails
    // SPELL_FAILED_BAD_TARGETS â€” IsValidAttackTarget can't see the target.
    // Bots have no client to send that opcode, so set the flag here once the
    // bot is fully in-world. (CharacterHandler.cpp:1176 is the equivalent set
    // for real players, gated on the time-sync queue which never populates
    // for sessionless bots.)
    p->SetPlayerLocalFlag(PLAYER_LOCAL_FLAG_OVERRIDE_TRANSPORT_SERVER_TIME);

    // Defense and the weapon skills at the level's maximum on every login: a bot created or levelled before the skills were
    // raised with the level (companions of an older build, world bots) otherwise keeps the skill-ups it happened to earn (a
    // warrior at Defense 181, a paladin at 52) and takes crushing blows on most swings from a boss.
    ::Playerbot::API(p).learn_weapon_skills_free();

    // Take off anything the client cannot draw BEFORE other players get a
    // chance to inspect this bot. Without it a bot that logs in wearing such an
    // item stays a client-crash hazard until the next 5-min gear-backfill pass.
    Services::Population().HealUnrenderableGear(p);

    // Cross-map travel on-ramp (2026-06-16): ensure the bot KNOWS its faction's
    // flight paths. The travel graph prunes every taxi edge to a flight master
    // NOT in the bot's taximask (UnifiedTravelGraph EdgeUsable → IsTaximaskNodeKnown),
    // so a bot with an empty mask sees an EDGELESS flight-master subgraph and
    // FindRoute returns 0 legs — the ~87% "both ends attached, legs=0" route
    // failures that blocked all cross-continent travel/relocation. The setup
    // pipeline only learned at SETUP and skipped sub-10 bots, so every
    // already-distributed bot had an empty mask. LearnAllFactionFlightpaths is
    // idempotent (SetTaximaskNode only flips unset bits) + cheap (~600 nodes), and
    // the mask persists in the character taximask column, so a one-time learn on
    // login repairs the whole fleet. (Sub-10 bots stay same-map via the relocation
    // band gate regardless, so a full mask doesn't send them cross-continent.)
    if (uint32 fp = ::Playerbot::V2::World::LearnAllFactionFlightpaths(p))
        TC_LOG_INFO("playerbot.v2",
            "[FlightLearn] {} learned {} flight paths on login (taxi network now routable)",
            p->GetName(), fp);

    // CRITICAL: Initialize phasing data for the bot.
    // 2026-05-20 — confirmed via [neutral_scan_miss] phases=0 diagnostic:
    // 77% of stuck quest-kill bots had EMPTY PhaseShift containers.
    // SendInitialPacketsAfterAddToMap (Player.cpp:25224) is supposed to
    // run PhasingHandler::OnMapChange during HandlePlayerLogin, but for
    // headless bot sessions some pre-condition (m_seer? grid registration?
    // session client-time-sync?) doesn't satisfy and the PhaseShift never
    // populates. Result: bot is in NO phase → quest creatures in
    // conditional phases (post-Shattering Cataclysm+, MoP+, BfA daily
    // hubs, etc.) are invisible → 80y scan finds 0 matching entries →
    // bot wanders forever. V1 had the same issue (BotWorldEntry.cpp:881);
    // explicit re-init here matches that fix. Both calls are idempotent
    // — if HandlePlayerLogin DID populate phases, this is a no-op.
    p->UpdateZone(p->GetZoneId(), p->GetAreaId());
    PhasingHandler::OnMapChange(p);

    // Quest-log hygiene at login. TC core Player::PushQuests() re-pushes
    // auto-granted feature quests (55660 "Time Trials", 84224 "To Delves!",
    // …) at EVERY login — a bot can never turn them in, and abandoning them
    // doesn't stick (CanTakeQuest passes for a non-rewarded quest, so the next
    // login re-pushes). Force-complete them to REWARDED now (SatisfyQuestStatus
    // then rejects them, ending the re-push), and resolve profession-spec
    // choice quests. Doing it at login — the exact moment the junk is
    // (re-)pushed — paces 1:1 with the re-push, which the idle-rule path alone
    // could not keep up with (2026-06-15: online holders grew faster than the
    // throttled rule cleared them). Bounded drain; one resolvable quest/call.
    for (int guard = 0; guard < 40; ++guard)
        if (V2::Fleet::JunkQuestResolver::RunFor(p).done)
            break;

    BotPersonality personality = Services::Config().random_personality()
        ? RandomPersonality(SeedForBot(id))
        : DefaultPersonality();
    Services::Registry().register_bot(id, personality, BotRng{SeedForBot(id)});
    Services::Scheduler().register_bot(id, ActivityTier::Idle);
    // Re-apply persisted squad state (formation type/slot, follow
    // distance, verbose flag) so a relog comes back with the same
    // owner-tunable preferences that were active before logout.
    if (BotAI* ai = Services::Registry().ai(id))
    {
        // ONE blocking read of this bot's playerbot_v2_character row feeds all
        // three login consumers below (squad state, R7 leveling target, #4A
        // archetype). Previously each ran its own SELECT of the same row -
        // three sync round trips per bot, ~120 when 40 companions log in
        // together. Stays synchronous: formation state has no not-loaded gate.
        // Columns: [0..3] squad state (OwnerRegistry::kSquadStateColumns order),
        // [4..8] leveling target, [9] archetype_id. No row -> the same defaults
        // each consumer used when its own read came back empty.
        SLOW_CALL("bot login row read (database)");
        QueryResult const row = CharacterDatabase.PQuery(
            "SELECT {}, "
            "leveling_target_hub, leveling_target_map, leveling_bracket_lo, "
            "leveling_bracket_hi, UNIX_TIMESTAMP(leveling_chosen_at), "
            "archetype_id "
            "FROM playerbot_v2_character WHERE character_guid_low={}",
            OwnerRegistry::kSquadStateColumns, id);
        Field const* const rf = row ? row->Fetch() : nullptr;

        OwnerRegistry::SquadState const s = rf ? OwnerRegistry::SquadStateFromFields(rf) : OwnerRegistry::SquadState{};
        ai->set_formation_type(static_cast<FormationType>(s.formation_type));
        ai->set_formation_slot(s.formation_slot);
        ai->set_follow_distance(s.follow_distance);
        ai->set_verbose_logging(s.owner_verbose);

        // The role the owner gave a companion lives in the bot logic and the Classic virtual spec, both memory only: pin it
        // again so a relog (or a restart) does not hand the bot's role back to a talent inference.
        // A companion with no stored role (added before roles were stored, or never asked) is pinned to the role it
        // plays now and stored: left unset, the raid quota fill treated it as free and moved paladins and druids from
        // heal to tank after a restart.
        if (Services::Owners().GetOwner(id).account_id != 0)
            V2::Fleet::BotPopulationManager::ApplyCompanionRole(p, Role::Unknown, "login");

        // R7: hydrate the sticky leveling-zone relocation target (0010 columns)
        // so a bot resumes traveling toward the SAME hub it picked before the
        // restart instead of re-picking (distance-ranked → would flip-flop). A
        // one-shot read; set_leveling_target marks the target loaded so the
        // builder won't clobber a persisted choice. hub_id 0 = never picked.
        if (rf)
        {
            Field const* const f = rf + 4;
            BotAI::LevelingZoneTarget t;
            t.hub_id     = f[0].GetUInt32();
            t.map_id     = f[1].GetUInt32();
            t.bracket_lo = f[2].GetUInt8();
            t.bracket_hi = f[3].GetUInt8();
            t.chosen_at  = f[4].IsNull() ? 0 : f[4].GetUInt64();
            ai->set_leveling_target(t);
        }
        else
            ai->mark_leveling_target_loaded();

        // ---- #4A: per-bot ARCHETYPE (WHAT/WHEN the bot plays) ----
        // Gate behind PlayerbotV2.Archetype.Enabled. When disabled every bot
        // reads as the default CasualSolo (id 0) so behavior is uniform for
        // controlled testing.
        if (Services::Config().archetype_enabled())
        {
            // Deterministic roll from the per-bot seed (same seed used for
            // personality + rng) so a bot's archetype is stable across
            // restarts. We read the persisted archetype_id to decide whether
            // a write-back is needed: on first spawn (or after a DB wipe) the
            // column is still the schema default and differs from the roll, so
            // we persist it once; thereafter the read matches and we skip the
            // UPDATE. RollArchetype is idempotent, so even a stale persisted
            // value converges to the deterministic roll without churn.
            const BotArchetype rolled = RollArchetype(SeedForBot(id));
            ai->set_archetype(rolled);

            bool need_write = true;
            if (rf && rf[9].GetUInt8() == rolled.archetype_id)
                need_write = false;
            if (need_write)
                CharacterDatabase.PExecute(
                    "UPDATE playerbot_v2_character SET archetype_id={} "
                    "WHERE character_guid_low={}",
                    uint32(rolled.archetype_id), id);
        }
    }
    // Bots taught by an earlier build can carry other classes' trainer spells (a rogue with Judgement): drop them at login.
    if (uint32 const removed = ::Playerbot::API(p).purge_foreign_class_spells())
        TC_LOG_INFO("playerbot.v2", "[levelup] {} lost {} spells of other classes at login", p->GetName(), removed);
    // INFO-level so the operator can see which bots actually attached bot logic
    // when bringing up a fresh worldserver / spawning via .playerbot login.
    // The DEBUG history can be inspected via /history or /inspect later.
    TC_LOG_INFO("playerbot.v2", "[PlayerbotV2] Bot logic attached: {} (id {}, class {} race {} level {})",
                p->GetName(), id, uint32(p->GetClass()), uint32(p->GetRace()), uint32(p->GetLevel()));
}

void Module::OnPlayerLogout(Player* p)
{
    if (!initialized_ || !p) return;
    const BotId id = p->GetGUID().GetCounter();
    if (WorldSession const* sess = p->GetSession(); sess && !sess->IsBot())
    {
        Services::Owners().NoteOwnerCharacterOffline(sess->GetAccountId(), id);
        g_addon_hints.Logout(id);
    }
    if (!Services::Registry().has(id)) return;     // not one of ours
    // Speak queued chat (the `.bot dismiss` goodbye) while the bot is still in the world.
    BotChatter::FlushPendingChat(p);
    Services::Scheduler().unregister_bot(id);
    Services::Snapshots().remove(id);
    Services::Registry().unregister_bot(id);
    // Clean per-bot entries in file-scope maps. These keys are uint64
    // player guid_low; without erase, entries accumulate forever on
    // populations that rotate bots in/out (BotPopulationManager
    // spawning fresh alts as old ones level up). At ~24 bytes/entry
    // the leak is slow but real for long-running servers — and the
    // entries are dead data (guid_low is unique per character).
    g_last_snap_z_ms.erase(id);
    g_global_stuck.erase(id);
    TC_LOG_INFO("playerbot.v2", "[PlayerbotV2] Bot logic detached: {} (id {})", p->GetName(), id);
}

void Module::OnLevelUp(Player* p, uint8 new_level)
{
    if (!initialized_ || !p) return;
    // A bot that gains a level by XP learns what its trainer would teach at the new level (all ranks the level opens), the
    // class quest abilities and weapon skills, as the setup pipeline does for a bot it levels. Without this a questing bot
    // only trains when it happens to stand beside a trainer with a gold in its bag and keeps rank 1 for dozens of levels.
    // Talents follow by themselves: idle:apply_context_talents is keyed on the level and spends the new point along the build.
    if (!Services::Registry().has(p->GetGUID().GetCounter()))
        return;
    ::Playerbot::API api(p);
    api.purge_foreign_class_spells();
    uint32 const quest_spells = api.learn_class_quest_spells_free();
    uint32 const spells = api.learn_trainer_spells_free();
    uint32 const weapon_skills = api.learn_weapon_skills_free();
    TC_LOG_INFO("playerbot.v2", "[levelup] {} reached L{} (class {} zone {}): +{} trainer spells, +{} class quest spells, +{} weapon skills",
        p->GetName(), uint32(new_level), uint32(p->GetClass()), p->GetZoneId(), spells, quest_spells, weapon_skills);
}

namespace {

// Killing blows on players, stashed by OnDamageTaken and consumed by OnDeath for the [bot_death] line the test-night
// digest reads (scripts/bot-log-digest.ps1). World thread only; one entry per player at most, erased at the death.
struct LethalHit { uint32 spell = 0; int32 amount = 0; int32 overkill = 0; };
std::unordered_map<uint64, LethalHit> g_lethal_hits;
std::mutex g_lethal_hits_mtx;   // OnDamageTaken may run on a map thread when MapUpdate.Threads > 1

// Members of a group still alive on this map, `skip` (the one who just died) excluded: the wipe test of tools/Fight-Report.ps1.
void CountGroupAlive(Group const* g, Player const* skip, uint32& alive, uint32& total)
{
    alive = total = 0;
    if (!g) return;
    for (GroupReference const& ref : g->GetMembers())
    {
        Player const* m = ref.GetSource();
        if (!m) continue;
        ++total;
        if (m != skip && m->IsAlive() && !m->HasAuraType(SPELL_AURA_GHOST)) ++alive;
    }
}

void LogPlayerDeath(Player* vp, Unit* killer, BotAI const* vai)
{
    // Consume the stashed hit first so an early return below cannot leak the entry.
    LethalHit hit;
    {
        std::lock_guard lk(g_lethal_hits_mtx);
        if (auto it = g_lethal_hits.find(vp->GetGUID().GetCounter()); it != g_lethal_hits.end())
        {
            hit = it->second;
            g_lethal_hits.erase(it);
        }
    }
    Map* map = vp->GetMap();
    if (!vai && !(map && (map->IsDungeon() || map->IsBattlegroundOrArena())))
        return;   // humans only where it matters: raids, dungeons, battlegrounds
    std::string killer_txt = "-";
    if (killer)
    {
        killer_txt = killer->GetName();
        if (Creature const* kc = killer->ToCreature())
            killer_txt += " (" + std::to_string(kc->GetEntry()) + ")";
        else if (killer == vp)
            killer_txt = "self";
    }
    std::string spell_txt = "melee";
    if (hit.spell)
    {
        SpellInfo const* si = sSpellMgr->GetSpellInfo(hit.spell, DIFFICULTY_NONE);
        spell_txt = std::to_string(hit.spell) + " " + (si ? std::string(si->SpellName->Str[LOCALE_enUS]) : std::string("?"));
    }
    else if (hit.amount == 0)
        spell_txt = "unknown";
    char const* rule = vai ? vai->last_rule_fired() : nullptr;
    // Where the killer stood and what the killing spell is in this server's data (first damaging effect's radius and
    // base points): settles "out of range but still hit" and "hit far harder than the spell should" from the log alone
    // (Molten Giant Smash 2026-10-06: 8-25k at ~23 y, vanilla is 810-990 within 10 y).
    std::string killer_at = "-";
    if (killer && killer != vp)
        killer_at = fmt::format("{:.1f},{:.1f} dist={:.1f}", killer->GetPositionX(), killer->GetPositionY(), vp->GetExactDist2d(killer));
    std::string spell_data = "-";
    if (hit.spell)
        if (SpellInfo const* si = sSpellMgr->GetSpellInfo(hit.spell, DIFFICULTY_NONE))
            for (SpellEffectInfo const& eff : si->GetEffects())
                if (eff.IsEffect(SPELL_EFFECT_SCHOOL_DAMAGE) || eff.IsEffect(SPELL_EFFECT_WEAPON_DAMAGE) ||
                    eff.IsEffect(SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL) || eff.IsEffect(SPELL_EFFECT_NORMALIZED_WEAPON_DMG) ||
                    eff.IsEffect(SPELL_EFFECT_WEAPON_PERCENT_DAMAGE) || eff.IsEffect(SPELL_EFFECT_HEALTH_LEECH))
                {
                    spell_data = fmt::format("eff{} type={} bp={} radius={:.0f}", uint32(eff.EffectIndex), uint32(eff.Effect),
                        eff.BasePoints, eff.CalcRadius(killer).Max);
                    break;
                }
    uint32 grp_alive = 0, grp_total = 0;
    CountGroupAlive(vp->GetGroup(), vp, grp_alive, grp_total);
    Playerbot::V2::DeathCause::Note(vp->GetGUID().GetCounter(), fmt::format("{} by {} ({} dmg)", spell_txt, killer_txt, hit.amount));
    TC_LOG_INFO("playerbot.v2",
        "[bot_death] ts={} {} ({}) map={} pos={:.1f},{:.1f},{:.1f} killer={} spell={} dmg={} overkill={} rule={} killer_at={} spell_data={} class={} grp_alive={}/{}",
        uint64(GameTime::GetGameTime()), vp->GetName(), vai ? "bot" : "player", vp->GetMapId(),
        vp->GetPositionX(), vp->GetPositionY(), vp->GetPositionZ(), killer_txt, spell_txt, hit.amount, hit.overkill,
        rule ? rule : "-", killer_at, spell_data, uint32(vp->GetClass()), grp_alive, grp_total);

    if (vai)
    {
        // A scenario test in progress takes the same facts as its failure reason.
        Diagnostics::ScenarioDeath sd;
        sd.name = vp->GetName();
        sd.map = vp->GetMapId();
        sd.x = vp->GetPositionX(); sd.y = vp->GetPositionY(); sd.z = vp->GetPositionZ();
        sd.killer = killer_txt;
        sd.spell = spell_txt;
        sd.damage = hit.amount;
        sd.rule = rule ? rule : "-";
        Diagnostics::Scenario::NoteBotDeath(vp->GetGUID().GetCounter(), std::move(sd));
    }
}

} // namespace

void Module::OnDeath(Unit* victim, Unit* killer)
{
    if (!initialized_ || !victim) return;

    // A bot died: stamp the real death time (world thread, the same thread
    // OrderRelease latches on). BotAI::tick's death edge, which can lag the
    // death by a snapshot or more, keeps a release order latched after this
    // instant and drops one from before it (a previous death).
    if (Player* vp = victim->ToPlayer())
    {
        BotAI* vai = Services::Registry().ai(vp->GetGUID().GetCounter());
        if (vai)
        {
            const uint32 now = GameTime::GetGameTimeMS();
            vai->set_died_at_ms(now ? now : 1u);
        }
        // Open-world deaths by a creature feed the hotspot map (quest pursuit avoids places that keep killing peers).
        if (vai && killer && killer != vp && killer->GetTypeId() == TYPEID_UNIT && vp->GetMap() && !vp->GetMap()->IsDungeon()
            && !vp->GetMap()->IsBattlegroundOrArena())
            DeathHotspots::Instance().NoteDeath(vp->GetMapId(), vp->GetPositionX(), vp->GetPositionY(),
                                                uint8(killer->GetLevel()), uint64(GameTime::GetGameTime()));
        LogPlayerDeath(vp, killer, vai);
        return;
    }

    // When a creature dies, queue its corpse on the pending-loot list of
    // every registered bot in its tap list. State_Idle walks the bot toward
    // the closest entry next tick and emits LootIntent once in range. We
    // queue here (rather than emit Intent immediately) because the bot is
    // typically still in combat with adds â€” looting now would interrupt
    // their rotation. The queue persists until State_Idle drains it.
    Creature* c = victim->ToCreature();
    if (c)
        Diagnostics::Scenario::NoteCreatureKilled(c->GetMapId(), c->GetEntry(), c->hasLootRecipient());
    // Boss kill line for tools/Fight-Report.ps1: dungeon and raid bosses, with how many of the tapping group survived.
    if (c && c->GetMap() && c->GetMap()->IsDungeon() && (c->IsDungeonBoss() || c->isWorldBoss()))
    {
        Group const* tap_group = nullptr;
        Player const* tap_player = nullptr;
        for (ObjectGuid const& tg : c->GetTapList())
            if (Player const* tp = ObjectAccessor::FindPlayer(tg))
            {
                tap_player = tp;
                if (tp->GetGroup()) { tap_group = tp->GetGroup(); break; }
            }
        uint32 alive = 0, total = 0;
        CountGroupAlive(tap_group, nullptr, alive, total);
        TC_LOG_INFO("playerbot.v2", "[boss_kill] ts={} map={} {} ({}) raiders_alive={}/{}", uint64(GameTime::GetGameTime()),
                    c->GetMapId(), c->GetName(), c->GetEntry(), tap_group ? alive : (tap_player ? 1u : 0u),
                    tap_group ? total : (tap_player ? 1u : 0u));
    }
    if (!c || !c->hasLootRecipient()) return;
    auto const& tap = c->GetTapList();
    if (tap.empty()) return;
    auto& reg = Services::Registry();
    const ObjectGuid corpse_guid = c->GetGUID();
    for (ObjectGuid const& guid : tap)
    {
        if (!guid.IsPlayer()) continue;
        const BotId id = guid.GetCounter();
        if (!reg.has(id)) continue;
        // Queue the corpse onto the bot's pending-loot list. State_Idle
        // walks the bot to the closest entry and emits a LootIntent each
        // tick; the entry is popped on success. The push helper is mutex-
        // protected since the bot worker drains concurrently.
        reg.push_loot(id, corpse_guid);
    }
}

void Module::OnResurrect(Player* /*p*/)
{
    if (!initialized_) return;
    // Intentionally a no-op. BotAI::tick() reacts to `is_alive` going true
    // by transitioning Dead â†’ Idle on the next snapshot, which restarts the
    // dispatch chain naturally. No event-bus push needed.
}

void Module::OnSpecChanged(Player* /*p*/, uint8 /*new_spec*/)
{
    if (!initialized_) return;
    // Intentionally a no-op. Rotation lookup is `Combat::GetRotation(cls,
    // spec)` every tick, so spec changes pick up the new APL within one
    // tick of the snapshot updating `spec`. No state to invalidate.
}

void Module::OnDamageDealt(Unit* /*attacker*/, Unit* /*victim*/, int32 /*amount*/, uint32 /*spell_id*/)
{
    if (!initialized_) return;
    // Intentionally a no-op. DamageDealt fires every swing/tick/DoT and
    // would saturate the 256-slot inbox ring. Snapshot deltas (in_combat,
    // victim hp) cover the cases rules actually need without burning event
    // capacity. Combat-tier promotion happens in OnWorldUpdate from the
    // freshly built snapshot's `in_combat` flag.
}

namespace { thread_local uint32 tl_damage_spell = 0; }

void Module::NoteDamageSpell(uint32 spell_id)
{
    tl_damage_spell = spell_id;
}

void Module::OnDamageTaken(Unit* attacker, Unit* victim, int32 amount, uint32 spell_id)
{
    // The hook that precedes OnDamage names the spell; consumed here so a later melee swing is not labelled with it.
    uint32 const lethal_spell = spell_id ? spell_id : tl_damage_spell;
    tl_damage_spell = 0;
    if (!initialized_) return;
    Diagnostics::Scenario::NoteDamage(attacker, victim, amount);   // a scenario's boss-pull statistics; one atomic load when none runs
    Diagnostics::Parity::NoteDamage(attacker, victim, amount, spell_id);   // the damage_parity tally; one atomic load when none runs
    // Killing blow on a player: remember the spell for the [bot_death] line (one compare per hit otherwise).
    if (victim && amount > 0 && victim->IsPlayer() && uint64(amount) >= victim->GetHealth())
    {
        std::lock_guard lk(g_lethal_hits_mtx);
        g_lethal_hits[victim->GetGUID().GetCounter()] =
            LethalHit{ lethal_spell, amount, int32(uint64(amount) - victim->GetHealth()) };
    }
    // Intentionally a no-op. The BotEventInbox was retired 2026-05-21
    // after audit found zero consumers + the snapshot's in_combat /
    // hp / attackers vector already covers every actual rule need.
    // V1 had an event bus with real subscribers (174 publishers, 210
    // subscribers across 7 domains); V2's snapshot-then-bot-worker
    // distribution model fills that role differently. If event-driven
    // dispatch becomes needed for a specific feature (e.g. kick-then-
    // pause), introduce a focused per-event hook then.
}

void Module::OnHitOutcome(Unit* attacker, Unit* victim, uint32 spell_id, uint8 outcome, uint32 damage)
{
    if (!initialized_) return;
    Diagnostics::Parity::NoteOutcome(attacker, victim, spell_id, outcome, damage);   // one atomic load when no damage_parity leg measures
}

void Module::OnEnergize(Unit* target, uint32 spell_id, int32 gained, uint8 power)
{
    if (!initialized_) return;
    Diagnostics::Parity::NoteEnergize(target, spell_id, gained, power);   // one atomic load when no economy leg measures
}

void Module::OnHealReceived(Unit* /*healer*/, Unit* /*target*/, int32 /*amount*/, uint32 /*spell_id*/)
{
    if (!initialized_) return;
    // See OnDamageTaken — same rationale. Snapshot's hp_pct + group
    // member HP tracking cover all current consumer cases.
}

void Module::OnAuraApplied(Unit* /*target*/, Aura* /*aura*/)
{
    if (!initialized_) return;
    // Intentionally a no-op. Aura applications fire constantly during combat
    // (every DoT tick stack, every refresh, every proc-aura) and would
    // saturate the 256-slot inbox ring. The snapshot's `own_auras` list (with
    // `mechanic` populated) lets rules reason about active CC within ~100ms
    // of application â€” fast enough for reactive defensives without the
    // event-bus overhead. If a use case ever needs sub-tick latency on a
    // specific aura, gate the push on that spell ID here.
}

void Module::OnAuraRemoved(Unit* /*target*/, Aura* /*aura*/)
{
    if (!initialized_) return;
    // Intentionally a no-op. Same rationale as OnAuraApplied â€” aura removals
    // fire too often to event-bus blindly. Snapshot delta on `own_auras` /
    // outbound aura tables covers refresh-detection use cases.
}

void Module::OnGroupMemberJoined(Group* /*g*/, Player* /*p*/)
{
    if (!initialized_) return;
    // Intentionally a no-op. The world-tick rebuilds GroupSnapshot every tick
    // for grouped bots, so newly-joined members appear in the next snapshot
    // without a hook-driven invalidation. If we ever cache snapshots across
    // ticks, this is the place to invalidate.
}

void Module::OnGroupMemberLeft(Group* /*g*/, Player* /*p*/)
{
    if (!initialized_) return;
    // Intentionally a no-op. Same rationale as OnGroupMemberJoined â€” the
    // next world-tick GroupSnapshot rebuild reflects departures. Leadership
    // re-election lives in TrinityCore's Group code; no V2 action needed.
}

void Module::OnWhisperReceived(Player* sender, Player* receiver, std::string const& msg)
{
    if (!initialized_ || !sender || !receiver) return;
    const BotId id = receiver->GetGUID().GetCounter();
    if (!Services::Registry().has(id)) return;     // not one of ours
    // Try the command parser first â€” if it recognized a verb it returns
    // true and we're done. Otherwise fall through to the social reactor
    // so non-command whispers ("hi", "ty for the help") still get a
    // friendly reply. Without this fallthrough, strangers whispering a
    // bot get silence, which reads as broken.
    if (BotCommandParser::Dispatch(sender, receiver, msg)) return;
    BotChatReactor::ReactWhisper(sender, receiver, msg);
}

void Module::OnGuildChat(Player* sender, uint64 guild_id, std::string const& msg)
{
    // Phase C.3: route to BotChatReactor::ReactGuild. The reactor
    // handles all gating (per-guild throttle, bot-self filter, online-
    // officer pick).
    if (!initialized_ || !sender || guild_id == 0 || msg.empty()) return;
    BotChatReactor::ReactGuild(sender, guild_id, msg);
}

void Module::OnSayChat(Player* sender, std::string const& msg)
{
    // SC-P1a: a real player spoke in /say. Let the reactor pick at most one
    // nearby bot to answer (range-gated by CONFIG_LISTEN_RANGE_SAY, per-bot
    // + per-area cooldown). All gating lives in the reactor.
    if (!initialized_ || !sender || msg.empty()) return;
    BotChatReactor::ReactSay(sender, msg);
}

void Module::OnYellChat(Player* sender, std::string const& msg)
{
    // SC-P1a: /yell variant — wider range (CONFIG_LISTEN_RANGE_YELL) but a
    // lower reply chance (the reactor keeps yell answers rarer than say).
    if (!initialized_ || !sender || msg.empty()) return;
    BotChatReactor::ReactYell(sender, msg);
}

void Module::OnTextEmote(Player* sender, uint32 emote_id, uint32 text_emote_id, ObjectGuid target)
{
    // SC-P2c: a player performed a text emote; a nearby bot reciprocates.
    if (!initialized_ || !sender) return;
    BotChatReactor::ReactEmote(sender, emote_id, text_emote_id, target);
}

void Module::OnItemLooted(Player* p, uint32 item_id)
{
    if (!initialized_ || !p || !p->GetSession() || !p->GetSession()->IsBot()) return;
    BotChatter::OnItemLooted(p, item_id);
}

void Module::OnRaidSubgroupChanged(Group* group, ObjectGuid member, uint8 subgroup)
{
    // The leader moved a raid member (or a restore did, which is not recorded). An owned companion's new sub-group is stored so a
    // relog or `.bot call all` puts it back. Battleground groups have sub-groups of their own and are not the roster.
    if (!initialized_ || !group || group->isBGGroup() || group->isBFGroup() || Fleet::g_subgroup_restoring)
        return;
    BotId const id = member.GetCounter();
    if (!member.IsPlayer() || Services::Owners().GetOwner(id).account_id == 0)
        return;
    Services::Owners().SetStoredSubgroup(id, subgroup);
    TC_LOG_INFO("playerbot.v2", "[roster_save] {} moved to raid group {}{}", member.ToString(), uint32(subgroup) + 1u, Services::Owners().GetOwner(id).saved ? " (saved)" : "");
}

void Module::OnSpiritReleased(Player* p)
{
    // The owner pressed Release Spirit (ike3 / the owner: companions release with
    // their master). Inside a dungeon or raid RepopAtGraveyard revives the
    // owner at the graveyard, so the owner is never a ghost there and
    // State_Dead's owner-released check (owner->is_ghost) cannot fire; this
    // hook is the release itself. Every dead bot of this owner in the group
    // on the owner's (death) map takes the same latch as a "release" order.
    // Bots release through PlayerbotAPI, never this handler; a bot owner is
    // skipped anyway.
    if (!initialized_ || !p || !p->GetSession() || p->GetSession()->IsBot()) return;
    Group* group = p->GetGroup();
    if (!group) return;
    uint32 const account = p->GetSession()->GetAccountId();
    uint32 released = 0, considered = 0;
    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member || member == p) continue;
        BotId const mid = member->GetGUID().GetCounter();
        if (!Services::Registry().has(mid) ||
            !Services::Owners().IsOwner(mid, account, p->GetGUID().GetCounter()))
            continue;
        if (member->IsAlive() || member->GetMapId() != p->GetMapId()) continue;
        ++considered;
        if (BotCommandParser::OrderRelease(p, member, "owner released spirit"))
            ++released;
    }
    TC_LOG_INFO("playerbot.v2",
        "[death-recovery] {} released spirit on map {}{}: {} of {} dead companion(s) on that map release with them",
        p->GetName(), p->GetMapId(), p->GetMap() && p->GetMap()->Instanceable() ? " (instance: owner revives at the graveyard)" : "",
        released, considered);
}

void Module::OnGuildMemberAdded(uint64 guild_id, ObjectGuid joiner_guid, std::string const& joiner_name)
{
    // SC-P2b: welcome a new guild member from one online bot guildmate.
    if (!initialized_ || guild_id == 0) return;
    BotChatReactor::ReactGuildJoin(guild_id, joiner_guid, joiner_name);
}

// Runs a recognized bare group order (ClassifyGroupOrder already normalized it) for the sender's bots in the group.
// Shared by party/raid chat and by the addon-message transport.
void Module::RouteGroupOrder(Player* sender, Group* group, std::string order)
{
    uint32 const account = sender->GetSession() ? sender->GetSession()->GetAccountId() : 0;
    if (account == 0)
        return;
    if (order.rfind("crowdcontrol ", 0) == 0 || order.rfind("crowd control ", 0) == 0 || order == "cc on" || order == "cc off")
    {
        // A switch of the owner's, not an order to each bot: set once, answer once.
        std::string const word = order.substr(order.find_last_of(' ') + 1);
        if (word != "on" && word != "off")
            return;
        bool const on = word == "on";
        bool const changed = Services::Owners().SetCcEnabled(account, on);
        TC_LOG_INFO("playerbot.v2", "[cc_switch] {} turns crowd control {} ({})", sender->GetName(), word, changed ? "changed" : "already");
        ChatHandler(sender->GetSession()).PSendSysMessage("Crowd control for your bots: %s.", on ? "on" : "off");
        return;
    }
    uint32 tanks = 0;
    // Raid pull: the main tank (raid-frame flag) pulls alone; with none set, every tank as before.
    ObjectGuid pull_main_tank;
    char const* pull_why = "every tank (no main tank set)";
    // A tank that can pull: one of the sender's own bots, alive, in the tank role.
    auto const usable_tank = [&](Player* m)
    {
        if (!m || m == sender || !m->IsAlive())
            return false;
        BotId const mid = m->GetGUID().GetCounter();
        return Services::Registry().has(mid) && Services::Owners().IsOwner(mid, account, sender->GetGUID().GetCounter()) &&
               BotCommandParser::BotRole(m) == Role::Tank;
    };
    std::string main_tank_skipped;
    if ((order == "pull" || order == "pull back") && group->isRaidGroup())
        for (Group::MemberSlot const& slot : group->GetMemberSlots())
        {
            if (!(slot.flags & MEMBER_FLAG_MAINTANK))
                continue;
            // The flagged main tank pulls only if it can: a dead one, a player, someone else's bot or a bot that is not
            // a tank would leave nobody pulling, so the next flagged one or the closest tank takes over.
            Player* flagged = ObjectAccessor::FindConnectedPlayer(slot.guid);
            if (usable_tank(flagged)) { pull_main_tank = slot.guid; pull_why = "the main tank"; break; }
            main_tank_skipped += (main_tank_skipped.empty() ? "" : ", ") + slot.name;
        }
    if (!main_tank_skipped.empty() && pull_main_tank.IsEmpty())
        TC_LOG_INFO("playerbot.v2", "[pull_back] flagged main tank {} cannot pull (dead, not your bot or not a tank): next best tank pulls",
                    main_tank_skipped);
    // A 5-man's Pull sends one tank like Pull back does (two tank-role bots would both run at the pack); a raid's Pull keeps every tank.
    if (order == "pull back" || (order == "pull" && !group->isRaidGroup()))
    {
        // Exactly ONE bot pulls: the tank assigned to the selected mob's raid icon, else the main tank, else the
        // owned tank closest to the mob. Every other bot (other tanks included) holds where it stands.
        Unit* sel = sender->GetSelectedUnit();
        ObjectGuid assigned;
        if (sel)
        {
            auto const marks = Services::Owners().TankMarks(group->GetGUID().GetCounter());
            for (uint8 i = 0; i < 8 && assigned.IsEmpty(); ++i)
                if (group->GetTargetIcon(i) == sel->GetGUID())
                    for (auto const& [tank_low, marker] : marks)
                        if ((marker & 7) == i) { assigned = ObjectGuid::Create<HighGuid::Player>(tank_low); break; }
        }
        if (!assigned.IsEmpty())
        { pull_main_tank = assigned; pull_why = "the tank assigned to that icon"; }
        else if (pull_main_tank.IsEmpty())
        {
            float best = 1e9f;
            for (GroupReference const& itr : group->GetMembers())
            {
                Player* m = itr.GetSource();
                if (!m || m == sender || !m->IsAlive()) continue;
                BotId const mid = m->GetGUID().GetCounter();
                if (!Services::Registry().has(mid) || !Services::Owners().IsOwner(mid, account, sender->GetGUID().GetCounter()) ||
                    BotCommandParser::BotRole(m) != Role::Tank)
                    continue;
                float const d = sel ? m->GetDistance(sel) : m->GetDistance(sender);
                if (d < best) { best = d; pull_main_tank = m->GetGUID(); }
            }
            pull_why = "the tank closest to the mob";
        }
        TC_LOG_INFO("playerbot.v2", "[pull_back] {} pulls: {} chosen as {}; every other bot holds",
            sender->GetName(), pull_main_tank.IsEmpty() ? std::string("nobody") : pull_main_tank.ToString(), pull_why);
    }
    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member || member == sender) continue;
        BotId const mid = member->GetGUID().GetCounter();
        // Quest words also reach an unowned bot in the group (a world bot the player invited): it shares and accepts
        // quests like a companion; every other order stays with the bots the sender owns.
        bool const quest_word = order == "share" || order == "accept" || order == "accept all" || order == "quests" ||
                                order == "quest list";
        if (!Services::Registry().has(mid) ||
            !(Services::Owners().IsOwner(mid, account, sender->GetGUID().GetCounter()) ||
              (quest_word && Services::Owners().GetOwner(mid).account_id == 0)))
            continue;
        if (order == "pull back")
        {
            // Only the tank pulls and returns (the main tank when a raid flags one); everyone else carries on.
            if (BotCommandParser::BotRole(member) != Role::Tank ||
                (!pull_main_tank.IsEmpty() && member->GetGUID() != pull_main_tank))
            {
                // The other tanks come and stand beside the owner, where the mob is brought to (playtest 2026-10-08: "all the
                // warriors should be standing beside me when I say pull back"); everyone else holds where they stand
                // (like "pull"); "fall back" is the whole-raid version.
                if (BotCommandParser::BotRole(member) == Role::Tank && member->IsAlive())
                    BotCommandParser::DispatchSingle(sender, member, "follow", "party");
                else
                    BotCommandParser::ReleaseOwnerFollowOrder(sender, member, "party pull back");
                continue;
            }
            BotCommandParser::DispatchSingle(sender, member, order, "party");
            ++tanks;
            continue;
        }
        if (order == "pull")
        {
            // A tank with an icon assignment goes too, to its own mob (MC Garr 2026-10-08: only the flagged main tank pulled).
            bool assigned_tank = false;
            for (auto const& [tank_low, marker] : Services::Owners().TankMarks(group->GetGUID().GetCounter()))
                if (tank_low == mid) { assigned_tank = true; break; }
            if (BotCommandParser::BotRole(member) != Role::Tank ||
                (!pull_main_tank.IsEmpty() && member->GetGUID() != pull_main_tank && !assigned_tank))
            {
                // The rest join once it is engaged: the owner handed the fight back.
                if (BotCommandParser::BotRole(member) == Role::Tank)
                    TC_LOG_INFO("playerbot.v2", "[raid_pull] {} stays back: tank without an icon assignment (main tank {})",
                                member->GetName(), pull_main_tank.IsEmpty() ? std::string("none") : pull_main_tank.ToString());
                BotCommandParser::ReleaseOwnerFollowOrder(sender, member, "party pull");
                continue;
            }
            TC_LOG_INFO("playerbot.v2", "[raid_pull] {} is sent to pull ({})", member->GetName(),
                        assigned_tank ? "its icon assignment" : "the main tank / no main tank set");
            BotCommandParser::DispatchSingle(sender, member, "tank", "party");
            ++tanks;
            continue;
        }
        BotCommandParser::DispatchSingle(sender, member, order, "party");
    }
    if ((order == "pull" || order == "pull back") && !tanks)
    {
        // Say why: the flagged main tank could not pull, or none of the sender's bots in this group is a living tank.
        uint32 owned_tanks = 0, dead_tanks = 0;
        for (GroupReference const& itr : group->GetMembers())
        {
            Player* m = itr.GetSource();
            if (!m || m == sender) continue;
            BotId const mid = m->GetGUID().GetCounter();
            if (!Services::Registry().has(mid) || !Services::Owners().IsOwner(mid, account, sender->GetGUID().GetCounter()) ||
                BotCommandParser::BotRole(m) != Role::Tank)
                continue;
            ++owned_tanks;
            if (!m->IsAlive()) ++dead_tanks;
        }
        ChatHandler(sender->GetSession()).PSendSysMessage(owned_tanks && dead_tanks == owned_tanks ?
            "Your tank bots are all dead; use attack instead." :
            "No tank among your bots to pull; use attack instead.");
        TC_LOG_INFO("playerbot.v2", "[pull_back] {} '{}': no tank pulled (your tank bots in the group: {}, dead {}; flagged main tank skipped: {})",
                    sender->GetName(), order, owned_tanks, dead_tanks, main_tank_skipped.empty() ? "none" : main_tank_skipped);
    }
    return;
}

void Module::OnAddonOrder(Player* sender, Group* group, std::string const& prefix, std::string const& text)
{
    if (!initialized_ || !sender || !group) return;
    auto const order = ClassifyAddonOrder(prefix, text, Services::Config().behavior_owner_commands());
    if (!order) return;
    TC_LOG_INFO("playerbot.v2", "[addon_order] {} -> '{}'", sender->GetName(), *order);
    RouteGroupOrder(sender, group, *order);
}

void Module::OnGroupPingUnit(Player* sender, Group* group, Unit* target, uint8 type)
{
    if (!initialized_ || !sender || !group || !target || type != 0) return;   // 0 = PingSubjectType::Attack
    if (target->IsPlayer() || !target->IsAlive() || !sender->IsValidAttackTarget(target)) return;
    TC_LOG_INFO("playerbot.v2", "[ping] {} pings attack on {}", sender->GetName(), target->GetName());
    // "attack" acts on the sender's selection: the ping picks the mob.
    sender->SetSelection(target->GetGUID());
    RouteGroupOrder(sender, group, "attack");
}

void Module::OnGroupPingPoint(Player* sender, Group* group, Position const& point, uint8 type)
{
    if (!initialized_ || !sender || !group || type != 3) return;   // 3 = PingSubjectType::OnMyWay
    uint32 const account = sender->GetSession() ? sender->GetSession()->GetAccountId() : 0;
    if (account == 0) return;
    uint32 set = 0;
    for (GroupReference const& itr : group->GetMembers())
    {
        Player* m = itr.GetSource();
        if (!m || m == sender || !m->IsAlive() || m->GetMapId() != sender->GetMapId()) continue;
        BotId const mid = m->GetGUID().GetCounter();
        if (!Services::Registry().has(mid) || !Services::Owners().IsOwner(mid, account, sender->GetGUID().GetCounter())) continue;
        if (BotAI* ai = Services::Registry().ai(mid))
        {
            ai->set_rally_point(sender->GetMapId(), point.GetPositionX(), point.GetPositionY(), point.GetPositionZ());
            ++set;
        }
    }
    if (!set) return;
    TC_LOG_INFO("playerbot.v2", "[ping] {} pings on my way to ({:.1f},{:.1f}): {} bots fall back there", sender->GetName(),
                point.GetPositionX(), point.GetPositionY(), set);
    RouteGroupOrder(sender, group, "fallback");
}

void Module::NoteAddonHello(Player* p)
{
    if (!initialized_ || !p) return;
    if (g_addon_hints.Pending(p->GetGUID().GetCounter()))
        TC_LOG_INFO("playerbot.v2", "[addon_hello] {} has the ForeverBots add-on", p->GetName());
    g_addon_hints.Hello(p->GetGUID().GetCounter());
}

void Module::SendDueAddonHints()
{
    if (!Services::Config().addon_hint_enabled())
        return;
    for (uint64 guid : g_addon_hints.TakeDue(GameTime::GetGameTimeMS()))
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
        if (!p || !p->IsInWorld() || !p->GetSession() || p->GetSession()->IsBot())
            continue;
        ChatHandler(p->GetSession()).PSendSysMessage("WoW Forever Bots add-on (buttons for your bots, Raid ready): %s - unzip it into Interface\\AddOns and type /reload.",
            Services::Config().addon_hint_url().c_str());
        TC_LOG_INFO("playerbot.v2", "[addon_hint] {} has no ForeverBots add-on: link sent", p->GetName());
    }
}

void Module::OnAddonWhisper(Player* sender, Player* receiver, std::string const& prefix, std::string const& text)
{
    // The add-on's greeting to itself on login: this client has it, so no hint.
    if (initialized_ && sender && sender == receiver && prefix == kAddonOrderPrefix && text == kAddonHelloText)
    {
        NoteAddonHello(sender);
        return;
    }
    if (!initialized_ || !sender || !receiver || text.empty() || prefix != kAddonOrderPrefix) return;
    TC_LOG_INFO("playerbot.v2", "[addon_whisper] {} -> {}", sender->GetName(), receiver->GetName());
    OnWhisperReceived(sender, receiver, text);
}

void Module::OnPartyChat(Player* sender, Group* group, std::string const& msg)
{
    // Squad-chat command surface. Only messages prefixed with `;` are
    // routed; every other party-chat line is ignored so we don't spam
    // bots with the human chatter that fills /p during dungeons.
    //
    // The leading `;` is stripped, then the rest is treated like a
    // whisper to the first owned bot in the group â€” the dispatch path
    // resolves any address prefix (`tank:`, `mage:`, `Areon:`, ...)
    // and applies to all matching bots. The whispered "primary" bot
    // is the messenger that emits the single summary reply.
    if (!initialized_ || !sender || !group || msg.empty()) return;
    // Social reaction layer: lets bots respond to "ty"/"gz"/"lol"/their
    // own name with short conversational replies so group chat reads like
    // a room of real players. Runs for every message; the reactor itself
    // gates on bot verbosity, per-bot throttle, and skips bot-sent lines.
    BotChatReactor::React(sender, group, msg);
    if (msg.front() != ';')
    {
        // A bare group order ("follow", "stay", ...) in party or raid chat goes to every bot of the sender's in the
        // group. Other chatter stays ignored.
        if (auto const order = ClassifyGroupOrder(msg, Services::Config().behavior_owner_commands()))
            RouteGroupOrder(sender, group, *order);
        return;
    }
    const std::string body = msg.substr(1);
    if (body.empty()) return;
    // Walk the group for the sender's first owned bot. We need a
    // primary bot to attach the reply whisper to; if the owner has
    // none in the group, route to the first owned-on-realm fallback.
    const uint32 sender_account =
        sender->GetSession() ? sender->GetSession()->GetAccountId() : 0;
    if (sender_account == 0) return;
    Player* primary = nullptr;
    // Pass 1: prefer an EXPLICITLY OWNED bot in the group. This is the
    // canonical squad-control path â€” the player has bound bots via
    // /squad mark or similar and wants commands routed to their squad.
    Player* any_group_bot = nullptr;
    for (GroupReference const& itr : group->GetMembers())
    {
        Player* member = itr.GetSource();
        if (!member || member == sender) continue;
        const BotId mid = member->GetGUID().GetCounter();
        if (!Services::Registry().has(mid)) continue;
        if (!any_group_bot) any_group_bot = member;  // remember for pass 2
        if (Services::Owners().IsOwner(mid, sender_account,
                                       sender->GetGUID().GetCounter()))
        { primary = member; break; }
    }
    // Pass 2: any owned bot online (off-tank parked elsewhere, etc.).
    if (!primary)
    {
        std::vector<BotId> const owned =
            Services::Owners().BotsOwnedBy(sender_account);
        for (BotId mid : owned)
        {
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(mid);
            if (Player* p = ObjectAccessor::FindConnectedPlayer(g))
            { primary = p; break; }
        }
    }
    // Pass 3: any V2 bot in the group. Covers the LFG-formed-group case
    // where the player ran the dungeon-finder UI and joined alongside
    // auto-spawned bots they don't formally own â€” the player is sharing
    // the dungeon with them and should be able to drive their behavior
    // (run/stop/pull/etc.). Authority is still scoped to the group: a
    // bot in someone else's group can't be commanded from outside.
    if (!primary)
        primary = any_group_bot;
    if (!primary) return;
    if (!BotCommandParser::Dispatch(sender, primary, body, "party;"))
    {
        // A ";" line is meant as an order: an unknown or refused one answers instead of vanishing. (A plain whisper falls
        // through to the social reactor and a bare party word is chatter, so only this explicit route replies.)
        BotId const pid = primary->GetGUID().GetCounter();
        if (Services::HasIntents(pid))
        {
            Intent it{};
            it.bot_id = pid;
            it.body = ChatIntent{WhisperIntent{
                sender->GetName(),
                fmt::format("{}: no order '{}' (unknown, or not yours to give)", primary->GetName(), body.substr(0, 40))}};
            Services::Intents(pid).push(std::move(it));
        }
    }
}

void Module::OnPlayerJoinedBgQueue(Player* player, uint32 bg_type_id, uint8 bracket)
{
    if (!initialized_ || !player) return;
    // SOLO bots ignore (they're queued by Filler intent emit, not by core
    // path — re-firing the fill for each would recurse). A bot GROUP LEADER
    // is the exception (audit B25): a premade (guild BG-night /
    // BgTeamForming) that just queued needs the OPPOSING faction filled or
    // the match can never form — the human-player path never fires for it
    // and SeedBgMatches only seeds ambient matches when no BG is active.
    if (player->GetSession() && player->GetSession()->IsBot())
    {
        Group const* g = player->GetGroup();
        if (!g || g->GetLeaderGUID() != player->GetGUID())
            return;
        // fall through: bot premade leader — fill both factions below
    }

    // A real player waiting for a place: MakeRoomForRealPlayers watches the wait.
    Fleet::BotQueueFiller::NoteHumanBgQueue(player, bg_type_id, bracket);
    // A running battleground of this type that has or can make room: the human joins it (a bot leaves), no second
    // instance is opened and no bot fill is queued (Playerbot.Behavior.BgHumanPriority).
    uint32 claimed_open_slots = 0;
    bool claimed = false;
    if (player->GetSession() && !player->GetSession()->IsBot() &&
        Fleet::BotQueueFiller::ClaimRunningBgForHuman(player, bg_type_id, bracket, &claimed_open_slots))
    {
        // Nearly full already: nothing to add. Otherwise the fill below tops the instance up while the human is porting in
        // (TC's queue update sends the queued bots to the instance with a human first).
        if (claimed_open_slots <= 2)
            return;
        claimed = true;
    }

    Fleet::BotQueueFiller filler;
    Fleet::BotQueueFiller::FillRequest req{};
    req.kind = Fleet::BotQueueFiller::QueueKind::Bg;
    req.bracket = bracket;
    req.faction = uint32(player->GetTeam());
    req.instance_id = bg_type_id;
    req.requesting_player = player;
    // Override target level to the queuing player's actual level. The
    // PVPDifficultyEntry bracket ID is NOT level/10 â€” for AV bracket 0
    // corresponds to L51-60 (or similar), not L0-9 as BracketMidpoint
    // would compute. Without this override the filler searches the wrong
    // level pool and returns no candidates. Observed 2026-05-13: AV queue
    // logged "kind=0 bracket=0 instance=1 needs=3T/3H/30D" with horde
    // online_seen=250 but only 7 actually queued because the level-15
    // midpoint with Â±15 window only matched L0-30 bots, when the user
    // (and the bracket's actual range) was L60+. Same issue as LFG â€”
    // same fix.
    req.target_level_override = uint8(std::clamp(int(player->GetLevel()), 1, int(MaxPlayerLevel())));
    // Cap initial queue fill at max_per_team for the BG so we never
    // overpopulate. The BattlegroundTemplate exposes the cap; for a
    // 10v10 BG (WSG/TP/AB/etc.) this is 10. Without the cap, the
    // default NeedsFor(Bg) of 3T/3H/30D = 36 per side overshot â€”
    // observed WSG with 13 alliance vs 7 horde when surplus invites
    // landed before TC's per-team check could reject them.
    if (BattlegroundTemplate const* tmpl =
            sBattlegroundMgr->GetBattlegroundTemplateByTypeId(BattlegroundTypeId(bg_type_id)))
    {
        if (uint16 const max_per_team = tmpl->GetMaxPlayersPerTeam())
            req.max_total_bots = uint8(std::min<uint16>(max_per_team, 255));
        // Look up the actual bracket level range on this BG's map so the
        // filler can reject bots whose level is outside [min, max]. Without
        // this, the Â±15 coarse window pulled in bots (e.g., L10-17 for an
        // L22 user) that AV's PVPDifficulty doesn't have a bracket for â€”
        // every queue intent then fails `no_bracket` in API::bg_queue and
        // TC's matchmaker never gets enough valid candidates to create
        // the BG. Crash 2026-05-13: AV stuck not starting.
        if (!tmpl->MapIDs.empty())
        {
            if (PVPDifficultyEntry const* diff =
                    DB2Manager::GetBattlegroundBracketById(tmpl->MapIDs.front(),
                                                            BattlegroundBracketId(bracket)))
            {
                req.bracket_min_level = uint8(std::clamp<int32>(diff->MinLevel, 1, int32(MaxPlayerLevel())));
                req.bracket_max_level = uint8(std::clamp<int32>(diff->MaxLevel, 1, int32(MaxPlayerLevel())));
            }
        }
    }
    if (claimed)
        req.max_total_bots = uint8(std::min<uint32>(req.max_total_bots ? req.max_total_bots : 255u, claimed_open_slots));
    filler.Fill(req);

    // BotCoordinationBus publish â€” BG team formation signal. Existing
    // BotQueueFiller path stays the primary filler (battle-tested);
    // bus subscribers (BotGroupBuilder) form an additional premade
    // 5-bot squad on the same faction so the queue has a cohesive
    // group entry alongside the solo invites.
    // Only a human's queue forms the premade: the premade's own leader queues through this hook too, and publishing again for it
    // formed a new 5-bot premade per round (same-faction overshoot, audit B 1.1).
    bool const queuerIsBot = player->GetSession() && player->GetSession()->IsBot();
    if (Services::Initialized() && !queuerIsBot)
    {
        V2::CoordEvent ev{};
        ev.kind         = V2::CoordSignal::BgTeamForming;
        ev.origin_low   = player->GetGUID().GetCounter();
        ev.content_id   = bg_type_id;
        ev.level_min    = req.bracket_min_level;
        ev.level_max    = req.bracket_max_level;
        ev.faction_mask = (player->GetTeam() == ALLIANCE) ? 0x1u : 0x2u;
        Services::Coordination().Publish(ev);
    }
}

bool Module::HoldNewBattleground(uint32 bg_type_id, uint8 bracket)
{
    return initialized_ && Fleet::BotQueueFiller::HoldNewBattleground(bg_type_id, bracket);
}

void Module::OnPlayerJoinedLfg(Player* player, uint32 dungeon_id, uint8 /*role_mask*/)
{
    if (!initialized_ || !player) return;
    if (player->GetSession() && player->GetSession()->IsBot()) return;

    Fleet::BotQueueFiller filler;
    Fleet::BotQueueFiller::FillRequest req{};
    // Detect raid vs 5-man dungeon from the LFGDungeons DB2 entry. LFR/raid
    // queues require Raid10 / Raid20 composition (2T/3H/5D or 2T/4H/14D)
    // rather than 1T/1H/3D. Pulling group-size from CountTank+CountHealer+
    // CountDamage covers any future content without a hard-coded type map.
    req.kind = Fleet::BotQueueFiller::QueueKind::Dungeon5;
    // Target level: derived from the DUNGEON's level range, falling back
    // to the QUEUEING PLAYER's level. Both bypass the coarse
    // BracketMidpoint mapping (which rounds level/10 to a midpoint that's
    // off by 10+ for low/mid brackets â€” e.g. L21 â†’ bracket 2 â†’ midpoint
    // 35, far too high for L17-24 dungeons). Empirical log confirmation
    // from a L21 Balastan in Wailing Caverns:
    //   [QueueFill] kind=1 bracket=2 needs=1T/1H/3D
    //   pool faction=HORDE â€¦ level=175 â†’ 175 horde bots rejected by Â±5
    //   filter around midpoint 35. ContentTuningID was 0 for that dungeon
    //   (vanilla data) so the prior ContentTuning lookup didn't fire.
    // Use the player's exact level as a robust fallback â€” level-scaling
    // and Â±15 LFG window will absorb minor under/over-match.
    req.bracket = uint8(player->GetLevel() / 10);
    req.target_level_override = uint8(std::clamp(int(player->GetLevel()), 1, int(MaxPlayerLevel())));
    if (LFGDungeonsEntry const* d = sLFGDungeonsStore.LookupEntry(dungeon_id))
    {
        const uint32 size = uint32(d->CountTank) + uint32(d->CountHealer) +
                            uint32(d->CountDamage);
        if (size > 5 && size <= 12)       req.kind = Fleet::BotQueueFiller::QueueKind::Raid10;
        else if (size > 12)               req.kind = Fleet::BotQueueFiller::QueueKind::Raid20;
        // size <= 5 keeps Dungeon5 (scenarios use size 3 â€” still Dungeon5
        // composition-wise, the filler clamps to <=5 fill).
        if (d->ContentTuningID)
        {
            if (Optional<ContentTuningLevels> lv =
                    sDB2Manager.GetContentTuningData(d->ContentTuningID, {}))
            {
                const int16 mid = (lv->TargetLevelMin > 0 || lv->TargetLevelMax > 0)
                    ? int16((lv->TargetLevelMin + lv->TargetLevelMax) / 2)
                    : int16((lv->MinLevel + lv->MaxLevel) / 2);
                if (mid > 0)
                    req.target_level_override = uint8(std::clamp(int(mid), 1, int(MaxPlayerLevel())));
            }
        }
    }
    req.faction = uint32(player->GetTeam());
    req.instance_id = dungeon_id;
    req.requesting_player = player;
    filler.Fill(req);

    // Enqueue for periodic top-up. Subsequent Fill retries strip the
    // requesting_player pointer (it could go stale across 30s) and
    // also clear the role-deficit at refill time by counting bots
    // already in the queue for this dungeon.
    {
        Fleet::BotQueueFiller::FillRequest persisted = req;
        persisted.requesting_player = nullptr;
        const uint32 now_ms = getMSTime();
        std::lock_guard lk(pending_lfg_refills_mtx_);
        // Replace any existing entry for this player (re-queue resets).
        const uint64 pguid = player->GetGUID().GetCounter();
        std::erase_if(pending_lfg_refills_,
            [pguid](PendingLfgRefill const& e){ return e.player_guid_low == pguid; });
        pending_lfg_refills_.push_back(PendingLfgRefill{
            pguid, std::move(persisted), now_ms, now_ms});
    }

    // BotCoordinationBus publish â€” fan out role-need signals so any
    // subscriber (BotGroupBuilder, future analytics) can react.
    // The original BotQueueFiller path above still handles the
    // primary fill; this publishes a coarse "tank/healer/dps needed"
    // event with the requesting player's level as the target so role-
    // specific subscribers can pick targeted bots.
    if (Services::Initialized())
    {
        V2::CoordEvent ev{};
        ev.origin_low   = player->GetGUID().GetCounter();
        ev.content_id   = dungeon_id;
        ev.level_min    = uint8(std::max(1, int(player->GetLevel()) - 5));
        ev.level_max    = uint8(std::min(int(MaxPlayerLevel()), int(player->GetLevel()) + 5));
        ev.faction_mask = (player->GetTeam() == ALLIANCE) ? 0x1u : 0x2u;
        // Publish all three role signals so subscribers can pick what
        // they handle. Subscriber filtering happens at the handler
        // level â€” cheap.
        ev.kind = V2::CoordSignal::LfgTankNeeded;
        Services::Coordination().Publish(ev);
        ev.kind = V2::CoordSignal::LfgHealerNeeded;
        Services::Coordination().Publish(ev);
        ev.kind = V2::CoordSignal::LfgDpsNeeded;
        Services::Coordination().Publish(ev);
    }
}

void Module::OnBGInvitationReceived(Player* player, uint32 bg_instance_id, uint32 bg_type_id)
{
    // Synchronous handler fired from BattlegroundQueue::InviteGroupToBG /
    // BGQueueInviteEvent::Execute the instant TC sends the
    // BattlefieldStatusNeedConfirmation packet. The queue is mid-iteration
    // here â€” we MUST NOT mutate BG queue data, AND we MUST NOT port bots
    // in the same tick (or even seconds-later) as the invite if a real
    // human is also invited â€” bots filling the BG can advance its state
    // and silently invalidate the human's invite.
    //
    // Strategy:
    //   - Human invitee: just bump expected_humans for this instance, return.
    //   - Bot invitee: enqueue deferred port; FireDueBgPorts (run each tick
    //     before DrainIntents) fires it once gating passes:
    //       * 0 expected humans â†’ fire after a 0-1500ms stagger.
    //       * 1+ expected humans â†’ fire only once a human is in the BG
    //         instance (sticky bool, then small per-bot stagger).
    //       * 80s hard expiry â†’ drop (TC's 90s INVITE_ACCEPT_WAIT_TIME -10s).
    //   - Snapshot-poll idle:bg_port_accept rule is suppressed for bg_type_id
    //     via BgPort cooldown stamp so it can't preempt the gating.
    if (!initialized_ || !player) return;

    uint32 const now_ms = GameTime::GetGameTimeMS();

    bool const is_bot = player->GetSession() && player->GetSession()->IsBot();
    if (!is_bot)
    {
        std::lock_guard lk(pending_bg_ports_mtx_);
        ++bg_invite_state_[bg_instance_id].expected_humans;
        return;
    }

    BotId const id = player->GetGUID().GetCounter();
    auto& reg = Services::Registry();
    if (!reg.has(id)) return;

    // CRASH FIX 2026-05-13: TC fires this hook from BOTH
    // BattlegroundQueue::InviteGroupToBG (initial) AND BGQueueInviteEvent
    // ::Execute (~30s reminder). Without per-(bot,bg_instance) dedup we'd
    // queue two staggered ports for the same bot â€” the second fires after
    // the first port succeeded, re-entering API::bg_port while the bot is
    // mid-teleport, tripping Map::RemovePlayerFromMap's ASSERT(remove)
    // when the bot isn't in any grid (Map.cpp:935). Two guards:
    //   (a) skip if a pending entry for (bot, bg_instance) already exists
    //   (b) skip if the bot is ALREADY in the target BG instance (port
    //       already completed; the reminder is just informational).
    if (player->InBattleground())
    {
        Battleground const* current = player->GetBattleground();
        if (current && current->GetInstanceID() == bg_instance_id)
            return;   // already in this BG â€” nothing to do
    }
    {
        std::lock_guard lk(pending_bg_ports_mtx_);
        for (auto const& p : pending_bg_ports_)
        {
            if (p.bot_id == uint64(id) && p.bg_instance_id == bg_instance_id)
                return;   // dedup
        }
        pending_bg_ports_.push_back({ now_ms, bg_instance_id, uint64(id),
                                       uint16(bg_type_id),
                                       /*bg_template_type_id*/ uint32(bg_type_id) });
    }

    if (BotAI* ai = reg.ai(id))
        ai->note_action_retry(BotAI::ActionKind::BgPort,
                              uint64(bg_type_id), now_ms);

    TC_LOG_INFO("playerbot.v2",
        "[OnBGInvitationReceived] {} deferred port bg_inst={} bg_type_id={} (gating on human-first)",
        player->GetName(), bg_instance_id, bg_type_id);
}

void Module::FireDueBgPorts()
{
    uint32 const now_ms = GameTime::GetGameTimeMS();
    std::vector<PendingBgPort> to_fire;
    {
        std::lock_guard lk(pending_bg_ports_mtx_);
        if (pending_bg_ports_.empty()) return;

        auto it = pending_bg_ports_.begin();
        while (it != pending_bg_ports_.end())
        {
            // Expire stale entries (TC's INVITE_ACCEPT_WAIT_TIME is 90s).
            if (now_ms - it->created_at_ms > 80u * 1000u)
            {
                it = pending_bg_ports_.erase(it);
                continue;
            }

            auto state_it = bg_invite_state_.find(it->bg_instance_id);
            uint32 const expected_humans = (state_it != bg_invite_state_.end())
                                               ? state_it->second.expected_humans : 0u;
            bool         human_in_bg     = (state_it != bg_invite_state_.end())
                                               ? state_it->second.any_human_seen : false;

            // No humans invited â†’ fire after light per-bot stagger so the BG
            // doesn't flood-start the same tick.
            if (expected_humans == 0)
            {
                // A human of this type is queued and not invited yet: TC's prep timer starts with the first player inside, so
                // bots going in now would burn the human's wait. Hold until the human has an invitation (80 s expiry above).
                if (Battleground const* held_bg = sBattlegroundMgr->GetBattleground(it->bg_instance_id, BattlegroundTypeId(it->bg_template_type_id)))
                    if (Fleet::BotQueueFiller::HasWaitingHuman(it->bg_template_type_id, uint8(held_bg->GetBracketId())))
                    {
                        ++it;
                        continue;
                    }
                uint32 const stagger_ms = uint32(it->bot_id % 1500u);
                if (now_ms - it->created_at_ms >= stagger_ms)
                {
                    to_fire.push_back(*it);
                    it = pending_bg_ports_.erase(it);
                }
                else
                    ++it;
                continue;
            }

            // Humans invited â†’ only fire once a human is actually IN the BG.
            // Once flagged sticky, the gate stays open for that instance.
            if (!human_in_bg)
            {
                Battleground* bg = sBattlegroundMgr->GetBattleground(
                    it->bg_instance_id,
                    BattlegroundTypeId(it->bg_template_type_id));
                if (bg)
                {
                    for (auto const& [guid, _] : bg->GetPlayers())
                    {
                        Player* p = ObjectAccessor::FindPlayer(guid);
                        if (p && p->GetSession() && !p->GetSession()->IsBot())
                        {
                            human_in_bg = true;
                            if (state_it != bg_invite_state_.end())
                                state_it->second.any_human_seen = true;
                            break;
                        }
                    }
                }
                if (!human_in_bg) { ++it; continue; }   // wait
            }

            // Gate open. Apply a small 0-1500ms per-bot stagger from the
            // moment the gate opened (created_at + 0 if gate already open
            // at invite-time, or current tick-time otherwise â€” using created_at
            // here gives a deterministic spread per bot anchored to invite).
            uint32 const stagger_ms = uint32(it->bot_id % 1500u);
            if (now_ms - it->created_at_ms >= stagger_ms)
            {
                to_fire.push_back(*it);
                it = pending_bg_ports_.erase(it);
            }
            else
                ++it;
        }
    }
    if (to_fire.empty()) return;

    auto& reg = Services::Registry();
    for (auto const& p : to_fire)
    {
        BotId const id = BotId(p.bot_id);
        if (!reg.has(id)) continue;

        // CRASH FIX 2026-05-13: belt-and-braces re-check at fire time â€”
        // between push and fire, the bot may have already entered the BG
        // (via the snapshot-poll fallback or a prior hook fire). Pushing
        // another BgPortIntent in that state re-enters API::bg_port with
        // currentBg == target_bg and races the teleport flow, tripping
        // Map::RemovePlayerFromMap ASSERT. Skip if already in target.
        Player* bot = ObjectAccessor::FindConnectedPlayer(
            ObjectGuid::Create<HighGuid::Player>(uint64(p.bot_id)));
        if (bot)
        {
            if (Battleground const* cur = bot->GetBattleground())
                if (cur->GetInstanceID() == p.bg_instance_id)
                    continue;
        }

        IntentQueue* iq = reg.intents(id);
        IntentId*    nid = reg.next_intent_id(id);
        if (!iq || !nid) continue;

        Intent intent;
        intent.id              = ++(*nid);
        intent.bot_id          = id;
        intent.source_snapshot = 0;
        intent.body            = QueueIntent{BgPortIntent{p.bg_type_id, /*accept*/ true}};
        iq->push(std::move(intent));
        if (bot)
        {
            TC_LOG_INFO("playerbot.v2",
                "[BgPortFire] {} faction={} bg_inst={} bg_type={} (deferred {}ms)",
                bot->GetName(),
                bot->GetTeam() == ALLIANCE ? "ALLIANCE" : "HORDE",
                p.bg_instance_id, p.bg_type_id,
                now_ms - p.created_at_ms);
        }
    }
}

void Module::OnLfgProposalReceived(Player* player, uint32 proposal_id)
{
    // Synchronous handler fired from LFGMgr::AddProposal. The proposal table
    // is mid-iteration here so we MUST NOT call sLFGMgr->UpdateProposal
    // directly â€” push an LfgProposalRespondIntent which drains safely on
    // the next world-tick DrainIntents pass.
    if (!initialized_ || !player) return;
    if (!player->GetSession() || !player->GetSession()->IsBot()) return;
    if (proposal_id == 0) return;

    BotId const id = player->GetGUID().GetCounter();
    auto& reg = Services::Registry();
    if (!reg.has(id)) return;

    uint32 const now_ms = GameTime::GetGameTimeMS();
    {
        std::lock_guard lk(pending_lfg_accepts_mtx_);
        pending_lfg_accepts_.push_back({ now_ms, uint64(id), proposal_id });
    }
    // Suppress snapshot-poll idle:lfg_proposal_accept so it can't double-fire.
    if (BotAI* ai = reg.ai(id))
        ai->set_lfg_proposal_acked_id(proposal_id);

    TC_LOG_INFO("playerbot.v2",
        "[OnLfgProposalReceived] {} deferred accept proposal_id={}",
        player->GetName(), proposal_id);
}

void Module::FireDueLfgAccepts()
{
    uint32 const now_ms = GameTime::GetGameTimeMS();
    std::vector<PendingLfgAccept> to_fire;
    {
        std::lock_guard lk(pending_lfg_accepts_mtx_);
        if (pending_lfg_accepts_.empty()) return;
        auto it = pending_lfg_accepts_.begin();
        while (it != pending_lfg_accepts_.end())
        {
            // Expire after 35s (TC's LFG_TIME_PROPOSAL is 40s).
            if (now_ms - it->created_at_ms > 35u * 1000u)
            {
                it = pending_lfg_accepts_.erase(it);
                continue;
            }
            // 0-800ms per-bot stagger so 5 (or 25) bots don't all fire intents
            // the exact same tick. Deterministic per bot_id.
            uint32 const stagger_ms = uint32(it->bot_id % 800u);
            if (now_ms - it->created_at_ms >= stagger_ms)
            {
                to_fire.push_back(*it);
                it = pending_lfg_accepts_.erase(it);
            }
            else
                ++it;
        }
    }
    if (to_fire.empty()) return;

    auto& reg = Services::Registry();
    for (auto const& p : to_fire)
    {
        BotId const id = BotId(p.bot_id);
        if (!reg.has(id)) continue;
        IntentQueue* iq = reg.intents(id);
        IntentId*    nid = reg.next_intent_id(id);
        if (!iq || !nid) continue;

        Intent intent;
        intent.id              = ++(*nid);
        intent.bot_id          = id;
        intent.source_snapshot = 0;
        intent.body            = QueueIntent{LfgProposalRespondIntent{p.proposal_id, /*accept*/ true}};
        iq->push(std::move(intent));
    }
}

void Module::TopUpPendingLfg(uint32 now_ms)
{
    constexpr uint32 kLfgTopUpIntervalMs = 30u * 1000u;

    // Snapshot the entries to refill outside the lock so the Fill calls
    // (which may take >1ms each) don't hold the mutex.
    std::vector<PendingLfgRefill> to_refill;
    {
        std::lock_guard lk(pending_lfg_refills_mtx_);
        auto it = pending_lfg_refills_.begin();
        while (it != pending_lfg_refills_.end())
        {
            // Drop entries older than 15 min â€” by then the queue has
            // either popped, been canceled, or gone stale (the player
            // moved on). Without this entries leak forever on logout.
            if (now_ms - it->created_at_ms > 15u * 60u * 1000u)
            {
                it = pending_lfg_refills_.erase(it);
                continue;
            }
            // Drop entries whose player is no longer in LFG_STATE_QUEUED.
            // sLFGMgr->GetState returns LFG_STATE_NONE for offline / not-
            // queued, LFG_STATE_PROPOSAL once a match formed, etc. Either
            // way the top-up is no longer needed.
            ObjectGuid pguid = ObjectGuid::Create<HighGuid::Player>(it->player_guid_low);
            lfg::LfgState const st = sLFGMgr->GetState(pguid);
            if (st != lfg::LFG_STATE_QUEUED)
            {
                it = pending_lfg_refills_.erase(it);
                continue;
            }
            // Cron cadence: refill at most every 30s per entry.
            if (now_ms - it->last_refill_ms < kLfgTopUpIntervalMs)
            {
                ++it;
                continue;
            }
            it->last_refill_ms = now_ms;
            to_refill.push_back(*it);
            ++it;
        }
    }

    if (to_refill.empty()) return;

    auto& reg = Services::Lifecycle();
    auto ids  = reg.snapshot_ids();
    for (auto& entry : to_refill)
    {
        // Count our online V2 bots already in LFG queue for this dungeon
        // by role. Subtract from the kind's default composition to get
        // the deficit. The walk is O(online bots) â€” bounded by what's
        // logged in, scales fine.
        uint32 q_t = 0, q_h = 0, q_d = 0;
        for (BotId id : ids)
        {
            ObjectGuid g = ObjectGuid::Create<HighGuid::Player>(id);
            Player* bot = ObjectAccessor::FindConnectedPlayer(g);
            if (!bot) continue;
            if (sLFGMgr->GetState(g) != lfg::LFG_STATE_QUEUED) continue;
            lfg::LfgDungeonSet const& dgs = sLFGMgr->GetSelectedDungeons(g);
            if (!dgs.count(entry.req.instance_id)) continue;
            uint8 const roles = sLFGMgr->GetRoles(g);
            if      (roles & lfg::PLAYER_ROLE_TANK)   ++q_t;
            else if (roles & lfg::PLAYER_ROLE_HEALER) ++q_h;
            else                                      ++q_d;
        }
        // Composition target. 5-man dungeon default: 1T/1H/3D minus
        // human player (-1 to whichever role they queued as â€” but
        // approximate by subtracting from DPS since most humans queue
        // DPS; if T/H human, we'd over-spawn by one which the LFG
        // queue de-dups anyway). For raids the kind sets bigger values
        // via NeedsFor in BotQueueFiller â€” here we hard-code 5-man
        // since that's what dungeon top-ups are about.
        uint8 target_t = 1, target_h = 1, target_d = 3;
        if (entry.req.kind == Fleet::BotQueueFiller::QueueKind::Raid10)
            { target_t = 2; target_h = 3; target_d = 5; }
        else if (entry.req.kind == Fleet::BotQueueFiller::QueueKind::Raid20)
            { target_t = 2; target_h = 4; target_d = 14; }

        const uint8 need_t = (q_t >= target_t) ? 0 : uint8(target_t - q_t);
        const uint8 need_h = (q_h >= target_h) ? 0 : uint8(target_h - q_h);
        const uint8 need_d = (q_d >= target_d) ? 0 : uint8(target_d - q_d);
        if (need_t + need_h + need_d == 0)
        {
            TC_LOG_INFO("playerbot.v2",
                "[LfgTopUp] dungeon={} queue full ({}T/{}H/{}D); skipping",
                entry.req.instance_id, q_t, q_h, q_d);
            continue;
        }

        Fleet::BotQueueFiller::FillRequest req = entry.req;
        req.needs_tank_override   = need_t;
        req.needs_healer_override = need_h;
        req.needs_dps_override    = need_d;
        // Pass null player â€” the original is potentially offline / moved
        // by now. BotQueueFiller doesn't strictly need the pointer; it
        // uses req.faction and target_level_override for filtering.
        req.requesting_player = nullptr;
        TC_LOG_INFO("playerbot.v2",
            "[LfgTopUp] dungeon={} bot-queued so far {}T/{}H/{}D; refilling {}T/{}H/{}D",
            entry.req.instance_id, q_t, q_h, q_d, need_t, need_h, need_d);

        Fleet::BotQueueFiller filler;
        filler.Fill(req);
    }
}

void Module::OnBGPortFailed(Player* player, uint8 reason_code, uint32 bg_instance_id)
{
    if (!initialized_ || !player) return;
    // Inert for bots â€” they port via the V2 BgPortIntent pathway and don't
    // use HandleBattleFieldPortOpcode. Diagnostic is for human players.
    if (player->GetSession() && player->GetSession()->IsBot()) return;

    char const* reason_name = "unknown";
    switch (reason_code)
    {
        case 1: reason_name = "not_in_queue"; break;
        case 2: reason_name = "invalid_queue_slot"; break;
        case 3: reason_name = "no_group_info"; break;
        case 4: reason_name = "ginfo_not_invited"; break;
        case 5: reason_name = "bg_instance_gone"; break;
        case 6: reason_name = "no_bracket_entry"; break;
        case 7: reason_name = "freeze_debuff"; break;
        case 8: reason_name = "player_invite_flag_cleared"; break;
    }
    TC_LOG_WARN("playerbot.v2",
        "[OnBGPortFailed] player={} reason={} ({}) bg_instance={}",
        player->GetName(), uint32(reason_code), reason_name, bg_instance_id);
}

void Module::OnPathOutcome(uint8 outcome)
{
    if (!initialized_) return;
    Services::Perf().record_path_outcome(static_cast<PerfCounters::PathOutcome>(outcome));
}

} // namespace Playerbot::V2
