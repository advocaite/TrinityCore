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

#include "../../src/modules/PlayerbotV2/Fleet/CityLayer.h"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

using namespace Playerbot::V2::Fleet;

TEST_CASE("A same-level ask whose ground is 20 y or more below is across a drop", "[ClassicCityLayer]")
{
    CHECK(AcrossDrop(98.6f, 98.6f, 59.5f));    // Stormwind: the city floor over the ground under it
    CHECK(AcrossDrop(98.0f, 97.7f, 75.1f));    // and the middle layer, 22.6 y down
    CHECK_FALSE(AcrossDrop(98.0f, 98.0f, 90.0f));   // a flight of steps
    CHECK_FALSE(AcrossDrop(98.0f, 98.0f, 78.5f));   // 19.5 y: still a slope
}

TEST_CASE("A goal at its own z is not a same-level ask", "[ClassicCityLayer]")
{
    // The hub is 30 y below the bot and the rule says so: the ground agrees with the hint, whatever the bot's z.
    CHECK_FALSE(AcrossDrop(98.0f, 59.5f, 59.5f));
    CHECK_FALSE(AcrossDrop(98.0f, 70.0f, 40.0f));    // 30 y below an explicit goal z, but the goal is not at the bot's level
}

TEST_CASE("Thirty failed path calls in three minutes are a storm", "[ClassicCityLayer]")
{
    NoPathRun r;
    uint32_t t = 1000;
    for (int i = 0; i < 29; ++i, t += 1000)
        NoteNoPath(r, t);
    CHECK_FALSE(NoPathStorm(r, t));
    NoteNoPath(r, t);
    CHECK(NoPathStorm(r, t));
}

TEST_CASE("A window that has run out starts again", "[ClassicCityLayer]")
{
    NoPathRun r;
    uint32_t t = 5000;
    for (int i = 0; i < 40; ++i)
        NoteNoPath(r, t += 500);
    CHECK(NoPathStorm(r, t));
    CHECK_FALSE(NoPathStorm(r, t + kNoPathWindowMs + 1));    // asked later, the old window is not counted
    NoteNoPath(r, t + kNoPathWindowMs + 1);
    CHECK(r.n == 1);
}

TEST_CASE("The ms clock wrapping does not open a false storm", "[ClassicCityLayer]")
{
    NoPathRun r;
    uint32_t t = 0xFFFFFF00u;
    NoteNoPath(r, t);
    NoteNoPath(r, t + 1000u);   // wraps past zero
    CHECK(r.n == 2);
    CHECK_FALSE(NoPathStorm(r, t + 1000u));
}

TEST_CASE("A path segment that falls as far as it runs is a cliff, a ramp or a stair is not", "[ClassicCityLayer]")
{
    CHECK(IsCliffSegment(30.0f, 31.0f));        // Dutchess: 35 y over to a floor 31 y under
    CHECK(IsCliffSegment(10.0f, 38.0f));
    CHECK_FALSE(IsCliffSegment(100.0f, 40.0f)); // a long ramp
    CHECK_FALSE(IsCliffSegment(8.0f, 4.0f));    // stairs
    CHECK_FALSE(IsCliffSegment(5.0f, 14.0f));   // short of the drop that counts
    CHECK_FALSE(IsCliffSegment(30.0f, -31.0f)); // going up is a climb the path would not have chosen
}

TEST_CASE("A path is checked for cliff faces going down and going up", "[ClassicCityLayer]")
{
    struct P { float x, y, z; };
    std::vector<P> ramp = { {0, 0, 100}, {40, 0, 110}, {80, 0, 121} };
    CHECK_FALSE(PathHasCliff(ramp));
    std::vector<P> drop = { {0, 0, 101}, {20, 0, 101}, {30, 0, 70} };
    CHECK(PathHasCliff(drop));
    std::vector<P> climb = { {0, 0, 70}, {10, 0, 101} };
    CHECK(PathHasCliff(climb));
    CHECK_FALSE(PathHasCliff(std::vector<P>{}));
}

// pathing-8 froze the world thread: Fleet/WorldBotSpawner.cpp declared `static bool PathHasCliff(PointsArray const&)` that forwarded to
// Fleet::PathHasCliff, and inside namespace Fleet the call bound to the wrapper itself. A function that calls itself on every path
// compiles to a jump to itself, and the first stranded-bot probe that reached it never returned. No module source may define a function
// under the name of a CityLayer.h function.
TEST_CASE("No module source shadows a CityLayer function with a wrapper of the same name", "[ClassicCityLayer]")
{
    namespace fs = std::filesystem;
    fs::path const root = fs::path(TC_SOURCE_DIR) / "src" / "modules" / "PlayerbotV2";   // TC_SOURCE_DIR comes from CMake (tests/CMakeLists.txt)
    REQUIRE(fs::exists(root));
    std::regex const shadow(R"(^\s*(?:static\s+|inline\s+)*(?:bool|void|float|int)\s+(PathHasCliff|IsCliffSegment|AcrossDrop)\s*\([^;{]*\)\s*(?:const\s*)?\{?\s*$)");
    uint32_t checked = 0;
    for (auto it = fs::recursive_directory_iterator(root); it != fs::recursive_directory_iterator(); ++it)
    {
        if (!it->is_regular_file() || it->path().extension() != ".cpp")
            continue;
        std::ifstream in(it->path(), std::ios::binary);
        std::string line;
        while (std::getline(in, line))
            CHECK_FALSE(std::regex_search(line, shadow));
        ++checked;
    }
    CHECK(checked > 0);
}

TEST_CASE("A same-level step over a pit of liquid is a pit, a slope or a dry drop is not", "[ClassicCityLayer]")
{
    using namespace Playerbot::V2::Fleet;
    CHECK(IntoLiquidPit(501.7f, 501.7f, 483.2f, true));           // Ironforge: the walkway over the Great Forge's lava
    CHECK_FALSE(IntoLiquidPit(501.7f, 501.7f, 483.2f, false));    // the same drop on dry ground is AcrossDrop's call
    CHECK_FALSE(IntoLiquidPit(501.7f, 501.7f, 498.0f, true));     // a puddle down a few steps
    CHECK_FALSE(IntoLiquidPit(501.7f, 483.2f, 483.2f, true));     // the goal itself was asked down there
}
