/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tc_catch2.h"

#include "../../src/modules/PlayerbotV2/Bot/Battleground/BgWaypointRoutes.h"
#include <cmath>
#include <set>
#include <string>

using namespace Playerbot::BgRoutes;

namespace
{
    // WSG flag stands (WarsongGulchScript.cpp): Alliance 1540.423,1481.325,351.818; Horde 916.023,1433.805,346.037.
    constexpr float kAlly[3]  = { 1540.423f, 1481.325f, 351.818f };
    constexpr float kHorde[3] = {  916.023f, 1433.805f, 346.037f };

    struct Walk { bool arrived = false; int steps = 0; float max_step = 0.0f; int drops = 0; bool bad_drop = false; };

    // Walks a bot from `from` to `goal` the way BgRouteMove does (next waypoint, 28 y lookahead; the plain walk to the goal when there
    // is none) and counts how many of a lane's own waypoints it reached (within 12 y of the waypoint it was sent to).
    Walk Run(float const* from, float const* goal, Lane const* lane, int variant, std::set<size_t>* reached)
    {
        Walk w;
        float x = from[0], y = from[1], z = from[2];
        for (int i = 0; i < 400; ++i)
        {
            w.steps = i;
            float const dx = goal[0] - x, dy = goal[1] - y;
            if (dx * dx + dy * dy < 12.0f * 12.0f) { w.arrived = true; break; }
            float nx = 0, ny = 0, nz = 0;
            bool drop = false;
            if (!NextWaypointLane(2, x, y, z, goal[0], goal[1], goal[2], 28.0f, nx, ny, nz, variant, lane, &drop))
            {
                nx = goal[0]; ny = goal[1]; nz = goal[2];   // the plain walk to the goal, as in BgRouteMove
            }
            if (drop)
            {
                // the committed drop move: a landing 4..40 y below within 40 y, crossed once
                float const fall = z - nz, dxy = std::sqrt((nx - x) * (nx - x) + (ny - y) * (ny - y));
                ++w.drops;
                if (fall < 4.0f || fall > 40.0f || dxy > 40.0f) w.bad_drop = true;
                // and the same call from the middle of the flight keeps the same landing (no second walk target)
                float mx = 0, my = 0, mz = 0; bool mdrop = false;
                float const hx = (x + nx) * 0.5f, hy = (y + ny) * 0.5f, hz = nz + (z - nz) * 0.4f;
                if (!NextWaypointLane(2, hx, hy, hz, goal[0], goal[1], goal[2], 28.0f, mx, my, mz, variant, lane, &mdrop) || !mdrop ||
                    std::fabs(mx - nx) > 0.01f || std::fabs(my - ny) > 0.01f)
                    w.bad_drop = true;
            }
            w.max_step = std::max(w.max_step, std::sqrt((nx - x) * (nx - x) + (ny - y) * (ny - y)));
            x = nx; y = ny; z = nz;
            if (lane && lane->waypoints)
                for (size_t k = 0; k < lane->waypoints->size(); ++k)
                {
                    LanePoint const& p = (*lane->waypoints)[k];
                    if ((p.x - x) * (p.x - x) + (p.y - y) * (p.y - y) < 9.0f && reached) reached->insert(k);
                }
            if (lane && lane->path)
            {
                Graph const* g = GraphFor(2);
                int const f = g->PathEnd(lane->path, false), l = g->PathEnd(lane->path, true);
                for (int n : { f, f + (l - f) / 2, l })
                {
                    RoutePoint const& p = g->Point(n);
                    if ((p.x - x) * (p.x - x) + (p.y - y) * (p.y - y) < 9.0f && reached) reached->insert(size_t(n));
                }
            }
        }
        return w;
    }
}

TEST_CASE("Warsong Gulch lanes: a team spreads over at least three lanes, each walked end to end", "[battleground][route]")
{
    for (bool east : { true, false })
    {
        float const* from = east ? kHorde : kAlly;
        float const* goal = east ? kAlly : kHorde;
        std::set<std::string> used;
        for (uint32_t guid = 1; guid <= 60; ++guid)
        {
            Lane const* lane = PickLane(LanesFor(2, east, false), (guid * 2246822519u >> 9));
            std::set<size_t> reached;
            Walk w = Run(from, goal, lane, int(guid % Graph::kVariants), &reached);
            CHECK(w.arrived);
            CHECK(!w.bad_drop);
            bool const ridge = lane && std::string(lane->name).rfind("ridge", 0) == 0;
            CHECK(w.drops == (ridge ? 1 : 0));            // a ridge lane crosses its one recorded drop; no other lane drops
            if (lane && lane->waypoints)
                CHECK(reached.size() >= lane->waypoints->size() - 1);   // every waypoint of the chain (the last one can fall inside the goal radius)
            if (lane && lane->path)
                CHECK(reached.size() >= 2);                              // at least two of first / middle / last node of the recorded path
            used.insert(lane ? lane->name : "plain");
        }
        CHECK(used.size() >= 5);
        CHECK(used.count("ridge>tunnel") + used.count("ridge>ramp") >= 1);   // the ridge lanes are in play again, over the drop crossing
    }
}

TEST_CASE("Warsong Gulch lanes: every chain stays on recorded ground", "[battleground][route]")
{
    // every waypoint of every lane chain lies within 25 y of a recorded graph node, except the two south mid-field points that exist
    // only in the lane data (see BgWsgLanes.inc)
    Graph const* g = GraphFor(2);
    for (bool east : { true, false })
        for (Lane const& l : LanesFor(2, east, false))
            if (l.waypoints)
                for (LanePoint const& p : *l.waypoints)
                {
                    float d = 0.0f;
                    g->Nearest(p.x, p.y, p.z, d);
                    bool const south_mid = std::fabs(p.x - 1269.96f) < 1.0f;
                    CHECK(d <= (south_mid ? 60.0f : 25.0f));
                }
}

TEST_CASE("Warsong Gulch lanes: a flag carrier never takes a base tunnel or the plain line", "[battleground][route]")
{
    for (bool east : { true, false })
    {
        CHECK(!LanesFor(2, east, true).empty());
        for (Lane const& l : LanesFor(2, east, true))
            CHECK(l.flank);
    }
}

TEST_CASE("Warsong Gulch lanes: other battlegrounds have no forced lanes", "[battleground][route]")
{
    CHECK(LanesFor(3, true, false).empty());
    CHECK(LanesFor(1, false, false).empty());
    CHECK(PickLane(LanesFor(3, true, false), 12345u) == nullptr);
}

// The recorded AV / AB graphs: every objective is reachable from both teams' starts with the one-way start-room, cave and graveyard
// exits (vPaths_NoReverseAllowed) kept one-way.
TEST_CASE("Recorded routes: AV and AB objectives are reachable from the starts", "[battleground][route]")
{
    auto reach = [](uint16_t bg, char const* from_path, bool from_last, char const* to_path, bool to_last)
    {
        Graph const* g = GraphFor(bg);
        int const from = g->PathEnd(from_path, from_last), to = g->PathEnd(to_path, to_last);
        if (from < 0 || to < 0) return false;
        int cur = from;
        for (int i = 0; i < 2000 && cur != to; ++i)
        {
            cur = g->NextToward(cur, to, 0);
            if (cur < 0) return false;
        }
        return cur == to;
    };
    // AV: caves and graveyards to the far base and the contested flags
    CHECK(reach(1, "Horde_Cave_to_Tower_Point_Crossroad", false, "Alliance_Base_Bunker_Third_Crossroad_to_Alliance_Base_Flag", true));
    CHECK(reach(1, "Alliance_Cave_to_Alliance_Cave_Slop_Crossroad", false, "Horde_Base_Second_Crossroads_to_Horde_Base_Graveyard_Flag", true));
    CHECK(reach(1, "Alliance_Cave_to_Alliance_Cave_Slop_Crossroad", false, "Stonehearth_Graveyard_Flag_to_Stonehearth_Graveyard_Second_Crossroad", false));
    CHECK(reach(1, "Stormpike_Graveyard_to_Stormpike_Flag", false, "Stormpike_Graveyard_to_Stormpike_Flag", true));
    // AB: from either base to every node
    for (char const* base : { "AllianceBase_to_Stables", "HordeBase_to_Farm" })
    {
        CHECK(reach(3, base, false, "AllianceBase_to_Stables", true));
        CHECK(reach(3, base, false, "AllianceBase_to_GoldMine", true));
        CHECK(reach(3, base, false, "AllianceBase_to_LumberMill", true));
    }
}

TEST_CASE("Warsong Gulch lanes: the last stretch to a ledge is walked straight, then the drop", "[battleground][route]")
{
    // The recorded ledge sits on the cliff edge, a few yards off the navmesh: a pathed move stops short of the 3.5 y ledge reach
    // (WSG 2026-10-09: 227 ridge routes, 3 drops). Within 10 y of the ledge the lane asks for a straight walk to it.
    Lane const* ridge = nullptr;
    for (Lane const& l : LanesFor(2, true, false))
        if (std::string(l.name) == "ridge>tunnel") ridge = &l;
    REQUIRE(ridge != nullptr);
    float nx = 0, ny = 0, nz = 0;
    bool drop = true, approach = true;

    // on the ridge, far from the ledge: a pathed move
    CHECK(NextWaypointLane(2, 1005.0f, 1415.0f, 345.0f, kAlly[0], kAlly[1], kAlly[2], 28.0f, nx, ny, nz, 0, ridge, &drop, &approach));
    CHECK(!drop);
    CHECK(!approach);

    // 6 y short of the ledge (1055.18,1396.97,339.36): straight walk to the ledge, no drop yet
    CHECK(NextWaypointLane(2, 1049.5f, 1398.5f, 339.5f, kAlly[0], kAlly[1], kAlly[2], 28.0f, nx, ny, nz, 0, ridge, &drop, &approach));
    CHECK(!drop);
    CHECK(approach);
    CHECK(std::fabs(nx - 1055.18f) < 0.1f);

    // at the ledge: the drop to the landing, no straight-walk request
    CHECK(NextWaypointLane(2, 1054.5f, 1397.0f, 339.4f, kAlly[0], kAlly[1], kAlly[2], 28.0f, nx, ny, nz, 0, ridge, &drop, &approach));
    CHECK(drop);
    CHECK(!approach);
    CHECK(std::fabs(nz - 324.0f) < 0.1f);

    // landed short of the landing (1076.8,1396.0,324.0), down at its level (WSG soak on 247: (1066.3,1396.5,325.2)): the drop is over,
    // no second drop to a landing 1 y below
    CHECK(NextWaypointLane(2, 1066.3f, 1396.5f, 325.2f, kAlly[0], kAlly[1], kAlly[2], 28.0f, nx, ny, nz, 0, ridge, &drop, &approach) == true);
    CHECK(!drop);
}

TEST_CASE("Warsong Gulch lanes: the middle of the field is a minority, the side lanes carry most bots", "[battleground][route]")
{
    // Playtest, WSG 2026-10-09: "still running up and down the middle". Tunnel-out lanes and the two plain middle lines are the middle.
    for (bool east : { true, false })
        for (bool carrier : { false, true })
        {
            int middle = 0, ridge = 0, total = 0;
            for (uint32_t guid = 1; guid <= 2000; ++guid)
            {
                Lane const* lane = PickLane(LanesFor(2, east, carrier), (guid * 2246822519u >> 9));
                std::string const n = lane ? lane->name : "plain";
                ++total;
                if (n == "tunnel>tunnel" || n == "tunnel>ramp" || n == "plain" || n == "mid2") ++middle;
                if (n.rfind("ridge", 0) == 0) ++ridge;
            }
            CHECK(middle * 100 <= total * 30);     // at most 30 % through the middle (carriers never)
            CHECK(ridge * 100 >= total * 40);      // the ridge drop lanes are the most used (a carrier only flanks: 40 % or more)
            if (carrier) CHECK(middle == 0);
        }
}

TEST_CASE("Warsong Gulch lanes: a bot on the graveyard ridge sees the drop below it", "[battleground][route]")
{
    auto const& lanes = LanesFor(2, true, false);
    LanePoint landing{};
    // 20 y before the ledge, on the ridge: the ridge lane and its landing
    Lane const* r = RidgeDropAbove(lanes, 0, 1040.0f, 1405.0f, 342.0f, 70.0f, &landing);
    REQUIRE(r != nullptr);
    CHECK(std::string(r->name).rfind("ridge", 0) == 0);
    CHECK(std::fabs(landing.z - 324.0f) < 0.1f);
    // already below (landed): nothing to drop from
    CHECK(RidgeDropAbove(lanes, 0, 1080.0f, 1396.0f, 324.5f, 70.0f) == nullptr);
    // far from the ridge
    CHECK(RidgeDropAbove(lanes, 0, 1300.0f, 1400.0f, 342.0f, 70.0f) == nullptr);
}

TEST_CASE("AV: a bot at the Horde start, off the recorded paths, is fed to the nearest recorded node", "[battleground][av]")
{
    using namespace Playerbot::BgRoutes;
    float nx = 0.f, ny = 0.f, nz = 0.f;
    // Horde start (-1438,-610,51) to Snowfall: 295 y from the Frostwolf base paths, the first recorded node.
    CHECK(NextWaypoint(1, -1438.f, -610.f, 51.f, -202.f, -113.f, 78.f, 28.0f, nx, ny, nz));
    CHECK(nx < -1300.f);
    CHECK(ny > -400.f);
    // Beyond the feeder reach, or in another battleground, nothing changes: the plain route.
    CHECK(!NextWaypoint(1, -1438.f, -1300.f, 51.f, -202.f, -113.f, 78.f, 28.0f, nx, ny, nz));
    CHECK(!NextWaypoint(2, 0.f, 0.f, 0.f, 1500.f, 1400.f, 340.f, 28.0f, nx, ny, nz));
}

TEST_CASE("AV: every recorded node has a route to Snowfall (the Horde half is not cut off by a doubly recorded junction)", "[battleground][av]")
{
    using namespace Playerbot::BgRoutes;
    Graph const* g = GraphFor(1);
    REQUIRE(g != nullptr);
    float d = 0.f;
    int const snow = g->Nearest(-202.f, -113.f, 78.f, d);
    REQUIRE(snow >= 0);
    int unreachable = 0;
    for (size_t i = 0; i < g->size(); ++i)
        if (int(i) != snow && g->NextToward(int(i), snow, 0) < 0)
            ++unreachable;
    CHECK(unreachable == 0);
    // And back: the Frostwolf base is reachable from the valley.
    int const base = g->Nearest(-1392.f, -318.f, 89.f, d);
    CHECK(g->NextToward(snow, base, 0) >= 0);
}

TEST_CASE("AV: a bot between two recorded nodes is not sent back and forth", "[battleground][av]")
{
    using namespace Playerbot::BgRoutes;
    float nx = 0.f, ny = 0.f, nz = 0.f;
    // Bot-only AV 254: Horde bots on the way to the Stonehearth waiting spot (28,-303) turned round between (-418.6,-342.5,18.7) and
    // (-398.0,-372.6,31.4), a node recorded 14 y above the ground the bots stand on at (-400,-367,17). Under it the route sent them back.
    for (int v = 0; v < Graph::kVariants; ++v)
    {
        REQUIRE(NextWaypoint(1, -400.f, -367.f, 17.f, 28.1f, -302.6f, 15.1f, 28.0f, nx, ny, nz, v));
        CHECK(!(std::fabs(nx - -418.6f) < 1.f && std::fabs(ny - -342.5f) < 1.f));
        CHECK(nx > -405.f);   // onwards, never back west
    }
    // The Alliance cave slope (AV 253/254: 417 turns near (488,-427) on the way to Dun Baldar): both sides of the node go the same way.
    float ax = 0.f, ay = 0.f, az = 0.f;
    REQUIRE(NextWaypoint(1, 491.f, -427.f, 37.f, 648.f, -61.f, 42.f, 28.0f, nx, ny, nz, 0));
    REQUIRE(NextWaypoint(1, 485.f, -428.f, 37.f, 648.f, -61.f, 42.f, 28.0f, ax, ay, az, 0));
    CHECK(nx == ax);
    CHECK(ny == ay);
    // The Frostwolf graveyard (256 turns at (-1055,-373)): the nearest node is on the one-way path into the graveyard, away from the goal.
    REQUIRE(NextWaypoint(1, -1055.f, -373.f, 51.f, 28.1f, -302.6f, 15.1f, 28.0f, nx, ny, nz, 0));
    CHECK(nx > -1055.f);
    // AV 255: Horde bots on the one-way Iceblood graveyard path at (-567,-391,52) were sent 35 y across to (-536.6,-373.8), the first
    // node of the tower-crossroad path, which no walk connects from there ([wedge] cat=Navmesh). They stay on the recorded path.
    for (int v = 0; v < Graph::kVariants; ++v)
    {
        REQUIRE(NextWaypoint(1, -567.f, -391.f, 52.f, 28.1f, -302.6f, 15.1f, 28.0f, nx, ny, nz, v));
        CHECK(!(std::fabs(nx - -536.6f) < 1.f && std::fabs(ny - -373.8f) < 1.f));
    }

    // A walk along the route (8 y steps toward the waypoint) reaches the waiting spot from the Iceblood Tower crossroad in every
    // variant without turning round.
    for (int v = 0; v < Graph::kVariants; ++v)
    {
        float bx = -579.7f, by = -315.0f, bz = 46.3f;
        bool reached = false;
        for (int i = 0; i < 400 && !reached; ++i)
        {
            if (!NextWaypoint(1, bx, by, bz, 28.1f, -302.6f, 15.1f, 28.0f, nx, ny, nz, v)) { nx = 28.1f; ny = -302.6f; nz = 15.1f; }
            float const dx = nx - bx, dy = ny - by, d = std::sqrt(dx * dx + dy * dy);
            if (d < 0.5f) break;
            float const st = std::min(8.0f, d);
            bx += dx / d * st; by += dy / d * st; bz += (nz - bz) * st / d;
            reached = (28.1f - bx) * (28.1f - bx) + (-302.6f - by) * (-302.6f - by) < 25.f * 25.f;
        }
        CHECK(reached);
    }
}

TEST_CASE("AV: the node squad waits on the near side of its target", "[battleground][av]")
{
    using namespace Playerbot::BgRoutes;
    float x = 0.f, y = 0.f, z = 0.f;
    // AV 257: the Horde gathered for Stonehearth Bunker (-152.4,-441.8) at Balinda's spot (28,-303), past the bunker. From the Horde
    // base the spot is now 100 yd short of the bunker, on the road in from Iceblood, west of it.
    REQUIRE(ApproachPoint(1, -890.f, -575.f, 70.24f, -152.4f, -441.8f, 40.4f, 100.f, x, y, z));
    CHECK(x < -152.4f - 50.f);
    CHECK((x + 152.4f) * (x + 152.4f) + (y + 441.8f) * (y + 441.8f) >= 100.f * 100.f);
    // The Alliance on Iceblood Tower (-572.3,-262.5): the same spot as Galvangar's waiting spot (-492,-187).
    REQUIRE(ApproachPoint(1, 873.98f, -491.79f, 96.54f, -572.3f, -262.5f, 88.6f, 100.f, x, y, z));
    CHECK(std::fabs(x - -492.2f) < 2.f);
    CHECK(std::fabs(y - -187.1f) < 2.f);
}

TEST_CASE("AV: the general squad waits below the enemy keep, on the way in", "[battleground][av]")
{
    using namespace Playerbot::BgRoutes;
    float x = 0.f, y = 0.f, z = 0.f;
    // Alliance on Drek'Thar (-1370.9,-220.2): below Frostwolf Keep, about 200 yd short, outside his room.
    REQUIRE(ApproachPoint(1, 873.98f, -491.79f, 96.54f, -1370.9f, -220.2f, 98.5f, 200.f, x, y, z));
    CHECK(std::fabs(x - -1210.2f) < 2.f);
    CHECK(std::fabs(y - -366.5f) < 2.f);
    // Horde on Vanndar (722.4,-11.0): in Dun Baldar below the bridge to the inner keep.
    REQUIRE(ApproachPoint(1, -890.f, -575.f, 70.24f, 722.4f, -11.0f, 50.7f, 200.f, x, y, z));
    CHECK(std::fabs(x - 625.3f) < 2.f);
    CHECK(std::fabs(y - -191.9f) < 2.f);
}

TEST_CASE("AV: the Horde reaches its captain waiting spot from Iceblood without passing Balinda", "[battleground][av]")
{
    using namespace Playerbot::BgRoutes;
    // AlteracValleyScript's Horde captain staging (-171.3,-266.6,9.1), 115 yd west of Balinda (-57.8,-286.6) on the river road.
    for (int v = 0; v < Graph::kVariants; ++v)
    {
        float bx = -612.5f, by = -397.0f, bz = 60.9f, nx = 0.f, ny = 0.f, nz = 0.f;
        bool reached = false;
        for (int i = 0; i < 400 && !reached; ++i)
        {
            if (!NextWaypoint(1, bx, by, bz, -171.3f, -266.6f, 9.1f, 28.0f, nx, ny, nz, v)) { nx = -171.3f; ny = -266.6f; nz = 9.1f; }
            float const dx = nx - bx, dy = ny - by, d = std::sqrt(dx * dx + dy * dy);
            if (d < 0.5f) break;
            float const st = std::min(8.0f, d);
            bx += dx / d * st; by += dy / d * st; bz += (nz - bz) * st / d;
            CHECK(bx < -120.f);   // never east of the spot, into Stonehearth Outpost
            reached = (-171.3f - bx) * (-171.3f - bx) + (-266.6f - by) * (-266.6f - by) < 10.f * 10.f;
        }
        CHECK(reached);
    }
}

TEST_CASE("AV: the general push walks round a living enemy captain's garrison", "[battleground][av]")
{
    using namespace Playerbot::BgRoutes;
    // AV 263: from Snowfall the plain route to Drek'Thar runs through Iceblood Garrison, past Galvangar (-545.2,-165.4).
    auto walk = [](int variant, float& closest)
    {
        float bx = -202.5f, by = -113.0f, bz = 78.0f, nx = 0.f, ny = 0.f, nz = 0.f;
        closest = 1e9f;
        for (int i = 0; i < 600; ++i)
        {
            if (!NextWaypoint(1, bx, by, bz, -1370.9f, -220.2f, 98.5f, 28.0f, nx, ny, nz, variant)) { nx = -1370.9f; ny = -220.2f; nz = 98.5f; }
            float const dx = nx - bx, dy = ny - by, d = std::sqrt(dx * dx + dy * dy);
            if (d < 0.5f) break;
            float const st = std::min(8.0f, d);
            bx += dx / d * st; by += dy / d * st; bz += (nz - bz) * st / d;
            closest = std::min(closest, std::hypot(bx - -545.2f, by - -165.4f));
        }
        return std::hypot(bx - -1370.9f, by - -220.2f) < 25.f;
    };
    float plain = 0.f, round = 0.f;
    REQUIRE(walk(0, plain));
    CHECK(plain < kCaptainAvoidRadius);
    REQUIRE(walk(kAvoidGalvangar * Graph::kVariants, round));
    CHECK(round > 2.f * kCaptainAvoidRadius);
}

TEST_CASE("AV route: no waypoint when the recorded graph has no way to the goal", "[battleground][route]")
{
    using namespace Playerbot::BgRoutes;
    // Snowfall to (-612,-397,37): the node nearest that goal, (-571,-397,53), lies on a path Snowfall has no way onto. The start node
    // came back as the waypoint 20-28 yd from it and the bot turned between it and the plain move for good.
    float nx = 0.f, ny = 0.f, nz = 0.f;
    CHECK(!NextWaypoint(1, -229.f, -131.f, 78.f, -612.f, -397.f, 37.f, 28.f, nx, ny, nz, 0));
    CHECK(!NextWaypoint(1, -222.f, -126.f, 78.f, -612.f, -397.f, 37.f, 28.f, nx, ny, nz, 0));
    // with the banner's height the goal is Iceblood Graveyard's own node, and the route runs
    CHECK(NextWaypoint(1, -229.f, -131.f, 78.f, -612.f, -396.f, 61.f, 28.f, nx, ny, nz, 0));
}
