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

#include "../../src/modules/PlayerbotV2/Fleet/StrangerBoard.h"

using namespace Playerbot::V2::Strangers;

namespace
{
Stranger At(std::uint64_t guid, float x, float y)
{
    Stranger p;
    p.guid = guid;
    p.x = x;
    p.y = y;
    return p;
}
}

TEST_CASE("A buff goes to the nearest player who lacks it", "[ClassicStrangerBoard]")
{
    Stranger far = At(1, 20.f, 0.f);
    Stranger near = At(2, 5.f, 0.f);
    near.buffs = kFortitude;
    Stranger warrior = At(3, 10.f, 0.f);
    std::vector<Stranger> list{ far, near, warrior };
    REQUIRE(PickBuffTarget(list, 0, true, 0, 0.f, 0.f, 28.f, kFortitude)->guid == 3);
    // Intellect only for a mana user.
    REQUIRE(PickBuffTarget(list, 0, true, 0, 0.f, 0.f, 28.f, kIntellect) == nullptr);
    list[0].has_mana = true;
    REQUIRE(PickBuffTarget(list, 0, true, 0, 0.f, 0.f, 28.f, kIntellect)->guid == 1);
    // The other faction, another map, out of range, in a fight: nobody.
    REQUIRE(PickBuffTarget(list, 0, false, 0, 0.f, 0.f, 28.f, kMark) == nullptr);
    REQUIRE(PickBuffTarget(list, 1, true, 0, 0.f, 0.f, 28.f, kMark) == nullptr);
    REQUIRE(PickBuffTarget(list, 0, true, 0, 100.f, 0.f, 28.f, kMark) == nullptr);
    for (Stranger& p : list) p.attacker = 9;
    REQUIRE(PickBuffTarget(list, 0, true, 0, 0.f, 0.f, 28.f, kMark) == nullptr);
}

TEST_CASE("Help goes to the player losing a fight the bot can take", "[ClassicStrangerBoard]")
{
    Stranger a = At(1, 5.f, 0.f);
    a.attacker = 77; a.attacker_level = 20; a.hp_pct = 30;
    Stranger b = At(2, 6.f, 0.f);
    b.attacker = 78; b.attacker_level = 20; b.hp_pct = 70;
    std::vector<Stranger> list{ a, b };
    REQUIRE(PickHelpTarget(list, 0, true, 0, 0.f, 0.f, 35.f, 40, 18, 3)->guid == 1);
    REQUIRE(PickHelpTarget(list, 0, true, 0, 0.f, 0.f, 35.f, 40, 16, 3) == nullptr);   // mob too high for the bot
    REQUIRE(PickHelpTarget(list, 0, true, 0, 0.f, 0.f, 35.f, 20, 20, 3) == nullptr);   // not low enough yet
}

TEST_CASE("Only a dead, unreleased player is resurrected; one kindness per cooldown", "[ClassicStrangerBoard]")
{
    Stranger dead = At(1, 5.f, 0.f);
    dead.alive = false;
    Stranger ghost = At(2, 5.f, 0.f);
    ghost.alive = false; ghost.ghost = true;
    REQUIRE(PickRezTarget({ ghost }, 0, true, 0, 0.f, 0.f, 28.f) == nullptr);
    REQUIRE(PickRezTarget({ ghost, dead }, 0, true, 0, 0.f, 0.f, 28.f)->guid == 1);
    // In the bot's own group: its group rules do that.
    dead.group = 5;
    REQUIRE(PickRezTarget({ dead }, 0, true, 5, 0.f, 0.f, 28.f) == nullptr);

    Cooldowns cd;
    REQUIRE(cd.Take(1, Kind::Buff, 1000, 60000));
    REQUIRE_FALSE(cd.Take(1, Kind::Buff, 30000, 60000));
    REQUIRE(cd.Take(1, Kind::Help, 30000, 60000));
    REQUIRE(cd.Take(1, Kind::Buff, 62000, 60000));
}

TEST_CASE("A heal goes to the most hurt player, never into a fight with players", "[ClassicStrangerBoard]")
{
    Stranger scratched = At(1, 5.f, 0.f);
    scratched.hp_pct = 80;
    Stranger hurt = At(2, 10.f, 0.f);
    hurt.hp_pct = 50;
    Stranger worse = At(3, 20.f, 0.f);
    worse.hp_pct = 30; worse.attacker = 77;   // losing to a mob: that is when a heal helps
    std::vector<Stranger> list{ scratched, hurt, worse };
    REQUIRE(PickHealTarget(list, 0, true, 0, 0.f, 0.f, 35.f, 60, false)->guid == 3);
    REQUIRE(PickHealTarget(list, 0, true, 0, 0.f, 0.f, 15.f, 60, false)->guid == 2);   // out of reach
    REQUIRE(PickHealTarget({ scratched }, 0, true, 0, 0.f, 0.f, 35.f, 60, false) == nullptr);   // not hurt enough
    REQUIRE(PickHealTarget(list, 0, false, 0, 0.f, 0.f, 35.f, 60, false) == nullptr);           // the other faction
    // A player fighting a player or in a duel: nobody steps in.
    list[2].pvp_fight = true;
    REQUIRE(PickHealTarget(list, 0, true, 0, 0.f, 0.f, 35.f, 60, false)->guid == 2);
    // A PvP-flagged player only from a healer that is flagged itself.
    list[1].pvp_flagged = true;
    REQUIRE(PickHealTarget(list, 0, true, 0, 0.f, 0.f, 35.f, 60, false) == nullptr);
    REQUIRE(PickHealTarget(list, 0, true, 0, 0.f, 0.f, 35.f, 60, true)->guid == 2);
    // Dead or released: the rez rule, not a heal.
    Stranger dead = At(4, 5.f, 0.f);
    dead.alive = false; dead.hp_pct = 0;
    REQUIRE(PickHealTarget({ dead }, 0, true, 0, 0.f, 0.f, 35.f, 60, false) == nullptr);

    Cooldowns cd;
    REQUIRE(cd.Take(2, Kind::Heal, 1000, 45000));
    REQUIRE_FALSE(cd.Take(2, Kind::Heal, 20000, 45000));
    REQUIRE(cd.Take(2, Kind::Rez, 20000, 45000));   // its own key, not the rez's
}
