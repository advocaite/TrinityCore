// State_InCombat - Combat dispatch. Looks up the (class, spec) APL from the
// rotation registry and runs it. If no rotation is registered, the bot just
// engages auto-attack on its current target — never freezes.

#include "StateBase.h"
#include "Bot/BotAI.h"
#include "Bot/Formation.h"                      // StableJitter / MeleeArcSpot (SpreadOut)
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/Raid/RaidCombat.h"                 // raid encounter orders (MC/BWL/Onyxia/Naxx)
#include "Bot/Raid/RaidEncounters.h"             // Raid::Retaunts
#include "Bot/RunToTankRules.h"                 // EnemyBlocksRunToTank
#include "Bot/ClassicUpkeep.h"                   // Classic::PickCombatHealthItem / PickCombatManaItem
#include "Bot/ClassTables.h"                     // ClassOocHeal — combat:pve_heal_focus
#include "Bot/MarkGate.h"                       // Playerbot.Behavior.MarksNeedOrder
#include "Bot/RacialRules.h"                    // EscapeArtistWanted
#include "Bot/ThreatDiscipline.h"                // opening lead + [raid_threat] / [raid_pull] log pacing
#include "Bot/ThreatLimits.h"                    // hard threat ratios shared with the raid brain
#include "ClassicSpec.h"                         // Classic::IsSingleSpecClass — combat:pve_heal_focus
#include "Bot/Dungeon/MarkCc.h"               // MarkCc::TryCast - combat:marked_cc
#include "Bot/Dungeon/DungeonScript.h"          // DungeonAdvice — DungeonCombatPositioning (BUG G-P0a)
#include "Bot/Battleground/BgTargetPick.h"
#include "Bot/Battleground/BgTactics.h"       // BgTargetPick::Pick — combat:bg_victim_invalid
#include "Bot/Battleground/BattlegroundScript.h" // BattlegroundAdvice — BgCarrierHomeward (BUG BG-P0a)
#include "Bot/States/MaintainHelpers.h"          // DungeonCombatPositioning / BgCarrierHomeward
#include "Group/GroupSnapshot.h"
#include "Combat/ApRegistry.h"
#include "Combat/ApRotation.h"
#include "Combat/Apl/ApClassicHeal.h"            // ClassicHeal::RankHeal - raid overheal cancel
#include "Combat/Apl/ApTankThreat.h"             // TankThreat::TauntReach - [tank_loose] logs the reach the Taunt rule uses
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "../Services.h"                          // Services::Dungeons().GetAdvice in combat path
#include "Util/ConfigReader.h"                    // full type for Services::Config() (combat_skip_unfightable)
#include "Travel/RepairVendorIndex.h"             // Services::RepairVendors() for critical-gear flee
#include "Travel/QuestHubDatabase.h"              // Services::Hubs() hub fallback for flee_to_repair (start-island escape)
#include "RaceMask.h"
#include "UnitDefines.h"                      // UNIT_NPC_FLAG_* (solo flee town-NPC pick)
#include "SharedDefines.h"
#include "Log.h"                                   // TC_LOG_INFO — TEMP AV push diag
#include <limits>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace Playerbot::States {

namespace {

// Returns true if the (class, spec) pair fights primarily in melee range.
// Used to decide whether to emit a gap-close MoveTo when victim is out of
// reach. Hunter Survival is the only melee Hunter spec (retail). On the
// vanilla realm the Classic table applies: Holy paladins heal from range and
// every hunter spec (Survival included) fights at range.
bool IsMeleeSpec(uint8 cls, uint32 spec)
{
    if (IsVanillaRealm())
        return Classic::IsMeleeSpec(cls, spec);
    switch (cls)
    {
        case CLASS_PALADIN:
            return spec != 65;           // Holy heals from range
        case CLASS_WARRIOR:
        case CLASS_DEATH_KNIGHT:
        case CLASS_ROGUE:
        case CLASS_DEMON_HUNTER:
        case CLASS_MONK:
            return true;
        case CLASS_HUNTER:
            return spec == 255;          // Survival
        case CLASS_DRUID:
            return spec == 103 || spec == 104; // Feral / Guardian
        case CLASS_SHAMAN:
            return spec == 263;          // Enhancement
        default:
            return false;
    }
}

// True when an attacker/victim can actually be fought. Two ingredient sets:
//  - flags: untargetable (UNIT_FLAG_UNINTERACTIBLE), pacified
//    (UNIT_FLAG_PACIFIED) or already dead — the exact tally BotSnapshot-
//    Builder counts into fightable_attackers (read as tank_fightable by
//    GroupWedgeWatchdog, fightable_attackers_count() by State_Idle). These
//    are static/scripted authoring flags (triggers, dummies) and never
//    encode path-unreachability.
//  - cannot_reach: Creature::CanNotReachTarget() — the mob's OWN Chase-
//    MovementGenerator reports it cannot path to its target (maintained
//    per-tick by TC on !isInAccessiblePlaceFor / NOPATH). TRUE precisely
//    during the aggro-but-unreachable window: the WC corridor freeze pack
//    (2026-07-03) is ordinary targetable creatures on a z-disconnected
//    ledge — flags read fightable — yet chasing it can never close and only
//    steals movement ownership from dungeon navigation every combat frame.
// Both are cheap snapshot boolean reads — no new per-tick pathfind. Caveat:
// cannot_reach covers the mob-cannot-reach-bot direction (the diagnosed
// case); the asymmetric bot-cannot-reach-mob case is handled by the
// pull-gate / disengage machinery, not here.
bool IsAttackerFightable(NearbyUnit const& u)
{
    // A boss pacified by its own spell (Geddon's Inferno) is the fight, not a prop; the builder's tally agrees.
    return !u.untargetable && (!u.is_pacified || u.is_dungeon_boss || u.is_boss_rank) && u.hp > 0 && !u.cannot_reach;
}

// Names which fightable ingredient failed, for the [skip_unfightable] diag:
// "flags" = untargetable/pacified/dead, "cannot_reach" = the mob's own chase
// generator reports it cannot path to us.
char const* UnfightableReason(NearbyUnit const& u)
{
    if (u.untargetable || u.is_pacified || u.hp <= 0)
        return "flags";
    return "cannot_reach";
}

// Throttled [skip_unfightable] diag — ONE log site for both re-aim gates
// below (melee gap-close + victimless self-acquire) so the WC corridor
// freeze forensics can grep a single tag instead of chasing per-site
// duplicates (mirrors the [step_hold]/[adv_route] throttle pattern already
// used in this module).
void DiagSkipUnfightable(BotSnapshotView const& s, ObjectGuid victim,
                         char const* reason)
{
    static uint32 s_skip_unfightable_dbg_ms = 0;
    const uint32 now = s.published_at_ms();
    if (now - s_skip_unfightable_dbg_ms > 1500u)
    {
        s_skip_unfightable_dbg_ms = now;
        TC_LOG_INFO("playerbot.v2", "[skip_unfightable] bot={} victim={} reason={}",
                    s.bot_id(), victim.GetCounter(), reason);
    }
}

// Emergency-survival item (the owner's UsePotionsStrategy): at 30% health or
// below use a healthstone, else the best healing potion in the bags for the
// bot's level, unless a group heal is about to land (kept down to 20%). Not
// mid-cast (a healer finishes its heal first), at most one combat consumable
// per 2.5 s, and only items off cooldown, so a potion on its shared cooldown
// never blocks the rotation. Returns true if an item was emitted.
bool MaybeUseSurvivalItem(BotSnapshotView const& s, GroupSnapshotView const& g, BotIntentEmitter& emit)
{
    uint32 const item = Classic::PickCombatHealthItem(s, g);
    if (!item)
        return false;
    emit.emit(UseItemByEntryIntent{item, ObjectGuid::Empty});
    Classic::NoteConsumableUse(s, item, Classic::IsHealthstoneItem(item) ? "healthstone" : "healing potion");
    return true;
}

// Resistance potion the raid plan orders (opt-in Playerbot.Raid.ResistPotions:
// Greater Fire Protection Potion once a Molten Core / Blackwing Lair / Onyxia
// boss is engaged), when the bot carries one and the protection is not up.
bool MaybeUseResistPotion(BotSnapshotView const& s, BotIntentEmitter& emit)
{
    uint32 const item = Classic::PickResistPotion(s);
    if (!item)
        return false;
    emit.emit(UseItemByEntryIntent{item, ObjectGuid::Empty});
    // Paced (one line per bot and item per 10 s): until the potion's cooldown
    // reaches the snapshot this fires on every combat tick.
    Classic::NoteConsumableUse(s, item, "resistance potion");
    return true;
}

// Defensive racials, each on the debuff it clears: Stoneform (Dwarf) on bleed
// / poison / disease (curse too outside vanilla), Will of the Forsaken
// (Undead) on fear / charm / sleep, Escape Artist (Gnome) on root / snare.
// Any fight, not only bosses. Returns true if a racial was cast.
bool MaybeUseDefensiveRacial(BotSnapshotView const& s, BotIntentEmitter& emit)
{
    // Dwarf: Stoneform clears bleed/poison/disease/curse + 30% armor for 8s.
    if (s.race() == RACE_DWARF)
    {
        constexpr uint32 STONEFORM = 20594;
        if (s.knows_spell(STONEFORM) && s.is_ready(STONEFORM))
        {
            for (auto const& a : s.raw().auras.own_auras)
            {
                if (!a.is_harmful) continue;
                switch (a.dispel_type)
                {
                    case DispelType::Bleed:
                    case DispelType::Poison:
                    case DispelType::Disease:
                        emit.cast(STONEFORM);
                        Classic::NoteCooldownUse(s, STONEFORM, "Stoneform");
                        return true;
                    case DispelType::Curse:
                        // Vanilla Stoneform removes poison, disease and
                        // bleed only; the modern one also curses.
                        if (IsVanillaRealm()) break;
                        emit.cast(STONEFORM);
                        return true;
                    default: break;
                }
            }
        }
    }
    // Undead: Will of the Forsaken instantly breaks fear/charm/sleep. Used
    // reactively — most fears in PvE are mind-control phases on bosses, but
    // the racial pays for itself even on a single break per encounter.
    if (s.race() == RACE_UNDEAD_PLAYER)
    {
        constexpr uint32 WILL_OF_FORSAKEN = 7744;
        if (s.knows_spell(WILL_OF_FORSAKEN) && s.is_ready(WILL_OF_FORSAKEN))
        {
            // SpellInfo::Mechanic on the harmful aura tells us what it is:
            // the racial breaks charm, fear and sleep (enum Mechanics).
            for (auto const& a : s.raw().auras.own_auras)
            {
                if (!a.is_harmful) continue;
                if (a.mechanic == MECHANIC_FEAR ||
                    a.mechanic == MECHANIC_CHARM ||
                    a.mechanic == MECHANIC_SLEEP ||
                    (!IsVanillaRealm() && a.mechanic == MECHANIC_HORROR))
                {
                    emit.cast(WILL_OF_FORSAKEN);
                    Classic::NoteCooldownUse(s, WILL_OF_FORSAKEN, "Will of the Forsaken");
                    return true;
                }
            }
        }
    }
    // Draenei: Gift of the Naaru — instant 20% HP HoT over 5s. Fires when
    // bot is at 50% HP or below and not currently casting (we don't want to
    // interrupt our own heal). The HoT stacks with normal healing so it's
    // pure topup, no overheal waste.
    if (s.race() == RACE_DRAENEI || s.race() == RACE_LIGHTFORGED_DRAENEI)
    {
        constexpr uint32 GIFT_OF_THE_NAARU = 59548;
        if (s.knows_spell(GIFT_OF_THE_NAARU) && s.is_ready(GIFT_OF_THE_NAARU)
            && s.hp_pct() <= 50 && !s.is_casting())
        {
            emit.cast(GIFT_OF_THE_NAARU);
            return true;
        }
    }
    // Gnome: Escape Artist — instant root/snare break. A root always; a snare only while the bot is on the move (following,
    // kiting, stepping out of a hazard): a snared bot standing in its fight loses nothing and the racial's 1 minute cooldown is
    // better spent on the next root (Deadmines 2026-10-09: spent on a slow while the group was fighting).
    if (s.race() == RACE_GNOME)
    {
        constexpr uint32 ESCAPE_ARTIST = 20589;
        if (s.knows_spell(ESCAPE_ARTIST) && s.is_ready(ESCAPE_ARTIST))
        {
            bool rooted = s.is_rooted();
            bool snared = false;
            for (auto const& a : s.raw().auras.own_auras)
            {
                if (!a.is_harmful) continue;
                if (a.mechanic == MECHANIC_ROOT) rooted = true;
                else if (a.mechanic == MECHANIC_SNARE) snared = true;
            }
            bool const impaired = EscapeArtistWanted(rooted, snared, s.is_moving());
            if (impaired)
            {
                emit.cast(ESCAPE_ARTIST);
                Classic::NoteCooldownUse(s, ESCAPE_ARTIST, "Escape Artist");
                return true;
            }
        }
    }
    // Worgen: Darkflight — instant 40% movement speed for 10s. Fires when
    // HP is critical so the bot can attempt to disengage / kite. Only when
    // not already mounted (mounting beats it) and not currently in melee
    // (running away mid-melee just feeds boss damage; for casters / ranged
    // it buys time to reposition).
    if (s.race() == RACE_WORGEN)
    {
        constexpr uint32 DARKFLIGHT = 68992;
        if (s.knows_spell(DARKFLIGHT) && s.is_ready(DARKFLIGHT)
            && s.hp_pct() <= 30 && !s.is_mounted())
        {
            emit.cast(DARKFLIGHT);
            return true;
        }
    }
    // Night Elf: Shadowmeld — instant stealth + threat drop. The threat
    // drop is the headline feature here: when bot HP critical and we're
    // the highest threat target, dropping aggro lets the tank pick the
    // mob back up. Out of combat the bot can use it for stealth-walk by
    // shadowing the leader; that lives in State_InGroup.
    if (s.race() == RACE_NIGHTELF)
    {
        constexpr uint32 SHADOWMELD = 58984;
        if (s.knows_spell(SHADOWMELD) && s.is_ready(SHADOWMELD)
            && s.hp_pct() <= 25 && !s.is_casting())
        {
            // Skip if no nearby attackers (drop-aggro doesn't help solo).
            if (!s.raw().combat.attackers.empty())
            {
                emit.cast(SHADOWMELD);
                return true;
            }
        }
    }
    // Human: Every Man for Himself — instant break of stun/root/silence/fear/
    // charm/horror/disorient (PvP trinket equivalent). Triggers on any
    // gameplay-blocking debuff.
    if (s.race() == RACE_HUMAN)
    {
        constexpr uint32 EVERY_MAN_FOR_HIMSELF = 59752;
        if (s.knows_spell(EVERY_MAN_FOR_HIMSELF) && s.is_ready(EVERY_MAN_FOR_HIMSELF))
        {
            // Trigger on any blocking mechanic. Stun=10, Silence=15, Root=18,
            // Sap=21, all share the "we cannot act/cast" property.
            for (auto const& a : s.raw().auras.own_auras)
            {
                if (!a.is_harmful) continue;
                switch (a.mechanic)
                {
                    case 5: case 6: case 7: case 8: case 10:
                    case 11: case 12: case 14: case 15: case 18:
                    case 19: case 21: case 23:
                        emit.cast(EVERY_MAN_FOR_HIMSELF);
                        return true;
                    default: break;
                }
            }
        }
    }
    return false;
}

// Trinket on-use auto-fire. Trinkets without an ON_USE effect (most older
// procs and stat sticks) leave on_use_spell_id at 0 in the snapshot — the
// view helper skips those slots. Strict gates: actively in combat with a
// live victim, not currently casting (don't interrupt), one trinket per
// tick (the view picks the first ready of TRINKET1 then TRINKET2).
bool MaybeUseOnUseTrinket(BotSnapshotView const& s, BotIntentEmitter& emit)
{
    if (!s.in_combat() || s.victim().IsEmpty()) return false;
    if (s.is_casting()) return false;
    const uint32 sid = s.ready_on_use_trinket();
    if (sid == 0) return false;
    // PvP-aware gate: real players hoard the trinket for CC-break. If the
    // bot is fighting players (any attacker is a Player), only fire when
    // *actively* CC'd. The CC check walks own_auras for any harmful
    // mechanic (stun=10, fear=5, poly=10/disorient=6, silence=15, root=18,
    // charm=8, sleep=14, horror=24, incapacitate=16, sap=21, freeze=11) —
    // i.e. anything a Medallion of the Adventurer would remove. Without
    // this gate the bot blows a 2-5min CD on the first PvE-style offensive
    // proc and then has nothing left when the inevitable fear/poly lands.
    if (s.under_player_attack())
    {
        bool cc_active = s.is_stunned() || s.is_silenced() || s.is_rooted();
        if (!cc_active)
        {
            for (auto const& a : s.raw().auras.own_auras)
            {
                if (!a.is_harmful) continue;
                switch (a.mechanic)
                {
                    case 5:  case 6:  case 8:  case 10: case 11:
                    case 12: case 14: case 15: case 16: case 18:
                    case 19: case 21: case 23: case 24:
                        cc_active = true;
                        break;
                    default: break;
                }
                if (cc_active) break;
            }
        }
        if (!cc_active)
            return false;
    }
    // Most trinkets self-buff (ObjectGuid::Empty target); a few target enemy
    // (e.g. damage-dealing trinkets). cast() with no target lets Spell.cpp
    // pick implicit targets from the spell info.
    emit.cast(sid);
    return true;
}

// Race-appropriate offensive cooldown. Fired once per cooldown cycle while
// engaged in combat — these racials are pure DPS pulses with no friend-fire
// or off-GCD penalty. Returns true if a racial was emitted (this tick is
// then spent on it).
bool MaybeUseOffensiveRacial(BotSnapshotView const& s, Role role, BotIntentEmitter& emit)
{
    if (!s.in_combat() || s.victim().IsEmpty()) return false;

    // Classic realms: Berserking / Blood Fury are boss cooldowns inside a
    // dungeon or raid (Classic::BossFight - never trash); the open world keeps
    // firing them on cooldown.
    bool const vanilla = IsVanillaRealm();
    bool const burst_ok = !vanilla || !s.is_in_instance() || Classic::BossFight(s);

    // Spell IDs validated against WoW 12.0 client; the vanilla ids next to
    // them (knows_spell skips the ones the bot does not have).
    constexpr uint32 BERSERKING_IDS[] = { 26297, 26296, 20554 };   // Troll: unified / vanilla rage / vanilla mana
    constexpr uint32 BLOOD_FURY_IDS[] = { 33697, 20572 };          // Orc: unified / vanilla
    constexpr uint32 ANCESTRAL_CALL   = 274738;    // Mag'har Orc
    constexpr uint32 FIREBLOOD        = 265221;    // Dark Iron Dwarf
    constexpr uint32 BAG_OF_TRICKS    = 312411;    // Vulpera
    constexpr uint32 ARCANE_TORRENT_MELEE = 25046; // Blood Elf (melee variant)
    constexpr uint32 ARCANE_TORRENT_CASTER = 28730;// Blood Elf (caster, mana variant)

    // Bot::Race enum follows TrinityCore SharedDefines RACE_* values.
    switch (s.race())
    {
        case RACE_TROLL:
            if (!burst_ok) break;
            for (uint32 id : BERSERKING_IDS)
                if (s.knows_spell(id) && s.is_ready(id))
                {
                    emit.cast(id);
                    Classic::NoteCooldownUse(s, id, "Berserking");
                    return true;
                }
            break;
        case RACE_ORC:
        {
            if (!burst_ok) break;
            // Vanilla Blood Fury is attack power only and halves the healing
            // the orc takes for its duration: physical damage dealers only,
            // never a tank.
            if (vanilla && (role == Role::Tank || role == Role::Healer
                            || s.raw().pve_order.tank_duty != 0
                            || !(IsMeleeSpec(s.cls(), s.spec()) || s.cls() == CLASS_HUNTER)))
                break;
            for (uint32 id : BLOOD_FURY_IDS)
                if (s.knows_spell(id) && s.is_ready(id))
                {
                    emit.cast(id);
                    Classic::NoteCooldownUse(s, id, "Blood Fury");
                    return true;
                }
        } break;
        case RACE_MAGHAR_ORC:      if (s.knows_spell(ANCESTRAL_CALL) && s.is_ready(ANCESTRAL_CALL)) { emit.cast(ANCESTRAL_CALL); return true; } break;
        case RACE_TAUREN:
        {
            // War Stomp: 8yd PB AoE 2s stun + 5yd interrupt. Best when 3+
            // melee on us — the stun shaves a global off everyone.
            constexpr uint32 WAR_STOMP = 20549;
            if (s.knows_spell(WAR_STOMP) && s.is_ready(WAR_STOMP))
            {
                if (s.attackers_count() >= 3)
                {
                    emit.cast(WAR_STOMP);
                    return true;
                }
            }
        } break;
        case RACE_HIGHMOUNTAIN_TAUREN:
        {
            // Bull Rush: charges forward 25yd + 1s stun on enemies. Used as
            // a gap-closer for melee bots when the victim is out of range.
            constexpr uint32 BULL_RUSH = 255654;
            if (s.knows_spell(BULL_RUSH) && s.is_ready(BULL_RUSH))
            {
                if (NearbyUnit const* v = s.target_info())
                {
                    float bx, by, bz; s.position(bx, by, bz);
                    const float dx = v->x - bx, dy = v->y - by, dz = v->z - bz;
                    const float d2 = dx*dx + dy*dy + dz*dz;
                    if (d2 > 7.0f * 7.0f && d2 < 25.0f * 25.0f)
                    {
                        emit.cast(BULL_RUSH);
                        return true;
                    }
                }
            }
        } break;
        case RACE_BLOODELF:
        {
            // Arcane Torrent: 8yd PB AoE silence + small resource refund.
            // The melee variant returns 15 energy/runic-power; caster variant
            // returns 1.5% mana. Both versions silence enemy casters in 8yd —
            // most useful for interrupting boss casts when our normal kick is
            // on CD. Trigger on a nearby interruptible cast.
            const uint32 sid = s.knows_spell(ARCANE_TORRENT_CASTER) ? ARCANE_TORRENT_CASTER
                             : s.knows_spell(ARCANE_TORRENT_MELEE)  ? ARCANE_TORRENT_MELEE
                             : 0;
            if (sid && s.is_ready(sid))
            {
                // Find an interruptible caster within 8yd of the bot.
                if (NearbyUnit const* tgt = s.interruptible_caster())
                {
                    float bx, by, bz; s.position(bx, by, bz);
                    const float dx = tgt->x - bx, dy = tgt->y - by, dz = tgt->z - bz;
                    if (dx*dx + dy*dy + dz*dz <= 8.0f * 8.0f)
                    {
                        emit.cast(sid);
                        return true;
                    }
                }
            }
        } break;
        case RACE_DARK_IRON_DWARF: if (s.knows_spell(FIREBLOOD)      && s.is_ready(FIREBLOOD))      { emit.cast(FIREBLOOD);      return true; } break;
        case RACE_VULPERA:         if (s.knows_spell(BAG_OF_TRICKS)  && s.is_ready(BAG_OF_TRICKS))  { emit.cast(BAG_OF_TRICKS,   s.victim()); return true; } break;
        default: break;
    }
    return false;
}

// Mana refill (the owner's UsePotionsStrategy): at 20% mana or below (a healer in
// a boss fight: 35%) a conjured mana gem, then the best mana potion for the
// level, then a Dark / Demonic Rune. Same cast / spacing / cooldown gates as
// the healing item above.
bool MaybeUseManaPotion(BotSnapshotView const& s, GroupSnapshotView const& g, BotIntentEmitter& emit)
{
    uint32 const item = Classic::PickCombatManaItem(s, &g);
    if (!item)
    {
        // Due but a cast is running: a healer chaining heals has no idle tick, so stop it starting the next cast. The item goes out
        // on the tick after this cast ends.
        if (s.is_casting() && Classic::PickCombatManaItem(s, &g, /*while_casting=*/true))
            emit.hold_casts_for_mana_item();
        else
            Classic::NoteManaItemRefused(s);
        return false;
    }
    emit.end_mana_break();
    emit.emit(UseItemByEntryIntent{item, ObjectGuid::Empty});
    Classic::NoteConsumableUse(s, item, "mana item");
    return true;
}

} // anonymous

// ---- Group threat discipline ----
// Port of Karrz's wow-1.12 group threat rules (MoltenCoreDungeonActions.cpp
// MCRaidHoldDamage / MCRaidWaitForTank, MoltenCoreDungeonStrategies.cpp
// GroupPveRestraint), generalised from Molten Core to any group with a tank.
// The pure verdict lives in BotSnapshotView::threat_hold(); this layer adds
// role / map / config gating and the bounded opening wait.
namespace {

using ThreatHold = BotSnapshotView::ThreatHold;

// First tick a (bot, target) pair entered the opening hold. Rules run on the
// bot worker pool, so the map is shared and locked (the owner's firstHeld map).
std::mutex                         g_threat_hold_mu;
std::unordered_map<uint64, uint32> g_threat_hold_since;
struct FightWaitClock { uint32 first_ms; uint32 last_ms; };
std::unordered_map<uint64, FightWaitClock> g_threat_fight_clock;   // per bot: when its current opening wait began (EvaluateThreatHold)

// Raid boss / elite loose on a non-tank member (not this bot: the victim runs
// its own escape, LooseMobEscape) while a live tank is within 30y of it and
// coming for it: the tank is already on it (its victim) or free (no victim,
// so its loose-attacker pick-up / taunt takes it). Non-victim DPS then hold
// damage as if over threat until a tank has it back - every hit they land
// now is threat the tank must out-taunt. Raids only: 5-man and non-elite
// trash keep the owner's "everyone fights a mob on a member" rule. The group
// snapshot carries no cooldowns, so a tank's taunt readiness is approximated
// by "on this mob or free".
constexpr float kLooseBossTankReach = 90.0f;   // how far a tank may be from a boss on a member and still be coming for it

bool LooseRaidEliteOnMember(BotSnapshotView const& s, NearbyUnit const& u, GroupSnapshotView const* g)
{
    if (!Services::Config().behavior_loose_mob_hold()) return false;   // Playerbot.Behavior.LooseMobHold
    if (!g || !g->exists() || !s.is_in_raid() || s.in_battleground()) return false;
    if (u.is_player || u.hp <= 0 || !u.unit_in_combat) return false;
    if (!(u.is_dungeon_boss || u.is_boss_rank) && !u.is_elite_or_rare) return false;
    if (ThreatVictim(u.victim_kind) != ThreatVictim::Member) return false;
    // The raid's kill focus on its last fifth dies faster than a tank can
    // pick it up: everyone finishes it (the damage is not worth a retake).
    if (u.max_hp > 0 && int64(u.hp) * 5 <= int64(u.max_hp) && s.raw().pve_order.active && s.raw().pve_order.kill_focus == u.guid)
        return false;
    auto const* mems = g->members();
    if (!mems) return false;
    // The victim is a healer, or any member already under 70 %: waiting for a tank means waiting while it dies (MC Garr 2026-10-10: eight
    // loose Firesworn, two tanks alive, and the casters held 6 s on each add while the healers fell). The raid helps at once.
    for (GroupMemberSummary const& m : *mems)
        if (m.guid == u.victim && m.online && m.is_alive && (m.role == Role::Healer || m.hp_pct() < 70))
            return false;
    for (GroupMemberSummary const& m : *mems)
    {
        if (m.role != Role::Tank || !m.online || !m.is_alive || m.map_id != s.map_id()) continue;
        if (m.guid == u.victim) continue;
        const float dx = m.x - u.x, dy = m.y - u.y, dz = m.z - u.z;
        // A boss pulled onto a member far from its tank (Onyxia on a warlock, the tank 68 y off, n10 run 1): the tank is coming, and the
        // casters that opened at once at 66 y out-threatened it 2900 to 1332 and she turned on a mage. The hold is bounded below.
        float const reach = (u.is_dungeon_boss || u.is_boss_rank) ? kLooseBossTankReach : 30.0f;
        if (dx * dx + dy * dy + dz * dz > reach * reach) continue;
        if (m.victim == u.guid || m.victim.IsEmpty()) return true;
    }
    return false;
}

// The loose-elite hold above is bounded: a tank that is "on it or free" but
// cannot actually take the mob (taunt on cooldown and walled off, stunned,
// the mob out of its reach) must not leave the whole raid idle while the mob
// kills a healer. Per (bot, mob): held for kLooseHoldMaxMs, then the DPS joins
// in (more threat for the tank to out-taunt beats a dead healer). The clock
// restarts when the bot has not been held on that mob for kLooseHoldGapMs.
constexpr uint32 kLooseHoldMaxMs = 6000;
constexpr uint32 kLooseHoldGapMs = 8000;   // was 2.5 s: a held bot that stepped away for 3 s got a fresh 6 s hold (MC 2026-10-07)
std::mutex g_loose_hold_mu;
struct LooseHoldClock { uint32 first_ms; uint32 last_ms; };
std::unordered_map<uint64, LooseHoldClock> g_loose_hold;

// One bounded-hold clock per (bot, target): true once the hold has run max_ms. The clock restarts when the bot has
// not been held on that target for gap_ms. Shared by the loose-elite hold and the party over-threat hold.
bool HoldClockExpired(std::mutex& mu, std::unordered_map<uint64, LooseHoldClock>& clocks,
                      BotSnapshotView const& s, ObjectGuid target, uint32 max_ms, uint32 gap_ms)
{
    const uint32 now = s.published_at_ms();
    const uint64 key = (uint64(s.raw().guid.GetCounter()) << 32) | uint64(uint32(target.GetCounter()));
    std::lock_guard<std::mutex> lock(mu);
    if (clocks.size() > 1024)
        for (auto it = clocks.begin(); it != clocks.end();)
            it = (now - it->second.last_ms > 60'000u) ? clocks.erase(it) : std::next(it);
    auto [it, inserted] = clocks.try_emplace(key, LooseHoldClock{ now, now });
    if (!inserted)
    {
        if (now - it->second.last_ms > gap_ms)
            it->second.first_ms = now;
        it->second.last_ms = now;
    }
    return now - it->second.first_ms >= max_ms;
}

// A boss the encounter data says loses its threat on a schedule (Ragnaros' Wrath) is on a member for the seconds a tank needs to
// taunt it back, not for a tank that cannot reach it: the raid waits for that taunt, not for the full bound (mc_ragnaros: every
// Wrath cost 3 to 6 s of 24 DPS bots, about a tenth of the fight's damage).
constexpr uint32 kLooseHoldRetauntMaxMs = 2500;

bool LooseHoldExpired(BotSnapshotView const& s, ObjectGuid target, bool retaunting)
{
    return HoldClockExpired(g_loose_hold_mu, g_loose_hold, s, target, retaunting ? kLooseHoldRetauntMaxMs : kLooseHoldMaxMs, kLooseHoldGapMs);
}

// A 5-man party's over-threat hold is bounded the same way (Deadmines 2026-10-09 run 3: the hunter and the rogue sat
// in combat:threat_hold for 20 s while the healer ran dry). "Stay under 90% of the tank's threat" is a restraint for
// 10-40 bots nobody can watch; in a party the tank is one GCD from a taunt and the healer sees every hit. After
// kPartyOverThreatMaxMs on one target the DPS fights on: a mob that turns on a member is answered by the tank
// (pick-up / taunt) and by the member's own escape.
constexpr uint32 kPartyOverThreatMaxMs = 3000;
constexpr uint32 kPartyOverThreatGapMs = 4000;
std::mutex g_party_hold_mu;
std::unordered_map<uint64, LooseHoldClock> g_party_hold;

bool PartyOverThreatExpired(BotSnapshotView const& s, ObjectGuid target)
{
    return HoldClockExpired(g_party_hold_mu, g_party_hold, s, target, kPartyOverThreatMaxMs, kPartyOverThreatGapMs);
}

// Majordomo Executus' Damage Shield, on himself and his Flamewakers (the Magic Reflection twin, 20619, is refused by the cast API).
constexpr uint32 kDamageShieldAura = 21075;

ThreatHold EvaluateThreatHold(BotAI& ai, BotSnapshotView const& s, ObjectGuid target,
                              GroupSnapshotView const* g = nullptr)
{
    if (target.IsEmpty()) return ThreatHold::None;
    ConfigReader const& cfg = Services::Config();
    if (!cfg.threat_hold_enabled() || s.in_battleground()) return ThreatHold::None;
    if (!s.is_in_instance() && !cfg.threat_hold_open_world()) return ThreatHold::None;
    if (ai.effective_role(s) == Role::Tank) return ThreatHold::None;
    NearbyUnit const* u = s.hostile_unit(target);
    if (!u) return ThreatHold::None;

    // Majordomo's Damage Shield (21075) sends every melee swing back at the swinger (n71 Majordomo wipe: two melee DPS at 35-39 % died
    // to their own hits, and the melee kept swinging at the shielded adds for the whole fight). Melee wait the 10 s out, or until a
    // purge strips it; casters are not hit by it. Not a tank: its swings are the threat, and it has a healer.
    if (s.is_in_raid() && u->magic_buff_spell == kDamageShieldAura && IsMeleeSpec(s.cls(), s.spec()))
    {
        if (RaidLogDue(RaidLogKey(s, RaidLogKind::ThreatHold), s.published_at_ms(), 5000u))
            TC_LOG_INFO("playerbot.v2", "[raid_threat] {} holds melee on entry {} ({}): Damage Shield up",
                s.name(), u->entry, target.GetCounter());
        return ThreatHold::ShieldUp;
    }

    if (s.is_in_raid() && LooseRaidEliteOnMember(s, *u, g) && !LooseHoldExpired(s, target, Raid::Retaunts(s.map_id(), u->entry)))
    {
        if (RaidLogDue(RaidLogKey(s, RaidLogKind::ThreatHold), s.published_at_ms(), 5000u))
            TC_LOG_INFO("playerbot.v2",
                "[raid_threat] {} holds damage on entry {} ({}): loose on member {}, tank in reach",
                s.name(), u->entry, target.GetCounter(), u->victim.GetCounter());
        return ThreatHold::OverThreat;
    }

    // Opening lead shared with the AoE and heal gates (Bot/ThreatDiscipline.h).
    const bool  boss  = (u->is_dungeon_boss || u->is_boss_rank);
    const float lead  = ThreatOpeningLead(s, *u);
    // A raid holds at the tank's threat, not under 90 % of it: aggro moves at 110 % (melee) / 130 % (ranged) of the holder's, and the
    // hard limits below keep a margin of 5 / 10 points under that. 90 % for 24 DPS capped the whole raid at about the tank's
    // threat per second less a tenth (MC Garr 2026-10-10: about 70 damage per second per DPS bot, mages held at 93 %).
    constexpr float kRaidThreatMaxRatio = 1.0f;
    // A boss that cuts its tank's threat on a schedule (Onyxia's Knock Away, -25 %) is held at what stays of the tank's threat after a cut.
    float const threat_share = (boss && s.is_in_raid()) ? Raid::ThreatShareAfterDrop(s.map_id(), u->entry) : 1.0f;
    bool const melee_spec = IsMeleeSpec(s.cls(), s.spec());
    float const base_ratio = std::min(s.is_in_raid() ? std::max(cfg.threat_max_ratio(), kRaidThreatMaxRatio) : cfg.threat_max_ratio(), threat_share);
    // Early in a raid pull the hold starts a typical hit short of the tank's threat (ThreatEarlyHoldRatio).
    float const max_ratio = s.is_in_raid() ? ThreatEarlyHoldRatio(base_ratio, melee_spec, u->tank_threat, threat_share) : base_ratio;
    ThreatHold h = s.threat_hold(target, lead, max_ratio);
    // A 5-man party has one threat rule: the DPS wait for the owner's Attack, at most kPartyOpenWaitMs (6 s) from the tank's first hit (bounded below). The over-threat
    // hold and its release limits are raid discipline; in a party the tank holds with its own threat and the healer heals.
    if (!s.is_in_raid() && h == ThreatHold::OverThreat)
        h = ThreatHold::None;
    // After the owner's Pull / Pull back the party's DPS wait for his Attack even once the tank has its lead. There is one timer:
    // kPartyOpenWaitMs below (6 s from the tank's first hit) lets them open by themselves. The Pull counts for the fight that
    // follows it, until an Attack or the end of that fight. Not a mob that hits this bot (self-defence), not a healer.
    if (!s.is_in_raid() && s.is_in_dungeon() && h == ThreatHold::None && u->victim != s.guid() &&
        ai.effective_role(s) != Role::Healer)
    {
        uint32 const pull_ms = ai.owner_pull_ms(), t = s.published_at_ms();
        uint32 const last_exit_ms = uint32(ai.combat_exit_ms());   // end of the bot's previous fight (0: none yet)
        if (pull_ms && ai.owner_attack_ms() < pull_ms && last_exit_ms < pull_ms &&
            !(ai.is_manual_mode(t) && ai.owner_command() == BotAI::OwnerCommand::Engage && ai.owner_target() == target))
            h = ThreatHold::TankOpening;
    }
    // The release is bounded: a mob switches to a non-tank at 110% of the tank's threat in melee reach, 130% at range. Past 100% (melee)
    // / 110% (caster) the hold stays however long it has lasted - after the 3 s release a caster kept nuking over the tank for the
    // whole fight and pulled the Defias Miner that killed the mage (deadmines_full run 9). Below those the release stands.
    // Deadmines 2026-10-10: with the tank stunned (its threat standing still) the 3 s release let the rogue climb to 1725 against
    // the tank's 1663, past the tank with no hold left; a melee DPS therefore never releases above the tank's own threat, a caster
    // keeps a tenth of margin under the 130 % a ranged attacker needs to take the mob.
    // The scheduled cut lowers both limits: a hold released at 100 % of a tank about to lose a quarter of it pulls the boss.
    // While one more hit would take the bot past the pull-safe ratio, the early ratio is also the release limit: the hold stays until the tank has built up.
    const float hard_ratio = std::min({ ThreatHardRatio(melee_spec, s.is_in_raid(), u->tank_threat), threat_share,
                                        max_ratio < base_ratio ? max_ratio : 1.0e9f });
    const bool past_hard_limit = u->tank_threat > 0.0f && u->my_threat >= u->tank_threat * hard_ratio;
    if (h == ThreatHold::OverThreat && !past_hard_limit && PartyOverThreatExpired(s, target))
    {
        if (RaidLogDue(RaidLogKey(s, RaidLogKind::ThreatHold), s.published_at_ms(), 5000u))
            TC_LOG_INFO("playerbot.v2",
                "[party_threat] {} stops holding on entry {} ({}) after {} ms over the tank's threat mine={:.0f} tank={:.0f}",
                s.name(), u->entry, target.GetCounter(), kPartyOverThreatMaxMs, u->my_threat, u->tank_threat);
        h = ThreatHold::None;
    }
    if (h == ThreatHold::OverThreat && s.is_in_raid() &&
        RaidLogDue(RaidLogKey(s, RaidLogKind::ThreatHold), s.published_at_ms(), 5000u))
        TC_LOG_INFO("playerbot.v2",
            "[raid_threat] {} holds damage on entry {} ({}): over threat mine={:.0f} tank={:.0f}",
            s.name(), u->entry, target.GetCounter(), u->my_threat, u->tank_threat);
    // Raid DPS wait for the owner's Attack: the tanks pull, everyone else holds until the owner presses Attack
    // (/focus, an Engage order, or the group's mark release) - the owner, MC Gehennas 2026-10-08. A mob that hits this
    // bot or its pet is still answered (self-defence), and no mob held by a bot-led group is touched.
    if (h == ThreatHold::None && cfg.raid_dps_wait_attack() && s.is_in_raid() && g && !g->leader_is_bot() &&
        u->victim != s.guid() && ai.effective_role(s) != Role::Healer)
    {
        const uint32 t = s.published_at_ms();
        const bool released = s.raw().pve_order.marks_released || ai.focus_target() == target ||
            (ai.is_manual_mode(t) && ai.owner_command() == BotAI::OwnerCommand::Engage && ai.owner_target() == target);
        if (!released)
        {
            if (RaidLogDue(RaidLogKey(s, RaidLogKind::ThreatHold), t, 5000u))
                TC_LOG_INFO("playerbot.v2", "[raid_threat] {} waits for the owner's attack on entry {} ({})",
                    s.name(), u->entry, target.GetCounter());
            return ThreatHold::TankOpening;
        }
    }
    if (h != ThreatHold::TankOpening) return h;

    const uint32 now = s.published_at_ms();

    // The owner's Attack on this target is instant in a party: no wait for the tank's lead (the over-threat limits above still hold).
    // Pull and Pull back put only the tank into Engage, so they keep the tank first.
    if (!s.is_in_raid() && ai.is_manual_mode(now) && ai.owner_command() == BotAI::OwnerCommand::Engage && ai.owner_target() == target)
        return ThreatHold::None;

    // Unpulled mob (nobody in the group has engaged it): opening on it IS a
    // pull, and pulls are the main tank's. No timeout - except for the
    // owner's explicit target (/focus, an Engage command, or the group's
    // skull / cross raid mark), which gets the tank kMaxUnpulledOwnerWaitMs
    // to take it first. A mob attacking this bot or another non-tank never
    // gets here (threat_hold: self-defence). A mark counts only under a
    // human leader: a bot leader's marks are its in-combat auto-skull, and
    // one left on an idle mob is a leftover of the last fight (MC
    // 2026-10-06, GroupSnapshotView::leader_is_bot).
    constexpr uint32 kMaxUnpulledOwnerWaitMs = 8000;   // a tank walking in from 25 y needs more than 4 s (Deadmines 2026-10-15: the mage opened first)
    const bool owner_pick = ai.focus_target() == target ||
        (ai.is_manual_mode(now) && ai.owner_command() == BotAI::OwnerCommand::Engage &&
         ai.owner_target() == target) ||
        (g && g->explicit_kill_mark() == target && !g->leader_is_bot() &&
         MarkGate::MayEngage(s, ai, g, target, u));   // MarksNeedOrder: a mark alone is no pull call
    if (!u->unit_in_combat && !owner_pick)
    {
        if (s.is_in_raid() && RaidLogDue(RaidLogKey(s, RaidLogKind::ThreatHold), now, 5000u))
            TC_LOG_INFO("playerbot.v2",
                "[raid_threat] {} waits for tank on entry {} ({}): unpulled, not opening on it",
                s.name(), u->entry, target.GetCounter());
        return ThreatHold::TankOpening;
    }

    // Engaged mob, tank under its opening lead (ThreatOpeningLead): open as
    // soon as the lead is there (threat_hold above stops returning
    // TankOpening), or after a bounded wait so a tank that cannot build
    // threat (out of reach, stunned, dead) never freezes the raid. The
    // config waits are honoured when longer.
    constexpr uint32 kMaxOpenWaitTrashMs = 3500;   // from the tank's first damage on the mob (playtest 2026-10-15)
    constexpr uint32 kMaxOpenWaitBossMs  = 6000;   // playtest 2026-10-15: never longer than six seconds, boss or not
    // A party's DPS wait for the owner's Attack after a Pull / Pull back and open by themselves this long after the tank's first hit
    // (playtest 2026-10-15: six seconds, in case the owner forgets).
    constexpr uint32 kPartyOpenWaitMs    = 6000;
    uint32 wait_ms = !u->unit_in_combat ? kMaxUnpulledOwnerWaitMs
                   : boss ? std::min<uint32>(kMaxOpenWaitBossMs, cfg.threat_opening_wait_boss_ms())
                          : (s.is_in_raid() ? std::max<uint32>(kMaxOpenWaitTrashMs, cfg.threat_opening_wait_trash_ms()) : kPartyOpenWaitMs);
    // A raid trash mob a tank already holds (it is the mob's victim) needs no long lead wait: its
    // threat only has to start building, so DPS opens ~2 s after the tank has it (MC 2026-10-06:
    // 26 bots sat the full 4 s on a Giant the tank was already hitting).
    // A party's trash the tank already holds gets the same short wait: every new target of a multi-mob pull restarts the
    // clock, and 4 s a target left the DPS idle through most of an 8-mob fight (Deadmines 2026-10-09 run 3).
    if (s.is_in_raid() && u->unit_in_combat && !boss && ThreatVictim(u->victim_kind) == ThreatVictim::Tank)
        wait_ms = std::min<uint32>(wait_ms, 2000u);
    // Separate clocks for the unpulled and the engaged phase of a target, so
    // time spent waiting for the pull does not eat the tank's opening wait.
    const uint64 key = (uint64(s.raw().guid.GetCounter()) << 32) |
                       uint64(uint32(target.GetCounter()) & 0x7FFFFFFFu) |
                       (u->unit_in_combat ? 0x80000000ull : 0ull);
    std::lock_guard<std::mutex> lock(g_threat_hold_mu);
    if (g_threat_hold_since.size() > 1024)
        for (auto it = g_threat_hold_since.begin(); it != g_threat_hold_since.end();)
            it = (now - it->second > 120'000u) ? g_threat_hold_since.erase(it) : std::next(it);
    auto [it, inserted] = g_threat_hold_since.try_emplace(key, now);
    if (!inserted && now - it->second > 120'000u) it->second = now;   // a new fight on a reused target
    // One opening wait per fight, not per mob: a bot that has already waited for the tank in this fight (held within the last 8 s)
    // does not wait again for each next mob of the pull (Deadmines 2026-10-09: the clock restarted on every target).
    uint32 since = it->second;
    // Counted from the tank's first damage: only the engaged phase feeds the fight clock, so the time spent waiting for the pull
    // (up to kMaxUnpulledOwnerWaitMs) is not taken off the opening wait, and a mob in combat gets the DPS 3.5 s after the first hit.
    if (u->unit_in_combat)
    {
        auto [fit, finserted] = g_threat_fight_clock.try_emplace(s.raw().guid.GetCounter(), FightWaitClock{ now, now });
        if (!finserted)
        {
            if (now - fit->second.last_ms > 8000u) fit->second.first_ms = now;
            fit->second.last_ms = now;
        }
        since = std::min(since, fit->second.first_ms);
    }
    if (now - since < wait_ms)
    {
        if (inserted && s.is_in_raid() &&
            RaidLogDue(RaidLogKey(s, RaidLogKind::ThreatHold), now, 5000u))
            TC_LOG_INFO("playerbot.v2",
                "[raid_threat] {} waits for tank on entry {} ({}): {} tank_threat={:.0f}/{:.0f} max wait={}ms",
                s.name(), u->entry, target.GetCounter(),
                u->unit_in_combat ? "opening" : "unpulled owner target",
                u->tank_threat, lead, wait_ms);
        return ThreatHold::TankOpening;
    }
    if (s.is_in_raid() && RaidLogDue(RaidLogKey(s, RaidLogKind::ThreatHold), now, 5000u))
        TC_LOG_INFO("playerbot.v2",
            "[raid_threat] {} max wait over on entry {} ({}): attacking with tank_threat={:.0f}/{:.0f}{}",
            s.name(), u->entry, target.GetCounter(), u->tank_threat, lead,
            u->unit_in_combat ? "" : " (owner's target, not yet pulled)");
    return ThreatHold::None;
}

// Over the tank's threat: shed it with the class's vanilla threat drop when
// one is ready (the owner's bots used the same set). Returns true on an emit.
bool TryThreatDrop(BotSnapshotView const& s, ObjectGuid target, BotIntentEmitter& emit)
{
    struct Drop { uint8 cls; uint32 spell; uint32 needs_aura; bool on_target; };
    static constexpr Drop kDrops[] = {
        { CLASS_PRIEST, 586,  0,   false },   // Fade
        { CLASS_HUNTER, 5384, 0,   false },   // Feign Death
        { CLASS_ROGUE,  1966, 0,   true  },   // Feint
        { CLASS_DRUID,  8998, 768, true  },   // Cower (Cat Form)
    };
    for (Drop const& d : kDrops)
    {
        if (d.cls != s.cls() || !s.knows_spell(d.spell) || !s.is_ready(d.spell)) continue;
        if (d.needs_aura && !s.has_aura(d.needs_aura)) continue;
        if (emit.cast(d.spell, d.on_target ? target : ObjectGuid::Empty)) return true;
    }
    return false;
}

// Loose elite / boss on this non-tank bot in a tanked dungeon or raid group
// (the owner's bots: the victim gets out of the way so the tank can pick the mob
// up). Hunter Feign Death, rogue Vanish (else Feint), priest Fade at once;
// a mage Frost Novas a mob in melee reach first; then everyone steps toward
// its anchor tank - a Classic healer casting its instant heals / HoTs /
// shields on the way (cast-time heals wait until it stops). Solo / tankless
// groups never get here. Returns true when it claimed the tick.
bool LooseMobEscape(BotAI& ai, BotSnapshotView const& s, GroupSnapshotView const& g, BotIntentEmitter& emit)
{
    if (!g.exists() || !s.is_in_instance() || s.in_battleground() || !s.is_alive()) return false;
    if (s.group_tank().IsEmpty() || s.group_tank() == s.guid()) return false;
    if (ai.effective_role(s) == Role::Tank || s.raw().group.my_role == Role::Tank) return false;

    NearbyUnit const* mob = nullptr;
    auto consider = [&](NearbyUnit const& u)
    {
        if (u.hp <= 0 || u.is_player || u.victim != s.guid()) return;
        if (!(u.is_dungeon_boss || u.is_boss_rank) && !u.is_elite_or_rare) return;
        if (!mob || ((u.is_dungeon_boss || u.is_boss_rank) && !(mob->is_dungeon_boss || mob->is_boss_rank))) mob = &u;
    };
    for (NearbyUnit const& u : s.attackers()) consider(u);
    for (NearbyUnit const& u : s.nearby_enemies()) consider(u);
    if (!mob) return false;

    constexpr uint32 FEIGN_DEATH = 5384, VANISH = 1856, FEINT = 1966, FADE = 586, FROST_NOVA = 122;
    if (s.cls() == CLASS_HUNTER && s.has_aura(FEIGN_DEATH))
    {
        ai.set_last_rule_fired("combat:loose_escape_feign");   // lying still: moving breaks it
        return true;
    }

    const uint32 now = s.published_at_ms();
    const bool log = RaidLogDue(RaidLogKey(s, RaidLogKind::Escape), now, 4000u);
    auto note = [&](char const* what)
    {
        if (log)
            TC_LOG_INFO("playerbot.v2", "[raid_escape] {} sheds entry {} ({}): {}",
                        s.name(), mob->entry, mob->guid.GetCounter(), what);
    };

    float bx, by, bz; s.position(bx, by, bz);
    const float mdx = mob->x - bx, mdy = mob->y - by, mdz = mob->z - bz;
    const float mob_d2 = mdx * mdx + mdy * mdy + mdz * mdz;

    if (!s.is_casting())
    {
        uint32 drop = 0;
        bool on_target = false;
        switch (s.cls())
        {
            case CLASS_HUNTER: drop = FEIGN_DEATH; break;
            case CLASS_PRIEST: drop = FADE; break;
            case CLASS_ROGUE:
                if (s.is_ready(VANISH)) drop = VANISH;
                else { drop = FEINT; on_target = true; }
                break;
            case CLASS_MAGE:
                // Root it before running (Frost Nova reaches 10y). Never on a
                // crowd-controlled neighbour or a school-immune mob.
                if (mob_d2 <= 8.0f * 8.0f && s.is_ready(FROST_NOVA))
                {
                    AoeHazard const h = FindAoeHazard(s, bx, by, bz, 10.0f, /*damaging*/ true,
                                                      /*caster_is_tank*/ false, FROST_NOVA);
                    if (h.kind != AoeHazardKind::CrowdControlled && h.kind != AoeHazardKind::SchoolAvoided)
                        drop = FROST_NOVA;
                }
                break;
            default: break;
        }
        if (drop && s.is_ready(drop) && emit.cast(drop, on_target ? mob->guid : ObjectGuid::Empty))
        {
            note(drop == FEIGN_DEATH ? "feign death" : drop == VANISH ? "vanish" : drop == FEINT ? "feint"
                 : drop == FADE ? "fade" : "frost nova");
            ai.set_last_rule_fired("combat:loose_escape_drop");
            return true;
        }
    }

    // A caster mob stays where it is and casts at range: a run to the tank changes nothing (its Flamestrike lands on the runner, who
    // has stopped hitting it). The threat drop above and the kill-focus stay; the tank picks it up through its own pull logic.
    if (mob->is_ranged_caster)
        return false;

    // Step toward the tank so it can taunt / body-pull the mob off us.
    GroupMemberSummary const* tk = GroupAnchorTank(s, ai, g);
    if (!tk || !tk->online || !tk->is_alive || tk->guid == s.guid() || tk->map_id != s.map_id() || s.is_rooted())
        return false;
    // The tank is feared / stunned: it picks nothing up, and a feared one runs off, so the walk toward it only drags the mob with us
    // and costs the casts (Deadmines 627eb082: the priest "stepping to tank" with Terrify on the tank, 25 y away). Stand and cast.
    if (tk->is_controlled)
    {
        note("tank is controlled: staying to cast");
        return false;
    }
    const float dx = tk->x - bx, dy = tk->y - by, dz = tk->z - bz;
    const float d2 = dx * dx + dy * dy + dz * dz;
    if (d2 <= 6.0f * 6.0f || d2 > 80.0f * 80.0f) return false;   // already on the tank / not this fight's tank
    const float dist = std::sqrt(d2);
    const float step = std::min(15.0f, dist - 4.0f);
    // A healer keeps healing on the run: instant heals / HoTs / Power Word:
    // Shield go out alongside the step (ClassicHeal::EmitInstantHealWhileMoving);
    // cast-time heals and channels wait until it stands - the rotation runs
    // again once this returns false (on the tank, rooted, mob gone).
    const bool healer = IsVanillaRealm() && ai.effective_role(s) == Role::Healer;
    auto heal_on_the_run = [&]() -> bool
    {
        if (!healer) return false;
        uint32 const sp = Combat::ClassicHeal::EmitInstantHealWhileMoving(s, g, emit);
        if (!sp) return false;
        if (log)
            TC_LOG_INFO("playerbot.v2", "[raid_escape] {} heals on the run ({}) from entry {} ({})",
                        s.name(), sp, mob->entry, mob->guid.GetCounter());
        ai.set_last_rule_fired("combat:loose_escape_heal_moving");
        return true;
    };
    const float sx = bx + dx / dist * step, sy = by + dy / dist * step, sz = bz + dz / dist * step;
    // Never run into a ground hazard / raid avoid circle on the way: the
    // rotation keeps the bot alive in place (and the drops above already ran).
    // Nor into an idle pack's aggro reach (raid): the tank comes for the mob
    // instead (its taunt / pick-up), a step into the next pack adds one.
    if (RaidStandPointBlocked(s, sx, sy, sz) || RaidPointInUnpulledReach(s, sx, sy, sz)) return false;
    if (!emit.move_to(sx, sy, sz, /*run*/ true))
    {
        if (!s.has_path_destination()) return false;
        heal_on_the_run();                 // same step already under way: keep it
        return true;
    }
    note("stepping to tank");
    if (!heal_on_the_run())
        ai.set_last_rule_fired("combat:loose_escape_to_tank");
    return true;
}

// A ranged DPS of a 5-man with a mob in melee on it (deadmines_full run, hunter: victim a far elite at 12-41 y while a Miner hit
// it at 3 y, the raid_spacing / raid_los steps walked it around and it died). In vanilla it turns on the mob that is on it, or
// runs the mob to the tank. Tank within 25 y but not at hand: step toward it (the tank taunts / body-pulls the mob off us);
// otherwise shoot / strike the attacker (Wing Clip, Raptor Strike, the rotation fires on the new victim). Elites and bosses on
// the bot are LooseMobEscape's; a caster mob stays at range and is the kill focus's. Returns true when it claimed the tick.
bool RangedMeleedDefense(BotAI& ai, BotSnapshotView const& s, GroupSnapshotView const& g, BotIntentEmitter& emit)
{
    if (!g.exists() || !s.is_in_instance() || s.is_in_raid() || s.in_battleground() || !s.is_alive()) return false;
    if (ai.effective_role(s) != Role::Dps || IsMeleeSpec(s.cls(), s.spec()) || s.is_casting()) return false;
    if (!s.raw().pve_order.cc_target.IsEmpty()) return false;   // on crowd-control duty: it holds its own mob

    float bx, by, bz; s.position(bx, by, bz);
    NearbyUnit const* mob = nullptr;
    float mob_d2 = 0.f;
    for (NearbyUnit const& u : s.attackers())
    {
        if (u.hp <= 0 || u.is_player || u.victim != s.guid() || u.is_cc_locked || u.is_ranged_caster) continue;
        const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > 10.0f * 10.0f || (mob && d2 >= mob_d2)) continue;
        mob = &u; mob_d2 = d2;
    }
    if (!mob) return false;

    const uint32 now = s.published_at_ms();
    const bool log = RaidLogDue(RaidLogKey(s, RaidLogKind::Escape), now, 4000u);
    GroupMemberSummary const* tk = GroupAnchorTank(s, ai, g);
    if (tk && tk->online && tk->is_alive && tk->guid != s.guid() && tk->map_id == s.map_id() && !s.is_rooted())
    {
        const float dx = tk->x - bx, dy = tk->y - by, dz = tk->z - bz;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist > 8.0f && dist <= 25.0f)
        {
            const float step = std::min(10.0f, dist - 5.0f);
            const float sx = bx + dx / dist * step, sy = by + dy / dist * step, sz = bz + dz / dist * step;
            // Never into an idle pack's aggro reach: the run to the tank pulled the next pack (playtest, Deadmines 2026-10-10).
            if (!RaidStandPointBlocked(s, sx, sy, sz) && !RaidPointInUnpulledReach(s, sx, sy, sz))
            {
                if (emit.move_to(sx, sy, sz, /*run*/ true) || s.has_path_destination())
                {
                    if (log)
                        TC_LOG_INFO("playerbot.v2", "[raid_escape] {} runs entry {} ({}) to the tank, {:.0f}y away (mob {:.0f}y)",
                                    s.name(), mob->entry, mob->guid.GetCounter(), dist, std::sqrt(mob_d2));
                    ai.set_last_rule_fired("combat:ranged_run_to_tank");
                    return true;
                }
            }
        }
    }
    if (s.victim() == mob->guid) return false;   // already on it: the rotation (Wing Clip, strikes, shots) runs
    if (!emit.start_attack(mob->guid)) return false;
    if (log)
        TC_LOG_INFO("playerbot.v2", "[raid_escape] {} turns on entry {} ({}) that is on it at {:.0f}y (was on {})",
                    s.name(), mob->entry, mob->guid.GetCounter(), std::sqrt(mob_d2), s.victim().GetCounter());
    ai.set_last_rule_fired("combat:ranged_self_defense");
    return true;
}

// Raid overheal cancel - the "stopcast" technique (Karrz's ike3 healers stop a
// heal whose target got topped up meanwhile). Direct single-target heals
// only: no channel, no Chain Heal, no party / area heal (Prayer of Healing's
// cast target is the priest itself). Mana is spent when a cast completes, so
// the healer lets the cast run - damage landing meanwhile makes the heal
// useful after all - and decides in its last moments: kWindowMaxMs ..
// kWindowMinMs left as the snapshot saw it (the snapshot age and the intent
// drain take ~100-200 ms more, so the stop reaches the server with ~0.1-0.5 s
// to go), it stops when the target is at 95%+ or missing less than 30% of
// the heal's expected amount (ClassicHeal::ExpectedHeal, bonus healing
// included). Before that window only a target already at full health with
// nothing hitting it is stopped (another healer landed first: the healer is
// freed at once). After a cancel the rotation leaves that patient alone for
// ClassicHeal::kCancelLockoutMs (an emergency excepted) and the tank pre-heal
// waits as long, so a cancelled cast is not restarted at once; at most one
// cancel per 1.5 s.
bool CancelOverheal(BotAI& ai, BotSnapshotView const& s, GroupSnapshotView const& g, BotIntentEmitter& emit)
{
    constexpr int64 kWindowMinMs = 200;
    constexpr int64 kWindowMaxMs = 600;
    if (!s.is_casting() || !s.is_in_raid() || s.in_battleground() || !g.exists()) return false;
    const int64 rem = s.current_cast_remaining().count();
    if (rem < kWindowMinMs) return false;   // lands before a stop would reach the server
    const ObjectGuid tgt = s.current_cast_target();
    if (tgt.IsEmpty()) return false;
    SpellInfo const* si = sSpellMgr->GetSpellInfo(s.current_cast_spell_id(), DIFFICULTY_NONE);
    if (!si || si->IsChanneled()) return false;
    for (SpellEffectInfo const& eff : si->GetEffects())
        if (eff.IsEffect(SPELL_EFFECT_HEAL) && (eff.ChainTargets > 1 || eff.IsTargetingArea()))
            return false;                                                        // Chain Heal, party heals
    const float amount = Combat::ClassicHeal::ExpectedHeal(si, s);
    if (amount <= 0.0f) return false;                                            // not a direct heal
    Combat::ClassicHeal::Patient const p = Combat::ClassicHeal::PatientFor(s, g, tgt);
    if (!p.valid || p.max_hp <= 0 || p.hp <= 0) return false;
    const int64 deficit = p.max_hp - p.hp;
    const bool final_window = rem <= kWindowMaxMs;
    const bool cancel = final_window ? (p.actual_pct >= 95 || float(deficit) < amount * 0.30f)
                                     : (deficit <= 0 && p.incoming <= 0);
    if (!cancel) return false;
    const uint32 now = s.published_at_ms();
    if (!RaidLogDue(RaidLogKey(s, RaidLogKind::CancelHeal), now, 1500u)) return false;
    if (!emit.cancel_cast()) return false;
    Combat::ClassicHeal::NoteHealCancel(s, tgt);
    TC_LOG_INFO("playerbot.v2", "[raid_heal] {} cancels {} on {}{} with {} ms left: {}% (deficit {}, heal ~{:.0f}){}",
                s.name(), si->Id, tgt.GetCounter(), p.tank ? " (tank)" : "", rem, p.actual_pct, deficit, amount,
                final_window ? "" : ", already full");
    ai.set_last_rule_fired("combat:cancel_overheal");
    return true;
}

// Raid tank pre-heal (the other half of the stopcast technique): a healer the
// coordinator put on a tank (pve_order.heal_focus naming a tank) starts its
// tank heal while a boss - or anything hitting it for 5%+ a round of swings -
// is meleeing that tank, before the damage lands; CancelOverheal stops it in
// its last half-second when the tank is still topped up, so the mana is only
// spent when the hit came. Raid, in combat, standing, 25%+ mana, the tank
// above 65% (a hurt tank is the rotation's) within 40 yd and in line of
// sight, nobody else under 80% (projected), no dispel job for this bot, and
// not within ClassicHeal::kCancelLockoutMs of this healer's last cancel. The
// rank covers the deficit plus the swings expected over the cast
// (ClassicHeal::PlanRank with the bonus healing).
bool PreemptiveTankHeal(BotAI& ai, BotSnapshotView const& s, GroupSnapshotView const& g, BotIntentEmitter& emit)
{
    namespace CH = Combat::ClassicHeal;
    if (!CH::RaidHealing(s) || !s.in_combat() || s.is_casting() || s.is_moving() || !g.exists()) return false;
    if (ai.effective_role(s) != Role::Healer) return false;
    auto const& po = s.raw().pve_order;
    if (!po.active || po.heal_focus.IsEmpty() || po.heal_focus == s.guid()) return false;
    // A luxury: below kSaveManaPct a healer keeps its mana for the heals the
    // damage has already earned (mckill 2026-10-06: two pre-healers ran from
    // 60% to 12% mana casting at a full-health tank).
    if (CH::ManaPct(s) < CH::kSaveManaPct) return false;
    const uint32 spell = CH::TankHealSpell(s);
    if (!spell || !s.is_ready(spell)) return false;
    if (s.cls() == CLASS_DRUID)
        for (uint32 form : { 768u, 5487u, 9634u, 24858u, 783u, 1066u })   // Cat, Bear, Dire Bear, Moonkin, Travel, Aquatic
            if (s.has_aura(form)) return false;
    if (CH::InCancelLockout(s)) return false;

    GroupMemberSummary const* tk = nullptr;
    if (auto const* mems = g.members())
        for (GroupMemberSummary const& m : *mems)
            if (m.guid == po.heal_focus) { tk = &m; break; }
    if (!tk || !tk->online || !tk->is_alive || tk->max_hp <= 0 || tk->map_id != s.map_id() || !CH::IsTank(s, *tk))
        return false;
    float bx, by, bz; s.position(bx, by, bz);
    if (CH::Dist2(tk->x, tk->y, tk->z, bx, by, bz) > CH::kHealRange * CH::kHealRange || !CH::InLos(s, tk->guid))
        return false;
    CH::Patient const tank = CH::MakePatient(s, tk->guid, tk->hp, tk->max_hp, tk->x, tk->y, tk->z, true);
    if (tank.actual_pct <= 65 || tank.incoming <= 0) return false;
    // No top-off on a tank near full unless the swings on it would take it under the spike mark (a boss on the tank hits every
    // 2 s; a heal per hit was the largest mana sink of the Garr kill, MC 2026-10-10).
    if (tank.actual_pct >= CH::kRaidTankSpikeFloorPct && tank.hp_pct >= CH::kRaidTankSpikePct) return false;
    bool boss = false;
    for (NearbyUnit const& u : s.nearby_enemies())
        if (u.hp > 0 && u.victim == tank.guid && Classic::IsBossUnit(u)) { boss = true; break; }
    if (!boss && tank.incoming * 20 < tank.max_hp) return false;   // light trash hits: the rotation's
    // Trash is the rotation's heal, not a pre-heal: a 5-man healer's pool is small, and pre-healing a 78-100% tank on every trash
    // pull was most of the level-21 priest's mana (deadmines_full run 10). In a raid it stays for mana of 70% and up.
    if (!boss && (!s.is_in_raid() || CH::ManaPct(s) < 70)) return false;
    CH::Patient const best = CH::PickPatient(s, g);
    if (best.valid && best.guid != tank.guid && best.hp_pct < 80) return false;
    if (Classic::PickDispel(s, g).spell) return false;

    const uint32 top = s.resolve_rank(spell);
    SpellInfo const* si = top ? sSpellMgr->GetSpellInfo(top, DIFFICULTY_NONE) : nullptr;
    const int32 cast_ms = si && si->CastTimeEntry ? std::max(si->CastTimeEntry->Base, 0) : 2500;
    const float swings = std::max(1.0f, float(cast_ms) / 2000.0f);
    const float want = float(tank.max_hp - tank.hp) + float(tank.incoming) * swings;
    // Only when the swings expected over the cast take the tank clearly under
    // full: at 100% a 10% hit leaves it at 90% and the heal lands as an
    // overheal (MC 2026-10-06: pre-heals on a 100% tank, cancelled or not,
    // were the largest mana sink of the kill).
    // A boss on the tank is different: its swings are certain and the tank must never start a fight behind (MC Gehennas
    // 2026-10-08: tank dead before the first heal landed), so its healers keep a heal going from 95% projected;
    // CancelOverheal still stops the cast in its last half-second when the hit did not come.
    constexpr float kPreHealProjectedPct = 85.0f;
    constexpr float kPreHealBossProjectedPct = 95.0f;
    float const projected_limit = boss ? kPreHealBossProjectedPct : kPreHealProjectedPct;
    if ((float(tank.hp) - float(tank.incoming) * swings) * 100.0f >= float(tank.max_hp) * projected_limit) return false;
    // Another healer's pre-heal already on its way covers the deficit.
    if (CH::HealClaimsOn())   // Playerbot.Behavior.HealClaims
    {
        CH::Patient covered = tank;
        CH::ApplyInFlight(s, g, covered, CH::CopyHealClaims(s.published_at_ms()));
        if (CH::CoveredByInFlight(s, covered)) return false;
    }
    CH::HealPlan const plan = CH::PlanRank(s, spell, want, /*bonus*/ true);
    const bool sent = plan.rank ? emit.cast_rank(plan.rank, tank.guid) : emit.cast(spell, tank.guid);
    if (!sent) return false;
    CH::NoteHealClaim(s, tank.guid, plan.rank ? plan.rank : top, plan.est > 0.0f ? plan.est : plan.want);
    if (RaidLogDue(CH::HealLogKey(s, CH::HealLogKind::PreHeal), s.published_at_ms(), 5000u))
        TC_LOG_INFO("playerbot.v2", "[raid_heal] {} pre-heals tank {} at {}% ({} incoming{}) with {} (want {:.0f}, est {:.0f}, mana {}%)",
                    s.name(), tank.guid.GetCounter(), tank.actual_pct, tank.incoming, boss ? ", boss" : "",
                    plan.rank ? plan.rank : top, plan.want, plan.est, CH::ManaPct(s));
    ai.set_last_rule_fired("combat:raid_preheal_tank");
    return true;
}

// Hold on a target the bot is NOT attacking (the re-acquire paths): a melee
// bot still walks up to the mob - the owner's "reach melee" was never vetoed -
// so it is in place the moment the hold lifts. Returns true when it claimed
// the tick.
bool ThreatHoldWait(BotAI& ai, BotSnapshotView const& s, BotIntentEmitter& emit,
                    ObjectGuid target, ThreatHold h, bool melee)
{
    if (melee && !s.is_rooted())
        if (NearbyUnit const* u = s.hostile_unit(target); u && u->unit_in_combat)   // walking onto an idle mob pulls it
        {
            float bx, by, bz; s.position(bx, by, bz);
            const float dx = u->x - bx, dy = u->y - by, dz = u->z - bz;
            if (dx * dx + dy * dy + dz * dz > 4.5f * 4.5f)
            {
                bool already = false;
                if (s.has_path_destination())
                {
                    float px, py, pz; s.path_destination(px, py, pz);
                    const float qx = u->x - px, qy = u->y - py, qz = u->z - pz;
                    already = qx * qx + qy * qy + qz * qz < 3.0f * 3.0f;
                }
                if (!already) emit.move_to(u->x, u->y, u->z, /*run*/ true);
            }
        }
    ai.set_last_rule_fired(h == ThreatHold::OverThreat ? "combat:threat_hold"
                           : h == ThreatHold::ShieldUp ? "combat:shield_hold"
                                                       : "combat:threat_wait_tank");
    return true;
}


// [dps_idle_reason] probe (live MC 2026-10-06: "only about half the casters do
// anything"). Runs when DispatchInCombat returns, whichever path it took: a
// ranged DPS bot that neither pushed a cast / attack / item use this tick nor
// is casting, channelling, wanding / auto-shooting or on the GCD is idle.
// After >= 3 s of that, one INFO line per 10 s per bot says why: the rule that
// held it this tick (threat / AoE / school hold, kill focus, raid holds...)
// or, when none fired, what the snapshot shows (no target, out of LoS / range,
// moving, OOM).
struct CombatIdleProbe
{
    BotAI&                  ai;
    BotSnapshotView const&  s;
    BotIntentEmitter const& emit;
    uint32                  actions_before;
    uint32                  rules_before;

    ~CombatIdleProbe()
    {
        BotAI::CombatIdleState& st = ai.combat_idle();
        uint32 const now = s.published_at_ms();
        bool const dps = s.is_alive() && ai.effective_role(s) == Role::Dps;
        bool const melee = IsMeleeSpec(s.cls(), s.spec());
        // A melee bot swings without any emitted action: it is busy while its victim is in reach.
        bool swinging = false;
        if (melee)
            if (NearbyUnit const* vi = s.victim_info(); vi && vi->hp > 0)
            {
                float bx, by, bz; s.position(bx, by, bz);
                float const dx = vi->x - bx, dy = vi->y - by;
                swinging = std::sqrt(dx * dx + dy * dy) <= 5.0f + vi->combat_reach;
            }
        bool const acting = !dps || emit.combat_action_count() != actions_before || s.is_casting() ||
                            s.raw().cast.is_autorepeat || s.gcd_active() || swinging;
        if (acting)
        {
            st.idle_since_ms = 0;
            return;
        }
        if (st.idle_since_ms == 0)
        {
            st.idle_since_ms = now ? now : 1u;
            return;
        }
        uint32 const idle_ms = now - st.idle_since_ms;
        if (idle_ms < 3000u || (st.logged_ms != 0 && now - st.logged_ms < 10000u))
            return;
        st.logged_ms = now ? now : 1u;

        ObjectGuid const v = s.victim();
        NearbyUnit const* u = v.IsEmpty() ? nullptr : s.hostile_unit(v);
        float dist = -1.f;
        if (u)
        {
            float bx, by, bz; s.position(bx, by, bz);
            float const dx = u->x - bx, dy = u->y - by, dz = u->z - bz;
            dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        }
        bool const has_mana = s.max_power(0) > 0;
        char const* derived =
            v.IsEmpty()                     ? "no target" :
            !u                              ? "target not in snapshot" :
            !u->in_los                      ? "out of LoS" :
            melee && dist > 5.f + u->combat_reach ? "not in melee range" :
            dist > 32.f                     ? "target out of range" :
            s.is_moving()                   ? "moving" :
            has_mana && s.power_pct(0) < 10 ? "OOM" :
                                              "no rule acted";
        char const* rule = ai.rule_fire_seq() != rules_before ? ai.last_rule_fired() : nullptr;
        TC_LOG_INFO("playerbot.v2", "[dps_idle_reason] {} not attacking because {}{}{} ({:.1f}s) target={} dist={:.1f} los={} moving={} mana={}",
            s.name(), rule ? rule : derived, rule ? " / " : "", rule ? derived : "", idle_ms / 1000.f,
            v.IsEmpty() ? std::string("none") : (u ? std::to_string(u->entry) + "/" : std::string()) + std::to_string(v.GetCounter()),
            dist, u ? int(u->in_los) : 0, int(s.is_moving()), has_mana ? s.power_pct(0) : -1);
    }
};

// Combat re-facing (Playerbot.Behavior.ReFace). A swing needs the victim within +-60 degrees
// (Unit::DoMeleeAttackIfReady -> BadFacing) and a facing cast within +-90; a bot has no client to
// turn it and ChaseMovementGenerator only faces on arrival, so a victim that moved, a target swap
// or a raid / hazard / converge move leaves the bot with its back to the fight (live MC 2026-10-07).
// A standing bot with a victim off-bearing emits face_victim at most every 250 ms, and at once on
// the first tick after a move ended. A moving, casting or stunned bot is never turned (kiting,
// hazard steps, channels stay intact); the core re-checks all of that against the live bot.
// [facing] log: bot off-bearing > 60 degrees with a victim, change-detected, once per 2 s per bot.
struct FacingClock { uint32 last_emit_ms = 0; uint32 last_log_ms = 0; bool was_moving = false; };
std::mutex g_facing_mu;
std::unordered_map<uint64, FacingClock> g_facing;

void ReFaceVictim(BotSnapshotView const& s, BotIntentEmitter& emit)
{
    if (!Services::Config().behavior_re_face()) return;
    ObjectGuid const v = s.victim();
    if (v.IsEmpty()) return;
    NearbyUnit const* vu = s.victim_info();
    if (!vu || vu->hp <= 0) return;
    float bx, by, bz; s.position(bx, by, bz);
    float const dx = vu->x - bx, dy = vu->y - by;
    float const dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.0f || dist > 45.0f) return;
    float d = s.raw().position.o - std::atan2(dy, dx);
    constexpr float kPi = 3.14159265f;
    while (d >  kPi) d -= 2.0f * kPi;
    while (d < -kPi) d += 2.0f * kPi;
    float const off_deg = std::fabs(d) * (180.0f / kPi);
    bool const melee = IsMeleeSpec(s.cls(), s.spec());
    bool const moving = s.is_moving();
    uint32 const now = s.published_at_ms();

    char const* action = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_facing_mu);
        FacingClock& c = g_facing[s.guid().GetCounter()];
        bool const just_stopped = c.was_moving && !moving;
        c.was_moving = moving;
        bool const off = off_deg > (melee ? 35.0f : 50.0f);
        if (off)
        {
            // A move flag on the snapshot is no proof the bot walks: a flag left behind a finished spline (the core clears it
            // when it answers) or a bot wobbling on the spot reads as moving and was never turned (MC Garr 2026-10-10: 874 of
            // 1042 [facing] lines "skipped: moving", a melee bot at 4 y with its back to the boss for minutes). The request goes
            // out at the usual pace; the core refuses a bot that really walks (Locked), and a walker gets the same request
            // again within a quarter second of stopping.
            if (moving && now - c.last_emit_ms < 500u) action = nullptr;
            else if (moving && emit.face_victim(v)) { c.last_emit_ms = now; action = "refaced while moving (core decides)"; }
            else if (moving)                  action = "skipped: emit refused while moving";
            else if (s.is_casting())          action = "skipped: casting";
            else if (s.is_stunned())          action = "skipped: controlled";
            else if (!just_stopped && now - c.last_emit_ms < 250u) action = nullptr;
            else if (emit.face_victim(v)) { c.last_emit_ms = now; action = "refaced"; }
            else                              action = "skipped: emit refused";
        }
        if (action && off_deg > 60.0f && now - c.last_log_ms >= 2000u)
        {
            c.last_log_ms = now;
            TC_LOG_INFO("playerbot.v2.facing", "[facing] {} victim={} off={:.0f}deg dist={:.1f} moving={} melee={} action={}",
                s.name(), vu->entry, off_deg, dist, int(moving), int(melee), action);
        }
    }
}

// ---- Solo low-HP run-away (combat:solo_flee, Playerbot.Behavior.SoloFlee) ----
// A solo open-world bot (no group, no owner, not in an instance / BG, not mounted / stunned / rooted) that is
// losing a fight to mobs that are not about to die: it uses its class escape tools (Feign Death / Disengage,
// Frost Nova + Blink, Psychic Scream / Fade, Death Coil, Vanish / Sprint, Intimidating Shout, Barkskin, Hammer of
// Justice; a paladin Lay on Hands heals instead of fleeing), then runs from the attackers toward the nearest
// friendly town NPC or quest hub that is not behind them (else straight away from them). The flee is committed for
// at least 3 s and ends once no attacker is within 25 y for 4 s (or after 30 s); then a 20 s cooldown applies, and
// the rest (ooc eat / drink / bandage) takes over once combat drops. A bot already using a survival item this tick
// waits for it. The mobs leash / evade on their own. Healers with mana and a single mob keep healing down to 15%.
struct SoloFleeClock
{
    bool   active = false;
    bool   has_dest = false;
    uint32 start_ms = 0, calm_since_ms = 0, last_end_ms = 0, last_tool_ms = 0, last_seen_ms = 0;
    uint8  tools = 0;
    float  dx = 0.f, dy = 0.f, dz = 0.f;
};
std::mutex g_solo_flee_mu;
std::unordered_map<uint64, SoloFleeClock> g_solo_flee;

bool SoloFleeTick(BotAI& ai, BotSnapshotView const& s, GroupSnapshotView const& g, BotIntentEmitter& emit,
                  bool survival_item_used)
{
    if (!Services::Config().behavior_solo_flee()) return false;
    uint32 const now = s.published_at_ms();
    uint64 const key = s.guid().GetCounter();
    auto drop_state = [&](bool was_active)
    {
        std::lock_guard<std::mutex> lock(g_solo_flee_mu);
        auto it = g_solo_flee.find(key);
        if (it == g_solo_flee.end()) return;
        if (was_active && it->second.active) { it->second.active = false; it->second.last_end_ms = now; }
    };

    bool const gated = !s.is_alive() || g.exists() || s.in_group() || s.has_owner_character() || s.is_companion_with_owner() ||
                       s.is_in_instance() || s.in_battleground() || s.is_bg_orphan() || s.is_mounted() ||
                       s.is_stunned() || s.is_rooted();
    if (gated) { drop_state(true); return false; }

    // Live non-player attackers (a player attacker is the PvP rules' business).
    float bx, by, bz; s.position(bx, by, bz);
    NearbyUnit const* nearest = nullptr;
    float nearest_d2 = 0.f, cx = 0.f, cy = 0.f, near25_d2 = 25.f * 25.f;
    uint32 n = 0, n_melee = 0;
    int32 max_hp_pct = 0;
    bool any_player = false, any_near25 = false;
    for (NearbyUnit const& u : s.attackers())
    {
        if (u.hp <= 0) continue;
        if (u.is_player) { any_player = true; continue; }
        float const ddx = u.x - bx, ddy = u.y - by, d2 = ddx * ddx + ddy * ddy;
        ++n; cx += u.x; cy += u.y;
        if (d2 <= 9.f * 9.f) ++n_melee;
        if (d2 <= near25_d2) any_near25 = true;
        if (!nearest || d2 < nearest_d2) { nearest = &u; nearest_d2 = d2; }
        int32 const p = u.max_hp > 0 ? int32((int64(u.hp) * 100) / u.max_hp) : 100;
        max_hp_pct = std::max(max_hp_pct, p);
    }
    for (NearbyUnit const& u : s.nearby_enemies())
    {
        if (u.hp <= 0 || u.is_player || u.victim != s.guid()) continue;
        float const ddx = u.x - bx, ddy = u.y - by;
        if (ddx * ddx + ddy * ddy <= near25_d2) any_near25 = true;
    }
    if (any_player) { drop_state(true); return false; }

    SoloFleeClock snap;
    {
        std::lock_guard<std::mutex> lock(g_solo_flee_mu);
        if (g_solo_flee.size() > 1024)
            for (auto it = g_solo_flee.begin(); it != g_solo_flee.end();)
                it = (!it->second.active && now - it->second.last_seen_ms > 300'000u) ? g_solo_flee.erase(it) : std::next(it);
        SoloFleeClock& c = g_solo_flee[key];
        c.last_seen_ms = now;
        snap = c;
    }

    // ---- Trigger ----
    if (!snap.active)
    {
        if (n == 0 || survival_item_used) return false;
        if (snap.last_end_ms != 0 && now - snap.last_end_ms < 20'000u) return false;
        int32 thr;
        switch (s.cls())
        {
            case CLASS_MAGE: case CLASS_PRIEST: case CLASS_WARLOCK: case CLASS_HUNTER: case CLASS_ROGUE: thr = 35; break;
            case CLASS_WARRIOR: case CLASS_PALADIN: thr = 25; break;
            default: thr = 30; break;
        }
        bool const heals = (s.cls() == CLASS_PRIEST || s.cls() == CLASS_PALADIN || s.cls() == CLASS_DRUID ||
                            s.cls() == CLASS_SHAMAN) && s.power_pct(POWER_MANA) >= 20 && n == 1;
        if (heals) thr = std::min(thr, 15);
        if (s.hp_pct() > thr) return false;
        if (max_hp_pct <= 25) return false;   // it is about to die: finish it
        // A paladin's Lay on Hands is the survival tool: heal to full and fight on.
        if (s.cls() == CLASS_PALADIN && s.knows_spell(633) && s.is_ready(633) && s.can_cast() && emit.cast(633, s.guid()))
        {
            ai.set_last_rule_fired("combat:solo_flee_lay_on_hands");
            return true;
        }
        if (s.is_casting() && s.raw().cast.current_cast_remaining.count() > 0 && s.hp_pct() > 10) return false;
    }

    // ---- Commit / end ----
    bool calm_done = false;
    if (snap.active)
    {
        uint32 const ran = now - snap.start_ms;
        if (any_near25) snap.calm_since_ms = 0;
        else if (snap.calm_since_ms == 0) snap.calm_since_ms = now ? now : 1u;
        calm_done = ran >= 3000u && snap.calm_since_ms != 0 && now - snap.calm_since_ms >= 4000u;
        if (ran >= 30'000u) calm_done = true;
        if (calm_done)
        {
            {
                std::lock_guard<std::mutex> lock(g_solo_flee_mu);
                SoloFleeClock& c = g_solo_flee[key];
                c.active = false; c.last_end_ms = now;
            }
            TC_LOG_INFO("playerbot.v2", "[solo_flee] {} ends after {:.1f}s hp={}% attackers_near={}",
                s.name(), ran / 1000.f, s.hp_pct(), int(any_near25));
            return false;
        }
    }
    else
    {
        snap.active = true; snap.start_ms = now ? now : 1u; snap.calm_since_ms = 0;
        snap.tools = 0; snap.last_tool_ms = 0; snap.has_dest = false;
        TC_LOG_INFO("playerbot.v2", "[solo_flee] {} runs: hp={}% attackers={} (melee {}) victim_hp<={}%",
            s.name(), s.hp_pct(), n, n_melee, max_hp_pct);
        if (!s.victim().IsEmpty()) ai.note_engage(s.victim(), now, 60'000u);
        if (!s.pet_guid().IsEmpty()) emit.pet_set_command_state(1 /*COMMAND_FOLLOW*/);
    }

    // Feign Death in effect: lie still, moving breaks it.
    if (s.cls() == CLASS_HUNTER && s.has_aura(5384))
    {
        ai.set_last_rule_fired("combat:solo_flee_feign");
        std::lock_guard<std::mutex> lock(g_solo_flee_mu);
        g_solo_flee[key] = snap;
        return true;
    }

    // ---- Escape tool (one per 1.2 s, three per run) ----
    bool cast_this_tick = false;
    if (n > 0 && s.can_cast() && snap.tools < 3 && (snap.last_tool_ms == 0 || now - snap.last_tool_ms >= 1200u))
    {
        struct Tool { uint8 cls; uint32 spell; float near_yd; bool on_nearest; bool moving; uint32 need_n; uint32 skip_aura; };
        static constexpr Tool kTools[] = {
            { CLASS_HUNTER,  5384, 0.f,  false, false, 1, 0    },   // Feign Death
            { CLASS_HUNTER,   781, 8.f,  false, false, 1, 0    },   // Disengage
            { CLASS_MAGE,     122, 9.f,  false, false, 1, 0    },   // Frost Nova
            { CLASS_MAGE,    1953, 12.f, false, true,  1, 0    },   // Blink (facing = run heading)
            { CLASS_PRIEST,    17, 0.f,  false, false, 1, 6788 },   // Power Word: Shield (not while Weakened Soul)
            { CLASS_PRIEST,  8122, 8.f,  false, false, 1, 0    },   // Psychic Scream
            { CLASS_PRIEST,   586, 0.f,  false, false, 1, 0    },   // Fade
            { CLASS_WARLOCK, 6789, 28.f, true,  false, 1, 0    },   // Death Coil
            { CLASS_WARLOCK, 5484, 9.f,  false, false, 2, 0    },   // Howl of Terror
            { CLASS_ROGUE,   1856, 0.f,  false, false, 1, 0    },   // Vanish
            { CLASS_ROGUE,   2983, 0.f,  false, false, 1, 0    },   // Sprint
            { CLASS_WARRIOR, 5246, 9.f,  false, false, 1, 0    },   // Intimidating Shout
            { CLASS_DRUID,  22812, 0.f,  false, false, 1, 0    },   // Barkskin
            { CLASS_DRUID,   1850, 0.f,  false, false, 1, 0    },   // Dash (cat form only, below)
            { CLASS_PALADIN,  853, 10.f, true,  false, 1, 0    },   // Hammer of Justice
        };
        for (Tool const& t : kTools)
        {
            if (t.cls != s.cls() || !s.knows_spell(t.spell) || !s.is_ready(t.spell)) continue;
            if (t.skip_aura && s.has_aura(t.skip_aura)) continue;
            if (t.spell == 1850 && !s.has_aura(768)) continue;
            if (t.moving && !(s.is_moving() && now - snap.start_ms >= 800u)) continue;
            if (t.near_yd > 0.f && !(nearest && nearest_d2 <= t.near_yd * t.near_yd)) continue;
            if (n < t.need_n) continue;
            if (t.on_nearest && !(nearest && nearest_d2 <= t.near_yd * t.near_yd)) continue;
            if (!emit.cast(t.spell, t.on_nearest ? nearest->guid : ObjectGuid::Empty)) continue;
            snap.tools++; snap.last_tool_ms = now;
            cast_this_tick = true;
            TC_LOG_INFO("playerbot.v2", "[solo_flee] {} escape tool spell {} (hp={}%)", s.name(), t.spell, s.hp_pct());
            break;
        }
    }

    // ---- Run ----
    // Away vector from the attackers' centroid; a friendly town NPC / quest hub not behind the bot wins.
    if (!snap.has_dest || (snap.dx - bx) * (snap.dx - bx) + (snap.dy - by) * (snap.dy - by) < 3.f * 3.f)
    {
        float ax = 0.f, ay = 1.f;
        if (n > 0)
        {
            ax = bx - cx / n; ay = by - cy / n;
            float const al = std::sqrt(ax * ax + ay * ay);
            if (al > 0.01f) { ax /= al; ay /= al; } else { ax = 0.f; ay = 1.f; }
        }
        else
        {
            // Nothing in the attackers list: keep the old heading.
            if (snap.has_dest)
            {
                ax = snap.dx - bx; ay = snap.dy - by;
                float const al = std::sqrt(ax * ax + ay * ay);
                if (al > 0.01f) { ax /= al; ay /= al; }
            }
        }
        float tx = bx + ax * 45.f, ty = by + ay * 45.f, tz = bz;
        char const* kind = "away";
        auto ahead = [&](float x, float y) {
            float const vx = x - bx, vy = y - by, vl = std::sqrt(vx * vx + vy * vy);
            return vl > 8.f && (vx * ax + vy * ay) / vl >= 0.25f;
        };
        constexpr uint32 kTownMask = uint32(UNIT_NPC_FLAG_VENDOR) | uint32(UNIT_NPC_FLAG_FLIGHTMASTER) |
                                     uint32(UNIT_NPC_FLAG_INNKEEPER) | uint32(UNIT_NPC_FLAG_REPAIR);
        bool found = false;
        for (NearbyUnit const& f : s.raw().combat.nearby_friends)
        {
            if ((f.npc_flags & kTownMask) == 0 || f.hp <= 0) continue;
            float const fx = f.x - bx, fy = f.y - by;
            if (fx * fx + fy * fy > 150.f * 150.f || !ahead(f.x, f.y)) continue;
            tx = f.x; ty = f.y; tz = f.z; kind = "town_npc"; found = true; break;
        }
        if (!found)
            if (auto const* hub = Services::Hubs().GetNearestQuestHub(s.raw()))
                if (hub->mapId == s.map_id())
                {
                    float const hx = hub->location.GetPositionX(), hy = hub->location.GetPositionY();
                    float const fx = hx - bx, fy = hy - by;
                    if (fx * fx + fy * fy <= 250.f * 250.f && ahead(hx, hy))
                    { tx = hx; ty = hy; tz = hub->location.GetPositionZ(); kind = "hub"; }
                }
        snap.dx = tx; snap.dy = ty; snap.dz = tz; snap.has_dest = true;
        TC_LOG_INFO("playerbot.v2", "[solo_flee] {} heads {} ({:.0f},{:.0f})", s.name(), kind, tx, ty);
    }
    if (!s.victim().IsEmpty()) emit.stop_attack();
    emit.move_to(snap.dx, snap.dy, snap.dz, /*run*/ true);
    ai.set_last_rule_fired(cast_this_tick ? "combat:solo_flee_tool" : "combat:solo_flee");
    {
        std::lock_guard<std::mutex> lock(g_solo_flee_mu);
        g_solo_flee[key] = snap;
    }
    return true;
}

} // anonymous

bool HeldByOwner(BotAI const& ai, BotSnapshotView const& s)
{
    if (!ai.holding_position(s.published_at_ms())) return false;
    for (NearbyUnit const& at : s.raw().combat.attackers)
        if (at.hp > 0 && at.victim == s.guid()) return false;
    return true;
}

bool ThreatHoldActive(BotAI& ai, BotSnapshotView const& s, ObjectGuid target,
                      GroupSnapshotView const* g)
{
    return EvaluateThreatHold(ai, s, target, g) != ThreatHold::None;
}

// Playerbot.Behavior.SpreadOut: see MaintainHelpers.h.
bool MeleeSpreadSpot(BotSnapshotView const& s, GroupSnapshotView const& g, NearbyUnit const& v, float& ox, float& oy)
{
    if (!Services::Config().behavior_spread_out() || v.hp <= 0) return false;
    uint32 n = 1, k = 0;
    if (auto const* mems = g.members())
        for (auto const& m : *mems)
        {
            if (m.guid == s.guid() || !m.online || !m.is_alive || m.map_id != s.map_id()) continue;
            if (m.victim != v.guid || m.role == Role::Tank || m.role == Role::Healer) continue;
            if (!IsMeleeSpec(m.cls, m.spec)) continue;
            ++n;
            if (m.guid < s.guid()) ++k;
        }
    // Inside melee reach (reach + ours + 4/3, never under 5): outer rim, jittered 0.5-2 y inward.
    const float outer = std::clamp(v.combat_reach + 2.5f, 3.0f, 6.0f);
    for (uint32 i = 0; i < 4; ++i)
    {
        const uint32 rank = (k + i * 3) % n;   // a blocked slot tries the one three along
        float x, y;
        // Raid (playtest 2026-10-12): melee stack on the boss's back, nearly on top of each other, unless a boss rule spreads them. A 70 degree
        // arc instead of the 160 degree one of a dungeon pack.
        if (s.is_in_raid())
            MeleeArcSpot(s.guid().GetCounter(), rank, n, v.x, v.y, v.o, 1.0f, outer, x, y, 1.2217f);
        else
            MeleeArcSpot(s.guid().GetCounter(), rank, n, v.x, v.y, v.o, 1.0f, outer, x, y);
        if (RaidStandPointBlocked(s, x, y, v.z)) continue;
        ox = x; oy = y;
        static std::atomic<uint32> s_last_log{ 0 };
        const uint32 now = s.published_at_ms();
        uint32 last = s_last_log.load(std::memory_order_relaxed);
        if (now - last >= 5000u && s_last_log.compare_exchange_strong(last, now))
            TC_LOG_INFO("playerbot.v2", "[spread] {} melee victim={} slot={}/{} spot=({:.1f},{:.1f}) r={:.1f} (every 5s, one bot)",
                s.name(), v.entry, rank, n, x, y, std::sqrt((x - v.x) * (x - v.x) + (y - v.y) * (y - v.y)));
        return true;
    }
    return false;
}

void DispatchInCombat(BotAI& ai,
                      BotSnapshotView snapshot,
                      GroupSnapshotView group,
                      BotIntentEmitter& emit)
{
    // Reports a damage bot that ends this tick with nothing to do ([dps_idle_reason]).
    CombatIdleProbe const idle_probe{ ai, snapshot, emit, emit.combat_action_count(), ai.rule_fire_seq() };
    ReFaceVictim(snapshot, emit);
    ApPredicateContext ctx{ snapshot, group, ai.aoe_preference(), {} };
    // PvP context — populated for BG/arena ticks. Default-constructed is
    // PvE-mode (all false / empty); PvP-aware predicates gate on
    // ctx.pvp.in_battleground first. Sourcing fields from the snapshot
    // is cheap and per-tick — APL evaluation runs once per combat tick.
    ctx.pvp.in_battleground       = snapshot.in_battleground();
    // in_arena left default false — no direct accessor; arena scripts
    // can branch on bg_type_id ∈ {4,5,6,8,10,11,719,757,808,816,...}
    // if needed. Most PvP-aware predicates don't distinguish arena
    // from BG.
    ctx.pvp.friendly_flag_carrier = snapshot.bg_friendly_flag_carrier();
    ctx.pvp.enemy_flag_carrier    = snapshot.bg_enemy_flag_carrier();
    ctx.pvp.under_player_attack   = snapshot.under_player_attack();
    // bg_role from the snapshot (BG audit N57): the builder publishes the
    // same rank-permutation + fixup role BgDispatch resolves, so the
    // combat APL is no longer role-blind (healer-spec rules can gate on
    // Healer role, FC openers on FlagCarrier, etc.).
    ctx.pvp.bg_role = snapshot.raw().bg.bg_role;

    // BG-orphan disengage. When a battleground ends, the core removes every
    // player and teleports them out — but the exit teleport can fail
    // transiently (player loading / mid-teleport) and the Battleground
    // object is deleted regardless. The bots left behind kept fighting
    // each other FOREVER on the dead map (live 2026-06-11: 3 bots, 15+ min
    // of StartAttack every tick on post-match map 566), and because they
    // never left combat, no idle recovery could reach them. Detect the
    // orphan signature (BG/arena map + no Battleground claims us) and stop
    // attacking; once everyone disengages, combat drops and
    // idle:bg_orphan_escape ports the bot home.
    if (snapshot.is_bg_orphan())
    {
        emit.stop_attack();
        ai.set_last_rule_fired("combat:bg_orphan_disengage");
        return;
    }

    // Owner said fall back: the run to the rally is not stopped by combat, and once re-formed
    // the bot fights only what followed (States::FallbackTick, PrologueRules.cpp).
    if (States::FallbackTick(snapshot, ai, group, emit, /*in_combat_state*/ true))
        return;
    // Owner said pull back: the opener, the run to the return point and the hold there (States::PullBackTick).
    if (States::PullBackTick(snapshot, ai, group, emit, /*in_combat_state*/ true))
        return;

    // Owner said flee: run to them and don't fight back until the window ends.
    if (ai.fleeing(snapshot.published_at_ms()))
    {
        // The follow was issued with the order (BotCommandParser "flee"); re-issuing it every tick would restart the
        // follow generator and stutter the run. Only drop a target the bot picked up again.
        if (!snapshot.victim().IsEmpty())
            emit.stop_attack();
        ai.set_last_rule_fired("combat:flee_to_owner");
        return;
    }

    // Dismount FIRST when in combat. Modern TC / WoW 12.x doesn't reliably
    // auto-dismount on damage for all mount types (ground mounts, some
    // legacy mounts, transmogged mounts). A mounted bot can't cast most
    // spells — without dismounting it just sits there while enemies kill
    // it. Fire one dismount per combat-entry; next tick the snapshot
    // reflects is_mounted=false and the rotation runs normally.
    if (snapshot.is_mounted())
    {
        emit.dismount();
        ai.set_last_rule_fired("combat:dismount");
        return;
    }

    // Survival item (healthstone / healing potion at 30% health) goes out
    // FIRST and alongside any move below: using an item neither stops nor
    // needs a stand-still, and the step-out / escape / LoS / healer moves
    // claim every tick of a multi-second walk (their in-flight holds) - a
    // bot at 20% running out of fire or from a loose elite used to get no
    // healthstone until it arrived. It also goes out ahead of a raid move
    // (a bot running out of Inferno still uses its healthstone). Without a
    // move it still consumes the tick at its old place below.
    const bool survival_item_used = MaybeUseSurvivalItem(snapshot, group, emit);

    // The raid's resistance potion goes out the same way: alongside any raid move, not after it. It sat below the raid dispatch, which
    // returns on every tick a bot has a move, a wait or a job, so only the bots the raid left idle ever drank it (Geddon, MC 2026-10-13:
    // 9 of 40 bots drank, no mage did; a mage Living Bomb carrier took the full 3200 and died).
    const bool resist_potion_used = !survival_item_used && MaybeUseResistPotion(snapshot, emit);

    // Raid encounter orders (Bot/Raid): hazard moves, taunt swaps, dispels,
    // purges, crowd control and forbidden targets come before everything else.
    if (Raid::DispatchRaidOrder(ai, snapshot, emit))
        return;

    // ---- Ghost-combat self-heal ----
    // A wedged combat flag: in combat 10s+ with NO victim, NO attackers,
    // not casting, and no nearby enemy fighting the group. TC's threat
    // bookkeeping can strand a combat ref against an unreachable/evaded
    // mob (2026-06-12 Stockades: healer InCombat=true for 30+ min at full
    // HP/mana, victim empty, attackers 0 — the tank's member-in-combat
    // advance gate held the entire run hostage until a GM combatstop).
    // The legitimate "healer in combat with no victim" case is excluded
    // by the enemy-fighting-the-group scan: real fights have mobs whose
    // victim is a member (or the bot's pet). clear_ghost_combat drops the
    // PvE refs server-side, guarded there on an empty attacker list.
    if (snapshot.victim().IsEmpty() &&
        snapshot.raw().combat.attackers.empty() &&
        !snapshot.is_casting() &&
        snapshot.combat_duration_ms() >= 10'000)
    {
        bool group_engaged = false;
        const ObjectGuid mypet = snapshot.pet_guid();
        for (auto const& u : snapshot.raw().combat.nearby_enemies)
        {
            if (u.victim.IsEmpty() || u.hp <= 0) continue;
            if (u.victim == snapshot.raw().guid ||
                (!mypet.IsEmpty() && u.victim == mypet))
            { group_engaged = true; break; }
            if (group.exists())
                if (auto const* mems = group.members())
                    for (auto const& m : *mems)
                        if (m.guid == u.victim) { group_engaged = true; break; }
            if (group_engaged) break;
        }
        if (!group_engaged)
        {
            emit.stop_attack(/*clear_ghost_combat*/ true);
            ai.set_last_rule_fired("combat:ghost_combat_clear");
            return;
        }
    }

    // ---- Critical-durability flee to repair (soft-lock break) ----
    // Gear at <=20% durability does almost no weapon damage, so the bot can
    // NEITHER kill nor die quickly — it churns in one combat forever (observed:
    // Bramwell, 155s+ on quest 26389 at 0% durability, never progressing, never
    // OOC to repair). The per-victim disengages below are defeated by target-
    // switching within a pack (their victim_watch resets on each switch) AND are
    // gated on a resolved victim/quest-target; this one keys off total combat
    // DURATION and fires UNCONDITIONALLY at top level (no victim dependency),
    // EVEN on quest targets — a kill the bot physically cannot land isn't worth
    // protecting. Flee TOWARD a known repair vendor so the pack leashes (combat
    // drops) and the trip makes progress; idle:critical_repair takes over once
    // OOC. Gated out in instance/BG/group (coordinated fights recover
    // differently). Routine <30%/<35% repairs stay on the OOC vendor pipeline.
    // Trigger on EITHER a long unwinnable CHURN (>60s — the original Bramwell case,
    // where 0% gear can neither kill nor die) OR a LOW-HP losing fight (<=40% HP —
    // the FAST-DEATH case). A broken-gear bot that is being killed in well under 60s
    // never reached the duration gate, so it fell through to a pure DEATH SPIRAL:
    // die -> resurrect -> re-engage with 0% gear -> die, never OOC long enough for
    // idle:critical_repair to run (observed: Zekani, L3 Troll Hunter on Echo Isles,
    // repeated [release_corpse] with zero repair attempts). Fleeing at 40% HP gives
    // the bot a chance to leash the pack and reach a repair target BEFORE dying.
    {
        auto const& v = snapshot.raw().vitals;
        const bool low_hp = v.max_hp > 0 && uint64(v.hp) * 100u <= uint64(v.max_hp) * 40u;
        const uint8 dura = snapshot.lowest_equipped_durability_pct();
        // A bot whose gear is DESTROYED (≤5%) deals essentially no weapon damage and
        // cannot win ANY fight — yet at full HP it also never dies, so the >60s-churn
        // and <40%-HP triggers below can both miss (combat_duration resets when the
        // unkillable mob's pack switches targets). That leaves it auto-attacking a mob
        // it can't kill, forever, never breaking off to repair (observed: Morthan, L9,
        // 0% gear, looping "Auto attack" at 100% HP in the Ruins of Lordaeron). When
        // gear is destroyed, fleeing to repair is ALWAYS correct — fire unconditionally.
        if (!snapshot.is_in_instance() && !snapshot.in_battleground() &&
            !group.exists() &&
            dura <= 20 &&
            (dura <= 5 || snapshot.combat_duration_ms() > 60000 || low_hp))
        {
            // Prefer a known repair vendor; fall back to the nearest SAME-MAP quest
            // hub (hubs carry repair vendors). The hub fallback also doubles as the
            // starter-island ESCAPE: a bot stranded on a no-known-vendor starting
            // island (Echo Isles / Ammen Vale) flees toward its next hub (e.g.
            // Sen'jin), which is exactly where it must relocate to continue questing
            // AND find repair — without it, CriticalRepairGate's same-map-hub branch
            // is the only repair path and it never runs because the death loop
            // starves State_Idle. Mirrors the idle gate's vendor/hub fallback chain.
            float tx = 0.f, ty = 0.f; bool have_target = false;
            if (auto hit = Services::RepairVendors().GetNearestRepairVendor(snapshot.raw()))
            {
                tx = hit->x; ty = hit->y; have_target = true;
            }
            else if (auto const* hub = Services::Hubs().GetNearestQuestHub(snapshot.raw()))
            {
                if (hub->mapId == snapshot.map_id())
                {
                    tx = hub->location.GetPositionX();
                    ty = hub->location.GetPositionY();
                    have_target = true;
                }
            }
            if (have_target)
            {
                emit.stop_attack();
                // Call the PET off too. A pet still locked on a target keeps the
                // OWNER flagged in-combat (PetAI threat linkage), so a 0%-gear bot
                // whose pet keeps fighting never goes OOC and never reaches the
                // vendor — it just death-spirals (observed: Morthan, Imp "Azlop"
                // combat=true, owner InCombat 54min at full-then-dropping HP with
                // EMPTY victim / 0 attackers, ghost_combat_clear skipped because the
                // pet's target reads as group-engaged). FOLLOW (AttackStop + recall)
                // + PASSIVE (don't re-engage) drops the pet's combat so the owner's
                // clears and the flee/repair can actually proceed.
                if (!snapshot.pet_guid().IsEmpty())
                {
                    emit.pet_set_react_state(0 /*REACT_PASSIVE*/);
                    emit.pet_set_command_state(1 /*COMMAND_FOLLOW*/);
                }
                float bx, by, bz; snapshot.position(bx, by, bz);
                // LEASH-BREAK OVERRIDE for a PROLONGED stalemate. When combat has
                // already run >60s the flee-toward-vendor is demonstrably NOT
                // breaking it — typically because the chosen repair target sits
                // PAST the mob pack (so fleeing toward it re-aggros) or the nearest-
                // vendor pick oscillates between far candidates each tick, leaving
                // the bot jittering in place (observed: Morthan, 0% gear, 14-MIN
                // continuous combat at the Ruins of Lordaeron — advancing paths in
                // OPPOSITE directions every tick, net-zero movement, never OOC).
                // A 0%-gear bot at 100% HP can neither kill nor die, so it churns
                // forever. Flee a STABLE direction directly AWAY from the enemy
                // centroid (deterministic, never toward the pack) to break the leash;
                // once OOC, idle:critical_repair routes to the vendor. The normal
                // (short-combat) case still flees toward the vendor/hub below.
                float fx = tx, fy = ty;
                if (snapshot.combat_duration_ms() > 60000)
                {
                    double cx = 0.0, cy = 0.0; uint32 n = 0;
                    for (auto const& e : snapshot.raw().combat.nearby_enemies)
                    {
                        if (e.hp <= 0) continue;
                        cx += e.x; cy += e.y; ++n;
                    }
                    if (n > 0)
                    {
                        cx /= n; cy /= n;
                        float ax = bx - float(cx), ay = by - float(cy);
                        const float al = std::sqrt(std::max(ax*ax + ay*ay, 0.0001f));
                        constexpr float kBreak = 60.0f;   // clear typical leash range
                        fx = bx + ax / al * kBreak;
                        fy = by + ay / al * kBreak;
                    }
                }
                // Full-path so PathGenerator routes around geometry and the bot
                // leashes the pack as it gains distance (idle:critical_repair, also
                // full-path, finishes the trip once OOC). Replaces the old 15y greedy
                // hop that wedged on terrain / ran the bot into the very pack it fled.
                emit.move_to(fx, fy, bz, /*run*/ true);
                ai.set_last_rule_fired("combat:flee_to_repair");
                return;
            }
        }
    }

    // Solo low-HP run-away (escape tools, then a run from the attackers; SoloFleeTick above).
    if (SoloFleeTick(ai, snapshot, group, emit, survival_item_used))
        return;

    // ---- Ground-hazard step-out ([raid_hazard], State_Idle.cpp) ----
    // A non-tank standing in a hostile damaging ground zone (Rain of Fire,
    // poison cloud, void zone) leaves it before anything below moves it
    // (LoS reposition, loose-mob escape, healer follow / gap-close, the melee
    // gap-close) or roots it in a cast. Instances only; also run from
    // DungeonCombatPositioning further down (a no-op the second time).
    if (snapshot.is_in_instance() && !snapshot.in_battleground() &&
        RaidHazardStepOut(snapshot, ai, group, emit))
        return;

    // ---- Zero-yield combat watchdog ----
    // (a) No-XP victim (Training Dummy etc.): the target is attackable but
    //     never dies and awards nothing — a bot that opens on one stays
    //     InCombat FOREVER, suppressing all questing/travel (observed live:
    //     L22 + L34 bots wedged on Razor Hill dummies for hours). Disengage
    //     immediately unless it is the bot's actual quest target (a few
    //     quests require hitting no-XP props).
    // (b) No-progress: same victim, HP not dropping for 45s outside
    //     instances (evade-wedged mob, perma-immune prop) — disengage.
    //     Instances excluded: scripted immune phases are normal there.
    {
        const ObjectGuid vg = snapshot.victim();
        if (!vg.IsEmpty())
        {
            NearbyUnit const* vu = nullptr;
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
                if (u.guid == vg) { vu = &u; break; }
            // ---- Invalid / unscannable victim (vu == null) ----
            // The bot is "attacking" a victim that is NOT in nearby_enemies. The
            // hostile scan only lists units that pass Player::IsValidAttackTarget
            // (BotSnapshotBuilder), so a victim missing from it is one we can deal
            // no real damage to: an immune / not-at-war / pacified prop such as a
            // Training Dummy (entry 44820 — its PACIFIED/NO_XP data is wrong, so
            // the flag filters never caught it, AND it never enters nearby_enemies,
            // so EVERY vu-based disengage below was silently skipped and the bot
            // wedged InCombat forever firing at a target it cannot kill). A genuine
            // elite / rare / world boss we can validly fight DOES appear in
            // nearby_enemies (vu != null), so this branch can never abandon a real
            // kill. Disengage once we've been stuck on the same unscannable,
            // non-quest creature for a short window, with a long engage shield so
            // the pickers don't re-acquire it. Quest target matched by the entry
            // encoded in the victim GUID (no template lookup needed off-thread).
            if (!vu && vg.IsCreature() && !snapshot.is_in_instance() && !group.exists())
            {
                const uint32 now_ms_iv = snapshot.published_at_ms();
                bool qt_iv = false;
                if (snapshot.has_current_objective())
                {
                    auto const& obj = snapshot.current_objective();
                    if (obj.type == /*MONSTER*/ 0)
                    {
                        if (uint32(obj.object_id) == vg.GetEntry()) qt_iv = true;
                        for (uint32 a : obj.credit_alias_entries)
                            if (a == vg.GetEntry()) { qt_iv = true; break; }
                    }
                }
                if (!qt_iv)
                {
                    constexpr uint32 kInvalidVictimMs = 20000u;   // 20s is ample — nothing valid stays unscannable
                    if (ai.combat_stuck_victim() != vg)
                        ai.set_combat_stuck(vg, 0, now_ms_iv);
                    else if (now_ms_iv - ai.combat_stuck_since_ms() > kInvalidVictimMs)
                    {
                        emit.stop_attack();
                        ai.note_engage(vg, now_ms_iv, /*shield_ms*/ 5u * 60u * 1000u);
                        ai.set_combat_stuck(ObjectGuid::Empty, 0, 0);
                        ai.set_last_rule_fired("combat:disengage_invalid_victim");
                        return;
                    }
                }
            }
            if (vu)
            {
                const uint32 now_ms = snapshot.published_at_ms();
                bool quest_target = false;
                if (snapshot.has_current_objective())
                {
                    auto const& obj = snapshot.current_objective();
                    if (obj.type == /*MONSTER*/ 0)
                    {
                        if (uint32(obj.object_id) == vu->entry) quest_target = true;
                        for (uint32 a : obj.credit_alias_entries)
                            if (a == vu->entry) { quest_target = true; break; }
                    }
                }
                // In an instance a no-XP creature that is fighting someone (a victim, not pacified) is a real enemy that merely gives
                // no experience: abandoning it leaves the bot idle under its blows. Only props and dummies are dropped.
                // A boss reads pacified while one of its own spells holds it (Baron Geddon's Inferno roots and pacifies him for the
                // channel): that is a pause in the fight, not a prop (MC Geddon 2026-10-11: 632 drops, the raid stood off him).
                const bool fighting_us = snapshot.is_in_instance() &&
                    ((!vu->is_pacified && !vu->victim.IsEmpty()) || vu->is_dungeon_boss || vu->is_boss_rank);
                // A target the owner ordered us onto (Attack / Engage) is never dropped for giving no XP: the order is the reason, and
                // dropping it every tick restarted the rotation (damage_parity 2026-10-11: ~590 drops a leg, white swings and Overpower only).
                const bool ordered = ai.is_manual_mode(now_ms) && ai.owner_command() == BotAI::OwnerCommand::Engage && ai.owner_target() == vg;
                if ((vu->no_xp_kill || vu->is_pacified) && !quest_target && !fighting_us && !ordered)
                {
                    emit.stop_attack();
                    ai.note_engage(vg, now_ms);   // 8s re-engage shield
                    // A group dungeon fight: dropping the dummy / prop must not leave the bot standing idle with real mobs on it
                    // (deadmines_full efeec980: the tank's last rule was this one when a Goblin Craftsman killed him). Turn on the
                    // attacker that is fighting the group instead; an out-of-instance bot has nothing else to pick.
                    if (snapshot.is_in_instance())
                        if (NearbyUnit const* real = snapshot.highest_threat_attacker();
                            real && real->guid != vg && real->hp > 0 && !real->no_xp_kill && !real->is_pacified && !real->untargetable)
                            emit.start_attack(real->guid);
                    TC_LOG_INFO("playerbot.v2", "[disengage] {} drops {} ({}): {}", snapshot.name(), vu->entry,
                                vu->is_pacified ? "pacified" : "no-XP kill", snapshot.is_in_instance() ? "in an instance, not fighting us" : "out of an instance");
                    ai.set_last_rule_fired("combat:disengage_no_xp");
                    return;
                }
                // ---- Unkillable-target leash (flag-independent backstop) ----
                // The flag catch above relies on creature flags, which some
                // Training Dummies carry incorrectly (entry 44820: PACIFIED is on
                // the template's unit_flags but neither the live unit nor the
                // difficulty-merged template exposes it, so every flag read is
                // false). And the HP-not-dropping timer below is defeated by a
                // dummy whose HP oscillates (each landed hit resets it).
                //
                // The robust, flag-independent signal is HP RECOVERY: an immortal
                // dummy (and an evade-leashing mob the bot can't reach) RESETS to
                // full HP — its health jumps back UP while we fight it. A genuine
                // kill, even a slow one against a big HP pool, only ever trends
                // DOWN. So we track the MINIMUM HP seen on this victim and bail
                // only when its HP climbs far back above that floor — a condition
                // that can NEVER fire on a real fight, so it cannot abandon a
                // tough-but-killable mob. As an extra hard safety net we also
                // exempt Elite / RareElite / Rare (world bosses, rares, elite
                // quest mobs) and any group / instance fight outright: those are
                // legitimately long, high-HP, and sometimes self-heal. On a real
                // immortal we disengage with a LONG engage shield so the grind/
                // pull pickers don't re-acquire it, letting the bot resume
                // questing (which walks it out of range and breaks combat).
                if (!snapshot.is_in_instance() && !group.exists() &&
                    !quest_target && !vu->is_elite_or_rare && !(vu->is_dungeon_boss || vu->is_boss_rank))
                {
                    const int32 vmax = vu->max_hp > 0 ? vu->max_hp : 1;
                    // kStuckMs is a HARD time cap on a single NORMAL-mob fight.
                    // A normal mob dies in seconds even for an under-geared bot;
                    // 90s on one means it's immortal/evade-bugged (the Training
                    // Dummy 44820 case — it can't be flagged out because its
                    // PACIFIED/NO_XP data is wrong, and the HP-based detectors are
                    // defeated by its oscillating health). Reset ONLY on victim
                    // change so HP wobble can't keep postponing it. SAFE because
                    // elites/rares/world bosses/dungeon bosses and group fights
                    // are all exempted above — only trash mobs reach here, so this
                    // never abandons a legitimately long, high-HP kill.
                    constexpr uint32 kStuckMs = 90000u;
                    if (ai.combat_stuck_victim() != vg)
                    {
                        ai.set_combat_stuck(vg, vu->hp, now_ms);     // new victim: floor + start time
                    }
                    else if ((now_ms - ai.combat_stuck_since_ms() > kStuckMs) ||
                             (vu->hp >= ai.combat_stuck_hp() + vmax * 30 / 100))
                    {
                        // Either stuck too long on one trash mob, OR its HP
                        // recovered far above its low (reset to full) — immortal /
                        // evade-leashing, can never be killed. Disengage with a
                        // long engage shield so the grind / pull pickers don't
                        // re-acquire it; the bot then resumes questing, which walks
                        // it out of range and breaks combat.
                        emit.stop_attack();
                        ai.note_engage(vg, now_ms, /*shield_ms*/ 5u * 60u * 1000u);
                        ai.set_combat_stuck(ObjectGuid::Empty, 0, 0);
                        ai.set_last_rule_fired("combat:disengage_unkillable");
                        return;
                    }
                    else if (vu->hp < ai.combat_stuck_hp())
                    {
                        // Track a new HP low WITHOUT resetting the start time, so
                        // the time cap keeps counting through the fight.
                        ai.set_combat_stuck(vg, vu->hp, ai.combat_stuck_since_ms());
                    }
                }
                // ---- Instance-safe invulnerable watchdog ----
                // The HP-based watchdog above is gated to !is_in_instance() to
                // protect scripted immune phases (Ragnaros hammer, etc). But
                // encounter-mechanic creatures — fire platters, cosmetic props
                // — are truly invulnerable: they absorb ZERO player damage and
                // stay at 100% HP forever. A boss in an immune phase is NOT at
                // 100% HP; it took damage before the phase began. So "victim at
                // 100% HP for 30 consecutive seconds of attacks" is a safe proxy
                // for "truly invulnerable, never going to die" even in instances.
                // Disengage with a 2-minute shield so the fallback target picker
                // won't re-acquire it for the rest of the encounter.
                // Never a player (a battleground: an enemy player at full health for 30 s is being healed, kited or
                // bubbled, not an invulnerable prop; dropping it would shield the flag carrier for two minutes).
                if (snapshot.is_in_instance() && !vu->is_player && !(vu->is_dungeon_boss || vu->is_boss_rank) && !quest_target)
                {
                    const uint32 now_ms_inv = snapshot.published_at_ms();
                    const bool at_full_hp = vu->max_hp > 0 &&
                        vu->hp >= vu->max_hp - (vu->max_hp / 100);
                    if (at_full_hp)
                    {
                        if (ai.combat_stuck_victim() != vg)
                            ai.set_combat_stuck(vg, vu->hp, now_ms_inv);
                        else if (now_ms_inv - ai.combat_stuck_since_ms() > 30000u)
                        {
                            emit.stop_attack();
                            ai.note_engage(vg, now_ms_inv, /*shield_ms*/ 120u * 1000u);
                            ai.set_combat_stuck(ObjectGuid::Empty, 0, 0);
                            ai.set_last_rule_fired("combat:disengage_invulnerable");
                            return;
                        }
                    }
                    else if (ai.combat_stuck_victim() == vg)
                    {
                        // HP dropped — this is a real kill; reset tracking
                        ai.set_combat_stuck(ObjectGuid::Empty, 0, 0);
                    }
                }
                // ---- Disengage for broken gear (death-spiral break) ----
                // A bot with critically broken gear (≤10% durability — its stats
                // are gutted) that is LOSING a fight against a NON-mandatory
                // target should flee so it can repair, rather than feed the
                // death spiral. "Losing" = own HP trending into the danger band
                // OR an already-armed open-world spiral. Reuses the SAME
                // quest_target check as the no-XP branch (never abandon a
                // mandatory quest kill) and the SAME note_engage cooldown so it
                // can't thrash (8s shield + the engage rules skip the target).
                //
                // GATED OUT in instance / BG / group: never abandon a
                // coordinated fight — a group wipe is recovered by the group, not
                // by one bot fleeing.
                if (!snapshot.is_in_instance() && !snapshot.in_battleground() &&
                    !group.exists() &&
                    snapshot.lowest_equipped_durability_pct() <= 10 &&
                    !quest_target &&
                    (snapshot.hp_pct() <= 35 || ai.consecutive_same_spot_deaths() >= 1) &&
                    ai.last_engage_target() != vg)   // honor the existing shield
                {
                    emit.stop_attack();
                    ai.note_engage(vg, now_ms);   // 8s re-engage shield
                    ai.set_last_rule_fired("combat:disengage_broken_gear");
                    return;
                }
                if (!snapshot.is_in_instance())
                {
                    constexpr uint32 kNoProgressMs = 45000u;
                    if (ai.victim_watch_guid() != vg || vu->hp < ai.victim_watch_hp())
                        ai.set_victim_watch(vg, vu->hp, now_ms);
                    else if (now_ms - ai.victim_watch_since_ms() > kNoProgressMs)
                    {
                        emit.stop_attack();
                        ai.note_engage(vg, now_ms);
                        ai.set_victim_watch(ObjectGuid::Empty, 0, 0);
                        ai.set_last_rule_fired("combat:disengage_no_progress");
                        return;
                    }
                }

                // ---- LOS / range reposition (audit B04/B17) ----
                // 22% of all cast rejections were LINE_OF_SIGHT with ZERO
                // reaction: the snapshot computes per-unit in_los but no
                // combat rule consumed it, so a caster behind a pillar (or
                // a ranged bot whose target leashed beyond ~40y) spammed
                // doomed casts until the mob happened to wander. Step
                // TOWARD the victim when it is out of LOS or beyond casting
                // range — walking at the target breaks both conditions; the
                // melee gap-close handles the final approach as before.
                // (Open world + BATTLEGROUNDS: BG maps count as instances,
                // which silently disabled this for all PvP — ranged bots
                // stood still spamming doomed casts at kited / LOS-broken
                // players (BG audit N40). Dungeon/raid instances stay
                // excluded: hiding behind a pillar is often the MECHANIC
                // there and the dungeon positioning cascade owns movement;
                // their ranged / healers get the formation-aware LoS step
                // RaidCastLosStep below instead of walking at the victim.)
                if (!snapshot.is_in_instance() || snapshot.in_battleground())
                {
                    float bx, by, bz;
                    snapshot.position(bx, by, bz);
                    const float dx = vu->x - bx, dy = vu->y - by, dz = vu->z - bz;
                    const float dsq = dx*dx + dy*dy + dz*dz;
                    // A ranged bot walks in once the victim is beyond ~30y + its reach: the
                    // rule-side gate (BotSnapshotView::in_spell_range) now holds every victim
                    // spell back until the victim is inside the spell's real range, so the old
                    // flat 38y threshold would have left bots 31-38y out standing idle.
                    // Melee specs keep the wide threshold; the gap-close below finishes them.
                    const float reposition_range = IsMeleeSpec(snapshot.cls(), snapshot.spec())
                        ? 38.0f : 30.0f + vu->combat_reach;
                    const bool out_of_range = dsq > reposition_range * reposition_range;
                    if (!vu->in_los || out_of_range)
                    {
                        // Wedge-guard (2026-06-15): stepping toward the victim can
                        // NEVER gain LoS when the target sits behind unreachable
                        // terrain / on a ledge above — the bot then steps into a
                        // wall forever (Somi stuck 180s at one spot on a quest-845
                        // target across a rise). The HP-based no-progress disengage
                        // above can't catch it: the bot never lands a hit to drop
                        // the victim's HP, so its watch timer never matures. When
                        // the reposition move is repeatedly path-blocked, disengage
                        // this victim so the bot re-targets or returns to its
                        // objective; the 8s note_engage shield stops an immediate
                        // re-pick of the same unreachable mob.
                        if (ai.check_anchor_wedge("combat:reposition_los",
                                                  snapshot.path_blocked_count(), now_ms))
                        {
                            emit.stop_attack();
                            ai.note_engage(vg, now_ms);
                            ai.set_victim_watch(ObjectGuid::Empty, 0, 0);
                            ai.set_last_rule_fired("combat:disengage_los_wedge");
                            return;
                        }
                        // Pace the step. A new path goes out at most once per kLosRepathMs, and only when
                        // the victim moved more than kLosRepathMoveYd since the last one, or the bot has
                        // stopped (arrived / path refused). After kLosMaxSteps steps or kLosEpisodeMs
                        // without ever gaining LoS / range, give this victim up: walking at it is not
                        // working, so drop it and let target selection pick another (the 8 s note_engage
                        // shield keeps the same target from being re-picked at once).
                        constexpr uint32 kLosRepathMs   = 1000u;
                        constexpr float  kLosRepathMoveYd = 3.0f;
                        constexpr uint32 kLosMaxSteps   = 6u;
                        constexpr uint32 kLosEpisodeMs  = 8000u;
                        constexpr uint32 kLosEpisodeGapMs = 3000u;   // a quiet gap this long ends an episode
                        auto& lr = ai.los_repath();
                        if (lr.victim != vg || now_ms - lr.last_ms > kLosEpisodeGapMs)
                        {
                            lr.victim = vg;
                            lr.steps = 0;
                            lr.episode_start_ms = now_ms;
                            lr.last_ms = 0;
                        }
                        if (lr.steps >= kLosMaxSteps || (lr.steps > 0 && now_ms - lr.episode_start_ms > kLosEpisodeMs))
                        {
                            TC_LOG_INFO("playerbot.v2", "[los_fallback] {} drops victim {} after {} reposition steps / {} ms",
                                         snapshot.name(), vg.ToString(), lr.steps, now_ms - lr.episode_start_ms);
                            lr = {};
                            emit.stop_attack();
                            ai.note_engage(vg, now_ms);
                            ai.set_victim_watch(ObjectGuid::Empty, 0, 0);
                            ai.set_last_rule_fired("combat:los_fallback");
                            return;
                        }
                        const float vmx = vu->x - lr.victim_x, vmy = vu->y - lr.victim_y;
                        const bool victim_moved = lr.steps == 0 ||
                            vmx * vmx + vmy * vmy > kLosRepathMoveYd * kLosRepathMoveYd;
                        const bool due = lr.steps == 0 ||
                            (now_ms - lr.last_ms >= kLosRepathMs && (victim_moved || !snapshot.is_moving()));
                        if (due)
                        {
                            const float dist = std::sqrt(std::max(dsq, 1.0f));
                            const float step = std::min(10.0f, dist);
                            emit.move_to(bx + dx / dist * step,
                                         by + dy / dist * step,
                                         bz, /*run*/ true);
                            lr.victim_x = vu->x;
                            lr.victim_y = vu->y;
                            lr.last_ms = now_ms;
                            ++lr.steps;
                        }
                        ai.set_last_rule_fired(!vu->in_los
                            ? "combat:reposition_los"
                            : "combat:reposition_range");
                        return;
                    }
                    else
                    {
                        // LoS and range regained: the episode is over.
                        ai.los_repath() = {};
                    }
                }
            }
        }
    }

    // ---- Loose elite / boss on this non-tank: shed it toward the tank ----
    if (LooseMobEscape(ai, snapshot, group, emit))
        return;
    if (RangedMeleedDefense(ai, snapshot, group, emit))
        return;

    // ---- Instance LoS step ([raid_los], State_Idle.cpp) ----
    // The victim LoS reposition above is open-world / BG only. In a dungeon
    // or raid a ranged / healer bot whose cast target (healer: its anchor
    // tank; ranged: its victim) is out of line of sight steps ~10y toward
    // the anchor tank or around the pillar, staying out of the melee splash
    // and ground hazards - before the healer follow / gap-close below, which
    // only react to range.
    // Under an owner Stay / Hold the bot keeps its spot unless a mob is on it (playtest, Deadmines 2026-10-10: Edrin and Graziella
    // kept stepping 10-16 y for LoS under Stay).
    bool held_in_place = false;
    if (ai.holding_position(snapshot.published_at_ms()))
    {
        held_in_place = true;
        for (auto const& at : snapshot.raw().combat.attackers)
            if (at.hp > 0 && at.victim == snapshot.guid()) { held_in_place = false; break; }
    }
    if (!held_in_place && snapshot.is_in_instance() && !snapshot.in_battleground() && group.exists() &&
        RaidCastLosStep(snapshot, ai, group, emit))
        return;

    // ---- Raid formation: keep the arc slot behind the tank (RaidFormationHold, State_Idle.cpp) ----
    // A ranged DPS / healer whose tank has moved it more than a few yards off its slot walks back to it, so the raid stays in shape
    // when the boss is pulled back or repositioned. A DPS bot with no victim is waiting for the Attack and stays put; a healer
    // (it has no victim of its own) holds its slot too, which lies 22 y / 17 y behind its anchor tank, inside heal range.
    if (snapshot.is_in_raid() && !snapshot.in_battleground() && group.exists() &&
        snapshot.raw().pve_order.cc_target.IsEmpty() &&   // a bot on crowd-control duty stands still to cast it
        (!snapshot.victim().IsEmpty() || ai.effective_role(snapshot) == Role::Healer) &&
        RaidFormationHold(snapshot, ai, group, emit))
        return;

    // ---- Raid ranged DPS: walk into cast range ([raid_range]) ----
    // The open-world / BG reposition above skips instances, and RaidCastLosStep only reacts to LoS, so a caster standing 35-45 y back
    // (where it waited for the owner's Attack) stayed there after the release: spell 704 at 43 y logged OUT_OF_RANGE x7 and the bot
    // idled (live MC Gehennas 2026-10-08, Gustinor). Past the reach of its spells it steps to ~24 y + the victim's reach from the
    // victim; paced like combat:reposition_los (one path per second, victim moved > 3 y or the bot stopped), never into a ground
    // hazard / avoid circle, never while casting or rooted.
    if (snapshot.is_in_raid() && !snapshot.in_battleground() && !snapshot.victim().IsEmpty() &&
        ai.effective_role(snapshot) == Role::Dps && !IsMeleeSpec(snapshot.cls(), snapshot.spec()) &&
        !snapshot.is_casting() && !snapshot.is_rooted())
    {
        NearbyUnit const* vu = snapshot.victim_info();
        if (vu && vu->hp > 0 && vu->in_los)
        {
            float bx, by, bz;
            snapshot.position(bx, by, bz);
            const float dx = vu->x - bx, dy = vu->y - by;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 30.0f + vu->combat_reach && dist > 1.0f)
            {
                const uint32 now_ms = snapshot.published_at_ms();
                const float want = 24.0f + vu->combat_reach;           // standoff from the victim
                const float tx = vu->x - dx / dist * want, ty = vu->y - dy / dist * want;
                auto& lr = ai.los_repath();
                const float vmx = vu->x - lr.victim_x, vmy = vu->y - lr.victim_y;
                const bool due = lr.victim != snapshot.victim() || lr.steps == 0 ||
                    (now_ms - lr.last_ms >= 1000u &&
                     (vmx * vmx + vmy * vmy > 9.0f || !snapshot.is_moving()));
                if (!RaidStandPointBlocked(snapshot, tx, ty, vu->z))
                {
                    if (due)
                    {
                        if (lr.victim != snapshot.victim())
                            lr = {};
                        lr.victim = snapshot.victim();
                        lr.victim_x = vu->x;
                        lr.victim_y = vu->y;
                        lr.last_ms = now_ms;
                        ++lr.steps;
                        if (RaidLogDue(RaidLogKey(snapshot, RaidLogKind::ThreatHold), now_ms, 5000u))
                            TC_LOG_INFO("playerbot.v2", "[raid_range] {} steps in: victim entry {} at {:.1f} y, cast reach {:.1f}",
                                        snapshot.name(), vu->entry, dist, 30.0f + vu->combat_reach);
                        emit.move_to(tx, ty, vu->z, /*run*/ true);
                    }
                    ai.set_last_rule_fired("combat:raid_range_close");
                    return;
                }
            }
        }
    }

    // ---- Healer STAY-IN-RANGE of the tank (Deadmines harbor, 2026-06-26) ----
    // ROOT CAUSE of the harbor wipe + the chronic dungeon cohesion lag: heal
    // spells cast at <=40y, but when the tank pulls beyond that (a fast boss-
    // advance, a descent into an aggro zone, the harbor stalker crossing) the
    // heal rotation still PICKS the tank and emits a heal that fails OutOfRange —
    // and the healer ROOTS itself re-casting the doomed heal every tick instead
    // of closing the gap. The tank then dies unhealed while the healer spams
    // OutOfRange (observed live: Dunghealer 46y back, 32 consecutive
    // CastSpell|OutOfRange, tank died holding at the harbor entrance at 0%). The
    // existing gap-close below is too narrow (only the SINGLE lowest member, only
    // <65%, only >45y) — a near hurt DPS as "lowest" leaves the far tank
    // un-followed. A real healer keeps the TANK in heal range first. So in a
    // dungeon, when the tank is beyond ~35y, run toward it BEFORE any cast.
    // Guarded by self-HP>40% so a near-death healer still self-heals instead of
    // chasing. This also closes the (0c) harbor cohesion gate quickly so the
    // group balls up and crosses together rather than the tank holding to death.
    // Not mid-cast: a cast in progress is on a target in range (an out-of-
    // range heal fails at once and never enters the casting state), and
    // moving would throw it away.
    if (snapshot.raw().group.my_role == Role::Healer && !snapshot.is_casting() &&
        snapshot.is_in_dungeon() && snapshot.hp_pct() > 40 && group.exists())
    {
        // A raid healer keeps ITS heal_focus tank in range (the tank rule 5c
        // walks it to), not g.tank() — following the main tank would drag it
        // off a spare tank holding a mob 50y away.
        if (GroupMemberSummary const* tk = GroupAnchorTank(snapshot, ai, group))
        {
            if (tk->online && tk->is_alive && tk->guid != snapshot.raw().guid &&
                tk->map_id == snapshot.map_id())
            {
                float bx, by, bz;
                snapshot.position(bx, by, bz);
                const float dx = tk->x - bx, dy = tk->y - by, dz = tk->z - bz;
                const float dsq = dx*dx + dy*dy + dz*dz;
                // Trigger only when the tank is BEYOND heal-cast range (~40y). At
                // <=40y the rotation's heals land, so following there would skip a
                // valid heal and STARVE the tank (a 35y trigger reduced gauntlet
                // healing and tipped a marginal Helix-pit fight into a wipe). Close
                // to ~30y (comfortably in range) so the next tick heals rather than
                // re-follows — no oscillation at the boundary.
                if (dsq > 41.0f * 41.0f)
                {
                    const float dist = std::sqrt(dsq);
                    const float step = std::min(25.0f, dist - 30.0f);
                    const float sx = bx + dx / dist * step, sy = by + dy / dist * step;
                    if (!RaidStandPointBlocked(snapshot, sx, sy, bz) &&   // never into fire / an avoid circle
                        !RaidPointInUnpulledReach(snapshot, sx, sy, bz))   // nor an idle pack's reach
                    {
                        emit.move_to(sx, sy, bz, /*run*/ true);
                        ai.set_last_rule_fired("combat:healer_follow_tank");
                        return;
                    }
                }
            }
        }
    }

    // ---- Healer gap-close (audit B23) ----
    // The heal pickers are now range-gated at 45y, so a wounded member
    // beyond that is no longer a rotation-wedging pick — but they'd be
    // IGNORED instead. A human healer runs toward the hurt teammate.
    // When a group member is genuinely hurt (<65%) on this map and beyond
    // the heal gate, step toward them. Mirrors the melee gap-close.
    // Same self-HP guard as the follow above: a healer under 40% with a mob on it heals itself instead of jogging to a hurt friend.
    if (snapshot.raw().group.my_role == Role::Healer && !snapshot.is_casting() && snapshot.hp_pct() > 40 && group.exists())
    {
        // Not mid-cast (see the follow above). Raid healer: never chase the
        // raid-wide lowest across the room. Close only on its heal_focus
        // tank or a hurt member just beyond heal range (45-60y): anyone
        // further is some other healer's patient.
        GroupMemberSummary const* low = nullptr;
        if (snapshot.is_in_raid() && !snapshot.in_battleground())
        {
            constexpr float kRaidGapMax = 60.0f;
            float rx, ry, rz;
            snapshot.position(rx, ry, rz);
            ObjectGuid const focus = snapshot.raw().pve_order.active
                ? snapshot.raw().pve_order.heal_focus : ObjectGuid::Empty;
            GroupMemberSummary const* best = nullptr;
            if (auto const* mems = group.members())
                for (auto const& m : *mems)
                {
                    if (!m.online || !m.is_alive || m.map_id != snapshot.map_id() ||
                        m.guid == snapshot.raw().guid || m.hp_pct() >= 65)
                        continue;
                    const float mx = m.x - rx, my = m.y - ry, mz = m.z - rz;
                    if (mx * mx + my * my + mz * mz > kRaidGapMax * kRaidGapMax)
                        continue;
                    if (m.guid == focus) { best = &m; break; }
                    if (!best || m.hp_pct() < best->hp_pct()) best = &m;
                }
            low = best;
        }
        else
            low = group.lowest_hp_on_map(snapshot.map_id());
        if (low)
        {
            const int32 low_pct = low->max_hp > 0 ? (low->hp * 100) / low->max_hp : 100;
            if (low_pct < 65 && low->guid != snapshot.raw().guid)
            {
                float bx, by, bz;
                snapshot.position(bx, by, bz);
                const float dx = low->x - bx, dy = low->y - by, dz = low->z - bz;
                const float dsq = dx*dx + dy*dy + dz*dz;
                // BG cap (audit N41): without an upper bound, a BG healer
                // abandoned its local fight and jogged map-wide toward
                // whichever raid member was lowest ANYWHERE (a 40-man AV
                // raid always has someone hurt 300y away). Beyond 80y in
                // a BG the wounded member belongs to a different fight —
                // stay with the local one.
                const float gap_max_sq = snapshot.in_battleground()
                    ? 80.0f * 80.0f
                    : std::numeric_limits<float>::max();
                if (dsq > 45.0f * 45.0f && dsq < gap_max_sq)
                {
                    const float dist = std::sqrt(dsq);
                    const float step = std::min(15.0f, dist - 40.0f);
                    const float sx = bx + dx / dist * step, sy = by + dy / dist * step;
                    if (!RaidStandPointBlocked(snapshot, sx, sy, bz) &&   // never into fire / an avoid circle
                        !RaidPointInUnpulledReach(snapshot, sx, sy, bz))   // nor an idle pack's reach
                    {
                        emit.move_to(sx, sy, bz, /*run*/ true);
                        ai.set_last_rule_fired("combat:healer_gap_close");
                        return;
                    }
                }
            }
        }
    }

    // Human reaction delay. Without this gate, the APL fires on the
    // SAME tick that snapshot.in_combat() first reads true — every
    // interrupt, panic CD, opener pops with sub-human precision (~50ms
    // from combat-start). Real humans react in 200-400ms. Per-bot
    // deterministic jitter (200-450ms) derived from bot_id keeps each
    // bot's reaction stable but de-synchronized from siblings, so a
    // raid of bots doesn't pop CDs in lockstep on the same world tick.
    // Auto-attack still runs server-side during this window; only
    // intent emission is gated.
    {
        const int64_t combat_ms = snapshot.combat_duration_ms();
        if (combat_ms > 0)
        {
            const uint32 jitter_ms =
                200u + (uint32(snapshot.bot_id()) * 1664525u) % 250u;
            if (combat_ms < int64_t(jitter_ms))
                return;
        }
    }

    // Carrier check — needed by the BG tactical layer below AND the
    // homeward/gap-close logic further down (hoisted from there).
    bool i_am_carrier = false;
    if (snapshot.in_battleground())
    {
        i_am_carrier = (snapshot.bg_friendly_flag_carrier() == snapshot.raw().guid);
        if (!i_am_carrier)
            for (auto const& fc : snapshot.bg_all_friendly_carriers())
                if (fc == snapshot.raw().guid) { i_am_carrier = true; break; }
    }

    // ---- Raid tank with no mob yet: wait ----
    // A duty tank the plan gave no engaged mob (PveOrder::tank_hold) stays off the main tank's mob: it drops the
    // victim it ran onto and holds until its mob engages (the plan hands it over within one tick) or a mob hits it.
    if (!snapshot.in_battleground() && ai.effective_role(snapshot) == Role::Tank)
    {
        auto const& th_po = snapshot.raw().pve_order;
        if (th_po.active && th_po.tank_hold && th_po.tank_target.IsEmpty() && snapshot.is_alive() &&
            !snapshot.is_casting() && snapshot.attackers_count() == 0 &&
            // ... unless a mob nobody was given stands on a group member: that one is this tank's to pick up or taunt, whatever the plan
            // holds it for (n24 mc_garr: Mctankwb sat in tank_hold 7 y from a Firesworn on a healer, taunt ready, for a minute).
            !snapshot.pickup_candidate())
        {
            if (!snapshot.victim().IsEmpty())
                emit.stop_attack();
            ai.set_last_rule_fired("combat:tank_hold");
            return;
        }
    }

    // ---- PvE-coordinator raid tank assignment ----
    // The raid plan gives every live tank its own engaged mob
    // (PveOrder::tank_target). Get onto it; the class rotation's taunt
    // takes it off whoever it is hitting. Same per-target hysteresis as
    // the kill focus so a refused switch doesn't re-emit every tick.
    if (!snapshot.in_battleground() && ai.effective_role(snapshot) == Role::Tank)
    {
        auto const& tt_po = snapshot.raw().pve_order;
        if (tt_po.active && !tt_po.tank_target.IsEmpty() &&
            snapshot.is_alive() && !snapshot.is_casting() &&
            snapshot.victim() != tt_po.tank_target)
        {
            for (auto const& u : snapshot.nearby_enemies())
            {
                if (u.guid != tt_po.tank_target) continue;
                if (u.hp <= 0) break;       // corpse - next plan reassigns
                const uint32 tt_now = snapshot.published_at_ms();
                const uint64 tt_key = tt_po.tank_target.GetCounter();
                if (!ai.action_recently_tried(
                        BotAI::ActionKind::PveTargetSwitch, tt_key, tt_now) &&
                    emit.start_attack(tt_po.tank_target))
                {
                    ai.note_action_retry(BotAI::ActionKind::PveTargetSwitch,
                                         tt_key, tt_now);
                    ai.set_last_rule_fired("combat:pve_tank_target");
                    return;
                }
                break;
            }
        }
    }

    // ---- Crowd control by raid symbol (PveOrder::cc_target, Playerbot.Behavior.MarkCc) ----
    // The owner put an icon on this caster and the same icon on a mob: hold the mob. Casts when it is not held (again
    // when the CC breaks); the plan ends the duty when the mob's icon turns skull or is gone, so the raid kills it.
    if (!snapshot.in_battleground())
    {
        auto const& cc_po = snapshot.raw().pve_order;
        if (cc_po.active && !cc_po.cc_target.IsEmpty() && snapshot.is_alive() &&
            ai.effective_role(snapshot) != Role::Tank &&
            MarkGate::MayEngage(snapshot, ai, &group, cc_po.cc_target) &&   // MarksNeedOrder: no CC (= pull) on an idle mob
            MarkCc::TryCast(snapshot, emit, cc_po.cc_target))
        {
            ai.set_last_rule_fired("combat:marked_cc");
            return;
        }
    }

    // ---- The owner's skull (else cross) is the DPS target in every group ----
    // The raid kill focus carries it in raids; 5-man and open-world groups have no coordinator focus, so a DPS bot that
    // already fights something would stay on it (playtest 2026-10-08: "DPS should follow the skull all the time"). Tanks hold
    // aggro and healers heal; an idle mark still needs the owner's order (MarkGate) and the tank's opening lead (threat hold).
    // The owner's latest Attack wins over a standing mark until its mob dies: a bot on the mob he just ordered stays on it.
    const bool on_owner_attack = ai.is_manual_mode(snapshot.published_at_ms()) &&
        ai.owner_command() == BotAI::OwnerCommand::Engage && !ai.owner_target().IsEmpty() && snapshot.victim() == ai.owner_target();
    if (!snapshot.in_battleground() && group.exists() && snapshot.is_alive() && !snapshot.is_casting() && !on_owner_attack)
    {
        // Skull, else cross: a dead skull's icon can stay on the corpse (nothing clears it), which hid the cross.
        ObjectGuid skull;
        for (ObjectGuid const mark : { group.raid_mark(7), group.raid_mark(6) })
        {
            if (mark.IsEmpty()) continue;
            bool alive = false;
            for (auto const& u : snapshot.nearby_enemies())
                if (u.guid == mark) { alive = u.hp > 0; break; }
            if (alive) { skull = mark; break; }
        }
        Role const sk_role = ai.effective_role(snapshot);
        if (!skull.IsEmpty() && snapshot.victim() != skull && sk_role != Role::Tank && sk_role != Role::Healer &&
            !HeldByOwner(ai, snapshot))
            for (auto const& u : snapshot.nearby_enemies())
            {
                if (u.guid != skull) continue;
                if (u.hp <= 0) break;
                const uint32 sk_now = snapshot.published_at_ms();
                const uint64 sk_key = skull.GetCounter();
                if (!ai.action_recently_tried(BotAI::ActionKind::PveTargetSwitch, sk_key, sk_now) &&
                    MarkGate::MayEngage(snapshot, ai, &group, skull, &u) &&
                    !ThreatHoldActive(ai, snapshot, skull, &group) &&
                    emit.start_attack(skull))
                {
                    ai.note_action_retry(BotAI::ActionKind::PveTargetSwitch, sk_key, sk_now);
                    ai.set_last_rule_fired("combat:follow_skull");
                    return;
                }
                break;
            }
    }

    // ---- PvE-coordinator kill focus ----
    // Synchronized add-burn: when the group plan designates a priority
    // kill target, every DPS switches to it TOGETHER instead of each bot
    // discovering the add on its own schedule (the old behavior smeared
    // the swap over several seconds and split damage across adds).
    // Hysteresis via PveTargetSwitch (8s per target) so the switch
    // doesn't re-emit every combat tick.
    if (!snapshot.in_battleground())
    {
        auto const& kf_po = snapshot.raw().pve_order;
        if (kf_po.active && !kf_po.kill_focus.IsEmpty() &&
            snapshot.is_alive() && !snapshot.is_casting() && !on_owner_attack &&
            snapshot.victim() != kf_po.kill_focus)
        {
            for (auto const& u : snapshot.nearby_enemies())
            {
                if (u.guid != kf_po.kill_focus) continue;
                if (u.hp <= 0) break;       // corpse — next plan reassigns
                const uint32 kf_now = snapshot.published_at_ms();
                const uint64 kf_key = kf_po.kill_focus.GetCounter();
                if (!ai.action_recently_tried(
                        BotAI::ActionKind::PveTargetSwitch, kf_key, kf_now) &&
                    !ThreatHoldActive(ai, snapshot, kf_po.kill_focus, &group) &&   // threat discipline
                    emit.start_attack(kf_po.kill_focus))
                {
                    ai.note_action_retry(BotAI::ActionKind::PveTargetSwitch,
                                         kf_key, kf_now);
                    ai.set_last_rule_fired("combat:pve_kill_focus");
                    return;
                }
                break;
            }
        }
    }

    // ---- BG tactical layer (BG audit N26/N27/N28/N39/N46) ----
    // Every PvP-tactical behavior used to live exclusively in State_Idle's
    // BgDispatch — structurally unreachable the moment a bot entered
    // combat, which in a BG is most of the match. State_InCombat fought
    // every battle as PvE: no focus-fire, no EFC kill, no peel, targets
    // only changed when the victim died, escorts wandered off mid-fight,
    // and a dropped flag two yards away was never clicked.
    if (snapshot.in_battleground())
    {
        const uint32 bg_now_ms = snapshot.published_at_ms();
        float bx, by, bz;
        snapshot.position(bx, by, bz);

        // (0) Siege-vehicle GATE fire in combat (BG audit IoC / SoTA). The
        // idle bg_vehicle_fire_gate rule is unreachable once the vehicle is
        // attacked — but a demolisher / siege engine ramming a DEFENDED gate
        // is in combat exactly when it most needs to fire. Mirror the idle
        // gate-fire: resolve this vehicle's seat spell and cast it at the
        // nearest STANDING target gate. Runs before the on-foot tactical
        // branches below (a seated bot has no melee/peel job).
        if (snapshot.on_vehicle())
        {
            BattlegroundAdvice const& adv = ai.bg_advice_cache().cached;
            uint32 seat_spell = 0;
            const uint32 ve = snapshot.vehicle_entry();
            if (ve != 0)
            {
                auto it = adv.vehicle_seat_spell_by_entry.find(ve);
                if (it != adv.vehicle_seat_spell_by_entry.end())
                    seat_spell = it->second;
            }
            if (seat_spell == 0) seat_spell = adv.vehicle_seat_spell;
            if (seat_spell != 0 && !adv.siege_target_go_entries.empty() &&
                !snapshot.is_casting())
            {
                BotSnapshot::NearbyObject const* gate = nullptr;
                float gate_dsq = 60.0f * 60.0f;
                for (auto const& o : snapshot.raw().world_objects.nearby_objects)
                {
                    if (o.go_type != /*DESTRUCTIBLE_BUILDING*/ 33) continue;
                    if (o.is_destroyed) continue;
                    bool match = false;
                    for (uint32 ge : adv.siege_target_go_entries)
                        if (o.entry == ge) { match = true; break; }
                    if (!match) continue;
                    const float dx = o.x - bx, dy = o.y - by;
                    const float dsq = dx * dx + dy * dy;
                    if (dsq < gate_dsq) { gate_dsq = dsq; gate = &o; }
                }
                if (gate)
                {
                    const uint64 gate_low = gate->guid.GetCounter();
                    if (!ai.action_recently_tried(BotAI::ActionKind::BgVehicleFire,
                                                  gate_low, bg_now_ms))
                    {
                        emit.cast_vehicle_at(seat_spell, gate->x, gate->y, gate->z);
                        ai.note_action_retry(BotAI::ActionKind::BgVehicleFire,
                                             gate_low, bg_now_ms);
                        ai.set_last_rule_fired("combat:bg_vehicle_fire_gate");
                        return;
                    }
                    // Gate in range, weapon on its short cooldown: HOLD here and
                    // keep battering rather than dropping to the tactical branches
                    // (a seated siege bot has no melee job anyway).
                    ai.set_last_rule_fired("combat:bg_vehicle_fire_gate_hold");
                    return;
                }
            }
        }

        // (1) Escort stickiness (N28): an FCEscort that drifted >40y from
        // its carrier mid-skirmish abandons the duel and returns — the
        // carrier dying alone loses the match, the road kill doesn't.
        // Team-coordinator orders override the legacy role test (N60
        // review): a bot ordered to DefendNode/HuntEFC/etc must NOT be
        // yanked back to the carrier by its hashed FCEscort role, and a
        // bot ORDERED to escort returns regardless of its hashed role.
        bool escort_duty;
        {
            auto const& ord = snapshot.raw().bg.order;
            escort_duty = ord.kind == BgState::BgOrder::None
                ? snapshot.raw().bg.bg_role == uint8(BgRole::FCEscort)
                : ord.kind == BgState::BgOrder::EscortFC;
        }
        // Before the pickup there is no carrier: the escorts of the pickup runner (order focus = the runner) keep to it the same way
        // (BgTactics::EscortMustReturn), so the runner does not reach the defended flag room alone.
        if (!i_am_carrier && escort_duty && snapshot.bg_friendly_flag_carrier().IsEmpty() &&
            snapshot.raw().bg.order.kind == BgState::BgOrder::EscortFC && !snapshot.raw().bg.order.focus.IsEmpty() &&
            snapshot.raw().bg.order.focus != snapshot.raw().guid)
        {
            ObjectGuid const rf = snapshot.raw().bg.order.focus;
            float rdist = -1.f;
            for (auto const& f : snapshot.raw().combat.nearby_friends)
                if (f.guid == rf)
                {
                    const float rdx = f.x - bx, rdy = f.y - by;
                    rdist = std::sqrt(rdx * rdx + rdy * rdy);
                    break;
                }
            bool runner_enemy_close = false;
            for (auto const& e : snapshot.nearby_enemies())
            {
                if (e.hp <= 0 || e.untargetable || e.is_pacified || e.is_cc_locked) continue;
                const float edx = e.x - bx, edy = e.y - by;
                if (edx * edx + edy * edy <= 30.0f * 30.0f) { runner_enemy_close = true; break; }
            }
            if (BgTactics::EscortMustReturn(rdist, runner_enemy_close))
            {
                emit.follow(rf, 8.0f);
                ai.set_last_rule_fired("combat:bg_escort_follow_runner");
                return;
            }
        }
        if (!i_am_carrier && escort_duty &&
            !snapshot.bg_friendly_flag_carrier().IsEmpty() &&
            snapshot.bg_friendly_flag_carrier() != snapshot.raw().guid)
        {
            const float fcx = snapshot.raw().bg.friendly_carrier_x;
            const float fcy = snapshot.raw().bg.friendly_carrier_y;
            if (fcx != 0.f || fcy != 0.f)
            {
                const float dx = fcx - bx, dy = fcy - by;
                // An enemy within 30 y wins over the return (playtest, WSG 2026-10-09: a druid on this rule stood in the tunnel for 40 s
                // with "no target" while enemy players ran past): the escort fights what is in front of it, and returns when none is close.
                bool escort_enemy_close = false;
                for (auto const& e : snapshot.nearby_enemies())
                {
                    if (e.hp <= 0 || e.untargetable || e.is_pacified || e.is_cc_locked) continue;
                    const float edx = e.x - bx, edy = e.y - by;
                    if (edx * edx + edy * edy <= 30.0f * 30.0f) { escort_enemy_close = true; break; }
                }
                if (dx * dx + dy * dy > 40.0f * 40.0f && !escort_enemy_close)
                {
                    emit.follow(snapshot.bg_friendly_flag_carrier(), 8.0f);
                    ai.set_last_rule_fired("combat:bg_escort_return");
                    return;
                }
            }
        }

        // (2) Dropped-flag click (N26): NEW_FLAG_DROP (37) / FLAGDROP (26)
        // GOs are instant interactions worth more than any rotation tick —
        // returning our flag or denying theirs mid-fight is how humans
        // play CTF.
        for (auto const& o : snapshot.raw().world_objects.nearby_objects)
        {
            if (o.go_type != /*NEW_FLAG_DROP*/ 37 && o.go_type != /*FLAGDROP*/ 26)
                continue;
            const float dx = o.x - bx, dy = o.y - by;
            if (dx * dx + dy * dy > 64.0f) continue;  // 8y
            const uint64 go_low = o.guid.GetCounter();
            if (ai.action_recently_tried(BotAI::ActionKind::BgUseGo, go_low, bg_now_ms))
                continue;
            emit.use_game_object(o.guid);
            ai.note_action_retry(BotAI::ActionKind::BgUseGo, go_low, bg_now_ms);
            ai.set_last_rule_fired("combat:bg_grab_dropped_flag");
            return;
        }

        // (2b) Standing-objective interaction in combat (BG audit S1 — the
        // #1 "no bot picks up a flag" blocker). The enemy flag PEDESTAL
        // (NEW_FLAG 36), capture points (42), flagstands/orbs (24) and the
        // entry-enumerated banners (AV) are INSTANT server-side interactions
        // with NO combat restriction (Player::CanUseBattlegroundObject checks
        // only alive + faction + recently-dropped debuff). The old comment
        // here claimed "capture-point channels break on damage" — false for
        // modern instant caps. A defended WSG/TP flagroom or a contested
        // AB/BfG/EotS/AV node keeps the grabber in combat the entire time it
        // stands on the objective, so without an in-combat click the flag is
        // never lifted / the node never flips. Run the IDENTICAL scan the
        // idle dispatch uses (shared helper). Carriers are excluded — their
        // job is to run the flag home (handled by BgCarrierHomeward below),
        // not to re-click objects. Placed before the target-switch / gap-close
        // so securing the objective preempts the brawl.
        if (!i_am_carrier && snapshot.is_alive() && !snapshot.is_casting() &&
            States::BgTryUseObjectiveGo(snapshot, ai, emit,
                                        ai.bg_advice_cache().cached))
            return;

        // (2c) In-combat ADVANCE to the enemy flag (BG audit follow-up
        // 2026-06-22). On a CTF map the enemy flag sits inside the enemy
        // base, which is permanently defended — a carrier-seeker pushing in
        // is ALWAYS in combat there. The idle grab rule (idle:bg_fc_grab_flag)
        // is `!in_combat`-gated, and (2b) above only fires within the helper's
        // 12y use/approach band, so a bot fighting at 13-60y from the flag had
        // NO rule to close the gap: it brawled in place and the flag was never
        // taken (live: bots floored at ~16y from the pedestal, 0 pickups even
        // on the correct map). Mirror the idle acts_as_fc condition (no friendly
        // carrier yet, carrier-seeker role) and keep walking toward the flag
        // while fighting; once inside 12y the (2b) helper next tick approaches
        // and clicks it. Scoped to CTF by `enemy_flag` being set (node-race
        // BGs leave it zero), and capped at 60y so a mid-field skirmisher
        // isn't yanked off an unrelated fight.
        if (!i_am_carrier && snapshot.is_alive() && !snapshot.is_casting() &&
            snapshot.bg_friendly_flag_carrier().IsEmpty())
        {
            BattlegroundAdvice const& adv = ai.bg_advice_cache().cached;
            const uint8 br = snapshot.raw().bg.bg_role;
            const bool carrier_seeker =
                br == uint8(BgRole::FlagCarrier) ||
                br == uint8(BgRole::Roamer)      ||
                br == uint8(BgRole::FCEscort);
            if (carrier_seeker &&
                (adv.enemy_flag_x != 0.f || adv.enemy_flag_y != 0.f))
            {
                const float fdx = adv.enemy_flag_x - bx;
                const float fdy = adv.enemy_flag_y - by;
                const float fd2 = fdx * fdx + fdy * fdy;
                // Push to the flag from ANY distance >12y (inside 12y the (2b)
                // helper approaches+clicks). NO upper cap: WSG bases are ~620y
                // apart, so a carrier-seeker that aggroes at mid-field is ~300y
                // from the enemy flag — a 60y cap never engaged and the bot
                // brawled in place (live: fc_advance=0, 0 pickups). A flag
                // runner's job IS to run the flag, not trade blows mid-field;
                // it pushes through, fighting only incidentally. Other roles
                // (Attacker/Defender/Healer) hold the line.
                // 2026-10-08 (WSG test, the owner: "running past each other without fighting"; [dps_idle] "85 s idle:
                // combat:bg_fc_advance_flag, enemy at 3.8 y"): this rule returned before the rotation every tick, so every
                // Roamer / carrier-seeker ran through the enemy team without casting or swinging. An enemy within 22 y now wins:
                // the bot fights it (the rotation below closes and chases), and runs for the flag only when nothing hostile is
                // close, or when it holds the PickupFlag order (the one runner stays on the flag).
                // The one runner holding the PickupFlag order rushes past enemies only with company (BgTactics::RunnerMayRush): alone it
                // fights what is near, the escorts catch up. WSG soak 2026-10-11: the same runner lifted the flag and died with it four
                // times running because it went into the defended room by itself.
                bool adv_enemy_close = false;
                {
                    int enemies_near = 0, friends_near = 0;
                    for (auto const& e : snapshot.nearby_enemies())
                    {
                        if (e.hp <= 0) continue;
                        const float edx = e.x - bx, edy = e.y - by;
                        if (edx * edx + edy * edy <= BgTactics::kRunnerEnemyRange * BgTactics::kRunnerEnemyRange) ++enemies_near;
                    }
                    if (snapshot.raw().bg.order.kind != BgState::BgOrder::PickupFlag)
                        adv_enemy_close = enemies_near > 0;
                    else
                    {
                        for (auto const& f : snapshot.raw().combat.nearby_friends)
                        {
                            if (f.hp <= 0) continue;
                            const float fdx2 = f.x - bx, fdy2 = f.y - by;
                            if (fdx2 * fdx2 + fdy2 * fdy2 <= BgTactics::kRunnerCompanyRange * BgTactics::kRunnerCompanyRange) ++friends_near;
                        }
                        adv_enemy_close = !BgTactics::RunnerMayRush(friends_near, enemies_near);
                    }
                }
                if (fd2 > 144.0f && !adv_enemy_close)
                {
                    emit.move_to(adv.enemy_flag_x, adv.enemy_flag_y,
                                 adv.enemy_flag_z, /*run=*/true);
                    ai.set_last_rule_fired("combat:bg_fc_advance_flag");
                    return;
                }
            }
        }

        // (2d) In-combat ADVANCE to the AV endgame target (BG audit follow-up
        // 2026-06-22). A PushEndgame order (AV captain/general rush) marches the
        // bot deep into the enemy base, which on AV is wall-to-wall elite NPCs +
        // enemy bots — so the pusher is ALWAYS in combat the whole way in. With
        // no in-combat advance the squad bogs down trading blows with trash
        // hundreds of yards short of the captain and never reaches it (live:
        // PUSH=1 for minutes, push_fires=0, score frozen at the 1-tower lead).
        // Mirror the flag-advance above: keep marching toward the ordered
        // boss-room position while fighting, and FOCUS the named captain/general
        // the instant it's visible (it grants the reinforcement swing / instant
        // win — worth more than any trash kill). Uncapped distance (the general
        // is across the map). Scoped strictly to PushEndgame + a target entry so
        // normal node/CTF combat is untouched.
        {
            auto const& ord = snapshot.raw().bg.order;
            // NOTE: deliberately NOT gated on !is_casting. A pusher stalled at
            // caster range (46y) was stuck CASTING at the out-of-LoS captain
            // every tick (328 LoS-failed casts, 0 landed) — the cast rooted it
            // so the push-move only fired in the gaps and the bot never closed.
            // Firing mid-cast lets the move_to below CANCEL the futile cast
            // (movement interrupts casting) and the bot rushes to point-blank,
            // where start_attack opens with clear LoS. At <=6y the start_attack
            // path is benign (does not cancel an in-range damaging cast).
            if (ord.kind == BgState::BgOrder::PushEndgame &&
                ord.target_entry != 0 && snapshot.is_alive())
            {
                NearbyUnit const* tgt = nullptr;
                float tgt_dsq = std::numeric_limits<float>::max();
                for (auto const& e : snapshot.raw().combat.nearby_enemies)
                {
                    if (e.entry != ord.target_entry || e.hp <= 0 ||
                        e.is_cc_locked)
                        continue;
                    const float dx = e.x - bx, dy = e.y - by;
                    const float d2 = dx * dx + dy * dy;
                    if (d2 < tgt_dsq) { tgt_dsq = d2; tgt = &e; }
                }
                if (tgt)
                {
                    // ENGAGE-ON-LINE-OF-SIGHT. The captain (Galvangar) / general
                    // sits inside an OPEN-TOP walled garrison/courtyard (live LoS
                    // probe [avlos]: roofed=0, every interior point within 15y of
                    // Galvangar has a clear shot). The hard part was never LoS at
                    // his position — it was bots opening fire from OUTSIDE the wall
                    // (no LoS through it) and eating SPELL_FAILED_LINE_OF_SIGHT.
                    // The `in_los` gate alone fixes that: a real raycast is clear
                    // ONLY from a position with an actual shot. So engage the
                    // instant the bot has LoS at normal combat range (<=40y, max
                    // cast range) and let the class bot logic position — casters fire from
                    // range, melee close to 5y. The old <=6y point-blank gate was a
                    // workaround for the missing LoS check; with `in_los` it just
                    // FORCED bots to chase the (moving) captain to within 6y before
                    // attacking, so the ~5 bots that reached the courtyard circled
                    // him at 6-7y los=1 landing almost no damage (live: capalive
                    // never flipped). Below 40y + clear LoS = open fire.
                    if (tgt->in_los && tgt_dsq <= 40.0f * 40.0f)
                    {
                        emit.start_attack(tgt->guid);
                        ai.set_last_rule_fired("combat:bg_push_endgame_target");
                        return;
                    }
                    // ENTRANCE STAGING. No LoS yet (wall between us and the boss).
                    // If this boss has a known interior entrance staging point and
                    // we're within 80y but still outside, route THROUGH the entrance
                    // (to the staging point) rather than straight at the boss — a
                    // direct move_to dead-ends in the perimeter wall pockets (live:
                    // pushers pinned at ~25-44y los=0, 0 damage). Once we reach the
                    // staging point (inside) we gain LoS and the engage branch above
                    // fires. Switch back to the boss within 8y of staging (we're in).
                    {
                        float sx = 0.f, sy = 0.f, sz = 0.f;
                        if (tgt_dsq <= 80.0f * 80.0f &&
                            BgBossStagingPoint(ord.target_entry, sx, sy, sz))
                        {
                            const float sdx = sx - bx, sdy = sy - by;
                            if (sdx * sdx + sdy * sdy > 8.0f * 8.0f)
                            {
                                emit.move_to(sx, sy, sz, /*run=*/true);
                                ai.set_last_rule_fired("combat:bg_push_endgame_stage");
                                return;
                            }
                        }
                    }
                    emit.move_to(tgt->x, tgt->y, tgt->z, /*run=*/true);
                    ai.set_last_rule_fired("combat:bg_push_endgame_advance");
                    return;
                }
                // Target not in sight yet — keep pushing toward the boss room
                // through the trash rather than brawling in place. The boss room
                // is 1400-2300y across the map, far beyond the pathfinder's
                // partial-path cap, so a raw move_to never advances; leapfrog the
                // node chain (same shared mover the idle push march uses) so each
                // leg is a single navmesh-routable hop.
                if ((ord.x != 0.f || ord.y != 0.f))
                {
                    const float odx = ord.x - bx, ody = ord.y - by;
                    if (odx * odx + ody * ody > 144.0f)  // >12y
                    {
                        BgPushThroughNodes(snapshot, ai, emit,
                                           ai.bg_advice_cache().cached,
                                           ord.x, ord.y, ord.z, bg_now_ms);
                        ai.set_last_rule_fired("combat:bg_push_endgame_advance");
                        return;
                    }
                }
            }
        }

        // (3) Priority target switch (N27/N39): EFC kill > peel for our
        // carrier > enemy healer focus. Hysteresis via BgTargetSwitch (8s)
        // so the focus doesn't flip every tick. Friendly-CC'd targets are
        // skipped — breaking our own CC wastes it.
        if (!i_am_carrier)
        {
            ObjectGuid switch_to;
            char const* switch_rule = nullptr;
            const ObjectGuid efc = snapshot.bg_enemy_flag_carrier();
            const ObjectGuid our_fc = snapshot.bg_friendly_flag_carrier();
            float best_dsq = BgTargetPick::kBgEngageRange * BgTargetPick::kBgEngageRange;
            // EFC within the engage range.
            if (!efc.IsEmpty())
                for (auto const& e : snapshot.raw().combat.nearby_enemies)
                    if (e.guid == efc && e.hp > 0 && !e.is_cc_locked)
                    {
                        const float dx = e.x - bx, dy = e.y - by;
                        if (dx * dx + dy * dy <= best_dsq)
                        { switch_to = efc; switch_rule = "combat:bg_focus_efc"; }
                        break;
                    }
            // Peel: enemy player beating on our carrier.
            if (switch_to.IsEmpty() && !our_fc.IsEmpty())
            {
                best_dsq = BgTargetPick::kBgEngageRange * BgTargetPick::kBgEngageRange;
                for (auto const& e : snapshot.raw().combat.nearby_enemies)
                {
                    if (!e.is_player || e.hp <= 0 || e.is_cc_locked) continue;
                    if (e.victim != our_fc) continue;
                    const float dx = e.x - bx, dy = e.y - by;
                    const float dsq = dx * dx + dy * dy;
                    if (dsq < best_dsq)
                    { best_dsq = dsq; switch_to = e.guid;
                      switch_rule = "combat:bg_peel_fc_attacker"; }
                }
            }
            // Enemy healer focus within 30y.
            if (switch_to.IsEmpty())
            {
                best_dsq = 40.0f * 40.0f;
                for (auto const& e : snapshot.raw().combat.nearby_enemies)
                {
                    if (!e.is_player || e.hp <= 0 || e.is_cc_locked) continue;
                    if (e.role != Role::Healer) continue;
                    const float dx = e.x - bx, dy = e.y - by;
                    const float dsq = dx * dx + dy * dy;
                    if (dsq < best_dsq)
                    { best_dsq = dsq; switch_to = e.guid;
                      switch_rule = "combat:bg_focus_enemy_healer"; }
                }
            }
            // Victim validity (playtest, WSG 2026-10-09: a priest stood beside him swinging at nothing): a victim that is gone, dead,
            // untargetable, unreachable, or a creature that does not fight us while an enemy player is close is dropped for the
            // nearest enemy player, or released when there is none (the attacker seeding below picks the next one).
            if (switch_to.IsEmpty() && !snapshot.victim().IsEmpty())
            {
                BgTargetPick::Why why = BgTargetPick::Why::None;
                ObjectGuid const alt = BgTargetPick::Pick<NearbyUnit, ObjectGuid>(
                    snapshot.guid(), snapshot.victim(), bx, by, IsRangedSpec(snapshot.cls(), uint16(snapshot.spec())), BgTargetPick::kBgEngageRange,
                    snapshot.raw().combat.nearby_enemies, snapshot.raw().combat.attackers, why);
                if (why != BgTargetPick::Why::None &&
                    !ai.action_recently_tried(BotAI::ActionKind::BgTargetSwitch, snapshot.victim().GetCounter(), bg_now_ms))
                {
                    TC_LOG_INFO("playerbot.v2", "[bg_target] bot={} victim={} dropped={} switch_to={}", snapshot.bot_id(),
                                snapshot.victim().GetCounter(), BgTargetPick::WhyName(why), alt.IsEmpty() ? 0u : uint32(alt.GetCounter()));
                    ai.note_action_retry(BotAI::ActionKind::BgTargetSwitch, snapshot.victim().GetCounter(), bg_now_ms);
                    if (!alt.IsEmpty())
                        switch_to = alt, switch_rule = "combat:bg_victim_invalid";
                    else if (why != BgTargetPick::Why::IdleMob)
                    {
                        emit.stop_attack();
                        ai.set_last_rule_fired("combat:bg_victim_release");
                    }
                }
            }
            if (!switch_to.IsEmpty() && switch_to != snapshot.victim() &&
                !ai.action_recently_tried(BotAI::ActionKind::BgTargetSwitch,
                                          switch_to.GetCounter(), bg_now_ms))
            {
                emit.start_attack(switch_to);
                ai.note_action_retry(BotAI::ActionKind::BgTargetSwitch,
                                     switch_to.GetCounter(), bg_now_ms);
                ai.set_last_rule_fired(switch_rule);
                // No return: the rotation continues this tick against the
                // new target once the attack intent lands.
            }
        }

        // (4) Moving-cart hold in combat (BG audit §2). In SM (map 727) and
        // DHR (2656) the scoring zone is a MOVING cart (published as a live
        // node_state). A bot drifting >8y from the cart while fighting leaves
        // the zone and stops scoring, but the idle move-to-cart is combat-
        // gated. Nudge back toward the nearest own / contested cart node
        // (NON-returning — the APL still fires instants and the survival items
        // below still run while the bot eases back into the zone). Scoped to
        // the two cart maps so static-node BGs are untouched, and skipped for
        // melee that has a victim (the gap-close below owns their movement —
        // avoids a double move_to), so it drives ranged / idle bots only.
        const uint32 cart_map = snapshot.map_id();
        if (!i_am_carrier && (cart_map == 727u || cart_map == 2656u) &&
            (snapshot.victim().IsEmpty() ||
             !IsMeleeSpec(snapshot.cls(), snapshot.spec())))
        {
            const uint8 mt = snapshot.team();
            BotSnapshot::BgNodeState const* hold = nullptr;
            float best = 30.0f * 30.0f;
            for (auto const& n : snapshot.bg_node_states())
            {
                if (n.owner_team != mt && !n.is_contested) continue;  // own / contesting only
                const float dx = n.x - bx, dy = n.y - by;
                const float dsq = dx * dx + dy * dy;
                if (dsq > 64.0f && dsq < best) { best = dsq; hold = &n; }  // 8-30y drift band
            }
            if (hold)
            {
                emit.move_to(hold->x, hold->y, hold->z, /*run=*/true);
                ai.set_last_rule_fired("combat:bg_cart_hold");
            }
        }
    }

    // Survival item before everything else — a healthstone at 25% HP is
    // worth more than the next rotation tick.
    // (Emitted above, ahead of the movement rules.)
    if (survival_item_used) return;

    // Raid resistance potion (boss engaged): emitted above, ahead of the raid orders.
    if (resist_potion_used) return;

    // Mana refill for low-mana casters. Shares the potion CD with the
    // healing potion above; one will block the other in the API layer.
    if (MaybeUseManaPotion(snapshot, group, emit)) return;

    // Defensive racial — Stoneform-style debuff clears.
    if (MaybeUseDefensiveRacial(snapshot, emit)) return;

    // Offensive racial cooldown — Berserking, Blood Fury, etc.
    if (MaybeUseOffensiveRacial(snapshot, ai.effective_role(snapshot), emit)) return;

    // On-use trinket — fire whenever ready in combat. Most trinkets are
    // damage / haste / crit pulses; gating on boss vs trash would leave
    // 5min trinket CDs unfired for entire dungeon runs.
    if (MaybeUseOnUseTrinket(snapshot, emit)) return;

    // Per-dungeon advice for this combat tick — constructed ONCE and reused by
    // the seed-loop ignore filter (below) and DungeonCombatPositioning + the
    // ignored-victim drop further down. Only paid inside instances.
    DungeonAdvice tick_advice;
    DungeonAdvice const* advice_ptr = nullptr;
    if (snapshot.is_in_instance() && Services::Initialized())
    {
        tick_advice = Services::Dungeons().GetAdvice(snapshot);
        advice_ptr = &tick_advice;
    }

    // Self-defense: if we're in combat with no victim but units are aggro'd
    // onto us, target the highest-threat attacker so the rotation has
    // something to act on. Most rotations gate their rules on victim() being
    // non-empty, so without this they'd no-op for the first tick after pull.
    //
    // The guard is ONLY victim().IsEmpty(): the old version also required
    // current_target().IsEmpty(), but a STALE selection (a mob that died /
    // evaded / left the hostile scan, still set as current_target) is non-empty
    // and PERMANENTLY blocked this self-acquire. The bot then sat InCombat with
    // attackers>0 but victim==0 forever — its offensive APL no-opping (every
    // rotation gates on victim()), doing only auto-attack (~0 dmg for a caster)
    // plus panic self-heal: a 50-60 MINUTE combat hang with zero kills / XP
    // (observed live: Velruun L4 Shaman combat_duration 3028s, Thaelyn L4 Druid
    // 3633s, both looping "Healing Surge/Regrowth (<=50% self) > Auto attack").
    // Engaging the highest-threat attacker sets victim so the rotation acts; if
    // current_target was already that attacker, start_attack simply (re)engages
    // it. start_attack validates the target and the executor's per-target
    // StartAttack refusal lockout gates any spam on a genuinely immune unit.
    if (snapshot.victim().IsEmpty())
    {
        // Pick the highest-threat attacker we have NOT already had refused.
        // combat.attackers is threat-sorted descending (BotSnapshotBuilder),
        // so the first non-refused entry is the best reachable target. Skipping
        // refused attackers is what makes the harbor wedge self-resolve: when
        // the tank aggroed ~8 evading/unreachable hostiles, IsValidAttackTarget
        // rejected every StartAttack (→ InvalidTarget → lockout, Fix #1), so
        // re-seeding the same top attacker every tick kept victim empty AND kept
        // re-issuing a doomed StartAttack. By skipping refused attackers we
        // leave victim genuinely empty ONLY when EVERY attacker is unreachable —
        // which is precisely the signal DungeonCombatPositioning's in-combat
        // boss-advance (below) keys on to drive the tank toward the boss instead
        // of standing frozen in fake combat. When at least one attacker is
        // reachable, this still engages it (no behavior change for normal pulls).
        NearbyUnit const* seed = nullptr;
        for (auto const& a : snapshot.raw().combat.attackers)
        {
            if (a.hp <= 0) continue;
            // Untargetable (UNIT_FLAG_UNINTERACTIBLE) attackers can never be
            // acquired — IsValidAttackTarget rejects them. Skipping them keeps
            // victim empty (vs churning doomed StartAttacks on a stalker swarm)
            // so the in-combat boss-advance recognizes the wedge and walks out.
            if (a.untargetable) continue;
            // [increment 1e] Skip a pacified (UNIT_FLAG_PACIFIED) attacker —
            // never a real threat — AND an attacker whose own chase generator
            // reports it cannot path to us (Creature::CanNotReachTarget(),
            // snapshot cannot_reach): the WC corridor ledge pack (2026-07-03)
            // is ordinary targetable creatures, so only this signal marks it.
            // Re-seeding either kind every empty-victim tick would re-install
            // a chase the melee gap-close below can never close. Together with
            // the hp<=0 / untargetable checks above this completes the
            // IsAttackerFightable test. Kill switch:
            // PlayerbotV2.Combat.SkipUnfightable.
            if (Services::Config().combat_skip_unfightable() &&
                (a.is_pacified || a.cannot_reach))
            {
                DiagSkipUnfightable(snapshot, a.guid, UnfightableReason(a));
                continue;
            }
            // Never SEED an unkillable IGNORED marker (e.g. Deadmines 49671
            // Vanessa cutscene double: IMMUNE_TO_NPC, boss-HP, Bloodwash pack-
            // healed). The downstream combat:drop_ignored_victim rule reads the
            // LAGGED snapshot victim and misses this re-seed — its drop window
            // never aligns with the empty-victim tick on which the seed happens —
            // so the tank/rogue grind the marker ~75s and die to ranged adds (the
            // live 06-27 harbor death-loop: tank PINNED at (-173,-582,19) on a
            // single 49671, d2rip flat, 87%->0%). Filtering at the SOURCE leaves
            // victim genuinely empty so the in-combat boss-advance / ghost-push
            // walks the group off the marker toward the boss.
            if (advice_ptr)
            {
                bool ignored = false;
                for (uint32_t ie : advice_ptr->ignore_entries)
                    if (ie == a.entry) { ignored = true; break; }
                if (ignored) continue;
            }
            if (ai.start_attack_recently_refused(a.guid.GetCounter(),
                                                 snapshot.published_at_ms()))
                continue;
            seed = &a;
            break;
        }
        if (seed)
            emit.start_attack(seed->guid);
        // Pet-assist (2026-06-19). A pet pulling mobs flags the OWNER in-combat
        // immediately (TC: owner is in combat while its pet is), so a pet-class
        // bot enters State_InCombat with NO victim and NO attackers — the mobs
        // are fighting the PET, not the bot. The idle:assist_pet rule that would
        // adopt the pet's target is structurally unreachable here (it is an
        // idle-state rule AND self-gates on !in_combat), and none of this
        // state's acquisition paths consult the pet. The bot therefore sat in
        // combat indefinitely with an empty victim, its rotation no-opping every
        // tick (Zekani, L10 BM hunter: 60+ minutes in combat, 0 XP). Adopt the
        // pet's victim / peel target so the rotation engages and the fight
        // resolves. Only when no attacker was grabbed above (real aggro on the
        // bot takes priority); SelectAssistPetTarget returns Empty unless the
        // target is a live hostile visible in our nearby_enemies. Honor the
        // engage shield: if the combat watchdog just disengaged this exact
        // target as unreachable/unkillable (no-progress / LoS-wedge), do NOT
        // re-adopt it via the pet — that would turn the forever-stall into a
        // slower watchdog-cycle loop. The pet retargets on its own; once it
        // bites a reachable mob, the shield (or a different pet_victim) lets
        // this fire again.
        else if (snapshot.has_pet())
        {
            const ObjectGuid pt = SelectAssistPetTarget(snapshot);
            const uint32 pa_now = snapshot.published_at_ms();
            const ObjectGuid pa_shielded = ai.last_engage_target();
            const bool pa_shield_on = !pa_shielded.IsEmpty() &&
                (pa_now - ai.last_engage_at_ms() < ai.last_engage_shield_ms());
            if (!pt.IsEmpty() && !(pa_shield_on && pt == pa_shielded) &&
                !ThreatHoldActive(ai, snapshot, pt, &group))   // threat discipline
                emit.start_attack(pt);
        }
    }

    // Increment 1m (2026-07-20): PROGRESS-STICKY ownership tick — the
    // in-combat counterpart of the one at the top of DungeonDispatch
    // (State_Idle.cpp; see BotAI::move_commit_note_progress()). A dungeon
    // bot mid boss-advance often stays in the InCombat FSM state for the
    // whole fight, so DungeonDispatch's idle-side tick never runs for it —
    // DungeonCombatPositioning/DungeonConvergeToFight below (this file's
    // dungeon movement entry) are the equivalent per-tick hook here. Same
    // kill switch as step-hold.
    if (snapshot.is_in_instance() && Services::Config().move_step_hold_enabled())
    {
        float mc_x, mc_y, mc_z;
        snapshot.position(mc_x, mc_y, mc_z);
        ai.move_commit_note_progress(snapshot.map_id(), mc_x, mc_y, mc_z,
                                     snapshot.published_at_ms());
    }

    // Stranded-member recovery (shared): a follower that respawned at the far
    // entrance graveyard or wedged on a disconnected poly cannot rejoin on foot and
    // would oscillate the in-combat rejoin (rule 0b inside DungeonCombatPositioning)
    // forever. Relocate it onto the group FIRST so it rejoins / rezzes. The helper's
    // own real-combat gate excludes a genuinely fighting bot (fightable attacker or
    // a real victim), so only a stalker-only false-combat straggler is teleported.
    // Not in a battleground: a BG raid is no dungeon squad, the "main body" a straggler is relocated onto
    // would be a cluster of bots on the far side of the map, and the relocation a near-teleport.
    if (snapshot.is_in_instance() && !snapshot.in_battleground() &&
        DungeonRecoverStrandedFollower(snapshot, ai, group, emit,
                                       snapshot.published_at_ms()))
        return;

    // Converge-to-fight runs here too: while the bot IS in combat this resets
    // its join clock (a no-op move), so the timer is clean when it next drops
    // combat still stranded from the tank. (Genuine in-combat bots are excluded
    // by the helper's own !in_combat gate.)
    if (snapshot.is_in_instance() && !snapshot.in_battleground() &&
        DungeonConvergeToFight(snapshot, ai, group, emit,
                               snapshot.published_at_ms()))
        return;

    // [BUG G-P0a] Dungeon-combat positioning. The avoidance / mechanic
    // cascade (step-out-of-fire → kite → soak → spread → stack → melee-behind)
    // used to live only in the idle dispatcher and therefore NEVER ran during
    // combat — bots stood in boss fire. Run it here, in-combat, BEFORE the
    // melee gap-close so avoidance preempts the chase. Gated on is_in_instance
    // so the GetAdvice() lookup (which constructs the per-dungeon advice
    // vectors) is only paid inside instances; out in the open world this is a
    // single bool check. Early-return on a positioning emit so the bot
    // actually moves out of danger instead of chasing its victim this tick.
    if (advice_ptr)
    {
        DungeonAdvice const& advice = *advice_ptr;
        if (DungeonCombatPositioning(snapshot, ai, group, emit, advice))
            return;
        // IGNORED-ENTRY VICTIM DROP (2026-06-27). advice.ignore_entries are
        // unkillable markers/props (e.g. Deadmines 49671 Vanessa VanCleef:
        // faction-17 hostile, IMMUNE_TO_NPC, boss-HP HealthModifier 20, AIName
        // SmartAI but ZERO scripts — every bot spell on her is rejected). The
        // unkillable-target leash EXEMPTS elites/bosses, so a bot that acquires
        // one grinds it forever AND its member-in-combat blocks tank_advance
        // (observed live 2026-06-27: tank+rogue+hunter pinned on a 49671 spawn
        // in the Helix foundry 15+ min, run frozen at 1/6, never reached the
        // harbor). If our victim is an ignored entry, DROP it and set the engage
        // shield so the last-ditch fallback (combat:fallback_attack honours the
        // shield via skip_target) cannot immediately re-grab it; the bot exits
        // her false-combat and falls through to dungeon dispatch / boss-nav.
        if (NearbyUnit const* vt = snapshot.victim_info())
            for (uint32_t ie : advice.ignore_entries)
                if (ie == vt->entry)
                {
                    ai.note_engage(snapshot.victim(),
                                   snapshot.published_at_ms(),
                                   /*shield_ms*/ 5u * 60u * 1000u);
                    emit.stop_attack();
                    ai.set_last_rule_fired("combat:drop_ignored_victim");
                    return;
                }
    }

    // ---- PvE-coordinator heal focus ----
    // The designated tank-healer pins its assigned focus (the main tank)
    // when that target is genuinely hurt — otherwise every healer in the
    // group converges on the same lowest-HP member while the tank eats
    // unhealed hits. Placed BELOW survival items / racials / combat
    // positioning so emergencies and fire-avoidance keep priority, and
    // the tick is only consumed on a successful emit (a queued/locked
    // cast falls through to the class APL instead of wasting the tick).
    // is_alive guard, not hp > 0: a released ghost main tank has hp == 1
    // and would otherwise soak front-loaded heals all corpse run.
    // Classic 1.60 healers skip this pin: their APL heal picker
    // (Combat/Apl/ApClassicHeal.h, the owner's HealScore) already weighs the
    // heal_focus (+20) against emergencies elsewhere and downranks the cast,
    // and the retail ClassOocHeal ids are not the vanilla heals.
    {
        auto const& heal_po = snapshot.raw().pve_order;
        if (heal_po.active && !heal_po.heal_focus.IsEmpty() &&
            !Classic::IsSingleSpecClass(snapshot.cls()) &&
            snapshot.is_alive() && !snapshot.is_casting() &&
            heal_po.heal_focus != snapshot.raw().guid &&
            group.exists())
        {
            const uint32 hspell = ClassOocHeal(snapshot.cls(), snapshot.spec());
            if (hspell != 0 && snapshot.knows_spell(hspell) &&
                snapshot.is_ready(hspell))
            {
                if (auto const* mems = group.members())
                    for (auto const& m : *mems)
                    {
                        if (m.guid != heal_po.heal_focus) continue;
                        if (!m.online || !m.is_alive ||
                            m.map_id != snapshot.map_id())
                            break;
                        if (m.hp_pct() >= 70) break;   // APL triage covers
                        float hx, hy, hz;
                        snapshot.position(hx, hy, hz);
                        const float dxh = m.x - hx, dyh = m.y - hy;
                        if (dxh * dxh + dyh * dyh < 40.0f * 40.0f &&
                            emit.cast(hspell, m.guid))
                        {
                            ai.set_last_rule_fired("combat:pve_heal_focus");
                            return;
                        }
                        break;
                    }
            }
        }
    }

    // [BUG BG-P0a] Flag/orb-carrier homeward. A CTF/orb carrier must keep
    // running toward its capture point even while attacked (real players don't
    // stop to trade blows with the chaser). Resolve the carrier signal /
    // destination via the shared helper, reusing the per-bot BG advice cache
    // (refreshed on idle ticks). When carrying, emit a run move_to toward the
    // cap point; we deliberately do NOT return here so the APL still fires the
    // bot's instant-cast damage while it runs — but the homeward move and the
    // `!i_am_carrier` guard below together suppress the melee gap-close so the
    // carrier never peels off to chase its attacker.
    // (i_am_carrier hoisted above the BG tactical layer.)
    if (snapshot.in_battleground() && i_am_carrier)
        BgCarrierHomeward(snapshot, ai, emit, ai.bg_advice_cache().cached);

    // Gap-close: if we're a melee spec and our victim is out of melee range,
    // emit a MoveTo to the victim's position. The rotation can still fire
    // its instant-cast rules during the close — start_attack already runs
    // auto-attack once we arrive. We use the snapshot's nearby_enemies /
    // attackers for victim position rather than chasing a moved target.
    // A raid healer never walks into melee: its job is range, whatever its
    // spec table says (a resto druid or holy paladin flagged melee by spec).
    // Playerbot.Behavior.DungeonFlow: a 5-man healer keeps range and waits for the next heal too ([dungeon_flow]).
    const bool raid_healer = (snapshot.is_in_raid() ||
                              (snapshot.is_in_dungeon() && Services::Config().behavior_dungeon_flow() &&
                               group.exists() && group.tank() != nullptr)) &&
                             !snapshot.in_battleground() &&
                             ai.effective_role(snapshot) == Role::Healer;
    if (!snapshot.victim().IsEmpty() && !i_am_carrier &&  // BG-P0a: carrier runs home, never chases
        !raid_healer && IsMeleeSpec(snapshot.cls(), snapshot.spec()))
    {
        constexpr float kMeleeRange = 5.0f;
        constexpr float kFleeLeashYd = 8.0f;   // a fleeing mob this far from the bot is let go
        constexpr uint16 kStuckChaseTicksMax = 50;   // ~5s @ 10Hz combat tier
        NearbyUnit const* vu = snapshot.victim_info();
        if (vu)
        {
            float bx, by, bz;
            snapshot.position(bx, by, bz);
            const float dx = vu->x - bx;
            const float dy = vu->y - by;
            const float dz = vu->z - bz;
            const float d2 = dx*dx + dy*dy + dz*dz;
            // Instance: a victim standing in a ground hazard / raid avoid
            // circle is not chased into it - the hazard step-out would walk
            // the bot straight back out (an endless in/out loop). The bot
            // waits at the edge, rotation running, until the tank's hazard
            // step-out (or the raid layer) moves the mob; the wait is not a
            // stuck chase.
            // An owner Stay / Hold keeps the bot at its spot too: a mob out of reach is not chased (a mob on the bot lifts the hold).
            const bool chase_blocked = d2 > kMeleeRange * kMeleeRange &&
                ((snapshot.is_in_instance() && !snapshot.in_battleground() &&
                  RaidStandPointBlocked(snapshot, vu->x, vu->y, vu->z)) ||
                 HeldByOwner(ai, snapshot));
            // Stuck-chase tracking: count ticks where we're out of melee
            // range with the same victim. Reset on different victim or once
            // we close distance. Past the cap → drop the chase to free the
            // bot from terrain-induced infinite gap-close loops.
            if (chase_blocked)
                ai.set_last_rule_fired("combat:melee_hold_hazard");
            else if (d2 > kMeleeRange * kMeleeRange && vu->is_fleeing && snapshot.is_in_dungeon() && d2 > kFleeLeashYd * kFleeLeashYd)
            {
                // A mob running for help is not chased (Deadmines 2026-10-13: the tank followed a fleeing Miner into the next
                // packs). Melee stop and leave it to the ranged bots or its return; the engage shield keeps the target pick
                // from taking it straight back.
                if (RaidLogDue(RaidLogKey(snapshot, RaidLogKind::ThreatHold) ^ 0x666c6565u, snapshot.published_at_ms(), 5000u))
                    TC_LOG_INFO("playerbot.v2", "[flee_chase] {} does not chase fleeing entry {} ({:.0f} y away)", snapshot.name(), vu->entry, std::sqrt(d2));
                emit.stop_attack();
                ai.note_engage(snapshot.victim(), snapshot.published_at_ms());
                ai.set_stuck_chase(ObjectGuid::Empty, 0);
                ai.set_last_rule_fired("combat:flee_no_chase");
                return;
            }
            else if (d2 > kMeleeRange * kMeleeRange)
            {
                if (ai.stuck_chase_victim() == snapshot.victim())
                    ai.set_stuck_chase(snapshot.victim(), ai.stuck_chase_ticks() + 1);
                else
                    ai.set_stuck_chase(snapshot.victim(), 1);
                // A tank never gives up a mob that is on a group member: dropping it leaves the mob killing the healer
                // (deadmines_full run 8: Rhahk'Zor on the priest for 57 s, the warrior silent after its 5 s chase cap). Its
                // chase ends when the mob is in reach (Taunt / melee) or dies; a victim it truly cannot path to is handled
                // above (unfightable skip) and by the mob's own leash.
                bool tank_keeps_chase = ai.effective_role(snapshot) == Role::Tank && vu->victim != snapshot.guid() &&
                                        ThreatVictim(vu->victim_kind) == ThreatVictim::Member;
                // ...but not far from the player it follows: in a dungeon a human leads, a mob that has run 30 y or more from him
                // is not walked after (Deadmines 2026-10-13: the tank ended 44 y from the group chasing a Miner). The tank stops
                // here, the group's own fight (and the mob's return) goes on.
                // A fleeing mob (a low-health one running for help) is never walked after by a dungeon tank: it brings the next
                // pack back with it, and the group's ranged finish it or it returns (playtest, Deadmines 2026-10-13).
                if (tank_keeps_chase && vu->is_fleeing && snapshot.is_in_dungeon())
                    tank_keeps_chase = false;
                if (tank_keeps_chase && snapshot.raw().owner_in_group && snapshot.is_in_instance() && !snapshot.raw().owner_guid.IsEmpty())
                    for (NearbyUnit const& f : snapshot.raw().combat.nearby_friends)
                        if (f.guid == snapshot.raw().owner_guid)
                        {
                            const float ox = vu->x - f.x, oy = vu->y - f.y;
                            constexpr float kTankChaseLeashYd = 30.0f;
                            if (ox * ox + oy * oy > kTankChaseLeashYd * kTankChaseLeashYd)
                            {
                                tank_keeps_chase = false;
                                if (RaidLogDue(RaidLogKey(snapshot, RaidLogKind::ThreatHold) ^ 0x6c656173u, snapshot.published_at_ms(), 5000u))
                                    TC_LOG_INFO("playerbot.v2", "[tank_chase] {} stops chasing entry {}: {:.0f} y from its owner (leash {:.0f} y)",
                                                snapshot.name(), vu->entry, std::sqrt(ox * ox + oy * oy), kTankChaseLeashYd);
                            }
                            break;
                        }
                // ...nor out of its healers' reach: in a 5-man the tank stays where the healer sees it and lets the mob come (the DPS
                // finish it). Ends the chase at once, not after the stuck-chase window.
                if (ai.effective_role(snapshot) == Role::Tank && vu->victim != snapshot.guid() && snapshot.beyond_healer_leash(vu->x, vu->y))
                {
                    if (RaidLogDue(RaidLogKey(snapshot, RaidLogKind::ThreatHold) ^ 0x68656c70u, snapshot.published_at_ms(), 5000u))
                        TC_LOG_INFO("playerbot.v2", "[tank_chase] {} stops chasing entry {} ({:.0f}y away): further than the healer's reach",
                                    snapshot.name(), vu->entry, std::sqrt(d2));
                    emit.stop_attack();
                    ai.note_engage(snapshot.victim(), snapshot.published_at_ms());
                    ai.set_stuck_chase(ObjectGuid::Empty, 0);
                    ai.set_last_rule_fired("combat:tank_healer_leash");
                    return;
                }
                if (tank_keeps_chase && ai.stuck_chase_ticks() == kStuckChaseTicksMax &&
                    RaidLogDue(RaidLogKey(snapshot, RaidLogKind::ThreatHold) ^ 0x63687365u, snapshot.published_at_ms(), 5000u))
                    TC_LOG_INFO("playerbot.v2", "[tank_chase] {} keeps chasing entry {} ({:.0f}y away) held by a group member",
                                snapshot.name(), vu->entry, std::sqrt(d2));
                if (ai.stuck_chase_ticks() >= kStuckChaseTicksMax && !tank_keeps_chase)
                {
                    emit.stop_attack();
                    // Re-engage shield (BG audit N44): without it the
                    // fallback target pick re-acquired the SAME kiting
                    // player next tick — a visible give-up/re-engage flap
                    // that never lands a hit. The shield makes the bot
                    // pick a different target (or objective) instead.
                    ai.note_engage(snapshot.victim(), snapshot.published_at_ms());
                    ai.set_stuck_chase(ObjectGuid::Empty, 0);
                    return;
                }
            }
            else
            {
                // Within melee — clear the counter so future losses of range
                // start a fresh window.
                if (ai.stuck_chase_ticks() > 0)
                    ai.set_stuck_chase(ObjectGuid::Empty, 0);
                // SpreadOut settle: in reach but piled on the victim - a raid / elite fight with the mob
                // busy on someone else - slide to this bot's own spot, only when > 2 y from it, at most
                // every 3 s, never while casting, rooted or already walking.
                if (Services::Config().behavior_spread_out() &&
                    snapshot.is_in_raid() &&
                    vu->victim != snapshot.guid() && ai.effective_role(snapshot) != Role::Tank &&
                    !snapshot.is_rooted() && !snapshot.is_casting() && !snapshot.has_path_destination() &&
                    !ai.action_recently_tried(BotAI::ActionKind::RaidSpacing, 0, snapshot.published_at_ms()))
                {
                    float sx, sy;
                    if (MeleeSpreadSpot(snapshot, group, *vu, sx, sy))
                    {
                        const float ex = sx - bx, ey = sy - by;
                        if (ex * ex + ey * ey > 2.0f * 2.0f)
                        {
                            ai.note_action_retry(BotAI::ActionKind::RaidSpacing, 0, snapshot.published_at_ms());
                            emit.move_to(sx, sy, vu->z, /*run*/ false);
                        }
                    }
                }
            }
            if (d2 > kMeleeRange * kMeleeRange && !snapshot.is_rooted() && !chase_blocked)
            {
                // [increment 1e] WC corridor freeze (2026-07-03): the victim
                // may be unfightable — untargetable / pacified / dead flags,
                // or (the live WC case) an ordinary targetable creature whose
                // OWN chase generator reports it cannot path to us
                // (Creature::CanNotReachTarget → snapshot cannot_reach; the
                // ledge pack is z-disconnected from the corridor). This
                // gap-close fired every InCombat frame (already_pathing never
                // held since the live spline dest was the route step, not the
                // victim) and kept yanking the tank off its route spline
                // toward a chase that could never close. Skip the emit — fall
                // through without stop_attack / claiming the tick — so
                // DungeonCombatPositioning (route-aware) keeps movement
                // ownership and the mob is left to leash.
                if (Services::Config().combat_skip_unfightable() &&
                    !IsAttackerFightable(*vu))
                {
                    DiagSkipUnfightable(snapshot, snapshot.victim(),
                                        UnfightableReason(*vu));
                }
                else
                {
                // Default: gap-close to the victim.
                float cx = vu->x, cy = vu->y, cz = vu->z;
                // Travel-through-combat (2026-06-15): when the bot has a FAR
                // same-map travel objective it is IN TRANSIT — incidental aggro
                // must not pull it off-course chasing mobs. Live: Somi drifted
                // EAST after Durotar mobs while his quest goal was ~1200y WEST,
                // "gaining XP but never arriving". Steer the gap-close toward the
                // GOAL instead of the victim: mobs in melee are still auto-attacked
                // as the bot passes, ones it outruns leash, and it makes real
                // travel progress (the stuck-chase cap above then drops the outrun
                // victim). Only when the goal is FAR (>150y — bot not yet at the
                // quest area) AND HP is healthy (low-HP defensives/flee fired
                // earlier own that case); a NEAR goal chases normally so the bot
                // can actually kill its quest mobs.
                if (snapshot.hp_pct() > 50 &&
                    snapshot.has_current_objective() &&
                    snapshot.current_objective_poi().valid &&
                    snapshot.current_objective_poi().map_id == snapshot.map_id())
                {
                    const auto& poi = snapshot.current_objective_poi();
                    const float gdx = poi.x - bx, gdy = poi.y - by;
                    if (gdx * gdx + gdy * gdy > 150.0f * 150.0f)
                    { cx = poi.x; cy = poi.y; cz = poi.z; }
                }
                // Skip the MoveTo if we're already pathing to ~the chase target —
                // avoids spamming MoveTo every tick. Replan only if it moved >3yd.
                // Rooted bots can't move, so don't bother emitting either.
                bool already_pathing = false;
                if (snapshot.has_path_destination())
                {
                    float px, py, pz;
                    snapshot.path_destination(px, py, pz);
                    const float pdx = cx - px;
                    const float pdy = cy - py;
                    const float pdz = cz - pz;
                    const float pd2 = pdx*pdx + pdy*pdy + pdz*pdz;
                    constexpr float kReplanRange = 3.0f;
                    if (pd2 < kReplanRange * kReplanRange) already_pathing = true;
                }
                // SpreadOut: walk to this bot's own spot round the victim's back / sides, not its centre.
                if (cx == vu->x && cy == vu->y)
                {
                    float sx, sy;
                    if (MeleeSpreadSpot(snapshot, group, *vu, sx, sy)) { cx = sx; cy = sy; }
                }
                // Skip the MoveTo if we're already pathing to ~the chase target —
                // avoids spamming MoveTo every tick. Replan only if it moved >3yd.
                // Rooted bots can't move, so don't bother emitting either.
                already_pathing = false;
                if (snapshot.has_path_destination())
                {
                    float px, py, pz;
                    snapshot.path_destination(px, py, pz);
                    const float pdx = cx - px;
                    const float pdy = cy - py;
                    const float pdz = cz - pz;
                    if (pdx*pdx + pdy*pdy + pdz*pdz < 3.0f * 3.0f) already_pathing = true;
                }
                if (!already_pathing)
                    emit.move_to(cx, cy, cz, /*run*/ true);
                }
            }
        }
    }

    // Pet command: keep the active pet attacking the bot's current victim.
    // pet_attack is idempotent server-side. has_pet() short-circuits when the
    // pet is dead (pet_alive=false) so we don't fire failing commands at a
    // corpse — Hunter rotation handles Revive Pet OOC via State_Idle.
    // Only emit while the pet is on the WRONG target: an unconditional emit
    // here re-issued PetAttack every combat tick (~7 intents/sec/bot observed
    // live on Uraimus 2026-06-10), drowning the intent history and queue for
    // zero behavioral gain. pet_victim() lags by one snapshot (~250ms) after
    // the pet switches, so at most 1-2 duplicate emits slip through per
    // target change.
    if (snapshot.has_pet() && !snapshot.victim().IsEmpty() &&
        snapshot.pet_victim() != snapshot.victim() &&
        !ThreatHoldActive(ai, snapshot, snapshot.victim(), &group))   // threat discipline: no pet on a held mob
        emit.pet_attack(snapshot.victim());

    // Battle-rez: cast a class-appropriate combat-castable rez on the first
    // dead group member. Spells:
    //   Druid Rebirth                    20484  (instant rez, brez budget)
    //   Death Knight Raise Ally          61999  (8s cast, brez budget, requires
    //                                            Corpse Dust unless talented)
    // Warlock's Soulstone Resurrection (20707) is intentionally NOT here:
    // 20707 is a *living-target* pre-buff that grants the auto-rez aura
    // (spell 3026 fires on next death). It belongs in the State_Idle buff-
    // maintenance layer, not the in-combat brez branch.
    // Only fires when not mid-cast (don't interrupt our own DPS cast for the
    // brez) and only when the dead target is on the same map. Server tracks
    // the per-encounter battle-rez budget; out-of-charges surfaces as
    // ServerRefused and the bot logic moves on. brez_acked dedups against re-emits
    // until the target rezzes (drops from dead_member) or leaves the group.
    if (group.exists() && !snapshot.is_casting())
    {
        uint32 brez_spell = 0;
        switch (snapshot.cls())
        {
            case CLASS_DRUID:        brez_spell = snapshot.spec() == 105 ? 0 : 20484; break;   // Restoration: Rebirth sits in its APL after the heals (classic_resto:rebirth), one decider
            case CLASS_DEATH_KNIGHT: brez_spell = 61999; break;
            default: break;
        }
        // A tank holds the boss and never rezzes in the fight (MC Garr 2026-10-10: the bear tank cast Rebirth mid-fight, twice refused).
        if (brez_spell != 0 && ai.effective_role(snapshot) != Role::Tank && snapshot.knows_spell(brez_spell) && snapshot.is_ready(brez_spell)
            && (snapshot.cls() != CLASS_DRUID || snapshot.power(0) >= snapshot.mana_cost(snapshot.resolve_rank(brez_spell) ? snapshot.resolve_rank(brez_spell) : brez_spell)))   // mana: the core refuses an unaffordable Rebirth NO_POWER
        {
            // Target: a corpse in range and line of sight, healers then tanks
            // first. A corpse another member is already battle-rezzing (any
            // rank of Rebirth / Raise Ally) is skipped and several casters of
            // this class spread over the corpses by guid order
            // (ClassicHeal::RezTarget) - with multi-druid raids all of them
            // used to race to the first corpse and burn their cast / cooldown.
            const uint8 brez_cls = snapshot.cls();
            const ObjectGuid dead = Combat::ClassicHeal::RezTarget(snapshot, group, 28.0f,
                [brez_cls](GroupMemberSummary const& m) { return m.cls == brez_cls && m.role != Role::Tank; }, /*in_combat_ok*/ true);   // a combat rez: the rezzers are in the fight
            if (!dead.IsEmpty())
            {
                // A druid's Rebirth is retried each tick it is not casting (an interrupted or out-of-sight cast must not latch); RezTarget skips a corpse another druid is casting on.
                if (snapshot.cls() == CLASS_DRUID || !ai.brez_acked() || ai.brez_target() != dead)
                {
                    emit.cast(brez_spell, dead);
                    ai.set_brez(dead, true);
                    return;
                }
            }
            else if (ai.brez_acked() && !group.dead_member(snapshot.map_id()))
            {
                // Target rezzed (or dropped from group): clear so the next
                // dead member kicks off a fresh attempt.
                ai.set_brez(ObjectGuid::Empty, false);
            }
        }
    }

    // While a hard cast is in flight on the bot, every additional cast intent
    // we emit gets rejected with SPELL_FAILED_SPELL_IN_PROGRESS by the engine
    // (Spell.cpp:3461). Skip APL evaluation entirely until the cast resolves.
    // We accept the small efficiency loss during the LAUNCHED window (where
    // new casts would actually be valid) in exchange for not flooding the
    // intent queue with rejected casts on every 250ms tick.
    if (CancelOverheal(ai, snapshot, group, emit)) return;
    if (snapshot.is_casting()) return;

    // ---- Group threat discipline: DPS / healer restraint ----
    // the owner's MCRaidHoldDamage: a non-tank bot must not out-threat the tank.
    // While held on its victim it stops swinging / shooting / wanding
    // (cancel_cast here only ends the auto-repeat slot - a real cast already
    // returned above), calls its pet off that mob, and over the ratio sheds
    // threat with Fade / Feign Death / Feint / Cower. stop_attack clears the
    // victim, so from the next tick the rotation runs target-less: heals,
    // buffs and defensives continue, every victim-gated damage rule is
    // silent, and the re-acquire paths below honour the same hold.
    {
        const ObjectGuid hv = snapshot.victim();
        const ThreatHold hold = EvaluateThreatHold(ai, snapshot, hv, &group);
        if (hold != ThreatHold::None)
        {
            NearbyUnit const* hu = snapshot.hostile_unit(hv);
            if (hold == ThreatHold::OverThreat &&
                !(hu && LooseRaidEliteOnMember(snapshot, *hu, &group)) &&   // not our threat to shed
                TryThreatDrop(snapshot, hv, emit))
            {
                ai.set_last_rule_fired("combat:threat_drop");
                return;
            }
            emit.cancel_cast();
            emit.stop_attack();
            if (snapshot.has_pet() && snapshot.pet_victim() == hv)
                emit.pet_set_command_state(1 /*COMMAND_FOLLOW*/);
            ai.set_last_rule_fired(hold == ThreatHold::OverThreat ? "combat:threat_hold"
                                   : hold == ThreatHold::ShieldUp ? "combat:shield_hold"
                                                                  : "combat:threat_wait_tank");
            return;
        }
    }

    // Raid tank healer on duty: pre-heal its tank ahead of the boss's swings
    // (the rotation's urgent heals and dispels keep priority, see the gates).
    if (PreemptiveTankHeal(ai, snapshot, group, emit)) return;

    // Why a dungeon tank has not taken a mob that is on a group member: one throttled line with everything the pick-up and
    // taunt rules read (deadmines_full run 10: a Defias Miner killed the priest in the 8 s the warrior spent on Sunder / Revenge).
    if (ai.effective_role(snapshot) == Role::Tank && snapshot.is_in_dungeon() && snapshot.in_combat())
        if (NearbyUnit const* lo = snapshot.loose_attacker(30.0f))
            if (RaidLogDue(RaidLogKey(snapshot, RaidLogKind::ThreatHold) ^ 0x6c6f6f73u, snapshot.published_at_ms(), 3000u))
            {
                float tx, ty, tz;
                snapshot.position(tx, ty, tz);
                NearbyUnit const* vi = snapshot.victim_info();
                // The class's own taunt (the line read the warrior's Taunt for every tank: a druid showed 'taunt known 0'; a paladin has none, so it shows the warrior one as unknown).
                uint32 const taunt_spell = snapshot.cls() == CLASS_DRUID ? 6795u : 355u;   // vanilla paladins have no taunt
                TC_LOG_INFO("playerbot.v2",
                    "[tank_loose] {} loose entry {} ({}) on {} {:.0f}y away; my victim entry {} kind {}; taunt known {} ready {}; stance {}; rage {}; pickup {}; "
                    "taunt reach {:.1f}y (spell data), candidate {} (10y) / {} (30y), claimed by another tank {}, assigned mob {}, casting {}; "
                    "last rule {}, stunned {} rooted {} moving {} hp {}%",
                    snapshot.name(), lo->entry, lo->guid.GetCounter(), lo->victim.GetCounter(),
                    std::sqrt((lo->x - tx) * (lo->x - tx) + (lo->y - ty) * (lo->y - ty) + (lo->z - tz) * (lo->z - tz)),
                    vi ? vi->entry : 0u, vi ? int(vi->victim_kind) : -1,
                    snapshot.knows_spell(taunt_spell) ? 1 : 0, snapshot.is_ready(taunt_spell) ? 1 : 0,
                    snapshot.has_aura(71) ? "defensive" : "other", snapshot.power(POWER_RAGE),
                    snapshot.pickup_candidate() ? 1 : 0,
                    Combat::TankThreat::TauntReach(snapshot, taunt_spell, 10.0f),
                    snapshot.taunt_candidate(10.f) ? snapshot.taunt_candidate(10.f)->entry : 0u,
                    snapshot.taunt_candidate(30.f) ? snapshot.taunt_candidate(30.f)->entry : 0u,
                    snapshot.raw().pve_order.active && snapshot.raw().pve_order.claimed_by_other_tank(lo->guid) ? 1 : 0,
                    snapshot.assigned_tank_mob() ? snapshot.assigned_tank_mob()->entry : 0u, snapshot.is_casting() ? 1 : 0,
                    ai.last_rule_fired() ? ai.last_rule_fired() : "-", snapshot.is_stunned() ? 1 : 0, snapshot.is_rooted() ? 1 : 0,
                    snapshot.is_moving() ? 1 : 0, snapshot.hp_pct());
            }

    // Sub-L10 bots are still in the "pre-spec" era: even though Midnight
    // auto-assigns a primary spec at character creation (so spec() is non-
    // zero from L1), the spec-locked rotations were authored against the
    // L10+ ability set. Routing a L7 BM hunter to the BM rotation skips
    // Arcane Shot and Hunter's Mark — abilities the bot DOES have via base
    // SkillLineAbility but that the BM rotation doesn't list. The baseline
    // (spec=0) rotation includes the full starter kit for each class, so
    // force it for pre-L10. At L10 specs unlock their full talent rows;
    // we flip back to the spec rotation cleanly on ding.
    uint32 rotation_spec = snapshot.level() < 10 ? 0u : snapshot.spec();
    // A tank-spec bot the raid's tank roster does not need plays its class's
    // melee DPS rotation (same gear, no tank threat): Arms, Retribution, cat.
    if (snapshot.raw().pve_order.active && snapshot.raw().pve_order.raid_dps_tank &&
        IsTankSpec(snapshot.cls(), uint16(rotation_spec)))
    {
        switch (snapshot.cls())
        {
            case CLASS_WARRIOR: rotation_spec = 71;  break;
            case CLASS_PALADIN:
                rotation_spec = 70;
                // Righteous Fury (25780) from its tanking days would keep
                // pulling threat off the tanks. Cancelling is instant.
                if (snapshot.has_aura(25780))
                    emit.cancel_aura(25780);
                break;
            case CLASS_DRUID:   rotation_spec = 103; break;
            default: break;
        }
    }
    if (ApRotation const* rot = Combat::GetRotation(snapshot.cls(), rotation_spec))
    {
        char const* rule_name = nullptr;
        if (rot->tick(ctx, emit, &rule_name))
        {
            ai.set_last_rule_fired(rule_name);
            return;
        }
    }

    // Fallback: keep the bot from looking inert. Try victim, then the
    // selected target, then the highest-threat attacker. start_attack is
    // idempotent server-side.
    // A raid healer whose rotation found nothing to do waits for the next
    // heal: opening on a mob here only drags it into melee range / LoS of
    // the fight and adds threat it cannot shed.
    if (raid_healer)
    {
        // A healer with nothing to cast while mobs hit it: say why (mana, who holds the mob), so a death
        // here is attributable (deadmines_full run 5: priest killed by Rhahk'Zor at 0 mana).
        if (snapshot.hp_pct() <= 60 && snapshot.attackers_count() >= 1 &&
            RaidLogDue(RaidLogKey(snapshot, RaidLogKind::ThreatHold) ^ 0x68656c64u, snapshot.published_at_ms(), 3000u))
        {
            NearbyUnit const* a = snapshot.highest_threat_attacker();
            TC_LOG_INFO("playerbot.v2", "[healer_hold] {} hp {}% mana {}% attackers {} top attacker {} victim_kind {} - rotation has no cast",
                        snapshot.name(), snapshot.hp_pct(), Combat::ClassicHeal::ManaPct(snapshot), snapshot.attackers_count(),
                        a ? a->guid.ToString() : std::string("none"), a ? int(a->victim_kind) : -1);
        }
        // Mobs on a healer that has nothing to cast: it walks to its tank (the mobs follow, and the tank's pick-up / Taunt
        // and AoE reach them) instead of standing and being killed.
        if (snapshot.hp_pct() <= 60 && snapshot.attackers_count() >= 1 && !snapshot.is_rooted() && group.exists() &&
            !HeldByOwner(ai, snapshot))   // a healer told to stay / hold keeps its spot unless a mob is on it (then it walks)
            if (GroupMemberSummary const* tk = group.tank(); tk && tk->online && tk->is_alive && tk->map_id == snapshot.map_id())
            {
                float hx, hy, hz;
                snapshot.position(hx, hy, hz);
                const float dx = tk->x - hx, dy = tk->y - hy;
                // Not across enemies that are not on the tank (Garr's Firesworn): the walk ends in their melee.
                bool path_clear = true;
                for (NearbyUnit const& e_u : snapshot.raw().combat.nearby_enemies)
                    if (e_u.hp > 0 && EnemyBlocksRunToTank(e_u.victim == tk->guid, hx, hy, tk->x, tk->y, e_u.x, e_u.y))
                    {
                        path_clear = false;
                        break;
                    }
                if (path_clear && dx * dx + dy * dy > 6.0f * 6.0f && emit.move_to(tk->x, tk->y, tk->z, /*run*/ true))
                {
                    ai.set_last_rule_fired("combat:healer_run_to_tank");
                    return;
                }
            }
        ai.set_last_rule_fired("combat:raid_healer_hold");
        return;
    }
    ObjectGuid t = snapshot.victim();
    if (t.IsEmpty()) t = snapshot.current_target();
    if (t.IsEmpty())
    {
        if (NearbyUnit const* a = snapshot.highest_threat_attacker())
            t = a->guid;
    }
    // Skull-marked target wins for everyone in combat too (mid-fight focus
    // change). Only honor it if it's actually nearby — out-of-vision skulls
    // are stale.
    if (t.IsEmpty() && group.exists())
    {
        if (ObjectGuid skull = group.skull_target(); !skull.IsEmpty())
        {
            // Skip a dead skull — corpses linger in nearby_enemies for a few
            // seconds and we'd just re-emit start_attack on a corpse every
            // tick until raid leader re-marks.
            // MarksNeedOrder: an idle skull is not a target to pivot onto (the owner's order starts that pull).
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
                if (u.guid == skull && u.hp > 0)
                {
                    if (MarkGate::MayEngage(snapshot, ai, &group, skull, &u)) t = skull;
                    break;
                }
            if (t.IsEmpty())
                for (auto const& u : snapshot.raw().combat.attackers)
                    if (u.guid == skull && u.hp > 0) { t = skull; break; }
        }
    }
    // Group-assist: when nothing else is engaged but we're in combat, pick up
    // whatever the leader (or any in-combat member) is hitting. Skip if we're
    // tanking — tanks pull their own targets.
    if (t.IsEmpty() && ai.effective_role(snapshot) != Role::Tank && group.exists())
    {
        if (auto const* members = group.members())
        {
            const ObjectGuid leader = group.leader();
            ObjectGuid leader_victim;
            ObjectGuid any_combat_victim;
            // Single-pass scan: capture leader's victim and the first
            // online-in-combat member's victim in the same walk. Pre-fix
            // this was two separate passes over `members`. Tiny win in
            // absolute ms (25-member raid × 2 passes), but the pattern
            // matters — group iteration runs every combat tick.
            // Copied member victims must resolve against our own hostile
            // lists (IsValidAttackTarget-gated at snapshot build). A member
            // wedged on an invalid victim (friendly pet, groupmate) must
            // not propagate it — see ingroup:assist for the 2026-06-11
            // Deadmines bots-fighting-each-other cascade this caused.
            auto resolves_hostile = [&](ObjectGuid v) -> bool
            {
                if (v.IsEmpty()) return false;
                for (auto const& u : snapshot.raw().combat.nearby_enemies)
                    if (u.guid == v && u.hp > 0) return true;
                for (auto const& u : snapshot.raw().combat.attackers)
                    if (u.guid == v && u.hp > 0) return true;
                // Trust Creature-typed victims beyond our capped scan —
                // see ingroup:assist (the wedge class was Player/Pet guids;
                // start_attack validates at engage time regardless).
                return v.IsCreature();
            };
            for (auto const& m : *members)
            {
                if (m.guid == leader && resolves_hostile(m.victim))
                {
                    leader_victim = m.victim;
                    if (!any_combat_victim.IsEmpty()) break;
                }
                else if (any_combat_victim.IsEmpty() && m.online && m.in_combat
                         && resolves_hostile(m.victim))
                {
                    any_combat_victim = m.victim;
                    if (!leader_victim.IsEmpty()) break;
                }
            }
            t = !leader_victim.IsEmpty() ? leader_victim : any_combat_victim;
        }
    }
    // Last-ditch engage: still no target after all the fallbacks → pick the
    // closest enemy from nearby_enemies. This catches stealth-pulls where the
    // bot is in combat but neither attackers nor any group member's victim
    // points to a valid target. Without this the bot just stands there
    // looking inert until combat times out.
    if (t.IsEmpty())
    {
        // Never re-acquire a target we just disengaged as unkillable, nor a
        // no-XP / pacified prop. Without this skip the last-ditch fallback
        // immediately re-grabs the nearest enemy the tick after a disengage
        // cleared the victim — re-locking the bot onto the immortal Training
        // Dummy it was just shaken off (the grind/pull pickers honour the engage
        // shield, but this fallback did not, so the bot cycled forever).
        const uint32 now_ms2 = snapshot.published_at_ms();
        const ObjectGuid shielded = ai.last_engage_target();
        const bool shield_on = !shielded.IsEmpty() &&
            (now_ms2 - ai.last_engage_at_ms() < ai.last_engage_shield_ms());
        // Instance group (one pull at a time): the nearest enemy is often the
        // NEXT pack, standing idle 20-40y away. Opening on it from here is a
        // chain pull mid-fight - and for a non-tank the threat hold only
        // delays it by its bounded opening wait. Pulls belong to the main
        // tank's pull rules once the group is ready (idle:tank_pull_next), so
        // this fallback only picks mobs already fighting the group (on the
        // bot, a member or a member's pet).
        const bool group_pull_discipline = snapshot.is_in_instance() &&
            !snapshot.in_battleground() && group.exists();
        auto fights_group = [](NearbyUnit const& u) -> bool
        {
            if (!u.unit_in_combat) return false;
            switch (ThreatVictim(u.victim_kind))
            {
                case ThreatVictim::Me: case ThreatVictim::Tank:
                case ThreatVictim::Member: case ThreatVictim::MemberPet:
                    return true;
                default:
                    return false;
            }
        };
        NearbyUnit const* skipped_idle = nullptr;
        auto skip_target = [&](NearbyUnit const& u) -> bool
        {
            if (u.no_xp_kill || u.is_pacified || (shield_on && u.guid == shielded))
                return true;
            if (group_pull_discipline && !fights_group(u))
            {
                if (!skipped_idle) skipped_idle = &u;
                return true;
            }
            return false;
        };
        // Prefer a target we can SEE — the scan returns mobs through walls,
        // and opening on one drags the party into line-of-sight cast errors
        // (same class as the tank-pull LoS fix). Fall back to any live
        // enemy only when nothing is visible (we ARE in combat with
        // something, e.g. a caster around a pillar).
        for (auto const& u : snapshot.raw().combat.nearby_enemies)
            if (u.hp > 0 && u.in_los && !skip_target(u)) { t = u.guid; break; }
        if (t.IsEmpty())
            for (auto const& u : snapshot.raw().combat.nearby_enemies)
                if (u.hp > 0 && !skip_target(u)) { t = u.guid; break; }
        // Still nothing from the enemy scan — RETALIATE against whatever is
        // actually meleeing us. attackers[] are by definition adjacent and
        // engaging, so they are reliably reachable; the nearby_enemies scan can
        // miss a mob that pulled us from outside scan range or got filtered.
        // Fixes the in-combat-with-attackers-but-victim==0 loop where a bot
        // takes damage forever without fighting back. Strictly additive (only
        // when no target was selected by any prior path). 2026-06-17.
        // (A pacified dummy can't be an attacker, but keep the skip for symmetry
        // so a shielded/no-XP guid is never re-grabbed here either.)
        // (Attackers are fighting us by definition: the group pull filter
        // does not apply to them.)
        if (t.IsEmpty())
            for (auto const& u : snapshot.raw().combat.attackers)
                if (u.hp > 0 && !u.no_xp_kill && !u.is_pacified &&
                    !(shield_on && u.guid == shielded)) { t = u.guid; break; }
        if (t.IsEmpty() && skipped_idle && snapshot.is_in_raid() &&
            RaidLogDue(RaidLogKey(snapshot, RaidLogKind::PullSkip), now_ms2, 10000u))
            TC_LOG_INFO("playerbot.v2",
                "[raid_pull] {} in combat with no target: not opening on idle entry {} ({}) - "
                "pulls are the main tank's",
                snapshot.name(), skipped_idle->entry, skipped_idle->guid.GetCounter());
    }
    // Dedup: if already attacking this exact target, the API's Player::Attack
    // would return false → ServerRefused. Skip the emit to avoid skewing the
    // intents_failed counter every tick of a long fight.
    // Threat discipline: never (re)open on a target this non-tank bot is
    // holding on - wait (melee walks up) until the tank's lead allows it.
    if (!t.IsEmpty() && t != snapshot.victim())
        if (const ThreatHold th = EvaluateThreatHold(ai, snapshot, t, &group); th != ThreatHold::None)
        {
            ThreatHoldWait(ai, snapshot, emit, t, th, IsMeleeSpec(snapshot.cls(), snapshot.spec()));
            return;
        }
    if (!t.IsEmpty() && t != snapshot.victim())
    {
        // Lockout-gate: refused StartAttack (immune / phased / vehicle /
        // already-attacking) shouldn't claim the dispatch slot every InCombat
        // tick — fall through cleanly so the rule doesn't dominate the
        // intents_failed counter on a wedged engage.
        if (!emit.start_attack(t)) return;
        ai.set_last_rule_fired("combat:fallback_attack");
    }
}

} // namespace Playerbot::States
