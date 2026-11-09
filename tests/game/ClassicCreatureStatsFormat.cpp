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

#include <algorithm>

#include "../../src/modules/PlayerbotV2/Diagnostics/CreatureStatsFormat.h"
#include "../../src/modules/PlayerbotV2/Diagnostics/SpellAuditFormat.h"

using namespace Playerbot::Diagnostics::CreatureStats;

TEST_CASE("A creature_stats line is one line of key=value pairs in the order compare_creatures.py reads", "[ClassicCreatureStats]")
{
    Row r;
    r.entry = 12118;
    r.name = "Lucifron";
    r.level = 63;
    r.rank = 3;
    r.health = 3'530'000;
    r.mana = 0;
    r.armor = 3731.4f;
    r.dmg_min = 1500.4f;
    r.dmg_max = 2100.6f;
    r.atk_ms = 2000;
    r.res = { 0, 15, 0, 0, 0, 0 };
    CHECK(FormatLine(r) == "[creature_stats] entry=12118 name=\"Lucifron\" level=63 rank=3 health=3530000 mana=0 armor=3731 dmg_min=1500 dmg_max=2101 atk_ms=2000 res=0/15/0/0/0/0");
}

TEST_CASE("A quote or line break in a creature name cannot break the line", "[ClassicCreatureStats]")
{
    Row r;
    r.entry = 1;
    r.name = "Odd \"Name\"\nTwo";
    std::string const line = FormatLine(r);
    CHECK(line.find('\n') == std::string::npos);
    CHECK(line.find("name=\"Odd 'Name'Two\"") == std::string::npos);
    CHECK(line.find("name=\"Odd 'Name''Two\"") != std::string::npos);
}

TEST_CASE("A spell_audit line has one tab separated column per header column", "[ClassicSpellAudit]")
{
    namespace Sa = Playerbot::Diagnostics::SpellAudit;
    Sa::Row r;
    r.id = 25307;
    r.name = "Shadow\tBolt";
    r.school_mask = 32;
    r.cast_ms = 3000;
    r.recovery_ms = 0;
    r.duration_ms = -1;
    r.range_max = 30.0f;
    r.mana_cost = 600;
    r.power_row_id = 77;
    r.family_name = 5;
    r.family_flags = { 1u, 2u, 3u, 4u };
    r.effects[1].radius_b_max = 8.0f;
    r.effects[0].effect = 2;
    r.effects[0].base_points = 257.0f;
    r.effects[0].classic_value = true;
    r.effects[0].classic_mean = 510.0f;
    std::string const line = Sa::FormatLine(r);
    auto tabs = [](std::string const& s) { return std::count(s.begin(), s.end(), '\t'); };
    CHECK(tabs(line) == tabs(Sa::Header()));
    CHECK(line.rfind("25307\tShadow Bolt\t32\t3000\t0\t0\t-1\t30.0\t", 0) == 0);
    CHECK(line.find("\t2\t0\t257.00\t510.00\t") != std::string::npos);
    CHECK(line.find("\t5\t1\t2\t3\t4\t0.0\t8.0\t0.0") != std::string::npos);        // family name and flags, then the TargetB radius of each effect
    CHECK(line.size() > 11);
    CHECK(line.substr(line.size() - 11) == "\t600\t0.0\t77");                  // the client mana cost and its SpellPower row come last
    CHECK(Sa::Header().find("\tfamily_name\tfamily_0\tfamily_1\tfamily_2\tfamily_3\te1_radius_b") != std::string::npos);
}
