// ClassicUpkeep - vanilla class upkeep decisions for Classic 1.60 (Forever) bots.
//
// Shared by the out-of-combat idle rules (Bot/States/Rules/ClassicUpkeepRules.cpp)
// and the vanilla rotations (Combat/Apl/Apl_Classic_*.cpp), so both pick the
// same blessing, imbue, poison and totem:
//   - paladin blessings, split between the group's paladins (ike3 / PallyPower
//     style: each paladin covers one blessing per class, Greater Blessings
//     once Symbol of Kings is in the bags);
//   - shaman weapon imbues and totems (one per element, by spec and group);
//   - rogue poisons (Instant on the main hand, Deadly on the off hand);
//   - whether the bot is short of a class tool or reagent (ClassicSupplies).
// Pure functions of the snapshot: safe on the bot worker threads.

#pragma once

#include <functional>
#include <string_view>
#include "InstanceSchools.h"
#include "BotTypes.h"
#include "ObjectGuid.h"
#include <vector>

namespace Playerbot {

class BotSnapshotView;
class GroupSnapshotView;
struct NearbyUnit;

namespace Classic {

struct CastPick
{
    uint32     spell = 0;   // 0 = nothing to do
    ObjectGuid target;      // unit for blessings, weapon item for imbues
    // Raid support picks: the unit of work (a Greater Blessing's class, a
    // Prayer's subgroup, a single target) so the rule can pace a cast the
    // snapshot hasn't caught up with yet. 0 = no pacing key.
    uint64     key = 0;
    // `spell` is an exact lower rank (BotIntentEmitter::cast_rank), not a
    // rank 1 id cast at the highest known rank (cast): a party buff whose top
    // rank's reagent is missing but the lower rank's is in the bags.
    bool       exact_rank = false;
    // A caster-centred buff (key kind 6): how many members it was needy of when picked, so the rule can tell a cast that reached nobody.
    uint32     area_needy = 0;
};

// Raid support picks skip work the caller is still pacing: `skip(ctx, key)`
// true = a cast for that key went out moments ago (BotAI action retry).
struct PickSkip
{
    bool (*fn)(void const* ctx, uint64 key) = nullptr;
    void const* ctx = nullptr;
    bool operator()(uint64 key) const { return fn && key && fn(ctx, key); }
};

struct ItemPick
{
    uint32     item = 0;    // 0 = nothing to do
    ObjectGuid weapon;
};

// Paladin: the next blessing this paladin should cast. Out of combat a
// blessing about to expire counts as missing; in combat only a missing one
// does. PallyPower split: each bot paladin of the group (by guid) owns the
// blessing at its index of every CLASS's order, cast as a Greater Blessing
// (whole class) once Symbol of Kings is in the bags. A class with a tank
// in it never gets a Greater Salvation: that paladin blesses its non-tanks
// one by one and gives the tank another blessing.
CastPick PickBlessing(BotSnapshotView const& s, GroupSnapshotView const& g, bool in_combat,
                      PickSkip skip = {});

// ======================= Raid support =======================
// Generic group / raid support every raid needs (port of Karrz's ike3 buff,
// cure and resistance strategies). Assignment is computed identically by
// every bot from the shared group snapshot, so exactly one bot takes each
// piece of work.

// Resistance an instance calls for (paladin aura, shaman totem):
// Molten Core / Blackwing Lair / Onyxia = fire, Naxxramas = frost,
// Ahn'Qiraj = nature.
Resist InstanceResistance(uint32 map_id);

// Do the bosses of this map deal `school`? (InstanceSchools.h)
bool InstanceDealsSchool(uint32 map_id, Resist school);


// Bosses that fear the raid (per instance map): Molten Core Magmadar (Panic),
// Blackwing Lair Nefarian and Onyxia (Bellowing Roar). A shaman drops Tremor
// Totem proactively once one is engaged (PickTotem), and the coordinator logs
// the fear prep ([raid_fear]).
bool BossFears(uint32 map_id, uint32 creature_entry);

// The resistance potion for an instance's resistance (Fire: Greater Fire
// Protection Potion 13457), 0 when there is none.
uint32 ResistPotionItem(Resist r);

// In combat: the resistance potion the raid plan asks this bot to drink
// (PveOrder::resist_potion, opt-in Playerbot.Raid.ResistPotions) when it is
// in the bags, off cooldown and its protection is not already up. 0 = none.
uint32 PickResistPotion(BotSnapshotView const& s);

// Dwarf priest: Fear Ward (6346) on the tank the raid plan gave it
// (PveOrder::fear_ward_target) while that tank is in range. spell 0 = nothing.
CastPick PickFearWard(BotSnapshotView const& s, GroupSnapshotView const& g);

// Priest / mage / druid class buff (Fortitude, Divine Spirit, Shadow
// Protection in raids, Arcane Intellect, Mark of the Wild, Thorns on tanks):
// the target and spell this bot should cast. Each subgroup belongs to one
// caster of the class (subgroup % casters within 30 yd of a needy member);
// the group version (Prayer / Brilliance / Gift) when 2+ of the subgroup need
// it and the reagent of the rank cast is in the bags (while the group cast is
// paced the subgroup waits: no single buffs on members it is about to cover).
// The druid specs that stay shapeshifted (Feral / Guardian / Balance) are not
// casters while a caster druid is; the form a druid is in right now does not
// change the plan. Out of combat only; nothing while this bot is in a form
// (DruidFormToLeave takes it out first).
CastPick PickClassBuff(BotSnapshotView const& s, GroupSnapshotView const& g, PickSkip skip = {});
// A cast PickClassBuff returned with a kind 6 key (caster-centred buff): tracks whether it reached anyone, and backs the buff off when not.
void NoteAreaBuffCast(ObjectGuid caster, uint64 key, uint32 needy, uint32 now_ms);

// Druid, out of combat: the form aura (Cat, Bear, Dire Bear, Travel, Moonkin;
// never Aquatic) to cancel because PickClassBuff has a buff for this druid once
// it is out of the form. 0 = stay.
uint32 DruidFormToLeave(BotSnapshotView const& s, GroupSnapshotView const& g, PickSkip skip = {});

// Feral druid (Cat 103 / Bear 104) grouped inside an instance, out of combat
// and in no form: the form to shift back into before the pull once it owes no
// class buff and is not eating, drinking or about to. 0 = nothing.
uint32 DruidFormToEnter(BotSnapshotView const& s, GroupSnapshotView const& g, PickSkip skip = {});

// Paladin: the aura this paladin should hold (0 = keep the current one).
// Paladins of a subgroup take distinct auras: Devotion where the subgroup
// has a tank, the instance's resistance aura, then Concentration (holy) /
// Retribution / Devotion.
uint32 PickPaladinAura(BotSnapshotView const& s, GroupSnapshotView const& g);

// Dispel / cure work assigned to this bot: the spell and the group member
// to cast it on. Every debuff the bot's class can remove (Remove Lesser
// Curse, Remove Curse, Abolish / Cure Poison, Dispel Magic, Abolish / Cure
// Disease, Cleanse / Purify, shaman Cure Poison / Disease) is handed to
// exactly one capable bot within 30 yd of the debuffed member, spread by
// load, so ten dispellers never chase the same debuff. Healers and DPS
// alike. Debuffs an encounter layer owns are skipped (IsEncounterOwnedDispel).
// Only bots whose dispel gates fire are planned: not tanks, Shadowform
// priests, Feral / Guardian druids, druids in a form, mounted bots or bots
// below kDispelMinManaPct mana; this bot returns nothing under the same
// conditions. `skip` paces a job by its key (member, debuff type): a skipped
// job stays this bot's and it moves on to its next one.
CastPick PickDispel(BotSnapshotView const& s, GroupSnapshotView const& g, PickSkip skip = {});

// Mana below which a bot takes no dispel job (and is left out of the plan).
constexpr int32 kDispelMinManaPct = 15;

// Encounter-owned dispels: auras a raid encounter layer removes itself
// (RaidOrder cast jobs that run before the rotation). The generic dispel
// plan (PickDispel) leaves them alone so no two bots dispel the same debuff.
// The list starts EMPTY: until a raid layer registers an aura, the generic
// plan removes it like any other debuff.
//
// How a raid encounter layer registers its auras:
//   - call RegisterEncounterOwnedDispel(map_id, aura_id) once per debuff it
//     dispels itself, from its own init (e.g. where it loads its encounter
//     table), with the instance map id (Molten Core = 409) and the EXACT aura
//     spell id as it appears on the debuffed unit (the group snapshot records
//     the applied aura's id, not a rank 1 / chain id): e.g. 409/19703
//     Lucifron's Curse, 409/19716 Gehennas' Curse, 409/19713 Shazzrah's
//     Curse, 409/19659 Ignite Mana, 409/19776 + 409/20294 Flamewaker Priest's
//     Shadow Word: Pain / Immolate, 409/19408 Magmadar's Panic;
//   - register only auras the layer really removes, or nobody will;
//   - registration is idempotent and thread-safe at any time (an immutable
//     list is republished atomically; bot workers read it lock-free), but
//     registering at startup avoids a window in which both the layer and
//     the generic plan dispel the same debuff. There is no unregister.
bool IsEncounterOwnedDispel(uint32 map_id, uint32 aura_id);
void RegisterEncounterOwnedDispel(uint32 map_id, uint32 aura_id);

// Warlock: Soulstone a healer between pulls (warlock k by guid keeps ward k:
// priest / paladin / shaman healers first, then druid healers, then tanks;
// a ward still carrying a soulstone is left alone). Returns the spell to cast
// (Create Soulstone when none is in the bags) or the soulstone item to use
// on `target` (item != 0).
struct SoulstonePick
{
    uint32     create_spell = 0;
    uint32     item = 0;
    ObjectGuid target;
};
SoulstonePick PickSoulstone(BotSnapshotView const& s, GroupSnapshotView const& g);

// Warlock healthstones. The Create Healthstone spells are found in the spell
// data (warlock-family spells whose Create Item effect makes an instant-heal
// consumable: every rank, Minor to Major, and the Improved Healthstone talent
// variants), not hardcoded; their ItemType ids are the healthstones.
// IsHealthstoneItem: one of those items (or any conjured instant-heal item).
// HasHealthstone: one is in the bags. CreateHealthstoneSpell: the best Create
// Healthstone spell the bot knows (0 = none).
bool   IsHealthstoneItem(uint32 item_entry);
bool   HasHealthstone(BotSnapshotView const& s);
uint32 CreateHealthstoneSpell(BotSnapshotView const& s);

// Mage mana gems: HasManaGem is a conjured mana-restoring item in the bags; CreateManaGemSpell the best Conjure Mana Agate / Citrine /
// Jade / Ruby the mage knows (0 = none, or not a mage). The gem is the first mana item PickCombatManaItem takes.
bool   HasManaGem(BotSnapshotView const& s);
uint32 CreateManaGemSpell(BotSnapshotView const& s);

// A group member carries the tracked buff `tracked_id` (any caster).
bool MemberHasBuff(GroupSnapshotView const& g, ObjectGuid guid, uint32 tracked_id);

// Pull readiness: the raid buff the most living members within `range` of
// (x, y, z) lack entirely, among those a present caster can provide - the
// class buffs above (a bot caster of the class, same map, alive, of the
// spell's level, assigned as PickClassBuff would) and a paladin blessing (a
// member with no blessing at all while a bot paladin is present), and a
// paladin aura (a member of a bot paladin's subgroup within 30y of it with no
// paladin aura). Battle Shout and totems are left out: no upkeep rule puts
// them up before a pull (see the .cpp). `name` is
// a short display name; members == 0 when nothing is missing.
struct MissingBuff
{
    uint32      spell = 0;
    char const* name = "";
    uint32      members = 0;
};
MissingBuff MissingGroupBuff(BotSnapshotView const& s, GroupSnapshotView const& g,
                             float x, float y, float z, float range);

// Rate-limited INFO summary of raid support work (one line per minute).
// Tranq: a Tranquilizing Shot cast at an enraged mob. Decurse: a curse removed by a mage / druid cast. FormationGiveUp: a bot gave up a
// raid formation slot walk that made no progress ([formation_stuck]). The last three are not in the minute line; the scenario runner reads
// their per-map totals (RaidSupportTotal) to judge a run.
enum class RaidSupportKind : uint8 { Buff, Blessing, Aura, Dispel, Soulstone, Tranq, Decurse, FormationGiveUp, Count };
void NoteRaidSupport(RaidSupportKind kind, uint32 map_id = 0);
// Events of `kind` noted on `map_id` since the process started (a monotonic count: a scenario compares two readings).
uint32 RaidSupportTotal(RaidSupportKind kind, uint32 map_id);
// True when `spell` is a friendly dispel that removes curses (Remove Lesser Curse, Remove Curse), by its dispel effect.
bool IsCurseDispelSpell(uint32 spell);

// Shaman: the weapon imbue to (re)apply to the main hand.
CastPick PickImbue(BotSnapshotView const& s, bool in_combat);

// Rogue: the poison to apply next and the weapon it goes on.
ItemPick PickPoison(BotSnapshotView const& s);

// Shaman: the next totem to drop (in combat), 0 when every element is covered.
uint32 PickTotem(BotSnapshotView const& s, GroupSnapshotView const& g);

// Healer downranking: the rank of the direct heal `spell` (any rank of its
// chain) to cast on `target`, the smallest known rank whose base heal covers
// the target's missing health. 0 means "cast the highest rank" (emergency
// at 40% health or below, or no lower rank covers the deficit). Base heal
// only: +healing gear makes the pick err towards a slightly bigger rank.
uint32 HealRank(BotSnapshotView const& s, GroupSnapshotView const& g, uint32 spell, ObjectGuid target);

// Every supply item (ClassicSupplies: class tools and reagents, food, water,
// potions, bandages) the bot is short of: the best rank for its level of each
// family it holds less than half its target of.
void ShortClassSupplies(BotSnapshotView const& s, std::vector<uint32>& out);

// Raid consumables (flask, elixirs, stat food; ClassicSupplies kits by role). Inside a raid instance only.
// PickRaidConsumable: the kit item in the bags whose buff is missing, to use now (0 = none).
// PickRaidConsumableStock: the kit item the bags are short of whose buff is missing, to stock (0 = none).
// Playerbot.Consumables.PvE / .PvP for the bot's current map (battlegrounds and arenas are PvP, everything else PvE).
bool ConsumablesAllowed(BotSnapshotView const& s);
uint32 PickRaidConsumable(BotSnapshotView const& s, GroupSnapshotView const& g);
uint32 PickRaidConsumableStock(BotSnapshotView const& s, GroupSnapshotView const& g);

// Physical melee damage dealer or tank by class and (virtual Classic) spec.
bool IsMeleeSpec(uint8 cls, uint32 spec);

// ======================= Consumables =======================
// Bag consumables are told apart by their on-use spell (item template + spell
// data, read-only stores loaded at startup), so looted, crafted, conjured and
// restocked items all count, whatever their id.
enum class ConsumableKind : uint8
{
    Healthstone,     // instant heal item that is not a potion (Healthstone, Whipper Root Tuber)
    HealingPotion,
    ManaPotion,
    ManaItem,        // mana restore that is not a potion: Dark / Demonic Rune, mana gems
    Food,
    Drink,
    Bandage,
};

// Best item of `kind` in the bags the bot can use now (required level, First
// Aid rank, off cooldown): highest required level, then item level, conjured
// first on a tie. With `conjured_only` only conjured items count. 0 = none.
uint32 BestConsumable(BotSnapshotView const& s, ConsumableKind kind, bool conjured_only = false);

// Mage Conjure Water / Conjure Food: the best rank the mage knows (found in the spell data, like
// the healthstones) when fewer than `want_below` conjured items of that kind (Drink / Food) are
// in the bags, else 0.
uint32 MageConjureSpell(BotSnapshotView const& s, ConsumableKind kind, uint32 want_below);
// Best Conjure Water (Drink) / Conjure Food (Food) rank `knows` accepts, 0 = none.
uint32 BestMageConjure(ConsumableKind kind, std::function<bool(uint32)> const& knows);
// A mage that can conjure the item of supply family `family` ("Water", "Food") makes its own: the restock skips it.
bool   MageConjuresFamily(uint8 cls, std::string_view family, std::function<bool(uint32)> const& knows);

// The bot used a combat consumable (potion, healthstone, rune, mana gem) in
// the last `window_ms` (the owner: at most one per 2.5 s, so a healthstone and a
// potion never go in the same instant).
bool CombatConsumableUsedRecently(BotSnapshotView const& s, uint32 window_ms = 2500);

// A group member is casting a direct heal on the bot.
bool HealIncoming(BotSnapshotView const& s, GroupSnapshotView const& g);

// A food / drink regeneration aura is up (any rank, any item).
bool IsEating(BotSnapshotView const& s);
bool IsDrinking(BotSnapshotView const& s);

// The quest / travel walkers (idle priority 697-700) stand down while the bot should recover first: under half health, a
// food / drink aura is up (standing up mid-meal wastes the item), or a caster with water in the bags is under a fifth mana.
bool RecoverBeforeWalking(BotSnapshotView const& s);

// the owner's SafeToRecover / UseFoodStrategy: free to sit down - nothing attacks
// the bot and no group member fights within 45 yards and no enemy engaged with the bot or its group stands within 30 yards (Bot/RecoverRules.h).
// Without a group view only the enemies are checked (a fight next to the bot
// still shows as an enemy with a victim).
bool SafeToRecover(BotSnapshotView const& s, GroupSnapshotView const* g);

// Out-of-combat eat / drink decision. Food when health < start_hp, a drink
// when a mana user's mana < start_mana (both may run together). While a food
// or drink aura runs the bot keeps sitting (hold) until topped up to 95%, and
// never uses the same kind again (each use eats another item).
struct RecoveryPick
{
    uint32 item = 0;     // food / drink to use now
    bool   hold = false; // already eating / drinking: stay seated
};
RecoveryPick PickRecovery(BotSnapshotView const& s, int32 start_hp, int32 start_mana);

// Healer mana a grouped instance tank waits for before it pulls (State_Idle
// dungeon group-ready gate). The drink rules start a grouped healer's drink
// below this same value, so a healer that holds the pull always drinks.
// 85 was Karrz's ike3 "raid prepull recover" target; 75 since 2026-10-07 (the owner: MC trash takes too long).
constexpr int32 kPullReadyHealerManaPct = 75;
// A five-man healer has no trash-pace cost to weigh (the owner's 2026-10-07 call was about MC): a drink that starts only below 75 %
// never starts for a priest at 75-94 %, so she pulled into every pack three-quarters full and was dry by the boss
// (deadmines_full@sneed). Dungeon healers start the drink, and the tank's pull gate waits, below this.
constexpr int32 kPullReadyDungeonHealerManaPct = 90;
// The tank's wait for a five-man healer to reach that mark is bounded per hold (a healer with no drink left sat on spirit regen and
// held deadmines_full's last gate until group_stalled); a raid's wait is bounded by kBossPullWaitCapMs.
constexpr uint32 kPullReadyDungeonHealerHoldCapMs = 90000;
// The mark that applies to this bot's group: kPullReadyHealerManaPct in a raid, kPullReadyDungeonHealerManaPct in a party;
// kBossPullHealerManaPct while the group's tank is about to pull a boss (NoteBossPullWait).
int32 PullReadyHealerManaPct(BotSnapshotView const& s);
// A boss is a long fight and the healers' mana is the clock on it (MC Garr 2026-10-10: healers began the pull at 1-6 % and 69 % average
// at 30 s, dry at 360 s): the tank's boss pull waits for, and its healers drink to, this mark. Bounded (kBossPullWaitCapMs from the
// first wait) so a healer that cannot drink never freezes the raid.
constexpr int32  kBossPullHealerManaPct = 95;
constexpr uint32 kBossPullWaitCapMs     = 150000;
// The group's tank is at the boss and checking its healers' mana: the group's healers read PullReadyHealerManaPct as the boss mark.
void NoteBossPullWait(ObjectGuid group);
// DPS casters and hunters need less between trash packs: a full drink above this costs the raid
// more time than the extra mana is worth (MC trash 2026-10-07: pulls minutes apart).
constexpr int32 kPullReadyDpsManaPct = 60;

// A raid healer that stands under an owner Hold / Stay waits for the Attack: that is the raid leader's wait for a boss pull, so it
// drinks to full instead of the trash threshold above (MC Garr 2026-10-10: healers stuck at 76-89 % never started a drink and went
// into the pull at about 80 %).
constexpr int32 kHeldRaidHealerManaPct = 95;

// Mana % a non-healer mana user (DPS caster, hunter, hybrid) drinks to before
// the next pull of a grouped instance (Playerbot.Behavior.PullRecover).
// Same value: the tank's gate waits for such a member for at most
// kPullReadyDpsHoldCapMs per hold (a bot with no drink must not freeze the raid).
constexpr uint32 kPullReadyDpsHoldCapMs = 20000;
// A member under the pull-ready health mark (70 %) holds the tank's pull for at most this long per hold: the healers top a hurt member off
// in seconds, so a longer wait means they cannot (out of reach, a tick that keeps hurting it) and the raid goes on.
constexpr uint32 kPullReadyHpHoldCapMs = 30000;

// This bot drinks for the next pull of its grouped instance: a healer, or
// (PullRecover on) any other mana user.
bool DrinksForPull(BotSnapshotView const& s, GroupSnapshotView const* g);

// This bot is a healer of its group (the group snapshot's role - the same
// role the tank's group-ready gate reads), or, with no group entry, its spec.
bool IsGroupHealer(BotSnapshotView const& s, GroupSnapshotView const* g);

// Mana % below which an out-of-combat drink starts: kPullReadyHealerManaPct
// for a healer grouped inside an instance (dungeon / raid), else open_world_pct.
int32 OocDrinkStartManaPct(BotSnapshotView const& s, GroupSnapshotView const* g,
                           int32 open_world_pct);

// In-combat emergency heal item (the owner's UsePotionsStrategy): at 30% health
// or below, healthstone first, then a healing potion; kept while a heal is
// about to land unless the bot is at 20% or below. 0 = nothing to use.
uint32 PickCombatHealthItem(BotSnapshotView const& s, GroupSnapshotView const& g);

// In-combat mana item: at 20% mana or below (a group healer in a boss fight,
// BossFight: 35%), a conjured mana gem, then a mana potion, then a Dark /
// Demonic Rune (which costs health: only above 50%). The potions' shared
// cooldown and the runes' own cooldown are read separately per item (an item
// on cooldown is skipped), so a healer drinks a potion and later a rune.
// <while_casting> asks whether an item would be due if no cast were running (the mana break: the caster stops starting casts).
uint32 PickCombatManaItem(BotSnapshotView const& s, GroupSnapshotView const* g = nullptr, bool while_casting = false);

// Health % at or below which PickCombatHealthItem uses an item.
constexpr int32 kCombatHealthItemPct = 30;
// Mana % at or below which a group healer in a boss fight uses a mana item.
constexpr int32 kBossHealerManaItemPct = 35;
// ... and the mana % above which it does not even look: from here down an item is used as soon as its restore fits in the missing mana.
constexpr int32 kHealerManaItemCeilingPct = 85;

// ======================= Boss fights (major cooldowns) =======================
// A boss by its creature data: the dungeon-boss flag or the world-boss rank
// (NearbyUnit::is_boss_rank).
bool IsBossUnit(NearbyUnit const& u);

// Major cooldowns (offensive talents and racials, Inner Focus, Innervate,
// the healers' earlier mana items) wait for a boss fight. Inside a dungeon or
// raid: an encounter of the instance is in progress, or a boss (IsBossUnit)
// is in combat with the bot, a group member or a pet - never trash, however
// big. In the open world the previous test stands: the bot's victim is a boss
// or has 100k+ health (elites, world bosses).
bool BossFight(BotSnapshotView const& s);

// [raid_cd] log line for a major cooldown the bot uses inside an instance
// (one per bot and spell per 10 s, so a retried cast logs once).
void NoteCooldownUse(BotSnapshotView const& s, uint32 spell, char const* what);

// [raid_consume] log line for a combat consumable the bot uses inside an
// instance (same pacing as NoteCooldownUse).
void NoteConsumableUse(BotSnapshotView const& s, uint32 item, char const* what);

// [raid_consume] line for a mana user at 20% or less in an instance fight with no mana item picked: the reason (casting, a recent
// consumable, none in the bags), once per bot per 10 s.
void NoteManaItemRefused(BotSnapshotView const& s);

} // namespace Classic
} // namespace Playerbot
