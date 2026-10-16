// SoakAnchors - characters that count as anchors for the world spawner although a bot plays them, so the world can be populated and
// watched with nobody logged in (overnight soaks). Playerbot.World.SoakAnchors = a comma-separated list of character names; empty
// (the default, and the release default) means none. An anchor is logged in and moved with the usual GM commands (`.playerbot login
// <name>`, `.tele name <name> <place>`); the spawner then fills the world around it like around a player, and never treats it as
// one of its own residents. Name parsing is pure and tested in tests/game/ClassicSoakAnchors.cpp.
#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>

namespace Playerbot::V2::Fleet
{

inline std::unordered_set<std::string> ParseSoakAnchors(std::string const& list)
{
    std::unordered_set<std::string> out;
    std::string cur;
    auto flush = [&]()
    {
        std::size_t b = 0, e = cur.size();
        while (b < e && std::isspace(static_cast<unsigned char>(cur[b]))) ++b;
        while (e > b && std::isspace(static_cast<unsigned char>(cur[e - 1]))) --e;
        if (e > b)
        {
            std::string name = cur.substr(b, e - b);
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            out.insert(std::move(name));
        }
        cur.clear();
    };
    for (char c : list)
    {
        if (c == ',' || c == ';') flush();
        else cur.push_back(c);
    }
    flush();
    return out;
}

// True when `name` is on the configured list. The list is re-read at most every 15 s.
bool IsSoakAnchor(std::string const& name);

} // namespace Playerbot::V2::Fleet
