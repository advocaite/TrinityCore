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

#ifndef AUCTION_HOUSE_BOT_BUYER_POLICY_H
#define AUCTION_HOUSE_BOT_BUYER_POLICY_H

#include "Define.h"
#include <string>

struct AuctionPosting;

// Optional buying rules for the AuctionHouseBot buyer (AuctionHouseBot.Buyer.*). All off by default.
namespace AuctionBotBuyerPolicy
{
    TC_GAME_API void LoadConfig();

    // Buyer.Stacks: stackable listings (herbs, ore, cloth...) can be bought, always the whole listing.
    TC_GAME_API bool StacksEnabled();

    // False while the listing is still too fresh (a stable per-listing delay between
    // Buyer.MinDelay and Buyer.MaxDelay seconds) or an hourly/daily limit is spent.
    TC_GAME_API bool Eligible(AuctionPosting const* auction);

    // Buyer.FairPricing: roll by fair value instead of the stock chance (see RollFair).
    TC_GAME_API bool FairPricingEnabled();

    // Fair-price roll for a buyout or bid at unitPrice: certain up to the fair price, then falling
    // linearly to Buyer.FairMinChance percent at the price cap, never above the cap.
    // qualityChance is the Buyer.ChanceMultiplier.<quality> value (100 = unchanged).
    TC_GAME_API bool RollFair(AuctionPosting const* auction, uint64 unitPrice, uint32 qualityChance);

    // Highest price per item the bot pays; 0 = no cap.
    TC_GAME_API uint64 MaxUnitPrice(AuctionPosting const* auction);

    // A listing's price (buyout or bid) per item: a stackable listing's price already is per item.
    TC_GAME_API uint64 PerItemPrice(AuctionPosting const* auction, uint64 price);

    // Total price of buying the whole listing at the given listing price.
    TC_GAME_API uint64 TotalPrice(AuctionPosting const* auction, uint64 unitPrice);

    // Updates the hourly/daily counters after a buyout.
    TC_GAME_API void RecordBuy(AuctionPosting const* auction, uint64 totalPrice, char const* house);

    // Buyer spend today and limits, for .ahbot stats
    TC_GAME_API std::string Describe();
}

#endif
