// RaidBrainInternal — the raid brain's plan types and helpers, shared between
// RaidBrain.cpp (generic rules) and the per-raid scripted mechanics
// (RaidMechanics_*.cpp). Internal to the raid layer: nothing outside
// Bot/Raid includes this. Everything here runs on the world thread inside
// Brain::Update.

#pragma once

#include "BossTactics.h"
#include "RaidEncounters.h"
#include "RaidOrder.h"
#include "ObjectGuid.h"

#include <cstdint>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Creature;
class Player;
class Unit;

namespace Playerbot::Raid::detail {

using Ranks = std::vector<uint32_t>;

struct Member
{
    Player* p = nullptr;
    bool bot = false;
    bool tank = false;
    bool healer = false;
    bool ranged = false;
    bool mana = false;
};

struct Circle { float x, y, r; };

struct Plan
{
    uint32_t mapId = 0;
    uint64_t groupLow = 0;         // the PvE coordinator's bucket is (group, map) too
    RaidRules const* rules = nullptr;
    std::vector<Member> members;   // alive, on the raid map, sorted by guid
    Player* mainTank = nullptr;
    std::vector<Unit*> enemies;    // alive creatures in combat with the group, sorted by guid
    bool emergency = false;        // somebody under 45% health: healers keep healing
    ObjectGuid ruleFocus;          // kill focus an encounter rule demands (Brain::FocusFor), set by PublishTargets
    // The player's boss plans (BossTactics, "tactics Lucifron: adds first") for the bosses in this fight that have one, from the
    // group's real players (the leader's first).
    std::vector<std::pair<Unit*, BossTactics>> tactics;
    uint16_t tacticsCcDeny = 0;    // the CcDeny kinds those plans leave out (the crowd-control election skips them)
};

bool Matches(Who who, Member const& m, Player* mainTank);
float Dist2D(float ax, float ay, float bx, float by);
bool InsideAny(std::vector<Circle> const& circles, float x, float y, float margin);
bool Controlled(Unit* u);
// A point the bot can actually stand on and walk to (height, line of sight, a complete path).
// Path queries are capped per plan pass (all bots together, see ResetPathProbes): urgent probes (leaving a hazard,
// a carrier run, scripted mechanics, a breath / cone) may use the whole cap, the rest (spread, role spots, stay near)
// stop early so an escape never finds the pass's budget spent.
// `fixedPlace`: a place the encounter fixes (a fixed tank row's slot). The ground, path and detour checks stay; the bot need not see it
// from where it stands (the path walks round what is between) and it need not be in the fight area round the main tank (the row is wider).
bool Usable(Player* bot, float x, float y, float zHint, float& outZ, bool urgent = false, bool hazard = false, bool fixedPlace = false);
void ResetPathProbes();
bool SetMove(RaidOrder& out, float x, float y, float z, char const* reason);
// Best nearby point that increases the distance to every raid member in `others`.
// Points inside `avoid` (the bot's hazard circles) are skipped.
bool Clearest(Player* bot, std::vector<Player*> const& others, float wanted, RaidOrder& out, char const* reason,
              std::vector<Circle> const& avoid, bool urgent);
uint32_t KnownRank(Player* p, Ranks const& ranks);
bool Ready(Player* p, uint32_t spellId, Unit* target, bool hostile);
bool Busy(Player* p);
uint32_t Stacks(Unit* u, uint32_t aura);
Member* FindMember(Plan& plan, Player* p);
// A boss of the encounter (RaidRules::bosses, a world boss or the boss classification).
bool IsBoss(Creature const* c, RaidRules const* rules);
// Target schools (RaidBrain.cpp, PublishTargets): what an enemy avoids, whose resistances stand for the raid's casters, what a member deals.
// reflect (optional): the schools of <avoid> the enemy turns back on the caster, the only ones an area spell must keep off it.
void EnemySchoolMasks(Unit* enemy, Unit const* caster, uint8_t& prefer, uint8_t& avoid, uint8_t* reflect = nullptr);
Unit const* SchoolReferenceCaster(Plan const& plan);
uint8_t MemberDamageSchools(Member const& m, Player* mainTank);

// One election round for a plan: no two bots get the same job.
struct Election
{
    Plan& plan;
    std::unordered_map<uint64_t, RaidOrder>& orders;
    std::unordered_set<uint64_t> busy;   // bots that already have a job this plan
    std::ostringstream& dump;

    RaidOrder* OrderOf(Player* p);
    // First free bot (idle casters first) that knows one of `families` and can cast it at `target`.
    bool Assign(Unit* target, std::vector<Ranks const*> const& families, bool hostile, RaidOrder::CastKind kind, char const* what,
                bool skipHealersInEmergency, int minHealerManaPct = 0);
};

// Scripted mechanics that do not fit the rule tables, one file per raid (RaidMechanics_<raid>.cpp); RaidBrain.cpp
// dispatches by map.
// MechanicsMove runs first in DecideMove; true = it decided this bot's movement (out may still have move == false: hold).
bool MechanicsMove(Plan const& plan, Member const& self, uint32_t now, RaidOrder& out);
// MechanicsElect runs right after the controlled-member election, before every other job.
void MechanicsElect(Election& e, uint32_t now);

// Out of combat (no enemies yet): pre-pull positions. Writes prepull orders for the bots that have one.
void MechanicsPrePull(Plan& plan, std::unordered_map<uint64_t, RaidOrder>& orders, std::ostringstream& dump, uint32_t now);
void NaxxPrePull(Plan& plan, std::unordered_map<uint64_t, RaidOrder>& orders, std::ostringstream& dump);

bool NaxxMechanicsMove(Plan const& plan, Member const& self, uint32_t now, RaidOrder& out);
void NaxxMechanicsElect(Election& e, uint32_t now);
void BwlMechanicsElect(Election& e, uint32_t now);
void BwlPrePull(Plan& plan, std::unordered_map<uint64_t, RaidOrder>& orders, std::ostringstream& dump, uint32_t now);
// A boss fight script (RaidFightScript.h): after the rule passes, overwrites the DPS bots' target and start wait for the scripted boss.
void ApplyFightScript(Plan& plan, std::unordered_map<uint64_t, RaidOrder>& orders, uint32_t now, std::ostringstream& dump);
// Zul'Gurub (RaidMechanics_ZG.cpp): the enemy a scripted fight's DPS must kill now whatever the kill order says (a Son of Hakkar before Blood
// Siphon; nullptr = none), and the movement the fight needs (the raid gathers on that Son as it dies).
Unit* ZgForcedFocus(Plan const& plan, uint32_t now);
bool ZgMechanicsMove(Plan const& plan, Member const& self, uint32_t now, RaidOrder& out);
void ZgMechanicsElect(Election& e, uint32_t now);

bool Aq40MechanicsMove(Plan const& plan, Member const& self, uint32_t now, RaidOrder& out);
void Aq40MechanicsElect(Election& e, uint32_t now);

} // namespace Playerbot::Raid::detail
