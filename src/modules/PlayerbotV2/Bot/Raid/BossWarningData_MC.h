// Molten Core boss warnings — DATA ONLY (see BossWarnings.h for the fields).
//
// Timers are the earliest times the encounter scripts use (scripts/Custom/Classic/instances/molten_core, and the Magmadar
// smart_scripts rows from VMaNGOS: Panic 10 s then 30-35 s, Frenzy 15 s then 15-20 s). Where a script randomises
// between min and max, the minimum is used, so the warning is never late, at worst a few seconds early.
// Lava Bomb, Rain of Fire, Shadow Bolt, Countspell and the other every-few-seconds abilities are left out on purpose:
// a warning each time would be noise.

#pragma once

#include "BossWarnings.h"

namespace Playerbot::Raid {

inline std::vector<BossWarning> const& MoltenCoreBossWarnings()
{
    static std::vector<BossWarning> const v = {
        // boss,  spell,  label,                                                   onCast, first,  repeat, lead,  aura
        { 12118, 19702, "Impending Doom - dispel it!",                              false, 10000, 20000, 4000, 19702 },   // Lucifron
        { 11982, 19408, "Panic - fear! Tremor Totem / Fear Ward",                   false, 10000, 30000, 5000, 19408 },   // Magmadar
        { 11982, 19451, "Frenzy - tranquilizing shot!",                             false, 15000, 15000, 3000, 19451 },
        { 12259, 19716, "Gehennas' Curse - remove curses",                          false,  5000, 25000, 4000, 19716 },   // Gehennas
        { 12057, 19492, "Antimagic Pulse - it strips buffs",                        false, 15000, 15000, 3000, 0 },       // Garr
        { 12057, 20482, "Massive Eruption - Garr blows up every Firesworn",         false, 360000,    0, 15000, 0 },
        { 12264, 19712, "Arcane Explosion - casters move out!",                     true,      0,     0,    0, 0 },       // Shazzrah
        { 12264, 23138, "Gate of Shazzrah - he blinks into the raid",               false, 25000, 25000, 4000, 0 },
        { 12056, 19695, "Inferno - run out of range!",                              false, 18000, 18000, 4000, 0 },       // Baron Geddon
        { 12056, 20475, "Living Bomb - spread out",                                 false, 15000, 12000, 3000, 20475, "has the Living Bomb - move away from the raid!" },
        { 12056, 19659, "Ignite Mana - dump your mana",                             false, 10000, 20000, 3000, 19659 },
        { 11988, 20228, "Pyroblast - interrupt or heal through",                    true,      0,     0,    0, 0 },       // Golemagg
        { 11988, 19798, "Earthquake - get out of the melee",                        true,      0,     0,    0, 0 },
        { 12018, 20619, "Magic Reflection or Damage Shield - check before attacking", false, 30000, 30000, 4000, 0 },     // Majordomo
        { 11502, 20566, "Wrath of Ragnaros - melee are knocked back",               false, 25000, 25000, 4000, 0 },       // Ragnaros
        { 11502, 20567, "Ragnaros submerges - Sons of Flame!",                      false, 180000,    0, 10000, 0 },
    };
    return v;
}

} // namespace Playerbot::Raid
