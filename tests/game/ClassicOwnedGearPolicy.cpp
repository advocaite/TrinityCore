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

#include "../../src/modules/PlayerbotV2/Bot/Gear/OwnedGearPolicy.h"

using namespace Playerbot::V2::Gear::Owned;

TEST_CASE("Below the cap a companion wears white to blue pieces up to the band above its level", "[ClassicOwnedGearPolicy]")
{
    CHECK(PieceAllowed(kQualityUncommon, 30, 21, kDefaultIlvlBand));    // a green dungeon piece
    CHECK(PieceAllowed(kQualityRare, 33, 21, kDefaultIlvlBand));        // the top of the band for level 21
    CHECK_FALSE(PieceAllowed(kQualityRare, 34, 21, kDefaultIlvlBand));  // too far above its level
    CHECK_FALSE(PieceAllowed(4, 25, 21, kDefaultIlvlBand));             // an epic is a raid or world drop, not quest gear
    CHECK_FALSE(PieceAllowed(0, 5, 21, kDefaultIlvlBand));              // grey
}

TEST_CASE("A blue outscores an equal green, a white only fills a gap", "[ClassicOwnedGearPolicy]")
{
    CHECK(QualityAdjusted(1000, kQualityUncommon) == 1000);
    CHECK(QualityAdjusted(1000, kQualityRare) == 1120);
    CHECK(QualityAdjusted(1000, kQualityCommon) == 700);
    // A piece the role scores at nothing or less (wrong stats) is not rescued by its colour.
    CHECK(QualityAdjusted(0, kQualityRare) == 0);
    CHECK(QualityAdjusted(-50, kQualityRare) == -50);
}

TEST_CASE("A green with the right stats beats a blue with the wrong ones", "[ClassicOwnedGearPolicy]")
{
    int32_t const green_for_role = QualityAdjusted(800, kQualityUncommon);
    int32_t const blue_wrong_stats = QualityAdjusted(400, kQualityRare);
    CHECK(green_for_role > blue_wrong_stats);
}

TEST_CASE("A worn piece is replaced only by a clear upgrade", "[ClassicOwnedGearPolicy]")
{
    CHECK(IsUpgrade(true, 0, 1));            // empty slot
    CHECK_FALSE(IsUpgrade(true, 0, 0));
    CHECK(IsUpgrade(false, 0, 5));           // worn piece worthless to the role
    CHECK(IsUpgrade(false, -300, -200));
    CHECK(IsUpgrade(false, 1000, 1100));     // exactly 10 % better
    CHECK_FALSE(IsUpgrade(false, 1000, 1099));
    CHECK_FALSE(IsUpgrade(false, 1000, 1000));  // a peer is not an upgrade
    CHECK_FALSE(IsUpgrade(false, 1000, 700));   // never a downgrade
}

TEST_CASE("The owner's average item level skips empty slots", "[ClassicOwnedGearPolicy]")
{
    CHECK(AverageItemLevel({}) == 0);
    CHECK(AverageItemLevel({0, 0, 0}) == 0);
    CHECK(AverageItemLevel({60, 70, 0, 80}) == 70);
}

TEST_CASE("A level-cap companion follows its owner's gear within the configured tier", "[ClassicOwnedGearPolicy]")
{
    int32_t const tier1 = 76;   // Playerbot.Gear.MaxTier = 1
    // Owner in tier 3 (average 72): slack 6 -> 78, held to the configured 76.
    CHECK(OwnerBasedCap(72, kDefaultOwnerSlack, kPreRaidFloor, tier1) == 76);
    // Owner in dungeon blues (average 62): 68.
    CHECK(OwnerBasedCap(62, kDefaultOwnerSlack, kPreRaidFloor, tier1) == 68);
    // A fresh 60 owner in quest greens (average 48): not held below pre-raid gear.
    CHECK(OwnerBasedCap(48, kDefaultOwnerSlack, kPreRaidFloor, tier1) == kPreRaidFloor);
    // Owner unknown (offline): the configured ceiling stands.
    CHECK(OwnerBasedCap(0, kDefaultOwnerSlack, kPreRaidFloor, tier1) == tier1);
    // Never above the configured tier, and a tier under the floor wins over it.
    CHECK(OwnerBasedCap(90, kDefaultOwnerSlack, kPreRaidFloor, tier1) == tier1);
    CHECK(OwnerBasedCap(48, kDefaultOwnerSlack, kPreRaidFloor, 50) == 50);
}

TEST_CASE("The gear refresh key changes on level-up and on entering or leaving an instance", "[ClassicOwnedGearPolicy]")
{
    uint64_t const open_world = RefreshKey(21, 0);
    CHECK(RefreshKey(21, 0) == open_world);
    CHECK(RefreshKey(22, 0) != open_world);       // levelled up
    CHECK(RefreshKey(21, 4711) != open_world);    // entered an instance
    CHECK(RefreshKey(21, 4711) != RefreshKey(21, 4712));   // another instance
}

TEST_CASE("The owner's words name a gear mode", "[ClassicOwnedGearPolicy]")
{
    GearMode m = GearMode::Match;
    CHECK((ParseGearMode("t3", m) && m == GearMode::T3));
    CHECK((ParseGearMode("tier 2", m) && m == GearMode::T2));
    CHECK((ParseGearMode(" tier one ", m) && m == GearMode::T1));
    CHECK((ParseGearMode("best", m) && m == GearMode::Best));
    CHECK((ParseGearMode("match", m) && m == GearMode::Match));
    CHECK((ParseGearMode("bis", m) && m == GearMode::Bis));
    CHECK((ParseGearMode("best in slot", m) && m == GearMode::Bis));
    CHECK_FALSE(ParseGearMode("t4", m));
    CHECK_FALSE(ParseGearMode("", m));
    CHECK(std::string(ModeName(GearMode::T3)) == "t3");
}

TEST_CASE("The server's allowed modes always include match and default to everything", "[ClassicOwnedGearPolicy]")
{
    uint32_t const all = ParseAllowedModes("");
    for (uint8_t i = 0; i < kGearModeCount; ++i)
        CHECK(ModeAllowed(all, GearMode(i)));
    uint32_t const some = ParseAllowedModes("best, t1");
    CHECK(ModeAllowed(some, GearMode::Match));
    CHECK(ModeAllowed(some, GearMode::Best));
    CHECK(ModeAllowed(some, GearMode::T1));
    CHECK_FALSE(ModeAllowed(some, GearMode::T2));
    CHECK_FALSE(ModeAllowed(some, GearMode::T3));
    CHECK(ParseAllowedModes("nonsense") == all);   // unreadable: everything, not nothing
    CHECK(ModeAllowed(ParseAllowedModes("bis"), GearMode::Bis));
    uint32_t const no_tiers = ParseAllowedModes("match,best");
    CHECK_FALSE(ModeAllowed(no_tiers, GearMode::T3));
}

TEST_CASE("A tier mode applies only at the level cap and only when the server allows it", "[ClassicOwnedGearPolicy]")
{
    uint32_t const all = ParseAllowedModes("");
    uint32_t const no_t3 = ParseAllowedModes("match,best,t1,t2");
    CHECK(EffectiveMode(GearMode::T3, all, true) == GearMode::T3);
    CHECK(EffectiveMode(GearMode::T3, all, false) == GearMode::Match);     // a level 40 bot has no tier set to wear
    CHECK(EffectiveMode(GearMode::T3, no_t3, true) == GearMode::Match);    // refused by the server admin
    CHECK(EffectiveMode(GearMode::T2, no_t3, true) == GearMode::T2);
    CHECK(EffectiveMode(GearMode::Best, no_t3, false) == GearMode::Best);
    CHECK(EffectiveMode(GearMode::Bis, all, true) == GearMode::Bis);
    CHECK(EffectiveMode(GearMode::Bis, all, false) == GearMode::Match);    // best in slot is a level cap mode
    CHECK(EffectiveMode(GearMode::Bis, ParseAllowedModes("match,best"), true) == GearMode::Match);
    CHECK_FALSE(IsTierMode(GearMode::Bis));
    CHECK(IsCapMode(GearMode::Bis));
    CHECK(TierNumber(GearMode::T1) == 1);
    CHECK(TierNumber(GearMode::Best) == 0);
}

TEST_CASE("Every Classic class with a tier set has one per tier", "[ClassicOwnedGearPolicy]")
{
    for (uint8_t cls : { 1, 2, 3, 4, 5, 7, 8, 9, 11 })
        for (int32_t tier = 1; tier <= 3; ++tier)
            CHECK_FALSE(TierSetName(cls, tier).empty());
    CHECK(TierSetName(9, 3) == "Plagueheart Raiment");   // Karrz's own set
    CHECK(TierSetName(2, 1) == "Lawbringer Armor");
    CHECK(TierSetName(6, 1).empty());
    CHECK(TierSetName(9, 4).empty());
}

TEST_CASE("The gear refresh key also changes with the owner's mode", "[ClassicOwnedGearPolicy]")
{
    CHECK(RefreshKey(60, 0, GearMode::Match) != RefreshKey(60, 0, GearMode::T3));
    CHECK(RefreshKey(60, 0, GearMode::T3) == RefreshKey(60, 0, GearMode::T3));
}
