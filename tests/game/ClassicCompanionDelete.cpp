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

#include "../../src/modules/PlayerbotV2/Fleet/CompanionDelete.h"

using namespace Playerbot::V2::Fleet;

TEST_CASE("CompanionDelete: a bot made for the player (pool account) may be deleted", "[companiondelete]")
{
    CHECK(MayDeleteCompanion(true));
}

TEST_CASE("CompanionDelete: the player's own character (a summoned alt) is never deleted", "[companiondelete]")
{
    CHECK_FALSE(MayDeleteCompanion(false));
}

TEST_CASE("CompanionDelete: the refusal names the character and points to dismiss", "[companiondelete]")
{
    std::string const msg = CompanionDeleteRefusal("Myalt");
    CHECK(msg.find("Myalt is one of your own characters") == 0);
    CHECK(msg.find("`.bot dismiss Myalt`") != std::string::npos);
}
