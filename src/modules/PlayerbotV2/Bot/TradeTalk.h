// TradeTalk - what a player whispers a bot about buying from it: "what do you have", then "buy 2" or "buy linen cloth".
// Pure text logic on a normalized line (lower case, punctuation folded to spaces). Tested in tests/game/ClassicBotTrade.cpp.

#pragma once

#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace Playerbot::V2::TradeTalk {

inline bool Has(std::string const& hay, char const* needle)
{
    std::string const pad_h = " " + hay + " ";
    std::string const pad_n = std::string(" ") + needle + " ";
    return pad_h.find(pad_n) != std::string::npos;
}

// Russian letters in lower case, and ё as е, so "Что продаёшь" matches "что продаешь" (the line is UTF-8; only ASCII is lowered
// before it gets here).
inline std::string FoldRussian(std::string const& in)
{
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i)
    {
        unsigned char const c = static_cast<unsigned char>(in[i]);
        unsigned char const n = i + 1 < in.size() ? static_cast<unsigned char>(in[i + 1]) : 0;
        if (c == 0xD0 && n >= 0x90 && n <= 0x9F)        { out += char(0xD0); out += char(n + 0x20); ++i; }   // А-П -> а-п
        else if (c == 0xD0 && n >= 0xA0 && n <= 0xAF)   { out += char(0xD1); out += char(n - 0x20); ++i; }   // Р-Я -> р-я
        else if ((c == 0xD0 && n == 0x81) || (c == 0xD1 && n == 0x91)) { out += char(0xD0); out += char(0xB5); ++i; }   // Ё ё -> е
        else out += char(c);
    }
    return out;
}

// "what do you have", "what are you selling", "show me your wares", "trade list", "anything for sale", and the same in Russian.
inline bool IsInventoryQuestion(std::string const& line)
{
    std::string const norm = FoldRussian(line);
    static constexpr char const* kPhrases[] = {
        "what do you have", "what have you got", "what you got", "what do you got", "what ya got", "what do you sell", "what are you selling",
        "what you selling", "what can i buy", "anything for sale", "got anything for sale", "anything to sell", "what is for sale",
        "whats for sale", "what's for sale", "show me your wares", "your wares", "trade list", "your goods", "what are you selling",
        "what is in your bags", "whats in your bags", "what do you carry", "show me what you have", "list your items", "for sale",
        "что у тебя есть", "что продаешь", "что ты продаешь", "что продаете", "что вы продаете", "что продается",
        "на продажу", "что можно купить", "что купить", "покажи товар", "покажи товары", "список товаров", "что в сумках",
    };
    for (char const* p : kPhrases)
        if (Has(norm, p)) return true;
    return false;
}

struct Order
{
    bool is_order = false;
    bool buy_verb = false;   // said "buy" / "purchase": a line that is about trading whatever follows
    int index = -1;          // 1-based position on the last list; -1 when named instead
    std::string name;        // item name part, lower case; empty when numbered
};

// "buy 2", "buy #2", "i'll take 2", "i want 2", "buy linen cloth", "ill take the linen cloth".
inline Order ParseOrder(std::string const& line)
{
    std::string const norm = FoldRussian(line);
    Order o;
    static constexpr char const* kVerbs[] = { "buy", "take", "want", "purchase", "куплю", "купить", "покупаю", "беру", "возьму" };
    size_t pos = std::string::npos;
    size_t vlen = 0;
    for (char const* v : kVerbs)
    {
        std::string const needle = std::string(v) + " ";
        size_t p = std::string::npos;
        if (norm.compare(0, needle.size(), needle) == 0) p = 0;
        else if (size_t q = norm.find(" " + needle); q != std::string::npos) p = q + 1;
        if (p != std::string::npos && (pos == std::string::npos || p < pos)) { pos = p; vlen = needle.size(); }
    }
    if (pos == std::string::npos) return o;
    o.buy_verb = norm.compare(pos, 3, "buy") == 0 || norm.compare(pos, 8, "purchase") == 0 ||
                 norm.compare(pos, std::string("куп").size(), "куп") == 0;
    std::string rest = norm.substr(pos + vlen);
    for (char const* filler : { "the ", "a ", "an ", "number ", "no ", "item ", "that ", "me ", "номер ", "\xE2\x84\x96 " })
    {
        std::string f(filler);
        if (rest.compare(0, f.size(), f) == 0) rest.erase(0, f.size());
    }
    while (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
    while (!rest.empty() && rest.back() == ' ') rest.pop_back();
    if (rest.empty()) return o;
    bool digits = true;
    for (char c : rest) if (!std::isdigit(static_cast<unsigned char>(c))) { digits = false; break; }
    if (digits && rest.size() <= 2)
    {
        o.is_order = true;
        o.index = std::stoi(rest);
        return o;
    }
    // "# 2" folded to " 2" arrives as "2"; anything else is a name
    o.is_order = true;
    o.name = rest;
    return o;
}

struct ListedItem
{
    std::string name;   // lower case
};

// Position (0-based) of the order on the list, -1 when it names nothing on it. A name matches by containing text, the shortest
// name wins a tie so "linen cloth" does not pick "heavy linen cloth bolt".
inline int ResolveOrder(Order const& o, std::vector<ListedItem> const& list)
{
    if (!o.is_order) return -1;
    if (o.index >= 1 && static_cast<size_t>(o.index) <= list.size()) return o.index - 1;
    if (o.name.empty()) return -1;
    int best = -1;
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].name.find(o.name) != std::string::npos && (best < 0 || list[i].name.size() < list[static_cast<size_t>(best)].name.size()))
            best = static_cast<int>(i);
    return best;
}

} // namespace Playerbot::V2::TradeTalk
