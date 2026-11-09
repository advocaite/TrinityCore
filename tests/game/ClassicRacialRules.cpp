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

#include "../../src/modules/PlayerbotV2/Bot/RacialRules.h"

using Playerbot::EscapeArtistWanted;

// Deadmines 2026-10-09: Escape Artist fired on every slow while the gnome stood in its fight.
TEST_CASE("Escape Artist is used whenever the bot is rooted", "[ClassicRacialRules]")
{
    CHECK(EscapeArtistWanted(true, false, false));
    CHECK(EscapeArtistWanted(true, false, true));
    CHECK(EscapeArtistWanted(true, true, false));
}

TEST_CASE("Escape Artist is used for a snare only while the bot is moving", "[ClassicRacialRules]")
{
    CHECK(EscapeArtistWanted(false, true, true));
    CHECK_FALSE(EscapeArtistWanted(false, true, false));
}

TEST_CASE("Escape Artist is not used without an impairment", "[ClassicRacialRules]")
{
    CHECK_FALSE(EscapeArtistWanted(false, false, false));
    CHECK_FALSE(EscapeArtistWanted(false, false, true));
}
