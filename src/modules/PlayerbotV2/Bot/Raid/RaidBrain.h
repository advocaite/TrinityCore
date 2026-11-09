// RaidBrain — encounter rules -> per-bot raid orders.
//
// Port of Karrz's raid10-revamp ike3 RaidBrain (movement) and DecideRaidCast
// (taunt swaps, dispels, purges, crowd control) onto PlayerbotV2.
//
// Threading contract (identical to PveGroupCoordinator, which runs just
// before it): Update() is the only WRITER of orders_ and runs on the WORLD
// THREAD, inline in OnWorldUpdate BEFORE the parallel snapshot build.
// OrderFor() is a const find() READER called from the snapshot-build worker
// threads, which never overlap Update(). Update() reads live Player / Unit
// state, so it must never move off the world thread.
//
// One plan per (group, raid map): elections (who taunts, who dispels whom,
// who banishes which add) are made once for the whole group and handed out,
// so no two bots take the same job — the ike3 version reached the same
// answer on every bot by sorting members by guid; here the world thread
// just decides once.

#pragma once

#include "RaidOrder.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Playerbot::Raid {

class Brain
{
public:
    // World tick driver; internally throttled to the plan cadence.
    void Update(uint32_t now_ms);

    // Order lookup for the snapshot builder (nullptr = no plan covers the bot).
    RaidOrder const* OrderFor(uint64_t bot_guid_low) const
    {
        auto it = orders_.find(bot_guid_low);
        return it == orders_.end() ? nullptr : &it->second;
    }

    // The group kill focus an encounter rule demands (a Core Hound pack: its healthiest hound), or Empty. Read by the PvE
    // coordinator on the world thread before its own generic pick, so the rule wins over "lowest health first".
    ObjectGuid FocusFor(uint64_t group_low, uint32_t map_id) const
    {
        auto it = focus_.find(FocusKey(group_low, map_id));
        return it == focus_.end() ? ObjectGuid::Empty : it->second;
    }

    // The kill focus the PvE coordinator settled on (marked, tank-held or loose pick; Empty = none). The crowd-control election reads it:
    // the mob the raid is burning is never banished (MC Garr 2026-10-10: the focus Firesworn was banished under 20 DPS).
    void NoteKillFocus(uint64_t group_low, uint32_t map_id, ObjectGuid g)
    {
        if (g.IsEmpty()) coordFocus_.erase(FocusKey(group_low, map_id));
        else coordFocus_[FocusKey(group_low, map_id)] = g;
    }
    ObjectGuid CoordinatorFocus(uint64_t group_low, uint32_t map_id) const
    {
        auto it = coordFocus_.find(FocusKey(group_low, map_id));
        return it == coordFocus_.end() ? ObjectGuid::Empty : it->second;
    }

    static uint64_t FocusKey(uint64_t group_low, uint32_t map_id) { return (group_low << 16) ^ uint64_t(map_id); }

    // Spell schools per engaged enemy, read live from the creature (SpellSchoolMask bits: 1 physical, 2 holy, 4 fire,
    // 8 nature, 16 frost, 32 shadow, 64 arcane). prefer: it takes at least 50% extra damage from the school (the Death
    // Talons' random Elemental Shield vulnerability). avoid: it is immune to the school (the Plague Slime's colour, a
    // gargoyle's Stoneskin), takes at most half damage from it, or reflects it (an Anubisath Sentinel's reflect). Only enemies with a non-zero mask are listed. The PvE
    // coordinator copies them into each bot's PveOrder school hints. nullptr = no plan or nothing to say.
    // reflect_mask: the part of avoid_mask the enemy reflects (an area spell must not touch it; immunity or resistance only wastes that hit).
    struct EnemySchools { ObjectGuid target; uint8_t prefer_mask = 0; uint8_t avoid_mask = 0; uint8_t reflect_mask = 0; };
    std::vector<EnemySchools> const* SchoolsFor(uint64_t group_low, uint32_t map_id) const
    {
        auto it = schools_.find(FocusKey(group_low, map_id));
        return it == schools_.end() ? nullptr : &it->second;
    }

    // True when the enemy was immune to (or reflecting) every damage school in the brain's last plan (a banished mob, a
    // bubble): nothing the raid does hurts it, so it is no kill focus while anything else is engaged.
    static constexpr uint8_t kAllSchools = 0x7F;
    bool AllSchoolsAvoided(uint64_t group_low, uint32_t map_id, ObjectGuid g) const
    {
        if (std::vector<EnemySchools> const* v = SchoolsFor(group_low, map_id))
            for (EnemySchools const& e : *v)
                if (e.target == g)
                    return (e.avoid_mask & kAllSchools) == kAllSchools;
        return false;
    }

    // Movement context per planned bot, for the generic movement rules (hazard step-out, LoS step, cone sidestep,
    // spacing): every hazard circle the raid rules put around this bot right now (ground effects, game objects, hazard
    // creatures, enemy auras, carriers, avoidEnemy rings, breath corridor samples), nearest edge first, whether or not
    // the bot stands in one; and the stayNear rule holding it close to an enemy (Anubisath Defender / Sentinel, Thane).
    // The PvE coordinator copies it into the bot's PveOrder. nullptr = the bot has no raid plan.
    struct MoveContext
    {
        struct Circle { float x = 0.f, y = 0.f, r = 0.f; };
        std::vector<Circle> circles;
        bool  stay_near = false;
        float stay_x = 0.f, stay_y = 0.f, stay_radius = 0.f;
    };
    MoveContext const* MoveContextFor(uint64_t bot_guid_low) const
    {
        auto it = move_ctx_.find(bot_guid_low);
        return it == move_ctx_.end() ? nullptr : &it->second;
    }

    // Human-readable plan summary (last plan's decisions).
    std::string const& DebugDump() const { return last_dump_; }

private:
    std::unordered_map<uint64_t, RaidOrder> orders_;
    std::unordered_map<uint64_t, ObjectGuid> focus_;   // FocusKey -> rule-demanded kill focus
    std::unordered_map<uint64_t, ObjectGuid> coordFocus_;   // FocusKey -> the coordinator's current kill focus (NoteKillFocus)
    std::unordered_map<uint64_t, std::vector<EnemySchools>> schools_;   // FocusKey -> enemies with school hints
    std::unordered_map<uint64_t, MoveContext> move_ctx_;                // bot guid low -> movement context
    uint32_t last_plan_ms_ = 0;
    std::string last_dump_;
};

// The single instance (owned here so the raid layer stays self-contained).
Brain& GetBrain();

} // namespace Playerbot::Raid
