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

#include "../../src/modules/PlayerbotV2/Bot/Raid/RaidEncounters.h"
#include "../../src/modules/PlayerbotV2/Bot/Raid/DispelWorth.h"

using namespace Playerbot::Raid;

// MC Garr 2026-10-10: "aura 19496 on 40 members" drove 330 Cleanse / Dispel Magic casts in one fight and emptied every healer.
TEST_CASE("Garr's Magma Shackles is never dispelled", "[ClassicRaidDispel]")
{
    RaidRules const* mc = RulesForMap(409);
    REQUIRE(mc != nullptr);
    CHECK(OwnsAura(409, 19496));    // the generic dispel plan leaves it alone
    for (Dispel const& rule : mc->dispel)
        CHECK(rule.aura != 19496);  // and the raid brain hands out no order for it
    CHECK(std::find(mc->noDispel.begin(), mc->noDispel.end(), 19496u) != mc->noDispel.end());
}

TEST_CASE("Lucifron's curse and Impending Doom are still dispelled", "[ClassicRaidDispel]")
{
    RaidRules const* mc = RulesForMap(409);
    REQUIRE(mc != nullptr);
    auto has = [&](uint32_t aura) { return std::any_of(mc->dispel.begin(), mc->dispel.end(), [&](Dispel const& d) { return d.aura == aura; }); };
    CHECK(has(19703));
    CHECK(has(19702));
}

// MC Lucifron kill: Impending Doom drew up to 11 dispel orders a round for 27 rounds, and every healer paid a heal's mana for it.
TEST_CASE("Impending Doom hands out three dispel orders per plan", "[ClassicRaidDispel]")
{
    RaidRules const* mc = RulesForMap(409);
    REQUIRE(mc != nullptr);
    auto it = std::find_if(mc->dispel.begin(), mc->dispel.end(), [](Dispel const& d) { return d.aura == 19702; });
    REQUIRE(it != mc->dispel.end());
    CHECK(it->urgent);                      // still healers and tanks first, still cast in an emergency
    CHECK(it->maxOrders >= 2);
    CHECK(it->maxOrders <= 3);
    unsigned given = 0;
    while (Playerbot::Raid::DispelOrderAllowed(it->urgent, it->maxOrders, given))
        ++given;
    CHECK(given == it->maxOrders);
}

TEST_CASE("Dispel order caps: an own cap wins, urgent without one is uncapped, non-urgent keeps its default", "[ClassicRaidDispel]")
{
    using Playerbot::Raid::DispelOrderAllowed;
    using Playerbot::Raid::kNonUrgentDispelOrdersPerPlan;
    CHECK(DispelOrderAllowed(true, 0, 50));                                    // Lucifron's Curse: every carrier
    CHECK(DispelOrderAllowed(false, 0, kNonUrgentDispelOrdersPerPlan - 1));
    CHECK_FALSE(DispelOrderAllowed(false, 0, kNonUrgentDispelOrdersPerPlan));
    CHECK(DispelOrderAllowed(true, 3, 2));
    CHECK_FALSE(DispelOrderAllowed(true, 3, 3));
    CHECK_FALSE(DispelOrderAllowed(false, 1, 1));
}

TEST_CASE("Lucifron's Flamewaker Protectors are never crowd-controlled by the group", "[ClassicRaidDispel]")
{
    RaidRules const* mc = RulesForMap(409);
    REQUIRE(mc != nullptr);
    auto never = [&](uint32_t entry) { return std::any_of(mc->noControl.begin(), mc->noControl.end(), [&](NoControl const& n) { return n.enemy == entry; }); };
    CHECK(never(12119));
    CHECK_FALSE(never(12118));   // Lucifron himself is a boss, not a sheep target either way, and has no rule
    // the raid brain's own election must not hold them either
    for (CrowdControl const& c : mc->crowdControl)
        CHECK(c.enemy != 12119);
}

// ---- Garr: no Firesworn dies before him ----

TEST_CASE("Garr spares his Firesworn near death while he lives, and the 17 y run-away rule is gone", "[ClassicRaidDispel]")
{
    RaidRules const* mc = RulesForMap(409);
    REQUIRE(mc != nullptr);
    auto it = std::find_if(mc->spare.begin(), mc->spare.end(), [](Spare const& s) { return s.enemy == 12099; });
    REQUIRE(it != mc->spare.end());
    CHECK(it->gateEnemy == 12057);
    CHECK(it->belowPct >= 15.f);          // above the healers' 15 percent step-away: no add is already dying when it is spared
    CHECK(it->belowPct <= 40.f);          // a rule that spares half-health adds would leave the tanks nothing to do
    CHECK(SpareNow(*it, 25.f, true));     // at the line
    CHECK(SpareNow(*it, 3.f, true));
    CHECK_FALSE(SpareNow(*it, 26.f, true));
    CHECK_FALSE(SpareNow(*it, 3.f, false));   // Garr dead: the generic rules kill the adds
    // The old rule made every ranged and healer bot run from every live add (MC Garr 2026-10-10: a healer died doing it).
    for (AvoidEnemy const& a : mc->avoidEnemy)
        if (a.enemy == 12099)
        {
            CHECK(a.who == Who::Healer);  // the Movement table's one rule, for a dying add
            CHECK(a.hpMax <= 15.f);
        }
}

// ---- Majordomo: the Elites are never sheeped; the Healers are the owner's, by icon only ----

TEST_CASE("Majordomo's Elites are never crowd-controlled, the Healers only by the owner's icons", "[ClassicRaidDispel]")
{
    RaidRules const* mc = RulesForMap(409);
    REQUIRE(mc != nullptr);
    auto never = [&](uint32_t entry) { return std::any_of(mc->noControl.begin(), mc->noControl.end(), [&](NoControl const& n) { return n.enemy == entry; }); };
    CHECK_FALSE(never(11663));
    CHECK(never(11664));
    CHECK_FALSE(never(12018));
    for (CrowdControl const& c : mc->crowdControl)       // no automatic pick on any of them
        CHECK((c.enemy != 11663 && c.enemy != 11664));
}
