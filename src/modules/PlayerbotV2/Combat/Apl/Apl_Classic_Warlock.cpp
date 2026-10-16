// Classic 1.60 (Forever) Warlock - vanilla rotation.
//
// Registered over the retail Affliction / Demonology / Destruction rotations
// and the warlock baseline (spec 0). One rotation serves all specs; talent
// spells (Siphon Life, Conflagrate, Shadowburn) are skipped automatically
// when not trained. Spell ids are vanilla rank 1 and resolve to the highest
// known rank.
//
// Pets: the view exposes has_pet()/pet_victim() and the emitter pet_attack(),
// so the rotation summons a pet out of combat (solo: Voidwalker once known,
// else Imp; dungeon / raid group: Felhunter for the designated Spell Lock
// warlocks, Imp for the rest) and points it at the victim in combat.
//
// Priorities follow Karrz's ike3 vanilla strategies (Karrz1/wow-1.12
// raid10-revamp, playerbot/strategy/warlock/*Strategy.cpp, MANGOSBOT_ZERO):
// armor + pet upkeep, emergency Death Coil / Howl of Terror / Drain Life,
// Life Tap at low mana, dots (Corruption, Curse of Agony, Immolate, Siphon
// Life), Conflagrate / Shadowburn, Shadow Bolt, wand when dry.
// Banish on a demon / elemental add (PickGroupCC).
// Raids (Classic): one curse per warlock from the raid plan (PveOrder::
// curse_spell, PveGroupCoordinator::PlanCurses: Elements, Shadow,
// Recklessness, the rest Agony) on the boss / kill focus, and the dots capped
// at Corruption + Immolate (+ that curse) - 40 players share the 16 debuff
// slots of a vanilla mob. Solo and 5-man keep Agony and Siphon Life.
// Shadow Trance instant bolt and Drain Soul on a dying target (shard top-up) come from the VMaNGOS bots.
// Skipped: Fear, Fel Domination (Soulstone / Healthstone live in
// the upkeep rules).

#include "AplPetThrottle.h"
#include "Bot/SpellFamily.h"
#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Apl_Baseline_Common.h"
#include "ApCrowdControl.h"
#include "ApSchoolHint.h"
#include "ApInterrupt.h"
#include "Bot/ThreatDiscipline.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"
#include "SpellMgr.h"

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::DoAutoAttack;
using ::Playerbot::Combat::baseline_common::HasLiveTarget;

// ---- Vanilla spell ids (rank 1) ----
constexpr uint32 SHADOW_BOLT        = 686;
constexpr uint32 SEARING_PAIN       = 5676;    // fire filler (raid school rule only)
constexpr uint32 CORRUPTION         = 172;
constexpr uint32 CURSE_OF_AGONY     = 980;
constexpr uint32 IMMOLATE           = 348;
constexpr uint32 LIFE_TAP           = 1454;
constexpr uint32 DRAIN_LIFE         = 689;
constexpr uint32 DEATH_COIL         = 6789;
constexpr uint32 HOWL_OF_TERROR     = 5484;
constexpr uint32 DEMON_SKIN         = 687;
constexpr uint32 DEMON_ARMOR        = 706;
constexpr uint32 SIPHON_LIFE        = 18265;   // Affliction talent
constexpr uint32 CONFLAGRATE        = 17962;   // Destruction talent
constexpr uint32 SHADOWBURN         = 17877;   // Destruction talent
constexpr uint32 SHOOT_WAND         = 5019;
constexpr float  kCurseRange        = 30.0f;

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
    return v && ((v->is_dungeon_boss || v->is_boss_rank) || v->max_hp >= 100'000);
}

// The mana the filler nukes stop at: a warlock with a wand shoots below 35% on ordinary fights, and casts down to 8% against a boss or
// when it holds no wand.
int32 NukeManaFloor(ApPredicateContext const& ctx)
{
    return ctx.bot.raw().consumables.wand_school_mask != 0 && !VictimIsBossLike(ctx) ? 35 : 8;
}

int32 VictimHpPct(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v || v->max_hp <= 0) return 100;
    return int32(int64(v->hp) * 100 / v->max_hp);
}

// Do not sink dots into a mob that is about to die (bosses always get them).
bool DotWorthwhile(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 10 && (VictimIsBossLike(ctx) || VictimHpPct(ctx) >= 35);
}

uint32 ArmorSpell(ApPredicateContext const& ctx)
{
    return ctx.bot.knows_spell(DEMON_ARMOR) ? DEMON_ARMOR : DEMON_SKIN;
}

// Solo: Voidwalker once known, else Imp. Dungeon / raid group: the group's
// designated Felhunter warlocks (one per subgroup, at most two per raid, by
// guid - Spell Lock interrupts) summon the Felhunter, the rest the Imp
// (Interrupt::ClassicWarlockPet).
uint32 PetSpell(ApPredicateContext const& ctx)
{
    return Interrupt::ClassicWarlockPet(ctx.bot, &ctx.group);
}

// ======================= Rules =======================

bool ShouldDeathCoil(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.attackers_count() >= 1 && ctx.bot.hp_pct() <= 30 && HasLiveTarget(ctx)
        && (Ready(ctx, DEATH_COIL) && ctx.bot.victim_in_spell_range(DEATH_COIL));
}
void DoDeathCoil(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(DEATH_COIL, ctx.bot.victim()); }

// Fear is ignored by bosses, so only against ordinary packs.
bool ShouldHowlOfTerror(ApPredicateContext const& ctx)
{
    if (!(ctx.bot.in_combat() && ctx.bot.hp_pct() <= 40 && ctx.bot.enemies_within(8.0f) >= 2
          && !VictimIsBossLike(ctx) && Ready(ctx, HOWL_OF_TERROR)))
        return false;
    // Raid school rule: not into a mob that avoids shadow (only that veto -
    // this is the emergency fear, the threat-discipline gates do not apply).
    float x, y, z; ctx.bot.position(x, y, z);
    // A crowd-controlled mob in the ring vetoes it too (as Frost Nova's does).
    AoeHazardKind const k = FindAoeHazard(ctx.bot, x, y, z, 10.0f, /*damaging*/ true, /*tank*/ false, HOWL_OF_TERROR).kind;
    if (k == AoeHazardKind::SchoolAvoided || k == AoeHazardKind::CrowdControlled)
        return false;
    // Raid groups and instances: no fear with an unpulled pack within 40y; raids
    // also only for a warlock an elite is hitting (ThreatDiscipline.h RaidFearAllowed).
    return RaidFearAllowed(ctx.bot, ctx.group.is_raid() || ctx.bot.is_in_raid(), "Howl of Terror");
}
void DoHowlOfTerror(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(HOWL_OF_TERROR); }

// Felhunter Spell Lock, cast by the pet on the interrupt rotation's pick
// (ApInterrupt.h: per-mob assignment / rank windows, 30y).
bool ShouldSpellLock(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && Interrupt::Should(ctx, 0) && Interrupt::ClassicAbility(ctx.bot).pet;
}
void DoSpellLock(ApPredicateContext const& ctx, BotIntentEmitter& e) { Interrupt::Cast(ctx, e, 0); }

bool ShouldDrainLife(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ctx.bot.in_combat() && ctx.bot.hp_pct() <= 35 && ManaPct(ctx) >= 10
        && (Ready(ctx, DRAIN_LIFE) && ctx.bot.victim_in_spell_range(DRAIN_LIFE));
}
void DoDrainLife(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(DRAIN_LIFE, ctx.bot.victim()); }

bool ShouldLifeTap(ApPredicateContext const& ctx)
{
    return ManaPct(ctx) <= 25 && ctx.bot.hp_pct() >= 60 && Ready(ctx, LIFE_TAP);
}
void DoLifeTap(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(LIFE_TAP); }

bool ShouldArmor(ApPredicateContext const& ctx)
{
    uint32 const spell = ArmorSpell(ctx);
    return Ready(ctx, spell) && !ctx.bot.has_aura(DEMON_ARMOR) && !ctx.bot.has_aura(DEMON_SKIN);
}
void DoArmor(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(ArmorSpell(ctx)); }

bool ShouldSummonPet(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat() || ctx.bot.current_cast_spell_id() != 0) return false;
    uint32 const want = PetSpell(ctx);
    if (!want || !Ready(ctx, want)) return false;
    // Out of combat with the wrong demon out (joined a raid with the
    // Voidwalker, or this warlock is a Felhunter warlock now): replace it -
    // only with the soul shard the summon costs in the bags, or the refused
    // cast repeats every tick while the old demon stays out.
    if (!ctx.bot.has_pet())
        return true;
    return ctx.bot.raw().pet.pet_family != Interrupt::SummonPetFamily(want) && ctx.bot.has_reagents(want);
}
void DoSummonPet(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(PetSpell(ctx)); }

bool ShouldPetAttack(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ctx.bot.in_combat() && ctx.bot.has_pet() && ctx.bot.pet_victim() != ctx.bot.victim()
        && Playerbot::AplPet::PetAttackDue(ctx.bot.guid().GetCounter(), ctx.bot.published_at_ms());
}
void DoPetAttack(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    Playerbot::AplPet::NotePetAttack(ctx.bot.guid().GetCounter(), ctx.bot.published_at_ms());
    e.pet_attack(ctx.bot.victim());
}

// A raid group: the curse comes from the raid plan and the dots are capped.
bool InRaid(ApPredicateContext const& ctx)
{
    return ctx.group.is_raid() || ctx.bot.is_in_raid();
}

// The raid plan's curse target when it is not the victim: in sight and in
// curse range.
NearbyUnit const* CurseTargetInReach(ApPredicateContext const& ctx, ObjectGuid target)
{
    float x, y, z; ctx.bot.position(x, y, z);
    for (NearbyUnit const& u : ctx.bot.nearby_enemies())
    {
        if (u.guid != target) continue;
        if (u.hp <= 0 || !u.in_los || u.untargetable) return nullptr;
        float const dx = u.x - x, dy = u.y - y, dz = u.z - z;
        return dx * dx + dy * dy + dz * dz <= kCurseRange * kCurseRange ? &u : nullptr;
    }
    return nullptr;
}

// Another caster's copy of a one-per-target curse (Elements, Shadow,
// Recklessness) is on the victim: a human warlock got there first.
bool VictimCurseFromOther(ApPredicateContext const& ctx, uint32 curse)
{
    auto root = [](uint32 id) { return SpellFamily::Root(id); };
    uint32 const want = root(curse);
    for (AuraEntry const& a : ctx.bot.raw().auras.victim_auras)
        if (a.spell_id && a.caster != ctx.bot.guid() && root(a.spell_id) == want)
            return true;
    return false;
}

// Raid curse (PveOrder::curse_spell). On the victim the bot's own aura view
// decides; on another target (the boss while this warlock burns an add) the
// plan's curse_needed does - the coordinator read the mob's auras.
bool ShouldRaidCurse(ApPredicateContext const& ctx)
{
    PveOrder const& po = ctx.bot.raw().pve_order;
    if (!po.active || !po.curse_spell || po.curse_target.IsEmpty() || !ctx.bot.in_combat())
        return false;
    if (ManaPct(ctx) < 10 || !Ready(ctx, po.curse_spell))
        return false;
    if (po.curse_target == ctx.bot.victim())
    {
        if (!HasLiveTarget(ctx) || ctx.bot.has_aura(po.curse_spell, ctx.bot.victim()))
            return false;
        return po.curse_spell == CURSE_OF_AGONY || !VictimCurseFromOther(ctx, po.curse_spell);
    }
    return po.curse_needed && CurseTargetInReach(ctx, po.curse_target) != nullptr;
}
void DoRaidCurse(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    PveOrder const& po = ctx.bot.raw().pve_order;
    e.cast(po.curse_spell, po.curse_target);
}

bool ShouldCorruption(ApPredicateContext const& ctx)
{
    return DotWorthwhile(ctx) && (Ready(ctx, CORRUPTION) && ctx.bot.victim_in_spell_range(CORRUPTION)) && !ctx.bot.has_aura(CORRUPTION, ctx.bot.victim());
}
void DoCorruption(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CORRUPTION, ctx.bot.victim()); }

bool ShouldCurseOfAgony(ApPredicateContext const& ctx)
{
    // Raid: the plan's curse (ShouldRaidCurse) is this warlock's only curse.
    return !InRaid(ctx) && DotWorthwhile(ctx) && (Ready(ctx, CURSE_OF_AGONY) && ctx.bot.victim_in_spell_range(CURSE_OF_AGONY)) && !ctx.bot.has_aura(CURSE_OF_AGONY, ctx.bot.victim());
}
void DoCurseOfAgony(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CURSE_OF_AGONY, ctx.bot.victim()); }

bool ShouldImmolate(ApPredicateContext const& ctx)
{
    return DotWorthwhile(ctx) && (Ready(ctx, IMMOLATE) && ctx.bot.victim_in_spell_range(IMMOLATE)) && !ctx.bot.has_aura(IMMOLATE, ctx.bot.victim());
}
void DoImmolate(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(IMMOLATE, ctx.bot.victim()); }

bool ShouldSiphonLife(ApPredicateContext const& ctx)
{
    // Raid: dots capped at Corruption + Immolate (+ the plan's curse).
    return !InRaid(ctx) && DotWorthwhile(ctx) && (Ready(ctx, SIPHON_LIFE) && ctx.bot.victim_in_spell_range(SIPHON_LIFE)) && !ctx.bot.has_aura(SIPHON_LIFE, ctx.bot.victim());
}
void DoSiphonLife(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SIPHON_LIFE, ctx.bot.victim()); }

// Conflagrate consumes Immolate, so it needs it up on the target.
bool ShouldConflagrate(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 10 && (Ready(ctx, CONFLAGRATE) && ctx.bot.victim_in_spell_range(CONFLAGRATE))
        && ctx.bot.has_aura(IMMOLATE, ctx.bot.victim());
}
void DoConflagrate(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CONFLAGRATE, ctx.bot.victim()); }

// Shadowburn costs a soul shard: execute-range finisher only, and never the last two (Healthstone / Soulstone / summons need them).
bool ShouldShadowburn(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= 10 && VictimHpPct(ctx) <= 20 && ctx.bot.item_count(6265) >= 3 && (Ready(ctx, SHADOWBURN) && ctx.bot.victim_in_spell_range(SHADOWBURN));
}
void DoShadowburn(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SHADOWBURN, ctx.bot.victim()); }

// Shadow Trance (Nightfall proc, aura 17941): the next Shadow Bolt is instant, so it goes first, before the dots.
// Priority idea from Karrz's VMaNGOS bots (warlock/WarlockStrategy.cpp, "shadow trance" trigger); rewritten here.
constexpr uint32 SHADOW_TRANCE_AURA = 17941;
bool ShouldShadowTrance(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ctx.bot.has_aura(SHADOW_TRANCE_AURA) && ManaPct(ctx) >= 8
        && Ready(ctx, SHADOW_BOLT) && ctx.bot.victim_in_spell_range(SHADOW_BOLT);
}

// Drain Soul on a nearly dead target while the shard bag is low (VMaNGOS "drain soul" action). Solo and 5-man only: in a
// raid the channel is a free DPS loss against the boss and shards are stocked by other means.
constexpr uint32 DRAIN_SOUL = 1120;
bool ShouldDrainSoul(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && !ctx.bot.is_in_raid() && !VictimIsBossLike(ctx) && VictimHpPct(ctx) <= 25
        && ctx.bot.item_count(6265) < 6 && ManaPct(ctx) >= 10 && Ready(ctx, DRAIN_SOUL) && ctx.bot.victim_in_spell_range(DRAIN_SOUL);
}
void DoDrainSoul(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(DRAIN_SOUL, ctx.bot.victim()); }

bool ShouldShadowBolt(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= NukeManaFloor(ctx) && (Ready(ctx, SHADOW_BOLT) && ctx.bot.victim_in_spell_range(SHADOW_BOLT));
}
void DoShadowBolt(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SHADOW_BOLT, ctx.bot.victim()); }

// Raid school rule (ApSchoolHint.h): with a preferred school on the victim,
// its spells go first among the dots and nukes - shadow: Corruption / Curse of
// Agony / Siphon Life / Shadowburn / Shadow Bolt; fire: Immolate / Conflagrate
// / Searing Pain (the fire filler, Shadow Bolt's mana gate).
bool ShouldSearingPain(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ManaPct(ctx) >= NukeManaFloor(ctx) && Ready(ctx, SEARING_PAIN);
}

SchoolHint::SchoolChoice const kSchoolChoices[] = {
    { CORRUPTION,     ShouldCorruption },
    { CURSE_OF_AGONY, ShouldCurseOfAgony },
    { SIPHON_LIFE,    ShouldSiphonLife },
    { IMMOLATE,       ShouldImmolate },
    { CONFLAGRATE,    ShouldConflagrate },
    { SHADOWBURN,     ShouldShadowburn },
    { SHADOW_BOLT,    ShouldShadowBolt },
    { SEARING_PAIN,   ShouldSearingPain },
};
bool ShouldSchoolNuke(ApPredicateContext const& ctx) { return SchoolHint::ShouldCast(ctx, kSchoolChoices); }
void DoSchoolNuke(ApPredicateContext const& ctx, BotIntentEmitter& e) { SchoolHint::DoCast(ctx, e, kSchoolChoices); }

bool ShouldWand(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && (Ready(ctx, SHOOT_WAND) && ctx.bot.victim_in_spell_range(SHOOT_WAND)) && ctx.bot.current_cast_spell_id() == 0;
}
void DoWand(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SHOOT_WAND, ctx.bot.victim()); }

// Banish: Demons / Elementals. In a group an add of a 3+ mob pull (never the kill target,
// the tank's target or a boss); solo an extra attacker. See PickGroupCC.
constexpr uint32 BANISH = 710;
bool ShouldCrowdControl(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ctx.bot.in_combat() && !ctx.bot.is_moving() && ManaPct(ctx) >= 15
        && Ready(ctx, BANISH) && !PickGroupCC(ctx, BANISH).IsEmpty();
}
void DoCrowdControl(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BANISH, PickGroupCC(ctx, BANISH));
}

ApRule const kWarlockRules[] = {
    { ShouldDeathCoil,      DoDeathCoil,      "classic_warlock:death_coil" },
    { ShouldHowlOfTerror,   DoHowlOfTerror,   "classic_warlock:howl_of_terror" },
    { ShouldSpellLock,      DoSpellLock,      "classic_warlock:spell_lock" },
    { ShouldCrowdControl,   DoCrowdControl,   "classic_warlock:banish" },
    { ShouldDrainLife,      DoDrainLife,      "classic_warlock:drain_life" },
    { ShouldLifeTap,        DoLifeTap,        "classic_warlock:life_tap" },
    { ShouldArmor,          DoArmor,          "classic_warlock:armor" },
    { ShouldSummonPet,      DoSummonPet,      "classic_warlock:summon_pet" },
    { ShouldPetAttack,      DoPetAttack,      "classic_warlock:pet_attack" },
    { ShouldRaidCurse,      DoRaidCurse,      "classic_warlock:raid_curse" },
    { ShouldShadowTrance,   DoShadowBolt,     "classic_warlock:shadow_trance" },
    { ShouldSchoolNuke,     DoSchoolNuke,     "classic_warlock:school_nuke" },
    { ShouldCorruption,     DoCorruption,     "classic_warlock:corruption" },
    { ShouldCurseOfAgony,   DoCurseOfAgony,   "classic_warlock:curse_of_agony" },
    { ShouldSiphonLife,     DoSiphonLife,     "classic_warlock:siphon_life" },
    { ShouldImmolate,       DoImmolate,       "classic_warlock:immolate" },
    { ShouldConflagrate,    DoConflagrate,    "classic_warlock:conflagrate" },
    { ShouldShadowburn,     DoShadowburn,     "classic_warlock:shadowburn" },
    { ShouldDrainSoul,      DoDrainSoul,      "classic_warlock:drain_soul" },
    { ShouldShadowBolt,     DoShadowBolt,     "classic_warlock:shadow_bolt" },
    { ShouldWand,           DoWand,           "classic_warlock:wand" },
    { HasLiveTarget,        DoAutoAttack,     "classic_warlock:auto_attack" },
};

} // namespace

void RegisterApl_Classic_Warlock()
{
    ApRotation const warlock{ std::span<ApRule const>(kWarlockRules) };
    RegisterRotationOverride(CLASS_WARLOCK, 0, warlock);
    RegisterRotationOverride(CLASS_WARLOCK, 265, warlock);
    RegisterRotationOverride(CLASS_WARLOCK, 266, warlock);
    RegisterRotationOverride(CLASS_WARLOCK, 267, warlock);
}

} // namespace Playerbot::Combat
