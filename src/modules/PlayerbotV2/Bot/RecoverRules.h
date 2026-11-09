// Pure rules for "is it safe to sit down and eat / drink" (Classic::SafeToRecover), kept free of snapshot types so the
// thresholds can be tested.

#pragma once

namespace Playerbot::Classic {

// A hostile blocks sitting down only when it is engaged with the bot's side (its victim is the bot or a group member) and
// close enough to join in. The old rule counted every enemy with any victim within 45 y, so a dungeon healer never sat:
// some pack member or patrol is always within 45 y (Deadmines, live 2026-10-10: priest at 87 % "not_safe_to_sit", started
// the next fights at 64 % and was dry two pulls later).
constexpr float kRecoverEnemyRange = 30.0f;      // aggro reach (20 y at most for same-level mobs) plus a margin
constexpr float kRecoverMemberFightRange = 45.0f;

inline bool EnemyBlocksRecovery(bool victim_is_bot_or_group, float dist2)
{
    return victim_is_bot_or_group && dist2 < kRecoverEnemyRange * kRecoverEnemyRange;
}

inline bool MemberFightBlocksRecovery(float dist2)
{
    return dist2 < kRecoverMemberFightRange * kRecoverMemberFightRange;
}

// A player's bots rest like the player would (the owner, 2026-10-29: "they sit down, stand up, sit down, stand up ... I want them to look
// a wee bit more lifelike"; "it's only the bots that need it, a rogue or warrior has no need to sit down"). In UBRS on RC5 the dungeon
// healer drank at 81-87 % mana every time the group stopped (Harmony 12 drinks in 7 minutes), the casters below 70 %, and the
// personality's food mark sent fighters to eat at up to 90 % health; every step of the player stood them up and the next stop sat them
// down with a new item. The rule:
//   - sit only when it is needed: a drink below kLedDrinkBelowManaPct, food below kLedEatBelowHpPct (a rogue or warrior has no mana
//     and only eats when properly hurt);
//   - stay down until full (PickRecovery's 95 %);
//   - not while the player (or the tank the bot keeps to) is walking on, and not again within kLedResitMs of standing up, so a bot
//     the follow got up walks with the group instead of dropping back down at the next pause.
constexpr int kLedDrinkBelowManaPct = 60;
constexpr int kLedEatBelowHpPct = 55;
constexpr unsigned kLedResitMs = 20000;
constexpr unsigned kLedAnchorStillMs = 3000;   // the player stood this long: the group has stopped

struct LedRestInputs
{
    bool resting = false;              // already eating / drinking: the hold decides, not this
    unsigned since_stood_ms = ~0u;     // since the bot last got up from eating / drinking (~0u: never)
    unsigned since_anchor_moved_ms = ~0u;   // since the player / anchor last moved (~0u: unknown, taken as still)
};

// Where the player who leads the bot (its owner, else the group's leader) last was, looked at every tick whatever rule runs. The
// follow's anchor tracker is fed only where the follow code runs and forgets a player it has not seen for 3 s, after which it read
// "standing still": in the Violet Hold (2026-11-08) Askara sat down to drink while the owner walked on, the follow stood her straight
// up and the drink was lost. A new or first-seen leader counts as just moved, so nobody sits before he has really stood kLedAnchorStillMs.
struct LeaderStillTracker
{
    unsigned long long leader = 0;   // the leader's guid counter (0: none seen)
    float x = 0.f, y = 0.f;          // where he was when last seen to move
    unsigned moved_ms = 0;           // when he last moved (game ms, never 0 once seen)

    void note(unsigned long long id, float px, float py, unsigned now_ms)
    {
        unsigned const stamp = now_ms ? now_ms : 1u;
        if (id == 0)
        {
            leader = 0;
            moved_ms = 0;
            return;
        }
        float const dx = px - x, dy = py - y;
        if (id != leader || dx * dx + dy * dy > 1.0f)
        {
            leader = id;
            x = px;
            y = py;
            moved_ms = stamp;
        }
    }
    // Since he last moved (~0u: no leader seen).
    unsigned since_moved_ms(unsigned now_ms) const
    {
        return moved_ms != 0 && now_ms >= moved_ms ? now_ms - moved_ms : ~0u;
    }
};

// The rest rule's "since the player / anchor moved": the more recent of the follow's anchor and the leader tracker.
inline unsigned SinceLeaderMovedMs(unsigned since_anchor_ms, unsigned since_leader_ms)
{
    return since_anchor_ms < since_leader_ms ? since_anchor_ms : since_leader_ms;
}

// May a player's bot sit down to eat or drink now (the thresholds are the caller's pick)?
inline bool LedMaySitDown(LedRestInputs const& in)
{
    if (in.resting)
        return true;
    if (in.since_stood_ms < kLedResitMs)
        return false;
    return in.since_anchor_moved_ms >= kLedAnchorStillMs;
}

} // namespace Playerbot::Classic
