// RaidOrder — what the raid brain wants one bot to do right now.
//
// Published by Raid::Brain (world thread, ~300ms cadence) and copied into
// BotSnapshot::raid_order by the snapshot builder, with the same
// writer-before-parallel-build contract as PveGroupCoordinator's PveOrder.
// Consumed at the top of the in-combat dispatch (Raid::DispatchRaidOrder).
// `active == false` means the bot is not on a raid map with rules and every
// consumer runs the normal bot logic.

#pragma once

#include "ObjectGuid.h"
#include <cstdint>
#include <vector>

namespace Playerbot {

struct RaidOrder
{
    enum class CastKind : uint8_t { None, Taunt, Dispel, Purge, CrowdControl, Nuke, Disarm, StanceDance };

    bool active = false;

    // Out-of-combat orders before a pull (Gothik's side split, a rogue disarming a Suppression Device). Only the move and
    // cast fields are set; consumed by the idle rule idle:raid_prepull, which also keeps the bot from following its owner.
    bool prepull = false;

    // Encounter movement (hazard escape, carrier run, spread, role position).
    bool  move = false;
    float move_x = 0.f, move_y = 0.f, move_z = 0.f;
    char const* move_reason = "";   // static string literal (log/diag only)
    // The bot stands at its encounter role position (a Stand rule, within its tolerance): the generic in-fight formation walk and the
    // clump separation leave it there (each would walk it off the post, and the next brain pass walk it back).
    bool at_post = false;
    // The enemy the bot's Stand post is placed for (set while the post applies, on it or walking back): the ranged spacing step does not
    // walk the bot away from it. The post already says where to stand against that enemy (Victor Nefarius' Shadow Blink lands him next to
    // a random raider every 20-25 s; bwl_nefarian 1108h: 96 steps back from him in one pull, the camp scattered).
    ObjectGuid post_enemy;
    // The bot is inside a StandBehind sector that holds its ground (StandBehind::holdGround): the ranged spacing step does not walk it away
    // from any elite. The sector is where the encounter wants the adds to come.
    bool hold_ground = false;

    // Melee bot whose target stands inside a hazard that applies to it
    // (Inferno, Blast Wave): pause the rotation so it does not chase back in.
    bool hold_melee = false;

    // One elected cast for this bot (no two bots get the same job).
    CastKind   cast_kind = CastKind::None;
    uint32_t   cast_spell = 0;     // a rank the bot knows (the emitter casts the highest rank)
    ObjectGuid cast_target;      // a unit, or a game object for Disarm

    // Enemies this bot must not attack right now: never-target entries
    // (non-tanks), crowd-controlled adds, Majordomo's reflecting adds, the
    // swap enemy for a tank that carries its debuff.
    std::vector<ObjectGuid> forbidden_targets;
    // Where a DPS bot goes when its victim is forbidden (empty = stop).
    ObjectGuid fallback_target;
    // Where an off-tank goes when the enemy it holds becomes forbidden (a banished add): the raid's main target, instead of
    // dropping it every tick and standing idle (MC Garr 2026-10-10: raid:drop_target was an off-tank's main rule, 0 boss damage).
    // Unlike fallback_target it never picks a target for an idle tank, which waits for its pack mob.
    ObjectGuid tank_fallback_target;

    // Tank-first hold: a DPS bot waits on this enemy until the tank has a
    // threat lead (bounded by a few seconds so a missing tank never freezes
    // the raid). Empty = attack freely.
    ObjectGuid hold_target;

    // A NoAoe rule enemy is within 40 yards (Garr's Firesworn).
    bool no_aoe = false;

    // Melee soak (Patchwerk): keep attacking this enemy whatever the rotation would pick.
    ObjectGuid attack_target;

    // The bot carries the enemy on it to its tank instead of dropping threat (Feign Death, Vanish, Fade): the bot that pulled a Son of
    // Hakkar brings it up to the raid, where the off-tank picks it up and it dies next to the crowd.
    bool keep_aggro = false;

    // Mind-controlled creature (Razuvious' understudies): what the bot makes it do. The bot itself only holds the channel.
    ObjectGuid charm_attack;      // attack and chase this enemy
    uint32_t   charm_spell = 0;   // cast this creature spell now (Taunt, Shield Wall)
    ObjectGuid charm_target;      // its target (empty = self)
    bool       charm_hold = false; // keep the control, give no command (Faerlina's worshipper waiting for her enrage)

    // Use this game object now (the brain already walked the bot into reach): Razorgore's Orb of Domination.
    ObjectGuid use_object;

    // The bot stands inside this scripted area trigger: fire it (C'Thun's stomach exit, which a client would send).
    uint32_t area_trigger = 0;

    // Remove this aura from the bot (Blessing of Protection on a tank during Nefarian's paladin call).
    uint32_t cancel_aura = 0;

    // Pause the rotation (Nefarian's priest call). Movement and elected jobs still run.
    bool hold_rotation = false;

    // While the bot carries this aura (Mandokir's Threatening Gaze): nothing at all, see HoldByAura::still. Set for every matching bot for
    // the whole fight, the bot tests its own auras each tick (HoldStill).
    uint32_t still_aura = 0;

    // The player's boss plan (BossTactics "no fear", "no sheep"): CcDeny kinds left out during this fight, added to the owner's own
    // crowd-control orders by the snapshot builder.
    uint16_t cc_deny = 0;

    // A boss fight script (RaidFightScript.h) owns this DPS bot's target and start: the tank-first threat holds and the skull / kill-focus
    // pickers stand down, the bot attacks script_focus (empty: the generic rules pick). Never set on tanks or healers.
    bool script_owns = false;
    ObjectGuid script_focus;
    // The boss script names this bot's kill target: it wins over every generic picker (skull, kill focus, fallback).
    bool script_names_target() const { return active && script_owns && !script_focus.IsEmpty(); }

    // Snapshot buffers are recycled: clear everything but keep the vector's capacity.
    void reset()
    {
        std::vector<ObjectGuid> keep;
        keep.swap(forbidden_targets);
        keep.clear();
        *this = RaidOrder{};
        forbidden_targets.swap(keep);
    }
};

} // namespace Playerbot
