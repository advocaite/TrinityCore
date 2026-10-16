// RezRule - who may resurrect, and when, in a group fight. Plain functions so they are tested without a world
// (tests/game/ClassicRezRule.cpp). Raid rules (playtest, MC 2026-10-10): only a druid (Rebirth) may raise someone while the fight is on;
// a priest, paladin or shaman waits until it is over, and one rezzer takes each corpse.

#pragma once

#include <cstdint>

namespace Playerbot::V2 {

// A combat resurrection: Rebirth (druid) and Raise Ally (death knight). Resurrection, Redemption and Ancestral Spirit are out-of-combat
// spells, and the core refuses them to a caster in combat (AFFECTING_COMBAT).
constexpr bool RezIsCombatRez(std::uint8_t cls)
{
    return cls == 11 /*CLASS_DRUID*/ || cls == 6 /*CLASS_DEATH_KNIGHT*/;
}

// The fight is live when a tank of the group is in combat, or three or more members are. One member in combat (a straggler, a pet
// pulled by a stray mob, a stalker's false combat) is not a fight, so it does not hold the raid's rezzes back for good.
constexpr bool RezFightLive(int tanks_in_combat, int members_in_combat)
{
    return tanks_in_combat > 0 || members_in_combat >= 3;
}

// One rezzer per corpse. `rezzers` ready rezzers (this one included) are ordered by guid; `index` is how many of them rank before this
// one. The front of the order turns every three seconds, so a front rezzer that cannot cast (no mana, a cooldown, another order) holds
// the others back for no more than that. Returns this rezzer's place: 0 takes the first corpse, 1 the second, and so on.
constexpr int RezRank(int index, int rezzers, std::uint32_t now_ms)
{
    return rezzers <= 1 ? 0 : (index + rezzers - int((now_ms / 3000u) % std::uint32_t(rezzers))) % rezzers;
}

} // namespace Playerbot::V2
