// Classic 1.60 (Forever) Rogue - vanilla rotation.
//
// Registered over the retail Assassination / Outlaw / Subtlety rotations and
// the rogue baseline (spec 0): one melee DPS rotation for every rogue spec.
//
// Spell ids are vanilla rank 1; the snapshot view and API::cast_spell resolve
// them to the highest rank the bot knows. Combo points are read as power
// type 4 (same as the retail Outlaw rotation).
//
// Openers: on a target nobody is fighting yet the rogue stealths, Saps a
// humanoid standing next to it, then opens with Ambush (behind, dagger),
// Cheap Shot, or Garrote (behind, before Cheap Shot is learned). Backstab
// replaces Sinister Strike when behind the target with a dagger. "Behind"
// is read from the target's facing in the snapshot. Poisons are kept up out
// of combat by idle:classic_weapon (Bot/ClassicUpkeep.cpp).
// Skipped: Kidney Shot, Vanish, Gouge. Feint: threat dump at 60% of the tank's threat (idea from Karrz's VMaNGOS bots).
//
// Priorities follow Karrz's ike3 vanilla strategies (Karrz1/wow-1.12
// raid10-revamp, playerbot/strategy/rogue/*Strategy.cpp, MANGOSBOT_ZERO).

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Apl_Baseline_Common.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/ClassicUpkeep.h"   // Classic::BossFight, NoteCooldownUse
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"
#include "ApCrowdControl.h"
#include "ApInterrupt.h"
#include "ApThreatDump.h"
#include "ItemTemplate.h"
#include "Player.h"   // EQUIPMENT_SLOT_MAINHAND
#include "ObjectMgr.h"
#include <cmath>

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::DoAutoAttack;
using ::Playerbot::Combat::baseline_common::HasLiveTarget;

// ---- Vanilla spell ids (rank 1) ----
constexpr uint32 SINISTER_STRIKE = 1752;
constexpr uint32 SLICE_AND_DICE  = 5171;
constexpr uint32 EVISCERATE      = 2098;
constexpr uint32 EXPOSE_ARMOR    = 8647;
constexpr uint32 SUNDER_ARMOR    = 7386;   // a warrior's debuff, read to skip Expose Armor
constexpr uint32 KICK            = 1766;
constexpr uint32 EVASION         = 5277;
constexpr uint32 FEINT           = 1966;    // threat drop
constexpr uint32 ADRENALINE_RUSH = 13750;   // Combat talent
constexpr uint32 BLADE_FLURRY    = 13877;   // Combat talent
constexpr uint32 COLD_BLOOD      = 14177;   // Assassination talent
constexpr uint32 RIPOSTE         = 14251;   // Combat talent, reactive (after a parry)
constexpr uint32 HEMORRHAGE      = 16511;   // Subtlety talent
constexpr uint32 STEALTH         = 1784;
constexpr uint32 SAP             = 6770;
constexpr uint32 CHEAP_SHOT      = 1833;
constexpr uint32 AMBUSH          = 8676;    // dagger, from behind
constexpr uint32 GARROTE         = 703;     // from behind
constexpr uint32 BACKSTAB        = 53;      // dagger, from behind

constexpr uint8 POWER_ENERGY = 3;
constexpr uint8 POWER_COMBO  = 4;

int32 Energy(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_ENERGY); }
int32 Combo(ApPredicateContext const& ctx)  { return ctx.bot.power(POWER_COMBO); }

bool Ready(ApPredicateContext const& ctx, uint32 spell)
{
    return ctx.bot.knows_spell(spell) && ctx.bot.is_ready(spell);
}

bool VictimIsBossLike(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    return v && (Classic::IsBossUnit(*v) || v->max_hp >= 100'000);
}

int32 VictimHpPct(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v || v->max_hp <= 0) return 100;
    return int32(int64(v->hp) * 100 / v->max_hp);
}

float DistTo(ApPredicateContext const& ctx, NearbyUnit const& u)
{
    auto const& me = ctx.bot.raw().position;
    float const dx = u.x - me.x, dy = u.y - me.y, dz = u.z - me.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Outside the target's front half (Ambush / Garrote / Backstab).
bool BehindVictim(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return false;
    auto const& me = ctx.bot.raw().position;
    float a = std::atan2(me.y - v->y, me.x - v->x) - v->o;
    while (a > float(M_PI))  a -= 2.f * float(M_PI);
    while (a < -float(M_PI)) a += 2.f * float(M_PI);
    return std::fabs(a) > float(M_PI) / 2.f;
}

bool MainHandDagger(ApPredicateContext const& ctx)
{
    uint32 const entry = ctx.bot.raw().inventory.equipped[EQUIPMENT_SLOT_MAINHAND].entry;
    ItemTemplate const* t = entry ? sObjectMgr->GetItemTemplate(entry) : nullptr;
    return t && t->GetClass() == ITEM_CLASS_WEAPON && t->GetSubClass() == ITEM_SUBCLASS_WEAPON_DAGGER;
}

bool Stealthed(ApPredicateContext const& ctx) { return ctx.bot.has_aura(STEALTH); }

// ======================= Openers =======================

// A fresh target: nobody is fighting it yet.
bool UnengagedVictim(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    return v && v->hp > 0 && v->victim.IsEmpty();
}

bool ShouldStealth(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat() || Stealthed(ctx) || ctx.bot.is_mounted() || !UnengagedVictim(ctx) || !Ready(ctx, STEALTH))
        return false;
    float const d = DistTo(ctx, *ctx.bot.victim_info());
    return d > 8.f && d < 40.f;
}
void DoStealth(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(STEALTH); }

// Sap a humanoid idling next to the target (not fighting anyone), so the
// pull is one mob. Breaks stealth in 1.12, so it goes before the opener.
ObjectGuid SapTarget(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return ObjectGuid::Empty;
    for (NearbyUnit const& u : ctx.bot.raw().combat.nearby_enemies)
    {
        if (u.guid == v->guid || u.hp <= 0 || u.is_player || u.is_cc_locked || !u.victim.IsEmpty()) continue;
        if (u.untargetable || u.is_pacified || (u.is_dungeon_boss || u.is_boss_rank) || !CanBeCCd(SAP, u.creature_type)) continue;
        float const dx = u.x - v->x, dy = u.y - v->y;
        if (dx * dx + dy * dy > 12.f * 12.f || DistTo(ctx, u) > 10.f) continue;
        return u.guid;
    }
    return ObjectGuid::Empty;
}
bool ShouldSap(ApPredicateContext const& ctx)
{
    return Stealthed(ctx) && !ctx.bot.in_combat() && Energy(ctx) >= 65 && Ready(ctx, SAP)
        && !SapTarget(ctx).IsEmpty();
}
void DoSap(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SAP, SapTarget(ctx)); }

bool ShouldAmbush(ApPredicateContext const& ctx)
{
    return Stealthed(ctx) && HasLiveTarget(ctx) && Energy(ctx) >= 60 && (Ready(ctx, AMBUSH) && ctx.bot.victim_in_spell_range(AMBUSH))
        && MainHandDagger(ctx) && BehindVictim(ctx);
}
void DoAmbush(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(AMBUSH, ctx.bot.victim()); }

bool ShouldCheapShot(ApPredicateContext const& ctx)
{
    return Stealthed(ctx) && HasLiveTarget(ctx) && Energy(ctx) >= 60 && (Ready(ctx, CHEAP_SHOT) && ctx.bot.victim_in_spell_range(CHEAP_SHOT));
}
void DoCheapShot(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CHEAP_SHOT, ctx.bot.victim()); }

bool ShouldGarrote(ApPredicateContext const& ctx)
{
    return Stealthed(ctx) && HasLiveTarget(ctx) && Energy(ctx) >= 50 && !ctx.bot.knows_spell(CHEAP_SHOT)
        && (Ready(ctx, GARROTE) && ctx.bot.victim_in_spell_range(GARROTE)) && BehindVictim(ctx);
}
void DoGarrote(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(GARROTE, ctx.bot.victim()); }

// ======================= Rules =======================

bool ShouldEvasion(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.hp_pct() <= 35 && ctx.bot.attackers_count() >= 1
        && Ready(ctx, EVASION) && !ctx.bot.has_aura(EVASION);
}
void DoEvasion(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(EVASION); }

// Feint when close to pulling the tank's aggro (VMaNGOS medium-threat trigger: 60% of the tank's threat).
bool ShouldFeint(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.group.members() && Ready(ctx, FEINT)
        && ThreatDump::ThreatPct(ctx.bot) >= ThreatDump::kMediumThreatPct;
}
void DoFeint(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FEINT); }

bool ShouldKick(ApPredicateContext const& ctx)
{
    // Interrupt rotation (ApInterrupt.h): the plan's pick in Kick range.
    return HasLiveTarget(ctx) && Energy(ctx) >= 25 && Interrupt::Should(ctx, KICK);
}
void DoKick(ApPredicateContext const& ctx, BotIntentEmitter& e) { Interrupt::Cast(ctx, e, KICK); }

// Kick on cooldown: Kidney Shot interrupts instead (ApInterrupt.h picks it only while Kick recovers).
bool ShouldKidneyInterrupt(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Energy(ctx) >= 25 && Interrupt::Should(ctx, Interrupt::KIDNEY_SHOT);
}
void DoKidneyInterrupt(ApPredicateContext const& ctx, BotIntentEmitter& e) { Interrupt::Cast(ctx, e, Interrupt::KIDNEY_SHOT); }

// Adrenaline Rush / Blade Flurry window: inside a dungeon or raid a boss
// fight only (Classic::BossFight, never trash); in the open world a boss-like
// victim or two mobs in melee.
bool BurstWindow(ApPredicateContext const& ctx)
{
    if (ctx.bot.is_in_instance())
        return Classic::BossFight(ctx.bot);
    return VictimIsBossLike(ctx) || ctx.bot.enemies_within(8.0f) >= 2;
}

bool ShouldAdrenalineRush(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && BurstWindow(ctx) && Ready(ctx, ADRENALINE_RUSH);
}
void DoAdrenalineRush(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ADRENALINE_RUSH);
    Classic::NoteCooldownUse(ctx.bot, ADRENALINE_RUSH, "Adrenaline Rush");
}

bool ShouldBladeFlurry(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Energy(ctx) >= 25 && BurstWindow(ctx)
        && Ready(ctx, BLADE_FLURRY) && !ctx.bot.has_aura(BLADE_FLURRY);
}
void DoBladeFlurry(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BLADE_FLURRY);
    Classic::NoteCooldownUse(ctx.bot, BLADE_FLURRY, "Blade Flurry");
}

// Slice and Dice at 1-2 combo points when it is missing; not worth it on a
// target about to die.
// Up with more than 3 s left: recast before it drops, not after (a gap of a GCD plus the next builder every cycle).
static bool SliceAndDiceUp(ApPredicateContext const& ctx)
{
    AuraEntry const* a = ctx.bot.find_aura(SLICE_AND_DICE);
    return a && a->remaining.count() > 3000;
}

bool ShouldSliceAndDice(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx) || Combo(ctx) < 1 || Energy(ctx) < 25) return false;
    if (VictimHpPct(ctx) < 25 && !VictimIsBossLike(ctx)) return false;
    return (Ready(ctx, SLICE_AND_DICE) && ctx.bot.victim_in_spell_range(SLICE_AND_DICE)) && !SliceAndDiceUp(ctx);
}
// Cast at the victim like a finishing move (the owner's PartyBotAI does the same): in the
// Classic data a finishing move is bound to the combo target, and the self cast
// (empty target) failed SPELL_FAILED_BAD_TARGETS on every attempt (MC wipe 2026-10-06).
void DoSliceAndDice(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SLICE_AND_DICE, ctx.bot.victim()); }

bool ShouldColdBlood(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Combo(ctx) >= 5 && Energy(ctx) >= 35 && Classic::BossFight(ctx.bot)
        && Ready(ctx, COLD_BLOOD) && !ctx.bot.has_aura(COLD_BLOOD);
}
void DoColdBlood(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(COLD_BLOOD);
    Classic::NoteCooldownUse(ctx.bot, COLD_BLOOD, "Cold Blood");
}

// Expose Armor at 5 points on a boss, only when nothing already strips its armor: a warrior's Sunder Armor (3+ stacks) or another
// Expose Armor with time left means the finisher is better spent on Eviscerate. One rogue per raid (lowest guid) keeps it up.
bool ShouldExposeArmor(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx) || Combo(ctx) < 5 || Energy(ctx) < 25 || !Classic::BossFight(ctx.bot) || !VictimIsBossLike(ctx)
        || !(Ready(ctx, EXPOSE_ARMOR) && ctx.bot.victim_in_spell_range(EXPOSE_ARMOR)))
        return false;
    ObjectGuid const victim = ctx.bot.victim();
    if (AuraEntry const* sunder = ctx.bot.find_aura_any_caster(SUNDER_ARMOR, victim); sunder && sunder->stacks >= 3)
        return false;
    if (AuraEntry const* expose = ctx.bot.find_aura_any_caster(EXPOSE_ARMOR, victim); expose && expose->remaining.count() > 5000)
        return false;
    if (auto const* members = ctx.group.members())
        for (auto const& m : *members)
            if (m.cls == CLASS_ROGUE && m.is_bot && m.online && m.is_alive && m.map_id == ctx.bot.map_id() && m.guid < ctx.bot.raw().guid)
                return false;
    return true;
}
void DoExposeArmor(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(EXPOSE_ARMOR, ctx.bot.victim()); }

// Finisher at 5 points; at 3+ when the target is nearly dead.
bool ShouldEviscerate(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx) || Energy(ctx) < 35 || !(Ready(ctx, EVISCERATE) && ctx.bot.victim_in_spell_range(EVISCERATE))) return false;
    int32 const cp = Combo(ctx);
    return cp >= 5 || (cp >= 3 && VictimHpPct(ctx) <= 20);
}
void DoEviscerate(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(EVISCERATE, ctx.bot.victim()); }

bool ShouldRiposte(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Energy(ctx) >= 10 && (Ready(ctx, RIPOSTE) && ctx.bot.victim_in_spell_range(RIPOSTE));
}
void DoRiposte(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(RIPOSTE, ctx.bot.victim()); }

// Subtlety builder, preferred over Sinister Strike when talented.
bool ShouldHemorrhage(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Energy(ctx) >= 35 && Combo(ctx) < 5 && (Ready(ctx, HEMORRHAGE) && ctx.bot.victim_in_spell_range(HEMORRHAGE));
}
void DoHemorrhage(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(HEMORRHAGE, ctx.bot.victim()); }

bool ShouldBackstab(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Energy(ctx) >= 60 && Combo(ctx) < 5 && (Ready(ctx, BACKSTAB) && ctx.bot.victim_in_spell_range(BACKSTAB))
        && MainHandDagger(ctx) && BehindVictim(ctx);
}
void DoBackstab(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(BACKSTAB, ctx.bot.victim()); }

bool ShouldSinisterStrike(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Energy(ctx) >= 40 && Combo(ctx) < 5 && (Ready(ctx, SINISTER_STRIKE) && ctx.bot.victim_in_spell_range(SINISTER_STRIKE));
}
void DoSinisterStrike(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SINISTER_STRIKE, ctx.bot.victim()); }

ApRule const kRules[] = {
    { ShouldEvasion,        DoEvasion,        "classic_rogue:evasion" },
    { ShouldFeint,          DoFeint,          "classic_rogue:feint" },
    { ShouldKick,           DoKick,           "classic_rogue:kick" },
    { ShouldKidneyInterrupt, DoKidneyInterrupt, "classic_rogue:kidney_interrupt" },
    { ShouldStealth,        DoStealth,        "classic_rogue:stealth" },
    { ShouldSap,            DoSap,            "classic_rogue:sap" },
    { ShouldAmbush,         DoAmbush,         "classic_rogue:ambush" },
    { ShouldCheapShot,      DoCheapShot,      "classic_rogue:cheap_shot" },
    { ShouldGarrote,        DoGarrote,        "classic_rogue:garrote" },
    { ShouldAdrenalineRush, DoAdrenalineRush, "classic_rogue:adrenaline_rush" },
    { ShouldBladeFlurry,    DoBladeFlurry,    "classic_rogue:blade_flurry" },
    { ShouldSliceAndDice,   DoSliceAndDice,   "classic_rogue:slice_and_dice" },
    { ShouldColdBlood,      DoColdBlood,      "classic_rogue:cold_blood" },
    { ShouldExposeArmor,    DoExposeArmor,    "classic_rogue:expose_armor" },
    { ShouldEviscerate,     DoEviscerate,     "classic_rogue:eviscerate" },
    { ShouldRiposte,        DoRiposte,        "classic_rogue:riposte" },
    { ShouldBackstab,       DoBackstab,       "classic_rogue:backstab" },
    { ShouldHemorrhage,     DoHemorrhage,     "classic_rogue:hemorrhage" },
    { ShouldSinisterStrike, DoSinisterStrike, "classic_rogue:sinister_strike" },
    { HasLiveTarget,        DoAutoAttack,     "classic_rogue:auto_attack" },
};

} // namespace

void RegisterApl_Classic_Rogue()
{
    ApRotation const rogue{ std::span<ApRule const>(kRules) };
    RegisterRotationOverride(CLASS_ROGUE, 0, rogue);
    RegisterRotationOverride(CLASS_ROGUE, 259, rogue);
    RegisterRotationOverride(CLASS_ROGUE, 260, rogue);
    RegisterRotationOverride(CLASS_ROGUE, 261, rogue);
}

} // namespace Playerbot::Combat
