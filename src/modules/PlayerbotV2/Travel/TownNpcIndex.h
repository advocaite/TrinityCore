// TownNpcIndex - every service NPC spawn in the open world (vendors, trainers,
// innkeepers, bankers, auctioneers, flight/stable masters, battlemasters,
// guild petitioners and tabard designers), so a world bot can find the NPCs of
// the town it stands in, or the nearest city with an auction house or bank.
//
// Drives the RPG / idle-life rules (Bot/States/Rules/RpgRules.cpp), the port
// of Karrz's ike3 "rpg" strategy: bots stroll between the NPCs of a town,
// talk to them, sell junk and repair while there, and now and then go and hang
// around in a city. The snapshot builder also asks it whether an auctioneer
// spawn is close, which gates the auction-house price scan.
//
// Only vanilla service flags are indexed, and only spawns on open-world maps
// (the Classic continents). Each spawn records whether Alliance and Horde
// players may use it, decided once at boot from its faction template against
// the Human and Orc player factions — a goblin town (Booty Bay, Gadgetzan,
// Everlook, Ratchet) is open to both, a faction capital to its own side only.
//
// THREADING: Initialize() runs once on the world thread at module init, before
// any bot worker ticks, and publishes the immutable per-map grids with a release
// store. Every query is a pure reader (acquire fence, no locks, no state).

#pragma once

#include "Define.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace Playerbot::V2::Travel {

struct TownNpc
{
    uint64 spawn_id     = 0;   // creature.guid — stable key for "visited lately" memory
    uint32 entry        = 0;
    uint32 map_id       = 0;
    float  x = 0.f, y = 0.f, z = 0.f;
    uint32 npc_flags    = 0;   // vanilla service bits only (see kIndexedFlags)
    uint8  trainer_class = 0;  // creature_template.trainer_class (class trainers)
    bool   alliance_ok  = false;
    bool   horde_ok     = false;

    bool usable_by(bool alliance) const { return alliance ? alliance_ok : horde_ok; }
};

class TownNpcIndex
{
public:
    static TownNpcIndex& Instance();

    // World thread, once, after ObjectMgr has loaded creature spawns.
    void Initialize();
    bool IsInitialized() const { return _initialized.load(std::memory_order_acquire); }
    size_t Size() const { return IsInitialized() ? _count : 0; }

    // Calls fn for every indexed spawn on `map` within `radius` (2D) of (x,y).
    void ForEachNear(uint32 map, float x, float y, float radius,
                     std::function<void(TownNpc const&)> const& fn) const;

    // Number of distinct spawns usable by the side within `radius` that carry
    // any of `flags`. A town is where several such NPCs stand together.
    uint32 CountNear(uint32 map, float x, float y, float radius, bool alliance, uint32 flags) const;

    // Nearest spawn on `map` (2D, then a same-floor check) carrying any of
    // `flags` and usable by the side, no farther than `max_dist`. Null if none.
    TownNpc const* Nearest(uint32 map, float x, float y, float z, uint32 flags,
                           bool alliance, float max_dist) const;

    // Nearest mailbox object on `map` within `max_dist` (2D, same floor). Mailboxes are kept apart from the service NPCs so they never
    // count as a town's NPCs.
    TownNpc const* NearestMailbox(uint32 map, float x, float y, float z, float max_dist) const;

    // True when any auctioneer spawn (either side) stands within `radius`.
    // Cheap gate for the snapshot builder's auction-house scan.
    bool AuctioneerNear(uint32 map, float x, float y, float radius) const;

private:
    static constexpr float kCell = 128.0f;
    static uint64 CellKey(int32 cx, int32 cy) { return (uint64(uint32(cx)) << 32) | uint32(cy); }

    struct MapGrid
    {
        std::vector<TownNpc> spawns;
        std::unordered_map<uint64, std::vector<uint32>> cells;   // cell -> indices into spawns
    };

    std::unordered_map<uint32, MapGrid> _maps;
    std::unordered_map<uint32, std::vector<TownNpc>> _mail;   // map -> mailbox object spawns
    size_t _count = 0;
    std::atomic<bool> _initialized{false};
};

} // namespace Playerbot::V2::Travel
