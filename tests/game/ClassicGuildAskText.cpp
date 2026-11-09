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

#ifdef TESTS_HAVE_PLAYERBOT_V2

#include "../../src/modules/PlayerbotV2/Bot/GuildAskText.h"

using namespace Playerbot::V2::GuildAsk;

namespace {
bool Ask(char const* line) { return IsJoinRequest(Normalize(line)); }
bool Yes(char const* line) { return IsShortYes(Normalize(line)); }
}

TEST_CASE("GuildAsk: English requests to join the bot's guild", "[GuildAsk]")
{
    CHECK(Ask("invite me to your guild"));
    CHECK(Ask("Can I join your guild?"));
    CHECK(Ask("guild inv pls"));
    CHECK(Ask("ginv"));
    CHECK(Ask("is your guild recruiting?"));
    CHECK(Ask("could you invite me to the guild"));
    CHECK(Ask("let me into your guild"));
    CHECK(Ask("any room in your guild?"));
    CHECK(Ask("looking for a guild"));
    CHECK(Ask("GUILD INVITE"));
}

TEST_CASE("GuildAsk: Russian requests", "[GuildAsk]")
{
    CHECK(Ask("пригласи в гильдию"));
    CHECK(Ask("Пригласите меня в гильдию, пожалуйста"));
    CHECK(Ask("можно в гильдию?"));
    CHECK(Ask("ПРИМИ В ГИЛЬДИЮ"));
    CHECK(Ask("хочу в гилду"));
    CHECK(Ask("кинь инвайт в ги"));
    CHECK(Ask("возьмите в клан"));
    CHECK(Ask("как вступить в вашу гильдию"));
    CHECK(Ask("гинв"));
}

TEST_CASE("GuildAsk: lines that are not a request to join", "[GuildAsk]")
{
    CHECK_FALSE(Ask("are you in a guild?"));
    CHECK_FALSE(Ask("what guild are you in"));
    CHECK_FALSE(Ask("want to join my guild?"));
    CHECK_FALSE(Ask("join our guild"));
    CHECK_FALSE(Ask("i don't want a guild"));
    CHECK_FALSE(Ask("no thanks, not looking for a guild"));
    CHECK_FALSE(Ask("invite me to your group"));
    CHECK_FALSE(Ask("invite"));
    CHECK_FALSE(Ask("вступай в мою гильдию"));
    CHECK_FALSE(Ask("не хочу в гильдию"));
    CHECK_FALSE(Ask("ты в гильдии?"));
    CHECK_FALSE(Ask("hi"));
    CHECK_FALSE(Ask(""));
}

TEST_CASE("GuildAsk: short yes after an offer", "[GuildAsk]")
{
    CHECK(Yes("yes"));
    CHECK(Yes("sure, pls"));
    CHECK(Yes("inv"));
    CHECK(Yes("can i get an invite"));
    CHECK(Yes("Да"));
    CHECK(Yes("давай"));
    CHECK(Yes("конечно, пригласи"));
    CHECK(Yes("инв плиз"));
    CHECK_FALSE(Yes("no"));
    CHECK_FALSE(Yes("no thanks"));
    CHECK_FALSE(Yes("нет"));
    CHECK_FALSE(Yes("не надо"));
    CHECK_FALSE(Yes("where are you from"));
    CHECK_FALSE(Yes("yes but i want to tell you a long story first ok"));
}

TEST_CASE("GuildAsk: Normalize folds case and punctuation", "[GuildAsk]")
{
    CHECK(Normalize("  Invite ME!!  ") == "invite me");
    CHECK(Normalize("ПРИГЛАСИ, в Гильдию?") == "пригласи в гильдию");
    CHECK(Normalize("ЁЖ ёж") == "еж еж");
    CHECK(Normalize("don't") == "dont");
}

#endif
