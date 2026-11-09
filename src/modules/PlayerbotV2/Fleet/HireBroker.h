// The hire brokers (Playerbot.Hire.*): Sellsword Captains at inns, Veteran Recruiters in the capitals, black-market brokers in
// Gadgetzan, Booty Bay and Ratchet. Their gossip hires a bot of the broker's grade; the hiring itself is `.bot add`'s
// (cs_playerbot_v2.cpp AddCompanionFor), so a hired bot is set up, geared and grouped like any companion.

#pragma once

#include "HirePolicy.h"
#include "../Bot/BotTypes.h"
#include <string>

class Player;

namespace Playerbot::V2 {

// Hires one companion of `cls` and `grade` from the broker beside `owner`; `msg` is what the owner is told.
bool HireCompanionForOwner(Player* owner, uint8 cls, Playerbot::Role role, Fleet::Hire::Grade grade, std::string const& className,
                           std::string& msg);
// Whether a race of the owner's faction can be a companion of class `cls` on this realm.
bool CanHaveCompanionClass(Player const* owner, uint8 cls);

}  // namespace Playerbot::V2

void AddSC_playerbot_v2_hire_broker();
