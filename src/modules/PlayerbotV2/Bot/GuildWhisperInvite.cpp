// GuildWhisperInvite - see GuildWhisperInvite.h.

#include "GuildWhisperInvite.h"

#include "BotIntent.h"
#include "BotRegistry.h"
#include "GuildAskText.h"
#include "../Fleet/BotGuildMgr.h"
#include "../Services.h"
#include "../Threading/IntentQueue.h"

#include "GameTime.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "World.h"
#include "WorldSession.h"

#include <fmt/format.h>
#include <mutex>
#include <unordered_map>

namespace Playerbot::V2::GuildWhisperInvite {

namespace {

constexpr uint32 kPlayerOfferMs = 3u * 60u * 1000u;    // after the bot talked about its guild to this player
constexpr uint32 kTradeOfferMs  = 30u * 60u * 1000u;   // after its "/w <name> for invite" line in Trade

std::mutex g_mtx;
std::unordered_map<uint64, uint32> g_player_offer;     // (bot << 32) | player -> game ms
std::unordered_map<uint64, uint32> g_trade_offer;      // bot -> game ms

uint64 Key(uint64 bot_low, uint64 player_low) { return (bot_low << 32) | (player_low & 0xFFFFFFFFull); }

bool OfferOpen(uint64 bot_low, uint64 player_low, uint32 now)
{
    std::lock_guard<std::mutex> lk(g_mtx);
    if (auto it = g_player_offer.find(Key(bot_low, player_low)); it != g_player_offer.end() && now - it->second < kPlayerOfferMs)
        return true;
    if (auto it = g_trade_offer.find(bot_low); it != g_trade_offer.end() && now - it->second < kTradeOfferMs)
        return true;
    return false;
}

void CloseOffer(uint64 bot_low, uint64 player_low)
{
    std::lock_guard<std::mutex> lk(g_mtx);
    g_player_offer.erase(Key(bot_low, player_low));
}

void Say(Player* bot, Player* to, std::string text)
{
    BotId const id = bot->GetGUID().GetCounter();
    Intent it{};
    it.bot_id = id;
    it.body = ChatIntent{WhisperIntent{to->GetName(), std::move(text)}};
    it.defer_until_ms = GameTime::GetGameTimeMS() + 900u + urand(0, 1400);
    Services::Intents(id).push(std::move(it));
}

bool IsOfficer(Guild const* g, ObjectGuid guid)
{
    Guild::Member const* m = g->GetMember(guid);
    return m && static_cast<uint8>(m->GetRankId()) <= 1;
}

// An online officer bot of the guild other than `self`, or nullptr.
Player* FindOnlineOfficerBot(Guild const* g, Player const* self)
{
    for (auto const& [guid, member] : g->GetMembers())
    {
        if (guid == self->GetGUID() || !member.IsOnline() || static_cast<uint8>(member.GetRankId()) > 1)
            continue;
        if (!Services::Registry().has(guid.GetCounter()))
            continue;
        Player* p = ObjectAccessor::FindConnectedPlayer(guid);
        if (p && p->IsInWorld() && p->GetSession())
            return p;
    }
    return nullptr;
}

bool Handle(Player* sender, Player* bot, std::string const& msg, char const* how)
{
    uint64 const bot_low = bot->GetGUID().GetCounter();
    uint64 const player_low = sender->GetGUID().GetCounter();
    CloseOffer(bot_low, player_low);

    auto log = [&](std::string const& outcome)
    {
        TC_LOG_INFO("playerbot.v2", "[guild_whisper] {} <- {} \"{}\" ({}): {}", bot->GetName(), sender->GetName(), msg, how, outcome);
    };

    uint64 const gid = bot->GetGuildId();
    Guild* g = gid ? sGuildMgr->GetGuildById(gid) : nullptr;
    if (!g)
    {
        Say(bot, sender, "sorry, i'm not in a guild");
        log("bot has no guild");
        return true;
    }
    std::string const gname = g->GetName();
    if (sender->GetGuildId() == gid)
    {
        Say(bot, sender, "you're already in <" + gname + "> :)");
        log("already a member");
        return true;
    }
    if (!Services::Guilds().IsBotManaged(gid))
    {
        Say(bot, sender, "sorry, i can't invite people to <" + gname + ">, ask one of its officers");
        log("not a bot-run guild");
        return true;
    }
    if (sender->GetGuildId() != 0)
    {
        Say(bot, sender, "you're already in a guild, leave it first and ask me again");
        log("player is in another guild");
        return true;
    }
    if (!sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GUILD) && sender->GetTeam() != bot->GetTeam())
    {
        Say(bot, sender, "sorry, i can only invite players of my faction");
        log("other faction");
        return true;
    }
    if (g->GetMembersCount() >= Services::Guilds().MaxMembersPerGuild())
    {
        Say(bot, sender, "sorry, <" + gname + "> is full right now");
        log("guild full");
        return true;
    }
    if (sender->GetGuildIdInvited() == gid)
    {
        Say(bot, sender, "i already sent you an invite, accept it on your screen");
        log("invite already pending");
        return true;
    }
    if (sender->GetGuildIdInvited() != 0)
    {
        Say(bot, sender, "you have another guild invite open, answer that one first");
        log("another invite pending");
        return true;
    }

    Player* inviter = IsOfficer(g, bot->GetGUID()) ? bot : FindOnlineOfficerBot(g, bot);
    if (!inviter)
    {
        Say(bot, sender, "i can't invite, and none of our officers are online right now, try again later");
        log("no officer online");
        return true;
    }
    if (!inviter->GetSession())
    {
        log("inviter has no session");
        return true;
    }

    g->HandleInviteMember(inviter->GetSession(), sender->GetName());
    if (sender->GetGuildIdInvited() != gid)
    {
        // Guild::HandleInviteMember refused (the player ignores the inviter, or the rank lost its invite right).
        Say(bot, sender, "sorry, the invite didn't go through");
        log(fmt::format("refused by the guild code (inviter {})", inviter->GetName()));
        return true;
    }
    if (inviter == bot)
        Say(bot, sender, "sure! invite to <" + gname + "> sent, welcome :)");
    else
        Say(bot, sender, "i can't invite myself, so " + inviter->GetName() + " sent you the invite to <" + gname + ">");
    log(fmt::format("invite to <{}> sent by {}", gname, inviter->GetName()));
    return true;
}

bool Usable(Player* sender, Player* bot)
{
    if (!sender || !bot || sender == bot)
        return false;
    if (!sender->GetSession() || sender->GetSession()->IsBot())
        return false;
    return Services::Registry().has(bot->GetGUID().GetCounter());
}

} // namespace

bool TryRequest(Player* sender, Player* bot, std::string const& msg)
{
    if (!Usable(sender, bot) || msg.empty() || msg.front() == ';' || msg.front() == '.' || msg.front() == '/')
        return false;
    if (!GuildAsk::IsJoinRequest(GuildAsk::Normalize(msg)))
        return false;
    return Handle(sender, bot, msg, "asked");
}

bool TryFollowUp(Player* sender, Player* bot, std::string const& msg)
{
    if (!Usable(sender, bot) || msg.empty())
        return false;
    if (!OfferOpen(bot->GetGUID().GetCounter(), sender->GetGUID().GetCounter(), GameTime::GetGameTimeMS()))
        return false;
    if (!GuildAsk::IsShortYes(GuildAsk::Normalize(msg)))
        return false;
    return Handle(sender, bot, msg, "yes after an offer");
}

void NoteOffer(uint64 bot_low, uint64 player_low)
{
    uint32 now = GameTime::GetGameTimeMS();
    if (now == 0)
        now = 1;
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_player_offer.size() > 4096)
        g_player_offer.clear();
    if (player_low == 0)
        g_trade_offer[bot_low] = now;
    else
        g_player_offer[Key(bot_low, player_low)] = now;
}

} // namespace Playerbot::V2::GuildWhisperInvite
