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

// The generated dungeon tactics tables live in the bot module's library; the test runs when that library is linked.
#ifdef TRINITY_PLAYERBOT_V2

#include "../../src/modules/PlayerbotV2/Bot/Dungeon/Tactics/DungeonTactics.h"
#include "../../src/modules/PlayerbotV2/Bot/Dungeon/DungeonScript.h"

#include <algorithm>
#include <set>

using namespace Playerbot::Tactics;

namespace {
constexpr uint32_t kMaps[] = { 36, 389, 43, 33, 48, 34, 189, 47, 90, 129, 70, 209, 230, 409 };   // batches 1-3, then Molten Core trash

bool Contains(std::vector<uint32_t> const& v, uint32_t x) { return std::find(v.begin(), v.end(), x) != v.end(); }
}

TEST_CASE("Every generated table is well formed", "[ClassicDungeonTactics]")
{
    for (uint32_t map : kMaps)
    {
        MapTactics const* t = TacticsForMap(map);
        REQUIRE(t != nullptr);
        CHECK(t->mob_count > 0);
        uint32_t prev = 0;
        for (size_t i = 0; i < t->mob_count; ++i)
        {
            MobTactic const& m = t->mobs[i];
            CAPTURE(map, m.entry, m.name);
            CHECK(m.entry > prev);                       // sorted and unique: FindMob binary-searches
            prev = m.entry;
            CHECK((m.spell_count == 0) == (m.spells == nullptr));
            CHECK((m.summon_count == 0) == (m.summons == nullptr));
            CHECK(m.level_min <= m.level_max);
            if (m.flags & Mob::CcPriority)
                CHECK(m.cc != Cc::None);
            for (uint8_t k = 0; k < m.spell_count; ++k)
            {
                SpellTactic const& s = m.spells[k];
                CAPTURE(s.spell, s.name);
                CHECK(s.hp_min <= s.hp_max);
                CHECK(s.first_min_ms <= s.first_max_ms);
                CHECK(s.repeat_min_ms <= s.repeat_max_ms);
                CHECK(s.severity <= 3);
                if (s.counters & Ctr::Interrupt)
                    CHECK(s.cast_ms != 0);               // an instant cast cannot be interrupted
                if (s.counters & Ctr::Purge)
                    CHECK(s.dispel == Disp::Magic);
                if (s.holds_schools)
                    CHECK((s.tags & (Tag::Shield | Tag::Immune)) != 0);
            }
        }
    }
}

TEST_CASE("Deadmines: the Evoker's Flamestrike is interrupted, stepped out of, and the Evoker is a priority target", "[ClassicDungeonTactics]")
{
    MobTactic const* evoker = FindMob(36, 1729);
    REQUIRE(evoker != nullptr);
    CHECK(evoker->priority >= 4);
    CHECK((evoker->cc & Cc::Polymorph) != 0);
    SpellTactic const* flamestrike = nullptr;
    for (uint8_t k = 0; k < evoker->spell_count; ++k)
        if (evoker->spells[k].spell == 11829)
            flamestrike = &evoker->spells[k];
    REQUIRE(flamestrike != nullptr);
    CHECK((flamestrike->counters & Ctr::Interrupt) != 0);
    CHECK((flamestrike->counters & Ctr::StepOut) != 0);
    CHECK((flamestrike->tags & Tag::Ground) != 0);
    CHECK(flamestrike->amount == 82);                    // the vanilla mean of 75 + 1d13, not the client's 479
}

TEST_CASE("Deadmines: advice lists", "[ClassicDungeonTactics]")
{
    MapTactics const* t = TacticsForMap(36);
    REQUIRE(t != nullptr);
    AdviceLists const l = DeriveAdvice(*t);
    CHECK(Contains(l.kill_order, 1729));                 // Defias Evoker
    CHECK(Contains(l.kill_order, 4418));                 // Defias Wizard
    CHECK(Contains(l.mandatory_interrupts, 11829));      // Flamestrike
    CHECK(Contains(l.mandatory_interrupts, 9053));       // Fireball
    CHECK(Contains(l.cc_entries, 4418));
    CHECK_FALSE(Contains(l.kill_order, 644));            // a boss is never in the trash kill order

    // highest priority first
    int last = 127;
    for (uint32_t e : l.kill_order)
    {
        MobTactic const* m = FindMob(36, e);
        REQUIRE(m != nullptr);
        CHECK(m->priority <= last);
        last = m->priority;
    }

    Playerbot::DungeonAdvice a;
    AppendAdvice(36, a);
    CHECK(Contains(a.high_priority_kill_entries, 1729));
    CHECK(Contains(a.mandatory_interrupt_spells, 11829));
    CHECK(a.bosses.empty());                             // the tables do not own the boss order
}

TEST_CASE("Healers, fears and summoners are the first kills", "[ClassicDungeonTactics]")
{
    MobTactic const* wolfguard = FindMob(33, 3854);      // Shadowfang Wolfguard heals its pack
    REQUIRE(wolfguard != nullptr);
    CHECK((wolfguard->flags & Mob::Healer) != 0);
    CHECK(wolfguard->priority == 5);
    MobTactic const* acolyte = FindMob(48, 4809);        // Twilight Acolyte (Renew)
    REQUIRE(acolyte != nullptr);
    CHECK(acolyte->priority >= 2);
}

TEST_CASE("Immune and absorb auras name the school the party holds back", "[ClassicDungeonTactics]")
{
    MobTactic const* springvale = FindMob(33, 4278);     // Commander Springvale: Divine Protection makes him immune to physical
    REQUIRE(springvale != nullptr);
    bool found = false;
    for (uint8_t k = 0; k < springvale->spell_count; ++k)
        if (springvale->spells[k].spell == 498)
        {
            found = true;
            CHECK(springvale->spells[k].holds_schools == 1);
            CHECK((springvale->spells[k].counters & Ctr::StopDamage) != 0);
        }
    CHECK(found);
}

TEST_CASE("A mob the table does not list is not found", "[ClassicDungeonTactics]")
{
    CHECK(FindMob(36, 1) == nullptr);
    CHECK(FindMob(9999, 1729) == nullptr);
    CHECK(TacticsForMap(9999) == nullptr);
}

#endif // TRINITY_PLAYERBOT_V2
