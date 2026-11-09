// A player's bots take the way he went where the navmesh has none (Playerbot.Behavior.FollowOwnerSteps).
//
// Karrz 2026-10-31: "On the way up to the UBRS, that they jump up the chains and run along and then jump onto the ledge. And if they
// can't, after 15 seconds, they automatically appear on you." The climb from the Blackrock Mountain floor to the Spire is a run up a
// chain, a jump to the next and a jump onto the ledge: no walking route on the navmesh, so the follow stood under the chain. A player gets
// there by doing it, so the bots do what he did: the module's world tick keeps the last stretch of his way (OwnerTrail: a step every
// kTrailStepYd while he runs, and each jump with where he left the ground, where he came down and how long he was in the air), and a
// bot that has had no walking route to him for a moment while it stands on that way walks it step by step and makes his jumps.
// When it cannot (it fell off, the way is gone), the catch-up (Fleet/OwnerCatchUp.h) sets it down beside him after its 15 s.
//
// Pure logic for PlayerbotV2.cpp DriveOwnerTrail; the world state is read and the moves are made by the caller. Tested in
// tests/game/ClassicOwnerTrail.cpp.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>

namespace Playerbot::V2::Fleet
{

struct TrailStep
{
    float x = 0.f, y = 0.f, z = 0.f;
    uint32_t ms = 0;              // when he was here
    bool jump = false;            // the way into this step from the one before is a jump
    uint32_t air_ms = 0;          // a jump: how long he was in the air
};

constexpr float    kTrailStepYd      = 3.0f;      // a step is kept every this far while he runs
constexpr float    kTrailTeleportYd  = 40.0f;     // farther than this between two looks is a teleport, not a run: the way is forgotten
constexpr uint32_t kTrailKeepMs      = 120000u;   // steps older than this are forgotten
constexpr size_t   kTrailMaxSteps    = 160u;
constexpr float    kTrailJoinYd      = 6.0f;      // a bot this close to a step (and kTrailJoinDz in height) can join the way there
constexpr float    kTrailJoinDz      = 3.0f;
constexpr float    kTrailArriveYd    = 1.5f;      // a bot this close to its next step goes on to the one after
constexpr float    kTrailArriveDz    = 2.5f;
constexpr uint32_t kTrailStepMaxMs   = 4000u;     // a step the bot does not reach in this long ends the walk (catch-up takes over)
constexpr float    kTrailJumpMinYd   = 1.0f;      // a jump shorter than this (a hop on the spot) is not kept as a jump
constexpr float    kTrailJumpMaxYd   = 25.0f;     // nor one longer (a fall off a cliff is follow_drop's, a long one a knockback)

struct OwnerTrail
{
    std::deque<TrailStep> steps;
    bool falling = false;
    bool has_ground = false;
    float gx = 0.f, gy = 0.f, gz = 0.f;          // where he last stood on the ground
    uint32_t fall_start_ms = 0;
    bool has_last = false;
    float lx = 0.f, ly = 0.f, lz = 0.f;          // where he was at the last look

    void Clear()
    {
        steps.clear();
        falling = false;
        has_ground = false;
        has_last = false;
    }
};

inline float TrailDist(float ax, float ay, float az, float bx, float by, float bz)
{
    return std::sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by) + (az - bz) * (az - bz));
}

inline void TrailPush(OwnerTrail& t, float x, float y, float z, uint32_t now_ms, bool jump, uint32_t air_ms)
{
    t.steps.push_back({ x, y, z, now_ms, jump, air_ms });
    while (t.steps.size() > kTrailMaxSteps)
        t.steps.pop_front();
}

// Feeds one look at the owner. usable: on the ground or in the air of his own doing (not on a taxi, a ship, a vehicle, swimming, dead).
// falling: his movement says he is in the air (a jump or a fall).
inline void TrailRecord(OwnerTrail& t, float x, float y, float z, bool falling, bool usable, uint32_t now_ms)
{
    while (!t.steps.empty() && now_ms - t.steps.front().ms > kTrailKeepMs)
        t.steps.pop_front();
    if (!usable || (t.has_last && TrailDist(x, y, z, t.lx, t.ly, t.lz) > kTrailTeleportYd))
    {
        t.Clear();
        if (!usable)
            return;
    }
    t.has_last = true;
    t.lx = x; t.ly = y; t.lz = z;

    if (falling)
    {
        if (!t.falling)
        {
            t.falling = true;
            t.fall_start_ms = now_ms;
            // The take-off is where he last stood: a step of its own, so the bot walks to it before it jumps.
            if (t.has_ground && (t.steps.empty() ||
                TrailDist(t.gx, t.gy, t.gz, t.steps.back().x, t.steps.back().y, t.steps.back().z) > 0.5f))
                TrailPush(t, t.gx, t.gy, t.gz, now_ms, false, 0);
        }
        return;
    }
    if (t.falling)
    {
        // He came down: the landing is reached by a jump from the take-off (the last step) when it is a jump a bot can copy.
        t.falling = false;
        bool const from_takeoff = !t.steps.empty();
        float const len = from_takeoff ? TrailDist(x, y, z, t.steps.back().x, t.steps.back().y, t.steps.back().z) : 0.f;
        if (from_takeoff && len >= kTrailJumpMinYd && len <= kTrailJumpMaxYd)
            TrailPush(t, x, y, z, now_ms, true, now_ms - t.fall_start_ms);
        else if (from_takeoff && len > kTrailJumpMaxYd)
            t.steps.clear();   // not a jump to copy: the way before it does not lead on
        t.has_ground = true;
        t.gx = x; t.gy = y; t.gz = z;
        return;
    }
    t.has_ground = true;
    t.gx = x; t.gy = y; t.gz = z;
    if (t.steps.empty() || TrailDist(x, y, z, t.steps.back().x, t.steps.back().y, t.steps.back().z) >= kTrailStepYd)
        TrailPush(t, x, y, z, now_ms, false, 0);
}

// Where a bot at (x, y, z) joins his way: the latest step it stands by and can walk to (so a way that crossed itself is not walked twice,
// and a bot under a ledge does not take the steps on top of it), or -1. reachable(step) is the caller's walking check (a straight navmesh
// path from the bot to the step). The last step is never a start (there is nothing after it to walk), and only a way with a jump still
// ahead is worth walking: without one the ordinary follow finds the route.
template <typename Reachable>
inline int TrailJoin(OwnerTrail const& t, float x, float y, float z, Reachable&& reachable)
{
    for (size_t n = t.steps.size(); n >= 2; --n)
    {
        size_t const i = n - 2;
        TrailStep const& s = t.steps[i];
        float const dxy = std::sqrt((s.x - x) * (s.x - x) + (s.y - y) * (s.y - y));
        if (dxy > kTrailJoinYd || std::fabs(s.z - z) > kTrailJoinDz || !reachable(s))
            continue;
        for (size_t j = i + 1; j < t.steps.size(); ++j)
            if (t.steps[j].jump)
                return int(i);
        return -1;
    }
    return -1;
}

inline int TrailJoin(OwnerTrail const& t, float x, float y, float z)
{
    return TrailJoin(t, x, y, z, [](TrailStep const&) { return true; });
}

// A bot walking the way: the step it heads for and since when.
struct TrailWalk
{
    bool active = false;
    size_t next = 0;               // index into the owner's steps
    uint32_t step_since_ms = 0;    // when it started for that step
    uint32_t air_until_ms = 0;     // a jump in progress: it lands at this time
    bool jumped = false;           // its last move was a jump: it must be on the landing when it comes down
};

enum class TrailAction : uint8_t { None, Walk, Jump, Wait, Done, Lost };

// One look at a bot walking the way. Returns what to do now; `target` is the step to walk or jump to. The caller re-finds `next` after
// the owner's deque dropped steps at the front (TrailReindex).
inline TrailAction TrailStepFor(OwnerTrail const& t, TrailWalk& w, float x, float y, float z, uint32_t now_ms, TrailStep& target)
{
    if (!w.active)
        return TrailAction::None;
    if (w.air_until_ms && int32_t(now_ms - w.air_until_ms) < 0)
        return TrailAction::Wait;   // in the air
    w.air_until_ms = 0;
    bool const check_landing = w.jumped;
    w.jumped = false;
    while (w.next < t.steps.size())
    {
        TrailStep const& s = t.steps[w.next];
        float const dxy = std::sqrt((s.x - x) * (s.x - x) + (s.y - y) * (s.y - y));
        if (dxy > kTrailArriveYd || std::fabs(s.z - z) > kTrailArriveDz)
            break;
        ++w.next;
        w.step_since_ms = now_ms;
    }
    if (check_landing && w.next < t.steps.size() && t.steps[w.next].jump)
    {
        // Came down, but not on his landing (short of the ledge): one try, then the catch-up.
        w.active = false;
        return TrailAction::Lost;
    }
    if (w.next >= t.steps.size())
    {
        w.active = false;
        return TrailAction::Done;
    }
    if (now_ms - w.step_since_ms > kTrailStepMaxMs)
    {
        w.active = false;
        return TrailAction::Lost;
    }
    target = t.steps[w.next];
    if (target.jump)
    {
        // Only from where he left the ground: a bot that came down somewhere else (short of the landing) has lost the way.
        TrailStep const* from = w.next ? &t.steps[w.next - 1] : nullptr;
        if (!from || std::sqrt((from->x - x) * (from->x - x) + (from->y - y) * (from->y - y)) > kTrailArriveYd + 1.0f ||
            std::fabs(from->z - z) > kTrailArriveDz)
        {
            w.active = false;
            return TrailAction::Lost;
        }
        // A jump is made from its take-off: the step before, which the loop above has just passed.
        w.air_until_ms = now_ms + (target.air_ms ? target.air_ms : 600u) + 150u;
        w.step_since_ms = now_ms;
        w.jumped = true;
        return TrailAction::Jump;
    }
    return TrailAction::Walk;
}

// Starts a bot on the way at step `join`: it walks to that step first, then on along his way.
inline void TrailBegin(OwnerTrail const& t, TrailWalk& w, int join, uint32_t now_ms)
{
    w.active = join >= 0 && size_t(join) + 1 < t.steps.size();
    w.next = w.active ? size_t(join) : 0;
    w.step_since_ms = now_ms;
    w.air_until_ms = 0;
    w.jumped = false;
}

// After the owner's steps were trimmed at the front: the walk's next step by its time stamp, or the walk ends.
inline void TrailReindex(OwnerTrail const& t, TrailWalk& w, uint32_t next_ms)
{
    if (!w.active)
        return;
    for (size_t i = 0; i < t.steps.size(); ++i)
        if (t.steps[i].ms == next_ms)
        {
            w.next = i;
            return;
        }
    w.active = false;
}

} // namespace Playerbot::V2::Fleet
