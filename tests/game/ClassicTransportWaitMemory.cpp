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

#include "../../src/modules/PlayerbotV2/Travel/TransportWaitMemory.h"

using Playerbot::V2::Travel::TransportWaitMemory;

TEST_CASE("TransportWaitMemory: a bot gives a ride up only after waiting the full time", "[transportwait]")
{
    CHECK_FALSE(TransportWaitMemory::ShouldGiveUp(0, 1000000));                                  // not waiting
    CHECK_FALSE(TransportWaitMemory::ShouldGiveUp(1000, 1000 + TransportWaitMemory::kGiveUpMs - 1));
    CHECK(TransportWaitMemory::ShouldGiveUp(1000, 1000 + TransportWaitMemory::kGiveUpMs));
}

TEST_CASE("TransportWaitMemory: two give-ups make a ride unusable until the time is up, boarding clears it", "[transportwait]")
{
    TransportWaitMemory m;
    CHECK_FALSE(m.Unusable(176084, 1000));
    m.NoteGaveUp(176084, 1000);
    CHECK_FALSE(m.Unusable(176084, 1100));                                       // one bot may have missed the ride
    m.NoteGaveUp(176084, 2000);
    CHECK(m.Unusable(176084, 3000));
    CHECK_FALSE(m.Unusable(176085, 3000));                                       // another ride is not affected
    CHECK_FALSE(m.Unusable(176084, 2000 + TransportWaitMemory::kTtlMs));
    m.NoteGaveUp(176084, 5000);
    m.NoteGaveUp(176084, 6000);
    m.NoteBoarded(176084);
    CHECK_FALSE(m.Unusable(176084, 7000));
    CHECK_FALSE(m.Unusable(0, 7000));
}
