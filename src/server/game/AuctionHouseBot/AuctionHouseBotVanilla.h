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

#ifndef AUCTION_HOUSE_BOT_VANILLA_H
#define AUCTION_HOUSE_BOT_VANILLA_H

struct ItemTemplate;

// Optional vanilla-era item filter for the AuctionHouseBot seller (AuctionHouseBot.Vanilla.*).
namespace AuctionBotVanilla
{
    // Reads AuctionHouseBot.Vanilla.* (called from the seller's Initialize).
    void LoadConfig();

    // True when the filter is switched on (AuctionHouseBot.Vanilla.Only = 1).
    bool Enabled();

    // False for anything that is not level-60 vanilla content; always true while the filter is off.
    bool IsAllowed(ItemTemplate const* proto);
}

#endif
