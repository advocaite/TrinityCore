// ApInterrupt - the Classic 1.60 interrupt rotation, shared by the Classic
// warrior / rogue / mage / shaman / priest (Silence) / warlock (Felhunter
// Spell Lock) rotations and the PvE group coordinator.
//
//   * ClassicAbility: the vanilla interrupt the bot can use right now - it
//     depends on live state (Pummel only in Berserker Stance, Shield Bash only
//     outside it with a shield, Spell Lock only with a Felhunter out, Bash
//     only in Bear / Dire Bear Form). The coordinator ranks only bots for
//     which this is non-zero, so rank 0 is never a bot that cannot kick.
//     Healers never interrupt. Bash is a melee stun: it needs 10 rage
//     (Ready) and never targets a stun-immune mob (InRange,
//     NearbyUnit::stun_immune) - most raid bosses.
//   * Pick: the unit to interrupt. With an active PvE plan (dungeon / raid
//     group) it is, in order: the mob the plan assigned to this bot
//     (PveOrder::interrupt_target, rank windows of interrupt_target_rank);
//     BotSnapshotView::interruptible_caster() (attackers, interrupt_rank
//     windows); then any caster engaged with the group (mobs on the tank are
//     not in this bot's attackers) under the same interrupt_rank window - a
//     mob the plan gave to someone else only as a late backup. Always limited
//     to the spell's range. Without a plan (solo, open world): the bot's own
//     victim while it casts, as before.
//   * Cast: casts on the picked unit (not only the victim) - a pet cast for
//     Spell Lock - and logs [raid_interrupt] in raids (rate-limited).

#pragma once

#include "../ApRotation.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshot.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/ClassTables.h"
#include "Bot/ThreatDiscipline.h"   // RaidLogDue
#include "Group/GroupSnapshot.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"                  // EQUIPMENT_SLOT_OFFHAND
#include "Services.h"
#include "Util/ConfigReader.h"       // Playerbot.Behavior.PurgeDuty
#include <mutex>
#include <unordered_map>
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace Playerbot::Combat::Interrupt {

// Vanilla rank-1 ids; the snapshot view and API::cast_spell resolve the
// highest known rank.
constexpr uint32 PUMMEL           = 6552;
constexpr uint32 SHIELD_BASH      = 72;
constexpr uint32 KICK             = 1766;
constexpr uint32 COUNTERSPELL     = 2139;
constexpr uint32 EARTH_SHOCK      = 8042;
constexpr uint32 SILENCE          = 15487;   // Shadow talent
constexpr uint32 BASH             = 5211;
constexpr uint32 KIDNEY_SHOT      = 408;     // rogue stun, the Kick fallback (VMaNGOS rogue: kick falls back to kidney shot)
constexpr uint32 BERSERKER_STANCE = 2458;
constexpr uint32 BEAR_FORM        = 5487;
constexpr uint32 DIRE_BEAR_FORM   = 9634;

struct Ability
{
    uint32 spell = 0;      // 0 = nothing usable now
    float  range = 0.f;
    bool   pet   = false;  // Spell Lock: cast by the pet (exact pet rank)
    bool   melee = false;
};

inline bool HasShield(BotSnapshotView const& s)
{
    uint32 const entry = s.raw().inventory.equipped[EQUIPMENT_SLOT_OFFHAND].entry;
    ItemTemplate const* t = entry ? sObjectMgr->GetItemTemplate(entry) : nullptr;
    return t && t->GetClass() == ITEM_CLASS_ARMOR && t->GetSubClass() == ITEM_SUBCLASS_ARMOR_SHIELD;
}

inline bool IsHealer(BotSnapshotView const& s)
{
    return s.raw().group.my_role == Role::Healer || IsHealerSpec(s.cls(), uint16(s.spec()));
}

// The interrupt this bot can use in its current stance / form / pet (cooldown
// not checked). Healers: none.
inline Ability ClassicAbility(BotSnapshotView const& s)
{
    if (IsHealer(s)) return {};
    switch (s.cls())
    {
        case CLASS_WARRIOR:
            if (s.has_aura(BERSERKER_STANCE))
                return s.knows_spell(PUMMEL) ? Ability{ PUMMEL, 5.f, false, true } : Ability{};
            if (s.knows_spell(SHIELD_BASH) && HasShield(s))
                return { SHIELD_BASH, 5.f, false, true };
            return {};
        case CLASS_ROGUE:
            // Kick, or while it recovers a Kidney Shot on a combo point (stun interrupts; not on a stun-immune mob, see Affects).
            if (s.knows_spell(KICK) && s.is_ready(KICK))
                return { KICK, 5.f, false, true };
            if (s.knows_spell(KIDNEY_SHOT) && s.is_ready(KIDNEY_SHOT) && s.power(4) >= 1)
                return { KIDNEY_SHOT, 5.f, false, true };
            return s.knows_spell(KICK) ? Ability{ KICK, 5.f, false, true } : Ability{};
        case CLASS_MAGE:
            return s.knows_spell(COUNTERSPELL) ? Ability{ COUNTERSPELL, 30.f } : Ability{};
        case CLASS_SHAMAN:
            return s.knows_spell(EARTH_SHOCK) ? Ability{ EARTH_SHOCK, 20.f } : Ability{};
        case CLASS_PRIEST:
            return s.knows_spell(SILENCE) ? Ability{ SILENCE, 20.f } : Ability{};
        case CLASS_WARLOCK:
            if (s.has_pet() && s.raw().pet.pet_interrupt_spell)
                return { s.raw().pet.pet_interrupt_spell, 30.f, true };
            return {};
        case CLASS_DRUID:
            if ((s.has_aura(BEAR_FORM) || s.has_aura(DIRE_BEAR_FORM)) && s.knows_spell(BASH))
                return { BASH, 5.f, false, true };
            return {};
        default:
            return {};   // paladin, hunter: no interrupt in 1.12
    }
}

inline bool Ready(BotSnapshotView const& s, Ability const& a)
{
    if (!a.spell) return false;
    if (a.pet) return s.raw().pet.pet_interrupt_ready;
    if (a.spell == BASH && s.power(POWER_RAGE) < 10) return false;   // Bash: 10 rage
    return s.knows_spell(a.spell) && s.is_ready(a.spell);
}

// The ability can affect the unit at all: Bash interrupts only through its
// stun, so a stun-immune mob is never a Bash target.
inline bool Affects(Ability const& a, NearbyUnit const& u)
{
    return (a.spell != BASH && a.spell != KIDNEY_SHOT) || !u.stun_immune;
}

inline float Dist(BotSnapshotView const& s, NearbyUnit const& u)
{
    float x, y, z; s.position(x, y, z);
    const float dx = u.x - x, dy = u.y - y, dz = u.z - z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Melee kicks: the victim the bot is swinging at is in reach by definition
// (large bosses sit beyond 5y centre to centre); another mob within 5y plus a
// little body radius. False when the ability cannot affect the unit (Affects).
inline bool InRange(BotSnapshotView const& s, Ability const& a, NearbyUnit const& u)
{
    if (!Affects(a, u)) return false;
    // The real spell range to this unit's reach (the Shield Bash / Pummel the
    // old "victim is in reach by definition" shortcut sent at a victim the
    // tank had not reached yet: live MC 2026-10-07 OUT_OF_RANGE x11 / API_BEYOND_RANGE).
    // The pet's Spell Lock keeps its flat range (the pet casts it, not this bot).
    if (a.pet) return Dist(s, u) <= a.range;
    return s.in_spell_range(a.spell, u);
}

inline bool Casting(NearbyUnit const& u)
{
    return u.hp > 0 && !u.is_player && u.is_casting && u.is_interruptible && !u.untargetable;
}

// Engaged with the group: hitting a member (incl. this bot and the tanks) or
// a member's pet.
inline bool EngagedWithGroup(NearbyUnit const& u)
{
    switch (ThreatVictim(u.victim_kind))
    {
        case ThreatVictim::Me: case ThreatVictim::Tank: case ThreatVictim::Member: case ThreatVictim::MemberPet:
            return true;
        default:
            return false;
    }
}

struct Choice
{
    NearbyUnit const* unit = nullptr;
    uint8             rank = 0xFF;
};

inline NearbyUnit const* FindEnemy(BotSnapshotView const& s, ObjectGuid g)
{
    for (NearbyUnit const& u : s.attackers())
        if (u.guid == g) return &u;
    for (NearbyUnit const& u : s.nearby_enemies())
        if (u.guid == g) return &u;
    return nullptr;
}

inline Choice Pick(BotSnapshotView const& s, Ability const& a)
{
    if (!a.spell) return {};
    PveOrder const& po = s.raw().pve_order;
    if (!po.active || s.in_battleground())
    {
        // Legacy: the bot's own victim while it casts.
        NearbyUnit const* v = s.victim_info();
        if (v && Casting(*v) && InRange(s, a, *v)) return { v, 0 };
        return {};
    }
    // 1. The mob the plan gave this bot.
    if (!po.interrupt_target.IsEmpty())
        if (NearbyUnit const* u = FindEnemy(s, po.interrupt_target))
            if (Casting(*u) && InRange(s, a, *u)
                && s.interrupt_window_open(u->cast_remaining.count(), po.interrupt_target_rank))
                return { u, po.interrupt_target_rank };
    // A mob assigned to another interrupter: this bot is only a late backup.
    auto rank_for = [&](NearbyUnit const& u) -> uint8
    {
        return po.interrupt_claimed_by_other(u.guid) ? std::max<uint8>(po.interrupt_rank, 2) : po.interrupt_rank;
    };
    // 2. Attackers under the interrupt_rank window.
    if (NearbyUnit const* u = s.interruptible_caster())
        if (Casting(*u) && InRange(s, a, *u) && s.interrupt_window_open(u->cast_remaining.count(), rank_for(*u)))
            return { u, rank_for(*u) };
    // 3. Any group-engaged caster in range, own victim first.
    if (NearbyUnit const* v = s.victim_info())
        if (Casting(*v) && EngagedWithGroup(*v) && InRange(s, a, *v)
            && s.interrupt_window_open(v->cast_remaining.count(), rank_for(*v)))
            return { v, rank_for(*v) };
    for (NearbyUnit const& u : s.nearby_enemies())
        if (Casting(u) && EngagedWithGroup(u) && InRange(s, a, u)
            && s.interrupt_window_open(u.cast_remaining.count(), rank_for(u)))
            return { &u, rank_for(u) };
    return {};
}

// Rule body for the Classic rotations. `spell` is the rotation's own
// interrupt (a warrior lists Pummel and Shield Bash as separate rules): the
// rule fires only when it is the ability ClassicAbility resolves now.
inline bool Should(ApPredicateContext const& ctx, uint32 spell)
{
    Ability const a = ClassicAbility(ctx.bot);
    if (!a.spell || (spell && a.spell != spell) || !Ready(ctx.bot, a)) return false;
    return Pick(ctx.bot, a).unit != nullptr;
}

inline void Cast(ApPredicateContext const& ctx, BotIntentEmitter& e, uint32 spell)
{
    BotSnapshotView const& s = ctx.bot;
    Ability const a = ClassicAbility(s);
    if (!a.spell || (spell && a.spell != spell)) return;
    Choice const c = Pick(s, a);
    if (!c.unit) return;
    const bool pushed = a.pet ? e.pet_cast(a.spell, c.unit->guid) : e.cast(a.spell, c.unit->guid);
    if (!pushed || !s.is_in_raid()
        || !RaidLogDue(RaidLogKey(s, RaidLogKind::Interrupt), s.published_at_ms(), 3000u))
        return;
    CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(c.unit->entry);
    SpellInfo const* si = sSpellMgr->GetSpellInfo(c.unit->casting_spell_id, DIFFICULTY_NONE);
    TC_LOG_INFO("playerbot.v2",
        "[raid_interrupt] {} interrupts {} ({}) casting {} ({}) with {}{} (rank {}, {}ms left)",
        s.name(), ct ? ct->Name : std::string("?"), c.unit->guid.GetCounter(),
        (si && si->SpellName) ? si->SpellName->Str[LOCALE_enUS] : "?", c.unit->casting_spell_id,
        a.spell, a.pet ? " (pet)" : "", uint32(c.rank), int64(c.unit->cast_remaining.count()));
}

// ---- Purge duty (the owner: ike3 DecideRaidCast purge election) ----
// Strips Magic buffs from an elite / boss engaged with a dungeon / raid
// group. One bot at a time: every bot computes the same election from the
// group snapshot - bot shamans with Purge first, then bot priests with
// Dispel Magic; non-healers before healers; guid order inside each. A healer
// yields (is not elected) while any member is under kEmergencyPct. Hunter
// Tranquilizing Shot (Enrage / Frenzy) has its own order (PlanTranq), and
// friendly dispels are the dispel plan's (Classic::PickDispel), so none of
// them conflict. Playerbot.Behavior.PurgeDuty = 0 turns it off.
namespace Purge {

constexpr uint32 PURGE        = 370;    // shaman
constexpr uint32 DISPEL_MAGIC = 527;    // priest
constexpr float  kRange       = 28.f;   // spell range 30, snapshot cadence slack
constexpr int32  kEmergencyPct = 45;
constexpr int32  kMinManaPct   = 40;
constexpr int32  kMinBuffAgeMs = 1500;  // let the cast that applied it finish
constexpr uint32 kCooldownMs   = 3000;  // per bot: one strip, then look again

inline uint32 Spell(BotSnapshotView const& s)
{
    if (s.cls() == CLASS_SHAMAN && s.knows_spell(PURGE))        return PURGE;
    // Shadowform blocks the Holy school: Dispel Magic cannot be cast in it.
    if (s.cls() == CLASS_PRIEST && s.knows_spell(DISPEL_MAGIC) && !s.has_aura(15473)) return DISPEL_MAGIC;
    return 0;
}

template <typename F>
inline auto WithLast(F&& f)
{
    static std::mutex mu;
    static std::unordered_map<uint64, uint32> last;
    std::lock_guard<std::mutex> lock(mu);
    return f(last);
}

inline bool Elected(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    auto const* mems = g.members();
    if (!mems) return false;
    bool emergency = false;
    for (GroupMemberSummary const& m : *mems)
        if (m.online && m.is_alive && m.max_hp > 0 && m.map_id == s.map_id() && m.hp_pct() < kEmergencyPct)
            { emergency = true; break; }
    struct Cand { uint8 healer; uint8 cls_order; uint64 low; };
    std::vector<Cand> cands;
    bool self_seen = false;
    for (GroupMemberSummary const& m : *mems)
    {
        if (m.guid == s.guid()) self_seen = true;
        if (!m.online || !m.is_alive || !m.is_bot || m.map_id != s.map_id()) continue;
        const bool shaman = m.cls == CLASS_SHAMAN && m.level >= 12;
        const bool priest = m.cls == CLASS_PRIEST && m.level >= 18;
        if (!shaman && !priest) continue;
        const bool healer = m.role == Role::Healer || (m.guid == s.guid() && IsHealer(s));
        if (healer && emergency) continue;
        cands.push_back({ uint8(healer ? 1 : 0), uint8(shaman ? 0 : 1), m.guid.GetCounter() });
    }
    if (!self_seen && (s.cls() == CLASS_SHAMAN || s.cls() == CLASS_PRIEST) && !(IsHealer(s) && emergency))
        cands.push_back({ uint8(IsHealer(s) ? 1 : 0), uint8(s.cls() == CLASS_SHAMAN ? 0 : 1), s.guid().GetCounter() });
    if (cands.empty()) return false;
    auto const best = std::min_element(cands.begin(), cands.end(), [](Cand const& a, Cand const& b)
    {
        if (a.healer != b.healer) return a.healer < b.healer;
        if (a.cls_order != b.cls_order) return a.cls_order < b.cls_order;
        return a.low < b.low;
    });
    return best->low == s.guid().GetCounter();
}

inline NearbyUnit const* Target(BotSnapshotView const& s)
{
    float x, y, z; s.position(x, y, z);
    NearbyUnit const* best = nullptr;
    float best_d2 = 0.f;
    for (NearbyUnit const& u : s.nearby_enemies())
    {
        if (u.hp <= 0 || u.is_player || !u.magic_buff_spell || u.magic_buff_age_ms < kMinBuffAgeMs) continue;
        if (!(u.is_elite_or_rare || u.is_dungeon_boss || u.is_boss_rank) || !u.unit_in_combat || !u.in_los
            || u.untargetable || !EngagedWithGroup(u)) continue;
        const float dx = u.x - x, dy = u.y - y, dz = u.z - z;
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > kRange * kRange) continue;
        if (!best || d2 < best_d2) { best = &u; best_d2 = d2; }
    }
    return best;
}

inline bool Should(ApPredicateContext const& ctx)
{
    BotSnapshotView const& s = ctx.bot;
    if (!Services::Config().behavior_purge_duty() || !s.in_combat() || s.in_battleground() || s.is_casting() || s.is_mounted())
        return false;
    PveOrder const& po = s.raw().pve_order;
    if (!po.active || !s.is_in_instance()) return false;
    const uint32 spell = Spell(s);
    if (!spell || !s.is_ready(spell)) return false;
    if (s.max_power(POWER_MANA) <= 0 || s.power_pct(POWER_MANA) < kMinManaPct) return false;
    const uint32 now = s.published_at_ms();
    if (WithLast([&](auto& last)
        {
            auto it = last.find(s.guid().GetCounter());
            return it != last.end() && now - it->second < kCooldownMs;
        }))
        return false;
    if (!Target(s)) return false;
    return Elected(s, ctx.group);
}

inline void Cast(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    BotSnapshotView const& s = ctx.bot;
    NearbyUnit const* u = Target(s);
    const uint32 spell = Spell(s);
    if (!u || !spell || !e.cast(spell, u->guid)) return;
    const uint32 now = s.published_at_ms();
    WithLast([&](auto& last) { last[s.guid().GetCounter()] = now; return 0; });
    if (s.is_in_raid() && RaidLogDue((uint64(s.guid().GetCounter()) << 8) | 35u, now, 3000u))
    {
        CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(u->entry);
        TC_LOG_INFO("playerbot.v2", "[raid_purge] {} strips aura {} from {} ({}) with {}",
            s.name(), u->magic_buff_spell, ct ? ct->Name : std::string("?"), u->guid.GetCounter(), spell);
    }
}

} // namespace Purge

// ---- Warlock pet choice (Felhunter for Spell Lock) ----
// In a dungeon / raid group: the first CAPABLE bot warlock (by guid) of each
// subgroup, at most two per raid, is a Felhunter warlock - Spell Lock is the
// group's ranged interrupt. With a group plan the coordinator decides it from
// every warlock's snapshot (PveOrder::felhunter_duty, PlanFelhunters): a
// warlock that has not learned Summon Felhunter, has no soul shard to
// summon one, or whose Felhunter was seen without Spell Lock hands the duty
// to the next warlock by guid. Without a plan (the coordinator does not
// cover the group): the roster alone, warlocks under 30 skipped (the
// Felhunter quest). Every other grouped warlock runs the Imp (Blood Pact);
// solo keeps the Voidwalker.
constexpr uint32 SUMMON_IMP        = 688;
constexpr uint32 SUMMON_VOIDWALKER = 697;
constexpr uint32 SUMMON_FELHUNTER  = 691;
constexpr uint32 SUMMON_SUCCUBUS   = 712;
constexpr uint8  kMaxRaidFelhunters = 2;

// What this warlock's snapshot says about fielding a Spell Lock Felhunter
// (PveGroupCoordinator::PlanFelhunters). A live Felhunter out decides it by
// its spellbook (Spell Lock is a Grimoire the pet must have learned); with
// any other demon, a dead one or none, the warlock needs Summon Felhunter
// and the soul shard it costs.
enum class FelhunterCap : uint8 { Unable, Able, NoSpellLock };
inline FelhunterCap FelhunterCapability(BotSnapshotView const& s)
{
    if (s.cls() != CLASS_WARLOCK || !s.knows_spell(SUMMON_FELHUNTER)) return FelhunterCap::Unable;
    PetState const& pet = s.raw().pet;
    if (s.has_pet() && pet.pet_family == CREATURE_FAMILY_FELHUNTER)
        return pet.pet_interrupt_spell ? FelhunterCap::Able : FelhunterCap::NoSpellLock;
    return s.has_reagents(SUMMON_FELHUNTER) ? FelhunterCap::Able : FelhunterCap::Unable;
}

inline bool FelhunterDuty(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    if (PveOrder const& po = s.raw().pve_order; po.active)
        return po.felhunter_duty;
    auto const* mems = g.members();
    if (!mems) return false;
    std::vector<GroupMemberSummary const*> locks;
    for (auto const& m : *mems)
        if (m.online && m.is_bot && m.cls == CLASS_WARLOCK && m.level >= 30 && m.map_id == s.map_id())
            locks.push_back(&m);
    std::sort(locks.begin(), locks.end(), [](GroupMemberSummary const* a, GroupMemberSummary const* b)
    {
        return a->guid.GetCounter() < b->guid.GetCounter();
    });
    uint8 taken_subgroups = 0;   // bit per subgroup 0..7
    uint8 felhunters = 0;
    for (GroupMemberSummary const* m : locks)
    {
        if (felhunters >= kMaxRaidFelhunters) break;
        const uint8 bit = uint8(1u << (m->subgroup & 7));
        if (taken_subgroups & bit) continue;
        taken_subgroups |= bit;
        ++felhunters;
        if (m->guid == s.guid()) return true;
    }
    return false;
}

// The summon spell this Classic warlock should have out (0 = none known).
inline uint32 ClassicWarlockPet(BotSnapshotView const& s, GroupSnapshotView const* g)
{
    if (g && g->exists() && s.is_in_instance())
    {
        // A raid warlock always has the Imp out: its Blood Pact is +stamina for the party (a tank's health), which
        // the raid needs more than a Spell Lock. Felhunter duty is for dungeon parties.
        if (!s.is_in_raid() && s.knows_spell(SUMMON_FELHUNTER) && FelhunterDuty(s, *g)) return SUMMON_FELHUNTER;
        if (s.knows_spell(SUMMON_IMP)) return SUMMON_IMP;
    }
    // An ambient (world / pool) warlock has its own demon out, stable per bot: a street full of voidwalkers looked uniform (the owner,
    // Ironforge 2026-10-12). One of the demons it has learned, picked by its guid (Imp L1, Voidwalker L10, Succubus L20, Felhunter L30).
    // Without the soul shard the Succubus / Felhunter / Voidwalker cost, and with no demon out, the Imp comes out instead; a demon already
    // out is kept. Owned companions keep the rule above and below.
    if (!s.has_owner_character())
    {
        uint32 known[4];
        uint32 n = 0;
        for (uint32 sp : { SUMMON_IMP, SUMMON_VOIDWALKER, SUMMON_SUCCUBUS, SUMMON_FELHUNTER })
            if (s.knows_spell(sp))
                known[n++] = sp;
        if (n)
        {
            uint64 const guid = s.guid().GetCounter();
            uint32 const h = uint32((guid * 0x9E3779B97F4A7C15ull) >> 40);
            uint32 const pick = known[h % n];
            if (pick == SUMMON_IMP || s.has_pet() || s.has_reagents(pick))
                return pick;
            return SUMMON_IMP;
        }
        return 0;
    }
    if (s.knows_spell(SUMMON_VOIDWALKER)) return SUMMON_VOIDWALKER;
    if (s.knows_spell(SUMMON_IMP)) return SUMMON_IMP;
    return 0;
}

// creature_template.family of the demon a summon spell brings.
inline uint32 SummonPetFamily(uint32 summon)
{
    switch (summon)
    {
        case SUMMON_IMP:        return CREATURE_FAMILY_IMP;
        case SUMMON_VOIDWALKER: return CREATURE_FAMILY_VOIDWALKER;
        case SUMMON_FELHUNTER:  return CREATURE_FAMILY_FELHUNTER;
        case SUMMON_SUCCUBUS:   return CREATURE_FAMILY_SUCCUBUS;
        default:                return 0;
    }
}

} // namespace Playerbot::Combat::Interrupt
