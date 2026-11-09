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

#include "../../src/modules/PlayerbotV2/Bot/CcPick.h"

using Playerbot::CcCandidate;
using Playerbot::PickCcTarget;
using Playerbot::kNoCcTarget;

static CcCandidate Eligible(bool kill_focus = false)
{
    CcCandidate c;
    c.in_priority_list = true;
    c.is_kill_focus = kill_focus;
    return c;
}

TEST_CASE("No candidates gives no CC target", "[ClassicCcPick]")
{
    CHECK(PickCcTarget({}) == kNoCcTarget);
}

TEST_CASE("The kill focus is crowd-controlled before an earlier eligible add", "[ClassicCcPick]")
{
    // Deadmines 2026-10-09: the Evoker (focus) got no Polymorph, a lesser caster did.
    CHECK(PickCcTarget({ Eligible(), Eligible(true), Eligible() }) == 1);
}

TEST_CASE("Without a kill focus the first eligible add is taken", "[ClassicCcPick]")
{
    CHECK(PickCcTarget({ Eligible(), Eligible() }) == 0);
}

TEST_CASE("Locked, skull, wrong-type, unlisted and self-attacking adds are skipped", "[ClassicCcPick]")
{
    CcCandidate locked = Eligible(true);   locked.locked = true;
    CcCandidate skull = Eligible(true);    skull.is_skull = true;
    CcCandidate wrong = Eligible(true);    wrong.wrong_type = true;
    CcCandidate hits = Eligible(true);     hits.hits_me = true;
    CcCandidate unlisted = Eligible(true); unlisted.in_priority_list = false;
    CHECK(PickCcTarget({ locked, skull, wrong, hits, unlisted }) == kNoCcTarget);
    CHECK(PickCcTarget({ locked, skull, wrong, hits, unlisted, Eligible() }) == 5);
}
