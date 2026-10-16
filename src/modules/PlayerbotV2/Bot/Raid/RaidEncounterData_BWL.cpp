#include "RaidEncounters.h"

// Ported from Karrz's raid10-revamp ike3 bots (Karrz1/wow-1.12). Script references (mc/..., bwl/..., ony/...) point at the
// VMaNGOS scripts that Forever's scripts/Custom/Classic instances were ported from; the ids are the same.

namespace Playerbot::Raid {

    // Blackwing Lair (map 469).
    // Sources: scripts in src/scripts/eastern_kingdoms/burning_steppes/blackwing_lair (abbreviated bwl/<file>.cpp:line below),
    // creature types from the world DB dump (creature_template.type), trash spells from creature_ai_events/creature_ai_scripts.
    // UNVERIFIED marks a value the scripts cannot prove (usually a spell's dispel type: spell_template is not in any extract).
    void RegisterBlackwingLair(RaidRules& r)
    {
        r.name = "Blackwing Lair";
        // Encounter order (progression advice): Razorgore, Vaelastrasz, Broodlord, Firemaw, Ebonroc, Flamegor, Chromaggus, Nefarian.
        r.bosses = { 12435, 13020, 12017, 11983, 14601, 11981, 14020, 11583 };

        // ---------------------------------------------------------------------------------------------------------------
        // RAZORGORE THE UNTAMED (12435) - the orb and the eggs are scripted in RaidMechanics_BWL.cpp (a DPS bot takes the Orb of
        // Domination and makes Razorgore destroy the eggs; nobody attacks him in phase 1); only the adds are ranked here.
        // Adds spawn every 15s at four points (bwl/boss_razorgore.cpp:406-547) and have to be killed fast.
        r.priority.push_back({ 12557, 6 });   // Grethok the Controller: casts Dominate Mind (14515, DB event 1255702); pulls the encounter (instance_blackwing_lair.cpp:301)
        r.priority.push_back({ 12420, 6 });   // Blackwing Mage (7.3k hp): Arcane Explosion 22271 + Fireball 17290 (DB events 1242001/2); killing casters first
        r.priority.push_back({ 12422, 4 });   // Death Talon Dragonspawn (45k hp): big melee add (boss_razorgore.cpp:461)
        r.priority.push_back({ 12416, 3 });   // Blackwing Legionnaire (9k hp): plain melee add (boss_razorgore.cpp:443)
        r.controlledMember.push_back({ 14515 });
        r.dispel.push_back({ 13747, DispelKind::Magic });                         // Slow (Grethok), dispel type magic (ike3)
        r.priority.push_back({ 12468, 3 });   // Death Talon Hatcher: Flamestrike 22275 caster (creature_spells 124680)  // Grethok's Dominate Mind: sheep or stun the controlled raider, never kill it
        // Phase 2: Conflagration 23023 is cast on himself every 15-25s (classic_boss_razorgore.cpp:345-349) and disorients whoever stands
        // in melee, the tank included: a tank without it takes him over.
        r.tankSwap.push_back({ 12435, 23023, 1 });
        r.priority.push_back({ 14456, 2 });   // Blackwing Guardsman: Strike 15580 only (DB event 1445601)

        // ---------------------------------------------------------------------------------------------------------------
        // VAELASTRASZ THE CORRUPT (13020)
        // Burning Adrenaline is thrown on a random mana user every 15s and on the current tank every 45s (boss_vaelastrasz.cpp:346-380).
        // Aura 23620 has three aura effects; it drains max health 5%/s and the comment at :43 describes a raid explosion at the end,
        // so a non-tank carrier must leave the raid. A tank carrier stays on the boss (carrierRun default NonTank) and the off tank taunts.
        r.tankSwap.push_back({ 13020, 23620, 1 });                                // tank carrier dies in 20s (5%/s): off tank taunts (boss_vaelastrasz.cpp:47,372-381)
        r.carrierRun.push_back({ 23620, 20.0f, Who::NonTank });                   // Burning Adrenaline (boss_vaelastrasz.cpp:44)
        r.avoidCarrier.push_back({ 23620, 15.0f, Who::NonTank });                 // everybody but tanks keeps off the carrier (explosion 23478 :45)
        // Cleave 19983 chains to everybody close to the main tank (boss_vaelastrasz.cpp:49-51: nobody within ~10 yd of the tank).
        r.spread.push_back({ 13020, 10.0f, Who::Ranged });                        // Cleave chain (boss_vaelastrasz.cpp:51)
        r.spread.push_back({ 13020, 10.0f, Who::Healer });                        // Cleave chain (boss_vaelastrasz.cpp:51)
        // Death Talon Captain (12467): Aura of Flames (22436) is spread to Flamescale/Wyrmkin/Seether within 15 yd while he lives
        // and removed when he dies (boss_vaelastrasz.cpp:569-594): kill him first. Also Mark of Detonation 22438 and Mark of Flames 25050 (dispels in TRASH below).
        r.priority.push_back({ 12467, 7 });                                       // boss_vaelastrasz.cpp:556-594
        // Death Talon Seether (12464) enrages with Frenzy every 15s (boss_vaelastrasz.cpp:659,688-696): hunters tranquilize.
        // Frenzy 22428: Tranquilizing Shot is the PvE coordinator's (PlanTranq, any aura the shot removes). Frenzy (boss_vaelastrasz.cpp:659); effect type 6 = aura (migration 20200515174239)

        // ---------------------------------------------------------------------------------------------------------------
        // BROODLORD LASHLAYER (12017)
        // Mortal Strike 24573, Cleave 15284, Blast Wave 23331 (damage + daze + knockback: effects 2/6/98), Knock Away 18670 (-50% threat,
        // boss_broodlord_lashlayer.cpp:105-106). The trash around him is made unattackable while he lives (:109-143), so no add rules.
        // Blast Wave keep-away for ranged/healers: see RegisterBlackwingMovement (avoidEnemy, boss_broodlord_lashlayer.cpp:172).
        // No tank swap: Mortal Strike is cast only every 20-30s (:178-182) and the script gives no aura/stack condition, so a swap would
        // only thrash against Knock Away's -50% threat (:105-106); the tanks keep the boss.

        // ---------------------------------------------------------------------------------------------------------------
        // FIREMAW (11983) / EBONROC (14601) / FLAMEGOR (11981)
        // Shadow Flame 22539 is a frontal cone (effects: damage + script), Wing Buffet 23339 knocks back and halves threat of the victim
        // (boss_firemaw.cpp:71-77), Firemaw casts Flame Buffet 23341 on himself every 1.8-3s (boss_firemaw.cpp:106): raid-wide, not a tank
        // debuff, so no swap. Ebonroc puts Shadow of Ebonroc 23340 on his victim whenever it is missing (boss_ebonroc.cpp:106, CF_AURA_NOT_PRESENT).
        // Flamegor has no tank debuff in his script (boss_flamegor.cpp:71-121): only Wing Buffet / Shadow Flame / Frenzy. Tank facing cannot be
        // a stand rule: no justified coordinates (the dragons are pulled to wherever the tank stands).
        r.tankSwap.push_back({ 14601, 23340, 1 });                                // Ebonroc: Shadow of Ebonroc (boss_ebonroc.cpp:24,106)
        // Frenzy 23342: Tranquilizing Shot is the PvE coordinator's (PlanTranq, any aura the shot removes). Flamegor Frenzy, periodically fire novas (boss_flamegor.cpp:26,105-113)

        // ---------------------------------------------------------------------------------------------------------------
        // CHROMAGGUS (14020)
        // Five Brood Afflictions are cast on 11-15 random players every 7.5s (boss_chromaggus.cpp:291-345), the 2 breaths are random out of 5.
        // A player holding all five becomes a "Chromatic Mutation" (:318-340). Red Affliction deaths heal him for 150k (:349-366).
        // DISPEL KIND of every entry below is UNVERIFIED (no spell_template in the extracts); Magic is the default requested by the lead.
        // The debuff ids ARE the cast ids: effect1 of each is 6 = APPLY_AURA (migration 20200515174239), so the player carries that id.
        r.dispel.push_back({ 23170, DispelKind::Magic });                         // Brood Affliction: Bronze (boss_chromaggus.cpp:47), dispel type magic (ike3)
        r.dispel.push_back({ 23153, DispelKind::Magic });                         // Brood Affliction: Blue  (boss_chromaggus.cpp:44) UNVERIFIED kind
        r.dispel.push_back({ 23154, DispelKind::Curse });                         // Brood Affliction: Black (boss_chromaggus.cpp:45) UNVERIFIED kind
        r.dispel.push_back({ 23155, DispelKind::Disease });                         // Brood Affliction: Red   (boss_chromaggus.cpp:46) UNVERIFIED kind; dying with it heals the boss
        r.dispel.push_back({ 23169, DispelKind::Poison });                         // Brood Affliction: Green (boss_chromaggus.cpp:48) UNVERIFIED kind
        // Breath debuffs that stay on the player (the breath itself is the aura spell): Time Lapse 23310 (aura,aura), Corrosive Acid 23313
        // (aura,aura), Ignite Flesh 23315 (aura,dummy), Frost Burn 23187 (damage,aura,...). Incinerate 23308/23309 is pure damage (effect 2)
        // -> there is nothing to dispel, the junior entry was removed.
        // Frenzy 23128: Tranquilizing Shot is the PvE coordinator's (PlanTranq, any aura the shot removes). Chromaggus Frenzy every 15s (boss_chromaggus.cpp:52,385-394)
        // Elemental shield auras 22277-22281 (fire/frost/shadow/nature/arcane, -75% all, +1100% one school, changes every 20s) are on the
        // boss: needs a school-aware spell choice, see raid-research.md.

        // ---------------------------------------------------------------------------------------------------------------
        // LORD VICTOR NEFARIUS (10162) - phase 1 (drakonid waves) of the Nefarian fight
        // Immune behind Nefarius Barrier 22663 (boss_victor_nefarius.cpp:44,321) and rooted: never target him.
        // Two drakonid types (random pair) spawn every 6-7s, a Chromatic Drakonid pair every 35s, until 40 are dead (:373-443).
        r.priority.push_back({ 10162, -1 });                                      // barrier immunity in phase 1 (boss_victor_nefarius.cpp:44)
        r.priority.push_back({ 14302, 6 });                                       // Chromatic Drakonid (24k hp) (boss_victor_nefarius.cpp:394)
        r.priority.push_back({ 14263, 5 });                                       // Bronze Drakonid (boss_victor_nefarius.cpp:58)
        r.priority.push_back({ 14261, 5 });                                       // Blue Drakonid (:59)
        r.priority.push_back({ 14264, 5 });                                       // Red Drakonid (:60)
        r.priority.push_back({ 14262, 5 });                                       // Green Drakonid (:61)
        r.priority.push_back({ 14265, 5 });                                       // Black Drakonid (:62)

        // NEFARIAN (11583)
        // Shadow Flame 22539 cone, Bellowing Roar 22686 fear, Veil of Shadow 22687 on the tank, Tail Lash 23364, class calls every 25-35s
        // (boss_nefarian.cpp:348-368), at 20% he raises every dead drakonid as a Bone Construct (:371-376, summon spell 23363 on drakonid death).
        r.priority.push_back({ 11583, 3 });                                       // the boss must beat the constructs (boss_nefarian.cpp:371-376)
        r.priority.push_back({ 14605, 0 });                                       // Bone Construct: up to 40 of them, 6k hp each; tanks hold them, DPS burn Nefarian (DB 14605)
        r.priority.push_back({ 14668, 6 });                                       // Corrupted Infernal (warlock call: 2 per warlock, stun+damage) (boss_nefarian.cpp:63,609-624)
        // Corrupted totems (shaman call, boss_nefarian.cpp:398-557): 200-2000 hp, root themselves, give Nefarian/drakonids Windfury, Stoneskin,
        // Healing Stream within 40 yd and have Avoidance 23198 (AoE does nothing to them). The junior noAoe entries were wrong: avoidance only
        // makes AoE useless, not harmful, and noAoe would have blocked AoE on the drakonids for the whole fight. Kill them with single target.
        r.priority.push_back({ 14666, 6 });                                       // Corrupted Windfury Totem III: extra attacks on Nefarian (boss_nefarian.cpp:543,379-381)
        r.priority.push_back({ 14663, 5 });                                       // Corrupted Stoneskin Totem VI (boss_nefarian.cpp:437)
        r.priority.push_back({ 14664, 4 });                                       // Corrupted Healing Stream Totem V (boss_nefarian.cpp:440)
        // Fire Nova totem 14662 removes itself after one cast (boss_nefarian.cpp:527-533): no rule.
        // Class-call debuffs on bots: Wild Polymorph 23603 (mage call, boss_nefarian.cpp:58,726). UNVERIFIED dispel kind.
        r.dispel.push_back({ 23603, DispelKind::Magic });                         // Nefarian Polymorph (boss_nefarian.cpp:58) UNVERIFIED kind
        r.dispel.push_back({ 22687, DispelKind::Curse });                         // Veil of Shadow on the tank, -healing (boss_nefarian.cpp:41,324) UNVERIFIED kind
        // Priest call: Corrupted Healing 23401 is put on every priest (classic_boss_nefarian.cpp HandleClassCall, AddAura) and turns
        // their heals into damage over time on the target: healing priests stop until it fades (dispels still run).
        r.holdByAura.push_back({ 23401, Who::Healer });
        // Paladin call (23418, cast by every paladin on himself): Blessing of Protection lands on raid members and a protected
        // tank or melee cannot attack, so they click it off. Ranks 1022/5599/10278; UNVERIFIED that the call uses these ids
        // (the call's spell data is not in the repo), harmless otherwise: nobody needs BoP on a tank or a melee here.
        for (uint32_t bop : { 1022u, 5599u, 10278u })
        {
            r.cancelAura.push_back({ 11583, bop, Who::Tank });
            r.cancelAura.push_back({ 11583, bop, Who::Physical });
        }

        // Druid call (23398, classic_boss_nefarian.cpp:70,246): the druid is forced into Cat Form. A healing druid in a form cannot
        // cast, so it leaves the form (the forced aura and the Cat Form itself) and keeps healing; feral druids stay as they are.
        r.cancelAura.push_back({ 11583, 23398, Who::Healer });
        r.cancelAura.push_back({ 11583, 768, Who::Healer });

        // ---------------------------------------------------------------------------------------------------------------
        // TRASH (types from creature_template: 2 Dragonkin, 7 Humanoid)
        // Casters first, healers (Taskmaster Healing Circle 22458, DB event 1245802) next.
        r.priority.push_back({ 12457, 5 });                                       // Blackwing Spellbinder (dragonkin, 90k hp), caster
        r.crowdControl.push_back({ 12457, CcKind::Hibernate, 0 });                // dragonkin: druid Hibernate works on the type (UNVERIFIED: rule type only, no script)
        r.priority.push_back({ 12459, 5 });                                       // Blackwing Warlock (humanoid), summons a Demon Portal (creature 14081, DB event 1245903)
        r.crowdControl.push_back({ 12459, CcKind::Polymorph, 0 });                // humanoid: polymorphable
        r.priority.push_back({ 12458, 4 });                                       // Blackwing Taskmaster: Healing Circle 22458 at <=70% hp (DB event 1245802)
        r.priority.push_back({ 12461, 4 });                                       // Death Talon Overseer: Fire Blast 20623 (instance_blackwing_lair.cpp:1258-1266), no brood power
        r.priority.push_back({ 13996, 2 });                                       // Blackwing Technician (15k hp): Poison Bottle 22335 + Bomb 22334 (instance_blackwing_lair.cpp:1283-1284,1399-1402)
        // Wyrmguard (12460): Cleave 15284 + War Stomp 24375 (instance_blackwing_lair.cpp:1242,1252): default priority, tank and spank.
        // Death Talon Flamescale/Wyrmkin (12463/12465) carry the Captain's Aura of Flames (boss_vaelastrasz.cpp:573-587): default priority.

        // Trash mechanics (casts: classic_instance_blackwing_lair.cpp and sql/custom/world/2026_09_28_15_world_vanilla_smartai.sql
        // "VMaNGOS spell list / EventAI" rows; radii, durations and dispel types: classic spell data).
        // Death Talon Captain (12467, classic_npc_death_talon_Captain in classic_boss_vaelastrasz.cpp): Mark of Detonation 22438 on a
        // random player, 30 s: every melee hit on the carrier deals 657-843 to it and all its allies near it. Magic: dispelled at once.
        r.dispel.push_back({ 22423, DispelKind::Magic });                         // Flame Shock (Flamescale), dispel type magic (ike3)
        r.dispel.push_back({ 22438, DispelKind::Magic });
        // ... and Mark of Flames 25050 on a random player: +1000 fire damage taken for 2 min. Magic.
        r.dispel.push_back({ 25050, DispelKind::Magic });
        // Blackwing Spellbinder (12457) / Death Talon Hatcher (12468): Greater Polymorph 22274 on a random non-tank (spell list
        // 124570), 20 s. Magic: a sheeped healer is dispelled.
        r.dispel.push_back({ 22274, DispelKind::Magic });
        // ... and Flamestrike 22275 (spell lists 124570 / 124680), a 5-yard fire patch for 8 s.
        r.avoidGroundEffect.push_back({ 12457, 22275, 2.0f, Who::Anyone });
        r.avoidGroundEffect.push_back({ 12468, 22275, 2.0f, Who::Anyone });
        // Blackwing Warlock (12459): Rain of Fire 19717 (spell list 124590).
        r.avoidGroundEffect.push_back({ 12459, 19717, 3.0f, Who::Anyone });
        // Death Talon Wyrmguard (12460): War Stomp 24375, weapon damage + 500, knockback and a 5 s stun within 15 yards
        // (classic_npc_death_talon, every 8-14 s). Casters and healers keep 18 yards.
        r.avoidEnemy.push_back({ 12460, 18.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 12460, 18.0f, Who::Healer });
        // Death Talon Wyrmkin (12465): Blast Wave 22424, fire within 10 yards (spell list 124650). Casters and healers keep 13.
        r.avoidEnemy.push_back({ 12465, 13.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 12465, 13.0f, Who::Healer });
        // Death Talon Overseer (12461) and Wyrmguard: Cleave (15754 / 15284) on the victim and its nearest allies.
        r.avoidFrontal.push_back({ 12461, 10.0f, 70.0f, Who::NonTank });
        r.avoidFrontal.push_back({ 12460, 10.0f, 70.0f, Who::NonTank });
        // Kicked before any other cast in reach: the Taskmaster's Healing Circle 22458 (spell list 124580) and the Spellbinder's
        // Greater Polymorph 22274 (spell list 124570; a dispel stays the backup when no kick is ready).
        r.interruptFirst.push_back(22458);
        r.interruptFirst.push_back(22274);
        // Not ruled: the Death Talons' random school vulnerability (22277-22281, classic_npc_death_talon) needs the casters'
        // rotations to pick the school, which the raid layer does not drive; Flamescale's Flame Shock 22423 (a short DoT on the
        // tank) is not worth a dispel.
    }
} // namespace Playerbot::Raid
