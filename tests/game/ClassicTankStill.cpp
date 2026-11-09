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

#include "../../src/modules/PlayerbotV2/Bot/Dungeon/TankStill.h"

using namespace Playerbot::TankStill;

TEST_CASE("TankStill: a line after 30 s standing, then every 30 s", "[playerbot][dungeon][tank_still]")
{
    Watch w;
    REQUIRE(Due(w, true, 36, 10.f, 10.f, 0.f, 1000) == 0);    // starts the watch
    REQUIRE(Due(w, true, 36, 10.5f, 10.f, 0.f, 20000) == 0);  // a small shuffle is still standing
    REQUIRE(Due(w, true, 36, 10.f, 10.f, 0.f, 31000) == 30000);
    REQUIRE(Due(w, true, 36, 10.f, 10.f, 0.f, 45000) == 0);
    REQUIRE(Due(w, true, 36, 10.f, 10.f, 0.f, 61000) == 60000);
}

TEST_CASE("TankStill: moving, a new map or not watching starts over", "[playerbot][dungeon][tank_still]")
{
    Watch w;
    Due(w, true, 36, 0.f, 0.f, 0.f, 1000);
    REQUIRE(Due(w, true, 36, 3.f, 0.f, 0.f, 25000) == 0);      // walked 3 y: stands from 25 s now
    REQUIRE(Due(w, true, 36, 3.f, 0.f, 0.f, 40000) == 0);
    REQUIRE(Due(w, true, 36, 3.f, 0.f, 0.f, 55000) == 30000);
    REQUIRE(Due(w, false, 36, 3.f, 0.f, 0.f, 60000) == 0);     // combat: watch off
    REQUIRE(Due(w, true, 36, 3.f, 0.f, 0.f, 61000) == 0);
    REQUIRE(Due(w, true, 36, 3.f, 0.f, 0.f, 90000) == 0);
    REQUIRE(Due(w, true, 36, 3.f, 0.f, 0.f, 91000) == 30000);
    REQUIRE(Due(w, true, 48, 3.f, 0.f, 0.f, 130000) == 0);     // same spot, other map
}

TEST_CASE("TankStill: reasons name everyone the tank waits for", "[playerbot][dungeon][tank_still]")
{
    Limits lim;
    Self self;
    REQUIRE(Reasons(self, {}, lim).empty());

    self.hp_pct = 55;
    self.mana_pct = 40;
    std::vector<Member> m(5);
    m[0].name = "Anna"; m[0].alive = false; m[0].ghost = true;
    m[1].name = "Bert"; m[1].other_map = true;
    m[2].name = "Cara"; m[2].healer = true; m[2].yards = 45.f; m[2].mana_pct = 70;
    m[3].name = "Dirk"; m[3].yards = 45.f; m[3].hp_pct = 50; m[3].mana_pct = 65;
    m[4].name = "Emil"; m[4].online = false; m[4].alive = false;
    REQUIRE(Reasons(self, m, lim) ==
            "own health 55%, own mana 40%, Anna dead (running back), Bert outside the instance, "
            "Cara (healer) far 45 y, Cara (healer) mana 70%, Dirk health 50%");
}

TEST_CASE("TankStill: no mana is not low mana", "[playerbot][dungeon][tank_still]")
{
    Limits lim;
    Self self;  // a warrior: mana -1
    std::vector<Member> m(1);
    m[0].name = "Fynn";  // a rogue: mana -1
    REQUIRE(Reasons(self, m, lim).empty());
    m[0].alive = false;
    REQUIRE(Reasons(self, m, lim) == "Fynn dead");
}
