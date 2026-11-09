// BotChatModel - see header.

#include "BotChatModel.h"
#include "BotBridge.h"
#include "BotChatModelClient.h"
#include "BotConversation.h"
#include "BotIntent.h"
#include "../Services.h"
#include "../Threading/IntentQueue.h"
#include "../Util/ConfigReader.h"

#include "Log.h"
#include "GameTime.h"
#include "Player.h"
#include "Util.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <ctime>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Playerbot {

namespace {

using Clock = std::chrono::steady_clock;

struct Job
{
    BotId                          bot = 0;
    std::string                    bot_name;
    std::string                    sender_name;
    BotChatModel::Channel          channel = BotChatModel::Channel::Whisper;
    uint64                         history_key = 0;
    std::string                    question;
    ChatModel::Settings            settings;
    std::vector<ChatModel::Message> messages;
    Clock::time_point              asked;
    uint32                         max_wait_ms = 30000;
    uint32                         extra_delay_ms = 0;
};

struct Done
{
    BotId                 bot = 0;
    std::string           bot_name;
    std::string           sender_name;
    BotChatModel::Channel channel = BotChatModel::Channel::Whisper;
    uint64                history_key = 0;
    std::string           question;
    std::string           answer;
    uint32                took_ms = 0;
    uint32                extra_delay_ms = 0;
};

// Shared between the world thread and the worker, under g_mtx.
std::mutex              g_mtx;
std::condition_variable g_cv;
std::deque<Job>         g_jobs;
std::vector<Done>       g_done;
bool                    g_stop = false;
ChatModel::Client*      g_client = nullptr;   // the worker's client while it runs (for Cancel)
std::thread             g_worker;

// After a failed request the model counts as down for a minute: Ask declines, so bots give their usual replies meanwhile
// instead of going quiet. Written by the worker, read by the world thread (steady_clock ticks).
std::atomic<Clock::rep> g_down_until{0};
constexpr std::chrono::seconds kDownFor{60};

// World thread only.
struct History
{
    std::deque<ChatModel::Message> lines;   // oldest first, always starting with the player's line
    Clock::time_point              last;    // the last exchange
};
std::unordered_map<uint64, History> g_history;   // (bot << 32 | sender) -> the conversation so far
std::unordered_map<BotId, Clock::time_point>               g_last_ask;
std::deque<Clock::time_point>                              g_minute;      // asks in the last 60 s
std::deque<Clock::time_point>                              g_say_minute;  // AskSay asks in the last 60 s (their own cap)
std::unordered_map<uint64, std::vector<std::pair<std::string, std::string>>> g_said;   // AskSay key -> lines said, not yet taken

ChatModel::Settings SettingsFrom(ConfigReader::ChatModelConfig const& cfg)
{
    ChatModel::Settings s;
    s.host = cfg.host;
    s.port = uint16(cfg.port);
    s.model = cfg.model;
    s.timeout_ms = cfg.timeout_ms;
    s.max_tokens = cfg.max_tokens;
    s.temperature = cfg.temperature;
    s.keep_alive = cfg.keep_alive;
    s.context_length = cfg.context_length;
    s.think = cfg.think;
    return s;
}

// World thread only: the model (host:port/model) the last warm-up was sent for.
std::string g_warmed;

bool IsDown()
{
    return Clock::now().time_since_epoch().count() < g_down_until.load();
}

uint32 MsSince(Clock::time_point t)
{
    return uint32(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t).count());
}

void WorkerLoop()
{
    ChatModel::Client client;
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_client = &client;
    }
    Clock::time_point last_warn{};
    uint32 failures_since_warn = 0;
    for (;;)
    {
        Job job;
        {
            std::unique_lock<std::mutex> lk(g_mtx);
            g_cv.wait(lk, [] { return g_stop || !g_jobs.empty(); });
            if (g_stop)
                break;
            job = std::move(g_jobs.front());
            g_jobs.pop_front();
        }
        // Waited too long in the queue: the moment has passed.
        if (MsSince(job.asked) > job.max_wait_ms)
            continue;

        Clock::time_point const start = Clock::now();
        std::string raw, error;
        bool const ok = client.Chat(job.settings, job.messages, raw, error);
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            if (g_stop)
                break;
        }
        if (job.bot == 0)   // warm-up: only loads the model
        {
            if (ok)
                TC_LOG_INFO("server.worldserver", "[PlayerbotV2] Chat model {} loaded and ready ({} ms).", job.settings.model, MsSince(start));
        }
        if (!ok)
        {
            // Ollama not running or the model not pulled is the usual cause: say so, at most once a minute.
            g_down_until = (Clock::now() + kDownFor).time_since_epoch().count();
            ++failures_since_warn;
            if (last_warn == Clock::time_point{} || MsSince(last_warn) > 60000)
            {
                TC_LOG_WARN("server.worldserver", "[PlayerbotV2] Chat model request to {}:{} ({}) failed: {} ({} failure(s) since the last warning)."
                    " Is Ollama running, and has the model been pulled? Bots use their usual replies for a minute.", job.settings.host, job.settings.port, job.settings.model, error,
                    failures_since_warn);
                last_warn = Clock::now();
                failures_since_warn = 0;
            }
            continue;
        }
        if (job.bot == 0)
            continue;
        std::string answer = ChatModel::CleanReply(raw, job.bot_name);
        if (answer.empty())
        {
            TC_LOG_INFO("playerbot.v2", "[conv] {} <- {}: chat model reply unusable: \"{}\"", job.bot_name, job.sender_name, raw);
            continue;
        }
        Done d;
        d.bot = job.bot;
        d.bot_name = std::move(job.bot_name);
        d.sender_name = std::move(job.sender_name);
        d.channel = job.channel;
        d.history_key = job.history_key;
        d.question = std::move(job.question);
        d.answer = std::move(answer);
        d.took_ms = MsSince(start);
        d.extra_delay_ms = job.extra_delay_ms;
        std::lock_guard<std::mutex> lk(g_mtx);
        // Too late to answer (asked > MaxWaitMs ago): drop it rather than reply to a conversation that moved on.
        if (MsSince(job.asked) > job.max_wait_ms)
            continue;
        g_done.push_back(std::move(d));
    }
    std::lock_guard<std::mutex> lk(g_mtx);
    g_client = nullptr;
}

char const* RaceName(uint8 race)
{
    static constexpr char const* kRace[] = { "", "human", "orc", "dwarf", "night elf", "undead", "tauren", "gnome", "troll",
                                              "goblin", "blood elf", "draenei" };
    return race < sizeof(kRace) / sizeof(kRace[0]) && kRace[race][0] ? kRace[race] : "";
}

// True when most letters in `msg` are Cyrillic (UTF-8 lead bytes 0xD0 / 0xD1: U+0400-U+047F).
bool IsMostlyCyrillic(std::string const& msg)
{
    std::size_t cyrillic = 0, latin = 0;
    for (std::size_t i = 0; i < msg.size(); ++i)
    {
        unsigned char const b = static_cast<unsigned char>(msg[i]);
        if ((b == 0xD0 || b == 0xD1) && i + 1 < msg.size())
        {
            ++cyrillic;
            ++i;
        }
        else if (std::isalpha(b))
            ++latin;
    }
    return cyrillic > 0 && cyrillic >= latin;
}

// The language line. "auto": the player's language - named outright for Cyrillic (small models answer an English prompt in
// English unless told), otherwise "the same language". Any other setting ("Russian", "German", ...) is used as given.
std::string LanguageLine(std::string const& setting, std::string const& msg)
{
    if (setting.empty() || setting == "auto" || setting == "Auto")
        return IsMostlyCyrillic(msg) ? " Reply in Russian." : " Reply in the same language the player wrote in.";
    return " Always reply in " + setting + ".";
}

std::string BuildSystemPrompt(Player* bot, V2::ConversationContext const& ctx, BotChatModel::Channel channel,
                              std::string const& language, std::string const& msg, std::string const& extra, bool date)
{
    std::string p = "You are " + ctx.self_name + ", a level " + std::to_string(ctx.level);
    if (char const* race = RaceName(bot->GetRace()); *race)
        p += std::string(" ") + race;
    p += " " + (ctx.class_name.empty() ? std::string("adventurer") : ctx.class_name);
    p += " playing World of Warcraft Classic (the original 2004 Azeroth, levels 1-60).";
    if (!ctx.zone.empty())
        p += " You are in " + ctx.zone + ".";
    if (!ctx.activity.empty())
        p += " Right now you are " + ctx.activity + ".";
    if (!ctx.quest_title.empty())
        p += " One quest in your log is \"" + ctx.quest_title + "\".";
    if (!ctx.guild_name.empty())
        p += " Your guild is <" + ctx.guild_name + ">.";
    if (!ctx.professions.empty())
    {
        p += " Your professions:";
        for (std::size_t i = 0; i < ctx.professions.size(); ++i)
            p += (i ? ", " : " ") + ctx.professions[i];
        p += ".";
    }
    if (channel == BotChatModel::Channel::SayReply)
        p += " " + ctx.sender_name + " is standing next to you and says something to you out loud.";
    else if (channel == BotChatModel::Channel::Whisper)
        p += " " + ctx.sender_name + (ctx.knows_sender ? ", who is in your group," : ", a player you don't know,") + " whispers you.";
    else if (channel == BotChatModel::Channel::Raid)
        p += " You are in a raid group and " + ctx.sender_name + " says something in raid chat.";
    else
        p += " You are in a party and " + ctx.sender_name + " says something in party chat.";
    p += " Answer as a real player typing in game chat: one short line, under 20 words, casual, no emojis, no actions in"
         " asterisks, no quotation marks, no name prefix. Never say you are a bot or a program. If you don't know"
         " something, say so briefly rather than making it up. Don't invent quests, items or places.";
    switch (ctx.tone)
    {
        case V2::ConvTone::Rude:   p += " You are blunt and a bit grumpy."; break;
        case V2::ConvTone::Polite: p += " You are friendly and polite."; break;
        default: break;
    }
    if (ctx.length == V2::ConvLength::Terse)
        p += " You type as little as possible.";
    else if (ctx.length == V2::ConvLength::Chatty)
        p += " You enjoy a chat.";
    if (date)
    {
        // The server's real clock: a small model otherwise makes a date up.
        tm const t = TimeBreakdown(GameTime::GetGameTime());
        char buf[96];
        if (std::strftime(buf, sizeof(buf), "%A %d %B %Y, %H:%M (%Z)", &t))
            p += std::string(" The real date and time now is ") + buf + ".";
    }
    p += LanguageLine(language, msg);
    if (!extra.empty())
        p += " " + extra;
    return p;
}

} // namespace

// Player lines go to the outside program through the bot bridge (Bot/BotBridge.h) while one answers there; otherwise to Ollama.
bool BotChatModel::WhispersOn()
{
    auto const& c = Services::Config().chat_model();
    return c.whispers && (BotBridge::ChatOn() || (c.enable && !IsDown()));
}

bool BotChatModel::PartyOn()
{
    auto const& c = Services::Config().chat_model();
    return c.party && (BotBridge::ChatOn() || (c.enable && !IsDown()));
}

bool BotChatModel::SayReplyOn()
{
    auto const& c = Services::Config().chat_model();
    return c.say_reply && (BotBridge::ChatOn() || (c.enable && !IsDown()));
}

bool BotChatModel::SayOn()
{
    return BotBridge::TalkOn() || (Services::Config().chat_model().enable && !IsDown());
}

bool BotChatModel::AskSay(Player* bot, std::string const& system, std::string const& chat, uint64 key, uint32 per_minute)
{
    // With a program answering the chat (Playerbot.Bridge.Talk), it writes the bots' talk too: one model for all of it, never the
    // built-in one loaded next to it on the same graphics card.
    if (BotBridge::TalkOn())
        return BotBridge::AskTalk(bot, system, chat, key, per_minute);
    auto const& cfg = Services::Config().chat_model();
    if (!cfg.enable || !bot || IsDown())
        return false;
    BotId const id = bot->GetGUID().GetCounter();
    Clock::time_point const now = Clock::now();
    while (!g_say_minute.empty() && now - g_say_minute.front() >= std::chrono::seconds(60))
        g_say_minute.pop_front();
    if (g_say_minute.size() >= per_minute)
        return false;

    Job job;
    job.bot = id;
    job.bot_name = bot->GetName();
    job.channel = Channel::Say;
    job.history_key = key;
    job.settings = SettingsFrom(cfg);
    job.asked = now;
    job.max_wait_ms = std::min<uint32>(cfg.max_wait_ms, 20000u);   // a late line in a talk that moved on reads wrong
    job.messages.push_back({ "system", system });
    job.messages.push_back({ "user", chat.empty() ? std::string("Start the talk with one line.")
                                                  : "The talk so far:\n" + chat + "\nYour next line:" });
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (g_stop || g_jobs.size() * 2 >= cfg.max_queue)
            return false;
        if (!g_worker.joinable())
        {
            TC_LOG_INFO("server.loading", "[PlayerbotV2] Chat model on: {} at {}:{}.", cfg.model, cfg.host, cfg.port);
            g_worker = std::thread(WorkerLoop);
        }
        g_jobs.push_back(std::move(job));
    }
    g_cv.notify_one();
    g_say_minute.push_back(now);
    return true;
}

std::vector<std::pair<std::string, std::string>> BotChatModel::TakeSaid(uint64 key)
{
    std::vector<std::pair<std::string, std::string>> out = BotBridge::TakeTalk(key);
    if (!out.empty())
        return out;
    if (auto it = g_said.find(key); it != g_said.end())
    {
        out.swap(it->second);
        g_said.erase(it);
    }
    return out;
}

bool BotChatModel::Ask(Player* bot, Player* sender, V2::ConversationContext const& ctx, Channel channel, std::string const& msg,
                       uint32 extra_delay_ms)
{
    auto const& cfg = Services::Config().chat_model();
    bool const bridge = BotBridge::ChatOn();
    if ((!cfg.enable && !bridge) || !bot || !sender || msg.empty())
        return false;

    BotId const id = bot->GetGUID().GetCounter();
    Clock::time_point const now = Clock::now();

    if (!bridge && IsDown())
        return false;
    if (auto it = g_last_ask.find(id); it != g_last_ask.end() && now - it->second < std::chrono::milliseconds(cfg.bot_cooldown_ms))
        return false;
    while (!g_minute.empty() && now - g_minute.front() >= std::chrono::seconds(60))
        g_minute.pop_front();
    if (g_minute.size() >= cfg.max_per_minute)
    {
        TC_LOG_INFO("playerbot.v2", "[conv] {} <- {}: chat model skipped (MaxPerMinute {} reached)", bot->GetName(), sender->GetName(),
                    cfg.max_per_minute);
        return false;
    }

    if (bridge)
    {
        // Same limits as the model above; the outside program answers in replies.jsonl and BotBridge says the answer.
        char const* const ch = channel == Channel::Whisper ? "whisper" : channel == Channel::Party ? "party"
                             : channel == Channel::Raid ? "raid" : "say";
        std::string const prompt = BuildSystemPrompt(bot, ctx, channel, cfg.language, msg, cfg.extra_prompt, cfg.date_in_prompt);
        if (!BotBridge::ReportChat(bot, sender, ch, msg, prompt))
            return false;
        g_last_ask[id] = now;
        g_minute.push_back(now);
        TC_LOG_INFO("playerbot.v2", "[conv] {} <- {}: sent to the bot bridge ({})", bot->GetName(), sender->GetName(), ch);
        return true;
    }

    Job job;
    job.bot = id;
    job.bot_name = bot->GetName();
    job.sender_name = sender->GetName();
    job.channel = channel;
    job.history_key = (uint64(id) << 32) | uint64(uint32(sender->GetGUID().GetCounter()));
    job.question = msg;
    job.settings = SettingsFrom(cfg);
    job.asked = now;
    job.max_wait_ms = cfg.max_wait_ms + extra_delay_ms;
    job.extra_delay_ms = extra_delay_ms;
    job.messages.push_back({ "system", BuildSystemPrompt(bot, ctx, channel, cfg.language, msg, cfg.extra_prompt, cfg.date_in_prompt) });
    // The conversation so far with this player, unless it went quiet for HistoryMinutes (a new talk starts fresh).
    if (auto h = g_history.find(job.history_key); h != g_history.end())
    {
        if (now - h->second.last > std::chrono::minutes(cfg.history_minutes))
            g_history.erase(h);
        else
            for (ChatModel::Message const& m : h->second.lines)
                job.messages.push_back(m);
    }
    job.messages.push_back({ "user", msg });

    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (g_stop)
            return false;
        if (g_jobs.size() >= cfg.max_queue)
        {
            TC_LOG_INFO("playerbot.v2", "[conv] {} <- {}: chat model skipped (queue full, MaxQueue {})", job.bot_name, job.sender_name,
                        cfg.max_queue);
            return false;
        }
        if (!g_worker.joinable())
        {
            TC_LOG_INFO("server.loading", "[PlayerbotV2] Chat model on: {} at {}:{}.", cfg.model, cfg.host, cfg.port);
            g_worker = std::thread(WorkerLoop);
        }
        g_jobs.push_back(std::move(job));
    }
    g_cv.notify_one();

    g_last_ask[id] = now;
    g_minute.push_back(now);
    if (g_last_ask.size() > 4096)
        std::erase_if(g_last_ask, [&](auto const& kv) { return now - kv.second > std::chrono::minutes(10); });
    return true;
}

void BotChatModel::WarmUp()
{
    auto const& cfg = Services::Config().chat_model();
    if (!cfg.enable)
        return;
    // With the bot bridge answering chat, the outside program brings its own model. Loading this one too kept a second model
    // (llama3.2:3b, 3 GB) in the graphics card next to the program's qwen2.5:14b (Karrz's PC, 2026-11-08). It is loaded on the
    // first line it really has to answer: when the program is not running.
    if (auto const& bridge = Services::Config().bridge(); bridge.enable && bridge.chat)
        return;
    std::string const which = cfg.host + ":" + std::to_string(cfg.port) + "/" + cfg.model;
    if (which == g_warmed)
        return;
    g_warmed = which;

    Job job;
    job.settings = SettingsFrom(cfg);
    // A cold load reads the whole model from disk; give it more than a normal reply.
    job.settings.timeout_ms = std::max<uint32>(cfg.timeout_ms, 120000u);
    job.settings.max_tokens = 1;
    job.asked = Clock::now();
    job.max_wait_ms = 300000u;
    job.messages.push_back({ "user", "hi" });
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (g_stop)
            return;
        if (!g_worker.joinable())
        {
            TC_LOG_INFO("server.loading", "[PlayerbotV2] Chat model on: {} at {}:{}; loading it now.", cfg.model, cfg.host, cfg.port);
            g_worker = std::thread(WorkerLoop);
        }
        g_jobs.push_front(std::move(job));
    }
    g_cv.notify_one();
}

void BotChatModel::DeliverReplies(uint32 now_ms)
{
    WarmUp();
    std::vector<Done> done;
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        if (g_done.empty())
            return;
        done.swap(g_done);
    }
    uint32 const history_lines = Services::Config().chat_model().history_lines;
    for (Done& d : done)
    {
        if (!Services::HasIntents(d.bot))   // logged out meanwhile
            continue;
        if (d.channel == Channel::Say)
        {
            TC_LOG_INFO("playerbot.v2", "[gathering] {} says (chat model, {} ms): \"{}\"", d.bot_name, d.took_ms, d.answer);
            if (g_said.size() > 256)
                g_said.clear();
            g_said[d.history_key].emplace_back(d.bot_name, d.answer);
            uint32 const typing_ms = std::min<uint32>(uint32(d.answer.size()) * 30u, 2500u);
            Intent it{};
            it.bot_id = d.bot;
            it.body = ChatIntent{SayChatIntent{std::move(d.answer)}};
            it.defer_until_ms = now_ms + typing_ms;
            Services::Intents(d.bot).push(std::move(it));
            continue;
        }
        std::string const target = d.channel == Channel::Whisper ? "whisper to " + d.sender_name
                                 : d.channel == Channel::Party   ? std::string("party chat")
                                 : d.channel == Channel::Raid    ? std::string("raid chat")
                                                                 : std::string("/say");
        TC_LOG_INFO("playerbot.v2", "[conv] {} <- {} \"{}\": chat model ({} ms) -> {}: \"{}\"", d.bot_name, d.sender_name, d.question,
                    d.took_ms, target, d.answer);

        if (history_lines > 0)
        {
            if (g_history.size() > 2048 && !g_history.contains(d.history_key))
                g_history.clear();
            History& h = g_history[d.history_key];
            // A long player line is kept cut short: the history stays a few hundred tokens inside ContextLength.
            h.lines.push_back({ "user", d.question.size() > 300 ? d.question.substr(0, 300) : d.question });
            h.lines.push_back({ "assistant", d.answer });
            h.last = Clock::now();
            while (h.lines.size() > history_lines || (!h.lines.empty() && h.lines.front().role != "user"))
                h.lines.pop_front();
        }

        // The model already took its time; add a short "typing" pause so a long line doesn't appear instantly.
        uint32 const typing_ms = std::min<uint32>(uint32(d.answer.size()) * 30u, 2500u);
        Intent it{};
        it.bot_id = d.bot;
        if (d.channel == Channel::Whisper)
            it.body = ChatIntent{WhisperIntent{std::move(d.sender_name), std::move(d.answer)}};
        else if (d.channel == Channel::Raid)
            it.body = ChatIntent{RaidChatIntent{std::move(d.answer)}};
        else if (d.channel == Channel::Party)
            it.body = ChatIntent{PartyChatIntent{std::move(d.answer)}};
        else
            it.body = ChatIntent{SayChatIntent{std::move(d.answer)}};
        it.defer_until_ms = now_ms + typing_ms + d.extra_delay_ms;
        Services::Intents(d.bot).push(std::move(it));
    }
}

void BotChatModel::Shutdown()
{
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_stop = true;
        if (g_client)
            g_client->Cancel();
        g_jobs.clear();
        g_done.clear();
    }
    g_cv.notify_all();
    if (g_worker.joinable())
        g_worker.join();
    g_history.clear();
    g_last_ask.clear();
    g_minute.clear();
    g_say_minute.clear();
    g_said.clear();
}

} // namespace Playerbot
