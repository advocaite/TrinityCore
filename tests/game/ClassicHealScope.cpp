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

#include "../../src/modules/PlayerbotV2/Bot/ClassicHealScope.h"

using Playerbot::Combat::ClassicHeal::EfficiencyRulesApply;

// Deadmines 2026-10-09: a level 21 party's priest ran out of mana (the efficiency rules were raid maps only).
TEST_CASE("The healer efficiency rules apply inside a raid map", "[ClassicHealScope]")
{
    CHECK(EfficiencyRulesApply(true, false, false, true));
}

TEST_CASE("The healer efficiency rules apply inside a 5-man dungeon", "[ClassicHealScope]")
{
    CHECK(EfficiencyRulesApply(false, true, false, true));
}

TEST_CASE("The healer efficiency rules stay off in the open world", "[ClassicHealScope]")
{
    CHECK_FALSE(EfficiencyRulesApply(false, false, false, true));
}

TEST_CASE("The healer efficiency rules stay off in a battleground and on a non-vanilla realm", "[ClassicHealScope]")
{
    CHECK_FALSE(EfficiencyRulesApply(false, false, true, true));
    CHECK_FALSE(EfficiencyRulesApply(true, false, true, true));
    CHECK_FALSE(EfficiencyRulesApply(false, true, false, false));
}
