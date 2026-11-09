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

#include "../../src/modules/PlayerbotV2/Api/ClassicBotSkills.h"

TEST_CASE("A bot's combat skills are five per level", "[ClassicBotSkills]")
{
    CHECK(ClassicBotSkills::Target(60, 0) == 300);
    CHECK(ClassicBotSkills::Target(60, 295) == 300);
    CHECK(ClassicBotSkills::Target(21, 105) == 105);
    CHECK(ClassicBotSkills::Target(1, 5) == 5);
}

TEST_CASE("A skill whose own maximum is higher keeps it", "[ClassicBotSkills]")
{
    CHECK(ClassicBotSkills::Target(30, 300) == 300);
}

TEST_CASE("A raise is needed when the rank or the maximum is below the target", "[ClassicBotSkills]")
{
    // The measured bots: a level 60 warrior at Defense 181 and a paladin at 52, both with the maximum already at 300.
    CHECK(ClassicBotSkills::NeedsRaise(181, 300, 300));
    CHECK(ClassicBotSkills::NeedsRaise(52, 300, 300));
    // The level-up lifted the rank's maximum but not yet the stored maximum.
    CHECK(ClassicBotSkills::NeedsRaise(300, 295, 300));
    CHECK_FALSE(ClassicBotSkills::NeedsRaise(300, 300, 300));
}
