// DamageParity - the runtime of the damage_parity scenarios (`.playerbot scenario run damage_parity_<class>`, `finisher_sweep_<class>`,
// `talent_audit_<class>`): one level 60 bot, a level 63 target dummy with a raid boss's armor, and the damage the bot does to it, tallied per
// spell and compared with what vanilla gives (DamageParityMath.h). The scenario runner stages the bot (level, gear pipeline, talents) and hands
// it over with Begin; Step is called once a second until it returns true.
//
// World thread only, except the two damage hooks, which come from the map's thread and only take the tally's lock.

#pragma once

#include "Diagnostics/DamageParityMath.h"

#include <cstdint>
#include <string>

class Player;
class Unit;

namespace Playerbot::Diagnostics::Parity {

// Economy: mana pool, regen in and out of combat and while casting, spell costs, heal sizes (a caster or healer, bot logic detached).
// Incoming: a tank is hit by a level 63 dummy at bare / tier 1 / tier 3 armor, then by Firesworn, Magmadar and a Deadmines mob.
// Trap: a hunter sets Explosive, Immolation, Frost and Freezing Trap out of combat, a target dummy is moved onto each, and the trap must fire
// (the [trap_cast] line carries the cast result and the spell's effect targets) and do its job to the dummy.
enum class Kind : std::uint8_t { None = 0, Damage = 1, Finisher = 2, TalentAudit = 3, Economy = 4, Incoming = 5, Trap = 6 };

// A leg is measuring (one atomic load; the hooks return at once when it is false).
bool Active();

// From Module::OnHitOutcome / OnDamageTaken: what a swing or direct spell did, and every damage event with its amount. Both ignore everything
// that is not the subject (or its pet) hitting the dummy.
void NoteOutcome(Unit* attacker, Unit* victim, std::uint32_t spell_id, std::uint8_t outcome, std::uint32_t damage);
void NoteDamage(Unit* attacker, Unit* victim, std::int32_t amount, std::uint32_t spell_id);
// From Module::OnEnergize: power a spell gave the subject. Counted by spell id inside the economy audit's casting-regen window only.
void NoteEnergize(Unit* target, std::uint32_t spell_id, std::int32_t gained, std::uint8_t power);

// Takes over the staged bot. False with `error` when the dummy cannot be made (no suitable creature template).
bool Begin(Player* subject, Kind kind, std::uint32_t leg_seconds, std::string& error);

// One step of a second. True when the scenario is over; `failure` is then the reason it failed, empty when it passed.
bool Step(std::string& failure);

// Puts everything back: the dummy removed, the stat bonuses taken off, the gear override cleared. Safe to call twice.
void End();

// "damage_parity: leg 3 of 7 (t3_sp), measuring, 94 s" for `.playerbot scenario status`.
std::string Status();

} // namespace Playerbot::Diagnostics::Parity
