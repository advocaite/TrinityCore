#include "RaidEncounters.h"

// Ported from Karrz's raid10-revamp ike3 bots (Karrz1/wow-1.12). Script references (mc/..., bwl/..., ony/...) point at the
// VMaNGOS scripts that Forever's scripts/Custom/Classic instances were ported from; the ids are the same.

namespace Playerbot::Raid {

    // Movement and positioning for Blackwing Lair (map 469). Maintained by the lead engineer.
    void RegisterBlackwingMovement(RaidRules& r)
    {
        // The three drakes (Firemaw 11983, Ebonroc 14601, Flamegor 11981): Wing Buffet and Shadow Flame hit the cone in front of them.
        // The tank holds the front; everyone else stays out of it.
        for (uint32_t drake : { 11983u, 14601u, 11981u })
            r.avoidFrontal.push_back({ drake, 24.0f, 55.0f, Who::NonTank });
        // Firemaw's Flame Buffet 23341 (classic_boss_firemaw.cpp: cast on himself every 1.8-3 s) hits everyone he can see within 100 y for 150
        // fire and adds a stack of +150 fire damage taken, 20 s, refreshed by every hit (70532 spell data, SpellAudit.tsv), so a stack only
        // falls off out of his sight. bwl_firemaw 2026-10-20: nobody left his sight, 36 of 40 died to it 40-47 s in, at 1,400-2,000 a hit.
        // DPS step out at 4-6 stacks and come back clean; healers at 6-8, to a spot that still sees the main tank when there is one; off-tanks
        // wait out of sight, clean for the swap at 8 stacks (TankSwap, RaidEncounterData_BWL.cpp).
        r.breakSight.push_back({ 11983, 23341, 0, Who::Tank });
        r.breakSight.push_back({ 11983, 23341, 6, Who::Healer, /*seeTank*/ true });
        r.breakSight.push_back({ 11983, 23341, 6, Who::Healer });
        r.breakSight.push_back({ 11983, 23341, 4, Who::Melee });
        r.breakSight.push_back({ 11983, 23341, 4, Who::Ranged });
        // Chromaggus: his two breaths are not cones. The client data (70532 SpellEffect: Incinerate 23308, Time Lapse 23310, Corrosive
        // Acid 23313, Ignite Flesh 23315, Frost Burn 23187) targets every enemy around the caster within radius index 12 (100 y), and the
        // area test keeps line of sight, so only a wall saves a raider. bwl_chromaggus 0.1.28c: Frost Burn and Ignite Flesh killed 14
        // non-tanks in one second at 5-33 y, 62 s in, and the raid wiped at 74%. classic_boss_chromaggus.cpp casts breath one 30 s into
        // the fight and breath two at 60 s, each again every 60 s: a breath every 30 s. Everyone but his victim goes out of his sight 5 s
        // before each one is due and stays until 3 s after (the cast is 2 s), and at once whenever he is casting one; then walks back
        // to where it stood. The off-tanks hide too: only his victim and the main tank take the breath (bwl_chromaggus 0.1.28f: an
        // off-tank standing by the main tank died of Ignite Flesh 10 s after him).
        r.hideBeforeBreath.push_back({ 14020, { 23308, 23310, 23313, 23315, 23187 }, 30000, 30000, 5000, 3000, Who::Anyone });
        // The cone below only keeps the raid off his face between breaths.
        r.avoidFrontal.push_back({ 14020, 35.0f, 50.0f, Who::NonTank });
        // Vaelastrasz: his flame breath cleave chains through anyone near the front.
        r.avoidFrontal.push_back({ 13020, 20.0f, 60.0f, Who::NonTank });
        // ... and his Tail Sweep 15847 every 4-6 s (classic_boss_vaelastrasz.cpp) is Onyxia's: the 120 degree arc behind him, knocking back.
        // The same rear rule as hers (RaidEncounterData_ONY.cpp): the melee stand at his flanks, between the two arcs.
        r.avoidFrontal.push_back({ 13020, 30.0f, kOnyTailHalfDeg, Who::NonTank, 180.0f, 100.0f, 0.0f, false, kOnyConeMarginDeg });
        // Broodlord Lashlayer (12017): Blast Wave 23331 is cast on himself (boss_broodlord_lashlayer.cpp:170-173) every 20-35s; it is a
        // point-blank AoE with knockback, 2,625-3,375 fire within 20 yards (vanilla data: radius index 9), so ranged and healers stay out of 22
        // (bwl_broodlord 2026-10-20: four casters died of it standing 10 y off).
        r.avoidEnemy.push_back({ 12017, 22.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 12017, 22.0f, Who::Healer });
        // Nefarian (11583): Shadow Flame 22539 and Cleave 20691 to the front (classic_boss_nefarian.cpp:314,341). Tail Lash 23364 behind
        // him: melee stand at his sides.
        // Shadow Flame reaches 45 yards (its spell data, printed on every death to it: "radius=45"), not the 25 the cone had: of 134 bots it
        // killed in the Nefarian sweeps from 1109w to 0136-r8, 62 stood 25-46 y from him (12 of 22 in the 0136 sweeps), outside the
        // cone the bots stepped out of (healers at their hold, casters at work, bots walking). 46 = the 45 and a step for his size. The
        // drakes cast the same spell; their 24 y cone lost nobody to it in these sweeps, so it stays.
        r.avoidFrontal.push_back({ 11583, 46.0f, 55.0f, Who::NonTank });
        r.avoidFrontal.push_back({ 11583, 20.0f, 45.0f, Who::NonTank, 180.0f });
        // His tank puts his front away from the raid: with no rule he faced whoever he walked at, and with no tank on him he walked into the
        // camp (bwl_nefarian 1110a run 1: Shadow Flame killed three casters standing on the camp 9 y in front of him, Cleave and his swings
        // the healers at their post).
        r.faceAway.push_back({ 11583 });
        // Phase three (under 20%, boss_nefarian.cpp DamageTaken): Victor raises every drakonid corpse as a Bone Construct, each one on a random
        // name of his threat list (SetAggressiveStateAfter with the summoner: SelectTarget Random), about 40 of them on 40 raiders. Spread
        // out, every one ran to its own target over the whole room, the tanks held one each and the healers died first (bwl_nefarian 0136-r10:
        // all twelve healers dead 20-50 s into the phase, every run, the tanks right after). The casters and healers close up in one sector
        // behind him, so the constructs come to one place where the tanks' area taunts and the AoE reach them: past his Tail Lash (20 y),
        // inside the 30 yard spells.
        // In the sector they hold their ground (no step back from the constructs the tanks hold there): the mages' Blizzard needs them
        // standing still, and r10's casters cast one to twelve a phase while walking between spacing, cone and formation steps.
        r.standBehind.push_back({ 11583, Who::Ranged, 22.0f, 28.0f, 30.0f, 20.0f, 0.0f, true });
        r.standBehind.push_back({ 11583, Who::Healer, 22.0f, 28.0f, 30.0f, 20.0f, 0.0f, true });
        // Corrupted Infernal (14668, warlock call): Immolation 20153 burns everything close; casters and healers stay off it (ike3).
        r.avoidEnemy.push_back({ 14668, 10.0f, Who::Ranged });
        r.avoidEnemy.push_back({ 14668, 10.0f, Who::Healer });
        // Victor Nefarius' drakonids: every Brood Power hits all enemies within 15 yards of the drakonid (70532 client data, SpellAudit.tsv:
        // radius 15 on every effect): Black 22560 1,250 fire, Red 22558 a 300 a tick fire DoT stacking to 5, Blue 22559 a mana burn, Bronze
        // 22642 900 arcane and a slow, Green 22561 a stun. bwl_nefarian 2026-11-08: healers and casters stood within it beside the tanks'
        // pile, 37 of 43 deaths were Brood Power (27 of them casters, hunters and healers), a healer burned to 3% mana. Healers keep
        // 18 yards from every drakonid; the melee stand in it as a raid's melee do. The ranged DPS do not: with the rule on them too
        // (bwl_nefarian 42560db4) raid:move was the rule they fired most, raid damage fell from 3,140 to 2,240 a second and the waves
        // won earlier. They fight from range on the drakonid the tanks hold (the Nefarian fight script), outside its 15 yards.
        for (uint32_t drakonid : { 14261u, 14262u, 14263u, 14264u, 14265u, 14302u })
            r.avoidEnemy.push_back({ drakonid, 18.0f, Who::Healer });
        // The tanks hold the drakonids on an arc round one camp, where the ranged DPS stand; the healers stand just behind them. Every pile
        // is 23 yards from the camp (outside Brood Power's 15, inside the 30 yard spells), the piles 18 yards apart (out of each other's
        // Brood Power), every tank within 40 yards of the healers. bwl_nefarian 1108f: the tanks held the drakonids where they came in, at
        // the west opening (-7599,-1191, classic_boss_victor_nefarius.cpp), and 703 casts failed on line of sight in one pull. 1108g, a
        // straight row 20 yards apart across the room (-7580 to -7520 with four tanks): the line of sight failures fell to 87-123 a pull,
        // but the focus drakonid was up to 55 yards from the ranged, so they kept walking in (raid_range, raid_los, the formation walk
        // toward the main tank) past the other piles: 9-12 casters and hunters a run died of Brood Power by the row, the warlocks did 4-43
        // damage a second and the raid 1,945-2,545. The camp is next to where the raid is placed (-7555,-1239), 44 yards from Victor
        // (-7588,-1262); the arc opens north, toward both drakonid doors, so a tank coming back from a door has its drakonid on the far
        // side. The two posts hold 6 yards round their point (16 ranged, 12 healers, without the clump separation; Stand / at_post).
        // A tank goes after a loose drakonid within 15 yards and brings it back; one further out (a new one still coming in from its door
        // after its random raider) it waits for at its slot (TankSpotFor).
        constexpr float kNefCampX = -7552.0f, kNefCampY = -1236.0f, kNefHealY = -1245.0f, kNefFloorZ = 476.8f;
        // The arc and the two posts are phase one's: they end when Nefarian lands (endEnemy 11583). Victor goes invisible and rooted at the
        // landing but stays in the fight (classic_boss_victor_nefarius.cpp, phase2), so with him as the only gate the layout outlived its
        // phase: bwl_nefarian 1111j, the tank holding Nefarian walked back to its arc slot 59-86 times a run after the landing and held him
        // there, 23 y from the ranged camp and about 26 y from the healers' post, at the edge of his Shadow Flame (25 y); the posts kept the
        // casters and healers there (236 "leave cone" moves, 46 Tail Lash stuns and 47 Bellowing Roar fears in 75 s of run 1's phase 3, and 9
        // of its deaths after the landing to Shadow Flame, Cleave and Tail Lash, at the camp and the post).
        r.tankSpot.push_back({ 10162, 14263, 23.0f, 18.0f, true, kNefCampX, kNefCampY, 100.0f, { 14261u, 14262u, 14264u, 14265u, 14302u }, 11583 });
        r.stand.push_back({ 10162, Who::Ranged, kNefCampX, kNefCampY, kNefFloorZ, 6.0f, false, 0, 100.0f, 0.0f, 0, 11583 });
        r.stand.push_back({ 10162, Who::Healer, kNefCampX, kNefHealY, kNefFloorZ, 6.0f, false, 0, 100.0f, 0.0f, 0, 11583 });
        // Their priorities and dispels are in the data file.
    }
} // namespace Playerbot::Raid
