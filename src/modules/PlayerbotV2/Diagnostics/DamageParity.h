// DamageParity - the runtime of the damage_parity scenarios (`.playerbot scenario run damage_parity_<class>`, `finisher_sweep_<class>`,
// `talent_audit_<class>`): one level 60 bot, a level 63 target dummy with a raid boss's armor, and the damage the bot does to it, tallied per
// spell and compared with what vanilla gives (DamageParityMath.h). The scenario runner stages the bot (level, gear pipeline, talents) and hands
// it over with Begin; Step is called once a second until it returns true.
//
// World thread only, except the two damage hooks, which come from the map's thread and only take the tally's lock.

#pragma once

#include "Diagnostics/DamageParityMath.h"

#include "ObjectGuid.h"

#include <cstdint>
#include <string>
#include <vector>

class Creature;
class Player;
class Unit;

namespace Playerbot::Diagnostics::Parity {

// Economy: mana pool, regen in and out of combat and while casting, spell costs, heal sizes (a caster or healer, bot logic detached).
// Incoming: a tank is hit by a level 63 dummy at bare / tier 1 / tier 3 armor, then by Firesworn, Magmadar and a Deadmines mob.
// Trap: a hunter sets Explosive, Immolation, Frost and Freezing Trap out of combat, a target dummy is moved onto each, and the trap must fire
// (the [trap_cast] line carries the cast result and the spell's effect targets) and do its job to the dummy.
// Group: the dps scorecard - a whole raid of one bot per dps spec (plus a tank and buffers) hits one boss dummy at once; one tally per bot, one
// scorecard row each (BeginGroup, DpsScorecard.tsv). The bots keep their own logic.
// SpellCheck: the player spell check (spell_check_<class>). The bot casts each key spell of its class at the dummy (or a heal on itself) the
// way a player's cast goes (Player::CastSpell, not triggered), several times, and each spell must land and hit for at least its floor and for
// at least half of what the client's own spell data says (base points plus the attack power and spell power coefficients). One PASS / FAIL /
// SKIP line per spell. Needs no game client.
enum class Kind : std::uint8_t { None = 0, Damage = 1, Finisher = 2, TalentAudit = 3, Economy = 4, Incoming = 5, Trap = 6, Threat = 7, Group = 8, SpellCheck = 10 };   // 9 is the support scorecard (Group in support mode, ScenarioDefs)

// A leg is measuring (one atomic load; the hooks return at once when it is false).
bool Active();

// The measuring dummy, which no fight can finish by design: the bot's give-up rules (opener stall, unkillable-target leash) leave it alone.
bool IsDummy(ObjectGuid const& guid);

// The support scorecard's dummy plays a raid boss: the bots read it as boss-ranked (NearbyUnit::is_boss_rank), so the tank and healer
// rules that wait for a boss (Demoralizing Roar on a single target, the boss cooldowns, the raid boss opening wait) run as in a boss
// fight. Its creature template (entry 1000, Classification Normal) is the world's and stays as it is.
bool IsSupportBoss(ObjectGuid const& guid);

// A level 63 hostile dummy `distance` y in front of the player, for `.playerbot proccheck` (no leg running). The caller despawns it.
Creature* SpawnProcDummy(Player* p, float distance, std::string& error);

// From Module::OnHitOutcome / OnDamageTaken: what a swing or direct spell did, and every damage event with its amount. Both ignore everything
// that is not the subject (or its pet) hitting the dummy.
void NoteOutcome(Unit* attacker, Unit* victim, std::uint32_t spell_id, std::uint8_t outcome, std::uint32_t damage);
void NoteDamage(Unit* attacker, Unit* victim, std::int32_t amount, std::uint32_t spell_id);
// From Module::OnEnergize: power a spell gave the subject. Counted by spell id inside the economy audit's casting-regen window only.
// From the bot API after the core accepted a cast: the damage legs count them per spell id and fail a damaging spell that was cast but left nothing
// on the dummy.
void NoteCast(Unit* caster, std::uint32_t spell_id);
// From Module::OnHealOutcome: every heal with what it was worth (`raw`) and what it restored (`effective`; the rest is overheal). Counted per healer
// and spell while a support scorecard measures.
void NoteHeal(Unit* healer, Unit* target, std::uint32_t spell_id, std::uint32_t raw, std::uint32_t effective);
void NoteEnergize(Unit* target, std::uint32_t spell_id, std::int32_t gained, std::uint8_t power);

// A spot on flat ground with line of sight along `distance` y in direction `o`, nearest to the bot (its own spot first). False when the
// ground around is not flat. The runner moves the bot there before Begin so the dummy can stand on the bot's level.
bool FindOpenSpot(Player* p, float distance, float& x, float& y, float& z, float& o);
// The flat ground the run measures from, remembered for the legs that follow (a leg whose bot ended in water or on a ledge goes back to it).
void NoteFlatSpot(uint32 map, float x, float y, float z, float o);

// Takes over the staged bot. False with `error` when the dummy cannot be made (no suitable creature template).
bool Begin(Player* subject, Kind kind, std::uint32_t leg_seconds, std::string& error, std::uint32_t spec = 0);

// The raid of the dps scorecard. `label` names the spec on the scorecard ("Arms Warrior"), `spec` is its Classic spec id, `melee` stands it at the
// boss, `scored` is false for the tank and the buffers (their damage is logged but they get no verdict).
struct GroupMember
{
    Player* player = nullptr;
    std::uint32_t spec = 0;
    std::string label;
    bool melee = false;
    bool scored = true;
    std::uint8_t role = 0;   // 0 dps, 1 healer, 2 tank
};

// Takes over a staged, grouped raid: the dummy is summoned by the first scored member, the raid is stood around it, and Step runs the buff wait,
// the warm-up and the measurement. False with `error` when the dummy cannot be made.
// `support` runs the support scorecard instead: the boss hits the tank (the member with role 2) and raiders in turn, the healers (role 1) heal, and
// every member is audited; the rows go to DpsScorecard.tsv (columns in ReportSupport).
bool BeginGroup(std::vector<GroupMember> const& members, std::uint32_t leg_seconds, std::string& error, bool support = false);

// One step of a second. True when the scenario is over; `failure` is then the reason it failed, empty when it passed.
bool Step(std::string& failure);

// Puts everything back: the dummy removed, the stat bonuses taken off, the gear override cleared. Safe to call twice.
void End();

// "damage_parity: leg 3 of 7 (t3_sp), measuring, 94 s" for `.playerbot scenario status`.
std::string Status();

} // namespace Playerbot::Diagnostics::Parity
