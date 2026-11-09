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

#include "../../src/modules/PlayerbotV2/Fleet/PlayerWorkAreas.h"

using namespace Playerbot::V2::WorkAreas;

namespace
{
Area PlayerAt(float x, float y, std::uint64_t group = 0)
{
    Area a;
    a.player = 42;
    a.group = group;
    a.map = 0;
    a.x = x;
    a.y = y;
    return a;
}
auto const kNoDrops = [](std::uint32_t, std::uint32_t) { return false; };
}

TEST_CASE("A mob next to a real player is his", "[ClassicPlayerWorkAreas]")
{
    std::vector<Area> areas{ PlayerAt(100.f, 100.f) };
    Settings cfg;
    REQUIRE(ClaimOf(areas, cfg, 0, 120.f, 100.f, 299, 0, kNoDrops) == Claim::Camp);
    REQUIRE(ClaimOf(areas, cfg, 0, 100.f + cfg.camp_radius + 1.f, 100.f, 299, 0, kNoDrops) == Claim::None);
    // Another map: not his.
    REQUIRE(ClaimOf(areas, cfg, 1, 120.f, 100.f, 299, 0, kNoDrops) == Claim::None);
}

TEST_CASE("A player's quest mob is his while he is near", "[ClassicPlayerWorkAreas]")
{
    Area a = PlayerAt(0.f, 0.f);
    a.kill_entries = { 1501 };
    a.item_entries = { 750 };
    std::vector<Area> areas{ a };
    Settings cfg;
    REQUIRE(ClaimOf(areas, cfg, 0, 80.f, 0.f, 1501, 0, kNoDrops) == Claim::Quest);
    REQUIRE(ClaimOf(areas, cfg, 0, 80.f, 0.f, 1502, 0, kNoDrops) == Claim::None);
    REQUIRE(ClaimOf(areas, cfg, 0, cfg.quest_radius + 5.f, 0.f, 1501, 0, kNoDrops) == Claim::None);
    auto drops = [](std::uint32_t item, std::uint32_t entry) { return item == 750 && entry == 1777; };
    REQUIRE(ClaimOf(areas, cfg, 0, 80.f, 0.f, 1777, 0, drops) == Claim::Quest);
    REQUIRE(ClaimOf(areas, cfg, 0, 80.f, 0.f, 1778, 0, drops) == Claim::None);
}

TEST_CASE("A bot in the player's own group shares his mobs; switched off claims nothing", "[ClassicPlayerWorkAreas]")
{
    std::vector<Area> areas{ PlayerAt(0.f, 0.f, 77) };
    Settings cfg;
    REQUIRE(ClaimOf(areas, cfg, 0, 5.f, 0.f, 1, 77, kNoDrops) == Claim::None);
    REQUIRE(ClaimOf(areas, cfg, 0, 5.f, 0.f, 1, 78, kNoDrops) == Claim::Camp);
    cfg.enabled = false;
    REQUIRE(ClaimOf(areas, cfg, 0, 5.f, 0.f, 1, 0, kNoDrops) == Claim::None);
}

TEST_CASE("A camp claim wins over a quest claim from a farther player", "[ClassicPlayerWorkAreas]")
{
    Area far = PlayerAt(100.f, 0.f);
    far.kill_entries = { 5 };
    std::vector<Area> areas{ far, PlayerAt(0.f, 0.f) };
    REQUIRE(ClaimOf(areas, Settings{}, 0, 10.f, 0.f, 5, 0, kNoDrops) == Claim::Camp);
}

TEST_CASE("A grind spot a player stands in is taken", "[ClassicPlayerWorkAreas]")
{
    std::vector<Area> areas{ PlayerAt(0.f, 0.f, 9) };
    REQUIRE(PlayerNear(areas, 0, 30.f, 0.f, 70.f));
    REQUIRE_FALSE(PlayerNear(areas, 0, 80.f, 0.f, 70.f));
    REQUIRE_FALSE(PlayerNear(areas, 0, 30.f, 0.f, 70.f, 9));
    Board::Get().Publish(areas);
    REQUIRE(Board::Get().Current()->size() == 1);
}
