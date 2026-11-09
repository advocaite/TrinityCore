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

#include "../../src/modules/PlayerbotV2/Fleet/WorldFactionMix.h"

#include <set>

using namespace Playerbot::V2::Fleet;

namespace
{
constexpr uint32_t STORMWIND = 1519, ORGRIMMAR = 1637, ELWYNN = 12, DUROTAR = 14, EPL = 139, WPL = 28, STRANGLETHORN = 33,
                   HILLSBRAD = 267, ARATHI = 45, BARRENS = 17, OUTLAND_UNKNOWN = 3483;
}

TEST_CASE("FactionMix tiers", "[worldmix]")
{
    FactionMix m;
    CHECK(FriendlyPctFor(m, STORMWIND) == 90);
    CHECK(FriendlyPctFor(m, ORGRIMMAR) == 90);
    CHECK(FriendlyPctFor(m, ELWYNN) == 90);
    CHECK(FriendlyPctFor(m, DUROTAR) == 90);
    CHECK(FriendlyPctFor(m, BARRENS) == 75);
    for (uint32_t z : { EPL, WPL, STRANGLETHORN, HILLSBRAD, ARATHI })
        CHECK(FriendlyPctFor(m, z) == 60);   // 40% enemy faction
    CHECK(FriendlyPctFor(m, OUTLAND_UNKNOWN) == 80);   // not listed: FriendlyPct
}

TEST_CASE("FactionMix contested zones hold more enemies than cities and home zones", "[worldmix]")
{
    FactionMix m;
    for (ZoneTierEntry const& e : kZoneTiers)
        if (e.tier == ZoneTier::Contested)
            CHECK(FriendlyPctFor(m, e.zone) < FriendlyPctFor(m, STORMWIND));
}

TEST_CASE("FactionMix switch off and clamp", "[worldmix]")
{
    FactionMix m;
    m.by_zone = false;
    m.default_pct = 80;
    CHECK(FriendlyPctFor(m, EPL) == 80);
    CHECK(FriendlyPctFor(m, STORMWIND) == 80);
    m.by_zone = true;
    m.contested_pct = 250;
    CHECK(FriendlyPctFor(m, EPL) == 100);
}

TEST_CASE("FactionMix zone table has no duplicate zone", "[worldmix]")
{
    std::set<uint32_t> seen;
    for (ZoneTierEntry const& e : kZoneTiers)
        CHECK(seen.insert(e.zone).second);
}

TEST_CASE("WorldFactionMix: the share is relative to the player's own faction, Alliance or Horde", "[worldfactionmix]")
{
    FactionMix m;
    uint32_t const pct = FriendlyPctFor(m, 33);   // Stranglethorn, contested
    m.contested_pct = 70;
    for (bool alliance_player : { true, false })
    {
        uint32_t own = 0;
        for (uint32_t roll = 0; roll < 100; ++roll)
            if (BotIsAlliance(alliance_player, roll, FriendlyPctFor(m, 33)) == alliance_player)
                ++own;
        CHECK(own == 70);          // 70% the player's faction, 30% the other, whichever side he is
    }
    CHECK(pct <= 100);
}

TEST_CASE("WorldFactionMix: a capital holds only its own faction, whoever the player is", "[worldfactionmix]")
{
    for (uint32_t roll = 0; roll < 100; ++roll)
    {
        // Ironforge, Stormwind, Darnassus: Alliance bots for an Alliance and for a Horde visitor
        for (uint32_t zone : { 1537u, 1519u, 1657u })
        {
            CHECK(BotIsAllianceInZone(zone, true, roll, 90));
            CHECK(BotIsAllianceInZone(zone, false, roll, 90));
        }
        // Orgrimmar, Undercity, Thunder Bluff: Horde bots for both
        for (uint32_t zone : { 1637u, 1497u, 1638u })
        {
            CHECK_FALSE(BotIsAllianceInZone(zone, true, roll, 90));
            CHECK_FALSE(BotIsAllianceInZone(zone, false, roll, 90));
        }
    }
    // outside a capital the share is still relative to the player
    uint32_t own = 0;
    for (uint32_t roll = 0; roll < 100; ++roll)
        own += BotIsAllianceInZone(33, false, roll, 70) == false;
    CHECK(own == 70);
}
