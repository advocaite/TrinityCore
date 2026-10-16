#include "TownNpcIndex.h"

#include "CreatureData.h"
#include "GameObjectData.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "SpawnData.h"
#include "UnitDefines.h"

#include <chrono>
#include <cmath>
#include <limits>

namespace Playerbot::V2::Travel {

namespace {

// Vanilla service NPCs only. Account/guild bankers, transmogrifiers, void
// storage, black market and the other retail-only flags are left out on
// purpose: Classic has none of them.
constexpr uint32 kIndexedFlags =
      UNIT_NPC_FLAG_TRAINER | UNIT_NPC_FLAG_VENDOR_MASK | UNIT_NPC_FLAG_REPAIR
    | UNIT_NPC_FLAG_FLIGHTMASTER | UNIT_NPC_FLAG_INNKEEPER | UNIT_NPC_FLAG_BANKER
    | UNIT_NPC_FLAG_PETITIONER | UNIT_NPC_FLAG_TABARDDESIGNER | UNIT_NPC_FLAG_BATTLEMASTER
    | UNIT_NPC_FLAG_AUCTIONEER | UNIT_NPC_FLAG_STABLEMASTER;

// Faction templates of a Human and an Orc player: the reference for "may an
// Alliance / Horde player walk up to this NPC". Every race of a side shares
// the town relations that matter here.
constexpr uint32 kAllianceFactionTemplate = 1;
constexpr uint32 kHordeFactionTemplate    = 2;

bool Hostile(FactionTemplateEntry const* npc, FactionTemplateEntry const* player)
{
    if (!npc || !player)
        return false;
    return npc->IsHostileTo(player) || player->IsHostileTo(npc);
}

} // namespace

TownNpcIndex& TownNpcIndex::Instance()
{
    static TownNpcIndex instance;
    return instance;
}

void TownNpcIndex::Initialize()
{
    if (_initialized.load(std::memory_order_acquire))
        return;

    auto const t0 = std::chrono::steady_clock::now();
    FactionTemplateEntry const* alliance = sFactionTemplateStore.LookupEntry(kAllianceFactionTemplate);
    FactionTemplateEntry const* horde    = sFactionTemplateStore.LookupEntry(kHordeFactionTemplate);

    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
    {
        MapEntry const* map = sMapStore.LookupEntry(data.mapId);
        if (!map || !map->IsWorldMap())
            continue;
        // Phased spawns are invisible to a normal player; a manual-spawn group
        // is not in the world until a script adds it. Neither is a place to go.
        if (data.phaseId != 0 || data.phaseGroup != 0)
            continue;
        if (data.spawnGroupData && (data.spawnGroupData->flags & SPAWNGROUP_FLAG_MANUAL_SPAWN))
            continue;

        CreatureTemplate const* tmpl = sObjectMgr->GetCreatureTemplate(data.id);
        if (!tmpl)
            continue;
        const uint32 flags = uint32(data.npcflag.value_or(tmpl->npcflag)) & kIndexedFlags;
        if (!flags)
            continue;

        FactionTemplateEntry const* ft = sFactionTemplateStore.LookupEntry(tmpl->faction);
        TownNpc n;
        n.spawn_id      = spawnId;
        n.entry         = data.id;
        n.map_id        = data.mapId;
        n.x             = data.spawnPoint.GetPositionX();
        n.y             = data.spawnPoint.GetPositionY();
        n.z             = data.spawnPoint.GetPositionZ();
        n.npc_flags     = flags;
        n.trainer_class = uint8(tmpl->trainer_class);
        n.alliance_ok   = ft && !Hostile(ft, alliance);
        n.horde_ok      = ft && !Hostile(ft, horde);
        if (!n.alliance_ok && !n.horde_ok)
            continue;

        MapGrid& grid = _maps[n.map_id];
        const uint32 idx = uint32(grid.spawns.size());
        grid.spawns.push_back(n);
        const int32 cx = int32(std::floor(n.x / kCell));
        const int32 cy = int32(std::floor(n.y / kCell));
        grid.cells[CellKey(cx, cy)].push_back(idx);
        ++_count;
    }

    size_t mailboxes = 0;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllGameObjectData())
    {
        MapEntry const* map = sMapStore.LookupEntry(data.mapId);
        if (!map || !map->IsWorldMap() || data.phaseId != 0 || data.phaseGroup != 0)
            continue;
        if (data.spawnGroupData && (data.spawnGroupData->flags & SPAWNGROUP_FLAG_MANUAL_SPAWN))
            continue;
        GameObjectTemplate const* tmpl = sObjectMgr->GetGameObjectTemplate(data.id);
        if (!tmpl || tmpl->type != GAMEOBJECT_TYPE_MAILBOX)
            continue;
        TownNpc n;
        n.spawn_id    = spawnId;
        n.entry       = data.id;
        n.map_id      = data.mapId;
        n.x           = data.spawnPoint.GetPositionX();
        n.y           = data.spawnPoint.GetPositionY();
        n.z           = data.spawnPoint.GetPositionZ();
        n.alliance_ok = n.horde_ok = true;
        _mail[n.map_id].push_back(n);
        ++mailboxes;
    }

    _initialized.store(true, std::memory_order_release);
    TC_LOG_INFO("playerbot.v2", "[TownNpcIndex] {} mailbox objects.", mailboxes);
    TC_LOG_INFO("playerbot.v2", "[TownNpcIndex] {} service NPC spawns on {} open-world maps in {} ms.",
        _count, _maps.size(),
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count());
}

void TownNpcIndex::ForEachNear(uint32 map, float x, float y, float radius,
                               std::function<void(TownNpc const&)> const& fn) const
{
    if (!_initialized.load(std::memory_order_acquire))
        return;
    auto it = _maps.find(map);
    if (it == _maps.end())
        return;
    MapGrid const& grid = it->second;
    const int32 cx0 = int32(std::floor((x - radius) / kCell));
    const int32 cx1 = int32(std::floor((x + radius) / kCell));
    const int32 cy0 = int32(std::floor((y - radius) / kCell));
    const int32 cy1 = int32(std::floor((y + radius) / kCell));
    const float r2 = radius * radius;
    for (int32 cx = cx0; cx <= cx1; ++cx)
        for (int32 cy = cy0; cy <= cy1; ++cy)
        {
            auto c = grid.cells.find(CellKey(cx, cy));
            if (c == grid.cells.end())
                continue;
            for (uint32 idx : c->second)
            {
                TownNpc const& n = grid.spawns[idx];
                const float dx = n.x - x, dy = n.y - y;
                if (dx * dx + dy * dy <= r2)
                    fn(n);
            }
        }
}

uint32 TownNpcIndex::CountNear(uint32 map, float x, float y, float radius, bool alliance, uint32 flags) const
{
    uint32 count = 0;
    ForEachNear(map, x, y, radius, [&](TownNpc const& n)
    {
        if ((n.npc_flags & flags) && n.usable_by(alliance))
            ++count;
    });
    return count;
}

TownNpc const* TownNpcIndex::Nearest(uint32 map, float x, float y, float z, uint32 flags,
                                     bool alliance, float max_dist) const
{
    if (!_initialized.load(std::memory_order_acquire))
        return nullptr;
    auto it = _maps.find(map);
    if (it == _maps.end())
        return nullptr;
    TownNpc const* best = nullptr;
    float best_d2 = max_dist * max_dist;
    for (TownNpc const& n : it->second.spawns)
    {
        if (!(n.npc_flags & flags) || !n.usable_by(alliance))
            continue;
        const float dx = n.x - x, dy = n.y - y;
        const float d2 = dx * dx + dy * dy;
        if (d2 >= best_d2)
            continue;
        // A spawn straight above or below (another floor of a city, a cave
        // under the bot) is not the nearest one in walking terms.
        const float dz = std::fabs(n.z - z);
        if (dz > 30.0f && dz * dz > d2)
            continue;
        best = &n;
        best_d2 = d2;
    }
    return best;
}

TownNpc const* TownNpcIndex::NearestMailbox(uint32 map, float x, float y, float z, float max_dist) const
{
    if (!_initialized.load(std::memory_order_acquire))
        return nullptr;
    auto it = _mail.find(map);
    if (it == _mail.end())
        return nullptr;
    TownNpc const* best = nullptr;
    float best_d2 = max_dist * max_dist;
    for (TownNpc const& n : it->second)
    {
        const float dx = n.x - x, dy = n.y - y;
        const float d2 = dx * dx + dy * dy;
        if (d2 >= best_d2 || std::fabs(n.z - z) > 12.0f)
            continue;
        best = &n;
        best_d2 = d2;
    }
    return best;
}

bool TownNpcIndex::AuctioneerNear(uint32 map, float x, float y, float radius) const
{
    bool found = false;
    ForEachNear(map, x, y, radius, [&](TownNpc const& n)
    {
        if (n.npc_flags & UNIT_NPC_FLAG_AUCTIONEER)
            found = true;
    });
    return found;
}

} // namespace Playerbot::V2::Travel
