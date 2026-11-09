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

#include "../../src/modules/PlayerbotV2/Api/PartialRouteStall.h"

using namespace Playerbot::BotMovement;

TEST_CASE("PartialRouteStall: a ramp ping-pong is stopped", "[partialroutestall]")
{
    // Stormwind ramp: from the top the partial ends down the ramp (33 yards farther from Duskwood), from the bottom the next partial
    // ends back at the top. The bot alternates between the two poses for ever.
    PartialRouteStall st;
    float const top = 2400.0f, bottom = 2433.0f;
    int driven = 0;
    for (int i = 0; i < 40; ++i)
    {
        if (!AllowPartialRoute(st, 7, (i % 2) ? bottom : top))
            break;
        ++driven;
    }
    CHECK(driven == int(kStallMaxPartials) + 1);   // the first drive refills the budget, then the budget is spent
    CHECK_FALSE(AllowPartialRoute(st, 7, top));    // stays refused while the bot is no closer
}

TEST_CASE("PartialRouteStall: a long haul that keeps closing in is never stopped", "[partialroutestall]")
{
    PartialRouteStall st;
    float d = 2400.0f;
    for (int i = 0; i < 100; ++i, d -= 60.0f)
        REQUIRE(AllowPartialRoute(st, 7, d));
}

TEST_CASE("PartialRouteStall: escaping a pocket refills the budget", "[partialroutestall]")
{
    // a few legs that lead away from the goal, then a partial that advances: the bot got out
    PartialRouteStall st;
    CHECK(AllowPartialRoute(st, 7, 500.0f));
    for (int i = 0; i < 4; ++i)
        CHECK(AllowPartialRoute(st, 7, 540.0f));
    CHECK(AllowPartialRoute(st, 7, 150.0f));       // closed in by far more than 5 yards
    CHECK(st.stalled == 0);
}

TEST_CASE("PartialRouteStall: a refused bot that is carried closer or given a new goal starts over", "[partialroutestall]")
{
    PartialRouteStall st;
    for (int i = 0; i < 20; ++i)
        AllowPartialRoute(st, 7, 500.0f);
    CHECK_FALSE(AllowPartialRoute(st, 7, 500.0f));
    CHECK(AllowPartialRoute(st, 7, 480.0f));       // moved 20 yards closer by some other means
    for (int i = 0; i < 20; ++i)
        AllowPartialRoute(st, 7, 480.0f);
    CHECK_FALSE(AllowPartialRoute(st, 7, 480.0f));
    CHECK(AllowPartialRoute(st, 8, 480.0f));       // different goal
}

TEST_CASE("PartialRouteStall: creeping gains under five yards still count as stalled", "[partialroutestall]")
{
    PartialRouteStall st;
    float d = 1000.0f;
    int driven = 0;
    for (int i = 0; i < 50; ++i, d -= 1.0f)
    {
        if (!AllowPartialRoute(st, 7, d)) break;
        ++driven;
    }
    CHECK(driven < 20);
}

TEST_CASE("PartialRouteStall: a near goal behind a long route is walked, a goal off the mesh is not", "[partialroutestall]")
{
    // RFC: the tank on the ledge 27 yards from Oggleflint's floor; the only way down winds back through the pit, so the capped
    // partial ends 98.9 yards from him. That is a route cut by the cap and is walked.
    CHECK(PartialIsTruncatedRoute(26.9f, 98.9f));
    // A near goal off the navmesh: Detour's partial ends at the polygon nearest it, about as far as the bot already is. Refused.
    CHECK_FALSE(PartialIsTruncatedRoute(26.9f, 27.5f));
    CHECK_FALSE(PartialIsTruncatedRoute(12.0f, 9.0f));
    // A far goal is walked as before, whatever the partial's end.
    CHECK(PartialIsTruncatedRoute(254.0f, 301.0f));
    CHECK(PartialIsTruncatedRoute(254.0f, 250.0f));
}
