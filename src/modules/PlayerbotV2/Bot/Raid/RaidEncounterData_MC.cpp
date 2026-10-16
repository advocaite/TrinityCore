#include "RaidEncounters.h"

// Ported from Karrz's raid10-revamp ike3 bots (Karrz1/wow-1.12). Script references (mc/..., bwl/..., ony/...) point at the
// VMaNGOS scripts that Forever's scripts/Custom/Classic instances were ported from; the ids are the same.

namespace Playerbot::Raid {

    // Molten Core: dispels, purges, crowd control, kill priorities and no-AoE rules.
    // Script paths are src/scripts/eastern_kingdoms/burning_steppes/molten_core/<file>.cpp (mc/...); creature types come from the world DB dump.
    void RegisterMoltenCore(RaidRules& r)
    {
        // Encounter order (progression advice): Lucifron, Magmadar, Gehennas, Garr, Shazzrah, Geddon, Sulfuron, Golemagg, Majordomo, Ragnaros.
        r.bosses = { 12118, 11982, 12259, 12057, 12264, 12056, 12098, 11988, 12018, 11502 };

        // LUCIFRON - boss 12118
        // Lucifron's Curse doubles the mana cost of every spell and ability of everyone within 40 yards for 5 min (classic_boss_lucifron.cpp:31):
        // on the healers it is the difference between a sustained kill and an out-of-mana wipe, so it is urgent (healers and tanks first,
        // even while the raid is in an emergency), like Gehennas' Curse.
        r.dispel.push_back({ 19703, DispelKind::Curse, true });      // Lucifron's Curse (mc/boss_lucifron.cpp:19)
        r.dispel.push_back({ 19702, DispelKind::Magic, true });      // Impending Doom, 2000 shadow after 10s within 40 yd (mc/boss_lucifron.cpp:18); kind UNVERIFIED
        r.priority.push_back({ 12119, 4 });                    // Flamewaker Protector (MCRaidPolicy.h:100)
        r.controlledMember.push_back({ 20604 });               // Protector's Dominate Mind (vanilla_smartai, VMaNGOS spell list 121190)

        // GEHENNAS - boss 12259
        r.dispel.push_back({ 19716, DispelKind::Curse, true });      // Gehennas' Curse (mc/boss_gehennas.cpp:29)
        r.priority.push_back({ 11661, 4 });                    // Flamewaker (MCRaidPolicy.h:100)

        // SHAZZRAH - boss 12264
        r.dispel.push_back({ 19713, DispelKind::Curse });      // Shazzrah's Curse (mc/boss_shazzrah.cpp:30)
        r.purge.push_back({ 19714 });                   // Deaden Magic (mc/boss_shazzrah.cpp:31)
        // Arcane Explosion around him, and he blinks into the raid (Gate of Shazzrah 23138): casters and healers keep out of it.
        r.avoidEnemy.push_back({ 12264, 22.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 12264, 22.0f, Who::Healer });

        // GARR - boss 12057
        // Every Firesworn that dies hastens Garr by 9% (stacking Enrage 19516) and explodes with a knockback (mc/boss_garr.cpp:232-249);
        // after 6 minutes Garr detonates the adds himself (:158-176). So the adds are held, banished or ignored while Garr is burned,
        // and are never AoE'd down together. Correction: priority was 4 (above Garr), now 0 (below Garr's default 1).
        // NOTE: conflicts with MCRaidPolicy.h:100,113 ("Garr adds first"); the lead decides which policy is used.
        // Magma Shackles (19496) is a 60 % slow Garr puts on the whole raid, not a tank problem: removing it cost 330 Cleanse / Dispel Magic
        // casts in one fight and every healer's mana (MC Garr 2026-10-10: "aura 19496 on 40 members"). Nobody removes it.
        r.noDispel.push_back(19496);
        r.dispel.push_back({ 15732, DispelKind::Magic, false, 75 });  // Firesworn Immolate (mc/boss_garr.cpp:226); from ike3. Only once the carrier is hurt: 330 dispels in one fight drained every healer (2026-10-10)
        r.noAoe.push_back({ 12099 });                          // Firesworn: no area damage (mc/boss_garr.cpp:232-249)
        r.crowdControl.push_back({ 12099, CcKind::Banish, 0, 0, true }); // Firesworn is an elemental (type 4) (MoltenCoreDungeonActions.cpp:201)
        r.priority.push_back({ 12099, 0 });                    // see correction above
        // (removed) noAoe on Garr himself: nothing in the script justifies it.
        // A Firesworn that dies explodes (Massive Eruption 19497, radius 15, mc/boss_garr.cpp:232-249): the tank beside it dies to it
        // (MC Garr 2026-10-10: Kaido at 203 s) and casters standing 14-28 y out went with earlier wipes. Ranged and healers keep outside.
        r.avoidEnemy.push_back({ 12099, 17.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 12099, 17.0f, Who::Healer });

        // BARON GEDDON - boss 12056
        r.dispel.push_back({ 19659, DispelKind::Magic });      // Ignite Mana (mc/boss_baron_geddon.cpp:30); kind UNVERIFIED

        // GOLEMAGG - boss 11988
        // Core Ragers (11672) are set to full health instead of dying when they drop below 50% and only die with Golemagg
        // (mc/boss_golemagg.cpp:275-284, 113): never a target.
        r.priority.push_back({ 11672, -1 });                   // Core Rager (mc/boss_golemagg.cpp:275-284)
        r.noAoe.push_back({ 11672 });                          // no AoE on the Ragers: they heal to full at 50% and only die with Golemagg (ike3)

        // SULFURON HARBINGER - boss 12098
        r.priority.push_back({ 11662, 5 });                    // Flamewaker Priest (MCRaidPolicy.h:99)
        // Inspire 19779: Sulfuron buffs himself and his priests (mc/classic_boss_sulfuron_harbinger.cpp:95-109), a magic buff the priests and shamans take off.
        r.purge.push_back({ 19779 });
        r.interruptFirst.push_back(19775);
        r.interruptFirst.push_back(20603);   // Flamewaker Healer: Shadow Shock (VMaNGOS creature_spells 116630; it has no heal)                     // Flamewaker Priest: Dark Mending, the heal on Sulfuron and his priests (spell list 116620)
        r.dispel.push_back({ 19776, DispelKind::Magic });      // Flamewaker Priest: Shadow Word: Pain (vanilla_smartai, VMaNGOS spell list 116620)
        r.dispel.push_back({ 20294, DispelKind::Magic });      // Flamewaker Priest: Immolate (same list)

        // MAJORDOMO EXECUTUS - boss 12018
        // He surrenders when the adds are dead and cannot be killed (mc/boss_majordomo_executus.cpp:335,379): never a target.
        // Every 30s one of Magic Reflection / Damage Shield is cast on ALL living adds (:485-501): dispel them off before attacking.
        r.priority.push_back({ 12018, -1 });                   // Majordomo Executus (mc/boss_majordomo_executus.cpp:335)
        // One plan (n71 wipe): one tank holds Majordomo, away from the raid, and the other tanks take the four Elites. Mages sheep up to
        // three Healers; the raid kills Healers first, then Elites (the owner's marks first). Casters stop on Magic Reflection, melee on
        // Damage Shield. He surrenders when all eight adds are dead.
        // The adds' casts go first for the kickers: Shadow Bolt (Healer) and Fireball (Elite) were interrupted four times in 95 s (n38).
        r.interruptFirst.push_back(21077);
        r.interruptFirst.push_back(20420);
        // Flamewaker Elite: Blast Wave 20229 hits everyone within about 10 yards for 1100-1300 (n38: seven raiders dead in 3 s, hunters,
        // rogues, a healer and a warrior beside the tanks holding the Elites). Casters, hunters and healers keep 13 yards from them.
        r.avoidEnemy.push_back({ 11664, 13.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 11664, 13.0f, Who::Healer });
        r.tankOrder.push_back(12018);
        r.tankOrder.push_back(11664);
        r.tankOrder.push_back(11663);
        // The brain already refuses to hit an add under either shield (Raid::kMagicReflection / kDamageShield) but nothing took
        // them off, so the raid stood idle for up to 30 s per cast. Priests and shamans purge them (Election::Assign purge).
        r.purge.push_back({ 20619 });                          // Magic Reflection (classic_boss_majordomo_executus.cpp:50)
        r.purge.push_back({ 21075 });                          // Damage Shield (classic_boss_majordomo_executus.cpp:51)
        r.priority.push_back({ 11664, 4 });                    // Flamewaker Elite (MCRaidPolicy.h:100)
        r.priority.push_back({ 11663, 5 });                    // Flamewaker Healer (MCRaidPolicy.h:99)
        // No raid-chosen sheep here: the owner marks the Healers and gives each to a mage; skull moves onto them in turn (n71).

        // RAGNAROS - boss 11502
        // Submerged phase: it ends early when every Son of Flame is banished (mc/boss_ragnaros.cpp:347-363).
        r.crowdControl.push_back({ 12143, CcKind::Banish, 0 }); // Son of Flame (mc/boss_ragnaros.cpp:56,355-363)
        r.priority.push_back({ 12143, 7 });                    // Son of Flame priority (MCRaidPolicy.h:97)
        r.priority.push_back({ 12265, 6 });                    // Lava Spawn (MCRaidPolicy.h:98)
        // Firelord: summons a Lava Spawn every 15-20 s (classic_molten_core.cpp:420-426) and Soul Burns a random raider every 3-4 s, so
        // the pack dies Firelord by Firelord, ahead of its Spawns (the off tanks hold the Spawns and the second Firelord). The old
        // order (Spawn 6, Firelord 3) chased an endless Spawn stream while the Firelords kept casting (06:30 wipe, 5 Spawns up).
        r.priority.push_back({ 11668, 8 });
        // The Spawns attack a random raider each (classic_molten_core.cpp:411): they are burnt down, not tanked. A tank that went to
        // the nearest loose Spawn left the Lava Annihilator (melee, 14 deaths in the 14:56 wipe) and the Firelord loose.
        r.noTank.push_back(12265);

        // TRASH
        // Lava Surger is an elemental (type 4) and its script explicitly stops its timers while banished (mc/molten_core.cpp:524).
        r.crowdControl.push_back({ 12101, CcKind::Banish, 0 }); // Lava Surger (mc/molten_core.cpp:489-540)

        // MAGMADAR - boss 11982 (no boss script in the source tree: the id is from the bot code, not a script)
        // Frenzy 19451: Tranquilizing Shot is the PvE coordinator's (PlanTranq, any aura the shot removes). Magmadar Frenzy tranquilize (MoltenCoreDungeonActions.cpp:191)
        r.dispel.push_back({ 19408, DispelKind::Magic, true });      // Panic, his AoE fear: dispel type Magic, mechanic Fear (client DB2 checked 2026-10-08); urgent = tanks and healers cleansed first

        // TRASH (spell data: classic spell tooltips/effects; casts: classic_molten_core.cpp and
        // sql/custom/world/2026_09_28_15_world_vanilla_smartai.sql).
        // Flameguard: Cone of Fire 19630, 1388-1612 fire in a 20-yard frontal cone (VMaNGOS spell list 116670). Was 12 yards.
        r.avoidFrontal.push_back({ 11667, 24.0f, 50.0f, Who::NonTank });
        // Ancient Core Hound: the same Cone of Fire 19630 (classic_molten_core.cpp:180).
        r.avoidFrontal.push_back({ 11673, 24.0f, 50.0f, Who::NonTank });
        // Lava Elemental: Pyroclast Barrage 19641, 1500 fire plus a 6 s stun in a 20-yard frontal cone (VMaNGOS spell list 120760).
        r.avoidFrontal.push_back({ 12076, 24.0f, 50.0f, Who::NonTank });
        // Lava Reaver: Cleave 19642 hits the victim and its nearest allies (VMaNGOS EventAI 1210002): nobody but the tank in front.
        r.avoidFrontal.push_back({ 12100, 10.0f, 70.0f, Who::NonTank });
        // Molten Giant: Smash 18944, 810-990 physical to everybody within 10 yards every 7-8 s (VMaNGOS spell list 116580), and
        // Knock Away 18945 on the tank every 15 s (EventAI 1165802) throws it back toward whoever stands behind it. Casters,
        // hunters and healers keep 15 yards (10 + both combat reaches).
        r.avoidEnemy.push_back({ 11658, 15.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 11658, 15.0f, Who::Healer });
        // Molten Destroyer: Massive Tremor 19129, 900-1100 physical plus a 2 s interrupt within 13 yards (VMaNGOS spell list
        // 116590). Knockdown 20276 stuns the tank and its 4 nearest allies (EventAI 1165901). Casters and healers keep 18 yards.
        r.avoidEnemy.push_back({ 11659, 18.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 11659, 18.0f, Who::Healer });
        // Firelord: Soul Burn 19393 on a random target every 3-4 s (classic_molten_core.cpp:431-437): 4000 fire over 16 s, silence
        // and -50% physical damage. Magic. A silenced healer is a dead tank, so it is dispelled. Its Lava Spawns (12265) are
        // killed first (priority 6 above).
        r.dispel.push_back({ 19393, DispelKind::Magic, true });
        // Ancient Core Hound: one random 45-yard raid-wide debuff, fixed per hound (classic_molten_core.cpp:145-190).
        r.dispel.push_back({ 19367, DispelKind::Magic });      // Withering Heat: -35% maximum health for 15 min
        r.dispel.push_back({ 19365, DispelKind::Magic });      // Ancient Dread: -50% casting speed, attacks 100% slower, 15 min
        r.dispel.push_back({ 19372, DispelKind::Curse });      // Ancient Hysteria: -50% spirit and intellect, 15 min
        // Not dispelled: Ancient Despair 19369 (5 s disorient, gone before most dispels land) and Cauterizing Flames 19366
        // (-200 fire resistance): 40 dispels each would cost the healers more than the debuff does. Ground Stomp 19364 is a stun.
        // Core Hound packs: a hound that would die feigns death at 1 hp under Pacify 19951; 10 s later it rises at full health
        // unless every hound of the pack is down (classic_molten_core.cpp:239-370). Spread the damage, finish them together.
        r.killTogether.push_back({ 11671, 19951, 15.0f });
    }

} // namespace Playerbot::Raid
