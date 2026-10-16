// OocConsumableRules - Refactor #3 pass 14. Migrates the out-of-combat
// recovery + auto-vote + auto-equip rules out of State_Idle's inline tail:
//   - idle:cannibalize        (Undead racial HP regen from a nearby corpse)
//   - idle:ooc_potion         (emergency HP potion when the bot can't eat)
//   - idle:ic_potion          (combat healthstone / potion, as State_InCombat)
//   - idle:ooc_eat_food       (sit-eat-drink between pulls)
//   - idle:ooc_bandage        (self-bandage when low HP and no food works)
//   - idle:loot_roll          (auto-vote on group loot rolls with the owner's
//                              human-first etiquette; reports loot_roll /
//                              loot_roll_quest / loot_roll_de / *_human)
//   - idle:equip_upgrade      (stat-priority + ilvl scored equip swap)
// Priorities sit just below the high-pri preemption band (600..670) so
// they fire from the bottom-of-tick dispatch in priority order, after
// the still-inline MaintainOoc* family.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "Bot/ClassicUpkeep.h"
#include "Bot/States/MaintainHelpers.h"   // States::DungeonLootRoll
#include "SharedDefines.h"
#include "RaceMask.h"        // RACE_UNDEAD_PLAYER
#include "GameTime.h"
#include "Services.h"
#include "Util/ConfigReader.h"

#include <array>

namespace Playerbot {

namespace {

// Per-bot HP / mana thresholds for OOC consumable use; duplicated from
// State_Idle.cpp::GetIdleConsumeThresholds (anonymous-namespace helper
// in the legacy TU). Cheap and small — keeping a copy avoids exposing
// the helper publicly.
struct IdleConsumeThresholds
{
    int32 food_floor;
    int32 bandage_below;
    int32 potion_below;
};

IdleConsumeThresholds GetIdleConsumeThresholds(BotAI const& ai)
{
    switch (ai.personality().risk_tolerance)
    {
        case RiskTolerance::Cautious: return {90, 70, 50};
        case RiskTolerance::Careful:  return {75, 60, 40};
        case RiskTolerance::Normal:   return {60, 50, 30};
        case RiskTolerance::Reckless: return {40, 30, 15};
    }
    return {60, 50, 30};
}

// Item score for equip-upgrade ranking; duplicated from State_Idle.cpp's
// anonymous-namespace ScoreItem helper (single arithmetic expression —
// duplication is cheaper than exposing a header).
inline float ScoreItem(ItemStatBlock const& stats,
                       uint16 item_level,
                       std::array<float, 12> const& weights,
                       float dps_weight)
{
    float score = static_cast<float>(item_level);
    for (size_t i = 0; i < weights.size(); ++i)
        score += static_cast<float>(stats.stats[i]) * weights[i];
    score += static_cast<float>(stats.weapon_dps_x10) * dps_weight;
    return score;
}

// ---------- idle:cannibalize ----------
bool CannibalizeGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (s.cls() == 0) return false;
    if (s.race() != RACE_UNDEAD_PLAYER) return false;
    if (s.in_combat() || s.is_casting()) return false;
    // Combat-flicker gate: Cannibalize is a 10s channel on a corpse,
    // breaks on damage. Don't start it in the brief single-tick OOC
    // window between trash pulls. 5s matches sibling food/bandage gates.
    if (s.recently_in_combat(5000)) return false;
    if (s.hp_pct() >= 70) return false;
    constexpr uint32 CANNIBALIZE = 20577;
    return s.knows_spell(CANNIBALIZE) && s.is_ready(CANNIBALIZE);
}

bool CannibalizeFire(BotSnapshotView const& s, BotAI& ai,
                     GroupSnapshotView const&,
                     BotIntentEmitter& emit, uint32)
{
    constexpr uint32 CANNIBALIZE = 20577;
    float bx, by, bz; s.position(bx, by, bz);
    for (auto const& u : s.raw().combat.nearby_enemies)
    {
        if (u.hp > 0) continue;
        const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
        if (dx*dx + dy*dy + dz*dz <= 5.0f * 5.0f)
        {
            if (emit.cast(CANNIBALIZE))
            {
                ai.set_last_rule_fired("idle:cannibalize");
                return true;
            }
        }
    }
    return false;
}

// ---------- idle:ooc_potion ----------
// Out of combat a healing potion is the last resort: only when the bot is
// low and can't sit down to eat (something is fighting nearby, or it has no
// food). Never a mana potion, elixir or other potion: BestConsumable tells
// them apart by their spell.
bool OocPotionGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32)
{
    if (s.in_combat() || s.is_casting()) return false;
    if (!Classic::ConsumablesAllowed(s)) return false;   // Playerbot.Consumables.PvE / .PvP
    if (ai.effective_role(s) == Role::Healer) return false;
    const auto thr = GetIdleConsumeThresholds(ai);
    if (s.hp_pct() >= thr.potion_below) return false;
    if (Classic::SafeToRecover(s, &g) && Classic::BestConsumable(s, Classic::ConsumableKind::Food))
        return false;   // eat instead (idle:ooc_eat_food)
    return Classic::BestConsumable(s, Classic::ConsumableKind::HealingPotion) != 0;
}

bool OocPotionFire(BotSnapshotView const& s, BotAI& ai,
                   GroupSnapshotView const&,
                   BotIntentEmitter& emit, uint32)
{
    const uint32 best_potion = Classic::BestConsumable(s, Classic::ConsumableKind::HealingPotion);
    if (!best_potion) return false;
    const uint32 pot_now_ms = GameTime::GetGameTimeMS();
    if (ai.action_recently_tried(BotAI::ActionKind::OocPotion,
                                  uint64(best_potion), pot_now_ms))
        return false;
    emit.emit(UseItemByEntryIntent{best_potion, s.raw().guid});
    ai.note_action_retry(BotAI::ActionKind::OocPotion,
                         uint64(best_potion), pot_now_ms);
    ai.set_last_rule_fired("idle:ooc_potion");
    return true;
}

// ---------- idle:ic_potion (emergency in-combat) ----------
// The in-combat dispatcher (State_InCombat MaybeUseSurvivalItem) covers a
// bot in its combat state; this catches the ticks where the bot is already
// in combat but still dispatched as idle. Same decision for both
// (Classic::PickCombatHealthItem, the owner's UsePotionsStrategy): at 30% health
// or below a healthstone, then the best healing potion for the level, kept
// while a group heal is about to land (unless at 20% or below), one combat
// consumable per 2.5 s and only items off cooldown.
bool IcPotionGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const& g, uint32)
{
    if (!s.in_combat() || !s.is_alive()) return false;
    return Classic::PickCombatHealthItem(s, g) != 0;
}

bool IcPotionFire(BotSnapshotView const& s, BotAI& ai,
                  GroupSnapshotView const& g,
                  BotIntentEmitter& emit, uint32)
{
    const uint32 item = Classic::PickCombatHealthItem(s, g);
    if (!item) return false;
    emit.emit(UseItemByEntryIntent{item, s.raw().guid});
    ai.set_last_rule_fired("idle:ic_potion");
    return true;
}

// ---------- idle:ooc_eat_food ----------
// Sit-eat-drink between pulls (the owner's UseFoodStrategy): eat below 90%
// health, drink below 70% mana, only when SafeToRecover - nothing attacks the
// bot and nobody (group member or enemy) is fighting within 45 yards - so a
// companion eats at its owner's side and stands up for the next pull. While
// the food / drink runs the bot stays seated (hold) until topped up instead
// of re-using the item every tick.
// A companion whose owner is not leading it right now (dead / running back as
// a ghost, offline, or on another map) is not about to be stood up by the
// follow, so it may sit — after a wipe the revived bots drink to full while
// the owner is still on the corpse run.
bool OwnerNotLeading(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    if (!s.is_companion_with_owner()) return false;
    auto const* members = g.members();
    if (!members) return false;
    for (auto const& m : *members)
        if (m.guid == s.raw().owner_guid)
            return !m.online || !m.is_alive || m.map_id != s.map_id();
    return false;
}

// Open-world drink threshold (the owner's UseFoodStrategy). A healer grouped in
// an instance drinks below Classic::kPullReadyHealerManaPct instead - the mana
// its tank waits for before the next pull - so it never holds a pull it is not
// drinking for (Classic::OocDrinkStartManaPct).
constexpr int32 kOocDrinkBelowManaPct = 70;

// The drink-start mana: the trash threshold, or the full-mana one for a raid healer holding for the owner's Attack.
int32 OocDrinkStart(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (s.is_in_raid() && ai.holding_position(now_ms) && Classic::IsGroupHealer(s, &g))
        return Classic::kHeldRaidHealerManaPct;
    // The owner's hold in a raid is the wait for a boss pull for the casters and hunters too: they drink to the same mark (playtest, live MC
    // 2026-10-12: "full mana for fights").
    if (s.is_in_raid() && ai.holding_position(now_ms) && s.max_power(POWER_MANA) > 0)
        return Classic::kHeldRaidHealerManaPct;
    return Classic::OocDrinkStartManaPct(s, &g, kOocDrinkBelowManaPct);
}

bool OocEatFoodGate(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const& g, uint32 now_ms)
{
    if (s.in_combat() || s.is_casting()) return false;
    if (!s.companion_may_sit() && !OwnerNotLeading(s, g)) return false;
    // Combat-flicker gate (see MaintainOocFood for full rationale).
    // Eating/drinking applies a Sit aura; first hit cancels it and
    // wastes the consumable.
    if (s.recently_in_combat(5000)) return false;
    if (s.raw().movement.is_mounted) return false;
    if (s.is_encounter_in_progress()) return false;
    if (!Classic::SafeToRecover(s, &g)) return false;
    // A solo quester does not sit down with its quest kill targets ready to engage in scan range; a grouped bot goes at the
    // group's pace, so this never applies to it.
    if (!s.in_group())
    {
        for (auto const& ao : s.actionable_objectives())
        {
            if (ao.type == /*MONSTER*/ 0 ||
                ao.type == /*KILL_WITH_LABEL*/ 21 ||
                (ao.type == /*ITEM*/ 1 && ao.source_guid.IsCreature()))
                return false;
        }
    }
    Classic::RecoveryPick const pick = Classic::PickRecovery(s, GetIdleConsumeThresholds(ai).food_floor, OocDrinkStart(s, ai, g, now_ms));
    if (pick.item)
        return !s.raw().movement.is_moving;
    return pick.hold;
}

bool OocEatFoodFire(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const& g,
                    BotIntentEmitter& emit, uint32 now_ms)
{
    Classic::RecoveryPick const pick = Classic::PickRecovery(s, GetIdleConsumeThresholds(ai).food_floor, OocDrinkStart(s, ai, g, now_ms));
    if (pick.item)
    {
        emit.emit(UseItemByEntryIntent{pick.item, s.raw().guid});
        ai.set_last_rule_fired("idle:ooc_eat_food");
        return true;
    }
    if (!pick.hold) return false;
    ai.set_last_rule_fired("idle:ooc_eat_food:sitting");
    return true;
}

// ---------- idle:ooc_bandage ----------
// Self-bandage below 50% health (the owner: out-of-combat bandages when no
// healer is near). Bandages need First Aid at the item's rank (the supplies
// restock teaches it); Recently Bandaged blocks another for a minute.
constexpr uint32 kRecentlyBandaged = 11196;

bool OocBandageGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32)
{
    if (s.in_combat() || s.is_casting()) return false;
    // Combat-flicker gate: bandage is 6-8s channel, breaks on damage.
    // 5s matches sibling food/cannibalize/conjure gates.
    if (s.recently_in_combat(5000)) return false;
    if (s.hp_pct() >= 50) return false;
    if (ai.effective_role(s) == Role::Healer) return false;
    if (s.raw().movement.is_moving || s.raw().movement.is_mounted) return false;
    if (s.has_aura(kRecentlyBandaged) || Classic::IsEating(s)) return false;
    if (!Classic::SafeToRecover(s, &g)) return false;
    // A healer of the group close by tops the bot up instead.
    if (auto const* members = g.members())
    {
        float bx, by, bz; s.position(bx, by, bz);
        for (auto const& m : *members)
        {
            if (m.guid == s.guid() || !m.online || !m.is_alive || m.role != Role::Healer || m.map_id != s.map_id()) continue;
            const float dx = m.x - bx, dy = m.y - by, dz = m.z - bz;
            if (dx * dx + dy * dy + dz * dz < 40.0f * 40.0f)
                return false;
        }
    }
    return Classic::BestConsumable(s, Classic::ConsumableKind::Bandage) != 0;
}

bool OocBandageFire(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const&,
                    BotIntentEmitter& emit, uint32)
{
    const uint32 best_bandage = Classic::BestConsumable(s, Classic::ConsumableKind::Bandage);
    if (!best_bandage) return false;
    const uint32 ban_now_ms = GameTime::GetGameTimeMS();
    if (ai.action_recently_tried(BotAI::ActionKind::OocBandage,
                                  uint64(best_bandage), ban_now_ms))
        return false;
    emit.emit(UseItemByEntryIntent{best_bandage, s.raw().guid});
    ai.note_action_retry(BotAI::ActionKind::OocBandage,
                         uint64(best_bandage), ban_now_ms);
    ai.set_last_rule_fired("idle:ooc_bandage");
    return true;
}

// ---------- idle:loot_roll ----------
bool LootRollGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (s.raw().loot.loot_rolls.empty())
    {
        // Roll-list drained — reset the decide-at slot so the next batch
        // (e.g. the next boss kill) gets a fresh 1–4s hesitation.
        ai.set_next_loot_roll_fire_ms(0);
        return false;
    }
    const uint32 now_ms = s.published_at_ms();
    uint32 fire_at = ai.next_loot_roll_fire_ms();
    if (fire_at == 0)
    {
        // First tick we see a roll waiting — schedule the vote for
        // 800–4500ms from now, jittered per-bot so a raid doesn't roll
        // in lockstep. ~3.7s spread covers the entire "looking at the
        // popup" window humans show.
        const uint32 jitter = 800u + (uint32(s.bot_id()) * 2246822519u) % 3700u;
        ai.set_next_loot_roll_fire_ms(now_ms + jitter);
        return false;
    }
    return now_ms >= fire_at;
}

bool LootRollFire(BotSnapshotView const& s, BotAI& ai,
                  GroupSnapshotView const&,
                  BotIntentEmitter& emit, uint32)
{
    for (auto const& r : s.raw().loot.loot_rolls)
    {
        uint8 vote = 0;
        char const* rule_name = "idle:loot_roll";
        // Loot etiquette with a human in the group (the owner's LootRollAction):
        // the human gets first claim. Bind on pickup, or not equipment
        // (tokens, quest items) the human could use: pass. Bind on equip the
        // human could use: greed at most.
        // Playerbot.Behavior.LootPolicy off: no etiquette, the bot rolls for itself.
        // Owner "loot all": the bot rolls for itself whatever a human could use.
        bool const etiquette = Services::Config().behavior_loot_policy() && !ai.loot_all();
        if (etiquette && r.human_wants && (r.is_bop || !r.is_equipment))
        {
            vote = 0;
            rule_name = "idle:loot_roll_pass_human";
        }
        else if (etiquette && r.human_wants)
        {
            vote = (r.vote_mask & 0x04) != 0 ? 2 : 0;
            rule_name = "idle:loot_roll_greed_human";
        }
        else if (r.is_quest_item && (r.vote_mask & 0x02) != 0)
        {
            vote = 1;
            rule_name = "idle:loot_roll_quest";
        }
        // NEED only a real upgrade: an empty slot or >= 5% over the worn
        // item (the builder's is_upgrade); side-grades fall through to greed.
        else if (r.is_upgrade && (r.vote_mask & 0x02) != 0)
        {
            vote = 1;
        }
        // L-P1b: only Disenchant when the bot actually has Enchanting
        // (SKILL_ENCHANTING = 333). Without the skill a DE vote is wasted
        // (the item vanishes and the bot gets nothing usable), so fall
        // through to GREED instead.
        else if ((r.vote_mask & 0x08) != 0 && s.has_skill(333))
        {
            vote = 3;
            rule_name = "idle:loot_roll_de";
        }
        else if ((r.vote_mask & 0x04) != 0) vote = 2;  // GREED
        // L-P3a: TRANSMOG (0x10) — bots don't collect appearances; treat
        // as GREED if offered alongside greed, else PASS below.
        else if ((r.vote_mask & 0x10) != 0)
        {
            vote = 0;  // PASS on transmog-only rolls
            rule_name = "idle:loot_roll_pass";
        }
        else if ((r.vote_mask & 0x01) != 0) vote = 0;
        // L-P3a: final PASS fallback so every roll resolves even when only
        // an unhandled/unexpected vote bit is set. An unresolved roll
        // would otherwise hang the loot UI until it times out.
        else
        {
            vote = 0;
            rule_name = "idle:loot_roll_pass";
        }
        emit.loot_roll(r.loot_object, r.loot_list_id, vote);
        ai.set_last_rule_fired(rule_name);
        // Inter-roll spacing: after firing one vote, schedule the next
        // one 400–1500ms out so a 3-item boss drop doesn't auto-resolve
        // in two ticks. Re-jitter on bot_id+published_at so consecutive
        // rolls from the same bot don't fall on the same offset.
        const uint32 now_ms = s.published_at_ms();
        const uint32 spacing =
            400u + ((uint32(s.bot_id()) ^ now_ms) * 2654435761u) % 1100u;
        ai.set_next_loot_roll_fire_ms(now_ms + spacing);
        return true;
    }
    return false;
}

// ---------- idle:equip_upgrade ----------
bool EquipUpgradeGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32 now_ms)
{
    if (s.in_combat() || s.is_casting())
    {
        if (ai.pending_equip_upgrade_at_ms() != 0)
            ai.set_pending_equip_upgrade_at_ms(0);
        return false;
    }
    // L-P1c: previously a blanket is_in_dungeon()||in_battleground() block
    // meant bots never equipped upgrades during instanced content. Removed.
    // We now rely on the existing !in_combat && !casting window plus a short
    // post-combat settle so the swap can't land mid-pull (the 1-tick OOC
    // gap between trash packs). recently_in_combat(3000) == true means
    // combat ended within the last 3s — defer until that window passes.
    if (s.recently_in_combat(3000))
    {
        if (ai.pending_equip_upgrade_at_ms() != 0)
            ai.set_pending_equip_upgrade_at_ms(0);
        return false;
    }
    // Quest-first (2026-06-16): a bot with a reachable quest action finishes the
    // quest instead of stopping to swap gear — this gate ignoring quest state was
    // the #1 GoalUnreachable wedge (bot AT its objective running equip_upgrade).
    // FULLY yield: there is NO bag-full carve-out because equipping does not free
    // bag slots (API::equip_item swaps the replaced item back into the bag); the
    // real full-bag blocker is idle:vendor_sell_trash, which is quest-aware.
    if (s.has_actionable_quest())
    {
        if (ai.pending_equip_upgrade_at_ms() != 0)
            ai.set_pending_equip_upgrade_at_ms(0);
        return false;
    }
    // Compare-tooltips hesitation: 2–6s before the first equip click
    // lands. We don't know inside the gate whether there IS an upgrade
    // candidate (that needs the full ScoreItem walk in Fire). So we
    // arm the slot unconditionally on first observation of "able to
    // equip" state and let Fire's own equip_recently_tried per-entry
    // throttle dedup subsequent ticks. Cost: every bot eligible for
    // a swap waits a one-time 2–6s window after entering town; cheap
    // for the realism payoff.
    uint32 ready_at = ai.pending_equip_upgrade_at_ms();
    if (ready_at == 0)
    {
        const uint32 jitter = 2000u + (uint32(s.bot_id()) * 2654435761u) % 4000u;
        ai.set_pending_equip_upgrade_at_ms(now_ms + jitter);
        return false;
    }
    return now_ms >= ready_at;
}

bool EquipUpgradeFire(BotSnapshotView const& s, BotAI& ai,
                     GroupSnapshotView const&,
                     BotIntentEmitter& emit, uint32)
{
    std::array<float, 12> weights = s.spec_stat_weights();
    if (s.in_battleground())
        weights[8] = weights[8] * 1.5f;
    const float dps_w   = s.spec_weapon_dps_weight();
    const uint32 equip_now_ms = GameTime::GetGameTimeMS();
    for (auto const& it : s.raw().inventory.bag_items)
    {
        if (it.equip_slot == 0xFF) continue;
        if (it.item_level == 0) continue;
        if (it.quality == 0) continue;
        if (ai.equip_recently_tried(it.entry, equip_now_ms)) continue;
        EquippedItem const& cur = s.equipped(it.equip_slot);
        const float score_new = ScoreItem(it.stats, it.item_level, weights, dps_w);
        const float score_cur = (cur.entry != 0)
            ? ScoreItem(cur.stats, cur.item_level, weights, dps_w)
            : 0.0f;
        // Swap margin scales with the CURRENT item's score. The old flat
        // max(50, 5%) floor demanded a ~+50-score jump on a scale where
        // leveling-range strict upgrades are worth +5..25 — so low/mid-level
        // bots NEVER equipped anything via this rule and armor piled up in
        // bags (audit C10: L34 healer at item level 7 with 6 pending
        // upgrades). 5% of current score (min 2) keeps swap-churn hysteresis
        // meaningful at endgame scores while letting real leveling upgrades
        // through.
        const float margin = std::max(2.0f, score_cur * 0.05f);
        if (score_new <= score_cur + margin) continue;
        emit.equip_item(it.bag, it.slot, it.equip_slot);
        ai.note_equip_try(it.entry, equip_now_ms);
        ai.set_last_rule_fired("idle:equip_upgrade");
        return true;
    }
    return false;
}

} // anonymous namespace

void RegisterOocConsumableRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:cannibalize";
        rule.priority = 670;
        rule.gate     = &CannibalizeGate;
        rule.fire     = &CannibalizeFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:ooc_potion";
        rule.priority = 668;
        rule.gate     = &OocPotionGate;
        rule.fire     = &OocPotionFire;
        r.register_rule(std::move(rule));
    }
    // In-combat emergency potion sits HIGHER than survival-tier
    // rules (875) — using a potion at 15% HP is the most life-
    // critical decision the bot makes mid-fight, above interrupt /
    // assist priorities. Shares lockout with idle:ooc_potion so the
    // one-per-fight Potion Sickness debuff is respected.
    {
        IdleRule rule;
        rule.name     = "idle:ic_potion";
        rule.priority = 875;
        rule.gate     = &IcPotionGate;
        rule.fire     = &IcPotionFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:ooc_eat_food";
        rule.priority = 666;
        rule.gate     = &OocEatFoodGate;
        rule.fire     = &OocEatFoodFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:ooc_bandage";
        rule.priority = 664;
        rule.gate     = &OocBandageGate;
        rule.fire     = &OocBandageFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:loot_roll";
        rule.priority = 660;
        rule.gate     = &LootRollGate;
        rule.fire     = &LootRollFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:equip_upgrade";
        rule.priority = 600;
        rule.gate     = &EquipUpgradeGate;
        rule.fire     = &EquipUpgradeFire;
        r.register_rule(std::move(rule));
    }
}

namespace States {

// The dungeon dispatch's tail hold would swallow idle:loot_roll (priority 660): vote here instead.
bool DungeonLootRoll(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g,
                     BotIntentEmitter& emit)
{
    const uint32 now_ms = s.published_at_ms();
    if (!LootRollGate(s, ai, g, now_ms))
        return false;
    return LootRollFire(s, ai, g, emit, now_ms);
}

} // namespace States

} // namespace Playerbot
