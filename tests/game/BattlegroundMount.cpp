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

#include "Battleground.h"

// WMO group flags read live in Warsong Gulch on 2026-10-09 ([bg_mount] lines): the interior tunnel groups must refuse a mount, the
// exterior group of the Horde flag room (group 19307, 953,1456,342) must not.
TEST_CASE("Battleground mount rule: WMO group indoor test", "[battleground][mount]")
{
    // tunnel / flag room interior groups 19305, 19342, 19344, 19302
    CHECK(Battleground::IsIndoorWmoGroup(0x2805));
    // exterior group 19307
    CHECK_FALSE(Battleground::IsIndoorWmoGroup(0x80000809));
    // no wmo flags at all (open ground reads as no WMO): not indoors
    CHECK_FALSE(Battleground::IsIndoorWmoGroup(0));
    // a group flagged both interior and exterior is outdoors, as in the client
    CHECK_FALSE(Battleground::IsIndoorWmoGroup(0x2008));
}

// Playtest Arathi Basin 2026-10-11: "Can only use outside" at the Alliance start (Trollbane Hall) during the preparation phase.
TEST_CASE("Battleground mount rule: only the Warsong Gulch start room refuses a mount during preparation", "[battleground][mount]")
{
    CHECK(Battleground::IsEnclosedStartArea(489));      // Warsong Gulch
    CHECK_FALSE(Battleground::IsEnclosedStartArea(529)); // Arathi Basin
    CHECK_FALSE(Battleground::IsEnclosedStartArea(30));  // Alterac Valley
}

// ab-no-horde 2026-10-11: 10 Alliance and 4 Horde inside a running 15 per team Arathi Basin, invited counters 1 and 0 (players who arrived
// without a queue invitation were never counted), and only one Horde bot at a time was let in.
TEST_CASE("Battleground balance gate: players inside count as invited", "[battleground][balance]")
{
    // Horde: other side 10 inside, this side 4 inside, stale invited counters 0 (this) and 1 (other)
    CHECK(Battleground::BalancedFreeSlots(0, 1, 4, 10, 15, 5) == 6);
    // Alliance is ahead: no more Alliance until the Horde catch up
    CHECK(Battleground::BalancedFreeSlots(1, 0, 10, 4, 15, 5) == 0);
    // equal sides: one at a time
    CHECK(Battleground::BalancedFreeSlots(8, 8, 8, 8, 15, 5) == 1);
    // never above the team size
    CHECK(Battleground::BalancedFreeSlots(13, 15, 13, 15, 15, 5) == 2);
    CHECK(Battleground::BalancedFreeSlots(15, 15, 15, 15, 15, 5) == 0);
}

// Playtest 2026-10-11: five Warsong Gulch, five Arathi Basin and five Alterac Valley instances up at once, none stealing bots from another.
TEST_CASE("Battleground fill: two live instances of one battleground are filled independently", "[battleground][balance]")
{
    // instance 5 is 15 v 15 (full): it asks for nothing
    CHECK(Battleground::BalancedFreeSlots(15, 15, 15, 15, 15, 5) == 0);
    // instance 6 is 10 v 4 with stale invited counters: Horde may take 6, Alliance none, whatever instance 5 holds
    CHECK(Battleground::BalancedFreeSlots(0, 1, 4, 10, 15, 5) == 6);
    CHECK(Battleground::BalancedFreeSlots(1, 0, 10, 4, 15, 5) == 0);

    // the queue is drawn by the instance with a human first, then the emptiest
    CHECK(Battleground::FillsBefore(1, 25, 0, 4));             // a human beats a fuller-or-emptier bot-only match
    CHECK_FALSE(Battleground::FillsBefore(0, 4, 1, 25));
    CHECK(Battleground::FillsBefore(3, 28, 1, 12));            // more real players first
    CHECK(Battleground::FillsBefore(0, 14, false, 30));    // the 10 v 4 instance before the 15 v 15 one
    CHECK_FALSE(Battleground::FillsBefore(0, 30, false, 14));
    CHECK_FALSE(Battleground::FillsBefore(0, 14, false, 14));   // equal: the order is kept (stable sort)
}

// Playtest 2026-10-11: as few instances as possible, as many real players in each. A real player who queues is put into a running instance
// (bot-only or mixed) even when a new one could be opened; a bot of the player's team is swapped out when that team is full.
TEST_CASE("Battleground placement: a real player goes to the instance with the most real players", "[battleground][placement]")
{
    // instance A: 3 real players, 8 bots on the player's team; instance B: bot-only, 15 bots on the player's team
    CHECK(Battleground::PlacementBetter(3, 8, 0, 15));
    CHECK_FALSE(Battleground::PlacementBetter(0, 15, 3, 8));
    // equal real players: the one with more bots on the team (more to swap out)
    CHECK(Battleground::PlacementBetter(1, 15, 1, 9));
    CHECK_FALSE(Battleground::PlacementBetter(1, 9, 1, 15));
    // a bot-only instance is still a place: two bot-only instances, the fuller side wins
    CHECK(Battleground::PlacementBetter(0, 15, 0, 6));
    // the team is full (0 free slots): one bot of that team has to leave; with free slots none does
    CHECK(Battleground::BalancedFreeSlots(15, 15, 15, 15, 15, 5) == 0);
    CHECK(Battleground::BalancedFreeSlots(14, 15, 14, 15, 15, 5) == 1);
}

// Playtest 2026-10-11: any level gets a full battleground, a level 12 twink as well as a level 60. A bot created on demand lands inside the bracket.
TEST_CASE("Battleground fill: a bot created for a bracket has a level inside it", "[battleground][bracket]")
{
    struct Bracket { uint32 lo, hi; };
    for (Bracket b : { Bracket{ 10, 19 }, Bracket{ 20, 29 }, Bracket{ 30, 39 }, Bracket{ 40, 49 }, Bracket{ 50, 59 }, Bracket{ 60, 60 } })
        for (uint32 roll = 0; roll < 1000; ++roll)
        {
            uint32 const level = Battleground::BracketLevelFor(b.lo, b.hi, 60, 1, roll);
            CHECK(level >= b.lo);
            CHECK(level <= b.hi);
        }
    // every level of the bracket is reachable
    bool seen[10] = { };
    for (uint32 roll = 0; roll < 10; ++roll) seen[Battleground::BracketLevelFor(10, 19, 60, 1, roll) - 10] = true;
    for (bool s : seen) CHECK(s);
    // a bracket above the realm cap is capped; no bracket range: the fallback
    CHECK(Battleground::BracketLevelFor(60, 69, 60, 1, 7) == 60);
    CHECK(Battleground::BracketLevelFor(0, 0, 60, 33, 7) == 33);
}

// AB 2026-10-11: 7 v 7 of 15 grew by one bot per side per minute because the balance gate allows one at a time between level sides.
TEST_CASE("Battleground fill: a top-up asks for the whole shortfall of each side", "[battleground][balance]")
{
    CHECK(Battleground::BalancedFreeSlots(7, 7, 7, 7, 15, 5) == 1);   // the gate itself still lets one in at a time
    CHECK(Battleground::TopUpRequest(7, 15) == 8);                    // but one cycle asks for 8 + 8
    CHECK(Battleground::TopUpRequest(5, 15) == 10);
    CHECK(Battleground::TopUpRequest(15, 15) == 0);
    CHECK(Battleground::TopUpRequest(16, 15) == 0);                   // never an overshoot
}

// WSG 2026-10-21: ten instances from one queue, because each prep tick asked again for the places that bots still in setup would take.
TEST_CASE("Battleground fill: a top-up leaves out the bots whose setup is still running", "[battleground][balance]")
{
    CHECK(Battleground::TopUpAfterInFlight(0, 10, 0) == 10);
    CHECK(Battleground::TopUpAfterInFlight(0, 10, 10) == 0);    // the first batch already covers the side: no second batch
    CHECK(Battleground::TopUpAfterInFlight(3, 10, 4) == 3);     // 7 short, 4 on the way
    CHECK(Battleground::TopUpAfterInFlight(3, 10, 40) == 0);    // never negative
    CHECK(Battleground::TopUpAfterInFlight(10, 10, 0) == 0);
}

// A real player queues into a full match whose bots are all in combat at that moment: wait a few seconds for one to be free, do not open a new match.
TEST_CASE("Battleground fill: a player waits for a busy bot instead of opening a second match", "[battleground][balance]")
{
    CHECK(Battleground::WaitForBotToFree(1, 0, 10));     // 10 bots on the side, none free right now
    CHECK(Battleground::WaitForBotToFree(2, 1, 8));
    CHECK_FALSE(Battleground::WaitForBotToFree(1, 1, 10));  // a bot can leave now
    CHECK_FALSE(Battleground::WaitForBotToFree(1, 0, 0));   // the side is all real players: a new match is right
    CHECK_FALSE(Battleground::WaitForBotToFree(3, 0, 2));   // fewer bots than places needed
}

// AB 2026-10-11: Alliance 14 of 15 inside, Horde 6: the gate gives Alliance nothing, but a real Alliance player may take the free place.
TEST_CASE("Battleground fill: a waiting real player ignores the balance gate for a free place", "[battleground][balance]")
{
    CHECK(Battleground::BalancedFreeSlots(14, 6, 14, 6, 15, 5) == 0);
    CHECK(Battleground::PhysicalRoom(14, 15) == 1);
    CHECK(Battleground::PhysicalRoom(15, 15) == 0);
    CHECK(Battleground::PhysicalRoom(16, 15) == 0);
}

// WSG 2026-10-11: 12 Alliance inside a 10-a-side match. In the preparation phase with no team balance the free places were counted from the
// invited counter alone (4 invited = 6 free) although 10 were already inside.
TEST_CASE("Battleground fill: places inside count against the team size in the preparation phase", "[battleground][balance]")
{
    CHECK(Battleground::PhysicalRoom(std::max<uint32>(4, 12), 10) == 0);   // 4 invited, 12 inside: none
    CHECK(Battleground::PhysicalRoom(std::max<uint32>(4, 10), 10) == 0);   // 10 inside: none, a real player makes room by evicting
    CHECK(Battleground::PhysicalRoom(std::max<uint32>(4, 7), 10) == 3);
    // the places to make for one waiting player
    CHECK(Battleground::PlacesToMake(12, 10, 1) == 3);   // 12 inside a 10-a-side match: 2 over plus the player's place
    CHECK(Battleground::PlacesToMake(10, 10, 1) == 1);
    CHECK(Battleground::PlacesToMake(9, 10, 1) == 0);
    CHECK(Battleground::PlacesToMake(9, 10, 3) == 2);
}

// AV 2026-10-11: the Alliance invited counter wrapped to 4294967295 / 4294967289 when players who never counted as invited left; the team
// then had no free places and was never refilled.
TEST_CASE("Battleground fill: the invited counter never wraps below zero", "[battleground][balance]")
{
    CHECK(Battleground::AfterLeave(5) == 4);
    CHECK(Battleground::AfterLeave(1) == 0);
    CHECK(Battleground::AfterLeave(0) == 0);   // was 4294967295
}
