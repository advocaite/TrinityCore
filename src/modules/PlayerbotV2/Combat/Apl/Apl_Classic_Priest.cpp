// Classic 1.60 (Forever) Priest - vanilla rotations.
//
// Registered over the retail Discipline / Holy / Shadow rotations and the
// priest baseline (spec 0). Holy and Discipline (256/257, also the default
// for an untalented priest) run the healer list; Shadow (258) the caster
// DPS list. Spell ids are vanilla rank 1 and resolve to the highest known
// rank in the snapshot view and API::cast_spell.
//
// Priorities follow Karrz's ike3 vanilla strategies (Karrz1/wow-1.12
// raid10-revamp, playerbot/strategy/priest/*Strategy.cpp, MANGOSBOT_ZERO):
// shield + Flash Heal on critical, Greater Heal on low, Renew on topping,
// Prayer of Healing on group damage, dispels (the raid dispel plan), Power
// Infusion on a caster in boss fights, then wand/Smite filler only
// while nobody needs healing.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Apl_Baseline_Common.h"
#include "ApClassicHeal.h"
#include "PriestRenewRules.h"
#include "ApCrowdControl.h"
#include "ApSchoolHint.h"
#include "ApInterrupt.h"
#include "ApThreatDump.h"
#include "Bot/ThreatDiscipline.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"
#include <algorithm>

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::DoAutoAttack;
using ::Playerbot::Combat::baseline_common::HasLiveTarget;

// ---- Vanilla spell ids (rank 1) ----
constexpr uint32 LESSER_HEAL        = 2050;
constexpr uint32 HEAL               = 2054;
constexpr uint32 GREATER_HEAL       = 2060;
constexpr uint32 FLASH_HEAL         = 2061;
constexpr uint32 RENEW              = 139;
constexpr uint32 POWER_WORD_SHIELD  = 17;
constexpr uint32 WEAKENED_SOUL      = 6788;    // debuff from Power Word: Shield
constexpr uint32 PRAYER_OF_HEALING  = 596;
constexpr uint32 FADE               = 586;
constexpr uint32 PSYCHIC_SCREAM     = 8122;
constexpr uint32 DESPERATE_PRAYER   = 13908;   // Human / Dwarf racial priest spell
constexpr uint32 INNER_FIRE         = 588;
constexpr uint32 INNER_FOCUS        = 14751;   // Discipline talent
constexpr uint32 POWER_INFUSION     = 10060;   // Discipline talent
constexpr uint32 RESURRECTION       = 2006;
constexpr uint32 SMITE              = 585;
constexpr uint32 HOLY_FIRE          = 14914;
constexpr uint32 SHADOW_WORD_PAIN   = 589;
constexpr uint32 MIND_BLAST         = 8092;
constexpr uint32 MIND_FLAY          = 15407;   // Shadow talent
constexpr uint32 SHADOWFORM         = 15473;   // Shadow talent
constexpr uint32 VAMPIRIC_EMBRACE   = 15286;   // Shadow talent
constexpr uint32 SHOOT_WAND         = 5019;

constexpr uint8 POWER_MANA = 0;

bool Ready(ApPredicateContext const& ctx, uint32 spell)
{
    return ctx.bot.knows_spell(spell) && ctx.bot.is_ready(spell);
}

int32 ManaPct(ApPredicateContext const& ctx)
{
    return ctx.bot.max_power(POWER_MANA) > 0 ? ctx.bot.power_pct(POWER_MANA) : 100;
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

bool TargetHasMyAura(ApPredicateContext const& ctx, uint32 spell, ObjectGuid target)
{
    return ctx.bot.has_aura(spell, target == ctx.bot.raw().guid ? ObjectGuid::Empty : target);
}

// Best direct heal the bot knows for a "low" target: Greater Heal, then Heal,
// then Lesser Heal while leveling.
uint32 BigHeal(ApPredicateContext const& ctx)
{
    if (ctx.bot.knows_spell(GREATER_HEAL)) return GREATER_HEAL;
    if (ctx.bot.knows_spell(HEAL))         return HEAL;
    return LESSER_HEAL;
}

// ======================= Shared =======================

bool ShouldDesperatePrayer(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.hp_pct() <= 30 && !ctx.bot.has_aura(SHADOWFORM) && Ready(ctx, DESPERATE_PRAYER);   // Shadowform blocks Holy
}
void DoDesperatePrayer(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DESPERATE_PRAYER); }

bool ShouldFade(ApPredicateContext const& ctx)
{
    // Hurt with something on me, or about to pull aggro off the tank (VMaNGOS medium-threat trigger, 60% of the tank's threat).
    return ctx.bot.in_combat() && ctx.group.members() && Ready(ctx, FADE)
        && ((ctx.bot.attackers_count() >= 1 && ctx.bot.hp_pct() <= 70)
            || ThreatDump::ThreatPct(ctx.bot) >= ThreatDump::kMediumThreatPct);
}
void DoFade(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FADE); }
// A healer does not spend the GCD on Fade while someone is in the emergency band (the critical heals come first in the table, this keeps the
// threat triggers from stealing the tick when the heal is not castable yet).
bool ShouldFadeHealer(ApPredicateContext const& ctx) { return LowestFriendOrSelf(ctx).hp_pct > 40 && ShouldFade(ctx); }

bool ShouldPsychicScream(ApPredicateContext const& ctx)
{
    if (!(ctx.bot.in_combat() && ctx.bot.hp_pct() <= 40 && ctx.bot.enemies_within(8.0f) >= 2
          && Ready(ctx, PSYCHIC_SCREAM)))
        return false;
    // Raid school rule: not into a mob that avoids shadow (only that veto -
    // this is the emergency fear, the threat-discipline gates do not apply).
    float x, y, z; ctx.bot.position(x, y, z);
    // A crowd-controlled mob in the ring vetoes it too (as Frost Nova's does).
    AoeHazardKind const k = FindAoeHazard(ctx.bot, x, y, z, 8.0f, /*damaging*/ true, /*tank*/ false, PSYCHIC_SCREAM).kind;
    if (k == AoeHazardKind::SchoolAvoided || k == AoeHazardKind::CrowdControlled)
        return false;
    // Raid groups and instances: no fear with an unpulled pack within 40y; raids
    // also only for a priest an elite is hitting (ThreatDiscipline.h RaidFearAllowed).
    return RaidFearAllowed(ctx.bot, ctx.group.is_raid() || ctx.bot.is_in_raid(), "Psychic Scream");
}
void DoPsychicScream(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PSYCHIC_SCREAM); }

// Silence (Shadow talent): the interrupt rotation's pick within 20y
// (ApInterrupt.h; never on a healer spec).
bool ShouldSilence(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 5 && Interrupt::Should(ctx, Interrupt::SILENCE);
}
void DoSilence(ApPredicateContext const& ctx, BotIntentEmitter& e) { Interrupt::Cast(ctx, e, Interrupt::SILENCE); }

bool ShouldInnerFire(ApPredicateContext const& ctx)
{
    return Ready(ctx, INNER_FIRE) && !ctx.bot.has_aura(INNER_FIRE) && !ctx.bot.has_aura(SHADOWFORM)
        && ClassicHeal::InnerFireWorthRecast(ctx.bot.in_combat(), ctx.bot.is_in_instance(), ManaPct(ctx));
}
void DoInnerFire(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(INNER_FIRE); }

// Dwarf priests: Fear Ward on the raid tank the coordinator gave this priest
// (Classic::PickFearWard; refreshed when it is missing - Fear Ward is
// consumed by the first fear).
bool ShouldFearWard(ApPredicateContext const& ctx)
{
    return !ctx.bot.has_aura(SHADOWFORM) && Classic::PickFearWard(ctx.bot, ctx.group).spell != 0;
}
void DoFearWard(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    Classic::CastPick const p = Classic::PickFearWard(ctx.bot, ctx.group);
    if (p.spell)
        e.cast(p.spell, p.target);
}

bool ShouldResurrection(ApPredicateContext const& ctx)
{
    return !ctx.bot.in_combat() && Ready(ctx, RESURRECTION) && !ClassicHeal::OocRezTarget(ctx.bot, ctx.group).IsEmpty();
}
void DoResurrection(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (ObjectGuid const t = ClassicHeal::OocRezTarget(ctx.bot, ctx.group); !t.IsEmpty())
        e.cast(RESURRECTION, t);
}

bool ShouldShieldSelf(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.attackers_count() >= 1 && ctx.bot.hp_pct() <= 60
        && Ready(ctx, POWER_WORD_SHIELD)
        && !ctx.bot.has_aura(POWER_WORD_SHIELD) && !ctx.bot.has_aura(WEAKENED_SOUL) && !ctx.bot.has_aura(SHADOWFORM);
}
void DoShieldSelf(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(POWER_WORD_SHIELD); }

bool ShouldWand(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && (Ready(ctx, SHOOT_WAND) && ctx.bot.victim_in_spell_range(SHOOT_WAND)) && ctx.bot.current_cast_spell_id() == 0;
}
void DoWand(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SHOOT_WAND, ctx.bot.victim()); }

// ======================= Healer =======================

bool ShouldShieldCritical(ApPredicateContext const& ctx)
{
    if (!Ready(ctx, POWER_WORD_SHIELD)) return false;
    HealTarget const t = LowestFriendOrSelf(ctx);
    // Weakened Soul from any priest's shield blocks it (member debuff list).
    return t.hp_pct <= 35
        && !TargetHasMyAura(ctx, POWER_WORD_SHIELD, t.guid) && !TargetHasMyAura(ctx, WEAKENED_SOUL, t.guid)
        && !ClassicHeal::MemberHasDebuff(ctx.group, t.guid, WEAKENED_SOUL)
        && !ClassicHeal::ShieldClaimedByOther(ctx.bot, t.guid);
}
void DoShieldCritical(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ObjectGuid const t = LowestFriendOrSelf(ctx).guid;
    if (e.cast(POWER_WORD_SHIELD, t))
        ClassicHeal::NoteShieldCast(ctx.bot, t);
}

// Power Word: Shield on the tank as the pull lands: inside an instance map, the first swings on the group's tank (a Deadmines
// pack of eight miners took 60% of the warrior's health in the first seconds, before the first heal landed). Not on a tank
// already shielded or Weakened Soul'd, not below 35% mana (the shield then waits for the emergency rule), and not while
// someone is in the emergency band, which belongs to the heals.
// A shield is the worst heal per mana (449 mana for about 900 absorbed, against Greater Heal's 556 for about 1950: MC Garr 2026-10-11,
// 33 shields cost the healers 14.8k mana, about 15 % of all they spent), so it goes on a tank only when the swings on it are worth the
// absorb and the tank is not already full: the swings on it add up to at least kShieldMinIncoming, its projected health is at or under
// kShieldMaxTankHpPct, and the healer has kShieldMinManaPct mana. The emergency rule (ShouldShieldCritical) is not gated by these.
constexpr int64 kShieldMinIncoming  = 450;
constexpr int32 kShieldMaxTankHpPct = 85;
constexpr int32 kShieldMinManaPct   = 50;
ObjectGuid TankToShield(ApPredicateContext const& ctx)
{
    auto const* members = ctx.group.members();
    if (!members || !ClassicHeal::RaidHealing(ctx.bot) || !ctx.bot.in_combat() || ClassicHeal::ManaPct(ctx.bot) < kShieldMinManaPct)
        return ObjectGuid::Empty;
    float bx, by, bz; ctx.bot.position(bx, by, bz);
    for (GroupMemberSummary const& m : *members)
    {
        if (!m.online || !m.is_alive || m.map_id != ctx.bot.map_id() || m.max_hp <= 0 || !ClassicHeal::IsTank(ctx.bot, m))
            continue;
        if (ClassicHeal::Dist2(m.x, m.y, m.z, bx, by, bz) > 30.0f * 30.0f)
            continue;
        ClassicHeal::Patient const p = ClassicHeal::PatientFor(ctx.bot, ctx.group, m.guid);
        if (!p.valid || p.incoming < kShieldMinIncoming || p.hp_pct > kShieldMaxTankHpPct)
            continue;       // nobody is hitting it hard enough yet, or it is still full
        if (TargetHasMyAura(ctx, POWER_WORD_SHIELD, m.guid) || TargetHasMyAura(ctx, WEAKENED_SOUL, m.guid)
            || ClassicHeal::MemberHasDebuff(ctx.group, m.guid, WEAKENED_SOUL) || ClassicHeal::ShieldClaimedByOther(ctx.bot, m.guid))
            continue;
        return m.guid;
    }
    return ObjectGuid::Empty;
}
bool ShouldShieldTank(ApPredicateContext const& ctx)
{
    return Ready(ctx, POWER_WORD_SHIELD) && LowestFriendOrSelf(ctx).hp_pct > 40 && !TankToShield(ctx).IsEmpty();
}
void DoShieldTank(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ObjectGuid const t = TankToShield(ctx);
    if (!t.IsEmpty() && e.cast(POWER_WORD_SHIELD, t))
        ClassicHeal::NoteShieldCast(ctx.bot, t);
}

// Renew on the tank early in a pull: instant, 15 s, and the cheapest healing a levelling priest has. The rotation's own Renew sits
// below Heal (at 65 % and under), which a tank taking hits all fight never leaves, so the HoT was never cast and Heal / Flash Heal
// carried every pull (Deadmines, live 2026-10-10: 46 direct casts, the priest dry two pulls after starting at 64 % mana). Same
// shape as the tank shield: instance healing only, tank in reach with damage coming, not in the emergency band, 35 %+ mana.
ObjectGuid TankToRenew(ApPredicateContext const& ctx)
{
    auto const* members = ctx.group.members();
    if (!members || !ClassicHeal::RaidHealing(ctx.bot) || !ctx.bot.in_combat() || ClassicHeal::ManaPct(ctx.bot) < 35)
        return ObjectGuid::Empty;
    float bx, by, bz; ctx.bot.position(bx, by, bz);
    for (GroupMemberSummary const& m : *members)
    {
        if (!m.online || !m.is_alive || m.map_id != ctx.bot.map_id() || m.max_hp <= 0 || !ClassicHeal::IsTank(ctx.bot, m))
            continue;
        if (ClassicHeal::Dist2(m.x, m.y, m.z, bx, by, bz) > ClassicHeal::kHealRange * ClassicHeal::kHealRange)
            continue;
        ClassicHeal::Patient const p = ClassicHeal::PatientFor(ctx.bot, ctx.group, m.guid);
        if (!p.valid || !ClassicHeal::RenewTankDue(p.hp_pct, p.incoming, TargetHasMyAura(ctx, RENEW, m.guid), ClassicHeal::ManaPct(ctx.bot),
                                                    LowestFriendOrSelf(ctx).hp_pct))
            continue;
        if (!ClassicHeal::HotAllowed(ctx.bot, ctx.group, m.guid) || ClassicHeal::HotClaimedByOther(ctx.bot, ctx.group, m.guid, RENEW))
            continue;
        return m.guid;
    }
    return ObjectGuid::Empty;
}
bool ShouldRenewTank(ApPredicateContext const& ctx)
{
    return Ready(ctx, RENEW) && !TankToRenew(ctx).IsEmpty();
}
void DoRenewTank(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ObjectGuid const t = TankToRenew(ctx);
    if (e.cast(RENEW, t))
        ClassicHeal::NoteHotCast(ctx.bot, t, RENEW);
}

// Inner Focus on the next big heal: inside a dungeon or raid a boss cooldown
// (Classic::BossFight, never trash); in the open world any fight.
bool ShouldInnerFocus(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && LowestFriendOrSelf(ctx).hp_pct <= 40 && Ready(ctx, INNER_FOCUS)
        && !ctx.bot.has_aura(INNER_FOCUS) && (!ctx.bot.is_in_instance() || Classic::BossFight(ctx.bot));
}
void DoInnerFocus(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(INNER_FOCUS);
    Classic::NoteCooldownUse(ctx.bot, INNER_FOCUS, "Inner Focus");
}

bool ShouldFlashHealCritical(ApPredicateContext const& ctx)
{
    return Ready(ctx, FLASH_HEAL) && LowestFriendOrSelf(ctx).hp_pct <= 40;
}
// In a raid Flash Heal is kept for an emergency or a tank under half health
// (ClassicHeal::NeedsFastHeal); a member merely projected under 40% gets the
// mana-efficient Heal / Greater Heal (ClassicHeal::RaidPriestHeal).
void DoFlashHeal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ClassicHeal::Patient const p = ClassicHeal::PickPatient(ctx);
    uint32 const spell = ClassicHeal::RaidChoose(ctx.bot, p, FLASH_HEAL, ClassicHeal::RaidPriestHeal(ctx.bot, p, BigHeal(ctx)));
    ClassicHeal::EmitHeal(ctx.bot, ctx.group, e, spell, p);
}

// Before Flash Heal is trained (level 20) the emergency heal is the big one.
bool ShouldEmergencyBigHeal(ApPredicateContext const& ctx)
{
    return !ctx.bot.knows_spell(FLASH_HEAL) && Ready(ctx, BigHeal(ctx)) && LowestFriendOrSelf(ctx).hp_pct <= 40;
}

// Prayer of Healing heals the party within 30 yd of the priest: 3 injured
// members there (the owner's "medium aoe heal").
bool ShouldPrayerOfHealing(ApPredicateContext const& ctx)
{
    return ManaPct(ctx) >= 20 && Ready(ctx, PRAYER_OF_HEALING)
        && ClassicHeal::InjuredNearMe(ctx.bot, ctx.group, 30.0f, ClassicHeal::kMediumPct) >= 3;
}
void DoPrayerOfHealing(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(PRAYER_OF_HEALING); }

bool ShouldBigHeal(ApPredicateContext const& ctx)
{
    uint32 const spell = BigHeal(ctx);
    return Ready(ctx, spell) && LowestFriendOrSelf(ctx).hp_pct <= 65;
}
// Raid: Heal (rank 2-4) for non-tanks when it covers the deficit, Greater
// Heal for tanks under half health and emergencies (ClassicHeal::RaidPriestHeal).
void DoBigHeal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ClassicHeal::Patient const p = ClassicHeal::PickPatient(ctx);
    ClassicHeal::EmitHeal(ctx.bot, ctx.group, e, ClassicHeal::RaidPriestHeal(ctx.bot, p, BigHeal(ctx)), p);
}

bool ShouldRenew(ApPredicateContext const& ctx)
{
    if (!Ready(ctx, RENEW)) return false;
    HealTarget const t = LowestFriendOrSelf(ctx);
    return t.hp_pct <= 85 && !TargetHasMyAura(ctx, RENEW, t.guid) && ClassicHeal::HotAllowed(ctx.bot, ctx.group, t.guid)
        && ClassicHeal::HotWorthIt(ctx.bot, ctx.group, t.guid)
        && !ClassicHeal::HotClaimedByOther(ctx.bot, ctx.group, t.guid, RENEW);
}
void DoRenew(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ObjectGuid const t = LowestFriendOrSelf(ctx).guid;
    if (e.cast(RENEW, t))
        ClassicHeal::NoteHotCast(ctx.bot, t, RENEW);
}

// Dispel Magic / Abolish Disease on the member the raid dispel plan gives
// this priest (Classic::PickDispel: one bot per debuffed member).
bool ShouldDispel(ApPredicateContext const& ctx) { return ClassicHeal::ShouldAssignedDispel(ctx); }
void DoDispel(ApPredicateContext const& ctx, BotIntentEmitter& e) { ClassicHeal::DoAssignedDispel(ctx, e); }

// Power Infusion (Discipline talent): a boss fight's (Classic::BossFight)
// mage or warlock in range that nobody has infused yet, mages first.
ObjectGuid PowerInfusionTarget(ApPredicateContext const& ctx)
{
    auto const* members = ctx.group.members();
    if (!members || !Classic::BossFight(ctx.bot)) return ObjectGuid::Empty;
    float bx, by, bz; ctx.bot.position(bx, by, bz);
    for (uint8 cls : { uint8(CLASS_MAGE), uint8(CLASS_WARLOCK) })
        for (GroupMemberSummary const& m : *members)
        {
            if (m.cls != cls || !m.online || !m.is_alive || !m.in_combat || m.map_id != ctx.bot.map_id()) continue;
            if (m.max_mana <= 0 || int64(m.mana) * 100 / m.max_mana < 10) continue;
            if (ClassicHeal::Dist2(m.x, m.y, m.z, bx, by, bz) > 30.0f * 30.0f) continue;
            if (Classic::MemberHasBuff(ctx.group, m.guid, POWER_INFUSION)) continue;
            return m.guid;
        }
    return ObjectGuid::Empty;
}
bool ShouldPowerInfusion(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ManaPct(ctx) >= 20 && Ready(ctx, POWER_INFUSION)
        && LowestFriendOrSelf(ctx).hp_pct > 50 && !PowerInfusionTarget(ctx).IsEmpty();
}
void DoPowerInfusion(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(POWER_INFUSION, PowerInfusionTarget(ctx));
    Classic::NoteCooldownUse(ctx.bot, POWER_INFUSION, "Power Infusion");
}

// Damage only while everyone is healthy and mana is comfortable. The wand
// costs no mana (it keeps the five-second rule); spells in a raid also wait
// for a quiet spell at 90%+ mana (ClassicHeal::RaidHealerMayDamage).
bool HealerMayWand(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ctx.bot.in_combat() && WoundedCount(ctx, 90) == 0 && LowestFriendOrSelf(ctx).hp_pct > 90;
}
bool HealerMayDps(ApPredicateContext const& ctx)
{
    return HealerMayWand(ctx) && ClassicHeal::RaidHealerMayDamage(ctx.bot, ctx.group);
}

bool ShouldHealerPain(ApPredicateContext const& ctx)
{
    return HealerMayDps(ctx) && ManaPct(ctx) >= 50 && (Ready(ctx, SHADOW_WORD_PAIN) && ctx.bot.victim_in_spell_range(SHADOW_WORD_PAIN))
        && !ctx.bot.has_aura(SHADOW_WORD_PAIN, ctx.bot.victim());
}
void DoPain(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SHADOW_WORD_PAIN, ctx.bot.victim()); }

bool ShouldHealerSmite(ApPredicateContext const& ctx)
{
    return HealerMayDps(ctx) && ManaPct(ctx) >= 70 && (Ready(ctx, SMITE) && ctx.bot.victim_in_spell_range(SMITE));
}
void DoSmite(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SMITE, ctx.bot.victim()); }

bool ShouldHealerWand(ApPredicateContext const& ctx)
{
    return HealerMayWand(ctx) && ShouldWand(ctx);
}

// Raid school rule (ApSchoolHint.h): the healer's damage picks in the
// victim's preferred school first - holy: Holy Fire / Smite, shadow: Shadow
// Word: Pain. Same may-dps and mana gates as Smite; heals are untouched.
bool ShouldHealerHolyFire(ApPredicateContext const& ctx)
{
    return HealerMayDps(ctx) && ManaPct(ctx) >= 70 && Ready(ctx, HOLY_FIRE);
}

SchoolHint::SchoolChoice const kHealerSchoolChoices[] = {
    { HOLY_FIRE,        ShouldHealerHolyFire },
    { SMITE,            ShouldHealerSmite },
    { SHADOW_WORD_PAIN, ShouldHealerPain },
};
bool ShouldHealerSchoolNuke(ApPredicateContext const& ctx) { return SchoolHint::ShouldCast(ctx, kHealerSchoolChoices); }
void DoHealerSchoolNuke(ApPredicateContext const& ctx, BotIntentEmitter& e) { SchoolHint::DoCast(ctx, e, kHealerSchoolChoices); }

ApRule const kHealerRules[] = {
    { ShouldDesperatePrayer,   DoDesperatePrayer,  "classic_heal:desperate_prayer" },
    { ShouldShieldCritical,    DoShieldCritical,   "classic_heal:shield_critical" },
    { ShouldShieldTank,        DoShieldTank,       "classic_heal:shield_tank" },
    { ShouldRenewTank,         DoRenewTank,        "classic_heal:renew_tank" },
    { ShouldInnerFocus,        DoInnerFocus,       "classic_heal:inner_focus" },
    { ShouldFlashHealCritical, DoFlashHeal,        "classic_heal:flash_heal" },
    { ShouldEmergencyBigHeal,  DoBigHeal,          "classic_heal:emergency_heal" },
    { ShouldFadeHealer,        DoFade,             "classic_heal:fade" },
    { ShouldPsychicScream,     DoPsychicScream,    "classic_heal:psychic_scream" },
    { ShouldPrayerOfHealing,   DoPrayerOfHealing,  "classic_heal:prayer_of_healing" },
    { ShouldBigHeal,           DoBigHeal,          "classic_heal:big_heal" },
    { ShouldDispel,            DoDispel,           "classic_heal:dispel" },
    { Interrupt::Purge::Should, Interrupt::Purge::Cast, "classic_heal:purge" },
    { ShouldPowerInfusion,     DoPowerInfusion,    "classic_heal:power_infusion" },
    { ShouldFearWard,          DoFearWard,         "classic_heal:fear_ward" },   // before Renew, which fires all fight in a raid
    { ShouldInnerFire,         DoInnerFire,        "classic_heal:inner_fire" },
    { ShouldRenew,             DoRenew,            "classic_heal:renew" },
    { ShouldResurrection,      DoResurrection,     "classic_heal:resurrection" },
    { ShouldHealerSchoolNuke,  DoHealerSchoolNuke, "classic_heal:school_nuke" },
    { ShouldHealerPain,        DoPain,             "classic_heal:shadow_word_pain" },
    { ShouldHealerSmite,       DoSmite,            "classic_heal:smite" },
    { ShouldHealerWand,        DoWand,             "classic_heal:wand" },
};

// ======================= Shadow =======================

bool ShouldShadowform(ApPredicateContext const& ctx)
{
    return Ready(ctx, SHADOWFORM) && !ctx.bot.has_aura(SHADOWFORM);
}
void DoShadowform(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(SHADOWFORM); }

bool ShouldShadowSelfHeal(ApPredicateContext const& ctx)
{
    // Shadowform blocks Holy spells; only heal outside it.
    return !ctx.bot.has_aura(SHADOWFORM) && ctx.bot.hp_pct() <= 35 && Ready(ctx, FLASH_HEAL);
}
void DoShadowSelfHeal(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FLASH_HEAL); }

bool ShouldVampiricEmbrace(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && (Ready(ctx, VAMPIRIC_EMBRACE) && ctx.bot.victim_in_spell_range(VAMPIRIC_EMBRACE)) && !ctx.bot.has_aura(VAMPIRIC_EMBRACE, ctx.bot.victim());
}
void DoVampiricEmbrace(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(VAMPIRIC_EMBRACE, ctx.bot.victim()); }

bool ShouldShadowPain(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 10 && (Ready(ctx, SHADOW_WORD_PAIN) && ctx.bot.victim_in_spell_range(SHADOW_WORD_PAIN))
        && !ctx.bot.has_aura(SHADOW_WORD_PAIN, ctx.bot.victim());
}

bool ShouldMindBlast(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 15 && (Ready(ctx, MIND_BLAST) && ctx.bot.victim_in_spell_range(MIND_BLAST));
}
void DoMindBlast(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(MIND_BLAST, ctx.bot.victim()); }

bool ShouldMindFlay(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 15 && (Ready(ctx, MIND_FLAY) && ctx.bot.victim_in_spell_range(MIND_FLAY));
}
void DoMindFlay(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(MIND_FLAY, ctx.bot.victim()); }

// Leveling Shadow before Mind Flay (talent at 20+): Smite / Holy Fire filler.
bool ShouldShadowSmite(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && !ctx.bot.has_aura(SHADOWFORM) && !ctx.bot.knows_spell(MIND_FLAY)
        && ManaPct(ctx) >= 20 && (Ready(ctx, SMITE) && ctx.bot.victim_in_spell_range(SMITE));
}

// Raid school rule (ApSchoolHint.h): shadow on the victim keeps Shadow Word:
// Pain / Mind Blast / Mind Flay first; holy puts Holy Fire / Smite first, but
// only outside Shadowform (which blocks Holy spells), otherwise the priest has
// no spell of that school and keeps its shadow rotation.
bool ShouldShadowHolyFire(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && !ctx.bot.has_aura(SHADOWFORM) && ManaPct(ctx) >= 20 && Ready(ctx, HOLY_FIRE);
}
bool ShouldShadowHolySmite(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && !ctx.bot.has_aura(SHADOWFORM) && ManaPct(ctx) >= 20 && (Ready(ctx, SMITE) && ctx.bot.victim_in_spell_range(SMITE));
}

SchoolHint::SchoolChoice const kShadowSchoolChoices[] = {
    { SHADOW_WORD_PAIN, ShouldShadowPain },
    { MIND_BLAST,       ShouldMindBlast },
    { MIND_FLAY,        ShouldMindFlay },
    { HOLY_FIRE,        ShouldShadowHolyFire },
    { SMITE,            ShouldShadowHolySmite },
};
bool ShouldShadowSchoolNuke(ApPredicateContext const& ctx) { return SchoolHint::ShouldCast(ctx, kShadowSchoolChoices); }
void DoShadowSchoolNuke(ApPredicateContext const& ctx, BotIntentEmitter& e) { SchoolHint::DoCast(ctx, e, kShadowSchoolChoices); }

// Shackle Undead: Undead. In a group an add of a 3+ mob pull (never the kill target,
// the tank's target or a boss); solo an extra attacker. See PickGroupCC.
constexpr uint32 SHACKLE_UNDEAD = 9484;
bool ShouldCrowdControl(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ctx.bot.in_combat() && !ctx.bot.is_moving() && ManaPct(ctx) >= 15
        && Ready(ctx, SHACKLE_UNDEAD) && !PickGroupCC(ctx, SHACKLE_UNDEAD).IsEmpty();
}
void DoCrowdControl(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(SHACKLE_UNDEAD, PickGroupCC(ctx, SHACKLE_UNDEAD));
}

ApRule const kShadowRules[] = {
    { ShouldDesperatePrayer,  DoDesperatePrayer,  "classic_shadow:desperate_prayer" },
    { ShouldShieldSelf,       DoShieldSelf,       "classic_shadow:shield_self" },
    { ShouldShadowSelfHeal,   DoShadowSelfHeal,   "classic_shadow:flash_heal_self" },
    { ShouldFade,             DoFade,             "classic_shadow:fade" },
    { ShouldPsychicScream,    DoPsychicScream,    "classic_shadow:psychic_scream" },
    { ShouldSilence,          DoSilence,          "classic_shadow:silence" },
    { Interrupt::Purge::Should, Interrupt::Purge::Cast, "classic_shadow:purge" },
    { ShouldCrowdControl,     DoCrowdControl,     "classic_shadow:shackle_undead" },
    { ShouldShadowform,       DoShadowform,       "classic_shadow:shadowform" },
    { ShouldFearWard,         DoFearWard,         "classic_shadow:fear_ward" },
    { ShouldInnerFire,        DoInnerFire,        "classic_shadow:inner_fire" },
    { ShouldVampiricEmbrace,  DoVampiricEmbrace,  "classic_shadow:vampiric_embrace" },
    { ShouldShadowSchoolNuke, DoShadowSchoolNuke, "classic_shadow:school_nuke" },
    { ShouldShadowPain,       DoPain,             "classic_shadow:shadow_word_pain" },
    { ShouldMindBlast,        DoMindBlast,        "classic_shadow:mind_blast" },
    { ShouldMindFlay,         DoMindFlay,         "classic_shadow:mind_flay" },
    { ShouldShadowSmite,      DoSmite,            "classic_shadow:smite" },
    { ShouldWand,             DoWand,             "classic_shadow:wand" },
    { HasLiveTarget,          DoAutoAttack,       "classic_shadow:auto_attack" },
};

} // namespace

void RegisterApl_Classic_Priest()
{
    ApRotation const healer{ std::span<ApRule const>(kHealerRules) };
    ApRotation const shadow{ std::span<ApRule const>(kShadowRules) };
    RegisterRotationOverride(CLASS_PRIEST, 0, healer);
    RegisterRotationOverride(CLASS_PRIEST, 256, healer);
    RegisterRotationOverride(CLASS_PRIEST, 257, healer);
    RegisterRotationOverride(CLASS_PRIEST, 258, shadow);
}

} // namespace Playerbot::Combat
