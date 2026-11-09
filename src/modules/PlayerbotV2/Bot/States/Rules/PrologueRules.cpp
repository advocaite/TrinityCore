// PrologueRules - Refactor #3 pass 19. Migrates the state-machine
// prologue rules that used to live inline at the top of DispatchIdle:
//   - idle:on_transport_wait     (999) freeze while a ship/zeppelin is in motion
//   - idle:watchdog_escape       (995) rule-fire watchdog wedge-break
//   - combat opener              (993) APL tick when victim is set OOC
//   - idle:owner_following       (990) manual /follow
//   - idle:owner_holding         (985) manual /hold (fire returns false: lets
//                                       lower-pri rules still run)
//   - idle:owner_stay            (980) manual /stay
//   - idle:owner_engaging        (975) manual /engage
//   - idle:owner_disengage       (970) manual /disengage
//   - idle:owner_action          (965) manual one-shot action
// All register above the survival band (880-900) so they preempt
// hazards. Holding mode is the exception — it preserves the legacy
// "fall through" semantic via fire-returns-false.

#include <atomic>
#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Diagnostics/DamageParity.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/ThreatDiscipline.h"            // RaidLogDue (opener pull discipline)
#include "Bot/States/StateBase.h"        // ThreatHoldActive
#include "Bot/MarkGate.h"                   // Playerbot.Behavior.MarksNeedOrder
#include "Bot/ClassicUpkeep.h"            // Classic::IsBossUnit
#include "Bot/States/MaintainHelpers.h"   // States::GroupAnchorTank
#include "Bot/Dungeon/DungeonScript.h"   // DungeonAdvice (boss list)
#include "Services.h"                    // Services::Dungeons() — vanishing-boss re-engage
#include "Combat/ApRotation.h"
#include "Combat/ApRegistry.h"
#include "Combat/Apl/ApClassicHeal.h"   // ClassicHeal::EmitInstantHealWhileMoving (fallback run)
#include "Bot/ClassTables.h"           // IsVanillaRealm
#include "Log.h"   // [bg_orphan] escape diagnostics

#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace Playerbot {

namespace {

// ---------- idle:on_transport_wait ----------
// Freeze the bot while a transport is carrying it. Two cases:
//   * Ships/zeppelins (type-15): the snapshot's transport_stopped flag
//     is accurate (Transport::IsStopped) — gate on !transport_stopped.
//   * Elevators (type-11): the snapshot builder's dynamic_cast<Transport>
//     fails and transport_stopped is always-true, but BotAI tracks
//     vertical-Z stability as a proxy. Freeze while we're moving
//     vertically (Z hasn't settled for ≥500ms). Without this freeze
//     the bot could be issued move_to commands by lower-priority
//     rules while the platform is mid-ascent and walk off the edge.
bool OnTransportGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (!s.on_transport()) return false;
    if (!s.transport_stopped()) return true;                   // ship in motion
    if (ai.transport_z_stable_ms() < 500) return true;         // elevator rising/falling
    return false;
}

bool OnTransportFire(BotSnapshotView const&, BotAI& ai,
                     GroupSnapshotView const&,
                     BotIntentEmitter&, uint32)
{
    ai.set_last_rule_fired("idle:on_transport_wait");
    return true;
}

// ---------- idle:watchdog_escape ----------
bool WatchdogGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (s.in_battleground()) return false;
    // Charter founders + signers are teleported onto the petitioner plaza
    // and rely on the FSM to advance phase 0→5 in place. The 50y random
    // escape move would yank them off the plaza (out of nearby_friends
    // range of the petitioner), and if it hits NoPath the path_blocks
    // counter trips GlobalStuckRescue, sending the bot to homebind (which
    // can be a TWW zone with sparse navmesh). The 20-min charter-grace
    // covers the full FSM budget; skip the watchdog while it's armed.
    const uint32 wdog_now_ms = s.published_at_ms();
    if (ai.in_charter_grace(wdog_now_ms)) return false;
    char const* last = ai.last_rule_fired();
    return last && ai.rule_watchdog_suppressed(last, wdog_now_ms);
}

bool WatchdogFire(BotSnapshotView const& s, BotAI& ai,
                  GroupSnapshotView const&,
                  BotIntentEmitter& emit, uint32)
{
    const uint32 wdog_now_ms = s.published_at_ms();
    float bx, by, bz; s.position(bx, by, bz);
    const uint32 epoch = wdog_now_ms / 8000u;   // one heading per 8 s window, so the escape runs straight instead of re-aiming every 2 s
    const uint64 mixed = (uint64(s.bot_id()) * 0x9E3779B97F4A7C15ULL)
                       ^ (uint64(epoch) * 0xBF58476D1CE4E5B9ULL);
    const float angle = (float(mixed & 0xFFFFu) / 65536.0f) * 6.2831853f;
    constexpr float kEscapeStep = 50.0f;
    emit.move_to(bx + std::cos(angle) * kEscapeStep,
                 by + std::sin(angle) * kEscapeStep,
                 bz, /*run*/ true);
    ai.set_last_rule_fired("idle:watchdog_escape");
    return true;
}

// ---------- idle:bg_orphan_escape ----------
// A battleground ended, the core's exit teleport missed this bot, and the
// Battleground object is gone — the bot is stranded on a dead BG/arena map
// (live 2026-06-11: 3 bots brawling forever on post-match map 566).
// combat:bg_orphan_disengage drops the fight; this rule then performs the
// exit the core owed the bot: hearthstone when ready, else a direct port
// to the homebind (the same destination TeleportToBGEntryPoint degrades
// to on error). 15s retry throttle via the action-retry table so a
// pending hearth cast / teleport isn't re-emitted every tick.
bool BgOrphanEscapeGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    return s.is_bg_orphan() && s.is_alive() && !s.in_combat();
}

bool BgOrphanEscapeFire(BotSnapshotView const& s, BotAI& ai,
                        GroupSnapshotView const&,
                        BotIntentEmitter& emit, uint32)
{
    const uint32 now_ms = s.published_at_ms();
    if (ai.action_recently_tried(BotAI::ActionKind::BgOrphanEscape, 0, now_ms))
        return false;
    auto const& trv = s.raw().travel;
    if (trv.has_hearthstone && trv.hearthstone_cd_ms == 0)
        emit.hearth();
    else if (trv.homebind_map_id != 0 || trv.homebind_x != 0.f || trv.homebind_y != 0.f)
        emit.teleport_to(trv.homebind_map_id, trv.homebind_x, trv.homebind_y, trv.homebind_z);
    else
        return false;
    ai.note_action_retry(BotAI::ActionKind::BgOrphanEscape, 0, now_ms);
    TC_LOG_INFO("playerbot.v2",
        "[bg_orphan] bot={} escaping dead BG map {} (hearth_ready={})",
        s.bot_id(), s.map_id(),
        (trv.has_hearthstone && trv.hearthstone_cd_ms == 0) ? 1 : 0);
    ai.set_last_rule_fired("idle:bg_orphan_escape");
    return true;
}

// ---------- combat:opener ----------
bool OpenerGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32)
{
    // Genuinely in combat / no victim / casting: the opener isn't owning
    // OOC ticks, so the absolute clock resets (it only counts UNBROKEN
    // out-of-combat opener ownership).
    if (s.victim().IsEmpty() || s.is_casting() || s.in_combat())
    {
        ai.reset_opener_own_since();
        return false;
    }
    // A tank on the owner's Attack / Pull order, the mob not yet reached: idle:owner_engaging walks it in (own pathing, straight
    // line when the navmesh has none). The opener (993) held the tick with a cast the mob was out of range for, and the tank stood
    // still for seconds before the pull (Deadmines 2026-10-15, 7 s from the order to the first step). Not only in a dungeon: a Protection
    // warrior ordered to attack in the open world stood still the same way while the Fury warriors beside him charged in (KailON,
    // Barrens, 30 Oct 2026, four warriors, Palli stood still on all three orders).
    // Beyond the 40 y enemy scan the mob is not reached either: the opener found no victim there, dropped it as "lost" and the order
    // with it (release-0130 zg_marli: the tank ordered onto Mar'li from 44 y never took a step, and the raid stood 120 s).
    if (ai.effective_role(s) == Role::Tank && ai.is_manual_mode(s.published_at_ms()) &&
        ai.owner_command() == BotAI::OwnerCommand::Engage && ai.owner_target() == s.victim())
    {
        bool in_scan = false;
        for (NearbyUnit const& u : s.nearby_enemies())
            if (u.guid == s.victim())
            {
                in_scan = true;
                float px, py, pz;
                s.position(px, py, pz);
                float const dx = u.x - px, dy = u.y - py;
                if (dx * dx + dy * dy > (std::max(u.combat_reach, 4.0f) + 3.5f) * (std::max(u.combat_reach, 4.0f) + 3.5f))
                    return false;
                break;
            }
        if (!in_scan && s.raw().combat.victim_pos_known)
            return false;
    }
    // ABSOLUTE opener ceiling inside a dungeon (2026-07-21). The per-victim
    // give-up (opener_victim_since_ms, 25s) is DEFEATED when the server-side
    // victim selection flips between GUIDs every tick: set_opener_victim
    // re-stamps since=now each time, so `now - since` never reaches 25s and
    // the opener (prio 993) owns the tick forever, ABOVE idle:dungeon_dispatch
    // (720). Live-proven with [opener_own]: every line fresh=1 since=0ms — a
    // frozen tank still "fighting" (rotation names in RuleHist, InCombat=0,
    // no MoveTo). This ceiling tracks the FIRST tick the opener took for this
    // bot out of combat, independent of the victim, and yields to the dungeon
    // navigator once it caps — the navigator then walks the bot to a real
    // target or holds cleanly. Reset whenever the bot is genuinely in combat
    // (opener_own_since is cleared on the in_combat exclusion above via
    // note_opener_own below).
    if (ai.dungeon_active())
    {
        const uint32 now = s.published_at_ms();
        const uint32 own_since = ai.opener_own_since_ms();
        if (own_since == 0)
            ai.set_opener_own_since(now);
        else if (now - own_since > 12000u)   // 12s of unbroken OOC opener → yield
            return false;
    }
    // Dungeon cohesion: a follower far from the tank must not burn ticks opening
    // on a target across a chokepoint it can't reach (the Gap-1 lip). The opener
    // (prio 993) otherwise preempts the DungeonDispatch regroup-cross (prio 720)
    // forever, stranding the follower while the tank fights the boss room unhealed
    // (3/5 wipe, Deadmines 2026-06-26). Yield so the cross runs; open after rejoin.
    if (ai.dungeon_active() && ai.effective_role(s) != Role::Tank && g.exists())
        if (GroupMemberSummary const* tk = States::GroupAnchorTank(s, ai, g))
            if (tk->online && tk->is_alive && tk->guid != s.raw().guid &&
                tk->map_id == s.map_id())
            {
                float bx, by, bz; s.position(bx, by, bz);
                const float dx = tk->x - bx, dy = tk->y - by;
                const float td2 = dx * dx + dy * dy;
                if (td2 > 40.0f * 40.0f)
                    return false;
                // Stuck out of the tank's ACTIVE fight: the tank is already
                // trading blows and we are not (in_combat was excluded above).
                // Burning opener ticks cast-spamming from the 12-40y dead-band
                // only ends in the 25s give-up (which 5-min-shields the kill
                // target), AND the opener (prio 993) monopolizes the tick so the
                // DungeonDispatch (720) converge/regroup that would WALK us into
                // range never runs — the tank then SOLOS a shielded mob it can't
                // finish for 30+ minutes (the Helix-approach Envoker deadlock,
                // 2026-06-27). Yield so converge-to-fight closes the gap; the
                // opener resumes the instant we're <12y and the cast lands.
                if (tk->in_combat && !tk->victim.IsEmpty() &&
                    td2 > 12.0f * 12.0f)
                    return false;
            }
    return true;
}

bool OpenerFire(BotSnapshotView const& s, BotAI& ai,
                GroupSnapshotView const& g,
                BotIntentEmitter& emit, uint32)
{
    // ---- Opener stall handling (2026-06-13) ----
    // The opener used to run the APL unconditionally against an out-of-
    // combat victim. With nothing closing the gap and nothing giving up,
    // a victim the bot couldn't reach produced an infinite
    // CastSpell|OutOfRange retry loop (~1.6s cadence, observed running
    // for hours fleet-wide): the rotation can't know its rules' ranges
    // here, State_InCombat's reposition/disengage never runs (the mob
    // never aggroes, so the state stays Idle), and can_autoact blocks
    // every idle movement rule while a victim is set.
    const uint32 now_ms = s.published_at_ms();
    ObjectGuid const vg = s.victim();
    // Fresh open when the victim changed OR the opener hasn't evaluated
    // recently (bot was in combat / casting / victimless in between) —
    // otherwise a re-open on the same GUID later would inherit a stale
    // since-timestamp and give up instantly.
    const bool fresh_open =
        (ai.opener_victim() != vg || now_ms - ai.opener_last_seen_ms() > 5000);
    if (fresh_open)
        ai.set_opener_victim(vg, now_ms);
    ai.set_opener_last_seen(now_ms);

    // Name the opener when it owns a dungeon tank's tick out of combat. This
    // rule (prio 993) sits ABOVE idle:dungeon_dispatch (720) and, for a tank
    // (which is exempt from the yield below), can claim every tick on a stale
    // victim selection with no MoveTo — presenting as a frozen tank still
    // "fighting" (InCombat=false, rotation names in RuleHist). If `since`
    // visibly RE-STAMPS while frozen, the 25s give-up can never fire because
    // the selection is flipping GUIDs. Same "make it say why" play as
    // [move_lock]/[emit_refused]. Throttled.
    if (ai.dungeon_active())
    {
        static uint32 s_opener_dbg_ms = 0;
        if (now_ms - s_opener_dbg_ms > 1500u)
        {
            s_opener_dbg_ms = now_ms;
            TC_LOG_INFO("playerbot.v2",
                "[opener_own] bot={} role={} in_combat={} fresh={} "
                "since={}ms oor={}",
                s.bot_id(), int(ai.effective_role(s)), s.in_combat() ? 1 : 0,
                fresh_open ? 1 : 0, now_ms - ai.opener_victim_since_ms(),
                ai.cast_oor_count());
        }
    }

    NearbyUnit const* vu = nullptr;
    for (auto const& u : s.raw().combat.nearby_enemies)
        if (u.guid == vg) { vu = &u; break; }
    // Record boss-ness WHILE the victim is visible so the lost-victim branch below
    // can tell a vanishing boss from despawned trash (the entry is gone once it
    // leaves the snapshot). Most scripted dungeon bosses (incl. Admiral Ripsnarl)
    // are not flagged as bosses by the core, so fall back to the per-dungeon
    // advice.bosses entry list. Computed once per fresh victim (the advice copy is
    // non-trivial) and cached on the bot logic.
    if (vu && fresh_open)
    {
        bool is_boss = Classic::IsBossUnit(*vu);  // either boss flag (is_dungeon_boss is never set on this core)
        if (!is_boss && s.is_in_dungeon())
        {
            DungeonAdvice const dav = Services::Dungeons().GetAdvice(s);
            for (uint32_t be : dav.bosses)
                if (be == vu->entry) { is_boss = true; break; }
        }
        ai.set_opener_victim_is_boss(is_boss);
    }
    if (!vu)
    {
        // Victim left the 40y snapshot sweep (despawned, evaded, kited
        // away, died to someone else). Holding the selection only keeps
        // can_autoact false — drop it and shield briefly.
        emit.stop_attack();
        // VANISHING-BOSS window (2026-06-29). A boss with a stealth/fog phase
        // (Admiral Ripsnarl) drops out of the snapshot for a few seconds, then
        // reappears. The default 30s engage-blacklist then keeps the tank from
        // re-acquiring him on reappear — it idles, drifts ~100y off the deck, and
        // the group fragments (the chronic Ripsnarl stalemate: 0 deaths, boss
        // alive, fight never finishes). When the lost victim was a BOSS, shield
        // only briefly (3s) so the bot re-grabs him the instant he's back. Trash
        // (despawned/evaded) keeps the full 30s so the unreachable-mob re-pick
        // loop the blacklist was added to prevent stays fixed.
        const uint32 reengage_ms = ai.opener_victim_is_boss() ? 3000u : 30000u;
        ai.note_engage(vg, now_ms, reengage_ms);
        ai.set_last_rule_fired("combat:opener_lost_victim");
        return true;
    }

    // Raid pull discipline (MC 2026-10-06): out of combat, a non-tank never
    // opens the fight. Its opener on a mob nobody fights yet (no victim, not
    // in combat - a mob hitting a member or a pet is in combat) IS the pull,
    // and the pull is the tank's, through idle:tank_pull_next and its gate
    // (group ready, buff hold). Live: healers and casters picked up an idle
    // Molten Giant (a leftover skull from the last wipe made it the raid's
    // kill call) and opened on it while the main tank held the pull "raid
    // not ready"; the Giants came loose onto the casters. Whatever handed
    // this bot the selection, the opener drops it here. Allowed: the owner's
    // own call to this bot (/focus, an Engage command), a human leader's
    // kill mark (the threat hold gives the tank 4s first), and a mob a tank
    // member is already opening on (its pull is in flight).
    // The same holds for EVERY role, tanks included, while the autopilot is suspended (the owner's
    // "follow" order, or a companion's small party in a raid map): there is no bot pull to wait
    // for, so the opener only fires on what the owner calls or is already fighting.
    const bool autopilot_suspended = ai.dungeon_autopilot_suspended() && s.is_in_dungeon();   // 5-man or raid map
    // Playerbot.Behavior.DungeonFlow: the same pull discipline in a 5-man while a living tank of the group stands on this map
    // ([dungeon_flow] pull held). Without a tank (or with the key off) a non-tank may still open: someone has to.
    bool five_man_tank_pull = false;
    if (!s.is_in_raid() && s.is_in_dungeon() && !s.in_battleground() && !autopilot_suspended &&
        ai.effective_role(s) != Role::Tank && g.exists() && Services::Config().behavior_dungeon_flow())
        if (GroupMemberSummary const* gt = g.tank())
            five_man_tank_pull = gt->is_alive && gt->map_id == s.map_id() && gt->guid != s.guid();
    if ((s.is_in_raid() || autopilot_suspended || five_man_tank_pull) && !s.in_battleground() &&
        (ai.effective_role(s) != Role::Tank || autopilot_suspended) &&
        !vu->unit_in_combat && vu->victim.IsEmpty())
    {
        bool allowed = ai.focus_target() == vg ||
            (ai.is_manual_mode(now_ms) && ai.owner_command() == BotAI::OwnerCommand::Engage &&
             ai.owner_target() == vg) ||
            (g.exists() && g.explicit_kill_mark() == vg && !g.leader_is_bot() &&
             MarkGate::MayEngage(s, ai, &g, vg, vu));   // MarksNeedOrder: a mark alone is no pull call
        if (!allowed)
            if (auto const* mems = g.members())
                for (GroupMemberSummary const& m : *mems)
                    if (m.guid != s.guid() && m.online && m.is_alive && m.victim == vg &&
                        (m.guid == s.raw().owner_guid || (!autopilot_suspended && m.role == Role::Tank) ||
                         (five_man_tank_pull && !m.is_bot)))   // a human's pull is the call to fight
                    { allowed = true; break; }
        if (!allowed)
        {
            emit.stop_attack();
            ai.note_engage(vg, now_ms, 10000u);
            if (RaidLogDue(RaidLogKey(s, RaidLogKind::OpenerHold), now_ms, 5000u))
                TC_LOG_INFO("playerbot.v2",
                    "[raid_pull] {} (role {}) drops its opener on unpulled entry {} ({}): {} "
                    "(last rule: {})",
                    s.name(), int(ai.effective_role(s)), vu->entry, vg.GetCounter(),
                    autopilot_suspended ? "the owner leads (autopilot suspended)"
                        : (five_man_tank_pull ? "[dungeon_flow] 5-man: the pull is the tank's" : "the pull is the tank's"),
                    ai.last_rule_fired() ? ai.last_rule_fired() : "?");
            ai.set_last_rule_fired("combat:opener_pull_held");
            return true;
        }
    }

    // Stalled open: still not in combat after this long means neither
    // casts nor approach steps worked (wall, elevation, unpathable mob).
    // Give up and shield the target for minutes so engage_nearby_mob /
    // assist rules don't re-pick it the moment an 8s shield lapses.
    constexpr uint32 kOpenerStallMs = 25000;
    if (now_ms - ai.opener_victim_since_ms() > kOpenerStallMs && !Diagnostics::Parity::IsDummy(vg))   // the measuring dummy is shielded by no one
    {
        emit.stop_attack();
        // A mob the group is fighting is shielded briefly, so the bot comes back to the fight from wherever it gets to; an unreachable
        // mob nobody fights keeps the long shield (the re-pick loop it exists for).
        ThreatVictim const vk = vu ? ThreatVictim(vu->victim_kind) : ThreatVictim::None;
        bool const group_fight = vu && vu->unit_in_combat &&
            (vk == ThreatVictim::Tank || vk == ThreatVictim::Member || vk == ThreatVictim::MemberPet);
        ai.note_engage(vg, now_ms, group_fight ? 15000u : 5u * 60u * 1000u);
        ai.set_last_rule_fired("combat:opener_give_up");
        TC_LOG_INFO("playerbot.v2",
            "[opener_give_up] bot={} victim={} unreachable for {}ms — shielded 5min",
            s.bot_id(), vg.ToString(), now_ms - ai.opener_victim_since_ms());
        return true;
    }

    // Server-verdict approach: the executor counts CastSpell results of
    // OutOfRange/LoS against our selection. Any since the last reset
    // means "can't reach from here" — step toward the victim (mirrors
    // combat:reposition_range), then reset so the APL gets a fresh try
    // from the new position on the next retry window.
    if (ai.cast_oor_count() > 0)
    {
        float bx, by, bz;
        s.position(bx, by, bz);
        const float dx = vu->x - bx, dy = vu->y - by, dz = vu->z - bz;
        const float dsq = dx * dx + dy * dy + dz * dz;
        if (dsq > 4.0f * 4.0f)
        {
            const float dist = std::sqrt(dsq);
            const float step = std::min(10.0f, dist);
            // A caster or healer is not walked into an idle pack's reach for a line of sight or a range it can find from here: the
            // owner's Attack on a mob up a ramp took two casters up it, and the Dragonspawn beside the target came with them (UBRS
            // 132f 22:15:16-22:15:26: LINE_OF_SIGHT on the first casts, then 10 y approach steps toward the unpulled Veteran, a rogue
            // dead 4 s after the pack arrived). The pack of the target itself counts. The rotation keeps trying from where it stands.
            // A healer whose casts failed out of range / sight in the last few seconds while a member in combat is under half health
            // takes the step anyway: the heal outweighs the idle pack (sfk_rethilgore 0.1.26 RC8: the priest at 100 % mana, 30 heals
            // failed on line of sight, refused every step because a Whitescalp stood idle 7-10 y from the tank, and the tank died).
            bool healer_los_emergency = false;
            if (IsHealerSpec(s.cls(), uint16(s.spec())) && ai.last_cast_oor_ms() != 0 && now_ms - ai.last_cast_oor_ms() <= 3000u)
                if (auto const* mems = g.exists() ? g.members() : nullptr)
                    for (GroupMemberSummary const& m : *mems)
                        if (m.online && m.is_alive && m.in_combat && m.map_id == s.map_id() && m.max_hp > 0 && m.hp * 2 < m.max_hp)
                        { healer_los_emergency = true; break; }
            if (ai.effective_role(s) != Role::Tank && (IsRangedSpec(s.cls(), uint16(s.spec())) || IsHealerSpec(s.cls(), uint16(s.spec()))) &&
                !healer_los_emergency &&
                States::RaidPointInUnpulledReach(s, bx + dx / dist * step, by + dy / dist * step, bz, /*exempt_victim_pack*/ false))
            {
                // Not straight in, but to a spot that sees the target and stays out of the idle packs' reach: standing still
                // here held a caster out of the group's fight until the 25 s give-up (playtest 2026-10-27, Zul'Farrak: the mage
                // and the other casters watched while only the player and the tank hit the mobs).
                ai.reset_cast_oor();
                float qx, qy, qz, qr;
                if (vu && States::SightSpotMove(s, *vu, /*melee*/ false, /*pack_safe*/ true, emit, qx, qy, qz, qr))
                {
                    ai.set_last_rule_fired("combat:opener_sight_spot");
                    return true;
                }
            }
            else
            {
                emit.move_to(bx + dx / dist * step,
                             by + dy / dist * step,
                             bz, /*run*/ true);
                ai.reset_cast_oor();
                ai.set_last_rule_fired("combat:opener_approach");
                return true;
            }
        }
        // Already adjacent yet OutOfRange — usually elevation/LoS; let
        // the APL retry and the stall timer above bound the loop.
        ai.reset_cast_oor();
    }

    ApRotation const* rot = Combat::GetRotation(s.cls(), s.spec());
    if (!rot) return false;
    ApPredicateContext ctx{s, g, ai.aoe_preference()};
    char const* rule_name = nullptr;
    if (!rot->tick(ctx, emit, &rule_name)) return false;
    ai.set_last_rule_fired(rule_name ? rule_name : "combat:opener");
    return true;
}

// ---------- Manual-mode helpers ----------
bool ManualModeActive(BotSnapshotView const& s, BotAI& ai)
{
    return ai.is_manual_mode(s.published_at_ms());
}

// idle:follow_pull_threat
// Bot following a group leader (legacy party follow OR manual-mode
// owner-follow) plows through mob aggro because TC's
// FollowMovementGenerator pathfinds straight-line — same death mode
// as the solo quest-walk one fixed by path_threat. When bot is
// "catching up" to leader (>15y away on same map) and a hostile sits
// in the corridor, pull it first. Bot's start_attack chases the
// threat, preempting the follow generator; once combat ends the
// follow resumes.
bool FollowPullThreatGate(BotSnapshotView const& s, BotAI& ai,
                          GroupSnapshotView const& g, uint32)
{
    if (s.in_combat() || s.is_casting() || !s.is_alive()) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.in_battleground() || s.is_in_dungeon()) return false;
    // Passive = defend only; Stay / Hold = keep the spot: neither starts a fight with a mob in the corridor.
    if (ai.passive_now(false) || ai.holding_position(s.published_at_ms())) return false;
    if (!g.exists()) return false;
    ObjectGuid const leader_guid = g.leader();
    if (leader_guid.IsEmpty() || leader_guid == s.guid()) return false;
    auto const* members = g.members();
    if (!members) return false;
    GroupMemberSummary const* leader = nullptr;
    for (auto const& m : *members)
        if (m.guid == leader_guid) { leader = &m; break; }
    if (!leader) return false;
    if (!leader->online) return false;
    if (leader->map_id != s.map_id()) return false;
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = leader->x - bx, dy = leader->y - by;
    const float dsq = dx*dx + dy*dy;
    // "Catching up" band: >15y means we're not in formation, <120y
    // means leader is still within reach of a corridor scan. Outside
    // these we either don't need to pull (in formation) or can't
    // path_threat usefully (leader out of nearby_enemies scan radius).
    constexpr float kMin = 15.0f, kMax = 120.0f;
    if (dsq < kMin * kMin || dsq > kMax * kMax) return false;
    return s.path_threat(leader->x, leader->y,
                          /*max_forward*/ 35.0f,
                          /*half_width*/  10.0f) != nullptr;
}

bool FollowPullThreatFire(BotSnapshotView const& s, BotAI& ai,
                          GroupSnapshotView const& g,
                          BotIntentEmitter& emit, uint32)
{
    ObjectGuid const leader_guid = g.leader();
    auto const* members = g.members();
    GroupMemberSummary const* leader = nullptr;
    if (members)
        for (auto const& m : *members)
            if (m.guid == leader_guid) { leader = &m; break; }
    if (!leader) return false;
    NearbyUnit const* threat = s.path_threat(leader->x, leader->y,
                                              /*max_forward*/ 35.0f,
                                              /*half_width*/  10.0f);
    if (!threat) return false;
    if (!emit.start_attack(threat->guid)) return false;
    ai.set_last_rule_fired("idle:follow_pull_threat");
    return true;
}

// idle:owner_following
bool OwnerFollowGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    // In a battleground the bot must PLAY the BG, not follow its owner. This
    // rule is priority 990 — above idle:bg_dispatch (718) — so without this
    // gate a selfbot left in manual Follow preempts the BG layer every tick
    // and, because State_InGroup's follow only yields once bg_active is set,
    // chases the owner/anchor toward an unreachable point (the closed prep
    // gate, or the owner who isn't in this instance) — observed as the bot
    // running through the walls into the void at the map edge. The owner can
    // still direct it mid-BG via engage/action; passive Follow yields to
    // objective play. Mirrors the BG exclusion on idle:follow_pull_threat.
    if (s.in_battleground()) return false;
    return ManualModeActive(s, ai) && ai.owner_command() == BotAI::OwnerCommand::Follow;
}
// A companion in its owner's group keeps up its upkeep while following or staying: consuming the tick here starved
// rez, heals, buffs, food and drink (all below 700) for as long as the order lasted, up to 30 minutes for Stay. The
// world-bot errands below are already skipped for such a bot (IdleRuleRegistry::tick) and the group layer keeps it
// on its follow slot, so the tick only falls through to upkeep.
bool OwnerFollowFire(BotSnapshotView const& s, BotAI& ai,
                     GroupSnapshotView const&, BotIntentEmitter&, uint32)
{
    ai.set_last_rule_fired("idle:owner_following");
    return !s.is_companion_with_owner();
}

// idle:owner_holding — fire returns FALSE so subsequent rules still get
// to run (mirrors legacy `break;`-out-of-switch behavior). The diagnostic
// tag is still set so /history reflects the manual mode.
bool OwnerHoldGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ManualModeActive(s, ai) && ai.owner_command() == BotAI::OwnerCommand::Hold;
}
bool OwnerHoldFire(BotSnapshotView const&, BotAI& ai,
                   GroupSnapshotView const&, BotIntentEmitter&, uint32)
{
    ai.set_last_rule_fired("idle:owner_holding");
    return false;   // intentional: let lower-priority rules continue
}

// idle:owner_stay
// A use order's Stay outlives the use by this much at most when the bot casts nothing (a ritual's channel starts at once).
static constexpr uint32 kUseStayDoneMs = 5000u;
bool OwnerStayGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ManualModeActive(s, ai) && ai.owner_command() == BotAI::OwnerCommand::Stay;
}
bool OwnerStayFire(BotSnapshotView const& s, BotAI& ai,
                   GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    ai.set_last_rule_fired("idle:owner_stay");
    // The Stay of a "use <object>" holds the bot only while the use itself needs it: a ritual's channel breaks when the user moves. Once
    // the use is done (no cast or channel running 5 s after it) the order is spent and the bot follows and fights again; it is a one-off,
    // not a Stay (UBRS 2026-10-28 22:16:06, "use altar" to all nine bots: two still under idle:owner_stay at 22:16:18 with the group gone).
    if (ai.use_stay() && !s.is_casting() && s.published_at_ms() - ai.last_owner_command_ms() >= kUseStayDoneMs)
    {
        ai.clear_use_stay();
        ai.exit_manual();
        TC_LOG_INFO("playerbot.v2", "[stay_end] {} leaves the object-use Stay: the use is done", s.name());
        return false;
    }
    // Back to the spot of a Stay the group's fight lifted (GroupFight.h): the bot fought where the fight was, now it goes back.
    {
        float rx, ry, rz;
        uint32 rmap;
        if (ai.stay_return(rx, ry, rz, rmap) && s.is_alive() && !s.in_combat())
        {
            float bx, by, bz;
            s.position(bx, by, bz);
            float const d = std::hypot(rx - bx, ry - by);
            if (rmap != s.map_id() || d > 80.f)
                ai.clear_stay_return();
            else if (d <= 2.5f)
            {
                // A Positions tab spot stays: the bot comes back to it after every fight, not only the first.
                if (!ai.stay_return_pinned())
                {
                    ai.clear_stay_return();
                    TC_LOG_INFO("playerbot.v2", "[stay_return] {} is back on its Stay spot", s.name());
                }
            }
            else if (!s.is_moving() && (emit.move_already_emitted(rx, ry, rz) || emit.move_to(rx, ry, rz, /*run*/ true)))
            {
                ai.set_last_rule_fired("idle:owner_stay_return");
                return true;
            }
            else if (s.is_moving())
                return true;
        }
    }
    // A Stay that holds a bot while its group fights is how a damage dealer stands out of a boss fight: say so (once per 5 s per bot).
    if (s.is_alive() && !s.in_combat())
    {
        if (auto const* members = g.members())
            for (auto const& m : *members)
                if (m.in_combat && m.guid != s.guid())
                {
                    static std::mutex sStayLogMutex;
                    static std::unordered_map<uint64, uint32> sStayLog;
                    uint32 const now = s.published_at_ms();
                    bool log_now = false;
                    {
                        std::lock_guard<std::mutex> lock(sStayLogMutex);
                        if (sStayLog.size() > 4096) sStayLog.clear();
                        uint32& last = sStayLog[s.guid().GetCounter()];
                        if (!last || now - last > 5000u) { last = now; log_now = true; }
                    }
                    if (ai.use_stay())
                    {
                        // The Stay of an object use ends when the group's fight starts, not only when the bot's own does: Halorio (hunter) stood
                        // at the altar for 16 s of the Emberseer fight, idle under it, because the boss never turned to him (UBRS 132g 23:38).
                        ai.clear_use_stay();
                        ai.exit_manual();
                        TC_LOG_INFO("playerbot.v2", "[stay_end] {} leaves the object-use Stay: {} is in combat", s.name(), m.name);
                        return false;   // the lower rules (engage, assist) run on this tick
                    }
                    if (log_now)
                        TC_LOG_INFO("playerbot.v2", "[stay_hold] {} idle under an owner Stay while {} is in combat", s.name(), m.name);
                    break;
                }
    }
    return !s.is_companion_with_owner();   // see OwnerFollowFire; loot runs wait (LootDrainGate)
}

// idle:owner_engaging
bool OwnerEngageGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ManualModeActive(s, ai) && ai.owner_command() == BotAI::OwnerCommand::Engage;
}
bool OwnerEngageFire(BotSnapshotView const& s, BotAI& ai,
                     GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    // A tank ordered onto a mob (raid boss or dungeon pull) walks up to it and attacks. The rule used to do nothing but hold the tick, which suits a bot that is
    // already in the fight; a tank ordered onto a stationary boss (Ragnaros has no combat movement) from 37 y away stood there for the
    // whole Engage window while the boss killed the casters with Magma Blast (MC Ragnaros 2026-10-13 eb79ff8d).
    ObjectGuid const owner_target = ai.owner_target();
    bool target_in_sight = false;
    for (NearbyUnit const& u : s.nearby_enemies())
        if (u.guid == owner_target && u.hp > 0)
        {
            target_in_sight = true;
            break;
        }
    // The order's own victim is in sight whatever the 40 y enemy scan holds: the ordered mob stood 40-46 y off at Red Cloud Mesa, the order was dropped
    // on the first tick as "mob gone" and the group went back to its formation slots instead of walking up (PC 20 Oct 21:06).
    if (!target_in_sight && !owner_target.IsEmpty() && s.victim() == owner_target)
        target_in_sight = true;
    // Dungeon or open world: the walk was dungeon-only, and an open-world tank on the owner's Attack order held the tick without a step.
    if (s.is_alive() && !s.in_combat() && !s.is_casting() && ai.effective_role(s) == Role::Tank)
    {
        ObjectGuid const target = owner_target;
        for (NearbyUnit const& u : s.nearby_enemies())
        {
            if (u.guid != target || u.hp <= 0)
                continue;
            float px, py, pz;
            s.position(px, py, pz);
            float const dx = u.x - px, dy = u.y - py;
            float const dist = std::sqrt(dx * dx + dy * dy);
            float const stand = std::max(u.combat_reach, 4.0f) + 2.0f;   // inside melee reach of a big boss, not inside the boss
            if (dist > stand + 1.5f && dist > 0.1f)
            {
                float const gx = u.x - dx / dist * stand, gy = u.y - dy / dist * stand;
                {
                    // Every 2 s (one line at a time across bots): where the tank is and what its walk to the ordered mob looks like.
                    static std::atomic<uint32> s_walk_log_ms{0};
                    uint32 const now_log = s.published_at_ms();
                    uint32 last_log = s_walk_log_ms.load(std::memory_order_relaxed);
                    if (now_log - last_log >= 2000u && s_walk_log_ms.compare_exchange_strong(last_log, now_log))
                        TC_LOG_INFO("playerbot.v2", "[engage_walk] {} at ({:.1f},{:.1f},{:.1f}) to mob {} at ({:.1f},{:.1f},{:.1f}) dist={:.1f} stand={:.1f} dst=({:.1f},{:.1f}) moving={} path_dest={} los={}",
                                    s.name(), px, py, pz, u.entry, u.x, u.y, u.z, dist, stand, gx, gy, s.is_moving() ? 1 : 0,
                                    s.has_path_destination() ? 1 : 0, u.in_los ? 1 : 0);
                }
                // The same walk emitted a moment ago is under way (the dedup answers false for it): not a refusal, so no direct
                // straight-line walk replaces the pathed one.
                if (emit.move_already_emitted(gx, gy, u.z) || emit.move_to(gx, gy, u.z, /*run*/ true))
                {
                    ai.set_last_rule_fired("idle:owner_engage_close");
                    return true;
                }
                // The navmesh gave no route (mine carts and props between the tank and the pull, Deadmines 2026-10-14: the tank said
                // "pulling" and stood still): a mob in sight and close is walked to in a straight line, bounded to 25 y.
                if (u.in_los && dist <= 25.0f &&
                    emit.move_to(u.x - dx / dist * stand, u.y - dy / dist * stand, u.z, /*run*/ true, /*direct*/ true))
                {
                    ai.set_last_rule_fired("idle:owner_engage_close_direct");
                    return true;
                }
            }
            else if (emit.start_attack(target))
            {
                ai.set_last_rule_fired("idle:owner_engaging");
                return true;
            }
            break;
        }
        // The ordered mob is the victim but stands beyond the 40 y enemy scan (a raid boss from its landing spot, 44 y): walk toward it
        // on the victim's own position until it comes into the scan, where the walk above takes over. The opener yields meanwhile.
        CombatTargetsState const& ct = s.raw().combat;
        bool scanned = false;
        for (NearbyUnit const& u : s.nearby_enemies())
            if (u.guid == target) { scanned = true; break; }
        if (!scanned && !target.IsEmpty() && s.victim() == target && ct.victim_pos_known)
        {
            float px, py, pz;
            s.position(px, py, pz);
            float const dx = ct.victim_x - px, dy = ct.victim_y - py;
            float const dist = std::sqrt(dx * dx + dy * dy);
            float const stand = std::max(ct.victim_reach, 4.0f) + 2.0f;
            if (dist > stand + 1.5f)
            {
                float const gx = ct.victim_x - dx / dist * stand, gy = ct.victim_y - dy / dist * stand;
                if (emit.move_already_emitted(gx, gy, ct.victim_z) || emit.move_to(gx, gy, ct.victim_z, /*run*/ true))
                {
                    ai.set_last_rule_fired("idle:owner_engage_close");
                    return true;
                }
            }
        }
    }
    // A non-tank that did not open on the Attack order (a party's casters wait for the tank, BotCommandParser "attack") opens once
    // the threat hold lets go: the tank has its lead, or the bounded wait is over.
    if (target_in_sight && s.is_alive() && !s.in_combat() && !s.is_casting() && s.victim().IsEmpty() &&
        ai.effective_role(s) != Role::Tank && !States::ThreatHoldActive(ai, s, owner_target, &g) &&
        emit.start_attack(owner_target))
    {
        ai.set_last_rule_fired("idle:owner_engaging");
        return true;
    }
    // The ordered mob is dead or gone and the bot is out of the fight: the Attack / Pull order is done. It ends here instead of
    // holding the tick for the rest of its 30 s window (looting, drinking and buffing below priority 975 were starved).
    if (!target_in_sight && !s.in_combat())
    {
        ai.exit_manual();
        return false;
    }
    ai.set_last_rule_fired("idle:owner_engaging");
    // A companion whose ordered mob is not near keeps its upkeep going (rez, heal, buffs, drink), as under follow and stay;
    // with the mob in sight the rule still holds the tick.
    return !s.is_companion_with_owner() || target_in_sight;
}

// idle:owner_disengage
bool OwnerDisengageGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ManualModeActive(s, ai) && ai.owner_command() == BotAI::OwnerCommand::Disengage;
}
bool OwnerDisengageFire(BotSnapshotView const&, BotAI& ai,
                        GroupSnapshotView const&, BotIntentEmitter&, uint32)
{
    ai.set_last_rule_fired("idle:owner_disengage");
    return true;
}

// idle:owner_action
bool OwnerActionGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ManualModeActive(s, ai) && ai.owner_command() == BotAI::OwnerCommand::Action;
}
bool OwnerActionFire(BotSnapshotView const&, BotAI& ai,
                     GroupSnapshotView const&, BotIntentEmitter&, uint32)
{
    ai.set_last_rule_fired("idle:owner_action");
    ai.exit_manual();
    return true;
}

// idle:fallback - the owner's fallback order (States::FallbackTick). 994: above the opener (993)
// and the owner rules, so the run and the re-formed hold own the tick.
bool FallbackGate(BotSnapshotView const&, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ai.fallback_active();
}
bool FallbackFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    return States::FallbackTick(s, ai, g, emit, /*in_combat_state*/ false);
}

// idle:pull_back - the owner's pull back order (States::PullBackTick), same slot as the fallback.
bool PullBackGate(BotSnapshotView const&, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ai.pull_back_active();
}
bool PullBackFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    return States::PullBackTick(s, ai, g, emit, /*in_combat_state*/ false);
}

// idle:owner_escort - an owned bot that isn't grouped stays with its owner
// while the owner is nearby, instead of running the autonomous questing /
// social layer. Without it a freshly summoned bot wandered off (and invited
// people) until the owner grouped it. Grouped bots follow via State_InGroup.
bool OwnerEscortGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    ObjectGuid const owner = s.raw().owner_guid;
    if (owner.IsEmpty()) return false;
    if (s.in_group() || s.in_battleground()) return false;
    if (ManualModeActive(s, ai)) return false;
    for (auto const& u : s.raw().combat.nearby_friends)
        if (u.guid == owner)
            return true;
    return false;
}
bool OwnerEscortFire(BotSnapshotView const& s, BotAI& ai,
                     GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    ObjectGuid const owner = s.raw().owner_guid;
    float bx, by, bz; s.position(bx, by, bz);
    for (auto const& u : s.raw().combat.nearby_friends)
    {
        if (u.guid != owner) continue;
        const float dx = u.x - bx, dy = u.y - by;
        constexpr float kSlack = 8.0f;
        if (dx * dx + dy * dy > kSlack * kSlack)
            emit.follow(owner, 3.0f);
        break;
    }
    ai.set_last_rule_fired("idle:owner_escort");
    return true;
}

} // anonymous namespace

namespace States {

// The owner's fallback (BotAI::FallbackPlan, set by BotCommandParser OrderFallback).
//  Run: no target, no attack, no casting except a healer's instants on the move, until the end
//   of the run (a tank's is 3y behind its slot, then one step forward so it faces the fight).
//   Combat does not stop it: DispatchInCombat runs this first.
//  Re-formed: fight only what followed. A mob counts when it is within 15y of the rally, or (not
//   for a tank) is hitting a member standing there and is within 30y (ranged reach it without
//   moving). Nobody chases past 20y from the rally: a victim beyond that is dropped and the bot
//   walks back to its slot.
bool FallbackTick(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g,
                  BotIntentEmitter& emit, bool in_combat_state)
{
    if (!ai.fallback_active() || !s.is_alive()) return false;
    BotAI::FallbackPlan const p = ai.fallback_plan();
    if (!p.active || p.map != s.map_id()) return false;
    const uint32 now_ms = s.published_at_ms();
    float bx, by, bz; s.position(bx, by, bz);
    auto d2xy = [](float ax, float ay, float cx, float cy) { return (ax - cx) * (ax - cx) + (ay - cy) * (ay - cy); };
    const bool healer = ai.effective_role(s) == Role::Healer;
    auto tag = [&](char const* idle_tag, char const* combat_tag)
    { ai.set_last_rule_fired(in_combat_state ? combat_tag : idle_tag); };
    // A raid avoid circle (Baron Geddon's Inferno, a Living Bomb carrier) outranks the order's walk. The bot standing in one is left to
    // the raid brain's step-out; a rally point or slot inside one is not walked into, the bot waits where it is (MC Geddon 2026-10-12:
    // Fall back sent bots back to a rally point inside Inferno, 14 of the deaths read combat:fallback_hold / fallback_return).
    PveOrder const& hazard_order = s.raw().pve_order;
    bool const hazards = Services::Config().behavior_avoid_circles() && hazard_order.active && hazard_order.avoid_circle_count > 0;
    if (hazards && hazard_order.in_avoid_circle(bx, by, 0.0f))
        return false;
    // Head for (x,y,z) unless a live path already ends there (re-issuing restarts the spline).
    auto head_to = [&](float x, float y, float z, bool run)
    {
        if (hazards && hazard_order.in_avoid_circle(x, y, 2.0f))
        {
            if (s.is_moving()) emit.stop_movement();
            return false;
        }
        if (s.is_moving() && s.has_path_destination())
        {
            float qx, qy, qz; s.path_destination(qx, qy, qz);
            if (d2xy(qx, qy, x, y) < 2.0f * 2.0f) return true;
        }
        if (emit.move_already_emitted(x, y, z)) return true;   // under way, not refused
        return emit.move_to(x, y, z, run);
    };

    if (!p.arrived)
    {
        if (d2xy(bx, by, p.px, p.py) > 2.5f * 2.5f)
        {
            if (!s.victim().IsEmpty())
                emit.stop_attack();
            if (!healer && s.is_casting())
                emit.cancel_cast();
            head_to(p.px, p.py, p.pz, /*run*/ true);
            if (healer && IsVanillaRealm())
                Combat::ClassicHeal::EmitInstantHealWhileMoving(s, g, emit);
            tag("idle:fallback_run", "combat:fallback_run");
            return true;
        }
        ai.mark_fallback_arrived();
        TC_LOG_INFO("playerbot.v2", "[fallback] {} arrived at rally ({:.1f},{:.1f}) after {}s",
            s.name(), p.rx, p.ry, (now_ms - p.started_ms) / 1000u);
        if (p.tank && d2xy(p.px, p.py, p.sx, p.sy) > 1.0f)
            emit.move_to(p.sx, p.sy, p.sz, /*run*/ false);   // step into the slot, facing the fight
        tag("idle:fallback_arrived", "combat:fallback_arrived");
        return true;
    }

    // ---- Re-formed at the rally ----
    constexpr float kRallyReach = 15.0f, kLeash = 20.0f, kRangedReach = 30.0f;
    auto member_at_rally = [&](ObjectGuid v) -> bool
    {
        if (v.IsEmpty()) return false;
        if (v == s.guid()) return d2xy(bx, by, p.rx, p.ry) <= kLeash * kLeash;
        if (v == s.pet_guid()) return true;
        if (auto const* mems = g.members())
            for (GroupMemberSummary const& m : *mems)
                if (m.guid == v)
                    return m.online && m.is_alive && m.map_id == s.map_id() &&
                           d2xy(m.x, m.y, p.rx, p.ry) <= kLeash * kLeash;
        return false;
    };
    auto allowed = [&](NearbyUnit const& u) -> bool
    {
        if (u.hp <= 0) return false;
        const float rd2 = d2xy(u.x, u.y, p.rx, p.ry);
        if (rd2 <= kRallyReach * kRallyReach) return true;
        return !p.tank && rd2 <= kRangedReach * kRangedReach && member_at_rally(u.victim);
    };
    auto find_unit = [&](ObjectGuid gd) -> NearbyUnit const*
    {
        for (auto const& u : s.raw().combat.attackers)      if (u.guid == gd) return &u;
        for (auto const& u : s.raw().combat.nearby_enemies) if (u.guid == gd) return &u;
        return nullptr;
    };

    const bool out_of_leash = d2xy(bx, by, p.rx, p.ry) > kLeash * kLeash;
    if (ObjectGuid const v = s.victim(); !v.IsEmpty())
    {
        NearbyUnit const* vu = find_unit(v);
        if (out_of_leash || !vu || !allowed(*vu))
        {
            emit.stop_attack();
            head_to(p.sx, p.sy, p.sz, /*run*/ true);
            tag("idle:fallback_leash", "combat:fallback_leash");
            return true;
        }
        return false;   // a mob that followed us: the rotation (tank logic included) fights it here
    }
    if (out_of_leash)
    {
        head_to(p.sx, p.sy, p.sz, /*run*/ true);
        tag("idle:fallback_return", "combat:fallback_return");
        return true;
    }
    // Healers pick no target: the rotation keeps healing.
    if (!healer)
    {
        // Tank: first what is hitting someone else (taunt / pick up), then the nearest. DPS: what
        // hits a member at the rally, then the nearest.
        NearbyUnit const* best = nullptr;
        int best_rank = 99;
        float best_d2 = 0.f;
        auto consider = [&](NearbyUnit const& u)
        {
            if (!allowed(u)) return;
            const bool on_other = !u.victim.IsEmpty() && u.victim != s.guid() && member_at_rally(u.victim);
            const bool on_any = on_other || u.victim == s.guid();
            const int rank = p.tank ? (on_other ? 0 : 1) : (on_any ? 0 : 1);
            const float ud2 = d2xy(bx, by, u.x, u.y);
            if (!best || rank < best_rank || (rank == best_rank && ud2 < best_d2))
            { best = &u; best_rank = rank; best_d2 = ud2; }
        };
        for (auto const& u : s.raw().combat.attackers)      consider(u);
        for (auto const& u : s.raw().combat.nearby_enemies)
            if (u.unit_in_combat || !u.victim.IsEmpty())       // only mobs that came after us
                consider(u);
        if (best && emit.start_attack(best->guid))
        {
            tag("idle:fallback_engage", "combat:fallback_engage");
            return true;
        }
    }
    // Nothing here to fight: hold the slot.
    if (d2xy(bx, by, p.sx, p.sy) > 4.0f * 4.0f && !s.is_casting())
    {
        head_to(p.sx, p.sy, p.sz, /*run*/ false);
        tag("idle:fallback_hold", "combat:fallback_hold");
        return true;
    }
    if (in_combat_state && !healer)
    {
        tag("idle:fallback_hold", "combat:fallback_hold");
        return true;   // in combat with nothing in reach: hold, don't let the gap-close chase
    }
    return false;      // idle at the slot: upkeep (buffs, food, drink) runs below
}

// The owner's pull back (BotAI::PullBackPlan, set by BotCommandParser OrderPullBack; main tank only).
//  Pull:    walk to within range of the target (never into a raid avoid circle), open with the first ranged
//           spell the class can use (Throw / Shoot, Faerie Fire (Feral), Judgement, Lightning Bolt); with
//           none (or after a NO_AMMO failure, or if the opener did not take within 3 s), engage it plain
//           (start_attack) from 15y. The mob being in combat with a victim ends the phase. An approach whose path is
//           refused twice (hazard / no path) or that gets no closer for 4 s is abandoned: pull from where the tank
//           stands if something reaches, else go back to the return point and end ("unreachable").
//  Retreat: stop attacking / casting and run to the return point (rally / owner / own spot); the mob follows. A mob
//           that dropped combat (evade) or left the scan ends the order. Same refusal / no-progress rule: a run
//           that cannot get there ends the order (the tank rules take over). At the point the mob's victim must be the
//           tank: otherwise (mob within 40y) go back to it and taunt / engage again, up to 3 times, then end.
//  Hold:    at the return point face the mob and wait for it there; once it is within 8y (or anything else
//           attacks us, or after 8 s) the rotation takes over and tanks it, 12y leash back to the point. Ends when the
//           mob is dead / gone or combat has been over for 3 s.
//  Whole order: 30 s from the order for Pull + Retreat (the tank then walks back to the return point); dead tank ends it.
bool PullBackTick(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,
                  BotIntentEmitter& emit, bool in_combat_state)
{
    if (!ai.pull_back_active()) return false;
    BotAI::PullBackPlan const p = ai.pull_back_plan();
    if (!p.active) return false;
    const uint32 now_ms = s.published_at_ms();
    auto finish = [&](char const* why) -> bool
    {
        ai.end_pull_back();
        TC_LOG_INFO("playerbot.v2", "[pull_back] {} order ends: {}", s.name(), why);
        return false;
    };
    if (!s.is_alive()) return finish("tank died");
    if (p.map != s.map_id()) return finish("left the map");
    using Phase = BotAI::PullBackPhase;
    constexpr uint32 kTimeoutMs = 30000u, kHoldCapMs = 300000u;
    auto tag = [&](char const* idle_tag, char const* combat_tag)
    { ai.set_last_rule_fired(in_combat_state ? combat_tag : idle_tag); };
    if (p.phase != Phase::Hold && now_ms - p.started_ms > kTimeoutMs)
    {
        // Don't stand where the pull left us (a ledge, the lava edge): walk back to the return point once, off the mob.
        if (!s.victim().IsEmpty()) emit.stop_attack();
        if (s.is_casting()) emit.cancel_cast();
        emit.move_to(p.rx, p.ry, p.rz, true);
        TC_LOG_INFO("playerbot.v2", "[pull_back] {} timeout: walking back to the return point ({:.1f},{:.1f})",
            s.name(), p.rx, p.ry);
        finish("timeout (30 s)");
        tag("idle:pull_back_retreat", "combat:pull_back_retreat");
        return true;
    }
    if (p.phase == Phase::Hold && now_ms - p.phase_ms > kHoldCapMs)
        return finish("hold cap");

    float bx, by, bz; s.position(bx, by, bz);
    auto d2xy = [](float ax, float ay, float cx, float cy) { return (ax - cx) * (ax - cx) + (ay - cy) * (ay - cy); };
    // head_to is true when the move was queued; false = refused (counted by hopeless()).
    auto head_to = [&](float x, float y, float z, bool run, bool direct = false)
    {
        if (s.is_moving() && s.has_path_destination())
        {
            float qx, qy, qz; s.path_destination(qx, qy, qz);
            if (d2xy(qx, qy, x, y) < 2.0f * 2.0f) return true;
        }
        // The same walk was emitted a moment ago: it is under way, not refused (Deadmines 2026-10-15 n43: the retake's approach
        // re-asked for its spot each tick, the dedup answered false, and two of those ended the order as "path refused 2x").
        if (emit.move_already_emitted(x, y, z)) return true;
        return emit.move_to(x, y, z, run, direct);
    };
    // The movement toward a goal is hopeless: a path refusal (move refused, or the path-block counter grew) on 2
    // ticks, or no closer to the goal for 4 s. `why` says which.
    auto hopeless = [&](float dgoal, bool refused, std::string& why) -> bool
    {
        uint32 const bc = s.path_blocked_count();
        bool give = false;
        // A stunned or rooted tank cannot move: its refused walks and its standing still say nothing about the way. smlibrary
        // 0.1.28f: a Scarlet Gallant's Hammer of Justice held the tank 4 s on the way back, both refusals came from the stun, and
        // the order gave the return point up 15 y short and fought the pack where the stun had caught him.
        if (s.is_stunned() || s.is_rooted())
        {
            ai.edit_pull_back([&](BotAI::PullBackPlan& q) { q.blocked_seen = bc; q.progress_ms = now_ms; });
            return false;
        }
        ai.edit_pull_back([&](BotAI::PullBackPlan& q)
        {
            if (q.blocked_seen == ~0u) q.blocked_seen = bc;
            bool const ref = refused || bc > q.blocked_seen;
            q.blocked_seen = bc;
            if (ref && q.refusals < 255) ++q.refusals;
            if (dgoal < q.best_d - 1.0f) { q.best_d = dgoal; q.progress_ms = now_ms; }
            if (q.progress_ms == 0) q.progress_ms = now_ms;
            if (q.refusals >= 2)                          { give = true; why = fmt::format("path refused {}x", q.refusals); }
            else if (now_ms - q.progress_ms > 4000u)      { give = true; why = "no progress for 4 s"; }
        });
        return give;
    };
    auto in_circle = [&](float x, float y) -> bool
    {
        if (!Services::Config().behavior_avoid_circles()) return false;
        PveOrder const& po = s.raw().pve_order;
        return po.active && po.in_avoid_circle(x, y, 2.0f);
    };
    NearbyUnit const* tu = nullptr;
    for (auto const& u : s.raw().combat.attackers)      if (u.guid == p.target) { tu = &u; break; }
    if (!tu)
        for (auto const& u : s.raw().combat.nearby_enemies) if (u.guid == p.target) { tu = &u; break; }
    // The target died or left the scan while its pack came: the order goes on with the pack mob that is on the group, so the
    // tank still runs the pack back to the return point. Ending the order here left the tank fighting where the pull had taken it
    // (SFK 2026-10-20 run 4: the Black Ravager died to the opener, "target left the scan", and the tank fought the two Bleak Worgs
    // on the balcony 9 y above the healer, out of its sight, for 42 s and died). A Hold ends as before: the tank is at the point.
    if (p.phase != Phase::Hold && (!tu || tu->hp <= 0))
    {
        NearbyUnit const* next = nullptr;
        float next_d2 = 30.0f * 30.0f;
        auto consider = [&](NearbyUnit const& u)
        {
            if (u.hp <= 0 || u.guid == p.target || !u.unit_in_combat) return;
            ThreatVictim const k = ThreatVictim(u.victim_kind);
            if (k != ThreatVictim::Me && k != ThreatVictim::Tank && k != ThreatVictim::Member && k != ThreatVictim::MemberPet) return;
            const float d2 = d2xy(bx, by, u.x, u.y);
            if (d2 < next_d2) { next_d2 = d2; next = &u; }
        };
        for (auto const& u : s.raw().combat.attackers)      consider(u);
        for (auto const& u : s.raw().combat.nearby_enemies) consider(u);
        if (next)
        {
            TC_LOG_INFO("playerbot.v2", "[pull_back] {} target {} {}: entry {} ({}) of its pack is on the group {:.0f}y away, the order goes on with it",
                s.name(), p.target.GetCounter(), tu ? "dead" : "left the scan", next->entry, next->guid.GetCounter(), std::sqrt(next_d2));
            ObjectGuid const ng = next->guid;
            float const nx = next->x, ny = next->y, nz = next->z;
            ai.edit_pull_back([&](BotAI::PullBackPlan& q) { q.target = ng; q.tx = nx; q.ty = ny; q.tz = nz; });
            ai.set_pull_back_phase(Phase::Retreat, now_ms);
            if (!s.victim().IsEmpty()) emit.stop_attack();
            if (s.is_casting()) emit.cancel_cast();
            head_to(p.rx, p.ry, p.rz, true);
            tag("idle:pull_back_retreat", "combat:pull_back_retreat");
            return true;
        }
        if (tu)
            return finish("target dead");
    }

    // Taunt-type pull spell the class knows (melee range), else 0.
    auto melee_taunt = [&]() -> uint32
    {
        static constexpr uint32 kTaunts[] = { 355u /*Taunt*/, 6795u /*Growl*/, 62124u /*Hand of Reckoning*/ };
        for (uint32 sp : kTaunts)
            if (s.knows_spell(sp) && s.cd_remaining(sp).count() == 0)
                return sp;
        return 0u;
    };

    // A mob of the pack, not the pull target, beating on a member at the return point: a retake would walk the tank away from it
    // (wc_cobrahn_led 0.1.27 set B: the Druid of the Fang stayed on its ledge 26 y off, the warrior went back up for it while the
    // Deviate Viper and Adder that came with it killed the priest 11 y from the return point; first Taunt 7 s late).
    auto pack_on_group_at_point = [&]() -> NearbyUnit const*
    {
        auto on_group = [&](NearbyUnit const& u)
        {
            if (u.hp <= 0 || u.guid == p.target || !u.unit_in_combat) return false;
            ThreatVictim const k = ThreatVictim(u.victim_kind);
            return (k == ThreatVictim::Member || k == ThreatVictim::MemberPet) && d2xy(p.rx, p.ry, u.x, u.y) <= 20.0f * 20.0f;
        };
        for (auto const& u : s.raw().combat.attackers)      if (on_group(u)) return &u;
        for (auto const& u : s.raw().combat.nearby_enemies) if (on_group(u)) return &u;
        return nullptr;
    };
    auto finish_for_pack = [&](NearbyUnit const& u) -> bool
    {
        TC_LOG_INFO("playerbot.v2", "[pull_back] {} entry {} ({}) of the pack is on the group {:.0f}y from the return point, the pull target {:.0f}y off",
            s.name(), u.entry, u.guid.GetCounter(), std::sqrt(d2xy(p.rx, p.ry, u.x, u.y)), tu ? std::sqrt(d2xy(bx, by, tu->x, tu->y)) : -1.0f);
        return finish("pack mob on the group at the return point - tank rules take over");
    };

    if (p.phase == Phase::Pull)
    {
        if (p.retakes > 0)
            if (NearbyUnit const* u = pack_on_group_at_point())
                return finish_for_pack(*u);
        // Another pack took the tank on the way to the opener spot: run that one back now and leave the far target alone.
        // smlibrary L33 (d90ef3de69, 0128m): the Adept had no walkable pull spot, the straight line to the opener spot passed two
        // idle Scarlet Gallants, they were on the tank 3 s before the Throw, the tank threw at the Adept all the same and ran
        // back with both packs; a Gallant stun held him and the Adept's Holy Smite killed him.
        if (p.opened_ms == 0 && p.retakes == 0)
        {
            NearbyUnit const* caught = nullptr;
            float caught_d2 = 30.0f * 30.0f;
            for (auto const& u : s.raw().combat.attackers)
            {
                if (u.hp <= 0 || u.guid == p.target || !u.unit_in_combat) continue;
                if (u.victim != s.guid()) continue;
                const float d2 = d2xy(bx, by, u.x, u.y);
                if (d2 < caught_d2) { caught_d2 = d2; caught = &u; }
            }
            if (caught)
            {
                TC_LOG_INFO("playerbot.v2", "[pull_back] {} entry {} ({}) took me {:.0f}y away before the opener on {}: the order goes on with "
                    "it, the far target stays where it is", s.name(), caught->entry, caught->guid.GetCounter(), std::sqrt(caught_d2),
                    p.target.GetCounter());
                ObjectGuid const ng = caught->guid;
                float const nx = caught->x, ny = caught->y, nz = caught->z;
                ai.edit_pull_back([&](BotAI::PullBackPlan& q) { q.target = ng; q.tx = nx; q.ty = ny; q.tz = nz; });
                ai.set_pull_back_phase(Phase::Retreat, now_ms);
                if (!s.victim().IsEmpty()) emit.stop_attack();
                if (s.is_casting()) emit.cancel_cast();
                head_to(p.rx, p.ry, p.rz, true);
                tag("idle:pull_back_retreat", "combat:pull_back_retreat");
                return true;
            }
        }
        if (tu && tu->unit_in_combat && !tu->victim.IsEmpty() && (p.retakes == 0 || tu->victim == s.guid()))
        {
            // Aggro'd (by the opener or anybody) - or, on a retake, back on the tank: run it back.
            ai.set_pull_back_phase(Phase::Retreat, now_ms);
            TC_LOG_INFO("playerbot.v2", "[pull_back] {} phase Retreat: target engaged{} -> return point ({:.1f},{:.1f})",
                s.name(), p.opened_ms ? " after the opener" : "", p.rx, p.ry);
            if (!s.victim().IsEmpty()) emit.stop_attack();
            if (s.is_casting()) emit.cancel_cast();
            head_to(p.rx, p.ry, p.rz, true);
            tag("idle:pull_back_retreat", "combat:pull_back_retreat");
            return true;
        }
        float const tx = tu ? tu->x : p.tx, ty = tu ? tu->y : p.ty, tz = tu ? tu->z : p.tz;
        const float d = std::sqrt(d2xy(bx, by, tx, ty));
        // A thrown weapon / ammo the warrior does not have: the cast failed NO_AMMO since the order began, so no ranged
        // opener for the rest of this order (the snapshot does not show thrown weapons).
        bool opener_unavail = p.opener_unavail;
        if (!opener_unavail && ai.last_no_ammo_ms() != 0 && ai.last_no_ammo_ms() >= p.started_ms)
        {
            opener_unavail = true;
            ai.edit_pull_back([](BotAI::PullBackPlan& q) { q.opener_unavail = true; });
            TC_LOG_INFO("playerbot.v2", "[pull_back] {} ranged opener refused (no ammo or no usable ranged weapon): taunt / plain engage instead", s.name());
        }
        // Opener: first ready ranged spell of the class whose window holds d; none (or it did not take) = plain engage.
        struct Opener { uint32 spell; float min_d, max_d; };
        static constexpr Opener kWarrior[] = { { 2764, 9.f, 28.f }, { 2480, 9.f, 33.f }, { 7918, 9.f, 33.f }, { 7919, 9.f, 33.f } };
        static constexpr Opener kPaladin[] = { { 20271, 0.f, 9.f }, { 853, 0.f, 9.f } };
        static constexpr Opener kShaman[]  = { { 403, 0.f, 28.f }, { 8042, 0.f, 18.f } };
        static constexpr Opener kDruid[]   = { { 16857, 0.f, 28.f }, { 8921, 0.f, 28.f } };
        Opener const* list = nullptr; size_t n = 0;
        switch (s.cls())
        {
            case 1:  list = kWarrior; n = std::size(kWarrior); break;    // CLASS_WARRIOR
            case 2:  list = kPaladin; n = std::size(kPaladin); break;    // CLASS_PALADIN
            case 7:  list = kShaman;  n = std::size(kShaman);  break;    // CLASS_SHAMAN
            case 11: list = kDruid;   n = std::size(kDruid);   break;    // CLASS_DRUID
            default: break;
        }
        const bool opener_failed = p.opened_ms != 0 && now_ms - p.opened_ms > 3000u;
        if (opener_failed && !p.opener_failed_logged)
        {
            ai.edit_pull_back([](BotAI::PullBackPlan& q) { q.opener_failed_logged = true; });
            TC_LOG_INFO("playerbot.v2", "[pull_back] {} the opener did not engage {} within 3 s: target {:.0f}y, in sight {}, hp {}",
                s.name(), p.target.GetCounter(), d, tu ? (tu->in_los ? "yes" : "NO") : "?", tu ? tu->hp : -1);
        }
        // A warrior's shot is the one his ranged slot fires: Throw needs a thrown weapon, Shoot Bow / Gun / Crossbow the matching
        // weapon and ammo. Picking by "known" alone sent Throw from a gun-wearing tank; the core refused it EQUIPPED_ITEM, the order
        // dropped the opener and the tank walked into the pack and body-pulled it (playtest 2026-10-15).
        auto weapon_fires = [&](uint32 spell) -> bool
        {
            if (s.cls() != 1) return true;   // CLASS_WARRIOR only
            uint8 const sub = s.ranged_subclass();
            switch (spell)
            {
                case 2764: return sub == 16;                              // Throw: ITEM_SUBCLASS_WEAPON_THROWN
                case 2480: return sub == 2 && s.ammo_count() > 0;         // Shoot Bow: ITEM_SUBCLASS_WEAPON_BOW
                case 7918: return sub == 3 && s.ammo_count() > 0;         // Shoot Gun: ITEM_SUBCLASS_WEAPON_GUN
                case 7919: return sub == 18 && s.ammo_count() > 0;        // Shoot Crossbow: ITEM_SUBCLASS_WEAPON_CROSSBOW
                default:   return true;
            }
        };
        Opener const* pick = nullptr;
        // A retake (the mob is on somebody else): a ranged hit does not take it, only a taunt / melee swing does.
        if (!opener_failed && !opener_unavail && p.retakes == 0)
            for (size_t i = 0; i < n; ++i)
                if (s.knows_spell(list[i].spell) && s.cd_remaining(list[i].spell).count() == 0 && weapon_fires(list[i].spell))
                { pick = &list[i]; break; }
        if (!pick && !p.opened_ms && p.retakes == 0 && !p.opener_failed_logged &&
            RaidLogDue(RaidLogKey(s, RaidLogKind::PullBackOpener), now_ms, 10000u))
            TC_LOG_INFO("playerbot.v2", "[pull_back] {} no ranged opener (ranged slot subclass {}, ammo {}): walking in to pull by hand",
                s.name(), s.ranged_subclass() == 0xFF ? -1 : int(s.ranged_subclass()), s.ammo_count());
        // No ranged opener: the plain engage starts from 15y (a retake: from taunt range).
        const float reach = pick ? pick->max_d : (p.retakes ? 5.0f : 15.0f);
        const float near_min = pick ? pick->min_d : 0.0f;
        if (!tu && d <= 38.0f)
            return finish("target left the scan");

        auto go_home_and_finish = [&](char const* why)
        {
            if (!s.victim().IsEmpty()) emit.stop_attack();
            if (s.is_casting()) emit.cancel_cast();
            emit.move_to(p.rx, p.ry, p.rz, true);
            TC_LOG_INFO("playerbot.v2", "[pull_back] {} unreachable: {} -> back to the return point ({:.1f},{:.1f})",
                s.name(), why, p.rx, p.ry);
            finish("unreachable");
            tag("idle:pull_back_retreat", "combat:pull_back_retreat");
            return true;
        };

        if (!p.stand_pull && (d > reach - 2.0f || d < near_min))
        {
            // Close to / back off to an in-window spot on the line target -> return point (the raid side; never into an
            // avoid circle), so a tank standing on a ledge below the pack walks back up to a spot the raid can reach.
            const float want = d > reach - 2.0f ? std::max(near_min + 1.0f, reach - 4.0f) : near_min + 3.0f;
            const float rd = std::sqrt(d2xy(p.rx, p.ry, tx, ty));
            const bool use_ret = rd > want + 1.0f;
            const float ox = use_ret ? p.rx : bx, oy = use_ret ? p.ry : by;
            const float od = use_ret ? rd : (d > 0.1f ? d : 0.1f);
            const float sx = tx + (ox - tx) / od * want, sy = ty + (oy - ty) / od * want;
            const float sz = use_ret ? tz + (p.rz - tz) * std::min(1.0f, want / od) : tz;
            if (in_circle(sx, sy))
                return finish("pull spot is inside an avoid circle");
            // A refused spot is not asked for again: smlibrary 0.1.28f, the spot (243,-217) lay outside the room with no ground under it,
            // and the tank asked for it and for the straight-line spot on every tick for 5 s (40 refused paths) while walking the line.
            bool queued = !p.spot_refused && head_to(sx, sy, sz, true);
            if (!queued)
            {
                bool const first_refusal = !p.spot_refused;
                if (first_refusal)
                {
                    ai.edit_pull_back([](BotAI::PullBackPlan& q) { q.spot_refused = true; });
                    TC_LOG_INFO("playerbot.v2", "[pull_back] {} move to pull spot ({:.1f},{:.1f},{:.1f}) refused: the straight line to the target from now on",
                        s.name(), sx, sy, sz);
                }
                // The spot on the line to the return point may sit where no path leads (a ledge, a wall): the straight line
                // toward the mob is the other way to get in window (deadmines_full: both refusals ended the order at 18 y).
                if (d > want + 1.0f)
                {
                    const float k = (d - want) / d;
                    const float ax = bx + (tx - bx) * k, ay = by + (ty - by) * k, az = bz + (tz - bz) * k;
                    if (!in_circle(ax, ay))
                    {
                        queued = head_to(ax, ay, az, true);
                        if (first_refusal || !queued)
                            TC_LOG_INFO("playerbot.v2", "[pull_back] {} straight-line spot ({:.1f},{:.1f},{:.1f}) {}", s.name(), ax, ay, az,
                                queued ? "queued" : "refused too");
                        // Mob in sight and close, no route from the navmesh (a lip in the floor, Deadmines 2026-10-15): walk the line itself.
                        if (!queued && tu && tu->in_los && d <= 25.0f)
                        {
                            queued = head_to(ax, ay, az, true, /*direct*/ true);
                            TC_LOG_INFO("playerbot.v2", "[pull_back] {} direct walk to ({:.1f},{:.1f},{:.1f}) {}", s.name(), ax, ay, az,
                                queued ? "queued" : "refused");
                        }
                    }
                }
                // Neither the spot nor the whole line is pathable (MC lava, 2026-10-22: the tank ran out and back without a shot):
                // try shorter steps along the line toward the target, each its own path, before giving the approach up.
                if (!queued && d > want + 1.0f)
                    for (float frac : { 0.75f, 0.5f, 0.25f })
                    {
                        const float k = frac * (d - want) / d;
                        const float ax = bx + (tx - bx) * k, ay = by + (ty - by) * k, az = bz + (tz - bz) * k;
                        if (in_circle(ax, ay)) continue;
                        if (head_to(ax, ay, az, true))
                        {
                            queued = true;
                            TC_LOG_INFO("playerbot.v2", "[pull_back] {} short step {:.0f}% toward the target ({:.1f},{:.1f},{:.1f}) queued",
                                s.name(), frac * 100.0f, ax, ay, az);
                            break;
                        }
                    }
            }
            std::string why;
            if (hopeless(std::sqrt(d2xy(bx, by, sx, sy)), !queued, why))
            {
                // Pull from where we stand if anything reaches from here; else abort.
                bool const can_range = pick && d >= pick->min_d && d <= pick->max_d && (!tu || tu->in_los);
                bool const can_melee = d <= 6.0f;
                if (!can_range && !can_melee)
                    return go_home_and_finish(why.c_str());
                ai.edit_pull_back([](BotAI::PullBackPlan& q) { q.stand_pull = true; });
                TC_LOG_INFO("playerbot.v2", "[pull_back] {} unreachable approach ({}): pulling from {:.0f}y where it stands",
                    s.name(), why, d);
            }
            tag("idle:pull_back_approach", "combat:pull_back_approach");
            return true;
        }
        if (p.stand_pull)
        {
            bool const can_range = pick && d >= pick->min_d && d <= pick->max_d && (!tu || tu->in_los);
            if (!can_range && d > 6.0f)
                return go_home_and_finish("nothing reaches the target from here");
            pick = can_range ? pick : nullptr;
        }
        bool sent = false;
        bool taunted = false;
        if (pick)
            sent = emit.cast(pick->spell, p.target);
        else if (d <= 5.5f)
            if (uint32 tsp = melee_taunt())
            { sent = emit.cast(tsp, p.target); taunted = sent; }
        if (!sent)
            sent = emit.start_attack(p.target);
        if (sent && !p.opened_ms)
        {
            ai.mark_pull_back_opened(now_ms);
            TC_LOG_INFO("playerbot.v2", "[pull_back] {} phase Pull: {} on {} at {:.0f}y",
                s.name(), pick && sent ? "ranged opener" : taunted ? "taunt" : "plain engage", p.target.GetCounter(), d);
        }
        tag("idle:pull_back_open", "combat:pull_back_open");
        return true;
    }

    if (!tu)
        return finish("target left the scan");
    if (p.phase == Phase::Retreat)
    {
        if (!tu->unit_in_combat && tu->victim.IsEmpty())
            return finish("target dropped combat (evaded)");
        float const dr = std::sqrt(d2xy(bx, by, p.rx, p.ry));
        float const dmob = std::sqrt(d2xy(bx, by, tu->x, tu->y));
        if (dr > 2.5f)
        {
            if (!s.victim().IsEmpty()) emit.stop_attack();
            if (s.is_casting()) emit.cancel_cast();
            bool queued = head_to(p.rx, p.ry, p.rz, true);
            // The emitter refuses a point for 6 s after one failed path to it (BotIntentEmitter refusal guard, 3 y), even a point
            // the tank stood on a moment ago. RFD rfd_glutton (0.1.27): the tank reached the return point, went back for the mob
            // (retake), and the run home was refused twice in the same second; the order ended 18 y out and the Boneflayer pack
            // ran over the mage. Ask once more for a spot 4 y short of the point, on the tank's side, before counting a refusal.
            if (!queued && dr > 6.0f)
            {
                float const k = 4.0f / dr;
                queued = head_to(p.rx + (bx - p.rx) * k, p.ry + (by - p.ry) * k, p.rz, true);
            }
            std::string why;
            if (hopeless(dr, !queued, why))
            {
                // The run home keeps failing: the tank holds where it stands and lets the pack come to it. Ending the order here
                // handed the pull to the tank rules, which walked the tank onto the pack (smlibrary_loksey 0.1.28b: the return point
                // refused twice 24 y short, the warrior went in and the run failed 15 s later). The Hold below hands the fight to the
                // rotation once the mob arrives, hits the tank, or 8 s pass.
                TC_LOG_INFO("playerbot.v2", "[pull_back] {} unreachable: retreat to ({:.1f},{:.1f}) {} ({:.0f}y short, mob {:.0f}y): holds here for the pack",
                    s.name(), p.rx, p.ry, why, dr, dmob);
                ai.edit_pull_back([&](BotAI::PullBackPlan& q) { q.rx = bx; q.ry = by; q.rz = bz; });
                ai.set_pull_back_phase(Phase::Hold, now_ms);
                emit.face_target(p.target);
                tag("idle:pull_back_hold", "combat:pull_back_hold");
                return true;
            }
            tag("idle:pull_back_retreat", "combat:pull_back_retreat");
            return true;
        }
        // At the return point. Hold only with the mob on the tank; else go and get it back (bounded).
        // A mob of the line-of-sight pull (PveOrder::sight_pull) stands off out of sight of this point and has to come to it: a retake
        // would walk the tank back out to where it stands.
        if (tu->victim != s.guid() && s.raw().pve_order.active && s.raw().pve_order.sight_pull_mob(tu->guid))
        {
            ai.set_pull_back_phase(Phase::Hold, now_ms);
            emit.face_target(p.target);
            TC_LOG_INFO("playerbot.v2", "[pull_back] {} phase Hold: at the line-of-sight spot after {}s, target {:.0f}y away stands off out of sight",
                s.name(), (now_ms - p.started_ms) / 1000u, dmob);
            tag("idle:pull_back_hold", "combat:pull_back_hold");
            return true;
        }
        if (tu->victim != s.guid())
        {
            if (NearbyUnit const* u = pack_on_group_at_point())
                return finish_for_pack(*u);
            if (dmob > 40.0f)
            {
                TC_LOG_INFO("playerbot.v2", "[pull_back] {} at the return point, target {:.0f}y away and not on the tank",
                    s.name(), dmob);
                return finish("mob loose and far - tank rules take over");
            }
            if (p.retakes >= 3)
            {
                TC_LOG_INFO("playerbot.v2", "[pull_back] {} the mob ({:.0f}y) is still not on the tank after 3 retakes",
                    s.name(), dmob);
                return finish("mob not on the tank after 3 tries - tank rules take over");
            }
            ai.edit_pull_back([&](BotAI::PullBackPlan& q) { ++q.retakes; q.retake_ms = now_ms; q.stand_pull = false; });
            ai.set_pull_back_phase(Phase::Pull, now_ms);
            TC_LOG_INFO("playerbot.v2", "[pull_back] {} at the return point but the mob ({:.0f}y) is not on the tank: retake {}",
                s.name(), dmob, p.retakes + 1u);
            tag("idle:pull_back_approach", "combat:pull_back_approach");
            return true;
        }
        ai.set_pull_back_phase(Phase::Hold, now_ms);
        emit.face_target(p.target);
        TC_LOG_INFO("playerbot.v2", "[pull_back] {} phase Hold: at the return point after {}s, target {:.0f}y away on the tank",
            s.name(), (now_ms - p.started_ms) / 1000u, dmob);
        tag("idle:pull_back_hold", "combat:pull_back_hold");
        return true;
    }

    // Hold
    if (!s.in_combat() && now_ms - p.phase_ms > 3000u && !tu->unit_in_combat)
        return finish("combat over");
    const float td2 = d2xy(bx, by, tu->x, tu->y);
    bool others = false;   // something besides the pull target is on us
    for (auto const& u : s.raw().combat.attackers)
        if (u.guid != p.target && u.hp > 0) { others = true; break; }
    // A caster on the tank that stays where it stands (Deadmines' Defias Squallshaper up on the ship's deck) does not come to the return
    // point: the tank waits here, still, so the line-of-sight pull can start (PveGroupCoordinator::PlanSightPull sees a mob of the fight
    // stand off while the tank holds at its return point), and the 8 s cap below still hands the fight on. Handing it to the rotation at
    // once walked the tank up after the caster (5079138c deadmines_full: 1 s after the hold, classic_prot:auto_attack up the gangplank,
    // eight mobs of the deck with Mr. Smite on the group, the priest out of mana at 1401 s, the warrior dead at 1427 s).
    PveOrder const& hpo = s.raw().pve_order;
    bool const sight_pull_on = hpo.active && hpo.sight_pull;
    bool const stands_off = tu->victim == s.guid() && td2 > 8.0f * 8.0f && !others && !sight_pull_on;
    // The line-of-sight pull has its spot: its own rules walk the group, the tank with it, out of the caster's sight.
    if (sight_pull_on && tu->victim == s.guid())
        return false;
    // The mob is on the tank and the tank is fighting: the pull is over and the tank rules own the fight. The leash below only walks a
    // tank the mob has not taken yet; applied to an engaged boss it dragged the main tank off Baron Geddon toward the return point, inside
    // his Inferno circle (MC 2026-10-13: Devane died 20 y out, rule combat:pull_back_leash, no target for 12 s before).
    if (tu->victim == s.guid() && s.in_combat() && now_ms - p.phase_ms > 3000u && !stands_off)
        return false;
    if (d2xy(bx, by, p.rx, p.ry) > 12.0f * 12.0f)
    {
        if (!s.victim().IsEmpty()) emit.stop_attack();
        head_to(p.rx, p.ry, p.rz, true);
        tag("idle:pull_back_leash", "combat:pull_back_leash");
        return true;
    }
    // It has arrived (or hits us, or something else does, or it stands off to cast: 8 s): the rotation tanks it here.
    if (td2 <= 8.0f * 8.0f || others || (tu->victim == s.guid() && !stands_off) || now_ms - p.phase_ms > 8000u)
        return false;
    if (stands_off && !s.victim().IsEmpty())
        emit.stop_attack();   // the auto-attack's chase would walk the tank out to it
    tag("idle:pull_back_hold", "combat:pull_back_hold");
    return true;
}

} // namespace States

// idle:sight_pull - the line-of-sight pull (PveOrder::sight_pull): a member out of combat walks to the spot with the others, so the
// first heal or shot does not open from where the mobs that stand off can see it.
bool SightPullIdleGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    return s.raw().pve_order.active && s.raw().pve_order.sight_pull;
}
bool SightPullIdleFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    return States::SightPullGather(s, ai, emit, /*in_combat_state*/ false);
}

// idle:owner_use - the owner's "use <object>": walk to within reach of the object, use it once, then stay (the command set Stay).
bool OwnerUseGate(BotSnapshotView const&, BotAI& ai, GroupSnapshotView const&, uint32)
{
    return ai.owner_use_active();
}
bool OwnerUseFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
{
    BotAI::OwnerUseOrder const o = ai.owner_use_order();
    uint32 const now_ms = s.published_at_ms();
    auto finish = [&](char const* why)
    {
        ai.end_owner_use();
        TC_LOG_INFO("playerbot.v2", "[owner_use] {} order ends: {}", s.name(), why);
        return false;
    };
    if (!o.set) return finish("cleared");
    if (!s.is_alive()) return finish("dead");
    if (o.map != s.map_id()) return finish("left the map");
    if (now_ms - o.set_ms > 30000u) return finish("not in reach after 30 s");
    float bx, by, bz; s.position(bx, by, bz);
    float const dx = o.x - bx, dy = o.y - by;
    float const d = std::sqrt(dx * dx + dy * dy);
    if (d > 4.5f || std::fabs(o.z - bz) > 5.0f)
    {
        // Stop 2.5 y short on the line from the bot: the object itself (an altar, a brazier) is often solid.
        float const k = d > 2.5f ? (d - 2.5f) / d : 0.0f;
        float const mx = bx + dx * k, my = by + dy * k;
        if (!emit.move_already_emitted(mx, my, o.z) && !emit.move_to(mx, my, o.z, /*run*/ true))
            return finish("no path to the object");
        ai.set_last_rule_fired("idle:owner_use_walk");
        return true;
    }
    emit.use_game_object(o.go);
    ai.end_owner_use();
    // The Stay the command set keeps the follow from walking the bot off the object while the ritual runs; it does not outlive it. The 30 min
    // hold pinned a rogue on the altar floor while the group fought Pyroguard Emberseer (UBRS 132f 22:25, "combat:melee_hold_hazard" with no
    // hazard in the log: the Stay, with the boss on a tank, blocks the melee chase). It ends when the bot's fight starts (DispatchInCombat); two minutes is only the backstop.
    ai.enter_manual(BotAI::OwnerCommand::Stay, ObjectGuid::Empty, now_ms, 120000u);
    ai.set_use_stay();
    TC_LOG_INFO("playerbot.v2", "[owner_use] {} uses object {} at {:.1f}y", s.name(), o.go.GetCounter(), d);
    ai.set_last_rule_fired("idle:owner_use");
    return true;
}

void RegisterPrologueRules(IdleRuleRegistry& r)
{
    auto add = [&r](char const* name, int prio,
                    bool (*gate)(BotSnapshotView const&, BotAI&,
                                 GroupSnapshotView const&, uint32),
                    bool (*fire)(BotSnapshotView const&, BotAI&,
                                 GroupSnapshotView const&,
                                 BotIntentEmitter&, uint32))
    {
        IdleRule rule;
        rule.name     = name;
        rule.priority = prio;
        rule.gate     = gate;
        rule.fire     = fire;
        r.register_rule(std::move(rule));
    };
    add("idle:on_transport_wait", 999, &OnTransportGate,    &OnTransportFire);
    // 998: a bot stranded on a dead BG map must leave before any other
    // idle decision — every other rule operates on a world it isn't in.
    add("idle:bg_orphan_escape",  998, &BgOrphanEscapeGate, &BgOrphanEscapeFire);
    add("idle:watchdog_escape",   995, &WatchdogGate,       &WatchdogFire);
    // 994: the owner's fallback order owns the tick above the opener (States::FallbackTick).
    add("idle:fallback",          994, &FallbackGate,       &FallbackFire);
    add("idle:pull_back",         994, &PullBackGate,       &PullBackFire);
    add("idle:owner_use",         994, &OwnerUseGate,       &OwnerUseFire);
    // 992: a member not yet in the fight joins the line-of-sight pull's spot (SightPullGather) before the opener and the follow rules.
    add("idle:sight_pull",        992, &SightPullIdleGate,  &SightPullIdleFire);
    add("combat:opener",          993, &OpenerGate,         &OpenerFire);
    // 991: fires BEFORE owner_following (990) so a corridor threat
    // preempts the no-op follow tag and start_attack interrupts the
    // engine-level FollowMovementGenerator.
    add("idle:follow_pull_threat",991, &FollowPullThreatGate, &FollowPullThreatFire);
    add("idle:owner_following",   990, &OwnerFollowGate,    &OwnerFollowFire);
    add("idle:owner_holding",     985, &OwnerHoldGate,      &OwnerHoldFire);
    add("idle:owner_stay",        980, &OwnerStayGate,      &OwnerStayFire);
    add("idle:owner_engaging",    975, &OwnerEngageGate,    &OwnerEngageFire);
    add("idle:owner_disengage",   970, &OwnerDisengageGate, &OwnerDisengageFire);
    add("idle:owner_action",      965, &OwnerActionGate,    &OwnerActionFire);
    add("idle:owner_escort",      960, &OwnerEscortGate,    &OwnerEscortFire);
}

} // namespace Playerbot
