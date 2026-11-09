// State_InGroup - Cross-cutting layer that runs alongside the primary state
// whenever the bot is grouped. Today: keep within follow distance of the
// leader when idle. Future: ready-checks, raid mark targeting, role hand-off.

#include "StateBase.h"
#include "MaintainHelpers.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotTextMgr.h"
#include "Bot/Quest/CompanionQuest.h"   // CompanionQuest::QuestLabel ([quest] log lines)
#include "Group/GroupSnapshot.h"
#include "Bot/Formation.h"
#include "Bot/Dungeon/DungeonScript.h"
#include "Bot/BotRegistry.h"
#include "Bot/MarkGate.h"       // Playerbot.Behavior.MarksNeedOrder
#include "Services.h"
#include "GroupControl.h"
#include "Util/ConfigReader.h"  // full type for Services::Config() (Playerbot.Behavior.Formation)
#include <algorithm>
#include "Travel/UnifiedTravelGraph.h"
#include "SharedDefines.h"
#include "Bot/ThreatDiscipline.h"   // ThreatOpeningLead (passive, tank first)
#include "Bot/ClassicUpkeep.h"      // IsEating / IsDrinking (stand to defend)
#include "Log.h"
#include "GameTime.h"
#include <cmath>
#include <limits>

namespace Playerbot::States {

namespace {

float DistanceXY(float ax, float ay, float bx, float by)
{
    const float dx = ax - bx;
    const float dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

// Same melee-spec table as State_InCombat::IsMeleeSpec. Duplicated locally
// rather than adding a cross-state header — both pieces are small and the
// definitions need to stay in sync per spec.
bool IsMeleeClass(uint8 cls, uint32 spec)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
        case CLASS_ROGUE:
        case CLASS_DEMON_HUNTER:
        case CLASS_MONK:
            return true;
        case CLASS_HUNTER:
            return spec == 255;          // Survival
        case CLASS_DRUID:
            return spec == 103 || spec == 104; // Feral / Guardian
        case CLASS_SHAMAN:
            return spec == 263;          // Enhancement
        default:
            return false;
    }
}

} // anonymous

// A ready check is answered by every grouped bot, dead or busy rezzing or not: a bot that skips it holds the whole check open until it times out.
static void AnswerReadyCheck(BotAI& ai, BotSnapshotView const& snapshot, GroupSnapshotView const& group, BotIntentEmitter& emit)
{
    // Auto-respond ready when the leader starts a ready check. Bots that don't
    // answer block the rest of the group from pulling. Edge-triggered: emit
    // exactly once per active window (cleared when the snapshot reports the
    // check no longer active). Server-side response is idempotent but the
    // intent still costs queue slots, so dedup is worth it.
    if (group.ready_check_active())
    {
        if (!ai.ready_check_acked())
        {
            // Real information (the owner's ReadyCheckAction): badly low health, a mana
            // user badly low on mana or broken gear answers not ready and says why
            // in party chat; the out-of-combat recovery rules then drink / eat
            // / repair. The check is advisory, nothing else waits on the answer.
            // A bot a little under (just arrived, a buff cast) is ready: the whole raid answering
            // "not ready" after a call-up read as broken.
            bool ready = true;
            char const* why = nullptr;
            if (!snapshot.is_alive())
            { ready = false; why = "dead"; }
            else if (Services::Config().behavior_ready_check_info())
            {
                if (snapshot.hp_pct() < 35)
                { ready = false; why = "low hp, eating first"; }
                else if (snapshot.max_power(POWER_MANA) > 0 && snapshot.power_pct(POWER_MANA) < 35)
                { ready = false; why = "low on mana, drinking first"; }
                else if (snapshot.lowest_equipped_durability_pct() < 15)
                { ready = false; why = "my gear is broken, need a repair"; }
            }
            emit.emit(GroupReadyResponseIntent{ready});
            TC_LOG_INFO("playerbot.v2", "[ready_check] {} answers {}{}{}", snapshot.name(), ready ? "ready" : "not ready", why ? ": " : "", why ? why : "");
            if (why)
            {
                if (snapshot.is_alive())
                    emit.say(why);
            }
            else if (ready)
            {
                // Now and then a plain "ready" in chat, like a person (Playerbot.Behavior.PartyBanter).
                std::string line = BotChatter::BanterLine(BotChatter::Banter::Ready, ai, snapshot);
                if (!line.empty())
                    emit.say(std::move(line));
            }
            ai.set_ready_check_acked(true);
            ai.set_last_rule_fired("ingroup:ready_check_ack");
        }
    }
    else if (ai.ready_check_acked())
    {
        ai.set_ready_check_acked(false);
    }
}


void DispatchInGroup(BotAI& ai,
                     BotSnapshotView snapshot,
                     GroupSnapshotView group,
                     BotIntentEmitter& emit)
{
    if (!group.exists()) return;

    AnswerReadyCheck(ai, snapshot, group, emit);

    // Raid: raise the dead (the owner first) before anything else; out of combat the raid never reaches the idle cascade's ooc_rez.
    if (RaidOocRez(snapshot, ai, group, emit))
        return;

    // Auto-accept any pending player-summon dialog (warlock ritual, meeting
    // stone, BG / LFG summon). Runs before the alive check so dead bots being
    // summoned to a graveyard / instance entrance still accept (the API
    // forwards to Player::SummonIfPossible which doesn't require alive).
    // Edge-trigger isn't necessary: snapshot.has_summon_pending flips false
    // server-side as soon as SummonIfPossible runs, so the next snapshot
    // won't re-trigger.
    // Auto-respond to LFG role-check ping. Server is waiting for every
    // member to declare a role before pushing the queue. Bot's spec maps
    // 1:1 to a role (Tank/Healer/Dps); add LEADER bit when bot is the
    // group leader so the role-check accepts. Without this the whole
    // group blocks indefinitely if any bot member doesn't respond.
    if (snapshot.lfg_role_check_pending())
    {
        uint8 lfg_role = 0;
        switch (ai.effective_role(snapshot))
        {
            case Role::Tank:   lfg_role = /*TANK*/   2; break;
            case Role::Healer: lfg_role = /*HEALER*/ 4; break;
            case Role::Dps:    lfg_role = /*DAMAGE*/ 8; break;
            default:           lfg_role = /*DAMAGE*/ 8; break;
        }
        if (group.leader() == snapshot.raw().guid)
            lfg_role |= /*LEADER*/ 1;
        emit.lfg_role_check(lfg_role);
        ai.set_last_rule_fired("ingroup:lfg_role_check");
    }
    // Group role for the raid / party frame icons (tank / healer / damage). No LFG round-trip happens in a
    // manually formed party or raid (`.bot add`, `;all run`), so nothing else ever sets it; it used to be gated
    // on an active dungeon run and so never fired for a raid or an owner-led group. Edge-triggered: the slot's
    // role (BotSnapshot lfg.published_role, a plain slot read) is compared with the bot's duty role and set only
    // when they differ, i.e. once on joining and again when the duty role (spec swap, `.bot add ... healer`) or
    // the leader bit changes. API::lfg_role_check -> Group::SetLfgRoles is a no-op for unchanged roles, so
    // SMSG_PARTY_UPDATE (watch its [netstat] rate) goes out once per real change, never per tick. Role::Unknown
    // (no spec read yet) waits.
    else if (Role const duty = ai.effective_role(snapshot); duty != Role::Unknown)
    {
        uint8 lfg_role = 0;
        switch (duty)
        {
            case Role::Tank:   lfg_role = /*TANK*/   2; break;
            case Role::Healer: lfg_role = /*HEALER*/ 4; break;
            default:           lfg_role = /*DAMAGE*/ 8; break;
        }
        if (group.leader() == snapshot.raw().guid)
            lfg_role |= /*LEADER*/ 1;
        if (snapshot.lfg_published_role() != lfg_role)
        {
            emit.lfg_role_check(lfg_role);
            ai.set_last_rule_fired("ingroup:set_role_no_lfg");
        }
    }

    // Auto-leave on vote-kick. When the player initiates an LFG
    // vote-to-kick on someone in the group, the bot bows out
    // voluntarily so the kick succeeds and LFG backfills the slot
    // with a fresh bot. TC's LFG vote-kick API doesn't expose WHO
    // the target is, so we accept the false-positive risk: if the
    // vote targeted a human / different bot, this bot leaves anyway
    // and the player just gets a refreshed group. Player intent of
    // "kick a bot" still resolves correctly.
    // Per-vote-kick lockout via BgPort ActionKind (re-used since
    // vote-kicks and BG ports never overlap in the same group).
    if (snapshot.lfg_vote_kick_active())
    {
        const uint32 vk_now = GameTime::GetGameTimeMS();
        const uint64 vk_key = uint64(group.leader().GetCounter()) ^ 0xDEADBEEFu;
        if (!ai.action_recently_tried(BotAI::ActionKind::BgPort,
                                      vk_key, vk_now))
        {
            emit.group_leave();
            ai.note_action_retry(BotAI::ActionKind::BgPort, vk_key, vk_now);
            ai.set_last_rule_fired("ingroup:vote_kick_auto_leave");
            return;
        }
    }

    // Auto-promote-human-to-leader. Bots end up as group leader when
    // LFG promotes the first-spawned bot, when the human gets kicked
    // and reformed, or when bots form a group among themselves and a
    // human later joins. The human should drive — they manage pulls.
    // When THIS bot is leader AND any non-bot human is in the group,
    // hand over leadership. Picks the first non-bot human in the
    // member list (stable enough; could be refined to prefer owner).
    if (group.leader() == snapshot.raw().guid)
    {
        BotRegistry& reg_promote = Services::Registry();
        ObjectGuid promote_target;
        if (auto const* mems_promote = group.members())
        {
            for (auto const& m : *mems_promote)
            {
                if (!m.online || m.guid == snapshot.raw().guid) continue;
                if (reg_promote.has(m.guid.GetCounter())) continue;  // is a bot
                promote_target = m.guid;
                break;
            }
        }
        if (!promote_target.IsEmpty())
        {
            // Edge-trigger: without this dedup the rule re-emits
            // CMSG_SET_PARTY_LEADER every snapshot tick (~3-5 Hz) for the
            // window between intent emit and the leader hand-off taking
            // effect. Server treats subsequent emits as no-ops but each
            // still pays packet-queue + lock cost. ActionKind::BgPort
            // slot is reused (mutually exclusive with vote-kick/promote).
            const uint32 promote_now_ms = GameTime::GetGameTimeMS();
            const uint64 promote_key =
                uint64(promote_target.GetCounter()) ^ 0xCAFEBABEu;
            if (!ai.action_recently_tried(BotAI::ActionKind::BgPort,
                                           promote_key, promote_now_ms))
            {
                emit.group_promote_to_leader(promote_target);
                ai.note_action_retry(BotAI::ActionKind::BgPort,
                                     promote_key, promote_now_ms);
                ai.set_last_rule_fired("ingroup:promote_human_leader");
            }
        }
    }

    // Auto-share newly accepted quests with the party. Each shared quest
    // is recorded in BotAI's shared_ring_ (16-slot circular buffer). Walk
    // the log in order; share the first incomplete quest not already in
    // the ring. Old single-id tracking would alternate sharing two quests
    // forever — the ring fixes that. Skip repeatable/dungeon/raid quests
    // (already universal or party-context-specific).
    // Suppress entirely when the bot is in a BG raid OR a dungeon
    // instance: prep-phase quest-share spam clutters the popup queue for
    // the real player; bots can't progress open-world quests inside
    // either type of instance anyway. The BG raid is recreated per
    // match. Dungeon groups are also typically formed via LFG and the
    // user just wants to run the dungeon — share spam is pure noise.
    //
    // A companion (grouped with its owner) shares like a party member does (Playerbot.Behavior.QuestShare): only
    // quests its owner does not have and could take (CompanionQuest::BuildState, on the snapshot), one pass per
    // 90 s, at most once per quest through the ring.
    if (group.exists() && snapshot.is_alive() &&
        !snapshot.in_battleground() &&
        !snapshot.is_in_dungeon() &&
        Services::Config().behavior_quest_share())
    {
        auto const now_share_ms = snapshot.published_at_ms();
        if (snapshot.is_companion_with_owner())
        {
            if (!ai.action_recently_tried(BotAI::ActionKind::CompanionQuestShare, 0, now_share_ms))
                for (uint32 const qid : snapshot.raw().companion_quest.share)
                {
                    if (ai.quest_recently_shared(qid)) continue;
                    emit.share_quest(qid);
                    ai.note_shared_quest(qid);
                    ai.note_action_retry(BotAI::ActionKind::CompanionQuestShare, 0, now_share_ms);
                    ai.set_last_rule_fired("ingroup:share_quest");
                    TC_LOG_INFO("playerbot.v2", "[quest] {} share {} {}", snapshot.name(), qid, CompanionQuest::QuestLabel(qid));
                    break;
                }
        }
        else
        {
            // A quest is only offered when a real player in the group is not far above it (a level 60 got a level 20 quest
            // from a leveling world bot, WSG test 2026-10-08); 4 = the default Quests.LowLevelHideDiff.
            uint32 lowest_real = 0;
            if (auto const* mems = group.members())
                for (auto const& m : *mems)
                    if (!m.is_bot && m.level > 0 && (lowest_real == 0 || m.level < lowest_real))
                        lowest_real = m.level;
            for (auto const& q : snapshot.raw().quest_log.quests)
            {
                if (q.state == 1) continue;
                if (q.flags & (0x01 | 0x08 | 0x10)) continue;
                if (lowest_real != 0 && lowest_real > uint32(q.level) + 4u) continue;
                if (ai.quest_recently_shared(q.quest_id)) continue;
                emit.share_quest(q.quest_id);
                ai.note_shared_quest(q.quest_id);
                ai.set_last_rule_fired("ingroup:share_quest");
                break;
            }
        }
    }

    if (snapshot.has_quest_share() && Services::Config().behavior_quest_accept())
    {
        // Auto-accept group-shared quests. Server pre-validated eligibility
        // (CanTakeQuest, CanAddQuest, level / class / faction gates) before
        // the share popup ever fires; the API call here just commits the
        // accept and clears m_playerSharingQuest. ServerRefused on the rare
        // race where state changed between share and our accept (log filled
        // up between ticks, etc.) — the next snapshot will drop the share
        // and we won't re-emit.
        //
        // Edge-trigger: m_playerSharingQuest is cleared by the server one
        // tick AFTER the accept lands, so without a dedup we re-emit
        // accept_shared_quest every tick for the round-trip window.
        // Mirror the promote_human_leader pattern using the BgPort
        // ActionKind slot (mutually exclusive with vote-kick/promote).
        const uint32 sq_now_ms = GameTime::GetGameTimeMS();
        constexpr uint64 sq_key = uint64(0xC0DECAFE'1u);
        if (!ai.action_recently_tried(BotAI::ActionKind::BgPort, sq_key, sq_now_ms))
        {
            emit.accept_shared_quest();
            ai.note_action_retry(BotAI::ActionKind::BgPort, sq_key, sq_now_ms);
            ai.set_last_rule_fired("ingroup:share_quest_accept");
            TC_LOG_INFO("playerbot.v2", "[quest] {} accept shared quest {}", snapshot.name(),
                CompanionQuest::QuestLabel(snapshot.shared_quest_id()));
        }
    }

    if (snapshot.has_summon_pending())
    {
        // Decline summons while the bot is in a battleground / dungeon
        // instance — accepting would yank the bot out and forfeit its slot.
        // The owner can re-summon after the bot leaves the instance. Use
        // map-based detection: snapshot.in_battleground() covers PvP, and
        // any encounter-active state (active_encounter_npc_id != 0) means
        // we're mid-fight in a scripted dungeon and shouldn't port out.
        if (snapshot.in_battleground() ||
            snapshot.raw().dungeon_exec.active_encounter_npc_id != 0)
        {
            emit.summon_decline();
            ai.set_last_rule_fired("ingroup:summon_decline");
        }
        else
        {
            emit.summon_accept();
            ai.set_last_rule_fired("ingroup:summon_accept");
        }
        // Don't return — let the rest of the InGroup dispatch run too. The
        // teleport happens on the world thread; this tick's other emits stay
        // valid (target the bot's current position) and naturally lapse on
        // arrival when the next snapshot inverts the relevant ranges.
    }

    if (!snapshot.is_alive()) return;

    // Owner's fallback order: the rally anchor and the target choice belong to States::FallbackTick
    // (idle:fallback / the top of DispatchInCombat); the follow / assist below would pull the bot
    // back out to the owner or the fight it just left.
    if (ai.fallback_active() && ai.fallback_plan().map == snapshot.map_id())
        return;

    // Auto-skull: when this bot is the group leader (or assistant in raids),
    // and the group is engaged, mark the lowest-HP attacking enemy with skull
    // (icon 7). Picks the lowest-HP target from the union of "things attacking
    // me" and "things attacking my groupmates", because that's the kill order
    // a competent leader would call. Skip if a live skull is already set —
    // re-marking mid-fight thrashes assist targets. Trigger only on the
    // leader's tick; the published skull propagates to all bot members
    // through the next group snapshot.
    // In a raid with Playerbot.Behavior.TankMarkers on, the raid coordinator marks the tanks' mobs with the symbols
    // 0-5 (PveGroupCoordinator::PlanRaidTanks never uses skull or cross, those stay the owner's kill call): no second
    // auto-skull from the leader.
    // A human in the group owns the marks (playtest 2026-10-15): the leader bot never calls a skull over his, so only an
    // all-bot group gets the auto-skull.
    bool group_has_human = false;
    if (auto const* mems = group.members())
        for (auto const& m : *mems)
            if (m.online && !m.is_bot) { group_has_human = true; break; }
    if (snapshot.in_combat() && !group.leader().IsEmpty() &&
        group.leader() == snapshot.raw().guid && !group_has_human &&
        !(group.is_raid() && Services::Config().behavior_tank_markers()))
    {
        const ObjectGuid current_skull = group.skull_target();
        bool skull_alive = false;
        if (!current_skull.IsEmpty())
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
                if (u.guid == current_skull && u.hp > 0) { skull_alive = true; break; }
        if (!skull_alive)
        {
            // Skull-pick preference order:
            //   1. attackers (units in combat with us) whose entry is in
            //      the per-dungeon script's high_priority_kill_entries —
            //      these are the "kill first" adds the script knows about
            //      (e.g. Cult Adherent on Deathwhisper, Bone Spike on
            //      Marrowgar).
            //   2. lowest-HP among generic attackers (no priority entry hit).
            //   3. lowest-HP among nearby_enemies (out-of-combat trash hint).
            DungeonAdvice const advice = Services::Initialized()
                ? Services::Dungeons().GetAdvice(snapshot)
                : DungeonAdvice{};
            ObjectGuid pick;
            int32 best_hp = std::numeric_limits<int32>::max();
            // Priority pass.
            if (!advice.high_priority_kill_entries.empty())
            {
                for (auto const& u : snapshot.raw().combat.attackers)
                {
                    if (u.hp <= 0) continue;
                    bool prio = false;
                    for (uint32_t e : advice.high_priority_kill_entries)
                        if (e == u.entry) { prio = true; break; }
                    if (!prio) continue;
                    if (u.hp < best_hp) { best_hp = u.hp; pick = u.guid; }
                }
            }
            auto is_env_ignored = [&](uint32_t entry) -> bool
            {
                for (uint32_t ie : advice.ignore_entries)
                    if (ie == entry) return true;
                return false;
            };
            // Fallback: lowest-HP attacker.
            if (pick.IsEmpty())
            {
                for (auto const& u : snapshot.raw().combat.attackers)
                {
                    if (u.hp <= 0) continue;
                    if (is_env_ignored(u.entry)) continue;
                    if (u.hp < best_hp) { best_hp = u.hp; pick = u.guid; }
                }
            }
            // Only mobs already fighting: a skull is the group's explicit kill
            // call (the DPS threat hold opens on it even before a pull), so
            // the leader's auto-skull must never mark an idle mob of the next
            // pack.
            if (pick.IsEmpty())
                for (auto const& u : snapshot.raw().combat.nearby_enemies)
                {
                    if (u.hp <= 0 || !u.unit_in_combat) continue;
                    if (is_env_ignored(u.entry)) continue;
                    if (u.hp < best_hp) { best_hp = u.hp; pick = u.guid; }
                }
            if (!pick.IsEmpty())
            {
                // Retry pacing: when the executor refuses the set (Locked —
                // bad group state / invalid target), an unthrottled emit
                // re-fired the SAME pick every tick (~6/sec, observed on the
                // 2026-06-11 BG orphans). One attempt per pick per 10s.
                const uint32 icon_now = GameTime::GetGameTimeMS();
                if (!ai.action_recently_tried(BotAI::ActionKind::RaidIcon,
                                              pick.GetCounter(), icon_now))
                {
                    emit.set_raid_target_icon(/*SKULL=*/7, pick);
                    ai.note_action_retry(BotAI::ActionKind::RaidIcon,
                                         pick.GetCounter(), icon_now);
                    ai.set_last_rule_fired("ingroup:auto_skull");
                }
            }
        }
    }
    // The leader's auto-skull outlives its fight: raid icons stay on a unit
    // through a wipe and its evade (and are saved with the group). MC
    // 2026-10-06: the skull from the last wipe sat on an idle Molten Giant,
    // the raid took it for the next kill call and opened on it while the
    // tank held the pull. Out of combat the leader takes its skull off a
    // mob nobody fights - only in an all-bot group (a human's pre-pull mark
    // is kept; the readers ignore a bot leader's mark on an idle mob anyway,
    // GroupSnapshotView::leader_is_bot).
    else if (!snapshot.in_combat() && !group.leader().IsEmpty() &&
             group.leader() == snapshot.raw().guid)
    {
        const ObjectGuid current_skull = group.skull_target();
        if (!current_skull.IsEmpty() && !group_has_human)
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
            {
                if (u.guid != current_skull) continue;
                if (u.hp > 0 && !u.unit_in_combat && u.victim.IsEmpty())
                {
                    const uint32 icon_now = GameTime::GetGameTimeMS();
                    if (!ai.action_recently_tried(BotAI::ActionKind::RaidIcon, 0, icon_now))
                    {
                        emit.set_raid_target_icon(/*SKULL=*/7, ObjectGuid::Empty);
                        ai.note_action_retry(BotAI::ActionKind::RaidIcon, 0, icon_now);
                        ai.set_last_rule_fired("ingroup:auto_skull_clear");
                        TC_LOG_INFO("playerbot.v2",
                            "[raid_pull] {} clears its leftover skull on idle entry {} ({})",
                            snapshot.name(), u.entry, u.guid.GetCounter());
                    }
                }
                break;
            }
    }

    // Leash break (a bot in a group with a player, Passive or Aggressive; open world and instance trash, never a boss or a run the dungeon
    // autopilot drives): a bot whose target is not on anyone in the group
    // and has drifted beyond the leash from the leader drops it and goes back to follow, so no bot runs off after a mob the group never
    // pulled (a kited or fleeing mob, a pack past the aggressive radius). A mob on the bot or on a member is never dropped.
    // Not while the leader has just ordered it (attack / pull window, an engage order, pull back, fallback): his call has no leash.
    if (snapshot.is_led_by_player() && !snapshot.in_battleground() && !snapshot.victim().IsEmpty() &&
        (!snapshot.is_in_instance() || (!ai.dungeon_active() && snapshot.victim_info() && !Classic::IsBossUnit(*snapshot.victim_info()))) &&
        !ai.owner_order_window_open() && !ai.pull_back_active() && !ai.fallback_active() &&
        !(ai.is_manual_mode(snapshot.published_at_ms()) && ai.owner_command() == BotAI::OwnerCommand::Engage))
    {
        auto const stance = ai.owner_stance();
        bool const aggressive = stance == BotAI::OwnerStance::Aggressive;
        bool const leashed = aggressive || ai.owner_passive() || stance == BotAI::OwnerStance::Auto;
        float const reach = aggressive ? Services::Config().behavior_aggressive_radius_yd() : Services::Config().behavior_passive_leash_yd();
        auto const* grp_members = group.members();
        NearbyUnit const* tgt = snapshot.victim_info();
        ObjectGuid const ctl = snapshot.controller_guid();
        if (leashed && reach > 0.f && grp_members && tgt && !ctl.IsEmpty())
        {
            bool on_group = tgt->victim == snapshot.raw().guid;
            GroupMemberSummary const* ctl_member = nullptr;
            for (auto const& m : *grp_members)
            {
                if (m.guid == tgt->victim || (m.guid != snapshot.raw().guid && m.victim == tgt->guid)) on_group = true;   // on a member, or a member (the leader) is fighting it
                if (m.guid == ctl && m.online && m.map_id == snapshot.map_id()) ctl_member = &m;
            }
            if (!on_group && ctl_member &&
                std::hypot(tgt->x - ctl_member->x, tgt->y - ctl_member->y) > reach + 10.0f)
            {
                emit.stop_attack();
                if (snapshot.has_pet())
                    emit.pet_set_command_state(1);   // COMMAND_FOLLOW: the pet comes back with it
                ai.set_last_rule_fired("ingroup:leash_break");
                static std::atomic<uint32> s_leash_log_ms{ 0 };
                uint32 const now_lb = snapshot.published_at_ms();
                uint32 last_lb = s_leash_log_ms.load(std::memory_order_relaxed);
                if (now_lb - last_lb >= 500u && s_leash_log_ms.compare_exchange_strong(last_lb, now_lb))
                    TC_LOG_INFO("playerbot.v2", "[leash] {} drops entry {}: {:.0f} y from {}, not on the group (reach {:.0f} y)",
                                snapshot.name(), tgt->entry, std::hypot(tgt->x - ctl_member->x, tgt->y - ctl_member->y), ctl_member->name, reach);
                return;
            }
        }
    }

    // Don't override combat-state actions — the primary InCombat dispatch is
    // already running the APL.
    if (ai.state() == BotState::InCombat) return;

    // A companion follows its owner, the human it came with, even when a bot holds the lead (the owner's GetGroupMaster).
    // While the owner is offline or on another map (a taxi, a zone-in) it falls back to the group's leader.
    ObjectGuid leader = group.leader();
    auto const* members = group.members();
    if (!members) return;
    if (snapshot.is_companion_with_owner())
        for (auto const& m : *members)
            if (m.guid == snapshot.raw().owner_guid && m.online && m.map_id == snapshot.map_id())
            { leader = m.guid; break; }
    if (leader.IsEmpty() || leader == snapshot.raw().guid) return;

    GroupMemberSummary const* leader_member = nullptr;
    for (auto const& m : *members)
        if (m.guid == leader && m.online) { leader_member = &m; break; }
    if (!leader_member) return;

    if (leader_member->map_id != snapshot.map_id()) return;

    // A player on a flight is not followed on the ground: the bots by him board it with him and the others are brought to him when
    // he lands (Fleet/OwnerCatchUp.h, the module's world tick). Chasing him mounted under the route ran them through every camp below it.
    if (leader_member->in_flight && !leader_member->is_bot && Services::Config().behavior_fly_with_owner())
    {
        if (snapshot.raw().movement.is_moving && !snapshot.raw().movement.is_in_taxi)
            emit.stop_movement(/*clear_generators=*/true);   // the follow generator would re-aim at him
        ai.set_last_rule_fired("ingroup:owner_in_flight");
        return;
    }

    // Nor a player on a ship or zeppelin: his x,y is a moving deck over the sea (11-08, Orgrimmar -> Undercity: three bots tried to walk to
    // him across the water). The bots by him ride with him and step off with him, the others are brought to him when he steps off
    // (PlayerbotV2.cpp DriveRideWithOwner). A bot on the deck keeps still: a passenger's walk is relative to the moving deck.
    if (!leader_member->is_bot && !leader_member->ship_guid.IsEmpty() && Services::Config().behavior_ride_with_owner())
    {
        if (snapshot.raw().movement.is_moving)
            emit.stop_movement(/*clear_generators=*/true);
        ai.set_last_rule_fired(snapshot.on_transport() ? "ingroup:rides_with_owner" : "ingroup:owner_on_ship");
        return;
    }

    // Bot's current world position — needed for both anchor sanity checks
    // and the follow-recall distance test further below. Computed once and
    // reused so the anchor-on-tank fallback can compare distances.
    float bx, by, bz;
    snapshot.position(bx, by, bz);

    // Off-mesh crossing commitment — honored FIRST, before any anchor/assist/follow.
    // A follower mid-jump over the Gap-1 void that gets routed to the InGroup layer
    // (assist/follow alongside the primary state) must COMPLETE the hop, not chase a
    // target across the gap — chasing replaces the off-mesh spline and strands it on
    // the off-mesh poly (NoPath thereafter; observed live 2026-06-26). Shared with
    // the idle/InCombat paths so the crossing is uninterruptible in every state.
    if (ai.dungeon_active() &&
        DungeonHonorCross(snapshot, ai, emit, snapshot.published_at_ms()))
        return;

    // Anchor selection:
    //   * Healer (always): stick to the tank. Being out of range to heal
    //     is the most common cause of group wipes from bot positioning.
    //   * DPS in dungeon-run mode: also stick to the tank. The tank drives
    //     the pull cadence and the pull location; DPS following the human
    //     leader instead would leave them out of melee on the wrong mob
    //     when the leader wanders. Outside dungeon-run mode DPS stays on
    //     the leader so /follow during travel still works.
    //   * Otherwise: anchor on the group leader.
    GroupMemberSummary const* anchor = leader_member;
    Role const my_role = ai.effective_role(snapshot);
    // While the owner leads (his "follow" order, or a companion's small party in a raid map;
    // BotAI::dungeon_autopilot_suspended) every role, healers too, follows the owner: the tank
    // is following him as well, and chasing the tank was the MC 2026-10-06 "ignored my follow".
    bool const anchor_on_tank =
        !(snapshot.is_companion_with_owner() && ai.dungeon_autopilot_suspended()) &&
        (my_role == Role::Healer ||
         (my_role == Role::Dps && ai.dungeon_active()));
    if (anchor_on_tank)
    {
        // A raid healer anchors on its heal_focus tank (see GroupAnchorTank).
        if (auto const* t = GroupAnchorTank(snapshot, ai, group))
        {
            if (t->online && t->map_id == snapshot.map_id() && t->guid != snapshot.raw().guid)
            {
                // Sanity gate: only commit to anchoring on the tank when the
                // tank is plausibly reachable. Same-map alone isn't enough —
                // a tank that zoned ahead through a portal we haven't crossed
                // yet, or one that ran far ahead through a door, leaves the
                // bot pathfinding through walls because the navmesh can't
                // reach the tank's tile. Heuristic: tank within 60yd OR not
                // far behind the leader. If both fail, anchor on the leader
                // instead — leader-anchor is the safe default that always
                // produces sane follow paths. Observed 2026-05-15 after
                // `;all run`: 3 bots wall-ran trying to reach a tank that
                // had already pulled into the next room.
                const float tank_dist = DistanceXY(bx, by, t->x, t->y);
                const float lead_dist = DistanceXY(bx, by,
                                                    leader_member->x,
                                                    leader_member->y);
                if (tank_dist <= 60.0f || tank_dist <= lead_dist + 20.0f)
                    anchor = t;
            }
        }
    }

    // Stranded-member recovery runs BEFORE the per-state rejoin: a follower that
    // respawned at the far entrance graveyard (or wedged on a disconnected poly)
    // cannot traverse back on foot and would otherwise oscillate the ingroup rejoin
    // below forever while the group holds for it (the sole-rezzer-healer harbor
    // stall, 2026-06-27). Relocate it onto the group so it can rejoin / rez.
    if (ai.dungeon_active() &&
        DungeonRecoverStrandedFollower(snapshot, ai, group, emit,
                                       snapshot.published_at_ms()))
        return;

    // Converge-to-fight: a DPS lagging in the 37-40y dead-band while the tank
    // is soloing a mob it cannot finish is walked into the fight (off-mesh
    // aware) so the assist below engages the tank's victim. Runs after strand
    // recovery and before the >40y rejoin (which only fires past the dead-band).
    if (RaidDutyTankEngage(snapshot, ai, emit, snapshot.published_at_ms()))
        return;
    if (ai.dungeon_active() &&
        DungeonConvergeToFight(snapshot, ai, group, emit,
                               snapshot.published_at_ms()))
        return;

    // ── Dungeon cohesion: rejoin a far tank BEFORE fighting ──────────────
    // A follower stranded far from the tank — the Gap-1 mouth case, where the
    // only visible enemies are ACROSS the offmesh bridge — must CROSS to the tank
    // first. Assisting a target it can't reach pins it at the chokepoint while
    // the tank fights the boss-room trash UNHEALED and the group wipes (observed
    // live 2026-06-26 Deadmines: healer + 2 DPS stuck at the mouth running
    // assist/opener across the bridge, tank died, 3/5 dead). Cross now; the assist
    // and follow below resume the instant the bot has rejoined the tank. Tank-
    // distance (not anchor) so it fires even when the >60y anchor sanity gate
    // above fell back to the leader. The tank's heal-leash holds it for us.
    if (ai.dungeon_active() && my_role != Role::Tank)
    {
        // Raid healer: its heal_focus tank, so this rejoin never pulls it off
        // the spare tank rule (5c) walked it to.
        if (auto const* tk = GroupAnchorTank(snapshot, ai, group))
        {
            if (tk->online && tk->is_alive && tk->guid != snapshot.raw().guid &&
                tk->map_id == snapshot.map_id() &&
                DistanceXY(bx, by, tk->x, tk->y) > 40.0f)
            {
                if (!snapshot.victim().IsEmpty())
                    emit.stop_attack();
                // Emit the SAME off-mesh-aware far-vertex goal the idle
                // regroup-cross (and in-combat rule 0b) emits — NOT a plain
                // move_to(tank). When a follower oscillates between Idle
                // (regroup → far vertex of the Gap-1 bridge) and InGroup
                // (this rejoin), two DIFFERENT goal-keys reach the spline:
                // move_to dedups on the goal, so each state-switch restarts
                // the spline at the bridge mouth and the follower never
                // completes the off-mesh hop — the observed Gap-1 flip-flop
                // (healer + hunter pinned at -214.7,-490.7 alternating
                // ingroup:dungeon_rejoin_tank / idle regroup-cross, never
                // crossing). DungeonStepTowardTank returns the bridge's SOLID
                // far vertex when the step crosses the off-mesh, so this
                // rejoin and the idle cross now share one stable goal-key and
                // the commit-crossing completes. Falls back to a plain
                // move_to(tank) only when no NORMAL path step resolves
                // (same-stratum follow, no bridge to commit to).
                {
                    float rx, ry, rz; bool roff = false;
                    if (DungeonStepTowardTank(snapshot.raw().guid,
                                              tk->x, tk->y, tk->z, 45.0f,
                                              rx, ry, rz, roff))
                    {
                        // When the step crosses an off-mesh bridge, COMMIT: store
                        // the far vertex so DungeonHonorCross (run first in every
                        // state) keeps re-asserting it and the jump completes even
                        // if combat would otherwise interrupt it mid-span.
                        if (roff)
                            ai.set_dungeon_cross(rx, ry, rz,
                                                 snapshot.published_at_ms() + 12000);
                        emit.move_to(rx, ry, rz, /*run=*/true);
                    }
                    // Direct path to the tank is gap-broken (NoPath). Before the
                    // doomed beeline, try to rejoin ALONG the crumb-route — the
                    // tank reached its spot by route-following down a descent the
                    // straight line NoPaths (RFC Adarogg pit); the follower must
                    // descend the SAME navigable crumbs, not beeline across the
                    // gap and strand the whole group on the cohesion gate.
                    else if (DungeonRouteStepTowardPos(snapshot,
                                                       tk->x, tk->y, tk->z, 45.0f,
                                                       rx, ry, rz))
                    {
                        emit.move_to(rx, ry, rz, /*run=*/true);
                        ai.set_last_rule_fired("ingroup:dungeon_rejoin_tank_route");
                        return;
                    }
                    else
                        emit.move_to(tk->x, tk->y, tk->z, /*run=*/true);
                }
                ai.set_last_rule_fired("ingroup:dungeon_rejoin_tank");
                return;
            }
        }
    }

    // Combat assist: if a group member is in combat with a target and we
    // don't have one ourselves, engage their target. Skull-marked target
    // beats both — DPS coordination convention is "kill skull first."
    // Tanks pick their own targets via threat; they ignore assist but DO
    // honor skull (so they can pull/taunt the focus mob).
    //
    // Full assist chain: focus, skull, leader victim, any in-combat member's victim. The owner's stance and orders gate it, not
    // the bot's personality.
    // Told to stay: keep the spot rather than run to the group's target, as a Passive bot would (it still fights back
    // when attacked).
    const bool staying = ai.holding_position(snapshot.published_at_ms());   // Stay or Hold ("defensive": fights only what attacks)
    // The bot's random personality does not decide who joins a fight: the owner's stance and orders are the only input here.
    // Owner "passive": no assist on the owner's, the skull's or a member's target; only a mob already on a group member is engaged
    // (the defend-member step below). Stance Passive, or Auto in an instance a human leads (BotAI::passive_now); an attack / pull
    // order opens a 15 s window in which it fights.
    // Led by a player (his companion, or a world bot he invited) and no stance picked: Passive, in the open world too.
    const bool auto_passive = (snapshot.is_in_instance() && !group.leader_is_bot()) || snapshot.is_led_by_player();
    const bool owner_passive_stance = ai.passive_now(auto_passive);
    // The player is fighting: his bots fight with him (the owner, 2026-10-27: "bots should always fight if you're fighting"). Passive
    // only keeps a bot from STARTING a fight; once the human leader is in combat, the leader's target and every mob on a group member
    // are the group's fight, joined without the passive leash or a line of sight from where the bot stands (the in-combat
    // positioning walks it to a spot that sees the mob).
    const bool leader_fighting = !leader_member->is_bot && leader_member->in_combat && leader_member->is_alive;
    // The group's fight releases Passive as the player's own does (GroupControl::PassiveHolds): a member by him in combat. A caster
    // who lets his tank pull is not in combat himself.
    bool group_fighting = false;
    for (auto const& m : *members)
    {
        if (!m.online || !m.is_alive || !m.in_combat || m.map_id != snapshot.map_id() || m.guid == snapshot.raw().guid) continue;
        if (std::hypot(m.x - leader_member->x, m.y - leader_member->y) <= GroupControl::kFightJoinYd) { group_fighting = true; break; }
    }
    const bool owner_passive = GroupControl::PassiveHolds(owner_passive_stance, !leader_member->is_bot, leader_member->is_alive,
                                                          leader_member->in_combat, group_fighting);
    const bool group_fight_on = !GroupControl::PassiveHolds(true, !leader_member->is_bot, leader_member->is_alive,
                                                            leader_member->in_combat, group_fighting);
    // A Passive bot inside an order window (every Attack / Pull / assist order opens 15 s of it) joins the owner's own target and
    // nothing wider: no skull, no kill focus, no other member's victim, no creature outside its scan or far from the owner (the owner,
    // Deadmines 2026-10-15: "the bots were attacking willy-nilly, hitting things that weren't close to me"; with Attack pressed every few
    // seconds the window never closed and the whole assist chain ran, the tank opening on mobs 32-43 y away).
    // Once the group fights, the window limits nothing: the owner pressing Attack through a pull kept his rogue and cat druid on his own
    // target only, and a warlock's is a spell's, not a melee victim; they stood beside him (UBRS 2026-10-28 22:10, Attack every 4-8 s).
    const bool passive_kind = ai.owner_passive() ||
                              (ai.owner_stance() == BotAI::OwnerStance::Auto && auto_passive);
    const bool window_limited = !owner_passive_stance && passive_kind && !group_fight_on;
    // A Passive bot the group's fight released joins only that fight (GroupControl::LeaderFightMayJoin): his target, a mob near him,
    // or a mob on a member by him that is fighting that member there. A mob on the bot itself is always fought.
    const bool fight_released = passive_kind && group_fight_on;
    auto fight_join_ok = [&](ObjectGuid mob) -> bool
    {
        if (!fight_released || mob.IsEmpty())
            return true;
        bool const leader_target = mob == leader_member->victim || (leader_member->is_casting && mob == leader_member->casting_target);
        NearbyUnit const* u = nullptr;
        for (auto const& e : snapshot.raw().combat.attackers)
            if (e.guid == mob) { u = &e; break; }
        if (!u)
            for (auto const& e : snapshot.raw().combat.nearby_enemies)
                if (e.guid == mob) { u = &e; break; }
        if (!u)
            return leader_target;
        if (u->victim == snapshot.raw().guid)
            return true;
        float const mob_to_leader = std::hypot(u->x - leader_member->x, u->y - leader_member->y);
        float member_to_leader = -1.f, mob_to_member = -1.f;
        for (auto const& m : *members)
        {
            if (!m.online || m.map_id != snapshot.map_id() || u->victim.IsEmpty()) continue;
            bool const on_member = m.guid == u->victim;
            bool const on_pet = m.pet_alive && m.pet_guid == u->victim;
            if (!on_member && !on_pet) continue;
            float const vx = on_member ? m.x : m.pet_x, vy = on_member ? m.y : m.pet_y;
            member_to_leader = std::hypot(vx - leader_member->x, vy - leader_member->y);
            mob_to_member = std::hypot(u->x - vx, u->y - vy);
            break;
        }
        return GroupControl::LeaderFightMayJoin(leader_target, mob_to_leader, member_to_leader, mob_to_member);
    };
    // A pet never starts a pull: an aggressive pet pulls every mob around it while the group stands still (Deadmines,
    // 2026-10-13). Bot pets are Passive and fight only the bot's own target (State_InCombat orders the pet onto the bot's
    // victim). Between fights any other react state (a manual "pet defensive" / "pet aggressive", a server default) is put
    // back to Passive.
    if (!snapshot.in_combat() && !snapshot.recently_in_combat(3000) && snapshot.has_pet() && snapshot.pet_react() != 0)
        emit.pet_set_react_state(0 /*REACT_PASSIVE*/);
    // Combat-in-progress signal: ANY same-map group member currently in combat. Symptom-driven and more reliable than the
    // dungeon-active flag (which can be inconsistent across the squad when `;all run` doesn't land on every bot).
    bool any_member_in_combat = false;
    for (auto const& m : *members)
    {
        if (!m.online || m.map_id != snapshot.map_id()) continue;
        if (m.guid == snapshot.raw().guid) continue;     // skip self
        if (m.in_combat) { any_member_in_combat = true; break; }
    }
    const bool engage_allowed =
        owner_passive || ai.owner_stance() == BotAI::OwnerStance::Aggressive || !staying;
    // Not in a battleground: BgDispatch (State_Idle) and the in-combat BG layer own movement and targets there. This chain trusts
    // creature victims at any range, so a teammate's fight with a mob pulled a bot off its ordered march (AV tower runs).
    // In the group's fight the target is GroupFightPick's (State_InCombat): this chain's Passive, leash and tank-first gates are its
    // out-of-combat half, and a second start_attack here on the same tick fought the pick.
    if (engage_allowed && !snapshot.in_battleground() && !ai.group_fight())
    {
    if (snapshot.victim().IsEmpty())
    {
        ObjectGuid assist_target;
        // Owner-pinned focus target wins over everything. /focus <name>
        // sets ai.focus_target() so the owner can micromanage which add the
        // bot prioritizes (kicks, dispels, spike damage). Cleared via
        // /focus clear or when the focus dies (drops out of nearby_enemies).
        if (ObjectGuid focus = ai.focus_target(); !focus.IsEmpty())
        {
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
                if (u.guid == focus && u.hp > 0) { assist_target = focus; break; }
            if (assist_target.IsEmpty())
                for (auto const& u : snapshot.raw().combat.attackers)
                    if (u.guid == focus && u.hp > 0) { assist_target = focus; break; }
        }
        // Skull mark wins for everyone (after focus). Only use it if it's
        // actually nearby — out-of-vision skulls are stale.
        //
        // CRITICAL: non-tanks must wait for the tank (or another group
        // member) to be engaged with the skull before opening on it.
        // Skull is "kill priority", NOT "fire at will" — a DPS bot
        // opening on skull before tank engages is the #1 overpull
        // source. Tanks always honor skull (they're the ones who
        // should be pulling it). Observed 2026-05-15: Defensive
        // warlock attacked skull immediately on mark, before tank
        // had any threat. Causes loss-of-aggro wipes.
        if (assist_target.IsEmpty() && !owner_passive && !window_limited)
        {
            if (ObjectGuid skull = group.skull_target(); !skull.IsEmpty())
            {
                const bool is_tank = ai.effective_role(snapshot) == Role::Tank;
                bool skull_engaged_by_member = false;
                if (!is_tank)
                {
                    for (auto const& m : *members)
                    {
                        if (!m.online || m.map_id != snapshot.map_id()) continue;
                        if (m.guid == snapshot.raw().guid) continue;
                        if (m.in_combat && m.victim == skull)
                        { skull_engaged_by_member = true; break; }
                    }
                }
                if (is_tank || skull_engaged_by_member)
                {
                    // Skip dead skull — corpses linger in nearby_enemies and
                    // we'd start_attack a body until raid leader re-marks.
                    // Nor a bot leader's skull on a mob nobody fights: that is
                    // a leftover of the last fight, not a pull call (the tank's
                    // pull goes through idle:tank_pull_next and its gate;
                    // GroupSnapshotView::leader_is_bot).
                    for (auto const& u : snapshot.raw().combat.nearby_enemies)
                        if (u.guid == skull && u.hp > 0)
                        {
                            // MarksNeedOrder: an idle skull is a plan, the owner's order starts the pull.
                            if (u.unit_in_combat || !u.victim.IsEmpty() ||
                                (!group.leader_is_bot() && MarkGate::MayEngage(snapshot, ai, &group, skull, &u)))
                                assist_target = skull;
                            break;
                        }
                    if (assist_target.IsEmpty())
                        for (auto const& u : snapshot.raw().combat.attackers)
                            if (u.guid == skull && u.hp > 0) { assist_target = skull; break; }
                    if (!fight_join_ok(assist_target))
                        assist_target.Clear();
                }
            }
        }
        // Group plan kill focus (PveGroupCoordinator: a dungeon script's
        // priority add, or in raids the one engaged mob every DPS burns).
        // It names only mobs already fighting the group, so it outranks the
        // leader's victim: a raid DPS joins the coordinated target instead
        // of whatever the (often human, non-tank) leader is hitting.
        if (assist_target.IsEmpty() && !owner_passive && !window_limited && ai.effective_role(snapshot) != Role::Tank)
        {
            auto const& kf = snapshot.raw().pve_order;
            if (kf.active && !kf.kill_focus.IsEmpty())
                for (auto const& u : snapshot.raw().combat.nearby_enemies)
                    if (u.guid == kf.kill_focus && u.hp > 0) { assist_target = kf.kill_focus; break; }
            if (!fight_join_ok(assist_target))
                assist_target.Clear();
        }
        // Non-tanks fall back to leader's / any in-combat member's victim.
        // The copied guid MUST resolve against our own hostile lists
        // (nearby_enemies / attackers — both IsValidAttackTarget-gated at
        // build time) before we engage it. member.victim is published
        // verbatim from Unit::GetVictim(), and a member wedged on an
        // INVALID victim (2026-06-11 Deadmines: healer stuck swinging at
        // a groupmate's hunter pet) would otherwise propagate that victim
        // to the whole party — bots visibly "fighting each other" while
        // every cast rejects BAD_TARGETS and the run stalls.
        // A human-led group's tank takes the owner's target too: the autopilot no longer pulls for it (BotAI::human_lead_hold), so
        // the owner's own pull is how the tank gets into the fight.
        const bool tank_follows_owner_pull = ai.effective_role(snapshot) == Role::Tank && ai.human_lead_hold() && !group.leader_is_bot();
        if (assist_target.IsEmpty() && !owner_passive && (ai.effective_role(snapshot) != Role::Tank || tank_follows_owner_pull))
        {
            auto resolves_hostile = [&](ObjectGuid v) -> bool
            {
                if (v.IsEmpty()) return false;
                for (auto const& u : snapshot.raw().combat.nearby_enemies)
                    if (u.guid == v && u.hp > 0) return true;
                for (auto const& u : snapshot.raw().combat.attackers)
                    if (u.guid == v && u.hp > 0) return true;
                // Creature-typed victims are trusted even when outside our
                // own capped 16-entry/40y scan (big pulls evict, range/LoS
                // hide) — refusing them left bots standing idle mid-fight
                // (user re-report 2026-06-11 Stockades). The friendly-fire
                // wedge class this guard exists for involved Player/Pet
                // guids only, and start_attack's IsValidAttackTarget gate
                // refuses anything that slips through at engage time.
                // In a raid only what the scan holds: a victim outside the scan has no threat figures (ThreatHold reads none and lets it through), so a
                // raid DPS 87 y out at the lair's cliff edge opened on Onyxia the moment the tank was ordered, before any hold applied
                // (onyxia_full n57 run 1: Mcshadb first, the lair whelps joined the pull at 12 s). It opens once the mob is in its scan.
                return v.IsCreature() && !snapshot.is_in_raid();
            };
            if (window_limited)
            {
                // The owner's own target only, in the bot's scan and within 25 y of the owner.
                for (auto const& u : snapshot.raw().combat.nearby_enemies)
                {
                    if (u.guid != leader_member->victim || u.hp <= 0) continue;
                    float const lx = u.x - leader_member->x, ly = u.y - leader_member->y;
                    if (lx * lx + ly * ly <= 25.f * 25.f) assist_target = u.guid;
                    break;
                }
            }
            // Only members on this bot's own map: a raider out of the instance (a corpse run, a graveyard) fights what it meets there, and
            // copying it set the stray bots fighting each other's mobs in Eastvale while the raid lay dead in Onyxia's Lair, so a member of
            // the group stayed in combat and the owner could not release (n58 live 2026-10-15 11:28, Amastorion).
            else if (leader_member->map_id == snapshot.map_id() && resolves_hostile(leader_member->victim))
                assist_target = leader_member->victim;
            // A caster leader has no melee victim: the target of the spell he is casting is his target.
            else if (leader_fighting && leader_member->map_id == snapshot.map_id() && leader_member->is_casting &&
                     resolves_hostile(leader_member->casting_target))
                assist_target = leader_member->casting_target;
            if (assist_target.IsEmpty() && !window_limited)
                for (auto const& m : *members)
                    if (m.online && m.map_id == snapshot.map_id() && m.in_combat && resolves_hostile(m.victim) && fight_join_ok(m.victim))
                    { assist_target = m.victim; break; }
        }
        // Defend-member engage (ALL roles, tanks very much included): a mob
        // whose victim is a group member is a pull in progress even when NO
        // member's own victim pointer is set — ranged/spell pulls never call
        // Unit::Attack on the puller, so a hunter (e.g. the user pulling
        // manually with Auto Shot) has GetVictim()==null and the whole
        // member-victim chain above sees nothing while the mob runs the
        // puller down (user report 2026-06-11 Stockades: "even if I start
        // pulling mobs the whole group doesn't react"). The enemy scan
        // publishes each mob's victim; engage the nearest visible mob
        // that's attacking one of ours.
        if (assist_target.IsEmpty())
        {
            auto const& dpo = snapshot.raw().pve_order;
            const bool spare_duty_tank = dpo.active && dpo.tank_duty != 0 && dpo.main_tank != snapshot.raw().guid;
            float sx, sy, sz; snapshot.position(sx, sy, sz);
            float best_dsq = std::numeric_limits<float>::max();
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
            {
                if (u.hp <= 0 || u.victim.IsEmpty() || (!u.in_los && !group_fight_on)) continue;
                bool attacks_member = (u.victim == snapshot.raw().guid);
                GroupMemberSummary const* victim_member = nullptr;
                if (!attacks_member)
                    for (auto const& m : *members)
                        if (m.guid == u.victim) { attacks_member = true; victim_member = &m; break; }
                if (!attacks_member) continue;
                if (!fight_join_ok(u.guid)) continue;
                // Leash (open world and instance trash, never a boss): a Passive bot defends a mob on someone else only while it is within PassiveLeashYd of the leader
                // or of the member it is on (a healer 30 y back is still defended), and that member is itself near the leader. It
                // never runs off to a pack the group has not pulled. A mob on the bot itself is always fought.
                if ((owner_passive || window_limited) && snapshot.is_led_by_player() &&
                    (!snapshot.is_in_instance() || (!ai.dungeon_active() && !Classic::IsBossUnit(u))))
                {
                    float const leash = Services::Config().behavior_passive_leash_yd();
                    float near_d = std::hypot(u.x - leader_member->x, u.y - leader_member->y);
                    if (victim_member && victim_member->map_id == snapshot.map_id() &&
                        std::hypot(victim_member->x - leader_member->x, victim_member->y - leader_member->y) <= 2.0f * leash)
                        near_d = std::min(near_d, std::hypot(u.x - victim_member->x, u.y - victim_member->y));
                    if (!GroupControl::PassiveMayDefend(u.victim == snapshot.raw().guid, near_d, leash)) continue;
                }
                // Passive, tank first (playtest 2026-10-14): a mob on another member is the tank's to pick up; a damage bot joins once the
                // tank has its opening lead, or at once when the member it is on is under 70 % (a healer waits for nothing here: it heals).
                if (owner_passive_stance && victim_member && ai.effective_role(snapshot) == Role::Dps && !u.is_player)
                    if (GroupMemberSummary const* gt = group.tank();
                        gt && gt->is_alive && gt->online && gt->map_id == snapshot.map_id() && gt->guid != snapshot.raw().guid &&
                        victim_member->hp_pct() >= 70 &&
                        // Bounded: the threat hold releases on the tank's lead or after its opening wait (a party's 6 s from the tank's first
                        // damage), so a tank that never reaches the lead (Deadmines 2026-10-15: Disc on the Overseer, the damage bots
                        // stood for the whole fight) does not hold the group out for good.
                        ThreatHoldActive(ai, snapshot, u.guid, &group))
                        continue;
                // A spare duty tank (the plan gave it tank duty but the main tank holds the pull) stays near the
                // pack until a mob hits it itself or the plan assigns it one; it does not run at every mob that
                // hits a member before the fight has started.
                if (spare_duty_tank && u.victim != snapshot.raw().guid) continue;
                const float ddx = u.x - sx, ddy = u.y - sy, ddz = u.z - sz;
                const float dsq = ddx*ddx + ddy*ddy + ddz*ddz;
                if (dsq < best_dsq) { best_dsq = dsq; assist_target = u.guid; }
            }
        }
        // Defending stands a bot up (playtest, Deadmines 2026-10-15: "the mage is getting attacked and the other three guys are just
        // sitting drinking"). A meal or drink is only broken by the bot's own damage or by standing, so a seated bot whose group
        // member has a mob on it gets to its feet at once, ahead of the tank-first wait and whether or not it has the mob in sight.
        if ((Classic::IsEating(snapshot) || Classic::IsDrinking(snapshot)) && snapshot.is_alive())
        {
            float ex, ey, ez; snapshot.position(ex, ey, ez);
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
            {
                if (u.hp <= 0 || u.victim.IsEmpty()) continue;
                bool on_member = (u.victim == snapshot.raw().guid);
                if (!on_member)
                    for (auto const& m : *members)
                        if (m.guid == u.victim && m.online && m.is_alive) { on_member = true; break; }
                if (!on_member) continue;
                float const edx = u.x - ex, edy = u.y - ey;
                if (edx * edx + edy * edy > 40.f * 40.f) continue;
                emit.stand();
                ai.set_last_rule_fired("ingroup:stand_to_defend");
                return;
            }
        }
        // Aggressive stance: any hostile close to the owner is attacked, whether or not anyone is fighting yet.
        // In a party the owner told it to go: the tank-first hold below does not cap this pick (a raid keeps it).
        bool aggressive_pick = false;
        if (assist_target.IsEmpty() && ai.owner_stance() == BotAI::OwnerStance::Aggressive && !group.leader_is_bot())
        {
            float best_dsq = std::numeric_limits<float>::max();
            // Measured from the leader (not the bot), so a bot never leaves the group to reach a mob.
            const float kRadius = Services::Config().behavior_aggressive_radius_yd();
            const float kR2 = kRadius * kRadius;
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
            {
                if (u.hp <= 0 || !u.in_los || u.no_xp_kill || u.is_pacified || u.untargetable) continue;
                const float ox = u.x - leader_member->x, oy = u.y - leader_member->y;
                const float d2 = ox * ox + oy * oy;
                if (d2 <= kR2 && d2 < best_dsq) { best_dsq = d2; assist_target = u.guid; }
            }
            aggressive_pick = !assist_target.IsEmpty() && !snapshot.is_in_raid();
        }
        // Threat discipline (State_InCombat.cpp ThreatHoldActive): a non-tank
        // does not open on the tank's mob until the tank has its opening lead.
        if (!assist_target.IsEmpty() && assist_target != snapshot.victim() &&
            (aggressive_pick || !ThreatHoldActive(ai, snapshot, assist_target, &group)))
        {
            emit.start_attack(assist_target);
            ai.set_last_rule_fired("ingroup:assist");
            // Why a bot opened on its own (playtest, Deadmines 2026-10-15: "they run in as if not passive"): the stance, the order window
            // and the mob's state at the moment of the opener, once a second per bot at most.
            static std::atomic<uint32> s_assist_why_ms{ 0 };
            uint32 const now_aw = snapshot.published_at_ms();
            uint32 last_aw = s_assist_why_ms.load(std::memory_order_relaxed);
            if (now_aw - last_aw >= 500u && s_assist_why_ms.compare_exchange_strong(last_aw, now_aw))
            {
                NearbyUnit const* au = nullptr;
                for (auto const& u : snapshot.raw().combat.nearby_enemies)
                    if (u.guid == assist_target) { au = &u; break; }
                float ax = 0.f, ay = 0.f, az = 0.f; snapshot.position(ax, ay, az);
                float const ad = au ? std::sqrt((au->x - ax) * (au->x - ax) + (au->y - ay) * (au->y - ay)) : -1.f;
                TC_LOG_INFO("playerbot.v2",
                    "[assist_why] {} opens on entry {} ({:.0f}y): stance {} passive_now {} order_window {} follow_order {} mob in_combat {} mob_victim_kind {} tank_threat {}",
                    snapshot.name(), au ? au->entry : 0u, ad, int(ai.owner_stance()), owner_passive ? 1 : 0,
                    ai.owner_order_window_open() ? 1 : 0, ai.owner_follow_order() ? 1 : 0, au && au->unit_in_combat ? 1 : 0,
                    au ? int(au->victim_kind) : -1, au ? au->tank_threat : -1);
            }
        }
    }
    else
    {
        // Bot already has a victim. Honor focus pivots first, then skull
        // pivots. Focus is owner-pinned and outranks the raid leader's
        // skull mark; both override the bot's pre-engaged target.
        if (ObjectGuid focus = ai.focus_target();
            !focus.IsEmpty() && focus != snapshot.victim())
        {
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
                if (u.guid == focus && u.hp > 0)
                {
                    emit.start_attack(focus);
                    ai.set_last_rule_fired("ingroup:focus_pivot");
                    return;
                }
        }
        {
            if (ObjectGuid skull = group.skull_target();
                !skull.IsEmpty() && skull != snapshot.victim())
            {
                // Same tank-or-member-engaged gate as the open-engage path.
                // Pivoting off the current target onto skull before anyone
                // has aggro on it is just as bad as opening on it cold.
                const bool is_tank = ai.effective_role(snapshot) == Role::Tank;
                bool skull_engaged_by_member = false;
                if (!is_tank)
                {
                    for (auto const& m : *members)
                    {
                        if (!m.online || m.map_id != snapshot.map_id()) continue;
                        if (m.guid == snapshot.raw().guid) continue;
                        if (m.in_combat && m.victim == skull)
                        { skull_engaged_by_member = true; break; }
                    }
                }
                if (is_tank || skull_engaged_by_member)
                {
                    for (auto const& u : snapshot.raw().combat.nearby_enemies)
                        if (u.guid == skull && u.hp > 0)
                        {
                            // A bot leader's leftover skull on an idle mob is
                            // no pivot target (see the open-engage path).
                            if (u.unit_in_combat || !u.victim.IsEmpty() ||
                                (!group.leader_is_bot() && MarkGate::MayEngage(snapshot, ai, &group, skull, &u)))
                            {
                                emit.start_attack(skull);
                                ai.set_last_rule_fired("ingroup:skull_pivot");
                            }
                            break;
                        }
                }
            }
        }
    }
    }

    // bx/by/bz already computed above at anchor-selection time.
    (void)bz;
    const float dist = DistanceXY(bx, by, anchor->x, anchor->y);

    // Anchor has moved past the slack distance — issue a follow. The follow
    // movement generator is sticky, so re-issuing every tick is wasteful;
    // we only emit when the gap exceeds the bot's preferred follow distance.
    // Healers/casters trail further so they don't eat melee cleaves; melee /
    // tanks stack tight to be in their threat range. Owner can override the
    // role-based default via /follow_distance N — pinned value wins (lets
    // the owner micro-position around encounter mechanics).
    // The effective role, so a `.bot add <class> healer` companion trails like a healer even when its spec reads DPS.
    Role const follow_role = ai.effective_role(snapshot);
    const bool ranged = follow_role == Role::Healer
                     || (follow_role == Role::Dps && !IsMeleeClass(snapshot.cls(), snapshot.spec()));
    float kFollowDistance = ranged ? 9.0f : 3.5f;
    float kRecallSlack    = ranged ? 13.0f : 6.0f;
    if (ai.follow_distance() > 0.f)
    {
        kFollowDistance = ai.follow_distance();
        // Recall slack scales proportionally — pin 8yd → ~12yd recall, etc.
        kRecallSlack = kFollowDistance * 1.5f;
    }
    else
    {
        // Unpinned: every bot trails at its own stable distance (+-20%), so a group does not stand in one tidy line.
        kFollowDistance += StableJitter(snapshot.raw().guid.GetCounter(), 41, 0.2f * kFollowDistance);
    }
    // M-P1a: follow into the bot's FORMATION SLOT, not generically
    // behind the anchor. Without the offset the whole squad piles onto
    // a single point behind the leader (clumping = AoE bait + obvious
    // bot tell). ComputeFormationOffset yields the per-slot
    // distance+angle for the owner-configured formation; Free/slot-0
    // collapses to a plain follow so unconfigured squads are unchanged.
    // One formation system at a time: with the owner formation on (Playerbot.Behavior.Formation) the slot ring below is the
    // formation, and the plain follow is the rest; the legacy fixed 12 y circle / 8 y spread would otherwise take over the moment
    // any member fights and send the group out of the owner's heal / buff range.
    FormationOffset formation_offset = ComputeFormationOffset(
        Services::Config().behavior_formation() ? FormationType::Free : ai.effective_formation_type(snapshot.in_combat()),
        ai.formation_slot(),
        kFollowDistance);
    // Owner formation (circle / arrow / line / column + spacing preset, Playerbot.Behavior.Formation): a stable
    // slot among the group's bots, sorted by duty row (tanks front, healers back) then guid, followed relative
    // to the owner and rotating with his facing (the follow generator re-aims as he turns). Only this
    // out-of-combat follow: a group member in combat, an active raid avoid circle near the owner, or the
    // dungeon autopilot (dungeon_run_owns_movement below) leave the plain follow / the raid brain in charge.
    bool formation_slot_active = false;
    float slot_x = 0.f, slot_y = 0.f;
    uint32 slot_index = 0, slot_count = 0;
    FormationType slot_type = FormationType::Free;
    uint32 const formation_now_ms = snapshot.published_at_ms();
    // A slot the bot could not reach (no progress for 3 s) puts it on the plain follow behind the owner for a while.
    bool const formation_fallback = ai.formation_track().fallback_until_ms > formation_now_ms;
    if (formation_fallback)
        formation_offset = ComputeFormationOffset(FormationType::Free, 0, kFollowDistance);
    if (Services::Config().behavior_formation() && !any_member_in_combat && anchor == leader_member && !formation_fallback)
    {
        FormationType ftype = ai.formation_type();
        if (ftype == FormationType::Spread) ftype = FormationType::Circle;   // the old 8 y ring is a circle now
        if (ftype == FormationType::Tight)  ftype = FormationType::Circle;
        if (ftype != FormationType::Free)
        {
            // Slot spacing from the owner's spacing preset / distance (4 y "close" -> 3 y between neighbours); the raid combat
            // spacing keeps reading spacing_yards() unchanged.
            float spacing = std::clamp(ai.spacing_yards() * 0.75f, 1.8f, 9.0f);
            if (ai.formation_type() == FormationType::Tight) spacing = std::min(spacing, 1.9f);   // "tight" type caps at the tight preset
            auto rank_of = [](GroupMemberSummary const& m) -> uint8
            {
                if (m.role == Role::Tank) return uint8(FormationRank::Tank);
                if (m.role == Role::Healer) return uint8(FormationRank::Healer);
                return uint8(IsMeleeClass(m.cls, m.spec) ? FormationRank::Melee : FormationRank::Ranged);
            };
            uint32 count = 0, index = 0, front_count = 0;
            uint8 const my_rank = [&] { for (auto const& m : *members) if (m.guid == snapshot.raw().guid) return rank_of(m); return uint8(FormationRank::Ranged); }();
            uint64 const my_guid = snapshot.raw().guid.GetCounter();
            for (auto const& m : *members)
            {
                if (!m.is_bot || !m.online || m.map_id != snapshot.map_id() || m.guid == leader) continue;
                uint8 const r = rank_of(m);
                ++count;
                if (r <= uint8(FormationRank::Melee)) ++front_count;
                if (r < my_rank || (r == my_rank && m.guid.GetCounter() < my_guid)) ++index;
            }
            bool circle_near = false;
            if (PveOrder const& po = snapshot.raw().pve_order; po.active)
                circle_near = po.in_avoid_circle(anchor->x, anchor->y, spacing * 2.0f + 6.0f);
            if (count > 0 && !circle_near)
            {
                formation_offset = ComputeOwnerSlotOffset(ftype, index, count, front_count, spacing, my_guid);
                formation_slot_active = true;
                slot_index = index; slot_count = count; slot_type = ftype;
                // The slot in world coordinates from the owner's LIVE position and facing (angle: 0 = ahead of
                // him, pi/2 = his left, the follow generator's convention).
                slot_x = leader_member->x + formation_offset.distance * std::cos(leader_member->o + formation_offset.angle_radians);
                slot_y = leader_member->y + formation_offset.distance * std::sin(leader_member->o + formation_offset.angle_radians);
            }
        }
    }
    // A slot further out than the slack would recall a bot that is standing exactly in it; an owner formation
    // keeps the bot to its slot instead of the wide per-role slack.
    kRecallSlack = formation_slot_active ? std::max(6.0f, formation_offset.distance + 3.0f)
                                         : std::max(kRecallSlack, formation_offset.distance + 4.0f);
    // In a battleground, the BG-objective rules in State_Idle drive
    // movement (node capture, flag carry, role patrol). Following the BG
    // raid leader here would stomp those move_to emits and pile all bots
    // onto whichever player the BG queue auto-assigned as leader, which
    // is the bug behind "bots stay idle at the BG start area". Skip the
    // follow when bg_active so the BG rules win. Combat assist / skull
    // pivots above still fire, and the InCombat dispatch still drives
    // attack rotations once a target is acquired.
    const bool in_active_bg = snapshot.in_battleground() && ai.bg_active();
    // Dungeon-run movement ownership: while dungeon-run mode is Active, the
    // dungeon dispatch in State_Idle owns ALL squad movement — the tank via
    // idle:dungeon_tank_advance/boss_nav/tank_pull_next, and every non-tank
    // via idle:dungeon_regroup_follow_tank (a navmesh-validated move_to that
    // advances on partial paths and CROSSES off-mesh bridges) plus the
    // idle:dungeon_hold cohesion floor. State_InGroup's generic follow must
    // stand down for the WHOLE squad, not just the tank.
    //
    // For the TANK: follow-recall would yank it back to the LFG group leader
    // (often a DPS), fighting tank_advance — the observed
    // `follow_recall <-> tank_waypoint` oscillation that stalls the tank.
    //
    // For NON-TANKS: dispatch_layers (this code) runs AFTER dispatch_primary
    // (State_Idle), so a follow-recall here re-emits MoveFollow every tick and
    // OVERRIDES the regroup move_to that State_Idle just issued. MoveFollow has
    // no partial-path fallback and PINS against an off-mesh bridge — observed
    // live 2026-06-25 at the Deadmines foundry Gap-1 bridge: tank + 1 dps
    // crossed, healer + 2 dps stuck 55y back on ingroup:follow_recall while
    // their dungeon_regroup move_to (which one bot used to cross successfully)
    // was clobbered the very next tick. Generic follow also anchors on the LFG
    // leader rather than the tank, which is wrong for a dungeon. Below the
    // 22y regroup threshold the regroup is silent and idle:dungeon_hold simply
    // holds position — that IS the dungeon cohesion target, so no follow is
    // needed there either. (The dungeon dispatch always re-anchors on g.tank().)
    const bool dungeon_run_owns_movement = ai.dungeon_active();
    // Detour lease: a purposeful idle move (loot/skin a corpse, walk to an
    // in-dungeon quest giver) granted itself a few seconds of slack — honor
    // it instead of yanking the bot back mid-errand (the follow_recall <->
    // move_to_corpse ping-pong starved looting for whole runs). Bounded:
    // a fight in the group or a real separation (>45y) overrides the lease.
    constexpr float kDetourMaxRange = 45.0f;
    const bool detour_leased = ai.detour_active(snapshot.published_at_ms()) &&
                               !any_member_in_combat && !snapshot.in_combat() &&
                               dist < kDetourMaxRange;
    // Owner said stay/hold: keep the spot until follow/resume.
    const bool holding_position = ai.holding_position(snapshot.published_at_ms());
    // While the tank fights, a raid's ranged DPS / healer stands on its formation slot behind it (RaidFormationHold), in its own combat
    // or not. A healer between heals drops out of combat, and the follow below walked it back to its leader beside the boss (238 support
    // scorecard: the shaman in melee reach 102 of 120 s, put back on its 24 y place and followed in again within 10 s, 11 times).
    const bool raid_formation_owns = RaidFormationOwnsMovement(snapshot, ai, group);
    if (raid_formation_owns && (!snapshot.victim().IsEmpty() || ai.effective_role(snapshot) == Role::Healer))
        RaidFormationHold(snapshot, ai, group, emit);
    // An Attack / Pull walk to the ordered mob, or a pull back's approach and retreat, is the idle rules' movement (this layer runs
    // after them): a follow or slot move here would walk the tank back to the owner mid-pull. Bounded to 45 y so a bot that is
    // far from the owner still rejoins.
    const bool order_owns_movement =
        (ai.pull_back_active() ||
         (ai.is_manual_mode(snapshot.published_at_ms()) && ai.owner_command() == BotAI::OwnerCommand::Engage)) &&
        dist < kDetourMaxRange;
    // A bot fighting the group's fight near its leader: the chase / gap-close moves it, not the follow (GroupControl::FightOwnsMovement).
    bool fight_owns_movement = false;
    if (NearbyUnit const* fv = snapshot.victim().IsEmpty() ? nullptr : snapshot.victim_info(); fv && fv->hp > 0)
    {
        ThreatVictim const on = ThreatVictim(fv->victim_kind);
        GroupMemberSummary const* mt = group.tank();
        bool const leader_target = fv->guid == leader_member->victim || (leader_member->is_casting && fv->guid == leader_member->casting_target);
        bool const group_fight = (mt && mt->guid != snapshot.raw().guid && fv->guid == mt->victim) ||
            on == ThreatVictim::Tank || on == ThreatVictim::Member || on == ThreatVictim::MemberPet;
        float member_to_leader = -1.f, mob_to_member = -1.f;
        if (!fv->victim.IsEmpty())
            for (auto const& m : *members)
            {
                if (!m.online || m.map_id != snapshot.map_id()) continue;
                bool const on_member = m.guid == fv->victim;
                bool const on_pet = m.pet_alive && m.pet_guid == fv->victim;
                if (!on_member && !on_pet) continue;
                float const vx = on_member ? m.x : m.pet_x, vy = on_member ? m.y : m.pet_y;
                member_to_leader = DistanceXY(vx, vy, leader_member->x, leader_member->y);
                mob_to_member = DistanceXY(fv->x, fv->y, vx, vy);
                break;
            }
        fight_owns_movement = GroupControl::FightOwnsMovement(true, leader_target, group_fight, on == ThreatVictim::Me,
            DistanceXY(fv->x, fv->y, leader_member->x, leader_member->y), dist, member_to_leader, mob_to_member);
    }
    // In the group's fight (GroupFight.h) the fight moves every bot, healers included (their in-combat rules keep the tank in sight):
    // follow, formation and follow distance are out-of-combat behaviour (rule 6). Not for a bot far off, which still rejoins.
    if (ai.group_fight() && dist < kDetourMaxRange)
        fight_owns_movement = true;
    // The one answer to "does another layer own this bot's movement right now": the follow, the slot move and the recall all stand
    // down for it (a rooted bot cannot follow either).
    bool const movement_owned_elsewhere =
        in_active_bg || dungeon_run_owns_movement || detour_leased || holding_position || order_owns_movement || raid_formation_owns ||
        fight_owns_movement || snapshot.is_rooted();
    bool const refollow = ai.take_formation_refollow();
    // Owner formation: the bot's goal is its SLOT, recomputed every tick from the owner's live position and facing,
    // not "within the recall slack of the owner" (a slot ring wider than the slack, or any spot inside the ring,
    // used to count as arrived, so most of a big group stood still while the owner ran). The destination is only
    // re-issued when the slot has moved more than 2 y from the last one (500 ms apart at most), a bot stops within
    // 1.5 y of the slot and restarts only beyond 2.5 y (no jitter), and a bot that makes no progress for 3 s falls
    // back to the plain follow behind the owner for 8 s.
    // While the anchor is on the move a follower starts at its follow distance (+1.5 yd), not at the full recall slack; a standing
    // anchor keeps the wide slack, so nobody shuffles about at a halt.
    const bool anchor_on_the_move = ai.anchor_moving(anchor->x, anchor->y, snapshot.published_at_ms());
    if (formation_slot_active)
    {
        auto& ft = ai.formation_track();
        constexpr float kArriveYd = 1.5f, kStartYd = 2.5f, kRetargetYd = 2.0f;
        constexpr uint32 kMinReissueMs = 500, kStuckMs = 3000, kFallbackMs = 8000;
        bool const gated = movement_owned_elsewhere;
        float const slot_dist = DistanceXY(bx, by, slot_x, slot_y);
        bool const moving = snapshot.raw().movement.is_moving;
        // 0 = none, 1 = move, 2 = hold, 3 = fallback
        uint8 action = 0;
        if (!gated)
        {
            if (slot_dist <= (moving ? kArriveYd : kStartYd))
            {
                ft.last_progress_ms = formation_now_ms;
                ft.dest_valid = false;
                action = 2;
            }
            else
            {
                if (moving) ft.last_progress_ms = formation_now_ms;
                bool const dest_moved = !ft.dest_valid ||
                    DistanceXY(ft.dest_x, ft.dest_y, slot_x, slot_y) > kRetargetYd;
                if (ft.dest_valid && !moving && formation_now_ms - ft.last_progress_ms > kStuckMs)
                {
                    ft.fallback_until_ms = formation_now_ms + kFallbackMs;
                    ft.dest_valid = false;
                    FormationOffset const plain = ComputeFormationOffset(FormationType::Free, 0, kFollowDistance);
                    emit.follow(anchor->guid, plain.distance, plain.angle_radians);
                    ai.set_last_rule_fired("ingroup:formation_fallback");
                    action = 3;
                }
                else if (anchor_on_the_move)
                {
                    // The owner is walking: the follow generator keeps the bot at its slot's distance and angle and re-aims every update, which
                    // starts the moment the owner does. Walking to the slot as it stood at the last issue (a point the owner has left by
                    // the time the bot gets there, re-issued at most every 500 ms and only after the slot moved 2 y) left the circle trailing
                    // far behind a walking owner (PC 20 Oct, circle, tight, 1 yd). The slot walk settles the bots once the owner halts.
                    ft.dest_valid = false;
                    ft.last_progress_ms = formation_now_ms;
                    emit.follow(anchor->guid, formation_offset.distance, formation_offset.angle_radians);
                    ai.set_last_rule_fired("ingroup:formation_follow");
                    action = 1;
                }
                else
                {
                    action = 1;
                    if (dest_moved && formation_now_ms - ft.last_issue_ms >= kMinReissueMs)
                    {
                        if (!ft.dest_valid) ft.last_progress_ms = formation_now_ms;
                        emit.move_to(slot_x, slot_y, leader_member->z, /*run*/ true);
                        ft.dest_valid = true;
                        ft.dest_x = slot_x; ft.dest_y = slot_y;
                        ft.last_issue_ms = formation_now_ms;
                        ai.set_last_rule_fired("ingroup:formation_slot");
                    }
                }
            }
        }
        // A bot that is told to move and does not, for more than 3 s: where it stands, what is around it, and whether its moves were refused.
        // Cause of a standstill that no path_fail line explains (a bot standing on a gameobject model is higher than the ground under it).
        if ((action == 1 || action == 3) && !moving && formation_now_ms - ft.last_progress_ms > kStuckMs &&
            formation_now_ms - ft.last_stuck_log_ms >= 5000)
        {
            ft.last_stuck_log_ms = formation_now_ms;
            std::string near_objs;
            for (auto const& o : snapshot.raw().world_objects.nearby_objects)
            {
                float const od = DistanceXY(bx, by, o.x, o.y);
                if (od <= 4.0f)
                    near_objs += fmt::format("{}{}(type {}) d={:.1f} dz={:.1f}", near_objs.empty() ? "" : "; ", o.entry, o.go_type, od, o.z - bz);
            }
            float const gz = snapshot.raw().movement.ground_z;
            TC_LOG_INFO("playerbot.v2",
                "[formation_stuck] {} bot=({:.1f},{:.1f},{:.1f}) ground_z={} slot=({:.1f},{:.1f}) slot_dist={:.1f} action={} stuck_ms={} "
                "path_blocked={} last_blocked_ms_ago={} rooted={} objects_within_4y=[{}]",
                snapshot.name(), bx, by, bz, gz > -50000.0f ? fmt::format("{:.1f}", gz) : std::string("n/a"), slot_x, slot_y, slot_dist,
                action == 1 ? "move" : "fallback", formation_now_ms - ft.last_progress_ms, snapshot.path_blocked_count(),
                snapshot.last_path_blocked_ms() ? formation_now_ms - snapshot.last_path_blocked_ms() : 0u, int(snapshot.is_rooted()), near_objs);
        }
        if (action != 0 && action != ft.last_action && formation_now_ms - ft.last_log_ms >= 5000)
        {
            TC_LOG_INFO("playerbot.v2",
                "[formation] {} type={} slot={}/{} slot=({:.1f},{:.1f}) owner=({:.1f},{:.1f}) dist={:.1f} action={} "
                "bot=({:.1f},{:.1f},{:.1f}) owner_z={:.1f} owner_dist={:.1f} moving={}",
                snapshot.name(), FormationTypeName(slot_type), slot_index, slot_count, slot_x, slot_y,
                leader_member->x, leader_member->y, slot_dist,
                action == 1 ? "move" : action == 2 ? "hold" : "fallback",
                bx, by, bz, leader_member->z, dist, moving ? 1 : 0);
            ft.last_log_ms = formation_now_ms;
            ft.last_action = action;
        }
    }
    else if (!formation_fallback && ai.formation_track().dest_valid)
        ai.formation_track().dest_valid = false;

    const float recall_now = anchor_on_the_move ? std::min(kRecallSlack, std::max(kFollowDistance + 1.5f, 5.0f)) : kRecallSlack;
    if (!formation_slot_active && !movement_owned_elsewhere &&
        (dist > recall_now || (refollow && dist > 1.5f)))
    {
        // Follow uses MotionMaster MoveFollow, which silently re-emits forever
        // when the leader is unreachable (across a gap / closed door) — and
        // grouped bots are exempt from GlobalStuckRescue, so they have no other
        // recovery. Track whether the gap is actually closing; if it stalls for
        // ~5s, fall back from follow to an explicit, navmesh-validated move_to
        // toward the leader. A failed move_to (Locked) feeds path_blocked +
        // last_move_to, which engages the walk-first unstick ladder (walk-escape
        // toward the nearest known-good node). No teleport.
        constexpr uint32 kFollowStaleTicks  = 25;    // ~5s at 5 Hz — gap not closing
        constexpr uint32 kFollowEscapeTicks = 100;   // ~20s — leader unreachable
        // M-P2b: note_follow_dist counts a tick "stale" whenever the gap
        // doesn't shrink by ≥2y. But when the leader is MOVING and the
        // follower is chasing at a matched pace, the gap holds roughly
        // constant even though MoveFollow is working perfectly — the
        // generator is tracking a moving target. The old code let stale
        // climb in that case and then yanked the bot off MoveFollow into
        // an explicit move_to (and eventually the escape-node path),
        // interrupting a follow that was actually fine.
        //
        // The bot being IN MOTION is the direct symptom of "following is
        // working": MoveFollow is producing movement toward the (moving)
        // leader. So while the bot is moving, treat the follow as healthy —
        // reset the stale counter and stay on the cheap MoveFollow path.
        // The stale escalation only fires when the bot is NOT moving (truly
        // wedged: MoveFollow re-emitting forever against an unreachable
        // leader, bot pinned in place). The proper, position-exact fix
        // (reset on leader-position-changed-since-last-tick) needs a
        // per-bot last-anchor-XY field on BotAI, which this file cannot add.
        //   // M-P2b: precise leader-moved reset needs last_anchor_x/y on BotAI.
        const bool bot_in_motion = snapshot.raw().movement.is_moving;
        uint32 stale;
        if (bot_in_motion)
        {
            // Following is actively progressing — don't let a constant gap
            // accrue false stale ticks. Re-seed the tracker at the current
            // gap so a genuine stall (motion stops, gap stops closing) still
            // escalates from a clean baseline.
            ai.reset_follow_progress();
            stale = ai.note_follow_dist(anchor->guid.GetCounter(), dist);
        }
        else
        {
            stale = ai.note_follow_dist(anchor->guid.GetCounter(), dist);
        }
        // Throttle the validated repath to ~every 2.5s; between attempts use the
        // cheap MotionMaster MoveFollow so we don't pathfind + log every tick.
        if (!bot_in_motion && stale >= kFollowStaleTicks &&
            ai.follow_repath_due(snapshot.published_at_ms(), 2500))
        {
            float tx = anchor->x, ty = anchor->y, tz = anchor->z;
            if (stale >= kFollowEscapeTicks)
            {
                // Stalled for ~20s with the gap not closing → the leader is
                // genuinely unreachable (across a gap / closed door). Aim the
                // move_to at the nearest known-good navmesh node instead of
                // hammering the unreachable leader, giving the bot a chance to
                // route onto valid mesh. No teleport. Falls back to the leader
                // position when no node is near.
                if (auto const* node = Services::TravelGraph().FindNearestNodeOnMap(
                        snapshot.map_id(), bx, by, 800.f))
                {
                    const float ndx = node->x - bx, ndy = node->y - by;
                    // M-P2a: the nearest node to the BOT can sit on the far
                    // side of the bot from the leader — routing onto it would
                    // walk the follower AWAY from the group (observed: a dock
                    // bot recalled to a pier node behind it, increasing the
                    // gap every escape cycle). Only accept the node when it
                    // makes positive progress toward the anchor: its XY
                    // distance to the anchor must be strictly less than the
                    // bot's current gap. Otherwise keep aiming the move_to at
                    // the anchor itself (the navmesh-validated move_to + the
                    // walk-first unstick ladder still handle a truly blocked
                    // leader without sending the bot backward).
                    const float node_to_anchor =
                        DistanceXY(node->x, node->y, anchor->x, anchor->y);
                    if (ndx * ndx + ndy * ndy > 25.f && node_to_anchor < dist)
                    {
                        tx = node->x; ty = node->y; tz = node->z;
                    }
                }
            }
            // Toward the owner himself (not the escape node): a gap across his side, level with the bot, is leapt from its edge (API::move_to follow_drop).
            emit.move_to(tx, ty, tz, /*run*/ true, false, false, /*follow_drop*/ stale < kFollowEscapeTicks);
            ai.set_last_rule_fired("ingroup:follow_recall_repath");
        }
        else
        {
            emit.follow(anchor->guid, formation_offset.distance, formation_offset.angle_radians);
            ai.set_last_rule_fired("ingroup:follow_recall");
        }
    }
    else
    {
        // Within slack (or following suppressed) — not separated, clear the
        // stale-progress counter so a future real separation starts fresh.
        // Not while an owner formation has the bot outside its slack: the slot walk and the plain-follow fallback alternate (3 s / 8 s),
        // and clearing the counter at every switch kept the stale count under the repath threshold forever, so a bot that could not
        // reach its slot never got the validated repath or the escape node (the Stormwind Trade District ramp).
        if (!(formation_slot_active && dist > kRecallSlack))
            ai.reset_follow_progress();
    }

    // Auto-mount when the leader is far ahead and we're outdoors — closes
    // the gap quickly. We only mount if the bot isn't already mounted, isn't
    // in combat (handled by InCombat short-circuit above), isn't swimming /
    // flying, and the leader is mounted (a strong signal we should be too).
    constexpr float kMountThreshold = 30.0f;
    // Same in_active_bg gate applies to leader-driven mount/dismount —
    // BG maps generally disallow mounts and the InCombat dispatch already
    // dismounts on attack. Letting these fire would spam intents the
    // spell engine rejects.
    // A bot in a group with a player (its owner's companion, or a world bot the player invited) mounts when the leader does, wherever it
    // is: leader and party ride off together (2026-10-19: a level 59 bot ran after a mounted player on foot). Others only mount to close a
    // real gap.
    const bool want_mount = !in_active_bg &&
                            (dist > kMountThreshold || snapshot.is_led_by_player()) &&
                            leader_member->is_mounted &&
                            !snapshot.is_mounted() &&
                            !snapshot.is_indoors() &&
                            !snapshot.raw().movement.is_swimming &&
                            !snapshot.raw().movement.is_flying;
    // M-P2e: dismount jitter shares the per-bot mount-timer field. Mount and
    // dismount are mutually exclusive in a single tick (want_mount requires
    // !is_mounted(); the dismount block below requires is_mounted()), so the
    // field is never contended. Computed here so the mount-else clear below
    // knows not to wipe a pending dismount deadline.
    const bool want_dismount = !in_active_bg && snapshot.is_mounted() &&
                               !leader_member->is_mounted &&
                               (dist <= kMountThreshold || snapshot.is_led_by_player());
    if (want_mount)
    {
        // Edge-triggered: a mount cast takes ~1.5s; without dedup we'd spam
        // MountIntent every tick during the cast and the API would just
        // reject all but the first. Cleared once the bot is mounted or the
        // condition lapses (e.g. leader dismounted, gap closed).
        if (!ai.mount_pending())
        {
            // Reaction-time hesitation: 300–1200ms per bot. Without this
            // a 5-bot raid all mounts on the same frame as the leader,
            // which is impossible for human-played followers.
            const uint32 now_ms = snapshot.published_at_ms();
            uint32 ready_at = ai.pending_follow_mount_at_ms();
            if (ready_at == 0)
            {
                const uint32 jitter =
                    300u + (uint32(snapshot.bot_id()) * 2654435761u) % 900u;
                ai.set_pending_follow_mount_at_ms(now_ms + jitter);
            }
            else if (now_ms >= ready_at)
            {
                emit.mount_appropriate();
                ai.set_mount_pending(true);
                ai.set_last_rule_fired("ingroup:mount");
            }
        }
        else
        {
            // Still not mounted a few seconds after the cast was sent: it failed (interrupted by the follow, moving, a stuck cast) and
            // nothing else clears the flag while the leader stays mounted, so the bot rode nowhere (playtest 10-19: one of three on foot).
            // Ask again.
            constexpr uint32 kMountRetryMs = 4000u;
            const uint32 now_ms = snapshot.published_at_ms();
            const uint32 sent_at = ai.pending_follow_mount_at_ms();
            if (sent_at != 0 && now_ms >= sent_at + kMountRetryMs)
            {
                ai.set_mount_pending(false);
                ai.set_pending_follow_mount_at_ms(0);
            }
        }
    }
    else
    {
        if (ai.mount_pending()) ai.set_mount_pending(false);
        // Don't wipe the shared timer when a dismount is pending — the
        // dismount block (M-P2e) is using it for its own stagger this tick.
        if (!want_dismount && ai.pending_follow_mount_at_ms() != 0)
            ai.set_pending_follow_mount_at_ms(0);
    }

    // Auto-dismount when the leader dismounts and we're close enough that
    // running speed catches up. Without this the bot stays mounted
    // indefinitely after the auto-mount fired earlier — mounted bots can't
    // cast or interact, so leader's "we're at the quest hub" signal needs
    // the bots to be ready too. dist gate avoids dismounting mid-travel
    // when the leader briefly dismounts to interact with something while
    // we're still 50yd back.
    if (want_dismount)
    {
        // M-P2e: reaction-time stagger, mirroring the mount path. Without it
        // the whole squad dismounts on the exact frame the leader does —
        // impossible for human-played followers and an obvious bot tell.
        // Reuse the per-bot mount-timer field (mount/dismount never overlap
        // in a tick; the mount-else above is taught not to wipe it while a
        // dismount is pending). Deterministic 300–1200ms per-bot offset keyed
        // on bot_id so the same bot is consistent run-to-run but the squad
        // spreads out. The action-retry dedup below still suppresses
        // in-flight duplicate packets once the stagger elapses.
        const uint32 dm_now_ms = GameTime::GetGameTimeMS();
        const uint32 sched_now_ms = snapshot.published_at_ms();
        uint32 dm_ready_at = ai.pending_follow_mount_at_ms();
        if (dm_ready_at == 0)
        {
            const uint32 jitter =
                300u + (uint32(snapshot.bot_id()) * 2654435761u) % 900u;
            ai.set_pending_follow_mount_at_ms(sched_now_ms + jitter);
        }
        else if (sched_now_ms >= dm_ready_at)
        {
            // Edge-trigger: dismount is a server-validated packet; the
            // `is_mounted()` snapshot field flips false the same tick the
            // SPELL_AURA_MOUNTED aura is removed but BEFORE the dismount
            // round-trip completes if invoked twice. Without dedup, a fresh
            // dismount intent in flight gets duplicated. Same pattern as
            // mount above.
            constexpr uint64 dm_key = uint64(0xD150FF1'1u);
            if (!ai.action_recently_tried(BotAI::ActionKind::BgPort, dm_key, dm_now_ms))
            {
                emit.dismount();
                ai.note_action_retry(BotAI::ActionKind::BgPort, dm_key, dm_now_ms);
                ai.set_last_rule_fired("ingroup:dismount");
            }
            // Stagger satisfied — release the shared timer so the next
            // mount/dismount cycle re-rolls a fresh offset.
            ai.set_pending_follow_mount_at_ms(0);
        }
    }
}

// Follow an owner who dropped or jumped to a floor 4+ y below (he went 11 y down into the arena; his bots, which
// only followed the walking route, went round through the Blackhand packs and died. A warlock then started the ledge walk, a buff cast
// stopped it, and a combat escape took it along the rim the long way). Runs at the top of every tick, whatever the state. The follow
// itself is API::move_to follow_drop: when the walking route to him is missing or a long detour it walks to the nearest ledge and
// drops or leaps; when the route is fine it is the ordinary pathed move and the intent executor marks it fine (no attempt for 3 s).
// While the walk or the flight is on, the tick is consumed: no buff, no stand-up, no combat step can re-aim the bot.
bool HonorFollowDrop(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit)
{
    uint32 const now = GameTime::GetGameTimeMS();
    if (!s.is_alive() || s.in_battleground() || ai.dungeon_active() || s.is_rooted() || s.raw().movement.is_mounted || !g.exists())
    { ai.clear_follow_drop(); return false; }
    uint32 const pub = s.published_at_ms();
    if (ai.holding_position(pub) || ai.pull_back_active() ||
        (ai.is_manual_mode(pub) && ai.owner_command() == BotAI::OwnerCommand::Engage))
    { ai.clear_follow_drop(); return false; }
    ObjectGuid const leader_guid = g.leader();
    auto const* members = g.members();
    if (leader_guid.IsEmpty() || leader_guid == s.guid() || !members)
    { ai.clear_follow_drop(); return false; }
    GroupMemberSummary const* leader = nullptr;
    for (auto const& m : *members)
        if (m.guid == leader_guid) { leader = &m; break; }
    // Only a human owner is followed off a ledge; a bot leader is not.
    if (!leader || !leader->online || !leader->is_alive || leader->is_bot || leader->map_id != s.map_id())
    { ai.clear_follow_drop(); return false; }
    float bx, by, bz; s.position(bx, by, bz);
    float const dist = DistanceXY(bx, by, leader->x, leader->y);
    if (dist < 6.0f || dist > 50.0f || bz - leader->z < 4.0f)
    { ai.clear_follow_drop(); return false; }

    bool const committed = ai.follow_drop_committed(now);
    if (!committed && (ai.follow_drop_fine(now) || ai.follow_drop_blocked(now)))
        return false;
    if (committed && now - ai.follow_drop_since_ms() > BotAI::kFollowDropMaxMs)
    {
        TC_LOG_INFO("playerbot.v2", "[follow_drop] {} gives up the walk to the ledge after {} s", s.name(), BotAI::kFollowDropMaxMs / 1000u);
        ai.block_follow_drop(now);
        return false;
    }
    bool const pushed = emit.move_to(leader->x, leader->y, leader->z, /*run=*/true, false, false, /*follow_drop=*/true);
    // A refused move (casting, no path) ends the attempt; the rules and the plain follow carry on.
    if (!pushed && ai.move_refused_recently(leader->x, leader->y, leader->z, pub))
    { ai.clear_follow_drop(); return false; }
    ai.set_last_rule_fired(committed ? "follow_drop:commit" : "follow_drop:try");
    // A first attempt consumes its tick so the plain follow cannot override the ledge walk it starts; a deduped re-attempt of one in
    // progress consumes it too (the walk is running); a deduped attempt with nothing in progress lets the rules run.
    return committed || pushed;
}


} // namespace Playerbot::States
