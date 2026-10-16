// RaidFight - the small decisions that decide whether a raid bot fights or walks, as plain functions so they are tested without a world
// (tests/game/ClassicRaidFight.cpp). Live MC 2026-10-11 (the owner: "half of the bots aren't fighting", about 15 casters and hunters at full mana
// doing nothing): the owner's Attack never released the DPS when the marked mob was still idle, and ranged DPS kept walking to their
// formation slot with the target 5-15 y away in line of sight.

#pragma once

#include <cstdint>

namespace Playerbot::V2 {

// How far a ranged raid bot's spells reach: the 30 y of the standard caster spells and the 35 y of a hunter's Auto Shot, less a margin so
// a target at the edge is walked to rather than cast at. The target's combat reach comes on top.
constexpr float kRaidCastReach = 28.0f;

// The owner's Attack was spoken within `window_ms`: it stands for the whole group, whoever carries the order. The party "attack" gives
// the Engage order to the main tank only while the marked mob is idle (the others are refused by the pull gate, "None+follow_order"),
// so the group's release cannot wait for a non-tank to carry it. A Pull never sets this: it is the tanks' own engage.
constexpr bool OwnerAttackRecent(std::uint32_t now_ms, std::uint32_t attack_ms, std::uint32_t window_ms)
{
    return attack_ms != 0 && now_ms >= attack_ms && now_ms - attack_ms < window_ms;
}

// A ranged DPS casts rather than walks to its formation slot while its target is in reach and in line of sight: a cast is refused while
// the bot moves, so a slot walk with the target in range is a bot that never fights. Out of reach or out of sight the slot walk (and the
// range / LoS rules) go on.
constexpr bool CastsInsteadOfSlotWalk(bool ranged_dps, bool has_victim, bool victim_in_los, float dist, float victim_reach)
{
    return ranged_dps && has_victim && victim_in_los && dist <= kRaidCastReach + victim_reach;
}

} // namespace Playerbot::V2
