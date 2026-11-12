// TalentCode - the talent string of the Wowhead Forever talent calculator, read and written.
//
// A player builds talents at https://www.wowhead.com/forever/talent-calc/<class>/... and gives a bot the link (or the code
// part of it): "change talents <link>". The code holds one run of digits per talent tab, tabs left to right as the game shows
// them, joined with '-': digit i of a tab is the points in that tab's talent i, talents counted row by row, left to right.
// Trailing zeroes of a tab and trailing dashes are left out, so "--05003201" is a warrior with points in Protection only.
// Wowhead puts a format version in front ("v2") and the levelling order behind the code ("_t0/2BEffh"); both are skipped.
//
// Pure functions without game includes, so the tests can call them. The tabs of a class come from the server's own talent
// data (Classic::GetTalentTabs, ClassicSpec.h), laid out the same way.

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Playerbot::Classic::TalentCode {

// Class ids as the game numbers them (SharedDefines.h Classes), and the class word Wowhead puts in its links.
struct ClassSlug { uint8_t cls; std::string_view slug; };
inline constexpr ClassSlug kClassSlugs[] = {
    { 1, "warrior" }, { 2, "paladin" }, { 3, "hunter" }, { 4, "rogue" }, { 5, "priest" },
    { 7, "shaman" }, { 8, "mage" }, { 9, "warlock" }, { 11, "druid" },
};

inline uint8_t ClassFromSlug(std::string_view slug)
{
    for (ClassSlug const& c : kClassSlugs)
        if (c.slug == slug)
            return c.cls;
    return 0;
}

inline std::string_view SlugOfClass(uint8_t cls)
{
    for (ClassSlug const& c : kClassSlugs)
        if (c.cls == cls)
            return c.slug;
    return {};
}

enum class Error : uint8_t
{
    None,
    Empty,          // no digits at all
    UnknownClass,   // the link names a class word that is not one of the nine
    NoCode,         // a talent calculator link without a build in it
    BadChar,        // something other than digits and dashes in the code
    TooManyTabs,    // more tabs than the class has
    TabTooLong,     // more digits in a tab than it has talents
    OverMax,        // more points in a talent than its ranks
    TooManyPoints,  // more points than a level 60 character has
    RowLocked,      // points in a row the points above it do not open (5 per row)
};

struct Parsed
{
    Error error = Error::Empty;
    uint8_t cls = 0;                          // the class the link names, 0 when only a code was given
    std::vector<std::vector<uint8_t>> tabs;   // the points per talent of each tab, as written (a tab may be shorter than its talents)

    bool ok() const { return error == Error::None; }
};

// Reads a Wowhead talent calculator link, or the code alone, from the text of a chat order (the first word that holds one).
inline Parsed Parse(std::string_view text)
{
    Parsed out;
    std::string s(text);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });

    std::string code;
    static constexpr std::string_view kCalc = "talent-calc/";
    if (size_t const at = s.find(kCalc); at != std::string::npos)
    {
        size_t const clsStart = at + kCalc.size();
        size_t clsEnd = clsStart;
        while (clsEnd < s.size() && s[clsEnd] != '/' && s[clsEnd] != ' ' && s[clsEnd] != '?' && s[clsEnd] != '#')
            ++clsEnd;
        out.cls = ClassFromSlug(std::string_view(s).substr(clsStart, clsEnd - clsStart));
        if (!out.cls)
        {
            out.error = Error::UnknownClass;
            return out;
        }
        if (clsEnd >= s.size() || s[clsEnd] != '/')
        {
            out.error = Error::NoCode;
            return out;
        }
        size_t codeEnd = clsEnd + 1;
        while (codeEnd < s.size() && s[codeEnd] != '/' && s[codeEnd] != ' ' && s[codeEnd] != '?' && s[codeEnd] != '#')
            ++codeEnd;
        code = s.substr(clsEnd + 1, codeEnd - clsEnd - 1);
        if (code.empty())
        {
            out.error = Error::NoCode;
            return out;
        }
    }
    else
    {
        size_t const b = s.find_first_not_of(" \t");
        if (b == std::string::npos)
            return out;
        size_t const e = s.find_first_of(" \t", b);
        code = s.substr(b, e == std::string::npos ? std::string::npos : e - b);
    }

    // The tree version in front: "v" and a word ("vkndmh", the current Forever trees on 11-12) or one digit (the older "v2").
    if (code.size() >= 2 && code[0] == 'v')
    {
        if (std::isalpha(static_cast<unsigned char>(code[1])))
        {
            size_t end = 1;
            while (end < code.size() && std::isalpha(static_cast<unsigned char>(code[end])))
                ++end;
            code.erase(0, end);
        }
        else if (std::isdigit(static_cast<unsigned char>(code[1])))
            code.erase(0, 2);
    }
    // "_t0..." behind: the levelling order and other extras, not points.
    if (size_t const u = code.find('_'); u != std::string::npos)
        code.erase(u);

    bool anyPoint = false;
    out.tabs.emplace_back();
    for (char c : code)
    {
        if (c == '-')
            out.tabs.emplace_back();
        else if (c >= '0' && c <= '9')
        {
            out.tabs.back().push_back(uint8_t(c - '0'));
            anyPoint = anyPoint || c != '0';
        }
        else
        {
            out.error = Error::BadChar;
            return out;
        }
    }
    if (out.tabs.size() > 3)
    {
        out.error = Error::TooManyTabs;
        return out;
    }
    out.error = anyPoint ? Error::None : Error::Empty;
    return out;
}

// One talent of a tab: its ranks and its row (0 = top).
struct Slot
{
    uint8_t maxRanks = 0;
    uint8_t row = 0;
};
using Layout = std::vector<std::vector<Slot>>;   // tabs left to right, each tab's talents row by row, left to right

struct Check
{
    Error error = Error::None;
    int tab = -1;    // where the error is (0-based), -1 for the whole build
    int slot = -1;
    std::vector<int> perTab;   // points per tab
    int total = 0;

    bool ok() const { return error == Error::None; }
};

// Fits the parsed points on the class's tabs: no more tabs or talents than it has, no talent over its ranks, no more points than
// maxPoints, and no points in a row before the tab holds 5 per row above it. Prerequisite talents are the game's check.
inline Check Validate(Parsed const& parsed, Layout const& layout, int maxPoints = 51)
{
    Check out;
    out.perTab.assign(layout.size(), 0);
    if (parsed.tabs.size() > layout.size())
    {
        out.error = Error::TooManyTabs;
        return out;
    }
    for (size_t t = 0; t < parsed.tabs.size(); ++t)
    {
        std::vector<uint8_t> const& points = parsed.tabs[t];
        std::vector<Slot> const& slots = layout[t];
        if (points.size() > slots.size())
        {
            out.error = Error::TabTooLong;
            out.tab = int(t);
            return out;
        }
        for (size_t i = 0; i < points.size(); ++i)
        {
            if (points[i] > slots[i].maxRanks)
            {
                out.error = Error::OverMax;
                out.tab = int(t);
                out.slot = int(i);
                return out;
            }
            out.perTab[t] += points[i];
        }
        // Row gates: a talent's row opens with 5 points per row in the rows above it.
        for (size_t i = 0; i < points.size(); ++i)
        {
            if (!points[i])
                continue;
            int above = 0;
            for (size_t j = 0; j < points.size(); ++j)
                if (slots[j].row < slots[i].row)
                    above += points[j];
            if (above < 5 * int(slots[i].row))
            {
                out.error = Error::RowLocked;
                out.tab = int(t);
                out.slot = int(i);
                return out;
            }
        }
        out.total += out.perTab[t];
    }
    if (out.total == 0)
        out.error = Error::Empty;
    else if (out.total > maxPoints)
        out.error = Error::TooManyPoints;
    return out;
}

// True when every talent with points sits in a row its tab has opened: 5 points per row in the rows above it, in the same tab.
// The game's own check (TraitMgr::ValidateConfig) lets a Forever talent through on fewer, and Wowhead then drops such a talent
// from the build (T4 on 11-12: an Arms bot with Anger Management on 9 points above it).
inline bool RowsOpen(std::vector<std::vector<uint8_t>> const& tabs, Layout const& layout)
{
    for (size_t t = 0; t < tabs.size() && t < layout.size(); ++t)
    {
        int above[8] = {};
        for (size_t i = 0; i < tabs[t].size() && i < layout[t].size(); ++i)
            if (layout[t][i].row < 7)
                for (int r = layout[t][i].row + 1; r < 8; ++r)
                    above[r] += tabs[t][i];
        for (size_t i = 0; i < tabs[t].size() && i < layout[t].size(); ++i)
            if (tabs[t][i] && layout[t][i].row < 8 && above[layout[t][i].row] < 5 * int(layout[t][i].row))
                return false;
    }
    return true;
}

// The code of points per tab, as the calculator writes it: trailing zeroes of each tab and trailing dashes left out.
inline std::string Encode(std::vector<std::vector<uint8_t>> const& tabs)
{
    std::string out;
    for (size_t t = 0; t < tabs.size(); ++t)
    {
        std::string tab;
        for (uint8_t p : tabs[t])
            tab.push_back(char('0' + std::min<uint8_t>(p, 9)));
        while (!tab.empty() && tab.back() == '0')
            tab.pop_back();
        if (t)
            out.push_back('-');
        out += tab;
    }
    while (!out.empty() && out.back() == '-')
        out.pop_back();
    return out;
}

// The tree version Wowhead's Forever calculator writes now (11-12). An older version opens too, but Wowhead then moves the build to the
// current trees and says so; with this one the link opens as it is.
inline constexpr std::string_view kTreeVersion = "kndmh";

inline std::string Link(uint8_t cls, std::string const& code)
{
    std::string_view const slug = SlugOfClass(cls);
    if (slug.empty())
        return {};
    return "https://www.wowhead.com/forever/talent-calc/" + std::string(slug) + "/v" + std::string(kTreeVersion) + code + "_t0";
}

// The order a bot buys a build's ranks in as it levels: the tab with the most points first (the earlier tab on a tie), each tab's
// talents row by row, every rank of a talent before the next. Returns (tab, slot, ranks) for each talent with points.
struct Step { int tab; int slot; int ranks; };
inline std::vector<Step> LevellingOrder(Parsed const& parsed)
{
    std::vector<int> order(parsed.tabs.size());
    std::vector<int> points(parsed.tabs.size(), 0);
    for (size_t t = 0; t < parsed.tabs.size(); ++t)
    {
        order[t] = int(t);
        for (uint8_t p : parsed.tabs[t])
            points[t] += p;
    }
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return points[a] > points[b]; });
    std::vector<Step> steps;
    for (int t : order)
        for (size_t i = 0; i < parsed.tabs[t].size(); ++i)
            if (parsed.tabs[t][i])
                steps.push_back({ t, int(i), parsed.tabs[t][i] });
    return steps;
}

// What the bot says when a code does not fit (English; the Russian lines are in the order's reply).
inline std::string_view ErrorText(Error e)
{
    switch (e)
    {
        case Error::Empty:         return "that build has no points in it";
        case Error::UnknownClass:  return "I can't tell which class that link is for";
        case Error::NoCode:        return "that link has no build in it, spend some points in the calculator first";
        case Error::BadChar:       return "that isn't a talent code, paste the whole Wowhead talent calculator link";
        case Error::TooManyTabs:   return "that code has more talent trees than my class";
        case Error::TabTooLong:    return "that code has more talents in a tree than my class has, is it from the Forever calculator?";
        case Error::OverMax:       return "that code puts more points in a talent than it has ranks, is it from the Forever calculator?";
        case Error::TooManyPoints: return "that build spends more than 51 points";
        case Error::RowLocked:     return "that build puts points in a row that isn't open yet (5 points per row above it)";
        default:                   return {};
    }
}

} // namespace Playerbot::Classic::TalentCode
