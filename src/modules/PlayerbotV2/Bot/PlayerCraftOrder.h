// PlayerCraftOrder - the pure part of "craft <item link or name> [x5]": a player in a bot's group asks that bot to make something with
// the recipes and reagents it has, and gets the result by mail. Parsing, recipe matching and the cast plan live here (no engine types),
// tested in tests/game/ClassicPlayerCraftOrder.cpp. The game side (recipe scan, casts, mail) is Fleet/PlayerCraftOrders.cpp.
//
// The request grammar, the matching order (link, exact name, prefix, substring) and the cap on a plan follow
// Trixx34/ForeverBots (src/server/game/Bots/BotCraftOrder.h, GPL-2.0), re-implemented for this engine.

#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace Playerbot::V2::PlayerCraft {

enum class Kind : std::uint8_t { None, List, Craft };

struct Request
{
    Kind type = Kind::None;
    std::uint32_t count = 1;       // items wanted (not casts), 1..max_count
    std::uint32_t item_entry = 0;  // from an item link, else 0
    std::string name;              // lower case free text, empty for a link or a list
};

namespace detail {

// "x5", "5x" or "5": the count, 0 when the word is not one.
inline std::uint32_t CountWord(std::string_view w)
{
    if (w.size() > 1 && (w.front() == 'x' || w.back() == 'x'))
        w = w.front() == 'x' ? w.substr(1) : w.substr(0, w.size() - 1);
    if (w.empty() || w.size() > 5)
        return 0;
    std::uint32_t n = 0;
    for (char c : w)
    {
        if (c < '0' || c > '9')
            return 0;
        n = n * 10u + std::uint32_t(c - '0');
    }
    return n;
}

inline std::string Squeeze(std::string_view s)
{
    std::string out;
    bool gap = false;
    for (char c : s)
    {
        if (c == ' ' || c == '\t')
        {
            gap = !out.empty();
            continue;
        }
        if (gap)
            out.push_back(' ');
        gap = false;
        out.push_back(c);
    }
    return out;
}

} // namespace detail

// `text` is the whole order, starting with the word "craft": "craft list" | "craft |hitem:2996::::|h[Bolt of Linen Cloth]|h x5" |
// "craft linen bandage 5". Case-insensitive. The count is the first or the last word ("x5", "5x" or "5"), clamped to 1..max_count.
// Kind::None when the first word is not "craft" or nothing was asked for.
inline Request ParseRequest(std::string_view text, std::uint32_t max_count)
{
    Request r;
    std::string s;
    s.reserve(text.size());
    for (char c : text)
        s.push_back(c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c);
    s = detail::Squeeze(s);
    if (s.rfind("craft", 0) != 0 || (s.size() > 5 && s[5] != ' '))
        return r;
    s = s.size() > 5 ? s.substr(6) : std::string();
    if (s.empty())
        return r;
    if (s == "list")
    {
        r.type = Kind::List;
        return r;
    }

    // an item link has spaces in its name: take it out first. Shape: [|cffRRGGBB]|hitem:<entry>:...|h[Name]|h[|r]
    if (auto h = s.find("|hitem:"); h != std::string::npos)
    {
        std::size_t i = h + 7;
        std::uint32_t entry = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9' && entry < 100000000u)
            entry = entry * 10u + std::uint32_t(s[i++] - '0');
        std::size_t from = h;
        if (h >= 10 && s.compare(h - 10, 2, "|c") == 0)
            from = h - 10;
        std::size_t to = s.size();
        if (auto open = s.find("|h", h + 7); open != std::string::npos)         // end of the link target: |h[Name]
            if (auto close = s.find("|h", open + 2); close != std::string::npos)  // end of the visible text
            {
                to = close + 2;
                if (s.compare(to, 2, "|r") == 0)
                    to += 2;
            }
        std::string const rest = detail::Squeeze(s.substr(0, from) + " " + s.substr(to));
        if (entry == 0)
            return r;
        r.type = Kind::Craft;
        r.item_entry = entry;
        std::uint32_t const n = detail::CountWord(rest);
        r.count = std::clamp<std::uint32_t>(n ? n : 1u, 1u, std::max<std::uint32_t>(max_count, 1u));
        return r;
    }

    // words: optional count first or last, the rest is the name
    std::string first, last;
    auto const sp = s.find(' ');
    first = s.substr(0, sp);
    auto const rsp = s.rfind(' ');
    last = rsp == std::string::npos ? s : s.substr(rsp + 1);
    std::string name = s;
    std::uint32_t n = 0;
    if (sp != std::string::npos)
    {
        if ((n = detail::CountWord(first)) != 0)
            name = s.substr(sp + 1);
        else if ((n = detail::CountWord(last)) != 0)
            name = s.substr(0, rsp);
    }
    name = detail::Squeeze(name);
    if (name.empty())
        return r;
    r.type = Kind::Craft;
    r.name = name;
    r.count = std::clamp<std::uint32_t>(n ? n : 1u, 1u, std::max<std::uint32_t>(max_count, 1u));
    return r;
}

enum class Conjured : std::uint8_t { None, Water, Food };

struct Option
{
    std::uint32_t spell = 0;
    std::uint32_t item = 0;
    std::uint32_t yield = 1;   // items one cast makes
    std::string name;          // lower case item name
    std::uint32_t rank = 0;    // how good the item is (required level, then item level): the better one wins a name that fits several ranks
    std::string spell_name;    // lower case recipe name ("conjure food"), tried only when no item name fits
    Conjured conjured = Conjured::None;   // a Conjure Water / Conjure Food rank, from the spell data (no names, so any locale)
};

namespace detail {

// "water" / "food" in English, and the Russian words, whatever the server's locale: the item and recipe names are in the server's
// language, so on a ruRU server "craft water" fitted nothing (Tornn, 2026-10-27). The add-on's Conjure buttons send the English words.
// The Russian words are UTF-8 bytes (this file is Latin-1): voda/vodu, napitok/napitki; eda/edu, pishcha/pishchu, lower and capitalised.
inline Conjured Keyword(std::string_view name)
{
    static constexpr std::string_view kWater[] = {
        "water", "drink", "drinks", "\xd0\xb2\xd0\xbe\xd0\xb4\xd0\xb0", "\xd0\xb2\xd0\xbe\xd0\xb4\xd1\x83", "\xd0\x92\xd0\xbe\xd0\xb4\xd0\xb0",
        "\xd0\x92\xd0\xbe\xd0\xb4\xd1\x83", "\xd0\xbd\xd0\xb0\xd0\xbf\xd0\xb8\xd1\x82\xd0\xbe\xd0\xba",
        "\xd0\xbd\xd0\xb0\xd0\xbf\xd0\xb8\xd1\x82\xd0\xba\xd0\xb8", "\xd0\x9d\xd0\xb0\xd0\xbf\xd0\xb8\xd1\x82\xd0\xbe\xd0\xba",
        "\xd0\x9d\xd0\xb0\xd0\xbf\xd0\xb8\xd1\x82\xd0\xba\xd0\xb8",
    };
    static constexpr std::string_view kFood[] = {
        "food", "\xd0\xb5\xd0\xb4\xd0\xb0", "\xd0\xb5\xd0\xb4\xd1\x83", "\xd0\x95\xd0\xb4\xd0\xb0", "\xd0\x95\xd0\xb4\xd1\x83",
        "\xd0\xbf\xd0\xb8\xd1\x89\xd0\xb0", "\xd0\xbf\xd0\xb8\xd1\x89\xd1\x83", "\xd0\x9f\xd0\xb8\xd1\x89\xd0\xb0",
        "\xd0\x9f\xd0\xb8\xd1\x89\xd1\x83",
    };
    for (std::string_view w : kWater)
        if (name == w)
            return Conjured::Water;
    for (std::string_view w : kFood)
        if (name == w)
            return Conjured::Food;
    return Conjured::None;
}

// `field` null: match by `want` (the conjured kind) instead of a name.
inline int MatchBy(Request const& request, std::span<Option const> options, std::string Option::*field, std::uint32_t user_level,
    Conjured want = Conjured::None)
{
    auto const usable = [user_level](Option const& o) { return user_level == 0 || o.rank / 1000u <= user_level; };
    std::string const empty;
    auto const text = [&](Option const& o) -> std::string const& { return field ? o.*field : empty; };
    int best = -1;
    int best_rank = 4;
    for (std::size_t i = 0; i < options.size(); ++i)
    {
        Option const& o = options[i];
        int rank = 4;
        if (request.item_entry)
            rank = o.item == request.item_entry ? 0 : 4;
        else if (!field)
            rank = o.conjured == want ? 1 : 4;
        else if (!request.name.empty())
        {
            if (text(o) == request.name) rank = 1;
            else if (text(o).rfind(request.name, 0) == 0) rank = 2;
            else if (text(o).find(request.name) != std::string::npos) rank = 3;
        }
        if (rank == 4)
            continue;
        bool better = best < 0 || rank < best_rank;
        if (!better && rank == best_rank)
        {
            Option const& b = options[std::size_t(best)];
            if (usable(o) != usable(b))
                better = usable(o);
            else if (o.rank != b.rank && (rank != 1 || field != &Option::name))   // every rank shares its recipe name and its kind
                better = o.rank > b.rank;
            else
                better = text(o).size() < text(b).size() || (text(o).size() == text(b).size() && o.spell < b.spell);
        }
        if (better)
        {
            best = int(i);
            best_rank = rank;
        }
    }
    return best;
}
} // namespace detail

// Index of the recipe that answers the request, -1 when none. A link matches by item entry; a name matches exactly, then as a prefix,
// then as a substring (ties: the better rank, then shorter name, then lower spell id). "craft water" fits every Conjure Water rank and the
// mage gave the first one; an exact name still gets exactly that item. When no item name fits, the recipe names are tried the same way:
// "craft food" names no conjured item (Conjured Muffin .. Conjured Cinnamon Roll) but fits every Conjure Food rank.
// `user_level` is the requester's level (0 = any): among the items a name fits, one he is high enough to use beats a better one he
// is not, so a level 30 player asking a level 60 mage for water gets water he can drink. "water" and "food" (and the Russian words) go
// first to the mage's Conjure Water / Conjure Food ranks by the spell data, so they work whatever language the server's names are in.
inline int Match(Request const& request, std::span<Option const> options, std::uint32_t user_level = 0)
{
    if (!request.item_entry)
        if (Conjured const want = detail::Keyword(request.name); want != Conjured::None)
            if (int const kind = detail::MatchBy(request, options, nullptr, user_level, want); kind >= 0)
                return kind;
    int const best = detail::MatchBy(request, options, &Option::name, user_level);
    if (best >= 0 || request.item_entry || request.name.empty())
        return best;
    return detail::MatchBy(request, options, &Option::spell_name, user_level);
}

struct Facts
{
    std::uint32_t wanted = 0;          // items asked for
    std::uint32_t yield = 1;           // items per cast
    std::uint32_t max_items = 0;       // config cap per order
    std::uint32_t casts_possible = 0;  // casts the bot's reagents allow right now
    std::uint32_t carry_room = 0xFFFFFFFFu;  // more of the item the bot may hold: a unique item (max count 1) it already carries leaves 0
};

struct Plan
{
    std::uint32_t casts = 0;
    std::uint32_t items = 0;           // casts * yield
    char const* refusal = nullptr;     // "no_reagents" | "too_many" | "already_carried" when nothing can be made
    bool trimmed = false;              // fewer than wanted
};

// Casts needed for the wanted items (rounded up), limited by the reagents and by the per order cap.
inline Plan PlanOrder(Facts const& f)
{
    Plan p;
    std::uint32_t const yield = std::max<std::uint32_t>(f.yield, 1u);
    if (f.max_items < yield)
    {
        p.refusal = "too_many";
        return p;
    }
    if (f.carry_room < yield)
    {
        p.refusal = "already_carried";
        return p;
    }
    if (f.casts_possible == 0)
    {
        p.refusal = "no_reagents";
        return p;
    }
    std::uint32_t const wanted = std::min({ f.wanted, f.max_items, f.carry_room });
    std::uint32_t const need = (wanted + yield - 1u) / yield;
    std::uint32_t const cap = std::min(f.max_items, f.carry_room) / yield;
    p.casts = std::min({ need, f.casts_possible, std::max<std::uint32_t>(cap, 1u) });
    p.items = p.casts * yield;
    p.trimmed = p.items < f.wanted;
    return p;
}

// How many more of an item the bot may hold: a unique item (max count 1, like Mana Jade) it already carries leaves none, 0 max count
// means no limit.
constexpr std::uint32_t CarryRoom(std::uint32_t max_count, std::uint32_t carried)
{
    if (max_count == 0)
        return 0xFFFFFFFFu;
    return max_count > carried ? max_count - carried : 0u;
}

// A finished order goes to the requester by mail, unless the item cannot travel that way: a conjured item is refused by the mail, so the
// bot hands it over in a trade window instead. A soulbound item cannot be given away by either route.
enum class Delivery : std::uint8_t { Mail, Trade, Bound };

constexpr Delivery DeliveryFor(bool conjured, bool soulbound)
{
    if (soulbound)
        return Delivery::Bound;
    return conjured ? Delivery::Trade : Delivery::Mail;
}

// The next cast of an order waits for mana. Out of mana, each cast stood the drinking mage up again (a cast stands the bot first), the
// cast failed, and the drink rule sat it down: it bobbed up and down on the spot and never drank (Tornn, 2026-10-27). So a bot that is
// drinking finishes the drink (mana full), and one short of the cast's cost waits for the drink rule (or its own regeneration).
// A cast without a mana cost (most profession recipes) never waits.
constexpr bool WaitForMana(bool drinking, std::uint32_t mana, std::uint32_t max_mana, std::uint32_t cost)
{
    if (cost == 0)
        return false;
    if (drinking && mana < max_mana)
        return true;
    return mana < cost;
}

} // namespace Playerbot::V2::PlayerCraft
