#include "RaidEncounters.h"

// Naxxramas (map 533), 40-player Classic version. New for Forever: Karrz's ike3 bots had no Naxxramas data.
// Sources: Forever's Classic ports of the VMaNGOS scripts, src/server/scripts/Custom/Classic/instances/naxxramas
// (abbreviated naxx/<file>.cpp below). Dispel kinds are only a fallback: the brain reads the real dispel type from
// the spell data. Fights that need a mechanic the bots cannot do are listed at the end.

namespace Playerbot::Raid {

    void RegisterNaxxramas(RaidRules& r)
    {
        r.name = "Naxxramas";

        // Kick-first casts (VMaNGOS creature_spells): Mad Scientist Mana Burn 28301, Living Monstrosity Chain Lightning 28293,
        // Acolyte / Necropolis Acolyte Shadow Bolt Volley 28448, Unholy Staff Polymorph 29848.
        r.interruptFirst.push_back(28301);
        r.interruptFirst.push_back(28293);
        r.interruptFirst.push_back(28448);
        r.interruptFirst.push_back(29848);

        // Encounter order (progression advice): Arachnid, Plague, Military, Construct quarter, then Sapphiron and Kel'Thuzad.
        r.bosses = { 15956, 15953, 15952,          // Anub'Rekhan, Faerlina, Maexxna
                     15954, 15936, 16011,          // Noth, Heigan, Loatheb
                     16061, 16060, 16062,          // Razuvious, Gothik, Four Horsemen (Mograine)
                     16028, 15931, 15932, 15928,   // Patchwerk, Grobbulus, Gluth, Thaddius
                     15989, 15990 };               // Sapphiron, Kel'Thuzad

        // ---------------------------------------------------------------------------------------------------------------
        // ANUB'REKHAN (15956): Locust Swarm 28785 is a self buff that pulses damage around him (naxx/classic_boss_anubrekhan.cpp);
        // the tank kites him, everyone else keeps away until it ends. Radius 30 is the usual swarm range (UNVERIFIED).
        r.avoidEnemyAura.push_back({ 15956, 28785, 30.0f, Who::NonTank });
        r.priority.push_back({ 16573, 3 });     // Crypt Guard: Acid Spit / Web / Cleave (classic_boss_anubrekhan.cpp:65)
        r.priority.push_back({ 16698, 2 });     // Corpse Scarab: spawned from corpses (classic_instance_naxxramas.cpp:1237)

        // ---------------------------------------------------------------------------------------------------------------
        // GRAND WIDOW FAERLINA (15953): Poison Bolt Volley 28796, Rain of Fire 28794 (naxx/classic_boss_faerlina.cpp).
        r.dispel.push_back({ 28796, DispelKind::Poison });
        r.avoidGroundEffect.push_back({ 15953, 28794, 3.0f, Who::Anyone });
        r.priority.push_back({ 16505, 3 });     // Naxxramas Follower (classic_boss_faerlina.cpp:53): silence, charge
        // Worshippers are the enrage counter: after each Enrage 28798 a priest mind-controls one and sacrifices it with Widow's
        // Embrace 28732 on her (RaidMechanics_Naxx.cpp). They have no bot logic; never a DPS target.
        r.priority.push_back({ 16506, -1 });

        // ---------------------------------------------------------------------------------------------------------------
        // MAEXXNA (15952): Web Wrap cocoons (16486) hold players until killed; Necrotic Poison 28776 on the tank (-90% healing,
        // "Dispelllable" per the script comment); spiderlings 17055 (naxx/classic_boss_maexxna.cpp).
        r.priority.push_back({ 16486, 7 });     // Web Wrap: free the wrapped player
        r.priority.push_back({ 17055, 4 });     // Maexxna Spiderling
        r.dispel.push_back({ 28776, DispelKind::Poison });

        // ---------------------------------------------------------------------------------------------------------------
        // NOTH THE PLAGUEBRINGER (15954): Curse of the Plaguebringer 29213; balcony phase adds (naxx/classic_boss_noth.cpp).
        r.dispel.push_back({ 29213, DispelKind::Curse });
        r.priority.push_back({ 16983, 3 });     // Plagued Champion
        r.priority.push_back({ 16981, 3 });     // Plagued Guardian
        r.priority.push_back({ 16984, 2 });     // Plagued Warrior

        // ---------------------------------------------------------------------------------------------------------------
        // HEIGAN THE UNCLEAN (15936): Decrepit Fever 29998 (naxx/classic_boss_heigan.cpp). The eruption dance is scripted.
        r.dispel.push_back({ 29998, DispelKind::Disease });
        // Mana Burn 29310 every 3 s on mana users within 28 yd in the fight phase: RaidMechanics_Naxx.cpp keeps them out.

        // ---------------------------------------------------------------------------------------------------------------
        // LOATHEB (16011): spores 16286 give the killers a crit buff (naxx/classic_boss_loatheb.cpp:458-466).
        r.priority.push_back({ 16286, 4 });     // Spore
        // Corrupted Mind 29201 hands every priest/druid (29194), paladin (29196) and shaman (29198) a one-heal-per-minute
        // debuff (:511-546). The heals fail while it lasts, so the healers rotate on their own; it is never dispelled.
        r.noDispel.push_back(29194);
        r.noDispel.push_back(29196);
        r.noDispel.push_back(29198);

        // ---------------------------------------------------------------------------------------------------------------
        // INSTRUCTOR RAZUVIOUS (16061): priests mind-control the Deathknight Understudies (16803), which taunt and tank him
        // (RaidMechanics_Naxx.cpp). The understudies must stay alive for that, so DPS never attack them; tanks hold the free ones.
        r.priority.push_back({ 16803, -1 });

        // ---------------------------------------------------------------------------------------------------------------
        // GOTHIK THE HARVESTER (16060): waves on both sides (naxx/classic_naxxramas.h:114-120). Riders hit hardest.
        r.priority.push_back({ 16126, 4 });     // Unrelenting Rider
        r.priority.push_back({ 16125, 3 });     // Unrelenting Deathknight
        r.priority.push_back({ 16124, 2 });     // Unrelenting Trainee
        r.priority.push_back({ 16150, 4 });     // Spectral Rider
        r.priority.push_back({ 16149, 3 });     // Spectral Horse
        r.priority.push_back({ 16148, 3 });     // Spectral Deathknight
        r.priority.push_back({ 16127, 2 });     // Spectral Trainee

        // ---------------------------------------------------------------------------------------------------------------
        // THE FOUR HORSEMEN (16062 Mograine, 16063 Zeliek, 16064 Thane, 16065 Blaumeux): Blaumeux's Void Zones are creatures
        // 16697 (naxx/classic_boss_four_horsemen.cpp:59,501) pulsing Consumption 28865 (radius index 15 = 3 yd, 3,960 shadow);
        // 5.5 yd = 3 yd plus combat reach plus a step. Zeliek's Holy Wrath chains between close players.
        r.avoidCreature.push_back({ 16697, 5.5f, Who::Anyone });
        r.spread.push_back({ 16063, 10.0f, Who::Ranged });
        r.spread.push_back({ 16063, 10.0f, Who::Healer });
        // Tank rotation: every mark (cast every 12 s, classic_boss_four_horsemen.cpp:407) stacks, and from the 3rd stack it hurts
        // (1000 / 3000 / 1000 x stacks). At 3 marks the tank hands its horseman to a tank without that mark, who walks over from
        // its own corner if it is out of taunt range, and the old tank moves on to a horseman whose mark it does not carry.
        r.tankSwap.push_back({ 16062, 28834, 3 });  // Highlord Mograine: Mark of Mograine
        r.tankSwap.push_back({ 16064, 28832, 3 });  // Thane Korth'azz: Mark of Korth'azz
        // Thane's Meteor 28884 (12825 fire split among everybody within 8 yd of a random player in 20 yd of him, every 12-15 s,
        // classic_boss_four_horsemen.cpp:684-695): his melee stack on him so it is shared.
        r.stayNear.push_back({ 16064, 8.0f, Who::Melee });
        r.tankSwap.push_back({ 16065, 28833, 3 });  // Lady Blaumeux: Mark of Blaumeux
        r.tankSwap.push_back({ 16063, 28835, 3 });  // Sir Zeliek: Mark of Zeliek

        // ---------------------------------------------------------------------------------------------------------------
        // PATCHWERK (16028): Hateful Strike hits the highest-health player in melee among his top four on threat, never the main
        // tank while others are in melee (naxx/classic_boss_patchwerk.cpp DoHatefulStrike). Three off tanks stay on him in melee.
        r.meleeSoak.push_back({ 16028, Who::OffTank, 3 });

        // ---------------------------------------------------------------------------------------------------------------
        // GROBBULUS (15931): Mutating Injection 28169 explodes into a poison cloud (creature 16363, triggers 28241 every
        // second); Slime Spray 28157 to the front spawns Fallout Slimes 16290 (naxx/classic_boss_grobbulus.cpp).
        // Dispelling it early only halves the explosion, the poison cloud spawns either way where the carrier stands
        // (classic_boss_grobbulus.cpp:224-257), so nobody dispels it in the middle of the raid: the carrier runs out.
        r.noDispel.push_back(28169);
        r.carrierRun.push_back({ 28169, 15.0f, Who::NonTank });
        r.avoidCarrier.push_back({ 28169, 10.0f, Who::NonTank });
        r.avoidCreature.push_back({ 16363, 9.0f, Who::Anyone });
        r.avoidFrontal.push_back({ 15931, 20.0f, 50.0f, Who::NonTank });
        r.priority.push_back({ 16290, 3 });     // Fallout Slime

        // ---------------------------------------------------------------------------------------------------------------
        // GLUTH (15932): Mortal Wound 25646 stacks -10% healing on the tank (naxx/classic_boss_gluth.cpp): swap at 5.
        // Zombie Chows (16360) heal him when he eats them; Decimate leaves them at 5% for the AoE.
        r.tankSwap.push_back({ 15932, 25646, 5 });
        // Frenzy 28371: Tranquilizing Shot is the PvE coordinator's (PlanTranq, any aura the shot removes). Frenzy every 10 s: tranquilized (classic_boss_gluth.cpp:221-232)
        // Zombie Chows heal him 5% each when he eats one within 15 yd (:290-312): they die before they reach him.
        r.priority.push_back({ 16360, 5 });     // Zombie Chow

        // ---------------------------------------------------------------------------------------------------------------
        // THADDIUS (15928): Polarity Shift gives Positive Charge 28059 / Negative Charge 28084 (naxx/classic_boss_thaddius.cpp).
        // Same charges stack up together, so each charge group stands on its own side of him. Sides follow the line through
        // Stalagg's and Feugen's platforms (ClassicThadAddPositions, :154-157): bearing 2.36 rad points at one, -0.78 at the other.
        r.standByAura.push_back({ 15928, 28059, 2.36f, 9.0f, 4.0f });
        r.standByAura.push_back({ 15928, 28084, -0.78f, 9.0f, 4.0f });

        // ---------------------------------------------------------------------------------------------------------------
        // SAPPHIRON (15989): Life Drain 28542, Blizzard creatures 16474 that drift around (naxx/classic_boss_sapphiron.cpp),
        // Cleave / Tail Sweep front and back.
        r.dispel.push_back({ 28542, DispelKind::Curse });
        r.avoidCreature.push_back({ 16474, 8.0f, Who::Anyone });
        r.avoidFrontal.push_back({ 15989, 25.0f, 50.0f, Who::NonTank });
        r.avoidFrontal.push_back({ 15989, 20.0f, 45.0f, Who::NonTank, 180.0f });   // Tail Sweep 15847 behind him

        // ---------------------------------------------------------------------------------------------------------------
        // KEL'THUZAD (15990): Detonate Mana 27819 on a mana user (the carrier runs, others keep clear), Shadow Fissure 27810
        // summons Shadow Fissure 16129 (spell 27810 effect 41, misc 16129). The fissure carries no aura: 3 s after it appears it
        // casts Void Blast 27812 (62,500 shadow, radius index 8 = 5 yd around itself) and despawns 2.25 s later
        // (classic_mob_shadow_fissure, naxx/classic_boss_kelthuzad.cpp). 7.5 yd = 5 yd plus the target's combat reach plus a step.
        // Frost Bolt Nova punishes stacking (naxx/classic_boss_kelthuzad.cpp:85-95).
        r.carrierRun.push_back({ 27819, 15.0f, Who::ManaUser });
        r.avoidCarrier.push_back({ 27819, 12.0f, Who::Anyone });
        r.avoidCreature.push_back({ 16129, 7.5f, Who::Anyone });
        r.spread.push_back({ 15990, 10.0f, Who::Ranged });
        r.spread.push_back({ 15990, 10.0f, Who::Healer });
        // Phase 1 adds (naxx/classic_naxxramas.h:123-126). Guardians of Icecrown (phase 3) are shackled.
        r.priority.push_back({ 16429, 4 });     // Soul Weaver
        r.priority.push_back({ 16427, 3 });     // Soldier of the Frozen Wastes
        r.priority.push_back({ 16428, 2 });     // Unstoppable Abomination
        // Shackle at most 3 Guardians: a 4th frees them all (classic_boss_kelthuzad.cpp:1027-1100).
        r.crowdControl.push_back({ 16441, CcKind::Shackle, 0, 3 });
        // Chains of Kel'Thuzad 28410 mind-controls a raider (:690-704): sheep or stun, never kill.
        r.controlledMember.push_back({ 28410 });

        // ---------------------------------------------------------------------------------------------------------------
        // TRASH (casts: sql/custom/world/2026_09_28_15_world_vanilla_smartai.sql, classic_instance_naxxramas.cpp; names, radii,
        // durations and dispel types: classic spell data). The dispel kind is a fallback, the brain reads the real type.
        r.dispel.push_back({ 28431, DispelKind::Poison });   // Poison Charge, 2500 every 3 s for 9 s: Venom/Necro Stalker (SAI 15976, 16453)
        r.dispel.push_back({ 29848, DispelKind::Magic });    // Polymorph, Unholy Staff 16215
        // (removed) Aura of Agony 28413 (Deathknight Cavalier/Vindicator): the spell data lists no dispel type, a dispel never lands.
        r.dispel.push_back({ 30113, DispelKind::Disease });  // Putrid Bite, +120 damage taken for 30 s: Plagued Bat 16037 on the tank
        r.dispel.push_back({ 28311, DispelKind::Poison });   // Slime Bolt, 475 every 5 s: Stitched Spewer 16025
        r.dispel.push_back({ 28440, DispelKind::Curse });    // Veil of Shadow, -75% healing taken for 6 s: Dread Creeper 15974
        r.dispel.push_back({ 28350, DispelKind::Curse });    // Veil of Darkness, cannot be healed for 7 s: Deathknight 16146
        // Construct wing.
        // Patchwork Golem 16017: War Stomp 27758, weapon + 936 and a 5 s stun within 15 yd; Cleave 27794 (2.5 s cast).
        r.avoidEnemy.push_back({ 16017, 18.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 16017, 18.0f, Who::Healer });
        r.avoidFrontal.push_back({ 16017, 10.0f, 70.0f, Who::NonTank });
        // Bile Retcher 16018: Bile Vomit 27807 on its victim, 1750 nature + DoT to everybody within 20 yd in front.
        r.avoidFrontal.push_back({ 16018, 24.0f, 50.0f, Who::NonTank });
        // Stitched Spewer 16025: Knockback 28405, 1850 physical and a knockback within 15 yd.
        r.avoidEnemy.push_back({ 16025, 18.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 16025, 18.0f, Who::Healer });
        // Embalming Slime 16024: Embalming Cloud 28322, 3515 nature within 8 yd of the slime.
        r.avoidEnemy.push_back({ 16024, 11.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 16024, 11.0f, Who::Healer });
        // Spider wing.
        // Carrion Spinner 15975: Poison Spray 30043, 1313 nature within 15 yd of the spider.
        r.avoidEnemy.push_back({ 15975, 18.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 15975, 18.0f, Who::Healer });
        // Naxxramas Cultist 15980: Shadow Burst 28447, shadow and a knockback within 8 yd.
        r.avoidEnemy.push_back({ 15980, 11.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 15980, 11.0f, Who::Healer });
        // Plague wing.
        // Plague Beast 16034: Mutated Spores 30110 pulses 30111 (324 nature within 10 yd) every 3 s, and Trample 5568 hits
        // everybody within 10 yd every 3 s.
        r.avoidEnemy.push_back({ 16034, 13.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 16034, 13.0f, Who::Healer });
        // Military wing.
        // Deathknight Captain 16145: Whirlwind 28333, a 6 s channel (weapon + 300 to everybody near it every 2 s): everybody but
        // the tank steps out while it spins. Unholy Axe 16194: Whirlwind 24236, a 2 s channel of 15589 (8 yd).
        r.avoidEnemyAura.push_back({ 16145, 28333, 11.0f, Who::NonTank });
        r.avoidEnemyAura.push_back({ 16194, 24236, 11.0f, Who::NonTank });
        // Bony Construct 16167: Sweeping Slam 25322, weapon + 1750-2250 and a knockback in a 15-yard frontal cone; Cleave 19632.
        r.avoidFrontal.push_back({ 16167, 18.0f, 60.0f, Who::NonTank });
        // Unholy Swords 16216: Cleave 19632.
        r.avoidFrontal.push_back({ 16216, 10.0f, 70.0f, Who::NonTank });
        // Unholy Staff 16215: Arcane Explosion 28450, 1838 arcane and a stun within 20 yd (0.5 s cast); Frost Nova 29849 (10 yd).
        r.avoidEnemy.push_back({ 16215, 23.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 16215, 23.0f, Who::Healer });
        // Stoneskin / Plagued Gargoyles (16168 / 16446) channel Stoneskin 28995 below 30% (6 s, immune, heals
        // 100000 a second; classic_naxxramas_gargoyle_ai): burn them first so they get there late and die through the cast.
        r.priority.push_back({ 16168, 4 });
        r.priority.push_back({ 16446, 4 });
        // Frenzy 24318: Tranquilizing Shot is the PvE coordinator's (PlanTranq, any aura the shot removes). Frenzy at 30%, Infectious/Plagued Ghoul (SAI 16244, 16447)
        r.priority.push_back({ 15981, 4 });     // Naxxramas Acolyte: Shadow Bolt Volley 28448
        r.priority.push_back({ 16368, 4 });     // Necropolis Acolyte: Arcane Explosion + Shadow Bolt Volley
        r.priority.push_back({ 16020, 5 });     // Mad Scientist: Great Heal at 30%, Mana Burn
        r.priority.push_back({ 16385, 6 });     // Lightning Totem from a Living Monstrosity
        r.priority.push_back({ 16021, 4 });     // Living Monstrosity: fear, Chain Lightning, totem
        r.priority.push_back({ 16022, 3 });     // Surgical Assistant: Mind Flay
        r.priority.push_back({ 16164, 5 });     // Shade of Naxxramas: Shadow Bolt Volley every 4 s
        r.priority.push_back({ 16420, 6 });     // Portal of Shadows from a Spirit of Naxxramas
        r.priority.push_back({ 16215, 5 });     // Unholy Staff: polymorph, Frost Nova, Arcane Explosion
        r.priority.push_back({ 16146, 4 });     // Deathknight: fear, Raise Dead

        // Scripted in RaidMechanics_Naxx.cpp: Heigan's eruption dance and Mana Burn range, Sapphiron's air phase (spread,
        // hide behind ice blocks), Razuvious' mind-controlled understudies, Faerlina's worshipper sacrifice, Gothik's side
        // split (pre-pull) and per-side targets while his combat gate is closed.
        //
        // Not ruled:
        //  * Thaddius: Forever's port gives every player Negative Charge (classic_boss_thaddius.cpp:851-858), so the charge
        //    sides above all resolve to one side; harmless. Magnetic Pull tank swap of Stalagg/Feugen relies on pvecoord.
        //  * Noth's balcony phase (immune 29230, adds) and blink: the generic tank pickup handles both.
        //  * Kel'Thuzad: Frost Blast 27808 (heal through), Frostbolt 28478 interrupt (pvecoord's kicks if it owns it).
        //  * Loatheb Inevitable Doom 29204 (heal through); Anub'Rekhan Impale (random line, nothing to dodge in time).
        //  * Trash: Plague Slime 16243 changes school immunity with its colour (needs the casters' rotations to pick a school);
        //    Plague Beast 16034 Mutated Spores (radius not in the spell data); Gargoyle Stoneskin needs an interrupt (pvecoord).
    }

} // namespace Playerbot::Raid
