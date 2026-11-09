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

#include "../../src/modules/PlayerbotV2/Bot/PackPullGate.h"

using Playerbot::PackPullGate;

TEST_CASE("PackPullGate: two packs across the gap are both passed by", "[packpullgate]")
{
    // RC1 rfc_oggleflint: the tank in the tunnel below two packs on the corridor above, each 160 y of walk away. One remembered
    // mob let the second verdict undo the first, and the pack stop held the tank for good.
    PackPullGate gate;
    gate.Note(9, true, 1000);
    gate.Note(14, true, 1200);
    CHECK(gate.Get(9, 1300) == PackPullGate::Verdict::Gated);
    CHECK(gate.Get(14, 1300) == PackPullGate::Verdict::Gated);
    CHECK(gate.Get(22, 1300) == PackPullGate::Verdict::Unknown);
}

TEST_CASE("PackPullGate: verdicts are measured again once their window ends", "[packpullgate]")
{
    PackPullGate gate;
    gate.Note(9, true, 1000);
    gate.Note(5, false, 1000);
    CHECK(gate.Get(5, 1000 + PackPullGate::kWalkableMs - 1) == PackPullGate::Verdict::Walkable);
    CHECK(gate.Get(5, 1000 + PackPullGate::kWalkableMs) == PackPullGate::Verdict::Unknown);
    CHECK(gate.Get(9, 1000 + PackPullGate::kGatedMs - 1) == PackPullGate::Verdict::Gated);
    CHECK(gate.Get(9, 1000 + PackPullGate::kGatedMs) == PackPullGate::Verdict::Unknown);
    // A note prunes what has run out; a fresh verdict replaces the old one.
    gate.Note(9, false, 20000);
    CHECK(gate.Get(9, 20001) == PackPullGate::Verdict::Walkable);
    CHECK(gate.Get(5, 20001) == PackPullGate::Verdict::Unknown);
}

TEST_CASE("PackPullGate: the game clock wrapping does not keep a verdict for ever", "[packpullgate]")
{
    PackPullGate gate;
    gate.Note(9, true, 0xFFFFF000u);
    CHECK(gate.Get(9, 0xFFFFF100u) == PackPullGate::Verdict::Gated);
    CHECK(gate.Get(9, 0xFFFFF000u + PackPullGate::kGatedMs) == PackPullGate::Verdict::Unknown);
}
