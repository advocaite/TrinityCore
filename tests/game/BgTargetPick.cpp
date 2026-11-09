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

#include "../../src/modules/PlayerbotV2/Bot/Battleground/BgTargetPick.h"

#include <vector>

using namespace Playerbot::BgTargetPick;

namespace
{
    struct G { uint64_t v = 0; bool operator==(G const& o) const { return v == o.v; } };
    struct U
    {
        G guid; int hp = 100; float x = 0.f, y = 0.f;
        bool untargetable = false, is_pacified = false, cannot_reach = false, is_player = false, is_cc_locked = false, in_los = true;
        G victim;
    };
}

TEST_CASE("BG victim validity: a gone, dead or idle victim is swapped for the nearest enemy player", "[battleground][target]")
{
    G const me{1};
    U player; player.guid = {2}; player.is_player = true; player.x = 5.f;
    U mob; mob.guid = {3}; mob.x = 3.f;
    std::vector<U> enemies, attackers;
    Why why;

    SECTION("a victim that is not around any more")
    {
        enemies = { player };
        CHECK(Pick<U, G>(me, G{9}, 0.f, 0.f, false, 40.f, enemies, attackers, why).v == 2);
        CHECK(why == Why::Missing);
        enemies.clear();
        CHECK(Pick<U, G>(me, G{9}, 0.f, 0.f, false, 40.f, enemies, attackers, why).v == 0);
        CHECK(why == Why::Missing);   // nobody to switch to: the caller releases the victim
    }
    SECTION("a dead victim")
    {
        U dead = mob; dead.hp = 0;
        enemies = { dead, player };
        CHECK(Pick<U, G>(me, G{3}, 0.f, 0.f, false, 40.f, enemies, attackers, why).v == 2);
        CHECK(why == Why::Unfightable);
    }
    SECTION("a creature that does not fight back while an enemy player is within 15 y")
    {
        enemies = { mob, player };
        CHECK(Pick<U, G>(me, G{3}, 0.f, 0.f, false, 40.f, enemies, attackers, why).v == 2);
        CHECK(why == Why::IdleMob);
        U fighting = mob; fighting.victim = me;
        enemies = { fighting, player };
        CHECK(Pick<U, G>(me, G{3}, 0.f, 0.f, false, 40.f, enemies, attackers, why).v == 0);
        CHECK(why == Why::None);
        U far = player; far.x = 30.f;
        enemies = { mob, far };
        CHECK(Pick<U, G>(me, G{3}, 0.f, 0.f, false, 40.f, enemies, attackers, why).v == 0);
        CHECK(why == Why::None);
    }
    SECTION("a healthy player victim, no victim, and players a ranged bot cannot see or that are crowd-controlled")
    {
        enemies = { player };
        CHECK(Pick<U, G>(me, G{2}, 0.f, 0.f, false, 40.f, enemies, attackers, why).v == 0);
        CHECK(why == Why::None);
        CHECK(Pick<U, G>(me, G{}, 0.f, 0.f, false, 40.f, enemies, attackers, why).v == 0);
        CHECK(why == Why::None);
        U hidden = player; hidden.in_los = false;
        enemies = { hidden };
        CHECK(Pick<U, G>(me, G{9}, 0.f, 0.f, true, 40.f, enemies, attackers, why).v == 0);
        U locked = player; locked.is_cc_locked = true;
        enemies = { locked };
        CHECK(Pick<U, G>(me, G{9}, 0.f, 0.f, false, 40.f, enemies, attackers, why).v == 0);
    }
}

TEST_CASE("BG open engage: a creature is opened on only when it fights us or is the order's target", "[battleground][target]")
{
    struct C { G guid; uint32_t entry = 0; G victim; };
    struct F { G guid; };
    G const me{1}, pet{5}, none{};
    std::vector<F> const friends = { { G{7} }, { G{8} } };
    C korrak; korrak.guid = {3}; korrak.entry = 12159;
    // AV 253: an idle Korrak beside the road is left alone
    CHECK(!CreatureWorthOpening(korrak, me, pet, friends, 0u));
    // ... unless he fights this bot, its pet or a teammate
    korrak.victim = me;
    CHECK(CreatureWorthOpening(korrak, me, pet, friends, 0u));
    korrak.victim = pet;
    CHECK(CreatureWorthOpening(korrak, me, pet, friends, 0u));
    CHECK(!CreatureWorthOpening(korrak, me, none, friends, 0u));
    korrak.victim = G{8};
    CHECK(CreatureWorthOpening(korrak, me, pet, friends, 0u));
    // fighting someone else (an enemy player): not our fight
    korrak.victim = G{42};
    CHECK(!CreatureWorthOpening(korrak, me, pet, friends, 0u));
    // the order's own target (a captain, a mine boss) is always fair game
    C balinda; balinda.guid = {4}; balinda.entry = 11949;
    CHECK(CreatureWorthOpening(balinda, me, pet, friends, 11949u));
    CHECK(!CreatureWorthOpening(balinda, me, pet, friends, 11947u));
}

TEST_CASE("BG push march: a creature hitting the bot in melee is fought, not walked away from", "[battleground][target]")
{
    G const me{1};
    U digger; digger.guid = {3}; digger.x = 4.f; digger.victim = me;
    std::vector<U> attackers;
    // nothing on us: march on
    CHECK(BesetBy<U, G>(me, 0.f, 0.f, attackers) == nullptr);
    // AV 255, Coldtooth Mine: a Whitewhisker Digger 4 yd away hitting the bot
    attackers = { digger };
    REQUIRE(BesetBy<U, G>(me, 0.f, 0.f, attackers) != nullptr);
    CHECK(BesetBy<U, G>(me, 0.f, 0.f, attackers)->guid.v == 3);
    // the nearest of two
    U vermin = digger; vermin.guid = {4}; vermin.x = 2.f;
    attackers = { digger, vermin };
    CHECK(BesetBy<U, G>(me, 0.f, 0.f, attackers)->guid.v == 4);
    // not when it hits someone else, is out of melee, dead, untargetable, unreachable, or is a player
    U other = digger; other.victim = G{9};
    U far = digger; far.x = 25.f;
    U dead = digger; dead.hp = 0;
    U ghost = digger; ghost.untargetable = true;
    U stuck = digger; stuck.cannot_reach = true;
    U player = digger; player.is_player = true;
    attackers = { other, far, dead, ghost, stuck, player };
    CHECK(BesetBy<U, G>(me, 0.f, 0.f, attackers) == nullptr);
}

TEST_CASE("BG push march: an enemy player shooting the bot is fought, not walked away from", "[battleground][target]")
{
    G const me{1};
    U hunter; hunter.guid = {3}; hunter.is_player = true; hunter.x = 32.f; hunter.victim = me;
    std::vector<U> attackers;
    CHECK(ShotBy<U, G>(me, 0.f, 0.f, attackers) == nullptr);
    // AV 264, river road under Iceblood: a Horde hunter's Auto Shot from 32 yd
    attackers = { hunter };
    REQUIRE(ShotBy<U, G>(me, 0.f, 0.f, attackers) != nullptr);
    CHECK(ShotBy<U, G>(me, 0.f, 0.f, attackers)->guid.v == 3);
    // the nearest of two
    U rogue = hunter; rogue.guid = {4}; rogue.x = 3.f;
    attackers = { hunter, rogue };
    CHECK(ShotBy<U, G>(me, 0.f, 0.f, attackers)->guid.v == 4);
    // not when it shoots someone else, is past 40 yd, dead, untargetable, held by our CC, or is a creature
    U other = hunter; other.victim = G{9};
    U far = hunter; far.x = 45.f;
    U dead = hunter; dead.hp = 0;
    U ghost = hunter; ghost.untargetable = true;
    U sheeped = hunter; sheeped.is_cc_locked = true;
    U mob = hunter; mob.is_player = false;
    attackers = { other, far, dead, ghost, sheeped, mob };
    CHECK(ShotBy<U, G>(me, 0.f, 0.f, attackers) == nullptr);
}
