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

#include "../../src/modules/PlayerbotV2/Fleet/SkyborneIsle.h"

using namespace Playerbot::V2::Fleet::Skyborne;

namespace
{
constexpr uint32_t WARRIOR = 1, PALADIN = 2, HUNTER = 3, ROGUE = 4, PRIEST = 5, SHAMAN = 7, MAGE = 8, WARLOCK = 9, DRUID = 11;
constexpr uint32_t HUMAN = 1, ORC = 2;
constexpr uint32_t ELWYNN_MAP = 0;
}

TEST_CASE("Skyborne races and their teams", "[skyborne]")
{
    CHECK(IsSkyborneRace(95));
    CHECK(IsSkyborneRace(96));
    CHECK_FALSE(IsSkyborneRace(HUMAN));
    CHECK_FALSE(IsSkyborneRace(ORC));
    CHECK(IsSkyborneAlliance(95));
    CHECK_FALSE(IsSkyborneAlliance(96));
    CHECK(RaceForTeam(true) == 95);
    CHECK(RaceForTeam(false) == 96);
}

TEST_CASE("Skyborne class combinations match the client's CharBaseInfo", "[skyborne]")
{
    // High Order Skyborne: Warrior, Hunter, Rogue, Mage, Druid
    for (uint32_t cls : { WARRIOR, HUNTER, ROGUE, MAGE, DRUID })
        CHECK(IsLegalClass(95, cls));
    for (uint32_t cls : { PALADIN, PRIEST, SHAMAN, WARLOCK })
        CHECK_FALSE(IsLegalClass(95, cls));
    // Windshaper Skyborne: Warrior, Hunter, Rogue, Shaman, Druid
    for (uint32_t cls : { WARRIOR, HUNTER, ROGUE, SHAMAN, DRUID })
        CHECK(IsLegalClass(96, cls));
    for (uint32_t cls : { PALADIN, PRIEST, MAGE, WARLOCK })
        CHECK_FALSE(IsLegalClass(96, cls));
    // not a Skyborne race at all
    CHECK_FALSE(IsLegalClass(HUMAN, WARRIOR));
}

TEST_CASE("Skyborne share of the ambient roll stays small", "[skyborne]")
{
    // vanilla race weights in BotComposition: Alliance 18+12+6+4, Horde 10+10+8+6
    constexpr double alliance = double(kRaceWeight) / double(18 + 12 + 6 + 4 + kRaceWeight);
    constexpr double horde    = double(kRaceWeight) / double(10 + 10 + 8 + 6 + kRaceWeight);
    CHECK(alliance < 0.12);
    CHECK(horde < 0.12);
    CHECK(alliance > 0.05);
    CHECK(horde > 0.05);
}

TEST_CASE("Zephras anchors use the island's own level window", "[skyborne]")
{
    Window const w = IsleWindow();
    CHECK(w.lo == 1);
    CHECK(w.hi == 9);
    CHECK(w.hi < kGraduationLevel);   // the window never reaches the level at which a bot leaves
    CHECK(IsOnIsle(2991));
    CHECK_FALSE(IsOnIsle(0));
    CHECK_FALSE(IsOnIsle(1));
}

TEST_CASE("A Skyborne bot leaves the island at level 10 and only then", "[skyborne]")
{
    // map, level, owned, grouped, in_combat, in_transit
    CHECK_FALSE(ShouldGraduate(kIsleMap, 9, false, false, false, false));
    CHECK(ShouldGraduate(kIsleMap, 10, false, false, false, false));
    CHECK(ShouldGraduate(kIsleMap, 14, false, false, false, false));
    // not on the island: nothing to do
    CHECK_FALSE(ShouldGraduate(ELWYNN_MAP, 20, false, false, false, false));
    // a player's companion or alt, a grouped bot, a bot in a fight or on its way somewhere is left alone
    CHECK_FALSE(ShouldGraduate(kIsleMap, 10, true, false, false, false));
    CHECK_FALSE(ShouldGraduate(kIsleMap, 10, false, true, false, false));
    CHECK_FALSE(ShouldGraduate(kIsleMap, 10, false, false, true, false));
    CHECK_FALSE(ShouldGraduate(kIsleMap, 10, false, false, false, true));
}

TEST_CASE("Sleeping Skyborne bots wake only where they belong", "[skyborne]")
{
    // around a Zephras anchor: only Skyborne of the island's levels
    CHECK(PoolRowFitsAnchor(true, 95, 1));
    CHECK(PoolRowFitsAnchor(true, 96, 9));
    CHECK_FALSE(PoolRowFitsAnchor(true, 96, 10));
    CHECK_FALSE(PoolRowFitsAnchor(true, HUMAN, 5));
    CHECK_FALSE(PoolRowFitsAnchor(true, ORC, 30));
    // anywhere else: an ordinary bot, except a low-level Skyborne who sleeps on the island
    CHECK(PoolRowFitsAnchor(false, HUMAN, 30));
    CHECK(PoolRowFitsAnchor(false, 95, 10));
    CHECK(PoolRowFitsAnchor(false, 96, 44));
    CHECK_FALSE(PoolRowFitsAnchor(false, 95, 4));
    CHECK_FALSE(PoolRowFitsAnchor(false, 96, 9));
}
