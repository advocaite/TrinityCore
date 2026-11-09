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

#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

// ScriptMgr asserts at startup when two scripts of one kind share a name ("Script 'x' already assigned with the same script name"),
// which took the go-live install down twice (go_classic_wsg_flag, go_classic_ab_banner registered by advocaite's script and ours).
// This reads the script sources and checks that every game object AI / battleground map script / single-name spell script name is
// registered once. It needs the source tree at the CMake source folder (TC_SOURCE_DIR) and fails when it is not there.
TEST_CASE("Script registration: every game object AI, battleground map script and spell script name is registered once", "[scripts][registration]")
{
    namespace fs = std::filesystem;
    fs::path const scripts = fs::path(TC_SOURCE_DIR) / "src" / "server" / "scripts";   // TC_SOURCE_DIR comes from CMake (tests/CMakeLists.txt); __FILE__ is trimmed in release builds
    std::error_code ec;
    INFO("script sources expected at " << scripts.string());
    REQUIRE(fs::is_directory(scripts, ec));   // a missing source folder is a failure, not a skip: the check would silently never run

    std::regex const reg(R"(^\s*(RegisterGameObjectAI|RegisterBattlegroundMapScript|RegisterSpellScript|RegisterAuraScript)\(\s*(\w+))");
    std::map<std::string, std::vector<std::string>> sites;   // "<macro family>:<name>" -> files
    for (fs::recursive_directory_iterator it(scripts, ec), end; !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file() || it->path().extension() != ".cpp")
            continue;
        std::ifstream in(it->path(), std::ios::binary);
        std::string line;
        while (std::getline(in, line))
        {
            std::smatch m;
            if (!std::regex_search(line, m, reg))
                continue;
            std::string family = m[1] == "RegisterAuraScript" ? "RegisterSpellScript" : m[1].str();
            sites[family + ":" + m[2].str()].push_back(it->path().filename().string());
        }
    }

    CHECK_FALSE(sites.empty());
    for (auto const& [key, files] : sites)
    {
        INFO(key << " is registered " << files.size() << " times, first in " << files.front());
        CHECK(files.size() == 1);
    }

    // the classic AB banner and WSG flag objects specifically
    CHECK(sites["RegisterGameObjectAI:go_classic_ab_banner"].size() == 1);
    CHECK(sites["RegisterGameObjectAI:go_classic_wsg_flag"].size() == 1);
}
