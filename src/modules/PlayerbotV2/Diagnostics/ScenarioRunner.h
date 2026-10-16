// ScenarioRunner - bot scenario tests that run on the server with no client logged in (`.playerbot scenario ...`).
//
// `.playerbot scenario run <name>` makes (first time) or reuses a squad of bots, levels and gears them, pins their roles, groups
// them, sends them into the content, and watches. Once a second the squad is sampled and handed to the ScenarioMonitor, which
// turns idle bots, strays, falls, dead healers' mana, deaths and stalls into failures with a reason. The run ends when the goal
// is met (PASS if nothing failed on the way), on a wipe or a stall, or on the timeout. The verdict goes to Playerbot.log as
// "[scenario] result=PASS|FAIL ..." lines and to Scenario.log, and `.playerbot scenario result` prints it again.
//
// World thread only (it moves Players): Tick is called from Module::OnWorldUpdate, the death hooks from Module::OnDeath.

#pragma once

#include "Diagnostics/ScenarioMonitor.h"

#include <cstdint>
#include <string>

class Unit;

namespace Playerbot::Diagnostics::Scenario {

// Starts the named scenario. False with `error` set when it is unknown or a run is already going.
bool Start(std::string const& name, std::string& error);

// Runs every scenario whose name contains `filter` (all when empty), one after another, each ended before the next starts; one line
// per scenario goes to the summary (`Summary()`, "[scenario_all]" in Playerbot.log). False when none matches or a run is going.
bool StartAll(std::string const& filter, std::string& error);

// Like StartAll, but `size` (default 5) dungeons at once: each with a squad of its own (characters named with the slot's letter after the
// first two, "Scbnwarrior") and its own instance. When the whole wave has ended and nothing failed, the next `size` start; a wave with a
// FAIL stops the batch (fix, then `runbatch` again). Raids and whole-dungeon clears in the filter run one at a time.
bool StartBatch(std::string const& filter, std::size_t size, std::string& error);

// Ends the running scenario(s) as failed ("aborted"); a runall stops after it.
bool Abort();

bool Running();

// One world tick. Cheap when nothing runs.
void Tick(std::uint32_t diff_ms);

// From Module::OnDeath: a player died / a creature died.
void NoteBotDeath(std::uint64_t guid_low, ScenarioDeath death);
void NoteCreatureKilled(std::uint32_t map, std::uint32_t entry, bool tapped);   // tapped: the raiders had a share in the kill (a script's KillSelf or a despawn is no kill)
// From Module::OnDamageTaken: every hit dealt; counted for the squad's raiders (and their pets) from the boss's first combat.
void NoteDamage(Unit* attacker, Unit* victim, std::int32_t amount);

// Human-readable text for the command: what is running now, and the last verdict.
std::string Status();
std::string LastResult();
std::string Summary();   // the runall lines so far: "<name> PASS in 412 s" / "<name> FAIL <first reason>"
std::string List();

} // namespace Playerbot::Diagnostics::Scenario
