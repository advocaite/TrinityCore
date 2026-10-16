// Classic 1.60 (Forever) Paladin - vanilla rotations.
//
// Registered over the retail Holy / Protection / Retribution rotations and the
// paladin baseline (spec 0). 65 = Holy healer, 66 = Protection tank,
// 70 and 0 = Retribution melee DPS. Spell ids are vanilla rank 1 and resolve
// to the highest known rank in the snapshot view and API::cast_spell.
//
// Priorities follow Karrz's ike3 vanilla strategies (Karrz1/wow-1.12
// raid10-revamp, playerbot/strategy/paladin/*Strategy.cpp, MANGOSBOT_ZERO).
// Blessings follow the group assignment in Bot/ClassicUpkeep.cpp (each
// paladin covers one blessing per class, Greater Blessings with Symbol of
// Kings). Auras and Cleanse follow the raid plans in Bot/ClassicUpkeep.cpp
// (PickPaladinAura, PickDispel). Skipped: Blessing of Protection/Freedom on
// others,
// and Crusader Strike (does not exist in 1.12). Exorcism / Holy Wrath are
// skipped (undead / demon only and the view exposes no creature type).

#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Apl_Baseline_Common.h"
#include "ApClassicHeal.h"
#include "ApTankThreat.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/ThreatDiscipline.h"
#include "Bot/ClassicUpkeep.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::DoAutoAttack;
using ::Playerbot::Combat::baseline_common::HasLiveTarget;

// ---- Vanilla spell ids (rank 1) ----
constexpr uint32 HOLY_LIGHT          = 635;
constexpr uint32 FLASH_OF_LIGHT      = 19750;
constexpr uint32 HOLY_SHOCK          = 20473;   // Holy talent
constexpr uint32 LAY_ON_HANDS        = 633;
constexpr uint32 DIVINE_PROTECTION   = 498;
constexpr uint32 DIVINE_SHIELD       = 642;
constexpr uint32 FORBEARANCE         = 25771;   // debuff from Divine Shield / Protection / Lay on Hands
constexpr uint32 REDEMPTION          = 7328;
constexpr uint32 RIGHTEOUS_FURY      = 25780;
constexpr uint32 HOLY_SHIELD         = 20925;   // Protection talent
constexpr uint32 CONSECRATION        = 26573;
constexpr uint32 HAMMER_OF_JUSTICE   = 853;
constexpr uint32 SEAL_RIGHTEOUSNESS  = 21084;
constexpr uint32 SEAL_COMMAND        = 20375;   // Retribution talent
constexpr uint32 SEAL_OF_LIGHT       = 20165;
constexpr uint32 SEAL_OF_WISDOM      = 20166;
constexpr uint32 SEAL_OF_CRUSADER    = 21082;
constexpr uint32 JUDGEMENT           = 20271;
constexpr uint32 HAMMER_OF_WRATH     = 24275;
constexpr uint32 DIVINE_FAVOR        = 20216;   // Holy talent

constexpr uint8 POWER_MANA = 0;

constexpr uint32 SPEC_HOLY = 65;
constexpr uint32 SPEC_PROT = 66;
constexpr uint32 SPEC_RET  = 70;

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

int32 VictimHpPct(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v || v->max_hp <= 0) return 100;
    return int32(int64(v->hp) * 100 / v->max_hp);
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

bool TargetHasAura(ApPredicateContext const& ctx, uint32 spell, ObjectGuid target)
{
    return ctx.bot.has_aura(spell, target == ctx.bot.raw().guid ? ObjectGuid::Empty : target);
}

bool HasAnySeal(ApPredicateContext const& ctx)
{
    for (uint32 const seal : { SEAL_RIGHTEOUSNESS, SEAL_COMMAND, SEAL_OF_LIGHT, SEAL_OF_WISDOM, SEAL_OF_CRUSADER })
        if (ctx.bot.has_aura(seal))
            return true;
    return false;
}

// Seal for the raid's judgement debuffs: the bot paladins that are not tanks, ordered by guid, take Wisdom (mana for the casters),
// Light (healing), then Crusader (melee only) - one judgement of each kind is enough, a second paladin on the same one is wasted.
// Raids only; every other paladin keeps its damage seal.
uint32 RaidJudgementSeal(ApPredicateContext const& ctx)
{
    auto const* members = ctx.group.members();
    if (!members || members->size() <= 5 || ctx.bot.my_role() == Role::Tank)
        return 0;
    thread_local std::vector<ObjectGuid> pals;
    pals.clear();
    for (auto const& m : *members)
        if (m.cls == CLASS_PALADIN && m.is_bot && m.online && m.is_alive && m.map_id == ctx.bot.map_id() && m.role != Role::Tank)
            pals.push_back(m.guid);
    std::sort(pals.begin(), pals.end());
    auto const it = std::find(pals.begin(), pals.end(), ctx.bot.raw().guid);
    if (it == pals.end())
        return 0;
    size_t const slot = size_t(it - pals.begin());
    uint32 const wanted = slot == 0 ? SEAL_OF_WISDOM : slot == 1 ? SEAL_OF_LIGHT : slot == 2 ? SEAL_OF_CRUSADER : 0;
    if (!wanted || !ctx.bot.knows_spell(wanted))
        return 0;
    // Crusader's judgement only helps melee: a healer paladin in the third slot keeps its damage seal.
    if (wanted == SEAL_OF_CRUSADER && ctx.bot.my_role() == Role::Healer)
        return 0;
    return wanted;
}

// Seal of Command (talent) when known, else Righteousness (level 1); in a raid the judgement plan above comes first.
uint32 DpsSeal(ApPredicateContext const& ctx)
{
    if (uint32 const planned = RaidJudgementSeal(ctx))
        return planned;
    return ctx.bot.knows_spell(SEAL_COMMAND) ? SEAL_COMMAND : SEAL_RIGHTEOUSNESS;
}

// Best direct heal for a "low" target: Holy Light, Flash of Light for fast.
uint32 FastHeal(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(FLASH_OF_LIGHT) ? FLASH_OF_LIGHT : HOLY_LIGHT;
}

// ======================= Shared =======================

bool ShouldDivineShield(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.hp_pct() <= 20 && Ready(ctx, DIVINE_SHIELD)
        && !ctx.bot.has_aura(FORBEARANCE);
}
void DoDivineShield(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIVINE_SHIELD); }

bool ShouldDivineProtection(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.hp_pct() <= 35 && Ready(ctx, DIVINE_PROTECTION)
        && !ctx.bot.has_aura(FORBEARANCE) && !Ready(ctx, DIVINE_SHIELD);
}
void DoDivineProtection(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIVINE_PROTECTION); }

bool ShouldSelfLayOnHands(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.hp_pct() <= 12 && Ready(ctx, LAY_ON_HANDS)
        && !ctx.bot.has_aura(FORBEARANCE);
}
void DoSelfLayOnHands(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LAY_ON_HANDS, ctx.bot.raw().guid);
}

bool ShouldResurrection(ApPredicateContext const& ctx)
{
    return !ctx.bot.in_combat() && Ready(ctx, REDEMPTION) && !ClassicHeal::OocRezTarget(ctx.bot, ctx.group).IsEmpty();
}
void DoResurrection(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (ObjectGuid const t = ClassicHeal::OocRezTarget(ctx.bot, ctx.group); !t.IsEmpty())
        e.cast(REDEMPTION, t);
}

// Aura upkeep (Classic::PickPaladinAura): paladins of a party hold distinct
// auras - Devotion where the party has a tank, the instance's resistance aura
// (Fire Resistance in Molten Core), then Concentration for a healer,
// Retribution / Sanctity for Retribution.
bool ShouldPaladinAura(ApPredicateContext const& ctx) { return Classic::PickPaladinAura(ctx.bot, ctx.group) != 0; }
void DoPaladinAura(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (uint32 const aura = Classic::PickPaladinAura(ctx.bot, ctx.group); aura && e.cast(aura))
        Classic::NoteRaidSupport(Classic::RaidSupportKind::Aura);
}

// Blessings: the group-wide assignment shared with idle:classic_blessings
// (Bot/ClassicUpkeep.cpp). In a fight only a missing blessing is recast.
bool ShouldBlessing(ApPredicateContext const& ctx)
{
    return Classic::PickBlessing(ctx.bot, ctx.group, true).spell != 0;
}
void DoBlessing(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    Classic::CastPick const b = Classic::PickBlessing(ctx.bot, ctx.group, true);
    e.cast(b.spell, b.target);
}

// Seal + Judgement, shared by tank and Retribution.
bool ShouldSeal(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && !HasAnySeal(ctx) && Ready(ctx, DpsSeal(ctx));
}
void DoSeal(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(DpsSeal(ctx)); }

bool ShouldJudgement(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && HasAnySeal(ctx) && Ready(ctx, JUDGEMENT) && DistToVictim(ctx) <= 9.f;
}
void DoJudgement(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(JUDGEMENT, ctx.bot.victim()); }

bool ShouldHammerOfJustice(ApPredicateContext const& ctx)
{
    // Stun an enemy that is casting (nothing better to interrupt with in 1.12).
    NearbyUnit const* v = ctx.bot.victim_info();
    return HasLiveTarget(ctx) && v && v->is_casting && v->is_interruptible && !(v->is_dungeon_boss || v->is_boss_rank)
        && (Ready(ctx, HAMMER_OF_JUSTICE) && ctx.bot.victim_in_spell_range(HAMMER_OF_JUSTICE)) && DistToVictim(ctx) <= 9.f;
}
void DoHammerOfJustice(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(HAMMER_OF_JUSTICE, ctx.bot.victim()); }

// ======================= Healer =======================

// Lay on Hands (1 h cooldown, drains all mana) is reserved for a tank at
// critical health (the owner's "party member critical tank health", 20%) and
// for the paladin itself; anyone else gets Flash of Light. Forbearance from
// any paladin (member debuff list) blocks it.
ObjectGuid LayOnHandsTarget(ApPredicateContext const& ctx)
{
    constexpr int32 kCritical = 20;
    if (auto const* members = ctx.group.members())
    {
        float bx, by, bz; ctx.bot.position(bx, by, bz);
        for (auto const& m : *members)
        {
            if (m.guid == ctx.bot.raw().guid || !ClassicHeal::IsTank(ctx.bot, m)) continue;
            if (!m.online || !m.is_alive || m.max_hp <= 0 || m.map_id != ctx.bot.map_id()) continue;
            if (ClassicHeal::Dist2(m.x, m.y, m.z, bx, by, bz) > 40.0f * 40.0f) continue;
            if (m.hp_pct() > kCritical) continue;
            if (TargetHasAura(ctx, FORBEARANCE, m.guid) || ClassicHeal::MemberHasDebuff(ctx.group, m.guid, FORBEARANCE)) continue;
            return m.guid;
        }
    }
    if (ctx.bot.hp_pct() <= kCritical && !ctx.bot.has_aura(FORBEARANCE))
        return ctx.bot.raw().guid;
    return ObjectGuid::Empty;
}
bool ShouldLayOnHands(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && Ready(ctx, LAY_ON_HANDS) && !LayOnHandsTarget(ctx).IsEmpty();
}
void DoLayOnHands(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(LAY_ON_HANDS, LayOnHandsTarget(ctx));
}

bool ShouldDivineFavor(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && LowestFriendOrSelf(ctx).hp_pct <= 40 && Ready(ctx, DIVINE_FAVOR)
        && !ctx.bot.has_aura(DIVINE_FAVOR);
}
void DoDivineFavor(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DIVINE_FAVOR); }

bool ShouldHolyShock(ApPredicateContext const& ctx)
{
    return Ready(ctx, HOLY_SHOCK) && LowestFriendOrSelf(ctx).hp_pct <= 55;
}
void DoHolyShock(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(HOLY_SHOCK, LowestFriendOrSelf(ctx).guid);
}

bool ShouldFlashCritical(ApPredicateContext const& ctx)
{
    return Ready(ctx, FLASH_OF_LIGHT) && LowestFriendOrSelf(ctx).hp_pct <= 45;
}
// Raid paladin heal choice: Flash of Light (by far the best heal per mana)
// on everyone but a tank under half health, which gets Holy Light
// (ClassicHeal::PlanHeal: top rank under pressure). Outside raids the
// rotation's own pick stands.
uint32 RaidPaladinHeal(ApPredicateContext const& ctx, ClassicHeal::Patient const& p, uint32 pick)
{
    if (!ClassicHeal::RaidHealing(ctx.bot))
        return pick;
    bool const tank_big = p.tank && p.actual_pct < ClassicHeal::kRaidTankBigHealPct;
    if (tank_big && ctx.bot.knows_spell(HOLY_LIGHT))
        return HOLY_LIGHT;
    if (!p.tank && ctx.bot.knows_spell(FLASH_OF_LIGHT))
        return FLASH_OF_LIGHT;
    return pick;
}

void DoFlashOfLight(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ClassicHeal::Patient const p = ClassicHeal::PickPatient(ctx);
    ClassicHeal::EmitHeal(ctx.bot, ctx.group, e, RaidPaladinHeal(ctx, p, FLASH_OF_LIGHT), p);
}

// Before Flash of Light (level 20) the emergency heal is Holy Light.
bool ShouldEmergencyHolyLight(ApPredicateContext const& ctx)
{
    return !ctx.bot.knows_spell(FLASH_OF_LIGHT) && Ready(ctx, HOLY_LIGHT) && LowestFriendOrSelf(ctx).hp_pct <= 40;
}

bool ShouldHolyLight(ApPredicateContext const& ctx)
{
    return Ready(ctx, HOLY_LIGHT) && LowestFriendOrSelf(ctx).hp_pct <= 65;
}
void DoHolyLight(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    ClassicHeal::Patient const p = ClassicHeal::PickPatient(ctx);
    ClassicHeal::EmitHeal(ctx.bot, ctx.group, e, RaidPaladinHeal(ctx, p, HOLY_LIGHT), p);
}

// Top-off with the cheap fast heal.
bool ShouldFlashTopOff(ApPredicateContext const& ctx)
{
    return Ready(ctx, FLASH_OF_LIGHT) && LowestFriendOrSelf(ctx).hp_pct <= 85 && ManaPct(ctx) >= 30;
}

// Cleanse / Purify on the member the raid dispel plan gives this paladin
// (Classic::PickDispel: one bot per debuffed member).
bool ShouldCleanse(ApPredicateContext const& ctx) { return ClassicHeal::ShouldAssignedDispel(ctx); }
void DoCleanse(ApPredicateContext const& ctx, BotIntentEmitter& e) { ClassicHeal::DoAssignedDispel(ctx, e); }

// In a raid only after a quiet spell at 90%+ mana (ClassicHeal::
// RaidHealerMayDamage: the five-second rule).
bool HealerMayDps(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ctx.bot.in_combat() && WoundedCount(ctx, 90) == 0 && LowestFriendOrSelf(ctx).hp_pct > 90
        && ClassicHeal::RaidHealerMayDamage(ctx.bot, ctx.group);
}

bool ShouldHealerSeal(ApPredicateContext const& ctx)
{
    return HealerMayDps(ctx) && ManaPct(ctx) >= 60 && DistToVictim(ctx) <= 9.f && ShouldSeal(ctx);   // out of judgement reach the seal is wasted mana
}

bool ShouldHealerJudgement(ApPredicateContext const& ctx)
{
    return HealerMayDps(ctx) && ManaPct(ctx) >= 60 && ShouldJudgement(ctx);
}

ApRule const kHealerRules[] = {
    { ShouldDivineShield,        DoDivineShield,        "classic_pal_heal:divine_shield" },
    { ShouldLayOnHands,          DoLayOnHands,          "classic_pal_heal:lay_on_hands" },
    { ShouldDivineFavor,         DoDivineFavor,         "classic_pal_heal:divine_favor" },
    { ShouldHolyShock,           DoHolyShock,           "classic_pal_heal:holy_shock" },
    { ShouldFlashCritical,       DoFlashOfLight,        "classic_pal_heal:flash_of_light" },
    { ShouldEmergencyHolyLight,  DoHolyLight,           "classic_pal_heal:emergency_holy_light" },
    { ShouldHolyLight,           DoHolyLight,           "classic_pal_heal:holy_light" },
    { ShouldCleanse,             DoCleanse,             "classic_pal_heal:cleanse" },
    { ShouldFlashTopOff,         DoFlashOfLight,        "classic_pal_heal:flash_top_off" },
    { ShouldResurrection,        DoResurrection,        "classic_pal_heal:redemption" },
    { ShouldPaladinAura,         DoPaladinAura,         "classic_pal_heal:aura" },
    { ShouldBlessing,            DoBlessing,            "classic_pal_heal:blessing" },
    { ShouldHealerSeal,          DoSeal,                "classic_pal_heal:seal" },
    { ShouldHealerJudgement,     DoJudgement,           "classic_pal_heal:judgement" },
};

// ======================= Protection =======================

bool ShouldRighteousFury(ApPredicateContext const& ctx)
{
    return Ready(ctx, RIGHTEOUS_FURY) && !ctx.bot.has_aura(RIGHTEOUS_FURY);
}
void DoRighteousFury(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(RIGHTEOUS_FURY); }

bool ShouldHolyShield(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.attackers_count() >= 1 && ManaPct(ctx) >= 10
        && Ready(ctx, HOLY_SHIELD) && !ctx.bot.has_aura(HOLY_SHIELD);
}
void DoHolyShield(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HOLY_SHIELD); }

// Consecration: the paladin tank's pack threat. Never over a CC'd / unpulled
// mob (AoeSafe) or one another tank holds close to its threat
// (TankThreat::AoeRipsOtherTank).
bool ShouldConsecration(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.enemies_within(8.0f) >= 2 && ManaPct(ctx) >= 30
        && Ready(ctx, CONSECRATION)
        && AoeSafeAroundSelf(ctx.bot, 8.0f, /*damaging*/ true, /*tank*/ true, "Consecration", CONSECRATION)
        && !TankThreat::AoeRipsOtherTank(ctx.bot, 8.0f, "Consecration");
}
void DoConsecration(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CONSECRATION); }

// Single-target Consecration as a threat filler once mana is comfortable.
bool ShouldConsecrationSingle(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.enemies_within(8.0f) >= 1 && ManaPct(ctx) >= 60
        && Ready(ctx, CONSECRATION)
        && AoeSafeAroundSelf(ctx.bot, 8.0f, /*damaging*/ true, /*tank*/ true, "Consecration", CONSECRATION);
}

bool ShouldTankSeal(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && !HasAnySeal(ctx) && ManaPct(ctx) >= 15 && Ready(ctx, DpsSeal(ctx));
}

bool ShouldTankJudgement(ApPredicateContext const& ctx)
{
    return ManaPct(ctx) >= 15 && ShouldJudgement(ctx);
}

// ---- Loose mobs and pack threat (no taunt in vanilla) ----
// Pick-up: the raid plan's mob for this tank, else (once the current victim
// is on us / another tank) the nearest loose attacker no other tank was
// given - the same pick as the warrior / bear rule, without the taunt.
bool PaladinVictimSecured(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return true;
    ThreatVictim const k = ThreatVictim(v->victim_kind);
    return v->hp <= 0 || k == ThreatVictim::Me || k == ThreatVictim::Tank;
}
bool ShouldPaladinPickUpLoose(ApPredicateContext const& ctx)
{
    if (!ctx.bot.in_combat()) return false;
    if (!ctx.bot.assigned_tank_mob() && !PaladinVictimSecured(ctx)) return false;
    NearbyUnit const* t = ctx.bot.pickup_candidate();
    return t && t->guid != ctx.bot.victim();
}
void DoPaladinPickUpLoose(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (auto const* t = ctx.bot.pickup_candidate())
        e.start_attack(t->guid);
}

// Judgement (10 yd) on the pack mob that needs threat most once the victim
// has its lead (TankThreat::SpreadTarget, a loose mob first).
NearbyUnit const* PaladinSpreadTarget(ApPredicateContext const& ctx)
{
    return TankThreat::SpreadTarget(ctx.bot, 9.0f);
}
bool ShouldTankSpreadJudgement(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 15 && HasAnySeal(ctx) && Ready(ctx, JUDGEMENT)
        && PaladinSpreadTarget(ctx) != nullptr;
}
void DoTankSpreadJudgement(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    if (NearbyUnit const* t = PaladinSpreadTarget(ctx))
        if (e.cast(JUDGEMENT, t->guid))
            TankThreat::LogSpread(ctx.bot, "Judgement", *t);
}

ApRule const kTankRules[] = {
    { ShouldDivineShield,        DoDivineShield,        "classic_pal_tank:divine_shield" },
    { ShouldDivineProtection,    DoDivineProtection,    "classic_pal_tank:divine_protection" },
    { ShouldSelfLayOnHands,      DoSelfLayOnHands,      "classic_pal_tank:lay_on_hands" },
    { ShouldRighteousFury,       DoRighteousFury,       "classic_pal_tank:righteous_fury" },
    { ShouldPaladinAura,         DoPaladinAura,         "classic_pal_tank:aura" },
    { ShouldHammerOfJustice,     DoHammerOfJustice,     "classic_pal_tank:hammer_of_justice" },
    { ShouldHolyShield,          DoHolyShield,          "classic_pal_tank:holy_shield" },
    { ShouldPaladinPickUpLoose,  DoPaladinPickUpLoose,  "classic_pal_tank:pick_up_loose" },
    { ShouldTankSeal,            DoSeal,                "classic_pal_tank:seal" },
    { ShouldTankSpreadJudgement, DoTankSpreadJudgement, "classic_pal_tank:spread_judgement" },
    { ShouldTankJudgement,       DoJudgement,           "classic_pal_tank:judgement" },
    { ShouldConsecration,        DoConsecration,        "classic_pal_tank:consecration" },
    { ShouldConsecrationSingle,  DoConsecration,        "classic_pal_tank:consecration_single" },
    { ShouldBlessing,            DoBlessing,            "classic_pal_tank:blessing" },
    { ShouldResurrection,        DoResurrection,        "classic_pal_tank:redemption" },
    { HasLiveTarget,             DoAutoAttack,          "classic_pal_tank:auto_attack" },
};

// ======================= Retribution =======================

bool ShouldRetSeal(ApPredicateContext const& ctx)
{
    return ShouldSeal(ctx) && ManaPct(ctx) >= 5;
}

bool ShouldRetJudgement(ApPredicateContext const& ctx)
{
    return ManaPct(ctx) >= 5 && ShouldJudgement(ctx);
}

bool ShouldRetHammerOfWrath(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && VictimHpPct(ctx) <= 20 && ManaPct(ctx) >= 10 && (Ready(ctx, HAMMER_OF_WRATH) && ctx.bot.victim_in_spell_range(HAMMER_OF_WRATH));
}
void DoHammerOfWrath(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(HAMMER_OF_WRATH, ctx.bot.victim()); }

bool ShouldRetSelfHeal(ApPredicateContext const& ctx)
{
    return ctx.bot.hp_pct() <= 35 && ManaPct(ctx) >= 15 && Ready(ctx, FastHeal(ctx));
}
void DoRetSelfHeal(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(FastHeal(ctx), ctx.bot.raw().guid);
}

bool ShouldRetConsecration(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.enemies_within(8.0f) >= 3 && ManaPct(ctx) >= 40
        && Ready(ctx, CONSECRATION)
        // Ret: not over a sheep, an unpulled mob or mobs the tank has not secured.
        && AoeSafeAroundSelf(ctx.bot, 8.0f, /*damaging*/ true, /*tank*/ false, "Consecration", CONSECRATION);
}

ApRule const kRetRules[] = {
    { ShouldDivineShield,        DoDivineShield,        "classic_pal_ret:divine_shield" },
    { ShouldSelfLayOnHands,      DoSelfLayOnHands,      "classic_pal_ret:lay_on_hands" },
    { ShouldRetSelfHeal,         DoRetSelfHeal,         "classic_pal_ret:self_heal" },
    { ShouldPaladinAura,         DoPaladinAura,         "classic_pal_ret:aura" },
    { ShouldBlessing,            DoBlessing,            "classic_pal_ret:blessing" },
    { ShouldCleanse,             DoCleanse,             "classic_pal_ret:cleanse" },
    { ShouldHammerOfJustice,     DoHammerOfJustice,     "classic_pal_ret:hammer_of_justice" },
    { ShouldRetHammerOfWrath,    DoHammerOfWrath,       "classic_pal_ret:hammer_of_wrath" },
    { ShouldRetSeal,             DoSeal,                "classic_pal_ret:seal" },
    { ShouldRetJudgement,        DoJudgement,           "classic_pal_ret:judgement" },
    { ShouldRetConsecration,     DoConsecration,        "classic_pal_ret:consecration" },
    { ShouldResurrection,        DoResurrection,        "classic_pal_ret:redemption" },
    { HasLiveTarget,             DoAutoAttack,          "classic_pal_ret:auto_attack" },
};

} // namespace

void RegisterApl_Classic_Paladin()
{
    ApRotation const healer{ std::span<ApRule const>(kHealerRules) };
    ApRotation const tank{ std::span<ApRule const>(kTankRules) };
    ApRotation const ret{ std::span<ApRule const>(kRetRules) };
    RegisterRotationOverride(CLASS_PALADIN, 0, ret);
    RegisterRotationOverride(CLASS_PALADIN, SPEC_HOLY, healer);
    RegisterRotationOverride(CLASS_PALADIN, SPEC_PROT, tank);
    RegisterRotationOverride(CLASS_PALADIN, SPEC_RET, ret);
}

} // namespace Playerbot::Combat
