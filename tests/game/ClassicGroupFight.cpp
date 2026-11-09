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

#include "../../src/modules/PlayerbotV2/Bot/GroupFight.h"

using namespace Playerbot::GroupFight;

namespace
{
Inputs Fighting()
{
    Inputs in;
    in.led_by_human = true;
    in.self_alive = true;
    in.leader_alive = true;
    in.leader_in_combat = true;
    in.fight_age_ms = 5000;
    return in;
}

Candidate Mob(float dist)
{
    Candidate c;
    c.dist = dist;
    c.on_member = true;
    return c;
}
}

TEST_CASE("GroupFight: anyone in the group fighting puts every bot in the fight", "[groupfight]")
{
    CHECK(Decide(Fighting()) == Mode::Fight);
    // A caster who lets his tank pull is not in combat himself: the tank by him is (UBRS 2026-10-28).
    Inputs in = Fighting();
    in.leader_in_combat = false;
    in.member_fighting_near_leader = true;
    CHECK(Decide(in) == Mode::Fight);
    // Nobody fights: the old rules (Passive, follow, Stay) decide.
    in.member_fighting_near_leader = false;
    CHECK(Decide(in) == Mode::Off);
}

TEST_CASE("GroupFight: the leader's combat flag with nothing near him ends the fight after a few seconds", "[groupfight]")
{
    // KailON 0.1.27.2: a Southsea Privateer fled 94 y and kept him flagged; his bots stood still for a minute.
    Inputs in = Fighting();
    in.leader_fight_near = false;
    in.leader_flag_only_ms = 0;
    CHECK(Decide(in) == Mode::Fight);                       // just started: the mob may still be on its way
    in.leader_flag_only_ms = kLeaderFlagOnlyMs - 1;
    CHECK(Decide(in) == Mode::Fight);
    in.leader_flag_only_ms = kLeaderFlagOnlyMs;
    CHECK(Decide(in) == Mode::Off);                         // nothing near him: the bots follow again
    in.leader_fight_near = true;
    CHECK(Decide(in) == Mode::Fight);                       // the mob comes back: they fight
    in.leader_fight_near = false;
    in.member_fighting_near_leader = true;
    CHECK(Decide(in) == Mode::Fight);                       // a member near him still fights
}

TEST_CASE("GroupFight: off without a player leader, in a battleground, on a raid boss, or dead", "[groupfight]")
{
    Inputs in = Fighting();
    in.led_by_human = false;
    CHECK(Decide(in) == Mode::Off);
    in = Fighting();
    in.battleground = true;
    CHECK(Decide(in) == Mode::Off);
    in = Fighting();
    in.raid_map = true;    // the caller sets it for a raid boss fight; raid trash keeps the gate
    CHECK(Decide(in) == Mode::Off);
    in = Fighting();
    in.self_alive = false;
    CHECK(Decide(in) == Mode::Off);
    in = Fighting();
    in.leader_alive = false;
    CHECK(Decide(in) == Mode::Off);
}

TEST_CASE("GroupFight: a fresh leader order beats the fight, an old Stay does not", "[groupfight]")
{
    Inputs in = Fighting();
    in.order = Order::Stop;
    in.order_age_ms = 2000;
    CHECK(Decide(in) == Mode::Ordered);
    in.order_age_ms = kOrderWinsMs + 1;
    CHECK(Decide(in) == Mode::Fight);

    // "use altar" at 22:16:06 left a Stay; the fight began minutes later: the bot fights.
    in = Fighting();
    in.order = Order::Stay;
    in.order_age_ms = 120000;
    in.fight_age_ms = 4000;
    CHECK(Decide(in) == Mode::Fight);
    // A Stay given during this fight holds, however long ago in it.
    in.order_age_ms = 15000;
    in.fight_age_ms = 20000;
    CHECK(Decide(in) == Mode::Ordered);
    // ...but never keeps a bot from answering a mob that hits it.
    in.self_in_combat = true;
    CHECK(Decide(in) == Mode::Fight);

    // Flee and Pull back hold while they run, whatever is hitting the bot.
    in = Fighting();
    in.order = Order::Flee;
    in.order_age_ms = 0;
    in.self_in_combat = true;
    CHECK(Decide(in) == Mode::Ordered);
}

TEST_CASE("GroupFight: after the leader's Pull back the rest hold until his Attack, at most kOrderWinsMs", "[groupfight]")
{
    // rfc_oggleflint_led: Pull back at 1000, the tank's opener puts the group in the fight; no Attack yet: the hunter holds.
    CHECK(PullWaitPending(1000, 0, 0, 3000));
    Inputs in = Fighting();
    in.leader_in_combat = false;
    in.member_fighting_near_leader = true;
    in.order = Order::PullBack;
    in.order_age_ms = 3000 - 1000;
    CHECK(Decide(in) == Mode::Ordered);
    // A mob on the bot does not lift it: the old rules answer it (self-defence in EvaluateThreatHold).
    in.self_in_combat = true;
    CHECK(Decide(in) == Mode::Ordered);
    // The tank still bringing it back after kOrderWinsMs: the mob is at the group by then, everyone fights.
    in.order_age_ms = kOrderWinsMs + 1;
    CHECK(Decide(in) == Mode::Fight);
    // The leader's Attack answers it; an Attack from before the Pull back does not; never a Pull back: nothing pending.
    CHECK_FALSE(PullWaitPending(1000, 1500, 0, 3000));
    CHECK(PullWaitPending(1000, 900, 0, 3000));
    CHECK_FALSE(PullWaitPending(0, 0, 0, 3000));
    // Never answered: it lapses after kOrderWinsMs, so a Follow 12 s after the Pull back is the order that counts.
    CHECK(PullWaitPending(1000, 0, 0, 1000 + kOrderWinsMs));
    CHECK_FALSE(PullWaitPending(1000, 0, 0, 1000 + 12000));
    // A later order (Follow, Stay) given before the 10 s are up wins at once; one given before the Pull back does not.
    CHECK_FALSE(PullWaitPending(1000, 0, 4000, 5000));
    CHECK(PullWaitPending(1000, 0, 800, 5000));
}

TEST_CASE("GroupFight: damage dealers take the leader's order, the priority add, the tank's target, then the leader's", "[groupfight]")
{
    std::vector<Candidate> c(5, Mob(10.f));
    c[0].dist = 2.f;                 // nearest: a mob on a member
    c[1].leader_target = true;
    c[2].tank_target = true;
    c[3].priority_add = true;
    c[4].leader_ordered = true;
    CHECK(PickTarget(c) == 4);
    c[4].leader_ordered = false;
    CHECK(PickTarget(c) == 3);
    c[3].priority_add = false;
    CHECK(PickTarget(c) == 2);
    c[2].tank_target = false;
    CHECK(PickTarget(c) == 1);
    c[1].leader_target = false;
    CHECK(PickTarget(c) == 0);
}

TEST_CASE("GroupFight: never a crowd-controlled or forbidden mob; a mob on the bot only when nothing else", "[groupfight]")
{
    std::vector<Candidate> c(3, Mob(10.f));
    c[0].tank_target = true;
    c[0].crowd_controlled = true;    // the mage's sheep
    c[1].leader_target = true;
    c[1].forbidden = true;
    c[2].on_member = false;
    c[2].on_me = true;
    CHECK(PickTarget(c) == 2);
    c[2].alive = false;
    CHECK(PickTarget(c) == kNone);
}

TEST_CASE("GroupFight: the group's own hold is fought once it is the last mob of the fight", "[groupfight]")
{
    // wc_cobrahn_led RC10: the Ectoplasm dead, only the mage's sheeped Deviate Guardian left; the damage dealers stood 6 s.
    std::vector<Candidate> c(2, Mob(10.f));
    c[0].crowd_controlled = true;
    c[0].held_by_group = true;
    c[0].dist = 15.f;
    c[1].tank_target = true;
    CHECK(PickTarget(c) == 1);           // something else to fight: the sheep stays
    c[1].alive = false;
    CHECK(PickTarget(c) == 0);           // the last mob: everyone fights it
    c[1].alive = true;
    c[1].crowd_controlled = true;        // a second hold, nearer, but someone outside the group's
    c[1].dist = 5.f;
    CHECK(PickTarget(c) == kNone);
    c[1].held_by_group = true;           // both the group's: the nearest
    CHECK(PickTarget(c) == 1);
    std::vector<TankCandidate> t(1);
    t[0].mob = c[0];
    CHECK(PickTankTarget(t) == 0);       // the tank too
}

TEST_CASE("GroupFight: a tank takes a mob off a non-tank first, nearest first", "[groupfight]")
{
    // UBRS 2026-10-28 22:16: six Incarcerators loose on the mages while both tanks waited for an assignment.
    std::vector<TankCandidate> c(3);
    c[0].mob = Mob(5.f);
    c[0].mob.leader_target = true;
    c[1].mob = Mob(20.f);
    c[1].on_non_tank = true;
    c[2].mob = Mob(12.f);
    c[2].on_non_tank = true;
    CHECK(PickTankTarget(c) == 2);
    c[2].mob.crowd_controlled = true;
    CHECK(PickTankTarget(c) == 1);
    // Nothing loose: the leader's target.
    c[1].on_non_tank = false;
    c[2].on_non_tank = false;
    CHECK(PickTankTarget(c) == 0);
}

TEST_CASE("GroupFight: idle in the fight is a fault after 3 s, not for healers or controlled bots", "[groupfight]")
{
    CHECK_FALSE(IdleFault(Mode::Fight, false, false, 1000, 1000 + kIdleFaultMs));
    CHECK(IdleFault(Mode::Fight, false, false, 1000, 1001 + kIdleFaultMs));
    CHECK_FALSE(IdleFault(Mode::Fight, true, false, 1000, 10000));
    CHECK_FALSE(IdleFault(Mode::Fight, false, true, 1000, 10000));
    CHECK_FALSE(IdleFault(Mode::Ordered, false, false, 1000, 10000));
    CHECK_FALSE(IdleFault(Mode::Off, false, false, 1000, 10000));
    CHECK_FALSE(IdleFault(Mode::Fight, false, false, 0, 10000));
}
