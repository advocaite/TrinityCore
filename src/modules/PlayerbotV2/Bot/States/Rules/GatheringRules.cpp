// GatheringRules - Refactor #3 pass 7. Migrates `idle:gather` (use a
// gathering node when in range) and `idle:quest_start_item` (trigger a
// quest-starting bag item) out of the State_Idle linear cascade.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Util/ChestLock.h"
#include "Log.h"

namespace Playerbot {

namespace {

// ---------- idle:gather ----------
constexpr uint16 kHerbalism = 182;
constexpr uint16 kMining    = 186;
constexpr uint16 kSkinning  = 393;
// kFishing (356) removed from the gather trigger — see PROF-P2a note in
// GatherGate. Fishing needs a different (cast-channel + bobber) actioning
// model that the current primitives don't support.
constexpr uint8  GO_GATHERING_NODE_LOCAL = 50;

// A node this bot can pick now. A modern GATHERING_NODE (type 50) as before; on Classic an herb or vein is a chest locked by
// Herbalism or Mining (NearbyObject::gather_skill): only a closed one, and only with the skill level its lock asks for, as the
// player's own right-click would fail below it. API::gather_node casts the gathering spell on it.
bool Gatherable(BotSnapshotView const& s, BotSnapshot::NearbyObject const& o)
{
    if (o.go_type == GO_GATHERING_NODE_LOCAL)
        return true;
    return o.gather_skill != 0 && o.gather_ready && s.skill_value(o.gather_skill) >= o.gather_req;
}

// The herb or vein whose loot the bot's gathering spell opened: it waits for the bot to take it (API::gather_node).
BotSnapshot::NearbyObject const* OpenGatherLoot(BotSnapshotView const& s)
{
    ObjectGuid const open = s.raw().world_objects.loot_open;
    if (open.IsEmpty())
        return nullptr;
    for (auto const& obj : s.raw().world_objects.nearby_objects)
        if (obj.guid == open && obj.gather_skill != 0)
            return &obj;
    return nullptr;
}

// The player a companion follows: its owner, else the player leading its group. Null when not with one on this map.
GroupMemberSummary const* PlayerAnchor(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    auto const* members = g.members();
    if (!members)
        return nullptr;
    ObjectGuid const want = !s.raw().owner_guid.IsEmpty() ? s.raw().owner_guid : g.leader();
    for (GroupMemberSummary const& m : *members)
        if (m.guid == want && !m.is_bot && m.online && m.map_id == s.map_id())
            return &m;
    return nullptr;
}

// A companion told "gather on" (BotAI::gather_ordered) picks nodes only out of a fight - its own and its group's - and only
// while its player is near, so the follow keeps it at his side.
constexpr float kCompanionGatherReach = 25.0f;   // from the bot to the node
constexpr float kCompanionGatherLeash = 30.0f;   // from the player to the node
bool CompanionMayGather(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g)
{
    if (!ai.gather_ordered())
        return false;
    if (auto const* members = g.members())
        for (GroupMemberSummary const& m : *members)
            if (m.online && m.in_combat && m.map_id == s.map_id())
                return false;
    return PlayerAnchor(s, g) != nullptr;
}

bool GatherGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32)
{
    // PROF-P2a: Fishing (356) is intentionally NOT a trigger for idle:gather.
    // This rule actions GAMEOBJECT_TYPE_GATHERING_NODE (50) via a single
    // UseObjectIntent — the herb/mine/skinning-node interaction model. Fishing
    // does NOT fit that model: it is a channeled cast that spawns a transient
    // FISHINGNODE bobber (GO type 17) which the player must then UseGameObject
    // on during a brief "splash/ready" window. None of the three primitives
    // that loop needs exist yet on the PlayerbotV2 surface:
    //   1. a cast-at-water fishing primitive (the cast has a liquid-validity
    //      precheck and channel semantics, not a plain emit.cast at a GO),
    //   2. the bobber GO is never published in the snapshot (the gather scan
    //      only appends GATHERING_NODE(50) + FISHINGHOLE(25), not FISHINGNODE(17)),
    //      and the bobber is the bot's own channel object, not a world node,
    //   3. no "bobber ready / fish on the line" signal is surfaced for the
    //      rule to know WHEN to UseGameObject within the splash window.
    // Until those exist, gating fishing in here would let the bot *pretend* to
    // fish — burning the channel and producing a FishEscaped every cycle.
    // We therefore exclude Fishing from the trigger so the gather system does
    // not claim to handle it. Fishing skill is still granted at setup (for
    // organic skill display / future use); it simply has no actioning rule.
    // PROF-P2a: implementing real fishing needs (1) a cast-at-water/FISHINGHOLE
    // primitive, (2) FISHINGNODE bobber surfaced in the snapshot or fetched via
    // the bot's channel object guid, and (3) a bobber-ready signal to time the
    // UseGameObject. Add those before re-enabling fishing here.
    // A GAMEOBJECT_TYPE_GATHERING_NODE (50) is ALWAYS an herb or a mining vein
    // — never a skinning target (skinning is a cast on a beast corpse, handled
    // in LootDrainRules, not a world node). Including Skinning here let a
    // skinning-only bot (e.g. Uraimus: Skinning+LW, no Herb/Mining) detour to
    // and "use" herb nodes it has no skill for. Gate strictly on the two skills
    // that actually open type-50 nodes. (Per-node herb-vs-vein matching — so a
    // herbalist doesn't try to mine a vein — is the next refinement; needs the
    // node's lock skill surfaced. kSkinning is intentionally NOT used here.)
    (void)kSkinning;
    const bool can_gather = s.has_skill(kHerbalism) || s.has_skill(kMining);
    if (!can_gather) return false;
    // A bot with a player picks nodes only when told to ("gather on"); IdleRuleRegistry::tick lets these rules through then.
    if (s.is_led_by_player() && !CompanionMayGather(s, ai, g)) return false;
    if (s.in_combat() || s.is_casting()) return false;
    // The is_moving check was previously here — removed 2026-05-22 so
    // gather can fire opportunistically during travel. Real players
    // detour ~12y to grab a node they're walking past instead of
    // ignoring it. Fire now emits move_to for nodes in the 5-12y band,
    // and the existing UseObject path takes over once in interact range.
    // Combat-flicker gate: gather is a 2-3s server-side channel that
    // breaks on damage. Bots cycling pulls near gather nodes (Bloodthistle,
    // Copper Veins, etc.) flicker out of combat for one tick between
    // mobs and start the channel before the next add aggros, wasting
    // the cooldown on the node retry timer.
    if (s.recently_in_combat(5000)) return false;
    // Skip detour inside dungeons / BGs / vehicles — opportunistic
    // gathering during scripted content disrupts the role.
    if (s.is_in_dungeon() || s.in_battleground()) return false;
    // Skip during relocation / manual-travel journeys: the herb-to-herb
    // chain stalls a cross-continent trip indefinitely (observed: Somi
    // ordered to Brill spent the trip farming peacebloom on the Razor
    // Hill road). A player on a journey rides past the flowers.
    if (s.raw().quest_log.objective_is_relocation) return false;
    if (s.raw().movement.is_mounted &&
        s.raw().movement.is_flying) return false;
    // Cheap precheck: any node to pick nearby, or the loot of one the bot just opened?
    if (OpenGatherLoot(s)) return true;
    for (auto const& obj : s.raw().world_objects.nearby_objects)
        if (Gatherable(s, obj)) return true;
    return false;
}

bool GatherFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32 now_ms)
{
    float bx, by, bz; s.position(bx, by, bz);
    constexpr float kInteract = 5.0f;
    constexpr float kInt2 = kInteract * kInteract;
    // The gathering spell finished and opened the node's loot: take it (the same use, API::gather_node).
    if (auto const* open = OpenGatherLoot(s))
    {
        emit.emit(UseObjectIntent{open->guid});
        if (s.is_led_by_player())
            ai.grant_detour(now_ms);
        ai.set_last_rule_fired("idle:gather");
        return true;
    }
    for (auto const& obj : s.raw().world_objects.nearby_objects)
    {
        if (!Gatherable(s, obj)) continue;
        const float dx = obj.x - bx, dy = obj.y - by, dz = obj.z - bz;
        if (dx*dx + dy*dy + dz*dz > kInt2) continue;
        const uint64 node_low = obj.guid.GetCounter();
        if (ai.action_recently_tried(BotAI::ActionKind::Gather, node_low, now_ms))
            continue;
        emit.emit(UseObjectIntent{obj.guid});
        ai.note_action_retry(BotAI::ActionKind::Gather, node_low, now_ms);
        // A companion stands still for the 3 s cast and the loot: the follow waits (ingroup:follow_recall honours the lease).
        if (s.is_led_by_player())
            ai.grant_detour(now_ms);
        ai.set_last_rule_fired("idle:gather");
        TC_LOG_INFO("playerbot.v2",
            "[gather] {} use_object entry={} at ({:.1f},{:.1f},{:.1f})",
            s.name(), obj.entry, obj.x, obj.y, obj.z);
        return true;
    }
    return false;
}

// Full gather rule (priority 440): the in-interact-range gather. Wraps the
// shared GatherGate predicate with a quest-first yield. gather_detour
// (priority 210) deliberately does NOT use this wrapper, so its own
// quest-prioritization (it already sits below quest_turnin/quest_accept) is
// left untouched.
bool GatherFullGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    // Quest-first (2026-06-16): yield to a reachable quest action. Not a companion: its quests are its player's to lead.
    if (!s.is_led_by_player() && s.has_actionable_quest()) return false;
    return GatherGate(s, ai, g, now_ms);
}

// Opportunistic detour: when a node is in the 5-12y band, emit a
// move_to so the bot diverts briefly. Next tick the gather rule above
// fires when in interact range. Lower priority than quest_turnin /
// quest_accept so detouring doesn't preempt quest hand-ins.
bool GatherDetourGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!GatherGate(s, ai, g, now_ms)) return false;
    // GatherGate already checks: skill, !combat, !casting, !recently
    // combat, !dungeon/BG, !flying. Detour adds nothing extra here —
    // the node-in-12y precheck happens in Fire.
    return true;
}

bool GatherDetourFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32 now_ms)
{
    float bx, by, bz; s.position(bx, by, bz);
    constexpr float kInteract = 5.0f;
    constexpr float kInt2 = kInteract * kInteract;
    // A world bot detours ~12 y for a node it walks past; a companion told "gather on" goes a little further, but never to a node
    // far from its player (the follow would only fetch it back).
    bool const companion = s.is_led_by_player();
    GroupMemberSummary const* anchor = companion ? PlayerAnchor(s, g) : nullptr;
    if (companion && !anchor) return false;
    const float kDetour = companion ? kCompanionGatherReach : 12.0f;
    const float kDet2 = kDetour * kDetour;
    NearbyObject const* best = nullptr;
    float best_dsq = kDet2;
    for (auto const& obj : s.raw().world_objects.nearby_objects)
    {
        if (!Gatherable(s, obj)) continue;
        const float dx = obj.x - bx, dy = obj.y - by, dz = obj.z - bz;
        const float dsq = dx*dx + dy*dy + dz*dz;
        if (dsq <= kInt2 || dsq > best_dsq) continue;
        if (anchor)
        {
            const float ax = obj.x - anchor->x, ay = obj.y - anchor->y;
            if (ax*ax + ay*ay > kCompanionGatherLeash * kCompanionGatherLeash) continue;
        }
        const uint64 node_low = obj.guid.GetCounter();
        if (ai.action_recently_tried(BotAI::ActionKind::Gather, node_low, now_ms))
            continue;
        best = &obj; best_dsq = dsq;
    }
    if (!best) return false;
    emit.move_to(best->x, best->y, best->z, /*run*/ companion);
    if (companion)
        ai.grant_detour(now_ms);
    ai.set_last_rule_fired("idle:gather_detour");
    return true;
}

// ---------- idle:loot_chest ----------
constexpr uint8 GO_CHEST_LOCAL = 3;

// The closest chest a bot may click: herbs, ore veins and locked boxes need a skill spell, not a click (API::use_game_object
// refuses them), so they are skipped here and a real chest behind one is still found.
BotSnapshot::NearbyObject const* NearestClickableChest(BotSnapshotView const& s)
{
    for (auto const& o : s.nearby_objects())
        if (o.go_type == GO_CHEST_LOCAL && !ChestNeedsSkill(sObjectMgr->GetGameObjectTemplate(o.entry)))
            return &o;
    return nullptr;
}

bool LootChestGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,uint32 now_ms)
{
    // Quest-first (2026-06-16): yield to a reachable quest action.
    if (s.has_actionable_quest()) return false;
    if (s.bag_free_slots() == 0) return false;
    auto const* chest = NearestClickableChest(s);
    if (!chest) return false;
    if (ai.chest_loot_recently_tried(chest->guid.GetCounter(), now_ms))
        return false;
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = chest->x - bx, dy = chest->y - by, dz = chest->z - bz;
    constexpr float kInteract = 5.0f;
    return dx*dx + dy*dy + dz*dz <= kInteract * kInteract;
}

bool LootChestFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32 now_ms)
{
    auto const* chest = NearestClickableChest(s);
    if (!chest) return false;
    emit.use_game_object(chest->guid);
    ai.note_chest_loot_try(chest->guid.GetCounter(), now_ms);
    ai.set_last_rule_fired("idle:loot_chest");
    return true;
}

// ---------- idle:quest_start_item ----------
bool QuestStartItemGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    return !s.raw().quest_discovery.quest_starting_items.empty();
}

bool QuestStartItemFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32 now_ms)
{
    for (auto const& si : s.raw().quest_discovery.quest_starting_items)
    {
        const uint64 entry_low = uint64(si.item_entry);
        if (ai.action_recently_tried(BotAI::ActionKind::QuestStartItem, entry_low, now_ms))
            continue;
        emit.emit(UseItemByEntryIntent{si.item_entry, ObjectGuid::Empty});
        ai.note_action_retry(BotAI::ActionKind::QuestStartItem, entry_low, now_ms);
        ai.set_last_rule_fired("idle:quest_start_item");
        return true;
    }
    return false;
}

} // anonymous namespace

void RegisterGatheringRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:gather";
        rule.priority = 440;   // High — opportunistic gather beats most utility
        rule.gate     = &GatherFullGate;
        rule.fire     = &GatherFire;
        r.register_rule(std::move(rule));
    }
    {
        // Detour to in-band nodes — lower priority than quest_turnin
        // (430) so accepting/turning-in a quest still wins. Above
        // wander (~5) so bots prefer gathering to aimless drift.
        IdleRule rule;
        rule.name     = "idle:gather_detour";
        rule.priority = 210;
        rule.gate     = &GatherDetourGate;
        rule.fire     = &GatherDetourFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:loot_chest";
        rule.priority = 438;
        rule.gate     = &LootChestGate;
        rule.fire     = &LootChestFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:quest_start_item";
        rule.priority = 435;
        rule.gate     = &QuestStartItemGate;
        rule.fire     = &QuestStartItemFire;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
