// BossTactics - the player's own plan for a boss, given in plain words and kept per boss:
//
//   tactics Lucifron: adds first, no aoe, tank here
//   tactics Baron Geddon: boss first, no fear
//   tactics: adds first                        (the boss you have targeted)
//   tactics Lucifron                           (what is kept for him)
//   tactics Lucifron: clear                    (drop it)
//   tactics                                    (every boss you have tactics for)
//
// The words a plan knows:
//   adds first / kill the adds first / boss last     the damage dealers leave the boss alone while one of his adds is up
//   boss first / ignore the adds / adds last         the damage dealers stay on the boss while he is up; the tanks keep the adds
//   any order                                        back to the raid rules' own order
//   no aoe / aoe off            aoe ok / aoe on      no spell that hits more than one enemy during that fight
//   no fear, no sheep, no banish ... (CcDeny words)  that crowd control is not used during that fight ("fear ok" takes it back)
//   tank here / tank him here / tank on me           the tank that holds the boss drags him to where the player stands now
//   tank anywhere                                    no place
//   clear / reset / forget                           the whole plan for that boss
//
// The plan is the owner account's, stored per boss (OwnerRegistry, playerbot_v2_owner_tactics), and the raid brain applies it every
// time that boss is in the fight (RaidBrain.cpp). The model is not needed: the player's own words in this form work everywhere. With
// the bot bridge on, a free sentence ("on Lucifron kill the adds before him and nobody fear") becomes this form through the order
// "tactics ...".
//
// Pure text: no Player, no database (unit tested in tests/game/ClassicBossWarnings.cpp).

#pragma once

#include "../CcDeny.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Playerbot::Raid {

struct BossTactics
{
    enum class Kill : uint8_t { Any = 0, AddsFirst = 1, BossFirst = 2 };

    Kill     kill = Kill::Any;
    bool     no_aoe = false;
    uint16_t cc_deny = 0;          // CcDeny kinds left out during the fight
    bool     spot = false;         // tank the boss at (x, y, z) on map spot_map
    uint32_t spot_map = 0;
    float    x = 0.0f, y = 0.0f, z = 0.0f;

    bool Empty() const { return kill == Kill::Any && !no_aoe && !cc_deny && !spot; }
    bool operator==(BossTactics const&) const = default;
};

// "kill=adds;aoe=0;cc=1;spot=409,1012.5,-480.2,-180.0" - digits, letters and . , - = ; only (safe in a query).
inline std::string SerializeTactics(BossTactics const& t)
{
    std::string s;
    if (t.kill != BossTactics::Kill::Any)
        s += t.kill == BossTactics::Kill::AddsFirst ? "kill=adds;" : "kill=boss;";
    if (t.no_aoe)
        s += "aoe=0;";
    if (t.cc_deny)
        s += "cc=" + std::to_string(t.cc_deny) + ";";
    if (t.spot)
    {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "spot=%u,%.1f,%.1f,%.1f;", t.spot_map, t.x, t.y, t.z);
        s += buf;
    }
    if (!s.empty())
        s.pop_back();
    return s;
}

// The stored form back; unknown or broken parts are left out (a later version's word is not an error).
inline BossTactics ParseStoredTactics(std::string_view s)
{
    BossTactics t;
    while (!s.empty())
    {
        size_t const semi = s.find(';');
        std::string_view part = s.substr(0, semi);
        s = semi == std::string_view::npos ? std::string_view() : s.substr(semi + 1);
        size_t const eq = part.find('=');
        if (eq == std::string_view::npos)
            continue;
        std::string_view const key = part.substr(0, eq);
        std::string const value(part.substr(eq + 1));
        if (key == "kill")
            t.kill = value == "adds" ? BossTactics::Kill::AddsFirst : value == "boss" ? BossTactics::Kill::BossFirst : BossTactics::Kill::Any;
        else if (key == "aoe")
            t.no_aoe = value == "0";
        else if (key == "cc")
            t.cc_deny = uint16_t(std::strtoul(value.c_str(), nullptr, 10) & CcDeny::All);
        else if (key == "spot")
        {
            unsigned map = 0;
            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (std::sscanf(value.c_str(), "%u,%f,%f,%f", &map, &x, &y, &z) == 4)
            {
                t.spot = true;
                t.spot_map = map;
                t.x = x;
                t.y = y;
                t.z = z;
            }
        }
    }
    return t;
}

// "adds first, no AoE, no fear, tank at your spot" / "nothing".
inline std::string DescribeTactics(BossTactics const& t)
{
    std::string s;
    auto const add = [&s](std::string const& part) { s += (s.empty() ? "" : ", ") + part; };
    if (t.kill == BossTactics::Kill::AddsFirst) add("adds first");
    if (t.kill == BossTactics::Kill::BossFirst) add("boss first");
    if (t.no_aoe) add("no AoE");
    if (t.cc_deny) add("no " + CcDeny::Describe(t.cc_deny));
    if (t.spot) add("tank at your spot");
    return s.empty() ? "nothing" : s;
}

// One "tactics ..." line, read.
struct TacticsOrder
{
    std::string boss;            // the words that name the boss ("" = the player's target)
    bool clear = false;
    std::optional<BossTactics::Kill> kill;
    std::optional<bool> no_aoe;
    uint16_t cc_add = 0;
    uint16_t cc_remove = 0;
    bool tank_here = false;
    bool spot_off = false;
    std::vector<std::string> unknown;   // parts no word matched, as typed (lower case)

    bool Changes() const { return clear || kill || no_aoe || cc_add || cc_remove || tank_here || spot_off; }
    bool Show() const { return !Changes() && unknown.empty(); }
};

namespace TacticsDetail {

inline std::string Lower(std::string_view in)
{
    std::string out;
    for (char c : in)
    {
        unsigned char const u = static_cast<unsigned char>(c);
        if (c == '\t' || c == '\n' || c == '\r')
            c = ' ';
        if (c == ' ' && (out.empty() || out.back() == ' '))
            continue;
        out += char(std::tolower(u));
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '.' || out.back() == '!'))
        out.pop_back();
    return out;
}

inline std::string Trim(std::string s)
{
    size_t const b = s.find_first_not_of(' ');
    if (b == std::string::npos)
        return {};
    size_t const e = s.find_last_not_of(" .!");
    return s.substr(b, e - b + 1);
}

// The parts of the plan: split at "," ";" "+" and " and ".
inline std::vector<std::string> Parts(std::string const& s)
{
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i)
    {
        bool split = s[i] == ',' || s[i] == ';' || s[i] == '+';
        size_t skip = 1;
        if (!split && s.compare(i, 5, " and ") == 0)
        {
            split = true;
            skip = 5;
        }
        if (split)
        {
            if (std::string t = Trim(cur); !t.empty())
                out.push_back(t);
            cur.clear();
            i += skip - 1;
        }
        else
            cur += s[i];
    }
    if (std::string t = Trim(cur); !t.empty())
        out.push_back(t);
    return out;
}

// One part into the order; false = no word matched.
inline bool ReadPart(std::string p, TacticsOrder& o)
{
    for (std::string_view const lead : { "please ", "pls ", "we ", "let's ", "lets ", "then " })
        if (p.compare(0, lead.size(), lead) == 0)
            p.erase(0, lead.size());
    auto const is = [&p](std::initializer_list<std::string_view> words)
    {
        for (std::string_view w : words)
            if (p == w)
                return true;
        return false;
    };
    if (is({ "clear", "reset", "forget", "forget it", "none", "nothing", "delete", "remove" }))
    {
        o.clear = true;
        return true;
    }
    if (is({ "adds first", "kill adds first", "kill the adds first", "kill adds", "kill the adds", "boss last", "adds before boss",
             "adds before the boss", "adds then boss", "adds then the boss", "kill adds then boss", "add first", "kill add first" }))
    {
        o.kill = BossTactics::Kill::AddsFirst;
        return true;
    }
    if (is({ "boss first", "kill boss first", "kill the boss first", "ignore adds", "ignore the adds", "adds last", "boss then adds",
             "boss then the adds", "nuke boss", "nuke the boss", "zerg boss", "zerg the boss", "burn boss", "burn the boss" }))
    {
        o.kill = BossTactics::Kill::BossFirst;
        return true;
    }
    if (is({ "any order", "kill anything", "normal order", "kill order off", "no kill order" }))
    {
        o.kill = BossTactics::Kill::Any;
        return true;
    }
    if (is({ "no aoe", "aoe off", "no aoes", "dont aoe", "don't aoe", "do not aoe", "single target", "single target only" }))
    {
        o.no_aoe = true;
        return true;
    }
    if (is({ "aoe ok", "aoe on", "aoe is fine", "aoe is ok", "aoe allowed", "yes aoe" }))
    {
        o.no_aoe = false;
        return true;
    }
    if (is({ "tank here", "tank him here", "tank her here", "tank it here", "tank the boss here", "tank boss here", "tank on me",
             "tank him on me", "tank her on me", "tank it on me", "tank the boss on me", "pull him here", "pull her here",
             "pull it here", "drag him here", "drag her here", "drag it here", "tank spot here" }))
    {
        o.tank_here = true;
        return true;
    }
    if (is({ "tank anywhere", "tank where you want", "no tank spot", "tank spot off", "forget the spot" }))
    {
        o.spot_off = true;
        return true;
    }
    // "no fear", "don't sheep", "no cc" / "fear ok", "sheep is fine": crowd control for that fight.
    for (std::string_view const neg : { "no ", "dont ", "don't ", "do not ", "never " })
        if (p.compare(0, neg.size(), neg) == 0)
            if (uint16_t const kind = CcDeny::KindOfWord(p.substr(neg.size())))
            {
                o.cc_add |= kind;
                o.cc_remove &= uint16_t(~kind);
                return true;
            }
    for (std::string_view const pos : { " ok", " is ok", " is fine", " allowed", " on" })
        if (p.size() > pos.size() && p.compare(p.size() - pos.size(), pos.size(), pos) == 0)
            if (uint16_t const kind = CcDeny::KindOfWord(p.substr(0, p.size() - pos.size())))
            {
                o.cc_remove |= kind;
                o.cc_add &= uint16_t(~kind);
                return true;
            }
    return false;
}

} // namespace TacticsDetail

// A "tactics" / "tactic" line ("boss tactics", "plan for" too), else nullopt (any other order or chatter).
inline std::optional<TacticsOrder> ParseTacticsOrder(std::string_view line)
{
    using namespace TacticsDetail;
    std::string s = Lower(line);
    while (!s.empty() && (s.front() == ';' || s.front() == '.' || s.front() == ' '))
        s.erase(0, 1);
    std::string rest;
    bool found = false;
    for (std::string_view const head : { "boss tactics", "tactics", "tactic", "boss plan" })
        if (s.compare(0, head.size(), head) == 0 && (s.size() == head.size() || s[head.size()] == ' ' || s[head.size()] == ':'))
        {
            rest = s.substr(head.size());
            found = true;
            break;
        }
    if (!found)
        return std::nullopt;

    TacticsOrder o;
    std::string items;
    size_t const colon = rest.find(':');
    if (colon != std::string::npos)
    {
        o.boss = Trim(rest.substr(0, colon));
        items = rest.substr(colon + 1);
    }
    else
        items = rest;
    std::vector<std::string> parts = Parts(items);
    if (colon == std::string::npos && !parts.empty())
    {
        // No colon: "tactics adds first, no aoe" (for the target), "tactics Lucifron" (show his), or the name in front of the first
        // plan word ("tactics lucifron adds first, no aoe"): the longest tail of the first part that is a plan word, the rest the name.
        TacticsOrder probe;
        if (!ReadPart(parts.front(), probe))
        {
            std::string const one = parts.front();
            bool split = false;
            for (size_t sp = one.find(' '); sp != std::string::npos && !split; sp = one.find(' ', sp + 1))
            {
                TacticsOrder tail;
                if (ReadPart(one.substr(sp + 1), tail))
                {
                    o.boss = one.substr(0, sp);
                    parts.front() = one.substr(sp + 1);
                    split = true;
                }
            }
            if (!split && parts.size() == 1)
            {
                o.boss = one;
                parts.clear();
            }
        }
    }
    for (std::string_view const lead : { "for ", "on ", "vs ", "against " })
        if (o.boss.compare(0, lead.size(), lead) == 0)
            o.boss = Trim(o.boss.substr(lead.size()));
    for (std::string const& p : parts)
        if (!ReadPart(p, o))
            o.unknown.push_back(p);
    return o;
}

// The stored plan after the order; (x, y, z) on `map` is where the player stands ("tank here").
inline void ApplyTacticsOrder(BossTactics& t, TacticsOrder const& o, uint32_t map, float x, float y, float z)
{
    if (o.clear)
        t = BossTactics{};
    if (o.kill)
        t.kill = *o.kill;
    if (o.no_aoe)
        t.no_aoe = *o.no_aoe;
    t.cc_deny = uint16_t((t.cc_deny | o.cc_add) & ~o.cc_remove);
    if (o.spot_off)
        t.spot = false;
    if (o.tank_here)
    {
        t.spot = true;
        t.spot_map = map;
        t.x = x;
        t.y = y;
        t.z = z;
    }
}

// True when a name the player typed means this boss, case-insensitive: the whole name, or typed words (4+ letters in all) that are each
// a word of the name or the start of one of 4+ letters ("geddon", "baron geddon", "majo" for Majordomo Executus; "rag" is too short).
inline bool BossNameMatches(std::string_view typed, std::string_view bossName)
{
    std::string const t = TacticsDetail::Lower(typed);
    std::string const n = TacticsDetail::Lower(bossName);
    if (t.empty() || n.empty())
        return false;
    if (t == n)
        return true;
    if (t.size() < 4)
        return false;
    size_t i = 0;
    while (i < t.size())
    {
        size_t const e = t.find(' ', i);
        std::string const w = t.substr(i, e == std::string::npos ? std::string::npos : e - i);
        i = e == std::string::npos ? t.size() : e + 1;
        if (w.size() < 3)
            continue;
        bool hit = false;
        size_t j = 0;
        while (j < n.size())
        {
            size_t const f = n.find_first_of(" -", j);
            std::string const nw = n.substr(j, f == std::string::npos ? std::string::npos : f - j);
            j = f == std::string::npos ? n.size() : f + 1;
            if (nw == w || (w.size() >= 4 && nw.compare(0, w.size(), w) == 0))
                hit = true;
        }
        if (!hit)
            return false;
    }
    return true;
}

} // namespace Playerbot::Raid
