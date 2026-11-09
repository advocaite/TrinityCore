// PlayerbotMovement.cpp
//
// See PlayerbotMovement.h for the contract.

#include "PlayerbotMovement.h"

#include "Map.h"
#include "MapDefines.h"
#include "MapManager.h"
#include "MMapManager.h"
#include "MMapDefines.h"
#include "FollowMovementGenerator.h"
#include "MotionMaster.h"
#include "MoveSplineInit.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "Unit.h"
#include "VMapDefinitions.h"
#include "Log.h"
#include "Config.h"
#include "GameTime.h"
#include "Fleet/CityLayer.h"
#include "BotSwim.h"
#include <unordered_map>

#include "DetourCommon.h"
#include "DetourNavMeshQuery.h"

#include <G3D/Vector3.h>

#include <algorithm>
#include <cmath>
#include <exception>

#ifdef _MSC_VER
#include <windows.h>   // EXCEPTION_EXECUTE_HANDLER for SehSafeCalculatePath
#endif

namespace Playerbot::BotMovement {

LiquidPolicy (*g_LiquidPathPolicy)(Player* p) = nullptr;

namespace {

// Same threshold BotSetupPipeline used (audit 2026-05-17 found 'Howling Fjord
// (Valgarde)' at Z=205 — 130y above ground; bots spawned on a cliff with no
// navmesh under their feet and emitted ~60k path-fail telemetry lines).
constexpr float kTeleportZSnapThresholdY = 50.0f;

// Distance above the queried ground that we drop the bot when snapping —
// matches the 2y lift BotSetupPipeline used so unit collision height never
// places the bot half-inside terrain on the first frame.
constexpr float kGroundLiftY = 2.0f;

} // namespace

// ---- SehSafeCalculatePath ---------------------------------------------------

#ifdef _MSC_VER
bool SehSafeCalculatePath(PathGenerator& path, float x, float y, float z) noexcept
{
    __try
    {
        return path.CalculatePath(x, y, z, /*forceDest=*/false);
    }
    // TC ASSERT/ABORT (EXCEPTION_ASSERTION_FAILURE) means corrupt state —
    // let it reach the crash handler for a dump at the fault site instead
    // of masking it as a failed path (same policy as SehSafeMapUpdate).
    __except (GetExceptionCode() == EXCEPTION_ASSERTION_FAILURE
              ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool SehSafeCalculatePathFrom(PathGenerator& path, float sx, float sy, float sz, float x, float y, float z) noexcept
{
    __try
    {
        return path.CalculatePath(sx, sy, sz, x, y, z, /*forceDest=*/false);
    }
    __except (GetExceptionCode() == EXCEPTION_ASSERTION_FAILURE
              ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

int SehSafeNearestPolyProbe(void const* query, float const* detourPt,
                            float const* extents, void const* filter,
                            float* nearestOut) noexcept
{
    __try
    {
        dtPolyRef ref = 0;
        static_cast<dtNavMeshQuery const*>(query)->findNearestPoly(
            detourPt, extents, static_cast<dtQueryFilter const*>(filter),
            &ref, nearestOut);
        return ref ? 1 : 0;
    }
    __except (GetExceptionCode() == EXCEPTION_ASSERTION_FAILURE
              ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}
#else
bool SehSafeCalculatePath(PathGenerator& path, float x, float y, float z) noexcept
{
    return path.CalculatePath(x, y, z, /*forceDest=*/false);
}

bool SehSafeCalculatePathFrom(PathGenerator& path, float sx, float sy, float sz, float x, float y, float z) noexcept
{
    return path.CalculatePath(sx, sy, sz, x, y, z, /*forceDest=*/false);
}

// Non-MSVC platforms have no SEH; run the probe directly. Mirrors the
// _MSC_VER branch above minus the __try/__except guard.
int SehSafeNearestPolyProbe(void const* query, float const* detourPt,
                            float const* extents, void const* filter,
                            float* nearestOut) noexcept
{
    dtPolyRef ref = 0;
    static_cast<dtNavMeshQuery const*>(query)->findNearestPoly(
        detourPt, extents, static_cast<dtQueryFilter const*>(filter),
        &ref, nearestOut);
    return ref ? 1 : 0;
}
#endif

// ---- SnapToGround -----------------------------------------------------------

bool SnapToGround(Player* p, float x, float y, float& z)
{
    if (!p)
        return false;

    Map* m = p->GetMap();
    if (!m)
        return false;

    // Pre-load the destination grid. UpdateAllowedPositionZ samples vmap
    // through the local grid; if the grid hasn't streamed in yet the
    // sampler returns INVALID_HEIGHT and we'd silently keep the bad Z.
    // LoadGrid is a no-op when the grid is already resident.
    m->LoadGrid(x, y);

    float const original = z;

    // Every step below can end on the terrain grid far under a floating floor: Map::GetHeight bounds only its vmap probe, and with no
    // model under (x, y) it returns the grid's height however far below (Hall of Thanes: the floor at z 500, the grid at 0). Where the
    // navmesh has a walkable surface at the asked level, that surface is the ground: hot_faldrim (0.1.26 RC8) sent the tank's advance
    // steps to z 0, Detour found no polygon there, and the group stood 50-60 s twice while the tank refused the same step.
    auto const far_below = [&](float found)
    {
        constexpr float kMaxDownwardSnapY = 40.0f;   // GroundZ's search depth
        if (found >= original - kMaxDownwardSnapY)
            return false;
        Position np;
        if (!NearestNavPoint(p, x, y, original, 3.0f, 6.0f, np))
            return false;
        z = np.GetPositionZ();
        return true;
    };

    // Step 1: UpdateAllowedPositionZ — same vmap/mmap composite TC uses
    // in Player::SetPosition. Wrap in try/catch because BIH::subdivide
    // throws "invalid node overlap" during dynamic GO churn (BG flags,
    // doors); crash 2026-05-13.
    try
    {
        p->UpdateAllowedPositionZ(x, y, z);
    }
    catch (std::exception const& e)
    {
        TC_LOG_WARN("playerbot.v2",
            "[BotMovement::SnapToGround] {} BIH exception at ({:.1f},{:.1f}): {}",
            p->GetName(), x, y, e.what());
        z = original;
    }
    catch (...)
    {
        z = original;
    }

    // UpdateAllowedPositionZ returns the input unchanged when it can't
    // find any ground/water. Detect that and fall through to the explicit
    // probes; otherwise we're done.
    if (std::fabs(z - original) > 0.05f)
    {
        far_below(z);
        return true;
    }

    // Step 2: GetWaterOrGroundLevel — swim-aware lookup with the player's
    // collision height accounted for. Best for shoreline / waterfall cases
    // where UpdateAllowedPositionZ picked the wrong surface.
    float ground = INVALID_HEIGHT;
    float const wog = m->GetWaterOrGroundLevel(p->GetPhaseShift(), x, y, original,
                                               &ground, /*swim*/ false);
    if (wog > INVALID_HEIGHT)
    {
        if (!far_below(wog))
            z = wog;
        return true;
    }

    // Step 3: top-down vmap probe. Last resort — finds any walkable
    // surface under (x, y, MAX_HEIGHT). If this returns INVALID_HEIGHT
    // there's genuinely no ground at this XY (off-map, hole in mesh).
    float const topdown = m->GetHeight(p->GetPhaseShift(), x, y, MAX_HEIGHT,
                                       /*vmap*/ true);
    if (topdown > INVALID_HEIGHT)
    {
        // Reject top-down results that are far ABOVE the input z. The
        // input z is often a player's actual in-game position (group
        // member, nearby creature) and is already valid. A large upward
        // result means the probe found outdoor terrain sitting above a
        // dungeon room — e.g., the Westfall hillside at z≈358 above the
        // Deadmines floor at z≈62. Accepting it corrupts the pathfinder
        // destination so that both src and dst map to the same entrance
        // poly, producing a perpetually-incomplete stub path and
        // [move_blocked] for every follow attempt.
        // 30y is generous: legitimate SnapToGround use cases only need a
        // few yards of upward correction; a 30y buried floor also corrects.
        constexpr float kMaxUpwardSnapY = 30.0f;
        if (topdown <= original + kMaxUpwardSnapY)
        {
            if (!far_below(topdown))
                z = topdown;
            return true;
        }
        // Outdoor terrain far above a dungeon — keep the caller's z.
        z = original;
        return false;
    }

    // No surface found. Restore the input so the caller can decide what
    // to do (refuse the move, log, retry with a different XY).
    z = original;
    return false;
}

// ---- GroundZ ----------------------------------------------------------------

// A height Map::GetHeight took from the terrain grid (no model under the probe) that lies well below the caller's z, on a dungeon map.
// Under a dungeon's buildings the grid is filler, not ground: where the probe falls through a gap in the floor model it hands back the
// grid however near it lies. SM Library (map 189, the hall floor at z 18.5 over a grid at 0): move targets at the edge of the entrance
// hall's open middle came back at z 0 ("[ground] Scnmage move_to z hint=18.6 ground=0.0", smlibrary_loksey 0.1.28h), and in 0.1.28g
// the tank and the priest stood at z 0.0 under the floor for ten minutes while the navmesh there lies at z 19-21. A gap of a few yards
// is a step or a stair; more is not a floor the caller asked about.
static bool GridFillerBelow(Map* m, Player* p, float x, float y, float zHint, float g)
{
    constexpr float kGridFillerGap = 6.0f;
    if (!m->IsDungeon() || g >= zHint - kGridFillerGap)
        return false;
    float const grid = m->GetGridHeight(p->GetPhaseShift(), x, y);
    return grid > INVALID_HEIGHT && std::fabs(g - grid) < 0.05f;
}

float GroundZ(Player* p, float x, float y, float zHint, char const* where, bool* ok)
{
    if (ok)
        *ok = false;
    static bool const enabled = sConfigMgr->GetBoolDefault("Playerbot.Behavior.GroundSnap", true, true);
    if (!enabled || !p)
        return zHint;
    Map* m = p->GetMap();
    if (!m)
        return zHint;

    constexpr float kGroundProbeUp   = 3.0f;
    constexpr float kGroundProbeDown = 40.0f;
    m->LoadGrid(x, y);
    float g = INVALID_HEIGHT;
    try
    {
        g = m->GetHeight(p->GetPhaseShift(), x, y, zHint + kGroundProbeUp, /*vmap*/ true,
                         kGroundProbeUp + kGroundProbeDown);
    }
    catch (...)
    {
        g = INVALID_HEIGHT;   // BIH churn race (see SnapToGround)
    }
    // Map::GetHeight bounds only its vmap probe by the search distance: when that finds nothing it hands back the terrain grid's height
    // however far below it lies. Over a floating hall that is no ground at all (Hall of Thanes, the floor at z 500 and the grid at 0:
    // a stand point beside the boss came back at z 0, and the mage sent there left the hall and the run).
    if (g <= INVALID_HEIGHT || g < zHint - kGroundProbeDown || GridFillerBelow(m, p, x, y, zHint, g))
        return zHint;
    if (ok)
        *ok = true;

    if (std::fabs(g - zHint) > 1.0f)
    {
        static std::unordered_map<uint64, uint32> s_groundLogAt;
        uint32 const now = GameTime::GetGameTimeMS();
        uint32& at = s_groundLogAt[p->GetGUID().GetCounter()];
        if (at == 0 || now - at >= 10000u)
        {
            at = now ? now : 1u;
            TC_LOG_INFO("playerbot.v2", "[ground] {} {} z hint={:.1f} ground={:.1f}",
                p->GetName(), where ? where : "?", zHint, g);
        }
    }
    return g;
}

bool ProbeGround(Player* p, float x, float y, float zHint, float& out)
{
    if (!p)
        return false;
    Map* m = p->GetMap();
    if (!m)
        return false;
    constexpr float kProbeUp   = 3.0f;
    constexpr float kProbeDown = 40.0f;
    m->LoadGrid(x, y);
    float g = INVALID_HEIGHT;
    try
    {
        g = m->GetHeight(p->GetPhaseShift(), x, y, zHint + kProbeUp, /*vmap*/ true, kProbeUp + kProbeDown);
    }
    catch (...)
    {
        g = INVALID_HEIGHT;   // BIH churn race (see SnapToGround)
    }
    if (g <= INVALID_HEIGHT || g < zHint - kProbeDown || GridFillerBelow(m, p, x, y, zHint, g))   // the terrain grid far below (see GroundZ)
        return false;
    out = g;
    return true;
}

// ---- NearestNavPoint --------------------------------------------------------

bool NearestNavPoint(Player* p, float tx, float ty, float tz,
                     float hxy, float hz, Position& out)
{
    if (!p)
        return false;
    Map* m = p->GetMap();
    if (!m)
        return false;

    // The tile under the target may not be resident (a tall lift shaft can
    // span into a grid the bot hasn't streamed). LoadGrid is a no-op when the
    // grid is already loaded; it keeps the navmesh tile alive for the query.
    m->LoadGrid(tx, ty);

    // Resolve the query the same way PathGenerator does: the *terrain* map id
    // (phased maps share a parent terrain) drives the navmesh selection, while
    // the instance map id + instance id pick the per-instance mesh.
    uint32 const terrainMapId = PhasingHandler::GetTerrainMapId(
        p->GetPhaseShift(), p->GetMapId(), m->GetTerrain(), tx, ty);
    dtNavMeshQuery const* query = MMAP::MMapManager::instance()->GetNavMeshQuery(
        terrainMapId, p->GetMapId(), p->GetInstanceId());
    if (!query)
        return false;

    // Walkable surfaces only (ground + steep ground + road — city ledges and
    // bridges beside lifts are frequently road-tagged by the road-aware mmaps,
    // so a GROUND-only filter would miss them). We deliberately do NOT include
    // WATER / MAGMA_SLIME: a disembark ledge must be dry footing, and a boarding
    // query must not snap a bot into a lava moat beside a raid lift.
    dtQueryFilter filter;
    filter.setIncludeFlags(NAV_GROUND | NAV_GROUND_STEEP | NAV_ROAD);
    filter.setExcludeFlags(0);

    // Detour coordinate convention: Recast is Y-up, so WoW (x,y,z) maps to
    // Recast (y,z,x); the snapped result maps back (x=pt[2], y=pt[0], z=pt[1]).
    float const center[3]  = { ty, tz, tx };
    float const extents[3] = { hxy, hz, hxy };
    float nearest[3] = { 0.f, 0.f, 0.f };
    // SEH-guarded probe — NearestNavPoint runs from the snapshot builder
    // (ElevatorIndex lazy ledge cache) on the world thread; same Detour
    // tile-race AV hazard as every other raw query (2026-06-12 sweep).
    if (SehSafeNearestPolyProbe(query, center, extents, &filter, nearest) <= 0)
        return false;

    out.Relocate(nearest[2], nearest[0], nearest[1]);
    return true;
}

// ---- LineIsClear ------------------------------------------------------------

bool LineIsClear(Player* p, float sx, float sy, float sz,
                            float dx, float dy, float dz)
{
    if (!p)
        return false;
    Map* m = p->GetMap();
    if (!m)
        return false;
    // VMAP::ModelIgnoreFlags::M2 lets us ignore decorative doodads (trees,
    // crates) that would otherwise block a jump's hop arc. Walls, doors,
    // ground geometry remain blocking — exactly what we want.
    // LineOfSightChecks is a bitflag-defined enum; `|` of two enumerators
    // promotes to int, so we re-cast to the enum type the signature wants.
    return m->isInLineOfSight(p->GetPhaseShift(), sx, sy, sz, dx, dy, dz,
                              static_cast<LineOfSightChecks>(
                                  LINEOFSIGHT_CHECK_VMAP |
                                  LINEOFSIGHT_CHECK_GOBJECT),
                              VMAP::ModelIgnoreFlags::M2);
}

// ---- SafeNearTeleport -------------------------------------------------------

bool SafeNearTeleport(Player* p, float x, float y, float z, float o, bool navmesh_point)
{
    if (!p)
        return false;

    // Remember the caller's intended Z. We need this to reject the
    // "snapped to wrong surface" case below (cave interior teleport
    // landing on the mountain roof above).
    float const requested_z = z;

    // Snap Z first. If there's no ground at all, refuse — the caller will
    // either retry with a different XY or escalate to stuck recovery.
    if (!SnapToGround(p, x, y, z))
    {
        if (!navmesh_point)
        {
            TC_LOG_INFO("playerbot.v2",
                "[BotMovement::SafeNearTeleport] {} no ground at ({:.1f},{:.1f}); refusing",
                p->GetName(), x, y);
            return false;
        }
        // The destination is a navmesh poly the caller found (the rally point of a stranded follower): the
        // height probe has no data there (BFD entrance, map 48: every probe is INVALID_HEIGHT), the mesh does.
        TC_LOG_INFO("playerbot.v2",
            "[BotMovement::SafeNearTeleport] {} no height data at ({:.1f},{:.1f}); landing on the navmesh point z={:.1f}",
            p->GetName(), x, y, requested_z);
        z = requested_z;
    }

    // Cave-interior safety check: when a bot is inside a cave / overhang
    // and the random XY offset lands on the mountain ABOVE the cave roof,
    // UpdateAllowedPositionZ's GetMapHeight picks the *topmost* walkable
    // surface (the mountain) rather than the cave floor. The bot teleports
    // out of the cave onto a hillside it can't path back down from
    // (Shadowthread Cave 2026-05-21, Uraimus). Reject when the snap moved
    // Z by more than 20y — that magnitude only happens when crossing into
    // a different surface stratum, and the right answer is "don't
    // teleport" rather than "land on the wrong floor".
    constexpr float kMaxSnapDeltaY = 20.0f;
    if (std::fabs(z - requested_z) > kMaxSnapDeltaY)
    {
        TC_LOG_INFO("playerbot.v2",
            "[BotMovement::SafeNearTeleport] {} Z snap delta {:.1f}y exceeds {:.0f}y "
            "(input z={:.1f} snapped z={:.1f}) at ({:.1f},{:.1f}); refusing",
            p->GetName(), z - requested_z, kMaxSnapDeltaY,
            requested_z, z, x, y);
        return false;
    }

    // Off-mesh rescue: if (x,y,z) is still FARFROMPOLY, ask Detour for
    // the closest poly and use that instead. Prevents landing in a wall
    // that vmap considers "ground" but Detour can't path away from.
    PathGenerator probe(p);
    if (SehSafeCalculatePath(probe, x, y, z))
    {
        PathType pt = probe.GetPathType();
        if (pt & PATHFIND_FARFROMPOLY_END)
        {
            G3D::Vector3 const& nearest = probe.GetActualEndPosition();
            TC_LOG_INFO("playerbot.v2",
                "[BotMovement::SafeNearTeleport] {} dest off-mesh, snapping ({:.1f},{:.1f},{:.1f}) -> ({:.1f},{:.1f},{:.1f})",
                p->GetName(), x, y, z, nearest.x, nearest.y, nearest.z);
            x = nearest.x;
            y = nearest.y;
            z = nearest.z;
        }
    }

    // Never land a bot in lava or slime. The ground snap above is swim-aware
    // (WorldObject::UpdateAllowedPositionZ), so a destination over a lava lake
    // resolves to the lava SURFACE, and a trigger / waypoint centre that hangs
    // over lava (the Molten Core entrance triggers over the Blackrock Mountain
    // lava) puts the bot into it: 25 bots died within seconds of a rejoin
    // summon, 2026-10-06. Resolve the nearest DRY walkable navmesh point
    // instead (ground / road polys only, never MAGMA_SLIME); refuse when there
    // is none so the caller sees a failure rather than a dead bot.
    auto const inHarmfulLiquid = [&](float px, float py, float pz)
    {
        LiquidData ld;
        ZLiquidStatus const st = p->GetMap()->GetLiquidStatus(
            p->GetPhaseShift(), px, py, pz,
            map_liquidHeaderTypeFlags::AllLiquids, &ld);
        // The vmap filters on the model's raw liquid type, before the area table's override (the Majordomo dais is lava only after it),
        // so ask for all liquids and test the flags that come back.
        if (st != LIQUID_MAP_NO_WATER && !ld.type_flags.HasFlag(map_liquidHeaderTypeFlags::Magma) &&
            !ld.type_flags.HasFlag(map_liquidHeaderTypeFlags::Slime))
            return false;
        // A point ON the surface reads WATER_WALK (delta > -0.1) and one just above it ABOVE_WATER, and the bot falls in at
        // once: the Molten Core entrance snap lands exactly on the lava surface (z 168.5, ground 165.2), so only IN / UNDER
        // let Cornelia and Cassanny die there on arrival (2026-10-11, killer=self). Anything within a yard of the surface
        // or below it is harmful; a bridge or ledge high above the lake is not.
        if (st & (LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER))
            return true;
        return (st & (LIQUID_MAP_WATER_WALK | LIQUID_MAP_ABOVE_WATER)) != 0 && pz - ld.level < 1.0f;
    };
    if (inHarmfulLiquid(x, y, z))
    {
        Position dry;
        constexpr float kDryHxy = 15.0f;
        constexpr float kDryHz  = 12.0f;
        if (!NearestNavPoint(p, x, y, requested_z, kDryHxy, kDryHz, dry)
            || inHarmfulLiquid(dry.GetPositionX(), dry.GetPositionY(), dry.GetPositionZ()))
        {
            TC_LOG_INFO("playerbot.v2",
                "[BotMovement::SafeNearTeleport] {} destination ({:.1f},{:.1f},{:.1f}) is in lava/slime and no dry "
                "navmesh point lies within {:.0f}y; refusing",
                p->GetName(), x, y, z, kDryHxy);
            return false;
        }
        TC_LOG_INFO("playerbot.v2",
            "[BotMovement::SafeNearTeleport] {} destination ({:.1f},{:.1f},{:.1f}) is in lava/slime, using dry ground "
            "({:.1f},{:.1f},{:.1f})",
            p->GetName(), x, y, z, dry.GetPositionX(), dry.GetPositionY(), dry.GetPositionZ());
        x = dry.GetPositionX();
        y = dry.GetPositionY();
        z = dry.GetPositionZ();
    }

    p->NearTeleportTo(x, y, z, o);
    return true;
}

// ---- SafeTeleport -----------------------------------------------------------

bool SafeTeleport(Player* p, uint32 map_id, float x, float y, float z, float o,
                  uint32 options)
{
    if (!p)
        return false;

    // Probe the destination ground if the target map is already loaded.
    // World maps stream in lazily; if the map isn't resident we accept
    // the teleport as-is (the map loads on Player::TeleportTo and we'd
    // need a post-teleport hook to validate after arrival).
    if (Map* destMap = sMapMgr->FindMap(map_id, 0))
    {
        float const ground = destMap->GetHeight(PhasingHandler::GetEmptyPhaseShift(),
                                                x, y, MAX_HEIGHT, /*vmap*/ true);
        if (ground > INVALID_HEIGHT && (z - ground) > kTeleportZSnapThresholdY)
        {
            float const snapped = ground + kGroundLiftY;
            TC_LOG_WARN("playerbot.v2",
                "[BotMovement::SafeTeleport] {} dest map={} ({:.1f},{:.1f},{:.1f}) is "
                "{:.1f}y above ground ({:.1f}); snapping to ground+{} = {:.1f}",
                p->GetName(), map_id, x, y, z, z - ground, ground,
                kGroundLiftY, snapped);
            z = snapped;
        }
    }

    // A bot on a taxi flight is carried on by the flight spline after a teleport: the MC Garr squad was sent into the raid while one
    // member was in the air from staging and flew on to 600 y outside the room (2026-10-10). End the flight first, as a landing does.
    if (p->IsInFlight())
    {
        p->FinishTaxiFlight();
        p->CleanupAfterTaxiFlight();
    }

    return p->TeleportTo(map_id, x, y, z, o,
                         static_cast<TeleportToOptions>(options));
}

bool SafeTeleport(Player* p, WorldLocation const& loc, uint32 options)
{
    return SafeTeleport(p, loc.GetMapId(), loc.GetPositionX(), loc.GetPositionY(),
                        loc.GetPositionZ(), loc.GetOrientation(), options);
}

// ---- TryUnstickJump ---------------------------------------------------------

bool TryUnstickJump(Player* p, float forward)
{
    if (!p)
        return false;

    float const sx = p->GetPositionX();
    float const sy = p->GetPositionY();
    float const sz = p->GetPositionZ();
    float const facing = p->GetOrientation();

    // M-P2c: an unstick hop emitted at the bot's CURRENT facing nearly
    // always launches INTO the obstacle that wedged it (the bot was walking
    // toward its goal when it jammed, so it's still pointing at the wall /
    // ledge / closed door). Try a fan of bearings and take the first that
    // both lands on real ground (SnapToGround) and has a clear arc chord
    // (LineIsClear). Order: current facing first (cheapest unstick when the
    // jam is a low ledge directly ahead), then ±90° sidesteps, then 180°
    // (back the way we came — the guaranteed-open escape from a dead-end
    // pocket). Face the chosen bearing before MoveJump so the parabolic arc
    // actually travels that way.
    constexpr float kPi = float(M_PI);
    float const bearings[] = {
        facing,                 // straight ahead
        facing + kPi * 0.5f,    // +90° (left)
        facing - kPi * 0.5f,    // -90° (right)
        facing + kPi,           // 180° (reverse out of the pocket)
    };

    for (float bearing : bearings)
    {
        float const dx = sx + std::cos(bearing) * forward;
        float const dy = sy + std::sin(bearing) * forward;

        float dz = sz;
        if (!SnapToGround(p, dx, dy, dz))
            continue;   // no ground under this landing — try the next bearing

        // Strict LoS gate on the arc chord at chest height. Hopping into a
        // closed door / wall / cliff face wedges the bot harder, so a bearing
        // whose chord is blocked is rejected.
        if (!LineIsClear(p, sx, sy, sz + 1.5f, dx, dy, dz + 1.5f))
            continue;

        // Face the bearing first; MoveJump launches along the unit's facing
        // and we want the arc to travel toward the open direction we found.
        p->SetFacingTo(bearing);

        Position dest = p->GetPosition();
        dest.m_positionX = dx;
        dest.m_positionY = dy;
        dest.m_positionZ = dz;
        p->GetMotionMaster()->MoveJump(/*id*/ 0, dest, /*speedXY*/ 7.0f);
        return true;
    }

    // Every bearing was blocked or had no ground — refuse rather than launch
    // the bot into geometry / the void.
    return false;
}

// ---- SettleToGround ---------------------------------------------------------

bool SettleToGround(Player* p, float minGap, float maxGap)
{
    if (!p || !p->IsInWorld() || !p->IsAlive() || p->IsBeingTeleported())
        return false;
    // Falling needs gravity and nothing else in charge of the bot: not on a taxi, vehicle or transport, not flying or swimming, not
    // already moving (a spline in flight lands where it was told to).
    if (p->isMoving() || p->IsFlying() || p->IsInFlight() || p->GetVehicle() || p->GetTransport() || p->IsInWater() ||
        p->HasUnitState(UNIT_STATE_NOT_MOVE) || p->IsFalling())
        return false;
    float const x = p->GetPositionX();
    float const y = p->GetPositionY();
    float const z = p->GetPositionZ();
    float const ground = p->GetMapHeight(x, y, z + 0.5f, true, maxGap + 1.0f);
    if (ground <= INVALID_HEIGHT)
        return false;
    float const gap = z - ground;
    if (gap < minGap || gap > maxGap)
        return false;
    // On deep water a bot's z is the surface (WATER_WALK: IsInWater() is false there), and the floor is the lake bed: that is a swim,
    // not a fall (a BG river: the bot sank to the bed and drowned).
    if (float swimZ = 0.f; SwimLevel(p, x, y, z, swimZ))
        return false;
    // A real player falls client-side; a bot has no client, so nothing ever moves it down. Launch the fall as a spline
    // (the same as MotionMaster::MoveFall does for creatures) so it drops to the floor and every viewer sees it land.
    Movement::MoveSplineInit init(p);
    init.MoveTo(x, y, ground, false);
    init.SetFall();
    init.Launch();
    p->SetFallInformation(0, ground);
    return true;
}

// ---- SwimLevel / UpdateSwimState -------------------------------------------

bool SwimLevel(Player* p, float x, float y, float zHint, float& swimZ)
{
    if (!p || !p->IsInWorld())
        return false;
    Map* m = p->GetMap();
    if (!m)
        return false;
    // The floor under the asked point, searched from a little above it: from the surface this finds the lake bed, from a bridge
    // the bridge (no water under the bridge's own floor, so no swim).
    float floor_z = INVALID_HEIGHT;
    try
    {
        floor_z = m->GetHeight(p->GetPhaseShift(), x, y, zHint + 2.0f, /*vmap*/ true, 100.0f);
    }
    catch (...)
    {
        return false;   // BIH churn race (see SnapToGround)
    }
    if (floor_z <= INVALID_HEIGHT)
        return false;
    LiquidData ld{};
    ZLiquidStatus const st = m->GetLiquidStatus(p->GetPhaseShift(), x, y, floor_z, map_liquidHeaderTypeFlags::AllLiquids, &ld,
                                                p->GetCollisionHeight());
    if (st == LIQUID_MAP_NO_WATER || st == LIQUID_MAP_ABOVE_WATER)
        return false;
    // Lava and slime are not swum: the flee and refuse rules deal with them.
    if (ld.type_flags.HasFlag(map_liquidHeaderTypeFlags::Magma) || ld.type_flags.HasFlag(map_liquidHeaderTypeFlags::Slime))
        return false;
    if (!Swim::IsDeep(ld.level, floor_z, p->GetCollisionHeight()) || Swim::AskedFromAbove(zHint, ld.level))
        return false;
    swimZ = Swim::SwimZ(ld.level, p->GetCollisionHeight());
    return true;
}

void UpdateSwimState(Player* p)
{
    if (!p || !p->IsInWorld() || p->IsBeingTeleported())
        return;
    bool const flagged = p->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING);
    // Cheap first: no liquid at all here (the status the core keeps from the last relocation) and not swimming.
    if (!flagged && p->GetLiquidStatus() == LIQUID_MAP_NO_WATER)
        return;

    // Twice a second per bot is enough: the flag only changes the animation and the speed of the next spline. World thread only.
    static std::unordered_map<uint64, uint32> s_swimCheckAt;
    uint32 const now = GameTime::GetGameTimeMS();
    uint32& at = s_swimCheckAt[p->GetGUID().GetCounter()];
    if (at != 0 && now - at < 500u)
        return;
    at = now ? now : 1u;

    float const x = p->GetPositionX(), y = p->GetPositionY(), z = p->GetPositionZ();
    float swimZ = 0.f;
    // A ghost walks the lake bed to its corpse (no breath, no drowning); flying, a taxi, a ship, a vehicle and water walking keep
    // their own movement.
    bool const deep = p->IsAlive() && !p->IsFlying() && !p->IsInFlight() && !p->GetTransport() && !p->GetVehicle() &&
        !p->HasAuraType(SPELL_AURA_WATER_WALK) && !p->HasAuraType(SPELL_AURA_HOVER) && SwimLevel(p, x, y, z, swimZ);
    if (p->SetSwim(deep))
    {
        static uint32 s_flagLogged = 0;
        if (s_flagLogged++ < 500)
            TC_LOG_INFO("playerbot.v2", "[swim] {} {} swimming at ({:.1f},{:.1f},{:.1f})", p->GetName(), deep ? "starts" : "stops", x, y, z);
    }
    if (!deep)
        return;

    // Standing still in deep water: float at the swim z. A bot that walked down to the lake bed (a goal's ground z) comes up before it
    // drowns, and one left on top of the water by a path along the navmesh's water polygons sinks in to swim. Never while a move,
    // a cast or a root is running: the next spline starts from where the bot is. A follower standing by its leader settles too (the
    // follow generator waits while the leader stands, and paths again from the swim z when he moves).
    MovementGeneratorType const gen = p->GetMotionMaster()->GetCurrentMovementGeneratorType();
    if (p->isMoving() || p->IsFalling() || (gen != IDLE_MOTION_TYPE && gen != FOLLOW_MOTION_TYPE) ||
        p->HasUnitState(UNIT_STATE_NOT_MOVE) || p->IsNonMeleeSpellCast(false) || !Swim::NeedsSettle(z, swimZ))
        return;
    // A follower settles only where it stays in its follow range. The follow generator measures that range in 3D and, once the follower
    // is out of it, paths again to a navmesh point by the leader, which in water is the surface: a settle that took the follower out
    // of range was undone at the leader's next step and the two took turns (lake_swim 2026-11-09: a follower 2 y behind a leader on
    // the surface, settled down and lifted back up 14 times in 25 s). Left where it is, it still swims (the flag above).
    if (gen == FOLLOW_MOTION_TYPE)
    {
        auto const* follow = static_cast<FollowMovementGenerator const*>(p->GetMotionMaster()->GetCurrentMovementGenerator());
        Unit const* leader = follow ? follow->GetTarget() : nullptr;
        if (!leader || leader->GetMapId() != p->GetMapId() ||
            !Swim::SettleKeepsFollowRange(x - leader->GetPositionX(), y - leader->GetPositionY(), swimZ - leader->GetPositionZ(),
                                          p->GetCombatReach() + leader->GetCombatReach() + follow->GetRange()))
            return;
    }
    Movement::MoveSplineInit init(p);
    init.MoveTo(x, y, swimZ, /*generatePath*/ false);
    init.Launch();
    static uint32 s_settleLogged = 0;
    if (s_settleLogged++ < 500)
        TC_LOG_INFO("playerbot.v2", "[swim_settle] {} at ({:.1f},{:.1f}) z {:.1f} -> {:.1f} ({})", p->GetName(), x, y, z, swimZ,
                    z < swimZ ? "up from the floor" : "down from the surface");
}

// ---- PostTeleportSnap -------------------------------------------------------

void PostTeleportSnap(Player* p)
{
    if (!p || !p->IsInWorld())
        return;
    // A teleport is still pending (delayed or waiting for its ack): the position here is the one being left, so the probe would judge
    // and snap the wrong place (WSG 2026-10-24: a bot's refused BG port logged an "off-mesh landing" on its Redridge spawn spot).
    if (p->IsBeingTeleported())
        return;
    Map* m = p->GetMap();
    if (!m)
        return;

    float const x = p->GetPositionX();
    float const y = p->GetPositionY();
    float const z = p->GetPositionZ();

    // Cheap probe: if Detour can reach the bot's current position, the
    // landing was fine and we exit silently.
    PathGenerator probe(p);
    if (!SehSafeCalculatePath(probe, x, y, z))
        return;
    PathType pt = probe.GetPathType();
    if (!(pt & (PATHFIND_FARFROMPOLY_START | PATHFIND_FARFROMPOLY_END | PATHFIND_NOPATH)))
        return;

    // Off-mesh landing. Try to recover via Detour's nearest poly first;
    // fall back to a top-down vmap probe if Detour gives nothing useful.
    G3D::Vector3 const& nearest = probe.GetActualEndPosition();
    float nx = nearest.x;
    float ny = nearest.y;
    float nz = nearest.z;

    if (!SnapToGround(p, nx, ny, nz))
    {
        // Last resort: probe directly under the current XY. If even that
        // fails the map has no ground here and we leave the bot alone —
        // the stuck-recovery escalation will take over.
        float top = m->GetHeight(p->GetPhaseShift(), x, y, MAX_HEIGHT, /*vmap*/ true);
        if (top <= INVALID_HEIGHT)
            return;
        nx = x;
        ny = y;
        nz = top + kGroundLiftY;
    }

    TC_LOG_INFO("playerbot.v2",
        "[BotMovement::PostTeleportSnap] {} off-mesh landing ({:.1f},{:.1f},{:.1f}) -> ({:.1f},{:.1f},{:.1f})",
        p->GetName(), x, y, z, nx, ny, nz);
    p->NearTeleportTo(nx, ny, nz, p->GetOrientation());
}

// ---- ClassifyDestinationLiquid ----------------------------------------------

LiquidVerdict ClassifyDestinationLiquid(Player* p, float x, float y, float z,
                                        LiquidData* outData /*= nullptr*/)
{
    if (!p)
        return LiquidVerdict::Dry;

    Map* m = p->GetMap();
    if (!m)
        return LiquidVerdict::Dry;

    // Decide the wade<->swim boundary against the bot's REAL collision height
    // (matches the client's per-model depth check), but floor it at
    // DEFAULT_COLLISION_HEIGHT. A smaller height would classify shallower water
    // as UNDER_WATER and start refusing the shoreline wading starter-zone hubs
    // depend on — flooring keeps the gate at-least-as-permissive as the
    // historical default while letting taller / mounted bots wade deeper.
    float const collisionHeight =
        std::max(p->GetCollisionHeight(), DEFAULT_COLLISION_HEIGHT);

    LiquidData liq{};
    ZLiquidStatus const zs = m->GetLiquidStatus(p->GetPhaseShift(), x, y, z,
                                                map_liquidHeaderTypeFlags::AllLiquids,
                                                &liq, collisionHeight);
    if (outData)
        *outData = liq;

    if (zs == LIQUID_MAP_NO_WATER || !LiquidMattersFor(p->IsAlive()))
        return LiquidVerdict::Dry;

    // Magma / slime is lethal on contact regardless of depth.
    if (liq.type_flags.HasFlag(map_liquidHeaderTypeFlags::Magma) ||
        liq.type_flags.HasFlag(map_liquidHeaderTypeFlags::Slime))
        return LiquidVerdict::Hazard;

    // Fully submerged for this bot's collision height => must swim: a
    // non-aquatic bot drowns, and the navmesh ends at the surface so there is
    // no walkable path below it anyway.
    //
    // Bitmask test, NOT exact-equality: GridMap/TerrainMgr OR in
    // LIQUID_MAP_OCEAN_FLOOR (0x10) whenever the probe sits within
    // GROUND_HEIGHT_TOLERANCE of the floor, so a bot on the BOTTOM of deep
    // water returns UNDER_WATER|OCEAN_FLOOR (0x18). Because routing probes the
    // ground-level Z, that floor case is the COMMON one — an `== UNDER_WATER`
    // test would miss it and wrongly allow walking into deep water. `& UNDER_
    // WATER` catches both 0x08 and 0x18; shallow IN_WATER|OCEAN_FLOOR (0x14)
    // and WATER_WALK|OCEAN_FLOOR (0x12) correctly stay Wadeable.
    if ((zs & LIQUID_MAP_UNDER_WATER) != 0)
        return LiquidVerdict::Swim;

    // ABOVE_WATER / WATER_WALK / IN_WATER / OCEAN_FLOOR: the bot's head stays
    // above the surface — wading depth, allowed (PathGenerator wades through).
    return LiquidVerdict::Wadeable;
}

// ---- NoPath storm ------------------------------------------------------------

namespace {
std::unordered_map<uint64, ::Playerbot::V2::Fleet::NoPathRun> g_noPathRun;
}

void NoteNoPath(Player const* p)
{
    if (p)
        ::Playerbot::V2::Fleet::NoteNoPath(g_noPathRun[p->GetGUID().GetCounter()], GameTime::GetGameTimeMS());
}

bool NoPathStorm(Player const* p)
{
    if (!p)
        return false;
    auto it = g_noPathRun.find(p->GetGUID().GetCounter());
    return it != g_noPathRun.end() && ::Playerbot::V2::Fleet::NoPathStorm(it->second, GameTime::GetGameTimeMS());
}

void ClearNoPath(Player const* p)
{
    if (p)
        g_noPathRun.erase(p->GetGUID().GetCounter());
}

} // namespace Playerbot::BotMovement
