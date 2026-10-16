// WorldFactionMix - share of an anchor's own faction among the world bots around it, by zone: the pure rule behind
// WorldBotSpawner (new spawns and wakes).
//
// the owner, 2026-10-11: in the contested zones (Plaguelands around Light's Hope Chapel, Stranglethorn, Hillsbrad, Arathi...) there must
// be a real enemy presence at the player's level for world PvP, while capitals and home zones stay mostly the player's own faction.
// Zones are data (kZoneTiers); a zone not listed keeps the plain FriendlyPct. The level window is not touched here: enemy bots
// are drawn from the same level range as friendly ones.
// Pure: no game types, tested in tests/game/ClassicWorldFactionMix.cpp.

#pragma once

#include <algorithm>
#include <cstdint>

namespace Playerbot::V2::Fleet {

enum class ZoneTier : uint8_t
{
    Unlisted,    // FriendlyPct
    Capital,     // capital cities
    Home,        // faction-only starter / home zones
    Border,      // mostly one faction's, the other faction visits
    Contested,   // both factions quest and fight here
};

struct ZoneTierEntry { uint32_t zone; ZoneTier tier; };

// Classic zone ids (AreaTable).
inline constexpr ZoneTierEntry kZoneTiers[] = {
    // capitals
    { 1519, ZoneTier::Capital }, { 1537, ZoneTier::Capital }, { 1657, ZoneTier::Capital },   // Stormwind, Ironforge, Darnassus
    { 1637, ZoneTier::Capital }, { 1497, ZoneTier::Capital }, { 1638, ZoneTier::Capital },   // Orgrimmar, Undercity, Thunder Bluff
    // home zones
    { 12, ZoneTier::Home }, { 1, ZoneTier::Home }, { 141, ZoneTier::Home }, { 38, ZoneTier::Home }, { 40, ZoneTier::Home },   // Elwynn, Dun Morogh, Teldrassil, Loch Modan, Westfall
    { 14, ZoneTier::Home }, { 215, ZoneTier::Home }, { 85, ZoneTier::Home }, { 130, ZoneTier::Home },                         // Durotar, Mulgore, Tirisfal, Silverpine
    // one faction's, the other visits
    { 148, ZoneTier::Border }, { 44, ZoneTier::Border }, { 10, ZoneTier::Border }, { 17, ZoneTier::Border },                  // Darkshore, Redridge, Duskwood, Barrens
    { 331, ZoneTier::Border }, { 406, ZoneTier::Border }, { 405, ZoneTier::Border }, { 8, ZoneTier::Border },                  // Ashenvale, Stonetalon, Desolace, Swamp of Sorrows
    { 11, ZoneTier::Border },                                                                                                  // Wetlands
    // contested
    { 139, ZoneTier::Contested }, { 28, ZoneTier::Contested }, { 33, ZoneTier::Contested }, { 267, ZoneTier::Contested },     // Eastern + Western Plaguelands, Stranglethorn, Hillsbrad
    { 45, ZoneTier::Contested }, { 36, ZoneTier::Contested }, { 47, ZoneTier::Contested }, { 3, ZoneTier::Contested },        // Arathi, Alterac, Hinterlands, Badlands
    { 51, ZoneTier::Contested }, { 46, ZoneTier::Contested }, { 4, ZoneTier::Contested }, { 15, ZoneTier::Contested },        // Searing Gorge, Burning Steppes, Blasted Lands, Dustwallow
    { 440, ZoneTier::Contested }, { 490, ZoneTier::Contested }, { 1377, ZoneTier::Contested }, { 618, ZoneTier::Contested }, // Tanaris, Un'Goro, Silithus, Winterspring
    { 361, ZoneTier::Contested }, { 16, ZoneTier::Contested }, { 357, ZoneTier::Contested }, { 400, ZoneTier::Contested },    // Felwood, Azshara, Feralas, Thousand Needles
};

inline ZoneTier TierOfZone(uint32_t zone)
{
    for (ZoneTierEntry const& e : kZoneTiers)
        if (e.zone == zone)
            return e.tier;
    return ZoneTier::Unlisted;
}

struct FactionMix
{
    uint32_t default_pct   = 80;   // Playerbot.World.FriendlyPct: zones not listed (these struct values are test data; the spawner and conf ship 100)
    uint32_t capital_pct   = 90;   // Playerbot.World.CapitalFriendlyPct
    uint32_t home_pct      = 90;   // Playerbot.World.HomeFriendlyPct
    uint32_t border_pct    = 75;   // Playerbot.World.BorderFriendlyPct
    uint32_t contested_pct = 60;   // Playerbot.World.ContestedFriendlyPct
    bool     by_zone       = true; // Playerbot.World.ZoneMix: false = FriendlyPct everywhere (the old behaviour)
};

// Percent (0..100) of the bots around an anchor in `zone` that belong to the anchor's own faction.
inline uint32_t FriendlyPctFor(FactionMix const& m, uint32_t zone)
{
    uint32_t pct = m.default_pct;
    if (m.by_zone)
        switch (TierOfZone(zone))
        {
            case ZoneTier::Capital:   pct = m.capital_pct; break;
            case ZoneTier::Home:      pct = m.home_pct; break;
            case ZoneTier::Border:    pct = m.border_pct; break;
            case ZoneTier::Contested: pct = m.contested_pct; break;
            case ZoneTier::Unlisted:  break;
        }
    return std::min<uint32_t>(100, pct);
}

// Faction of one bot around an anchor: the share is relative to the ANCHOR's faction, so an Alliance player sees `pct`% Alliance and a
// Horde player sees `pct`% Horde. roll is 0..99.
inline bool BotIsAlliance(bool anchor_alliance, uint32_t roll, uint32_t pct)
{
    return roll < pct ? anchor_alliance : !anchor_alliance;
}

// The faction a capital city belongs to (playtest, 2026-10-12: a level 60 Horde troll walked around Ironforge among level 21s; a real
// Horde player does not stroll through an Alliance capital). 1 = Alliance, 2 = Horde, 0 = not a capital.
inline int CapitalFactionOfZone(uint32_t zone)
{
    switch (zone)
    {
        case 1519: case 1537: case 1657: return 1;   // Stormwind, Ironforge, Darnassus
        case 1637: case 1497: case 1638: return 2;   // Orgrimmar, Undercity, Thunder Bluff
        default: return 0;
    }
}

// Faction of one bot around an anchor standing in `zone`: in a capital it is always the capital's own faction, whoever the anchor is
// (a Horde player visiting Ironforge meets Alliance bots); elsewhere the zone share relative to the anchor.
inline bool BotIsAllianceInZone(uint32_t zone, bool anchor_alliance, uint32_t roll, uint32_t pct)
{
    if (int const capital = CapitalFactionOfZone(zone))
        return capital == 1;
    return BotIsAlliance(anchor_alliance, roll, pct);
}

} // namespace Playerbot::V2::Fleet
