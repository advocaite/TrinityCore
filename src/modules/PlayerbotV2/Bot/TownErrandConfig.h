// The Playerbot.World.Town.* keys (TownErrand.h, the town shows in AmbientRules.cpp, the town walk in ChunkedWalkToward and the
// capital idle watch). Read once, at the first use.
#pragma once

#include "Config.h"

#include <algorithm>
#include <cstdint>

namespace Playerbot::V2::TownErrand
{

struct Config
{
    bool          errands = true;          // Errands: capital residents run from place to place
    float         radius = 300.0f;         // ErrandRadius: how far across the city the next place may be
    std::uint32_t linger_min_ms = 4000;    // LingerMinSec / LingerMaxSec: the stop at a place
    std::uint32_t linger_max_ms = 12000;
    std::uint32_t spot_cap = 5;            // SpotCap: bots at one place (a bank, an auction house, a trainer) at once
    std::uint32_t inn_cap = 4;             // InnCap: bots resting at one inn at once
    std::uint32_t mailbox_cap = 2;         // MailboxCap: bots at one mailbox at once
    std::uint32_t inn_share_pct = 50;      // InnSharePct: the inn's share of the bots (TownLife::InnPercent) scaled by this
    bool          exits = true;            // Exits: the city gate is one of the places, when the gate-duel driver has found it
    bool          smooth_walk = true;      // SmoothWalk: in a capital a walk of up to SmoothWalkYards is one pathed move, not 35 y chunks
    float         smooth_walk_yards = 200.0f;   // a navmesh route of up to 60 points (about 240 y) goes out as one spline
    float         nudge_yards = 5.0f;      // PlayerNudgeYards / PlayerNudgeSec: a standing bot a real player stands next to moves on
    std::uint32_t nudge_ms = 6000;
    std::uint32_t idle_ms = 20000;         // IdleSec: a bot in a capital standing this long with no reason is given something to do
    std::uint32_t field_idle_ms = 30000;   // Playerbot.World.IdleSec: the same anywhere else (the idle watch, Bot/IdleWatch.h)
};

inline Config const& Cfg()
{
    static Config const c = []
    {
        Config k;
        k.errands           = sConfigMgr->GetBoolDefault("Playerbot.World.Town.Errands", true, true);
        k.radius            = float(std::clamp<int>(sConfigMgr->GetIntDefault("Playerbot.World.Town.ErrandRadius", 300, true), 60, 1000));
        k.linger_min_ms     = std::uint32_t(std::clamp<int>(sConfigMgr->GetIntDefault("Playerbot.World.Town.LingerMinSec", 4, true), 0, 300)) * 1000u;
        k.linger_max_ms     = std::uint32_t(std::clamp<int>(sConfigMgr->GetIntDefault("Playerbot.World.Town.LingerMaxSec", 12, true), 0, 600)) * 1000u;
        k.linger_max_ms     = std::max(k.linger_max_ms, k.linger_min_ms);
        k.spot_cap          = std::uint32_t(std::clamp<int>(sConfigMgr->GetIntDefault("Playerbot.World.Town.SpotCap", 5, true), 1, 100));
        k.inn_cap           = std::uint32_t(std::clamp<int>(sConfigMgr->GetIntDefault("Playerbot.World.Town.InnCap", 4, true), 0, 100));
        k.mailbox_cap       = std::uint32_t(std::clamp<int>(sConfigMgr->GetIntDefault("Playerbot.World.Town.MailboxCap", 2, true), 0, 100));
        k.inn_share_pct     = std::uint32_t(std::clamp<int>(sConfigMgr->GetIntDefault("Playerbot.World.Town.InnSharePct", 50, true), 0, 100));
        k.exits             = sConfigMgr->GetBoolDefault("Playerbot.World.Town.Exits", true, true);
        k.smooth_walk       = sConfigMgr->GetBoolDefault("Playerbot.World.Town.SmoothWalk", true, true);
        k.smooth_walk_yards = float(std::clamp<int>(sConfigMgr->GetIntDefault("Playerbot.World.Town.SmoothWalkYards", 200, true), 60, 240));
        k.nudge_yards       = float(std::clamp<int>(sConfigMgr->GetIntDefault("Playerbot.World.Town.PlayerNudgeYards", 5, true), 0, 30));
        k.nudge_ms          = std::uint32_t(std::clamp<int>(sConfigMgr->GetIntDefault("Playerbot.World.Town.PlayerNudgeSec", 6, true), 1, 600)) * 1000u;
        k.idle_ms           = std::uint32_t(std::clamp<int>(sConfigMgr->GetIntDefault("Playerbot.World.Town.IdleSec", 20, true), 5, 600)) * 1000u;
        k.field_idle_ms     = std::uint32_t(std::clamp<int>(sConfigMgr->GetIntDefault("Playerbot.World.IdleSec", 30, true), 5, 600)) * 1000u;
        return k;
    }();
    return c;
}

} // namespace Playerbot::V2::TownErrand
