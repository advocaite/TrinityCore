// ClassicUpkeepRules - out-of-combat class upkeep for Classic 1.60 (Forever) bots.
//
//   idle:classic_supplies  top up class tools and reagents, food, water, potions
//                          and bandages (ClassicSupplies)
//   idle:classic_weapon    shaman weapon imbue / rogue poisons
//   idle:classic_blessings paladin blessings, split between the group's paladins
//   idle:classic_raid_buffs priest / mage / druid class buffs (Fortitude, Spirit,
//                          Shadow Protection, Intellect, Mark, Thorns on tanks),
//                          one caster per subgroup, Prayer / Brilliance / Gift
//   idle:classic_druid_form a druid that owes a class buff leaves its form to
//                          cast it; a feral druid shifts back before the pull
//   idle:classic_aura      paladin aura, distinct per paladin of a party, the
//                          instance's resistance aura (Molten Core: fire)
//   idle:classic_dispel    curses, magic, poison, disease between pulls, one
//                          capable bot per debuff (same plan as the rotations)
//   idle:classic_soulstone warlocks keep a Soulstone on a healer
//   idle:classic_healthstone warlocks keep a healthstone in the bags inside
//                          dungeons / raids (the dungeon hold swallows the
//                          world idle:conjured_item there)
//   idle:classic_mana_gem  mages keep a conjured mana gem in the bags inside
//                          dungeons / raids
//   idle:classic_fear_ward dwarf priests keep Fear Ward on their raid tank
//
// The decisions live in Bot/ClassicUpkeep.cpp, shared with the vanilla
// rotations (which refresh blessings and imbues that drop mid-fight and drop
// the totems). Priority 992: above the owner rules (idle:owner_following at
// 990 claims every out-of-combat tick of a bot told to follow, and
// idle:owner_escort an ungrouped owned bot's), so a bot at its owner's side
// still buffs and poisons; below combat:opener (993). Only between fights:
// nothing attacking the bot or the party, no target picked.

#include "Log.h"
#include "Bot/ConjureGate.h"
#include <unordered_map>
#include <mutex>
#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/ClassicUpkeep.h"
#include "Services.h"
#include "SharedDefines.h"
#include "Util/ConfigReader.h"
#include <vector>

namespace Playerbot {

namespace {

// Settled between fights and free to cast.
bool UpkeepWindow(BotSnapshotView const& s, GroupSnapshotView const& g)
{
    if (!s.is_alive() || s.in_combat() || s.is_casting() || s.is_mounted() || s.on_transport()
        || s.raw().vehicle.on_vehicle || !s.victim().IsEmpty() || s.attackers_count() > 0)
        return false;
    // The 992 band sits above the lava / drowning rules (900-865): get out of the hazard before any buff.
    if (s.is_in_damaging_liquid() || s.is_swimming())
        return false;
    if (auto const* members = g.members())
        for (GroupMemberSummary const& m : *members)
            if (m.in_combat && m.map_id == s.map_id())
                return false;
    return true;
}

// ---------- idle:classic_supplies ----------

// Class tools and reagents, and every class's food, water, potions and
// bandages: free, like ike3's factory stock (PlayerbotFactory::InitReagents /
// InitFood / InitPotions), so a companion never runs dry.
bool SuppliesGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!(Services::Config().classic_class_supplies() || Services::Config().behavior_ammo_supply()) || !UpkeepWindow(s, g))
        return false;
    thread_local std::vector<uint32> shortItems;
    Classic::ShortClassSupplies(s, shortItems);
    for (uint32 item : shortItems)
        if (!ai.action_recently_tried(BotAI::ActionKind::ClassSupply, item, now_ms))
            return true;
    return false;
}
bool SuppliesFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    // One restock covers every short item; lock out each of them so an item
    // the bot can't take (bags full, no Poisons skill yet) waits 5 minutes
    // without hiding the families behind it.
    thread_local std::vector<uint32> shortItems;
    Classic::ShortClassSupplies(s, shortItems);
    if (shortItems.empty() || !emit.restock_class_supplies())
        return false;
    for (uint32 item : shortItems)
        ai.note_action_retry(BotAI::ActionKind::ClassSupply, item, now_ms);
    ai.set_last_rule_fired("idle:classic_supplies");
    return true;
}

// ---------- idle:classic_weapon ----------

bool WeaponGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!UpkeepWindow(s, g))
        return false;
    if (Classic::CastPick const imbue = Classic::PickImbue(s, false); imbue.spell)
        return !ai.action_recently_tried(BotAI::ActionKind::ClassUpkeep, imbue.spell, now_ms);
    if (s.is_moving())   // a poison is a 3s cast
        return false;
    Classic::ItemPick const poison = Classic::PickPoison(s);
    return poison.item && !ai.action_recently_tried(BotAI::ActionKind::ClassUpkeep, poison.item, now_ms);
}
bool WeaponFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    if (Classic::CastPick const imbue = Classic::PickImbue(s, false); imbue.spell)
    {
        if (!emit.cast_on_item(imbue.spell, imbue.target))
            return false;
        ai.note_action_retry(BotAI::ActionKind::ClassUpkeep, imbue.spell, now_ms);
        ai.set_last_rule_fired("idle:classic_weapon:imbue");
        return true;
    }
    Classic::ItemPick const poison = Classic::PickPoison(s);
    if (!poison.item || !emit.use_item_by_entry(poison.item, poison.weapon))
        return false;
    ai.note_action_retry(BotAI::ActionKind::ClassUpkeep, poison.item, now_ms);
    ai.set_last_rule_fired("idle:classic_weapon:poison");
    return true;
}

// ---------- idle:classic_blessings ----------

// Raid support picks skip a unit of work (a Greater Blessing's class, a
// Prayer's subgroup, a member) cast on in the last 30 s: the group snapshot
// lags the cast, and a second Greater Blessing or Prayer on the next member
// of the same class / party would only burn another reagent.
struct SkipCtx
{
    BotAI const* ai;
    uint32 now_ms;
};
bool SkipRecent(void const* ctx, uint64 key)
{
    SkipCtx const& c = *static_cast<SkipCtx const*>(ctx);
    return c.ai->action_recently_tried(BotAI::ActionKind::ClassUpkeep, key, c.now_ms) || c.ai->buff_cast_trusted(key, c.now_ms);
}

// Blessings and class buffs run at 992, above idle:ooc_eat_food (875): after
// a wipe a priest or paladin would spend its mana on forty single buffs and
// stand up from its drink to do it. Below kBuffMinManaPct it drinks first;
// while drinking it sits until topped up (Classic::PickRecovery's 95%).
constexpr int32 kBuffMinManaPct   = 45;
constexpr int32 kBuffDrinkDonePct = 95;

bool BuffManaOk(BotSnapshotView const& s)
{
    if (s.max_power(0) <= 0)
        return true;
    int32 const mana = s.power_pct(0);
    if (mana < kBuffMinManaPct)
        return false;
    return !(Classic::IsDrinking(s) && mana < kBuffDrinkDonePct);
}

bool BlessingsGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    SkipCtx const ctx{ &ai, now_ms };
    return UpkeepWindow(s, g) && BuffManaOk(s) && Classic::PickBlessing(s, g, false, { &SkipRecent, &ctx }).spell != 0;
}
bool BlessingsFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32 now_ms)
{
    SkipCtx const ctx{ &ai, now_ms };
    Classic::CastPick const b = Classic::PickBlessing(s, g, false, { &SkipRecent, &ctx });
    if (!b.spell || !emit.cast(b.spell, b.target))
        return false;
    if (b.key)
    {
        ai.note_action_retry(BotAI::ActionKind::ClassUpkeep, b.key, now_ms);
        ai.note_buff_cast(b.key, now_ms);
    }
    // [raid_support] why this blessing goes out (MC 2026-10-08: 45-105 blessings a minute): the target's tracked buffs as the
    // group snapshot sees them, once per paladin and key per minute.
    {
        static std::mutex logMu;
        static std::unordered_map<uint64, uint32> lastLog;
        bool due = false;
        {
            std::lock_guard<std::mutex> lk(logMu);
            uint32& l = lastLog[(s.guid().GetCounter() << 20) ^ b.key];
            if (!l || now_ms - l >= 60000u) { l = now_ms ? now_ms : 1u; due = true; }
        }
        if (due)
        {
            std::string seen;
            if (auto const* members = g.members())
                for (GroupMemberSummary const& m : *members)
                    if (m.guid == b.target)
                        for (AuraEntry const& a : m.buffs)
                            seen += fmt::format("{}{}:{}s", seen.empty() ? "" : " ", a.spell_id, a.remaining.count() / 1000);
            TC_LOG_INFO("playerbot.v2", "[raid_support] {} casts spell {} on {} (key {:x}): target's tracked buffs [{}]", s.name(), b.spell,
                        b.target.GetCounter(), b.key, seen);
        }
    }
    Classic::NoteRaidSupport(Classic::RaidSupportKind::Blessing);
    ai.set_last_rule_fired("idle:classic_blessings");
    return true;
}

// ---------- idle:classic_raid_buffs ----------

bool RaidBuffsGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!UpkeepWindow(s, g) || !BuffManaOk(s))
        return false;
    SkipCtx const ctx{ &ai, now_ms };
    return Classic::PickClassBuff(s, g, { &SkipRecent, &ctx }).spell != 0;
}
bool RaidBuffsFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32 now_ms)
{
    SkipCtx const ctx{ &ai, now_ms };
    Classic::CastPick const b = Classic::PickClassBuff(s, g, { &SkipRecent, &ctx });
    if (!b.spell || !(b.exact_rank ? emit.cast_rank(b.spell, b.target) : emit.cast(b.spell, b.target)))
        return false;
    if (b.key)
    {
        ai.note_action_retry(BotAI::ActionKind::ClassUpkeep, b.key, now_ms);
        ai.note_buff_cast(b.key, now_ms);
        Classic::NoteAreaBuffCast(s.guid(), b.key, b.area_needy, now_ms);
    }
    // [raid_support] why this class buff goes out (MC Garr 2026-10-10: the druids cast Mark of the Wild over and over): the target's
    // tracked buffs as the group snapshot sees them, once per caster and key per minute.
    {
        static std::mutex logMu;
        static std::unordered_map<uint64, uint32> lastLog;
        bool due = false;
        {
            std::lock_guard<std::mutex> lk(logMu);
            uint32& l = lastLog[(s.guid().GetCounter() << 20) ^ b.key];
            if (!l || now_ms - l >= 60000u) { l = now_ms ? now_ms : 1u; due = true; }
        }
        if (due)
        {
            std::string seen;
            if (auto const* members = g.members())
                for (GroupMemberSummary const& m : *members)
                    if (m.guid == b.target)
                        for (AuraEntry const& a : m.buffs)
                            seen += fmt::format("{}{}:{}s", seen.empty() ? "" : " ", a.spell_id, a.remaining.count() / 1000);
            TC_LOG_INFO("playerbot.v2", "[raid_support] {} casts buff {} on {} (key {:x}, exact_rank {}, group exists {} members {}): target's tracked buffs [{}]", s.name(),
                        b.spell, b.target.GetCounter(), b.key, b.exact_rank, int(g.exists()), g.members() ? g.members()->size() : 0u, seen);
        }
    }
    Classic::NoteRaidSupport(Classic::RaidSupportKind::Buff);
    ai.set_last_rule_fired("idle:classic_raid_buffs");
    return true;
}

// ---------- idle:classic_druid_form ----------
// The one owner of a druid's out-of-combat form: a druid that owes a class
// buff (Mark / Gift of the Wild, Thorns) leaves its form so
// idle:classic_raid_buffs can cast it (the pull buff hold waits on the same
// plan), and a feral druid in an instance group shifts back into Cat / Bear
// Form once it owes nothing, so it starts the pull in form. In combat the
// rotations own the form. Commitment, so the two never flap: no shift back
// within 6 s of leaving (ActionKind::DruidFormLeft) (the buff casts go out first), no leaving
// again within 10 s of shifting back (DruidFormEntered) (a buff that can't land is
// paced 30 s by its own key anyway).

bool DruidFormGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (s.cls() != CLASS_DRUID || !UpkeepWindow(s, g))
        return false;
    SkipCtx const ctx{ &ai, now_ms };
    if (Classic::DruidFormToLeave(s, g, { &SkipRecent, &ctx }))
        return BuffManaOk(s) && !ai.action_recently_tried(BotAI::ActionKind::DruidFormEntered, 0, now_ms);
    return Classic::DruidFormToEnter(s, g, { &SkipRecent, &ctx }) != 0
        && !ai.action_recently_tried(BotAI::ActionKind::DruidFormLeft, 0, now_ms);
}
bool DruidFormFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32 now_ms)
{
    SkipCtx const ctx{ &ai, now_ms };
    if (uint32 const form = Classic::DruidFormToLeave(s, g, { &SkipRecent, &ctx }))
    {
        if (!emit.emit(CancelAuraIntent{ form }))
            return false;
        ai.note_action_retry(BotAI::ActionKind::DruidFormLeft, 0, now_ms);
        ai.set_last_rule_fired("idle:classic_druid_form:leave");
        return true;
    }
    uint32 const form = Classic::DruidFormToEnter(s, g, { &SkipRecent, &ctx });
    if (!form || !emit.cast(form))
        return false;
    ai.note_action_retry(BotAI::ActionKind::DruidFormEntered, 0, now_ms);
    ai.set_last_rule_fired("idle:classic_druid_form:enter");
    return true;
}

// ---------- idle:classic_aura ----------

// Paced like the blessings: one cast per aura per 30 s. The aura check reads the bot's own aura list; when that never shows the aura
// (MC 2026-10-08: 70-90 aura casts a minute) the bot recast it every tick it was off the GCD.
constexpr uint64 kAuraPaceKey = 1ull << 40;
bool AuraGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!(s.is_alive() && !s.in_combat() && !s.is_casting() && !s.on_transport()))
        return false;
    // Not out of a drink: standing up to recast an aura restarts the sit (MC Garr 2026-10-10: buffs interrupted the healers' drinks).
    if (s.max_power(0) > 0 && Classic::IsDrinking(s) && s.power_pct(0) < kBuffDrinkDonePct)
        return false;
    uint32 const aura = Classic::PickPaladinAura(s, g);
    return aura != 0 && !ai.action_recently_tried(BotAI::ActionKind::ClassUpkeep, kAuraPaceKey | aura, now_ms);
}
bool AuraFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32 now_ms)
{
    uint32 const aura = Classic::PickPaladinAura(s, g);
    if (!aura || ai.action_recently_tried(BotAI::ActionKind::ClassUpkeep, kAuraPaceKey | aura, now_ms) || !emit.cast(aura))
        return false;
    ai.note_action_retry(BotAI::ActionKind::ClassUpkeep, kAuraPaceKey | aura, now_ms);
    {
        std::string own;
        for (AuraEntry const& a : s.raw().auras.own_auras)
            own += fmt::format("{}{}(caster {}):{}s", own.empty() ? "" : " ", a.spell_id, a.caster == s.guid() ? "self" : "other", a.remaining.count() / 1000);
        static std::mutex logMu;
        static std::unordered_map<uint64, uint32> lastLog;
        bool due = false;
        {
            std::lock_guard<std::mutex> lk(logMu);
            uint32& l = lastLog[s.guid().GetCounter()];
            if (!l || now_ms - l >= 60000u) { l = now_ms ? now_ms : 1u; due = true; }
        }
        if (due)
            TC_LOG_INFO("playerbot.v2", "[raid_support] {} casts aura {}: own auras [{}]", s.name(), aura, own);
    }
    Classic::NoteRaidSupport(Classic::RaidSupportKind::Aura);
    ai.set_last_rule_fired("idle:classic_aura");
    return true;
}

// ---------- idle:classic_dispel ----------
// Between pulls (a curse or disease outlasts the fight); in combat the
// rotations run the same assignment. Not gated on the party being out of
// combat: a debuff on a member is worth removing while others still fight.
// Each (member, debuff type) job is paced like the other raid support picks:
// a dispel that can't land (out of line of sight) waits 30 s instead of
// re-firing every global cooldown. The mana floor and the mounted / druid
// form checks live in PickDispel (the plan applies the same ones).

bool DispelGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!s.is_alive() || s.in_combat() || s.is_casting() || s.is_mounted() || s.on_transport())
        return false;
    SkipCtx const ctx{ &ai, now_ms };
    return Classic::PickDispel(s, g, { &SkipRecent, &ctx }).spell != 0;
}
bool DispelFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32 now_ms)
{
    SkipCtx const ctx{ &ai, now_ms };
    Classic::CastPick const d = Classic::PickDispel(s, g, { &SkipRecent, &ctx });
    if (!d.spell || !emit.cast(d.spell, d.target))
        return false;
    if (d.key)
        ai.note_action_retry(BotAI::ActionKind::ClassUpkeep, d.key, now_ms);
    Classic::NoteRaidSupport(Classic::RaidSupportKind::Dispel);
    ai.set_last_rule_fired("idle:classic_dispel");
    return true;
}

// ---------- idle:classic_soulstone ----------

bool SoulstoneGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!UpkeepWindow(s, g))
        return false;
    Classic::SoulstonePick const p = Classic::PickSoulstone(s, g);
    if (p.create_spell)
        return !s.is_moving() && V2::MayConjure(s.in_combat(), s.recently_in_combat(5000), s.power(POWER_MANA), s.mana_cost(p.create_spell))
            && !ai.action_recently_tried(BotAI::ActionKind::ClassUpkeep, p.create_spell, now_ms);
    return p.item && !ai.action_recently_tried(BotAI::ActionKind::ClassUpkeep, p.item, now_ms);
}
bool SoulstoneFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32 now_ms)
{
    Classic::SoulstonePick const p = Classic::PickSoulstone(s, g);
    if (p.create_spell)
    {
        if (!emit.cast(p.create_spell))
            return false;
        ai.note_action_retry(BotAI::ActionKind::ClassUpkeep, p.create_spell, now_ms);
        ai.set_last_rule_fired("idle:classic_soulstone:create");
        return true;
    }
    if (!p.item || !emit.use_item_by_entry(p.item, p.target))
        return false;
    ai.note_action_retry(BotAI::ActionKind::ClassUpkeep, p.item, now_ms);
    Classic::NoteRaidSupport(Classic::RaidSupportKind::Soulstone);
    ai.set_last_rule_fired("idle:classic_soulstone");
    return true;
}

// ---------- idle:classic_healthstone ----------
// Inside an instance the dungeon dispatch (720) holds every world idle rule,
// idle:conjured_item (672) included, so a raid warlock never made a stone.
// Out of the instance idle:conjured_item keeps doing it.

bool HealthstoneGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!s.is_in_instance() || !UpkeepWindow(s, g) || s.is_moving() || s.recently_in_combat(5000))
        return false;
    uint32 const create = Classic::CreateHealthstoneSpell(s);
    return create && s.is_ready(create) && !Classic::HasHealthstone(s)
        && V2::MayConjure(s.in_combat(), s.recently_in_combat(5000), s.power(POWER_MANA), s.mana_cost(create))
        && !ai.action_recently_tried(BotAI::ActionKind::ClassUpkeep, create, now_ms);
}
bool HealthstoneFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    uint32 const create = Classic::CreateHealthstoneSpell(s);
    if (!create || !emit.cast(create))
        return false;
    ai.note_action_retry(BotAI::ActionKind::ClassUpkeep, create, now_ms);
    ai.set_last_rule_fired("idle:classic_healthstone");
    return true;
}

// ---------- idle:classic_mana_gem ----------
// A mage keeps a conjured mana gem in the bags inside dungeons / raids (the gem is the first mana item of the fight, and nothing
// conjured one: the mage branch of idle:conjured_item makes retail Refreshment).

// A conjure that leaves no gem the bag scan recognises must not repeat for ever: after kGemAttemptCap casts without a gem the
// bot pauses kGemPauseMs, then gets a fresh set of attempts.
constexpr uint32 kGemAttemptCap = 3;
constexpr uint32 kGemPauseMs = 10 * 60 * 1000;

struct GemTries { uint32 count = 0; uint32 pause_until = 0; };

std::mutex& GemTriesMutex() { static std::mutex m; return m; }
std::unordered_map<uint64, GemTries>& GemTriesTable() { static std::unordered_map<uint64, GemTries> t; return t; }

bool GemMayTry(uint64 bot, uint32 now_ms)
{
    std::lock_guard lk(GemTriesMutex());
    auto it = GemTriesTable().find(bot);
    if (it == GemTriesTable().end() || !it->second.pause_until)
        return true;
    if (static_cast<int32>(now_ms - it->second.pause_until) < 0)
        return false;
    GemTriesTable().erase(it);   // pause over: fresh attempts
    return true;
}

void GemForget(uint64 bot)
{
    std::lock_guard lk(GemTriesMutex());
    GemTriesTable().erase(bot);
}

// Counts a cast; returns true when it was the last attempt before the pause.
bool GemNoteCast(uint64 bot, uint32 now_ms)
{
    std::lock_guard lk(GemTriesMutex());
    GemTries& t = GemTriesTable()[bot];
    if (++t.count < kGemAttemptCap)
        return false;
    t.pause_until = now_ms + kGemPauseMs;
    return true;
}

bool ManaGemGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!s.is_in_instance() || !UpkeepWindow(s, g) || s.is_moving() || s.is_casting() || s.recently_in_combat(5000))
        return false;
    uint32 const create = Classic::CreateManaGemSpell(s);
    if (!create)
        return false;
    if (Classic::HasManaGem(s))
    {
        GemForget(s.guid().GetCounter());   // the conjure worked: forget earlier misses
        return false;
    }
    return s.is_ready(create)
        && V2::MayConjure(s.in_combat(), s.recently_in_combat(5000), s.power(POWER_MANA), s.mana_cost(create))
        && !ai.action_recently_tried(BotAI::ActionKind::ClassUpkeep, create, now_ms)
        && GemMayTry(s.guid().GetCounter(), now_ms);
}
bool ManaGemFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    uint32 const create = Classic::CreateManaGemSpell(s);
    if (!create || !emit.cast(create))
        return false;
    ai.note_action_retry(BotAI::ActionKind::ClassUpkeep, create, now_ms);
    if (GemNoteCast(s.guid().GetCounter(), now_ms))
        TC_LOG_INFO("playerbot.v2", "[supplies] {} cast mana gem spell {} {} times without a gem reaching the bags; pausing {} min",
            s.name(), create, kGemAttemptCap, kGemPauseMs / 60000);
    ai.set_last_rule_fired("idle:classic_mana_gem");
    return true;
}

// ---------- idle:classic_conjure ----------
// Mages: Conjure Water and Conjure Food when the conjured stock runs low, in instances (out in the world
// idle:conjured_item does it). The conjured items are drunk / eaten like any other (consumables are
// classified by their on-use spell).

constexpr uint32 kConjureWaterBelow = 20;
constexpr uint32 kConjureFoodBelow = 10;

uint32 PickConjure(BotSnapshotView const& s, BotAI& ai, uint32 now_ms)
{
    for (auto kind : { Classic::ConsumableKind::Drink, Classic::ConsumableKind::Food })
    {
        uint32 const create = Classic::MageConjureSpell(s, kind, kind == Classic::ConsumableKind::Drink ? kConjureWaterBelow : kConjureFoodBelow);
        if (create && s.is_ready(create)
            && V2::MayConjure(s.in_combat(), s.recently_in_combat(5000), s.power(POWER_MANA), s.mana_cost(create))
            && !ai.action_recently_tried(BotAI::ActionKind::ClassUpkeep, create, now_ms))
            return create;
    }
    return 0;
}
bool ConjureGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (s.cls() != CLASS_MAGE || !s.is_in_instance() || !UpkeepWindow(s, g) || s.is_moving() || s.recently_in_combat(5000))
        return false;
    return PickConjure(s, ai, now_ms) != 0;
}
bool ConjureFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, BotIntentEmitter& emit, uint32 now_ms)
{
    uint32 const create = PickConjure(s, ai, now_ms);
    if (!create || !emit.cast(create))
        return false;
    ai.note_action_retry(BotAI::ActionKind::ClassUpkeep, create, now_ms);
    ai.set_last_rule_fired("idle:classic_conjure");
    return true;
}

// ---------- idle:classic_fear_ward ----------
// The raid plan (PveGroupCoordinator) gives each dwarf priest one tank that
// lacks Fear Ward; between pulls the priest puts it back up (in combat the
// priest rotations do).

bool FearWardGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const& g, uint32)
{
    return s.is_alive() && !s.in_combat() && !s.is_casting() && !s.on_transport()
        && Classic::PickFearWard(s, g).spell != 0;
}
bool FearWardFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32)
{
    Classic::CastPick const p = Classic::PickFearWard(s, g);
    if (!p.spell || !emit.cast(p.spell, p.target))
        return false;
    ai.set_last_rule_fired("idle:classic_fear_ward");
    return true;
}

// ---------- idle:classic_raid_consumables ----------
// Inside a raid, between pulls: the role's flask / elixirs / stat food from the bags, whose buff is missing
// (Classic::PickRaidConsumable). With Playerbot.Raid.ProvideConsumables the bags are stocked first, one kit item per
// attempt (PlayerbotAPI::stock_raid_consumable). Keys carry a high bit so an item id never meets a spell id.

constexpr uint64 kConsumableKeyBit = 1ull << 33;

bool RaidConsumablesGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, uint32 now_ms)
{
    if (!UpkeepWindow(s, g) || s.is_moving() || s.recently_in_combat(3000) || Classic::IsEating(s) || Classic::IsDrinking(s))
        return false;
    if (uint32 const use = Classic::PickRaidConsumable(s, g);
        use && !ai.action_recently_tried(BotAI::ActionKind::ClassUpkeep, use | kConsumableKeyBit, now_ms))
        return true;
    if (!Services::Config().raid_provide_consumables())
        return false;
    uint32 const stock = Classic::PickRaidConsumableStock(s, g);
    return stock && !ai.action_recently_tried(BotAI::ActionKind::ClassSupply, stock | kConsumableKeyBit, now_ms);
}
bool RaidConsumablesFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const& g, BotIntentEmitter& emit, uint32 now_ms)
{
    if (uint32 const use = Classic::PickRaidConsumable(s, g);
        use && !ai.action_recently_tried(BotAI::ActionKind::ClassUpkeep, use | kConsumableKeyBit, now_ms))
    {
        if (!emit.use_item_by_entry(use))
            return false;
        ai.note_action_retry(BotAI::ActionKind::ClassUpkeep, use | kConsumableKeyBit, now_ms);
        Classic::NoteConsumableUse(s, use, "raid prep");
        ai.set_last_rule_fired("idle:classic_raid_consumables:use");
        return true;
    }
    uint32 const stock = Classic::PickRaidConsumableStock(s, g);
    if (!stock || !emit.stock_raid_consumable(stock))
        return false;
    ai.note_action_retry(BotAI::ActionKind::ClassSupply, stock | kConsumableKeyBit, now_ms);
    ai.set_last_rule_fired("idle:classic_raid_consumables:stock");
    return true;
}

} // namespace

void RegisterClassicUpkeepRules(IdleRuleRegistry& r)
{
    auto add = [&r](char const* name, int prio,
                    bool (*gate)(BotSnapshotView const&, BotAI&, GroupSnapshotView const&, uint32),
                    bool (*fire)(BotSnapshotView const&, BotAI&, GroupSnapshotView const&, BotIntentEmitter&, uint32))
    {
        IdleRule rule;
        rule.name     = name;
        rule.priority = prio;
        rule.gate     = gate;
        rule.fire     = fire;
        r.register_rule(std::move(rule));
    };
    add("idle:classic_supplies",  992, &SuppliesGate,  &SuppliesFire);
    add("idle:classic_weapon",    992, &WeaponGate,    &WeaponFire);
    add("idle:classic_blessings", 992, &BlessingsGate, &BlessingsFire);
    add("idle:classic_raid_buffs", 992, &RaidBuffsGate, &RaidBuffsFire);
    add("idle:classic_druid_form", 992, &DruidFormGate, &DruidFormFire);
    add("idle:classic_aura",      992, &AuraGate,      &AuraFire);
    add("idle:classic_dispel",    992, &DispelGate,    &DispelFire);
    add("idle:classic_soulstone", 992, &SoulstoneGate, &SoulstoneFire);
    add("idle:classic_healthstone", 992, &HealthstoneGate, &HealthstoneFire);
    add("idle:classic_mana_gem",  992, &ManaGemGate,   &ManaGemFire);
    add("idle:classic_conjure", 992, &ConjureGate, &ConjureFire);
    add("idle:classic_fear_ward", 992, &FearWardGate, &FearWardFire);
    add("idle:classic_raid_consumables", 991, &RaidConsumablesGate, &RaidConsumablesFire);
}

} // namespace Playerbot
