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

#include "../../src/modules/PlayerbotV2/Group/TrackedBuffMatch.h"

#include <array>
#include <string_view>

using namespace Playerbot;

namespace
{
constexpr std::uint32_t MOTW = 1126, GOTW = 21849, MOTW_R6 = 9885, MIGHT = 19740, MIGHT_R7 = 27140, GREATER_MIGHT = 25782;

std::string_view NameOf(std::uint32_t id)
{
    switch (id)
    {
        case MOTW: case MOTW_R6: return "Mark of the Wild";
        case GOTW: return "Gift of the Wild";
        case MIGHT: case MIGHT_R7: return "Blessing of Might";
        case GREATER_MIGHT: return "Greater Blessing of Might";
        case 12345: return "Mark of the Wild";   // a rank no chain knows
        default: return {};
    }
}

constexpr std::array<TrackedBuffEntry, 4> kTable = { {
    { MOTW, MOTW }, { GOTW, MOTW }, { MIGHT, MIGHT }, { GREATER_MIGHT, MIGHT },
} };

// The chain / family test of the spell data: rank 6 of Mark of the Wild is in a chain, rank 7 of Might is in none.
bool SameByChain(std::uint32_t a, std::uint32_t b) { return (a == MOTW_R6 && b == MOTW) || (a == MOTW && b == MOTW_R6); }
}

// MC Garr 2026-10-10: the druids and paladins recast their buffs every 30 s because the aura of the rank they cast never counted.
TEST_CASE("A rank the spell data does not chain is still the tracked buff, by name", "[ClassicTrackedBuff]")
{
    auto const table = std::span<TrackedBuffEntry const>(kTable);

    TrackedMatch const chained = MatchTrackedBuff(MOTW_R6, table, SameByChain, NameOf);
    CHECK(chained.recorded_as == MOTW);
    CHECK(chained.how == TrackedHow::ById);

    TrackedMatch const loose = MatchTrackedBuff(MIGHT_R7, table, SameByChain, NameOf);
    CHECK(loose.recorded_as == MIGHT);
    CHECK(loose.how == TrackedHow::ByName);

    TrackedMatch const loose_motw = MatchTrackedBuff(12345, table, SameByChain, NameOf);
    CHECK(loose_motw.recorded_as == MOTW);
    CHECK(loose_motw.how == TrackedHow::ByName);
}

TEST_CASE("The group version and the Greater Blessing record under the single buff's id", "[ClassicTrackedBuff]")
{
    auto const table = std::span<TrackedBuffEntry const>(kTable);
    CHECK(MatchTrackedBuff(GOTW, table, SameByChain, NameOf).recorded_as == MOTW);
    CHECK(MatchTrackedBuff(GREATER_MIGHT, table, SameByChain, NameOf).recorded_as == MIGHT);
}

TEST_CASE("An aura of another name, or with no name, is not a tracked buff", "[ClassicTrackedBuff]")
{
    auto const table = std::span<TrackedBuffEntry const>(kTable);
    CHECK(MatchTrackedBuff(99999, table, SameByChain, NameOf).how == TrackedHow::None);
    CHECK(MatchTrackedBuff(99999, table, SameByChain, NameOf).recorded_as == 0u);
    // A table entry whose spell has no name must not match every nameless aura.
    constexpr std::array<TrackedBuffEntry, 1> unnamed = { { { 777, 777 } } };
    CHECK(MatchTrackedBuff(888, std::span<TrackedBuffEntry const>(unnamed), SameByChain, NameOf).how == TrackedHow::None);
}
