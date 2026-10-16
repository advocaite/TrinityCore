// A pet of a playerbot that cannot walk to its owner (the owner stands on another level of a city, over a ledge the pet cannot take,
// or the path stops short): left to the follow generator it cheat-follows with a straight shortcut through the wall and shakes in
// place against it for as long as the owner stays there (Ironforge: a voidwalker on the Military Ward bridge, its warlock on a lower
// level). The way a player's pet ends up beside its owner when it falls behind is a snap to the owner's side, so: after a few
// looks in a row that find no way, and at most every kPetSnapCooldownMs, the pet is set down next to the owner.
// Pure decision logic for the module's world tick; the probe (the path) is made by the caller.
#pragma once

#include <cstdint>

namespace Playerbot::V2::Fleet
{

struct PetSnapState
{
    uint32_t next_check_ms = 0;   // module ms before which the pet is not looked at again
    uint32_t last_snap_ms = 0;
    bool snapped_once = false;
    uint8_t strikes = 0;          // consecutive looks that found the pet with no way to its owner
};

constexpr uint32_t kPetSnapCheckMs    = 1000u;   // a pet is looked at this often
constexpr uint8_t  kPetSnapStrikes    = 3u;      // no way to the owner three looks running (about three seconds) before it is snapped
constexpr uint32_t kPetSnapCooldownMs = 4000u;   // at most one snap per pet in this time
constexpr float    kPetSnapMinGap     = 6.0f;    // a pet this close to its owner is left alone

inline bool PetSnapDue(PetSnapState const& st, uint32_t now_ms)
{
    return int32_t(now_ms - st.next_check_ms) >= 0;
}

// Feeds one look. unreachable: the pet has no usable path to its owner. Returns true when the pet is to be snapped now.
inline bool PetSnapUpdate(PetSnapState& st, bool unreachable, uint32_t now_ms)
{
    st.next_check_ms = now_ms + kPetSnapCheckMs;
    if (!unreachable)
    {
        st.strikes = 0;
        return false;
    }
    if (st.strikes < 255) ++st.strikes;
    if (st.strikes < kPetSnapStrikes)
        return false;
    if (st.snapped_once && int32_t(now_ms - st.last_snap_ms) < int32_t(kPetSnapCooldownMs))
        return false;
    st.snapped_once = true;
    st.last_snap_ms = now_ms;
    return true;
}

} // namespace Playerbot::V2::Fleet
