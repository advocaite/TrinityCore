#include "ClassicUpkeep.h"
#include "RecoverRules.h"
#include "Bot/SpellFamily.h"
#include "BotSnapshotView.h"
#include "ClassTables.h"
#include "ClassicSupplies.h"
#include "Group/GroupSnapshot.h"
#include "Raid/RaidEncounters.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "GameTime.h"
#include "Services.h"
#include "Util/ConfigReader.h"
#include "Log.h"
#include "World.h"
#include "fmt/format.h"
#include <algorithm>
#include <functional>
#include <array>
#include <atomic>
#include <initializer_list>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace Playerbot::Classic {

namespace {

constexpr uint8 POWER_MANA = 0;
constexpr uint32 MECHANIC_FEAR_ID = 5;

float Dist2(float ax, float ay, float az, float bx, float by, float bz)
{
    float const dx = ax - bx, dy = ay - by, dz = az - bz;
    return dx * dx + dy * dy + dz * dz;
}

int32 ManaPct(BotSnapshotView const& s)
{
    return s.max_power(POWER_MANA) > 0 ? s.power_pct(POWER_MANA) : 100;
}

bool CanCast(BotSnapshotView const& s, uint32 spell)
{
    return s.knows_spell(spell) && s.is_ready(spell);
}

// ======================= Paladin blessings =======================

enum Blessing : uint8 { MIGHT, WISDOM, KINGS, SALVATION, LIGHT, SANCTUARY };

struct BlessingSpells
{
    uint32 normal;    // rank 1; the group snapshot records every rank and the Greater version under this id
    uint32 greater;   // rank 1 of the Greater Blessing (Symbol of Kings)
};

constexpr BlessingSpells kBlessings[] = {
    { 19740, 25782 },   // Might
    { 19742, 25894 },   // Wisdom
    { 20217, 25898 },   // Kings (Protection talent)
    { 1038,  25895 },   // Salvation
    { 19977, 25890 },   // Light
    { 20911, 25899 },   // Sanctuary (Protection talent)
};

constexpr uint32 SYMBOL_OF_KINGS = 21177;
constexpr float  kBlessRange     = 30.0f;

// Blessing order per recipient CLASS (PallyPower defaults). A Greater
// Blessing reaches every member of the target's class, so the plan is per
// class, not per member: the i-th paladin of the group gives each class the
// blessing at index i of its order. Physical classes get Might, mana classes
// Wisdom; in a raid Salvation leads (threat). The solo / tank orders below
// are for single blessings.
constexpr Blessing kSoloTankOrder[]    = { KINGS, MIGHT, SANCTUARY, LIGHT };
constexpr Blessing kMeleeOrder[]       = { MIGHT, KINGS, SALVATION, LIGHT };
constexpr Blessing kMeleeRaidOrder[]   = { SALVATION, MIGHT, KINGS, LIGHT };
constexpr Blessing kCasterOrder[]      = { WISDOM, KINGS, SALVATION, LIGHT };
constexpr Blessing kCasterRaidOrder[]  = { SALVATION, WISDOM, KINGS, LIGHT };
// A tank's own blessing from the paladin whose class slot is Salvation
// (Salvation on a tank cuts its threat): the first of these nobody else gives it.
constexpr Blessing kTankSubstitute[]   = { KINGS, SANCTUARY, LIGHT, MIGHT };

// Warriors and rogues are the physical classes; every other class has mana
// (Wisdom), hybrids included, since a Greater Blessing can't tell specs apart.
bool PhysicalClass(uint8 cls)
{
    return cls == CLASS_WARRIOR || cls == CLASS_ROGUE;
}

std::span<Blessing const> ClassBlessingOrder(uint8 cls, bool raid)
{
    if (PhysicalClass(cls))
        return raid ? std::span<Blessing const>(kMeleeRaidOrder) : std::span<Blessing const>(kMeleeOrder);
    return raid ? std::span<Blessing const>(kCasterRaidOrder) : std::span<Blessing const>(kCasterOrder);
}

// Order for one member's normal blessing: by its spec (a Retribution
// paladin or feral druid wants Might, not Wisdom).
std::span<Blessing const> MemberBlessingOrder(uint8 cls, uint32 spec, bool raid)
{
    if (PhysicalClass(cls) || IsMeleeSpec(cls, spec))
        return raid ? std::span<Blessing const>(kMeleeRaidOrder) : std::span<Blessing const>(kMeleeOrder);
    return raid ? std::span<Blessing const>(kCasterRaidOrder) : std::span<Blessing const>(kCasterOrder);
}

// A Greater Blessing that does something on this server. The Greater Blessing spells of this data carry no effect at all (the spell
// audit lists effect 0 on 25782, 25890, 25894, 25898, 25899, 25916): the cast spends a Symbol of Kings, applies nothing, and the paladin
// that sees no aura on the class casts it again on the next tick (MC Magmadar 2026-10-10 and the 14-17 Greater casts per paladin in 14
// minutes of the 2026-10-12 raid). Such a spell is not used; the paladin blesses the members one by one with the normal blessings, which
// the same data does apply. Answered once per spell id.
bool GreaterBlessingWorks(uint32 spell_id)
{
    static std::mutex mu;
    static std::unordered_map<uint32, bool> memo;
    {
        std::lock_guard<std::mutex> lk(mu);
        if (auto it = memo.find(spell_id); it != memo.end())
            return it->second;
    }
    bool works = false;
    if (SpellInfo const* si = sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE))
        for (SpellEffectInfo const& eff : si->GetEffects())
            if (eff.Effect != SPELL_EFFECT_NONE)
            {
                works = true;
                break;
            }
    {
        std::lock_guard<std::mutex> lk(mu);
        memo.emplace(spell_id, works);
    }
    if (!works)
        TC_LOG_INFO("playerbot.v2", "[bless] Greater Blessing spell {} has no effect in this data: paladins bless member by member instead", spell_id);
    return works;
}

// Blessing `b` on a buff list, with more than `min_ms` left, normal or
// Greater. `caster` set: only that paladin's; `not_caster` set: anyone else's.
bool HasBlessing(std::vector<AuraEntry> const& buffs, Blessing b, int64 min_ms,
                 ObjectGuid caster = ObjectGuid::Empty, ObjectGuid not_caster = ObjectGuid::Empty)
{
    for (AuraEntry const& a : buffs)
        if (a.spell_id == kBlessings[b].normal && a.remaining.count() > min_ms
            && (caster.IsEmpty() || a.caster == caster) && (not_caster.IsEmpty() || a.caster != not_caster))
            return true;
    return false;
}

// This paladin's blessing at `paladin_index` of `order`, skipping blessings it
// hasn't learned (Kings / Sanctuary are Protection talents).
int BlessingAt(BotSnapshotView const& s, size_t paladin_index, std::span<Blessing const> order)
{
    for (size_t i = paladin_index; i < order.size(); ++i)
        if (s.knows_spell(kBlessings[order[i]].normal))
            return order[i];
    return -1;
}

// A tank of the group, as PveGroupCoordinator counts them (role Tank or a
// tank spec: main, off and spare raid tanks alike), or the plan's main tank.
// A Protection warrior off-tank never gets a Greater Salvation and keeps its
// global cooldowns out of the dispel pool.
bool IsTankMember(BotSnapshotView const& s, GroupMemberSummary const& m)
{
    if (m.role == Role::Tank || IsTankSpec(m.cls, uint16(m.spec)))
        return true;
    auto const& po = s.raw().pve_order;
    return po.active && !po.main_tank.IsEmpty() && po.main_tank == m.guid;
}

// ======================= Shaman =======================

constexpr uint32 ROCKBITER_WEAPON   = 8017;
constexpr uint32 FLAMETONGUE_WEAPON = 8024;
constexpr uint32 WINDFURY_WEAPON    = 8232;

constexpr uint32 SPEC_SHAMAN_ELEMENTAL = 262;
constexpr uint32 SPEC_SHAMAN_RESTO     = 264;

enum Element : uint8 { EARTH, FIRE, WATER, AIR };

// Earth
constexpr uint32 STRENGTH_OF_EARTH_TOTEM = 8075;
constexpr uint32 STONESKIN_TOTEM         = 8071;
constexpr uint32 TREMOR_TOTEM            = 8143;
constexpr uint32 EARTHBIND_TOTEM         = 2484;
constexpr uint32 STONECLAW_TOTEM         = 5730;
// Fire
constexpr uint32 SEARING_TOTEM           = 3599;
constexpr uint32 FIRE_NOVA_TOTEM         = 1535;
constexpr uint32 MAGMA_TOTEM             = 8190;
constexpr uint32 FLAMETONGUE_TOTEM       = 8227;
constexpr uint32 FROST_RESISTANCE_TOTEM  = 8181;
// Water
constexpr uint32 MANA_SPRING_TOTEM       = 5675;
constexpr uint32 HEALING_STREAM_TOTEM    = 5394;
constexpr uint32 POISON_CLEANSING_TOTEM  = 8166;
constexpr uint32 DISEASE_CLEANSING_TOTEM = 8170;
constexpr uint32 FIRE_RESISTANCE_TOTEM   = 8184;
constexpr uint32 MANA_TIDE_TOTEM         = 16190;
// Air
constexpr uint32 WINDFURY_TOTEM          = 8512;
constexpr uint32 GRACE_OF_AIR_TOTEM      = 8835;
constexpr uint32 GROUNDING_TOTEM         = 8177;
constexpr uint32 NATURE_RESISTANCE_TOTEM = 10595;
constexpr uint32 WINDWALL_TOTEM          = 15107;
constexpr uint32 TRANQUIL_AIR_TOTEM      = 25908;

constexpr uint32 kEarthTotems[] = { STRENGTH_OF_EARTH_TOTEM, STONESKIN_TOTEM, TREMOR_TOTEM, EARTHBIND_TOTEM, STONECLAW_TOTEM };
constexpr uint32 kFireTotems[]  = { SEARING_TOTEM, FIRE_NOVA_TOTEM, MAGMA_TOTEM, FLAMETONGUE_TOTEM, FROST_RESISTANCE_TOTEM };
constexpr uint32 kWaterTotems[] = { MANA_SPRING_TOTEM, HEALING_STREAM_TOTEM, POISON_CLEANSING_TOTEM, DISEASE_CLEANSING_TOTEM,
                                    FIRE_RESISTANCE_TOTEM, MANA_TIDE_TOTEM };
constexpr uint32 kAirTotems[]   = { WINDFURY_TOTEM, GRACE_OF_AIR_TOTEM, GROUNDING_TOTEM, NATURE_RESISTANCE_TOTEM,
                                    WINDWALL_TOTEM, TRANQUIL_AIR_TOTEM };

std::span<uint32 const> ElementTotems(Element e)
{
    switch (e)
    {
        case EARTH: return kEarthTotems;
        case FIRE:  return kFireTotems;
        case WATER: return kWaterTotems;
        default:    return kAirTotems;
    }
}

struct LiveTotem
{
    uint32 spell = 0;   // the rank-1 id from the element table
    TotemUpkeep const* totem = nullptr;
};

LiveTotem CurrentTotem(BotSnapshotView const& s, Element e)
{
    for (uint32 spell : ElementTotems(e))
        if (TotemUpkeep const* t = s.totem_from(spell))
            return { spell, t };
    return {};
}

bool ShamanIsCaster(uint32 spec)
{
    return spec == SPEC_SHAMAN_ELEMENTAL || spec == SPEC_SHAMAN_RESTO;
}

// The bot's raid subgroup (0 for a 5-man party or when the bot isn't listed).
uint8 SelfSubgroup(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    if (auto const* members = g.members())
        for (GroupMemberSummary const& m : *members)
            if (m.guid == s.guid())
                return m.subgroup;
    return 0;
}

// A party member other than the bot, alive and close enough to sit in totem
// range. Totems only reach the shaman's own party: in a raid, its subgroup.
template <typename Pred>
bool PartyMemberNear(BotSnapshotView const& s, GroupSnapshotView const& g, Pred&& pred)
{
    auto const* members = g.members();
    if (!members) return false;
    uint8 const sg = SelfSubgroup(s, g);
    float bx, by, bz; s.position(bx, by, bz);
    for (GroupMemberSummary const& m : *members)
    {
        if (m.guid == s.guid() || !m.online || !m.is_alive || m.map_id != s.map_id() || m.subgroup != sg) continue;
        if (Dist2(m.x, m.y, m.z, bx, by, bz) > 30.f * 30.f) continue;
        if (pred(m)) return true;
    }
    return false;
}

// A member of the shaman's subgroup carries a debuff of `type`.
bool PartyDispelCandidate(BotSnapshotView const& s, GroupSnapshotView const& g, DispelType type)
{
    if (s.self_dispellable(type))
        return true;
    return PartyMemberNear(s, g, [type](GroupMemberSummary const& m)
    {
        for (AuraEntry const& a : m.debuffs)
            if (a.is_harmful && a.dispel_type == type)
                return true;
        return false;
    });
}

// This shaman's index among the bot shamans of its subgroup (by guid), so
// two shamans of one party drop different totems of an element instead of
// overwriting each other's.
size_t ShamanIndexInParty(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    size_t index = 0;
    if (auto const* members = g.members())
    {
        uint8 const sg = SelfSubgroup(s, g);
        for (GroupMemberSummary const& m : *members)
            if (m.cls == CLASS_SHAMAN && m.is_bot && m.online && m.is_alive && m.map_id == s.map_id()
                && m.subgroup == sg && m.guid < s.guid())
                ++index;
    }
    return index;
}

// Mana Tide Totem (talent): in combat, when the healers of the shaman's
// subgroup (itself included) within totem range average under 40% mana -
// members whose mana is unknown (max_mana 0) are left out. A Mana Tide
// already standing is kept for its 12 s instead of being swapped back to
// Mana Spring as soon as the mana comes up.
bool PartyHealersNeedManaTide(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    if (!s.in_combat() || !s.knows_spell(MANA_TIDE_TOTEM))
        return false;
    LiveTotem const cur = CurrentTotem(s, WATER);
    if (cur.spell == MANA_TIDE_TOTEM && cur.totem)
        return true;
    if (!s.is_ready(MANA_TIDE_TOTEM))
        return false;
    auto const* members = g.members();
    if (!members)
        return false;
    uint8 const sg = SelfSubgroup(s, g);
    float bx, by, bz; s.position(bx, by, bz);
    int64 pct_sum = 0;
    int n = 0;
    for (GroupMemberSummary const& m : *members)
    {
        if (m.role != Role::Healer || !m.online || !m.is_alive || m.map_id != s.map_id() || m.subgroup != sg) continue;
        if (m.max_mana <= 0) continue;
        if (Dist2(m.x, m.y, m.z, bx, by, bz) > 30.f * 30.f) continue;
        pct_sum += int64(m.mana) * 100 / m.max_mana;
        ++n;
    }
    return n > 0 && pct_sum < int64(n) * 40;
}

// The totem at `index` of `wanted` (0 entries skipped), walking on past the
// ones the bot hasn't learned.
uint32 TotemAt(BotSnapshotView const& s, std::initializer_list<uint32> wanted, size_t index)
{
    uint32 list[8];
    size_t n = 0;
    for (uint32 t : wanted)
        if (t && n < 8) list[n++] = t;
    for (size_t i = index; i < n; ++i)
        if (s.knows_spell(list[i]))
            return list[i];
    return 0;
}

// A living hostile NPC in combat whose victim is the bot, a group member or
// a pet of one (the snapshot's threat classification).
bool FightingTheGroup(NearbyUnit const& u)
{
    if (u.hp <= 0 || u.is_player || !u.unit_in_combat)
        return false;
    switch (ThreatVictim(u.victim_kind))
    {
        case ThreatVictim::Me: case ThreatVictim::Tank: case ThreatVictim::Member: case ThreatVictim::MemberPet:
            return true;
        default:
            return false;
    }
}

// A boss that fears the raid (BossFears) is in combat with the group.
bool FearBossEngaged(BotSnapshotView const& s)
{
    for (NearbyUnit const& u : s.raw().combat.nearby_enemies)
        if (FightingTheGroup(u) && BossFears(s.map_id(), u.entry))
            return true;
    return false;
}

// Molten Core: one of the bosses whose fire needs resistance (Magmadar, Baron Geddon, Ragnaros) is in combat with the group.
bool FireResistBossEngaged(BotSnapshotView const& s)
{
    for (NearbyUnit const& u : s.raw().combat.nearby_enemies)
        if (FightingTheGroup(u) && (u.entry == 11982 || u.entry == 12056 || u.entry == 11502))
            return true;
    return false;
}

// The resistance school the fire resistance aura / totem is held for. In Molten Core they are held only against the three fire
// bosses: the aura puts a gold haze round every player in range (playtest 2026-10-13), so elsewhere the normal aura and totem stand.
Resist AuraResistance(BotSnapshotView const& s)
{
    Resist const r = InstanceResistance(s.map_id());
    if (r == Resist::Fire && s.map_id() == 409 && !FireResistBossEngaged(s))
        return Resist::None;
    return r;
}

// Raid: the shaman's subgroup (itself included) has a tank in it, as the
// coordinator counts tanks (role / tank spec / plan main tank).
bool SubgroupHasTank(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    auto const* members = g.members();
    if (!members) return false;
    uint8 const sg = SelfSubgroup(s, g);
    for (GroupMemberSummary const& m : *members)
        if (m.online && m.map_id == s.map_id() && m.subgroup == sg && IsTankMember(s, m))
            return true;
    return false;
}

uint32 DesiredTotem(BotSnapshotView const& s, GroupSnapshotView const& g, Element e)
{
    bool const self_melee = !ShamanIsCaster(s.spec());
    auto const melee_mate = [](GroupMemberSummary const& m) { return m.role == Role::Tank || IsMeleeSpec(m.cls, m.spec); };
    size_t const index = ShamanIndexInParty(s, g);
    Resist const resist = AuraResistance(s);
    switch (e)
    {
        case EARTH:
        {
            // Reactive: a member is feared. Proactive: a boss that fears the
            // raid is engaged - the first shaman of each party holds Tremor
            // through the fight instead of waiting for the first Panic.
            if (s.knows_spell(TREMOR_TOTEM)
                && (g.group_has_mechanic(MECHANIC_FEAR_ID, s.map_id()) || (index == 0 && FearBossEngaged(s))))
                return TREMOR_TOTEM;
            bool const melee = self_melee || PartyMemberNear(s, g, melee_mate);
            return TotemAt(s, { melee ? STRENGTH_OF_EARTH_TOTEM : 0u, STONESKIN_TOTEM }, index);
        }
        case WATER:
            if (PartyHealersNeedManaTide(s, g))
                return MANA_TIDE_TOTEM;
            if (s.knows_spell(POISON_CLEANSING_TOTEM) && PartyDispelCandidate(s, g, DispelType::Poison))
                return POISON_CLEANSING_TOTEM;
            if (s.knows_spell(DISEASE_CLEANSING_TOTEM) && PartyDispelCandidate(s, g, DispelType::Disease))
                return DISEASE_CLEANSING_TOTEM;
            // Fire instance (Molten Core): the first shaman of each party
            // holds Fire Resistance Totem, the next one Mana Spring.
            return TotemAt(s, { resist == Resist::Fire ? FIRE_RESISTANCE_TOTEM : 0u, MANA_SPRING_TOTEM, HEALING_STREAM_TOTEM }, index);
        case AIR:
        {
            if (resist == Resist::Nature && index == 0 && s.knows_spell(NATURE_RESISTANCE_TOTEM))
                return NATURE_RESISTANCE_TOTEM;
            // Tranquil Air: Horde raids have no Blessing of Salvation, so the
            // first shaman of a raid party without a tank in it drops the
            // threat reduction totem (a tank's party keeps its damage totem).
            if (g.is_raid() && index == (resist == Resist::Nature ? 1u : 0u) && s.knows_spell(TRANQUIL_AIR_TOTEM)
                && !SubgroupHasTank(s, g))
                return TRANQUIL_AIR_TOTEM;
            // Windfury Totem doesn't stack with the shaman's own Windfury
            // Weapon, so it is for the other melee of the party.
            bool const melee_near = PartyMemberNear(s, g, melee_mate);
            if (!self_melee && !melee_near)
                return 0;
            return TotemAt(s, { melee_near ? WINDFURY_TOTEM : 0u, GRACE_OF_AIR_TOTEM }, resist == Resist::Nature && index > 0 ? index - 1 : index);
        }
        case FIRE:
        {
            if (resist == Resist::Frost && index == 0 && s.knows_spell(FROST_RESISTANCE_TOTEM))
                return FROST_RESISTANCE_TOTEM;
            if (s.spec() == SPEC_SHAMAN_RESTO && ManaPct(s) < 70)
                return 0;
            if (self_melee && s.knows_spell(MAGMA_TOTEM) && s.enemies_within(8.0f) >= 3)
                return MAGMA_TOTEM;
            NearbyUnit const* v = s.victim_info();
            if (!v || v->hp_pct() < 25)
                return 0;
            float bx, by, bz; s.position(bx, by, bz);
            return Dist2(v->x, v->y, v->z, bx, by, bz) <= 18.f * 18.f ? SEARING_TOTEM : 0;
        }
    }
    return 0;
}

} // namespace

bool IsMeleeSpec(uint8 cls, uint32 spec)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
        case CLASS_ROGUE:
            return true;
        case CLASS_PALADIN: return spec == 66 || spec == 70;    // Protection / Retribution
        case CLASS_SHAMAN:  return spec == 263;                 // Enhancement
        case CLASS_DRUID:   return spec == 103 || spec == 104;  // Feral / Guardian
        default:            return false;
    }
}

CastPick PickBlessing(BotSnapshotView const& s, GroupSnapshotView const& g, bool in_combat, PickSkip skip)
{
    if (s.cls() != CLASS_PALADIN || s.is_mounted())
        return {};
    int64 const min_ms = in_combat ? 0 : 60'000;

    auto const* members = g.members();
    if (!members || members->empty())
    {
        // Solo: bless self by own role.
        bool const tank = IsTankSpec(CLASS_PALADIN, uint16(s.spec()));
        int const b = BlessingAt(s, 0, tank ? std::span<Blessing const>(kSoloTankOrder)
                                          : MemberBlessingOrder(CLASS_PALADIN, s.spec(), false));
        if (b < 0 || !s.is_ready(kBlessings[b].normal))
            return {};
        AuraEntry const* mine = s.find_aura(kBlessings[b].normal);
        if (mine && mine->remaining.count() > min_ms)
            return {};
        uint64 const key = (5ull << 48) | uint64(b);   // paced: a blessing the snapshot does not show must not repeat every tick
        if (skip(key))
            return {};
        return { kBlessings[b].normal, s.guid(), key };
    }

    // This paladin's index among the group's bot paladins on the map (by
    // guid, stable across ticks). A human paladin blesses by hand: its
    // blessings still count as present (HasBlessing reads any caster).
    size_t index = 0;
    for (GroupMemberSummary const& m : *members)
        if (m.cls == CLASS_PALADIN && m.is_bot && m.online && m.is_alive && m.map_id == s.map_id()
            && m.guid != s.guid() && m.guid < s.guid())
            ++index;

    bool const raid = g.is_raid();
    bool const greater_ok = s.item_count(SYMBOL_OF_KINGS) > 0;
    float bx, by, bz; s.position(bx, by, bz);

    // Kings for the tanks (+10% stamina is the biggest blessing a tank gets): the lowest-guid Protection bot paladin
    // of the map owns Kings on every class that has a tank in it (Kings is a Protection talent), whatever the class
    // order says; the other paladins plan that class from the order without Kings, their index not counting it.
    ObjectGuid kings_guid;
    for (GroupMemberSummary const& m : *members)
        if (m.cls == CLASS_PALADIN && m.is_bot && m.online && m.is_alive && m.map_id == s.map_id() && m.spec == 66
            && (kings_guid.IsEmpty() || m.guid < kings_guid))
            kings_guid = m.guid;
    size_t const index_without_kings = (!kings_guid.IsEmpty() && kings_guid != s.guid() && kings_guid < s.guid()) ? index - 1 : index;
    struct ClassPlan { std::array<Blessing, 6> order{}; size_t n = 0; size_t index = 0; };
    auto plan_for = [&](std::span<Blessing const> base, bool tank_class) -> ClassPlan
    {
        ClassPlan p;
        if (tank_class && !kings_guid.IsEmpty())
        {
            if (kings_guid == s.guid())
            {
                p.order[p.n++] = KINGS;
                return p;
            }
            for (Blessing b : base)
                if (b != KINGS)
                    p.order[p.n++] = b;
            p.index = index_without_kings;
            return p;
        }
        for (Blessing b : base)
            p.order[p.n++] = b;
        p.index = index;
        return p;
    };
    auto in_reach = [&](GroupMemberSummary const& m)
    {
        return m.online && m.is_alive && m.map_id == s.map_id()
            && (m.guid == s.guid() || Dist2(m.x, m.y, m.z, bx, by, bz) <= kBlessRange * kBlessRange);
    };
    uint16 seen = 0;   // bit per recipient class already planned (classes 1..11)
    for (GroupMemberSummary const& first : *members)
    {
        uint8 const cls = first.cls;
        if (!cls || cls >= 16 || (seen & (1u << cls))) continue;
        seen = uint16(seen | (1u << cls));

        bool class_has_tank = false;
        for (GroupMemberSummary const& m : *members)
            if (m.cls == cls && m.online && IsTankMember(s, m)) { class_has_tank = true; break; }

        // Greater Blessing (Symbol of Kings): one cast covers the class, so
        // the class order decides. Never a Greater Salvation on a class with
        // a tank in it - that one is blessed member by member below.
        ClassPlan const class_plan = plan_for(ClassBlessingOrder(cls, raid), class_has_tank);
        int const bc = BlessingAt(s, class_plan.index, std::span<Blessing const>(class_plan.order.data(), class_plan.n));
        if (bc >= 0 && greater_ok && s.knows_spell(kBlessings[bc].greater) && GreaterBlessingWorks(kBlessings[bc].greater) && !(class_has_tank && bc == SALVATION))
        {
            uint64 const key = (1ull << 48) | (uint64(bc) << 8) | cls;
            for (GroupMemberSummary const& m : *members)
            {
                if (m.cls != cls || !in_reach(m) || HasBlessing(m.buffs, Blessing(bc), min_ms)) continue;
                if (s.is_ready(kBlessings[bc].greater) && !skip(key))
                    return { kBlessings[bc].greater, m.guid, key };
                break;
            }
            continue;
        }

        // Normal blessings, member by member, by the member's own spec. A
        // tank whose slot from this paladin is Salvation gets this paladin's
        // substitute instead: the first of kTankSubstitute nobody else gives it.
        for (GroupMemberSummary const& m : *members)
        {
            if (m.cls != cls || !in_reach(m)) continue;
            bool const tank = IsTankMember(s, m);
            ClassPlan const member_plan = plan_for(MemberBlessingOrder(m.cls, m.spec, raid), class_has_tank);
            int const b = BlessingAt(s, member_plan.index, std::span<Blessing const>(member_plan.order.data(), member_plan.n));
            if (b < 0) continue;
            if (tank && b == SALVATION)
            {
                bool mine = false;
                for (Blessing t : kTankSubstitute)
                    if (HasBlessing(m.buffs, t, min_ms, s.guid())) { mine = true; break; }
                if (mine) continue;
                for (Blessing t : kTankSubstitute)
                {
                    if (!s.knows_spell(kBlessings[t].normal) || HasBlessing(m.buffs, t, min_ms, ObjectGuid::Empty, s.guid()))
                        continue;
                    uint64 const key = (2ull << 48) | (uint64(t) << 40) | (m.guid.GetCounter() & 0xFFFFFFFFFFull);
                    if (s.is_ready(kBlessings[t].normal) && !skip(key))
                        return { kBlessings[t].normal, m.guid, key };
                    break;
                }
                continue;
            }
            if (HasBlessing(m.buffs, Blessing(b), min_ms)) continue;
            uint64 const key = (2ull << 48) | (uint64(b) << 40) | (m.guid.GetCounter() & 0xFFFFFFFFFFull);
            if (!s.is_ready(kBlessings[b].normal) || skip(key)) continue;
            return { kBlessings[b].normal, m.guid, key };
        }
    }
    return {};
}

CastPick PickImbue(BotSnapshotView const& s, bool in_combat)
{
    if (s.cls() != CLASS_SHAMAN || s.is_mounted())
        return {};
    WeaponUpkeep const& mh = s.classic_upkeep().main_hand;
    if (mh.guid.IsEmpty())
        return {};
    static constexpr uint32 kMelee[]  = { WINDFURY_WEAPON, FLAMETONGUE_WEAPON, ROCKBITER_WEAPON };
    static constexpr uint32 kCaster[] = { FLAMETONGUE_WEAPON, ROCKBITER_WEAPON };
    std::span<uint32 const> const order = ShamanIsCaster(s.spec()) ? std::span<uint32 const>(kCaster) : std::span<uint32 const>(kMelee);
    for (uint32 spell : order)
    {
        if (!s.knows_spell(spell)) continue;
        bool const current = s.weapon_enchant_from(mh, spell);
        if (current && (in_combat || mh.temp_remaining.count() > 60'000))
            break;
        return s.is_ready(spell) ? CastPick{ spell, mh.guid } : CastPick{};
    }
    // Dual-wielding melee shaman: the off-hand takes Flametongue (Rockbiter before it is learned). A caster has no off-hand weapon.
    WeaponUpkeep const& oh = s.classic_upkeep().off_hand;
    if (!ShamanIsCaster(s.spec()) && !oh.guid.IsEmpty())
        for (uint32 spell : { FLAMETONGUE_WEAPON, ROCKBITER_WEAPON })
        {
            if (!s.knows_spell(spell)) continue;
            if (s.weapon_enchant_from(oh, spell) && (in_combat || oh.temp_remaining.count() > 60'000))
                break;
            return s.is_ready(spell) ? CastPick{ spell, oh.guid } : CastPick{};
        }
    return {};
}

ItemPick PickPoison(BotSnapshotView const& s)
{
    if (s.cls() != CLASS_ROGUE || s.is_mounted())
        return {};
    auto best_in_bags = [&s](char const* family) -> uint32
    {
        SupplyFamily const* f = FindSupplyFamily(family);
        if (!f) return 0;
        for (auto it = f->ranks.rbegin(); it != f->ranks.rend(); ++it)
            if (it->min_level <= s.level() && s.item_count(it->item) > 0)
                return it->item;
        return 0;
    };
    auto needs = [](WeaponUpkeep const& w)
    {
        return !w.guid.IsEmpty() && (!w.temp_enchant || w.temp_remaining.count() < 60'000);
    };
    ClassicUpkeepState const& cu = s.classic_upkeep();
    uint32 const instant = best_in_bags("Instant Poison");
    if (needs(cu.main_hand) && instant)
        return { instant, cu.main_hand.guid };
    if (needs(cu.off_hand))
    {
        uint32 const deadly = best_in_bags("Deadly Poison");
        if (deadly || instant)
            return { deadly ? deadly : instant, cu.off_hand.guid };
    }
    return {};
}

uint32 PickTotem(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    if (s.cls() != CLASS_SHAMAN)
        return 0;
    float bx, by, bz; s.position(bx, by, bz);
    bool const mana_ok = ManaPct(s) >= 25;
    for (Element e : { EARTH, WATER, AIR, FIRE })
    {
        uint32 const want = DesiredTotem(s, g, e);
        if (!want || !CanCast(s, want)) continue;
        bool const reactive = want == TREMOR_TOTEM || want == POISON_CLEANSING_TOTEM || want == DISEASE_CLEANSING_TOTEM
                           || want == MANA_TIDE_TOTEM;
        if (!mana_ok && !reactive) continue;
        LiveTotem const cur = CurrentTotem(s, e);
        if (cur.spell == want && cur.totem)
        {
            // In place: keep it unless the fight moved away from it.
            if (e == FIRE)
            {
                NearbyUnit const* v = s.victim_info();
                if (!v || Dist2(v->x, v->y, v->z, cur.totem->x, cur.totem->y, cur.totem->z) <= 18.f * 18.f)
                    continue;
            }
            else if (Dist2(cur.totem->x, cur.totem->y, cur.totem->z, bx, by, bz) <= 25.f * 25.f)
                continue;
        }
        return want;
    }
    return 0;
}

uint32 HealRank(BotSnapshotView const& s, GroupSnapshotView const& g, uint32 spell, ObjectGuid target)
{
    int64 hp = 0, max_hp = 0;
    if (target.IsEmpty() || target == s.guid())
    {
        hp = s.hp();
        max_hp = s.max_hp();
    }
    else if (auto const* members = g.members())
    {
        for (GroupMemberSummary const& m : *members)
            if (m.guid == target) { hp = m.hp; max_hp = m.max_hp; break; }
    }
    if (max_hp <= 0 || hp * 100 <= max_hp * 40)
        return 0;
    int64 const deficit = max_hp - hp;

    uint32 const top = s.resolve_rank(spell);
    if (!top)
        return 0;
    for (uint32 const rank : SpellFamily::Ranks(spell))
    {
        if (rank == top)
            return 0;   // only the top rank covers it
        SpellInfo const* si = sSpellMgr->GetSpellInfo(rank, DIFFICULTY_NONE);
        if (!si) continue;
        for (SpellEffectInfo const& eff : si->GetEffects())
        {
            if (!eff.IsEffect(SPELL_EFFECT_HEAL)) continue;
            float heal = eff.BasePoints;
            if (eff.RealPointsPerLevel != 0.f && si->BaseLevel)
            {
                uint32 lvl = s.level();
                if (si->MaxLevel && lvl > si->MaxLevel) lvl = si->MaxLevel;
                if (lvl > si->BaseLevel) heal += eff.RealPointsPerLevel * float(lvl - si->BaseLevel);
            }
            if (int64(heal) >= deficit)
                return rank;
            break;
        }
    }
    return 0;
}

void ShortClassSupplies(BotSnapshotView const& s, std::vector<uint32>& out)
{
    out.clear();
    // Ammo for the equipped bow / crossbow / gun: short under 30% of the target (the entry is the
    // ammo item the restock will hand out, so it doubles as the retry-lockout key).
    if (Services::Config().behavior_ammo_supply() && s.ammo_kind() != 0 && s.ammo_count() < AmmoLowBelow())
        if (uint32 const ammo = BestAmmo(s.ammo_kind(), s.level()))
            out.push_back(ammo);
    if (!Services::Config().classic_class_supplies())
        return;
    // A party buff spell the bot's level allows and it has not learned: the restock teaches it.
    for (uint32 spell : GroupBuffSpells(s.cls()))
        if (!s.knows_spell(spell))
            if (LearnableGroupBuff(spell, uint8(s.level())))
                out.push_back(spell);
    for (SupplyFamily const& f : GetSupplyFamilies())
    {
        if (!SupplyFamilyForClass(f, s.cls())) continue;
        if (MageConjuresFamily(s.cls(), f.name, [&](uint32 id) { return s.knows_spell(id); })) continue;
        uint32 const best = BestSupplyRank(f, s.level());
        if (!best) continue;
        uint32 have = 0;
        if (f.flags & SUPPLY_ALL_RANKS)
        {
            for (SupplyRank const& r : f.ranks)
                have += s.item_count(r.item);
        }
        else
            have = s.item_count(best);
        if (have < std::max<uint32>(1u, SupplyTarget(f, s.level()) / 2u))
            out.push_back(best);
    }
}

// ======================= Consumables =======================

namespace {

constexpr uint8 KindBit(ConsumableKind k) { return uint8(1u << uint8(k)); }

// The item data files the Demonic Rune under Trade Goods / Other (class 7, subclass 11), not Consumable (MC Geddon on next-6: "class 7 subclass 11,
// 1 item effects [trigger 0 spell 16666 effect 30 ...]"): every bag scan and the classification take both classes, and the effects decide the kind.
constexpr bool IsConsumableClass(uint32 item_class, uint32 item_subclass)
{
    return item_class == ITEM_CLASS_CONSUMABLE || (item_class == ITEM_CLASS_TRADE_GOODS && item_subclass == ITEM_SUBCLASS_TRADE_GOODS_OTHER);
}
constexpr uint8 kCombatKinds = KindBit(ConsumableKind::Healthstone) | KindBit(ConsumableKind::HealingPotion)
                             | KindBit(ConsumableKind::ManaPotion) | KindBit(ConsumableKind::ManaItem);

struct ConsumableInfo
{
    uint8  kinds = 0;          // KindBit mask, 0 = not a consumable we use
    bool   conjured = false;
    uint32 spell = 0;          // on-use spell (its cooldown is published under this id)
    uint32 restore_mana = 0;   // mana the on-use spell gives (its energize effects at their base roll)
    uint32 cooldown_ms = 0;    // full cooldown of that spell
    int32  req_level = 0;
    uint32 item_level = 0;
    uint32 req_skill = 0;
    uint32 req_skill_rank = 0;
};

ConsumableInfo ClassifyItem(uint32 entry)
{
    ConsumableInfo info;
    ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(entry);
    if (!tmpl || !IsConsumableClass(tmpl->GetClass(), tmpl->GetSubClass()))
        return info;
    for (ItemEffectEntry const* eff : tmpl->Effects)
    {
        if (!eff || eff->TriggerType != ITEM_SPELLTRIGGER_ON_USE || eff->SpellID <= 0)
            continue;
        SpellInfo const* si = sSpellMgr->GetSpellInfo(uint32(eff->SpellID), DIFFICULTY_NONE);
        if (!si)
            continue;
        info.spell = si->Id;
        for (SpellEffectInfo const& e : si->GetEffects())
            if (e.IsEffect(SPELL_EFFECT_ENERGIZE) && e.MiscValue == POWER_MANA)
                info.restore_mana += uint32(std::max(e.CalcValueAsInt(), 0));
        info.cooldown_ms = uint32(std::max({ eff->CoolDownMSec, eff->CategoryCoolDownMSec, 0 }));
        if (!info.cooldown_ms)
            info.cooldown_ms = std::max(si->RecoveryTime, si->CategoryRecoveryTime);

        uint32 const sub = tmpl->GetSubClass();
        if (sub == ITEM_SUBCLASS_BANDAGE)
            info.kinds |= KindBit(ConsumableKind::Bandage);
        else
        {
            switch (si->GetSpellSpecific())
            {
                case SPELL_SPECIFIC_FOOD:           info.kinds |= KindBit(ConsumableKind::Food); break;
                case SPELL_SPECIFIC_DRINK:          info.kinds |= KindBit(ConsumableKind::Drink); break;
                case SPELL_SPECIFIC_FOOD_AND_DRINK: info.kinds |= KindBit(ConsumableKind::Food) | KindBit(ConsumableKind::Drink); break;
                default: break;
            }
            // Instant restores: generic consumables (healthstones, runes,
            // mana gems) and potions; elixirs, flasks and scrolls are buffs.
            // The 12.1 item data files Demonic Runes and the like under "Other" (subclass 8), not "Consumable" (0): the Ragnaros squad
            // carried runes the bots never counted or used (0 runes in 76 bag censuses).
            if (!info.kinds && tmpl->GetClass() == ITEM_CLASS_TRADE_GOODS)
            {
                // Only a mana restore: a trade good with a healing on-use is not a healthstone.
                for (SpellEffectInfo const& e : si->GetEffects())
                    if (e.IsEffect(SPELL_EFFECT_ENERGIZE) && e.MiscValue == POWER_MANA)
                        info.kinds |= KindBit(ConsumableKind::ManaItem);
            }
            else if (!info.kinds && (sub == ITEM_SUBCLASS_CONSUMABLE || sub == ITEM_SUBCLASS_POTION || sub == ITEM_SUBCLASS_CONSUMABLE_OTHER))
            {
                for (SpellEffectInfo const& e : si->GetEffects())
                {
                    if (e.IsEffect(SPELL_EFFECT_HEAL))
                        info.kinds |= KindBit(sub == ITEM_SUBCLASS_POTION ? ConsumableKind::HealingPotion : ConsumableKind::Healthstone);
                    else if (e.IsEffect(SPELL_EFFECT_ENERGIZE) && e.MiscValue == POWER_MANA)
                        info.kinds |= KindBit(sub == ITEM_SUBCLASS_POTION ? ConsumableKind::ManaPotion : ConsumableKind::ManaItem);
                }
            }
        }
        break;   // the first on-use spell is the one UseItem casts
    }
    if (info.kinds)
    {
        info.conjured = tmpl->IsConjuredConsumable();
        info.req_level = tmpl->GetBaseRequiredLevel();
        info.item_level = tmpl->GetBaseItemLevel();
        info.req_skill = tmpl->GetRequiredSkill();
        info.req_skill_rank = tmpl->GetRequiredSkillRank();
    }
    return info;
}

// Item templates never change at runtime: classify each entry once.
ConsumableInfo const& Classify(uint32 entry)
{
    static std::unordered_map<uint32, ConsumableInfo> cache;
    static std::shared_mutex mtx;
    {
        std::shared_lock lock(mtx);
        if (auto it = cache.find(entry); it != cache.end())
            return it->second;   // node-based map: references stay valid
    }
    ConsumableInfo info = ClassifyItem(entry);
    std::unique_lock lock(mtx);
    return cache.try_emplace(entry, info).first->second;
}

bool UsableNow(BotSnapshotView const& s, ConsumableInfo const& info)
{
    if (info.req_level > int32(s.level()))
        return false;
    if (info.req_skill && s.skill_value(uint16(info.req_skill)) < info.req_skill_rank)
        return false;
    return s.cd_remaining(info.spell).count() <= 0;
}

bool IsHealSpell(uint32 spell_id)
{
    SpellInfo const* si = spell_id ? sSpellMgr->GetSpellInfo(spell_id, DIFFICULTY_NONE) : nullptr;
    if (!si) return false;
    for (SpellEffectInfo const& e : si->GetEffects())
        if (e.IsEffect(SPELL_EFFECT_HEAL))
            return true;
    return false;
}

bool HasRegenAura(BotSnapshotView const& s, bool drink)
{
    for (AuraEntry const& a : s.raw().auras.own_auras)
    {
        SpellInfo const* si = sSpellMgr->GetSpellInfo(a.spell_id, DIFFICULTY_NONE);
        if (!si) continue;
        SpellSpecificType const spec = si->GetSpellSpecific();
        if (spec == SPELL_SPECIFIC_FOOD_AND_DRINK || spec == (drink ? SPELL_SPECIFIC_DRINK : SPELL_SPECIFIC_FOOD))
            return true;
    }
    return false;
}

} // namespace

uint32 BestConsumable(BotSnapshotView const& s, ConsumableKind kind, bool conjured_only)
{
    uint8 const bit = KindBit(kind);
    uint32 best = 0;
    ConsumableInfo const* bestInfo = nullptr;
    for (InventoryItem const& it : s.raw().inventory.bag_items)
    {
        if (!IsConsumableClass(it.item_class, it.item_subclass) || it.entry == best)
            continue;
        ConsumableInfo const& info = Classify(it.entry);
        if (!(info.kinds & bit) || (conjured_only && !info.conjured) || !UsableNow(s, info))
            continue;
        if (bestInfo)
        {
            auto const rank = [](ConsumableInfo const& i) { return std::tuple(i.req_level, i.item_level, i.conjured); };
            if (rank(info) <= rank(*bestInfo))
                continue;
        }
        best = it.entry;
        bestInfo = &info;
    }
    return best;
}

bool CombatConsumableUsedRecently(BotSnapshotView const& s, uint32 window_ms)
{
    for (InventoryItem const& it : s.raw().inventory.bag_items)
    {
        if (!IsConsumableClass(it.item_class, it.item_subclass))
            continue;
        ConsumableInfo const& info = Classify(it.entry);
        if (!(info.kinds & kCombatKinds) || !info.cooldown_ms)
            continue;
        int64 const rem = s.cd_remaining(info.spell).count();
        if (rem > 0 && rem <= int64(info.cooldown_ms) && int64(info.cooldown_ms) - rem < int64(window_ms))
            return true;
    }
    return false;
}

bool HealIncoming(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    auto const* members = g.members();
    if (!members) return false;
    for (GroupMemberSummary const& m : *members)
        if (m.guid != s.guid() && m.online && m.is_alive && m.is_casting
            && m.casting_target == s.guid() && IsHealSpell(m.casting_spell))
            return true;
    return false;
}

bool IsEating(BotSnapshotView const& s)   { return HasRegenAura(s, false); }
bool IsDrinking(BotSnapshotView const& s) { return HasRegenAura(s, true); }

bool RecoverBeforeWalking(BotSnapshotView const& s)
{
    if (s.hp_pct() < 50) return true;
    if (IsEating(s) || IsDrinking(s)) return true;
    return s.max_power(POWER_MANA) > 0 && s.power_pct(POWER_MANA) < 20 && BestConsumable(s, ConsumableKind::Drink) != 0;
}

bool SafeToRecover(BotSnapshotView const& s, GroupSnapshotView const* g)
{
    if (!s.is_alive() || s.in_combat() || s.attackers_count() > 0)
        return false;
    float bx, by, bz; s.position(bx, by, bz);
    auto const* members = g ? g->members() : nullptr;
    if (members)
        for (GroupMemberSummary const& m : *members)
            if (m.guid != s.guid() && m.online && m.is_alive && m.in_combat && m.map_id == s.map_id()
                && MemberFightBlocksRecovery(Dist2(m.x, m.y, m.z, bx, by, bz)))
                return false;
    for (NearbyUnit const& u : s.raw().combat.nearby_enemies)
    {
        if (u.hp <= 0 || u.victim.IsEmpty())
            continue;
        bool ours = u.victim == s.guid();
        if (!ours && members)
            for (GroupMemberSummary const& m : *members)
                if (m.guid == u.victim) { ours = true; break; }
        // Without a group view any engaged enemy next to the bot counts, as before.
        if (EnemyBlocksRecovery(ours || !members, Dist2(u.x, u.y, u.z, bx, by, bz)))
            return false;
    }
    return true;
}

bool IsGroupHealer(BotSnapshotView const& s, GroupSnapshotView const* g)
{
    if (auto const* members = g ? g->members() : nullptr)
        for (GroupMemberSummary const& m : *members)
            if (m.guid == s.guid())
                return m.role == Role::Healer;
    return IsHealerSpec(s.cls(), uint16(s.spec()));
}

namespace
{
struct BossPullWait { uint32 first_ms = 0, last_ms = 0; };
std::mutex g_boss_pull_mu;
std::unordered_map<uint64, BossPullWait> g_boss_pull;
}

void NoteBossPullWait(ObjectGuid group)
{
    if (group.IsEmpty()) return;
    uint32 const now = GameTime::GetGameTimeMS();
    std::lock_guard<std::mutex> lk(g_boss_pull_mu);
    BossPullWait& w = g_boss_pull[group.GetCounter()];
    if (!w.last_ms || now - w.last_ms > 10000u) w.first_ms = now;   // a new approach to a boss restarts the bounded wait
    w.last_ms = now;
}

int32 PullReadyHealerManaPct(BotSnapshotView const& s)
{
    if (!s.is_in_raid())
        return kPullReadyDungeonHealerManaPct;
    if (!s.group_guid().IsEmpty())
    {
        uint32 const now = GameTime::GetGameTimeMS();
        std::lock_guard<std::mutex> lk(g_boss_pull_mu);
        auto const it = g_boss_pull.find(s.group_guid().GetCounter());
        if (it != g_boss_pull.end() && now - it->second.last_ms <= 5000u && now - it->second.first_ms < kBossPullWaitCapMs)
            return kBossPullHealerManaPct;
    }
    return kPullReadyHealerManaPct;
}

int32 OocDrinkStartManaPct(BotSnapshotView const& s, GroupSnapshotView const* g,
                           int32 open_world_pct)
{
    if (s.in_group() && s.is_in_instance() && DrinksForPull(s, g))
        return std::max(open_world_pct, IsGroupHealer(s, g) ? PullReadyHealerManaPct(s) : kPullReadyDpsManaPct);
    return open_world_pct;
}

// ======================= Raid consumables =======================

namespace {

// The spells whose aura shows an item's buff: the on-use spell (a flask or elixir applies its aura directly) and the
// spells its effects trigger.
void ItemBuffSpells(uint32 entry, std::vector<uint32>& out)
{
    out.clear();
    ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(entry);
    if (!tmpl)
        return;
    for (ItemEffectEntry const* eff : tmpl->Effects)
    {
        if (!eff || eff->TriggerType != ITEM_SPELLTRIGGER_ON_USE || eff->SpellID <= 0)
            continue;
        out.push_back(uint32(eff->SpellID));
        if (SpellInfo const* si = sSpellMgr->GetSpellInfo(uint32(eff->SpellID), DIFFICULTY_NONE))
            for (SpellEffectInfo const& e : si->GetEffects())
                if (e.TriggerSpell)
                    out.push_back(e.TriggerSpell);
    }
}

// Any own aura named "Well Fed": every stat food's buff, whichever food it came from.
bool WellFed(BotSnapshotView const& s)
{
    for (AuraEntry const& a : s.raw().auras.own_auras)
    {
        SpellInfo const* si = sSpellMgr->GetSpellInfo(a.spell_id, DIFFICULTY_NONE);
        if (si && si->SpellName && std::string_view((*si->SpellName)[LOCALE_enUS]) == "Well Fed")
            return true;
    }
    return false;
}

bool RaidConsumableBuffUp(BotSnapshotView const& s, RaidConsumable const& c)
{
    if (c.food)
        return WellFed(s);
    thread_local std::vector<uint32> spells;
    ItemBuffSpells(c.item, spells);
    for (AuraEntry const& a : s.raw().auras.own_auras)
        if (std::find(spells.begin(), spells.end(), a.spell_id) != spells.end())
            return true;
    return false;
}

// The raid role whose kit the bot uses. The group's role wins (a healer-flipped paladin takes the healer kit), then
// the tank test the blessings use, then the spec.
RaidConsumableRole RaidConsumableRoleOf(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    RaidConsumableRole role;
    GroupMemberSummary const* self = nullptr;
    if (auto const* members = g.members())
        for (GroupMemberSummary const& m : *members)
            if (m.guid == s.guid())
                self = &m;
    if (IsGroupHealer(s, &g))
        role = RaidConsumableRole::Healer;
    else if (self && IsTankMember(s, *self))
        role = RaidConsumableRole::Tank;
    else if (s.cls() == CLASS_HUNTER)
        role = RaidConsumableRole::Hunter;
    else if (IsMeleeSpec(s.cls(), s.spec()) || s.cls() == CLASS_WARRIOR || s.cls() == CLASS_ROGUE)
        role = RaidConsumableRole::Melee;
    else
        role = RaidConsumableRole::Caster;
    return role;
}

bool RaidConsumablesApply(BotSnapshotView const& s)
{
    return IsVanillaRealm() && s.in_group() && s.is_in_raid() && s.is_alive() && !s.is_mounted() && ConsumablesAllowed(s);
}

} // namespace

bool ConsumablesAllowed(BotSnapshotView const& s)
{
    // A battleground or arena map is PvP; everything else (open world, dungeons, raids) is PvE.
    bool const pvp = s.in_battleground() || s.is_bg_orphan();
    return pvp ? Services::Config().consumables_pvp() : Services::Config().consumables_pve();
}

uint32 PickRaidConsumable(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    if (!RaidConsumablesApply(s))
        return 0;
    RaidConsumable const* next = NextRaidConsumable(RaidConsumableRoleOf(s, g),
        [&](uint32 item) { return s.item_count(item) > 0; },
        [&](RaidConsumable const& c) { return RaidConsumableBuffUp(s, c); });
    return next ? next->item : 0;
}

uint32 PickRaidConsumableStock(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    if (!RaidConsumablesApply(s))
        return 0;
    RaidConsumable const* next = NextRaidConsumableToStock(RaidConsumableRoleOf(s, g),
        [&](uint32 item) { return s.item_count(item); },
        [&](RaidConsumable const& c) { return RaidConsumableBuffUp(s, c); });
    return next ? next->item : 0;
}

bool DrinksForPull(BotSnapshotView const& s, GroupSnapshotView const* g)
{
    if (!s.in_group() || !s.is_in_instance())
        return false;
    if (IsGroupHealer(s, g))
        return true;
    return Services::Config().behavior_pull_recover() && s.max_power(POWER_MANA) > 0;
}

RecoveryPick PickRecovery(BotSnapshotView const& s, int32 start_hp, int32 start_mana)
{
    constexpr int32 kTopOffPct = 95;
    int32 const hp = s.hp_pct();
    bool const mana_user = s.max_power(POWER_MANA) > 0;
    int32 const mana = mana_user ? s.power_pct(POWER_MANA) : 100;
    bool const eating = IsEating(s);
    bool const drinking = IsDrinking(s);
    if (!eating && hp < start_hp)
        if (uint32 const food = BestConsumable(s, ConsumableKind::Food))
            return { food, false };
    if (!drinking && mana_user && mana < start_mana)
        if (uint32 const drink = BestConsumable(s, ConsumableKind::Drink))
            return { drink, false };
    // A grouped instance DPS (start_mana == kPullReadyDpsManaPct) sits only to 85%: the rest is minutes of waiting for the raid.
    int32 const top_mana = start_mana == kPullReadyDpsManaPct ? 85 : kTopOffPct;
    if ((eating && hp < kTopOffPct) || (drinking && mana < top_mana))
        return { 0, true };
    return {};
}

uint32 PickCombatHealthItem(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    int32 const hp = s.hp_pct();
    if (hp > kCombatHealthItemPct || hp <= 0 || s.is_casting() || !ConsumablesAllowed(s))
        return 0;
    // A heal is about to land: keep the item unless the bot is critical.
    if (hp > 20 && HealIncoming(s, g))
        return 0;
    if (CombatConsumableUsedRecently(s))
        return 0;
    if (uint32 const stone = BestConsumable(s, ConsumableKind::Healthstone))
        return stone;
    return BestConsumable(s, ConsumableKind::HealingPotion);
}

uint32 PickCombatManaItem(BotSnapshotView const& s, GroupSnapshotView const* g, bool while_casting)
{
    if (s.max_power(POWER_MANA) <= 0 || (s.is_casting() && !while_casting) || !ConsumablesAllowed(s))
        return 0;
    int32 const mana = s.power_pct(POWER_MANA);
    // A healer in a boss fight, or in any fight of a raid (the trash packs
    // run for minutes), drinks early (its mana is the raid's health);
    // everyone else at 20%.
    bool const raid_healer = g && IsGroupHealer(s, g) && (BossFight(s) || s.is_in_raid());
    // A raid healer uses an item whenever it is ready and the restore fits in what the pool is missing: the items are limited by their
    // 2-minute cooldown, not by stock, and a bot that waits for 35% fires them late (MC Geddon n8 run 3: potion and rune both at 33% about
    // 60 s in, the next ones ready at 180 s, the kill at 159 s, four healers dry for the last 30 s).
    int32 const ceiling = raid_healer ? kHealerManaItemCeilingPct : 20;
    if (mana > ceiling)
        return 0;
    if (CombatConsumableUsedRecently(s))
        return 0;
    int32 const max_mana = s.max_power(POWER_MANA);
    auto const fits = [&](uint32 item)
    {
        if (!item)
            return false;
        if (!raid_healer)
            return true;
        int64 const room = int64(max_mana) * (100 - mana) / 100;   // mana the pool is missing
        return mana <= kBossHealerManaItemPct || int64(Classify(item).restore_mana) <= room;
    };
    if (uint32 const gem = BestConsumable(s, ConsumableKind::ManaItem, /*conjured_only*/ true); fits(gem))
        return gem;
    if (uint32 const potion = BestConsumable(s, ConsumableKind::ManaPotion); fits(potion))
        return potion;
    // Dark / Demonic Rune: 600-1000 damage to the user.
    if (s.hp_pct() > 50)
        if (uint32 const rune = BestConsumable(s, ConsumableKind::ManaItem); fits(rune))
            return rune;
    return 0;
}

// ======================= Boss fights (major cooldowns) =======================

bool IsBossUnit(NearbyUnit const& u)
{
    return u.is_dungeon_boss || u.is_boss_rank;
}

bool BossFight(BotSnapshotView const& s)
{
    if (!s.in_combat())
        return false;
    // Owner "max dps": every fight is a burn fight (cooldowns, healers' earlier mana items).
    if (s.tactic_burn())
        return true;
    // A battleground has no bosses: every fight is against players (or a base's guards) and the
    // major cooldowns are for them. Without this the instance rule below read it as "trash, hold
    // the cooldowns" and Bestial Wrath / Rapid Fire / Death Wish / Adrenaline Rush / Inner Focus /
    // racials never fired in a BG.
    if (s.in_battleground())
        return true;
    if (s.is_in_instance())
    {
        if (s.is_encounter_in_progress())
            return true;
        for (NearbyUnit const& u : s.raw().combat.nearby_enemies)
            if (IsBossUnit(u) && FightingTheGroup(u))
                return true;
        return false;
    }
    NearbyUnit const* v = s.victim_info();
    return v && v->hp > 0 && (IsBossUnit(*v) || v->max_hp >= 100'000);
}

namespace {

// One [raid_cd] / [raid_consume] line per (bot, spell or item) per 10 s: the
// rules run on the bot workers, so the pacing map is locked.
bool LogPaced(BotSnapshotView const& s, uint32 id)
{
    static std::mutex mtx;
    static std::unordered_map<uint64, uint32> last;
    uint32 const now = s.published_at_ms();
    uint64 const key = (s.guid().GetCounter() << 24) ^ id;
    std::lock_guard lock(mtx);
    if (last.size() > 4096)
        last.clear();
    auto [it, fresh] = last.try_emplace(key, now);
    if (!fresh && now - it->second < 10'000u)
        return false;
    it->second = now;
    return true;
}

} // namespace

void NoteCooldownUse(BotSnapshotView const& s, uint32 spell, char const* what)
{
    if (!s.is_in_instance() || !LogPaced(s, spell))
        return;
    NearbyUnit const* v = s.victim_info();
    TC_LOG_INFO("playerbot.v2", "[raid_cd] {} map={} uses {} ({}) victim={} boss={} encounter={} hp={}% mana={}%",
        s.name(), s.map_id(), what, spell, v ? v->entry : 0u, v && IsBossUnit(*v) ? 1 : 0,
        s.is_encounter_in_progress() ? 1 : 0, s.hp_pct(), s.max_power(POWER_MANA) > 0 ? s.power_pct(POWER_MANA) : 0);
}

void NoteConsumableUse(BotSnapshotView const& s, uint32 item, char const* what)
{
    if (!s.is_in_instance() || !LogPaced(s, item))
        return;
    TC_LOG_INFO("playerbot.v2", "[raid_consume] {} map={} uses {} item {} hp={}% mana={}% boss_fight={}",
        s.name(), s.map_id(), what, item, s.hp_pct(), s.max_power(POWER_MANA) > 0 ? s.power_pct(POWER_MANA) : 0,
        BossFight(s) ? 1 : 0);
}

// A mana user at 20% or less in an instance fight with nothing picked: why, once per 10 s. (MC Ragnaros 2026-10-13 650994ea: 8 mana items in
// a 4-minute fight for 12 healers, the five holy priests never drank and six healers sat under 10% from 210 s.)
void NoteManaItemRefused(BotSnapshotView const& s)
{
    if (s.max_power(POWER_MANA) <= 0 || !s.in_combat() || !s.is_in_instance() || s.power_pct(POWER_MANA) > 20 || !LogPaced(s, 0x6D616E61u))
        return;
    uint32 potions = 0, runes = 0, gems = 0, other = 0, rune_items = 0;
    for (InventoryItem const& it : s.raw().inventory.bag_items)
    {
        if (it.entry == 12662)
            rune_items += it.count;   // Demonic Rune by entry, whatever the classification below says
        if (!IsConsumableClass(it.item_class, it.item_subclass))
            continue;
        ConsumableInfo const& info = Classify(it.entry);
        if (info.kinds & KindBit(ConsumableKind::ManaPotion))
            potions += it.count;
        else if (info.kinds & KindBit(ConsumableKind::ManaItem))
            (info.conjured ? gems : runes) += it.count;
        else if (info.kinds & kCombatKinds)
            other += it.count;
    }
    // Runes in the bag that the classification above does not count: what the item data says about them, once per entry and process. The item's
    // class and subclass, then each effect with its trigger and the spell's effect types (MC scenarios on af2c045a: 5 Demonic Runes by entry in
    // every healer bag, 0 classified, 218 refusals).
    if (rune_items > runes)
    {
        static std::atomic<bool> s_runeLogged{ false };
        if (!s_runeLogged.exchange(true))
        {
            ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(12662);
            std::string effects;
            if (tmpl)
                for (ItemEffectEntry const* eff : tmpl->Effects)
                {
                    if (!eff)
                        continue;
                    SpellInfo const* si = eff->SpellID > 0 ? sSpellMgr->GetSpellInfo(uint32(eff->SpellID), DIFFICULTY_NONE) : nullptr;
                    effects += fmt::format(" [trigger {} spell {} known {}", int(eff->TriggerType), eff->SpellID, si ? 1 : 0);
                    if (si)
                        for (SpellEffectInfo const& e : si->GetEffects())
                            effects += fmt::format(" effect {} misc {}", int(e.Effect), e.MiscValue);
                    effects += "]";
                }
            TC_LOG_INFO("playerbot.v2", "[raid_consume] Demonic Rune 12662 is not classified as a mana item: template {}, class {} subclass {}, {} item effects{}",
                        tmpl ? 1 : 0, tmpl ? int(tmpl->GetClass()) : -1, tmpl ? int(tmpl->GetSubClass()) : -1, tmpl ? tmpl->Effects.size() : 0u, effects);
        }
    }
    TC_LOG_INFO("playerbot.v2", "[raid_consume] {} mana {}% and no mana item picked: casting {}, consumables allowed {}, a combat consumable used recently {}, "
                "usable potion {}, in the bags: {} mana potions, {} runes ({} Demonic Runes by entry), {} gems, {} other combat consumables, hp {}%",
                s.name(), s.power_pct(POWER_MANA), s.is_casting() ? 1 : 0, ConsumablesAllowed(s) ? 1 : 0, CombatConsumableUsedRecently(s) ? 1 : 0,
                BestConsumable(s, ConsumableKind::ManaPotion), potions, runes, rune_items, gems, other, s.hp_pct());
}

// ======================= Raid support =======================
// Port of Karrz's ike3 generic raid behaviour (playerbot/strategy: priest /
// mage / druid buff strategies with BUFF_PARTY / GREATER_BUFF_PARTY actions,
// PartyMemberWithoutAuraValue, paladin aura + PallyPower blessings, shaman
// totems, PartyMemberToDispel / CombatBotBaseAI::SelectDispelTarget, warlock
// soulstone), rewritten as pure snapshot picks with a deterministic split of
// the work: every bot computes the same assignment from the group snapshot.

namespace {

constexpr int64 kBuffRefreshMs  = 60'000;   // out of combat a buff with < 1 min left counts as missing
// Cast range of every class buff, and the reach a caster needs to a needy
// member of a subgroup to own it: assigning by a wider radius than the bot
// casts at would hand a subgroup to a caster that then can't reach anyone.
constexpr float kBuffCastRange  = 30.0f;

float MemberDist2(GroupMemberSummary const& m, float x, float y, float z)
{
    return Dist2(m.x, m.y, m.z, x, y, z);
}

bool SameMapAlive(BotSnapshotView const& s, GroupMemberSummary const& m)
{
    return m.online && m.is_alive && m.map_id == s.map_id();
}

// A tracked buff (GroupSnapshotBuilder kTrackedRaidBuffs id) on a member with
// more than `min_ms` left.
bool HasTracked(GroupMemberSummary const& m, uint32 tracked_id, int64 min_ms)
{
    for (AuraEntry const& a : m.buffs)
        if (a.spell_id == tracked_id && a.remaining.count() > min_ms)
            return true;
    return false;
}

// Spell level of a rank-1 spell: the level a member must reach before other
// bots count it as able to cast it.
uint32 SpellLevel(uint32 spell)
{
    SpellInfo const* si = spell ? sSpellMgr->GetSpellInfo(spell, DIFFICULTY_NONE) : nullptr;
    return si ? si->SpellLevel : 0;
}

// ---------------- Class buffs ----------------

enum class BuffWho : uint8 { Everyone, ManaUsers, Tanks };

struct ClassBuffDef
{
    uint8   cls;
    uint32  single;        // rank 1, also the tracked id (kTrackedRaidBuffs)
    uint32  group;         // rank 1 of the party version, 0 = none (reagent: spell data, per rank)
    BuffWho who;
    bool    raid_only;
    uint32  caster_spec;   // talent buff: only casters of this spec are assigned (0 = any)
    uint32  skip_spec;     // casters of this spec are not assigned (Shadowform blocks Holy spells)
    Resist  school = Resist::None;   // a resistance buff: kept up only where the instance deals this school (InstanceDealsSchool)
};

constexpr uint32 SPEC_PRIEST_DISC   = 256;
constexpr uint32 SPEC_PRIEST_SHADOW = 258;

// Party versions and their reagents (spell data decides per rank): Prayer of
// Fortitude r1 Holy Candle / r2 Sacred Candle, Prayer of Spirit and Prayer of
// Shadow Protection Sacred Candle, Arcane Brilliance Arcane Powder, Gift of
// the Wild r1 Wild Berries / r2 Wild Thornroot.
constexpr ClassBuffDef kClassBuffs[] = {
    { CLASS_PRIEST, 1243,  21562, BuffWho::Everyone,  false, 0,                SPEC_PRIEST_SHADOW },  // Power Word / Prayer of Fortitude
    { CLASS_PRIEST, 14752, 27681, BuffWho::ManaUsers, false, SPEC_PRIEST_DISC, SPEC_PRIEST_SHADOW },  // Divine / Prayer of Spirit
    { CLASS_PRIEST, 976,   27683, BuffWho::Everyone,  true,  0,                0, Resist::Shadow },   // (Prayer of) Shadow Protection (only where shadow damage is dealt)
    { CLASS_MAGE,   1459,  23028, BuffWho::ManaUsers, false, 0,                0 },                   // Arcane Intellect / Brilliance
    { CLASS_DRUID,  1126,  21849, BuffWho::Everyone,  false, 0,                0 },                   // Mark / Gift of the Wild
    { CLASS_DRUID,  467,   0,     BuffWho::Tanks,     false, 0,                0 },                   // Thorns on the tanks
};

// Druid shapeshift forms. A druid in one casts no caster spell (no buff, no
// cure). Feral and Guardian live in Cat / Bear Form, a Balance druid in
// Moonkin Form; out of combat a druid that owes a class buff leaves its form
// to cast it (DruidFormToLeave) and a feral one shifts back before the pull
// (DruidFormToEnter), both run by idle:classic_druid_form.
constexpr uint32 kDruidForms[] = { 768, 5487, 9634, 783, 1066, 24858 };   // Cat, Bear, Dire Bear, Travel, Aquatic, Moonkin

bool SelfInDruidForm(BotSnapshotView const& s)
{
    if (s.cls() != CLASS_DRUID)
        return false;
    for (uint32 form : kDruidForms)
        if (s.has_aura(form))
            return true;
    return false;
}

// A member in a druid form (group snapshot tracked buff; forms have no
// duration, so presence alone counts).
bool MemberInDruidForm(GroupMemberSummary const& m)
{
    if (m.cls != CLASS_DRUID)
        return false;
    for (AuraEntry const& a : m.buffs)
        for (uint32 form : kDruidForms)
            if (a.spell_id == form)
                return true;
    return false;
}

// Druid specs that stay shapeshifted: Balance (Moonkin), Feral (Cat),
// Guardian (Bear).
bool ShapeshifterSpec(uint8 cls, uint32 spec)
{
    return cls == CLASS_DRUID && (spec == 102 || spec == 103 || spec == 104);
}

bool NeedsBuff(BotSnapshotView const& s, ClassBuffDef const& d, GroupMemberSummary const& m)
{
    if (!SameMapAlive(s, m))
        return false;
    switch (d.who)
    {
        case BuffWho::ManaUsers: if (m.max_mana <= 0) return false; break;
        case BuffWho::Tanks:     if (!IsTankMember(s, m)) return false; break;
        default: break;
    }
    return !HasTracked(m, d.single, kBuffRefreshMs);
}

// The bot casters the buff is split between, sorted by guid. Members whose
// spec can't (or would rather not) cast it are left out when others remain:
// Shadowform priests, and the druid specs that stay shapeshifted (Feral /
// Guardian / Balance: a caster druid takes the work first). The form a druid
// is in right now does not change the plan - an assigned druid leaves its form
// to cast (DruidFormToLeave) - so shifting out or back never moves the work
// to another druid.
void BuffCasters(BotSnapshotView const& s, std::vector<GroupMemberSummary> const& members, ClassBuffDef const& d,
                 std::vector<GroupMemberSummary const*>& out)
{
    out.clear();
    uint32 const level = SpellLevel(d.single);
    for (int pass = 0; pass < 2 && out.empty(); ++pass)
        for (GroupMemberSummary const& m : members)
        {
            if (m.cls != d.cls || !m.is_bot || !SameMapAlive(s, m) || m.level < level) continue;
            // Only a bot that knows the spell can cast it: a talent buff
            // (Divine Spirit) is known by the priests that took the talent,
            // so a raid without one has no caster and nothing is waited for.
            if (!m.knows_class_buff(d.single)) continue;
            if (pass == 0 && ((d.caster_spec && m.spec != d.caster_spec) || (d.skip_spec && m.spec == d.skip_spec)
                              || ShapeshifterSpec(m.cls, m.spec)))
                continue;
            out.push_back(&m);
        }
    std::sort(out.begin(), out.end(), [](GroupMemberSummary const* a, GroupMemberSummary const* b) { return a->guid < b->guid; });
}

// Forever's client data leaves some vanilla buff spells without any effect (Mark of the Wild 1126 and its ranks, Gift of the Wild: the
// [buff_plan] line shows "effect targets" empty and the research diff lists them 0/0), so a cast does nothing and the buff stays missing.
// The working version is another spell of the same name (Mark of the Wild 364163, level 50). A buff is cast with the spell that applies an
// aura: the rank 1 id itself, else the highest same-name spell of the class the bot knows and its level allows.
bool SpellAppliesAura(SpellInfo const* si)
{
    if (!si)
        return false;
    for (SpellEffectInfo const& e : si->GetEffects())
        if (e.IsEffect() && e.IsAura())
            return true;
    return false;
}

uint32 EffectiveBuffSpell(BotSnapshotView const& s, uint32 base)
{
    static std::mutex mu;
    static std::unordered_map<uint32, std::vector<std::pair<uint32, uint32>>> cache;   // base -> (spell level, id), ascending
    std::vector<std::pair<uint32, uint32>> cands;
    {
        std::lock_guard<std::mutex> lk(mu);
        auto it = cache.find(base);
        if (it == cache.end())
        {
            std::vector<std::pair<uint32, uint32>> built;
            if (SpellInfo const* bi = sSpellMgr->GetSpellInfo(base, DIFFICULTY_NONE))
            {
                if (SpellAppliesAura(bi))
                    built.emplace_back(bi->SpellLevel, base);
                else if (bi->SpellName)
                {
                    LocaleConstant const loc = sWorld->GetDefaultDbcLocale();
                    char const* name = (*bi->SpellName)[loc];
                    sSpellMgr->ForEachSpellInfo([&](SpellInfo const* si)
                    {
                        if (!si || si->Id == base || si->SpellFamilyName != bi->SpellFamilyName || !SpellAppliesAura(si))
                            return;
                        if (!si->SpellLevel || si->SpellLevel > 60 || si->HasAttribute(SPELL_ATTR0_PASSIVE) || !si->SpellName)
                            return;
                        char const* n = (*si->SpellName)[loc];
                        if (name && n && std::string_view(name) == std::string_view(n))
                            built.emplace_back(si->SpellLevel, si->Id);
                    });
                    // Ascending level; at one level the LOWEST id comes last, so the walk from the back takes the vanilla rank (Mark of the
                    // Wild: 9885, level 60) before the same-name spells added later (24752 a 30 minute variant, 1291335 and 1310503 retail
                    // versions). MC 2026-10-13 4839bcf3: the druids cast 1310503 and not one of 40 raiders showed the buff.
                    std::sort(built.begin(), built.end(), [](auto const& a, auto const& b) { return a.first != b.first ? a.first < b.first : a.second > b.second; });
                    std::string ids;
                    for (auto const& [lvl, id] : built)
                        ids += fmt::format(" {}(L{})", id, lvl);
                    TC_LOG_INFO("playerbot.v2", "[buff_plan] spell {} \"{}\" has no aura effect; same-name spells that do:{}", base, name ? name : "",
                                ids.empty() ? " none" : ids);
                }
            }
            it = cache.emplace(base, std::move(built)).first;
        }
        cands = it->second;
    }
    for (auto it = cands.rbegin(); it != cands.rend(); ++it)
        if (it->first <= s.level() && s.knows_spell(it->second))
            return it->second;
    return base;
}

// The rank of the party version this bot casts: the highest rank it knows
// whose reagents (spell data of that rank) are in the bags, walking down the
// chain so a bot with only the lower rank's reagent (Holy Candle, Wild
// Berries) casts the lower rank instead of failing the top one. 0 = none.
uint32 GroupBuffRank(BotSnapshotView const& s, ClassBuffDef const& d)
{
    if (!d.group)
        return 0;
    uint32 rank = s.resolve_rank(d.group);
    for (int guard = 0; rank && guard < 32; ++guard, rank = sSpellMgr->GetPrevSpellInChain(rank))
    {
        SpellInfo const* si = sSpellMgr->GetSpellInfo(rank, DIFFICULTY_NONE);
        if (!si || !SpellAppliesAura(si))
            continue;   // a rank with no effect (Gift of the Wild in Forever's data) buffs nobody
        bool have = true;
        for (size_t i = 0; i < si->Reagent.size() && have; ++i)
            if (si->Reagent[i] > 0 && si->ReagentCount[i] > 0
                && s.item_count(uint32(si->Reagent[i])) < uint32(si->ReagentCount[i]))
                have = false;
        if (have)
            return rank;
    }
    return 0;
}

// Own buff (solo bot): any rank of the single or the party version.
bool SelfHasBuff(BotSnapshotView const& s, ClassBuffDef const& d)
{
    for (uint32 spell : { d.single, d.group })
        if (spell)
            if (AuraEntry const* a = s.find_aura(spell); a && a->remaining.count() > kBuffRefreshMs)
                return true;
    return false;
}

// ---------------- Paladin auras ----------------

constexpr uint32 DEVOTION_AURA      = 465;
constexpr uint32 RETRIBUTION_AURA   = 7294;
constexpr uint32 CONCENTRATION_AURA = 19746;
constexpr uint32 SHADOW_RES_AURA    = 19876;
constexpr uint32 FROST_RES_AURA     = 19888;
constexpr uint32 FIRE_RES_AURA      = 19891;
constexpr uint32 SANCTITY_AURA      = 20218;   // Retribution talent
constexpr uint32 kPaladinAuras[]    = { DEVOTION_AURA, RETRIBUTION_AURA, CONCENTRATION_AURA, SHADOW_RES_AURA,
                                        FROST_RES_AURA, FIRE_RES_AURA, SANCTITY_AURA };
constexpr uint32 SPEC_PALADIN_HOLY  = 65;
constexpr uint32 SPEC_PALADIN_PROT  = 66;

uint32 ResistAura(Resist r)
{
    switch (r)
    {
        case Resist::Fire:   return FIRE_RES_AURA;
        case Resist::Frost:  return FROST_RES_AURA;
        case Resist::Shadow: return SHADOW_RES_AURA;
        default:             return 0;
    }
}

// The aura a paladin member holds itself (its own aura on itself), 0 if none.
uint32 HeldAura(GroupMemberSummary const& m)
{
    for (AuraEntry const& a : m.buffs)
        if (a.caster == m.guid)
            for (uint32 aura : kPaladinAuras)
                if (a.spell_id == aura)
                    return aura;
    return 0;
}

// Wish list of a paladin: Devotion where its party has a tank, the
// instance's resistance aura, then by its own role.
void AuraWishes(bool party_has_tank, Resist resist, uint32 spec, Role role, uint32 (&out)[8], size_t& n)
{
    n = 0;
    auto add = [&](uint32 a) { if (!a) return; for (size_t i = 0; i < n; ++i) if (out[i] == a) return; if (n < 8) out[n++] = a; };
    if (party_has_tank) add(DEVOTION_AURA);
    add(ResistAura(resist));
    if (role == Role::Healer || spec == SPEC_PALADIN_HOLY)
        for (uint32 a : { CONCENTRATION_AURA, DEVOTION_AURA, RETRIBUTION_AURA }) add(a);
    else if (role == Role::Tank || spec == SPEC_PALADIN_PROT)
        for (uint32 a : { DEVOTION_AURA, RETRIBUTION_AURA, CONCENTRATION_AURA }) add(a);
    else
        for (uint32 a : { SANCTITY_AURA, RETRIBUTION_AURA, DEVOTION_AURA, CONCENTRATION_AURA }) add(a);
}

// Whether another paladin could hold `aura` (spell level; Sanctity is a talent
// only the paladin itself can vouch for, via HeldAura).
bool OtherKnowsAura(GroupMemberSummary const& m, uint32 aura)
{
    if (aura == SANCTITY_AURA)
        return false;
    return m.level >= SpellLevel(aura);
}

// ---------------- Dispels ----------------

constexpr uint8 kMagic   = 1u << 0;
constexpr uint8 kCurse   = 1u << 1;
constexpr uint8 kPoison  = 1u << 2;
constexpr uint8 kDisease = 1u << 3;

uint8 TypeBit(DispelType t)
{
    switch (t)
    {
        case DispelType::Magic:   return kMagic;
        case DispelType::Curse:   return kCurse;
        case DispelType::Poison:  return kPoison;
        case DispelType::Disease: return kDisease;
        default:                  return 0;
    }
}

struct CureSpell
{
    uint8  cls;
    uint32 spell;          // rank 1 (cast resolves the highest known rank)
    uint8  types;
    uint32 abolish_aura;   // cleanse-over-time this spell leaves (tracked buff), 0 = none
};

// Better spell first per class and type.
constexpr CureSpell kCures[] = {
    { CLASS_MAGE,    475,  kCurse,                      0 },     // Remove Lesser Curse
    { CLASS_DRUID,   2782, kCurse,                      0 },     // Remove Curse
    { CLASS_DRUID,   2893, kPoison,                     2893 },  // Abolish Poison
    { CLASS_DRUID,   8946, kPoison,                     0 },     // Cure Poison
    { CLASS_PRIEST,  527,  kMagic,                      0 },     // Dispel Magic
    { CLASS_PRIEST,  552,  kDisease,                    552 },   // Abolish Disease
    { CLASS_PRIEST,  528,  kDisease,                    0 },     // Cure Disease
    { CLASS_PALADIN, 4987, kMagic | kPoison | kDisease, 0 },     // Cleanse
    { CLASS_PALADIN, 1152, kPoison | kDisease,          0 },     // Purify
    { CLASS_SHAMAN,  526,  kPoison,                     0 },     // Cure Poison
    { CLASS_SHAMAN,  2870, kDisease,                    0 },     // Cure Disease
};

// Debuff types a member can remove, from class, level and spec - only for a
// bot whose own gates (idle:classic_dispel, the rotation's assigned-dispel
// rule, PickDispel's self checks) will actually fire, so no job is handed to
// a bot that sits on it:
//   - a tank keeps its global cooldowns for threat (IsTankMember: a
//     Protection paladin runs kTankRules, which has no Cleanse);
//   - a Shadowform priest's rotation has no dispel rule (Holy spells);
//   - Feral / Guardian druids run the Cat / Bear rotations (no cure rule);
//   - a druid in any form can't cast it (Moonkin, Travel, a Restoration
//     druid that shifted);
//   - mounted (PickDispel and the idle gate skip a mounted bot);
//   - below kDispelMinManaPct mana (ShouldAssignedDispel, the idle gate).
// Bots only: a human dispels by hand.
uint8 MemberCureTypes(BotSnapshotView const& s, GroupMemberSummary const& m)
{
    if (!m.is_bot || !SameMapAlive(s, m) || IsTankMember(s, m) || m.is_mounted)
        return 0;
    if ((m.cls == CLASS_PRIEST && m.spec == SPEC_PRIEST_SHADOW)
        || (m.cls == CLASS_DRUID && (m.spec == 103 || m.spec == 104)) || MemberInDruidForm(m))
        return 0;
    if (m.max_mana > 0 && int64(m.mana) * 100 < int64(m.max_mana) * kDispelMinManaPct)
        return 0;
    uint8 types = 0;
    for (CureSpell const& c : kCures)
        if (c.cls == m.cls && m.level >= SpellLevel(c.spell))
            types |= c.types;
    return types;
}

struct DispelJob
{
    GroupMemberSummary const* member;
    uint8 type;
};

// ---------------- Encounter-owned dispels ----------------

struct OwnedDispel
{
    uint32 map_id;
    uint32 aura;
};

// Debuffs a raid encounter layer dispels itself. Empty until that layer
// registers its auras (RegisterEncounterOwnedDispel): without it the generic
// plan must remove everything, Molten Core's curses included.
//
// Readers are the bot worker threads, at any time. Each registration builds a
// new immutable list under g_ownedDispelsWrite and publishes it with one
// release store; readers take one acquire load and walk a list that never
// changes. Superseded lists are kept alive (registration happens a handful
// of times per process), so a reader still walking one is never left with a
// dangling pointer.
using OwnedDispelList = std::vector<OwnedDispel>;
std::atomic<OwnedDispelList const*> g_ownedDispels{nullptr};
std::mutex g_ownedDispelsWrite;
std::vector<std::unique_ptr<OwnedDispelList const>> g_ownedDispelsKeep;   // guarded by g_ownedDispelsWrite

// ---------------- Soulstone ----------------

constexpr uint32 SOUL_SHARD = 6265;
constexpr uint32 SOULSTONE_TRACKED = 20707;   // every rank of Soulstone Resurrection is recorded under it
// Create Soulstone (Minor .. Major) and the item each one makes, best last.
constexpr uint32 kCreateSoulstone[] = { 693, 20752, 20755, 20756, 20757 };
constexpr uint32 kSoulstoneItems[]  = { 5232, 16892, 16893, 16895, 16896 };

// ---------------- Summary log ----------------

std::atomic<uint32> g_raidSupportCounts[size_t(RaidSupportKind::Count)];
std::atomic<uint32> g_raidSupportLastLogMs{0};
// Monotonic per-map totals for the scenario runner (the minute counters above are reset by the summary line).
std::mutex g_raidSupportTotalsMu;
std::map<std::pair<uint32, uint8>, uint32> g_raidSupportTotals;

} // namespace

Resist InstanceResistance(uint32 map_id)
{
    return InstanceMainSchool(kInstanceSchools, map_id);
}

bool InstanceDealsSchool(uint32 map_id, Resist school)
{
    return InstanceDealsSchool(kInstanceSchools, map_id, school);
}

bool BossFears(uint32 map_id, uint32 creature_entry)
{
    struct FearBoss { uint32 map; uint32 entry; };
    static constexpr FearBoss kFearBosses[] = {
        { 409, 11982 },   // Molten Core: Magmadar (Panic)
        { 469, 11583 },   // Blackwing Lair: Nefarian (Bellowing Roar)
        { 249, 10184 },   // Onyxia's Lair: Onyxia (Bellowing Roar)
    };
    for (FearBoss const& b : kFearBosses)
        if (b.map == map_id && b.entry == creature_entry)
            return true;
    return false;
}

uint32 ResistPotionItem(Resist r)
{
    switch (r)
    {
        case Resist::Fire: return 13457;   // Greater Fire Protection Potion
        default:           return 0;
    }
}

uint32 PickResistPotion(BotSnapshotView const& s)
{
    uint32 item = s.raw().pve_order.active ? s.raw().pve_order.resist_potion : 0;
    // A carrier aura on the bot itself (Living Bomb) asks for its potion whatever the boss order says.
    if (!item)
        if (Raid::RaidRules const* rules = Raid::RulesForMap(s.map_id()))
            for (Raid::CarrierPotion const& cp : rules->carrierPotion)
                if (s.has_aura(cp.aura)) { item = cp.item; break; }
    if (!item || !s.in_combat() || s.is_casting() || s.item_count(item) == 0 || !ConsumablesAllowed(s))
        return 0;
    ConsumableInfo const& info = Classify(item);
    // The protection is already up, or the potion is on cooldown.
    if (!info.spell || s.has_aura(info.spell) || s.cd_remaining(info.spell).count() > 0)
        return 0;
    if (CombatConsumableUsedRecently(s))
        return 0;
    return item;
}

CastPick PickFearWard(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    constexpr uint32 FEAR_WARD = 6346;
    PveOrder const& po = s.raw().pve_order;
    if (s.cls() != CLASS_PRIEST || !po.active || po.fear_ward_target.IsEmpty() || s.is_mounted()
        || !CanCast(s, FEAR_WARD))
        return {};
    auto const* members = g.members();
    if (!members)
        return {};
    float bx, by, bz; s.position(bx, by, bz);
    for (GroupMemberSummary const& m : *members)
        if (m.guid == po.fear_ward_target)
        {
            if (!m.online || !m.is_alive || m.map_id != s.map_id()
                || Dist2(m.x, m.y, m.z, bx, by, bz) > 30.f * 30.f)
                return {};
            return { FEAR_WARD, m.guid };
        }
    return {};
}

namespace {

// Create Healthstone spells and the healthstones they make, from the spell
// data: warlock-family spells with a Create Item effect whose item is an
// instant-heal consumable (Classify). Spell and item stores are read-only
// after load; scanned once on first use.
struct HealthstoneSpell
{
    uint32 spell = 0;
    uint32 item = 0;
    uint32 level = 0;   // spell level: the best known rank wins
};

std::vector<HealthstoneSpell> const& HealthstoneSpells()
{
    static std::vector<HealthstoneSpell> const spells = []
    {
        std::vector<HealthstoneSpell> out;
        sSpellMgr->ForEachSpellInfo([&out](SpellInfo const* si)
        {
            if (!si || si->SpellFamilyName != SPELLFAMILY_WARLOCK)
                return;
            for (SpellEffectInfo const& e : si->GetEffects())
            {
                if (!e.IsEffect(SPELL_EFFECT_CREATE_ITEM) || !e.ItemType)
                    continue;
                if (!(Classify(e.ItemType).kinds & KindBit(ConsumableKind::Healthstone)))
                    continue;
                if (std::none_of(out.begin(), out.end(), [&](HealthstoneSpell const& h) { return h.spell == si->Id; }))
                    out.push_back({ si->Id, e.ItemType, si->SpellLevel });
            }
        });
        return out;
    }();
    return spells;
}

} // namespace

bool IsHealthstoneItem(uint32 item_entry)
{
    if (!item_entry)
        return false;
    for (HealthstoneSpell const& h : HealthstoneSpells())
        if (h.item == item_entry)
            return true;
    ConsumableInfo const& info = Classify(item_entry);
    return info.conjured && (info.kinds & KindBit(ConsumableKind::Healthstone));
}

bool HasHealthstone(BotSnapshotView const& s)
{
    for (InventoryItem const& it : s.raw().inventory.bag_items)
        if (it.item_class == ITEM_CLASS_CONSUMABLE && IsHealthstoneItem(it.entry))
            return true;
    return false;
}

uint32 CreateHealthstoneSpell(BotSnapshotView const& s)
{
    if (s.cls() != CLASS_WARLOCK)
        return 0;
    HealthstoneSpell const* best = nullptr;
    for (HealthstoneSpell const& h : HealthstoneSpells())
        if (s.knows_spell(h.spell) && (!best || h.level > best->level))
            best = &h;
    if (best)
        return best->spell;
    // Spell data whose Create Healthstone is a script effect (the retail 6201
    // casts the item-creating spell itself): the base spell.
    constexpr uint32 CREATE_HEALTHSTONE = 6201;
    return s.knows_spell(CREATE_HEALTHSTONE) ? CREATE_HEALTHSTONE : 0;
}

namespace {

// The item a Conjure Mana Gem spell creates (its Create Item effect), 0 when the spell data has none.
uint32 ManaGemItemOf(uint32 spell)
{
    SpellInfo const* si = sSpellMgr->GetSpellInfo(spell, DIFFICULTY_NONE);
    if (!si)
        return 0;
    for (SpellEffectInfo const& e : si->GetEffects())
        if (e.IsEffect(SPELL_EFFECT_CREATE_ITEM) && e.ItemType)
            return e.ItemType;
    return 0;
}

} // namespace

bool HasManaGem(BotSnapshotView const& s)
{
    for (InventoryItem const& it : s.raw().inventory.bag_items)
    {
        // The item the conjure spells create, by entry: whatever the item data's class, subclass or conjured flag say.
        for (uint32 const spell : { 10054u, 10053u, 3552u, 759u })
            if (it.entry == ManaGemItemOf(spell))
                return true;
        if (it.item_class == ITEM_CLASS_CONSUMABLE)
        {
            ConsumableInfo const& info = Classify(it.entry);
            if (info.conjured && (info.kinds & KindBit(ConsumableKind::ManaItem)))
                return true;
        }
    }
    return false;
}

uint32 CreateManaGemSpell(BotSnapshotView const& s)
{
    if (s.cls() != CLASS_MAGE)
        return 0;
    // Conjure Mana Ruby 10054 (58), Jade 10053 (48), Citrine 3552 (38), Agate 759 (28)
    for (uint32 const spell : { 10054u, 10053u, 3552u, 759u })
        if (s.knows_spell(spell))
            return spell;
    return 0;
}

namespace {

// Conjure Water / Conjure Food spells, found like the healthstones: mage-family spells with a
// Create Item effect whose item is a drink or food (Classify). Scanned once on first use.
struct ConjureSpell
{
    uint32 spell = 0;
    uint32 item = 0;
    uint32 level = 0;
    ConsumableKind kind = ConsumableKind::Food;
};

std::vector<ConjureSpell> const& MageConjureSpells()
{
    static std::vector<ConjureSpell> const spells = []
    {
        std::vector<ConjureSpell> out;
        sSpellMgr->ForEachSpellInfo([&out](SpellInfo const* si)
        {
            if (!si || si->SpellFamilyName != SPELLFAMILY_MAGE)
                return;
            for (SpellEffectInfo const& e : si->GetEffects())
            {
                if (!e.IsEffect(SPELL_EFFECT_CREATE_ITEM) || !e.ItemType)
                    continue;
                ConsumableInfo const& info = Classify(e.ItemType);
                if (!info.conjured)
                    continue;
                ConsumableKind kind;
                if (info.kinds & KindBit(ConsumableKind::Drink))
                    kind = ConsumableKind::Drink;
                else if (info.kinds & KindBit(ConsumableKind::Food))
                    kind = ConsumableKind::Food;
                else
                    continue;
                if (std::none_of(out.begin(), out.end(), [&](ConjureSpell const& c) { return c.spell == si->Id; }))
                    out.push_back({ si->Id, e.ItemType, si->SpellLevel, kind });
            }
        });
        return out;
    }();
    return spells;
}

} // namespace

uint32 BestMageConjure(ConsumableKind kind, std::function<bool(uint32)> const& knows)
{
    ConjureSpell const* best = nullptr;
    for (ConjureSpell const& c : MageConjureSpells())
        if (c.kind == kind && knows(c.spell) && (!best || c.level > best->level))
            best = &c;
    return best ? best->spell : 0;
}

bool MageConjuresFamily(uint8 cls, std::string_view family, std::function<bool(uint32)> const& knows)
{
    if (cls != CLASS_MAGE)
        return false;
    if (family == "Water")
        return BestMageConjure(ConsumableKind::Drink, knows) != 0;
    if (family == "Food")
        return BestMageConjure(ConsumableKind::Food, knows) != 0;
    return false;
}

uint32 MageConjureSpell(BotSnapshotView const& s, ConsumableKind kind, uint32 want_below)
{
    if (s.cls() != CLASS_MAGE || (kind != ConsumableKind::Drink && kind != ConsumableKind::Food))
        return 0;
    uint32 const family_top = BestMageConjure(kind, [&](uint32 id) { return s.knows_spell(id); });
    if (!family_top)
        return 0;
    // knows_spell answers for the whole rank family, so family_top is the highest rank of the data, not of the spellbook (a level 20
    // mage asked for rank 7 Conjure Food and got API_NOT_KNOWN on every try, 2026-10-14 Deadmines): cast the rank it has learned.
    uint32 const spell = s.resolve_rank(family_top);
    if (!spell)
        return 0;
    uint32 have = 0;
    for (InventoryItem const& it : s.raw().inventory.bag_items)
        if (it.item_class == ITEM_CLASS_CONSUMABLE && Classify(it.entry).conjured
            && (Classify(it.entry).kinds & KindBit(kind)))
            have += it.count;
    return have < want_below ? spell : 0;
}

bool MemberHasBuff(GroupSnapshotView const& g, ObjectGuid guid, uint32 tracked_id)
{
    if (auto const* members = g.members())
        for (GroupMemberSummary const& m : *members)
            if (m.guid == guid)
                return HasTracked(m, tracked_id, 0);
    return false;
}

MissingBuff MissingGroupBuff(BotSnapshotView const& s, GroupSnapshotView const& g,
                             float x, float y, float z, float range)
{
    MissingBuff best;
    if (!IsVanillaRealm())
        return best;
    auto const* members = g.members();
    if (!members || members->empty())
        return best;
    float const r2 = range * range;
    auto in_range = [&](GroupMemberSummary const& m)
    { return SameMapAlive(s, m) && MemberDist2(m, x, y, z) <= r2; };
    auto consider = [&](uint32 spell, char const* name, uint32 n)
    {
        if (n > best.members) best = { spell, name, n };
    };
    auto buff_name = [](uint32 spell) -> char const*
    {
        switch (spell)
        {
            case 1243:  return "Fortitude";
            case 14752: return "Divine Spirit";
            case 976:   return "Shadow Protection";
            case 1459:  return "Arcane Intellect";
            case 1126:  return "Mark of the Wild";
            case 467:   return "Thorns";
            default:    return "class buff";
        }
    };

    thread_local std::vector<GroupMemberSummary const*> casters;
    for (ClassBuffDef const& d : kClassBuffs)
    {
        if (d.raid_only && !g.is_raid())
            continue;
        if (!InstanceDealsSchool(s.map_id(), d.school))
            continue;
        BuffCasters(s, *members, d, casters);
        if (casters.empty())
            continue;
        uint32 n = 0;
        for (GroupMemberSummary const& m : *members)
        {
            if (!in_range(m)) continue;
            if (d.who == BuffWho::ManaUsers && m.max_mana <= 0) continue;
            if (d.who == BuffWho::Tanks && !IsTankMember(s, m)) continue;
            if (!HasTracked(m, d.single, 0)) ++n;
        }
        consider(d.single, buff_name(d.single), n);
    }

    // Blessings: any blessing counts (the plan picks which one per class).
    bool paladin = false;
    uint32 const might_level = SpellLevel(kBlessings[MIGHT].normal);
    for (GroupMemberSummary const& m : *members)
        if (m.cls == CLASS_PALADIN && m.is_bot && SameMapAlive(s, m) && m.level >= might_level)
        { paladin = true; break; }
    if (paladin)
    {
        uint32 n = 0;
        for (GroupMemberSummary const& m : *members)
        {
            if (!in_range(m)) continue;
            bool blessed = false;
            for (size_t b = 0; b < std::size(kBlessings) && !blessed; ++b)
                blessed = HasBlessing(m.buffs, Blessing(b), 0);
            if (!blessed) ++n;
        }
        consider(kBlessings[MIGHT].normal, "a paladin blessing", n);
    }

    // Paladin aura: a party (subgroup) aura reaching 30y around the paladin.
    // A member of a subgroup with a live bot paladin on the map, within 30y
    // of one, holding no paladin aura at all. idle:classic_aura recasts the
    // aura out of combat, so it is in place before the pull.
    {
        uint32 n = 0;
        for (GroupMemberSummary const& m : *members)
        {
            if (!in_range(m)) continue;
            bool covered_by_paladin = false;
            for (GroupMemberSummary const& p : *members)
                if (p.cls == CLASS_PALADIN && p.is_bot && p.subgroup == m.subgroup && SameMapAlive(s, p)
                    && MemberDist2(m, p.x, p.y, p.z) <= 30.0f * 30.0f)
                { covered_by_paladin = true; break; }
            if (!covered_by_paladin) continue;
            bool has_aura = false;
            for (AuraEntry const& a : m.buffs)
            {
                for (uint32 aura : kPaladinAuras)
                    if (a.spell_id == aura) { has_aura = true; break; }
                if (has_aura) break;
            }
            if (!has_aura) ++n;
        }
        consider(kPaladinAuras[0], "a paladin aura", n);
    }

    // Not held for: warrior Battle Shout (the Classic warrior rotation only
    // shouts with 10+ rage, and out of combat the rage has decayed to 0, so
    // no upkeep rule can put it up before the pull) and shaman totems
    // (PickTotem drops them from the in-combat rotation only, on a live
    // target). Holding the pull for either would only burn the 30s cap.
    return best;
}

namespace {

// A class buff whose single spell has no explicit unit target (Mark of the Wild in the Forever data: the cast log shows an empty
// target, whoever the bot aimed at) is cast around the caster: it reaches the caster's party, or its raid, in range, not a chosen
// member. Picking a target per member for it casts the same spell once per member (MC 2026-10-12: 42 + 40 casts of a 20 mana spell in a
// 40-man raid, none of the picked targets showing the buff afterwards).
struct AreaBuff
{
    bool area = false;
    bool party_only = true;   // reaches only the caster's subgroup
};

AreaBuff AreaBuffOf(uint32 spell)
{
    AreaBuff out;
    SpellInfo const* si = spell ? sSpellMgr->GetSpellInfo(spell, DIFFICULTY_NONE) : nullptr;
    if (!si)
        return out;
    {
        // Once per spell: how the data targets it (the cast log's target column is empty for such a spell).
        static std::mutex seen_mu;
        static std::unordered_set<uint32> seen;
        bool first;
        {
            std::lock_guard<std::mutex> lk(seen_mu);
            first = seen.insert(spell).second;
        }
        if (first)
        {
            std::string targets;
            for (SpellEffectInfo const& e : si->GetEffects())
                targets += fmt::format(" [{}/{}]", uint32(e.TargetA.GetTarget()), uint32(e.TargetB.GetTarget()));
            char const* n = si->SpellName ? (*si->SpellName)[sWorld->GetDefaultDbcLocale()] : "";
            TC_LOG_INFO("playerbot.v2", "[buff_plan] spell {} \"{}\": explicit unit target {}, targets area {}, effect targets A/B{}", spell, n ? n : "",
                        si->NeedsExplicitUnitTarget() ? "yes" : "no", si->IsTargetingArea() ? "yes" : "no", targets);
        }
    }
    if (si->NeedsExplicitUnitTarget() || !si->IsTargetingArea())
        return out;
    out.area = true;
    for (SpellEffectInfo const& e : si->GetEffects())
        for (Targets const t : { e.TargetA.GetTarget(), e.TargetB.GetTarget() })
            if (t == TARGET_UNIT_CASTER_AREA_RAID || t == TARGET_UNIT_TARGET_ALLY_OR_RAID)
                out.party_only = false;
    return out;
}

// Back-off of such a cast that reached nobody: the needy count after a cast was no lower than before it, twice running (the spell
// does not reach those members: another subgroup, out of its radius, or caster only). The buff then waits kAreaBuffBackoffMs.
constexpr uint32 kAreaBuffBackoffMs = 5u * 60u * 1000u;
struct AreaBuffState { uint32 last_needy = 0; uint32 stalls = 0; uint32 backoff_until_ms = 0; };
std::mutex g_area_buff_mu;
std::unordered_map<uint64, AreaBuffState> g_area_buff;   // (caster counter << 8) ^ buff index

bool AreaBuffBackedOff(ObjectGuid caster, size_t di)
{
    uint32 const now = GameTime::GetGameTimeMS();
    std::lock_guard<std::mutex> lk(g_area_buff_mu);
    auto const it = g_area_buff.find((caster.GetCounter() << 8) ^ di);
    return it != g_area_buff.end() && it->second.backoff_until_ms && int32(it->second.backoff_until_ms - now) > 0;
}

// The class buff this bot owes, whether or not it is shapeshifted right now
// (the plan does not depend on the form: see BuffCasters).
CastPick ClassBuffPick(BotSnapshotView const& s, GroupSnapshotView const& g, PickSkip skip)
{
    if (!IsVanillaRealm() || s.is_mounted() || s.in_combat())
        return {};
    auto const* members = g.members();
    bool const grouped = members && !members->empty();
    float bx, by, bz; s.position(bx, by, bz);
    thread_local std::vector<GroupMemberSummary const*> casters;

    for (size_t di = 0; di < std::size(kClassBuffs); ++di)
    {
        ClassBuffDef const& d = kClassBuffs[di];
        if (d.cls != s.cls() || !s.knows_spell(d.single))
            continue;
        // The spell this buff is cast with (see EffectiveBuffSpell); a buff with no spell that applies an aura is not cast at all.
        uint32 const single = EffectiveBuffSpell(s, d.single);
        if (!SpellAppliesAura(sSpellMgr->GetSpellInfo(single, DIFFICULTY_NONE)))
            continue;
        if (d.raid_only && !g.is_raid())
            continue;
        if (!InstanceDealsSchool(s.map_id(), d.school))
            continue;

        if (!grouped)
        {
            // Solo: the bot's own buff (Thorns only on a tank-spec druid).
            if (d.who == BuffWho::Tanks && !IsTankSpec(s.cls(), uint16(s.spec())))
                continue;
            if (d.who == BuffWho::ManaUsers && s.max_power(POWER_MANA) <= 0)
                continue;
            // Paced like the group picks: a cast whose aura the snapshot does not show must not repeat every tick.
            uint64 const key = (5ull << 48) | uint64(di);
            if (!SelfHasBuff(s, d) && s.is_ready(single) && !skip(key))
                return { single, s.guid(), key };
            continue;
        }

        BuffCasters(s, *members, d, casters);
        auto const me = std::find_if(casters.begin(), casters.end(),
                                     [&s](GroupMemberSummary const* c) { return c->guid == s.guid(); });
        if (me == casters.end())
            continue;

        if (AreaBuff const area = AreaBuffOf(single); area.area)
        {
            // One cast from this caster reaches its party (or raid) in range: owed when a member it reaches lacks the buff.
            GroupMemberSummary const* const mine = *me;
            if (AreaBuffBackedOff(s.guid(), di))
                continue;
            uint32 needy = 0;
            for (GroupMemberSummary const& m : *members)
            {
                if (!NeedsBuff(s, d, m)) continue;
                if (area.party_only && m.subgroup != mine->subgroup) continue;
                if (m.guid != s.guid() && MemberDist2(m, bx, by, bz) > kBuffCastRange * kBuffCastRange) continue;
                ++needy;
            }
            if (!needy)
                continue;
            // Casters of the same subgroup share the work: the lowest guid casts (a raid-wide spell: every caster that reaches someone).
            if (area.party_only)
                if (GroupMemberSummary const* lead = *std::find_if(casters.begin(), casters.end(),
                        [&](GroupMemberSummary const* c) { return c->subgroup == mine->subgroup; });
                    lead && lead->guid != s.guid())
                    continue;
            uint64 const key = (6ull << 48) | uint64(di);
            if (!s.is_ready(single) || skip(key))
                continue;
            CastPick pick{ single, s.guid(), key };
            pick.area_needy = needy;
            return pick;
        }

        uint32 const group_rank = GroupBuffRank(s, d);
        for (uint8 sg = 0; sg < 8; ++sg)
        {
            // Which caster owns this subgroup: among the casters within cast
            // range of one of its needy members (all of them when none is),
            // the subgroup number picks one. Same answer on every bot.
            auto near_needy = [&](GroupMemberSummary const* c)
            {
                for (GroupMemberSummary const& m : *members)
                    if (m.subgroup == sg && NeedsBuff(s, d, m)
                        && MemberDist2(m, c->x, c->y, c->z) <= kBuffCastRange * kBuffCastRange)
                        return true;
                return false;
            };
            bool any_needy = false;
            for (GroupMemberSummary const& m : *members)
                if (m.subgroup == sg && NeedsBuff(s, d, m)) { any_needy = true; break; }
            if (!any_needy)
                continue;
            thread_local std::vector<GroupMemberSummary const*> eligible;
            eligible.clear();
            for (GroupMemberSummary const* c : casters)
                if (near_needy(c))
                    eligible.push_back(c);
            std::vector<GroupMemberSummary const*> const& pool = eligible.empty() ? casters : eligible;
            GroupMemberSummary const* owner = pool[sg % pool.size()];
            if (!owner || owner->guid != s.guid())
                continue;

            // This bot's subgroup: the needy members in cast range.
            GroupMemberSummary const* first = nullptr;
            int needy = 0;
            for (GroupMemberSummary const& m : *members)
            {
                if (m.subgroup != sg || !NeedsBuff(s, d, m)) continue;
                if (m.guid != s.guid() && MemberDist2(m, bx, by, bz) > kBuffCastRange * kBuffCastRange) continue;
                ++needy;
                if (!first) first = &m;
            }
            if (!first)
                continue;
            // MC 2026-10-12: 82 single Mark of the Wild and 44 single Power Word: Fortitude casts across two 40-man runs, no Gift of the
            // Wild or Prayer of Fortitude at all. Name why the party version is not used, once a minute per caster and buff.
            if (d.group && needy >= 2 && !group_rank)
            {
                static std::mutex why_mu;
                static std::unordered_map<uint64, uint32> why_last;
                uint32 const now = GameTime::GetGameTimeMS();
                bool due = false;
                {
                    std::lock_guard<std::mutex> lk(why_mu);
                    uint32& l = why_last[(s.guid().GetCounter() << 20) ^ d.single];
                    if (!l || now - l >= 60000u) { l = now ? now : 1u; due = true; }
                }
                if (due)
                {
                    uint32 const known = s.resolve_rank(d.group);
                    std::string reagents;
                    if (SpellInfo const* si = known ? sSpellMgr->GetSpellInfo(known, DIFFICULTY_NONE) : nullptr)
                        for (size_t i = 0; i < si->Reagent.size(); ++i)
                            if (si->Reagent[i] > 0 && si->ReagentCount[i] > 0)
                                reagents += fmt::format(" {}x{}(has {})", si->ReagentCount[i], si->Reagent[i], s.item_count(uint32(si->Reagent[i])));
                    TC_LOG_INFO("playerbot.v2", "[buff_plan] {} casts single {} on {} needy of subgroup {}: no party version ({}; known rank {}, reagents{})",
                                s.name(), d.single, needy, uint32(sg), known ? "reagents short" : "spell not known", known, reagents.empty() ? " none" : reagents);
                }
            }
            if (group_rank && needy >= 2)
            {
                // The party version covers the subgroup: while it is paced
                // (cast moments ago, the snapshot not caught up) or not ready,
                // leave the subgroup alone rather than single-buff the
                // members it is about to cover.
                uint64 const key = (3ull << 48) | (uint64(di) << 8) | sg;
                if (!s.is_ready(group_rank) || skip(key))
                    continue;
                bool const top = group_rank == s.resolve_rank(d.group);
                return { top ? d.group : group_rank, first->guid, key, !top };
            }
            for (GroupMemberSummary const& m : *members)
            {
                if (m.subgroup != sg || !NeedsBuff(s, d, m)) continue;
                if (m.guid != s.guid() && MemberDist2(m, bx, by, bz) > kBuffCastRange * kBuffCastRange) continue;
                uint64 const key = (4ull << 48) | (uint64(di) << 40) | (m.guid.GetCounter() & 0xFFFFFFFFFFull);
                if (s.is_ready(single) && !skip(key))
                    return { single, m.guid, key };
            }
        }
    }
    return {};
}

// Forms an out-of-combat druid leaves to cast its class buff: Cat, Bear,
// Dire Bear, Travel, Moonkin. Aquatic Form is left alone (under water it is
// the druid's breath).
constexpr uint32 kLeavableDruidForms[] = { 768, 5487, 9634, 783, 24858 };
constexpr uint32 kAquaticForm = 1066;
constexpr uint32 kCatForm = 768, kBearForm = 5487, kDireBearForm = 9634;
// Open-world drink threshold of idle:ooc_eat_food (OocConsumableRules.cpp
// kOocDrinkBelowManaPct): a druid that is about to drink stays out of form.
constexpr int32 kFormDrinkBelowManaPct = 70;

} // namespace

void NoteAreaBuffCast(ObjectGuid caster, uint64 key, uint32 needy, uint32 now_ms)
{
    if ((key >> 48) != 6)
        return;
    size_t const di = size_t(key & 0xFFFFu);
    std::lock_guard<std::mutex> lk(g_area_buff_mu);
    AreaBuffState& st = g_area_buff[(caster.GetCounter() << 8) ^ di];
    // This cast comes after the last one's trust window: were that many members, or more, still needy, it reached nobody new.
    if (st.last_needy && needy >= st.last_needy)
        ++st.stalls;
    else
        st.stalls = 0;
    st.last_needy = needy;
    if (st.stalls >= 2)
    {
        st.backoff_until_ms = now_ms + kAreaBuffBackoffMs;
        st.stalls = 0;
        st.last_needy = 0;
        TC_LOG_INFO("playerbot.v2", "[buff_plan] caster {} buff index {}: {} members still needy after two caster-centred casts, backing off {} s", caster.GetCounter(), di,
                    needy, kAreaBuffBackoffMs / 1000u);
    }
}

CastPick PickClassBuff(BotSnapshotView const& s, GroupSnapshotView const& g, PickSkip skip)
{
    // A druid in a form can't cast Mark of the Wild / Thorns: it leaves the
    // form first (DruidFormToLeave, idle:classic_druid_form).
    if (SelfInDruidForm(s))
        return {};
    return ClassBuffPick(s, g, skip);
}

uint32 DruidFormToLeave(BotSnapshotView const& s, GroupSnapshotView const& g, PickSkip skip)
{
    if (!IsVanillaRealm() || s.cls() != CLASS_DRUID || s.in_combat() || s.is_mounted() || s.has_aura(kAquaticForm))
        return 0;
    uint32 form = 0;
    for (uint32 f : kLeavableDruidForms)
        if (s.has_aura(f)) { form = f; break; }
    if (!form)
        return 0;
    return ClassBuffPick(s, g, skip).spell ? form : 0;
}

uint32 DruidFormToEnter(BotSnapshotView const& s, GroupSnapshotView const& g, PickSkip skip)
{
    if (!IsVanillaRealm() || s.cls() != CLASS_DRUID || s.in_combat() || s.is_mounted()
        || !s.is_in_instance() || !g.exists() || SelfInDruidForm(s))
        return 0;
    // A druid the group counts as its healer never goes back into a form, whatever its spec says:
    // every heal fails NOT_SHAPESHIFT in one.
    if (s.my_role() == Role::Healer)
        return 0;
    uint32 form = 0;
    switch (s.spec())
    {
        case 103: form = kCatForm; break;
        case 104: form = s.knows_spell(kDireBearForm) ? kDireBearForm : kBearForm; break;
        default:  return 0;
    }
    if (!s.knows_spell(form) || !s.is_ready(form))
        return 0;
    // Buffs first, and no form while eating / drinking or about to (food
    // and drink can't be used shapeshifted).
    if (ClassBuffPick(s, g, skip).spell)
        return 0;
    RecoveryPick const rec = PickRecovery(s, 90, OocDrinkStartManaPct(s, &g, kFormDrinkBelowManaPct));
    if (rec.item || rec.hold)
        return 0;
    return form;
}

uint32 PickPaladinAura(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    if (!IsVanillaRealm() || s.cls() != CLASS_PALADIN || s.is_mounted())
        return 0;
    Resist const resist = AuraResistance(s);
    uint32 wishes[8];
    size_t n = 0;

    // The aura this bot holds now (its own, on itself).
    uint32 held = 0;
    for (AuraEntry const& a : s.raw().auras.own_auras)
    {
        if (a.caster != s.guid()) continue;
        for (uint32 aura : kPaladinAuras)
            if (SpellFamily::Same(a.spell_id, aura)) { held = aura; break; }
        if (held) break;
    }

    auto const* members = g.members();
    uint32 pick = 0;
    if (!members || members->empty())
    {
        Role const role = IsTankSpec(CLASS_PALADIN, uint16(s.spec())) ? Role::Tank
                        : IsHealerSpec(CLASS_PALADIN, uint16(s.spec())) ? Role::Healer : Role::Dps;
        AuraWishes(role == Role::Tank, resist, s.spec(), role, wishes, n);
        for (size_t i = 0; i < n && !pick; ++i)
            if (s.knows_spell(wishes[i])) pick = wishes[i];
    }
    else
    {
        uint8 const sg = SelfSubgroup(s, g);
        bool party_has_tank = false;
        for (GroupMemberSummary const& m : *members)
            if (m.subgroup == sg && m.online && IsTankMember(s, m)) { party_has_tank = true; break; }

        // Auras taken in this party: human paladins' first (they don't follow
        // the plan), then each bot paladin by guid takes its first free wish.
        uint32 taken[8];
        size_t nt = 0;
        auto is_taken = [&](uint32 a) { for (size_t i = 0; i < nt; ++i) if (taken[i] == a) return true; return false; };
        thread_local std::vector<GroupMemberSummary const*> pals;
        pals.clear();
        for (GroupMemberSummary const& m : *members)
        {
            if (m.cls != CLASS_PALADIN || m.subgroup != sg || !SameMapAlive(s, m)) continue;
            if (!m.is_bot)
            {
                if (uint32 const h = HeldAura(m); h && nt < 8) taken[nt++] = h;
                continue;
            }
            pals.push_back(&m);
        }
        std::sort(pals.begin(), pals.end(), [](GroupMemberSummary const* a, GroupMemberSummary const* b) { return a->guid < b->guid; });
        for (GroupMemberSummary const* p : pals)
        {
            bool const me = p->guid == s.guid();
            AuraWishes(party_has_tank, resist, p->spec, p->role, wishes, n);
            uint32 choice = 0;
            if (!me)
            {
                // Another bot paladin: what it holds when that is one of its
                // free wishes, else its first free wish it can know.
                uint32 const h = HeldAura(*p);
                for (size_t i = 0; i < n && !choice; ++i)
                    if (wishes[i] == h && !is_taken(h)) choice = h;
                for (size_t i = 0; i < n && !choice; ++i)
                    if (!is_taken(wishes[i]) && OtherKnowsAura(*p, wishes[i])) choice = wishes[i];
            }
            else
                for (size_t i = 0; i < n && !choice; ++i)
                    if (!is_taken(wishes[i]) && s.knows_spell(wishes[i])) choice = wishes[i];
            if (me)
            {
                pick = choice;
                break;
            }
            if (choice && nt < 8)
                taken[nt++] = choice;
        }
        // Every wish taken (more paladins than auras worth holding): keep
        // whatever is up, or any aura nobody in the party holds.
        if (!pick && !held)
            for (uint32 aura : kPaladinAuras)
                if (!is_taken(aura) && s.knows_spell(aura)) { pick = aura; break; }
    }
    if (!pick || pick == held || !s.is_ready(pick))
        return 0;
    return pick;
}

bool IsEncounterOwnedDispel(uint32 map_id, uint32 aura_id)
{
    if (OwnedDispelList const* list = g_ownedDispels.load(std::memory_order_acquire))
        for (OwnedDispel const& o : *list)
            if (o.map_id == map_id && o.aura == aura_id)
                return true;
    // The raid layer's own rules for every raid map (Bot/Raid/RaidEncounterData_*.cpp): its dispel jobs, mind-controlled
    // members and do-not-dispel auras. Read-only after the registry's one-time init, so safe from the bot workers; this is
    // why the raid layer never calls RegisterEncounterOwnedDispel.
    return Raid::OwnsAura(map_id, aura_id);
}

void RegisterEncounterOwnedDispel(uint32 map_id, uint32 aura_id)
{
    std::lock_guard<std::mutex> lock(g_ownedDispelsWrite);
    OwnedDispelList const* cur = g_ownedDispels.load(std::memory_order_relaxed);
    if (cur)
        for (OwnedDispel const& o : *cur)
            if (o.map_id == map_id && o.aura == aura_id)
                return;
    auto next = std::make_unique<OwnedDispelList>(cur ? *cur : OwnedDispelList{});
    next->push_back({ map_id, aura_id });
    OwnedDispelList const* published = next.get();
    g_ownedDispelsKeep.push_back(std::move(next));   // owned before it is visible
    g_ownedDispels.store(published, std::memory_order_release);
    TC_LOG_INFO("playerbot.v2", "[raid_support] encounter-owned dispel registered: map {} aura {}", map_id, aura_id);
}

CastPick PickDispel(BotSnapshotView const& s, GroupSnapshotView const& g, PickSkip skip)
{
    // The same self gates MemberCureTypes applies to every planned dispeller.
    if (!IsVanillaRealm() || s.is_mounted() || SelfInDruidForm(s))
        return {};
    if (s.max_power(POWER_MANA) > 0 && s.power_pct(POWER_MANA) < kDispelMinManaPct)
        return {};
    uint8 known = 0;
    for (CureSpell const& c : kCures)
        if (c.cls == s.cls() && s.knows_spell(c.spell))
            known |= c.types;
    if (!known)
        return {};

    // The spell this bot uses on a debuff type (Abolish first, skipped on a
    // member already under somebody's Abolish of that type).
    auto spell_for = [&s](uint8 type) -> CureSpell const*
    {
        for (CureSpell const& c : kCures)
            if (c.cls == s.cls() && (c.types & type) && s.knows_spell(c.spell))
                return &c;
        return nullptr;
    };

    auto const* members = g.members();
    if (!members || members->empty())
    {
        for (DispelType t : { DispelType::Curse, DispelType::Magic, DispelType::Poison, DispelType::Disease })
        {
            uint8 const bit = TypeBit(t);
            CureSpell const* c = (known & bit) ? spell_for(bit) : nullptr;
            if (c && s.self_dispellable(t) && !(c->abolish_aura && s.has_aura(c->abolish_aura)) && s.is_ready(c->spell))
                return { c->spell, s.guid(), 0 };
        }
        return {};
    }

    // The work: one job per (member, debuff type) on the bot's map.
    thread_local std::vector<DispelJob> jobs;
    jobs.clear();
    for (GroupMemberSummary const& m : *members)
    {
        if (!SameMapAlive(s, m)) continue;
        uint8 types = 0;
        for (AuraEntry const& a : m.debuffs)
            if (a.is_harmful && !IsEncounterOwnedDispel(s.map_id(), a.spell_id))
                types |= TypeBit(a.dispel_type);
        if ((types & kPoison) && HasTracked(m, 2893, 0)) types &= ~kPoison;    // Abolish Poison already ticking
        if ((types & kDisease) && HasTracked(m, 552, 0)) types &= ~kDisease;   // Abolish Disease already ticking
        for (uint8 bit : { kCurse, kMagic, kPoison, kDisease })
            if (types & bit)
                jobs.push_back({ &m, bit });
    }
    if (jobs.empty())
        return {};
    std::sort(jobs.begin(), jobs.end(), [](DispelJob const& a, DispelJob const& b)
    {
        return a.member->guid != b.member->guid ? a.member->guid < b.member->guid : a.type < b.type;
    });

    // The dispellers, by guid, with what they can remove and their load.
    // Every bot judges every dispeller (itself included) by the same rule,
    // so the plan agrees across the raid.
    struct Dispeller { GroupMemberSummary const* m; uint8 types; uint32 load; };
    thread_local std::vector<Dispeller> dispellers;
    dispellers.clear();
    bool me_listed = false;
    for (GroupMemberSummary const& m : *members)
        if (uint8 const t = MemberCureTypes(s, m))
        {
            dispellers.push_back({ &m, t, 0 });
            me_listed |= m.guid == s.guid();
        }
    if (!me_listed)
        return {};
    std::sort(dispellers.begin(), dispellers.end(), [](Dispeller const& a, Dispeller const& b) { return a.m->guid < b.m->guid; });

    // Greedy, identical on every bot: each job goes to the least-loaded
    // capable dispeller within range of the member; ties rotate by the
    // member's guid so equal dispellers spread over the raid.
    for (DispelJob const& job : jobs)
    {
        GroupMemberSummary const& m = *job.member;
        Dispeller* best = nullptr;
        size_t const n = dispellers.size();
        size_t const start = n ? size_t(m.guid.GetCounter() % n) : 0;
        for (size_t i = 0; i < n; ++i)
        {
            Dispeller& d = dispellers[(start + i) % n];
            if (!(d.types & job.type)) continue;
            if (d.m->guid != m.guid && MemberDist2(m, d.m->x, d.m->y, d.m->z) > 30.0f * 30.0f) continue;
            if (!best || d.load < best->load)
                best = &d;
        }
        if (!best)
            continue;
        ++best->load;
        if (best->m->guid != s.guid())
            continue;
        // Paced (cast on this member's debuff of this type moments ago: out
        // of line of sight, or the snapshot not caught up): the job stays
        // this bot's, so it moves on to its next one.
        uint64 const key = (5ull << 48) | (uint64(job.type) << 40) | (m.guid.GetCounter() & 0xFFFFFFFFFFull);
        if (skip(key))
            continue;
        CureSpell const* c = spell_for(job.type);
        if (!c || !s.is_ready(c->spell))
            return {};
        return { c->spell, m.guid, key };
    }
    return {};
}

SoulstonePick PickSoulstone(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    if (!IsVanillaRealm() || s.cls() != CLASS_WARLOCK || s.is_mounted() || s.in_combat())
        return {};
    auto const* members = g.members();
    if (!members || members->empty())
        return {};

    // Warlock k stones ward k (each tier by guid): the healers who cannot
    // bring themselves back - priests, paladins, shamans (a shaman's
    // Reincarnation is on a long cooldown) - then the druid healers (Rebirth
    // can raise the others), then the tanks. The first warlock always covers
    // a non-druid healer when the raid has one.
    thread_local std::vector<GroupMemberSummary const*> locks, wards;
    locks.clear();
    wards.clear();
    for (GroupMemberSummary const& m : *members)
        if (m.cls == CLASS_WARLOCK && m.is_bot && SameMapAlive(s, m))
            locks.push_back(&m);
    for (int pass = 0; pass < 3; ++pass)
    {
        size_t const before = wards.size();
        for (GroupMemberSummary const& m : *members)
            if (SameMapAlive(s, m)
                && (pass == 0 ? m.role == Role::Healer && m.cls != CLASS_DRUID
                  : pass == 1 ? m.role == Role::Healer && m.cls == CLASS_DRUID
                  : IsTankMember(s, m) && m.role != Role::Healer))
                wards.push_back(&m);
        std::sort(wards.begin() + before, wards.end(),
                  [](GroupMemberSummary const* a, GroupMemberSummary const* b) { return a->guid < b->guid; });
    }
    std::sort(locks.begin(), locks.end(), [](GroupMemberSummary const* a, GroupMemberSummary const* b) { return a->guid < b->guid; });
    size_t index = 0;
    while (index < locks.size() && locks[index]->guid != s.guid()) ++index;
    if (index >= locks.size() || index >= wards.size())
        return {};
    GroupMemberSummary const& ward = *wards[index];
    if (HasTracked(ward, SOULSTONE_TRACKED, 0))
        return {};

    // A stone in the bags: use it on the ward (30 min shared cooldown).
    for (size_t i = std::size(kSoulstoneItems); i-- > 0;)
    {
        uint32 const item = kSoulstoneItems[i];
        if (s.item_count(item) == 0) continue;
        ConsumableInfo const& info = Classify(item);
        if (info.spell && s.cd_remaining(info.spell).count() > 0)
            return {};
        float bx, by, bz; s.position(bx, by, bz);
        if (MemberDist2(ward, bx, by, bz) > kBuffCastRange * kBuffCastRange)
            return {};
        return { 0, item, ward.guid };
    }
    // None: make the best one the warlock knows (a 3 s cast, one Soul Shard).
    if (s.item_count(SOUL_SHARD) == 0)
        return {};
    for (size_t i = std::size(kCreateSoulstone); i-- > 0;)
    {
        uint32 const spell = kCreateSoulstone[i];
        if (s.resolve_rank(spell) == spell && s.is_ready(spell))
            return { spell, 0, ObjectGuid::Empty };
    }
    return {};
}

void NoteRaidSupport(RaidSupportKind kind, uint32 map_id)
{
    {
        std::lock_guard<std::mutex> lk(g_raidSupportTotalsMu);
        ++g_raidSupportTotals[{ map_id, uint8(kind) }];
    }
    // Tranq / Decurse / FormationGiveUp have their own log lines and a scenario tally; the minute line is the original five.
    if (kind > RaidSupportKind::Soulstone)
        return;
    g_raidSupportCounts[size_t(kind)].fetch_add(1, std::memory_order_relaxed);
    uint32 const now = GameTime::GetGameTimeMS();
    uint32 last = g_raidSupportLastLogMs.load(std::memory_order_relaxed);
    if (now - last < 60'000u || !g_raidSupportLastLogMs.compare_exchange_strong(last, now))
        return;
    uint32 c[size_t(RaidSupportKind::Soulstone) + 1];
    for (size_t i = 0; i < std::size(c); ++i)
        c[i] = g_raidSupportCounts[i].exchange(0, std::memory_order_relaxed);
    TC_LOG_INFO("playerbot.v2", "[raid_support] last minute: buffs={} blessings={} auras={} dispels={} soulstones={}",
                c[0], c[1], c[2], c[3], c[4]);
}

uint32 RaidSupportTotal(RaidSupportKind kind, uint32 map_id)
{
    std::lock_guard<std::mutex> lk(g_raidSupportTotalsMu);
    auto const it = g_raidSupportTotals.find({ map_id, uint8(kind) });
    return it == g_raidSupportTotals.end() ? 0u : it->second;
}

bool IsCurseDispelSpell(uint32 spell)
{
    SpellInfo const* si = spell ? sSpellMgr->GetSpellInfo(spell, DIFFICULTY_NONE) : nullptr;
    if (!si)
        return false;
    for (SpellEffectInfo const& eff : si->GetEffects())
        if (eff.IsEffect(SPELL_EFFECT_DISPEL) && eff.MiscValue == int32(DISPEL_CURSE))
            return true;
    return false;
}

} // namespace Playerbot::Classic
