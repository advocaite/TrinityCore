// ChannelTalk - reading what a real player writes in General, Trade or LookingForGroup, and the lines bots post there. Pure: no
// Player / BotAI access, tested offline (tests/game/ClassicChannelTalk.cpp). Bot/ChannelChat.cpp applies it to the live world.
//
// A line is one of: a player looking for members (LFM, "LF healer", "need tank"), a player looking for a group (LFG, "lfg dm"), a
// want-to-buy (WTB, with an item link or a name), a question, a greeting, or nothing a bot answers.
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace Playerbot::V2::ChannelTalk
{

enum class Kind : std::uint8_t { None, LookingForMore, LookingForGroup, WantToBuy, Question, Greeting };

enum RoleBits : std::uint8_t { kTank = 1, kHealer = 2, kDps = 4 };

// The 1.12 dungeons by the words players use for them. `map` is the instance map, the level range is the usual party's.
struct Dungeon { char const* name; std::uint32_t map; std::uint8_t lo, hi; };
inline constexpr Dungeon kDungeons[] = {
    { "Ragefire Chasm",        389, 13, 18 }, { "Wailing Caverns",      43, 17, 24 }, { "the Deadmines",         36, 17, 26 },
    { "Shadowfang Keep",        33, 20, 30 }, { "Blackfathom Deeps",    48, 24, 32 }, { "the Stockade",          34, 24, 32 },
    { "Gnomeregan",             90, 29, 38 }, { "Razorfen Kraul",       47, 29, 38 }, { "the Scarlet Monastery", 189, 30, 45 },
    { "Razorfen Downs",        129, 37, 46 }, { "Uldaman",              70, 41, 51 }, { "Zul'Farrak",           209, 44, 54 },
    { "Maraudon",              349, 46, 55 }, { "the Sunken Temple",   109, 50, 60 }, { "Blackrock Depths",     230, 52, 60 },
    { "Lower Blackrock Spire", 229, 55, 60 }, { "Upper Blackrock Spire", 229, 58, 60 }, { "Dire Maul",          429, 56, 60 },
    { "Stratholme",            329, 58, 60 }, { "Scholomance",         289, 58, 60 },
};
constexpr std::size_t kDungeonCount = sizeof(kDungeons) / sizeof(kDungeons[0]);
constexpr int kNoDungeon = -1;

struct Ask
{
    Kind kind = Kind::None;
    std::uint8_t roles = 0;          // LookingForMore: the places wanted; LookingForGroup: the role the player says he plays
    int dungeon = kNoDungeon;        // index into kDungeons
    std::uint32_t item_entry = 0;    // WantToBuy with an item link
    std::string item_name;           // WantToBuy: the link's name or the words after "wtb", lower-case
};

inline std::string Lower(std::string s)
{
    for (char& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Replaces each item link (|cff..|Hitem:ENTRY:...|h[Name]|h|r) by its name; the first link's entry and name come out.
inline std::string StripLinks(std::string const& raw, std::uint32_t& first_entry, std::string& first_name)
{
    first_entry = 0;
    first_name.clear();
    std::string out;
    out.reserve(raw.size());
    std::size_t i = 0;
    while (i < raw.size())
    {
        if (raw[i] == '|' && i + 1 < raw.size())
        {
            char const t = raw[i + 1];
            if (t == 'c' && i + 2 < raw.size() && raw[i + 2] == 'n')         // |cnIQ4: (the 12.x client's quality colour)
            {
                std::size_t const colon = raw.find(':', i + 3);
                i = colon == std::string::npos ? raw.size() : colon + 1;
                continue;
            }
            if (t == 'c' && i + 10 <= raw.size()) { i += 10; continue; }      // |cAARRGGBB
            if (t == 'r') { i += 2; continue; }
            if (t == 'H')
            {
                std::size_t const end = raw.find("|h", i + 2);
                if (end == std::string::npos) { i += 2; continue; }
                std::string_view const link(raw.data() + i + 2, end - (i + 2));
                if (link.rfind("item:", 0) == 0 && !first_entry)
                    first_entry = std::uint32_t(std::strtoul(std::string(link.substr(5)).c_str(), nullptr, 10));
                i = end + 2;
                std::size_t const close = raw.find("|h", i);
                std::string name = raw.substr(i, close == std::string::npos ? std::string::npos : close - i);
                if (name.size() >= 2 && name.front() == '[' && name.back() == ']')
                    name = name.substr(1, name.size() - 2);
                if (first_name.empty() && first_entry)
                    first_name = Lower(name);
                out += name;
                i = close == std::string::npos ? raw.size() : close + 2;
                continue;
            }
        }
        out += raw[i++];
    }
    return out;
}

// Words of a lower-case line: letters, digits and apostrophes; everything else separates.
inline std::vector<std::string> Words(std::string const& lower)
{
    std::vector<std::string> w;
    std::string cur;
    for (char c : lower)
    {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '\'')
            cur += c;
        else if (!cur.empty())
        {
            w.push_back(cur);
            cur.clear();
        }
    }
    if (!cur.empty())
        w.push_back(cur);
    return w;
}

inline bool HasWord(std::vector<std::string> const& w, std::string_view x)
{
    return std::find(w.begin(), w.end(), x) != w.end();
}

inline bool HasPhrase(std::string const& lower, std::string_view x)
{
    return lower.find(x) != std::string::npos;
}

// The dungeon the line names, kNoDungeon when none.
inline int FindDungeon(std::vector<std::string> const& w, std::string const& lower)
{
    struct Alias { char const* word; int idx; };
    static constexpr Alias kWords[] = {
        { "rfc", 0 }, { "ragefire", 0 }, { "wc", 1 }, { "wailing", 1 }, { "dm", 2 }, { "vc", 2 }, { "deadmines", 2 }, { "deadmine", 2 },
        { "sfk", 3 }, { "shadowfang", 3 }, { "bfd", 4 }, { "blackfathom", 4 }, { "stocks", 5 }, { "stockade", 5 }, { "stockades", 5 },
        { "gnomer", 6 }, { "gnomeregan", 6 }, { "rfk", 7 }, { "kraul", 7 }, { "sm", 8 }, { "scarlet", 8 }, { "cath", 8 }, { "armory", 8 },
        { "lib", 8 }, { "library", 8 }, { "gy", 8 }, { "rfd", 9 }, { "downs", 9 }, { "ulda", 10 }, { "uld", 10 }, { "uldaman", 10 },
        { "zf", 11 }, { "zul'farrak", 11 }, { "zulfarrak", 11 }, { "mara", 12 }, { "maraudon", 12 }, { "st", 13 }, { "sunken", 13 },
        { "brd", 14 }, { "lbrs", 15 }, { "ubrs", 16 }, { "dme", 17 }, { "dmw", 17 }, { "dmn", 17 }, { "strat", 18 },
        { "stratholme", 18 }, { "strath", 18 }, { "scholo", 19 }, { "scholomance", 19 }, { "scholo's", 19 },
    };
    if (HasPhrase(lower, "dire maul")) return 17;
    if (HasPhrase(lower, "blackrock depths")) return 14;
    if (HasPhrase(lower, "lower blackrock") || HasPhrase(lower, "lower spire")) return 15;
    if (HasPhrase(lower, "upper blackrock") || HasPhrase(lower, "upper spire")) return 16;
    if (HasPhrase(lower, "sunken temple") || HasPhrase(lower, "temple of atal")) return 13;
    for (std::string const& x : w)
        for (Alias const& a : kWords)
            if (x == a.word)
                return a.idx;
    return kNoDungeon;
}

inline std::uint8_t FindRoles(std::vector<std::string> const& w)
{
    std::uint8_t r = 0;
    for (std::string const& x : w)
    {
        if (x == "tank" || x == "tanks" || x == "mt" || x == "prot") r |= kTank;
        else if (x == "heal" || x == "heals" || x == "healer" || x == "healers" || x == "heala" || x == "heale") r |= kHealer;
        else if (x == "dps" || x == "dd" || x == "damage" || x == "melee" || x == "caster" || x == "rdps" || x == "mdps") r |= kDps;
    }
    return r;
}

inline bool IsLfmWord(std::string const& x)
{
    if (x == "lfm" || x == "lf" || x == "need" || x == "needs" || x == "lfmore") return true;
    // lf1m, lf2m, lf3m, lf4m
    return x.size() == 4 && x[0] == 'l' && x[1] == 'f' && std::isdigit(static_cast<unsigned char>(x[2])) && x[3] == 'm';
}

// Reads one channel line.
inline Ask Parse(std::string const& raw)
{
    Ask a;
    std::uint32_t entry = 0;
    std::string link_name;
    std::string const lower = Lower(StripLinks(raw, entry, link_name));
    std::vector<std::string> const w = Words(lower);
    if (w.empty())
        return a;

    bool const wtb = HasWord(w, "wtb") || (w.front() == "buying") || HasPhrase(lower, "want to buy") || HasPhrase(lower, "looking to buy");
    if (wtb)
    {
        a.kind = Kind::WantToBuy;
        a.item_entry = entry;
        if (!link_name.empty())
            a.item_name = link_name;
        else
        {
            // The words after the verb, up to a price or a "pst".
            std::size_t start = 0;
            for (std::size_t i = 0; i < w.size(); ++i)
                if (w[i] == "wtb" || w[i] == "buying" || w[i] == "buy") { start = i + 1; break; }
            std::string name;
            for (std::size_t i = start; i < w.size(); ++i)
            {
                std::string const& x = w[i];
                if (x == "pst" || x == "pm" || x == "whisper" || x == "w" || x == "for" || x == "paying" || x == "msg" || x == "me") break;
                bool const count = (x.size() >= 2 && x[0] == 'x' && std::isdigit(static_cast<unsigned char>(x[1])))
                    || std::all_of(x.begin(), x.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
                if (count) continue;
                if (!name.empty()) name += ' ';
                name += x;
                if (name.size() > 40) break;
            }
            a.item_name = name;
        }
        if (!a.item_entry && a.item_name.size() < 3)
            a.kind = Kind::None;
        return a;
    }

    // "lfm" / "lf2m" always ask for members; "lf" / "need" only with a role or a dungeon ("lf gold farm" is not a group call).
    bool strong_lfm = false, weak_lfm = false, lfg = false;
    for (std::string const& x : w)
    {
        if (x == "lfg" || x == "lf2g") lfg = true;
        else if (x == "lf" || x == "need" || x == "needs") weak_lfm = true;
        else if (IsLfmWord(x)) strong_lfm = true;
    }
    if (HasPhrase(lower, "looking for group") || HasPhrase(lower, "looking for a group") || HasPhrase(lower, "lf group")
        || HasPhrase(lower, "lf a group"))
        lfg = true;
    if (HasPhrase(lower, "looking for more") || HasPhrase(lower, "looking for a tank") || HasPhrase(lower, "looking for a heal")
        || HasPhrase(lower, "looking for dps"))
        strong_lfm = true;
    std::uint8_t const roles = FindRoles(w);
    int const dungeon = FindDungeon(w, lower);
    bool const lfm = !lfg && (strong_lfm || (weak_lfm && (roles || dungeon != kNoDungeon)));
    if (lfm)
    {
        a.kind = Kind::LookingForMore;
        a.roles = roles ? roles : std::uint8_t(kDps);
        a.dungeon = dungeon;
        return a;
    }
    if (lfg)
    {
        a.kind = Kind::LookingForGroup;
        a.roles = roles;
        a.dungeon = dungeon;
        return a;
    }

    std::string const& f = w.front();
    bool const question = raw.find('?') != std::string::npos || f == "where" || f == "how" || f == "anyone" || f == "does"
        || f == "what" || f == "who" || f == "is" || f == "can" || HasPhrase(lower, "anyone know");
    if (question && w.size() >= 2)
    {
        a.kind = Kind::Question;
        return a;
    }
    if (w.size() <= 4 && (f == "hi" || f == "hello" || f == "hey" || f == "heya" || f == "hiya" || f == "yo" || f == "sup" || f == "greetings"
                          || f == "evening" || f == "morning" || f == "o"))
        a.kind = Kind::Greeting;
    return a;
}

// Is `level` inside the dungeon's band (with `slack` levels either way)?
inline bool LevelFits(int dungeon, std::uint32_t level, std::uint32_t slack = 0)
{
    if (dungeon < 0 || std::size_t(dungeon) >= kDungeonCount)
        return false;
    Dungeon const& d = kDungeons[dungeon];
    return level + slack >= d.lo && level <= std::uint32_t(d.hi) + slack;
}

// A text left with a placeholder nobody filled ("<subzone>", "%zone_name"): never say it.
inline bool HasUnfilledPlaceholder(std::string const& text)
{
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '<')
        {
            std::size_t j = i + 1;
            while (j < text.size() && (std::islower(static_cast<unsigned char>(text[j])) || text[j] == '_')) ++j;
            if (j > i + 1 && j < text.size() && text[j] == '>') return true;
        }
        else if (text[i] == '%' && i + 2 < text.size() && std::isalpha(static_cast<unsigned char>(text[i + 1]))
                 && (std::isalpha(static_cast<unsigned char>(text[i + 2])) || text[i + 2] == '_'))
            return true;
    }
    return false;
}

// ---- what bots write ----

// The whisper a bot sends to a player whose LFM wants its role.
inline std::string ShortName(int dungeon)
{
    if (dungeon < 0 || std::size_t(dungeon) >= kDungeonCount)
        return "it";
    std::string n = kDungeons[dungeon].name;
    return n.rfind("the ", 0) == 0 ? n.substr(4) : n;
}

inline std::string OfferLine(std::uint8_t role_bit, int dungeon, std::uint32_t level, std::uint32_t seed)
{
    static constexpr std::array<char const*, 8> kOpen = {
        "inv, {r} here", "{r} lvl {l}, inv pls", "hey, can {r} {d} if you still need one", "inv me, {r}", "{r} here, still need one?",
        "can come {r}", "i can {r} {d}, inv", "{r}, ready to go",
    };
    std::string s = kOpen[seed % kOpen.size()];
    auto put = [&s](std::string_view key, std::string const& val)
    {
        for (std::size_t p; (p = s.find(key)) != std::string::npos;)
            s.replace(p, key.size(), val);
    };
    put("{r}", role_bit == kTank ? std::string("tank") : role_bit == kHealer ? std::string((seed & 8u) ? "heals" : "healer") : std::string("dps"));
    put("{d}", ShortName(dungeon));
    put("{l}", std::to_string(level));
    return s;
}

// The whisper a member of a listed bot group sends to a player looking for a group.
inline std::string GroupSpotLine(int dungeon, std::uint32_t seed)
{
    std::string const d = dungeon >= 0 ? std::string(kDungeons[dungeon].name) : std::string("a run");
    switch (seed % 5)
    {
        case 0: return "we've got a spot for " + d + ", invite me and you're in";
        case 1: return "need one more for " + d + ", want in? just invite me";
        case 2: return "our " + d + " group has room, inv me to join";
        case 3: return "hey, forming " + d + " now, inv me and i'll pull you in";
        default: return "still lf1m for " + d + ", send me an invite";
    }
}

// The channel call of a bot that wants a group: role, the dungeon of its level (nullptr = any), its level. Many shapes, so a busy
// LookingForGroup channel does not repeat itself.
inline std::string LfgCallLine(std::uint8_t role_bit, char const* dungeon, std::uint32_t level, std::uint32_t seed)
{
    char const* role = role_bit == kTank ? "tank" : role_bit == kHealer ? "healer" : "dps";
    std::string const lvl = std::to_string(level);
    std::string const d = dungeon ? std::string(dungeon) : std::string();
    std::string const dshort = d.rfind("the ", 0) == 0 ? d.substr(4) : d;
    std::uint32_t const k = (seed >> 3) % 10;
    if (!dungeon)
    {
        switch (k)
        {
            case 0: return std::string(role) + " LFG";
            case 1: return std::string("lvl ") + lvl + " " + role + " lfg, anything";
            case 2: return std::string("LFG any dungeon, ") + role;
            case 3: return std::string(role) + " looking for a group, whisper me";
            case 4: return std::string("anyone need a ") + role + "?";
            case 5: return std::string(role) + " " + lvl + " lf group";
            case 6: return std::string("lfg, ") + role + ", can go now";
            case 7: return std::string("any groups need a ") + role + "? pst";
            case 8: return std::string(lvl) + " " + role + " LFG";
            default: return std::string("bored, ") + role + " lfg anything";
        }
    }
    switch (k)
    {
        case 0: return std::string(role) + " LFG " + dshort;
        case 1: return std::string("LFG ") + dshort + ", " + role + " here, whisper me";
        case 2: return std::string("lvl ") + lvl + " " + role + " lfg " + dshort;
        case 3: return std::string("anyone running ") + d + "? " + role + " looking";
        case 4: return std::string(role) + " lf " + dshort + " group";
        case 5: return std::string("LFG ") + dshort + " or anything, " + role;
        case 6: return std::string("need a ") + role + " for " + dshort + "? pst";
        case 7: return std::string(dshort) + " anyone? " + role + " lvl " + lvl;
        case 8: return std::string("lf group ") + dshort + ", " + role;
        default: return std::string(role) + " " + lvl + " LFG " + dshort + ", can summon at the stone";
    }
}

// The last few lines posted, realm-wide, so the same text does not come round again within the window.
class RecentLines
{
public:
    static constexpr std::size_t kKeep = 24;
    bool Seen(std::string const& line) const
    {
        std::uint64_t const h = Hash(line);
        return std::find(hashes_.begin(), hashes_.end(), h) != hashes_.end();
    }
    void Note(std::string const& line)
    {
        hashes_[next_ % kKeep] = Hash(line);
        ++next_;
    }

private:
    static std::uint64_t Hash(std::string const& s)
    {
        std::uint64_t h = 1469598103934665603ull;
        for (char c : Lower(s)) { h ^= std::uint8_t(c); h *= 1099511628211ull; }
        return h ? h : 1;
    }
    std::array<std::uint64_t, kKeep> hashes_{};
    std::size_t next_ = 0;
};

} // namespace Playerbot::V2::ChannelTalk
