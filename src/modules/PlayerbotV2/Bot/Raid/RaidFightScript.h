// RaidFightScript - a boss script that owns the DPS side of one fight.
//
// Not a second rule engine: the raid rule tables, the tank assignment (PveGroupCoordinator::PlanRaidTanks) and the healing stay
// exactly as they are. A fight script only decides two things for the DPS bots of its boss, and switches off the rules that would
// decide them a second time:
//   1. WHICH enemy every DPS bot attacks: the kill order, one enemy at a time, the same one for everybody.
//   2. WHEN the DPS start: a fixed wait after the fight begins, then no threat holds at all.
// The owner's skull (else cross) always wins over the script's pick.
// Pure functions only (no world access), so tests/game/ClassicRaidFightScript.cpp covers them.

#pragma once

#include "../ThreatLimits.h"

#include <cstddef>
#include <cstdint>

namespace Playerbot::Raid::Script {

constexpr size_t kKillOrderMax = 8;

struct Fight
{
    uint32_t map;
    uint32_t boss;                 // creature entry that names the fight
    uint32_t killOrder[kKillOrderMax];   // creature entries, first dies first, 0 ends the list
    uint32_t startWaitMs;          // DPS wait this long from the first engaged enemy, then attack
    char const* name;              // log only
    bool skullOnly = false;        // the owner's skull is the only enemy the DPS hit; no skull, no damage (the kill order is not used)
    uint8_t sameRank = 0;          // the first <sameRank> entries of the kill order share the first rank (adds of one wave, any colour)
    bool heldFirst = false;        // inside a rank, an enemy a tank holds comes before a loose one
    uint32_t preferEntry = 0;      // inside a rank (after heldFirst), this entry comes before the others
    bool schoolAware = false;      // a DPS bot that cannot hurt an enemy (every school it deals resisted or immune) does not count it
    uint32_t freeAura = 0;         // a DPS bot carrying this aura is left to the generic rules (Delusions of Jin'do: only it sees the Shades)
};

// Lucifron (MC 409): the two Flamewaker Protectors first, then Lucifron. The start wait is the raid's one boss opening wait (Playerbot::kRaidBossOpenWaitMs, 8 s: the main tank takes a while to run in); there is no threat cap after
// that (a bot that pulls a Protector is taunted back by the loose-mob rule).
// Majordomo Executus (MC 409): the owner's skull is the only thing the DPS hit (skullOnly). The owner sheeps with his icons, so nothing else is
// attacked, marked or not; with no skull the DPS stand. He is never the focus (he cannot be killed, he yields when the eight are dead).
// Golemagg (MC 409): Golemagg only. The Core Ragers are not on the list: they stay off every DPS bot's target (Priority -1) and the off tank keeps them.
// Sulfuron Harbinger (MC 409): the four Flamewaker Priests one at a time, then Sulfuron. The priests are immune to every crowd control, so the script
// dropping CC on its own enemies changes nothing here.
// Shazzrah (MC 409): one boss. After his Gate he stands on a non-tank for a moment; the DPS stay on him and the retaunt rule gives him back.
// Baron Geddon (MC 409): one boss, no adds. The script's worth here is the one start wait in place of the threat hold (four DPS were held for 20 s).
// Garr (MC 409): Garr only. The Firesworn are not on the list, so the warlocks' Banish jobs on them stay, and the Spare rule (RaidEncounters.h)
// keeps them alive until he falls; once he is dead the generic rules finish them.
// Nefarian (BWL 469): phase 1 is Lord Victor Nefarius (10162, immune behind his barrier, never the focus) sending two drakonids every 6-7 s
// and a Chromatic pair every 35 s (classic_boss_victor_nefarius.cpp); Nefarian lands when 40 are dead. Every colour is one rank: the DPS
// all hit one drakonid a tank holds, the most hurt first, and stay on it until it dies, then Nefarian. bwl_nefarian 2026-11-08 (three
// sweeps): each DPS bot picked its own drakonid and a fresh threat hold on each new one (raid:threat_hold / raid:switch_target every
// second), 2,200-3,100 raid damage a second against the ~6,100 health a second the waves bring (15.3k a drakonid, 24.4k a Chromatic);
// the tanks ended up holding 5-9 each and died at 100-120 s, before 30 of the 40 were dead.
// Among the held ones a Black Drakonid (14265) goes first: Brood Power: Black 22560 is 1,250 fire to everyone within 15 yards of it every
// 4-6 s (100%: VMaNGOS smart_scripts), the others 20 s at 70%. bwl_nefarian 1108e (with the focus): 11 of 15 deaths were Black Brood Power on
// the melee and hunters in one pile, within 3 s, and the first tank died to Black melee. A DPS bot that cannot hurt the focus (every
// school it deals is resisted: a warlock on a Black, 400 fire and shadow) takes its own pick instead (RaidFightScript.cpp).
// Zul'Gurub (309, 20 raiders). The same two decisions as Molten Core: one start wait in place of the threat holds, and one enemy for every DPS
// bot. zg_venoxis 2026-10-20 (build 124, the last ZG run with a raid log): at t=9-21 s ten of the twelve DPS bots had no target ("no target
// 10, on the focus 2") while the tanks held Venoxis and the cobras; 1,200-1,400 raid damage a second for the whole pull. The rule rows of
// RaidEncounterData_ZG.cpp (spread, dispels, kicks, kill-together) stay as they are; kicks and dispels are cast orders and run before the
// script's target.
// Jeklik: the Bloodseeker Bats (11368, six at 40 s and every 65 s) and in her second half the Frenzied Bats (14965) die first, any of them
//   (one rank), then Jeklik. Great Heal 23954 is kicked by the kick election as before.
// Venoxis: a Parasitic Serpent (14884, on a random raider every 10 s in snake form) first, then the four Razzashi Cobras, then Venoxis.
// Mar'li: the Spawns of Mar'li (15041) grow while they live (Enlarge 24109): first, then Mar'li.
// Bloodlord Mandokir: the Vilebranch Speaker (11391, his death starts the fight), Ohgan (14988), then Mandokir (see RaidEncounterData_ZG.cpp
//   for why Ohgan goes early). Threatening Gaze (hold_rotation) still stops the watched bot: that check runs before the script's target.
// Jin'do: the Powerful Healing Ward (14987) and the Brain Wash Totem (15112) first (one rank), then Jin'do. A bot carrying Delusions of
//   Jin'do (24306) is left to the generic rules: only it sees the Shades of Jin'do (14986) and only it can kill them.
// Arlokk: Arlokk while she can be hit; while she is vanished (out of the fight) the Zulian Prowlers (15101).
// Gahz'ranka, Hakkar: one boss (Hakkar's Sons of Hakkar have their own rules, RaidEncounterData_ZG.cpp).
// Edge of Madness: Hazza'rah's Nightmare Illusions (15163, 457 hp) before him; Gri'lek, Renataki and Wushoolay are one boss each.
// Not scripted: Thekal (the three must fall within 10 s of each other: the killTogether rule spreads the damage, one focus would break it).
inline constexpr Fight kFights[] = {
    { 409, 12118, { 12119, 12118, 0, 0 }, kRaidBossOpenWaitMs, "Lucifron" },
    { 409, 12057, { 12057, 0, 0, 0 }, kRaidBossOpenWaitMs, "Garr" },
    { 409, 12056, { 12056, 0, 0, 0 }, kRaidBossOpenWaitMs, "Baron Geddon" },
    { 409, 12264, { 12264, 0, 0, 0 }, kRaidBossOpenWaitMs, "Shazzrah" },
    { 409, 12098, { 11662, 12098, 0, 0 }, kRaidBossOpenWaitMs, "Sulfuron Harbinger" },
    { 409, 11988, { 11988, 0, 0, 0 }, kRaidBossOpenWaitMs, "Golemagg" },
    { 409, 12018, { 11663, 11664, 0, 0 }, kRaidBossOpenWaitMs, "Majordomo Executus", true },
    { 469, 10162, { 14261, 14262, 14263, 14264, 14265, 14302, 11583, 0 }, kRaidBossOpenWaitMs, "Nefarian", false, 6, true, 14265, true },
    { 309, 14517, { 11368, 14965, 14517, 0 }, kRaidBossOpenWaitMs, "High Priestess Jeklik", false, 2 },
    { 309, 14507, { 14884, 11373, 14507, 0 }, kRaidBossOpenWaitMs, "High Priest Venoxis" },
    { 309, 14510, { 15041, 14510, 0, 0 }, kRaidBossOpenWaitMs, "High Priestess Mar'li" },
    { 309, 11382, { 11391, 14988, 11382, 0 }, kRaidBossOpenWaitMs, "Bloodlord Mandokir" },
    { 309, 11380, { 14987, 15112, 11380, 0 }, kRaidBossOpenWaitMs, "Jin'do the Hexxer", false, 2, false, 0, false, 24306 },
    { 309, 14515, { 14515, 15101, 0, 0 }, kRaidBossOpenWaitMs, "High Priestess Arlokk" },
    { 309, 15114, { 15114, 0, 0, 0 }, kRaidBossOpenWaitMs, "Gahz'ranka" },
    { 309, 14834, { 14834, 0, 0, 0 }, kRaidBossOpenWaitMs, "Hakkar" },
    { 309, 15082, { 15082, 0, 0, 0 }, kRaidBossOpenWaitMs, "Gri'lek" },
    { 309, 15083, { 15163, 15083, 0, 0 }, kRaidBossOpenWaitMs, "Hazza'rah" },
    { 309, 15084, { 15084, 0, 0, 0 }, kRaidBossOpenWaitMs, "Renataki" },
    { 309, 15085, { 15085, 0, 0, 0 }, kRaidBossOpenWaitMs, "Wushoolay" },
};

// The fight an engaged enemy belongs to on this map (nullptr: none).
inline Fight const* FightFor(uint32_t map, uint32_t entry)
{
    for (Fight const& f : kFights)
    {
        if (f.map != map)
            continue;
        if (f.boss == entry)
            return &f;
        for (uint32_t e : f.killOrder)
            if (e && e == entry)
                return &f;
    }
    return nullptr;
}

// Where an entry stands in the kill order (0 = first); a large number when it is not on the list.
inline size_t RankOf(Fight const& f, uint32_t entry)
{
    for (size_t i = 0; i < kKillOrderMax; ++i)
        if (f.killOrder[i] && f.killOrder[i] == entry)
            return i < f.sameRank ? 0 : i - (f.sameRank ? f.sameRank - 1 : 0);
    return 99;
}

struct Enemy
{
    uint64_t guid = 0;
    uint32_t entry = 0;
    float hpPct = 100.f;
    bool eligible = false;     // alive, engaged, not crowd-controlled or reflecting, not forbidden for this bot
    bool held = false;         // a tank of the raid is its victim (Fight::heldFirst)
};

// The enemy every DPS bot attacks now, 0 = none (the generic rules pick).
//   ownerMark: the owner's skull or cross, when it is an eligible enemy it wins.
//   previous : last pick, kept while it is still eligible and still the best rank (no flip-flop between two equal Protectors).
// Lowest rank first; inside a rank (Fight::heldFirst) one a tank holds, then (Fight::preferEntry) that entry, then the most hurt one (finish what was started), then the lowest guid.
inline uint64_t PickFocus(Fight const& f, Enemy const* enemies, size_t n, uint64_t ownerMark, uint64_t previous)
{
    if (f.skullOnly)
    {
        for (size_t i = 0; i < n; ++i)
            if (ownerMark && enemies[i].eligible && enemies[i].guid == ownerMark)
                return ownerMark;
        return 0;
    }
    if (ownerMark)
        for (size_t i = 0; i < n; ++i)
            if (enemies[i].eligible && enemies[i].guid == ownerMark)
                return ownerMark;
    size_t bestRank = 99;
    for (size_t i = 0; i < n; ++i)
        if (enemies[i].eligible)
            bestRank = bestRank < RankOf(f, enemies[i].entry) ? bestRank : RankOf(f, enemies[i].entry);
    if (bestRank == 99)
        return 0;
    if (previous)
        for (size_t i = 0; i < n; ++i)
            if (enemies[i].eligible && enemies[i].guid == previous && RankOf(f, enemies[i].entry) == bestRank)
                return previous;
    Enemy const* best = nullptr;
    for (size_t i = 0; i < n; ++i)
    {
        Enemy const& e = enemies[i];
        if (!e.eligible || RankOf(f, e.entry) != bestRank)
            continue;
        if (best && f.heldFirst && e.held != best->held)
        {
            if (e.held)
                best = &e;
            continue;
        }
        if (best && f.preferEntry && (e.entry == f.preferEntry) != (best->entry == f.preferEntry))
        {
            if (e.entry == f.preferEntry)
                best = &e;
            continue;
        }
        if (!best || e.hpPct < best->hpPct || (e.hpPct == best->hpPct && e.guid < best->guid))
            best = &e;
    }
    return best ? best->guid : 0;
}

// True while the DPS still wait for the tank to get in: from the first engaged enemy (sinceMs, 0 = none yet) for waitMs.
inline bool StartWait(uint32_t sinceMs, uint32_t nowMs, uint32_t waitMs)
{
    return sinceMs != 0 && nowMs >= sinceMs && nowMs - sinceMs < waitMs;
}

} // namespace Playerbot::Raid::Script
