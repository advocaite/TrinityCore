// LfgRoleGap.h: which places of a 5-man or bigger group are still free (5-man: 1 tank, 1 healer, 3 damage; 10-man: 1, 3, 6).
// Header only, no game types.
#pragma once

#include "Define.h"

#include <algorithm>
#include <span>

namespace Playerbot::V2
{

// Role bits as the group stores them (lfg::PLAYER_ROLE_*).
constexpr uint8 kLfgRoleTank   = 0x02;
constexpr uint8 kLfgRoleHealer = 0x04;
constexpr uint8 kLfgRoleDamage = 0x08;

struct LfgRoleGap
{
    uint8 tank = 0, healer = 0, dps = 0;
    uint8 Total() const { return uint8(tank + healer + dps); }
    bool operator==(LfgRoleGap const&) const = default;
};

constexpr uint32 kLfgMinPartySize = 5;
constexpr uint32 kLfgMaxPartySize = 40;

// The role mix of an empty group of `size` places: about 1 tank per 10 players and 1 healer per 3, the rest damage.
// 5 -> 1 tank, 1 healer, 3 damage; 10 -> 1, 3, 6; 20 -> 2, 6, 12; 40 -> 4, 13, 23.
inline LfgRoleGap LfgRoleMix(uint32 size)
{
    size = size < kLfgMinPartySize ? kLfgMinPartySize : (size > kLfgMaxPartySize ? kLfgMaxPartySize : size);
    uint32 const tank = size / 10 ? size / 10 : 1;
    uint32 const healer = size / 3 ? size / 3 : 1;
    return LfgRoleGap{ uint8(tank), uint8(healer), uint8(size - tank - healer) };
}

// The party size a listing of an activity is filled to: an explicit size of the activity (Playerbot.Lfg.ActivityMaps) wins; a raid
// is as big as its map says (20, 40); any other dungeon is a 5-man. The Map table's MaxPlayers is NOT used for a dungeon: it says
// 10 for most classic dungeons (Deadmines, Uldaman, Scarlet Monastery) that are 5-mans.
inline uint32 LfgActivityPartySize(uint32 explicitSize, bool raidMap, uint32 mapMaxPlayers)
{
    uint32 size = kLfgMinPartySize;
    if (explicitSize)
        size = explicitSize;
    else if (raidMap)
        size = mapMaxPlayers;
    return size < kLfgMinPartySize ? kLfgMinPartySize : (size > kLfgMaxPartySize ? kLfgMaxPartySize : size);
}

// How long the fill of a group a player joined (a listed bot group) is spread over: the configured wait, but never more than a
// minute: the bots are already there, only the open roles are missing.
inline uint32 LfgJoinedFillSeconds(uint32 configuredSeconds)
{
    return configuredSeconds < 60 ? configuredSeconds : 60;
}

// How the places of a group of `size` are taken and what it still needs. `gap` is what the bots must bring; `dropDamage` is how many
// damage dealers past the damage places must leave so the tank and healer places can be filled (a full group of ten without a tank
// gives up a damage dealer for one).
struct LfgSeatPlan
{
    LfgRoleGap gap{};
    uint8 dropDamage = 0;
};

// `memberRoles` holds one role mask per member (0 = unknown: counts as damage). A member takes the first of tank, healer, damage
// from their mask whose place is still free; a tank or healer whose own places are taken can deal damage in a free damage place.
// A damage dealer never takes a tank or healer place: three healers and seven damage dealers in a ten-man group still lack a tank
// (Blackrock Spire, 2026-11-08: the seventh damage dealer took the tank place and the fill thought the group was complete). The
// tank and healer places come before the damage places: when the group has no room left for them, damage dealers past the damage
// places are to make room (dropDamage), and the damage places get only the room that is left. A place no room can be made for is
// not asked for.
inline LfgSeatPlan PlanLfgSeats(std::span<uint8 const> memberRoles, uint32 size = kLfgMinPartySize)
{
    LfgSeatPlan plan;
    LfgRoleGap& gap = plan.gap;
    gap = LfgRoleMix(size);
    uint32 const places = uint32(gap.Total());
    uint32 surplusDamage = 0;
    for (uint8 mask : memberRoles)
    {
        if (!mask)
            mask = kLfgRoleDamage;
        if ((mask & kLfgRoleTank) && gap.tank)          --gap.tank;
        else if ((mask & kLfgRoleHealer) && gap.healer) --gap.healer;
        else if (gap.dps)                               --gap.dps;
        else if (!(mask & (kLfgRoleTank | kLfgRoleHealer)))
            ++surplusDamage;
    }
    uint32 const members = uint32(memberRoles.size());
    uint32 const freePlaces = places > members ? places - members : 0;
    uint32 const needed = uint32(gap.tank) + gap.healer;
    if (needed > freePlaces)
    {
        plan.dropDamage = uint8(std::min<uint32>(needed - freePlaces, surplusDamage));
        gap.dps = 0;
        // no more room can be made (the extra members are real players, or tanks and healers): the tank comes before a healer
        uint32 room = freePlaces + plan.dropDamage;
        uint32 const tank = std::min<uint32>(gap.tank, room);
        room -= tank;
        gap.tank = uint8(tank);
        gap.healer = uint8(std::min<uint32>(gap.healer, room));
    }
    else
        gap.dps = uint8(std::min<uint32>(gap.dps, freePlaces - needed));
    return plan;
}

// The role bits a class can play (class ids as the game numbers them: 1 warrior, 2 paladin, 3 hunter, 4 rogue, 5 priest, 6 death
// knight, 7 shaman, 8 mage, 9 warlock, 10 monk, 11 druid, 12 demon hunter, 13 evoker). Every class can deal damage.
inline uint8 LfgClassRoles(uint8 cls)
{
    switch (cls)
    {
        case 1: case 6: case 12:  return kLfgRoleTank | kLfgRoleDamage;
        case 2: case 10: case 11: return kLfgRoleTank | kLfgRoleHealer | kLfgRoleDamage;
        case 5: case 7: case 13:  return kLfgRoleHealer | kLfgRoleDamage;
        default:                  return kLfgRoleDamage;
    }
}

// A member's role mask with the roles their class cannot play taken out (0 when none is left: the caller then uses the class's own
// role). A rogue who picked Tank in the finder is not a tank: Scarlet Monastery, 2026-11-08, the rogue leader counted as the tank and
// the bots brought a healer and three damage dealers, no tank.
inline uint8 LfgRoleForClass(uint8 cls, uint8 mask)
{
    return uint8(mask & (kLfgRoleTank | kLfgRoleHealer | kLfgRoleDamage) & LfgClassRoles(cls));
}

// The places a group of `size` (default a 5-man party) still lacks after its members have taken theirs (PlanLfgSeats).
inline LfgRoleGap ComputeLfgRoleGap(std::span<uint8 const> memberRoles, uint32 size = kLfgMinPartySize)
{
    return PlanLfgSeats(memberRoles, size).gap;
}

// What a pending Group Finder listing does at a check. `msLeft` is the time left of the wait (<= 0 when it is over),
// `realInRange` the real (non-bot) players online in the leader's level range who could still join. With
// Lfg.InstantFillIfNobody nobody to wait for means the bots come at once; otherwise they come when the wait is over.
enum class LfgFillAction : uint8 { Wait, FillNow, FillAfterWait };

inline LfgFillAction DecideLfgFill(int32 msLeft, uint32 realInRange, bool instantFillIfNobody)
{
    if (instantFillIfNobody && realInRange == 0)
        return LfgFillAction::FillNow;
    return msLeft <= 0 ? LfgFillAction::FillAfterWait : LfgFillAction::Wait;
}
} // namespace Playerbot::V2
