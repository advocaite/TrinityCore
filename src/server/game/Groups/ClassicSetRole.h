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

#ifndef TRINITY_CLASSIC_SET_ROLE_H
#define TRINITY_CLASSIC_SET_ROLE_H

#include <cstdint>

// Classic 1.60: who may change a party role with CMSG_SET_ROLE. A player sets their own role; the leader or an assistant may set
// the role of any member of the same group. Outside a group a player can only set their own. The role is a LfgRoles mask
// (leader 0x01, tank 0x02, healer 0x04, damage 0x08); a value with other bits set is not a role. Before this check any client could
// rewrite the role of anyone, in any group, and broadcast it to that group.
namespace ClassicSetRole
{
    constexpr uint8_t ValidRoleBits = 0x01 | 0x02 | 0x04 | 0x08;

    struct Request
    {
        bool InGroup = false;              // the sender is in the group the packet names
        bool TargetIsSelf = false;
        bool TargetIsMember = false;       // the target is a member of that group
        bool SenderIsLeaderOrAssistant = false;
        uint8_t Role = 0;
    };

    inline bool IsAllowed(Request const& r)
    {
        if (r.Role & ~ValidRoleBits)
            return false;
        if (!r.InGroup)
            return r.TargetIsSelf;
        if (!r.TargetIsMember)
            return false;
        return r.TargetIsSelf || r.SenderIsLeaderOrAssistant;
    }
}

#endif
