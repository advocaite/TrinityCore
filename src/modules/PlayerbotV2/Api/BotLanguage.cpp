#include "BotLanguage.h"

#include "AsyncCallbackProcessor.h"
#include "CellImpl.h"
#include "Channel.h"
#include "ChatPackets.h"
#include "ChatTextBuilder.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Guild.h"
#include "Log.h"
#include "Player.h"
#include "QueryCallback.h"
#include "ScriptMgr.h"
#include "StringFormat.h"
#include "Util.h"
#include "World.h"
#include "WorldSession.h"

#include <atomic>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

namespace Playerbot::BotLanguage {

namespace {

std::atomic<bool> g_enabled{ true };

std::shared_mutex                          g_mtx;
std::unordered_map<uint32, Pref>           g_prefs;           // account -> the player's choice (Auto rows are not kept)
std::unordered_map<uint32, LocaleConstant> g_client_locale;   // account -> the locale its client reported at the last login

QueryCallbackProcessor g_queries;   // world thread only

Locale::Lang FromLocale(LocaleConstant l)
{
    return l == LOCALE_ruRU ? Locale::Lang::Ru : Locale::Lang::En;
}

// One packet per language, built when the first reader of that language is met (Trinity::LocalizedDo keyed on our language
// instead of the database locale, which a realm without Load.Locales forces to English).
class ChatByLang
{
public:
    ChatByLang(Player const* from, ChatMsg type, Language language, std::string const& english)
        : from_(from), type_(type), language_(language), english_(english) { }

    void operator()(Player const* reader)
    {
        if (!reader->GetSession()) return;
        size_t const i = size_t(LangOf(reader));
        if (!senders_[i])
            senders_[i] = std::make_unique<Trinity::ChatPacketSender>(type_, language_, from_, from_,
                Locale::Translate(Locale::Lang(i), english_));
        (*senders_[i])(reader);
    }

private:
    Player const* from_;
    ChatMsg       type_;
    Language      language_;
    std::string const& english_;
    std::unique_ptr<Trinity::ChatPacketSender> senders_[2];
};

} // namespace

void Load()
{
    g_enabled = sConfigMgr->GetBoolDefault("Playerbot.Language.Enable", true, true);
    std::unordered_map<uint32, Pref> prefs;
    if (QueryResult r = CharacterDatabase.Query("SELECT owner_account_id, language FROM playerbot_v2_owner_language"))
        do
        {
            Field* f = r->Fetch();
            uint8 const v = f[1].GetUInt8();
            if (v == uint8(Pref::En) || v == uint8(Pref::Ru))
                prefs[f[0].GetUInt32()] = Pref(v);
        } while (r->NextRow());
    size_t const n = prefs.size();
    {
        std::unique_lock lk(g_mtx);
        g_prefs = std::move(prefs);
    }
    TC_LOG_INFO("playerbot.v2", "[language] bot replies in the player's language {} ({} Russian catalog entries, {} players chose a language)",
        g_enabled ? "on" : "off", Locale::Russian().Size(), n);
    for (std::string const& bad : Locale::Russian().Rejected())
        TC_LOG_ERROR("playerbot.v2", "[language] Russian catalog entry refused (malformed or duplicate): \"{}\"", bad);
}

void OnLogin(Player* p)
{
    WorldSession const* s = p ? p->GetSession() : nullptr;
    if (!s || s->IsBot()) return;
    uint32 const account = s->GetAccountId();
    g_queries.AddCallback(LoginDatabase.AsyncQuery(Trinity::StringFormat("SELECT locale FROM account WHERE id = {}", account).c_str())
        .WithCallback([account](QueryResult r)
    {
        if (!r) return;
        uint8 const loc = r->Fetch()[0].GetUInt8();
        if (loc >= TOTAL_LOCALES) return;
        std::unique_lock lk(g_mtx);
        g_client_locale[account] = LocaleConstant(loc);
    }));
}

void Update()
{
    g_queries.ProcessReadyCallbacks();
}

Locale::Lang LangOf(Player const* reader)
{
    if (!reader || !g_enabled) return Locale::Lang::En;
    WorldSession const* s = reader->GetSession();
    if (!s || s->IsBot()) return Locale::Lang::En;
    uint32 const account = s->GetAccountId();
    {
        std::shared_lock lk(g_mtx);
        if (auto it = g_prefs.find(account); it != g_prefs.end())
            return it->second == Pref::Ru ? Locale::Lang::Ru : Locale::Lang::En;
        if (s->GetSessionDbcLocale() != LOCALE_enUS) return FromLocale(s->GetSessionDbcLocale());
        if (s->GetSessionDbLocaleIndex() != LOCALE_enUS) return FromLocale(s->GetSessionDbLocaleIndex());
        if (auto it = g_client_locale.find(account); it != g_client_locale.end())
            return FromLocale(it->second);
    }
    return Locale::Lang::En;
}

Pref GetPref(uint32 account_id)
{
    std::shared_lock lk(g_mtx);
    auto it = g_prefs.find(account_id);
    return it == g_prefs.end() ? Pref::Auto : it->second;
}

void SetPref(uint32 account_id, Pref pref)
{
    {
        std::unique_lock lk(g_mtx);
        if (pref == Pref::Auto) g_prefs.erase(account_id);
        else g_prefs[account_id] = pref;
    }
    if (pref == Pref::Auto)
        CharacterDatabase.DirectPExecute("DELETE FROM playerbot_v2_owner_language WHERE owner_account_id = {}", account_id);
    else
        CharacterDatabase.DirectPExecute("REPLACE INTO playerbot_v2_owner_language (owner_account_id, language) VALUES ({}, {})",
            account_id, uint32(pref));
}

char const* PrefName(Pref pref)
{
    switch (pref)
    {
        case Pref::En: return "en";
        case Pref::Ru: return "ru";
        default:       return "auto";
    }
}

bool ParsePref(std::string_view word, Pref& out)
{
    std::string w(word);
    strToLower(w);
    if (w == "auto" || w == "авто")                                        { out = Pref::Auto; return true; }
    if (w == "en" || w == "english" || w == "eng" || w == "англ")          { out = Pref::En;   return true; }
    if (w == "ru" || w == "russian" || w == "rus" || w == "рус" || w == "русский") { out = Pref::Ru; return true; }
    // strToLower leaves Cyrillic capitals alone: the capitalised forms a Russian keyboard gives.
    if (word == "Авто")                                                    { out = Pref::Auto; return true; }
    if (word == "Рус" || word == "Русский")                                { out = Pref::Ru;   return true; }
    if (word == "Англ")                                                    { out = Pref::En;   return true; }
    return false;
}

std::string For(Player const* reader, std::string_view english)
{
    return Locale::Translate(LangOf(reader), english);
}

void Whisper(Player* from, Player* to, std::string const& english)
{
    if (!from || !to) return;
    from->Whisper(For(to, english), LANG_UNIVERSAL, to);
}

void GroupChat(Player* from, Group* group, ChatMsg type, std::string const& english, int subgroup)
{
    if (!from || !group) return;
    ChatByLang send(from, type, LANG_UNIVERSAL, english);
    for (GroupReference const& itr : group->GetMembers())
    {
        Player const* member = itr.GetSource();
        if (!member || (subgroup != -1 && itr.getSubGroup() != subgroup)) continue;
        send(member);
    }
}

void SayInRange(Player* from, ChatMsg type, Language language, std::string const& english, float range)
{
    if (!from) return;
    std::string hook_text(english);
    sScriptMgr->OnPlayerChat(from, type, language, hook_text);

    ChatByLang send(from, type, language, english);
    send(from);   // the speaker sees its own line, as Player::SendChatMessageToSetInRange does
    Trinity::MessageDistDeliverer<ChatByLang> notifier(from, send, range, false, nullptr, true);
    Cell::VisitWorldObjects(from, notifier, range);
}

namespace {

// The line in one reader's language, translated once per language when the first reader of it is met (the channel and guild
// broadcasts send one packet per distinct text).
class TextByLang
{
public:
    explicit TextByLang(std::string const& english) : english_(english) { }

    std::string const& operator()(Player const* reader)
    {
        size_t const i = size_t(LangOf(reader));
        if (!made_[i])
        {
            texts_[i] = Locale::Translate(Locale::Lang(i), english_);
            made_[i] = true;
        }
        return texts_[i];
    }

private:
    std::string const& english_;
    std::string texts_[2];
    bool made_[2] = { false, false };
};

} // namespace

void ChannelSay(Player* from, Channel* channel, std::string const& english)
{
    if (!from || !channel) return;
    TextByLang text(english);
    channel->Say(from->GetGUID(), LANG_UNIVERSAL, std::ref(text));
}

void GuildSay(Player* from, Guild* guild, bool officer, std::string const& english)
{
    if (!from || !guild) return;
    TextByLang text(english);
    guild->BroadcastToGuild(from->GetSession(), officer, std::ref(text), LANG_UNIVERSAL);
}

LocalizedHandler::LocalizedHandler(ChatHandler* original)
    : ChatHandler(original ? original->GetSession() : nullptr), original_(original)
{
    if (original_ && original_->GetSession())
    {
        lang_ = LangOf(original_->GetSession()->GetPlayer());
        active_ = lang_ != Locale::Lang::En;
    }
}

LocalizedHandler::LocalizedHandler(WorldSession* session)
    : ChatHandler(session), original_(nullptr)
{
    if (session)
    {
        lang_ = LangOf(session->GetPlayer());
        active_ = lang_ != Locale::Lang::En;
    }
}

LocalizedHandler::~LocalizedHandler()
{
    if (active_ && original_ && HasSentErrorMessage())
        original_->SetSentErrorMessage(true);
}

void LocalizedHandler::SendSysMessage(std::string_view str, bool escapeCharacters)
{
    // One line per packet, as the base handler sends them; each line is its own catalog lookup.
    std::string out;
    bool first = true;
    for (std::string_view line : Trinity::Tokenize(str, '\n', true))
    {
        if (!first) out += '\n';
        out += Locale::Translate(lang_, line);
        first = false;
    }
    ChatHandler::SendSysMessage(out, escapeCharacters);
}

} // namespace Playerbot::BotLanguage
