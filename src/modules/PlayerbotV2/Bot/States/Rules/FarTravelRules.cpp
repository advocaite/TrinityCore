// FarTravelRules - far same-map travel. Adds idle:far_same_map_travel @697,
// the dedicated rule for driving a bot toward a SAME-MAP objective POI that is
// too far for the pursue_quest_goal routable band (>2500y). pursue_quest_goal
// (@698) caps at 2500y and DEFERS larger goals "to the flight/taxi cascade",
// but that cascade lived inline in AutoactDispatch at the starved autoact@50
// level — so a bot with a 2500y+ same-map goal never proactively walked to a
// flight master nor chunk-walked toward the goal, and wedged (the long-distance
// pathfind area noted in the fleet-behavior memos: Durnan far-turn-in, etc.).
//
// This rule sits at 697 (just below pursue_quest_goal@698, above the OOC
// maintenance band 660-696) so questing toward a near goal still wins, but a
// far same-map goal now drives travel from the top-of-tick registry pass. It
// reuses the SAME extracted travel executors as AutoactDispatch
// (::Playerbot::States::DriveRecommendedTaxi / DriveTravelPlanTo) so it composes
// the identical proactive-flight / chunk-walk behavior — no duplicated logic.
//
// Same-map transports (2026-10-05). Vanilla has rides whose two docks are on
// ONE map — the Moonspray (Auberdine <-> Rut'theran), the Feathermoon ferry,
// the Undercity <-> Grom'gol zeppelin — and the Deeprun Tram (both stations on
// map 369). The travel graph routes them as same-map Ship legs (PortalIndex /
// UnifiedTravelGraph). This file:
//   * idle:transport_ride @1001 — owns every ship/zeppelin ride (and a tram
//     ride on a planned leg): holds the bot still aboard, and steps it off at
//     the leg's arrival dock once the transport is paused there.
//   * idle:far_same_map_travel also takes same-map goals that need a bridge
//     (other landmass, or the builder's stuck-bridge verdict) at ANY distance,
//     and a >2500y goal whose graph route rides a same-map transport.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/ClassicUpkeep.h"
#include "Bot/States/MaintainHelpers.h"
#include "Services.h"
#include "Travel/PortalIndex.h"
#include "Travel/RegionMapper.h"
#include "Travel/UnifiedTravelGraph.h"
#include "Log.h"
#include <cmath>
#include <limits>

namespace Playerbot {

namespace {

using namespace ::Playerbot::States;

// ---------- idle:far_same_map_travel ----------
// Threshold mirrors pursue_quest_goal's upper band edge (2500y). Below this the
// pursue rule owns the goal with full-path routing; at/above it the goal is
// continent-scale and this rule drives the flight/taxi + chunk-walk approach.
constexpr float kFarTravelThreshold = 2500.0f;

// A same-map goal the graph must bridge: on another landmass of this map (no
// walk reaches it), or flagged by the snapshot builder after the bot kept
// failing to walk there (objective_needs_bridge: the graph found a route with a
// non-walk leg). idle:pursue_quest_goal@698 yields these to this rule.
bool SameMapGoalNeedsBridge(BotSnapshotView const& s)
{
    auto const& poi = s.current_objective_poi();
    if (!poi.valid || poi.map_id != s.map_id()) return false;
    if (s.objective_needs_bridge()) return true;
    float bx, by, bz; s.position(bx, by, bz);
    return ::Playerbot::V2::Travel::RegionForPosition(s.map_id(), bx, by) !=
           ::Playerbot::V2::Travel::RegionForPosition(poi.map_id, poi.x, poi.y);
}

// Does the bot's planted travel plan still have a same-map transport ride ahead?
bool PlanRidesSameMapTransport(BotSnapshotView const& s, BotAI& ai)
{
    auto const& plan = ai.travel_plan();
    for (size_t i = ai.travel_plan_leg_index(); i < plan.size(); ++i)
        if (plan[i].kind == 4 /*Ship*/ && plan[i].to_map == s.map_id() &&
            Services::Portals().IsSameMapTransport(plan[i].payload))
            return true;
    return false;
}

bool FarSameMapTravelGate(BotSnapshotView const& s, BotAI& ai,
                          GroupSnapshotView const&, uint32 /*now_ms*/)
{
    // Mirror the pursue_quest_goal / walk_to_known_hub exclusions: no scripted
    // content, no profession detours, heal before a long trek.
    if (s.in_combat() || s.is_casting()) return false;
    if (s.is_in_dungeon() || s.in_battleground()) return false;
    if (ai.in_profession_mode(s.published_at_ms())) return false;
    // HP<50%: heal first (same gate as pursue_quest_goal — a hurt bot should not
    // set off across open terrain to a far goal unhealed).
    if (Classic::RecoverBeforeWalking(s))
        return false;
    // Yield to a nearby acceptable offer (giver in scan) or any pending turn-in —
    // accept/turn-in own those; trekking to a far goal must not preempt a quest
    // hand-in the bot could complete right here (mirrors pursue_quest_goal).
    // EXCEPTION (Durnan wedge, 2026-06-21): if walk_to_known_hub@700 has declared
    // its offer close-approach FUTILE (hub_offer_backoff active — the bot is
    // wedged and physically cannot reach the accept margin), those in-scan offers
    // are confirmed un-grabable for now. Yielding to them here too would deadlock
    // far-travel on the SAME phantom offers that wedged the hub rule, so the bot
    // never travels (Durnan L15: 4 in-scan offers it can't accept, every travel
    // rule yielding to them, position+XP frozen). During the back-off, ignore the
    // offers and proceed with travel; the bot retries the offers after the cooldown.
    if (s.published_at_ms() >= ai.hub_offer_backoff_until())
        for (auto const& off : s.raw().quest_discovery.quest_offers)
        {
            bool giver_inscan = false;
            for (auto const& u : s.raw().combat.nearby_friends)
                if (u.guid == off.giver) { giver_inscan = true; break; }
            if (!giver_inscan)
                for (auto const& o : s.raw().world_objects.nearby_objects)
                    if (o.guid == off.giver) { giver_inscan = true; break; }
            if (giver_inscan) return false;
        }
    if (!s.raw().quest_discovery.quest_turnins.empty()) return false;
    // Same-map POI only — cross-map goals stay with the AutoactDispatch
    // portal/ship relocation cascade (driveTravelPlanTo handles to_map != map_id).
    auto const& poi = s.current_objective_poi();
    if (!poi.valid || poi.map_id != s.map_id()) return false;
    // A goal across water / behind a gap is this rule's at any distance: the
    // graph route (boat, ferry, flight, teleport) is the only way there.
    if (SameMapGoalNeedsBridge(s)) return true;
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = poi.x - bx, dy = poi.y - by;
    const float d2 = dx * dx + dy * dy;
    return d2 > (kFarTravelThreshold * kFarTravelThreshold);
}

bool FarSameMapTravelFire(BotSnapshotView const& s, BotAI& ai,
                          GroupSnapshotView const&,
                          BotIntentEmitter& emit, uint32 now_ms)
{
    float bx, by, bz; s.position(bx, by, bz);

    // (A) Proactive flight: if the builder published a recommended taxi route for
    // a far same-map goal, fly it (walk to the FM, dismount, fly_to_node). Reuses
    // the exact extracted cascade so behavior matches AutoactDispatch. When it
    // falls through (no taxi route / no proactive start), drop to the chunk walk.
    if (s.has_recommended_taxi_route())
    {
        bool ft = false;
        if (::Playerbot::States::DriveRecommendedTaxi(s, ai, emit, bx, by, bz, &ft))
            return true;
        if (!ft)
            return false;   // wedged/yield — consume tick (the taxi cascade decided)
        // ft == true: no proactive start position — fall through to the walk.
    }

    auto const& poi = s.current_objective_poi();

    // (B) Goal on ANOTHER LANDMASS of this map (Teldrassil vs mainland Kalimdor;
    // the 530 islands), or one the builder found needs a non-walk leg: there is
    // no walk to it, and a chunk walk heads into the sea / the cliff. Plan
    // through the travel graph instead (the Moonspray, a flight, a teleport
    // that cross the water, e.g. Rut'theran <-> Auberdine). No route = yield,
    // never swim.
    if (SameMapGoalNeedsBridge(s))
        return ::Playerbot::States::DriveTravelPlanTo(s, ai, emit, bx, by, bz,
                                                      poi.map_id, poi.x, poi.y, poi.z,
                                                      /*quest_goal=*/true);

    // (B2) A continent-scale goal the graph reaches by a same-map ride (the
    // Undercity <-> Grom'gol zeppelin for a Tirisfal -> Stranglethorn trip):
    // take the ride rather than walking the continent. The plan is computed once
    // per goal (EnsureTravelPlan caches it by goal key); a plan without such a
    // ride leaves the trip to the chunk walk below, unchanged.
    ::Playerbot::States::EnsureTravelPlan(s, ai, bx, by, bz, poi.map_id, poi.x, poi.y, poi.z,
                                          /*quest_goal=*/true);
    if (PlanRidesSameMapTransport(s, ai))
        return ::Playerbot::States::DriveTravelPlanTo(s, ai, emit, bx, by, bz,
                                                      poi.map_id, poi.x, poi.y, poi.z,
                                                      /*quest_goal=*/true);

    // (C) No taxi route (or it yielded a fall-through): chunk-walk toward the POI.
    // Shared ChunkedWalkToward steps in bounded chunks (a single far move_to
    // NoPaths on unloaded tiles), deflects on blocks, ESCALATES the deflection on
    // a position stall to escape ledges/corners, and yields when genuinely stuck.
    return ::Playerbot::States::ChunkedWalkToward(s, ai, emit, poi.x, poi.y, bz,
                                                  "idle:far_same_map_travel", now_ms);
}

// ---------- idle:transport_ride ----------
// Owns the bot while it rides a ship / zeppelin (any type-15 transport) or a
// tram car on a planned same-map Ship leg. Aboard, the bot must not run its
// walk rules: a passenger's move_to is relative to the moving deck and walks it
// off into the sea (and between docks a type-11 tram car keeps a steady Z, so
// idle:on_transport_wait's elevator freeze does not hold it). The rule holds
// the bot still and steps it off (use_game_object on the ridden transport ->
// RemovePassenger) when it has arrived:
//   * same-map ride leg (the plan's current leg rides this route): ship /
//     zeppelin paused at a stop within kShipArrivalRadius of the leg's arrival
//     dock; tram car standing at the arrival station (the rider is carried at
//     the car's pivot X/Y, which only reaches the station's dwell point there).
//   * cross-map ride: paused at the planned leg's arrival dock (on its map);
//     without a plan, paused at a stop on the objective's map; with neither,
//     paused at the next stop.
// 1001: above idle:elevator_step_off (1000) and idle:on_transport_wait (999),
// which keep owning type-11 lift rides (the gate leaves those alone).
bool TransportRideGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (!s.on_transport() || s.in_combat() || s.is_casting()) return false;
    // A group-rejoin journey (idle:rejoin_group_instance) owns its own ride:
    // RejoinRide keys the cross-map arrival on the plan leg rather than a
    // quest POI (which a rejoining bot has none of, so this rule would ride
    // it back and forth forever) and replans when it loses track of the ride.
    if (ai.rejoin_run().far_at_id != 0) return false;
    if (s.transport_is_ship()) return true;
    BotAI::PlanLeg const* lg = ai.current_travel_leg();
    return lg && lg->kind == 4 /*Ship*/ && lg->to_map == s.map_id() &&
           Services::Portals().IsSameMapTransport(lg->payload);
}

bool TransportRideFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,
                       BotIntentEmitter& emit, uint32)
{
    using namespace ::Playerbot::V2::Travel;
    float bx, by, bz; s.position(bx, by, bz);
    const bool ship = s.transport_is_ship();
    BotAI::PlanLeg const* lg = ai.current_travel_leg();
    const bool same_map_leg = lg && lg->kind == 4 /*Ship*/ && lg->to_map == s.map_id() &&
                              Services::Portals().IsSameMapTransport(lg->payload);

    // The ridden transport carries the bot, so it is the nearest transport GO of
    // the ridden kind (on a planned ride: of the leg's route).
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

    bool arrived = false;
    if (same_map_leg)
    {
        const float dx = lg->to_x - bx, dy = lg->to_y - by;
        const float d2 = dx * dx + dy * dy;
        arrived = ship ? (s.transport_stopped() && d2 <= kShipArrivalRadius * kShipArrivalRadius)
                       : (d2 <= kShuttleArrivalRadius * kShuttleArrivalRadius);
    }
    else if (ship && s.transport_stopped())
    {
        // Cross-map ride. A planned Ship leg names its arrival dock: step off
        // there once the map change has happened (a bot travelling for town
        // life or a group, with no quest objective, keys only on its plan).
        // A quest rider without a plan steps off on the objective's map. A
        // rider with neither has no destination to hold out for, so it steps
        // off at the next stop rather than riding the route forever.
        if (lg && lg->kind == 4 /*Ship*/)
        {
            const float dx = lg->to_x - bx, dy = lg->to_y - by;
            arrived = lg->to_map == s.map_id() &&
                      dx * dx + dy * dy <= kShipArrivalRadius * kShipArrivalRadius;
        }
        else if (s.has_current_objective() && s.current_objective_poi().valid)
            arrived = s.current_objective_poi().map_id == s.map_id();
        else
            arrived = true;
    }

    if (arrived && !ridden.IsEmpty())
    {
        TC_LOG_INFO("playerbot.v2",
            "[xport_board] {} arrived map={} pos=({:.0f},{:.0f},{:.0f}) same_map_leg={} -> disembarking",
            s.name(), s.map_id(), bx, by, bz, same_map_leg ? 1 : 0);
        emit.use_game_object(ridden);   // toggle -> RemovePassenger
        ai.set_last_rule_fired("idle:transport_ride_disembark");
        return true;
    }
    ai.set_last_rule_fired("idle:transport_ride");
    return true;
}

} // anonymous namespace

void RegisterFarTravelRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:transport_ride";
        rule.priority = 1001;
        rule.gate     = &TransportRideGate;
        rule.fire     = &TransportRideFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:far_same_map_travel";
        // 697: just BELOW idle:pursue_quest_goal (698, the routable 80-2500y
        // band) and ABOVE the OOC maintenance band (660-696). A near goal stays
        // with pursue; a far same-map goal (>2500y) drives flight/taxi + chunk
        // walk from the top-of-tick pass instead of the starved autoact@50 path.
        rule.priority = 697;
        rule.gate     = &FarSameMapTravelGate;
        rule.fire     = &FarSameMapTravelFire;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
