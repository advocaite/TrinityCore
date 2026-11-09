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

#include "../../src/modules/PlayerbotV2/Api/JunkDrop.h"

using namespace Playerbot;

TEST_CASE("JunkDrop: the cheapest grey makes room for real loot", "[junkdrop]")
{
    std::vector<JunkCandidate> bags{ {50, true, false}, {7, true, false}, {900, false, false}, {20, true, false} };
    CHECK(PickJunkToDrop(bags, /*incoming_poor*/ false, 0) == 1);
}

TEST_CASE("JunkDrop: only greys, never quest items or better gear", "[junkdrop]")
{
    std::vector<JunkCandidate> none{ {1, false, false}, {2, true, true} };
    CHECK(PickJunkToDrop(none, false, 100) == kNoJunk);
    CHECK(PickJunkToDrop({}, false, 100) == kNoJunk);
}

TEST_CASE("JunkDrop: a grey only displaces a cheaper grey", "[junkdrop]")
{
    std::vector<JunkCandidate> bags{ {30, true, false}, {10, true, false} };
    CHECK(PickJunkToDrop(bags, true, 30) == 1);
    CHECK(PickJunkToDrop(bags, true, 10) == kNoJunk);
    CHECK(PickJunkToDrop(bags, true, 5) == kNoJunk);
}

TEST_CASE("JunkDrop: room for food and drink takes a grey first, then a spare supply", "[junkdrop]")
{
    // deadmines_full on 0.1.23: the priest's bags were full and she never got water.
    std::vector<JunkCandidate> bags{ {900, false, false, false}, {40, false, false, true}, {12, true, false, false}, {5, true, true, false} };
    CHECK(PickRoomForSupply(bags) == 2);   // the grey, not the cheaper grey quest item
    bags[2].poor = false;
    CHECK(PickRoomForSupply(bags) == 1);   // no grey left: the spare potion stack
}

TEST_CASE("JunkDrop: gear and the player's items are never destroyed for supplies", "[junkdrop]")
{
    std::vector<JunkCandidate> bags{ {900, false, false, false}, {3, false, false, false}, {1, true, true, false} };
    CHECK(PickRoomForSupply(bags) == kNoJunk);
}
