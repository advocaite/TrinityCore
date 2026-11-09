#include "BotTradeShop.h"
#include "BotLanguage.h"
#include "BotItemScorer.h"
#include "BotTrade.h"
#include "PlayerbotAPI.h"
#include "AuctionHouseMgr.h"
#include "GameTime.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TradeData.h"
#include "TradePackets.h"
#include "WorldSession.h"
#include <algorithm>
#include <cctype>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace Playerbot::TradeShop {

namespace {

constexpr uint32 kShelfLifeMs = 10u * 60u * 1000u;
constexpr uint32 kSayRepeatMs = 20000u;

struct Shelf
{
    uint32 stamp_ms = 0;
    std::vector<Offer> list;
    bool has_order = false;
    Offer order;
};

struct Delivery
{
    ObjectGuid requester;
    uint32 entry = 0;
    uint32 stamp_ms = 0;
};

struct Gift
{
    ObjectGuid partner;
    std::vector<ObjectGuid> items;
    uint32 stamp_ms = 0;
};

std::mutex g_mtx;
std::unordered_map<uint64, Delivery> g_deliveries;                     // bot -> the craft order waiting to be handed over
std::unordered_map<uint64, Gift> g_gifts;                              // bot -> what its owner asked for, for the window it opened
std::unordered_map<uint64, Shelf> g_shelves;                           // (bot << 32) | buyer
std::unordered_map<uint64, std::pair<uint64, uint32>> g_last_say;      // bot -> (what was said, when)
std::unordered_map<uint64, std::vector<uint32>> g_gift_gear;           // bot -> gear entries it took in a finished trade as upgrades

uint64 Key(Player const* bot, Player const* buyer)
{
    return (uint64(bot->GetGUID().GetCounter()) << 32) | uint64(buyer->GetGUID().GetCounter());
}

std::string NameOf(ItemTemplate const* t)
{
    return t ? t->GetName(DEFAULT_LOCALE) : std::string("?");
}

bool IsTradeable(Item* it)
{
    return it && !it->IsNotEmptyBag() && it->CanBeTraded(false, true);
}

// Items of the classes a bot lives on or that belong to a quest are never for sale.
bool KeptByBot(ItemTemplate const* t)
{
    switch (t->GetClass())
    {
        case ITEM_CLASS_CONSUMABLE:
        case ITEM_CLASS_REAGENT:
        case ITEM_CLASS_PROJECTILE:
        case ITEM_CLASS_QUEST:
        case ITEM_CLASS_KEY:
            return true;
        default:
            return t->GetStartQuest() != 0 || t->GetBonding() == BIND_QUEST;
    }
}

void Say(Player* bot, Player* to, BotTrade::Say what, uint64 money, std::string const& text)
{
    if (text.empty()) return;
    uint64 const key = (uint64(what) << 56) ^ money;
    uint32 const now = GameTime::GetGameTimeMS();
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        auto& last = g_last_say[bot->GetGUID().GetCounter()];
        if (last.first == key && now - last.second < kSayRepeatMs) return;
        last = { key, now };
    }
    BotLanguage::Whisper(bot, to, text);   // in the buyer's language
}

} // namespace

std::vector<uint64> UnitPrices(Player const* viewer, std::vector<uint32> const& entries)
{
    std::unordered_set<uint32> const wanted(entries.begin(), entries.end());
    std::unordered_map<uint32, std::vector<uint64>> listed;
    if (viewer && !wanted.empty())
        if (AuctionHouseObject* ah = sAuctionMgr->GetAuctionsMap(viewer->GetFaction()))
            for (auto it = ah->GetAuctionsBegin(); it != ah->GetAuctionsEnd(); ++it)
            {
                AuctionPosting const& a = it->second;
                if (a.Items.empty() || !a.Items[0] || !wanted.count(a.Items[0]->GetEntry()) || a.BuyoutOrUnitPrice == 0) continue;
                uint32 const n = a.GetTotalItemCount();
                if (!n) continue;
                listed[a.Items[0]->GetEntry()].push_back(a.IsCommodity() ? a.BuyoutOrUnitPrice : a.BuyoutOrUnitPrice / n);
            }

    std::vector<uint64> out;
    out.reserve(entries.size());
    for (uint32 entry : entries)
    {
        ItemTemplate const* t = sObjectMgr->GetItemTemplate(entry);
        if (!t) { out.push_back(0); continue; }
        BotTrade::PriceInputs in;
        auto f = listed.find(entry);
        if (f != listed.end()) in.ah_unit = BotTrade::MedianPrice(f->second);
        in.vendor_sell = t->GetSellPrice();
        in.vendor_buy = t->GetBuyPrice() / t->GetBuyCount();
        in.poor = t->GetQuality() == ITEM_QUALITY_POOR;
        out.push_back(BotTrade::FairUnitPrice(in));
    }
    return out;
}

bool IsForLater(Player const* bot, ItemTemplate const* t)
{
    if (!bot || !t || (t->GetClass() != ITEM_CLASS_WEAPON && t->GetClass() != ITEM_CLASS_ARMOR) || t->GetInventoryType() == INVTYPE_NON_EQUIP)
        return false;
    int32 const need = t->GetBaseRequiredLevel();
    int32 const have = int32(bot->GetLevel());
    if (need <= have || need > have + kLaterLevels)
        return false;
    // Everything else (class, race, proficiency) must already fit: only the level is missing.
    return bot->CanUseItem(t, /*skipRequiredLevelCheck*/ true) == EQUIP_ERR_OK;
}

bool IsUpgrade(Player* bot, ItemTemplate const* t)
{
    if (!bot || !t || (t->GetClass() != ITEM_CLASS_WEAPON && t->GetClass() != ITEM_CLASS_ARMOR)) return false;
    uint16 dest = 0;
    if (bot->CanEquipNewItem(NULL_SLOT, dest, t->GetId(), false) != EQUIP_ERR_OK) return false;
    uint8 const slot = dest & 0xFF;
    if (slot >= EQUIPMENT_SLOT_END) return false;
    Item const* cur = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!cur) return true;
    return Gear::ScoreItemForBot(t, bot) > Gear::ScoreItemForBot(cur->GetTemplate(), bot);
}

std::vector<Offer> BuildOffers(Player* bot, uint32 max)
{
    std::vector<Offer> out;
    if (!bot) return out;
    std::vector<Item*> stacks;
    bot->ForEachItem(ItemSearchLocation::Inventory, [&](Item* it)
    {
        ItemTemplate const* t = it ? it->GetTemplate() : nullptr;
        if (t && IsTradeable(it) && !KeptByBot(t) && !IsUpgrade(bot, t))
            stacks.push_back(it);
        return ItemSearchCallbackResult::Continue;
    });
    std::vector<uint32> entries;
    for (Item* it : stacks) entries.push_back(it->GetEntry());
    std::vector<uint64> const unit = UnitPrices(bot, entries);
    for (size_t i = 0; i < stacks.size(); ++i)
    {
        if (!unit[i]) continue;
        Offer o;
        o.entry = stacks[i]->GetEntry();
        o.count = stacks[i]->GetCount();
        o.price = unit[i] * o.count;
        o.item = stacks[i]->GetGUID();
        o.name = NameOf(stacks[i]->GetTemplate());
        out.push_back(std::move(o));
    }
    std::sort(out.begin(), out.end(), [](Offer const& a, Offer const& b) { return a.price > b.price; });
    if (out.size() > max) out.resize(max);
    return out;
}

std::vector<uint32> TakeGiftGear(uint64 bot_id)
{
    std::lock_guard<std::mutex> lk(g_mtx);
    auto it = g_gift_gear.find(bot_id);
    if (it == g_gift_gear.end()) return {};
    std::vector<uint32> out = std::move(it->second);
    g_gift_gear.erase(it);
    return out;
}

void StoreList(Player const* bot, Player const* buyer, std::vector<Offer> list)
{
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_shelves.size() > 4096) g_shelves.clear();
    Shelf& s = g_shelves[Key(bot, buyer)];
    s = Shelf{};
    s.stamp_ms = GameTime::GetGameTimeMS();
    s.list = std::move(list);
}

bool GetList(Player const* bot, Player const* buyer, std::vector<Offer>& out)
{
    std::lock_guard<std::mutex> lk(g_mtx);
    auto it = g_shelves.find(Key(bot, buyer));
    if (it == g_shelves.end() || GameTime::GetGameTimeMS() - it->second.stamp_ms > kShelfLifeMs) return false;
    out = it->second.list;
    return true;
}

bool PlaceOrder(Player const* bot, Player const* buyer, size_t index, Offer& out)
{
    std::lock_guard<std::mutex> lk(g_mtx);
    auto it = g_shelves.find(Key(bot, buyer));
    if (it == g_shelves.end() || index >= it->second.list.size()) return false;
    it->second.order = it->second.list[index];
    it->second.has_order = true;
    it->second.stamp_ms = GameTime::GetGameTimeMS();
    out = it->second.order;
    return true;
}

namespace {
// What eating or drinking the item gives back while sitting: mana (a drink), health (food), or both.
void RegenOf(ItemTemplate const* t, bool& mana, bool& health)
{
    mana = health = false;
    for (ItemEffectEntry const* e : t->Effects)
        if (e)
            if (SpellInfo const* si = sSpellMgr->GetSpellInfo(uint32(e->SpellID), DIFFICULTY_NONE))
            {
                if (si->HasAura(SPELL_AURA_MOD_POWER_REGEN))
                    mana = true;
                if (si->HasAura(SPELL_AURA_MOD_REGEN))
                    health = true;
            }
}
} // namespace

std::vector<Item*> PickGifts(Player* bot, Player* partner, std::string const& wanted)
{
    std::vector<Item*> out;
    if (!bot || !partner) return out;
    // The words of the ask, lower case.
    std::vector<std::string> words;
    {
        std::string w;
        for (char c : wanted + " ")
        {
            unsigned char const u = static_cast<unsigned char>(c);
            if (std::isalnum(u) || u >= 0x80 || c == '\'') w += char(std::tolower(u));
            else if (!w.empty()) { words.push_back(std::move(w)); w.clear(); }
        }
    }
    static constexpr std::string_view kSmall[] = { "some", "me", "the", "a", "an", "your", "you", "of", "and", "my", "please", "pls",
                                                  "give", "trade", "few", "couple", "spare", "extra", "any", "all" };
    std::erase_if(words, [](std::string const& w) { return std::find(std::begin(kSmall), std::end(kSmall), w) != std::end(kSmall); });
    auto has = [&](std::initializer_list<std::string_view> any)
    {
        for (std::string const& w : words)
            for (std::string_view a : any)
                if (w == a) return true;
        return false;
    };
    bool const general = words.empty() || has({ "stuff", "things", "anything", "something", "everything", "useful", "loot", "items" });
    // Water and food apart: "trade water" from a mage with only cheese in its bags handed over the cheese (Karrz, 2026-11-08).
    bool const want_food = has({ "food", "eat", "bread", "consumables" });
    bool const want_drink = has({ "drink", "drinks", "water", "consumables" });
    bool const want_potions = has({ "potion", "potions", "pots", "elixir", "elixirs", "flask", "flasks", "consumables" });
    bool const want_bandages = has({ "bandage", "bandages", "consumables" });
    bool const want_bags = has({ "bag", "bags" });
    bool const want_gear = has({ "gear", "upgrade", "upgrades", "armor", "armour", "weapon", "weapons" });
    bool const by_category = general || want_food || want_drink || want_potions || want_bandages || want_bags || want_gear;

    uint8 const level = partner->GetLevel();
    auto for_level = [&](ItemTemplate const* t) { return t->GetBaseRequiredLevel() <= int32(level); };
    // Ranks: 0 his quest items, 1 upgrades for him, 2 bags, 3 consumables (higher required level first inside a rank).
    struct Pick { Item* item; int rank; int32 req; };
    std::vector<Pick> picks;
    bot->ForEachItem(ItemSearchLocation::Inventory, [&](Item* it)
    {
        ItemTemplate const* t = it ? it->GetTemplate() : nullptr;
        if (!t || !IsTradeable(it) || it->IsBindedNotWith(partner)) return ItemSearchCallbackResult::Continue;
        if (!by_category)
        {
            std::string name = NameOf(t);
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            bool all = true;
            for (std::string const& w : words)
                if (name.find(w) == std::string::npos) { all = false; break; }
            if (all) picks.push_back({ it, 0, 0 });
            return ItemSearchCallbackResult::Continue;
        }
        uint32 const cls = t->GetClass(), sub = t->GetSubClass();
        bool const food_drink = cls == ITEM_CLASS_CONSUMABLE && sub == ITEM_SUBCLASS_FOOD_DRINK;
        bool gives_mana = false, gives_health = false;
        if (food_drink)
            RegenOf(t, gives_mana, gives_health);
        bool const fits_food_ask = food_drink && ((want_food && (gives_health || !gives_mana)) || (want_drink && gives_mana));
        bool const potion = cls == ITEM_CLASS_CONSUMABLE && (sub == ITEM_SUBCLASS_POTION || sub == ITEM_SUBCLASS_ELIXIR || sub == ITEM_SUBCLASS_FLASK);
        bool const bandage = cls == ITEM_CLASS_CONSUMABLE && sub == ITEM_SUBCLASS_BANDAGE;
        if (general && partner->HasQuestForItem(t->GetId()))
            picks.push_back({ it, 0, 0 });
        else if ((general || want_gear) && IsUpgrade(partner, t) && partner->CanUseItem(t) == EQUIP_ERR_OK)
            picks.push_back({ it, 1, t->GetBaseRequiredLevel() });
        else if ((general || want_bags) && cls == ITEM_CLASS_CONTAINER && sub == 0)
            picks.push_back({ it, 2, int32(t->GetContainerSlots()) });
        else if ((general && food_drink) || fits_food_ask || ((general || want_potions) && potion) || ((general || want_bandages) && bandage))
        {
            if (for_level(t))
                picks.push_back({ it, 3, t->GetBaseRequiredLevel() });
        }
        return ItemSearchCallbackResult::Continue;
    });
    std::stable_sort(picks.begin(), picks.end(), [](Pick const& a, Pick const& b) { return a.rank != b.rank ? a.rank < b.rank : a.req > b.req; });
    for (Pick const& p : picks)
    {
        if (out.size() >= TRADE_SLOT_TRADED_COUNT) break;
        out.push_back(p.item);
    }
    return out;
}

void StoreGift(Player const* bot, ObjectGuid partner, std::vector<ObjectGuid> items)
{
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_gifts.size() > 4096) g_gifts.clear();
    g_gifts[bot->GetGUID().GetCounter()] = Gift{ partner, std::move(items), GameTime::GetGameTimeMS() };
}

void StoreDelivery(Player const* bot, ObjectGuid requester, uint32 entry)
{
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_deliveries.size() > 4096) g_deliveries.clear();
    g_deliveries[bot->GetGUID().GetCounter()] = Delivery{ requester, entry, GameTime::GetGameTimeMS() };
}

std::vector<std::string> Describe(std::vector<Offer> const& list)
{
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i < list.size(); ++i)
    {
        std::string line = std::to_string(i + 1) + ") " + list[i].name;
        if (list[i].count > 1) line += " x" + std::to_string(list[i].count);
        line += " - " + BotTrade::FormatMoney(list[i].price);
        if (!cur.empty() && cur.size() + line.size() + 2 > 200)
        {
            out.push_back(std::move(cur));
            cur.clear();
        }
        cur += cur.empty() ? line : "; " + line;
    }
    if (!cur.empty()) out.push_back(std::move(cur));
    return out;
}

namespace {

// Is `entry` a reagent of a recipe (a spell that makes an item) the bot knows? Reagents the owner hands over for a craft order are a gift.
bool IsRecipeReagent(Player const* bot, uint32 entry)
{
    for (auto const& [spell_id, ps] : bot->GetSpellMap())
    {
        if (ps.state == PLAYERSPELL_REMOVED || !ps.active || ps.disabled)
            continue;
        SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
        if (!si || !si->HasEffect(SPELL_EFFECT_CREATE_ITEM))
            continue;
        for (std::size_t i = 0; i < si->Reagent.size(); ++i)
            if (si->Reagent[i] > 0 && si->ReagentCount[i] > 0 && uint32(si->Reagent[i]) == entry)
                return true;
    }
    return false;
}

BotTrade::Line LineOf(Player const* viewer_for_price, Item* it, uint64 unit)
{
    (void)viewer_for_price;
    BotTrade::Line l;
    l.entry = it->GetEntry();
    l.count = it->GetCount();
    l.value = unit * it->GetCount();
    return l;
}

// The lines of one side of the window.
std::vector<BotTrade::Line> Lines(Player* bot, TradeData* side, bool mark_upgrades, std::vector<Item*>* items_out)
{
    std::vector<Item*> items;
    for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
        if (Item* it = side->GetItem(TradeSlots(i)))
            items.push_back(it);
    std::vector<uint32> entries;
    for (Item* it : items) entries.push_back(it->GetEntry());
    std::vector<uint64> const unit = UnitPrices(bot, entries);
    std::vector<BotTrade::Line> out;
    for (size_t i = 0; i < items.size(); ++i)
    {
        BotTrade::Line l = LineOf(bot, items[i], unit[i]);
        if (mark_upgrades)
        {
            ItemTemplate const* t = items[i]->GetTemplate();
            l.upgrade = IsUpgrade(bot, t);
            l.reagent = IsRecipeReagent(bot, items[i]->GetEntry());
            l.later = !l.upgrade && IsForLater(bot, t);
            l.value = BotTrade::PaidUnitPrice(unit[i], t->GetSellPrice(), l.upgrade) * items[i]->GetCount();
        }
        out.push_back(l);
    }
    if (items_out) *items_out = std::move(items);
    return out;
}

bool HasRoomFor(Player* bot, std::vector<Item*>& items)
{
    if (items.empty()) return true;
    uint32 offending = 0;
    if (bot->CanStoreItems(items.data(), int(items.size()), &offending) == EQUIP_ERR_OK) return true;
    // Bags full: clear what a player would vendor, then ask again.
    MakeBagRoom(bot, uint32(items.size()) + BotTrade::kKeepFreeSlots);
    return bot->CanStoreItems(items.data(), int(items.size()), &offending) == EQUIP_ERR_OK;
}

} // namespace

Result Step(Player* bot, TradeData* mine, TradeData* his, bool gift_ok)
{
    Player* partner = mine->GetTrader();
    if (!partner) return Result::Locked;

    BotTrade::View v;
    v.gift_partner = gift_ok;
    std::vector<Item*> his_items;
    v.his = Lines(bot, his, /*mark_upgrades*/ true, &his_items);
    v.his_money = his->GetMoney();
    std::vector<Item*> my_items;
    v.mine = Lines(bot, mine, false, &my_items);
    v.my_money = mine->GetMoney();
    v.bot_gold = bot->GetMoney();
    v.his_accepted = his->IsAccepted();
    v.my_accepted = mine->IsAccepted();
    v.bot_has_room = true;

    Offer order;
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        auto it = g_shelves.find(Key(bot, partner));
        if (it != g_shelves.end() && it->second.has_order && GameTime::GetGameTimeMS() - it->second.stamp_ms <= kShelfLifeMs)
        {
            order = it->second.order;
            v.has_order = true;
        }
    }
    if (v.has_order)
    {
        Item* oi = bot->GetItemByGuid(order.item);
        v.order_entry = order.entry;
        v.order_price = order.price;
        v.order_item_present = oi && oi->GetEntry() == order.entry && oi->GetCount() == order.count && IsTradeable(oi);
    }
    // What the bot hands over for nothing in this window: a craft order made for this player that could not go by mail, or the
    // things its owner asked for (StoreGift, in a window the bot opened). Each still in the bags goes in, then the bot accepts.
    std::vector<Item*> pending;
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        uint32 const now = GameTime::GetGameTimeMS();
        auto it = g_deliveries.find(bot->GetGUID().GetCounter());
        if (it != g_deliveries.end())
        {
            if (now - it->second.stamp_ms > kShelfLifeMs)
                g_deliveries.erase(it);
            else if (it->second.requester == partner->GetGUID())
            {
                std::vector<Item*> held = bot->GetItemListByEntry(it->second.entry, false);
                if (held.empty())
                    g_deliveries.erase(it);
                else
                {
                    pending.push_back(held.front());
                    v.has_delivery = true;
                }
            }
        }
        auto g = g_gifts.find(bot->GetGUID().GetCounter());
        if (g != g_gifts.end())
        {
            if (now - g->second.stamp_ms > kShelfLifeMs)
                g_gifts.erase(g);
            else if (g->second.partner == partner->GetGUID())
            {
                std::size_t const before = pending.size();
                for (ObjectGuid const& guid : g->second.items)
                    if (Item* gi = bot->GetItemByGuid(guid); gi && IsTradeable(gi) && !gi->IsBindedNotWith(partner) &&
                        std::find(pending.begin(), pending.end(), gi) == pending.end())
                        pending.push_back(gi);
                if (pending.size() > before)
                    v.has_delivery = true;
                else
                    g_gifts.erase(g);   // all of it went (or was used up): his next window is an ordinary one
            }
        }
    }
    v.delivery_present = !pending.empty();
    // The ones not in the window yet.
    std::vector<Item*> to_add;
    for (Item* p : pending)
    {
        bool placed = false;
        for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT && !placed; ++i)
            placed = mine->GetItem(TradeSlots(i)) == p;
        if (!placed)
            to_add.push_back(p);
    }
    v.delivery_left = uint32(to_add.size());
    // Only look at room when the bot is about to take something in: it is the costly check.
    if (!v.his.empty() && v.his_accepted && !v.my_accepted)
        v.bot_has_room = HasRoomFor(bot, his_items);

    BotTrade::Action const a = BotTrade::Decide(v);

    // What the bot says about it.
    if (a.say != BotTrade::Say::None)
    {
        std::string text;
        switch (a.say)
        {
            case BotTrade::Say::OfferBuy:    text = "i'll give you " + BotTrade::FormatMoney(a.money) + " for that"; break;
            case BotTrade::Say::PriceIs:     text = order.name + " is " + BotTrade::FormatMoney(order.price); break;
            default:                         text = BotTrade::SayText(a.say); break;
        }
        Say(bot, partner, a.say, a.money ? a.money : order.price, text);
    }

    switch (a.act)
    {
        case BotTrade::Act::Wait:
            return Result::Locked;
        case BotTrade::Act::Cancel:
            bot->TradeCancel(true);
            return Result::ServerRefused;
        case BotTrade::Act::SetMoney:
            mine->UpdateClientStateIndex();
            mine->SetMoney(a.money);
            return Result::Locked;
        case BotTrade::Act::ClearMine:
            mine->UpdateClientStateIndex();
            for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
                mine->SetItem(TradeSlots(i), nullptr);
            return Result::Locked;
        case BotTrade::Act::AddOrdered:
        {
            Item* oi = bot->GetItemByGuid(order.item);
            if (!oi) return Result::Locked;
            for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
                if (!mine->GetItem(TradeSlots(i)))
                {
                    if (oi->IsBindedNotWith(partner)) return Result::ServerRefused;
                    mine->UpdateClientStateIndex();
                    mine->SetItem(TradeSlots(i), oi);
                    break;
                }
            return Result::Locked;
        }
        case BotTrade::Act::AddDelivery:
        {
            // The next one not in the window, in the first free slot.
            Item* next = to_add.empty() ? (pending.empty() ? nullptr : pending.front()) : to_add.front();
            if (!next) return Result::Locked;
            for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
                if (!mine->GetItem(TradeSlots(i)))
                {
                    if (next->IsBindedNotWith(partner)) return Result::ServerRefused;
                    mine->UpdateClientStateIndex();
                    mine->SetItem(TradeSlots(i), next);
                    break;
                }
            return Result::Locked;
        }
        case BotTrade::Act::Accept:
        {
            WorldPacket data(CMSG_ACCEPT_TRADE);
            data << uint32(his->GetServerStateIndex());
            WorldPackets::Trade::AcceptTrade accept(std::move(data));
            accept.Read();
            bot->GetSession()->HandleAcceptTradeOpcode(accept);
            if (v.has_delivery)
            {
                std::lock_guard<std::mutex> lk(g_mtx);
                g_gifts.erase(bot->GetGUID().GetCounter());
            }
            // The trade went through (the window is gone): the gear it took as upgrades is in its bags now, to be worn at once
            // (BotAI::WearGiftGear). Only a finished trade counts; a window still open waits for the player.
            if (!bot->GetTradeData())
            {
                std::vector<uint32> gear;
                for (BotTrade::Line const& l : v.his)
                    if ((l.upgrade || l.later) && l.entry)
                    {
                        gear.push_back(l.entry);
                        if (l.later)
                            if (ItemTemplate const* t = sObjectMgr->GetItemTemplate(l.entry))
                                BotLanguage::Whisper(bot, partner, fmt::format("thanks, i'll put {} on at level {}", NameOf(t),
                                                                               t->GetBaseRequiredLevel()));
                    }
                if (!gear.empty())
                {
                    std::lock_guard<std::mutex> lk(g_mtx);
                    if (g_gift_gear.size() > 4096) g_gift_gear.clear();
                    auto& list = g_gift_gear[bot->GetGUID().GetCounter()];
                    list.insert(list.end(), gear.begin(), gear.end());
                    TC_LOG_INFO("playerbot.v2", "[trade] bot {} took {} piece(s) of gear from {} as upgrades (now or at a later level), to wear", bot->GetName(),
                        gear.size(), partner->GetName());
                }
            }
            if (v.has_order && !v.mine.empty())
            {
                std::lock_guard<std::mutex> lk(g_mtx);
                auto it = g_shelves.find(Key(bot, partner));
                if (it != g_shelves.end()) it->second.has_order = false;
            }
            return Result::Ok;
        }
    }
    return Result::Locked;
}

} // namespace Playerbot::TradeShop
