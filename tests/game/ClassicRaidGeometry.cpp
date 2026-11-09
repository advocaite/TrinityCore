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

#include "../../src/modules/PlayerbotV2/Bot/Raid/RaidEncounters.h"
#include "../../src/modules/PlayerbotV2/Bot/Raid/RaidGeometry.h"

#include <cmath>
#include <string>
#include <vector>

using namespace Playerbot::Raid;
using namespace Playerbot::Raid::geo;

namespace
{
constexpr float kDeg = kPi / 180.0f;
}

TEST_CASE("Magmadar: the tank ends up on the far side of the boss from the raid", "[ClassicRaidGeometry]")
{
    // Boss at the origin, raid centre 20 yards to the west.
    float tx, ty;
    FaceAwaySpot(0.f, 0.f, -20.f, 0.f, 3.5f, 0.f, tx, ty);
    CHECK(tx == Catch::Approx(3.5f).margin(0.001));
    CHECK(ty == Catch::Approx(0.f).margin(0.001));

    // The boss turns toward the tank, so its back is to the raid: the raid centre is directly behind it.
    float const facing = Bearing(0.f, 0.f, tx, ty);
    CHECK(OffRear(0.f, 0.f, facing, -20.f, 0.f) == Catch::Approx(0.f).margin(0.001));
}

TEST_CASE("Magmadar: a tank already opposite the raid stays, one beside or in front of it walks round", "[ClassicRaidGeometry]")
{
    float const tol = 30.f * kDeg;
    CHECK(TankOpposesRaid(0, 0, 4, 0, -20, 0, tol));                 // dead opposite
    CHECK(TankOpposesRaid(0, 0, 4, 1.5f, -20, 0, tol));              // ~20 degrees off the line
    CHECK_FALSE(TankOpposesRaid(0, 0, 0, 4, -20, 0, tol));           // beside the boss
    CHECK_FALSE(TankOpposesRaid(0, 0, -4, 0, -20, 0, tol));          // between boss and raid, the usual pull
}

TEST_CASE("Magmadar: the rear sector holds those behind the boss and nobody in front or at its side", "[ClassicRaidGeometry]")
{
    float const facing = 0.f;                      // boss faces east, the raid belongs to the west
    float const half = 70.f * kDeg;
    CHECK(InRearSector(0, 0, facing, -22, 0, half, 18, 26));
    CHECK(InRearSector(0, 0, facing, -22 * std::cos(50 * kDeg), 22 * std::sin(50 * kDeg), half, 18, 26));
    CHECK_FALSE(InRearSector(0, 0, facing, 22, 0, half, 18, 26));     // in front
    CHECK_FALSE(InRearSector(0, 0, facing, 0, 22, half, 18, 26));     // at its side (90 degrees)
    CHECK_FALSE(InRearSector(0, 0, facing, -10, 0, half, 18, 26));    // too close (Panic range)
    CHECK_FALSE(InRearSector(0, 0, facing, -40, 0, half, 18, 26));    // too far
}

TEST_CASE("Magmadar: a bot in front of the boss walks to a spot behind it on its own side", "[ClassicRaidGeometry]")
{
    float const half = 70.f * kDeg;
    float x, y;
    // A caster at the boss's front right (north-east): the nearest rear spot is on the north side, inside the sector.
    RearSpot(0, 0, 0.f, 15.f, 15.f, half, 18, 26, x, y);
    CHECK(InRearSector(0, 0, 0.f, x, y, half, 18, 26));
    CHECK(y > 0.f);
    CHECK(x < 0.f);
    // One on the south side keeps the south side.
    RearSpot(0, 0, 0.f, 15.f, -15.f, half, 18, 26, x, y);
    CHECK(InRearSector(0, 0, 0.f, x, y, half, 18, 26));
    CHECK(y < 0.f);
    // Too close and too far are pulled into range along the same bearing.
    RearSpot(0, 0, 0.f, -5.f, 0.f, half, 18, 26, x, y);
    CHECK(std::hypot(x, y) == Catch::Approx(18.f).margin(0.01));
    RearSpot(0, 0, 0.f, -60.f, 0.f, half, 18, 26, x, y);
    CHECK(std::hypot(x, y) == Catch::Approx(26.f).margin(0.01));
}

TEST_CASE("Magmadar: the raid settles once the tank has turned him", "[ClassicRaidGeometry]")
{
    // The pull: tank between boss and raid, boss faces west toward the tank and the raid. Everyone is in front.
    float const half = 70.f * kDeg;
    float const bossFacingPull = kPi;               // facing west
    CHECK_FALSE(InRearSector(0, 0, bossFacingPull, -22, 0, half, 18, 26));
    // The tank walks round; the boss turns to face him (east). The raid centre is now straight behind the boss.
    float tx, ty;
    FaceAwaySpot(0, 0, -22, 0, 3.5f, 0.f, tx, ty);
    float const facingNow = Bearing(0, 0, tx, ty);
    CHECK(InRearSector(0, 0, facingNow, -22, 0, half, 18, 26));
    CHECK(TankOpposesRaid(0, 0, tx, ty, -22, 0, 30.f * kDeg));
}

TEST_CASE("Molten Core rules: Magmadar is turned away by the tank and the raid stands behind him", "[ClassicRaidGeometry]")
{
    RaidRules const* r = RulesForMap(409);
    REQUIRE(r != nullptr);
    bool face = false;
    for (FaceAway const& f : r->faceAway)
        face = face || f.enemy == 11982;
    CHECK(face);
    bool melee = false, ranged = false, healer = false;
    for (StandBehind const& s : r->standBehind)
    {
        if (s.enemy != 11982)
            continue;
        CHECK(s.minDist < s.maxDist);
        CHECK(s.halfAngleDeg > 0.f);
        CHECK(s.halfAngleDeg < 90.f);          // a sector wider than a half plane is not "behind"
        melee = melee || s.who == Who::Melee;
        ranged = ranged || s.who == Who::Ranged;
        healer = healer || s.who == Who::Healer;
    }
    CHECK(melee);
    CHECK(ranged);
    CHECK(healer);
}

#include "../../src/modules/PlayerbotV2/Bot/Raid/RaidReadiness.h"

TEST_CASE("Ready check: a missing boss need is spelled out for the raid owner", "[ClassicRaidReadiness]")
{
    constexpr uint32_t hunter = 1u << 3, priest = 1u << 5, shaman = 1u << 7;
    std::vector<NeedGap> gaps = {
        { "tranquilizing shot (Frenzy)", 0, 1, hunter },
        { "fear cover (Panic)", 3, 1, priest | shaman },     // covered: not mentioned
        { "healers", 6, 8, 0 },
    };
    CHECK(ReadyWarning("Magmadar", gaps) == "Raid check for Magmadar: missing tranquilizing shot (Frenzy) 0/1 (Hunter); healers 6/8.");
}

TEST_CASE("Ready check: nothing is said when the raid has everything", "[ClassicRaidReadiness]")
{
    CHECK(ReadyWarning("Magmadar", {}).empty());
    CHECK(ReadyWarning("Magmadar", { { "tranquilizing shot (Frenzy)", 2, 1, 1u << 3 } }).empty());
}

TEST_CASE("Ready check: class names list every class that can do the job", "[ClassicRaidReadiness]")
{
    CHECK(ClassNamesOf((1u << 5) | (1u << 7)) == "Priest/Shaman");
    CHECK(ClassNamesOf((1u << 8) | (1u << 11)) == "Mage/Druid");
    CHECK(ClassNamesOf(0).empty());
}

TEST_CASE("Ready check: Magmadar's fear cover is named by what can do it", "[ClassicRaidReadiness]")
{
    NeedGap fear{ "fear cover (Panic)", 0, 1, (1u << 5) | (1u << 7), "Tremor Totem shaman or Fear Ward priest" };
    CHECK(ReadyWarning("Magmadar", { fear }) ==
          "Raid check for Magmadar: missing fear cover (Panic) 0/1 (Tremor Totem shaman or Fear Ward priest).");
}

TEST_CASE("Ready check: low healer mana is reported before the pull, healthy mana is not", "[ClassicRaidReadiness]")
{
    CHECK(ReadyWarning("Magmadar", {}, 58) == "Raid check for Magmadar: Healers at 58% average mana.");
    CHECK(ReadyWarning("Magmadar", {}, 79) == "Raid check for Magmadar: Healers at 79% average mana.");
    CHECK(ReadyWarning("Magmadar", {}, 80).empty());
    CHECK(ReadyWarning("Magmadar", {}, 100).empty());
    CHECK(ReadyWarning("Magmadar", {}, -1).empty());     // not checked (the engage line)
    NeedGap hunter{ "tranquilizing shot (Frenzy)", 0, 1, 1u << 3, "" };
    CHECK(ReadyWarning("Magmadar", { hunter }, 58) ==
          "Raid check for Magmadar: missing tranquilizing shot (Frenzy) 0/1 (Hunter). Healers at 58% average mana.");
}

TEST_CASE("Ready check: the pre-pull warning is sent once per boss per minute", "[ClassicRaidReadiness]")
{
    CHECK(ReadyWarnDue(0, 5000));            // never warned
    CHECK_FALSE(ReadyWarnDue(5000, 20000));
    CHECK_FALSE(ReadyWarnDue(5000, 64999));
    CHECK(ReadyWarnDue(5000, 65000));
}

TEST_CASE("Magmadar: a tank spot a little off the straight line still turns the boss away", "[ClassicRaidGeometry]")
{
    // Every fallback the brain tries (3.5 / 2.8 / 2.2 yd, up to 28 degrees to the side) keeps the tank opposite the raid within the
    // 30 degrees that the "already facing away" check accepts, so the tank does not walk round again once it has a spot.
    for (float reach : { 3.5f, 2.8f, 2.2f })
        for (float turnDeg : { 0.0f, 15.0f, -15.0f, 28.0f, -28.0f })
        {
            float tx, ty;
            FaceAwaySpot(0, 0, -22, 0, reach, turnDeg * kDeg, tx, ty);
            CHECK(std::hypot(tx, ty) == Catch::Approx(reach).margin(0.01));
            CHECK(TankOpposesRaid(0, 0, tx, ty, -22, 0, 30.f * kDeg));
        }
    // 35 degrees off is not accepted: the brain never offers it.
    float tx, ty;
    FaceAwaySpot(0, 0, -22, 0, 3.5f, 35.f * kDeg, tx, ty);
    CHECK_FALSE(TankOpposesRaid(0, 0, tx, ty, -22, 0, 30.f * kDeg));
}

// Onyxia, live 2026-10-16 kill: all eight deaths came 3-27 s after a roar, feared bots walking back through her front arc. A raid move whose straight line
// crosses the widened arc is dropped (RaidBrain ApplyFrontArcGuard); the bot waits on its flank.
TEST_CASE("Onyxia: a walk from one flank to the other through her front arc is detected", "[ClassicRaidGeometry]")
{
    constexpr float half = 72.0f * kDeg;   // 60 degrees and the margin
    // She faces east at the origin. A bot south of her (feared there) sent to a slot on the north flank walks through the front.
    CHECK(PathCrossesFrontSector(0, 0, 0.0f, half, 45.0f, 10.0f, -20.0f, 4.0f, 24.0f));
    // The same walk on the far side of her reach is not through the arc.
    CHECK_FALSE(PathCrossesFrontSector(0, 0, 0.0f, half, 45.0f, 60.0f, -30.0f, 60.0f, 30.0f));
    // The ends are not tested: a bot already inside the arc is the cone step's business.
    CHECK_FALSE(PathCrossesFrontSector(0, 0, 0.0f, half, 45.0f, 20.0f, 0.0f, 20.0f, 0.0f));
}

TEST_CASE("Onyxia: a walk along one flank or behind her is left alone", "[ClassicRaidGeometry]")
{
    constexpr float half = 72.0f * kDeg;
    CHECK_FALSE(PathCrossesFrontSector(0, 0, 0.0f, half, 45.0f, -2.0f, 25.0f, 3.0f, 22.0f));      // along the north flank
    CHECK_FALSE(PathCrossesFrontSector(0, 0, 0.0f, half, 45.0f, -15.0f, -20.0f, -2.0f, -24.0f));  // south flank, a little behind
    CHECK_FALSE(PathCrossesFrontSector(0, 0, 0.0f, half, 45.0f, -20.0f, 20.0f, -20.0f, -20.0f));  // from flank to flank behind her
    CHECK(InFrontSector(0, 0, 0.0f, 20.0f, 10.0f, half, 45.0f));
    CHECK_FALSE(InFrontSector(0, 0, 0.0f, 5.0f, 24.0f, half, 45.0f));   // 78 degrees off her facing: in the wedge, outside the widened arc
    CHECK_FALSE(InFrontSector(0, 0, 0.0f, 50.0f, 0.0f, half, 45.0f));   // beyond the arc's reach
}
