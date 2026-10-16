// ClassicHealScope - where the Classic healers' mana-efficiency rules apply (Combat/Apl/ApClassicHeal.h RaidHealing).
//
// They used to run in raid maps only. A level 21 party in Deadmines (playtest, 2026-10-09) runs out of mana for the same reasons a
// raid does - heals sized to the deficit, the efficient spell for everyone but an emergency or a tank under half health, heals
// already on their way counted, topping up held back at low mana, the tank pre-healed - so every group inside an instance map
// (5-man dungeon or raid) gets them. Open-world parties, solo play and battlegrounds keep the plain rules.
// Pure: no game types, tested in tests/game/ClassicHealScope.cpp.

#pragma once

namespace Playerbot::Combat::ClassicHeal {

inline bool EfficiencyRulesApply(bool raid_map, bool dungeon_map, bool battleground, bool vanilla_realm)
{
    return vanilla_realm && !battleground && (raid_map || dungeon_map);
}

}  // namespace Playerbot::Combat::ClassicHeal
