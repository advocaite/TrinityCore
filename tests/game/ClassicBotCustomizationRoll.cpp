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

#include "../../src/modules/PlayerbotV2/Fleet/BotCustomizationRoll.h"

using namespace Playerbot::V2;

TEST_CASE("Bot customization roll honours dependent choices", "[botcustomization]")
{
    // Option 1 = hair style (choices 10, 11); option 2 = hair colour whose choices 20 and 21 exist only for
    // style 10 and style 11. The colour option comes BEFORE the style in the list, as option ids need not be ordered.
    std::vector<CustomizationRollOption> options(2);
    options[0].id = 2;
    options[0].choices = { { 20, { { 10 } } }, { 21, { { 11 } } } };
    options[1].id = 1;
    options[1].choices = { { 10, {} }, { 11, {} } };

    std::mt19937 rng(1234);
    for (int i = 0; i < 500; ++i)
    {
        auto const rolled = RollCustomizationChoices(options, rng);
        REQUIRE(rolled.size() == 2);
        CHECK(rolled[0].first == 1);
        CHECK(rolled[1].first == 2);
        CHECK(rolled[1].second == rolled[0].second + 10);   // 10 -> 20, 11 -> 21
    }
}

TEST_CASE("Bot customization roll leaves out an option that can never be offered", "[botcustomization]")
{
    std::vector<CustomizationRollOption> options(2);
    options[0].id = 1;
    options[0].choices = { { 10, {} } };
    options[1].id = 2;
    options[1].choices = { { 20, { { 99 } } } };   // needs a choice nobody can select

    std::mt19937 rng(1);
    auto const rolled = RollCustomizationChoices(options, rng);
    REQUIRE(rolled.size() == 1);
    CHECK(rolled[0] == std::pair<uint32, uint32>{ 1, 10 });
}
