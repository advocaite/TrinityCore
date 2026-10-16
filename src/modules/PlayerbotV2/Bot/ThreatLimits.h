// ThreatLimits - the one place the hard threat limits live, shared by the generic damage hold (States::EvaluateThreatHold) and the raid
// brain's hold orders (Raid::PublishThreatHolds). A mob switches to a non-tank at 110 % of the tank's threat in melee reach and 130 % at
// range; the hold never releases at or past these ratios, however long it has lasted (the release after a few seconds only applies below
// them). With separate copies, a caster at 1.15 x the tank was released by the brain and held for good by the generic gate.

#pragma once

namespace Playerbot {

constexpr float kThreatHardRatioMelee  = 1.0f;    // a melee DPS never releases above the tank's own threat
constexpr float kThreatHardRatioCaster = 1.10f;   // a caster keeps a tenth of margin under the 130 % a ranged attacker needs

// A raid tank with tens of thousands of threat takes one cast as a few percent, so a caster keeps ten points under the 130 % there (n10
// mc_geddon: a mage held at 109 % of the tank for 20 s while the tank, out of melee in the Inferno, gained nothing). Early in a pull the
// tank holds a few thousand and one Frostbolt is a fifth of it: below kRaidTankThreatFloor the 110 % limit stays.
constexpr float kThreatHardRatioCasterRaid = 1.20f;
constexpr float kRaidTankThreatFloor = 8000.0f;

constexpr float ThreatHardRatio(bool melee, bool raid = false, float tank_threat = 0.0f)
{
    return melee ? kThreatHardRatioMelee
                 : (raid && tank_threat >= kRaidTankThreatFloor ? kThreatHardRatioCasterRaid : kThreatHardRatioCaster);
}

// A hold that starts at the tank's own threat is too late while the tank holds only a few thousand: the next cast is already in flight and
// is a large share of it (n14 Onyxia: tank 1586, a Frostbolt about 700, three casts took a mage to 132 % before the first poll could hold it).
// The hold therefore starts, and is not released, at the ratio from which one more typical hit still leaves the bot under the ratio that
// takes the boss (130 % ranged, 110 % melee, less a margin). That ratio reaches the normal limits once the tank holds a few thousand: a
// bound by tank threat alone froze casters for 20 s on Geddon, whose tank stands at 6-7k for half a minute (n17 mc_geddon).
constexpr float kNextHitThreatRanged = 1000.0f;   // a crit Frostbolt or Fireball
constexpr float kNextHitThreatMelee  = 350.0f;
constexpr float kPullSafeRanged      = 1.25f;
constexpr float kPullSafeMelee       = 1.05f;
constexpr float kMinEarlyHoldRatio   = 0.4f;

// The ratio of the tank's threat a non-tank holds at: <base>, or lower while one typical hit would take the bot past kPullSafe* (never
// under kMinEarlyHoldRatio). Unknown tank threat leaves <base>.
// <share> is what stays of the tank's threat after the boss's scheduled cut (1 when it has none): the pull-safe ratio is measured against the
// tank as it will be after the cut (n17 Onyxia: a crit took a mage held at 75 % of the tank to 105 % of it, 140 % of the tank after the cut).
constexpr float ThreatEarlyHoldRatio(float base, bool melee, float tank_threat, float share = 1.0f)
{
    if (tank_threat <= 0.0f)
        return base;
    float const r = (melee ? kPullSafeMelee : kPullSafeRanged) * share - (melee ? kNextHitThreatMelee : kNextHitThreatRanged) / tank_threat;
    float const floor = kMinEarlyHoldRatio < base ? kMinEarlyHoldRatio : base;
    float const lowered = r < base ? r : base;
    return lowered > floor ? lowered : floor;
}

}  // namespace Playerbot
