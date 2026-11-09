/*
 * walk_link_check - offline check of candidate walk links for the bots' travel graph.
 *
 * Reads the "[walk_gap_pair]" lines the worldserver writes at startup (Playerbot.log), loads the mmaps of each map named there, and runs
 * Detour findPath between the two places of every pair with the same filter the movement uses (ground, steep ground, road; no water). A pair
 * is a link only when the path is complete (not partial, ends on the target polygon) and has no cliff face (a drop of 15 y or more over no
 * more ground than the drop). The checked links are printed as a table, one line each with the path length.
 *
 * Usage: walk_link_check <mmaps_dir> <Playerbot.log> [links_out.txt]
 *        walk_link_check --road-census <mmaps_dir> <map_id> [road_polys_out.txt]
 *        walk_link_check --road-trace <mmaps_dir> <map_id> <x1> <y1> <x2> <y2> [road_rows_out.sql]
 *
 * Pure Detour API, like mmap_probe: no TC core, no worldserver needed. The mmaps must be the ones the server runs on.
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
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

// Header layout copied verbatim from src/common/mmaps_common/MMapDefines.h.
// MUST stay byte-for-byte identical with the generator output.
// Header layout copied verbatim from src/common/mmaps_common/MMapDefines.h.
// MUST stay byte-for-byte identical with the generator output.
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

// Convert a TC area id to the bitmask Detour's filter expects (matches the
// formula in MMapDefines.h NavTerrainFlag). The probe enables WALK on the
// usual surfaces (ground, ground steep, water, road). Off-mesh links and
// magma/slime intentionally left off — same as PathGenerator's default
// "land creature" filter.
static constexpr uint16_t NAV_GROUND       = 1u << (11 - 11); // 0x01
static constexpr uint16_t NAV_GROUND_STEEP = 1u << (11 - 10); // 0x02
static constexpr uint16_t NAV_WATER        = 1u << (11 - 9);  // 0x04
// NAV_MAGMA_SLIME = 0x08
static constexpr uint16_t NAV_ROAD         = 1u << (11 - 7);  // 0x10
static constexpr int      NAV_AREA_ROAD    = 7;               // area id of a road polygon (MMapDefines.h)

namespace
{
std::string FormatMmapPath(std::string const& dir, uint32_t mapId)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04u.mmap", mapId);
    return (std::filesystem::path(dir) / buf).string();
}

bool LoadNavMeshParams(std::string const& dir, uint32_t mapId, dtNavMeshParams& params)
{
    std::string path = FormatMmapPath(dir, mapId);
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
    {
        std::fprintf(stderr, "ERROR: cannot open %s\n", path.c_str());
        return false;
    }
    MmapNavMeshHeader header{};
    if (std::fread(&header, sizeof(header), 1, f) != 1)
    {
        std::fprintf(stderr, "ERROR: short read of %s\n", path.c_str());
        std::fclose(f);
        return false;
    }
    std::fclose(f);

    if (header.mmapMagic != MMAP_MAGIC)
    {
        std::fprintf(stderr, "ERROR: %s magic mismatch (got 0x%08x, want 0x%08x)\n",
            path.c_str(), header.mmapMagic, MMAP_MAGIC);
        return false;
    }
    if (header.mmapVersion != MMAP_VERSION_EXPECTED)
    {
        std::fprintf(stderr, "ERROR: %s version mismatch (got %u, want %u)\n",
            path.c_str(), header.mmapVersion, MMAP_VERSION_EXPECTED);
        return false;
    }
    params = header.params;
    return true;
}

struct LoadedTile
{
    int x;
    int y;
};

// Add one .mmtile file to the dtNavMesh. Returns true on success.
bool AddTileFromFile(dtNavMesh* navMesh, std::string const& path, int tx, int ty)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
    {
        std::fprintf(stderr, "WARN: cannot open %s\n", path.c_str());
        return false;
    }

    MmapTileHeader header{};
    if (std::fread(&header, sizeof(header), 1, f) != 1)
    {
        std::fprintf(stderr, "WARN: short header read %s\n", path.c_str());
        std::fclose(f);
        return false;
    }
    if (header.mmapMagic != MMAP_MAGIC || header.mmapVersion != MMAP_VERSION_EXPECTED)
    {
        std::fprintf(stderr, "WARN: bad header for %s (magic=0x%08x ver=%u)\n",
            path.c_str(), header.mmapMagic, header.mmapVersion);
        std::fclose(f);
        return false;
    }

    void* data = dtAlloc(header.size, DT_ALLOC_PERM);
    if (!data)
    {
        std::fclose(f);
        return false;
    }
    if (std::fread(data, header.size, 1, f) != 1)
    {
        std::fprintf(stderr, "WARN: short data read %s\n", path.c_str());
        dtFree(data);
        std::fclose(f);
        return false;
    }
    std::fclose(f);

    dtTileRef tileRef = 0;
    dtStatus s = navMesh->addTile(static_cast<unsigned char*>(data), header.size, DT_TILE_FREE_DATA, 0, &tileRef);
    if (dtStatusFailed(s))
    {
        std::fprintf(stderr, "WARN: addTile failed for [%d,%d] status=0x%08x\n", tx, ty, s);
        dtFree(data);
        return false;
    }
    return true;
}

int LoadAllTiles(dtNavMesh* navMesh, std::string const& dir, uint32_t mapId, std::vector<LoadedTile>& loaded)
{
    // Pattern: 0001_XX_YY.mmtile
    char prefix[16];
    std::snprintf(prefix, sizeof(prefix), "%04u_", mapId);
    std::regex re(R"((\d{4})_(\d{2})_(\d{2})\.mmtile)");

    int n = 0;
    for (auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file())
            continue;
        std::string name = entry.path().filename().string();
        std::smatch m;
        if (!std::regex_match(name, m, re))
            continue;
        if (std::stoul(m[1].str()) != mapId)
            continue;
        int tx = std::stoi(m[2].str());
        int ty = std::stoi(m[3].str());
        if (AddTileFromFile(navMesh, entry.path().string(), tx, ty))
        {
            loaded.push_back({ tx, ty });
            ++n;
        }
    }
    return n;
}

} // namespace

namespace
{
constexpr int kMaxPolys = 4096;            // a 1,100 y walk can cross some hundreds of polygons
constexpr float kMaxPathYards = 4000.0f;   // a complete path longer than this is not a link (its real length is the edge cost, so a long way round is fair below it)
constexpr float kCliffDropYards = 15.0f;   // mirrors Fleet::kCliffDropYards (CityLayer.h)

struct Pair
{
    uint32_t map = 0;
    float ax = 0, ay = 0, az = 0;
    float bx = 0, by = 0, bz = 0;
    float d = 0;
};

bool Parse(std::string const& line, Pair& p)
{
    static std::regex const re(R"(\[walk_gap_pair\] map=(\d+) a=\d+ ax=(-?[\d.]+) ay=(-?[\d.]+) az=(-?[\d.]+) b=\d+ bx=(-?[\d.]+) by=(-?[\d.]+) bz=(-?[\d.]+) d=(\d+))");
    std::smatch m;
    if (!std::regex_search(line, m, re))
        return false;
    p.map = uint32_t(std::stoul(m[1]));
    p.ax = std::stof(m[2]); p.ay = std::stof(m[3]); p.az = std::stof(m[4]);
    p.bx = std::stof(m[5]); p.by = std::stof(m[6]); p.bz = std::stof(m[7]);
    p.d = std::stof(m[8]);
    return true;
}

// A stuck bot from a "[wedge]" line: where it stood and the (x, y) goal it was walking to (the goal's height is not logged).
struct Wedge
{
    uint32_t map = 0;
    float x = 0, y = 0, z = 0;
    float gx = 0, gy = 0;
    std::string bot;
    std::string cat;
};

bool ParseWedge(std::string const& line, Wedge& w)
{
    static std::regex const re(R"(\[wedge\] bot=(\S+) cat=(\S+) map=(\d+) zone=\d+ pos=\((-?[\d.]+),(-?[\d.]+),(-?[\d.]+)\) dur=\d+s obj=(?:\S+ )*?(?:move_goal|quest_goal)@\((-?[\d.]+),(-?[\d.]+)\))");
    std::smatch m;
    if (!std::regex_search(line, m, re))
        return false;
    w.bot = m[1]; w.cat = m[2];
    w.map = uint32_t(std::stoul(m[3]));
    w.x = std::stof(m[4]); w.y = std::stof(m[5]); w.z = std::stof(m[6]);
    w.gx = std::stof(m[7]); w.gy = std::stof(m[8]);
    return true;
}

struct Verdict
{
    char const* status = "";
    float length = 0.f;
    bool ok = false;
};

// TC (x, y, z) -> Detour (y, z, x).
Verdict Check(dtNavMeshQuery* query, Pair const& p)
{
    Verdict v;
    dtQueryFilter filter;
    filter.setIncludeFlags(NAV_GROUND | NAV_GROUND_STEEP | NAV_ROAD);
    filter.setExcludeFlags(0);
    float const ext[3] = { 4.0f, 12.0f, 4.0f };
    float a[3] = { p.ay, p.az, p.ax }, b[3] = { p.by, p.bz, p.bx };
    dtPolyRef ra = 0, rb = 0;
    float ca[3] = {}, cb[3] = {};
    query->findNearestPoly(a, ext, &filter, &ra, ca);
    query->findNearestPoly(b, ext, &filter, &rb, cb);
    if (!ra || !rb) { v.status = "NO_POLY"; return v; }
    static dtPolyRef polys[kMaxPolys];
    int n = 0;
    dtStatus s = query->findPath(ra, rb, ca, cb, &filter, polys, &n, kMaxPolys);
    if (dtStatusFailed(s) || n == 0) { v.status = "NOPATH"; return v; }
    if ((s & DT_PARTIAL_RESULT) || polys[n - 1] != rb) { v.status = "PARTIAL"; return v; }
    static float pts[kMaxPolys * 3];
    static unsigned char flags[kMaxPolys];
    static dtPolyRef refs[kMaxPolys];
    int np = 0;
    s = query->findStraightPath(ca, cb, polys, n, pts, flags, refs, &np, kMaxPolys, 0);
    if (dtStatusFailed(s) || np < 2) { v.status = "NOPATH"; return v; }
    float len = 0.f;
    bool cliff = false;
    for (int i = 1; i < np; ++i)
    {
        float const dx = pts[i * 3 + 2] - pts[(i - 1) * 3 + 2], dy = pts[i * 3 + 0] - pts[(i - 1) * 3 + 0];
        float const dz = pts[i * 3 + 1] - pts[(i - 1) * 3 + 1];
        float const run = std::hypot(dx, dy);
        len += std::sqrt(run * run + dz * dz);
        if (std::fabs(dz) >= kCliffDropYards && run <= std::fabs(dz))
            cliff = true;
    }
    v.length = len;
    if (cliff) { v.status = "CLIFF"; return v; }
    if (len > kMaxPathYards) { v.status = "LONG"; return v; }
    v.status = "OK";
    v.ok = true;
    return v;
}
// Path from where a bot got stuck to its goal on the real mesh. A partial path says where the walkable ground ends (the barrier).
// The goal's height is unknown, so the polygon nearest the goal in x, y (within 300 y of the bot's height) is used.
void CheckWedge(dtNavMeshQuery* query, Wedge const& w)
{
    dtQueryFilter filter;
    filter.setIncludeFlags(NAV_GROUND | NAV_GROUND_STEEP | NAV_ROAD);
    filter.setExcludeFlags(0);
    float const ext_start[3] = { 6.0f, 20.0f, 6.0f }, ext_goal[3] = { 6.0f, 300.0f, 6.0f };
    float a[3] = { w.y, w.z, w.x }, b[3] = { w.gy, w.z, w.gx };
    dtPolyRef ra = 0, rb = 0;
    float ca[3] = {}, cb[3] = {};
    query->findNearestPoly(a, ext_start, &filter, &ra, ca);
    query->findNearestPoly(b, ext_goal, &filter, &rb, cb);
    char const* status = "OK";
    float end_x = 0, end_y = 0, end_z = 0, gap = 0;
    if (!ra) status = "START_OFF_MESH";
    else if (!rb) status = "GOAL_OFF_MESH";
    else
    {
        static dtPolyRef polys[kMaxPolys];
        int n = 0;
        dtStatus const s = query->findPath(ra, rb, ca, cb, &filter, polys, &n, kMaxPolys);
        if (dtStatusFailed(s) || n == 0) status = "NOPATH";
        else if ((s & DT_PARTIAL_RESULT) || polys[n - 1] != rb)
        {
            status = "PARTIAL";
            float last[3] = {};
            bool over = false;
            if (dtStatusSucceed(query->closestPointOnPoly(polys[n - 1], cb, last, &over)))
            {
                end_x = last[2]; end_y = last[0]; end_z = last[1];
                gap = std::hypot(end_x - w.gx, end_y - w.gy);
            }
        }
    }
    std::printf("  %-14s map %u %s [%s] (%.0f,%.0f,%.0f) -> (%.0f,%.0f) goal poly z %.0f", status, w.map, w.bot.c_str(), w.cat.c_str(), w.x, w.y, w.z, w.gx, w.gy, cb[1]);
    if (std::strcmp(status, "PARTIAL") == 0)
        std::printf("; walkable ground ends at (%.0f,%.0f,%.0f), %.0f y short of the goal", end_x, end_y, end_z, gap);
    std::printf("\n");
}
// --road-census: how many polygons of each area a map's mmaps carry, and (with an output file) one line per road polygon (TC x y z of its
// centre). Tells whether the mmaps hold road data at all before anything is built on it.
int RoadCensus(std::string const& dir, uint32_t map_id, char const* out_path)
{
    dtNavMeshParams params{};
    if (!LoadNavMeshParams(dir, map_id, params))
        return 2;
    dtNavMesh* mesh = dtAllocNavMesh();
    if (!mesh || dtStatusFailed(mesh->init(&params)))
        return 2;
    std::vector<LoadedTile> loaded;
    int const tiles = LoadAllTiles(mesh, dir, map_id, loaded);
    std::map<int, size_t> by_area;
    size_t road_tiles = 0;
    std::ofstream out;
    if (out_path)
        out.open(out_path);
    for (int i = 0; i < mesh->getMaxTiles(); ++i)
    {
        dtMeshTile const* t = static_cast<dtNavMesh const*>(mesh)->getTile(i);
        if (!t || !t->header)
            continue;
        size_t roads = 0;
        for (int k = 0; k < t->header->polyCount; ++k)
        {
            dtPoly const& poly = t->polys[k];
            if (poly.getType() != DT_POLYTYPE_GROUND)
                continue;
            ++by_area[poly.getArea()];
            if (poly.getArea() != NAV_AREA_ROAD)
                continue;
            ++roads;
            if (out)
            {
                float c[3] = {};
                for (int v = 0; v < poly.vertCount; ++v)
                    for (int d = 0; d < 3; ++d)
                        c[d] += t->verts[poly.verts[v] * 3 + d] / poly.vertCount;
                out << c[2] << ' ' << c[0] << ' ' << c[1] << '\n';   // Detour (y, z, x) -> TC x y z
            }
        }
        road_tiles += roads > 0;
    }
    std::printf("map %u: %d tile(s)\n", map_id, tiles);
    for (auto& [area, n] : by_area)
        std::printf("  area %d%s: %zu polygon(s)\n", area, area == NAV_AREA_ROAD ? " (ROAD)" : "", n);
    std::printf("  tiles with road polygons: %zu\n", road_tiles);
    dtFreeNavMesh(mesh);
    return 0;
}

// --road-trace: the walk between two places on the real mesh, written as handcrafted_road rows (a centerline a bot can follow). Every
// segment lies inside walkable polygons because it comes from the straight path; a long straight run is split so the road graph gets
// nodes at least every 40 y, and each segment is raycast again and rejected (the whole trace fails) if it climbs a cliff face.
int RoadTrace(std::string const& dir, uint32_t map_id, float ax, float ay, float bx, float by, char const* out_path)
{
    dtNavMeshParams params{};
    if (!LoadNavMeshParams(dir, map_id, params))
        return 2;
    dtNavMesh* mesh = dtAllocNavMesh();
    if (!mesh || dtStatusFailed(mesh->init(&params)))
        return 2;
    std::vector<LoadedTile> loaded;
    LoadAllTiles(mesh, dir, map_id, loaded);
    dtNavMeshQuery* query = dtAllocNavMeshQuery();
    if (loaded.empty() || !query || dtStatusFailed(query->init(mesh, 65535)))
        return 2;
    dtQueryFilter filter;
    filter.setIncludeFlags(NAV_GROUND | NAV_GROUND_STEEP | NAV_ROAD);
    filter.setExcludeFlags(0);
    float const ext[3] = { 6.0f, 500.0f, 6.0f };
    float a[3] = { ay, 100.0f, ax }, b[3] = { by, 100.0f, bx };
    dtPolyRef ra = 0, rb = 0;
    float ca[3] = {}, cb[3] = {};
    query->findNearestPoly(a, ext, &filter, &ra, ca);
    query->findNearestPoly(b, ext, &filter, &rb, cb);
    if (!ra || !rb)
    {
        std::printf("TRACE NO_POLY (start %s, goal %s)\n", ra ? "ok" : "off the mesh", rb ? "ok" : "off the mesh");
        return 1;
    }
    static dtPolyRef polys[kMaxPolys];
    int n = 0;
    dtStatus s = query->findPath(ra, rb, ca, cb, &filter, polys, &n, kMaxPolys);
    if (dtStatusFailed(s) || n == 0 || (s & DT_PARTIAL_RESULT) || polys[n - 1] != rb)
    {
        std::printf("TRACE %s\n", n == 0 || dtStatusFailed(s) ? "NOPATH" : "PARTIAL");
        return 1;
    }
    static float pts[kMaxPolys * 3];
    int np = 0;
    s = query->findStraightPath(ca, cb, polys, n, pts, nullptr, nullptr, &np, kMaxPolys, 0);
    if (dtStatusFailed(s) || np < 2)
    {
        std::printf("TRACE NOPATH\n");
        return 1;
    }
    struct P { float x, y, z; };
    std::vector<P> line;   // TC x y z
    auto push = [&](float x, float y, float z) { line.push_back({ x, y, z }); };
    for (int i = 0; i < np; ++i)
    {
        P cur{ pts[i * 3 + 2], pts[i * 3 + 0], pts[i * 3 + 1] };
        if (i > 0)
        {
            P prev = line.back();
            float const d = std::hypot(cur.x - prev.x, cur.y - prev.y);
            int const parts = std::max(1, static_cast<int>(std::ceil(d / 40.0f)));
            for (int k = 1; k < parts; ++k)
            {
                float const t = static_cast<float>(k) / parts;
                push(prev.x + (cur.x - prev.x) * t, prev.y + (cur.y - prev.y) * t, prev.z + (cur.z - prev.z) * t);
            }
        }
        push(cur.x, cur.y, cur.z);
    }
    // First and last node sit exactly on the asked places, so the new line joins the existing road nodes.
    line.front().x = ax; line.front().y = ay;
    line.back().x = bx;  line.back().y = by;
    float length = 0.0f, worst_drop = 0.0f;
    int cliffs = 0;
    for (size_t i = 1; i < line.size(); ++i)
    {
        float const d = std::hypot(line[i].x - line[i - 1].x, line[i].y - line[i - 1].y);
        float const dz = line[i].z - line[i - 1].z;
        length += d;
        worst_drop = std::max(worst_drop, std::fabs(dz));
        if (std::fabs(dz) >= kCliffDropYards && d < std::fabs(dz) * 2.0f)
            ++cliffs;
    }
    std::printf("TRACE %s: %zu node(s), %.0f y long, start z %.1f goal z %.1f, steepest segment dz %.1f, %d cliff segment(s)\n",
        cliffs ? "CLIFF" : "OK", line.size(), length, line.front().z, line.back().z, worst_drop, cliffs);
    if (cliffs)
        return 1;
    if (out_path)
    {
        std::ofstream out(out_path);
        out << "INSERT INTO `handcrafted_road` (`mapId`, `fromX`, `fromY`, `toX`, `toY`, `width`, `comment`, `verified`) VALUES\n";
        for (size_t i = 1; i < line.size(); ++i)
        {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "(%u,%.2f,%.2f,%.2f,%.2f,8,'Elwynn-Westfall traced',0)%s\n", map_id, line[i - 1].x, line[i - 1].y,
                line[i].x, line[i].y, i + 1 == line.size() ? ";" : ",");
            out << buf;
        }
    }
    dtFreeNavMesh(mesh);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc >= 8 && std::string(argv[1]) == "--road-trace")
        return RoadTrace(argv[2], static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10)), std::strtof(argv[4], nullptr), std::strtof(argv[5], nullptr),
            std::strtof(argv[6], nullptr), std::strtof(argv[7], nullptr), argc > 8 ? argv[8] : nullptr);
    if (argc >= 4 && std::string(argv[1]) == "--road-census")
        return RoadCensus(argv[2], static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10)), argc > 4 ? argv[4] : nullptr);
    if (argc < 3)
    {
        std::fprintf(stderr, "Usage: %s <mmaps_dir> <Playerbot.log> [links_out.txt]\n  reads [walk_gap_pair] lines (candidate links) and [wedge] lines (where bots stood stuck and the goal they walked to)\n", argv[0]);
        return 2;
    }
    std::string const dir = argv[1];
    std::ifstream in(argv[2]);
    if (!in)
    {
        std::fprintf(stderr, "ERROR: cannot open %s\n", argv[2]);
        return 2;
    }
    std::map<uint32_t, std::vector<Pair>> by_map;
    std::map<uint32_t, std::vector<Wedge>> wedges_by_map;
    std::string line;
    size_t total = 0, total_wedges = 0;
    std::set<std::string> seen_wedges;   // one per bot and 50 y cell of the start and of the goal
    while (std::getline(in, line))
    {
        Pair p;
        Wedge w;
        if (Parse(line, p))
        {
            by_map[p.map].push_back(p);
            ++total;
        }
        else if (ParseWedge(line, w))
        {
            char key[160];
            std::snprintf(key, sizeof(key), "%u/%d/%d/%d/%d", w.map, int(std::floor(w.x / 50)), int(std::floor(w.y / 50)), int(std::floor(w.gx / 50)), int(std::floor(w.gy / 50)));
            if (seen_wedges.insert(key).second)
            {
                wedges_by_map[w.map].push_back(w);
                by_map[w.map];   // the map is loaded even when it has no pair
                ++total_wedges;
            }
        }
    }
    if (!total && !total_wedges)
    {
        std::fprintf(stderr, "ERROR: no [walk_gap_pair] or [wedge] lines in %s (start a worldserver build that writes them first)\n", argv[2]);
        return 2;
    }

    std::vector<std::string> table;
    for (auto& [map_id, pairs] : by_map)
    {
        dtNavMeshParams params{};
        if (!LoadNavMeshParams(dir, map_id, params))
            continue;
        dtNavMesh* mesh = dtAllocNavMesh();
        if (!mesh || dtStatusFailed(mesh->init(&params)))
        {
            std::fprintf(stderr, "ERROR: dtNavMesh init failed for map %u\n", map_id);
            continue;
        }
        std::vector<LoadedTile> loaded;
        int const tiles = LoadAllTiles(mesh, dir, map_id, loaded);
        std::printf("map %u: %d tile(s), %zu candidate pair(s), %zu wedge(s)\n", map_id, tiles, pairs.size(), wedges_by_map[map_id].size());
        dtNavMeshQuery* query = dtAllocNavMeshQuery();
        if (tiles == 0 || !query || dtStatusFailed(query->init(mesh, 65535)))
        {
            dtFreeNavMesh(mesh);
            continue;
        }
        for (Pair const& p : pairs)
        {
            Verdict const v = Check(query, p);
            std::printf("  %-8s map %u (%.0f,%.0f,%.0f) -> (%.0f,%.0f,%.0f) straight %.0f y, path %.0f y\n",
                        v.status, map_id, p.ax, p.ay, p.az, p.bx, p.by, p.bz, p.d, v.length);
            if (v.ok)
            {
                char buf[256];
                std::snprintf(buf, sizeof(buf), "%u %.1f %.1f %.1f %.1f %.1f %.1f %.0f", map_id, p.ax, p.ay, p.az, p.bx, p.by, p.bz, v.length);
                table.push_back(buf);
            }
        }
        for (Wedge const& w : wedges_by_map[map_id])
            CheckWedge(query, w);
        dtFreeNavMeshQuery(query);
        dtFreeNavMesh(mesh);
    }
    std::printf("\nLINK TABLE (map ax ay az bx by bz path_length), %zu of %zu pair(s) checked:\n", table.size(), total);
    for (std::string const& t : table)
        std::printf("%s\n", t.c_str());
    if (argc > 3)
    {
        std::ofstream out(argv[3]);
        for (std::string const& t : table)
            out << t << "\n";
    }
    return 0;
}
