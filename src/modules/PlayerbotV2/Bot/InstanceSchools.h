// InstanceSchools - which schools of spell damage a raid instance's bosses deal.
//
// Resistance buffs (Shadow Protection, the paladin resistance auras, the shaman totems) are kept up only for a school the instance
// deals. MC 2026-10-11 (the owner: priests standing with hands glowing): five priests spent a refresh round of 40 Shadow Protection casts
// in Molten Core, where the raid takes fire. Data, not per-spell rules: a new instance or school is one row. Pure: tested in
// tests/game/ClassicInstanceSchools.cpp.

#pragma once

#include <cstdint>
#include <span>

namespace Playerbot::Classic {

enum class Resist : std::uint8_t { None, Fire, Frost, Shadow, Nature };

// A map whose bosses deal `school`. The first row of a map is its main school (InstanceResistance: the paladin aura, the totems).
struct InstanceSchool { std::uint32_t map; Resist school; };

inline constexpr InstanceSchool kInstanceSchools[] = {
    { 409, Resist::Fire },     // Molten Core
    { 469, Resist::Fire },     // Blackwing Lair
    { 249, Resist::Fire },     // Onyxia's Lair
    { 533, Resist::Frost },    // Naxxramas (Sapphiron, Kel'Thuzad)
    { 533, Resist::Shadow },   //   Loatheb, the Four Horsemen
    { 531, Resist::Nature },   // Temple of Ahn'Qiraj (Huhuran, Viscidus)
};

// The main school of an instance, None when it is not in the table.
constexpr Resist InstanceMainSchool(std::span<InstanceSchool const> table, std::uint32_t map_id)
{
    for (InstanceSchool const& e : table)
        if (e.map == map_id)
            return e.school;
    return Resist::None;
}

// Do the bosses of `map_id` deal `school`? None (a buff that is not a resistance) is always wanted. A map not in the table (a dungeon,
// the open world) deals none, so a resistance buff is not kept up there.
constexpr bool InstanceDealsSchool(std::span<InstanceSchool const> table, std::uint32_t map_id, Resist school)
{
    if (school == Resist::None)
        return true;
    for (InstanceSchool const& e : table)
        if (e.map == map_id && e.school == school)
            return true;
    return false;
}

} // namespace Playerbot::Classic
