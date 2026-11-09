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

#ifndef TRINITY_CLASSIC_FLEE_ANGLE_H
#define TRINITY_CLASSIC_FLEE_ANGLE_H

#include <cmath>

// Classic 1.60: where a feared or fleeing unit steps next. A unit near its attacker runs away from it, one far away drifts back
// toward it, and one in between wanders. The angles here are world angles (0 = east, counter-clockwise), as GetAbsoluteAngle gives
// them; WorldObject::MovePositionToFirstCollision takes an angle relative to the unit's facing, so the step is converted before it
// is used. Without that conversion the unit ran in a direction that depended on where it happened to face, often straight at the
// attacker.
namespace ClassicFleeAngle
{
    constexpr float MinQuietDistance = 28.0f;
    constexpr float MaxQuietDistance = 43.0f;
    constexpr float Pi = 3.14159265358979323846f;

    // awayAngle: the world angle from the attacker to the fleeing unit. jitter: a roll in [-1, 1] that spreads the step inside its
    // band (a quarter circle near the attacker, a half circle far away). freeAngle: a roll in [0, 2 pi) used inside the quiet band.
    inline float StepWorldAngle(float casterDistance, float awayAngle, float jitter, float freeAngle)
    {
        if (casterDistance < MinQuietDistance)
            return awayAngle + jitter * (Pi / 8.0f);
        if (casterDistance > MaxQuietDistance)
            return awayAngle + Pi + jitter * (Pi / 4.0f);
        return freeAngle;
    }

    // The world angle as an angle relative to a unit facing 'orientation', normalised to [0, 2 pi).
    inline float ToFacingRelative(float worldAngle, float orientation)
    {
        float relative = std::fmod(worldAngle - orientation, 2.0f * Pi);
        if (relative < 0.0f)
            relative += 2.0f * Pi;
        return relative;
    }
}

#endif
