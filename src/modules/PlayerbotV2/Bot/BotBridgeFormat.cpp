// BotBridgeFormat - see header.

#include "BotBridgeFormat.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string_view>

namespace Playerbot::Bridge {

std::string JsonEscape(std::string const& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s)
    {
        switch (c)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20)
                {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", unsigned(c));
                    out += buf;
                }
                else
                    out += char(c);
        }
    }
    return out;
}

namespace {

void AddString(std::string& out, char const* key, std::string const& value)
{
    out += ",\"";
    out += key;
    out += "\":\"";
    out += JsonEscape(value);
    out += '"';
}

std::string Num(float v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", double(v));
    return buf;
}

} // namespace

std::string BuildEventLine(Event const& e)
{
    std::string out = "{\"id\":" + std::to_string(e.id);
    AddString(out, "t", e.time);
    AddString(out, "type", e.type);
    if (!e.bot.empty())
    {
        AddString(out, "bot", e.bot);
        out += ",\"guid\":" + std::to_string(e.guid);
        if (!e.cls.empty())
            AddString(out, "class", e.cls);
        if (!e.race.empty())
            AddString(out, "race", e.race);
        out += ",\"level\":" + std::to_string(e.level);
        if (!e.zone.empty())
            AddString(out, "zone", e.zone);
        if (!e.area.empty() && e.area != e.zone)
            AddString(out, "area", e.area);
        out += ",\"map\":" + std::to_string(e.map);
    }
    if (e.has_pos)
        out += ",\"pos\":[" + Num(e.x) + "," + Num(e.y) + "," + Num(e.z) + "]";
    if (e.quest_id)
    {
        out += ",\"quest\":{\"id\":" + std::to_string(e.quest_id);
        if (!e.quest_name.empty())
            AddString(out, "name", e.quest_name);
        out += '}';
    }
    if (!e.detail.empty())
        AddString(out, "detail", e.detail);
    for (auto const& [k, v] : e.fields)
        AddString(out, k.c_str(), v);
    for (auto const& [k, v] : e.numbers)
        out += ",\"" + JsonEscape(k) + "\":" + std::to_string(v);
    for (auto const& [k, v] : e.raw)
        if (!v.empty())
            out += ",\"" + JsonEscape(k) + "\":" + v;
    out += '}';
    return out;
}

namespace {

struct Parser
{
    std::string const& s;
    std::size_t i = 0;

    void Ws()
    {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n'))
            ++i;
    }

    static void PutUtf8(std::string& out, std::uint32_t cp)
    {
        if (cp < 0x80)
            out += char(cp);
        else if (cp < 0x800)
        {
            out += char(0xC0 | (cp >> 6));
            out += char(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            out += char(0xE0 | (cp >> 12));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        }
        else
        {
            out += char(0xF0 | (cp >> 18));
            out += char(0x80 | ((cp >> 12) & 0x3F));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        }
    }

    bool Hex4(std::uint32_t& v)
    {
        if (i + 4 > s.size())
            return false;
        v = 0;
        for (int k = 0; k < 4; ++k)
        {
            char const c = s[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= std::uint32_t(c - '0');
            else if (c >= 'a' && c <= 'f') v |= std::uint32_t(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= std::uint32_t(c - 'A' + 10);
            else return false;
        }
        return true;
    }

    bool String(std::string& out)
    {
        if (i >= s.size() || s[i] != '"')
            return false;
        ++i;
        while (i < s.size())
        {
            char const c = s[i++];
            if (c == '"')
                return true;
            if (c != '\\')
            {
                out += c;
                continue;
            }
            if (i >= s.size())
                return false;
            char const e = s[i++];
            switch (e)
            {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u':
                {
                    std::uint32_t cp = 0;
                    if (!Hex4(cp))
                        return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u')
                    {
                        i += 2;
                        std::uint32_t lo = 0;
                        if (!Hex4(lo))
                            return false;
                        if (lo >= 0xDC00 && lo <= 0xDFFF)
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        else
                            cp = 0xFFFD;
                    }
                    else if (cp >= 0xD800 && cp <= 0xDFFF)
                        cp = 0xFFFD;
                    PutUtf8(out, cp);
                    break;
                }
                default:
                    return false;
            }
        }
        return false;
    }

    // A number, true, false or null: kept as its text.
    bool Scalar(std::string& out)
    {
        std::size_t const start = i;
        while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '-' || s[i] == '+' || s[i] == '.'))
            ++i;
        if (i == start)
            return false;
        out = s.substr(start, i - start);
        return true;
    }

    // Skips any value (nested objects and arrays too), depth limited.
    bool Skip(int depth)
    {
        if (depth > 32)
            return false;
        Ws();
        if (i >= s.size())
            return false;
        if (s[i] == '"')
        {
            std::string tmp;
            return String(tmp);
        }
        if (s[i] == '{' || s[i] == '[')
        {
            char const close = s[i] == '{' ? '}' : ']';
            bool const obj = s[i] == '{';
            ++i;
            Ws();
            if (i < s.size() && s[i] == close)
            {
                ++i;
                return true;
            }
            for (;;)
            {
                if (obj)
                {
                    Ws();
                    std::string key;
                    if (!String(key))
                        return false;
                    Ws();
                    if (i >= s.size() || s[i] != ':')
                        return false;
                    ++i;
                }
                if (!Skip(depth + 1))
                    return false;
                Ws();
                if (i >= s.size())
                    return false;
                if (s[i] == ',')
                {
                    ++i;
                    continue;
                }
                if (s[i] == close)
                {
                    ++i;
                    return true;
                }
                return false;
            }
        }
        std::string tmp;
        return Scalar(tmp);
    }
};

} // namespace

bool ParseFlatObject(std::string const& line, FlatObject& out, std::string& error)
{
    out.clear();
    Parser p{line};
    p.Ws();
    if (p.i >= line.size() || line[p.i] != '{')
    {
        error = "not a JSON object";
        return false;
    }
    ++p.i;
    p.Ws();
    if (p.i < line.size() && line[p.i] == '}')
        return true;
    for (;;)
    {
        p.Ws();
        std::string key;
        if (!p.String(key))
        {
            error = "bad key";
            return false;
        }
        p.Ws();
        if (p.i >= line.size() || line[p.i] != ':')
        {
            error = "missing ':'";
            return false;
        }
        ++p.i;
        p.Ws();
        if (p.i >= line.size())
        {
            error = "line ends early";
            return false;
        }
        char const c = line[p.i];
        if (c == '"')
        {
            std::string v;
            if (!p.String(v))
            {
                error = "bad string";
                return false;
            }
            out[key] = std::move(v);
        }
        else if (c == '{' || c == '[')
        {
            if (!p.Skip(0))
            {
                error = "bad nested value";
                return false;
            }
        }
        else
        {
            std::string v;
            if (!p.Scalar(v))
            {
                error = "bad value";
                return false;
            }
            out[key] = std::move(v);
        }
        p.Ws();
        if (p.i >= line.size())
        {
            error = "line ends early";
            return false;
        }
        if (line[p.i] == ',')
        {
            ++p.i;
            continue;
        }
        if (line[p.i] == '}')
            return true;
        error = "expected ',' or '}'";
        return false;
    }
}

namespace {

std::uint64_t ToU64(std::string const& v)
{
    if (v.empty() || !std::isdigit(static_cast<unsigned char>(v[0])))
        return 0;
    std::uint64_t n = 0;
    for (char c : v)
    {
        if (!std::isdigit(static_cast<unsigned char>(c)))
            break;
        n = n * 10 + std::uint64_t(c - '0');
    }
    return n;
}

std::string Lower(std::string v)
{
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return v;
}

// A number field ("12", "-5340.1"); false when it is missing or not a finite number.
bool ToFloat(FlatObject const& o, char const* key, float& out)
{
    auto it = o.find(key);
    if (it == o.end() || it->second.empty())
        return false;
    char* end = nullptr;
    double const v = std::strtod(it->second.c_str(), &end);
    if (end == it->second.c_str() || *end != '\0' || !std::isfinite(v) || std::fabs(v) > 100000.0)
        return false;
    out = float(v);
    return true;
}

void ReadAction(FlatObject const& o, std::string const& word, Action& a, std::string& error)
{
    a = Action{};
    a.act = ParseAct(word);
    if (a.act == Act::None)
    {
        error = "unknown action \"" + word.substr(0, 40) + "\"";
        return;
    }
    if (auto it = o.find("target"); it != o.end())
        a.target = Lower(it->second);
    if (auto it = o.find("ref"); it != o.end())
        a.ref = std::uint32_t(std::min<std::uint64_t>(ToU64(it->second), 0xFFFFFFFFu));
    if (auto it = o.find("quest"); it != o.end())
        a.quest = std::uint32_t(std::min<std::uint64_t>(ToU64(it->second), 0xFFFFFFFFu));
    if (auto it = o.find("emote"); it != o.end())
        a.emote = Lower(it->second);
    if (auto it = o.find("spell"); it != o.end())
        a.spell = it->second.substr(0, 80);
    if (auto it = o.find("place"); it != o.end())
        a.place = Lower(it->second.substr(0, 80));
    if (auto it = o.find("order"); it != o.end())
        a.order = it->second.substr(0, 120);
    if (auto it = o.find("items"); it != o.end())
        a.items = Lower(it->second.substr(0, 120));
    if (auto it = o.find("dir"); it != o.end())
        a.dir = Lower(it->second.substr(0, 20));
    else if (word == "back_up" || word == "step_back")
        a.dir = "back";
    if (float yd = 0; ToFloat(o, "yards", yd))
        a.yards = std::clamp(yd, 1.0f, 20.0f);
    a.has_point = ToFloat(o, "x", a.x) && ToFloat(o, "y", a.y);
    a.has_z = a.has_point && ToFloat(o, "z", a.z);

    auto bad = [&](std::string why) { error = std::move(why); a.act = Act::None; };
    switch (a.act)
    {
        case Act::Follow:
        case Act::Flee:
            if (!a.target.empty() && a.target != "speaker" && a.target != "owner")
                bad(std::string(ActName(a.act)) + ": \"target\" must be speaker or owner");
            break;
        case Act::Attack:
            if (!a.ref && !a.target.empty() && a.target != "speaker_target")
                bad("attack: give \"ref\" (a hostile from \"near\") or \"target\":\"speaker_target\"");
            break;
        case Act::GoTo:
            if (!a.ref && !a.has_point && a.place.empty() && a.target != "speaker" && a.target != "owner")
                bad("go_to: give \"ref\", \"place\", \"x\" and \"y\", or \"target\":\"speaker\"");
            break;
        case Act::Order:
        {
            std::string norm, why;
            if (!SafeOrder(a.order, norm, why))
                bad("order: " + why);
            else
                a.order = norm;
            break;
        }
        case Act::AcceptQuest:
        case Act::DropQuest:
            if (!a.quest)
                bad(std::string(ActName(a.act)) + ": needs \"quest\" (the quest id)");
            break;
        case Act::Cast:
            if (SpellNameKey(a.spell).empty())
                bad("cast: needs \"spell\" (a name from \"spells\")");
            else if (!a.target.empty() && a.target != "speaker" && a.target != "self" && a.target != "owner" &&
                     a.target != "speaker_target")
                bad("cast: \"target\" must be speaker, self, owner or speaker_target (or give \"ref\")");
            break;
        case Act::Step:
        {
            // The words a model is likely to use for each way.
            struct DirWord { std::string_view word; char const* dir; };
            static constexpr DirWord kDirs[] = {
                { "back", "back" }, { "backward", "back" }, { "backwards", "back" }, { "behind", "back" },
                { "forward", "forward" }, { "forwards", "forward" }, { "ahead", "forward" }, { "front", "forward" },
                { "left", "left" }, { "right", "right" },
                { "away", "away" }, { "out", "away" },
                { "closer", "closer" }, { "close", "closer" }, { "toward", "closer" }, { "towards", "closer" }, { "in", "closer" },
                { "me", "closer" }, { "to me", "closer" },
            };
            std::string dir = a.dir;
            a.dir.clear();
            for (DirWord const& d : kDirs)
                if (dir == d.word)
                    a.dir = d.dir;
            if (a.dir.empty())
                bad("step: \"dir\" must be back, forward, left, right, away or closer");
            break;
        }
        case Act::Emote:
            if (!EmoteId(a.emote))
                bad("emote: unknown emote \"" + a.emote.substr(0, 20) + "\"");
            break;
        default:
            break;
    }
}

} // namespace

bool ParseReplyLine(std::string const& line, Reply& out, std::string& error)
{
    out = Reply{};
    FlatObject o;
    if (!ParseFlatObject(line, o, error))
        return false;
    if (auto it = o.find("ping"); it != o.end() && it->second != "0" && it->second != "false" && it->second != "null")
    {
        out.ping = true;
        return true;
    }
    if (auto it = o.find("reply_to"); it != o.end())
        out.reply_to = ToU64(it->second);
    if (auto it = o.find("bot"); it != o.end())
        out.bot = it->second;
    if (auto it = o.find("say"); it != o.end())
        out.say = it->second;
    if (auto it = o.find("channel"); it != o.end())
        out.channel = Lower(it->second);
    if (auto it = o.find("to"); it != o.end())
        out.to = it->second;
    if (auto it = o.find("do"); it != o.end())
        ReadAction(o, it->second, out.action, out.action_error);
    if (auto it = o.find("delay_ms"); it != o.end())
        out.delay_ms = std::int32_t(std::min<std::uint64_t>(ToU64(it->second), 10000));
    bool const has_do = out.action.act != Act::None || !out.action_error.empty();
    if (out.say.empty() && !has_do)
    {
        error = "no \"say\" text and no \"do\"";
        return false;
    }
    if (!out.reply_to && out.bot.empty())
    {
        error = "needs \"reply_to\" or \"bot\"";
        return false;
    }
    // general / trade: only for the answer to a channel post the server asked for (BotBridge checks it).
    static char const* const kChannels[] = { "", "say", "party", "raid", "whisper", "yell", "general", "trade" };
    if (std::find_if(std::begin(kChannels), std::end(kChannels), [&](char const* c) { return out.channel == c; }) == std::end(kChannels))
    {
        error = "unknown channel \"" + out.channel + "\"";
        return false;
    }
    if (out.channel == "whisper" && !out.reply_to && out.to.empty())
    {
        error = "a whisper needs \"to\"";
        return false;
    }
    return true;
}

std::vector<std::string> SplitLines(std::string& carry, std::string const& chunk)
{
    std::vector<std::string> lines;
    carry += chunk;
    std::size_t start = 0;
    for (;;)
    {
        std::size_t const nl = carry.find('\n', start);
        if (nl == std::string::npos)
            break;
        std::string l = carry.substr(start, nl - start);
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        if (!l.empty())
            lines.push_back(std::move(l));
        start = nl + 1;
    }
    carry.erase(0, start);
    // A line that never ends (a program writing garbage without newlines) must not grow without bound.
    if (carry.size() > 64 * 1024)
        carry.clear();
    return lines;
}

bool EventGate::Allow(std::uint64_t bot, std::string const& type, std::uint64_t now_ms, std::uint32_t* repeats)
{
    if (repeats)
        *repeats = 0;
    while (!second_.empty() && now_ms - second_.front() >= 1000)
        second_.pop_front();
    if (per_second_ && second_.size() >= per_second_)
    {
        ++dropped_;
        return false;
    }
    if (bot && per_bot_type_ms_)
    {
        std::string const key = std::to_string(bot) + "/" + type;
        auto it = last_.find(key);
        if (it != last_.end() && now_ms - it->second.ms < per_bot_type_ms_)
        {
            ++it->second.repeats;   // the same problem again: told with the next event of this bot and type, not "dropped"
            ++total_repeats_;
            return false;
        }
        Last& l = last_[key];
        if (repeats)
            *repeats = l.repeats;
        l.ms = now_ms;
        l.repeats = 0;
        if (last_.size() > 20000)
            std::erase_if(last_, [&](auto const& kv) { return now_ms - kv.second.ms >= per_bot_type_ms_; });
    }
    second_.push_back(now_ms);
    return true;
}

std::uint32_t RepeatCounter::Hit(std::string const& key, std::uint64_t now_ms, std::uint64_t window_ms)
{
    auto& d = hits_[key];
    d.push_back(now_ms);
    while (!d.empty() && now_ms - d.front() > window_ms)
        d.pop_front();
    if (hits_.size() > 20000)
        std::erase_if(hits_, [&](auto const& kv) { return kv.second.empty() || now_ms - kv.second.back() > window_ms; });
    auto it = hits_.find(key);
    return it == hits_.end() ? 1u : std::uint32_t(it->second.size());
}

std::string CleanSayText(std::string const& raw, std::size_t max_bytes)
{
    std::string line;
    for (char c : raw)
    {
        if (c == '\n' || c == '\r')
        {
            if (!line.empty())
                break;
            continue;
        }
        if (c == '|' || (static_cast<unsigned char>(c) < 0x20 && c != '\t'))
            continue;
        line += c == '\t' ? ' ' : c;
    }
    auto const first = line.find_first_not_of(' ');
    if (first == std::string::npos)
        return {};
    line.erase(0, first);
    while (!line.empty() && line.back() == ' ')
        line.pop_back();
    if (line.size() <= max_bytes)
        return line;
    std::size_t cut = max_bytes;
    while (cut > 0 && (static_cast<unsigned char>(line[cut]) & 0xC0) == 0x80)   // not inside a UTF-8 character
        --cut;
    std::size_t const space = line.rfind(' ', cut);
    if (space != std::string::npos && space > max_bytes / 2)
        cut = space;
    line.resize(cut);
    while (!line.empty() && line.back() == ' ')
        line.pop_back();
    return line;
}

namespace {

struct ActWord { Act act; char const* word; };
constexpr ActWord kActs[] = {
    { Act::Follow, "follow" }, { Act::Stay, "stay" }, { Act::Resume, "resume" }, { Act::Attack, "attack" },
    { Act::StopAttack, "stop_attack" }, { Act::Flee, "flee" }, { Act::GoTo, "go_to" },
    { Act::AcceptQuest, "accept_quest" }, { Act::TurnIn, "turn_in" }, { Act::DropQuest, "drop_quest" },
    { Act::ShareQuest, "share_quest" }, { Act::Unstuck, "unstuck" }, { Act::Sit, "sit" }, { Act::Stand, "stand" },
    { Act::Mount, "mount" }, { Act::Dismount, "dismount" }, { Act::Emote, "emote" }, { Act::Cast, "cast" },
    { Act::Order, "order" }, { Act::Trade, "trade" }, { Act::Step, "step" },
    { Act::SignCharter, "sign_charter" },
};

struct EmoteWord { char const* word; std::uint32_t id; };
// EMOTE_ONESHOT_* and EMOTE_STATE_DANCE (SharedDefines.h Emote).
constexpr EmoteWord kEmotes[] = {
    { "wave", 3 }, { "hello", 3 }, { "bye", 3 }, { "bow", 2 }, { "thank", 2 }, { "cheer", 4 }, { "laugh", 11 },
    { "dance", 10 }, { "salute", 66 }, { "point", 25 }, { "yes", 273 }, { "nod", 273 }, { "no", 274 }, { "cry", 18 },
    { "roar", 15 }, { "flex", 23 }, { "kneel", 16 }, { "applaud", 21 }, { "shy", 24 }, { "talk", 1 }, { "question", 6 },
    { "exclamation", 5 }, { "rude", 14 }, { "chicken", 19 }, { "beg", 20 }, { "shout", 22 }, { "eat", 7 },
};

} // namespace

char const* ActName(Act a)
{
    for (ActWord const& w : kActs)
        if (w.act == a)
            return w.word;
    return "";
}

Act ParseAct(std::string const& word)
{
    std::string const w = Lower(word);
    for (ActWord const& a : kActs)
        if (w == a.word)
            return a.act;
    // A few spellings a model is likely to use.
    if (w == "goto" || w == "go" || w == "move_to" || w == "walk_to") return Act::GoTo;
    if (w == "stop" || w == "stopattack" || w == "disengage") return Act::StopAttack;
    if (w == "accept" || w == "acceptquest") return Act::AcceptQuest;
    if (w == "turnin" || w == "hand_in") return Act::TurnIn;
    if (w == "drop" || w == "abandon_quest" || w == "dropquest") return Act::DropQuest;
    if (w == "share" || w == "sharequest") return Act::ShareQuest;
    if (w == "hold" || w == "wait") return Act::Stay;
    if (w == "cast_spell" || w == "spell" || w == "castspell") return Act::Cast;
    if (w == "command") return Act::Order;
    if (w == "give" || w == "give_items" || w == "open_trade") return Act::Trade;
    if (w == "move" || w == "sidestep" || w == "step_aside" || w == "back_up" || w == "step_back") return Act::Step;
    if (w == "sign" || w == "sign_petition" || w == "signcharter" || w == "sign_guild_charter") return Act::SignCharter;
    return Act::None;
}

std::uint32_t EmoteId(std::string const& word)
{
    std::string const w = Lower(word);
    for (EmoteWord const& e : kEmotes)
        if (w == e.word)
            return e.id;
    return 0;
}

bool SafeOrder(std::string const& raw, std::string& norm, std::string& why)
{
    // Lower case, single spaces, no punctuation at the ends ("Mount up!" -> "mount up").
    norm.clear();
    for (char c : raw)
    {
        unsigned char const u = static_cast<unsigned char>(c);
        if (c == '\t' || c == '\n' || c == '\r')
            c = ' ';
        else if (u < 0x20 || c == '|')
            continue;
        if (c == ' ' && (norm.empty() || norm.back() == ' '))
            continue;
        norm += char(std::tolower(u));
    }
    while (!norm.empty() && (norm.back() == ' ' || norm.back() == '.' || norm.back() == '!' || norm.back() == '?'))
        norm.pop_back();
    while (!norm.empty() && (norm.front() == '.' || norm.front() == ';' || norm.front() == '/'))
        norm.erase(0, 1);
    if (norm.empty())
    {
        why = "empty";
        return false;
    }
    // The whole order. Group orders (GroupOrderText.cpp) and the owner's plain words (BotCommandParser OwnerWordOrder).
    static constexpr std::string_view kExact[] = {
        "follow", "stay", "hold", "resume", "come", "disengage", "flee", "attack", "assist", "mount", "dismount", "sit", "stand",
        "release", "passive", "defensive", "aggressive", "accept", "accept all", "turn in", "turnin", "share", "quests",
        "quest list", "loot", "loot all", "sell grey", "sell junk", "repair", "equip upgrades", "upgrades", "use your best gear",
        "wear your best gear", "gear", "inventory", "stats", "who", "buff", "heal", "jump", "wave", "salute", "dance",
        // The fight's group words: the tank's pull and pull back, falling back to (and setting) the rally point, the group's spacing,
        // and ike3's tactics.
        "pull", "pull back", "pullback", "fall back", "fallback", "back", "rally here", "spread", "spread out", "spaced out", "stack",
        "tight", "close", "max dps", "save mana", "normal",
    };
    for (std::string_view w : kExact)
        if (norm == w)
            return true;
    // Orders with words after them: an object to use, a quest to drop, an item to put on, something to make or conjure for the asker
    // ("craft water 20": PlayerCraftOrders, the same as his whisper).
    // "formation circle", "spacing tight", "distance 10" set how the group stands (BotCommandParser OrderFormation / OrderDistance).
    // "tactics lucifron: adds first, no aoe" keeps the player's plan for a raid boss (Raid/BossTactics.h).
    static constexpr std::string_view kWithArgs[] = { "use ", "drop quest ", "equip ", "craft ", "formation ", "spacing ", "distance ",
                                                      "follow distance ", "tactics " };
    for (std::string_view w : kWithArgs)
        if (norm.size() > w.size() && norm.compare(0, w.size(), w) == 0)
            return true;
    why = "\"" + norm.substr(0, 40) + "\" is not one of the orders a reply may give";
    return false;
}

PlaceKind ParsePlace(std::string const& place, std::string& rest)
{
    std::vector<std::string> words;
    std::string w;
    for (char c : place)
    {
        unsigned char const u = static_cast<unsigned char>(c);
        if (std::isalnum(u) || u >= 0x80 || c == '\'')
            w += char(std::tolower(u));
        else if (!w.empty())
        {
            words.push_back(std::move(w));
            w.clear();
        }
    }
    if (!w.empty())
        words.push_back(std::move(w));

    struct KindWord { std::string_view word; PlaceKind kind; };
    static constexpr KindWord kKinds[] = {
        { "inn", PlaceKind::Innkeeper }, { "innkeeper", PlaceKind::Innkeeper }, { "tavern", PlaceKind::Innkeeper },
        { "pub", PlaceKind::Innkeeper }, { "mailbox", PlaceKind::Mailbox }, { "mail", PlaceKind::Mailbox },
        { "flightmaster", PlaceKind::FlightMaster }, { "flight", PlaceKind::FlightMaster }, { "gryphon", PlaceKind::FlightMaster },
        { "griffon", PlaceKind::FlightMaster }, { "wyvern", PlaceKind::FlightMaster }, { "hippogryph", PlaceKind::FlightMaster },
        { "taxi", PlaceKind::FlightMaster }, { "vendor", PlaceKind::Vendor }, { "merchant", PlaceKind::Vendor },
        { "shop", PlaceKind::Vendor }, { "repair", PlaceKind::Repair }, { "blacksmith", PlaceKind::Repair },
        { "bank", PlaceKind::Banker }, { "banker", PlaceKind::Banker }, { "trainer", PlaceKind::Trainer },
        { "stable", PlaceKind::StableMaster }, { "stables", PlaceKind::StableMaster }, { "stablemaster", PlaceKind::StableMaster },
        { "auction", PlaceKind::Auctioneer }, { "auctioneer", PlaceKind::Auctioneer }, { "ah", PlaceKind::Auctioneer },
        { "graveyard", PlaceKind::SpiritHealer }, { "spirit", PlaceKind::SpiritHealer },
    };
    // Small words, and the words that only finish a kind ("flight master", "auction house", "spirit healer").
    static constexpr std::string_view kSkip[] = {
        "the", "a", "an", "at", "in", "to", "of", "on", "near", "nearest", "closest", "by", "over", "there", "here", "go", "walk",
        "run", "travel", "head", "please", "pls", "master", "house", "healer", "keeper", "box", "point", "place",
    };
    PlaceKind kind = PlaceKind::Name;
    rest.clear();
    for (std::string const& word : words)
    {
        bool used = false;
        if (kind == PlaceKind::Name)
            for (KindWord const& k : kKinds)
                if (word == k.word)
                {
                    kind = k.kind;
                    used = true;
                    break;
                }
        if (used || std::find(std::begin(kSkip), std::end(kSkip), word) != std::end(kSkip))
            continue;
        if (!rest.empty())
            rest += ' ';
        rest += word;
    }
    return kind;
}

std::string ActivityFromRule(std::string_view rule)
{
    std::string r(rule);
    for (char& c : r)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    auto has = [&r](std::initializer_list<char const*> words)
    {
        for (char const* w : words)
            if (r.find(w) != std::string::npos)
                return true;
        return false;
    };
    if (r.empty())
        return {};
    // first the ones whose words would also match a broader group below
    if (r == "idle:gathering")
        return "hanging out with some people, chatting";
    if (has({ "craft_order" }))
        return "making something someone ordered";
    if (has({ "dungeon", "boss" }))
        return "in a dungeon run with my group";
    if (has({ "bags_full", "vendor", "sell" }))
        return "on the way to a vendor to sell junk";
    if (has({ "repair" }))
        return "getting my gear repaired";
    if (has({ "ah_", "auction" }))
        return "doing some business at the auction house";
    if (has({ "mail" }))
        return "checking my mail";
    if (has({ "bank" }))
        return "at the bank";
    if (has({ "inn_", "_inn", "innkeeper" }))
        return "taking a break at the inn";
    if (has({ "trainer" }))
        return "visiting my trainer";
    if (has({ "flight", "taxi" }))
        return "catching a flight";
    if (has({ "transport", "dock", "boat", "zeppelin", "portal", "elevator" }))
        return "travelling";
    if (has({ "turnin", "quest_ender" }))
        return "handing in a quest";
    if (has({ "quest", "wq_" }))
        return "doing a quest";
    if (has({ "grind", "roam" }))
        return "killing mobs for xp";
    if (has({ "herb", "mining", "mine_", "skin", "gather_node", "fish" }))
        return "gathering stuff";
    if (has({ "pvp", "duel" }))
        return "up for some pvp";
    if (has({ "lfg", "yell_lfg", "group_find" }))
        return "looking for a group";
    if (has({ "party_follow", "follow" }))
        return "following my group";
    if (has({ "_rest", "rest_", "drink", "res_sick" }))   // not "eat": "pull_back_retreat"
        return "resting a bit";
    if (has({ "town_errand", "town_linger", "wander", "rpg", "service", "watch_walk" }))
        return "running errands in town";
    if (has({ "travel", "walk_to", "hub" }))
        return "on my way somewhere";
    if (r.rfind("combat:", 0) == 0)
        return "fighting";
    return {};
}

bool IsGroupAct(Act a)
{
    switch (a)
    {
        case Act::Follow: case Act::Stay: case Act::Resume: case Act::Attack: case Act::StopAttack: case Act::Flee: case Act::GoTo:
        case Act::Sit: case Act::Stand: case Act::Mount: case Act::Dismount: case Act::Emote: case Act::Order: case Act::Step:
        case Act::SignCharter:
            return true;
        default:
            return false;
    }
}

std::string LineAfterAction(Act a, bool ok, bool speaks_later, std::string const& why, std::string const& said)
{
    if (ok)
        return speaks_later ? std::string() : said;
    if (a == Act::Emote || a == Act::Sit || a == Act::Stand)
        return said;
    if (why.empty() || why == "the player who asked is gone")
        return {};
    if (why.find("turned off on this server") != std::string::npos)
        return "Sorry, I can't do that here.";
    // Reasons written for the program, not for a player: config keys, refs, rate limits, a moment that passed.
    static constexpr std::string_view kForProgram[] = { "Playerbot.", "ref", "chat_in", "\"", "too old", "no such action",
        "too many", "too soon", "every ", "not ready", "not online", "no action", ": ", "(" };
    for (std::string_view w : kForProgram)
        if (why.find(w) != std::string::npos)
            return "Sorry, I can't do that right now.";
    std::string out = "Sorry, " + why;
    if (out.back() != '.' && out.back() != '!' && out.back() != '?')
        out += '.';
    return out;
}

char const* CheckActionContext(Act a, ActionContext const& ctx)
{
    if (a == Act::None || a == Act::Count)
        return "no action";
    bool const harmless = a == Act::Emote || a == Act::Sit || a == Act::Stand;
    if (ctx.owned)
    {
        if (ctx.has_speaker && !ctx.speaker_is_owner)
            return "only my owner can tell me to do that";
        if (!ctx.has_speaker && !ctx.owned_bots_allowed)
            return "this bot belongs to a player: it only acts when its owner asks";
    }
    else if (ctx.grouped_with_human && !harmless && !(ctx.has_speaker && ctx.speaker_controls))
        return "only the player leading my group can tell me that";
    // An order and a trade are the bot's owner's (or his group leader's) to ask for, never a stranger's or the program's own idea.
    // So is a step in a dungeon or a fight: "get out of the fire" is the leader's call there.
    bool const step_in_fight = a == Act::Step && (ctx.in_instance || ctx.in_combat);
    if ((a == Act::Order || a == Act::Trade || step_in_fight) && !(ctx.has_speaker && (ctx.speaker_is_owner || ctx.speaker_controls)))
        return "only the player who owns me or leads my group can ask me that";
    // An order is what the player could type himself: the parser's own rules decide in a dungeon and in a fight.
    if (ctx.in_instance && !(harmless || a == Act::Follow || a == Act::Stay || a == Act::Resume || a == Act::Order || a == Act::Trade ||
                             a == Act::Step))
        return "not in a dungeon, raid or battleground (the bots' own logic leads there)";
    if (ctx.in_combat && !(a == Act::Attack || a == Act::StopAttack || a == Act::Flee || a == Act::Follow || a == Act::Stay ||
                           a == Act::Cast || a == Act::Order || a == Act::Step))
        return "not while fighting";
    return nullptr;
}

std::string AllowedActsJson(ActionContext const& ctx, std::vector<bool> const& allowed)
{
    std::string out = "[";
    for (ActWord const& w : kActs)
    {
        if (std::size_t(w.act) >= allowed.size() || !allowed[std::size_t(w.act)] || CheckActionContext(w.act, ctx))
            continue;
        if (out.size() > 1)
            out += ',';
        out += '"';
        out += w.word;
        out += '"';
    }
    return out + "]";
}

std::vector<bool> ParseAllowedActions(std::string const& list)
{
    std::vector<bool> allowed(std::size_t(Act::Count), false);
    std::string word;
    auto take = [&]()
    {
        if (word == "all")
            std::fill(allowed.begin() + 1, allowed.end(), true);
        else if (Act const a = ParseAct(word); a != Act::None)
            allowed[std::size_t(a)] = true;
        word.clear();
    };
    for (char c : list)
    {
        if (c == ',' || c == ' ' || c == ';' || c == '\t')
            take();
        else
            word += char(std::tolower(static_cast<unsigned char>(c)));
    }
    take();
    return allowed;
}

char const* ActionGate::Allow(std::uint64_t bot, Act a, std::uint64_t now_ms)
{
    PerBot& b = bots_[bot];
    if (b.paused_until_ms && now_ms < b.paused_until_ms)
        return "too many refused actions: this bot takes none for a few minutes";
    while (!second_.empty() && now_ms - second_.front() >= 1000)
        second_.pop_front();
    if (per_second_ && second_.size() >= per_second_)
        return "too many actions on the server this second";
    if (b.last_ms && now_ms - b.last_ms < bot_gap_ms_)
        return "too soon after this bot's last action";
    if (a == Act::Emote && b.last_emote_ms && now_ms - b.last_emote_ms < 10000)
        return "one emote every 10 seconds";
    if (a == Act::Unstuck && b.last_unstuck_ms && now_ms - b.last_unstuck_ms < 120000)
        return "one unstuck every 2 minutes";
    b.last_ms = now_ms ? now_ms : 1;
    if (a == Act::Emote)
        b.last_emote_ms = b.last_ms;
    if (a == Act::Unstuck)
        b.last_unstuck_ms = b.last_ms;
    second_.push_back(now_ms);
    if (bots_.size() > 20000)
        std::erase_if(bots_, [&](auto const& kv) { return now_ms - kv.second.last_ms > 600000 && now_ms >= kv.second.paused_until_ms; });
    return nullptr;
}

void ActionGate::NoteRefused(std::uint64_t bot, std::uint64_t now_ms)
{
    PerBot& b = bots_[bot];
    b.refused.push_back(now_ms);
    while (!b.refused.empty() && now_ms - b.refused.front() > 60000)
        b.refused.pop_front();
    if (b.refused.size() >= 10)
    {
        b.paused_until_ms = now_ms + 300000;
        b.refused.clear();
    }
}

namespace {

std::string Num1(float v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.0f", double(v));
    return buf;
}

} // namespace

std::string BuildNearJson(std::vector<NearItem> const& items)
{
    if (items.empty())
        return {};
    std::string out = "[";
    for (NearItem const& n : items)
    {
        if (out.size() > 1)
            out += ',';
        out += "{\"ref\":" + std::to_string(n.ref);
        AddString(out, "kind", n.kind);
        AddString(out, "name", n.name);
        if (n.level)
            out += ",\"level\":" + std::to_string(n.level);
        out += ",\"dist\":" + Num1(n.dist);
        if (n.count > 1)
            out += ",\"count\":" + std::to_string(n.count);
        if (!n.quests.empty())
        {
            out += ",\"quests\":[";
            bool first = true;
            for (auto const& [id, title] : n.quests)
            {
                if (!first)
                    out += ',';
                first = false;
                out += "{\"id\":" + std::to_string(id);
                AddString(out, "name", title);
                out += '}';
            }
            out += ']';
        }
        out += '}';
    }
    out += ']';
    return out;
}

std::string BuildSpellsJson(std::vector<SpellItem> const& spells)
{
    std::string out = "[";
    for (SpellItem const& sp : spells)
    {
        if (out.size() > 1)
            out += ',';
        out += '{';
        out += "\"name\":\"" + JsonEscape(sp.name) + "\"";
        AddString(out, "kind", sp.kind);
        out += '}';
    }
    return out + "]";
}

std::string SpellNameKey(std::string const& name)
{
    std::string key;
    for (unsigned char c : name)
    {
        if (std::isalnum(c) || c >= 0x80)
            key += char(c < 0x80 ? std::tolower(c) : c);
        else if (c == '(')
            break;   // "(Rank 2)" and anything after it
        else if (!key.empty() && key.back() != ' ')
            key += ' ';
    }
    while (!key.empty() && key.back() == ' ')
        key.pop_back();
    // "... rank 3" without brackets
    if (auto const pos = key.rfind(" rank "); pos != std::string::npos &&
        key.find_first_not_of("0123456789", pos + 6) == std::string::npos && pos + 6 < key.size())
        key.erase(pos);
    return key;
}

namespace {

std::tm Utc(std::int64_t utc_seconds)
{
    std::time_t const t = std::time_t(utc_seconds);
    std::tm out{};
#ifdef _WIN32
    gmtime_s(&out, &t);
#else
    gmtime_r(&t, &out);
#endif
    return out;
}

} // namespace

std::string EventsFileName(std::int64_t utc_seconds)
{
    std::tm const t = Utc(utc_seconds);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "events-%04d%02d%02d-%02d00.jsonl", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour);
    return buf;
}

std::string IsoTime(std::int64_t utc_seconds)
{
    std::tm const t = Utc(utc_seconds);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    return buf;
}

} // namespace Playerbot::Bridge
