// Classic 1.60 (Forever) Mage - vanilla rotation.
//
// Registered over the retail Arcane / Fire / Frost rotations and the mage
// baseline (spec 0). One rotation serves all four: the nuke is picked from
// the talents the bot actually has (Fire talents -> Fireball, otherwise
// Frostbolt, Fireball while Frostbolt is not yet trained, Arcane Missiles
// as the last resort). Spell ids are vanilla rank 1 and resolve to the
// highest known rank.
//
// Priorities follow Karrz's ike3 vanilla strategies (Karrz1/wow-1.12
// raid10-revamp, playerbot/strategy/mage/*Strategy.cpp, MANGOSBOT_ZERO):
// armor + Arcane Intellect upkeep, Counterspell on casters, Ice Block /
// Frost Nova to survive, Evocation at low mana, boss cooldowns, Arcane
// Explosion on packs in melee, Fire Blast / nuke, wand when dry. Remove
// Lesser Curse on the member the raid dispel plan gives the mage
// (Classic::PickDispel: one bot per cursed member).
// Polymorph on an add (PickGroupCC). Skipped: Blink, Mana Shield,
// Pyroblast opener, Cone of Cold.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Apl_Baseline_Common.h"
#include "ApClassicHeal.h"
#include "ApCrowdControl.h"
#include "ApInterrupt.h"
#include "ApSchoolHint.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/ThreatDiscipline.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::DoAutoAttack;
using ::Playerbot::Combat::baseline_common::HasLiveTarget;

// ---- Vanilla spell ids (rank 1) ----
constexpr uint32 FROSTBOLT          = 116;
constexpr uint32 FIREBALL           = 133;
constexpr uint32 SCORCH             = 2948;
constexpr uint32 FIRE_VULNERABILITY = 22959;   // the Improved Scorch debuff, 5 stacks, shared by every mage
constexpr uint32 ARCANE_MISSILES    = 5143;
constexpr uint32 FIRE_BLAST         = 2136;
constexpr uint32 ARCANE_EXPLOSION   = 1449;
constexpr uint32 FROST_ARMOR        = 168;
constexpr uint32 ICE_ARMOR          = 7302;
constexpr uint32 MAGE_ARMOR         = 6117;
constexpr uint32 ARCANE_INTELLECT   = 1459;
constexpr uint32 COUNTERSPELL       = 2139;
constexpr uint32 FROST_NOVA         = 122;
constexpr uint32 EVOCATION          = 12051;
constexpr uint32 ICE_BARRIER        = 11426;   // Frost talent
constexpr uint32 PRESENCE_OF_MIND   = 12043;   // Arcane talent
constexpr uint32 ARCANE_POWER       = 12042;   // Arcane talent
constexpr uint32 COMBUSTION         = 11129;   // Fire talent
constexpr uint32 PYROBLAST          = 11366;   // Fire talent (only used to detect a Fire build)
constexpr uint32 SHOOT_WAND         = 5019;
// Ice Block is a Frost talent in 1.12 (11958). Icy Veins does not exist in
// vanilla (12472 is Cold Snap there), so neither is used as a burst cooldown.
constexpr uint32 ICE_BLOCK          = 11958;

constexpr uint8 POWER_MANA = 0;

bool Ready(ApPredicateContext const& ctx, uint32 spell)
{
    return ctx.bot.knows_spell(spell) && ctx.bot.is_ready(spell);
}

int32 ManaPct(ApPredicateContext const& ctx)
{
    return ctx.bot.max_power(POWER_MANA) > 0 ? ctx.bot.power_pct(POWER_MANA) : 100;
}

bool VictimIsBossLike(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    return v && (Classic::IsBossUnit(*v) || v->max_hp >= 100'000);
}

// The mana the nukes stop at: a mage with a wand shoots below 35% on ordinary fights (a short rest before the next pull instead of a
// drink to 60%), and casts down to 8% against a boss or when it holds no wand.
int32 NukeManaFloor(ApPredicateContext const& ctx)
{
    return ctx.bot.raw().consumables.wand_school_mask != 0 && !VictimIsBossLike(ctx) ? 35 : 8;
}

uint32 IceBlockSpell(ApPredicateContext const&)
{
    return ICE_BLOCK;
}

// Fire talents -> Fireball; else Frostbolt; Fireball before Frostbolt is
// trained; Arcane Missiles only if nothing else is known.
uint32 NukeSpell(ApPredicateContext const& ctx)
{
    bool const fire_build = ctx.bot.knows_spell(PYROBLAST) || ctx.bot.knows_spell(COMBUSTION);
    if (fire_build && ctx.bot.knows_spell(FIREBALL)) return FIREBALL;
    if (ctx.bot.knows_spell(FROSTBOLT))              return FROSTBOLT;
    if (ctx.bot.knows_spell(FIREBALL))               return FIREBALL;
    return ARCANE_MISSILES;
}

uint32 ArmorSpell(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(MAGE_ARMOR)) return MAGE_ARMOR;
    if (ctx.bot.knows_spell(ICE_ARMOR))  return ICE_ARMOR;
    return FROST_ARMOR;
}

// ======================= Rules =======================

bool ShouldArmor(ApPredicateContext const& ctx)
{
    uint32 const spell = ArmorSpell(ctx);
    if (!Ready(ctx, spell)) return false;
    return !ctx.bot.has_aura(MAGE_ARMOR) && !ctx.bot.has_aura(ICE_ARMOR) && !ctx.bot.has_aura(FROST_ARMOR);
}
void DoArmor(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(ArmorSpell(ctx)); }

// Own Arcane Intellect when solo; Arcane Brilliance from the raid's buff
// plan (idle:classic_raid_buffs) counts.
constexpr uint32 ARCANE_BRILLIANCE  = 23028;
bool ShouldArcaneIntellect(ApPredicateContext const& ctx)
{
    return !ctx.bot.in_combat() && Ready(ctx, ARCANE_INTELLECT) && !ctx.bot.has_aura(ARCANE_INTELLECT)
        && !ctx.bot.has_aura(ARCANE_BRILLIANCE);
}

bool ShouldRemoveCurse(ApPredicateContext const& ctx) { return ClassicHeal::ShouldAssignedDispel(ctx); }
void DoRemoveCurse(ApPredicateContext const& ctx, BotIntentEmitter& e) { ClassicHeal::DoAssignedDispel(ctx, e); }
void DoArcaneIntellect(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ARCANE_INTELLECT); }

bool ShouldIceBlock(ApPredicateContext const& ctx)
{
    uint32 const spell = IceBlockSpell(ctx);
    return ctx.bot.in_combat() && ctx.bot.attackers_count() >= 1 && ctx.bot.hp_pct() <= 20
        && Ready(ctx, spell) && !ctx.bot.has_aura(spell);
}
void DoIceBlock(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(IceBlockSpell(ctx)); }

// Cold Snap (Frost talent, 12472 in the Classic data) resets the spent Ice Block.
// Playerbot.Behavior.RangedSlow.
constexpr uint32 COLD_SNAP = 12472;
bool ShouldColdSnap(ApPredicateContext const& ctx)
{
    return Services::Config().behavior_ranged_slow() && ctx.bot.in_combat() && ctx.bot.attackers_count() >= 1
        && ctx.bot.hp_pct() <= 35 && ctx.bot.knows_spell(ICE_BLOCK) && !ctx.bot.is_ready(ICE_BLOCK)
        && !ctx.bot.has_aura(ICE_BLOCK) && Ready(ctx, COLD_SNAP);
}
void DoColdSnap(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(COLD_SNAP); }

bool ShouldIceBarrier(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.attackers_count() >= 1 && Ready(ctx, ICE_BARRIER)
        && !ctx.bot.has_aura(ICE_BARRIER);
}
void DoIceBarrier(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ICE_BARRIER); }

// Root things that are chewing on the mage. In a group the tank holds
// aggro, so only when the mage is actually hurt; solo it is a kiting tool.
bool ShouldFrostNova(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat() || ctx.bot.attackers_count() < 1 || !Ready(ctx, FROST_NOVA)) return false;
    if (ctx.bot.enemies_within(8.0f) < 1 || VictimIsBossLike(ctx)) return false;
    int32 const threshold = ctx.group.members() ? 50 : 75;
    if (ctx.bot.hp_pct() > threshold) return false;
    // Self-defence root: only a crowd-controlled mob in the 10y ring vetoes it
    // (the nova's damage breaks a Polymorph / Sap / Shackle).
    float x, y, z; ctx.bot.position(x, y, z);
    // So does a mob the raid school rule says avoids frost (Nova's damage part).
    AoeHazardKind const k = FindAoeHazard(ctx.bot, x, y, z, 10.0f, /*damaging*/ true, /*tank*/ false, FROST_NOVA).kind;
    return k != AoeHazardKind::CrowdControlled && k != AoeHazardKind::SchoolAvoided;
}
void DoFrostNova(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FROST_NOVA); }

bool ShouldCounterspell(ApPredicateContext const& ctx)
{
    // Interrupt rotation (ApInterrupt.h): the plan's pick within 30y.
    return HasLiveTarget(ctx) && Interrupt::Should(ctx, COUNTERSPELL);
}
void DoCounterspell(ApPredicateContext const& ctx, BotIntentEmitter& e) { Interrupt::Cast(ctx, e, COUNTERSPELL); }

bool ShouldEvocation(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ManaPct(ctx) <= 15 && Ready(ctx, EVOCATION);
}
void DoEvocation(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EVOCATION); }

// Boss cooldowns (all talent spells; Ready() skips them when not trained):
// Classic::BossFight - inside a dungeon or raid an engaged boss or encounter,
// never trash; in the open world a boss-like victim.
bool BossCooldownGate(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Classic::BossFight(ctx.bot) && ctx.bot.current_cast_spell_id() == 0;
}

bool ShouldArcanePower(ApPredicateContext const& ctx)
{
    return BossCooldownGate(ctx) && ManaPct(ctx) >= 40 && Ready(ctx, ARCANE_POWER) && !ctx.bot.has_aura(ARCANE_POWER);
}
void DoArcanePower(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ARCANE_POWER);
    Classic::NoteCooldownUse(ctx.bot, ARCANE_POWER, "Arcane Power");
}

bool ShouldPresenceOfMind(ApPredicateContext const& ctx)
{
    return BossCooldownGate(ctx) && ManaPct(ctx) >= 30 && Ready(ctx, PRESENCE_OF_MIND)
        && !ctx.bot.has_aura(PRESENCE_OF_MIND);
}
void DoPresenceOfMind(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(PRESENCE_OF_MIND);
    Classic::NoteCooldownUse(ctx.bot, PRESENCE_OF_MIND, "Presence of Mind");
}

bool ShouldCombustion(ApPredicateContext const& ctx)
{
    return BossCooldownGate(ctx) && ManaPct(ctx) >= 30 && Ready(ctx, COMBUSTION) && !ctx.bot.has_aura(COMBUSTION);
}
void DoCombustion(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(COMBUSTION);
    Classic::NoteCooldownUse(ctx.bot, COMBUSTION, "Combustion");
}

bool ShouldArcaneExplosion(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.enemies_within(8.0f) >= 3 && ManaPct(ctx) >= 30
        && Ready(ctx, ARCANE_EXPLOSION)
        // Never over a sheep, an unpulled mob or a pack the tank is still
        // picking up (Bot/ThreatDiscipline.h). Arcane Explosion reaches 10y.
        && AoeSafeAroundSelf(ctx.bot, 10.0f, /*damaging*/ true, /*tank*/ false, "Arcane Explosion", ARCANE_EXPLOSION);
}
void DoArcaneExplosion(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ARCANE_EXPLOSION); }

bool ShouldFireBlast(ApPredicateContext const& ctx)
{
    // Instant but on the global cooldown and mana-hungry: worth it while moving (a hard cast cannot run) or as a filler for a fire
    // build with mana to spare; a frost / arcane mage standing still casts its main spell instead.
    bool const fire_build = ctx.bot.knows_spell(PYROBLAST) || ctx.bot.knows_spell(COMBUSTION);
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 25 && (ctx.bot.is_moving() || (fire_build && ManaPct(ctx) >= 50))
        && (Ready(ctx, FIRE_BLAST) && ctx.bot.victim_in_spell_range(FIRE_BLAST));
}
void DoFireBlast(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(FIRE_BLAST, ctx.bot.victim()); }

bool ShouldNuke(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= NukeManaFloor(ctx) && Ready(ctx, NukeSpell(ctx));
}
void DoNuke(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(NukeSpell(ctx), ctx.bot.victim()); }

// Raid school rule (ApSchoolHint.h): with a preferred school on the victim,
// its spell goes first among the nukes (Frostbolt on frost, Fire Blast /
// Fireball on fire, Arcane Missiles on arcane), same mana gates as the nuke.
bool NukeUsable(ApPredicateContext const& ctx, uint32 spell)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= NukeManaFloor(ctx) && Ready(ctx, spell);
}
bool ShouldFrostboltNuke(ApPredicateContext const& ctx)      { return NukeUsable(ctx, FROSTBOLT); }
bool ShouldFireballNuke(ApPredicateContext const& ctx)       { return NukeUsable(ctx, FIREBALL); }
bool ShouldArcaneMissilesNuke(ApPredicateContext const& ctx) { return NukeUsable(ctx, ARCANE_MISSILES); }

SchoolHint::SchoolChoice const kSchoolChoices[] = {
    { FROSTBOLT,       ShouldFrostboltNuke },
    { FIRE_BLAST,      ShouldFireBlast },
    { FIREBALL,        ShouldFireballNuke },
    { ARCANE_MISSILES, ShouldArcaneMissilesNuke },
};
// A fire mage on a boss keeps Fire Vulnerability at 5 stacks with Scorch before it throws Fireballs (raid vanilla rule); the stacks are
// shared, so the mage only casts it when no mage has them at 5 with time left.
bool ShouldScorch(ApPredicateContext const& ctx)
{
    if (!(ctx.bot.knows_spell(PYROBLAST) || ctx.bot.knows_spell(COMBUSTION)) || !Classic::BossFight(ctx.bot))
        return false;
    if (!NukeUsable(ctx, SCORCH)) return false;
    AuraEntry const* fv = ctx.bot.find_aura_any_caster(FIRE_VULNERABILITY, ctx.bot.victim());
    return !fv || fv->stacks < 5 || fv->remaining.count() < 8000;
}
void DoScorch(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SCORCH, ctx.bot.victim()); }

bool ShouldSchoolNuke(ApPredicateContext const& ctx) { return SchoolHint::ShouldCast(ctx, kSchoolChoices); }
void DoSchoolNuke(ApPredicateContext const& ctx, BotIntentEmitter& e) { SchoolHint::DoCast(ctx, e, kSchoolChoices); }

bool ShouldWand(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && (Ready(ctx, SHOOT_WAND) && ctx.bot.victim_in_spell_range(SHOOT_WAND)) && ctx.bot.current_cast_spell_id() == 0
        && ManaPct(ctx) < NukeManaFloor(ctx);
}
void DoWand(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SHOOT_WAND, ctx.bot.victim()); }

// Polymorph: Beasts / Humanoids / Critters. In a group an add of a 3+ mob pull (never the kill target,
// the tank's target or a boss); solo an extra attacker. See PickGroupCC.
constexpr uint32 POLYMORPH = 118;
bool ShouldCrowdControl(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ctx.bot.in_combat() && !ctx.bot.is_moving() && ManaPct(ctx) >= 15
        && Ready(ctx, POLYMORPH) && !PickGroupCC(ctx, POLYMORPH).IsEmpty();
}
void DoCrowdControl(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(POLYMORPH, PickGroupCC(ctx, POLYMORPH));
}

ApRule const kMageRules[] = {
    { ShouldIceBlock,        DoIceBlock,        "classic_mage:ice_block" },
    { ShouldColdSnap,        DoColdSnap,        "classic_mage:cold_snap" },
    { ShouldFrostNova,       DoFrostNova,       "classic_mage:frost_nova" },
    { ShouldCounterspell,    DoCounterspell,    "classic_mage:counterspell" },
    { ShouldRemoveCurse,     DoRemoveCurse,     "classic_mage:remove_lesser_curse" },
    { ShouldCrowdControl,    DoCrowdControl,    "classic_mage:polymorph" },
    { ShouldEvocation,       DoEvocation,       "classic_mage:evocation" },
    { ShouldArmor,           DoArmor,           "classic_mage:armor" },
    { ShouldArcaneIntellect, DoArcaneIntellect, "classic_mage:arcane_intellect" },
    { ShouldIceBarrier,      DoIceBarrier,      "classic_mage:ice_barrier" },
    { ShouldArcanePower,     DoArcanePower,     "classic_mage:arcane_power" },
    { ShouldPresenceOfMind,  DoPresenceOfMind,  "classic_mage:presence_of_mind" },
    { ShouldCombustion,      DoCombustion,      "classic_mage:combustion" },
    { ShouldArcaneExplosion, DoArcaneExplosion, "classic_mage:arcane_explosion" },
    { ShouldScorch,          DoScorch,          "classic_mage:scorch" },
    { ShouldSchoolNuke,      DoSchoolNuke,      "classic_mage:school_nuke" },
    { ShouldFireBlast,       DoFireBlast,       "classic_mage:fire_blast" },
    { ShouldNuke,            DoNuke,            "classic_mage:nuke" },
    { ShouldWand,            DoWand,            "classic_mage:wand" },
    { HasLiveTarget,         DoAutoAttack,      "classic_mage:auto_attack" },
};

} // namespace

void RegisterApl_Classic_Mage()
{
    ApRotation const mage{ std::span<ApRule const>(kMageRules) };
    RegisterRotationOverride(CLASS_MAGE, 0, mage);
    RegisterRotationOverride(CLASS_MAGE, 62, mage);
    RegisterRotationOverride(CLASS_MAGE, 63, mage);
    RegisterRotationOverride(CLASS_MAGE, 64, mage);
}

} // namespace Playerbot::Combat
