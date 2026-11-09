#include "PlayerCraftOrders.h"

#include "../Api/BotTradeShop.h"
#include "../Bot/BotIntent.h"
#include "../Bot/BotIntentEmitter.h"
#include "../Bot/BotRegistry.h"
#include "../Bot/BotSnapshot.h"
#include "../Bot/ClassicUpkeep.h"
#include "../Bot/PlayerCraftOrder.h"
#include "../Services.h"
#include "../Threading/IntentQueue.h"
#include "../Threading/SnapshotPublisher.h"

#include "GameTime.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TradeData.h"
#include "TradePackets.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <cctype>
#include <fmt/format.h>
#include <vector>

namespace Playerbot::V2::PlayerCraftOrders {
namespace {

namespace pc = ::Playerbot::V2::PlayerCraft;

constexpr std::uint32_t kMaxItems = 20;            // per order (fits one mail stack of most crafts)
constexpr std::uint32_t kMaxListed = 12;
constexpr std::uint32_t kCastGapMs = 2500;         // between two casts, long enough for a profession cast to finish and report
constexpr std::uint32_t kOrderDeadlineMs = 10u * 60u * 1000u;
constexpr std::uint32_t kMailWaitMs = 15000;       // for the mail intent to take the items out of the bags
constexpr std::uint32_t kMaxDismounts = 3;         // a rule that mounts the bot again is not fought for ever
constexpr std::uint32_t kExtraCasts = 3;           // casts beyond the plan before the order is given up (a cast can fail and be retried)

enum class Phase : std::uint8_t { Crafting, Mailing };

struct Job
{
    std::uint64_t bot = 0;                // guid counter
    std::string requester;
    ObjectGuid requester_guid;
    std::uint32_t spell = 0;
    std::uint32_t item = 0;
    std::uint32_t yield = 1;
    std::uint32_t wanted = 0;             // items the plan promised
    std::uint32_t baseline = 0;           // the bot's stock of the item when the order was taken
    std::uint32_t casts_planned = 0;
    std::uint32_t casts_pushed = 0;
    std::uint32_t dismounts = 0;          // times the bot was told to get off its mount (a mounted caster is refused)
    std::uint32_t started_ms = 0;
    std::uint32_t last_cast_ms = 0;
    std::uint32_t mail_ms = 0;
    std::uint32_t mailing = 0;            // items handed to the mail intents
    std::uint32_t before_mail = 0;        // the bot's stock when the mail intents were pushed
    bool told_mana = false;               // the requester was told the bot drinks first
    bool by_trade = false;                // handed over in a trade window, not mailed (conjured, or he stood next to the bot)
    Phase phase = Phase::Crafting;
};

std::vector<Job> g_jobs;

template <class T>
bool Push(Player const* bot, T body)
{
    BotId const id = bot->GetGUID().GetCounter();
    IntentQueue* q = Services::Registry().intents(id);
    IntentId* next = Services::Registry().next_intent_id(id);
    if (!q || !next)
        return false;
    BotIntentEmitter emit(q, id, /*source*/ 0, next);
    return emit.emit(WrapForIntentBody(std::move(body)));
}

void Say(Player const* bot, std::string const& to, std::string text)
{
    Push(bot, WhisperIntent{ to, std::move(text) });
}

std::string Lower(std::string s)
{
    for (char& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string ItemLink(ItemTemplate const* t)
{
    return fmt::format("[{}]", t->GetName(sWorld->GetDefaultDbcLocale()));
}

// Casts the bot's bags allow for this recipe right now.
std::uint32_t CastsPossible(Player const* bot, SpellInfo const* si)
{
    std::uint32_t casts = 1000;
    bool any = false;
    for (std::size_t i = 0; i < si->Reagent.size(); ++i)
    {
        std::int32_t const entry = si->Reagent[i];
        std::int16_t const need = si->ReagentCount[i];
        if (entry <= 0 || need <= 0)
            continue;
        any = true;
        casts = std::min<std::uint32_t>(casts, bot->GetItemCount(std::uint32_t(entry), false) / std::uint32_t(need));
    }
    return any ? casts : 1000u;   // no reagents: bounded by the item cap in PlanOrder
}

// Every item the bot can make with a recipe it knows.
std::vector<pc::Option> Options(Player const* bot)
{
    std::vector<pc::Option> out;
    BotId const id = bot->GetGUID().GetCounter();
    auto snap = Services::Snapshots().latest(id);
    if (!snap)
        return out;
    for (std::uint32_t spell : snap->spellbook.known_recipes)
    {
        SpellInfo const* si = sSpellMgr->GetSpellInfo(spell, DIFFICULTY_NONE);
        if (!si)
            continue;
        for (SpellEffectInfo const& eff : si->GetEffects())
        {
            if (eff.Effect != SPELL_EFFECT_CREATE_ITEM || eff.ItemType == 0)
                continue;
            ItemTemplate const* t = sObjectMgr->GetItemTemplate(eff.ItemType);
            if (!t)
                continue;
            pc::Option o;
            o.spell = spell;
            o.item = eff.ItemType;
            o.yield = std::max<std::uint32_t>(1u, std::uint32_t(std::max(eff.CalcValueAsInt(bot), 1)));
            o.name = Lower(t->GetName(sWorld->GetDefaultDbcLocale()));
            o.rank = std::uint32_t(std::max(t->GetBaseRequiredLevel(), 0)) * 1000u + std::min<std::uint32_t>(t->GetBaseItemLevel(), 999u);
            if (si->SpellName)
                o.spell_name = Lower(si->SpellName->Str[sWorld->GetDefaultDbcLocale()]);
            if (::Playerbot::Classic::ConsumableKind kind = {}; ::Playerbot::Classic::IsMageConjure(spell, kind))
                o.conjured = kind == ::Playerbot::Classic::ConsumableKind::Drink ? pc::Conjured::Water : pc::Conjured::Food;
            out.push_back(std::move(o));
            break;
        }
    }
    return out;
}

// An item that binds when made (conjured mana gems) can never be given to the requester: left out of the list, refused up front.
bool BindsWhenMade(ItemTemplate const* t)
{
    return t && pc::DeliveryFor(false, t->GetBonding() == BIND_ON_ACQUIRE) == pc::Delivery::Bound;
}

// How the bot's stock of `item` can reach the requester.
pc::Delivery DeliveryOf(Player const* bot, std::uint32_t item)
{
    ItemTemplate const* t = sObjectMgr->GetItemTemplate(item);
    std::vector<Item*> const held = bot->GetItemListByEntry(item, false);
    return pc::DeliveryFor(t && t->HasFlag(ITEM_FLAG_CONJURED), !held.empty() && held.front()->IsSoulBound());
}

// A food / drink aura is up (the drink rule sat the bot down).
bool Drinking(Player const* bot)
{
    for (auto const& [id, app] : bot->GetAppliedAuras())
    {
        SpellSpecificType const spec = app->GetBase()->GetSpellInfo()->GetSpellSpecific();
        if (spec == SPELL_SPECIFIC_DRINK || spec == SPELL_SPECIFIC_FOOD_AND_DRINK)
            return true;
    }
    return false;
}

// The mana one cast of `spell` costs the bot now, 0 for a spell without a mana cost.
std::uint32_t ManaCost(Player const* bot, std::uint32_t spell)
{
    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell, DIFFICULTY_NONE);
    if (!si)
        return 0;
    for (SpellPowerCost const& cost : si->CalcPowerCost(bot, si->GetSchoolMask()))
        if (cost.Power == POWER_MANA)
            return std::uint32_t(std::max(cost.Amount, 0));
    return 0;
}

// The order is ready and goes by trade: when the requester stands within reach and neither is trading, the bot opens the window
// itself as a player would (CMSG_INITIATE_TRADE; the handler makes every check), and TradeShop puts the item in. True: it opened.
bool OpenTradeWith(Player* bot, Player* requester)
{
    if (!bot || !requester || bot->GetTradeData() || requester->GetTradeData() || requester->GetMap() != bot->GetMap()
        || !bot->IsWithinDistInMap(requester, TRADE_DISTANCE - 1.0f))
        return false;
    WorldPacket packet(CMSG_INITIATE_TRADE);
    WorldPackets::Trade::InitiateTrade init(std::move(packet));
    init.Guid = requester->GetGUID();
    bot->GetSession()->HandleInitiateTradeOpcode(init);
    return bot->GetTradeData() != nullptr;
}

void Finish(Job const& j, Player const* bot, std::string const& text)
{
    if (bot)
        Say(bot, j.requester, text);
    TC_LOG_INFO("playerbot.v2", "[craft_order] bot {} for {}: {}", j.bot, j.requester, text);
}

} // namespace

bool Order(Player* requester, Player* bot, std::string const& text)
{
    pc::Request const req = pc::ParseRequest(text, kMaxItems);
    if (req.type == pc::Kind::None)
        return false;
    std::string const to = requester->GetName();

    std::vector<pc::Option> const opts = Options(bot);
    if (req.type == pc::Kind::List)
    {
        std::uint32_t listed = 0, total = 0;
        for (pc::Option const& o : opts)
        {
            SpellInfo const* si = sSpellMgr->GetSpellInfo(o.spell, DIFFICULTY_NONE);
            if (!si || CastsPossible(bot, si) == 0 || BindsWhenMade(sObjectMgr->GetItemTemplate(o.item)))
                continue;
            ++total;
            if (listed >= kMaxListed)
                continue;
            ItemTemplate const* t = sObjectMgr->GetItemTemplate(o.item);
            Say(bot, to, fmt::format("I can make {} (up to {} now)", t ? ItemLink(t) : std::string("?"), std::min(CastsPossible(bot, si) * o.yield, kMaxItems)));
            ++listed;
        }
        if (total == 0)
            Say(bot, to, "I can't make anything with what I carry.");
        else if (total > listed)
            Say(bot, to, fmt::format("...and {} more", total - listed));
        return true;
    }

    int const idx = pc::Match(req, opts, requester->GetLevel());
    if (idx < 0)
    {
        Say(bot, to, "I don't know a recipe for that.");
        return true;
    }
    pc::Option const& o = opts[std::size_t(idx)];
    SpellInfo const* si = sSpellMgr->GetSpellInfo(o.spell, DIFFICULTY_NONE);
    ItemTemplate const* t = sObjectMgr->GetItemTemplate(o.item);
    if (!si || !t)
    {
        Say(bot, to, "I can't make that right now.");
        return true;
    }
    if (BindsWhenMade(t))
    {
        Say(bot, to, fmt::format("{} is soulbound once it is made, I can't give it to you.", ItemLink(t)));
        return true;
    }
    for (Job const& j : g_jobs)
        if (j.bot == bot->GetGUID().GetCounter())
        {
            Say(bot, to, "I'm still busy with an order, ask me again in a moment.");
            return true;
        }

    std::uint32_t const carried = bot->GetItemCount(o.item, false);
    pc::Plan const plan = pc::PlanOrder({ req.count, o.yield, kMaxItems, CastsPossible(bot, si), pc::CarryRoom(t->GetMaxCount(), carried) });
    if (plan.refusal)
    {
        if (std::string_view(plan.refusal) == "already_carried")
        {
            // A unique item (a conjured Mana Jade): a second cast would make nothing, so the one it carries is handed over by trade.
            if (carried > 0 && pc::DeliveryFor(t->HasFlag(ITEM_FLAG_CONJURED), false) == pc::Delivery::Trade)
            {
                TradeShop::StoreDelivery(bot, requester->GetGUID(), o.item);
                Say(bot, to, fmt::format("I already carry {}, I can only hold one. Open a trade with me and I'll hand it over.", ItemLink(t)));
            }
            else
                Say(bot, to, fmt::format("I already carry {} and can only hold one.", ItemLink(t)));
            return true;
        }
        if (std::string_view(plan.refusal) == "no_reagents")
        {
            std::string need;
            for (std::size_t i = 0; i < si->Reagent.size(); ++i)
                if (si->Reagent[i] > 0 && si->ReagentCount[i] > 0)
                    if (ItemTemplate const* r = sObjectMgr->GetItemTemplate(std::uint32_t(si->Reagent[i])))
                        need += fmt::format("{}{} x{}", need.empty() ? "" : ", ", ItemLink(r), si->ReagentCount[i]);
            Say(bot, to, fmt::format("I'm out of reagents for {}. One needs: {}. Trade them to me and ask again.", ItemLink(t), need));
        }
        else
            Say(bot, to, "That's more than I make in one order.");
        return true;
    }

    // Will the bags take it? A cast whose item does not fit is refused before it goes out (API CastBlockedByOwnState,
    // API_ITEM_AT_LIMIT), and the order used to end in "no item came out" with no reason (Gun's water, 2026-11-08).
    {
        ItemPosCountVec dest;
        InventoryResult const room = bot->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, o.item, std::max<std::uint32_t>(1, o.yield));
        if (room != EQUIP_ERR_OK)
        {
            TC_LOG_INFO("playerbot.v2", "[craft_order] {} can't store item {} x{}: InventoryResult {} (limit category {}, max count {}, "
                        "free slots {})", bot->GetName(), o.item, o.yield, uint32(room), t->GetItemLimitCategory(), t->GetMaxCount(),
                        bot->GetFreeInventorySlotCount());
            if (room == EQUIP_ERR_INV_FULL || room == EQUIP_ERR_BAG_FULL)
            {
                Push(bot, MakeBagRoomIntent{ 2 });
                Say(bot, to, "My bags are full, I'm making some room first.");
            }
            else
            {
                Say(bot, to, fmt::format("I can't carry any more {} right now.", ItemLink(t)));
                return true;
            }
        }
    }

    // A conjured item can't be mailed, and a player standing next to the bot wants it now: those go by trade, and the bot says so.
    bool const by_trade = DeliveryOf(bot, o.item) != pc::Delivery::Mail || t->HasFlag(ITEM_FLAG_CONJURED)
        || (requester->GetMap() == bot->GetMap() && bot->IsWithinDistInMap(requester, 30.0f));

    Job j;
    j.bot = bot->GetGUID().GetCounter();
    j.requester = to;
    j.requester_guid = requester->GetGUID();
    j.spell = o.spell;
    j.item = o.item;
    j.yield = o.yield;
    j.wanted = plan.items;
    j.baseline = bot->GetItemCount(o.item, false);
    j.casts_planned = plan.casts;
    j.by_trade = by_trade;
    j.started_ms = GameTime::GetGameTimeMS();
    g_jobs.push_back(j);
    char const* const how = by_trade ? "trade it to you" : "mail it to you";
    Say(bot, to, plan.items > req.count
        ? fmt::format("One cast makes {}, so I'm making {} x{} and will hand them over when done.", o.yield, ItemLink(t), plan.items)
        : plan.trimmed
        ? fmt::format("I can make {} of the {} you asked for with my reagents; I'll {} when done.", plan.items, ItemLink(t), how)
        : fmt::format("Making {} x{}, I'll {} when done.", ItemLink(t), plan.items, how));
    TC_LOG_INFO("playerbot.v2", "[craft_order] queued: {} for {} item {} x{} (spell {}, {} casts, {} per cast)", bot->GetName(), to, o.item, plan.items, o.spell, plan.casts, o.yield);
    return true;
}

void Tick(std::uint32_t now_ms)
{
    for (std::size_t i = 0; i < g_jobs.size();)
    {
        Job& j = g_jobs[i];
        Player* bot = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(j.bot));
        bool done = false;
        if (!bot)
        {
            if (now_ms - j.started_ms > kOrderDeadlineMs)
            {
                Finish(j, nullptr, "bot gone, order dropped");
                done = true;
            }
        }
        else
        {
            std::uint32_t const have = bot->GetItemCount(j.item, false);
            std::uint32_t const crafted = have > j.baseline ? have - j.baseline : 0;
            if (j.phase == Phase::Crafting)
            {
                bool const out_of_time = now_ms - j.started_ms > kOrderDeadlineMs;
                bool const gave_up = j.casts_pushed >= j.casts_planned + kExtraCasts && !bot->IsNonMeleeSpellCast(false)
                    && now_ms - j.last_cast_ms >= kCastGapMs;
                if (crafted >= j.wanted || out_of_time || gave_up)
                {
                    if (crafted == 0)
                    {
                        ItemTemplate const* t = sObjectMgr->GetItemTemplate(j.item);
                        TC_LOG_INFO("playerbot.v2",
                            "[craft_order] no item came out: bot {} spell {} known {} casts {} carried {} maxcount {} mounted {} mana {} dismounts {}",
                            j.bot, j.spell, bot->HasSpell(j.spell), j.casts_pushed, have, t ? t->GetMaxCount() : 0u, bot->IsMounted(),
                            bot->GetPower(POWER_MANA), j.dismounts);
                        if (have > 0 && DeliveryOf(bot, j.item) == pc::Delivery::Trade)
                        {
                            // The bot already carries one and the data does not call it unique: the cast made nothing, so that one goes over by trade.
                            TradeShop::StoreDelivery(bot, j.requester_guid, j.item);
                            Finish(j, bot, fmt::format("I already carry {} and can't make another. Open a trade with me and I'll hand it over.", ItemLink(t)));
                        }
                        else
                            Finish(j, bot, "I couldn't make that (no item came out), sorry.");
                        done = true;
                    }
                    else if (pc::Delivery const how = DeliveryOf(bot, j.item); how != pc::Delivery::Mail || j.by_trade)
                    {
                        // The mail refuses a conjured item, and a requester at hand gets it now: it goes over in a trade window,
                        // which the bot opens itself when he is within reach.
                        std::string const link = ItemLink(sObjectMgr->GetItemTemplate(j.item));
                        if (how == pc::Delivery::Bound)
                            Finish(j, bot, fmt::format("I made {}, but it is soulbound to me and I cannot give it away.", link));
                        else
                        {
                            TradeShop::StoreDelivery(bot, j.requester_guid, j.item);
                            if (OpenTradeWith(bot, ObjectAccessor::FindConnectedPlayer(j.requester_guid)))
                                Finish(j, bot, fmt::format("Here you go, {}.", link));
                            else
                                Finish(j, bot, fmt::format("I made {}. Open a trade with me and I'll hand it over.", link));
                        }
                        done = true;
                    }
                    else
                    {
                        // mail exactly what was made, stack by stack
                        std::uint32_t left = std::min(crafted, j.wanted);
                        j.before_mail = have;
                        for (Item* it : bot->GetItemListByEntry(j.item, false))
                        {
                            if (!left)
                                break;
                            std::uint32_t const n = std::min(left, it->GetCount());
                            if (Push(bot, MailSendItemIntent{ j.requester, it->GetGUID(), n, 0, 0, "Your crafted order", "Made to order." }))
                            {
                                left -= n;
                                j.mailing += n;
                            }
                        }
                        j.phase = Phase::Mailing;
                        j.mail_ms = now_ms;
                    }
                }
                else if (!bot->IsNonMeleeSpellCast(false) && !bot->IsInCombat() && bot->IsAlive() && now_ms - j.last_cast_ms >= kCastGapMs)
                {
                    if (pc::WaitForMana(Drinking(bot), bot->GetPower(POWER_MANA), bot->GetMaxPower(POWER_MANA), ManaCost(bot, j.spell)))
                    {
                        // Not a cast: the give-up count stands still while the bot drinks (the order deadline still runs).
                        if (!j.told_mana)
                        {
                            Say(bot, j.requester, "I'm out of mana, I'll drink first and then finish your order.");
                            j.told_mana = true;
                        }
                    }
                    else if (bot->IsMounted() && j.dismounts < kMaxDismounts)
                    {
                        // The spell code refuses every cast of a mounted caster (NOT_MOUNTED): get off first.
                        Push(bot, DismountIntent{});
                        ++j.dismounts;
                    }
                    else
                    {
                        // exact rank: the recipe asked for, not the highest rank the bot knows (that one makes another item and the order would never see it)
                        Push(bot, CastSpellIntent{ j.spell, ObjectGuid::Empty, true });
                        ++j.casts_pushed;
                    }
                    j.last_cast_ms = now_ms;
                }
            }
            else
            {
                std::uint32_t const gone = j.before_mail > have ? j.before_mail - have : 0;
                if (gone >= j.mailing)
                {
                    Finish(j, bot, fmt::format("Mailed {} to you.", j.mailing));
                    done = true;
                }
                else if (now_ms - j.mail_ms > kMailWaitMs)
                {
                    TradeShop::StoreDelivery(bot, j.requester_guid, j.item);
                    Finish(j, bot, fmt::format("I made it but the mail failed ({} of {} left my bags); open a trade with me and I'll hand it over.", gone, j.mailing));
                    done = true;
                }
            }
        }
        if (done)
            g_jobs.erase(g_jobs.begin() + std::ptrdiff_t(i));
        else
            ++i;
    }
}

std::uint32_t Pending()
{
    return std::uint32_t(g_jobs.size());
}

} // namespace Playerbot::V2::PlayerCraftOrders
