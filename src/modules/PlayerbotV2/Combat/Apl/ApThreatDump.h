#pragma once
// Threat-dump trigger for DPS and healers: how close the bot is to pulling aggro off the tank.
//
// Idea from Karrz's VMaNGOS raid layer (Karrz1/wow-1.12, playerbot/strategy/values/ThreatValues.cpp relative threat %,
// GenericTriggers.cpp HighThreat / MediumThreat at 80% / 60%); rewritten against our snapshot (NearbyUnit::my_threat and
// tank_threat, filled by the snapshot builder). Used to time Fade and Feint.

#include "Bot/BotSnapshotView.h"
#include <algorithm>

namespace Playerbot::Combat::ThreatDump {

constexpr int32 kMediumThreatPct = 60;   // VM MediumThreatTrigger
constexpr int32 kHighThreatPct   = 80;   // VM HighThreatTrigger

// Highest share (percent) of the group tank's threat the bot holds on any mob that is fighting and whose victim is not the
// bot already. 0 when no tank holds threat on a mob or nothing qualifies.
inline int32 ThreatPct(BotSnapshotView const& s)
{
    int32 best = 0;
    for (auto const& u : s.raw().combat.nearby_enemies)
    {
        if (u.hp <= 0 || !u.has_threat_list || !u.unit_in_combat || u.tank_threat <= 0.f || u.my_threat <= 0.f)
            continue;
        if (u.victim_kind == uint8(ThreatVictim::Me))
            continue;   // already the target: a dump now is a taunt problem for the tank, not a threat one
        best = std::max(best, int32(u.my_threat * 100.f / u.tank_threat));
    }
    return best;
}

} // namespace Playerbot::Combat::ThreatDump
