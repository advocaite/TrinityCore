// Classic 1.60 (WoW Forever) Legacy perks shared by more than one place in the core and the scripts. Pure data, no server types.
#pragma once

#include <cstdint>

namespace ClassicLegacyPerks
{
    // Conquering Hero 1225506 and Second Chance 1225508: "effects gained from celebrating the defeat of bosses". The world buffs the
    // server casts on a boss kill (Rallying Cry of the Dragonslayer for Onyxia and Nefarian, Warchief's Blessing for Rend, Spirit of
    // Zandalar for Hakkar) and the client's copies of them.
    constexpr bool IsBossCelebrationBuff(uint32_t spellId)
    {
        switch (spellId)
        {
            case 22888: case 355363: case 1304452:      // Rallying Cry of the Dragonslayer
            case 16609: case 355366: case 1304594:      // Warchief's Blessing
            case 24425:                                 // Spirit of Zandalar
                return true;
            default:
                return false;
        }
    }

    constexpr uint32_t CONQUERING_HERO              = 1225506;  // effect 0: duration percent
    constexpr uint32_t SECOND_CHANCE                = 1225508;
    constexpr uint32_t SECOND_CHANCE_USED           = 1262777;  // death persistent, infinite: the boss buffs already survived a death
}
