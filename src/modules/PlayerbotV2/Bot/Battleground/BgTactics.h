// Battleground tactics rules as pure functions (no Player / Battleground), so tests/game/BgTactics.cpp can run them.
//
// Ideas ported from the two reference bot families, rewritten for our team planner (credit: ike3 playerbots for the
// capture-skip / role split ideas, liyunfan1223/mod-playerbots BattleGroundTactics.cpp for the capture abort, the 60 s
// objective reset and the camp-the-enemy-graveyard rule). Values are vanilla 1.12 behaviour (AB capture 60 s, AV tower 5 min).
// Written for Karrz.
#pragma once

#include <cmath>
#include <cstdint>

namespace Playerbot::BgTactics
{
    // A bot that is channeling a banner capture stops and fights when an enemy player is this close to the banner (mod-playerbots:
    // 11 yd). Past that range it keeps clicking: an enemy at the other end of the node cannot undo the channel.
    constexpr float kCaptureAbortEnemyRange = 11.0f;

    enum class CaptureAbort : uint8_t
    {
        None,        // keep capturing
        EnemyNear,   // an enemy player is within kCaptureAbortEnemyRange of the banner: turn and fight
        BannerGone,  // the banner is no longer capturable (someone else finished it): re-roll the objective
    };

    inline CaptureAbort ShouldAbortCapture(bool banner_capturable, bool have_enemy, float nearest_enemy_dist_to_banner)
    {
        if (!banner_capturable)
            return CaptureAbort::BannerGone;
        if (have_enemy && nearest_enemy_dist_to_banner <= kCaptureAbortEnemyRange)
            return CaptureAbort::EnemyNear;
        return CaptureAbort::None;
    }

    // How long an assault on a node takes before it flips (vanilla): AB/EotS/Deephaul 60 s, AV towers and graveyards 5 min.
    // type ids: 1 = AV, 2 = WSG, 3 = AB.
    inline uint32_t CaptureWindowMs(uint16_t bg_type_id)
    {
        return bg_type_id == 1 ? 300000u : 60000u;
    }

    // Fastest a bot covers ground in a battleground (mounted, straight line): a defender that cannot arrive before the flip even at
    // this speed is not sent (it would run to a node that is already lost; the next plan sends the attack squad to retake it).
    constexpr float kBgFastSpeed = 14.0f;   // yd/s

    inline bool CanArriveBeforeFlip(float dist, uint32_t window_ms, uint32_t contested_for_ms)
    {
        uint32_t const left = contested_for_ms >= window_ms ? 0u : window_ms - contested_for_ms;
        return dist <= kBgFastSpeed * (float(left) / 1000.0f);
    }

    // Escorts for the lone pickup runner on a CTF map. The soak (WSG 2026-10-11) lost ~9 of 12 bots on the way to the enemy flag with a
    // single escort; the carrier gets 3. Two once the team can spare them.
    inline int RunnerEscortCount(int team_size)
    {
        return team_size >= 8 ? 2 : 1;
    }

    // Stand-still watchdog (mod-playerbots resets a bot's objective every 60 s in WSG/AV): a bot that has not moved kStallRadius in
    // kStallMs while idle (not in combat, not casting, not holding an ordered post) is stalled and should re-pick its objective.
    constexpr uint32_t kStallMs = 60000;
    constexpr float    kStallRadius = 3.0f;

    struct StallWatch
    {
        float    x = 0.f, y = 0.f;
        uint32_t since_ms = 0;
        bool     init = false;

        // `busy` = in combat, casting, mounted-travelling on an order, dead, or waiting at the gate: resets the timer.
        bool Update(uint32_t now_ms, float nx, float ny, bool busy)
        {
            float const dx = nx - x, dy = ny - y;
            if (!init || busy || dx * dx + dy * dy > kStallRadius * kStallRadius)
            {
                x = nx; y = ny; since_ms = now_ms; init = true;
                return false;
            }
            if (now_ms - since_ms >= kStallMs)
            {
                since_ms = now_ms;   // fire once per window
                return true;
            }
            return false;
        }
    };

    // AB: a team that owns every node and is not capping anything camps the enemy graveyard instead of standing on the banners
    // (mod-playerbots does this at fixed coordinates).
    inline bool CampEnemyGraveyard(int owned_nodes, int total_nodes, int contested_nodes)
    {
        return total_nodes > 0 && owned_nodes >= total_nodes && contested_nodes == 0;
    }

    // The lone pickup runner. A flag room is defended; the soak (WSG 2026-10-11, 8428be7c) shows pickup -> die -> drop loops, the same
    // runner lifting the flag four times and dying with it each time. The runner may rush the stand past enemies only with company:
    // at least kRunnerMinCompany friends within kRunnerCompanyRange. Without them it fights what is near (the escorts catch up, and the
    // next plan hands the pickup to whoever is there) instead of running in alone.
    constexpr int   kRunnerMinCompany = 2;
    constexpr float kRunnerCompanyRange = 25.0f;
    constexpr float kRunnerEnemyRange = 30.0f;

    inline bool RunnerMayRush(int friends_near, int enemies_near)
    {
        return enemies_near == 0 || friends_near >= kRunnerMinCompany;
    }

    // An escort follows the runner from the moment the order is given, not only once the flag is lifted: before the pickup there is
    // no carrier, and the in-combat return rule (which keeps escorts within 40 yd) used to need one. `focus_dist` < 0 = the focus is
    // not among the units around the bot (far away).
    constexpr float kEscortLeashYards = 40.0f;

    inline bool EscortMustReturn(float focus_dist, bool enemy_close)
    {
        return !enemy_close && (focus_dist < 0.f || focus_dist > kEscortLeashYards);
    }
}
