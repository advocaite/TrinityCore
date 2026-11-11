// BotLanguage - which language a player reads, and the send points that give a bot's English reply to each player in it.
//
// The text itself is translated by Util/BotLocaleText (the catalogs). This file decides the reader's language and sends:
//
//   * the player's choice, `.bot language auto|en|ru` (per account, kept in playerbot_v2_owner_language);
//   * else the client's language: the session's locale when the realm loads that language (Load.Locales / DBC files), else the
//     locale the client reported at login (auth `account.locale`, read once per login), so a Russian client on a realm with
//     English data still gets Russian replies;
//   * else English. Bots and the console always read English (their text feeds the bot logic and the logs).
//
// Send points (the only places a bot's text leaves the server for a player): a whisper, party / raid chat (each member gets
// the line in their own language), say / yell (each listener in range), a chat channel and guild chat (each member), and the `.bot` command
// replies (LocalizedHandler).
// Nothing is translated for a reader of English, so an English realm costs one lookup per line.
//
//   Playerbot.Language.Enable  1   0 = every bot reply in English (as before), whatever the client
//
// World thread only (the send points run there; LangOf takes a shared lock, so it is safe elsewhere too).

#pragma once

#include "Chat.h"
#include "Define.h"
#include "SharedDefines.h"
#include "Util/BotLocaleText.h"

#include <string>
#include <string_view>

class Channel;
class Group;
class Guild;
class Player;
class WorldSession;

namespace Playerbot::BotLanguage {

// A player's choice (stored as this number).
enum class Pref : uint8
{
    Auto = 0,   // the client's language
    En   = 1,
    Ru   = 2,
};

// Reads the config and the stored choices. World thread, at module init.
void Load();

// A player (not a bot) logged in: remember the language their client reported. Runs the login database query asynchronously.
void OnLogin(Player* p);

// Delivers the finished login queries. World thread, every world update.
void Update();

Locale::Lang LangOf(Player const* reader);
Pref         GetPref(uint32 account_id);
void         SetPref(uint32 account_id, Pref pref);
char const*  PrefName(Pref pref);
bool         ParsePref(std::string_view word, Pref& out);

// `english` in `reader`'s language.
std::string For(Player const* reader, std::string_view english);

// `from` whispers `to` (Player::Whisper with the line in `to`'s language).
void Whisper(Player* from, Player* to, std::string const& english);

// `from` speaks in group chat: every member of `group` (only `subgroup` when it is not -1) reads the line in their language.
void GroupChat(Player* from, Group* group, ChatMsg type, std::string const& english, int subgroup);

// `from` says / yells: everyone within `range` (and `from`) reads the line in their language. Calls the chat script hook with
// the English line first, as Player::Say does, so the listening bots see what they always saw.
void SayInRange(Player* from, ChatMsg type, Language language, std::string const& english, float range);

// `from` speaks on a chat channel it is on (General, Trade, LookingForGroup): every member reads the line in their language.
void ChannelSay(Player* from, Channel* channel, std::string const& english);

// `from` speaks in guild chat (officer chat when `officer`): every member who may listen reads the line in their language.
void GuildSay(Player* from, Guild* guild, bool officer, std::string const& english);

// A ChatHandler that translates every system line it sends for its player. A player command takes one at its top:
//
//     LocalizedHandler local(handler); handler = local.Get();
//
// Get() is the original handler when the player reads English (or the command came from the console), so nothing changes there.
// The error flag a handler sets is copied back to the original one when this goes out of scope.
class LocalizedHandler : public ChatHandler
{
public:
    explicit LocalizedHandler(ChatHandler* original);
    // For a line the bot logic sends a player outside a command (`LocalizedHandler(p->GetSession()).PSendSysMessage(...)`).
    explicit LocalizedHandler(WorldSession* session);
    ~LocalizedHandler();
    LocalizedHandler(LocalizedHandler const&) = delete;
    LocalizedHandler& operator=(LocalizedHandler const&) = delete;

    ChatHandler* Get() { return active_ ? this : original_; }

    using ChatHandler::SendSysMessage;
    void SendSysMessage(std::string_view str, bool escapeCharacters = false) override;

private:
    ChatHandler* original_;
    Locale::Lang lang_ = Locale::Lang::En;
    bool         active_ = false;
};

} // namespace Playerbot::BotLanguage
