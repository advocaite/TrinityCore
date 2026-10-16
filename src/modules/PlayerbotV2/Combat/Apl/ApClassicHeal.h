// ApClassicHeal.h - Classic 1.60 healer logic shared by the vanilla healer
// rotations (Apl_Classic_Priest / Druid / Paladin / Shaman) and the
// out-of-combat top-up (State_Idle MaintainOocHeal).
//
// Port of Karrz's ike3 vanilla healer (wow-1.12 PlayerBots):
//   - PickPatient: values/PartyMemberToHeal.cpp Calculate + MCRaidPolicy
//     HealScore. Candidates within heal range on the bot's map; members out
//     of line of sight are skipped unless they are a tank or below 35% (the
//     healer must not freeze on a target it cannot reach); health is
//     projected through the melee swings already landing on the member
//     ("preheal", CombatBotBaseAI::FindAndPreHealTarget); tanks score higher
//     (+35 under 70%, +10 otherwise, +10 while a boss hits them), the
//     coordinator's heal_focus gets +20, emergencies below 35% outrank
//     everything, and a member another healer is already casting a direct
//     heal on scores -30 so healers spread instead of stacking.
//   - CastHeal: actions/GenericSpellActions.cpp ExecuteTunedHeal /
//     PlanDirectHeal. The cheapest rank whose heal covers ~90% of the
//     deficit + incoming damage (75% below 40% mana), never smaller than a
//     third of the top rank; the top rank below 35% health or on a tank
//     under pressure; no cast at all when the deficit is tiny next to the
//     smallest sensible rank (overheal), and no direct top-up heals on
//     members at 70%+ (tanks 85%+) while the healer is below 30% mana.
//   - AoE heals need 3 injured members (TriggerContext "medium aoe heal");
//     chain heals go to the best cluster (GenericSpellActions
//     CountInjuredAround).
//   - DispelTarget: values/PartyMemberToDispel.cpp / CombatBotBaseAI
//     SelectDispelTarget. Members within 30 yd and in line of sight, picked
//     by bot guid modulo the candidate count so several healers spread over
//     the debuffed members; the bot itself last.
// Rank chains come from SpellMgr (vanilla rank 1 ids resolve through
// BotSnapshotView::resolve_rank). Pure functions of the snapshots: safe on
// the bot worker threads.
//
// Mana efficiency (RaidHealing: vanilla realm, inside a raid or 5-man instance map, not a
// battleground; open-world parties and solo play keep the rules above):
//   - Ranks are sized with the bot's bonus healing (ExpectedHeal: base points
//     + heal_bonus x the effect's bonus coefficient, as Unit::
//     SpellHealingBonusDone adds it) and against more of the deficit the
//     fuller the healer's mana (100% / 90% / 75% at 70% / 40% mana).
//   - Spell choice (RaidChoose / RaidPriestHeal): the fast or big heal (Flash
//     Heal, Greater Heal, Holy Light, Lesser Healing Wave, Regrowth) only for
//     an emergency or a tank under half health; everyone else gets the
//     efficient one (Heal rank 2-4, Flash of Light, downranked Healing Wave /
//     Healing Touch).
//   - Boss fight below 25% mana: no heal on anyone above 80%, HoTs only on
//     tanks (HotAllowed).
//   - Overheal cancel lockout (HealTiming): State_InCombat CancelOverheal
//     stops a heal in its last half-second when the target is topped up; the
//     same target is not started on again for kCancelLockoutMs (a patient in
//     an emergency excepted), and the tank pre-heal
//     (State_InCombat PreemptiveTankHeal) waits that long after any cancel.
//   - [raid_heal] logs: spell / rank changes (per bot, 3 s pacing), cancels,
//     pre-heals and the low-mana holds.

#pragma once

#include "../ApRotation.h"
#include "Bot/SpellFamily.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/ClassTables.h"
#include "Bot/ClassicUpkeep.h"
#include "Bot/RezClaims.h"      // IsRezSpell, rez cast refusals
#include "Bot/RezRule.h"        // RezRank, RezFightLive
#include "Bot/ThreatDiscipline.h"
#include "Bot/Raid/RaidEncounters.h"   // Raid::ThreatShareAfterDrop
#include "Bot/Raid/RaidHealStats.h"
#include "Bot/Raid/RaidManaLedger.h"
#include "Bot/ClassicHealScope.h"
#include "Bot/ClassicHealCover.h"
#include "HealClaimStore.h"
#include "Group/GroupSnapshot.h"
#include "Services.h"
#include "Log.h"
#include "Util/ConfigReader.h"   // Playerbot.Behavior.HealClaims
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Playerbot::Combat {

namespace ClassicHeal {

constexpr float kHealRange      = 40.0f;   // the owner: HealDistance 38 + slack for the snapshot cadence
constexpr float kDispelRange    = 30.0f;
constexpr float kMeleeReach     = 8.0f;    // enemy-to-member distance that counts as "swinging at"
constexpr int32 kEmergencyPct   = 35;      // HEAL_DOWNRANK_MIN_PCT / LOS exemption
constexpr int32 kAlmostFullPct  = 90;      // AiPlayerbot.AlmostFullHealth
constexpr int32 kHealGroupStayPct = 50;    // a triage healer leaves its subgroups for an emergency only while its own best patient is at or above this
constexpr int32 kMediumPct      = 70;      // AiPlayerbot.MediumHealth (AoE heal trigger)
constexpr int32 kLowManaPct     = 30;      // no top-up heals below this
constexpr int32 kSaveManaPct    = 40;      // AiPlayerbot.MediumMana: 75% deficit sizing
constexpr int32 kRaidTankTopUpPct    = 93; // raid: only a tank's own healers heal a tank at or above this
constexpr int32 kRaidTankFavourPct   = 80; // raid: a tank at or above this is ranked like any other member
constexpr int32 kRaidTankBigHealPct  = 50; // raid: the fast / big heal for a tank under this
constexpr int32 kRaidLowManaPct      = 25; // raid boss fight: below this mana ...
constexpr int32 kRaidLowManaCeilPct  = 80; // ... nobody above this is healed, HoTs only on tanks
constexpr uint32 kCancelLockoutMs    = 1500;
// Raid: no top-off on a tank at or above kRaidTankSpikeFloorPct unless damage is spiking (the swings on it would take it under
// kRaidTankSpikePct) - not even for the tank's own healers (MC Garr 2026-10-10: paladins cast Flash of Light on tanks at 80-99 %,
// one went from 96 % to 30 % mana in a minute).
constexpr int32 kRaidTankSpikeFloorPct = 90;
constexpr int32 kRaidTankSpikePct      = 75;
// Raid boss fight: below kRaidReservePct mana a healer keeps what it has for real trouble, healing nobody at or above these
// (a tank kRaidReserveTankPct) except to a spike (projected under kEmergencyPct).
constexpr int32 kRaidReservePct        = 45;
constexpr int32 kRaidReserveRaidPct    = 65;
constexpr int32 kRaidReserveTankPct    = 75;

struct Patient
{
    ObjectGuid guid;
    int32      hp_pct    = 100;   // projected: health after the incoming swings
    int32      actual_pct = 100;
    int64      hp        = 0;
    int64      max_hp    = 0;
    int64      incoming  = 0;     // summed melee swings on the unit
    int64      inflight  = 0;     // raid: other healers' direct heals already on their way (ApplyInFlight)
    uint32     inflight_land_ms = 0;   // ... and when the first of them lands (published_at_ms clock)
    bool       tank      = false;
    bool       valid     = false; // false: the bot itself as a "nobody needs healing" fallback
};

inline float Dist2(float ax, float ay, float az, float bx, float by, float bz)
{
    float const dx = ax - bx, dy = ay - by, dz = az - bz;
    return dx * dx + dy * dy + dz * dz;
}

inline int32 ManaPct(BotSnapshotView const& s)
{
    return s.max_power(0) > 0 ? s.power_pct(0) : 100;
}

// The mana-efficiency rules apply (see the header comment): any group inside a raid or 5-man instance map
// (Bot/ClassicHealScope.h). The name is the older "raid" one.
inline bool RaidHealing(BotSnapshotView const& s)
{
    return EfficiencyRulesApply(s.is_in_raid(), s.is_in_dungeon(), s.in_battleground(), IsVanillaRealm());
}

// Playerbot.Behavior.HealClaims (default on): the raid in-flight heal dedupe, the HoT claims and
// the pre-heal's covered check. Off: every healer picks and sizes from current health alone.
inline bool HealClaimsOn()
{
    return Services::Config().behavior_heal_claims();
}

// Per-bot heal timing shared by the rotation and State_InCombat (the
// overheal cancel lockout and the [raid_heal] rank-change log). Keyed by the
// bot's guid; one small entry per healer bot.
struct HealTiming
{
    uint32     cancel_ms = 0;     // last overheal cancel (published_at_ms)
    ObjectGuid cancel_target;     // the patient that cancel was on
    uint32     logged_spell = 0;  // last spell id (exact rank) logged by NoteHealChoice
    uint32     logged_ms = 0;
    uint32     need_ms = 0;       // raid healer: last time its assignment needed healing (RaidHealerMayDamage)
    uint32     need_eval_ms = 0;  // ... and the last time that was checked
    uint32     pick_logged_ms = 0; // last NoteRaidPick line
    uint64     dispel_key = 0;     // last assigned dispel cast (member + debuff type) ...
    uint32     dispel_ms = 0;      // ... and when (published_at_ms clock)
};

template <typename F>
auto WithHealTiming(BotSnapshotView const& s, F&& f)
{
    static std::mutex mu;
    static std::unordered_map<uint64, HealTiming> timing;
    std::lock_guard<std::mutex> lock(mu);
    return f(timing[s.guid().GetCounter()]);
}

inline void NoteHealCancel(BotSnapshotView const& s, ObjectGuid target)
{
    WithHealTiming(s, [&](HealTiming& t) { t.cancel_ms = s.published_at_ms(); t.cancel_target = target; return 0; });
}

// Within the lockout after this bot's last overheal cancel; with a target,
// only when that cancel was on it.
inline bool InCancelLockout(BotSnapshotView const& s, ObjectGuid target = ObjectGuid::Empty)
{
    uint32 const now = s.published_at_ms();
    return WithHealTiming(s, [&](HealTiming& t)
    {
        if (!t.cancel_ms || now - t.cancel_ms >= kCancelLockoutMs) return false;
        return target.IsEmpty() || t.cancel_target == target;
    });
}

// Direct heal (SPELL_EFFECT_HEAL) - what another healer's cast must be to
// count as "receiving" (PartyMemberToHeal IsTargetOfHealingSpell).
inline bool IsDirectHealSpell(uint32 spell)
{
    SpellInfo const* si = spell ? sSpellMgr->GetSpellInfo(spell, DIFFICULTY_NONE) : nullptr;
    if (!si) return false;
    for (SpellEffectInfo const& eff : si->GetEffects())
        if (eff.IsEffect(SPELL_EFFECT_HEAL))
            return true;
    return false;
}

inline bool IsTank(BotSnapshotView const& s, GroupMemberSummary const& m)
{
    auto const& po = s.raw().pve_order;
    return m.role == Role::Tank || (po.active && !po.main_tank.IsEmpty() && po.main_tank == m.guid);
}

// Sum of the melee swings of live enemies currently attacking `guid` and
// standing within melee reach of it (getIncomingdamage).
inline int64 IncomingMelee(BotSnapshotView const& s, ObjectGuid guid, float x, float y, float z)
{
    int64 dmg = 0;
    for (NearbyUnit const& u : s.nearby_enemies())
    {
        if (u.hp <= 0 || u.victim != guid || u.melee_swing <= 0.f) continue;
        if (Dist2(u.x, u.y, u.z, x, y, z) > kMeleeReach * kMeleeReach) continue;
        dmg += int64(u.melee_swing);
    }
    return dmg;
}

inline bool BossOn(BotSnapshotView const& s, ObjectGuid guid)
{
    for (NearbyUnit const& u : s.nearby_enemies())
        if (u.hp > 0 && (u.is_dungeon_boss || u.is_boss_rank) && u.victim == guid)
            return true;
    return false;
}

// Another group member (not the bot) is casting a direct heal on `guid`.
inline bool ReceivingHeal(BotSnapshotView const& s, GroupSnapshotView const& g, ObjectGuid guid)
{
    auto const* members = g.members();
    if (!members) return false;
    for (GroupMemberSummary const& m : *members)
        if (m.guid != s.guid() && m.is_casting && m.casting_target == guid && IsDirectHealSpell(m.casting_spell))
            return true;
    return false;
}

// Line of sight from the bot, from the nearby_friends scan. A member the
// scan did not reach (12-unit cap) counts as visible.
inline bool InLos(BotSnapshotView const& s, ObjectGuid guid)
{
    if (guid == s.guid()) return true;
    for (NearbyUnit const& u : s.nearby_friends())
        if (u.guid == guid)
            return u.in_los;
    return true;
}

inline Patient MakePatient(BotSnapshotView const& s, ObjectGuid guid, int64 hp, int64 max_hp,
                           float x, float y, float z, bool tank)
{
    Patient p;
    p.guid   = guid;
    p.hp     = hp;
    p.max_hp = max_hp;
    p.tank   = tank;
    p.valid  = true;
    if (max_hp <= 0)
        return p;
    // Swings are pre-armour; a tank in plate (or bear form) takes roughly half of that.
    int64 const swings = IncomingMelee(s, guid, x, y, z);
    p.incoming   = std::min<int64>(tank ? swings / 2 : swings, hp);
    p.actual_pct = int32(hp * 100 / max_hp);
    p.hp_pct     = int32((hp - p.incoming) * 100 / max_hp);
    return p;
}

// The bot itself as a patient.
inline Patient SelfPatient(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    bool tank = false;
    if (auto const* members = g.members())
        for (GroupMemberSummary const& m : *members)
            if (m.guid == s.guid()) { tank = IsTank(s, m); break; }
    float x, y, z; s.position(x, y, z);
    return MakePatient(s, s.guid(), s.hp(), s.max_hp(), x, y, z, tank);
}

// Patient data for a specific unit (the bot or a group member).
inline Patient PatientFor(BotSnapshotView const& s, GroupSnapshotView const& g, ObjectGuid guid)
{
    if (guid.IsEmpty() || guid == s.guid())
        return SelfPatient(s, g);
    if (auto const* members = g.members())
        for (GroupMemberSummary const& m : *members)
            if (m.guid == guid)
                return MakePatient(s, m.guid, m.hp, m.max_hp, m.x, m.y, m.z, IsTank(s, m));
    return {};
}

// RaidLogDue channels of the raid healing logs (low byte of the key, as
// RaidLogKey builds it; ThreatDiscipline.h RaidLogKind leaves 19-31 free).
enum class HealLogKind : uint8 { LowMana = 24, PreHeal = 25, Covered = 26 };
inline uint64 HealLogKey(BotSnapshotView const& s, HealLogKind k)
{
    return (uint64(s.guid().GetCounter()) << 8) | uint64(k);
}

// ---- Heals in flight (raid healing) -------------------------------------
// Several raid healers used to start the same heal on the same member at once
// (MC 2026-10-06, Molten Giant: three priests Flash Heal on one 31% member,
// two druids Regrowth on another), each one blind to the others. A raid
// healer now counts the direct heals already on their way to a patient:
//   - casts the group snapshot shows (GroupMemberSummary casting_spell /
//     casting_target / casting_end_ms, filled on the world thread), sized
//     conservatively with the caster's base rank heal (RankHeal at its level:
//     its bonus healing is not in the summary) unless that healer's own claim
//     below names the same spell, which carries its ExpectedHeal;
//   - claims: every raid heal EmitHeal sends is recorded here (healer ->
//     patient, spell, expected heal, start / land time) the moment the rule
//     emits it, under one mutex. A claim counts for kHealClaimGraceMs on its
//     own - the window in which the group snapshot cannot show the cast yet
//     (the intent executes on the next world tick, the group view is up to
//     50 ms old) - and after that only while the snapshot shows the cast.
// The claim check and the claim itself happen under the same lock in
// EmitHeal, so of two healers whose rules picked the same patient in the
// same tick (their snapshots showed nobody casting) exactly one sends the
// heal when it covers the deficit; the other sends nothing this tick and its
// next PickPatient sees the claim and moves on. That is simpler and more
// robust than a guid tie-break, which would need every healer to predict the
// others' picks from snapshots it does not have.
// PickPatient projects the in-flight amount onto each patient's health and
// skips one whose deficit it covers (a critical patient, kHealCriticalPct, needs
// kHealCriticalCover times the deficit in flight; a healer's own tank under half
// health kHealTankTroubleCover times it: the tank's healers are not exempt, only
// asked for a margin).
// Copy of the live claims for one PickPatient pass (one lock, not one per candidate).
inline HealClaims CopyHealClaims(uint32 now)
{
    return WithHealClaims(now, [](std::unordered_map<uint64, HealClaim> const& c)
    {
        return HealClaims(c.begin(), c.end());
    });
}

inline float RankHeal(SpellInfo const* si, uint32 level);

// Healing-taken multiplier of a group member from its debuffs (Gehennas' Curse, Mortal Strike, Wound Poison, Veil of Shadow ...),
// read from the spells' MOD_HEALING_PCT effects, floored at 0.1. 1.0 = no reduction. Under -75% every heal lands a quarter: the
// in-flight cover and the rank choice must count that, or the healers see a tank as "covered" and leave it to die.
inline float HealTakenFactor(GroupSnapshotView const& g, ObjectGuid guid)
{
    auto const* members = g.members();
    if (!members) return 1.0f;
    for (GroupMemberSummary const& m : *members)
    {
        if (m.guid != guid) continue;
        float f = 1.0f;
        for (AuraEntry const& a : m.debuffs)
        {
            if (!a.is_harmful) continue;
            SpellInfo const* si = sSpellMgr->GetSpellInfo(a.spell_id, DIFFICULTY_NONE);
            if (!si) continue;
            for (SpellEffectInfo const& eff : si->GetEffects())
                if (eff.IsAura(SPELL_AURA_MOD_HEALING_PCT) && eff.BasePoints < 0.0f)
                    f *= std::max(0.0f, 1.0f + eff.BasePoints / 100.0f);
        }
        return std::max(f, 0.1f);
    }
    return 1.0f;
}

// Other healers' direct heals in flight on `p` (see the block comment); fills
// p.inflight / p.inflight_land_ms and projects p.hp_pct with them. Raid only.
template <typename Claims>
void ApplyInFlight(BotSnapshotView const& s, GroupSnapshotView const& g, Patient& p, Claims const& claims)
{
    p.inflight = 0;
    p.inflight_land_ms = 0;
    if (!p.valid || p.max_hp <= 0 || p.guid.IsEmpty())
        return;
    uint32 const now = s.published_at_ms();
    uint64 const self = s.guid().GetCounter();
    float amount = 0.0f;
    bool any = false;
    auto add = [&](float amt, uint32 land)
    {
        amount += amt;
        if (!any || int32(land - p.inflight_land_ms) < 0)
            p.inflight_land_ms = land;
        any = true;
    };
    auto claim_of = [&](uint64 healer) -> HealClaim const*
    {
        for (auto const& [h, c] : claims)
            if (h == healer) return &c;
        return nullptr;
    };
    uint64 seen[40];
    int n_seen = 0;
    if (auto const* members = g.members())
        for (GroupMemberSummary const& m : *members)
        {
            if (m.guid.GetCounter() == self || !m.is_casting || m.casting_target != p.guid || !IsDirectHealSpell(m.casting_spell))
                continue;
            float amt = RankHeal(sSpellMgr->GetSpellInfo(m.casting_spell, DIFFICULTY_NONE), m.level);
            if (HealClaim const* c = claim_of(m.guid.GetCounter()); c && c->patient == p.guid && c->spell == m.casting_spell)
                amt = std::max(amt, c->amount);
            add(amt, m.casting_end_ms ? m.casting_end_ms : now);
            if (n_seen < 40) seen[n_seen++] = m.guid.GetCounter();
        }
    for (auto const& [healer, c] : claims)
    {
        if (healer == self || c.patient != p.guid || int32(now - c.start_ms) >= int32(kHealClaimGraceMs))
            continue;
        if (std::find(seen, seen + n_seen, healer) != seen + n_seen)
            continue;   // already counted from the group snapshot
        add(c.amount, c.land_ms);
    }
    amount *= HealTakenFactor(g, p.guid);   // a healing-reduced patient gets a fraction of what is on the way
    p.inflight = any ? int64(amount) : 0;
    if (!any)
        p.inflight_land_ms = 0;
    // From the raw health each time (as MakePatient), so a patient handed in
    // already projected is never projected twice.
    int64 const projected = std::min<int64>(p.hp - p.incoming + p.inflight, p.max_hp);
    p.hp_pct = int32(std::max<int64>(projected, 0) * 100 / p.max_hp);
}

// The heals in flight cover `p`'s deficit (health missing + incoming swings) times `margin` (the tank's own healers while it is in
// trouble ask for a margin: damage goes on while the heals cast). A critical patient asks for kHealCriticalCover times the deficit
// whatever lands when: a heal beyond that is mana, not life (MC Garr 2026-10-10: up to 9 healers started a heal on one tank within
// one second; the tank's own healers were exempt from this check while it was under half health, which is when it matters most).
inline int64 HealDeficit(Patient const& p) { return HealDeficitOf(p.max_hp, p.hp, p.incoming); }
inline bool CoveredByInFlight(BotSnapshotView const& s, Patient const& p, float margin = 1.0f)
{
    return CoveredByHealsInFlight(p.max_hp, p.hp, p.incoming, p.inflight, p.actual_pct, margin, s.in_combat());
}

inline void NoteCoveredSkip(BotSnapshotView const& s, Patient const& p, char const* where)
{
    if (!RaidLogDue(HealLogKey(s, HealLogKind::Covered), s.published_at_ms(), 3000u))
        return;
    TC_LOG_INFO("playerbot.v2",
        "[raid_heal] {} leaves {}{} at {}% to heals in flight ({}): {} incoming heal for a deficit of {}, first lands in {} ms",
        s.name(), p.guid.GetCounter(), p.tank ? " (tank)" : "", p.actual_pct, where, p.inflight,
        p.max_hp - p.hp + p.incoming, std::max<int32>(int32(p.inflight_land_ms - s.published_at_ms()), 0));
}

// MCRaidPolicy::HealScore.
// raid_share: a raid healer shares its attention. A tank is only favoured while it is in real
// trouble (under kRaidTankFavourPct); above that its score is the plain missing-health score, so a
// raid member in worse shape than the tank wins, and a tank healer whose tank is comfortable heals
// the raid by lowest HP%. Tanks that drop below it keep their priority.
inline float HealScore(int32 pct, bool tank, bool assigned, bool receiving, bool boss_target, bool raid_share = false)
{
    if (pct >= 99) return -1000.0f;
    float score = pct < kEmergencyPct ? 1000.0f - float(pct) * 10.0f : 100.0f - float(pct);
    if (tank && !(raid_share && pct >= kRaidTankFavourPct))
        score += (pct < 70 ? 35.0f : 10.0f) + (boss_target ? 10.0f : 0.0f);
    if (assigned && !(raid_share && tank && pct >= kRaidTankFavourPct)) score += 20.0f;
    if (pct >= kEmergencyPct && receiving) score -= 30.0f;
    return score;
}

// [raid_heal] who this healer is working on: the target's role and whether it is the healer's own tank,
// paced to one line per healer per 5 s, so the digest shows the tank / raid split.
inline void NoteRaidPick(BotSnapshotView const& s, GroupSnapshotView const& g, Patient const& p, ObjectGuid focus)
{
    uint32 const now = s.published_at_ms();
    uint32 const prev = WithHealTiming(s, [&](HealTiming& t) -> uint32
    {
        uint32 const was = t.pick_logged_ms;
        if (was && now - was < 5000u) return was;
        t.pick_logged_ms = now;
        return 0u;
    });
    if (prev)
        return;
    uint32 tanks = 0, raid_members = 0;
    if (auto const* members = g.members())
        for (GroupMemberSummary const& m : *members)
        {
            if (!m.online || !m.is_alive || m.max_hp <= 0 || m.hp_pct() >= kAlmostFullPct) continue;
            (IsTank(s, m) ? tanks : raid_members)++;
        }
    TC_LOG_INFO("playerbot.v2",
        "[raid_heal] {} works on {} ({}, {}% projected {}%){}: hurt tanks {} raid {}",
        s.name(), p.guid.GetCounter(), p.tank ? "tank" : "raid", p.actual_pct, p.hp_pct,
        focus.IsEmpty() ? ", triage healer" : (focus == p.guid ? ", own tank" : ", tank healer on the raid"), tanks, raid_members);
}

// A boss the tank is holding but has not out-threatened yet (the opening lead, as the damage dealers wait for it): a heal on a member
// puts threat on everything that member is fighting, so a raid healer leaves members above kHealOpeningHoldPct alone until the tank has
// its lead (Onyxia n6: Flash Heal on a raid member at tank_threat=0 took Onyxia off the tank). Bounded at 12 s of the healer's own
// combat: a tank that never builds the lead must not starve the raid. A tank is never held off.
constexpr int32 kHealOpeningHoldPct = 50;
constexpr int64 kHealOpeningHoldMs  = 12000;
inline bool TankOpeningOnBoss(BotSnapshotView const& s)
{
    if (!ThreatDisciplineApplies(s) || s.combat_duration_ms() >= kHealOpeningHoldMs)
        return false;
    for (NearbyUnit const& u : s.raw().combat.nearby_enemies)
        if ((u.is_dungeon_boss || u.is_boss_rank) && !u.is_player && u.hp > 0 && u.has_threat_list && u.unit_in_combat &&
            ThreatVictim(u.victim_kind) == ThreatVictim::Tank && u.tank_threat < ThreatOpeningLead(s, u))
            return true;
    return false;
}

// A healer whose heals have put it near the tank's threat on a boss the tank holds (n14 Onyxia at 180 s: a healer at 96 % of the tank, then a Knock
// Away cut the tank by a quarter and she turned on it). Healing threat is the effective health done, so the healer leaves members above
// kHealOpeningHoldPct alone until the tank is clear of it again. Emergencies are never held, and a tank is never held off.
constexpr float kHealerThreatHoldRatio = 0.85f;
inline bool HealerNearTankThreat(BotSnapshotView const& s)
{
    if (!ThreatDisciplineApplies(s))
        return false;
    for (NearbyUnit const& u : s.raw().combat.nearby_enemies)
        if ((u.is_dungeon_boss || u.is_boss_rank) && !u.is_player && u.hp > 0 && u.has_threat_list && u.unit_in_combat &&
            ThreatVictim(u.victim_kind) == ThreatVictim::Tank && u.tank_threat > 0.0f &&
            u.my_threat >= u.tank_threat * kHealerThreatHoldRatio * Raid::ThreatShareAfterDrop(s.map_id(), u.entry))
            return true;
    return false;
}

// Fair share of the raid's healing by mana. Whoever's rules run first claims a patient, so the fast, cheap-to-start heals (Flash of Light,
// Healing Touch) took nearly all of them and drained to 5-13 % mana while the priests, whose Greater Heal casts long, kept 92-100 % and
// cast a third as often (n24 mc_geddon, five priests 10-37 casts against 44-75 per paladin). A healer well under the other healers' mana
// leaves the patients who are not in trouble to them, which both lets it regenerate (the five second rule only runs while it is not
// casting) and gives the unspent mana something to do. Emergencies are never left.
constexpr int32 kHealerManaGapPct        = 20;   // under the other healers' average by this much
constexpr int32 kHealerManaStandDownPct  = 65;   // and under this much itself
constexpr int32 kHealerStandDownPatientPct = 60; // patients at or above this are left to the others
constexpr size_t kHealerManaPeersMin     = 3;
// The further a healer is below the others, the more it leaves: a healer under kHealerManaLowPct while the others average kHealerPeersFullPct
// or more heals only emergencies, one under kHealerManaThinPct leaves everyone at kHealerThinPatientPct or more (n32 mc_ragnaros: the druids
// ended at 5 % mana and the paladins at 22-45 % with 80-105 heals each, the four priests at 100 % with 15-36).
constexpr int32 kHealerManaLowPct      = 30;
constexpr int32 kHealerManaThinPct     = 45;
constexpr int32 kHealerPeersFullPct    = 60;
constexpr int32 kHealerThinPatientPct  = 45;
constexpr int32 kHealerNoStandDown     = 101;   // PatientPct above any health: nothing is left
// The health at or above which this healer leaves a patient to the others (kHealerNoStandDown = it leaves none).
inline int32 HealerStandDownPatientPct(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    if (s.max_power(0) <= 0 || ManaPct(s) >= kHealerManaStandDownPct)
        return kHealerNoStandDown;
    int64 sum = 0;
    size_t peers = 0;
    if (auto const* members = g.members())
        for (GroupMemberSummary const& m : *members)
        {
            if (m.guid == s.guid() || m.role != Role::Healer || !m.online || !m.is_alive || m.max_mana <= 0 || m.map_id != s.map_id())
                continue;
            sum += int64(m.mana) * 100 / m.max_mana;
            ++peers;
        }
    if (peers < kHealerManaPeersMin)
        return kHealerNoStandDown;
    int32 const avg = int32(sum / int64(peers));
    if (ManaPct(s) + kHealerManaGapPct >= avg)
        return kHealerNoStandDown;
    if (avg >= kHealerPeersFullPct)
    {
        if (ManaPct(s) < kHealerManaLowPct) return kEmergencyPct;
        if (ManaPct(s) < kHealerManaThinPct) return kHealerThinPatientPct;
    }
    return kHealerStandDownPatientPct;
}

// Best heal target for this healer (PartyMemberToHeal::Calculate). Returns
// the bot itself with valid == false and hp_pct 100 when nobody needs a heal.
inline Patient PickPatient(BotSnapshotView const& s, GroupSnapshotView const& g, float range = kHealRange)
{
    Patient best;
    float best_score = -1000.0f;
    Patient focus_p;   // the healer's own tank, when it is a candidate (raid: tank healers stay on their tank)
    // Raid triage healers look after their own subgroups first (RaidHealGroups.h): own_best is the best patient among them.
    uint16_t const own_groups = s.raw().pve_order.active ? s.raw().pve_order.heal_groups : uint16_t(0);
    Patient own_best;
    float own_score = -1000.0f;
    ObjectGuid const focus_guid = s.raw().pve_order.active ? s.raw().pve_order.heal_focus : ObjectGuid::Empty;
    // Raid: other healers' heals in flight (ApplyInFlight). The heal focus
    // (`assigned`) is left as it was - the tank healer's duty.
    bool const raid = RaidHealing(s) && HealClaimsOn();
    HealClaims const claims = raid ? CopyHealClaims(s.published_at_ms()) : HealClaims{};
    bool const opening_hold = raid && (TankOpeningOnBoss(s) || HealerNearTankThreat(s));
    int32 const stand_down_pct = raid ? HealerStandDownPatientPct(s, g) : kHealerNoStandDown;
    auto consider = [&](Patient const& raw, bool assigned, bool own = false)
    {
        if (!raw.valid || raw.max_hp <= 0) return;
        Patient p = raw;
        if (opening_hold && !raw.tank && raw.actual_pct >= kHealOpeningHoldPct) return;
        if (raw.actual_pct >= stand_down_pct) return;
        // A member at 95%+ is healed on projected damage only by the healers assigned to that tank (Garr 2026-10-08: 19 picks at
        // 100% "projected 65%", 185 cancels, ~39.8k mana wasted).
        if (raid && raw.actual_pct >= 95 && !(assigned && raw.tank)) return;
        // A tank near full is topped off only when the swings on it would take it under the spike mark (the real deficit is nothing).
        if (raid && raw.tank && raw.actual_pct >= kRaidTankSpikeFloorPct && raw.hp_pct >= kRaidTankSpikePct) return;
        // The tank's own healers are exempt only while it is in real trouble: above that, the 2-4 healers on one tank at 65-80%
        // all cast on it (Garr 2026-10-08: 77% of the picks overlapped another healer's within 3 s), so they count each other's heals.
        if (raid)
        {
            ApplyInFlight(s, g, p, claims);
            bool const trouble = assigned && !(raw.actual_pct >= kRaidTankBigHealPct && raw.hp_pct >= kRaidTankBigHealPct);
            if (CoveredByInFlight(s, p, trouble ? kHealTankTroubleCover : 1.0f))
            {
                NoteCoveredSkip(s, p, "pick");
                return;
            }
        }
        // ConsiderHeal: tanks always, everyone else below almost-full. In a
        // raid a healer that is not the tank's own (heal_focus) leaves a
        // tank at 93%+ (projected and actual) to the tank healers: seven
        // healers topping up one tank is the mana the long fights run out of.
        if (!p.tank && p.hp_pct >= kAlmostFullPct) return;
        if (raid && p.tank && !assigned && p.hp_pct >= kRaidTankTopUpPct && p.actual_pct >= kRaidTankTopUpPct) return;
        // A tank is never held off while mobs hit it (MC 2026-10-06: the
        // old "tank has not secured its mob yet" heal hold kept the healers
        // off each tank that picked up a Molten Giant - 1.0-1.25k a swing -
        // until it fell under 50%, and every tank died 6-8 s after its
        // pickup). Heal threat on a tank's mob is the tank's to out-threat.
        float const score = HealScore(p.hp_pct, p.tank, assigned, ReceivingHeal(s, g, p.guid), p.tank && BossOn(s, p.guid), raid);
        if (score > -1000.0f && !focus_guid.IsEmpty() && p.guid == focus_guid)
            focus_p = p;
        if (own && score > own_score)
        {
            own_score = score;
            own_best = p;
        }
        if (score > -1000.0f && score > best_score)
        {
            best_score = score;
            best = p;
        }
    };

    auto const& po = s.raw().pve_order;
    ObjectGuid const focus = po.active ? po.heal_focus : ObjectGuid::Empty;
    float bx, by, bz; s.position(bx, by, bz);
    bool self_seen = false;
    if (auto const* members = g.members())
    {
        for (GroupMemberSummary const& m : *members)
        {
            if (m.guid == s.guid())
            {
                self_seen = true;
                consider(SelfPatient(s, g), focus == m.guid, HealGroupHas(own_groups, m.subgroup));
                continue;
            }
            if (!m.online || !m.is_alive || m.max_hp <= 0 || m.map_id != s.map_id()) continue;
            if (Dist2(m.x, m.y, m.z, bx, by, bz) > range * range) continue;
            bool const tank = IsTank(s, m);
            int32 const pct = m.hp_pct();
            if (!tank && pct >= kEmergencyPct && !InLos(s, m.guid)) continue;
            Patient const mp = MakePatient(s, m.guid, m.hp, m.max_hp, m.x, m.y, m.z, tank);
            // A warlock's own Life Tap is intentional damage: not a reason for a heal until he is low (playtest, live MC 2026-10-10).
            if (raid && LifeTapSkip(m.cls == CLASS_WARLOCK, tank, mp.actual_pct, mp.incoming))
                continue;
            consider(mp, focus == m.guid, HealGroupHas(own_groups, m.subgroup));
        }
    }
    if (!self_seen)
        consider(SelfPatient(s, g), false);

    // Raid: a tank healer stays on its tank while that tank is under 80%, and leaves only for an emergency (someone under 40%)
    // once the tank is above 50% (MC Garr 2026-10-08: healers hopped between tanks, 41 "tank healer on the raid" picks against 33
    // on their own tank, and the tanks died with the healers at 83% mana).
    if (raid && focus_p.valid && best.valid && best.guid != focus_p.guid && focus_p.actual_pct < 80 &&
        !(best.actual_pct < 40 && focus_p.actual_pct >= 50) &&
        // ... and a tank far below its own tank is not a hop but a rescue: with the focus tank under 50 % the healer used to stay for
        // the whole fight while the other tanks fell (MC Garr 2026-10-10: Mchealsa held 2571 at 28 % while 2572 sat at 12 %).
        !(best.tank && best.actual_pct < 35 && best.actual_pct + 15 <= focus_p.actual_pct) &&
        // Self-preservation: a healer that mobs are hitting, under 60%, heals itself before it keeps topping the tank
        // (deadmines_full run 10: "stays on its tank 2561 at 48% (better score 2562 at 13%)", then a Miner killed her).
        !(best.guid == s.guid() && best.actual_pct < 60 && s.attackers_count() >= 1))
    {
        if (RaidLogDue(HealLogKey(s, HealLogKind::Covered), s.published_at_ms(), 3000u))
            TC_LOG_INFO("playerbot.v2", "[raid_heal] {} stays on its tank {} at {}% (the better score was {} at {}%)", s.name(),
                        focus_p.guid.GetCounter(), focus_p.actual_pct, best.guid.GetCounter(), best.actual_pct);
        best = focus_p;
    }
    // Raid triage healer with subgroups: it heals its own groups first, and leaves them only for an emergency elsewhere (someone under
    // kEmergencyPct while its own best is at kHealGroupStayPct or more). Without this every triage healer picked the lowest member
    // of the whole raid and they piled onto the same few.
    if (raid && own_groups && own_best.valid && best.valid && best.guid != own_best.guid &&
        !(best.actual_pct < kEmergencyPct && own_best.actual_pct >= kHealGroupStayPct))
        best = own_best;
    if (!best.valid)
    {
        best = SelfPatient(s, g);
        best.valid = false;
        best.hp_pct = best.actual_pct = 100;
    }
    else if (raid)
        NoteRaidPick(s, g, best, focus);
    return best;
}

inline Patient PickPatient(ApPredicateContext const& ctx) { return PickPatient(ctx.bot, ctx.group); }

// Raid five-second rule: a healer (the group snapshot's effective role) in a
// raid (RaidHealing) casts no damage spell - every one costs mana and stops
// its mana regeneration for five seconds - unless its assignment has needed
// no healing for kRaidHealerDpsQuietMs and its mana is at kRaidHealerDpsManaPct
// or more. Its assignment: its heal focus (PveOrder::heal_focus, wherever it
// stands) and everyone PickPatient would heal within reach. The quiet clock
// restarts whenever the check was not run for kRaidHealerNeedGapMs (the
// rotation was busy healing, casting or out of combat), so a healer never
// nukes straight out of a heal. Dungeons, parties and solo: always true (the
// rotations' own gates stand). Wands cost no mana and are not gated here.
constexpr int32  kRaidHealerDpsManaPct = 90;
constexpr uint32 kRaidHealerDpsQuietMs = 10000;
constexpr uint32 kRaidHealerNeedGapMs  = 2000;

inline bool RaidHealerMayDamage(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    if (!RaidHealing(s) || !Classic::IsGroupHealer(s, &g))
        return true;
    bool need = PickPatient(s, g).valid;
    auto const& po = s.raw().pve_order;
    if (!need && po.active && !po.heal_focus.IsEmpty() && po.heal_focus != s.guid())
        if (auto const* members = g.members())
            for (GroupMemberSummary const& m : *members)
                if (m.guid == po.heal_focus)
                {
                    need = m.online && m.is_alive && m.max_hp > 0 && m.map_id == s.map_id()
                        && m.hp_pct() < kAlmostFullPct;
                    break;
                }
    uint32 const now = s.published_at_ms() ? s.published_at_ms() : 1u;
    uint32 const since_need = WithHealTiming(s, [&](HealTiming& t)
    {
        if (need || !t.need_ms || !t.need_eval_ms || now - t.need_eval_ms >= kRaidHealerNeedGapMs)
            t.need_ms = now;
        t.need_eval_ms = now;
        return now - t.need_ms;
    });
    return since_need >= kRaidHealerDpsQuietMs && ManaPct(s) >= kRaidHealerDpsManaPct;
}

// Base heal of one rank at the caster's level (no +healing gear: the pick
// errs towards a slightly bigger rank).
inline float RankHeal(SpellInfo const* si, uint32 level)
{
    if (!si) return 0.0f;
    for (SpellEffectInfo const& eff : si->GetEffects())
    {
        if (!eff.IsEffect(SPELL_EFFECT_HEAL)) continue;
        float heal = eff.BasePoints;
        if (eff.RealPointsPerLevel != 0.f && si->BaseLevel)
        {
            uint32 lvl = level;
            if (si->MaxLevel && lvl > si->MaxLevel) lvl = si->MaxLevel;
            if (lvl > si->BaseLevel) heal += eff.RealPointsPerLevel * float(lvl - si->BaseLevel);
        }
        return heal;
    }
    return 0.0f;
}

// Expected heal of one rank for this bot: the base heal plus the bot's bonus
// healing times the heal effect's bonus coefficient - what Unit::
// SpellHealingBonusDone adds. The percentage talents (Spiritual Healing,
// Healing Light, ...) are left out, which only tilts the pick towards a
// slightly bigger rank.
inline float ExpectedHeal(SpellInfo const* si, BotSnapshotView const& s)
{
    float const base = RankHeal(si, s.level());
    if (base <= 0.0f) return 0.0f;
    float coeff = 0.0f;
    for (SpellEffectInfo const& eff : si->GetEffects())
        if (eff.IsEffect(SPELL_EFFECT_HEAL)) { coeff = eff.BonusCoefficient; break; }
    return base + float(s.raw().secondary_stats.heal_bonus) * std::max(0.0f, coeff);
}

// Share of the deficit + incoming damage a raid healer's rank must cover:
// the fuller its mana, the more aggressive the rank.
inline float RaidDeficitShare(int32 mana)
{
    return mana >= 70 ? 1.0f : mana >= kSaveManaPct ? 0.9f : 0.75f;
}

// The fast / big heal is warranted: an emergency, or a tank under half health.
inline bool NeedsFastHeal(Patient const& p)
{
    return p.actual_pct < kEmergencyPct || p.hp_pct < kEmergencyPct || (p.tank && p.actual_pct < kRaidTankBigHealPct);
}

// Raid boss fight below kRaidLowManaPct mana: the healer leaves members above
// kRaidLowManaCeilPct alone (direct heals and HoTs alike).
inline bool RaidLowManaHold(BotSnapshotView const& s, Patient const& p)
{
    if (!RaidHealing(s) || ManaPct(s) >= kRaidLowManaPct || p.actual_pct <= kRaidLowManaCeilPct || !Classic::BossFight(s))
        return false;
    if (RaidLogDue(HealLogKey(s, HealLogKind::LowMana), s.published_at_ms(), 10000u))
        TC_LOG_INFO("playerbot.v2", "[raid_heal] {} low mana ({}%) in a boss fight: holds heals on members above {}% and HoTs on non-tanks",
                    s.name(), ManaPct(s), kRaidLowManaCeilPct);
    return true;
}

// HoT (Renew, Rejuvenation) on `target`: in a raid boss fight below
// kRaidLowManaPct mana only on a tank at or under kRaidLowManaCeilPct.
inline bool HotAllowed(BotSnapshotView const& s, GroupSnapshotView const& g, ObjectGuid target)
{
    if (!RaidHealing(s) || ManaPct(s) >= kRaidLowManaPct)
        return true;
    Patient const p = PatientFor(s, g, target);
    if (!p.valid)
        return true;
    if (p.tank)
        return !RaidLowManaHold(s, p);
    if (!Classic::BossFight(s))
        return true;
    Patient full = p;
    full.actual_pct = 100;   // a non-tank: held whatever its health (logs the hold)
    return !RaidLowManaHold(s, full);
}

// HoT claims (raid healing): Renew / Rejuvenation another healer cast on the
// same patient moments ago. A bot only sees its own HoT as an aura it can
// test cheaply (the group snapshot tracks raid buffs, not HoTs), so seven
// healers each saw "no HoT of mine on the tank" and all rolled one: seven
// stacked Renews for one hurt member (mckill 2026-10-06, mana 44% average on
// a 113 s kill). The claim lasts kHotClaimMs per (patient, HoT chain); the
// claimant itself is never blocked by its own claim (its aura check rules).
constexpr uint32 kHotClaimMs = 11000;   // Rejuvenation lasts 12 s, Renew 15 s: a claim of 8 s let a second healer roll over the first's with 4 s left

template <typename F>
auto WithHotClaims(F&& f)
{
    struct Claim { uint64 caster; uint32 ms; };
    static std::mutex mu;
    static std::unordered_map<uint64, Claim> claims;
    std::lock_guard<std::mutex> lock(mu);
    return f(claims);
}

inline uint64 HotClaimKey(ObjectGuid target, uint32 spell)
{
    uint32 const chain = SpellFamily::Root(spell);
    return (uint64(target.GetCounter()) << 24) ^ uint64(chain ? chain : spell);
}

// The claim of any HoT kind on `target` (Renew, Rejuvenation, Regrowth's tail ...): one per patient, whatever the spell.
inline uint64 HotAnyClaimKey(ObjectGuid target)
{
    return (uint64(target.GetCounter()) << 24) ^ 0xFFFFFFull;
}

// Another healer's HoT of `spell`'s chain is already on (or on its way to) `target`; for a member that is not a tank, a HoT of any
// other kind counts too (playtest, live MC 2026-10-10: a Life Tapping warlock carried a Renew, a Rejuvenation and a Regrowth at once,
// three healers' mana for one member). The tanks keep one HoT of each kind: their healers stack them on purpose.
inline bool HotClaimedByOther(BotSnapshotView const& s, GroupSnapshotView const& g, ObjectGuid target, uint32 spell)
{
    if (!RaidHealing(s) || !HealClaimsOn() || target.IsEmpty())
        return false;
    uint32 const now = s.published_at_ms();
    uint64 const self = s.guid().GetCounter();
    uint64 const key = HotClaimKey(target, spell);
    uint64 const any_key = HotAnyClaimKey(target);
    bool const tank = PatientFor(s, g, target).tank;
    return WithHotClaims([&](auto& claims)
    {
        auto const fresh = [&](uint64 k)
        {
            auto const it = claims.find(k);
            return it != claims.end() && it->second.caster != self && int32(now - it->second.ms) < int32(kHotClaimMs);
        };
        return fresh(key) || (!tank && fresh(any_key));
    });
}

inline void NoteHotCast(BotSnapshotView const& s, ObjectGuid target, uint32 spell)
{
    if (!RaidHealing(s) || !HealClaimsOn() || target.IsEmpty())
        return;
    uint32 const now = s.published_at_ms();
    uint64 const key = HotClaimKey(target, spell);
    uint64 const self = s.guid().GetCounter();
    WithHotClaims([&](auto& claims)
    {
        if (claims.size() > 256)
            for (auto it = claims.begin(); it != claims.end();)
                it = int32(now - it->second.ms) > int32(kHotClaimMs) ? claims.erase(it) : std::next(it);
        claims[key] = { self, now };
        claims[HotAnyClaimKey(target)] = { self, now };
        return 0;
    });
}

// Power Word: Shield claims (ClassicHealCover.h ShieldClaimFresh): the cast time of the last shield any priest started on a patient.
inline bool ShieldClaimedByOther(BotSnapshotView const& s, ObjectGuid target)
{
    if (!RaidHealing(s) || !HealClaimsOn() || target.IsEmpty())
        return false;
    uint32 const now = s.published_at_ms();
    uint64 const self = s.guid().GetCounter();
    uint64 const key = (uint64(target.GetCounter()) << 24) ^ 0x5348ull;   // "SH"
    return WithHotClaims([&](auto& claims)
    {
        auto const it = claims.find(key);
        return it != claims.end() && it->second.caster != self && ShieldClaimFresh(it->second.ms, now);
    });
}

inline void NoteShieldCast(BotSnapshotView const& s, ObjectGuid target)
{
    if (!RaidHealing(s) || !HealClaimsOn() || target.IsEmpty())
        return;
    uint64 const key = (uint64(target.GetCounter()) << 24) ^ 0x5348ull;
    uint64 const self = s.guid().GetCounter();
    uint32 const now = s.published_at_ms();
    WithHotClaims([&](auto& claims)
    {
        claims[key] = { self, now ? now : 1u };
        return 0;
    });
}

// A HoT on a raid member is worth its mana (ClassicHealCover.h HotWorthCasting).
inline bool HotWorthIt(BotSnapshotView const& s, GroupSnapshotView const& g, ObjectGuid target)
{
    Patient p = PatientFor(s, g, target);
    if (!p.valid)
        return true;
    bool const raid = RaidHealing(s);
    if (raid && HealClaimsOn())
        ApplyInFlight(s, g, p, CopyHealClaims(s.published_at_ms()));
    return HotWorthCasting(raid, p.actual_pct, HealDeficit(p), p.inflight);
}

struct HealPlan
{
    uint32 rank = 0;      // 0 = cast the highest known rank
    bool   skip = false;  // overheal / mana rule: do not cast this heal now
    float  want = 0.0f;   // heal the rank was sized for (0: top rank, unsized)
    float  est  = 0.0f;   // expected heal of the chosen rank (0: unsized)
};

// Cheapest known rank of `spell`'s chain whose heal covers `want`, never
// smaller than a third of the top rank (`bonus`: sized with ExpectedHeal,
// else the base heal). Ranks above the top known one are never picked.
inline HealPlan PlanRank(BotSnapshotView const& s, uint32 spell, float want, bool bonus)
{
    HealPlan plan;
    uint32 const top = s.resolve_rank(spell);
    if (!top)
        return plan;
    auto heal_of = [&](uint32 id)
    {
        SpellInfo const* si = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
        return bonus ? ExpectedHeal(si, s) : RankHeal(si, s.level());
    };
    float const top_heal = heal_of(top);
    if (top_heal <= 0.0f)
        return plan;
    plan.want = std::max(want, top_heal / 3.0f);
    plan.est = top_heal;
    if (plan.want >= top_heal)
        return plan;
    for (uint32 const rank : s.known_ranks(spell))
    {
        if (rank == top) break;
        float const h = heal_of(rank);
        if (h >= plan.want)
        {
            plan.rank = rank;   // ranks ascend: the first that covers it is the cheapest
            plan.est = h;
            return plan;
        }
    }
    return plan;
}

// PlanDirectHeal + the low-mana top-up rule (ShouldSkipForEfficiency), and
// in a raid the mana-efficiency rules (header comment).
inline HealPlan PlanHeal(BotSnapshotView const& s, GroupSnapshotView const& g, uint32 spell, Patient const& p)
{
    HealPlan plan;
    if (!p.valid || p.max_hp <= 0)
        return plan;

    int32 const mana = ManaPct(s);
    bool const raid = RaidHealing(s);
    // Owner "save mana": top-up heals are skipped at any mana level, a little earlier than the low-mana rule.
    bool const save_mana = s.tactic_save_mana() && !s.tactic_burn();
    if (g.exists() && (mana < kLowManaPct || save_mana) && p.actual_pct >= (p.tank ? (save_mana ? 80 : 85) : (save_mana ? 65 : 70)))
    {
        plan.skip = true;
        return plan;
    }
    if (RaidLowManaHold(s, p))
    {
        plan.skip = true;
        return plan;
    }
    // The mana reserve: in a boss fight the healer under kRaidReservePct leaves the nearly healthy alone and keeps its mana for the
    // members who are in trouble (the fight is long; a dry healer heals nobody).
    if (raid && mana < kRaidReservePct && Classic::BossFight(s) && p.hp_pct >= kEmergencyPct &&
        p.actual_pct >= (p.tank ? kRaidReserveTankPct : kRaidReserveRaidPct))
    {
        plan.skip = true;
        return plan;
    }
    // Overheal-cancel lockout: not straight back onto the patient a heal was
    // just cancelled on (State_InCombat CancelOverheal) - that is the
    // cast / cancel loop. An emergency there is healed regardless.
    if (raid && p.actual_pct >= kEmergencyPct && p.hp_pct >= kEmergencyPct && InCancelLockout(s, p.guid))
    {
        plan.skip = true;
        return plan;
    }

    // Swings are pre-armour, so heavy incoming alone only calls for the top rank once the tank is actually hurt;
    // a topped-up tank holding a few mobs gets a sized heal instead of repeated full-rank overheals.
    bool const tank_pressure = p.tank && (p.actual_pct < 60 || (p.incoming * 10 >= p.max_hp && p.actual_pct < 80));
    if (p.actual_pct < kEmergencyPct || p.hp_pct < kEmergencyPct || tank_pressure)
        return plan;

    uint32 const top = s.resolve_rank(spell);
    if (!top)
        return plan;
    SpellInfo const* top_si = sSpellMgr->GetSpellInfo(top, DIFFICULTY_NONE);
    float const top_heal = raid ? ExpectedHeal(top_si, s) : RankHeal(top_si, s.level());
    if (top_heal <= 0.0f)
        return plan;

    // Raid: what other healers already have on the way is not this heal's to cover (ApplyInFlight).
    // A healing-reduced patient (Gehennas' Curse -75%) needs a proportionally bigger heal for the same deficit.
    float const deficit = float(std::max<int64>(HealDeficit(p) - p.inflight, 0)) / HealTakenFactor(g, p.guid);
    if (deficit < top_heal / 3.0f * 0.25f)
    {
        plan.skip = true;   // tiny deficit: even the smallest sensible rank mostly overheals
        return plan;
    }
    float const share = raid ? RaidDeficitShare(save_mana ? std::min(mana, kSaveManaPct - 1) : mana)
                             : (mana < kSaveManaPct || save_mana ? 0.75f : 0.9f);
    return PlanRank(s, spell, deficit * share, raid);
}

// Bot-relative count of injured members (actual health <= below_pct)
// within `radius` of a point, on the bot's map. Includes the bot.
inline int CountInjuredAround(BotSnapshotView const& s, GroupSnapshotView const& g,
                              float cx, float cy, float cz, float radius, int32 below_pct)
{
    auto const* members = g.members();
    if (!members)
        return s.hp_pct() <= below_pct ? 1 : 0;
    int n = 0;
    for (GroupMemberSummary const& m : *members)
    {
        if (!m.online || !m.is_alive || m.max_hp <= 0 || m.map_id != s.map_id()) continue;
        if (Dist2(m.x, m.y, m.z, cx, cy, cz) > radius * radius) continue;
        if (m.hp_pct() <= below_pct) ++n;
    }
    return n;
}

// Caster-centred party heal (Prayer of Healing, Tranquility): injured
// members of the caster's own party subgroup within `radius` of the bot -
// in vanilla both spells reach only the caster's party, so in a raid the
// rest of the room does not count. Includes the bot.
inline int InjuredNearMe(BotSnapshotView const& s, GroupSnapshotView const& g, float radius, int32 below_pct)
{
    auto const* members = g.members();
    if (!members)
        return s.hp_pct() <= below_pct ? 1 : 0;
    int my_sub = -1;
    for (GroupMemberSummary const& m : *members)
        if (m.guid == s.guid()) { my_sub = m.subgroup; break; }
    float x, y, z; s.position(x, y, z);
    int n = 0;
    for (GroupMemberSummary const& m : *members)
    {
        if (!m.online || !m.is_alive || m.max_hp <= 0 || m.map_id != s.map_id()) continue;
        if (my_sub >= 0 && m.subgroup != my_sub) continue;
        if (Dist2(m.x, m.y, m.z, x, y, z) > radius * radius) continue;
        if (m.hp_pct() <= below_pct) ++n;
    }
    return n;
}

// Chain heal target: the injured member (in heal range and line of sight)
// with the most injured members within `jump` yards of it; ties go to the
// lower health. Empty when no cluster reaches `min_count`.
inline ObjectGuid BestClusterTarget(BotSnapshotView const& s, GroupSnapshotView const& g,
                                    float jump, int32 below_pct, int min_count)
{
    auto const* members = g.members();
    if (!members) return ObjectGuid::Empty;
    float bx, by, bz; s.position(bx, by, bz);
    ObjectGuid best;
    int best_n = 0;
    int32 best_pct = 101;
    for (GroupMemberSummary const& m : *members)
    {
        if (!m.online || !m.is_alive || m.max_hp <= 0 || m.map_id != s.map_id()) continue;
        int32 const pct = m.hp_pct();
        if (pct > below_pct) continue;
        if (Dist2(m.x, m.y, m.z, bx, by, bz) > kHealRange * kHealRange || !InLos(s, m.guid)) continue;
        int const n = CountInjuredAround(s, g, m.x, m.y, m.z, jump, below_pct);
        if (n > best_n || (n == best_n && pct < best_pct))
        {
            best = m.guid;
            best_n = n;
            best_pct = pct;
        }
    }
    return best_n >= min_count ? best : ObjectGuid::Empty;
}

// A rez cast longer than any real one (Redemption / Resurrection / Ancestral Spirit are 10 s) that is still "in flight" on the
// same corpse holds the claim forever and every other healer waits on it (MC 2026-10-08: 84 "already being raised" lines, the owner
// never raised). After 14 s of one caster on one corpse the claim stops counting, and is logged once.
inline bool RezCastStuck(GroupMemberSummary const& caster, ObjectGuid corpse, uint32 now_ms)
{
    static std::mutex mu;
    struct Seen { uint32 first_ms, last_ms; bool logged; };
    static std::unordered_map<uint64, Seen> seen;
    uint64 const key = (caster.guid.GetCounter() << 24) ^ corpse.GetCounter();
    std::lock_guard<std::mutex> lk(mu);
    Seen& e = seen[key];
    if (e.last_ms == 0 || uint32(now_ms - e.last_ms) > 3000u)
        e = Seen{ now_ms, now_ms, false };
    e.last_ms = now_ms;
    bool const stuck = uint32(now_ms - e.first_ms) > 14000u;
    if (stuck && !e.logged)
    {
        e.logged = true;
        TC_LOG_INFO("playerbot.v2", "[raid_rez] {} has been casting spell {} on {} for {} s (cast ends in {} ms): the claim no longer counts",
                    caster.name, caster.casting_spell, corpse.GetCounter(), (now_ms - e.first_ms) / 1000u,
                    int64(caster.casting_end_ms) - int64(now_ms));
    }
    return stuck;
}

// Corpse for this bot to resurrect. Dead (hp 0, not released) members on the
// bot's map within `range` and in line of sight, healers first, then tanks,
// then the rest (guid order inside each band). A corpse another member is
// already casting a rez on (any rank, Rebirth included) is skipped, so two
// druids never Rebirth the same corpse. Several rezzers spread: this bot
// takes the corpse at its index among the rezzers within 40 yd that are not
// already casting one (`rezzer_class` true for a member that can cast this
// kind of rez; the bot counts itself). Empty when there is nothing to do.
template <typename RezzerPred>
ObjectGuid RezTarget(BotSnapshotView const& s, GroupSnapshotView const& g, float range, RezzerPred&& rezzer_class, bool in_combat_ok = false)
{
    auto const* members = g.members();
    if (!members) return ObjectGuid::Empty;
    float bx, by, bz; s.position(bx, by, bz);
    // The owner (a human member) is raised first (playtest, MC Gehennas 2026-10-08: dead, nobody rezzed him), then healers, tanks, the rest.
    auto band = [](GroupMemberSummary const& m) { return !m.is_bot ? -1 : m.role == Role::Healer ? 0 : m.role == Role::Tank ? 1 : 2; };
    GroupMemberSummary const* cand[40];
    int n = 0, dead = 0, far_away = 0, no_los = 0, taken_n = 0;
    float nearest_dead = 1e9f;
    std::string claimer;   // who holds the claim on a taken corpse (for the [raid_rez] line)
    for (GroupMemberSummary const& m : *members)
    {
        if (n >= 40) break;
        if (!m.online || m.max_hp <= 0 || m.hp > 0 || m.map_id != s.map_id() || m.guid == s.guid()) continue;
        ++dead;
        float const dd = std::sqrt(Dist2(m.x, m.y, m.z, bx, by, bz));
        nearest_dead = std::min(nearest_dead, dd);
        if (dd > range) { ++far_away; continue; }
        if (!InLos(s, m.guid)) { ++no_los; continue; }
        bool taken = false;
        // The offer is up and not accepted yet: the corpse stays claimed after the caster's cast ended.
        if (m.rez_pending)
        {
            taken = true;
            claimer = "an offer already pending";
        }
        else
        for (GroupMemberSummary const& o : *members)
            if (o.guid != s.guid() && o.is_casting && o.casting_target == m.guid && IsRezSpell(o.casting_spell) &&
                !RezClaims::RecentlyFailed(o.guid, m.guid, s.published_at_ms()) &&
                !RezCastStuck(o, m.guid, s.published_at_ms()))
            {
                taken = true;
                claimer = fmt::format("{} spell {} ends in {} ms", o.name, o.casting_spell, int64(o.casting_end_ms) - int64(s.published_at_ms()));
                break;
            }
        if (taken) ++taken_n; else cand[n++] = &m;
    }
    // [raid_rez] (playtest, MC Gehennas 2026-10-08: dead, nobody rezzed him; the log said nothing): why a dead member is, or is not,
    // picked up by this bot, at most once per 5 s per bot and only in a raid.
    if (dead > 0 && s.is_in_raid())
    {
        static std::mutex mu;
        static std::unordered_map<uint64, uint32> last;
        uint32 const now = s.published_at_ms();
        bool due = false;
        {
            std::lock_guard<std::mutex> lk(mu);
            uint32& l = last[s.guid().GetCounter()];
            if (!l || uint32(now - l) >= 5000u) { l = now; due = true; }
        }
        if (due)
            TC_LOG_INFO("playerbot.v2", "[raid_rez] {} dead on map {}, nearest {:.0f} y (cast range {:.0f}): {} too far, {} no LoS, {} already being raised{}{}, {} candidates, in_combat={}",
                        s.name(), dead, nearest_dead, range, far_away, no_los, taken_n, claimer.empty() ? "" : " by ", claimer, n, int(s.in_combat()));
    }
    if (n == 0) return ObjectGuid::Empty;
    std::sort(cand, cand + n, [&](GroupMemberSummary const* a, GroupMemberSummary const* b)
    {
        int const ba = band(*a), bb = band(*b);
        return ba != bb ? ba < bb : a->guid < b->guid;
    });
    // One rezzer per corpse (playtest, MC 2026-10-10: a dozen healers cast on him at once): every rezzer reads the same snapshot, so a "taken
    // by whoever is already casting" check alone lets all of them start in the same tick. The rezzers within 40 yd that can cast now
    // (alive, out of combat, not already on a rez) are ranked by guid; the first n of them take the n corpses, the rest wait. The
    // ranking turns every 3 s, so a front rezzer that cannot cast (no mana, on a cooldown, held by another order) does not stall the
    // rest for longer than that, and the claim check above keeps the next one off a corpse the front rezzer has begun.
    int index = 0, rezzers = 1;   // rezzers counts this bot
    for (GroupMemberSummary const& m : *members)
    {
        if (m.guid == s.guid() || !m.online || !m.is_alive || (m.in_combat && !in_combat_ok) || m.map_id != s.map_id() || !rezzer_class(m)) continue;
        if (m.is_casting && IsRezSpell(m.casting_spell)) continue;   // busy on its own corpse
        if (Dist2(m.x, m.y, m.z, bx, by, bz) > 40.0f * 40.0f) continue;
        ++rezzers;
        if (m.guid < s.guid()) ++index;
    }
    int const rank = V2::RezRank(index, rezzers, s.published_at_ms());
    if (rank >= n) return ObjectGuid::Empty;
    return cand[rank]->guid;
}

// True when the bot can pay the mana of the top rez rank it knows (MC 2026-10-08: protection paladins with an empty bar were refused
// NO_POWER 39 times on Redemption, every tick, and still counted as rezzers). Flat costs are exact; a percent cost is taken of the
// maximum mana. The DB2 cost is the untalented one: it never lets a cast through early. A bot with no rez spell is "affordable".
inline bool RezManaOk(BotSnapshotView const& s)
{
    uint32 const root = s.cls() == CLASS_PRIEST ? 2006u : s.cls() == CLASS_SHAMAN ? 2008u : s.cls() == CLASS_PALADIN ? 7328u : 0u;
    if (!root) return true;
    uint32 const known = s.resolve_rank(root);
    if (!known) return true;
    SpellInfo const* si = sSpellMgr->GetSpellInfo(known, DIFFICULTY_NONE);
    if (!si) return true;
    int32 cost = 0;
    for (SpellPowerEntry const* pw : si->PowerCosts)
        if (pw && pw->PowerType == POWER_MANA && !pw->RequiredAuraSpellID)
            cost += pw->ManaCost + int32(pw->PowerCostPct * float(s.max_power(0)) / 100.0f);
    return s.power(0) >= cost;
}

// Vanilla out-of-combat rez (Resurrection / Ancestral Spirit / Redemption;
// druids have none) and the druids' Rebirth: the corpse this bot takes.
// True while the group's fight is on (a tank in combat, or three or more members): the out-of-combat rezzes wait for it to end
// (V2::RezFightLive). The rotations of priest / paladin / shaman reach OocRezTarget with a healer that is itself out of combat, and
// raised the owner in the middle of the encounter; Rebirth does not come through here.
inline bool RaidFightLive(GroupSnapshotView const& g, uint32 map_id)
{
    auto const* members = g.members();
    if (!members) return false;
    int tanks = 0, fighting = 0;
    for (GroupMemberSummary const& m : *members)
    {
        if (!m.online || !m.is_alive || !m.in_combat || m.map_id != map_id) continue;
        ++fighting;
        if (m.role == Role::Tank) ++tanks;
    }
    return V2::RezFightLive(tanks, fighting);
}

// The fight is on while a boss encounter is in progress too: a submerged Ragnaros leaves the raid with no tank and few members in combat, and the
// priests, paladins and shamans raised the dead in the middle of the encounter (playtest, MC Rag 2026-10-12). Only a druid raises in a live fight.
inline bool RezFightOn(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    return s.is_encounter_in_progress() || RaidFightLive(g, s.map_id());
}

inline ObjectGuid OocRezTarget(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    if (!RezManaOk(s) || RezFightOn(s, g))
        return ObjectGuid::Empty;
    return RezTarget(s, g, 28.0f, [](GroupMemberSummary const& m)
    {
        return m.level >= 10 && (m.cls == CLASS_PRIEST || m.cls == CLASS_SHAMAN || m.cls == CLASS_PALADIN);
    });
}

inline ObjectGuid RebirthTarget(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    // Rebirth is a combat rez: the druids in the fight are the rezzers it is ranked among.
    return RezTarget(s, g, 28.0f, [](GroupMemberSummary const& m) { return m.cls == CLASS_DRUID && m.level >= 20 && m.role != Role::Tank; }, /*in_combat_ok*/ true);   // a tank druid never rezzes in the fight
}

inline bool MemberHasDebuff(GroupSnapshotView const& g, ObjectGuid guid, uint32 spell)
{
    if (auto const* members = g.members())
        for (GroupMemberSummary const& m : *members)
            if (m.guid == guid)
            {
                for (AuraEntry const& a : m.debuffs)
                    if (a.spell_id == spell)
                        return true;
                return false;
            }
    return false;
}

// Group member to dispel/cure: alive, within 30 yd, in line of sight, with
// a harmful aura of one of `types`. Several candidates are spread over the
// group's healers by bot guid. `skip_aura` (an Abolish spell) skips members
// already carrying the bot's own cleanse-over-time. Falls back to the bot
// itself; empty when nobody needs it.
inline ObjectGuid DispelTarget(BotSnapshotView const& s, GroupSnapshotView const& g,
                               std::initializer_list<DispelType> types, uint32 skip_aura = 0)
{
    auto has_type = [&](std::vector<AuraEntry> const& auras)
    {
        for (AuraEntry const& a : auras)
            if (a.is_harmful)
                for (DispelType t : types)
                    if (a.dispel_type == t)
                        return true;
        return false;
    };
    ObjectGuid candidates[40];
    int n = 0;
    if (auto const* members = g.members())
    {
        float bx, by, bz; s.position(bx, by, bz);
        for (GroupMemberSummary const& m : *members)
        {
            if (n >= 40) break;
            if (m.guid == s.guid() || !m.online || !m.is_alive || m.map_id != s.map_id()) continue;
            if (Dist2(m.x, m.y, m.z, bx, by, bz) > kDispelRange * kDispelRange || !InLos(s, m.guid)) continue;
            if (!has_type(m.debuffs)) continue;
            if (skip_aura && s.has_aura(skip_aura, m.guid)) continue;
            candidates[n++] = m.guid;
        }
    }
    if (n > 0)
        return candidates[s.guid().GetCounter() % uint64(n)];
    for (DispelType t : types)
        if (s.self_dispellable(t) && !(skip_aura && s.has_aura(skip_aura)))
            return s.guid();
    return ObjectGuid::Empty;
}

inline ObjectGuid DispelTarget(ApPredicateContext const& ctx, std::initializer_list<DispelType> types, uint32 skip_aura = 0)
{
    return DispelTarget(ctx.bot, ctx.group, types, skip_aura);
}

// Raid-wide dispel / cure assignment (Classic::PickDispel): the debuff work
// of the whole group split so exactly one capable bot takes each debuffed
// member, healers and DPS alike. Every vanilla rotation with a cure spell
// runs it; the target-picker above stays for callers outside the plan. The
// mana floor (Classic::kDispelMinManaPct) is PickDispel's own, so the plan
// never hands a job to a bot this gate would turn away.
// A dispel of the same debuff type on the same member is not repeated inside this window: a cure that worked takes the debuff off
// at once, so a second cast means it did not take (not dispellable, out of line of sight, or the debuff comes straight back).
// deadmines_full c82a7e21: a level 21 priest cast Dispel Magic ten times on one member, 830 of its 2345 mana.
constexpr uint32 kDispelRepeatMs = 8000;

inline bool DispelRecently(void const* ctx, uint64 key)
{
    BotSnapshotView const& s = *static_cast<BotSnapshotView const*>(ctx);
    uint32 const now = s.published_at_ms();
    return WithHealTiming(s, [&](HealTiming& t) { return t.dispel_key == key && t.dispel_ms && now - t.dispel_ms < kDispelRepeatMs; });
}

inline bool ShouldAssignedDispel(ApPredicateContext const& ctx)
{
    if (ctx.bot.is_casting())
        return false;
    return Classic::PickDispel(ctx.bot, ctx.group, { &DispelRecently, &ctx.bot }).spell != 0;
}
inline void DoAssignedDispel(ApPredicateContext const& ctx, BotIntentEmitter& e)
{
    Classic::CastPick const d = Classic::PickDispel(ctx.bot, ctx.group, { &DispelRecently, &ctx.bot });
    if (d.spell && e.cast(d.spell, d.target))
    {
        WithHealTiming(ctx.bot, [&](HealTiming& t) { t.dispel_key = d.key; t.dispel_ms = ctx.bot.published_at_ms(); return 0; });
        Classic::NoteRaidSupport(Classic::RaidSupportKind::Dispel, ctx.bot.map_id());
        if (Classic::IsCurseDispelSpell(d.spell))
            Classic::NoteRaidSupport(Classic::RaidSupportKind::Decurse, ctx.bot.map_id());
    }
}

// [raid_heal] spell / rank change: the exact rank this bot casts differs from
// the one it last logged (per bot, at most one line per 5 s).
inline void NoteHealChoice(BotSnapshotView const& s, uint32 spell, Patient const& p, HealPlan const& plan)
{
    uint32 const cast = plan.rank ? plan.rank : s.resolve_rank(spell);
    if (!cast)
        return;
    uint32 const now = s.published_at_ms();
    uint32 const prev = WithHealTiming(s, [&](HealTiming& t) -> uint32
    {
        if (t.logged_spell == cast || (t.logged_ms && now - t.logged_ms < 5000u))
            return cast;   // no change, or paced
        uint32 const was = t.logged_spell;
        t.logged_spell = cast;
        t.logged_ms = now;
        return was;
    });
    if (prev == cast)
        return;
    std::string ranks;
    for (uint32 r : s.known_ranks(spell))
        ranks += fmt::format("{}{}", ranks.empty() ? "" : "/", r);
    TC_LOG_INFO("playerbot.v2",
        "[raid_heal] {} known ranks of {}: {}", s.name(), spell, ranks.empty() ? "none" : ranks);
    TC_LOG_INFO("playerbot.v2",
        "[raid_heal] {} heal choice {} (chain {} rank {}) <- {} on {}{} at {}% (projected {}%): deficit {} want {:.0f} est {:.0f}, mana {}%",
        s.name(), cast, sSpellMgr->GetFirstSpellInChain(cast), uint32(sSpellMgr->GetSpellRank(cast)), prev,
        p.guid.GetCounter(), p.tank ? " (tank)" : "", p.actual_pct, p.hp_pct, p.max_hp - p.hp + p.incoming,
        plan.want, plan.est, ManaPct(s));
}

// The flat mana cost of the spell (talent discounts ignored, as is_ready does for rage and energy): a heal the healer cannot pay is
// refused by the core with NO_POWER; sending it keeps the healer casting at nothing instead of the rules that can still help.
// Percent-of-base-mana costs are not in the snapshot and count as affordable.
inline bool CanPayMana(BotSnapshotView const& s, uint32 spell_id)
{
    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
    if (!si)
        return true;
    for (SpellPowerEntry const* pw : si->PowerCosts)
        if (pw && pw->PowerType == POWER_MANA && pw->ManaCost > 0 && !pw->RequiredAuraSpellID && s.power(POWER_MANA) < pw->ManaCost)
            return false;
    return true;
}

inline bool PayableOrLog(BotSnapshotView const& s, uint32 spell_id, Patient const& p)
{
    if (CanPayMana(s, spell_id))
        return true;
    if (RaidLogDue(HealLogKey(s, HealLogKind::LowMana), s.published_at_ms(), 10000u))
        TC_LOG_INFO("playerbot.v2", "[raid_heal] {} cannot pay spell {} for {} at mana {}%: no heal sent", s.name(), spell_id,
                    p.guid.GetCounter(), ManaPct(s));
    return false;
}

// Emit a planned direct heal. Returns false (nothing emitted, the rotation
// falls through to the next rule) when the plan says not to cast.
// Nothing is cast for an invalid patient (PickPatient's "nobody needs a
// heal" fallback is the bot itself at 100%, not a heal target).
// Raid: the in-flight check and this healer's claim happen under one lock
// (WithHealClaims), so two healers whose rules picked the same patient in the
// same tick cannot both start a heal its deficit only needs once (see the
// "Heals in flight" block). The heal focus is exempt from the check (the
// tank healer's duty) but is still claimed, so others see that heal coming.
inline bool EmitHeal(BotSnapshotView const& s, GroupSnapshotView const& g, BotIntentEmitter& e,
                     uint32 spell, Patient const& p)
{
    if (!p.valid)
        return false;
    // Feared, stunned, confused or silenced: the core refuses the cast, so nothing is picked, logged or claimed (MC Magmadar
    // 2026-10-10: feared healers stepped up the ranks on every refused cast).
    if (s.cannot_heal())
        return false;
    ObjectGuid const target = p.guid.IsEmpty() ? s.guid() : p.guid;
    if (!RaidHealing(s) || !HealClaimsOn())
    {
        HealPlan const plan = PlanHeal(s, g, spell, p);
        if (plan.skip)
            return false;
        if (RaidHealing(s) && !PayableOrLog(s, plan.rank ? plan.rank : s.resolve_rank(spell), p))
            return false;
        bool const sent = plan.rank ? e.cast_rank(plan.rank, target) : e.cast(spell, target);
        if (sent && RaidHealing(s))
        {
            Playerbot::Raid::NoteHealCastSent(s.guid().GetCounter(), s.name(), s.published_at_ms());
            Playerbot::Raid::ManaLedgerHealSent(s.guid().GetCounter(), plan.est);
            // The same audit line as the claimed path below (deadmines_full: a 5-man priest wrote [raid_heal] choices but no [heal_cast]).
            TC_LOG_INFO("playerbot.v2", "[heal_cast] t={} {} spell {} -> {}{} at {}%: deficit {} (missing {} incoming {}) est {:.0f}, mana {}% (no heal claims)",
                        s.published_at_ms(), s.name(), plan.rank ? plan.rank : spell, target.GetCounter(), p.tank ? " (tank)" : "",
                        p.actual_pct, p.max_hp - p.hp + p.incoming, p.max_hp - p.hp, p.incoming, plan.est, ManaPct(s));
            NoteHealChoice(s, spell, p, plan);
        }
        return sent;
    }
    auto const& po = s.raw().pve_order;
    bool const focus = po.active && !po.heal_focus.IsEmpty() && po.heal_focus == target;
    uint32 const now = s.published_at_ms();
    return WithHealClaims(now, [&](std::unordered_map<uint64, HealClaim>& claims)
    {
        Patient q = p;
        {
            ApplyInFlight(s, g, q, claims);
            if (CoveredByInFlight(s, q, focus && NeedsFastHeal(p) ? kHealTankTroubleCover : 1.0f))
            {
                NoteCoveredSkip(s, q, "claimed this tick");
                return false;
            }
        }
        HealPlan const plan = PlanHeal(s, g, spell, q);
        if (plan.skip)
            return false;
        if (!PayableOrLog(s, plan.rank ? plan.rank : s.resolve_rank(spell), q))
            return false;
        bool const sent = plan.rank ? e.cast_rank(plan.rank, target) : e.cast(spell, target);
        if (!sent)
            return false;
        Playerbot::Raid::NoteHealCastSent(s.guid().GetCounter(), s.name(), s.published_at_ms());
        uint32 const cast = plan.rank ? plan.rank : s.resolve_rank(spell);
        SpellInfo const* si = sSpellMgr->GetSpellInfo(cast ? cast : spell, DIFFICULTY_NONE);
        int32 const cast_ms = si && si->CastTimeEntry ? std::max(si->CastTimeEntry->Base, 0) : 0;
        HealClaim& c = claims[s.guid().GetCounter()];
        c.patient  = target;
        c.spell    = cast ? cast : spell;
        c.amount   = plan.est > 0.0f ? plan.est : ExpectedHeal(si, s);
        c.start_ms = now;
        c.land_ms  = now + uint32(cast_ms);
        Playerbot::Raid::ManaLedgerHealSent(s.guid().GetCounter(), c.amount);
        // One line per raid heal sent, for the mana audit (MC Garr 2026-10-10: the choice lines log only a change, so same-second
        // stacking and the size of each cast could not be counted). est / deficit above 1 is an overheal risk.
        // The spell data chains few Classic ranks (GetSpellRank reads 0 for them), so the rank shows as the spell's base level. The
        // deficit is health missing plus the swings still to come; missing, incoming and inflight are its parts, so a cast can be
        // judged against what the target lacked when it was sent (est above missing + incoming is overheal).
        TC_LOG_INFO("playerbot.v2", "[heal_cast] t={} {} spell {} (level {}) -> {}{} at {}%: deficit {} (missing {} incoming {} inflight {}) est {:.0f}, mana {}%{}",
                    now, s.name(), c.spell, si ? uint32(si->BaseLevel) : 0u, target.GetCounter(), q.tank ? " (tank)" : "",
                    q.actual_pct, q.max_hp - q.hp + q.incoming, q.max_hp - q.hp, q.incoming, q.inflight, c.amount, ManaPct(s), focus ? " focus" : "");
        NoteHealChoice(s, spell, q, plan);
        return true;
    });
}

// Record a heal a caller other than EmitHeal sent (the tank pre-heal) as this
// healer's claim, so the other healers' in-flight counts see it.
inline void NoteHealClaim(BotSnapshotView const& s, ObjectGuid target, uint32 spell, float amount)
{
    if (!RaidHealing(s) || !HealClaimsOn() || target.IsEmpty() || !spell || s.cannot_heal())
        return;
    uint32 const now = s.published_at_ms();
    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell, DIFFICULTY_NONE);
    int32 const cast_ms = si && si->CastTimeEntry ? std::max(si->CastTimeEntry->Base, 0) : 0;
    WithHealClaims(now, [&](std::unordered_map<uint64, HealClaim>& claims)
    {
        HealClaim& c = claims[s.guid().GetCounter()];
        c.patient  = target;
        c.spell    = spell;
        c.amount   = amount;
        c.start_ms = now;
        c.land_ms  = now + uint32(cast_ms);
        return 0;
    });
}

// Raid spell choice: `fast` (the fast or big heal) for an emergency or a tank
// under half health (NeedsFastHeal), `efficient` for everyone else. Outside
// raids, or before `efficient` is trained, always `fast`.
inline uint32 RaidChoose(BotSnapshotView const& s, Patient const& p, uint32 fast, uint32 efficient)
{
    if (!RaidHealing(s) || !efficient || !s.knows_spell(efficient) || NeedsFastHeal(p))
        return fast;
    return efficient;
}

// Raid priest direct heal: Greater Heal for an emergency or a tank under half
// health; everyone else gets Heal (sized to rank 2-4 by PlanHeal) when its
// top rank covers the wanted share of the deficit - all of it at 70%+ mana,
// three quarters from 40%, half below - else a downranked Greater Heal.
// `big` is the rotation's pick (Greater Heal, or Heal / Lesser Heal while
// levelling); outside raids it is returned unchanged.
inline uint32 RaidPriestHeal(BotSnapshotView const& s, Patient const& p, uint32 big)
{
    constexpr uint32 HEAL = 2054;
    if (!RaidHealing(s) || big == HEAL || !s.knows_spell(HEAL) || NeedsFastHeal(p))
        return big;
    uint32 const top = s.resolve_rank(HEAL);
    float const heal = top ? ExpectedHeal(sSpellMgr->GetSpellInfo(top, DIFFICULTY_NONE), s) : 0.0f;
    int32 const mana = ManaPct(s);
    float const cover = mana >= 70 ? 1.0f : mana >= kSaveManaPct ? 0.75f : 0.5f;
    float const deficit = float(p.max_hp - p.hp + p.incoming);
    // A small deficit goes to Lesser Heal when its top rank covers most of it: below the rank-2 Heal (level 22) Heal's lowest
    // rank is the only downrank the planner had, and a 300-point Heal on a 100-point deficit cost the level-21 priest her
    // mana in two pulls (deadmines_full run 10: Heal rank 1 on a tank at 78-100%).
    constexpr uint32 LESSER_HEAL = 2050;
    if (s.knows_spell(LESSER_HEAL))
    {
        uint32 const lesser_top = s.resolve_rank(LESSER_HEAL);
        float const lesser = lesser_top ? ExpectedHeal(sSpellMgr->GetSpellInfo(lesser_top, DIFFICULTY_NONE), s) : 0.0f;
        if (lesser > 0.0f && heal > lesser && lesser >= deficit * RaidDeficitShare(mana) * cover * 0.75f)
            return LESSER_HEAL;
    }
    return heal > 0.0f && heal >= deficit * RaidDeficitShare(mana) * cover ? HEAL : big;
}

// Tank heal of a vanilla healer class for the raid pre-heal (State_InCombat
// PreemptiveTankHeal): Greater Heal (Heal / Lesser Heal while levelling),
// Holy Light, Healing Touch, Healing Wave. 0 when the bot knows none.
inline uint32 TankHealSpell(BotSnapshotView const& s)
{
    auto first_known = [&s](std::initializer_list<uint32> spells) -> uint32
    {
        for (uint32 sp : spells)
            if (s.knows_spell(sp))
                return sp;
        return 0;
    };
    switch (s.cls())
    {
        case CLASS_PRIEST:  return first_known({ 2060, 2054, 2050 });
        case CLASS_PALADIN: return first_known({ 635 });
        case CLASS_DRUID:   return first_known({ 5185 });
        case CLASS_SHAMAN:  return first_known({ 331 });
        default:            return 0;
    }
}

// Out-of-combat top-up heal for a vanilla healer class: the mana-efficient
// direct heal (downranked by PlanHeal), not the fast emergency one.
// 0 when the bot knows none.
inline uint32 OocHealSpell(BotSnapshotView const& s)
{
    auto first_known = [&s](std::initializer_list<uint32> spells) -> uint32
    {
        for (uint32 sp : spells)
            if (s.knows_spell(sp))
                return sp;
        return 0;
    };
    switch (s.cls())
    {
        case CLASS_PRIEST:  return first_known({ 2060, 2054, 2050 });   // Greater Heal, Heal, Lesser Heal
        case CLASS_DRUID:   return first_known({ 5185 });               // Healing Touch
        case CLASS_PALADIN: return first_known({ 19750, 635 });         // Flash of Light, Holy Light
        case CLASS_SHAMAN:  return first_known({ 331 });                // Healing Wave
        default:            return 0;
    }
}

// Healing on the run (State_InCombat LooseMobEscape: a loose elite chases a
// raid healer toward the tank). Only spells that land while moving - a cast
// or channel would fail SPELL_FAILED_MOVING and halt nothing but the heal, so
// those wait until the healer stands again and the rotation runs. Patient:
// PickPatient (the chased healer itself counts, with the elite's swings
// projected). Priest: Power Word: Shield on a patient at <=50%, else on
// itself (the one taking the hits), then Renew at <=85%; druid in caster
// form: Swiftmend at <=50% on its own Rejuvenation / Regrowth, then
// Rejuvenation at <=85%; paladin: Holy Shock (Holy talent, 20 yd) at <=60%.
// Shaman: Nature's Swiftness (Restoration talent) for a patient at <=40%,
// then on the next step the Healing Wave it makes instant (<=70%); else a
// Healing Stream Totem when none of its own stands within 20 yd and a party
// member is in its reach (someone below 90%). Ghost Wolf casts nothing.
// One instant per step. Returns the spell emitted, 0 = none.
inline uint32 EmitInstantHealWhileMoving(BotSnapshotView const& s, GroupSnapshotView const& g, BotIntentEmitter& e)
{
    constexpr uint32 POWER_WORD_SHIELD = 17, WEAKENED_SOUL = 6788, RENEW = 139;
    constexpr uint32 REJUVENATION = 774, REGROWTH = 8936, SWIFTMEND = 18562;
    constexpr uint32 HOLY_SHOCK = 20473;
    constexpr uint32 NATURES_SWIFTNESS = 16188, HEALING_WAVE = 331, HEALING_STREAM_TOTEM = 5394, GHOST_WOLF = 2645;
    constexpr uint32 kDruidForms[] = { 768, 5487, 9634, 24858, 783, 1066 };   // Cat, Bear, Dire Bear, Moonkin, Travel, Aquatic
    if (s.is_casting() || ManaPct(s) < 5)
        return 0;
    auto ready = [&s](uint32 sp) { return s.knows_spell(sp) && s.is_ready(sp); };
    auto mine_on = [&s](uint32 sp, ObjectGuid t) { return s.has_aura(sp, t == s.guid() ? ObjectGuid::Empty : t); };
    auto cast = [&e](uint32 sp, ObjectGuid t) { return e.cast(sp, t) ? sp : 0u; };
    switch (s.cls())
    {
        case CLASS_PRIEST:
        {
            Patient const p = PickPatient(s, g);
            if (ready(POWER_WORD_SHIELD))
            {
                ObjectGuid const t = (p.valid && p.hp_pct <= 50) ? p.guid : s.guid();
                bool const blocked = t == s.guid()
                    ? (s.has_aura(POWER_WORD_SHIELD) || s.has_aura(WEAKENED_SOUL))
                    : (mine_on(POWER_WORD_SHIELD, t) || mine_on(WEAKENED_SOUL, t) || MemberHasDebuff(g, t, WEAKENED_SOUL));
                if (!blocked)
                    if (uint32 const sp = cast(POWER_WORD_SHIELD, t))
                        return sp;
            }
            if (p.valid && p.hp_pct <= 85 && ready(RENEW) && !mine_on(RENEW, p.guid))
                return cast(RENEW, p.guid);
            return 0;
        }
        case CLASS_DRUID:
        {
            for (uint32 form : kDruidForms)
                if (s.has_aura(form))
                    return 0;
            Patient const p = PickPatient(s, g);
            if (!p.valid) return 0;
            if (p.hp_pct <= 50 && ready(SWIFTMEND) && (mine_on(REJUVENATION, p.guid) || mine_on(REGROWTH, p.guid)))
                if (uint32 const sp = cast(SWIFTMEND, p.guid))
                    return sp;
            if (p.hp_pct <= 85 && ready(REJUVENATION) && !mine_on(REJUVENATION, p.guid))
                return cast(REJUVENATION, p.guid);
            return 0;
        }
        case CLASS_PALADIN:
        {
            if (!ready(HOLY_SHOCK)) return 0;
            Patient const p = PickPatient(s, g, 20.0f);
            return p.valid && p.hp_pct <= 60 ? cast(HOLY_SHOCK, p.guid) : 0u;
        }
        case CLASS_SHAMAN:
        {
            if (s.has_aura(GHOST_WOLF)) return 0;   // no casting in Ghost Wolf
            Patient const p = PickPatient(s, g);
            // Nature's Swiftness up (cast on an earlier step): the next nature
            // spell under 10 s cast is instant, so Healing Wave lands on the run.
            if (s.has_aura(NATURES_SWIFTNESS))
                return p.valid && p.hp_pct <= 70 && ready(HEALING_WAVE) ? cast(HEALING_WAVE, p.guid) : 0u;
            if (p.valid && p.hp_pct <= 40 && ready(NATURES_SWIFTNESS))
                if (uint32 const sp = cast(NATURES_SWIFTNESS, ObjectGuid::Empty))
                    return sp;
            // Healing Stream Totem: heals the caster's party within 20 yd for
            // as long as it stands. Not over a stream of our own still in reach.
            if (!ready(HEALING_STREAM_TOTEM)) return 0;
            float bx, by, bz; s.position(bx, by, bz);
            constexpr float kHstRange = 20.0f;
            if (TotemUpkeep const* t = s.totem_from(HEALING_STREAM_TOTEM))
                if (Dist2(t->x, t->y, t->z, bx, by, bz) <= kHstRange * kHstRange)
                    return 0;
            auto const* members = g.members();
            if (!members) return 0;
            uint8 my_subgroup = 0;
            for (GroupMemberSummary const& m : *members)
                if (m.guid == s.guid()) { my_subgroup = m.subgroup; break; }
            bool party_near = false, hurt = s.hp_pct() < kAlmostFullPct;
            for (GroupMemberSummary const& m : *members)
            {
                if (m.guid == s.guid() || m.subgroup != my_subgroup || !m.online || !m.is_alive
                    || m.map_id != s.map_id() || Dist2(m.x, m.y, m.z, bx, by, bz) > kHstRange * kHstRange)
                    continue;
                party_near = true;
                if (m.hp_pct() < kAlmostFullPct) hurt = true;
            }
            return party_near && hurt ? cast(HEALING_STREAM_TOTEM, ObjectGuid::Empty) : 0u;
        }
        default:
            return 0;
    }
}

} // namespace ClassicHeal

// Classic downranked direct heal on `target` (see ClassicHeal::PlanHeal).
inline void CastHeal(ApPredicateContext const& ctx, BotIntentEmitter& e, uint32 spell, ObjectGuid target)
{
    ClassicHeal::EmitHeal(ctx.bot, ctx.group, e, spell, ClassicHeal::PatientFor(ctx.bot, ctx.group, target));
}

} // namespace Playerbot::Combat
