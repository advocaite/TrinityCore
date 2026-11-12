// BotChatModelClient - the HTTP side of the optional bot chat model (Playerbot.ChatModel.*).
//
// Talks to a local Ollama server (POST /api/chat, stream off). Holds no game state and includes no game headers, so it can
// be built and tested on its own. One Client per thread: Chat() blocks its caller (BotChatModel's worker thread, never the
// world thread) until the reply, an error or the timeout. Cancel() may be called from any thread and makes a running Chat()
// return at once.

#pragma once

#include <boost/asio/io_context.hpp>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace Playerbot::ChatModel {

struct Settings
{
    std::string   host        = "127.0.0.1";
    std::uint16_t port        = 11434;
    std::string   model       = "llama3.2:3b";
    std::uint32_t timeout_ms  = 45000;
    std::uint32_t max_tokens  = 60;
    float         temperature = 0.8f;
    std::string   keep_alive  = "30m";   // how long Ollama keeps the model loaded after a reply
    std::uint32_t context_length = 2048;  // options.num_ctx: Ollama's default context can be 32k, a multi-GB KV cache
    bool          think       = false;   // let a reasoning model (qwen3, deepseek-r1...) think before it answers
};

struct Message
{
    std::string role;      // "system", "user" or "assistant"
    std::string content;
};

// The /api/chat request body.
std::string BuildRequestBody(Settings const& s, std::vector<Message> const& messages);

// Reads message.content (or Ollama's "error") out of an /api/chat response body.
bool ParseReply(std::string const& body, std::string& out, std::string& error);

// Turns a raw model reply into one chat line a bot can say: drops a <think> block, keeps the first non-empty line, strips a
// leading "Name:" (also "<Name>:" / "[Name]:") and wrapping quotes or asterisks, removes '|' (the client's escape character) and characters the game
// font has no glyph for (emoji), and cuts it to `max_bytes` on a word boundary. Empty when nothing usable is left.
std::string CleanReply(std::string const& raw, std::string const& bot_name, std::size_t max_bytes = 250);

class Client
{
public:
    // Sends one chat request. True with `out` set to the model's raw text (CleanReply makes it a chat line); false with `error` set.
    bool Chat(Settings const& s, std::vector<Message> const& messages, std::string& out, std::string& error);

    // Ends a running Chat() and makes every later one fail at once (the client is done: used at shutdown).
    void Cancel();

private:
    boost::asio::io_context ioc_;
    std::atomic<bool>       cancelled_{false};
};

} // namespace Playerbot::ChatModel
