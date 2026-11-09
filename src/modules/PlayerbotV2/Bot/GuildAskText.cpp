// GuildAskText - see GuildAskText.h.

#include "GuildAskText.h"

#include <initializer_list>
#include <vector>

namespace Playerbot::V2::GuildAsk {

namespace {

std::vector<std::string> Words(std::string const& norm)
{
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos < norm.size())
    {
        size_t const sp = norm.find(' ', pos);
        size_t const end = sp == std::string::npos ? norm.size() : sp;
        if (end > pos)
            out.emplace_back(norm.substr(pos, end - pos));
        pos = end + 1;
    }
    return out;
}

bool StartsWith(std::string const& w, char const* prefix)
{
    std::string_view const p(prefix);
    return w.size() >= p.size() && std::string_view(w).substr(0, p.size()) == p;
}

bool AnyExact(std::vector<std::string> const& words, std::initializer_list<char const*> list)
{
    for (std::string const& w : words)
        for (char const* l : list)
            if (w == l)
                return true;
    return false;
}

bool AnyPrefix(std::vector<std::string> const& words, std::initializer_list<char const*> list)
{
    for (std::string const& w : words)
        for (char const* l : list)
            if (StartsWith(w, l))
                return true;
    return false;
}

bool HasPhrase(std::string const& norm, char const* phrase)
{
    std::string const padded = " " + norm + " ";
    return padded.find(std::string(" ") + phrase + " ") != std::string::npos;
}

// One-word guild invite asks.
bool IsGuildInviteWord(std::vector<std::string> const& words)
{
    return AnyExact(words, { "ginv", "ginvite", "guildinvite", "гинв", "гинвайт" });
}

bool HasGuildWord(std::vector<std::string> const& words)
{
    // guild, guilds, guildie, guildy; gild (a common typo); гильдия / гильдию / гильдии, гилд / гилда / гилду, клан / клана;
    // "ги" is how Russian players write guild in chat ("в ги").
    return AnyPrefix(words, { "guild", "gild", "гильд", "гилд", "клан" }) || AnyExact(words, { "ги" });
}

bool HasAskWord(std::vector<std::string> const& words, std::string const& norm)
{
    if (AnyPrefix(words, { "invit", "recruit", "join", "accept", "apply" }))
        return true;
    if (AnyExact(words, { "inv", "add", "take", "want", "wanna", "pls", "plz", "please", "room", "spot", "space", "looking", "lf",
                          "need", "membership", "sign", "get" }))
        return true;
    if (HasPhrase(norm, "can i") || HasPhrase(norm, "could i") || HasPhrase(norm, "may i") || HasPhrase(norm, "let me") ||
        HasPhrase(norm, "can you inv") || HasPhrase(norm, "can u inv"))
        return true;
    // пригласи / пригласите / приглашение, инвайт / инв, вступить / вступлю, прими / принять / примите, возьми / возьмите, взять,
    // добавь / добавьте, кинь / кинуть, пусти / впусти, ищу; можно, хочу, пожалуйста, плиз, пж, пжл.
    if (AnyPrefix(words, { "пригла", "инв", "вступ", "прими", "приня", "возьм", "взять", "добав", "кинь", "кину", "пусти", "впусти",
                           "ищу", "пожалуйст" }))
        return true;
    return AnyExact(words, { "можно", "хочу", "плиз", "пж", "пжл", "пжлст" });
}

// About the sender's own guild, or a no.
bool IsExcluded(std::vector<std::string> const& words, std::string const& norm)
{
    if (AnyExact(words, { "my", "our", "mine", "мою", "моя", "моей", "мой", "нашу", "наша", "нашей", "наш", "свою", "своя" }))
        return true;
    if (AnyExact(words, { "dont", "not", "never", "nah", "нет", "неа" }))
        return true;
    return HasPhrase(norm, "no thanks") || HasPhrase(norm, "no thx") || HasPhrase(norm, "не хочу") || HasPhrase(norm, "не надо") ||
           HasPhrase(norm, "не нужно") || HasPhrase(norm, "не буду");
}

} // namespace

std::string Normalize(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size() < 256 ? raw.size() : 256);
    bool last_space = true;
    for (size_t i = 0; i < raw.size() && out.size() < 256; ++i)
    {
        unsigned char const c = static_cast<unsigned char>(raw[i]);
        // Russian capitals in UTF-8: А..П = D0 90..9F -> D0 B0..BF; Р..Я = D0 A0..AF -> D1 80..8F; Ё = D0 81 -> D1 91.
        if (c == 0xD0 && i + 1 < raw.size())
        {
            unsigned char const n = static_cast<unsigned char>(raw[i + 1]);
            if (n >= 0x90 && n <= 0x9F) { out.push_back(char(0xD0)); out.push_back(char(n + 0x20)); }
            else if (n >= 0xA0 && n <= 0xAF) { out.push_back(char(0xD1)); out.push_back(char(n - 0x20)); }
            else if (n == 0x81) { out.push_back(char(0xD1)); out.push_back(char(0x91)); }
            else { out.push_back(char(c)); out.push_back(char(n)); }
            ++i;
            last_space = false;
            continue;
        }
        if (c == '\'' || c == '`')
            continue;
        if (c < 0x80 && !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
        {
            if (!last_space)
            {
                out.push_back(' ');
                last_space = true;
            }
            continue;
        }
        out.push_back(c >= 'A' && c <= 'Z' ? char(c + 32) : char(c));
        last_space = false;
    }
    while (!out.empty() && out.back() == ' ')
        out.pop_back();
    // ё reads as е, so "ещё" and "еще" are the same word.
    std::string folded;
    folded.reserve(out.size());
    for (size_t i = 0; i < out.size(); ++i)
    {
        if (static_cast<unsigned char>(out[i]) == 0xD1 && i + 1 < out.size() && static_cast<unsigned char>(out[i + 1]) == 0x91)
        {
            folded.push_back(char(0xD0));
            folded.push_back(char(0xB5));
            ++i;
            continue;
        }
        folded.push_back(out[i]);
    }
    return folded;
}

bool IsJoinRequest(std::string const& norm)
{
    if (norm.empty())
        return false;
    std::vector<std::string> const words = Words(norm);
    if (IsExcluded(words, norm))
        return false;
    if (IsGuildInviteWord(words))
        return true;
    return HasGuildWord(words) && HasAskWord(words, norm);
}

bool IsShortYes(std::string const& norm)
{
    if (norm.empty())
        return false;
    std::vector<std::string> const words = Words(norm);
    if (words.size() > 6 || IsExcluded(words, norm))
        return false;
    if (IsGuildInviteWord(words) || HasAskWord(words, norm))
        return true;
    return AnyExact(words, { "yes", "yeah", "yea", "yep", "yup", "ya", "y", "sure", "ok", "okay", "k", "pls", "plz", "please", "inv",
                             "invite", "go", "da",
                             "да", "ага", "угу", "давай", "давайте", "конечно", "хочу", "го", "инв", "инвайт", "плиз", "пж",
                             "пожалуйста", "можно" });
}

} // namespace Playerbot::V2::GuildAsk
