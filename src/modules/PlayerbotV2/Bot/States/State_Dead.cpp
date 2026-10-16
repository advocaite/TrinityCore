// State_Dead — Bot has died. Auto-recovery picks between:
//
//   1. Corpse run (no resurrection sickness, slow): release the corpse to
//      ghost form, walk back to the corpse, reclaim. The canonical
//      human path; chosen for short corpse-to-graveyard distances and
//      for L1-10 where sickness doesn't apply at all.
//
//   2. Spirit healer (instant alive at graveyard, with sickness +
//      durability hit): chosen when the corpse run would be too long
//      (> kSpiritHealerDistanceThreshold yards) for an L11+ bot. Saves
//      fleet throughput at the cost of 10 minutes of -75% damage/healing
//      and 25% durability.
//
// Self-rez (Shaman Reincarnation, Druid Rebirth, Soulstone) is preferred
// over both when available — class-specific resurrects bypass the
// sickness and the corpse run entirely.

#include "StateBase.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotTextMgr.h"
#include "Bot/BotIntentEmitter.h"
#include "Group/GroupSnapshot.h"
#include "Services.h"
#include "Util/ConfigReader.h"
#include "Travel/RepairVendorIndex.h"
#include "Travel/QuestHubDatabase.h"
#include "GameTime.h"
#include <atomic>
#include "Log.h"
#include "SharedDefines.h"   // CLASS_* for the owner-rez check
#include <cmath>

namespace Playerbot::States {

namespace {

// Threshold separating corpse-run from spirit-healer for L11+ bots.
// Ghost movement is ~10.5 y/s in flat terrain (basic 7 y/s walk * 1.5
// ghost speed bonus). 600y → ~57s wall-clock — short enough that an L11+
// bot eats less than 6 min equivalent of "if we'd taken sickness." Past
// this we prefer the spirit-healer to free up the bot for combat.
constexpr float kSpiritHealerDistanceThreshold = 600.0f;

// Reclaim radius (CORPSE_RECLAIM_RADIUS) is 39yd; we approach to 30yd
// before emitting reclaim so we're comfortably inside even with mmap
// drift. Below this, we stop walking and reclaim.
constexpr float kCorpseApproachRadius = 30.0f;

bool ShouldUseSpiritHealer(BotSnapshotView const& s)
{
    // L1-10: no sickness regardless, so corpse-run is strictly cheaper.
    if (s.level() <= 10) return false;
    // No corpse yet (haven't released, or self-rezzed) — irrelevant decision.
    if (!s.has_corpse()) return false;
    // Far corpse run → spirit-healer.
    return s.corpse_to_graveyard_dist() > kSpiritHealerDistanceThreshold;
}

} // namespace

void DispatchDead(BotAI& ai,
                  BotSnapshotView snapshot,
                  GroupSnapshotView group,
                  BotIntentEmitter& emit)
{
    if (snapshot.is_alive())
    {
        // BotAI::tick will demote the state on the next call, but emit any
        // standing intents now: clearing combat target so we don't re-engage
        // mid-corpse-run if the previous victim is still alive.
        emit.stop_attack();
        ai.set_rez_acked(false);
        ai.set_corpse_recovery_emitted(false);
        ai.set_ghost_since_ms(0);  // reset stuck-ghost timer
        ai.set_release_pending_at_ms(0);  // reset death-release delay
        ai.set_dead_watchdog_ms(0);  // reset bounded-recovery watchdog
        // DMM-P1b: clear the Shaman Reincarnation self-rez guard so the next
        // death gets a fresh attempt. (A successful Reincarnation lands here
        // via is_alive=true; a failed one is cleared so it isn't permanently
        // suppressed.)
        ai.set_reincarnation_attempted(false);
        ai.set_reincarnation_attempt_ms(0);
        // DMM-P3a: reset the corpse-run no-progress tracker.
        ai.set_corpse_run_last_dist(-1.0f);
        ai.set_last_rule_fired("dead:revived");
        return;
    }

    // --- Death-duration clock (drives every pre-release bound below) ---
    // Stamp the moment of death off the server-authoritative GameTime clock (NOT
    // snapshot.published_at_ms) on the first dead tick, then measure forward. This
    // MUST be computed before ANY early-return branch (rez-accept, group-cast hold),
    // because each of those can trap the handler indefinitely and must be bounded by
    // it. ROOT CAUSE of the 377s wedge (2026-06-26): a dead bot was offered a rez
    // that never completed (rezzer perma-interrupted in combat), so it returned in
    // the `has_resurrect_request` branch below EVERY tick — a watchdog placed AFTER
    // that branch was never reached, and only fired (at 377s) once the offer finally
    // lapsed on its own. With the clock here, every pre-release branch yields to the
    // force-release backstop once dead_for passes the cap. Reset on revival (above)
    // and on the death edge (BotAI.cpp).
    const bool dead_clock_active = !snapshot.in_battleground();
    uint32 dead_for = 0;
    if (dead_clock_active)
    {
        const uint32 gnow = GameTime::GetGameTimeMS();
        if (ai.dead_watchdog_ms() == 0)
            ai.set_dead_watchdog_ms(gnow);
        dead_for = gnow - ai.dead_watchdog_ms();
    }
    constexpr uint32 kPreReleaseStallMs = 20000u;  // 20s >> the 3-7s release jitter

    // Can a groupmate rez us IN PLACE? True if any online, ALIVE groupmate is on
    // our map. A surviving healer's out-of-combat MaintainOocRez (or a battle-rez)
    // raises us at our corpse once it clears combat — vastly better than releasing,
    // which teleports our ghost to the zone graveyard. For many instances that
    // graveyard is CROSS-MAP (Deadmines zone 1581 -> Westfall map 0 GY): releasing
    // there ejects the bot from the instance, drops its dungeon run_mode on the map
    // change, and it wanders off as an open-world bot — exactly how a single tank
    // death ended a Deadmines run on 2026-06-26 (tank died mid-gauntlet with 4
    // groupmates ALIVE, released 3-7s later, teleported to (-11174,1623) map 0, and
    // left for a flight master). Real players hold the death screen for their group;
    // so do we. While this is true we WAIT in place (see the release block below)
    // and the watchdog uses a longer cap so we don't eject at 20s mid-fight. On a
    // true wipe (no alive groupmate) it's false and recovery releases promptly.
    // Require an alive HEALER (not just any alive member) on our map. A surviving
    // hunter/rogue can't rez us, so waiting on them is pointless — it just burns
    // the 90s backstop before we eject anyway (observed 2026-06-26: a full
    // gauntlet wipe left only the hunter alive, yet all 4 corpses waited the full
    // 90s because group_can_rez keyed off "any alive member"). With a healer the
    // wait pays off (it rezzes us in place once OOC); without one we should
    // release/corpse-run promptly at the normal cap. (Battle-rez DPS classes are
    // not modelled here; the common dungeon comp has a healer, and on a true wipe
    // the healer is usually dead — exactly when we want the prompt release.)
    bool group_can_rez = false;
    if (dead_clock_active && group.exists() && group.members())
    {
        const ObjectGuid me = snapshot.raw().guid;
        float cx, cy, cz; snapshot.position(cx, cy, cz);
        // Forward-only recovery (2026-06-27): only WAIT in place for a group rez if
        // an alive healer is CLOSE (within kFetchMaxY of our corpse). The in-instance
        // graveyard sits ~200y from the harbor/gauntlet body, so a FAR healer would
        // have to trek back to rez us — dragging the whole group backward (the harbor
        // fragmentation cycle). The rezzer never treks (go_rez / escort_fallen share
        // this cap), so a death far from the healer instead RELEASES promptly (this
        // instance instant-rezzes alive at the GY) and DungeonRecoverStrandedFollower
        // teleports us FORWARD onto the body. Same 80y threshold as go_rez.
        constexpr float kFetchMaxY = 80.0f;
        for (auto const& m : *group.members())
            if (m.online && m.is_alive && m.guid != me &&
                m.map_id == snapshot.map_id() &&
                (m.role == Role::Healer ||
                 // A companion's owner rezzes it whatever their spec, if their class can (a shadow priest, a ret paladin).
                 (m.guid == snapshot.raw().owner_guid &&
                  (m.cls == CLASS_PRIEST || m.cls == CLASS_PALADIN || m.cls == CLASS_SHAMAN || m.cls == CLASS_DRUID))))
            {
                const float dx = m.x - cx, dy = m.y - cy, dz = m.z - cz;
                if (dx * dx + dy * dy + dz * dz <= kFetchMaxY * kFetchMaxY)
                { group_can_rez = true; break; }
            }
    }
    // --- Owner orders and wipes (ike3 ReleaseSpiritAction, the owner's raid rules) ---
    // release_ordered: the owner said "release" (BotCommandParser::OrderRelease).
    // owner_released: the companion's owner (a group member on our map) is a
    // ghost on a corpse run; we release and run back with them instead of
    // lying here. wipe: nobody of the group is alive in this instance any more
    // (every member dead, or every living one outside it) — no rez can come, so
    // we release after the short release jitter below rather than waiting.
    const bool release_ordered = ai.release_order_ms() != 0;
    GroupMemberSummary const* owner = nullptr;
    bool wipe = false;
    if (group.exists() && group.members())
    {
        const ObjectGuid me = snapshot.raw().guid;
        bool any_alive_here = false;
        for (auto const& m : *group.members())
        {
            if (m.guid == me || !m.online) continue;
            if (m.guid == snapshot.raw().owner_guid && snapshot.is_companion_with_owner())
                owner = &m;
            if (m.is_alive && m.map_id == snapshot.map_id())
                any_alive_here = true;
        }
        wipe = !snapshot.is_ghost() && snapshot.is_in_instance() && !any_alive_here;
    }
    const bool owner_released =
        owner && !owner->is_alive && owner->is_ghost && !snapshot.is_ghost();
    if (wipe)
    {
        group_can_rez = false;
        if (!ai.wipe_logged())
        {
            ai.set_wipe_logged(true);
            TC_LOG_INFO("playerbot.v2",
                "[death-recovery] {} wipe detected on map {} (no living group member in the instance), releasing shortly",
                snapshot.name(), snapshot.map_id());
            // A dungeon wipe: now and then one companion says "my bad" / "wp" (Playerbot.Behavior.PartyBanter;
            // raids have the 80% rule below).
            if (snapshot.is_in_dungeon() && snapshot.in_group())
            {
                std::string line = BotChatter::BanterLine(BotChatter::Banter::Wipe, ai, snapshot);
                if (!line.empty())
                    emit.say(std::move(line));
            }
        }
    }
    // Survivors get a long window to clear combat and rez us; a true wipe releases
    // promptly. The watchdog and the rez-accept wait both key off this cap.
    constexpr uint32 kGroupRezWaitMs = 90000u;
    // A raid does not wait the full 90 s: its bots hold the death screen a
    // per-bot random 10-40 s for a living healer's resurrect (the owner's
    // ReleaseSpiritAction), then release (Playerbot.Behavior.WipeReleaseWait).
    const bool raid_wait = group_can_rez && snapshot.is_in_raid() && Services::Config().behavior_wipe_release_wait();
    const uint32 raid_wait_ms = 10000u + (uint32(snapshot.bot_id()) * 2246822519u) % 30001u;
    // A raider who dies mid-encounter stays dead until the fight is over (a combat rez may still come; MC Garr 2026-10-10: a priest
    // released 31 s into the fight and spent it on a 1063 y walk back, then died again on arrival). Not on a wipe (nobody is left,
    // the release goes ahead) and not on the owner's order. The long cap only keeps a stuck encounter flag from holding a corpse for ever.
    constexpr uint32 kEncounterWaitMs = 900000u;
    const bool encounter_wait = snapshot.is_in_raid() && snapshot.is_encounter_in_progress() && !wipe && !release_ordered &&
                                !owner_released && !snapshot.is_ghost();
    const uint32 release_cap = encounter_wait ? kEncounterWaitMs
        : raid_wait ? raid_wait_ms : (group_can_rez ? kGroupRezWaitMs : kPreReleaseStallMs);
    // Wipe chatter: once 80% of the raid on this map is dead, one corpse in
    // four says so, at most once per 600 s across the fleet (the owner: 1 in 4,
    // 600 s). Party chat, the same helper as the rez thanks above.
    if (dead_clock_active && snapshot.is_in_raid() && group.exists() && group.members() &&
        Services::Config().behavior_wipe_release_wait() && !snapshot.is_ghost())
    {
        uint32 online = 1, dead = 1;   // this bot counts
        for (auto const& m : *group.members())
        {
            if (m.guid == snapshot.raw().guid || !m.online || m.map_id != snapshot.map_id()) continue;
            ++online;
            if (!m.is_alive) ++dead;
        }
        static std::atomic<uint32> s_last_wipe_chat_ms{0};
        uint32 const wnow = GameTime::GetGameTimeMS();
        uint32 last = s_last_wipe_chat_ms.load();
        // The 1-in-4 roll is a hash of (bot, death start): stable for the whole
        // death, so every tick of the same death reaches the same verdict.
        uint32 const roll = (uint32(snapshot.bot_id()) * 2654435761u ^ ai.dead_watchdog_ms() * 2246822519u) >> 7;
        if (online >= 10 && dead * 100u >= online * 80u && roll % 4u == 0u &&
            (last == 0 || wnow - last >= 600000u))
        {
            if (s_last_wipe_chat_ms.compare_exchange_strong(last, wnow ? wnow : 1u))
            {
                static constexpr char const* kWipe[] = {
                    "well, that went badly", "wipe :(", "ouch", "ok that one is on me", "and we're all dead",
                };
                emit.say(kWipe[ai.rng().int_range(0, int32_t(sizeof(kWipe) / sizeof(*kWipe)))]);
            }
        }
    }
    // Rate-limited "waiting" log (one line per 15s per bot).
    auto log_waiting = [&](char const* what)
    {
        const uint32 lnow = GameTime::GetGameTimeMS();
        if (ai.death_log_ms() != 0 && lnow - ai.death_log_ms() < 15000u) return;
        ai.set_death_log_ms(lnow ? lnow : 1u);
        TC_LOG_INFO("playerbot.v2", "[death-recovery] {} {} (dead {}s)", snapshot.name(), what, dead_for / 1000u);
    };

    // Auto-accept any pending rez popup before falling back to the recovery
    // flow. Edge-triggered: snapshot retains has_resurrect_request until the
    // world thread processes the accept; we only emit once per request
    // window.
    if (snapshot.has_resurrect_request())
    {
        if (!ai.rez_acked())
        {
            emit.emit(AcceptRezIntent{});
            ai.set_rez_acked(true);
            // Real players /thank the rezzer immediately. We don't have
            // the caster's GUID in the rez-request snapshot (only the
            // bool flag), so a party-channel "ty!" is the closest
            // analog — addresses whoever's listening. Skip when solo
            // (no group → no one to thank in chat, would be cringe).
            if (group.exists() && Services::Config().behavior_party_banter())
            {
                // Gated, rare and varied (cooldowns, 1-in-2 roll, never in a big raid or when chatter is off).
                std::string line = BotChatter::BanterLine(BotChatter::Banter::Thanks, ai, snapshot);
                if (!line.empty())
                    emit.say(std::move(line));
                if (Services::Config().behavior_social_emotes() && !line.empty())
                    emit.perform_emote(2 /*EMOTE_ONESHOT_BOW*/, ObjectGuid::Empty);
            }
            else if (group.exists())
            {
                static constexpr char const* kThanks[] = {
                    "ty for the rez!", "ty rez", "thanks for the res :)",
                    "tyvm rez", "thanks rezzer",
                };
                const size_t n = sizeof(kThanks) / sizeof(*kThanks);
                const size_t idx = static_cast<size_t>(ai.rng().int_range(0, int32_t(n)));
                emit.say(kThanks[idx]);
            }
        }
        ai.set_last_rule_fired("dead:waiting_rez_accept");
        log_waiting("waiting for an offered rez to land");
        // Wait for the offered rez to land — but only up to the release cap.
        // Past it, a rez that is offered yet never completes (rezzer perma-
        // interrupted in combat — the 377s wedge) must NOT trap us here: fall
        // through to the force-release backstop below. The accept above is
        // edge-latched (rez_acked), so it still fires once if the rez ever lands.
        if (!dead_clock_active || dead_for < release_cap)
            return;
    }
    else if (ai.rez_acked())
    {
        ai.set_rez_acked(false);
    }

    // --- Bounded-recovery watchdog (2026-06-26) ---
    // Death recovery MUST terminate, or one stuck corpse deadlocks the whole
    // group: the dungeon tank-advance "group_not_ready" gate waits forever on a
    // dead member. Belt-and-braces backstop for ANY pre-release stall (the
    // rez-accept loop above, the group-cast hold below, or anything else): keyed
    // off the hoisted GameTime death clock (NOT snapshot.published_at_ms), if a
    // death goes unrecovered past the cap WITHOUT releasing to a ghost, FORCE the
    // release (BuildPlayerRepop+RepopAtGraveyard) and hand off to the normal
    // post-release corpse-run/spirit-healer flow — the same path that recovers
    // every healthy death (NOT a direct spirit-resurrect, which from a pre-release
    // state left the bot in a tangled ghost-but-not-revived half-state live). Gate
    // on !ghost so a legitimate post-release corpse-run (already a ghost, handled
    // by the ghost-no-progress path below) is never cut short. Not in BG.
    if (dead_clock_active)
    {
        if (!snapshot.is_ghost() && dead_for > release_cap)
        {
            TC_LOG_INFO("playerbot.v2",
                "[death-recovery] {} force-released after {}ms pre-release stall",
                snapshot.name(), dead_for);
            emit.emit(ReleaseCorpseIntent{});
            ai.set_corpse_recovery_emitted(true);            // advance FSM to post-release
            ai.set_release_pending_at_ms(snapshot.published_at_ms());
            ai.set_last_rule_fired("dead:watchdog_force_release");
            return;
        }
    }

    // Hold release if a group member is mid-cast targeting this corpse —
    // they're rezzing us. Releasing now would teleport the corpse to the
    // graveyard and the rez would fizzle InvalidTarget. Once their cast
    // finishes the rez popup arrives and the branch above accepts.
    // CRITICAL: require the caster to be ALIVE. A DEAD groupmate retains its
    // last-frame is_casting flag (the snapshot can't observe a cast-cancel on a
    // bot that died mid-cast), so without this gate a wipe leaves every corpse
    // perma-held by the stale is_casting of a dead would-be rezzer — the exact
    // deadlock that wedged the whole Deadmines group in dead:release_pending
    // (2026-06-26). A dead bot cannot rez, so its cast can never land.
    // Bound the rez-hold to ~ONE Resurrection cast window. A live groupmate that
    // perpetually RE-initiates a rez that never lands (out of range, or its cast
    // is interrupted every tick by combat/movement) would otherwise hold this
    // corpse forever — the live-caster variant of the wipe deadlock (observed
    // 2026-06-26: bots held 75-91s mid-gauntlet-wipe by a still-casting healer
    // before the watchdog force-released them). A genuine in-progress rez lands
    // well inside this window and is still received by the accept branch above;
    // past it, stop waiting and release. Measured off the GameTime dead-stamp set
    // by the watchdog block above (non-zero for any non-BG dead bot by now).
    constexpr uint32 kRezHoldWindowMs = 12000u;  // > a 10s Resurrection cast
    if (group.exists() && dead_for < kRezHoldWindowMs)
    {
        if (auto const* members = group.members())
        {
            const ObjectGuid me = snapshot.raw().guid;
            for (auto const& m : *members)
            {
                if (!m.online || !m.is_alive || !m.is_casting) continue;
                if (m.casting_target == me) return;
            }
        }
    }

    // Self-rez attempt (corpse-only, ~30min CD). Shamans get Reincarnation.
    // 20608 is the castable spell that performs the in-place resurrect; 21169
    // ("Reincarnation") is the linked spell that actually carries the ~30-min
    // cooldown (modern WoW splits the cast from the cooldown record). Reading
    // is_ready(20608) was the DMM-P1b bug: 20608 may carry no cooldown row in
    // the snapshot while 21169 holds the real CD, so a shaman that already
    // self-rezzed in the last 30 min would (a) re-cast 20608, fizzle on the
    // server-side CD, and then — since the OLD code set corpse_recovery_emitted
    // and never released — sit forever in dead:waiting_release (is_ghost()==
    // false because it never released). Permanent wedge for any shaman dying
    // twice within 30 min. Fix: gate readiness on the REAL cooldown spell
    // 21169, and use a dedicated reincarnation_attempted_ guard with a ~2.5s
    // fall-through so a failed/fizzled cast drops into the normal release path
    // instead of looping. Cast as ghost — the cast resurrects the bot in place
    // at full HP/Mana, skipping the corpse-run. Other classes (warlock
    // soulstone, druid Rebirth) need pre-applied buffs we don't yet model, so
    // they fall through to the recovery flow.
    constexpr uint32 SHAMAN_REINCARNATION_CAST = 20608;  // the cast we emit
    constexpr uint32 SHAMAN_REINCARNATION_CD   = 21169;  // the ~30-min CD spell
    constexpr uint8  CLASS_SHAMAN_LOCAL = 7;  // SharedDefines CLASS_SHAMAN
    constexpr uint32 kReincarnationFallThroughMs = 2500u;
    // Readiness gate (DMM-P1b): cast-eligibility is keyed off the castable
    // spell the bot actually has in its book (20608), while the COOLDOWN is
    // read off the CD-bearing spell (21169) via cd_remaining(), NOT is_ready().
    // We deliberately avoid is_ready(21169): is_ready() first requires
    // knows_spell(21169), but 21169 is a passive that isn't reliably present
    // in every bot's spellbook (distribution-leveled bots may have 20608 but
    // not the passive row). cd_remaining() reads the cooldown row directly
    // (CopyCooldowns records 21169's ~30-min CD whenever it's active) and
    // returns 0 when no row exists — i.e. ready. This is the exact fix for the
    // wedge: a shaman that self-rezzed <30 min ago now sees 21169 on CD and
    // skips the cast, falling straight into the release/corpse-run path.
    if (snapshot.cls() == CLASS_SHAMAN_LOCAL &&
        snapshot.knows_spell(SHAMAN_REINCARNATION_CAST) &&
        !ai.reincarnation_attempted() &&
        snapshot.cd_remaining(SHAMAN_REINCARNATION_CD).count() == 0)
    {
        // One attempt per death. Stamp the time so we can fall through if
        // the cast doesn't actually revive us within the window.
        emit.cast(SHAMAN_REINCARNATION_CAST);
        ai.set_reincarnation_attempted(true);
        ai.set_reincarnation_attempt_ms(snapshot.published_at_ms());
        ai.set_last_rule_fired("dead:reincarnation");
        return;
    }
    // DMM-P1b fall-through: we attempted Reincarnation but the snapshot still
    // shows us dead. Give the cast ~2.5s to land (is_alive flips in the branch
    // at the top of this function on success). If it hasn't by then, the cast
    // failed (no reagent / wrong state / server CD we couldn't see) — proceed
    // into the normal release/corpse-run flow below instead of wedging. We do
    // NOT clear reincarnation_attempted_ here, so we won't re-spam the cast;
    // it's cleared on the next revival.
    if (ai.reincarnation_attempted() && !ai.corpse_recovery_emitted())
    {
        const uint32 now_ms = snapshot.published_at_ms();
        const uint32 since  = ai.reincarnation_attempt_ms();
        if (since != 0 && (now_ms - since) < kReincarnationFallThroughMs)
        {
            ai.set_last_rule_fired("dead:reincarnation_pending");
            return;
        }
        // else: timed out — fall through to the recovery flow below.
    }

    // BG short-circuit (priority over the spirit-healer vs corpse-run
    // decision). In a battleground, the BG's spirit guide NPC auto-rezzes
    // every ghost in radius on a 30s cycle. The bot's correct behaviour is:
    //   1) release spirit (puts them at the BG graveyard with the spirit guide)
    //   2) stay put as a ghost — do NOT walk to corpse
    //   3) wait for spirit guide auto-rez → snapshot flips is_alive=true
    //   4) state machine demotes Dead→Idle next tick, combat resumes
    // Without this fast-path, corpse_to_graveyard_dist can read 0 or small
    // for BG deaths (the BG graveyard is on the same map close to the
    // death spot), pushing the recovery flow into the corpse-run branch,
    // which then walks the ghost to the corpse — wasted motion and the
    // corpse-run wedges in TP/WSG geometry. Always-release in BG.
    if (snapshot.in_battleground())
    {
        uint32 const bg_dead_now_ms = snapshot.published_at_ms();
        if (!ai.corpse_recovery_emitted())
        {
            emit.emit(ReleaseCorpseIntent{});
            ai.set_corpse_recovery_emitted(true);
            ai.set_release_pending_at_ms(bg_dead_now_ms);
            ai.set_last_rule_fired("dead:bg_release_spirit");
        }
        else if (!snapshot.is_ghost() && bg_dead_now_ms - ai.release_pending_at_ms() > 4000u)
        {
            // AB test 2026-10-08: dead bots stood on the ground for good. The one-shot release can fail (intent dropped, the
            // repop refused); a dead non-ghost in a BG repeats it every 4 s until the ghost state appears.
            TC_LOG_INFO("playerbot.v2", "[bg_death] {} still not a ghost 4 s after release; releasing again (map {})",
                snapshot.name(), snapshot.map_id());
            emit.emit(ReleaseCorpseIntent{});
            ai.set_release_pending_at_ms(bg_dead_now_ms);
            ai.set_last_rule_fired("dead:bg_release_retry");
        }
        else
        {
            // Released — stay put as a ghost. The spirit guide only rezzes ghosts that asked it to (a real client does that when
            // the player clicks the guide), so every 5 s the release call asks the nearest guide in range (API::release_corpse).
            if (bg_dead_now_ms - ai.release_pending_at_ms() > 5000u)
            {
                emit.emit(ReleaseCorpseIntent{});
                ai.set_release_pending_at_ms(bg_dead_now_ms);
            }
            ai.set_last_rule_fired("dead:bg_await_spirit_rez");
        }
        return;
    }

    // Recovery flow.
    //
    // DMM-P1d (2026-05-30): the spirit-healer-vs-corpse-run decision used to
    // be made HERE, pre-release. That was a dead branch: ShouldUseSpiritHealer
    // reads has_corpse() and corpse_to_graveyard_dist(), but before release no
    // corpse object exists (has_corpse()==false) and the distance is 0 — so
    // the predicate ALWAYS returned false and every PvE death took the
    // corpse-run path. The old comment claiming "before releasing the corpse
    // position is the bot's current position so the distance is a clean read"
    // was false: corpse_to_graveyard_dist is only populated by the snapshot
    // builder once a corpse exists, i.e. AFTER release. Restructure (option b
    // from the audit): ALWAYS emit ReleaseCorpseIntent in the pre-release
    // branch, then choose spirit-healer-vs-corpse-run further down, once
    // corpse_to_graveyard_dist is real.
    //
    // Release-delay 2026-05-21: instant release was a robot tell. Real
    // humans stare at the death screen for several seconds, waiting on a
    // battle-rez chat ping or a druid casting Rebirth out of LOS.
    // Capture a per-bot release-at timestamp on first observation of
    // this death (release_pending_at_ms_ == 0 means "fresh"); only emit
    // ReleaseCorpseIntent once now_ms >= that + 3–7s jitter. The waiting tick
    // still consumes the accept-rez branch above and the group-mid-cast hold
    // below, so a late incoming res is still received.
    if (!ai.corpse_recovery_emitted() && (release_ordered || owner_released))
    {
        // Owner's order, or the owner released and is running back: release now
        // (no death-screen jitter, no waiting on a rez) and follow the run back.
        TC_LOG_INFO("playerbot.v2", "[death-recovery] {} releases spirit ({}), corpse on map {}",
            snapshot.name(), release_ordered ? "release order" : "owner released", snapshot.map_id());
        emit.emit(ReleaseCorpseIntent{});
        ai.set_corpse_recovery_emitted(true);
        ai.set_release_pending_at_ms(snapshot.published_at_ms());
        ai.set_last_rule_fired(release_ordered ? "dead:release_ordered" : "dead:release_with_owner");
        return;
    }
    if (!ai.corpse_recovery_emitted())
    {
        const uint32 now_ms = snapshot.published_at_ms();
        uint32 release_at = ai.release_pending_at_ms();
        if (release_at == 0)
        {
            const uint32 jitter =
                3000u + (uint32(snapshot.bot_id()) * 2654435761u) % 4000u;
            release_at = now_ms + jitter;
            ai.set_release_pending_at_ms(release_at);
            ai.set_last_rule_fired("dead:release_pending");
            return;
        }
        if (now_ms < release_at)
        {
            ai.set_last_rule_fired("dead:release_pending");
            return;
        }
        // Survivors can rez us in place — do NOT release. Releasing teleports our
        // ghost to the zone graveyard, which for Deadmines (zone 1581) is the
        // Westfall map-0 GY: that ejects us from the instance, drops dungeon
        // run_mode on the map change, and we wander off. Instead hold at the
        // corpse and let a surviving healer's out-of-combat MaintainOocRez raise
        // us once it clears combat (mirrors a real group waiting to rez its tank).
        // The watchdog above uses release_cap = kGroupRezWaitMs while group_can_rez,
        // so a survivor that never actually rezzes (no rezzer alive / stuck) still
        // force-releases us at the backstop — this can't wedge forever.
        if (group_can_rez)
        {
            ai.set_last_rule_fired("dead:waiting_group_rez");
            log_waiting("waiting for a rez from a living group healer nearby");
            return;
        }
        // Always release. The spirit-healer choice happens post-release,
        // once a corpse exists and corpse_to_graveyard_dist is meaningful.
        TC_LOG_INFO("playerbot.v2", "[death-recovery] {} releases spirit ({}), corpse on map {}",
            snapshot.name(), wipe ? "wipe" : "no rezzer nearby", snapshot.map_id());
        emit.emit(ReleaseCorpseIntent{});
        ai.set_corpse_recovery_emitted(true);
        ai.set_last_rule_fired("dead:release_corpse");
        return;
    }

    // We've already released. If we're not yet a ghost, the release intent
    // hasn't drained (or the teleport ack hasn't fired yet). Normally one or
    // two ticks — DriveTeleportAck in OnWorldUpdate finalizes it.
    if (!snapshot.is_ghost())
    {
        // Stale-latch guard (companion to the death-entry reset in BotAI.cpp).
        // corpse_recovery_emitted_ claims we released, but a legitimate release
        // reaches ghost state within ~1-2 ticks. If we've been "released but not
        // a ghost" well past that, the latch is stale: an unobserved reclaim->
        // re-death (the bot revived at 50% HP in the killing camp and re-died
        // before the snapshot — dead bots tick at Hibernate ~2s cadence, so the
        // brief alive frame was missed) left corpse_recovery_emitted_ set for a
        // PREVIOUS corpse. The state never left Dead, so the BotAI death-entry
        // reset never fired either. Re-arm here so the release branch fires for
        // the CURRENT corpse instead of wedging forever (Bramwell L4 Elwynn).
        const uint32 now_ms = snapshot.published_at_ms();
        const uint32 rel    = ai.release_pending_at_ms();
        constexpr uint32 kReleaseToGhostTimeoutMs = 4000u;
        if (rel != 0 && now_ms > rel && (now_ms - rel) > kReleaseToGhostTimeoutMs)
        {
            ai.set_corpse_recovery_emitted(false);
            ai.set_release_pending_at_ms(0);
            ai.set_ghost_since_ms(0);
            ai.set_corpse_run_last_dist(-1.0f);
            ai.set_last_rule_fired("dead:rerelease_stale_latch");
            return;
        }
        ai.set_last_rule_fired("dead:waiting_release");
        return;
    }

    // BG short-circuit: in a battleground the spirit-guide at the
    // graveyard auto-resurrects all ghosts in range every 30s — no
    // corpse-run needed. Walking the ghost to its corpse can wedge in
    // TP/WSG geometry (mid-air corpses, off-map graves) and the
    // watchdog-escape used to kick ghosts further out of bounds. Best
    // behavior in BG: stay at the graveyard as a ghost, the BG handles
    // the rez. Once alive, the snapshot's is_alive flips true and the
    // state machine demotes Dead→Idle on the next tick.
    if (snapshot.in_battleground())
    {
        ai.set_last_rule_fired("dead:bg_await_spirit_rez");
        return;
    }

    // Ghost reached the graveyard. Now we're in the corpse-run loop.
    // 1. Verify we still have a corpse to walk back to. If a teammate
    //    accepts a rez or Spawn-bones happened, has_corpse will go false
    //    and we'll wait for is_alive to flip.
    if (!snapshot.has_corpse())
    {
        ai.set_last_rule_fired("dead:waiting_revive");
        return;
    }

    // ---- Death-spiral escape (CRITICAL repair path) ----
    // A 0%-durability bot dies to trivial mobs, corpse-runs straight back into
    // the same camp (broken gear guts its stats), and dies again — perpetually
    // InCombat/Dead, never OOC, so the opportunistic repair rules never fire.
    // When the bot is genuinely spiralling (≥2 same-spot open-world deaths) OR
    // its gear is critically broken (≤10% — the same threshold the vendor-FSM
    // critical band uses), PREFER a spirit-healer / graveyard rez over the
    // corpse run that would walk it back into the camp, and set a post-rez "go
    // repair" goal so it fixes its gear instead of resuming the objective POI.
    //
    // GROUP/DUNGEON/BG GATED OUT entirely: in a group or instance or BG the bot
    // must follow the normal corpse-run / battle-rez / instance-regroup flow (a
    // coordinated wipe is not a broken-gear spiral). The spiral COUNTER itself
    // is already open-world-only (BotAI::tick), so this is belt-and-braces.
    const bool spiral_escape_allowed =
        !group.exists() && !snapshot.is_in_instance() && !snapshot.in_battleground();
    const bool death_spiralling =
        spiral_escape_allowed &&
        (ai.consecutive_same_spot_deaths() >= 2 ||
         snapshot.lowest_equipped_durability_pct() <= 10);

    // DMM-P1d: spirit-healer-vs-corpse-run decision, made HERE (post-release)
    // because only now is a corpse object live and corpse_to_graveyard_dist
    // populated by the snapshot builder. ShouldUseSpiritHealer returns true
    // for L11+ bots whose corpse run exceeds kSpiritHealerDistanceThreshold
    // (600y) — too long to be worth the walk vs. the sickness hit. The
    // SpiritResurrect intent does ResurrectPlayer(0.5f, sickness=true),
    // durability hit, spawns bones and teleports to the corpse graveyard;
    // is_alive flips true next tick and the state demotes Dead→Idle.
    //
    // Cross-map dungeon deaths are intentionally NOT routed here: their
    // corpse_to_graveyard_dist is cross-realm-meaningless, and the
    // server-side walk-through-portal auto-rez (below) is the cheaper, no-
    // sickness recovery. The cross-map block runs first and returns.
    if (ShouldUseSpiritHealer(snapshot) || death_spiralling)
    {
        const uint32 cmap_chk = snapshot.raw().death.corpse_map_id;
        const uint32 cinst_chk = snapshot.raw().death.corpse_instance_id;
        const uint32 binst_chk = snapshot.raw().position.instance_id;
        const bool cross_map_chk =
            (cmap_chk != 0 && cmap_chk != snapshot.map_id()) ||
            (cmap_chk != 0 && cmap_chk == snapshot.map_id() && cinst_chk != binst_chk);
        if (!cross_map_chk)
        {
            // Re-arm via ghost_since_ms_ so we emit at most once per ~5s
            // window instead of every tick while the async resurrect lands.
            // (ghost_since_ms_ is otherwise the corpse-run no-progress timer;
            // safe to share — the corpse-run path below is unreachable once
            // we take the spirit-healer branch and return.)
            const uint32 now_ms = GameTime::GetGameTimeMS();
            constexpr uint32 kSpiritReArmMs = 5000u;
            if (ai.ghost_since_ms() == 0 ||
                (now_ms - ai.ghost_since_ms()) > kSpiritReArmMs)
            {
                emit.emit(SpiritResurrectIntent{});
                ai.set_ghost_since_ms(now_ms);
                // Death-spiral escape: after the graveyard rez, route to a
                // repair vendor instead of resuming the objective POI that put
                // the bot back in the camp. set_manual_travel synthesizes a
                // current_objective_poi (same pipeline as /goto + R7 reloc), so
                // the full bounded travel composer drives the bot there — never
                // a teleport-rescue. BROKE GUARD: if the bot can't afford the
                // repair (gold < estimated cost), do NOT set a goal — that would
                // wedge it at a vendor it can't use. It just spirit-heals and
                // resumes normal behavior (which will look for cheaper recovery /
                // resume objectives); the spiral counter caps the loop.
                if (death_spiralling)
                {
                    const int32  gold = snapshot.gold();
                    const uint32 cost = snapshot.estimated_repair_cost();
                    const bool   can_afford = cost == 0 || uint64(gold) >= cost;
                    if (can_afford)
                    {
                        uint32 rmap = 0; float rx = 0.f, ry = 0.f, rz = 0.f; bool have = false;
                        if (auto hit = Services::RepairVendors().GetNearestRepairVendor(snapshot.raw()))
                        { rmap = hit->map_id; rx = hit->x; ry = hit->y; rz = hit->z; have = true; }
                        else if (auto const* hub = Services::Hubs().GetNearestQuestHub(snapshot.raw()))
                        {
                            if (hub->mapId == snapshot.map_id())
                            {
                                rmap = hub->mapId;
                                rx = hub->location.GetPositionX();
                                ry = hub->location.GetPositionY();
                                rz = hub->location.GetPositionZ();
                                have = true;
                            }
                        }
                        if (have)
                            ai.set_manual_travel(rmap, rx, ry, rz);
                    }
                }
            }
            ai.set_last_rule_fired(death_spiralling ? "dead:spiral_spirit_resurrect"
                                                    : "dead:spirit_resurrect");
            return;
        }
    }

    // Cross-map ghost recovery (dungeon death). TC's RepopAtGraveyard
    // teleports the ghost to the INSTANCE'S EXTERIOR graveyard (outside
    // the portal); the corpse stays inside. Walking the ghost back
    // through the dungeon portal teleports them into the instance AND
    // auto-resurrects them at full HP (TC handles this server-side —
    // no sickness, no SpiritResurrect required).
    //
    // The portal's parent-map XY is `MapEntry::GetEntrancePos` for the
    // corpse's map_id. Snapshot builder pre-resolves this into
    // instance_entrance_* whenever ghost-and-cross-map (otherwise
    // those fields point to the entrance of the bot's CURRENT map).
    // Without this rule, the walk-to-corpse code below treats the
    // cross-map corpse coords as same-map and pathfinds to bogus X/Y.
    // Observed 2026-05-15: bots released, stranded outside dungeon.
    // Cross-Map detection covers two cases:
    //   1. Different map_id (typical: died in dungeon, ghost ported out
    //      to continent graveyard but corpse stayed inside).
    //   2. Same map_id but different *Map* instance (corpse is in a
    //      dungeon-instance Map* with same parent map_id as the bot's
    //      current continent Map*, OR bot is in an instance and corpse
    //      is on the continent shell). Without this, reclaim_corpse
    //      fires every tick against an unreachable corpse — the
    //      "0.0y > 39y" log spam observed 2026-05-17.
    const uint32 cmap = snapshot.raw().death.corpse_map_id;
    const uint32 cinst = snapshot.raw().death.corpse_instance_id;
    const uint32 binst = snapshot.raw().position.instance_id;
    const bool cross_map = (cmap != 0 && cmap != snapshot.map_id()) ||
                           (cmap != 0 && cmap == snapshot.map_id() && cinst != binst);
    if (cross_map)
    {
        // Preferred: the instance's entrance area trigger on this map (resolved
        // by the snapshot builder). Walk into it and cross it — what a client's
        // CMSG_AREA_TRIGGER does — so the server ports the ghost inside, where
        // the worldport ack revives it at its corpse (a raid wipe in Molten Core:
        // graveyard -> Blackrock Mountain -> the MC portal -> alive at the body).
        // Bounded: no progress toward the trigger for kPortalNoProgressMs falls
        // back to the spirit healer (alive outside; the group-instance rejoin
        // brings the bot back to its group from there).
        if (const uint32 at_id = snapshot.raw().death.ghost_portal_at_id)
        {
            const float px = snapshot.raw().death.ghost_portal_x;
            const float py = snapshot.raw().death.ghost_portal_y;
            const float pz = snapshot.raw().death.ghost_portal_z;
            float bx, by, bz; snapshot.position(bx, by, bz);
            const float pdx = px - bx, pdy = py - by, pdz = pz - bz;
            const float pd = std::sqrt(pdx * pdx + pdy * pdy);
            const uint32 pnow = GameTime::GetGameTimeMS();
            constexpr uint32 kPortalNoProgressMs = 180000u;
            if (ai.portal_run_last_dist() < 0.0f || pd < ai.portal_run_last_dist() - 5.0f ||
                ai.portal_run_since_ms() == 0)
            {
                if (ai.portal_run_last_dist() < 0.0f)
                    TC_LOG_INFO("playerbot.v2",
                        "[death-recovery] {} ghost runs to the entrance trigger {} at ({:.1f},{:.1f},{:.1f}) map {}, {:.0f}y away; corpse on map {}",
                        snapshot.name(), at_id, px, py, pz, snapshot.map_id(), pd, cmap);
                ai.set_portal_run(pd, pnow);
            }
            if (pnow - ai.portal_run_since_ms() > kPortalNoProgressMs)
            {
                TC_LOG_INFO("playerbot.v2",
                    "[death-recovery] {} ghost made no progress to entrance trigger {} for {}s ({:.0f}y left): spirit healer",
                    snapshot.name(), at_id, kPortalNoProgressMs / 1000u, pd);
                emit.emit(SpiritResurrectIntent{});
                ai.set_portal_run(pd, pnow);   // re-arm: one emit per window while it lands
                ai.set_last_rule_fired("dead:portal_no_progress_spirit_resurrect");
                return;
            }
            // Inside (or at the edge of) the trigger volume: cross it. The server
            // re-checks the volume, so a near miss is a harmless OutOfRange and the
            // walk below continues. Re-emitted at most every 2s while the port lands.
            // The trigger centre can hang over a lava lake (the Molten Core
            // entrances over the Blackrock Mountain lava) while its volume
            // reaches the dry ledge, so try from 12y out (25y vertically); a
            // near miss costs one OutOfRange per 2s.
            if (pd <= 12.0f && std::fabs(pdz) <= 25.0f)
            {
                if (ai.portal_enter_ms() == 0 || pnow - ai.portal_enter_ms() >= 2000u)
                {
                    emit.emit(ReclaimCorpseIntent{at_id});
                    ai.set_portal_enter_ms(pnow ? pnow : 1u);
                }
                ai.set_last_rule_fired("dead:ghost_enter_instance");
                return;
            }
            emit.move_to(px, py, pz, /*run=*/true);
            ai.set_last_rule_fired("dead:ghost_walk_to_portal");
            return;
        }
        const uint32 entrance_map = snapshot.raw().dungeon_exec.instance_entrance_map;
        const float  ex = snapshot.raw().dungeon_exec.instance_entrance_x;
        const float  ey = snapshot.raw().dungeon_exec.instance_entrance_y;
        const float  ez = snapshot.raw().dungeon_exec.instance_entrance_z;
        if (entrance_map == snapshot.map_id() &&
            (ex != 0.f || ey != 0.f))
        {
            float bx, by, bz; snapshot.position(bx, by, bz);
            const float dx = ex - bx, dy = ey - by;
            const float d2 = dx * dx + dy * dy;
            // Within 3y of the portal coords: the area-trigger /
            // teleport-on-enter has fired (or will fire next tick).
            // Stop emitting move_to so we don't keep walking past.
            if (d2 > 9.0f)
            {
                emit.move_to(ex, ey, ez, /*run=*/true);
                ai.set_last_rule_fired("dead:ghost_walk_to_portal");
                return;
            }
            // At the entrance but no entrance trigger was resolved for this
            // map (ghost_portal_at_id above), so nothing ports the ghost in.
            // Bounded: after 30s here take the spirit healer instead of
            // standing at the portal forever.
            const uint32 anow = GameTime::GetGameTimeMS();
            if (ai.portal_enter_ms() == 0)
                ai.set_portal_enter_ms(anow ? anow : 1u);
            else if (anow - ai.portal_enter_ms() > 30000u)
            {
                TC_LOG_INFO("playerbot.v2",
                    "[death-recovery] {} at the instance entrance on map {} with no entrance trigger to cross: spirit healer",
                    snapshot.name(), snapshot.map_id());
                emit.emit(SpiritResurrectIntent{});
                ai.set_portal_enter_ms(anow ? anow : 1u);
                ai.set_last_rule_fired("dead:ghost_at_portal_spirit_resurrect");
                return;
            }
            ai.set_last_rule_fired("dead:ghost_at_portal");
            return;
        }
        // DMM-P2a: no entrance info — the snapshot builder couldn't resolve
        // the portal coords (corpse on a map with no DBC CorpseMapID, or an
        // instance whose entrance lives on a different parent map than ours).
        // The OLD code "fell through" to the bottom of the function, which
        // (a) waited the full level-tiered timeout — up to 5 min at max level
        // — before SpiritResurrect, and (b) kept emitting ReclaimCorpseIntent
        // / move_to against the unreachable cross-map corpse every tick (the
        // "0.0y > 39y" spam). Handle it here instead: there is no reachable
        // corpse and no portal to walk to, so the ONLY recovery is the spirit
        // healer. Use a short ~25s timeout (ghost_since_ms_ as the clock) then
        // SpiritResurrect, and emit nothing toward the corpse in the meantime.
        const uint32 now_ms = GameTime::GetGameTimeMS();
        if (ai.ghost_since_ms() == 0)
            ai.set_ghost_since_ms(now_ms);
        constexpr uint32 kCrossMapUnresolvedTimeoutMs = 25u * 1000u;
        if (now_ms - ai.ghost_since_ms() > kCrossMapUnresolvedTimeoutMs)
        {
            emit.emit(SpiritResurrectIntent{});
            // Re-arm so we emit once per timeout window rather than every
            // tick while the async resurrect lands.
            ai.set_ghost_since_ms(now_ms);
            ai.set_last_rule_fired("dead:crossmap_unresolved_spirit_resurrect");
            return;
        }
        // Within the grace window — wait. Crucially do NOT fall through to
        // the reclaim/walk block (that corpse is cross-map unreachable).
        ai.set_last_rule_fired("dead:crossmap_unresolved_wait");
        return;
    }

    // 2. Wait for the corpse-reclaim window to open (default 30s after
    //    release). HandleReclaimCorpse rejects until ghost-time + delay
    //    has elapsed; emitting reclaim early is wasted intent traffic.
    const int64 now = static_cast<int64>(GameTime::GetGameTime());
    if (snapshot.corpse_reclaim_at_unix() > now)
    {
        // Walk closer while we wait so we're in range when the window opens.
        // Fall through to the move/reclaim block below.
    }

    // 3. Distance to corpse. Same map check is implicit — RepopAtGraveyard
    //    drops the ghost on the map containing the corpse for any normal
    //    death (cross-map dies route to homebind via the dungeon-exit path).
    const float dx = snapshot.corpse_x() - snapshot.raw().position.x;
    const float dy = snapshot.corpse_y() - snapshot.raw().position.y;
    const float dz = snapshot.corpse_z() - snapshot.raw().position.z;
    const float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

    if (dist <= kCorpseApproachRadius && snapshot.corpse_reclaim_at_unix() <= now)
    {
        // 4. In range and reclaim window open. BEFORE reclaiming, check who
        //    is standing on the corpse: reviving at 50% HP in the middle of
        //    the camp that killed the bot re-aggroes instantly and produces a
        //    permanent death loop (verified live: Zorinus, 5 deaths at the
        //    same spot). A human ghost-walks to the EDGE of aggro range and
        //    rezzes there — mirror that: if a live hostile sits within 20y of
        //    the corpse, shift the reclaim spot ~30y away from the nearest
        //    threat (still inside the 39y reclaim radius) and only reclaim
        //    once the bot stands clear of the camp.
        {
            NearbyUnit const* nearest_threat = nullptr;
            float nearest_threat_dsq = 20.0f * 20.0f;
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
            {
                if (!u.guid.IsCreature()) continue;
                if (u.hp <= 0) continue;
                if (u.no_xp_kill || u.is_pacified) continue; // dummies/props aren't a threat
                const float tdx = u.x - snapshot.corpse_x();
                const float tdy = u.y - snapshot.corpse_y();
                const float tdsq = tdx*tdx + tdy*tdy;
                if (tdsq < nearest_threat_dsq) { nearest_threat_dsq = tdsq; nearest_threat = &u; }
            }
            if (nearest_threat)
            {
                // Safe spot: corpse position pushed 30y directly away from
                // the threat. Reclaim radius is 39y, so this stays valid.
                float ax = snapshot.corpse_x() - nearest_threat->x;
                float ay = snapshot.corpse_y() - nearest_threat->y;
                const float alen = std::sqrt(ax*ax + ay*ay);
                if (alen > 0.1f) { ax /= alen; ay /= alen; } else { ax = 1.f; ay = 0.f; }
                const float sx = snapshot.corpse_x() + ax * 30.0f;
                const float sy = snapshot.corpse_y() + ay * 30.0f;
                const float sdx = sx - snapshot.raw().position.x;
                const float sdy = sy - snapshot.raw().position.y;
                if (sdx*sdx + sdy*sdy > 6.0f * 6.0f)
                {
                    emit.move_to(sx, sy, snapshot.corpse_z(), /*run*/ true);
                    ai.set_last_rule_fired("dead:reclaim_offset_from_camp");
                    return;
                }
                // Standing at the safe offset — reclaim from here (39y range).
            }
        }
        // Revive with the owner (ike3 ReviveFromCorpseAction): while the owner's
        // ghost is running back close by, wait at the corpse and stand up
        // together once they revive. Bounded so a stalled owner never holds the
        // bot dead for good.
        if (owner && !owner->is_alive && owner->is_ghost && owner->map_id == snapshot.map_id())
        {
            float bx, by, bz; snapshot.position(bx, by, bz);
            const float odx = owner->x - bx, ody = owner->y - by;
            const uint32 onow = GameTime::GetGameTimeMS();
            constexpr float kOwnerNearY = 80.0f;
            constexpr uint32 kOwnerWaitMs = 120000u;
            if (odx * odx + ody * ody <= kOwnerNearY * kOwnerNearY)
            {
                if (ai.owner_wait_since_ms() == 0)
                    ai.set_owner_wait_since_ms(onow ? onow : 1u);
                if (onow - ai.owner_wait_since_ms() < kOwnerWaitMs)
                {
                    log_waiting("at corpse, waiting for the owner to revive");
                    ai.set_last_rule_fired("dead:waiting_owner_revive");
                    return;
                }
            }
        }
        if (ai.death_log_ms() != 1u)
        {
            // One line per corpse reached (1 marks "logged"; the rate limiter
            // above treats it as long past).
            TC_LOG_INFO("playerbot.v2", "[death-recovery] {} reached its corpse ({:.0f}y), reviving",
                snapshot.name(), dist);
            ai.set_death_log_ms(1u);
        }
        emit.emit(ReclaimCorpseIntent{});
        ai.set_last_rule_fired("dead:reclaim_corpse");
        return;
    }

    // 5. Walk to the corpse. move_to is idempotent — emitting the same
    //    target every tick is fine; the motion master only re-pathfinds
    //    if the destination changed beyond a small threshold.
    //
    // Stuck-ghost fallback: if the bot has been a ghost for more than
    // <timeout> without reaching its corpse, the path from graveyard to
    // corpse is probably blocked (corpse fell off a cliff, despawn-grid
    // unreachable, water/terrain Detour can't solve, the spirit healer
    // sits on a non-navmesh WMO platform, etc). Force the spirit-healer
    // path so the bot resurrects (with sickness if L>10) instead of
    // wandering as a ghost forever and leaving an unclaimed corpse on
    // the map.
    //
    // Level-tiered timeout (2026-05-21): the original 5-minute window
    // was tuned for max-level bots. For starter-zone bots (Teldrassil
    // L1-10) it's pathological — Uraimus observed stuck at the
    // Teldrassil spirit healer with /whyidle showing "walking_to_corpse"
    // but no movement, because the spirit healer perch isn't on the
    // navmesh and API::move_to keeps returning Locked. 5 min of ghosting
    // before the escape fires is far longer than the user-experience
    // budget for low-level questing. Map sizes also shrink for low-level
    // zones, so a real corpse-run there completes in well under 60s.
    //
    // DMM-P3a (2026-05-30): the timeout used to be measured from when the
    // bot FIRST became a ghost (ghost_since_ms_ stamped once, never reset).
    // That force-SpiritResurrected NORMAL corpse runs: a max-level bot with
    // a legitimately long (but progressing) run would hit the 5-min wall and
    // get yanked to the spirit healer — eating sickness it didn't need — even
    // though it was steadily closing on its corpse. Fix: treat ghost_since_ms_
    // as a NO-PROGRESS timer. Each tick, if the ghost has moved meaningfully
    // closer to its corpse since the last sample, reset the timer (and the
    // tracked distance). Only when the bot has made no meaningful progress for
    // the full window — i.e. it's genuinely stuck (blocked path, off-navmesh
    // perch, corpse fell off geometry) — do we SpiritResurrect.
    const uint8 lvl = snapshot.level();
    const uint32 kGhostStuckTimeoutMs =
        (lvl > 0 && lvl <= 10) ? (30u * 1000u)        // 30s starter zones
      : (lvl <= 30)            ? (90u * 1000u)        // 90s low-mid level
                               : (5u * 60u * 1000u);  // 5 min everywhere else
    const uint32 ghost_now_ms = GameTime::GetGameTimeMS();
    // "Meaningful" progress: at least 5y closer than the last sampled distance.
    // Small enough to credit a steadily-advancing run every couple of seconds,
    // large enough that navmesh jitter / standing-still drift doesn't count.
    constexpr float kProgressEpsilonY = 5.0f;
    const float last_dist = ai.corpse_run_last_dist();
    if (last_dist < 0.0f || dist < last_dist - kProgressEpsilonY)
    {
        // First sample this run, or real progress — (re)baseline the timer.
        ai.set_corpse_run_last_dist(dist);
        ai.set_ghost_since_ms(ghost_now_ms);
    }
    else if (ai.ghost_since_ms() == 0)
    {
        // Defensive: timer was cleared (e.g. by the revival reset) but we
        // already have a baseline distance. Re-arm without crediting progress.
        ai.set_ghost_since_ms(ghost_now_ms);
    }
    if (ghost_now_ms - ai.ghost_since_ms() > kGhostStuckTimeoutMs)
    {
        emit.emit(SpiritResurrectIntent{});
        // Re-arm the no-progress window after each emit so the rule fires
        // once per timeout cycle instead of every tick. The intent may be
        // executed asynchronously and the snapshot's is_alive flips one
        // tick later — without this, the rule re-fires at ~5Hz and floods
        // the executor while waiting for the resurrect to land server-side.
        ai.set_ghost_since_ms(ghost_now_ms);
        ai.set_last_rule_fired("dead:ghost_no_progress_spirit_resurrect");
        return;
    }
    emit.move_to(snapshot.corpse_x(), snapshot.corpse_y(), snapshot.corpse_z(), /*run*/ true);
    ai.set_last_rule_fired("dead:walking_to_corpse");
}

} // namespace Playerbot::States
