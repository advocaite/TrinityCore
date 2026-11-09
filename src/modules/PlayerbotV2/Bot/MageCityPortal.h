// MageCityPortal - the pure part of "portal <city>" / "teleport <city>": which Classic mage spell opens a portal or teleports to a city.
// The spells are found by id, not by name: the names are in the server's language, so on a ruRU server "portal ironforge" (the add-on's
// Open a portal menu sends the English city) matched no Russian "Portal: Stalgorn" and the mage said it knew no such spell (Tornn, 2026-10-27).
// Ids from the 70532 client's SpellName. Tested in tests/game/ClassicMageCityPortal.cpp; the command is in BotCommandParser.cpp.
// The Russian city names are UTF-8 bytes (this file is Latin-1).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace Playerbot::V2::MageCity {

struct City
{
    std::uint32_t teleport = 0;
    std::uint32_t portal = 0;
    std::string_view words[8];   // lower case English name and short forms, then the Russian name (lower case and capitalised)
};

inline constexpr City kCities[] = {
    { 3561, 10059, { "stormwind", "sw", "\xd1\x88\xd1\x82\xd0\xbe\xd1\x80\xd0\xbc\xd0\xb3\xd1\x80\xd0\xb0\xd0\xb4",
                     "\xd0\xa8\xd1\x82\xd0\xbe\xd1\x80\xd0\xbc\xd0\xb3\xd1\x80\xd0\xb0\xd0\xb4" } },
    { 3562, 11416, { "ironforge", "if", "\xd1\x81\xd1\x82\xd0\xb0\xd0\xbb\xd1\x8c\xd0\xb3\xd0\xbe\xd1\x80\xd0\xbd",
                     "\xd0\xa1\xd1\x82\xd0\xb0\xd0\xbb\xd1\x8c\xd0\xb3\xd0\xbe\xd1\x80\xd0\xbd" } },
    { 3565, 11419, { "darnassus", "darn", "\xd0\xb4\xd0\xb0\xd1\x80\xd0\xbd\xd0\xb0\xd1\x81", "\xd0\x94\xd0\xb0\xd1\x80\xd0\xbd\xd0\xb0\xd1\x81" } },
    { 3567, 11417, { "orgrimmar", "org", "og", "\xd0\xbe\xd1\x80\xd0\xb3\xd1\x80\xd0\xb8\xd0\xbc\xd0\xbc\xd0\xb0\xd1\x80",
                     "\xd0\x9e\xd1\x80\xd0\xb3\xd1\x80\xd0\xb8\xd0\xbc\xd0\xbc\xd0\xb0\xd1\x80" } },
    { 3563, 11418, { "undercity", "uc", "\xd0\xbf\xd0\xbe\xd0\xb4\xd0\xb3\xd0\xbe\xd1\x80\xd0\xbe\xd0\xb4",
                     "\xd0\x9f\xd0\xbe\xd0\xb4\xd0\xb3\xd0\xbe\xd1\x80\xd0\xbe\xd0\xb4" } },
    { 3566, 11420, { "thunder bluff", "thunderbluff", "tb",
                     "\xd0\xb3\xd1\x80\xd0\xbe\xd0\xbc\xd0\xbe\xd0\xb2\xd0\xbe\xd0\xb9 \xd1\x83\xd1\x82\xd0\xb5\xd1\x81",
                     "\xd0\x93\xd1\x80\xd0\xbe\xd0\xbc\xd0\xbe\xd0\xb2\xd0\xbe\xd0\xb9 \xd1\x83\xd1\x82\xd0\xb5\xd1\x81",
                     "\xd0\xb3\xd1\x80\xd0\xbe\xd0\xbc\xd0\xbe\xd0\xb2\xd0\xbe\xd0\xb9 \xd1\x83\xd1\x82\xd1\x91\xd1\x81",
                     "\xd0\x93\xd1\x80\xd0\xbe\xd0\xbc\xd0\xbe\xd0\xb2\xd0\xbe\xd0\xb9 \xd1\x83\xd1\x82\xd1\x91\xd1\x81" } },
};

// The city `want` names (ASCII already lower case), nullptr when none: a word of the city exactly, or a part of its English name of
// three letters or more ("storm", "thunder").
inline City const* FindCity(std::string_view want)
{
    if (want.empty())
        return nullptr;
    for (City const& c : kCities)
        for (std::string_view w : c.words)
            if (!w.empty() && w == want)
                return &c;
    if (want.size() >= 3)
        for (City const& c : kCities)
            if (c.words[0].find(want) != std::string_view::npos)
                return &c;
    return nullptr;
}

// The portal (or teleport) spell to `want` that `knows` accepts, 0 when the city is not one of these or the mage lacks the spell.
template <typename Knows>
std::uint32_t CitySpell(std::string_view want, bool portal, Knows const& knows)
{
    City const* c = FindCity(want);
    if (!c)
        return 0;
    std::uint32_t const spell = portal ? c->portal : c->teleport;
    return knows(spell) ? spell : 0;
}

} // namespace Playerbot::V2::MageCity
