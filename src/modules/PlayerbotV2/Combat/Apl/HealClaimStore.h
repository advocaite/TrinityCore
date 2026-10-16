// HealClaimStore - the raid healers' claims on a patient (see "Heals in flight" in ApClassicHeal.h), in a header of their own so the
// intent executor can release a claim when the core refuses the cast.
//
// MC Magmadar 2026-10-10: Denny and Yngve were feared by Panic, every heal was refused (PREVENTED_BY_MECHANIC), and each refused heal
// still left its claim; the other healers counted heals that never ran, and the feared healers' own re-picks stepped up through the
// ranks (1649 -> 1188 -> 1109 -> 680 -> 0 in flight).

#pragma once

#include "ObjectGuid.h"
#include "../../Bot/ClassicHealCover.h"
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Playerbot::Combat::ClassicHeal {

using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using int32  = std::int32_t;

constexpr uint32 kHealClaimGraceMs   = 600;
constexpr uint32 kHealClaimMaxAgeMs  = 10000;
constexpr int32  kHealCriticalLandMs = 500;

struct HealClaim
{
    ObjectGuid patient;
    uint32     spell    = 0;      // exact rank cast
    float      amount   = 0.0f;   // ExpectedHeal of that rank
    uint32     start_ms = 0;      // claimant's published_at_ms at emit
    uint32     land_ms  = 0;      // start + base cast time
};
using HealClaims = std::vector<std::pair<uint64, HealClaim>>;   // healer guid counter -> claim

// One book for the whole process. The map and its mutex must not be function-local statics of a template: every lambda type is a
// separate instantiation, and each would get a private map, so a claim written by EmitHeal would be invisible to PickPatient,
// CopyHealClaims and ReleaseHealClaim.
struct HealClaimBook
{
    std::mutex mu;
    std::unordered_map<uint64, HealClaim> claims;
};

inline HealClaimBook& HealClaimBookInstance()
{
    static HealClaimBook book;
    return book;
}

template <typename F>
auto WithHealClaims(uint32 now, F&& f)
{
    HealClaimBook& book = HealClaimBookInstance();
    std::lock_guard<std::mutex> lock(book.mu);
    auto& claims = book.claims;
    if (claims.size() > 64)
        for (auto it = claims.begin(); it != claims.end();)
            it = int32(now - it->second.start_ms) > int32(kHealClaimMaxAgeMs) ? claims.erase(it) : std::next(it);
    return f(claims);
}


// A cast the core refused started nothing: its claim goes, so nobody counts a heal that never runs.
// Only a claim made this tick goes: an older claim is a heal still running, which an unrelated refused cast must not drop.
inline bool ClaimIsFromThisCast(HealClaim const& c, ObjectGuid patient, uint32 now)
{
    return ClaimReleasable(patient.IsEmpty(), c.patient == patient, c.start_ms, now);
}

inline void ReleaseHealClaim(uint64 healer_low, ObjectGuid patient, uint32 now)
{
    WithHealClaims(now, [&](std::unordered_map<uint64, HealClaim>& claims)
    {
        auto const it = claims.find(healer_low);
        if (it != claims.end() && ClaimIsFromThisCast(it->second, patient, now))
            claims.erase(it);
        return 0;
    });
}

}  // namespace Playerbot::Combat::ClassicHeal
