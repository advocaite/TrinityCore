// BotTradeShop - the live side of bot trading: what a bot has for sale, what things are worth, and one step of the trade window.
// The decisions are in BotTrade.h (pure, tested); this reads and changes the Player, the TradeData and the auction house.
// World thread only.

#pragma once

#include "Define.h"
#include "ObjectGuid.h"
#include <string>
#include <vector>

class Item;
class Player;
class TradeData;
struct ItemTemplate;

namespace Playerbot
{
enum class Result : uint8;
}

namespace Playerbot::TradeShop {

struct Offer
{
    uint32 entry = 0;
    uint32 count = 0;
    uint64 price = 0;        // for the whole stack, copper
    ObjectGuid item;
    std::string name;
};

// What one stack is worth on the market, for the bot's faction. `entries` are looked up in the live auction house together.
// Items the auction house does not list are priced from their vendor value (BotTrade::FairUnitPrice).
std::vector<uint64> UnitPrices(Player const* viewer, std::vector<uint32> const& entries);

// Gear the bot would wear in place of what it has there (an empty slot counts).
bool IsUpgrade(Player* bot, ItemTemplate const* tmpl);

// Gear the bot can wear but only from a level up to kLaterLevels above its own (class, race and proficiency fit already).
constexpr int32 kLaterLevels = 10;
bool IsForLater(Player const* bot, ItemTemplate const* tmpl);

// The stacks the bot is willing to sell, most valuable first, at most `max` of them: tradeable, no quest items, nothing the bot
// itself lives on (consumables, reagents, ammo) and no gear that would be an upgrade for it.
std::vector<Offer> BuildOffers(Player* bot, uint32 max = 10);

// What the bot last listed to `buyer`, and the order he placed from it (both forgotten after ten minutes).
void StoreList(Player const* bot, Player const* buyer, std::vector<Offer> list);
bool GetList(Player const* bot, Player const* buyer, std::vector<Offer>& out);
bool PlaceOrder(Player const* bot, Player const* buyer, size_t index, Offer& out);

// A finished craft order that cannot go by mail (a conjured item, or the mail failed): when `requester` opens a trade with the bot
// within ten minutes, the bot puts the item (`entry`, from its bags) in the window and accepts once he has. One per bot.
void StoreDelivery(Player const* bot, ObjectGuid requester, uint32 entry);

// Things from the bot's bags for a trade it opens with its owner (or the player leading its group) because he asked for them
// ("trade me some stuff", Bridge "trade"). `wanted` empty or a general word ("stuff", "anything"): what helps him, his quest items
// first, then gear that is an upgrade for him, empty bags, then food, drink, potions and bandages for his level. A category
// ("food", "potions", "bandages", "bags", "gear") or words of an item's name ("linen cloth"): only those. At most a trade
// window's six item slots; only what may change hands (not soulbound, not a bag with things in it).
std::vector<Item*> PickGifts(Player* bot, Player* partner, std::string const& wanted);

// The things a bot hands over for nothing in its next trade window with `partner` (within ten minutes); one list per bot.
void StoreGift(Player const* bot, ObjectGuid partner, std::vector<ObjectGuid> items);

// "1) Linen Cloth x20 - 40s" lines packed into whisper-sized chunks.
std::vector<std::string> Describe(std::vector<Offer> const& list);

// The gear entries the bot took as upgrades (BotTrade::Line::upgrade or ::later) in trades that went through since the last call, and
// forgets them. Any thread.
std::vector<uint32> TakeGiftGear(uint64 bot_id);

// One step of an open trade window; see API::trade_with.
Result Step(Player* bot, TradeData* mine, TradeData* his, bool gift_ok);

} // namespace Playerbot::TradeShop

namespace Playerbot {
// Clears bag rot until `min_free` slots are free (defined with API::make_room).
bool MakeBagRoom(Player* bot, uint32 min_free);
}
