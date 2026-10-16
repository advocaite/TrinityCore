#include "DamageParity.h"

#include "../Bot/BotAI.h"
#include "../Bot/BotRegistry.h"
#include "../Bot/Gear/BotGearGenerator.h"
#include "../Fleet/BotPopulationManager.h"
#include "../Services.h"
#include "../Threading/SnapshotPublisher.h"
#include "../Threading/TickScheduler.h"

#include "ClassicArmor.h"
#include "Config.h"
#include "Creature.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Item.h"
#include "Log.h"
#include "DB2Stores.h"
#include "GridDefines.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAPI.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Unit.h"
#include "World.h"
#include "fmt/format.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace Playerbot::Diagnostics::Parity {

namespace {

// ---- The tally (written by the damage hooks, read by the world thread) ----

constexpr std::uint32_t kPetFlag = 0x80000000u;   // a pet's spell id in the tally

std::atomic<bool> g_measuring{false};
std::atomic<bool> g_energy_on{false};                // the economy audit's casting-regen window is counting energize events
std::map<std::uint32_t, std::int64_t> g_energy;      // spell id -> mana the subject was given by it in the window, under g_mu
std::mutex g_mu;
ObjectGuid g_subject, g_dummy;                       // under g_mu
std::map<std::uint32_t, SpellAgg> g_aggs;            // spell id (| kPetFlag) -> tally, under g_mu
std::map<std::uint32_t, SpellAgg> g_aggs_watched;    // the same, for hits that landed while the watched aura was on the dummy
std::string g_watch_aura;                            // an aura name to split the tally by ("" = none)
std::uint32_t g_watch_caster_low = 0;

// The outcome the next damage event of the same spell on the same victim belongs to: the outcome hook runs just before the damage is dealt.
struct Pending
{
    bool valid = false;
    std::uint32_t spell = 0;
    ObjectGuid victim;
    Outcome outcome = Outcome::Hit;
    bool pet = false;
    bool watched = false;
};
thread_local Pending t_pending;

bool HasAuraNamed(Unit const* target, std::string const& name, ObjectGuid caster)
{
    for (auto const& kv : target->GetAppliedAuras())
    {
        Aura const* a = kv.second->GetBase();
        if (!a || (!caster.IsEmpty() && a->GetCasterGUID() != caster))
            continue;
        SpellInfo const* i = a->GetSpellInfo();
        if (i && i->SpellName && name == (*i->SpellName)[sWorld->GetDefaultDbcLocale()])
            return true;
    }
    return false;
}

// Whether `attacker` is the subject or its pet and `victim` the dummy. g_mu held.
bool g_reverse = false;   // under g_mu: tally what the creature in g_dummy does to the subject (the incoming damage scenarios)

bool IsOurs(Unit* attacker, Unit* victim, bool& pet)
{
    if (!attacker || !victim)
        return false;
    if (g_reverse)
    {
        pet = false;
        return attacker->GetGUID() == g_dummy && victim->GetGUID() == g_subject;
    }
    Unit* owner = attacker->GetCharmerOrOwnerOrSelf();
    if (!owner || owner->GetGUID() != g_subject || victim->GetGUID() != g_dummy)
        return false;
    pet = owner != attacker;
    return true;
}

// ---- The run ----

struct Leg
{
    char const* label;
    int gear;          // 0 nothing worn, 1 tier 1, 3 tier 3
    int armor;         // the dummy's armor
    int sp;            // spell power added on top of the gear
    int ap;            // attack power added (melee and ranged)
};

constexpr int kDummyArmor = 3731;                  // a level 63 raid boss
constexpr int kSunderedArmor = kDummyArmor - 5 * 520;   // five Sunder Armors of the top rank
constexpr int kSensitivitySp = 500, kSensitivityAp = 400;   // large enough to stand out of the sampling noise of a few dozen hits

constexpr Leg kLegs[] = {
    { "none",        0, kDummyArmor,    0, 0 },
    { "t1",          1, kDummyArmor,    0, 0 },
    { "t3",          3, kDummyArmor,    0, 0 },
    { "t3_sp500",    3, kDummyArmor,    kSensitivitySp, 0 },
    { "t3_ap400",    3, kDummyArmor,    0, kSensitivityAp },
    { "t3_armor0",   3, 0,              0, 0 },
    { "t3_sunder5",  3, kSunderedArmor, 0, 0 },
};
constexpr std::size_t kLegCount = sizeof(kLegs) / sizeof(kLegs[0]);

// What one leg left behind, kept for the cross-leg checks.
struct LegResult
{
    Leg leg{};
    double seconds = 0.0;
    double dps = 0.0;
    std::map<std::uint32_t, SpellAgg> aggs;
    std::map<std::uint32_t, std::string> names;
    double weapon_speed = 0.0;
    bool dual_wield = false;
    double ap = 0.0, rap = 0.0, crit_melee = 0.0, crit_spell = 0.0;
    std::string range;
    RangeVerdict verdict = RangeVerdict::NoReference;
};

// ---- Sweeps ----

enum class Res : std::uint8_t { None, Combo, Rage };
enum class Measure : std::uint8_t { Damage, DotTotal, AuraDuration, ArmorDrop };

struct SweepItem
{
    std::uint8_t cls;
    char const* spell;
    Res res;
    std::vector<int> points;
    Measure measure;
    int reps;
    char const* watch_aura = nullptr;     // split the hits by this aura on the dummy (Improved Shadow Bolt's Shadow Vulnerability)
    double expect_ratio = 0.0;            // with the aura against without it
    bool low_health = false;              // the dummy is at 15 % (Execute)
};

std::vector<SweepItem> SweepItemsFor(Kind kind, std::uint8_t cls)
{
    std::vector<SweepItem> v;
    std::vector<int> const combo = { 1, 2, 3, 4, 5 };
    if (kind == Kind::Finisher)
    {
        if (cls == 4)
        {
            v.push_back({ 4, "Eviscerate", Res::Combo, combo, Measure::Damage, 6 });
            v.push_back({ 4, "Rupture", Res::Combo, combo, Measure::DotTotal, 2 });
            v.push_back({ 4, "Kidney Shot", Res::Combo, combo, Measure::AuraDuration, 1 });
            v.push_back({ 4, "Slice and Dice", Res::Combo, combo, Measure::AuraDuration, 1 });
            v.push_back({ 4, "Expose Armor", Res::Combo, combo, Measure::ArmorDrop, 1 });
        }
        else if (cls == 11)
        {
            v.push_back({ 11, "Ferocious Bite", Res::Combo, combo, Measure::Damage, 6 });
            v.push_back({ 11, "Rip", Res::Combo, combo, Measure::DotTotal, 2 });
        }
        else if (cls == 1)
        {
            SweepItem exec = { 1, "Execute", Res::Rage, { 15, 30, 45, 60, 80, 100 }, Measure::Damage, 6 };
            exec.low_health = true;
            v.push_back(exec);
        }
    }
    else if (kind == Kind::TalentAudit && cls == 9)
    {
        SweepItem isb = { 9, "Shadow Bolt", Res::None, { 0 }, Measure::Damage, 70 };
        isb.watch_aura = "Shadow Vulnerability";
        isb.expect_ratio = 1.20;
        v.push_back(isb);
    }
    return v;
}

// Talents the sweep method names, looked up in the bot's spellbook by name: found or not, and what the talent's effects are. `expect` is the
// amount one of its effects should have in vanilla (0 = no reference, the effects are only listed).
struct TalentRef { std::uint8_t cls; char const* name; double expect; };
constexpr TalentRef kTalents[] = {
    { 9, "Ruin", 100 }, { 9, "Shadow Mastery", 10 }, { 9, "Bane", 500 }, { 9, "Improved Shadow Bolt", 0 }, { 9, "Emberstorm", 10 },
    { 9, "Devastation", 5 }, { 9, "Improved Corruption", 0 }, { 9, "Demonic Sacrifice", 0 }, { 9, "Improved Immolate", 0 },
    { 1, "Cruelty", 5 }, { 1, "Flurry", 0 }, { 1, "Impale", 20 }, { 1, "Dual Wield Specialization", 0 }, { 1, "Death Wish", 0 }, { 1, "Mortal Strike", 0 },
    { 4, "Lethality", 30 }, { 4, "Dual Wield Specialization", 0 }, { 4, "Improved Eviscerate", 0 }, { 4, "Malice", 5 }, { 4, "Aggression", 0 },
    { 7, "Elemental Fury", 0 }, { 7, "Flurry", 0 }, { 7, "Convection", 0 }, { 7, "Call of Thunder", 0 },
    { 8, "Arcane Concentration", 0 }, { 8, "Ignite", 0 }, { 8, "Improved Scorch", 0 }, { 8, "Winter's Chill", 0 }, { 8, "Critical Mass", 0 },
    { 5, "Shadow Weaving", 0 }, { 5, "Shadow Power", 0 }, { 5, "Darkness", 0 },
    { 11, "Predatory Strikes", 0 }, { 11, "Savage Fury", 0 }, { 11, "Moonfury", 0 }, { 11, "Vengeance", 0 },
    { 2, "Conviction", 5 }, { 2, "Vengeance", 0 }, { 2, "Two-Handed Weapon Specialization", 0 },
    { 3, "Lethal Shots", 5 }, { 3, "Mortal Shots", 0 }, { 3, "Slaying", 0 }, { 3, "Barrage", 0 },
};

enum class Step : std::uint8_t { Prepare, Settle, Warm, Measure, SweepSetup, SweepCast, SweepWait, Econ, Incoming, Trap, Finished };

// The economy audit's progress: phase 0 regen out of combat, 1 regen in combat, 2 regen while casting, 3 heal sizes, 4 done.
struct Econ
{
    int phase = 0;
    std::uint32_t t = 0;
    double m0 = 0.0, r_oc = -1.0, r_ic = -1.0, r_cast = -1.0;
    int casts = 0, ic_seconds_in_combat = 0;
    double cost = 0.0;
    std::uint32_t main_spell = 0;
    std::vector<std::uint32_t> heals;
    std::size_t heal_i = 0;
    int rep = 0;
    bool cast_sent = false;
    std::uint32_t health_before = 0;
    std::vector<double> got;
    std::vector<std::uint32_t> auras_before;   // casting-regen window: the subject's aura ids at its start
    int cast_starts = 0;                       // casts that actually began (api.cast_spell can say Ok for one that never starts)
    double spent = 0.0;                        // mana the accepted casts cost when they were issued (a free Clearcasting cast adds 0)
    int cast_attempts = 0;                   // casting-regen window: cast_spell calls, accepted or not
    int cast_gcd_ticks = 0;                    // ticks the window skipped a cast because the global cooldown (or a cast) was running
    int last_result = -1;                      // Playerbot::Result of the last refused cast_spell
    std::uint32_t last_fail = 0;               // its SpellCastResult (API::last_cast_fail)
    bool was_casting = false;
    std::set<std::uint32_t> recov;             // food / drink auras seen in the current window
    bool sat = false;
};

// The incoming damage scenario's progress: legs 0-2 are the dummy at bare / tier 1 / tier 3 armor, 3-5 the mobs.
struct Incoming
{
    int leg = 0;
    std::uint32_t t = 0;
    bool setup = false;
    bool crush_logged = false;
    std::uint32_t out_of_reach_s = 0;     // consecutive seconds the attacker has been out of melee range of the tank
    std::uint32_t repositioned = 0;       // times the harness put it back next to the tank this leg
    struct Result { std::string label; double armor = 0.0; SpellAgg melee; std::map<std::uint32_t, SpellAgg> all; };
    std::vector<Result> results;
};

// The trap audit's progress: one entry per trap spell, each in four phases (0 cast, 1 find the object and walk the dummy onto it, 2 wait for it to
// fire, 3 let the effect land and judge).
struct TrapAudit
{
    std::size_t item = 0;
    int phase = 0;
    std::uint32_t t = 0;
    std::uint32_t spell_id = 0;
    ObjectGuid trap;
    std::uint32_t trap_entry = 0;
    std::uint32_t health_before = 0;
    bool fired = false;
    int attempt = 1;                   // a spell can miss the level 63 dummy: a trap is broken only when none of its attempts works
    std::uint32_t effect_spell = 0;    // what the trap object casts when it fires (the object's template)
    bool control_ran = false;          // the direct cast of the effect spell at the dummy has been done (then the trap itself is judged broken)
    std::uint32_t control_lost = 0;
    std::size_t control_auras = 0;
};

struct State
{
    bool active = false;
    Kind kind = Kind::None;
    ObjectGuid subject, dummy;
    std::uint8_t cls = 0;
    std::string cls_name;
    std::uint32_t leg_seconds = 180;
    Step step = Step::Prepare;
    std::uint32_t step_s = 0;           // seconds in the current step
    std::size_t leg = 0;
    std::vector<LegResult> results;
    std::string failure;
    // time usage over the leg
    std::uint32_t casting_s = 0, attacking_s = 0, idle_s = 0, mana_refills = 0;
    std::string gear_at_start;
    int sp_applied = 0, ap_applied = 0;
    // sweeps
    std::vector<SweepItem> items;
    std::size_t item = 0, point = 0;
    int rep = 0;
    std::vector<double> samples, series;
    std::uint32_t cast_spell_id = 0;
    std::uint32_t cast_failures = 0, cast_ok = 0;
    std::string last_fail;
    double armor_before = 0.0;
    bool audit_done = false;
    Econ econ;
    Incoming incoming;
    TrapAudit trap;
    std::vector<std::string> table;     // markdown rows
    std::string dummy_name;
};
State g;

char const* ClassName(std::uint8_t cls)
{
    switch (cls)
    {
        case 1:  return "Warrior";
        case 2:  return "Paladin";
        case 3:  return "Hunter";
        case 4:  return "Rogue";
        case 5:  return "Priest";
        case 7:  return "Shaman";
        case 8:  return "Mage";
        case 9:  return "Warlock";
        case 11: return "Druid";
        default: return "?";
    }
}

std::string SpellNameOf(std::uint32_t id)
{
    SpellInfo const* i = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
    return i && i->SpellName ? std::string((*i->SpellName)[sWorld->GetDefaultDbcLocale()]) : std::string("?");
}

Player* Subject()
{
    return g.subject.IsEmpty() ? nullptr : ObjectAccessor::FindConnectedPlayer(g.subject);
}

Creature* DummyCreature(Player* p)
{
    return p && !g.dummy.IsEmpty() ? ObjectAccessor::GetCreature(*p, g.dummy) : nullptr;
}

// The highest known rank of the named spell.
std::uint32_t FindKnownSpell(Player const* p, std::string const& name)
{
    std::uint32_t best = 0;
    int best_rank = -1;
    for (auto const& [id, ps] : p->GetSpellMap())
    {
        if (ps.state == PLAYERSPELL_REMOVED || ps.disabled)
            continue;
        SpellInfo const* i = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
        if (!i || !i->SpellName || name != (*i->SpellName)[sWorld->GetDefaultDbcLocale()])
            continue;
        int const rank = int(i->GetRank());
        if (rank > best_rank || (rank == best_rank && id > best))
        {
            best = id;
            best_rank = rank;
        }
    }
    return best;
}

// ---- The dummy ----

// Why a creature template cannot be summoned as a visible target; empty when it can. A template whose display the client store (70291) does
// not hold, or that has no model row, makes Map::SummonCreature return null without a word.
std::string UnusableReason(std::uint32_t entry)
{
    CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(entry);
    if (!ct)
        return "no creature_template row";
    CreatureModel const* model = ct->GetFirstValidModel();
    if (!model)
        return "no valid model (creature_model_info has none of its display ids)";
    if (!sCreatureDisplayInfoStore.HasRecord(model->CreatureDisplayID))
        return fmt::format("display {} is not in the client's CreatureDisplayInfo store", model->CreatureDisplayID);
    return {};
}

std::uint32_t FindDummyEntry()
{
    // The operator's choice first, then known test dummies: 1000 Unkillable Test Dummy, 2673 Target Dummy, 1921 (the old training dummy).
    // Each is taken only if the server can actually put it in the world.
    std::vector<std::uint32_t> candidates;
    if (int const cfg = sConfigMgr->GetIntDefault("Playerbot.Scenario.DummyEntry", 0, true); cfg > 0)
        candidates.push_back(std::uint32_t(cfg));
    for (std::uint32_t const e : { 1000u, 2673u, 1921u })
        candidates.push_back(e);
    // Then a training dummy by name, without a script of its own (a retail dummy's script resets its health and swallows the damage).
    static char const* const kNames[] = { "Training Dummy", "Target Dummy", "Raider's Training Dummy", "Dungeoneer's Training Dummy" };
    for (char const* name : kNames)
        for (auto const& [entry, ct] : sObjectMgr->GetCreatureTemplates())
            if (ct.Name == name && ct.AIName.empty() && !ct.ScriptID)
                candidates.push_back(entry);
    for (char const* name : kNames)
        for (auto const& [entry, ct] : sObjectMgr->GetCreatureTemplates())
            if (ct.Name == name)
                candidates.push_back(entry);
    for (std::uint32_t const entry : candidates)
    {
        std::string const why = UnusableReason(entry);
        if (why.empty())
            return entry;
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] dummy candidate {} rejected: {}", entry, why);
    }
    return 0;
}

void SetDummyArmor(Creature* d, int armor)
{
    d->SetStatFlatModifier(UNIT_MOD_ARMOR, BASE_VALUE, float(armor));
    d->UpdateArmor();
}

Creature* SpawnDummy(Player* p, float distance, int armor, std::string& error)
{
    std::uint32_t const entry = FindDummyEntry();
    if (!entry)
    {
        error = "no usable target dummy (1000, 2673, 1921 and every Training/Target Dummy template were rejected, see the [dmg_parity] dummy candidate lines; set Playerbot.Scenario.DummyEntry to an attackable creature that does nothing)";
        return nullptr;
    }
    Position const pos = p->GetNearPosition(distance, 0.f);
    TempSummon* s = p->GetMap()->SummonCreature(entry, pos, nullptr, 0ms, p);
    if (!s)
    {
        std::string why = UnusableReason(entry);
        if (why.empty())
            why = Trinity::IsValidMapCoord(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()) ? "template and model are fine; Map::SummonCreature refused (see Server.log)"
                                                                                                   : "the summon position is not a valid map coordinate";
        error = fmt::format("could not summon creature {} next to {} at {:.1f},{:.1f},{:.1f} on map {}: {}", entry, p->GetName(), pos.GetPositionX(), pos.GetPositionY(),
                            pos.GetPositionZ(), p->GetMapId(), why);
        TC_LOG_ERROR("playerbot.v2", "[dmg_parity] {}", error);
        return nullptr;
    }
    s->SetFaction(14);   // hostile to every player
    s->SetReactState(REACT_PASSIVE);
    s->SetLevel(63);
    s->SetRegenerateHealth(false);
    s->SetMaxHealth(2000000000u);
    s->SetFullHealth();
    SetDummyArmor(s, armor);
    g.dummy = s->GetGUID();
    g.dummy_name = s->GetName();
    TC_LOG_INFO("playerbot.v2", "[dmg_parity] dummy {} (entry {}) level 63 armor {} at {:.1f} y from {}", s->GetName(), entry, armor, distance, p->GetName());
    return s;
}

// ---- Stat snapshot ----

struct Stats { double ap, rap, crit_melee, crit_spell, armor_pen_dummy; double sp[7]; double speed; bool dual_wield; };

Stats SnapshotStats(Player* p, Creature* d)
{
    Stats s{};
    s.ap = p->GetTotalAttackPowerValue(BASE_ATTACK);
    s.rap = p->GetTotalAttackPowerValue(RANGED_ATTACK);
    s.crit_melee = p->GetUnitCriticalChanceDone(BASE_ATTACK);
    s.crit_spell = p->m_activePlayerData->SpellCritPercentage;
    for (int school = 1; school < 7; ++school)
        s.sp[school] = p->SpellBaseDamageBonusDone(SpellSchoolMask(1 << school));
    s.armor_pen_dummy = d ? double(d->GetArmor()) : 0.0;
    s.speed = double(p->GetBaseAttackTime(BASE_ATTACK)) / 1000.0;
    s.dual_wield = p->haveOffhandWeapon();
    return s;
}

std::string GearSummary(Player* p)
{
    std::uint32_t sum = 0, n = 0;
    std::string items;
    for (std::uint8_t slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (Item const* it = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            items += fmt::format("{}:{} ", uint32(slot), it->GetEntry());
            if (slot != EQUIPMENT_SLOT_BODY && slot != EQUIPMENT_SLOT_TABARD)
            {
                sum += it->GetItemLevel(p);
                ++n;
            }
        }
    return fmt::format("avg_ilvl={} pieces={} items={}", n ? sum / n : 0u, n, items);
}

int TierPieces(Player* p, int tier)
{
    if (tier <= 0)
        return 0;
    int worn = 0;
    for (std::uint32_t const entry : ::Playerbot::V2::Gear::TierSetPieces(p->GetClass(), tier))
        for (std::uint8_t slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            if (Item const* it = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot); it && it->GetEntry() == entry)
                ++worn;
    return worn;
}

void ApplyBonuses(Player* p, int sp, int ap)
{
    if (sp != g.sp_applied)
    {
        p->ApplySpellPowerBonus(sp - g.sp_applied, true);
        g.sp_applied = sp;
    }
    if (ap != g.ap_applied)
    {
        float const delta = float(ap - g.ap_applied);
        // A positive step adds to the flat bonus, a negative one (taking the bonus off) subtracts from it: the same call with `apply` flipped.
        bool const add = delta > 0.f;
        float const amount = std::fabs(delta);
        p->HandleAttackPowerModifier(AttackPowerModIndex::Melee, AttackPowerModType::FlatPositive, amount, add);
        p->HandleAttackPowerModifier(AttackPowerModIndex::Ranged, AttackPowerModType::FlatPositive, amount, add);
        g.ap_applied = ap;
    }
}

void StripGear(Player* p)
{
    for (std::uint8_t slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            p->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);
}

void Engage(Player* p, Creature* d)
{
    ::Playerbot::API api(p);
    api.start_attack(d->GetGUID());
    if (BotAI* ai = ::Playerbot::Services::Registry().ai(p->GetGUID().GetCounter()))
    {
        ai->enter_manual(BotAI::OwnerCommand::Engage, d->GetGUID(), GameTime::GetGameTimeMS());
        ai->release_owner_lead();
    }
}

// The bot's own bot logic stops (a sweep casts by hand): the same three calls the scenario stand-in uses.
void DetachAi(Player* p)
{
    ::Playerbot::BotId const id = p->GetGUID().GetCounter();
    if (::Playerbot::Services::Registry().has(id))
    {
        ::Playerbot::Services::Scheduler().unregister_bot(id);
        ::Playerbot::Services::Snapshots().remove(id);
        ::Playerbot::Services::Registry().unregister_bot(id);
    }
}

void TopUp(Player* p)
{
    p->SetFullHealth();
    Powers const pt = p->GetPowerType();
    if (pt == POWER_MANA || pt == POWER_ENERGY)
        p->SetPower(pt, p->GetMaxPower(pt));
}

// ---- Output ----

void AppendFile(std::string const& text)
{
    std::ofstream out(sLog->GetLogsDir() + "DamageParity.md", std::ios::app);
    if (out)
        out << text << '\n';
}

void Row(std::string const& line)
{
    g.table.push_back(line);
    TC_LOG_INFO("playerbot.v2", "[dmg_parity] {}", line);
    AppendFile(line);
}

std::string TopGaps(LegResult const& r)
{
    std::vector<std::pair<std::uint64_t, std::string>> top;
    for (auto const& [key, a] : r.aggs)
    {
        auto const name = r.names.find(key & ~kPetFlag);
        top.emplace_back(a.TotalDamage(), (key & kPetFlag ? "pet " : "") + (name != r.names.end() ? name->second : std::string("?")));
    }
    std::sort(top.rbegin(), top.rend());
    std::string out;
    for (std::size_t i = 0; i < top.size() && i < 3; ++i)
        out += fmt::format("{}{} {}", i ? ", " : "", top[i].second, top[i].first);
    return out;
}

// ---- Damage legs ----

void BeginLegSetup(Player* p)
{
    Leg const& leg = kLegs[g.leg];
    ::Playerbot::V2::Fleet::BotPopulationManager::SetGearModeOverride(p->GetGUID().GetCounter(), leg.gear == 0 ? -1 : (leg.gear == 1 ? 2 : 4));
    if (leg.gear == 0)
        StripGear(p);
    else
        ::Playerbot::Services::Population().RegearNow(p);
    ApplyBonuses(p, leg.sp, leg.ap);
    if (Creature* d = DummyCreature(p))
        SetDummyArmor(d, leg.armor);
    TopUp(p);
    p->CombatStop(true);
    g.step = Step::Settle;
    g.step_s = 0;
}

void StartMeasuring(Player* p)
{
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_aggs.clear();
        g_aggs_watched.clear();
        g_subject = p->GetGUID();
        g_dummy = g.dummy;
    }
    g.casting_s = g.attacking_s = g.idle_s = g.mana_refills = 0;
    g.gear_at_start = GearSummary(p);
    g_measuring = true;
    g.step = Step::Measure;
    g.step_s = 0;
}

void ReportLeg(Player* p)
{
    g_measuring = false;
    LegResult r;
    r.leg = kLegs[g.leg];
    r.seconds = double(g.step_s);
    {
        std::lock_guard<std::mutex> lock(g_mu);
        r.aggs = g_aggs;
    }
    Creature* d = DummyCreature(p);
    Stats const st = SnapshotStats(p, d);
    r.weapon_speed = st.speed;
    r.dual_wield = st.dual_wield;
    r.ap = st.ap; r.rap = st.rap; r.crit_melee = st.crit_melee; r.crit_spell = st.crit_spell;

    std::uint64_t total = 0, pet_total = 0;
    for (auto const& [key, a] : r.aggs)
    {
        r.names[key & ~kPetFlag] = key & ~kPetFlag ? SpellNameOf(key & ~kPetFlag) : std::string("white swing");
        (key & kPetFlag ? pet_total : total) += a.TotalDamage();
    }
    r.dps = r.seconds > 0.0 ? double(total + pet_total) / r.seconds : 0.0;
    double const pet_dps = r.seconds > 0.0 ? double(pet_total) / r.seconds : 0.0;

    int const tier = r.leg.gear;
    // The two reference legs (no stat bonus, normal armor) are held to the raid ranges; the others are sensitivity probes.
    bool const reference = r.leg.sp == 0 && r.leg.ap == 0 && r.leg.armor == kDummyArmor;
    r.verdict = reference ? DpsVerdict(g.cls, tier, /*raid_buffs*/ false, r.dps, &r.range) : RangeVerdict::NoReference;

    TC_LOG_INFO("playerbot.v2", "[dmg_parity] leg class={} leg={} gear={} tier_pieces={} armor={} secs={:.0f} dps={:.0f} dps_busy={:.0f} pet_dps={:.0f} casting_s={} attacking_s={} idle_s={} mana_refills={} range={} verdict={}",
                g.cls_name, r.leg.label, tier, TierPieces(p, tier), r.leg.armor, r.seconds, r.dps, (g.casting_s + g.attacking_s) ? double(total + pet_total) / double(g.casting_s + g.attacking_s) : 0.0, pet_dps, g.casting_s, g.attacking_s, g.idle_s, g.mana_refills,
                r.range.empty() ? "-" : r.range, ToString(r.verdict));
    TC_LOG_INFO("playerbot.v2", "[dmg_parity] gear leg={} {} -> {}", r.leg.label, g.gear_at_start, GearSummary(p));
    TC_LOG_INFO("playerbot.v2", "[dmg_parity] stats leg={} AP={:.0f} RAP={:.0f} SP[hol,fir,nat,fro,sha,arc]={:.0f},{:.0f},{:.0f},{:.0f},{:.0f},{:.0f} crit_melee={:.2f} crit_spell={:.2f} dummy_armor={:.0f} weapon_speed={:.2f} dual_wield={}",
                r.leg.label, st.ap, st.rap, st.sp[1], st.sp[2], st.sp[3], st.sp[4], st.sp[5], st.sp[6], st.crit_melee, st.crit_spell, st.armor_pen_dummy, st.speed, st.dual_wield);

    std::vector<std::pair<std::uint64_t, std::uint32_t>> order;
    for (auto const& [key, a] : r.aggs)
        order.emplace_back(a.TotalDamage(), key);
    std::sort(order.rbegin(), order.rend());
    for (std::size_t i = 0; i < order.size() && i < 14; ++i)
    {
        std::uint32_t const key = order[i].second;
        SpellAgg const& a = r.aggs[key];
        TC_LOG_INFO("playerbot.v2", "{}", FormatSpellLine(r.leg.label, g.cls_name, (key & kPetFlag ? "pet " : "") + r.names[key & ~kPetFlag], a, r.seconds));
        // The crit rate the character sheet promises, for the spell or the swing.
        double const expect = (key & ~kPetFlag) ? r.crit_spell : r.crit_melee;
        if (!(key & kPetFlag) && !CritPctConsistent(a.CritPct(), expect, a.Landed()))
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] crit_mismatch leg={} spell=\"{}\" measured={:.1f}% sheet={:.1f}% over {} landed", r.leg.label,
                        r.names[key & ~kPetFlag], a.CritPct(), expect, a.Landed());
    }
    Row(SummaryRow(g.cls_name, r.leg.label, tier == 0 ? "none" : tier == 1 ? "t1" : "t3", false, r.dps, r.range, r.verdict, TopGaps(r)));
    g.results.push_back(std::move(r));
}

// The checks across legs, once all of them have run.
void CrossLegChecks(Player* p)
{
    auto find = [&](char const* label) -> LegResult const*
    {
        for (LegResult const& r : g.results)
            if (std::string(r.leg.label) == label)
                return &r;
        return nullptr;
    };
    LegResult const* base = find("t3");
    LegResult const* sp = find("t3_sp500");
    LegResult const* ap = find("t3_ap400");
    LegResult const* armor0 = find("t3_armor0");
    LegResult const* sunder = find("t3_sunder5");
    if (!base)
        return;

    // Spell power: the measured coefficient of every direct damage spell with a vanilla reference.
    if (sp)
        for (auto const& [key, a] : base->aggs)
        {
            if (key & kPetFlag || !key)
                continue;
            auto const it = sp->aggs.find(key);
            std::string const name = base->names.at(key);
            double const ref = VanillaSpCoefficient(name);
            if (it == sp->aggs.end() || a.N(Outcome::Hit) < 15 || it->second.N(Outcome::Hit) < 15)
                continue;
            double const coeff = CoefficientPerPoint(a.AvgNormal(), it->second.AvgNormal(), kSensitivitySp);
            Verdict const v = ref > 0.0 ? CompareValue(coeff, ref, 35.0) : Verdict::NoData;
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] sp_coefficient class={} spell=\"{}\" measured={:.3f} vanilla={} verdict={} (hit avg {:.0f} -> {:.0f} over +{} spell power, {} / {} hits)",
                        g.cls_name, name, coeff, ref > 0.0 ? fmt::format("{:.3f}", ref) : std::string("-"), ToString(v), a.AvgNormal(), it->second.AvgNormal(),
                        kSensitivitySp, a.N(Outcome::Hit), it->second.N(Outcome::Hit));
            if (v == Verdict::TooLow || v == Verdict::TooHigh)
                g.failure += fmt::format("{} {} spell power coefficient {:.3f}, vanilla {:.3f} ({}); ", g.cls_name, name, coeff, ref, ToString(v));
        }

    // Attack power on the white swing: speed / 14 per point, less what armor takes off. Dual wielders mix two hands; they are only logged.
    if (ap)
    {
        auto const w = base->aggs.find(0), wa = ap->aggs.find(0);
        if (w != base->aggs.end() && wa != ap->aggs.end() && w->second.N(Outcome::Hit) >= 15 && wa->second.N(Outcome::Hit) >= 15)
        {
            double const coeff = CoefficientPerPoint(w->second.AvgNormal(), wa->second.AvgNormal(), kSensitivityAp);
            double const ref = base->weapon_speed / 14.0 * (1.0 - VanillaArmorReduction(kDummyArmor, 60));
            Verdict const v = base->dual_wield ? Verdict::NoData : CompareValue(coeff, ref, 25.0);
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] ap_coefficient class={} measured={:.4f} vanilla={:.4f} (speed {:.2f} / 14, armor {}) dual_wield={} verdict={}", g.cls_name, coeff, ref,
                        base->weapon_speed, kDummyArmor, base->dual_wield, ToString(v));
            if (v == Verdict::TooLow || v == Verdict::TooHigh)
                g.failure += fmt::format("{} attack power coefficient {:.4f}, vanilla {:.4f} ({}); ", g.cls_name, coeff, ref, ToString(v));
        }
    }

    // Armor: the same physical hit against 0, 1131 and 3731 armor must fall as armor / (armor + 400 + 85 * attacker level).
    if (armor0)
    {
        // The probe is the white swing or a physical spell with the most normal hits in every leg that is compared.
        std::uint32_t probe = 0;
        std::uint32_t best_n = 0;
        for (auto const& [key, a] : base->aggs)
        {
            if (key & kPetFlag)
                continue;
            SpellInfo const* i = key ? sSpellMgr->GetSpellInfo(key, DIFFICULTY_NONE) : nullptr;
            if (key && (!i || i->GetSchoolMask() != SPELL_SCHOOL_MASK_NORMAL))
                continue;
            auto const z = armor0->aggs.find(key);
            if (z != armor0->aggs.end() && std::min(a.N(Outcome::Hit), z->second.N(Outcome::Hit)) > best_n)
            {
                best_n = std::min(a.N(Outcome::Hit), z->second.N(Outcome::Hit));
                probe = key;
            }
        }
        if (best_n >= 15)
        {
            double const a3731 = base->aggs.at(probe).AvgNormal(), a0 = armor0->aggs.at(probe).AvgNormal();
            double const ratio = a0 > 0.0 ? a3731 / a0 : 0.0;
            double const ref = 1.0 - VanillaArmorReduction(kDummyArmor, 60);
            Verdict const v = RatioVerdict(ratio, ref, 8.0);
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] armor_ratio class={} probe=\"{}\" damage at {} armor / at 0 armor = {:.3f}, vanilla {:.3f} (armor / (armor + 400 + 85 * level)) verdict={} ({} / {} hits)",
                        g.cls_name, probe ? SpellNameOf(probe) : std::string("white swing"), kDummyArmor, ratio, ref, ToString(v), base->aggs.at(probe).N(Outcome::Hit),
                        armor0->aggs.at(probe).N(Outcome::Hit));
            if (v != Verdict::Ok)
                g.failure += fmt::format("{} physical damage at {} armor is {:.3f} of unarmored, vanilla {:.3f} ({}); ", g.cls_name, kDummyArmor, ratio, ref, ToString(v));
            if (sunder)
            {
                auto const s = sunder->aggs.find(probe);
                if (s != sunder->aggs.end() && s->second.N(Outcome::Hit) >= 15 && a0 > 0.0)
                {
                    double const rs = s->second.AvgNormal() / a0, refs = 1.0 - VanillaArmorReduction(kSunderedArmor, 60);
                    Verdict const vs = RatioVerdict(rs, refs, 8.0);
                    TC_LOG_INFO("playerbot.v2", "[dmg_parity] armor_ratio class={} probe=\"{}\" damage at {} armor (five Sunders) / at 0 armor = {:.3f}, vanilla {:.3f} verdict={}", g.cls_name,
                                probe ? SpellNameOf(probe) : std::string("white swing"), kSunderedArmor, rs, refs, ToString(vs));
                    if (vs != Verdict::Ok)
                        g.failure += fmt::format("{} physical damage at {} armor is {:.3f} of unarmored, vanilla {:.3f} ({}); ", g.cls_name, kSunderedArmor, rs, refs, ToString(vs));
                }
            }
        }
        else
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] armor_ratio class={} no physical attack landed 15 normal hits in both armor legs: nothing to compare", g.cls_name);
    }

    // The reference ranges: a gear leg outside its range is a finding.
    for (LegResult const& r : g.results)
        if (r.verdict == RangeVerdict::Below || r.verdict == RangeVerdict::Above)
            g.failure += fmt::format("{} {} leg {:.0f} dps is {} (vanilla {}); ", g.cls_name, r.leg.label, r.dps, ToString(r.verdict), r.range);
    (void)p;
}

// ---- Sweeps ----

double Mean(std::vector<double> const& v)
{
    double s = 0.0;
    for (double x : v) s += x;
    return v.empty() ? 0.0 : s / double(v.size());
}

void LogTalents(Player* p)
{
    for (TalentRef const& t : kTalents)
    {
        if (t.cls != g.cls)
            continue;
        std::uint32_t const id = FindKnownSpell(p, t.name);
        if (!id)
        {
            TC_LOG_INFO("playerbot.v2", "[talent_audit] class={} talent=\"{}\" not in the spellbook (the bot's build does not have it, or the talent spell is named otherwise)", g.cls_name, t.name);
            continue;
        }
        SpellInfo const* info = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
        std::string effects;
        bool any = false, match = false;
        for (SpellEffectInfo const& e : info->GetEffects())
        {
            if (!e.IsEffect())
                continue;
            any = true;
            double const value = static_cast<double>(e.CalcValue(p));
            effects += fmt::format(" [effect={} aura={} misc={} value={:.0f}]", int(e.Effect), int(e.ApplyAuraName), int(e.MiscValue), value);
            if (t.expect > 0.0 && std::fabs(std::fabs(value) - t.expect) < 0.5)
                match = true;
        }
        char const* verdict = !any ? "NO_EFFECT" : t.expect > 0.0 ? (match ? "ok" : "value_differs") : "listed";
        TC_LOG_INFO("playerbot.v2", "[talent_audit] class={} talent=\"{}\" spell={} rank={} expect={} verdict={}{}", g.cls_name, t.name, id, int(info->GetRank()),
                    t.expect > 0.0 ? fmt::format("{:.0f}", t.expect) : std::string("-"), verdict, effects);
        if (!any)
            g.failure += fmt::format("talent {} ({}) has no effect at all; ", t.name, id);
    }
}

// Starts one point of the current sweep item.
bool SweepSetup(Player* p)
{
    SweepItem const& it = g.items[g.item];
    g.cast_spell_id = FindKnownSpell(p, it.spell);
    if (!g.cast_spell_id)
    {
        g.failure += fmt::format("{} does not know {}; ", g.cls_name, it.spell);
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] sweep class={} spell=\"{}\" is not in the spellbook", g.cls_name, it.spell);
        return false;
    }
    g.rep = 0;
    g.samples.clear();
    g.cast_ok = g.cast_failures = 0;
    g.last_fail.clear();
    return true;
}

// The raw core refusal behind a cast_spell result (API::last_cast_fail: a SpellCastResult, or one of the API's own gate codes).
std::string CastFailText(std::uint32_t raw)
{
    switch (raw)
    {
        case SPELL_FAILED_OUT_OF_RANGE:      return "SPELL_FAILED_OUT_OF_RANGE";
        case SPELL_FAILED_LINE_OF_SIGHT:     return "SPELL_FAILED_LINE_OF_SIGHT";
        case SPELL_FAILED_UNIT_NOT_INFRONT:  return "SPELL_FAILED_UNIT_NOT_INFRONT";
        case SPELL_FAILED_NOT_BEHIND:        return "SPELL_FAILED_NOT_BEHIND";
        case SPELL_FAILED_ONLY_SHAPESHIFT:   return "SPELL_FAILED_ONLY_SHAPESHIFT";
        case SPELL_FAILED_NO_POWER:          return "SPELL_FAILED_NO_POWER";
        case SPELL_FAILED_NOT_READY:         return "SPELL_FAILED_NOT_READY";
        case SPELL_FAILED_BAD_TARGETS:       return "SPELL_FAILED_BAD_TARGETS";
        case SPELL_FAILED_MOVING:            return "SPELL_FAILED_MOVING";
        case SPELL_FAILED_STUNNED:           return "SPELL_FAILED_STUNNED";
        case SPELL_FAILED_AFFECTING_COMBAT:  return "SPELL_FAILED_AFFECTING_COMBAT";
        default:                             return fmt::format("code {}", raw);
    }
}

// A spell that needs a stance or form (Execute, Ferocious Bite, Rip, Maul) is cast from one the caster is not in: shift into a form that
// satisfies it, as a player would. Triggered, so the shift costs nothing.
void EnsureForm(Player* p, SpellInfo const* info)
{
    std::uint64_t const mask = info->Stances;
    if (!mask)
        return;
    auto fits = [&](int form) { return form > 0 && form < 64 && (mask & (std::uint64_t(1) << (form - 1))); };
    if (fits(int(p->GetShapeshiftForm())))
        return;
    for (auto const& [id, ps] : p->GetSpellMap())
    {
        if (ps.state == PLAYERSPELL_REMOVED || ps.disabled)
            continue;
        SpellInfo const* si = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
        if (!si || si->IsPassive())
            continue;
        for (SpellEffectInfo const& e : si->GetEffects())
            if (e.ApplyAuraName == SPELL_AURA_MOD_SHAPESHIFT && fits(e.MiscValue))
            {
                p->CastSpell(p, id, true);
                TC_LOG_INFO("playerbot.v2", "[dmg_parity] sweep class={} shifts with {} ({}) for {}", g.cls_name, SpellNameOf(id), id, SpellNameOf(info->Id));
                return;
            }
    }
    TC_LOG_INFO("playerbot.v2", "[dmg_parity] sweep class={} {} needs a form (mask 0x{:x}) and none of the known spells gives it", g.cls_name, SpellNameOf(info->Id), mask);
}

// One cast of the sweep. False when it was refused.
bool SweepCastOnce(Player* p, Creature* d)
{
    SweepItem const& it = g.items[g.item];
    int const pts = it.points[g.point];
    SpellInfo const* info = sSpellMgr->GetSpellInfo(g.cast_spell_id, DIFFICULTY_NONE);
    p->GetSpellHistory()->ResetAllCooldowns();
    if (info)
        EnsureForm(p, info);
    TopUp(p);
    if (it.res == Res::Combo)
        p->SetPower(POWER_COMBO_POINTS, pts);
    else if (it.res == Res::Rage)
        p->SetPower(POWER_RAGE, pts * 10);
    d->SetHealth(it.low_health ? d->GetMaxHealth() / 7 : d->GetMaxHealth());
    if (!it.watch_aura)   // a watched aura (the debuff a crit leaves on the dummy) has to survive from cast to cast
        d->RemoveAllAuras();
    // The dummy stands where the spell reaches: between its minimum and maximum range (a melee finisher with a minimum range, or a bot that
    // drifted, is out of range otherwise).
    if (info)
    {
        float const lo = info->GetMinRange(false), hi = info->GetMaxRange(false, p);
        float const want = std::clamp(3.0f, lo + 1.0f, std::max(lo + 1.0f, hi - 1.0f));
        if (std::fabs(p->GetExactDist(d) - want) > 1.5f)
            d->NearTeleportTo(p->GetNearPosition(want, 0.f));
    }
    p->SetFacingToObject(d);
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (!it.watch_aura)
            g_aggs.clear();
    }
    g.armor_before = double(d->GetArmor());
    ::Playerbot::API api(p);
    ::Playerbot::Result const r = api.cast_spell(g.cast_spell_id, d->GetGUID());
    if (r != ::Playerbot::Result::Ok)
    {
        ++g.cast_failures;
        g.last_fail = fmt::format("result {} = {}", int(r), CastFailText(api.last_cast_fail()));
        if (g.cast_failures == 1 || g.cast_failures == 8)
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] sweep refusal class={} spell=\"{}\" id={} api_result={} core={} dist={:.1f} range={:.1f}-{:.1f} los={} infront={} form={} combo={} rage={} energy={}",
                        g.cls_name, it.spell, g.cast_spell_id, int(r), CastFailText(api.last_cast_fail()), p->GetExactDist(d), info ? info->GetMinRange(false) : 0.f,
                        info ? info->GetMaxRange(false, p) : 0.f, p->IsWithinLOSInMap(d), p->HasInArc(float(M_PI), d), int(p->GetShapeshiftForm()),
                        p->GetPower(POWER_COMBO_POINTS), p->GetPower(POWER_RAGE), p->GetPower(POWER_ENERGY));
        return false;
    }
    ++g.cast_ok;
    return true;
}

// Reads the value of the cast that just finished.
void SweepSample(Player* p, Creature* d)
{
    SweepItem const& it = g.items[g.item];
    SpellAgg a;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        auto const f = g_aggs.find(g.cast_spell_id);
        if (f != g_aggs.end())
            a = f->second;
        // The damage can be dealt by another id than the one cast (a trigger spell, or a rank of the same name): take it by name.
        if (!a.N(Outcome::Hit) && !a.ticks)
        {
            std::string const cast_name = SpellNameOf(g.cast_spell_id);
            for (auto const& [key, agg] : g_aggs)
                if (key && !(key & kPetFlag) && key != g.cast_spell_id && SpellNameOf(key) == cast_name && (agg.N(Outcome::Hit) || agg.ticks))
                {
                    a = agg;
                    break;
                }
        }
        if (it.measure == Measure::Damage && !a.N(Outcome::Hit) && !a.ticks && g.samples.empty())
        {
            std::string seen;
            for (auto const& [key, agg] : g_aggs)
                seen += fmt::format(" {}({}) attempts={} hits={}", key & ~kPetFlag, key & ~kPetFlag ? SpellNameOf(key & ~kPetFlag) : std::string("white swing"), agg.Attempts(), agg.N(Outcome::Hit));
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] sweep class={} spell=\"{}\" id={} point={} produced no damage sample; damage seen:{}", g.cls_name, it.spell, g.cast_spell_id,
                        it.points[g.point], seen.empty() ? " none" : seen);
        }
    }
    switch (it.measure)
    {
        case Measure::Damage:
            if (!it.watch_aura && a.N(Outcome::Hit))
                g.samples.push_back(a.AvgOf(Outcome::Hit));
            break;
        case Measure::DotTotal:
            if (a.ticks)
                g.samples.push_back(double(a.tick_damage));
            break;
        case Measure::AuraDuration:
        {
            SpellInfo const* info = sSpellMgr->GetSpellInfo(g.cast_spell_id, DIFFICULTY_NONE);
            Unit* holder = info && info->IsPositive() ? static_cast<Unit*>(p) : static_cast<Unit*>(d);
            if (Aura* aura = holder->GetAura(g.cast_spell_id, p->GetGUID()))
                g.samples.push_back(double(aura->GetMaxDuration()) / 1000.0);
            else if (Aura* any = holder->GetAura(g.cast_spell_id))
                g.samples.push_back(double(any->GetMaxDuration()) / 1000.0);
            break;
        }
        case Measure::ArmorDrop:
            g.samples.push_back(g.armor_before - double(d->GetArmor()));
            break;
    }
}

// Finishes a sweep item: the series, its verdict, the failure when it does not scale.
void SweepItemDone()
{
    SweepItem const& it = g.items[g.item];
    if (it.watch_aura)
    {
        SpellAgg without, with;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            if (auto f = g_aggs.find(g.cast_spell_id); f != g_aggs.end()) without = f->second;
            if (auto f = g_aggs_watched.find(g.cast_spell_id); f != g_aggs_watched.end()) with = f->second;
        }
        double const ratio = without.AvgNormal() > 0.0 ? with.AvgNormal() / without.AvgNormal() : 0.0;
        bool const enough = with.N(Outcome::Hit) >= 8 && without.N(Outcome::Hit) >= 8;
        Verdict const v = enough ? RatioVerdict(ratio, it.expect_ratio, 10.0) : Verdict::NoData;
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] aura_ratio class={} spell=\"{}\" watched=\"{}\" hit avg with it {:.0f} ({} hits), without {:.0f} ({} hits): ratio {:.3f}, expected {:.2f} verdict={} (crits seen {})",
                    g.cls_name, it.spell, it.watch_aura, with.AvgNormal(), with.N(Outcome::Hit), without.AvgNormal(), without.N(Outcome::Hit), ratio, it.expect_ratio, ToString(v),
                    with.N(Outcome::Crit) + without.N(Outcome::Crit));
        if (v != Verdict::Ok)
            g.failure += fmt::format("{} {} with {} against without: ratio {:.3f}, expected {:.2f} ({}, {} watched hits, {} crits); ", g.cls_name, it.spell, it.watch_aura, ratio,
                                     it.expect_ratio, ToString(v), with.N(Outcome::Hit), with.N(Outcome::Crit) + without.N(Outcome::Crit));
        return;
    }
    std::string text;
    for (double v : g.series)
        text += fmt::format("{}{:.0f}", text.empty() ? "" : ", ", v);
    Scaling const s = ScalingVerdict(g.series);
    TC_LOG_INFO("playerbot.v2", "[dmg_parity] finisher class={} spell=\"{}\" resource={} points=[{}] value=[{}] verdict={} step_share={:.2f}", g.cls_name, it.spell,
                it.res == Res::Combo ? "combo" : "rage", [&]{ std::string t; for (int p : it.points) t += fmt::format("{}{}", t.empty() ? "" : ",", p); return t; }(), text,
                ToString(s), LinearStepShare(g.series));
    if (s != Scaling::Scales)
        g.failure += fmt::format("{} {} does not scale with {}: [{}] ({}); ", g.cls_name, it.spell, it.res == Res::Combo ? "combo points" : "rage", text, ToString(s));
}

// ---- Economy: mana pool, regen, costs, heals ----

double ManaCostOf(Player* p, SpellInfo const* i)
{
    for (SpellPowerCost const& c : i->CalcPowerCost(p, i->GetSchoolMask()))
        if (c.Power == POWER_MANA)
            return double(c.Amount);
    return 0.0;
}

// The scenario measures regeneration, not reach: the dummy is summoned once, 18 y in front of where the bot happened to stand, and a bot
// that was placed against a wall, a slope or a drop (runall 2026-10-11: priest, shaman and warlock "line of sight", druid beyond range)
// has it behind geometry or too far. As the sweeps do (SweepCast), put the dummy where the spell reaches it: within the spell's range
// and in line of sight, trying a few distances and directions. True when the dummy is reachable afterwards.
bool PlaceDummyInReach(Player* p, Creature* d, SpellInfo const* info)
{
    float const range = std::max(5.0f, info->GetMaxRange(info->IsPositive(), p));
    auto reachable = [&] { return p->GetExactDist(d) <= range - 1.0f && p->IsWithinLOSInMap(d); };
    if (reachable())
        return true;
    float const base = std::min(18.0f, range - 2.0f);
    for (float const distance : { base, 12.0f, 8.0f, 5.0f, 3.0f })
        for (float const angle : { 0.0f, 1.5708f, -1.5708f, 3.1416f })
        {
            if (distance > range - 1.0f)
                continue;
            d->NearTeleportTo(p->GetNearPosition(distance, angle));
            if (reachable())
                return true;
        }
    return false;
}

char const* MainSpellFor(std::uint8_t cls)
{
    switch (cls)
    {
        case 5:  return "Smite";
        case 2:  return "Holy Light";       // no damage spell worth the name: a heal on itself
        case 7:  return "Lightning Bolt";
        case 11: return "Wrath";
        case 8:  return "Frostbolt";
        case 9:  return "Shadow Bolt";
        default: return "";
    }
}

constexpr char const* kCostSpells[] = { "Flash of Light", "Holy Light", "Heal", "Greater Heal", "Flash Heal", "Renew", "Healing Touch", "Rejuvenation",
                                        "Regrowth", "Healing Wave", "Lesser Healing Wave", "Shadow Bolt", "Frostbolt", "Fireball", "Smite",
                                        "Lightning Bolt", "Wrath", "Mind Blast", "Arcane Missiles" };
constexpr char const* kHealSpells[] = { "Flash Heal", "Heal", "Greater Heal", "Flash of Light", "Holy Light", "Healing Touch", "Lesser Healing Wave", "Healing Wave" };

// The cost as the client data holds it, before the caster's modifiers: SpellPower ManaCost and PowerCostPct (percent of base mana).
std::string RawManaCost(SpellInfo const* i)
{
    for (SpellPowerEntry const* pw : i->PowerCosts)
        if (pw && pw->PowerType == POWER_MANA)
            return fmt::format("raw_mana_cost={} raw_cost_pct={:.2f} raw_cost_per_level={}", pw->ManaCost, pw->PowerCostPct, pw->ManaCostPerLevel);
    return "raw_mana_cost=none";
}

// Every effect of a spell as the data holds it: the effect type, base points and the spell power coefficient.
void LogSpellEffects(Player* p, std::uint32_t id)
{
    SpellInfo const* i = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
    if (!i)
    {
        TC_LOG_INFO("playerbot.v2", "[econ] effects id={} is not in this world's spell data", id);
        return;
    }
    TC_LOG_INFO("playerbot.v2", "[econ] effects spell=\"{}\" id={} rank={} cast_ms={} {}", SpellNameOf(id), id, int(i->GetRank()), i->CalcCastTime(), RawManaCost(i));
    for (SpellEffectInfo const& e : i->GetEffects())
        if (e.IsEffect())
            TC_LOG_INFO("playerbot.v2", "[econ] effects spell={} index={} effect_type={} aura={} base_points={:.2f} points_per_level={:.2f} calc_value={:.0f} bonus_coefficient={:.4f} bonus_coefficient_from_ap={:.4f}",
                        id, int(e.EffectIndex), int(e.Effect), int(e.ApplyAuraName), double(e.BasePoints), double(e.RealPointsPerLevel),
                        static_cast<double>(e.CalcValue(p)), double(e.BonusCoefficient), double(e.BonusCoefficientFromAP));
}

// The auras that change mana regeneration, summed: aura 85 (MOD_POWER_REGEN, flat mana per 5 s) and aura 134 (MOD_MANA_REGEN_INTERRUPT,
// the percent of spirit regen kept while casting). Each source is named.
void LogRegenAuras(Player* p)
{
    double mp5 = 0.0, interrupt = 0.0;
    std::string src85, src134;
    for (AuraEffect const* a : p->GetAuraEffectsByType(SPELL_AURA_MOD_POWER_REGEN))
        if (a->GetMiscValue() == POWER_MANA)
        {
            mp5 += static_cast<double>(a->GetAmount());
            src85 += fmt::format(" {}({})={:.0f}", a->GetId(), SpellNameOf(a->GetId()), static_cast<double>(a->GetAmount()));
        }
    for (AuraEffect const* a : p->GetAuraEffectsByType(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT))
    {
        interrupt += static_cast<double>(a->GetAmount());
        src134 += fmt::format(" {}({})={:.0f}", a->GetId(), SpellNameOf(a->GetId()), static_cast<double>(a->GetAmount()));
    }
    TC_LOG_INFO("playerbot.v2", "[econ] regen_auras class={} aura85_mp5_total={:.0f} sources:{} aura134_casting_pct_total={:.0f} sources:{} (gear MP5 and spirit-while-casting come from the item and stat system, not from auras)",
                g.cls_name, mp5, src85.empty() ? " none" : src85, interrupt, src134.empty() ? " none" : src134);
}

// Every modifier that touches the mana cost of a spell: the SPELLMOD_COST auras (flat and percent, by class mask or spell label) and the
// school cost auras (72 percent, 73 flat), each with its source spell and value. This is what turns the client's raw cost into the final one.
std::string CostModifiers(Player* p, SpellInfo const* i)
{
    std::string out;
    struct Kind { AuraType type; char const* name; };
    static constexpr Kind kSpellMods[] = {
        { SPELL_AURA_ADD_FLAT_MODIFIER, "spellmod_flat" }, { SPELL_AURA_ADD_PCT_MODIFIER, "spellmod_pct" },
        { SPELL_AURA_ADD_FLAT_MODIFIER_BY_SPELL_LABEL, "spellmod_flat_label" }, { SPELL_AURA_ADD_PCT_MODIFIER_BY_SPELL_LABEL, "spellmod_pct_label" } };
    for (Kind const& k : kSpellMods)
        for (AuraEffect const* a : p->GetAuraEffectsByType(k.type))
            if (a->GetMiscValue() == int32(SpellModOp::PowerCost0) && a->IsAffectingSpell(i))
                out += fmt::format(" {}:{}({})={:.0f}", k.name, a->GetId(), SpellNameOf(a->GetId()), static_cast<double>(a->GetAmount()));
    for (AuraEffect const* a : p->GetAuraEffectsByType(SPELL_AURA_MOD_POWER_COST_SCHOOL_PCT))
        if (a->GetMiscValue() & int32(i->GetSchoolMask()))
            out += fmt::format(" school_cost_pct:{}({})={:.0f}", a->GetId(), SpellNameOf(a->GetId()), static_cast<double>(a->GetAmount()));
    for (AuraEffect const* a : p->GetAuraEffectsByType(SPELL_AURA_MOD_POWER_COST_SCHOOL))
        if (a->GetMiscValue() & int32(i->GetSchoolMask()))
            out += fmt::format(" school_cost_flat:{}({})={:.0f}", a->GetId(), SpellNameOf(a->GetId()), static_cast<double>(a->GetAmount()));
    return out.empty() ? std::string(" none") : out;
}

// Once per run: the pieces of the mana regeneration the stat system multiplies (Player::UpdatePowerRegen), so a ratio that is off can be
// read against its factors.
void LogRegenModel(Player* p)
{
    double const spirit_regen = double(p->OCTRegenMPPerSpirit());
    double const aura379 = double(p->GetTotalAuraMultiplier(SPELL_AURA_MOD_MANA_REGEN_PCT));
    double const flat_mp5 = double(p->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA));
    double const pct110 = double(p->GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA));
    double const interrupt = std::min(100.0, double(p->GetTotalAuraModifier(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT)));
    TC_LOG_INFO("playerbot.v2",
        "[econ] regen_model class={} spirit={:.0f} spirit_regen_per_s={:.3f} mod_mana_regen_pct_379_multiplier={:.3f} mp5_from_aura85={:.0f} (per s {:.3f}) power_regen_percent_110_multiplier={:.3f} "
        "rate_power_mana={:.3f} regen_while_casting_pct_134={:.0f} => out_of_combat_per_s={:.3f} casting_per_s={:.3f} (the sheet value GetPowerRegen: {:.3f})",
        g.cls_name, p->GetStat(STAT_SPIRIT), spirit_regen, aura379, flat_mp5, flat_mp5 / 5.0, pct110, double(sWorld->getRate(RATE_POWER_MANA)), interrupt,
        (spirit_regen * aura379 * pct110 * double(sWorld->getRate(RATE_POWER_MANA))) + flat_mp5 / 5.0,
        (spirit_regen * aura379 * interrupt / 100.0 * pct110 * double(sWorld->getRate(RATE_POWER_MANA))) + flat_mp5 / 5.0, double(p->GetPowerRegen(POWER_MANA)));
    for (AuraType t : { SPELL_AURA_MOD_POWER_REGEN_PERCENT, SPELL_AURA_MOD_MANA_REGEN_PCT })
        for (AuraEffect const* a : p->GetAuraEffectsByType(t))
            TC_LOG_INFO("playerbot.v2", "[econ] regen_aura class={} type={} id={}({}) value={:.0f} misc={}", g.cls_name, int(t), a->GetId(), SpellNameOf(a->GetId()),
                        static_cast<double>(a->GetAmount()), a->GetMiscValue());
}

// Food and drink are auras that break on standing; a sitting bot or any such aura inside a window means the bot ate or drank in it.
void NoteRecovery(Player* p, std::set<std::uint32_t>& seen, bool& sat)
{
    for (auto const& kv : p->GetAppliedAuras())
        if (kv.second->GetBase()->GetSpellInfo()->HasAuraInterruptFlag(SpellAuraInterruptFlags::Standing))
            seen.insert(kv.first);
    if (p->GetStandState() != UNIT_STAND_STATE_STAND)
        sat = true;
}

std::string RecoveryText(std::set<std::uint32_t> const& seen, bool sat)
{
    std::string out = sat ? "sat" : "stood";
    for (std::uint32_t id : seen)
        out += fmt::format(" {}({})", id, SpellNameOf(id));
    return out;
}

// The mana pool against the base the server's table holds, and what the main spells cost.
void EconStatic(Player* p)
{
    std::uint32_t table_base = 0;
    sObjectMgr->GetPlayerClassLevelInfo(p->GetClass(), p->GetLevel(), table_base);
    double const int_ = p->GetStat(STAT_INTELLECT), spirit = p->GetStat(STAT_SPIRIT), maxm = double(p->GetMaxPower(POWER_MANA));
    // Vanilla: base mana of the level + 20 for the first 20 intellect + 15 for each point above (StatSystem.cpp).
    double const implied_base = maxm - (20.0 + 15.0 * (int_ - 20.0));
    TC_LOG_INFO("playerbot.v2", "[econ] mana class={} level={} max_mana={:.0f} intellect={:.0f} spirit={:.0f} implied_base={:.0f} table_base={} (compare table_base with the VMaNGOS player_classlevelstats.basemana of this class and level)",
                g.cls_name, uint32(p->GetLevel()), maxm, int_, spirit, implied_base, table_base);
    LogRegenModel(p);
    if (std::fabs(implied_base - double(table_base)) > 0.02 * std::max(1.0, double(table_base)))
        g.failure += fmt::format("{} max mana {:.0f} with intellect {:.0f} implies a base of {:.0f}, the table says {}; ", g.cls_name, maxm, int_, implied_base, table_base);
    for (char const* name : kCostSpells)
    {
        std::uint32_t const id = FindKnownSpell(p, name);
        if (!id)
            continue;
        SpellInfo const* i = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
        double const cost = ManaCostOf(p, i);
        double value = 0.0;
        for (SpellEffectInfo const& e : i->GetEffects())
            if (e.IsEffect() && (e.Effect == SPELL_EFFECT_HEAL || e.Effect == SPELL_EFFECT_SCHOOL_DAMAGE))
            {
                value = static_cast<double>(e.CalcValue(p));
                break;
            }
        TC_LOG_INFO("playerbot.v2", "[econ] cost spell=\"{}\" id={} rank={} rank_id={} mana_cost={:.0f} {} cast_ms={} base_value={:.0f} casts_from_full_mana={:.1f} cost_modifiers:{} (compare mana_cost with the VMaNGOS spell_template manaCost)",
                    name, id, int(i->GetRank()), id, cost, RawManaCost(i), i->CalcCastTime(), value, cost > 0.0 ? maxm / cost : 0.0, CostModifiers(p, i));
    }
    // One-off: the heals that are off against VMaNGOS (Flash of Light, Holy Light about 90% high; Healing Wave about 25% low).
    for (std::uint32_t const id : { 19943u, 10329u, 10396u })
        LogSpellEffects(p, id);
}

bool EconStep(Player* p, Creature* d, std::string& failure)
{
    Econ& e = g.econ;
    ++e.t;
    Powers const mana = POWER_MANA;
    double const cur = double(p->GetPower(mana));
    double const maxm = double(p->GetMaxPower(mana));
    auto next = [&](int phase) { e.phase = phase; e.t = 0; e.cast_sent = false; e.recov.clear(); e.sat = false; };
    if (e.phase <= 2 && (e.phase == 2 || e.t >= 8))
        NoteRecovery(p, e.recov, e.sat);
    switch (e.phase)
    {
        case 0:   // out of combat: 7 s for any five-second window to pass, then ten seconds counted
            if (e.t == 1)
            {
                p->AttackStop();
                p->CombatStop(true);
                p->SetPower(mana, std::uint32_t(maxm * 0.2));
            }
            if (e.t == 8) e.m0 = cur;
            if (e.t == 18)
            {
                e.r_oc = cur - e.m0;
                TC_LOG_INFO("playerbot.v2", "[econ] regen class={} state=out_of_combat mana_gained_10s={:.0f} (spirit {:.0f}) ate_or_drank_in_window: {}", g.cls_name, e.r_oc, p->GetStat(STAT_SPIRIT), RecoveryText(e.recov, e.sat));
                next(1);
            }
            return false;
        case 1:   // in combat, nothing cast for more than six seconds: vanilla gives the whole spirit regen
            if (e.t == 1)
                p->SetPower(mana, std::uint32_t(maxm * 0.2));
            p->Attack(d, true);
            p->SetInCombatWith(d);
            d->SetInCombatWith(p);
            if (e.t >= 8 && p->IsInCombat())
                ++e.ic_seconds_in_combat;
            if (e.t == 8) e.m0 = cur;
            if (e.t == 18)
            {
                e.r_ic = cur - e.m0;
                TC_LOG_INFO("playerbot.v2", "[econ] regen class={} state=in_combat_idle mana_gained_10s={:.0f} seconds_in_combat={} of 10 ate_or_drank_in_window: {}", g.cls_name, e.r_ic, e.ic_seconds_in_combat, RecoveryText(e.recov, e.sat));
                p->AttackStop();
                p->CombatStop(true);
                next(2);
            }
            return false;
        case 2:   // casting without a pause: only the aura share regenerates
        {
            if (e.t == 1)
            {
                e.main_spell = FindKnownSpell(p, MainSpellFor(g.cls));
                if (!e.main_spell)
                {
                    TC_LOG_INFO("playerbot.v2", "[econ] regen class={} state=casting: {} is not in the spellbook", g.cls_name, MainSpellFor(g.cls));
                    next(3);
                    return false;
                }
                SpellInfo const* i = sSpellMgr->GetSpellInfo(e.main_spell, DIFFICULTY_NONE);
                e.cost = ManaCostOf(p, i);
                e.casts = 0;
                e.spent = 0.0;
                e.cast_attempts = 0;
                e.cast_gcd_ticks = 0;
                e.last_result = -1;
                e.last_fail = 0;
                p->SetPower(mana, std::uint32_t(maxm * 0.7));
                e.m0 = double(p->GetPower(mana));
                e.cast_starts = 0;
                e.was_casting = false;
                e.auras_before.clear();
                for (auto const& kv : p->GetAppliedAuras())
                    e.auras_before.push_back(kv.first);
                LogRegenAuras(p);
                {
                    std::lock_guard<std::mutex> lock(g_mu);
                    g_energy.clear();
                }
                g_energy_on = true;
            }
            {
                bool const now_casting = p->HasUnitState(UNIT_STATE_CASTING);
                if (now_casting && !e.was_casting)
                    ++e.cast_starts;
                e.was_casting = now_casting;
            }
            if (e.main_spell)
            {
                SpellInfo const* i = sSpellMgr->GetSpellInfo(e.main_spell, DIFFICULTY_NONE);
                bool const casting = p->HasUnitState(UNIT_STATE_CASTING);
                if (e.t < 16 && !casting && !p->GetSpellHistory()->HasGlobalCooldown(i))
                {
                    if (!i->IsPositive() && !PlaceDummyInReach(p, d, i))
                        TC_LOG_INFO("playerbot.v2", "[econ] regen_window class={} t={} no reachable spot for the dummy: dist={:.1f} range={:.1f} los={} bot=({:.1f},{:.1f},{:.1f}) map={}",
                                    g.cls_name, e.t, p->GetExactDist(d), i->GetMaxRange(false, p), p->IsWithinLOSInMap(d), p->GetPositionX(), p->GetPositionY(),
                                    p->GetPositionZ(), p->GetMapId());
                    p->SetFacingToObject(d);
                    p->GetSpellHistory()->ResetAllCooldowns();
                    ::Playerbot::API api(p);
                    ObjectGuid const target = i->IsPositive() ? p->GetGUID() : d->GetGUID();
                    ++e.cast_attempts;
                    // What this cast costs now (a Clearcasting proc makes it free); the window adds what was really paid back, not casts x the first cost.
                    double const cost_now = ManaCostOf(p, i);
                    ::Playerbot::Result const result = api.cast_spell(e.main_spell, target);
                    if (result == ::Playerbot::Result::Ok)
                    {
                        ++e.casts;
                        e.spent += cost_now;
                    }
                    else
                    {
                        e.last_result = int(result);
                        e.last_fail = api.last_cast_fail();
                        TC_LOG_INFO("playerbot.v2", "[econ] regen_window class={} t={} cast of {} ({}) refused: result={} spell_cast_result={} dist_to_dummy={:.1f} dummy_alive={} mana={:.0f}/{:.0f} moving={} stand_state={}",
                                    g.cls_name, e.t, e.main_spell, SpellNameOf(e.main_spell), e.last_result, e.last_fail, p->GetDistance(d), d->IsAlive(), cur, maxm,
                                    p->isMoving(), int(p->GetStandState()));
                    }
                }
                else if (e.t < 16)
                    ++e.cast_gcd_ticks;
                if (e.t >= 16 && !casting)
                {
                    // The window is longer than the ten seconds the other two states count: bring it to the same ten (m0 was read on tick 1).
                    if (e.casts == 0)
                    {
                        // Nothing was cast, so the window measured an idle caster: that says nothing about the five second rule. Report the
                        // missing casts (the refusals are logged above) and leave r_cast unset so the regen comparison is not made on it.
                        g.failure += fmt::format("{} cast nothing in the casting-regen window ({} attempts, {} ticks skipped for the global cooldown or a running cast, last result {} / spell cast result {}); ",
                                                 g.cls_name, e.cast_attempts, e.cast_gcd_ticks, e.last_result, e.last_fail);
                        TC_LOG_INFO("playerbot.v2", "[econ] regen_window class={} spell=\"{}\" id={} no cast was made: attempts={} gcd_or_casting_ticks={} last_result={} spell_cast_result={}",
                                    g.cls_name, MainSpellFor(g.cls), e.main_spell, e.cast_attempts, e.cast_gcd_ticks, e.last_result, e.last_fail);
                        g_energy_on = false;
                        next(3);
                        return false;
                    }
                    double const window_s = double(std::max<std::uint32_t>(1, e.t - 1));
                    double const gained_window = cur - e.m0 + e.spent;
                    e.r_cast = gained_window * 10.0 / window_s;
                    TC_LOG_INFO("playerbot.v2", "[econ] regen class={} state=casting spell=\"{}\" casts={} cost_each={:.0f} window_s={:.0f} mana_gained_window={:.0f} (net change {:.0f} plus the {:.0f} spent) mana_gained_10s={:.0f} ate_or_drank_in_window: {}",
                                g.cls_name, MainSpellFor(g.cls), e.casts, e.cost, window_s, gained_window, cur - e.m0, e.spent, e.r_cast, RecoveryText(e.recov, e.sat));
                    g_energy_on = false;
                    // What else put mana in or fired in the window. The bot logic is detached, so anything listed here is a finding.
                    std::string energy;
                    double energy_total = 0.0;
                    {
                        std::lock_guard<std::mutex> lock(g_mu);
                        for (auto const& kv : g_energy)
                        {
                            energy += fmt::format(" {}({})={}", kv.first, SpellNameOf(kv.first), kv.second);
                            energy_total += double(kv.second);
                        }
                    }
                    std::string new_auras;
                    for (auto const& kv : p->GetAppliedAuras())
                        if (std::find(e.auras_before.begin(), e.auras_before.end(), kv.first) == e.auras_before.end())
                            new_auras += fmt::format(" {}({})", kv.first, SpellNameOf(kv.first));
                    TC_LOG_INFO("playerbot.v2", "[econ] regen_window class={} energize_by_spell:{} energize_total={:.0f} (of the mana_gained above) casts_accepted={} casts_seen_starting={} (sampled once a second, so back-to-back casts count once) auras_gained_in_window:{} (any item mana use shows as an energize by its spell; nothing else is meant to fire here)",
                                g.cls_name, energy.empty() ? " none" : energy, energy_total, e.casts, e.cast_starts, new_auras.empty() ? " none" : new_auras);
                    if (e.cast_starts != e.casts)
                        TC_LOG_INFO("playerbot.v2", "[econ] regen_window class={} {} casts were accepted and {} seen starting; if casts truly failed to begin, mana_gained is overcounted by up to {:.0f}",
                                    g.cls_name, e.casts, e.cast_starts, double(e.casts - e.cast_starts) * e.cost);
                    next(3);
                }
            }
            return false;
        }
        case 3:   // heal sizes on itself
        {
            if (e.heals.empty() && e.heal_i == 0 && e.t == 1)
                for (char const* name : kHealSpells)
                    if (std::uint32_t const id = FindKnownSpell(p, name))
                        e.heals.push_back(id);
            if (e.heal_i >= e.heals.size())
            {
                next(4);
                return false;
            }
            std::uint32_t const id = e.heals[e.heal_i];
            SpellInfo const* i = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
            if (!e.cast_sent)
            {
                p->GetSpellHistory()->ResetAllCooldowns();
                p->SetPower(mana, std::uint32_t(maxm));
                p->SetHealth(std::max<std::uint64_t>(1, p->GetMaxHealth() / 20));
                e.health_before = std::uint32_t(p->GetHealth());
                ::Playerbot::API api(p);
                if (api.cast_spell(id, p->GetGUID()) != ::Playerbot::Result::Ok)
                {
                    TC_LOG_INFO("playerbot.v2", "[econ] heal spell=\"{}\" id={} cast refused", SpellNameOf(id), id);
                    ++e.heal_i;
                    e.rep = 0;
                    e.got.clear();
                    return false;
                }
                e.cast_sent = true;
                e.t = 0;
                return false;
            }
            if (e.t < 2 || p->HasUnitState(UNIT_STATE_CASTING))
                return false;
            e.got.push_back(double(p->GetHealth()) - double(e.health_before));
            e.cast_sent = false;
            if (++e.rep < 6)
                return false;
            // Done with this heal: the smallest result is a normal (not critical) heal.
            double lo = e.got.front(), sum = 0.0;
            for (double v : e.got) { lo = std::min(lo, v); sum += v; }
            double base = 0.0, base_points = 0.0, data_coeff = 0.0;
            for (SpellEffectInfo const& ef : i->GetEffects())
                if (ef.IsEffect() && ef.Effect == SPELL_EFFECT_HEAL)
                {
                    base = static_cast<double>(ef.CalcValue(p));
                    base_points = double(ef.BasePoints);
                    data_coeff = double(ef.BonusCoefficient);
                    break;
                }
            double const cast_s = std::min(double(i->CalcCastTime()) / 1000.0, 3.5);
            // The healing power the formula really sees: the healer's own (gear, auras) plus what the target's auras add (Blessing of Light and the like).
            int32 const own_power = p->SpellBaseHealingBonusDone(i->GetSchoolMask());
            double const target_power = double(p->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_HEALING, i->GetSchoolMask()));
            double const hp = double(own_power) + target_power;
            double const done_mod = double(p->SpellHealingPctDone(p, i));
            double const taken_mod = double(p->SpellHealingBonusTaken(p, i, 1000, HEAL)) / 1000.0;
            float coeff_mod = float(data_coeff) * 100.0f;
            p->ApplySpellMod(i, SpellModOp::BonusCoefficient, coeff_mod);
            double const coeff_used = double(coeff_mod) / 100.0;
            double const expected = (base + cast_s / 3.5 * hp) * done_mod * taken_mod;   // the vanilla coefficient (cast time / 3.5), the server's multipliers
            double const server_formula = (base + hp * coeff_used) * done_mod * taken_mod;   // the coefficient the spell data holds
            double const cost = ManaCostOf(p, i);
            Verdict const v = CompareValue(lo, expected, 12.0);
            std::string results;
            for (double r : e.got)
                results += fmt::format(" {:.0f}{}", r, r > 1.3 * lo ? "(crit?)" : "");
            TC_LOG_INFO("playerbot.v2", "[econ] heal spell=\"{}\" id={} rank={} min={:.0f} avg={:.0f} expected={:.0f} (base {:.0f} + {:.2f} x {:.0f} healing power, x {:.3f} done x {:.3f} taken) verdict={} mana_cost={:.0f} heal_per_mana={:.2f} casts_to_oom={:.1f}",
                        SpellNameOf(id), id, int(i->GetRank()), lo, sum / double(e.got.size()), expected, base, cast_s / 3.5, hp, done_mod, taken_mod, ToString(v), cost,
                        cost > 0.0 ? lo / cost : 0.0, cost > 0.0 ? maxm / cost : 0.0);
            TC_LOG_INFO("playerbot.v2", "[econ] heal_detail spell={} base_points={:.1f} base_roll={:.0f} healing_power_own={} healing_power_from_target_auras={:.0f} (DoneAdvertisedBenefit {:.0f}) data_coefficient={:.4f} coefficient_after_spellmods={:.4f} vanilla_coefficient={:.4f} "
                        "DoneTotalMod={:.3f} TakenTotalMod={:.3f} server_formula_heal={:.0f} results:{}",
                        id, base_points, base, own_power, target_power, hp, data_coeff, coeff_used, cast_s / 3.5, done_mod, taken_mod, server_formula, results);
            ++e.heal_i;
            e.rep = 0;
            e.got.clear();
            return false;
        }
        default:
        {
            // The regen rule of vanilla: after the five seconds, combat changes nothing.
            if (e.r_oc <= 0.0)
                g.failure += fmt::format("{} gained no mana out of combat in 10 s; ", g.cls_name);
            else
            {
                double const ratio = e.r_ic / e.r_oc;
                TC_LOG_INFO("playerbot.v2", "[econ] regen_check class={} in_combat_idle / out_of_combat = {:.2f} (vanilla 1.00), casting / out_of_combat = {:.2f} (vanilla: only the aura share)",
                            g.cls_name, ratio, e.r_cast >= 0.0 ? e.r_cast / e.r_oc : -1.0);
                if (std::fabs(ratio - 1.0) > 0.15)
                    g.failure += fmt::format("{} regenerates {:.0f}% of its out-of-combat mana while idle in combat (vanilla 100%); ", g.cls_name, 100.0 * ratio);
                if (e.r_cast >= 0.0 && e.r_cast > 0.6 * e.r_oc)
                    g.failure += fmt::format("{} regenerates {:.0f}% of its out-of-combat mana while casting without a pause (the five second rule leaves only the aura share); ",
                                             g.cls_name, 100.0 * e.r_cast / e.r_oc);
            }
            failure = g.failure;
            return true;
        }
    }
}

// ---- Incoming damage ----

struct MobRow { std::uint32_t entry; char const* label; };
constexpr MobRow kMobs[] = { { 12099, "Firesworn" }, { 11982, "Magmadar" }, { 598, "Defias Miner" } };
constexpr int kIncomingDummyLegs = 3;
constexpr std::uint32_t kIncomingSettleS = 3, kIncomingDummyS = 90, kIncomingMobS = 120;
constexpr std::uint32_t kIncomingReachWaitS = 4;     // seconds out of melee range before the harness puts the attacker back next to the tank
constexpr std::uint32_t kIncomingMinSwings = 8;   // landed plain hits and crushing blows a leg needs before its mean says anything

Creature* SpawnMob(Player* p, std::uint32_t entry, std::string& error)
{
    if (!sObjectMgr->GetCreatureTemplate(entry))
    {
        error = fmt::format("creature {} is not in creature_template", entry);
        return nullptr;
    }
    TempSummon* s = p->GetMap()->SummonCreature(entry, p->GetNearPosition(3.0f, 0.f), nullptr, 0ms, p);
    if (!s)
    {
        std::string const why = UnusableReason(entry);
        error = fmt::format("could not summon creature {}: {}", entry, why.empty() ? std::string("Map::SummonCreature refused (see Server.log)") : why);
        TC_LOG_ERROR("playerbot.v2", "[incoming] {}", error);
        return nullptr;
    }
    g.dummy = s->GetGUID();
    return s;
}

// Landed non-critical swings: a normal hit or a crushing blow. A tank far below the attacker's weapon skill is crushed by most swings, so the
// "normal hit" count alone says nothing about how often the creature swung.
std::uint32_t NormalSwings(SpellAgg const& a) { return a.N(Outcome::Hit) + a.N(Outcome::Crushing); }

// Mean damage of those swings with the crushing blows brought back to a normal hit (vanilla: a crushing blow deals 150%).
double NormalEquivalent(SpellAgg const& a)
{
    std::uint32_t const n = NormalSwings(a);
    return n ? (double(a.damage[std::size_t(Outcome::Hit)]) + double(a.damage[std::size_t(Outcome::Crushing)]) / 1.5) / double(n) : 0.0;
}

// The creature stops fighting and the tank leaves combat, so the gear can change (RegearNow does nothing in combat).
void CeaseFire(Player* p)
{
    if (Creature* c = DummyCreature(p); c && c->IsAlive())
    {
        c->SetReactState(REACT_PASSIVE);
        c->AttackStop();
        c->CombatStop(true);
    }
    p->AttackStop();
    p->CombatStop(true);
}

// The attacker is hostile to the tank whatever faction the summon inherited, and its melee damage reads the range set (the creature's
// stat system recomputes the damage fields from the template, so the update runs after the set). `want` > 0 scales the base damage so that the
// effective range, with the creature's own damage modifiers, is that value.
void ArmMob(Creature* mob, float want)
{
    mob->SetFaction(14);   // monster faction, hostile to every player
    mob->SetReactState(REACT_AGGRESSIVE);
    if (want > 0.f)
    {
        mob->SetBaseAttackTime(BASE_ATTACK, 2000);
        mob->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, want);
        mob->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, want);
        float lo = 0.f, hi = 0.f;
        mob->CalculateMinMaxDamage(BASE_ATTACK, false, true, lo, hi);
        if (hi > 0.f && std::fabs(hi - want) > 0.02f * want)
        {
            float const scaled = want * want / hi;   // the effective damage is linear in the base range
            mob->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, scaled);
            mob->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, scaled);
        }
    }
    mob->UpdateDamagePhysical(BASE_ATTACK);
}

// The runtime state the crushing-blow roll reads (Unit::RollMeleeOutcomeAgainst step 7), as that code computes it: attacker level and scaling,
// the victim's live Defense and its parts, and the tmp value (attacker max skill - the victim's defense capped at its own max skill).
void LogCrushState(Player* p, Creature* mob, char const* when, SpellAgg const& m)
{
    if (!mob)
        return;
    int const att_max_skill = int(mob->GetMaxSkillValueForLevel(p));
    int const def_classic = int(p->GetClassicDefenseSkill(mob));
    int const def_cap = int(p->GetMaxSkillValueForLevel(mob));
    int const tmp = att_max_skill - std::min(def_classic, def_cap);
    TC_LOG_INFO("playerbot.v2",
        "[incoming] crush_state when={} attacker={} level={} level_for_target={} scalable_levels={} scaling_min={} scaling_max={} scaling_delta={} content_tuning={} "
        "victim_level={} victim_level_for_attacker={} defense_skill={} defense_pure={} defense_temp_bonus={} defense_perm_bonus={} defense_max={} classic_defense_skill={} "
        "defense_cap_victim_level_x5={} attacker_max_skill={} crush_tmp={} (crush when tmp >= 15; chance 2 x tmp - 15 percent) crush_swings_so_far={} of {}",
        when, mob->GetName(), uint32(mob->GetLevel()), uint32(mob->GetLevelForTarget(p)), mob->HasScalableLevels(),
        int(mob->m_unitData->ScalingLevelMin), int(mob->m_unitData->ScalingLevelMax), int(mob->m_unitData->ScalingLevelDelta), mob->GetContentTuning(),
        uint32(p->GetLevel()), uint32(p->GetLevelForTarget(mob)), int(p->GetSkillValue(SKILL_DEFENSE)), int(p->GetPureSkillValue(SKILL_DEFENSE)),
        int(p->GetSkillTempBonusValue(SKILL_DEFENSE)), int(p->GetSkillPermBonusValue(SKILL_DEFENSE)), int(p->GetMaxSkillValue(SKILL_DEFENSE)), def_classic,
        def_cap, att_max_skill, tmp, m.N(Outcome::Crushing), m.Attempts());
}

bool IncomingStep(Player* p, std::string& failure)
{
    Incoming& in = g.incoming;
    constexpr int kLegCountIn = kIncomingDummyLegs + int(sizeof(kMobs) / sizeof(kMobs[0]));
    if (in.leg >= kLegCountIn)
    {
        // The armor rule: damage taken at tier armor against bare armor falls by the vanilla formula (attacker level 63).
        // A run whose legs did not differ proves nothing: the armor must rise from bare to tier 1 to tier 3 (the gear was swapped), and
        // every leg's attacker must have swung.
        if (in.results.size() >= std::size_t(kIncomingDummyLegs))
        {
            for (int k = 1; k < kIncomingDummyLegs; ++k)
                if (in.results[k].armor <= in.results[k - 1].armor)
                    g.failure += fmt::format("{} armor did not rise from leg {} ({:.0f}) to leg {} ({:.0f}): the gear was not swapped between legs; ", g.cls_name,
                                             in.results[k - 1].label, in.results[k - 1].armor, in.results[k].label, in.results[k].armor);
            auto const& bare = in.results[0];
            for (int k = 1; k < kIncomingDummyLegs; ++k)
            {
                auto const& r = in.results[k];
                if (NormalSwings(bare.melee) < kIncomingMinSwings || NormalSwings(r.melee) < kIncomingMinSwings)
                {
                    g.failure += fmt::format("{} leg {}: too few normal hits ({} bare, {} here) to compare armor; ", g.cls_name, r.label,
                                             NormalSwings(bare.melee), NormalSwings(r.melee));
                    continue;
                }
                double const measured = NormalEquivalent(r.melee) / NormalEquivalent(bare.melee);
                double const expected = (1.0 - VanillaArmorReduction(r.armor, 63)) / (1.0 - VanillaArmorReduction(bare.armor, 63));
                Verdict const v = RatioVerdict(measured, expected, 8.0);
                TC_LOG_INFO("playerbot.v2", "[incoming] armor_ratio class={} leg={} armor {:.0f} vs bare {:.0f}: damage taken ratio {:.3f}, vanilla {:.3f} (armor / (armor + 400 + 85 * 63)) verdict={}",
                            g.cls_name, r.label, r.armor, bare.armor, measured, expected, ToString(v));
                if (v != Verdict::Ok)
                    g.failure += fmt::format("{} {} armor {:.0f}: damage taken is {:.3f} of bare, vanilla {:.3f} ({}); ", g.cls_name, r.label, r.armor, measured, expected, ToString(v));
            }
        }
        failure = g.failure;
        return true;
    }
    bool const dummy_leg = in.leg < kIncomingDummyLegs;
    std::uint32_t const measure_s = dummy_leg ? kIncomingDummyS : kIncomingMobS;
    if (!in.setup)
    {
        std::string error;
        Creature* mob = nullptr;
        if (dummy_leg)
        {
            int const mode = in.leg == 0 ? -1 : in.leg == 1 ? 2 : 4;
            CeaseFire(p);
            ::Playerbot::V2::Fleet::BotPopulationManager::SetGearModeOverride(p->GetGUID().GetCounter(), mode);
            if (in.leg == 0)
                StripGear(p);
            else if (!::Playerbot::Services::Population().RegearNow(p))
                TC_LOG_INFO("playerbot.v2", "[incoming] leg {}: RegearNow changed nothing (mode {}, in combat {})", in.leg, mode, p->IsInCombat());
            TC_LOG_INFO("playerbot.v2", "[incoming] gear leg={} armor={:.0f} {}", in.leg, double(p->GetArmor()), GearSummary(p));
            mob = DummyCreature(p);
            if (!mob || !mob->IsAlive())
                mob = SpawnDummy(p, 3.0f, kDummyArmor, error);
            if (mob)
                ArmMob(mob, 1000.f);
        }
        else
        {
            if (Creature* old = DummyCreature(p))
                old->DespawnOrUnsummon();
            CeaseFire(p);
            ::Playerbot::V2::Fleet::BotPopulationManager::SetGearModeOverride(p->GetGUID().GetCounter(), 4);
            ::Playerbot::Services::Population().RegearNow(p);
            mob = SpawnMob(p, kMobs[in.leg - kIncomingDummyLegs].entry, error);
            if (mob)
                ArmMob(mob, 0.f);
        }
        if (!mob)
        {
            TC_LOG_INFO("playerbot.v2", "[incoming] leg {} skipped: {}", in.leg, error);
            g.failure += fmt::format("{} leg {} could not start: {}; ", g.cls_name, in.leg, error);
            ++in.leg;
            return false;
        }
        p->SetFullHealth();
        p->CombatStop(true);
        in.setup = true;
        in.out_of_reach_s = 0;
        in.repositioned = 0;
        in.t = 0;
        std::lock_guard<std::mutex> lock(g_mu);
        g_subject = p->GetGUID();
        g_dummy = g.dummy;
        g_reverse = true;
        return false;
    }
    Creature* mob = DummyCreature(p);
    ++in.t;
    p->SetFullHealth();
    if (mob && mob->IsAlive())
    {
        if (mob->GetVictim() != p)
        {
            mob->EngageWithTarget(p);   // threat and combat, so the creature's AI keeps the tank as its victim
            mob->Attack(p, true);
            mob->GetMotionMaster()->MoveChase(p);
        }
        // The leg measures damage taken, not pathing: an attacker that stays out of melee range of the tank (paladin legs 2026-10-10: 8.2 y
        // away, in combat, never closing) is put back next to it after kIncomingReachWaitS, with the movement state logged so the cause
        // stays visible. Counted per leg and reported at its end.
        if (mob->IsWithinMeleeRange(p))
            in.out_of_reach_s = 0;
        else if (++in.out_of_reach_s >= kIncomingReachWaitS)
        {
            ++in.repositioned;
            TC_LOG_INFO("playerbot.v2", "[incoming] leg={} t={} {} out of melee range for {} s (dist {:.1f}, movement={} rooted={} stunned={} tank_moving={} tank_speed={:.1f}): placed next to the tank (#{})",
                        in.leg, in.t, mob->GetName(), in.out_of_reach_s, mob->GetExactDist(p), int(mob->GetMotionMaster()->GetCurrentMovementGeneratorType()),
                        mob->HasUnitState(UNIT_STATE_ROOT), mob->HasUnitState(UNIT_STATE_STUNNED), p->isMoving(), p->GetSpeed(MOVE_RUN), in.repositioned);
            mob->NearTeleportTo(p->GetNearPosition(2.0f, 0.f));
            mob->GetMotionMaster()->MoveChase(p);
            in.out_of_reach_s = 0;
        }
        // Why a creature is not swinging, every 15 s of a leg.
        if (in.t % 15 == 0)
            TC_LOG_INFO("playerbot.v2", "[incoming] watch leg={} t={} mob={} victim={} in_combat={} dist={:.1f} melee_range={} can_attack_tank={} tank_alive={} tank_attackable={} cannot_autoattack={} react={} faction={}",
                        in.leg, in.t, mob->GetName(), mob->GetVictim() ? mob->GetVictim()->GetName() : std::string("none"), mob->IsInCombat(), mob->GetExactDist(p),
                        mob->IsWithinMeleeRange(p), mob->IsValidAttackTarget(p), p->IsAlive(), !p->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE), mob->HasUnitState(UNIT_STATE_CANNOT_AUTOATTACK),
                        int(mob->GetReactState()), mob->GetFaction());
    }
    if (in.t == kIncomingSettleS)
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_aggs.clear();
        g_measuring = true;
    }
    if (in.t == kIncomingSettleS)
    {
        LogCrushState(p, mob, "leg_start", SpellAgg{});
        in.crush_logged = false;
    }
    if (g_measuring.load() && !in.crush_logged)
    {
        SpellAgg first;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            if (auto const f = g_aggs.find(0); f != g_aggs.end())
                first = f->second;
        }
        if (first.N(Outcome::Crushing) > 0)
        {
            in.crush_logged = true;
            LogCrushState(p, mob, "first_crush", first);
        }
    }
    if (in.t < kIncomingSettleS + measure_s)
        return false;

    g_measuring = false;
    Incoming::Result r;
    r.label = dummy_leg ? (in.leg == 0 ? "bare" : in.leg == 1 ? "t1" : "t3") : kMobs[in.leg - kIncomingDummyLegs].label;
    r.armor = double(p->GetArmor());
    {
        std::lock_guard<std::mutex> lock(g_mu);
        r.all = g_aggs;
    }
    if (auto const f = r.all.find(0); f != r.all.end())
        r.melee = f->second;
    SpellAgg const& m = r.melee;
    LogCrushState(p, mob, "leg_end", m);
    std::uint32_t const attempts = m.Attempts();
    TC_LOG_INFO("playerbot.v2", "[incoming] leg={} target={} tank_class={} armor={:.0f} secs={} swings={} hit={} crit={} crush={} glance={} block={} dodge={} parry={} miss={} avoid_pct={:.1f} avg_normal={:.0f} min={} max={} (weapon damage range {:.0f}-{:.0f}, attack time {} ms)",
                r.label, mob ? mob->GetName() : std::string("?"), g.cls_name, r.armor, measure_s, attempts, m.N(Outcome::Hit), m.N(Outcome::Crit), m.N(Outcome::Crushing),
                m.N(Outcome::Glancing), m.N(Outcome::Block), m.N(Outcome::Dodge), m.N(Outcome::Parry), m.N(Outcome::Miss), m.AvoidPct(), m.AvgNormal(), m.min_hit, m.max_hit,
                mob ? mob->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE) : 0.f, mob ? mob->GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE) : 0.f,
                mob ? mob->GetBaseAttackTime(BASE_ATTACK) : 0u);
    {
        // The skills the attack table reads: the tank's Defense (value/max) against the attacker's weapon skill (5 per level in vanilla).
        int const def_val = int(p->GetSkillValue(SKILL_DEFENSE)), def_max = int(p->GetMaxSkillValue(SKILL_DEFENSE));
        int const mob_level = mob ? int(mob->GetLevel()) : 0;
        int const weapon_skill = mob ? int(mob->GetMaxSkillValueForLevel(p)) : 0;
        TC_LOG_INFO("playerbot.v2", "[incoming] skills leg={} tank_level={} defense={}/{} defense_rating_bonus={:.1f}% attacker_level={} attacker_weapon_skill={} skill_gap={} (weapon skill - defense)",
                    r.label, uint32(p->GetLevel()), def_val, def_max, double(p->GetRatingBonusValue(CR_DEFENSE_SKILL)), mob_level, weapon_skill, weapon_skill - def_val);
        // The damage range the creature is set to against what landed: the observed mean (crushing blows brought back to a normal hit)
        // has to sit where the effective range says, less the tank's armor.
        float eff_lo = 0.f, eff_hi = 0.f;
        if (mob)
            mob->CalculateMinMaxDamage(BASE_ATTACK, false, true, eff_lo, eff_hi);
        double const reduction = VanillaArmorReduction(r.armor, std::max(1, mob_level));
        double const expected_mid = 0.5 * double(eff_lo + eff_hi) * (1.0 - reduction);
        double const seen = NormalEquivalent(m);
        // The tolerance follows the sample: 20 %, or three standard errors of the mean of a uniform range (the spread of the set range after
        // armor over the landed swings), or one point of damage (hits are whole numbers), whichever is widest. A mob that hits for 8 has a
        // mean that moves by more than 20 % between runs of a few dozen swings.
        double const swings = double(std::max<std::uint32_t>(1u, NormalSwings(m)));
        double const sd = double(eff_hi - eff_lo) * (1.0 - reduction) / std::sqrt(12.0);
        double const slack = std::max(3.0 * sd / std::sqrt(swings), 1.0);
        double const tol_pct = expected_mid > 0.0 ? std::max(20.0, 100.0 * slack / expected_mid) : 20.0;
        Verdict const dv = NormalSwings(m) >= kIncomingMinSwings ? CompareValue(seen, expected_mid, tol_pct) : Verdict::NoData;
        TC_LOG_INFO("playerbot.v2", "[incoming] damage_range leg={} effective_range={:.0f}-{:.0f} armor_reduction={:.3f} expected_mean={:.1f} observed_mean={:.1f} (normal-equivalent over {} swings) tolerance={:.0f}% min_seen={} max_seen={} verdict={}",
                    r.label, eff_lo, eff_hi, reduction, expected_mid, seen, NormalSwings(m), tol_pct, m.min_hit, m.max_hit, ToString(dv));
        // What the server's armor step reads at the end of the leg, against what the verdict assumed (armor at the start of the leg, the creature's own
        // level): a tank near the 75% cap moves the expected mean a lot with a few hundred armor, and CalcArmorReducedDamage takes the level from
        // GetLevelForTarget and the armor from GetArmor x the attacker-specific multiplier.
        if (mob)
        {
            float const live_armor = float(p->GetArmor()) * p->GetArmorMultiplierForTarget(mob);
            unsigned const live_level = unsigned(mob->GetLevelForTarget(p));
            TC_LOG_INFO("playerbot.v2", "[incoming] armor_check leg={} armor_at_leg_start={:.0f} armor_now={} armor_x_multiplier_for_attacker={:.0f} mob_level={} level_for_target={} mitigation_assumed={:.3f} mitigation_server_now={:.3f} hits={} crush={} crit={}",
                        r.label, r.armor, p->GetArmor(), live_armor, mob_level, live_level, reduction, ClassicArmor::Mitigation(live_armor, live_level),
                        m.N(Outcome::Hit), m.N(Outcome::Crushing), m.N(Outcome::Crit));
        }
        if (attempts == 0)
            g.failure += fmt::format("{} leg {}: the attacker never swung (hostile {}, victim {}); ", g.cls_name, r.label,
                                     mob ? (mob->IsHostileTo(p) ? "yes" : "no") : "no mob", mob && mob->GetVictim() ? mob->GetVictim()->GetName() : std::string("none"));
        else if (dv == Verdict::TooLow || dv == Verdict::TooHigh)
            g.failure += fmt::format("{} leg {}: observed mean damage {:.0f} against {:.0f} expected from the set range ({}); ", g.cls_name, r.label, seen, expected_mid, ToString(dv));
        else if (dv == Verdict::NoData && NormalSwings(m) < kIncomingMinSwings)
            g.failure += fmt::format("{} leg {}: only {} landed swings in {} s; ", g.cls_name, r.label, NormalSwings(m), measure_s);
    }
    for (auto const& [key, a] : r.all)
        if (key)
            TC_LOG_INFO("playerbot.v2", "{}", FormatSpellLine(r.label.c_str(), g.cls_name, SpellNameOf(key), a, double(measure_s)));
    if (!dummy_leg)
    {
        if (mob)
            mob->DespawnOrUnsummon();
        std::lock_guard<std::mutex> lock(g_mu);
        g_dummy = ObjectGuid::Empty;
    }
    in.results.push_back(std::move(r));
    ++in.leg;
    in.setup = false;
    return false;
}

} // namespace

// ---- Hooks ----

bool Active() { return g_measuring.load(std::memory_order_relaxed); }

void NoteEnergize(Unit* target, std::uint32_t spell_id, std::int32_t gained, std::uint8_t power)
{
    if (!g_energy_on.load(std::memory_order_relaxed) || power != POWER_MANA || !target)
        return;
    std::lock_guard<std::mutex> lock(g_mu);
    if (target->GetGUID() == g_subject)
        g_energy[spell_id] += gained;
}

void NoteOutcome(Unit* attacker, Unit* victim, std::uint32_t spell_id, std::uint8_t outcome, std::uint32_t damage)
{
    if (!g_measuring.load(std::memory_order_relaxed))
        return;
    std::lock_guard<std::mutex> lock(g_mu);
    bool pet = false;
    if (!IsOurs(attacker, victim, pet))
        return;
    Outcome const o = Outcome(std::min<std::uint8_t>(outcome, std::uint8_t(kOutcomeCount - 1)));
    std::uint32_t const key = spell_id | (pet ? kPetFlag : 0u);
    if (!DealsDamage(o))
    {
        g_aggs[key].spell_id = spell_id;
        g_aggs[key].AddOutcome(o, 0);
        t_pending.valid = false;
        return;
    }
    t_pending.valid = true;
    t_pending.spell = spell_id;
    t_pending.victim = victim->GetGUID();
    t_pending.outcome = o;
    t_pending.pet = pet;
    t_pending.watched = !g_watch_aura.empty() && HasAuraNamed(victim, g_watch_aura, g_subject);
    (void)damage;
}

void NoteDamage(Unit* attacker, Unit* victim, std::int32_t amount, std::uint32_t spell_id)
{
    if (!g_measuring.load(std::memory_order_relaxed))
        return;
    if (amount <= 0)
    {
        t_pending.valid = false;   // a fully absorbed hit: its outcome must not be taken by the next damage event
        return;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    bool pet = false;
    if (!IsOurs(attacker, victim, pet))
        return;
    std::uint32_t const key = spell_id | (pet ? kPetFlag : 0u);
    bool const direct = t_pending.valid && t_pending.spell == spell_id && t_pending.victim == victim->GetGUID();
    SpellAgg& agg = direct && t_pending.watched ? g_aggs_watched[key] : g_aggs[key];
    agg.spell_id = spell_id;
    if (direct)
        agg.AddOutcome(t_pending.outcome, std::uint32_t(amount));
    else
        agg.AddTick(std::uint32_t(amount));
    t_pending.valid = false;
}

// The trap audit: the hunter sets each trap out of combat (the trap object needs no combat to arm), the dummy is moved onto the object, and the
// object must fire and do what the trap does. The core's own [trap_cast] line (GameObject.cpp) gives the cast result and the spell's effect
// targets; this adds the verdict: damage traps must hurt the dummy, Frost and Freezing Trap must leave an aura on it.
struct TrapSpec { char const* spell; bool damages; };
constexpr TrapSpec kTraps[] = { { "Explosive Trap", true }, { "Immolation Trap", true }, { "Frost Trap", false }, { "Freezing Trap", false } };
constexpr std::size_t kTrapCount = sizeof(kTraps) / sizeof(kTraps[0]);

bool TrapStep(Player* p, Creature* d, std::string& failure)
{
    TrapAudit& t = g.trap;
    if (t.item >= kTrapCount)
    {
        failure = g.failure;
        return true;
    }
    TrapSpec const& spec = kTraps[t.item];
    ++t.t;
    switch (t.phase)
    {
        case 0:
        {
            t.spell_id = FindKnownSpell(p, spec.spell);
            if (!t.spell_id)
            {
                g.failure += fmt::format("{} does not know {}; ", g.cls_name, spec.spell);
                TC_LOG_INFO("playerbot.v2", "[trap_audit] {} is not in the spellbook", spec.spell);
                ++t.item;
                t.t = 0;
                return false;
            }
            p->CombatStop(true);
            p->GetSpellHistory()->ResetAllCooldowns();
            TopUp(p);
            d->RemoveAllAuras();
            d->SetFullHealth();
            // The dummy stays away from the spot the trap goes down on until the object exists: a dummy left at the last trap's place would set
            // the new one off at once. Recheck each second until it really is away.
            if (p->GetExactDist(d) < 10.f)
            {
                d->NearTeleportTo(p->GetNearPosition(15.f, 0.f));
                if (t.t < 6)
                    return false;
            }
            ::Playerbot::API api(p);
            ::Playerbot::Result const r = api.cast_spell(t.spell_id, p->GetGUID(), true);
            if (r != ::Playerbot::Result::Ok)
            {
                g.failure += fmt::format("{} cast of {} ({}) refused: {}; ", g.cls_name, spec.spell, t.spell_id, CastFailText(api.last_cast_fail()));
                TC_LOG_INFO("playerbot.v2", "[trap_audit] {} ({}) cast refused: {}", spec.spell, t.spell_id, CastFailText(api.last_cast_fail()));
                ++t.item;
                t.t = 0;
                return false;
            }
            t.phase = 1;
            t.t = 0;
            return false;
        }
        case 1:
        {
            GameObject* go = p->GetGameObject(t.spell_id);
            if (!go)
                go = p->FindNearestGameObjectOfType(GAMEOBJECT_TYPE_TRAP, 10.f);
            if (!go)
            {
                // The object is gone: it fired by itself before the dummy was moved onto it (the effect is judged below on what the dummy shows).
                // A cast that made no object at all fails the same judgement, as nothing happens to the dummy.
                if (t.t >= 3)
                {
                    TC_LOG_INFO("playerbot.v2", "[trap_audit] {} ({}) no trap object left after {} s: judged on the dummy", spec.spell, t.spell_id, t.t);
                    t.fired = true;
                    t.phase = 3;
                    t.t = 0;
                }
                return false;
            }
            t.trap = go->GetGUID();
            t.trap_entry = go->GetEntry();
            t.effect_spell = go->GetGOInfo()->trap.spell;
            t.health_before = d->GetHealth();
            d->NearTeleportTo(go->GetPosition());
            TC_LOG_INFO("playerbot.v2", "[trap_audit] {} ({}) object {} placed, dummy moved onto it", spec.spell, t.spell_id, t.trap_entry);
            t.phase = 2;
            t.t = 0;
            return false;
        }
        case 2:
        {
            GameObject* go = ObjectAccessor::GetGameObject(*p, t.trap);
            // Fired = the object is gone (a single charge trap despawns after it triggers).
            if (!go || !go->IsInWorld() || go->getLootState() == GO_JUST_DEACTIVATED)
            {
                t.fired = true;
                t.phase = 3;
                t.t = 0;
                return false;
            }
            if (t.t == 3)
                d->NearTeleportTo(go->GetPosition());   // a dummy that was pushed off is put back
            if (t.t >= 12)
            {
                t.fired = false;
                t.phase = 3;
                t.t = 0;
            }
            return false;
        }
        case 4:
        {
            if (t.t < 6)
                return false;
            std::uint32_t const lost = d->GetMaxHealth() > d->GetHealth() ? d->GetMaxHealth() - d->GetHealth() : 0u;
            t.control_lost = lost;
            t.control_auras = d->GetAppliedAuras().size();
            TC_LOG_INFO("playerbot.v2", "[trap_audit] control cast of {} straight at the dummy: health lost {} auras {}", t.effect_spell, lost, d->GetAppliedAuras().size());
            t.phase = 3;
            t.t = 6;   // fall through to the verdict below on the next step
            return false;
        }
        default:
        {
            if (t.t < 6)   // a periodic trap (Immolation) needs its ticks
                return false;
            std::uint32_t const lost = d->GetMaxHealth() > d->GetHealth() ? d->GetMaxHealth() - d->GetHealth() : 0u;
            std::size_t const auras = d->GetAppliedAuras().size();
            bool ok = t.fired && !t.control_ran && (spec.damages ? lost > 0 : auras > 0);   // a control run means the trap itself failed all attempts
            TC_LOG_INFO("playerbot.v2", "[trap_audit] {} ({}) object {} effect spell {} attempt {} fired={} dummy_health_lost={} dummy_auras={} verdict={}", spec.spell, t.spell_id,
                        t.trap_entry, t.effect_spell, t.attempt, t.fired, lost, auras, ok ? "ok" : "BROKEN");
            if (!ok && t.attempt < 3)
            {
                ++t.attempt;   // try again: one miss against a level 63 target proves nothing
                t.phase = 0;
                t.t = 0;
                return false;
            }
            if (!ok && t.fired && t.effect_spell && !t.control_ran)
            {
                // Control: the same effect spell cast by the hunter straight at the dummy, no trap object. If this lands, the trap's own path (a
                // game object as the caster) is the fault; if it does not, the spell data or the dummy (an immunity) is.
                if (SpellInfo const* es = sSpellMgr->GetSpellInfo(t.effect_spell, DIFFICULTY_NONE))
                {
                    std::string imm;
                    for (SpellEffectInfo const& e : es->GetEffects())
                        if (e.IsEffect())
                            imm += fmt::format(" eff{}:immune={}", uint32(e.EffectIndex), d->IsImmunedToSpellEffect(es, e, p));
                    int missed = 0;
                    for (int i = 0; i < 40; ++i)
                        if (p->SpellHitResult(d, es, false, true) != SPELL_MISS_NONE)
                            ++missed;
                    TC_LOG_INFO("playerbot.v2", "[trap_audit] control {} ({}) dummy entry {} school {} spell_immune={} missed {} of 40 rolls{}", spec.spell, t.effect_spell,
                                d->GetEntry(), uint32(es->GetSchoolMask()), d->IsImmunedToSpell(es, MAX_EFFECT_MASK, p), missed, imm);
                    d->RemoveAllAuras();
                    d->SetFullHealth();
                    p->CastSpell(d, t.effect_spell, true);
                    t.control_ran = true;
                    t.phase = 4;
                    t.t = 0;
                    return false;
                }
            }
            AppendFile(fmt::format("| trap audit | {} ({}) | object {} | fired {} | health lost {} | auras {} | {} |", spec.spell, t.spell_id, t.trap_entry, t.fired, lost, auras,
                                   ok ? "ok" : "broken"));
            if (!ok)
                g.failure += fmt::format("{} ({}): {} after {} attempts{}; ", spec.spell, t.spell_id,
                                         !t.fired ? "the trap never fired" : spec.damages ? "it fired and the dummy lost no health" : "it fired and left no aura on the dummy", t.attempt,
                                         t.control_ran ? fmt::format(" (the effect spell {} cast straight at the dummy: health lost {}, auras {})", t.effect_spell, t.control_lost, t.control_auras) : std::string());
            ++t.item;
            t.phase = 0;
            t.t = 0;
            t.attempt = 1;
            t.control_ran = false;
            t.control_lost = 0;
            t.control_auras = 0;
            return false;
        }
    }
}

// ---- The runner's calls ----

bool Begin(Player* subject, Kind kind, std::uint32_t leg_seconds, std::string& error)
{
    End();
    g = State{};
    g.active = true;
    g.kind = kind;
    g.subject = subject->GetGUID();
    g.cls = subject->GetClass();
    g.cls_name = ClassName(g.cls);
    g.leg_seconds = leg_seconds ? leg_seconds : 180;
    g.items = SweepItemsFor(kind, g.cls);
    if (kind == Kind::Finisher && g.items.empty())
    {
        error = fmt::format("{} has no combo, rage or energy scaled spell in the sweep", g.cls_name);
        g.active = false;
        return false;
    }
    // The dummy: close for a melee sweep, further for a caster.
    float const distance = kind == Kind::Finisher || kind == Kind::Incoming ? 3.0f : kind == Kind::TalentAudit || kind == Kind::Economy ? 18.0f : 15.0f;
    Creature* d = SpawnDummy(subject, distance, kDummyArmor, error);
    if (!d)
    {
        g.active = false;
        return false;
    }
    if (kind != Kind::Damage)
        DetachAi(subject);
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_subject = subject->GetGUID();
        g_dummy = g.dummy;
        g_watch_aura.clear();
    }
    if (kind == Kind::Damage)
    {
        g.step = Step::Prepare;
        AppendFile(fmt::format("\n## {} ({})\n\n{}", g.cls_name, "damage_parity", HeaderRow()));
    }
    else if (kind == Kind::Trap)
    {
        g.step = Step::Trap;
        AppendFile(fmt::format("\n## {} (trap audit)\n", g.cls_name));
    }
    else if (kind == Kind::Incoming)
    {
        g.step = Step::Incoming;   // the legs set their own gear
        AppendFile(fmt::format("\n## {} (incoming damage)\n", g.cls_name));
    }
    else
    {
        // Tier 3 gear for the sweeps: the spells scale with the character a raid fields.
        ::Playerbot::V2::Fleet::BotPopulationManager::SetGearModeOverride(subject->GetGUID().GetCounter(), 4);
        ::Playerbot::Services::Population().RegearNow(subject);
        TopUp(subject);
        g.step = kind == Kind::Economy ? Step::Econ : Step::SweepSetup;
        if (kind == Kind::TalentAudit)
            LogTalents(subject);
        if (kind == Kind::Economy)
            EconStatic(subject);
    }
    g.step_s = 0;
    return true;
}

bool Step(std::string& failure)
{
    if (!g.active)
        return true;
    Player* p = Subject();
    if (!p || !p->IsInWorld() || !p->IsAlive())
    {
        failure = "the bot is gone or dead";
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] subject lost: class={} kind={} found={} in_world={} alive={} step={} leg={} step_s={} map={} pos=({:.1f},{:.1f},{:.1f}) in_combat={} last_attacker={}",
                    g.cls_name, int(g.kind), p != nullptr, p && p->IsInWorld(), p && p->IsAlive(), int(g.step), g.leg, g.step_s, p ? p->GetMapId() : 0u,
                    p ? p->GetPositionX() : 0.f, p ? p->GetPositionY() : 0.f, p ? p->GetPositionZ() : 0.f, p && p->IsInCombat(),
                    p && p->getAttackers().size() ? (*p->getAttackers().begin())->GetName() : std::string("none"));
        End();
        return true;
    }
    Creature* d = DummyCreature(p);
    if (g.kind == Kind::Incoming)
        return IncomingStep(p, failure) ? (End(), true) : false;   // its legs spawn and remove their own targets
    if (!d || !d->IsAlive())
    {
        std::string error;
        d = SpawnDummy(p, g.kind == Kind::Finisher ? 3.0f : g.kind == Kind::TalentAudit || g.kind == Kind::Economy ? 18.0f : 15.0f,
                       g.kind == Kind::Damage ? kLegs[std::min(g.leg, kLegCount - 1)].armor : kDummyArmor, error);
        if (!d)
        {
            failure = error;
            End();
            return true;
        }
        std::lock_guard<std::mutex> lock(g_mu);
        g_dummy = g.dummy;
    }
    ++g.step_s;

    switch (g.step)
    {
        case Step::Prepare:
            BeginLegSetup(p);
            return false;
        case Step::Settle:
            if (g.step_s >= 4)
            {
                g.step = Step::Warm;
                g.step_s = 0;
            }
            return false;
        case Step::Warm:
            Engage(p, d);
            if (g.step_s >= 5)
                StartMeasuring(p);
            return false;
        case Step::Measure:
        {
            if (p->GetVictim() != d && !p->HasUnitState(UNIT_STATE_CASTING))
                Engage(p, d);
            if (p->HasUnitState(UNIT_STATE_CASTING))
                ++g.casting_s;
            else if (p->GetVictim() == d)
                ++g.attacking_s;
            else
                ++g.idle_s;
            if (d->GetHealthPct() < 60.0)
                d->SetFullHealth();
            Powers const pt = p->GetPowerType();
            if (pt == POWER_MANA && p->GetMaxPower(pt) && p->GetPower(pt) * 10u < p->GetMaxPower(pt) * 3u)
            {
                p->SetPower(pt, p->GetMaxPower(pt));
                ++g.mana_refills;
            }
            if (g.step_s == 20)
            {
                bool silent;
                {
                    std::lock_guard<std::mutex> lock(g_mu);
                    silent = g_aggs.empty();
                }
                if (silent)
                    TC_LOG_INFO("playerbot.v2", "[dmg_parity] {} leg {}: 20 s in and nothing dealt to the dummy yet (target {}, in combat {}, casting {})", g.cls_name,
                                kLegs[g.leg].label, p->GetVictim() ? p->GetVictim()->GetName() : std::string("none"), p->IsInCombat(), p->HasUnitState(UNIT_STATE_CASTING));
            }
            if (g.step_s >= g.leg_seconds)
            {
                ReportLeg(p);
                p->CombatStop(true);
                if (++g.leg >= kLegCount)
                {
                    CrossLegChecks(p);
                    failure = g.failure;
                    End();
                    return true;
                }
                g.step = Step::Prepare;
                g.step_s = 0;
            }
            return false;
        }
        case Step::SweepSetup:
        {
            if (g.item >= g.items.size())
            {
                failure = g.failure;
                End();
                return true;
            }
            if (!SweepSetup(p))
            {
                ++g.item;
                g.point = 0;
                return false;
            }
            if (g.items[g.item].watch_aura)
            {
                std::lock_guard<std::mutex> lock(g_mu);
                g_watch_aura = g.items[g.item].watch_aura;
                g_aggs.clear();
                g_aggs_watched.clear();
            }
            g_measuring = true;
            g.series.clear();
            g.point = 0;
            g.step = Step::SweepCast;
            g.step_s = 0;
            return false;
        }
        case Step::SweepCast:
        {
            if (p->HasUnitState(UNIT_STATE_CASTING) || p->GetSpellHistory()->HasGlobalCooldown(sSpellMgr->GetSpellInfo(g.cast_spell_id, DIFFICULTY_NONE)))
                return false;
            if (!SweepCastOnce(p, d))
            {
                if (g.cast_failures >= 8 && !g.cast_ok)
                {
                    g.failure += fmt::format("{} {}: cast refused 8 times ({}); ", g.cls_name, g.items[g.item].spell, g.last_fail);
                    TC_LOG_INFO("playerbot.v2", "[dmg_parity] sweep class={} spell=\"{}\" cast refused: {}", g.cls_name, g.items[g.item].spell, g.last_fail);
                    g_measuring = false;
                    ++g.item;
                    g.point = 0;
                    g.step = Step::SweepSetup;
                }
                return false;
            }
            g.step = Step::SweepWait;
            g.step_s = 0;
            return false;
        }
        case Step::SweepWait:
        {
            SweepItem const& it = g.items[g.item];
            if (p->HasUnitState(UNIT_STATE_CASTING))
                return false;
            // A damage over time runs its course (24 s at most); a direct hit or an aura is read a moment after the cast.
            bool const dot = it.measure == Measure::DotTotal;
            bool const gone = !d->HasAura(g.cast_spell_id);
            if (dot ? (!gone ? g.step_s < 26 : g.step_s < 2) : g.step_s < 2)
                return false;
            SweepSample(p, d);
            ++g.rep;
            if (g.rep < it.reps)
            {
                g.step = Step::SweepCast;
                g.step_s = 0;
                return false;
            }
            if (!it.watch_aura)
            {
                g.series.push_back(Mean(g.samples));
                g.samples.clear();
                g.rep = 0;
                if (++g.point < it.points.size())
                {
                    g.step = Step::SweepCast;
                    g.step_s = 0;
                    return false;
                }
            }
            g_measuring = false;
            SweepItemDone();
            {
                std::lock_guard<std::mutex> lock(g_mu);
                g_watch_aura.clear();
            }
            ++g.item;
            g.point = 0;
            g.step = Step::SweepSetup;
            g.step_s = 0;
            return false;
        }
        case Step::Econ:
            if (EconStep(p, d, failure))
            {
                End();
                return true;
            }
            return false;
        case Step::Trap:
            if (TrapStep(p, d, failure))
            {
                End();
                return true;
            }
            return false;
        case Step::Incoming:   // handled above
        case Step::Finished:
            return true;
    }
    return true;
}

void End()
{
    g_measuring = false;
    g_energy_on = false;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_subject = ObjectGuid::Empty;
        g_dummy = ObjectGuid::Empty;
        g_watch_aura.clear();
        g_reverse = false;
    }
    if (!g.active)
        return;
    if (Player* p = Subject())
    {
        ApplyBonuses(p, 0, 0);
        ::Playerbot::V2::Fleet::BotPopulationManager::SetGearModeOverride(p->GetGUID().GetCounter(), -1);
        if (Creature* d = DummyCreature(p))
            d->DespawnOrUnsummon();
    }
    g.active = false;
    g.step = Step::Finished;
}

std::string Status()
{
    if (!g.active)
        return {};
    if (g.kind == Kind::Damage)
        return fmt::format("damage_parity {}: leg {} of {} ({}), {} s", g.cls_name, std::min(g.leg + 1, kLegCount), kLegCount, kLegs[std::min(g.leg, kLegCount - 1)].label, g.step_s);
    if (g.kind == Kind::Economy)
        return fmt::format("economy_audit {}: phase {} of 4, {} s", g.cls_name, g.econ.phase, g.econ.t);
    if (g.kind == Kind::Trap)
        return fmt::format("trap_audit {}: trap {} of {}, phase {}, {} s", g.cls_name, std::min(g.trap.item + 1, kTrapCount), kTrapCount, g.trap.phase, g.trap.t);
    if (g.kind == Kind::Incoming)
        return fmt::format("incoming_damage {}: leg {} of 6, {} s", g.cls_name, g.incoming.leg + 1, g.incoming.t);
    return fmt::format("{} {}: item {} of {}", g.kind == Kind::Finisher ? "finisher_sweep" : "talent_audit", g.cls_name, std::min(g.item + 1, g.items.size()), g.items.size());
}

} // namespace Playerbot::Diagnostics::Parity
