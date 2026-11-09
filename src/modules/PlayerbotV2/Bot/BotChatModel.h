// BotChatModel - optional chat-model replies for bots (Playerbot.ChatModel.*, off by default).
//
// With Playerbot.ChatModel.Enable = 1, a whisper to a bot and party chat with a bot are answered by a local Ollama server
// instead of the keyword pools. The world thread only builds the prompt and queues it (Ask); one worker thread sends the
// requests, one at a time, and parks finished replies; DeliverReplies (world thread, every update) hands them to the bots'
// intent queues as WhisperIntent / PartyChatIntent. Nothing here ever waits on the network from the world thread.
//
// Limits: a per-bot cooldown, a realm-wide cap per minute and a short queue (Ask returns false when any of them is hit, and
// the caller falls back to its usual reply). After a failed request (Ollama not running, model missing, timeout) Ask declines
// for a minute, so bots keep talking the usual way. A reply that arrives after Playerbot.ChatModel.MaxWaitMs is dropped. Each bot
// remembers the last few lines with each player (HistoryLines) so a conversation can follow on.

#pragma once

#include "BotTypes.h"

#include <string>
#include <utility>
#include <vector>

class Player;

namespace Playerbot {

namespace V2 { struct ConversationContext; }

class BotChatModel
{
public:
    // Say: a bot-to-bot gathering line (AskSay). SayReply: a bot answering a player's /say that addressed it. Each answer
    // goes out on the channel it was asked on (whisper, party, raid, /say).
    enum class Channel : uint8 { Whisper, Party, Say, SayReply, Raid };

    // On in the config, and not resting after a failed request.
    static bool WhispersOn();
    static bool PartyOn();
    static bool SayReplyOn();

    // World thread. Queues `msg` from `sender` for `bot` to answer on `channel`; `ctx` describes the bot (BotChatReactor's
    // MakeConvContext). False when the request was not queued (off, cooling down, over the cap, queue full).
    // `extra_delay_ms` holds the answer back that much longer (a second bot chiming in after the first).
    static bool Ask(Player* bot, Player* sender, V2::ConversationContext const& ctx, Channel channel, std::string const& msg,
                    uint32 extra_delay_ms = 0);

    // World thread. A /say line for a bot talking with other bots (the gate gatherings, Fleet/Gathering.h): `system` sets the scene,
    // `chat` is the talk so far as "Name: line" rows (empty: the bot opens it). Under its own `per_minute` cap, and only while the
    // queue is at most half full so a player's whisper always has room. The reply is said in /say and kept for TakeSaid(`key`).
    static bool AskSay(Player* bot, std::string const& system, std::string const& chat, uint64 key, uint32 per_minute);
    // World thread. The lines said for `key` since the last call (name, line), oldest first.
    static std::vector<std::pair<std::string, std::string>> TakeSaid(uint64 key);
    // On in the config and not resting after a failed request.
    static bool SayOn();

    // World thread (from DeliverReplies): once chat is on, and again when the model, host or port changes, sends a one-token
    // request so Ollama loads the model before the first player line instead of during it. Not while the bot bridge is set to
    // answer chat (Playerbot.Bridge.Enable and .Chat): its program brings its own model.
    static void WarmUp();

    // World thread, from Module::OnWorldUpdate.
    static void DeliverReplies(uint32 now_ms);

    // Stops the worker (cancels a request in flight). From Module::Shutdown.
    static void Shutdown();
};

} // namespace Playerbot
