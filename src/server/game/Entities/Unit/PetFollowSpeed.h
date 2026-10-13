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

#ifndef TRINITYCORE_PET_FOLLOW_SPEED_H
#define TRINITYCORE_PET_FOLLOW_SPEED_H

// A pet following its owner scales its run speed with the gap to the owner (up to +30%) to catch up. The scale moves a little
// with every step and is recomputed on every update, so each small change sent the nearby players a speed packet several times
// a second. A catch-up change smaller than this is not sent; the pet keeps the speed it has.
constexpr float PET_CATCH_UP_SPEED_HYSTERESIS = 0.08f;

constexpr bool PetCatchUpSpeedWorthSending(float currentRate, float wantedRate)
{
    return (wantedRate > currentRate ? wantedRate - currentRate : currentRate - wantedRate) >= PET_CATCH_UP_SPEED_HYSTERESIS;
}

#endif // TRINITYCORE_PET_FOLLOW_SPEED_H
