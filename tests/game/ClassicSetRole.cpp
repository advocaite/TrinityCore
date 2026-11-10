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

#include "../../src/server/game/Groups/ClassicSetRole.h"

using ClassicSetRole::IsAllowed;
using ClassicSetRole::Request;

namespace
{
Request Make(bool inGroup, bool self, bool member, bool leader, uint8_t role)
{
    Request r;
    r.InGroup = inGroup;
    r.TargetIsSelf = self;
    r.TargetIsMember = member;
    r.SenderIsLeaderOrAssistant = leader;
    r.Role = role;
    return r;
}
}

TEST_CASE("A member sets their own role, not another member's", "[ClassicSetRole]")
{
    CHECK(IsAllowed(Make(true, true, true, false, 0x04)));
    CHECK_FALSE(IsAllowed(Make(true, false, true, false, 0x04)));
}

TEST_CASE("The leader or an assistant sets any member's role", "[ClassicSetRole]")
{
    CHECK(IsAllowed(Make(true, false, true, true, 0x02)));
    CHECK(IsAllowed(Make(true, false, true, true, 0x00)));   // clearing a role
}

TEST_CASE("Nobody sets the role of someone outside the group", "[ClassicSetRole]")
{
    CHECK_FALSE(IsAllowed(Make(true, false, false, true, 0x08)));
    CHECK_FALSE(IsAllowed(Make(false, false, false, false, 0x08)));
}

TEST_CASE("Outside a group a player sets only their own role", "[ClassicSetRole]")
{
    CHECK(IsAllowed(Make(false, true, false, false, 0x08)));
}

TEST_CASE("A value with bits outside the role mask is not a role", "[ClassicSetRole]")
{
    CHECK(IsAllowed(Make(true, true, true, false, 0x0F)));
    CHECK_FALSE(IsAllowed(Make(true, true, true, true, 0x10)));
    CHECK_FALSE(IsAllowed(Make(false, true, false, false, 0xFF)));
}
