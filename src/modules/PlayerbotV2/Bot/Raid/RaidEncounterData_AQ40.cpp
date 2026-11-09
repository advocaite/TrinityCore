#include "RaidEncounters.h"

// Temple of Ahn'Qiraj (map 531), 40-player. New for Forever: Karrz's ike3 bots had no AQ40 data.
// Sources: Forever's Classic ports of the VMaNGOS scripts, src/server/scripts/Custom/Classic/instances/temple_of_ahnqiraj
// (abbreviated aq40/<file>.cpp below), and the trash spell lists in sql/custom/world/2026_09_28_15_world_vanilla_smartai.sql.

namespace Playerbot::Raid {

    void RegisterTempleOfAhnQiraj(RaidRules& r)
    {
        r.name = "Temple of Ahn'Qiraj";

        // Kick-first casts (vanilla spell ids): Qiraji Brainwasher Cause Insanity 26079 (mind control), Princess Yauj's Great
        // Heal 25807 on the trio (classic_boss_bug_trio.cpp SPELL_YAUJ_HEAL), Skeram's Arcane Explosion 26192 (30 yd, only when more
        // than 4 stand in his melee range, classic_boss_skeram.cpp:94), Mana Burn 26046, Obsidian Nullifier / Destroyer Drain Mana
        // 25671 / 25755, the Anubisath Warder's Silence 26069, Fear 26070 and Entangling Roots 26071 (1.5 s casts), Mind Flay 26143.
        r.interruptFirst.push_back(26079);
        r.interruptFirst.push_back(25807);
        r.interruptFirst.push_back(26192);
        r.interruptFirst.push_back(26046);
        r.interruptFirst.push_back(25671);
        r.interruptFirst.push_back(25755);
        r.interruptFirst.push_back(26069);
        r.interruptFirst.push_back(26070);
        r.interruptFirst.push_back(26071);
        r.interruptFirst.push_back(26143);

        // One entry per encounter, in the order of the clear and index for index with the script's progression waypoints: the
        // owed boss is bosses[encounters done] and is steered to by the waypoint at that index. The bug trio and the twins are one
        // encounter each (temple_of_ahnqiraj.h DATA_BUG_TRIO / DATA_TWIN_EMPERORS), so Vem stands for the trio and Vek'nilash for
        // the twins; Ouro has no static spawn (raised from his mound) and no waypoint. With all twelve listed, the waypoints
        // answered the wrong boss from the third on. Kri, Yauj, Vek'lor and Ouro are boss rank, so RaidBrain still reads them as
        // bosses (detail::IsBoss).
        r.bosses = { 15263,                 // The Prophet Skeram
                     15544,                 // Bug trio (Vem, with Kri and Yauj)
                     15516, 15510, 15299,   // Sartura, Fankriss, Viscidus
                     15509, 15275,          // Huhuran, Twin Emperors (Vek'nilash, with Vek'lor)
                     15727 };               // C'Thun

        // ---------------------------------------------------------------------------------------------------------------
        // TRASH
        // Qiraji Mindslayer 15246 / Brainwasher 15247: Cause Insanity 26079 controls a raider (VMaNGOS EventAI 1524601, list 152470).
        r.controlledMember.push_back({ 26079 });
        r.priority.push_back({ 15246, 4 });
        r.priority.push_back({ 15247, 4 });
        // Anubisath Defender 15277: Plague 26556 on a random raider (EventAI 1527705): the carrier leaves, the raid keeps clear.
        r.carrierRun.push_back({ 26556, 15.0f, Who::NonTank });
        r.avoidCarrier.push_back({ 26556, 12.0f, Who::NonTank });
        // Trash mechanics (casts: sql/custom/world/2026_09_28_15_world_vanilla_smartai.sql "VMaNGOS spell list / EventAI" rows;
        // radii, durations and dispel types: classic spell data).
        // Qiraji Mindslayer / Brainwasher: Mana Burn 26049 / 26046 burns everybody's mana within 30 yd (the Mindslayer's also fears):
        // healers heal from outside it.
        r.avoidEnemy.push_back({ 15246, 32.0f, Who::Healer });
        r.avoidEnemy.push_back({ 15247, 32.0f, Who::Healer });
        // Anubisath Warder 15311: AoE Silence 26069, Fear 26070, Dust Cloud 26072 (-85% hit chance) and Entangling Roots 26071, all
        // magic, 45 yd. The silence and the fear take healers out, the dust cloud the tanks' threat.
        r.dispel.push_back({ 26069, DispelKind::Magic });
        r.dispel.push_back({ 26070, DispelKind::Magic });
        r.dispel.push_back({ 26072, DispelKind::Magic });
        // Qiraji Lasher 15249: Whirlwind 26038 (8 s aura) and Knockback 26027 (10 yd). Qiraji Slayer 15250: Whirlwind 13736 (2 s
        // channel). Non-tanks leave while they spin.
        r.avoidEnemyAura.push_back({ 15249, 26038, 10.0f, Who::NonTank });
        r.avoidEnemyAura.push_back({ 15250, 13736, 10.0f, Who::NonTank });
        // Qiraji Slayer: Frenzy 26041 (enrage, +150% attack speed, silences around it): hunters tranquilize.
        // Frenzy 26041: Tranquilizing Shot is the PvE coordinator's (PlanTranq, any aura the shot removes).
        // Qiraji Champion 15252: Vengeance 25164 (magic self buff, +50% attack speed, +40% run speed): purged. Knock Away 11130 and
        // Intimidating Shout 19134 around it: casters and healers keep 13 yards.
        r.purge.push_back({ 25164 });
        r.avoidEnemy.push_back({ 15252, 13.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 15252, 13.0f, Who::Healer });
        // Vekniss Guardian 15233: Impale 26025, a knockback and a bleed within 10 yd.
        r.avoidEnemy.push_back({ 15233, 13.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 15233, 13.0f, Who::Healer });
        // Vekniss Hive Crawler 15240: Poison Bolt 26601 (2250 + 500 a second for 10 s) and Mind-numbing Poison 25810 (-50% cast speed).
        r.dispel.push_back({ 26601, DispelKind::Poison });
        r.dispel.push_back({ 25810, DispelKind::Poison });
        // Obsidian Eradicator 15262 / Obsidian Nullifier 15312: drain mana until full, then Shock Blast 26458 (6563 nature, 40 yd) /
        // Nullify 26552 (stun, 45 yd). Nothing to dodge: they die before they fill up.
        r.priority.push_back({ 15262, 5 });
        r.priority.push_back({ 15312, 5 });
        // Hive'Zara Tail Lasher 15336: Poison 25645 (vanilla data: poison).
        r.dispel.push_back({ 25645, DispelKind::Poison });
        // Anubisath Defender 15277: Meteor 26558 (14250 fire split among everybody within 8 yd of the target) and Shadow Storm
        // 26555 (only hits between 25 and 45 yd). The raid stacks on it: the more share the Meteor, the less each takes.
        r.stayNear.push_back({ 15277, 8.0f, Who::NonTank });
        // Anubisath Sentinel 15264: Shadow Storm 26546 (only between 25 and 45 yd) when it rolled that power. Stay inside 25.
        // Its reflect is covered by the school hints (Raid::Brain::SchoolsFor).
        r.stayNear.push_back({ 15264, 22.0f, Who::NonTank });

        // ---------------------------------------------------------------------------------------------------------------
        // THE PROPHET SKERAM (15263): True Fulfillment 785 mind-controls a raider (aq40/classic_boss_skeram.cpp); Arcane
        // Explosion 26192 around him (kicked first, see above). His images share his entry. He Blinks every 10-18 s
        // (classic_boss_skeram.cpp:340-343) and lands on whoever he likes: a tank taunts him back.
        r.controlledMember.push_back({ 785 });
        r.avoidEnemy.push_back({ 15263, 15.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 15263, 15.0f, Who::Healer });
        r.retaunt.push_back(15263);

        // ---------------------------------------------------------------------------------------------------------------
        // BUG TRIO (aq40/classic_boss_bug_trio.cpp): Yauj heals the others (25807, kicked) and fears (19408); Kri's Toxic Volley 25812 is a
        // poison. Kill order Yauj, Kri, Vem; Yauj's brood (15621) comes out when she dies.
        r.priority.push_back({ 15543, 4 });     // Princess Yauj
        r.priority.push_back({ 15621, 5 });     // Yauj Brood
        r.priority.push_back({ 15511, 3 });     // Lord Kri
        r.priority.push_back({ 15544, 2 });     // Vem
        r.dispel.push_back({ 25812, DispelKind::Poison });
        r.dispel.push_back({ 19408, DispelKind::Magic });
        // Yauj's fear and Vem's Charge 26561 / Knock Away leave the two on whoever is left: a tank taunts them back.
        r.retaunt.push_back(15543);
        r.retaunt.push_back(15544);
        // Kri leaves a Toxic Vapors cloud 25786 where he dies (classic_boss_bug_trio.cpp:261-265): keep off his corpse.
        r.avoidCreature.push_back({ 15511, 12.0f, Who::Anyone, true });

        // ---------------------------------------------------------------------------------------------------------------
        // BATTLEGUARD SARTURA (15516) and her Royal Guards (15984): everyone but the tanks leaves their Whirlwinds
        // (26083 / 26038, aq40/classic_boss_sartura.cpp). Guards first.
        r.avoidEnemyAura.push_back({ 15516, 26083, 10.0f, Who::NonTank });
        r.avoidEnemyAura.push_back({ 15984, 26038, 10.0f, Who::NonTank });
        r.priority.push_back({ 15984, 3 });
        // Casters and healers never stand in Whirlwind reach of any of them (the step-out above is for the melee).
        r.avoidEnemy.push_back({ 15516, 11.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 15516, 11.0f, Who::Healer });
        r.avoidEnemy.push_back({ 15984, 9.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 15984, 9.0f, Who::Healer });

        // ---------------------------------------------------------------------------------------------------------------
        // FANKRISS THE UNYIELDING (15510): Mortal Wound 25646 on the tank (aq40/classic_boss_fankriss.cpp): swap at 5. Spawns of
        // Fankriss (15630) enrage if left alone.
        r.tankSwap.push_back({ 15510, 25646, 5 });
        r.priority.push_back({ 15630, 6 });
        // Vekniss Hatchlings (15962) come 2-4 at a time out of the webs he ports raiders into (classic_boss_fankriss.cpp:351): killed
        // before the spawns.
        r.priority.push_back({ 15962, 5 });

        // ---------------------------------------------------------------------------------------------------------------
        // VISCIDUS (15299): Poison Bolt Volley 25991 (aq40/classic_boss_viscidus.cpp). Globs (15667) walk back to him when he
        // shatters: kill them on the way.
        r.dispel.push_back({ 25991, DispelKind::Poison });
        r.priority.push_back({ 15667, 5 });
        // Toxin cloud: a trigger (15922) summoned under a random player every 30-40 s keeps casting 25989 (:558-584).
        r.avoidCreature.push_back({ 15922, 10.0f, Who::Anyone });
        // Frost hits to freeze him and the stomach/Dark Glare of C'Thun are scripted in RaidMechanics_AQ40.cpp.

        // ---------------------------------------------------------------------------------------------------------------
        // PRINCESS HUHURAN (15509): Frenzy 26051 is tranquilized; Noxious Poison 26053 silences (aq40/classic_boss_huhuran.cpp).
        // Wyvern Sting 26180 is not dispelled: removing it deals the damage at once.
        // Frenzy 26051: Tranquilizing Shot is the PvE coordinator's (PlanTranq, any aura the shot removes).
        r.dispel.push_back({ 26053, DispelKind::Poison });
        r.noDispel.push_back(26180);
        // Her Poison Bolt Volley and Acid Spit are nature: Greater Nature Protection for everyone.
        r.resistPotion.push_back({ 15509, 13458, Who::Anyone });
        // Noxious Poison lands on everybody within 6 yd of its target (vanilla data, radius index 18): the ranged and the healers keep 8 apart.
        r.spread.push_back({ 15509, 8.0f, Who::Ranged });
        r.spread.push_back({ 15509, 8.0f, Who::Healer });
        // Poison Bolt Volley 26052 (1,500-2,500 nature on the 15 nearest) is not only a berserk move: vanilla Frenzy 26051, which she
        // casts every 25-35 s from the pull, carries it as a periodic trigger every 4 s (vmangos spell data, effect 3), and Berserk 26068
        // every 2 s below 31% or after 5 min. The ranged and the healers stand 20 yards off for the whole fight so the 15 nearest are
        // the melee and the tanks (123 run: healers at 10-12 y died to the volley at 87% boss health, 4 s apart).
        r.avoidEnemy.push_back({ 15509, 20.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 15509, 20.0f, Who::Healer });

        // ---------------------------------------------------------------------------------------------------------------
        // TWIN EMPERORS (aq40/classic_boss_twinemperors.cpp): Vek'nilash (15275) is immune to magic, Vek'lor (15276) to physical
        // damage, so physical DPS (melee and hunters) only fight Vek'nilash and casters only Vek'lor. Vek'lor's Blizzard 26607 is left.
        r.onlyAttackers.push_back({ 15275, Who::Physical });
        r.onlyAttackers.push_back({ 15276, Who::Caster });
        r.avoidGroundEffect.push_back({ 15276, 26607, 2.0f, Who::Anyone });
        // They swap places every 30-40 s (SPELL_TWIN_TELEPORT 799/800): after each teleport a tank taunts the twin that landed on him.
        r.retaunt.push_back(15275);
        r.retaunt.push_back(15276);
        // Vek'lor's Arcane Burst 568 hits and throws everybody within 10 yd (vanilla data, radius index 13): his casters keep 13.
        r.avoidEnemy.push_back({ 15276, 13.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 15276, 13.0f, Who::Healer });

        // ---------------------------------------------------------------------------------------------------------------
        // OURO (15517): Sand Blast 26102 to the front, Sweep 26103 around him; while submerged he chases from under a Dirt Mound
        // (15712); scarabs (15718) come out of the mounds (aq40/classic_boss_ouro.cpp).
        r.avoidFrontal.push_back({ 15517, 30.0f, 45.0f, Who::NonTank });
        r.avoidCreature.push_back({ 15712, 10.0f, Who::Anyone });
        r.priority.push_back({ 15718, 3 });
        // He comes back up on whoever is nearest: a tank taunts him back.
        r.retaunt.push_back(15517);

        // ---------------------------------------------------------------------------------------------------------------
        // C'THUN (15589 Eye of C'Thun, 15727 C'Thun; aq40/classic_boss_cthun.cpp): Eye Beam 26134 chains between players
        // standing close, so everyone spreads; the tentacles die first (eye tentacles cast Mind Flay).
        r.spread.push_back({ 15589, 10.0f, Who::Anyone });
        r.spread.push_back({ 15727, 10.0f, Who::Ranged });
        r.spread.push_back({ 15727, 10.0f, Who::Healer });
        r.priority.push_back({ 15726, 5 });     // Eye Tentacle
        r.priority.push_back({ 15725, 4 });     // Claw Tentacle
        r.priority.push_back({ 15334, 6 });     // Giant Eye Tentacle
        r.priority.push_back({ 15728, 5 });     // Giant Claw Tentacle
        // Ground Rupture when a claw tentacle comes up (classic_boss_cthun.cpp:83-86): 26139 within 10 yd of a Claw Tentacle, 26478
        // within 6 yd of a Giant Claw Tentacle (vanilla data, radius indexes 13 / 18). Casters and healers keep out of it.
        r.avoidEnemy.push_back({ 15725, 13.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 15725, 13.0f, Who::Healer });
        r.avoidEnemy.push_back({ 15728, 9.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 15728, 9.0f, Who::Healer });

        // Not ruled: Twin Emperors' teleport tank pickup (the PvE coordinator's per-tank assignment re-picks after the swap).
    }

} // namespace Playerbot::Raid
