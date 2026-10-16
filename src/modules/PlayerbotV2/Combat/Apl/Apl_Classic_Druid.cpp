// Classic 1.60 (Forever) Druid - vanilla rotations.
//
// Registered over the retail Balance / Feral / Guardian / Restoration
// rotations and the druid baseline (spec 0). 102 and 0 = Balance caster DPS,
// 103 = Feral cat DPS, 104 = Feral bear tank, 105 = Restoration healer.
// Spell ids are vanilla rank 1 and resolve to the highest known rank in the
// snapshot view and API::cast_spell.
//
// Shapeshifting rules: caster spells (Wrath, Healing Touch, ...) are never
// cast while in Cat / Bear / Dire Bear / Moonkin Form (every caster rule
// goes through CasterReady, which fails in any form). A form is cast only
// when the bot is not already in it. The rotations never leave a form in
// combat: a caster-spec bot that is stuck in an animal form (spec change
// mid-fight) simply auto-attacks until the form drops (death, or a form
// spell). Out of combat idle:classic_druid_form owns the form: it leaves it
// to cast an owed class buff and puts a feral druid back in before the pull.
//
// Skipped on purpose: Prowl / Pounce / Ravage (stealth positioning), Shred
// (needs behind-the-target positioning, Claw is used instead), Maim, Bash
// as a CC, Hurricane, Mark of the Wild / Thorns (out-of-combat group buffs
// are handled by the idle-state buff maintenance), Omen of Clarity, Cat
// bail-out to heal.
//
// Priorities follow Karrz's ike3 vanilla strategies (Karrz1/wow-1.12
// raid10-revamp, playerbot/strategy/druid/*Strategy.cpp, MANGOSBOT_ZERO).

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Apl_Baseline_Common.h"
#include "ApClassicHeal.h"
#include "ApCrowdControl.h"
#include "ApInterrupt.h"
#include "ApSchoolHint.h"
#include "ApTankThreat.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/ThreatDiscipline.h"
#include "Group/GroupSnapshot.h"
#include "Services.h"
#include "Util/ConfigReader.h"   // Playerbot.Behavior.DruidHealForm
#include "Log.h"
#include "SharedDefines.h"
#include <algorithm>

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::DoAutoAttack;
using ::Playerbot::Combat::baseline_common::HasLiveTarget;

// ---- Vanilla spell ids (rank 1) ----
// Forms (cast id == aura id)
constexpr uint32 CAT_FORM           = 768;
constexpr uint32 BEAR_FORM          = 5487;
constexpr uint32 DIRE_BEAR_FORM     = 9634;
constexpr uint32 MOONKIN_FORM       = 24858;   // Balance talent
// Balance
constexpr uint32 WRATH              = 5176;
constexpr uint32 STARFIRE           = 2912;
constexpr uint32 MOONFIRE           = 8921;
constexpr uint32 INSECT_SWARM       = 5570;
constexpr uint32 FAERIE_FIRE        = 770;
constexpr uint32 INNERVATE          = 29166;
constexpr uint32 BARKSKIN           = 22812;
// Cat
constexpr uint32 CLAW               = 1082;
constexpr uint32 RAKE               = 1822;
constexpr uint32 RIP                = 1079;
constexpr uint32 FEROCIOUS_BITE     = 22568;
constexpr uint32 TIGERS_FURY        = 5217;
constexpr uint32 FAERIE_FIRE_FERAL  = 16857;
// Bear
constexpr uint32 GROWL              = 6795;
constexpr uint32 CHALLENGING_ROAR   = 5209;
constexpr uint32 MAUL               = 6807;
constexpr uint32 SWIPE              = 779;
constexpr uint32 DEMORALIZING_ROAR  = 99;
constexpr uint32 ENRAGE             = 5229;
constexpr uint32 FRENZIED_REGEN     = 22842;
constexpr uint32 BASH               = 5211;
// Restoration
constexpr uint32 HEALING_TOUCH      = 5185;
constexpr uint32 REGROWTH           = 8936;
constexpr uint32 REJUVENATION       = 774;
constexpr uint32 SWIFTMEND          = 18562;   // Restoration talent
constexpr uint32 NATURES_SWIFTNESS  = 17116;   // Restoration talent
constexpr uint32 REBIRTH            = 20484;
constexpr uint32 TRANQUILITY        = 740;

constexpr uint8 POWER_MANA   = 0;
constexpr uint8 POWER_RAGE   = 1;
constexpr uint8 POWER_ENERGY = 3;
constexpr uint8 POWER_COMBO_POINTS_IDX = 4;

bool Ready(ApPredicateContext const& ctx, uint32 spell)
{
    return ctx.bot.knows_spell(spell) && ctx.bot.is_ready(spell);
}

bool InAnimalForm(ApPredicateContext const& ctx)
{
    return ctx.bot.has_aura(CAT_FORM) || ctx.bot.has_aura(BEAR_FORM) || ctx.bot.has_aura(DIRE_BEAR_FORM);
}

bool InAnyForm(ApPredicateContext const& ctx)
{
    // Aquatic / Travel Form are shapeshifts too: Rebirth and every caster spell
    // fail SPELL_FAILED_NOT_SHAPESHIFT in them (MC wipe 2026-10-06).
    constexpr uint32 AQUATIC_FORM = 1066, TRAVEL_FORM = 783;
    return InAnimalForm(ctx) || ctx.bot.has_aura(MOONKIN_FORM)
        || ctx.bot.has_aura(AQUATIC_FORM) || ctx.bot.has_aura(TRAVEL_FORM);
}

// The form aura the bot wears that its spec does not want (0 = none): a heal
// or a different form cast from it fails SPELL_FAILED_NOT_SHAPESHIFT, so the
// rotations cancel it first (an instant removal, the cast follows next tick).
// Playerbot.Behavior.DruidHealForm = 0 turns every form cancel off.
enum class WantedForm : uint8 { None, Cat, Bear, Moonkin };
uint32 WrongFormAura(ApPredicateContext const& ctx, WantedForm wanted)
{
    if (!Services::Config().behavior_druid_heal_form())
        return 0;
    constexpr uint32 AQUATIC_FORM = 1066, TRAVEL_FORM = 783;
    struct F { uint32 aura; WantedForm kind; };
    constexpr F kForms[] = {
        { CAT_FORM, WantedForm::Cat }, { BEAR_FORM, WantedForm::Bear }, { DIRE_BEAR_FORM, WantedForm::Bear },
        { MOONKIN_FORM, WantedForm::Moonkin }, { AQUATIC_FORM, WantedForm::None }, { TRAVEL_FORM, WantedForm::None },
    };
    for (F const& f : kForms)
        if (ctx.bot.has_aura(f.aura) && (wanted == WantedForm::None || f.kind != wanted))
            return f.aura;
    return 0;
}
void DoCancelWrongForm(ApPredicateContext const& ctx, BotIntentEmitter& e, WantedForm wanted)
{
    if (uint32 const form = WrongFormAura(ctx, wanted))
        e.cancel_aura(form);
}

// Caster spells: never while shapeshifted.
bool CasterReady(ApPredicateContext const& ctx, uint32 spell)
{
    return !InAnyForm(ctx) && Ready(ctx, spell);
}

// Balance damage spells: Moonkin Form is the Balance druid's own form (Wrath / Starfire / Moonfire / Insect Swarm / Faerie Fire are
// cast in it in 1.12), so only the animal, Aquatic and Travel forms block them. Heals and dispels still need CasterReady.
// If the 70205 data refuses them in Moonkin Form the [cast_fail] log shows NOT_SHAPESHIFT for classic_balance rules.
bool BalanceReady(ApPredicateContext const& ctx, uint32 spell)
{
    constexpr uint32 AQUATIC_FORM = 1066, TRAVEL_FORM = 783;
    return !InAnimalForm(ctx) && !ctx.bot.has_aura(AQUATIC_FORM) && !ctx.bot.has_aura(TRAVEL_FORM) && Ready(ctx, spell);
}

int32 ManaPct(ApPredicateContext const& ctx)
{
    return ctx.bot.max_power(POWER_MANA) > 0 ? ctx.bot.power_pct(POWER_MANA) : 100;
}
int32 Rage(ApPredicateContext const& ctx)   { return ctx.bot.power(POWER_RAGE); }
int32 Energy(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_ENERGY); }
int32 ComboPoints(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_COMBO_POINTS_IDX); }

int32 VictimHpPct(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v || v->max_hp <= 0) return 100;
    return int32(int64(v->hp) * 100 / v->max_hp);
}

bool VictimIsBossLike(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    return v && (Classic::IsBossUnit(*v) || v->max_hp >= 100'000);
}

struct HealTarget
{
    ObjectGuid guid;
    int32      hp_pct = 100;
};

// Heal target picked by the owner's scoring (ClassicHeal::PickPatient: tank
// first, LOS, incoming-damage projection, healers spread); hp_pct is the
// projected health. The bot itself at 100% when nobody needs a heal.
HealTarget LowestFriendOrSelf(ApPredicateContext const& ctx)
{
    ClassicHeal::Patient const p = ClassicHeal::PickPatient(ctx);
    return { p.guid, p.hp_pct };
}

bool TargetHasMyAura(ApPredicateContext const& ctx, uint32 spell, ObjectGuid target)
{
    return ctx.bot.has_aura(spell, target == ctx.bot.raw().guid ? ObjectGuid::Empty : target);
}

int WoundedCount(ApPredicateContext const& ctx, int32 below_pct)
{
    auto const* members = ctx.group.members();
    if (!members)
        return ctx.bot.hp_pct() <= below_pct ? 1 : 0;
    int n = 0;
    for (auto const& m : *members)
    {
        if (!m.online || !m.is_alive || m.max_hp <= 0 || m.map_id != ctx.bot.map_id()) continue;
        if (int64(m.hp) * 100 / m.max_hp <= below_pct) ++n;
    }
    return n;
}

// ======================= Balance =======================

// A Balance druid in Cat / Bear / Travel / Aquatic Form leaves it before Moonkin Form.
bool ShouldLeaveWrongFormBalance(ApPredicateContext const& ctx) { return WrongFormAura(ctx, WantedForm::Moonkin) != 0; }
void DoLeaveWrongFormBalance(ApPredicateContext const& ctx, BotIntentEmitter& e) { DoCancelWrongForm(ctx, e, WantedForm::Moonkin); }

bool ShouldMoonkinForm(ApPredicateContext const& ctx)
{
    return Ready(ctx, MOONKIN_FORM) && !ctx.bot.has_aura(MOONKIN_FORM);
}
void DoMoonkinForm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(MOONKIN_FORM); }

bool ShouldBalanceSelfHeal(ApPredicateContext const& ctx)
{
    // Out of form only (Moonkin included); Regrowth first, then Healing Touch.
    if (ctx.bot.hp_pct() > 35) return false;
    return CasterReady(ctx, REGROWTH) || CasterReady(ctx, HEALING_TOUCH);
}
void DoBalanceSelfHeal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(CasterReady(ctx, REGROWTH) ? REGROWTH : HEALING_TOUCH, ctx.bot.raw().guid);
}

// Innervate: the druid itself when dry, otherwise a healer running out
// (InnervateTarget, shared with Restoration). Innervate is a Balance spell,
// so Moonkin Form keeps it.
ObjectGuid InnervateTarget(ApPredicateContext const& ctx);
void DoHealerInnervate(ApPredicateContext const& ctx, BotIntentEmitter& e);
bool ShouldBalanceInnervate(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat() || InAnimalForm(ctx) || !Ready(ctx, INNERVATE)) return false;
    ObjectGuid const t = InnervateTarget(ctx);
    return !t.IsEmpty() && !ctx.bot.has_aura(INNERVATE, t == ctx.bot.raw().guid ? ObjectGuid::Empty : t);
}

bool ShouldBarkskin(ApPredicateContext const& ctx)
{
    // Castable in any form, including Moonkin.
    return ctx.bot.in_combat() && ctx.bot.attackers_count() >= 1 && ctx.bot.hp_pct() <= 50
        && Ready(ctx, BARKSKIN) && !ctx.bot.has_aura(BARKSKIN);
}
void DoBarkskin(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(BARKSKIN); }

// Faerie Fire (either form of the spell) from any druid is on the victim: the armor debuff does not stack across casters.
static bool FaerieFireUp(ApPredicateContext const& ctx)
{
    for (uint32 const ff : { FAERIE_FIRE, FAERIE_FIRE_FERAL })
        if (AuraEntry const* a = ctx.bot.find_aura_any_caster(ff, ctx.bot.victim()))
            if (a->remaining.count() > 4000)
                return true;
    return false;
}

bool ShouldFaerieFireBalance(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && VictimIsBossLike(ctx) && ManaPct(ctx) >= 15
        && (BalanceReady(ctx, FAERIE_FIRE) && ctx.bot.victim_in_spell_range(FAERIE_FIRE)) && !FaerieFireUp(ctx);
}
void DoFaerieFire(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(FAERIE_FIRE, ctx.bot.victim()); }

bool ShouldMoonfire(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 10 && VictimHpPct(ctx) >= 25
        && (BalanceReady(ctx, MOONFIRE) && ctx.bot.victim_in_spell_range(MOONFIRE)) && !ctx.bot.has_aura(MOONFIRE, ctx.bot.victim());
}
void DoMoonfire(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(MOONFIRE, ctx.bot.victim()); }

bool ShouldInsectSwarm(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 10 && VictimHpPct(ctx) >= 25
        && (BalanceReady(ctx, INSECT_SWARM) && ctx.bot.victim_in_spell_range(INSECT_SWARM)) && !ctx.bot.has_aura(INSECT_SWARM, ctx.bot.victim());
}
void DoInsectSwarm(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(INSECT_SWARM, ctx.bot.victim()); }

// Starfire is the main nuke while mana is comfortable; Wrath is the cheap
// filler (and the only nuke before level 20).
bool ShouldStarfire(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 20 && (BalanceReady(ctx, STARFIRE) && ctx.bot.victim_in_spell_range(STARFIRE));
}
void DoStarfire(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(STARFIRE, ctx.bot.victim()); }

bool ShouldWrath(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 5 && (BalanceReady(ctx, WRATH) && ctx.bot.victim_in_spell_range(WRATH));
}
void DoWrath(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(WRATH, ctx.bot.victim()); }

// Raid school rule (ApSchoolHint.h): the victim's preferred school first among
// Balance's dots and nukes - nature: Insect Swarm / Wrath; arcane: Moonfire /
// Starfire. Same gates as the rules below.
SchoolHint::SchoolChoice const kBalanceSchoolChoices[] = {
    { MOONFIRE,     ShouldMoonfire },
    { INSECT_SWARM, ShouldInsectSwarm },
    { STARFIRE,     ShouldStarfire },
    { WRATH,        ShouldWrath },
};
bool ShouldBalanceSchoolNuke(ApPredicateContext const& ctx) { return SchoolHint::ShouldCast(ctx, kBalanceSchoolChoices); }
void DoBalanceSchoolNuke(ApPredicateContext const& ctx, BotIntentEmitter& e) { SchoolHint::DoCast(ctx, e, kBalanceSchoolChoices); }

// Hibernate: Beasts / Dragonkin. In a group an add of a 3+ mob pull (never the kill target,
// the tank's target or a boss); solo an extra attacker. See PickGroupCC.
constexpr uint32 HIBERNATE = 2637;
bool ShouldCrowdControl(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ctx.bot.in_combat() && !ctx.bot.is_moving() && ManaPct(ctx) >= 15
        && Ready(ctx, HIBERNATE) && !PickGroupCC(ctx, HIBERNATE).IsEmpty();
}
void DoCrowdControl(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HIBERNATE, PickGroupCC(ctx, HIBERNATE));
}

// Remove Curse / Abolish Poison on the member the raid dispel plan gives
// this druid (Classic::PickDispel: one bot per debuffed member). Restoration
// and Balance (spec 0 too) run it; never in a form - the plan leaves a druid
// it sees in a form (Moonkin included) out, so the job goes elsewhere.
bool ShouldDispel(ApPredicateContext const& ctx)
{
    return !InAnyForm(ctx) && ClassicHeal::ShouldAssignedDispel(ctx);
}
void DoDispel(ApPredicateContext const& ctx, BotIntentEmitter& e) { ClassicHeal::DoAssignedDispel(ctx, e); }

ApRule const kBalanceRules[] = {
    { ShouldBalanceSelfHeal,   DoBalanceSelfHeal, "classic_balance:self_heal" },
    { ShouldBarkskin,          DoBarkskin,        "classic_balance:barkskin" },
    { ShouldCrowdControl,      DoCrowdControl,    "classic_balance:hibernate" },
    { ShouldBalanceInnervate,  DoHealerInnervate, "classic_balance:innervate" },
    { ShouldDispel,            DoDispel,          "classic_balance:dispel" },
    { ShouldLeaveWrongFormBalance, DoLeaveWrongFormBalance, "classic_balance:leave_wrong_form" },
    { ShouldMoonkinForm,       DoMoonkinForm,     "classic_balance:moonkin_form" },
    { ShouldFaerieFireBalance, DoFaerieFire,      "classic_balance:faerie_fire" },
    { ShouldBalanceSchoolNuke, DoBalanceSchoolNuke, "classic_balance:school_nuke" },
    { ShouldMoonfire,          DoMoonfire,        "classic_balance:moonfire" },
    { ShouldInsectSwarm,       DoInsectSwarm,     "classic_balance:insect_swarm" },
    { ShouldStarfire,          DoStarfire,        "classic_balance:starfire" },
    { ShouldWrath,             DoWrath,           "classic_balance:wrath" },
    { HasLiveTarget,           DoAutoAttack,      "classic_balance:auto_attack" },
};

// ======================= Feral cat =======================

// Cat Form cast from Bear / Moonkin / Travel / Aquatic Form fails NOT_SHAPESHIFT
// (capture 2026-10-07): leave the other form first, Cat Form follows next tick.
bool ShouldLeaveWrongFormCat(ApPredicateContext const& ctx) { return WrongFormAura(ctx, WantedForm::Cat) != 0; }
void DoLeaveWrongFormCat(ApPredicateContext const& ctx, BotIntentEmitter& e) { DoCancelWrongForm(ctx, e, WantedForm::Cat); }

bool ShouldCatForm(ApPredicateContext const& ctx)
{
    return !ctx.bot.has_aura(CAT_FORM) && !WrongFormAura(ctx, WantedForm::Cat) && Ready(ctx, CAT_FORM);
}
void DoCatForm(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CAT_FORM); }

bool InCat(ApPredicateContext const& ctx) { return ctx.bot.has_aura(CAT_FORM); }

bool ShouldTigersFury(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InCat(ctx) && Energy(ctx) >= 30 && Ready(ctx, TIGERS_FURY)
        && !ctx.bot.has_aura(TIGERS_FURY);
}
void DoTigersFury(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TIGERS_FURY); }

bool ShouldFaerieFireCat(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InCat(ctx) && VictimIsBossLike(ctx) && Ready(ctx, FAERIE_FIRE_FERAL)
        && !FaerieFireUp(ctx);
}
void DoFaerieFireFeral(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FAERIE_FIRE_FERAL, ctx.bot.victim());
}

// 5 combo points: Rip while the target will live long enough, otherwise Bite.
bool ShouldRip(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InCat(ctx) && ComboPoints(ctx) >= 5 && Energy(ctx) >= 30
        && VictimHpPct(ctx) >= 40 && (Ready(ctx, RIP) && ctx.bot.victim_in_spell_range(RIP)) && !ctx.bot.has_aura(RIP, ctx.bot.victim());
}
void DoRip(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(RIP, ctx.bot.victim()); }

bool ShouldFerociousBite(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx) || !InCat(ctx) || Energy(ctx) < 35 || !(Ready(ctx, FEROCIOUS_BITE) && ctx.bot.victim_in_spell_range(FEROCIOUS_BITE))) return false;
    int32 const cp = ComboPoints(ctx);
    return cp >= 5 || (cp >= 3 && VictimHpPct(ctx) <= 20);
}
void DoFerociousBite(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(FEROCIOUS_BITE, ctx.bot.victim()); }

bool ShouldRake(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InCat(ctx) && Energy(ctx) >= 35 && VictimHpPct(ctx) >= 30
        && (Ready(ctx, RAKE) && ctx.bot.victim_in_spell_range(RAKE)) && !ctx.bot.has_aura(RAKE, ctx.bot.victim());
}
void DoRake(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(RAKE, ctx.bot.victim()); }

bool ShouldClaw(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InCat(ctx) && Energy(ctx) >= 45 && (Ready(ctx, CLAW) && ctx.bot.victim_in_spell_range(CLAW));
}
void DoClaw(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CLAW, ctx.bot.victim()); }

ApRule const kCatRules[] = {
    { ShouldBarkskin,         DoBarkskin,         "classic_cat:barkskin" },
    { ShouldLeaveWrongFormCat, DoLeaveWrongFormCat, "classic_cat:leave_wrong_form" },
    { ShouldCatForm,          DoCatForm,          "classic_cat:cat_form" },
    { ShouldTigersFury,       DoTigersFury,       "classic_cat:tigers_fury" },
    { ShouldFaerieFireCat,    DoFaerieFireFeral,  "classic_cat:faerie_fire_feral" },
    { ShouldRip,              DoRip,              "classic_cat:rip" },
    { ShouldFerociousBite,    DoFerociousBite,    "classic_cat:ferocious_bite" },
    { ShouldRake,             DoRake,             "classic_cat:rake" },
    { ShouldClaw,             DoClaw,             "classic_cat:claw" },
    { HasLiveTarget,          DoAutoAttack,       "classic_cat:auto_attack" },
};

// ======================= Feral bear tank =======================

// Dire Bear Form (level 40) replaces Bear Form once trained.
uint32 BearSpell(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(DIRE_BEAR_FORM) ? DIRE_BEAR_FORM : BEAR_FORM;
}

bool InBear(ApPredicateContext const& ctx)
{
    return ctx.bot.has_aura(BEAR_FORM) || ctx.bot.has_aura(DIRE_BEAR_FORM);
}

bool ShouldLeaveWrongFormBear(ApPredicateContext const& ctx) { return WrongFormAura(ctx, WantedForm::Bear) != 0; }
void DoLeaveWrongFormBear(ApPredicateContext const& ctx, BotIntentEmitter& e) { DoCancelWrongForm(ctx, e, WantedForm::Bear); }

bool ShouldBearForm(ApPredicateContext const& ctx)
{
    return !ctx.bot.has_aura(BearSpell(ctx)) && !WrongFormAura(ctx, WantedForm::Bear) && Ready(ctx, BearSpell(ctx));
}
void DoBearForm(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(BearSpell(ctx)); }

bool ShouldFrenziedRegen(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && InBear(ctx) && ctx.bot.hp_pct() <= 40 && Rage(ctx) >= 10
        && Ready(ctx, FRENZIED_REGEN) && !ctx.bot.has_aura(FRENZIED_REGEN);
}
void DoFrenziedRegen(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FRENZIED_REGEN); }

// ---- Threat: loose attackers (the owner's LooseAttackersTrigger) ----
// A "loose attacker" is a mob hitting a NON-TANK group member (snapshot
// victim_kind == Member). Growl's reach is read from the spell data
// (TankThreat::TauntReach, 10 yd when the data has none); the pick-up rule
// walks the bear into range of the rest.
constexpr float kGrowlRangeFallback = 10.0f;
float GrowlRange(ApPredicateContext const& ctx)
{
    return TankThreat::TauntReach(ctx.bot, GROWL, kGrowlRangeFallback);
}

// The bear's current mob is secured when it is on us / another tank (or we
// have none) - only then may it leave that mob for a loose attacker.
bool BearVictimSecured(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return true;
    ThreatVictim const k = ThreatVictim(v->victim_kind);
    return v->hp <= 0 || k == ThreatVictim::Me || k == ThreatVictim::Tank;
}

// Challenging Roar when 3+ loose attackers are within 10 yd (the owner) - only
// those no other tank was given, and never with a crowd-controlled mob in
// reach (TankThreat::AreaTauntWorth).
bool ShouldChallengingRoar(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && InBear(ctx) && Rage(ctx) >= 15 && Ready(ctx, CHALLENGING_ROAR)
        && TankThreat::AreaTauntWorth(ctx.bot, 10.0f, "Challenging Roar");
}
void DoChallengingRoar(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (e.cast(CHALLENGING_ROAR))
        if (ctx.bot.is_in_instance())
            TC_LOG_INFO("playerbot.v2", "[raid_tank] {} Challenging Roar, {} loose within 10y", ctx.bot.name(),
                TankThreat::UnclaimedLooseWithin(ctx.bot, 10.0f));
}

bool ShouldGrowl(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && InBear(ctx) && Ready(ctx, GROWL) && ctx.bot.taunt_candidate(GrowlRange(ctx)) != nullptr;
}
void DoGrowl(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    // Growl forces the mob onto us; switch our Mauls / swings onto it too.
    if (auto const* t = ctx.bot.taunt_candidate(GrowlRange(ctx)))
        if (e.cast(GROWL, t->guid))
        {
            if (ctx.bot.is_in_raid())
                TC_LOG_INFO("playerbot.v2", "[taunt] {} Growl -> entry {} ({}) {}", ctx.bot.name(), t->entry,
                    t->guid.GetCounter(), t == ctx.bot.assigned_tank_mob() ? "assigned" : "loose");
            if (t->guid != ctx.bot.victim())
                e.start_attack(t->guid);
        }
}

// Growl on cooldown / out of range: switch victim to the nearest loose
// attacker and build threat on it (Maul / Swipe below).
bool ShouldBearPickUpLoose(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat() || !InBear(ctx)) return false;
    // An assigned raid mob is always ours to hold; a loose mob only once
    // the current one is secured.
    if (!ctx.bot.assigned_tank_mob() && !BearVictimSecured(ctx)) return false;
    NearbyUnit const* t = ctx.bot.pickup_candidate();
    return t && t->guid != ctx.bot.victim();
}
void DoBearPickUpLoose(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.pickup_candidate())
        e.start_attack(t->guid);
}

bool ShouldEnrage(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && InBear(ctx) && Rage(ctx) < 15 && ctx.bot.hp_pct() > 50 && Ready(ctx, ENRAGE);
}
void DoEnrage(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ENRAGE); }

// Interrupt rotation (ApInterrupt.h): the plan's pick in melee reach - the
// bear's own victim or another casting mob beside it - never a stun-immune
// mob (Bash interrupts through its stun). Solo: the own victim while it casts.
bool ShouldBash(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InBear(ctx) && Rage(ctx) >= 10 && Interrupt::Should(ctx, BASH);
}
void DoBash(ApPredicateContext const& ctx, BotIntentEmitter& e) { Interrupt::Cast(ctx, e, BASH); }

// Demoralizing Roar over the pack in reach (10 yd): 2+ engaged mobs carry no
// Demoralizing Roar / Shout from anyone or under 3 s of it, or a boss in
// reach does (TankThreat::DemoRefreshWorth, NearbyUnit::ap_debuff_ms). Only
// with rage to spare: the roar's 10 and 10 more for the next Maul. Never with
// a crowd-controlled mob in reach (MeleeAoeSafe in raids, AoeSafe everywhere).
constexpr int32 kDemoRoarRage = 20;
bool ShouldDemoRoar(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InBear(ctx) && ctx.bot.in_combat() && Rage(ctx) >= kDemoRoarRage
        && Ready(ctx, DEMORALIZING_ROAR)
        && TankThreat::DemoRefreshWorth(ctx.bot, 10.0f)
        && MeleeAoeSafe(ctx.bot, ctx.group.is_raid() || ctx.bot.is_in_raid(), 10.0f, /*tank*/ true, "Demoralizing Roar")
        // No damage (a sheep survives it), but it aggroes every mob it lands on.
        && AoeSafeAroundSelf(ctx.bot, 10.0f, /*damaging*/ false, /*tank*/ true, "Demoralizing Roar");
}
void DoDemoRoar(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DEMORALIZING_ROAR); }

// Swipe: the bear's pack threat (up to three mobs in front). Never into a
// CC'd / unpulled mob (AoeSafe) or one another tank holds close to its
// threat (TankThreat::AoeRipsOtherTank: an off-tank does not steal the main
// tank's mobs).
bool ShouldSwipe(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InBear(ctx) && Rage(ctx) >= 20 && ctx.bot.enemies_within(6.0f) >= 2
        && Ready(ctx, SWIPE)
        && AoeSafeAroundSelf(ctx.bot, 8.0f, /*damaging*/ true, /*tank*/ true, "Swipe", SWIPE)
        && !TankThreat::AoeRipsOtherTank(ctx.bot, 8.0f, "Swipe");
}
void DoSwipe(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SWIPE); }

// Faerie Fire (Feral) - free, 30 yd, threat on its own - on the pack mob that
// needs threat most once the victim has its lead (TankThreat::SpreadTarget:
// a loose mob first). Maul only hits the victim, so this is the bear's
// tab-target ability.
NearbyUnit const* BearSpreadTarget(ApPredicateContext const& ctx)
{
    return TankThreat::SpreadTarget(ctx.bot, 20.0f);
}
bool ShouldBearSpreadFaerieFire(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InBear(ctx) && Ready(ctx, FAERIE_FIRE_FERAL) && BearSpreadTarget(ctx) != nullptr;
}
void DoBearSpreadFaerieFire(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (NearbyUnit const* t = BearSpreadTarget(ctx))
        if (e.cast(FAERIE_FIRE_FERAL, t->guid))
            TankThreat::LogSpread(ctx.bot, "Faerie Fire (Feral)", *t);
}

bool ShouldFaerieFireBear(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InBear(ctx) && Ready(ctx, FAERIE_FIRE_FERAL)
        && !ctx.bot.has_aura(FAERIE_FIRE_FERAL, ctx.bot.victim());
}

bool ShouldMaul(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && InBear(ctx) && Rage(ctx) >= 15 && (Ready(ctx, MAUL) && ctx.bot.victim_in_spell_range(MAUL));
}
void DoMaul(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(MAUL, ctx.bot.victim()); }

ApRule const kBearRules[] = {
    { ShouldFrenziedRegen,    DoFrenziedRegen,    "classic_bear:frenzied_regeneration" },
    { ShouldBarkskin,         DoBarkskin,         "classic_bear:barkskin" },
    { ShouldLeaveWrongFormBear, DoLeaveWrongFormBear, "classic_bear:leave_wrong_form" },
    { ShouldBearForm,         DoBearForm,         "classic_bear:bear_form" },
    { ShouldChallengingRoar,  DoChallengingRoar,  "classic_bear:challenging_roar" },
    { ShouldGrowl,            DoGrowl,            "classic_bear:growl" },
    { ShouldBearPickUpLoose,  DoBearPickUpLoose,  "classic_bear:pick_up_loose" },
    { ShouldEnrage,           DoEnrage,           "classic_bear:enrage" },
    { ShouldBash,             DoBash,             "classic_bear:bash" },
    { ShouldDemoRoar,         DoDemoRoar,         "classic_bear:demoralizing_roar" },
    { ShouldSwipe,            DoSwipe,            "classic_bear:swipe" },
    { ShouldBearSpreadFaerieFire, DoBearSpreadFaerieFire, "classic_bear:spread_faerie_fire" },
    { ShouldFaerieFireBear,   DoFaerieFireFeral,  "classic_bear:faerie_fire_feral" },
    { ShouldMaul,             DoMaul,             "classic_bear:maul" },
    { HasLiveTarget,          DoAutoAttack,       "classic_bear:auto_attack" },
};

// ======================= Restoration =======================

// Before Regrowth (level 12) the only emergency heal is Healing Touch.
uint32 FastHeal(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(REGROWTH) ? REGROWTH : HEALING_TOUCH;
}

bool ShouldNaturesSwiftnessHeal(ApPredicateContext const& ctx)
{
    // Swiftness is up: instant Healing Touch on whoever is lowest.
    return ctx.bot.has_aura(NATURES_SWIFTNESS) && CasterReady(ctx, HEALING_TOUCH)
        && LowestFriendOrSelf(ctx).hp_pct <= 60;
}
// The instant (Nature's Swiftness) Healing Touch is the emergency save: top rank.
void DoSwiftHealingTouch(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HEALING_TOUCH, LowestFriendOrSelf(ctx).guid);
}
void DoHealingTouch(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    CastHeal(ctx, e, HEALING_TOUCH, LowestFriendOrSelf(ctx).guid);
}

bool ShouldNaturesSwiftness(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && LowestFriendOrSelf(ctx).hp_pct <= 30 && CasterReady(ctx, NATURES_SWIFTNESS)
        && !ctx.bot.has_aura(NATURES_SWIFTNESS);
}
void DoNaturesSwiftness(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(NATURES_SWIFTNESS); }

bool ShouldSwiftmend(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat() || !CasterReady(ctx, SWIFTMEND)) return false;
    HealTarget const t = LowestFriendOrSelf(ctx);
    return t.hp_pct <= 50 && (TargetHasMyAura(ctx, REJUVENATION, t.guid) || TargetHasMyAura(ctx, REGROWTH, t.guid));
}
void DoSwiftmend(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SWIFTMEND, LowestFriendOrSelf(ctx).guid);
}

bool ShouldRebirth(ApPredicateContext const& ctx)
{
    // The core refuses a cast the bar cannot pay with NO_POWER (MC Rag 2026-10-12: Rebirth refused NO_POWER on every attempt, the druid
    // at 1.4-1.8k of 8.6k mana against a 1057 rank-2 cost): wait for the mana instead of burning the claim and the cooldown tick.
    uint32 const known = ctx.bot.resolve_rank(REBIRTH);
    return ctx.bot.in_combat() && CasterReady(ctx, REBIRTH) && ctx.bot.power(0) >= ctx.bot.mana_cost(known ? known : REBIRTH)
        && !ClassicHeal::RebirthTarget(ctx.bot, ctx.group).IsEmpty();
}
void DoRebirth(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    // Healers, then tanks, then the rest, in range and line of sight; a corpse
    // another druid is already casting Rebirth on is left to it, and several
    // druids spread over the corpses (ClassicHeal::RezTarget).
    if (ObjectGuid const t = ClassicHeal::RebirthTarget(ctx.bot, ctx.group); !t.IsEmpty())
        e.cast(REBIRTH, t);
}

bool ShouldFastHeal(ApPredicateContext const& ctx)
{
    return CasterReady(ctx, FastHeal(ctx)) && LowestFriendOrSelf(ctx).hp_pct <= 45;
}
// In a raid Regrowth (poor heal per mana) is kept for an emergency or a tank
// under half health; everyone else gets a downranked Healing Touch
// (ClassicHeal::RaidChoose).
void DoFastHeal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ClassicHeal::Patient const p = ClassicHeal::PickPatient(ctx);
    // Raid: Regrowth (880 mana for ~1.2k and a HoT, 31 % of a druid's mana in n31 mc_geddon) only for someone about to die; a tank under
    // half health gets Healing Touch, which is twice as efficient per mana and lands within the same window with a downranked cast.
    uint32 fast = FastHeal(ctx);
    if (ClassicHeal::RaidHealing(ctx.bot) && p.actual_pct >= ClassicHeal::kEmergencyPct && p.hp_pct >= ClassicHeal::kEmergencyPct)
        fast = HEALING_TOUCH;
    ClassicHeal::EmitHeal(ctx.bot, ctx.group, e, ClassicHeal::RaidChoose(ctx.bot, p, fast, HEALING_TOUCH), p);
}

bool ShouldHealingTouch(ApPredicateContext const& ctx)
{
    return CasterReady(ctx, HEALING_TOUCH) && LowestFriendOrSelf(ctx).hp_pct <= 65;
}

bool ShouldRejuvenation(ApPredicateContext const& ctx)
{
    if (!CasterReady(ctx, REJUVENATION)) return false;
    HealTarget const t = LowestFriendOrSelf(ctx);
    return t.hp_pct <= 85 && !TargetHasMyAura(ctx, REJUVENATION, t.guid) && ClassicHeal::HotAllowed(ctx.bot, ctx.group, t.guid)
        && ClassicHeal::HotWorthIt(ctx.bot, ctx.group, t.guid)
        && !ClassicHeal::HotClaimedByOther(ctx.bot, ctx.group, t.guid, REJUVENATION);
}
void DoRejuvenation(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ObjectGuid const t = LowestFriendOrSelf(ctx).guid;
    if (e.cast(REJUVENATION, t))
        ClassicHeal::NoteHotCast(ctx.bot, t, REJUVENATION);
}

// Innervate (the owner's fixed trigger: mana below AiPlayerbot.LowMana): the
// druid itself first, otherwise another healer within 30 yd who is
// running dry and isn't under any druid's Innervate yet (tracked buff), so
// the raid's druids don't spend two Innervates on one priest.
// In a coordinated dungeon / raid group (PveOrder active) the plan owns the
// Innervates on healers (PveGroupCoordinator::PlanInnervates: boss fight,
// lowest-mana healer under 30% first, one druid per healer): this druid casts
// only its assigned one, or on itself when dry.
ObjectGuid InnervateTarget(ApPredicateContext const& ctx)
{
    auto const& po = ctx.bot.raw().pve_order;
    if (po.active && !po.innervate_target.IsEmpty())
        return po.innervate_target;
    if (ManaPct(ctx) <= 20)
        return ctx.bot.raw().guid;
    if (po.active)
        return ObjectGuid::Empty;
    auto const* members = ctx.group.members();
    if (!members) return ObjectGuid::Empty;
    float bx, by, bz; ctx.bot.position(bx, by, bz);
    for (auto const& m : *members)
    {
        if (m.guid == ctx.bot.raw().guid || m.role != Role::Healer) continue;
        if (!m.online || !m.is_alive || m.max_mana <= 0 || m.map_id != ctx.bot.map_id()) continue;
        if (ClassicHeal::Dist2(m.x, m.y, m.z, bx, by, bz) > 30.0f * 30.0f) continue;
        if (int64(m.mana) * 100 / m.max_mana < 25 && !Classic::MemberHasBuff(ctx.group, m.guid, INNERVATE))
            return m.guid;
    }
    return ObjectGuid::Empty;
}
bool ShouldHealerInnervate(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat() || !CasterReady(ctx, INNERVATE)) return false;
    ObjectGuid const t = InnervateTarget(ctx);
    return !t.IsEmpty() && !ctx.bot.has_aura(INNERVATE, t == ctx.bot.raw().guid ? ObjectGuid::Empty : t);
}
void DoHealerInnervate(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(INNERVATE, InnervateTarget(ctx));
    Classic::NoteCooldownUse(ctx.bot, INNERVATE, "Innervate");
}

// Tranquility: 3 injured party members around the druid (the owner's "medium
// aoe heal"); a 10 s channel on a 5 min cooldown, so only with the mana for it.
bool ShouldTranquility(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ManaPct(ctx) >= 30 && CasterReady(ctx, TRANQUILITY)
        && ClassicHeal::InjuredNearMe(ctx.bot, ctx.group, 30.0f, ClassicHeal::kMediumPct) >= 3;
}
void DoTranquility(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TRANQUILITY); }

// Damage only while everyone is healthy and mana is comfortable; in a raid
// only after a quiet spell at 90%+ mana (ClassicHeal::RaidHealerMayDamage:
// the five-second rule).
bool HealerMayDps(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ctx.bot.in_combat() && WoundedCount(ctx, 90) == 0 && LowestFriendOrSelf(ctx).hp_pct > 90
        && ClassicHeal::RaidHealerMayDamage(ctx.bot, ctx.group);
}

bool ShouldHealerMoonfire(ApPredicateContext const& ctx)
{
    return HealerMayDps(ctx) && ManaPct(ctx) >= 50 && (CasterReady(ctx, MOONFIRE) && ctx.bot.victim_in_spell_range(MOONFIRE))
        && !ctx.bot.has_aura(MOONFIRE, ctx.bot.victim());
}

bool ShouldHealerWrath(ApPredicateContext const& ctx)
{
    return HealerMayDps(ctx) && ManaPct(ctx) >= 70 && (CasterReady(ctx, WRATH) && ctx.bot.victim_in_spell_range(WRATH));
}

// Raid school rule: Restoration's two damage picks, preferred school first
// (Wrath on nature, Moonfire on arcane), under the same may-dps gates.
SchoolHint::SchoolChoice const kHealerSchoolChoices[] = {
    { MOONFIRE, ShouldHealerMoonfire },
    { WRATH,    ShouldHealerWrath },
};
bool ShouldHealerSchoolNuke(ApPredicateContext const& ctx) { return SchoolHint::ShouldCast(ctx, kHealerSchoolChoices); }
void DoHealerSchoolNuke(ApPredicateContext const& ctx, BotIntentEmitter& e) { SchoolHint::DoCast(ctx, e, kHealerSchoolChoices); }

// A Restoration druid never keeps a form: every heal fails NOT_SHAPESHIFT in
// one, and CasterReady holds all caster rules back, so a healer that woke up in
// Cat / Bear / Moonkin / Travel / Aquatic Form would stand there doing nothing.
// In combat, or when anyone is hurt, it cancels the form (instant) and the
// heal goes out on the next tick. Nothing re-enters a form for a healer:
// DruidFormToEnter only shifts Feral specs (103 / 104).
bool ShouldHealerLeaveForm(ApPredicateContext const& ctx)
{
    return WrongFormAura(ctx, WantedForm::None) != 0 && (ctx.bot.in_combat() || LowestFriendOrSelf(ctx).hp_pct <= 90);
}
void DoHealerLeaveForm(ApPredicateContext const& ctx, BotIntentEmitter& e) { DoCancelWrongForm(ctx, e, WantedForm::None); }

ApRule const kHealerRules[] = {
    { ShouldHealerLeaveForm,      DoHealerLeaveForm,   "classic_resto:leave_form" },
    { ShouldNaturesSwiftnessHeal, DoSwiftHealingTouch, "classic_resto:natures_swiftness_touch" },
    { ShouldNaturesSwiftness,     DoNaturesSwiftness,  "classic_resto:natures_swiftness" },
    { ShouldSwiftmend,            DoSwiftmend,         "classic_resto:swiftmend" },
    { ShouldFastHeal,             DoFastHeal,          "classic_resto:regrowth" },
    { ShouldHealerInnervate,      DoHealerInnervate,   "classic_resto:innervate" },
    { ShouldRebirth,              DoRebirth,           "classic_resto:rebirth" },
    { ShouldTranquility,          DoTranquility,       "classic_resto:tranquility" },
    { ShouldHealingTouch,         DoHealingTouch,      "classic_resto:healing_touch" },
    { ShouldDispel,               DoDispel,            "classic_resto:dispel" },
    { ShouldRejuvenation,         DoRejuvenation,      "classic_resto:rejuvenation" },
    { ShouldHealerSchoolNuke,     DoHealerSchoolNuke,  "classic_resto:school_nuke" },
    { ShouldHealerMoonfire,       DoMoonfire,          "classic_resto:moonfire" },
    { ShouldHealerWrath,          DoWrath,             "classic_resto:wrath" },
    { HasLiveTarget,              DoAutoAttack,        "classic_resto:auto_attack" },
};

} // namespace

void RegisterApl_Classic_Druid()
{
    ApRotation const balance{ std::span<ApRule const>(kBalanceRules) };
    ApRotation const cat{ std::span<ApRule const>(kCatRules) };
    ApRotation const bear{ std::span<ApRule const>(kBearRules) };
    ApRotation const healer{ std::span<ApRule const>(kHealerRules) };
    RegisterRotationOverride(CLASS_DRUID, 0, balance);
    RegisterRotationOverride(CLASS_DRUID, 102, balance);
    RegisterRotationOverride(CLASS_DRUID, 103, cat);
    RegisterRotationOverride(CLASS_DRUID, 104, bear);
    RegisterRotationOverride(CLASS_DRUID, 105, healer);
}

} // namespace Playerbot::Combat
