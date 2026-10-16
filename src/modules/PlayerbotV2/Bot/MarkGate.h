#pragma once
// Playerbot.Behavior.MarksNeedOrder: raid icons only PLAN. A mark on a mob nobody fights yet (or an icon on a bot, an
// rti list) never starts the fight by itself: the owner's order does (attack / engage, pull, pull back, pull rti, /focus
// - PveGroupCoordinator latches that as PveOrder::marks_released for the whole group). A marked mob already in combat
// with the group is handled as before. With the key off every check passes.
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "Services.h"
#include "Util/ConfigReader.h"
#include "Log.h"

#include <mutex>
#include <unordered_map>

namespace Playerbot::MarkGate {

inline bool Enabled() { return Services::Config().behavior_marks_need_order(); }

inline char const* SymbolName(uint8 sym)
{
    static char const* const names[8] = { "star", "circle", "diamond", "triangle", "moon", "square", "cross", "skull" };
    return sym < 8 ? names[sym] : "?";
}

// Icon (0..7) the group carries on `mob`, 0xFF none.
inline uint8 SymbolOf(GroupSnapshotView const& g, ObjectGuid mob)
{
    if (mob.IsEmpty() || !g.exists()) return 0xFF;
    for (uint8 i = 0; i < 8; ++i)
        if (g.raid_mark(i) == mob) return i;
    return 0xFF;
}

// The owner has released the marks for this bot: its own order names the mob, or the group's order latch is on.
inline bool Released(BotSnapshotView const& s, BotAI const& ai, ObjectGuid mob)
{
    if (!Enabled()) return true;
    if (s.raw().pve_order.marks_released) return true;
    if (!mob.IsEmpty() && ai.focus_target() == mob) return true;
    return ai.is_manual_mode(s.published_at_ms()) && ai.owner_command() == BotAI::OwnerCommand::Engage &&
           (mob.IsEmpty() || ai.owner_target() == mob);
}

// May this bot start (or crowd-control, or pivot onto) `mob`, a mob carrying a raid mark? True when the mob is already
// in combat / has a victim (defensive reaction, the unit as the bot sees it) or the owner released the marks. When it
// is held, logs once per 5 s per bot. `unit` null = the mob is looked up in the snapshot (unknown = not engaged).
inline bool MayEngage(BotSnapshotView const& s, BotAI const& ai, GroupSnapshotView const* g, ObjectGuid mob,
                      NearbyUnit const* unit = nullptr)
{
    if (!Enabled()) return true;
    if (!unit)
    {
        for (auto const& u : s.raw().combat.nearby_enemies)
            if (u.guid == mob) { unit = &u; break; }
        if (!unit)
            for (auto const& u : s.raw().combat.attackers)
                if (u.guid == mob) { unit = &u; break; }
    }
    if (unit && (unit->unit_in_combat || !unit->victim.IsEmpty())) return true;
    // A bot already fighting opens an idle marked mob (the next pack's skull or moon the owner marks mid-fight) only on its own order
    // for that mob, never on the group latch: the latch (any Attack, a mob on a non-tank, the DPS wait timeout) runs for the whole fight,
    // so a mark put on the next pack pulled it into this one.
    if (s.in_combat())
    {
        if (!mob.IsEmpty() && (ai.focus_target() == mob ||
            (ai.is_manual_mode(s.published_at_ms()) && ai.owner_command() == BotAI::OwnerCommand::Engage && ai.owner_target() == mob)))
            return true;
    }
    else if (Released(s, ai, mob))
        return true;

    static std::mutex mx;
    static std::unordered_map<uint64, uint32> last_ms;
    uint32 const now = s.published_at_ms();
    uint64 const key = s.raw().guid.GetCounter();
    {
        std::lock_guard<std::mutex> lk(mx);
        auto [it, ins] = last_ms.try_emplace(key, now);
        if (!ins && now - it->second < 5000u) return false;
        it->second = now;
    }
    uint8 const sym = g ? SymbolOf(*g, mob) : uint8(0xFF);
    TC_LOG_INFO("playerbot.v2", "[mark_gate] {} held: mark {} on {} awaiting owner order", s.name(), SymbolName(sym),
                unit ? std::to_string(unit->entry) : mob.ToString());
    return false;
}

} // namespace Playerbot::MarkGate
