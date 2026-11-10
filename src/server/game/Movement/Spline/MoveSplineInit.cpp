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

#include "MoveSplineInit.h"
#include "Log.h"
#include "Creature.h"
#include "MoveSpline.h"
#include "MovementPackets.h"
#include "PathGenerator.h"
#include "Timer.h"
#include "Unit.h"
#include "VehicleDefines.h"
#include <cmath>

namespace Movement
{
    namespace
    {
        // A unit whose movement the client has to merge with its own (a bot, or a pet / guardian of a player): these get plain, short,
        // walkable splines (see the Launch notes). Pets of human players share the treatment: the same client assert hit a hunter pet
        // ("Unit(Pet-...), count(11) index(4), mergePathResult(10)", WSG Alliance ramp, 2026-10-08).
        bool PlayerSideUnit(Unit const* unit)
        {
            return unit->IsPlayer() || unit->GetCharmerOrOwnerGUID().IsPlayer();
        }
    }

    // Transforms coordinates from global to transport offsets
    class TransportPathTransform
    {
    public:
        TransportPathTransform(Unit const* owner, bool transformForTransport)
            : _transport(transformForTransport ? owner->GetDirectTransport() : nullptr) { }

        Vector3 operator()(Vector3 input) const
        {
            if (_transport)
                _transport->GetPositionOffsetTo({ input.x, input.y, input.z }).GetPosition(input.x, input.y, input.z);

            return input;
        }

        float operator()(float input) const
        {
            if (_transport)
                input -= _transport->GetTransportOrientation();

            return input;
        }

    private:
        TransportBase* _transport;
    };

    UnitMoveType SelectSpeedType(MovementFlags moveFlags)
    {
        if (moveFlags & MOVEMENTFLAG_FLYING)
        {
            if (moveFlags & MOVEMENTFLAG_BACKWARD /*&& speed_obj.flight >= speed_obj.flight_back*/)
                return MOVE_FLIGHT_BACK;
            else
                return MOVE_FLIGHT;
        }
        else if (moveFlags & MOVEMENTFLAG_SWIMMING)
        {
            if (moveFlags & MOVEMENTFLAG_BACKWARD /*&& speed_obj.swim >= speed_obj.swim_back*/)
                return MOVE_SWIM_BACK;
            else
                return MOVE_SWIM;
        }
        else if (moveFlags & MOVEMENTFLAG_WALKING)
        {
            //if (speed_obj.run > speed_obj.walk)
            return MOVE_WALK;
        }
        else if (moveFlags & MOVEMENTFLAG_BACKWARD /*&& speed_obj.run >= speed_obj.run_back*/)
            return MOVE_RUN_BACK;

        // Flying creatures use MOVEMENTFLAG_CAN_FLY or MOVEMENTFLAG_DISABLE_GRAVITY
        // Run speed is their default flight speed.
        return MOVE_RUN;
    }

    int32 MoveSplineInit::Launch()
    {
        MoveSpline& move_spline = *unit->movespline;

        bool transport = !unit->GetTransGUID().IsEmpty();
        Location real_position;
        // there is a big chance that current position is unknown if current state is not finalized, need compute it
        // this also allows calculate spline position and update map position in much greater intervals
        // Don't compute for transport movement if the unit is in a motion between two transports
        if (!move_spline.Finalized() && move_spline.onTransport == transport)
            real_position = move_spline.ComputePosition();
        else
        {
            Position const* pos;
            if (!transport)
                pos = unit;
            else
                pos = &unit->m_movementInfo.transport.pos;

            real_position.x = pos->GetPositionX();
            real_position.y = pos->GetPositionY();
            real_position.z = pos->GetPositionZ();
            real_position.orientation = unit->GetOrientation();
        }

        // should i do the things that user should do? - no.
        if (args.path.empty())
            return 0;

        // correct first vertex
        args.path[0] = real_position;
        args.initialOrientation = real_position.orientation;

        // The client rejects a path with a non-finite point or a segment shorter than 0.1 y (BadSpline / FLT_DIVIDE_BY_ZERO) and
        // Validate() does not check the first segment: the corrected start can coincide with path[1]. Drop such points so the server
        // spline has no zero-length segment either. Paths whose points are indexed (cyclic, catmullrom, parabolic, falling, spell
        // effect) are left alone.
        if (!args.flags.Cyclic && !args.flags.Catmullrom && !args.flags.Parabolic && !args.flags.Falling &&
            !args.spellEffectExtra && args.effect_start_point == 0)
        {
            auto const isFinite = [](Vector3 const& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); };
            size_t kept = 1;    // path[0] is the corrected start
            for (size_t i = 1; i < args.path.size(); ++i)
            {
                if (!isFinite(args.path[i]))
                    continue;
                if ((args.path[i] - args.path[kept - 1]).squaredLength() < 0.01f)
                {
                    if (i == args.path.size() - 1 && kept > 1)
                        args.path[kept - 1] = args.path[i];     // keep the exact destination
                    continue;
                }
                args.path[kept++] = args.path[i];
            }
            args.path.resize(kept);
            if (args.path.size() < 2)
            {
                // facing only (MoveSpline::splineIsFacingOnly), turn, animation and transport enter/exit splines in place keep the
                // unit's own position twice
                if (args.facing.type != MONSTER_MOVE_NORMAL || args.turnData || args.animTier || args.flags.TransportEnter || args.flags.TransportExit)
                    args.path.push_back(args.path[0]);
                else
                {
                    // nothing to move to, end the current movement instead
                    if (!move_spline.Finalized())
                        MoveSplineInit(unit).Stop(true);
                    return 0;
                }
            }
        }

        args.flags.Enter_Cycle = args.flags.Cyclic;
        move_spline.onTransport = transport;

        MovementFlags moveFlags = unit->m_movementInfo.GetMovementFlags();
        if (!args.flags.Backward)
            moveFlags = (moveFlags & ~MOVEMENTFLAG_BACKWARD) | MOVEMENTFLAG_FORWARD;
        else
            moveFlags = (moveFlags & ~MOVEMENTFLAG_FORWARD) | MOVEMENTFLAG_BACKWARD;

        if (moveFlags & MOVEMENTFLAG_ROOT)
            moveFlags &= ~MOVEMENTFLAG_MASK_MOVING;

        if (!args.HasVelocity)
        {
            // If spline is initialized with SetWalk method it only means we need to select
            // walk move speed for it but not add walk flag to unit
            MovementFlags moveFlagsForSpeed = moveFlags;
            if (args.walk)
                moveFlagsForSpeed |= MOVEMENTFLAG_WALKING;
            else
                moveFlagsForSpeed &= ~MOVEMENTFLAG_WALKING;

            args.velocity = unit->GetSpeed(SelectSpeedType(moveFlagsForSpeed));
            if (Creature* creature = unit->ToCreature())
                if (creature->HasSearchedAssistance())
                    args.velocity *= 0.66f;
        }

        // limit the speed in the same way the client does
        float speedLimit = [&]()
        {
            if (args.flags.UnlimitedSpeed)
                return std::numeric_limits<float>::max();

            if (args.flags.Falling || args.flags.Catmullrom || args.flags.Flying || args.flags.Parabolic)
                return 50.0f;

            return std::max(28.0f, unit->GetSpeed(MOVE_RUN) * 4.0f);
        }();

        args.velocity = std::min(args.velocity, speedLimit);

        if (!args.Validate(unit))
            return 0;

        // A player unit (a bot) that re-paths while its previous spline is still in flight: end that spline on the client first. The
        // client otherwise merges the new path into the one running and asserts when the merged path does not fit
        // (Unit_C.cpp(9651) "count(18) index(7) ... mergePathResult(12) splineFlags(00200000)", WSG start room 2026-10-08, with the
        // Steering flags already off). After the stop the new spline starts on an idle unit and there is nothing to merge.
        // The server finishes a spline before the client does (latency, and the client interpolates against its own clock), so
        // "not finalized" misses the common case: the third WSG crash had the unit at the same spot with a 76-point merged path although
        // this stop was already there. A player unit that got a spline in the last 3 s is stopped on the client as well. The map is
        // thread_local (a unit is updated by one map thread at a time; a stale entry on another thread only costs an extra stop).
        if (PlayerSideUnit(unit))
        {
            thread_local std::unordered_map<uint64, uint32> s_lastSplineMs;
            uint32 const nowMs = getMSTime();
            uint32& last = s_lastSplineMs[unit->GetGUID().GetCounter()];
            // A pet re-paths every follow step: its window is shorter so it does not stop and start continuously.
            bool const recent = last != 0 && (nowMs - last) < (unit->IsPlayer() ? 3000u : 1000u);
            if (!move_spline.Finalized() || recent)
                MoveSplineInit(unit).Stop(true);
            last = nowMs;
            if (s_lastSplineMs.size() > 8192)
                s_lastSplineMs.clear();
        }

        unit->m_movementInfo.SetMovementFlags(moveFlags);
        move_spline.Initialize(args);

        WorldPackets::Movement::MonsterMove packet;
        packet.MoverGUID = unit->GetGUID();
        packet.Pos = Position(real_position.x, real_position.y, real_position.z, real_position.orientation);
        packet.InitializeSplineData(move_spline);
        if (transport)
        {
            packet.SplineData.Move.TransportGUID = unit->GetTransGUID();
            packet.SplineData.Move.VehicleSeat = unit->GetTransSeat();
        }

        unit->SendMessageToSet(packet.Write(), true);

        return move_spline.Duration();
    }

    void MoveSplineInit::Stop(bool force /*= false*/)
    {
        MoveSpline& move_spline = *unit->movespline;

        // No need to stop if we are not moving
        if (!force && move_spline.Finalized())
            return;

        bool transport = !unit->GetTransGUID().IsEmpty();
        Location loc;
        if (move_spline.onTransport == transport && !move_spline.Finalized())
            loc = move_spline.ComputePosition();
        else
        {
            Position const* pos;
            if (!transport)
                pos = unit;
            else
                pos = &unit->m_movementInfo.transport.pos;

            loc.x = pos->GetPositionX();
            loc.y = pos->GetPositionY();
            loc.z = pos->GetPositionZ();
            loc.orientation = unit->GetOrientation();
        }

        if (move_spline.isTurning())
            SetFacing(loc.orientation);

        args.flags = MoveSplineFlagEnum::Done;
        unit->m_movementInfo.RemoveMovementFlag(MOVEMENTFLAG_FORWARD);
        move_spline.onTransport = transport;
        move_spline.Initialize(args);

        WorldPackets::Movement::MonsterMove packet;
        packet.MoverGUID = unit->GetGUID();
        packet.Pos = Position(loc.x, loc.y, loc.z, loc.orientation);
        packet.SplineData.StopSplineStyle = 2;
        packet.SplineData.ID = move_spline.GetId();
        packet.SplineData.StopUseFaceDirection = args.facing.type == MONSTER_MOVE_FACING_ANGLE;
        packet.SplineData.Move.Face = args.facing.type;
        packet.SplineData.Move.FaceDirection = args.facing.angle;

        if (transport)
        {
            packet.SplineData.Move.TransportGUID = unit->GetTransGUID();
            packet.SplineData.Move.VehicleSeat = unit->GetTransSeat();
        }

        unit->SendMessageToSet(packet.Write(), true);
    }

    MoveSplineInit::MoveSplineInit(Unit* m) : unit(m)
    {
        args.splineId = splineIdGen.NewId();
        // Elevators also use MOVEMENTFLAG_ONTRANSPORT but we do not keep track of their position changes
        args.TransformForTransport = !unit->GetTransGUID().IsEmpty();
        // mix existing state into new
        args.walk = unit->HasUnitMovementFlag(MOVEMENTFLAG_WALKING);
        args.flags.CanSwim = unit->CanSwim();
        args.flags.Flying = unit->HasUnitMovementFlag(MovementFlags(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_DISABLE_GRAVITY));
        args.flags.FastSteering = true;
        args.flags.Steering = unit->HasNpcFlag2(UNIT_NPC_FLAG_2_STEERING) || !unit->IsInCombat();
        // Steering makes the client merge a new path into the spline still in flight; for a player unit (a bot driven by monster moves)
        // that merge asserted on the client ("Unit(Player-...), count(11) index(5), mergePathResult(12)", splineFlags 0x11200000 =
        // Steering | FastSteering | CanSwim). Players get plain splines: the new path replaces the old one.
        if (PlayerSideUnit(unit))
        {
            args.flags.FastSteering = false;
            args.flags.Steering = false;
        }
    }

    MoveSplineInit::~MoveSplineInit() = default;

    void MoveSplineInit::SetFacing(Vector3 const& spot)
    {
        TransportPathTransform transform(unit, args.TransformForTransport);
        Vector3 finalSpot = transform(spot);
        args.facing.f.x = finalSpot.x;
        args.facing.f.y = finalSpot.y;
        args.facing.f.z = finalSpot.z;
        args.facing.type = MONSTER_MOVE_FACING_SPOT;
    }

    void MoveSplineInit::SetFacing(float x, float y, float z)
    {
        SetFacing({ x, y, z });
    }

    void MoveSplineInit::SetFacing(Unit const* target)
    {
        args.facing.angle = unit->GetAbsoluteAngle(target);
        args.facing.target = target->GetGUID();
        args.facing.type = MONSTER_MOVE_FACING_TARGET;
    }

    void MoveSplineInit::SetFacing(float angle)
    {
        TransportPathTransform transform(unit, args.TransformForTransport);
        args.facing.angle = Position::NormalizeOrientation(transform(angle));
        args.facing.type = MONSTER_MOVE_FACING_ANGLE;
    }

    void MoveSplineInit::MovebyPath(std::span<Vector3 const> path, int32 pointId)
    {
        args.path_Idx_offset = pointId;
        args.path.resize(path.size());
        std::ranges::transform(path, args.path.begin(), TransportPathTransform(unit, args.TransformForTransport));
    }

    void MoveSplineInit::MoveTo(float x, float y, float z, bool generatePath, bool forceDestination)
    {
        MoveTo(G3D::Vector3(x, y, z), generatePath, forceDestination);
    }

    void MoveSplineInit::MoveTo(Vector3 const& dest, bool generatePath, bool forceDestination)
    {
        if (generatePath)
        {
            PathGenerator path(unit);
            bool result = path.CalculatePath(dest.x, dest.y, dest.z, forceDestination);
            if (result && !(path.GetPathType() & PATHFIND_NOPATH))
            {
                MovebyPath(path.GetPath());
                return;
            }
        }

        args.path_Idx_offset = 0;
        args.path.resize(2);
        TransportPathTransform transform(unit, args.TransformForTransport);
        args.path[1] = transform(dest);
    }

    void MoveSplineInit::SetFall()
    {
        args.flags.Falling = true;
        args.flags.FallingSlow = unit->HasUnitMovementFlag(MOVEMENTFLAG_FALLING_SLOW);
    }

    void MoveSplineInitFacingVisitor::operator()(Position const& point) const
    {
        _init.SetFacing(point.GetPositionX(), point.GetPositionY(), point.GetPositionZ());
    }
}
