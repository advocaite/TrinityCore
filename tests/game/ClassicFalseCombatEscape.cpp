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

#include "../../src/modules/PlayerbotV2/Bot/FalseCombatEscape.h"

using namespace Playerbot::FalseCombatEscape;

namespace
{
Inputs Held(bool autopilot, bool tank)
{
    Inputs in;
    in.autopilot = autopilot;
    in.is_tank = tank;
    in.tank_known = !tank;
    in.far_from_boss = true;
    return in;
}
}

TEST_CASE("FalseCombatEscape: the autopilot tank walks on to the boss it owes", "[falsecombat]")
{
    CHECK(Pick(Held(true, true)) == Goal::BossWaypoint);
    Inputs in = Held(true, false);
    CHECK(Pick(in) == Goal::Tank);
}

TEST_CASE("FalseCombatEscape: a bot whose autopilot is suspended never walks to a boss", "[falsecombat]")
{
    // UBRS 2026-10-26: a passive companion tank, its owner leading, walked to the first boss's waypoint and pulled every pack.
    for (bool tank : { true, false })
        for (bool far : { true, false })
            for (bool immune : { true, false })
                for (bool leader : { true, false })
                {
                    Inputs in = Held(false, tank);
                    in.far_from_boss = far;
                    in.immune_lock = immune;
                    in.leader_known = leader;
                    Goal const g = Pick(in);
                    CHECK(g != Goal::BossWaypoint);
                    CHECK(g != Goal::Tank);
                    CHECK(g == (leader ? Goal::Leader : Goal::Disengage));
                }
}

TEST_CASE("FalseCombatEscape: an immune lock or a boss close by drops the fight in place", "[falsecombat]")
{
    Inputs in = Held(true, true);
    in.immune_lock = true;
    CHECK(Pick(in) == Goal::Disengage);
    in = Held(true, false);
    in.far_from_boss = false;
    CHECK(Pick(in) == Goal::Disengage);
    in = Held(true, false);
    in.tank_known = false;
    CHECK(Pick(in) == Goal::Disengage);
}
