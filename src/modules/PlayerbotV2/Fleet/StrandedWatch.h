// A resident bot that stands where it cannot live: in a lava pit, or on a piece of ground with no way out of it (the Great Forge
// of Ironforge: the pit floor is a walkable surface the ground probe finds, but nothing on the navmesh joins it to the city).
// Pure decision logic for the world spawner's watch; the probes (liquid type, path) are made by the caller.
#pragma once

#include <cmath>
#include <cstdint>

namespace Playerbot::V2::Fleet
{

struct StrandedState
{
    uint32_t next_check_ms = 0;   // module ms before which the bot is not looked at again
    float x = 0.0f, y = 0.0f, z = 0.0f;
    uint8_t strikes = 0;          // consecutive checks that found it with no path out, on the same spot
};

constexpr uint32_t kStrandedCheckMs  = 20000u;   // a bot is looked at this often
constexpr uint8_t  kStrandedStrikes  = 3u;       // no path to the player three checks running (a minute) before it is moved

inline bool StrandedDue(StrandedState const& st, uint32_t now_ms)
{
    return int32_t(now_ms - st.next_check_ms) >= 0;
}

// Feeds one check. in_magma: lava (or slime) under its feet; path_out: a complete navmesh path to the town's player exists
// (or the bot has no reference to test against: pass true). Returns true when the bot is to be moved out now. Where the bot
// stands does not matter: a bot pacing a ledge over the forge is as cut off as one standing still on the pit floor.
inline bool StrandedUpdate(StrandedState& st, bool in_magma, bool path_out, float x, float y, float z, uint32_t now_ms)
{
    st.next_check_ms = now_ms + kStrandedCheckMs;
    st.x = x; st.y = y; st.z = z;
    if (in_magma)
    {
        st.strikes = 0;
        return true;   // lava is never somewhere to stand: no second look
    }
    if (path_out)
    {
        st.strikes = 0;
        return false;
    }
    if (st.strikes < 255) ++st.strikes;
    if (st.strikes >= kStrandedStrikes)
    {
        st.strikes = 0;
        return true;
    }
    return false;
}

} // namespace Playerbot::V2::Fleet
