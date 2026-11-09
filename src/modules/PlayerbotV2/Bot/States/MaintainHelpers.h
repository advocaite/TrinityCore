// MaintainHelpers - forward declarations for the State_Idle Maintain*
// family. Definitions live in Bot/States/State_Idle.cpp; this header
// lets per-subsystem rule files (e.g. Rules/MaintainRules.cpp) wrap each
// helper as a registered IdleRule without duplicating bodies.

#pragma once

#include "../BotTypes.h"
#include "ObjectGuid.h"

namespace Playerbot {

class BotSnapshotView;
struct NearbyUnit;
class GroupSnapshotView;
class BotAI;
class BotIntentEmitter;
enum class Role : uint8;
struct GroupMemberSummary;
struct DungeonAdvice;
struct BattlegroundAdvice;

namespace States {

// Shared dungeon-combat positioning (BUG G-P0a). The avoidance / mechanic
// rules (step-out-of-fire, kite-fixate, soak, spread, stack, melee-behind)
// were originally inline in DungeonDispatch (the idle dungeon cascade), so
// they NEVER executed during combat — the FSM routes combat ticks to
// DispatchInCombat, not DispatchIdle. This single implementation is now
// called from BOTH paths: early in DispatchInCombat (so avoidance preempts
// the melee gap-close) and in DungeonDispatch (so idle ticks still react).
// Returns true if any sub-rule emitted a positioning action (the caller
// should consume the tick / suppress the chase). Cheaply returns false when
// the bot is not in an instance or the advice carries no positioning data.
bool DungeonCombatPositioning(BotSnapshotView const& s, BotAI& ai,
                              GroupSnapshotView const& g,
                              BotIntentEmitter& emit,
                              DungeonAdvice const& advice);

// Off-mesh-aware single step toward a dungeon target (the tank), GUID-keyed with
// float out-params. Resolves the bot from self_guid, then returns the next
// navmesh waypoint (capped at maxStep) toward (tx,ty,tz); sets is_offmesh when
// that step crosses an off-mesh connection (its SOLID far endpoint). Shared so
// the InGroup / InCombat dungeon-rejoins emit the IDENTICAL far-vertex goal the
// idle regroup-cross does — a per-state goal-key mismatch defeats the move_to
// spline dedup and the follower never completes the bridge hop (Gap-1 flip-flop).
// Definition in State_Idle.cpp. Returns false when the bot is gone or no NORMAL
// path exists (caller falls back to a plain move_to for the same-stratum case).
bool DungeonStepTowardTank(ObjectGuid self_guid, float tx, float ty, float tz,
                           float maxStep, float& ox, float& oy, float& oz,
                           bool& is_offmesh);

// Route-aware fallback for a follower whose DIRECT step to (tx,ty,tz) is broken
// by a navmesh gap: walks the follower along the dungeon crumb-route toward the
// target (the same navigable hops the tank descended) instead of beelining
// across the gap and stranding. Definition in State_Idle.cpp. Returns false
// when there is no route, self is co-located on the route, or the next crumb
// toward the target is itself unreachable. Fallback-only — call after the
// direct step fails so reachable rejoins / non-routed dungeons are unchanged.
bool DungeonRouteStepTowardPos(BotSnapshotView const& s,
                               float tx, float ty, float tz, float maxStep,
                               float& ox, float& oy, float& oz);

// Off-mesh crossing COMMITMENT honored in EVERY state (idle / InGroup / InCombat).
// When a follower starts crossing a Gap-1-style off-mesh bridge it stores a fixed
// far-vertex goal (BotAI::set_dungeon_cross). The MoveSpline traverses the off-mesh
// segment in one motion — but if combat (opener/assist) emits a competing move_to
// MID-JUMP, the spline is replaced, the bot stops ON the off-mesh poly over the
// void, and every later move_to NoPaths from there (it is on a valid poly, so the
// FARFROMPOLY recovery never fires either) — a permanent stall (observed live
// 2026-06-26: healer stuck at (-213,-533) 2.5min, tank died unhealed). This helper,
// called FIRST in each dispatch path, re-asserts the fixed far-vertex goal and
// REFRESHES the commit TTL until the bot lands (<=6y), so a slow combat-contested
// crossing can neither expire nor be interrupted — the opener can't pin a mid-jump
// follower. Returns true (caller consumes the tick) while crossing; clears the
// commit and returns false on landing or when no commit is active. now_ms is the
// snapshot publish time. Definition in State_Idle.cpp.
bool DungeonHonorCross(BotSnapshotView const& s, BotAI& ai,
                       BotIntentEmitter& emit, uint32 now_ms);

// Following an owner who dropped or jumped to a floor 4+ y below (API::move_to follow_drop). Run at the top of every tick, whatever the
// bot's state: starts the follow when the owner is that far below and near, and while the bot is walking to a ledge or in the air it
// returns true so nothing else (a buff cast, a combat escape, the follow generator) can stop it. Clears itself on landing. Definition in
// State_InGroup.cpp.
bool HonorFollowDrop(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit);

// Stranded-follower recovery (shared idle / InGroup / InCombat). Relocates a
// non-tank follower that cannot rejoin the group on foot — a navmesh-disconnected
// perch, a void pocket, or a death that respawns it at the far in-instance entrance
// graveyard from which every step NoPaths — onto the rally point (the living tank,
// else the nearest living same-map member, so a dead tank is still recoverable).
// Two no-progress tiers off BotAI's best-distance clock: FAST on a confirmed-
// unreachable path verdict, SLOW (longer dwell) on a "reachable" path the per-state
// rejoin logic demonstrably cannot traverse. Hard-gated (alive non-tank, not
// casting, not in REAL combat — stalker-only false-combat excluded, > kStrandFar).
// Definition in State_Idle.cpp. Run FIRST in each dungeon dispatch path so a
// stranded member relocates in ANY state instead of oscillating the per-state
// rejoin rules forever while the group holds for it (the harbor sole-rezzer stall).
bool DungeonRecoverStrandedFollower(BotSnapshotView const& s, BotAI& ai,
                                    GroupSnapshotView const& g,
                                    BotIntentEmitter& emit, uint32 now_ms);

// Dungeon converge-to-fight. A non-tank bot NOT in combat while its tank IS in
// combat with a real victim closes to the tank (off-mesh aware) so the existing
// assist/opener engages the tank's victim. Fixes the 37-40y dead-band where a
// lagging DPS can neither rejoin (>40y) nor strand-relocate (>40y) yet cannot
// reach the tank's mob on its own, leaving the tank to SOLO a shielded mob it
// cannot finish (the 30-min Helix-approach Envoker deadlock). Definition in
// State_Idle.cpp. Run right AFTER strand-recovery in each dungeon dispatch path:
// strand handles the >40y unreachable TELEPORT, this the reachable pull-in.
bool DungeonConvergeToFight(BotSnapshotView const& s, BotAI& ai,
                            GroupSnapshotView const& g,
                            BotIntentEmitter& emit, uint32 now_ms);

// A raid tank that is not in combat yet but whose PvE plan names an engaged mob (PveOrder::tank_target) goes and takes it.
// The in-combat rule does this for a tank already fighting; an off tank that sat out the opening (MC Firelord wipe 2026-10-07:
// two off tanks at 100% health with no victim for a minute while the second Firelord killed the healers) never reached it.
bool RaidDutyTankEngage(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit, uint32 now_ms);

// The tank a non-tank bot anchors its group position on. In a raid the PvE
// coordinator pins each healer to one tank (PveOrder::heal_focus) and may send
// that tank after its own mobs well away from the main tank; while the focus
// tank is online, alive and on this map it is the anchor, so the cohesion /
// converge / rejoin rules agree with rule (5c) raid:healer_to_focus instead of
// pulling the healer back to g.tank() (the ping-pong). Otherwise g.tank().
// Definition in State_Idle.cpp.
GroupMemberSummary const* GroupAnchorTank(BotSnapshotView const& s, BotAI const& ai,
                                          GroupSnapshotView const& g);

// Ground-hazard step-out ([raid_hazard]). A non-tank inside a hostile damaging
// ground zone (snapshot combat.ground_hazards, radius + 1y) walks to the
// nearest walkable point outside every zone, toward its anchor tank / the
// group. Instances only, in and out of combat. Returns true when it emitted
// or holds a step already under way (caller consumes the tick). Definition
// in State_Idle.cpp.
// traps_only: only when the zone the bot stands in is a trap game object.
bool RaidHazardStepOut(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g,
                       BotIntentEmitter& emit, bool traps_only = false);

// Ground-hazard detour ([raid_hazard_detour]). A melee bot outside every ground
// hazard whose line to its victim crosses a trap zone walks to a point beside it first.
// Instances only. Returns true when it emitted or holds that walk (caller
// consumes the tick). Definition in State_Idle.cpp.
bool RaidHazardDetour(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit);

// The generic stand-point filter: (x, y, z) lies in a hostile ground hazard
// (snapshot combat.ground_hazards, same floor) or in a raid-layer avoid circle
// (PveOrder::avoid_circles), each with the 2y margin. Every generic move that
// picks a point (here: State_InCombat's loose-mob escape, healer follow /
// gap-close, melee gap-close) rejects such a point. Definition in State_Idle.cpp.
bool RaidStandPointBlocked(BotSnapshotView const& s, float x, float y, float z);

// In-fight raid formation: a ranged DPS / healer re-takes its own arc slot behind the anchor tank once the tank has moved it more
// than a few yards away (the boss pulled back or repositioned). True when it emitted a move or holds one in flight. Definition in
// State_Idle.cpp.
bool RaidFormationHold(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit);

// The raid formation, not the group follow, places this bot: a ranged DPS / healer of a raid (or raid group) whose anchor tank is
// fighting, whether or not the bot itself is in combat. Definition in State_Idle.cpp.
bool RaidFormationOwnsMovement(BotSnapshotView const& s, BotAI const& ai, GroupSnapshotView const& g);

// Playerbot.Behavior.SpreadOut: this melee bot's own stand spot on an arc round the back / sides of `victim`
// (rank = guid order among the group's melee on the same victim, evenly spaced, plus stable per-bot angle and
// radial jitter, inside melee reach). False when the key is off or every candidate is in a hazard / avoid
// circle - the caller keeps its old point. Definition in State_InCombat.cpp.
bool MeleeSpreadSpot(BotSnapshotView const& s, GroupSnapshotView const& g, NearbyUnit const& victim,
                     float& ox, float& oy);

// Raid only: (x, y, z) lies deeper inside an unpulled (idle) mob's aggro reach
// + margin than the bot stands now. Every raid move that picks a point (spacing,
// hazard / cone / LoS steps, the loose-mob escape, healer follow / gap-close)
// rejects such a point, so no step of ours walks into the next pack (MC
// 2026-10-06: Lava Annihilator and Firelord joined a Molten Giant pull).
// Definition in State_Idle.cpp.
// exempt_victim_pack false: the pack of the bot's own victim counts too (a caster's approach step must not walk into the
// neighbours of the mob it is after).
bool RaidPointInUnpulledReach(BotSnapshotView const& s, float x, float y, float z, bool exempt_victim_pack = true);

// (x, y, z) lies inside an unpulled (idle) mob's aggro reach + margin, whatever the bot's own position (the world-thread planners
// that pick a spot for the whole group: PveGroupCoordinator::PlanSightPull). Instances only. Definition in State_Idle.cpp.
bool PointInUnpulledPackReach(BotSnapshotView const& s, float x, float y, float z);

// Line-of-sight pull (PveOrder::sight_pull, PveGroupCoordinator::PlanSightPull): a bot of the group farther than a few yards from the
// spot walks there (a cast in progress goes out first). True when the walk owns the tick. In combat (State_InCombat SightPullTick)
// and out of it (idle:sight_pull, a member not yet hit). Definition in State_InCombat.cpp.
bool SightPullGather(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit, bool in_combat_state);

// A spot on the victim's level that sees it and is in reach for the role (the melee ring, or 25-10 y out for a ranged bot);
// pack_safe also keeps it out of unpulled packs' reach (RaidPointInUnpulledReach). Emits the run there and returns the spot and
// its distance from the victim. Definition in State_InCombat.cpp (used by [idle_reposition] and the opener's blocked approach).
bool SightSpotMove(BotSnapshotView const& s, NearbyUnit const& victim, bool melee, bool pack_safe, BotIntentEmitter& emit,
                   float& ox, float& oy, float& oz, float& oradius);

// Instance LoS step ([raid_los]). A ranged / healer bot whose cast target
// (healer: its anchor tank; ranged: its victim) is out of line of sight steps
// ~10y toward the anchor tank or around the obstacle, clear of the melee
// splash and ground hazards. Returns true when it emitted a step. Definition
// in State_Idle.cpp; called from DispatchInCombat.
bool RaidCastLosStep(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g,
                     BotIntentEmitter& emit);

// Shared BG carrier-homeward resolution (BUG BG-P0a). When the bot is
// carrying the flag / an orb, resolve the capture destination (static own
// flag pedestal, else closest owned node, else Kotmogu home_base) and emit a
// run move_to toward it. Returns true if a homeward move was emitted (the
// caller should suppress the melee gap-close so the carrier keeps running
// home instead of chasing its attacker). Returns false when the bot is not a
// carrier or no destination can be resolved.
bool BgCarrierHomeward(BotSnapshotView const& s, BotAI& ai,
                       BotIntentEmitter& emit,
                       BattlegroundAdvice const& bg_advice);

// Shared BG objective-GO interaction (BG audit S1). Scans for the nearest
// opted-in objective GameObject (auto_use_go_types — flag pedestals 36,
// capture points 42, flagstands/orbs 24 — plus auto_use_go_entries for the
// banner BGs whose nodes are generic BUTTON/GOOBER types), skips own fully-
// held nodes, dismounts if needed, walks the dead-zone (5-12y), and emits
// use_game_object within 5y (3s per-GO retry lockout). Returns true if it
// emitted an approach / dismount / use (the caller consumes the tick).
//
// Extracted from the idle auto-use loop so it can ALSO run in combat: the
// idle caller gates on !in_combat(); the State_InCombat caller does NOT, so
// a bot fighting over a defended WSG flag / contested AB-EotS node can still
// click it (server allows BG-object use in combat — Player.cpp
// CanUseBattlegroundObject has no combat check; modern caps are instant, not
// channeled). The defended-flagroom combat-gate was the #1 "no bot picks up
// a flag" blocker. is_alive / casting gating is the caller's responsibility.
bool BgTryUseObjectiveGo(BotSnapshotView const& s, BotAI& ai,
                         BotIntentEmitter& emit,
                         BattlegroundAdvice const& bg_advice);

// Shared BG endgame "push to the boss" mover (AV captain / general; reusable for
// any far same-map objective). The enemy captain/general sits 1400-2300y across
// the AV valley from the home base — far beyond the navmesh PathGenerator's
// partial-path cap (~74 polys / ~292y), so a single move_to (or a straight-line
// ChunkedWalkToward over river/bridge terrain) never advances and the push
// squad froze at the 1-tower lead. This leapfrogs the bot through the script's
// node chain (the GYs + towers, spaced ~150-250y along the valley road): it
// hops to the forward-most node within one navmesh-routable leg that makes net
// progress toward the target, repeating each tick until within kDirect of the
// boss, then emits a full move_to so PathGenerator routes the final approach.
// Returns true if it emitted a move (caller consumes the tick); false only if
// already on top of the target (let the caller's engage logic open on it).
bool BgPushThroughNodes(BotSnapshotView const& s, BotAI& ai,
                        BotIntentEmitter& emit,
                        BattlegroundAdvice const& bg_advice,
                        float tx, float ty, float tz, uint32 now_ms);

// AV walled-boss ENTRANCE staging. The enemy captains/generals sit inside
// walled garrisons/keeps whose interior is navmesh-reachable ONLY through a
// directional entrance — the attackers' approach side dead-ends at the
// perimeter wall (Detour returns a partial; bots pile at the wall, no LoS, 0
// damage). Routing a no-LoS pusher to a LoS-CLEAR INTERIOR staging point just
// inside the real entrance funnels it through the doorway instead of piling at
// the wall; once inside it gains LoS and the engage-on-LoS rule fires. Returns
// true + fills the staging point for a known boss entry, false otherwise (caller
// keeps its direct-to-boss behavior). Coords are interior LoS-clear polys
// verified by mmap_probe + the [avlos] LoS probe.
bool BgBossStagingPoint(uint32 entry, float& sx, float& sy, float& sz);

// Pet-assist target selection (peel-before-assist). Walks pet_attackers for
// a mob nuking the pet while the pet is busy elsewhere; otherwise falls back
// to the pet's own victim. Returns Empty unless the chosen target is a live
// hostile visible in the bot's nearby_enemies. Defined in Rules/MaintainRules.cpp.
// Shared so State_InCombat can assist the pet too: when a pet pulls, the
// owner is flagged in-combat immediately, which routes ticks to
// DispatchInCombat and makes the idle:assist_pet rule (gated on !in_combat)
// unreachable — leaving a pet-class bot standing in combat with no victim.
ObjectGuid SelectAssistPetTarget(BotSnapshotView const& s);

bool MaintainSelfBuff(BotSnapshotView const& s, GroupSnapshotView const& g,
                      BotIntentEmitter& emit);
bool MaintainSoulstone(BotSnapshotView const& s, GroupSnapshotView const& g,
                       BotIntentEmitter& emit);
// g: the bot's group (Classic warlock pet choice: Felhunter duty); nullptr = solo choice.
bool MaintainPet(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit,
                 GroupSnapshotView const* g = nullptr);
bool MaintainGroupUtility(BotSnapshotView const& s, GroupSnapshotView const& g,
                          BotIntentEmitter& emit);
bool MaintainConjuredItem(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit, uint32 now_ms);
bool MaintainOocFood(BotSnapshotView const& s, BotAI const& ai, BotIntentEmitter& emit);
bool MaintainOocDispel(BotSnapshotView const& s, GroupSnapshotView const& g,
                       BotIntentEmitter& emit);
bool MaintainAutoEquipUpgrades(BotSnapshotView const& s, BotAI& ai,
                               BotIntentEmitter& emit);
bool MaintainBagUpgrade(BotSnapshotView const& s, BotAI& ai,
                        BotIntentEmitter& emit);
bool MaintainContextTalents(BotSnapshotView const& s, BotAI& ai,
                            BotIntentEmitter& emit);
bool MaintainStarterTalents(BotSnapshotView const& s, BotAI& ai,
                            BotIntentEmitter& emit);
bool MaintainStarterTalentsExtend(BotSnapshotView const& s, BotAI& ai,
                                  BotIntentEmitter& emit);
bool MaintainOocHeal(BotSnapshotView const& s, GroupSnapshotView const& g,
                     BotIntentEmitter& emit, Role effective_role);
// Raid out-of-combat rez for State_InGroup (raid bots out of combat do not run the idle cascade): owner first, cast in range,
// otherwise the two nearest rezzers walk up. True = tick consumed. Definition in State_Idle.cpp.
bool RaidOocRez(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit);

bool MaintainOocRez(BotSnapshotView const& s, GroupSnapshotView const& g,
                    BotIntentEmitter& emit);

// Dungeon-run loot upkeep. The dungeon dispatch's tail hold swallows every lower-priority idle
// rule, which left an active 5-man / raid run without loot-roll votes (the human's roll window
// waited out its timer on the bots) and without corpse looting. These run the same gate + fire
// pair as idle:loot_roll (OocConsumableRules.cpp) and idle:loot_drain (LootDrainRules.cpp);
// the drain additionally waits for the fight to be over (nobody in the group in combat).
bool DungeonLootRoll(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g,
                     BotIntentEmitter& emit);
bool DungeonLootDrain(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g,
                      BotIntentEmitter& emit);

// Dispatchers — extracted from the inline `if (ai.dungeon_active())` /
// `if (ai.bg_active() && s.in_battleground())` blocks in State_Idle.cpp
// (REFACTOR_3 pass 16). Each runs the corresponding sub-rule cascade and
// returns true when any sub-rule fired (consuming the tick).
bool DungeonDispatch(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const& g,
                    BotIntentEmitter& emit);
bool BgDispatch(BotSnapshotView const& s, BotAI& ai,
                GroupSnapshotView const& g,
                BotIntentEmitter& emit);
bool AutoactDispatch(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const& g,
                    BotIntentEmitter& emit);

// Owner "pull back" order (BotAI::PullBackPlan): ranged opener on the target, the run back to the
// return point with the mob following, then the hold there. Defined in PrologueRules.cpp; run by
// idle:pull_back and at the top of DispatchInCombat. True = consumed.
// Owner "fallback" order (BotAI::FallbackPlan): the run to the rally point, then the re-formed
// hold there (fight only what follows). Defined in PrologueRules.cpp; run by idle:fallback and
// at the top of DispatchInCombat. `in_combat_state` only names the rule tags. True = consumed.
bool PullBackTick(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g,
                  BotIntentEmitter& emit, bool in_combat_state);
bool FallbackTick(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g,
                  BotIntentEmitter& emit, bool in_combat_state);

// Extracted travel executors (REFACTOR_3 follow-up) — defined in
// State_Idle.cpp, reused by idle:far_same_map_travel (FarTravelRules.cpp).
// quest_goal: the goal is the bot's current quest objective. Only then does a
// cross-map goal the graph cannot route blacklist that objective (5 min) so
// the picker rotates; a vendor / flight master / group rejoin / RPG trip goal
// passes false and never touches the quest picker.
bool DriveTravelPlanTo(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit,
                       float bx, float by, float bz,
                       uint32 to_map, float tx, float ty, float tz, bool quest_goal);
// The planting half of DriveTravelPlanTo: plans a route to the goal (graph A*,
// or the builder's validated bridge route) unless one is already planted for it.
void EnsureTravelPlan(BotSnapshotView const& s, BotAI& ai,
                      float bx, float by, float bz,
                      uint32 to_map, float tx, float ty, float tz, bool quest_goal);
bool DriveRecommendedTaxi(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit,
                          float bx, float by, float bz, bool* out_fell_through);

bool LegacyVendorDispatch(BotSnapshotView const& s, BotAI& ai,
                          GroupSnapshotView const& g,
                          BotIntentEmitter& emit);

// Shared chunked walk toward a same-map point (2026-06-21). Replaces the single
// long emit.move_to() used by the capital / repair / vendor / far-travel walks,
// which climbed onto ledges and stalled on canals/slopes without recovering.
// Steps in personality-scaled chunks with blocked-bearing deflection that
// ESCALATES on a position stall (ledge/corner escape — see BotAI::walk_stall_note)
// and yields (returns false) once a goal is genuinely unreachable this episode so
// stuck-recovery / the watchdog can act. Returns true if it emitted a step (the
// caller consumes the tick), false on arrival (<9y) or give-up. Near targets with
// no stall get a full move_to so PathGenerator navmesh-routes the last leg.
bool ChunkedWalkToward(BotSnapshotView const& s, BotAI& ai, BotIntentEmitter& emit,
                       float tx, float ty, float tz, char const* tag, uint32 now_ms);

} // namespace States
} // namespace Playerbot
