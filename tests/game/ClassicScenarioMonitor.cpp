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

#include "../../src/modules/PlayerbotV2/Diagnostics/ScenarioDefs.h"

#include <cctype>
#include <cstdio>
#include <cmath>
#include <set>
#include <string>

using namespace Playerbot::Diagnostics;

namespace {

ScenarioSample Bot(std::uint64_t id, char const* name, ScenarioRoleKind role, float x, float y)
{
    ScenarioSample s;
    s.id = id; s.name = name; s.role = role;
    s.map = 36; s.x = x; s.y = y; s.z = 50.f;
    return s;
}

// A five-man squad whose members each stand `step` yards further on every second (all walking), or still when step is 0.
std::vector<ScenarioSample> Squad(float x, float y)
{
    return {
        Bot(1, "Tank",   ScenarioRoleKind::Tank,   x,        y),
        Bot(2, "Healer", ScenarioRoleKind::Healer, x - 5.f,  y),
        Bot(3, "Mage",   ScenarioRoleKind::Dps,    x - 8.f,  y + 3.f),
        Bot(4, "Rogue",  ScenarioRoleKind::Dps,    x + 2.f,  y - 3.f),
        Bot(5, "Hunter", ScenarioRoleKind::Dps,    x - 10.f, y - 2.f),
    };
}

ScenarioLimits Limits()
{
    ScenarioLimits l;
    l.expected_map = 36;
    l.bounds = { true, -420.f, 100.f, -900.f, -340.f, 0.f, 110.f };
    return l;
}

bool Has(ScenarioMonitor const& m, ScenarioFailKind k, char const* bot = nullptr)
{
    for (ScenarioFailure const& f : m.failures())
        if (f.kind == k && (!bot || f.bot == bot))
            return true;
    return false;
}

} // namespace

TEST_CASE("ScenarioMonitor: a squad that walks the whole way has no failures", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    for (std::uint32_t t = 0; t <= 300000; t += 1000)
        CHECK_FALSE(m.Feed(t, Squad(-100.f - float(t) / 1000.f, -500.f), true));
    CHECK(m.failures().empty());
    CHECK(m.passed(true));
    CHECK_FALSE(m.passed(false));
}

TEST_CASE("ScenarioMonitor: a bot that stands while the others walk is idle after 45 seconds, once", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    for (std::uint32_t t = 0; t <= 100000; t += 1000)
    {
        auto squad = Squad(-100.f - float(t) / 1000.f, -500.f);
        squad[2].x = -108.f; squad[2].y = -497.f;       // the mage never moves
        m.Feed(t, squad, true);
    }
    REQUIRE(m.failures().size() >= 1);
    CHECK(Has(m, ScenarioFailKind::Idle, "Mage"));
    std::size_t idle = 0;
    for (ScenarioFailure const& f : m.failures())
        if (f.kind == ScenarioFailKind::Idle)
        {
            ++idle;
            CHECK(f.at_ms >= 45000);
            CHECK(f.at_ms <= 47000);
        }
    CHECK(idle == 1);
}

TEST_CASE("ScenarioMonitor: standing is fine in a fight, while casting, and while recovering mana or health", "[scenariomonitor]")
{
    for (int reason = 0; reason < 3; ++reason)
    {
        ScenarioMonitor m(Limits());
        for (std::uint32_t t = 0; t <= 100000; t += 1000)
        {
            auto squad = Squad(-100.f - float(t) / 1000.f, -500.f);
            squad[1].x = -105.f; squad[1].y = -500.f;   // the healer stays put
            if (reason == 0) squad[1].casting = true;
            if (reason == 1) squad[1].mana_pct = 30.f;
            if (reason == 2) squad[1].hp_pct = 40.f;
            m.Feed(t, squad, true);
        }
        CHECK_FALSE(Has(m, ScenarioFailKind::Idle));
    }
}

TEST_CASE("ScenarioMonitor: a group that all waits is stalled, not idle, and ends the run after two minutes", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    bool terminal = false;
    std::uint32_t at = 0;
    for (std::uint32_t t = 0; t <= 200000 && !terminal; t += 1000)
    {
        terminal = m.Feed(t, Squad(-100.f, -500.f), true);
        at = t;
    }
    CHECK(terminal);
    CHECK(at >= 120000);
    CHECK(at <= 122000);
    CHECK(Has(m, ScenarioFailKind::GroupStalled));
    CHECK_FALSE(Has(m, ScenarioFailKind::Idle));
}

TEST_CASE("ScenarioMonitor: a fight keeps a standing group from counting as stalled", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    for (std::uint32_t t = 0; t <= 400000; t += 1000)
    {
        auto squad = Squad(-100.f, -500.f);
        squad[0].in_combat = true;
        CHECK_FALSE(m.Feed(t, squad, true));
    }
    CHECK(m.failures().empty());
}

TEST_CASE("ScenarioMonitor: a bot far from the tank for 20 seconds has strayed", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    for (std::uint32_t t = 0; t <= 60000; t += 1000)
    {
        float const x = -100.f - float(t) / 1000.f * 2.f;
        auto squad = Squad(x, -500.f);
        squad[3].x = x + 90.f;                          // the rogue runs ahead, but keeps walking
        squad[3].y += float(t) / 1000.f;
        m.Feed(t, squad, true);
    }
    CHECK(Has(m, ScenarioFailKind::Stray, "Rogue"));
    CHECK_FALSE(Has(m, ScenarioFailKind::Stray, "Mage"));
}

TEST_CASE("ScenarioMonitor: falling out of the map and walking out of the box are out_of_bounds, the wrong map is wrong_map", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    auto squad = Squad(-100.f, -500.f);
    squad[2].z = -40.f;                                 // under the floor
    squad[3].x = 400.f;                                 // outside the box
    squad[4].map = 0;                                   // still in Stormwind
    m.Feed(0, squad, true);
    CHECK(Has(m, ScenarioFailKind::OutOfBounds, "Mage"));
    CHECK(Has(m, ScenarioFailKind::OutOfBounds, "Rogue"));
    CHECK(Has(m, ScenarioFailKind::WrongMap, "Hunter"));
    CHECK_FALSE(Has(m, ScenarioFailKind::OutOfBounds, "Hunter"));

    ScenarioMonitor before(Limits());
    before.Feed(0, squad, /*entered*/ false);           // not sent in yet: nothing to judge
    CHECK(before.failures().empty());
}

TEST_CASE("ScenarioMonitor: a raider that died and stands on another map afterwards is a death, not a wrong_map", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    auto squad = Squad(-100.f, -500.f);
    squad[4].alive = false;                             // the hunter falls (ghost sample)
    m.Feed(0, squad, true);
    squad[4].alive = true;                              // and is back at a spirit healer in Stormwind
    squad[4].map = 0;
    m.Feed(1000, squad, true);
    CHECK_FALSE(Has(m, ScenarioFailKind::WrongMap, "Hunter"));
    CHECK_FALSE(Has(m, ScenarioFailKind::OutOfBounds, "Hunter"));

    ScenarioMonitor living(Limits());
    auto other = Squad(-100.f, -500.f);
    other[4].map = 0;                                   // never died: still a wrong map
    living.Feed(0, other, true);
    CHECK(Has(living, ScenarioFailKind::WrongMap, "Hunter"));
}

TEST_CASE("ScenarioMonitor: a healer under 10 percent mana for 10 seconds is out of mana", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    for (std::uint32_t t = 0; t <= 30000; t += 1000)
    {
        auto squad = Squad(-100.f - float(t) / 1000.f, -500.f);
        squad[1].mana_pct = 4.f;
        squad[1].in_combat = true;
        m.Feed(t, squad, true);
    }
    CHECK(Has(m, ScenarioFailKind::HealerOom, "Healer"));

    ScenarioMonitor dip(Limits());
    for (std::uint32_t t = 0; t <= 30000; t += 1000)
    {
        auto squad = Squad(-100.f - float(t) / 1000.f, -500.f);
        squad[1].mana_pct = (t % 8000 < 4000) ? 4.f : 50.f;     // a short dip is not OOM
        dip.Feed(t, squad, true);
    }
    CHECK_FALSE(Has(dip, ScenarioFailKind::HealerOom));
}

TEST_CASE("ScenarioMonitor: a death keeps who killed the bot, with what, and where", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    ScenarioDeath d;
    d.name = "Scnmage"; d.x = -190.f; d.y = -450.f; d.z = 54.f;
    d.killer = "Defias Evoker (1729)"; d.spell = "11829 Flamestrike"; d.damage = 479; d.rule = "idle:dungeon_dispatch";
    m.NoteDeath(61000, d);
    REQUIRE(m.failures().size() == 1);
    ScenarioFailure const& f = m.failures().front();
    CHECK(f.kind == ScenarioFailKind::Death);
    CHECK(f.bot == "Scnmage");
    CHECK(f.at_ms == 61000);
    CHECK(f.detail.find("Defias Evoker (1729)") != std::string::npos);
    CHECK(f.detail.find("11829 Flamestrike 479") != std::string::npos);
    CHECK(f.detail.find("-190.0,-450.0,54.0") != std::string::npos);
    CHECK_FALSE(m.passed(true));
}

TEST_CASE("ScenarioMonitor: all members dead is a wipe and ends the run", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    auto squad = Squad(-100.f, -500.f);
    for (ScenarioSample& s : squad)
        s.alive = false;
    CHECK(m.Feed(5000, squad, true));
    CHECK(Has(m, ScenarioFailKind::Wipe));
    CHECK(m.Feed(6000, squad, true));
    std::size_t wipes = 0;
    for (ScenarioFailure const& f : m.failures())
        if (f.kind == ScenarioFailKind::Wipe) ++wipes;
    CHECK(wipes == 1);
}

TEST_CASE("ScenarioDefs: the Deadmines scenario is a five-man party with one tank and one healer, ending at Rhahk'Zor", "[scenariomonitor]")
{
    ScenarioDef const* d = FindScenario("deadmines_rhahkzor");
    REQUIRE(d != nullptr);
    CHECK(FindScenario("no_such_scenario") == nullptr);
    CHECK(d->squad.size() == 5);
    int tanks = 0, healers = 0;
    for (ScenarioMember const& m : d->squad)
    {
        if (m.role == ScenarioRoleKind::Tank) ++tanks;
        if (m.role == ScenarioRoleKind::Healer) ++healers;
        std::string const name = m.name;
        CHECK(name.size() >= 2);
        CHECK(name.size() <= 12);
        for (char c : name)
            CHECK(std::isalpha(static_cast<unsigned char>(c)));
    }
    CHECK(tanks == 1);
    CHECK(healers == 1);
    CHECK(d->map == 36);
    CHECK(d->limits.expected_map == 36);
    CHECK(d->goal == ScenarioGoalKind::KillCreature);
    CHECK(d->goal_entry == 644);
    // Within the squad every name is unique.
    for (std::size_t i = 0; i < d->squad.size(); ++i)
        for (std::size_t j = i + 1; j < d->squad.size(); ++j)
            CHECK(std::string(d->squad[i].name) != d->squad[j].name);
}

TEST_CASE("ScenarioMonitor: the first death ends the run, so nothing that follows it is reported", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    CHECK_FALSE(m.terminal());
    ScenarioDeath d;
    d.name = "Scnwarrior";
    m.NoteDeath(34000, d);
    CHECK(m.terminal());
    CHECK_FALSE(m.passed(true));

    ScenarioLimits tolerant = Limits();
    tolerant.max_deaths = 2;
    ScenarioMonitor two(tolerant);
    two.NoteDeath(1000, d);
    CHECK_FALSE(two.terminal());
    two.NoteDeath(2000, d);
    CHECK(two.terminal());

    ScenarioLimits never = Limits();
    never.max_deaths = 0;
    ScenarioMonitor free_run(never);
    for (int i = 0; i < 5; ++i)
        free_run.NoteDeath(1000u * std::uint32_t(i), d);
    CHECK_FALSE(free_run.terminal());
}

TEST_CASE("ScenarioMonitor: a bot that is in no group, or a smaller one, is group_broken once", "[scenariomonitor]")
{
    ScenarioLimits l = Limits();
    l.expected_group = 5;
    ScenarioMonitor m(l);
    for (std::uint32_t t = 0; t <= 5000; t += 1000)
    {
        auto squad = Squad(-100.f - float(t) / 1000.f, -500.f);
        for (ScenarioSample& s : squad)
            s.group_size = 5;
        squad[0].group_size = 0;                        // the tank lost its group
        squad[2].group_size = 3;
        m.Feed(t, squad, true);
    }
    CHECK(Has(m, ScenarioFailKind::GroupBroken, "Tank"));
    CHECK(Has(m, ScenarioFailKind::GroupBroken, "Mage"));
    CHECK_FALSE(Has(m, ScenarioFailKind::GroupBroken, "Healer"));
    std::size_t n = 0;
    for (ScenarioFailure const& f : m.failures())
        if (f.kind == ScenarioFailKind::GroupBroken) ++n;
    CHECK(n == 2);

    ScenarioMonitor off(Limits());                      // expected_group 0: not checked
    off.Feed(0, Squad(-100.f, -500.f), true);
    CHECK(off.failures().empty());
}

TEST_CASE("ScenarioMonitor: the rule trace keeps each bot's rule changes, not every second", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    for (std::uint32_t t = 0; t <= 9000; t += 1000)
    {
        auto squad = Squad(-100.f - float(t) / 1000.f, -500.f);
        squad[1].rule = t < 4000 ? "idle:dungeon_follow" : "classic_heal:big_heal";
        squad[0].rule = "combat:auto_attack";
        m.Feed(t, squad, true);
    }
    auto const trace = m.Trace();
    std::size_t healer_lines = 0, tank_lines = 0;
    for (std::string const& l : trace)
    {
        if (l.rfind("Healer ", 0) == 0) ++healer_lines;
        if (l.rfind("Tank ", 0) == 0) ++tank_lines;
    }
    CHECK(healer_lines == 2);
    CHECK(tank_lines == 1);
    bool found = false;
    for (std::string const& l : trace)
        if (l == "Healer t=4s classic_heal:big_heal") found = true;
    CHECK(found);
    CHECK(m.Trace(1).size() == 5);                      // one line per bot
}

TEST_CASE("ScenarioDefs: every dungeon row makes a complete scenario with a unique name", "[scenariomonitor]")
{
    std::vector<ScenarioDef> all;
    for (DungeonRow const& r : kDungeons)
        all.push_back(MakeDungeonScenario(r));
    CHECK(all.size() == sizeof(kDungeons) / sizeof(kDungeons[0]));
    CHECK(all.size() >= 12);
    // Lowest level first: a runall levels the shared party up from row to row and never down.
    for (std::size_t i = 1; i < all.size(); ++i)
        CHECK(all[i].level >= all[i - 1].level);
    std::vector<std::string> names;
    std::vector<std::uint32_t> maps;
    for (ScenarioDef const& d : all)
    {
        INFO(d.name);
        std::string const n = d.name;
        for (std::string const& seen : names)
            CHECK(seen != n);
        names.push_back(n);
        CHECK(FindScenario(n) != nullptr);
        CHECK(d.level >= 10);
        CHECK(d.level <= 60);
        CHECK(d.map != 0);
        CHECK(d.goal_entry != 0);
        CHECK(std::string(d.goal_name).size() > 2);
        CHECK(d.timeout_ms >= 10u * 60u * 1000u);
        CHECK(d.squad.size() == 5);
        CHECK(d.limits.expected_map == d.map);
        CHECK(d.limits.bounds.enabled);
        // The party arrives inside its own box.
        CHECK(d.x >= d.limits.bounds.min_x);
        CHECK(d.x <= d.limits.bounds.max_x);
        CHECK(d.y >= d.limits.bounds.min_y);
        CHECK(d.y <= d.limits.bounds.max_y);
        CHECK(d.z >= d.limits.bounds.min_z);
        CHECK(d.z <= d.limits.bounds.max_z);
        CHECK(d.o >= 0.f);
        CHECK(d.o <= 6.2832f);
    }
    // The shared party is five characters in total, whatever the number of dungeons (a name is one character).
    std::vector<std::string> chars;
    for (ScenarioDef const& d : all)
        for (ScenarioMember const& m : d.squad)
        {
            bool known = false;
            for (std::string const& c : chars)
                if (c == m.name) known = true;
            if (!known) chars.push_back(m.name);
        }
    CHECK(chars.size() == 5);
}

// ---- raid scenarios ----

namespace {

// 40 raiders: 4 tanks, 12 healers, 24 DPS, all standing still `spread` yards apart on a line.
std::vector<ScenarioSample> Raid40(float x, float y)
{
    std::vector<ScenarioSample> v;
    for (int i = 0; i < 40; ++i)
    {
        ScenarioRoleKind const role = i < 4 ? ScenarioRoleKind::Tank : i < 16 ? ScenarioRoleKind::Healer : ScenarioRoleKind::Dps;
        char name[16];
        std::snprintf(name, sizeof(name), "Raider%02d", i);
        ScenarioSample s = Bot(std::uint64_t(100 + i), name, role, x + float(i % 8) * 2.f, y - float(i / 8) * 2.f);
        s.map = 409;
        s.z = -100.f;
        v.push_back(s);
    }
    return v;
}

ScenarioLimits RaidLimits()
{
    ScenarioLimits l;
    l.expected_map = 409;
    l.tolerated_deaths = 5;
    l.max_deaths = 6;
    l.dps_hold_ms = 20000;
    l.tanks_dead_is_wipe = true;
    return l;
}

// The pass bar of the raid scenarios: the boss's death passes the run however many die.
ScenarioLimits KillPassLimits()
{
    ScenarioLimits l;
    l.expected_map = 409;
    l.kill_is_pass = true;
    l.dps_hold_ms = 20000;
    return l;
}

bool HasWarning(ScenarioMonitor const& m, ScenarioFailKind k, char const* bot = nullptr)
{
    for (ScenarioFailure const& f : m.warnings())
        if (f.kind == k && (!bot || f.bot == bot))
            return true;
    return false;
}

ScenarioDeath Died(std::uint64_t id, char const* name)
{
    ScenarioDeath d;
    d.id = id; d.name = name; d.killer = "Firesworn (12099)"; d.spell = "melee"; d.damage = 900;
    return d;
}

} // namespace

TEST_CASE("ScenarioMonitor raid: up to five deaths are noted, the sixth fails the run and ends it", "[scenariomonitor]")
{
    ScenarioMonitor m(RaidLimits());
    auto raid = Raid40(1000.f, -600.f);
    m.Feed(1000, raid, true);
    for (int i = 0; i < 5; ++i)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "Raider%02d", 20 + i);
        m.NoteDeath(2000u + 1000u * std::uint32_t(i), Died(std::uint64_t(120 + i), name));
    }
    CHECK(m.failures().empty());
    CHECK_FALSE(m.terminal());
    CHECK(m.tolerated().size() == 5);
    CHECK(m.passed(true));
    m.NoteDeath(9000, Died(125, "Raider25"));
    CHECK(Has(m, ScenarioFailKind::Death, "Raider25"));
    CHECK(m.terminal());
    CHECK_FALSE(m.passed(true));
    CHECK(m.deaths() == 6);
}

TEST_CASE("ScenarioMonitor raid, kill is the pass bar: deaths are warnings, never end the run, and a kill passes", "[scenariomonitor]")
{
    ScenarioMonitor m(KillPassLimits());
    auto raid = Raid40(1000.f, -600.f);
    m.Feed(1000, raid, true);
    for (int i = 0; i < 20; ++i)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "Raider%02d", 20 + i);
        m.NoteDeath(2000u + 1000u * std::uint32_t(i), Died(std::uint64_t(120 + i), name));
    }
    CHECK(m.failures().empty());
    CHECK(m.warnings().size() == 20);
    CHECK(HasWarning(m, ScenarioFailKind::Death, "Raider25"));
    CHECK_FALSE(m.terminal());
    CHECK(m.deaths() == 20);
    CHECK(m.passed(true));
    CHECK_FALSE(m.passed(false));                 // no kill, no pass
}

TEST_CASE("ScenarioMonitor raid, kill is the pass bar: dead tanks do not end the run, a full wipe, a reset and a timeout do", "[scenariomonitor]")
{
    {
        ScenarioMonitor m(KillPassLimits());
        auto raid = Raid40(1000.f, -600.f);
        for (int i = 0; i < 4; ++i)
            raid[std::size_t(i)].alive = false;
        CHECK_FALSE(m.Feed(1000, raid, true));
        CHECK(m.failures().empty());
        for (ScenarioSample& s : raid)
            s.alive = false;
        CHECK(m.Feed(2000, raid, true));
        CHECK(Has(m, ScenarioFailKind::Wipe));
        CHECK_FALSE(m.passed(true));
    }
    {
        ScenarioMonitor m(KillPassLimits());
        m.NoteTimeout(900000, "goal not met");
        CHECK(Has(m, ScenarioFailKind::Timeout));
        CHECK_FALSE(m.passed(false));
    }
}

TEST_CASE("ScenarioMonitor raid, kill is the pass bar: a squad with nobody left in the fight, or a tenth alive, is wiped", "[scenariomonitor]")
{
    {
        // One raider alive and out of combat after a fight: over after 15 s, not at the timeout.
        ScenarioMonitor m(KillPassLimits());
        auto raid = Raid40(1000.f, -600.f);
        for (ScenarioSample& s : raid)
            s.in_combat = true;
        CHECK_FALSE(m.Feed(1000, raid, true));
        for (std::size_t i = 1; i < raid.size(); ++i)
            raid[i].alive = false;
        raid[0].in_combat = false;
        CHECK_FALSE(m.Feed(2000, raid, true));
        CHECK_FALSE(m.Feed(16000, raid, true));
        CHECK(m.Feed(17000, raid, true));
        CHECK(Has(m, ScenarioFailKind::Wipe));
    }
    {
        // Four of 40 alive and still in combat: a wipe after 30 s of it.
        ScenarioMonitor m(KillPassLimits());
        auto raid = Raid40(1000.f, -600.f);
        for (ScenarioSample& s : raid)
            s.in_combat = true;
        m.Feed(1000, raid, true);
        for (std::size_t i = 4; i < raid.size(); ++i)
            raid[i].alive = false;
        CHECK_FALSE(m.Feed(2000, raid, true));
        CHECK_FALSE(m.Feed(31000, raid, true));
        CHECK(m.Feed(32000, raid, true));
        CHECK(Has(m, ScenarioFailKind::Wipe));
    }
    {
        // Before any fight, a squad standing out of combat is not a wipe.
        ScenarioMonitor m(KillPassLimits());
        auto raid = Raid40(1000.f, -600.f);
        for (std::uint32_t t = 1000; t <= 60000; t += 1000)
            CHECK_FALSE(m.Feed(t, raid, true));
        CHECK_FALSE(Has(m, ScenarioFailKind::Wipe));
    }
}

TEST_CASE("ScenarioMonitor raid, kill is the pass bar: a DPS hold and a missing support job are warnings", "[scenariomonitor]")
{
    ScenarioMonitor m(KillPassLimits());
    for (std::uint32_t t = 0; t <= 25000; t += 1000)
    {
        auto raid = Raid40(1000.f, -600.f);
        for (ScenarioSample& s : raid)
            s.in_combat = true;
        raid[30].rule = "combat:threat_hold";
        m.Feed(t, raid, true);
    }
    m.NoteSupportMissing(30000, "0 Tranquilizing Shot(s) cast");
    CHECK(HasWarning(m, ScenarioFailKind::DpsHold, "Raider30"));
    CHECK(HasWarning(m, ScenarioFailKind::SupportMissing));
    CHECK(m.failures().empty());
    CHECK(m.passed(true));
}

TEST_CASE("ScenarioMonitor raid: every tank dead is a wipe even with the rest alive", "[scenariomonitor]")
{
    ScenarioMonitor m(RaidLimits());
    auto raid = Raid40(1000.f, -600.f);
    CHECK_FALSE(m.Feed(1000, raid, true));
    for (int i = 0; i < 3; ++i)
        raid[std::size_t(i)].alive = false;
    CHECK_FALSE(m.Feed(2000, raid, true));      // one tank left
    raid[3].alive = false;
    CHECK(m.Feed(3000, raid, true));
    CHECK(Has(m, ScenarioFailKind::Wipe));
}

TEST_CASE("ScenarioMonitor raid: the boss resetting after it was engaged ends the run", "[scenariomonitor]")
{
    ScenarioMonitor m(RaidLimits());
    ScenarioBossSample boss;
    boss.found = true; boss.alive = true; boss.in_combat = false; boss.hp_pct = 100.f;
    CHECK_FALSE(m.NoteBoss(1000, boss));         // not pulled yet: nothing to reset
    boss.in_combat = true; boss.hp_pct = 80.f;
    CHECK_FALSE(m.NoteBoss(20000, boss));
    boss.in_combat = false; boss.hp_pct = 75.f;
    CHECK_FALSE(m.NoteBoss(21000, boss));        // a short gap is a leash flicker
    boss.in_combat = true;
    CHECK_FALSE(m.NoteBoss(22000, boss));
    boss.in_combat = false;
    CHECK_FALSE(m.NoteBoss(23000, boss));
    CHECK(m.NoteBoss(28500, boss));              // out of combat for 5.5 s: it reset
    CHECK(Has(m, ScenarioFailKind::BossReset));
    CHECK(m.terminal());
}

TEST_CASE("ScenarioMonitor raid: a boss that evades is a reset at once; a dead or missing boss is none", "[scenariomonitor]")
{
    ScenarioMonitor m(RaidLimits());
    ScenarioBossSample boss;
    boss.found = true; boss.alive = true; boss.in_combat = true;
    CHECK_FALSE(m.NoteBoss(1000, boss));
    boss.alive = false;
    CHECK_FALSE(m.NoteBoss(2000, boss));
    boss.found = false;
    CHECK_FALSE(m.NoteBoss(3000, boss));
    boss.found = true; boss.alive = true; boss.in_combat = true; boss.evading = true;
    CHECK(m.NoteBoss(4000, boss));
    CHECK(Has(m, ScenarioFailKind::BossReset));
}

TEST_CASE("ScenarioMonitor raid: a DPS held by the threat rules for 20 seconds in a fight fails, a healer does not", "[scenariomonitor]")
{
    ScenarioMonitor m(RaidLimits());
    for (std::uint32_t t = 0; t <= 25000; t += 1000)
    {
        auto raid = Raid40(1000.f, -600.f);
        for (ScenarioSample& s : raid)
            s.in_combat = true;
        raid[30].rule = "combat:threat_wait_tank";          // a DPS
        raid[10].rule = "combat:threat_wait_tank";          // a healer
        raid[31].rule = t < 10000 ? "combat:threat_hold" : "classic_mage:nuke";   // held 10 s, then released
        raid[32].rule = t % 2000 ? "combat:threat_hold" : "raid:threat_hold";     // two hold rules in turn is one hold
        m.Feed(t, raid, true);
    }
    CHECK(Has(m, ScenarioFailKind::DpsHold, "Raider30"));
    CHECK(Has(m, ScenarioFailKind::DpsHold, "Raider32"));
    CHECK_FALSE(Has(m, ScenarioFailKind::DpsHold, "Raider31"));
    CHECK_FALSE(Has(m, ScenarioFailKind::DpsHold, "Raider10"));
    std::size_t holds = 0;
    for (ScenarioFailure const& f : m.failures())
        if (f.kind == ScenarioFailKind::DpsHold) ++holds;
    CHECK(holds == 2);
}

TEST_CASE("ScenarioMonitor raid: a hold outside a fight is not a DPS hold", "[scenariomonitor]")
{
    ScenarioMonitor m(RaidLimits());
    for (std::uint32_t t = 0; t <= 30000; t += 1000)
    {
        auto raid = Raid40(1000.f, -600.f);
        raid[30].rule = "combat:threat_wait_tank";
        m.Feed(t, raid, true);
    }
    CHECK_FALSE(Has(m, ScenarioFailKind::DpsHold));
}

TEST_CASE("ScenarioMonitor raid: the trace shows the tanks, a few healers and DPS, and whoever failed", "[scenariomonitor]")
{
    ScenarioMonitor m(RaidLimits());
    for (std::uint32_t t = 0; t <= 3000; t += 1000)
    {
        auto raid = Raid40(1000.f, -600.f);
        for (ScenarioSample& s : raid)
            s.rule = "r" + std::to_string(t / 1000u);
        m.Feed(t, raid, true);
    }
    m.NoteDeath(3500, Died(139, "Raider39"));
    m.NoteDeath(3600, Died(138, "Raider38"));
    m.NoteDeath(3700, Died(137, "Raider37"));
    m.NoteDeath(3800, Died(136, "Raider36"));
    m.NoteDeath(3900, Died(135, "Raider35"));
    m.NoteDeath(4000, Died(134, "Raider34"));             // the sixth: a failure, so this bot is in the trace
    auto const trace = m.TraceSelected(25, 3, 3);
    std::set<std::string> bots;
    for (std::string const& l : trace)
        bots.insert(l.substr(0, l.find(' ')));
    CHECK(bots.size() == 4 + 3 + 3 + 1);
    CHECK(bots.count("Raider00") == 1);                    // a tank
    CHECK(bots.count("Raider03") == 1);
    CHECK(bots.count("Raider04") == 1);                    // the first healers by name
    CHECK(bots.count("Raider16") == 1);                    // the first DPS
    CHECK(bots.count("Raider34") == 1);                    // the sixth death
    CHECK(bots.count("Raider39") == 0);                    // a tolerated death is not picked
    bool died = false;
    for (std::string const& l : trace)
        if (l.rfind("Raider34 ", 0) == 0 && l.find("DIED") != std::string::npos) died = true;
    CHECK(died);
}

TEST_CASE("ScenarioDefs: the Molten Core scenarios are a 40-man raid with 4 tanks and 12 healers, one boss each", "[scenariomonitor]")
{
    std::set<std::string> found;
    for (RaidBossRow const& r : kMcBosses)
    {
        ScenarioDef const* d = FindScenario(r.name);
        REQUIRE(d != nullptr);
        INFO(r.name);
        found.insert(r.name);
        CHECK(d->map == 409);
        CHECK(d->level == 60);
        CHECK(d->squad.size() == 40);
        CHECK(d->goal_entry == r.boss_entry);
        CHECK(d->arrive_near_entry == r.boss_entry);
        CHECK(d->order_attack);
        CHECK(d->limits.expected_group == 40);
        CHECK(d->limits.kill_is_pass);
        CHECK(d->limits.max_deaths == 0);
        CHECK_FALSE(d->limits.tanks_dead_is_wipe);
        CHECK(d->limits.dps_hold_ms == 20000);
        // Garr's Firesworn must not die before him; no other raid boss checks an add.
        CHECK(d->spare_entry == (r.boss_entry == 12057 ? 12099u : 0u));
        CHECK(d->limits.bounds.enabled);
        CHECK(d->x >= d->limits.bounds.min_x);
        CHECK(d->x <= d->limits.bounds.max_x);
        std::size_t tanks = 0, healers = 0, warlocks = 0;
        std::set<std::string> names;
        for (ScenarioMember const& m : d->squad)
        {
            if (m.role == ScenarioRoleKind::Tank) ++tanks;
            if (m.role == ScenarioRoleKind::Healer) ++healers;
            if (m.cls == ScnClass::Warlock) ++warlocks;
            names.insert(m.name);
            CHECK(m.name.size() >= 2);
            CHECK(m.name.size() <= 12);
            // A character name is letters only, first one upper case, and has no letter three times in a row.
            CHECK(std::isupper(static_cast<unsigned char>(m.name[0])));
            for (std::size_t i = 0; i < m.name.size(); ++i)
            {
                CHECK(std::isalpha(static_cast<unsigned char>(m.name[i])));
                if (i) CHECK(((i > 1 && std::tolower(m.name[i]) == std::tolower(m.name[i - 1]) && std::tolower(m.name[i]) == std::tolower(m.name[i - 2])) == false));
            }
        }
        CHECK(tanks == 4);
        CHECK(healers == 12);
        CHECK(warlocks >= 3);                                   // Banish for Garr's Firesworn
        CHECK(names.size() == 40);
        // The raid is one faction: Alliance races only.
        for (ScenarioMember const& m : d->squad)
            CHECK((m.race == ScnRace::Human || m.race == ScnRace::Dwarf || m.race == ScnRace::NightElf || m.race == ScnRace::Gnome));
    }
    // The raid set, by name: a boss added to or dropped from it has to be said here (mc_geddon joined the first four, mc_ragnaros after it).
    CHECK(found == std::set<std::string>{ "mc_garr", "mc_lucifron", "mc_magmadar", "mc_shazzrah", "mc_gehennas", "mc_sulfuron", "mc_golemagg", "mc_majordomo", "mc_geddon", "mc_ragnaros" });
    // Both catalogues share one list, with no clash between a dungeon and a raid name.
    std::set<std::string> all;
    for (ScenarioDef const& d : ScenarioCatalogue())
        CHECK(all.insert(d.name).second);
}

TEST_CASE("ScenarioDefs: the other raids are one boss each, 40 raiders (BWL, AQ40, Naxxramas) or 20 (ZG, AQ20), their adds kept", "[scenariomonitor]")
{
    struct Family { RaidBossRow const* rows; std::size_t n; std::uint32_t map; std::size_t raiders; char const* prefix; };
    Family const families[] = {
        { kBwlBosses,  sizeof(kBwlBosses) / sizeof(kBwlBosses[0]),   469, 40, "bwl_" },
        { kZgBosses,   sizeof(kZgBosses) / sizeof(kZgBosses[0]),     309, 20, "zg_" },
        { kAq20Bosses, sizeof(kAq20Bosses) / sizeof(kAq20Bosses[0]), 509, 20, "aq20_" },
        { kAq40Bosses, sizeof(kAq40Bosses) / sizeof(kAq40Bosses[0]), 531, 40, "aq40_" },
        { kNaxxBosses, sizeof(kNaxxBosses) / sizeof(kNaxxBosses[0]), 533, 40, "naxx_" },
    };
    std::set<std::string> found;
    for (Family const& f : families)
        for (std::size_t i = 0; i < f.n; ++i)
        {
            RaidBossRow const& r = f.rows[i];
            ScenarioDef const* d = FindScenario(r.name);
            REQUIRE(d != nullptr);
            INFO(r.name);
            CHECK(found.insert(r.name).second);
            CHECK(std::string(r.name).rfind(f.prefix, 0) == 0);
            CHECK(d->map == f.map);
            CHECK(d->level == 60);
            CHECK(d->squad.size() == f.raiders);
            CHECK(d->limits.expected_group == f.raiders);
            CHECK(d->goal_entry == r.boss_entry);
            CHECK(d->arrive_near_entry == r.boss_entry);
            CHECK(std::string(d->goal_name).size() > 2);
            CHECK(d->order_attack);
            CHECK(d->limits.kill_is_pass);
            CHECK(d->timeout_ms >= 15u * 60u * 1000u);
            CHECK(d->keep_entry == r.keep_entry);
            CHECK(d->keep_entry_b == r.keep_entry_b);
            CHECK(d->keep_entry_c == r.keep_entry_c);
            // A kept add is never the boss itself, and the extra ones only come with a first one.
            CHECK(d->keep_entry != r.boss_entry);
            if (d->keep_entry_b || d->keep_entry_c)
                CHECK(d->keep_entry != 0);
            CHECK(d->limits.bounds.enabled);
            CHECK(d->x >= d->limits.bounds.min_x);
            CHECK(d->x <= d->limits.bounds.max_x);
            std::size_t tanks = 0, healers = 0;
            std::set<std::string> names;
            for (ScenarioMember const& m : d->squad)
            {
                if (m.role == ScenarioRoleKind::Tank) ++tanks;
                if (m.role == ScenarioRoleKind::Healer) ++healers;
                names.insert(m.name);
                CHECK((m.race == ScnRace::Human || m.race == ScnRace::Dwarf || m.race == ScnRace::NightElf || m.race == ScnRace::Gnome));
            }
            CHECK(names.size() == f.raiders);
            CHECK(tanks == (f.raiders == 40 ? 4u : 2u));
            CHECK(healers == (f.raiders == 40 ? 12u : 6u));
        }
    // The three fights that keep two or three kinds of add: the bug trio (Yauj, Kri), Thekal (Zath, Lor'Khan), the Horsemen (the other three).
    CHECK(FindScenario("aq40_bugtrio")->keep_entry_c == 0);
    CHECK(FindScenario("zg_thekal")->keep_entry_b == 11347);
    CHECK(FindScenario("zg_mandokir")->start_add == 11391);   // the order is onto the Vilebranch Speaker; Mandokir is immune until he dies
    CHECK(FindScenario("zg_jindo")->start_add == 0);
    // Hakkar is fought after the five High Priests: their DONE states, then his Power stacks recounted (type 0).
    REQUIRE(FindScenario("zg_hakkar")->start_instance_data.size() == 6);
    CHECK(FindScenario("zg_hakkar")->start_instance_data.back() == std::pair<std::uint32_t, std::uint32_t>{ 0u, 0u });
    CHECK(FindScenario("zg_jindo")->start_instance_data.empty());
    CHECK(FindScenario("naxx_horsemen")->keep_entry_c == 16065);
    CHECK(found.size() == 5 + 9 + 5 + 8 + 14);
    // Arlokk has no creature row (the gong summons her): the runner summons her at the gong's spot.
    CHECK(FindScenario("zg_arlokk")->lair_z != 0.f);
}

TEST_CASE("deadmines_full is a whole-dungeon clear that runall leaves out", "[ScenarioMonitor]")
{
    auto const* d = Playerbot::Diagnostics::FindScenario("deadmines_full");
    REQUIRE(d != nullptr);
    CHECK(d->goal_entry == 639);
    CHECK(d->full_clear);
    CHECK(d->squad.size() == 5);
    CHECK(d->limits.bounds.min_y <= -877.f);
    CHECK(d->timeout_ms == 45u * 60u * 1000u);
}

TEST_CASE("deadmines_player is deadmines_full with the rogue played like a player", "[ScenarioMonitor]")
{
    auto const* d = Playerbot::Diagnostics::FindScenario("deadmines_player");
    auto const* f = Playerbot::Diagnostics::FindScenario("deadmines_full");
    REQUIRE(d != nullptr);
    REQUIRE(f != nullptr);
    CHECK(std::string(d->stand_in) == "Scnrogue");
    CHECK(f->stand_in == nullptr);
    CHECK(d->goal_entry == f->goal_entry);
    CHECK(d->full_clear);
    bool found = false;
    for (auto const& m : d->squad)
        found = found || m.name == d->stand_in;
    CHECK(found);
}

TEST_CASE("ScenarioMonitor: a failure keeps the failing bot's rule changes from the 90 s before it, including a hold that never changed", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    for (std::uint32_t t = 0; t <= 100000; t += 1000)
    {
        auto squad = Squad(-100.f - float(t) / 1000.f, -500.f);
        squad[2].x = -108.f; squad[2].y = -497.f;       // the mage never moves
        squad[2].rule = t < 20000 ? "combat:nuke" : "idle:dungeon_hold";
        m.Feed(t, squad, true);
    }
    REQUIRE(Has(m, ScenarioFailKind::Idle, "Mage"));
    bool nuke = false, hold = false;
    for (std::string const& c : m.contexts())
    {
        if (c.rfind("idle Mage t=", 0) != 0)
            continue;   // the mage that stays behind also strays
        nuke = nuke || c.find("combat:nuke") != std::string::npos;
        hold = hold || c.find("idle:dungeon_hold") != std::string::npos;
    }
    CHECK(nuke);
    CHECK(hold);
}

TEST_CASE("ScenarioMonitor: a squad waiting for its healer's mana is not idle, a healer that never drinks back is reported", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    for (std::uint32_t t = 0; t <= 120000; t += 1000)
    {
        auto squad = Squad(-100.f, -500.f);
        for (auto& b : squad)
            b.mana_pct = 100.f;
        squad[0].x = -100.f; squad[0].y = -500.f;               // the tank waits
        squad[3].x = -100.f + float(t % 20000) / 1000.f;        // the rogue wanders
        squad[1].mana_pct = 50.f;                               // the healer is not drinking back
        m.Feed(t, squad, true);
    }
    CHECK_FALSE(Has(m, ScenarioFailKind::Idle, "Tank"));
    CHECK(Has(m, ScenarioFailKind::HealerNoRest, "Healer"));
}

TEST_CASE("ScenarioMonitor: a squad standing while a member sits drinking is not idle", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    for (std::uint32_t t = 0; t <= 120000; t += 1000)
    {
        auto squad = Squad(-100.f, -500.f);
        for (auto& b : squad)
            b.mana_pct = 100.f;
        squad[0].x = -100.f; squad[0].y = -500.f;               // the tank waits
        squad[3].x = -100.f + float(t % 20000) / 1000.f;        // the rogue wanders
        squad[1].mana_pct = 90.f;                               // above the pull-ready mana, still sitting to 95%
        squad[1].rule = "idle:dungeon_healer_drink:sitting";
        m.Feed(t, squad, true);
    }
    CHECK_FALSE(Has(m, ScenarioFailKind::Idle, "Tank"));
}

TEST_CASE("ScenarioMonitor: a healer under 10 percent mana between fights is not out of mana", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    for (std::uint32_t t = 0; t <= 30000; t += 1000)
    {
        auto squad = Squad(-100.f - float(t) / 1000.f, -500.f);
        squad[1].mana_pct = 4.f;   // nobody is fighting: a drink in progress
        m.Feed(t, squad, true);
    }
    CHECK_FALSE(Has(m, ScenarioFailKind::HealerOom));
}

TEST_CASE("ScenarioMonitor: a missing support job is a failure of its own kind, a count line is not", "[scenariomonitor]")
{
    ScenarioMonitor m(RaidLimits());
    m.NoteInfo("support: tranquilizing shots 0, curses removed 3, formation slot give-ups 2");
    CHECK(m.failures().empty());
    CHECK(m.passed(true));
    m.NoteSupportMissing(90000, "0 Tranquilizing Shot(s) cast, at least 1 expected");
    CHECK(Has(m, ScenarioFailKind::SupportMissing));
    CHECK_FALSE(m.passed(true));
    CHECK(std::string(ToString(ScenarioFailKind::SupportMissing)) == "support_missing");
}

TEST_CASE("ScenarioDefs: Magmadar needs a Tranquilizing Shot, Lucifron and Gehennas a curse removed", "[scenariomonitor]")
{
    ScenarioDef const* magmadar = FindScenario("mc_magmadar");
    ScenarioDef const* lucifron = FindScenario("mc_lucifron");
    ScenarioDef const* gehennas = FindScenario("mc_gehennas");
    REQUIRE(magmadar != nullptr);
    REQUIRE(lucifron != nullptr);
    REQUIRE(gehennas != nullptr);
    CHECK(magmadar->limits.min_tranq_shots == 1);
    CHECK(magmadar->limits.min_decurses == 0);
    CHECK(lucifron->limits.min_decurses == 1);
    CHECK(gehennas->limits.min_decurses == 1);
    CHECK(FindScenario("mc_shazzrah")->limits.min_decurses == 1);   // Shazzrah's Curse
    CHECK(FindScenario("mc_garr")->limits.min_tranq_shots == 0);
    // The raid has the classes that do the job: hunters for the shot, mages and druids for the curse.
    bool hunter = false, mage = false;
    for (ScenarioMember const& s : magmadar->squad)
    {
        hunter = hunter || s.cls == ScnClass::Hunter;
        mage = mage || s.cls == ScnClass::Mage;
    }
    CHECK(hunter);
    CHECK(mage);
}

TEST_CASE("ScenarioDefs: a Molten Core boss with no creature row carries the lair its script summons it at", "[scenariomonitor]")
{
    // Ragnaros has no spawn row in the world (Majordomo's script summons him at 838.3,-831.5,-232.2); the runner summons him there.
    ScenarioDef const* rag = FindScenario("mc_ragnaros");
    REQUIRE(rag != nullptr);
    CHECK(rag->goal_entry == 11502u);
    CHECK(rag->lair_z != 0.f);
    CHECK(std::fabs(rag->lair_x - 838.308f) < 0.01f);
    CHECK(std::fabs(rag->lair_y + 831.466f) < 0.01f);
    // The bosses the world spawns have none: they are found from their rows.
    for (char const* name : { "mc_garr", "mc_lucifron", "mc_magmadar", "mc_gehennas", "mc_geddon" })
    {
        ScenarioDef const* d = FindScenario(name);
        REQUIRE(d != nullptr);
        CHECK(d->lair_z == 0.f);
    }
}

TEST_CASE("ScenarioDefs: the Deadmines cove beach and water are inside the deadmines_full box", "[scenariomonitor]")
{
    for (char const* name : { "deadmines_full", "deadmines_player" })
    {
        ScenarioDef const* d = FindScenario(name);
        REQUIRE(d != nullptr);
        ScenarioMonitor m(d->limits);
        auto squad = Squad(-90.f, -707.f);
        for (ScenarioSample& s : squad)
            s.z = -0.04f;                               // 048360b9: the squad at the cove's water line, printed as z 0.0
        squad[1].x = -59.4f; squad[1].y = -701.2f; squad[1].z = 0.138f;   // route chain [37], the beach past the Iron Clad Door
        m.Feed(0, squad, true);
        CHECK_FALSE(Has(m, ScenarioFailKind::OutOfBounds));
        // Every boss staging spot is inside the box too.
        for (auto const& c : d->checkpoints)
        {
            ScenarioSample s = Bot(9, "Spot", ScenarioRoleKind::Dps, c.stage_x, c.stage_y);
            s.z = c.stage_z;
            ScenarioMonitor one(d->limits);
            one.Feed(0, { s }, true);
            CHECK_FALSE(Has(one, ScenarioFailKind::OutOfBounds));
        }
        // Falling through the map is still out of bounds.
        ScenarioMonitor fell(d->limits);
        squad[2].z = -200.f;
        fell.Feed(0, squad, true);
        CHECK(Has(fell, ScenarioFailKind::OutOfBounds, "Mage"));
    }
}

TEST_CASE("ScenarioMonitor: a non-healer idle more than 5 s in the group's fight fails group_idle, once per streak", "[scenariomonitor]")
{
    ScenarioMonitor m(Limits());
    for (std::uint32_t t = 0; t <= 10000; t += 1000)
    {
        auto squad = Squad(-100.f, -500.f);
        for (ScenarioSample& s : squad)
            s.in_combat = true;
        squad[3].group_idle_ms = t;          // the rogue has done nothing since the fight began
        squad[1].group_idle_ms = t;          // a healer with nobody to heal is not idle
        squad[2].group_idle_ms = t <= 5000 ? t : 0;   // the mage was idle 5 s, then cast
        m.Feed(t, squad, true);
    }
    CHECK(Has(m, ScenarioFailKind::GroupIdle, "Rogue"));
    CHECK_FALSE(Has(m, ScenarioFailKind::GroupIdle, "Healer"));
    CHECK_FALSE(Has(m, ScenarioFailKind::GroupIdle, "Mage"));
    std::size_t n = 0;
    for (ScenarioFailure const& f : m.failures())
        if (f.kind == ScenarioFailKind::GroupIdle)
        {
            ++n;
            CHECK(f.at_ms == 6000);
        }
    CHECK(n == 1);
}

TEST_CASE("ScenarioMonitor: water checks find a bot under the water, one that bobs, and one that does not swim", "[scenariomonitor]")
{
    ScenarioLimits l = Limits();
    l.water_under_ms = 15000;
    l.water_flips = 6;
    l.water_noswim_ms = 5000;

    SECTION("swimming bots pass")
    {
        ScenarioMonitor m(l);
        for (std::uint32_t t = 0; t <= 120000; t += 1000)
        {
            auto squad = Squad(-100.f + float(t) * 0.001f, -500.f);
            for (auto& s : squad) { s.deep_water = true; s.swim_flag = true; }
            m.Feed(t, squad, true);
        }
        REQUIRE_FALSE(Has(m, ScenarioFailKind::Water));
    }
    SECTION("head under water for 15 s")
    {
        ScenarioMonitor m(l);
        for (std::uint32_t t = 0; t <= 20000; t += 1000)
        {
            auto squad = Squad(-100.f + float(t) * 0.001f, -500.f);
            for (auto& s : squad) { s.deep_water = true; s.swim_flag = true; }
            squad[2].head_under = true;
            m.Feed(t, squad, true);
        }
        REQUIRE(Has(m, ScenarioFailKind::Water, "Mage"));
        REQUIRE_FALSE(Has(m, ScenarioFailKind::Water, "Tank"));
    }
    SECTION("under and back up again and again")
    {
        ScenarioMonitor m(l);
        for (std::uint32_t t = 0; t <= 30000; t += 1000)
        {
            auto squad = Squad(-100.f + float(t) * 0.001f, -500.f);
            for (auto& s : squad) { s.deep_water = true; s.swim_flag = true; }
            squad[3].head_under = (t / 3000) % 2 == 1;   // 3 s down, 3 s up
            m.Feed(t, squad, true);
        }
        REQUIRE(Has(m, ScenarioFailKind::Water, "Rogue"));
    }
    SECTION("in deep water without swimming")
    {
        ScenarioMonitor m(l);
        for (std::uint32_t t = 0; t <= 8000; t += 1000)
        {
            auto squad = Squad(-100.f + float(t) * 0.001f, -500.f);
            for (auto& s : squad) { s.deep_water = true; s.swim_flag = true; }
            squad[4].swim_flag = false;
            m.Feed(t, squad, true);
        }
        REQUIRE(Has(m, ScenarioFailKind::Water, "Hunter"));
    }
    SECTION("off unless the scenario asks")
    {
        ScenarioMonitor m(Limits());
        for (std::uint32_t t = 0; t <= 30000; t += 1000)
        {
            auto squad = Squad(-100.f + float(t) * 0.001f, -500.f);
            for (auto& s : squad) { s.deep_water = true; s.head_under = true; }
            m.Feed(t, squad, true);
        }
        REQUIRE_FALSE(Has(m, ScenarioFailKind::Water));
    }
}
