// BotChatModelClient - see header.

#include "BotChatModelClient.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>

#include "Errors.h"   // rapidjson's RAPIDJSON_ASSERT is WPAssert in this tree

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>

namespace Playerbot::ChatModel {

namespace beast = boost::beast;
namespace http  = boost::beast::http;
using tcp       = boost::asio::ip::tcp;

std::string BuildRequestBody(Settings const& s, std::vector<Message> const& messages)
{
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> w(buf);
    w.StartObject();
    w.Key("model");      w.String(s.model.c_str(), rapidjson::SizeType(s.model.size()));
    w.Key("stream");     w.Bool(false);
    w.Key("keep_alive"); w.String(s.keep_alive.c_str(), rapidjson::SizeType(s.keep_alive.size()));
    // A reasoning model (qwen3, qwen3.5, deepseek-r1...) thinks first by default, and its thinking used up the whole
    // MaxTokens budget, so the reply came back empty and the bot never answered. Only "think": true is refused by a
    // model that cannot think, so false is safe for every model.
    if (!s.think)
    {
        w.Key("think"); w.Bool(false);
    }
    w.Key("messages");
    w.StartArray();
    for (Message const& m : messages)
    {
        w.StartObject();
        w.Key("role");    w.String(m.role.c_str(), rapidjson::SizeType(m.role.size()));
        w.Key("content"); w.String(m.content.c_str(), rapidjson::SizeType(m.content.size()));
        w.EndObject();
    }
    w.EndArray();
    w.Key("options");
    w.StartObject();
    w.Key("num_predict"); w.Uint(s.max_tokens);
    w.Key("num_ctx");     w.Uint(s.context_length);
    w.Key("temperature"); w.Double(std::round(double(s.temperature) * 100.0) / 100.0);
    w.EndObject();
    w.EndObject();
    return std::string(buf.GetString(), buf.GetSize());
}

bool ParseReply(std::string const& body, std::string& out, std::string& error)
{
    rapidjson::Document doc;
    doc.Parse(body.c_str(), body.size());
    if (doc.HasParseError() || !doc.IsObject())
    {
        error = "reply is not JSON";
        return false;
    }
    if (auto e = doc.FindMember("error"); e != doc.MemberEnd() && e->value.IsString())
    {
        error = e->value.GetString();
        return false;
    }
    auto m = doc.FindMember("message");
    if (m == doc.MemberEnd() || !m->value.IsObject())
    {
        error = "reply has no message";
        return false;
    }
    auto c = m->value.FindMember("content");
    if (c == m->value.MemberEnd() || !c->value.IsString())
    {
        error = "reply has no content";
        return false;
    }
    out.assign(c->value.GetString(), c->value.GetStringLength());
    return true;
}

namespace {

std::string Trim(std::string s)
{
    auto const not_space = [](unsigned char ch) { return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::string Lower(std::string s)
{
    for (char& ch : s)
        ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

// Keeps what the game font can draw: ASCII (minus control characters and '|') and 2-byte UTF-8 (accented letters). Typographic
// quotes, dashes and the ellipsis become their ASCII forms; other symbols from U+2000 up and every 4-byte sequence (emoji) go.
std::string FilterChars(std::string const& in)
{
    std::string out;
    out.reserve(in.size());
    std::size_t i = 0;
    while (i < in.size())
    {
        unsigned char const b = static_cast<unsigned char>(in[i]);
        if (b < 0x80)
        {
            if (b == '|')
                ;
            else if (b < 0x20 || b == 0x7F)
                out += ' ';
            else
                out += char(b);
            ++i;
            continue;
        }
        std::size_t len = (b & 0xE0) == 0xC0 ? 2 : (b & 0xF0) == 0xE0 ? 3 : (b & 0xF8) == 0xF0 ? 4 : 1;
        if (i + len > in.size())
            break;
        bool valid = len > 1;
        for (std::size_t k = 1; valid && k < len; ++k)
            valid = (static_cast<unsigned char>(in[i + k]) & 0xC0) == 0x80;
        if (!valid)
        {
            ++i;
            continue;
        }
        if (len == 2)
            out.append(in, i, 2);
        else if (len == 3)
        {
            std::uint32_t const cp = (std::uint32_t(b & 0x0F) << 12) | (std::uint32_t(in[i + 1] & 0x3F) << 6) | std::uint32_t(in[i + 2] & 0x3F);
            if (cp == 0x2018 || cp == 0x2019)
                out += '\'';
            else if (cp == 0x201C || cp == 0x201D)
                out += '"';
            else if (cp == 0x2013 || cp == 0x2014)
                out += '-';
            else if (cp == 0x2026)
                out += "...";
            else if (cp < 0x2000)
                out.append(in, i, 3);
        }
        i += len;
    }
    return out;
}

} // namespace

std::string CleanReply(std::string const& raw, std::string const& bot_name, std::size_t max_bytes)
{
    std::string s = raw;

    // A reasoning model's <think> block (an unclosed one drops everything after it).
    for (std::size_t open; (open = s.find("<think>")) != std::string::npos;)
    {
        std::size_t const close = s.find("</think>", open);
        s.erase(open, close == std::string::npos ? std::string::npos : close + 8 - open);
    }

    // First non-empty line.
    {
        std::string line;
        std::size_t start = 0;
        while (start <= s.size())
        {
            std::size_t const nl = s.find('\n', start);
            line = Trim(s.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
            if (!line.empty() || nl == std::string::npos)
                break;
            start = nl + 1;
        }
        s = line;
    }

    s = Trim(FilterChars(s));

    // "*waves*" style actions.
    for (std::size_t open; (open = s.find('*')) != std::string::npos;)
    {
        std::size_t const close = s.find('*', open + 1);
        s.erase(open, close == std::string::npos ? 1 : close + 1 - open);
    }
    s = Trim(s);

    // "Name: text", "<Name>: text", "[Name]: text" - the model speaking as the bot by name (the add-on also hides a whisper
    // that starts "<Name>:" as an order confirmation).
    if (std::size_t const colon = s.find(':'); colon != std::string::npos && colon <= 26)
    {
        std::string head = Lower(Trim(s.substr(0, colon)));
        if (head.size() >= 2 && ((head.front() == '<' && head.back() == '>') || (head.front() == '[' && head.back() == ']')))
            head = Trim(head.substr(1, head.size() - 2));
        if (!bot_name.empty() && head == Lower(bot_name))
            s = Trim(s.substr(colon + 1));
    }

    // Wrapping quotes.
    while (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')))
        s = Trim(s.substr(1, s.size() - 2));

    // Collapse runs of spaces left by the filters.
    {
        std::string collapsed;
        collapsed.reserve(s.size());
        for (char ch : s)
            if (ch != ' ' || collapsed.empty() || collapsed.back() != ' ')
                collapsed += ch;
        s = collapsed;
    }

    if (s.size() > max_bytes)
    {
        std::size_t cut = max_bytes;
        while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80)
            --cut;
        std::size_t const space = s.rfind(' ', cut);
        if (space != std::string::npos && space > max_bytes / 2)
            cut = space;
        s = Trim(s.substr(0, cut));
    }
    return s;
}

bool Client::Chat(Settings const& s, std::vector<Message> const& messages, std::string& out, std::string& error)
{
    ioc_.restart();
    if (cancelled_)
    {
        error = "cancelled";
        return false;
    }

    tcp::resolver resolver(ioc_);
    beast::tcp_stream stream(ioc_);
    beast::flat_buffer buffer;
    http::response<http::string_body> res;

    http::request<http::string_body> req{http::verb::post, "/api/chat", 11};
    req.set(http::field::host, s.host + ":" + std::to_string(s.port));
    req.set(http::field::content_type, "application/json");
    req.body() = BuildRequestBody(s, messages);
    req.prepare_payload();

    beast::error_code result;
    char const* stage = "resolve";
    // One deadline for the whole exchange: resolve, connect, send and the (slow) generation.
    stream.expires_after(std::chrono::milliseconds(s.timeout_ms));
    resolver.async_resolve(s.host, std::to_string(s.port),
        [&](beast::error_code ec, tcp::resolver::results_type results)
        {
            if (ec) { result = ec; return; }
            stage = "connect";
            stream.async_connect(results, [&](beast::error_code ec2, tcp::endpoint const&)
            {
                if (ec2) { result = ec2; return; }
                stage = "send";
                http::async_write(stream, req, [&](beast::error_code ec3, std::size_t)
                {
                    if (ec3) { result = ec3; return; }
                    stage = "read";
                    http::async_read(stream, buffer, res, [&](beast::error_code ec4, std::size_t)
                    {
                        result = ec4;
                        if (!ec4)
                            stage = "done";
                    });
                });
            });
        });
    ioc_.run();

    beast::error_code ignored;
    stream.socket().shutdown(tcp::socket::shutdown_both, ignored);

    if (result || std::string_view(stage) != "done")
    {
        error = std::string(stage) + ": " + (result ? result.message() : std::string("cancelled"));
        return false;
    }
    if (res.result() != http::status::ok)
    {
        std::string unused, detail;
        ParseReply(res.body(), unused, detail);
        error = "HTTP " + std::to_string(res.result_int()) + (detail.empty() ? std::string() : ": " + detail);
        return false;
    }
    return ParseReply(res.body(), out, error);
}

void Client::Cancel()
{
    cancelled_ = true;
    ioc_.stop();
}

} // namespace Playerbot::ChatModel
