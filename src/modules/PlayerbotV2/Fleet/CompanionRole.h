// CompanionRole - which role a companion plays, and whether that choice is remembered: the pure rule behind
// BotPopulationManager::ApplyCompanionRole.
//
// Only a role somebody CHOSE is stored with the companion (and pinned on the bot logic and the virtual spec): the one the owner
// asked for (`.bot add <class> healer`, /setrole, the raid quota fill) or the one already stored. A role INFERRED from the
// talents is a guess about a build that may not exist yet, so it is used for now and never stored: the login and the 5-minute
// upkeep read the spec before the setup step has spent the talent points, so the guess was "dps" for a healer, was stored for
// good, and the talent walk then followed the stored role (playtest, Stormwind 2026-10-09: 285 stored rows, healer priests as dps,
// paladins and druids split over tank and dps). Left unset, a companion is still free for the raid quota fill, which is what
// RaidBrain's role_set test relies on.
// Pure: no game types, tested in tests/game/ClassicCompanionRole.cpp.

#pragma once

#include "../Bot/BotTypes.h"

namespace Playerbot::V2::Fleet {

enum class CompanionRoleSource : uint8
{
    Declared,    // asked for now, or stored with the companion
    Spec,        // read from the talents: not remembered
    SpecHybrid,  // read from the talents of a hybrid class that came out as tank: it heals until the quota fill says otherwise
};

struct CompanionRoleChoice
{
    ::Playerbot::Role   role = ::Playerbot::Role::Dps;
    CompanionRoleSource source = CompanionRoleSource::Spec;
    bool                pin = false;    // set the bot role override and the virtual spec
    bool                store = false;  // keep the role with the companion (only a chosen role)
};

// requested: what the owner asked for now (Unknown = nothing). declared: the stored role (Unknown = none).
// inferred: what the talents say (only read when neither of the others is known). hybrid: the class is a paladin or druid.
inline CompanionRoleChoice ChooseCompanionRole(::Playerbot::Role requested, ::Playerbot::Role declared, ::Playerbot::Role inferred, bool hybrid)
{
    using ::Playerbot::Role;
    CompanionRoleChoice c;
    if (requested != Role::Unknown || declared != Role::Unknown)
    {
        c.role = requested != Role::Unknown ? requested : declared;
        c.source = CompanionRoleSource::Declared;
        c.pin = true;
        c.store = true;
        return c;
    }
    // A hybrid nobody gave a role never becomes a tank by its talents (MC 2026-10-08: healer paladins came back as tanks because
    // their Protection talents said so). The raid's tank quota is the raid brain's job (warriors first).
    if (inferred == Role::Tank && hybrid)
    {
        c.role = Role::Healer;
        c.source = CompanionRoleSource::SpecHybrid;
        c.pin = true;       // the healer spec is pinned so the talent walk does not follow the Protection build; not stored
        return c;
    }
    c.role = (inferred == Role::Tank || inferred == Role::Healer) ? inferred : Role::Dps;
    c.source = CompanionRoleSource::Spec;
    return c;
}

// The spec a companion plays: the one stored by `.bot save` when it exists and still fits the role the companion plays now,
// else the one the role rule picked. saved_role is the role the stored spec belongs to (tank, healer or dps spec of the class).
// A role changed since the save (a paladin saved Retribution, now asked to heal) goes by the role rule, not the old spec.
inline uint32 ChooseCompanionSpec(uint32 picked, uint32 saved, ::Playerbot::Role saved_role, ::Playerbot::Role role, bool role_stored)
{
    if (!role_stored || saved == 0 || saved_role != role)
        return picked;
    return saved;
}

}  // namespace Playerbot::V2::Fleet
