#include "GroupSnapshotBuilder.h"
#include "Bot/SpellFamily.h"
#include "TrackedBuffMatch.h"
#include "ClassicSpec.h"
#include "../Bot/ClassTables.h"  // canonical IsTankSpec / IsHealerSpec
#include "../Services.h"                    // Lifecycle() accessor
#include "../Fleet/BotIdentityRegistry.h"   // is_bot definition (Services.h fwd-declares)
#include "Player.h"
#include "Group.h"
#include "GameTime.h"
#include "Unit.h"
#include "ObjectAccessor.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Spell.h"
#include "LFG.h"
#include "Log.h"
#include <mutex>
#include <unordered_set>
#include <shared_mutex>
#include <span>
#include <string_view>
#include <unordered_map>
#include "SharedDefines.h"

namespace Playerbot {

namespace {

// Same dispel-id translation table as BotSnapshotBuilder (DRY-violating
// duplicate kept here so the Group module doesn't depend on Bot internals).
DispelType TranslateDispelType(uint32 core)
{
    switch (core)
    {
        case 1:  return DispelType::Magic;
        case 2:  return DispelType::Curse;
        case 3:  return DispelType::Disease;
        case 4:  return DispelType::Poison;
        case 9:  return DispelType::Enrage;
        default: return DispelType::None;
    }
}

// Long-duration raid buffs the State_Idle group-buff maintainer cares about.
// Kept tiny on purpose — every entry costs N members * sizeof(AuraEntry) per
// snapshot. Add an id here only when a class-buff rule needs it. Order
// doesn't matter; the lookup is linear.
// Classic 1.60: buffs come in rank chains and group versions, so each entry
// maps the first rank of a chain to the id the buff rules ask about
// (ClassSelfBuff); every rank of the chain is recorded under that id.
struct TrackedRaidBuff
{
    uint32 chain_first;
    uint32 recorded_as;
};

constexpr TrackedRaidBuff kTrackedRaidBuffs[] = {
    { 1243,   1243   },   // Power Word: Fortitude   (Priest)
    { 21562,  1243   },   // Prayer of Fortitude     (Priest, group version)
    { 1126,   1126   },   // Mark of the Wild        (Druid)
    { 21849,  1126   },   // Gift of the Wild        (Druid, group version)
    { 6673,   6673   },   // Battle Shout            (Warrior)
    { 1459,   1459   },   // Arcane Intellect        (Mage)
    { 23028,  1459   },   // Arcane Brilliance       (Mage, group version)
    // Paladin blessings (Classic): normal and Greater recorded as the normal
    // rank 1, for the blessing assignment in Bot/ClassicUpkeep.cpp.
    { 19740,  19740  },   // Blessing of Might
    { 25782,  19740  },   // Greater Blessing of Might
    { 19742,  19742  },   // Blessing of Wisdom
    { 25894,  19742  },   // Greater Blessing of Wisdom
    { 20217,  20217  },   // Blessing of Kings
    { 25898,  20217  },   // Greater Blessing of Kings
    { 1038,   1038   },   // Blessing of Salvation
    { 25895,  1038   },   // Greater Blessing of Salvation
    { 19977,  19977  },   // Blessing of Light
    { 25890,  19977  },   // Greater Blessing of Light
    { 20911,  20911  },   // Blessing of Sanctuary
    { 25899,  20911  },   // Greater Blessing of Sanctuary
    // Classic raid support (Bot/ClassicUpkeep.cpp): the rest of the vanilla
    // class buffs, the paladin auras (recorded with their caster, so two
    // paladins of a subgroup never hold the same aura), the cleanse-over-
    // time spells (a member under someone's Abolish is not dispelled again)
    // and the single-target cooldowns several bots could stack on one ally.
    { 14752,  14752  },   // Divine Spirit           (Priest, Discipline talent)
    { 27681,  14752  },   // Prayer of Spirit        (Priest, group version)
    { 976,    976    },   // Shadow Protection       (Priest)
    { 27683,  976    },   // Prayer of Shadow Protection
    { 467,    467    },   // Thorns                  (Druid)
    { 465,    465    },   // Devotion Aura           (Paladin)
    { 7294,   7294   },   // Retribution Aura
    { 19746,  19746  },   // Concentration Aura
    { 19876,  19876  },   // Shadow Resistance Aura
    { 19888,  19888  },   // Frost Resistance Aura
    { 19891,  19891  },   // Fire Resistance Aura
    { 20218,  20218  },   // Sanctity Aura           (Retribution talent)
    { 2893,   2893   },   // Abolish Poison          (Druid)
    { 552,    552    },   // Abolish Disease         (Priest)
    { 29166,  29166  },   // Innervate               (Druid)
    { 10060,  10060  },   // Power Infusion          (Priest, Discipline talent)
    { 20707,  20707  },   // Soulstone Resurrection  (Minor Soulstone)
    { 20762,  20707  },   //   Lesser Soulstone
    { 20763,  20707  },   //   Soulstone
    { 20764,  20707  },   //   Greater Soulstone
    { 20765,  20707  },   //   Major Soulstone
    // Druid shapeshift forms (no duration: remaining reads 0). A druid in a
    // form can't cast caster spells, so the raid plans in ClassicUpkeep.cpp
    // don't hand it a class buff or a dispel (MemberInDruidForm).
    { 768,    768    },   // Cat Form
    { 5487,   5487   },   // Bear Form
    { 9634,   9634   },   // Dire Bear Form
    { 783,    783    },   // Travel Form
    { 1066,   1066   },   // Aquatic Form
    { 24858,  24858  },   // Moonkin Form            (Balance talent)
    { 364342, 364342 },   // Blessing of the Bronze  (Evoker)
    { 462854, 462854 },   // Skyfury                 (Shaman)
};

// Returns the id to record the buff under, 0 when it isn't tracked. By id first (any rank of the chain or family), then by spell
// name: see TrackedBuffMatch.h. The answer per spell id never changes, so it is memoised (every positive aura of every member is
// asked about on every snapshot).
uint32 TrackedRaidBuffId(uint32 spell_id)
{
    static std::shared_mutex mu;
    static std::unordered_map<uint32, uint32> memo;
    {
        std::shared_lock lk(mu);
        if (auto it = memo.find(spell_id); it != memo.end())
            return it->second;
    }
    static TrackedBuffEntry table[std::size(kTrackedRaidBuffs)];
    static std::once_flag table_once;
    std::call_once(table_once, [] { for (size_t i = 0; i < std::size(kTrackedRaidBuffs); ++i) table[i] = { kTrackedRaidBuffs[i].chain_first, kTrackedRaidBuffs[i].recorded_as }; });
    LocaleConstant const loc = sWorld->GetDefaultDbcLocale();
    auto name_of = [loc](uint32 id) -> std::string_view
    {
        SpellInfo const* si = sSpellMgr->GetSpellInfo(id, DIFFICULTY_NONE);
        char const* n = si && si->SpellName ? (*si->SpellName)[loc] : nullptr;
        return n ? std::string_view(n) : std::string_view();
    };
    TrackedMatch const m = MatchTrackedBuff(spell_id, std::span<TrackedBuffEntry const>(table), [](uint32 a, uint32 b) { return SpellFamily::Same(a, b); }, name_of);
    if (m.how == TrackedHow::ByName)
        TC_LOG_INFO("playerbot.v2", "[buff_track] aura {} ({}) is not a rank of any tracked id by chain or family; recorded as {} by its name",
                    spell_id, std::string(name_of(spell_id)), m.recorded_as);
    {
        std::unique_lock lk(mu);
        memo.emplace(spell_id, m.recorded_as);
    }
    return m.recorded_as;
}

// Single-walk filler for both tracked raid buffs and harmful debuffs on
// `u`. Replaces a prior two-pass design that walked GetAppliedAuras
// twice; at 40-member raids × N grouped bots × snapshot cadence this
// adds up. Each applied aura is classified once (positive=buff path,
// otherwise debuff path) and pushed to the matching out-vector.
void CopyMemberAuras(Unit const* u,
                     std::vector<AuraEntry>& buffs_out,
                     std::vector<AuraEntry>& debuffs_out)
{
    if (!u) return;
    auto const& applied = u->GetAppliedAuras();
    debuffs_out.reserve(applied.size());
    for (auto const& [id, app] : applied)
    {
        if (!app) continue;
        Aura const* base = app->GetBase();
        if (!base) continue;
        SpellInfo const* si = base->GetSpellInfo();
        if (!si) continue;

        const bool positive = app->IsPositive();
        if (positive)
        {
            // Buffs we only care about a small fixed whitelist — the
            // long-duration raid buffs the State_Idle group-buff
            // maintainer needs to avoid double-casting.
            uint32 const tracked_id = TrackedRaidBuffId(si->Id);
            if (!tracked_id)
            {
                // MC Magmadar 2026-10-10: the targets of every blessing showed no tracked buff and the paladins recast them all fight.
                // Once per spell id, name any long positive aura that looks like a raid buff and is not tracked, so the next log shows
                // which id the cast actually applies.
                if (base->GetMaxDuration() >= 120000)
                {
                    static std::mutex untracked_mu;
                    static std::unordered_set<uint32> untracked_seen;
                    bool first;
                    {
                        std::lock_guard<std::mutex> lk(untracked_mu);
                        first = untracked_seen.insert(si->Id).second;
                    }
                    if (first)
                    {
                        char const* n = si->SpellName ? (*si->SpellName)[sWorld->GetDefaultDbcLocale()] : nullptr;
                        TC_LOG_INFO("playerbot.v2", "[buff_track] untracked positive aura {} \"{}\" lasts {} s", si->Id, n ? n : "", base->GetMaxDuration() / 1000);
                    }
                }
                continue;
            }
            AuraEntry e{};
            e.spell_id    = tracked_id;
            e.stacks      = base->GetStackAmount();
            const int32 dur = base->GetDuration();
            e.remaining   = Ms{dur > 0 ? dur : 0};
            e.caster      = base->GetCasterGUID();
            e.dispel_type = DispelType::None;
            e.mechanic    = si->Mechanic;
            e.is_harmful  = false;
            e.is_stealable = false;
            buffs_out.push_back(e);
        }
        else
        {
            // Healers query this list to find dispel candidates per
            // group member.
            AuraEntry e{};
            e.spell_id    = si->Id;
            e.stacks      = base->GetStackAmount();
            const int32 dur = base->GetDuration();
            e.remaining   = Ms{dur > 0 ? dur : 0};
            e.caster      = base->GetCasterGUID();
            e.dispel_type = TranslateDispelType(si->Dispel);
            e.mechanic    = si->Mechanic;
            // Bleed promotion mirrors BotSnapshotBuilder — let
            // bleed-clearing helpers find the debuff via DispelType::Bleed.
            if (e.dispel_type == DispelType::None && si->Mechanic == MECHANIC_BLEED)
                e.dispel_type = DispelType::Bleed;
            e.is_harmful  = true;
            e.is_stealable = false;
            debuffs_out.push_back(e);
        }
    }
}

// Translate a MemberSlot's role flags into our compact Role enum. The LFG
// role bits cover tank/healer/damage; main-tank flag (set by ` /mt` or RAID
// commands) takes precedence when present. When neither flag is set (typical
// for player-led groups that didn't queue via LFG), fall back to class+spec
// inference so healers/tanks are still identifiable. Spec → role mapping
// lives in Bot/ClassTables.cpp — single source for all callers.
Role InferRoleFromSpec(uint8 cls, uint32 spec)
{
    if (IsTankSpec(cls, uint16(spec)))   return Role::Tank;
    if (IsHealerSpec(cls, uint16(spec))) return Role::Healer;
    return cls != 0 ? Role::Dps : Role::Unknown;
}

Role TranslateRole(uint8 lfg_role_bits, uint8 group_flags, uint8 cls, uint32 spec)
{
    if (group_flags & MEMBER_FLAG_MAINTANK)      return Role::Tank;
    if (lfg_role_bits & lfg::PLAYER_ROLE_TANK)   return Role::Tank;
    if (lfg_role_bits & lfg::PLAYER_ROLE_HEALER) return Role::Healer;
    if (lfg_role_bits & lfg::PLAYER_ROLE_DAMAGE) return Role::Dps;
    return InferRoleFromSpec(cls, spec);
}

} // anonymous

std::shared_ptr<GroupSnapshot const> GroupSnapshotBuilder::Build(Player* p, SnapshotVer version)
{
    if (!p) return {};
    Group* g = p->GetGroup();
    if (!g) return {};

    auto snap = std::make_shared<GroupSnapshot>();
    snap->version        = version;
    snap->group_guid     = g->GetGUID();
    snap->leader         = g->GetLeaderGUID();
    snap->is_raid        = g->isRaidGroup();
    snap->is_lfg         = g->isLFGGroup();
    snap->loot_method    = static_cast<uint8>(g->GetLootMethod());
    snap->ready_check_active = g->IsReadyCheckStarted();
    // Raid marks (Skull/Cross/etc.) — used by future kill-priority and CC
    // assignment rules. Empty for parties since GroupBuilder clears them.
    for (uint8 i = 0; i < snap->raid_marks.size(); ++i)
        snap->raid_marks[i] = g->GetTargetIcon(i);

    snap->members.reserve(g->GetMembersCount());
    for (auto const& slot : g->GetMemberSlots())
    {
        GroupMemberSummary m{};
        m.guid    = slot.guid;
        m.name    = slot.name;
        m.cls     = slot._class;
        m.subgroup = slot.group;
        m.is_bot  = Services::Initialized() &&
                    Services::Lifecycle().is_bot(slot.guid.GetCounter());

        // Live state from the actual Player if they're online & in-world.
        if (Player* mp = ObjectAccessor::FindConnectedPlayer(slot.guid))
        {
            m.online       = true;
            m.spec         = ::Playerbot::Classic::EffectiveSpecId(mp);
            m.level        = mp->GetLevel();
            m.hp           = static_cast<int32>(mp->GetHealth());
            m.max_hp       = static_cast<int32>(mp->GetMaxHealth());
            m.mana         = static_cast<int32>(mp->GetPower(POWER_MANA));
            m.max_mana     = static_cast<int32>(mp->GetMaxPower(POWER_MANA));
            m.in_combat    = mp->IsInCombat();
            m.is_controlled = mp->HasUnitState(UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED | UNIT_STATE_STUNNED);
            m.is_mounted   = mp->IsMounted();
            m.is_alive     = mp->IsAlive();
            m.rez_pending  = mp->IsResurrectRequested();
            m.is_ghost     = mp->HasPlayerFlag(PLAYER_FLAGS_GHOST);
            if (Unit* v = mp->GetVictim())
                m.victim   = v->GetGUID();
            // Fallback to the SELECTED target when there's no melee victim yet.
            // A member chasing an out-of-melee-range mob (the tank StuckChasing a
            // harbor 47403 Mechanical Reaper it can't path to, or a ranged class
            // mid-engage) has GetVictim()==null but GetTarget()==the mob, so
            // without this the GROUP snapshot shows an EMPTY victim and the
            // DPS-assist / harbor-dps-open rules can't focus what the tank is
            // actually engaging — the group stalls (live 2026-06-29: tank stuck on
            // a 47403, member.victim empty, idle:dungeon_harbor_dps_open fired 0x).
            // Mirrors the bot's OWN-snapshot victim seeding (BotSnapshotBuilder
            // ~2442): in-combat + a live, legally-attackable hostile selection only
            // (never seed a friendly heal-target a healer tab'd).
            if (m.victim.IsEmpty() && mp->IsInCombat())
                if (ObjectGuid t = mp->GetTarget(); !t.IsEmpty() && t != mp->GetGUID())
                    if (Unit* sel = ObjectAccessor::GetUnit(*mp, t);
                        sel && sel->IsAlive() && mp->IsValidAttackTarget(sel))
                        m.victim = sel->GetGUID();
            m.x            = mp->GetPositionX();
            m.y            = mp->GetPositionY();
            m.z            = mp->GetPositionZ();
            m.o            = mp->GetOrientation();
            m.map_id       = mp->GetMapId();
            // Mirror what bot's own snapshot tracks for itself, so dispatchers
            // can reason about teammates the same way (e.g. interrupt-coord
            // when a healer is mid-cast, or skip Mass Dispel if everyone's CC
            // already broke).
            for (uint32 slot_t : { CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL })
            {
                if (Spell* casting = mp->GetCurrentSpell(static_cast<CurrentSpellTypes>(slot_t)))
                {
                    m.is_casting    = true;
                    m.casting_spell = casting->m_spellInfo->Id;
                    // By GUID: the Spell's raw object pointer is only refreshed on the spell's own update, so a target that despawned or died in the
                    // last map update (a whelp, n33 onyxia_full crash 02:38) is a dangling pointer here.
                    m.casting_target = casting->m_targets.GetUnitTargetGUID();
                    m.casting_end_ms = GameTime::GetGameTimeMS()
                        + static_cast<uint32>(std::max<int32>(casting->GetRemainingCastTime(), 0));
                    // [raid_rez] cast state: a resurrection in flight (MC 2026-10-08: every healer waited on a rez cast that never
                    // landed and never logged a failure) - spell state, time left and the target's life, once per 5 s per caster.
                    if (casting->m_spellInfo->HasEffect(SPELL_EFFECT_RESURRECT) || casting->m_spellInfo->HasEffect(SPELL_EFFECT_RESURRECT_NEW) || casting->m_spellInfo->HasEffect(SPELL_EFFECT_RESURRECT_WITH_AURA))
                    {
                        static std::mutex rez_mu;
                        static std::unordered_map<uint64, uint32> rez_last;
                        uint32 const rnow = GameTime::GetGameTimeMS();
                        bool due = false;
                        {
                            std::lock_guard<std::mutex> lk(rez_mu);
                            uint32& l = rez_last[mp->GetGUID().GetCounter()];
                            if (!l || uint32(rnow - l) >= 5000u) { l = rnow; due = true; }
                        }
                        if (due)
                        {
                            Unit* rt = ObjectAccessor::GetUnit(*mp, casting->m_targets.GetUnitTargetGUID());
                            TC_LOG_INFO("playerbot.v2",
                                "[raid_rez] cast state: {} spell {} state={} time_left={} ms target={} alive={} ghost={} resurrect_requested={}",
                                mp->GetName(), casting->m_spellInfo->Id, uint32(casting->getState()), casting->GetRemainingCastTime(),
                                rt ? rt->GetName() : std::string("-"), rt ? int(rt->IsAlive()) : -1,
                                rt && rt->IsPlayer() ? int(rt->ToPlayer()->HasPlayerFlag(PLAYER_FLAGS_GHOST)) : -1,
                                rt && rt->IsPlayer() ? int(rt->ToPlayer()->IsResurrectRequested()) : -1);
                        }
                    }
                    break;
                }
            }
            CopyMemberAuras(mp, m.buffs, m.debuffs);
            for (size_t i = 0; i < sizeof(kGroupClassBuffSpells) / sizeof(kGroupClassBuffSpells[0]); ++i)
                if (mp->HasSpell(kGroupClassBuffSpells[i]))
                    m.known_class_buffs |= uint8(1u << i);
        }
        else
        {
            m.online       = false;
        }

        // Resolve role last so spec inference (when needed) sees populated cls/spec.
        m.role = TranslateRole(slot.roles, slot.flags, m.cls, m.spec);
        m.is_main_tank = (slot.flags & MEMBER_FLAG_MAINTANK) != 0;

        snap->members.push_back(std::move(m));
    }

    return snap;
}

} // namespace Playerbot
