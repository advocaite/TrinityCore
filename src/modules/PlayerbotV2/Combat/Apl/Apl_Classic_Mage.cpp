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
// Polymorph on an add (PickGroupCC). Forever procs, in the order the
// wowsims/forever (MIT) frost and fire lists use them: Ice Lance on the last
// Fingers of Frost charge, Pyroblast at 3 Heating Up stacks, Arcane Wrath
// (Arcane talent) on cooldown, right after Presence of Mind. Skipped: Blink,
// Mana Shield, Pyroblast opener, Cone of Cold.

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
constexpr uint32 BLIZZARD           = 10;
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

// A Fire mage: the specialization says so (62 Arcane and 64 Frost never are). Knowing Pyroblast or Combustion alone cannot decide it: the 166 Arcane
// mage held them and ran the fire rules, 111 Scorch casts in a leg and no Frostbolt. With no specialization set the spells decide.
bool FireBuild(ApPredicateContext const& ctx)
{
    uint32 const spec = ctx.bot.spec();
    if (spec == 63) return true;
    if (spec == 62 || spec == 64) return false;
    return ctx.bot.knows_spell(PYROBLAST) || ctx.bot.knows_spell(COMBUSTION);
}

// Fire talents -> Fireball; else Frostbolt; Fireball before Frostbolt is
// trained; Arcane Missiles only if nothing else is known.
uint32 NukeSpell(ApPredicateContext const& ctx)
{
    bool const fire_build = FireBuild(ctx);
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

// Blizzard: an 8 s ground channel of 8 ticks on every enemy within 8 yd of the point (classic_spell_ground_area_damage, rank 6: 149 frost
// plus 4.17% spell power a tick). Three enemies in the area out-damage the single-target nukes: a T2 mage's Frostbolt is about 330 a
// second on one target, Blizzard about 165 a second on each one in it. Cast on the victim from range, so it reaches the piles a raid's
// tanks hold (bwl_nefarian 1108h: 3-5 drakonids on each tank, and the mages' one area spell, Arcane Explosion, reaches 10 yd round
// the mage only). Enemies that avoid frost (a Blue Drakonid's resistance, the raid school hint) are not counted. Standing still, with
// the mana for the channel, and never over a crowd-controlled, unpulled or not yet secured mob (AoeSafeAroundVictim).
constexpr float kBlizzardRadius = 8.0f;
constexpr int kBlizzardMinTargets = 3;
bool ShouldBlizzard(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v || !HasLiveTarget(ctx) || !ctx.bot.in_combat() || ctx.bot.is_moving() || ManaPct(ctx) < 40)
        return false;
    if (ctx.bot.raw().raid_order.no_aoe || !(Ready(ctx, BLIZZARD) && ctx.bot.victim_in_spell_range(BLIZZARD)))
        return false;
    uint8 const frost = BotIntentEmitter::SpellSchoolMaskOf(BLIZZARD, 0);
    PveOrder const& po = ctx.bot.raw().pve_order;
    int n = 0;
    float const r2 = kBlizzardRadius * kBlizzardRadius;
    for (NearbyUnit const& u : ctx.bot.nearby_enemies())
    {
        if (u.hp <= 0 || u.untargetable || u.is_pacified || u.is_player || !u.unit_in_combat) continue;
        float const dx = u.x - v->x, dy = u.y - v->y, dz = u.z - v->z;
        if (dx * dx + dy * dy + dz * dz > r2) continue;
        if (PveOrder::SchoolHint const* h = po.school_hint_for(u.guid); h && (h->avoid_mask & frost)) continue;
        ++n;
    }
    return n >= kBlizzardMinTargets && AoeSafeAroundVictim(ctx.bot, kBlizzardRadius, "Blizzard", BLIZZARD);
}
void DoBlizzard(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (NearbyUnit const* v = ctx.bot.victim_info())
        e.cast_at(BLIZZARD, v->x, v->y, v->z);
}

bool ShouldFireBlast(ApPredicateContext const& ctx)
{
    // Instant but on the global cooldown and mana-hungry: worth it while moving (a hard cast cannot run) or as a filler for a fire
    // build with mana to spare; a frost / arcane mage standing still casts its main spell instead.
    bool const fire_build = FireBuild(ctx);
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 25 && (ctx.bot.is_moving() || (fire_build && ManaPct(ctx) >= 50))
        && (Ready(ctx, FIRE_BLAST) && ctx.bot.victim_in_spell_range(FIRE_BLAST));
}
void DoFireBlast(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(FIRE_BLAST, ctx.bot.victim()); }

bool ShouldNuke(ApPredicateContext const& ctx)
{
    uint32 const spell = NukeSpell(ctx);
    return HasLiveTarget(ctx) && ManaPct(ctx) >= NukeManaFloor(ctx) && (Ready(ctx, spell) && ctx.bot.victim_in_spell_range(spell));
}
void DoNuke(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(NukeSpell(ctx), ctx.bot.victim()); }

// Raid school rule (ApSchoolHint.h): with a preferred school on the victim,
// its spell goes first among the nukes (Frostbolt on frost, Fire Blast /
// Fireball on fire, Arcane Missiles on arcane), same mana gates as the nuke.
bool NukeUsable(ApPredicateContext const& ctx, uint32 spell)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= NukeManaFloor(ctx) && (Ready(ctx, spell) && ctx.bot.victim_in_spell_range(spell));
}
// Arcane Wrath (Arcane talent 1326079, trainer ranks 1326080-1326082): 3.5 s cast, 6 s cooldown, extra spell damage of 5% of the
// mage's maximum mana (classic_spell_mage_arcane_wrath). The wowsims/forever (MIT) arcane list casts it on cooldown, straight
// after Presence of Mind, above its other spells. The hard cast waits while the bot moves; under Presence of Mind it is instant.
constexpr uint32 ARCANE_WRATH = 1326079;
bool ShouldArcaneWrath(ApPredicateContext const& ctx)
{
    return ctx.bot.current_cast_spell_id() == 0 && (!ctx.bot.is_moving() || ctx.bot.has_aura(PRESENCE_OF_MIND))
        && NukeUsable(ctx, ARCANE_WRATH);
}
void DoArcaneWrath(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(ARCANE_WRATH, ctx.bot.victim()); }

bool ShouldFrostboltNuke(ApPredicateContext const& ctx)      { return NukeUsable(ctx, FROSTBOLT); }
bool ShouldFireballNuke(ApPredicateContext const& ctx)       { return NukeUsable(ctx, FIREBALL); }
bool ShouldArcaneMissilesNuke(ApPredicateContext const& ctx) { return NukeUsable(ctx, ARCANE_MISSILES); }

SchoolHint::SchoolChoice const kSchoolChoices[] = {
    { FROSTBOLT,       ShouldFrostboltNuke },
    { FIRE_BLAST,      ShouldFireBlast },
    { FIREBALL,        ShouldFireballNuke },
    { ARCANE_WRATH,    ShouldArcaneWrath },
    { ARCANE_MISSILES, ShouldArcaneMissilesNuke },
};
// A fire mage on a boss keeps Fire Vulnerability at 5 stacks with Scorch before it throws Fireballs (raid vanilla rule); the stacks are
// shared, so the mage only casts it when no mage has them at 5 with time left.
bool ShouldScorch(ApPredicateContext const& ctx)
{
    if (!FireBuild(ctx) || !Classic::BossFight(ctx.bot))
        return false;
    if (!NukeUsable(ctx, SCORCH)) return false;
    AuraEntry const* fv = ctx.bot.find_aura_any_caster(FIRE_VULNERABILITY, ctx.bot.victim());
    return !fv || fv->stacks < 5 || fv->remaining.count() < 8000;
}
void DoScorch(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SCORCH, ctx.bot.victim()); }

// Forever procs (server scripts classic_talents_mage_shaman.cpp). Fingers of Frost puts 2 charges (400670) on the mage; each cast
// spends one and counts as hitting a Frozen target, and Ice Lance (400640, Forever trainer, level 28) then deals three times its
// damage. Like the wowsims/forever frost list, the first charge goes to the Frostbolt and the last one to an Ice Lance.
constexpr uint32 ICE_LANCE               = 400640;
constexpr uint32 FINGERS_OF_FROST_STACKS = 400670;
// Heating Up (400625): each stack takes 25% off the next Pyroblast's cast time, 3 stacks at most, 20 s. The wowsims/forever fire
// list casts Pyroblast at 3 stacks; before that the bot never cast Pyroblast at all.
constexpr uint32 HEATING_UP_BUFF         = 400625;

bool ShouldIceLanceFingers(ApPredicateContext const& ctx)
{
    return ctx.bot.aura_stacks(FINGERS_OF_FROST_STACKS) == 1 && ctx.bot.current_cast_spell_id() == 0 && NukeUsable(ctx, ICE_LANCE);
}
void DoIceLanceFingers(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(ICE_LANCE, ctx.bot.victim()); }

bool ShouldPyroblastHeatingUp(ApPredicateContext const& ctx)
{
    return FireBuild(ctx) && ctx.bot.aura_stacks(HEATING_UP_BUFF) >= 3 && !ctx.bot.is_moving()
        && ctx.bot.current_cast_spell_id() == 0 && NukeUsable(ctx, PYROBLAST);
}
void DoPyroblastHeatingUp(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(PYROBLAST, ctx.bot.victim()); }

bool ShouldSchoolNuke(ApPredicateContext const& ctx) { return SchoolHint::ShouldCast(ctx, kSchoolChoices); }
void DoSchoolNuke(ApPredicateContext const& ctx, BotIntentEmitter& e) { SchoolHint::DoCast(ctx, e, kSchoolChoices); }

bool ShouldWand(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ctx.bot.raw().consumables.wand_school_mask != 0 && (Ready(ctx, SHOOT_WAND) && ctx.bot.victim_in_spell_range(SHOOT_WAND)) && ctx.bot.current_cast_spell_id() == 0
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
    { ShouldBlizzard,        DoBlizzard,        "classic_mage:blizzard" },
    { ShouldScorch,          DoScorch,          "classic_mage:scorch" },
    { ShouldIceLanceFingers, DoIceLanceFingers, "classic_mage:ice_lance_fingers" },
    { ShouldPyroblastHeatingUp, DoPyroblastHeatingUp, "classic_mage:pyroblast_heating_up" },
    { ShouldSchoolNuke,      DoSchoolNuke,      "classic_mage:school_nuke" },
    { ShouldArcaneWrath,     DoArcaneWrath,     "classic_mage:arcane_wrath" },
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
