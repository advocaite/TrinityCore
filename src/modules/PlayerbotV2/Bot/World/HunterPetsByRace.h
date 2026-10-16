// HunterPetsByRace - Race -> default Hunter starter pet (creature template entry).
//
// Distribution-leveled Hunter bots skip their racial intro questline, which is
// where retail Hunters get their first pet (Northshire Vineyards "Hunter
// Training" / Coldridge Valley / Shadowglen / etc. all hand the player a
// race-themed pet). Without that quest, the bot has no pet — Call Pet (883)
// fires every tick into nothing.
//
// This table maps each Hunter-eligible race to the creature template a real
// player would tame as their first pet on that race's starter zone. The
// pipeline summons one of these for every distribution-leveled Hunter bot,
// scaled to the bot's current level.
//
// Creature entries are stable, well-known low-level beasts in TC's
// creature_template; renames are rare across patches.

#pragma once

#include "Bot/BotTypes.h"

namespace Playerbot::V2::World {

// Returns the creature template entry to use as a starter Hunter pet for
// `race`, or 0 if no race-specific default exists. The caller falls back to
// a faction-generic pet (Stormpike Wolf for Alliance, Mottled Boar for Horde)
// when this returns 0.
uint32 HunterPetEntryForRace(uint32 race);

// Faction-generic fallback pets, used when HunterPetEntryForRace returns 0
// (allied races / unmapped races). Standard starter beasts that exist in
// every TC creature_template.
constexpr uint32 GenericAllianceHunterPet = 299;   // Stormpike Wolf (Dun Morogh wolf, lvl 1-2)
constexpr uint32 GenericHordeHunterPet    = 5826;  // Mottled Boar (Valley of Trials, lvl 1-2)

// A world (ambient) hunter's own first pet, stable per bot: one of the faction's classic starter beasts picked by guid, so a street of
// hunters is not all one race's beast. Creature entries of HunterPetEntryForRace's classic races; the caller's tameable check still
// decides whether the pick exists as a beast in this data. `fallback` (the race's pet) is returned for an owned bot (guid 0).
inline uint32 VariedHunterPetEntry(bool alliance, uint64 guid, uint32 fallback)
{
    static constexpr uint32 kAlliance[] = { 299, 1132, 2031, 299, 1132 };      // wolf, boar, spider
    static constexpr uint32 kHorde[]    = { 5826, 1908, 2956, 2952, 5826, 1908 }; // boar, wolf, plainstrider, raptor
    if (!guid)
        return fallback;
    uint64 h = guid * 0x9E3779B97F4A7C15ull;
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 32;
    if (alliance)
        return kAlliance[h % (sizeof(kAlliance) / sizeof(kAlliance[0]))];
    return kHorde[h % (sizeof(kHorde) / sizeof(kHorde[0]))];
}

// The race's starter pet, else the faction-generic one.
inline uint32 StarterHunterPetEntry(uint32 race, bool alliance)
{
    if (uint32 const entry = HunterPetEntryForRace(race))
        return entry;
    return alliance ? GenericAllianceHunterPet : GenericHordeHunterPet;
}

} // namespace Playerbot::V2::World
