// Hakkar's Blood Siphon (Zul'Gurub 309), the clock and the windows the bots act in. Pure functions; RaidMechanics_ZG.cpp does the world part
// and tests/game/ClassicRaidFightScript.cpp covers these.
//
// The fight (zulgurub/classic_boss_hakkar.cpp, VMaNGOS boss_hakkar.cpp; spell data of the 1.60 client, build 70685):
//  - Blood Siphon 24324 comes 90 s after the pull and every 90 s after it is cast. It stuns Hakkar himself for 8 s (aura 12 on the caster)
//    and hits every raider within 100 y (script effect, classic_spell_zg_hakkar_blood_siphon): one carrying Poisonous Blood 24321 casts
//    24323 on him (poison, 8 s), every other one 24322 (a health funnel into him, 8 s).
//  - Poisonous Blood 24321 (90 s, 100 nature every 3 s on the carrier) comes from a Son of Hakkar (11357) as it dies: its death spell 24320
//    (smart_scripts "VMaNGOS EventAI 1135701") is a script effect on every enemy within 8 y of it (spell_zulgurub_poisonous_blood casts
//    24321 on each). There is no cloud on the ground: the raid has to stand next to the Son when it dies.
// So 45 s before each siphon the DPS kill one Son, and once it is low everybody gathers on it. 90 s of poison covers the siphon.

#pragma once

#include <cstdint>

namespace Playerbot::Raid::Zg {

constexpr uint32_t kHakkar = 14834;
constexpr uint32_t kSonOfHakkar = 11357;
constexpr uint32_t kBloodSiphon = 24324;          // Hakkar's cast (he carries its stun while it drains)
constexpr uint32_t kSiphonFeed = 24322;           // a raider's funnel into him
constexpr uint32_t kSiphonPoison = 24323;         // a poisoned raider's damage on him
constexpr uint32_t kPoisonousBlood = 24321;       // the aura that turns the siphon against him (90 s)
constexpr uint32_t kCorruptedBlood = 24328;       // spreads within 10 y: a carrier does not join the crowd on the Son
constexpr uint32_t kSiphonFirstMs = 90000;
constexpr uint32_t kSiphonPeriodMs = 90000;
constexpr uint32_t kSiphonLeadMs = 45000;         // a Son is pulled and killed from this long before the siphon is due (25 s was too short:
                                                  // sweep-zg-1109w, the Son was still at 100% 7 s before the siphon)
constexpr uint32_t kSiphonKillStopMs = 3000;      // ... up to this close to it
constexpr float kSonDeathReach = 8.0f;            // Poisonous Blood reaches this far from the dying Son (SpellRadius 14)
constexpr float kGatherYards = 4.0f;              // a bot gathering on the Son stands this close to it
constexpr float kGatherBelowPct = 60.0f;          // the raid gathers once the Son is this low
constexpr float kGatherReach = 45.0f;             // a bot farther than this from the Son does not walk over
constexpr float kPlatformFloorZ = 48.0f;          // Hakkar's platform (z 52.9) ends here: below 45.8 he evades (classic_boss_hakkar.cpp), so no
                                                  // tank fetches a Son from lower down

// When the next siphon is due, in ms from the pull. lastSiphonMs: when one was last seen (0: none yet).
inline uint32_t SiphonDueMs(uint32_t lastSiphonMs)
{
    return lastSiphonMs ? lastSiphonMs + kSiphonPeriodMs : kSiphonFirstMs;
}

// A Son is killed now (t ms into the fight): from the lead before the due siphon to a few seconds before it, while fewer than half of the
// raid carry Poisonous Blood (one Son dying in a crowd is enough; a second one is not started).
inline bool KillWindow(uint32_t t, uint32_t lastSiphonMs, uint32_t poisoned, uint32_t raiders)
{
    uint32_t const due = SiphonDueMs(lastSiphonMs);
    return poisoned * 2 < raiders && t + kSiphonLeadMs >= due && t + kSiphonKillStopMs < due;
}

// A bot walks onto the Son being killed: it has no Poisonous Blood yet, it is not Hakkar's tank or a Corrupted Blood carrier, the Son is
// low enough, and the bot is not already within the reach of its death.
inline bool Gathers(bool poisoned, bool excluded, float sonHpPct, float yardsToSon)
{
    return !poisoned && !excluded && sonHpPct <= kGatherBelowPct && yardsToSon <= kGatherReach && yardsToSon > kGatherYards;
}

} // namespace Playerbot::Raid::Zg
