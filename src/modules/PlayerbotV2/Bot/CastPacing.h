// CastPacing.h - per-bot cast-speed context and multiplier (Playerbot.Cast.*).
//
// Pure functions over a snapshot view and the config: no Player access, safe on the bot logic
// threads. PlayerbotV2.cpp publishes the result core-side (API::set_cast_speed_multiplier)
// and BotSnapshotView scales its in-flight cast caps by the same number.
#pragma once

#include "BotSnapshotView.h"
#include "../Services.h"
#include "../Util/ConfigReader.h"
#include <algorithm>

namespace Playerbot {

enum class CastContext : uint8
{
    World = 0, Dungeon, Raid, WorldPvp, Battleground
};

inline char const* CastContextName(CastContext c)
{
    switch (c)
    {
        case CastContext::Battleground: return "bg";
        case CastContext::WorldPvp:     return "world_pvp";
        case CastContext::Raid:         return "raid";
        case CastContext::Dungeon:      return "dungeon";
        default:                        return "world";
    }
}

// Battleground (incl. arena) > WorldPvp > Raid > Dungeon > World.
// WorldPvp = in combat outside a BG and either under attack by a hostile player or
// fighting one (the auto-attack victim is a player).
inline CastContext ClassifyCastContext(BotSnapshotView const& v)
{
    if (v.in_battleground())
        return CastContext::Battleground;
    if (v.in_combat())
    {
        if (v.under_player_attack())
            return CastContext::WorldPvp;
        if (NearbyUnit const* victim = v.victim_info(); victim && victim->is_player)
            return CastContext::WorldPvp;
    }
    if (v.is_in_raid())
        return CastContext::Raid;
    if (v.is_in_dungeon())
        return CastContext::Dungeon;
    return CastContext::World;
}

// Constant per-bot skill offset in percent points, in [-variance, +variance], from the guid
// hash (no RNG: the same bot is always the same speed, across restarts).
inline int32 CastSkillOffsetPct(ObjectGuid guid, uint32 variance_pct)
{
    if (!variance_pct)
        return 0;
    uint32 const h = (uint32(guid.GetCounter()) * 2654435761u) ^ 0x9E3779B9u;
    uint32 const x = (h >> 8) * 2246822519u;
    return int32((x >> 4) % (2 * variance_pct + 1)) - int32(variance_pct);
}

inline uint32 CastContextPct(ConfigReader::CastPacing const& c, CastContext ctx)
{
    switch (ctx)
    {
        case CastContext::Battleground: return c.bg_pct;
        case CastContext::WorldPvp:     return c.world_pvp_pct;
        case CastContext::Raid:         return c.raid_pct;
        case CastContext::Dungeon:      return c.dungeon_pct;
        default:                        return c.world_pct;
    }
}

// Multiplier on cast time / GCD for this bot in this context; 1.0 when pacing is off.
inline float CastSpeedMultiplier(ConfigReader::CastPacing const& c, CastContext ctx, ObjectGuid guid)
{
    if (!c.enabled)
        return 1.0f;
    int32 pct = int32(CastContextPct(c, ctx)) + CastSkillOffsetPct(guid, c.variance_pct);
    pct = std::clamp(pct, int32(c.min_pct), int32(c.max_pct));
    return float(pct) / 100.0f;
}

} // namespace Playerbot
