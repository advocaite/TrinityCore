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

#include "../../src/modules/PlayerbotV2/Session/HumanLogin.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace Playerbot::V2;

namespace
{
std::string ReadSourceFile(char const* relative)
{
    std::ifstream in(std::filesystem::path(TC_SOURCE_DIR) / relative, std::ios::binary);   // TC_SOURCE_DIR comes from CMake (tests/CMakeLists.txt)
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}
}

TEST_CASE("HumanLogin: an alt once summoned as a bot is released when its player logs into it", "[humanlogin]")
{
    // Marked by the summon, own account, game client: unmark and unbind, no bot logic.
    CHECK(HumanClientAction(true, false, false) == HumanLoginAction::Release);
}

TEST_CASE("HumanLogin: a summoned alt logging in as a bot keeps its bot logic", "[humanlogin]")
{
    CHECK(HumanClientAction(true, true, false) == HumanLoginAction::None);
}

TEST_CASE("HumanLogin: world bots and companions logging in headless are untouched", "[humanlogin]")
{
    CHECK(HumanClientAction(true, true, true) == HumanLoginAction::None);
}

TEST_CASE("HumanLogin: a bot pool character logged into by hand gets no bot logic but stays a bot", "[humanlogin]")
{
    CHECK(HumanClientAction(true, false, true) == HumanLoginAction::SkipThisLogin);
}

TEST_CASE("HumanLogin: an ordinary character is left alone", "[humanlogin]")
{
    CHECK(HumanClientAction(false, false, false) == HumanLoginAction::None);
    CHECK(HumanClientAction(false, true, false) == HumanLoginAction::None);
}

TEST_CASE("HumanLogin: self-bot is never saved, so it cannot come back at login", "[humanlogin]")
{
    // `.playerbot self on` must use the transient mark, and the login and logout hooks must run the check.
    std::string const self_cmd = ReadSourceFile("src/modules/PlayerbotV2/Commands/cs_playerbot_v2.cpp");
    std::size_t const on = self_cmd.find("if (mode == \"on\")", self_cmd.find("static bool HandleSelf("));
    std::size_t const off = self_cmd.find("if (mode == \"off\")", on);
    REQUIRE(on != std::string::npos);
    REQUIRE(off != std::string::npos);
    std::string const on_branch = self_cmd.substr(on, off - on);
    CHECK(on_branch.find("mark_as_bot_transient(id)") != std::string::npos);
    CHECK(on_branch.find("mark_as_bot(id)") == std::string::npos);

    std::string const hooks = ReadSourceFile("src/modules/PlayerbotV2/PlayerbotV2.cpp");
    std::size_t const login = hooks.find("void Module::OnPlayerLogin(");
    std::size_t const logout = hooks.find("void Module::OnPlayerLogout(");
    REQUIRE(login != std::string::npos);
    REQUIRE(logout != std::string::npos);
    CHECK(hooks.find("HumanClientAction(", login) < hooks.find("Services::Registry().register_bot(", login));
    CHECK(hooks.find("HumanClientAction(", logout) != std::string::npos);
}

TEST_CASE("HumanLogin: the add-on's Self off clears the saved mark like `.playerbot self off`", "[humanlogin]")
{
    std::string const addon = ReadSourceFile("src/modules/PlayerbotV2/Session/AddonControl.cpp");
    std::size_t const toggle = addon.find("void HandleSelfToggle(");
    REQUIRE(toggle != std::string::npos);
    std::size_t const off = addon.find("else if (mode == \"off\")", toggle);
    std::size_t const end = addon.find("SendFields(sess, client_seq, \"SELF_RESP\"", off);
    REQUIRE(off != std::string::npos);
    REQUIRE(end != std::string::npos);
    std::string const off_branch = addon.substr(off, end - off);
    CHECK(off_branch.find("HumanClientAction(") != std::string::npos);
    CHECK(off_branch.find("Lifecycle().unmark_as_bot(id)") != std::string::npos);
}
