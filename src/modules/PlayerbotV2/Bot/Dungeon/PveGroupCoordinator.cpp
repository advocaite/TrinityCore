// PveGroupCoordinator implementation. See header for the design contract.

#include "PveGroupCoordinator.h"
#include "Bot/SpellFamily.h"
#include "Bot/DeathCause.h"
#include "DungeonScript.h"
#include "../BotRegistry.h"
#include "../BotCommandParser.h"
#include "../BotSnapshotView.h"
#include "../RaidFight.h"
#include "../ClassTables.h"
#include "../Raid/RaidBrain.h"
#include "../Raid/RaidEncounters.h"
#include "../Raid/KickRules.h"
#include "../Raid/RaidHealSlots.h"
#include "../Raid/RaidHealGroups.h"
#include "../ClassicUpkeep.h"
#include "../ThreatLimits.h"
#include "MarkCc.h"
#include "PrePullCc.h"
#include "Combat/Apl/ApInterrupt.h"
#include "../../Group/GroupSnapshot.h"
#include "../../Fleet/BotIdentityRegistry.h"
#include "../../Fleet/OwnerRegistry.h"
#include "../../Services.h"
#include "../../Threading/SnapshotPublisher.h"

#include "Config.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "Creature.h"
#include "GameTime.h"
#include "CreatureData.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RaceMask.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "WorldSession.h"
#include "InstanceScript.h"
#include "PathGenerator.h"
#include "Pet.h"
#include "PlayerbotMovement.h"
#include "Bot/States/MaintainHelpers.h"   // PointInUnpulledPackReach: the line-of-sight pull spot
#include "Diagnostics/ScenarioHuman.h"   // Scenario::PlaysAsHuman: a player-led scenario's stand-in counts as the player

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace Playerbot {

namespace {

// Plan cadence. PvE duty assignments (tank roles, interrupt ranks, healer
// focus) are stable for whole pulls; the only fast-moving output is the
// synchronized kill target, and a 500ms refresh after an add dies is well
// inside human reaction time.
constexpr uint32 kPlanIntervalMs = 500;

// How long a spoken Attack counts as "just given" for the group's release (the release then stands for the fight, see the latch below).
constexpr uint32 kOwnerAttackWindowMs = 10000;

using V2::OwnerAttackRecent;

// Damage done to creatures per player (pets count for their owner), for the [raid_dps] line. Written from the map threads, read and
// reset on the world thread at the start and end of a pull.
std::mutex g_dmg_done_mtx;
std::unordered_map<uint64, uint64> g_dmg_done;

uint64 TakeDamageDone(uint64 guid_low, bool reset)
{
    std::lock_guard lk(g_dmg_done_mtx);
    auto it = g_dmg_done.find(guid_low);
    if (it == g_dmg_done.end()) return 0;
    uint64 const v = it->second;
    if (reset) g_dmg_done.erase(it);
    return v;
}

char const* ShortClass(uint8 cls)
{
    switch (cls)
    {
        case CLASS_WARRIOR: return "War";   case CLASS_PALADIN: return "Pal";  case CLASS_HUNTER: return "Hun";
        case CLASS_ROGUE:   return "Rog";   case CLASS_PRIEST:  return "Pri";  case CLASS_SHAMAN: return "Sha";
        case CLASS_MAGE:    return "Mag";   case CLASS_WARLOCK: return "Wlk";  case CLASS_DRUID:  return "Dru";
        default:            return "?";
    }
}

} // namespace

void PveGroupCoordinator::NoteDamageDone(Unit* attacker, Unit* victim, int32 amount)
{
    if (!attacker || !victim || amount <= 0 || victim->IsPlayer()) return;
    Player* owner = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!owner) return;
    std::lock_guard lk(g_dmg_done_mtx);
    if (g_dmg_done.size() > 4096) g_dmg_done.clear();
    g_dmg_done[owner->GetGUID().GetCounter()] += uint64(amount);
}

void PveGroupCoordinator::Update(uint32 now_ms)
{
    if (now_ms - last_plan_ms_ < kPlanIntervalMs)
        return;
    last_plan_ms_ = now_ms;

    if (!sConfigMgr->GetBoolDefault("Playerbot.Pve.Coordinator.Enable", true, true))
    {
        if (!orders_.empty()) orders_.clear();
        last_dump_ = "coordinator disabled (Playerbot.Pve.Coordinator.Enable=0)";
        return;
    }

    // -- Bucket every grouped, instanced bot by (group, map) -----------------
    // The map is part of the key: a group can momentarily span TWO dungeon
    // maps (laggards still zoning out of the previous instance). One plan
    // per (group, map) keeps every duty holder physically able to act —
    // the review found a single per-group plan could hand main-tank /
    // soaker / rank-0 duties to members in a different instance.
    struct Bucket
    {
        uint64 group_low = 0;
        uint32 map_id = 0;
        std::vector<std::pair<uint64, Player*>> bots;
        bool in_combat = false;     // a living member bot is in combat (sustains the release latch)
        bool owner_order = false;   // a member bot carries an owner attack / pull order (MarksNeedOrder release)
        std::vector<uint64> pullers;   // bots running an owner "pull back": released for their own pull only, not the group
        std::unordered_map<uint64, BotAI*> ais;   // the line-of-sight pull reads and moves the tank's pull back (PlanSightPull)
    };
    std::unordered_map<uint64, Bucket> buckets;
    Services::Registry().for_each([&](BotId id, BotRegistryEntry const& e)
    {
        if (!e.ai) return;
        Player* p = ObjectAccessor::FindConnectedPlayer(
                        ObjectGuid::Create<HighGuid::Player>(id));
        if (!p) return;
        Group const* grp = p->GetGroup();
        if (!grp) return;
        // Dungeons AND raids (Map::IsDungeon covers both); the open world
        // keeps the legacy behavior — coordination there is the BG
        // coordinator's job (battlegrounds) or unnecessary (quest mobs).
        Map* m = p->GetMap();
        if (!m || !m->IsDungeon()) return;
        const uint64 glow = grp->GetGUID().GetCounter();
        const uint64 key  = (glow << 16) ^ uint64(m->GetId());
        Bucket& b = buckets[key];
        b.group_low = glow;
        b.map_id    = m->GetId();
        b.bots.emplace_back(uint64(id), p);
        b.ais[uint64(id)] = e.ai.get();
        if (p->IsAlive() && p->IsInCombat())
            b.in_combat = true;
        // The owner's go (MarksNeedOrder): an attack / engage order, a /focus pin or a pull back running on a member bot.
        // A pull back is NOT the group's go (playtest, MC Molten Giants 2026-10-08: four marked tanks pulled back and the whole raid
        // ran in): only the pulling tank is released; the others wait until the mob is on a tank.
        if (e.ai->pull_back_active())
            b.pullers.push_back(uint64(id));
        else if ((!e.ai->focus_target().IsEmpty() ||
                  (e.ai->is_manual_mode(now_ms) && e.ai->owner_command() == BotAI::OwnerCommand::Engage)) &&
                 // A Pull is the tanks' own engage: it must not release the rest of the raid (playtest, MC Gehennas
                 // 2026-10-08: "pull, then I say attack"; a party's Pull waits for the owner's Attack too). Only a non-tank bot carrying the order is the owner's Attack.
                 BotCommandParser::BotRole(p) != Role::Tank)   // raid and 5-man alike: Pull never releases the others
            b.owner_order = true;
        // The spoken Attack, however the pull gate treated this bot (only the main tank carries Engage while the mob is idle).
        if (OwnerAttackRecent(now_ms, e.ai->owner_attack_ms(), kOwnerAttackWindowMs))
            b.owner_order = true;
    });

    next_orders_.clear();
    std::ostringstream dump;

    for (auto& [bkey, b] : buckets)
    {
        // Shared group snapshot: one per group per tick, covers EVERY
        // member including humans (roles, positions, vitals).
        // The first bot that has one: a bucket's first bot can be a corpse or a ghost whose group snapshot is gone, and the
        // whole raid plan (tanks, curses, [raid_fight]) went silent for the rest of the fight (Ragnaros 2026-10-13: the fight
        // lines stopped at 103 s of 249 s, after the first deaths).
        std::shared_ptr<GroupSnapshot const> gs;
        for (auto const& [blow, p] : b.bots)
        {
            gs = Services::Snapshots().latest_group(blow);
            if (gs && !gs->members.empty())
                break;
        }
        if (!gs || gs->members.empty())
        {
            static uint32 s_cold_log_ms = 0;
            if (b.in_combat && now_ms - s_cold_log_ms >= 5000u)
            {
                s_cold_log_ms = now_ms;
                TC_LOG_INFO("playerbot.v2", "[pve_plan] group={} map={} skipped: none of its {} bots has a group snapshot, the raid plan does not run",
                    b.group_low, b.map_id, uint32(b.bots.size()));
            }
            continue;   // group data cold — legacy runs
        }

        // Reference bot snapshot: freshest member view; provides
        // nearby_enemies for the synchronized kill target and the view
        // GetAdvice reads. Same world-thread-computed-advice pattern as
        // the BG coordinator — never read a BotAI's advice cache here.
        std::shared_ptr<BotSnapshot const> ref;
        std::vector<std::shared_ptr<BotSnapshot const>> snaps;
        snaps.reserve(b.bots.size());
        for (auto const& [blow, p] : b.bots)
        {
            auto s = Services::Snapshots().latest(blow);
            if (!s) continue;
            snaps.push_back(s);
            if (!ref || s->version > ref->version)
                ref = s;
        }
        if (!ref)
            continue;
        // The raid plan (tank roster, off tank, mob assignment, curses, fear) is for a raid group on a raid map (20 and 40 players) or in
        // the open world (a world boss). A raid group in a dungeon plays it as a dungeon: UBRS is a 10-man, and its raid plan held both
        // tanks on an assigned mob while six Incarcerators sat loose on the casters for 18 s (UBRS 2026-10-28 22:16; the owner: "UBRS is
        // a 10-man, I wouldn't classify that as a raid ... anything above 20-man, you should start falling into your raid tactics").
        const bool raid_rules = gs->is_raid && ((ref->instance_ctx.is_in_raid && (ref->instance_ctx.max_players == 0 ||
                                    ref->instance_ctx.max_players >= BotSnapshotView::kRaidTacticsMinPlayers)) || !ref->instance_ctx.is_in_instance);
        DungeonAdvice const advice =
            Services::Dungeons().GetAdvice(BotSnapshotView(*ref));

        // -- Census ----------------------------------------------------------
        // Humans first, then bots, each tier ordered by guid: a human tank
        // claims main-tank duty, a human healer claims the tank-heal slot,
        // and duty assignment stays deterministic across plans.
        //
        // Two presence semantics from the review:
        //  * MAP SCOPE: only members physically on the bucket's instance
        //    map hold duties. A tank corpse-running outside the portal
        //    must not keep main-tank (which gated the inside tank out of
        //    every pull until they zoned back in).
        //  * TRUE DEATH STATE: m.is_alive (Unit::IsAlive), NOT hp > 0 —
        //    a released ghost has hp == 1 and would otherwise keep its
        //    duty for the whole graveyard run (ghost rank-0 kicker =
        //    nobody interrupts; ghost soakers = circles unsoaked).
        std::vector<Member> mem;
        mem.reserve(gs->members.size());
        auto& lifecycle = Services::Lifecycle();
        for (auto const& m : gs->members)
        {
            if (!m.online) continue;
            if (m.map_id != b.map_id) continue;
            Member e;
            e.guid_low = m.guid.GetCounter();
            e.is_bot   = lifecycle.is_bot(e.guid_low);
            e.alive    = m.is_alive;
            e.cls      = m.cls;
            e.spec     = uint16(m.spec);
            // One duty per member: a declared Healer (the slot role a bot publishes from its role pin) never counts as a
            // tank on a spec reading, nor a declared Tank as a healer; the spec only decides for a DPS-roled member
            // (the spare tanks the roster below demotes publish DPS). Only the owner's Main Tank flag overrides a Healer.
            e.tank     = m.role == Role::Tank   || m.is_main_tank || (m.role != Role::Healer && IsTankSpec(m.cls, e.spec));
            e.healer   = !e.tank && (m.role == Role::Healer || (m.role != Role::Tank && IsHealerSpec(m.cls, e.spec)));
            e.interrupter = !e.healer && ClassInterrupt(m.cls, m.spec) != 0;
            e.src      = &m;
            mem.push_back(e);
        }
        std::sort(mem.begin(), mem.end(), [](Member const& a, Member const& c)
        {
            if (a.is_bot != c.is_bot) return !a.is_bot;   // humans first
            return a.guid_low < c.guid_low;
        });
        if (mem.empty())
            continue;

        // -- Duty assignment ---------------------------------------------------
        // Start every BOT member with an active, otherwise-empty order:
        // `active` alone already suppresses the legacy herd paths that
        // need an explicit assignment to act (soak stays governed by the
        // `soaker` field below).
        // Raid icons only plan until the owner orders the attack (latched 10 s past the last sign of the order).
        if (b.owner_order)
            mark_release_until_[bkey] = now_ms + 10000u;
        // The Attack stands for the whole fight: while the group is in combat a live release is carried on, so it neither
        // lapses when the owner dies or his order expires, nor re-holds the DPS mid-fight (playtest, MC Gehennas 2026-10-08:
        // rogues idle after the owner fell). It ends 10 s after the fight.
        else if (b.in_combat && mark_release_until_.count(bkey) && now_ms < mark_release_until_[bkey])
            mark_release_until_[bkey] = now_ms + 10000u;
        // Optional: a forgotten Attack must not wipe the raid. Once the fight has run Playerbot.Raid.DpsWaitTimeoutMs without
        // the order, the DPS go (the release then stands for the fight like a real Attack).
        if (b.in_combat)
        {
            uint32& started = fight_start_[bkey];
            if (!started)
                started = now_ms;
            // A raid waits as long as configured (up to eight seconds: the tank takes a while to run in); a party keeps its six.
            uint32 const wait_ms = raid_rules ? Services::Config().raid_dps_wait_timeout_ms()
                                               : std::min<uint32>(Services::Config().raid_dps_wait_timeout_ms(), kPartyDpsWaitTimeoutMs);
            if (wait_ms && now_ms - started >= wait_ms && !(mark_release_until_.count(bkey) && now_ms < mark_release_until_[bkey]))
            {
                mark_release_until_[bkey] = now_ms + 10000u;
                TC_LOG_INFO("playerbot.v2", "[raid_threat] group {} fought {} s without the owner's Attack: the DPS are released", bkey,
                            (now_ms - started) / 1000u);
            }
        }
        else
            fight_start_.erase(bkey);
        // Defence: the DPS wait for the Attack to open a pull, not to watch a mob chew on a healer or a caster. Once an engaged mob has
        // stood on a non-tank member for kDefendMs of the fight and the Attack has not come, the order is released as if given (MC Garr
        // 2026-10-10: adds engaged at placement, a Firesworn killed a healer 4 s in, and the casters held their damage until the order
        // at 20 s). Stands for the fight like a real Attack; the tanks still decide who holds what.
        if (b.in_combat)
        {
            constexpr uint32 kDefendMs = 2500;
            bool on_non_tank = false;
            for (auto const& sn : snaps)
            {
                if (sn->position.map_id != b.map_id) continue;
                for (auto const& u : sn->combat.nearby_enemies)
                {
                    if (u.hp <= 0 || u.is_player || u.untargetable || u.is_pacified || !u.unit_in_combat || u.victim.IsEmpty()) continue;
                    if (u.is_cc_locked) continue;
                    for (auto const& e : mem)
                        if (e.src->guid == u.victim && !e.tank)
                            on_non_tank = true;
                    if (on_non_tank) break;
                }
                if (on_non_tank) break;
            }
            uint32& since = defend_since_[bkey];
            if (!on_non_tank)
                since = 0;
            else if (!since)
                since = now_ms;
            else if (now_ms - since >= kDefendMs && !(mark_release_until_.count(bkey) && now_ms < mark_release_until_[bkey]))
            {
                mark_release_until_[bkey] = now_ms + 10000u;
                TC_LOG_INFO("playerbot.v2", "[raid_threat] group {} has had a mob on a non-tank member for {} ms without the owner's Attack: the DPS are released",
                            bkey, now_ms - since);
            }
        }
        else
            defend_since_.erase(bkey);
        const bool marks_released = !Services::Config().behavior_marks_need_order() ||
                                    (mark_release_until_.count(bkey) && now_ms < mark_release_until_[bkey]);
        std::unordered_map<uint64, PveOrder> plan;
        for (auto const& e : mem)
            if (e.is_bot)
            {
                PveOrder o;
                o.active = true;
                o.marks_released = marks_released ||
                    std::find(b.pullers.begin(), b.pullers.end(), e.guid_low) != b.pullers.end();
                plan[e.guid_low] = o;
            }
        auto bot_order = [&](uint64 g) -> PveOrder*
        {
            auto it = plan.find(g);
            return it == plan.end() ? nullptr : &it->second;
        };

        // -- Raid tank roster ---------------------------------------------------
        // A raid acts with as many tanks as it has mobs to hold, plus a swap
        // tank: max(3, engaged mobs + 1). The tank-spec bots beyond that play
        // DPS (raid_dps_tank) and count as DPS for every duty below, so the
        // healers, the mob assignment and the raid layer see only the roster
        // (MC 2026-10-06: 13 tank-spec bots all tanked one Molten Giant and
        // the healers were split over them). Sticky: roster members stay
        // while they live and still tank; during a fight the roster only
        // grows (a new add gets a tank, nobody drops a mob mid-pull), out of
        // combat it shrinks back to three. Humans who tank are always on it.
        if (raid_rules)
        {
            std::vector<ObjectGuid> engaged;
            for (auto const& sn : snaps)
            {
                if (sn->position.map_id != b.map_id) continue;
                for (auto const& u : sn->combat.nearby_enemies)
                {
                    if (u.hp <= 0 || u.is_player || u.untargetable || u.is_pacified || !u.unit_in_combat) continue;
                    if (Raid::NoTank(b.map_id, u.entry)) continue;   // a summoned add the DPS burn down
                    if (Raid::GetBrain().AllSchoolsAvoided(b.group_low, b.map_id, u.guid)) continue;   // immune to everything: no tank needed
                    ThreatVictim const k = ThreatVictim(u.victim_kind);
                    if (k != ThreatVictim::Me && k != ThreatVictim::Tank &&
                        k != ThreatVictim::Member && k != ThreatVictim::MemberPet)
                        continue;
                    if (std::find(engaged.begin(), engaged.end(), u.guid) == engaged.end())
                        engaged.push_back(u.guid);
                }
            }
            const size_t want = std::max<size_t>(3, engaged.size() + 1);
            auto live_tank = [&](uint64 low)
            {
                for (auto const& e : mem)
                    if (e.guid_low == low)
                        return e.tank && e.alive;
                return false;
            };
            auto& roster = raid_tank_roster_[bkey];
            std::vector<uint64> next;
            auto on_next = [&](uint64 low) { return std::find(next.begin(), next.end(), low) != next.end(); };
            for (auto const& e : mem)
                if (e.tank && e.alive && (!e.is_bot || e.src->is_main_tank))
                    next.push_back(e.guid_low);
            for (uint64 low : roster)
                if (live_tank(low) && !on_next(low) && (!engaged.empty() || next.size() < want))
                    next.push_back(low);
            for (auto const& e : mem)
                if (next.size() < want && e.tank && e.alive && !on_next(e.guid_low))
                    next.push_back(e.guid_low);
            if (next != roster)
            {
                std::string names;
                for (uint64 low : next)
                    for (auto const& e : mem)
                        if (e.guid_low == low) { names += (names.empty() ? "" : ", ") + e.src->name; break; }
                TC_LOG_INFO("playerbot.v2",
                    "[raid_tank] group={} map={} tank roster ({} engaged mobs, want {}): {}",
                    b.group_low, b.map_id, engaged.size(), want, names.empty() ? std::string("-") : names);
            }
            roster = std::move(next);
            for (auto& e : mem)
                if (e.tank && e.is_bot && std::find(roster.begin(), roster.end(), e.guid_low) == roster.end())
                {
                    e.tank = false;
                    if (auto it = plan.find(e.guid_low); it != plan.end())
                        it->second.raid_dps_tank = true;
                }
        }

        // Tanks: first (human-preferred) = main. The OFF-tank duty — a
        // dedicated second tank that shadows the main and taunts on swap
        // triggers — is a RAID concept ONLY. A classic 5-man runs
        // 1 tank / 1 healer / 3 DPS: a second tank-spec member there is
        // just a DPS, NOT a bodyguard — they keep tank_duty=0 and run
        // legacy behavior (the pull gates still keep them from starting
        // pulls, because they aren't the designated main tank).
        // Deliberately NOT keyed on advice.tank_swap_on_spells: the
        // MPlusAffix script merges Necrotic (209858) into EVERY dungeon's
        // advice unconditionally, so list-presence is "always" — and a
        // party tank-swap emergency is already handled by the legacy
        // taunt-swap rule without a standing off-tank designation.
        const bool wants_off_tank = raid_rules;
        // Main tank: the raid-frame Main Tank flag first, else the first live tank (humans first).
        uint64 main_tank = 0, off_tank = 0;
        for (auto const& e : mem)
            if (e.tank && e.alive && e.src->is_main_tank) { main_tank = e.guid_low; break; }
        // Without the flag the main tank stays whoever it was while that tank lives: a death and a rez no longer hand the role back to the
        // lowest guid (tank_duty and the heal quotas flipped twice per death).
        if (!main_tank)
            if (auto kept = last_main_tank_.find(bkey); kept != last_main_tank_.end())
                for (auto const& e : mem)
                    if (e.guid_low == kept->second && e.tank && e.alive) { main_tank = e.guid_low; break; }
        for (auto const& e : mem)
        {
            if (!e.tank || !e.alive) continue;
            if (!main_tank) { main_tank = e.guid_low; continue; }
            if (e.guid_low == main_tank) continue;
            if (!off_tank && wants_off_tank)
            { off_tank = e.guid_low; continue; }
        }
        if (main_tank)
            last_main_tank_[bkey] = main_tank;
        for (auto const& e : mem)
        {
            if (!e.tank) continue;
            if (PveOrder* o = bot_order(e.guid_low))
            {
                if (e.guid_low == main_tank)     o->tank_duty = 1;
                else if (e.guid_low == off_tank) o->tank_duty = 2;
                else if (wants_off_tank && e.alive) o->tank_duty = 3;
            }
        }

        // Healers. 5-man: the first healer is the tank healer (focus = main
        // tank); every further healer triages. Raid: every live tank gets
        // its own healers - main tank 3, off tank 2, each spare tank 1 -
        // while at least a third of the healers stay on raid triage, and
        // tanks are served in duty order. Humans occupy healer slots in
        // census order but are never ordered.
        if (!wants_off_tank)
        {
            last_heal_focus_.erase(bkey);
            last_heal_groups_.erase(bkey);
            bool tank_heal_taken = false;
            for (auto const& e : mem)
            {
                if (!e.healer || !e.alive) continue;
                const bool take_focus = !tank_heal_taken && main_tank != 0;
                tank_heal_taken = tank_heal_taken || take_focus;
                if (PveOrder* o = bot_order(e.guid_low))
                    if (take_focus)
                        o->heal_focus =
                            ObjectGuid::Create<HighGuid::Player>(main_tank);
            }
        }
        else
        {
            std::vector<std::pair<uint64, uint32>> tank_quota;   // (tank, healers wanted)
            if (main_tank) tank_quota.emplace_back(main_tank, 3u);
            if (off_tank)  tank_quota.emplace_back(off_tank, 2u);
            for (auto const& e : mem)
                if (e.tank && e.alive && e.guid_low != main_tank && e.guid_low != off_tank)
                    tank_quota.emplace_back(e.guid_low, 1u);
            uint32 n_healers = 0;
            for (auto const& e : mem)
                n_healers += e.healer && e.alive;
            uint32 budget = n_healers - n_healers / 3;   // the rest triage
            // Slot order for a short budget: the main tank (it holds the boss and takes most of the damage; MC Gehennas
            // 2026-10-08: with 4 tanks and 5 healers a plain round-robin left it ONE healer and it died in 12 s) gets its
            // first two healers, then the off tank and every spare tank get one, then the main tank's third and the off
            // tank's second.
            std::vector<uint64> spare_tanks;
            for (auto const& [tlow, want] : tank_quota)
                if (tlow != main_tank && tlow != off_tank)
                    spare_tanks.push_back(tlow);
            std::vector<uint64> const slots = Raid::HealSlotOrder(main_tank, off_tank, spare_tanks, budget);
            // Sticky: a healer keeps its tank while that tank still has a
            // slot for it, so one healer dying doesn't shuffle the rest
            // across the room. Free slots then go to unassigned healers
            // in census order, tanks in duty order.
            std::unordered_map<uint64, uint32> cap, filled;
            for (uint64 tlow : slots)
                ++cap[tlow];
            auto& prev = last_heal_focus_[bkey];
            std::unordered_map<uint64, uint64> kept;
            std::vector<Member const*> waiting;
            for (auto const& e : mem)
            {
                if (!e.healer || !e.alive) continue;
                auto it = prev.find(e.guid_low);
                if (it != prev.end() && filled[it->second] < cap[it->second])
                {
                    ++filled[it->second];
                    kept[e.guid_low] = it->second;
                }
                else
                    waiting.push_back(&e);
            }
            for (Member const* h : waiting)
                for (uint64 tlow : slots)
                    if (filled[tlow] < cap[tlow])
                    {
                        ++filled[tlow];
                        kept[h->guid_low] = tlow;
                        break;
                    }
            for (auto const& [hlow, tlow] : kept)
            {
                if (PveOrder* o = bot_order(hlow))
                    o->heal_focus = ObjectGuid::Create<HighGuid::Player>(tlow);
                if (auto pv = prev.find(hlow); pv == prev.end() || pv->second != tlow)
                    TC_LOG_INFO("playerbot.v2", "[raid_heal] assign: healer {} -> tank {} (group {}, {} tank slots, {} healers)", hlow, tlow,
                                bkey, slots.size(), n_healers);
            }
            prev = std::move(kept);

            // The healers that are not on a tank (the triage healers) take one raid subgroup each, heal it first, and cover for each
            // other when one dies or runs dry (RaidHealGroups.h). Humans are never ordered.
            {
                std::vector<Raid::GroupHealer> triage;
                std::vector<uint8> groups;
                for (auto const& e : mem)
                {
                    if (!e.alive || !e.src)
                        continue;
                    groups.push_back(e.src->subgroup);
                    if (e.healer && e.is_bot && !prev.count(e.guid_low))
                        triage.push_back({ e.guid_low, e.src->max_mana > 0 ? int(int64(e.src->mana) * 100 / e.src->max_mana) : 100 });
                }
                std::sort(groups.begin(), groups.end());
                groups.erase(std::unique(groups.begin(), groups.end()), groups.end());
                auto& prev_groups = last_heal_groups_[bkey];
                std::map<uint64, uint16_t> const assigned = Raid::AssignHealGroups(triage, groups, prev_groups);
                for (auto const& [hlow, mask] : assigned)
                {
                    if (PveOrder* o = bot_order(hlow))
                        o->heal_groups = mask;
                    auto const was = prev_groups.find(hlow);
                    if (was == prev_groups.end() || was->second != mask)
                        TC_LOG_INFO("playerbot.v2", "[raid_heal] assign: healer {} -> subgroups mask {:#04x} (group {}, {} triage healers, {} subgroups)",
                                    hlow, mask, bkey, triage.size(), groups.size());
                }
                prev_groups = assigned;
            }
        }

        // Interrupt rotation (global ranks + Classic per-mob assignment)
        // and the hunter tranq order. See PlanInterrupts / PlanTranq.
        // The Felhunter duty goes first: it is pet choice, not live state.
        PlanFelhunters(bkey, mem, snaps, plan);
        PlanInterrupts(bkey, mem, snaps, plan);
        PlanTranq(bkey, mem, snaps, plan);

        // Soak duty: assigned UNCONDITIONALLY every plan (the fields cost
        // nothing and the consumer only acts when the live advice lists
        // soak mechanics — assigning here regardless removes any plan-
        // time/consume-time advice disagreement window). Two designated
        // DPS soakers (more bodies add nothing and feed the AoE);
        // everyone else explicitly stays out — the legacy rule walked the
        // ENTIRE group into the circle. Tanks and healers never soak.
        // Living human DPS occupy soaker slots first (census order puts
        // them ahead): a human already standing in for the mechanic means
        // fewer bots need drafting.
        {
            int soakers = 0;
            for (auto const& e : mem)
            {
                const bool dps = !e.tank && !e.healer;
                if (!e.is_bot)
                {
                    if (dps && e.alive && soakers < 2) ++soakers;
                    continue;
                }
                PveOrder* o = bot_order(e.guid_low);
                if (!o) continue;
                if (dps && e.alive && soakers < 2) { o->soaker = 1; ++soakers; }
                else                               { o->soaker = 2; }
            }
        }

        // Spread slots: stable per-member bearing indices so spread
        // mechanics fan the group out instead of every bot stepping away
        // from its (mutually nearest) neighbour in lockstep.
        uint8 slot = 0;
        for (auto const& e : mem)
            if (PveOrder* o = bot_order(e.guid_low))
                o->spread_slot = slot < 0xFF ? slot++ : 0xFF;
            else
                ++slot;   // humans consume a bearing too

        // Synchronized kill target: ONE live priority add, every bot DPS
        // burns it together. Scan the script's priority list in order
        // (front = top priority) against the reference snapshot's live
        // enemies. Sticky: keep the previous focus while it is still a
        // live candidate so the focus doesn't flap between two adds of
        // the same entry.
        ObjectGuid kill_focus;
        if (!advice.high_priority_kill_entries.empty())
        {
            // Stickiness is PER-GROUP coordinator state, not a read-back
            // from a member's previous order (members leave groups; a
            // single cold-data tick would silently drop the focus and
            // re-roll it next plan).
            ObjectGuid const prev_focus = [&]() -> ObjectGuid
            {
                auto it = last_kill_focus_.find(bkey);
                return it != last_kill_focus_.end() ? it->second
                                                    : ObjectGuid::Empty;
            }();
            // Candidate guards (review): skip adds CC-locked by a group
            // member (burning them breaks the CC — same rule the per-bot
            // target pickers follow) and adds that are not engaged with
            // anyone (ordering DPS onto an unpulled pack is a coordinated
            // BAD pull; the tank rules own first contact).
            auto cc_by_group = [&](NearbyUnit const& u) -> bool
            {
                if (!u.is_cc_locked || u.cc_caster.IsEmpty()) return false;
                const uint64 caster_low = u.cc_caster.GetCounter();
                for (auto const& e : mem)
                    if (e.guid_low == caster_low) return true;
                return false;
            };
            // Every member's view, not only the freshest one: the reference snapshot changes from plan to plan, and a member that
            // does not see the current focus (out of its range, behind a wall) made the stickiness drop it (zf_antusul RC10: the
            // focus changed 11 times in 90 s between the Witch Doctors and Shadowcasters of one fight, 112 -> 108 among two
            // Shadowcasters both alive, while Shadowhunters hexed the tank and the casters were eaten). A mob any member sees dead
            // or held by the group's crowd control is not a candidate, whatever an older view says.
            std::vector<ObjectGuid> ruled_out;
            for (auto const& sp : snaps)
                for (auto const& u : sp->combat.nearby_enemies)
                    if (u.hp <= 0 || cc_by_group(u))
                        ruled_out.push_back(u.guid);
            auto out = [&](ObjectGuid g) { return std::find(ruled_out.begin(), ruled_out.end(), g) != ruled_out.end(); };
            // The freshest view first, so a new pick is the one it always was; the others only keep the old focus or fill a gap.
            std::vector<BotSnapshot const*> views{ ref.get() };
            for (auto const& sp : snaps)
                if (sp != ref)
                    views.push_back(sp.get());
            for (uint32 want : advice.high_priority_kill_entries)
            {
                bool kept = false;
                for (BotSnapshot const* sp : views)
                {
                    for (auto const& u : sp->combat.nearby_enemies)
                    {
                        if (u.hp <= 0 || u.entry != want || out(u.guid)) continue;
                        if (u.victim.IsEmpty()) continue;   // unpulled — leave it
                        // Not one that stands in an idle pack's reach: the damage dealers run to the focus, and that pulls the pack
                        // (eea74327b3 smlibrary_loksey36: the focus went to a Scarlet Beastmaster shooting from 18 y beside the next
                        // pack, the rogue ran to it, that pack and the one after it joined, and the warrior died).
                        if (States::PointInUnpulledPackReach(BotSnapshotView(*sp), u.x, u.y, u.z)) continue;
                        if (kill_focus.IsEmpty()) kill_focus = u.guid;
                        if (u.guid == prev_focus) { kill_focus = prev_focus; kept = true; break; }
                    }
                    if (kept)
                        break;
                }
                if (!kill_focus.IsEmpty())
                    break;   // highest-priority entry with a live mob wins
            }
        }
        // A raid encounter rule that names the target itself wins (a Core Hound pack must die together: the raid brain
        // names its healthiest hound, never the downed or trailing ones the priority scan or the generic pick would take).
        // The brain plans after this coordinator, so this is its previous plan (one raid cadence old).
        if (ObjectGuid const rule_focus = Raid::GetBrain().FocusFor(b.group_low, b.map_id); !rule_focus.IsEmpty())
            kill_focus = rule_focus;
        // The owner's skull, else cross, on a mob somebody fights is the kill target, above the script's priority list and the encounter
        // rule: "skull then cross" is the one order the raid always follows (MC Majordomo 2026-10-15: the owner skulled a Flamewaker Healer,
        // the priority list picked another one and the raid burned that, while the polymorphers sheeped the skull).
        if (!GroupSnapshotView(*gs).leader_is_bot() && b.bots.front().second)
            if (Group* kg = b.bots.front().second->GetGroup())
                for (uint8 icon : { uint8(7), uint8(6) })   // skull, cross
                {
                    ObjectGuid const mark = kg->GetTargetIcon(icon);
                    if (mark.IsEmpty())
                        continue;
                    bool fought = false;
                    for (auto const& u : ref->combat.nearby_enemies)
                        if (u.guid == mark && u.hp > 0 && !u.victim.IsEmpty())
                            fought = true;
                    if (fought)
                    {
                        kill_focus = mark;
                        break;
                    }
                }
        if (kill_focus.IsEmpty())
            last_kill_focus_.erase(bkey);
        else
        {
            last_kill_focus_[bkey] = kill_focus;
            for (auto const& e : mem)
            {
                if (e.tank || e.healer) continue;   // tanks hold aggro, healers heal
                if (PveOrder* o = bot_order(e.guid_low))
                    o->kill_focus = kill_focus;
            }
        }

        // -- Raid tank -> mob assignment + fight diagnostics ------------------
        PlanRaidTanks(bkey, b.group_low, b.map_id, wants_off_tank, now_ms,
                      main_tank, off_tank, kill_focus,
                      GroupSnapshotView(*gs).explicit_kill_mark(), !GroupSnapshotView(*gs).leader_is_bot(),
                      mem, snaps, plan);

        // -- The tank's pre-pull crowd-control request (dungeons and raids) --
        if (Services::Config().behavior_tank_markers())
            PlanPrePullCc(bkey, b.group_low, b.map_id, now_ms, mem, snaps, plan);

        // -- Raid symbols on bots: kill / tank / crowd-control by icon (after the generic kill focus, before the curses) --
        PlanMarkOrders(b.group_low, b.map_id, wants_off_tank, advice.bosses, mem, snaps, plan);

        // -- Classic raid prep: warlock curses, fear, resistance potions -----
        // After PlanRaidTanks: the curses fall back to its kill focus.
        PlanCurses(bkey, b.group_low, b.map_id, wants_off_tank, b.bots, mem, snaps, plan);
        PlanFearAndResist(bkey, b.group_low, b.map_id, wants_off_tank, main_tank, off_tank,
                          b.bots, mem, snaps, plan);
        // After the tank duties: a tanking druid is never asked to Innervate.
        PlanInnervates(bkey, b.group_low, b.map_id, mem, snaps, plan);
        // The line-of-sight pull: a dungeon plan only (a raid's trash and bosses are the raid layer's), after the tank duties.
        if (!raid_rules)
            PlanSightPull(bkey, b.group_low, b.map_id, now_ms, b.in_combat, main_tank, b.bots, b.ais, snaps, plan);
        else
            sight_pull_.erase(bkey);
        // The main tank's pull back, out at the opener or on the way back (not the hold at the return point): the others wait at the
        // return point and leave the pack alone (eea74327b3 dmnorth_moldar: during the pull of a Gordok Mage-Lord pack the group walked
        // on after the tank to 20 y of the pack, the mage, hunter and rogue opened on it and on a Brute at 48-50 y, two Brutes came
        // through to the priest and killed it at 317 s).
        if (!raid_rules)
            if (auto it = b.ais.find(main_tank); it != b.ais.end() && it->second && it->second->pull_back_active())
            {
                BotAI::PullBackPlan const tpb = it->second->pull_back_plan();
                if (tpb.active && tpb.map == b.map_id && tpb.phase != BotAI::PullBackPhase::Hold)
                    for (auto& [low, o] : plan)
                    {
                        o.tank_pulling_back = true;
                        o.tank_pull_x = tpb.rx; o.tank_pull_y = tpb.ry; o.tank_pull_z = tpb.rz;
                    }
            }

        // Publish the main tank's guid to every ordered bot — the
        // off-tank's between-pull follow consumer and future assist
        // logic key off it.
        if (main_tank != 0)
        {
            ObjectGuid const mt_guid =
                ObjectGuid::Create<HighGuid::Player>(main_tank);
            for (auto& [pg_low, po] : plan)
                po.main_tank = mt_guid;
        }

        // -- Raid school hints (raid layer: Raid::Brain::SchoolsFor) ---------
        // Per bot, slot order: its own victim, its kill focus, then the other hinted enemies by guid. Every plan pass rewrites
        // all four slots, so a dead enemy or one whose schools changed drops out by itself.
        if (std::vector<Raid::Brain::EnemySchools> const* hints = Raid::GetBrain().SchoolsFor(b.group_low, b.map_id))
        {
            for (auto const& e : mem)
            {
                PveOrder* o = bot_order(e.guid_low);
                if (!o) continue;
                o->school_hints = {};
                size_t n = 0;
                auto put = [&](ObjectGuid g)
                {
                    if (g.IsEmpty() || n >= o->school_hints.size() || o->school_hint_for(g)) return;
                    for (Raid::Brain::EnemySchools const& h : *hints)
                        if (h.target == g)
                        {
                            o->school_hints[n++] = { h.target, h.prefer_mask, h.avoid_mask, h.reflect_mask };
                            return;
                        }
                };
                put(e.src ? e.src->victim : ObjectGuid::Empty);
                put(o->kill_focus);
                for (Raid::Brain::EnemySchools const& h : *hints)
                    put(h.target);
            }
        }

        // -- Raid movement context (raid layer: Raid::Brain::MoveContextFor) -
        // Hazard circles (ground effects, bombs on a raid member, breath corridors, frontal cones) and the stay-near leash for
        // the generic approach / spacing / escape steps. The brain sorts the circles nearest edge first, so the ones that
        // matter for this bot survive the cap. Rewritten every pass: a bot the brain no longer plans for gets none.
        for (auto const& e : mem)
        {
            PveOrder* o = bot_order(e.guid_low);
            if (!o) continue;
            o->avoid_circle_count = 0;
            o->stay_near_active = false;
            Raid::Brain::MoveContext const* ctx = Raid::GetBrain().MoveContextFor(e.guid_low);
            if (!ctx) continue;
            for (Raid::Brain::MoveContext::Circle const& c : ctx->circles)
            {
                if (o->avoid_circle_count >= PveOrder::kMaxAvoidCircles) break;
                o->avoid_circles[o->avoid_circle_count++] = { c.x, c.y, c.r };
            }
            if (ctx->stay_near)
            {
                o->stay_near_active = true;
                o->stay_near_x = ctx->stay_x;
                o->stay_near_y = ctx->stay_y;
                o->stay_near_r = ctx->stay_radius;
            }
        }

        // -- Publish + diagnostics --------------------------------------------
        int n_bots = 0, n_soak = 0, n_kick = 0;
        // The kill focus the DPS were actually given: the raid-mark / tank-held pick (PlanRaidTanks, PlanMarkOrders) writes it
        // straight into the orders, so the local script/priority pick above reads 0 on a raid (MC Garr 2026-10-09: the log said
        // kill_focus=0 while [raid_threat] had the Firesworn).
        for (auto const& [pl, po] : plan)
            if (!po.kill_focus.IsEmpty()) { kill_focus = po.kill_focus; break; }
        uint64 sig = 1469598103934665603ull;
        auto mix = [&sig](uint64 v) { sig ^= v; sig *= 1099511628211ull; };
        for (auto& [bg_low, o] : plan)
        {
            next_orders_[bg_low] = o;
            ++n_bots;
            if (o.soaker == 1) ++n_soak;
            if (o.interrupt_rank != 0xFF) ++n_kick;
            mix(bg_low);
            mix(uint64(o.tank_duty) | (uint64(o.interrupt_rank) << 8) |
                (uint64(o.soaker) << 16));
            mix(o.heal_focus.GetCounter());
            mix(o.heal_groups);
            mix(o.kill_focus.GetCounter());
        }
        auto sig_it = plan_sig_.find(bkey);
        if (sig_it == plan_sig_.end() || sig_it->second != sig)
        {
            plan_sig_[bkey] = sig;
            TC_LOG_INFO("playerbot.v2",
                "[pvecoord] group={} map={} plan changed: members={} bots={} "
                "main_tank={} off_tank={} kickers={} soakers={} kill_focus={}",
                b.group_low, b.map_id, uint32(mem.size()), n_bots, main_tank,
                off_tank, n_kick, n_soak, kill_focus.GetCounter());
        }

        dump << "group=" << b.group_low << " map=" << b.map_id
             << " members=" << mem.size()
             << " bots=" << n_bots
             << " main_tank=" << main_tank << " off_tank=" << off_tank
             << " kickers=" << n_kick << " soakers=" << n_soak
             << " kill_focus=" << kill_focus.GetCounter() << "\n";
    }

    // Forget buckets that no longer exist (group left the instance).
    for (auto it = fight_log_.begin(); it != fight_log_.end();)
        it = buckets.count(it->first) ? std::next(it) : fight_log_.erase(it);
    for (auto it = last_tank_target_.begin(); it != last_tank_target_.end();)
        it = buckets.count(it->first) ? std::next(it) : last_tank_target_.erase(it);
    for (auto it = cc_held_.begin(); it != cc_held_.end();)
        it = buckets.count(it->first) ? std::next(it) : cc_held_.erase(it);
    for (auto it = mark_release_until_.begin(); it != mark_release_until_.end();)
        it = buckets.count(it->first) ? std::next(it) : mark_release_until_.erase(it);
    for (auto it = kick_plan_logged_.begin(); it != kick_plan_logged_.end();)
        it = buckets.count(it->first) ? std::next(it) : kick_plan_logged_.erase(it);
    for (auto it = fight_start_.begin(); it != fight_start_.end();)
        it = buckets.count(it->first) ? std::next(it) : fight_start_.erase(it);
    for (auto it = defend_since_.begin(); it != defend_since_.end();)
        it = buckets.count(it->first) ? std::next(it) : defend_since_.erase(it);
    for (auto it = raid_tank_roster_.begin(); it != raid_tank_roster_.end();)
        it = buckets.count(it->first) ? std::next(it) : raid_tank_roster_.erase(it);
    for (auto it = last_heal_focus_.begin(); it != last_heal_focus_.end();)
        it = buckets.count(it->first) ? std::next(it) : last_heal_focus_.erase(it);
    for (auto it = last_heal_groups_.begin(); it != last_heal_groups_.end();)
        it = buckets.count(it->first) ? std::next(it) : last_heal_groups_.erase(it);
    for (auto it = last_raid_focus_.begin(); it != last_raid_focus_.end();)
        it = buckets.count(it->first) ? std::next(it) : last_raid_focus_.erase(it);
    for (auto it = tranq_ranks_.begin(); it != tranq_ranks_.end();)
        it = buckets.count(it->first) ? std::next(it) : tranq_ranks_.erase(it);
    for (auto it = last_curse_.begin(); it != last_curse_.end();)
        it = buckets.count(it->first) ? std::next(it) : last_curse_.erase(it);
    for (auto it = last_fear_ward_.begin(); it != last_fear_ward_.end();)
        it = buckets.count(it->first) ? std::next(it) : last_fear_ward_.erase(it);
    for (auto it = last_fear_boss_.begin(); it != last_fear_boss_.end();)
        it = buckets.count(it->first) ? std::next(it) : last_fear_boss_.erase(it);
    for (auto it = last_resist_potion_.begin(); it != last_resist_potion_.end();)
        it = buckets.count(it->first) ? std::next(it) : last_resist_potion_.erase(it);
    for (auto it = last_innervate_.begin(); it != last_innervate_.end();)
        it = buckets.count(it->first) ? std::next(it) : last_innervate_.erase(it);
    for (auto it = felhunter_holders_.begin(); it != felhunter_holders_.end();)
        it = buckets.count(it->first) ? std::next(it) : felhunter_holders_.erase(it);
    for (auto it = sight_pull_.begin(); it != sight_pull_.end();)
        it = buckets.count(it->first) ? std::next(it) : sight_pull_.erase(it);

    orders_ = std::move(next_orders_);
    next_orders_.clear();
    for (auto it = plan_sig_.begin(); it != plan_sig_.end();)
        it = buckets.count(it->first) ? std::next(it) : plan_sig_.erase(it);
    for (auto it = last_kill_focus_.begin(); it != last_kill_focus_.end();)
        it = buckets.count(it->first) ? std::next(it)
                                      : last_kill_focus_.erase(it);
    last_dump_ = dump.str();
    if (last_dump_.empty())
        last_dump_ = "no coordinated dungeon/raid groups";
}

void PveGroupCoordinator::PlanInterrupts(uint64 bkey, std::vector<Member> const& mem,
                                         std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                                         std::unordered_map<uint64, PveOrder>& plan)
{
    namespace Kick = Combat::Interrupt;
    auto bot_order = [&](uint64 g) -> PveOrder*
    {
        auto it = plan.find(g);
        return it == plan.end() ? nullptr : &it->second;
    };

    // Retail: capable non-healer bots ranked 0..N-1 in census order (rank 0
    // kicks on sight, rank 1 backs up, 2+ hold for the NEXT cast -
    // preserving kicks instead of dumping every cooldown on one cast).
    // Human kickers can't be scheduled, so they are simply not part of the
    // rotation.
    if (!IsVanillaRealm())
    {
        uint8 irank = 0;
        for (auto const& e : mem)
        {
            if (!e.interrupter || !e.alive || !e.is_bot) continue;
            if (PveOrder* o = bot_order(e.guid_low))
                o->interrupt_rank = irank < 0xFF ? irank++ : 0xFF;
        }
        return;
    }

    // Classic: the census flag only says the class has a vanilla kick; the
    // ability is live state (Pummel needs Berserker Stance, Shield Bash a
    // shield, Spell Lock a Felhunter out, Bash bear form) - rank only bots
    // that can kick now, so rank 0 is never a bot that cannot.
    std::unordered_map<uint64, BotSnapshot const*> snap_of;
    for (auto const& sp : snaps)
        snap_of[sp->guid.GetCounter()] = sp.get();
    struct Kicker
    {
        uint64        low = 0;
        std::string   name;
        Kick::Ability a;
        bool          ready = false;
        bool          busy = false;   // mid own cast: cannot kick before it finishes (a pet's Spell Lock does not wait for its master)
        float         x = 0.f, y = 0.f, z = 0.f;
        ObjectGuid    victim;
        PveOrder*     o = nullptr;
    };
    std::vector<Kicker> kickers;
    uint8 irank = 0;
    for (auto const& e : mem)
    {
        if (!e.interrupter || !e.alive || !e.is_bot) continue;
        PveOrder* o = bot_order(e.guid_low);
        auto it = snap_of.find(e.guid_low);
        if (!o || it == snap_of.end()) continue;
        BotSnapshotView const v(*it->second);
        Kick::Ability const a = Kick::ClassicAbility(v);
        if (!a.spell) continue;
        o->interrupt_rank = irank < 0xFF ? irank++ : 0xFF;
        Kicker k;
        k.low = e.guid_low;
        k.name = e.src ? e.src->name : std::to_string(e.guid_low);
        k.a = a;
        k.ready = Kick::Ready(v, a);
        k.busy = !a.pet && it->second->cast.is_casting && it->second->cast.current_cast_remaining.count() > 0;
        k.x = it->second->position.x; k.y = it->second->position.y; k.z = it->second->position.z;
        k.victim = v.victim();
        k.o = o;
        kickers.push_back(k);
    }
    if (kickers.empty())
        return;

    // Per casting mob: the union of every bot's view of interruptible casts
    // on mobs engaged with the group, most urgent first (Raid::Kicks::CastBefore). Each gets the
    // nearest free interrupters with their kick ready and the mob in range:
    // rank 0 kicks on sight, rank 1 backs it up (BotSnapshotView::
    // interrupt_window_open). One mob per interrupter.
    struct CastingMob { ObjectGuid guid; float x, y, z; int64 rem; NearbyUnit const* u; size_t order; };
    std::vector<CastingMob> casts;
    for (auto const& sp : snaps)
        for (NearbyUnit const& u : sp->combat.nearby_enemies)
        {
            if (!Kick::Casting(u) || !Kick::EngagedWithGroup(u) || u.cast_remaining.count() < 350) continue;
            if (std::any_of(casts.begin(), casts.end(), [&](CastingMob const& c) { return c.guid == u.guid; }))
                continue;
            // A raid rule names casts that go first, in order (Dark Mending before Shadow Bolt; Healing Circle, Greater Polymorph).
            casts.push_back({ u.guid, u.x, u.y, u.z, int64(u.cast_remaining.count()), &u,
                Raid::InterruptOrder(sp->position.map_id, u.casting_spell_id) });
        }
    if (casts.empty())
        return;
    std::sort(casts.begin(), casts.end(), [](CastingMob const& a, CastingMob const& b)
    {
        return Raid::Kicks::CastBefore({ a.order, a.rem, a.guid.GetCounter() }, { b.order, b.rem, b.guid.GetCounter() });
    });
    auto& logged = kick_plan_logged_[bkey];
    std::array<ObjectGuid, PveOrder::kMaxInterruptClaims> claimed{};
    size_t n_claimed = 0;
    for (CastingMob const& c : casts)
    {
        if (n_claimed >= claimed.size()) break;
        std::vector<std::pair<Raid::Kicks::Kicker, Kicker*>> cand;
        for (Kicker& k : kickers)
        {
            if (!k.ready || !k.o->interrupt_target.IsEmpty()) continue;
            if (!Kick::Affects(k.a, *c.u)) continue;   // a bear's Bash on a stun-immune mob
            const float dx = c.x - k.x, dy = c.y - k.y, dz = c.z - k.z;
            const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
            const bool reach = (k.a.melee && k.victim == c.guid) || d <= k.a.range + (k.a.melee ? 2.5f : 0.f);
            if (reach) cand.emplace_back(Raid::Kicks::Kicker{ k.busy, d, k.low }, &k);
        }
        std::sort(cand.begin(), cand.end(), [](auto const& a, auto const& b) { return Raid::Kicks::KickerBefore(a.first, b.first); });
        // A listed cast (the Healers' Dark Mending) is logged once per cast with who takes it, or that nobody can: the kick
        // rotation is the Majordomo fight's key, and only a log line says whether it happened.
        if (c.order != Raid::kNotInterruptFirst)
        {
            const uint64 state = (uint64(c.u->casting_spell_id) << 32) | (cand.empty() ? 0xFFFFFFFFull : (cand[0].second->low & 0xFFFFFFFFull));
            if (uint64& was = logged[c.guid.GetCounter()]; was != state)
            {
                was = state;
                CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(c.u->entry);
                if (cand.empty())
                    TC_LOG_INFO("playerbot.v2", "[raid_interrupt] plan: {} ({}) casts {} ({}ms left), no kicker ready in reach ({} kickers)",
                                ct ? ct->Name : std::string("?"), c.guid.GetCounter(), c.u->casting_spell_id, c.rem, kickers.size());
                else
                    TC_LOG_INFO("playerbot.v2", "[raid_interrupt] plan: {} ({}) casts {} ({}ms left) -> {} kicks (spell {}, {:.0f}y{}){}",
                                ct ? ct->Name : std::string("?"), c.guid.GetCounter(), c.u->casting_spell_id, c.rem,
                                cand[0].second->name, cand[0].second->a.spell, cand[0].first.dist, cand[0].first.busy ? ", mid-cast" : "",
                                cand.size() > 1 ? ", " + cand[1].second->name + " backs up" : std::string(", no backup"));
            }
        }
        if (cand.empty()) continue;
        for (size_t r = 0; r < cand.size() && r < 2; ++r)
        {
            cand[r].second->o->interrupt_target = c.guid;
            cand[r].second->o->interrupt_target_rank = uint8(r);
        }
        claimed[n_claimed++] = c.guid;
    }
    if (n_claimed)
        for (Kicker& k : kickers)
            k.o->interrupt_claimed = claimed;
}

void PveGroupCoordinator::PlanFelhunters(uint64 bkey, std::vector<Member> const& mem,
                                         std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                                         std::unordered_map<uint64, PveOrder>& plan)
{
    namespace Kick = Combat::Interrupt;
    if (!IsVanillaRealm())
        return;
    std::unordered_map<uint64, BotSnapshot const*> snap_of;
    for (auto const& sp : snaps)
        snap_of[sp->guid.GetCounter()] = sp.get();

    // Capable bot warlocks, census order (bots by guid). A Felhunter seen out
    // without Spell Lock marks the warlock incapable until it levels - it
    // would otherwise read capable again the moment it swapped to the Imp
    // (shard in the bags) and take the duty straight back.
    struct Lock { uint64 low; uint8 subgroup; };
    std::vector<Lock> able;
    for (auto const& e : mem)
    {
        if (!e.is_bot || !e.alive || e.cls != CLASS_WARLOCK || !e.src) continue;
        auto it = snap_of.find(e.guid_low);
        if (it == snap_of.end()) continue;
        BotSnapshotView const v(*it->second);
        Kick::FelhunterCap cap = Kick::FelhunterCapability(v);
        const bool felhunter_out = v.has_pet() && v.raw().pet.pet_family == CREATURE_FAMILY_FELHUNTER;
        if (cap == Kick::FelhunterCap::NoSpellLock)
            felhunter_no_lock_[e.guid_low] = v.level();
        else if (auto nl = felhunter_no_lock_.find(e.guid_low); nl != felhunter_no_lock_.end())
        {
            if (nl->second != v.level() || felhunter_out)
                felhunter_no_lock_.erase(nl);           // levelled, or its Felhunter has Spell Lock now
            else
                cap = Kick::FelhunterCap::NoSpellLock;
        }
        if (cap == Kick::FelhunterCap::Able)
            able.push_back({ e.guid_low, uint8(e.src->subgroup & 7) });
    }

    // One per subgroup, at most kMaxRaidFelhunters: holders that are still
    // capable keep the duty, free slots go by guid.
    auto& holders = felhunter_holders_[bkey];
    std::vector<uint64> next;
    uint8 taken_subgroups = 0;
    auto take = [&](Lock const& l)
    {
        const uint8 bit = uint8(1u << l.subgroup);
        if (next.size() >= Kick::kMaxRaidFelhunters || (taken_subgroups & bit)) return;
        if (std::find(next.begin(), next.end(), l.low) != next.end()) return;
        taken_subgroups |= bit;
        next.push_back(l.low);
    };
    for (uint64 h : holders)
        for (Lock const& l : able)
            if (l.low == h) take(l);
    for (Lock const& l : able)
        take(l);
    for (uint64 low : next)
        if (auto it = plan.find(low); it != plan.end())
            it->second.felhunter_duty = true;
    if (next != holders)
    {
        std::ostringstream os;
        for (uint64 low : next)
            os << ' ' << low;
        TC_LOG_INFO("playerbot.v2", "[pvecoord] bucket {} Felhunter (Spell Lock) warlocks:{} ({} capable)",
                    bkey, next.empty() ? std::string(" none") : os.str(), uint32(able.size()));
        holders = std::move(next);
    }
}

void PveGroupCoordinator::PlanTranq(uint64 bkey, std::vector<Member> const& mem,
                                    std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                                    std::unordered_map<uint64, PveOrder>& plan)
{
    constexpr uint32 TRANQUILIZING_SHOT = 19801;
    // An elite / boss engaged with the group carrying an aura Tranquilizing
    // Shot removes (NearbyUnit::enrage_age_ms: Enrage / vanilla Frenzy).
    bool enraged = false;
    for (auto const& sp : snaps)
    {
        for (NearbyUnit const& u : sp->combat.nearby_enemies)
            if (u.hp > 0 && !u.is_player && u.enrage_age_ms >= 0 && (u.is_elite_or_rare || (u.is_dungeon_boss || u.is_boss_rank))
                && u.unit_in_combat && Combat::Interrupt::EngagedWithGroup(u))
            {
                enraged = true;
                break;
            }
        if (enraged) break;
    }
    auto& ranks = tranq_ranks_[bkey];
    if (!enraged)
    {
        ranks.clear();
        return;
    }
    // Fixed at the enrage's onset: bot hunters with the shot ready, by guid
    // (census order). Held while the enrage lasts; re-tried each plan while
    // nobody had it ready.
    if (ranks.empty())
    {
        std::unordered_map<uint64, BotSnapshot const*> snap_of;
        for (auto const& sp : snaps)
            snap_of[sp->guid.GetCounter()] = sp.get();
        uint8 r = 0;
        for (auto const& e : mem)
        {
            if (!e.is_bot || !e.alive || e.cls != CLASS_HUNTER) continue;
            auto it = snap_of.find(e.guid_low);
            if (it == snap_of.end()) continue;
            BotSnapshotView const v(*it->second);
            if (v.knows_spell(TRANQUILIZING_SHOT) && v.is_ready(TRANQUILIZING_SHOT) && r < 0xFF)
                ranks[e.guid_low] = r++;
        }
    }
    else
    {
        // A hunter that died or left the bucket since the onset drops out and
        // the ones behind it move up, so rank 0 is always a live hunter (a
        // dead rank 0 used to delay every shot by a second per dead rank).
        std::vector<std::pair<uint8, uint64>> live;
        for (auto const& [low, r] : ranks)
        {
            const uint64 l = low;
            auto m = std::find_if(mem.begin(), mem.end(), [l](Member const& e) { return e.guid_low == l; });
            if (m != mem.end() && m->alive)
                live.emplace_back(r, low);
        }
        if (live.size() != ranks.size())
        {
            std::sort(live.begin(), live.end());
            ranks.clear();
            uint8 r = 0;
            for (auto const& [old_r, low] : live)
                ranks[low] = r++;
        }
    }
    for (auto const& [low, r] : ranks)
        if (auto it = plan.find(low); it != plan.end())
            it->second.tranq_rank = r;
}

namespace {

int32 Pct(int64 cur, int64 max)
{
    return max > 0 ? int32(cur * 100 / max) : 0;
}

std::string MobName(uint32 entry)
{
    if (CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(entry))
        return ct->Name;
    return "entry " + std::to_string(entry);
}

// Raid fight summary cadence: often enough to see a pull fall apart,
// rare enough that a 40-man log stays readable.
constexpr uint32 kFightSummaryMs = 3000;
// A spare tank never runs further than this for a mob; beyond it the mob
// belongs to whoever is closer (or the loose-attacker pick-up).
constexpr float kTankAssignRange = 60.0f;
constexpr uint32 kCcHoldMs = 45000;   // how long a tank is remembered as the holder of a mob under a group member's crowd control
// An unpulled mob this close to an engaged one belongs to the pack (the link-pull reach).
constexpr float kPackReach = 25.0f;

} // namespace

void PveGroupCoordinator::ReassignTankTarget(uint64 group_low, uint32 map_id, uint64 tank_low, ObjectGuid mob)
{
    const uint64 bkey = (group_low << 16) ^ uint64(map_id);
    auto& sticky = last_tank_target_[bkey];
    for (auto it = sticky.begin(); it != sticky.end();)
    {
        if (it->second == mob && it->first != tank_low)
        {
            if (auto o = orders_.find(it->first); o != orders_.end() && o->second.tank_target == mob)
                o->second.tank_target = ObjectGuid::Empty;
            it = sticky.erase(it);
        }
        else
            ++it;
    }
    sticky[tank_low] = mob;
    if (auto o = orders_.find(tank_low); o != orders_.end())
        o->second.tank_target = mob;
}

// The CC switch of the human who runs the group (OwnerRegistry::CcEnabled, group order "crowdcontrol on|off"): the leader when it is a
// player, else the first player in the group. A group without a player has no switch and keeps crowd control on.
static bool GroupCcSwitchOn(Group const* grp)
{
    if (!grp)
        return true;
    Player const* human = ObjectAccessor::FindConnectedPlayer(grp->GetLeaderGUID());
    if (!human || !human->GetSession() || !Scenario::PlaysAsHuman(human))
    {
        human = nullptr;
        for (GroupReference const& r : grp->GetMembers())
            if (Player const* m = r.GetSource(); m && m->GetSession() && Scenario::PlaysAsHuman(m)) { human = m; break; }
    }
    return !human || !human->GetSession() || Services::Owners().CcEnabled(human->GetSession()->GetAccountId());
}

void PveGroupCoordinator::PlanMarkOrders(uint64 group_low, uint32 map_id, bool is_raid, std::vector<uint32_t> const& bosses,
                                          std::vector<Member> const& mem,
                                          std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                                          std::unordered_map<uint64, PveOrder>& plan)
{
    const bool tank_dps_on = Services::Config().behavior_tank_markers();
    bool cc_on = Services::Config().behavior_mark_cc();
    if (!tank_dps_on && !cc_on)
        return;
    Group* grp = nullptr;
    for (auto const& e : mem)
        if (Player* gp = ObjectAccessor::FindConnectedPlayer(e.src->guid))
            if ((grp = gp->GetGroup())) break;
    if (!grp)
        return;
    // The owner's CC switch off: no crowd-control marks or holds for this group (the kill-order marks go on).
    cc_on = cc_on && GroupCcSwitchOn(grp);

    std::unordered_map<uint64, Member const*> by_low;
    for (auto const& e : mem)
        by_low[e.guid_low] = &e;
    // The group's icons; held_by[m] = the member carrying icon m (an icon on a bot is its order), 0 = none / a mob.
    std::array<ObjectGuid, 8> icon{};
    std::array<uint64, 8> held_by{};
    bool any_held = false;
    for (uint8 m = 0; m < 8; ++m)
    {
        icon[m] = grp->GetTargetIcon(m);
        if (icon[m].IsEmpty() || !icon[m].IsPlayer()) continue;
        if (auto it = by_low.find(icon[m].GetCounter()); it != by_low.end())
        {
            held_by[m] = it->second->guid_low;
            any_held = true;
        }
    }
    auto held_symbol = [&](uint64 low) -> uint8
    {
        for (uint8 m = 0; m < 8; ++m)
            if (held_by[m] == low) return m;
        return 0xFF;
    };
    std::unordered_map<uint64, BotSnapshot const*> snap_of;
    for (auto const& sn : snaps)
        snap_of[sn->guid.GetCounter()] = sn.get();
    auto unit_any = [&](ObjectGuid g) -> NearbyUnit const*
    {
        if (g.IsEmpty() || g.IsPlayer()) return nullptr;
        // The entry that carries a creature type wins: a bot's view built without one (type 0) must not hide it
        // (Garr 2026-10-08: the Firesworn read type 0 and no warlock was assigned a Banish).
        NearbyUnit const* found = nullptr;
        for (auto const& sn : snaps)
        {
            if (sn->position.map_id != map_id) continue;
            for (auto const& u : sn->combat.nearby_enemies)
                if (u.guid == g && u.hp > 0 && !u.is_player && !u.untargetable)
                {
                    if (u.creature_type != 0) return &u;
                    if (!found) found = &u;
                }
        }
        return found;
    };
    auto mob_of = [&](uint8 sym) -> NearbyUnit const* { return (sym < 8 && !held_by[sym]) ? unit_any(icon[sym]) : nullptr; };

    // Healers keep healing while somebody is in trouble: no crowd control duty for them then.
    bool emergency = false;
    for (auto const& e : mem)
        if (e.alive && e.src->max_hp > 0 && Pct(e.src->hp, e.src->max_hp) < 40) { emergency = true; break; }

    // An enemy the raid's data never holds (Raid::NoControl): neither an icon nor the automatic pick sends a caster to it.
    auto never_controlled = [&](uint32 entry) -> bool
    {
        if (!is_raid) return false;
        Raid::RaidRules const* rr = Raid::RulesForMap(map_id);
        return rr && std::any_of(rr->noControl.begin(), rr->noControl.end(), [&](Raid::NoControl const& n) { return n.enemy == entry; });
    };
    // A boss is never held. The core flags few of them (Classic::IsBossUnit reads the classification, and most classic dungeon
    // bosses are plain elites), so the instance script's boss list counts too: in Shadowfang Keep the mage sheeped Rethilgore,
    // an elite on the core, nine times in 40 s while the tank fell to 9% and the priest ran out of mana (RC4 sfk_rethilgore).
    auto is_boss = [&](NearbyUnit const& u) -> bool
    {
        return Classic::IsBossUnit(u) || std::find(bosses.begin(), bosses.end(), u.entry) != bosses.end();
    };
    std::vector<ObjectGuid> cc_taken;
    bool moon_is_tank_mark = false;
    for (auto const& [tank_low, marker] : Services::Owners().TankMarks(group_low))
        if (marker == 4) moon_is_tank_mark = true;
    if (auto h = by_low.find(held_by[4]); held_by[4] && h != by_low.end() && h->second->tank)
        moon_is_tank_mark = true;   // the moon on a tank bot itself
    // Pass 1: crowd control. Bots in member order, one bot per mob, never the skull, never a boss.
    if (cc_on)
        for (auto const& e : mem)
        {
            if (!e.is_bot || !e.alive || e.tank || !MarkCc::IsMarkCcClass(e.cls)) continue;
            if (e.healer && emergency) continue;
            auto po = plan.find(e.guid_low);
            auto sn = snap_of.find(e.guid_low);
            if (po == plan.end() || sn == snap_of.end() || cc_taken.size() >= PveOrder::kMaxCcReserved) continue;
            std::vector<uint8> order;
            if (uint8 const h = held_symbol(e.guid_low); h != 0xFF && h != 7) order.push_back(h);
            for (uint8 c : Services::Owners().RtiOrderOf(e.guid_low).cc)
                if (c < 7 && std::find(order.begin(), order.end(), c) == order.end()) order.push_back(c);
            // No icon or rti cc of its own: the caster holds the owner's moon (ike3's default rti cc). Marked mobs left the auto crowd
            // control, so without this a moon nobody was assigned to stayed free.
            // Not when the owner gave the moon to a tank ("assign moon" on a tank bot): that tank holds the mob, and a sheep on
            // it breaks on the tank's first hit (n71 Majordomo: Deleria sheeped the Healer Yuna was tanking).
            if (order.empty() && !moon_is_tank_mark) order.push_back(4);
            BotSnapshotView const view(*sn->second);
            // Sticky: a bot keeps its mob while the icon stays on it and the mob lives; a view that loses the mob for a few
            // seconds (a banished mob drops out of a lock's list) is no release (Garr 2026-10-08: Gustinor was assigned and
            // released the same Firesworn three times, each time recasting Banish on a held mob).
            struct CcStick { ObjectGuid mob; uint8 sym = 0; uint32 seen_ms = 0; };
            static std::mutex cc_stick_mx;
            static std::unordered_map<uint64, CcStick> cc_stick;
            uint32 const cc_now = GameTime::GetGameTimeMS();
            // A tank (or the owner) hitting the mob is the call to kill it, the last of its pack: the hold ends, as the automatic
            // crowd control's does, instead of re-sheeping the tank's target at every break (wc_cobrahn 127).
            auto const tank_hits = [&](ObjectGuid mob)
            {
                for (auto const& m : mem)
                    if ((!m.is_bot || m.tank) && m.src && m.src->victim == mob) return true;
                return false;
            };
            bool kept = false;
            {
                std::lock_guard lk(cc_stick_mx);
                if (auto it = cc_stick.find(e.guid_low); it != cc_stick.end())
                {
                    CcStick& st = it->second;
                    bool const icon_on = st.sym < 8 && !held_by[st.sym] && icon[st.sym] == st.mob &&
                                         st.mob != icon[7] && st.mob != icon[6];   // a skull / cross mob is a kill order, never held
                    NearbyUnit const* su = icon_on ? unit_any(st.mob) : nullptr;
                    if (su)
                        st.seen_ms = cc_now;
                    if (icon_on && !tank_hits(st.mob) && (su || uint32(cc_now - st.seen_ms) <= 4000u) &&
                        std::find(cc_taken.begin(), cc_taken.end(), st.mob) == cc_taken.end() &&
                        !(su && su->max_hp > 0 && su->hp * 2 < su->max_hp) &&
                        (!su || MarkCc::PickOption(view, e.cls, MarkCc::TypeOf(*su), !e.src->in_combat && !su->unit_in_combat)))
                    {
                        po->second.cc_target = st.mob;
                        po->second.cc_symbol = st.sym;
                        cc_taken.push_back(st.mob);
                        kept = true;
                    }
                    else
                        cc_stick.erase(it);
                }
            }
            if (kept) continue;
            for (uint8 sym : order)
            {
                NearbyUnit const* u = mob_of(sym);
                if (!u || is_boss(*u) || never_controlled(u->entry)) continue;
                if (u->guid == icon[7] || u->guid == icon[6]) continue;   // marked skull / cross: killed, never crowd-controlled
                if (u->max_hp > 0 && u->hp * 2 < u->max_hp) continue;     // under half health: being killed, not held again
                if (tank_hits(u->guid)) continue;                          // the tank is on it: killed, not held again
                if (std::find(cc_taken.begin(), cc_taken.end(), u->guid) != cc_taken.end()) continue;
                const bool ooc = !e.src->in_combat && !u->unit_in_combat;
                if (!MarkCc::PickOption(view, e.cls, MarkCc::TypeOf(*u), ooc))
                {
                    MarkCc::LogRefusal(e.guid_low, GameTime::GetGameTimeMS(), e.src->name.c_str(),
                                       "not assigned: no spell of this class holds that creature type (or not known / no shard)",
                                       u->entry, MarkCc::TypeOf(*u), 0.f);
                    continue;
                }
                po->second.cc_target = u->guid;
                po->second.cc_symbol = sym;
                cc_taken.push_back(u->guid);
                {
                    std::lock_guard lk(cc_stick_mx);
                    cc_stick[e.guid_low] = CcStick{ u->guid, sym, cc_now };
                }
                break;
            }
            // No icon call for this caster: it picks a mob of its own to hold, so the owner never has to assign crowd control
            // (playtest 2026-10-15). In a raid, an entry the encounter rules already crowd-control (RaidBrain::ElectCrowdControl) is left to them.
            // Never the owner's skull / cross or the raid's kill focus, never a boss or an unpulled mob, never one already under half
            // health; at least three live mobs are on the group (with two, killing them is quicker than holding one).
            // A healer holds nothing of its own once it is in a fight: its mana is the group's healing (rfd_tutenkash 127: the
            // priest shackled a fresh ghoul every few seconds between Greater Heals, 94% to 15% mana in 27 s, out of mana three
            // times, the tank died). Before the pull it still shackles; an icon the owner gave it is still its call.
            // Not in a raid: ten or more players kill a pack quicker than two of the six damage dealers can hold one mob each, and
            // the hold broke and re-formed every few seconds as the kill focus moved (UBRS 20 Oct: mark_cc assign / release 66 times
            // in 5 minutes, two casters' GCDs gone). The raid's encounter rules and the owner's icons still hold mobs.
            if (!is_raid && po->second.cc_target.IsEmpty() && !(e.healer && e.src->in_combat))
            {
                static std::mutex auto_cc_mx;
                static std::unordered_map<uint64, ObjectGuid> auto_cc;   // caster -> the mob it holds
                auto const& kill_focus_of = [&](ObjectGuid g)
                {
                    for (auto const& [low, o] : plan)
                        if (o.kill_focus == g) return true;
                    return false;
                };
                std::vector<NearbyUnit const*> pack;
                {
                    std::unordered_set<uint64> seen;
                    for (auto const& sn : snaps)
                    {
                        if (sn->position.map_id != map_id) continue;
                        for (auto const& u : sn->combat.nearby_enemies)
                            if (u.hp > 0 && !u.is_player && !u.untargetable && u.unit_in_combat && !u.victim.IsEmpty() &&
                                !is_boss(u) && seen.insert(u.guid.GetCounter()).second)
                                if (NearbyUnit const* full = unit_any(u.guid))
                                    pack.push_back(full);
                    }
                }
                auto holdable = [&](NearbyUnit const& u) -> bool
                {
                    // A mob carrying any raid icon is the owner's call (or a tank's assignment): no automatic hold on it. Only an icon
                    // the owner put on a caster sends that caster to hold it (the loop above).
                    for (ObjectGuid const& marked : icon)
                        if (u.guid == marked) return false;
                    if (kill_focus_of(u.guid) || never_controlled(u.entry)) return false;
                    // Nor the mob the owner (any human) or a tank is hitting: a sheep on it breaks at once and is recast, over and over.
                    for (auto const& m : mem)
                        if ((!m.is_bot || m.tank) && m.src && m.src->victim == u.guid) return false;
                    if (is_raid)
                        if (Raid::RaidRules const* rr = Raid::RulesForMap(map_id))
                            for (Raid::CrowdControl const& rule : rr->crowdControl)
                                if (rule.enemy == u.entry) return false;
                    if (u.max_hp > 0 && u.hp * 2 < u.max_hp) return false;
                    if (std::find(cc_taken.begin(), cc_taken.end(), u.guid) != cc_taken.end()) return false;
                    MarkCc::Option const* opt = MarkCc::PickOption(view, e.cls, MarkCc::TypeOf(u), !e.src->in_combat && !u.unit_in_combat);
                    // A root is not a hold on a caster: a rooted Flamewaker Elite stands and fireballs the healers (n71 Majordomo: four
                    // raid members died to Fireball). Only the owner's own mark roots one.
                    return opt && !(opt->root && u.is_ranged_caster);
                };
                ObjectGuid pick;
                {
                    std::lock_guard lk(auto_cc_mx);
                    if (auto it = auto_cc.find(e.guid_low); it != auto_cc.end())
                    {
                        NearbyUnit const* kept_u = unit_any(it->second);
                        if (kept_u && holdable(*kept_u))
                            pick = it->second;
                        else
                            auto_cc.erase(it);
                    }
                }
                if (pick.IsEmpty() && pack.size() >= 3)
                {
                    // Mobs nobody tanks first (they are on a member), then the lowest guid so every plan chooses the same one.
                    NearbyUnit const* best = nullptr;
                    bool best_loose = false;
                    for (NearbyUnit const* u : pack)
                    {
                        if (!holdable(*u)) continue;
                        auto tv = by_low.find(u->victim.GetCounter());
                        bool const loose = tv == by_low.end() || !tv->second->tank;
                        if (!best || (loose && !best_loose) || (loose == best_loose && u->guid.GetCounter() < best->guid.GetCounter()))
                        {
                            best = u;
                            best_loose = loose;
                        }
                    }
                    if (best)
                    {
                        pick = best->guid;
                        std::lock_guard lk(auto_cc_mx);
                        auto_cc[e.guid_low] = pick;
                    }
                }
                if (!pick.IsEmpty())
                {
                    po->second.cc_target = pick;
                    po->second.cc_symbol = 0xFF;
                    cc_taken.push_back(pick);
                }
            }
        }

    // [mark_cc] assign / break / release, change-detected per bot (the plan runs every tick).
    if (cc_on)
    {
        struct CcLog { ObjectGuid mob; bool held = false; };
        static std::mutex cc_log_mx;
        static std::unordered_map<uint64, CcLog> cc_log;
        std::lock_guard lk(cc_log_mx);
        for (auto const& e : mem)
        {
            if (!e.is_bot) continue;
            auto po = plan.find(e.guid_low);
            ObjectGuid const now = po == plan.end() ? ObjectGuid::Empty : po->second.cc_target;
            auto it = cc_log.find(e.guid_low);
            if (it == cc_log.end())
            {
                if (now.IsEmpty()) continue;
                it = cc_log.emplace(e.guid_low, CcLog{}).first;
            }
            CcLog& st = it->second;
            NearbyUnit const* u = now.IsEmpty() ? nullptr : unit_any(now);
            if (st.mob != now)
            {
                if (!st.mob.IsEmpty())
                    TC_LOG_INFO("playerbot.v2", "[mark_cc] group={} map={} {} released {} ({})", group_low, map_id,
                        e.src->name, st.mob.GetEntry(), st.mob.GetCounter());
                if (!now.IsEmpty())
                    TC_LOG_INFO("playerbot.v2", "[mark_cc] group={} map={} {} assigned {} ({}) symbol={}", group_low,
                        map_id, e.src->name, now.GetEntry(), now.GetCounter(), po->second.cc_symbol);
                st.mob = now;
                st.held = false;
            }
            bool const held = u && u->is_cc_locked;
            if (!now.IsEmpty() && st.held && !held)
                TC_LOG_INFO("playerbot.v2", "[mark_cc] group={} map={} {} CC on {} ({}) broke", group_low, map_id,
                    e.src->name, now.GetEntry(), now.GetCounter());
            st.held = held;
            if (now.IsEmpty())
                cc_log.erase(it);
        }
    }

    // Pass 2: every bot's symbol order (tank: holds; DPS: kills first) and the CC reservation list.
    for (auto const& e : mem)
    {
        if (!e.is_bot) continue;
        auto po = plan.find(e.guid_low);
        if (po == plan.end()) continue;
        PveOrder& o = po->second;
        for (size_t i = 0; i < cc_taken.size(); ++i)
            o.cc_reserved[i] = cc_taken[i];
        // A kill focus the plan picked on a mob under CC duty is dropped (the symbol's mob is controlled, not burned).
        if (o.cc_reserved_mob(o.kill_focus))
            o.kill_focus = ObjectGuid::Empty;
        if (!tank_dps_on || !e.alive) continue;
        uint8 const held = held_symbol(e.guid_low);
        o.mark_symbol = held;
        if (held == 0xFF && !any_held && Services::Owners().RtiOrderOf(e.guid_low).kill.empty()) continue;
        std::vector<uint8> order;
        if (held != 0xFF) order.push_back(held);
        for (uint8 k : Services::Owners().RtiOrderOf(e.guid_low).kill)
            if (k < 8 && std::find(order.begin(), order.end(), k) == order.end()) order.push_back(k);
        if (e.tank)
        {
            for (uint8 sym : order)
                if (NearbyUnit const* u = mob_of(sym))
                {
                    o.mark_target = u->guid;
                    // A 5-man tank holds it once it is engaged (a raid tank: PlanRaidTanks); never a pull from here.
                    if (!is_raid && u->unit_in_combat && o.tank_target.IsEmpty()) o.tank_target = u->guid;
                    break;
                }
            continue;
        }
        if (e.healer || !o.cc_target.IsEmpty()) continue;
        for (uint8 sym : order)
        {
            NearbyUnit const* u = mob_of(sym);
            if (!u || o.cc_reserved_mob(u->guid)) continue;
            if (u->is_cc_locked && sym != 7) continue;   // a held mob is left alone unless it is the skull
            o.mark_target = u->guid;
            // Engaged mobs only: an unpulled mob stays the tank's call. A bot's own kill symbol never replaces the group's kill focus
            // (the owner's skull / cross); only an icon the owner put on that bot does.
            if (u->unit_in_combat && (o.kill_focus.IsEmpty() || held != 0xFF)) o.kill_focus = u->guid;
            break;
        }
    }
    (void)group_low;
}

// Answers the tank's pre-pull crowd-control request (PrePullCc.h): the moon on the mob, the mage released for it. Dungeons and raids
// alike: it used to sit inside PlanRaidTanks' raid-only branch, so a 5-man's "asks Scnmage to hold" was never answered (deadmines_full:
// no mark, no sheep, the tank pulled after 23.9 s).
void PveGroupCoordinator::PlanPrePullCc(uint64 bkey, uint64 group_low, uint32 map_id, uint32 now_ms,
                                        std::vector<Member> const& mem,
                                        std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                                        std::unordered_map<uint64, PveOrder>& plan)
{
    PrePullCc::Request pcc;
    bool const have_pcc = PrePullCc::Get(group_low, now_ms, pcc);
    auto moon_it = prepull_moon_.find(bkey);
    if (!have_pcc && moon_it == prepull_moon_.end())
        return;
    Group* mk_grp = nullptr;
    for (auto const& e : mem)
        if (Player* gp = ObjectAccessor::FindConnectedPlayer(e.src->guid))
            if ((mk_grp = gp->GetGroup())) break;
    if (!mk_grp)
        return;
    constexpr uint8 kMoon = 4;
    ObjectGuid const by = mk_grp->GetLeaderGUID();
    // The moon this code put on a mob comes off when the request is finished or gone (it expires after kLifeMs, also inside the
    // tank's own Get), unless somebody has moved it since. It used to be tracked in marks_set_, which a 5-man clears every pass, so
    // it was never taken off: hot_faldrim RC3, the mage never got in range to sheep the marked rat, the tank pulled on without it,
    // and the rat kept the moon. The tank's pull skips a mob with an icon and its pack stop still yielded to it, so the group stood
    // there for the rest of the run.
    auto clear_moon = [&]
    {
        if (moon_it == prepull_moon_.end())
            return;
        if (mk_grp->GetTargetIcon(kMoon) == moon_it->second)
            mk_grp->SetTargetIcon(kMoon, ObjectGuid::Empty, by);
        prepull_moon_.erase(moon_it);
        moon_it = prepull_moon_.end();
    };
    if (!have_pcc || (moon_it != prepull_moon_.end() && moon_it->second != pcc.mob))
        clear_moon();
    if (!have_pcc)
        return;
    auto unit_of = [&](ObjectGuid g) -> NearbyUnit const*
    {
        for (auto const& sn : snaps)
        {
            if (sn->position.map_id != map_id) continue;
            for (auto const& u : sn->combat.nearby_enemies)
                if (u.guid == g) return &u;
        }
        return nullptr;
    };
    Member const* caster = nullptr;
    for (auto const& e : mem)
        if (e.guid_low == pcc.caster_low) { caster = &e; break; }
    bool const caster_ok = caster && caster->alive;
    NearbyUnit const* cu = unit_of(pcc.mob);
    bool const mob_ok = cu && cu->hp > 0;
    bool const finished = pcc.done_ms && uint32(now_ms - pcc.done_ms) > PrePullCc::kAfterDoneMs;
    if (!caster_ok || !mob_ok || finished)
    {
        clear_moon();
        PrePullCc::Erase(group_low);
        return;
    }
    ObjectGuid const cur = mk_grp->GetTargetIcon(kMoon);
    if (cur != pcc.mob)
    {
        bool other_icon = false;
        for (uint8 m2 = 0; m2 < 8 && !other_icon; ++m2)
            other_icon = m2 != kMoon && mk_grp->GetTargetIcon(m2) == pcc.mob;
        bool const moon_taken = !cur.IsEmpty() && cur != marks_set_[bkey][kMoon] && unit_of(cur) && unit_of(cur)->hp > 0;
        if (other_icon || moon_taken)
        {
            if (pcc.state != PrePullCc::State::Refused)
            {
                PrePullCc::SetState(group_low, PrePullCc::State::Refused);
                TC_LOG_INFO("playerbot.v2", "[raid_pull] group={} map={} pre-pull CC on {} ({}) refused: {}",
                    group_low, map_id, cu->entry, pcc.mob.GetCounter(),
                    other_icon ? "the mob already carries another icon" : "the moon is somebody else's call");
            }
        }
        else
        {
            mk_grp->SetTargetIcon(kMoon, pcc.mob, by);
            prepull_moon_[bkey] = pcc.mob;
            moon_it = prepull_moon_.find(bkey);
            PrePullCc::SetState(group_low, PrePullCc::State::Marked);
            TC_LOG_INFO("playerbot.v2", "[raid_pull] group={} map={} marks {} ({}) with the moon for {}'s crowd control",
                group_low, map_id, cu->entry, pcc.mob.GetCounter(), caster->src->name);
        }
    }
    // The mage may crowd-control the idle marked mob: the squad's tank asked for it (MarksNeedOrder).
    if (pcc.state != PrePullCc::State::Refused)
        if (auto pit = plan.find(pcc.caster_low); pit != plan.end())
            pit->second.marks_released = true;
}

void PveGroupCoordinator::PlanRaidTanks(uint64 bkey, uint64 group_low, uint32 map_id, bool is_raid,
                                        uint32 now_ms, uint64 main_tank, uint64 off_tank,
                                        ObjectGuid script_focus, ObjectGuid kill_mark, bool idle_mark_is_call,
                                        std::vector<Member> const& mem,
                                        std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                                        std::unordered_map<uint64, PveOrder>& plan)
{
    std::unordered_map<uint64, Member const*> by_low;
    for (auto const& e : mem)
        by_low[e.guid_low] = &e;
    auto member_of = [&](ObjectGuid g) -> Member const*
    {
        if (g.IsEmpty() || !g.IsPlayer()) return nullptr;
        auto it = by_low.find(g.GetCounter());
        return it == by_low.end() ? nullptr : it->second;
    };

    // -- Engaged mobs: the union of every bot's view, one entry per guid.
    // Engaged = in combat and hitting a group member or a member's pet
    // (the observing bot's victim_kind). CC'd-by-group mobs are left out:
    // a tank walking onto a sheep breaks it.
    struct Mob
    {
        NearbyUnit const* u;
        bool loose;          // hitting a non-tank member
    };
    std::vector<Mob> mobs;
    auto find_mob = [&](ObjectGuid g) -> Mob const*
    {
        for (auto const& m : mobs)
            if (m.u->guid == g) return &m;
        return nullptr;
    };
    for (auto const& sn : snaps)
    {
        // A snapshot from before the bot zoned lists another map's mobs.
        if (sn->position.map_id != map_id) continue;
        for (auto const& u : sn->combat.nearby_enemies)
        {
            if (u.hp <= 0 || u.is_player || u.untargetable || u.is_pacified) continue;
            if (!u.unit_in_combat) continue;
            ThreatVictim const k = ThreatVictim(u.victim_kind);
            if (k != ThreatVictim::Me && k != ThreatVictim::Tank &&
                k != ThreatVictim::Member && k != ThreatVictim::MemberPet)
                continue;
            if (u.is_cc_locked && member_of(u.cc_caster)) continue;
            // A mob immune to every school (Lord Victor Nefarius behind Nefarius Barrier 22663, a banished mob) is nothing to hold: nothing
            // the tank does reaches it. bwl_nefarian 2026-11-07: the main tank was assigned to the rooted, immune Victor for the whole
            // drakonid phase, the "boss is never left without a tank" pass gave him back every time, and the raid fell to the waves.
            if (Raid::GetBrain().AllSchoolsAvoided(group_low, map_id, u.guid)) continue;
            if (find_mob(u.guid)) continue;
            Member const* v = member_of(u.victim);
            mobs.push_back({ &u, v && !v->tank });
        }
    }

    // -- Tank -> mob assignment (raids only) -------------------------------
    // Every live tank owns one engaged mob: main tank first, then the off
    // tank, then the spare tanks in census order. A tank keeps its mob
    // while it lives; a new owner takes the mob it is already hitting, else
    // the nearest free one (mobs on non-tanks first). Mobs beyond the tank
    // count fall to the per-tank loose-attacker pick-up.
    if (is_raid)
    {
        std::vector<Member const*> tanks;
        auto add_tank = [&](uint64 low)
        {
            auto it = by_low.find(low);
            if (it != by_low.end() && it->second->tank && it->second->alive &&
                std::find(tanks.begin(), tanks.end(), it->second) == tanks.end())
                tanks.push_back(it->second);
        };
        if (main_tank) add_tank(main_tank);
        if (off_tank)  add_tank(off_tank);
        for (auto const& e : mem)
            add_tank(e.guid_low);

        auto& sticky = last_tank_target_[bkey];
        auto unit_any_early = [&](ObjectGuid g) -> NearbyUnit const*
        {
            for (auto const& sn : snaps)
            {
                if (sn->position.map_id != map_id) continue;
                for (auto const& u : sn->combat.nearby_enemies)
                    if (u.guid == g) return &u;
            }
            return nullptr;
        };
        // A mob the raid brain's order forbids this tank (a fixed tank row's tank waits at its slot for an add still far out, RaidBrain
        // TankSpotFor) is not this tank's to hold now: the brain reads the room's shape, this pass only distances.
        auto raid_forbids = [&](uint64 tank_low, ObjectGuid mob) -> bool
        {
            RaidOrder const* ro = Raid::GetBrain().OrderFor(tank_low);
            return ro && ro->active && std::find(ro->forbidden_targets.begin(), ro->forbidden_targets.end(), mob) != ro->forbidden_targets.end();
        };
        for (auto it = sticky.begin(); it != sticky.end();)
        {
            auto t = by_low.find(it->first);
            bool keep = t != by_low.end() && t->second->tank && t->second->alive &&
                        find_mob(it->second) != nullptr && !raid_forbids(it->first, it->second);
            if (!keep && t != by_low.end() && t->second->tank && t->second->alive)
            {
                // [raid_tank] why a tank lost its mob (Garr 2026-10-08: one tank hopped between Firesworn ~15 times).
                NearbyUnit const* gone = unit_any_early(it->second);
                if (gone && gone->is_cc_locked && member_of(gone->cc_caster))
                    cc_held_[bkey][it->second.GetCounter()] = { t->first, now_ms + kCcHoldMs };
                TC_LOG_INFO("playerbot.v2", "[raid_tank] group={} map={} {} drops {} ({}): {}", group_low, map_id, t->second->src->name,
                            it->second.GetEntry(), it->second.GetCounter(),
                            !gone ? "out of every view or dead" : raid_forbids(it->first, it->second) ? "the raid order forbids it" : (gone->is_cc_locked ? "crowd-controlled" : (!gone->unit_in_combat ? "out of combat" : "no longer hitting the group")));
            }
            it = keep ? std::next(it) : sticky.erase(it);
        }

        // -- Marker assignments (Playerbot.Behavior.TankMarkers) --------------
        // A tank with a raid marker (the automatic split below, or the owner's "tank <name> <marker>" order, which
        // wins) holds whatever engaged mob carries that marker, whichever mob it was on: this overrides the
        // per-tank split that follows. A tank that is dead or not a duty tank is skipped (the split covers its mob).
        const bool mk_on = Services::Config().behavior_tank_markers();
        Group* mk_grp = nullptr;
        if (mk_on)
            for (auto const& e : mem)
                if (Player* gp = ObjectAccessor::FindConnectedPlayer(e.src->guid))
                    if ((mk_grp = gp->GetGroup())) break;
        std::unordered_map<uint64, uint8> mk_eff;   // tank guid-low -> marker, automatic overlaid by manual
        // The owner's icons on bots: held_by[m] = the member (guid-low) carrying icon m, 0 = none. A duty tank bot with an
        // icon (or one named in its `rti` order) holds the mob carrying the same icon: user_sym tank -> icon, which beats
        // every other marker rule. user_reserved: such a tank's mob that is not engaged yet; it stays out of the other
        // tanks' reach (reserved below) and is never pulled by the tank_target path. Icons on bots are never auto-marked.
        std::array<uint64, 8> held_by{};
        std::unordered_map<uint64, uint8> user_sym;
        std::unordered_map<uint64, ObjectGuid> reserved;   // tank guid-low -> the unpulled mob reserved for it
        auto unit_any = [&](ObjectGuid g) -> NearbyUnit const*
        {
            if (g.IsEmpty()) return nullptr;
            for (auto const& sn : snaps)
            {
                if (sn->position.map_id != map_id) continue;
                for (auto const& u : sn->combat.nearby_enemies)
                    if (u.guid == g) return &u;
            }
            return nullptr;
        };
        if (mk_grp)
        {
            for (uint8 m = 0; m < 8; ++m)
                if (Member const* hm = member_of(mk_grp->GetTargetIcon(m)))
                    held_by[m] = hm->guid_low;
            // An automatic marker whose icon an icon holder (a bot with that symbol) or someone else's call on a mob now
            // carries is dropped: the tank re-picks a free one below.
            {
                std::array<ObjectGuid, 8> const& ours0 = marks_set_[bkey];
                auto& auto0 = tank_mark_auto_[bkey];
                for (auto it = auto0.begin(); it != auto0.end();)
                {
                    ObjectGuid const ic = mk_grp->GetTargetIcon(it->second);
                    it = (held_by[it->second] != 0 || (!ic.IsEmpty() && ic != ours0[it->second])) ? auto0.erase(it)
                                                                                                    : std::next(it);
                }
            }
            if (auto ai = tank_mark_auto_.find(bkey); ai != tank_mark_auto_.end())
                mk_eff = ai->second;
            for (auto const& [tl, mkr] : Services::Owners().TankMarks(group_low))
            {
                if (mkr >= 6) continue;   // a stored skull / cross tank marker (older order): kill orders are never a tank's mark
                for (auto it = mk_eff.begin(); it != mk_eff.end();)
                    it = (it->first == tl || it->second == mkr) ? mk_eff.erase(it) : std::next(it);
                mk_eff[tl] = mkr;
            }
            for (Member const* t : tanks)
            {
                if (!t->is_bot) continue;
                uint8 sym = 0xFF;
                for (uint8 m = 0; m < 8; ++m)
                    if (held_by[m] == t->guid_low) { sym = m; break; }
                if (sym == 0xFF)
                    for (uint8 m : Services::Owners().RtiOrderOf(t->guid_low).kill)
                        if (m < 8 && !mk_grp->GetTargetIcon(m).IsEmpty() && !held_by[m]) { sym = m; break; }
                if (sym == 0xFF) continue;
                for (auto it = mk_eff.begin(); it != mk_eff.end();)
                    it = (it->first == t->guid_low || it->second == sym) ? mk_eff.erase(it) : std::next(it);
                mk_eff[t->guid_low] = sym;
                user_sym[t->guid_low] = sym;
            }
            for (auto const& [tl, mkr] : mk_eff)
            {
                Member const* t = nullptr;
                for (Member const* c : tanks) if (c->guid_low == tl) { t = c; break; }
                if (!t || !t->is_bot) continue;
                Mob const* mm = find_mob(mk_grp->GetTargetIcon(mkr));
                if (!mm) continue;
                if (Raid::OffTankMob(map_id, mm->u->entry)) continue;   // the second tank's, whatever tank the icon was given to
                // The raid brain's forbid wins over the icon: the sticky check above drops it again on the next pass, and the tank took
                // and dropped the marked add every pass (bwl_nefarian 1109a run 1: Mctankwa "takes … why=marker 0" / "drops" in a loop).
                if (raid_forbids(tl, mm->u->guid)) continue;
                if (auto sit = sticky.find(tl); sit != sticky.end() && sit->second == mm->u->guid) continue;
                for (auto it = sticky.begin(); it != sticky.end();)
                    it = it->second == mm->u->guid ? sticky.erase(it) : std::next(it);
                sticky[tl] = mm->u->guid;
                TC_LOG_INFO("playerbot.v2", "[raid_tank] group={} map={} {} takes {} ({}) {}% why=marker {}",
                    group_low, map_id, t->src->name, MobName(mm->u->entry), mm->u->guid.GetCounter(),
                    Pct(mm->u->hp, mm->u->max_hp), mkr);
            }
        }

        // A symbol tank whose mob is not engaged yet: reserve it (the pull is the main tank's call, a spare tank holds).
        if (mk_grp)
            for (auto const& [tl, sym] : user_sym)
            {
                ObjectGuid const g = mk_grp->GetTargetIcon(sym);
                if (g.IsEmpty() || find_mob(g)) continue;
                NearbyUnit const* u = unit_any(g);
                if (!u || u->hp <= 0 || u->is_player || u->untargetable) continue;
                reserved[tl] = g;
            }

        // Human tanks are never ordered, but the mob they are hitting is
        // theirs: a bot tank holding it yields (it would rip threat off
        // the player otherwise).
        std::vector<ObjectGuid> taken;
        auto is_taken = [&](ObjectGuid g)
        { return std::find(taken.begin(), taken.end(), g) != taken.end(); };
        for (Member const* t : tanks)
        {
            if (t->is_bot || !find_mob(t->src->victim) || is_taken(t->src->victim)) continue;
            taken.push_back(t->src->victim);
            for (auto it = sticky.begin(); it != sticky.end();)
                it = it->second == t->src->victim ? sticky.erase(it) : std::next(it);
        }
        for (auto const& [tlow, mob] : sticky)
            taken.push_back(mob);
        for (auto const& [tl, g] : reserved)
            taken.push_back(g);

        auto assign = [&](Member const* t, Mob const& m, char const* why)
        {
            sticky[t->guid_low] = m.u->guid;
            cc_held_[bkey].erase(m.u->guid.GetCounter());
            taken.push_back(m.u->guid);
            const float dx = m.u->x - t->src->x, dy = m.u->y - t->src->y;
            Member const* v = member_of(m.u->victim);
            TC_LOG_INFO("playerbot.v2",
                "[raid_tank] group={} map={} {} takes {} ({}) {}% on {}{} dist={:.0f}y why={}",
                group_low, map_id, t->src->name, MobName(m.u->entry), m.u->guid.GetCounter(),
                Pct(m.u->hp, m.u->max_hp), v ? v->src->name : std::string("-"),
                m.loose ? " (loose)" : "", std::sqrt(dx * dx + dy * dy), why);
        };
        // The nearest free engaged mob in reach: a summoned add the rules leave to the DPS last, mobs on non-tanks first,
        // then mobs on a tank that holds a boss (the boss's tank must not keep the adds too: Garr 2026-10-08, the main tank
        // held Garr and every Firesworn), distance within a class.
        auto on_boss_tank = [&](Mob const& m) -> bool
        {
            Member const* v = member_of(m.u->victim);
            if (!v || !v->tank) return false;
            auto sit = sticky.find(v->guid_low);
            if (sit == sticky.end()) return false;
            Mob const* held = find_mob(sit->second);
            return held && held->u->guid != m.u->guid && (held->u->is_dungeon_boss || held->u->is_boss_rank);
        };
        // Holds of this bucket still in force: one that ran out is dropped (a CC'd mob is not in `mobs`, so only time ends it).
        auto& cc_held = cc_held_[bkey];
        for (auto it = cc_held.begin(); it != cc_held.end();)
            it = now_ms >= it->second.until_ms ? cc_held.erase(it) : std::next(it);
        // A mob carrying an icon the owner gave a crowd-control caster is that caster's (Majordomo n78: the circle on an Elite was
        // Scott's, and Hannorto took the same Elite as "nearest", so his hits broke the sheep over and over). Tanks leave it alone
        // until it is loose on a member.
        auto cc_owned = [&](Mob const& m) -> bool
        {
            if (!mk_grp || m.loose) return false;
            for (uint8 sym = 0; sym < 7; ++sym)
            {
                if (mk_grp->GetTargetIcon(sym) != m.u->guid) continue;
                for (Member const& e : mem)
                {
                    if (!e.is_bot || e.tank || !MarkCc::IsMarkCcClass(e.cls)) continue;
                    if (held_by[sym] == e.guid_low) return true;
                    for (uint8 c : Services::Owners().RtiOrderOf(e.guid_low).cc)
                        if (c == sym) return true;
                }
            }
            return false;
        };
        auto nearest_free = [&](Member const* t) -> Mob const*
        {
            auto rank = [&](Mob const& m) -> int
            {
                if (Raid::NoTank(map_id, m.u->entry)) return 3;
                if (Raid::TankLast(map_id, m.u->entry)) return 4;   // Majordomo: the tank left over takes him
                if (auto const hold = cc_held.find(m.u->guid.GetCounter()); hold != cc_held.end() && hold->second.tank_low == t->guid_low)
                    return -1;   // the tank that held it before the crowd control ended takes it back first
                if (m.loose) return 0;
                return on_boss_tank(m) ? 1 : 2;
            };
            Mob const* best = nullptr;
            int best_rank = 99;
            uint32 best_order = UINT32_MAX;
            float best_d2 = kTankAssignRange * kTankAssignRange;
            for (auto const& m : mobs)
            {
                if (is_taken(m.u->guid) || cc_owned(m) || raid_forbids(t->guid_low, m.u->guid)) continue;
                const float dx = m.u->x - t->src->x, dy = m.u->y - t->src->y;
                const float d2 = dx * dx + dy * dy;
                if (d2 > kTankAssignRange * kTankAssignRange) continue;
                const int r = rank(m);
                // Within a class the encounter's tank order comes before distance (Majordomo: Healers, then Elites).
                const uint32 order = Raid::TankOrder(map_id, m.u->entry);
                if (best && (r > best_rank || (r == best_rank && (order > best_order || (order == best_order && d2 >= best_d2))))) continue;
                best = &m;
                best_rank = r;
                best_order = order;
                best_d2 = d2;
            }
            return best;
        };
        // A free engaged mob other than <except> that is neither a summoned add nor one the rules leave to the tank left over: while one
        // exists, a tankLast mob (Majordomo) is nobody's to hold.
        auto other_free_mob = [&](ObjectGuid except) -> bool
        {
            for (auto const& m : mobs)
                if (m.u->guid != except && !is_taken(m.u->guid) && !Raid::NoTank(map_id, m.u->entry) && !Raid::TankLast(map_id, m.u->entry))
                    return true;
            return false;
        };
        // A boss the rules give the second tank (Majordomo): the off tank holds him whatever it held, and whoever held him takes a
        // free mob below. The owner's plan: off tank on him, the other tanks one Elite each, the main tank the skull.
        if (off_tank)
            if (auto ot = by_low.find(off_tank); ot != by_low.end() && ot->second->is_bot && ot->second->alive && !user_sym.count(off_tank))
                for (auto const& m : mobs)
                {
                    if (!Raid::OffTankMob(map_id, m.u->entry)) continue;
                    if (auto sit = sticky.find(off_tank); sit != sticky.end() && sit->second == m.u->guid) break;
                    if (auto sit = sticky.find(off_tank); sit != sticky.end())
                    {
                        taken.erase(std::remove(taken.begin(), taken.end(), sit->second), taken.end());
                        sticky.erase(sit);
                    }
                    for (auto it = sticky.begin(); it != sticky.end();)
                    {
                        if (it->second == m.u->guid)
                        {
                            taken.erase(std::remove(taken.begin(), taken.end(), it->second), taken.end());
                            it = sticky.erase(it);
                        }
                        else
                            ++it;
                    }
                    assign(ot->second, m, "off_tank_mob");
                    break;
                }
        // The main tank chooses first: the mob it already hits (its pull), else the mob engaged with it, else the
        // nearest. Without this a spare tank that ran at the pull target before the main tank had a victim of its own
        // would be handed the main tank's mob, and the main tank the next one.
        if (main_tank)
            if (auto mt = by_low.find(main_tank); mt != by_low.end() && mt->second->is_bot && mt->second->tank &&
                mt->second->alive && !sticky.count(main_tank))
            {
                Member const* t = mt->second;
                Mob const* pick = find_mob(t->src->victim);
                char const* why = "already_on_it";
                if (pick && Raid::NoTank(map_id, pick->u->entry)) pick = nullptr;   // a Spawn is no mob to hold
                if (pick && raid_forbids(t->guid_low, pick->u->guid)) pick = nullptr;
                if (pick && Raid::TankLast(map_id, pick->u->entry) && other_free_mob(pick->u->guid)) pick = nullptr;   // the adds first
                if (!pick || is_taken(pick->u->guid))
                {
                    pick = nullptr;
                    for (auto const& m : mobs)
                        if (m.u->victim == t->src->guid && !is_taken(m.u->guid) &&
                            !(Raid::TankLast(map_id, m.u->entry) && other_free_mob(m.u->guid))) { pick = &m; why = "on_main_tank"; break; }
                }
                if (!pick) { pick = nearest_free(t); why = "nearest"; }
                if (pick) assign(t, *pick, why);
            }
        // A mob that has just come off a group member's crowd control goes back to the tank that held it, before any other tank
        // is offered it as a loose mob.
        for (Member const* t : tanks)
        {
            if (!t->is_bot || sticky.count(t->guid_low)) continue;
            for (auto const& m : mobs)
                if (auto const hold = cc_held.find(m.u->guid.GetCounter());
                    hold != cc_held.end() && hold->second.tank_low == t->guid_low && !is_taken(m.u->guid) && !Raid::NoTank(map_id, m.u->entry) &&
                    !cc_owned(m))   // a sheep the owner gave a mage broke: the mage recasts it, the tank does not hit it
                {
                    assign(t, m, "cc_released");
                    break;
                }
        }
        for (Member const* t : tanks)
        {
            if (!t->is_bot || sticky.count(t->guid_low)) continue;
            if (Mob const* cur = find_mob(t->src->victim); cur && !is_taken(cur->u->guid) && !Raid::NoTank(map_id, cur->u->entry) && !cc_owned(*cur) &&
                !raid_forbids(t->guid_low, cur->u->guid) &&
                !(Raid::TankLast(map_id, cur->u->entry) && other_free_mob(cur->u->guid)))
                assign(t, *cur, "already_on_it");
        }
        for (Member const* t : tanks)
        {
            if (!t->is_bot || sticky.count(t->guid_low)) continue;
            if (Mob const* best = nearest_free(t))
            {
                // A tank reserved for its symbol's mob takes only a loose mob meanwhile (nearest_free lists those first).
                if (reserved.count(t->guid_low) && !best->loose) continue;
                assign(t, *best, best->loose ? "loose_nearest" : "nearest");
            }
        }
        // -- The owner's skull and cross are held by a tank -----------------------
        // They are the owner's kill calls, and a mob nobody tanks is on a healer or a mage while the raid burns it (n78 Majordomo: the
        // skull moved onto a sheeped Healer, the sheep broke, and no tank took it; then the cross Healer ran loose for 20 s and killed
        // the owner, while every tank sat on its own Elite or dropped target). A tank with no symbol of the owner's drops its mob for
        // it: the main tank first, else the one with the fewest mobs on it. Skull first, then cross, each on a different tank.
        if (mk_grp)
            for (uint8 const kill_icon : { uint8(7), uint8(6) })
            {
                ObjectGuid const sk = mk_grp->GetTargetIcon(kill_icon);
                if (sk.IsEmpty()) continue;
                Mob const* sm = find_mob(sk);
                if (!sm || is_taken(sk) || Raid::NoTank(map_id, sm->u->entry) || sm->u->is_dungeon_boss || sm->u->is_boss_rank)
                    continue;
                Member const* best = nullptr;
                size_t best_load = SIZE_MAX;
                for (Member const* t : tanks)
                {
                    if (!t->is_bot || !t->alive || user_sym.count(t->guid_low)) continue;
                    // The tank on a boss the rules give the second tank keeps him.
                    if (auto sit = sticky.find(t->guid_low); sit != sticky.end())
                        if (Mob const* held = find_mob(sit->second); held && Raid::OffTankMob(map_id, held->u->entry))
                            continue;
                    // A tank already on the other kill mark keeps it.
                    if (auto sit = sticky.find(t->guid_low); sit != sticky.end() &&
                        (sit->second == mk_grp->GetTargetIcon(7) || sit->second == mk_grp->GetTargetIcon(6)))
                        continue;
                    size_t load = 0;
                    for (Mob const& o : mobs)
                        if (o.u->victim == t->src->guid) ++load;
                    if (!best || (t->guid_low == main_tank && best->guid_low != main_tank) ||
                        (best->guid_low != main_tank && load < best_load))
                    {
                        best = t;
                        best_load = load;
                    }
                }
                if (!best) continue;
                if (auto sit = sticky.find(best->guid_low); sit != sticky.end())
                {
                    taken.erase(std::remove(taken.begin(), taken.end(), sit->second), taken.end());
                    sticky.erase(sit);
                }
                assign(best, *sm, kill_icon == 7 ? "owner_skull" : "owner_cross");
            }
        // -- The boss is never left without a tank ----------------------------
        // An engaged boss nobody holds (its tank died, or every tank is busy with adds) is the threat that wipes the raid;
        // the strongest bot tank drops its add for it, the add becoming a free mob for the next pass (Garr 2026-10-08: the
        // boss's tank died at 41 s, the two left kept their adds and nobody picked Garr up). A tank with the owner's symbol
        // or a human tank is not moved.
        for (auto const& m : mobs)
        {
            if (!(m.u->is_dungeon_boss || m.u->is_boss_rank) || Raid::NoTank(map_id, m.u->entry) || is_taken(m.u->guid))
                continue;
            bool const tank_last = Raid::TankLast(map_id, m.u->entry);
            bool const adds_up = tank_last &&
                std::any_of(mobs.begin(), mobs.end(), [&](Mob const& o) { return o.u->guid != m.u->guid && !Raid::NoTank(map_id, o.u->entry) && !Raid::TankLast(map_id, o.u->entry); });
            // No free tank leaves an add for him while an add is loose: the adds are the threat. Once every bot tank holds something he goes to
            // the tank with the fewest mobs on it, loose add or not (Majordomo: nobody held him, he stood on a healer all fight and
            // killed two by melee; with eight adds one is always loose, so he stood on one add after the other for 30 s).
            bool const free_tank = std::any_of(tanks.begin(), tanks.end(), [&](Member const* t)
                { return t->is_bot && t->alive && !user_sym.count(t->guid_low) && !sticky.count(t->guid_low); });
            if (adds_up && free_tank && std::any_of(mobs.begin(), mobs.end(), [&](Mob const& o) { return o.loose && !Raid::NoTank(map_id, o.u->entry) && !Raid::TankLast(map_id, o.u->entry); }))
                continue;
            Member const* best = nullptr;
            size_t best_load = SIZE_MAX;
            for (Member const* t : tanks)
            {
                if (!t->is_bot || user_sym.count(t->guid_low)) continue;
                size_t load = 0;
                if (adds_up)
                    for (Mob const& o : mobs)
                        if (o.u->victim == t->src->guid && o.u->guid != m.u->guid) ++load;
                // The main tank first, else the healthiest (else, with adds up, the lightest load first).
                bool const better = !best || (adds_up ? (load < best_load || (load == best_load && Pct(t->src->hp, t->src->max_hp) > Pct(best->src->hp, best->src->max_hp)))
                    : ((t->guid_low == main_tank && best->guid_low != main_tank) ||
                       (best->guid_low != main_tank && Pct(t->src->hp, t->src->max_hp) > Pct(best->src->hp, best->src->max_hp))));
                if (better)
                {
                    best = t;
                    best_load = load;
                }
            }
            if (!best) break;
            if (auto sit = sticky.find(best->guid_low); sit != sticky.end())
            {
                taken.erase(std::remove(taken.begin(), taken.end(), sit->second), taken.end());
                sticky.erase(sit);
            }
            assign(best, m, "boss_untanked");
            break;
        }
        // -- Pack mobs not pulled yet ---------------------------------------
        // The pack is known before it engages (Molten Giants link-pull, Core Hounds): an idle mob standing by an engaged
        // one. Each duty tank still without a mob is reserved a distinct pack mob (nearest first), so it is not sent at
        // the main tank's mob and nobody else takes it. A reserved mob is never in tank_target (no pull); the tank
        // holds until the mob engages, and the next plan pass (500 ms) assigns it. A tank with neither waits too.
        std::vector<Member const*> waiting;
        if (!mobs.empty())
        {
            std::vector<NearbyUnit const*> pack;
            for (auto const& sn : snaps)
            {
                if (sn->position.map_id != map_id) continue;
                for (auto const& u : sn->combat.nearby_enemies)
                {
                    if (u.hp <= 0 || u.is_player || u.untargetable || u.is_pacified || u.unit_in_combat) continue;
                    if (!u.victim.IsEmpty() || find_mob(u.guid) || (u.is_cc_locked && member_of(u.cc_caster))) continue;
                    bool known = false;
                    for (NearbyUnit const* p : pack)
                        if (p->guid == u.guid) { known = true; break; }
                    if (known) continue;
                    for (auto const& m : mobs)
                    {
                        const float dx = u.x - m.u->x, dy = u.y - m.u->y;
                        if (dx * dx + dy * dy <= kPackReach * kPackReach) { pack.push_back(&u); break; }
                    }
                }
            }
            for (Member const* t : tanks)
            {
                if (!t->is_bot || sticky.count(t->guid_low)) continue;
                if (t->guid_low != main_tank) waiting.push_back(t);
                if (reserved.count(t->guid_low)) continue;   // its mob carries its symbol
                NearbyUnit const* best = nullptr;
                float best_d2 = kTankAssignRange * kTankAssignRange;
                for (NearbyUnit const* p : pack)
                {
                    if (is_taken(p->guid)) continue;
                    const float dx = p->x - t->src->x, dy = p->y - t->src->y;
                    if (dx * dx + dy * dy < best_d2) { best = p; best_d2 = dx * dx + dy * dy; }
                }
                if (best)
                {
                    taken.push_back(best->guid);
                    reserved[t->guid_low] = best->guid;
                    TC_LOG_INFO("playerbot.v2",
                        "[raid_tank] group={} map={} {} reserved pack mob {} ({}) not pulled yet, holds until it engages",
                        group_low, map_id, t->src->name, MobName(best->entry), best->guid.GetCounter());
                }
            }
        }
        for (Member const* t : waiting)
            if (auto it = plan.find(t->guid_low); it != plan.end())
                it->second.tank_hold = true;
        for (auto const& [tlow, mob] : sticky)
            if (auto it = plan.find(tlow); it != plan.end())
                it->second.tank_target = mob;
        // Every tank also learns what the others own (humans' mobs too),
        // so its loose-mob taunt never steals one.
        for (Member const* t : tanks)
        {
            auto it = plan.find(t->guid_low);
            if (it == plan.end()) continue;
            size_t n = 0;
            for (ObjectGuid const& g : taken)
            {
                if (g == it->second.tank_target) continue;
                if (n >= PveOrder::kMaxClaimedMobs) break;
                it->second.claimed_by_other_tanks[n++] = g;
            }
        }
        // Who holds which mob, logged when it changes: the evidence for a tank contest on a loose mob.
        {
            static std::unordered_map<uint64, std::string> s_last_assign;
            std::string line;
            for (Member const* t : tanks)
            {
                auto it = plan.find(t->guid_low);
                if (it == plan.end()) continue;
                line += t->src->name + (t->guid_low == main_tank ? "(MT)" : "") + "=" + std::to_string(it->second.tank_target.GetCounter()) +
                        "/claims" + std::to_string([&] { size_t c = 0; for (ObjectGuid const& g : it->second.claimed_by_other_tanks) c += !g.IsEmpty(); return c; }()) +
                        (it->second.tank_hold ? "/hold" : "") + " ";
            }
            std::string& last = s_last_assign[bkey];
            if (line != last)
            {
                last = line;
                TC_LOG_INFO("playerbot.v2", "[raid_tank] group={} map={} assignment: {}", group_low, map_id, line.empty() ? std::string("no tanks") : line);
            }
        }

        // -- Automatic raid markers (Playerbot.Behavior.TankMarkers) -----------
        // Every duty tank's mob (engaged, or the pack mob reserved for it) gets the next marker in star, circle, diamond,
        // triangle, moon, square order; skull and cross stay the owner's kill call. A tank keeps its marker
        // for the pull, and the marker then keeps the mob with that tank (override above). Marks are set from here,
        // server side on the group's icon store: an assistant flag is not needed, so it works under a human leader.
        // Edge-triggered: an icon already on the mob is left alone, nothing is re-sent. A mark someone else put on a
        // live mob is never overwritten. Marks this code set are cleared when their mob is dead or gone, or idle
        // once the pack is over, so no leftover skull can start the next opener (see State_InGroup auto-skull).
        if (mk_grp)
        {
            std::array<ObjectGuid, 8>& ours = marks_set_[bkey];
            auto unit_of = [&](ObjectGuid g) -> NearbyUnit const*
            {
                for (auto const& sn : snaps)
                {
                    if (sn->position.map_id != map_id) continue;
                    for (auto const& u : sn->combat.nearby_enemies)
                        if (u.guid == g) return &u;
                }
                return nullptr;
            };
            ObjectGuid const by = mk_grp->GetLeaderGUID();
            // Pre-pull crowd control (PrePullCc.h): the tank's request for one pack mob to be held by the mage. The moon
            // stays on that mob while the request lives, though the pack is idle ("pack over" below would clear it).
            constexpr uint8 kMoon = 4;
            PrePullCc::Request pcc;
            bool const have_pcc = PrePullCc::Get(group_low, now_ms, pcc);
            for (uint8 m = 0; m < 8; ++m)
            {
                if (ours[m].IsEmpty()) continue;
                if (mk_grp->GetTargetIcon(m) != ours[m]) { ours[m] = ObjectGuid::Empty; continue; }   // changed by someone
                if (have_pcc && m == kMoon && ours[m] == pcc.mob) continue;
                NearbyUnit const* u = unit_of(ours[m]);
                const bool gone = !u || u->hp <= 0;
                const bool pack_over = u && mobs.empty() && !u->unit_in_combat && u->victim.IsEmpty();
                if (gone || pack_over)
                {
                    mk_grp->SetTargetIcon(m, ObjectGuid::Empty, by);
                    ours[m] = ObjectGuid::Empty;
                }
            }
            auto& auto_map = tank_mark_auto_[bkey];
            if (mobs.empty())
            {
                auto_map.clear();
                mk_eff.clear();
                for (auto const& [tl, mkr] : Services::Owners().TankMarks(group_low))
                    mk_eff[tl] = mkr;
                for (auto const& [tl, sym] : user_sym)
                {
                    for (auto it = mk_eff.begin(); it != mk_eff.end();)
                        it = (it->first == tl || it->second == sym) ? mk_eff.erase(it) : std::next(it);
                    mk_eff[tl] = sym;
                }
            }
            // Never skull or cross: those are the owner's kill call (explicit_kill_mark), not a tank's assignment (Garr 2026-10-08: the
            // main tank marked the boss with skull and the raid left the adds).
            // Nor moon: it is the owner's crowd-control call, and a caster holds whatever carries it.
            static constexpr uint8 kOrder[] = { 0, 1, 2, 3, 5 };
            // An icon is never used automatically while a member holds it (the owner's symbol on a bot).
            auto marker_used = [&](uint8 m)
            {
                if (held_by[m]) return true;
                for (auto const& [tl, mkr] : mk_eff) if (mkr == m) return true;
                return false;
            };
            for (Member const* t : tanks)
            {
                if (!t->is_bot || user_sym.count(t->guid_low)) continue;   // the owner's symbol on the tank itself
                ObjectGuid mob;
                if (auto sit = sticky.find(t->guid_low); sit != sticky.end()) mob = sit->second;
                else if (auto rit = reserved.find(t->guid_low); rit != reserved.end()) mob = rit->second;
                if (mob.IsEmpty()) continue;
                uint8 m = 0xFF;
                if (auto eit = mk_eff.find(t->guid_low); eit != mk_eff.end())
                    m = eit->second;
                else
                {
                    for (uint8 o : kOrder)
                        if (!marker_used(o)) { m = o; break; }
                    if (m == 0xFF) continue;
                    mk_eff[t->guid_low] = m;
                    auto_map[t->guid_low] = m;
                }
                ObjectGuid const cur = mk_grp->GetTargetIcon(m);
                if (cur == mob) { ours[m] = mob; continue; }
                if (!cur.IsEmpty() && cur != ours[m])
                    if (NearbyUnit const* cu = unit_of(cur); cu && cu->hp > 0)
                        continue;   // somebody else's call on a live mob
                // The mob already carries an icon someone else put on it (SetTargetIcon would strip it): theirs wins.
                bool user_marked = false;
                for (uint8 m2 = 0; m2 < 8 && !user_marked; ++m2)
                    user_marked = mk_grp->GetTargetIcon(m2) == mob && ours[m2] != mob;
                if (user_marked) continue;
                mk_grp->SetTargetIcon(m, mob, t->src->guid);
                ours[m] = mob;
                TC_LOG_INFO("playerbot.v2", "[raid_tank] group={} map={} {} marks {} with raid icon {}",
                    group_low, map_id, t->src->name, mob.GetCounter(), uint32(m));
            }

        }

        // -- Generic raid kill focus ----------------------------------------
        // the owner's raid DPS target (DpsTargetValue McFocusTarget): every DPS
        // on ONE engaged, non-CC'd mob, lowest health first, instead of each
        // bot opening on whatever is nearest - 30 DPS spread over a pack
        // out-threat every tank at once. A dungeon script's priority add
        // (script_focus) wins. Candidates, best first:
        //   1. a LOOSE mob (on a non-tank member) no tank has been given -
        //      it is chewing on a healer / caster and nobody is coming;
        //   2. a mob a tank is holding (its victim is a tank member).
        // Loose mobs a tank is on the way to are left to that tank (damage
        // would only bounce them between DPS). The per-bot threat hold still
        // keeps each DPS off the focus until the tank has its opening lead.
        // Sticky while the previous focus stays a candidate of the best
        // class present, so the raid does not flap between equal mobs.
        // The leader's / owner's skull (else cross) mark overrides the pick
        // while the marked mob is alive in sight, not crowd-controlled by the
        // group and engaged or idle (an idle marked mob is the owner's call:
        // the per-bot hold gives the tank 4s to take it first). An idle mob
        // counts only under a human leader (idle_mark_is_call): a bot
        // leader's mark is its in-combat auto-skull, and one on an idle mob
        // is left over from the last fight (MC 2026-10-06: a skull from the
        // previous wipe on an evaded Molten Giant became the raid's kill
        // focus while the tank held the pull).
        // With tank markers on, the next marker in order (cross, star, circle, diamond, triangle, moon, square)
        // follows the skull as the kill target; those engaged mobs only (a marker on an idle mob is no kill call).
        NearbyUnit const* marked = nullptr;
        std::vector<ObjectGuid> mark_cands;
        if (script_focus.IsEmpty())
        {
            if (!kill_mark.IsEmpty())
                mark_cands.push_back(kill_mark);
            // An icon that is a crowd-control symbol (a caster bot carries it, or a bot's `rti cc` lists it) is no kill
            // call: its mob is controlled until the owner turns it skull.
            auto cc_symbol = [&](uint8 m) -> bool
            {
                if (m == 7 || !Services::Config().behavior_mark_cc() || !GroupCcSwitchOn(mk_grp)) return false;
                if (held_by[m])
                    if (auto h = by_low.find(held_by[m]); h != by_low.end() && h->second->is_bot && !h->second->tank &&
                        MarkCc::IsMarkCcClass(h->second->cls))
                        return true;
                for (Member const& e : mem)
                    if (e.is_bot)
                        for (uint8 c : Services::Owners().RtiOrderOf(e.guid_low).cc)
                            if (c == m) return true;
                return false;
            };
            if (mk_grp)
                for (uint8 m : { uint8(7), uint8(6), uint8(0), uint8(1), uint8(2), uint8(3), uint8(4), uint8(5) })
                {
                    // Only skull and cross are kill orders; the other icons are tank / crowd-control markers (playtest, MC Garr
                    // 2026-10-08: circle marked on Garr and every DPS attacked it). An icon some bot has as its own kill symbol
                    // (assign <bot> <icon> kill) stays a kill order for the group.
                    bool kill_icon = m >= 6;
                    if (!kill_icon)
                        for (Member const& e : mem)
                            if (e.is_bot)
                                for (uint8 c : Services::Owners().RtiOrderOf(e.guid_low).kill)
                                    if (c == m) kill_icon = true;
                    if (!kill_icon) continue;
                    if (ObjectGuid const g = mk_grp->GetTargetIcon(m); !g.IsEmpty() && g != kill_mark && !cc_symbol(m))
                        mark_cands.push_back(g);
                }
        }
        // MarksNeedOrder: a skull / cross on a mob nobody fights is only the owner's call once he ordered the attack.
        const bool idle_call = idle_mark_is_call &&
            (!Services::Config().behavior_marks_need_order() ||
             (mark_release_until_.count(bkey) && now_ms < mark_release_until_[bkey]));
        // Adds before the boss: a boss that merely carries a mark (a tank assignment on Garr) is the kill focus only when no
        // other marked mob is engaged and alive (first pass skips bosses; the second takes them). The owner's skull / cross
        // is always his call, boss or not.
        // An engaged add (non-boss, not crowd-controlled) alive: the boss is never the focus, not by a mark and not by the generic pick
        // below (Garr 2026-10-08: the skull add was banished, the next mark was Garr).
        bool adds_alive = false;
        for (auto const& m : mobs)
            if (!(m.u->is_dungeon_boss || m.u->is_boss_rank)) { adds_alive = true; break; }
        auto is_boss = [](NearbyUnit const& u) { return u.is_dungeon_boss || u.is_boss_rank; };
        for (int pass = 0; pass < (adds_alive ? 1 : 2) && !marked; ++pass)
        for (ObjectGuid const& mc : mark_cands)
        {
            const bool explicit_call = mc == kill_mark;
            for (auto const& sn : snaps)
            {
                if (sn->position.map_id != map_id) continue;
                for (auto const& u : sn->combat.nearby_enemies)
                    if (u.guid == mc)
                    {
                        if (pass == 0 && !explicit_call && (u.is_dungeon_boss || u.is_boss_rank)) break;   // not yet: adds first
                        if (u.hp > 0 && !u.is_player && !u.untargetable &&
                            // A mob immune to every school (banished, bubbled) cannot be the focus: the raid would stand and wait on it
                            // (Garr 2026-10-09: the skull Firesworn read all-school immune and 3 DPS held for minutes). The next mark or the
                            // generic pick takes over; the skull returns to it once the immunity ends.
                            !Raid::GetBrain().AllSchoolsAvoided(group_low, map_id, u.guid) &&
                            // A skull the owner puts on a crowd-controlled mob is the call to kill it, in a fight too: the caster stops
                            // holding it (TryCast refuses the kill target) and the raid switches (n71 Majordomo: the owner rotates the
                            // skull onto each sheeped Healer in turn). Any other mark on a held mob is no kill call.
                            !(u.is_cc_locked && member_of(u.cc_caster) && !explicit_call) &&
                            (u.unit_in_combat || (explicit_call && idle_call)))
                            marked = &u;
                        break;
                    }
                if (marked) break;
            }
            if (marked) break;
        }
        if (marked)
        {
            ObjectGuid const prev = [&]() -> ObjectGuid
            {
                auto it = last_raid_focus_.find(bkey);
                return it != last_raid_focus_.end() ? it->second : ObjectGuid::Empty;
            }();
            if (prev != marked->guid)
                TC_LOG_INFO("playerbot.v2",
                    "[raid_threat] group={} map={} kill focus -> {} ({}) {}% (raid mark{})",
                    group_low, map_id, MobName(marked->entry), marked->guid.GetCounter(),
                    Pct(marked->hp, marked->max_hp), marked->unit_in_combat ? "" : ", not pulled yet");
            last_raid_focus_[bkey] = marked->guid;
            Raid::GetBrain().NoteKillFocus(group_low, map_id, marked->guid);
            for (auto const& e : mem)
            {
                if (e.tank || e.healer) continue;
                if (auto it = plan.find(e.guid_low); it != plan.end())
                    it->second.kill_focus = marked->guid;
            }
        }
        else if (script_focus.IsEmpty())
        {
            auto tank_held = [&](Mob const& m)
            {
                Member const* v = member_of(m.u->victim);
                return v && v->tank;
            };
            auto loose_unowned = [&](Mob const& m) { return m.loose && !is_taken(m.u->guid); };
            Mob const* pick = nullptr;
            bool pick_loose = false;
            // A mob the encounter's rules mark "never a DPS target" (Majordomo: he surrenders when the adds die) is no focus, with or
            // without adds engaged this tick.
            Raid::RaidRules const* const focus_rules = Raid::RulesForMap(map_id);
            auto never_target = [&](NearbyUnit const& u) { return focus_rules && Raid::PriorityFor(*focus_rules, u.entry) < 0; };
            for (auto const& m : mobs)
            {
                if (never_target(*m.u)) continue;
                if (adds_alive && is_boss(*m.u)) continue;   // adds first, the boss last
                const bool lu = loose_unowned(m);
                if (!lu && !tank_held(m)) continue;
                if (pick && pick_loose && !lu) continue;
                if (!pick || (lu && !pick_loose) || m.u->hp < pick->u->hp ||
                    (m.u->hp == pick->u->hp && m.u->guid.GetCounter() < pick->u->guid.GetCounter()))
                {
                    pick = &m;
                    pick_loose = lu;
                }
            }
            // No symbols: the raid kills the main tank's target (the first duty tank's when none is designated) - a
            // loose mob nobody has been sent to (above) still comes first. Only when the main tank has no engaged
            // target does the lowest-health tank-held mob stand in.
            bool pick_is_mt = false;
            if (!pick_loose)
            {
                Member const* mtm = nullptr;
                if (main_tank)
                    if (auto mt = by_low.find(main_tank); mt != by_low.end() && mt->second->alive) mtm = mt->second;
                if (!mtm && !tanks.empty()) mtm = tanks.front();
                if (mtm)
                    if (Mob const* mm = find_mob(mtm->src->victim); mm && !(adds_alive && is_boss(*mm->u)) && !never_target(*mm->u))
                    {
                        pick = mm;
                        pick_is_mt = true;
                    }
            }
            ObjectGuid const prev = [&]() -> ObjectGuid
            {
                auto it = last_raid_focus_.find(bkey);
                return it != last_raid_focus_.end() ? it->second : ObjectGuid::Empty;
            }();
            // The previous focus is kept while it is still a fair pick: a boss is not once adds are engaged (n31 mc_majordomo: the focus
            // fell on Majordomo in the first second, before the adds were counted, and the stickiness kept it for two minutes while the
            // raid hit a boss that cannot die and the adds killed it).
            // The main tank's target is no reason to leave a mob that a tank holds too: the tank's pick moves with every stun and
            // drop, and the focus followed it every 2 s (UBRS 20 Oct, 27 -> 30 -> 27), a fresh curse and a new target each time.
            if (Mob const* pm = find_mob(prev); pm && pick && pm != pick && !(pick_is_mt && !tank_held(*pm)) && !(adds_alive && is_boss(*pm->u)) && !never_target(*pm->u))
            {
                const bool plu = loose_unowned(*pm);
                if ((plu || tank_held(*pm)) && (plu || !pick_loose))
                {
                    pick = pm;
                    pick_loose = plu;
                }
            }
            ObjectGuid const focus = pick ? pick->u->guid : ObjectGuid::Empty;
            if (focus != prev)
            {
                if (pick)
                {
                    Member const* v = member_of(pick->u->victim);
                    TC_LOG_INFO("playerbot.v2",
                        "[raid_threat] group={} map={} kill focus -> {} ({}) {}% on {} ({}), mobs engaged={}",
                        group_low, map_id, MobName(pick->u->entry), pick->u->guid.GetCounter(),
                        Pct(pick->u->hp, pick->u->max_hp), v ? v->src->name : std::string("-"),
                        pick_loose ? "loose, no tank assigned" : (pick_is_mt ? "main tank's target" : "tank-held"),
                        uint32(mobs.size()));
                }
                else
                    TC_LOG_INFO("playerbot.v2",
                        "[raid_threat] group={} map={} kill focus cleared (no tank-held or loose mob)",
                        group_low, map_id);
            }
            if (focus.IsEmpty())
            {
                last_raid_focus_.erase(bkey);
                Raid::GetBrain().NoteKillFocus(group_low, map_id, ObjectGuid::Empty);
            }
            else
            {
                last_raid_focus_[bkey] = focus;
                Raid::GetBrain().NoteKillFocus(group_low, map_id, focus);
                for (auto const& e : mem)
                {
                    if (e.tank || e.healer) continue;   // tanks hold aggro, healers heal
                    if (auto it = plan.find(e.guid_low); it != plan.end())
                        it->second.kill_focus = focus;
                }
            }
        }
        else
            last_raid_focus_.erase(bkey);
    }
    else
    {
        last_tank_target_.erase(bkey);
        cc_held_.erase(bkey);
        last_raid_focus_.erase(bkey);
        tank_mark_auto_.erase(bkey);
        marks_set_.erase(bkey);
    }

    // The fight diagnostics are for raids: a 5-man pull is short and the
    // per-3s lines would drown the dungeon logs.
    if (!is_raid)
    {
        fight_log_.erase(bkey);
        return;
    }

    // -- Fight diagnostics --------------------------------------------------
    FightLog& fl = fight_log_[bkey];
    bool any_combat = !mobs.empty();
    for (auto const& e : mem)
        if (e.alive && e.src->in_combat) { any_combat = true; break; }

    auto pct_hp = [](GroupMemberSummary const& m) { return Pct(m.hp, m.max_hp); };
    auto role_tag = [&](Member const& e) -> char const*
    {
        if (e.guid_low == main_tank) return "main tank";
        if (e.guid_low == off_tank)  return "off tank";
        if (e.tank)   return "tank";
        if (e.healer) return "healer";
        // A dead tank drops off the roster and reads as DPS; label it by what it is (2026-10-08: Raveni died as "dps bot").
        if (!e.alive && (e.src->role == Role::Tank || IsTankSpec(e.src->cls, e.spec))) return "tank";
        return "dps";
    };

    if (any_combat && !fl.in_fight)
    {
        fl.in_fight = true;
        fl.started_ms = now_ms;
        fl.last_summary_ms = 0;
        fl.deaths = 0;
        fl.mob_hp.clear();
        fl.dmg_total = 0;
        fl.dmg_last_ms = 0;
        fl.start_mana.clear();
        for (auto const& e : mem)
        {
            TakeDamageDone(e.guid_low, true);
            if (e.src && e.src->max_mana > 0) fl.start_mana[e.guid_low] = Pct(e.src->mana, e.src->max_mana);
        }
        uint32 alive = 0, tanks = 0, healers = 0;
        for (auto const& e : mem)
        {
            if (!e.alive) continue;
            ++alive;
            tanks += e.tank;
            healers += e.healer;
        }
        TC_LOG_INFO("playerbot.v2",
            "[raid_fight] group={} map={} pull started: alive={}/{} tanks={} healers={} mobs={}",
            group_low, map_id, alive, uint32(mem.size()), tanks, healers, uint32(mobs.size()));
    }

    // Deaths since the previous plan, with what was hitting them then.
    for (auto const& e : mem)
    {
        auto prev = fl.alive.find(e.guid_low);
        if (prev == fl.alive.end() || !prev->second || e.alive)
            continue;
        if (fl.in_fight) ++fl.deaths;
        auto hb = fl.hit_by.find(e.guid_low);
        auto hp = fl.hp_pct.find(e.guid_low);
        // An area spell has no victim on the mob: the death hook's killing blow says what it was.
        std::string const killing_blow = V2::DeathCause::Take(e.guid_low);
        std::string hit_by = hb != fl.hit_by.end() ? hb->second : std::string();
        if (!killing_blow.empty())
            hit_by += (hit_by.empty() ? "" : "; ") + std::string("killing blow ") + killing_blow;
        TC_LOG_INFO("playerbot.v2",
            "[raid_death] group={} map={} t={}s {} ({} {}) died; last seen {}% hp, hit by: {}",
            group_low, map_id, fl.in_fight ? (now_ms - fl.started_ms) / 1000 : 0,
            e.src->name, role_tag(e), e.is_bot ? "bot" : "player",
            hp != fl.hp_pct.end() ? hp->second : -1,
            hit_by.empty() ? std::string("nothing seen") : hit_by);
    }
    for (auto const& e : mem)
    {
        std::string hit;
        for (auto const& m : mobs)
        {
            if (m.u->victim != e.src->guid) continue;
            if (!hit.empty()) hit += ", ";
            hit += MobName(m.u->entry) + " " + std::to_string(Pct(m.u->hp, m.u->max_hp)) + "%";
        }
        fl.alive[e.guid_low]  = e.alive;
        fl.hit_by[e.guid_low] = std::move(hit);
        fl.hp_pct[e.guid_low] = pct_hp(*e.src);
    }

    if (fl.in_fight && now_ms - fl.last_summary_ms >= kFightSummaryMs)
    {
        fl.last_summary_ms = now_ms;
        uint32 alive = 0;
        for (auto const& e : mem)
            alive += e.alive;

        std::string tanks;
        auto const* sticky = [&]() -> std::unordered_map<uint64, ObjectGuid> const*
        {
            auto it = last_tank_target_.find(bkey);
            return it == last_tank_target_.end() ? nullptr : &it->second;
        }();
        for (auto const& e : mem)
        {
            if (!e.tank || !e.alive) continue;
            if (!tanks.empty()) tanks += ", ";
            // Where it stands: a tank 40-50 y from the healers' post is out of their reach (bwl_nefarian 1110a: the healers stepped toward
            // tanks 40-51 y off for sight and never healed, and the log could not say where the tanks were).
            tanks += e.src->name + " " + std::to_string(pct_hp(*e.src)) + "% @" + std::to_string(int32(e.src->x)) + "," +
                     std::to_string(int32(e.src->y)) + ">";
            if (Mob const* v = find_mob(e.src->victim))
                tanks += MobName(v->u->entry) + " " + std::to_string(Pct(v->u->hp, v->u->max_hp)) + "%";
            else
                tanks += "-";
            if (sticky)
                if (auto it = sticky->find(e.guid_low); it != sticky->end() && it->second != e.src->victim)
                    if (Mob const* want = find_mob(it->second))
                        tanks += " (assigned " + MobName(want->u->entry) + ")";
        }

        std::string mob_txt;
        uint32 loose = 0, listed = 0;
        for (auto const& m : mobs)
        {
            loose += m.loose;
            if (listed++ >= 8) continue;
            if (!mob_txt.empty()) mob_txt += ", ";
            Member const* v = member_of(m.u->victim);
            mob_txt += MobName(m.u->entry) + " " + std::to_string(Pct(m.u->hp, m.u->max_hp)) + "%>" +
                       (v ? v->src->name : std::string("pet")) + (m.loose ? "!" : "");
        }

        uint32 healers = 0, low_mana = 0, casting = 0;
        int64 mana_sum = 0;
        for (auto const& e : mem)
        {
            if (!e.healer || !e.alive) continue;
            ++healers;
            const int32 mp = Pct(e.src->mana, e.src->max_mana);
            mana_sum += mp;
            low_mana += mp < 20;
            casting += e.src->is_casting;
        }

        // DPS throughput: what the damage dealers were doing at this instant, and the raid's damage rate (health the
        // engaged mobs lost since the previous summary) per DPS bot.
        uint32 dps_n = 0, dps_cast = 0, dps_hit = 0, dps_none = 0, dps_focus = 0;
        ObjectGuid const fight_focus = [&]() -> ObjectGuid
        {
            auto fit = last_raid_focus_.find(bkey);
            return fit != last_raid_focus_.end() ? fit->second : ObjectGuid::Empty;
        }();
        int64 dps_mana_sum = 0, dps_mana_n = 0;
        for (auto const& e : mem)
        {
            if (e.tank || e.healer || !e.alive || !e.is_bot) continue;
            ++dps_n;
            if (e.src->is_casting) ++dps_cast;
            else if (!e.src->victim.IsEmpty()) ++dps_hit;
            if (!fight_focus.IsEmpty() && e.src->victim == fight_focus) ++dps_focus;
            // Idle = no victim and not casting. A bot on another mob than the focus is fighting (it counts under hitting).
            if (e.src->victim.IsEmpty() && !e.src->is_casting) ++dps_none;
            if (e.src->max_mana > 0) { dps_mana_sum += Pct(e.src->mana, e.src->max_mana); ++dps_mana_n; }
        }
        uint64 dmg_window = 0;
        for (auto const& m : mobs)
        {
            int64 const cur = m.u->hp;
            auto [it, fresh] = fl.mob_hp.try_emplace(m.u->guid.GetCounter(), cur);
            if (!fresh)
            {
                if (cur < it->second) dmg_window += uint64(it->second - cur);
                it->second = cur;
            }
        }
        fl.dmg_total += dmg_window;
        uint32 const win_ms = fl.dmg_last_ms ? now_ms - fl.dmg_last_ms : 0;
        fl.dmg_last_ms = now_ms;
        uint32 const secs = std::max<uint32>((now_ms - fl.started_ms) / 1000, 1);
        uint32 const win_dps = win_ms ? uint32(dmg_window * 1000ull / win_ms) : 0;

        TC_LOG_INFO("playerbot.v2",
            "[raid_fight] group={} map={} t={}s alive={}/{} deaths={} | tanks: {} | mobs {} (loose {}): {} | "
            "healers {} mana avg {}% under20% {} casting {} | dps bots {} casting {} hitting {} idle {} (on the focus {}) mana avg {}% | "
            "raid damage {}/s now, {}/s avg, {} per dps bot",
            group_low, map_id, (now_ms - fl.started_ms) / 1000, alive, uint32(mem.size()), fl.deaths,
            tanks.empty() ? std::string("none alive") : tanks, uint32(mobs.size()), loose,
            mob_txt.empty() ? std::string("-") : mob_txt, healers,
            healers ? int32(mana_sum / healers) : 0, low_mana, casting,
            dps_n, dps_cast, dps_hit, dps_none, dps_focus, dps_mana_n ? int32(dps_mana_sum / dps_mana_n) : 0,
            win_dps, uint32(fl.dmg_total / secs), dps_n ? uint32(win_dps / dps_n) : 0);
    }

    if (!any_combat && fl.in_fight)
    {
        uint32 alive = 0;
        for (auto const& e : mem)
            alive += e.alive;
        TC_LOG_INFO("playerbot.v2",
            "[raid_fight] group={} map={} pull ended after {}s: alive={}/{} deaths={} | raid damage seen {} ({}/s)",
            group_low, map_id, (now_ms - fl.started_ms) / 1000, alive, uint32(mem.size()), fl.deaths,
            fl.dmg_total, fl.dmg_total / std::max<uint32>((now_ms - fl.started_ms) / 1000, 1));
        // One line per pull: what each damage dealer did to the mobs (own hits, DoTs and pet), highest first, so per-class tuning has numbers.
        std::vector<std::pair<uint64, std::string>> rows;
        uint32 const dur = std::max<uint32>((now_ms - fl.started_ms) / 1000, 1);
        for (auto const& e : mem)
        {
            uint64 const d = TakeDamageDone(e.guid_low, true);
            if (e.tank || e.healer || !e.is_bot || !d) continue;
            std::string mana;
            auto const sm = fl.start_mana.find(e.guid_low);
            if (sm != fl.start_mana.end() && e.src->max_mana > 0)
                mana = ", mana " + std::to_string(sm->second) + ">" + std::to_string(Pct(e.src->mana, e.src->max_mana)) + "%";
            rows.emplace_back(d, std::string(e.src->name) + "(" + ShortClass(e.cls) + ") " + std::to_string(d) + " (" +
                                     std::to_string(d / dur) + "/s" + mana + ")");
        }
        std::sort(rows.begin(), rows.end(), [](auto const& a, auto const& b) { return a.first > b.first; });
        std::string txt;
        for (auto const& r : rows) { if (!txt.empty()) txt += ", "; txt += r.second; }
        if (!txt.empty())
            TC_LOG_INFO("playerbot.v2", "[raid_dps] group={} map={} pull of {}s, damage per dps bot: {}", group_low, map_id, dur, txt);
        fl.in_fight = false;
    }
}

namespace {

constexpr uint32 CURSE_OF_ELEMENTS     = 1490;
constexpr uint32 CURSE_OF_SHADOW       = 17862;
constexpr uint32 CURSE_OF_RECKLESSNESS = 704;
constexpr uint32 CURSE_OF_AGONY        = 980;
constexpr uint32 FEAR_WARD             = 6346;
constexpr uint32 TREMOR_TOTEM          = 8143;
// A curse is recast when this warlock's copy has less than this left.
constexpr int32  kCurseRefreshMs       = 3000;

char const* CurseName(uint32 curse)
{
    switch (curse)
    {
        case CURSE_OF_ELEMENTS:     return "Curse of the Elements";
        case CURSE_OF_SHADOW:       return "Curse of Shadow";
        case CURSE_OF_RECKLESSNESS: return "Curse of Recklessness";
        case CURSE_OF_AGONY:        return "Curse of Agony";
        default:                    return "none";
    }
}

uint32 SpellRoot(uint32 id)
{
    return SpellFamily::Root(id);
}

// The engaged boss the group is fighting (first seen across the members'
// views), optionally restricted to the fearing bosses; nullptr when none.
NearbyUnit const* EngagedBoss(uint32 map_id, std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                              bool fearing_only)
{
    for (auto const& sn : snaps)
    {
        if (sn->position.map_id != map_id) continue;
        for (NearbyUnit const& u : sn->combat.nearby_enemies)
            if (u.hp > 0 && !u.is_player && Classic::IsBossUnit(u) && u.unit_in_combat
                && Combat::Interrupt::EngagedWithGroup(u)
                && (!fearing_only || Classic::BossFears(map_id, u.entry)))
                return &u;
    }
    return nullptr;
}

// Debuff slots on a boss: 1.12 holds 16 harmful auras per unit, and a raid leader fills them by value (Sunder, the curses, Faerie Fire,
// Fire Vulnerability, Shadow Weaving) before any filler. The core here has more room, so the rule is kept in the plan, not by the core.
constexpr uint32 kBossDebuffSlots = 16;

uint32 HarmfulAuraCount(Unit const* boss)
{
    uint32 n = 0;
    for (auto const& [id, app] : boss->GetAppliedAuras())
        if (!app->IsPositive())
            ++n;
    return n;
}

char const* ClassTag(uint8 cls)
{
    switch (cls)
    {
        case CLASS_WARRIOR: return "warrior";
        case CLASS_PALADIN: return "paladin";
        case CLASS_HUNTER:  return "hunter";
        case CLASS_ROGUE:   return "rogue";
        case CLASS_PRIEST:  return "priest";
        case CLASS_SHAMAN:  return "shaman";
        case CLASS_MAGE:    return "mage";
        case CLASS_WARLOCK: return "warlock";
        case CLASS_DRUID:   return "druid";
        default:            return "other";
    }
}

// What the raid has put on its boss: every harmful aura a player cast, with stacks, time left and the caster's class, and how many of
// the boss's debuff slots are taken. One line per 10 s while the boss is engaged.
void LogBossDebuffs(uint64 group_low, uint32 map_id, Creature* boss, uint32 now_ms)
{
    static std::unordered_map<uint64, uint32> s_last;
    uint32& last = s_last[group_low];
    if (last && now_ms - last < 10'000u)
        return;
    last = now_ms ? now_ms : 1u;
    std::string list;
    for (auto const& [id, app] : boss->GetAppliedAuras())
    {
        if (app->IsPositive())
            continue;
        Aura const* a = app->GetBase();
        Unit* caster = a->GetCaster();
        if (!caster || !caster->IsPlayer())
            continue;
        SpellInfo const* si = a->GetSpellInfo();
        if (!list.empty()) list += ", ";
        list += std::string(si && si->SpellName && (*si->SpellName)[LOCALE_enUS] ? (*si->SpellName)[LOCALE_enUS] : "?") + " " + std::to_string(a->GetId());
        if (a->GetStackAmount() > 1)
            list += " x" + std::to_string(a->GetStackAmount());
        list += std::string(" (") + ClassTag(caster->GetClass()) + (a->IsPermanent() ? "" : ", " + std::to_string(a->GetDuration() / 1000) + " s") + ")";
    }
    uint32 const slots = HarmfulAuraCount(boss);
    TC_LOG_INFO("playerbot.v2", "[raid_debuffs] group={} map={} {} {}% carries {}/{} debuff slots: {}{}",
        group_low, map_id, boss->GetName(), uint32(boss->GetHealthPct()), slots, kBossDebuffSlots,
        list.empty() ? std::string("none from the raid") : list, slots >= kBossDebuffSlots ? " (full)" : "");
}

} // namespace

void PveGroupCoordinator::PlanCurses(uint64 bkey, uint64 group_low, uint32 map_id, bool is_raid,
                                     std::vector<std::pair<uint64, Player*>> const& bots,
                                     std::vector<Member> const& mem,
                                     std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                                     std::unordered_map<uint64, PveOrder>& plan)
{
    // Raids only: a 5-man warlock keeps its own Agony + Siphon Life rotation.
    if (!is_raid || !IsVanillaRealm())
    {
        last_curse_.erase(bkey);
        return;
    }
    std::unordered_map<uint64, BotSnapshot const*> snap_of;
    for (auto const& sp : snaps)
        snap_of[sp->guid.GetCounter()] = sp.get();
    // What the raid has put on the boss it fights, whether or not a warlock is in it.
    if (NearbyUnit const* fb = EngagedBoss(map_id, snaps, false))
        for (auto const& [low, p] : bots)
            if (p && p->IsInWorld() && p->GetMapId() == map_id)
            {
                if (Creature* bc = ObjectAccessor::GetCreature(*p, fb->guid); bc && bc->IsAlive())
                    LogBossDebuffs(group_low, map_id, bc, GameTime::GetGameTimeMS());
                break;
            }
    // The raid's bot warlocks by guid (census order puts bots by guid).
    std::vector<Member const*> locks;
    std::vector<ObjectGuid> lock_guids;
    for (auto const& e : mem)
        if (e.is_bot && e.alive && e.cls == CLASS_WARLOCK && snap_of.count(e.guid_low) && plan.count(e.guid_low))
        {
            locks.push_back(&e);
            lock_guids.push_back(e.src->guid);
        }
    auto& last = last_curse_[bkey];
    if (locks.empty())
    {
        last.clear();
        return;
    }
    Player* anchor = nullptr;
    for (auto const& [low, p] : bots)
        if (p && p->IsInWorld() && p->GetMapId() == map_id) { anchor = p; break; }

    // Target: the engaged boss, else the raid's kill focus (PlanRaidTanks /
    // the script focus / the raid mark). Recklessness only on a boss.
    ObjectGuid target;
    bool target_boss = false;
    if (NearbyUnit const* boss = EngagedBoss(map_id, snaps, false))
    {
        target = boss->guid;
        target_boss = true;
    }
    else
        for (auto const& e : mem)
            if (auto it = plan.find(e.guid_low); it != plan.end() && !it->second.kill_focus.IsEmpty())
            {
                target = it->second.kill_focus;
                break;
            }
    Creature* mob = (anchor && !target.IsEmpty()) ? ObjectAccessor::GetCreature(*anchor, target) : nullptr;
    if (mob && !mob->IsAlive())
        mob = nullptr;

    auto knows = [&](Member const* l, uint32 spell) { return BotSnapshotView(*snap_of[l->guid_low]).knows_spell(spell); };
    auto immune = [&](uint32 curse)
    {
        SpellInfo const* si = sSpellMgr->GetSpellInfo(curse, DIFFICULTY_NONE);
        if (!si) return true;
        uint32 mask = 0;
        for (SpellEffectInfo const& eff : si->GetEffects())
            if (eff.IsEffect())
                mask |= 1u << eff.EffectIndex;
        return mob->IsImmunedToSpell(si, mask, anchor);
    };
    // One-per-target curse already up from a warlock outside the bot plan
    // (a human warlock): leave it to them.
    auto carried_by_other = [&](uint32 curse)
    {
        uint32 const root = SpellRoot(curse);
        return mob->GetAura([&](Aura const* a)
        {
            return SpellRoot(a->GetId()) == root
                && std::find(lock_guids.begin(), lock_guids.end(), a->GetCasterGUID()) == lock_guids.end();
        }) != nullptr;
    };

    std::unordered_map<uint64, uint32> assign;
    if (mob)
    {
        // Curse of the Elements pays when a fire / frost / shadow caster
        // besides its own warlock is present: a mage, or a second warlock.
        uint32 casters = 0;
        for (auto const& e : mem)
            casters += e.alive && !e.healer && (e.cls == CLASS_MAGE || e.cls == CLASS_WARLOCK);
        std::vector<uint32> wanted;
        if (casters >= 2) wanted.push_back(CURSE_OF_ELEMENTS);
        // Recklessness (the tanks' armour) before Shadow on a boss: with two warlocks Shadow used the second one and Recklessness never came.
        if (target_boss) wanted.push_back(CURSE_OF_RECKLESSNESS);
        wanted.push_back(CURSE_OF_SHADOW);
        for (uint32 curse : wanted)
        {
            if (immune(curse) || carried_by_other(curse)) continue;
            for (Member const* l : locks)
                if (!assign.count(l->guid_low) && knows(l, curse))
                {
                    assign[l->guid_low] = curse;
                    break;
                }
        }
        // Agony stacks across casters; two of them are plenty and leave the boss's debuff slots to Sunder / Faerie Fire / Expose.
        // Only while the boss has slots to spare: a full list loses a Sunder stack or a Faerie Fire to the next debuff cast.
        uint32 agony = 0;
        if (!immune(CURSE_OF_AGONY) && HarmfulAuraCount(mob) + 2 < kBossDebuffSlots)
            for (Member const* l : locks)
                if (agony < 2 && !assign.count(l->guid_low) && knows(l, CURSE_OF_AGONY))
                {
                    assign[l->guid_low] = CURSE_OF_AGONY;
                    ++agony;
                }
    }

    std::unordered_map<uint64, std::pair<uint32, ObjectGuid>> now;
    for (Member const* l : locks)
    {
        auto a = assign.find(l->guid_low);
        uint32 const spell = a != assign.end() ? a->second : 0;
        if (spell)
        {
            PveOrder& o = plan[l->guid_low];
            o.curse_spell = spell;
            o.curse_target = target;
            uint32 const root = SpellRoot(spell);
            Aura const* mine = mob->GetAura([&](Aura const* au)
            { return SpellRoot(au->GetId()) == root && au->GetCasterGUID() == l->src->guid; });
            o.curse_needed = !mine || (!mine->IsPermanent() && mine->GetDuration() < kCurseRefreshMs);
            now[l->guid_low] = { spell, target };
        }
        auto prev = last.find(l->guid_low);
        uint32 const prev_spell = prev != last.end() ? prev->second.first : 0;
        ObjectGuid const prev_target = prev != last.end() ? prev->second.second : ObjectGuid::Empty;
        if (spell == prev_spell && (!spell || target == prev_target))
            continue;
        if (spell)
            TC_LOG_INFO("playerbot.v2", "[raid_curse] group={} map={} {} -> {} on {} ({}){}",
                group_low, map_id, l->src->name, CurseName(spell), MobName(mob->GetEntry()), target.GetCounter(),
                target_boss ? " boss" : " kill focus");
        else
            TC_LOG_INFO("playerbot.v2", "[raid_curse] group={} map={} {} curse cleared ({})",
                group_low, map_id, l->src->name, mob ? "no curse left for it" : "no boss / kill focus");
    }
    last = std::move(now);
}

void PveGroupCoordinator::PlanFearAndResist(uint64 bkey, uint64 group_low, uint32 map_id, bool is_raid,
                                            uint64 main_tank, uint64 off_tank,
                                            std::vector<std::pair<uint64, Player*>> const& /*bots*/,
                                            std::vector<Member> const& mem,
                                            std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                                            std::unordered_map<uint64, PveOrder>& plan)
{
    if (!is_raid || !IsVanillaRealm())
    {
        last_fear_ward_.erase(bkey);
        last_fear_boss_.erase(bkey);
        last_resist_potion_.erase(bkey);
        return;
    }
    std::unordered_map<uint64, BotSnapshot const*> snap_of;
    for (auto const& sp : snaps)
        snap_of[sp->guid.GetCounter()] = sp.get();
    std::unordered_map<uint64, Member const*> by_low;
    for (auto const& e : mem)
        by_low[e.guid_low] = &e;
    auto name_of = [&](uint64 low) -> std::string
    {
        auto it = by_low.find(low);
        return it != by_low.end() ? it->second->src->name : std::to_string(low);
    };

    // -- Fear Ward: dwarf priests (by guid) on the main / off tank, one each.
    std::vector<uint64> tanks;
    for (uint64 t : { main_tank, off_tank })
        if (t) tanks.push_back(t);
    std::vector<uint64> priests;
    for (auto const& e : mem)
    {
        if (!e.is_bot || !e.alive || e.cls != CLASS_PRIEST) continue;
        auto it = snap_of.find(e.guid_low);
        if (it == snap_of.end()) continue;
        BotSnapshotView const v(*it->second);
        if (v.race() == RACE_DWARF && v.knows_spell(FEAR_WARD))
            priests.push_back(e.guid_low);
    }
    auto& prev_fw = last_fear_ward_[bkey];
    std::unordered_map<uint64, uint64> now_fw;
    for (size_t i = 0; i < priests.size() && i < tanks.size(); ++i)
    {
        uint64 const plow = priests[i], tlow = tanks[i];
        now_fw[plow] = tlow;
        auto pit = prev_fw.find(plow);
        if (pit == prev_fw.end() || pit->second != tlow)
            TC_LOG_INFO("playerbot.v2", "[raid_fear] group={} map={} {} keeps Fear Ward on {} ({} tank)",
                group_low, map_id, name_of(plow), name_of(tlow), tlow == main_tank ? "main" : "off");
        ObjectGuid const tguid = ObjectGuid::Create<HighGuid::Player>(tlow);
        Player const* t = ObjectAccessor::FindConnectedPlayer(tguid);
        if (!t || !t->IsInWorld() || !t->IsAlive() || t->GetMapId() != map_id || t->HasAura(FEAR_WARD))
            continue;
        if (auto it = plan.find(plow); it != plan.end())
            it->second.fear_ward_target = tguid;
    }
    prev_fw = std::move(now_fw);

    // -- Fearing boss engaged: Tremor Totem goes down proactively
    // (Classic::PickTotem, first shaman of each party); log the fear prep once.
    if (NearbyUnit const* fb = EngagedBoss(map_id, snaps, true))
    {
        ObjectGuid& prev_boss = last_fear_boss_[bkey];
        if (prev_boss != fb->guid)
        {
            prev_boss = fb->guid;
            std::string tremor;
            std::vector<uint8> seen;
            for (auto const& e : mem)
            {
                if (!e.is_bot || !e.alive || e.cls != CLASS_SHAMAN) continue;
                if (std::find(seen.begin(), seen.end(), e.src->subgroup) != seen.end()) continue;
                auto it = snap_of.find(e.guid_low);
                if (it == snap_of.end() || !BotSnapshotView(*it->second).knows_spell(TREMOR_TOTEM)) continue;
                seen.push_back(e.src->subgroup);
                if (!tremor.empty()) tremor += ", ";
                tremor += e.src->name + " (group " + std::to_string(e.src->subgroup + 1) + ")";
            }
            TC_LOG_INFO("playerbot.v2",
                "[raid_fear] group={} map={} {} ({}) engaged: Tremor Totem from {}; Fear Ward priests {}; "
                "warrior tanks stance-dance Berserker Rage on its fear casts",
                group_low, map_id, MobName(fb->entry), fb->guid.GetCounter(),
                tremor.empty() ? std::string("nobody") : tremor, uint32(prev_fw.size()));
        }
    }
    else
        last_fear_boss_.erase(bkey);

    // -- Resistance potions (opt-in): a boss of a resistance instance engaged.
    uint32 potion = 0;
    NearbyUnit const* boss = nullptr;
    if (sConfigMgr->GetBoolDefault("Playerbot.Raid.ResistPotions", false, true))
        if (uint32 const item = Classic::ResistPotionItem(Classic::InstanceResistance(map_id)))
            if ((boss = EngagedBoss(map_id, snaps, false)) != nullptr)
                potion = item;
    uint32& prev_potion = last_resist_potion_[bkey];
    if (potion != prev_potion)
    {
        if (potion)
            TC_LOG_INFO("playerbot.v2", "[raid_resist] group={} map={} {} ({}) engaged: bots drink item {} if they carry it",
                group_low, map_id, MobName(boss->entry), boss->guid.GetCounter(), potion);
        prev_potion = potion;
    }
    if (potion)
        for (auto& [low, o] : plan)
            o.resist_potion = potion;
    // Boss data (RaidRules::resistPotion): the potion a role drinks when this boss is engaged, whatever the global switch says. Ragnaros'
    // Elemental Fire killed five tanks in three minutes (MC 2026-10-12). The global order, when on, wins for the bots it covers.
    if (Raid::RaidRules const* rules = Raid::RulesForMap(map_id))
        if (!rules->resistPotion.empty())
            if (NearbyUnit const* eng = EngagedBoss(map_id, snaps, false))
                for (Raid::ResistPotion const& rp : rules->resistPotion)
                    if (rp.enemy == eng->entry)
                        for (auto& [low, o] : plan)
                        {
                            auto const mit = by_low.find(low);
                            if (o.resist_potion || mit == by_low.end())
                                continue;
                            if (rp.who == Raid::Who::Anyone || (rp.who == Raid::Who::Tank && mit->second->tank))
                                o.resist_potion = rp.item;
                        }
}

std::string PveGroupCoordinator::DebugDump() const
{
    return last_dump_.empty() ? std::string("coordinator has not planned yet")
                              : last_dump_;
}

void PveGroupCoordinator::PlanInnervates(uint64 bkey, uint64 group_low, uint32 map_id,
                                         std::vector<Member> const& mem,
                                         std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                                         std::unordered_map<uint64, PveOrder>& plan)
{
    constexpr uint32 INNERVATE          = 29166;
    constexpr int32  kInnervateManaPct  = 30;
    constexpr float  kInnervateRange    = 30.0f;
    constexpr uint16 SPEC_DRUID_BALANCE = 102;
    constexpr uint16 SPEC_DRUID_RESTO   = 105;

    auto& last = last_innervate_[bkey];
    if (!IsVanillaRealm())
    {
        last.clear();
        return;
    }
    std::unordered_map<uint64, BotSnapshot const*> snap_of;
    bool boss_fight = false;
    for (auto const& sp : snaps)
    {
        snap_of[sp->guid.GetCounter()] = sp.get();
        // Any fight in the raid, not only a boss: Molten Core's trash packs
        // (Giants, hounds, Firelord groups) run healers dry for minutes, and
        // the 6 min cooldown is back before the next boss otherwise unused.
        if (!boss_fight && sp->position.map_id == map_id && BotSnapshotView(*sp).in_combat())
            boss_fight = true;
    }

    std::unordered_map<uint64, uint64> now;
    if (boss_fight)
    {
        // Druids free to Innervate: bot Balance / Restoration druids (the
        // rotations that cast it) in combat, not tanking, Innervate ready.
        std::vector<Member const*> druids;
        for (auto const& e : mem)
        {
            if (!e.is_bot || !e.alive || e.cls != CLASS_DRUID) continue;
            if (e.spec != 0 && e.spec != SPEC_DRUID_BALANCE && e.spec != SPEC_DRUID_RESTO) continue;
            auto po = plan.find(e.guid_low);
            auto sn = snap_of.find(e.guid_low);
            if (po == plan.end() || po->second.tank_duty != 0 || sn == snap_of.end()) continue;
            BotSnapshotView const v(*sn->second);
            // Not in Cat / Bear / Dire Bear Form: Innervate is a caster spell
            // (Moonkin keeps it), and the rotations never leave a form in combat.
            if (v.has_aura(768) || v.has_aura(5487) || v.has_aura(9634)) continue;
            if (v.in_combat() && v.knows_spell(INNERVATE) && v.cd_remaining(INNERVATE).count() <= 0)
                druids.push_back(&e);
        }
        // Healers running dry without an Innervate on them, lowest mana first.
        std::vector<std::pair<int32, Member const*>> needy;
        for (auto const& e : mem)
        {
            if (!e.healer || !e.alive || e.src->max_mana <= 0) continue;
            int32 const pct = int32(int64(e.src->mana) * 100 / e.src->max_mana);
            if (pct >= kInnervateManaPct) continue;
            Player const* p = ObjectAccessor::FindConnectedPlayer(e.src->guid);
            if (!p || !p->IsInWorld() || p->GetMapId() != map_id || p->HasAura(INNERVATE)) continue;
            needy.emplace_back(pct, &e);
        }
        std::sort(needy.begin(), needy.end(), [](auto const& a, auto const& b)
        {
            return a.first != b.first ? a.first < b.first : a.second->guid_low < b.second->guid_low;
        });
        // Each healer gets the nearest free druid in range (a dry druid
        // healer is its own nearest), one druid per healer.
        for (auto const& [pct, h] : needy)
        {
            Member const* best = nullptr;
            float best_d2 = kInnervateRange * kInnervateRange;
            for (Member const* d : druids)
            {
                if (now.count(d->guid_low)) continue;
                float const dx = d->src->x - h->src->x, dy = d->src->y - h->src->y, dz = d->src->z - h->src->z;
                float const d2 = dx * dx + dy * dy + dz * dz;
                if (d2 <= best_d2)
                {
                    best_d2 = d2;
                    best = d;
                }
            }
            if (!best) continue;
            now[best->guid_low] = h->guid_low;
            plan[best->guid_low].innervate_target = h->src->guid;
            auto prev = last.find(best->guid_low);
            if (prev == last.end() || prev->second != h->guid_low)
                TC_LOG_INFO("playerbot.v2", "[raid_cd] group={} map={} {} -> Innervate on {} (mana {}%, {:.0f}y)",
                    group_low, map_id, best->src->name, h->src->name, pct, std::sqrt(best_d2));
        }
    }
    last = std::move(now);
}

// ---------- Line-of-sight pull ([sight_pull]) ----------
// smlibrary_loksey 8d2fd499aa: the tank threw at a Scarlet Tracking Hound and ran it back to the return point, and two mobs of its pack, a
// Scarlet Beastmaster and an Adept, stayed where they stood 37-47 y off and shot and cast at the priest. The tank walked out to taunt
// them (taunt reach 4 y), the fight moved to their spot, the Beastmaster fled at 15% to the next pack 28 y on, and twelve mobs killed the
// group. What a player does instead: break their line of sight, so they have to walk round the corner into the fight. Each plan:
//  1. Watch the mobs on the group: one that stands still (not rooted, stunned, feared or crowd-controlled, not unable to reach), more
//     than kStandOffYd from its victim with the victim in its sight, for kStandOffMs, stands off.
//  2. When a mob of the fight stands off within kStandOffNearYd of the anchor, search a spot near the anchor (rings kSpotRings around
//     it, its floor) out of every such mob's sight, with a margin to each side, outside every idle pack's reach, that the anchor has a
//     short path to. The anchor is the pull back's return point while the bot main tank runs one home or holds there; with no pull
//     back it is the tank itself, and only once every mob still on the group stands off (nothing left in reach to fight).
//  3. Publish it (PveOrder::sight_*) to every bot, and move a running pull back's return point there. The bots gather at it and do not
//     walk out to a mob on the list (State_InCombat SightPullTick, the loose-mob pickup).
//  4. A mob leaves the list once it is within kArrivedYd of the spot, dies or drops combat; the pull ends when the list is empty,
//     after kSightPullCapMs, or when a mob stops again with the spot in its sight (one search again, then the end).
// Not for a group with a player in it (the player leads where the group fights) or during a boss encounter.
void PveGroupCoordinator::PlanSightPull(uint64 bkey, uint64 group_low, uint32 map_id, uint32 now_ms, bool in_combat, uint64 main_tank,
                                        std::vector<std::pair<uint64, Player*>> const& bots,
                                        std::unordered_map<uint64, BotAI*> const& ais,
                                        std::vector<std::shared_ptr<BotSnapshot const>> const& snaps,
                                        std::unordered_map<uint64, PveOrder>& plan)
{
    constexpr float  kStandOffYd      = 8.0f;     // farther than this from its victim
    constexpr uint32 kStandOffMs      = 2000;     // and still this long
    constexpr float  kStillYd         = 1.0f;     // "still": within this of where it was first seen
    constexpr float  kStandOffNearYd  = 50.0f;    // the mob is part of the fight at the return point
    constexpr float  kArrivedYd       = 10.0f;    // a mob this close to the spot has come in
    // b3ca0b4b67 overnight: a spot the mobs did not come to held the group for 20 s; 12 s is a caster's walk round a corner and back.
    constexpr uint32 kSightPullCapMs  = 12000;
    constexpr float  kSideYd          = 2.0f;     // the spot stays hidden this far to each side of it
    constexpr float  kFloorDz         = 4.0f;     // the spot is on the return point's floor
    constexpr float  kMaxPathYd       = 35.0f;
    constexpr int    kMaxPathProbes   = 6;
    static constexpr float kSpotRings[] = { 6.0f, 9.0f, 12.0f, 15.0f, 18.0f, 21.0f, 24.0f };
    constexpr int    kBearings        = 24;

    SightPull& sp = sight_pull_[bkey];
    auto end = [&](char const* why)
    {
        if (sp.active)
            TC_LOG_INFO("playerbot.v2", "[sight_pull] group={} map={} ends after {} ms: {}", group_low, map_id, now_ms - sp.since_ms, why);
        sp.active = false;
        sp.mobs.clear();
        sp.replans = 0;
    };

    Player* tank = nullptr;
    for (auto const& [low, p] : bots)
        if (low == main_tank) { tank = p; break; }
    auto ai_it = ais.find(main_tank);
    BotAI* tank_ai = ai_it == ais.end() ? nullptr : ai_it->second;
    if (!in_combat || !tank || !tank_ai || !tank->IsInWorld() || !tank->IsAlive() || tank->GetMapId() != map_id)
    {
        end(!in_combat ? "combat over" : "no bot main tank");
        sp.watch.clear();
        return;
    }
    Map* map = tank->GetMap();
    if (!map || !map->IsDungeon())
        return end("not in an instance");
    // A player in the group leads where it fights (the player, or a scenario's stand-in for one).
    if (Group const* grp = tank->GetGroup())
        for (GroupReference const& r : grp->GetMembers())
            if (Player const* m = r.GetSource(); m && Scenario::PlaysAsHuman(m))
                return end("a player is in the group");
    if (InstanceMap* im = map->ToInstanceMap())
        if (InstanceScript const* is = im->GetInstanceScript(); is && is->IsEncounterInProgress())
            return end("boss encounter");

    // 1. Who stands off. Every mob on a bot of the group or its pet.
    std::vector<Creature*> engaged;
    auto add_attackers = [&](Unit const* u)
    {
        if (!u) return;
        for (Unit* a : u->getAttackers())
            if (Creature* c = a ? a->ToCreature() : nullptr; c && std::find(engaged.begin(), engaged.end(), c) == engaged.end())
                engaged.push_back(c);
    };
    for (auto const& [low, p] : bots)
    {
        if (!p || !p->IsInWorld() || p->GetMapId() != map_id) continue;
        add_attackers(p);
        add_attackers(p->GetPet());
    }
    std::vector<Creature*> standing;
    std::unordered_map<uint64, SightPull::Watch> watch;
    for (Creature* c : engaged)
    {
        if (!c->IsAlive() || !c->IsInCombat() || c->IsDungeonBoss() || c->isWorldBoss()) continue;
        Unit* v = c->GetVictim();
        if (!v) continue;
        bool const still_able = !c->HasUnitState(UNIT_STATE_NOT_MOVE | UNIT_STATE_LOST_CONTROL | UNIT_STATE_EVADE) &&
                                !c->HasUnitMovementFlag(MOVEMENTFLAG_ROOT) && !c->CanNotReachTarget();
        if (!still_able || c->isMoving() || c->GetExactDist2d(v) <= kStandOffYd || !c->IsWithinLOSInMap(v))
            continue;
        uint64 const key = c->GetGUID().GetCounter();
        SightPull::Watch w;
        if (auto it = sp.watch.find(key); it != sp.watch.end() &&
            c->GetExactDist2d(it->second.x, it->second.y) <= kStillYd)
            w = it->second;
        else
            w = { now_ms, now_ms, c->GetPositionX(), c->GetPositionY() };
        w.seen_ms = now_ms;
        watch[key] = w;
        if (now_ms - w.since_ms >= kStandOffMs)
            standing.push_back(c);
    }
    sp.watch = std::move(watch);

    auto dist2 = [](float ax, float ay, float bx, float by) { return (ax - bx) * (ax - bx) + (ay - by) * (ay - by); };
    PhaseShift const& ps = tank->GetPhaseShift();
    float const eye = tank->GetCollisionHeight();
    auto sees = [&](Creature const* c, float x, float y, float z)
    {
        return map->isInLineOfSight(ps, x, y, z + eye, c->GetPositionX(), c->GetPositionY(), c->GetPositionZ() + eye,
                                    LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::M2);
    };
    // Hidden from c at (x, y, z) and kSideYd to each side across c's line to it: a spot on the very edge of a corner is no cover.
    auto hidden_from = [&](Creature const* c, float x, float y, float z)
    {
        float lx = x - c->GetPositionX(), ly = y - c->GetPositionY();
        float const ll = std::sqrt(lx * lx + ly * ly);
        if (ll < 0.5f) return false;
        lx /= ll; ly /= ll;
        return !sees(c, x, y, z) && !sees(c, x - ly * kSideYd, y + lx * kSideYd, z) && !sees(c, x + ly * kSideYd, y - lx * kSideYd, z);
    };
    std::shared_ptr<BotSnapshot const> tank_snap;
    for (auto const& sn : snaps)
        if (sn->guid.GetCounter() == main_tank) { tank_snap = sn; break; }

    // The search: rings round the return point, the nearest ring first; within a ring the bearings that do not bring the group
    // closer to the mobs first. A short complete path from the return point.
    auto search = [&](std::vector<Creature*> const& mobs, float ax, float ay, float az, float& ox, float& oy, float& oz, std::string& why) -> bool
    {
        struct Cand { float x, y, z, score; };
        std::vector<Cand> cands;
        uint32 n_floor = 0, n_seen = 0, n_pack = 0;
        for (float r : kSpotRings)
            for (int i = 0; i < kBearings; ++i)
            {
                float const a = float(i) * 2.0f * float(M_PI) / float(kBearings);
                float const x = ax + r * std::cos(a), y = ay + r * std::sin(a);
                float const z = map->GetHeight(ps, x, y, az + kFloorDz, true, kFloorDz * 2.0f);
                if (z <= INVALID_HEIGHT + 1.0f || std::fabs(z - az) > kFloorDz) { ++n_floor; continue; }
                bool hidden = true;
                float closer = 0.0f;   // how much nearer the mobs the spot is than the return point
                for (Creature const* c : mobs)
                {
                    if (!hidden_from(c, x, y, z)) { hidden = false; break; }
                    float const ds = std::sqrt(dist2(x, y, c->GetPositionX(), c->GetPositionY()));
                    float const da = std::sqrt(dist2(ax, ay, c->GetPositionX(), c->GetPositionY()));
                    closer = std::max(closer, da - ds);
                }
                if (!hidden) { ++n_seen; continue; }
                if (tank_snap && States::PointInUnpulledPackReach(BotSnapshotView(*tank_snap), x, y, z)) { ++n_pack; continue; }
                cands.push_back({ x, y, z, r + 2.0f * closer });
            }
        std::stable_sort(cands.begin(), cands.end(), [](Cand const& l, Cand const& rr) { return l.score < rr.score; });
        int probes = 0;
        uint32 n_path = 0;
        for (Cand const& c : cands)
        {
            if (probes++ >= kMaxPathProbes) break;
            PathGenerator path(tank);
            if (!BotMovement::SehSafeCalculatePathFrom(path, ax, ay, az, c.x, c.y, c.z) ||
                (path.GetPathType() & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE)) || path.GetPathLength() > kMaxPathYd)
            {
                ++n_path;
                continue;
            }
            G3D::Vector3 const& e = path.GetActualEndPosition();
            ox = e.x; oy = e.y; oz = e.z;
            return true;
        }
        why = fmt::format("{} points off the floor, {} in sight, {} in an idle pack's reach, {} without a short path ({} candidates)",
                          n_floor, n_seen, n_pack, n_path, uint32(cands.size()));
        return false;
    };
    auto names = [](std::vector<Creature*> const& mobs)
    {
        std::string out;
        for (Creature const* c : mobs)
            out += fmt::format("{}{} ({}) at ({:.1f},{:.1f})", out.empty() ? "" : ", ", c->GetEntry(), c->GetGUID().GetCounter(),
                               c->GetPositionX(), c->GetPositionY());
        return out;
    };

    BotAI::PullBackPlan const pb = tank_ai->pull_back_plan();
    if (sp.active)
    {
        // 4. Who is still out there.
        std::vector<ObjectGuid> left;
        bool seen_again = false;
        std::vector<Creature*> stopped;
        for (ObjectGuid const& g : sp.mobs)
        {
            Creature* c = ObjectAccessor::GetCreature(*tank, g);
            if (!c || !c->IsAlive() || !c->IsInCombat() || c->IsInEvadeMode()) continue;
            if (dist2(c->GetPositionX(), c->GetPositionY(), sp.x, sp.y) <= kArrivedYd * kArrivedYd) continue;
            left.push_back(g);
            if (std::find(standing.begin(), standing.end(), c) != standing.end())
            {
                stopped.push_back(c);
                if (sees(c, sp.x, sp.y, sp.z)) seen_again = true;
            }
        }
        // A mob that started standing off since (a caster that came round the corner and stopped short of it).
        for (Creature* c : standing)
            if (std::find(left.begin(), left.end(), c->GetGUID()) == left.end() &&
                dist2(c->GetPositionX(), c->GetPositionY(), sp.x, sp.y) > kArrivedYd * kArrivedYd && sees(c, sp.x, sp.y, sp.z))
            {
                left.push_back(c->GetGUID());
                stopped.push_back(c);
                seen_again = true;
            }
        if (left.empty())
            return end("every mob that stood off has come in");
        if (now_ms - sp.since_ms > kSightPullCapMs)
            return end(fmt::format("{} s cap, {} still out", kSightPullCapMs / 1000u, left.size()).c_str());
        if (seen_again)
        {
            float x = 0.f, y = 0.f, z = 0.f;
            std::string why;
            if (sp.replans >= 1 || !search(stopped, sp.x, sp.y, sp.z, x, y, z, why))
                return end(fmt::format("a mob stopped with the spot in its sight: {}{}", names(stopped),
                                       why.empty() ? std::string() : " (" + why + ")").c_str());
            ++sp.replans;
            TC_LOG_INFO("playerbot.v2", "[sight_pull] group={} map={} {} stopped with the spot ({:.1f},{:.1f}) in sight: new spot ({:.1f},{:.1f},{:.1f})",
                        group_low, map_id, names(stopped), sp.x, sp.y, x, y, z);
            sp.x = x; sp.y = y; sp.z = z;
            if (tank_ai->pull_back_active())
                tank_ai->edit_pull_back([&](BotAI::PullBackPlan& q) { q.rx = x; q.ry = y; q.rz = z; });
        }
        sp.mobs = std::move(left);
    }
    else
    {
        // 2. The start: a mob of the fight stands off while the tank runs a pull back home or holds at its return point; or, with no pull
        // back, while every mob still on the group stands off, so the tank has nothing in reach to fight and would walk out to them.
        // eea74327b3 smlibrary_loksey36 (all three runs): with the pack beaten, the tank walked out to a Scarlet Adept or Beastmaster
        // casting from 15-20 y, next to the next pack; that pack and the one after it joined, the priest ran dry and the warrior or the
        // rogue died. The tank-anchored start of 0035 moved the tank in the middle of its fight (taken out in 0036); this one starts
        // only when the fight in reach is over, so the walk to the spot replaces the walk out.
        bool const pulling_back = tank_ai->pull_back_active() && pb.active && pb.map == map_id;
        if (pulling_back && pb.phase == BotAI::PullBackPhase::Pull)
            return;
        if (!pulling_back)
        {
            if (standing.empty() || !tank->IsInCombat())
                return;
            // A mob that fights in reach or still walks in: the fight is on, nobody moves. One held by crowd control does not count.
            for (Creature* c : engaged)
                if (c->IsAlive() && c->IsInCombat() && c->GetVictim() && !c->HasUnitState(UNIT_STATE_LOST_CONTROL) &&
                    std::find(standing.begin(), standing.end(), c) == standing.end())
                    return;
        }
        float const ax = pulling_back ? pb.rx : tank->GetPositionX();
        float const ay = pulling_back ? pb.ry : tank->GetPositionY();
        float const az = pulling_back ? pb.rz : tank->GetPositionZ();
        char const* const from = pulling_back ? "the return point" : "the tank";
        std::vector<Creature*> mobs;
        for (Creature* c : standing)
            if (dist2(c->GetPositionX(), c->GetPositionY(), ax, ay) <= kStandOffNearYd * kStandOffNearYd)
                mobs.push_back(c);
        if (mobs.empty())
            return;
        if (mobs.size() > PveOrder::kMaxSightMobs)
            mobs.resize(PveOrder::kMaxSightMobs);
        float x = 0.f, y = 0.f, z = 0.f;
        std::string why;
        if (!search(mobs, ax, ay, az, x, y, z, why))
        {
            if (now_ms - sp.no_spot_log_ms >= 5000u)
            {
                sp.no_spot_log_ms = now_ms;
                TC_LOG_INFO("playerbot.v2",
                            "[sight_pull] group={} map={} {} stand off, no spot out of their sight within {:.0f} y of {} ({:.1f},{:.1f}): {}",
                            group_low, map_id, names(mobs), kSpotRings[std::size(kSpotRings) - 1], from, ax, ay, why);
            }
            return;
        }
        sp.active = true;
        sp.since_ms = now_ms ? now_ms : 1u;
        sp.replans = 0;
        sp.x = x; sp.y = y; sp.z = z;
        sp.ax = ax; sp.ay = ay; sp.az = az;
        sp.mobs.clear();
        for (Creature const* c : mobs)
            sp.mobs.push_back(c->GetGUID());
        if (pulling_back)
            tank_ai->edit_pull_back([&](BotAI::PullBackPlan& q) { q.rx = x; q.ry = y; q.rz = z; });
        TC_LOG_INFO("playerbot.v2", "[sight_pull] group={} map={} {} stand off and shoot from where they stood: the group gathers at ({:.1f},{:.1f},{:.1f}), "
                    "{:.0f} y from {} ({:.1f},{:.1f}), out of their sight",
                    group_low, map_id, names(mobs), x, y, z, std::sqrt(dist2(x, y, ax, ay)), from, ax, ay);
    }

    // 3. Publish.
    for (auto& [low, o] : plan)
    {
        o.sight_pull = true;
        o.sight_x = sp.x; o.sight_y = sp.y; o.sight_z = sp.z;
        o.sight_mobs = {};
        for (size_t i = 0; i < sp.mobs.size() && i < PveOrder::kMaxSightMobs; ++i)
            o.sight_mobs[i] = sp.mobs[i];
        // The DPS burn a mob at the spot meanwhile: a kill focus out there would send them to it (loksey: kill_focus=23, the Beastmaster).
        if (o.sight_pull_mob(o.kill_focus))
            o.kill_focus = ObjectGuid::Empty;
    }
}

} // namespace Playerbot
