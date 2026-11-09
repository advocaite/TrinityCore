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

#include "../../src/modules/PlayerbotV2/Fleet/BotParties.h"

using namespace Playerbot::V2::Parties;

namespace
{
Bot At(std::uint64_t guid, float x, float y, std::uint8_t level = 20)
{
    Bot b;
    b.guid = guid;
    b.x = x;
    b.y = y;
    b.level = level;
    return b;
}

Settings All()
{
    Settings st;
    st.pct = 100;
    st.max_size = 2;
    st.life_ms = 60000;
    return st;
}
}

TEST_CASE("Two nearby bots of one faction and level team up, the higher level leads", "[ClassicBotParties]")
{
    Former f;
    std::vector<Bot> bots{ At(1, 0.f, 0.f, 20), At(2, 20.f, 0.f, 21) };
    auto links = f.Step(bots, All(), 1000);
    REQUIRE(links.size() == 2);
    REQUIRE(links[1].leader == 2);
    REQUIRE(links[2].slot == 0);
    REQUIRE(links[1].slot == 1);
    REQUIRE(links[1].mates.size() == 1);
    REQUIRE(links[1].mates[0].guid == 2);
}

TEST_CASE("No party across factions, far apart, or a wide level gap", "[ClassicBotParties]")
{
    Former f;
    Bot horde = At(2, 10.f, 0.f);
    horde.alliance = false;
    REQUIRE(f.Step({ At(1, 0.f, 0.f), horde }, All(), 1000).empty());
    REQUIRE(f.Step({ At(1, 0.f, 0.f), At(2, 200.f, 0.f) }, All(), 1000).empty());
    REQUIRE(f.Step({ At(1, 0.f, 0.f, 20), At(2, 5.f, 0.f, 25) }, All(), 1000).empty());
    Bot busy = At(2, 5.f, 0.f);
    busy.can_join = false;
    REQUIRE(f.Step({ At(1, 0.f, 0.f), busy }, All(), 1000).empty());
}

TEST_CASE("Only about the configured share of bots is partied", "[ClassicBotParties]")
{
    Former f;
    std::vector<Bot> bots;
    for (std::uint64_t i = 1; i <= 20; ++i)
        bots.push_back(At(i, float(i), 0.f));
    Settings st = All();
    st.pct = 30;
    st.max_size = 3;
    auto links = f.Step(bots, st, 1000);
    REQUIRE(links.size() >= 4);
    REQUIRE(links.size() <= 6);
    // A second pass keeps the same parties and adds none.
    auto again = f.Step(bots, st, 2000);
    REQUIRE(again.size() == links.size());
    for (auto const& [g, l] : links)
        REQUIRE(again.at(g).leader == l.leader);
}

TEST_CASE("A party ends when it expires, its leader dies or a member wanders off", "[ClassicBotParties]")
{
    Settings st = All();
    {
        Former f;
        std::vector<Bot> bots{ At(1, 0.f, 0.f), At(2, 10.f, 0.f) };
        REQUIRE(f.Step(bots, st, 1000).size() == 2);
        bots[0].can_join = bots[1].can_join = false;   // so no new party forms on the same pass
        REQUIRE(f.Step(bots, st, 1000 + 2 * st.life_ms).empty());
        REQUIRE(f.dissolved() == 1);
    }
    {
        Former f;
        std::vector<Bot> bots{ At(1, 0.f, 0.f, 21), At(2, 10.f, 0.f, 20) };
        REQUIRE(f.Step(bots, st, 1000).size() == 2);
        bots[0].alive = false;
        REQUIRE(f.Step(bots, st, 2000).empty());
    }
    {
        Former f;
        std::vector<Bot> bots{ At(1, 0.f, 0.f, 21), At(2, 10.f, 0.f, 20) };
        REQUIRE(f.Step(bots, st, 1000).size() == 2);
        bots[1].x = 500.f;
        REQUIRE(f.Step(bots, st, 2000).empty());
    }
}

TEST_CASE("A mate's fight is in the link", "[ClassicBotParties]")
{
    Former f;
    std::vector<Bot> bots{ At(1, 0.f, 0.f, 21), At(2, 10.f, 0.f, 20) };
    f.Step(bots, All(), 1000);
    bots[0].in_combat = true;
    bots[0].target = 4242;
    auto links = f.Step(bots, All(), 2000);
    REQUIRE(links[2].mates[0].in_combat);
    REQUIRE(links[2].mates[0].target == 4242);
}
