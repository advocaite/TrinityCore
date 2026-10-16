// RaidEncounters — data-driven raid encounter rules for the bots.
//
// Ported from Karrz's raid10-revamp ike3 playerbots (Karrz1/wow-1.12,
// src/game/PlayerBots/playerbot/strategy/generic/RaidEncounters.h). Forever's
// raids run the Classic ports of the VMaNGOS scripts (scripts/Custom/Classic),
// so the creature entries and spell ids carry over unchanged.
//
// A rule set is registered per MAP ID (409 Molten Core, 469 Blackwing Lair,
// 249 Onyxia's Lair, 533 Naxxramas). RaidBrain.cpp reads these tables on the
// world thread and turns them into per-bot raid orders (move out of hazards,
// spread, stand at role positions, taunt swaps, dispels, purges, crowd
// control, forbidden targets). The data files (RaidEncounterData_*.cpp)
// contain DATA ONLY - no logic.
//
// Conventions
//  * enemy / boss values are creature template ENTRY ids, aura / spell values are spell ids.
//  * radii and distances are in yards.
//  * Who selects which raid members a rule applies to.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Playerbot::Raid {

enum class Who : uint8_t
{
    Anyone,
    Tank,       // any tank
    MainTank,   // the raid's main tank
    OffTank,    // a tank that is not the main tank
    NonTank,    // everybody except tanks
    Melee,      // melee DPS (not tank, not healer, not ranged)
    Ranged,     // casters / hunters
    Healer,
    ManaUser,   // anybody with a mana bar (Kel'Thuzad's Detonate Mana)
    Caster,     // ranged DPS that deal spell damage (not hunters)
    Physical,   // DPS that deal physical damage: melee and hunters
    AoeShare    // every second caster (by guid): the share of the ranged DPS that a rule hands to an area-damage job
};

enum class DispelKind : uint8_t { Magic, Curse, Poison, Disease };

enum class CcKind : uint8_t { Banish, Polymorph, Shackle, Hibernate };

// While <enemy> carries <aura>, the matching bots stay outside <radius> of that enemy (e.g. Baron Geddon's Inferno).
struct AvoidEnemyAura { uint32_t enemy; uint32_t aura; float radius; Who who; };

// Everybody (matching) stays outside <radius> of any raid member that carries <aura> (e.g. Living Bomb).
struct AvoidCarrier { uint32_t aura; float radius; Who who; };

// A bot that carries <aura> runs until it is at least <clearDistance> from every other raid member (e.g. Living Bomb).
// who: which carriers run (a tank carrying it stays on the boss and the off tank taunts if needed).
struct CarrierRun { uint32_t aura; float clearDistance; Who who = Who::NonTank; };

// Frontal cone (breath, Wing Buffet, Shadow Flame, cleaves): matching bots stay out of the sector in front of <enemy> that is
// <radius> yards deep and <halfAngleDeg> degrees either side of where it faces. Use Who::NonTank so the tank keeps the boss.
// facingOffsetDeg turns the cone: 180 is a tail sweep behind the enemy.
// tankTurns: when three or more non-tanks stand in the cone, the tank holding the enemy steps to the far side of it to turn it away from the raid
// (Magmadar). Off for a boss whose raid stands on fixed flank wedges (Onyxia): there the turn sends her breath across the wedges, the whole
// ranged group has to re-position each time, and onyxia_full n57 run 1 saw the main tank do it every 9 s for four minutes (1880 "leave cone" steps).
struct AvoidFrontal { uint32_t enemy; float radius; float halfAngleDeg; Who who; float facingOffsetDeg = 0.0f; float hpMax = 100.0f; float hpMin = 0.0f; bool tankTurns = true; };

// Ground effects created by <enemy> through <spell> (dynamic objects such as Rain of Fire): stay out of them, plus <margin>.
struct AvoidGroundEffect { uint32_t enemy; uint32_t spell; float margin; Who who; };

// Ground hazards that are game objects (e.g. Magmadar's Lava Bomb fire patches): stay <radius> yards away from any with this entry.
struct AvoidGameObject { uint32_t entry; float radius; Who who; };

// Ground hazards that are creatures which never fight (Grobbulus' poison cloud, Sapphiron's Blizzard, Kel'Thuzad's Shadow
// Fissure): stay <radius> yards away from any living one with this entry. New for Forever (Naxxramas).
// corpse: the dead creature is the hazard (Lord Kri's Toxic Vapors cloud where he died).
struct AvoidCreature { uint32_t entry; float radius; Who who; bool corpse = false; };

// While <enemy> is alive and fighting, matching bots keep at least <radius> from the nearest other raid member.
// No raid move ends inside this circle while <enemy> is alive and fighting (one clamp for every rule, applied to the finished move target in
// RaidBrain). Onyxia's two cave alcoves: the static whelps lie there, and a bot that walks up to them pulls the lot.
struct KeepOut { uint32_t enemy; float x, y, radius; };

struct Spread { uint32_t enemy; float radius; Who who; float hpMax = 100.0f; float hpMin = 0.0f; };

// Matching bots go to (x,y,z) and hold within <tolerance> while <enemy> is alive and fighting.
// needOtherEnemy: when not 0, this other entry must also be alive and fighting (e.g. Golemagg's Core Ragers).
// needVictimIsMe: only when the enemy is attacking this bot - the <needOtherEnemy> creature when one is given, else <enemy>.
struct Stand { uint32_t enemy; Who who; float x, y, z; float tolerance; bool needVictimIsMe; uint32_t needOtherEnemy; float hpMax = 100.0f; float hpMin = 0.0f;
              uint8_t share = 0; };   // share 1 / 2: only the even / odd numbered of the members this rule matches (by guid order), so one rule list can split e.g. the off-tanks between two posts

// While <enemy> is alive and fighting, a member carrying <aura> stands <distance> yards from the enemy's live position
// along the fixed world bearing <bearing> (radians) and holds within <tolerance> (Thaddius' Positive / Negative Charge
// sides). Relative to the enemy so no floor height has to be guessed. New for Forever (Naxxramas).
struct StandByAura { uint32_t enemy; uint32_t aura; float bearing; float distance; float tolerance; };

// A raid member carrying <aura> should be dispelled. <kind> is a fallback: the engine reads the real dispel type
// from the spell data first.
// urgent: the debuff kills or silences (Soul Burn, Impending Doom), so healers dispel it even while the raid is in an emergency and
// the carriers that heal or tank are cleansed first.
// hurtBelowPct: dispel only while the carrier is at or under this health (a weak damage-over-time that costs more mana to remove
// than it does harm); 0 = always. See DispelWorth.h.
struct Dispel { uint32_t aura; DispelKind kind; bool urgent = false; int hurtBelowPct = 0; };

// An enemy carrying <aura> should be purged (priest Dispel Magic / shaman Purge). Tranquilizing Shot is not a raid rule:
// the PvE coordinator's PlanTranq ranks the hunters for any aura the shot removes.
struct PurgeEnemy { uint32_t aura; };

// <enemy> should be crowd controlled with <kind> unless it is the current kill target. skipIfAura: ignore when it has this aura.
// maxActive > 0: never more than that many of <enemy> held at once (Kel'Thuzad frees every Guardian when a 4th is shackled).
// oneAtATime: hold as many as possible, and once none of <enemy> is left uncontrolled (the unheld ones are dead) let ONE go - its holder
// stops refreshing it, a tank takes it, the raid kills it, then the next (Garr's Firesworn; the boss comes last).
struct CrowdControl { uint32_t enemy; CcKind kind; uint32_t skipIfAura; uint32_t maxActive = 0; bool oneAtATime = false; };

// Kill order: higher value first. Entries not listed use the default priority 1. Negative = never a DPS target
// (tanks may still hold it, e.g. Golemagg's Core Ragers). Priority >= 2 feeds the group's synchronized kill focus.
// who / gateEnemy / hpMax / hpMin make an entry conditional: it applies only to matching bots, and, when gateEnemy is not 0, only while a
// living enemy of that entry is inside the health window (Onyxia's whelps outrank her for the melee in phase 2, not for the casters).
// The first matching entry per enemy wins, so list the specific ones first. Only unconditional entries feed the synchronized kill focus.
struct Priority { uint32_t enemy; int value; Who who = Who::Anyone; uint32_t gateEnemy = 0; float hpMax = 100.0f; float hpMin = 0.0f; };

// A boss that cuts its tank's threat on a schedule (Onyxia's Knock Away: -25 percent on its victim). Non-tank damage holds at the
// remaining share of the tank's threat, so the cut leaves the tank level with the raid and never under it: a ranged bot only needs
// 130 percent of the holder's threat to take the boss, melee 110.
struct ThreatDrop { uint32_t enemy; float percent; };

// Do not use area damage while this enemy is alive within 40 yards (adds that explode or must not be pulled).
struct NoAoe { uint32_t enemy; };

// Tank swap: while <enemy>'s current victim is a tank that carries at least <stacks> stacks of <debuffAura>, ONE other tank
// that does not carry the aura (the lowest-GUID one whose taunt is ready) taunts the enemy. A tank carrying the aura stops
// attacking that enemy while a clean tank exists, so the swap is not undone. Paladins have no taunt in 1.12, so only
// warriors and druids can take over.
struct TankSwap { uint32_t enemy; uint32_t debuffAura; uint32_t stacks; };

// While <enemy> is alive and fighting, matching bots stay at least <radius> yards from it (point-blank AoE such as Blast Wave).
// Do not use Who::Tank / Anyone for melee-range bosses: only bots that do not have to stand next to the enemy.
struct AvoidEnemy { uint32_t enemy; float radius; Who who; float hpMax = 100.0f; float hpMin = 0.0f; };

// Line hazard for a breath that sweeps a straight corridor (Onyxia's Deep Breath). When <enemy> is casting <castSpell> (or carries
// it as an aura), or is flying at >= 2.5x run speed (the breath pass itself), the corridor is latched for about 10 seconds: it
// starts 10 yards behind the enemy's position and facing at that moment, runs <length> yards straight ahead and is <halfWidth>
// yards either side of the centre line. Matching bots leave it.
// <windup>: the enemy also counts as breathing while it hovers in the air, standing still with no target. Onyxia's script clears
// her target and waits 5 s facing the far waypoint before the pass (classic_boss_onyxia.cpp DoMovement); the path spell is cast
// triggered on herself at that moment and is neither an aura nor a current spell by the time a bot ticks (Onyxia wipe 2026-10-13:
// 33 of 40 stood in the corridor, no bot moved), so the wind-up is the only warning before the speed-3 pass.
// fixedPath: the corridor is the segment (ax,ay)-(bx,by) in world coordinates, <length> unused, and it opens only while <enemy> casts <castSpell>
// (or carries it): the spell's chained triggers land on fixed points, whatever the caster's position and facing. Onyxia's script starts
// the breath wherever her movement timer runs out, mid-flight, so a corridor taken from her position and facing pointed the wrong way
// (onyxia_full n31 runs 5 and 6: she stood at -8.7,-216.5 facing east, the breath ran west, 16 raiders died in each).
// Fixed standing points out of one breath spell's corridor (the AzerothCore bots' per-breath safe zones): a member inside the
// corridor of <castSpell> walks to the nearest of these instead of searching for a way out.
struct BreathSafe { uint32_t enemy; uint32_t castSpell; float x, y; };

struct AvoidBreath { uint32_t enemy; uint32_t castSpell; float halfWidth; float length; Who who; bool windup = false;
                     bool fixedPath = false; float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f; };

// Melee soak (Patchwerk's Hateful Strike hits the highest-health player in melee among his top four on threat): up to <count>
// matching bots (never the main tank) keep attacking <enemy> in melee so the strike lands on them instead of a DPS.
struct MeleeSoak { uint32_t enemy; Who who; uint32_t count; };

// A raid member carrying <aura> is mind-controlled by the enemy (Lucifron's Flamewaker Protectors: Dominate Mind). A mage
// polymorphs it; without one a stun (Hammer of Justice, Gouge, Bash) takes it out. Nobody attacks it. Ends with the aura.
struct ControlledMember { uint32_t aura; };

// Only the matching bots (plus tanks) may attack <enemy>; everybody else leaves it alone (Twin Emperors: melee on
// Vek'nilash, casters on Vek'lor).
struct OnlyAttackers { uint32_t enemy; Who who; };

// A matching bot carrying <aura> pauses its rotation while the aura lasts (Nefarian's priest call: every heal the priest
// casts hurts its target). Elected jobs (dispels) and movement still run.
struct HoldByAura { uint32_t aura; Who who; };

// While <enemy> is in the fight, matching bots remove <aura> from themselves at once (Nefarian's paladin call: Blessing of
// Protection stops a tank or a melee from attacking).
struct CancelAura { uint32_t enemy; uint32_t aura; Who who; };

// A pack whose members must die together (Molten Core's Core Hounds: each one drops to 1 hp and feigns death, and if any
// hound of the pack is still up 10 seconds later the downed ones rise again at full health). Damage is spread instead of
// focused: a downed member (carrying <downAura>, or at 1 hp) is never attacked, DPS bots leave members that are more than
// <spreadPct> health percent below the healthiest one alone, and the healthiest one is the DPS fallback target.
struct KillTogether { uint32_t enemy; uint32_t downAura; float spreadPct; };

// While <enemy> is alive and fighting, matching bots stay within <radius> yards of it (an Anubisath Defender's Meteor is
// split among everybody within 8 yards, its Shadow Storm only hits beyond 25). A bot further out walks back to 60% of the
// radius on its own side of the enemy, outside any hazard. With several such enemies, the nearest one counts.
struct StayNear { uint32_t enemy; float radius; Who who; float hpMax = 100.0f; float hpMin = 0.0f; };

// While <enemy> is alive and a tank holds it, that tank stands on the far side of the enemy from the raid (the line from the
// raid's centre through the enemy, at melee range), so the enemy turns its back on everybody else (Magmadar). The raid's centre
// is the mean position of the non-tank members within 60 yards. A tank that already opposes the raid within 30 degrees stays.
// bearingDeg (when not kNoBearing): the world bearing from the enemy to the tank's spot, instead of the line from the raid's centre. Onyxia: east,
// the back of the lair, wherever the raid stands (the raid enters from the west and spreads; a bearing taken from where it stood at the pull
// put the tank on the wrong side once the raid moved).
constexpr float kNoBearing = 1000.0f;
struct FaceAway { uint32_t enemy; float hpMax = 100.0f; float hpMin = 0.0f; float bearingDeg = kNoBearing; };

// While <enemy> is alive and fighting, matching bots stand in the sector behind it: <halfAngleDeg> either side of the line
// straight behind where it faces, between <minDist> and <maxDist> yards. A bot outside walks to the nearest such spot on its
// own side. Pair with FaceAway so the tank decides which way "behind" is.
struct StandBehind { uint32_t enemy; Who who; float minDist; float maxDist; float halfAngleDeg; float hpMax = 100.0f; float hpMin = 0.0f; };

// While <enemy> is alive and fighting (in the hp window), matching bots stand on its flank: <halfAngleDeg> either side of the line
// <centerDeg> off where it faces, on the side they are already on, between <minDist> and <maxDist> yards. For a boss that hits the
// front (breath, Wing Buffet, Cleave) and the whole back half (Tail Sweep): melee reach it from the flank wedge between the two cones
// (Onyxia: centre 70, half 15, so 55..85 degrees off her facing; the tail sweep starts at 90). Uses her live facing, so the tank keeps
// her still; a bot already inside the wedge does not move, so there is no leave / come back cycle.
struct StandFlank { uint32_t enemy; Who who; float minDist; float maxDist; float centerDeg; float halfAngleDeg; float hpMax = 100.0f; float hpMin = 0.0f; };

// A resistance potion a matching bot drinks when <enemy> is engaged, without the global Playerbot.Raid.ResistPotions switch (Ragnaros'
// Elemental Fire: 2400 a tick on the tank, five tank deaths in three minutes, MC 2026-10-12). The kit stocks it (Classic::RaidConsumable::combat).
// Only Who::Tank and Who::Anyone are read.
struct ResistPotion { uint32_t enemy; uint32_t item; Who who; };

// A member that gains <aura> in this raid drinks <item> at once, whoever it is and whatever the boss order says: the aura is a hit that
// kills the carrier or the carrier's neighbours and the absorb is what carries it (Living Bomb: 3200 fire to the carrier, a caster under
// 3200 health dies of its own bomb). The boss-wide ResistPotion order is spent by the first Inferno before the next bomb lands.
struct CarrierPotion { uint32_t aura; uint32_t item; };

// Phase windows (hpMax / hpMin on AvoidFrontal, Spread, Stand, AvoidEnemy, StayNear): the rule only applies while the enemy's
// health percent is within [hpMin, hpMax] (Onyxia's Fireball spread is phase 2: 65..40). Defaults 100 / 0 = always.

// Everything for one map.
struct RaidRules
{
    std::string name;
    std::vector<uint32_t> bosses;   // encounter order, for the dungeon progression advice
    std::vector<AvoidEnemyAura> avoidEnemyAura;
    std::vector<AvoidCarrier> avoidCarrier;
    std::vector<CarrierRun> carrierRun;
    std::vector<AvoidGroundEffect> avoidGroundEffect;
    std::vector<AvoidGameObject> avoidGameObject;
    std::vector<AvoidCreature> avoidCreature;
    std::vector<AvoidFrontal> avoidFrontal;
    std::vector<Spread> spread;
    std::vector<KeepOut> keepOut;
    std::vector<Stand> stand;
    std::vector<StandByAura> standByAura;
    std::vector<Dispel> dispel;
    std::vector<PurgeEnemy> purge;
    std::vector<CrowdControl> crowdControl;
    std::vector<Priority> priority;
    std::vector<NoAoe> noAoe;
    std::vector<TankSwap> tankSwap;
    std::vector<AvoidEnemy> avoidEnemy;
    std::vector<AvoidBreath> avoidBreath;
    std::vector<BreathSafe> breathSafe;
    std::vector<MeleeSoak> meleeSoak;
    std::vector<ControlledMember> controlledMember;
    std::vector<OnlyAttackers> onlyAttackers;
    std::vector<HoldByAura> holdByAura;
    std::vector<CancelAura> cancelAura;
    std::vector<KillTogether> killTogether;
    std::vector<StayNear> stayNear;
    std::vector<FaceAway> faceAway;
    std::vector<StandBehind> standBehind;
    std::vector<StandFlank> standFlank;
    std::vector<ResistPotion> resistPotion;
    std::vector<CarrierPotion> carrierPotion;
    std::vector<uint32_t> retaunt;   // bosses whose threat a Blink, a fear or a threat reset (Ragnaros' Wrath) can leave on a non-tank: a tank taunts them back at once (ElectRetaunt)
    std::vector<ThreatDrop> threatDrop;   // bosses that cut their tank's threat on a schedule (ThreatDropFraction)
    std::vector<uint32_t> noDispel;   // auras nobody removes (removing them hurts: Wyvern Sting, Mutating Injection)
    std::vector<uint32_t> noTank;   // summoned adds the DPS burns down: tanks hold every other mob first and never count them as a roster need
    std::vector<uint32_t> tankOrder;   // mobs the tanks take in this order, first listed first (Majordomo: Healers, then Elites); unlisted mobs come after
    std::vector<uint32_t> tankLast;    // a mob the tanks take only when no other free mob is left (Majordomo: he does little damage and cannot die)
    std::vector<uint32_t> interruptFirst;   // casts the PvE coordinator's kick election takes before any other (heals, sheep)
};

// Registry. Returns nullptr when the map has no rules.
RaidRules const* RulesForMap(uint32_t mapId);

// True when the raid layer decides what happens to <aura> on this map: it elects the dispel itself (dispel,
// controlledMember) or nobody may remove it (noDispel). The generic dispel assignment leaves these alone.
bool OwnsAura(uint32_t mapId, uint32_t aura);

// True when the map's rules say this cast is kicked before any other cast in reach (an enemy heal, a polymorph). Rules are
// static data, so the PvE coordinator may call it from its planning pass.
bool InterruptFirst(uint32_t mapId, uint32_t spell);

// True when the map's rules list <entry> as an add the tanks do not hold (Firelord's Lava Spawns): the tank assignment gives a
// tank such a mob only when no other engaged mob is free, and the tank roster does not grow for it.
bool NoTank(uint32_t mapId, uint32_t entry);

// RaidRules::tankOrder: the position of <entry> in the map's tank order (0 = first), or UINT32_MAX when it is not listed.
uint32_t TankOrder(uint32_t mapId, uint32_t entry);

// True when the map's rules list <entry> in RaidRules::tankLast.
bool TankLast(uint32_t mapId, uint32_t entry);

// True when the map's rules list <entry> as a boss whose threat a scripted reset or a Blink can leave on a non-tank (RaidRules::retaunt): a
// tank taunts it back at once, so the raid's wait for that tank is short.
bool Retaunts(uint32_t mapId, uint32_t entry);

// The share of its tank's threat that stays after <entry>'s scheduled threat cut (ThreatDrop), 1.0 when the map's rules list none. Non-tank
// damage holds at this share of the tank's threat on that boss (EvaluateThreatHold).
float ThreatShareAfterDrop(uint32_t mapId, uint32_t entry);
float ThreatShareAfterDrop(RaidRules const& rules, uint32_t entry);

// Rule-based kill priority for an enemy entry (default 1 when no rule exists).
int PriorityFor(RaidRules const& rules, uint32_t entry);

// Entries with priority >= 2, highest first (the dungeon advice's high_priority_kill_entries).
std::vector<uint32_t> KillOrder(uint32_t mapId);

// Data registration entry points, one per data file.
void RegisterMoltenCoreMovement(RaidRules& rules);
void RegisterMoltenCore(RaidRules& rules);
void RegisterBlackwingLair(RaidRules& rules);
void RegisterBlackwingMovement(RaidRules& rules);
void RegisterOnyxia(RaidRules& rules);
void RegisterNaxxramas(RaidRules& rules);
void RegisterTempleOfAhnQiraj(RaidRules& rules);

} // namespace Playerbot::Raid
