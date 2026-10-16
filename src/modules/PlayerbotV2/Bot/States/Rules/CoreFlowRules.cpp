// CoreFlowRules - Refactor #3 pass 13. Migrates a small family of urgent
// "always preempt" rules that benefit from the two-stage dispatch model:
//   - idle:swim_stuck_walk_to_hub / idle:swim_stuck_hearth (priority 870)
//     A bot stuck swimming > 90s walks to the nearest quest hub on map or
//     hearths out. Lower priority than survival hazards (which can also
//     kill a bot), higher than every utility / questing rule.
//   - idle:group_convert_to_raid (priority 860) — leader auto-converts to
//     raid when inside a raid map.
//   - Acceptance family: group_invite_accept (855), guild_invite_accept (853),
//     lfg_proposal_accept (850), bg_port_accept (847). All preempt the inline
//     cascade so the bot reacts the moment the invite/proposal/port arrives.
// All these register at priority >= 700 so they fire from the high-priority
// top-of-tick dispatch in `State_Idle::DispatchIdle`.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "Services.h"
#include "Util/ConfigReader.h"   // Playerbot.Behavior.PoliteWorldBots
#include "Fleet/OwnerRegistry.h"
#include "Fleet/BotPool.h"       // Pool::IsScenarioBot - scenario squads never invite or accept
#include "Bot/BotRegistry.h"
#include "Group/GroupSnapshot.h"
#include "Travel/QuestHubDatabase.h"
#include "Travel/PortalIndex.h"
#include "Travel/UnifiedTravelGraph.h"
#include "Bot/States/MaintainHelpers.h"
#include "GameTime.h"
#include "DB2Stores.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Log.h"
#include <cmath>
#include <limits>
#include <vector>

namespace Playerbot {

namespace {

// ---------- idle:swim_stuck ----------
bool SwimStuckGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    const uint32 swim_now_ms = s.published_at_ms();
    ai.update_swim_state(s.is_swimming(), swim_now_ms);
    if (!ai.swim_stuck(swim_now_ms)) return false;
    return !s.in_battleground() && !s.is_in_dungeon();
}

bool SwimStuckFire(BotSnapshotView const& s, BotAI& ai,
                   GroupSnapshotView const&,
                   BotIntentEmitter& emit, uint32)
{
    const uint32 swim_now_ms = s.published_at_ms();
    ::Playerbot::V2::Travel::QuestHub const* hub =
        Services::Hubs().GetNearestQuestHub(s.raw());
    if (hub && hub->mapId == s.map_id())
    {
        emit.move_to(hub->location.GetPositionX(),
                     hub->location.GetPositionY(),
                     hub->location.GetPositionZ(),
                     /*run*/ true);
        ai.set_last_rule_fired("idle:swim_stuck_walk_to_hub");
        ai.update_swim_state(false, swim_now_ms);
        return true;
    }
    emit.hearth();
    ai.set_last_rule_fired("idle:swim_stuck_hearth");
    ai.update_swim_state(false, swim_now_ms);
    return true;
}

// ---------- idle:water_escape ----------
// Open-world water-escape FOCUS (FIX #12). When the bot is in swim-water the
// builder resolves the nearest DRY navmesh footing (s.water_escape_*). This rule
// drives straight to it at priority 865 — above ALL travel/quest/wander movement
// (<700) — so while the bot is in the water NOTHING else moves it. Without this
// the bot oscillates forever: the move_to water-exit recovery pulls it to shore,
// then travel/wander immediately walk it back into the water (Tindle, Stormwind
// harbor, 2026-06-19). Sits just BELOW idle:swim_stuck (870): if the bot can't
// reach dry footing and stays swim-stuck long enough, swim_stuck's hub/hearth
// escalation takes over (its gate still runs every tick and updates the swim
// timer). Once the bot is on land the builder clears water_escape_valid and the
// normal rules (now-correct travel) resume.
bool WaterEscapeGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    return s.water_escape_valid()
        && s.is_alive() && !s.in_combat() && !s.is_casting()
        && !s.on_transport()
        && !s.in_battleground() && !s.is_in_dungeon();
}

bool WaterEscapeFire(BotSnapshotView const& s, BotAI& ai,
                     GroupSnapshotView const&,
                     BotIntentEmitter& emit, uint32)
{
    emit.move_to(s.water_escape_x(), s.water_escape_y(), s.water_escape_z(),
                 /*run*/ true);
    ai.set_last_rule_fired("idle:water_escape");
    return true;
}

// ---------- idle:group_convert_to_raid ----------
bool GroupConvertGate(BotSnapshotView const& s, BotAI&,
                     GroupSnapshotView const& g, uint32)
{
    return g.exists() && g.leader() == s.guid() && g.is_party() &&
           s.raw().instance_ctx.is_in_raid;
}

bool GroupConvertFire(BotSnapshotView const&, BotAI& ai,
                      GroupSnapshotView const&,
                      BotIntentEmitter& emit, uint32)
{
    emit.group_convert_to_raid();
    ai.set_last_rule_fired("idle:group_convert_to_raid");
    return true;
}

// ---------- idle:group_invite_accept ----------
// Edge-trigger gate: snapshot keeps `has_group_invite` true for a few ticks
// after the bot fires GroupAcceptIntent (world thread processes the accept
// async). The `invite_acked` BotAI flag dedups the emit within that window
// AND auto-clears when the invite goes away. The gate has a side effect
// (clearing the flag) — that's a one-off state-machine cleanup, runs once
// per invite cycle.
bool GroupInviteAcceptGate(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&, uint32)
{
    // A scenario squad member belongs to the run's group and takes no other (mc_garr: the solo squad bots paired off with each other
    // while the raid was staged, and 20 of 40 were out of the raid group when the run began).
    if (Pool::IsScenarioBot(s.bot_id()))
        return false;
    if (!s.has_group_invite())
    {
        if (ai.invite_acked()) ai.set_invite_acked(false);
        if (ai.pending_group_invite_accept_at_ms() != 0)
            ai.set_pending_group_invite_accept_at_ms(0);
        return false;
    }
    if (ai.invite_acked()) return false;
    // Hesitation: 500–2500ms per-bot before accepting. Without this the
    // accept packet arrives <100ms after the invite, in lockstep across
    // an entire bot squad — easiest possible anti-bot tell.
    const uint32 now_ms = s.published_at_ms();
    uint32 accept_at = ai.pending_group_invite_accept_at_ms();
    if (accept_at == 0)
    {
        const uint32 jitter = 500u + (uint32(s.bot_id()) * 2654435761u) % 2000u;
        ai.set_pending_group_invite_accept_at_ms(now_ms + jitter);
        TC_LOG_INFO("playerbot.v2", "[group_invite] {} saw an invite from {}{}, answers in {} ms",
                    s.name(), s.group_invite_leader_name(), s.group_invite_leader_is_real() ? " (player)" : "", jitter);
        return false;
    }
    return now_ms >= accept_at;
}

bool GroupInviteAcceptFire(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&,
                           BotIntentEmitter& emit, uint32)
{
    // Playerbot.Behavior.PoliteWorldBots: a world bot (no owner) that is busy, or whose would-be leader is well
    // above its level, turns a REAL player's invite down with a short whisper instead of being dragged along.
    if (Services::Config().behavior_polite_world_bots() && !s.has_owner_character() &&
        s.group_invite_leader_is_real() && !s.group_invite_leader_name().empty())
    {
        char const* why = nullptr;
        if (!s.is_alive())
            why = "sorry, can't right now";
        else if (s.in_combat())
            why = "sorry, in the middle of a fight";
        else if (s.in_battleground())
            why = "sorry, busy in a battleground";
        else if (int32(s.group_invite_leader_level()) > int32(s.level()) + 5 && !s.group_invite_leader_privileged())
            why = "sorry, you're a bit too high level for me";
        if (why)
        {
            TC_LOG_INFO("playerbot.v2", "[group_invite] {} declines {}: {}", s.name(), s.group_invite_leader_name(), why);
            // A decline the inviter can see even when whispers do not reach them: the bot says it out loud as well.
            emit.say_world(why);
            emit.whisper(s.group_invite_leader_name(), why);
            emit.group_decline();
            ai.set_invite_acked(true);
            ai.set_last_rule_fired("idle:group_invite_decline");
            return true;
        }
    }
    TC_LOG_INFO("playerbot.v2", "[group_invite] {} accepts the invite from {}", s.name(), s.group_invite_leader_name());
    emit.group_accept();
    ai.set_invite_acked(true);
    ai.set_last_rule_fired("idle:group_invite_accept");
    return true;
}

// ---------- idle:guild_invite_accept ----------
bool GuildInviteAcceptGate(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&, uint32)
{
    if (!s.has_guild_invite())
    {
        if (ai.guild_invite_acked()) ai.set_guild_invite_acked(false);
        return false;
    }
    return !ai.guild_invite_acked();
}

bool GuildInviteAcceptFire(BotSnapshotView const&, BotAI& ai,
                           GroupSnapshotView const&,
                           BotIntentEmitter& emit, uint32)
{
    emit.guild_accept_invite();
    ai.set_guild_invite_acked(true);
    ai.set_last_rule_fired("idle:guild_invite_accept");
    return true;
}

// ---------- idle:lfg_proposal_accept ----------
bool LfgProposalAcceptGate(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&, uint32)
{
    if (!s.has_lfg_proposal())
    {
        if (ai.lfg_proposal_acked_id() != 0) ai.set_lfg_proposal_acked_id(0);
        if (ai.pending_lfg_proposal_accept_at_ms() != 0)
            ai.set_pending_lfg_proposal_accept_at_ms(0);
        return false;
    }
    const uint32 pid = s.lfg_proposal_id();
    if (pid == 0 || ai.lfg_proposal_acked_id() == pid) return false;
    // Hesitation: real players see the LFG popup and pause to read the
    // role/dungeon line — 800–3000ms accept-at jitter. Without it the
    // entire queued group accepts inside 100ms, which is impossible for
    // a human-played party of 5.
    const uint32 now_ms = s.published_at_ms();
    uint32 accept_at = ai.pending_lfg_proposal_accept_at_ms();
    if (accept_at == 0)
    {
        const uint32 jitter = 800u + (uint32(s.bot_id()) * 2654435761u) % 2200u;
        ai.set_pending_lfg_proposal_accept_at_ms(now_ms + jitter);
        return false;
    }
    return now_ms >= accept_at;
}

bool LfgProposalAcceptFire(BotSnapshotView const& s, BotAI& ai,
                           GroupSnapshotView const&,
                           BotIntentEmitter& emit, uint32)
{
    const uint32 pid = s.lfg_proposal_id();
    emit.lfg_proposal_respond(pid, /*accept*/ true);
    ai.set_lfg_proposal_acked_id(pid);
    ai.set_last_rule_fired("idle:lfg_proposal_accept");
    return true;
}

// ---------- idle:bg_port_accept ----------
bool BgPortAcceptGate(BotSnapshotView const& s, BotAI& ai,
                      GroupSnapshotView const&, uint32)
{
    const uint32 port_now_ms = GameTime::GetGameTimeMS();
    for (auto const& q : s.raw().bg.queues)
    {
        if (q.invited_to_instance == 0) continue;
        if (ai.action_recently_tried(BotAI::ActionKind::BgPort,
                                     uint64(q.bg_type_id), port_now_ms))
            continue;
        return true;
    }
    return false;
}

bool BgPortAcceptFire(BotSnapshotView const& s, BotAI& ai,
                      GroupSnapshotView const&,
                      BotIntentEmitter& emit, uint32)
{
    const uint32 port_now_ms = GameTime::GetGameTimeMS();
    for (auto const& q : s.raw().bg.queues)
    {
        if (q.invited_to_instance == 0) continue;
        if (ai.action_recently_tried(BotAI::ActionKind::BgPort,
                                     uint64(q.bg_type_id), port_now_ms))
            continue;
        emit.bg_port(q.bg_type_id, /*accept*/ true);
        ai.note_action_retry(BotAI::ActionKind::BgPort,
                             uint64(q.bg_type_id), port_now_ms);
        ai.set_last_rule_fired("idle:bg_port_accept");
        return true;
    }
    return false;
}

// ---------- idle:bg_jit_staging ----------
// Owner directive 2026-06-22: a JIT bot (spawned ONLY to fill a BG queue) must do
// nothing but its purpose. While it holds a JIT purpose and is not yet inside the
// instance, confine it: ensure it's queued for its BG, then stand STAGED and wait
// for the invite — no questing, roaming, grinding, or vendor trips (those lower
// rules are suppressed because this consumes the tick). That confinement is also
// what makes matches FORM: staged bots stay queued + ready so both factions reach
// MinPlayers and the matchmaker can invite (JIT bots used to wander off / get into
// combat and never assemble, so invites never fired). An invited bot is handled by
// idle:bg_port_accept (847, just above this). A hard staging timeout clears the
// purpose so a bot whose match never forms reverts and the TotalTarget=0 LRU
// retires the surplus instead of it staging forever.
static constexpr uint32 kJitStageTimeoutMs = 12u * 60u * 1000u;

bool BgJitStagingGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ai.is_jit_purpose() && !s.in_battleground();
}

bool BgJitStagingFire(BotSnapshotView const& /*s*/, BotAI& ai, GroupSnapshotView const&,
                      BotIntentEmitter& /*emit*/, uint32)
{
    const uint32 now_ms = GameTime::GetGameTimeMS();
    // Time out a match that never formed: clear the purpose and yield so the
    // population manager's LRU (TotalTarget=0) retires the surplus bot.
    if (ai.jit_purpose_set_ms() != 0 &&
        (now_ms - ai.jit_purpose_set_ms()) > kJitStageTimeoutMs)
    {
        ai.clear_jit_purpose();
        return false;
    }
    // Stand staged: whether currently queued (waiting for the invite) or PARKED
    // between purposes (post-match, available for reuse), the bot does nothing but
    // wait — consuming the tick suppresses every lower behavior rule (no questing,
    // roaming, grinding, or vendor trips). It does NOT auto-re-queue: a parked JIT
    // bot stays idle and AVAILABLE so the filler's online-first pass can REUSE it
    // for the next purpose (avoiding the costly create+setup); if nothing reuses it,
    // its 10-min kick-protect lease lapses and the TotalTarget=0 LRU logs it out
    // (owner directive: no dead bots idling around, but reuse to save creation
    // load). The initial queue comes from the setup pipeline; reuse re-queues via
    // the filler.
    ai.set_last_rule_fired("idle:bg_jit_staging_wait");
    return true;
}

// ---------- idle:rejoin_group_instance ----------
// A living bot outside the instance its group is in walks back to the
// instance's entrance area trigger and zones in through it, as a player does
// (CMSG_AREA_TRIGGER via API::enter_area_trigger, which runs the server's own
// checks: raid group, lock, instance full / in combat). This is how a
// companion follows its owner in through Molten Core's portal, and how a bot
// that released inside a dungeon or raid gets back: RepopAtGraveyard revives
// it ALIVE at the graveyard outside (never a ghost), so State_Dead's ghost
// portal run does not apply.
//
// A companion goes where its owner goes: it rejoins only an owner who is
// alive inside an instance. While the owner is on its own map the group layer
// follows the owner (State_InGroup); while the owner lies dead inside it
// waits at the graveyard for the owner to release and come out with the
// group, instead of zoning into a wiped instance alone. Other bots anchor on
// a living human inside, else a living bot inside.
//
// When the entrance trigger is not within kWalkRange on this map (a bot that
// logged in at its hearth inn after a restart / disconnect while the group
// stayed inside, or one whose instance lies on another continent) it travels
// there for real (RejoinTravelFar): the entrance is resolved anywhere in the
// world, and the shared travel-graph executor drives the bot by road, flight,
// boat, zeppelin, the Deeprun Tram and portals, mounted on long legs. Never a
// port to the group: an entrance no route reaches is logged and the bot waits.
bool RejoinInstanceGate(BotSnapshotView const& s, BotAI&,
                        GroupSnapshotView const& g, uint32)
{
    // Mounted is fine: a companion rides behind its owner up to the portal
    // and crosses mounted, as the owner does, and rides the long legs of a
    // far journey.
    if (!s.is_alive() || s.in_combat() || s.is_casting()) return false;
    return g.exists();
}

// Instance-entrance area triggers: areatrigger_teleport rows whose trigger
// volume lies on one map and whose destination is an instanceable map. Built
// once from load-time stores (AreaTrigger.db2, ObjectMgr's trigger map), both
// read-only after load, so the bot logic thread may read them. The same set the
// snapshot builder resolves for a ghost's portal run.
struct InstanceEntranceTrigger { uint32 at_id; uint32 src_map; uint32 dst_map; float x, y, z; };
std::vector<InstanceEntranceTrigger> const& InstanceEntranceTriggers()
{
    static std::vector<InstanceEntranceTrigger> const triggers = []
    {
        std::vector<InstanceEntranceTrigger> v;
        for (AreaTriggerEntry const* at : sAreaTriggerStore)
        {
            if (!at) continue;
            AreaTriggerTeleport const* dest = sObjectMgr->GetAreaTrigger(at->ID);
            if (!dest) continue;
            MapEntry const* dme = sMapStore.LookupEntry(dest->Loc.GetMapId());
            if (!dme || !dme->Instanceable() || uint32(at->ContinentID) == dest->Loc.GetMapId()) continue;
            v.push_back({ at->ID, uint32(at->ContinentID), dest->Loc.GetMapId(), at->Pos.X, at->Pos.Y, at->Pos.Z });
        }
        return v;
    }();
    return triggers;
}

// The nearest trigger on `from_map` into `target_map`, or into an instance of
// target_map's parent chain (InstanceTemplate::Parent: enter the outer
// instance first; its inner portal is then on the bot's new map).
InstanceEntranceTrigger const* FindEntranceTrigger(uint32 from_map, float x, float y, uint32 target_map)
{
    InstanceEntranceTrigger const* best = nullptr;
    int best_rank = 0;
    float best_d2 = 0.f;
    for (InstanceEntranceTrigger const& t : InstanceEntranceTriggers())
    {
        if (t.src_map != from_map) continue;
        int rank = 0;
        if (t.dst_map == target_map)
            rank = 2;
        else
        {
            uint32 chain = target_map;
            for (int hops = 0; hops < 4 && chain; ++hops)
            {
                InstanceTemplate const* it = sObjectMgr->GetInstanceTemplate(chain);
                chain = it ? it->Parent : 0;
                if (chain && chain == t.dst_map) { rank = 1; break; }
            }
        }
        if (rank == 0) continue;
        float const d2 = (t.x - x) * (t.x - x) + (t.y - y) * (t.y - y);
        if (!best || rank > best_rank || (rank == best_rank && d2 < best_d2))
        { best = &t; best_rank = rank; best_d2 = d2; }
    }
    return best;
}

bool IsInstanceMap(uint32 map_id)
{
    MapEntry const* me = sMapStore.LookupEntry(map_id);
    return me && (me->IsDungeon() || me->IsRaid());
}

// The entrance trigger a bot on `from_map` heads for to reach `target_map`,
// at any distance: the nearest one on its own map when there is one, else one
// standing on a continent anywhere in the world (the travel graph routes
// continents only). An instance whose entrances all sit inside another
// instance (or whose InstanceTemplate::Parent names one) is reached through
// that outer instance: the search moves out one level and repeats.
InstanceEntranceTrigger const* ResolveEntranceTrigger(uint32 from_map, float x, float y, uint32 target_map)
{
    uint32 want = target_map;
    for (int hops = 0; hops < 4 && want; ++hops)
    {
        if (InstanceEntranceTrigger const* here = FindEntranceTrigger(from_map, x, y, want))
            return here;
        InstanceEntranceTrigger const* world = nullptr;
        uint32 outer = 0;
        for (InstanceEntranceTrigger const& t : InstanceEntranceTriggers())
        {
            if (t.dst_map != want) continue;
            MapEntry const* sme = sMapStore.LookupEntry(t.src_map);
            if (!sme) continue;
            if (!sme->Instanceable())
            {
                if (!world || t.at_id < world->at_id) world = &t;   // stable pick
            }
            else if (!outer)
                outer = t.src_map;
        }
        if (world) return world;
        if (!outer)
            if (InstanceTemplate const* it = sObjectMgr->GetInstanceTemplate(want))
                outer = it->Parent;
        if (outer == want) break;
        want = outer;
    }
    return nullptr;
}

// Drop the rejoin bookkeeping; a far journey's travel plan goes with it.
void ResetRejoin(BotAI& ai)
{
    BotAI::RejoinRun& rr = ai.rejoin_run();
    if (rr.far_at_id != 0)
        ai.clear_travel_plan();
    rr = BotAI::RejoinRun{};
}

// Aboard a ship, zeppelin or tram car on the rejoin journey: hold still and
// step off at the leg's arrival. A same-map ride ends at the leg's arrival
// dock / station (as idle:transport_ride); a cross-map ride ends once the
// transport pauses on the leg's destination map (that rule keys a cross-map
// arrival on a quest POI, and is a world errand a companion never runs).
// Returns false when the bot rides no such transport (a lift: the elevator
// rules own it), and when it has lost track of the ride at a stop (below).
bool RejoinRide(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit, uint32 now_ms)
{
    using namespace ::Playerbot::V2::Travel;
    if (!s.on_transport()) return false;
    const bool ship = s.transport_is_ship();
    BotAI::PlanLeg const* lg = ai.current_travel_leg();
    const bool same_map_leg = lg && lg->kind == 4 /*Ship*/ && lg->to_map == s.map_id() &&
                              Services::Portals().IsSameMapTransport(lg->payload);
    if (!ship && !same_map_leg) return false;

    float bx, by, bz; s.position(bx, by, bz);
    ObjectGuid ridden;
    float ridden_dsq = std::numeric_limits<float>::max();
    for (auto const& go : s.raw().world_objects.nearby_objects)
    {
        if (go.go_type != (ship ? /*MAP_OBJ_TRANSPORT*/ 15 : /*TRANSPORT*/ 11)) continue;
        if (same_map_leg && !Services::Portals().ServesSameRoute(lg->payload, go.entry)) continue;
        const float dx = go.x - bx, dy = go.y - by, dz = go.z - bz;
        const float dsq = dx * dx + dy * dy + dz * dz;
        if (dsq < ridden_dsq) { ridden_dsq = dsq; ridden = go.guid; }
    }

    // Lost track of the ride: the transport is not in the object sweep (so it
    // cannot be stepped off), or no plan leg names a ride (the plan was dropped
    // or advanced past it), so no arrival can ever be recognised. Holding would
    // ride the transport back and forth forever. While it moves, still hold: a
    // passenger's move_to is relative to the moving deck and walks it off into
    // the sea. Once it pauses at a stop, hand the bot back to the travel
    // executor: a stale plan is dropped (paced, so a GO that stays missing does
    // not replan every tick) and the next one is planned from this stop.
    if (ridden.IsEmpty() || !lg || lg->kind != 4 /*Ship*/)
    {
        if (!s.transport_stopped())
        {
            ai.set_last_rule_fired("idle:rejoin_ride");
            return true;
        }
        BotAI::RejoinRun& rr = ai.rejoin_run();
        if (rr.replan_ms == 0 || now_ms - rr.replan_ms >= 10000u)
        {
            rr.replan_ms = now_ms ? now_ms : 1u;
            TC_LOG_INFO("playerbot.v2",
                "[rejoin] {} aboard a transport stopped on map {} at ({:.0f},{:.0f},{:.0f}) has lost the ride "
                "(transport {}, leg kind {}): replans from here",
                s.name(), s.map_id(), bx, by, bz, ridden.IsEmpty() ? "not in sight" : "in sight",
                lg ? uint32(lg->kind) : 0u);
            ai.clear_travel_plan();
        }
        return false;
    }

    bool arrived = false;
    if (same_map_leg)
    {
        const float dx = lg->to_x - bx, dy = lg->to_y - by;
        const float d2 = dx * dx + dy * dy;
        arrived = ship ? (s.transport_stopped() && d2 <= kShipArrivalRadius * kShipArrivalRadius)
                       : (d2 <= kShuttleArrivalRadius * kShuttleArrivalRadius);
    }
    else
    {
        // Cross-map ride: the current leg names the far shore's map (the ship
        // changes map mid-voyage, so docked on that map = the arrival dock).
        arrived = s.transport_stopped() && lg->to_map == s.map_id();
    }
    if (arrived)
    {
        TC_LOG_INFO("playerbot.v2",
            "[rejoin] {} arrived by transport on map {} at ({:.0f},{:.0f},{:.0f}): steps off",
            s.name(), s.map_id(), bx, by, bz);
        emit.use_game_object(ridden);   // toggle -> RemovePassenger
        ai.set_last_rule_fired("idle:rejoin_ride_disembark");
        return true;
    }
    ai.set_last_rule_fired("idle:rejoin_ride");
    return true;
}

// The journey to an entrance trigger beyond walking range or on another map.
// Reuses the shared travel-graph executor (States::DriveTravelPlanTo, as the
// quest / RPG / far-same-map travel do): walk legs chunked, flights from known
// flight masters only (the graph prunes undiscovered nodes, so a bot new to
// the continent walks and takes boats), same-map boats / zeppelins / the tram,
// continent area-trigger teleports. What that executor leaves to the quest
// cascade (AutoactDispatch, a world errand a companion never runs, keyed on
// the quest POI) is done here: taking a cross-map portal and boarding a
// cross-map ship / zeppelin at its dock. Mounts for long legs.
bool RejoinTravelFar(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit,
                     InstanceEntranceTrigger const& t, GroupMemberSummary const& anchor, uint32 now_ms)
{
    using namespace ::Playerbot::V2::Travel;
    BotAI::RejoinRun& rr = ai.rejoin_run();
    float bx, by, bz;
    s.position(bx, by, bz);
    const bool same_map = t.src_map == s.map_id();
    const float gdx = t.x - bx, gdy = t.y - by;
    const float gd = std::sqrt(gdx * gdx + gdy * gdy);

    if (rr.far_at_id != t.at_id)
    {
        ResetRejoin(ai);
        rr.far_at_id = t.at_id;
        rr.since_ms = now_ms ? now_ms : 1u;
        rr.log_ms = now_ms ? now_ms : 1u;
        TC_LOG_INFO("playerbot.v2",
            "[rejoin] {} on map {} at ({:.0f},{:.0f},{:.0f}) travels to rejoin {} in map {}: entrance trigger {} at ({:.1f},{:.1f},{:.1f}) map {}{}",
            s.name(), s.map_id(), bx, by, bz, anchor.name, anchor.map_id, t.at_id, t.x, t.y, t.z, t.src_map,
            same_map ? fmt::format(", {:.0f}y away", gd) : std::string(" (another map)"));
    }

    if (s.on_transport() && RejoinRide(s, ai, emit, now_ms))
        return true;
    // A move already under way runs out before the next leg / chunk is issued.
    if (s.is_moving())
    {
        ai.set_last_rule_fired("idle:rejoin_travel");
        return true;
    }

    // Plan once per goal (cached by goal key); a failed plan is retried every
    // 60s (a route can open up: the bot learns a flight path, a tile loads).
    ::Playerbot::States::EnsureTravelPlan(s, ai, bx, by, bz, t.src_map, t.x, t.y, t.z,
                                          /*quest_goal=*/false);
    if (!ai.has_travel_plan() && !IsInstanceMap(s.map_id()) &&
        (rr.replan_ms == 0 || now_ms - rr.replan_ms >= 60000u))
    {
        rr.replan_ms = now_ms ? now_ms : 1u;
        ai.clear_travel_plan();
        ::Playerbot::States::EnsureTravelPlan(s, ai, bx, by, bz, t.src_map, t.x, t.y, t.z,
                                          /*quest_goal=*/false);
    }

    // Long walk ahead and outdoors: ride. The next walking target is the
    // current leg's end (walk) or its start (flight master, dock, portal).
    if (!s.is_mounted() && !s.is_indoors() && (rr.mount_ms == 0 || now_ms - rr.mount_ms >= 30000u))
    {
        float wx = t.x, wy = t.y;
        bool on_map = same_map;
        if (BotAI::PlanLeg const* lg = ai.current_travel_leg())
        {
            const bool walk = lg->kind == 1 /*Walk*/;
            wx = walk ? lg->to_x : lg->from_x;
            wy = walk ? lg->to_y : lg->from_y;
            on_map = !walk || lg->to_map == s.map_id();
        }
        const float wdx = wx - bx, wdy = wy - by;
        if (on_map && wdx * wdx + wdy * wdy > 150.0f * 150.0f)
        {
            rr.mount_ms = now_ms ? now_ms : 1u;
            if (emit.mount_appropriate())
            {
                ai.set_last_rule_fired("idle:rejoin_mount");
                return true;
            }
        }
    }

    auto progress_log = [&](char const* how)
    {
        if (rr.log_ms != 0 && now_ms - rr.log_ms < 60000u) return;
        rr.log_ms = now_ms ? now_ms : 1u;
        BotAI::PlanLeg const* lg = ai.current_travel_leg();
        TC_LOG_INFO("playerbot.v2",
            "[rejoin] {} en route to entrance trigger {} (map {}) for {}s: on map {} at ({:.0f},{:.0f},{:.0f}){}, leg {}/{} kind {}, {}",
            s.name(), t.at_id, t.src_map, (now_ms - rr.since_ms) / 1000u, s.map_id(), bx, by, bz,
            same_map ? fmt::format(", {:.0f}y left", gd) : std::string(),
            ai.travel_plan_leg_index() + (lg ? 1 : 0), ai.travel_plan().size(), lg ? uint32(lg->kind) : 0u, how);
    };

    if (::Playerbot::States::DriveTravelPlanTo(s, ai, emit, bx, by, bz, t.src_map, t.x, t.y, t.z,
                                               /*quest_goal=*/false))
    {
        progress_log(ai.last_rule_fired());
        return true;
    }

    // At the anchor of a cross-map portal or ship / zeppelin leg: take it.
    if (BotAI::PlanLeg const* lg = ai.current_travel_leg())
    {
        const bool portal = lg->kind == 3 /*Portal*/;
        const bool xship  = lg->kind == 4 /*Ship*/ && !Services::Portals().IsSameMapTransport(lg->payload);
        if ((portal || xship) && lg->to_map != s.map_id())
        {
            BotSnapshot::NearbyObject const* best = nullptr;
            float best_dsq = std::numeric_limits<float>::max();
            for (auto const& go : s.raw().world_objects.nearby_objects)
            {
                if (go.go_type != (portal ? /*SPELLCASTER*/ 22 : /*MAP_OBJ_TRANSPORT*/ 15)) continue;
                if (go.teleport_dest_map != lg->to_map) continue;
                // The planned transport, not a sibling sharing the dock and the
                // destination map (the two Orgrimmar tower zeppelins).
                if (xship && lg->payload != 0 && go.entry != lg->payload &&
                    !Services::Portals().ServesSameRoute(lg->payload, go.entry))
                    continue;
                const float dx = go.x - bx, dy = go.y - by, dz = go.z - bz;
                const float dsq = dx * dx + dy * dy + dz * dz;
                if (dsq < best_dsq) { best_dsq = dsq; best = &go; }
            }
            if (best && portal)
            {
                if (best_dsq <= 10.0f * 10.0f)
                {
                    emit.use_game_object(best->guid);
                    ai.set_last_rule_fired("idle:rejoin_use_portal");
                }
                else
                {
                    emit.move_to(best->x, best->y, best->z, /*run=*/true);
                    ai.set_last_rule_fired("idle:rejoin_walk_to_portal");
                }
                progress_log(ai.last_rule_fired());
                return true;
            }
            if (best && best_dsq <= 30.0f * 30.0f && !s.on_transport())
            {
                // Board server-side once it stands docked (the API walks the
                // bot onto the deck); the ride is RejoinRide's.
                TC_LOG_INFO("playerbot.v2",
                    "[rejoin] {} boards transport {} to map {} at ({:.0f},{:.0f},{:.0f}) map {}",
                    s.name(), best->entry, lg->to_map, bx, by, bz, s.map_id());
                emit.use_game_object(best->guid);
                ai.set_last_rule_fired("idle:rejoin_board_transport");
                return true;
            }
            // The transport is not in yet (or the portal not in sight from the
            // anchor): hold at the dock. The dock anchor of a ship is its
            // offshore stop, so the pier within 150y of it counts as there.
            const float fdx = lg->from_x - bx, fdy = lg->from_y - by;
            if (fdx * fdx + fdy * fdy <= 150.0f * 150.0f)
            {
                ai.set_last_rule_fired("idle:rejoin_wait_transport");
                progress_log("waiting for the transport");
                return true;
            }
        }
    }

    // Entrance on this map beyond walking range with no graph route (inside an
    // outer instance, which the travel graph does not cover): chunked walk.
    if (same_map && ::Playerbot::States::ChunkedWalkToward(s, ai, emit, t.x, t.y, t.z, "idle:rejoin_far_walk", now_ms))
    {
        progress_log("chunked walk");
        return true;
    }

    // No way on from here: say so once per target and wait where the bot
    // stands (never a port). The plan is retried every 60s above.
    if (rr.gap_map != anchor.map_id)
    {
        rr.gap_map = anchor.map_id;
        TC_LOG_INFO("playerbot.v2",
            "[rejoin] {} on map {} at ({:.0f},{:.0f},{:.0f}) finds no route on to entrance trigger {} at ({:.1f},{:.1f},{:.1f}) map {} into map {} "
            "(plan legs {}, at leg {}): waits here for a route or for {} to come out",
            s.name(), s.map_id(), bx, by, bz, t.at_id, t.x, t.y, t.z, t.src_map, anchor.map_id,
            ai.travel_plan().size(), ai.travel_plan_leg_index(), anchor.name);
    }
    ai.set_last_rule_fired("idle:rejoin_no_route");
    return false;
}

bool RejoinInstanceFire(BotSnapshotView const& s, BotAI& ai,
                        GroupSnapshotView const& g,
                        BotIntentEmitter& emit, uint32)
{
    auto const* mems = g.members();
    if (!mems) return false;
    const uint32 now_ms = GameTime::GetGameTimeMS();
    BotAI::RejoinRun& rr = ai.rejoin_run();

    GroupMemberSummary const* anchor = nullptr;
    GroupMemberSummary const* owner = nullptr;
    if (s.is_companion_with_owner())
        for (auto const& m : *mems)
            if (m.guid == s.raw().owner_guid && m.online) { owner = &m; break; }
    if (owner)
    {
        // Owner here: the group layer follows them. Owner elsewhere and not
        // in an instance: nothing to rejoin.
        if (owner->map_id == s.map_id() || !IsInstanceMap(owner->map_id))
        {
            ResetRejoin(ai);
            return false;
        }
        if (!owner->is_alive)
        {
            if (rr.log_ms == 0 || now_ms - rr.log_ms >= 60000u)
            {
                rr.log_ms = now_ms ? now_ms : 1u;
                TC_LOG_INFO("playerbot.v2",
                    "[death-recovery] {} alive on map {} waits for its owner {}, dead inside map {}, to release and come out",
                    s.name(), s.map_id(), owner->name, owner->map_id);
            }
            // Mid-journey aboard a ship / tram: still step off at the far
            // shore rather than riding it back and forth while waiting.
            if (rr.far_at_id != 0 && RejoinRide(s, ai, emit, now_ms))
                return true;
            ai.set_last_rule_fired("idle:rejoin_wait_owner");
            return false;
        }
        anchor = owner;
    }
    else
    {
        BotRegistry& reg_rejoin = Services::Registry();
        for (int pass = 0; pass < 2 && !anchor; ++pass)
            for (auto const& m : *mems)
            {
                if (!m.online || !m.is_alive) continue;
                if (m.guid == s.raw().guid) continue;
                if (pass == 0 && reg_rejoin.has(m.guid.GetCounter())) continue;   // humans first
                if (m.map_id == s.map_id() || !IsInstanceMap(m.map_id)) continue;
                anchor = &m;
                break;
            }
    }
    if (!anchor)
    {
        ResetRejoin(ai);
        return false;
    }

    float bx, by, bz;
    s.position(bx, by, bz);
    // Walking range with hysteresis: switching mode resets the rejoin (the far
    // mode's travel plan is dropped and replanned), so a bot near the 500y
    // line must not flip every tick. Far mode holds until within 400y, near
    // mode until beyond 600y; a fresh rejoin splits at 500y.
    constexpr float kWalkRange      = 500.0f;
    constexpr float kWalkRangeEnter = 400.0f;
    constexpr float kWalkRangeLeave = 600.0f;
    // The owner is inside and this is the owner's own companion: it goes in to the owner's spot at once. The door checks (a refusal
    // is logged with its reason, a stale lock on another instance is dropped) are API::teleport_to's. After a few refusals the
    // entrance trigger below takes over again (walk in, or be summoned to the entrance).
    if (owner == anchor && rr.direct_tries < BotAI::RejoinRun::kDirectTries && (rr.summon_ms == 0 || now_ms - rr.summon_ms >= 3000u))
    {
        rr.summon_ms = now_ms ? now_ms : 1u;
        ++rr.direct_tries;
        TC_LOG_INFO("playerbot.v2", "[rejoin] {} is ported into map {} to {} at ({:.0f},{:.0f},{:.0f}) (try {}/{})", s.name(),
            anchor->map_id, anchor->name, anchor->x, anchor->y, anchor->z, rr.direct_tries, BotAI::RejoinRun::kDirectTries);
        if (s.is_mounted())
            emit.dismount();
        emit.teleport_to(anchor->map_id, anchor->x, anchor->y, anchor->z, anchor->o);
        ai.set_last_rule_fired("idle:rejoin_enter_owner");
        return true;
    }
    if (owner == anchor && rr.direct_tries < BotAI::RejoinRun::kDirectTries)
    {
        ai.set_last_rule_fired("idle:rejoin_enter_owner");
        return true;
    }
    InstanceEntranceTrigger const* t = ResolveEntranceTrigger(s.map_id(), bx, by, anchor->map_id);
    if (!t)
    {
        // Data gap: no areatrigger_teleport row leads into this instance (or
        // its outer instance) from anywhere. Logged once; the bot waits.
        if (rr.gap_map != anchor->map_id)
        {
            ResetRejoin(ai);
            rr.gap_map = anchor->map_id;
            TC_LOG_INFO("playerbot.v2",
                "[rejoin] {} cannot rejoin {} in map {}: no entrance area trigger into that instance exists in "
                "areatrigger_teleport / AreaTrigger.db2 (data gap) - waits on map {} at ({:.0f},{:.0f},{:.0f})",
                s.name(), anchor->name, anchor->map_id, s.map_id(), bx, by, bz);
        }
        ai.set_last_rule_fired("idle:rejoin_no_entrance");
        return false;
    }
    const float pdx = t->x - bx, pdy = t->y - by, pdz = t->z - bz;
    const float pd = std::sqrt(pdx * pdx + pdy * pdy);
    const float walk_range = rr.at_id == t->at_id     ? kWalkRangeLeave
                           : rr.far_at_id == t->at_id ? kWalkRangeEnter
                                                      : kWalkRange;
    if (t->src_map == s.map_id() && pd <= walk_range)
    {
        if (rr.at_id != t->at_id)
        {
            ResetRejoin(ai);
            rr.at_id = t->at_id;
            TC_LOG_INFO("playerbot.v2",
                "[death-recovery] {} rejoins {} on map {}: walks to entrance trigger {} at ({:.1f},{:.1f},{:.1f}) map {}, {:.0f}y away",
                s.name(), anchor->name, anchor->map_id, t->at_id, t->x, t->y, t->z, s.map_id(), pd);
        }
        // No-progress watch: logged, never a port. A bot that cannot path to
        // the portal is a navigation bug to fix where it is, not to hide.
        if (rr.last_dist < 0.0f || pd < rr.last_dist - 5.0f || rr.since_ms == 0)
        {
            rr.last_dist = pd;
            rr.since_ms = now_ms ? now_ms : 1u;
        }
        else if (now_ms - rr.since_ms >= 60000u && (rr.log_ms == 0 || now_ms - rr.log_ms >= 60000u))
        {
            rr.log_ms = now_ms ? now_ms : 1u;
            TC_LOG_INFO("playerbot.v2",
                "[death-recovery] {} no progress toward entrance trigger {} for {}s: at ({:.1f},{:.1f},{:.1f}) map {}, {:.0f}y left",
                s.name(), t->at_id, (now_ms - rr.since_ms) / 1000u, bx, by, bz, s.map_id(), pd);
        }
        // In (or at the edge of) the trigger volume: cross it. The server
        // re-checks the volume, so a near miss is a harmless OutOfRange while
        // the walk to the centre continues. One attempt per 3s while it lands.
        // The trigger centre can hang over a lava lake (the Molten Core
        // entrances over the Blackrock Mountain lava) while its volume reaches
        // the dry ledge, so the bot stops where the navmesh ends: try from 12y
        // out, vertically 25y, and let the server's volume check decide.
        if (pd <= 12.0f && std::fabs(pdz) <= 25.0f)
        {
            if (rr.enter_ms == 0 || now_ms - rr.enter_ms >= 3000u)
            {
                emit.enter_area_trigger(t->at_id);
                rr.enter_ms = now_ms ? now_ms : 1u;
            }
            if (pd > 1.0f)
                emit.move_to(t->x, t->y, t->z, /*run=*/true);
            ai.set_last_rule_fired("idle:rejoin_instance_enter");
            return true;
        }
        emit.move_to(t->x, t->y, t->z, /*run=*/true);
        ai.set_last_rule_fired("idle:rejoin_instance_walk");
        return true;
    }

    // A companion whose owner is inside is summoned to the entrance, as a
    // raid summons its members to the meeting stone: the owner went in (often
    // by teleport) and waits there, and a cross-continent walk would leave the
    // raid short for many minutes (40 companions left in Stormwind while their
    // owner stood in Molten Core, 2026-10-06). Bounded: only to the entrance
    // trigger of the owner's instance, never to the owner or into the
    // instance, and the bot still zones in through the portal on foot, under
    // the server's own raid / lock / full checks. Bots without an owner there
    // travel for real.
    if (owner == anchor)
    {
        if (rr.summon_ms == 0 || now_ms - rr.summon_ms >= 10000u)
        {
            rr.summon_ms = now_ms ? now_ms : 1u;
            TC_LOG_INFO("playerbot.v2",
                "[rejoin] {} on map {} at ({:.0f},{:.0f},{:.0f}) summoned to entrance trigger {} at ({:.1f},{:.1f},{:.1f}) map {} "
                "to rejoin its owner {} in map {}{}",
                s.name(), s.map_id(), bx, by, bz, t->at_id, t->x, t->y, t->z, t->src_map, anchor->name, anchor->map_id,
                t->src_map == s.map_id() ? fmt::format(" ({:.0f}y away)", pd) : std::string(" (from another map)"));
            if (s.is_mounted())
                emit.dismount();
            if (t->src_map == s.map_id())
                emit.near_teleport_to(t->x, t->y, t->z, s.raw().position.o);
            else
                emit.teleport_to(t->src_map, t->x, t->y, t->z, s.raw().position.o);
        }
        ai.set_last_rule_fired("idle:rejoin_summon");
        return true;
    }

    // Entrance on another map, or on this one beyond walking range: travel.
    return RejoinTravelFar(s, ai, emit, *t, *anchor, now_ms);
}

// ---------- idle:dungeon_loop_requeue ----------
bool DungeonLoopRequeueGate(BotSnapshotView const& s, BotAI& ai,
                            GroupSnapshotView const& g, uint32)
{
    if (!ai.dungeon_loop_mode()) return false;
    if (ai.last_lfg_dungeon_id() == 0) return false;
    if (ai.dungeon_run_mode() != BotAI::DungeonRunMode::Off) return false;
    if (s.is_in_instance() || s.in_combat() || s.is_casting()) return false;
    if (!s.is_alive()) return false;
    // TC's WorldSession::HandleLfgJoinOpcode rejects LFG-join from non-
    // leaders — group queues as a whole, only leader drives it. Skip the
    // emit when grouped and non-leader so we don't spam an intent the
    // server will reject (5 bots × every 10s = 30 rejected joins/min per
    // 5-bot dungeon party).
    if (g.exists() && g.leader() != s.raw().guid) return false;
    const uint32 now_ms = s.published_at_ms();
    const uint32 last = ai.last_loop_requeue_ms();
    return last == 0 || (now_ms - last) >= 10000u;
}

bool DungeonLoopRequeueFire(BotSnapshotView const& s, BotAI& ai,
                            GroupSnapshotView const&,
                            BotIntentEmitter& emit, uint32)
{
    Role role = ai.effective_role(s);
    if (role == Role::Unknown) role = s.my_role();
    if (role == Role::Unknown) role = Role::Dps;
    emit.lfg_queue(ai.last_lfg_dungeon_id(), role);
    ai.note_loop_requeue(s.published_at_ms());
    ai.set_last_rule_fired("idle:dungeon_loop_requeue");
    return true;
}

// ---------- idle:invite_nearby_player ----------
// Outgoing invite: solo bot >= L25 in a questing area (no rest bonus =
// not in a city/inn) invites a same-faction same-bracket nearby player
// to a group. Per-target 5-min cooldown via InviteOther absorbs declines.
bool InviteNearbyPlayerGate(BotSnapshotView const& s, BotAI&,
                            GroupSnapshotView const&, uint32)
{
    if (s.in_group() || s.has_group_invite()) return false;
    // A scenario squad member only ever groups through the run (the runner's raid group), never with a stranger.
    if (Pool::IsScenarioBot(s.bot_id())) return false;
    // An owned bot stays with its owner (idle:owner_escort) and never
    // invites strangers - nor the owner: the owner invites it. Checked on
    // the binding itself: owner_guid is empty while an account-bound
    // owner is offline.
    if (!s.raw().owner_guid.IsEmpty()) return false;
    if (Services::Owners().GetOwner(s.raw().bot_id).account_id != 0) return false;
    if (s.level() < 25) return false;
    if (!s.is_alive() || s.in_combat() || s.is_casting()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.raw().identity.rest_bonus_xp != 0) return false;
    return true;
}

bool InviteNearbyPlayerFire(BotSnapshotView const& s, BotAI& ai,
                            GroupSnapshotView const&,
                            BotIntentEmitter& emit, uint32)
{
    const uint32 inv_now_ms = GameTime::GetGameTimeMS();
    for (auto const& u : s.raw().combat.nearby_friends)
    {
        if (u.guid.IsCreature()) continue;
        if (u.entry != 0) continue;
        if (u.guid == s.raw().guid) continue;
        if (u.is_in_group) continue;
        if (Pool::IsScenarioBot(u.guid.GetCounter())) continue;   // a test squad member is not a pickup-party recruit
        if (!u.victim.IsEmpty()) continue;
        const int dlvl = int(u.level) - int(s.level());
        if (dlvl > 5 || dlvl < -5) continue;
        float bx, by, bz; s.position(bx, by, bz);
        const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
        constexpr float kInviteRangeSq = 30.0f * 30.0f;
        if (dx*dx + dy*dy + dz*dz > kInviteRangeSq) continue;
        const uint64 inv_key = u.guid.GetCounter();
        if (ai.action_recently_tried(BotAI::ActionKind::InviteOther,
                                     inv_key, inv_now_ms))
            continue;
        emit.invite_to_group(u.guid);
        ai.note_action_retry(BotAI::ActionKind::InviteOther,
                             inv_key, inv_now_ms);
        ai.set_last_rule_fired("idle:invite_nearby_player");
        return true;
    }
    return false;
}

} // anonymous namespace

void RegisterCoreFlowRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:swim_stuck";
        rule.priority = 870;
        rule.gate     = &SwimStuckGate;
        rule.fire     = &SwimStuckFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:water_escape";
        rule.priority = 865;   // below swim_stuck (870), above all travel/wander
        rule.gate     = &WaterEscapeGate;
        rule.fire     = &WaterEscapeFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:group_convert_to_raid";
        rule.priority = 860;
        rule.gate     = &GroupConvertGate;
        rule.fire     = &GroupConvertFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:group_invite_accept";
        rule.priority = 855;
        rule.gate     = &GroupInviteAcceptGate;
        rule.fire     = &GroupInviteAcceptFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:guild_invite_accept";
        rule.priority = 853;
        rule.gate     = &GuildInviteAcceptGate;
        rule.fire     = &GuildInviteAcceptFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:lfg_proposal_accept";
        rule.priority = 850;
        rule.gate     = &LfgProposalAcceptGate;
        rule.fire     = &LfgProposalAcceptFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:bg_port_accept";
        rule.priority = 847;
        rule.gate     = &BgPortAcceptGate;
        rule.fire     = &BgPortAcceptFire;
        r.register_rule(std::move(rule));
    }
    {
        // 846: just below bg_port_accept (so an invited JIT bot ports in first),
        // above the entire quest/wander/grind/vendor band — this is the JIT
        // confinement (owner directive: JIT bots only do their purpose).
        IdleRule rule;
        rule.name     = "idle:bg_jit_staging";
        rule.priority = 846;
        rule.gate     = &BgJitStagingGate;
        rule.fire     = &BgJitStagingFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:rejoin_group_instance";
        rule.priority = 840;
        rule.gate     = &RejoinInstanceGate;
        rule.fire     = &RejoinInstanceFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:dungeon_loop_requeue";
        rule.priority = 836;
        rule.gate     = &DungeonLoopRequeueGate;
        rule.fire     = &DungeonLoopRequeueFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:invite_nearby_player";
        rule.priority = 830;
        rule.gate     = &InviteNearbyPlayerGate;
        rule.fire     = &InviteNearbyPlayerFire;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
