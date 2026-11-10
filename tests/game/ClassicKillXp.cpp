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
#include "Formulas.h"
#include "World.h"

// Classic 1.60 kill XP (Trinity::XP::BaseGain): mobs of the player's level or higher give the player's base (5 * level + 45) plus 5%
// per level above (at most 4); lower mobs give (ZD + mob - player) / ZD of the player's base, or with Classic.ForeverKillXp of the
// mob's own base; gray mobs give nothing. The cases are worked values of the formulas (the sniffed kills are not published).
namespace
{
    void SetKillXp(bool forever)
    {
        sWorld->setIntConfig(CONFIG_MIN_CREATURE_SCALED_XP_RATIO, 0);
        sWorld->setBoolConfig(CONFIG_CLASSIC_FOREVER_KILL_XP, forever);
    }
}

TEST_CASE("Classic kill XP of a mob of the player's level or higher", "[ClassicKillXp]")
{
    for (bool forever : { false, true })
    {
        INFO("Classic.ForeverKillXp " << forever);
        SetKillXp(forever);

        CHECK(Trinity::XP::BaseGain(1, 1) == 50);
        CHECK(Trinity::XP::BaseGain(30, 30) == 195);
        CHECK(Trinity::XP::BaseGain(60, 60) == 345);
        CHECK(Trinity::XP::BaseGain(30, 34) == 234);    // +20%
        CHECK(Trinity::XP::BaseGain(30, 40) == 234);    // capped at 4 levels
    }
    SetKillXp(false);
}

TEST_CASE("Classic kill XP of a lower mob, vanilla: the player's base", "[ClassicKillXp]")
{
    SetKillXp(false);

    CHECK(Trinity::XP::BaseGain(10, 8) == 68);      // 95 * 5 / 7
    CHECK(Trinity::XP::BaseGain(20, 15) == 79);     // 145 * 6 / 11
    CHECK(Trinity::XP::BaseGain(60, 55) == 244);    // 345 * 12 / 17
    CHECK(Trinity::XP::BaseGain(60, 59) == 325);    // 345 * 16 / 17
}

TEST_CASE("Classic kill XP of a lower mob, Classic.ForeverKillXp: the mob's own base", "[ClassicKillXp]")
{
    SetKillXp(true);

    CHECK(Trinity::XP::BaseGain(10, 8) == 61);      // 85 * 5 / 7
    CHECK(Trinity::XP::BaseGain(20, 15) == 65);     // 120 * 6 / 11
    CHECK(Trinity::XP::BaseGain(60, 55) == 226);    // 320 * 12 / 17
    CHECK(Trinity::XP::BaseGain(60, 59) == 320);    // 340 * 16 / 17

    SetKillXp(false);
}

TEST_CASE("Classic kill XP of a gray mob is zero", "[ClassicKillXp]")
{
    for (bool forever : { false, true })
    {
        INFO("Classic.ForeverKillXp " << forever);
        SetKillXp(forever);

        CHECK(Trinity::XP::GetGrayLevel(60) == 47);
        CHECK(Trinity::XP::BaseGain(60, 47) == 0);
        CHECK(Trinity::XP::BaseGain(60, 48) > 0);
        CHECK(Trinity::XP::GetGrayLevel(20) == 13);
        CHECK(Trinity::XP::BaseGain(20, 13) == 0);
    }
    SetKillXp(false);
}
