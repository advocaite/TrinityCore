// A companion goes where its owner goes, also where it cannot walk.
//
// Flights (Playerbot.Behavior.FlyWithOwner): a taxi stays on the owner's map, so the follow chased a flying owner on the ground, mounted
// because a player on a taxi counts as mounted, through every camp under the route, and was left far behind when he landed (Karrz
// 2026-10-30: "the bots are going to have to fly as well"). Now the bots of his group that stand by him when he takes off board the
// same route a moment later and land with him; the ones that could not board (too far off, dead, fighting) are brought to him when he
// lands, as his "summon" would.
//
// Catch-up (Playerbot.Behavior.CatchUpToOwner): a bot that cannot get any closer to its owner while he is well away, for kCatchUpStrikes
// looks running, is set down beside him (Karrz 2026-10-30: "if they're stuck and they can't get to you and you're running away, they
// just teleport to you"). The case is Blackrock Mountain: a player takes the chains and the jumps up to the Spire, a bot leaps down
// ledges (API::move_to follow_drop) but never up or across, finds no way and stood where it was. Pets already work this way
// (Fleet/PetSnap.h). It also brings a bot left on another continent when the owner took a ship or a zeppelin it did not board, and one
// left outside (or inside) a dungeon he went into (or out of): Karrz 2026-10-31, "after 15 seconds, they automatically appear on you".
// Out of range (Karrz 2026-11-02, after a port from Dun Morogh to Crystal Lake left his bots running down from Ironforge): a bot more
// than kCatchUpFarYards from its owner on the same map for the same 15 s is brought too, even while its walk still gets it closer.
//
// Pure decision logic for the module's world tick (PlayerbotV2.cpp DriveOwnerCatchUp / DriveFlyWithOwner); the world state is read by
// the caller. Tested in tests/game/ClassicOwnerCatchUp.cpp.
#pragma once

#include <cstdint>

namespace Playerbot::V2::Fleet
{

// ---- Flights ----

constexpr float    kFlyBoardYards     = 45.0f;   // a bot this close to the owner when he takes off boards his flight
constexpr uint32_t kFlyBoardDelayMinMs = 300u;   // each bot boards a moment after him, not on the same frame
constexpr uint32_t kFlyBoardDelayMaxMs = 1200u;

// Whether a bot of the owner's group can board his flight now.
inline bool FlyBoardEligible(bool alive, bool in_combat, bool in_flight, bool on_transport_or_vehicle, bool same_map_instance,
                             float dist_to_owner)
{
    return alive && !in_combat && !in_flight && !on_transport_or_vehicle && same_map_instance && dist_to_owner <= kFlyBoardYards;
}

// ---- Catch-up ----

struct CatchUpState
{
    uint32_t next_check_ms = 0;   // ms before which the bot is not looked at again
    uint32_t last_snap_ms = 0;
    bool snapped_once = false;
    uint8_t strikes = 0;          // looks running that found the bot apart from its owner and no closer
    float best_gap = -1.0f;       // the smallest gap to the owner since the strikes began; -1 = none (another map, or reset)
};

constexpr uint32_t kCatchUpCheckMs     = 1000u;   // a bot is looked at this often
constexpr uint8_t  kCatchUpStrikes     = 15u;     // about 15 s with no way closer before it is brought
constexpr uint32_t kCatchUpCooldownMs  = 10000u;  // at most one catch-up per bot in this time
constexpr float    kCatchUpGapYards    = 40.0f;   // apart: farther than this from the owner
constexpr float    kCatchUpClimbYards  = 8.0f;    // ...or on another level: this much higher or lower and
constexpr float    kCatchUpClimbGap    = 12.0f;   //    farther than this (the owner jumped up a ledge the bot cannot take)
constexpr float    kCatchUpProgressYd  = 3.0f;    // getting this much closer than the best gap so far is progress
constexpr float    kCatchUpFarYards    = 100.0f;  // out of range: farther than this, walking closer does not stop the count

inline bool CatchUpDue(CatchUpState const& st, uint32_t now_ms)
{
    return int32_t(now_ms - st.next_check_ms) >= 0;
}

// Whether a bot this far from its owner (3D gap, height difference) is apart from him.
inline bool CatchUpApart(float gap, float dz)
{
    float const adz = dz < 0.f ? -dz : dz;
    return gap > kCatchUpGapYards || (adz > kCatchUpClimbYards && gap > kCatchUpClimbGap);
}

// Feeds one look. eligible: both out of combat and alive, nothing else owns the bot's movement (see the caller). other_map: the owner is
// on another map or in another copy of an instance, a dungeon included (gap is ignored: no walking gets the bot there). A bot out of
// range (gap > kCatchUpFarYards) counts the same way, walking or not. Returns true when the bot is to be brought to its owner now.
inline bool CatchUpUpdate(CatchUpState& st, bool eligible, bool other_map, float gap, float dz, uint32_t now_ms)
{
    st.next_check_ms = now_ms + kCatchUpCheckMs;
    if (!eligible || (!other_map && !CatchUpApart(gap, dz)))
    {
        st.strikes = 0;
        st.best_gap = -1.0f;
        return false;
    }
    if (!other_map && gap <= kCatchUpFarYards)
    {
        if (st.best_gap < 0.f || gap < st.best_gap - kCatchUpProgressYd)
        {
            // First look apart, or closing in: the walk is working.
            bool const first = st.best_gap < 0.f;
            st.best_gap = gap;
            st.strikes = first ? 1u : 0u;
            return false;
        }
    }
    if (st.strikes < 255) ++st.strikes;
    if (st.strikes < kCatchUpStrikes)
        return false;
    if (st.snapped_once && int32_t(now_ms - st.last_snap_ms) < int32_t(kCatchUpCooldownMs))
        return false;
    st.snapped_once = true;
    st.last_snap_ms = now_ms;
    st.strikes = 0;
    st.best_gap = -1.0f;
    return true;
}

// How many times the module's world tick has brought this bot to its player (PlayerbotV2.cpp DriveOwnerCatchUp); the brm_follow scenario
// tells walked from brought by it. World thread.
uint32_t CatchUpCount(uint64_t bot_low);

} // namespace Playerbot::V2::Fleet
