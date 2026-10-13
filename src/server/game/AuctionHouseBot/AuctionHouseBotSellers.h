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

#ifndef AUCTION_HOUSE_BOT_SELLERS_H
#define AUCTION_HOUSE_BOT_SELLERS_H

#include "Define.h"

// Named seller characters for the AuctionHouseBot account (AuctionHouseBot.Sellers.Count).
namespace AuctionBotSellers
{
    // AuctionHouseBot.Sellers.Count > 0: makes sure the ahbot account owns that many named level-60
    // characters (bare rows in the characters table, never logged in), so listings show a seller name.
    // Returns how many characters it created.
    uint32 EnsureCharacters(uint32 accountId);
}

#endif
