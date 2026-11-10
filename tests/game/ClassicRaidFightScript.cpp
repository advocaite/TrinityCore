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

#include "../../src/modules/PlayerbotV2/Bot/Raid/KickRules.h"
#include "../../src/modules/PlayerbotV2/Bot/Raid/RaidEncounters.h"
#include "../../src/modules/PlayerbotV2/Bot/Raid/RaidFightScript.h"
#include "../../src/modules/PlayerbotV2/Bot/Raid/ShieldRules.h"
#include "../../src/modules/PlayerbotV2/Bot/Raid/ZgHakkar.h"

#include <cmath>

using namespace Playerbot::Raid::Script;

namespace
{
constexpr uint32_t kMc = 409, kLucifron = 12118, kProtector = 12119;

Fight const& Luc()
{
    Fight const* f = FightFor(kMc, kLucifron);
    REQUIRE(f != nullptr);
    return *f;
}

Enemy E(uint64_t guid, uint32_t entry, float hp, bool eligible = true)
{
    Enemy e;
    e.guid = guid;
    e.entry = entry;
    e.hpPct = hp;
    e.eligible = eligible;
    return e;
}
}

TEST_CASE("Lucifron's script covers Lucifron and his Protectors on Molten Core only", "[ClassicRaidFightScript]")
{
    CHECK(FightFor(kMc, kLucifron) != nullptr);
    CHECK(FightFor(kMc, kProtector) == FightFor(kMc, kLucifron));
    CHECK(FightFor(kMc, 11982) == nullptr);      // Magmadar has none yet
    CHECK(FightFor(469, kLucifron) == nullptr);  // another map
}

TEST_CASE("The Protectors die before Lucifron, one at a time", "[ClassicRaidFightScript]")
{
    Enemy const all[] = { E(1, kLucifron, 100.f), E(2, kProtector, 100.f), E(3, kProtector, 100.f) };
    // Equal health: the lower guid, then the hurt one wins.
    CHECK(PickFocus(Luc(), all, 3, 0, 0) == 2);
    Enemy const hurt[] = { E(1, kLucifron, 100.f), E(2, kProtector, 100.f), E(3, kProtector, 80.f) };
    CHECK(PickFocus(Luc(), hurt, 3, 0, 0) == 3);
    // Both Protectors dead (or not eligible): Lucifron.
    Enemy const last[] = { E(1, kLucifron, 100.f), E(2, kProtector, 0.f, false), E(3, kProtector, 0.f, false) };
    CHECK(PickFocus(Luc(), last, 3, 0, 0) == 1);
}

TEST_CASE("The pick does not flip between two Protectors", "[ClassicRaidFightScript]")
{
    // Guid 2 was the pick; guid 3 is now slightly more hurt. The raid stays on 2 until it dies.
    Enemy const e[] = { E(1, kLucifron, 100.f), E(2, kProtector, 60.f), E(3, kProtector, 59.f) };
    CHECK(PickFocus(Luc(), e, 3, 0, 2) == 2);
    // The previous pick is dead: the other Protector.
    Enemy const dead[] = { E(1, kLucifron, 100.f), E(2, kProtector, 0.f, false), E(3, kProtector, 59.f) };
    CHECK(PickFocus(Luc(), dead, 3, 0, 2) == 3);
    // A previous pick of a later rank is dropped for an earlier one (Lucifron picked while no Protector was eligible).
    Enemy const back[] = { E(1, kLucifron, 100.f), E(2, kProtector, 100.f) };
    CHECK(PickFocus(Luc(), back, 2, 0, 1) == 2);
}

TEST_CASE("The owner's skull wins over the script's order", "[ClassicRaidFightScript]")
{
    Enemy const e[] = { E(1, kLucifron, 100.f), E(2, kProtector, 100.f), E(3, kProtector, 100.f) };
    CHECK(PickFocus(Luc(), e, 3, 1, 0) == 1);
    // A mark on an enemy that cannot be attacked (controlled, dead) is ignored.
    Enemy const shut[] = { E(1, kLucifron, 100.f, false), E(2, kProtector, 100.f), E(3, kProtector, 100.f) };
    CHECK(PickFocus(Luc(), shut, 3, 1, 0) == 2);
}

TEST_CASE("Nothing eligible or nothing on the list names no target", "[ClassicRaidFightScript]")
{
    Enemy const none[] = { E(1, kLucifron, 100.f, false), E(2, kProtector, 100.f, false) };
    CHECK(PickFocus(Luc(), none, 2, 0, 0) == 0);
    Enemy const stray[] = { E(9, 11671, 100.f) };   // a Core Hound is not part of the fight
    CHECK(PickFocus(Luc(), stray, 1, 0, 0) == 0);
    CHECK(PickFocus(Luc(), nullptr, 0, 0, 0) == 0);
}

TEST_CASE("The DPS start wait is bounded and starts at the first engaged enemy", "[ClassicRaidFightScript]")
{
    uint32_t const wait = Luc().startWaitMs;
    CHECK(wait == 8000u);                              // the raid's one boss opening wait: the tank takes a while to run in
    CHECK(wait == Playerbot::kRaidBossOpenWaitMs);
    CHECK_FALSE(StartWait(0, 10000, wait));            // nothing engaged yet: no clock
    CHECK(StartWait(10000, 10000, wait));
    CHECK(StartWait(10000, 17999, wait));
    CHECK_FALSE(StartWait(10000, 18000, wait));        // over: no threat hold afterwards, ever
    CHECK_FALSE(StartWait(10000, 9000, wait));         // a clock that ran backwards does not freeze the raid
}

// ---- Garr ----

namespace
{
constexpr uint32_t kGarr = 12057, kFiresworn = 12099;

Fight const& GarrFight()
{
    Fight const* f = FightFor(kMc, kGarr);
    REQUIRE(f != nullptr);
    return *f;
}
}

TEST_CASE("Garr's script covers Garr alone: the Firesworn are not its enemies, so their Banish stays", "[ClassicRaidFightScript]")
{
    CHECK(FightFor(kMc, kGarr) != nullptr);
    CHECK(FightFor(kMc, kFiresworn) == nullptr);        // the script drops crowd control on its own enemies only
    CHECK(FightFor(469, kGarr) == nullptr);             // another map
    CHECK(FightFor(kMc, kGarr) != FightFor(kMc, kLucifron));
    CHECK(GarrFight().startWaitMs == Playerbot::kRaidBossOpenWaitMs);
}

TEST_CASE("Every DPS bot hits Garr, whatever the Firesworn do", "[ClassicRaidFightScript]")
{
    Enemy const all[] = { E(1, kGarr, 100.f), E(2, kFiresworn, 30.f), E(3, kFiresworn, 100.f) };
    CHECK(PickFocus(GarrFight(), all, 3, 0, 0) == 1);   // the Firesworn rank behind everything: not on the list
    Enemy const noGarr[] = { E(1, kGarr, 100.f, false), E(2, kFiresworn, 30.f) };
    CHECK(PickFocus(GarrFight(), noGarr, 2, 0, 0) == 0); // Garr down or out of reach: the generic rules pick
    CHECK(PickFocus(GarrFight(), all, 3, 2, 0) == 2);    // the owner's skull still wins
}

TEST_CASE("Baron Geddon has the one-boss script: eight second start wait, DPS on him", "[ClassicRaidFightScript]")
{
    constexpr uint32_t kGeddon = 12056;
    Fight const* f = FightFor(kMc, kGeddon);
    REQUIRE(f != nullptr);
    CHECK(f->startWaitMs == Playerbot::kRaidBossOpenWaitMs);
    CHECK(FightFor(469, kGeddon) == nullptr);
    CHECK(f != FightFor(kMc, kGarr));
    Enemy const e[] = { E(1, kGeddon, 80.f) };
    CHECK(PickFocus(*f, e, 1, 0, 0) == 1);
    Enemy const out[] = { E(1, kGeddon, 80.f, false) };
    CHECK(PickFocus(*f, out, 1, 0, 0) == 0);
}

TEST_CASE("Shazzrah has the one-boss script", "[ClassicRaidFightScript]")
{
    constexpr uint32_t kShazzrah = 12264;
    Fight const* f = FightFor(kMc, kShazzrah);
    REQUIRE(f != nullptr);
    CHECK(f->startWaitMs == Playerbot::kRaidBossOpenWaitMs);
    Enemy const e[] = { E(1, kShazzrah, 100.f) };
    CHECK(PickFocus(*f, e, 1, 0, 0) == 1);
    CHECK(f != FightFor(kMc, 12056));
}

TEST_CASE("Sulfuron's priests die before him, one at a time, the same one for everybody", "[ClassicRaidFightScript]")
{
    constexpr uint32_t kSulfuron = 12098, kPriest = 11662;
    Fight const* f = FightFor(kMc, kSulfuron);
    REQUIRE(f != nullptr);
    CHECK(FightFor(kMc, kPriest) == f);
    CHECK(f->startWaitMs == Playerbot::kRaidBossOpenWaitMs);
    Enemy const all[] = { E(1, kSulfuron, 100.f), E(2, kPriest, 100.f), E(3, kPriest, 70.f), E(4, kPriest, 100.f), E(5, kPriest, 100.f) };
    CHECK(PickFocus(*f, all, 5, 0, 0) == 3);             // the most hurt priest first
    CHECK(PickFocus(*f, all, 5, 0, 3) == 3);             // and no flip-flop while it lives
    Enemy const last[] = { E(1, kSulfuron, 100.f), E(2, kPriest, 0.f, false), E(3, kPriest, 0.f, false), E(4, kPriest, 0.f, false), E(5, kPriest, 0.f, false) };
    CHECK(PickFocus(*f, last, 5, 0, 3) == 1);            // all four down: Sulfuron
}

TEST_CASE("Golemagg's script names him alone; the Core Ragers are never the focus", "[ClassicRaidFightScript]")
{
    constexpr uint32_t kGolemagg = 11988, kRager = 11672;
    Fight const* f = FightFor(kMc, kGolemagg);
    REQUIRE(f != nullptr);
    CHECK(FightFor(kMc, kRager) == nullptr);   // not the script's enemies: the rules that keep them off every target stay in charge
    CHECK(f->startWaitMs == Playerbot::kRaidBossOpenWaitMs);
    Enemy const e[] = { E(1, kGolemagg, 60.f), E(2, kRager, 20.f), E(3, kRager, 20.f) };
    CHECK(PickFocus(*f, e, 3, 0, 0) == 1);
    Enemy const noBoss[] = { E(1, kGolemagg, 60.f, false), E(2, kRager, 20.f) };
    CHECK(PickFocus(*f, noBoss, 2, 0, 0) == 0);
}

// ---- Majordomo, version B ----

TEST_CASE("Majordomo: only the owner's skull is hit, and he is never the focus", "[ClassicRaidFightScript]")
{
    constexpr uint32_t kMajordomo = 12018, kHealer = 11663, kElite = 11664;
    Fight const* f = FightFor(kMc, kMajordomo);
    REQUIRE(f != nullptr);
    CHECK(f->skullOnly);
    CHECK(FightFor(kMc, kHealer) == f);
    CHECK(FightFor(kMc, kElite) == f);
    CHECK(f->startWaitMs == Playerbot::kRaidBossOpenWaitMs);
    Enemy const all[] = { E(1, kMajordomo, 100.f), E(2, kElite, 50.f), E(3, kElite, 100.f), E(4, kHealer, 100.f), E(5, kHealer, 80.f) };
    CHECK(PickFocus(*f, all, 5, 0, 0) == 0);              // no skull: nobody is hit, however hurt
    CHECK(PickFocus(*f, all, 5, 4, 5) == 4);              // the skull, not the last pick
    Enemy const sheeped[] = { E(1, kMajordomo, 100.f), E(4, kHealer, 100.f, false), E(5, kHealer, 80.f) };
    CHECK(PickFocus(*f, sheeped, 3, 4, 0) == 0);          // a skull that cannot be hit is not replaced by another
}

TEST_CASE("Majordomo: the second tank holds him, the Healers' Shock circle is 21 yards", "[ClassicRaidFightScript]")
{
    CHECK(Playerbot::Raid::OffTankMob(kMc, 12018));
    CHECK_FALSE(Playerbot::Raid::OffTankMob(kMc, 11664));
    Playerbot::Raid::RaidRules const* mc = Playerbot::Raid::RulesForMap(kMc);
    REQUIRE(mc != nullptr);
    CHECK(mc->tankSpot.empty());
    int shock = 0;
    for (auto const& a : mc->avoidEnemy)
        if (a.enemy == 11663)
        {
            CHECK(a.radius == 21.0f);
            ++shock;
        }
    CHECK(shock == 2);
}

// Majordomo's shields take half the raid off an add, not all of it: Magic Reflection the casters, Damage Shield the melee DPS. A hunter and a
// tank are stopped by neither (acmc, n040, n124 Majordomo 2026-10-15: either shield idled everybody for 10 s of every 30).
TEST_CASE("Majordomo: Magic Reflection stops the casters, Damage Shield the melee, nothing else", "[ClassicRaidFightScript]")
{
    using namespace Playerbot::Raid::Shields;
    CHECK(ShieldForbids(kMagicReflection, false, true));     // caster off a reflecting add
    CHECK_FALSE(ShieldForbids(kMagicReflection, true, false)); // melee keeps swinging at it
    CHECK(ShieldForbids(kDamageShield, true, false));        // melee off a Damage Shield add
    CHECK_FALSE(ShieldForbids(kDamageShield, false, true));  // casters keep casting at it
    CHECK_FALSE(ShieldForbids(kMagicReflection, false, false)); // hunter / tank: neither
    CHECK_FALSE(ShieldForbids(kDamageShield, false, false));
    CHECK_FALSE(ShieldForbids(21086, true, true));           // Encouragement is no shield
}

// The cast API refuses a spell at a reflecting target only when the reflect can send it back (Spell::m_canReflect). The tanks' abilities go through,
// so a tank keeps Taunt, Sunder Armor, Revenge, Heroic Strike and Shield Slam on a Magic Reflection add (n124 Majordomo 2026-10-15: all five refused).
TEST_CASE("Spell reflect refusal: magic spells are refused, abilities and reflect-piercing spells are not", "[ClassicRaidFightScript]")
{
    using namespace Playerbot::Raid::Shields;
    //                    magic   ability allow  noRefl noImm  passive
    CHECK(ReflectRefuses(true,  false,  false, false, false, false));   // Frostbolt, Shadow Bolt, Polymorph
    CHECK_FALSE(ReflectRefuses(true,  true,   false, false, false, false));   // a magic-class ability (Taunt-like)
    CHECK_FALSE(ReflectRefuses(false, true,   false, false, false, false));   // Sunder Armor, Heroic Strike, Revenge, Shield Slam
    CHECK_FALSE(ReflectRefuses(false, false,  false, false, false, false));   // a melee-class spell with no ability flag: not reflectable either
    CHECK(ReflectRefuses(false, true,   true,  false, false, false));   // flagged to allow reflection: refused whatever its class
    CHECK_FALSE(ReflectRefuses(true,  false,  false, true,  false, false));   // pierces reflection
    CHECK_FALSE(ReflectRefuses(true,  false,  false, false, true,  false));   // pierces immunities
    CHECK_FALSE(ReflectRefuses(true,  false,  false, false, false, true));    // a passive
}

// His Teleport resets his threat list every 20-30 s and nobody hits him (Priority -1), so he is on the retaunt list like the other threat-reset
// bosses (acmc 2026-10-15: on two healers for 30 s after a teleport).
TEST_CASE("Majordomo: a tank taunts him back after his teleport's threat reset", "[ClassicRaidFightScript]")
{
    CHECK(Playerbot::Raid::Retaunts(kMc, 12018));
    CHECK(Playerbot::Raid::Retaunts(kMc, kLucifron));        // the list before him is unchanged
    CHECK_FALSE(Playerbot::Raid::Retaunts(kMc, 11663));      // not the adds: they are held, not re-taunted off a DPS
}

// Dark Mending 19775 is the heal the Flamewaker Healers cast (sql/custom/world/2026_10_16_00_world_classic_flamewaker_healer_dark_mending): the
// kick election takes it before their Shadow Bolt, and gives it to a kicker that can kick now. In the three 2026-10-15 fights most kicks landed
// in the last 550 ms by the backups (rank 2+): a rank-0 kicker mid-Frostbolt cannot kick on sight.
TEST_CASE("Kick election: Dark Mending before every other cast, a free kicker before a busy one", "[ClassicRaidFightScript]")
{
    using namespace Playerbot::Raid;
    CHECK(InterruptOrder(kMc, 19775) == 0);
    CHECK(InterruptOrder(kMc, 19775) < InterruptOrder(kMc, 21077));          // the heal before the Healer's Shadow Bolt
    CHECK(InterruptOrder(kMc, 21077) < kNotInterruptFirst);
    CHECK(InterruptOrder(kMc, 20229) == kNotInterruptFirst);                 // Blast Wave: not listed
    CHECK(InterruptFirst(kMc, 19775));
    CHECK_FALSE(InterruptFirst(kMc, 20229));
    CHECK(InterruptOrder(1, 19775) == kNotInterruptFirst);                   // a map without rules

    Kicks::Cast const mending{ 0, 1500, 7 }, bolt{ 2, 400, 3 }, other{ kNotInterruptFirst, 350, 1 };
    CHECK(Kicks::CastBefore(mending, bolt));                                 // the list's order wins over the time left
    CHECK_FALSE(Kicks::CastBefore(bolt, mending));
    CHECK(Kicks::CastBefore(bolt, other));
    CHECK(Kicks::CastBefore(Kicks::Cast{ 2, 400, 9 }, Kicks::Cast{ 2, 900, 1 }));   // same order: the one about to land first
    CHECK(Kicks::CastBefore(Kicks::Cast{ 2, 400, 1 }, Kicks::Cast{ 2, 400, 9 }));   // then the lowest guid

    Kicks::Kicker const freeFar{ false, 20.f, 5 }, busyNear{ true, 3.f, 2 }, freeNear{ false, 3.f, 9 };
    CHECK(Kicks::KickerBefore(freeFar, busyNear));                           // a mage mid-Frostbolt cannot kick on sight
    CHECK_FALSE(Kicks::KickerBefore(busyNear, freeFar));
    CHECK(Kicks::KickerBefore(freeNear, freeFar));                           // both free: the nearest
    CHECK(Kicks::KickerBefore(Kicks::Kicker{ false, 3.f, 2 }, freeNear));    // same distance: the lowest guid
}

// ---- Nefarian (Blackwing Lair) ----

namespace
{
constexpr uint32_t kBwl = 469, kVictor = 10162, kNefarian = 11583, kRed = 14264, kBronze = 14263, kChromatic = 14302, kConstruct = 14605;

Fight const& Nef()
{
    Fight const* f = FightFor(kBwl, kVictor);
    REQUIRE(f != nullptr);
    return *f;
}

Enemy H(uint64_t guid, uint32_t entry, float hp, bool held, bool eligible = true)
{
    Enemy e = E(guid, entry, hp, eligible);
    e.held = held;
    return e;
}
}

TEST_CASE("Nefarian's script covers Victor Nefarius, every drakonid colour and Nefarian on Blackwing Lair only", "[ClassicRaidFightScript]")
{
    for (uint32_t entry : { 14261u, 14262u, kBronze, kRed, 14265u, kChromatic, kNefarian })
        CHECK(FightFor(kBwl, entry) == &Nef());
    CHECK(FightFor(kBwl, kConstruct) == nullptr);   // the Bone Constructs stay with the generic rules (Nefarian before them)
    CHECK(FightFor(kMc, kRed) == nullptr);
    CHECK(Nef().startWaitMs == Playerbot::kRaidBossOpenWaitMs);
    // Every colour is one rank, Nefarian the next; Victor (immune behind his barrier) is never on the list.
    CHECK(RankOf(Nef(), kRed) == 0);
    CHECK(RankOf(Nef(), kBronze) == 0);
    CHECK(RankOf(Nef(), kChromatic) == 0);
    CHECK(RankOf(Nef(), kNefarian) == 1);
    CHECK(RankOf(Nef(), kVictor) == 99);
    // The other fights keep one rank per entry.
    CHECK(RankOf(Luc(), kProtector) == 0);
    CHECK(RankOf(Luc(), kLucifron) == 1);
}

TEST_CASE("Drakonids: the one a tank holds first, the most hurt of those, whatever the colour", "[ClassicRaidFightScript]")
{
    Enemy const e[] = { H(1, kVictor, 100.f, false), H(2, kRed, 30.f, false), H(3, kBronze, 90.f, true), H(4, kRed, 60.f, true) };
    CHECK(PickFocus(Nef(), e, 4, 0, 0) == 4);                 // held beats the more hurt loose one; among held the most hurt
    Enemy const loose[] = { H(1, kVictor, 100.f, false), H(2, kRed, 80.f, false), H(3, kChromatic, 50.f, false) };
    CHECK(PickFocus(Nef(), loose, 3, 0, 0) == 3);             // nothing held: the most hurt loose one
    CHECK(PickFocus(Nef(), e, 4, 0, 3) == 3);                 // no flip while the pick lives
    CHECK(PickFocus(Nef(), e, 4, 2, 0) == 2);                 // the owner's skull still wins
    Enemy const victorOnly[] = { H(1, kVictor, 100.f, false) };
    CHECK(PickFocus(Nef(), victorOnly, 1, 0, 0) == 0);        // between waves: the generic rules
}

TEST_CASE("Nefarian comes after the drakonids still up", "[ClassicRaidFightScript]")
{
    Enemy const both[] = { H(1, kNefarian, 100.f, true), H(2, kRed, 90.f, true) };
    CHECK(PickFocus(Nef(), both, 2, 0, 0) == 2);
    CHECK(PickFocus(Nef(), both, 2, 0, 1) == 2);              // a Nefarian pick gives way to a drakonid
    Enemy const last[] = { H(1, kNefarian, 100.f, true), H(2, kRed, 0.f, true, false), H(3, kConstruct, 100.f, false) };
    CHECK(PickFocus(Nef(), last, 3, 0, 0) == 1);
}

TEST_CASE("The held rule is the Nefarian fight's only: Sulfuron's priests stay most hurt first", "[ClassicRaidFightScript]")
{
    constexpr uint32_t kSulfuron = 12098, kPriest = 11662;
    Fight const* f = FightFor(kMc, kSulfuron);
    REQUIRE(f != nullptr);
    CHECK_FALSE(f->heldFirst);
    Enemy const e[] = { H(1, kSulfuron, 100.f, true), H(2, kPriest, 50.f, false), H(3, kPriest, 90.f, true) };
    CHECK(PickFocus(*f, e, 3, 0, 0) == 2);
}

TEST_CASE("Drakonids: among the held ones a Black Drakonid first, its Brood Power hits every 4-6 s", "[ClassicRaidFightScript]")
{
    constexpr uint32_t kBlack = 14265;
    CHECK(Nef().preferEntry == kBlack);
    CHECK(Nef().schoolAware);
    CHECK_FALSE(Luc().schoolAware);
    Enemy const e[] = { H(1, kRed, 20.f, true), H(2, kBlack, 90.f, true), H(3, kBlack, 95.f, false) };
    CHECK(PickFocus(Nef(), e, 3, 0, 0) == 2);                 // held Black before a more hurt held Red; a loose Black after both
    Enemy const noHeldBlack[] = { H(1, kRed, 20.f, true), H(2, kRed, 40.f, true), H(3, kBlack, 95.f, false) };
    CHECK(PickFocus(Nef(), noHeldBlack, 3, 0, 0) == 1);
    Enemy const twoBlack[] = { H(1, kBlack, 60.f, true), H(2, kBlack, 30.f, true) };
    CHECK(PickFocus(Nef(), twoBlack, 2, 0, 0) == 2);          // then the most hurt
    CHECK(PickFocus(Nef(), e, 3, 0, 1) == 1);                 // and no flip while the pick lives
}

TEST_CASE("Nefarian: the tanks hold every drakonid colour on one arc round the ranged camp", "[ClassicRaidFightScript]")
{
    Playerbot::Raid::RaidRules const* bwl = Playerbot::Raid::RulesForMap(kBwl);
    REQUIRE(bwl != nullptr);
    REQUIRE(bwl->tankSpot.size() == 1u);
    Playerbot::Raid::TankSpot const& arc = bwl->tankSpot.front();
    CHECK(arc.fixed);
    CHECK(arc.anchor == kVictor);
    CHECK(arc.spacing >= 16.0f);                              // a pile's Brood Power (15 y) does not reach the next tank
    for (uint32_t entry : { 14261u, 14262u, kBronze, kRed, 14265u, kChromatic })
        CHECK(Playerbot::Raid::InTankSpot(arc, entry));
    CHECK_FALSE(Playerbot::Raid::InTankSpot(arc, kNefarian));
    CHECK_FALSE(Playerbot::Raid::InTankSpot(arc, kVictor));

    // The ranged stand at the arc's centre: every pile out of Brood Power and inside the 30 yard spells, also from the edge of the post.
    Playerbot::Raid::Stand const* ranged = nullptr;
    Playerbot::Raid::Stand const* healer = nullptr;
    for (Playerbot::Raid::Stand const& st : bwl->stand)
        if (st.enemy == kVictor)
            (st.who == Playerbot::Raid::Who::Ranged ? ranged : healer) = &st;
    REQUIRE(ranged != nullptr);
    REQUIRE(healer != nullptr);
    CHECK(healer->who == Playerbot::Raid::Who::Healer);
    CHECK(ranged->x == arc.x);
    CHECK(ranged->y == arc.y);
    CHECK(arc.reach - ranged->tolerance > 15.0f);
    CHECK(arc.reach + ranged->tolerance <= 30.0f);
    // The healers' post: every slot of a four-tank arc within 40 yards from anywhere on it.
    float const step = 2.0f * std::asin(arc.spacing / (2.0f * arc.reach));
    for (int k = 0; k < 4; ++k)
    {
        float const a = arc.rowDeg * 3.14159265f / 180.0f + (float(k) - 1.5f) * step;
        float const sx = arc.x + arc.reach * std::cos(a), sy = arc.y + arc.reach * std::sin(a);
        CHECK(std::hypot(sx - healer->x, sy - healer->y) + healer->tolerance <= 40.0f);
    }
}

namespace
{
constexpr uint32_t kZg = 309;
Fight const& Zg(uint32_t boss)
{
    Fight const* f = FightFor(kZg, boss);
    REQUIRE(f != nullptr);
    REQUIRE(f->boss == boss);
    return *f;
}
}

TEST_CASE("Zul'Gurub: every boss but Thekal has a fight script, on map 309 only", "[ClassicRaidFightScript]")
{
    for (uint32_t boss : { 14517u, 14507u, 14510u, 11382u, 11380u, 14515u, 15114u, 14834u, 15082u, 15083u, 15084u, 15085u })
    {
        CHECK(FightFor(kZg, boss) != nullptr);
        CHECK(FightFor(kMc, boss) == nullptr);
        CHECK(Zg(boss).startWaitMs == Playerbot::kRaidBossOpenWaitMs);
        CHECK_FALSE(Zg(boss).skullOnly);
    }
    // Thekal, Zath and Lor'Khan fall within 10 s of each other (the killTogether rule); one focus would kill one alone and see him raised.
    for (uint32_t trio : { 14509u, 11348u, 11347u })
        CHECK(FightFor(kZg, trio) == nullptr);
    // The Shades of Jin'do are only for the cursed bot, never a raid focus.
    CHECK(RankOf(Zg(11380), 14986) == 99);
}

TEST_CASE("Zul'Gurub: the adds die before their boss", "[ClassicRaidFightScript]")
{
    // Jeklik: either kind of bat first (one rank), then her.
    Enemy const jek[] = { E(1, 14517, 50.f), E(2, 11368, 90.f), E(3, 14965, 70.f) };
    CHECK(PickFocus(Zg(14517), jek, 3, 0, 0) == 3);
    CHECK(RankOf(Zg(14517), 11368) == RankOf(Zg(14517), 14965));
    // Venoxis: a Parasitic Serpent, then the cobras, then Venoxis.
    Enemy const ven[] = { E(1, 14507, 40.f), E(2, 11373, 20.f), E(3, 14884, 100.f) };
    CHECK(PickFocus(Zg(14507), ven, 3, 0, 0) == 3);
    Enemy const venNoSerpent[] = { E(1, 14507, 40.f), E(2, 11373, 80.f), E(3, 11373, 20.f) };
    CHECK(PickFocus(Zg(14507), venNoSerpent, 3, 0, 0) == 3);
    // Mar'li: her Spawns first.
    Enemy const mar[] = { E(1, 14510, 30.f), E(2, 15041, 100.f) };
    CHECK(PickFocus(Zg(14510), mar, 2, 0, 0) == 2);
    // Mandokir: the Speaker, Ohgan, then Mandokir.
    Enemy const man[] = { E(1, 11382, 100.f), E(2, 14988, 100.f), E(3, 11391, 100.f) };
    CHECK(PickFocus(Zg(11382), man, 3, 0, 0) == 3);
    Enemy const manNoSpeaker[] = { E(1, 11382, 10.f), E(2, 14988, 100.f) };
    CHECK(PickFocus(Zg(11382), manNoSpeaker, 2, 0, 0) == 2);
    // Jin'do: the ward and the totem (one rank, the most hurt), then him; the cursed bot is left alone.
    Enemy const jin[] = { E(1, 11380, 10.f), E(2, 15112, 100.f), E(3, 14987, 60.f) };
    CHECK(PickFocus(Zg(11380), jin, 3, 0, 0) == 3);
    CHECK(Zg(11380).freeAura == 24306);
    // Hazza'rah: the Nightmare Illusions first.
    Enemy const haz[] = { E(1, 15083, 50.f), E(2, 15163, 100.f) };
    CHECK(PickFocus(Zg(15083), haz, 2, 0, 0) == 2);
}

TEST_CASE("Arlokk: she is the focus while she can be hit, her Prowlers while she is vanished", "[ClassicRaidFightScript]")
{
    Enemy const up[] = { E(1, 14515, 90.f), E(2, 15101, 10.f) };
    CHECK(PickFocus(Zg(14515), up, 2, 0, 0) == 1);
    Enemy const vanished[] = { E(1, 14515, 90.f, false), E(2, 15101, 60.f), E(3, 15101, 30.f) };
    CHECK(PickFocus(Zg(14515), vanished, 3, 0, 0) == 3);
    CHECK(PickFocus(Zg(14515), up, 2, 0, 3) == 1);   // back out: a Prowler pick gives way to her
}

TEST_CASE("Hakkar: the raid kills a Son and gathers on it before each Blood Siphon", "[ClassicRaidFightScript]")
{
    using namespace Playerbot::Raid::Zg;
    // The first siphon 90 s after the pull, then 90 s after the last one seen.
    CHECK(SiphonDueMs(0) == 90000u);
    CHECK(SiphonDueMs(93000) == 183000u);
    // A Son dies from 45 s before it to 3 s before it, while fewer than half of the raid carry the poison.
    CHECK_FALSE(KillWindow(44000, 0, 0, 20));
    CHECK(KillWindow(45000, 0, 0, 20));
    CHECK(KillWindow(86000, 0, 9, 20));
    CHECK_FALSE(KillWindow(87000, 0, 0, 20));
    CHECK_FALSE(KillWindow(70000, 0, 10, 20));
    CHECK_FALSE(KillWindow(120000, 93000, 0, 20));
    CHECK(KillWindow(160000, 93000, 0, 20));
    // Poisonous Blood lasts 90 s: a Son killed 45 s before still covers the siphon.
    CHECK(kSiphonLeadMs < 90000u);
    // The gather: low enough Son, not poisoned, not excluded, in reach, not already on it (the death reaches 8 y).
    CHECK(Gathers(false, false, 50.f, 20.f));
    CHECK_FALSE(Gathers(true, false, 50.f, 20.f));
    CHECK_FALSE(Gathers(false, true, 50.f, 20.f));
    CHECK_FALSE(Gathers(false, false, 80.f, 20.f));
    CHECK_FALSE(Gathers(false, false, 50.f, 60.f));
    CHECK_FALSE(Gathers(false, false, 50.f, 3.f));
    CHECK(kGatherYards < kSonDeathReach);
    // Hakkar's own fight script is the boss alone: the Sons are not hit outside the window.
    CHECK(RankOf(Zg(kHakkar), kSonOfHakkar) == 99);
}
