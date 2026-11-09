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

#ifndef TRINITY_BOT_GROUP_RULES_H
#define TRINITY_BOT_GROUP_RULES_H

#include <algorithm>
#include <cstddef>
#include <vector>

// Groups with bot members (WorldSession::IsBot): the players in them keep the loot rights and the quest credit the bots would
// otherwise take. A group without bots follows the normal rules. Pure: no game types.
namespace BotGroupRules
{
    // Calling a fifth companion turns the party into a raid, and a raid group cannot progress or take normal quests. That rule is
    // for raids of players: it holds only while the raid has more players than a party has room for.
    inline bool QuestRaidRuleApplies(bool raidGroup, std::size_t playerMembers, std::size_t partySize)
    {
        return raidGroup && playerMembers > partySize;
    }

    // A tap outside raid maps covers the tapper's subgroup only. In a group with bots every player of the group is covered too, so
    // a bot in another subgroup tagging first no longer leaves its player with no loot on the corpse.
    inline bool SharesTap(bool raidMap, bool sameSubGroup, bool groupHasBot, bool memberIsBot)
    {
        return raidMap || sameSubGroup || (groupHasBot && !memberIsBot);
    }

    // Nothing turns a raid back into a party, so a player who once called a fifth companion kept a raid with one or two bots left
    // (Discord report 2026-10-27: no credit for Lieutenant Benedict with 1-2 bots until he left the group). A raid of players and bots
    // that fits a party again goes back to one, unless someone in it is on a raid map (the instance needs the raid group).
    inline bool RevertBotRaidToParty(bool raidGroup, bool battlegroundOrLfg, std::size_t members, std::size_t partySize, bool anyBot,
        bool anyPlayer, bool anyOnRaidMap)
    {
        return raidGroup && !battlegroundOrLfg && members <= partySize && anyBot && anyPlayer && !anyOnRaidMap;
    }

    struct LooterCandidate
    {
        bool inRange = false;
        bool bot = false;
    };

    // Round robin: the next member after `current` (from the first one when `current` is not a member) in range of the corpse.
    // Bots take a turn only while no player of the group is in range: a bot playing with a player loots just its quest drops, so
    // a corpse on a bot's turn stayed locked to the player. keepCurrent: the current looter keeps the turn while it may still take
    // it. Returns candidates.size() when nobody may.
    inline std::size_t NextRoundRobinLooter(std::vector<LooterCandidate> const& candidates, std::size_t current, bool keepCurrent)
    {
        std::size_t const count = candidates.size();
        bool const playerInRange = std::ranges::any_of(candidates, [](LooterCandidate const& c) { return c.inRange && !c.bot; });
        auto mayLoot = [&](std::size_t i) { return candidates[i].inRange && (!candidates[i].bot || !playerInRange); };

        if (current < count && keepCurrent && mayLoot(current))
            return current;

        std::size_t const start = current < count ? current + 1 : 0;
        for (std::size_t step = 0; step < count; ++step)
            if (std::size_t const i = (start + step) % count; mayLoot(i))
                return i;

        return count;
    }
}

#endif
