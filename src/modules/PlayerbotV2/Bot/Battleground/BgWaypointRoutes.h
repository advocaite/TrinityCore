#pragma once

// Hand-recorded battleground walk paths for the classic maps (WSG 489, AB 529, AV 30), taken from ike3's BattleGroundTactics
// (vmangos). The paths form a graph (every path point is a node, consecutive points and path junctions are edges). The bot rules
// keep choosing the objective (orders, roles); this only answers "which waypoint do I walk to next to get there", so a long
// march follows the tunnels / bridges / ramps the paths were recorded along instead of one straight navmesh request.
//
// Header only on purpose: adding it needs no cmake reconfigure.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Playerbot::BgRoutes
{

struct RoutePoint { float x, y, z; };
struct RoutePath { char const* name; std::vector<RoutePoint> pts; };

#include "BgWaypointData.inc"

// A path that may only be walked first point to last: a jump / roof drop, and the start-room, cave and graveyard exits that the
// original bots never walk backwards (vPaths_NoReverseAllowed, Karrz1/wow-1.12 raid10-revamp bcc8b9f BattleBotWaypoints.cpp).
// Horde_Cave_to_Tower_Point_Crossroad is in their list but is NOT one-way here: our router also uses it backwards, it is the only
// link by which a bot from the Alliance half reaches the Horde base (the test below fails when it is made one-way).
inline bool IsOneWayPath(std::string_view nm)
{
    if (nm.find("Jump") != std::string_view::npos || nm.find("Roof") != std::string_view::npos)
        return true;
    static constexpr std::string_view kNoReverse[] = {
        "AllianceBase_to_Stables", "AllianceBase_to_GoldMine", "AllianceBase_to_LumberMill",
        "HordeBase_to_Farm", "HordeBase_to_GoldMine", "HordeBase_to_LumberMill",
        "Frostwolf_Graveyard_to_Frostwolf_Graveyard_Flag",
        "Iceblood_Graveyard_to_Iceblood_Graveyard_Flag", "Stonehearth_Graveyard_to_Stonehearth_Graveyard_Flag",
        "Alliance_Cave_to_Alliance_Cave_Slop_Crossroad", "Horde_Cave_to_Frostwolf_Graveyard_Flag",
        "Alliance_Cave_Slop_Crossroad_to_Alliance_Slope_Crossroad", "Stormpike_Graveyard_to_Stormpike_Flag",
    };
    for (std::string_view const n : kNoReverse)
        if (nm == n) return true;
    return false;
}

class Graph
{
public:
    // junction_xy: recordings of one place that lie within this many yards (and 4 y in height) are the same place. AV needs 3.5: the
    // Frostwolf graveyard flag was recorded twice 3.15 y apart ((-1081.3,-348.2) and (-1079.6,-345.5)), which cut the whole Horde half
    // of the map (129 nodes) off from the valley, so no Horde bot had a route to any Alliance objective (av-stuck-2, 2026-10-11).
    Graph(RoutePath const* paths, size_t count, float junction_xy = 2.5f)
    {
        for (size_t pi = 0; pi < count; ++pi)
        {
            RoutePath const& path = paths[pi];
            names_.push_back(path.name);
            // A jump / roof path is a one-way drop or climb; everything else can be walked both ways.
            std::string_view const nm = path.name;
            // A roof path is recorded jumps up onto the base roof, an area the navmesh does not connect: a bot snapped onto one of its nodes
            // (the nearest node to a bot beside the base wall) was sent up it and stood there unable to path (WSG 2026-10-09, bot at
            // (1534,1387,348): NoPath to every waypoint). The path keeps its name entry so the indices stay aligned but has no nodes.
            if (nm.find("Roof") != std::string_view::npos)
                continue;
            bool const one_way = IsOneWayPath(nm);
            int prev = -1;
            for (RoutePoint const& p : path.pts)
            {
                int const id = int(nodes_.size());
                nodes_.push_back({p, {}, {}, uint16_t(pi)});
                if (prev >= 0)
                {
                    AddEdge(prev, id);
                    if (!one_way) AddEdge(id, prev);
                }
                prev = id;
            }
        }
        // Junctions: points of different paths that lie on top of each other are the same place.
        for (size_t a = 0; a < nodes_.size(); ++a)
            for (size_t b = a + 1; b < nodes_.size(); ++b)
            {
                RoutePoint const& pa = nodes_[a].p;
                RoutePoint const& pb = nodes_[b].p;
                float const dx = pa.x - pb.x, dy = pa.y - pb.y;
                if (dx * dx + dy * dy <= junction_xy * junction_xy && std::fabs(pa.z - pb.z) <= 4.0f)
                {
                    AddEdge(int(a), int(b));
                    AddEdge(int(b), int(a));
                }
            }
    }

    size_t size() const { return nodes_.size(); }

    // Nearest node to (x,y,z); z counts triple so a point on the upper level does not snap to the tunnel below it.
    int Nearest(float x, float y, float z, float& dist) const
    {
        int best = -1;
        float best_d = std::numeric_limits<float>::max();
        for (size_t i = 0; i < nodes_.size(); ++i)
        {
            float const dx = nodes_[i].p.x - x, dy = nodes_[i].p.y - y, dz = (nodes_[i].p.z - z) * 3.0f;
            float const d = dx * dx + dy * dy + dz * dz;
            if (d < best_d) { best_d = d; best = int(i); }
        }
        dist = best >= 0 ? std::sqrt(best_d) : 0.0f;
        return best;
    }

    // Next node to step to from `from` on the shortest path to `target`, or -1 when `target` is unreachable.
    // variant 0 = the plain shortest path; 1..kVariants-1 scale every recorded path's length by a per-variant factor (0.75 .. 1.6) so
    // different bots / different runs prefer different tunnels, graveyard jumps and roofs instead of all walking one lane.
    static constexpr int kVariants = 8;
    int NextToward(int from, int target, int variant = 0) const
    {
        if (from == target) return -1;
        Table const& t = HopsTo(target, variant % kVariants);
        return t.hop[size_t(from)];
    }

    // Weighted distance from `from` to `target` on the recorded paths under `variant`; +inf when unreachable.
    float DistanceTo(int from, int target, int variant = 0) const
    {
        if (from == target) return 0.0f;
        return HopsTo(target, variant % kVariants).dist[size_t(from)];
    }

    // The first / last node of the recorded path called `name`, or -1.
    int PathEnd(std::string_view name, bool last) const
    {
        int found = -1;
        for (size_t i = 0; i < nodes_.size(); ++i)
            if (std::string_view(PathName(nodes_[i].path)) == name)
            {
                if (!last) return int(i);
                found = int(i);
            }
        return found;
    }

    RoutePoint const& Point(int id) const { return nodes_[size_t(id)].p; }

private:
    struct Table { std::vector<int> hop; std::vector<float> dist; };
    char const* PathName(uint16_t path) const { return path < names_.size() ? names_[path] : ""; }
    struct Node { RoutePoint p; std::vector<std::pair<int, float>> out; std::vector<std::pair<int, float>> in; uint16_t path = 0; };

    void AddEdge(int a, int b)
    {
        float const dx = nodes_[size_t(a)].p.x - nodes_[size_t(b)].p.x;
        float const dy = nodes_[size_t(a)].p.y - nodes_[size_t(b)].p.y;
        float const dz = nodes_[size_t(a)].p.z - nodes_[size_t(b)].p.z;
        float const w = std::sqrt(dx * dx + dy * dy + dz * dz) + 0.01f;
        nodes_[size_t(a)].out.emplace_back(b, w);
        nodes_[size_t(b)].in.emplace_back(a, w);
    }

    // Dijkstra from the target over the reversed edges: hop[v] = the neighbour to move to from v on the way to target.
    Table const& HopsTo(int target, int variant) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        int const key = target * kVariants + variant;
        auto it = cache_.find(key);
        if (it != cache_.end()) return it->second;
        size_t const n = nodes_.size();
        std::vector<float> dist(n, std::numeric_limits<float>::max());
        std::vector<int> hop(n, -1);
        using QItem = std::pair<float, int>;
        std::priority_queue<QItem, std::vector<QItem>, std::greater<QItem>> pq;
        dist[size_t(target)] = 0.0f;
        pq.emplace(0.0f, target);
        while (!pq.empty())
        {
            auto [d, v] = pq.top();
            pq.pop();
            if (d > dist[size_t(v)]) continue;
            for (auto const& [u, w] : nodes_[size_t(v)].in)       // u -> v exists
                if (float const w2 = w * Factor(nodes_[size_t(u)].path, variant); d + w2 < dist[size_t(u)])
                {
                    dist[size_t(u)] = d + w2;
                    hop[size_t(u)] = v;
                    pq.emplace(d + w2, u);
                }
        }
        return cache_.emplace(key, Table{std::move(hop), std::move(dist)}).first->second;
    }

    static float Factor(uint16_t path, int variant)
    {
        if (variant == 0) return 1.0f;
        uint32_t h = (uint32_t(path) + 1u) * 2654435761u ^ (uint32_t(variant) * 40503u + 0x9E3779B9u);
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
        return 0.75f + 0.85f * float(h % 1000u) / 999.0f;
    }

    std::vector<Node> nodes_;
    mutable std::mutex mutex_;
    mutable std::unordered_map<int, Table> cache_;
    std::vector<char const*> names_;
};

inline Graph const* GraphFor(uint16_t bg_type)
{
    static Graph const wsg(kPathsWsg, sizeof(kPathsWsg) / sizeof(kPathsWsg[0]));
    static Graph const ab(kPathsAb, sizeof(kPathsAb) / sizeof(kPathsAb[0]));
    static Graph const av(kPathsAv, sizeof(kPathsAv) / sizeof(kPathsAv[0]), 3.5f);
    switch (bg_type)
    {
        case 1: return &av;
        case 2: return &wsg;
        case 3: return &ab;
        default: return nullptr;
    }
}

// The waypoint to walk to next on the way from (bx,by,bz) to (tx,ty,tz): the first node on the recorded route that is at least
// `lookahead` yards from the bot (or the route's last node). False = no usable route (not a classic BG, a leg shorter than 40 y,
// the bot or the goal far from every recorded path, no connection): the caller then walks straight to the goal as before.
inline bool NextWaypoint(uint16_t bg_type, float bx, float by, float bz, float tx, float ty, float tz, float lookahead,
                         float& nx, float& ny, float& nz, int variant = 0)
{
    Graph const* g = GraphFor(bg_type);
    if (!g || g->size() == 0) return false;
    float const gdx = tx - bx, gdy = ty - by;
    if (gdx * gdx + gdy * gdy < 40.0f * 40.0f) return false;
    float db = 0.f, dt = 0.f;
    int const from = g->Nearest(bx, by, bz, db);
    int const to = g->Nearest(tx, ty, tz, dt);
    if (from < 0 || to < 0 || dt > 70.0f) return false;
    if (db > 120.0f)
    {
        // Feeder leg (AV 2026-10-11, av-stuck-2: ~20 Horde bots stood at the start (-1438,-610) for the whole match). The recorded
        // paths start at the Frostwolf base, 295 y away; from the start the plain move to a goal 1300-1500 y off found only a partial
        // path that went nowhere (move_blocked 20 on idle:bg_order_march). A bot off the graph but within kFeederMax of it walks to the
        // nearest recorded node first, a goal Detour can path toward; the route takes over from there. AV only: the WSG / AB
        // recordings already cover their start rooms. Only when that node reaches the goal on the graph.
        constexpr float kFeederMax = 480.0f;
        if (bg_type != 1 || db > kFeederMax || (from != to && g->NextToward(from, to, variant) < 0)) return false;
        RoutePoint const& f = g->Point(from);
        nx = f.x; ny = f.y; nz = f.z;
        return true;
    }
    int cur = from;
    int steps = 0;
    while (steps++ < 64)
    {
        RoutePoint const& p = g->Point(cur);
        float const dx = p.x - bx, dy = p.y - by;
        if (cur == to || dx * dx + dy * dy >= lookahead * lookahead)
        {
            nx = p.x; ny = p.y; nz = p.z;
            // The bot is already past the last recorded node of the route and the goal is the next thing: go straight.
            return !(cur == to && dx * dx + dy * dy < 9.0f);
        }
        int const next = g->NextToward(cur, to, variant);
        if (next < 0)
        {
            if (cur == from) return false;        // goal not connected to the bot's path
            nx = p.x; ny = p.y; nz = p.z;
            return true;
        }
        cur = next;
    }
    return false;
}

// ---- Lanes ----------------------------------------------------------------------------------------------------------------------
// A lane is one of the ways across the field a bot may take: either one of the recorded graph paths above (a graveyard-ridge jump
// into the enemy flag room, the second recording of the mid line), or a chain of hand-recorded waypoints (BgWsgLanes.inc: the
// tunnel, the graveyard ridge and the north ramp out of the own base, the mid field, then the enemy tunnel or ramp). The route to a
// goal is forced through the lane, so a team spreads over the whole map instead of every bot walking the one shortest line (the owner,
// WSG 2026-10-09: "just straight out the tunnel and straight back up the tunnel"). Nothing here is invented: every coordinate comes
// from recorded data.
// drop: the leg from the previous point to this one is a drop off a ledge (the previous point is the ledge, this one the landing).
// It is crossed with a committed one-way drop move, never pathed: the navmesh has no link down that edge.
struct LanePoint { float x, y, z; bool drop = false; };
struct Lane
{
    char const* path;                         // a recorded graph path to be forced, or nullptr
    std::vector<LanePoint> const* waypoints;  // a hand-recorded waypoint chain to be followed, or nullptr
    int weight;
    bool flank;                               // a flank lane: not through the base tunnels / the plain mid line (flag carriers)
    char const* name;
};

#include "BgWsgLanes.inc"

// The lanes a bot may take on a run toward `east` (the goal lies at larger x) in this battleground; flag carriers only take flank lanes.
inline std::vector<Lane> const& LanesFor(uint16_t bg_type, bool east, bool flank_only)
{
    static std::vector<Lane> const none;
    if (bg_type != 2)  // Warsong Gulch only
        return none;
    static std::vector<Lane> const lanes[2][2] = {
        { BuildWsgLanes(false, false), BuildWsgLanes(false, true) },
        { BuildWsgLanes(true, false),  BuildWsgLanes(true, true) },
    };
    return lanes[east ? 1 : 0][flank_only ? 1 : 0];
}

// A fight below the graveyard ridge: the ridge lane whose drop the bot stands above, when the bot is on the ridge within `reach` y of the ledge
// and has not dropped yet (it stands more than 8 y above the landing). Out: the ridge lane (of the two, `selector` picks) and the landing.
// The caller sends the bot over the ledge, instead of the random lane, when enemies are at the landing (playtest, WSG 2026-10-09: the whole
// rez wave ran the long way round the back while the enemy was fighting below the cliff).
inline Lane const* RidgeDropAbove(std::vector<Lane> const& lanes, uint32_t selector, float bx, float by, float bz, float reach, LanePoint* landing = nullptr)
{
    Lane const* found[2] = { nullptr, nullptr };
    LanePoint land{};
    int n = 0;
    for (Lane const& l : lanes)
    {
        if (!l.waypoints || std::string_view(l.name).substr(0, 5) != "ridge") continue;
        for (size_t i = 1; i < l.waypoints->size(); ++i)
        {
            LanePoint const& d = (*l.waypoints)[i];
            if (!d.drop) continue;
            LanePoint const& ledge = (*l.waypoints)[i - 1];
            float const dx = ledge.x - bx, dy = ledge.y - by;
            if (dx * dx + dy * dy > reach * reach || bz - d.z < 8.0f) break;
            if (n < 2) found[n++] = &l;
            land = d;
            break;
        }
    }
    if (n == 0) return nullptr;
    if (landing) *landing = land;
    return found[n == 2 ? selector % 2u : 0u];
}

// Picks a lane for `selector` (a per-bot hash, shifted by the run so the choice changes from cap to cap). nullptr = the plain route.
inline Lane const* PickLane(std::vector<Lane> const& lanes, uint32_t selector)
{
    int total = 0;
    for (Lane const& l : lanes) total += l.weight;
    if (total <= 0) return nullptr;
    int roll = int(selector % uint32_t(total));
    for (Lane const& l : lanes)
    {
        if (roll < l.weight) return (l.path || l.waypoints) ? &l : nullptr;
        roll -= l.weight;
    }
    return nullptr;
}

// Follows a hand-recorded waypoint chain. A bot within 45 y of the chain walks to the end of the segment it stands on (the next one
// once it is within 12 y of it); a bot not on the chain yet that is still before its entry (in the direction of travel) heads for the
// entry along the recorded graph; past the entry, or at the chain's end, the plain route takes over. The state is the bot's position.
// `drop` (out) is set when the bot stands at a ledge, or is already in the air, on a recorded drop leg: nx/ny/nz is then the landing
// and the caller crosses with the committed drop move. On the way to a ledge the target is the ledge itself, so the plain route never
// has to path down it. `approach` (out) is set when the target is a ledge the bot is within 10 y of: the recorded ledge sits on the cliff
// edge, usually a few yards off the navmesh, so a pathed move stops short (path_fail_backoff, "inching toward the same wp" in the WSG log
// 2026-10-09, 227 ridge routes against 3 drops) and the 3.5 y ledge reach is never met; the caller walks that last stretch straight.
inline bool FollowLaneWaypoints(uint16_t bg_type, std::vector<LanePoint> const& w, bool east, float bx, float by, float bz,
                                float tx, float ty, float tz, float lookahead, float& nx, float& ny, float& nz, int variant, bool* drop = nullptr,
                                bool* approach = nullptr)
{
    if (drop) *drop = false;
    if (approach) *approach = false;
    size_t const n = w.size();
    if (n < 2) return false;
    float best = 45.0f * 45.0f;
    size_t bi = n;
    for (size_t i = 0; i + 1 < n; ++i)
    {
        float const ax = w[i].x, ay = w[i].y, dx = w[i + 1].x - ax, dy = w[i + 1].y - ay;
        float const len2 = dx * dx + dy * dy;
        float const t = len2 > 0.0f ? std::clamp(((bx - ax) * dx + (by - ay) * dy) / len2, 0.0f, 1.0f) : 0.0f;
        float const px = ax + t * dx - bx, py = ay + t * dy - by;
        float const pz = w[i].z + t * (w[i + 1].z - w[i].z) - bz;
        if (std::fabs(pz) > 25.0f) continue;
        float const d2 = px * px + py * py;
        if (d2 < best) { best = d2; bi = i; }
    }
    if (bi < n)
    {
        constexpr float kLedgeR = 3.5f;                   // a ledge or a landing is reached to this precision, any other point to 12 y
        size_t next = bi + 1;
        auto const reach2 = [&](size_t i) { float const r = (w[i].drop || (i + 1 < n && w[i + 1].drop)) ? kLedgeR : 12.0f; return r * r; };
        if (w[next].drop)
        {
            float const ax = w[next].x - bx, ay = w[next].y - by;
            if (ax * ax + ay * ay < kLedgeR * kLedgeR && std::fabs(w[next].z - bz) < 4.0f)
            {
                if (next + 1 >= n) return false;          // landed at the chain's end: on to the goal
                ++next;                                    // landed: the leg after the landing
                nx = w[next].x; ny = w[next].y; nz = w[next].z;
                return true;
            }
        }
        if (w[next].drop)
        {
            // On a drop leg: at the ledge (or already below it, in the air) the landing is the target and the drop is committed;
            // away from it the target is the ledge.
            float const lx = w[bi].x - bx, ly = w[bi].y - by;
            bool const at_ledge = lx * lx + ly * ly < kLedgeR * kLedgeR || bz < w[bi].z - 3.0f;
            if (at_ledge)
            {
                nx = w[next].x; ny = w[next].y; nz = w[next].z;
                if (drop) *drop = true;
            }
            else
            {
                nx = w[bi].x; ny = w[bi].y; nz = w[bi].z;
                if (approach && lx * lx + ly * ly < 10.0f * 10.0f) *approach = true;
            }
            return true;
        }
        float const ex = w[next].x - bx, ey = w[next].y - by;
        if (ex * ex + ey * ey < reach2(next))
        {
            if (next + 1 >= n) return false;          // at the chain's end: on to the goal
            ++next;
            if (w[next].drop)
            {
                nx = w[next].x; ny = w[next].y; nz = w[next].z;   // reached the ledge: leave it
                if (drop) *drop = true;
                return true;
            }
        }
        nx = w[next].x; ny = w[next].y; nz = w[next].z;
        if (approach && next + 1 < n && w[next + 1].drop && ex * ex + ey * ey < 10.0f * 10.0f) *approach = true;
        return true;
    }
    float const progress = east ? bx : -bx, entry = east ? w[0].x : -w[0].x;
    if (progress < entry - 10.0f)
    {
        if (!NextWaypoint(bg_type, bx, by, bz, w[0].x, w[0].y, w[0].z, lookahead, nx, ny, nz, variant))
        {
            nx = w[0].x; ny = w[0].y; nz = w[0].z;
        }
        return true;
    }
    (void)tx; (void)ty; (void)tz;
    return false;
}

// NextWaypoint, forced through the recorded path `lane_path` (nullptr = no forced path). A bot standing on the lane (within 20 y of
// one of its nodes) walks the lane's own nodes in order toward its far end, then continues to the goal; a bot that is not on it yet
// and is farther from the goal than the lane's entry heads for the entry; a bot already past the entry, or a lane that does not lead
// toward the goal (going home, a one-way drop the wrong way), uses the plain route. The state is the bot's position alone.
inline bool NextWaypointLane(uint16_t bg_type, float bx, float by, float bz, float tx, float ty, float tz, float lookahead,
                             float& nx, float& ny, float& nz, int variant, Lane const* lane, bool* drop = nullptr,
                             bool* approach = nullptr)
{
    if (drop) *drop = false;
    if (approach) *approach = false;
    if (lane && lane->waypoints)
        return FollowLaneWaypoints(bg_type, *lane->waypoints, tx > bx, bx, by, bz, tx, ty, tz, lookahead, nx, ny, nz, variant, drop, approach);
    char const* const lane_path = lane ? lane->path : nullptr;
    if (lane_path)
        if (Graph const* g = GraphFor(bg_type); g && g->size() > 0)
        {
            float db = 0.f, dt = 0.f;
            int const from = g->Nearest(bx, by, bz, db);
            int const to = g->Nearest(tx, ty, tz, dt);
            int first = g->PathEnd(lane_path, false);
            int last = g->PathEnd(lane_path, true);
            if (from >= 0 && to >= 0 && first >= 0 && last > first && db <= 120.0f && dt <= 70.0f)
            {
                constexpr float kInf = std::numeric_limits<float>::max();
                // A walkable (two-way) path can be taken either way: enter at the end farther from the goal. A jump / roof path is
                // one-way, first to last, and is only a lane when that way leads toward the goal.
                std::string_view const nm = lane_path;
                bool const one_way = IsOneWayPath(nm);
                if (!one_way && g->DistanceTo(first, to, 0) < g->DistanceTo(last, to, 0))
                    std::swap(first, last);
                int const step = last > first ? 1 : -1;
                float const d_from  = g->DistanceTo(from, to, 0);
                float const d_first = g->DistanceTo(first, to, 0);
                float const d_last  = g->DistanceTo(last, to, 0);
                if (d_from < kInf && d_first < kInf && d_last < kInf && d_first > d_last + 40.0f)   // the lane leads toward the goal
                {
                    // Nearest node of the lane itself.
                    int k = -1;
                    float best = 20.0f * 20.0f;
                    for (int n = first; n != last + step; n += step)
                    {
                        RoutePoint const& p = g->Point(n);
                        float const dx = p.x - bx, dy = p.y - by, dz = p.z - bz;
                        if (std::fabs(dz) > 8.0f) continue;
                        float const d2 = dx * dx + dy * dy;
                        if (d2 < best) { best = d2; k = n; }
                    }
                    if (k >= 0 && k != last)
                    {
                        // On the lane: the first lane node at least `lookahead` ahead (or the lane's last node).
                        int n = k;
                        while (n != last)
                        {
                            n += step;
                            RoutePoint const& p = g->Point(n);
                            float const dx = p.x - bx, dy = p.y - by;
                            if (dx * dx + dy * dy >= lookahead * lookahead) break;
                        }
                        RoutePoint const& p = g->Point(n);
                        nx = p.x; ny = p.y; nz = p.z;
                        return true;
                    }
                    if (k < 0 && d_from > d_first + 12.0f)
                    {
                        RoutePoint const& v = g->Point(first);
                        return NextWaypoint(bg_type, bx, by, bz, v.x, v.y, v.z, lookahead, nx, ny, nz, 0) ||
                               (nx = v.x, ny = v.y, nz = v.z, true);
                    }
                }
            }
        }
    return NextWaypoint(bg_type, bx, by, bz, tx, ty, tz, lookahead, nx, ny, nz, variant);
}

} // namespace Playerbot::BgRoutes
