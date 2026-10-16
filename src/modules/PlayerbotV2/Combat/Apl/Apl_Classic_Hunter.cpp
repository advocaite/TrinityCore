// Classic 1.60 (Forever) Hunter - vanilla rotation.
//
// Registered over the retail BM / MM / Survival rotations and the hunter
// baseline (spec 0): one ranged rotation for every hunter spec.
//
// Spell ids are vanilla rank 1; the snapshot view and API::cast_spell resolve
// them to the highest rank the bot knows. Auto Shot is armed by
// API::start_attack (see DoAutoAttack, last rule), not cast here.
//
// Pet handling: the emitter exposes pet_attack and the view exposes
// has_pet / pet_victim / pet_hp_pct, so the pet is sent at the victim,
// Mend Pet / Revive Pet / Call Pet are cast. Pet growl / feeding are skipped.
// Freezing Trap goes down before a pull of a pack (1.12 traps can't be set
// in combat), so the first add to reach the hunter freezes.
// Skipped: Feign Death, Scatter Shot, Viper Sting, aspect swapping
// (Monkey / Cheetah / Viper) - Hawk is the only aspect maintained.
//
// Priorities follow Karrz's ike3 vanilla strategies (Karrz1/wow-1.12
// raid10-revamp, playerbot/strategy/hunter/*Strategy.cpp, MANGOSBOT_ZERO).

#include "Bot/SpellFamily.h"
#include "AplPetThrottle.h"
#include "../ApRegistry.h"
#include "../ApRotation.h"
#include "Apl_Baseline_Common.h"
#include "ApSchoolHint.h"
#include "ApInterrupt.h"   // EngagedWithGroup
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/ClassicUpkeep.h"   // Classic::BossFight, NoteCooldownUse
#include "Bot/ThreatDiscipline.h"
#include "Group/GroupSnapshot.h"
#include "SharedDefines.h"
#include <cmath>

namespace Playerbot::Combat {

namespace {

using ::Playerbot::Combat::baseline_common::DoAutoAttack;
using ::Playerbot::Combat::baseline_common::HasLiveTarget;

// ---- Vanilla spell ids (rank 1) ----
constexpr uint32 ASPECT_OF_THE_HAWK = 13165;
constexpr uint32 HUNTERS_MARK       = 1130;
constexpr uint32 SERPENT_STING      = 1978;
constexpr uint32 ARCANE_SHOT        = 3044;
constexpr uint32 MULTI_SHOT         = 2643;
constexpr uint32 AIMED_SHOT         = 19434;   // Marksmanship talent
constexpr uint32 RAPTOR_STRIKE      = 2973;
constexpr uint32 MONGOOSE_BITE      = 1495;    // reactive (after a dodge)
constexpr uint32 WING_CLIP          = 2974;
constexpr uint32 CONCUSSIVE_SHOT    = 5116;
constexpr uint32 RAPID_FIRE         = 3045;
constexpr uint32 BESTIAL_WRATH      = 19574;   // Beast Mastery talent
constexpr uint32 TRUESHOT_AURA      = 19506;   // Marksmanship talent
constexpr uint32 DETERRENCE         = 19263;   // Survival talent
constexpr uint32 MEND_PET           = 136;
constexpr uint32 REVIVE_PET         = 982;
constexpr uint32 CALL_PET           = 883;

constexpr uint8 POWER_MANA = 0;

float DistToVictim(ApPredicateContext const& ctx)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return 1000.f;
    auto const& me = ctx.bot.raw().position;
    float const dx = v->x - me.x, dy = v->y - me.y, dz = v->z - me.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool Ready(ApPredicateContext const& ctx, uint32 spell)
{
    return ctx.bot.knows_spell(spell) && ctx.bot.is_ready(spell);
}

int32 Mana(ApPredicateContext const& ctx) { return ctx.bot.power(POWER_MANA); }

bool VictimInMelee(ApPredicateContext const& ctx) { return DistToVictim(ctx) <= 5.5f; }
// Vanilla shots have an 8 yard minimum range.
// A shot needs a ranged weapon with ammo in the bags: without it the core refuses every one with NO_AMMO (Deadmines 2026-10-14).
bool VictimInShotRange(ApPredicateContext const& ctx)
{
    if (ctx.bot.ammo_kind() == 0 || ctx.bot.ammo_count() == 0)
        return false;
    float const d = DistToVictim(ctx);
    return d >= 8.f && d <= 35.f;
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

// ======================= Upkeep / pet =======================

bool ShouldAspectHawk(ApPredicateContext const& ctx)
{
    return Ready(ctx, ASPECT_OF_THE_HAWK) && !ctx.bot.has_aura(ASPECT_OF_THE_HAWK);
}
void DoAspectHawk(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(ASPECT_OF_THE_HAWK); }

bool ShouldTrueshotAura(ApPredicateContext const& ctx)
{
    return Ready(ctx, TRUESHOT_AURA) && !ctx.bot.has_aura(TRUESHOT_AURA);
}
void DoTrueshotAura(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(TRUESHOT_AURA); }

bool ShouldCallPet(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat() || !ctx.bot.pet_guid().IsEmpty()) return false;
    if (!Ready(ctx, CALL_PET)) return false;
    for (auto const& sp : ctx.bot.stable_pets())
        if (sp.slot_kind == 0 && sp.creature_id != 0)
            return true;
    return false;
}
void DoCallPet(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(CALL_PET); }

bool ShouldRevivePet(ApPredicateContext const& ctx)
{
    // Only when a pet existed and died; an empty guid means never tamed.
    return !ctx.bot.pet_guid().IsEmpty() && !ctx.bot.has_pet() && Ready(ctx, REVIVE_PET);
}
void DoRevivePet(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(REVIVE_PET); }

bool ShouldMendPet(ApPredicateContext const& ctx)
{
    return ctx.bot.has_pet() && ctx.bot.pet_hp_pct() <= 50 && Ready(ctx, MEND_PET)
        && ctx.bot.find_pet_aura(MEND_PET) == nullptr;
}
void DoMendPet(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(MEND_PET); }

bool ShouldPetAttack(ApPredicateContext const& ctx)
{
    return ctx.bot.has_pet() && HasLiveTarget(ctx) && ctx.bot.pet_victim() != ctx.bot.victim()
        && Playerbot::AplPet::PetAttackDue(ctx.bot.guid().GetCounter(), ctx.bot.published_at_ms());
}
void DoPetAttack(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    Playerbot::AplPet::NotePetAttack(ctx.bot.guid().GetCounter(), ctx.bot.published_at_ms());
    e.pet_attack(ctx.bot.victim());
}

// ======================= Defensive =======================

bool ShouldDeterrence(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && ctx.bot.hp_pct() <= 25 && Ready(ctx, DETERRENCE);
}
void DoDeterrence(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(DETERRENCE); }

// ======================= Cooldowns =======================

// Boss fights only (Classic::BossFight - inside a dungeon or raid an engaged
// boss or encounter, never trash).
bool ShouldBestialWrath(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && ctx.bot.has_pet() && Classic::BossFight(ctx.bot) && Ready(ctx, BESTIAL_WRATH);
}
void DoBestialWrath(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(BESTIAL_WRATH);
    Classic::NoteCooldownUse(ctx.bot, BESTIAL_WRATH, "Bestial Wrath");
}

bool ShouldRapidFire(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && Classic::BossFight(ctx.bot) && Ready(ctx, RAPID_FIRE)
        && !ctx.bot.has_aura(RAPID_FIRE);
}
void DoRapidFire(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    e.cast(RAPID_FIRE);
    Classic::NoteCooldownUse(ctx.bot, RAPID_FIRE, "Rapid Fire");
}

// ======================= Ranged =======================

bool ShouldHuntersMark(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx) || !(Ready(ctx, HUNTERS_MARK) && ctx.bot.victim_in_spell_range(HUNTERS_MARK))) return false;
    if (!VictimInShotRange(ctx) && !VictimInMelee(ctx)) return false;
    if (VictimHpPct(ctx) < 50 && !VictimIsBossLike(ctx)) return false;
    // Any hunter's mark counts: marks do not stack, and with several hunters each one saw "my mark absent" and overwrote the others'.
    for (AuraEntry const& a : ctx.bot.raw().auras.victim_auras)
        if (a.spell_id && SpellFamily::Same(a.spell_id, HUNTERS_MARK) && a.remaining.count() > 5000)
            return false;
    return true;
}
void DoHuntersMark(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(HUNTERS_MARK, ctx.bot.victim()); }

bool ShouldSerpentSting(ApPredicateContext const& ctx)
{
    if (!HasLiveTarget(ctx) || !VictimInShotRange(ctx) || Mana(ctx) < 50) return false;
    if (VictimHpPct(ctx) < 40) return false;
    return (Ready(ctx, SERPENT_STING) && ctx.bot.victim_in_spell_range(SERPENT_STING)) && !ctx.bot.has_aura(SERPENT_STING, ctx.bot.victim());
}
void DoSerpentSting(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(SERPENT_STING, ctx.bot.victim()); }

// Living enemies within `radius` of the victim, the victim included: Multi-Shot's extra arrows hit the victim's neighbours,
// not the bot's (enemies_within counts around the bot, 25+ yards away from a ranged hunter's target).
int EnemiesAroundVictim(ApPredicateContext const& ctx, float radius)
{
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v) return 0;
    int n = 0;
    for (NearbyUnit const& u : ctx.bot.raw().combat.nearby_enemies)
    {
        if (u.hp <= 0 || u.is_pacified || u.untargetable) continue;
        float const dx = u.x - v->x, dy = u.y - v->y, dz = u.z - v->z;
        if (dx * dx + dy * dy + dz * dz <= radius * radius) ++n;
    }
    return n;
}

bool ShouldMultiShot(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && VictimInShotRange(ctx) && ctx.bot.power_pct(POWER_MANA) >= 30
        && EnemiesAroundVictim(ctx, 10.0f) >= 2 && (Ready(ctx, MULTI_SHOT) && ctx.bot.victim_in_spell_range(MULTI_SHOT))
        // The extra arrows hit the victim's neighbours: not a sheep, an
        // unpulled mob or one the tank has not secured.
        && AoeSafeAroundVictim(ctx.bot, 10.0f, "Multi-Shot", MULTI_SHOT);
}
void DoMultiShot(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(MULTI_SHOT, ctx.bot.victim()); }

// Hard-cast: skip while moving so the shot is not cancelled.
bool ShouldAimedShot(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && VictimInShotRange(ctx) && !ctx.bot.is_moving()
        && ctx.bot.power_pct(POWER_MANA) >= 25 && (Ready(ctx, AIMED_SHOT) && ctx.bot.victim_in_spell_range(AIMED_SHOT));
}
void DoAimedShot(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(AIMED_SHOT, ctx.bot.victim()); }

bool ShouldArcaneShot(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && VictimInShotRange(ctx) && ctx.bot.power_pct(POWER_MANA) >= 20
        && (Ready(ctx, ARCANE_SHOT) && ctx.bot.victim_in_spell_range(ARCANE_SHOT));
}
void DoArcaneShot(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(ARCANE_SHOT, ctx.bot.victim()); }

// Raid school rule (ApSchoolHint.h): a preferred school on the victim puts its
// shot first among the ranged picks - nature: Serpent Sting, arcane: Arcane
// Shot - ahead of the physical Multi-Shot / Aimed Shot. Same range and mana
// gates as their own rules.
SchoolHint::SchoolChoice const kSchoolChoices[] = {
    { SERPENT_STING, ShouldSerpentSting },
    { ARCANE_SHOT,   ShouldArcaneShot },
};
bool ShouldSchoolShot(ApPredicateContext const& ctx) { return SchoolHint::ShouldCast(ctx, kSchoolChoices); }
void DoSchoolShot(ApPredicateContext const& ctx, BotIntentEmitter& e) { SchoolHint::DoCast(ctx, e, kSchoolChoices); }

// ======================= Melee (target inside the dead zone) =======================

bool ShouldWingClip(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && VictimInMelee(ctx) && Mana(ctx) >= 40 && (Ready(ctx, WING_CLIP) && ctx.bot.victim_in_spell_range(WING_CLIP))
        && !ctx.bot.has_aura(WING_CLIP, ctx.bot.victim());
}
void DoWingClip(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(WING_CLIP, ctx.bot.victim()); }

bool ShouldMongooseBite(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && VictimInMelee(ctx) && (Ready(ctx, MONGOOSE_BITE) && ctx.bot.victim_in_spell_range(MONGOOSE_BITE));
}
void DoMongooseBite(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(MONGOOSE_BITE, ctx.bot.victim()); }

bool ShouldRaptorStrike(ApPredicateContext const& ctx)
{
    return HasLiveTarget(ctx) && VictimInMelee(ctx) && (Ready(ctx, RAPTOR_STRIKE) && ctx.bot.victim_in_spell_range(RAPTOR_STRIKE));
}
void DoRaptorStrike(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(RAPTOR_STRIKE, ctx.bot.victim()); }

// Pre-pull Freezing Trap at the hunter's feet when the target stands in a
// pack nobody is fighting yet and is far enough away for the trap to sit
// between them.
constexpr uint32 FREEZING_TRAP = 1499;
bool ShouldPrePullTrap(ApPredicateContext const& ctx)
{
    if (ctx.bot.in_combat() || !HasLiveTarget(ctx) || !Ready(ctx, FREEZING_TRAP))
        return false;
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v || !v->victim.IsEmpty())
        return false;
    auto const& me = ctx.bot.raw().position;
    float const vx = v->x - me.x, vy = v->y - me.y;
    if (vx * vx + vy * vy < 15.f * 15.f)
        return false;
    for (NearbyUnit const& u : ctx.bot.raw().combat.nearby_enemies)
    {
        if (u.guid == v->guid || u.hp <= 0 || !u.victim.IsEmpty() || u.is_pacified || u.untargetable) continue;
        float const dx = u.x - v->x, dy = u.y - v->y;
        if (dx * dx + dy * dy <= 12.f * 12.f)
            return true;
    }
    return false;
}
void DoPrePullTrap(ApPredicateContext const&, BotIntentEmitter& e) { e.cast(FREEZING_TRAP); }

// Tranquilizing Shot (level 60 book; only once learned): an elite / boss
// engaged with the group carrying an aura the shot removes (Enrage / vanilla
// Frenzy: Magmadar, Death Talon Seether, Chromaggus, Gluth -
// NearbyUnit::enrage_age_ms). In a planned group the hunters take turns
// (PveOrder::tranq_rank, fixed by guid among the hunters with it ready when
// the enrage appeared): rank 0 shoots at once, rank r once the enrage has
// been up r seconds (the earlier shot missed or was resisted); a hunter
// outside the order only after 4s. Solo / no plan: at once.
constexpr uint32 TRANQUILIZING_SHOT = 19801;
NearbyUnit const* TranqTarget(ApPredicateContext const& ctx)
{
    PveOrder const& po = ctx.bot.raw().pve_order;
    const bool planned = po.active && !ctx.bot.in_battleground();
    const int32 wait_ms = !planned ? 0 : po.tranq_rank == 0xFF ? 4000 : int32(po.tranq_rank) * 1000;
    auto const& me = ctx.bot.raw().position;
    NearbyUnit const* best = nullptr;
    float best_d2 = 0.f;
    for (NearbyUnit const& u : ctx.bot.raw().combat.nearby_enemies)
    {
        if (u.hp <= 0 || u.is_player || u.enrage_age_ms < 0 || u.enrage_age_ms < wait_ms) continue;
        if (!(u.is_elite_or_rare || (u.is_dungeon_boss || u.is_boss_rank)) || !u.unit_in_combat || !u.in_los) continue;
        if (!Interrupt::EngagedWithGroup(u)) continue;
        float const dx = u.x - me.x, dy = u.y - me.y, dz = u.z - me.z;
        float const d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < 8.f * 8.f || d2 > 38.f * 38.f) continue;   // ranged dead zone .. 35y (+ boss body)
        if (!best || d2 < best_d2) { best = &u; best_d2 = d2; }
    }
    return best;
}
bool ShouldTranqShot(ApPredicateContext const& ctx)
{
    return ctx.bot.in_combat() && Ready(ctx, TRANQUILIZING_SHOT) && TranqTarget(ctx) != nullptr;
}
void DoTranqShot(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    NearbyUnit const* u = TranqTarget(ctx);
    if (!u || !e.cast(TRANQUILIZING_SHOT, u->guid)) return;
    BotSnapshotView const& s = ctx.bot;
    Classic::NoteRaidSupport(Classic::RaidSupportKind::Tranq, s.map_id());
    if (s.is_in_raid() && RaidLogDue(RaidLogKey(s, RaidLogKind::Tranq), s.published_at_ms(), 3000u))
    {
        CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(u->entry);
        TC_LOG_INFO("playerbot.v2", "[raid_tranq] {} tranquilizes {} ({}) aura {} up {}ms (rank {})",
            s.name(), ct ? ct->Name : std::string("?"), u->guid.GetCounter(), u->enrage_spell,
            u->enrage_age_ms, uint32(s.raw().pve_order.tranq_rank));
    }
}

// Concussive Shot (kiting): a mob that is chasing the hunter (not a boss -
// bosses are immune to snares, and a tank holds them) is slowed once it is
// outside the dead zone, so the next shots land before it closes. Inside the
// dead zone Wing Clip does the same. Playerbot.Behavior.RangedSlow.
bool ShouldConcussiveShot(ApPredicateContext const& ctx)
{
    if (!Services::Config().behavior_ranged_slow() || !ctx.bot.in_combat() || !HasLiveTarget(ctx)
        || !VictimInShotRange(ctx) || ctx.bot.power_pct(POWER_MANA) < 25 || !(Ready(ctx, CONCUSSIVE_SHOT) && ctx.bot.victim_in_spell_range(CONCUSSIVE_SHOT)))
        return false;
    NearbyUnit const* v = ctx.bot.victim_info();
    if (!v || v->victim != ctx.bot.guid() || v->is_player || VictimIsBossLike(ctx) || v->stun_immune)
        return false;
    return !ctx.bot.has_aura(CONCUSSIVE_SHOT, ctx.bot.victim());
}
void DoConcussiveShot(ApPredicateContext const& ctx, BotIntentEmitter& e) { e.cast(CONCUSSIVE_SHOT, ctx.bot.victim()); }

ApRule const kRules[] = {
    { ShouldDeterrence,    DoDeterrence,    "classic_hunter:deterrence" },
    { ShouldConcussiveShot, DoConcussiveShot, "classic_hunter:concussive_shot" },
    { ShouldTranqShot,     DoTranqShot,     "classic_hunter:tranquilizing_shot" },
    { ShouldCallPet,       DoCallPet,       "classic_hunter:call_pet" },
    { ShouldRevivePet,     DoRevivePet,     "classic_hunter:revive_pet" },
    { ShouldMendPet,       DoMendPet,       "classic_hunter:mend_pet" },
    { ShouldAspectHawk,    DoAspectHawk,    "classic_hunter:aspect_of_the_hawk" },
    { ShouldTrueshotAura,  DoTrueshotAura,  "classic_hunter:trueshot_aura" },
    { ShouldPrePullTrap,   DoPrePullTrap,   "classic_hunter:freezing_trap" },
    { ShouldPetAttack,     DoPetAttack,     "classic_hunter:pet_attack" },
    { ShouldBestialWrath,  DoBestialWrath,  "classic_hunter:bestial_wrath" },
    { ShouldRapidFire,     DoRapidFire,     "classic_hunter:rapid_fire" },
    { ShouldHuntersMark,   DoHuntersMark,   "classic_hunter:hunters_mark" },
    { ShouldSchoolShot,    DoSchoolShot,    "classic_hunter:school_shot" },
    { ShouldSerpentSting,  DoSerpentSting,  "classic_hunter:serpent_sting" },
    { ShouldMultiShot,     DoMultiShot,     "classic_hunter:multi_shot" },
    { ShouldAimedShot,     DoAimedShot,     "classic_hunter:aimed_shot" },
    { ShouldArcaneShot,    DoArcaneShot,    "classic_hunter:arcane_shot" },
    { ShouldWingClip,      DoWingClip,      "classic_hunter:wing_clip" },
    { ShouldMongooseBite,  DoMongooseBite,  "classic_hunter:mongoose_bite" },
    { ShouldRaptorStrike,  DoRaptorStrike,  "classic_hunter:raptor_strike" },
    { HasLiveTarget,       DoAutoAttack,    "classic_hunter:auto_attack" },
};

} // namespace

void RegisterApl_Classic_Hunter()
{
    ApRotation const hunter{ std::span<ApRule const>(kRules) };
    RegisterRotationOverride(CLASS_HUNTER, 0, hunter);
    RegisterRotationOverride(CLASS_HUNTER, 253, hunter);
    RegisterRotationOverride(CLASS_HUNTER, 254, hunter);
    RegisterRotationOverride(CLASS_HUNTER, 255, hunter);
}

} // namespace Playerbot::Combat
