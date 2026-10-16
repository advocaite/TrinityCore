#pragma once
// Pure positioning decision for the melee-behind step.
//
// Idea from Karrz's VMaNGOS bots (Karrz1/wow-1.12, playerbot/values/StanceReposition.cpp, Stances.cpp), rewritten against our
// snapshot (no code copied): the melee "behind" step fires only for a mob a tank holds, inside a 135 degree front arc, never on
// the mob's own victim, never on Onyxia.
// State_Idle.cpp calls it; keeping it here keeps that file's diff small.

#include "Bot/BotSnapshot.h"
#include <cmath>
#include <deque>
#include <mutex>
#include <unordered_map>

namespace Playerbot::Positioning {

constexpr uint32 kOnyxiaEntry = 10184;   // tail sweep behind: standing behind is not safe there
constexpr float  kBehindFrontHalfAngle = 1.178097f;   // 67.5 deg either side of the facing = a 135 degree front arc

// A melee bot standing in the front arc of `mob` should step behind it. False when the bot is the mob's victim (it is the tank
// or has aggro: moving it is a plain bug), when no tank holds the mob (the mob is on a squishy: run, do not circle), or on a tail-sweep boss.
inline bool MeleeBehindWanted(NearbyUnit const& mob, ObjectGuid me, float bot_x, float bot_y)
{
    if (mob.victim == me || mob.victim_kind != uint8(ThreatVictim::Tank) || mob.entry == kOnyxiaEntry)
        return false;
    const float dx = bot_x - mob.x, dy = bot_y - mob.y;
    const float d2 = dx * dx + dy * dy;
    if (d2 > 5.0f * 5.0f || d2 <= 0.25f)
        return false;
    // Angle between the mob's facing and the bot's bearing from it.
    const float dot = (dx * std::cos(mob.o) + dy * std::sin(mob.o)) / std::sqrt(d2);
    return dot > std::cos(kBehindFrontHalfAngle);
}

} // namespace Playerbot::Positioning
