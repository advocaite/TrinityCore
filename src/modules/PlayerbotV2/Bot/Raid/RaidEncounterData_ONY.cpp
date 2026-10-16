#include "RaidEncounters.h"

// Ported from Karrz's raid10-revamp ike3 bots (Karrz1/wow-1.12). Script references (mc/..., bwl/..., ony/...) point at the
// VMaNGOS scripts that Forever's scripts/Custom/Classic instances were ported from; the ids are the same.

namespace Playerbot::Raid {

    // Onyxia's Lair (map 249). Source: src/scripts/kalimdor/dustwallow_marsh/onyxias_lair/boss_onyxia.cpp (abbreviated ony/boss_onyxia.cpp).
    // Boss entry 10184 (Onyxia, dragonkin, 1.1M hp), whelp 11262 (2.8k hp), warder 12129 (83k hp elite, dragonkin).
    //
    // Fight as scripted: phase 1 (100-65%) on the ground: Flame Breath 18435 (cone, 2s cast), Cleave 19983, Wing Buffet 18500,
    // Knock Away 19633 (-25% threat), Tail Sweep 15847 behind her every 3.5s (ony/boss_onyxia.cpp:362-418).
    // Phase 2 (65-40%) she flies between 8 waypoints, casts Fireball 18392 at her victim with a 100% threat wipe so the target rotates,
    // sometimes a Deep Breath along one of 8 paths (35% clockwise / 35% counter-clockwise / 30% breath, :503-536) and summons 5-7
    // whelps per wave from two caves at (-30.1,-254.5,-89.4) and (-30.8,-177.1,-89.3) (:109-113,473-500).
    // Phase 3 (<40%) she lands at (-8.86,-212.75,-87.5), Bellowing Roar 18431 fear every 15-30s and a whelp every 1-10s (:540-565).
    void RegisterOnyxia(RaidRules& r)
    {
        r.name = "Onyxia's Lair";
        r.bosses = { 10184 };

        // Knock Away (19633) every 15-30 s takes 25 percent of her victim's threat (classic_boss_onyxia.cpp ModifyThreatByPercent -25). The
        // raid held at 100 percent of the tank's, so every cut put the mages at 133 percent and she turned on one (n8 run 1: a Taunt every
        // 9-12 s, her facing swinging the melee cones with it).
        r.threatDrop.push_back({ 10184, 25.0f });

        // Whelps die to a single AoE tick but several at once overwhelm healers; the Warders sit in the egg pit and are respawned
        // when Onyxia aggroes (ony/boss_onyxia.cpp:298-302) - they are not part of the kill.
        // Phase 2 (65..40 percent) she flies and only ranged can reach her; in n24 run 2 every DPS (casters and hunters included) went
        // to the whelps and she sat at 57-61 percent from 121 s to 333 s while the waves stacked. Ranged DPS stay on her; the melee and an
        // AoE share of the casters (every second one by guid) take the whelps. Tanks and everyone outside phase 2 rank the whelps above
        // her (a landed Onyxia in phase 1 and 3 is the tank's, the whelps are the adds). First matching entry wins.
        r.priority.push_back({ 11262, 2, Who::Tank,     10184, 65.0f, 40.0f });   // tanks pick up the whelps while she flies
        r.priority.push_back({ 11262, 2, Who::Melee,    10184, 65.0f, 40.0f });
        r.priority.push_back({ 11262, 0, Who::NonTank,  10184, 65.0f, 40.0f });   // below Onyxia (1): the rest stay on her
        r.priority.push_back({ 11262, 2, Who::Anyone,   10184, 100.0f, 65.0f });
        r.priority.push_back({ 11262, 2, Who::Anyone,   10184, 40.0f, 0.0f });
        r.priority.push_back({ 12129, 0 });   // Onyxian Warder: below Onyxia, never chased (ony/boss_onyxia.cpp:71,298-302)

        // Phase 2 Fireball (18392) is cast at her current victim and the victim changes every cast (ony/boss_onyxia.cpp:461-466);
        // splash/standing-in-a-stack is not in the script, the 8-yard value is taken from the AzerothCore bots (raid-research.md), so it is
        // kept small. Only ranged and healers, tanks have to stay put for the whelps.
        // Phase window 65..40 percent: Fireball is a phase 2 spell, so the spread no longer scatters the raid in phases 1 and 3.
        r.spread.push_back({ 10184, 8.0f, Who::Ranged, 65.0f, 40.0f });    // UNVERIFIED radius (spell 18392 area not in the scripts)
        r.spread.push_back({ 10184, 8.0f, Who::Healer, 65.0f, 40.0f });    // UNVERIFIED radius

        // Deep Breath (phase 2, boss_onyxia.cpp:512-538): she casts the path spell of her CURRENT waypoint on herself (5 s cast), then flies
        // to the opposite waypoint at speed rate 3.0. The spell's chained triggers (250 ms apart, 20 steps) land on fixed points along the
        // line between the two waypoints, whatever her position and facing: the script starts the breath where its timer runs out, often
        // mid-flight. onyxia_full n31 runs 5 and 6: she stood at -8.7,-216.5 facing east, the old corridor (taken from her position and
        // facing) ran east, the breath ran west along the north-south line and killed 16 of 40 both times; every death sat within 31 y of
        // that line. So the corridor is the segment between the two waypoints of the cast spell (ClassicOnyxMoveData in
        // classic_boss_onyxia.cpp), half width 34. The hall is about 70 y across, the safe ground is the two cave mouths at its sides.
        struct BreathPath { uint32_t spell; float ax, ay, bx, by; };
        static constexpr BreathPath kPaths[] =
        {
            { 17086u,  22.8763f, -217.152f, -65.8444f, -213.809f },    // north to south
            { 18351u, -65.8444f, -213.809f,  22.8763f, -217.152f },    // south to north
            { 18576u, -31.4963f, -250.123f, -33.5561f, -182.682f },    // east to west
            { 18609u, -33.5561f, -182.682f, -31.4963f, -250.123f },    // west to east
            { 18564u, -63.5156f, -240.096f,   6.8951f, -180.246f },    // south-east to north-west
            { 18584u,   6.8951f, -180.246f, -63.5156f, -240.096f },    // north-west to south-east
            { 18596u, -58.2509f, -189.020f,  10.2191f, -247.912f },    // south-west to north-east
            { 18617u,  10.2191f, -247.912f, -58.2509f, -189.020f },    // north-east to south-west
        };
        for (BreathPath const& path : kPaths)
            r.avoidBreath.push_back({ 10184, path.spell, 34.0f, 0.0f, Who::Anyone, false, true, path.ax, path.ay, path.bx, path.by });

        // Phase 1 on the ground: Flame Breath 18435, Wing Buffet 18500 and Cleave 19983 hit the front, Tail Sweep 15847 the back
        // (classic_boss_onyxia.cpp:20-25). Only the tank stands in front, nobody stands behind her; melee and casters take the
        // flanks. The cones skip her while she flies (phase 2), where her facing changes at every waypoint. Lengths are the spells' radii
        // from the spell audit (research/pc-2026-10-11/SpellAudit.tsv): Flame Breath 45 y (3500 fire, 2 s cast), Tail Sweep 30 y (800),
        // Wing Buffet 20 y. The cone angle (45 degrees either side) is still the engine's default for a front cone, UNVERIFIED.
        // onyxia_full n17 2026-10-14: casters died to hits of 2000-3900 from 19-38 y away, outside the 25 y this rule had.
        r.avoidFrontal.push_back({ 10184, 45.0f, 45.0f, Who::NonTank, 0.0f, 100.0f, 0.0f, false });
        r.avoidFrontal.push_back({ 10184, 30.0f, 80.0f, Who::NonTank, 180.0f, 100.0f, 0.0f, false });

        // Where to stand for each breath, the AzerothCore bots' safe zones (mod-playerbots Ai/Raid/Ony/OnyActions.h): two points per breath,
        // 32-53 y off its line, at the hall's ends. A member inside a corridor walks to the nearer one.
        struct SafeSpot { uint32_t spell; float x1, y1, x2, y2; };
        static constexpr SafeSpot kSafe[] =
        {
            { 17086u, -10.0f, -180.0f, -20.0f, -250.0f }, { 18351u, -10.0f, -180.0f, -20.0f, -250.0f },
            { 18576u,  20.0f, -210.0f, -75.0f, -210.0f }, { 18609u,  20.0f, -210.0f, -75.0f, -210.0f },
            { 18564u, -60.0f, -195.0f,  10.0f, -240.0f }, { 18584u, -60.0f, -195.0f,  10.0f, -240.0f },
            { 18596u,   7.0f, -185.0f, -60.0f, -240.0f }, { 18617u,   7.0f, -185.0f, -60.0f, -240.0f },
        };
        for (SafeSpot const& s : kSafe)
        {
            r.breathSafe.push_back({ 10184, s.spell, s.x1, s.y1 });
            r.breathSafe.push_back({ 10184, s.spell, s.x2, s.y2 });
        }

        // Greater Fire Protection Potion for everyone: Deep Breath (1500 fire a tick) cannot be left on its north-south and south-north lines, where the
        // corridor covers the width of the hall (n71 live 2026-10-15: two such breaths killed 35 of 40, the raid found no way out in the west half).
        r.resistPotion.push_back({ 10184, 13457, Who::Anyone });

        // The tank holds her on the far side from the raid, so her front (Flame Breath, Wing Buffet, Cleave) points at the wall and not at the
        // raiders. One step to the far side, then it stays: the tank's spot lies due east of her (bearing 0, the back of the lair; the raid enters from the west), so
        // there is no repeated turning. The cone-triggered turn of AvoidFrontal is off for her (tankTurns false: n57 run 1, the tank turned her
        // every 9 s and the ranged re-positioned each time). Phases 1 and 3; in phase 2 she flies.
        r.faceAway.push_back({ 10184, 100.0f, 65.0f, 0.0f });
        r.faceAway.push_back({ 10184, 40.0f, 0.0f, 0.0f });

        // Melee DPS stand on her flank, in the wedge between the front cones (45 degrees) and the tail sweep (everything behind, from 90):
        // 55..85 degrees off her facing, 3-8 yards out. Phases 1 and 3 only; she flies in phase 2 (the rule skips a flying enemy too).
        // Onyxia n7 2026-10-14: six melee DPS died to one tail sweep at 19 s while alternating "leave cone" and attack.
        r.standFlank.push_back({ 10184, Who::Melee, 2.0f, 9.0f, 70.0f, 20.0f, 100.0f, 65.0f });
        r.standFlank.push_back({ 10184, Who::Melee, 2.0f, 9.0f, 70.0f, 20.0f, 40.0f, 0.0f });

        // Each bot has its own slot in the wedge (RaidBrain 3d: the matching members on its side share a grid of rows and columns) and that is
        // the only position rule for it in phases 1 and 3; healers take the inner band (20-26 y), ranged the outer one (26-34 y), melee 2-9 y.
        // Ranged DPS and healers take the same side wedges, out of Wing Buffet (20 y) and the tail sweep, 22-30 y from her. Her facing follows
        // whoever she is hitting; with the ranged massed on one bearing a single Flame Breath (45 y) took ten of them at once, in the first
        // minute of n38 run 5 (all ten stood 48-68 degrees off her facing, 8-30 y out) and in a dozen breaths of run 6's phase 3, where the
        // whole raid stood round her. In the wedges only the side she turns to is exposed, and the other side is clear. Phases 1 and 3.
        r.standFlank.push_back({ 10184, Who::Ranged, 26.0f, 34.0f, 70.0f, 20.0f, 100.0f, 65.0f });
        r.standFlank.push_back({ 10184, Who::Ranged, 26.0f, 34.0f, 70.0f, 20.0f, 40.0f, 0.0f });
        r.standFlank.push_back({ 10184, Who::Healer, 20.0f, 26.0f, 70.0f, 20.0f, 100.0f, 65.0f });
        r.standFlank.push_back({ 10184, Who::Healer, 20.0f, 26.0f, 70.0f, 20.0f, 40.0f, 0.0f });

        // The two cave alcoves (-30.1,-254.5 and -30.8,-177.1, ony/boss_onyxia.cpp whelp spawn points) are out of bounds: static whelps lie in
        // them, and raiders sent there (the off-tanks stood 6 y inside each mouth, the casters' AoE share behind them) and raiders pushed there by
        // a flank, spacing or dodge move pulled out dozens of whelps, 38 loose at 3 min (n58 live 2026-10-15). No move target may end within
        // 22 y of either centre; the whelps the script sends out come to the raid, which fights them where it stands.
        r.keepOut.push_back({ 10184, -30.1f, -254.5f, 22.0f });
        r.keepOut.push_back({ 10184, -30.8f, -177.1f, 22.0f });

        // Phase 2 range. In n50 run 1 she went 56% -> 54% in 36 s with 20 DPS "hitting": the ranged stood 54-78 y from her (dps_idle_reason
        // "target out of range" with the flying Onyxia, moving=1 for whole minutes) and the casters on the whelp share ran 36-71 y after
        // whelps at the far cave. She flies a ring and moves every 15-25 s, so the ranged that work on her follow her to within 20 y on the
        // ground (about 30 y with her 20 y altitude, the casters' range).
        r.stayNear.push_back({ 10184, 20.0f, Who::Ranged, 65.0f, 40.0f });

        // The floor cracks of phase 3: the lair's two lava trap game objects, 177984 (16.8,-225.1) and 177985 (-28.1,-190.7), both spawned in
        // our world data on map 249 and declared (GO_LAVATRAP_1/2) in the VMaNGOS boss_onyxia.cpp; nobody stands within 8 y of one. The radius is
        // UNVERIFIED (the template's trap radius was not in the data we could read).
        r.avoidGameObject.push_back({ 177984, 8.0f, Who::Anyone });
        r.avoidGameObject.push_back({ 177985, 8.0f, Who::Anyone });

        // Left out on purpose (needs engine support or coordinates the scripts do not give):
        //  - Bellowing Roar fear (18431): no dispel (fear), tremor totem / fear ward is a buff decision.
        //  - Heated Ground 22191-22202: trigger positions only, no dynamic object proven.
    }
} // namespace Playerbot::Raid
