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

#include "../../src/modules/PlayerbotV2/Bot/ChannelTalk.h"

#include <set>

using namespace Playerbot::V2::ChannelTalk;

TEST_CASE("Looking for more: roles and the dungeon", "[ClassicChannelTalk]")
{
    Ask a = Parse("LF healer DM");
    REQUIRE(a.kind == Kind::LookingForMore);
    REQUIRE(a.roles == kHealer);
    REQUIRE(a.dungeon >= 0);
    REQUIRE(kDungeons[a.dungeon].map == 36);

    a = Parse("LF2M SFK need tank and heals");
    REQUIRE(a.kind == Kind::LookingForMore);
    REQUIRE(a.roles == (kTank | kHealer));
    REQUIRE(kDungeons[a.dungeon].map == 33);

    a = Parse("LFM strat live, dps");
    REQUIRE(a.kind == Kind::LookingForMore);
    REQUIRE(a.roles == kDps);
    REQUIRE(kDungeons[a.dungeon].map == 329);

    // LFM without a role asks for damage.
    REQUIRE(Parse("lfm brd").roles == kDps);
    // "lf" with neither role nor dungeon is no group call.
    REQUIRE(Parse("lf someone to help me with hogger").kind != Kind::LookingForMore);
}

TEST_CASE("Looking for group", "[ClassicChannelTalk]")
{
    Ask a = Parse("LFG Deadmines, warrior tank");
    REQUIRE(a.kind == Kind::LookingForGroup);
    REQUIRE(a.roles == kTank);
    REQUIRE(kDungeons[a.dungeon].map == 36);
    REQUIRE(Parse("looking for a group for dire maul").dungeon == 17);
    REQUIRE(Parse("lfg").kind == Kind::LookingForGroup);
}

TEST_CASE("Want to buy, by link or by name", "[ClassicChannelTalk]")
{
    Ask a = Parse("WTB |cffffffff|Hitem:2589::::::::20:::::::|h[Linen Cloth]|h|r x20 pst");
    REQUIRE(a.kind == Kind::WantToBuy);
    REQUIRE(a.item_entry == 2589);
    REQUIRE(a.item_name == "linen cloth");

    // The 12.x client colours a link by quality as |cnIQ<q>: and sends every field of the item string.
    a = Parse("WTB |cnIQ2:|Hitem:7071::::::::30::::::::::|h[Iron Buckle]|h|r need 2");
    REQUIRE(a.kind == Kind::WantToBuy);
    REQUIRE(a.item_entry == 7071);
    REQUIRE(a.item_name == "iron buckle");

    a = Parse("wtb silk cloth x40, paying well");
    REQUIRE(a.kind == Kind::WantToBuy);
    REQUIRE(a.item_entry == 0);
    REQUIRE(a.item_name == "silk cloth");

    REQUIRE(Parse("wtb").kind == Kind::None);
}

TEST_CASE("Questions and greetings", "[ClassicChannelTalk]")
{
    REQUIRE(Parse("where is the warlock trainer?").kind == Kind::Question);
    REQUIRE(Parse("anyone know how to get to Booty Bay").kind == Kind::Question);
    REQUIRE(Parse("hi all").kind == Kind::Greeting);
    REQUIRE(Parse("lol").kind == Kind::None);
}

TEST_CASE("A line with an unfilled placeholder is never said", "[ClassicChannelTalk]")
{
    REQUIRE(HasUnfilledPlaceholder("So, the <subzone> is a piece of cake"));
    REQUIRE(HasUnfilledPlaceholder("selling stuff in %zone_name"));
    REQUIRE_FALSE(HasUnfilledPlaceholder("50% off, <3"));
    REQUIRE_FALSE(HasUnfilledPlaceholder("LFG Deadmines, tank here"));
}

TEST_CASE("Bot lines vary and fill every slot", "[ClassicChannelTalk]")
{
    std::set<std::string> calls;
    for (std::uint32_t seed = 0; seed < 400; ++seed)
    {
        std::string const l = LfgCallLine(std::uint8_t(1u << (seed % 3)), (seed & 1) ? "the Deadmines" : nullptr, 20, seed);
        REQUIRE_FALSE(l.empty());
        REQUIRE(l.find('{') == std::string::npos);
        calls.insert(l);
        std::string const o = OfferLine(kHealer, 2, 21, seed);
        REQUIRE(o.find('{') == std::string::npos);
        REQUIRE_FALSE(GroupSpotLine(14, seed).empty());
    }
    REQUIRE(calls.size() >= 40);

    RecentLines r;
    REQUIRE_FALSE(r.Seen("tank LFG"));
    r.Note("tank LFG");
    REQUIRE(r.Seen("Tank lfg"));
    for (int i = 0; i < int(RecentLines::kKeep); ++i)
        r.Note("line " + std::to_string(i));
    REQUIRE_FALSE(r.Seen("tank LFG"));
}
