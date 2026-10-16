// PveGroupCoordinator — group-level dungeon/raid coordination.
//
// Counterpart of Bot/Battleground/BgTeamCoordinator for instanced PvE.
// Before this, group play relied on the tank hierarchy plus per-bot greed:
// every capable kicker burned its interrupt on the same mandatory cast
// (the "stagger" was a SELF-throttle, not a rotation), all healers triaged
// the same lowest-HP target, every bot walked into the same soak circle,
// DPS swapped to adds independently, and two bot tanks could pull
// different packs simultaneously. That roughly works at 5-man scale where
// the tank does the coordinating; in raids (10-30 bots) it reproduces the
// BG thundering herd — and even 5-mans waste kicks and stack soaks.
//
// This service computes ONE plan per GROUP on the WORLD THREAD every
// kPlanIntervalMs for groups with at least one V2 bot inside a dungeon or
// raid map, and publishes composable per-bot duties through the snapshot
// (BotSnapshot::PveOrder): main/off tank designation, an interrupt
// rotation (rank 0 kicks, rank 1 backs up, the rest HOLD), designated
// soakers, healer focus assignments, a synchronized kill target, and
// spread-bearing slots. `active == false` (or per-field sentinels) falls
// back to legacy behavior everywhere — graceful degradation by design.
//
// Threading contract (identical to BgTeamCoordinator): Update() is the only
// WRITER of orders_ and runs on the WORLD THREAD, inline in OnWorldUpdate
// BEFORE the parallel snapshot-build barrier (run_and_wait). OrderFor() is a
// pure const find() READER and is called concurrently from the Phase 4
// snapshot-build WORKER threads. This is safe ONLY because writer and readers
// are temporally disjoint: orders_ is never mutated during the build phase, and
// concurrent reads of a non-mutating unordered_map are well-defined. If Update()
// is ever moved to run concurrently with the build, orders_ needs a shared_mutex.
// Inputs are the published immutable GroupSnapshot / BotSnapshot shared_ptrs and
// DungeonScriptMgr::GetAdvice computed HERE from a member snapshot — never a
// BotAI-owned cache (those are bot-worker property; reading them cross-thread is a
// use-after-free, see the 2026-06-10 BG coordinator review).
//
// Human members are never ordered, but they OCCUPY duties: a human tank
// makes every bot tank an off-tank, a human healer shifts bot healers
// toward raid triage, and human DPS reduce how many bot soakers are
// drafted.

#pragma once

#include "../BotSnapshot.h"
#include <map>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

class Player;

namespace Playerbot {

struct GroupMemberSummary;

class PveGroupCoordinator
{
public:
    PveGroupCoordinator() = default;

    // World tick driver; internally throttled to the plan cadence.
    void Update(uint32 now_ms);

    // Order lookup for the snapshot builder. Returns nullptr when no
    // current plan covers the bot (consumer publishes a default-inactive
    // order and the bot logic runs legacy logic).
    PveOrder const* OrderFor(uint64 bot_guid_low) const
    {
        auto it = orders_.find(bot_guid_low);
        return it == orders_.end() ? nullptr : &it->second;
    }

    // Human-readable plan dump for `.playerbot pvecoord`.
    std::string DebugDump() const;

    // Raid layer hook (Bot/Raid/RaidBrain.cpp, boss tank swaps): hand <mob> to the bot tank <tank_low> in the raid
    // tank assignment. Takes effect at once (the published order) and sticks for later plans; the previous owner
    // loses the mob and is given another one by the next plan. World thread only, same contract as Update().
    void ReassignTankTarget(uint64 group_low, uint32 map_id, uint64 tank_low, ObjectGuid mob);

private:
    struct Member
    {
        uint64 guid_low = 0;
        bool   is_bot = false;
        bool   alive = false;
        uint8  cls = 0;
        uint16 spec = 0;
        bool   tank = false;
        bool   healer = false;
        bool   interrupter = false;
        // Census source row; valid for the duration of one Update() only.
        GroupMemberSummary const* src = nullptr;
    };

    // Per-(group, map) fight diagnostics. Logged at INFO so the default
    // `Logger.playerbot.v2=3` shows how a pull went: who tanked what, who
    // died to which mob, how healer mana held up.
    struct FightLog
    {
        bool   in_fight = false;
        uint32 started_ms = 0;
        uint32 last_summary_ms = 0;
        uint32 deaths = 0;
        // Members alive at the previous plan, and what was hitting each
        // one then ("Molten Giant 63%, ..."): the death line's evidence.
        std::unordered_map<uint64, bool> alive;
        std::unordered_map<uint64, std::string> hit_by;
        std::unordered_map<uint64, int32> hp_pct;
        // Raid damage meter for the [raid_fight] line: engaged mobs' last seen health, and the health they lost
        // since the previous summary (a mob that dies between two summaries is undercounted).
        std::unordered_map<uint64, int64> mob_hp;
        uint64 dmg_total = 0;
        uint32 dmg_last_ms = 0;
    };

    // Raid tank -> mob assignment for one (group, map) bucket, the generic
    // raid kill focus (when the dungeon script named no priority add,
    // `script_focus` empty; the group's skull / cross mark `kill_mark`
    // overrides the generic pick), plus the [raid_fight] / [raid_death] INFO
    // diagnostics. World thread, inside Update(); writes tank_target and
    // kill_focus into `plan`.
    void PlanRaidTanks(uint64 bkey, uint64 group_low, uint32 map_id, bool is_raid,
                       uint32 now_ms, uint64 main_tank, uint64 off_tank,
                       ObjectGuid script_focus, ObjectGuid kill_mark, bool idle_mark_is_call,
                       std::vector<Member> const& mem,
                       std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                       std::unordered_map<uint64, PveOrder>& plan);

    // The tank's pre-pull crowd-control request (Bot/Dungeon/PrePullCc.h): moon on the mob, the mage released for it.
    // Dungeons and raids, after PlanRaidTanks (whose mark cleanup leaves the requested moon alone). World thread.
    void PlanPrePullCc(uint64 bkey, uint64 group_low, uint32 map_id, uint32 now_ms,
                       std::vector<Member> const& mem,
                       std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                       std::unordered_map<uint64, PveOrder>& plan);

    // Raid-symbol orders (Playerbot.Behavior.TankMarkers / MarkCc), after PlanRaidTanks: the owner's icons on bots.
    // An icon on a bot is its job on the mob carrying the same icon - a DPS kills it first (kill_focus / mark_target),
    // a caster with a crowd control spell crowd-controls it (cc_target, never for the skull; released when the mob's icon
    // turns skull or goes), a duty tank in a 5-man holds it (tank_target; raids: PlanRaidTanks). The bot's chat `rti`
    // orders extend the icon. Writes mark_symbol / mark_target / cc_* into `plan`. World thread.
    void PlanMarkOrders(uint64 group_low, uint32 map_id, bool is_raid,
                        std::vector<Member> const& mem,
                        std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                        std::unordered_map<uint64, PveOrder>& plan);

    // Per-(group, map) automatic tank markers (tank guid-low -> marker index) and the raid icons this coordinator
    // set itself (marker -> mob), so it clears only its own marks and never a mark someone else put on a live mob.
    // Both reset when the pack is gone (PlanRaidTanks, Playerbot.Behavior.TankMarkers).
    std::unordered_map<uint64, std::unordered_map<uint64, uint8>> tank_mark_auto_;
    std::unordered_map<uint64, std::array<ObjectGuid, 8>> marks_set_;

    std::unordered_map<uint64, PveOrder> orders_;
    std::unordered_map<uint64, PveOrder> next_orders_;
    // Per-(group, map) plan signature for change-only logging.
    std::unordered_map<uint64, uint64>   plan_sig_;
    // Per-(group, map) kill-focus stickiness — coordinator state rather
    // than a read-back from member orders, which broke when the sampled
    // member left the group or the data ticked cold.
    std::unordered_map<uint64, ObjectGuid> last_kill_focus_;
    // Per-(group, map) raid tank -> mob assignment, sticky while the mob
    // lives so tanks don't trade mobs (and threat) every plan.
    std::unordered_map<uint64, std::unordered_map<uint64, ObjectGuid>> last_tank_target_;
    std::unordered_map<uint64, uint64> last_main_tank_;   // bucket -> guid low of the main tank, kept while that tank lives
    // Per-(group, map): the tank that held a mob when a group member's crowd control (Banish, Polymorph, Shackle) took it out of the
    // assignment -> that tank, until when. When the CC ends the same tank takes the mob back first (MC Garr 2026-10-12: banished
    // Firesworn came off and hit mages while the nearest tank was 27-57 y away).
    struct CcHold { uint64 tank_low = 0; uint32 until_ms = 0; };
    std::unordered_map<uint64, std::unordered_map<uint64, CcHold>> cc_held_;   // bucket -> mob guid counter -> hold
    // Per-(group, map) raid tank roster (guid lows, duty order): the tank-spec
    // members that act as tanks. Grows while the raid fights, shrinks back
    // out of combat; the other tank-spec bots play DPS (raid_dps_tank).
    std::unordered_map<uint64, std::vector<uint64>> raid_tank_roster_;
    // Per-(group, map) raid healer -> tank (guid lows), sticky across plans.
    std::unordered_map<uint64, std::unordered_map<uint64, uint64>> last_heal_focus_;
    // Per-(group, map) raid triage healer -> subgroup mask (RaidHealGroups.h), sticky across plans.
    std::unordered_map<uint64, std::map<uint64, uint16_t>> last_heal_groups_;
    // Per-(group, map) generic raid kill focus (PlanRaidTanks), sticky while
    // it stays a candidate so 30 DPS don't flap between two mobs.
    std::unordered_map<uint64, ObjectGuid> last_raid_focus_;
    // Per-(group, map) end (ms) of the owner's attack / pull release (Playerbot.Behavior.MarksNeedOrder): while it runs
    // raid icons on mobs nobody fights yet may start the fight (PveOrder::marks_released).
    std::unordered_map<uint64, uint32> mark_release_until_;
    std::unordered_map<uint64, uint32> fight_start_;   // first ms of the current fight (Playerbot.Raid.DpsWaitTimeoutMs)
    std::unordered_map<uint64, uint32> defend_since_;  // first ms a mob has stood on a non-tank member this fight (0 = none now)
    std::unordered_map<uint64, FightLog> fight_log_;
    // Interrupt rotation (Classic: live-ability ranks + per-casting-mob
    // assignment) and the hunter Tranquilizing Shot order. World thread,
    // inside Update(); write interrupt_* / tranq_rank into `plan`.
    void PlanInterrupts(std::vector<Member> const& mem,
                        std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                        std::unordered_map<uint64, PveOrder>& plan);
    void PlanTranq(uint64 bkey, std::vector<Member> const& mem,
                   std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                   std::unordered_map<uint64, PveOrder>& plan);
    // Per-(group, map) tranq order (hunter guid low -> rank), fixed when an
    // enrage appears and held while one lasts, so a hunter whose shot just
    // went on cooldown does not hand rank 0 to the next mid-flight.
    std::unordered_map<uint64, std::unordered_map<uint64, uint8>> tranq_ranks_;
    // Classic warlocks: who runs the Felhunter for Spell Lock (writes
    // PveOrder::felhunter_duty). Holders stay sticky while still capable;
    // free slots go to the next capable warlock by guid. World thread.
    void PlanFelhunters(uint64 bkey, std::vector<Member> const& mem,
                        std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                        std::unordered_map<uint64, PveOrder>& plan);
    std::unordered_map<uint64, std::vector<uint64>> felhunter_holders_;   // per (group, map) bucket
    // Warlock guid low -> level at which its Felhunter was seen out without
    // Spell Lock: not capable again until it levels (a Grimoire may follow).
    std::unordered_map<uint64, uint8> felhunter_no_lock_;
    // Classic raids: warlock curse assignment (PveOrder::curse_*) on the
    // engaged boss, else the kill focus. World thread (reads the mob's live
    // auras and immunities); [raid_curse] logs each assignment change.
    void PlanCurses(uint64 bkey, uint64 group_low, uint32 map_id, bool is_raid,
                    std::vector<std::pair<uint64, Player*>> const& bots,
                    std::vector<Member> const& mem,
                    std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                    std::unordered_map<uint64, PveOrder>& plan);
    // Classic raids: dwarf-priest Fear Ward on the main / off tank, the
    // [raid_fear] log when a fearing boss (Classic::BossFears) is engaged,
    // and the opt-in resistance potion order (Playerbot.Raid.ResistPotions).
    void PlanFearAndResist(uint64 bkey, uint64 group_low, uint32 map_id, bool is_raid,
                           uint64 main_tank, uint64 off_tank,
                           std::vector<std::pair<uint64, Player*>> const& bots,
                           std::vector<Member> const& mem,
                           std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                           std::unordered_map<uint64, PveOrder>& plan);
    // Per-(group, map) last logged curse (warlock low -> spell, target) and
    // fear prep (priest low -> tank low; the engaged fearing boss).
    std::unordered_map<uint64, std::unordered_map<uint64, std::pair<uint32, ObjectGuid>>> last_curse_;
    std::unordered_map<uint64, std::unordered_map<uint64, uint64>> last_fear_ward_;
    std::unordered_map<uint64, ObjectGuid> last_fear_boss_;
    std::unordered_map<uint64, uint32> last_resist_potion_;
    // Classic dungeons / raids, boss fight: Innervate assignment
    // (PveOrder::innervate_target) - healers under 30% mana, lowest first,
    // each to the nearest bot Balance / Restoration druid in range with
    // Innervate ready; one druid per healer. World thread (reads the healers'
    // live Innervate aura); [raid_cd] logs each new assignment.
    void PlanInnervates(uint64 bkey, uint64 group_low, uint32 map_id,
                        std::vector<Member> const& mem,
                        std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                        std::unordered_map<uint64, PveOrder>& plan);
    // Per-(group, map) last logged Innervate assignment (druid low -> healer low).
    std::unordered_map<uint64, std::unordered_map<uint64, uint64>> last_innervate_;
    uint32 last_plan_ms_ = 0;
    std::string last_dump_;
};

} // namespace Playerbot
