#include "RaidEncounters.h"

// Ruins of Ahn'Qiraj (map 509), 20-player. New for Forever: Karrz's ike3 bots had no AQ20 data.
// Sources: Forever's Classic ports of the VMaNGOS scripts, src/server/scripts/Custom/Classic/instances/ruins_of_ahnqiraj
// (abbreviated aq20/<file>.cpp below), the captains' and trash spell lists in sql/custom/world/2026_09_28_15_world_vanilla_smartai.sql,
// and the vanilla spell data (VMaNGOS spell_template: effect, radius, dispel type) for what each cast reaches and what removes it.

namespace Playerbot::Raid {

    void RegisterRuinsOfAhnQiraj(RaidRules& r)
    {
        r.name = "Ruins of Ahn'Qiraj";

        // Kick-first casts: the Flesh Hunter's Poison Bolt 25424 (1125-1875 plus the dot) and the mana drains that fill Moam and the
        // Obsidian Destroyers (25754 / 25676 pull 500 mana from six mana users each cast and hand it to the caster).
        r.interruptFirst.push_back(25424);
        r.interruptFirst.push_back(25754);
        r.interruptFirst.push_back(25676);

        r.bosses = { 15348,                 // Kurinnaxx
                     15341,                 // General Rajaxx (after the seven waves)
                     15340, 15370, 15369,   // Moam, Buru the Gorger, Ayamiss the Hunter
                     15339 };               // Ossirian the Unscarred

        // ---------------------------------------------------------------------------------------------------------------
        // TRASH
        // Anubisath Guardian 15355 (smartai rows: two random pairs of powers): Meteor 24340 is one hit of 13300-14700 split among everybody
        // within 8 yd of its target, and Shadow Storm 26546 only reaches past 25 yd, so the raid stacks on it; Explode 25698 is a 6 s wind-up
        // aura before 25699 (4688-5312 fire within 20 yd): everybody leaves while it carries the aura.
        r.stayNear.push_back({ 15355, 8.0f, Who::NonTank });
        r.avoidEnemyAura.push_back({ 15355, 25698, 22.0f, Who::Anyone });
        // Obsidian Destroyer 15338: Drain Mana 25676 every 7 s until it is full, then its full-mana spell. It dies before it fills up.
        r.priority.push_back({ 15338, 4 });
        // Flesh Hunter 15335: Consume 25371 swallows a raider (10% of their health a second until Spit Out 25383); Poison Bolt 25424 (poison).
        // Nothing frees the swallowed one but the Hunter's death.
        r.priority.push_back({ 15335, 4 });
        r.dispel.push_back({ 25424, DispelKind::Poison });
        // Qiraji Swarmguard: Vengeance 25164 (magic self buff when a friend dies, +attack speed): purged.
        r.purge.push_back({ 25164 });
        // Silicate Feeder 15333: Cloud of Disease 17742, a 5 yd persistent cloud (350 a second).
        r.avoidGroundEffect.push_back({ 15333, 17742, 2.0f, Who::Anyone });

        // ---------------------------------------------------------------------------------------------------------------
        // KURINNAXX (15348, aq20/classic_boss_kurinnaxx.cpp): Mortal Wound 25646 stacks on the tank (-10% healing a stack): swap at 5.
        // Sand Trap: a trap object 180647 under a random raider fires 25656 (15 yd) 4.5 s later. Wide Slash 25814 is a 10 yd cone in front.
        r.tankSwap.push_back({ 15348, 25646, 5 });
        r.avoidGameObject.push_back({ 180647, 15.0f, Who::Anyone });
        r.avoidFrontal.push_back({ 15348, 10.0f, 60.0f, Who::NonTank });

        // ---------------------------------------------------------------------------------------------------------------
        // GENERAL RAJAXX (15341) and the seven waves (aq20/classic_instance_ruins_of_ahnqiraj.cpp): each wave is a captain with Qiraji
        // Warriors 15387 and Swarmguard Needlers 15344; the captain carries the dangerous cast, so it dies first, the escort next, and
        // Rajaxx himself last (his Thundercrash 25599 halves everybody's health within reach and knocks the tank back, Disarm 6713 on the
        // tank; the raid finishes the adds first, then holds him).
        r.priority.push_back({ 15385, 3 });     // Colonel Zerran: Enlarge 25462 on a friend (+438-562 damage, magic): purged
        r.priority.push_back({ 15386, 3 });     // Major Yeggeth: Cleave, Sunder Armor, Shield
        r.priority.push_back({ 15388, 3 });     // Major Pakkon: Sweeping Slam 25322 (1750-2250, 15 yd knockback)
        r.priority.push_back({ 15389, 3 });     // Captain Drenn: Lightning Cloud 26550 (a 15 s ground cloud)
        r.priority.push_back({ 15390, 3 });     // Captain Xurrem: Shockwave 25425 (1313-1687 and a stun, 20 yd)
        r.priority.push_back({ 15391, 3 });     // Captain Qeez: Intimidating Shout 19134 (8 yd fear)
        r.priority.push_back({ 15392, 3 });     // Captain Tuubid: Attack Order, Cleave, Sunder Armor
        r.priority.push_back({ 15387, 2 });     // Qiraji Warrior: Uppercut, Thunderclap, Enrage at 30%
        r.priority.push_back({ 15344, 2 });     // Swarmguard Needler
        r.purge.push_back({ 25462 });
        r.avoidGroundEffect.push_back({ 15389, 26550, 2.0f, Who::Anyone });
        r.avoidEnemy.push_back({ 15390, 22.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 15390, 22.0f, Who::Healer });
        r.avoidEnemy.push_back({ 15388, 17.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 15388, 17.0f, Who::Healer });
        r.avoidEnemy.push_back({ 15391, 10.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 15391, 10.0f, Who::Healer });
        r.retaunt.push_back(15341);

        // ---------------------------------------------------------------------------------------------------------------
        // MOAM (15340, aq20/classic_boss_moam.cpp): Arcane Eruption 25672 (3063-3937 arcane and a knockback, 45 yd) once his mana is full,
        // so the ranged keep apart and the tank takes him back after the throw; the Mana Fiends 15527 of his stone phase die first. Trample
        // 15550 (10 yd knockback) is the tank's.
        r.priority.push_back({ 15527, 5 });
        r.spread.push_back({ 15340, 8.0f, Who::Ranged });
        r.retaunt.push_back(15340);

        // ---------------------------------------------------------------------------------------------------------------
        // BURU THE GORGER (15370, aq20/classic_boss_buru.cpp): he chases a random raider at growing speed and Dismembers 96 (a bleed) whoever
        // he reaches; Creeping Plague 20512 (70 nature a tick, stacking) sits on everybody within 100 yd. Vanilla data gives neither a dispel
        // type. A Buru Egg 15514 killed while he stands on it hurts him (19593) and leaves a Hive'Zara Hatchling 15521; the hatchlings die
        // first. At 20% he transforms (24721): his armor goes, the chase ends, and a tank takes him.
        r.priority.push_back({ 15521, 5 });
        r.retaunt.push_back(15370);
        // Not ruled: the egg kite (the chased raider drags him over an egg, the raid breaks that egg when he is within reach of it) has no
        // data-row shape; it needs a scripted mechanic like the Viscidus freeze in RaidMechanics_AQ40.cpp. Until then the eggs keep the
        // default priority: breaking one with Buru away from it only adds a hatchling.

        // ---------------------------------------------------------------------------------------------------------------
        // AYAMISS THE HUNTER (15369, aq20/classic_boss_ayamiss.cpp): Paralyze 25725 stuns a raider and summons them to the altar, where a
        // Hive'Zara Larva 15555 walks up and Feeds 25721 on them (the raider dies and becomes a Hive'Zara Hornet 15934) unless the larva
        // dies first: highest kill priority of the raid. Swarmers 15546 come every 60 s. Poison Stinger 25748 stacks on her target (vanilla
        // data: no dispel type).
        r.priority.push_back({ 15555, 6 });
        r.priority.push_back({ 15546, 4 });
        r.priority.push_back({ 15934, 3 });

        // ---------------------------------------------------------------------------------------------------------------
        // OSSIRIAN THE UNSCARRED (15339, aq20/classic_boss_ossirian.cpp): Curse of Tongues 25195 (curse, -75% casting speed, 45 yd) is
        // dispelled; the Sand Vortex 15428 tornados roam the plaza (Sand Storm 25160 on touch); War Stomp 25188 (20 yd knockback) throws the
        // tank, who taunts him back.
        r.dispel.push_back({ 25195, DispelKind::Curse });
        r.avoidCreature.push_back({ 15428, 8.0f, Who::Anyone });
        r.retaunt.push_back(15339);
        // Not ruled: the crystal runner (a melee uses the next Ossirian Crystal 180619 whenever Strength of Ossirian 25176 is up, which
        // removes it and opens a 45 s school weakness) is scripted work, not a data row. Without it he keeps +300% damage.
    }

} // namespace Playerbot::Raid
