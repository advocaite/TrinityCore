// LootDrainRules - Refactor #3 pass 10. Migrates the pending-loot drain
// (idle:loot_corpse + idle:move_to_corpse) out of the State_Idle cascade.
// OnDeath queues tagged-but-unreachable corpses in BotRegistry; this rule
// pops the closest live entry per tick: in-range -> emit LootIntent, then
// (PROF-P2b) for skinnable corpses keep the entry queued and SEQUENCE the
// skinning cast after the corpse loot is released (the skinning spell fails
// "not looted" until then), looting the resulting skinning loot, before
// finally popping. Out-of-range -> emit MoveTo. Stale GUIDs are popped
// silently. Skipped when bags are full.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "Bot/States/MaintainHelpers.h"   // States::DungeonLootDrain
#include "Bot/ClassicUpkeep.h"
#include "Services.h"
#include "Bot/BotRegistry.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Creature.h"
#include "CreatureData.h"
#include "Loot.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

#include <cmath>
#include <deque>

namespace Playerbot {

namespace {

constexpr float kLootRange = 11.0f;
// A human leads an instance group: bots walk only to corpses this close to him (the rest wait in the queue).
constexpr float kHumanLootLeashYd = 20.0f;
// A bot tank leads: bots walk only to corpses this close to the anchor tank (rejoin_tank pulls a follower back at 40 y).
constexpr float kTankLootLeashYd = 28.0f;
// A corpse with an idle hostile mob this close is not walked to (the walk would pull it).
constexpr float kLootIdleMobClearYd = 16.0f;
constexpr uint16 SKILL_SKINNING_LOCAL = 393;

// PROF-P2b: resolve the rank-appropriate skinning cast spell from the bot's
// own spellbook instead of hardcoding 8613 (classic Apprentice Skinning).
// The player-cast skinning action is the known spell carrying
// SPELL_EFFECT_SKINNING; across expansions a bot may know a different /
// higher-rank variant. We scan once per call (only reached for skinnable
// corpses with a skinner present) and fall back to 8613 if the spellbook
// scan finds nothing (e.g. data quirk) so behaviour never regresses.
uint32 ResolveSkinningSpell(Player* p)
{
    constexpr uint32 APPRENTICE_SKINNING = 8613;   // universal fallback
    for (auto const& [sid, ps] : p->GetSpellMap())
    {
        if (ps.state == PLAYERSPELL_REMOVED || ps.disabled)
            continue;
        SpellInfo const* si = sSpellMgr->GetSpellInfo(sid, DIFFICULTY_NONE);
        if (!si) continue;
        for (SpellEffectInfo const& eff : si->GetEffects())
            if (uint32(eff.Effect) == uint32(SPELL_EFFECT_SKINNING))
                return sid;
    }
    return APPRENTICE_SKINNING;
}

bool LootDrainGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    // Told to stay: the corpse can wait until it is released.
    if (ai.is_manual_mode(s.published_at_ms()) && ai.owner_command() == BotAI::OwnerCommand::Stay)
        return false;
    // A dungeon mana user under the pull-ready mana drinks instead of walking to corpses (the tank's pull waits for that mana, and a
    // walking caster never sits): the healer mark for a healer, the DPS mark for the rest, as the tank's gate reads them. This one check
    // serves the registered rule and DungeonLootDrain. deadmines_full c82a7e21, Scnpriest under 10% mana at t=383-404 s walked to
    // corpses and back to the tank without drinking.
    if (s.is_in_instance() && s.max_power(POWER_MANA) > 0 && Classic::DrinksForPull(s, &g)
        && s.power_pct(POWER_MANA) < Classic::OocDrinkStartManaPct(s, &g, 0)
        && Classic::BestConsumable(s, Classic::ConsumableKind::Drink))   // no drink in the bags: it loots rather than waits for nothing
        return false;
    // Loot etiquette with a human in the group (the owner's AddLootAction): no
    // looting while the bot or a human of the group on its map is still
    // fighting. The queue keeps the corpses for after the fight.
    if (auto const* members = g.members())
        for (GroupMemberSummary const& m : *members)
            if (!m.is_bot && m.online && m.map_id == s.map_id() && m.guid != s.guid()
                && (m.in_combat || s.in_combat()))
                return false;
    // L-P0a: do NOT bail on full bags. A real player still loots the GOLD and
    // any stackable that fits even with a near-full inventory, and abandoning
    // the corpse permanently forfeits that gold + loot (the queue entry is
    // popped, never revisited). loot_corpse takes gold first and StoreLootItem
    // safely leaves non-fitting items on the corpse. Bag pressure is surfaced
    // to the vendor FSM separately (vendor_visit_phases bag-full bit derived
    // from bag_free_slots), so urgency to go sell still escalates — we just
    // don't throw away loot in the meantime.
    if (Services::Registry().peek_loot_size(s.bot_id()) <= 0)
    {
        // Queue drained — reset the "look at the body" pause so the
        // next death gets a fresh hesitation window.
        if (ai.pending_loot_drain_at_ms() != 0)
            ai.set_pending_loot_drain_at_ms(0);
        return false;
    }
    // First-corpse-in-chain hesitation: 300–1500ms. Movement towards a
    // far corpse is allowed (the Fire branch picks move_to or loot based
    // on distance), so we apply the pause inside Fire instead — see
    // LootDrainFire below for the in-range gate.
    return true;
}

bool LootDrainFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    Player* p = ObjectAccessor::FindConnectedPlayer(s.raw().guid);
    if (!p) return false;
    float bx, by, bz; s.position(bx, by, bz);
    bool emitted = false;
    // Guard for the Phase-2 rotate: remembers the first corpse rotated to
    // the back this tick so the drain loop can't spin on a single corpse.
    ObjectGuid rotate_sentinel;
    Services::Registry().with_loot(s.bot_id(),
        [&](std::deque<ObjectGuid>& q)
        {
            while (!q.empty())
            {
                Creature* c = ObjectAccessor::GetCreature(*p, q.front());
                if (!c || !c->IsInWorld()) { q.pop_front(); continue; }
                const float dx = c->GetPositionX() - bx;
                const float dy = c->GetPositionY() - by;
                const float dz = c->GetPositionZ() - bz;
                const float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                if (dist <= kLootRange)
                {
                    // First-in-chain hesitation: when we're in range but
                    // haven't yet emitted a loot for this drain sequence,
                    // arm a 300–1500ms pause. Subsequent corpses in the
                    // same chain skip the pause (slot is non-zero and
                    // past ready_at), only the leading corpse waits.
                    const uint32 now_ms2 = s.published_at_ms();
                    // A4: hard per-corpse deadline. The loot drain wedges when a
                    // corpse re-presents "pending" loot every tick (un-taken gold,
                    // an unwanted quest item, or — bags full — an item that won't
                    // fit): Phase-1 LootIntent re-fires forever and the Phase-2
                    // skin is never reached. idle:loot_corpse was the #1 Watchdog
                    // hot-loop (one bot+corpse pair repeated 12,465x). After
                    // kLootCorpseDeadlineMs of continuous attempts, give up on the
                    // corpse and move to the next queued one. (12s comfortably
                    // covers a legit loot->release->skin->skin-loot sequence.)
                    const uint64 corpse_low0 = c->GetGUID().GetCounter();
                    if (ai.loot_corpse_overdue(corpse_low0, now_ms2))
                    {
                        // SIGNAL, not silent: a corpse we couldn't drain within
                        // the deadline is abandoned — log WHY (once per give-up)
                        // so a genuine loot/skin bug stays visible at INFO. This
                        // REPLACES the old per-tick [skin_dbg] flood (8.27M lines)
                        // with one actionable line per stuck corpse; the full
                        // per-tick [skin_dbg] remains available at DEBUG.
                        Loot* gl = c->GetLootForPlayer(p);
                        TC_LOG_INFO("playerbot.v2",
                            "[loot_give_up] {} corpse={} entry={} gold={} items_left={} "
                            "skinnable={} can_skin={} bag_free={}",
                            s.name(), corpse_low0, c->GetEntry(),
                            gl ? gl->gold : 0u, gl ? uint32(gl->items.size()) : 0u,
                            c->HasUnitFlag(UNIT_FLAG_SKINNABLE) ? 1 : 0,
                            (p->HasSkill(SKILL_SKINNING_LOCAL) && c->GetCreatureDifficulty()
                                && c->GetCreatureDifficulty()->SkinLootID != 0) ? 1 : 0,
                            s.bag_free_slots());
                        ai.clear_loot_corpse_seen(corpse_low0);
                        q.pop_front();
                        continue;
                    }
                    uint32 ready_at = ai.pending_loot_drain_at_ms();
                    if (ready_at == 0)
                    {
                        const uint32 jitter =
                            300u + (uint32(s.bot_id()) * 2654435761u) % 1200u;
                        ai.set_pending_loot_drain_at_ms(now_ms2 + jitter);
                        return;
                    }
                    if (now_ms2 < ready_at) return;

                    // PROF-P2b: skinning is a separate, sequenced step from
                    // looting — NOT a same-tick opportunistic cast.
                    //   - The SPELL_EFFECT_SKINNING precheck (Spell.cpp:6396/
                    //     6401) requires UNIT_FLAG_SKINNABLE *and* the normal
                    //     corpse loot to be fully looted (SPELL_FAILED_TARGET_
                    //     NOT_LOOTED otherwise). The old code cast skinning the
                    //     same tick it emitted LootIntent (which also
                    //     SendLootRelease's the corpse) and then popped the
                    //     corpse unconditionally — so the skin reliably failed
                    //     "not looted" and the corpse was discarded, losing the
                    //     hide forever.
                    // New flow, driven by live creature loot state, keeping the
                    // corpse queued until every phase is done:
                    //   Phase 1: normal loot still present  -> LootIntent, keep.
                    //   Phase 2: looted + skinnable + not yet skinned -> cast
                    //            skinning (rank-appropriate spell), keep.
                    //   Phase 3: skinning loot present       -> LootIntent, keep.
                    //   Done:    nothing left / not skinnable -> pop.
                    const bool can_skin =
                        p->HasSkill(SKILL_SKINNING_LOCAL) &&
                        c->GetCreatureDifficulty() &&
                        c->GetCreatureDifficulty()->SkinLootID != 0;
                    const bool is_skinnable =
                        can_skin && c->HasUnitFlag(UNIT_FLAG_SKINNABLE) &&
                        !c->HasUnitFlag3(UNIT_FLAG3_ALREADY_SKINNED);
                    // Diagnostic: why a looted skinnable corpse doesn't get
                    // skinned (skinLootId==0 ⇒ wrong field on this core;
                    // flagSkinnable==0 ⇒ flag not set / set after loot).
                    // A9/A4: DEBUG, not INFO — this fired every tick for every
                    // skinnable corpse in range = 8.27M lines in the 4-day run
                    // (2nd-biggest playerbot.v2 tag). The operator can re-enable
                    // it via Logger.playerbot.v2=2 (DEBUG) when investigating skins.
                    TC_LOG_DEBUG("playerbot.v2",
                        "[skin_dbg] {} corpse={} hasSkill={} skinLootId={} flagSkinnable={} alreadySkinned={} can_skin={} is_skinnable={}",
                        s.name(), c->GetGUID().GetCounter(),
                        p->HasSkill(SKILL_SKINNING_LOCAL) ? 1 : 0,
                        (c->GetCreatureDifficulty() ? c->GetCreatureDifficulty()->SkinLootID : 0u),
                        c->HasUnitFlag(UNIT_FLAG_SKINNABLE) ? 1 : 0,
                        c->HasUnitFlag3(UNIT_FLAG3_ALREADY_SKINNED) ? 1 : 0,
                        can_skin ? 1 : 0, is_skinnable ? 1 : 0);

                    // "Pending loot" must mean "something THIS bot can still
                    // take", not "any item is left on the corpse". A quest item
                    // the bot no longer needs (e.g. 40+ Discolored Fang after
                    // the quest is done) stays in loot->items forever as
                    // un-takeable — the old `!loot->items.empty()` test then
                    // reported the corpse perpetually un-looted, so Phase 2
                    // skinning never fired and the corpse kept its loot icon.
                    // Use the SAME takeable criterion loot_corpse stores by
                    // (slot resolvable for this player, not looted, not blocked)
                    // plus gold, so an un-takeable leftover doesn't block skin.
                    Loot* loot = c->GetLootForPlayer(p);
                    bool has_pending_loot = false;
                    if (loot)
                    {
                        if (loot->gold != 0)
                            has_pending_loot = true;
                        else
                            for (uint8 ls = 0; ls < loot->items.size(); ++ls)
                            {
                                // FFA-aware: a free-for-all item's per-player taken
                                // state lives in ffaItem->is_looted, NOT the shared
                                // li->is_looted (which the server never sets for FFA
                                // — see StoreLootItem's `if (!item->freeforall)`).
                                // Reading li->is_looted alone made every FFA item
                                // this bot had ALREADY taken read as still-pending,
                                // so has_pending_loot stayed true forever: the drain
                                // looped Phase 1 (loot_corpse correctly skips the
                                // already-taken FFA item, so the corpse never
                                // appeared drained), Phase-2 skinning never ran, and
                                // the corpse hit the 12s give-up still flagged
                                // skinnable — the 94 lost-skin corpses in the run.
                                NotNormalLootItem* ffaItem = nullptr;
                                LootItem* li = loot->LootItemInSlot(ls, p, &ffaItem);
                                if (!li || li->is_blocked) continue;
                                const bool taken = ffaItem ? ffaItem->is_looted
                                                           : li->is_looted;
                                if (taken) continue;
                                // Mirror loot_corpse's storability skip: an item
                                // the executor will NEVER store (unique-count cap,
                                // bag full — the [loot_skip] reason=cant_store
                                // class) must not count as pending, or Phase 1
                                // re-loots every tick until the 12s deadline
                                // abandons the corpse and the SKIN is lost
                                // (live 2026-06-11: Uraimus [loot_give_up]
                                // items_left=2 skinnable=1 can_skin=1).
                                ItemPosCountVec dest;
                                if (p->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest,
                                                       li->itemid, li->count) != EQUIP_ERR_OK)
                                    continue;
                                has_pending_loot = true;
                                break;
                            }
                    }

                    if (has_pending_loot)
                    {
                        // Phase 1 (corpse loot) or Phase 3 (skinning loot).
                        // Either way drain via LootIntent. Keep the corpse
                        // queued if it is still skinnable so the skin step
                        // gets a turn next tick once this loot is released.
                        emit.emit(LootIntent{c->GetGUID()});
                        // Keep the corpse queued if it's a skinnable TYPE
                        // (can_skin), not only if the SKINNABLE flag is already
                        // set: the flag may not appear until the normal loot is
                        // released, and popping on !is_skinnable here discarded
                        // the corpse before Phase 2 could ever skin it.
                        if (!can_skin)
                            q.pop_front();
                        ai.set_last_rule_fired("idle:loot_corpse");
                    }
                    else if (is_skinnable)
                    {
                        // Phase 2: corpse fully looted and still skinnable.
                        // Cast the rank-appropriate skinning spell; a fresh
                        // LOOT_SKINNING loot then appears next tick (Phase 3).
                        // Per-corpse backoff (reuse ActionKind::Gather keyed on
                        // the corpse guid_low) so a refused / interrupted skin
                        // doesn't busy-loop the cast every tick; once the
                        // backoff elapses without UNIT_FLAG3_ALREADY_SKINNED we
                        // retry, and if it never becomes skinned we eventually
                        // fall through to the pop below.
                        const uint64 corpse_low = c->GetGUID().GetCounter();
                        if (!ai.action_recently_tried(
                                BotAI::ActionKind::Gather, corpse_low, now_ms2))
                        {
                            // Only arm the per-corpse backoff if the cast was
                            // ACTUALLY emitted. emit.cast returns false when the
                            // per-SPELL dedup lockout drops it (the bot skinned
                            // a DIFFERENT corpse with the same spell within the
                            // ~1.5s window). The old code armed the backoff
                            // regardless, so a deduped skin started this corpse's
                            // cooldown and it was never actually skinned —
                            // 25,634 can_skin=1 checks produced ~0 skins live.
                            // On a dropped cast, hold the corpse queued and retry
                            // next tick once the spell dedup clears.
                            if (emit.cast(ResolveSkinningSpell(p), c->GetGUID()))
                                ai.note_action_retry(
                                    BotAI::ActionKind::Gather, corpse_low, now_ms2);
                            ai.set_last_rule_fired("idle:skin_corpse");
                        }
                        else
                        {
                            // Skin attempted recently — the ~2s skinning cast
                            // may still be IN FLIGHT. The old pop here orphaned
                            // the cast's result: the skin loot (Phase 3)
                            // appeared on a corpse no longer queued and was
                            // never collected. ROTATE the corpse to the back
                            // instead so other corpses drain meanwhile; the
                            // per-corpse 12s deadline still bounds a corpse
                            // that never resolves. The sentinel stops the
                            // while-loop from spinning forever when this is
                            // the only queued corpse.
                            if (rotate_sentinel == c->GetGUID())
                                return;             // came full circle this tick
                            if (rotate_sentinel.IsEmpty())
                                rotate_sentinel = c->GetGUID();
                            q.push_back(q.front());
                            q.pop_front();
                            continue;
                        }
                    }
                    else
                    {
                        // Nothing left to take and nothing to skin — drop it
                        // and try the next queued corpse this tick.
                        q.pop_front();
                        continue;
                    }
                }
                else
                {
                    // Walk deadline: give up on a corpse the bot can't REACH (the
                    // loot deadline above only starts once in range). Without this
                    // a corpse on a ledge / across a gap loops idle:move_to_corpse
                    // forever (live: Wylius wedged 273s on it).
                    const uint64 corpse_walk_low = c->GetGUID().GetCounter();
                    if (ai.loot_corpse_walk_overdue(corpse_walk_low, s.published_at_ms()))
                    {
                        TC_LOG_INFO("playerbot.v2",
                            "[loot_walk_give_up] {} corpse={} entry={} dist={:.0f}y unreachable",
                            s.name(), corpse_walk_low, c->GetEntry(), dist);
                        ai.clear_loot_corpse_walk(corpse_walk_low);
                        q.pop_front();
                        continue;
                    }
                    // A human leads the instance group: the bots follow him (Follow / Stay obeyed), so a corpse far from him waits
                    // in the queue until he walks near it. The owner, Deadmines 2026-10-10: he hit Follow and two bots stayed down
                    // the tunnel - the loot walk leased the follow-recall away (grant_detour below).
                    if (s.is_in_instance() && !g.leader_is_bot())
                        if (auto const* mems = g.members())
                            for (GroupMemberSummary const& m : *mems)
                                if (!m.is_bot && m.online && m.is_alive && m.map_id == s.map_id())
                                {
                                    const float lx = c->GetPositionX() - m.x, ly = c->GetPositionY() - m.y;
                                    if (lx * lx + ly * ly > kHumanLootLeashYd * kHumanLootLeashYd)
                                        return;
                                    break;
                                }
                    // A bot-led dungeon group: a corpse further from the anchor tank than kTankLootLeashYd waits (the queue rotates to
                    // the next corpse). ingroup:dungeon_rejoin_tank walks a follower back once it is 40 y from the tank, so a loot
                    // walk to a corpse near that line ping-ponged with it and the follower stood still for 45 s (deadmines_full
                    // c82a7e21: Scnpriest alternated idle:move_to_corpse / ingroup:dungeon_rejoin_tank from t=947 s to t=1020 s).
                    if (s.is_in_instance() && g.leader_is_bot())
                        if (GroupMemberSummary const* tk = States::GroupAnchorTank(s, ai, g);
                            tk && tk->online && tk->is_alive && tk->guid != s.guid() && tk->map_id == s.map_id())
                        {
                            const float tx = c->GetPositionX() - tk->x, ty = c->GetPositionY() - tk->y;
                            if (tx * tx + ty * ty > kTankLootLeashYd * kTankLootLeashYd)
                            {
                                if (rotate_sentinel == c->GetGUID())
                                    return;             // came full circle this tick: every queued corpse is too far
                                if (rotate_sentinel.IsEmpty())
                                    rotate_sentinel = c->GetGUID();
                                q.push_back(q.front());
                                q.pop_front();
                                continue;
                            }
                        }
                    // An idle hostile mob beside the corpse (the rest of the pack, a patrol standing by it) is aggroed by the walk: the
                    // corpse waits in the queue until the mob is dealt with (playtest, Deadmines 2026-10-15: bots ran to loot and pulled more).
                    for (NearbyUnit const& u : s.raw().combat.nearby_enemies)
                    {
                        if (u.hp <= 0 || u.unit_in_combat || !u.victim.IsEmpty() || u.is_pacified || u.untargetable || u.no_xp_kill) continue;
                        const float ux = u.x - c->GetPositionX(), uy = u.y - c->GetPositionY();
                        if (ux * ux + uy * uy <= kLootIdleMobClearYd * kLootIdleMobClearYd)
                            return;
                    }
                    emit.move_to(c->GetPositionX(), c->GetPositionY(),
                                 c->GetPositionZ(), /*run*/ true);
                    // Lease a few seconds of follow-recall slack so a grouped
                    // bot can actually REACH the corpse — without it,
                    // ingroup:follow_recall yanked the bot back every other
                    // tick and looting starved (2026-06-11 Deadmines: 7-33
                    // corpses pending, zero drained).
                    ai.grant_detour(s.published_at_ms());
                    ai.set_last_rule_fired("idle:move_to_corpse");
                }
                emitted = true;
                return;   // one corpse per tick
            }
        });
    return emitted;
}

} // anonymous namespace

void RegisterLootDrainRules(IdleRuleRegistry& r)
{
    IdleRule rule;
    rule.name     = "idle:loot_drain";   // /whyidle name; per-tick fire
                                          // reports loot_corpse / move_to_corpse.
    rule.priority = 222;
    rule.gate     = &LootDrainGate;
    rule.fire     = &LootDrainFire;
    r.register_rule(std::move(rule));
}

namespace States {

// The dungeon dispatch's tail hold would swallow idle:loot_drain (priority 222). Corpses are looted
// between pulls: once nobody in the group is fighting (the gate already holds for a fighting human).
bool DungeonLootDrain(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g,
                      BotIntentEmitter& emit)
{
    if (s.in_combat())
        return false;
    if (auto const* members = g.members())
        for (GroupMemberSummary const& m : *members)
            if (m.guid != s.guid() && m.online && m.is_alive && m.in_combat && m.map_id == s.map_id())
                return false;
    // The drink-before-loot check (deadmines_rhahkzor run 7: a looting healer is always "moving" and never sat) lives in LootDrainGate.
    const uint32 now_ms = s.published_at_ms();
    if (!LootDrainGate(s, ai, g, now_ms))
        return false;
    return LootDrainFire(s, ai, g, emit, now_ms);
}

} // namespace States

} // namespace Playerbot
