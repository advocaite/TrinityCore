// Classic 1.60 (Forever) Shaman - vanilla rotations.
//
// Registered over the retail Elemental / Enhancement / Restoration rotations
// and the shaman baseline (spec 0). 262 = Elemental caster DPS, 263 and 0 =
// Enhancement melee DPS, 264 = Restoration healer. Spell ids are vanilla rank
// 1 and resolve to the highest known rank in the snapshot view and
// API::cast_spell.
//
// Priorities follow Karrz's ike3 vanilla strategies (Karrz1/wow-1.12
// raid10-revamp, playerbot/strategy/shaman/*Strategy.cpp, MANGOSBOT_ZERO).
// Totems and weapon imbues come from Bot/ClassicUpkeep.cpp (one totem per
// element by spec and party, Tremor / cleansing totems when the party needs
// them; Windfury / Flametongue / Rockbiter Weapon by spec), shared with the
// out-of-combat idle:classic_weapon rule.

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Apl_Baseline_Common.h"
#include "ApClassicHeal.h"
#include "ApSchoolHint.h"
#include "ApInterrupt.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/ThreatDiscipline.h"
#include "Bot/ClassicUpkeep.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"
#include <cmath>

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::DoAutoAttack;
using ::Playerbot::Combat::baseline_common::HasLiveTarget;

// ---- Vanilla spell ids (rank 1) ----
constexpr uint32 HEALING_WAVE        = 331;
constexpr uint32 LESSER_HEALING_WAVE = 8004;
constexpr uint32 CHAIN_HEAL          = 1064;
constexpr uint32 ANCESTRAL_SPIRIT    = 2008;
constexpr uint32 NATURES_SWIFTNESS   = 16188;   // Restoration talent (also Elemental)
constexpr uint32 LIGHTNING_SHIELD    = 324;
constexpr uint32 LIGHTNING_BOLT      = 403;
constexpr uint32 CHAIN_LIGHTNING     = 421;
constexpr uint32 EARTH_SHOCK         = 8042;
constexpr uint32 FLAME_SHOCK         = 8050;
constexpr uint32 FROST_SHOCK         = 8056;    // raid school rule only (frost preferred)
constexpr uint32 STORMSTRIKE         = 17364;   // Enhancement talent
constexpr uint32 ELEMENTAL_MASTERY   = 16166;   // Elemental talent

constexpr uint8 POWER_MANA = 0;

constexpr uint32 SPEC_ELEMENTAL = 262;
constexpr uint32 SPEC_ENHANCE   = 263;
constexpr uint32 SPEC_RESTO     = 264;

bool Ready(ApPredicateContext const& ctx, uint32 spell)
{
    return spell != 0 && ctx.bot.knows_spell(spell) && ctx.bot.is_ready(spell);
}

int32 ManaPct(ApPredicateContext const& ctx)
{
    return ctx.bot.max_power(POWER_MANA) > 0 ? ctx.bot.power_pct(POWER_MANA) : 100;
}

float DistToVictim(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return 1000.f;
    auto const& me = ctx.bot.raw().position;
    float const dx = v->x - me.x, dy = v->y - me.y, dz = v->z - me.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
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

// ======================= Shared =======================

bool ShouldLightningShield(ApPredicateContext const& ctx)
{
    return Ready(ctx, LIGHTNING_SHIELD) && !ctx.bot.has_aura(LIGHTNING_SHIELD);
}
void DoLightningShield(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(LIGHTNING_SHIELD); }

bool ShouldResurrection(ApPredicateContext const& ctx)
{
    return !ctx.bot.in_combat() && Ready(ctx, ANCESTRAL_SPIRIT) && !ClassicHeal::OocRezTarget(ctx.bot, ctx.group).IsEmpty();
}
void DoResurrection(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (ObjectGuid const t = ClassicHeal::OocRezTarget(ctx.bot, ctx.group); !t.IsEmpty())
        e.cast(ANCESTRAL_SPIRIT, t);
}

bool ShouldEarthShockInterrupt(ApPredicateContext const& ctx)
{
    // Interrupt rotation (ApInterrupt.h): the plan's pick within 20y.
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 10 && Interrupt::Should(ctx, EARTH_SHOCK);
}
void DoEarthShockInterrupt(ApPredicateContext const& ctx, BotIntentEmitter& e) { Interrupt::Cast(ctx, e, EARTH_SHOCK); }
void DoEarthShock(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(EARTH_SHOCK, ctx.bot.victim()); }

// Cure Poison / Cure Disease on the member the raid dispel plan gives this
// shaman (Classic::PickDispel: one bot per debuffed member), every spec.
bool ShouldCure(ApPredicateContext const& ctx) { return ClassicHeal::ShouldAssignedDispel(ctx); }
void DoCure(ApPredicateContext const& ctx, BotIntentEmitter& e) { ClassicHeal::DoAssignedDispel(ctx, e); }

// Imbue that faded mid-fight (out of combat idle:classic_weapon keeps it up).
bool ShouldImbue(ApPredicateContext const& ctx)
{
    return Classic::PickImbue(ctx.bot, true).spell != 0;
}
void DoImbue(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    Classic::CastPick const p = Classic::PickImbue(ctx.bot, true);
    e.cast_on_item(p.spell, p.target);
}

// Totems: worth the mana in a group, on elites and multi-mob pulls, or a
// fresh solo target; not on trash about to die.
bool ShouldTotem(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx) || !ctx.bot.in_combat())
        return false;
    if (!ctx.group.exists())
    {
        NearbyUnit const* v = ctx.bot.victim_info();
        if (!v || !(v->is_elite_or_rare || ctx.bot.attackers_count() >= 2 || v->hp_pct() >= 60))
            return false;
    }
    return Classic::PickTotem(ctx.bot, ctx.group) != 0;
}
void DoTotem(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(Classic::PickTotem(ctx.bot, ctx.group));
}

// ======================= Restoration =======================

bool ShouldNaturesSwiftness(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && LowestFriendOrSelf(ctx).hp_pct <= 30 && Ready(ctx, NATURES_SWIFTNESS)
        && !ctx.bot.has_aura(NATURES_SWIFTNESS);
}
void DoNaturesSwiftness(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(NATURES_SWIFTNESS); }

// With Nature's Swiftness up the next Healing Wave is instant: use the big one.
bool ShouldSwiftHealingWave(ApPredicateContext const& ctx)
{
    return ctx.bot.has_aura(NATURES_SWIFTNESS) && Ready(ctx, HEALING_WAVE) && LowestFriendOrSelf(ctx).hp_pct <= 40;
}
// The instant (Nature's Swiftness) Healing Wave is the emergency save: top rank.
void DoSwiftHealingWave(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HEALING_WAVE, LowestFriendOrSelf(ctx).guid);
}
void DoHealingWave(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    CastHeal(ctx, e, HEALING_WAVE, LowestFriendOrSelf(ctx).guid);
}

bool ShouldLesserHealingWaveCritical(ApPredicateContext const& ctx)
{
    return Ready(ctx, LESSER_HEALING_WAVE) && LowestFriendOrSelf(ctx).hp_pct <= 40;
}
// In a raid Lesser Healing Wave (poor heal per mana) is kept for an
// emergency or a tank under half health; everyone else gets a downranked
// Healing Wave (ClassicHeal::RaidChoose).
void DoLesserHealingWave(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ClassicHeal::Patient const p = ClassicHeal::PickPatient(ctx);
    ClassicHeal::EmitHeal(ctx.bot, ctx.group, e, ClassicHeal::RaidChoose(ctx.bot, p, LESSER_HEALING_WAVE, HEALING_WAVE), p);
}

// Before Lesser Healing Wave (level 20) the emergency heal is Healing Wave.
bool ShouldEmergencyHealingWave(ApPredicateContext const& ctx)
{
    return !ctx.bot.knows_spell(LESSER_HEALING_WAVE) && Ready(ctx, HEALING_WAVE) && LowestFriendOrSelf(ctx).hp_pct <= 40;
}

// Chain Heal: 3 injured members (the owner's "medium aoe heal"), cast on the
// injured member with the most injured within a 12.5 yd jump of it.
ObjectGuid ChainHealTarget(ApPredicateContext const& ctx)
{
    return ClassicHeal::BestClusterTarget(ctx.bot, ctx.group, 12.5f, ClassicHeal::kMediumPct, 3);
}
bool ShouldChainHeal(ApPredicateContext const& ctx)
{
    return ManaPct(ctx) >= 20 && Ready(ctx, CHAIN_HEAL) && !ChainHealTarget(ctx).IsEmpty();
}
// In a raid the rank is sized to the first target's deficit (ClassicHeal::
// PlanHeal: Chain Heal rank 1 / 2 on a lightly hurt cluster).
void DoChainHeal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ObjectGuid const t = ChainHealTarget(ctx);
    if (ClassicHeal::RaidHealing(ctx.bot))
        ClassicHeal::EmitHeal(ctx.bot, ctx.group, e, CHAIN_HEAL, ClassicHeal::PatientFor(ctx.bot, ctx.group, t));
    else
        e.cast(CHAIN_HEAL, t);
}

bool ShouldHealingWave(ApPredicateContext const& ctx)
{
    return Ready(ctx, HEALING_WAVE) && LowestFriendOrSelf(ctx).hp_pct <= 70;
}

bool ShouldTopOffLesser(ApPredicateContext const& ctx)
{
    return Ready(ctx, LESSER_HEALING_WAVE) && LowestFriendOrSelf(ctx).hp_pct <= 85 && ManaPct(ctx) >= 30;
}


// Damage only while everyone is healthy and mana is comfortable; in a raid
// only after a quiet spell at 90%+ mana (ClassicHeal::RaidHealerMayDamage:
// the five-second rule).
bool HealerMayDps(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ctx.bot.in_combat() && WoundedCount(ctx, 90) == 0 && LowestFriendOrSelf(ctx).hp_pct > 90
        && ClassicHeal::RaidHealerMayDamage(ctx.bot, ctx.group);
}

bool ShouldHealerLightningBolt(ApPredicateContext const& ctx)
{
    return HealerMayDps(ctx) && ManaPct(ctx) >= 70 && (Ready(ctx, LIGHTNING_BOLT) && ctx.bot.victim_in_spell_range(LIGHTNING_BOLT));
}
void DoLightningBolt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(LIGHTNING_BOLT, ctx.bot.victim()); }

// Raid school rule (ApSchoolHint.h). Every shaman list puts the victim's
// preferred school first among its damage picks: nature Lightning Bolt /
// Chain Lightning / Earth Shock, fire Flame Shock, frost Frost Shock. Shocks
// keep the 20y reach the rotation uses for them.
bool ShockUsable(ApPredicateContext const& ctx, uint32 spell, int32 min_mana)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= min_mana && Ready(ctx, spell) && DistToVictim(ctx) <= 20.f;
}

// Healer: only while it may dps at all (healer gates as Lightning Bolt's).
bool ShouldHealerFlameShock(ApPredicateContext const& ctx)
{
    return HealerMayDps(ctx) && ShockUsable(ctx, FLAME_SHOCK, 70) && !ctx.bot.has_aura(FLAME_SHOCK, ctx.bot.victim());
}
bool ShouldHealerFrostShock(ApPredicateContext const& ctx)
{
    return HealerMayDps(ctx) && ShockUsable(ctx, FROST_SHOCK, 70);
}

SchoolHint::SchoolChoice const kHealerSchoolChoices[] = {
    { LIGHTNING_BOLT, ShouldHealerLightningBolt },
    { FLAME_SHOCK,    ShouldHealerFlameShock },
    { FROST_SHOCK,    ShouldHealerFrostShock },
};
bool ShouldHealerSchoolNuke(ApPredicateContext const& ctx) { return SchoolHint::ShouldCast(ctx, kHealerSchoolChoices); }
void DoHealerSchoolNuke(ApPredicateContext const& ctx, BotIntentEmitter& e) { SchoolHint::DoCast(ctx, e, kHealerSchoolChoices); }

ApRule const kHealerRules[] = {
    { ShouldNaturesSwiftness,          DoNaturesSwiftness,    "classic_sham_heal:natures_swiftness" },
    { ShouldSwiftHealingWave,          DoSwiftHealingWave,    "classic_sham_heal:swift_healing_wave" },
    { ShouldLesserHealingWaveCritical, DoLesserHealingWave,   "classic_sham_heal:lesser_healing_wave" },
    { ShouldEmergencyHealingWave,      DoHealingWave,         "classic_sham_heal:emergency_healing_wave" },
    { ShouldChainHeal,                 DoChainHeal,           "classic_sham_heal:chain_heal" },
    { ShouldHealingWave,               DoHealingWave,         "classic_sham_heal:healing_wave" },
    { ShouldCure,                      DoCure,                "classic_sham_heal:cure" },
    { ShouldTotem,                     DoTotem,               "classic_sham_heal:totem" },
    { Interrupt::Purge::Should,        Interrupt::Purge::Cast, "classic_sham_heal:purge" },
    { ShouldTopOffLesser,              DoLesserHealingWave,   "classic_sham_heal:top_off" },
    { ShouldResurrection,              DoResurrection,        "classic_sham_heal:ancestral_spirit" },
    { ShouldLightningShield,           DoLightningShield,     "classic_sham_heal:lightning_shield" },
    { ShouldImbue,                     DoImbue,               "classic_sham_heal:imbue" },
    { ShouldHealerSchoolNuke,          DoHealerSchoolNuke,    "classic_sham_heal:school_nuke" },
    { ShouldHealerLightningBolt,       DoLightningBolt,       "classic_sham_heal:lightning_bolt" },
};

// ======================= Elemental =======================

// Inside a dungeon or raid a boss cooldown (Classic::BossFight, never trash);
// in the open world on cooldown, as before.
bool ShouldElementalMastery(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ctx.bot.in_combat() && ManaPct(ctx) >= 20 && Ready(ctx, ELEMENTAL_MASTERY)
        && (!ctx.bot.is_in_instance() || Classic::BossFight(ctx.bot));
}
void DoElementalMastery(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(ELEMENTAL_MASTERY);
    Classic::NoteCooldownUse(ctx.bot, ELEMENTAL_MASTERY, "Elemental Mastery");
}

bool ShouldSelfHeal(ApPredicateContext const& ctx)
{
    return ctx.bot.hp_pct() <= 30 && ctx.bot.attackers_count() == 0
        && ManaPct(ctx) >= 20 && Ready(ctx, LESSER_HEALING_WAVE);
}
void DoSelfHeal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LESSER_HEALING_WAVE, ctx.bot.raw().guid);
}

bool ShouldFlameShock(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 15 && (Ready(ctx, FLAME_SHOCK) && ctx.bot.victim_in_spell_range(FLAME_SHOCK))
        && !ctx.bot.has_aura(FLAME_SHOCK, ctx.bot.victim()) && DistToVictim(ctx) <= 20.f;
}
void DoFlameShock(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(FLAME_SHOCK, ctx.bot.victim()); }

bool ShouldChainLightning(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 15 && ctx.bot.enemies_within(30.0f) >= 2 && (Ready(ctx, CHAIN_LIGHTNING) && ctx.bot.victim_in_spell_range(CHAIN_LIGHTNING))
        // The jumps land on the victim's neighbours (~12y hops).
        && AoeSafeAroundVictim(ctx.bot, 12.0f, "Chain Lightning", CHAIN_LIGHTNING);
}
void DoChainLightning(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CHAIN_LIGHTNING, ctx.bot.victim()); }

// Single target: Chain Lightning is also the stronger filler on cooldown.
bool ShouldChainLightningSingle(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 30 && (Ready(ctx, CHAIN_LIGHTNING) && ctx.bot.victim_in_spell_range(CHAIN_LIGHTNING))
        && AoeSafeAroundVictim(ctx.bot, 12.0f, "Chain Lightning", CHAIN_LIGHTNING);
}

bool ShouldLightningBolt(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 5 && (Ready(ctx, LIGHTNING_BOLT) && ctx.bot.victim_in_spell_range(LIGHTNING_BOLT));
}

bool ShouldEleFrostShock(ApPredicateContext const& ctx) { return ShockUsable(ctx, FROST_SHOCK, 15); }

SchoolHint::SchoolChoice const kEleSchoolChoices[] = {
    { FLAME_SHOCK,     ShouldFlameShock },
    { FROST_SHOCK,     ShouldEleFrostShock },
    { CHAIN_LIGHTNING, ShouldChainLightning },
    { CHAIN_LIGHTNING, ShouldChainLightningSingle },
    { LIGHTNING_BOLT,  ShouldLightningBolt },
};
bool ShouldEleSchoolNuke(ApPredicateContext const& ctx) { return SchoolHint::ShouldCast(ctx, kEleSchoolChoices); }
void DoEleSchoolNuke(ApPredicateContext const& ctx, BotIntentEmitter& e) { SchoolHint::DoCast(ctx, e, kEleSchoolChoices); }

// Out of mana falls through to auto attack (melee) until it recovers.
ApRule const kElementalRules[] = {
    { ShouldEarthShockInterrupt, DoEarthShockInterrupt, "classic_sham_ele:earth_shock_interrupt" },
    { ShouldSelfHeal,            DoSelfHeal,          "classic_sham_ele:self_heal" },
    { ShouldLightningShield,     DoLightningShield,   "classic_sham_ele:lightning_shield" },
    { ShouldImbue,               DoImbue,             "classic_sham_ele:imbue" },
    { ShouldElementalMastery,    DoElementalMastery,  "classic_sham_ele:elemental_mastery" },
    { ShouldTotem,               DoTotem,             "classic_sham_ele:totem" },
    { Interrupt::Purge::Should,  Interrupt::Purge::Cast, "classic_sham_ele:purge" },
    { ShouldCure,                DoCure,              "classic_sham_ele:cure" },
    { ShouldEleSchoolNuke,       DoEleSchoolNuke,     "classic_sham_ele:school_nuke" },
    { ShouldFlameShock,          DoFlameShock,        "classic_sham_ele:flame_shock" },
    { ShouldChainLightning,      DoChainLightning,    "classic_sham_ele:chain_lightning_aoe" },
    { ShouldChainLightningSingle, DoChainLightning,   "classic_sham_ele:chain_lightning" },
    { ShouldLightningBolt,       DoLightningBolt,     "classic_sham_ele:lightning_bolt" },
    { ShouldResurrection,        DoResurrection,      "classic_sham_ele:ancestral_spirit" },
    { HasLiveTarget,             DoAutoAttack,        "classic_sham_ele:auto_attack" },
};

// ======================= Enhancement =======================

bool ShouldEnhEarthShockInterrupt(ApPredicateContext const& ctx)
{
    return ShouldEarthShockInterrupt(ctx);
}

bool ShouldStormstrike(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 10 && (Ready(ctx, STORMSTRIKE) && ctx.bot.victim_in_spell_range(STORMSTRIKE)) && DistToVictim(ctx) <= 6.f;
}
void DoStormstrike(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(STORMSTRIKE, ctx.bot.victim()); }

bool ShouldEnhEarthShock(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 40 && (Ready(ctx, EARTH_SHOCK) && ctx.bot.victim_in_spell_range(EARTH_SHOCK)) && DistToVictim(ctx) <= 20.f;
}

bool ShouldEnhFlameShock(ApPredicateContext const& ctx)
{
    // Leveling filler before Stormstrike exists: Flame Shock DoT, then Earth Shock.
    return !ctx.bot.knows_spell(STORMSTRIKE) && ShouldFlameShock(ctx) && ManaPct(ctx) >= 30;
}

// Enhancement: a preferred spell school on the victim puts its shock ahead of
// Stormstrike (physical); the shocks keep their 30-40% mana floors.
bool ShouldEnhSchoolFlameShock(ApPredicateContext const& ctx) { return ShouldFlameShock(ctx) && ManaPct(ctx) >= 30; }
bool ShouldEnhFrostShock(ApPredicateContext const& ctx)       { return ShockUsable(ctx, FROST_SHOCK, 40); }

SchoolHint::SchoolChoice const kEnhSchoolChoices[] = {
    { FLAME_SHOCK, ShouldEnhSchoolFlameShock },
    { FROST_SHOCK, ShouldEnhFrostShock },
    { EARTH_SHOCK, ShouldEnhEarthShock },
};
bool ShouldEnhSchoolNuke(ApPredicateContext const& ctx) { return SchoolHint::ShouldCast(ctx, kEnhSchoolChoices); }
void DoEnhSchoolNuke(ApPredicateContext const& ctx, BotIntentEmitter& e) { SchoolHint::DoCast(ctx, e, kEnhSchoolChoices); }

ApRule const kEnhRules[] = {
    { ShouldEnhEarthShockInterrupt, DoEarthShockInterrupt, "classic_sham_enh:earth_shock_interrupt" },
    { ShouldSelfHeal,               DoSelfHeal,        "classic_sham_enh:self_heal" },
    { ShouldLightningShield,        DoLightningShield, "classic_sham_enh:lightning_shield" },
    { ShouldImbue,                  DoImbue,           "classic_sham_enh:imbue" },
    { ShouldEnhSchoolNuke,          DoEnhSchoolNuke,   "classic_sham_enh:school_nuke" },
    { ShouldStormstrike,            DoStormstrike,     "classic_sham_enh:stormstrike" },
    { ShouldTotem,                  DoTotem,           "classic_sham_enh:totem" },
    { Interrupt::Purge::Should,     Interrupt::Purge::Cast, "classic_sham_enh:purge" },
    { ShouldCure,                   DoCure,            "classic_sham_enh:cure" },
    { ShouldEnhFlameShock,          DoFlameShock,      "classic_sham_enh:flame_shock" },
    { ShouldEnhEarthShock,          DoEarthShock,      "classic_sham_enh:earth_shock" },
    { ShouldResurrection,           DoResurrection,    "classic_sham_enh:ancestral_spirit" },
    { HasLiveTarget,                DoAutoAttack,      "classic_sham_enh:auto_attack" },
};

} // namespace

void RegisterApl_Classic_Shaman()
{
    ApRotation const ele{ std::span<ApRule const>(kElementalRules) };
    ApRotation const enh{ std::span<ApRule const>(kEnhRules) };
    ApRotation const resto{ std::span<ApRule const>(kHealerRules) };
    RegisterRotationOverride(CLASS_SHAMAN, 0, enh);
    RegisterRotationOverride(CLASS_SHAMAN, SPEC_ELEMENTAL, ele);
    RegisterRotationOverride(CLASS_SHAMAN, SPEC_ENHANCE, enh);
    RegisterRotationOverride(CLASS_SHAMAN, SPEC_RESTO, resto);
}

} // namespace Playerbot::Combat
