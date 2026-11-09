#include "DamageParity.h"

#include "../Api/ClassicSpec.h"
#include "../Api/ClassicTalentBuilds.h"
#include "../Api/ClassicSupplies.h"
#include "../Bot/BotSnapshotView.h"
#include "../Bot/ClassicUpkeep.h"
#include "../Bot/ThreatLimits.h"
#include "../Group/GroupSnapshot.h"
#include "../Bot/BotAI.h"
#include "../Bot/BotRegistry.h"
#include "../Bot/Gear/BotGearGenerator.h"
#include "../Fleet/BotPopulationManager.h"
#include "../Services.h"
#include "../Threading/SnapshotPublisher.h"
#include "../Threading/TickScheduler.h"

#include "ClassicArmor.h"
#include "SpellCheckExpected.h"
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
#include "MoveSpline.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerbotAPI.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Unit.h"
#include "World.h"
#include <string_view>
#include "fmt/format.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <ctime>
#include <fstream>
#include <array>
#include <map>
#include <mutex>
#include <set>
#include <span>

extern TC_GAME_API std::atomic<std::uint64_t> g_classicRageHitDamage;   // Unit.cpp: melee hit damage that rewarded rage
extern TC_GAME_API std::atomic<std::uint64_t> g_classicRageHitGained;   // Unit.cpp: the rage it gave, in tenths
#include <string>
#include <vector>

namespace Playerbot::Diagnostics::Parity {

bool FindOpenSpot(Player* p, float distance, float& x, float& y, float& z, float& o);   // defined below, used by the leg setup

namespace {
struct FlatSpot { bool set = false; std::uint32_t map = 0; float x = 0, y = 0, z = 0, o = 0; } g_flat_spot;
}
void NoteFlatSpot(std::uint32_t map, float x, float y, float z, float o) { g_flat_spot = { true, map, x, y, z, o }; }

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
std::map<std::uint32_t, std::uint32_t> g_casts;      // spell id -> casts the core accepted from the subject in the leg, under g_mu
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
    int group_idx = -1;   // the raider the pending outcome belongs to (Kind::Group)
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

// ---- Group scorecard state (Kind::Group) ----
// One tally per raider, picked by the damage hooks from the attacker's owner. All under g_mu.
std::atomic<bool> g_group_mode{false};
std::vector<std::map<std::uint32_t, SpellAgg>> g_group_aggs;   // index = the raider's place in the scorecard
std::vector<std::map<std::uint32_t, std::uint32_t>> g_group_casts;   // the same, casts the core accepted
std::vector<std::vector<std::pair<std::uint32_t, std::uint32_t>>> g_group_timeline;   // per bot: (game time ms, spell) of every accepted cast, in order
std::vector<std::vector<std::int32_t>> g_group_real_ms;   // per bot, per timeline entry: the cast or channel time the core gave the spell (talents and auras applied)
std::map<ObjectGuid, std::size_t> g_group_index;

// The support scorecard (healers and tanks): what each healer's heals were worth and restored, and what the boss did to each raider. All under g_mu.
std::atomic<bool> g_support_mode{false};
struct HealAgg { std::uint64_t raw = 0, effective = 0; std::uint32_t heals = 0; };
std::vector<std::map<std::uint32_t, HealAgg>> g_group_heals;   // per healer: spell -> heals that landed (a hot tick is one heal)
std::vector<std::map<std::uint32_t, SpellAgg>> g_group_taken;   // per raider: the boss's swings at it (spell 0)
std::vector<std::uint64_t> g_group_pulse;                        // per raider: damage of the boss's scripted raid pulses

// What the damage belongs to: -2 not ours, -1 the single subject's tally, >= 0 that raider's tally. g_mu held.
int Resolve(Unit* attacker, Unit* victim, bool& pet)
{
    if (g_group_mode.load(std::memory_order_relaxed))
    {
        if (!attacker || !victim || victim->GetGUID() != g_dummy)
            return -2;
        Unit* owner = attacker->GetCharmerOrOwnerOrSelf();
        if (!owner)
            return -2;
        auto const it = g_group_index.find(owner->GetGUID());
        if (it == g_group_index.end())
            return -2;
        pet = owner != attacker;
        return int(it->second);
    }
    return IsOurs(attacker, victim, pet) ? -1 : -2;
}

// The raider the boss (the dummy) hit: its place in the scorecard, -1 when the attacker is not the boss or the victim not a raider. g_mu held.
int TakenIndex(Unit* attacker, Unit* victim)
{
    if (!g_support_mode.load(std::memory_order_relaxed) || !attacker || !victim || attacker->GetGUID() != g_dummy)
        return -1;
    auto const it = g_group_index.find(victim->GetGUID());
    return it == g_group_index.end() ? -1 : int(it->second);
}

std::map<std::uint32_t, SpellAgg>& AggsFor(int idx) { return idx >= 0 ? g_group_aggs[std::size_t(idx)] : g_aggs; }

// ---- The run ----

struct Leg
{
    char const* label;
    int gear;          // 0 nothing worn, 1 tier 1, 3 tier 3
    int armor;         // the dummy's armor
    int sp;            // spell power added on top of the gear
    int ap;            // attack power added (melee and ranged)
    int sunders = 0;       // Sunder Armor stacks the dummy carries, cast as the real aura (SPELL_SUNDER_ARMOR)
    bool buffed = false;   // the raid buffs of kRaidBuffSpells are on the bot (their values come from the client spell data)
};

// The buffs a raid stood in when the 800-1100 warrior / Naxxramas reference was logged, one list in one place. Highest rank of each at level 60; the
// amounts are the client's own spell data (nothing is hard-coded), so a Forever re-tune or a sniffed value flows into the buffed leg. Not on the list:
// Windfury Totem (needs a totem and a second weapon hit), Improved Blessing of Might and the other talents of the buffers, consumables and world buffs.
// The list is the author's reading of what a vanilla raid brought; correct it from a raid log when one is at hand.
constexpr std::uint32_t kRaidBuffSpells[] = {
    25289,   // Battle Shout (rank 7)
    25291,   // Blessing of Might (rank 7)
    20906,   // Trueshot Aura (rank 3)
    25362,   // Strength of Earth Totem (rank 5 aura)
    25360,   // Grace of Air Totem (rank 3 aura)
    21850,   // Gift of the Wild (rank 2)
    24932,   // Leader of the Pack (crit)
};

// The same for a caster. What a vanilla raid caster stood in: Arcane Intellect (top rank) and the Flask of Supreme Power on the bot, Curse of the
// Elements on the boss, and, from the bot's own talents, the debuffs a mage puts on it (Winter's Chill, Improved Scorch) or a shadow priest does
// (Shadow Weaving, which every warlock raid had). Highest rank at level 60, amounts from the client's spell data; a flask replaces the two elixirs,
// so there is no elixir on the list. Not on it: Wizard Oil (an enchant, not an aura), Mark of the Wild beyond the Gift already in the list, world buffs.
constexpr std::uint32_t kSupremePowerSpell = 17628;   // Supreme Power (Flask of Supreme Power): +150 spell damage
constexpr std::uint32_t kCasterBuffSpells[] = {
    10157,   // Arcane Intellect (rank 4)
    kSupremePowerSpell,
};
constexpr std::uint32_t kCasterDebuffSpells[] = {
    402792,  // Curse of the Elements: -75 resistance, +10% Fire and Frost damage taken (the client's 1.60 row)
    402791,  // Curse of Shadow: -75 shadow resistance, +10% Shadow and Arcane damage taken (168: the buffed warlock had only Elements, which does nothing for Shadow Bolt)
};
// What a physical raid added beyond the list above (the two measured 147 buffed legs lacked these): Blessing of Kings on everyone, the
// consumables of the role's kit (Api/ClassicSupplies.cpp kMeleeKit / kCasterKit hold the items; these are the auras they leave) and the
// armor shred a melee raid kept on its boss besides Sunder Armor. Windfury Totem is looked up by name at run time (kWindfuryTotem): its
// aura is a weapon proc and the id is not the same in every data set, so a missing one is logged and the leg runs without it.
constexpr std::uint32_t kKingsSpell = 20217;   // Blessing of Kings (a row of the Forever spell_threat data)
// The consumables by the name of the aura they leave (an elixir's buff carries the elixir's name); looked up in the spell data, so no id is
// taken from memory. Dumplings and other foods are left out: their buff is a shared "Well Fed".
constexpr char const* kMeleeConsumableAuras[] = {
    "Elixir of the Mongoose",
    "Elixir of the Giants",
    "Juju Power",
};
constexpr char const* kCasterConsumableAuras[] = {
    "Greater Arcane Elixir",
};
// Ids confirmed against the Forever spell_ranks data (Faerie Fire 770 chain rank 4, Curse of Recklessness 704 chain rank 4).
constexpr std::uint32_t kArmorShredSpells[] = {
    9907,    // Faerie Fire (rank 4)
    11717,   // Curse of Recklessness (rank 4)
};
constexpr char const* kWindfuryTotem = "Windfury Totem";
struct TalentDebuff { std::uint32_t talent; std::uint32_t debuff; };
constexpr TalentDebuff kTalentDebuffs[] = {
    { 11180, 12579 },   // Winter's Chill -> Winter's Chill (5 stacks)
    { 11095, 22959 },   // Improved Scorch -> Fire Vulnerability (5 stacks)
    { 15257, 15258 },   // Shadow Weaving -> Shadow Weaving (5 stacks)
};
constexpr int kDebuffStacks = 5;

bool IsCasterClass(std::uint8_t cls)
{
    return cls == CLASS_MAGE || cls == CLASS_WARLOCK || cls == CLASS_PRIEST || cls == CLASS_SHAMAN || cls == CLASS_DRUID;
}

// A shaman or druid plays a melee spec with the melee kit: the buffed enhancement shaman 151 got Arcane Intellect, Greater Arcane Elixir and Curse of the
// Elements and no Mongoose, Juju, Faerie Fire or Curse of Recklessness, and gained x1.4 from t3 to the buffed leg where arms gained x2.7.
bool IsCasterBot(Player* p)
{
    std::uint8_t const cls = p->GetClass();
    if (!IsCasterClass(cls))
        return false;
    auto const spec = std::uint32_t(::Playerbot::Classic::EffectiveSpecId(p));
    if (cls == CLASS_SHAMAN)
        return spec != 263;                       // Enhancement
    if (cls == CLASS_DRUID)
        return spec != 103 && spec != 104;        // Feral and Guardian; Balance (102) and Restoration (105) cast
    return true;
}

constexpr int kDummyArmor = 3731;                  // a level 63 raid boss
constexpr int kSunderedArmor = kDummyArmor - 5 * 520;   // only the expectation when a leg's measured armor is missing; the legs apply the real aura
constexpr std::uint32_t kSunderArmorSpell = 11597;      // Sunder Armor, top rank; its amount and stack limit are the client's
constexpr int kSunderStacks = 5;
constexpr int kSensitivitySp = 500, kSensitivityAp = 400;   // large enough to stand out of the sampling noise of a few dozen hits

constexpr Leg kLegs[] = {
    { "none",        0, kDummyArmor,    0, 0 },
    { "t1",          1, kDummyArmor,    0, 0 },
    { "t3",          3, kDummyArmor,    0, 0 },
    { "t3_buffed",   3, kDummyArmor,    0, 0, kSunderStacks, true },
    { "t3_sp500",    3, kDummyArmor,    kSensitivitySp, 0 },
    { "t3_ap400",    3, kDummyArmor,    0, kSensitivityAp },
    { "t3_armor0",   3, 0,              0, 0 },
    { "t3_sunder5",  3, kDummyArmor,    0, 0, kSunderStacks },
};
constexpr std::size_t kLegCount = sizeof(kLegs) / sizeof(kLegs[0]);
constexpr std::size_t kReferenceLegCount = 4;      // none, t1, t3, t3_buffed

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
enum class Measure : std::uint8_t { Damage, DotTotal, AuraDuration, ArmorDrop, Heal };

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
    // The player spell check (Kind::SpellCheck) only:
    double floor = 0.0;                   // the mean plain hit (or heal) must be at least this at level 60 in tier 3 gear, after the dummy's armor
    std::uint32_t exact_id = 0;           // cast exactly this id (the one a player's spellbook has), not the highest rank found by name
    bool teach = false;                   // with exact_id: a bot that does not know it learns it for the check and loses it afterwards
    std::uint32_t pre_aura = 0;           // put this aura on the caster before each cast (Victorious before Victory Rush)
};

// One player spell check item: damage on the dummy (or a heal on the caster), `reps` casts, with its floor.
SweepItem CheckItem(std::uint8_t cls, char const* spell, double floor, Measure m = Measure::Damage, int reps = 5)
{
    // A damage over time is cast three times: one cast can miss the level 63 dummy (Corruption, second 0.1.30 run: one cast, one miss).
    SweepItem it{ cls, spell, Res::None, { 0 }, m, m == Measure::DotTotal ? 3 : reps };
    it.floor = floor;
    return it;
}

// The key spells per class for the player spell check. The floors are deliberately low (roughly a third of a vanilla rank-max hit at level
// 60, after the level 63 dummy's 3731 armor for physical hits): they catch a spell that hits for 1 or 2, or for a tenth of what it should,
// without failing on normal rolls, partial resists or missing talents. The second check, against the client's own spell data, is in
// SpellCheckDataValue. A talent spell the bot's build does not have is SKIPPED, not failed.
std::vector<SweepItem> SpellCheckItems(std::uint8_t cls)
{
    std::vector<SweepItem> v;
    switch (cls)
    {
        case 1:
        {
            // Victory Rush is the Classic 1.60 spell a player has (402927), usable after a kill gives Victorious (402975); a bot may not have it.
            SweepItem vr = CheckItem(1, "Victory Rush", 60);   // 1 + 15 % of attack power, through 3731 armor
            vr.exact_id = 402927;
            vr.teach = true;
            vr.pre_aura = 402975;
            v.push_back(vr);
            v.push_back(CheckItem(1, "Heroic Strike", 200));
            v.push_back(CheckItem(1, "Mortal Strike", 250));
            v.push_back(CheckItem(1, "Bloodthirst", 200));
            v.push_back(CheckItem(1, "Whirlwind", 200));
            v.push_back(CheckItem(1, "Cleave", 150));
            v.push_back(CheckItem(1, "Slam", 200));
            v.push_back(CheckItem(1, "Shield Slam", 150));
            v.push_back(CheckItem(1, "Thunder Clap", 30));
            v.push_back(CheckItem(1, "Rend", 60, Measure::DotTotal));
            SweepItem ex = CheckItem(1, "Execute", 300);
            ex.res = Res::Rage;
            ex.points = { 30 };
            ex.low_health = true;
            v.push_back(ex);
            break;
        }
        case 2:
            v.push_back(CheckItem(2, "Crusader Strike", 100));
            v.push_back(CheckItem(2, "Holy Shock", 200));
            v.push_back(CheckItem(2, "Consecration", 40, Measure::DotTotal));   // a ground effect: read 2 s after the cast, a few ticks
            {
                SweepItem how = CheckItem(2, "Hammer of Wrath", 200);
                how.low_health = true;
                v.push_back(how);
            }
            v.push_back(CheckItem(2, "Holy Light", 500, Measure::Heal, 3));
            v.push_back(CheckItem(2, "Flash of Light", 200, Measure::Heal, 3));
            break;
        case 3:
            v.push_back(CheckItem(3, "Arcane Shot", 80));
            v.push_back(CheckItem(3, "Multi-Shot", 100));
            v.push_back(CheckItem(3, "Aimed Shot", 250));
            v.push_back(CheckItem(3, "Raptor Strike", 80));
            v.push_back(CheckItem(3, "Serpent Sting", 150, Measure::DotTotal));
            break;
        case 4:
        {
            v.push_back(CheckItem(4, "Sinister Strike", 100));
            v.push_back(CheckItem(4, "Hemorrhage", 100));
            SweepItem ev = CheckItem(4, "Eviscerate", 300);
            ev.res = Res::Combo;
            ev.points = { 5 };
            v.push_back(ev);
            break;
        }
        case 5:
            v.push_back(CheckItem(5, "Smite", 250));
            v.push_back(CheckItem(5, "Mind Blast", 300));
            v.push_back(CheckItem(5, "Holy Fire", 300));
            v.push_back(CheckItem(5, "Mind Flay", 150, Measure::DotTotal));   // a channel: its damage comes as ticks
            v.push_back(CheckItem(5, "Shadow Word: Pain", 400, Measure::DotTotal));
            v.push_back(CheckItem(5, "Flash Heal", 400, Measure::Heal, 3));
            v.push_back(CheckItem(5, "Greater Heal", 1000, Measure::Heal, 3));
            break;
        case 7:
            v.push_back(CheckItem(7, "Lightning Bolt", 250));
            v.push_back(CheckItem(7, "Chain Lightning", 250));
            v.push_back(CheckItem(7, "Earth Shock", 250));
            v.push_back(CheckItem(7, "Flame Shock", 100));
            v.push_back(CheckItem(7, "Stormstrike", 150));
            v.push_back(CheckItem(7, "Healing Wave", 700, Measure::Heal, 3));
            v.push_back(CheckItem(7, "Lesser Healing Wave", 400, Measure::Heal, 3));
            break;
        case 8:
            v.push_back(CheckItem(8, "Fireball", 350));
            v.push_back(CheckItem(8, "Frostbolt", 300));
            v.push_back(CheckItem(8, "Fire Blast", 250));
            v.push_back(CheckItem(8, "Scorch", 120));
            v.push_back(CheckItem(8, "Arcane Explosion", 100));
            v.push_back(CheckItem(8, "Arcane Missiles", 80));
            v.push_back(CheckItem(8, "Pyroblast", 500));
            break;
        case 9:
            v.push_back(CheckItem(9, "Shadow Bolt", 350));
            v.push_back(CheckItem(9, "Searing Pain", 120));
            v.push_back(CheckItem(9, "Immolate", 100));
            v.push_back(CheckItem(9, "Soul Fire", 500));
            v.push_back(CheckItem(9, "Corruption", 400, Measure::DotTotal));
            v.push_back(CheckItem(9, "Curse of Agony", 500, Measure::DotTotal));
            break;
        case 11:
        {
            v.push_back(CheckItem(11, "Wrath", 150));
            v.push_back(CheckItem(11, "Starfire", 350));
            v.push_back(CheckItem(11, "Moonfire", 80));
            v.push_back(CheckItem(11, "Insect Swarm", 150, Measure::DotTotal));
            v.push_back(CheckItem(11, "Claw", 80));
            v.push_back(CheckItem(11, "Maul", 80));
            SweepItem fb = CheckItem(11, "Ferocious Bite", 250);
            fb.res = Res::Combo;
            fb.points = { 5 };
            v.push_back(fb);
            v.push_back(CheckItem(11, "Healing Touch", 800, Measure::Heal, 3));
            v.push_back(CheckItem(11, "Regrowth", 300, Measure::Heal, 3));
            break;
        }
        default:
            break;
    }
    return v;
}

std::vector<SweepItem> SweepItemsFor(Kind kind, std::uint8_t cls)
{
    std::vector<SweepItem> v;
    std::vector<int> const combo = { 1, 2, 3, 4, 5 };
    if (kind == Kind::Finisher)
    {
        if (cls == 4)
        {
            // A finisher's hit is a roll inside a range, and a crit casts leave no plain-hit sample: twelve casts a point keep the mean steady enough to order the points
            v.push_back({ 4, "Eviscerate", Res::Combo, combo, Measure::Damage, 12 });
            v.push_back({ 4, "Rupture", Res::Combo, combo, Measure::DotTotal, 2 });
            v.push_back({ 4, "Kidney Shot", Res::Combo, combo, Measure::AuraDuration, 1 });
            v.push_back({ 4, "Slice and Dice", Res::Combo, combo, Measure::AuraDuration, 1 });
            v.push_back({ 4, "Expose Armor", Res::Combo, combo, Measure::ArmorDrop, 1 });
        }
        else if (cls == 11)
        {
            v.push_back({ 11, "Ferocious Bite", Res::Combo, combo, Measure::Damage, 12 });
            v.push_back({ 11, "Rip", Res::Combo, combo, Measure::DotTotal, 2 });
        }
        else if (cls == 1)
        {
            SweepItem exec = { 1, "Execute", Res::Rage, { 15, 30, 45, 60, 80, 100 }, Measure::Damage, 6 };
            exec.low_health = true;
            v.push_back(exec);
        }
    }
    else if (kind == Kind::SpellCheck)
        return SpellCheckItems(cls);
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
    { 4, "Ruthlessness", 0 }, { 4, "Initiative", 0 }, { 4, "Improved Kick", 0 }, { 4, "Seal Fate", 0 }, { 4, "Relentless Strikes", 0 }, { 4, "Setup", 0 },
    { 4, "Murder", 0 }, { 4, "Hack and Slash", 0 }, { 4, "Cold Blood", 0 }, { 4, "Improved Poisons", 0 }, { 4, "Vile Poisons", 0 },
    // The shaman audit runs the Enhancement spec: Elemental Fury and Call of Thunder are Elemental talents it never buys (the 147 audit listed them as missing).
    { 7, "Flurry", 0 }, { 7, "Thundering Strikes", 0 }, { 7, "Elemental Weapons", 0 }, { 7, "Anticipation", 0 }, { 7, "Spirit Weapons", 0 },
    { 7, "Shamanistic Focus", 0 }, { 7, "Improved Stormstrike", 0 }, { 7, "Convection", 0 },
    { 8, "Arcane Concentration", 0 }, { 8, "Ignite", 0 }, { 8, "Improved Scorch", 0 }, { 8, "Winter's Chill", 0 }, { 8, "Critical Mass", 0 },
    { 5, "Shadow Weaving", 0 }, { 5, "Shadow Power", 0 }, { 5, "Darkness", 0 },
    { 11, "Predatory Strikes", 0 }, { 11, "Savage Fury", 0 }, { 11, "Moonfury", 0 }, { 11, "Vengeance", 0 },
    { 11, "Leader of the Pack", 0 }, { 11, "Heart of the Wild", 0 }, { 11, "Omen of Clarity", 0 }, { 11, "Blood Frenzy", 0 }, { 11, "Furor", 0 },
    { 11, "Ferocity", 0 }, { 11, "Nature's Grace", 0 }, { 11, "Improved Starfire", 0 }, { 11, "Improved Enrage", 0 }, { 11, "Sharpened Claws", 0 },
    { 11, "Natural Weapons", 0 },
    { 2, "Conviction", 5 }, { 2, "Vengeance", 0 }, { 2, "Two-Handed Weapon Specialization", 0 },
    { 3, "Lethal Shots", 5 }, { 3, "Mortal Shots", 0 }, { 3, "Slaying", 0 }, { 3, "Barrage", 0 },
};

enum class Step : std::uint8_t { Prepare, Settle, Warm, Measure, SweepSetup, SweepCast, SweepWait, Econ, Incoming, Trap, Threat, Finished };

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

// The threat audit's progress: the class's threat reducer (Feint, Fade, Disengage, Cower) cast at two starting threat levels. Phase 0 sets the
// threat up and casts, phase 1 reads it back.
struct ThreatAudit
{
    std::size_t level = 0;
    int phase = 0;
    std::uint32_t t = 0;
    std::uint32_t spell_id = 0;
    float before = 0.f;
    int attempt = 1;
};

struct State
{
    bool active = false;
    Kind kind = Kind::None;
    ObjectGuid subject, dummy;
    std::uint8_t cls = 0;
    int sunder_armor = 0;   // the dummy's armor under five real Sunder Armors, measured
    int leg_armor = 0;   // the armor the dummy had in the leg running, auras included
    std::string cls_name;
    std::uint32_t leg_seconds = 180;
    Step step = Step::Prepare;
    std::uint32_t step_s = 0;           // seconds in the current step
    std::size_t leg = 0;
    std::vector<std::size_t> leg_idx;   // the legs this run measures, as indexes into kLegs (Playerbot.DamageParity.Legs; all of them when unset)
    std::vector<LegResult> results;
    std::string failure;
    bool no_reference = false;          // no sourced dps reference for this spec (warrior Arms: the 800-1100 note has no source); judged by the formula check, dps is reported only
    bool tank_spec = false;             // a protection leg is a tank: its damage is not held to the vanilla DPS ranges (the casts are still checked)
    // time usage over the leg
    std::uint32_t casting_s = 0, attacking_s = 0, idle_s = 0, mana_refills = 0;
    // rage users, sampled once a second over the leg: whether the rage dump (Heroic Strike at 50) is starved or capped
    std::uint32_t rage_samples = 0, rage_sum = 0, rage_ge30_s = 0, rage_ge50_s = 0, rage_cap_s = 0;
    // seconds each aura of the bot's own was up over the leg (Flurry, Enrage, Death Wish, ...), by spell name
    std::map<std::string, std::uint32_t> aura_s;
    std::uint64_t rage_hit_damage_at_start = 0, rage_hit_gained_at_start = 0;
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
    // the player spell check
    std::vector<std::uint32_t> taught;    // spells learned for the check, unlearned at the end
    std::uint32_t health_before = 0;      // a heal's caster health just before the cast
    std::vector<double> check_samples;    // every sample of the item (g.samples is emptied per point)
    std::string check_seen;               // what the last cast of the item did, by spell id (for a cast that landed nothing)
    double dummy_lost = 0.0;              // the dummy's health lost over the item's casts (it is reset before each cast)
    std::uint32_t dummy_hp_start = 0;     // the dummy's health right after the reset of this cast
    std::uint32_t item_started_ms = 0;    // when the item began: one spell may not hold up the class
    std::uint32_t cast_begin_ms = 0;      // the cast-time spell just started, and how long its cast is
    std::int32_t cast_expected_ms = 0;
    int casts_completed = 0, casts_cut = 0;   // cast-time casts that ran their full time / ended early (interrupted, cancelled)
    std::uint32_t caster_hp_lost = 0;     // health the caster lost while casting (it is topped up before each cast)
    std::int64_t power_before = 0;        // the caster's mana (or rage, energy) just before the cast, and what the casts spent in all:
    std::int64_t power_spent = 0;         // a cast the core finished takes its cost, one that failed at its end takes nothing
    bool far = false;                     // the item is cast again from 15 yd: nothing at all landed from 3 yd
    std::string near_result;              // what the 3 yd casts did, for the row of the 15 yd retry
    std::string dummy_state;              // the dummy at the last sample: evading, immune to the spell, in combat, its combat reach
    int evade_ticks = 0;                  // runner ticks of the item the dummy spent evading
    std::uint32_t cast_done_s = UINT32_MAX;   // step_s at which the cast and its missile were over: a sample settles from there, not from the cast start
    double data_value = 0.0;              // what the client's spell data gives for the item (SpellCheckDataValue)
    bool state_from_aura = false;         // the pre-cast aura set the spell's caster aura state by itself
    int check_pass = 0, check_fail = 0, check_skip = 0;
    std::string check_failed;             // the failed spells, for the summary line
    bool audit_done = false;
    Econ econ;
    Incoming incoming;
    TrapAudit trap;
    ThreatAudit threat;
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

// The leg the run is on, and how many it has.
Leg const& CurLeg()
{
    return kLegs[g.leg_idx.empty() ? std::min(g.leg, kLegCount - 1) : g.leg_idx[std::min(g.leg, g.leg_idx.size() - 1)]];
}

std::size_t LegTotal() { return g.leg_idx.empty() ? kLegCount : g.leg_idx.size(); }

// Playerbot.DamageParity.Legs: a comma list of leg labels (none, t1, t3, t3_sp500, t3_ap400, t3_armor0, t3_sunder5), measured in the order
// given. Unset or empty = every leg. A label the table does not have is logged and skipped; a list with none valid falls back to every leg.
std::vector<std::size_t> ConfiguredLegs()
{
    std::vector<std::size_t> out;
    std::string const list = sConfigMgr->GetStringDefault("Playerbot.DamageParity.Legs", "", true);
    std::size_t pos = 0;
    while (pos <= list.size())
    {
        std::size_t const comma = list.find(',', pos);
        std::string label = list.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        pos = comma == std::string::npos ? list.size() + 1 : comma + 1;
        label.erase(std::remove_if(label.begin(), label.end(), [](unsigned char c) { return std::isspace(c) != 0; }), label.end());
        if (label.empty())
            continue;
        bool found = false;
        for (std::size_t i = 0; i < kLegCount; ++i)
            if (label == kLegs[i].label)
            {
                out.push_back(i);
                found = true;
                break;
            }
        if (!found)
            TC_LOG_WARN("playerbot.v2", "[dmg_parity] Playerbot.DamageParity.Legs: no leg called \"{}\" (legs: none, t1, t3, t3_sp500, t3_ap400, t3_armor0, t3_sunder5)", label);
    }
    return out;
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

// The dummy's Sunder Armor stacks as the real aura (what a player's Sunder Armor casts leave), or none; returns the armor the dummy then has,
// so the ratio checks use the client's amount and not a number of ours.
int SetSunders(Player* p, Creature* d, int stacks)
{
    d->RemoveAurasDueToSpell(kSunderArmorSpell);
    if (stacks > 0)
    {
        if (Aura* aura = p->AddAura(kSunderArmorSpell, d))
        {
            aura->SetStackAmount(std::uint8_t(stacks));
            // A raid keeps Sunder Armor up for the whole fight; the aura lasts 30 s, so left alone it fell off a sixth of the way into a 180 s leg
            // (139: the buffed leg's armor read 3731 again at its end and its damage was an unsundered one).
            aura->SetMaxDuration(-1);
            aura->SetDuration(-1);
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] Sunder Armor {}: {} stacks asked, {} on the dummy, armor {} (base {})", kSunderArmorSpell, stacks, aura->GetStackAmount(), d->GetArmor(),
                        d->GetFlatModifierValue(UNIT_MOD_ARMOR, BASE_VALUE));
        }
        else
            TC_LOG_ERROR("playerbot.v2", "[dmg_parity] Sunder Armor {} could not be applied to the dummy", kSunderArmorSpell);
    }
    return int(d->GetArmor());
}

// The highest-rank spell of that exact name with an aura effect and no summon effect (Windfury Totem: the totem spell summons, its aura is a
// second spell of the same name), or 0.
std::uint32_t FindAuraSpellByName(char const* name)
{
    std::uint32_t found = 0;
    int found_rank = -1;
    LocaleConstant const loc = sWorld->GetDefaultDbcLocale();
    sSpellMgr->ForEachSpellInfo([&](SpellInfo const* si)
    {
        if (!si || si->Difficulty != DIFFICULTY_NONE || !si->SpellName || std::string_view(name) != (*si->SpellName)[loc])
            return;
        bool aura = false, summon = false;
        for (SpellEffectInfo const& e : si->GetEffects())
        {
            aura = aura || (e.IsEffect() && e.IsAura());
            summon = summon || e.Effect == SPELL_EFFECT_SUMMON;
        }
        if (aura && !summon && (int(si->GetRank()) > found_rank || (int(si->GetRank()) == found_rank && si->Id > found)))
        {
            found = si->Id;
            found_rank = int(si->GetRank());
        }
    });
    return found;
}

// Faerie Fire and Curse of Recklessness on the dummy (with the Sunders, what a melee raid's boss wore), or off it; held for the whole leg.
void SetArmorShred(Player* p, Creature* d, bool on)
{
    for (std::uint32_t const id : kArmorShredSpells)
    {
        d->RemoveAurasDueToSpell(id);
        if (!on || IsCasterBot(p))
            continue;
        if (Aura* aura = p->AddAura(id, d))
        {
            aura->SetMaxDuration(-1);
            aura->SetDuration(-1);
        }
        else
            TC_LOG_ERROR("playerbot.v2", "[dmg_parity] armor shred {} could not be applied to the dummy", id);
    }
    if (on && !IsCasterBot(p))
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] armor shred on the dummy: armor now {}", d->GetArmor());
}

// The boss's raid debuffs for a caster leg (kCasterDebuffSpells, and the ones the bot's own talents put on it), or none. They last for the whole leg
// like the Sunders do.
void SetCasterDebuffs(Player* p, Creature* d, bool on)
{
    auto remove = [&](std::uint32_t id) { d->RemoveAurasDueToSpell(id); };
    auto add = [&](std::uint32_t id, int stacks)
    {
        if (Aura* aura = p->AddAura(id, d))
        {
            aura->SetStackAmount(std::uint8_t(stacks));
            aura->SetMaxDuration(-1);
            aura->SetDuration(-1);
        }
        else
            TC_LOG_ERROR("playerbot.v2", "[dmg_parity] raid debuff {} could not be applied to the dummy", id);
    };
    for (std::uint32_t const id : kCasterDebuffSpells)
    {
        remove(id);
        if (on && IsCasterBot(p))
            add(id, 1);
    }
    for (TalentDebuff const& t : kTalentDebuffs)
    {
        remove(t.debuff);
        // A warlock raid had a shadow priest's Shadow Weaving on its boss, whatever the warlock's own talents.
        if (on && (p->HasAura(t.talent) || (t.debuff == 15258 && p->GetClass() == CLASS_WARLOCK)))
            add(t.debuff, kDebuffStacks);
    }
    if (on && IsCasterBot(p))
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] caster raid debuffs on the dummy: {}", [&]
        {
            std::string out;
            for (auto const& [id, app] : d->GetAppliedAuras())
                out += fmt::format("{}{}x{}", out.empty() ? "" : ",", id, app->GetBase()->GetStackAmount());
            return out;
        }());
}

// A caster is measured from a spot it can cast from: the dummy is put on the bot's own level (a bot standing on a bridge or a roof has ground
// 20 y below the 15 y spot in front of it: path_fail, level_guard, then the opener gave up on an "unreachable" dummy and shielded it for
// five minutes, 2026-10-19 Stormwind) and in line of sight, trying a few distances and directions. False when no spot works.
// The dummy must also be near: a bot that walked off on a bag run between legs left it 40 y away on the same level, so no leg-start relocation
// happened and the melee bot lost its first 60 s walking to it (warrior 144: 121 white swings in a leg instead of 204).
bool DummyOnBotLevel(Player* p, Creature* d, float distance, float max_distance = 30.0f)
{
    bool const shooter = p->GetClass() == CLASS_HUNTER;
    if (shooter)
        max_distance = std::min(max_distance, 20.0f);
    // A shooter's spot also has to be 12 y or more away (outside its dead zone: 159 left the dummy 3.5 y from the hunter after the melee leg) and clear from the shooter's side: the 156 hunter stood 26 y from a dummy that passed the dummy's own
    // line-of-sight test and still failed the shot's (autoshot_fail result 71, los 0), then walked up into its dead zone. It is measured
    // from 20 y at most, where the 143 run shot without trouble.
    auto good = [&] { return std::fabs(d->GetPositionZ() - p->GetPositionZ()) <= 2.5f && p->GetExactDist(d) <= max_distance && d->IsWithinLOSInMap(p) &&
                             (!shooter || (p->IsWithinLOSInMap(d) && p->GetExactDist(d) >= 12.0f)); };
    if (good())
        return true;
    Position const origin = d->GetPosition();
    bool const origin_level = std::fabs(d->GetPositionZ() - p->GetPositionZ()) <= 2.5f && d->IsWithinLOSInMap(p);
    // A hunter cannot shoot from inside its dead zone (11 y): the closer spots would put it into melee (146: Raptor Strike and white swings,
    // one Auto Shot), so its fallback spots lie farther out instead.
    std::array<float, 4> const distances = shooter ? std::array<float, 4>{ distance, 18.0f, 20.0f, 16.0f } : std::array<float, 4>{ distance, 12.0f, 8.0f, 5.0f };
    for (float const dist : distances)
        for (float const angle : { 0.0f, 1.5708f, -1.5708f, 3.1416f, 0.7854f, -0.7854f })
        {
            d->NearTeleportTo(p->GetNearPosition(dist, angle));
            if (good())
                return true;
        }
    // A hunter that walked off between legs can stand where no ground around it gives a clear shot (147: the dummy 21 y away without line
    // of sight at every spot tried, so the hunter walked up to it and stood at 3.5 y, inside its dead zone, in melee). The harness puts the
    // hunter itself back, at a shooting distance from the dummy, the way it staged it.
    if (shooter)
    {
        Position const bot_origin = p->GetPosition();
        d->NearTeleportTo(origin);
        for (float const dist : { distance, 18.0f, 20.0f })
            for (float const angle : { 0.0f, 1.5708f, -1.5708f, 3.1416f, 0.7854f, -0.7854f })
            {
                p->NearTeleportTo(d->GetNearPosition(dist, angle));
                if (good())
                    return true;
            }
        p->NearTeleportTo(bot_origin);
    }
    // No spot near the bot works. A dummy left at the last try stood 5 y from a hunter (146: Raptor Strike only, 93 dps against 219): it goes back
    // to where it was, which at least was on the bot's level and in sight when it was far (the old rule).
    d->NearTeleportTo(origin);
    return origin_level;
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
    // A leg ends the fight with CombatStop, which leaves a passive dummy's AI "engaged" with no combat: its next update (CreatureAI::UpdateVictim)
    // evades, and an evade removes every aura (Unit::RemoveAurasOnEvade), the raid debuffs the next leg's setup had just put on it. That is why
    // buffed legs started with the dummy at base armor and no Sunder (hunter, priest, shaman, fury 149/151; mage fire 166). The dummy keeps its auras on evade.
    s->SetRestoreStateOnEvade(false);
    s->SetLevel(63);
    s->SetRegenerateHealth(false);
    s->SetMaxHealth(2000000000u);
    s->SetFullHealth();
    if (!DummyOnBotLevel(p, s, distance))
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] no spot on {}'s level with line of sight to the dummy: bot z={:.1f} dummy z={:.1f}", p->GetName(), p->GetPositionZ(), s->GetPositionZ());
    SetDummyArmor(s, armor);
    // The test dummy's template carries 100 fire and 100 arcane resistance, which no Naxxramas boss base has (vanilla raid bosses have none beyond the
    // level difference the spell hit already adds); it took about a quarter off every Arcane Shot and fire spell (hunter 201: Arcane Shot 138 against 182).
    for (std::uint32_t school = SPELL_SCHOOL_HOLY; school <= SPELL_SCHOOL_ARCANE; ++school)
    {
        s->SetStatFlatModifier(UnitMods(UNIT_MOD_RESISTANCE_START + school), BASE_VALUE, 0.0f);
        s->UpdateResistances(school);
    }
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

// The raid buffs of kRaidBuffSpells on the bot, or off it; the auras are the real spells, so their amounts are the client's.
// Brilliant Wizard Oil on a caster's main hand, as the caster kit carries it (ClassicSupplies kCasterKit): a temporary weapon enchant, not an aura,
// so it is set on the item itself. The enchantment is the one the oil's use spell names (nothing hard-coded); a leg without raid buffs takes it off.
constexpr std::uint32_t kWizardOilItem = 20749;
std::uint32_t WizardOilEnchant()
{
    ItemTemplate const* tpl = sObjectMgr->GetItemTemplate(kWizardOilItem);
    if (!tpl)
        return 0;
    for (ItemEffectEntry const* eff : tpl->Effects)
    {
        if (!eff || eff->TriggerType != ITEM_SPELLTRIGGER_ON_USE || eff->SpellID <= 0)
            continue;
        if (SpellInfo const* si = sSpellMgr->GetSpellInfo(std::uint32_t(eff->SpellID), DIFFICULTY_NONE))
            for (SpellEffectInfo const& e : si->GetEffects())
                if (e.IsEffect(SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY))
                    return std::uint32_t(e.MiscValue);
    }
    return 0;
}

void SetWizardOil(Player* p, bool on)
{
    Item* mh = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    std::uint32_t const enchant = WizardOilEnchant();
    if (!mh || !enchant)
        return;
    bool const has = mh->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT) == enchant;
    bool const want = on && IsCasterBot(p) && p->GetClass() != CLASS_SHAMAN;
    if (has == want)
        return;
    p->ApplyEnchantment(mh, TEMP_ENCHANTMENT_SLOT, false);
    mh->SetEnchantment(TEMP_ENCHANTMENT_SLOT, want ? enchant : 0, want ? 30u * 60u * 1000u : 0u, 0);
    p->ApplyEnchantment(mh, TEMP_ENCHANTMENT_SLOT, true);
    TC_LOG_INFO("playerbot.v2", "[dmg_parity] Brilliant Wizard Oil (enchantment {}) {} the main hand of {}", enchant, want ? "on" : "off", p->GetName());
}

// The caster kit's mana potions, runes and oil (ClassicSupplies kCasterKit, the entries flagged combat) in the bags, as a raid bot carries them: the
// fight's mana item rule drinks them when mana runs low. A leg without raid buffs empties them again.
void SetCasterKit(Player* p, bool on)
{
    if (!IsCasterBot(p) || p->GetClass() == CLASS_SHAMAN)
        return;
    for (Classic::RaidConsumable const& c : Classic::RaidConsumablesFor(Classic::RaidConsumableRole::Caster))
    {
        if (!c.combat || c.item == kWizardOilItem)
            continue;
        ItemTemplate const* tpl = sObjectMgr->GetItemTemplate(c.item);
        if (!tpl || tpl->GetClass() != ITEM_CLASS_CONSUMABLE)
            continue;
        std::uint32_t const held = p->GetItemCount(c.item);
        if (on && held < c.stock)
            p->StoreNewItemInBestSlots(c.item, c.stock - held, ItemContext::NONE);
        else if (!on && held)
            p->DestroyItemCount(c.item, held, true);
    }
}

// The regear of each leg leaves the gear it replaced in the bags, and with the caster kit stocked on top the bags were full: idle:capital_bag_run then
// fired in the buffed leg of the 205 fire mage (46 log lines against 4 in the leg before) and its casting fell to 115 s of 180. A raid bot sells its old gear;
// the scenario character drops it, leaving at least 8 free slots.
void ClearBagGear(Player* p, std::uint32_t kFreeWanted = 8)
{
    if (p->GetFreeInventorySlotCount() >= kFreeWanted)
        return;
    std::vector<Item*> gear;
    p->ForEachItem(ItemSearchLocation::Inventory, [&](Item* item)
    {
        if (ItemTemplate const* tpl = item->GetTemplate(); tpl && (tpl->GetClass() == ITEM_CLASS_ARMOR || tpl->GetClass() == ITEM_CLASS_WEAPON) && !item->IsEquipped())
            gear.push_back(item);
        return ItemSearchCallbackResult::Continue;
    });
    std::uint32_t dropped = 0;
    for (Item* item : gear)
    {
        if (p->GetFreeInventorySlotCount() >= kFreeWanted)
            break;
        p->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
        ++dropped;
    }
    TC_LOG_INFO("playerbot.v2", "[dmg_parity] {} dropped {} old gear items from the bags ({} free slots)", p->GetName(), dropped, p->GetFreeInventorySlotCount());
}

// `group`: the scorecard raid. A flask fills both elixir slots and the battle elixirs exclude each other, so the group applies the flask alone to a caster
// (the solo leg's Greater Arcane Elixir on top is the likeliest thing that took the flask off: group-4 casters held neither) and one battle elixir to a melee bot.
// `consumables` false: no flask, elixir or Juju, the bot's own kit (idle:always_buff) is what it fights with. The support scorecard (233) re-applied Supreme
// Power to the resto shaman and the Mongoose to the holy paladin and the tank every 15 s, each taking off the flask of the bot's own kit, which the bot then
// put back: the readiness audit found "no flask or elixir of the kit" on whichever had lost the last swap.
// A raid buff already on the bot is renewed when it has less than this left, not only put on once it is gone: an aura carried over from
// the scenario before (or a shout cast two minutes earlier) was kept as it was and ran out in the first seconds of the measurement
// (244 paladin run 1: Battle Shout missing on the fury, the rogue, the mage and the warlock at the start, 19:39:14). The window
// renews the buffs every 15 s, so 30 s leaves no gap.
constexpr int32 kRaidBuffRenewMs = 30000;

void SetRaidBuffs(Player* p, bool on, bool group = false, bool consumables = true)
{
    SetWizardOil(p, on);
    SetCasterKit(p, on);
    auto keep = [p](std::uint32_t id)
    {
        if (Aura* a = p->GetAura(id))
        {
            if (!a->IsPermanent() && a->GetDuration() < kRaidBuffRenewMs)
                a->RefreshDuration();
        }
        else
            p->AddAura(id, p);
    };
    if (IsCasterBot(p))
        for (std::uint32_t const id : kCasterBuffSpells)
        {
            if (on && !consumables && id == kSupremePowerSpell)
                continue;
            if (on)
                keep(id);
            else
                p->RemoveAurasDueToSpell(id);
        }
    for (std::uint32_t const id : kRaidBuffSpells)
    {
        if (on)
            keep(id);
        else
            p->RemoveAurasDueToSpell(id);
    }
    // Kings, the role's consumables and, for a melee class, Windfury Totem.
    auto set = [&](std::uint32_t id, char const* what)
    {
        if (!id)
        {
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] raid buff {} not found in the spell data, leg runs without it", what);
            return;
        }
        // Renewed like the list above, not only put on once gone: 246 warrior run 2 began with Kings missing on four raiders, a Kings carried
        // over from a session before (214 s left at the login) still on at the run reset and gone 5 s later at the readiness audit.
        if (!on)
            p->RemoveAurasDueToSpell(id);
        else if (Aura* a = p->GetAura(id))
        {
            if (!a->IsPermanent() && a->GetDuration() < kRaidBuffRenewMs)
                a->RefreshDuration();
        }
        else if (!p->AddAura(id, p))
            TC_LOG_ERROR("playerbot.v2", "[dmg_parity] raid buff {} ({}) could not be applied", id, what);
    };
    set(kKingsSpell, "Blessing of Kings");
    if (!on || consumables)
    {
        if (IsCasterBot(p))
        {
            if (!group)
                for (char const* name : kCasterConsumableAuras)
                    set(FindAuraSpellByName(name), name);
        }
        else if (p->GetClass() == CLASS_HUNTER)
            // A hunter's elixir is the Mongoose (agility and crit): Giants and Juju Power are strength, which does nothing for a bow, and as battle
            // elixirs they displaced the Mongoose (hunter 201: no Mongoose among the buffed leg's auras).
            set(FindAuraSpellByName("Elixir of the Mongoose"), "Elixir of the Mongoose");
        else
        {
            bool elixir_on = false;
            for (char const* name : kMeleeConsumableAuras)
            {
                bool const elixir = std::string_view(name).find("Elixir") != std::string_view::npos;
                if (group && elixir && elixir_on)
                    continue;
                set(FindAuraSpellByName(name), name);
                elixir_on = elixir_on || elixir;
            }
        }
    }
    if (!IsCasterBot(p) && p->GetClass() != CLASS_HUNTER && p->GetClass() != CLASS_SHAMAN)   // a shaman's own Windfury Weapon does not stack with the totem
    {
        std::uint32_t const wf = FindAuraSpellByName(kWindfuryTotem);
        if (on)
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] Windfury Totem aura spell {}", wf);
        set(wf, kWindfuryTotem);
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
    // Not while a placement's near teleport is still pending: the engage reads the old spot. 230: a hunter put back from 3.5 y to 24 y was
    // engaged in the same second, still at 3.5 y, so start_attack saw the dead zone and gave it a melee chase ([hunter_chase] opening_melee
    // dist 3.5); the teleport landed and the chase walked it straight back in (16 y, 8.6 y, 3.5 y over the next three seconds), every 10 s.
    if (p->IsBeingTeleported())
        return;
    ::Playerbot::API api(p);
    api.start_attack(d->GetGUID());
    if (BotAI* ai = ::Playerbot::Services::Registry().ai(p->GetGUID().GetCounter()))
    {
        ai->enter_manual(BotAI::OwnerCommand::Engage, d->GetGUID(), GameTime::GetGameTimeMS());
        ai->release_owner_lead();
    }
}

// The owner's Attack order alone, as a player gives it: the bot opens by its own rules, when its threat hold lets it. Engage's start_attack
// swung at once whatever the hold said, and repeated every second against a bot whose hold had stopped its swing it took the swing back up
// (241 support scorecard: the fury opened 2 s before the paladin tank, 2556 threat against 508, kept the boss for 107-120 of 120 s and was
// at 103-108 % of the tank while "held").
void EngageOrder(Player* p, Creature* d)
{
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
    // A hunter's shots (Auto Shot and every instant shot) each take an arrow: the t1 leg of 201 started with 92 and ran dry 120 s in, so the last
    // minute was melee (46 Auto Shots against 56 at a 3.2 s weapon). A quiver of a raid night is more than a leg uses.
    if (p->GetClass() == CLASS_HUNTER && p->NeedsAmmo())
    {
        // The ammo of the weapon now equipped: a leg that swaps a gun for a bow leaves the old bullets set as the ammo (Player::AutoSelectAmmo keeps
        // what is set), so 204's t1 leg had 199 arrows and a stack of bullets it could not shoot.
        Item const* ranged = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
        std::uint8_t const kind = ranged && ranged->GetTemplate()->GetSubClass() == ITEM_SUBCLASS_WEAPON_GUN ? 2 : 1;
        if (std::uint32_t const ammo = ::Playerbot::Classic::BestAmmo(kind, std::uint8_t(p->GetLevel())))
        {
            if (std::uint32_t const have = p->GetItemCount(ammo); have < 400u)
                p->AddItem(ammo, 400u - have);
            p->SetAmmo(ammo);
        }
    }
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
    Leg const& leg = CurLeg();
    ::Playerbot::V2::Fleet::BotPopulationManager::SetGearModeOverride(p->GetGUID().GetCounter(), leg.gear == 0 ? -1 : (leg.gear == 1 ? 2 : 4));
    // RegearNow does nothing in combat: the mage was still fighting the dummy when the tier 3 leg began, kept its tier 1 gear and measured
    // 265 dps with it as "tier 3" (2026-10-19).
    if (Creature* fighting = DummyCreature(p); fighting && fighting->IsAlive())
    {
        fighting->AttackStop();
        fighting->CombatStop(true);
    }
    p->AttackStop();
    p->CombatStop(true);
    if (leg.gear == 0)
        StripGear(p);
    else
        ::Playerbot::Services::Population().RegearNow(p);
    ClearBagGear(p);
    ApplyBonuses(p, leg.sp, leg.ap);
    // Own auras that the leg's setup takes off the bot are logged by name: the 180 demonology warlock lost its Burning Shadow (Demonic Sacrifice, 30 min)
    // between the t3 and the buffed leg, and nothing said which step removed it.
    std::set<std::uint32_t> before;
    for (auto const& [id, app] : p->GetAppliedAuras())
        before.insert(id);
    SetRaidBuffs(p, leg.buffed);
    for (std::uint32_t const id : before)
        if (!p->HasAura(id))
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] leg {} setup removed the bot's own aura {} ({})", leg.label, id, SpellNameOf(id));
    if (Creature* d = DummyCreature(p))
    {
        SetDummyArmor(d, leg.armor);
        SetArmorShred(p, d, leg.buffed);
        g.leg_armor = SetSunders(p, d, leg.sunders);
        SetCasterDebuffs(p, d, leg.buffed);
        if (leg.sunders > 0)
            g.sunder_armor = g.leg_armor;
        // The bot may have walked off in the last leg. When no spot near it puts the dummy on its level (the Stormwind staging spot is a ledge with
        // water 20 y below), the dummy stays down there and the melee bot's chase walks it off the edge: rogue 149 died of the fall (836, killer
        // self, z 79.8 to 60.5) as the third leg began. The bot is then put on open, flat ground and the dummy in front of it, as at the start.
        bool const spot_found = DummyOnBotLevel(p, d, 15.0f, 20.0f);
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] leg {} staging: dummy {:.1f} y from {} (spot {}, z {:.1f} / {:.1f}, los {})", leg.label, p->GetExactDist(d), p->GetName(),
                    spot_found ? "found" : "not found", p->GetPositionZ(), d->GetPositionZ(), int(p->IsWithinLOSInMap(d)));
        if (!spot_found)
        {
            float x, y, z, o;
            bool found = FindOpenSpot(p, 15.0f, x, y, z, o);
            if (!found && g_flat_spot.set && g_flat_spot.map == p->GetMapId() && p->GetExactDist(g_flat_spot.x, g_flat_spot.y, g_flat_spot.z) < 120.0f)
            {
                x = g_flat_spot.x; y = g_flat_spot.y; z = g_flat_spot.z; o = g_flat_spot.o;
                found = true;
            }
            if (found)
            {
                p->NearTeleportTo(x, y, z, o);
                d->NearTeleportTo(p->GetNearPosition(15.0f, 0.0f));
                TC_LOG_INFO("playerbot.v2", "[dmg_parity] leg={} no spot on the bot's level near the dummy: the bot is staged on open ground at {:.1f},{:.1f},{:.1f}", leg.label, x, y, z);
            }
        }
        // A mob held by a tank faces the tank, so the damage dealers stand behind it: no parries, and Shred, Backstab and the other
        // from-behind attacks are available. The dummy is passive and keeps whatever facing it was summoned or teleported with, so
        // the leg started with the bot in its front arc or behind it by chance (druid 156: t1 behind, 177 swings, no parry, 33 Shred;
        // t3 in front, 21 parries, no Shred, 43 Claw: 530 against 411 dps for the same character).
        d->SetFacingTo(p->GetAbsoluteAngle(d));
        // Why a dummy that should dodge about 6.5 % of a warrior's swings (creature base 5 %, skill 300 against a level 63 maximum of 315) showed 0 dodges in
        // hundreds of swings (136-140): its state, its auras, the chance the core computes, and whether the bot is in front of it.
        {
            std::string auras;
            for (auto const& [id, app] : d->GetAppliedAuras())
                auras += fmt::format("{}{}", auras.empty() ? "" : ",", id);
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] dummy dodge check leg={} dodge_chance={:.2f} parry_chance={:.2f} controlled={} casting={} bot_in_front_of_dummy={} dummy_in_front_of_bot={} stunned_flag={} flags_extra={:#x} auras=[{}]",
                        leg.label, p->GetUnitDodgeChance(BASE_ATTACK, d), p->GetUnitParryChance(BASE_ATTACK, d), d->HasUnitState(UNIT_STATE_CONTROLLED),
                        d->IsNonMeleeSpellCast(false, false, true), d->HasInArc(float(M_PI), p), p->HasInArc(float(M_PI), d), d->HasUnitFlag(UNIT_FLAG_STUNNED),
                        d->GetCreatureTemplate()->flags_extra, auras);
        }
        // What takes a spell's damage off on the way: the dummy's resistance per school and the damage-taken multiplier per school
        // (damage_parity 147: a balance druid's Starfire landed at 0.75-0.79 of the vanilla table with no spell power at all).
        {
            static constexpr std::array<std::pair<char const*, SpellSchoolMask>, 7> kSchools = {{
                { "physical", SPELL_SCHOOL_MASK_NORMAL }, { "holy", SPELL_SCHOOL_MASK_HOLY }, { "fire", SPELL_SCHOOL_MASK_FIRE },
                { "nature", SPELL_SCHOOL_MASK_NATURE }, { "frost", SPELL_SCHOOL_MASK_FROST }, { "shadow", SPELL_SCHOOL_MASK_SHADOW },
                { "arcane", SPELL_SCHOOL_MASK_ARCANE } }};
            std::string res;
            for (std::size_t i = 0; i < kSchools.size(); ++i)
                res += fmt::format("{}{}: resistance {} taken x{:.3f}", res.empty() ? "" : ", ", kSchools[i].first, d->GetResistance(SpellSchools(i)),
                                   d->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, kSchools[i].second));
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] dummy resistances leg={} level={} bot_level={} {}", leg.label, d->GetLevel(), p->GetLevel(), res);
            // The other half of what multiplies a spell's damage (the dummy's taken multiplier is above): what the bot itself does extra per school, and
            // every damage-done aura it carries, so an above-range leg can be told "Forever tuning" from "an aura stacking twice" (192 priest: 6-17 %
            // over its range, direct spells and ticks alike).
            std::string done;
            for (std::size_t i = 0; i < kSchools.size(); ++i)
                done += fmt::format("{}{}: x{:.3f}", done.empty() ? "" : ", ", kSchools[i].first, p->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, kSchools[i].second));
            std::string sources;
            for (AuraEffect const* eff : p->GetAuraEffectsByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE))
                if (eff->GetMiscValue() & (SPELL_SCHOOL_MASK_SHADOW | SPELL_SCHOOL_MASK_NORMAL | SPELL_SCHOOL_MASK_FIRE | SPELL_SCHOOL_MASK_FROST | SPELL_SCHOOL_MASK_NATURE))
                    sources += fmt::format("{}{}({}) mask {} {}%", sources.empty() ? "" : ", ", eff->GetSpellInfo()->Id, (*eff->GetSpellInfo()->SpellName)[sWorld->GetDefaultDbcLocale()], eff->GetMiscValue(), eff->GetAmount());
            std::string taken_sources;
            for (AuraEffect const* eff : d->GetAuraEffectsByType(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN))
                taken_sources += fmt::format("{}{}({}) mask {} {}%", taken_sources.empty() ? "" : ", ", eff->GetSpellInfo()->Id, (*eff->GetSpellInfo()->SpellName)[sWorld->GetDefaultDbcLocale()], eff->GetMiscValue(), eff->GetAmount());
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] damage multipliers leg={} bot done {} | bot sources [{}] | dummy taken sources [{}]", leg.label, done, sources, taken_sources);
        }
        if (BotAI* ai = ::Playerbot::Services::Registry().ai(p->GetGUID().GetCounter()))
            ai->note_engage(d->GetGUID(), GameTime::GetGameTimeMS(), 1u);   // an earlier leg's opener give-up must not shield the dummy now
    }
    // The staged bot stands on the ground with no movement left over from the walk or teleport that put it there: a falling flag or an unfinished
    // spline makes Unit::isMoving() true, which refuses its cast-time spells and every turn toward the dummy (166 priest buffed leg: 90 degrees off
    // the dummy for the whole leg).
    {
        float const gz = p->GetMap() ? p->GetMap()->GetHeight(p->GetPhaseShift(), p->GetPositionX(), p->GetPositionY(), p->GetPositionZ() + 1.0f, true, 4.0f) : INVALID_HEIGHT;
        if (gz > INVALID_HEIGHT + 1.0f && std::fabs(gz - p->GetPositionZ()) > 0.3f)
        {
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] leg={} the bot stood {:.1f} y off the ground (z {:.1f}, ground {:.1f}): put on it", leg.label, p->GetPositionZ() - gz, p->GetPositionZ(), gz);
            p->NearTeleportTo(p->GetPositionX(), p->GetPositionY(), gz, p->GetOrientation());
        }
        if (p->isMoving() || !p->movespline->Finalized())
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] leg={} the bot still moved at the start (flags 0x{:X}, spline finalized {}): stopped", leg.label, uint32(p->GetUnitMovementFlags()), int(p->movespline->Finalized()));
        // The motion master keeps the chase of the last leg's fight: a hunter that closed in on the old dummy to melee it walked on into the new
        // one (204: dummy staged 15 y away, bot at 3.5 y two seconds later, melee for the whole t1 and t3 leg while t3_buffed, which followed a
        // ranged leg, stood at range).
        p->GetMotionMaster()->Clear();
        p->StopMoving();
        p->RemoveUnitMovementFlag(MOVEMENTFLAG_MASK_MOVING);
        p->ClearUnitState(UNIT_STATE_MOVING);
        if (Creature* facing = DummyCreature(p))
            p->SetFacingToObject(facing);
    }
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
        g_casts.clear();
        g_subject = p->GetGUID();
        g_dummy = g.dummy;
    }
    g.casting_s = g.attacking_s = g.idle_s = g.mana_refills = 0;
    g.rage_samples = g.rage_sum = g.rage_ge30_s = g.rage_ge50_s = g.rage_cap_s = 0;
    g.aura_s.clear();
    g.rage_hit_damage_at_start = g_classicRageHitDamage.load();
    g.rage_hit_gained_at_start = g_classicRageHitGained.load();
    g.gear_at_start = GearSummary(p);
    // The settle and warm-up seconds are not measured, and the bot's rotation fires its long cooldowns in them: Combustion (3 min) was cast 5 s before
    // the 202 fire mage's buffed window and missing from it. The window starts with every cooldown ready, as a pull does.
    p->GetSpellHistory()->ResetAllCooldowns();
    g_measuring = true;
    g.step = Step::Measure;
    g.step_s = 0;
}

// A damaging spell the core accepted several times in a leg and that left no outcome at all on the dummy (not a hit, a miss or a tick) is
// broken, whatever the data says: the 2026-10-21 Execute and Overpower findings were only visible as a spell missing from the damage list.
// Matched by spell name, because a spell can deal its damage through another id (Execute casts 20647). Buffs and utility spells have no damage
// effect and are skipped.
constexpr std::uint32_t kMinCastsForZeroEffect = 3;

bool IsDamageSpell(SpellInfo const* info)
{
    if (!info || info->IsPositive())
        return false;
    bool dummy = false, aura = false;
    for (SpellEffectInfo const& effect : info->GetEffects())
    {
        switch (effect.Effect)
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
            case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
                return true;
            case SPELL_EFFECT_APPLY_AURA:
                if (effect.ApplyAuraName == SPELL_AURA_PERIODIC_DAMAGE)
                    return true;
                aura = true;
                break;
            case SPELL_EFFECT_DUMMY:
                dummy = true;
                break;
            default:
                break;
        }
    }
    // A dummy effect that carries the damage in a script (Execute), but not a debuff that happens to have one (Sunder Armor)
    return dummy && !aura;
}

// The spells each class's rotation must get the core to accept in a reference leg (ExpectedSpells/<Class>.inc, by name).
std::vector<char const*> ExpectedSpellNames(std::uint8_t cls)
{
    switch (cls)
    {
        case CLASS_WARRIOR: return {
#include "ExpectedSpells/Warrior.inc"
        };
        case CLASS_PALADIN: return {
#include "ExpectedSpells/Paladin.inc"
        };
        case CLASS_HUNTER: return {
#include "ExpectedSpells/Hunter.inc"
        };
        case CLASS_ROGUE: return {
#include "ExpectedSpells/Rogue.inc"
        };
        case CLASS_PRIEST: return {
#include "ExpectedSpells/Priest.inc"
        };
        case CLASS_SHAMAN: return {
#include "ExpectedSpells/Shaman.inc"
        };
        case CLASS_MAGE: return {
#include "ExpectedSpells/Mage.inc"
        };
        case CLASS_WARLOCK: return {
#include "ExpectedSpells/Warlock.inc"
        };
        case CLASS_DRUID: return {
#include "ExpectedSpells/Druid.inc"
        };
        default: return {};
    }
}

void CheckCastsLanded(LegResult const& r, std::map<std::uint32_t, std::uint32_t> const& casts)
{
    // Only the two reference legs: the sensitivity probes change gear and stats, not the rotation
    {
        // Every cast the core accepted in the leg, by name: a rotation that spends its time on instants, Life Tap or the wand shows here (the
        // 167 warlock stood 99 s of 180 not casting and the damage rows alone could not say what it did instead).
        std::string list;
        for (auto const& [spell_id, count] : casts)
            list += fmt::format("{}{} x{}", list.empty() ? "" : ", ", SpellNameOf(spell_id), count);
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] casts accepted leg={} {}", r.leg.label, list);
    }
    bool const reference = r.leg.sp == 0 && r.leg.ap == 0 && r.leg.armor == kDummyArmor && r.leg.sunders == 0 && r.leg.gear != 0;
    if (reference)
    {
        std::set<std::string> cast_names;
        for (auto const& [spell_id, count] : casts)
            cast_names.insert(SpellNameOf(spell_id));
        // A Fire mage's filler is Fireball (APL NukeSpell), so the Frostbolt of the list is the one it does not cast (149 mage_fire).
        bool const fire_mage = g.cls == CLASS_MAGE && ::Playerbot::Classic::GetSpecOverride(g_subject) == 63u;
        for (char const* listed : ExpectedSpellNames(g.cls))
        {
            char const* expected = fire_mage && std::string_view(listed) == "Frostbolt" ? "Fireball" : listed;
            if (!cast_names.contains(expected))
            {
                g.failure += fmt::format("{} never got {} accepted in the {} leg; ", g.cls_name, expected, r.leg.label);
                TC_LOG_ERROR("playerbot.v2", "[dmg_parity] expected_spell_not_cast class={} leg={} spell=\"{}\" verdict=FAIL", g.cls_name, r.leg.label, expected);
            }
        }
    }

    std::set<std::string> seen_names;
    for (auto const& [key, a] : r.aggs)
        if (!(key & kPetFlag) && (a.Attempts() > 0 || a.ticks > 0))   // direct hits are booked as ticks while the core does not report outcomes
            seen_names.insert(r.names.at(key));

    for (auto const& [spell_id, count] : casts)
    {
        if (count < kMinCastsForZeroEffect)
            continue;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE);
        if (!IsDamageSpell(info))
            continue;
        std::string const name = SpellNameOf(spell_id);
        if (seen_names.contains(name))
            continue;
        g.failure += fmt::format("{} cast {} ({}) {} times in the {} leg and it never hit, missed or ticked on the dummy; ", g.cls_name, name, spell_id, count, r.leg.label);
        TC_LOG_ERROR("playerbot.v2", "[dmg_parity] zero_effect class={} leg={} spell=\"{}\" id={} casts={} outcomes=0 verdict=FAIL", g.cls_name, r.leg.label, name, spell_id, count);
    }
}

void ReportLeg(Player* p)
{
    g_measuring = false;
    LegResult r;
    std::map<std::uint32_t, std::uint32_t> casts;
    r.leg = CurLeg();
    r.seconds = double(g.step_s);
    {
        std::lock_guard<std::mutex> lock(g_mu);
        r.aggs = g_aggs;
        casts = g_casts;
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
    CheckCastsLanded(r, casts);
    double const pet_dps = r.seconds > 0.0 ? double(pet_total) / r.seconds : 0.0;

    int const tier = r.leg.gear;
    // The two reference legs (no stat bonus, normal armor) are held to the raid ranges; the others are sensitivity probes.
    bool const reference = r.leg.sp == 0 && r.leg.ap == 0 && r.leg.armor == kDummyArmor && (r.leg.sunders == 0 || r.leg.buffed) && !g.tank_spec && !g.no_reference;
    // The buffed leg (raid buffs, five Sunders) is what the Naxxramas range was logged under: held to the whole range, no unbuffed allowance.
    r.verdict = g.no_reference ? RangeVerdict::NoReference
              : r.leg.buffed && !g.tank_spec ? DpsVerdict(g.cls, tier, /*raid_buffs*/ true, r.dps, &r.range)
              // The Naxxramas ranges are raid-buffed numbers: the unbuffed T3 leg (no consumables, no enchants, a dummy with its full armor) is a
              // sensitivity probe, not held to them (paladin 198: 291 unbuffed against 658 buffed, from armor 3731 vs 336, AP and crit alone).
              : reference && tier != 3       ? DpsVerdict(g.cls, tier, /*raid_buffs*/ false, r.dps, &r.range)
                                             : RangeVerdict::NoReference;

    TC_LOG_INFO("playerbot.v2", "[dmg_parity] leg class={} leg={} gear={} tier_pieces={} armor={} secs={:.0f} dps={:.0f} dps_busy={:.0f} pet_dps={:.0f} casting_s={} attacking_s={} idle_s={} mana_refills={} range={} verdict={}",
                g.cls_name, r.leg.label, tier, TierPieces(p, tier), r.leg.armor, r.seconds, r.dps, (g.casting_s + g.attacking_s) ? double(total + pet_total) / double(g.casting_s + g.attacking_s) : 0.0, pet_dps, g.casting_s, g.attacking_s, g.idle_s, g.mana_refills,
                r.range.empty() ? "-" : r.range, ToString(r.verdict));
    if (g.rage_samples)
    {
        // Rage the melee hits gave over the leg against the formula, 7.5 x damage / (0.0091107836 L^2 + 3.225598133 L + 4.2652911): any gap is a
        // modifier (Dual Wield Specialization off hand, the rage-from-damage aura) and not the base rate.
        double const level = double(p->GetLevel());
        double const conversion = 0.0091107836 * level * level + 3.225598133 * level + 4.2652911;
        std::uint64_t const dmg = g_classicRageHitDamage.load() - g.rage_hit_damage_at_start;
        double const gained = double(g_classicRageHitGained.load() - g.rage_hit_gained_at_start) / 10.0;
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] rage from hits leg={} hit_damage={} rage_gained={:.0f} formula_rage={:.0f} ratio={:.2f}", r.leg.label, dmg, gained,
                    double(dmg) / conversion * 7.5, dmg ? gained / (double(dmg) / conversion * 7.5) : 0.0);
    }
    if (g.rage_samples)
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] rage leg={} samples={} avg={:.1f} secs_ge30={} secs_ge50={} secs_at_cap={}", r.leg.label, g.rage_samples,
                    double(g.rage_sum) / double(g.rage_samples), g.rage_ge30_s, g.rage_ge50_s, g.rage_cap_s);
    {
        std::string auras;
        for (auto const& [name, secs] : g.aura_s)
            auras += fmt::format("{}{}={}", auras.empty() ? "" : ", ", name, secs);
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] own auras leg={} (seconds up of {}): {}", r.leg.label, g.attacking_s + g.casting_s, auras.empty() ? "none" : auras);
    }
    // The dummy's auras at the end of the leg: debuffs the class puts on it (Shadow Weaving's Shadow Vulnerability, Shadow Word: Pain, Stormstrike...)
    // never show among the bot's own auras.
    if (Creature* d = DummyCreature(p))
    {
        std::string theirs;
        for (auto const& [id, app] : d->GetAppliedAuras())
            theirs += fmt::format("{}{}({}) x{}{}", theirs.empty() ? "" : ", ", SpellNameOf(id), id, app->GetBase()->GetStackAmount(),
                                  app->GetBase()->GetCasterGUID() == p->GetGUID() ? " mine" : "");
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] dummy auras leg={} at the end: {}", r.leg.label, theirs.empty() ? "none" : theirs);
    }
    // Weapon imbues and poisons: the temporary enchant on each hand and what the enchantment casts on a hit. A shaman with no Windfury
    // line in a leg is either missing the imbue or holding an enchantment whose spell does nothing here.
    {
        std::string imbues;
        for (uint8 slot : { EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND })
        {
            Item const* item = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;
            uint32 const enchantId = item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT);
            SpellItemEnchantmentEntry const* ench = sSpellItemEnchantmentStore.LookupEntry(enchantId);
            if (!ench)
                continue;
            std::string effects;
            for (uint8 e = 0; e < MAX_ITEM_ENCHANTMENT_EFFECTS; ++e)
                if (ench->Effect[e])
                    effects += fmt::format("{}type{}:arg{}({}) pts{}", effects.empty() ? "" : " ", uint32(ench->Effect[e]), ench->EffectArg[e],
                                           ench->Effect[e] == ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL ? SpellNameOf(ench->EffectArg[e]) : std::string("-"),
                                           ench->EffectPointsMin[e]);
            imbues += fmt::format("{}{}=enchant {} [{}] {}s left", imbues.empty() ? "" : "; ", slot == EQUIPMENT_SLOT_MAINHAND ? "main" : "off", enchantId,
                                  effects, item->GetEnchantmentDuration(TEMP_ENCHANTMENT_SLOT) / 1000);
        }
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] weapon imbues leg={} at the end: {}", r.leg.label, imbues.empty() ? "none" : imbues);
    }
    TC_LOG_INFO("playerbot.v2", "[dmg_parity] gear leg={} {} -> {}", r.leg.label, g.gear_at_start, GearSummary(p));
    if (p->GetClass() == CLASS_HUNTER)
        for (std::uint32_t const sid : { 2643u, 14290u, 25294u, 19434u, 20904u })
            if (SpellInfo const* si = sSpellMgr->GetSpellInfo(sid, DIFFICULTY_NONE))
                TC_LOG_INFO("playerbot.v2", "[dmg_parity] spell cooldown {} \"{}\": category {} category_recovery {} ms recovery {} ms cast {} ms (the shots that share category 2 share one cooldown)",
                            sid, (*si->SpellName)[sWorld->GetDefaultDbcLocale()], si->GetCategory(), si->CategoryRecoveryTime, si->RecoveryTime, si->CalcCastTime());
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
        // A melee / ranged class spell (Holy Strike, Seal of Command, the paladin judgements of the client data) rolls the melee crit chance.
        SpellInfo const* sheetSpell = (key & ~kPetFlag) ? sSpellMgr->GetSpellInfo(key & ~kPetFlag, DIFFICULTY_NONE) : nullptr;
        bool const physicalClass = sheetSpell && (sheetSpell->DmgClass == SPELL_DAMAGE_CLASS_MELEE || sheetSpell->DmgClass == SPELL_DAMAGE_CLASS_RANGED);
        double const expect = (key & ~kPetFlag) && !physicalClass ? r.crit_spell : r.crit_melee;
        if (!(key & kPetFlag) && !CritPctConsistent(a.CritPct(), expect, a.Landed()))
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] crit_mismatch leg={} spell=\"{}\" measured={:.1f}% sheet={:.1f}% over {} landed (spell dmg class {})", r.leg.label,
                        r.names[key & ~kPetFlag], a.CritPct(), expect, a.Landed(), sheetSpell ? int(sheetSpell->DmgClass) : -1);
    }
    Row(SummaryRow(g.cls_name, r.leg.label, tier == 0 ? "none" : tier == 1 ? "t1" : "t3", r.leg.buffed, r.dps, r.range, r.verdict, TopGaps(r)));
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
            // The spell power is added to the base damage before the damage-done multipliers, so the damage per point is the coefficient times those
            // multipliers (Darkness, Shadowform, Shadow Weaving, the curses): 190 read a shadow priest's Mind Blast as 0.806 against 0.429, which is
            // 0.429 x about 1.5 and no double application. The multipliers read now (the bot's own, the dummy's taken) are divided out.
            double mult = 1.0;
            if (SpellInfo const* info = sSpellMgr->GetSpellInfo(key, DIFFICULTY_NONE))
            {
                std::uint32_t const school = info->GetSchoolMask();
                mult = p->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, school);
                if (Creature* d = DummyCreature(p))
                    mult *= d->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, school);
            }
            double const raw_coeff = CoefficientPerPoint(a.AvgNormal(), it->second.AvgNormal(), kSensitivitySp);
            double const coeff = mult > 0.1 ? raw_coeff / mult : raw_coeff;
            Verdict const v = ref > 0.0 ? CompareValue(coeff, ref, 35.0) : Verdict::NoData;
            TC_LOG_INFO("playerbot.v2", "[dmg_parity] sp_coefficient class={} spell=\"{}\" measured={:.3f} (raw {:.3f} / damage multipliers {:.3f}) vanilla={} verdict={} (hit avg {:.0f} -> {:.0f} over +{} spell power, {} / {} hits)",
                        g.cls_name, name, coeff, raw_coeff, mult, ref > 0.0 ? fmt::format("{:.3f}", ref) : std::string("-"), ToString(v), a.AvgNormal(), it->second.AvgNormal(),
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
            // Two hands swing at different damage and the tally cannot tell them apart: the average of 60 swings moves 1.6 times between legs on the
            // mix of hands alone (rogue 146: 1.616 against vanilla 0.596). A dual wielder is compared on its special attacks only.
            if (!key && base->dual_wield)
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
                int const sa = g.sunder_armor > 0 ? g.sunder_armor : kSunderedArmor;
                auto const s = sunder->aggs.find(probe);
                if (s != sunder->aggs.end() && s->second.N(Outcome::Hit) >= 15 && a0 > 0.0)
                {
                    double const rs = s->second.AvgNormal() / a0, refs = 1.0 - VanillaArmorReduction(sa, 60);
                    Verdict const vs = RatioVerdict(rs, refs, 8.0);
                    TC_LOG_INFO("playerbot.v2", "[dmg_parity] armor_ratio class={} probe=\"{}\" damage at {} armor (five Sunders) / at 0 armor = {:.3f}, vanilla {:.3f} verdict={}", g.cls_name,
                                probe ? SpellNameOf(probe) : std::string("white swing"), sa, rs, refs, ToString(vs));
                    if (vs != Verdict::Ok)
                        g.failure += fmt::format("{} physical damage at {} armor is {:.3f} of unarmored, vanilla {:.3f} ({}); ", g.cls_name, sa, rs, refs, ToString(vs));
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
        // What fires the talent: the proc entry the core resolved (client flags or spell_proc row) and whether the aura sits on the bot
        if (SpellProcEntry const* proc = sSpellMgr->GetSpellProcEntry(info))
            effects += fmt::format(" [proc flags=0x{:x}/0x{:x} phase={} hit=0x{:x} family={} chance={:.0f} ppm={:.1f} cd={}ms charges={}]", std::uint32_t(proc->ProcFlags[0]),
                                   std::uint32_t(proc->ProcFlags[1]), int(proc->SpellPhaseMask), int(proc->HitMask), proc->SpellFamilyName, proc->Chance, proc->ProcsPerMinute,
                                   proc->Cooldown.count(), proc->Charges);
        else
            effects += " [no proc entry]";
        effects += p->HasAura(id) ? " [aura on the bot]" : " [aura NOT on the bot]";
        char const* verdict = !any ? "NO_EFFECT" : t.expect > 0.0 ? (match ? "ok" : "value_differs") : "listed";
        TC_LOG_INFO("playerbot.v2", "[talent_audit] class={} talent=\"{}\" spell={} rank={} expect={} verdict={}{}", g.cls_name, t.name, id, int(info->GetRank()),
                    t.expect > 0.0 ? fmt::format("{:.0f}", t.expect) : std::string("-"), verdict, effects);
        if (!any)
            g.failure += fmt::format("talent {} ({}) has no effect at all; ", t.name, id);
    }
}

// Starts one point of the current sweep item.
void SpellCheckRow(SweepItem const& it, std::uint32_t id, char const* verdict, std::string const& detail);

bool SweepSetup(Player* p)
{
    SweepItem const& it = g.items[g.item];
    g.cast_spell_id = FindKnownSpell(p, it.spell);
    if (g.kind == Kind::SpellCheck && it.exact_id)
    {
        // The id a player casts. A bot that knows the spell by another id (a retail rank) is still checked on the player's one.
        if (!p->HasSpell(it.exact_id) && it.teach && sSpellMgr->GetSpellInfo(it.exact_id, DIFFICULTY_NONE))
        {
            p->LearnSpell(it.exact_id, false, 0, true);
            if (p->HasSpell(it.exact_id))
            {
                g.taught.push_back(it.exact_id);
                TC_LOG_INFO("playerbot.v2", "[spell_check] class={} spell=\"{}\" id={} learned for the check (removed at the end)", g.cls_name, it.spell, it.exact_id);
            }
        }
        g.cast_spell_id = p->HasSpell(it.exact_id) ? it.exact_id : 0;
    }
    if (!g.cast_spell_id && g.kind == Kind::SpellCheck && it.exact_id)
    {
        // The player's own id must be there: a skip here would hide exactly the spell the check is for.
        ++g.check_fail;
        SpellCheckRow(it, it.exact_id, "FAIL", fmt::format("reason=\"the player's spell {} could not be learned or is not in the spell data\"", it.exact_id));
        g.failure += fmt::format("{} {} ({}): not learnable; ", g.cls_name, it.spell, it.exact_id);
        return false;
    }
    if (!g.cast_spell_id && g.kind == Kind::SpellCheck)
    {
        // A talent spell of another build, or a spell this client does not have: not a failure, but listed.
        ++g.check_skip;
        SpellCheckRow(it, it.exact_id, "SKIP", "not in the spellbook (talent of another build, or not in this client)");
        return false;
    }
    if (!g.cast_spell_id)
    {
        g.failure += fmt::format("{} does not know {}; ", g.cls_name, it.spell);
        TC_LOG_INFO("playerbot.v2", "[dmg_parity] sweep class={} spell=\"{}\" is not in the spellbook", g.cls_name, it.spell);
        return false;
    }
    g.rep = 0;
    g.samples.clear();
    g.check_samples.clear();
    g.check_seen.clear();
    g.dummy_lost = 0.0;
    g.item_started_ms = GameTime::GetGameTimeMS();
    g.cast_begin_ms = 0;
    g.cast_expected_ms = 0;
    g.casts_completed = g.casts_cut = 0;
    g.caster_hp_lost = 0;
    g.power_spent = 0;
    g.far = false;
    g.near_result.clear();
    g.dummy_state.clear();
    g.evade_ticks = 0;
    g.state_from_aura = false;
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
    // A bot that mounted on its way to the staging spot is refused every cast with SPELL_FAILED_NOT_MOUNTED (code 88); a player would dismount.
    if (p->IsMounted())
        p->RemoveAurasByType(SPELL_AURA_MOUNTED);
    if (info)
        EnsureForm(p, info);
    // A form that blocks the spell (Shadowform for a Holy heal, cat or bear form for Healing Touch after the feral items): a player shifts
    // out before casting. SPELL_FAILED_NOT_SHAPESHIFT (92) otherwise, which the first 0.1.30 run (N63) showed for all four heals.
    if (g.kind == Kind::SpellCheck && info && p->GetShapeshiftForm() != FORM_NONE && info->CheckShapeshift(p->GetShapeshiftForm()) != SPELL_CAST_OK)
        p->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
    TopUp(p);
    if (it.res == Res::Combo)
        p->SetPower(POWER_COMBO_POINTS, pts);
    else if (it.res == Res::Rage)
        p->SetPower(POWER_RAGE, pts * 10);
    else if (g.kind == Kind::SpellCheck && p->GetPowerType() == POWER_RAGE)
        p->SetPower(POWER_RAGE, p->GetMaxPower(POWER_RAGE));   // a warrior or a bear has rage as a player in a fight would
    if (g.kind == Kind::SpellCheck && info)
    {
        // The reagent a player carries (Soul Fire's Soul Shard: the fourth 0.1.30 run ran out after five casts and was refused 121).
        for (std::size_t r = 0; r < info->Reagent.size(); ++r)
            if (info->Reagent[r] > 0 && info->ReagentCount[r] > 0)
                if (std::uint32_t const have = p->GetItemCount(std::uint32_t(info->Reagent[r])); have < std::uint32_t(info->ReagentCount[r]) * 2u)
                    p->AddItem(std::uint32_t(info->Reagent[r]), std::uint32_t(info->ReagentCount[r]) * 2u - have);
        if (it.pre_aura)
        {
            bool const first = g.cast_ok == 0 && g.cast_failures == 0;
            p->AddAura(it.pre_aura, p);
            // Whether the aura alone opens the spell, as it must for a player after a kill (Victorious -> Victory Rush): logged, then forced so
            // the damage is checked either way.
            if (first)   // before any forcing below: a state forced for an earlier cast would hide a broken aura
                g.state_from_aura = !info->CasterAuraState || p->HasAuraState(AuraStateType(info->CasterAuraState), info, p);
        }
        if (info->CasterAuraState && !p->HasAuraState(AuraStateType(info->CasterAuraState), info, p))
            p->ModifyAuraState(AuraStateType(info->CasterAuraState), true);
        if (it.measure == Measure::Heal)
        {
            p->SetHealth(std::max<std::uint32_t>(1, p->GetMaxHealth() * 3 / 10));
            g.health_before = std::uint32_t(p->GetHealth());
        }
    }
    d->SetHealth(it.low_health ? d->GetMaxHealth() / 7 : d->GetMaxHealth());
    // The core sets the wounded aura states on the next unit update; an Execute cast in this same tick is refused (code 132, target aura
    // state) with the dummy already below 20 %.
    d->ModifyAuraState(AURA_STATE_WOUNDED_20_PERCENT, it.low_health);
    if (!it.watch_aura)   // a watched aura (the debuff a crit leaves on the dummy) has to survive from cast to cast
    {
        d->RemoveAllAuras();
        // Every point is a cast of its own: the stun of the cast before must not shorten (or refuse) this one through diminishing returns
        // (Kidney Shot 137: 2 s at one point, no stun at points two to five).
        d->ClearDiminishings();
    }
    // The dummy stands where the spell reaches: between its minimum and maximum range (a melee finisher with a minimum range, or a bot that
    // drifted, is out of range otherwise).
    if (info)
    {
        float const lo = info->GetMinRange(false), hi = info->GetMaxRange(false, p);
        // A spell with a minimum range (the hunter's shots, 8 yd) measures it with both combat reaches added: the second 0.1.30 run
        // stood the hunter 9 yd from the dummy (minimum + 1) and every shot was refused OUT_OF_RANGE. The spell check stands well clear.
        float const min_pad = g.kind == Kind::SpellCheck && lo > 0.0f ? p->GetCombatReach() + d->GetCombatReach() + 3.0f : 1.0f;
        float const want = std::clamp(g.far ? 15.0f : 3.0f, lo + min_pad, std::max(lo + min_pad, hi - 1.0f));
        if (std::fabs(p->GetExactDist(d) - want) > 1.5f)
            d->NearTeleportTo(p->GetNearPosition(want, 0.f));
    }
    // SetFacingToObject is a MoveFace spline: a caster turning on a spline counts as moving, and a cast-time spell that a move
    // interrupts can be cancelled on its first update (the second 0.1.30 run: Fireball, Frostbolt, Soul Fire cast five times each and
    // nothing at all recorded; Shadow Bolt and Pyroblast passed one run and failed another). The spell check turns the bot only when
    // it does not already face the dummy, and leaves the turn to cast_spell's own facing, as a player's cast faces once.
    if (g.kind != Kind::SpellCheck)
        p->SetFacingToObject(d);
    else if (!p->HasInArc(float(M_PI) / 3.0f, d))
        p->SetOrientation(p->GetAbsoluteAngle(d));
    // A missile hits a moment after the cast, and an evading target takes it as SPELL_MISS_EVADE, which reports no outcome at all
    // (Spell::PreprocessSpellHit). The passive dummy evades on its next update when it is engaged with no combat left (PassiveAI::UpdateAI),
    // towards its home, the spot it was summoned on. A mob a player fights stays in combat with them, so the spell check keeps the dummy in
    // combat with the bot and makes the spot it stands on its home. (The missiles that recorded nothing in the 0.1.30 runs were read
    // before they landed: see SweepWait.)
    if (g.kind == Kind::SpellCheck)
    {
        d->SetHomePosition(d->GetPosition());
        if (!d->IsInCombatWith(p))
            d->EngageWithTarget(p);
    }
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (!it.watch_aura)
            g_aggs.clear();
    }
    g.armor_before = double(d->GetArmor());
    g.dummy_hp_start = std::uint32_t(d->GetHealth());
    // Heroic Strike, Cleave, Maul replace the next white swing: a player is swinging, so the bot swings, and the swing comes at once.
    if (g.kind == Kind::SpellCheck && info && info->IsNextMeleeSwingSpell())
    {
        p->Attack(d, true);
        p->setAttackTimer(BASE_ATTACK, 0);
    }
    ::Playerbot::API api(p);
    ObjectGuid const cast_target = it.measure == Measure::Heal ? p->GetGUID() : d->GetGUID();
    ::Playerbot::Result const r = api.cast_spell(g.cast_spell_id, cast_target);
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
    // The spell check watches a cast-time spell to its end (SweepWait): whether the cast ran its full time is what tells a spell that
    // never lands (the second 0.1.30 run: Fireball, Frostbolt and Soul Fire, nothing recorded at all) from a cast that never finished.
    if (g.kind == Kind::SpellCheck)
    {
        Spell const* cur = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        g.cast_expected_ms = cur && cur->GetSpellInfo()->Id == g.cast_spell_id ? cur->GetCastTime() : 0;
        g.cast_begin_ms = GameTime::GetGameTimeMS();
        g.power_before = std::int64_t(p->GetPower(p->GetPowerType()));
    }
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
        // The player spell check measures the direct hit: a spell that also ticks (Fireball, Pyroblast) can have its ticks on the cast id
        // and its hit on another id, which the name search below then has to find.
        bool const want_hit = g.kind == Kind::SpellCheck && it.measure == Measure::Damage;
        if (want_hit && !a.N(Outcome::Hit))
        {
            std::string const cast_name = SpellNameOf(g.cast_spell_id);
            std::set<std::uint32_t> triggered;
            if (SpellInfo const* ci = sSpellMgr->GetSpellInfo(g.cast_spell_id, DIFFICULTY_NONE))
                for (SpellEffectInfo const& e : ci->GetEffects())
                    if (e.TriggerSpell)
                        triggered.insert(e.TriggerSpell);
            for (auto const& [key, agg] : g_aggs)
                if (key && !(key & kPetFlag) && key != g.cast_spell_id && agg.N(Outcome::Hit) && (triggered.count(key) || SpellNameOf(key) == cast_name))
                {
                    a = agg;
                    break;
                }
        }
        if (g.kind == Kind::SpellCheck)
        {
            g.check_seen.clear();
            for (auto const& [key, agg] : g_aggs)
            {
                std::string outs;
                for (std::size_t o = 0; o < kOutcomeCount; ++o)
                    if (agg.count[o])
                        outs += fmt::format("{}{}={}", outs.empty() ? "" : "/", ToString(Outcome(o)), agg.count[o]);
                g.check_seen += fmt::format("{}{}({}):{} dmg={} ticks={}", g.check_seen.empty() ? "" : " ", key & ~kPetFlag,
                                            key & ~kPetFlag ? SpellNameOf(key & ~kPetFlag) : std::string("white swing"), outs.empty() ? "none" : outs,
                                            agg.DirectDamage(), agg.ticks);
            }
            if (g.dummy_hp_start > d->GetHealth())
                g.dummy_lost += double(g.dummy_hp_start - std::uint32_t(d->GetHealth()));
            // A spell that hits an evading or immune target reports no outcome at all (Spell.cpp: no OnHitOutcome for immune, evade,
            // reflect or a zero damage roll): say which it was.
            SpellInfo const* si = sSpellMgr->GetSpellInfo(g.cast_spell_id, DIFFICULTY_NONE);
            g.dummy_state = fmt::format("dummy: evading={} immune_to_spell={} immune_to_damage={} in_combat={} reach={:.1f} dist={:.1f}",
                                        d->IsInEvadeMode() || d->HasUnitState(UNIT_STATE_EVADE), si ? d->IsImmunedToSpell(si, MAX_EFFECT_MASK, p) : false,
                                        si ? d->IsImmunedToDamage(p, si) : false, d->IsInCombat(), d->GetCombatReach(), p->GetExactDist(d));
        }
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
            if (g.kind == Kind::SpellCheck && a.N(Outcome::Hit))
                g.check_samples.push_back(a.AvgOf(Outcome::Hit));
            break;
        case Measure::DotTotal:
            if (a.ticks)
                g.samples.push_back(double(a.tick_damage));
            // A ground effect deals its damage as hits of its own id (Consecration: 1280349 hit six times for 224, no ticks): count both.
            if (g.kind == Kind::SpellCheck && a.ticks + a.Landed())
                g.check_samples.push_back(double(a.tick_damage + a.DirectDamage()));
            break;
        case Measure::AuraDuration:
        {
            SpellInfo const* info = sSpellMgr->GetSpellInfo(g.cast_spell_id, DIFFICULTY_NONE);
            Unit* holder = info && info->IsPositive() ? static_cast<Unit*>(p) : static_cast<Unit*>(d);
            if (Aura* aura = holder->GetAura(g.cast_spell_id, p->GetGUID()))
                g.samples.push_back(double(aura->GetMaxDuration()) / 1000.0);
            else if (Aura* any = holder->GetAura(g.cast_spell_id))
                g.samples.push_back(double(any->GetMaxDuration()) / 1000.0);
            else if (info)
                TC_LOG_INFO("playerbot.v2", "[dmg_parity] sweep class={} spell=\"{}\" id={} point={} left no aura on {}: immune to the spell={}", g.cls_name, it.spell, g.cast_spell_id,
                            it.points[g.point], holder->GetName(), holder->IsImmunedToSpell(info, MAX_EFFECT_MASK, p));
            break;
        }
        case Measure::Heal:
        {
            double const healed = double(p->GetHealth()) - double(g.health_before);
            if (healed > 0.0)
            {
                g.samples.push_back(healed);
                g.check_samples.push_back(healed);
            }
            break;
        }
        case Measure::ArmorDrop:
            g.samples.push_back(g.armor_before - double(d->GetArmor()));
            if (g.samples.back() == 0.0)
                if (SpellInfo const* armor_info = sSpellMgr->GetSpellInfo(g.cast_spell_id, DIFFICULTY_NONE))
                {
                    Aura const* armor_aura = d->GetAura(g.cast_spell_id);
                    AuraEffect const* armor_eff = armor_aura ? armor_aura->GetEffect(EFFECT_0) : nullptr;
                    TC_LOG_INFO("playerbot.v2", "[dmg_parity] sweep class={} spell=\"{}\" id={} point={} did not lower the armor: aura on the dummy={}, aura amount {}, immune to the spell={}, "
                                "armor now {}, bonus resistance {}, total aura armor {:.0f}", g.cls_name, it.spell, g.cast_spell_id, it.points[g.point], armor_aura != nullptr,
                                armor_eff ? armor_eff->GetAmount() : 0.0, d->IsImmunedToSpell(armor_info, MAX_EFFECT_MASK, p), d->GetArmor(), d->GetBonusResistanceMod(SPELL_SCHOOL_NORMAL),
                                d->GetTotalAuraModValue(UNIT_MOD_ARMOR));
                }
            break;
    }
}

// Finishes a sweep item: the series, its verdict, the failure when it does not scale.
// What the client's spell data says one cast of `info` should do on the dummy (or heal), before talents, crits and resists: per damage or
// heal effect, its value plus the attack power and spell power coefficients; weapon effects add the weapon's average hit; a physical hit
// goes through the dummy's armor. 0 when the spell has no direct damage or heal effect (a dot, a script, a trigger): then only the floor counts.
double SpellCheckDataValue(Player* p, Creature* d, SpellInfo const* info, bool heal)
{
    if (!info)
        return 0.0;
    bool const ranged = info->DmgClass == SPELL_DAMAGE_CLASS_RANGED;
    WeaponAttackType const att = ranged ? RANGED_ATTACK : BASE_ATTACK;
    double const ap = double(p->GetTotalAttackPowerValue(att));
    double const sp = heal ? double(p->SpellBaseHealingBonusDone(info->GetSchoolMask())) : double(p->SpellBaseDamageBonusDone(info->GetSchoolMask()));
    double const weapon = (double(p->GetWeaponDamageRange(att, MINDAMAGE)) + double(p->GetWeaponDamageRange(att, MAXDAMAGE))) / 2.0;
    double total = 0.0, pct = 0.0;
    bool any = false;
    for (SpellEffectInfo const& e : info->GetEffects())
    {
        switch (e.Effect)
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            case SPELL_EFFECT_HEAL:
                if ((e.Effect == SPELL_EFFECT_HEAL) != heal)
                    break;
                total += double(e.CalcValue(p, nullptr, heal ? static_cast<Unit*>(p) : static_cast<Unit*>(d))) + double(e.BonusCoefficientFromAP) * ap + double(e.BonusCoefficient) * sp;
                any = true;
                break;
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
                if (heal)
                    break;
                total += weapon + double(e.CalcValue(p, nullptr, d));
                any = true;
                break;
            case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
                if (heal)
                    break;
                pct = double(e.CalcValue(p, nullptr, d));
                any = true;
                break;
            default:
                break;
        }
    }
    if (!any)
        return 0.0;
    if (pct > 0.0)
        total = (total > 0.0 ? total : weapon) * pct / 100.0;
    if (!heal && (info->GetSchoolMask() & SPELL_SCHOOL_MASK_NORMAL) && total > 0.0)
        total = double(Unit::CalcArmorReducedDamage(p, d, std::uint32_t(total), info, att, std::uint8_t(p->GetLevel())));
    return total;
}

// One row of the player spell check: the Playerbot.log line and the table row.
void SpellCheckRow(SweepItem const& it, std::uint32_t id, char const* verdict, std::string const& detail)
{
    TC_LOG_INFO("playerbot.v2", "[spell_check] class={} spell=\"{}\" id={} verdict={} {}", g.cls_name, it.spell, id, verdict, detail);
    AppendFile(fmt::format("| {} | {} | {} | {} | {} |", g.cls_name, it.spell, id, verdict, detail));
    if (std::string_view(verdict) == "FAIL")
        g.check_failed += fmt::format("{}{} ({})", g.check_failed.empty() ? "" : ", ", it.spell, id);
}

void SpellCheckItemDone(Player* p, Creature* d)
{
    SweepItem const& it = g.items[g.item];
    SpellInfo const* info = sSpellMgr->GetSpellInfo(g.cast_spell_id, DIFFICULTY_NONE);
    double const data = it.measure == Measure::DotTotal ? 0.0 : SpellCheckDataValue(p, d, info, it.measure == Measure::Heal);
    std::vector<double> const& got = g.check_samples;
    double const mean = got.empty() ? 0.0 : Mean(got);
    double lo = 0.0, hi = 0.0;
    for (double v : got)
    {
        lo = lo == 0.0 ? v : std::min(lo, v);
        hi = std::max(hi, v);
    }
    std::string why;
    if (!g.cast_ok)
        why = fmt::format("never cast: {}", g.last_fail.empty() ? "refused" : g.last_fail);
    else if (got.empty() || mean <= 0.0)
        why = it.measure == Measure::Heal ? "cast but healed nothing" : "cast but no plain hit landed (all missed, crit-only or no damage)";
    else if (mean < it.floor)
        why = fmt::format("too low: mean {:.0f} under the floor {:.0f}", mean, it.floor);
    else if (data >= 20.0 && mean < data * 0.5)
        why = fmt::format("too low: mean {:.0f} under half of the spell data's {:.0f}", mean, data);
    if (info && info->IsNextMeleeSwingSpell())
        p->AttackStop();
    if (!why.empty() && g.cast_ok)
        why += fmt::format("; dummy lost {:.0f} health over the casts; last cast did: {}{}", g.dummy_lost, g.check_seen.empty() ? "nothing recorded" : g.check_seen,
                           g.cast_failures ? "; last refusal: " + g.last_fail : std::string());
    if (!why.empty() && g.casts_completed + g.casts_cut)
        why += fmt::format("; cast-time casts: {} ran their full time, {} ended early; caster lost {} health while casting; cost spent over the casts {}",
                           g.casts_completed, g.casts_cut, g.caster_hp_lost, g.power_spent);
    if (!why.empty() && !g.dummy_state.empty())
        why += fmt::format("; {}; dummy seen evading on {} ticks", g.dummy_state, g.evade_ticks);
    if (g.far)
        why = why.empty() ? fmt::format("lands from 15 yd but nothing at all from 3 yd ({})", g.near_result)
                          : fmt::format("from 15 yd: {} (from 3 yd: {})", why, g.near_result);
    std::string const detail = fmt::format("casts={} refused={} mean={:.0f} min={:.0f} max={:.0f} floor={:.0f} data_value={:.0f} ap={:.0f} sp={}{}{}",
        g.cast_ok, g.cast_failures, mean, lo, hi, it.floor, data,
        double(p->GetTotalAttackPowerValue(info && info->DmgClass == SPELL_DAMAGE_CLASS_RANGED ? RANGED_ATTACK : BASE_ATTACK)),
        info ? p->SpellBaseDamageBonusDone(info->GetSchoolMask()) : 0,
        it.pre_aura ? fmt::format(" aura_{}_opens_it={}", it.pre_aura, g.state_from_aura) : std::string(),
        why.empty() ? std::string() : " reason=\"" + why + "\"");
    if (!why.empty())
    {
        ++g.check_fail;
        SpellCheckRow(it, g.cast_spell_id, "FAIL", detail);
        g.failure += fmt::format("{} {} ({}): {}; ", g.cls_name, it.spell, g.cast_spell_id, why);
    }
    else
    {
        ++g.check_pass;
        SpellCheckRow(it, g.cast_spell_id, "PASS", detail);
        if (data >= 20.0 && mean > data * 3.0)
            TC_LOG_INFO("playerbot.v2", "[spell_check] class={} spell=\"{}\" id={} WARN mean {:.0f} is over three times the spell data's {:.0f}", g.cls_name, it.spell, g.cast_spell_id, mean, data);
    }
    if (it.pre_aura && !g.state_from_aura)
    {
        // The damage can be right while a player still cannot press the button: say so as its own failure.
        ++g.check_fail;
        SpellCheckRow(it, g.cast_spell_id, "FAIL", fmt::format("aura {} does not make it usable (caster aura state {} not set by the aura)", it.pre_aura, info ? info->CasterAuraState : 0));
        g.failure += fmt::format("{} {}: aura {} does not make it usable; ", g.cls_name, it.spell, it.pre_aura);
    }
    if (it.pre_aura)
        p->RemoveAurasDueToSpell(it.pre_aura);
    if (info && info->CasterAuraState)
        p->ModifyAuraState(AuraStateType(info->CasterAuraState), false);
}

// A damage spell that was cast but left nothing at all from 3 yd (no hit, no miss, no health lost) is cast again from 15 yd once.
bool SpellCheckWantsFarRetry(Player* p, Creature* /*d*/)
{
    SweepItem const& it = g.items[g.item];
    if (g.far || it.measure != Measure::Damage || !g.cast_ok || !g.check_samples.empty() || g.dummy_lost > 0.0)
        return false;
    SpellInfo const* info = sSpellMgr->GetSpellInfo(g.cast_spell_id, DIFFICULTY_NONE);
    if (!info || info->GetMaxRange(false, p) < 16.0f)   // a melee spell cannot be cast from 15 yd
        return false;
    g.near_result = fmt::format("{} casts, {} ran their full time, cost spent {}, last cast did: {}; {}", g.cast_ok, g.casts_completed, g.power_spent,
                                g.check_seen.empty() ? "nothing recorded" : g.check_seen, g.dummy_state);
    TC_LOG_INFO("playerbot.v2", "[spell_check] class={} spell=\"{}\" id={} nothing landed from 3 yd ({}); casting again from 15 yd", g.cls_name, it.spell,
                g.cast_spell_id, g.near_result);
    (void)p;
    return true;
}

// One spell of the player spell check may take 90 s at most: then it is a FAIL of its own and the class goes on with the next spell.
// True when it ran out (the caller returns; the step is the next item's setup).
bool SpellCheckStuck(Player* p)
{
    if (!g.item_started_ms || GameTime::GetGameTimeMS() - g.item_started_ms < 90u * 1000u)
        return false;
    SweepItem const& it = g.items[g.item];
    p->InterruptNonMeleeSpells(true);
    p->AttackStop();
    ++g.check_fail;
    SpellCheckRow(it, g.cast_spell_id, "FAIL", fmt::format("casts={} refused={} reason=\"stuck for 90 s (casting state {}, last refusal: {})\"", g.cast_ok, g.cast_failures,
                  p->HasUnitState(UNIT_STATE_CASTING), g.last_fail.empty() ? "none" : g.last_fail));
    g.failure += fmt::format("{} {}: stuck; ", g.cls_name, it.spell);
    g_measuring = false;
    g.item_started_ms = 0;
    ++g.item;
    g.point = 0;
    g.step = Step::SweepSetup;
    g.step_s = 0;
    return true;
}

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
// stat system recomputes the damage fields from the template, so the update runs after the set). `want` > 0 makes the middle of the effective
// range, with the creature's own damage modifiers, that value.
void ArmMob(Creature* mob, float want)
{
    mob->SetFaction(14);   // monster faction, hostile to every player
    mob->SetReactState(REACT_AGGRESSIVE);
    if (want > 0.f)
    {
        mob->SetBaseAttackTime(BASE_ATTACK, 2000);
        mob->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, want);
        mob->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, want);
        // A creature with a vanilla melee row (creature_classic_melee) takes its hit from that row, not from the weapon damage set above
        // (Creature::CalculateMinMaxDamage): the Unkillable Test Dummy has one, and the support scorecard's 3500 swing landed for about 15 on 230
        // (477 damage on the tank in 120 s). The flat damage modifier counts on both paths and linearly, so it is set to move the middle of the
        // range onto `want`: its slope is read off the creature itself.
        auto const mid = [mob]
        {
            float lo = 0.f, hi = 0.f;
            mob->CalculateMinMaxDamage(BASE_ATTACK, false, true, lo, hi);
            return 0.5f * (lo + hi);
        };
        float const f0 = mob->GetFlatModifierValue(UNIT_MOD_DAMAGE_MAINHAND, BASE_VALUE);
        float const m0 = mid();
        mob->SetStatFlatModifier(UNIT_MOD_DAMAGE_MAINHAND, BASE_VALUE, f0 + 100.f);
        float const slope = (mid() - m0) / 100.f;
        mob->SetStatFlatModifier(UNIT_MOD_DAMAGE_MAINHAND, BASE_VALUE, slope > 0.f ? f0 + (want - m0) / slope : f0);
        float lo = 0.f, hi = 0.f;
        mob->CalculateMinMaxDamage(BASE_ATTACK, false, true, lo, hi);
        TC_LOG_INFO("playerbot.v2", "[incoming] {} (entry {}) armed: wanted {:.0f}, range before {:.0f} (middle), slope {:.2f}, now {:.0f}-{:.0f}, vanilla melee row {}",
                    mob->GetName(), mob->GetEntry(), want, m0, slope, lo, hi, sObjectMgr->GetCreatureClassicMelee(mob->GetEntry()) ? "yes" : "no");
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

// ---- Group scorecard (Kind::Group) ----
// A raid of one bot per dps spec (plus a tank and the buffers) on one boss dummy at the same time: the bots keep their own logic, so the raid
// buffs, the shared debuffs and the consumables come from the raid itself. The wait for buffs, the warm-up and the measurement run one second at
// a time; the result is one row per bot in DpsScorecard.tsv and one [dmg_group] line each in the module log.

struct GroupSub
{
    ObjectGuid guid;
    std::string name, label;
    std::uint8_t cls = 0;
    std::uint32_t spec = 0;
    bool melee = false, scored = true;
    std::uint32_t casting_s = 0, attacking_s = 0, idle_s = 0, dead_s = 0, mana_refills = 0, replaced = 0;
    std::uint32_t buff_count = 0;
    std::string buffs;
    // The readiness audit: what is wrong with the bot, by category (empty = nothing), and the seconds a ranged bot spent in melee reach / a melee bot out of it.
    std::string gap_ranks, gap_talents, gap_gear, gap_consumables, gap_rotation, gap_position;
    std::uint32_t ranged_close_s = 0, melee_far_s = 0;
    float closest = -1.0f;   // the nearest the bot stood to the dummy in the measured seconds (edge to edge, as a spell's range is checked), -1 = no sample
    std::uint32_t rage_samples = 0, rage_sum = 0, rage_ge30_s = 0, rage_ge50_s = 0, rage_cap_s = 0;   // rage users only, one sample a measured second
    std::uint32_t trace_s = 0;          // seconds left of the per-second trace after a hunter's placement
    std::uint8_t role = 0;              // 0 dps, 1 healer, 2 tank
    bool has_mana = false;              // support scorecard: the mana the bot started with and held lowest, percent, and the seconds under 5 %
    std::uint32_t mana_start = 0, mana_min = 100, mana_end = 0, oom_s = 0;
    float threat_start = 0.f, threat_end = 0.f;   // support scorecard: its threat on the boss when the measurement began and now
    float peak_threat_pct = 0.f;        // support scorecard, a dps: its highest threat on the boss in percent of the tank's, one sample a measured second
    std::uint32_t pull_threat_s = 0;    // and the seconds it held enough to take the boss from the tank (kAggroSwitchRatio*)
    std::uint32_t last_replace_s = 0;   // measure second of the last placement: a bot is put back at most every 10 s, or at once when it is 40 y away
    Position last_spot;                 // the ring point of the last placement (has_last_spot), and the ring points the bot stood on without seeing the dummy
    bool has_last_spot = false;
    std::vector<Position> blind_spots;
};

enum class GroupPhase : std::uint8_t { Place, Buff, Warm, Measure };

struct GroupRun
{
    std::vector<GroupSub> subs;
    GroupPhase phase = GroupPhase::Place;
    std::uint32_t t = 0;             // seconds in the phase
    std::uint32_t measure_s = 0;
    std::uint32_t leg_s = 120;
    std::uint32_t buff_s = 75;
    ObjectGuid dummy;
    std::string dummy_name;
    bool has_site = false;           // the dummy was sent to a flat site and is held there (rooted, put back if it drifts more than 1 y)
    Position site;
    std::uint32_t runs = 1;          // Playerbot.DamageParity.Runs: how many times the fight is measured (the buffs are applied once)
    std::uint32_t run = 1;           // the run under way, from 1
    std::vector<std::vector<double>> run_dps;   // per bot (same order as subs), the dps of each finished run (the support scorecard: its value)
    // Support scorecard, per bot and run: effective hps, overheal %, mana at the end %, seconds under 5 % mana, threat a second, damage taken a
    // second, avoided % of the boss's swings (-1 where it does not apply: no mana, no swings at it).
    std::vector<std::vector<std::array<double, 7>>> run_support;
    std::string baseline_path;       // Playerbot.DamageParity.Baseline; empty = no comparison
    // The support scorecard (healers and tanks): the boss hits the tank with swings of `swing` and every `pulse_s` seconds shoots `pulse_dmg` at
    // `pulse_n` raiders in turn, and the healers have to keep the raid up.
    bool support = false;
    float swing = 3500.f, pulse_dmg = 1200.f;
    std::uint32_t pulse_n = 6, pulse_s = 6, pulse_i = 0;
    std::size_t tank = 0;            // index of the tank in subs
    std::uint32_t boss_on_tank_s = 0, danger_s = 0, run_fails = 0;
    float lowest_hp = 100.f;
};
GroupRun gr;

constexpr float kGroupMeleeRadius = 4.0f;
constexpr float kGroupRangedRadius = 24.0f;
constexpr float kSupportRangedArc = 1.5707963f;   // a quarter of the ring: a 34 y chord at 24 y, inside a heal's 40 y (GroupPlaceOne)

void ZeroResistances(Creature* d)
{
    for (std::uint32_t school = SPELL_SCHOOL_HOLY; school < MAX_SPELL_SCHOOL; ++school)
    {
        d->SetStatFlatModifier(UnitMods(UNIT_MOD_ARMOR + school), BASE_VALUE, 0.0f);
        d->UpdateResistances(school);
    }
}

// The bot's positive auras: how many, and the first few by name (the raid buffs, flask, elixirs and food it carries into the fight).
void BuffSummary(Player* p, std::uint32_t& count, std::string& names)
{
    count = 0;
    names.clear();
    for (auto const& kv : p->GetAppliedAuras())
    {
        AuraApplication const* app = kv.second;
        if (!app || !app->IsPositive())
            continue;
        SpellInfo const* info = app->GetBase()->GetSpellInfo();
        if (!info || !info->SpellName)
            continue;
        ++count;
        if (count <= 60)
        {
            if (!names.empty())
                names += ", ";
            names += (*info->SpellName)[sWorld->GetDefaultDbcLocale()];
        }
    }
}

// Stands one raider at its place around the dummy: a melee fighter at the dummy's feet, everyone else at casting range, on the same ring slot.
// The role's whole kit (flask, elixirs, food, the mana potion and runes) into the bags of a raider, as a raid night arrives with it. API::stock_raid_consumable
// refuses outside a raid instance, and the scorecard stands on open ground: its 75 s of buffing found three Greater Arcane Elixirs among the casters and no
// Flask of Supreme Power (209: fire mage SP 646 against 800+ of a raid mage). The bots' own rules drink what the bags hold.
void StockGroupKit(Player* p)
{
    auto const snap = ::Playerbot::Services::Snapshots().latest(p->GetGUID().GetCounter());
    auto const gsnap = ::Playerbot::Services::Snapshots().latest_group(p->GetGUID().GetCounter());
    if (!snap || !gsnap)
        return;
    Classic::RaidConsumableRole const role = Classic::RaidConsumableRoleOf(BotSnapshotView(*snap), GroupSnapshotView(*gsnap));
    std::uint32_t stocked = 0;
    for (Classic::RaidConsumable const& c : Classic::RaidConsumablesFor(role))
    {
        ItemTemplate const* tpl = sObjectMgr->GetItemTemplate(c.item);
        if (!tpl || p->CanUseItem(tpl) != EQUIP_ERR_OK)
            continue;
        if (std::uint32_t const held = p->GetItemCount(c.item); held < c.stock && p->StoreNewItemInBestSlots(c.item, c.stock - held, ItemContext::NONE))
            ++stocked;
    }
    TC_LOG_INFO("playerbot.v2", "[dmg_group] {} kit: {} items stocked", p->GetName(), stocked);
}

// The bot's own always-on kit (Playerbot.AlwaysBuffs) renewed, with the set and kinds its idle:always_buff rule gives it. The runs of a scorecard follow
// each other in combat, so that idle rule does not run between them, and an hour-long elixir that ran out in one run stayed off for the rest (245 support
// druid runs 2-3: the fury's Mongoose, put on at 12:52 and carried since, ran out in run 1; "no flask or elixir of the kit active" in runs 2 and 3).
void RenewOwnKit(Player* p)
{
    auto const snap = ::Playerbot::Services::Snapshots().latest(p->GetGUID().GetCounter());
    auto const gsnap = ::Playerbot::Services::Snapshots().latest_group(p->GetGUID().GetCounter());
    if (!snap || !gsnap)
        return;
    BotSnapshotView const s(*snap);
    GroupSnapshotView const g(*gsnap);
    ::Playerbot::API(p).always_buffs(uint8(AlwaysBuffSetOf(s, g)), AlwaysMask(s, g));
}

// A flat, open site for the dummy: the staging spot is wherever the first bot stood (Stormwind, (-8948,494,80): a ledge, 46 ring points off its level and
// 50 without line of sight), so every scorecard fought on a different patch of ground. Round the bot, centres 0 to 160 y out in 20 y steps and 16 bearings,
// each scored by the ring points (16 each at 10, 16 and 24 y) with ground within 2.5 of the centre (the rule GroupFindSpot uses) and a line of sight to it.
// The first centre with 44 of 48 wins; failing that the best one with at least 16 (the all-or-nothing version found none among 36 centres in Stormwind, 227).
bool FindFlatDummySite(Player* origin, Position& out, std::string& log)
{
    Map* map = origin->GetMap();
    if (!map)
        return false;
    PhaseShift const& ps = origin->GetPhaseShift();
    auto const ground = [&](float x, float y, float z) { return map->GetHeight(ps, x, y, z + 2.0f, true, 10.0f); };
    unsigned tried = 0, best_score = 0;
    Position best;
    for (int ring = 0; ring <= 8; ++ring)
        for (int k = 0; k < (ring ? 16 : 1); ++k)
        {
            float const a = 0.3927f * float(k);
            float const cx = origin->GetPositionX() + std::cos(a) * 20.0f * float(ring), cy = origin->GetPositionY() + std::sin(a) * 20.0f * float(ring);
            float const cz = ground(cx, cy, origin->GetPositionZ());
            if (cz <= INVALID_HEIGHT + 1.0f || std::fabs(cz - origin->GetPositionZ()) > 12.0f)
                continue;
            ++tried;
            unsigned score = 0;
            for (float const radius : { 10.0f, 16.0f, 24.0f })
                for (int i = 0; i < 16; ++i)
                {
                    float const x = cx + std::cos(0.3927f * float(i)) * radius, y = cy + std::sin(0.3927f * float(i)) * radius;
                    float const z = ground(x, y, cz);
                    if (z > INVALID_HEIGHT + 1.0f && std::fabs(z - cz) <= 2.5f &&
                        map->isInLineOfSight(ps, x, y, z + 2.0f, cx, cy, cz + 2.0f, LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing))
                        ++score;
                }
            if (score > best_score)
            {
                best_score = score;
                best = Position(cx, cy, cz, 0.0f);
            }
            if (score >= 44)
                goto done;
        }
done:
    if (best_score < 16)
    {
        log = fmt::format("no usable site among {} centres within 160 y (best {} of 48 ring points)", tried, best_score);
        return false;
    }
    out = best;
    log = fmt::format("site with {} of 48 ring points on its level in sight, best of {} centres", best_score, tried);
    return true;
}

// A place on the ring round the dummy that stands on its level and sees it: the ring is walked from the slot's angle outwards in 7.5 degree steps (219: with 15 degrees six raiders, Survival among them, found no point where the dummy stood in a hollow, and the next slot's lattice did) and, if
// nothing on it sees the dummy, at shorter radii (a hunter keeps 13 y, its Auto Shot needs 11). The first scorecard in Stormwind put casters and hunters
// on ring points behind walls: no line of sight, no cast, and a teleport back onto the same point every second.
// A point within 4 y of one of `avoid` is skipped: the bot was put there and did not see the dummy (243: the elemental, put back 9 times onto
// (-8941.0,555.7,77.1), whose height is 2 y above the dummy's floor and whose check from 2 y above that saw over what stood in the way; the bot
// stood at 75.1, 4 y off it, with no line of sight, cast nothing for the 3 runs and scored 0).
// `see`: places the point must have a line of sight to as well (the support scorecard's healers).
bool GroupFindSpot(Creature* d, bool melee, float angle0, Position& out, float* radius_out = nullptr, std::vector<Position> const* avoid = nullptr,
                   std::vector<Position> const* see = nullptr)
{
    Map* map = d->GetMap();
    if (!map)
        return false;
    PhaseShift const& ps = d->GetPhaseShift();
    float const dx = d->GetPositionX(), dy = d->GetPositionY(), dz = d->GetPositionZ();
    // The last radii are inside the 10 y a ranged bot is put back from: a caster that stands 6 y from the boss and casts beats one that stands on a
    // ring point without line of sight and casts nothing (219: the elemental 36 dps, the heal priest 91, the paladin 0, casting 4 s of 120).
    static constexpr float kRangedRadii[] = { kGroupRangedRadius, 20.0f, 16.0f, 13.0f, 10.0f, 7.0f };
    static constexpr float kMeleeRadii[] = { kGroupMeleeRadius, 3.0f };
    unsigned off_level = 0, no_ground = 0, no_sight = 0;
    for (float const radius : melee ? std::span<float const>(kMeleeRadii) : std::span<float const>(kRangedRadii))
        for (int k = 0; k < 48; ++k)
        {
            float const angle = angle0 + float((k + 1) / 2) * 0.1309f * (k % 2 ? 1.0f : -1.0f);
            float const x = dx + std::cos(angle) * radius, y = dy + std::sin(angle) * radius;
            if (avoid && std::any_of(avoid->begin(), avoid->end(), [&](Position const& a) { return a.GetExactDist2dSq(x, y) < 4.0f * 4.0f; }))
                continue;
            float const z = map->GetHeight(ps, x, y, dz + 2.0f, true, 6.0f);
            if (z <= INVALID_HEIGHT + 1.0f)
            {
                ++no_ground;
                continue;
            }
            if (std::fabs(z - dz) > 2.5f)
            {
                ++off_level;
                continue;
            }
            if (!map->isInLineOfSight(ps, x, y, z + 2.0f, dx, dy, dz + 2.0f, LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing))
            {
                ++no_sight;
                continue;
            }
            if (see && std::any_of(see->begin(), see->end(), [&](Position const& h) {
                    return !map->isInLineOfSight(ps, x, y, z + 2.0f, h.GetPositionX(), h.GetPositionY(), h.GetPositionZ() + 2.0f, LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing); }))
            {
                ++no_sight;
                continue;
            }
            out = Position(x, y, z, Position::NormalizeOrientation(std::atan2(dy - y, dx - x)));
            if (radius_out)
                *radius_out = radius;
            return true;
        }
    TC_LOG_INFO("playerbot.v2", "[dmg_group] no ring point found round the dummy at ({:.1f},{:.1f},{:.1f}) evading {} in combat {}: {} points without ground, {} off its level, {} without line of sight",
                dx, dy, dz, int(d->IsInEvadeMode()), int(d->IsInCombat()), no_ground, off_level, no_sight);
    return false;
}

void GroupPlaceOne(Player* p, Creature* d, GroupSub& s, std::size_t slot, std::size_t slots, bool log_target = false)
{
    float angle = 6.2831853f * float(slot) / float(std::max<std::size_t>(slots, 1));
    if (s.melee)
    {
        // Melee stand behind the dummy, the tank (protection warrior) in front of it, as at a boss: from the front the dummy parries and blocks white
        // hits (group 20: the fury landed 71% of its swings against 83% and scored 927 against 1100-1200, in the two groups where the dummy faced it,
        // 17 and 20; the dummy's position and facing differ every group). The slots spread over at most 160 degrees behind it.
        float const o = d->GetOrientation();
        if (s.spec == 73)
            angle = o;
        else
        {
            float const step = slots > 1 ? std::min(0.35f, 2.8f / float(slots - 1)) : 0.0f;
            angle = o + 3.1415927f + (float(slot) - float(slots - 1) / 2.0f) * step;
        }
    }
    else if (gr.support && slots > 1)
    {
        // The support scorecard's healers must reach every raider: spread round the whole 24 y ring, raiders on opposite sides stood 48 y
        // apart, past the 40 y of a heal (244 druid run 3: the healers on the ring's north side, the fire mage 46-48 y and the warlock
        // 50 y from all five; neither got a heal and both died, 2 died in 243's druid run 3 as well). The ranged stand on a quarter of the
        // ring, at most 34 y apart, on the tank's side: the bots' own raid formation (combat:raid_formation, "stops 22y short of tank") takes
        // them there. Behind the dummy with the protection warrior in front, every healer walked to its formation spot through the dummy and was
        // put back (245 warrior runs: 30 put-backs against 2 in the paladin and druid runs, where the tank stands behind it with the melee;
        // the shaman flagged "ranged bot in melee reach 5 s" in run 2).
        bool const tank_in_front = gr.tank < gr.subs.size() && gr.subs[gr.tank].spec == 73;
        float const step = kSupportRangedArc / float(slots - 1);
        angle = d->GetOrientation() + (tank_in_front ? 0.0f : 3.1415927f) + (float(slot) - float(slots - 1) / 2.0f) * step;
    }
    // A ranged raider takes no other ranged raider's place (4 y) and, in the support scorecard, stands where it sees every healer already placed.
    // 246 paladin runs 1-2: the fire mage and the warlock were put on one point that saw the dummy but no healer; neither was picked for a heal
    // until 10 % (the triage leaves a member out of sight above 35 %), then five healers cast at it for 12 s and none landed. Both died in both
    // runs (48 and 84 s, 6 and 54 s dead).
    std::vector<Position> avoid = s.blind_spots, healers;
    if (!s.melee)
        for (GroupSub const& o : gr.subs)
            if (&o != &s && o.has_last_spot && !o.melee)
            {
                avoid.push_back(o.last_spot);
                if (gr.support && o.role == 1)
                    healers.push_back(o.last_spot);
            }
    Position pos;
    float radius = 0.0f;
    bool found = GroupFindSpot(d, s.melee, angle, pos, &radius, &avoid, healers.empty() ? nullptr : &healers);
    if (!found && !healers.empty())
    {
        TC_LOG_INFO("playerbot.v2", "[dmg_group] {}: no ring point sees the dummy and all {} healers, placed by the dummy alone", s.name, healers.size());
        found = GroupFindSpot(d, s.melee, angle, pos, &radius, &avoid);
    }
    s.last_spot = pos;
    s.has_last_spot = found;
    if (log_target)
        TC_LOG_INFO("playerbot.v2", "[dmg_group] {} put back to ({:.1f},{:.1f},{:.1f}) {} radius {:.0f} from the bot's ({:.1f},{:.1f},{:.1f}), dummy at ({:.1f},{:.1f},{:.1f}) ground under it {:.1f}", s.name,
                    pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), found ? "on the ring" : "(no ring point: melee fallback below)", radius, p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(),
                    d->GetPositionX(), d->GetPositionY(), d->GetPositionZ(), d->GetMap()->GetHeight(d->GetPhaseShift(), d->GetPositionX(), d->GetPositionY(), d->GetPositionZ() + 2.0f, true, 6.0f));
    if (!found)
    {
        // Neither ring has a point that sees the dummy: next to the melee, where the tank sees it and swings, rather than on a ring point behind a wall
        // where a caster casts nothing for the whole window.
        TC_LOG_INFO("playerbot.v2", "[dmg_group] {}: no place with a line of sight to the dummy on its ring, stands at the melee ring point", s.name);
        pos = d->GetNearPosition(kGroupMeleeRadius, angle);
        pos.SetOrientation(pos.GetAbsoluteAngle(d->GetPositionX(), d->GetPositionY()));
    }
    p->CombatStop(true);
    p->NearTeleportTo(pos);
    // The chase of an earlier order goes with the old spot: a hunter that closed in on the dummy to melee it stayed there (the first scorecard: white
    // swing and Raptor Strike were all three hunters' damage), as the solo legs clear it at each staging.
    p->GetMotionMaster()->Clear();
    p->StopMoving();
    p->RemoveUnitMovementFlag(MOVEMENTFLAG_MASK_MOVING);
    p->ClearUnitState(UNIT_STATE_MOVING);
    p->SetFacingToObject(d);
}

void GroupPlaceAll(Creature* d)
{
    std::size_t melee_slot = 0, ranged_slot = 0, melee_n = 0, ranged_n = 0;
    for (GroupSub& s : gr.subs)
    {
        (s.melee ? melee_n : ranged_n)++;
        s.has_last_spot = false;   // everyone is placed anew: the places of the run before are not kept clear or looked at (GroupPlaceOne)
    }
    for (GroupSub& s : gr.subs)
        if (Player* p = ObjectAccessor::FindConnectedPlayer(s.guid); p && p->IsInWorld())
        {
            if (!p->IsAlive())
            {
                p->ResurrectPlayer(1.0f);
                p->SpawnCorpseBones();
            }
            GroupPlaceOne(p, d, s, s.melee ? melee_slot++ : ranged_slot++, s.melee ? melee_n : ranged_n);
        }
}

// What the solo t3_buffed leg gives its subject, once for the raid: the raid buffs and consumables of each dps bot (SetRaidBuffs, which the bots' own
// logic does not bring: the first scorecard's fury had no Mongoose, Juju or Windfury, its fire mage no flask), and on the dummy five Sunder Armors, the
// armor shred of a melee raid, Curse of the Elements and Shadow and the debuffs the raid's talents put on a boss. The scorecard measures against the
// same ranges as that leg, so it stands in the same buffs.
void GroupRaidBuffs()
{
    for (GroupSub const& s : gr.subs)   // the tank and the healers stand in the raid buffs too
        if (Player* p = ObjectAccessor::FindConnectedPlayer(s.guid); p && p->IsAlive())
            SetRaidBuffs(p, true, /*group*/ true, /*consumables*/ !gr.support);
}

void GroupRaidDebuffs(Creature* d)
{
    Player* melee = nullptr;     // the source of the armor shred and the Sunders: a non-caster (the tank)
    Player* any = nullptr;
    for (GroupSub const& s : gr.subs)
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(s.guid);
        if (!p || !p->IsAlive())
            continue;
        any = any ? any : p;
        if (!melee && !IsCasterBot(p))
            melee = p;
    }
    if (!any)
        return;
    if (!melee)
        melee = any;
    SetSunders(melee, d, kSunderStacks);
    SetArmorShred(melee, d, true);

    auto add = [&](Player* source, std::uint32_t id, int stacks)
    {
        if (Aura* aura = source->AddAura(id, d))
        {
            aura->SetStackAmount(std::uint8_t(stacks));
            aura->SetMaxDuration(-1);
            aura->SetDuration(-1);
        }
        else
            TC_LOG_ERROR("playerbot.v2", "[dmg_group] raid debuff {} could not be applied to the dummy", id);
    };
    for (std::uint32_t const id : kCasterDebuffSpells)
    {
        d->RemoveAurasDueToSpell(id);
        add(any, id, 1);
    }
    for (TalentDebuff const& t : kTalentDebuffs)
    {
        d->RemoveAurasDueToSpell(t.debuff);
        for (GroupSub const& s : gr.subs)
            if (Player* p = ObjectAccessor::FindConnectedPlayer(s.guid); p && p->IsAlive() && (p->HasAura(t.talent) || (t.debuff == 15258 && p->GetClass() == CLASS_WARLOCK)))
            {
                add(p, t.debuff, kDebuffStacks);
                break;
            }
    }
    std::string names;
    for (auto const& [slot, app] : d->GetAppliedAuras())
    {
        Aura const* a = app->GetBase();
        names += fmt::format("{}{}x{}", names.empty() ? "" : ", ", SpellNameOf(a->GetId()), a->GetStackAmount());
    }
    TC_LOG_INFO("playerbot.v2", "[dmg_group] raid debuffs on the dummy: armor {}, {} auras: {}", d->GetArmor(), d->GetAppliedAuras().size(), names.empty() ? "-" : names);
}

// ---- Readiness audit (Kind::Group) ----
// Before the measured phase each bot is checked against what the client data and the role kit say it should have, and after it against what it did:
// every gap is a flag, and a bot with any flag is FAIL in the scorecard whatever its dps. One list per category, so a row says what to fix.

std::string JoinGaps(std::vector<std::string> const& gaps, std::size_t cap = 8)
{
    std::string out;
    for (std::size_t i = 0; i < gaps.size() && i < cap; ++i)
        out += (i ? "; " : "") + gaps[i];
    if (gaps.size() > cap)
        out += fmt::format("; +{} more", gaps.size() - cap);
    for (char& c : out)
        if (c == '\t' || c == '\n' || c == '\r')
            c = ' ';
    return out;
}

std::string LowerName(std::string s)
{
    for (char& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool PlayerHasAuraNamed(Player* p, std::string const& name)
{
    std::string const want = LowerName(name);
    for (auto const& kv : p->GetAppliedAuras())
        if (kv.second && kv.second->GetBase())
            if (SpellInfo const* info = kv.second->GetBase()->GetSpellInfo(); info && info->SpellName && LowerName((*info->SpellName)[sWorld->GetDefaultDbcLocale()]) == want)
                return true;
    return false;
}

// (1) After the fight: a spell the bot cast at a lower rank than the data has for its level (a Forever-only spell its spec never casts is not listed:
// only what the rotation really used is judged).
void AuditRanks(Player* p, std::set<std::uint32_t> const& cast_ids, std::vector<std::string>& gaps)
{
    std::set<std::uint32_t> chains;
    for (std::uint32_t const id : cast_ids)
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
        if (!info || info->IsPassive() || !info->GetRank())
            continue;
        std::uint32_t best = id;
        for (std::uint32_t next = sSpellMgr->GetNextSpellInChain(best); next; next = sSpellMgr->GetNextSpellInChain(next))
        {
            SpellInfo const* ni = sSpellMgr->GetSpellInfo(next, DIFFICULTY_NONE);
            if (!ni || ni->SpellLevel == 0 || ni->SpellLevel > p->GetLevel() || !p->IsSpellFitByClassAndRace(next) || !SpellMgr::IsSpellValid(ni, p, false))
                break;
            best = next;
        }
        if (best != id && chains.insert(sSpellMgr->GetFirstSpellInChain(id)).second)
            gaps.push_back(fmt::format("{} cast at rank {}, rank {} is {}", SpellNameOf(id), info->GetRank(), sSpellMgr->GetSpellInfo(best, DIFFICULTY_NONE)->GetRank(),
                                       p->HasSpell(best) ? "known" : "not learned"));
    }
}

// (2) The build of the bot's spec, pick by pick up to the points of its level (the walk apply_classic_talents makes), against the ranks it holds.
void AuditTalents(Player* p, std::uint32_t spec, std::vector<std::string>& gaps)
{
    auto const build = ::Playerbot::Classic::GetTalentBuild(p->GetClass(), spec);
    if (build.empty())
    {
        gaps.push_back(fmt::format("no talent build for spec {}", spec));
        return;
    }
    UF::TraitConfig const* cfg = p->GetTraitConfig(p->m_activePlayerData->ActiveCombatTraitConfigID);
    if (!cfg)
    {
        gaps.push_back("no active trait config");
        return;
    }
    int const points = std::max(0, int(p->GetLevel()) - 9);
    int placed = 0;
    for (::Playerbot::Classic::TalentPick const& pick : build)
    {
        if (placed >= points)
            break;
        if (pick.filler)
            continue;
        placed += pick.ranks;
        ::Playerbot::Classic::TalentNode const* node = ::Playerbot::Classic::FindTalent(p, pick.name);
        if (!node)
        {
            gaps.push_back(fmt::format("{}: the class trees have no such node", pick.name));
            continue;
        }
        int rank = 0;
        for (UF::TraitEntry const& e : cfg->Entries)
            if (e.TraitNodeID == node->TraitNodeID && e.TraitNodeEntryID == node->TraitNodeEntryID)
                rank = e.Rank + e.GrantedRanks;
        int const want = std::min<int>(pick.ranks, node->MaxRanks);
        if (rank < want)
            gaps.push_back(fmt::format("{} {}/{}", pick.name, rank, want));
    }
}

// (3) Empty slots, items without their permanent enchant, and a hunter's ammo.
void AuditGear(Player* p, GroupSub const& s, std::vector<std::string>& gaps)
{
    struct SlotName { std::uint8_t slot; char const* name; bool enchant; };
    static constexpr SlotName kSlots[] = {
        { EQUIPMENT_SLOT_HEAD, "head", true }, { EQUIPMENT_SLOT_NECK, "neck", false }, { EQUIPMENT_SLOT_SHOULDERS, "shoulders", true },
        { EQUIPMENT_SLOT_BACK, "back", true }, { EQUIPMENT_SLOT_CHEST, "chest", true }, { EQUIPMENT_SLOT_WRISTS, "wrists", true },
        { EQUIPMENT_SLOT_HANDS, "hands", true }, { EQUIPMENT_SLOT_WAIST, "waist", false }, { EQUIPMENT_SLOT_LEGS, "legs", true },
        { EQUIPMENT_SLOT_FEET, "feet", true }, { EQUIPMENT_SLOT_FINGER1, "ring 1", false }, { EQUIPMENT_SLOT_FINGER2, "ring 2", false },
        { EQUIPMENT_SLOT_TRINKET1, "trinket 1", false }, { EQUIPMENT_SLOT_TRINKET2, "trinket 2", false }, { EQUIPMENT_SLOT_MAINHAND, "main hand", true },
    };
    std::vector<std::string> empty, bare;
    for (SlotName const& sn : kSlots)
    {
        Item const* it = p->GetItemByPos(INVENTORY_SLOT_BAG_0, sn.slot);
        if (!it)
            empty.emplace_back(sn.name);
        else if (sn.enchant && !it->GetEnchantmentId(PERM_ENCHANTMENT_SLOT))
            bare.emplace_back(sn.name);
    }
    // The off hand: a second weapon for the dual wielders, a shield for a tank; the others may carry anything or nothing. A shaman is a dual wielder
    // only if the skill is known (vanilla's enhancement shaman used a two-hander) and the main hand is a one-hander.
    Item const* main = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    bool const shaman_dual = s.spec == 263 && p->CanDualWield() && main && main->GetTemplate()->GetInventoryType() != INVTYPE_2HWEAPON;
    bool const needs_offhand = s.cls == CLASS_ROGUE || s.spec == 72 || s.spec == 73 || shaman_dual;
    Item const* off = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (!off && needs_offhand)
        empty.emplace_back("off hand");
    else if (off && off->GetTemplate()->GetInventoryType() != INVTYPE_HOLDABLE && !off->GetEnchantmentId(PERM_ENCHANTMENT_SLOT))
        bare.emplace_back("off hand");
    Item const* ranged = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
    if (s.cls == CLASS_HUNTER)
    {
        if (!ranged)
            empty.emplace_back("ranged weapon");
        else
        {
            std::uint32_t const ammo = p->GetAmmoId();
            ItemTemplate const* atpl = ammo ? sObjectMgr->GetItemTemplate(ammo) : nullptr;
            std::uint32_t const want_sub = ranged->GetTemplate()->GetSubClass() == ITEM_SUBCLASS_WEAPON_GUN ? ITEM_SUBCLASS_BULLET : ITEM_SUBCLASS_ARROW;
            if (!ammo || !atpl)
                gaps.push_back("ammo: none set");
            else if (atpl->GetClass() != ITEM_CLASS_PROJECTILE || atpl->GetSubClass() != want_sub)
                gaps.push_back(fmt::format("ammo: {} does not fit the ranged weapon", atpl->GetName(sWorld->GetDefaultDbcLocale())));
            else if (p->GetItemCount(ammo) < 200)
                gaps.push_back(fmt::format("ammo: only {} left", p->GetItemCount(ammo)));
        }
    }
    if (!empty.empty())
        gaps.push_back("empty: " + JoinGaps(empty, 16));
    if (!bare.empty())
        gaps.push_back("no enchant: " + JoinGaps(bare, 16));
}

// (4) The role's kit and the raid buffs against the auras the bot carries.
void AuditConsumables(Player* p, GroupSub const& s, std::vector<std::string>& gaps)
{
    using Role = ::Playerbot::Classic::RaidConsumableRole;
    Role role = Role::Melee;
    if (s.role == 2)
        role = Role::Tank;
    else if (s.role == 1)
        role = Role::Healer;
    else if (s.cls == CLASS_HUNTER)
        role = Role::Hunter;
    else if (IsCasterBot(p))
        role = Role::Caster;
    LocaleConstant const loc = sWorld->GetDefaultDbcLocale();
    auto buffed = [&](::Playerbot::Classic::RaidConsumable const& c)
    {
        ItemTemplate const* tpl = sObjectMgr->GetItemTemplate(c.item);
        if (!tpl)
            return true;   // not in the data: nothing to expect
        if (c.food)
            return PlayerHasAuraNamed(p, "Well Fed");
        for (ItemEffectEntry const* eff : tpl->Effects)
            if (eff && eff->SpellID && p->HasAura(eff->SpellID))
                return true;
        return PlayerHasAuraNamed(p, tpl->GetName(loc));
    };
    // A flask fills both elixir slots and the battle elixirs exclude each other, so the kit is met when a flask or any elixir of it is up; the rest of the
    // kit (Juju, scrolls) one by one. Food is only noted: the bots eat it by their own rule, which needs a raid and a seat, not this open ground.
    bool flask_or_elixir_up = false, any_elixir_in_kit = false;
    std::string elixirs;
    bool food_up = true;
    for (::Playerbot::Classic::RaidConsumable const& c : ::Playerbot::Classic::RaidConsumablesFor(role))
    {
        ItemTemplate const* tpl = sObjectMgr->GetItemTemplate(c.item);
        if (!tpl || c.combat)
            continue;
        if (c.food)
        {
            food_up = food_up && buffed(c);
            continue;
        }
        std::string const name = tpl->GetName(loc);
        if (c.flask || name.find("Elixir") != std::string::npos)
        {
            any_elixir_in_kit = true;
            flask_or_elixir_up = flask_or_elixir_up || buffed(c);
            elixirs += (elixirs.empty() ? "" : ", ") + name;
        }
        else if (!buffed(c))
            gaps.push_back(fmt::format("{} not active", name));
    }
    if (any_elixir_in_kit && !flask_or_elixir_up)
        gaps.push_back(fmt::format("no flask or elixir of the kit active ({})", elixirs));
    if (!food_up)
        TC_LOG_INFO("playerbot.v2", "[readiness] {} note: no Well Fed (food is stocked, the bot's eat rule has not run on open ground)", s.label);
    for (std::uint32_t const id : kRaidBuffSpells)
        if (!p->HasAura(id))
        {
            gaps.push_back(fmt::format("buff {} missing", SpellNameOf(id)));
            if (id == 24932)
            {
                // The 230 support scorecard lost Leader of the Pack in its first run on exactly the five bots of the resto druid's subgroup, and the
                // 15 s refresh put it back for the next runs. Who stands in which subgroup, and the form of every druid, to tell what takes it off.
                std::string druids;
                for (GroupSub const& o : gr.subs)
                    if (Player* q = ObjectAccessor::FindConnectedPlayer(o.guid); q && q->GetClass() == CLASS_DRUID)
                        druids += fmt::format("{}{} subgroup {} form {} lotp {}", druids.empty() ? "" : ", ", o.label, uint32(q->GetSubGroup()), uint32(q->GetShapeshiftForm()), int(q->HasAura(24932)));
                TC_LOG_INFO("playerbot.v2", "[readiness] {} has no Leader of the Pack: subgroup {}; druids: {}", s.label, uint32(p->GetSubGroup()), druids.empty() ? "none" : druids);
            }
        }
    if (!p->HasAura(kKingsSpell))
        gaps.push_back("buff Blessing of Kings missing");
    if (role == Role::Caster)
        for (std::uint32_t const id : kCasterBuffSpells)
            if (!p->HasAura(id))
                gaps.push_back(fmt::format("buff {} missing", SpellNameOf(id)));
}

// (5) The spells a spec's rotation cannot do without; a name may list alternatives with '|'. A spell the bot does not know is a gap of its own.
struct KeySpells { std::uint32_t spec; char const* names; };
constexpr KeySpells kKeySpells[] = {
    { 71,  "Mortal Strike,Heroic Strike" },
    { 72,  "Bloodthirst,Whirlwind,Heroic Strike" },
    { 70,  "Judgement|Judgment,Holy Strike" },
    { 253, "Auto Shot,Arcane Shot,Serpent Sting" },
    { 254, "Auto Shot,Aimed Shot|Multi-Shot,Arcane Shot" },   // one shared 6 s cooldown (spell category 2): the instant Multi-Shot takes it
    { 255, "Auto Shot,Arcane Shot" },
    { 260, "Sinister Strike,Eviscerate|Expose Armor" },   // with no warrior's Sunder Armor in the raid the finishers go to Expose Armor (support 236)
    { 259, "Sinister Strike,Eviscerate" },
    { 258, "Mind Blast,Mind Flay,Shadow Word: Pain" },
    { 262, "Lightning Bolt,Chain Lightning" },
    { 263, "Stormstrike,Earth Shock" },
    { 63,  "Fireball,Fire Blast,Scorch" },
    { 64,  "Frostbolt" },
    { 62,  "Frostbolt" },
    { 265, "Shadow Bolt,Corruption,Bane of Agony" },
    { 266, "Shadow Bolt,Immolate,Corruption" },
    { 267, "Shadow Bolt,Immolate,Conflagrate" },
    { 102, "Starfire,Moonfire,Insect Swarm" },
    { 103, "Claw,Rip,Ferocious Bite" },
    // The healers and the tanks (support scorecard).
    { 256, "Power Word: Shield,Greater Heal|Flash Heal" },
    { 257, "Greater Heal|Flash Heal,Renew" },
    { 65,  "Flash of Light|Holy Light" },
    { 105, "Healing Touch,Rejuvenation|Regrowth" },
    // A healer's emergency heal (Flash Heal, Holy Light, Lesser Healing Wave) is cast only when someone is low: with the raid healing by raid rules
    // the support scorecard (236) kept everyone up on the efficient heals, overheal 5-48 %, and the readiness audit failed the healers for the heal
    // the fight never called for. One of them is enough. The same for the holy priest's Flash Heal (238: 4 of 9 runs without one, the
    // raid kept up on Greater Heal, Renew and Prayer of Healing at 40-58 % mana) and the resto druid's two heals over time (Rejuvenation
    // or Regrowth, whichever its patients called for: 238 had runs with only one of them, the raid healed at 15-46 % overheal).
    // The resto shaman's Chain Heal waits for 3 raiders under the medium threshold within 12.5 y of each other (ChainHealTarget): 240 had 3 of 9
    // runs without one, the raid kept up on Healing Wave, two of them with Lesser Healing Wave for the low ones.
    { 264, "Healing Wave,Chain Heal|Lesser Healing Wave" },
    { 73,  "Shield Slam,Sunder Armor,Revenge" },
    { 66,  "Holy Shield,Consecration" },
    { 104, "Maul,Demoralizing Roar" },   // Swipe is the bear's pack threat (2+ mobs in reach); on the one boss Maul is the rage dump
};

// The stance or form a spec plays in (checked after the fight, while the bot stands).
struct KeyStance { std::uint32_t spec; char const* aura; };
constexpr KeyStance kKeyStances[] = { { 71, "Battle Stance" }, { 72, "Berserker Stance" }, { 73, "Defensive Stance" }, { 103, "Cat Form" }, { 104, "Dire Bear Form" } };

void AuditRotation(Player* p, GroupSub const& s, std::map<std::uint32_t, std::uint32_t> const& casts, std::map<std::uint32_t, SpellAgg> const& aggs,
                   std::vector<std::string>& gaps)
{
    if (!s.scored)
        return;
    std::map<std::string, std::uint32_t> by_name;   // lower-cased spell name -> casts (or swings and ticks the tally saw, whichever is more)
    for (auto const& [id, n] : casts)
    {
        std::uint32_t& c = by_name[LowerName(SpellNameOf(id))];
        c = std::max(c, n);
    }
    for (auto const& [key, a] : aggs)
        if (!(key & kPetFlag) && key)
        {
            std::uint32_t& c = by_name[LowerName(SpellNameOf(key))];
            c += a.Attempts();
        }
    std::map<std::string, std::uint32_t> known;   // lower-cased spell name -> a known spell of that name
    for (auto const& [id, ps] : p->GetSpellMap())
        if (ps.state != PLAYERSPELL_REMOVED)
            known.emplace(LowerName(SpellNameOf(id)), id);
    for (KeySpells const& k : kKeySpells)
    {
        if (k.spec != s.spec)
            continue;
        std::string list = k.names;
        for (std::size_t pos = 0; pos <= list.size();)
        {
            std::size_t const comma = std::min(list.find(',', pos), list.size());
            std::string const entry = list.substr(pos, comma - pos);
            pos = comma + 1;
            std::string shown = entry;
            std::uint32_t used = 0;
            bool is_known = false;
            float reach = 0.0f;   // the longest range among the known alternatives (0 = none has a range)
            for (std::size_t a = 0; a <= entry.size();)
            {
                std::size_t const bar = std::min(entry.find('|', a), entry.size());
                std::string const alt = LowerName(entry.substr(a, bar - a));
                a = bar + 1;
                if (auto kn = known.find(alt); kn != known.end())
                {
                    is_known = true;
                    if (SpellInfo const* info = sSpellMgr->GetSpellInfo(kn->second, DIFFICULTY_NONE))
                        reach = std::max(reach, info->GetMaxRange(false, p));
                }
                if (auto it = by_name.find(alt); it != by_name.end())
                    used += it->second;
            }
            // A spell whose range the bot never stood in had no chance to be cast: support 239 put the fire mage on its 24 y ring, out of Fire
            // Blast's 20 y, and the audit failed it for 4 of 9 runs for the Fire Blast that runs at 15-21 y cast.
            if (!is_known)
                gaps.push_back(fmt::format("{} not known", shown));
            else if (!used && reach > 0.0f && s.closest > reach)
                TC_LOG_INFO("playerbot.v2", "[readiness] {} {} cast 0 times: the bot stood {:.1f} y or more from the dummy, its range is {:.1f} y (not a gap)",
                            s.label, shown, s.closest, reach);
            else if (!used)
                gaps.push_back(fmt::format("{} cast 0 times", shown));
        }
    }
    for (KeyStance const& k : kKeyStances)
        if (k.spec == s.spec && !PlayerHasAuraNamed(p, k.aura))
            gaps.push_back(fmt::format("not in {}", k.aura));
}

void AuditReadiness(GroupSub& s, Player* p)
{
    std::vector<std::string> talents, gear, consumables;
    AuditTalents(p, s.spec, talents);
    AuditGear(p, s, gear);
    AuditConsumables(p, s, consumables);
    s.gap_talents = JoinGaps(talents);
    s.gap_gear = JoinGaps(gear);
    s.gap_consumables = JoinGaps(consumables);
    TC_LOG_INFO("playerbot.v2", "[readiness] {} before the fight: talents=[{}] gear=[{}] consumables=[{}]", s.label, s.gap_talents, s.gap_gear, s.gap_consumables);
}

void GroupStartMeasuring(Creature* d)
{
    {
        std::lock_guard<std::mutex> lock(g_mu);
        for (auto& m : g_group_aggs)
            m.clear();
        for (auto& m : g_group_casts)
            m.clear();
        for (auto& t : g_group_timeline)
            t.clear();
        for (auto& t : g_group_real_ms)
            t.clear();
        for (auto& m : g_group_heals)
            m.clear();
        for (auto& m : g_group_taken)
            m.clear();
        std::fill(g_group_pulse.begin(), g_group_pulse.end(), std::uint64_t(0));
        g_dummy = gr.dummy;
    }
    for (GroupSub& s : gr.subs)
    {
        s.casting_s = s.attacking_s = s.idle_s = s.dead_s = s.mana_refills = s.replaced = 0;
        s.rage_samples = s.rage_sum = s.rage_ge30_s = s.rage_ge50_s = s.rage_cap_s = 0;
        s.ranged_close_s = s.melee_far_s = s.last_replace_s = 0;
        s.closest = -1.0f;
        s.oom_s = 0;
        s.mana_min = 100;
        if (Player* p = ObjectAccessor::FindConnectedPlayer(s.guid))
        {
            s.has_mana = p->GetPowerType() == POWER_MANA && p->GetMaxPower(POWER_MANA) > 0;
            s.mana_start = s.mana_end = s.has_mana ? std::uint32_t(100u * p->GetPower(POWER_MANA) / p->GetMaxPower(POWER_MANA)) : 0u;
            s.threat_start = s.threat_end = d ? d->GetThreatManager().GetThreat(p) : 0.f;
            s.peak_threat_pct = 0.f;
            s.pull_threat_s = 0;
            BuffSummary(p, s.buff_count, s.buffs);
            AuditReadiness(s, p);
        }
    }
    gr.measure_s = 0;
    gr.boss_on_tank_s = gr.danger_s = 0;
    gr.lowest_hp = 100.f;
    gr.phase = GroupPhase::Measure;
    gr.t = 0;
    g_measuring = true;
}

void AppendTsv(std::string const& text)
{
    std::ofstream out(sLog->GetLogsDir() + "DpsScorecard.tsv", std::ios::app);
    if (out)
        out << text << '\n';
}

// After the fight: the key spells of the spec against the casts, the stance, and where the bot stood.
void AuditAfterFight(std::vector<std::map<std::uint32_t, std::uint32_t>> const& casts, std::vector<std::map<std::uint32_t, SpellAgg>> const& aggs)
{
    for (std::size_t i = 0; i < gr.subs.size(); ++i)
    {
        GroupSub& s = gr.subs[i];
        std::vector<std::string> ranks, rotation, position;
        if (Player* p = ObjectAccessor::FindConnectedPlayer(s.guid); p && p->IsInWorld())
        {
            std::set<std::uint32_t> used;
            for (auto const& kv : casts[i])
                used.insert(kv.first);
            for (auto const& kv : aggs[i])
                if (!(kv.first & kPetFlag) && kv.first)
                    used.insert(kv.first);
            AuditRanks(p, used, ranks);
            AuditRotation(p, s, casts[i], aggs[i], rotation);
        }
        s.gap_ranks = JoinGaps(ranks);
        if (s.ranged_close_s >= 5)
            position.push_back(fmt::format("ranged bot in melee reach {} s", s.ranged_close_s));
        if (s.melee_far_s >= 5)
            position.push_back(fmt::format("melee bot out of reach {} s", s.melee_far_s));
        if (s.dead_s >= 5)
            position.push_back(fmt::format("dead {} s", s.dead_s));
        s.gap_rotation = JoinGaps(rotation);
        s.gap_position = JoinGaps(position);
    }
}

// One row per bot: its dps, the three spells that made it, how its seconds went, the gear and the buffs it fought with.
void ReportGroup(Creature* d)
{
    g_measuring = false;
    std::vector<std::map<std::uint32_t, SpellAgg>> aggs;
    std::vector<std::map<std::uint32_t, std::uint32_t>> casts;
    std::vector<std::vector<std::pair<std::uint32_t, std::uint32_t>>> timelines;
    std::vector<std::vector<std::int32_t>> real_ms;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        aggs = g_group_aggs;
        casts = g_group_casts;
        timelines = g_group_timeline;
        real_ms = g_group_real_ms;
    }
    AuditAfterFight(casts, aggs);
    double const secs = double(std::max<std::uint32_t>(gr.measure_s, 1));

    char stamp[32] = {};
    std::time_t const now = std::time(nullptr);
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", std::gmtime(&now));

    AppendTsv(fmt::format("# dps_scorecard_raid {} UTC: {} bots on one level 63 dummy (armor {}, no resistances, {} health), Tier 3, {} s measured after {} s of buffing (run {} of {})",
                          stamp, gr.subs.size(), kDummyArmor, 2000000000u, gr.measure_s, gr.buff_s, gr.run, gr.runs));
    AppendTsv("# Raid debuffs are shared: whatever the raid applies to the dummy (Sunder Armor, curses, Shadow Weaving, Improved Scorch, Winter's Chill, Judgement of the Crusader, Faerie Fire ...) benefits every bot, and a debuff that needs a particular class lifts the dps of the others; a bot's row is its dps in this raid, not alone.");
    AppendTsv("# Raid buffs and consumables come from the raid's own bots (buffers cast, the bots drink their kit); the buff_count/buffs columns list what each bot actually carried when the measurement began.");
    AppendTsv("run\tlabel\tclass\tspec\trole\tdps\tdps_busy\tpet_dps\traid_share_pct\trange_t3\tverdict\ttop1\ttop2\ttop3\tcasting_s\tattacking_s\tidle_s\tdead_s\tmana_refills\tavg_ilvl\tt3_pieces\tap\trap\tcrit_melee\tcrit_spell\tsp_max\tbuff_count\tbuffs\treadiness\tgaps_ranks\tgaps_talents\tgaps_gear\tgaps_consumables\tgaps_rotation\tgaps_position\trage_avg\trage_ge30_s\trage_ge50_s\trage_cap_s\tsources8");

    struct Out { std::size_t i; double dps, pet, busy; std::string top; std::string sources; };
    std::vector<Out> outs;
    double raid_total = 0.0;
    for (std::size_t i = 0; i < gr.subs.size(); ++i)
    {
        std::uint64_t total = 0, pet_total = 0;
        std::vector<std::pair<std::uint64_t, std::string>> top;
        for (auto const& [key, a] : aggs[i])
        {
            (key & kPetFlag ? pet_total : total) += a.TotalDamage();
            std::uint32_t const id = key & ~kPetFlag;
            top.emplace_back(a.TotalDamage(), std::string(key & kPetFlag ? "pet " : "") + (id ? SpellNameOf(id) : std::string("white swing")));
        }
        {
            // Every source with its attempts, landed hits and ticks: the three top cells cannot show a spell that is never cast.
            std::vector<std::pair<std::uint64_t, std::string>> all;
            for (auto const& [key, a] : aggs[i])
            {
                std::uint32_t const id = key & ~kPetFlag;
                all.emplace_back(a.TotalDamage(), fmt::format("{}{} dmg={} casts={} landed={} crit={} ticks={}", key & kPetFlag ? "pet " : "", id ? SpellNameOf(id) : std::string("white swing"),
                                                              a.TotalDamage(), a.Attempts(), a.Landed(), a.N(Outcome::Crit), a.ticks));
            }
            std::sort(all.rbegin(), all.rend());
            std::string line;
            for (auto const& e : all)
                line += fmt::format("{}[{}]", line.empty() ? "" : " ", e.second);
            TC_LOG_INFO("playerbot.v2", "[dmg_group] {} sources: {}", gr.subs[i].label, line.empty() ? "none" : line);
        }
        std::sort(top.rbegin(), top.rend());
        std::string cells;
        for (std::size_t k = 0; k < 3; ++k)
        {
            if (k < top.size())
            {
                double const pct = (total + pet_total) ? 100.0 * double(top[k].first) / double(total + pet_total) : 0.0;
                cells += fmt::format("{}{} {} ({:.0f}%)", k ? "\t" : "", top[k].second, top[k].first, pct);
            }
            else
                cells += k ? "\t-" : "-";
        }
        GroupSub const& s = gr.subs[i];
        double const dps = double(total + pet_total) / secs;
        double const busy_s = double(s.casting_s + s.attacking_s);
        // The eight biggest damage sources with the casts the core accepted for each (a swing has none; a tick or proc shows 0 casts).
        std::string sources;
        for (std::size_t k = 0; k < 8 && k < top.size(); ++k)
        {
            std::uint32_t id = 0;
            for (auto const& [key, a] : aggs[i])
            {
                std::string const name = std::string(key & kPetFlag ? "pet " : "") + ((key & ~kPetFlag) ? SpellNameOf(key & ~kPetFlag) : std::string("white swing"));
                if (name == top[k].second && a.TotalDamage() == top[k].first)
                {
                    id = (key & kPetFlag) ? 0u : (key & ~kPetFlag);
                    break;
                }
            }
            std::uint32_t n = 0;
            if (id && i < casts.size())
                if (auto const it = casts[i].find(id); it != casts[i].end())
                    n = it->second;
            double const pct = (total + pet_total) ? 100.0 * double(top[k].first) / double(total + pet_total) : 0.0;
            sources += fmt::format("{}{} {} ({:.0f}%, {} casts)", sources.empty() ? "" : "; ", top[k].second, top[k].first, pct, n);
        }
        outs.push_back({ i, dps, double(pet_total) / secs, busy_s > 0.0 ? double(total + pet_total) / busy_s : 0.0, cells, sources });
        raid_total += dps;
        gr.run_dps[i].push_back(dps);
    }

    std::size_t failing = 0;
    auto OrDash = [](std::string const& v) { return v.empty() ? std::string("-") : v; };
    for (Out const& o : outs)
    {
        GroupSub const& s = gr.subs[o.i];
        Player* p = ObjectAccessor::FindConnectedPlayer(s.guid);
        Stats st{};
        std::string gear = "avg_ilvl=0 pieces=0";
        int t3 = 0;
        if (p)
        {
            st = SnapshotStats(p, d);
            gear = GearSummary(p);
            if (std::size_t const cut = gear.find(" items="); cut != std::string::npos)
                gear.resize(cut);
            t3 = TierPieces(p, 3);
        }
        double sp_max = 0.0;
        for (int school = 1; school < 7; ++school)
            sp_max = std::max(sp_max, st.sp[school]);
        std::string range;
        RangeVerdict const verdict = s.scored ? DpsVerdict(s.cls, 3, /*raid_buffs*/ true, o.dps, &range, s.spec) : RangeVerdict::NoReference;
        std::string ilvl = gear;
        if (std::size_t const sp = gear.find(' '); sp != std::string::npos)
            ilvl = gear.substr(0, sp);
        if (ilvl.rfind("avg_ilvl=", 0) == 0)
            ilvl.erase(0, 9);
        bool const ready = s.gap_ranks.empty() && s.gap_talents.empty() && s.gap_gear.empty() && s.gap_consumables.empty() && s.gap_rotation.empty() && s.gap_position.empty();
        std::string const verdict_text = !ready ? (s.scored ? std::string("FAIL (") + ToString(verdict) + ")" : std::string("FAIL")) : s.scored ? std::string(ToString(verdict)) : std::string("-");
        failing += !ready;
        TC_LOG_INFO("playerbot.v2", "[readiness] {} {}: ranks=[{}] talents=[{}] gear=[{}] consumables=[{}] rotation=[{}] position=[{}]", s.label, ready ? "PASS" : "FAIL",
                    s.gap_ranks, s.gap_talents, s.gap_gear, s.gap_consumables, s.gap_rotation, s.gap_position);
        AppendTsv(fmt::format("{}\t{}\t{}\t{}\t{}\t{:.0f}\t{:.0f}\t{:.0f}\t{:.1f}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{:.0f}\t{:.0f}\t{:.2f}\t{:.2f}\t{:.0f}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
                              stamp, s.label, ClassName(s.cls), s.spec, s.scored ? "dps" : "support", o.dps, o.busy, o.pet, raid_total > 0.0 ? 100.0 * o.dps / raid_total : 0.0,
                              range.empty() ? "-" : range, verdict_text, o.top, s.casting_s, s.attacking_s, s.idle_s, s.dead_s, s.mana_refills, ilvl, t3,
                              st.ap, st.rap, st.crit_melee, st.crit_spell, sp_max, s.buff_count, s.buffs.empty() ? "-" : s.buffs, ready ? "PASS" : "FAIL",
                              OrDash(s.gap_ranks), OrDash(s.gap_talents), OrDash(s.gap_gear), OrDash(s.gap_consumables), OrDash(s.gap_rotation), OrDash(s.gap_position),
                              s.rage_samples ? fmt::format("{:.1f}", double(s.rage_sum) / double(s.rage_samples)) : std::string("-"), s.rage_ge30_s, s.rage_ge50_s, s.rage_cap_s, o.sources.empty() ? std::string("-") : o.sources));
        if (s.rage_samples)
            TC_LOG_INFO("playerbot.v2", "[dmg_group] {} rage: samples={} avg={:.1f} secs_ge30={} secs_ge50={} secs_at_cap={} talents_refused=[{}]", s.label, s.rage_samples,
                        double(s.rage_sum) / double(s.rage_samples), s.rage_ge30_s, s.rage_ge50_s, s.rage_cap_s, s.gap_talents.empty() ? "none" : s.gap_talents);
        TC_LOG_INFO("playerbot.v2", "[dmg_group] {} ({}) dps={:.0f} busy_dps={:.0f} pet_dps={:.0f} share={:.1f}% range={} verdict={} casting_s={} attacking_s={} idle_s={} dead_s={} refills={} replaced={} t3_pieces={} {} buffs={}: {}",
                    s.label, s.name, o.dps, o.busy, o.pet, raid_total > 0.0 ? 100.0 * o.dps / raid_total : 0.0, range.empty() ? "-" : range, verdict_text,
                    s.casting_s, s.attacking_s, s.idle_s, s.dead_s, s.mana_refills, s.replaced, t3, gear, s.buff_count, s.buffs.empty() ? "-" : s.buffs);
        TC_LOG_INFO("playerbot.v2", "[dmg_group] {} sources: {}", s.label, o.sources.empty() ? "-" : o.sources);
        // The cast timeline: every accepted cast, how long it keeps the bot busy (cast time, channel or global cooldown, whichever is longer,
        // before haste), and the gap to the next cast. Gaps over 0.5 s are summed, and by the spell they follow, so "46 s of 120 not casting"
        // reads as global cooldowns, cast starts the bot sent late, or a wait (movement, a rule that did not fire).
        if (o.i < timelines.size() && !timelines[o.i].empty())
        {
            auto const& tl = timelines[o.i];
            auto busy_ms = [](std::uint32_t id) -> std::int64_t
            {
                SpellInfo const* info = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
                if (!info)
                    return 0;
                std::int64_t const cast = info->IsChanneled() ? std::int64_t(info->GetDuration()) : std::int64_t(info->CalcCastTime());
                return std::max<std::int64_t>(cast, info->StartRecoveryTime > 0 ? std::int64_t(info->StartRecoveryTime) : 0);
            };
            std::int64_t gap_total_ms = 0, busy_total_ms = 0;
            std::uint32_t gap_n = 0;
            std::map<std::uint32_t, std::pair<std::uint32_t, std::int64_t>> gap_after;   // spell -> (count, ms)
            std::string line;
            for (std::size_t k = 0; k < tl.size(); ++k)
            {
                std::int64_t const busy = busy_ms(tl[k].second);
                std::int64_t const real = o.i < real_ms.size() && k < real_ms[o.i].size() ? real_ms[o.i][k] : 0;
                busy_total_ms += busy;
                std::int64_t gap = 0;
                if (k + 1 < tl.size())
                    gap = std::int64_t(tl[k + 1].first) - std::int64_t(tl[k].first) - busy;
                if (gap > 500)
                {
                    gap_total_ms += gap;
                    ++gap_n;
                    auto& e = gap_after[tl[k].second];
                    ++e.first;
                    e.second += gap;
                }
                if (k < 45)
                    line += fmt::format("{}{}@{:.1f}({} real {}{}, next at +{})", line.empty() ? "" : " ", SpellNameOf(tl[k].second), double(tl[k].first - tl.front().first) / 1000.0, busy, real,
                                        gap > 500 ? fmt::format(", gap {}", gap) : std::string(), k + 1 < tl.size() ? std::int64_t(tl[k + 1].first) - std::int64_t(tl[k].first) : 0);
            }
            std::string after;
            for (auto const& [id, e] : gap_after)
                after += fmt::format("{}{} x{} {:.1f} s", after.empty() ? "" : ", ", SpellNameOf(id), e.first, double(e.second) / 1000.0);
            TC_LOG_INFO("playerbot.v2", "[dmg_cast] {} casts={} busy_by_data={:.1f} s gaps_over_0.5s={} totalling {:.1f} s of {} s measured; gap seconds by the spell before: [{}]", s.label, tl.size(),
                        double(busy_total_ms) / 1000.0, gap_n, double(gap_total_ms) / 1000.0, gr.measure_s, after.empty() ? "none" : after);
            TC_LOG_INFO("playerbot.v2", "[dmg_cast] {} timeline (spell@seconds(busy ms, gap ms)), first 45 casts: {}", s.label, line);
        }
    }
    TC_LOG_INFO("playerbot.v2", "[readiness] {} of {} bots FAIL", failing, gr.subs.size());
    AppendTsv(fmt::format("# readiness: {} of {} bots FAIL (any gap in ranks, talents, gear, consumables, rotation or position fails a bot whatever its dps)", failing, gr.subs.size()));
    TC_LOG_INFO("playerbot.v2", "[dmg_group] raid total {:.0f} dps over {} s ({} bots) on {}", raid_total, gr.measure_s, gr.subs.size(), gr.dummy_name);
    AppendTsv(fmt::format("# raid total {:.0f} dps over {} s", raid_total, gr.measure_s));
}

// The dps column of a TSV by label. The columns are found from the header row (a row whose cells include "label" and "dps"), so the
// scorecard's own layout and the trimmed run/label/dps layout both read; rows before a header are skipped. A file holding several
// scorecards (the log appends) gives the last row seen for a label. CRLF line ends are accepted.
std::map<std::string, double> LoadBaseline(std::string const& path)
{
    std::map<std::string, double> out;
    std::ifstream in(path);
    std::string line;
    std::size_t label_col = 0, dps_col = 0;
    bool have_header = false;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        std::vector<std::string> cols;
        std::size_t from = 0;
        for (std::size_t tab; (tab = line.find('\t', from)) != std::string::npos; from = tab + 1)
            cols.push_back(line.substr(from, tab - from));
        cols.push_back(line.substr(from));
        auto const find_col = [&](char const* name) { for (std::size_t i = 0; i < cols.size(); ++i) if (cols[i] == name) return i; return cols.size(); };
        std::size_t const l = find_col("label"), d = find_col("dps");
        if (l != cols.size() && d != cols.size())
        {
            label_col = l;
            dps_col = d;
            have_header = true;
            continue;
        }
        if (!have_header || cols.size() <= std::max(label_col, dps_col))
            continue;
        out[cols[label_col]] = std::strtod(cols[dps_col].c_str(), nullptr);
    }
    return out;
}

// After the last run: each bot's average over the runs and its spread (highest minus lowest), and the comparison with the baseline scorecard.
// A bot whose average is below the baseline by more than its spread is a REGRESSION; any REGRESSION fails the whole scorecard.
void ReportRuns()
{
    std::map<std::string, double> const base = gr.baseline_path.empty() ? std::map<std::string, double>() : LoadBaseline(gr.baseline_path);
    if (!gr.baseline_path.empty() && base.empty())
        TC_LOG_ERROR("playerbot.v2", "[dmg_group] baseline {} is missing or holds no rows: nothing to compare", gr.baseline_path);
    AppendTsv(fmt::format("# averages of {} runs; spread = highest run minus lowest run; baseline {}", gr.runs, base.empty() ? std::string("none") : gr.baseline_path));
    AppendTsv(std::string("summary\tlabel\tclass\tspec\tavg_value\tspread\tmin\tmax\tbaseline_value\tchange\tresult") +
              (gr.support ? "\trole\tavg_hps\tavg_overheal_pct\tavg_mana_end\tavg_oom_s\tavg_threat_per_s\tavg_damage_taken_per_s\tavg_avoid_pct" : ""));
    std::size_t regressions = 0;
    double avg_total = 0.0, base_total = 0.0;
    for (std::size_t i = 0; i < gr.subs.size(); ++i)
    {
        GroupSub const& s = gr.subs[i];
        std::vector<double> const& v = gr.run_dps[i];
        if (v.empty())
            continue;
        double const lo = *std::min_element(v.begin(), v.end());
        double const hi = *std::max_element(v.begin(), v.end());
        double avg = 0.0;
        for (double x : v)
            avg += x;
        avg /= double(v.size());
        double const spread = hi - lo;
        avg_total += avg;
        std::string result = "-";
        std::string base_text = "-", change_text = "-";
        if (auto const it = base.find(s.label); it != base.end())
        {
            base_total += it->second;
            base_text = fmt::format("{:.0f}", it->second);
            change_text = fmt::format("{:+.0f}", avg - it->second);
            if (it->second - avg > spread)
            {
                result = "REGRESSION";
                ++regressions;
            }
            else
                result = "ok";
        }
        else if (!base.empty())
            result = "no baseline";
        std::string extra;
        if (gr.support && i < gr.run_support.size() && !gr.run_support[i].empty())
        {
            // The healer and tank numbers, each averaged over the runs that have it.
            extra = std::string("\t") + (s.role == 1 ? "healer" : s.role == 2 ? "tank" : "dps");
            for (std::size_t k = 0; k < 7; ++k)
            {
                double sum = 0.0;
                std::size_t n = 0;
                for (auto const& r : gr.run_support[i])
                    if (r[k] >= 0.0)
                    {
                        sum += r[k];
                        ++n;
                    }
                extra += n ? fmt::format("\t{:.{}f}", sum / double(n), k == 1 || k == 6 ? 1 : 0) : std::string("\t-");
            }
        }
        AppendTsv(fmt::format("summary\t{}\t{}\t{}\t{:.0f}\t{:.0f}\t{:.0f}\t{:.0f}\t{}\t{}\t{}{}", s.label, ClassName(s.cls), s.spec, avg, spread, lo, hi, base_text, change_text, result, extra));
        TC_LOG_INFO("playerbot.v2", "[dmg_group] {} avg {:.0f} over {} runs (min {:.0f} max {:.0f} spread {:.0f}) baseline {} {}{}", s.label, avg, v.size(), lo, hi, spread, base_text, result,
                    extra.empty() ? std::string() : " | role hps overheal% mana_end oom_s threat/s taken/s avoid%:" + extra);
    }
    std::string const verdict = base.empty() ? std::string("NO BASELINE") : regressions ? std::string("FAIL") : std::string("PASS");
    TC_LOG_INFO("playerbot.v2", "[dmg_group] scorecard {}: {} regressions against the baseline, raid average {:.0f} over {} runs (baseline {:.0f}), {} readiness or role failures in the runs", verdict, regressions, avg_total, gr.runs, base_total, gr.run_fails);
    AppendTsv(fmt::format("# SCORECARD {}: {} regressions, raid average {:.0f} over {} runs, baseline raid total {:.0f}, {} readiness or role failures in the runs", verdict, regressions, avg_total, gr.runs, base_total, gr.run_fails));
}

// ---- The support scorecard (healers and tanks) ----
// The same raid, buffs and measurement as the dps scorecard, but the boss fights back: it swings at the tank and every few seconds shoots a pulse at
// raiders in turn, and the healers have to keep everyone up. Healers are scored on what they restore (effective healing per second, overheal, mana,
// casts), the tank on threat, aggro and what it takes; every bot on the readiness audit.

// The boss goes for the tank when it has no target. A dps that pulls the boss off the tank stays the target: that is what the aggro column reports.
void SupportBossOnTank(Creature* d)
{
    if (gr.tank >= gr.subs.size() || d->GetVictim())
        return;
    Player* tank = ObjectAccessor::FindConnectedPlayer(gr.subs[gr.tank].guid);
    if (!tank || !tank->IsAlive())
        return;
    d->EngageWithTarget(tank);
    d->Attack(tank, true);
    d->GetMotionMaster()->MoveChase(tank);
}

// Each run's boss starts on the tank. Armed aggressive (ArmMob), or with the raid's combat stopped between runs, it takes the nearest
// raider on its next update before SupportBossOnTank sees it free: in all three paladin runs of 237 it opened on a member (the warlock in
// run 2), the dps kept hitting a boss on a member, and the paladin tank, with no taunt, never had it (0 of 120 s).
// The raid's damage over time from the run before goes too: it kept ticking into the cleared threat list with the tank at no threat, and
// the boss turned on its caster within a second (243 paladin run 3: Sptaff at 925 threat against the tank's 116 one second in; run 2 the
// fire mage at 686 against 58). The fury then opened on a boss loose on a member and held it, and the paladin, with no taunt, had it 0 of
// 120 s in runs 2 and 3 of both 242 and 243. Run 1 starts on a dummy with nothing on it; now every run does.
void SupportBossStartOnTank(Creature* d)
{
    ObjectGuid const self = d->GetGUID();
    d->RemoveAppliedAuras([self](AuraApplication const* a) { return a->GetBase()->GetCasterGUID() != self; });
    d->AttackStop();
    d->GetThreatManager().ResetAllThreat();
    SupportBossOnTank(d);
}

// Once per measured second: who the boss is hitting, how low the raid is, each bot's threat, and the raid pulse.
void SupportSecond(Creature* d)
{
    SupportBossOnTank(d);
    Player* tank = gr.tank < gr.subs.size() ? ObjectAccessor::FindConnectedPlayer(gr.subs[gr.tank].guid) : nullptr;
    if (tank && tank->IsAlive() && d->GetVictim() == tank)
        ++gr.boss_on_tank_s;
    bool danger = false;
    std::vector<Player*> targets;   // the raiders a pulse can hit: everyone alive but the tank
    for (std::size_t i = 0; i < gr.subs.size(); ++i)
    {
        GroupSub& s = gr.subs[i];
        Player* p = ObjectAccessor::FindConnectedPlayer(s.guid);
        if (!p || !p->IsInWorld() || !p->IsAlive())
            continue;
        float const hp = float(p->GetHealthPct());
        gr.lowest_hp = std::min(gr.lowest_hp, hp);
        danger = danger || hp < 35.f;
        s.threat_end = d->GetThreatManager().GetThreat(p);
        if (i != gr.tank)
            targets.push_back(p);
    }
    gr.danger_s += danger;
    // A dps over the tank's threat takes the boss only past the core's switch ratio, 110 % in melee reach and 130 % at range: a fury held at the
    // tank's threat (237: 65437 against 64758 at the end of run 1, the boss on the tank) gains threat a little faster than the tank over the
    // window and is not a threat to its aggro.
    if (tank && tank->IsAlive() && gr.subs[gr.tank].threat_end > 0.f)
    {
        float const tank_threat = gr.subs[gr.tank].threat_end;
        for (GroupSub& s : gr.subs)
        {
            Player* p = s.role == 0 ? ObjectAccessor::FindConnectedPlayer(s.guid) : nullptr;
            if (!p || !p->IsInWorld() || !p->IsAlive())
                continue;
            s.peak_threat_pct = std::max(s.peak_threat_pct, 100.f * s.threat_end / tank_threat);
            if (s.threat_end >= tank_threat * (d->IsWithinMeleeRange(p) ? kAggroSwitchRatioMelee : kAggroSwitchRatioRanged))
                ++s.pull_threat_s;
        }
    }
    if (gr.pulse_s && gr.measure_s % gr.pulse_s == 0)
        for (std::uint32_t n = 0; n < gr.pulse_n && !targets.empty(); ++n)
            Unit::DealDamage(d, targets[(gr.pulse_i++) % targets.size()], std::uint32_t(gr.pulse_dmg), nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_FIRE, nullptr, false);
}

std::string TabRow(std::vector<std::string> const& cells)
{
    std::string out;
    for (std::size_t i = 0; i < cells.size(); ++i)
    {
        if (i)
            out += '\t';
        out += cells[i];
    }
    return out;
}

std::string Num(double v, int decimals = 0) { return fmt::format("{:.{}f}", v, decimals); }

void ReportSupport(Creature* d)
{
    g_measuring = false;
    std::vector<std::map<std::uint32_t, SpellAgg>> aggs, taken;
    std::vector<std::map<std::uint32_t, std::uint32_t>> casts;
    std::vector<std::map<std::uint32_t, HealAgg>> heals;
    std::vector<std::uint64_t> pulses;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        aggs = g_group_aggs;
        casts = g_group_casts;
        heals = g_group_heals;
        taken = g_group_taken;
        pulses = g_group_pulse;
    }
    AuditAfterFight(casts, aggs);
    double const secs = double(std::max<std::uint32_t>(gr.measure_s, 1));

    char stamp[32] = {};
    std::time_t const now = std::time(nullptr);
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", std::gmtime(&now));
    AppendTsv(fmt::format("# support_scorecard {} UTC: {} bots, {} tanking (swings of {:.0f} every 2 s) and a pulse of {:.0f} at {} raiders every {} s on a level 63 boss, Tier 3, {} s measured after {} s of buffing (run {} of {})",
                          stamp, gr.subs.size(), gr.subs[gr.tank].label, gr.swing, gr.pulse_dmg, gr.pulse_n, gr.pulse_s, gr.measure_s, gr.buff_s, gr.run, gr.runs));
    AppendTsv("# value = effective healing per second for a healer, threat per second for the tank, damage per second for a dps. Overheal = the share of a heal's worth that found the target already full. Mana columns are percent of the bot's mana pool.");
    AppendTsv(TabRow({ "run", "label", "class", "spec", "role", "value", "hps_effective", "hps_raw", "overheal_pct", "heals_landed", "heal_casts_per_min", "casts_per_min", "top1", "top2", "top3",
                       "casting_s", "idle_s", "dead_s", "mana_start", "mana_min", "mana_end", "oom_s", "threat_per_s", "boss_on_me_pct", "damage_taken", "damage_taken_per_s",
                       "avoid_pct", "swings", "dodge", "parry", "block", "miss", "crit", "crush", "avg_ilvl", "t3_pieces", "readiness", "gaps_ranks", "gaps_talents", "gaps_gear", "gaps_consumables", "gaps_rotation",
                       "gaps_position", "gaps_role", "buff_count", "buffs", "heal_spells" }));

    // The dps that came closest to taking the boss: the seconds it held the switch ratio's threat, then its peak share of the tank's.
    GroupSub const* top_puller = nullptr;
    for (std::size_t i = 0; i < gr.subs.size(); ++i)
        if (GroupSub const& o = gr.subs[i]; i != gr.tank && o.role == 0 &&
            (!top_puller || o.pull_threat_s > top_puller->pull_threat_s ||
             (o.pull_threat_s == top_puller->pull_threat_s && o.peak_threat_pct > top_puller->peak_threat_pct)))
            top_puller = &o;

    std::uint64_t raid_eff = 0, raid_raw = 0, raid_taken = 0, raid_swings = 0, raid_avoided = 0;
    std::size_t failing = 0, deaths = 0;
    std::string dead_names;
    for (std::size_t i = 0; i < gr.subs.size(); ++i)
    {
        GroupSub& s = gr.subs[i];
        std::uint64_t eff = 0, raw = 0, heal_n = 0, heal_casts = 0;
        std::vector<std::pair<std::uint64_t, std::string>> top;
        for (auto const& [id, h] : heals[i])
        {
            eff += h.effective;
            raw += h.raw;
            heal_n += h.heals;
            if (auto const it = casts[i].find(id); it != casts[i].end())
                heal_casts += it->second;
            top.emplace_back(h.effective, id ? SpellNameOf(id) : std::string("heal without a spell"));
        }
        // Every heal by name (the ranks of a spell together): casts, heals landed (a hot tick is one), what it restored and its overheal.
        std::string heal_spells;
        {
            struct ByName { std::uint64_t casts = 0, landed = 0, raw = 0, eff = 0; };
            std::map<std::string, ByName> by_name;
            for (auto const& [id, h] : heals[i])
            {
                ByName& b = by_name[id ? SpellNameOf(id) : std::string("heal without a spell")];
                b.landed += h.heals;
                b.raw += h.raw;
                b.eff += h.effective;
                if (auto const it = casts[i].find(id); it != casts[i].end())
                    b.casts += it->second;
            }
            std::vector<std::pair<std::uint64_t, std::string>> rows;
            for (auto const& [name, b] : by_name)
                rows.emplace_back(b.eff, fmt::format("{} casts={} landed={} healed={} overheal={:.0f}%", name, b.casts, b.landed, b.eff, b.raw ? 100.0 * double(b.raw - b.eff) / double(b.raw) : 0.0));
            std::sort(rows.rbegin(), rows.rend());
            for (auto const& r : rows)
                heal_spells += (heal_spells.empty() ? "" : "; ") + r.second;
        }
        std::sort(top.rbegin(), top.rend());
        std::vector<std::string> tops;
        for (std::size_t k = 0; k < 3; ++k)
            tops.push_back(k < top.size() && eff ? fmt::format("{} {} ({:.0f}%)", top[k].second, top[k].first, 100.0 * double(top[k].first) / double(eff)) : std::string("-"));
        std::uint64_t cast_n = 0;
        for (auto const& kv : casts[i])
            cast_n += kv.second;
        std::uint64_t dps_total = 0;
        for (auto const& [key, a] : aggs[i])
            dps_total += a.TotalDamage();
        SpellAgg swings;
        if (auto const it = taken[i].find(0); it != taken[i].end())
            swings = it->second;
        std::uint64_t const dmg_taken = swings.TotalDamage() + pulses[i];
        raid_eff += eff;
        raid_raw += raw;
        raid_taken += dmg_taken;
        raid_swings += swings.Attempts();
        raid_avoided += swings.N(Outcome::Miss) + swings.N(Outcome::Dodge) + swings.N(Outcome::Parry) + swings.N(Outcome::Block);
        double const tps = double(s.threat_end - s.threat_start) / secs;
        double const hps = double(eff) / secs;
        double const value = s.role == 1 ? hps : s.role == 2 ? tps : double(dps_total) / secs;

        // What is wrong with the role itself.
        std::vector<std::string> role_gaps;
        if (s.role == 1)
        {
            if (!eff)
                role_gaps.push_back("healed nothing");
            if (s.oom_s >= 10)
                role_gaps.push_back(fmt::format("under 5% mana for {} s", s.oom_s));
        }
        else if (s.role == 2)
        {
            if (s.dead_s)
                role_gaps.push_back(fmt::format("the tank was dead {} s", s.dead_s));
            if (gr.boss_on_tank_s * 100u < gr.measure_s * 95u)
                role_gaps.push_back(fmt::format("the boss was on the tank {} of {} s", gr.boss_on_tank_s, gr.measure_s));
            if (top_puller && top_puller->pull_threat_s)
                role_gaps.push_back(fmt::format("{} held the threat to take the boss for {} s (peak {:.0f}% of the tank's)", top_puller->label, top_puller->pull_threat_s,
                                                top_puller->peak_threat_pct));
        }
        if (s.dead_s)
        {
            ++deaths;
            dead_names += (dead_names.empty() ? "" : ", ") + s.label;
        }
        std::string const role_text = JoinGaps(role_gaps);

        Player* p = ObjectAccessor::FindConnectedPlayer(s.guid);
        std::string gear = "avg_ilvl=0 pieces=0";
        int t3 = 0;
        if (p)
        {
            gear = GearSummary(p);
            if (std::size_t const cut = gear.find(" items="); cut != std::string::npos)
                gear.resize(cut);
            t3 = TierPieces(p, 3);
        }
        std::string ilvl = gear;
        if (std::size_t const sp = gear.find(' '); sp != std::string::npos)
            ilvl = gear.substr(0, sp);
        if (ilvl.rfind("avg_ilvl=", 0) == 0)
            ilvl.erase(0, 9);
        bool const ready = s.gap_ranks.empty() && s.gap_talents.empty() && s.gap_gear.empty() && s.gap_consumables.empty() && s.gap_rotation.empty() && s.gap_position.empty() && role_text.empty();
        failing += !ready;
        auto OrDash = [](std::string const& v) { return v.empty() ? std::string("-") : v; };
        char const* const role_name = s.role == 1 ? "healer" : s.role == 2 ? "tank" : "dps";
        TC_LOG_INFO("playerbot.v2", "[readiness] {} {}: ranks=[{}] talents=[{}] gear=[{}] consumables=[{}] rotation=[{}] position=[{}] role=[{}]", s.label, ready ? "PASS" : "FAIL",
                    s.gap_ranks, s.gap_talents, s.gap_gear, s.gap_consumables, s.gap_rotation, s.gap_position, role_text);
        AppendTsv(TabRow({ stamp, s.label, ClassName(s.cls), std::to_string(s.spec), role_name, Num(value), Num(hps), Num(double(raw) / secs),
                           raw ? Num(100.0 * double(raw - eff) / double(raw), 1) : std::string("-"), std::to_string(heal_n), Num(60.0 * double(heal_casts) / secs, 1), Num(60.0 * double(cast_n) / secs, 1),
                           tops[0], tops[1], tops[2], std::to_string(s.casting_s), std::to_string(s.idle_s), std::to_string(s.dead_s),
                           s.has_mana ? std::to_string(s.mana_start) : std::string("-"), s.has_mana ? std::to_string(s.mana_min) : std::string("-"), s.has_mana ? std::to_string(s.mana_end) : std::string("-"),
                           std::to_string(s.oom_s), Num(tps), i == gr.tank ? Num(100.0 * double(gr.boss_on_tank_s) / secs, 1) : std::string("-"), std::to_string(dmg_taken), Num(double(dmg_taken) / secs),
                           swings.Attempts() ? Num(swings.AvoidPct(), 1) : std::string("-"), std::to_string(swings.Attempts()), std::to_string(swings.N(Outcome::Dodge)), std::to_string(swings.N(Outcome::Parry)),
                           std::to_string(swings.N(Outcome::Block)), std::to_string(swings.N(Outcome::Miss)), std::to_string(swings.N(Outcome::Crit)), std::to_string(swings.N(Outcome::Crushing)),
                           ilvl, std::to_string(t3), ready ? "PASS" : "FAIL", OrDash(s.gap_ranks), OrDash(s.gap_talents), OrDash(s.gap_gear), OrDash(s.gap_consumables), OrDash(s.gap_rotation),
                           OrDash(s.gap_position), OrDash(role_text), std::to_string(s.buff_count), s.buffs.empty() ? std::string("-") : s.buffs, OrDash(heal_spells) }));
        TC_LOG_INFO("playerbot.v2", "[support] {} ({}) {} value={:.0f} eff_hps={:.0f} raw_hps={:.0f} overheal={:.1f}% heal_casts={} casts={} mana {}->{} (min {}, under 5% {} s) threat/s={:.0f} taken={} dead_s={} {}",
                    s.label, s.name, role_name, value, hps, double(raw) / secs, raw ? 100.0 * double(raw - eff) / double(raw) : 0.0, heal_casts, cast_n,
                    s.mana_start, s.mana_end, s.mana_min, s.oom_s, tps, dmg_taken, s.dead_s, ready ? "PASS" : "FAIL");
        if (!heal_spells.empty())
            TC_LOG_INFO("playerbot.v2", "[support] {} heals: {}", s.label, heal_spells);
        gr.run_dps[i].push_back(value);
        gr.run_support[i].push_back({ hps, raw ? 100.0 * double(raw - eff) / double(raw) : 0.0, s.has_mana ? double(s.mana_end) : -1.0, double(s.oom_s), tps,
                                      double(dmg_taken) / secs, swings.Attempts() ? swings.AvoidPct() : -1.0 });
    }
    gr.run_fails += std::uint32_t(failing + deaths);
    TC_LOG_INFO("playerbot.v2", "[readiness] {} of {} bots FAIL", failing, gr.subs.size());
    AppendTsv(fmt::format("# readiness: {} of {} bots FAIL (any gap in ranks, talents, gear, consumables, rotation, position or role fails a bot)", failing, gr.subs.size()));
    std::string const summary = fmt::format("raid: the boss took the tank for {} of {} s; damage taken {} ({:.0f} a second, {} swings, {:.1f}% avoided); effective healing {} ({:.0f} a second) of {} raw, {:.1f}% overheal; a raider was under 35% health for {} s, lowest {:.0f}%; {} died{}{}",
                                             gr.boss_on_tank_s, gr.measure_s, raid_taken, double(raid_taken) / secs, raid_swings, raid_swings ? 100.0 * double(raid_avoided) / double(raid_swings) : 0.0,
                                             raid_eff, double(raid_eff) / secs, raid_raw, raid_raw ? 100.0 * double(raid_raw - raid_eff) / double(raid_raw) : 0.0, gr.danger_s, double(gr.lowest_hp),
                                             deaths, deaths ? ": " : "", dead_names);
    TC_LOG_INFO("playerbot.v2", "[support] {}", summary);
    AppendTsv("# " + summary);
    (void)d;
}

bool GroupStep(std::string& failure)
{
    Player* first = nullptr;
    for (GroupSub const& s : gr.subs)
        if (Player* p = ObjectAccessor::FindConnectedPlayer(s.guid); p && p->IsInWorld())
        {
            first = p;
            break;
        }
    if (!first)
    {
        failure = "the raid is gone";
        End();
        return true;
    }
    Creature* d = ObjectAccessor::GetCreature(*first, gr.dummy);
    if (!d || !d->IsAlive())
    {
        failure = "the boss dummy is gone";
        End();
        return true;
    }
    ++gr.t;
    // The dummy stays on its site: put back, and said so, when it ever drifts more than 1 y (the first second after the teleport is skipped, as the
    // creature takes it at its next update).
    if (gr.has_site && gr.phase != GroupPhase::Place && d->GetExactDist2d(gr.site.GetPositionX(), gr.site.GetPositionY()) > 1.0f)
    {
        TC_LOG_INFO("playerbot.v2", "[dmg_group] the dummy drifted {:.1f} y off its site to ({:.1f},{:.1f},{:.1f}) in phase {}: put back", d->GetExactDist2d(gr.site.GetPositionX(), gr.site.GetPositionY()),
                    d->GetPositionX(), d->GetPositionY(), d->GetPositionZ(), int(gr.phase));
        d->NearTeleportTo(gr.site.GetPositionX(), gr.site.GetPositionY(), gr.site.GetPositionZ(), d->GetOrientation());
    }
    switch (gr.phase)
    {
        case GroupPhase::Place:
            for (GroupSub const& s : gr.subs)
                if (Player* p = ObjectAccessor::FindConnectedPlayer(s.guid))
                {
                    ClearBagGear(p, 20);   // a full bag sends a bot to the vendor (idle:capital_bag_run) in the middle of the window
                    StockGroupKit(p);
                }
            GroupPlaceAll(d);
            gr.phase = GroupPhase::Buff;
            gr.t = 0;
            TC_LOG_INFO("playerbot.v2", "[dmg_group] {} bots placed around {} at ({:.1f},{:.1f},{:.1f}); {} s for the raid buffs and consumables", gr.subs.size(), gr.dummy_name, d->GetPositionX(), d->GetPositionY(), d->GetPositionZ(), gr.buff_s);
            return false;
        case GroupPhase::Buff:
            // Full health and mana while the buffers work and the bots eat and drink their kit; nobody fights yet.
            // A bot without a snapshot at the placement (just logged in) gets its kit now; the bags are checked by count, so a second pass adds nothing twice.
            if (gr.t == 8)
                for (GroupSub const& s : gr.subs)
                    if (Player* p = ObjectAccessor::FindConnectedPlayer(s.guid); p && p->IsAlive())
                        StockGroupKit(p);
            for (GroupSub const& s : gr.subs)
                if (Player* p = ObjectAccessor::FindConnectedPlayer(s.guid); p && p->IsAlive() && !p->IsInCombat())
                    TopUp(p);
            if (gr.t >= gr.buff_s)
            {
                GroupPlaceAll(d);   // the bots follow their leader while they buff: back to their places
                for (GroupSub const& s : gr.subs)
                    if (Player* p = ObjectAccessor::FindConnectedPlayer(s.guid); p && p->IsAlive())
                        TopUp(p);
                GroupRaidBuffs();
                if (!gr.support)   // the tank casts its own Sunder Armor in a support scorecard: threat is part of what it measures
                    GroupRaidDebuffs(d);
                gr.phase = GroupPhase::Warm;
                gr.t = 0;
                if (gr.support)
                {
                    ArmMob(d, gr.swing);
                    SupportBossStartOnTank(d);
                }
            }
            return false;
        case GroupPhase::Warm:
            // The support scorecard measures threat discipline: its tank opens, the rest get the owner's Attack order (EngageOrder).
            for (std::size_t i = 0; i < gr.subs.size(); ++i)
                if (Player* p = ObjectAccessor::FindConnectedPlayer(gr.subs[i].guid); p && p->IsAlive() && !(gr.support && gr.subs[i].role == 1))
                {
                    if (gr.support && i != gr.tank)
                        EngageOrder(p, d);
                    else
                        Engage(p, d);
                }
            if (gr.support)
                SupportBossOnTank(d);
            if (gr.t >= 5)
                GroupStartMeasuring(d);
            return false;
        case GroupPhase::Measure:
            break;
    }

    ++gr.measure_s;
    for (GroupSub& s : gr.subs)
    {
        Player* p = ObjectAccessor::FindConnectedPlayer(s.guid);
        if (!p || !p->IsInWorld() || !p->IsAlive())
        {
            ++s.dead_s;
            continue;
        }
        // A bot's own errand (a bag run, a walk to the vendor) takes it off its place: put it back, once counted.
        // A ranged bot is also put back when it has closed to melee (a hunter inside its dead zone shoots nothing and swings).
        // A teleport cancels the cast under way and the bot's own repositioning (reposition_los), so it is rationed: the first scorecard teleported six
        // bots onto a point without line of sight every second for the whole window and they never cast.
        bool const off_place = p->GetExactDist(d) > (s.melee ? 12.0f : 40.0f) || (!s.melee && p->GetExactDist(d) < 10.0f) || !d->IsWithinLOSInMap(p) ||
                               std::fabs(d->GetPositionZ() - p->GetPositionZ()) > 3.0f;
        if (!p->HasUnitState(UNIT_STATE_CASTING) && off_place && (p->GetExactDist(d) > 40.0f || s.replaced == 0 || gr.measure_s >= s.last_replace_s + 10u))
        {
            std::size_t slot = 0;
            for (GroupSub const& o : gr.subs)
            {
                if (&o == &s)
                    break;
                if (o.melee == s.melee)
                    ++slot;
            }
            // A bot that stands near the ring point it was put on and does not see the dummy from there: that point is not used for it again (243:
            // the elemental put back onto the same blind point every 10 s for 3 runs). The check from the point sees over what stands in the way.
            if (s.has_last_spot && !d->IsWithinLOSInMap(p) && p->GetExactDist2d(s.last_spot) <= 6.0f)
            {
                s.blind_spots.push_back(s.last_spot);
                s.has_last_spot = false;
                TC_LOG_INFO("playerbot.v2", "[dmg_group] {} sees no dummy from its ring point ({:.1f},{:.1f},{:.1f}) (stands at ({:.1f},{:.1f},{:.1f})): the point is not used for it again ({} so far)",
                            s.name, s.last_spot.GetPositionX(), s.last_spot.GetPositionY(), s.last_spot.GetPositionZ(), p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), s.blind_spots.size());
            }
            if (s.replaced < 3 && !s.melee)
            {
                // Why a ranged bot stood in melee reach: what moved it and what its engage saw (first three times per bot).
                BotAI* ai = ::Playerbot::Services::Registry().ai(p->GetGUID().GetCounter());
                Item const* rw = p->GetWeaponForAttack(RANGED_ATTACK, true);
                TC_LOG_INFO("playerbot.v2", "[dmg_group] {} left its place: dist {:.1f} dz {:.1f} (replace: too far {}, too close {}, no line of sight {}, height {}) last_rule {} generator {} ranged {} needs_ammo {} ammo {} count {} in_los {}", s.name, p->GetExactDist(d),
                            p->GetPositionZ() - d->GetPositionZ(), int(p->GetExactDist(d) > 40.0f), int(p->GetExactDist(d) < 10.0f), int(!d->IsWithinLOSInMap(p)), int(std::fabs(d->GetPositionZ() - p->GetPositionZ()) > 3.0f),
                            ai && ai->last_rule_fired() ? ai->last_rule_fired() : "-", int(p->GetMotionMaster()->GetCurrentMovementGeneratorType()), rw ? rw->GetEntry() : 0u,
                            int(p->NeedsAmmo()), p->GetAmmoId(), p->GetAmmoId() ? p->GetItemCount(p->GetAmmoId()) : 0u, int(d->IsWithinLOSInMap(p)));
            }
            GroupPlaceOne(p, d, s, slot, std::max<std::size_t>(std::size_t(std::count_if(gr.subs.begin(), gr.subs.end(), [&](GroupSub const& o) { return o.melee == s.melee; })), 1), s.replaced < 3 && !s.melee);
            ++s.replaced;
            s.last_replace_s = gr.measure_s;
            if (p->GetClass() == CLASS_HUNTER)
                s.trace_s = 8;
        }
        if (s.trace_s > 0)
        {
            // What walks a hunter from its ring point back into melee (159: put back at 24 y, 3.5 y from the dummy by the next check, in one leg
            // for one spec and the next leg for the others): its distance, movement and last rule each second after the placement.
            --s.trace_s;
            BotAI* tai = ::Playerbot::Services::Registry().ai(p->GetGUID().GetCounter());
            Position dest = p->movespline->Finalized() ? p->GetPosition() : Position(p->movespline->FinalDestination().x, p->movespline->FinalDestination().y, p->movespline->FinalDestination().z);
            TC_LOG_INFO("playerbot.v2", "[dmg_group] {} {} s after its placement: dist {:.1f} moving {} generator {} walks to dist {:.1f} from the dummy, victim is the dummy {} melee swing {} last_rule {} manual {} owner_command {}", s.name, 8u - s.trace_s,
                        p->GetExactDist(d), int(!p->movespline->Finalized()), int(p->GetMotionMaster()->GetCurrentMovementGeneratorType()), std::sqrt(dest.GetExactDistSq(d->GetPosition())), int(p->GetVictim() == d),
                        int(p->HasUnitState(UNIT_STATE_MELEE_ATTACKING)), tai && tai->last_rule_fired() ? tai->last_rule_fired() : "-", tai ? int(tai->is_manual_mode(GameTime::GetGameTimeMS())) : -1,
                        tai ? int(tai->owner_command()) : -1);
        }
        if (p->GetVictim() != d && !p->HasUnitState(UNIT_STATE_CASTING) && !(gr.support && s.role == 1))   // a healer heals, it does not attack
        {
            if (gr.support && !(gr.tank < gr.subs.size() && &s == &gr.subs[gr.tank]))
                EngageOrder(p, d);   // a dps whose threat hold dropped its victim keeps its hold
            else
                Engage(p, d);
        }
        if (p->GetPowerType() == POWER_RAGE)
        {
            std::uint32_t const rage = p->GetPower(POWER_RAGE) / 10u;
            ++s.rage_samples;
            s.rage_sum += rage;
            s.rage_ge30_s += rage >= 30u;
            s.rage_ge50_s += rage >= 50u;
            s.rage_cap_s += rage >= p->GetMaxPower(POWER_RAGE) / 10u;
        }
        if (p->HasUnitState(UNIT_STATE_CASTING))
            ++s.casting_s;
        else if (p->GetVictim() == d)
            ++s.attacking_s;
        else
            ++s.idle_s;
        if (s.melee ? !p->IsWithinMeleeRange(d) : p->GetExactDist(d) < 10.0f)
            ++(s.melee ? s.melee_far_s : s.ranged_close_s);
        if (float const edge = p->GetDistance(d); s.closest < 0.0f || edge < s.closest)
            s.closest = edge;
        // Where each bot stands against the dummy, 10 s and 60 s in (group 14: ret 103, elemental 0 and the beast master 147 stood 3.8 y from the dummy and
        // 3.8 above it, and the scorecard log never named the dummy's position or the bot's, so the cause of the stand was not readable).
        if (gr.measure_s == 10 || gr.measure_s == 60)
        {
            BotAI* ai = ::Playerbot::Services::Registry().ai(p->GetGUID().GetCounter());
            TC_LOG_INFO("playerbot.v2", "[dmg_group] {} at {}s: bot ({:.1f},{:.1f},{:.1f}) ground {:.1f} dummy ({:.1f},{:.1f},{:.1f}) dist {:.1f} dist2d {:.1f} melee_range {} in_los {} facing_dummy {} dummy_facing_bot {} swing_ready {} victim {} casting {} generator {} last_rule {}",
                        s.name, gr.measure_s, p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), p->GetMap()->GetHeight(p->GetPhaseShift(), p->GetPositionX(), p->GetPositionY(), p->GetPositionZ() + 2.0f, true, 6.0f),
                        d->GetPositionX(), d->GetPositionY(), d->GetPositionZ(), p->GetExactDist(d), p->GetExactDist2d(d), int(p->IsWithinMeleeRange(d)), int(d->IsWithinLOSInMap(p)),
                        int(p->HasInArc(float(M_PI), d)), int(d->HasInArc(float(M_PI), p)), int(p->isAttackReady(BASE_ATTACK)), p->GetVictim() ? p->GetVictim()->GetName() : "-",
                        int(p->HasUnitState(UNIT_STATE_CASTING)), int(p->GetMotionMaster()->GetCurrentMovementGeneratorType()), ai && ai->last_rule_fired() ? ai->last_rule_fired() : "-");
            // Every gate between a melee bot's victim and its swing (Unit::DoMeleeAttackIfReady, Unit::AttackerStateUpdate): the 239 arms bot held the
            // dummy as its victim in melee range and facing it for the whole first run, out of combat, with no swing, no rage and no cast, and the line
            // above names none of the gates, so the one that held it was not readable.
            if (s.melee)
            {
                std::string negative;
                for (auto const& [auraId, app] : p->GetAppliedAuras())
                    if (!app->IsPositive())
                        negative += fmt::format("{}{} {}", negative.empty() ? "" : ", ", auraId, app->GetBase()->GetSpellInfo()->SpellName->Str[LOCALE_enUS]);
                TC_LOG_INFO("playerbot.v2", "[dmg_group] {} swing gates at {}s: melee_attacking {} stunned {} confused {} fleeing {} charging {} pacified {} disable_attacking {} disable_autoattack {} "
                            "valid_target {} los_from_bot {} in_combat {} attack_timer {} mounted {} melee_spell {} negative auras: {}",
                            s.name, gr.measure_s, int(p->HasUnitState(UNIT_STATE_MELEE_ATTACKING)), int(p->HasUnitState(UNIT_STATE_STUNNED)), int(p->HasUnitState(UNIT_STATE_CONFUSED)),
                            int(p->HasUnitState(UNIT_STATE_FLEEING)), int(p->HasUnitState(UNIT_STATE_CHARGING)),
                            int(p->HasUnitFlag(UNIT_FLAG_PACIFIED)), int(p->HasAuraType(SPELL_AURA_DISABLE_ATTACKING_EXCEPT_ABILITIES)), int(p->HasAuraType(SPELL_AURA_DISABLE_AUTOATTACK)),
                            int(p->IsValidAttackTarget(d)), int(p->IsWithinLOSInMap(d)), int(p->IsInCombat()), p->getAttackTimer(BASE_ATTACK), int(p->IsMounted()),
                            p->GetCurrentSpell(CURRENT_MELEE_SPELL) ? p->GetCurrentSpell(CURRENT_MELEE_SPELL)->GetSpellInfo()->Id : 0u, negative.empty() ? "none" : negative);
            }
        }
        Powers const pt = p->GetPowerType();
        if (gr.support && s.has_mana)
        {
            // The mana a healer starts with is all it gets: how far it falls, and the seconds it has under 5 %, are what the scorecard reports.
            std::uint32_t const pct = std::uint32_t(100u * p->GetPower(POWER_MANA) / std::max<std::uint32_t>(p->GetMaxPower(POWER_MANA), 1u));
            s.mana_min = std::min(s.mana_min, pct);
            s.mana_end = pct;
            s.oom_s += pct < 5u;
        }
        else if (pt == POWER_MANA && p->GetMaxPower(pt) && p->GetPower(pt) * 20u < p->GetMaxPower(pt) * 9u)
        {
            p->SetPower(pt, p->GetMaxPower(pt));
            ++s.mana_refills;
        }
    }
    if (d->GetHealthPct() < 60.0)
        d->SetFullHealth();
    // The raid keeps its buffs and its debuffs up for the whole fight (a Battle Shout lasts two minutes, the window began 80 s after the first buff).
    if (gr.measure_s % 15 == 0)
        GroupRaidBuffs();
    for (GroupSub const& s : gr.subs)
        if (s.scored && (!gr.support || s.role == 0))
            if (Player* p = ObjectAccessor::FindConnectedPlayer(s.guid); p && p->IsAlive())
                SetWizardOil(p, true);
    if (gr.support)
        SupportSecond(d);
    else if (!d->HasAura(kSunderArmorSpell))
    {
        TC_LOG_INFO("playerbot.v2", "[dmg_group] Sunder Armor was gone from the dummy {} s into the measurement: the raid debuffs go back on", gr.measure_s);
        GroupRaidDebuffs(d);
    }

    if (gr.measure_s % 30 == 0)
    {
        std::string where;
        for (GroupSub const& s : gr.subs)
            if (Player* p = ObjectAccessor::FindConnectedPlayer(s.guid); p && p->IsInWorld())
                where += fmt::format("{}{}={:.0f}{}", where.empty() ? "" : " ", s.label, p->GetExactDist(d), s.replaced ? fmt::format("(+{})", s.replaced) : std::string());
        TC_LOG_INFO("playerbot.v2", "[dmg_group] {} s distances to the dummy (replaced): {}", gr.measure_s, where);
        double total = 0.0;
        {
            std::lock_guard<std::mutex> lock(g_mu);
            for (auto const& m : g_group_aggs)
                for (auto const& kv : m)
                    total += double(kv.second.TotalDamage());
        }
        TC_LOG_INFO("playerbot.v2", "[dmg_group] {} s: raid {:.0f} dps so far", gr.measure_s, total / double(gr.measure_s));
    }
    if (gr.measure_s >= gr.leg_s)
    {
        if (gr.support)
            ReportSupport(d);
        else
            ReportGroup(d);
        if (gr.run < gr.runs)
        {
            // Another run on the same buffs: everyone back to full, back in place, with fresh cooldowns, and the dummy's debuffs renewed.
            // (A support scorecard's boss starts each run on the tank with its threat cleared: SupportBossStartOnTank.)
            ++gr.run;
            TC_LOG_INFO("playerbot.v2", "[dmg_group] run {} of {} done, starting the next", gr.run - 1, gr.runs);
            for (GroupSub const& s : gr.subs)
                if (Player* p = ObjectAccessor::FindConnectedPlayer(s.guid); p && p->IsAlive())
                {
                    p->CombatStop(true);
                    p->GetSpellHistory()->ResetAllCooldowns();
                    TopUp(p);
                    RenewOwnKit(p);
                }
            d->SetFullHealth();
            GroupPlaceAll(d);
            GroupRaidBuffs();
            if (!gr.support)
                GroupRaidDebuffs(d);
            else
                SupportBossStartOnTank(d);
            gr.phase = GroupPhase::Warm;
            gr.t = 0;
            return false;
        }
        ReportRuns();
        End();
        return true;
    }
    return false;
}

void GroupEnd()
{
    if (g.kind != Kind::Group)
        return;
    g_group_mode = false;
    g_support_mode = false;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_group_heals.clear();
        g_group_taken.clear();
        g_group_pulse.clear();
        g_group_index.clear();
        g_group_aggs.clear();
        g_group_casts.clear();
        g_group_timeline.clear();
        g_group_real_ms.clear();
    }
    for (GroupSub const& s : gr.subs)
        if (Player* p = ObjectAccessor::FindConnectedPlayer(s.guid))
        {
            ::Playerbot::V2::Fleet::BotPopulationManager::SetGearModeOverride(p->GetGUID().GetCounter(), -1);
            p->CombatStop(true);
            if (!gr.dummy.IsEmpty())
                if (Creature* d = ObjectAccessor::GetCreature(*p, gr.dummy))
                    d->DespawnOrUnsummon();
        }
    gr.subs.clear();
}

} // namespace

// ---- Hooks ----

bool Active() { return g_measuring.load(std::memory_order_relaxed); }

// Any thread: the rules ask it before they give up on a target.
bool IsDummy(ObjectGuid const& guid)
{
    if (!g_measuring.load(std::memory_order_relaxed) && !g_group_mode.load(std::memory_order_relaxed))
        return false;
    std::lock_guard<std::mutex> lock(g_mu);
    return !guid.IsEmpty() && guid == g_dummy;
}

bool IsSupportBoss(ObjectGuid const& guid)
{
    if (!g_group_mode.load(std::memory_order_relaxed) || !g_support_mode.load(std::memory_order_relaxed))
        return false;
    std::lock_guard<std::mutex> lock(g_mu);
    return !guid.IsEmpty() && guid == g_dummy;
}

Creature* SpawnProcDummy(Player* p, float distance, std::string& error)
{
    return SpawnDummy(p, distance, kDummyArmor, error);
}

// A spot to measure from: ground that is flat for `distance` y in a direction, with line of sight along it, so the dummy can stand on the bot's
// own level (the Stormwind staging spot is on a ledge: the ground 15 y ahead is 11 y lower, no dummy spot had line of sight, and the casters
// ran 33 and 14 dps with LINE_OF_SIGHT failures, 2026-10-19). The bot's own spot first, then rings around it on the same level.
bool FindOpenSpot(Player* p, float distance, float& x, float& y, float& z, float& o)
{
    Map* const map = p->GetMap();
    if (!map)
        return false;
    PhaseShift const& ps = p->GetPhaseShift();
    float const z0 = p->GetPositionZ();
    auto const ground = [&](float gx, float gy, float ref) { return map->GetHeight(ps, gx, gy, ref + 1.0f, true, 4.0f); };
    auto const flat_along = [&](float bx, float by, float bz, float angle)
    {
        for (float const f : { 0.25f, 0.5f, 0.75f, 1.0f })
        {
            float const h = ground(bx + std::cos(angle) * distance * f, by + std::sin(angle) * distance * f, bz);
            if (h <= INVALID_HEIGHT + 1.0f || std::fabs(h - bz) > 1.0f)
                return false;
        }
        return map->isInLineOfSight(ps, bx, by, bz + 1.5f, bx + std::cos(angle) * distance, by + std::sin(angle) * distance, bz + 1.5f,
                                    LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing);
    };
    float const pi = 3.14159265f;
    for (float radius = 0.0f; radius <= 48.0f; radius += 6.0f)
    {
        int const steps = radius == 0.0f ? 1 : 12;
        for (int i = 0; i < steps; ++i)
        {
            float const around = 2.0f * pi * float(i) / float(steps);
            float const bx = p->GetPositionX() + std::cos(around) * radius, by = p->GetPositionY() + std::sin(around) * radius;
            float const bz = radius == 0.0f ? z0 : ground(bx, by, z0);
            if (bz <= INVALID_HEIGHT + 1.0f || std::fabs(bz - z0) > 3.0f)
                continue;
            for (int k = 0; k < 8; ++k)
            {
                float const angle = 2.0f * pi * float(k) / 8.0f;
                if (flat_along(bx, by, bz, angle))
                {
                    x = bx; y = by; z = bz; o = angle;
                    return true;
                }
            }
        }
    }
    return false;
}

void NoteEnergize(Unit* target, std::uint32_t spell_id, std::int32_t gained, std::uint8_t power)
{
    if (!g_energy_on.load(std::memory_order_relaxed) || power != POWER_MANA || !target)
        return;
    std::lock_guard<std::mutex> lock(g_mu);
    if (target->GetGUID() == g_subject)
        g_energy[spell_id] += gained;
}

void NoteCast(Unit* caster, std::uint32_t spell_id)
{
    if (!g_measuring.load(std::memory_order_relaxed) || !caster)
        return;
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_group_mode.load(std::memory_order_relaxed))
    {
        if (auto it = g_group_index.find(caster->GetGUID()); it != g_group_index.end() && it->second < g_group_casts.size())
        {
            ++g_group_casts[it->second][spell_id];
            if (it->second < g_group_timeline.size())
            {
                g_group_timeline[it->second].emplace_back(GameTime::GetGameTimeMS(), spell_id);
                // The cast the core just started: its time with the talents and auras applied (Improved Frostbolt, Bane, haste), not the data's.
                std::int32_t real = 0;
                if (Spell const* cur = caster->GetCurrentSpell(CURRENT_GENERIC_SPELL))
                    real = cur->GetCastTime();
                else if (Spell const* chan = caster->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
                    real = chan->GetSpellInfo()->GetDuration();
                if (it->second < g_group_real_ms.size())
                    g_group_real_ms[it->second].push_back(real);
            }
        }
        return;
    }
    if (caster->GetGUID() == g_subject)
        ++g_casts[spell_id];
}

void NoteHeal(Unit* healer, Unit* /*target*/, std::uint32_t spell_id, std::uint32_t raw, std::uint32_t effective)
{
    if (!g_measuring.load(std::memory_order_relaxed) || !g_support_mode.load(std::memory_order_relaxed) || !healer)
        return;
    Unit* owner = healer->GetCharmerOrOwnerOrSelf();   // a totem heals for its shaman
    if (!owner)
        return;
    std::lock_guard<std::mutex> lock(g_mu);
    auto const it = g_group_index.find(owner->GetGUID());
    if (it == g_group_index.end() || it->second >= g_group_heals.size())
        return;
    HealAgg& a = g_group_heals[it->second][spell_id];
    a.raw += raw;
    a.effective += effective;
    ++a.heals;
}

void NoteOutcome(Unit* attacker, Unit* victim, std::uint32_t spell_id, std::uint8_t outcome, std::uint32_t damage)
{
    if (!g_measuring.load(std::memory_order_relaxed))
        return;
    std::lock_guard<std::mutex> lock(g_mu);
    if (int const ti = TakenIndex(attacker, victim); ti >= 0 && std::size_t(ti) < g_group_taken.size())
    {
        Outcome const taken = Outcome(std::min<std::uint8_t>(outcome, std::uint8_t(kOutcomeCount - 1)));
        SpellAgg& agg = g_group_taken[std::size_t(ti)][0];
        agg.spell_id = 0;
        if (!DealsDamage(taken))
        {
            agg.AddOutcome(taken, 0);
            t_pending.valid = false;
            return;
        }
        t_pending.group_idx = -3;   // marks a pending swing at a raider
        t_pending.valid = true;
        t_pending.spell = spell_id;
        t_pending.victim = victim->GetGUID();
        t_pending.outcome = taken;
        return;
    }
    bool pet = false;
    int const idx = Resolve(attacker, victim, pet);
    if (idx == -2)
        return;
    Outcome const o = Outcome(std::min<std::uint8_t>(outcome, std::uint8_t(kOutcomeCount - 1)));
    std::uint32_t const key = spell_id | (pet ? kPetFlag : 0u);
    if (!DealsDamage(o))
    {
        AggsFor(idx)[key].spell_id = spell_id;
        AggsFor(idx)[key].AddOutcome(o, 0);
        t_pending.valid = false;
        return;
    }
    t_pending.group_idx = idx;
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
    if (int const ti = TakenIndex(attacker, victim); ti >= 0 && std::size_t(ti) < g_group_taken.size())
    {
        // The boss hit a raider: a swing the outcome hook announced, or a scripted pulse (no outcome before it).
        if (t_pending.valid && t_pending.victim == victim->GetGUID() && t_pending.group_idx == -3)
            g_group_taken[std::size_t(ti)][0].AddOutcome(t_pending.outcome, std::uint32_t(amount));
        else
            g_group_pulse[std::size_t(ti)] += std::uint32_t(amount);
        t_pending.valid = false;
        return;
    }
    bool pet = false;
    int const idx = Resolve(attacker, victim, pet);
    if (idx == -2)
        return;
    std::uint32_t const key = spell_id | (pet ? kPetFlag : 0u);
    bool const direct = t_pending.valid && t_pending.spell == spell_id && t_pending.victim == victim->GetGUID() && t_pending.group_idx == idx;
    SpellAgg& agg = direct && t_pending.watched ? g_aggs_watched[key] : AggsFor(idx)[key];
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

// The threat audit: a level 63 dummy with a known amount of the subject's threat on it, then the class's threat reducer. The drop is measured on the
// dummy's own threat list and set against what vanilla's value would have taken off: Forever's client data carries the reducers at 2-5x the vanilla
// amounts (Fade, Feint, Disengage, Cower), and a threat drop is judged by what it does to the caster's standing on a mob, not by the number.
constexpr char const* kThreatSpellByClass(std::uint8_t cls)
{
    return cls == 4 ? "Feint" : cls == 5 ? "Fade" : cls == 3 ? "Disengage" : cls == 11 ? "Cower" : nullptr;
}
constexpr float kThreatLevels[] = { 500.f, 5000.f };

// What vanilla 1.12 takes off: the effect 63 (threat) or aura 103 (total threat) amount of the spell, from the VMaNGOS inputs.
bool VanillaThreatAmount(std::uint8_t cls, std::uint32_t spell, int32 level, double& amount)
{
    for (SpellCheck::Expected const& e : SpellCheck::kExpected)
        if (e.cls == cls && e.spell == spell && (e.effect == 63 || e.aura == 103))
        {
            amount = -SpellCheck::VanillaRange(e, level).mean;
            return true;
        }
    return false;
}

bool ThreatStep(Player* p, Creature* d, std::string& failure)
{
    ThreatAudit& t = g.threat;
    if (t.level >= std::size(kThreatLevels))
    {
        failure = g.failure;
        return true;
    }
    ++t.t;
    if (t.phase == 0)
    {
        if (!t.spell_id)
        {
            char const* name = kThreatSpellByClass(g.cls);
            t.spell_id = name ? FindKnownSpell(p, name) : 0u;
            if (!t.spell_id)
            {
                g.failure += fmt::format("{} has no threat reducer in the spellbook; ", g.cls_name);
                TC_LOG_INFO("playerbot.v2", "[threat_audit] {}: no threat reducer known", g.cls_name);
                t.level = std::size(kThreatLevels);
                return false;
            }
        }
        SpellInfo const* info = sSpellMgr->GetSpellInfo(t.spell_id, DIFFICULTY_NONE);
        p->GetSpellHistory()->ResetAllCooldowns();
        if (p->IsMounted())
            p->RemoveAurasByType(SPELL_AURA_MOUNTED);
        if (info)
            EnsureForm(p, info);
        TopUp(p);
        p->RemoveAurasDueToSpell(t.spell_id);
        d->RemoveAllAuras();
        d->SetFullHealth();
        d->GetThreatManager().ResetAllThreat();
        d->SetInCombatWith(p);
        p->SetInCombatWith(d);
        d->GetThreatManager().AddThreat(p, kThreatLevels[t.level], nullptr, true, true);
        t.before = d->GetThreatManager().GetThreat(p);
        p->SetFacingToObject(d);
        ::Playerbot::API api(p);
        ::Playerbot::Result const r = api.cast_spell(t.spell_id, info && info->NeedsExplicitUnitTarget() ? d->GetGUID() : p->GetGUID());
        if (r != ::Playerbot::Result::Ok)
        {
            TC_LOG_INFO("playerbot.v2", "[threat_audit] {} ({}) cast refused at threat {}: {}, attempt {}", SpellNameOf(t.spell_id), t.spell_id, t.before, CastFailText(api.last_cast_fail()), t.attempt);
            if (++t.attempt > 3)
            {
                g.failure += fmt::format("{} ({}) refused: {}; ", SpellNameOf(t.spell_id), t.spell_id, CastFailText(api.last_cast_fail()));
                ++t.level;
                t.attempt = 1;
            }
            t.t = 0;
            return false;
        }
        t.phase = 1;
        t.t = 0;
        return false;
    }
    if (t.t < 3)   // the cast, the global cooldown and a periodic application
        return false;
    float const after = d->GetThreatManager().GetThreat(p);
    float const drop = t.before - after;
    double vanilla = 0.0;
    bool const have_vanilla = VanillaThreatAmount(g.cls, t.spell_id, int32(p->GetLevel()), vanilla);
    SpellInfo const* info = sSpellMgr->GetSpellInfo(t.spell_id, DIFFICULTY_NONE);
    double client = 0.0;
    if (info)
        for (SpellEffectInfo const& e : info->GetEffects())
            if (e.Effect == SPELL_EFFECT_THREAT || e.ApplyAuraName == SPELL_AURA_MOD_TOTAL_THREAT)
            {
                client = e.CalcValue(p);
                break;
            }
    double const drop_pct = t.before > 0.f ? 100.0 * drop / t.before : 0.0;
    double const vanilla_pct = t.before > 0.f && have_vanilla ? 100.0 * std::min<double>(vanilla, t.before) / t.before : 0.0;
    TC_LOG_INFO("playerbot.v2", "[threat_audit] {} ({}) threat before {:.0f} after {:.0f} dropped {:.0f} = {:.1f} % of the caster's threat; client amount {:.0f}, vanilla amount {}{:.0f} would drop {:.1f} %",
                SpellNameOf(t.spell_id), t.spell_id, t.before, after, drop, drop_pct, client, have_vanilla ? "" : "(no vanilla row) ", vanilla, vanilla_pct);
    AppendFile(fmt::format("| threat audit | {} ({}) | threat {:.0f} | after {:.0f} | dropped {:.0f} ({:.1f} %) | client {:.0f} | vanilla {:.0f} ({:.1f} %) |", SpellNameOf(t.spell_id), t.spell_id,
                           t.before, after, drop, drop_pct, client, vanilla, vanilla_pct));
    if (drop <= 0.f && t.attempt < 3)
    {
        ++t.attempt;   // a resisted or dodged cast lowers nothing: retry before calling it broken
        t.phase = 0;
        t.t = 0;
        return false;
    }
    if (drop <= 0.f)
        g.failure += fmt::format("{} ({}) lowered no threat at {:.0f}; ", SpellNameOf(t.spell_id), t.spell_id, t.before);
    ++t.level;
    t.phase = 0;
    t.t = 0;
    t.attempt = 1;
    return false;
}

// ---- The runner's calls ----

bool Begin(Player* subject, Kind kind, std::uint32_t leg_seconds, std::string& error, std::uint32_t spec)
{
    End();
    // A scenario that plays a class as a given specialization: the rotation reads the override, not the (untouched) talents.
    if (spec && !::Playerbot::Classic::SetSpecOverride(subject, spec))
        TC_LOG_WARN("playerbot.v2", "[dmg_parity] spec override {} refused for {}", spec, subject->GetName());
    g = State{};
    g.tank_spec = spec == 73;   // ChrSpecialization: warrior Protection
    g.no_reference = subject->GetClass() == CLASS_WARRIOR && spec != 72;   // Arms (71, or the bot's own talents): see no_reference
    g.active = true;
    g.kind = kind;
    g.subject = subject->GetGUID();
    g.cls = subject->GetClass();
    g.cls_name = ClassName(g.cls);
    g.leg_seconds = leg_seconds ? leg_seconds : 180;
    // Playerbot.DamageParity.LegSeconds overrides the scenario's own leg length (180 s); 0 = the scenario's.
    if (int const cfg = sConfigMgr->GetIntDefault("Playerbot.DamageParity.LegSeconds", 0, true); cfg > 0)
        g.leg_seconds = std::uint32_t(cfg);
    if (kind == Kind::Damage)
        g.leg_idx = ConfiguredLegs();
    g.items = SweepItemsFor(kind, g.cls);
    if ((kind == Kind::Finisher || kind == Kind::SpellCheck) && g.items.empty())
    {
        error = fmt::format("{} has no combo, rage or energy scaled spell in the sweep", g.cls_name);
        g.active = false;
        return false;
    }
    // The dummy: close for a melee sweep, further for a caster.
    float const distance = kind == Kind::Finisher || kind == Kind::SpellCheck || kind == Kind::Incoming || (kind == Kind::Threat && (g.cls == 4 || g.cls == 11)) ? 3.0f : kind == Kind::TalentAudit || kind == Kind::Economy ? 18.0f : 15.0f;
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
    else if (kind == Kind::Threat)
    {
        ::Playerbot::V2::Fleet::BotPopulationManager::SetGearModeOverride(subject->GetGUID().GetCounter(), 4);
        ::Playerbot::Services::Population().RegearNow(subject);
        TopUp(subject);
        g.step = Step::Threat;
        AppendFile(fmt::format("\n## {} (threat audit)\n", g.cls_name));
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
        if (kind == Kind::SpellCheck)
            AppendFile(fmt::format("\n## {} (player spell check)\n\n| class | spell | id | verdict | detail |\n|---|---|---|---|---|", g.cls_name));
        if (kind == Kind::TalentAudit)
            LogTalents(subject);
        if (kind == Kind::Economy)
            EconStatic(subject);
    }
    g.step_s = 0;
    return true;
}

bool BeginGroup(std::vector<GroupMember> const& members, std::uint32_t leg_seconds, std::string& error, bool support)
{
    End();
    g = State{};
    g.active = true;
    g.kind = Kind::Group;
    g.cls_name = "Raid";
    gr = GroupRun{};
    gr.leg_s = leg_seconds ? leg_seconds : 120;
    if (int const cfg = sConfigMgr->GetIntDefault("Playerbot.DamageParity.LegSeconds", 0, true); cfg > 0)
        gr.leg_s = std::uint32_t(cfg);
    gr.buff_s = std::uint32_t(std::max(10, sConfigMgr->GetIntDefault("Playerbot.DamageParity.BuffSeconds", 75, true)));
    gr.runs = std::uint32_t(std::max(1, sConfigMgr->GetIntDefault("Playerbot.DamageParity.Runs", 3, true)));
    // The baseline scorecard to compare with: a path, relative to the logs directory unless absolute; "" turns the comparison off.
    gr.baseline_path = sConfigMgr->GetStringDefault("Playerbot.DamageParity.Baseline", support ? "SupportBaseline.tsv" : "DpsBaseline.tsv", true);
    gr.support = support;
    if (support)
    {
        // The boss: a swing of BossSwing every 2 s at the tank, and a pulse of PulseDamage at PulseTargets raiders in turn every PulseSeconds.
        gr.swing = float(std::max(1, sConfigMgr->GetIntDefault("Playerbot.DamageParity.BossSwing", 3500, true)));
        gr.pulse_dmg = float(std::max(0, sConfigMgr->GetIntDefault("Playerbot.DamageParity.PulseDamage", 1200, true)));
        gr.pulse_n = std::uint32_t(std::max(0, sConfigMgr->GetIntDefault("Playerbot.DamageParity.PulseTargets", 6, true)));
        gr.pulse_s = std::uint32_t(std::max(0, sConfigMgr->GetIntDefault("Playerbot.DamageParity.PulseSeconds", 6, true)));
    }
    if (!gr.baseline_path.empty() && gr.baseline_path[0] != '/' && gr.baseline_path.find(':') == std::string::npos)
        gr.baseline_path = sLog->GetLogsDir() + gr.baseline_path;
    Player* first = nullptr;
    for (GroupMember const& m : members)
    {
        if (!m.player)
            continue;
        GroupSub s;
        s.guid = m.player->GetGUID();
        s.name = m.player->GetName();
        s.label = m.label;
        s.cls = m.player->GetClass();
        s.spec = m.spec;
        s.melee = m.melee;
        s.scored = m.scored;
        s.role = m.role;
        if (support && m.role == 2)
            gr.tank = gr.subs.size();
        gr.subs.push_back(std::move(s));
        gr.run_dps.emplace_back();
        gr.run_support.emplace_back();
        if (!first && (support ? m.role == 2 : m.scored))
            first = m.player;
    }
    if (!first)
    {
        error = "the raid has no dps member to summon the dummy next to";
        g.active = false;
        return false;
    }
    Creature* d = SpawnDummy(first, 15.0f, kDummyArmor, error);
    if (!d)
    {
        g.active = false;
        return false;
    }
    ZeroResistances(d);
    {
        Position site;
        std::string why;
        bool const found = FindFlatDummySite(first, site, why);
        TC_LOG_INFO("playerbot.v2", "[dmg_group] dummy site: {}{}", why, found ? fmt::format(" at ({:.1f},{:.1f},{:.1f})", site.GetPositionX(), site.GetPositionY(), site.GetPositionZ()) : std::string(", stays 15 y from the first bot"));
        gr.has_site = found;
        if (found)
        {
            gr.site = site;
            d->NearTeleportTo(site.GetPositionX(), site.GetPositionY(), site.GetPositionZ(), 0.0f);
            d->SetControlled(true, UNIT_STATE_ROOT);   // 228: it walked 10 y off the site to the tank and one bot stood on it
        }
    }
    gr.dummy = d->GetGUID();
    gr.dummy_name = d->GetName();
    // Where the dummy stands, so two scorecards can be compared: it is summoned 15 y from the first dps bot, which is wherever that bot was.
    TC_LOG_INFO("playerbot.v2", "[dmg_group] dummy summoned map {} zone {} area {} at ({:.1f},{:.1f},{:.1f}), ground under it {:.1f}, 15 y from {} at ({:.1f},{:.1f},{:.1f}); in combat {} evading {}",
                d->GetMapId(), d->GetZoneId(), d->GetAreaId(), d->GetPositionX(), d->GetPositionY(), d->GetPositionZ(),
                d->GetMap()->GetHeight(d->GetPhaseShift(), d->GetPositionX(), d->GetPositionY(), d->GetPositionZ() + 2.0f, true, 6.0f),
                first->GetName(), first->GetPositionX(), first->GetPositionY(), first->GetPositionZ(), int(d->IsInCombat()), int(d->IsInEvadeMode()));
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_subject = ObjectGuid::Empty;
        g_dummy = gr.dummy;
        g_watch_aura.clear();
        g_group_index.clear();
        for (std::size_t i = 0; i < gr.subs.size(); ++i)
            g_group_index[gr.subs[i].guid] = i;
        g_support_mode = support;
        g_group_heals.assign(gr.subs.size(), {});
        g_group_taken.assign(gr.subs.size(), {});
        g_group_pulse.assign(gr.subs.size(), 0);
        g_group_aggs.assign(gr.subs.size(), {});
        g_group_casts.assign(gr.subs.size(), {});
        g_group_timeline.assign(gr.subs.size(), {});
        g_group_real_ms.assign(gr.subs.size(), {});
    }
    g_group_mode = true;
    g.step_s = 0;
    return true;
}

bool Step(std::string& failure)
{
    if (!g.active)
        return true;
    if (g.kind == Kind::Group)
        return GroupStep(failure);
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
        d = SpawnDummy(p, g.kind == Kind::Finisher || g.kind == Kind::SpellCheck ? 3.0f : g.kind == Kind::TalentAudit || g.kind == Kind::Economy ? 18.0f : 15.0f,
                       g.kind == Kind::Damage ? CurLeg().armor : kDummyArmor, error);
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
            // The bot's own errands start the moment it has no order: the new leg's regear fills the bags and idle:capital_bag_run walked it off to a
            // vendor for the first minute of the fury t3 leg (153: 22 s idle, 135 dps against 154 busy). The attack order is held from the first tick.
            Engage(p, d);
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
            // The bot's own errands (a bag run, a walk to the vendor) take it off its spot between fights: bring the dummy to where it stands
            // rather than measure a priest that cast into a wall (LINE_OF_SIGHT x30 a leg, 50 dps, 2026-10-20).
            if (!p->HasUnitState(UNIT_STATE_CASTING) && (p->GetExactDist(d) > 30.0f || !d->IsWithinLOSInMap(p) || std::fabs(d->GetPositionZ() - p->GetPositionZ()) > 2.5f))
            {
                Position const dummy_before = d->GetPosition();
                DummyOnBotLevel(p, d, 15.0f);
                // The spots it tries lie at 0, +-90 and 180 degrees from the bot's facing: the 166 and 189 priests found the front blocked, got the dummy
                // at their side and cast nothing for the rest of the leg (SW:P and VE only, 149 dps). A player turns toward the boss it was moved to.
                if (d->GetExactDist(&dummy_before) > 0.5f && !p->HasInArc(float(M_PI), d))
                {
                    TC_LOG_INFO("playerbot.v2", "[dmg_parity] leg={} the dummy was moved to the bot's side ({:.1f} y): the bot turns to it", CurLeg().label, p->GetExactDist(d));
                    p->SetFacingToObject(d);
                }
            }
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
            // The raid keeps Sunder Armor on its boss for the whole fight. The buffed legs of the hunter, priest, shaman and fury 149/151 ended with
            // the dummy at its base armor 3731 and no Sunder among its auras (the legs that kept it were the ones that put their own armor debuff
            // on): put it back and log when it was lost, so the cause can be read off the log.
            if (CurLeg().buffed)
                SetWizardOil(p, true);   // a regear after the setup swaps the weapon and takes the oil with it
            if (CurLeg().sunders > 0 && !d->HasAura(kSunderArmorSpell))
            {
                TC_LOG_INFO("playerbot.v2", "[dmg_parity] leg={} Sunder Armor was gone from the dummy {} s into the measurement (auras left: {}), applied again", CurLeg().label,
                            g.step_s, d->GetAppliedAuras().size());
                SetSunders(p, d, CurLeg().sunders);
                // Whatever took the Sunders took the rest of the boss's debuffs with it (the dummy's auras are cleared as one: 166 priest lost Shadow
                // Weaving x5, Vampiric Embrace, SW:P and Curse of the Elements 10 s in): the whole set goes back.
                SetArmorShred(p, d, CurLeg().buffed);
                SetCasterDebuffs(p, d, CurLeg().buffed);
            }
            // Why a bot that has a victim does not cast: its movement and facing state, 20 s apart (damage_parity 166 priest, 126 dps: 175 of 180 s
            // "attacking", 5 s casting, 90 degrees off its dummy).
            if (g.step_s % 20 == 0)
            {
                float const off = std::fabs(Position::NormalizeOrientation(p->GetOrientation() - p->GetAbsoluteAngle(d)));
                TC_LOG_INFO("playerbot.v2", "[dmg_parity] leg={} bot state {} s in: dist {:.1f} off_deg {:.0f} moving {} flags 0x{:X} spline_finalized {} generator {} casting {} autorepeat {} dummy_evading {} dummy_auras {}",
                            CurLeg().label, g.step_s, p->GetExactDist(d), std::min(off, 2.0f * float(M_PI) - off) * 180.0f / float(M_PI), int(p->isMoving()),
                            uint32(p->GetUnitMovementFlags()), int(p->movespline->Finalized()), int(p->GetMotionMaster()->GetCurrentMovementGeneratorType()),
                            int(p->HasUnitState(UNIT_STATE_CASTING)), int(p->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL) != nullptr), int(d->IsInEvadeMode()),
                            d->GetAppliedAuras().size());
            }
            Powers const pt = p->GetPowerType();
            if (pt == POWER_RAGE)
            {
                std::uint32_t const rage = p->GetPower(POWER_RAGE) / 10u;
                ++g.rage_samples;
                g.rage_sum += rage;
                g.rage_ge30_s += rage >= 30u;
                g.rage_ge50_s += rage >= 50u;
                g.rage_cap_s += rage >= p->GetMaxPower(POWER_RAGE) / 10u;
            }
            {
                std::set<std::string> up;
                for (auto const& [id, app] : p->GetAppliedAuras())
                    if (SpellInfo const* info = app->GetBase()->GetSpellInfo(); info && info->SpellName && app->GetBase()->GetCaster() == p)
                        up.insert(std::string((*info->SpellName)[sWorld->GetDefaultDbcLocale()]));
                for (std::string const& name : up)
                    ++g.aura_s[name];
            }
            // Refilled above every floor a rotation keeps: the casters wand below 35 % mana, so a refill at 30 % left a priest sitting at 31-35 %
            // shooting its wand for a whole leg (T1: 31 s casting, 149 s wanding, 197 dps; 2026-10-20). The legs measure the rotation with mana to spare.
            // A warlock converts health into mana with Life Tap (the rule under test), so it gets no mana refill; a healer stands in for the health it
            // spends (the real raid has one), topped up below 70 %.
            if (p->GetClass() == CLASS_WARLOCK && p->GetHealth() * 10u < p->GetMaxHealth() * 7u)
                p->SetHealth(p->GetMaxHealth());
            if (pt == POWER_MANA && p->GetClass() != CLASS_WARLOCK && p->GetMaxPower(pt) && p->GetPower(pt) * 20u < p->GetMaxPower(pt) * 9u)
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
                                CurLeg().label, p->GetVictim() ? p->GetVictim()->GetName() : std::string("none"), p->IsInCombat(), p->HasUnitState(UNIT_STATE_CASTING));
                // Why nothing lands (druid 161 t3_buffed: attacking for 180 s, never in combat): where the two stand, whether the bot is walking
                // and under which generator, whether it faces the dummy, and the states that stop a swing.
                if (silent)
                    if (Creature* dd = DummyCreature(p))
                        TC_LOG_INFO("playerbot.v2", "[dmg_parity] {} leg {}: bot ({:.1f},{:.1f},{:.1f}) dummy ({:.1f},{:.1f},{:.1f}) dist {:.1f} melee_range {} facing_dummy {} generator {} spline_left_ms {} dummy_victim {} form {} rooted {} stunned {} melee_state {} dummy_alive {} can_attack {}",
                                    g.cls_name, CurLeg().label, p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), dd->GetPositionX(), dd->GetPositionY(),
                                    dd->GetPositionZ(), p->GetExactDist(dd), p->IsWithinMeleeRange(dd), p->HasInArc(float(M_PI), dd),
                                    int(p->GetMotionMaster()->GetCurrentMovementGeneratorType()), p->movespline->timeRemaining(),
                                    dd->GetVictim() ? dd->GetVictim()->GetName() : std::string("none"), int(p->GetShapeshiftForm()), p->HasUnitState(UNIT_STATE_ROOT),
                                    p->HasUnitState(UNIT_STATE_STUNNED), p->HasUnitState(UNIT_STATE_MELEE_ATTACKING), dd->IsAlive(), p->IsValidAttackTarget(dd));
            }
            if (g.step_s >= g.leg_seconds)
            {
                ReportLeg(p);
                p->CombatStop(true);
                // The three reference legs (none, t1, t3) are the default: the sensitivity legs (spell power, attack power, armor) run only when
                // asked for (Playerbot.DamageParity.AllLegs = 1) or when the class already failed a check. A Playerbot.DamageParity.Legs list is run as given.
                std::size_t const legs_to_run = !g.leg_idx.empty() ? LegTotal() : (sConfigMgr->GetBoolDefault("Playerbot.DamageParity.AllLegs", false, true) || !g.failure.empty()) ? kLegCount : kReferenceLegCount;
                if (++g.leg >= legs_to_run)
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
            if (g.kind == Kind::SpellCheck && SpellCheckStuck(p))
                return false;
            // A shot can leave Auto Shot repeating, which keeps the casting state up for good (the first 0.1.30 run: the hunter check
            // never got past its first spell and timed out). A player's next button press goes out anyway.
            if (g.kind == Kind::SpellCheck && p->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
                p->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
            if (p->HasUnitState(UNIT_STATE_CASTING) || p->GetSpellHistory()->HasGlobalCooldown(sSpellMgr->GetSpellInfo(g.cast_spell_id, DIFFICULTY_NONE)))
                return false;
            if (!SweepCastOnce(p, d))
            {
                if (g.cast_failures >= 8 && !g.cast_ok)
                {
                    bool const needs_item = g.last_fail.find(fmt::format("code {}", std::uint32_t(::Playerbot::API::CastRejectNoEquippedItem))) != std::string::npos;
                    if (g.kind == Kind::SpellCheck && needs_item)
                    {
                        // Shield Slam with the two-hander of the test gear: the spell needs an item a player of another build carries.
                        ++g.check_skip;
                        SpellCheckRow(g.items[g.item], g.cast_spell_id, "SKIP", "needs an equipped item the test gear does not have (a shield, a wand)");
                    }
                    else if (g.kind == Kind::SpellCheck)
                    {
                        ++g.check_fail;
                        SpellCheckRow(g.items[g.item], g.cast_spell_id, "FAIL", fmt::format("reason=\"refused 8 times: {}\" dist={:.1f} caster=({:.1f},{:.1f},{:.1f} map {}) dummy=({:.1f},{:.1f},{:.1f} map {}) teleporting={}",
                                      g.last_fail, p->GetExactDist(d), p->GetPositionX(), p->GetPositionY(), p->GetPositionZ(), p->GetMapId(),
                                      d->GetPositionX(), d->GetPositionY(), d->GetPositionZ(), d->GetMapId(), p->IsBeingTeleported()));
                    }
                    if (!(g.kind == Kind::SpellCheck && needs_item))
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
            g.cast_done_s = UINT32_MAX;
            return false;
        }
        case Step::SweepWait:
        {
            SweepItem const& it = g.items[g.item];
            if (g.kind == Kind::SpellCheck && SpellCheckStuck(p))
                return false;
            if (g.kind == Kind::SpellCheck && d->IsEvadingAttacks())
                ++g.evade_ticks;
            if (g.kind == Kind::SpellCheck && p->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
                p->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
            if (p->HasUnitState(UNIT_STATE_CASTING))
                return false;
            if (g.kind == Kind::SpellCheck && g.cast_expected_ms > 0)
            {
                // Seen once the casting state has gone: a cast that ended more than a second before its cast time was cut short.
                std::uint32_t const elapsed = GameTime::GetGameTimeMS() - g.cast_begin_ms;
                if (elapsed + 1100u < std::uint32_t(g.cast_expected_ms))
                    ++g.casts_cut;
                else
                    ++g.casts_completed;
                if (p->GetHealth() < p->GetMaxHealth())
                    g.caster_hp_lost += std::uint32_t(p->GetMaxHealth() - p->GetHealth());
                g.power_spent += g.power_before - std::int64_t(p->GetPower(p->GetPowerType()));
                g.cast_expected_ms = 0;
            }
            // A missile (Fireball, Frostbolt, Pyroblast, Soul Fire, Shadow Bolt) is still the caster's current spell while it flies
            // (SPELL_STATE_LAUNCHED); the slot is let go once the hit has been dealt. The settle time below counted from the cast START:
            // a cast of 3 s or 6 s, whole runner ticks, was read in the tick its casting state went, before its missile arrived, and the
            // next cast's reset (full health, tally cleared) threw the late hit away (0.1.30 runs 1 to 5: mana spent, full cast time,
            // nothing recorded; the 2.5 s Shadow Bolt ended mid-tick and passed). The sample waits for the hit, then settles a tick.
            if (Spell const* flying = p->GetCurrentSpell(CURRENT_GENERIC_SPELL); flying && flying->getState() == SPELL_STATE_LAUNCHED && g.step_s < 40)
                return false;
            if (g.cast_done_s == UINT32_MAX)
                g.cast_done_s = g.step_s;
            std::uint32_t const since_done = g.step_s - g.cast_done_s;
            // A damage over time runs its course (24 s at most); a direct hit or an aura is read a moment after the cast.
            bool const dot = it.measure == Measure::DotTotal;
            bool const gone = !d->HasAura(g.cast_spell_id);
            // An aura is read after a second, not two: Kidney Shot at one point lasts two seconds, and 139 read it just after it had run out (no aura, not immune)
            int const settle = it.measure == Measure::AuraDuration ? 1 : 2;
            // A direct hit is read at least one tick after the hit landed (an aura's duration keeps the read of 1 s after the cast).
            std::uint32_t const after_done = it.measure == Measure::AuraDuration ? 0u : 1u;
            // A ground effect (Consecration) puts no aura on the dummy until its first tick: the spell check gives it 6 s.
            int const dot_min = g.kind == Kind::SpellCheck ? 6 : 2;
            if (dot ? (!gone ? g.step_s < 26 : g.step_s < std::uint32_t(dot_min)) : (g.step_s < std::uint32_t(settle) || since_done < after_done))
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
            if (g.kind == Kind::SpellCheck && SpellCheckWantsFarRetry(p, d))
            {
                // Nothing at all landed from 3 yd: the same five casts again from 15 yd (the distance the damage legs cast from), so the
                // row tells a close-range failure from a spell that never lands.
                g.far = true;
                g.check_samples.clear();
                g.check_seen.clear();
                g.dummy_lost = 0.0;
                g.cast_ok = g.cast_failures = 0;
                g.casts_completed = g.casts_cut = 0;
                g.power_spent = 0;
                g.item_started_ms = GameTime::GetGameTimeMS();
                g.rep = 0;
                g.point = 0;
                g.series.clear();
                g_measuring = true;
                g.step = Step::SweepCast;
                g.step_s = 0;
                return false;
            }
            if (g.kind == Kind::SpellCheck)
                SpellCheckItemDone(p, d);
            else
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
        case Step::Threat:
            if (ThreatStep(p, d, failure))
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
    GroupEnd();
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_subject = ObjectGuid::Empty;
        g_dummy = ObjectGuid::Empty;
        g_watch_aura.clear();
        g_reverse = false;
    }
    if (!g.active)
        return;
    if (g.kind == Kind::SpellCheck)
        TC_LOG_INFO("playerbot.v2", "[spell_check] summary class={} pass={} fail={} skip={} failed=[{}] verdict={}", g.cls_name, g.check_pass, g.check_fail, g.check_skip,
                    g.check_failed, g.check_fail ? "FAIL" : "PASS");
    if (Player* p = Subject())
    {
        for (std::uint32_t id : g.taught)
            p->RemoveSpell(id, false, false, true);
        g.taught.clear();
        ApplyBonuses(p, 0, 0);
        SetRaidBuffs(p, false);
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
    if (g.kind == Kind::Group)
        return fmt::format("dps_scorecard: {} bots, {} {} s of {}", gr.subs.size(),
                           gr.phase == GroupPhase::Place ? "placing" : gr.phase == GroupPhase::Buff ? "buffing" : gr.phase == GroupPhase::Warm ? "warming up" : "measuring",
                           gr.phase == GroupPhase::Measure ? gr.measure_s : gr.t, gr.phase == GroupPhase::Buff ? gr.buff_s : gr.leg_s);
    if (g.kind == Kind::Damage)
        return fmt::format("damage_parity {}: leg {} of {} ({}), {} s", g.cls_name, std::min(g.leg + 1, LegTotal()), LegTotal(), CurLeg().label, g.step_s);
    if (g.kind == Kind::Economy)
        return fmt::format("economy_audit {}: phase {} of 4, {} s", g.cls_name, g.econ.phase, g.econ.t);
    if (g.kind == Kind::Trap)
        return fmt::format("trap_audit {}: trap {} of {}, phase {}, {} s", g.cls_name, std::min(g.trap.item + 1, kTrapCount), kTrapCount, g.trap.phase, g.trap.t);
    if (g.kind == Kind::Threat)
        return fmt::format("threat_audit {}: level {} of {}, phase {}, {} s", g.cls_name, std::min(g.threat.level + 1, std::size(kThreatLevels)), std::size(kThreatLevels), g.threat.phase, g.threat.t);
    if (g.kind == Kind::Incoming)
        return fmt::format("incoming_damage {}: leg {} of 6, {} s", g.cls_name, g.incoming.leg + 1, g.incoming.t);
    return fmt::format("{} {}: item {} of {}", g.kind == Kind::Finisher ? "finisher_sweep" : g.kind == Kind::SpellCheck ? "spell_check" : "talent_audit", g.cls_name, std::min(g.item + 1, g.items.size()), g.items.size());
}

} // namespace Playerbot::Diagnostics::Parity
