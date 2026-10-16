// RaidCombat implementation. See RaidCombat.h / RaidOrder.h.

#include "RaidCombat.h"
#include "RaidOrder.h"
#include "../BotAI.h"
#include "../BotIntentEmitter.h"
#include "../BotSnapshotView.h"
#include "../ClassicUpkeep.h"
#include "../IdleRule.h"

#include <algorithm>

namespace Playerbot::Raid {

namespace {

char const* CastRuleName(RaidOrder::CastKind kind)
{
    switch (kind)
    {
        case RaidOrder::CastKind::Taunt:        return "raid:taunt_swap";
        case RaidOrder::CastKind::Dispel:       return "raid:dispel";
        case RaidOrder::CastKind::Purge:        return "raid:purge";
        case RaidOrder::CastKind::CrowdControl: return "raid:crowd_control";
        case RaidOrder::CastKind::Nuke:         return "raid:nuke";
        case RaidOrder::CastKind::Disarm:       return "raid:disarm";
        case RaidOrder::CastKind::StanceDance:  return "raid:stance_dance";
        case RaidOrder::CastKind::None:         break;
    }
    return "raid:cast";
}

} // namespace

bool DispatchRaidOrder(BotAI& ai, BotSnapshotView const& s, BotIntentEmitter& emit)
{
    RaidOrder const& o = s.raw().raid_order;
    if (!o.active || o.prepull || !s.is_alive())
        return false;

    // A self aura the encounter says to drop (Blessing of Protection on a tank during Nefarian's paladin call). Instant, so the
    // rest of the tick still runs.
    if (o.cancel_aura)
        emit.cancel_aura(o.cancel_aura);

    // A scripted area trigger the bot stands in (C'Thun's stomach): a client sends it on entering, a bot fires it here.
    if (o.area_trigger)
        emit.enter_area_trigger(o.area_trigger);

    // Razorgore's Orb of Domination: the brain walked the bot into reach, now take control.
    if (!o.use_object.IsEmpty())
    {
        emit.use_game_object(o.use_object);
        ai.set_last_rule_fired("raid:use_object");
        return true;
    }

    // 0. Holding a mind-controlled creature (Razuvious' understudies): command it and never break the channel.
    if (o.charm_spell || !o.charm_attack.IsEmpty() || o.charm_hold)
    {
        if (o.charm_spell)
            emit.pet_cast(o.charm_spell, o.charm_target);
        else if (!o.charm_attack.IsEmpty())
            emit.pet_attack(o.charm_attack);
        ai.set_last_rule_fired("raid:charm");
        return true;
    }

    // 1. Hazards first: leaving Inferno / Living Bomb range / a breath corridor beats everything else.
    //    A deduped re-emit (same point, still walking) still owns the tick so the rotation does not chase back in. A move that was
    //    refused (the path ring, the API's no-path answer) owns nothing: the bot stood on the spot casting and attacking nothing.
    if (o.move)
    {
        if (emit.move_to(o.move_x, o.move_y, o.move_z, /*run=*/true) || emit.move_already_emitted(o.move_x, o.move_y, o.move_z))
        {
            ai.set_last_rule_fired("raid:move");
            return true;
        }
    }

    // A debuff that turns the bot's rotation against the raid (Nefarian's priest call): no rotation until it fades, a heal
    // already being cast is stopped. An elected job (a dispel) still runs.
    if (o.hold_rotation && o.cast_kind == RaidOrder::CastKind::None)
    {
        if (s.is_casting())
            emit.cancel_cast();
        ai.set_last_rule_fired("raid:hold_rotation");
        return true;
    }

    // A melee bot whose target stands in a hazard it has just left (Inferno, a cloud): no rotation, the rotation's approach would walk it
    // straight back in. The brain sets the flag every plan while the hazard is up; it clears when the circle closes. (MC Geddon n8 run 4: six
    // melee left the Inferno circle, the rotation sent them back at second six, and the last tick killed all six at 17-20 y; the flag had
    // been set for every plan and nothing read it.) An elected job (a dispel) still runs.
    if (o.hold_melee && o.cast_kind == RaidOrder::CastKind::None)
    {
        // The victim stays selected: the brain raises the flag only while the victim stands in the hazard, so dropping it would clear the flag
        // and the rotation would walk back in on the next plan.
        ai.set_last_rule_fired("raid:melee_hold");
        return true;
    }

    // 2. The elected job (taunt swap, dispel, purge, crowd control). Let a running cast finish, then cast.
    if (o.cast_kind != RaidOrder::CastKind::None && o.cast_spell)
    {
        if (s.is_casting())
        {
            ai.set_last_rule_fired("raid:wait_cast");
            return true;
        }
        // Banish / Polymorph / Shackle / Hibernate are hard casts, refused while the caster walks (MC Garr 2026-10-10: "Banish failing with
        // MOVING" several times): stop, the cast goes out next tick.
        if (o.cast_kind == RaidOrder::CastKind::CrowdControl && s.is_moving())
        {
            emit.stop_movement();
            ai.set_last_rule_fired("raid:crowd_control_stop");
            return true;
        }
        if (emit.cast(o.cast_spell, o.cast_target))
        {
            if (o.cast_kind == RaidOrder::CastKind::Dispel && Classic::IsCurseDispelSpell(o.cast_spell))
                Classic::NoteRaidSupport(Classic::RaidSupportKind::Decurse, s.map_id());
            ai.set_last_rule_fired(CastRuleName(o.cast_kind));
            return true;
        }
    }

    // 3. Tank-first hold: no damage on that enemy until the tank has a threat lead (the brain bounds the wait).
    ObjectGuid const held = o.hold_target;
    if (!held.IsEmpty() && (s.victim() == held || s.victim().IsEmpty()))
    {
        if (s.victim() == held)
            emit.stop_attack();
        ai.set_last_rule_fired("raid:threat_hold");
        return true;
    }

    // 4. Never-target / crowd-controlled / reflecting enemies, and the swap enemy for a debuffed tank.
    ObjectGuid const victim = s.victim();
    bool const forbidden = !victim.IsEmpty() &&
        std::find(o.forbidden_targets.begin(), o.forbidden_targets.end(), victim) != o.forbidden_targets.end();
    // An idle DPS bot picks up the fallback too, unless its owner told it to stay.
    bool const idleDps = victim.IsEmpty() && !o.fallback_target.IsEmpty() && !ai.holding_position(s.published_at_ms());
    // An idle tank (its enemy is airborne or dead) takes the brain's ground target.
    bool const idleTank = victim.IsEmpty() && o.fallback_target.IsEmpty() && !o.tank_fallback_target.IsEmpty() && !ai.holding_position(s.published_at_ms());
    if (idleTank && emit.start_attack(o.tank_fallback_target))
    {
        ai.set_last_rule_fired("raid:switch_target");
        return true;
    }
    if (forbidden || idleDps)
    {
        if (!o.fallback_target.IsEmpty() && o.fallback_target != victim)
        {
            if (emit.start_attack(o.fallback_target))
            {
                ai.set_last_rule_fired("raid:switch_target");
                return true;
            }
        }
        if (forbidden && !o.tank_fallback_target.IsEmpty() && o.tank_fallback_target != victim && o.tank_fallback_target != o.fallback_target)
        {
            if (emit.start_attack(o.tank_fallback_target))
            {
                ai.set_last_rule_fired("raid:switch_target");
                return true;
            }
        }
        if (forbidden)
        {
            emit.stop_attack();
            ai.set_last_rule_fired("raid:drop_target");
            return true;
        }
    }

    // 5. Melee soak (Patchwerk): stay on the soak enemy whatever the rotation would pick.
    if (!o.attack_target.IsEmpty() && victim != o.attack_target)
        if (emit.start_attack(o.attack_target))
        {
            ai.set_last_rule_fired("raid:soak");
            return true;
        }

    // A melee bot whose target stands in a hazard is not held here: the hazard circles reach PveOrder::avoid_circles and the
    // generic gap-close hold keeps it outside while the rest of its rotation still runs.
    return false;
}

} // namespace Playerbot::Raid

namespace Playerbot {

// Pre-pull orders from the raid brain (Gothik's side split, a rogue disarming a Suppression Device). Above the
// owner-follow rules (990) so the bot walks to its spot and stays there instead of following; below the between-pull
// buffs and dispels (992).
void RegisterRaidIdleRules(IdleRuleRegistry& r)
{
    IdleRule rule;
    rule.name = "idle:raid_prepull";
    rule.priority = 991;
    rule.gate = [](BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
    {
        RaidOrder const& o = s.raw().raid_order;
        return o.active && o.prepull && (o.move || o.cast_spell) && s.is_alive();
    };
    rule.fire = [](BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
    {
        RaidOrder const& o = s.raw().raid_order;
        if (!o.move)
        {
            // In reach: the cast (Disarm Trap on the device). Let a running cast finish.
            if (!s.is_casting())
                emit.cast(o.cast_spell, o.cast_target);
            ai.set_last_rule_fired("idle:raid_prepull");
            return true;
        }
        float x, y, z;
        s.position(x, y, z);
        if ((x - o.move_x) * (x - o.move_x) + (y - o.move_y) * (y - o.move_y) > 2.0f * 2.0f)
            emit.move_to(o.move_x, o.move_y, o.move_z, /*run=*/true);
        ai.set_last_rule_fired("idle:raid_prepull");
        return true;   // holding the spot: no owner follow
    };
    r.register_rule(std::move(rule));

    // A hazard move for a bot that is not in combat yet while its raid fights: a caster held off by the tank-first threat
    // hold, a healer with nothing to heal. DispatchRaidOrder only runs in combat, so without this the dungeon idle rules
    // (converge_to_fight, 720) walked such bots into a Molten Giant's Smash and the brain's move never ran (MC log
    // 2026-10-06). Above the class upkeep (992): leaving fire beats buffing.
    IdleRule move;
    move.name = "idle:raid_move";
    move.priority = 996;
    move.gate = [](BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
    {
        RaidOrder const& o = s.raw().raid_order;
        return o.active && !o.prepull && o.move && s.is_alive();
    };
    move.fire = [](BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32)
    {
        RaidOrder const& o = s.raw().raid_order;
        // A refused move does not claim the tick (see raid:move in DispatchRaidOrder): the idle rules below get the bot.
        if (!emit.move_to(o.move_x, o.move_y, o.move_z, /*run=*/true) && !emit.move_already_emitted(o.move_x, o.move_y, o.move_z))
            return false;
        ai.set_last_rule_fired("idle:raid_move");
        return true;
    };
    r.register_rule(std::move(move));
}

} // namespace Playerbot
