// HearthRules - Refactor #3 pass 10. Migrates idle:walk_to_innkeeper_rebind
// (proactive activity-driven hearth rebind) and idle:homebind (passive
// proximity rebind) out of the State_Idle cascade. The proactive rule
// fires first (priority just above) so a bot that's been questing far
// from any city walks itself to the local innkeeper; the proximity rule
// then completes the bind on arrival.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/States/MaintainHelpers.h"   // ChunkedWalkToward
#include "UnitDefines.h"
#include "World/WorldMetadata.h"

#include <cmath>
#include <limits>

namespace Playerbot {

namespace {

constexpr float kInnInteract = 5.0f;

// ---------- idle:walk_to_innkeeper_rebind ----------
bool WalkInnkeeperGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,uint32)
{
    const uint32 zid = s.zone_id();
    if (zid == 0) return false;
    if (ai.hearth_zone() == zid) return false;
    if (s.has_actionable_quest()) return false;   // quest work in reach wins; the quest walkers would pull the bot back over the 80 y line
    return ai.ms_in_zone(zid) >= ::Playerbot::BotAI::kHearthRebindThresholdMs;
}

bool WalkInnkeeperFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32 now_ms)
{
    float bx2, by2, bz2; s.position(bx2, by2, bz2);
    float target_x = 0.f, target_y = 0.f, target_z = 0.f;
    float dsq = std::numeric_limits<float>::infinity();
    bool have_target = false;
    if (auto const* inn = s.nearest_npc_with_flag(UNIT_NPC_FLAG_INNKEEPER))
    {
        target_x = inn->x;
        target_y = inn->y;
        target_z = inn->z;
        const float dxi = target_x - bx2, dyi = target_y - by2;
        dsq = dxi*dxi + dyi*dyi;
        have_target = true;
    }
    else
    {
        // Fallback to operator-curated Innkeeper annotation. Only fires
        // when the snapshot's 40y NPC scan didn't find one — i.e. the
        // bot is in the wilds and needs a longer-range hint to walk
        // toward town. Walks up to 600y toward the metadata waypoint;
        // once within 40y the in_world inn returns non-null on the
        // next snapshot and this code path is no longer taken.
        using ::Playerbot::V2::World::WorldMetadataStore;
        using ::Playerbot::V2::World::WorldMetadataKind;
        auto rows = WorldMetadataStore::Instance().RecordsForMapAndKind(
            s.map_id(), WorldMetadataKind::Innkeeper);
        for (auto const& r : rows)
        {
            const float dx = r.x - bx2, dy = r.y - by2;
            const float d2 = dx*dx + dy*dy;
            if (d2 < dsq && d2 <= 600.0f * 600.0f)
            {
                target_x = r.x; target_y = r.y; target_z = r.z;
                dsq = d2;
                have_target = true;
            }
        }
    }
    if (!have_target) return false;
    if (dsq <= kInnInteract * kInnInteract)
    {
        // Defensive: stamp hearth_zone so we don't re-loop while the
        // proximity bind rule completes its work.
        ai.set_hearth_zone(s.zone_id());
        return false;
    }
    const float dist = std::sqrt(dsq);
    const float ahead = std::min(35.0f, dist) / dist;
    if (NearbyUnit const* threat = s.path_threat(
            bx2 + (target_x - bx2) * ahead, by2 + (target_y - by2) * ahead,
            /*max_forward*/ std::min(dist, 35.0f),
            /*half_width*/  10.0f))
    {
        if (emit.start_attack(threat->guid))
        {
            ai.set_last_rule_fired("idle:walk_innkeeper_pull_threat");
            return true;
        }
    }
    // The shared chunked walk (stall escape, deflection) to within 9 y; the last yards to the 5 y bind range as one pathed move.
    if (::Playerbot::States::ChunkedWalkToward(s, ai, emit, target_x, target_y, target_z, "idle:walk_to_innkeeper_rebind", now_ms))
        return true;
    if (dist > 9.0f)
        return false;   // the walk gave up: unreachable this episode
    if (!emit.move_to(target_x, target_y, target_z, /*run*/ true) && !emit.move_already_emitted(target_x, target_y, target_z))
        return false;
    ai.set_last_rule_fired("idle:walk_to_innkeeper_rebind");
    return true;
}

// ---------- idle:homebind ----------
bool HomebindGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,uint32)
{
    auto const* inn = s.nearest_npc_with_flag(UNIT_NPC_FLAG_INNKEEPER);
    if (!inn)
    {
        if (!ai.last_homebind_innkeeper().IsEmpty())
            ai.set_last_homebind_innkeeper(ObjectGuid::Empty);
        return false;
    }
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = inn->x - bx, dy = inn->y - by, dz = inn->z - bz;
    if (dx*dx + dy*dy + dz*dz > kInnInteract * kInnInteract)
    {
        if (!ai.last_homebind_innkeeper().IsEmpty())
            ai.set_last_homebind_innkeeper(ObjectGuid::Empty);
        return false;
    }
    return ai.last_homebind_innkeeper() != inn->guid;
}

bool HomebindFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32)
{
    auto const* inn = s.nearest_npc_with_flag(UNIT_NPC_FLAG_INNKEEPER);
    if (!inn) return false;
    emit.bind_homebind(inn->guid);
    ai.set_last_homebind_innkeeper(inn->guid);
    ai.set_last_rule_fired("idle:homebind");
    return true;
}

} // anonymous namespace

void RegisterHearthRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:walk_to_innkeeper_rebind";
        rule.priority = 240;
        rule.gate     = &WalkInnkeeperGate;
        rule.fire     = &WalkInnkeeperFire;
        // Looked at every tick (the gate is two compares): with a 5 s throttle the walk held the bot one tick in five and the rules
        // below walked it elsewhere in between, r10 12 Nov: 18 town_errand <> walk_to_innkeeper_rebind flips in the capitals.
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:homebind";
        rule.priority = 238;
        rule.gate     = &HomebindGate;
        rule.fire     = &HomebindFire;
        rule.min_interval_ms = 3000;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
