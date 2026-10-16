// BG victim validity: a battleground bot never keeps a target it cannot hurt, and re-picks when an enemy player is close.
//
// the owner, WSG 2026-10-09: a Horde priest stood next to him swinging at nothing and ignored him. The idle engage rule only ran with an empty
// victim, and the in-combat BG switches only react to a carrier, a peel or a healer, so a victim that was gone, dead, untargetable or
// not fighting back held the bot for good. Pure function over the snapshot's unit lists (templated, so tests/game/BgTargetPick.cpp can
// run it on plain structs). Used by State_Idle and State_InCombat; both log the result as [bg_target].
#pragma once

#include <cstdint>
#include <vector>

namespace Playerbot::BgTargetPick
{
    enum class Why : uint8_t
    {
        None,           // keep the victim
        Missing,        // the victim is not among the units around the bot (gone, out of range, not visible)
        Unfightable,    // dead, untargetable, pacified, or unreachable
        IdleMob,        // a creature that does not fight us while an enemy player is close
    };

    inline char const* WhyName(Why w)
    {
        switch (w)
        {
            case Why::Missing:     return "missing";
            case Why::Unfightable: return "unfightable";
            case Why::IdleMob:     return "idle_mob";
            default:               return "keep";
        }
    }

    // How far a battleground bot looks for an enemy to fight (playtest, WSG 2026-10-09: "make the radius where they attack people bigger").
    // The snapshot scans this far in a battleground (BotSnapshotBuilder), the engage and re-pick rules read it.
    constexpr float kBgEngageRange = 55.0f;
    constexpr float kIdleMobPlayerRange = 15.0f;   // an enemy player this close beats a creature that is not attacking us

    // `me` is this bot's guid. `enemies` / `attackers` are the snapshot's lists (victim lookup uses both, as victim_info() does).
    // Returns the guid to switch to (default-constructed guid when none) and why the victim was dropped.
    template <class Unit, class Guid>
    Guid Pick(Guid const& me, Guid const& victim, float bx, float by, bool ranged, float range,
              std::vector<Unit> const& enemies, std::vector<Unit> const& attackers, Why& why)
    {
        why = Why::None;
        Guid const none{};
        if (victim == none)
            return none;

        Unit const* v = nullptr;
        for (Unit const& u : attackers)
            if (u.guid == victim) { v = &u; break; }
        if (!v)
            for (Unit const& u : enemies)
                if (u.guid == victim) { v = &u; break; }

        auto dist2 = [&](Unit const& u) { float const dx = u.x - bx, dy = u.y - by; return dx * dx + dy * dy; };
        auto fightable = [](Unit const& u) { return u.hp > 0 && !u.untargetable && !u.is_pacified && !u.cannot_reach; };

        if (!v) why = Why::Missing;
        else if (!fightable(*v)) why = Why::Unfightable;

        // The nearest live enemy player, in line of sight for ranged bots and not locked down by a friendly CC.
        Unit const* best = nullptr;
        float best_d = range * range;
        for (Unit const& u : enemies)
        {
            if (!u.is_player || !fightable(u) || u.is_cc_locked || u.guid == victim) continue;
            if (ranged && !u.in_los) continue;
            float const d = dist2(u);
            if (d < best_d) { best_d = d; best = &u; }
        }

        if (why == Why::None && v && !v->is_player && !(v->victim == me) && best &&
            best_d <= kIdleMobPlayerRange * kIdleMobPlayerRange)
            why = Why::IdleMob;

        if (why == Why::None || !best)
            return none;
        return best->guid;
    }
}
