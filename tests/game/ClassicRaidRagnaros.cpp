/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "tc_catch2.h"
#include <algorithm>
#include <cmath>

#include "../../src/modules/PlayerbotV2/Bot/Raid/RaidEncounters.h"

using namespace Playerbot::Raid;

namespace
{
constexpr uint32_t kRagnaros = 11502;
RaidRules const& Mc()
{
    RaidRules const* r = RulesForMap(409);
    REQUIRE(r != nullptr);
    return *r;
}
}

// MC 2026-10-12 (c8456ad0): Wrath of Ragnaros (20566) runs ResetThreatList; with nobody in melee reach he casts Magma Blast (6000) on the top threat.
TEST_CASE("Ragnaros is taken back by a tank after his Wrath resets threat", "[ClassicRaidRagnaros]")
{
    RaidRules const& r = Mc();
    CHECK(std::find(r.retaunt.begin(), r.retaunt.end(), kRagnaros) != r.retaunt.end());
    // The bosses that had the rule before it became data keep it, and Geddon, Garr, Golemagg and Sulfuron take their boss back from a DPS too.
    for (uint32_t boss : { 12118u, 11982u, 12259u, 12264u, 12056u, 12057u, 11988u, 12098u })
        CHECK(std::find(r.retaunt.begin(), r.retaunt.end(), boss) != r.retaunt.end());
}

// Five tank deaths in three minutes to Elemental Fire (20564, 2400 a tick, reapplied by every swing).
TEST_CASE("Ragnaros' tanks swap on Elemental Fire and drink Greater Fire Protection", "[ClassicRaidRagnaros]")
{
    RaidRules const& r = Mc();
    bool swap = false;
    for (TankSwap const& t : r.tankSwap)
        if (t.enemy == kRagnaros)
        {
            swap = true;
            CHECK(t.debuffAura == 20564u);
            CHECK(t.stacks == 1u);
        }
    CHECK(swap);
    bool potion = false;
    for (ResistPotion const& p : r.resistPotion)
        if (p.enemy == kRagnaros)
        {
            potion = true;
            CHECK(p.item == 13457u);
            CHECK(p.who == Who::Anyone);   // Might of Ragnaros: 2000 fire to 20 y round a random mana user
        }
    CHECK(potion);
}

// Lava Burst (21908) summons game object 178088 for 3 s; its trap (spell 21158) triggers within 3.5 y.
TEST_CASE("Ragnaros' Lava Burst traps are avoided by everybody but the tanks", "[ClassicRaidRagnaros]")
{
    RaidRules const& r = Mc();
    bool found = false;
    for (AvoidGameObject const& g : r.avoidGameObject)
        if (g.entry == 178088u)
        {
            found = true;
            CHECK(g.radius > 3.5f);          // outside the trigger radius
            CHECK(g.who == Who::NonTank);    // the tanks stay in his melee reach
        }
    CHECK(found);
}

TEST_CASE("Ragnaros' casters and healers stand outside his knockback", "[ClassicRaidRagnaros]")
{
    RaidRules const& r = Mc();
    bool ranged = false, healer = false;
    for (AvoidEnemy const& a : r.avoidEnemy)
        if (a.enemy == kRagnaros)
        {
            CHECK(a.radius >= 20.0f);
            ranged = ranged || a.who == Who::Ranged;
            healer = healer || a.who == Who::Healer;
        }
    CHECK(ranged);
    CHECK(healer);
}

// MC Geddon 2026-10-12 (b0a1c616): the ranged / healer slot rows start at the circle's radius + the hazard margin + 1 (State_Idle.cpp,
// RaidRangedApproachTank), measured from Geddon; the Inferno circle and the standing circle must be the same number or the rows move
// while it opens, and the rows must stay inside the 33 y spell reach (30 y spells plus the mob's reach).
TEST_CASE("Geddon's Inferno circle and his ranged / healer circle agree and leave the rows in spell reach", "[ClassicRaidGeddon]")
{
    RaidRules const& r = Mc();
    float aura = 0.f, ranged = 0.f, healer = 0.f;
    for (AvoidEnemyAura const& a : r.avoidEnemyAura)
        if (a.enemy == 12056u)
            aura = a.radius;
    for (AvoidEnemy const& a : r.avoidEnemy)
        if (a.enemy == 12056u)
        {
            if (a.who == Who::Ranged) ranged = a.radius;
            if (a.who == Who::Healer) healer = a.radius;
        }
    CHECK(aura > 21.5f);            // Inferno's radius 20 plus the victim's combat reach
    CHECK(ranged == aura);
    CHECK(healer == aura);
    CHECK(aura + 2.0f + 1.0f + 3.0f <= 33.0f);   // the second row (one spacing further out) is still in reach
}

// MC Geddon 2026-10-13 (09a8ff07): three casters died to one Living Bomb Explosion (20476: radius index 19 = 18 yards from the carrier plus the
// victim's reach) because the carrier only got 13-15 y from the nearest raider.
TEST_CASE("Living Bomb: the carrier runs, and the raid keeps, more than the Explosion's 18 yards", "[ClassicRaidGeddon]")
{
    RaidRules const& r = Mc();
    bool run = false, avoid = false;
    for (CarrierRun const& c : r.carrierRun)
        if (c.aura == 20475u)
        {
            run = true;
            CHECK(c.clearDistance >= 19.5f);
        }
    for (AvoidCarrier const& c : r.avoidCarrier)
        if (c.aura == 20475u)
        {
            avoid = true;
            CHECK(c.radius >= 19.5f);
        }
    CHECK(run);
    CHECK(avoid);
}

// MC 2026-10-13 (5b31e3df): Living Bomb's Explosion (3200) hits its carrier too; a caster with 3150 health died of its own bomb 29 y from the boss.
TEST_CASE("Baron Geddon's raid drinks Greater Fire Protection against the Living Bomb", "[ClassicRaidRagnaros]")
{
    RaidRules const& r = Mc();
    bool potion = false;
    for (ResistPotion const& p : r.resistPotion)
        if (p.enemy == 12056u)
        {
            potion = true;
            CHECK(p.item == 13457u);
            CHECK(p.who == Who::Anyone);   // any role can carry the bomb
        }
    CHECK(potion);
}

// n24 onyxia run 2: every DPS was ranked onto the whelps while Onyxia flew and she never left 57-61 percent.
TEST_CASE("Onyxia's whelps outrank her only for melee in phase 2", "[ClassicRaidRagnaros]")
{
    RaidRules const* r = RulesForMap(249);
    REQUIRE(r != nullptr);
    int phase2Ranged = 1, phase2Melee = 0, phase2Share = 0;   // share must stay 0: ranged stay on her
    bool gatedOnly = true;
    for (Priority const& p : r->priority)
    {
        if (p.enemy != 11262)
            continue;
        if (p.gateEnemy != 10184)
            gatedOnly = false;
        if (p.hpMax == 65.0f && p.hpMin == 40.0f)
        {
            if (p.who == Who::NonTank) phase2Ranged = p.value;
            if (p.who == Who::Melee) phase2Melee = p.value;
            if (p.who == Who::AoeShare) phase2Share = p.value;
        }
    }
    CHECK(phase2Ranged < 1);      // below Onyxia's default 1
    CHECK(phase2Melee >= 2);
    CHECK(phase2Share == 0);
    CHECK(gatedOnly);
    CHECK(std::find(KillOrder(249).begin(), KillOrder(249).end(), 11262u) == KillOrder(249).end());   // no unconditional group focus on whelps
}

// n31 onyxia_full runs 5 and 6: the Deep Breath corridor must come from the cast spell's waypoints, not from her position and facing.
TEST_CASE("Onyxia's Deep Breath corridors are fixed paths between opposite waypoints", "[ClassicRaidRagnaros]")
{
    RaidRules const* r = RulesForMap(249);
    REQUIRE(r != nullptr);
    int count = 0;
    for (AvoidBreath const& b : r->avoidBreath)
    {
        if (b.enemy != 10184)
            continue;
        ++count;
        CHECK(b.fixedPath);
        CHECK(b.halfWidth >= 31.0f);   // every n31 victim sat within 31 y of its line
        float const len = std::sqrt((b.bx - b.ax) * (b.bx - b.ax) + (b.by - b.ay) * (b.by - b.ay));
        CHECK(len > 60.0f);
        CHECK(len < 100.0f);
    }
    CHECK(count == 8);
}

// n38 onyxia_full: ten ranged massed on one bearing died to one Flame Breath; ranged and healers take the flank wedges like the melee.
TEST_CASE("Onyxia's ranged and healers stand in the flank wedges in phases 1 and 3", "[ClassicRaidRagnaros]")
{
    RaidRules const* r = RulesForMap(249);
    REQUIRE(r != nullptr);
    for (Who who : { Who::Melee, Who::Ranged, Who::Healer })
    {
        int phases = 0;
        for (StandFlank const& f : r->standFlank)
            if (f.enemy == 10184 && f.who == who)
            {
                ++phases;
                // Outside the widened front cone and in front of the widened tail sweep (read from the rules, so a change of either moves the test).
                float frontEdge = 0.0f, tailEdge = 180.0f;
                for (AvoidFrontal const& c : r->avoidFrontal)
                    if (c.enemy == 10184)
                    {
                        if (c.facingOffsetDeg == 0.0f)
                            frontEdge = ConeEdgeDeg(c);
                        else
                            tailEdge = 180.0f - ConeEdgeDeg(c);
                    }
                CHECK(f.centerDeg - f.halfAngleDeg > frontEdge);
                CHECK(f.centerDeg + f.halfAngleDeg < tailEdge);
                if (who != Who::Melee)
                {
                    CHECK(f.minDist >= 20.0f);                  // outside Wing Buffet
                    CHECK(f.maxDist <= 29.0f);                  // inside the casters' 30 y reach: the range step must not pull them out again
                }
            }
        CHECK(phases == 2);
    }
}

// n58 live 2026-10-15: posts and dodge moves at the two cave mouths pulled out the static whelps; no raid move may end near the caves.
TEST_CASE("Onyxia's cave alcoves are out of bounds and no post stands in them", "[ClassicRaidRagnaros]")
{
    RaidRules const* r = RulesForMap(249);
    REQUIRE(r != nullptr);
    int caves = 0;
    for (KeepOut const& k : r->keepOut)
        if (k.enemy == 10184)
        {
            ++caves;
            CHECK(k.radius == kOnyCaveKeepOutRadius);
            CHECK(k.radius <= 13.0f);   // the whelps' pull range (15 y less combat reach, plus the level difference), no wider
            CHECK(std::fabs(k.x + 30.5f) < 1.0f);
            CHECK((std::fabs(k.y + 254.5f) < 1.0f || std::fabs(k.y + 177.1f) < 1.0f));
        }
    CHECK(caves == 2);
    for (Stand const& s : r->stand)
        if (s.enemy == 10184)
            for (KeepOut const& k : r->keepOut)
                CHECK(std::hypot(s.x - k.x, s.y - k.y) >= k.radius);
}

// Live 2026-10-16 (kill, 405 s): she stood at her spawn spot (-34.6,-212.6) and the wedge's side slots lay inside the 22 y cave circles, so the casters
// had no slot. At both of her spots every role must have a valid distance inside its band on every column of both wedges (the brain tries the band
// in 2 y steps from the slot's own distance, RaidBrain 3d), outside both cave circles.
TEST_CASE("Onyxia's flank wedges have a valid slot outside the cave circles at both of her spots", "[ClassicRaidRagnaros]")
{
    RaidRules const* r = RulesForMap(249);
    REQUIRE(r != nullptr);
    struct Spot { float x, y; };
    for (Spot const& boss : { Spot{ -34.6f, -212.6f }, Spot{ -8.86f, -212.75f } })
        for (StandFlank const& f : r->standFlank)
        {
            if (f.enemy != 10184)
                continue;
            for (float side : { -1.0f, 1.0f })
                for (float deg = f.centerDeg - f.halfAngleDeg; deg <= f.centerDeg + f.halfAngleDeg + 0.01f; deg += 3.0f)
                {
                    float const angle = side * deg * (3.14159265f / 180.0f);   // her facing is east (bearing 0)
                    bool valid = false;
                    for (float d = f.minDist; d <= f.maxDist + 0.01f && !valid; d += 2.0f)
                    {
                        float const x = boss.x + d * std::cos(angle), y = boss.y + d * std::sin(angle);
                        valid = true;
                        for (KeepOut const& k : r->keepOut)
                            if (k.enemy == 10184 && std::hypot(x - k.x, y - k.y) < k.radius)
                                valid = false;
                    }
                    CHECK(valid);
                }
        }
}

// n50 onyxia_full run 1: the ranged stood 54-78 y from the flying Onyxia and she did not lose health in phase 2.
TEST_CASE("Onyxia's phase 2 ranged follow her", "[ClassicRaidRagnaros]")
{
    RaidRules const* r = RulesForMap(249);
    REQUIRE(r != nullptr);
    bool follow = false;
    for (StayNear const& s : r->stayNear)
        if (s.enemy == 10184 && s.who == Who::Ranged && s.hpMax == 65.0f && s.hpMin == 40.0f)
        {
            follow = true;
            CHECK(s.radius <= 22.0f);   // 20 y of altitude leave about 30 y of range
        }
    CHECK(follow);
}

// n57 onyxia_full run 1: the main tank turned Onyxia away from the raid every 9 s; the ranged on the flank wedges re-positioned each time.
TEST_CASE("Onyxia's frontal cones do not make the tank turn her", "[ClassicRaidRagnaros]")
{
    RaidRules const* r = RulesForMap(249);
    REQUIRE(r != nullptr);
    int cones = 0;
    for (AvoidFrontal const& f : r->avoidFrontal)
        if (f.enemy == 10184)
        {
            ++cones;
            CHECK(!f.tankTurns);
            CHECK(f.marginDeg > 0.0f);   // a cast lands two seconds after she turns
        }
    CHECK(cones == 2);
}

// Third live pull 2026-10-16: 27 of 38 deaths were Flame Breath at raiders 55-77 degrees off her facing, one breath 117 degrees across.
TEST_CASE("Onyxia's Flame Breath cone is modelled wider than 45 degrees with a margin, and the wedges sit between it and the tail", "[ClassicRaidRagnaros]")
{
    CHECK(kOnyFrontHalfDeg >= 60.0f);
    CHECK(kOnyWedgeLoDeg == kOnyFrontHalfDeg + kOnyConeMarginDeg + kOnyWedgeGapDeg);
    CHECK(kOnyWedgeHiDeg == 180.0f - kOnyTailHalfDeg - kOnyConeMarginDeg - kOnyWedgeGapDeg);
    CHECK(kOnyWedgeHalfDeg >= 8.0f);   // room for a row of casters
    CHECK(kOnyWedgeCentreDeg - kOnyWedgeHalfDeg == kOnyWedgeLoDeg);
    CHECK(kOnyWedgeCentreDeg + kOnyWedgeHalfDeg == kOnyWedgeHiDeg);
    RaidRules const* r = RulesForMap(249);
    REQUIRE(r != nullptr);
    bool front = false;
    for (AvoidFrontal const& f : r->avoidFrontal)
        if (f.enemy == 10184 && f.facingOffsetDeg == 0.0f)
        {
            front = true;
            CHECK(f.halfAngleDeg == kOnyFrontHalfDeg);
            CHECK(f.radius >= 45.0f);                          // Flame Breath's 45 y
            CHECK(ConeEdgeDeg(f) == kOnyFrontHalfDeg + kOnyConeMarginDeg);
        }
    CHECK(front);
}

// 2026-10-15: the tank holds Onyxia with her back to the raid in the two ground phases (one locked step, not a turn at every cone).
TEST_CASE("Onyxia's tank faces her away from the raid in the ground phases", "[ClassicRaidRagnaros]")
{
    RaidRules const* r = RulesForMap(249);
    REQUIRE(r != nullptr);
    int phases = 0;
    for (FaceAway const& f : r->faceAway)
        if (f.enemy == 10184)
        {
            ++phases;
            CHECK(f.hpMax <= 100.0f);
            CHECK(f.hpMin < f.hpMax);
            CHECK(!(f.hpMax > 65.0f && f.hpMin < 65.0f));   // no window runs into the flying phase
            CHECK((!(f.hpMax > 40.0f && f.hpMin < 40.0f) || f.hpMax <= 65.0f));
        }
    CHECK(phases == 2);
}

TEST_CASE("Onyxia's eight breaths each list two safe points off their line", "[ClassicRaidRagnaros]")
{
    RaidRules const* r = RulesForMap(249);
    REQUIRE(r != nullptr);
    for (AvoidBreath const& path : r->avoidBreath)
    {
        int n = 0;
        for (BreathSafe const& s : r->breathSafe)
            if (s.enemy == path.enemy && s.castSpell == path.castSpell)
                ++n;
        CHECK(n == 2);
    }
}

// onyxia_full live 2026-10-16 (wipe at 267 s): the corridor opened with the 5 s cast, 16 raiders died in the second it ended, eight of them 45 y or more
// from the nearest safe point. The line is known as soon as she hovers at a waypoint, so every corridor opens on her arrival.
TEST_CASE("Onyxia's breath corridors open while she hovers at their first waypoint", "[ClassicRaidRagnaros]")
{
    RaidRules const* r = RulesForMap(249);
    REQUIRE(r != nullptr);
    int hover = 0;
    for (AvoidBreath const& path : r->avoidBreath)
    {
        if (path.enemy != 10184)
            continue;
        CHECK(path.hover);
        CHECK(path.who == Who::Anyone);   // tanks and healers leave the line too
        ++hover;
        // At its own waypoint, and a few yards off it, the hover breath is this one.
        AvoidBreath const* at = HoverBreath(*r, 10184, path.ax, path.ay, 12.0f);
        REQUIRE(at != nullptr);
        CHECK(at->castSpell == path.castSpell);
        CHECK(HoverBreath(*r, 10184, path.ax + 5.0f, path.ay - 5.0f, 12.0f) == at);
        // The end point of the line is the opposite waypoint: her next hover breath there is another spell.
        AvoidBreath const* far = HoverBreath(*r, 10184, path.bx, path.by, 12.0f);
        REQUIRE(far != nullptr);
        CHECK(far->castSpell != path.castSpell);
        CHECK(far->ax == path.bx);
    }
    CHECK(hover == 8);
    // The middle of the hall is no waypoint, and another enemy has no hover breath.
    CHECK(HoverBreath(*r, 10184, -32.0f, -215.0f, 12.0f) == nullptr);
    CHECK(HoverBreath(*r, 12118u, 22.8763f, -217.152f, 12.0f) == nullptr);
}

// The breath damage is one instant at the end of the cast and the flight after it hurts nobody: a corridor held much longer shut the raid in while
// Heated Ground burned (rays in avoid circle on every exit).
TEST_CASE("A cast breath's corridor closes soon after its damage", "[ClassicRaidRagnaros]")
{
    CHECK(kBreathCorridorOpenMs >= 6000u);    // the 5 s cast, the plan interval and the trigger chain
    CHECK(kBreathCorridorOpenMs <= 10000u);
}

// Eleven raiders died in Heated Ground (22191..22202), standing on the safe points of the breaths, which lie in it.
TEST_CASE("Onyxia's Heated Ground is a ground hazard for everybody", "[ClassicRaidRagnaros]")
{
    RaidRules const* r = RulesForMap(249);
    REQUIRE(r != nullptr);
    for (uint32_t spell = 22191u; spell <= 22202u; ++spell)
    {
        bool found = false;
        for (AvoidGroundEffect const& g : r->avoidGroundEffect)
            if (g.enemy == 10184 && g.spell == spell)
            {
                found = true;
                CHECK(g.who == Who::Anyone);
                CHECK(g.margin > 0.0f);
            }
        CHECK(found);
    }
}

// MC Geddon n44 2026-10-15: a rogue, a warrior, a retribution paladin and the main tank died to Armageddon (20479, 8000 fire, radius 20 y) at
// 5 y from him. It fires when his health crosses 2 percent (mc/boss_baron_geddon.cpp:70), and the raid takes the last percent in seconds.
TEST_CASE("Geddon's melee leave his 24 y before Armageddon, the tank stays", "[ClassicRaidGeddon]")
{
    RaidRules const& r = Mc();
    bool melee = false;
    for (AvoidEnemy const& a : r.avoidEnemy)
        if (a.enemy == 12056u && a.who == Who::Melee)
        {
            melee = true;
            CHECK(a.radius > 21.5f);          // Armageddon's radius 20 plus the victim's combat reach
            CHECK(a.hpMax > 2.0f);            // above the 2 percent line, with room for the leave
            CHECK(a.hpMax <= 15.0f);          // the melee burn him for nearly all of the fight
            CHECK(a.hpMin == 0.0f);
        }
        else if (a.enemy == 12056u)
            CHECK(a.who != Who::Tank);        // he follows the tank: no avoid rule for it, a tank cannot outrun him
    CHECK(melee);
}

// Armageddon 20478 is an 8 s aura on Geddon that roots him (geddon-spell-data 2026-10-16), so the tank can leave it too, like Inferno.
TEST_CASE("Everybody, the tank included, leaves Geddon's 24 y while Armageddon runs", "[ClassicRaidGeddon]")
{
    RaidRules const& r = Mc();
    float inferno = 0.f, armageddon = 0.f;
    for (AvoidEnemyAura const& a : r.avoidEnemyAura)
        if (a.enemy == 12056u)
        {
            if (a.aura == 19695u) inferno = a.radius;
            if (a.aura == 20478u)
            {
                armageddon = a.radius;
                CHECK(a.who == Who::Anyone);
            }
        }
    CHECK(armageddon > 21.5f);
    CHECK(armageddon == inferno);
}

// Golemagg (golemagg-spell-data 2026-10-16): Earthquake 19798 is instant, 1500 physical, 18 y, every 3 s from 10 percent health; Magma Splash 13880
// stacks to 50 on whoever hits him, 50 fire a tick per stack for 30 s.
TEST_CASE("Golemagg: everybody but the tanks leaves 18 y plus reach once Earthquake starts", "[ClassicRaidGolemagg]")
{
    RaidRules const& r = Mc();
    bool found = false;
    for (AvoidEnemy const& a : r.avoidEnemy)
        if (a.enemy == 11988u)
        {
            found = true;
            CHECK(a.radius > 18.0f + 1.5f);       // the 18 y radius plus the victim's combat reach
            CHECK(a.who == Who::NonTank);         // tanks stay: he follows the main tank
            CHECK(a.hpMax == 10.0f);              // the script starts it below 10 percent (boss_golemagg.cpp:80)
            CHECK(a.hpMin == 0.0f);
        }
    CHECK(found);
}

TEST_CASE("Golemagg: a bot with 20 stacks of Magma Splash backs off his melee reach", "[ClassicRaidGolemagg]")
{
    RaidRules const& r = Mc();
    bool found = false;
    for (AvoidOnStacks const& a : r.avoidOnStacks)
        if (a.enemy == 11988u)
        {
            found = true;
            CHECK(a.aura == 13880u);
            CHECK(a.stacks >= 10);                // a few stacks are harmless, the cap is 50
            CHECK(a.stacks < 50);                 // and the rule must trigger before the cap
            CHECK(a.radius > 5.0f);               // outside melee reach
            CHECK(a.radius < 18.0f);              // and not a second Earthquake circle
            CHECK(a.who == Who::NonTank);
        }
    CHECK(found);
    // No tank swap on the splash: it could pull the Ragers' tank off them.
    for (TankSwap const& t : r.tankSwap)
        CHECK(t.enemy != 11988u);
}
