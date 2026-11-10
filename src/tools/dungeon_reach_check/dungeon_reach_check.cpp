/*
 * dungeon_reach_check - offline check that the bots can walk a dungeon boss to boss on the navmesh.
 *
 * Reads a list of dungeons (the entrance where the party lands and the bosses in the order the bots take them), loads the mmaps of each
 * map, and walks the navmesh with the same filter a player's PathGenerator uses (ground, steep ground, water, magma/slime, road). Every
 * leg is checked: entrance to the first boss, then each boss to the next. A boss fails when no navmesh lies under its spot, or when its
 * spot is on a piece of navmesh the previous stop cannot walk to (an island). For a failed boss the line says how big the island is and
 * how close the walkable floor gets, so the gap can be found in game. A leg that only goes one way (a drop with no way back up) is a
 * warning, not a failure.
 *
 * Usage: dungeon_reach_check <mmaps_dir> <list.txt> [--islands] [--map <id>]
 *   --islands   also list every other piece of navmesh of at least 20 polygons on the map, with its middle and the nearest boss
 *   --map <id>  only the dungeons on that map (repeat for more)
 *
 * List file, one line each, '#' starts a comment:
 *   dungeon <map_id> <key> [name ...]       starts a dungeon; a map can have several (one per wing or entrance)
 *   start <x> <y> <z> [label ...]           where the party lands (the entrance areatrigger target)
 *   boss <entry> <x> <y> <z> [name ...]     the next boss, in the order the bots take them
 *   missing <entry> [name ...]              a boss with no known spot (summoned, or no spawn): listed as not checked
 *
 * Exit code: 0 every boss reached, 1 at least one boss cannot be reached, 2 bad input or missing mmaps.
 *
 * Pure Detour API, like mmap_probe and walk_link_check: no TC core, no worldserver, no database. The mmaps must be the ones the
 * server runs on.
 */

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourCommon.h>
#include <DetourAlloc.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Header layout as in src/common/mmaps_common/MMapDefines.h; must match the generator output byte for byte.
static constexpr uint32_t MMAP_MAGIC = 0x4d4d4150; // 'MMAP'
static constexpr uint32_t MMAP_VERSION_EXPECTED = 16;

#pragma pack(push, 1)
struct MmapNavMeshHeader
{
    uint32_t mmapMagic;
    uint32_t mmapVersion;
    dtNavMeshParams params;
    uint32_t offmeshConnectionCount;
};
struct MmapTileHeader
{
    uint32_t mmapMagic;
    uint32_t dtVersion;
    uint32_t mmapVersion;
    uint32_t size;
    char     usesLiquids;
    char     padding[3];
};
#pragma pack(pop)

static_assert(sizeof(MmapNavMeshHeader) == 40, "MmapNavMeshHeader size drift");
static_assert(sizeof(MmapTileHeader) == 20, "MmapTileHeader size drift");

// NavTerrainFlag (MMapDefines.h): 1 << (NAV_AREA_MAX_VALUE - area).
static constexpr uint16_t NAV_GROUND       = 0x01;
static constexpr uint16_t NAV_GROUND_STEEP = 0x02;
static constexpr uint16_t NAV_WATER        = 0x04;
static constexpr uint16_t NAV_MAGMA_SLIME  = 0x08;
static constexpr uint16_t NAV_ROAD         = 0x10;

// A boss whose own spot is not walkable still counts as reached when walkable floor comes this close: melee reach plus the boss's size.
static constexpr float kFightReachYards = 5.0f;

// An entrance whose walkable floor is smaller than this is a pocket (the dungeon floor is elsewhere): warned about.
static constexpr size_t kSmallPieceForEntrance = 200;

namespace
{
struct Point
{
    float x = 0.f, y = 0.f, z = 0.f;   // TC world coordinates
};

struct Stop
{
    bool is_boss = false;
    uint32_t entry = 0;
    Point pos;
    std::string name;
};

struct Dungeon
{
    uint32_t map = 0;
    std::string key;
    std::string name;
    std::vector<Stop> stops;     // stops[0] is the start
    std::vector<Stop> missing;   // bosses with no known spot: printed, not checked
};

// TC (x, y, z) <-> Detour (y, z, x), as PathGenerator does.
void ToDetour(Point const& p, float* out)
{
    out[0] = p.y;
    out[1] = p.z;
    out[2] = p.x;
}

Point FromDetour(float const* d)
{
    return Point{ d[2], d[0], d[1] };
}

float Dist3(Point const& a, Point const& b)
{
    float const dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

std::string Trim(std::string s)
{
    size_t const a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return {};
    size_t const b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string RestOfLine(std::istringstream& iss)
{
    std::string rest;
    std::getline(iss, rest);
    return Trim(rest);
}

bool ReadList(std::string const& path, std::vector<Dungeon>& out)
{
    std::ifstream in(path);
    if (!in)
    {
        std::fprintf(stderr, "ERROR: cannot open %s\n", path.c_str());
        return false;
    }
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line))
    {
        ++lineNo;
        if (size_t const hash = line.find('#'); hash != std::string::npos)
            line.resize(hash);
        line = Trim(line);
        if (line.empty())
            continue;
        std::istringstream iss(line);
        std::string kind;
        iss >> kind;
        if (kind == "dungeon")
        {
            Dungeon d;
            if (!(iss >> d.map >> d.key))
            {
                std::fprintf(stderr, "ERROR: %s:%d: dungeon needs <map_id> <key>\n", path.c_str(), lineNo);
                return false;
            }
            d.name = RestOfLine(iss);
            out.push_back(std::move(d));
            continue;
        }
        if (out.empty())
        {
            std::fprintf(stderr, "ERROR: %s:%d: '%s' before any dungeon line\n", path.c_str(), lineNo, kind.c_str());
            return false;
        }
        Dungeon& d = out.back();
        Stop s;
        if (kind == "start")
        {
            if (!d.stops.empty())
            {
                std::fprintf(stderr, "ERROR: %s:%d: second start for %s\n", path.c_str(), lineNo, d.key.c_str());
                return false;
            }
            if (!(iss >> s.pos.x >> s.pos.y >> s.pos.z))
            {
                std::fprintf(stderr, "ERROR: %s:%d: start needs <x> <y> <z>\n", path.c_str(), lineNo);
                return false;
            }
            s.name = RestOfLine(iss);
            if (s.name.empty())
                s.name = "entrance";
        }
        else if (kind == "boss")
        {
            if (d.stops.empty())
            {
                std::fprintf(stderr, "ERROR: %s:%d: boss before the start of %s\n", path.c_str(), lineNo, d.key.c_str());
                return false;
            }
            s.is_boss = true;
            if (!(iss >> s.entry >> s.pos.x >> s.pos.y >> s.pos.z))
            {
                std::fprintf(stderr, "ERROR: %s:%d: boss needs <entry> <x> <y> <z>\n", path.c_str(), lineNo);
                return false;
            }
            s.name = RestOfLine(iss);
        }
        else if (kind == "missing")
        {
            if (!(iss >> s.entry))
            {
                std::fprintf(stderr, "ERROR: %s:%d: missing needs <entry>\n", path.c_str(), lineNo);
                return false;
            }
            s.is_boss = true;
            s.name = RestOfLine(iss);
            d.missing.push_back(std::move(s));
            continue;
        }
        else
        {
            std::fprintf(stderr, "ERROR: %s:%d: unknown line '%s'\n", path.c_str(), lineNo, kind.c_str());
            return false;
        }
        d.stops.push_back(std::move(s));
    }
    return true;
}

// ---- navmesh loading ----

struct NavMeshDeleter { void operator()(dtNavMesh* m) const { dtFreeNavMesh(m); } };
struct QueryDeleter { void operator()(dtNavMeshQuery* q) const { dtFreeNavMeshQuery(q); } };

struct LoadedMap
{
    std::unique_ptr<dtNavMesh, NavMeshDeleter> mesh;
    std::unique_ptr<dtNavMeshQuery, QueryDeleter> query;
    int tiles = 0;
};

bool LoadMap(std::string const& dir, uint32_t mapId, LoadedMap& out)
{
    char name[32];
    std::snprintf(name, sizeof(name), "%04u.mmap", mapId);
    std::string const mmapPath = (std::filesystem::path(dir) / name).string();
    FILE* f = std::fopen(mmapPath.c_str(), "rb");
    if (!f)
    {
        std::fprintf(stderr, "ERROR: cannot open %s\n", mmapPath.c_str());
        return false;
    }
    MmapNavMeshHeader header{};
    bool const headerOk = std::fread(&header, sizeof(header), 1, f) == 1;
    std::fclose(f);
    if (!headerOk || header.mmapMagic != MMAP_MAGIC || header.mmapVersion != MMAP_VERSION_EXPECTED)
    {
        std::fprintf(stderr, "ERROR: %s is not a version %u mmap\n", mmapPath.c_str(), MMAP_VERSION_EXPECTED);
        return false;
    }

    out.mesh.reset(dtAllocNavMesh());
    if (!out.mesh || dtStatusFailed(out.mesh->init(&header.params)))
    {
        std::fprintf(stderr, "ERROR: dtNavMesh init failed for map %u\n", mapId);
        return false;
    }

    char prefix[16];
    std::snprintf(prefix, sizeof(prefix), "%04u_", mapId);
    std::regex const re(R"((\d{4})_(\d{2})_(\d{2})\.mmtile)");
    for (auto const& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file())
            continue;
        std::string const file = entry.path().filename().string();
        std::smatch m;
        if (!std::regex_match(file, m, re) || std::stoul(m[1].str()) != mapId)
            continue;
        FILE* t = std::fopen(entry.path().string().c_str(), "rb");
        if (!t)
            continue;
        MmapTileHeader th{};
        if (std::fread(&th, sizeof(th), 1, t) != 1 || th.mmapMagic != MMAP_MAGIC || th.mmapVersion != MMAP_VERSION_EXPECTED)
        {
            std::fprintf(stderr, "WARN: skipping %s (bad header)\n", file.c_str());
            std::fclose(t);
            continue;
        }
        void* data = dtAlloc(th.size, DT_ALLOC_PERM);
        if (!data || std::fread(data, th.size, 1, t) != 1)
        {
            std::fprintf(stderr, "WARN: skipping %s (short read)\n", file.c_str());
            dtFree(data);
            std::fclose(t);
            continue;
        }
        std::fclose(t);
        if (dtStatusFailed(out.mesh->addTile(static_cast<unsigned char*>(data), th.size, DT_TILE_FREE_DATA, 0, nullptr)))
        {
            std::fprintf(stderr, "WARN: addTile failed for %s\n", file.c_str());
            dtFree(data);
            continue;
        }
        ++out.tiles;
    }
    if (out.tiles == 0)
    {
        std::fprintf(stderr, "ERROR: no mmtile for map %u in %s\n", mapId, dir.c_str());
        return false;
    }

    out.query.reset(dtAllocNavMeshQuery());
    if (!out.query || dtStatusFailed(out.query->init(out.mesh.get(), 65535)))
    {
        std::fprintf(stderr, "ERROR: dtNavMeshQuery init failed for map %u\n", mapId);
        return false;
    }
    return true;
}

// ---- the walk ----

dtQueryFilter PlayerFilter()
{
    dtQueryFilter filter;
    filter.setIncludeFlags(NAV_GROUND | NAV_GROUND_STEEP | NAV_WATER | NAV_MAGMA_SLIME | NAV_ROAD);
    filter.setExcludeFlags(0);
    return filter;
}

struct Snap
{
    dtPolyRef ref = 0;
    Point on;           // the closest point on that polygon
    float off = 0.f;    // how far the spot is from it
};

// The polygon under a spot: PathGenerator's box first (3 y around, 5 y up and down), then a wider one so a boss standing on a pedestal or
// a little off the floor still has its floor named (the line then says how far off it is).
Snap SnapPoint(dtNavMeshQuery const& q, dtQueryFilter const& filter, Point const& p)
{
    float const boxes[][3] = { { 3.f, 5.f, 3.f }, { 8.f, 12.f, 8.f } };
    float pd[3];
    ToDetour(p, pd);
    for (auto const& box : boxes)
    {
        Snap s;
        float closest[3];
        if (dtStatusSucceed(q.findNearestPoly(pd, box, &filter, &s.ref, closest)) && s.ref)
        {
            s.on = FromDetour(closest);
            s.off = Dist3(p, s.on);
            return s;
        }
    }
    return {};
}

// Every polygon the filter lets a walker reach from `from`, following Detour's links (one-way off-mesh links only from their start side).
std::unordered_set<dtPolyRef> Flood(dtNavMesh const& mesh, dtQueryFilter const& filter, dtPolyRef from)
{
    std::unordered_set<dtPolyRef> seen;
    std::deque<dtPolyRef> open;
    seen.insert(from);
    open.push_back(from);
    while (!open.empty())
    {
        dtPolyRef const ref = open.front();
        open.pop_front();
        dtMeshTile const* tile = nullptr;
        dtPoly const* poly = nullptr;
        if (dtStatusFailed(mesh.getTileAndPolyByRef(ref, &tile, &poly)))
            continue;
        for (unsigned int i = poly->firstLink; i != DT_NULL_LINK; i = tile->links[i].next)
        {
            dtPolyRef const nei = tile->links[i].ref;
            if (!nei || seen.count(nei))
                continue;
            dtMeshTile const* nt = nullptr;
            dtPoly const* np = nullptr;
            if (dtStatusFailed(mesh.getTileAndPolyByRef(nei, &nt, &np)) || !filter.passFilter(nei, nt, np))
                continue;
            seen.insert(nei);
            open.push_back(nei);
        }
    }
    return seen;
}

Point PolyCenter(dtNavMesh const& mesh, dtPolyRef ref)
{
    dtMeshTile const* tile = nullptr;
    dtPoly const* poly = nullptr;
    mesh.getTileAndPolyByRef(ref, &tile, &poly);
    float c[3] = { 0, 0, 0 };
    for (int i = 0; i < poly->vertCount; ++i)
        dtVadd(c, c, &tile->verts[poly->verts[i] * 3]);
    if (poly->vertCount)
        dtVscale(c, c, 1.0f / poly->vertCount);
    return FromDetour(c);
}

struct Walk
{
    bool found = false;      // findPath ended on the target polygon
    float yards = 0.f;       // length of the string-pulled path
    int polys = 0;
    bool wet = false;        // goes through water
    bool lava = false;       // goes through magma or slime
};

Walk WalkPath(dtNavMesh const& mesh, dtNavMeshQuery& q, dtQueryFilter const& filter, Snap const& a, Snap const& b)
{
    Walk w;
    constexpr int kMaxPolys = 8192;
    std::vector<dtPolyRef> polys(kMaxPolys);
    float pa[3], pb[3];
    ToDetour(a.on, pa);
    ToDetour(b.on, pb);
    dtStatus const s = q.findPath(a.ref, b.ref, pa, pb, &filter, polys.data(), &w.polys, kMaxPolys);
    if (dtStatusFailed(s) || w.polys == 0)
        return w;
    w.found = polys[w.polys - 1] == b.ref && !(s & DT_PARTIAL_RESULT);
    for (int i = 0; i < w.polys; ++i)
    {
        unsigned short flags = 0;
        if (dtStatusSucceed(mesh.getPolyFlags(polys[i], &flags)))
        {
            w.wet |= (flags & NAV_WATER) != 0;
            w.lava |= (flags & NAV_MAGMA_SLIME) != 0;
        }
    }
    std::vector<float> pts(kMaxPolys * 3);
    int n = 0;
    if (dtStatusSucceed(q.findStraightPath(pa, pb, polys.data(), w.polys, pts.data(), nullptr, nullptr, &n, kMaxPolys, 0)))
        for (int i = 0; i + 1 < n; ++i)
            w.yards += dtVdist(&pts[i * 3], &pts[(i + 1) * 3]);
    return w;
}

// The reachable floor nearest a spot: the closest point on any polygon of `reach`.
float NearestReachable(dtNavMesh const& mesh, dtNavMeshQuery const& q, std::unordered_set<dtPolyRef> const& reach, Point const& p, Point& at)
{
    float pd[3];
    ToDetour(p, pd);
    float best = 1e30f;
    for (dtPolyRef ref : reach)
    {
        // Cheap reject on the polygon's middle before the exact closest point.
        Point const c = PolyCenter(mesh, ref);
        if (Dist3(c, p) > best + 60.f)
            continue;
        float closest[3];
        if (dtStatusFailed(q.closestPointOnPoly(ref, pd, closest, nullptr)))
            continue;
        Point const cp = FromDetour(closest);
        float const d = Dist3(cp, p);
        if (d < best)
        {
            best = d;
            at = cp;
        }
    }
    return best;
}

// The narrowest place between a piece of navmesh and the walkable floor: of the piece's polygons, the one whose middle comes closest to a
// walkable polygon. That is where an off-mesh link or a mesh fix would join the two.
struct Gap
{
    float yards = 1e30f;
    Point on_piece, on_floor;
};

Gap GapBetween(dtNavMesh const& mesh, dtNavMeshQuery const& q, std::vector<dtPolyRef> const& piece, std::unordered_set<dtPolyRef> const& floor)
{
    Gap g;
    for (dtPolyRef r : piece)
    {
        Point const c = PolyCenter(mesh, r);
        Point at;
        if (float const d = NearestReachable(mesh, q, floor, c, at); d < g.yards)
        {
            g.yards = d;
            g.on_piece = c;
            g.on_floor = at;
        }
    }
    return g;
}

struct Totals
{
    int dungeons = 0, bosses = 0, reached = 0, failed = 0, warnings = 0, skipped = 0, unplaced = 0;
};

// The other pieces of navmesh in the dungeon: pieces the entrance cannot walk to that come within 60 y (and 10 y of height) of the entrance
// or a boss. Each line says where the piece is, how big, which stop it is near, and how wide the gap to the walkable floor is.
void ListIslands(dtNavMesh const& mesh, dtNavMeshQuery const& q, dtQueryFilter const& filter, std::unordered_set<dtPolyRef> const& main,
    std::vector<Stop> const& stops)
{
    std::unordered_set<dtPolyRef> done(main.begin(), main.end());
    struct Piece
    {
        std::vector<dtPolyRef> polys;
        Point mid, lo, hi;
        Stop const* near = nullptr;
        float near_d = 1e30f;
    };
    auto nearStop = [&](Point const& c, float& d) -> Stop const*
    {
        Stop const* best = nullptr;
        d = 1e30f;
        for (Stop const& s : stops)
        {
            float const dx = c.x - s.pos.x, dy = c.y - s.pos.y;
            float const flat = std::sqrt(dx * dx + dy * dy);
            if (flat <= 60.f && std::fabs(c.z - s.pos.z) <= 10.f && flat < d)
            {
                d = flat;
                best = &s;
            }
        }
        return best;
    };
    std::vector<Piece> pieces;
    for (int t = 0; t < mesh.getMaxTiles(); ++t)
    {
        dtMeshTile const* tile = mesh.getTile(t);
        if (!tile || !tile->header)
            continue;
        dtPolyRef const base = mesh.getPolyRefBase(tile);
        for (int i = 0; i < tile->header->polyCount; ++i)
        {
            dtPolyRef const ref = base | static_cast<dtPolyRef>(i);
            dtPoly const* poly = &tile->polys[i];
            if (done.count(ref) || poly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION || !filter.passFilter(ref, tile, poly))
                continue;
            Piece pc;
            pc.lo = { 1e9f, 1e9f, 1e9f };
            pc.hi = { -1e9f, -1e9f, -1e9f };
            double sx = 0, sy = 0, sz = 0;
            for (dtPolyRef r : Flood(mesh, filter, ref))
            {
                if (!done.insert(r).second)
                    continue;
                Point const c = PolyCenter(mesh, r);
                sx += c.x; sy += c.y; sz += c.z;
                pc.lo = { std::min(pc.lo.x, c.x), std::min(pc.lo.y, c.y), std::min(pc.lo.z, c.z) };
                pc.hi = { std::max(pc.hi.x, c.x), std::max(pc.hi.y, c.y), std::max(pc.hi.z, c.z) };
                pc.polys.push_back(r);
                float d;
                if (Stop const* s = nearStop(c, d); s && d < pc.near_d)
                {
                    pc.near_d = d;
                    pc.near = s;
                }
            }
            if (pc.near && pc.polys.size() >= 5)
            {
                float const n = float(pc.polys.size());
                pc.mid = { float(sx / n), float(sy / n), float(sz / n) };
                pieces.push_back(std::move(pc));
            }
        }
    }
    std::sort(pieces.begin(), pieces.end(), [](Piece const& a, Piece const& b) { return a.polys.size() > b.polys.size(); });
    std::printf("  islands: %zu piece(s) of navmesh near the entrance or a boss that the entrance cannot walk to\n", pieces.size());
    for (Piece const& pc : pieces)
    {
        Gap const gap = GapBetween(mesh, q, pc.polys, main);
        std::printf("    island %5zu polys  middle (%.1f, %.1f, %.1f)  box x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f  %.0f y from %s; "
            "gap %.1f y between (%.1f, %.1f, %.1f) on it and (%.1f, %.1f, %.1f) on the walkable floor\n",
            pc.polys.size(), pc.mid.x, pc.mid.y, pc.mid.z, pc.lo.x, pc.hi.x, pc.lo.y, pc.hi.y, pc.lo.z, pc.hi.z,
            pc.near_d, pc.near->name.c_str(), gap.yards, gap.on_piece.x, gap.on_piece.y, gap.on_piece.z, gap.on_floor.x, gap.on_floor.y,
            gap.on_floor.z);
    }
}

void CheckDungeon(Dungeon const& d, LoadedMap& m, bool islands, Totals& tot)
{
    ++tot.dungeons;
    dtQueryFilter const filter = PlayerFilter();
    std::printf("\n== %s (map %u) %s  [%d tiles]\n", d.key.c_str(), d.map, d.name.c_str(), m.tiles);
    if (d.stops.empty())
    {
        std::printf("  no start: skipped\n");
        return;
    }

    Stop const& start = d.stops[0];
    Snap const s0 = SnapPoint(*m.query, filter, start.pos);
    if (!s0.ref)
    {
        std::printf("FAIL  %-12s start %s (%.1f, %.1f, %.1f): no navmesh under the entrance; nothing on this map can be checked\n",
            d.key.c_str(), start.name.c_str(), start.pos.x, start.pos.y, start.pos.z);
        for (size_t i = 1; i < d.stops.size(); ++i)
        {
            ++tot.bosses;
            ++tot.failed;
        }
        return;
    }
    std::unordered_set<dtPolyRef> const fromEntrance = Flood(*m.mesh, filter, s0.ref);
    std::printf("  entrance %s snapped %.1f y, walkable from it: %zu polygons\n", start.name.c_str(), s0.off, fromEntrance.size());
    if (fromEntrance.size() < kSmallPieceForEntrance)
    {
        ++tot.warnings;
        std::printf("WARN  %-12s the entrance stands on a small piece of navmesh (%zu polygons): the party may be boxed in where it lands\n",
            d.key.c_str(), fromEntrance.size());
    }
    for (Stop const& b : d.missing)
    {
        ++tot.unplaced;
        std::printf("SKIP  %-12s %s (%u): no spot known (summoned or no spawn row), not checked\n", d.key.c_str(), b.name.c_str(), b.entry);
    }

    // The leg starts at the last stop the bots could reach, so one bad boss does not fail every boss after it.
    Snap from = s0;
    std::string fromName = start.name;
    std::unordered_set<dtPolyRef> reach = fromEntrance;
    for (size_t i = 1; i < d.stops.size(); ++i)
    {
        Stop const& b = d.stops[i];
        ++tot.bosses;
        Snap const sb = SnapPoint(*m.query, filter, b.pos);
        char head[160];
        std::snprintf(head, sizeof(head), "%-12s %s (%u) at (%.1f, %.1f, %.1f)", d.key.c_str(), b.name.c_str(), b.entry, b.pos.x, b.pos.y, b.pos.z);
        if (!sb.ref)
        {
            ++tot.failed;
            Point at;
            float const near = NearestReachable(*m.mesh, *m.query, reach, b.pos, at);
            std::printf("FAIL  %s: no navmesh within 8 y of the boss; nearest floor walkable from %s is %.1f y away at (%.1f, %.1f, %.1f)\n",
                head, fromName.c_str(), near, at.x, at.y, at.z);
            continue;
        }
        bool const reachable = reach.count(sb.ref) != 0;
        if (!reachable)
        {
            Point at;
            float const near = NearestReachable(*m.mesh, *m.query, reach, b.pos, at);
            std::unordered_set<dtPolyRef> const island = Flood(*m.mesh, filter, sb.ref);
            if (near <= kFightReachYards)
            {
                // A boss on a pedestal or a ledge of its own (The Abandoned on its statue): the bots fight it from the floor next to it.
                ++tot.reached;
                std::printf("OK    %s: stands on its own piece of %zu polygons, the floor walkable from %s is %.1f y from it at (%.1f, %.1f, %.1f)\n",
                    head, island.size(), fromName.c_str(), near, at.x, at.y, at.z);
                continue;
            }
            ++tot.failed;
            bool const toEntrance = island.count(s0.ref) != 0;
            bool const fromEntranceOk = fromEntrance.count(sb.ref) != 0;
            Gap const gap = GapBetween(*m.mesh, *m.query, std::vector<dtPolyRef>(island.begin(), island.end()), reach);
            std::printf("FAIL  %s: cannot walk there from %s. The boss floor is a piece of %zu polygons%s%s; the walkable floor comes %.1f y from the "
                "boss at (%.1f, %.1f, %.1f); narrowest gap %.1f y between (%.1f, %.1f, %.1f) on the boss floor and (%.1f, %.1f, %.1f)\n",
                head, fromName.c_str(), island.size(),
                fromEntranceOk ? " (the entrance does reach it: the gap is between the bosses)" : "",
                toEntrance ? " (from it the entrance can be walked to: a one-way drop the other way)" : "",
                near, at.x, at.y, at.z, gap.yards, gap.on_piece.x, gap.on_piece.y, gap.on_piece.z, gap.on_floor.x, gap.on_floor.y, gap.on_floor.z);
            continue;
        }

        Walk const w = WalkPath(*m.mesh, *m.query, filter, from, sb);
        std::unordered_set<dtPolyRef> const fromBoss = Flood(*m.mesh, filter, sb.ref);
        bool const back = fromBoss.count(from.ref) != 0;
        ++tot.reached;
        std::string notes;
        if (sb.off > 3.f)
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "; boss stands %.1f y off the navmesh", sb.off);
            notes += buf;
        }
        if (!w.found)
            notes += "; Detour could not string a full path (search too big), the floor is joined though";
        if (w.wet)
            notes += "; the way goes through water";
        if (w.lava)
            notes += "; the way goes through lava or slime";
        if (!back)
        {
            notes += "; ONE WAY: no walk back to the previous stop";
            ++tot.warnings;
        }
        std::printf("OK    %s: from %s %.0f y walk, %d polygons%s\n", head, fromName.c_str(), w.yards, w.polys, notes.c_str());

        from = sb;
        fromName = b.name;
        reach = fromBoss;
    }

    if (islands)
    {
        // An island near a boss is the first thing to look at in game, so list them, measured from the entrance.
        ListIslands(*m.mesh, *m.query, filter, fromEntrance, d.stops);
    }
}
} // anonymous namespace

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "Usage: %s <mmaps_dir> <list.txt> [--islands] [--map <id>]\n", argv[0]);
        return 2;
    }
    std::string const dir = argv[1];
    std::string const listPath = argv[2];
    bool islands = false;
    std::set<uint32_t> onlyMaps;
    for (int i = 3; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--islands") == 0)
            islands = true;
        else if (std::strcmp(argv[i], "--map") == 0 && i + 1 < argc)
            onlyMaps.insert(static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10)));
        else
        {
            std::fprintf(stderr, "Unknown argument %s\n", argv[i]);
            return 2;
        }
    }

    std::vector<Dungeon> dungeons;
    if (!ReadList(listPath, dungeons))
        return 2;

    std::printf("dungeon_reach_check: mmaps %s, list %s, %zu dungeon(s)\n", dir.c_str(), listPath.c_str(), dungeons.size());
    std::map<uint32_t, LoadedMap> maps;
    std::vector<std::string> missing;
    Totals tot;
    for (Dungeon const& d : dungeons)
    {
        if (!onlyMaps.empty() && !onlyMaps.count(d.map))
            continue;
        auto it = maps.find(d.map);
        if (it == maps.end())
        {
            LoadedMap m;
            if (!LoadMap(dir, d.map, m))
            {
                missing.push_back(d.key);
                tot.skipped += int(d.stops.size() > 0 ? d.stops.size() - 1 : 0);
                continue;
            }
            it = maps.emplace(d.map, std::move(m)).first;
        }
        CheckDungeon(d, it->second, islands, tot);
    }

    std::printf("\nSUMMARY: %d dungeon(s), %d boss(es): %d reached, %d cannot be reached, %d warning(s), %d with no spot",
        tot.dungeons, tot.bosses, tot.reached, tot.failed, tot.warnings, tot.unplaced);
    if (!missing.empty())
    {
        std::printf("; %d boss(es) not checked, no mmaps for:", tot.skipped);
        for (std::string const& k : missing)
            std::printf(" %s", k.c_str());
    }
    std::printf("\n");
    if (tot.failed)
        return 1;
    return missing.empty() ? 0 : 2;
}
