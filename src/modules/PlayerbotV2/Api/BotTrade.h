// BotTrade - what a bot does when a player trades with it, as pure decisions.
//
// A bot trades in three ways:
//   gift      - the owner or a group mate hands over gear that is an upgrade for the bot, or money: accepted for nothing.
//   buy       - a player puts an item in and nothing else: the bot offers gold and takes the item (the market price for gear
//               that is an upgrade for it, the vendor price for anything else).
//   sell      - a player ordered an item from the bot's list (whisper "what do you have", then "buy 2") and puts the price in:
//               the bot adds that item to its side and accepts.
// The live state (Player, TradeData, auction house) is read by BotTradeShop.cpp; this header only decides.
// Tested in tests/game/ClassicBotTrade.cpp.

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace Playerbot::BotTrade {

// ---- price ------------------------------------------------------------------------------------------------------------
struct PriceInputs
{
    std::uint64_t ah_unit = 0;        // median per-unit buyout of the live listings, 0 when the auction house has none
    std::uint64_t vendor_sell = 0;    // what a vendor pays per unit
    std::uint64_t vendor_buy = 0;     // what a vendor charges per unit
    bool poor = false;                // grey quality
};

// The market value of one unit: the auction house's when it lists the item, otherwise derived from the vendor price (greys are
// worth what a vendor pays; everything else what a vendor charges, or four times what it pays when the vendor does not sell
// it - the vanilla ratio). 0 means worthless.
constexpr std::uint64_t FairUnitPrice(PriceInputs const& in)
{
    if (in.ah_unit > 0) return in.ah_unit;
    if (in.poor) return in.vendor_sell;
    if (in.vendor_buy > 0) return in.vendor_buy;
    return in.vendor_sell * 4u;
}

// What a bot pays per unit for an item a player sells it. Gear that is an upgrade for the bot is worth its market price to it;
// anything else it can only sell on to a vendor, so it pays what the vendor would. Otherwise a player farms gold by selling
// bots greens they cannot use at four times the vendor price.
constexpr std::uint64_t PaidUnitPrice(std::uint64_t fair_unit, std::uint64_t vendor_sell, bool upgrade)
{
    return upgrade ? fair_unit : std::min(fair_unit, vendor_sell);
}

// Median of the listings' unit prices; 0 for none. Resists the one absurd listing that a lowest-price rule would copy.
inline std::uint64_t MedianPrice(std::vector<std::uint64_t> v)
{
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    std::size_t const n = v.size();
    return (n & 1u) ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2u;
}

// "1g 20s 5c", zero parts left out, "0c" for nothing.
inline std::string FormatMoney(std::uint64_t copper)
{
    std::uint64_t const g = copper / 10000u, s = (copper / 100u) % 100u, c = copper % 100u;
    std::string out;
    auto add = [&out](std::uint64_t n, char unit)
    {
        if (!n) return;
        if (!out.empty()) out += ' ';
        out += std::to_string(n);
        out += unit;
    };
    add(g, 'g');
    add(s, 's');
    add(c, 'c');
    return out.empty() ? std::string("0c") : out;
}

// ---- trade window -------------------------------------------------------------------------------------------------------
struct Line
{
    std::uint32_t entry = 0;
    std::uint32_t count = 0;
    std::uint64_t value = 0;     // fair price of the whole stack; on the player's side what the bot pays for it (PaidUnitPrice)
    bool upgrade = false;        // equippable and better than what the bot wears there (only meaningful on the player's side)
    bool reagent = false;        // a reagent of a recipe the bot knows: a gift from the owner or a group mate, so "trade them to me and ask again" works
    bool later = false;          // gear the bot may wear but only a few levels from now: a gift from the owner or a group mate, kept until then
                                 // (Gremloc 11-08: a group mate's piece above the bot's level was bought at the vendor price)
};

struct View
{
    bool gift_partner = false;               // the owner or a player in the bot's own group or raid
    std::vector<Line> his;                   // what the player put up
    std::uint64_t his_money = 0;
    std::vector<Line> mine;                  // what the bot put up
    std::uint64_t my_money = 0;
    std::uint64_t bot_gold = 0;
    bool his_accepted = false;
    bool my_accepted = false;
    bool has_order = false;                  // the player ordered an item from the bot's list
    std::uint32_t order_entry = 0;
    std::uint64_t order_price = 0;
    bool order_item_present = false;         // the ordered item is still in the bot's bags (or already in the window)
    bool bot_has_room = true;                // the bot can store everything the player put up
    bool has_delivery = false;               // a craft order made for this player is waiting in the bot's bags, to be handed over for free
    bool delivery_present = false;           // that item is still in the bags (or already in the window)
    std::uint32_t delivery_left = 0;         // of the things to hand over (a craft order, or what the owner asked for), how many are
                                             // still in the bags and not in the window yet
};

// The item slots of a trade window that change hands (the seventh only shows an enchant).
constexpr std::size_t kTradeItemSlots = 6;

enum class Act : unsigned char
{
    Wait,           // nothing to do yet
    Accept,         // accept the trade as it stands
    Cancel,         // close the window
    SetMoney,       // put `money` in the bot's side (0 clears it)
    AddOrdered,     // put the ordered item in the bot's side
    AddDelivery,    // put the finished craft order in the bot's side
    ClearMine       // take the bot's items back
};

enum class Say : unsigned char
{
    None,
    OfferBuy,       // "i'll give you <money> for that"
    CantAfford,     // the bot is short of gold for what the player offers
    WorthNothing,   // the items have no value to the bot
    NoRoom,         // the bot's bags are full
    PriceIs,        // the player put in less than the ordered item costs
    OneWay,         // items and gold from the player at once, or gifts mixed with goods
    OrderGone       // the ordered item is no longer there
};

struct Action
{
    Act act = Act::Wait;
    std::uint64_t money = 0;
    Say say = Say::None;
};

inline bool AllGifts(std::vector<Line> const& v)
{
    for (Line const& l : v)
        if (!l.upgrade && !l.reagent && !l.later) return false;
    return true;
}

inline bool AnyGift(std::vector<Line> const& v)
{
    for (Line const& l : v)
        if (l.upgrade || l.reagent || l.later) return true;
    return false;
}

inline std::uint64_t TotalValue(std::vector<Line> const& v)
{
    std::uint64_t t = 0;
    for (Line const& l : v) t += l.value;
    return t;
}

// One step of the bargain. Called every tick while the window is open; returns what to do now, which is Wait whenever the
// window already shows the deal.
inline Action Decide(View const& v)
{
    bool const he_gives = !v.his.empty();

    // A finished craft order that could not go by mail, or the things the owner asked the bot for: handed over for nothing in a
    // window with nothing in his side, one by one, then accepted once he has.
    if (!he_gives && v.has_delivery && v.his_money == 0 && v.my_money == 0)
    {
        if (v.delivery_left > 0 && v.mine.size() < kTradeItemSlots)
            return { Act::AddDelivery };
        if (v.mine.empty())
            return v.delivery_present ? Action{ Act::AddDelivery } : Action{ Act::Cancel, 0, Say::OrderGone };
        if (v.his_accepted && !v.my_accepted) return { Act::Accept };
        return {};
    }

    // Sell to the player: no items from him, an order on the bot's list.
    if (!he_gives && v.has_order)
    {
        bool const placed = !v.mine.empty();
        if (!placed)
        {
            if (v.his_money == 0) return {};
            if (!v.order_item_present) return { Act::Cancel, 0, Say::OrderGone };
            if (v.his_money < v.order_price) return { Act::Wait, 0, Say::PriceIs };
            return { Act::AddOrdered };
        }
        if (v.his_money < v.order_price) return { Act::ClearMine, 0, Say::PriceIs };
        if (v.his_accepted && !v.my_accepted) return { Act::Accept };
        return {};
    }

    // Money alone from the owner or a group mate is a gift.
    if (!he_gives)
    {
        if (v.his_money > 0 && v.mine.empty() && v.my_money == 0 && v.gift_partner)
            return v.his_accepted && !v.my_accepted ? Action{ Act::Accept } : Action{};
        return {};
    }

    // Gear that is an upgrade, or reagents for a recipe the bot knows, from the owner or a group mate: a gift.
    if (v.gift_partner && AllGifts(v.his) && v.mine.empty() && v.my_money == 0)
    {
        if (!v.his_accepted || v.my_accepted) return {};
        return v.bot_has_room ? Action{ Act::Accept } : Action{ Act::Cancel, 0, Say::NoRoom };
    }

    // A mix of gifts and goods, or goods and gold from the player at once, is not a deal the bot can price.
    if (v.his_money > 0 || (v.gift_partner && AnyGift(v.his)))
        return { Act::Wait, 0, Say::OneWay };

    // Buy from the player at the fair price.
    std::uint64_t const total = TotalValue(v.his);
    if (total == 0) return { Act::Cancel, 0, Say::WorthNothing };
    if (v.my_money != total)
    {
        if (v.bot_gold < total) return { Act::Cancel, 0, Say::CantAfford };
        return { Act::SetMoney, total, Say::OfferBuy };
    }
    if (!v.his_accepted || v.my_accepted) return {};
    return v.bot_has_room ? Action{ Act::Accept } : Action{ Act::Cancel, 0, Say::NoRoom };
}

inline char const* SayText(Say s)
{
    switch (s)
    {
        case Say::CantAfford:   return "i can't afford that, my purse is too light";
        case Say::WorthNothing: return "sorry, i've no use for that";
        case Say::NoRoom:       return "sorry, my bags are full";
        case Say::OneWay:       return "one thing at a time please: gear i need is a gift, everything else i buy";
        case Say::OrderGone:    return "sorry, i no longer have that one";
        default:                return "";
    }
}

// ---- bag room ---------------------------------------------------------------------------------------------------------
// Bag slots a bot keeps free so a trade or a loot drop never finds it full.
constexpr std::uint32_t kKeepFreeSlots = 4;

// How many slots to clear: the shortfall against the free slots wanted, 0 when there is enough.
constexpr std::uint32_t SlotsToClear(std::uint32_t free_slots, std::uint32_t wanted = kKeepFreeSlots)
{
    return free_slots >= wanted ? 0u : wanted - free_slots;
}

} // namespace Playerbot::BotTrade
