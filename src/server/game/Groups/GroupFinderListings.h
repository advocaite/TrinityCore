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

#ifndef TRINITY_GROUP_FINDER_LISTINGS_H
#define TRINITY_GROUP_FINDER_LISTINGS_H

#include "Define.h"

#include <string>
#include <vector>

class Group;
class Player;

// The Classic Group Finder listings (GuildHandler.cpp) write every change to the "groupfinder" log, one JSON line each, for the
// Discord bot's LFG channels (contrib/discord_bot). These keep a posted group current when its members change.
namespace GroupFinderListings
{
    TC_GAME_API void OnGroupChanged(Group const* group);
    TC_GAME_API void OnLogout(Player const* player);

    // The module lists a group a bot leads (the same list a player's own listing goes in; no packet of its own). The play style is the
    // Group Finder dropdown value: 1 Learning, 2 Relaxed, 3 Competitive, 4 Carry Offered.
    TC_GAME_API void PostBotListing(Player const* leader, uint32 categoryId, std::vector<uint32> const& activities,
                                    std::string const& name, std::string const& comment, uint8 playStyle);
    TC_GAME_API void RemoveBotListing(Player const* leader);
    // The category a player's own listing was posted in (0 = none): the module learns the dungeon category from it.
    TC_GAME_API uint32 CategoryOf(Player const* leader);
}

#endif
