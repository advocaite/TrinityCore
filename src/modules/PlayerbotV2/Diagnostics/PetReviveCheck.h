// pet_revive_hunter: a dead hunter pet comes back from Revive Pet as happy as it died, and the pet spells come with the pet.
//
// A player's pet that died and was loaded again from the database (its body gone after about a minute, a relog, a mount, a teleport) used
// to come back from Revive Pet with 0 happiness: unhappy, losing loyalty every 12 s, and a Rebellious pet ran away for good (player report,
// 5 Nov). And no trainer taught Revive Pet, Feed Pet or Dismiss Pet to players (2026_11_05_85: they now come with Tame Beast and Call Pet).
//
// The check plays the player's path on one level 60 bot hunter with its bot logic detached (a bot would revive its pet itself, and a bot's
// pet is kept happy on every update, which would hide the bug):
//   1. the pet's happiness is set to a known value (content), then the pet dies;
//   2. the runner waits until the server removes the body (Pet::Update, as for a player; after 150 s it removes it the same way itself);
//   3. the saved pet must be dead with that happiness;
//   4. spell_learn_spell must give Revive Pet, Feed Pet and Dismiss Pet with Tame Beast and Call Pet;
//   5. Revive Pet is cast (triggered, so it lands in the same call: the next pet update would top a bot pet's happiness up and hide a 0);
//      the pet must be alive with the happiness, loyalty level and loyalty points it died with.
// Lines: "[pet_revive] ..." in Playerbot.log; the verdict is the scenario's PASS / FAIL.
//
// Header only and included by ScenarioRunner.cpp alone, so no CMake reconfigure is needed for it.

#pragma once

#include "../Services.h"
#include "../Bot/BotRegistry.h"
#include "../Threading/SnapshotPublisher.h"
#include "../Threading/TickScheduler.h"
#include "CharmInfo.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "SpellDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "fmt/format.h"

#include <array>
#include <cstdint>
#include <string>

namespace Playerbot::Diagnostics::PetRevive {

inline constexpr std::uint8_t kParityKind = 11;            // ScenarioDef::parity value of pet_revive_hunter (not a Parity::Kind)
inline constexpr std::uint32_t kRevivePet = 982, kFeedPet = 6991, kDismissPet = 2641, kCallPet = 883, kTameBeast = 1515;
inline constexpr std::int32_t kTestHappiness = 600000;     // content (333334..666666), far from 0 and from the 1000000 a bot pet is kept at
inline constexpr std::uint32_t kBodyWaitMs = 150u * 1000u;

struct State
{
    bool active = false;
    ObjectGuid subject;
    std::uint8_t step = 0;
    std::uint32_t step_start_ms = 0;
    std::uint32_t pet_number = 0;
    std::uint8_t loyalty_level = 0;
    std::int32_t loyalty_points = 0;
    bool body_forced = false;
};

inline State& St()
{
    static State s;
    return s;
}

inline void Line(std::string const& text)
{
    TC_LOG_INFO("playerbot.v2", "[pet_revive] {}", text);
}

// False with `error` set when the bot cannot run the check (no hunter, no pet).
inline bool Begin(Player* p, std::string& error)
{
    State& s = St();
    s = State{};
    if (!p || p->GetClass() != CLASS_HUNTER)
    {
        error = "the subject is not a hunter";
        return false;
    }
    Pet* pet = p->GetPet();
    // A reused character can have its pet put away (dismissed, or out when it logged off): call it, as a player would.
    if (!pet && p->HasSpell(kCallPet))
    {
        p->CastSpell(p, kCallPet, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        pet = p->GetPet();
    }
    // A run that failed half way left the pet dead: bring it back first (this is not the measured revive).
    if ((!pet || !pet->IsAlive()) && sSpellMgr->GetSpellInfo(kRevivePet, DIFFICULTY_NONE))
    {
        p->CastSpell(p, kRevivePet, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
        pet = p->GetPet();
    }
    if (!pet || !pet->IsAlive() || pet->getPetType() != HUNTER_PET || !pet->GetCharmInfo())
    {
        error = "the hunter has no hunter pet out";
        return false;
    }

    // The bot logic stops, as for a parity sweep: from here the hunter is played like a player who does nothing on its own.
    ::Playerbot::BotId const id = p->GetGUID().GetCounter();
    if (::Playerbot::Services::Registry().has(id))
    {
        ::Playerbot::Services::Scheduler().unregister_bot(id);
        ::Playerbot::Services::Snapshots().remove(id);
        ::Playerbot::Services::Registry().unregister_bot(id);
    }

    s.active = true;
    s.subject = p->GetGUID();
    s.step = 0;
    s.step_start_ms = getMSTime();
    Line(fmt::format("begin hunter={} level={} pet={} entry={} level={}", p->GetName(), p->GetLevel(), pet->GetName(), pet->GetEntry(), pet->GetLevel()));
    return true;
}

inline bool Fail(std::string& failure, std::string const& why)
{
    St().active = false;
    failure = why;
    Line(fmt::format("verdict=FAIL reason=\"{}\"", why));
    return true;
}

// One step per scenario tick. True when the check is over; `failure` is empty on a PASS.
inline bool Step(std::string& failure)
{
    State& s = St();
    if (!s.active)
        return Fail(failure, "the check is not running");
    Player* p = ObjectAccessor::FindConnectedPlayer(s.subject);
    if (!p || !p->IsInWorld())
        return Fail(failure, "the hunter went offline");
    if (!p->IsAlive())
        return Fail(failure, "the hunter died");

    switch (s.step)
    {
        case 0:   // the pet dies with a known happiness
        {
            Pet* pet = p->GetPet();
            if (!pet)
                return Fail(failure, "the pet is gone before the test");
            p->CombatStop(true);
            s.pet_number = pet->GetCharmInfo()->GetPetNumber();
            s.loyalty_level = pet->GetLoyaltyLevel();
            s.loyalty_points = pet->GetLoyaltyPoints();
            pet->SetPower(POWER_HAPPINESS, kTestHappiness);
            if (pet->IsAlive())
                pet->KillSelf(false);
            if (pet->IsAlive())
                return Fail(failure, "the pet did not die");
            Line(fmt::format("step=1 pet number={} died happiness={} loyalty={} points={}", s.pet_number, pet->GetPower(POWER_HAPPINESS), s.loyalty_level,
                             s.loyalty_points));
            s.step = 1;
            s.step_start_ms = getMSTime();
            return false;
        }
        case 1:   // the body goes, as it does for a player who does not revive the pet within about a minute
        {
            if (Pet* pet = p->GetPet())
            {
                if (pet->IsAlive())
                    return Fail(failure, "the pet came back to life on its own while its body should lie there");
                if (GetMSTimeDiffToNow(s.step_start_ms) < kBodyWaitMs)
                    return false;
                // Same call Pet::Update makes when the body's time is up.
                s.body_forced = true;
                p->RemovePet(pet, PET_SAVE_NOT_IN_SLOT);
            }
            PetStable const* stable = p->GetPetStable();
            PetStable::PetInfo const* saved = nullptr;
            if (stable)
                for (Optional<PetStable::PetInfo> const& info : stable->ActivePets)
                    if (info && info->PetNumber == s.pet_number)
                        saved = &*info;
            if (!saved)
                return Fail(failure, fmt::format("pet number {} is not in an active stable slot after its body went", s.pet_number));
            Line(fmt::format("step=2 body gone after {} s{}: saved health={} happiness={}", GetMSTimeDiffToNow(s.step_start_ms) / 1000u,
                             s.body_forced ? " (removed by the check after 150 s)" : "", saved->Health, saved->Mana));
            if (saved->Health != 0)
                return Fail(failure, fmt::format("the saved pet is not dead (health {})", saved->Health));
            if (std::int32_t(saved->Mana) != kTestHappiness)
                return Fail(failure, fmt::format("the saved pet's happiness is {}, it died with {}", saved->Mana, kTestHappiness));
            s.step = 2;
            s.step_start_ms = getMSTime();
            return false;
        }
        case 2:   // the pet spells come with the pet (2026_11_05_85), and Revive Pet brings it back as it was
        {
            std::string missing;
            for (std::uint32_t source : { kTameBeast, kCallPet })
                for (std::uint32_t spell : { kRevivePet, kFeedPet, kDismissPet })
                {
                    bool found = false;
                    SpellLearnSpellMapBounds const bounds = sSpellMgr->GetSpellLearnSpellMapBounds(source);
                    for (auto itr = bounds.first; itr != bounds.second; ++itr)
                        if (itr->second.Spell == spell)
                            found = true;
                    if (!found)
                        missing += fmt::format(" {}->{}", source, spell);
                }
            if (!missing.empty())
                return Fail(failure, fmt::format("spell_learn_spell does not give the pet spells:{} (world SQL 2026_11_05_85 not applied?)", missing));
            Line(fmt::format("step=3 spell_learn_spell: Tame Beast and Call Pet give Revive Pet, Feed Pet, Dismiss Pet; hunter knows Revive {} Feed {} Dismiss {}",
                             p->HasSpell(kRevivePet), p->HasSpell(kFeedPet), p->HasSpell(kDismissPet)));

            if (!sSpellMgr->GetSpellInfo(kRevivePet, DIFFICULTY_NONE))
                return Fail(failure, "Revive Pet (982) is not in the spell data");
            p->CastSpell(p, kRevivePet, CastSpellExtraArgs(TRIGGERED_FULL_MASK));
            Pet* pet = p->GetPet();
            if (!pet)
                return Fail(failure, "Revive Pet did not bring the pet back (no pet after the cast)");
            if (!pet->IsAlive())
                return Fail(failure, "Revive Pet did not bring the pet back (it is still dead)");
            std::int32_t const happiness = pet->GetPower(POWER_HAPPINESS);
            Line(fmt::format("step=4 revived: number={} health={}/{} happiness={} loyalty={} points={}", pet->GetCharmInfo()->GetPetNumber(), pet->GetHealth(),
                             pet->GetMaxHealth(), happiness, pet->GetLoyaltyLevel(), pet->GetLoyaltyPoints()));
            if (pet->GetCharmInfo()->GetPetNumber() != s.pet_number)
                return Fail(failure, fmt::format("a different pet came back (number {}, expected {})", pet->GetCharmInfo()->GetPetNumber(), s.pet_number));
            if (happiness != kTestHappiness)
                return Fail(failure, fmt::format("the revived pet's happiness is {}, it died with {} (0 = the bug: it turns unhappy and runs away)", happiness,
                                                 kTestHappiness));
            if (pet->GetLoyaltyLevel() != s.loyalty_level || pet->GetLoyaltyPoints() != s.loyalty_points)
                return Fail(failure, fmt::format("loyalty changed over death and revive: {} / {} before, {} / {} after", s.loyalty_level, s.loyalty_points,
                                                 pet->GetLoyaltyLevel(), pet->GetLoyaltyPoints()));
            s.active = false;
            failure.clear();
            Line("verdict=PASS");
            return true;
        }
        default:
            return Fail(failure, "unknown step");
    }
}

inline void End()
{
    St().active = false;
}

} // namespace Playerbot::Diagnostics::PetRevive
