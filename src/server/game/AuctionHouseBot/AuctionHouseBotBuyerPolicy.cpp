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

#include "AuctionHouseBotBuyerPolicy.h"
#include "AuctionHouseMgr.h"
#include "Common.h"
#include "Config.h"
#include "Duration.h"
#include "GameTime.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Random.h"
#include "StringFormat.h"
#include "World.h"
#include <algorithm>

namespace AuctionBotBuyerPolicy
{
namespace
{
    bool _stacks = false;
    float _maxPriceMultiplier = 0.0f; // 0 = no price cap
    uint32 _minDelay = 0;
    uint32 _maxDelay = 0;
    uint64 _dailyBudget = 0;          // copper, 0 = unlimited
    uint32 _maxBuysPerHour = 0;       // 0 = unlimited
    bool _fair = false;
    float _fairMinChance = 35.0f;     // percent at the price cap

    time_t _dayStart = 0;
    uint64 _spentToday = 0;
    uint32 _boughtToday = 0;
    time_t _hourStart = 0;
    uint32 _boughtThisHour = 0;

    void RollWindows(time_t now)
    {
        if (!_dayStart || now - _dayStart >= DAY)
        {
            _dayStart = now;
            _spentToday = 0;
            _boughtToday = 0;
        }
        if (!_hourStart || now - _hourStart >= HOUR)
        {
            _hourStart = now;
            _boughtThisHour = 0;
        }
    }

    // What the item is worth per unit: the vendor buy price, or four times the vendor sell price
    // (the same stand-in the seller uses when an item has no buy price).
    uint64 FairUnitPrice(ItemTemplate const* proto)
    {
        uint64 buy = proto->GetBuyPrice() / std::max<uint32>(1, proto->GetBuyCount());
        uint64 sell = uint64(proto->GetSellPrice()) * 4;
        return std::max<uint64>({ buy, sell, SILVER });
    }
}

void LoadConfig()
{
    _stacks = sConfigMgr->GetBoolDefault("AuctionHouseBot.Buyer.Stacks", false);
    _maxPriceMultiplier = sConfigMgr->GetFloatDefault("AuctionHouseBot.Buyer.MaxPriceMultiplier", 0.0f);
    _minDelay = sConfigMgr->GetIntDefault("AuctionHouseBot.Buyer.MinDelay", 0);
    _maxDelay = std::max(_minDelay, uint32(sConfigMgr->GetIntDefault("AuctionHouseBot.Buyer.MaxDelay", 0)));
    _dailyBudget = uint64(sConfigMgr->GetIntDefault("AuctionHouseBot.Buyer.DailyBudgetGold", 0)) * GOLD;
    _maxBuysPerHour = sConfigMgr->GetIntDefault("AuctionHouseBot.Buyer.MaxBuysPerHour", 0);
    _fair = sConfigMgr->GetBoolDefault("AuctionHouseBot.Buyer.FairPricing", false);
    _fairMinChance = std::clamp(sConfigMgr->GetFloatDefault("AuctionHouseBot.Buyer.FairMinChance", 35.0f), 0.0f, 100.0f);
}

bool StacksEnabled()
{
    return _stacks;
}

bool Eligible(AuctionPosting const* auction)
{
    time_t const now = GameTime::GetGameTime();
    RollWindows(now);

    if (_maxBuysPerHour && _boughtThisHour >= _maxBuysPerHour)
        return false;
    if (_dailyBudget && _spentToday >= _dailyBudget)
        return false;

    // Leave new listings alone for a while; the delay differs per listing but never changes for one.
    uint32 const span = _maxDelay - _minDelay;
    uint32 const delay = _minDelay + (span ? (auction->Id * 2654435761u) % (span + 1) : 0);
    return now - time_t(std::chrono::duration_cast<Seconds>(auction->StartTime.time_since_epoch()).count()) >= time_t(delay);
}

bool FairPricingEnabled()
{
    return _fair;
}

bool RollFair(AuctionPosting const* auction, uint64 unitPrice, uint32 qualityChance)
{
    if (!unitPrice || auction->Items.empty())
        return false;

    uint64 const fair = FairUnitPrice(auction->Items[0]->GetTemplate());
    uint64 const cap = _maxPriceMultiplier > 0.0f ? uint64(fair * _maxPriceMultiplier) : fair * 3;

    float chance;
    if (unitPrice <= fair)
        chance = 100.0f;
    else if (unitPrice >= cap || cap <= fair)
        chance = unitPrice == cap ? _fairMinChance : 0.0f;
    else
        chance = 100.0f - (100.0f - _fairMinChance) * float(unitPrice - fair) / float(cap - fair);

    chance *= qualityChance / 100.0f;
    bool const win = frand(0.0f, 100.0f) < chance;
    TC_LOG_DEBUG("ahbot", "AHBot: fair roll {}: price {} fair {} cap {} chance {:.1f}% -> {}",
        auction->Items[0]->GetTemplate()->GetName(sWorld->GetDefaultDbcLocale()), unitPrice, fair, cap, chance, win ? "buy" : "pass");
    return win;
}

uint64 MaxUnitPrice(AuctionPosting const* auction)
{
    if (_maxPriceMultiplier <= 0.0f || auction->Items.empty())
        return 0;
    return uint64(FairUnitPrice(auction->Items[0]->GetTemplate()) * _maxPriceMultiplier);
}

uint64 PerItemPrice(AuctionPosting const* auction, uint64 price)
{
    if (auction->IsCommodity())
        return price;
    uint32 const count = auction->GetTotalItemCount();
    return count > 1 ? price / count : price;
}

uint64 TotalPrice(AuctionPosting const* auction, uint64 unitPrice)
{
    return auction->IsCommodity() ? unitPrice * auction->GetTotalItemCount() : unitPrice;
}

void RecordBuy(AuctionPosting const* auction, uint64 totalPrice, char const* house)
{
    RollWindows(GameTime::GetGameTime());
    _spentToday += totalPrice;
    ++_boughtToday;
    ++_boughtThisHour;

    TC_LOG_DEBUG("ahbot", "AHBot: buy {} x{} for {:.2f}g in {} (today: {} buys, {:.2f}g)",
        auction->Items[0]->GetTemplate()->GetName(sWorld->GetDefaultDbcLocale()), auction->GetTotalItemCount(),
        float(totalPrice) / float(GOLD), house, _boughtToday, float(_spentToday) / float(GOLD));
}

std::string Describe()
{
    return Trinity::StringFormat("AuctionHouseBot buyer today: {} buys, {:.2f}g spent (budget {}g, max {} per hour, price cap x{:.2f}, stacks {})",
        _boughtToday, float(_spentToday) / float(GOLD), _dailyBudget / GOLD, _maxBuysPerHour, _maxPriceMultiplier, _stacks ? "on" : "off");
}
}
