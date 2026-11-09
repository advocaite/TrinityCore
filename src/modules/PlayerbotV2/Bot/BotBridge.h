// BotBridge - an optional file bridge between the bots and an outside program (Playerbot.Bridge.*, off by default).
//
// With Playerbot.Bridge.Enable = 1 the server writes what happens to the bots into JSON-lines files in Playerbot.Bridge.Folder:
// chat aimed at a bot (chat_in), a bot stuck on a goal (stuck), a quest the bot had to drop (quest_fail), a spell failing over and
// over (spell_fail), a bot dying again and again in one place (death_loop), and server start/stop (start carries the newest crash
// report when the last stop was not a clean shutdown). Any program on the same PC (a local model, a bug collector, a player's own
// tool) reads those and may write lines to replies.jsonl: the bot then says them, on the channel it was asked on. The format is
// in BotBridgeFormat.h and doc/BOT_BRIDGE.md in the module.
//
// Off: nothing runs (no folder, no thread, every Report* returns at once). On but no program answering: bots talk and act exactly as
// before (chat goes to the bridge only while a program has written to replies.jsonl in the last Playerbot.Bridge.ClientTimeoutSec).
// A reply is text, plus (only with Playerbot.Bridge.Actions.Enable = 1) one action from a fixed list of ordinary bot orders, which the
// server checks (who asked, where the bot is, distance, combat, rate limits) before the bot does it; see BotBridgeFormat.h Act.
// A broken line is skipped.
//
// Threads: Report* may be called from any thread (they only queue). Update, OnStartup and Shutdown run on the world thread. Writing
// files happens on one small worker thread; when the disk is slow its queue drops the oldest lines instead of slowing the game.

#pragma once

#include "BotTypes.h"
#include "ObjectGuid.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class Player;

namespace Playerbot {

class BotBridge
{
public:
    // On in the config (cheap: one atomic read).
    static bool Enabled();

    // Chat should go to the bridge: Enabled, Playerbot.Bridge.Chat on, and a program answered recently.
    static bool ChatOn();

    // World thread. Writes a chat_in event for `bot` and returns its id (0 when not written). `channel` is say/party/raid/whisper,
    // `prompt` the bot description the built-in chat model would have used (a program may use it or ignore it).
    // `group`: the line is an order to these bots of the sender's (bot itself among them): the program picks one action and every
    // one of them runs it (each with its own checks), while only `bot` answers.
    static uint64 ReportChat(Player* bot, Player* sender, std::string const& channel, std::string const& text, std::string const& prompt,
                             std::vector<BotId> const& group = {});
    // World thread. A party / raid order the bot command parser does not know ("all go to the bank and wait"): to the program
    // when it answers chat with actions on, as a group order for `group` (one bot: an order to that bot only). False: not sent.
    static bool AskGroupOrder(Player* sender, std::vector<Player*> const& group, std::string const& channel, std::string const& text);

    // Any thread. A bot problem; `detail` says what went wrong in words. quest_id 0: the bot's current quest is looked up.
    static void ReportStuck(BotId bot, std::string detail);
    static void ReportQuestFail(BotId bot, uint32 quest_id, std::string detail);
    // World thread (from the cast path): counts failures and reports a spell failing Playerbot.Bridge.SpellFailCount times in a minute.
    static void NoteCastFail(BotId bot, uint32 spell_id, uint32 reason);
    // World thread (from Module::OnDeath): reports a bot dying Playerbot.Bridge.DeathLoopCount times in 10 minutes within 60 yards.
    static void NoteDeath(Player* bot, std::string const& killer);

    // Any thread. The bots' own party lines come from the program now (Playerbot.Bridge.Moments, a program answering chat).
    static bool MomentsOn();
    // Any thread. Hands a moment to the program: `what` a short word (loot, level_up, wipe, boss_kill, ...), `detail` what happened
    // in plain words, `example` the fixed line the bot would have said (an idea for the program, may be empty). True: queued, the
    // caller says nothing itself. False (moments off, MomentsPerMinute reached): the caller says its fixed line as before.
    static bool QueueMoment(BotId bot, std::string what, std::string detail, std::string example);

    // World bots talking with each other (Playerbot.Bridge.Talk): the program writes their lines. World thread.
    static bool TalkOn();
    // Asks the program for `bot`'s next line in a talk: `scene` who it is with, where and what about, `chat` the lines so far ("Name:
    // line" each), `key` the talk. False (talk off, `per_minute` reached, players' chat waiting): the caller does without. The line
    // is said in /say and kept for TakeTalk(`key`), like BotChatModel::AskSay.
    static bool AskTalk(Player* bot, std::string const& scene, std::string const& chat, uint64 key, uint32 per_minute);
    static std::vector<std::pair<std::string, std::string>> TakeTalk(uint64 key);
    // Any thread (idle:react_to_passerby): `bot` just waved at the real player `player` walking past; with Talk on the program may
    // add a short /say line to him (a greeting, a word about his mount). Counted with the talk (Gathering.ChatModelPerMinute).
    static bool QueueGreet(BotId bot, ObjectGuid player);

    // Any thread (the bot worker's channel suggestions, BotTextMgr Suggest). With Talk on, a world bot's post in General (channel 1)
    // or Trade (2) is written by the program from `detail` (what it is about, in words); `link_name` / `link`: an item or quest whose
    // name in the line becomes its link; `example` the fixed line, said when the model leaves the name out. Now and then another
    // world bot answers it in the channel. True: queued, the caller says nothing. False: the caller posts its fixed line.
    static bool QueuePost(BotId bot, uint8 channel_id, std::string what, std::string detail, std::string example, std::string link_name,
                          std::string link);

    // World thread, from Module::OnWorldUpdate: writes queued events, reads new reply lines, status every 10 s.
    static void Update(uint32 now_ms);

    // World thread, from Module::Shutdown: writes the stop event, marks the stop as clean, stops the worker.
    static void Shutdown();
};

} // namespace Playerbot
