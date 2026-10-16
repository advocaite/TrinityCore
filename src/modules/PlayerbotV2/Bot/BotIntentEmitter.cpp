#include "BotIntentEmitter.h"
#include <atomic>
#include <mutex>
#include "BotAI.h"
#include "BotSnapshot.h"
#include "BotSnapshotView.h"
#include "Log.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Threading/IntentQueue.h"
#include "../Services.h"
#include "../Diagnostics/PerfCounters.h"

namespace Playerbot {

bool BotIntentEmitter::push(Intent i)
{
    if (!queue_) return false;
    if (!queue_->push(std::move(i)))
    {
        // Per-bot ring overflow. The intent is silently dropped — the
        // rotation will re-fire next tick. We track here so SystemStatus
        // surfaces a sustained backlog as a real signal rather than
        // silently degrading bot responsiveness.
        if (Services::Initialized())
            Services::Perf().record_intent_dropped();
        return false;
    }
    return true;
}

// ---- Raid damage-school rule (PveOrder::SchoolHint) ----

namespace {

std::string SchoolNames(uint8 mask)
{
    static char const* const kNames[] = { "physical", "holy", "fire", "nature", "frost", "shadow", "arcane" };
    std::string out;
    for (uint8 b = 0; b < 7; ++b)
    {
        if (!(mask & (1u << b))) continue;
        if (!out.empty()) out += '+';
        out += kNames[b];
    }
    return out.empty() ? std::string("none") : out;
}

char const* SchoolSpellName(SpellInfo const* si)
{
    return si && si->SpellName ? (*si->SpellName)[LOCALE_enUS] : "?";
}

} // namespace

uint8 BotIntentEmitter::SpellSchoolMaskOf(uint32 spell_id, uint8 wand_school_mask)
{
    // sSpellMgr is a read-only store after load; the Classic APL helpers read
    // it from the bot workers the same way (ApClassicHeal.h).
    SpellInfo const* si = spell_id ? sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE) : nullptr;
    if (!si) return 0;
    // Spell::Spell: a wand user's ranged-attack spell deals the wand's school.
    // wand_school_mask is only set for wand users holding a ranged weapon.
    if (wand_school_mask && si->GetAttackType() == RANGED_ATTACK)
        return wand_school_mask;
    return uint8(si->GetSchoolMask() & SPELL_SCHOOL_MASK_ALL);
}

uint8 BotIntentEmitter::LockedSchools(BotSnapshot const& snap)
{
    constexpr uint32 SHADOWFORM = 15473;   // "unable to cast Holy spells while in this form"
    return BotSnapshotView(snap).has_aura(SHADOWFORM) ? uint8(SPELL_SCHOOL_MASK_HOLY) : uint8(0);
}

bool BotIntentEmitter::school_blocked(uint32 spell_id, ObjectGuid target)
{
    if (!school_snap_) return false;
    PveOrder const& po = school_snap_->pve_order;
    // Resolve the unit the spell lands on: the explicit target, or for a
    // target-less harmful single-target spell the bot's selection (what the
    // core falls back to). Self-centred AoE (no explicit unit) is not gated.
    ObjectGuid t = target;
    SpellInfo const* si = nullptr;
    auto info = [&]() -> SpellInfo const*
    {
        if (!si) si = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
        return si;
    };
    if (t.IsEmpty())
    {
        if (!info() || !si->NeedsExplicitUnitTarget()) return false;
        t = school_snap_->combat.current_target;
    }
    PveOrder::SchoolHint const* h = po.school_hint_for(t);
    if (!h || !h->avoid_mask) return false;
    // Only harmful spells: buffs, heals and dispels on allies never reach a
    // hinted enemy, and a positive spell carries no damage school.
    if (!info() || si->IsPositive()) return false;
    return (SpellSchoolMaskOf(spell_id, school_snap_->consumables.wand_school_mask) & h->avoid_mask) != 0;
}

void BotIntentEmitter::note_school_cast(uint32 spell_id, ObjectGuid target)
{
    if (!school_snap_ || !ai_) return;
    ObjectGuid const t = target.IsEmpty() ? school_snap_->combat.current_target : target;
    PveOrder::SchoolHint const* h = school_snap_->pve_order.school_hint_for(t);
    if (!h) return;
    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
    if (!si || si->IsPositive()) return;
    BotAI::RaidSchoolState& rs = ai_->raid_school();
    if (rs.log_target == t && rs.log_prefer == h->prefer_mask && rs.log_avoid == h->avoid_mask) return;
    rs.log_target = t;
    rs.log_prefer = h->prefer_mask;
    rs.log_avoid  = h->avoid_mask;
    uint8 const school = SpellSchoolMaskOf(spell_id, school_snap_->consumables.wand_school_mask);
    uint8 const locked = LockedSchools(*school_snap_);
    uint8 const usable_prefer = uint8(h->prefer_mask & ~h->avoid_mask & ~locked);
    std::string pick = SchoolSpellName(si);
    if (h->prefer_mask && !usable_prefer && (h->prefer_mask & ~h->avoid_mask & locked))
        pick += " (" + SchoolNames(uint8(h->prefer_mask & locked)) + " locked out by the bot's form)";
    else if (h->prefer_mask && !(school & usable_prefer))
        pick = "no spell of that school (" + pick + ")";
    TC_LOG_INFO("playerbot.v2", "[raid_school] {} on {}: prefer {} / avoid {} -> {}",
        school_snap_->identity.name, t.ToString(), SchoolNames(h->prefer_mask), SchoolNames(h->avoid_mask), pick);
}

uint8 BotIntentEmitter::pet_hold_mask(ObjectGuid target) const
{
    if (!school_snap_) return 0;
    PveOrder::SchoolHint const* h = school_snap_->pve_order.school_hint_for(target);
    if (!h) return 0;
    uint8 const pet_schools = school_snap_->pet.pet_damage_school_mask
        ? school_snap_->pet.pet_damage_school_mask : uint8(SPELL_SCHOOL_MASK_NORMAL);
    return uint8(h->avoid_mask & pet_schools);
}

bool BotIntentEmitter::hold_pet(ObjectGuid target, uint8 mask)
{
    if (!ai_) return false;
    BotAI::RaidSchoolState& rs = ai_->raid_school();
    if (rs.pet_hold_target == target && rs.pet_hold_mask == mask) return false;
    // Passive first so the follow sticks: a defensive / aggressive pet would
    // re-engage the mob as soon as it hits the pet or its owner.
    bool const passive = emit(HunterPetIntent{PetSetReactStateIntent{0 /*REACT_PASSIVE*/}});
    bool const follow  = emit(HunterPetIntent{PetSetCommandStateIntent{1 /*COMMAND_FOLLOW*/}});
    if (!passive || !follow) return passive || follow;   // retried next tick
    if (rs.pet_hold_target != target || rs.pet_hold_mask != mask)
        TC_LOG_INFO("playerbot.v2", "[raid_school] {} pet {} on {}: holds (avoid {})",
            school_snap_->identity.name, school_snap_->pet.pet_name, target.ToString(), SchoolNames(mask));
    rs.pet_hold_target = target;
    rs.pet_hold_mask   = mask;
    return true;
}

bool BotIntentEmitter::pet_attack(ObjectGuid target)
{
    if (uint8 const mask = pet_hold_mask(target))
        return hold_pet(target, mask);   // already held there: nothing pushed
    const bool pushed = emit(PetAttackIntent{target});
    // PetAttack sets the pet aggressive and on `target`: any hold is over.
    if (pushed && ai_)
    {
        ai_->raid_school().pet_hold_target = ObjectGuid::Empty;
        ai_->raid_school().pet_hold_mask   = 0;
    }
    return pushed;
}

bool BotIntentEmitter::pet_cast(uint32 spell_id, ObjectGuid target)
{
    if (school_snap_)
    {
        SpellInfo const* si = spell_id ? sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE) : nullptr;
        ObjectGuid t = target;
        if (t.IsEmpty() && si && si->NeedsExplicitUnitTarget())
            t = school_snap_->pet.pet_victim;
        if (si && !si->IsPositive())
            if (PveOrder::SchoolHint const* h = school_snap_->pve_order.school_hint_for(t))
                if ((si->GetSchoolMask() & h->avoid_mask) != 0)
                    return false;
    }
    return emit(PetCastSpellIntent{spell_id, target});
}

bool BotIntentEmitter::sync_school_hold(ObjectGuid victim)
{
    if (!school_snap_ || !ai_) return false;
    bool pushed_pet = false;
    // The pet: held off a hinted mob it is fighting (its own pick, an assist
    // or an earlier PetAttack); sent back to the victim once that hint clears.
    {
        PetState const& pet = school_snap_->pet;
        BotAI::RaidSchoolState& rs = ai_->raid_school();
        if (pet.pet_guid.IsEmpty() || !pet.pet_alive)
        {
            rs.pet_hold_target = ObjectGuid::Empty;   // a new pet starts unheld
            rs.pet_hold_mask   = 0;
        }
        else if (uint8 const mask = pet.pet_victim.IsEmpty() ? uint8(0) : pet_hold_mask(pet.pet_victim))
            pushed_pet = hold_pet(pet.pet_victim, mask);
        else if (!rs.pet_hold_target.IsEmpty() && pet_hold_mask(rs.pet_hold_target) == 0)
        {
            // The held target's hint cleared (or it died and left the
            // hints): send the pet back in on the victim, or, with no
            // victim, back to the defensive stance a pet gets on summon.
            ObjectGuid const back = rs.pet_hold_target;
            if (!victim.IsEmpty() && pet_hold_mask(victim) == 0)
                pushed_pet = pet_attack(victim);
            else
            {
                pushed_pet = emit(HunterPetIntent{PetSetReactStateIntent{1 /*REACT_DEFENSIVE*/}});
                if (pushed_pet)
                {
                    rs.pet_hold_target = ObjectGuid::Empty;
                    rs.pet_hold_mask   = 0;
                }
            }
            if (pushed_pet)
                TC_LOG_INFO("playerbot.v2", "[raid_school] {} pet {} on {}: hold over",
                    school_snap_->identity.name, pet.pet_name, back.ToString());
        }
    }
    if (victim.IsEmpty()) return pushed_pet;
    PveOrder::SchoolHint const* h = school_snap_->pve_order.school_hint_for(victim);
    uint8 const white = uint8(SPELL_SCHOOL_MASK_NORMAL | school_snap_->consumables.wand_school_mask);
    uint8 const want = h ? uint8(h->avoid_mask & white) : uint8(0);
    BotAI::RaidSchoolState const& rs = ai_->raid_school();
    if (rs.hold_target == victim && rs.hold_mask == want) return false;
    // A victim with nothing to hold and no hold of ours on it: the engage
    // that set it already left the swing as normal.
    if (rs.hold_target != victim && want == 0)
    {
        ai_->raid_school().hold_target = victim;
        ai_->raid_school().hold_mask   = 0;
        return false;
    }
    return start_attack(victim);
}

bool BotIntentEmitter::start_attack(ObjectGuid target)
{
    // Per-target lockout against the StartAttack ServerRefused loop. See
    // BotAI::kStartAttackLockoutMs. The lockout is set by the executor
    // when the API call returns ServerRefused (Player::Attack failed —
    // typically immune / phased / faction-locked target). Inside the
    // window the rule's emit becomes a silent no-op; outside, one retry
    // is allowed in case the underlying condition cleared.
    if (ai_)
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        if (ai_->start_attack_recently_refused(target.GetCounter(), now_ms))
            return false;
    }
    uint8 avoid = 0;
    if (school_snap_)
        if (PveOrder::SchoolHint const* h = school_snap_->pve_order.school_hint_for(target))
            avoid = h->avoid_mask;
    // A mob immune to every school (a banished add, a bubbled boss) is no target: nothing the bot swings or casts reaches it, and the
    // swing only pins the bot to it (MC Garr 2026-10-10: 202 "holding melee / Auto Shot" starts in 40 s on banished Firesworn while the raid
    // focus stood loose). The refusal leaves the bot targetless, which hands it the raid order's fallback target.
    if ((avoid & 0x7Fu) == 0x7Fu)
    {
        if (ai_ && school_snap_)
        {
            BotAI::RaidSchoolState& rs = ai_->raid_school();
            // Once a minute per bot and mob: the bot flips between the immune mob and its fallback target every tick (Rag's submerge,
            // MC 2026-10-12: 3807 lines in one fight), and the first line already says why (the [raid_school_why] line names the aura).
            // start_attack runs on the bot workers, so the pacing map is locked and bounded.
            static std::mutex sLoggedMutex;
            static std::unordered_map<uint64_t, uint32_t> sLogged;
            uint64_t const lkey = (school_snap_->guid.GetCounter() << 24) ^ target.GetCounter();
            uint32_t const lnow = school_snap_->published_at_ms;
            bool log_now = false;
            if (rs.hold_target != target)
            {
                std::lock_guard<std::mutex> lock(sLoggedMutex);
                if (sLogged.size() > 4096)
                    sLogged.clear();
                uint32_t& lat = sLogged[lkey];
                if (!lat || lnow - lat > 60000)
                {
                    lat = lnow;
                    log_now = true;
                }
            }
            if (log_now)
            {
                TC_LOG_INFO("playerbot.v2", "[raid_school] {} on {}: immune to every school -> not attacking it",
                    school_snap_->identity.name, target.ToString());
            }
            rs.hold_target = target;
            rs.hold_mask   = 0;
        }
        return false;
    }
    const bool pushed = emit(StartAttackIntent{target, avoid});
    if (pushed && ai_ && school_snap_)
    {
        static std::mutex sOpenLogMutex;
        static std::unordered_map<uint64_t, uint32_t> sOpenLog;
        uint32_t const onow = school_snap_->published_at_ms;
        uint64_t const okey = (school_snap_->guid.GetCounter() << 24) ^ target.GetCounter();
        bool open_log = false;
        {
            std::lock_guard<std::mutex> lock(sOpenLogMutex);
            if (sOpenLog.size() > 4096)
                sOpenLog.clear();
            uint32_t& last = sOpenLog[okey];
            if (!last || onow - last > 10000u)
            {
                last = onow;
                open_log = true;
            }
        }
        if (open_log)
        {
            int in_combat = -1, victim_kind = -1;
            for (NearbyUnit const& u : school_snap_->combat.nearby_enemies)
                if (u.guid == target) { in_combat = u.unit_in_combat ? 1 : 0; victim_kind = int(u.victim_kind); break; }
            TC_LOG_INFO("playerbot.v2", "[attack_start] {} attacks {} stance {} last_rule {} mob in_combat {} victim_kind {}",
                        school_snap_->identity.name, target.ToString(), int(ai_->owner_stance()), ai_->last_rule_fired() ? ai_->last_rule_fired() : "none", in_combat, victim_kind);
        }
    }
    if (pushed && ai_ && school_snap_)
    {
        BotAI::RaidSchoolState& rs = ai_->raid_school();
        uint8 const white = uint8(SPELL_SCHOOL_MASK_NORMAL | school_snap_->consumables.wand_school_mask);
        uint8 const held  = uint8(avoid & white);
        if (held && (rs.hold_target != target || rs.hold_mask != held))
            TC_LOG_INFO("playerbot.v2", "[raid_school] {} on {}: avoid {} -> holding melee / Auto Shot / wand",
                school_snap_->identity.name, target.ToString(), SchoolNames(avoid));
        rs.hold_target = target;
        rs.hold_mask   = held;
    }
    return pushed;
}

bool BotIntentEmitter::move_already_emitted(float x, float y, float z) const
{
    return ai_ && ai_->move_to_recently_emitted(x, y, z, GameTime::GetGameTimeMS());
}

bool BotIntentEmitter::move_to(float x, float y, float z, bool run, bool direct, bool drop)
{
    // Per-bot dedup: skip if the previous move_to went to ~the same XYZ
    // within the lockout window. See BotAI::kMoveToEmitLockoutMs / the long
    // comment on note_move_to_emitted for the rationale (spline thrash,
    // Detour mid-curve reset, visible stutter). Applies to direct moves too
    // (a committed link crossing re-asserts the same exit every tick; the
    // dedup holds the running straight spline instead of restarting it).
    if (ai_)
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        if (ai_->move_to_recently_emitted(x, y, z, now_ms))
        {
            // Why a move_to returned false (pull back "refused" on flat ground, Deadmines 2026-10-15): the dedup, not the navmesh.
            // Per bot: one global throttle was used up by the 300+ dedups of background bots, and the squad's lines never showed.
            if (ai_->dedup_log_due(now_ms, 1000u))
                TC_LOG_INFO("playerbot.v2",
                    "[move_to_refused] bot={} dst=({:.1f},{:.1f},{:.1f}) direct={} reason=dedup: a move to within 3 y was emitted in the last 1.5 s",
                    ai_->bot_id(), x, y, z, direct ? 1 : 0);
            return false;
        }
        // REFUSAL GUARD (emitter-level on purpose). PlayerbotAPI::move_to
        // keeps a per-destination path-fail backoff: one failed pathfind
        // poisons that dst, every later move_to to ~it returns Locked
        // WITHOUT issuing a spline, and a rule that re-selects the same dst
        // re-arms the backoff forever -> permanent freeze over a VALID
        // navmesh (live: RFK/Gnomeregan/Shadow Labyrinth/Arcatraz/Stonecore;
        // headless probes of those corridors come back COMPLETE).
        // The first cut of this fix guarded 12 rule-level step sites and the
        // route leapfrog scan — and the live acceptance test showed ZERO
        // [step_refused] lines, because the route follower's COMMITTED steer
        // emits through its own path and bypassed all of them. Placing the
        // check HERE is the robust placement: every caller funnels through
        // this one function, so no emit path can bypass it. Returning false
        // is exactly what an emitter-deduped call already returns, so callers
        // that test the result take their existing "not emitted" branch and
        // fall through to their next candidate.
        // Bounded: entries live only kMoveRefusedTtlMs (6s), so a dst that
        // merely had a transient failure becomes selectable again.
        if (!direct && !drop && ai_->move_refused_recently(x, y, z, now_ms))
        {
            static uint32 s_refused_dbg_ms = 0;
            if (now_ms - s_refused_dbg_ms > 2000u)
            {
                s_refused_dbg_ms = now_ms;
                TC_LOG_INFO("playerbot.v2",
                    "[emit_refused] bot={} dst=({:.1f},{:.1f},{:.1f}) "
                    "(recently refused; caller must pick another target)",
                    ai_->bot_id(), x, y, z);
            }
            return false;
        }
        const bool pushed = emit(MoveToIntent{x, y, z, run, direct, drop});
        if (pushed)
            ai_->note_move_to_emitted(x, y, z, now_ms);
        else
        {
            if (ai_->dedup_log_due(now_ms, 1000u))
                TC_LOG_INFO("playerbot.v2",
                    "[move_to_refused] bot={} dst=({:.1f},{:.1f},{:.1f}) direct={} reason=intent queue refused the push",
                    ai_->bot_id(), x, y, z, direct ? 1 : 0);
        }
        return pushed;
    }
    return emit(MoveToIntent{x, y, z, run, direct, drop});
}

// The global cooldown a cast of this spell starts, in ms (0 = off the GCD: stances,
// racials, item-less instants the data puts outside it). Read from the spell data.
static uint32 GcdLengthMs(uint32 spell_id)
{
    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
    if (!si || si->StartRecoveryTime <= 0)
        return 0;
    return uint32(si->StartRecoveryTime);
}

void BotIntentEmitter::hold_casts_for_mana_item()
{
    if (ai_)
        ai_->note_mana_break(GameTime::GetGameTimeMS());
}

void BotIntentEmitter::end_mana_break()
{
    if (ai_)
        ai_->clear_mana_break();
}

// A spell with a cast time: the kind a running cast of the same bot would have to finish first.
static bool HasCastTime(uint32 spell_id)
{
    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
    return si && si->CalcCastTime() > 0;
}

bool BotIntentEmitter::cast(uint32 spell_id, ObjectGuid target)
{
    if (ai_ && ai_->mana_break_active(GameTime::GetGameTimeMS()) && HasCastTime(spell_id))
        return false;
    // Optimistic per-spell emit dedup. The snapshot's spell_cooldowns
    // table reflects server-side cooldowns one world-tick after the cast
    // lands; in the ~200-1000ms gap, the bot worker may re-tick on the
    // same stale snapshot, see "is_ready=true" for a spell whose cast
    // just landed, and re-emit. Server rejects with SPELL_FAILED_NOT_READY
    // (91) — every rejection logs to Server.log and burns one intent slot.
    //
    // Solution: track per-spell emit timestamps in BotAI; drop duplicate
    // emits within kCastEmitLockoutMs (~1.5s, > snapshot cadence). The
    // next snapshot's real cooldown table picks up before the lockout
    // expires, so the bot logic never sees a "ready" window the server disagrees
    // with.
    //
    // Skipped when ai_ is null (test harness etc.) — the dedup is
    // strictly an optimization; correctness comes from the snapshot CDs.
    // Raid school rule: an avoided school at a hinted enemy is not sent.
    if (school_blocked(spell_id, target))
        return false;
    if (ai_)
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        if (ai_->cast_recently_emitted(spell_id, now_ms))
            return false;
        uint32 const gcd_ms = GcdLengthMs(spell_id);
        if (gcd_ms && ai_->gcd_recently_started(now_ms))
            return false;
        const bool pushed = emit(CastSpellIntent{spell_id, target});
        if (pushed)
        {
            ai_->note_cast_emitted(spell_id, now_ms);
            if (gcd_ms) ai_->note_gcd_started(now_ms, gcd_ms);
            note_school_cast(spell_id, target);
        }
        return pushed;
    }
    return emit(CastSpellIntent{spell_id, target});
}

bool BotIntentEmitter::cast_rank(uint32 spell_id, ObjectGuid target)
{
    if (ai_ && ai_->mana_break_active(GameTime::GetGameTimeMS()) && HasCastTime(spell_id))
        return false;
    // Same raid school gate and per-spell emit dedup as cast(); ranks have their own ids.
    if (school_blocked(spell_id, target))
        return false;
    if (ai_)
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        if (ai_->cast_recently_emitted(spell_id, now_ms))
            return false;
        uint32 const gcd_ms = GcdLengthMs(spell_id);
        if (gcd_ms && ai_->gcd_recently_started(now_ms))
            return false;
        const bool pushed = emit(CastSpellIntent{spell_id, target, true});
        if (pushed)
        {
            ai_->note_cast_emitted(spell_id, now_ms);
            if (gcd_ms) ai_->note_gcd_started(now_ms, gcd_ms);
            note_school_cast(spell_id, target);
        }
        return pushed;
    }
    return emit(CastSpellIntent{spell_id, target, true});
}

bool BotIntentEmitter::ah_buyout(ObjectGuid auctioneer, uint32 auction_id, uint64 price)
{
    // Per-auction_id dedup so the buy-side economy rule can fire every tick
    // it wants the listing without double-spending: one buyout emit per
    // auction per 30s window. Inside the window the emit is a silent no-op;
    // once the executor settles (Result::Ok) the snapshot's on-demand AH
    // scan drops the consumed listing so the rule naturally stops re-emitting.
    if (ai_)
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        if (ai_->action_recently_tried(BotAI::ActionKind::AhBuyout, auction_id, now_ms))
            return false;
        const bool pushed = emit(EconomyIntent{EconomyOp::AhBuyout{auctioneer, auction_id, price}});
        if (pushed)
            ai_->note_action_retry(BotAI::ActionKind::AhBuyout, auction_id, now_ms);
        return pushed;
    }
    return emit(EconomyIntent{EconomyOp::AhBuyout{auctioneer, auction_id, price}});
}

bool BotIntentEmitter::ah_bid(ObjectGuid auctioneer, uint32 auction_id, uint64 bid)
{
    if (ai_)
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        if (ai_->action_recently_tried(BotAI::ActionKind::AhBid, auction_id, now_ms))
            return false;
        const bool pushed = emit(EconomyIntent{EconomyOp::AhBid{auctioneer, auction_id, bid}});
        if (pushed)
            ai_->note_action_retry(BotAI::ActionKind::AhBid, auction_id, now_ms);
        return pushed;
    }
    return emit(EconomyIntent{EconomyOp::AhBid{auctioneer, auction_id, bid}});
}

bool BotIntentEmitter::ah_buy_commodity(ObjectGuid auctioneer, uint32 item_entry,
                                        uint32 quantity, uint64 max_total)
{
    // Per-item_entry dedup (AhBuyCommodity 30s). Unlike AhBuyout/AhBid the key
    // is the reagent ENTRY, not an auction_id: commodity listings collapse into
    // one bucket, so the buy-side rule wants one purchase per wanted reagent per
    // 30s window. Inside the window the emit is a silent no-op; once the
    // executor settles (Result::Ok) the on-demand AH scan re-derives the bot's
    // remaining shortfall so the rule naturally stops re-emitting for a topped-
    // up reagent.
    if (ai_)
    {
        const uint32 now_ms = GameTime::GetGameTimeMS();
        if (ai_->action_recently_tried(BotAI::ActionKind::AhBuyCommodity, item_entry, now_ms))
            return false;
        const bool pushed = emit(EconomyIntent{EconomyOp::AhBuyCommodity{auctioneer, item_entry, quantity, max_total}});
        if (pushed)
            ai_->note_action_retry(BotAI::ActionKind::AhBuyCommodity, item_entry, now_ms);
        return pushed;
    }
    return emit(EconomyIntent{EconomyOp::AhBuyCommodity{auctioneer, item_entry, quantity, max_total}});
}

} // namespace Playerbot
